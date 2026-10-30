// Measurement probe for the W4.3 picture design (docs/app/game-shell.md, "Building
// changes after tick 0"): what one whole-scene rebuild costs on the fixed map,
// and what a 50-build burst costs when it is coalesced into one rebuild. It is
// not a gate: it runs only with ANNO_PROBE=1 and the game files, prints its
// figures with MESSAGE, and passes.
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>
#include <anno/test/budget.hpp>
#include <anno/test/game_dir.hpp>

namespace {

namespace fs = std::filesystem;
using clock_type = std::chrono::steady_clock;

double ms_since(clock_type::time_point start) {
    return std::chrono::duration<double, std::milli>(clock_type::now() - start).count();
}

fs::path fixed_map() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

} // namespace

TEST_CASE("probe: whole-scene rebuild and a coalesced 50-build burst on the fixed map" *
          doctest::test_suite("probe")) {
    if (std::getenv("ANNO_PROBE") == nullptr) {
        MESSAGE("skipping: set ANNO_PROBE=1 to measure");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: no game files");
        return;
    }
    auto loaded = anno::app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    anno::app::PlayLoad play = std::move(loaded).value();
    std::vector<anno::app::SceneBuilding> buildings = anno::app::play_buildings(play.world);
    MESSAGE("tick-0 buildings: " << buildings.size() << ", islands: " << play.placed.size());

    auto build = [&](const std::vector<anno::app::SceneBuilding>& list) {
        anno::app::SceneObjects objects;
        objects.is_building = [&](std::uint16_t id) {
            return anno::app::anlage_is_building(play.world, id);
        };
        objects.buildings = list;
        return anno::app::load_placed_scene(*dir, play.name, play.placed, &objects);
    };

    // One change: the whole scene from the buildings, as --play builds it at tick 0.
    for (int rep = 0; rep < 5; ++rep) {
        const auto start = clock_type::now();
        auto built = build(buildings);
        const double ms = ms_since(start);
        REQUIRE(built.ok());
        MESSAGE("probe one change, scene build " << rep << ": " << ms << " ms");
    }

    // The constructor alone, from the parts a built scene holds (no file reads, no parse):
    // what a rebuild costs when the static assets are reused.
    {
        auto built = build(buildings);
        REQUIRE(built.ok());
        const anno::render::Scene& held = *built.value().scene;
        const auto start = clock_type::now();
        anno::render::Scene again(held.name, held.islands, held.textures, held.anlagen,
                                  anno::render::kHeightLift);
        MESSAGE("probe Scene constructor from cached parts (copies, no file read): "
                << ms_since(start) << " ms");
        static_cast<void>(again.islands.size());
    }

    // The first frame's preparation of a fresh scene (river flow, sea pattern).
    {
        auto built = build(buildings);
        REQUIRE(built.ok());
        const auto start = clock_type::now();
        built.value().scene->prefetch_river_flow();
        MESSAGE("probe fresh scene, prefetch_river_flow: " << ms_since(start) << " ms");
        const auto sea_start = clock_type::now();
        static_cast<void>(built.value().scene->sea(2));
        MESSAGE("probe fresh scene, sea(group 2): " << ms_since(sea_start) << " ms");
    }

    // A burst of 50 builds in one turn, coalesced into one rebuild.
    std::vector<anno::app::SceneBuilding> burst = buildings;
    for (int i = 0; i < 50 && !buildings.empty(); ++i) {
        anno::app::SceneBuilding extra = buildings.front();
        extra.x += i + 1;
        burst.push_back(extra);
    }
    {
        const auto start = clock_type::now();
        auto built = build(burst);
        const double ms = ms_since(start);
        REQUIRE(built.ok());
        MESSAGE("probe 50-build burst, one coalesced scene build: " << ms << " ms (" << burst.size()
                                                                    << " buildings)");
    }

    // Memory of one live scene: the private-bytes delta of holding it.
    const auto before = anno::test::process_private_bytes();
    {
        auto built = build(buildings);
        REQUIRE(built.ok());
        const auto held = anno::test::process_private_bytes();
        if (before && held) {
            MESSAGE("probe one scene held: private bytes +" << (*held - *before) / (1024 * 1024)
                                                            << " MiB");
        }
    }
}

// W4.3 round du, step 7: the object-list path through ScenePicture. One change on one island, and a
// 50-build burst coalesced into one rebuild per island. Milliseconds, and frames at 60 Hz (16.7 ms)
// and 120 Hz (8.3 ms). A probe, not a gate (ANNO_PROBE=1).
TEST_CASE("probe: per-island object-list build, commit and a 50-build burst (ScenePicture)" *
          doctest::test_suite("probe")) {
    if (std::getenv("ANNO_PROBE") == nullptr) {
        MESSAGE("skipping: set ANNO_PROBE=1 to measure");
        return;
    }
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: no game files");
        return;
    }
    auto loaded = anno::app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    anno::app::PlayLoad play = std::move(loaded).value();
    std::vector<anno::app::SceneBuilding> buildings = anno::app::play_buildings(play.world);
    anno::app::sort_buildings(buildings);
    anno::app::SceneObjects objects;
    objects.is_building = [&](std::uint16_t id) {
        return anno::app::anlage_is_building(play.world, id);
    };
    objects.buildings = buildings;
    auto built = anno::app::load_placed_scene(*dir, play.name, play.placed, &objects);
    REQUIRE(built.ok());
    anno::app::LoadedScene ls = std::move(built).value();
    ls.scene->river_cfg.enabled = true;
    const std::shared_ptr<const anno::render::Scene> base(std::move(ls.scene));
    const auto is_building = [&](std::uint16_t id) {
        return anno::app::anlage_is_building(play.world, id);
    };
    const double frame60 = 1000.0 / 60.0, frame120 = 1000.0 / 120.0;

    for (int threads : {1, 2}) {
        anno::app::ScenePicture picture(base, buildings, is_building,
                                        static_cast<std::size_t>(threads));
        anno::render::FrameRenderer frames(1);
        // One change: the first building moves two tiles right.
        auto one = buildings.front();
        one.x = static_cast<std::uint16_t>(one.x + 2);
        auto batch1 = std::make_shared<anno::app::BuildingBatch>();
        batch1->turn = 1;
        batch1->deltas = {{anno::app::BuildingDelta::Kind::Removed, buildings.front()},
                          {anno::app::BuildingDelta::Kind::Added, one}};
        const auto t0 = std::chrono::steady_clock::now();
        picture.apply({batch1});
        picture.wait_idle();
        const double wait_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count();
        const std::size_t n = picture.commit(frames);
        const double total_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count();
        const auto st = picture.stats();
        MESSAGE("probe threads=" << threads << " one change: islands=" << n
                                 << " build=" << st.last_build_ms
                                 << " ms commit=" << st.last_commit_ms << " ms wait=" << wait_ms
                                 << " ms total=" << total_ms << " ms (" << total_ms / frame60
                                 << " frames at 60 Hz, " << total_ms / frame120
                                 << " at 120 Hz) invalidated keys=" << st.invalidated_keys);

        // A burst: 50 added buildings in one batch (one turn), coalesced per island.
        auto burst = std::make_shared<anno::app::BuildingBatch>();
        burst->turn = 2;
        for (int i = 0; i < 50; ++i) {
            auto extra = buildings.front();
            extra.x = static_cast<std::uint16_t>(extra.x + i + 1);
            burst->deltas.push_back({anno::app::BuildingDelta::Kind::Added, extra});
        }
        const auto t1 = std::chrono::steady_clock::now();
        const std::size_t requested = picture.apply({burst});
        picture.wait_idle();
        const std::size_t committed = picture.commit(frames);
        const double burst_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1)
                .count();
        const auto sb = picture.stats();
        MESSAGE("probe threads=" << threads << " 50-build burst: islands requested=" << requested
                                 << " committed=" << committed << " build=" << sb.last_build_ms
                                 << " ms commit=" << sb.last_commit_ms << " ms total=" << burst_ms
                                 << " ms (" << burst_ms / frame60 << " frames at 60 Hz, "
                                 << burst_ms / frame120 << " at 120 Hz)");
    }
}
