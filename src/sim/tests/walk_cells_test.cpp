#include <array>
#include <filesystem>
#include <memory>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>
namespace sim = anno::sim;
namespace {
sim::BuildingType house() {
    sim::BuildingType t;
    t.id = sim::TypeId{101};
    t.width = 1;
    t.height = 1;
    t.walk_mask = {7, 0, 0}; // top subcell row blocked
    return t;
}
auto registry(sim::BuildingType t) {
    sim::TypeRegistry::Builder b;
    b.add(t);
    auto r = b.build();
    REQUIRE(r);
    return r.value();
}
sim::Island island() {
    sim::Island i;
    REQUIRE(i.tiles.reset(10, 20, 3, 3, 4));
    return i;
}
} // namespace
TEST_CASE("walk_cells: encoder and all four rotated subcell rows") {
    auto t = house();
    const std::array<std::uint16_t, 4> masks{7, 73, 448, 292};
    for (std::uint32_t r = 0; r < 4; ++r) {
        CHECK(sim::walk_code(t, r) == 4);
        CHECK(sim::walk_mask(t, 0, 0, r) == masks[r]);
    }
    t.head_raw.resize(56);
    t.head_raw[0x30] = 3;
    CHECK(sim::walk_code(t, 0) == 0); // road HEAD pair0,3, regardless of flags
    t.head_raw[0x2f] = 10;
    t.head_raw[0x30] = 0;
    CHECK(sim::walk_code(t, 0) == 0xd2);
    CHECK(sim::walk_code(t, 1) == 0xaa);
    CHECK(sim::walk_code(t, 2) == 0xd2);
    t.width = 2;
    t.walk_mask = {1, 0, 0};
    CHECK(sim::walk_mask(t, 0, 0, 0) == 1);
    CHECK(sim::walk_mask(t, 0, 1, 1) == 64); // unrotated(0,0) moves to output(0,5)
}
TEST_CASE("walk_cells: house instances retain rotation and water is explicitly stopped") {
    auto t = house();
    sim::BuildingType road;
    road.id = sim::TypeId{102};
    road.head_raw.resize(56);
    road.head_raw[0x30] = 3;
    sim::TypeRegistry::Builder builder;
    builder.add(t).add(road);
    auto built = builder.build();
    REQUIRE(built);
    auto types = built.value();
    auto i = island();
    i.tiles.set_object(0, 102);
    i.tiles.set_object(4, 101);
    i.cities.resize(1);
    i.cities[0] = sim::make_city(sim::CityNo{0}, sim::PlayerId{0}, sim::SimConfig{});
    auto& slots = i.cities[0]->index.lists[0].slots;
    slots.resize(1);
    slots[0] = sim::Building{};
    auto& b = *slots[0];
    b.type = t.id;
    b.x = 11;
    b.y = 21;
    const std::array<std::uint16_t, 4> masks{7, 73, 448, 292};
    auto tables = *sim::synthetic_walk_tables();
    for (std::size_t r = 0; r < 4; ++r) {
        b.rotation = static_cast<std::uint8_t>(r);
        tables.mapper[masks[r]] = static_cast<std::uint16_t>(100 + r);
        const auto record = sim::walk_record(i, *types, {11, 21});
        CHECK(record.supported);
        CHECK(record.code == 4);
        CHECK(record.mask9 == masks[r]);
        sim::WalkCells cells(i, *types, tables, {10, 20, 3, 3});
        CHECK(cells.get({10, 20}) == sim::WalkCell{0xfff, 0});
        CHECK(cells.get({12, 22}) == sim::WalkCell{0xfff, 2});
        CHECK(cells.get({11, 21}) == sim::WalkCell{static_cast<std::uint16_t>(100 + r), 4});
    }
    i.terrain = sim::make_terrain(std::vector<std::uint8_t>(9, 0), {}, {});
    const auto water = sim::walk_record(i, *types, {12, 22});
    CHECK(water.code == 2);
    CHECK(water.mask9 == 0); // OQ-200 fallback, no guessed threshold
    i.cities.clear();
    CHECK_FALSE(sim::walk_record(i, *types, {11, 21}).supported);
    sim::WalkCells unsupported(i, *types, tables, {10, 20, 3, 3});
    CHECK(unsupported.get({11, 21}).low == 0);
    CHECK_FALSE(unsupported.complete());
}
TEST_CASE("walk_cells: lazy clipping, chunk partitions, disc and scratch isolation") {
    auto i = island();
    auto types = registry(house());
    auto tables = sim::synthetic_walk_tables();
    sim::SearchArea area;
    area.cx0 = area.cx1 = 11;
    area.cy0 = area.cy1 = 21;
    area.radius = 0;
    area.half = {0};
    sim::WalkCells a(i, *types, *tables, {9, 19, 5, 5}, 2, area),
        b(i, *types, *tables, {10, 20, 3, 3}, 4, area);
    CHECK(a.rect() == sim::TileRect{10, 20, 3, 3});
    CHECK(a.allocated_chunks() == 0);
    CHECK(a.get({9, 20}) == sim::WalkCell{});
    CHECK(a.allocated_chunks() == 0);
    for (std::int32_t y = 20; y < 23; ++y) {
        for (std::int32_t x = 10; x < 13; ++x) {
            CHECK(a.get({x, y}) == b.get({x, y}));
            CHECK(a.get({x, y}).high == ((x == 11 && y == 21) ? 2 : 7));
        }
    }
    CHECK(a.allocated_chunks() == 4);
    CHECK(b.allocated_chunks() == 1);
    REQUIRE(a.set({11, 21}, {0x1234, 0x486}));
    CHECK(b.get({11, 21}) == sim::WalkCell{0xfff, 2});
    CHECK_FALSE(a.set({9, 20}, {}));
    sim::WalkCells bad(i, *types, *tables, {10, 20, 3, 3}, 0);
    CHECK(bad.get({10, 20}) == sim::WalkCell{});
}
TEST_CASE("walk_cells: tables validate without changing registry digest or stream") {
    sim::TypeRegistry::Builder a;
    a.figure_type({1, 1, 1, 0, 1});
    auto ta = a.build();
    REQUIRE(ta);
    CHECK(sim::SimConfig{}.validate(*ta.value()).empty());
    auto tables = std::make_shared<sim::WalkTables>(*sim::synthetic_walk_tables());
    tables->layers[0][0] = 0x42;
    tables->layers[0][0x400 + 8 * 4 + 6] = 37;
    a.walk_tables(tables);
    auto tb = a.build();
    REQUIRE(tb);
    CHECK(ta.value()->digest() == tb.value()->digest());
    auto wa = sim::World::create(sim::ScenarioSpec{}, ta.value());
    auto wb = sim::World::create(sim::ScenarioSpec{}, tb.value());
    REQUIRE(wa);
    REQUIRE(wb);
    CHECK(wa.value().state_hash() == wb.value().state_hash());
    CHECK(wa.value().save() == wb.value().save());
    a.walk_tables({});
    auto missing = a.build();
    REQUIRE(missing);
    CHECK_FALSE(sim::SimConfig{}.validate(*missing.value()).empty());
    CHECK_FALSE(sim::World::create(sim::ScenarioSpec{}, missing.value()));
    CHECK(sim::admission(*tables, 0, 0) == 0x42);
    CHECK(sim::admission(*tables, 3, 2) == 0);
    CHECK(sim::admission(*tables, 0, 2) == 255);
    CHECK(sim::admission(*tables, 0, 0x400) == 0x42);
    CHECK(sim::cost(*tables, 0, 4, 6) == 37);
    CHECK(sim::cost(*tables, 0, 4, 4) == 1);
    CHECK(sim::cost(*tables, 0, 0x404, 6) == 37);
    CHECK(sim::cost(*tables, 99, 0, 0) == 0);
    CHECK(sim::cost(*tables, 0, 1023, 7) == 0);
    CHECK(sim::cost(*tables, 0, 4, 8) == 0);
}
TEST_CASE("walk_cells: original S01_07_01 road and house cells" * doctest::test_suite("corpus")) {
    const auto game = anno::test::game_dir();
    if (!game) {
        MESSAGE("ANNO1503_GAME_DIR not set: skipping original walk cells");
        return;
    }
    const auto ab = anno::test::read_file(*game / "Anlagen.dat");
    auto an = anno::formats::anlagen::AnlagenFile::parse(ab);
    REQUIRE(an);
    auto types =
        sim::TypeRegistry::build(an.value().entries, sim::original_goods(), sim::SimConfig{});
    REQUIRE(types);
    const auto wb = anno::test::read_file(*game / "AnnoWelt.dll");
    auto tables = anno::formats::annowelt::read_walk_tables(wb);
    REQUIRE(tables);
    const auto sb = anno::test::read_file(*game / "Scenes" / "0100Kampagne01" / "S01_07_01.sze");
    auto loaded = sim::load_scenario(sb, *types.value(), sim::SimConfig{});
    REQUIRE(loaded);
    auto world = sim::World::create(loaded.value().spec, types.value());
    REQUIRE(world);
    std::uint32_t roads = 0, houses = 0;
    for (const auto& slot : world.value().islands()) {
        if (!slot) {
            continue;
        }
        const auto& i = *slot;
        sim::WalkCells cells(i, *types.value(), tables.value(), i.rect());
        for (std::uint32_t y = 0; y < i.tiles.height(); ++y) {
            for (std::uint32_t x = 0; x < i.tiles.width(); ++x) {
                const auto index = std::uint64_t{y} * i.tiles.width() + x;
                const sim::TileXY tile{i.tiles.x0() + static_cast<std::int32_t>(x),
                                       i.tiles.y0() + static_cast<std::int32_t>(y)};
                if (i.tiles.object(index) == 2527) {
                    CHECK(cells.get(tile) == sim::WalkCell{0xfff, 0});
                    ++roads;
                }
            }
        }
        for (const auto& city : i.cities) {
            if (!city) {
                continue;
            }
            for (const auto& list : city->index.lists) {
                for (const auto& b : list.slots) {
                    if (!b) {
                        continue;
                    }
                    const auto* t = types.value()->find(b->type);
                    REQUIRE(t);
                    if (t->prodtype < 97 || t->prodtype > 101) {
                        continue;
                    }
                    // House origin sample independently derives the 3x3 source bits.
                    std::uint16_t mask = 0;
                    for (std::uint32_t v = 0; v < 3; ++v) {
                        for (std::uint32_t u = 0; u < 3; ++u) {
                            std::uint32_t sx = u, sy = v;
                            const auto w = 3 * t->width, h = 3 * t->height;
                            if (b->rotation == 1) {
                                sx = w - 1 - v;
                                sy = u;
                            }
                            if (b->rotation == 2) {
                                sx = w - 1 - u;
                                sy = h - 1 - v;
                            }
                            if (b->rotation == 3) {
                                sx = v;
                                sy = h - 1 - u;
                            }
                            if (sy < t->walk_mask.size() && sx < 32 &&
                                ((t->walk_mask[sy] >> sx) & 1U)) {
                                mask = static_cast<std::uint16_t>(mask | (1U << (3 * v + u)));
                            }
                        }
                    }
                    const auto c = cells.get({b->x, b->y});
                    CHECK(c.high == 4);
                    CHECK(c.low == tables.value().mapper[mask]);
                    ++houses;
                }
            }
        }
    }
    CHECK(roads > 0);
    CHECK(houses > 0);
    for (auto layer : {0U, 3U}) {
        CHECK(sim::admission(tables.value(), layer, 0) != 0);
        CHECK(sim::admission(tables.value(), layer, 4) != 0);
    }
    CHECK(sim::admission(tables.value(), 0, 2) != 0);
    CHECK(sim::admission(tables.value(), 3, 2) == 0);
    MESSAGE("S01_07_01 checked dirt-road tiles=" << roads << ", house origins=" << houses);
}
