// The legal city workload (round dy, B2-B5; docs/perf/README.md, "Legal city workload"). Five
// cities on five islands of the fixed map, built through the sim's Build commands with the
// registry's types; then one accepted build and one accepted demolish, each reaching the picture
// through the bridge and compared with a freshly loaded scene of the desired state (objects, chunk
// layers and PROD2 items). Needs the game files (skips without them).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/legal_workload.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace render = anno::render;
namespace app = anno::app;

namespace {

fs::path fixed_map() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

/// The fixed map's scene with `buildings` as its desired list, loaded independently of any
/// picture (the oracle), rivers off as in the verifier (river flow is asynchronous, tested apart).
std::shared_ptr<const render::Scene> scene_with(const fs::path& dir, const app::PlayLoad& play,
                                                const std::vector<app::SceneBuilding>& buildings) {
    app::SceneObjects objects;
    objects.is_building = [&play](std::uint16_t id) {
        return app::anlage_is_building(play.world, id);
    };
    objects.buildings = buildings;
    auto loaded = app::load_placed_scene(dir, play.name, play.placed, &objects);
    REQUIRE(loaded.ok());
    app::LoadedScene ls = std::move(loaded).value();
    ls.scene->river_cfg.enabled = false;
    return std::shared_ptr<const render::Scene>(std::move(ls.scene));
}

render::FrameRenderer rendered(const render::Scene& scene, render::Rect view) {
    render::FrameRenderer frames(1);
    for (int round = 0; round < 3; ++round) {
        frames.update_chunks(scene, view, 2);
    }
    return frames;
}

app::ChunkLookup lookup_of(const render::FrameRenderer& frames) {
    return [&frames](const render::ChunkKey& key) { return frames.chunk(key); };
}

/// A group-2 view of 1024 x 768 scene pixels centred on island-local tile (x, y) of the island
/// numbered `number` (the anchor formula of Scene::object_bounds for a 1 x 1 footprint).
render::Rect view_at(const render::Scene& scene, std::uint32_t number, std::int32_t x,
                     std::int32_t y) {
    std::size_t index = scene.islands.size();
    for (std::size_t i = 0; i < scene.islands.size(); ++i) {
        if (scene.islands[i].number == number) {
            index = i;
        }
    }
    REQUIRE(index < scene.islands.size());
    for (const render::Placement& p : scene.layouts[2]) {
        if (p.index == index) {
            const auto [tw, th] = render::kTile[2];
            const int cx2 = 2 * x + 1, cy2 = 2 * y + 1;
            const int sx = (cx2 - cy2) * tw / 4 + p.offset.first;
            const int sy = (cx2 + cy2) * th / 4 + p.offset.second;
            return render::Rect{sx - 512, sy - 384, 1024, 768};
        }
    }
    FAIL("no layout for the island");
    return render::Rect{};
}

bool drain(app::ScenePicture& picture, render::FrameRenderer& frames) {
    for (int round = 0; round < 40 && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    return picture.pending() == 0;
}

/// The picture's committed generation equals a fresh scene of the desired state: every island's
/// PROD2 items, and the chunk layers of `view` (all keys present, objects in view). The edit
/// changed the expected pixels: the committed view differs from `before` in object pixels.
void check_against_fresh(const fs::path& dir, const app::LegalWorkload& w,
                         const app::ScenePicture& picture, const render::Scene& before,
                         const app::LegalEdit& edit) {
    const auto fresh = scene_with(dir, w.load(), w.buildings());
    const render::Scene& committed = *picture.scene();
    REQUIRE(committed.islands.size() == fresh->islands.size());
    std::size_t differing = 0;
    for (std::size_t i = 0; i < fresh->islands.size(); ++i) {
        differing +=
            app::building_items(committed.islands[i]) == app::building_items(fresh->islands[i])
                ? 0U
                : 1U;
    }
    CHECK(differing == 0);
    const render::Rect view = view_at(*fresh, edit.island, edit.x, edit.y);
    const auto keys = render::chunk_keys_touching(*fresh, view, 2);
    REQUIRE(!keys.empty());
    const render::FrameRenderer ref = rendered(*fresh, view);
    const render::FrameRenderer got = rendered(committed, view);
    const render::FrameRenderer old = rendered(before, view);
    const app::ChunkDiff same = app::compare_chunks(keys, lookup_of(got), lookup_of(ref));
    CAPTURE(same.compared);
    CAPTURE(same.ground);
    CAPTURE(same.objects);
    CAPTURE(same.missing);
    CHECK(same.equal());
    CHECK(same.compared == keys.size());
    CHECK(same.missing == 0);
    CHECK(same.with_objects > 0);
    const app::ChunkDiff changed = app::compare_chunks(keys, lookup_of(got), lookup_of(old));
    CHECK(changed.objects > 0);
    MESSAGE("edit " << edit.what << " island=" << edit.island << " tile=" << edit.x << "," << edit.y
                    << " type=" << edit.type << " keys=" << keys.size() << " with_objects="
                    << same.with_objects << " changed_object_keys=" << changed.objects);
}

/// A gate that lets builds through one at a time: build n (from 0) waits until `released` > n.
/// Opened for good when the opener declared after the picture goes out of scope.
struct Turnstile {
    std::atomic<bool> open{false};
    std::atomic<int> entered{0}, released{0};
    void pass() {
        const int me = entered.fetch_add(1);
        while (!open.load() && released.load() <= me) {
            std::this_thread::yield();
        }
    }
};
struct TurnstileOpener {
    Turnstile& t;
    TurnstileOpener(const TurnstileOpener&) = delete;
    TurnstileOpener& operator=(const TurnstileOpener&) = delete;
    explicit TurnstileOpener(Turnstile& turnstile) : t(turnstile) {}
    ~TurnstileOpener() { t.open.store(true); }
};

template <class F> bool eventually(F done) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (!done()) {
        if (std::chrono::steady_clock::now() > until) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

/// A committed generation equals a fresh scene of `buildings` (the state it represents): every
/// island's PROD2 items, and the chunk layers of each view, both rendered from the committed scene
/// alone and as the bridge's own frame renderer `frames` shows them after its invalidations.
void check_state(const fs::path& dir, const app::LegalWorkload& w,
                 const std::shared_ptr<const render::Scene>& committed,
                 const std::vector<app::SceneBuilding>& buildings,
                 const std::vector<render::Rect>& views, render::FrameRenderer& frames) {
    const auto fresh = scene_with(dir, w.load(), buildings);
    REQUIRE(committed->islands.size() == fresh->islands.size());
    std::size_t differing = 0;
    for (std::size_t i = 0; i < fresh->islands.size(); ++i) {
        differing +=
            app::building_items(committed->islands[i]) == app::building_items(fresh->islands[i])
                ? 0U
                : 1U;
    }
    CHECK(differing == 0);
    for (const render::Rect& view : views) {
        const auto keys = render::chunk_keys_touching(*fresh, view, 2);
        REQUIRE(!keys.empty());
        const render::FrameRenderer ref = rendered(*fresh, view);
        const render::FrameRenderer got = rendered(*committed, view);
        const app::ChunkDiff same = app::compare_chunks(keys, lookup_of(got), lookup_of(ref));
        CAPTURE(same.objects);
        CAPTURE(same.missing);
        CHECK(same.equal());
        CHECK(same.with_objects > 0);
        // The bridge's renderer: its cached chunks of the replaced generations were invalidated by
        // the commits; once brought up to date it shows the reference.
        for (int round = 0; round < 3; ++round) {
            frames.update_chunks(committed, view, 2);
        }
        const app::ChunkDiff shown = app::compare_chunks(keys, lookup_of(frames), lookup_of(ref));
        CAPTURE(shown.objects);
        CAPTURE(shown.missing);
        CHECK(shown.equal());
    }
}

} // namespace

TEST_CASE("legal workload: five cities on five islands, built through the sim's commands (game "
          "files)" *
          doctest::test_suite("legal_workload")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    app::LegalWorkloadOptions options;
    auto built = app::LegalWorkload::build(*dir, fixed_map(), options);
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const auto w = std::move(built).value();
    MESSAGE(w->manifest());
    REQUIRE(w->cities().size() == 5);
    std::set<std::uint32_t> islands;
    for (const app::LegalCity& c : w->cities()) {
        CAPTURE(c.island_slot);
        islands.insert(c.island_slot);
        // Every wanted building was placed by the sim (not a request count).
        CHECK(c.accepted == options.buildings_per_city);
        CHECK(!c.capacity_reached);
        std::size_t buildings = 0;
        for (const auto& [type, n] : c.by_type) {
            buildings += n;
        }
        CHECK(buildings == c.accepted + 1); // the Kontor
        CHECK(c.by_type.size() >= 4);       // varied types and extents
        CHECK(c.occupied_tiles > c.accepted);
    }
    CHECK(islands.size() == 5);
    CHECK(w->invariants().empty());
    // The workload is deterministic: the same seed gives the same world.
    auto again = app::LegalWorkload::build(*dir, fixed_map(), options);
    REQUIRE(again.ok());
    CHECK(again.value()->state_hash() == w->state_hash());
    CHECK(again.value()->manifest() == w->manifest());
    // Larger unrelated cities leave city 0 as it was: the same buildings and the same next edit
    // (each city's search has its own stream), so a local-edit measurement can grow the rest.
    app::LegalWorkloadOptions larger = options;
    larger.unrelated_extra = 40;
    auto big_built = app::LegalWorkload::build(*dir, fixed_map(), larger);
    REQUIRE(big_built.ok());
    const auto big = std::move(big_built).value();
    const std::uint32_t island0 = w->cities()[0].island_slot;
    auto on_island0 = [island0](const std::vector<app::SceneBuilding>& list) {
        std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint16_t, std::uint16_t>> out;
        for (const app::SceneBuilding& b : list) {
            if (b.island == island0) {
                out.emplace_back(b.type, b.rotation, b.x, b.y);
            }
        }
        return out;
    };
    MESSAGE(big->manifest());
    CHECK(on_island0(big->buildings()) == on_island0(w->buildings()));
    for (std::size_t c = 1; c < big->cities().size(); ++c) {
        // Every wanted building, or a capacity reported by the bounded search (never a silent
        // shortfall).
        const app::LegalCity& city = big->cities()[c];
        CHECK((city.accepted == options.buildings_per_city + 40 || city.capacity_reached));
        CHECK(city.accepted > options.buildings_per_city);
    }
    const app::LegalEdit small_edit = w->build_one(0);
    const app::LegalEdit big_edit = big->build_one(0);
    CHECK(small_edit.accepted);
    CHECK(big_edit.accepted);
    CHECK(small_edit.x == big_edit.x);
    CHECK(small_edit.y == big_edit.y);
    CHECK(small_edit.type == big_edit.type);
}

TEST_CASE("legal workload: one accepted build and one accepted demolish reach the picture as a "
          "fresh scene of the desired state (game files)" *
          doctest::test_suite("legal_workload")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    app::LegalWorkloadOptions options;
    options.buildings_per_city = 12;
    options.settle_turns = 5;
    auto built = app::LegalWorkload::build(*dir, fixed_map(), options);
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const auto w = std::move(built).value();
    const auto base = scene_with(*dir, w->load(), w->buildings());
    app::ScenePicture picture(
        base, w->buildings(),
        [&w](std::uint16_t id) { return app::anlage_is_building(w->world(), id); }, 2);
    render::FrameRenderer frames(1);

    const app::LegalEdit build = w->build_one(0);
    REQUIRE_MESSAGE(build.accepted, build.reason);
    REQUIRE(build.batch != nullptr);
    CHECK(w->invariants().empty());
    picture.apply({build.batch});
    REQUIRE(drain(picture, frames));
    CHECK(picture.error().empty());
    CHECK(!picture.last_invalidated().empty()); // the new building draws: its keys were marked
    check_against_fresh(*dir, *w, picture, *base, build);

    const auto before_demolish = picture.scene();
    const app::LegalEdit demolish = w->demolish_one(1);
    REQUIRE_MESSAGE(demolish.accepted, demolish.reason);
    REQUIRE(demolish.batch != nullptr);
    CHECK(w->invariants().empty());
    const auto keys_before = picture.stats().invalidated_keys;
    picture.apply({demolish.batch});
    REQUIRE(drain(picture, frames));
    CHECK(picture.error().empty());
    CHECK(picture.stats().invalidated_keys > keys_before);
    check_against_fresh(*dir, *w, picture, *before_demolish, demolish);
}

TEST_CASE("legal workload: intermediate publications under a stream of builds and a demolish each "
          "equal a fresh scene of the state they represent (game files)" *
          doctest::test_suite("legal_workload")) {
    // Monotonic publication (the default) on real data. Four accepted edits of city 0 (build,
    // build, demolish, build) reach the bridge while its one worker is held in a turnstile, so
    // the desired version is always ahead of the finished build. Version 1 and version 3 are
    // published as intermediate generations (version 2 ran while version 1 was committed and is
    // requeued; docs/app/game-shell.md, "Publication policy"), then version 4. Each published
    // generation is compared with a freshly loaded scene of the desired list as it was at that
    // version, and the bridge's own chunk cache with the reference rendering of that scene.
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    app::LegalWorkloadOptions options;
    options.buildings_per_city = 12;
    options.settle_turns = 5;
    auto built = app::LegalWorkload::build(*dir, fixed_map(), options);
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const auto w = std::move(built).value();
    const auto base = scene_with(*dir, w->load(), w->buildings());
    // The edits and the desired list after each (state k is the desired list at version k).
    std::vector<app::LegalEdit> edits;
    std::vector<std::vector<app::SceneBuilding>> states{w->buildings()};
    for (int k = 0; k < 4; ++k) {
        edits.push_back(k == 2 ? w->demolish_one(0) : w->build_one(0));
        REQUIRE_MESSAGE(edits.back().accepted, edits.back().reason);
        REQUIRE(edits.back().batch != nullptr);
        CHECK(edits.back().island == edits.front().island);
        states.push_back(w->buildings());
    }
    CHECK(w->invariants().empty());
    std::vector<render::Rect> views;
    for (const app::LegalEdit& e : edits) {
        views.push_back(view_at(*base, e.island, e.x, e.y));
    }
    Turnstile turnstile;
    app::ScenePicture::Limits limits;
    limits.build_fault = [&turnstile](std::size_t) { turnstile.pass(); };
    app::ScenePicture picture(
        base, states.front(),
        [&w](std::uint16_t id) { return app::anlage_is_building(w->world(), id); }, 1, limits);
    const TurnstileOpener opener(turnstile);
    render::FrameRenderer frames(1);
    for (const render::Rect& view : views) {
        for (int round = 0; round < 3; ++round) {
            frames.update_chunks(base, view, 2); // the base's chunks are cached, then invalidated
        }
    }
    picture.apply({edits[0].batch});
    REQUIRE(eventually([&] { return turnstile.entered.load() == 1; }));
    std::vector<std::uint64_t> published;
    for (std::size_t k = 1; k < edits.size(); ++k) {
        CAPTURE(k);
        picture.apply({edits[k].batch});
        turnstile.released.fetch_add(1);
        REQUIRE(eventually([&] { return turnstile.entered.load() == static_cast<int>(k + 1); }));
        // The reaper and the process reclaimer catch up first, so the commit is not skipped for
        // reclamation this case does not test.
        REQUIRE(eventually([&] {
            const auto st = picture.stats();
            const auto r = render::SceneReclaimer::shared().stats();
            return st.reclaim_completed == st.reclaim_queued && r.waiting == 0;
        }));
        if (picture.commit(frames) == 0) {
            continue;
        }
        REQUIRE(picture.last_commit().size() == 1);
        const std::uint64_t version = picture.last_commit().front().version;
        published.push_back(version);
        CHECK(version < k + 1); // intermediate: newer edits are owed
        CHECK(picture.pending() == 1);
        CHECK(!picture.last_commit().front().keys.empty());
        check_state(*dir, *w, picture.scene(), states[version], views, frames);
        CHECK(picture.buildings() == states[k + 1]); // the desired state is version k + 1
    }
    turnstile.open.store(true);
    REQUIRE(drain(picture, frames));
    CHECK(picture.error().empty());
    CHECK(published == std::vector<std::uint64_t>{1, 3});
    check_state(*dir, *w, picture.scene(), states.back(), views, frames);
    MESSAGE("intermediate versions published: " << published.size() << ", requeues "
                                                << picture.stats().version_requeues);
}
