#include <array>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

#include "world_access.hpp"
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
    // Road at world (10, 20) = index 0; house at world (11, 21) = index 4.
    write_walk_record(i.tiles, road, 10, 20, 0);
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
        // Write the per-tile walk record for the house at this rotation (F5 step 3(b)).
        write_walk_record(i.tiles, t, 11, 21, b.rotation);
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
    // Rotation 3 is still stored: code 4, mask 292, and mapper[292] was set to 103.
    // An untouched tile is mapper[0] = 0xfff with code 2 ({12, 22} above).
    const auto kept = sim::walk_record(i, *types, {11, 21});
    CHECK(kept.supported);
    CHECK(kept.code == 4);
    CHECK(kept.mask9 == 292);
    sim::WalkCells cells(i, *types, tables, {10, 20, 3, 3});
    CHECK(cells.get({11, 21}) == sim::WalkCell{103, 4});
    CHECK(cells.complete());
}
namespace {
auto registry_of(std::initializer_list<sim::BuildingType> types) {
    sim::TypeRegistry::Builder b;
    for (const sim::BuildingType& t : types) {
        b.add(t);
    }
    auto r = b.build();
    REQUIRE(r);
    return r.value();
}
} // namespace
TEST_CASE("place_object_record writes one anchor per block") {
    sim::BuildingType block;
    block.id = sim::TypeId{7001};
    block.width = 3;
    block.height = 3;
    block.walk_mask = {1};
    const auto types = registry_of({block});
    sim::TileLayer layer;
    REQUIRE(layer.reset(0, 0, 16, 16, 8));
    CHECK(sim::place_object_record(layer, *types, block, 6, 6, 0, 9) == 0);
    std::uint32_t anchor_n = 0;
    for (std::int32_t y = 6; y <= 8; ++y) {
        for (std::int32_t x = 6; x <= 8; ++x) {
            const auto i = layer.index_of(x, y);
            INFO("tile " << x << "," << y);
            CHECK(layer.object(i) == 7001);
            CHECK(layer.foot_dx(i) == static_cast<std::uint16_t>(x - 6));
            CHECK(layer.foot_dy(i) == static_cast<std::uint16_t>(y - 6));
            CHECK(layer.foot_rotation(i) == 0);
            CHECK(layer.phase(i) == 9);
            CHECK(layer.walk_code(i) == 4);
            const bool anchor = (x == 6 && y == 6) || (x == 8 && y == 6) || (x == 6 && y == 8) ||
                                (x == 8 && y == 8);
            CHECK(layer.foot_anchor(i) == (anchor ? 1 : 0));
            anchor_n += layer.foot_anchor(i);
        }
    }
    CHECK(anchor_n == 4);
    CHECK(layer.object(layer.index_of(5, 6)) == 0);
}
TEST_CASE("place_object_record overlay keeps another overlay's anchor tile") {
    sim::BuildingType rock;
    rock.id = sim::TypeId{7101};
    rock.width = 3;
    rock.height = 3;
    rock.flags_word = 0x10;
    rock.walk_mask = {1};
    sim::BuildingType later;
    later.id = sim::TypeId{7102};
    later.width = 3;
    later.height = 3;
    later.flags_word = 0x10;
    later.walk_mask = {2};
    later.head_raw.assign(56, 0);
    later.head_raw[0x2f] = 1;
    const auto types = registry_of({rock, later});
    sim::TileLayer layer;
    REQUIRE(layer.reset(0, 0, 16, 16, 8));
    CHECK(sim::place_object_record(layer, *types, rock, 6, 6, 0, 4) == 0);
    CHECK(sim::place_object_record(layer, *types, later, 6, 6, 0, 7) == 4);
    const auto anchor = layer.index_of(6, 6);
    CHECK(layer.object(anchor) == 7101);
    CHECK(layer.foot_dx(anchor) == 0);
    CHECK(layer.foot_dy(anchor) == 0);
    CHECK(layer.foot_rotation(anchor) == 0);
    CHECK(layer.phase(anchor) == 4);
    CHECK(layer.foot_anchor(anchor) == 1);
    CHECK(layer.walk_code(anchor) == 1);
    // (6,6) was skipped, so (7,6) is the first written tile of block (0,0)
    // and takes the anchor (0x10082289 clears it only after a write).
    const auto written = layer.index_of(7, 6);
    CHECK(layer.object(written) == 7102);
    CHECK(layer.foot_dx(written) == 1);
    CHECK(layer.foot_dy(written) == 0);
    CHECK(layer.phase(written) == 7);
    CHECK(layer.foot_anchor(written) == 1);
    CHECK(layer.walk_code(written) == 1);
    const auto second = layer.index_of(7, 7);
    CHECK(layer.object(second) == 7102);
    CHECK(layer.foot_anchor(second) == 0);
}
TEST_CASE("place_object_record overlay takes a non-overlay object's tiles") {
    sim::BuildingType house;
    house.id = sim::TypeId{7201};
    house.width = 2;
    house.height = 3;
    house.walk_mask = {1};
    sim::BuildingType overlay;
    overlay.id = sim::TypeId{7202};
    overlay.width = 2;
    overlay.height = 3;
    overlay.flags_word = 0x10;
    overlay.walk_mask = {2};
    overlay.head_raw.assign(56, 0);
    overlay.head_raw[0x2f] = 1;
    const auto types = registry_of({house, overlay});
    sim::TileLayer layer;
    REQUIRE(layer.reset(0, 0, 8, 8, 8));
    CHECK(sim::place_object_record(layer, *types, house, 1, 1, 0, 9) == 0);
    CHECK(layer.foot_anchor(layer.index_of(1, 1)) == 1);
    CHECK(sim::place_object_record(layer, *types, overlay, 1, 1, 0, 3) == 0);
    for (std::uint32_t dy = 0; dy < 3; ++dy) {
        for (std::uint32_t dx = 0; dx < 2; ++dx) {
            const auto i = layer.index_of(1 + static_cast<std::int32_t>(dx),
                                          1 + static_cast<std::int32_t>(dy));
            INFO("tile " << dx << "," << dy);
            CHECK(layer.object(i) == 7202);
            CHECK(layer.phase(i) == 3);
            CHECK(layer.walk_code(i) == 1);
        }
    }
    CHECK(layer.foot_anchor(layer.index_of(1, 1)) == 1);
    CHECK(layer.object(layer.index_of(0, 1)) == 0);
}

TEST_CASE("walk_cells: footprint record rotates, overlays and resets") {
    // 2x2, only the unrotated source subcell (sx=0, sy=0) blocked. w = h = 6.
    // Rotation 0 samples that bit at output (dx=0, dy=0, u=0, v=0): mask 1.
    // Rotation 1: sx = 5 - y, sy = x, so y = 5 and x = 0, which is
    // (dx=0, dy=1, u=0, v=2), output bit 6, mask 64, on tile (0, 1).
    // Rotation 2: sx = 5 - x, sy = 5 - y, so x = y = 5, tile (1, 1), bit 8, mask 256.
    // Rotation 3: sx = y, sy = 5 - x, so y = 0 and x = 5, tile (1, 0), bit 2, mask 4.
    // Empty HEAD: code 4 on every tile of the footprint. The other tiles stay 0.
    sim::BuildingType scenery;
    scenery.id = sim::TypeId{5001};
    scenery.width = 2;
    scenery.height = 2;
    scenery.walk_mask = {1};
    const std::uint16_t expect[4][4] = {
        {1, 0, 0, 0},
        {0, 0, 64, 0},
        {0, 0, 0, 256},
        {0, 4, 0, 0},
    };
    sim::TileLayer layer;
    for (std::uint32_t r = 0; r < 4; ++r) {
        REQUIRE(layer.reset(0, 0, 4, 4, 4));
        sim::write_walk_record(layer, scenery, 0, 0, r);
        for (std::uint32_t dy = 0; dy < 2; ++dy) {
            for (std::uint32_t dx = 0; dx < 2; ++dx) {
                const auto i =
                    layer.index_of(static_cast<std::int32_t>(dx), static_cast<std::int32_t>(dy));
                INFO("rotation " << r << " tile " << dx << "," << dy);
                CHECK(layer.walk_code(i) == 4);
                CHECK(layer.walk_mask(i) == expect[r][dy * 2 + dx]);
            }
        }
        CHECK(layer.walk_code(layer.index_of(2, 0)) == 2);
        CHECK(layer.walk_mask(layer.index_of(2, 0)) == 0);
    }

    // Flag 0x10 ORs. The walk code is 1, which does not have bit 0x10, so the
    // OR is the HEAD flags word (AnnoWelt 0x1008218d) and not the code.
    // Sampled mask of {2} at rotation 0, tile (0, 0), is bit u=1: mask 2.
    // 1 | 2 = 3. The other three tiles of the 2x2 had mask 0 and become code 1.
    sim::BuildingType overlay;
    overlay.id = sim::TypeId{5003};
    overlay.width = 2;
    overlay.height = 2;
    overlay.flags_word = 0x10;
    overlay.walk_mask = {2};
    overlay.head_raw.assign(56, 0);
    overlay.head_raw[0x2f] = 1;
    REQUIRE(layer.reset(0, 0, 4, 4, 4));
    sim::write_walk_record(layer, scenery, 0, 0, 0);
    sim::write_walk_record(layer, overlay, 0, 0, 0);
    CHECK(layer.walk_code(0) == 1);
    CHECK(layer.walk_mask(0) == 3);
    CHECK(layer.walk_code(1) == 1);
    CHECK(layer.walk_mask(1) == 0);
    CHECK(layer.walk_code(4) == 1);
    CHECK(layer.walk_mask(4) == 0);

    // A code that itself has bit 0x10 (first 8, shape 4 -> 0x90) with flags_word
    // 0 replaces. The old test of the walk code would have ORed this to 3.
    sim::BuildingType coded;
    coded.id = sim::TypeId{5004};
    coded.width = 1;
    coded.height = 1;
    coded.walk_mask = {2};
    coded.head_raw.assign(56, 0);
    coded.head_raw[0x2f] = 8;
    coded.head_raw[0x30] = 4;
    sim::write_walk_record(layer, coded, 1, 0, 0);
    CHECK(layer.walk_code(1) == 0x90);
    CHECK(layer.walk_mask(1) == 2);

    // A later object with the flag clear replaces its own footprint only.
    sim::BuildingType later;
    later.id = sim::TypeId{5005};
    later.width = 1;
    later.height = 1;
    later.walk_mask = {4};
    later.head_raw.assign(56, 0);
    later.head_raw[0x2f] = 2;
    sim::write_walk_record(layer, later, 0, 0, 0);
    CHECK(layer.walk_code(0) == 2);
    CHECK(layer.walk_mask(0) == 4);
    CHECK(layer.walk_code(4) == 1);
    CHECK(layer.walk_mask(4) == 0);

    // A building placed after the scenery overwrites the tiles it covers, and
    // remove_building resets that footprint to code 2, mask 0.
    sim::BuildingType house;
    house.id = sim::TypeId{5002};
    house.prodtype = 97;
    house.cls = sim::class_for_prodtype(97).value();
    house.width = 1;
    house.height = 1;
    house.walk_mask = {4};
    house.head_raw.assign(56, 0);
    house.head_raw[0x2f] = 3;
    sim::TypeRegistry::Builder builder;
    builder.add(scenery).add(house);
    auto built = builder.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    sim::ScenarioSpec spec = sim::synthetic::empty_scenario(1);
    sim::ScenarioSpec::PlayerSpec player;
    player.id = sim::PlayerId{0};
    spec.players.push_back(player);
    sim::ScenarioSpec::IslandSpec isle = sim::synthetic::plain_island(0, 0, 0, 4, 4);
    isle.owner_runs = {{0, 16}};
    isle.objects.push_back(sim::ScenarioSpec::TileObjectSpec{0, 5001, 0, 0});
    sim::ScenarioSpec::CitySpec city = sim::synthetic::plain_city(0, 0);
    sim::ScenarioSpec::BuildingSpec placed;
    placed.type = sim::TypeId{5002};
    placed.x = 0;
    placed.y = 0;
    city.buildings.push_back(placed);
    isle.cities.push_back(std::move(city));
    spec.islands.push_back(std::move(isle));
    auto made = sim::World::create(spec, built.value());
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
    sim::World world = std::move(made).value();
    const sim::Island& loaded_island = *world.islands()[0];
    const auto at = [&](std::int32_t x, std::int32_t y) {
        return loaded_island.tiles.index_of(x, y);
    };
    CHECK(loaded_island.tiles.walk_code(at(0, 0)) == 3);
    CHECK(loaded_island.tiles.walk_mask(at(0, 0)) == 4);
    CHECK(loaded_island.tiles.object(at(0, 0)) == 5002);
    CHECK(loaded_island.tiles.walk_code(at(1, 0)) == 4);
    CHECK(loaded_island.tiles.walk_mask(at(1, 0)) == 0);
    CHECK(loaded_island.tiles.walk_code(at(0, 1)) == 4);
    CHECK(loaded_island.tiles.walk_code(at(1, 1)) == 4);
    CHECK(loaded_island.tiles.walk_code(at(2, 0)) == 2);
    REQUIRE(loaded_island.cache.by_id.size() == 1);
    const sim::BuildingKey key = loaded_island.cache.by_id.begin()->second;
    REQUIRE(sim::WorldTestAccess::remove(world, key));
    CHECK(loaded_island.tiles.walk_code(at(0, 0)) == 2);
    CHECK(loaded_island.tiles.walk_mask(at(0, 0)) == 0);
    CHECK(loaded_island.tiles.object(at(0, 0)) == 0);
    CHECK(loaded_island.tiles.walk_code(at(1, 0)) == 4);
    CHECK(loaded_island.tiles.walk_mask(at(1, 0)) == 0);
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
TEST_CASE("walk_cells: table digest pins state save and load without changing registry digest") {
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
    CHECK(sim::walk_tables_digest(*ta.value()->walk_tables()) !=
          sim::walk_tables_digest(*tb.value()->walk_tables()));
    CHECK(wa.value().state_hash() != wb.value().state_hash());
    CHECK(wa.value().save() != wb.value().save());
    auto mismatch = sim::World::load(wa.value().save(), tb.value());
    CHECK_FALSE(mismatch);
    CHECK(mismatch.error().find("walk-table digest differs") != std::string::npos);
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
                } else {
                    static_cast<void>(cells.get(tile));
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
        CHECK(cells.complete());
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
