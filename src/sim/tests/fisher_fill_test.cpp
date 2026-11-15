#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <doctest/doctest.h>

#include <anno/formats/insel.hpp>
#include <anno/sim/fisher.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "command_helpers.hpp"
#include "fisher_helpers.hpp"
namespace sim = anno::sim;
using namespace harvest_test;
namespace {
std::vector<std::uint8_t> read_bytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    REQUIRE(f.good());
    return {std::istreambuf_iterator<char>(f), {}};
}
} // namespace
TEST_CASE("fisher fill: clipped rectangles, occupied cells, missing geometry and flag off") {
    sim::TileLayer t(100, 100, 4, 3, 4);
    const std::vector<std::uint8_t> heights(12, 0);
    t.set_object(0, 16384); // the whole object word, not id & 0x3fff
    CHECK(sim::fill_fish_resources(t, heights, {99, 99, 3, 3}, false) == 0);
    CHECK(sim::fill_fish_resources(t, {}, {t.x0(), t.y0(), t.width(), t.height()}, true) == 0);
    CHECK(sim::fill_fish_resources(t, heights, {99, 99, 3, 3}, true) == 3);
    CHECK(t.object(0) == 16384);
    for (unsigned y = 0; y < 3; ++y)
        for (unsigned x = 0; x < 4; ++x) {
            if (x == 0 && y == 0)
                continue;
            CHECK(t.object(y * 4 + x) == (x < 2 && y < 2 ? 11 : 0));
        }
    CHECK(sim::fill_fish_resources(t, heights, {200, 200, 1, 1}, true) == 0);
}
TEST_CASE(
    "fisher fill: world creation fills once and native reload never regenerates a cleared cell") {
    const auto types = fishery_registry();
    auto rig = fishery_rig();
    auto& spec = rig.spec.islands[0];
    spec.terrain = sim::make_terrain(std::vector<std::uint8_t>(spec.w * spec.h, 0), {}, {});
    auto off = create_fishery(rig, types, false), on = create_fishery(rig, types, true);
    CHECK(off.rng().cursor() == on.rng().cursor());
    std::uint64_t changed = 0;
    auto& is = *sim::WorldTestAccess::islands(on)[0];
    for (std::uint64_t i = 0; i < is.tiles.tile_count(); ++i) {
        const auto old = off.islands()[0]->tiles.object(i);
        CHECK(is.tiles.object(i) == (old == 0 ? 11 : old));
        changed += old == 0;
    }
    REQUIRE(changed > 0);
    const auto hole = is.tiles.index_of(120, 120);
    is.tiles.set_object(hole, 0);
    const auto saved = on.save();
    auto loaded = sim::World::load(saved, types);
    REQUIRE(loaded);
    CHECK(loaded.value().save() == saved);
    CHECK(loaded.value().islands()[0]->tiles.object(hole) == 0);
    on.advance(2);
    CHECK(is.tiles.object(hole) == 0); // no per-tick spawn invented
}
TEST_CASE("fisher fill: public demolition refills only the affected footprint when enabled") {
    const auto types = fishery_registry();
    auto rig = fishery_rig();
    auto& spec = rig.spec.islands[0];
    spec.terrain = sim::make_terrain(std::vector<std::uint8_t>(spec.w * spec.h, 0), {}, {});
    for (bool enabled : {false, true}) {
        auto w = create_fishery(rig, types, enabled);
        auto& is = *sim::WorldTestAccess::islands(w)[0];
        const auto outside = is.tiles.index_of(120, 120);
        is.tiles.set_object(outside, 0);
        const auto cursor = w.rng().cursor();
        const auto r =
            w.apply(command_test::demolish(0, {sim::IslandId{0}, sim::CityNo{0}}, {{110, 110}}));
        REQUIRE(r.ok());
        for (int y = 110; y < 114; ++y)
            for (int x = 110; x < 113; ++x)
                CHECK(is.tiles.object(is.tiles.index_of(x, y)) == (enabled ? 11 : 0));
        CHECK(is.tiles.object(outside) == 0);
        CHECK(w.rng().cursor() == cursor);
        CHECK(w.check_invariants().empty());
    }
}
TEST_CASE("fisher fill corpus: fifty embedded maps match the accepted runtime census") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        std::cout << "SKIPPED: ANNO1503_GAME_DIR not set (original game files unavailable)\n";
        return;
    }
    const auto registry =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(registry.types, registry.error);
    std::ifstream expected(std::filesystem::path(ANNO_FIXTURES_DIR) /
                           "sim/fisher_runtime_census.txt");
    REQUIRE(expected.good());
    std::map<std::string, std::pair<std::uint64_t, std::uint64_t>> counts;
    for (std::string line; std::getline(expected, line);) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream in(line);
        std::string name;
        std::uint64_t generated = 0, usable = 0;
        REQUIRE(bool(in >> name >> generated >> usable));
        REQUIRE(counts.emplace(name, std::pair{generated, usable}).second);
    }
    const auto files = anno::test::files_with_ext(*dir / "Scenes", "sze");
    REQUIRE(files.size() == 50);
    REQUIRE(counts.size() == files.size());
    std::uint64_t total = 0;
    for (const auto& file : files) {
        const auto name = std::filesystem::relative(file, *dir).generic_string();
        INFO(name);
        const auto data = read_bytes(file);
        const auto islands = anno::formats::insel::islands(data);
        REQUIRE(islands);
        std::uint64_t generated = 0, usable = 0;
        for (const auto& raw : islands.value()) {
            const auto heights = raw.heights();
            REQUIRE(heights);
            sim::TileLayer tiles(raw.x, raw.y, raw.width, raw.height, 4);
            // Geometry projection: all raw placements, including orphan PROD2, no actor/script
            // bypass claim.
            for (const auto& record : raw.records) {
                const auto tag = record.name();
                if (tag != "GELAENDE" && tag != "FLUESSE" && tag != "PROD2")
                    continue;
                const auto block = anno::formats::insel::Block::parse(record.payload);
                REQUIRE(block);
                const auto items = block.value().data;
                for (std::size_t at = 0; at + 6 <= items.size(); at += 6) {
                    const auto o = anno::formats::insel::PlacedObject::parse(items.subspan(at, 6),
                                                                             block.value());
                    if (o.anlage == 0)
                        continue;
                    const auto* type = registry.types->find(sim::TypeId{o.anlage});
                    REQUIRE(type != nullptr);
                    static_cast<void>(sim::place_object_record(tiles, *registry.types, *type,
                                                               raw.x + o.x, raw.y + o.y, o.rotation,
                                                               o.phase(), o.height()));
                }
            }
            std::vector<std::uint32_t> before;
            for (std::uint64_t i = 0; i < tiles.tile_count(); ++i)
                before.push_back(tiles.object(i));
            CHECK(sim::fill_fish_resources(tiles, heights.value(),
                                           {tiles.x0(), tiles.y0(), tiles.width(), tiles.height()},
                                           false) == 0);
            generated += sim::fill_fish_resources(
                tiles, heights.value(), {tiles.x0(), tiles.y0(), tiles.width(), tiles.height()},
                true);
            for (std::uint32_t y = 0; y < raw.height; ++y)
                for (std::uint32_t x = 0; x < raw.width; ++x) {
                    const auto i = std::uint64_t{y} * raw.width + x;
                    if (before[i] == 0 && tiles.object(i) == 11) {
                        const auto c =
                            sim::fisher_cell_at(heights.value(), raw.width, raw.height, x, y);
                        REQUIRE(c);
                        usable += c->usable_mask != 0;
                    }
                }
        }
        REQUIRE(counts.contains(name));
        CHECK(generated == counts.at(name).first);
        CHECK(usable == counts.at(name).second);
        if (file.filename() == "S01_02_01.sze")
            CHECK(generated == 179016);
        std::cout << "FISHER_FILL_CORPUS " << name << " generated=" << generated
                  << " usable=" << usable << "\n";
        total += generated;
    }
    CHECK(total == 4908270);
    std::cout << "FISHER_FILL_CORPUS maps=" << files.size() << " generated=" << total << "\n";
}
