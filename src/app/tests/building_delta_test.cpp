// Building changes for the play picture (docs/app/game-shell.md, "Building
// changes after tick 0"). The synthetic cases need no game files; the scenario
// cases skip when ANNO1503_GAME_DIR is unset.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/play.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;

namespace {

anno::app::SceneBuilding building(std::uint32_t island, std::uint16_t x, std::uint16_t y,
                                  std::uint32_t type, std::uint8_t rotation = 0,
                                  std::uint8_t height = 0) {
    anno::app::SceneBuilding b;
    b.island = island;
    b.x = x;
    b.y = y;
    b.type = type;
    b.rotation = rotation;
    b.height = height;
    return b;
}

std::vector<anno::app::SceneBuilding> sorted(std::vector<anno::app::SceneBuilding> list) {
    anno::app::sort_buildings(list);
    return list;
}

} // namespace

TEST_CASE("building delta: equal lists give no change") {
    const auto prev = sorted({building(0, 1, 1, 5), building(1, 4, 2, 6, 2, 3)});
    const auto cur = sorted({building(1, 4, 2, 6, 2, 3), building(0, 1, 1, 5)});
    CHECK(anno::app::diff_sorted_buildings(prev, cur).empty());
}

TEST_CASE("building delta: add, remove, and a rotation change as a pair") {
    const auto prev = sorted({building(0, 1, 1, 5, 0), building(0, 2, 2, 6, 0)});
    const auto cur =
        sorted({building(0, 1, 1, 5, 1), building(0, 2, 2, 6, 0), building(0, 3, 3, 7, 0)});
    const auto deltas = anno::app::diff_sorted_buildings(prev, cur);
    REQUIRE(deltas.size() == 3);
    // Removed first, then added, each in building order.
    CHECK(deltas[0].kind == anno::app::BuildingDelta::Kind::Removed);
    CHECK(deltas[0].building == building(0, 1, 1, 5, 0));
    CHECK(deltas[1].kind == anno::app::BuildingDelta::Kind::Added);
    CHECK(deltas[1].building == building(0, 1, 1, 5, 1));
    CHECK(deltas[2].kind == anno::app::BuildingDelta::Kind::Added);
    CHECK(deltas[2].building == building(0, 3, 3, 7, 0));
}

TEST_CASE("building delta: a building listed twice on one side is counted") {
    const auto twice = sorted({building(0, 1, 1, 5), building(0, 1, 1, 5)});
    const auto once = sorted({building(0, 1, 1, 5)});
    const auto removed = anno::app::diff_sorted_buildings(twice, once);
    REQUIRE(removed.size() == 1);
    CHECK(removed[0].kind == anno::app::BuildingDelta::Kind::Removed);
    const auto added = anno::app::diff_sorted_buildings(once, twice);
    REQUIRE(added.size() == 1);
    CHECK(added[0].kind == anno::app::BuildingDelta::Kind::Added);
}

TEST_CASE("building delta: the order is total on the record") {
    CHECK(anno::app::building_before(building(0, 1, 1, 5), building(0, 1, 1, 6)));
    CHECK(anno::app::building_before(building(0, 1, 1, 5, 0), building(0, 1, 1, 5, 1)));
    CHECK(anno::app::building_before(building(0, 1, 1, 5, 1, 0), building(0, 1, 1, 5, 1, 1)));
    CHECK(anno::app::building_before(building(0, 9, 9, 9), building(1, 0, 0, 0)));
    CHECK_FALSE(anno::app::building_before(building(0, 1, 1, 5), building(0, 1, 1, 5)));
}

TEST_CASE("play: a turn with no commands publishes no building changes") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = *dir / "Scenes" / "0200Single" / "Single_01.sze";
    if (!fs::exists(file)) {
        MESSAGE("skipping: Single_01.sze is not in this game directory");
        return;
    }
    auto opened = anno::app::load_play(*dir, file);
    REQUIRE(opened);
    anno::app::PlayRunner runner(std::move(opened.value().world));
    runner.run_n(50);
    const auto batches = runner.take_building_batches();
    std::size_t deltas = 0;
    for (const auto& batch : batches) {
        deltas += batch->deltas.size();
    }
    MESSAGE("Single_01, 50 turns, no commands: " << batches.size() << " batches, " << deltas
                                                 << " deltas");
    CHECK(deltas == 0);
    CHECK(runner.take_building_batches().empty());
}

TEST_CASE("play: the building diff on the heavy endless scenario") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const fs::path file = *dir / "Scenes" / "0010SingleEndlos" / "06_1024_heavy_256.sze";
    if (!fs::exists(file)) {
        MESSAGE("skipping: 06_1024_heavy_256.sze is not in this game directory");
        return;
    }
    auto opened = anno::app::load_play(*dir, file);
    REQUIRE(opened);
    // The list the runner diffs against, and a second one as the "after" side:
    // both are the same world, so the diff is empty and only its cost is measured.
    // At load the endless scenario has no city, so no buildings: this is the
    // floor of the cost (the list walk and the sort of nothing). The realistic
    // size is measured below with synthetic lists.
    auto before = anno::app::play_buildings(opened.value().world);
    anno::app::sort_buildings(before);
    constexpr int kRuns = 1000;
    std::vector<double> ms;
    ms.reserve(kRuns);
    std::size_t total = 0;
    for (int i = 0; i < kRuns; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        auto cur = anno::app::play_buildings(opened.value().world);
        anno::app::sort_buildings(cur);
        const auto deltas = anno::app::diff_sorted_buildings(before, cur);
        total += deltas.size();
        ms.push_back(
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                .count());
    }
    std::sort(ms.begin(), ms.end());
    const auto at = [&](std::size_t numer, std::size_t denom) {
        std::size_t i = (ms.size() * numer + denom - 1) / denom;
        if (i == 0) {
            i = 1;
        }
        return ms[std::min(ms.size(), i) - 1];
    };
    MESSAGE("heavy: " << before.size() << " buildings; diff (list, sort, diff) ms over " << kRuns
                      << " runs p50 " << at(1, 2) << " p95 " << at(95, 100) << " max " << ms.back()
                      << "; deltas " << total);
    CHECK(total == 0);
}

TEST_CASE("building delta: cost at 1000 and 5000 buildings, one changed per turn") {
    // Synthetic lists, the size a 5-player map reaches (game-shell.md: order of
    // 10^3 buildings). One building moves each turn; the diff must find exactly it.
    for (const std::uint32_t n : {1000u, 5000u}) {
        std::vector<anno::app::SceneBuilding> before;
        before.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            before.push_back(building(i % 5, static_cast<std::uint16_t>(i % 512),
                                      static_cast<std::uint16_t>(i / 512), 100 + i % 7));
        }
        anno::app::sort_buildings(before);
        constexpr int kRuns = 200;
        std::vector<double> ms;
        ms.reserve(kRuns);
        for (int run = 0; run < kRuns; ++run) {
            auto cur = before;
            cur[static_cast<std::size_t>(run) % cur.size()].rotation = 1;
            const auto t0 = std::chrono::steady_clock::now();
            auto sorted_cur = cur;
            anno::app::sort_buildings(sorted_cur);
            const auto deltas = anno::app::diff_sorted_buildings(before, sorted_cur);
            ms.push_back(
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                    .count());
            CHECK(deltas.size() == 2);
        }
        std::sort(ms.begin(), ms.end());
        MESSAGE("synthetic " << n << " buildings: sort + diff ms p50 " << ms[ms.size() / 2]
                             << " p95 " << ms[ms.size() * 95 / 100] << " max " << ms.back());
    }
}
