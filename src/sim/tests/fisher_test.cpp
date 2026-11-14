// FISHER-1a: pure geometry against the accepted independent reference and imports.
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/fisher.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

namespace {
namespace sim = anno::sim;
using Fields = std::map<std::string, std::string>;
Fields fields(std::istringstream& in) {
    Fields out;
    for (std::string word; in >> word;) {
        const auto split = word.find('=');
        REQUIRE(split != std::string::npos);
        REQUIRE(out.emplace(word.substr(0, split), word.substr(split + 1)).second);
    }
    return out;
}
std::uint32_t number(const Fields& f, const std::string& name) {
    REQUIRE(f.contains(name));
    const auto n = std::stoull(f.at(name));
    REQUIRE(n <= UINT32_MAX);
    return static_cast<std::uint32_t>(n);
}
std::vector<std::uint8_t> bytes(const std::string& text) {
    std::vector<std::uint8_t> out;
    std::istringstream in(text);
    for (std::string value; std::getline(in, value, ',');) {
        const auto n = std::stoul(value);
        REQUIRE(n <= UINT8_MAX);
        out.push_back(static_cast<std::uint8_t>(n));
    }
    return out;
}
} // namespace

TEST_CASE("fisher geometry: every sea-cell, corner-grid and search-rectangle vector") {
    std::ifstream input(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim/fisher_vectors.txt");
    REQUIRE(input);
    std::uint32_t cells = 0, grids = 0, rects = 0, skipped = 0;
    bool border31 = false, border32 = false;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream prefix(line);
        std::string kind, name;
        REQUIRE(static_cast<bool>(prefix >> kind >> name));
        INFO(kind, " ", name);
        if (kind == "farm_tick" || kind == "converted" || kind == "trip" || kind == "cut_match") {
            ++skipped;
            continue;
        }
        REQUIRE((kind == "sea_cell" || kind == "corner_grid" || kind == "search_rect"));
        const auto arrow = line.find("=>");
        REQUIRE(arrow != std::string::npos);
        std::string rest;
        std::getline(prefix, rest);
        std::istringstream left(rest.substr(0, rest.find("=>")));
        std::istringstream right(line.substr(arrow + 2));
        const auto given = fields(left), expected = fields(right);
        REQUIRE(given.at("draws") == "-");
        CHECK(number(expected, "consumed") == 0);
        if (kind == "sea_cell") {
            const auto h = bytes(given.at("heights"));
            REQUIRE(h.size() == 4);
            const auto cell = sim::fisher_cell({h[0], h[1], h[2], h[3]});
            CHECK(cell.walkable_water == (number(expected, "open") != 0));
            CHECK(cell.goal_mask == number(expected, "free"));
            if (name == "border_below") {
                CHECK(cell.mean == 31);
                border31 = true;
            }
            if (name == "border_equal") {
                CHECK(cell.mean == 32);
                border32 = true;
            }
            ++cells;
        } else if (kind == "corner_grid") {
            const auto w = number(given, "width"), h = number(given, "height");
            const auto samples = bytes(given.at("heights"));
            const auto want = bytes(expected.at("grid"));
            CHECK(std::uint64_t{w} + 1 == number(expected, "stride"));
            REQUIRE(want.size() == (std::uint64_t{w} + 1) * (std::uint64_t{h} + 1));
            std::uint64_t at = 0;
            for (std::uint32_t y = 0; y <= h; ++y)
                for (std::uint32_t x = 0; x <= w; ++x) {
                    const auto corner = sim::fisher_corner(samples, w, h, x, y);
                    REQUIRE(corner);
                    CHECK(*corner == want[at++]);
                }
            ++grids;
        } else {
            const auto x = number(given, "x"), y = number(given, "y");
            REQUIRE(x <= UINT16_MAX);
            REQUIRE(y <= UINT16_MAX);
            const auto rect = sim::fisher_search_rect(
                static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
                number(given, "width"), number(given, "height"), number(given, "radius"));
            REQUIRE(rect);
            CHECK(rect->x == number(expected, "x"));
            CHECK(rect->y == number(expected, "y"));
            CHECK(rect->width == number(expected, "width"));
            CHECK(rect->height == number(expected, "height"));
            ++rects;
        }
    }
    CHECK_FALSE(input.bad());
    CHECK(cells >= 6);
    CHECK(grids >= 5);
    CHECK(rects >= 5);
    CHECK(border31);
    CHECK(border32);
    std::cout << "FISHER_VECTORS sea_cell=" << cells << " corner_grid=" << grids
              << " search_rect=" << rects << " skipped_other_types=" << skipped << '\n';
}

TEST_CASE("fisher geometry: threshold, unsigned mean and logical corner borders") {
    CHECK(sim::fisher_cell({32, 32, 32, 32}) == sim::FisherCell{32, false, 511});
    CHECK(sim::fisher_cell({32, 32, 32, 32}, 33) == sim::FisherCell{32, true, 0});
    CHECK(sim::fisher_cell({31, 32, 32, 32}).mean == 31);
    CHECK(sim::fisher_cell({255, 255, 255, 255}).mean == 255);
    CHECK_FALSE(sim::fisher_cell({0, 0, 0, 0}, 0).walkable_water);
    const std::vector<std::uint8_t> singleton{128};
    CHECK(sim::fisher_corner(singleton, 1, 1, 0, 0) == 128);
    CHECK(sim::fisher_corner(singleton, 1, 1, 1, 0) == 0);
    CHECK(sim::fisher_corner(singleton, 1, 1, 0, 1) == 0);
    CHECK(sim::fisher_corner(singleton, 1, 1, 1, 1) == 0);
    CHECK(sim::fisher_cell_at(singleton, 1, 1, 0, 0) == sim::FisherCell{32, false, 511});
    CHECK_FALSE(sim::fisher_corner(singleton, 2, 2, 0, 0));
    CHECK_FALSE(sim::fisher_corner(singleton, 1, 1, 2, 0));
    CHECK_FALSE(sim::fisher_cell_at(singleton, 1, 1, 1, 0));
    CHECK_FALSE(sim::fisher_corner({}, UINT32_MAX, UINT32_MAX, 0, 0));
}

TEST_CASE("fisher geometry: rectangle wraps without map clamping or radius caps") {
    CHECK(sim::fisher_search_rect(0, 0, 3, 4, 7) == sim::FisherSearchRect{65530, 65530, 15, 16});
    CHECK(sim::fisher_search_rect(1000, 1000, 4, 4, 127) == sim::FisherSearchRect{874, 874, 0, 0});
    CHECK(sim::fisher_search_rect(65535, 65535, 3, 3, 0) == sim::FisherSearchRect{0, 0, 1, 1});
    CHECK(sim::fisher_search_rect(0, 0, 1, 1, 256) == sim::FisherSearchRect{65280, 65280, 1, 1});
    CHECK_FALSE(sim::fisher_search_rect(0, 0, 0, 1, 7));
    CHECK_FALSE(sim::fisher_search_rect(0, 0, 1, 0, 7));
}

TEST_CASE("fisher geometry corpus: embedded fish goals and eight-neighbour water") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: original game files (ANNO1503_GAME_DIR unset)\n";
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    const auto files = anno::test::files_with_ext(*dir / "Scenes", "sze");
    REQUIRE(files.size() == 50);
    std::uint64_t fish = 0, goals = 0, adjacent = 0, failures = 0;
    for (const auto& file : files) {
        INFO(file.generic_string());
        // Owner-authorized geometry component (2026-11-14), chosen BEFORE loading,
        // never a fallback after normal import fails. Same production terrain/object
        // adapter; unsupported actors are outside this geometry-only measurement.
        // Count serialized embedded islands before procedural link selection, exactly
        // the Python census's population. This does not prove whole-game import.
        sim::UnitImportOptions units;
        units.projection = sim::OriginalActorProjection::ActorFreeComponent;
        const auto loaded =
            sim::load_scenario(anno::test::read_file(file), *registry.types, {}, units);
        if (!loaded) {
            std::cout << "FISHER_IMPORT_REFUSED " << file.filename().string() << ": "
                      << loaded.error() << '\n';
            ++failures;
            continue;
        }
        for (const auto& island : loaded.value().spec.islands) {
            REQUIRE(island.terrain);
            const auto& samples = island.terrain->height;
            REQUIRE(samples.size() == std::uint64_t{island.w} * island.h);
            for (const auto& item : island.objects) {
                if (item.object != 11)
                    continue;
                REQUIRE(island.w != 0);
                const auto x = item.index % island.w, y = item.index / island.w;
                const auto cell = sim::fisher_cell_at(samples, island.w, island.h, x, y);
                REQUIRE(cell);
                ++fish;
                goals += cell->goal_mask == 511 ? 1U : 0U;
                bool water = false;
                for (std::int64_t ny = std::int64_t{y} - 1; ny <= std::int64_t{y} + 1; ++ny)
                    for (std::int64_t nx = std::int64_t{x} - 1; nx <= std::int64_t{x} + 1; ++nx) {
                        if (nx < 0 || ny < 0 || nx >= island.w || ny >= island.h ||
                            (nx == x && ny == y))
                            continue;
                        const auto neighbour = sim::fisher_cell_at(samples, island.w, island.h,
                                                                   static_cast<std::uint32_t>(nx),
                                                                   static_cast<std::uint32_t>(ny));
                        REQUIRE(neighbour);
                        water = water || neighbour->walkable_water;
                    }
                adjacent += water ? 1U : 0U;
            }
        }
    }
    std::cout << "FISHER_CORPUS scenarios=" << files.size() << " refused=" << failures
              << " fish=" << fish << " goals=" << goals << " adjacent_water=" << adjacent
              << " without_adjacent_water=" << fish - adjacent << " neighbourhood=8\n";
    CHECK(failures == 0);
    CHECK(fish == 2473);
    CHECK(goals == 2473);
    CHECK(adjacent == 619);
    CHECK(fish - adjacent == 1854);
}
