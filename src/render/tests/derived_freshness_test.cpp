// Derived-cache freshness (round dv, R2; docs/render/iso.md, derived zoom levels, and
// docs/app/game-shell.md, stale completions): a warm derived chunk must never be rebuilt from a
// stale child, labelled current and kept. Synthetic cases (an empty scene with two derived levels;
// no game files) and one game-file case: the warm chunks carry a sentinel colour, and the refreshed
// pixels are compared with a fresh renderer's output (a CPU reference that never saw the warm
// state).
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
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

#include "corpus_scene.hpp"

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
    // The scene has no islands: every key lies outside it; the chain is built as for one inside.
    fresh.build_outside_chunks = true;
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
    frames.build_outside_chunks = true; // the derivation chain of a scene without islands
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
    frames.build_outside_chunks = true; // the derivation chain of a scene without islands
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

// Review 2026-11-03 (P2): the prewarm gave its target up whenever the last submit left nothing
// queued or running. A job that finishes between update_chunks' take_done and its submit is skipped
// by that submit (done, not taken yet), so the target's last missing child arrived one frame after
// the target was given up, and the chunk was never prewarmed again. The gate holds that child's
// build; the before-submit seam lets it finish inside the gap. The case runs on a synthetic island
// (CI) and on a real one (game files).
namespace {

void prewarm_gap(const std::shared_ptr<render::Scene>& scene) {
    const std::size_t first_derived = scene->levels().built_count();
    std::vector<render::ChunkKey> overview;
    for (std::size_t g = first_derived; g < scene->levels().size(); ++g) {
        const auto b = scene->bounds(g);
        REQUIRE(b.has_value());
        const auto keys = render::chunk_keys_touching(*scene, *b, g);
        overview.insert(overview.end(), keys.begin(), keys.end());
    }
    // The target: an overview chunk of the first derived level; the child: one of its children
    // with land (a real build).
    const auto built_bounds = scene->bounds(first_derived - 1);
    REQUIRE(built_bounds.has_value());
    std::optional<render::ChunkKey> target, child;
    for (const render::ChunkKey& k : overview) {
        if (std::get<0>(k) != first_derived) {
            continue;
        }
        for (const render::ChunkKey& kid : render::derived_children(k, scene->levels())) {
            if (render::chunk_rect(std::get<1>(kid), std::get<2>(kid)).intersect(*built_bounds)) {
                target = k;
                child = kid;
                break;
            }
        }
        if (target) {
            break;
        }
    }
    REQUIRE(target.has_value());
    const render::Rect away{-200000, -200000, 64, 64};
    // The gate's state outlives the workers (declared before the renderer that owns them), and the
    // guard opens the gate whatever ends the test (destroyed before the renderer), so a failing
    // check never leaves a worker waiting on it.
    std::mutex mu;
    std::condition_variable cv;
    bool held = false, open = false;
    render::FrameRenderer frames(2);
    struct OpenGate {
        std::mutex& mu;
        std::condition_variable& cv;
        bool& open;
        ~OpenGate() {
            {
                const std::lock_guard lock(mu);
                open = true;
            }
            cv.notify_all();
        }
    } const guard{mu, cv, open};
    auto workers = std::make_unique<render::ChunkWorkers>(4);
    render::ChunkWorkers* w = workers.get();
    frames.set_workers(std::move(workers));
    frames.prewarm_far_levels = true;
    frames.resident_level_chunks = 64;
    frames.spare_chunks = 100000; // the children stay cached: the rebuild needs only `child`
    const auto settle = [&](const std::shared_ptr<const render::Scene>& s) {
        for (int round = 0; round < 400; ++round) {
            frames.update_chunks(s, away, 2);
            w->wait_idle();
        }
    };
    settle(scene);
    REQUIRE(frames.chunk(*target) != nullptr);
    REQUIRE(frames.chunk(*child) != nullptr);
    const std::size_t rebuilds_before = frames.chunk_rebuilds(*target);
    // The child's rebuild for the next generation waits in the gate until it is opened.
    const auto next = scene->with_islands({});
    w->set_test_gate([&](const render::ChunkKey& k, std::uint64_t generation) {
        if (k != *child || generation != next->generation()) {
            return;
        }
        std::unique_lock lock(mu);
        held = true;
        cv.notify_all();
        cv.wait(lock, [&] { return open; });
    });
    frames.invalidate({*target, *child}, next->generation());
    // Frame 1: the stale target is the prewarm's; its plan is the child, which the gate holds.
    frames.update_chunks(next, away, 2);
    {
        std::unique_lock lock(mu);
        REQUIRE(cv.wait_for(lock, std::chrono::seconds(60), [&] { return held; }));
    }
    // Frame 2: take_done finds nothing; then, before the submit, the child finishes. The submit
    // skips it as done and queues nothing: no work pending.
    w->set_test_before_submit([&] {
        {
            const std::lock_guard lock(mu);
            open = true;
        }
        cv.notify_all();
        w->wait_idle();
    });
    frames.update_chunks(next, away, 2);
    w->set_test_before_submit(nullptr);
    CHECK_FALSE(frames.busy());
    CHECK(frames.stale_generation(*target) != 0);
    // Frame 3 on: the child arrives and the target is reduced from it, in the background.
    settle(next);
    CHECK(frames.stale_generation(*child) == 0);
    CHECK(frames.stale_generation(*target) == 0);
    REQUIRE(frames.chunk(*target) != nullptr);
    CHECK(frames.chunk(*target)->generation == next->generation());
    CHECK(frames.chunk_rebuilds(*target) - rebuilds_before == 1); // once, no loop
}

} // namespace

TEST_CASE("prewarm: a child finished between take_done and submit does not give the target up" *
          doctest::test_suite("derived_freshness")) {
    // A 40 x 30 island without terrain records: its chunks build (ground without tiles), and its
    // first derived level has a chunk whose children lie on it.
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    anno::formats::insel::Island island;
    island.head = (*bytes)[0];
    island.number = 0;
    island.x = 100;
    island.y = 40;
    island.width = 40;
    island.height = 30;
    auto scene =
        render::Scene::create("prewarm gap", {island}, anno::assets::island::GroundTextures{},
                              anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
    scene->set_levels(render::ZoomLevels::with_derived(2));
    prewarm_gap(scene);
}

TEST_CASE("prewarm: a child finished between take_done and submit does not give the target up "
          "(game files)" *
          doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    prewarm_gap(island_scene(*dir));
}

namespace {

/// Every field of two chunks but the serial and the generation.
bool same_chunk(const render::Chunk& a, const render::Chunk& b) {
    const bool draws = a.draws.has_value() == b.draws.has_value() &&
                       (!a.draws || a.draws->object_count() == b.draws->object_count());
    return a.ground == b.ground && a.objects == b.objects && a.field == b.field &&
           a.terrain == b.terrain && a.water_below == b.water_below &&
           a.water_above == b.water_above && a.ships == b.ships && a.base == b.base && draws &&
           a.built_by == b.built_by && a.flow.uv == b.flow.uv && a.flow.aux == b.flow.aux &&
           a.flow.shear == b.flow.shear && a.flow_waiting == b.flow_waiting;
}

/// The keys of `group` in a ring of `width` chunks around the scene's bounds: none touches them.
std::vector<std::pair<int, int>> ring_outside(const render::Scene& scene, std::size_t group,
                                              int width) {
    std::vector<std::pair<int, int>> out;
    const auto b = scene.bounds(group);
    REQUIRE(b.has_value());
    const int pad = width * render::kChunk;
    for (const auto& [cx, cy] :
         render::visible_chunks({b->x - pad, b->y - pad, b->w + 2 * pad, b->h + 2 * pad})) {
        if (!render::chunk_rect(cx, cy).intersect(*b)) {
            out.emplace_back(cx, cy);
        }
    }
    return out;
}

} // namespace

// Owner playtest 2026-11-03, round 6: most chunk builds of a zoom out were open sea around the map,
// each far chunk built from the 4^n chunks below it. A chunk outside Scene::bounds is now returned
// as open sea at once; this checks it is what the full build gives, on a scenario with ships and
// soldiers (units widen the bounds), at every level, for both builders and both ship settings.
TEST_CASE("a chunk outside the scene is the open sea a full build gives, at every level (game "
          "files)" *
          doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const render::test::Game game(*dir);
    auto loaded = game.scene(*dir / "Scenes" / "0200Single" / "Single_01.sze");
    REQUIRE_FALSE(loaded->units.empty());
    loaded->river_cfg.enabled = false;
    loaded->set_levels(render::ZoomLevels::with_derived(4));
    const render::Scene& scene = *loaded;
    std::size_t compared = 0;
    for (std::size_t g = 0; g < scene.levels().size(); ++g) {
        // The ring next to the bounds; at a derived level a full build reaches down to group 2
        // (4^(g-2) chunks each), so a few keys of it there.
        auto keys = ring_outside(scene, g, 1);
        if (g >= scene.levels().built_count() && keys.size() > 6) {
            keys = {keys.front(), keys[keys.size() / 3], keys[keys.size() / 2],
                    keys[2 * keys.size() / 3], keys.back()};
        }
        REQUIRE_FALSE(keys.empty());
        for (const auto& [cx, cy] : keys) {
            CHECK(render::chunk_outside_scene(scene, g, cx, cy));
            for (const bool ships : {true, false}) {
                for (const auto builder :
                     {render::ChunkBuilder::Cpu, render::ChunkBuilder::GpuList}) {
                    render::ChunkBuildOptions quick;
                    quick.ships = ships;
                    quick.builder = builder;
                    render::ChunkBuildOptions full = quick;
                    full.skip_outside = false;
                    const render::Chunk a = render::render_chunk(scene, g, cx, cy, quick);
                    const render::Chunk b = render::render_chunk(scene, g, cx, cy, full);
                    CHECK_MESSAGE(same_chunk(a, b), "group " << g << " chunk " << cx << "," << cy
                                                             << " ships " << ships);
                    CHECK(a.serial != 0);
                    ++compared;
                }
            }
        }
        // The chunks that touch the bounds are not outside.
        const auto b = scene.bounds(g);
        for (const auto& [cx, cy] : render::visible_chunks(*b)) {
            CHECK_FALSE(render::chunk_outside_scene(scene, g, cx, cy));
        }
    }
    MESSAGE(compared << " chunk builds outside the scene compared with the full build");
}

// The planner: a view of a far level wholly outside the scene asks for its chunks only, never for
// the chunks below them (before: 4^n children each, down to group 2).
TEST_CASE("the planner builds a far chunk outside the scene without its children (game files)" *
          doctest::test_suite("derived_freshness")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto scene = island_scene(*dir);
    const std::size_t top = scene->levels().size() - 1;
    const auto b = scene->bounds(top);
    REQUIRE(b.has_value());
    // A view of the farthest level, three of its chunks to the right of the scene.
    const render::Rect view{b->x + b->w + 3 * render::kChunk, b->y, 600, 400};
    const auto visible = render::visible_chunks(view);
    render::FrameRenderer frames(2);
    auto workers = std::make_unique<render::ChunkWorkers>(2);
    render::ChunkWorkers* w = workers.get();
    frames.set_workers(std::move(workers));
    for (int round = 0; round < 20; ++round) {
        frames.update_chunks(scene, view, top);
        w->wait_idle();
    }
    for (const auto& [cx, cy] : visible) {
        REQUIRE(frames.chunk({top, cx, cy}) != nullptr);
        CHECK(frames.chunk_builds({top, cx, cy}) == 1);
    }
    // Nothing two levels or more below the view was built (the level just below is the zoom's
    // prefetch of the neighbouring group, outside too): before, every visible chunk was reduced
    // from its children, and they from theirs, down to group 2.
    std::size_t below = 0, depth = 0;
    std::vector<render::ChunkKey> layer;
    for (const auto& [cx, cy] : visible) {
        layer.push_back({top, cx, cy});
    }
    while (!layer.empty() && std::get<0>(layer.front()) >= scene->levels().built_count()) {
        std::vector<render::ChunkKey> next;
        for (const auto& k : layer) {
            for (const auto& kid : render::derived_children(k, scene->levels())) {
                below += depth >= 1 ? frames.chunk_builds(kid) : 0U;
                next.push_back(kid);
            }
        }
        layer = std::move(next);
        ++depth;
    }
    REQUIRE(depth >= 2);
    CHECK(below == 0);
}
