// MAP-INTEGRATE-1 I3. Explicitly authored mechanics world using shipped type data.
// Terrain, stocks, actors, fields and program are fixture inputs, not unchanged endless01.
#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/placement.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/transport.hpp>

#include "command_helpers.hpp"
#include "island7_town.hpp"
#include "world_access.hpp"
namespace sim = anno::sim;
namespace {
constexpr sim::CityRef town{sim::IslandId{0}, sim::CityNo{0}};
constexpr sim::CityRef other_town{sim::IslandId{1}, sim::CityNo{0}};
constexpr sim::TypeId farm_type{809}, target_type{1001};
constexpr sim::BuildingId farm_id{1};
std::string program() {
    return "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" 0\n\"OTHER\" 1\n"
           "[SCRIPT_KERNEL]\nSET SEEN = FALSE;\nINITBLOCK(){ SEEN = TRUE; MONEY_ADD(\"ME\",7); "
           "PLAYWAVE(\"A.WAV\"); SETCONSTRUCTION_DRAWSTATE(\"ME\",\"1001\",FALSE); "
           "SETCONSTRUCTION_DRAWSTATE(\"OTHER\",\"1001\",TRUE); }\n"
           "IF PLAYTIME == 1 THEN { SETCONSTRUCTION_DRAWSTATE(\"ME\",\"1001\",TRUE); }\n"
           "IF PLAYTIME > 1 THEN { SETCONSTRUCTION_DRAWSTATE(\"ME\",\"1001\",FALSE); }\n"
           "[SIGNALS]\n[CAMARAPOS]\n0,0\n";
}
sim::World authored(const tutorial_town::Original& o, bool hygiene = false) {
    auto spec = sim::synthetic::empty_scenario(77);
    for (unsigned owner = 0; owner < 2; ++owner) {
        sim::ScenarioSpec::PlayerSpec player;
        player.id = sim::PlayerId{owner};
        player.money = 10000;
        spec.players.push_back(player);
        auto island =
            sim::synthetic::plain_island(owner, 100 + 100 * static_cast<int>(owner), 100, 32, 32);
        island.owner_runs = {{0, 1024}};
        sim::synthetic::give_flat_terrain(island, 0x30);
        auto city = sim::synthetic::plain_city(0, owner);
        city.stock = std::vector<std::uint32_t>(o.cfg.warehouse_goods, 50 * sim::kTon);
        island.cities.push_back(std::move(city));
        spec.islands.push_back(std::move(island));
    }
    sim::ScenarioSpec::BuildingSpec farm;
    farm.type = farm_type;
    farm.x = 110;
    farm.y = 110;
    farm.payload = sim::make_small_farm(o.types->find(farm_type)->prod);
    spec.islands[0].cities[0].buildings.push_back(farm);
    // Three authored ripe potato fields and grass, with original resource types/production.
    for (auto [x, y, type] : std::array<std::array<unsigned, 3>, 4>{
             {{113, 111, 709}, {113, 112, 709}, {111, 113, 709}, {114, 112, 2}}})
        spec.islands[0].objects.push_back({(y - 100) * 32 + x - 100, type, 0});
    sim::ScenarioSpec::FigureSpec ship;
    ship.kind = 1503;
    ship.owner = sim::PlayerId{0};
    ship.pos = {2688, 2688};
    ship.dir16 = 4;
    ship.health = 12000;
    ship.state_word = 0x8000;
    ship.registry_slot = 37;
    ship.holds = {{41, 12801}, {42, 512}};
    ship.nav_position_q32 =
        std::array<std::int64_t, 2>{(std::int64_t{10} << 32) + (std::int64_t{1} << 31) + (1 << 20),
                                    (std::int64_t{10} << 32) + (std::int64_t{1} << 31) + (1 << 20)};
    auto crew = ship;
    crew.kind = 717;
    crew.parent = 0;
    crew.crew_slot = 0;
    crew.registry_slot = 2;
    crew.health.reset();
    crew.nav_position_q32.reset();
    crew.hp = 350;
    crew.holds = {{26, 128}};
    crew.state_word = 0x88;
    auto donkey = crew;
    donkey.kind = 718;
    donkey.parent = 1;
    donkey.crew_slot.reset();
    donkey.registry_slot = 7;
    donkey.hp = 400;
    donkey.holds = {{41, 1}};
    spec.figures = {ship, crew, donkey};
    spec.original_figure_slots = 100;
    spec.script_context = sim::ScriptContext::Pending;
    spec.script_source = program();
    spec.script_asset_namespace = "authored/joint";
    sim::Rules rules;
    rules.forestry_claim_hygiene = hygiene;
    auto w = sim::World::create(spec, o.types, o.cfg, rules);
    REQUIRE_MESSAGE(w, (w ? "" : w.error()));
    return std::move(w).value();
}
const sim::SmallFarm& farm(const sim::World& w) {
    const auto* b = w.islands()[0]->find_building(farm_id);
    REQUIRE(b);
    return std::get<sim::SmallFarm>(b->payload);
}
const sim::Figure* worker(const sim::World& w) {
    for (const auto& slot : w.figures().list.slots)
        if (slot && slot->home == farm_id && slot->job == 0x2a && !slot->expiry_stamp)
            return &*slot;
    return nullptr;
}
bool worker_is_cut(const sim::World& w) {
    const auto* current = worker(w);
    if (!current)
        return false;
    REQUIRE(current->harvest);
    return current->harvest->step == sim::HarvestStep::Cut;
}
const sim::Player& player(const sim::World& w, sim::PlayerId id) {
    const auto* current = w.players().find(id);
    REQUIRE(current != nullptr);
    return *current;
}
void actors_intact(const sim::World& w) {
    REQUIRE(w.figures().list.slots[37]);
    REQUIRE(w.figures().list.slots[2]);
    REQUIRE(w.figures().list.slots[7]);
    const auto& ship = *w.figures().list.slots[37];
    const auto& crew = *w.figures().list.slots[2];
    const auto& donkey = *w.figures().list.slots[7];
    CHECK(ship.id == sim::FigureId{1});
    CHECK(crew.id == sim::FigureId{2});
    CHECK(donkey.id == sim::FigureId{3});
    CHECK(crew.parent == ship.id);
    CHECK(crew.crew_slot == 0);
    CHECK(donkey.parent == crew.id);
    CHECK(ship.holds == std::vector<sim::ShipHold>{{41, 12801}, {42, 512}});
    CHECK(crew.holds == std::vector<sim::ShipHold>{{26, 128}});
    CHECK(donkey.holds == std::vector<sim::ShipHold>{{41, 1}});
    REQUIRE(ship.nav);
    CHECK(ship.nav->health == 12000);
    CHECK(ship.state_word == 0x8000);
    CHECK(ship.nav->pos_u[0] ==
          ((std::int64_t{10} << 32) + (std::int64_t{1} << 31) + (1 << 20)) * 500);
}
void same_step(sim::World& a, sim::World& b, std::uint32_t ms) {
    a.advance(ms);
    b.advance(ms);
    REQUIRE_FALSE(a.faulted());
    REQUIRE_FALSE(b.faulted());
    CHECK(a.last_output().rng_cursor_before == b.last_output().rng_cursor_before);
    CHECK(a.last_output().rng_cursor_after == b.last_output().rng_cursor_after);
    REQUIRE(a.last_output().draws.size() == b.last_output().draws.size());
    for (std::size_t i = 0; i < a.last_output().draws.size(); ++i) {
        CHECK(a.last_output().draws[i].site == b.last_output().draws[i].site);
        CHECK(a.last_output().draws[i].count == b.last_output().draws[i].count);
    }
    CHECK(a.drain_events() == b.drain_events());
    CHECK(a.rng().cursor() == b.rng().cursor());
}
} // namespace
TEST_CASE("integration joint: Pending INIT permissions and imported slots coexist with real farm "
          "dispatch") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto w = authored(*o);
    actors_intact(w);
    CHECK(w.figures().list.hint == 7);
    CHECK(w.ids().next_figure == 4);
    const auto cmd = command_test::build(0, town, {command_test::at(target_type, 120, 120)});
    auto before = w.save();
    CHECK(w.apply(cmd).status == sim::Reject::PermissionContext);
    CHECK(w.save() == before);
    (void)w.drain_events(); // the rejected command event is intentionally absent from native state
    auto pending = sim::World::load(before, o->types);
    REQUIRE(pending);
    // Both normal public session paths initialize before any subsystem advance/command.
    auto rec = sim::Recorder::start(w);
    REQUIRE(rec);
    rec.value().advance(0);
    sim::run_frame(pending.value(), nullptr, {}, 0);
    CHECK(w.save() == pending.value().save());
    CHECK(player(w, sim::PlayerId{0}).money == 10007);
    auto events = w.drain_events();
    CHECK(events == pending.value().drain_events());
    CHECK(std::count_if(events.begin(), events.end(), [](const auto& e) {
              return e.kind == sim::SimEventKind::ScriptWave;
          }) == 1);
    before = w.save();
    CHECK(w.initialize_scripts().empty());
    CHECK(w.save() == before);
    CHECK(w.drain_events().empty());
    CHECK(w.apply(cmd).status == sim::Reject::ConstructionDenied);
    CHECK(w.save() == before);
    CHECK(w.construction_eligibility(sim::PlayerId{1}, target_type).allowed());
    rec.value().advance(1000);
    rec.value().advance(0);
    REQUIRE(w.construction_eligibility(sim::PlayerId{0}, target_type).allowed());
    const auto* started_worker = worker(w);
    REQUIRE(started_worker != nullptr);
    CHECK(started_worker->id == sim::FigureId{4});
    REQUIRE(w.figures().list.slots[8]);
    CHECK(w.figures().list.slots[8]->id == sim::FigureId{4});
    CHECK(farm(w).figures_out[0] == 1);
    actors_intact(w);
    const auto money = player(w, sim::PlayerId{0}).money;
    const auto stock = w.islands()[0]->city(sim::CityNo{0})->stock.stock;
    REQUIRE(rec.value().submit(cmd).ok());
    CHECK(money - player(w, sim::PlayerId{0}).money == 300);
    const auto& after = w.islands()[0]->city(sim::CityNo{0})->stock.stock;
    CHECK(stock[41] - after[41] == 768);
    CHECK(stock[42] - after[42] == 1024);
    CHECK(w.apply(command_test::build(1, town, {command_test::at(target_type, 124, 120)})).status ==
          sim::Reject::NotCityOwner);
    auto& seam = sim::WorldTestAccess::seams(w).placement_rule;
    seam = {"authored_joint_terrain_refusal",
            +[](void*, const sim::Island&, const sim::TypeRegistry&, const sim::BuildingType&,
                const sim::PlacementRecord&) -> sim::PlacementResult {
                return {sim::Reject::TerrainRefused, 0};
            },
            nullptr};
    before = w.save();
    CHECK(w.apply(command_test::build(0, town, {command_test::at(target_type, 124, 120)})).status ==
          sim::Reject::TerrainRefused);
    CHECK(w.save() == before);
    seam = {};
    auto poor = w;
    sim::WorldTestAccess::players(poor).find(sim::PlayerId{0})->money = 0;
    before = poor.save();
    CHECK(poor.apply(command_test::build(0, town, {command_test::at(target_type, 124, 120)}))
              .status == sim::Reject::CannotAfford);
    CHECK(poor.save() == before);
    rec.value().advance(1000);
    rec.value().advance(0);
    REQUIRE_FALSE(w.construction_eligibility(sim::PlayerId{0}, target_type).allowed());
    before = w.save();
    CHECK(rec.value()
              .submit(command_test::build(0, town, {command_test::at(target_type, 124, 120)}))
              .status == sim::Reject::ConstructionDenied);
    CHECK(w.save() == before);
    REQUIRE(w.apply(command_test::build(1, other_town, {command_test::at(target_type, 220, 120)}))
                .ok());
    CHECK(w.check_invariants().empty());
    actors_intact(w);
}
TEST_CASE("integration joint: active worker Ready script nested actors and transfers continue "
          "exactly after native cut") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    for (bool hygiene : {false, true}) {
        auto w = authored(*o, hygiene);
        auto started = sim::Recorder::start(w);
        REQUIRE(started);
        sim::TransportStats pre;
        w.set_transport_stats(&pre);
        for (int i = 0; i < 2000 && !worker_is_cut(w); ++i)
            w.advance(100);
        const auto* active = worker(w);
        REQUIRE(active);
        REQUIRE(active->harvest);
        REQUIRE(active->harvest->step == sim::HarvestStep::Cut);
        CHECK(w.script().state.context == sim::ScriptContext::Ready);
        REQUIRE(w.script().state.signals.size() == 2);
        CHECK(w.script().state.signals[1] == 1);
        actors_intact(w);
        CHECK(w.check_invariants().empty());
        auto cut = sim::World::load(w.save(), o->types);
        REQUIRE_MESSAGE(cut, (cut ? "" : cut.error()));
        auto& back = cut.value();
        CHECK(back.save() == w.save());
        (void)w.drain_events();
        (void)back.drain_events();
        sim::TransportStats a, b;
        w.set_transport_stats(&a);
        back.set_transport_stats(&b);
        const auto cursor_at_cut = w.rng().cursor();
        std::map<sim::DrawSite, std::uint64_t> draws;
        for (int i = 0; i < 1200; ++i) {
            same_step(w, back, 100);
            for (const auto& d : w.last_output().draws)
                draws[d.site] += d.count;
        }
        CHECK(draws[sim::DrawSite::FarmWorkerDispatch] ==
              0); // SmallFarm uses its existing animal site only.
        // 60000 ticks / 500 gate ticks = 120 line-1 draws; no script/import draw site.
        CHECK(draws[sim::DrawSite::SmallFarmAnimal] == 120);
        CHECK(w.rng().cursor() == (cursor_at_cut + 120) % w.rng().size());
        CHECK(a.forestry.raw_delivered_88 > 0);
        CHECK(a.forestry.raw_credited_88 > 0);
        CHECK(a.forestry.raw_delivered_88 == b.forestry.raw_delivered_88);
        CHECK(a.forestry.raw_credited_88 == b.forestry.raw_credited_88);
        CHECK(a.forestry.raw_lost_88 == b.forestry.raw_lost_88);
        const auto& delivered = a.forestry.small_farms.at(farm_id);
        const auto& resumed = b.forestry.small_farms.at(farm_id);
        CHECK(delivered.consumed_88[0] > 0);
        CHECK(delivered.made_88[0] > 0);
        CHECK(delivered.consumed_88[1] > 0);
        CHECK(delivered.made_88[1] > 0);
        for (unsigned line = 0; line < 2; ++line) {
            CHECK(delivered.consumed_88[line] == resumed.consumed_88[line]);
            CHECK(delivered.made_88[line] == resumed.made_88[line]);
        }
        CHECK(a.forestry.cut_mismatches == 0);
        CHECK(b.forestry.cut_mismatches == 0);
        CHECK(farm(w).out == farm(back).out);
        CHECK(farm(w).raw == farm(back).raw);
        CHECK(w.save() == back.save());
        CHECK(w.check_invariants().empty());
        actors_intact(w);
        actors_intact(back);
    }
}
TEST_CASE("integration prerequisite: original Tutorial script permits paid farm809 rebuild before "
          "long recovery") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto c = island7::start(*o, true);
    auto& w = c->w;
    REQUIRE(w.script().state.context == sim::ScriptContext::Ready);
    REQUIRE(w.construction_eligibility(c->city->owner, farm_type).allowed());
    REQUIRE(w.apply(command_test::demolish(c->city->owner.v, c->ref, {{261, 344}})).ok());
    const auto money = w.players().find(c->city->owner)->money;
    const auto tools = c->city->stock.stock[41], wood = c->city->stock.stock[42];
    const auto build = w.apply(
        command_test::build(c->city->owner.v, c->ref, {command_test::at(farm_type, 261, 344)}));
    REQUIRE_MESSAGE(build.ok(), std::string(sim::reject_name(build.status)));
    CHECK(money - w.players().find(c->city->owner)->money == 250);
    CHECK(tools - c->city->stock.stock[41] == 512);
    CHECK(wood - c->city->stock.stock[42] == 1024);
    REQUIRE(c->isl->building_at(w.config().layout, c->city->number, 261, 344));
    CHECK(c->isl->building_at(w.config().layout, c->city->number, 261, 344)->type == farm_type);
    CHECK(w.check_invariants().empty());
}
TEST_CASE("integration joint: repeated public worker end releases once and preserves imported "
          "actors and claim policy") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    for (bool hygiene : {false, true}) {
        auto w = authored(*o, hygiene);
        REQUIRE(w.initialize_scripts().empty());
        for (int i = 0; i < 2000 && !worker_is_cut(w); ++i)
            w.advance(100);
        const auto* live = worker(w);
        REQUIRE(live);
        REQUIRE(live->harvest);
        REQUIRE(live->harvest->step == sim::HarvestStep::Cut);
        const auto id = live->id;
        const auto target = live->harvest->target;
        const auto before = farm(w).figures_out[0];
        REQUIRE(before > 0);
        // Freeze further production through normal public commands before terminating the worker.
        for (auto goods : o->types->find(farm_type)->output_goods) {
            sim::Command off;
            off.issuer = sim::PlayerId{0};
            off.body = sim::SetProductionLineCmd{{110, 110}, goods, false};
            REQUIRE(w.apply(off).ok());
        }
        struct Ender {
            sim::FigureId id;
            unsigned first = 0, repeat = 0;
        } ender{id};
        auto& seam = sim::WorldTestAccess::seams(w).figure_tick;
        seam = {"authored_joint_public_end",
                +[](void* ctx, const sim::TickContext& tick, sim::SerialWorld& sw, sim::Figure& f) {
                    auto& e = *static_cast<Ender*>(ctx);
                    if (f.id != e.id || f.expiry_stamp)
                        return;
                    e.first += sw.end_figure(f, tick.t) ? 1u : 0u;
                    e.repeat += sw.end_figure(f, tick.t) ? 1u : 0u;
                },
                &ender};
        for (int i = 0; i < 200 && ender.first == 0; ++i)
            w.advance(10);
        REQUIRE(ender.first == 1);
        CHECK(ender.repeat == 0);
        CHECK(farm(w).figures_out[0] == before - 1);
        const auto* ended = sim::find_figure(w.figures(), id);
        REQUIRE(ended);
        REQUIRE(ended->expiry_stamp);
        REQUIRE(ended->harvest);
        CHECK(ended->harvest->step == sim::HarvestStep::Done);
        CHECK_FALSE(ended->home.valid());
        const auto& island = *w.islands()[0];
        CHECK(((island.tiles.aux(island.tile_index(target.x, target.y)) & sim::kTileClaim) != 0) ==
              !hygiene);
        if (hygiene)
            CHECK_FALSE(island.claims.contains(target));
        CHECK(w.check_invariants().empty());
        actors_intact(w);
        auto loaded = sim::World::load(w.save(), o->types, w.seams());
        REQUIRE(loaded);
        CHECK(loaded.value().save() == w.save());
        for (int i = 0; i < 20; ++i)
            w.advance(10);
        CHECK(farm(w).figures_out[0] == before - 1);
        CHECK(ender.first == 1);
        CHECK(ender.repeat == 0);
        CHECK(w.check_invariants().empty());
        actors_intact(w);
    }
}
TEST_CASE("integration original components: strict actor refusal and explicit component projection "
          "stay separate") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto file = o->dir / "Scenes/0200Single/Single_24.sze";
    const auto normal =
        anno::sim_load::load_scenario_file(file, o->dir, o->templates, *o->types, o->cfg, {});
    REQUIRE_FALSE(normal);
    CHECK(normal.error().find("unsupported") != std::string::npos);
    CHECK(normal.error().find("players[") != std::string::npos);
    // This second fixture explicitly declares projection before parsing; the normal result
    // above is never reused as a fallback. Preserve the complete source in this imported spec.
    const auto projected = anno::sim_load::load_scenario_file(
        file, o->dir, o->templates, *o->types, o->cfg, {}, original_component::actor_free_units());
    REQUIRE_MESSAGE(projected, (projected ? "" : projected.error()));
    CHECK(projected.value().loaded.spec.figures.empty());
    const auto component =
        original_component::import_only(projected.value().loaded.spec, o->types, o->cfg);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    if (component.value().isolated) {
        const auto refused = sim::World::create(projected.value().loaded.spec, o->types, o->cfg);
        REQUIRE_FALSE(refused);
        CHECK_FALSE(component.value().diagnostic.empty());
        CHECK(component.value().spec.script_context == sim::ScriptContext::Missing);
    } else
        CHECK(component.value().spec.script_source == projected.value().loaded.spec.script_source);
}
