// Derived-cache freshness (round dv, R2; docs/render/iso.md, derived zoom levels, and
// docs/app/game-shell.md, stale completions): a warm derived chunk must never be rebuilt from a
// stale child, labelled current and kept. Synthetic cases (an empty scene with two derived levels;
// no game files) and one game-file case: the warm chunks carry a sentinel colour, and the refreshed
// pixels are compared with a fresh renderer's output (a CPU reference that never saw the warm
// state).
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <tuple>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace render = anno::render;

namespace {

constexpr std::uint32_t kSentinel = 0x00ff0000;

std::shared_ptr<render::Scene> empty_scene() {
    auto scene = std::make_shared<render::Scene>(
        "derived freshness", std::vector<anno::formats::insel::Island>{},
        anno::assets::island::GroundTextures{}, anno::formats::anlagen::AnlagenFile{},
        render::kHeightLift);
    scene->set_levels(render::ZoomLevels::with_derived(2));
    return scene;
}

/// A warm chunk: the sentinel colour everywhere, as if built before the change.
render::Chunk warm_chunk() {
    render::Chunk chunk;
    chunk.ground.assign(static_cast<std::size_t>(render::kChunk) *
                            static_cast<std::size_t>(render::kChunk),
                        kSentinel);
    return chunk;
}

/// The keys of the warm state: the built child (level 2), its parent (level 3), and the grandparent
/// (level 4), all at chunk (0, 0).
const render::ChunkKey kChild{2, 0, 0};
const render::ChunkKey kParent{3, 0, 0};
const render::ChunkKey kGrand{4, 0, 0};

/// A reference renderer that never saw the warm state: the fresh pixels of the three keys.
struct Reference {
    std::vector<std::uint32_t> child, parent, grand;
};

Reference fresh_reference(const std::shared_ptr<render::Scene>& scene) {
    render::FrameRenderer fresh(1);
    // Asking for the grandparent builds its children and its parent first (derived chunks are
    // reduced from cached children; the planner builds the missing ones before it).
    for (int round = 0; round < 4; ++round) {
        fresh.update_chunks(scene, render::Rect{0, 0, 1, 1}, 2);
        fresh.update_chunks(scene, render::Rect{0, 0, 1, 1}, 3);
        fresh.update_chunks(scene, render::Rect{0, 0, 1, 1}, 4);
    }
    Reference r;
    r.child = fresh.chunk(kChild)->ground;
    r.parent = fresh.chunk(kParent)->ground;
    r.grand = fresh.chunk(kGrand)->ground;
    return r;
}

/// Warms a renderer with sentinel chunks at the three keys of the chain (the child, its parent
/// and the grandparent). The other children of the parent stay unbuilt: they are built fresh, so
/// the reference (which builds every chunk fresh) is the same scene and the same inputs.
void warm(render::FrameRenderer& frames) {
    for (const render::ChunkKey& key : {kChild, kParent, kGrand}) {
        frames.insert_chunk(key, warm_chunk());
    }
}

} // namespace

TEST_CASE("derived freshness: a warm parent is refreshed from its changed child, two levels up "
          "(synchronous)" *
          doctest::test_suite("derived_freshness")) {
    const auto scene = empty_scene();
    const Reference ref = fresh_reference(scene);
    render::FrameRenderer frames(1);
    warm(frames);
    // The child changes (generation 1): its ancestors are marked by the same box.
    frames.invalidate({kChild, kParent, kGrand}, 1);
    // The scene is generation 1 (the derivation below keeps the levels).
    const auto next = scene->with_islands({});
    for (int round = 0; round < 4; ++round) {
        frames.update_chunks(next, render::Rect{0, 0, 1, 1}, 4);
    }
    CHECK(frames.stale_generation(kChild) == 0);
    CHECK(frames.stale_generation(kParent) == 0);
    CHECK(frames.stale_generation(kGrand) == 0);
    REQUIRE(frames.chunk(kChild) != nullptr);
    REQUIRE(frames.chunk(kParent) != nullptr);
    REQUIRE(frames.chunk(kGrand) != nullptr);
    // The pixels are the fresh ones, not the sentinel that the warm parent carried.
    CHECK(frames.chunk(kChild)->ground == ref.child);
    CHECK(frames.chunk(kParent)->ground == ref.parent);
    CHECK(frames.chunk(kGrand)->ground == ref.grand);
    // The reference of an empty scene has no ground: the warm sentinel must be gone everywhere.
    CHECK(std::count(frames.chunk(kParent)->ground.begin(), frames.chunk(kParent)->ground.end(),
                     kSentinel) == 0);
    CHECK(std::count(frames.chunk(kGrand)->ground.begin(), frames.chunk(kGrand)->ground.end(),
                     kSentinel) == 0);
}

TEST_CASE("derived freshness: an older scene never clears a newer mark (synchronous)" *
          doctest::test_suite("derived_freshness")) {
    const auto scene = empty_scene();
    render::FrameRenderer frames(1);
    warm(frames);
    // A change of generation 2 marks the child, but the renderer is handed generation 1 (an older
    // scene): the mark stays, and nothing built from generation 1 replaces the warm chunk.
    frames.invalidate({kChild}, 2);
    frames.update_chunks(scene, render::Rect{0, 0, 1, 1}, 2);
    CHECK(frames.stale_generation(kChild) == 2);
    REQUIRE(frames.chunk(kChild) != nullptr);
    CHECK(frames.chunk(kChild)->ground.front() == kSentinel);
}

TEST_CASE("derived freshness: a warm parent is refreshed through the workers (asynchronous)" *
          doctest::test_suite("derived_freshness")) {
    const auto scene = empty_scene();
    const Reference ref = fresh_reference(scene);
    render::FrameRenderer frames(2);
    warm(frames);
    auto owned = std::make_unique<render::ChunkWorkers>(*scene, 2);
    render::ChunkWorkers* workers = owned.get();
    frames.set_workers(std::move(owned));
    frames.invalidate({kChild, kParent, kGrand}, 1);
    const auto next = scene->with_islands({});
    for (int round = 0; round < 16; ++round) {
        frames.update_chunks(next, render::Rect{0, 0, 1, 1}, 4);
        workers->wait_idle();
    }
    frames.update_chunks(next, render::Rect{0, 0, 1, 1}, 4);
    CHECK(frames.stale_generation(kParent) == 0);
    REQUIRE(frames.chunk(kParent) != nullptr);
    CHECK(frames.chunk(kParent)->ground == ref.parent);
    CHECK(frames.chunk(kGrand)->ground == ref.grand);
}

// The same chain over a real island (game files): the refreshed warm chain must equal a fresh CPU
// scene's pixels, where the island has content. Rivers are off here: a river island's flow
// finishes on its own schedule, which is not this test's subject (river_lifecycle_test covers it).
namespace {

/// The river-free island D_256x256_0045_5B as a shared scene with two derived levels. The scene
/// owns the file bytes its islands and Anlagen view (render::Scene::Backing).
std::shared_ptr<render::Scene> island_scene(const std::filesystem::path& dir) {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(
        anno::test::read_file(dir / "Scenes" / "Islands" / "Dschungel" / "D_256x256_0045_5B.scp"));
    bytes->push_back(anno::test::read_file(dir / "Anlagen.dat"));
    bytes->push_back(anno::test::read_file(dir / "Textures" / "Felder.tex"));
    auto anlagen = anno::formats::anlagen::AnlagenFile::parse((*bytes)[1]).value();
    const auto felder = anno::formats::tex::TexFile::parse((*bytes)[2]).value();
    auto textures = anno::assets::island::GroundTextures::load(felder).value();
    if (const auto c = render::water_colour(anlagen)) {
        textures.set_water(*c);
    }
    auto scene = std::make_shared<render::Scene>(
        "derived island", anno::formats::insel::islands((*bytes)[0]).value(), std::move(textures),
        std::move(anlagen), render::kHeightLift, bytes);
    scene->river_cfg.enabled = false;
    scene->set_levels(render::ZoomLevels::with_derived(2));
    return scene;
}

} // namespace

TEST_CASE("derived freshness: a warm chain over a real island refreshes to the fresh CPU pixels "
          "(game files, synchronous)" *
          doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scene = island_scene(*dir);
    const auto view = scene->bounds(2);
    REQUIRE(view.has_value());

    // The reference: the group-2 chunks of the island built fresh, from a renderer that never saw a
    // warm state. The chain is a built child with content, and its two ancestors (the parent and
    // the grandparent of derived_children: a parent at (g, x, y) reduces the children at 2x + q %
    // 2, 2y + q / 2).
    render::FrameRenderer reference(1);
    for (int round = 0; round < 6; ++round) {
        reference.update_chunks(scene, *view, 2);
        reference.update_chunks(scene, *view, 3);
        reference.update_chunks(scene, *view, 4);
    }
    render::ChunkKey child{2, 0, 0};
    bool found = false;
    for (const render::ChunkKey& key : render::chunk_keys_touching(*scene, *view, 2)) {
        const render::Chunk* c = reference.chunk(key);
        if (c != nullptr && !c->ground.empty()) {
            child = key;
            found = true;
            break;
        }
    }
    REQUIRE(found);
    const int cx = std::get<1>(child), cy = std::get<2>(child);
    const render::ChunkKey parent{3, cx >> 1, cy >> 1};
    const render::ChunkKey grand{4, cx >> 2, cy >> 2};
    REQUIRE(reference.chunk(parent) != nullptr);
    REQUIRE(reference.chunk(grand) != nullptr);
    const auto ref_child = reference.chunk(child)->ground;
    const auto ref_parent = reference.chunk(parent)->ground;
    const auto ref_grand = reference.chunk(grand)->ground;

    // The warm renderer: the chain is cached with sentinel pixels, then the child changes.
    render::FrameRenderer frames(1);
    for (const render::ChunkKey& key : {child, parent, grand}) {
        frames.insert_chunk(key, warm_chunk());
    }
    frames.invalidate({child, parent, grand}, 1);
    const auto next = scene->with_islands({});
    for (int round = 0; round < 6; ++round) {
        frames.update_chunks(next, *view, 2);
        frames.update_chunks(next, *view, 3);
        frames.update_chunks(next, *view, 4);
    }
    REQUIRE(frames.chunk(child) != nullptr);
    REQUIRE(frames.chunk(parent) != nullptr);
    REQUIRE(frames.chunk(grand) != nullptr);
    CHECK(frames.stale_generation(child) == 0);
    CHECK(frames.stale_generation(parent) == 0);
    CHECK(frames.stale_generation(grand) == 0);
    CHECK(frames.chunk(child)->ground == ref_child);
    CHECK(frames.chunk(parent)->ground == ref_parent);
    CHECK(frames.chunk(grand)->ground == ref_grand);
}

// Owner playtest 2026-11-03: the first zoom out to the whole map waited for thousands of chunks and
// showed islands cut off; the overview levels are now built in the background while the view needs
// nothing (FrameRenderer::prewarm_far_levels). Without it nothing asks for them.
TEST_CASE("prewarm: the resident overview levels are built in the background, and only on request "
          "(game files)" *
          doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scene = island_scene(*dir);
    // The island's whole area at each derived level: few chunks, so they are resident.
    std::vector<render::ChunkKey> overview;
    for (std::size_t g = scene->levels().built_count(); g < scene->levels().size(); ++g) {
        const auto b = scene->bounds(g);
        REQUIRE(b.has_value());
        const auto keys = render::chunk_keys_touching(*scene, *b, g);
        REQUIRE(keys.size() <= 64);
        overview.insert(overview.end(), keys.begin(), keys.end());
    }
    REQUIRE(!overview.empty());
    // A view far from the island: nothing visible to build but open sea.
    const render::Rect away{-200000, -200000, 64, 64};
    const auto run = [&](bool prewarm) {
        render::FrameRenderer frames(2);
        auto workers = std::make_unique<render::ChunkWorkers>(4);
        render::ChunkWorkers* w = workers.get();
        frames.set_workers(std::move(workers));
        frames.prewarm_far_levels = prewarm;
        frames.resident_level_chunks = 64; // the island's levels 3 and 4 are 24 and 8 chunks
        for (int round = 0; round < 400; ++round) {
            frames.update_chunks(scene, away, 2);
            w->wait_idle();
        }
        std::size_t built = 0;
        for (const render::ChunkKey& k : overview) {
            built += frames.chunk(k) != nullptr ? 1U : 0U;
        }
        return built;
    };
    CHECK(run(false) < overview.size()); // nothing asked for them
    CHECK(run(true) == overview.size()); // every chunk of every overview level is cached
}

// Owner playtest 2026-11-03, round 5: after the founding the overview needed a moment again. A
// change marks the chunks over its island stale at every level (invalidate); a stale chunk keeps
// drawing until its replacement arrives, and before this nothing rebuilt a resident one until it
// was shown.
TEST_CASE(
    "prewarm: a resident overview chunk marked stale is rebuilt in the background (game files)" *
    doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scene = island_scene(*dir);
    std::vector<render::ChunkKey> overview;
    for (std::size_t g = scene->levels().built_count(); g < scene->levels().size(); ++g) {
        const auto b = scene->bounds(g);
        REQUIRE(b.has_value());
        const auto keys = render::chunk_keys_touching(*scene, *b, g);
        overview.insert(overview.end(), keys.begin(), keys.end());
    }
    REQUIRE(!overview.empty());
    const render::Rect away{-200000, -200000, 64, 64};
    render::FrameRenderer frames(2);
    auto workers = std::make_unique<render::ChunkWorkers>(4);
    render::ChunkWorkers* w = workers.get();
    frames.set_workers(std::move(workers));
    frames.prewarm_far_levels = true;
    frames.resident_level_chunks = 64;
    const auto settle = [&](const std::shared_ptr<const render::Scene>& s) {
        for (int round = 0; round < 400; ++round) {
            frames.update_chunks(s, away, 2);
            w->wait_idle();
        }
    };
    settle(scene);
    for (const render::ChunkKey& k : overview) {
        REQUIRE(frames.chunk(k) != nullptr);
    }
    // A change over the whole island: the next generation, its keys marked with it, as the
    // picture's commit does (SceneLoader: with_islands, then invalidate with its generation).
    const auto next = scene->with_islands({});
    REQUIRE(next->generation() > scene->generation());
    std::size_t builds_before = 0;
    for (const render::ChunkKey& k : overview) {
        builds_before += frames.chunk_builds(k) + frames.chunk_rebuilds(k);
    }
    frames.invalidate(overview, next->generation());
    const auto t0 = std::chrono::steady_clock::now();
    settle(next);
    MESSAGE(
        "background rebuild of "
        << overview.size() << " stale overview chunks: "
        << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()
        << " ms with 4 workers (400 idle rounds included)");
    std::size_t builds_after = 0;
    for (const render::ChunkKey& k : overview) {
        builds_after += frames.chunk_builds(k) + frames.chunk_rebuilds(k);
    }
    CHECK(builds_after - builds_before == overview.size()); // each stale chunk once, no loop
    std::size_t fresh = 0, rebuilt = 0;
    for (const render::ChunkKey& k : overview) {
        const auto chunk = frames.chunk(k);
        fresh += chunk != nullptr && frames.stale_generation(k) == 0 &&
                         chunk->generation == next->generation()
                     ? 1U
                     : 0U;
        rebuilt += frames.chunk_rebuilds(k) > 0 ? 1U : 0U;
    }
    CHECK(fresh == overview.size());
    CHECK(rebuilt == overview.size());
}

// Review 2026-11-03: a prewarm target whose build fails is never retried by the workers; the
// prewarm waited for it forever and built nothing after it. It now gives that chunk up and goes on.
TEST_CASE("prewarm: a resident chunk that cannot be built does not stop the rest (game files)" *
          doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scene = island_scene(*dir);
    std::vector<render::ChunkKey> overview;
    for (std::size_t g = scene->levels().built_count(); g < scene->levels().size(); ++g) {
        const auto b = scene->bounds(g);
        REQUIRE(b.has_value());
        const auto keys = render::chunk_keys_touching(*scene, *b, g);
        overview.insert(overview.end(), keys.begin(), keys.end());
    }
    REQUIRE(overview.size() > 1);
    // The prewarm starts at the farthest level: its first chunk fails.
    const render::ChunkKey broken = overview.back();
    const render::Rect away{-200000, -200000, 64, 64};
    render::FrameRenderer frames(2);
    auto workers = std::make_unique<render::ChunkWorkers>(4);
    render::ChunkWorkers* w = workers.get();
    w->set_test_failure([broken](const render::ChunkKey& k) { return k == broken; });
    frames.set_workers(std::move(workers));
    frames.prewarm_far_levels = true;
    frames.resident_level_chunks = 64;
    for (int round = 0; round < 400; ++round) {
        frames.update_chunks(scene, away, 2);
        w->wait_idle();
    }
    std::size_t built = 0;
    for (const render::ChunkKey& k : overview) {
        built += frames.chunk(k) != nullptr ? 1U : 0U;
    }
    CHECK(frames.chunk(broken) == nullptr);
    CHECK(built == overview.size() - 1);
}
