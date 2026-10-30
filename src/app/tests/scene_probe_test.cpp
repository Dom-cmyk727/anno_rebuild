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
