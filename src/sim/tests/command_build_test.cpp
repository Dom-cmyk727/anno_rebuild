// Stage S10: Build, Demolish and the trade commands (docs/sim/state.md §8.4, §15 S10),
// the legacy 0x64 import (§8.5) and the free-slot order after Demolish (§15 S10, the
// hand-derived table).
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/legacy_codec.hpp>
#include <anno/sim/world.hpp>

#include "command_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace ct = command_test;

namespace {

using ct::at;
using ct::build;
using ct::demolish;
using ct::kCity0;
using ct::kCity1;

/// The g2 world (one player, city 0 with 13 t tools and 30 t wood, 10 000 gold).
struct Rig {
    syn::SyntheticTypes t = ct::build_types();
    sim::World w = ct::make_world(t, syn::g2_scenario(t));
};

std::int64_t money(const sim::World& w, std::uint32_t player = 0) {
    const sim::Player* p = w.players().find(sim::PlayerId{player});
    REQUIRE(p != nullptr);
    return p->money;
}

std::uint32_t stock(const sim::World& w, sim::GoodsId g, sim::CityRef c = kCity0) {
    return ct::city(w, c).stock.stock.at(g);
}

/// Every rejected command leaves the hash and the save as they were.
void check_refused(sim::World& w, const sim::Command& c, sim::Reject expected) {
    const std::uint64_t before = w.state_hash();
    const std::vector<std::uint8_t> bytes = w.save();
    const sim::ApplyResult r = w.apply(c);
    CHECK_MESSAGE(r.status == expected, "got " << sim::reject_name(r.status) << ", expected "
                                               << sim::reject_name(expected));
    CHECK(w.state_hash() == before);
    CHECK(w.save() == bytes);
}

/// A record of type id 0: skipped by Build (D, §8.4 step 1). (A default PlacementWire
/// has the invalid type id, which is UnknownType.)
const sim::PlacementWire kZero = at(sim::TypeId{0}, 0, 0);

sim::Reject record_status(const sim::ApplyResult& r, std::size_t i) {
    REQUIRE(i < r.records.size());
    return r.records[i].status;
}

} // namespace

TEST_CASE("build charges gold and goods") {
    Rig r;
    const sim::GoodsId tools = ct::goods(r.t, "WERKZEUG");
    const sim::GoodsId wood = ct::goods(r.t, "HOLZ");
    const sim::GoodsId bricks = ct::goods(r.t, "ZIEGEL");
    REQUIRE(tools == r.w.config().build_goods_first); // 0x29, the first charged goods
    REQUIRE(wood == tools + 1);
    REQUIRE(stock(r.w, tools) == 13 * sim::kTon);
    REQUIRE(stock(r.w, wood) == 30 * sim::kTon);
    REQUIRE(money(r.w) == 10000);
    r.w.advance(100); // tick 50: the placement tick is the clock's
    const sim::ApplyResult res = r.w.apply(build(0, kCity0, {at(r.t.weaver, 112, 115)}));
    REQUIRE(res.ok());
    REQUIRE(res.records.size() == 1);
    CHECK(res.records[0].status == sim::Reject::None);
    // The weaving hut's cost (Anlagen.dat 1001: 300 gold, 3 t tools, 4 t wood).
    CHECK(money(r.w) == 9700);
    CHECK(stock(r.w, tools) == 10 * sim::kTon);
    CHECK(stock(r.w, wood) == 26 * sim::kTon);
    const sim::City& c = ct::city(r.w, kCity0);
    CHECK(c.build_goods.at(tools - r.w.config().build_goods_first).spent == 3 * sim::kTon);
    CHECK(c.build_goods.at(wood - r.w.config().build_goods_first).spent == 4 * sim::kTon);
    CHECK(c.build_goods.at(bricks - r.w.config().build_goods_first).spent == 0);
    // The building exists where it was placed, made at the placement tick (F1).
    const sim::Building* b = r.w.find_building(res.records[0].building);
    REQUIRE(b != nullptr);
    CHECK(b->type == r.t.weaver);
    CHECK(b->x == 112);
    CHECK(b->y == 115);
    CHECK(b->byte0f == 0);
    CHECK(std::get<sim::Workshop>(b->payload).last == 50);
    CHECK(r.w.building_at(kCity0, 112, 115) == b);
    CHECK(r.w.check_invariants() == "");

    // Gold exactly the price is enough; one less is not (CannotAfford, nothing spent).
    sim::WorldTestAccess::players(r.w).slots[0]->money = 300;
    CHECK(r.w.apply(build(0, kCity0, {at(r.t.weaver, 113, 115)})).ok());
    CHECK(money(r.w) == 0);
    sim::WorldTestAccess::players(r.w).slots[0]->money = 299;
    check_refused(r.w, build(0, kCity0, {at(r.t.weaver, 114, 115)}), sim::Reject::CannotAfford);
    // A goods short: the grain farm needs 3 t of bricks and the warehouse has none.
    sim::WorldTestAccess::players(r.w).slots[0]->money = 10000;
    check_refused(r.w, build(0, kCity0, {at(r.t.farm, 114, 115)}), sim::Reject::CannotAfford);
    // Exactly the goods: with 3 t of tools one more weaver, then none.
    ct::city_mut(r.w, kCity0).stock.stock[tools] = 3 * sim::kTon;
    CHECK(r.w.apply(build(0, kCity0, {at(r.t.weaver, 114, 115)})).ok());
    CHECK(stock(r.w, tools) == 0);
    check_refused(r.w, build(0, kCity0, {at(r.t.weaver, 115, 115)}), sim::Reject::CannotAfford);
    // A type that costs only goods (the pioneer house: 3 t wood) spends no gold.
    const std::int64_t gold = money(r.w);
    const std::uint32_t wood_before = stock(r.w, wood);
    CHECK(r.w.apply(build(0, kCity0, {at(r.t.pioneer_house, 120, 120)})).ok());
    CHECK(money(r.w) == gold);
    CHECK(stock(r.w, wood) == wood_before - 3 * sim::kTon);
    CHECK(r.w.check_invariants() == "");
}

TEST_CASE("build drag spends in stream order") {
    // D8 (0x10058a6d): a drag build continues after a failed record and spends record by
    // record, so it can end partially built. 13 t of tools pay for four weavers.
    Rig r;
    const sim::GoodsId tools = ct::goods(r.t, "WERKZEUG");
    const sim::ApplyResult res = r.w.apply(
        build(0, kCity0,
              {at(r.t.weaver, 112, 115), at(r.t.weaver, 113, 115), at(r.t.weaver, 114, 115),
               at(r.t.weaver, 115, 115), at(r.t.weaver, 116, 115), at(r.t.weaver, 117, 115)}));
    CHECK(res.ok()); // partly built: accepted, the records tell which
    REQUIRE(res.records.size() == 6);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(record_status(res, i) == sim::Reject::None);
        CHECK(res.records[i].building.valid());
    }
    CHECK(record_status(res, 4) == sim::Reject::CannotAfford);
    CHECK(record_status(res, 5) == sim::Reject::CannotAfford);
    CHECK_FALSE(res.records[4].building.valid());
    CHECK(money(r.w) == 10000 - 4 * 300);
    CHECK(stock(r.w, tools) == 1 * sim::kTon);
    CHECK(r.w.building_at(kCity0, 116, 115) == nullptr);
    CHECK(r.w.check_invariants() == "");

    // A failed record in the middle does not stop the ones after it.
    Rig r2;
    const sim::ApplyResult mid =
        r2.w.apply(build(0, kCity0,
                         {at(r2.t.weaver, 112, 115), at(r2.t.weaver, 108, 103) /* tile object */,
                          kZero, at(r2.t.weaver, 113, 115)}));
    CHECK(mid.ok());
    CHECK(record_status(mid, 0) == sim::Reject::None);
    CHECK(record_status(mid, 1) == sim::Reject::Occupied);
    CHECK(record_status(mid, 2) == sim::Reject::None);
    CHECK_FALSE(mid.records[2].building.valid());
    CHECK(record_status(mid, 3) == sim::Reject::None);

    // Stream order decides what the money buys: 320 gold pays one weaver (300) or the
    // stall (50) and then no weaver.
    Rig a, b;
    sim::WorldTestAccess::players(a.w).slots[0]->money = 320;
    sim::WorldTestAccess::players(b.w).slots[0]->money = 320;
    const sim::ApplyResult ra = a.w.apply(build(
        0, kCity0,
        {at(a.t.weaver, 112, 115), at(a.t.weaver, 113, 115), at(a.t.market_stall, 114, 115)}));
    CHECK(record_status(ra, 0) == sim::Reject::None);
    CHECK(record_status(ra, 1) == sim::Reject::CannotAfford);
    CHECK(record_status(ra, 2) == sim::Reject::CannotAfford); // 20 gold left
    CHECK(money(a.w) == 20);
    const sim::ApplyResult rb = b.w.apply(build(
        0, kCity0,
        {at(b.t.market_stall, 114, 115), at(b.t.weaver, 112, 115), at(b.t.weaver, 113, 115)}));
    CHECK(record_status(rb, 0) == sim::Reject::None);
    CHECK(record_status(rb, 1) == sim::Reject::CannotAfford); // 270 left
    CHECK(record_status(rb, 2) == sim::Reject::CannotAfford);
    CHECK(money(b.w) == 270);

    // Every record refused: the command is refused with the first reason and changes
    // nothing.
    Rig n;
    sim::WorldTestAccess::players(n.w).slots[0]->money = 0;
    check_refused(n.w, build(0, kCity0, {at(n.t.weaver, 112, 115), at(n.t.weaver, 108, 103)}),
                  sim::Reject::CannotAfford);
    // An empty build and a build of type-0 records only are accepted no-ops.
    const std::uint64_t h = n.w.state_hash();
    CHECK(n.w.apply(build(0, kCity0, {})).ok());
    CHECK(n.w.apply(build(0, kCity0, {kZero, kZero})).ok());
    CHECK(n.w.state_hash() == h);
}

TEST_CASE("build atomic rule") {
    // Rules::build_atomic (D8 opt-in): all records are validated against a scratch budget
    // first; one failure places nothing.
    syn::SyntheticTypes t = ct::build_types();
    sim::Rules atomic;
    atomic.build_atomic = true;
    sim::World w = ct::make_world(t, syn::g2_scenario(t), atomic);
    const std::vector<sim::PlacementWire> six = {at(t.weaver, 112, 115), at(t.weaver, 113, 115),
                                                 at(t.weaver, 114, 115), at(t.weaver, 115, 115),
                                                 at(t.weaver, 116, 115), at(t.weaver, 117, 115)};
    const std::uint64_t before = w.state_hash();
    const sim::ApplyResult res = w.apply(build(0, kCity0, six));
    CHECK(res.status == sim::Reject::AtomicAbort);
    REQUIRE(res.records.size() == 6);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(record_status(res, i) == sim::Reject::AtomicAbort);
    }
    // The fifth record is the one the budget cannot pay (4 x 3 t of 13 t tools spent).
    CHECK(record_status(res, 4) == sim::Reject::CannotAfford);
    CHECK(record_status(res, 5) == sim::Reject::CannotAfford);
    CHECK(w.state_hash() == before);
    CHECK(money(w) == 10000);

    // Two records on the same tile: each alone passes, together the second is Occupied
    // in the dry run (the earlier record's tiles count as taken).
    const sim::ApplyResult twin =
        w.apply(build(0, kCity0, {at(t.weaver, 112, 115), at(t.weaver, 112, 115)}));
    CHECK(twin.status == sim::Reject::AtomicAbort);
    CHECK(record_status(twin, 0) == sim::Reject::AtomicAbort);
    CHECK(record_status(twin, 1) == sim::Reject::Occupied);
    CHECK(w.state_hash() == before);
    // A multi-tile footprint over an earlier record's tile likewise (fur trapper 2 x 3).
    const sim::ApplyResult overlap =
        w.apply(build(0, kCity0, {at(t.weaver, 113, 117), at(t.fur_trapper, 112, 115)}));
    CHECK(overlap.status == sim::Reject::AtomicAbort);
    CHECK(record_status(overlap, 1) == sim::Reject::Occupied);
    CHECK(w.state_hash() == before);

    // When every record passes, all are placed and charged in order (type 0 skipped).
    const sim::ApplyResult ok =
        w.apply(build(0, kCity0,
                      {at(t.weaver, 112, 115), kZero, at(t.weaver, 113, 115),
                       at(t.weaver, 114, 115), at(t.weaver, 115, 115)}));
    CHECK(ok.ok());
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(record_status(ok, i) == sim::Reject::None);
    }
    CHECK(money(w) == 10000 - 4 * 300);
    CHECK(stock(w, ct::goods(t, "WERKZEUG")) == 1 * sim::kTon);
    CHECK(w.check_invariants() == "");

    // The same six records without the rule build four (D8, the original's default).
    sim::World plain = ct::make_world(t, syn::g2_scenario(t));
    CHECK(plain.apply(build(0, kCity0, six)).ok());
    CHECK(money(plain) == 10000 - 4 * 300);
    // The rule is hashed (Rules digest in the header).
    CHECK(ct::make_world(t, syn::g2_scenario(t)).state_hash() !=
          ct::make_world(t, syn::g2_scenario(t), atomic).state_hash());
}

TEST_CASE("build footprint owner rules") {
    // Free builds (the cheat rule), so that the cost never hides an owner rule.
    syn::SyntheticTypes t = ct::build_types();
    sim::Rules free;
    free.free_build_cheat = true;
    sim::World w = ct::make_world(t, ct::two_city_spec(t), free);
    REQUIRE(w.check_invariants() == "");
    const sim::TypeId trapper = t.fur_trapper; // 2 x 3, 3 x 2 when rotated
    // Wholly on city 0.
    CHECK(w.apply(build(0, kCity0, {at(trapper, 112, 110)})).ok());
    // Neutral tiles may be part of the footprint (D 0x10058890): rows 100-101 are
    // neutral, row 102 is city 0's.
    CHECK(w.apply(build(0, kCity0, {at(trapper, 120, 100)})).ok());
    // An all-neutral footprint belongs to no city: refused.
    check_refused(w, build(0, kCity0, {at(t.weaver, 130, 100)}), sim::Reject::TileNotOwned);
    check_refused(w, build(0, kCity0, {at(trapper, 130, 100, 1)}), sim::Reject::TileNotOwned);
    // Across the border of city 0 (rows .. 131) and city 1 (rows 132 ..): two owners.
    check_refused(w, build(0, kCity0, {at(trapper, 130, 130)}), sim::Reject::TileNotOwned);
    check_refused(w, build(1, kCity1, {at(trapper, 130, 130)}), sim::Reject::TileNotOwned);
    // Wholly on city 1: not the target city's tiles.
    check_refused(w, build(0, kCity0, {at(t.weaver, 130, 140)}), sim::Reject::TileNotOwned);
    // ... but city 1's owner may build there for city 1.
    CHECK(w.apply(build(1, kCity1, {at(t.weaver, 130, 140)})).ok());
    // The issuer must own the target city (F9).
    check_refused(w, build(0, kCity1, {at(t.weaver, 131, 140)}), sim::Reject::NotCityOwner);
    check_refused(w, build(1, kCity0, {at(t.weaver, 131, 115)}), sim::Reject::NotCityOwner);
    // Unknown island and city slots (F11).
    check_refused(w, build(0, sim::CityRef{sim::IslandId{5}, sim::CityNo{0}}, {}),
                  sim::Reject::UnknownIsland);
    check_refused(w, build(0, sim::CityRef{sim::IslandId{}, sim::CityNo{0}}, {}),
                  sim::Reject::UnknownIsland);
    check_refused(w, build(0, sim::CityRef{sim::IslandId{0}, sim::CityNo{7}}, {}),
                  sim::Reject::UnknownCity);
    check_refused(w, build(0, sim::CityRef{sim::IslandId{0}, sim::CityNo{15}}, {}),
                  sim::Reject::UnknownCity);
    // Off the island, also by one tile; the rotation swaps the footprint (D 0x100279e6):
    // at x = 162 the 2-wide trapper fits, the 3-wide rotated one does not.
    check_refused(w, build(0, kCity0, {at(t.weaver, 99, 110)}), sim::Reject::OutOfIsland);
    check_refused(w, build(0, kCity0, {at(t.weaver, 164, 110)}), sim::Reject::OutOfIsland);
    check_refused(w, build(0, kCity0, {at(trapper, 112, 162)}), sim::Reject::OutOfIsland);
    check_refused(w, build(0, kCity0, {at(trapper, 162, 120, 1)}), sim::Reject::OutOfIsland);
    check_refused(w, build(0, kCity0, {at(t.weaver, -0x7fffffff - 1, 0x7fffffff)}),
                  sim::Reject::OutOfIsland);
    CHECK(w.apply(build(0, kCity0, {at(trapper, 162, 120, 0)})).ok());
    // Rotation 1 at y = 129: 3 wide, 2 high, rows 129-130, all city 0.
    CHECK(w.apply(build(0, kCity0, {at(trapper, 140, 129, 1)})).ok());
    // Rotation 0 at the same place would reach row 131 and 132 (city 1).
    check_refused(w, build(0, kCity0, {at(trapper, 150, 130, 0)}), sim::Reject::TileNotOwned);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("build occupied") {
    Rig r;
    // The stand-in placement rule (D17): a tile object (scenery at (108,103)) refuses.
    check_refused(r.w, build(0, kCity0, {at(r.t.weaver, 108, 103)}), sim::Reject::Occupied);
    // A building's tiles carry its object (O-5): the weaver at (110,110) refuses.
    check_refused(r.w, build(0, kCity0, {at(r.t.weaver, 110, 110)}), sim::Reject::Occupied);
    // A footprint that only partly covers a building (fur trapper 2 x 3 from (109,108)
    // covers (110,110)).
    check_refused(r.w, build(0, kCity0, {at(r.t.fur_trapper, 109, 108)}), sim::Reject::Occupied);
    // A record placed earlier in the same command occupies its tiles for the later ones.
    const sim::ApplyResult res =
        r.w.apply(build(0, kCity0, {at(r.t.weaver, 112, 115), at(r.t.weaver, 112, 115)}));
    CHECK(record_status(res, 0) == sim::Reject::None);
    CHECK(record_status(res, 1) == sim::Reject::Occupied);
    // After the demolition the tile is free again.
    CHECK(r.w.apply(demolish(0, kCity0, {{112, 115}})).ok());
    CHECK(r.w.apply(build(0, kCity0, {at(r.t.weaver, 112, 115)})).ok());
    // Demolishing the scenery frees its tile too (stand-in O-5).
    CHECK(r.w.apply(demolish(0, kCity0, {{108, 103}})).ok());
    CHECK(r.w.apply(build(0, kCity0, {at(r.t.weaver, 108, 103)})).ok());
    CHECK(r.w.check_invariants() == "");
}

TEST_CASE("build unknown type and rotation") {
    Rig r;
    ct::city_mut(r.w, kCity0).stock.stock[ct::goods(r.t, "WERKZEUG")] = 50 * sim::kTon;
    // F4: an unknown type fails closed, before the rotation is looked at.
    check_refused(r.w, build(0, kCity0, {at(sim::TypeId{12345}, 112, 115)}),
                  sim::Reject::UnknownType);
    check_refused(r.w, build(0, kCity0, {at(sim::TypeId{12345}, 112, 115, 9)}),
                  sim::Reject::UnknownType);
    check_refused(r.w, build(0, kCity0, {at(sim::TypeId{0xffffffffU}, 112, 115)}),
                  sim::Reject::UnknownType);
    // F4: a rotation above 3.
    for (const std::uint8_t rot : {std::uint8_t{4}, std::uint8_t{5}, std::uint8_t{0xff}}) {
        check_refused(r.w, build(0, kCity0, {at(r.t.weaver, 112, 115, rot)}),
                      sim::Reject::BadRotation);
    }
    // PRODTYPE above 192 (a road): a tile object, not ported (O-4).
    check_refused(r.w, build(0, kCity0, {at(r.t.road, 112, 115)}), sim::Reject::Unsupported);
    // Type id 0 is skipped, not refused (D).
    const std::uint64_t h = r.w.state_hash();
    const sim::ApplyResult zero = r.w.apply(build(0, kCity0, {at(sim::TypeId{0}, 112, 115)}));
    CHECK(zero.ok());
    CHECK(record_status(zero, 0) == sim::Reject::None);
    CHECK_FALSE(zero.records[0].building.valid());
    CHECK(r.w.state_hash() == h);
    // Rotations 0..3 are all valid.
    for (std::uint8_t rot = 0; rot < 4; ++rot) {
        CHECK(r.w.apply(build(0, kCity0, {at(r.t.weaver, 112 + rot, 116, rot)})).ok());
        CHECK(r.w.building_at(kCity0, 112 + rot, 116)->rotation == rot);
    }
    // A client's byte8 is not used: the building's byte0f is 0 (§8.4).
    sim::PlacementWire p = at(r.t.weaver, 120, 116);
    p.byte7 = 0x55;
    p.byte8 = 0xaa;
    CHECK(r.w.apply(build(0, kCity0, {p})).ok());
    CHECK(r.w.building_at(kCity0, 120, 116)->byte0f == 0);
    CHECK(r.w.check_invariants() == "");
}

TEST_CASE("build flags are not on the wire") {
    // F13: the original's build flags (free build 0x2000000, no upgrade 0x4000000) came
    // from the payload. The rebuild's Build has no flags: the record is island, city,
    // count and the placements, nothing else.
    static_assert(anno::sim::field_count<sim::BuildCmd, 8>() == 2);
    static_assert(anno::sim::field_count<sim::PlacementWire, 8>() == 6);
    const sim::BuildCmd cmd{kCity0, {sim::PlacementWire{sim::TypeId{9001}, 1, 2, 3, 4, 5}}};
    auto bytes = sim::encode_record(sim::Body{cmd});
    REQUIRE(bytes.ok());
    CHECK(bytes.value().size() == 4 + 12 + 16);
    // The importer of the original codec reads the flags word and drops it.
    Rig r;
    sim::WorldTestAccess::players(r.w).slots[0]->money = 0;
    const std::vector<std::uint8_t> legacy = {
        0x00, 0x00, 0x00, 0x02, // flags 0x02000000: free build in the original
        0x00, 0x00,             // island 0, city 0
        0xe9, 0x03,             // type 1001 (the weaving hut), no followers
        112,  0x00, 115,  0x00, // x 112, y 115
        0x00, 0x00};            // rotation 0, rec8 0
    auto decoded = sim::legacy::decode_0x64(legacy);
    REQUIRE_MESSAGE(decoded.ok(), (decoded.ok() ? std::string() : decoded.error()));
    REQUIRE(decoded.value().placements.size() == 1);
    CHECK(decoded.value().placements[0].type == sim::TypeId{1001});
    // The synthetic weaver stands in for 1001 (its id is beyond the codec's 12 bits).
    sim::BuildCmd imported = decoded.value();
    imported.placements[0].type = r.t.weaver;
    // Without money the flag of the original does not make the build free.
    check_refused(r.w, sim::Command{sim::PlayerId{0}, imported}, sim::Reject::CannotAfford);
}

TEST_CASE("free build cheat rule") {
    syn::SyntheticTypes t = ct::build_types();
    sim::ScenarioSpec spec = syn::g2_scenario(t);
    spec.players[0].money = 0;
    spec.islands[0].cities[0].stock.reset(); // an empty warehouse
    // Default (the original for an honest client): nothing is free.
    sim::World plain = ct::make_world(t, spec);
    check_refused(plain, build(0, kCity0, {at(t.weaver, 112, 115)}), sim::Reject::CannotAfford);
    // Rules::free_build_cheat (F13): every build of the session is free, nothing is
    // charged and nothing counted as spent.
    sim::Rules cheat;
    cheat.free_build_cheat = true;
    sim::World w = ct::make_world(t, spec, cheat);
    const sim::ApplyResult res = w.apply(build(0, kCity0, {at(t.weaver, 112, 115)}));
    CHECK(res.ok());
    CHECK(money(w) == 0);
    for (const sim::BuildGoods& g : ct::city(w, kCity0).build_goods) {
        CHECK(g.spent == 0);
    }
    CHECK(w.check_invariants() == "");
    // The rule is part of the hashed header.
    CHECK(plain.state_hash() != ct::make_world(t, spec, cheat).state_hash());
    // The owner's waiver (D owner slot +0x9c(7), O-9) is honoured without the rule.
    sim::WorldTestAccess::players(plain).slots[0]->free_build = true;
    CHECK(plain.apply(build(0, kCity0, {at(t.weaver, 112, 115)})).ok());
    CHECK(money(plain) == 0);
    // ... and only for that player's own builds: the waiver is the owner's.
    sim::World two = ct::make_world(t, ct::two_city_spec(t));
    sim::WorldTestAccess::players(two).slots[0]->free_build = true;
    sim::WorldTestAccess::players(two).slots[1]->money = 0;
    check_refused(two, build(1, kCity1, {at(t.weaver, 130, 140)}), sim::Reject::CannotAfford);
}

TEST_CASE("demolish frees the slot and keeps the hint") {
    // The hand-derived table of docs/sim/state.md §15 S10, row by row. Builds are free
    // here (the cheat rule), so thirteen weavers fit the warehouse; the slot rule does
    // not depend on the cost.
    syn::SyntheticTypes t = ct::build_types();
    sim::Rules rules;
    rules.free_build_cheat = true;
    sim::World w = ct::make_world(t, syn::g1_scenario(t), rules);
    auto list = [&]() -> const sim::BuildingList& { return ct::production_list(w, kCity0, t); };
    REQUIRE(list().slots.size() == 10);
    REQUIRE(list().hint == 1);
    std::vector<sim::BuildingId> b(14);
    std::int32_t next_x = 112;
    // Builds B<i> and returns the slot it took.
    auto place = [&](std::size_t i) {
        const sim::ApplyResult res = w.apply(build(0, kCity0, {at(t.weaver, next_x, 125)}));
        REQUIRE(res.ok());
        b[i] = res.records.at(0).building;
        CHECK(sim::serial_of(b[i]) == 2 + i); // serials 3 .. 15, never reused
        ++next_x;
        return ct::key_of(w, b[i]).slot;
    };
    auto remove_at = [&](sim::TileXY tile) {
        const sim::ApplyResult res = w.apply(demolish(0, kCity0, {tile}));
        REQUIRE(res.ok());
        CHECK(res.records.at(0).status == sim::Reject::None);
    };
    const sim::TileXY farm{120, 110};
    // Step 1-2.
    CHECK(place(1) == 2);
    CHECK(list().hint == 2);
    CHECK(place(2) == 3);
    CHECK(list().hint == 3);
    // Step 3: the farm (slot 1) goes; hint and length stay.
    REQUIRE(ct::key_of(w, w.building_at(kCity0, farm.x, farm.y)->id).slot == 1);
    remove_at(farm);
    CHECK_FALSE(list().slots[1].has_value());
    CHECK(list().hint == 3);
    CHECK(list().slots.size() == 10);
    // Step 4: the scan starts after the hint, so the freed slot 1 is NOT taken.
    CHECK(place(3) == 4);
    CHECK(list().hint == 4);
    // Step 5.
    for (std::size_t i = 4; i <= 8; ++i) {
        CHECK(place(i) == i + 1);
    }
    CHECK(list().hint == 9);
    // Step 6: the scan wraps to 0 (the first weaver) and takes slot 1.
    CHECK(place(9) == 1);
    CHECK(list().hint == 1);
    CHECK(list().slots.size() == 10);
    // Step 7: no free slot: the list grows to 12, the new building takes slot 10.
    CHECK(place(10) == 10);
    CHECK(list().hint == 10);
    CHECK(list().slots.size() == 12);
    // Step 8: B2 (slot 3), then B1 (slot 2) go; the hint stays.
    remove_at({113, 125});
    remove_at({112, 125});
    CHECK(list().hint == 10);
    CHECK_FALSE(list().slots[2].has_value());
    CHECK_FALSE(list().slots[3].has_value());
    // Steps 9-11.
    CHECK(place(11) == 11);
    CHECK(list().hint == 11);
    CHECK(place(12) == 2);
    CHECK(list().hint == 2);
    CHECK(place(13) == 3);
    CHECK(list().hint == 3);
    CHECK(list().slots.size() == 12);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("demolish skips unowned tiles") {
    syn::SyntheticTypes t = ct::build_types();
    sim::World w = ct::make_world(t, ct::two_city_spec(t));
    REQUIRE(w.apply(build(0, kCity0, {at(t.fur_trapper, 112, 115)})).ok());
    const sim::BuildingId trapper = w.building_at(kCity0, 112, 115)->id;
    const sim::BuildingId other = w.building_at(kCity1, 110, 140)->id;
    const std::uint64_t h0 = w.state_hash();
    // Tiles that are not city 0's are skipped silently (D, op 6 of 0x10058e90): outside
    // the island, neutral, another city's (with that city's building on it). The
    // command is accepted; the records say why nothing happened.
    const sim::ApplyResult skip =
        w.apply(demolish(0, kCity0, {{0, 0}, {110, 100}, {110, 140}, {130, 125} /* empty own */}));
    CHECK(skip.ok());
    REQUIRE(skip.records.size() == 4);
    CHECK(record_status(skip, 0) == sim::Reject::OutOfIsland);
    CHECK(record_status(skip, 1) == sim::Reject::TileNotOwned);
    CHECK(record_status(skip, 2) == sim::Reject::TileNotOwned);
    CHECK(record_status(skip, 3) == sim::Reject::NoBuildingAtTile);
    CHECK(w.state_hash() == h0);
    CHECK(w.find_building(other) != nullptr);
    // Another player may not demolish in city 0 at all (F9).
    check_refused(w, demolish(1, kCity0, {{112, 115}}), sim::Reject::NotCityOwner);
    // Any tile of a footprint finds the building (FindByTile over the lookup order); a
    // second tile of the same building then finds nothing.
    const sim::ApplyResult res = w.apply(demolish(0, kCity0, {{113, 117}, {112, 115}}));
    CHECK(res.ok());
    CHECK(record_status(res, 0) == sim::Reject::None);
    CHECK(res.records[0].building == trapper);
    CHECK(record_status(res, 1) == sim::Reject::NoBuildingAtTile);
    CHECK(w.find_building(trapper) == nullptr);
    // A tile object without a building is cleared (stand-in, O-5).
    REQUIRE(w.islands()[0]->tiles.object(3U * 64U + 8U) != 0);
    CHECK(w.apply(demolish(0, kCity0, {{108, 103}})).ok());
    CHECK(w.islands()[0]->tiles.object(3U * 64U + 8U) == 0);
    // Demolishing refunds nothing (manual: docs/research/playbook.md §14).
    CHECK(money(w) == 10000 - 100);
    CHECK(w.check_invariants() == "");
}

namespace {

int g_empty_calls = 0;

/// A city-empty seam for the tests: "empty" when no list holds a building.
bool city_has_no_building(void*, const sim::City& c) {
    ++g_empty_calls;
    for (const sim::BuildingList& l : c.index.lists) {
        if (l.live() != 0) {
            return false;
        }
    }
    return true;
}

sim::Seams empty_seams() {
    sim::Seams s;
    s.city_is_empty.name = "test_city_has_no_building";
    s.city_is_empty.fn = &city_has_no_building;
    return s;
}

} // namespace

TEST_CASE("city removal default never") {
    syn::SyntheticTypes t = ct::build_types();
    const std::vector<sim::TileXY> all = {{110, 110}, {120, 110}};
    // Default (no seam; D9's predicate 0x1002f7f0 is unread, O-6): never removed.
    sim::World w = ct::make_world(t, syn::g2_scenario(t));
    CHECK(w.apply(demolish(0, kCity0, all)).ok());
    REQUIRE(w.islands()[0]->city(sim::CityNo{0}) != nullptr);
    CHECK(ct::city(w, kCity0).index.lists.size() == w.config().layout.lists.size());
    CHECK(w.check_invariants() == "");

    // With a predicate the city goes after the tile that emptied it: its buildings, its
    // slot, and its tiles become neutral; later tiles of the command find no city.
    g_empty_calls = 0;
    sim::World e = ct::make_world(t, syn::g2_scenario(t), {}, empty_seams());
    const sim::ApplyResult res = e.apply(demolish(0, kCity0, {{110, 110}, {120, 110}, {130, 130}}));
    CHECK(res.ok());
    CHECK(record_status(res, 0) == sim::Reject::None);
    CHECK(record_status(res, 1) == sim::Reject::None);
    CHECK(record_status(res, 2) == sim::Reject::UnknownCity);
    CHECK(g_empty_calls == 2); // after each tile while the city exists
    CHECK(e.islands()[0]->city(sim::CityNo{0}) == nullptr);
    const sim::TileLayer& tiles = e.islands()[0]->tiles;
    for (std::uint64_t i = 0; i < tiles.tile_count(); ++i) {
        CHECK(tiles.owner(i) == e.config().neutral_owner);
    }
    CHECK(e.check_invariants() == "");
    // A command for the removed city is refused.
    check_refused(e, build(0, kCity0, {at(t.weaver, 112, 115)}), sim::Reject::UnknownCity);

    // Rules::keep_empty_city keeps it although the predicate says empty.
    sim::Rules keep;
    keep.keep_empty_city = true;
    g_empty_calls = 0;
    sim::World k = ct::make_world(t, syn::g2_scenario(t), keep, empty_seams());
    CHECK(k.apply(demolish(0, kCity0, all)).ok());
    CHECK(k.islands()[0]->city(sim::CityNo{0}) != nullptr);
    CHECK(g_empty_calls == 0);
    CHECK(k.check_invariants() == "");
}

TEST_CASE("trade flag modes 0 1 9") {
    Rig r;
    const sim::GoodsId wool = ct::goods(r.t, "WOLLE");
    const sim::GoodsId wood = ct::goods(r.t, "HOLZ");
    // An entry for wool on list 0 (sell) and one on list 1 (buy).
    REQUIRE(
        r.w.apply(ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, wool, 7 * sim::kTon, 40)).ok());
    REQUIRE(
        r.w.apply(ct::trade_slot(0, kCity0, 1, sim::kAppendIndex, wood, 2 * sim::kTon, 9)).ok());
    REQUIRE(
        r.w.apply(ct::trade_slot(0, kCity0, 1, sim::kAppendIndex, wool, 1 * sim::kTon, 8)).ok());
    const sim::GoodsSetting fresh = ct::city(r.w, kCity0).goods.at(wool);
    CHECK(fresh.list_index == -1);
    CHECK_FALSE(fresh.buy);
    // Mode 0: the goods' index on list 0, buy cleared (D 0x1002dbd0).
    CHECK(r.w.apply(ct::trade_flag(0, kCity0, 0, wool, 0)).ok());
    CHECK(ct::city(r.w, kCity0).goods.at(wool).list_index == 0);
    CHECK_FALSE(ct::city(r.w, kCity0).goods.at(wool).buy);
    CHECK(r.w.check_invariants() == "");
    // Mode 1: list 1, where wool is entry 1.
    CHECK(r.w.apply(ct::trade_flag(0, kCity0, 1, wool, 1)).ok());
    CHECK(ct::city(r.w, kCity0).goods.at(wool).list_index == 1);
    CHECK(ct::city(r.w, kCity0).goods.at(wool).buy);
    CHECK(r.w.check_invariants() == "");
    // -1 (0xffffffff) clears the index and keeps the mode bit.
    CHECK(r.w.apply(ct::trade_flag(0, kCity0, 1, wool, 0xffffffffU)).ok());
    CHECK(ct::city(r.w, kCity0).goods.at(wool).list_index == -1);
    CHECK(ct::city(r.w, kCity0).goods.at(wool).buy);
    // An index whose entry holds other goods, or no entry: BadValue (§9.6).
    check_refused(r.w, ct::trade_flag(0, kCity0, 1, wool, 0), sim::Reject::BadValue);
    check_refused(r.w, ct::trade_flag(0, kCity0, 0, wool, 1), sim::Reject::BadValue);
    check_refused(r.w, ct::trade_flag(0, kCity0, 0, wood, 0), sim::Reject::BadValue);
    // Mode 9: the setting of a build goods ([city + 0x48c + 8g]).
    CHECK(r.w.apply(ct::trade_flag(0, kCity0, 9, wood, 77)).ok());
    CHECK(ct::city(r.w, kCity0).build_goods.at(wood - r.w.config().build_goods_first).setting ==
          77);
    CHECK(r.w.apply(ct::trade_flag(0, kCity0, 9, wood, 0xfffffff0U)).ok());
    CHECK(ct::city(r.w, kCity0).build_goods.at(wood - r.w.config().build_goods_first).setting ==
          0xfffffff0U);
    // Mode 9 on a goods outside the build goods and every other mode are ignored.
    const std::uint64_t h = r.w.state_hash();
    CHECK(r.w.apply(ct::trade_flag(0, kCity0, 9, wool, 5)).ok());
    for (const std::uint16_t mode :
         {std::uint16_t{2}, std::uint16_t{8}, std::uint16_t{10}, std::uint16_t{0xffff}}) {
        CHECK(r.w.apply(ct::trade_flag(0, kCity0, mode, wool, 0)).ok());
    }
    CHECK(r.w.state_hash() == h);
    // City checks first: another player's command is refused.
    check_refused(r.w, ct::trade_flag(3, kCity0, 0, wool, 0), sim::Reject::UnknownIssuer);
    CHECK(r.w.check_invariants() == "");
}

TEST_CASE("trade slot index") {
    Rig r;
    const sim::GoodsId wool = ct::goods(r.t, "WOLLE");
    const sim::GoodsId wood = ct::goods(r.t, "HOLZ");
    // kAppendIndex (the original's 0x8a) appends; an index names an entry.
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, wool, 5, 6)).ok());
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, wood, 7, 8)).ok());
    const auto& list0 = [&]() -> const std::vector<sim::TradeEntry>& {
        return ct::city(r.w, kCity0).trade_lists.at(0);
    };
    REQUIRE(list0().size() == 2);
    CHECK(list0()[1].goods == wood);
    CHECK(list0()[1].amount_88 == 7);
    CHECK(list0()[1].price == 8);
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, 1, wood, 70, 80)).ok());
    CHECK(list0()[1].amount_88 == 70);
    CHECK(list0()[1].price == 80);
    CHECK(list0().size() == 2);
    // Beyond the list: BadIndex (only kAppendIndex appends).
    check_refused(r.w, ct::trade_slot(0, kCity0, 0, 2, wool), sim::Reject::BadIndex);
    check_refused(r.w, ct::trade_slot(0, kCity0, 0, sim::kAppendIndex - 1, wool),
                  sim::Reject::BadIndex);
    // Goods 0 clears an entry in place (the list keeps its length); it appends nothing.
    REQUIRE(r.w.apply(ct::trade_flag(0, kCity0, 0, wool, 0)).ok());
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, 0, 0, 99, 99)).ok());
    CHECK(list0().size() == 2);
    CHECK(list0()[0].goods == 0);
    CHECK(list0()[0].amount_88 == 0);
    CHECK(list0()[0].price == 0);
    // The goods setting that named the cleared entry is unlinked (the original's removal
    // writes 0xff, 0x100285f4).
    CHECK(ct::city(r.w, kCity0).goods.at(wool).list_index == -1);
    check_refused(r.w, ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, 0), sim::Reject::BadValue);
    // Overwriting an entry with other goods unlinks the old goods' setting as well.
    REQUIRE(r.w.apply(ct::trade_flag(0, kCity0, 0, wood, 1)).ok());
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, 1, wool, 1, 1)).ok());
    CHECK(ct::city(r.w, kCity0).goods.at(wood).list_index == -1);
    // ... but not a setting of the same goods, nor one that names the other list.
    REQUIRE(r.w.apply(ct::trade_flag(0, kCity0, 0, wool, 1)).ok());
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, 1, wool, 2, 2)).ok());
    CHECK(ct::city(r.w, kCity0).goods.at(wool).list_index == 1);
    REQUIRE(r.w.apply(ct::trade_slot(0, kCity0, 1, sim::kAppendIndex, wood, 1, 1)).ok());
    REQUIRE(r.w.apply(ct::trade_slot(0, kCity0, 1, sim::kAppendIndex, wool, 1, 1)).ok());
    REQUIRE(r.w.apply(ct::trade_flag(0, kCity0, 1, wool, 1)).ok());     // wool on list 1 at 1
    CHECK(r.w.apply(ct::trade_slot(0, kCity0, 0, 1, wood, 3, 3)).ok()); // list 0 entry 1
    CHECK(ct::city(r.w, kCity0).goods.at(wool).list_index == 1);
    CHECK(ct::city(r.w, kCity0).goods.at(wool).buy);
    CHECK(r.w.check_invariants() == "");
}

TEST_CASE("trade limit and list bounds") {
    Rig r;
    const sim::GoodsId wool = ct::goods(r.t, "WOLLE");
    const std::uint32_t lists = r.w.config().trade_lists;
    REQUIRE(lists == 2);
    // The list number is a u32 on the wire (F3: a byte in the original): every value from
    // trade_lists up is refused.
    for (const std::uint32_t list : {lists, lists + 1, 255U, 256U, 0xffffffffU}) {
        check_refused(r.w, ct::trade_slot(0, kCity0, list, sim::kAppendIndex, wool),
                      sim::Reject::BadIndex);
    }
    // A goods number beyond the warehouse: BadIndex for the flag, BadValue for an entry.
    const std::uint32_t wg = r.w.config().warehouse_goods;
    for (const std::uint32_t g : {wg, wg + 1, 0xffffffffU}) {
        check_refused(r.w, ct::trade_flag(0, kCity0, 0, g, 0xffffffffU), sim::Reject::BadIndex);
        check_refused(r.w, ct::trade_flag(0, kCity0, 9, g, 1), sim::Reject::BadIndex);
        check_refused(r.w, ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, g),
                      sim::Reject::BadValue);
    }
    // The flag's value is an index into the list, not a free number: beyond the entries,
    // beyond i32, and the original's byte 0xff (an index here, not -1) are refused.
    REQUIRE(r.w.apply(ct::trade_slot(0, kCity0, 0, sim::kAppendIndex, wool)).ok());
    for (const std::uint32_t v : {1U, 0xffU, 0x7fffffffU, 0x80000000U, 0xfffffffeU}) {
        check_refused(r.w, ct::trade_flag(0, kCity0, 0, wool, v), sim::Reject::BadValue);
    }
    // A config with three lists: list 2 exists; mode 1 still names list 1 only.
    syn::SyntheticTypes t = ct::build_types();
    sim::SimConfig cfg;
    cfg.trade_lists = 3;
    sim::World w3 = ct::make_world(t, syn::g2_scenario(t), {}, {}, cfg);
    CHECK(w3.apply(ct::trade_slot(0, kCity0, 2, sim::kAppendIndex, wool)).ok());
    check_refused(w3, ct::trade_slot(0, kCity0, 3, sim::kAppendIndex, wool), sim::Reject::BadIndex);
    check_refused(w3, ct::trade_flag(0, kCity0, 1, wool, 0), sim::Reject::BadValue);
    // With one list, mode 1 has no list to name: only -1 is accepted.
    sim::SimConfig one;
    one.trade_lists = 1;
    sim::World w1 = ct::make_world(t, syn::g2_scenario(t), {}, {}, one);
    CHECK(w1.apply(ct::trade_flag(0, kCity0, 1, wool, 0xffffffffU)).ok());
    check_refused(w1, ct::trade_flag(0, kCity0, 1, wool, 0), sim::Reject::BadValue);
    CHECK(w1.check_invariants() == "");
    CHECK(r.w.check_invariants() == "");
}

TEST_CASE("legacy 0x64 decode on synthetic bytes") {
    // Our own bytes in the layout of §8.5 (D14): two groups, the first with two
    // followers, the second with none. The type field has 12 bits (ids up to 4095,
    // Anlagen.dat's ids are below 2600).
    const std::vector<std::uint8_t> bytes = {
        0x78, 0x56, 0x34, 0x12, // flags (dropped)
        3, 1,                   // island 3, city 1
        // group 1: type 1001 (0x3e9) | 2 << 12, x 110, y 120, rotation 1 | rec7 5 << 2,
        // rec8 9
        0xe9, 0x23, 110, 0, 120, 0, 1 | (5 << 2), 9,
        // follower {rotation 2, dx -1, dy +3}, {rotation 0, dx +5, dy -128}
        2, 0xff, 3, 0, 5, 0x80,
        // group 2: type 1205 (0x4b5), x 65535, y 0, rotation 3 | rec7 63 << 2, rec8 0
        0xb5, 0x04, 0xff, 0xff, 0, 0, 0xff, 0};
    auto d = sim::legacy::decode_0x64(bytes);
    REQUIRE_MESSAGE(d.ok(), (d.ok() ? std::string() : d.error()));
    const sim::BuildCmd& b = d.value();
    CHECK(b.city.island == sim::IslandId{3});
    CHECK(b.city.city == sim::CityNo{1});
    REQUIRE(b.placements.size() == 4);
    CHECK(b.placements[0] == sim::PlacementWire{sim::TypeId{1001}, 110, 120, 1, 5, 9});
    // Followers: the head's type, rec7 and rec8, the head's tile plus their offset (not
    // the previous follower's).
    CHECK(b.placements[1] == sim::PlacementWire{sim::TypeId{1001}, 109, 123, 2, 5, 9});
    CHECK(b.placements[2] == sim::PlacementWire{sim::TypeId{1001}, 115, -8, 0, 5, 9});
    CHECK(b.placements[3] == sim::PlacementWire{sim::TypeId{1205}, 65535, 0, 3, 63, 0});
    // Every prefix: a cut inside the head or a group is an error; a cut on a group
    // boundary decodes the groups before it.
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        const std::span<const std::uint8_t> prefix(bytes.data(), n);
        auto p = sim::legacy::decode_0x64(prefix);
        if (n == 6) {
            REQUIRE(p.ok());
            CHECK(p.value().placements.empty());
        } else if (n == 6 + 8 + 6) {
            REQUIRE(p.ok());
            CHECK(p.value().placements.size() == 3);
        } else {
            CHECK_FALSE(p.ok());
        }
    }
    // The decoded command goes through the same rules as a native one (here: island 3
    // does not exist in the g2 world).
    Rig r;
    check_refused(r.w, sim::Command{sim::PlayerId{0}, b}, sim::Reject::UnknownIsland);
    // A follower's rotation byte is kept whole, so apply refuses a rotation above 3.
    const std::vector<std::uint8_t> rot = {0, 0,   0, 0, 0, 0, 0xe9, 0x13, 112,
                                           0, 115, 0, 0, 0, 7, 1,    0};
    auto dr = sim::legacy::decode_0x64(rot);
    REQUIRE(dr.ok());
    REQUIRE(dr.value().placements.size() == 2);
    CHECK(dr.value().placements[1].rotation == 7);
    CHECK(dr.value().placements[1].x == 113);
    sim::BuildCmd imported = dr.value();
    for (sim::PlacementWire& p : imported.placements) {
        p.type = r.t.weaver; // the synthetic stand-in for 1001
    }
    const sim::ApplyResult res = r.w.apply(sim::Command{sim::PlayerId{0}, imported});
    CHECK(record_status(res, 0) == sim::Reject::None);
    CHECK(record_status(res, 1) == sim::Reject::BadRotation);
}
