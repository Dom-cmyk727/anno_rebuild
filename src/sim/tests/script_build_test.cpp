#include <doctest/doctest.h>

#include <anno/sim/placement.hpp>

#include "original_component.hpp"
#include "script_fixture.hpp"
namespace sim = anno::sim;
using namespace command_test;
TEST_CASE("SCRIPT1 public build pending deny allow stale query and player isolation") {
    auto f = script_test::make();
    auto& w = f.world;
    const auto type = f.types.weaver;
    const auto command = build(0, kCity0, {at(type, 120, 112)});
    // Imported custom building is present even while ordinary construction is pending/locked.
    REQUIRE(w.building_at(kCity0, 110, 110) != nullptr);
    auto hash = w.state_hash();
    CHECK(w.apply(command).status == sim::Reject::PermissionContext);
    CHECK(w.state_hash() == hash);
    w.advance(0);
    hash = w.state_hash();
    CHECK(w.apply(command).status == sim::Reject::ConstructionDenied);
    CHECK(w.state_hash() == hash);
    CHECK(w.construction_eligibility(sim::PlayerId{1}, type).allowed());
    w.advance(1000);
    w.advance(0);
    const auto preview = w.construction_eligibility(sim::PlayerId{0}, type);
    REQUIRE(preview.allowed());
    const auto before_gold = w.players().find(sim::PlayerId{0})->money;
    const auto before_tools = city(w, kCity0).stock.stock[41];
    const auto before_wood = city(w, kCity0).stock.stock[42];
    auto placed = w.apply(command);
    REQUIRE(placed.ok());
    REQUIRE(placed.records[0].building.valid());
    CHECK(w.players().find(sim::PlayerId{0})->money == before_gold - 300);
    CHECK(city(w, kCity0).stock.stock[41] == before_tools - 768);
    CHECK(city(w, kCity0).stock.stock[42] == before_wood - 1024);
    w.advance(1000);
    w.advance(0);
    hash = w.state_hash();
    CHECK(preview.allowed());
    CHECK(w.apply(build(0, kCity0, {at(type, 121, 112)})).status ==
          sim::Reject::ConstructionDenied);
    CHECK(w.state_hash() == hash);
    CHECK(w.construction_eligibility(sim::PlayerId{1}, type).allowed());
    auto other = w.apply(build(1, kCity1, {at(type, 120, 145)}));
    REQUIRE(other.ok());
    CHECK(other.records[0].building.valid());
}
TEST_CASE("SCRIPT1 public road result uses tile outcome and current permission") {
    auto f = script_test::make();
    auto& w = f.world;
    w.advance(0);
    auto hash = w.state_hash();
    CHECK(w.apply(build(0, kCity0, {at(f.types.road, 120, 112)})).status ==
          sim::Reject::ConstructionDenied);
    CHECK(w.state_hash() == hash);
    auto legal = w.apply(build(1, kCity1, {at(f.types.road, 120, 145)}));
    REQUIRE(legal.ok());
    REQUIRE(legal.records.size() == 1);
    CHECK_FALSE(legal.records[0].building.valid());
    const auto& is = *w.islands()[0];
    CHECK(is.tiles.object(is.tile_index(120, 145)) == f.types.road.v);
}
TEST_CASE("SCRIPT1 build atomic and sequential preserve documented prefix policy") {
    for (bool atomic : {false, true}) {
        auto f = script_test::make(atomic);
        auto& w = f.world;
        w.advance(0);
        const auto hash = w.state_hash();
        const auto command =
            build(0, kCity0, {at(f.types.farm, 120, 112), at(f.types.weaver, 121, 112)});
        auto result = w.apply(command);
        REQUIRE(result.records.size() == 2);
        CHECK(result.records[1].status == sim::Reject::ConstructionDenied);
        if (atomic) {
            CHECK(result.records[0].status == sim::Reject::AtomicAbort);
            CHECK(w.state_hash() == hash);
        } else {
            CHECK(result.records[0].status == sim::Reject::None);
            CHECK(w.state_hash() != hash);
        }
    }
}
TEST_CASE("SCRIPT1 build still refuses ownership terrain and resource failures") {
    auto f = script_test::make();
    auto& w = f.world;
    script_test::unlock(w);
    CHECK(w.apply(build(1, kCity0, {at(f.types.weaver, 120, 112)})).status ==
          sim::Reject::NotCityOwner);
    CHECK(w.apply(build(0, kCity0, {at(f.types.weaver, 120, 140)})).status ==
          sim::Reject::TileNotOwned);
    sim::WorldTestAccess::seams(w).placement_rule = {
        "script_terrain_refusal",
        +[](void*, const sim::Island&, const sim::TypeRegistry&, const sim::BuildingType&,
            const sim::PlacementRecord&) -> sim::PlacementResult {
            return {sim::Reject::TerrainRefused, 0};
        },
        nullptr};
    auto terrain_hash = w.state_hash();
    CHECK(w.apply(build(0, kCity0, {at(f.types.weaver, 120, 112)})).status ==
          sim::Reject::TerrainRefused);
    CHECK(w.state_hash() == terrain_hash);
    sim::WorldTestAccess::seams(w).placement_rule = {};
    sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->money = 0;
    auto hash = w.state_hash();
    CHECK(w.apply(build(0, kCity0, {at(f.types.weaver, 120, 112)})).status ==
          sim::Reject::CannotAfford);
    CHECK(w.state_hash() == hash);
    sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->money = 10000;
    city_mut(w, kCity0).stock.stock[41] = 0;
    hash = w.state_hash();
    CHECK(w.apply(build(0, kCity0, {at(f.types.weaver, 120, 112)})).status ==
          sim::Reject::CannotAfford);
    CHECK(w.state_hash() == hash);
}
namespace {
void rewrite(void* context, const sim::BuildingType&, sim::PlacementRecord& rec) {
    rec.type = *static_cast<sim::TypeId*>(context);
}
} // namespace
TEST_CASE("SCRIPT1 rewrite requires requested and final effective types in live and atomic") {
    for (bool atomic : {false, true}) {
        auto target = sim::TypeId{};
        sim::Seams seams;
        seams.upgrade_rewrite = {"script_rewrite", &rewrite, &target};
        auto f = script_test::make(atomic, {}, seams);
        auto& w = f.world;
        w.advance(0);
        target = f.types.farm;
        const auto hash = w.state_hash();
        auto requested_denied = w.apply(build(0, kCity0, {at(f.types.weaver, 120, 112)}));
        CHECK(requested_denied.records[0].status == sim::Reject::ConstructionDenied);
        CHECK(w.state_hash() == hash);
        target = f.types.weaver;
        auto effective_denied = w.apply(build(0, kCity0, {at(f.types.farm, 120, 112)}));
        CHECK(effective_denied.records[0].status == sim::Reject::ConstructionDenied);
        CHECK(w.state_hash() == hash);
        script_test::unlock(w);
        auto legal = w.apply(build(0, kCity0, {at(f.types.farm, 120, 112)}));
        REQUIRE(legal.ok());
    }
}
TEST_CASE("SCRIPT1 missing context and unsupported program cannot authorize construction") {
    auto types = build_types();
    auto spec = sim::synthetic::g2_scenario(types);
    spec.script_context = sim::ScriptContext::Missing;
    auto made = sim::World::create(spec, types.registry);
    REQUIRE(made);
    auto& w = made.value();
    const auto hash = w.state_hash();
    CHECK(w.apply(build(0, kCity0, {at(types.weaver, 120, 112)})).status ==
          sim::Reject::PermissionContext);
    CHECK(w.state_hash() == hash);
    spec.script_context = sim::ScriptContext::Pending;
    spec.script_source = script_test::source("INITBLOCK(){ PLAYWAVE(\"x\"); }");
    auto refused = sim::World::create(spec, types.registry);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().find("unsupported action") != std::string::npos);
    CHECK(w.state_hash() == hash);
}

TEST_CASE("SCRIPT1 staged world permission grants and timers commit or roll back together") {
    for (auto budget : {1u, 2u}) {
        const auto kernel = "INITBLOCK(){" +
                            script_test::permission("ME", sim::TypeId{90001}, true) +
                            "TIMER_SET(1,7);}";
        auto f = script_test::make(false, kernel, {}, {.work = budget});
        auto& w = f.world;
        REQUIRE(w.players().find(sim::PlayerId{0})->permissions.empty());
        const auto tick = w.advance(500);
        const auto* p = w.players().find(sim::PlayerId{0});
        if (budget == 1) {
            CHECK(tick == 0);
            CHECK(w.faulted());
            CHECK(p->permissions.empty());
            CHECK(p->flags == 1);
            CHECK(w.timers().counter[1] == 0);
        } else {
            CHECK(tick == 250);
            CHECK_FALSE(w.faulted());
            REQUIRE(p->permissions.size() == 1);
            CHECK(p->permissions[0].value == 1);
            CHECK(p->flags == 9);
            CHECK(w.timers().counter[1] == 3500);
        }
    }
}

TEST_CASE("SCRIPT1 explicit startup is a construction barrier before world advancement") {
    auto f = script_test::make(
        false, "INITBLOCK(){" + script_test::permission("ME", sim::TypeId{90001}, true) + "}");
    auto& w = f.world;
    const auto cmd = build(0, kCity0, {at(f.types.weaver, 120, 112)});
    const auto before = w.state_hash();
    CHECK(w.apply(cmd).status == sim::Reject::PermissionContext);
    CHECK(w.state_hash() == before);
    REQUIRE(w.initialize_scripts().empty());
    CHECK(w.clock().ms == 0);
    CHECK(w.construction_eligibility(sim::PlayerId{0}, f.types.weaver).allowed());
    REQUIRE(w.apply(cmd).ok());
}

TEST_CASE(
    "SCRIPT1 original-data component fixtures retain complete refusal and never grant builds") {
    auto types = command_test::build_types();
    auto spec = command_test::two_city_spec(types);
    spec.script_context = sim::ScriptContext::Pending;
    spec.script_source = script_test::source("INITBLOCK(){ MONEY_ADD(\"ME\",1); }");
    auto original = sim::World::create(spec, types.registry);
    REQUIRE_FALSE(original);
    auto component = original_component::import_only(spec, types.registry);
    REQUIRE(component);
    CHECK(component.value().isolated);
    CHECK(component.value().diagnostic == original.error());
    CHECK(component.value().spec.script_context == sim::ScriptContext::Missing);
    CHECK(component.value().spec.script_source.empty());
    auto made = sim::World::create(component.value().spec, types.registry);
    REQUIRE(made);
    auto& w = made.value();
    REQUIRE(w.building_at(kCity0, 110, 110) != nullptr);
    const auto hash = w.state_hash();
    CHECK(w.apply(build(0, kCity0, {at(types.weaver, 120, 112)})).status ==
          sim::Reject::PermissionContext);
    CHECK(w.state_hash() == hash);
    spec.script_source = script_test::source("");
    auto retained = original_component::import_only(spec, types.registry);
    REQUIRE(retained);
    CHECK_FALSE(retained.value().isolated);
    CHECK(retained.value().spec.script_source == spec.script_source);
    CHECK(retained.value().spec.script_context == sim::ScriptContext::Pending);
}
