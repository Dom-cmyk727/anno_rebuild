// check_invariants and the hostile loads of cities and buildings: one test per
// structural fact of docs/sim/state.md §9.6, each also fed through load (stage S6).
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/world.hpp>

#include "alloc_probe.hpp"
#include "building_helpers.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using namespace building_test;

namespace {

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

sim::Building& bld(sim::World& w, std::uint32_t city, sim::ListId list, std::size_t slot) {
    return *city_of(w, 0, city).index.lists[list].slots[slot];
}

template <class P> P& pay(sim::World& w, sim::ListId list, std::size_t slot) {
    return std::get<P>(bld(w, 0, list, slot).payload);
}

struct Row {
    const char* name;
    void (*mutate)(sim::World&);
    const char* expect;
    /// What load says when it is not the same text (the stream reader bounds some
    /// counts before check_invariants could); nullptr = `expect`.
    const char* load_expect = nullptr;
};

/// Each row corrupts exactly one fact of a good world: check_invariants names it
/// (with the path), and a save carrying the corruption is refused by load with a
/// Result error naming the same fact (no exception, no crash).
void run_rows(const Kit& k, const sim::World& good, const std::vector<Row>& rows) {
    REQUIRE(good.check_invariants() == "");
    REQUIRE(load_error(k, good.save()) == "");
    for (const Row& row : rows) {
        INFO("row " << row.name);
        sim::World w = good;
        row.mutate(w);
        const std::string err = w.check_invariants();
        CHECK(contains(err, row.expect));
        if (!contains(err, row.expect)) {
            MESSAGE("check_invariants said: " << err);
        }
        const std::string load = load_error(k, w.save());
        const char* load_want = row.load_expect != nullptr ? row.load_expect : row.expect;
        CHECK(contains(load, load_want));
        if (!contains(load, load_want)) {
            MESSAGE("load said: " << load);
        }
    }
}

} // namespace

TEST_CASE("invariant payload alternative vs type") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"a workshop type with a farm payload",
             [](sim::World& w) { bld(w, 0, 4, 0).payload = sim::Farm{}; },
             "islands[0].cities[0].index.lists[4].slots[0].payload: holds Farm, the class needs "
             "Workshop"},
            {"a farm type with a workshop payload",
             [](sim::World& w) { bld(w, 0, 4, 1).payload = sim::Workshop{}; },
             "lists[4].slots[1].payload: holds Workshop, the class needs Farm"},
            {"a fur trapper with a mine payload",
             [](sim::World& w) { bld(w, 0, 4, 2).payload = sim::Mine{}; },
             "lists[4].slots[2].payload: holds Mine, the class needs Farm"},
            {"a small farm with a farm payload",
             [](sim::World& w) { bld(w, 0, 4, 3).payload = sim::Farm{}; },
             "lists[4].slots[3].payload: holds Farm, the class needs SmallFarm"},
            {"a weapons workshop with a workshop payload",
             [](sim::World& w) { bld(w, 0, 4, 4).payload = sim::Workshop{}; },
             "lists[4].slots[4].payload: holds Workshop, the class needs WeaponsWorkshop"},
            {"a mine with an inert payload",
             [](sim::World& w) { bld(w, 0, 4, 5).payload = sim::Inert{}; },
             "lists[4].slots[5].payload: holds Inert, the class needs Mine"},
            {"a house with a farm payload",
             [](sim::World& w) { bld(w, 0, 3, 0).payload = sim::Farm{}; },
             "lists[3].slots[0].payload: holds Farm, the class needs Inert"},
            {"a market building with a workshop payload",
             [](sim::World& w) { bld(w, 0, 5, 0).payload = sim::Workshop{}; },
             "lists[5].slots[0].payload: holds Workshop, the class needs Inert"},
        });
}

TEST_CASE("invariant payload vector sizes vs type counts") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"workshop out too short",
             [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).out.clear(); },
             "slots[0].payload: out has 0 entries, the type needs 1"},
            {"workshop out too long",
             [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).out.push_back(0); },
             "slots[0].payload: out has 2 entries, the type needs 1"},
            {"workshop in too short", [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).in.clear(); },
             "slots[0].payload: in has 0 entries, the type needs 1"},
            {"workshop activity",
             [](sim::World& w) { pay<sim::Workshop>(w, 4, 0).activity.push_back(1); },
             "slots[0].payload: activity has 2 entries, the type needs 1"},
            {"weapons workshop line_on",
             [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).line_on.assign(3, true); },
             "slots[4].payload: line_on has 3 entries, the type needs 2"},
            {"weapons workshop out",
             [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).out.pop_back(); },
             "slots[4].payload: out has 1 entries, the type needs 2"},
            {"weapons workshop in",
             [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).in.pop_back(); },
             "slots[4].payload: in has 1 entries, the type needs 2"},
            {"weapons workshop activity",
             [](sim::World& w) { pay<sim::WeaponsWorkshop>(w, 4, 4).activity.clear(); },
             "slots[4].payload: activity has 0 entries, the type needs 2"},
            {"small farm line_on",
             [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).line_on.assign(1, true); },
             "slots[3].payload: line_on has 1 entries, the type needs 2"},
            {"small farm out", [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).out.push_back(0); },
             "slots[3].payload: out has 3 entries, the type needs 2"},
            {"small farm raw", [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).raw.clear(); },
             "slots[3].payload: raw has 0 entries, the type needs 2"},
            {"small farm activity",
             [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).activity.pop_back(); },
             "slots[3].payload: activity has 1 entries, the type needs 2"},
            {"small farm figure counters",
             [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).figures_out.pop_back(); },
             "slots[3].payload: figures_out has 4 entries, the type needs 5"},
            {"small farm without counters",
             [](sim::World& w) { pay<sim::SmallFarm>(w, 4, 3).figures_out.clear(); },
             "slots[3].payload: figures_out has 0 entries, the type needs 5"},
        });
}

TEST_CASE("invariant mine without outputs") {
    // A registry never builds a Mine type without an output (F25), so the fact is
    // tested on the function that carries it, with a hand-made type, and on the
    // deposit index of a mine, which a save can corrupt.
    const Kit k = make_kit();
    sim::BuildingType t = *k.registry->find(k.iron_mine);
    t.prod.outputs.clear();
    const sim::ClassInfo& cls = k.registry->class_info(sim::RuntimeClass::Mine);
    const sim::Payload mine = sim::Mine{};
    CHECK(sim::payload_problem(mine, t, cls, sim::SimConfig{}) ==
          "a Mine type has no output goods");
    // With an output the same payload is fine, and any other class does not need one.
    CHECK(sim::payload_problem(mine, *k.registry->find(k.iron_mine), cls, sim::SimConfig{}) == "");
    sim::BuildingType house = *k.registry->find(k.pioneer_house);
    CHECK(sim::payload_problem(sim::Inert{}, house, k.registry->class_info(house.cls),
                               sim::SimConfig{}) == "");
    const sim::World good = all_classes_world(k);
    run_rows(k, good,
             {
                 {"mine deposit beyond the island's deposits",
                  [](sim::World& w) { pay<sim::Mine>(w, 4, 5).deposit = 1; },
                  "slots[5].payload.deposit: index 1 is outside the 1 deposits"},
                 {"mine deposit far beyond",
                  [](sim::World& w) { pay<sim::Mine>(w, 4, 5).deposit = 4000000000ULL; },
                  "slots[5].payload.deposit: index 4000000000 is outside the 1 deposits"},
             });
    // The boundary: the last deposit is a valid index.
    sim::World w = good;
    pay<sim::Mine>(w, 4, 5).deposit = 0;
    CHECK(w.check_invariants() == "");
    pay<sim::Mine>(w, 4, 5).deposit.reset();
    CHECK(w.check_invariants() == "");
}

TEST_CASE("invariant config-sized container sizes") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"the player table loses a slot",
             [](sim::World& w) { sim::WorldTestAccess::players(w).slots.pop_back(); },
             "players.slots: has 31"},
            {"a player index",
             [](sim::World& w) {
                 sim::WorldTestAccess::players(w).slots[0]->index = sim::PlayerId{4};
             },
             "players"},
            {"more islands than slots",
             [](sim::World& w) { sim::WorldTestAccess::islands(w).resize(257); },
             "islands: 257 slots, SimConfig.island_slots is 256",
             "slot count 257 is above SimConfig.island_slots 256"},
            {"the city table loses a slot", [](sim::World& w) { isl(w, 0).cities.pop_back(); },
             "islands[0].cities: 15 slots, SimConfig.city_slots is 16"},
            {"the city table gains a slot", [](sim::World& w) { isl(w, 0).cities.emplace_back(); },
             "islands[0].cities: 17 slots, SimConfig.city_slots is 16"},
            {"a city in the wrong slot",
             [](sim::World& w) { city_of(w, 0, 0).number = sim::CityNo{3}; },
             "islands[0].cities[0].number: is 3, the slot is 0"},
            {"stock goods", [](sim::World& w) { city_of(w, 0, 0).stock.stock.pop_back(); },
             "islands[0].cities[0].stock: 55 goods, SimConfig.warehouse_goods is 56"},
            {"goods settings",
             [](sim::World& w) { city_of(w, 0, 1).goods.push_back(sim::GoodsSetting{}); },
             "islands[0].cities[1].goods: 57 settings, SimConfig.warehouse_goods is 56"},
            {"build goods", [](sim::World& w) { city_of(w, 0, 0).build_goods.pop_back(); },
             "islands[0].cities[0].build_goods: 4 records, SimConfig.build_goods_count is 5"},
            {"trade lists", [](sim::World& w) { city_of(w, 0, 0).trade_lists.emplace_back(); },
             "islands[0].cities[0].trade_lists: 3 lists, SimConfig.trade_lists is 2"},
            {"index lists", [](sim::World& w) { city_of(w, 0, 0).index.lists.pop_back(); },
             "islands[0].cities[0].index.lists: 5 lists, SimConfig.layout has 6"},
        });
}

TEST_CASE("invariant trade data: list indexes and goods of the trade entries") {
    // D: the byte city+12g+0x0b is the index of the goods in the trade list that bit 0
    // of +0x0a names (0x1002db53, 0x1002dbd0, the STADT2 loader), -1 for none; an entry
    // names a goods of the warehouse. A hostile save or a resync snapshot must not
    // carry an index the Kontor commands (S10) would use past the end of a list.
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"a list index below -1",
             [](sim::World& w) { city_of(w, 0, 0).goods[7].list_index = -2; },
             "islands[0].cities[0].goods[7].list_index: -2 is neither -1 (none) nor an index"},
            {"an index into an empty list",
             [](sim::World& w) { city_of(w, 0, 0).goods[7].list_index = 0; },
             "islands[0].cities[0].goods[7].list_index: 0 names no entry of trade list 0"},
            {"an index past the end of the list",
             [](sim::World& w) {
                 city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{7, 0, 0});
                 city_of(w, 0, 0).goods[7].list_index = 1;
             },
             "goods[7].list_index: 1 names no entry of trade list 0"},
            {"the buy bit selects the second list",
             [](sim::World& w) {
                 city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{7, 0, 0});
                 city_of(w, 0, 0).goods[7].list_index = 0;
                 city_of(w, 0, 0).goods[7].buy = true;
             },
             "goods[7].list_index: 0 names no entry of trade list 1"},
            {"a list index of 0x7fffffff",
             [](sim::World& w) { city_of(w, 0, 0).goods[7].list_index = 0x7fffffff; },
             "goods[7].list_index: 2147483647 names no entry of trade list 0"},
            {"the entry holds another goods",
             [](sim::World& w) {
                 city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{8, 0, 0});
                 city_of(w, 0, 0).goods[7].list_index = 0;
             },
             "goods[7].list_index: 0 names the entry of trade list 0 that holds goods 8, not 7"},
            {"an entry names a goods beyond the warehouse",
             [](sim::World& w) {
                 city_of(w, 0, 0).trade_lists[1].push_back(sim::TradeEntry{56, 0, 0});
             },
             "islands[0].cities[0].trade_lists[1][0].goods: 56 is not below "
             "SimConfig.warehouse_goods 56"},
            {"an entry names goods 0xffffffff",
             [](sim::World& w) {
                 city_of(w, 0, 0).trade_lists[0].push_back(sim::TradeEntry{0xffffffffU, 0, 0});
             },
             "trade_lists[0][0].goods: 4294967295 is not below SimConfig.warehouse_goods"},
        });
    // The consistent states are accepted: an entry on either list, at any position,
    // and -1 everywhere else.
    sim::World w = good;
    auto& c = city_of(w, 0, 0);
    c.trade_lists[0].push_back(sim::TradeEntry{3, 2 * 256, 10});
    c.trade_lists[0].push_back(sim::TradeEntry{7, 0, 0});
    c.trade_lists[1].push_back(sim::TradeEntry{55, 5, 5});
    c.goods[7].list_index = 1;
    c.goods[55].list_index = 0;
    c.goods[55].buy = true;
    CHECK(w.check_invariants() == "");
    CHECK(load_error(k, w.save()) == "");
    // The values that are open stay open (O-57): any build-goods setting, any capacity.
    c.build_goods[2].setting = 0xffffffffU;
    c.stock.capacity = 0;
    CHECK(w.check_invariants() == "");
}

TEST_CASE("invariant scheduler facts of a building list") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"a list with no element",
             [](sim::World& w) { city_of(w, 0, 0).index.lists[2].slots.clear(); },
             "islands[0].cities[0].index.lists[2].slots: the list has no element"},
            {"hint below -1", [](sim::World& w) { city_of(w, 0, 0).index.lists[4].hint = -2; },
             "lists[4].hint: -2 is outside -1 .. 10"},
            {"hint at the length", [](sim::World& w) { city_of(w, 0, 0).index.lists[4].hint = 10; },
             "lists[4].hint: 10 is outside -1 .. 10"},
            {"hint far outside",
             [](sim::World& w) { city_of(w, 0, 0).index.lists[1].hint = 1LL << 40; },
             "lists[1].hint: 1099511627776 is outside -1 .. 1"},
            {"cursor at the length",
             [](sim::World& w) { city_of(w, 0, 0).index.lists[4].cursor = 10; },
             "lists[4].cursor: 10 is not below the 10 slots"},
            {"cursor of a one-slot list",
             [](sim::World& w) { city_of(w, 0, 1).index.lists[0].cursor = 1; },
             "cities[1].index.lists[0].cursor: 1 is not below the 1 slots"},
        });
    // The edges are valid: hint -1 .. n - 1, cursor 0 .. n - 1.
    sim::World w = good;
    city_of(w, 0, 0).index.lists[4].hint = 9;
    city_of(w, 0, 0).index.lists[4].cursor = 9;
    CHECK(w.check_invariants() == "");
    city_of(w, 0, 0).index.lists[4].hint = -1;
    city_of(w, 0, 0).index.lists[4].cursor = 0;
    CHECK(w.check_invariants() == "");
}

TEST_CASE("invariant facts of a building") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"unknown type", [](sim::World& w) { bld(w, 0, 4, 0).type = sim::TypeId{424242}; },
             "lists[4].slots[0].type: 424242 is not in the registry"},
            {"rotation 4", [](sim::World& w) { bld(w, 0, 4, 0).rotation = 4; },
             "lists[4].slots[0].rotation: 4 is not below 4"},
            {"a building in the list of another type",
             [](sim::World& w) {
                 // The weaver's record copied into the house list: the first list
                 // that holds a wrong building is reported (lists 0 to 5 in order).
                 city_of(w, 0, 0).index.lists[3].slots[5] = bld(w, 0, 4, 0);
             },
             "lists[3].slots[5]: the building sits in list 3 but its type belongs to list 4"},
            {"a type without a list",
             [](sim::World& w) { bld(w, 0, 4, 0).type = sim::TypeId{9010}; },
             "lists[4].slots[0]: the building sits in list 4 but its type belongs to no list"},
            {"the id names another island",
             [](sim::World& w) { bld(w, 0, 4, 0).id = sim::make_building_id(sim::IslandId{3}, 1); },
             "lists[4].slots[0].id: names island 3, the island is 0"},
            {"serial 0",
             [](sim::World& w) { bld(w, 0, 4, 0).id = sim::make_building_id(sim::IslandId{0}, 0); },
             "lists[4].slots[0].id: serial 0 is not in 1 .. next_serial"},
            {"serial at next_serial",
             [](sim::World& w) {
                 bld(w, 0, 4, 0).id =
                     sim::make_building_id(sim::IslandId{0}, isl(w, 0).next_serial);
             },
             "lists[4].slots[0].id: serial"},
            {"next_serial not above the serials", [](sim::World& w) { isl(w, 0).next_serial = 3; },
             "is not in 1 .. next_serial (3)"},
        });
}

TEST_CASE("invariant footprints and relations") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"a city of a player that does not exist",
             [](sim::World& w) { city_of(w, 0, 0).owner = sim::PlayerId{9}; },
             "islands[0].cities[0].owner: player 9 is not in the player table"},
            {"a city of an invalid player",
             [](sim::World& w) { city_of(w, 0, 1).owner = sim::PlayerId{}; },
             "islands[0].cities[1].owner: player 4294967295 is not in the player table"},
            {"a tile owned by a city that does not exist",
             [](sim::World& w) { isl(w, 0).tiles.set_owner(5, 7); },
             "islands[0].tiles: tile 5 has owner 7, which is not a city of the island"},
            {"a tile owned by a city of another island",
             [](sim::World& w) { isl(w, 3).tiles.set_owner(0, 0); },
             "islands[3].tiles: tile 0 has owner 0, which is not a city of the island"},
            {"a footprint left of the island", [](sim::World& w) { bld(w, 0, 4, 0).x = 99; },
             "slots[0]: the footprint is not inside the island"},
            {"a footprint beyond the island", [](sim::World& w) { bld(w, 0, 3, 1).y = 163; },
             "lists[3].slots[1]: the footprint is not inside the island"},
            {"two footprints on one tile",
             [](sim::World& w) {
                 bld(w, 0, 4, 1).x = bld(w, 0, 4, 0).x;
                 bld(w, 0, 4, 1).y = bld(w, 0, 4, 0).y;
             },
             "shares tile (101, 101) with another building"},
            {"footprints of two cities on one tile",
             [](sim::World& w) {
                 bld(w, 1, 4, 0).x = 101;
                 bld(w, 1, 4, 0).y = 101;
             },
             "shares tile (101, 101) with another building"},
            {"a footprint tile without an object",
             [](sim::World& w) { isl(w, 0).tiles.set_object(isl(w, 0).tile_index(103, 101), 0); },
             "the footprint tile (103, 101) carries no object"},
        });
}

TEST_CASE("invariant climate index") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(k, good,
             {
                 {"climate 7", [](sim::World& w) { isl(w, 0).climate = 7; },
                  "islands[0].climate: 7 has no entry in SimConfig.monopoly_climate_mask"},
                 {"climate far outside", [](sim::World& w) { isl(w, 3).climate = 4000000000U; },
                  "islands[3].climate: 4000000000 has no entry"},
             });
    sim::World w = good;
    isl(w, 0).climate = 6;
    CHECK(w.check_invariants() == "");
}

TEST_CASE("invariant island rect") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    run_rows(
        k, good,
        {
            {"an island outside the world",
             [](sim::World& w) {
                 isl(w, 3).tiles.reset(490, 200, 30, 20, 8);
                 isl(w, 3).deposits.clear();
                 for (std::uint64_t i = 0; i < isl(w, 3).tiles.tile_count(); ++i) {
                     isl(w, 3).tiles.set_owner(i, 15);
                 }
             },
             "islands[3].tiles: the rectangle is not inside the world (512 x 512)"},
            {"two islands on the same tiles",
             [](sim::World& w) {
                 isl(w, 3).tiles.reset(100, 100, 30, 20, 8);
                 isl(w, 3).deposits.clear();
                 for (std::uint64_t i = 0; i < isl(w, 3).tiles.tile_count(); ++i) {
                     isl(w, 3).tiles.set_owner(i, 15);
                 }
             },
             "islands[3].tiles: the rectangle overlaps islands[0]"},
            {"an empty rectangle", [](sim::World& w) { isl(w, 3).tiles.reset(300, 200, 0, 20, 8); },
             "islands[3].tiles: the island rectangle is empty"},
        });
}

TEST_CASE("hostile city and building streams are refused before they are sized") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    // A city table claiming more slots than the table limit: refused at the count,
    // before a single slot is made.
    {
        sim::World w = good;
        isl(w, 3).cities.resize(sim::kSlotTableBound + 1);
        const std::string err = load_error(k, w.save());
        CHECK(contains(err, "slot count 65537 is above the table limit 65536"));
    }
    // A building list claiming more slots than a list can hold, written by hand: it
    // fails at the count with no allocation.
    {
        island_test::Stream s;
        s.u64(0xffffffffffffffffULL); // hint -1
        s.u32(0);
        s.u64(std::uint64_t{sim::kListSizeBound} + 1);
        // The input must back the count (else the generic check fails first): 16 MB of
        // empty slots, which the bound refuses without making a single one.
        s.v.resize(s.v.size() + sim::kListSizeBound + 1, 0);
        sim::BuildingList list;
        sim::ReadArchive r(s.v);
        fields(r, list);
        CHECK(r.failed());
        CHECK(contains(r.error(), "slot count 16777217 is above the list limit 16777216"));
        CHECK(list.slots.empty());
    }
    // A count the bytes cannot back fails the generic check.
    {
        island_test::Stream s;
        s.u64(0xffffffffffffffffULL);
        s.u32(0);
        s.u64(1000);
        s.u8(0);
        sim::BuildingList list;
        sim::ReadArchive r(s.v);
        fields(r, list);
        CHECK(r.failed());
    }
    // The memory of a list read tracks the bytes read: 5000 empty slots are 5000
    // bytes of input and the list is read.
    {
        island_test::Stream s;
        s.u64(0xffffffffffffffffULL);
        s.u32(0);
        s.u64(5000);
        for (int i = 0; i < 5000; ++i) {
            s.u8(0);
        }
        sim::BuildingList list;
        sim::ReadArchive r(s.v);
        fields(r, list);
        CHECK_FALSE(r.failed());
        CHECK(list.slots.size() == 5000);
    }
    // A flat read of a stream whose lists are the wrong count for the layout fails
    // with the path, and leaves the target unchanged.
    {
        sim::World w = good;
        city_of(w, 0, 0).index.lists.push_back(sim::BuildingList{});
        sim::World target = make_world(k, syn::empty_scenario());
        const std::uint64_t before = target.state_hash();
        const std::string err = island_test::read_flat(target, island_test::flat_stream(w));
        CHECK(contains(err, "islands[0].cities[0].index.lists: 7 lists, SimConfig.layout has 6"));
        CHECK(target.state_hash() == before);
        CHECK(target.islands().empty());
    }
    // Every prefix of a save with buildings fails, also with a valid checksum.
    {
        const std::vector<std::uint8_t> bytes = good.save();
        for (std::size_t n = 0; n < bytes.size(); n += 61) {
            std::vector<std::uint8_t> cut(bytes.begin(),
                                          bytes.begin() + static_cast<std::ptrdiff_t>(n));
            CHECK(load_error(k, cut) != "");
            if (n > 13) {
                island_test::reseal(cut);
                CHECK(load_error(k, cut) != "");
            }
        }
    }
}

TEST_CASE("a loaded world is checked: a save from another registry is refused") {
    const Kit k = make_kit();
    const sim::World good = all_classes_world(k);
    const std::vector<std::uint8_t> bytes = good.save();
    // The empty registry has no types: the header's registry digest differs, so the
    // frame is refused before any building is looked up.
    auto r = sim::World::load(bytes, sim::TypeRegistry::empty());
    REQUIRE_FALSE(r.ok());
    CHECK(contains(r.error(), "registry digest"));
    // A save whose building names a type the registry does not have fails with the
    // path, not with a crash (the registry digest is the world's own here).
    sim::World w = good;
    bld(w, 0, 4, 0).type = sim::TypeId{424242};
    CHECK(contains(load_error(k, w.save()), "type: 424242 is not in the registry"));
}

TEST_CASE("the derived index is sized by the cities the state holds, not by the configuration") {
    // F33: SimConfig::validate accepts city_slots 65536, house_groups 4096 and
    // service_bits 32, i.e. 65536 x (128 + 16 384) bytes of counters (1.08 GB) if
    // every slot got them. The counters belong to the PRESENT cities only. The probe
    // counts what operator new is asked for (the loader, the rebuild and the check
    // all recompute the index).
    const Kit k = make_kit();
    sim::SimConfig cfg;
    cfg.city_slots = sim::kSlotTableBound;
    cfg.house_groups = sim::kEntryTableBound;
    cfg.service_bits = 32;
    REQUIRE(cfg.validate() == "");
    sim::ScenarioSpec spec = kit_scenario();
    spec.islands[0].cities[0].buildings = {place(k.weaver, 101, 101), place(k.church, 105, 105),
                                           place(k.pioneer_house, 103, 105)};
    sim::World w = make_world(k, spec, cfg);
    REQUIRE(w.check_invariants() == "");
    sim::Island& island = isl(w, 0);

    // The probe sees allocations at all (else the bounds below would pass vacuously).
    {
        anno::test::AllocScope scope;
        std::unique_ptr<char[]> block(new char[1U << 20]);
        anno::test::keep(block.get());
        CHECK(scope.bytes() >= (1U << 20));
    }
    constexpr std::uint64_t kOneIslandBudget = 1U << 20;
    {
        anno::test::AllocScope scope;
        sim::rebuild_island_index(island, w.config(), *k.registry);
        INFO("rebuild allocated " << scope.bytes() << " bytes");
        CHECK(scope.bytes() < kOneIslandBudget);
    }
    {
        anno::test::AllocScope scope;
        const std::string problem = sim::island_index_problem(island, w.config(), *k.registry);
        INFO("check allocated " << scope.bytes() << " bytes");
        CHECK(problem == "");
        CHECK(scope.bytes() < kOneIslandBudget);
    }
    // The whole load: the city tables themselves are allocated (an absent slot is one
    // byte of the stream and a whole optional<City> in memory, F33), but nothing close
    // to 1.08 GB per island on top.
    const std::vector<std::uint8_t> bytes = w.save();
    {
        anno::test::AllocScope scope;
        const std::string err = load_error(k, bytes);
        INFO("load allocated " << scope.bytes() << " bytes");
        CHECK(err == "");
        CHECK(scope.bytes() < (512ULL << 20));
    }
}
