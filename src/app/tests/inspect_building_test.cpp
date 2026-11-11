// W4-NEXT-1 part S, finding 2 (W4-MAIN-2 review of W4-SETTLEMENT-1, part S): no anno_app_tests
// case covered inspect_building or its per-kind formatters (play.cpp's DescribePayload). Every
// line below is hand-derived from play.cpp's own formatting code (tons(): whole = raw/256,
// frac = ((raw%256)*100)/256, two digits), not copied from a run, and every amount is an exact
// multiple of kTon (256) so the fractional part is always "00" and cannot hide a rounding bug.
// One negative control (a tile with no building) closes finding 2.
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include <doctest/doctest.h>

#include <anno/app/play.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

namespace app = anno::app;
namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
namespace formats = anno::formats;

namespace {

// make_synthetic_types() plus one Inn-class type (PRODTYPE 66): the same extension pattern
// synthetic.cpp's own make_transport_types() uses to add one type to the base registry.
struct Types {
    syn::SyntheticTypes base;
    sim::TypeId inn{9201};
    sim::TypeId quarry{9202};
};

formats::Result<Types, std::string> make_types() {
    auto base = syn::make_synthetic_types();
    if (!base) {
        return base.error();
    }
    Types t;
    t.base = std::move(base).value();
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& one : t.base.registry->types()) {
        b.add(one);
    }
    b.add_plain(t.inn, 66);
    b.add_plain(t.quarry, 13).entrances(t.quarry, {{0, 0}});
    auto built = b.build();
    if (!built) {
        return built.error();
    }
    t.base.registry = std::move(built).value();
    return t;
}

sim::ScenarioSpec::BuildingSpec place(sim::TypeId type, std::int32_t x, std::int32_t y,
                                      sim::Payload payload) {
    sim::ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    b.payload = std::move(payload);
    return b;
}

constexpr sim::TileXY kWorkshop{110, 110}, kFarm{112, 110}, kSmallFarm{114, 110}, kMine{116, 110},
    kInn{118, 110}, kQuarry{120, 110}, kEmpty{122, 110};

struct Rig {
    Types types;
    sim::World world;
};

Rig make_rig() {
    auto types = make_types();
    REQUIRE_MESSAGE(types.ok(), (types.ok() ? std::string() : types.error()));
    Types t = std::move(types).value();
    const sim::TypeRegistry& reg = *t.base.registry;

    // carriers_out/workers_out count actual figures referencing this building (none exist in
    // this scenario), so they stay 0 - a non-zero value is a payload_problem invariant
    // violation, not a display-only field, same as SmallFarm's figures_out below.
    sim::Workshop workshop = sim::make_workshop(reg.find(t.base.weaver)->prod);
    workshop.out[0] = 3 * sim::kTon;
    workshop.in[0] = 2 * sim::kTon;
    workshop.activity[0] = 7;

    sim::Farm farm;
    farm.out = 4 * sim::kTon;
    farm.raw = 5 * sim::kTon;
    farm.activity = 9;

    sim::SmallFarm small = sim::make_small_farm(reg.find(t.base.small_farm)->prod);
    small.out[0] = 1 * sim::kTon;
    small.out[1] = 2 * sim::kTon;
    small.raw[0] = 3 * sim::kTon;
    small.raw[1] = 4 * sim::kTon;
    small.activity[0] = 11;
    small.activity[1] = 13;
    // figures_out counts actual figures referencing this building (none exist in this scenario,
    // so every counter must stay 0 - a non-zero value here is a payload_problem invariant
    // violation, not a display-only field).

    sim::Mine mine;
    mine.out = 6 * sim::kTon;
    mine.activity = 42;
    // basic_scenario() gives island 0 exactly one deposit (EISENERZ, index 0); the iron mine's
    // own output good is EISENERZ, so index 0 is the only value payload_problem accepts here.
    mine.deposit = 0;

    sim::Inn inn;
    inn.tavern = 8;
    inn.taken = 3;

    sim::Quarry quarry;
    quarry.claims = {1};

    sim::ScenarioSpec spec = syn::basic_scenario();
    spec.islands[0].cities[0].buildings = {
        place(t.base.weaver, kWorkshop.x, kWorkshop.y, sim::Payload{workshop}),
        place(t.base.farm, kFarm.x, kFarm.y, sim::Payload{farm}),
        place(t.base.small_farm, kSmallFarm.x, kSmallFarm.y, sim::Payload{small}),
        place(t.base.iron_mine, kMine.x, kMine.y, sim::Payload{mine}),
        place(t.inn, kInn.x, kInn.y, sim::Payload{inn}),
        place(t.quarry, kQuarry.x, kQuarry.y, sim::Payload{quarry}),
    };
    auto world = sim::World::create(spec, t.base.registry);
    REQUIRE_MESSAGE(world.ok(), (world.ok() ? std::string() : world.error()));
    return Rig{std::move(t), std::move(world).value()};
}

constexpr sim::CityRef kCity{sim::IslandId{0}, sim::CityNo{0}};

} // namespace

TEST_CASE("inspect_building: workshop, hand-derived status line") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kWorkshop);
    REQUIRE(found.has_value());
    CHECK(found->kind == "Workshop");
    // out=3*256/256=3.00t, in=2.00t, activity=7, carriers_out=0 (left at the rig's default -
    // a nonzero figure-reference counter with no real figure in the scenario fails World::create).
    CHECK(found->status == "out=3.00t in=2.00t activity=7 carriers_out=0");
}

TEST_CASE("inspect_building: farm, hand-derived status line") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kFarm);
    REQUIRE(found.has_value());
    CHECK(found->kind == "Farm");
    CHECK(found->status == "out=4.00t raw=5.00t activity=9 workers_out=0");
}

TEST_CASE("inspect_building: small farm, hand-derived status line") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kSmallFarm);
    REQUIRE(found.has_value());
    CHECK(found->kind == "SmallFarm");
    // status.code defaults to 0 (StatusTracker{}); figures_out is sized per animal kind
    // (SmallFarmAnimals' default five kind_by_draw entries), not per output line, and stays at
    // its default (0 each: no figure in this scenario references this building).
    CHECK(found->status == "out=1.00t,2.00t raw=3.00t,4.00t activity=11,13 "
                           "figures_out=0,0,0,0,0 status_code=0");
}

TEST_CASE("inspect_building: mine, hand-derived status line") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kMine);
    REQUIRE(found.has_value());
    CHECK(found->kind == "Mine");
    CHECK(found->status == "out=6.00t activity=42 deposit=0");
}

TEST_CASE("inspect_building: inn, hand-derived status line") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kInn);
    REQUIRE(found.has_value());
    CHECK(found->kind == "Inn");
    CHECK(found->status == "tavern=8 taken=3 carriers_out=0");
}

TEST_CASE("inspect_building: quarry, hand-derived status line") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kQuarry);
    REQUIRE(found.has_value());
    CHECK(found->kind == "Quarry");
    CHECK(found->status == "claims=1 status_code=0");
}

TEST_CASE("inspect_building: a tile with no building is the negative control") {
    Rig rig = make_rig();
    const auto found = app::inspect_building(rig.world, kCity, kEmpty);
    CHECK_FALSE(found.has_value());
}
