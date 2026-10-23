// The vertical slice of stage S7 (docs/sim/state.md §15, §14.3): the driver
// (tick_building, consume), the effect merge (spawns, releases, money, goods traffic,
// timed events, UI events), the per-player seams bound to the player table, the
// views, the figure registry and the golden G1 (a weaver and a farm through the real
// scheduler). Expected values are derived by hand from the spec's formulas.
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "slice_helpers.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using namespace slice_test;
using building_test::Kit;
using building_test::make_kit;
using building_test::place;

namespace {

std::string num(std::uint64_t v) {
    return std::to_string(v);
}

bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

/// A scripted seam: each call for the chosen (island, city) hands out the next
/// prepared Effects buffer (and none after the script).
struct Script {
    std::vector<sim::Effects> calls;
    std::size_t next = 0;
    std::uint32_t island = 0, city = 0;
};

void run_script(void* ctx, const sim::TickContext&, sim::Island& is, sim::City& c,
                sim::Effects& fx) {
    auto& s = *static_cast<Script*>(ctx);
    if (is.index.v != s.island || c.number.v != s.city) {
        return;
    }
    if (s.next < s.calls.size()) {
        fx = s.calls[s.next++];
    }
}

sim::Seams script_seams(Script* s) {
    sim::Seams seams = named_seams();
    seams.city_pass[0].name = "script";
    seams.city_pass[0].ctx = s;
    seams.city_pass[0].fn = &run_script;
    return seams;
}

/// The kit scenario (island 0 with the cities 0 and 1 of player 0, island 3 with the
/// city 1 of player 1) with one building of every payload kind in city 0 of island 0
/// and a weaver in city 1 of island 3. Serials on island 0: weaver 1, farm 2, small
/// farm 3, bow maker 4, iron mine 5, kontor 6; the weaver of island 3 is serial 1.
sim::ScenarioSpec merge_scenario(const Kit& k) {
    sim::ScenarioSpec spec = building_test::kit_scenario();
    spec.islands[0].cities[0].buildings = {
        place(k.weaver, 101, 101),    place(k.farm, 103, 101),      place(k.small_farm, 105, 101),
        place(k.bow_maker, 107, 101), place(k.iron_mine, 110, 118), place(k.kontor, 112, 101)};
    spec.islands[1].cities[0].buildings = {place(k.weaver, 305, 205)};
    return spec;
}

sim::BuildingId bid(std::uint32_t island, std::uint32_t serial) {
    return sim::make_building_id(sim::IslandId{island}, serial);
}

sim::CityRef cref(std::uint32_t island, std::uint32_t city) {
    return sim::CityRef{sim::IslandId{island}, sim::CityNo{city}};
}

sim::Building& building(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    sim::Island& is = *sim::WorldTestAccess::islands(w)[island];
    sim::Building* b = sim::find_building_mut(is, bid(island, serial));
    REQUIRE(b != nullptr);
    return *b;
}

template <class P> P& payload(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    return std::get<P>(building(w, island, serial).payload);
}

sim::SpawnRequest spawn(sim::BuildingId home, std::uint32_t kind, std::uint32_t job = 0,
                        std::uint32_t home_counter = 0, std::uint32_t owner = 0) {
    sim::SpawnRequest s;
    s.home = home;
    s.city = cref(0, 0);
    s.home_counter = home_counter;
    s.job = job;
    s.figure_kind = kind;
    s.owner = sim::PlayerId{owner};
    s.pos = sim::FixedPos{256 * 101 + 7, 256 * 102};
    return s;
}

/// One advance of 100 ms (50 ticks): the ring makes a full lap, so every city is
/// visited exactly once and a seam for one city is called exactly once.
void lap(sim::World& w) {
    w.advance(100);
}

std::uint64_t hash_events(const std::vector<sim::SimEvent>& events) {
    sim::StateHasher h;
    for (const sim::SimEvent& e : events) {
        h.u8(static_cast<std::uint8_t>(e.kind));
        h.u64(e.tick);
        h.u32(e.player.v);
        h.u64(e.building.v);
        h.u64(e.figure.v);
        h.u32(e.code);
        h.u32(static_cast<std::uint32_t>(e.x));
        h.u32(static_cast<std::uint32_t>(e.y));
    }
    return h.value();
}

} // namespace

// --------------------------------------------------------------- inert classes

// Stage S7 also tested its temporary mask here (the weapons workshop, fur trapper, mine
// and small farm did not tick); stage S11 removed the mask and drives those classes
// (driver_classes_test.cpp). What stays: a class without a ported tick never ticks.
TEST_CASE("inert classes do not tick even with an open gate") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    // Open the gate: last = 0xF000 makes the 16-bit difference at tick 500 at least 500.
    payload<sim::Inert>(w, 0, 6).last = 0xF000;
    sim::Effects fx;
    Tick t(w, 500);
    sim::City& c = city(w, 0);
    sim::tick_building(t.ctx, island(w), c, building(w, 0, 6), fx);
    CHECK(payload<sim::Inert>(w, 0, 6).last == 0xF000);
    CHECK(fx.ui.empty());
    CHECK(fx.spawns.empty());
    // The same through the scheduler: a full lap at tick 500 leaves the kontor alone.
    sim::Effects fx2;
    c.index.stamp = 0;
    Tick t2(w, 500);
    sim::building_scheduler(t2.ctx, island(w), c, fx2);
    CHECK(payload<sim::Inert>(w, 0, 6).last == 0xF000);
}

// ---------------------------------------------------------------- tick_building

TEST_CASE("tick_building steps a workshop and raises its carrier counter") {
    const Kit k = kit_with_figure_ids(11, 22);
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    sim::Effects fx;
    Tick t(w, 500);
    sim::City& c = city(w, 0);
    sim::Building& b = building(w, 0, 1);
    std::get<sim::Workshop>(b.payload).in[0] = 5 * sim::kTon;
    sim::tick_building(t.ctx, island(w), c, b, fx);
    const auto& wk = std::get<sim::Workshop>(b.payload);
    CHECK(wk.last == 500);
    CHECK(wk.out[0] == 6);
    CHECK(wk.in[0] == 5 * sim::kTon - 11);
    CHECK(wk.carriers_out == 1);
    // One spawn: the weaver's first figure id, the carrier job 0x33 (D 0x100680b0), counter 0,
    // owner = the city's.
    REQUIRE(fx.spawns.size() == 1);
    const sim::SpawnRequest& s = fx.spawns[0];
    CHECK(s.home == b.id);
    CHECK(s.city == cref(0, 0));
    CHECK(s.home_counter == 0);
    CHECK(s.job == 0x33);
    CHECK(s.figure_kind == 11);
    CHECK(s.owner == sim::PlayerId{0});
    CHECK(s.pos.x == 101 * 256);
    CHECK(s.pos.y == 101 * 256);
    CHECK(fx.ui.empty());
    // The gate is closed at tick 600: nothing at all.
    sim::Effects fx2;
    Tick t2(w, 600);
    sim::tick_building(t2.ctx, island(w), c, b, fx2);
    CHECK(wk.last == 500);
    CHECK(wk.out[0] == 6);
    CHECK(fx2.spawns.empty());
}

TEST_CASE("tick_building steps a farm and a farm without raw material sends a worker") {
    const Kit k = kit_with_figure_ids(11, 22);
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    sim::City& c = city(w, 0);
    sim::Building& b = building(w, 0, 2);
    auto& farm = std::get<sim::Farm>(b.payload);
    farm.raw = 5 * sim::kTon;
    {
        sim::Effects fx;
        Tick t(w, 500);
        sim::tick_building(t.ctx, island(w), c, b, fx);
        CHECK(farm.last == 500);
        CHECK(farm.out == 37);
        CHECK(farm.raw == 5 * sim::kTon - 80);
        CHECK(farm.workers_out == 0); // it made something: no worker
        CHECK(fx.spawns.empty());
    }
    farm.raw = 0;
    {
        // No raw material: ratio 0, "missing input", blocked (a change), and a worker is
        // sent (384 / 37 >= 0 / 80).
        sim::Effects fx;
        Tick t(w, 1000);
        sim::tick_building(t.ctx, island(w), c, b, fx);
        CHECK(farm.last == 1000);
        CHECK(farm.workers_out == 1);
        CHECK(farm.blocked);
        REQUIRE(fx.spawns.size() == 1);
        CHECK(fx.spawns[0].figure_kind == 22);
        CHECK(fx.spawns[0].home == b.id);
        CHECK(fx.spawns[0].pos.x == 103 * 256);
        // The blocked flag changed: a BlockedChanged event (code 1) at the building's tile.
        REQUIRE(fx.ui.size() == 1);
        CHECK(fx.ui[0].kind == sim::SimEventKind::BlockedChanged);
        CHECK(fx.ui[0].code == 1);
        CHECK(fx.ui[0].building == b.id);
        CHECK(fx.ui[0].player == sim::PlayerId{0});
        CHECK(fx.ui[0].tick == 1000);
        CHECK(fx.ui[0].x == 103);
        CHECK(fx.ui[0].y == 101);
    }
}

TEST_CASE("tick_building refreshes the status icons from the owner's Volk") {
    const Kit k = make_kit();
    for (const std::uint32_t volk : {1U, 2U}) {
        INFO("volk " << volk);
        sim::ScenarioSpec spec = merge_scenario(k);
        spec.players[0].volk = volk;
        sim::World w = make_world_with(k, spec, named_seams());
        sim::Building& b = building(w, 0, 1);
        auto& wk = std::get<sim::Workshop>(b.payload);
        // No wool; the status has been "missing input" long enough for a message at
        // tick 50000 (repeat 45000): the icon is set only for Volk 1.
        wk.status.code = sim::Status::MissingInput;
        wk.status.time = static_cast<std::uint16_t>(50000 - 45000);
        wk.status.shows_icons = volk != 1; // the driver must overwrite it
        wk.last = static_cast<std::uint16_t>(50000 - 500);
        sim::Effects fx;
        Tick t(w, 50000);
        sim::tick_building(t.ctx, island(w), city(w, 0), b, fx);
        CHECK(wk.status.shows_icons == (volk == 1));
        CHECK(wk.status.icons == (volk == 1 ? 2 : 0));
        // The message is due either way (UI event StatusMessage MissingInput).
        REQUIRE_FALSE(fx.ui.empty());
        bool message = false;
        for (const sim::SimEvent& e : fx.ui) {
            message = message || (e.kind == sim::SimEventKind::StatusMessage &&
                                  e.code == static_cast<std::uint32_t>(sim::Status::MissingInput));
        }
        CHECK(message);
    }
    // The farm refreshes them the same way (repeat 60000 for "missing input").
    for (const std::uint32_t volk : {1U, 2U}) {
        INFO("farm volk " << volk);
        sim::ScenarioSpec spec = merge_scenario(k);
        spec.players[0].volk = volk;
        sim::World w = make_world_with(k, spec, named_seams());
        auto& farm = std::get<sim::Farm>(building(w, 0, 2).payload);
        farm.raw = 0;
        farm.status.code = sim::Status::MissingInput;
        farm.status.time = static_cast<std::uint16_t>(50000 - 60000);
        farm.status.shows_icons = volk != 1;
        farm.last = static_cast<std::uint16_t>(50000 - 500);
        sim::Effects fx;
        Tick t(w, 50000);
        sim::tick_building(t.ctx, island(w), city(w, 0), building(w, 0, 2), fx);
        CHECK(farm.status.shows_icons == (volk == 1));
        CHECK(farm.status.icons == (volk == 1 ? 2 : 0));
    }
}

TEST_CASE("tick_building with a mismatched payload returns without throwing") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    sim::City& c = city(w, 0);
    // A weaver (class Workshop) that holds a Farm payload: a corrupt copy.
    sim::Building& weaver = building(w, 0, 1);
    sim::Farm farm;
    farm.last = 0;
    farm.raw = 4;
    weaver.payload = farm;
    sim::Effects fx;
    Tick t(w, 500);
    sim::tick_building(t.ctx, island(w), c, weaver, fx);
    CHECK(std::get<sim::Farm>(weaver.payload).last == 0);
    CHECK(std::get<sim::Farm>(weaver.payload).raw == 4);
    // A farm that holds a Workshop.
    sim::Building& f = building(w, 0, 2);
    f.payload = sim::make_workshop(k.registry->find(k.weaver)->prod);
    sim::tick_building(t.ctx, island(w), c, f, fx);
    CHECK(std::get<sim::Workshop>(f.payload).last == 0);
    // An unknown type id, and a payload of no alternative the class wants.
    sim::Building& g = building(w, 0, 6);
    g.type = sim::TypeId{424242};
    sim::tick_building(t.ctx, island(w), c, g, fx);
    CHECK(fx.ui.empty());
    CHECK(fx.spawns.empty());
}

// --------------------------------------------------------------------- consume

namespace {

struct ConsumeKit {
    Kit k = kit_with_figure_ids(11, 22);
    sim::World w;
    ConsumeKit() : w(make_world_with(k, merge_scenario(k), named_seams())) {}
};

sim::StepResult result_with(bool blocked_changed, bool message, sim::Status status,
                            std::uint16_t notice) {
    sim::StepResult r;
    r.blocked_changed = blocked_changed;
    r.message = message;
    r.status = status;
    r.notice = notice;
    return r;
}

} // namespace

TEST_CASE("consume turns a result into UI events in the order blocked, status, notice") {
    ConsumeKit ck;
    sim::World& w = ck.w;
    sim::Building& b = building(w, 0, 1);
    const sim::BuildingType& type = *ck.k.registry->find(ck.k.weaver);
    Tick t(w, 321);
    sim::Effects fx;
    std::get<sim::Workshop>(b.payload).blocked = true;
    sim::consume(t.ctx, island(w), city(w, 0), b, type,
                 result_with(true, true, sim::Status::StorageFull, 0x19), fx);
    REQUIRE(fx.ui.size() == 3);
    CHECK(fx.ui[0].kind == sim::SimEventKind::BlockedChanged);
    CHECK(fx.ui[0].code == 1);
    CHECK(fx.ui[0].x == 101);
    CHECK(fx.ui[0].y == 101);
    CHECK(fx.ui[1].kind == sim::SimEventKind::StatusMessage);
    CHECK(fx.ui[1].code == static_cast<std::uint32_t>(sim::Status::StorageFull));
    CHECK(fx.ui[2].kind == sim::SimEventKind::Notice);
    CHECK(fx.ui[2].code == 0x19);
    for (const sim::SimEvent& e : fx.ui) {
        CHECK(e.player == sim::PlayerId{0});
        CHECK(e.building == b.id);
        CHECK(e.tick == 321);
    }
    // A cleared flag is code 0.
    std::get<sim::Workshop>(b.payload).blocked = false;
    sim::Effects fx2;
    sim::consume(t.ctx, island(w), city(w, 0), b, type,
                 result_with(true, false, sim::Status::None, 0), fx2);
    REQUIRE(fx2.ui.size() == 1);
    CHECK(fx2.ui[0].code == 0);
}

TEST_CASE("consume raises a message only when the step asked for one") {
    ConsumeKit ck;
    sim::World& w = ck.w;
    sim::Building& b = building(w, 0, 1);
    const sim::BuildingType& type = *ck.k.registry->find(ck.k.weaver);
    Tick t(w, 5);
    sim::Effects fx;
    // A status without the message flag, and no notice: nothing.
    sim::consume(t.ctx, island(w), city(w, 0), b, type,
                 result_with(false, false, sim::Status::MissingInput, 0), fx);
    CHECK(fx.ui.empty());
    CHECK(fx.spawns.empty());
    // made output changes nothing here.
    sim::StepResult r;
    r.made = {6, 7};
    sim::consume(t.ctx, island(w), city(w, 0), b, type, r, fx);
    CHECK(fx.ui.empty());
    // A message with status MissingInput carries code 1; a notice alone is only the notice.
    sim::consume(t.ctx, island(w), city(w, 0), b, type,
                 result_with(false, true, sim::Status::MissingInput, 0), fx);
    REQUIRE(fx.ui.size() == 1);
    CHECK(fx.ui[0].kind == sim::SimEventKind::StatusMessage);
    CHECK(fx.ui[0].code == 1);
    sim::Effects fx2;
    sim::consume(t.ctx, island(w), city(w, 0), b, type,
                 result_with(false, false, sim::Status::None, 0x15b), fx2);
    REQUIRE(fx2.ui.size() == 1);
    CHECK(fx2.ui[0].kind == sim::SimEventKind::Notice);
    CHECK(fx2.ui[0].code == 0x15b);
}

TEST_CASE("consume sends a figure by raising the home counter and queueing a spawn") {
    ConsumeKit ck;
    sim::World& w = ck.w;
    Tick t(w, 77);
    sim::StepResult send;
    send.send_figure = true;
    // weaver 1 (kind 11, carrier job 0x33), farm 2 (kind 22, job unknown: 0); the fur trapper
    // job is 0x0d.
    {
        sim::Effects fx;
        sim::Building& b = building(w, 0, 1);
        sim::consume(t.ctx, island(w), city(w, 0), b, *ck.k.registry->find(ck.k.weaver), send, fx);
        CHECK(std::get<sim::Workshop>(b.payload).carriers_out == 1);
        REQUIRE(fx.spawns.size() == 1);
        CHECK(fx.spawns[0].figure_kind == 11);
        CHECK(fx.spawns[0].job == 0x33);
        CHECK(fx.spawns[0].home_counter == 0);
    }
    {
        sim::Effects fx;
        sim::Building& b = building(w, 0, 2);
        sim::consume(t.ctx, island(w), city(w, 0), b, *ck.k.registry->find(ck.k.farm), send, fx);
        CHECK(std::get<sim::Farm>(b.payload).workers_out == 1);
        REQUIRE(fx.spawns.size() == 1);
        CHECK(fx.spawns[0].figure_kind == 22);
        CHECK(fx.spawns[0].job == 0);
    }
    {
        // A weapons workshop raises carriers_out, a small farm its counter 0.
        sim::Effects fx;
        sim::Building& wb = building(w, 0, 4);
        sim::consume(t.ctx, island(w), city(w, 0), wb, *ck.k.registry->find(ck.k.bow_maker), send,
                     fx);
        CHECK(std::get<sim::WeaponsWorkshop>(wb.payload).carriers_out == 1);
        sim::Building& sb = building(w, 0, 3);
        sim::consume(t.ctx, island(w), city(w, 0), sb, *ck.k.registry->find(ck.k.small_farm), send,
                     fx);
        CHECK(std::get<sim::SmallFarm>(sb.payload).figures_out[0] == 1);
        REQUIRE(fx.spawns.size() == 2);
        CHECK(fx.spawns[0].job == 0x33); // the weapons workshop's carrier
        CHECK(fx.spawns[1].job == 0x2a); // the small farm's line-0 worker
    }
    {
        // The fur trapper (class FurTrapper, a Farm payload) sends job 0x0d. Its payload is
        // a Farm, so put one into the weaver's building to have a Farm payload at hand.
        sim::Effects fx;
        sim::Building& b = building(w, 0, 2);
        std::get<sim::Farm>(b.payload).workers_out = 0;
        sim::consume(t.ctx, island(w), city(w, 0), b, *ck.k.registry->find(ck.k.fur_trapper), send,
                     fx);
        CHECK(std::get<sim::Farm>(b.payload).workers_out == 1);
        REQUIRE(fx.spawns.size() == 1);
        CHECK(fx.spawns[0].job == 0x0d);
    }
    {
        // A payload without a counter (a mine, an inert building) sends nothing.
        sim::Effects fx;
        sim::Building& mine = building(w, 0, 5);
        sim::consume(t.ctx, island(w), city(w, 0), mine, *ck.k.registry->find(ck.k.iron_mine), send,
                     fx);
        sim::Building& kontor = building(w, 0, 6);
        sim::consume(t.ctx, island(w), city(w, 0), kontor, *ck.k.registry->find(ck.k.kontor), send,
                     fx);
        CHECK(fx.spawns.empty());
    }
}

TEST_CASE("consume sends a small farm animal by kind and refuses kinds outside the counters") {
    ConsumeKit ck;
    sim::World& w = ck.w;
    Tick t(w, 9);
    sim::Building& b = building(w, 0, 3);
    const sim::BuildingType& type = *ck.k.registry->find(ck.k.small_farm);
    auto& farm = std::get<sim::SmallFarm>(b.payload);
    REQUIRE(farm.figures_out.size() == 5);
    struct Row {
        std::uint16_t kind;
        int counter; // the counter that rises, -1 none
    };
    for (const Row& row : std::vector<Row>{{106, 0}, {107, 1}, {110, 4}, {105, -1}, {111, -1}}) {
        INFO("kind " << row.kind);
        farm.figures_out.assign(5, 0);
        sim::StepResult r;
        r.send_animal = true;
        r.animal_kind = row.kind;
        sim::Effects fx;
        sim::consume(t.ctx, island(w), city(w, 0), b, type, r, fx);
        for (int i = 0; i < 5; ++i) {
            CHECK(farm.figures_out[static_cast<std::size_t>(i)] == (i == row.counter ? 1U : 0U));
        }
        if (row.counter < 0) {
            CHECK(fx.spawns.empty());
        } else {
            REQUIRE(fx.spawns.size() == 1);
            CHECK(fx.spawns[0].job == 0x2e);
            CHECK(fx.spawns[0].figure_kind == row.kind);
            CHECK(fx.spawns[0].home_counter == static_cast<std::uint32_t>(row.counter));
            CHECK(fx.spawns[0].home == b.id);
        }
    }
    // The animal counter is one of the small farm: another payload sends nothing.
    sim::Building& weaver = building(w, 0, 1);
    sim::StepResult r;
    r.send_animal = true;
    r.animal_kind = 106;
    sim::Effects fx;
    sim::consume(t.ctx, island(w), city(w, 0), weaver, *ck.k.registry->find(ck.k.weaver), r, fx);
    CHECK(std::get<sim::Workshop>(weaver.payload).carriers_out == 0);
    CHECK(fx.spawns.empty());
    // A modded base moves the counters.
    sim::SimConfig cfg;
    cfg.small_farm_animals.base = 100;
    cfg.small_farm_animals.kind_by_draw = {101, 102};
    cfg.small_farm_animals.limit = {1, 1, 1, 1, 1, 1, 1, 1};
    sim::World w2 = make_world_with(ck.k, merge_scenario(ck.k), named_seams(), cfg);
    sim::Building& b2 = building(w2, 0, 3);
    sim::StepResult r2;
    r2.send_animal = true;
    r2.animal_kind = 102;
    sim::Effects fx2;
    Tick t2(w2, 9);
    sim::consume(t2.ctx, island(w2), city(w2, 0), b2, *ck.k.registry->find(ck.k.small_farm), r2,
                 fx2);
    CHECK(std::get<sim::SmallFarm>(b2.payload).figures_out[2] == 1);
    REQUIRE(fx2.spawns.size() == 1);
    CHECK(fx2.spawns[0].home_counter == 2);
}

TEST_CASE("consume names the island, the city and the owner of the building it works on") {
    ConsumeKit ck;
    sim::World& w = ck.w;
    // The weaver of island 3 (serial 1) sits in city 1, which belongs to player 1; the tick
    // is beyond 2^32 so that the 64-bit tick of the events can be told from its low bits.
    sim::Building& b = building(w, 3, 1);
    const sim::BuildingType& type = *ck.k.registry->find(ck.k.weaver);
    const std::uint64_t tick = (std::uint64_t{1} << 32) + 321;
    Tick t(w, tick);
    sim::StepResult r = result_with(true, true, sim::Status::MissingInput, 0x15b);
    r.send_figure = true;
    sim::Effects fx;
    sim::consume(t.ctx, island(w, 3), city(w, 1, 3), b, type, r, fx);
    REQUIRE(fx.ui.size() == 3);
    for (const sim::SimEvent& e : fx.ui) {
        CHECK(e.player == sim::PlayerId{1});
        CHECK(e.building == b.id);
        CHECK(e.tick == tick);
    }
    CHECK(fx.ui[0].x == 305);
    CHECK(fx.ui[0].y == 205);
    REQUIRE(fx.spawns.size() == 1);
    CHECK(fx.spawns[0].city == cref(3, 1));
    CHECK(fx.spawns[0].owner == sim::PlayerId{1});
    CHECK(fx.spawns[0].home == b.id);
    CHECK(fx.spawns[0].pos.x == 305 * 256);
    CHECK(fx.spawns[0].pos.y == 205 * 256);
}

TEST_CASE("consume sends a figure and an animal in this order") {
    ConsumeKit ck;
    sim::World& w = ck.w;
    Tick t(w, 9);
    sim::Building& b = building(w, 0, 3);
    sim::StepResult r;
    r.send_figure = true;
    r.send_animal = true;
    r.animal_kind = 107;
    sim::Effects fx;
    sim::consume(t.ctx, island(w), city(w, 0), b, *ck.k.registry->find(ck.k.small_farm), r, fx);
    REQUIRE(fx.spawns.size() == 2);
    // The line-0 worker has job 0x2a (D 0x1006f660, docs/re/production.md §4.5).
    CHECK(fx.spawns[0].job == 0x2a);
    CHECK(fx.spawns[1].job == 0x2e);
    const auto& farm = std::get<sim::SmallFarm>(b.payload);
    CHECK(farm.figures_out[0] == 1);
    CHECK(farm.figures_out[1] == 1);
}

// ------------------------------------------------------------ counters and ids

TEST_CASE("figure counters and output stores by payload") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    // Counters: Workshop, WeaponsWorkshop, Farm have counter 0 only; the small farm five.
    for (const std::uint32_t serial : {1U, 2U, 4U}) {
        sim::Payload& p = building(w, 0, serial).payload;
        CHECK(sim::figure_counter_count(p) == 1);
        REQUIRE(sim::figure_counter(p, 0) != nullptr);
        CHECK(sim::figure_counter(p, 1) == nullptr);
        CHECK(sim::figure_counter(p, 0xFFFFFFFF) == nullptr);
    }
    sim::Payload& small = building(w, 0, 3).payload;
    CHECK(sim::figure_counter_count(small) == 5);
    for (std::uint32_t i = 0; i < 5; ++i) {
        REQUIRE(sim::figure_counter(small, i) != nullptr);
        *sim::figure_counter(small, i) = i + 1;
    }
    CHECK(sim::figure_counter(small, 5) == nullptr);
    CHECK(std::get<sim::SmallFarm>(small).figures_out == std::vector<std::uint32_t>{1, 2, 3, 4, 5});
    const sim::Payload& csmall = small;
    CHECK(*sim::figure_counter(csmall, 4) == 5);
    CHECK(sim::figure_counter(csmall, 5) == nullptr);
    // Mine and Inert have none.
    for (const std::uint32_t serial : {5U, 6U}) {
        sim::Payload& p = building(w, 0, serial).payload;
        CHECK(sim::figure_counter_count(p) == 0);
        CHECK(sim::figure_counter(p, 0) == nullptr);
    }
    // The pointers point at the right members.
    sim::Payload& wk = building(w, 0, 1).payload;
    *sim::figure_counter(wk, 0) = 9;
    CHECK(std::get<sim::Workshop>(wk).carriers_out == 9);
    sim::Payload& fm = building(w, 0, 2).payload;
    *sim::figure_counter(fm, 0) = 8;
    CHECK(std::get<sim::Farm>(fm).workers_out == 8);
    sim::Payload& ww = building(w, 0, 4).payload;
    *sim::figure_counter(ww, 0) = 7;
    CHECK(std::get<sim::WeaponsWorkshop>(ww).carriers_out == 7);
    // Output stores.
    sim::Payload& weaver = building(w, 0, 1).payload;
    REQUIRE(sim::output_store(weaver, 0) != nullptr);
    *sim::output_store(weaver, 0) = 31;
    CHECK(std::get<sim::Workshop>(weaver).out[0] == 31);
    CHECK(sim::output_store(weaver, 1) == nullptr);
    sim::Payload& bow = building(w, 0, 4).payload;
    *sim::output_store(bow, 1) = 32;
    CHECK(std::get<sim::WeaponsWorkshop>(bow).out[1] == 32);
    CHECK(sim::output_store(bow, 2) == nullptr);
    sim::Payload& farm = building(w, 0, 2).payload;
    *sim::output_store(farm, 0) = 33;
    CHECK(std::get<sim::Farm>(farm).out == 33);
    CHECK(sim::output_store(farm, 1) == nullptr);
    *sim::output_store(small, 1) = 34;
    CHECK(std::get<sim::SmallFarm>(small).out[1] == 34);
    CHECK(sim::output_store(small, 2) == nullptr);
    sim::Payload& mine = building(w, 0, 5).payload;
    *sim::output_store(mine, 0) = 35;
    CHECK(std::get<sim::Mine>(mine).out == 35);
    CHECK(sim::output_store(mine, 1) == nullptr);
    CHECK(sim::output_store(building(w, 0, 6).payload, 0) == nullptr);
    // payload_blocked reads every alternative's flag.
    for (const std::uint32_t serial : {1U, 2U, 3U, 4U, 5U, 6U}) {
        sim::Payload& p = building(w, 0, serial).payload;
        CHECK_FALSE(sim::payload_blocked(p));
        std::visit([](auto& a) { a.blocked = true; }, p);
        CHECK(sim::payload_blocked(p));
    }
}

TEST_CASE("find_building_mut finds a building by id and refuses stale ids") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    sim::Island& is = island(w, 0);
    sim::Building* b = sim::find_building_mut(is, bid(0, 2));
    REQUIRE(b != nullptr);
    CHECK(b->id == bid(0, 2));
    CHECK(b->type == k.farm);
    b->x = 103; // writable
    CHECK(sim::find_building_mut(is, bid(0, 99)) == nullptr);
    CHECK(sim::find_building_mut(is, bid(3, 1)) == nullptr); // the id of another island
    CHECK(sim::find_building_mut(is, sim::BuildingId{}) == nullptr);
    // A removed building is gone.
    REQUIRE(sim::WorldTestAccess::remove(w, {cref(0, 0), 4, 1}));
    CHECK(sim::find_building_mut(is, bid(0, 2)) == nullptr);
    // The map entry is stale: a slot that holds another building does not answer.
    is.cache.by_id[bid(0, 77)] = sim::BuildingKey{cref(0, 0), 4, 0};
    CHECK(sim::find_building_mut(is, bid(0, 77)) == nullptr);
    is.cache.by_id[bid(0, 78)] = sim::BuildingKey{cref(0, 0), 4, 5000};
    CHECK(sim::find_building_mut(is, bid(0, 78)) == nullptr);
    is.cache.by_id[bid(0, 79)] = sim::BuildingKey{cref(0, 0), 99, 0};
    CHECK(sim::find_building_mut(is, bid(0, 79)) == nullptr);
    is.cache.by_id[bid(0, 80)] = sim::BuildingKey{cref(0, 9), 4, 0};
    CHECK(sim::find_building_mut(is, bid(0, 80)) == nullptr);
}

// ------------------------------------------------------------------------ merge

TEST_CASE("merge creates figures in order with the world's ids slots and events") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 11, 0, 0, 0), spawn(bid(0, 2), 22, 5, 0, 1)};
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w); // tick 50
    const sim::FigureRegistry& reg = w.figures();
    REQUIRE(reg.list.slots[0].has_value());
    REQUIRE(reg.list.slots[1].has_value());
    CHECK(reg.list.slots.size() == 10); // 1 -> 10 with the second insert (D 0x10026a00)
    CHECK(reg.list.hint == 1);
    const sim::Figure& a = *reg.list.slots[0];
    const sim::Figure& b = *reg.list.slots[1];
    CHECK(a.id == sim::FigureId{1});
    CHECK(b.id == sim::FigureId{2});
    CHECK(a.kind == 11);
    CHECK(b.kind == 22);
    CHECK(a.owner == sim::PlayerId{0});
    CHECK(b.owner == sim::PlayerId{1});
    CHECK(a.home == bid(0, 1));
    CHECK(b.home == bid(0, 2));
    CHECK(a.job == 0);
    CHECK(b.job == 5);
    CHECK(a.home_counter == 0);
    CHECK(a.pos.x == 256 * 101 + 7);
    CHECK(a.pos.y == 256 * 102);
    CHECK(a.born == 50);
    CHECK(a.cargo_goods == 0);
    CHECK(a.cargo_88 == 0);
    CHECK(a.hp == 0);
    CHECK_FALSE(a.created_stamp.has_value());
    CHECK_FALSE(a.marked_for_delete);
    CHECK(w.ids().next_figure == 3);
    CHECK(reg.by_id.at(sim::FigureId{1}) == 0);
    CHECK(reg.by_id.at(sim::FigureId{2}) == 1);
    // One FigureSpawned event per figure, in order, with the tile of the position.
    const std::vector<sim::SimEvent> ui = w.drain_events();
    REQUIRE(ui.size() == 2);
    CHECK(ui[0].kind == sim::SimEventKind::FigureSpawned);
    CHECK(ui[0].tick == 50);
    CHECK(ui[0].player == sim::PlayerId{0});
    CHECK(ui[0].building == bid(0, 1));
    CHECK(ui[0].figure == sim::FigureId{1});
    CHECK(ui[0].code == 11);
    CHECK(ui[0].x == 101);
    CHECK(ui[0].y == 102);
    CHECK(ui[1].figure == sim::FigureId{2});
    CHECK(ui[1].player == sim::PlayerId{1});
    CHECK(ui[1].code == 22);
    // The registry itself is consistent (the home counters were not raised by this script,
    // so the counter invariant of check_invariants would object).
    CHECK(sim::validate_figures(w.figures(), w.ids()) == "");
}

TEST_CASE("merge figure ids continue across advances and the slot policy is the list's") {
    const Kit k = make_kit();
    Script script;
    sim::Effects one;
    one.spawns = {spawn(bid(0, 1), 1)};
    script.calls = {one, one, one};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w);
    lap(w);
    CHECK(w.ids().next_figure == 3);
    // Make a hole in slot 0 and move the hint to slot 1: the original policy scans from
    // the hint + 1 and takes slot 2, LowestFree takes the hole.
    sim::WorldTestAccess::figures(w).list.slots[0].reset();
    sim::WorldTestAccess::figures(w).by_id.erase(sim::FigureId{1});
    sim::World lowest = w;
    sim::WorldTestAccess::rules(lowest).slot_policy = sim::SlotPolicy::LowestFree;
    lap(w);
    script.next = 2; // the copy makes the same third call
    lap(lowest);
    REQUIRE(w.figures().list.slots[2].has_value());
    CHECK(w.figures().list.slots[2]->id == sim::FigureId{3});
    CHECK_FALSE(w.figures().list.slots[0].has_value());
    REQUIRE(lowest.figures().list.slots[0].has_value());
    CHECK(lowest.figures().list.slots[0]->id == sim::FigureId{3});
    CHECK_FALSE(lowest.figures().list.slots[2].has_value());
}

TEST_CASE("merge skips a spawn when the id space is used up or the id is taken") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 1), spawn(bid(0, 1), 2)};
    script.calls = {e, e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    // One id left (the maximum is the invalid id): the first spawn takes it, the second
    // finds the space used up and creates nothing. The used-up counter is a legal state
    // (the loader and check_invariants accept it, F35).
    sim::WorldTestAccess::ids(w).next_figure = std::numeric_limits<std::uint64_t>::max() - 1;
    lap(w);
    CHECK(w.ids().next_figure == std::numeric_limits<std::uint64_t>::max());
    CHECK(w.figures().by_id.size() == 1);
    CHECK(w.drain_events().size() == 1);
    CHECK(sim::validate_figures(w.figures(), w.ids()) == "");
    CHECK_FALSE(contains(w.check_invariants(), "ids."));
    // A counter that points at an id already in the registry creates nothing and does not
    // advance.
    sim::World taken = make_world_with(k, merge_scenario(k), script_seams(&script));
    script.next = 0;
    sim::Figure f;
    f.id = sim::FigureId{5};
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(taken), f, taken.config(),
                            taken.rules()) != sim::kNoSlot);
    sim::WorldTestAccess::ids(taken).next_figure = 5;
    lap(taken);
    CHECK(taken.figures().by_id.size() == 1);
    CHECK(taken.ids().next_figure == 5);
    CHECK(taken.drain_events().empty());
}

TEST_CASE("a refused spawn takes the raised counter back and the invariants hold") {
    // The tick raises the home counter, the merge creates the figure; when the merge
    // refuses (id space used up, id taken, list cannot grow) it must undo the raise, or
    // the building would count a carrier for ever (F35).
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 1), spawn(bid(0, 1), 2), spawn(bid(0, 3), 3, 0x2e, 2)};
    script.calls = {e, e, e};
    // 1. The id space: one id left, so the first spawn is created and the other two refused.
    {
        sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
        payload<sim::Workshop>(w, 0, 1).carriers_out = 2; // raised by the tick for both
        payload<sim::SmallFarm>(w, 0, 3).figures_out[2] = 1;
        sim::WorldTestAccess::ids(w).next_figure = std::numeric_limits<std::uint64_t>::max() - 1;
        lap(w);
        CHECK(w.figures().by_id.size() == 1);
        CHECK(payload<sim::Workshop>(w, 0, 1).carriers_out == 1);    // one figure
        CHECK(payload<sim::SmallFarm>(w, 0, 3).figures_out[2] == 0); // none
        CHECK(w.check_invariants() == "");
        // The used-up counter survives a save and a load and still refuses.
        auto loaded = sim::World::load(w.save(), k.registry, script_seams(&script));
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
        CHECK(loaded.value().ids().next_figure == std::numeric_limits<std::uint64_t>::max());
        CHECK(loaded.value().state_hash() == w.state_hash());
    }
    // 2. An id that is already in the registry.
    {
        script.next = 0;
        sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
        sim::Figure f;
        f.id = sim::FigureId{5};
        f.home = bid(0, 1);
        REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
                sim::kNoSlot);
        payload<sim::Workshop>(w, 0, 1).carriers_out = 3; // the old figure and two raises
        payload<sim::SmallFarm>(w, 0, 3).figures_out[2] = 1;
        sim::WorldTestAccess::ids(w).next_figure = 5; // a corrupt counter: it names a taken id
        lap(w);
        CHECK(w.figures().by_id.size() == 1);
        CHECK(w.ids().next_figure == 5);
        CHECK(payload<sim::Workshop>(w, 0, 1).carriers_out == 1);
        CHECK(payload<sim::SmallFarm>(w, 0, 3).figures_out[2] == 0);
    }
    // 3. A counter that is already 0 is no error, and stays 0.
    {
        script.next = 0;
        sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
        sim::WorldTestAccess::ids(w).next_figure = std::numeric_limits<std::uint64_t>::max();
        lap(w);
        CHECK(w.figures().by_id.empty());
        CHECK(payload<sim::Workshop>(w, 0, 1).carriers_out == 0);
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("merge releases lower the named counter of a building that still exists") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    auto rel = [](sim::BuildingId b, std::uint32_t counter) {
        sim::ReleaseCounter r;
        r.home = b;
        r.home_counter = counter;
        return r;
    };
    e.releases = {rel(bid(0, 1), 0),  rel(bid(0, 1), 0), rel(bid(0, 1), 0), // 2 -> 0, floor
                  rel(bid(0, 3), 4),                                        // small farm counter 4
                  rel(bid(0, 3), 1),                                        // counter 1
                  rel(bid(0, 2), 0),                                        // farm workers
                  rel(bid(0, 1), 1),                                        // no such counter
                  rel(bid(0, 6), 0),                                        // an inert building
                  rel(bid(0, 99), 0),                                       // gone
                  rel(bid(3, 1), 0),                                        // other island
                  rel(bid(0, 4), 0),                                        // weapons workshop
                  rel(bid(5, 1), 0),                                        // no island
                  rel(bid(4, 1), 0)};                                       // one past the table
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    payload<sim::Workshop>(w, 0, 1).carriers_out = 2;
    payload<sim::SmallFarm>(w, 0, 3).figures_out = {1, 2, 0, 0, 3};
    payload<sim::Farm>(w, 0, 2).workers_out = 1;
    payload<sim::Workshop>(w, 3, 1).carriers_out = 4;
    payload<sim::WeaponsWorkshop>(w, 0, 4).carriers_out = 1;
    lap(w);
    CHECK(payload<sim::Workshop>(w, 0, 1).carriers_out == 0);
    CHECK(payload<sim::SmallFarm>(w, 0, 3).figures_out ==
          std::vector<std::uint32_t>{1, 1, 0, 0, 2});
    CHECK(payload<sim::Farm>(w, 0, 2).workers_out == 0);
    CHECK(payload<sim::Workshop>(w, 3, 1).carriers_out == 3); // the island-3 release
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, 4).carriers_out == 0);
}

TEST_CASE("merge money adds in i64 and saturates at the ends") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    auto money = [](std::uint32_t p, std::int64_t d) {
        sim::MoneyDelta m;
        m.player = sim::PlayerId{p};
        m.delta = d;
        return m;
    };
    e.money = {money(0, 100), money(0, -250), money(1, 5), money(7, 50), money(0, 1)};
    sim::Effects ends;
    ends.money = {money(0, 100), money(1, -100)};
    // The exact edges: reaching the end needs no saturation, one more does.
    sim::Effects edges;
    edges.money = {money(0, 5), money(1, -6)};
    script.calls = {e, ends, edges};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w);
    CHECK(w.players().find(sim::PlayerId{0})->money == 10000 + 100 - 250 + 1);
    CHECK(w.players().find(sim::PlayerId{1})->money == 25000 + 5);
    CHECK(w.players().find(sim::PlayerId{7}) == nullptr); // an absent player stays absent
    sim::WorldTestAccess::players(w).slots[0]->money = std::numeric_limits<std::int64_t>::max() - 5;
    sim::WorldTestAccess::players(w).slots[1]->money = std::numeric_limits<std::int64_t>::min() + 5;
    lap(w);
    CHECK(w.players().find(sim::PlayerId{0})->money == std::numeric_limits<std::int64_t>::max());
    CHECK(w.players().find(sim::PlayerId{1})->money == std::numeric_limits<std::int64_t>::min());
    sim::WorldTestAccess::players(w).slots[0]->money = std::numeric_limits<std::int64_t>::max() - 5;
    sim::WorldTestAccess::players(w).slots[1]->money = std::numeric_limits<std::int64_t>::min() + 5;
    lap(w);
    CHECK(w.players().find(sim::PlayerId{0})->money == std::numeric_limits<std::int64_t>::max());
    CHECK(w.players().find(sim::PlayerId{1})->money == std::numeric_limits<std::int64_t>::min());
}

TEST_CASE("merge goods moves add and remove with the stock's clipping") {
    const Kit k = make_kit();
    Script script;
    auto mv = [](sim::CityRef c, sim::GoodsId g, std::int64_t d) {
        sim::GoodsMove m;
        m.city = c;
        m.goods = g;
        m.delta_88 = d;
        return m;
    };
    sim::Effects a;
    a.goods_moves = {mv(cref(0, 0), 9, 300), mv(cref(0, 0), 9, -100), mv(cref(0, 0), 10, -50),
                     mv(cref(0, 0), 11, 0), mv(cref(0, 1), 9, 77)};
    sim::Effects b;
    // Beyond the capacity (50 t = 12800), beyond the stock, a magnitude beyond u32, a goods
    // number outside the warehouse, a city and an island that are not there.
    b.goods_moves = {mv(cref(0, 0), 9, 20000),
                     mv(cref(0, 0), 10, -99999),
                     mv(cref(0, 0), 12, std::int64_t{1} << 40),
                     mv(cref(0, 0), 13, -(std::int64_t{1} << 40)),
                     mv(cref(0, 0), 56, 5),
                     mv(cref(0, 7), 14, 5),
                     mv(cref(9, 0), 14, 5),
                     mv(cref(3, 1), 9, 40),  // the city of another island
                     mv(cref(3, 0), 14, 6)}; // a city of that island that is not there
    script.calls = {a, b};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    city(w, 0).stock.stock[10] = 40;
    city(w, 0).stock.stock[13] = 500;
    lap(w);
    CHECK(city(w, 0).stock.stock[9] == 200);
    CHECK(city(w, 0).stock.stock[10] == 0); // 40 - 50 clipped at 0
    CHECK(city(w, 0).stock.stock[11] == 0);
    CHECK(city(w, 1).stock.stock[9] == 77); // another city of the island
    lap(w);
    CHECK(city(w, 0).stock.stock[9] == 12800); // clipped at the capacity
    CHECK(city(w, 0).stock.stock[10] == 0);
    CHECK(city(w, 0).stock.stock[12] == 12800); // 2^40 cut to 0xFFFFFFFF, clipped by the capacity
    CHECK(city(w, 0).stock.stock[13] == 0);     // the huge removal empties it
    CHECK(city(w, 0).stock.stock.size() == 56);
    CHECK(city(w, 0).stock.stock[14] == 0); // the cities that are not there changed nothing
    CHECK(city(w, 1).stock.stock[14] == 0);
    CHECK(city(w, 1, 3).stock.stock[9] == 40); // island 3, city 1
    CHECK(city(w, 1).stock.stock[9] == 77);    // not island 0's city 1
}

TEST_CASE("merge reserves move output into the cart's cargo") {
    const Kit k = make_kit();
    const sim::GoodsId cloth = k.registry->find(k.weaver)->output_goods[0];
    Script script;
    auto rs = [](sim::FigureId f, sim::BuildingId b, std::uint32_t out, std::uint32_t amount) {
        sim::Reserve r;
        r.figure = f;
        r.building = b;
        r.out_index = out;
        r.amount_88 = amount;
        return r;
    };
    sim::Effects a;
    a.reserves = {rs(sim::FigureId{1}, bid(0, 1), 0, 400), // takes 400 of the weaver's 1000
                  rs(sim::FigureId{1}, bid(0, 1), 0, 700), // takes the remaining 600
                  rs(sim::FigureId{1}, bid(0, 1), 0, 50),  // the store is empty: 0
                  rs(sim::FigureId{3}, bid(3, 1), 0, 20)}; // the weaver of island 3
    sim::Effects b;
    b.reserves = {rs(sim::FigureId{2}, bid(0, 4), 1, 30),  // the second line of the bow maker
                  rs(sim::FigureId{2}, bid(0, 1), 0, 10),  // the cart carries other goods: 0
                  rs(sim::FigureId{9}, bid(0, 1), 0, 10),  // no such figure
                  rs(sim::FigureId{2}, bid(0, 99), 0, 10), // no such building
                  rs(sim::FigureId{2}, bid(0, 4), 2, 10),  // no such store
                  rs(sim::FigureId{2}, bid(0, 6), 0, 10)}; // an inert building
    script.calls = {a, b};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    for (std::uint32_t id = 1; id <= 3; ++id) {
        sim::Figure f;
        f.id = sim::FigureId{id};
        REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
                sim::kNoSlot);
    }
    sim::WorldTestAccess::ids(w).next_figure = 4;
    payload<sim::Workshop>(w, 0, 1).out[0] = 1000;
    payload<sim::Workshop>(w, 3, 1).out[0] = 100;
    payload<sim::WeaponsWorkshop>(w, 0, 4).out[1] = 100;
    lap(w);
    const sim::Figure& f1 = *sim::find_figure(w.figures(), sim::FigureId{1});
    CHECK(f1.cargo_goods == cloth);
    CHECK(f1.cargo_88 == 1000);
    CHECK(payload<sim::Workshop>(w, 0, 1).out[0] == 0);
    // The building of island 3 is found through its own island.
    CHECK(sim::find_figure(w.figures(), sim::FigureId{3})->cargo_88 == 20);
    CHECK(payload<sim::Workshop>(w, 3, 1).out[0] == 80);
    // The second buffer: cart 2 already carries 5 of cloth, so it cannot take the arrows
    // of the bow maker, but the weaver's cloth goes on top of the cargo; the figure, the
    // building and the store that are not there take nothing.
    sim::Figure& f2 = *sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{2});
    f2.cargo_goods = cloth;
    f2.cargo_88 = 5;
    payload<sim::Workshop>(w, 0, 1).out[0] = 100;
    lap(w);
    CHECK(f2.cargo_goods == cloth);
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, 4).out[1] == 100);
    CHECK(f2.cargo_88 == 5 + 10);
    CHECK(payload<sim::Workshop>(w, 0, 1).out[0] == 90);
}

TEST_CASE("merge reserves fill an empty cart, set its goods and respect the room left") {
    const Kit k = make_kit();
    const sim::GoodsId arrows = k.registry->find(k.bow_maker)->output_goods[1];
    Script script;
    sim::Effects a;
    auto rs = [](std::uint32_t out, std::uint32_t amount) {
        sim::Reserve r;
        r.figure = sim::FigureId{1};
        r.building = bid(0, 4);
        r.out_index = out;
        r.amount_88 = amount;
        return r;
    };
    a.reserves = {rs(1, 30)};
    sim::Effects b;
    b.reserves = {rs(1, 1000)};
    script.calls = {a, b};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    sim::Figure f;
    f.id = sim::FigureId{1};
    f.cargo_goods = 3; // empty cart with some old goods number
    sim::WorldTestAccess::ids(w).next_figure = 2;
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    payload<sim::WeaponsWorkshop>(w, 0, 4).out[1] = 100;
    lap(w);
    sim::Figure& cart = *sim::find_figure(sim::WorldTestAccess::figures(w), sim::FigureId{1});
    CHECK(cart.cargo_goods == arrows);
    CHECK(cart.cargo_88 == 30);
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, 4).out[1] == 70);
    // The room left in the cargo (u32) limits the amount taken.
    cart.cargo_88 = std::numeric_limits<std::uint32_t>::max() - 20;
    lap(w);
    CHECK(cart.cargo_88 == std::numeric_limits<std::uint32_t>::max());
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, 4).out[1] == 50);
}

TEST_CASE("merge reserves leave the cart alone when nothing is taken") {
    const Kit k = make_kit();
    Script script;
    auto rs = [](std::uint32_t out) {
        sim::Reserve r;
        r.figure = sim::FigureId{1};
        r.building = bid(0, 1);
        r.out_index = out;
        r.amount_88 = 50;
        return r;
    };
    sim::Effects a;
    // The weaver's store 0 is empty; store 1 exists in a corrupt payload (two stores for a
    // type with one output) but names no goods, so it is refused.
    a.reserves = {rs(0), rs(1)};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    sim::Figure f;
    f.id = sim::FigureId{1};
    f.cargo_goods = 3; // an empty cart with an old goods number
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    sim::WorldTestAccess::ids(w).next_figure = 2;
    auto& weaver = payload<sim::Workshop>(w, 0, 1);
    weaver.out.assign(2, 0);
    weaver.out[1] = 77;
    lap(w);
    const sim::Figure& cart = *sim::find_figure(w.figures(), sim::FigureId{1});
    CHECK(cart.cargo_goods == 3);
    CHECK(cart.cargo_88 == 0);
    CHECK(weaver.out[0] == 0);
    CHECK(weaver.out[1] == 77);
}

TEST_CASE("merge deliveries move the cargo into the city stock and keep what does not fit") {
    const Kit k = make_kit();
    Script script;
    auto dv = [](std::uint32_t figure, sim::CityRef c) {
        sim::Deliver d;
        d.figure = sim::FigureId{figure};
        d.city = c;
        return d;
    };
    sim::Effects a;
    a.deliveries = {dv(1, cref(0, 0)), dv(2, cref(0, 0)), dv(3, cref(0, 0)), dv(4, cref(0, 0)),
                    dv(5, cref(0, 0)), dv(1, cref(0, 7)), dv(9, cref(0, 0))};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    auto add = [&](std::uint32_t id, sim::GoodsId goods, std::uint32_t cargo) {
        sim::Figure f;
        f.id = sim::FigureId{id};
        f.cargo_goods = goods;
        f.cargo_88 = cargo;
        REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
                sim::kNoSlot);
    };
    add(1, 20, 500);  // fits
    add(2, 21, 5000); // the stock holds 12000, capacity 12800: 800 fit, 4200 stay
    add(3, 56, 100);  // a goods number outside the warehouse: stays
    add(4, 22, 0);    // nothing to deliver
    add(5, 23, 300);  // delivered to a city that exists, the other deliveries aside
    sim::WorldTestAccess::ids(w).next_figure = 6;
    city(w, 0).stock.stock[21] = 12000;
    lap(w);
    auto fig = [&](std::uint32_t id) { return *sim::find_figure(w.figures(), sim::FigureId{id}); };
    CHECK(city(w, 0).stock.stock[20] == 500);
    CHECK(fig(1).cargo_88 == 0);
    CHECK(fig(1).cargo_goods == 0); // an empty cart carries no goods
    CHECK(city(w, 0).stock.stock[21] == 12800);
    CHECK(fig(2).cargo_88 == 4200);
    CHECK(fig(2).cargo_goods == 21);
    CHECK(fig(3).cargo_88 == 100);
    CHECK(fig(3).cargo_goods == 56);
    CHECK(fig(4).cargo_88 == 0);
    CHECK(fig(4).cargo_goods == 22); // nothing to deliver: the cart is left as it is
    CHECK(city(w, 0).stock.stock[22] == 0);
    CHECK(city(w, 0).stock.stock[23] == 300);
    CHECK(fig(5).cargo_88 == 0);
}

TEST_CASE("merge delivery of a stock above its capacity does not hand the cart a negative amount") {
    // CityStock::add lowers a stock that is above the capacity (and returns a wrapped
    // amount): the cart keeps its whole cargo then.
    const Kit k = make_kit();
    Script script;
    sim::Effects a;
    sim::Deliver d;
    d.figure = sim::FigureId{1};
    d.city = cref(0, 0);
    a.deliveries = {d};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    sim::Figure f;
    f.id = sim::FigureId{1};
    f.cargo_goods = 20;
    f.cargo_88 = 500;
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    sim::WorldTestAccess::ids(w).next_figure = 2;
    city(w, 0).stock.stock[20] = 20000; // above the capacity 12800
    lap(w);
    const sim::Figure& cart = *sim::find_figure(w.figures(), sim::FigureId{1});
    CHECK(cart.cargo_88 == 500);
    CHECK(cart.cargo_goods == 20);
    CHECK(city(w, 0).stock.stock[20] == 12800);
}

TEST_CASE("merge goods moves and deliveries with a huge amount never lower the stock") {
    // CityStock::add forms stock + amount in u64 (F35): with a u32 sum the amount
    // 2^32 - 1 on a stock of 12800 would wrap to 12799 and skip the capacity clip.
    const Kit k = make_kit();
    Script script;
    sim::GoodsMove m;
    m.city = cref(0, 0);
    m.goods = 15;
    m.delta_88 = std::int64_t{1} << 40; // cut to 0xFFFFFFFF
    sim::GoodsMove m2 = m;
    m2.goods = 16;
    m2.delta_88 = 0xFFFFFFFFLL;
    sim::GoodsMove m3 = m;
    m3.goods = 17;
    m3.delta_88 = 0xFFFFFFFFLL;
    sim::Deliver d;
    d.figure = sim::FigureId{1};
    d.city = cref(0, 0);
    sim::Effects a;
    a.goods_moves = {m, m2, m3};
    a.deliveries = {d};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    city(w, 0).stock.stock[15] = 12800; // at the capacity
    city(w, 0).stock.stock[16] = 100;
    city(w, 0).stock.stock[17] = 0;
    city(w, 0).stock.stock[18] = 12800;
    sim::Figure f;
    f.id = sim::FigureId{1};
    f.cargo_goods = 18;
    f.cargo_88 = 0xFFFFFFFFU;
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    sim::WorldTestAccess::ids(w).next_figure = 2;
    lap(w);
    CHECK(city(w, 0).stock.stock[15] == 12800);
    CHECK(city(w, 0).stock.stock[16] == 12800);
    CHECK(city(w, 0).stock.stock[17] == 12800);
    // The delivery on a full stock: nothing fits, the cart keeps its whole cargo.
    CHECK(city(w, 0).stock.stock[18] == 12800);
    const sim::Figure& cart = *sim::find_figure(w.figures(), sim::FigureId{1});
    CHECK(cart.cargo_88 == 0xFFFFFFFFU);
    CHECK(cart.cargo_goods == 18);
}

TEST_CASE("merge timed events saturate the due tick and refuse when the sequence is used up") {
    const Kit k = make_kit();
    Script script;
    sim::EnqueueEvent far;
    far.delay = std::numeric_limits<std::uint64_t>::max();
    sim::EnqueueEvent near_end;
    near_end.delay = std::numeric_limits<std::uint64_t>::max() - 10; // tick 50 + delay wraps
    sim::EnqueueEvent plain;
    plain.delay = 7;
    sim::Effects a;
    a.events = {far, near_end, plain};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w); // tick 50
    const auto& q = w.events().q;
    REQUIRE(q.size() == 3);
    auto it = q.begin();
    CHECK(it->first == sim::EventKey{57, 3}); // the plain one fires first
    ++it;
    CHECK(it->first == sim::EventKey{std::numeric_limits<std::uint64_t>::max(), 1});
    ++it;
    CHECK(it->first == sim::EventKey{std::numeric_limits<std::uint64_t>::max(), 2});
    CHECK(w.ids().next_event_seq == 4);
    // The last sequence number: the event with 2^64 - 2 is queued, the counter reaches
    // 2^64 - 1 (used up, a legal state) and the next enqueue is refused, never wrapped.
    sim::IdCounters ids;
    ids.next_event_seq = std::numeric_limits<std::uint64_t>::max() - 1;
    sim::EventQueue queue;
    CHECK(queue.enqueue(ids, 5, 1, sim::TimedEvent{}));
    CHECK(ids.next_event_seq == std::numeric_limits<std::uint64_t>::max());
    CHECK_FALSE(queue.enqueue(ids, 5, 1, sim::TimedEvent{}));
    CHECK(ids.next_event_seq == std::numeric_limits<std::uint64_t>::max());
    CHECK(queue.q.size() == 1);
    CHECK(queue.q.begin()->first ==
          sim::EventKey{6, std::numeric_limits<std::uint64_t>::max() - 1});
}

TEST_CASE("the figure birth keeps all 32 low bits of the tick") {
    // A birth cut to 16 bits would pass the tests at small ticks (D12 reads u32(t - born)).
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 1)};
    script.calls = {e};
    for (const std::uint64_t first_tick : {std::uint64_t{70000}, std::uint64_t{3000000000}}) {
        INFO("tick " << first_tick);
        script.next = 0;
        sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
        sim::WorldTestAccess::clock(w).ms = first_tick * 2;
        lap(w);
        REQUIRE(w.clock().tick() == first_tick + 50);
        REQUIRE(w.figures().list.slots[0].has_value());
        CHECK(w.figures().list.slots[0]->born == first_tick + 50);
    }
}

TEST_CASE("merge stamps events with the 64 bit tick and the figure's birth with its low 32 bits") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 1)};
    sim::EnqueueEvent ev;
    ev.delay = 25;
    e.events = {ev};
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    sim::WorldTestAccess::clock(w).ms = std::uint64_t{2} << 32; // tick 2^32
    lap(w); // tick 2^32 + 50, whose low 32 bits are 50
    const std::uint64_t tick = (std::uint64_t{1} << 32) + 50;
    REQUIRE(w.clock().tick() == tick);
    CHECK(w.figures().list.slots[0]->born == 50);
    const std::vector<sim::SimEvent> ui = w.drain_events();
    REQUIRE(ui.size() == 1);
    CHECK(ui[0].tick == tick);
    REQUIRE(w.events().q.size() == 1);
    CHECK(w.events().q.begin()->first.first == tick + 25);
}

TEST_CASE("merge queues timed events with the world's sequence numbers") {
    const Kit k = make_kit();
    Script script;
    sim::Effects a;
    sim::EnqueueEvent first;
    first.delay = 25;
    first.e.kind = sim::EventKind::TileCleanup;
    first.e.tile = sim::TileXY{3, 4};
    // Delay 1, not 0: an event the island phase queues due now is dispatched by step 2
    // of the same advance (stage S12, world_events_test.cpp), and this case looks at
    // the queue after the advance.
    sim::EnqueueEvent second;
    second.delay = 1;
    second.e.kind = sim::EventKind::DelayedHit;
    second.e.subtype = 2;
    second.e.value = 7;
    second.e.target = sim::FigureId{5};
    second.e.attacker_kind = 2;
    second.e.attacker = sim::FigureId{6};
    a.events = {first, second};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w); // tick 50
    const auto& q = w.events().q;
    REQUIRE(q.size() == 2);
    // Keyed (due, seq): the delayed hit is due at 51 (seq 2), the cleanup at 75 (seq 1).
    auto it = q.begin();
    CHECK(it->first == sim::EventKey{51, 2});
    CHECK(it->second.kind == sim::EventKind::DelayedHit);
    CHECK(it->second.value == 7);
    CHECK(it->second.target == sim::FigureId{5});
    CHECK(it->second.attacker == sim::FigureId{6});
    CHECK(it->second.attacker_kind == 2);
    ++it;
    CHECK(it->first == sim::EventKey{75, 1});
    CHECK(it->second.kind == sim::EventKind::TileCleanup);
    CHECK(it->second.tile == sim::TileXY{3, 4});
    CHECK(w.ids().next_event_seq == 3);
}

TEST_CASE("merge appends UI events to the advance output and to the host queue") {
    const Kit k = make_kit();
    Script script;
    sim::Effects a;
    sim::SimEvent e1;
    e1.kind = sim::SimEventKind::Event15a;
    e1.x = 4;
    sim::SimEvent e2;
    e2.kind = sim::SimEventKind::TimerExpired;
    a.ui = {e1, e2};
    sim::Effects b;
    b.ui = {e2};
    script.calls = {a, b};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w);
    REQUIRE(w.last_output().events.size() == 2);
    CHECK(w.last_output().events[0].kind == sim::SimEventKind::Event15a);
    CHECK(w.last_output().events[0].x == 4);
    CHECK(w.last_output().events[1].kind == sim::SimEventKind::TimerExpired);
    lap(w);
    // The advance output holds the last advance only, the queue everything since the last
    // drain, oldest first.
    REQUIRE(w.last_output().events.size() == 1);
    const std::vector<sim::SimEvent> all = w.drain_events();
    REQUIRE(all.size() == 3);
    CHECK(all[0].kind == sim::SimEventKind::Event15a);
    CHECK(all[2].kind == sim::SimEventKind::TimerExpired);
    CHECK(w.drain_events().empty());
    // Draining does not touch the last output, and neither is state.
    CHECK(w.last_output().events.size() == 1);
    sim::World same = make_world_with(k, merge_scenario(k), script_seams(&script));
    script.next = 0;
    lap(same);
    lap(same);
    CHECK(same.state_hash() == w.state_hash());
}

TEST_CASE("the host event queue is bounded and drops the oldest events") {
    const Kit k = make_kit();
    Script script;
    sim::Effects a;
    for (std::uint32_t i = 0; i < 5; ++i) {
        sim::SimEvent e;
        e.kind = sim::SimEventKind::Event15a;
        e.x = static_cast<std::int32_t>(i);
        a.ui.push_back(e);
    }
    script.calls = {a, a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    CHECK(w.event_capacity() == sim::kHostEventCapacityDefault);
    CHECK(w.events_dropped() == 0);
    w.set_event_capacity(3);
    lap(w);
    // The advance output keeps all five; the host queue keeps the newest three.
    CHECK(w.last_output().events.size() == 5);
    CHECK(w.events_dropped() == 2);
    const std::vector<sim::SimEvent> kept = w.drain_events();
    REQUIRE(kept.size() == 3);
    CHECK(kept[0].x == 2);
    CHECK(kept[2].x == 4);
    // Lowering the capacity drops the oldest queued events at once.
    lap(w);
    CHECK(w.events_dropped() == 2 + 2);
    w.set_event_capacity(1);
    CHECK(w.events_dropped() == 2 + 2 + 2);
    const std::vector<sim::SimEvent> last = w.drain_events();
    REQUIRE(last.size() == 1);
    CHECK(last[0].x == 4);
    // Capacity 0 keeps nothing, the advance output still has the events, and none of it
    // is state.
    script.next = 0;
    sim::World none = make_world_with(k, merge_scenario(k), script_seams(&script));
    none.set_event_capacity(0);
    lap(none);
    script.next = 0;
    sim::World full = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(full);
    CHECK(none.drain_events().empty());
    CHECK(none.events_dropped() == 5);
    CHECK(none.last_output().events.size() == 5);
    CHECK(none.state_hash() == full.state_hash());
    // A copy of a world carries the queue, bounded by the capacity.
    sim::World copy = full;
    CHECK(copy.drain_events().size() == 5);
    CHECK(full.drain_events().size() == 5);
}

TEST_CASE("merge applies the kinds of one buffer in the order spawns reserves deliveries") {
    const Kit k = make_kit();
    const sim::GoodsId cloth = k.registry->find(k.weaver)->output_goods[0];
    Script script;
    sim::Effects a;
    a.spawns = {spawn(bid(0, 1), 3)};
    sim::Reserve r;
    r.figure = sim::FigureId{1}; // the figure the spawn of this very buffer creates
    r.building = bid(0, 1);
    r.amount_88 = 40;
    a.reserves = {r};
    sim::Deliver d;
    d.figure = sim::FigureId{1};
    d.city = cref(0, 0);
    a.deliveries = {d};
    sim::GoodsMove m;
    m.city = cref(0, 0);
    m.goods = cloth;
    m.delta_88 = 100;
    a.goods_moves = {m};
    script.calls = {a};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    payload<sim::Workshop>(w, 0, 1).out[0] = 90;
    lap(w);
    // spawn (figure 1) -> reserve takes 40 -> delivery moves them: 100 from the goods move
    // plus 40 delivered, 50 left in the weaver.
    CHECK(city(w, 0).stock.stock[cloth] == 140);
    CHECK(payload<sim::Workshop>(w, 0, 1).out[0] == 50);
    const sim::Figure& cart = *sim::find_figure(w.figures(), sim::FigureId{1});
    CHECK(cart.cargo_88 == 0);
}

namespace {

/// A synchronous reference executor for effects (C1, docs/sim/state.md §4.11): it applies
/// every effect of a buffer at once to a plain model of the state the merge writes to,
/// in the documented order of a buffer (spawns, releases, money, goods moves, events,
/// UI). The figure slots are allocated by a naive transliteration of the group list's
/// insert loop (§5.2), written down here independently of SlotList.
struct SyncModel {
    std::int64_t money[2] = {10000, 25000};
    std::uint32_t stock[4] = {0, 0, 0, 0}; // goods 9 .. 12 of city 0 on island 0
    std::uint32_t carriers[2] = {40, 40};  // the weavers of island 0 and island 3
    struct Fig {
        std::uint64_t id;
        std::uint32_t kind;
        std::uint32_t owner;
        std::uint64_t home;
        std::size_t slot;
    };
    std::vector<Fig> figures;
    std::vector<int> used = std::vector<int>(1, 0); // the slot vector: 0 free, 1 taken
    std::int64_t hint = -1;
    std::uint64_t next_figure = 1, next_seq = 1;
    std::vector<std::pair<sim::EventKey, std::uint32_t>> events; // key and the event's value
    std::vector<sim::SimEvent> ui;

    std::size_t insert() {
        const std::size_t n = used.size();
        std::size_t slot = n;
        bool found = false;
        for (std::size_t i = 0; i < n && !found; ++i) {
            const std::size_t idx = static_cast<std::size_t>(
                (hint + 1 + static_cast<std::int64_t>(i)) % static_cast<std::int64_t>(n));
            if (used[idx] == 0) {
                slot = idx;
                found = true;
            }
        }
        if (!found) {
            std::size_t grown = n + n / 4;
            if (grown < 10) {
                grown = 10;
            }
            if (grown <= n) {
                grown = n + 1;
            }
            used.resize(grown, 0);
        }
        used[slot] = 1;
        hint = static_cast<std::int64_t>(slot);
        return slot;
    }

    void apply(const sim::Effects& fx, std::uint64_t tick) {
        std::vector<sim::SimEvent> spawned;
        for (const sim::SpawnRequest& s : fx.spawns) {
            Fig f{next_figure++, s.figure_kind, s.owner.v, s.home.v, insert()};
            figures.push_back(f);
            sim::SimEvent e;
            e.kind = sim::SimEventKind::FigureSpawned;
            e.tick = tick;
            e.player = s.owner;
            e.building = s.home;
            e.figure = sim::FigureId{f.id};
            e.code = s.figure_kind;
            e.x = s.pos.x / 256;
            e.y = s.pos.y / 256;
            spawned.push_back(e);
        }
        for (const sim::ReleaseCounter& r : fx.releases) {
            const std::size_t w = sim::island_of(r.home).v == 0 ? 0 : 1;
            if (r.home_counter == 0 && carriers[w] > 0) {
                --carriers[w];
            }
        }
        for (const sim::MoneyDelta& m : fx.money) {
            money[m.player.v] += m.delta;
        }
        for (const sim::GoodsMove& g : fx.goods_moves) {
            std::uint32_t& s = stock[g.goods - 9];
            if (g.delta_88 > 0) {
                s = static_cast<std::uint32_t>(std::min<std::int64_t>(12800, s + g.delta_88));
            } else {
                s = static_cast<std::uint32_t>(std::max<std::int64_t>(0, s + g.delta_88));
            }
        }
        for (const sim::EnqueueEvent& e : fx.events) {
            events.push_back({sim::EventKey{tick + e.delay, next_seq++}, e.e.value});
        }
        std::sort(events.begin(), events.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const sim::SimEvent& e : fx.ui) {
            ui.push_back(e);
        }
        for (const sim::SimEvent& e : spawned) {
            ui.push_back(e);
        }
    }

    /// Step 2 of the advance (stage S12): with no handler installed every event due at
    /// `tick` is taken out of the queue and nothing else happens.
    void dispatch(std::uint64_t tick) {
        std::erase_if(events, [&](const auto& e) { return e.first.first <= tick; });
    }
};

/// Two scripted buffers streams in one seam: island 0 city 0 and island 3 city 1 each have
/// a list of buffers.
struct TwoScripts {
    std::vector<sim::Effects> a, b;
    std::size_t na = 0, nb = 0;
};

} // namespace

TEST_CASE("effects applied after the island ticks equal the synchronous application") {
    const Kit k = make_kit();
    // A random but fixed stream of buffers (an LCG, not the world's PRNG).
    std::uint32_t x = 777;
    auto rnd = [&](std::uint32_t n) {
        x = x * 1103515245U + 12345U;
        return ((x >> 16) & 0x7fffU) % n;
    };
    auto make_buffer = [&](std::uint32_t island_slot, std::uint32_t city_no) {
        sim::Effects fx;
        for (std::uint32_t i = rnd(3); i > 0; --i) {
            sim::SpawnRequest s = spawn(bid(island_slot, 1), rnd(100), rnd(3), 0, rnd(2));
            s.city = cref(island_slot, city_no);
            fx.spawns.push_back(s);
        }
        for (std::uint32_t i = rnd(3); i > 0; --i) {
            sim::ReleaseCounter r;
            r.home = bid(rnd(2) == 0 ? 0 : 3, 1);
            r.home_counter = rnd(4) == 0 ? 1 : 0;
            fx.releases.push_back(r);
        }
        for (std::uint32_t i = rnd(3); i > 0; --i) {
            sim::MoneyDelta m;
            m.player = sim::PlayerId{rnd(2)};
            m.delta = static_cast<std::int64_t>(rnd(2000)) - 1000;
            fx.money.push_back(m);
        }
        for (std::uint32_t i = rnd(4); i > 0; --i) {
            sim::GoodsMove g;
            g.city = cref(0, 0);
            g.goods = 9 + rnd(4);
            g.delta_88 = static_cast<std::int64_t>(rnd(9000)) - 4000;
            fx.goods_moves.push_back(g);
        }
        for (std::uint32_t i = rnd(3); i > 0; --i) {
            sim::EnqueueEvent e;
            e.delay = rnd(200);
            e.e.value = rnd(1000);
            fx.events.push_back(e);
        }
        for (std::uint32_t i = rnd(3); i > 0; --i) {
            sim::SimEvent e;
            e.kind = sim::SimEventKind::Notice;
            e.code = rnd(1000);
            fx.ui.push_back(e);
        }
        return fx;
    };
    TwoScripts script;
    const int laps = 60;
    for (int i = 0; i < laps; ++i) {
        script.a.push_back(make_buffer(0, 0));
        script.b.push_back(make_buffer(3, 1));
    }
    sim::Seams seams = named_seams();
    seams.city_pass[0].name = "two_scripts";
    seams.city_pass[0].ctx = &script;
    seams.city_pass[0].fn = [](void* ctx, const sim::TickContext&, sim::Island& is, sim::City& c,
                               sim::Effects& fx) {
        auto& s = *static_cast<TwoScripts*>(ctx);
        if (is.index.v == 0 && c.number.v == 0 && s.na < s.a.size()) {
            fx = s.a[s.na++];
        } else if (is.index.v == 3 && c.number.v == 1 && s.nb < s.b.size()) {
            fx = s.b[s.nb++];
        }
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    // The buildings must not tick (no production, no spawn of their own): switched off.
    std::get<sim::Workshop>(building(w, 0, 1).payload).switched_on = false;
    std::get<sim::Farm>(building(w, 0, 2).payload).switched_on = false;
    std::get<sim::Workshop>(building(w, 3, 1).payload).switched_on = false;
    payload<sim::Workshop>(w, 0, 1).carriers_out = 40;
    payload<sim::Workshop>(w, 3, 1).carriers_out = 40;
    SyncModel model;
    for (int lapno = 0; lapno < laps; ++lapno) {
        lap(w);
        const std::uint64_t tick = w.clock().tick();
        // Island order: island 0's buffer, then island 3's.
        model.apply(script.a[static_cast<std::size_t>(lapno)], tick);
        model.apply(script.b[static_cast<std::size_t>(lapno)], tick);
        model.dispatch(tick);
        INFO("lap " << lapno);
        REQUIRE(w.players().find(sim::PlayerId{0})->money == model.money[0]);
        REQUIRE(w.players().find(sim::PlayerId{1})->money == model.money[1]);
        for (std::uint32_t g = 0; g < 4; ++g) {
            REQUIRE(city(w, 0).stock.stock[9 + g] == model.stock[g]);
        }
        REQUIRE(payload<sim::Workshop>(w, 0, 1).carriers_out == model.carriers[0]);
        REQUIRE(payload<sim::Workshop>(w, 3, 1).carriers_out == model.carriers[1]);
        REQUIRE(w.ids().next_figure == model.next_figure);
        REQUIRE(w.ids().next_event_seq == model.next_seq);
        REQUIRE(w.figures().by_id.size() == model.figures.size());
        for (const SyncModel::Fig& f : model.figures) {
            const sim::Figure* got = sim::find_figure(w.figures(), sim::FigureId{f.id});
            REQUIRE(got != nullptr);
            REQUIRE(got->kind == f.kind);
            REQUIRE(got->owner.v == f.owner);
            REQUIRE(got->home.v == f.home);
            REQUIRE(w.figures().by_id.at(sim::FigureId{f.id}) == f.slot);
        }
        REQUIRE(w.figures().list.slots.size() == model.used.size());
        REQUIRE(w.figures().list.hint == model.hint);
        REQUIRE(w.events().q.size() == model.events.size());
        std::size_t i = 0;
        for (const auto& [key, event] : w.events().q) {
            REQUIRE(key == model.events[i].first);
            REQUIRE(event.value == model.events[i].second);
            ++i;
        }
    }
    const std::vector<sim::SimEvent> ui = w.drain_events();
    REQUIRE(ui.size() == model.ui.size());
    for (std::size_t i = 0; i < ui.size(); ++i) {
        REQUIRE(ui[i].kind == model.ui[i].kind);
        REQUIRE(ui[i].tick == model.ui[i].tick);
        REQUIRE(ui[i].code == model.ui[i].code);
        REQUIRE(ui[i].building == model.ui[i].building);
        REQUIRE(ui[i].figure == model.ui[i].figure);
        REQUIRE(ui[i].player == model.ui[i].player);
    }
    CHECK(model.figures.size() > 20);
    CHECK(model.next_seq > 21);      // events queued over the run
    CHECK(!model.events.empty());    // some still pending at the end
    CHECK(model.events.size() < 20); // the rest were dispatched (stage S12)
    CHECK(model.ui.size() > 40);
}

TEST_CASE("merge happens after each island and before the next island's tick") {
    const Kit k = make_kit();
    struct Probe {
        const sim::World* world = nullptr;
        std::vector<std::size_t> figures_seen;
    } probe;
    sim::Seams seams = named_seams();
    seams.city_pass[0].name = "probe";
    seams.city_pass[0].ctx = &probe;
    seams.city_pass[0].fn = [](void* ctx, const sim::TickContext&, sim::Island& is, sim::City& c,
                               sim::Effects& fx) {
        auto& p = *static_cast<Probe*>(ctx);
        if (c.number.v != 1 && is.index.v == 3) {
            return;
        }
        p.figures_seen.push_back(p.world->figures().by_id.size());
        if (is.index.v == 0 && c.number.v == 0) {
            fx.spawns.push_back(spawn(bid(0, 1), 1));
            fx.spawns.push_back(spawn(bid(0, 1), 2));
        }
        if (is.index.v == 3) {
            fx.spawns.push_back(spawn(bid(3, 1), 3, 0, 0, 1));
        }
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    probe.world = &w;
    lap(w);
    // The calls: island 0 city 0 and island 0 city 1 belong to ONE island tick, whose
    // buffer is merged after it (not after each city): both see no figure. Island 3 comes
    // after that merge and sees the two figures of island 0.
    REQUIRE(probe.figures_seen.size() == 3);
    CHECK(probe.figures_seen[0] == 0);
    CHECK(probe.figures_seen[1] == 0);
    CHECK(probe.figures_seen[2] == 2);
    // The figure of island 3 is the third, in island order.
    REQUIRE(w.figures().by_id.size() == 3);
    CHECK(w.figures().list.slots[2]->kind == 3);
    CHECK(w.figures().list.slots[2]->id == sim::FigureId{3});
}

// ----------------------------------------------------------- the player seams

TEST_CASE("player seams are called per player with the merge of each call before the next") {
    const Kit k = make_kit();
    struct Seen {
        std::vector<std::string> lines;
    } seen;
    sim::Seams seams = named_seams();
    seams.player_groups.name = "groups";
    seams.player_groups.ctx = &seen;
    seams.player_groups.fn = [](void* ctx, const sim::TickContext& tc, sim::SerialWorld& sw,
                                sim::Player& p) {
        auto& s = *static_cast<Seen*>(ctx);
        const sim::Player* other = sw.players().find(sim::PlayerId{1});
        const sim::Player* first = sw.players().find(sim::PlayerId{0});
        s.lines.push_back("groups " + num(p.index.v) + " tick=" + num(tc.tick) +
                          " money0=" + num(static_cast<std::uint64_t>(first->money)) +
                          " money1=" + num(static_cast<std::uint64_t>(other->money)));
        // The queries of the serial world are the const queries of the whole state.
        s.lines.push_back("island " + num(sw.island(sim::IslandId{3}) != nullptr ? 1 : 0) +
                          num(sw.island(sim::IslandId{1}) != nullptr ? 1 : 0) + " city " +
                          num(sw.city(cref(0, 1)) != nullptr ? 1 : 0) +
                          num(sw.city(cref(0, 9)) != nullptr ? 1 : 0) + " building " +
                          num(sw.find_building(bid(0, 1)) != nullptr ? 1 : 0) +
                          num(sw.find_building(bid(0, 77)) != nullptr ? 1 : 0));
        sim::MoneyDelta m;
        m.player = sim::PlayerId{1};
        m.delta = 1000;
        sw.fx.money.push_back(m);
    };
    seams.defeat.name = "defeat";
    seams.defeat.ctx = &seen;
    seams.defeat.fn = [](void* ctx, const sim::TickContext&, sim::SerialWorld& sw, sim::Player& p) {
        auto& s = *static_cast<Seen*>(ctx);
        s.lines.push_back("defeat " + num(p.index.v) + " pending=" + num(p.defeat_pending ? 1 : 0));
        sim::SpawnRequest sp;
        sp.home = bid(0, 1);
        sp.figure_kind = 99;
        sp.owner = p.index;
        sw.fx.spawns.push_back(sp);
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    sim::WorldTestAccess::players(w).slots[1]->defeat_pending = true;
    w.advance(10); // tick 5: the table visits 5 * 32 / 50 + 1 = 4 slots: players 0 and 1
    REQUIRE(seen.lines.size() == 5);
    // Player 0: groups (money of player 1 not yet changed), no defeat pass.
    CHECK(seen.lines[0] == "groups 0 tick=5 money0=10000 money1=25000");
    CHECK(seen.lines[1] == "island 10 city 10 building 10");
    // Player 1: the effect of player 0's call (+1000) is already merged (C2) ...
    CHECK(seen.lines[2] == "groups 1 tick=5 money0=10000 money1=26000");
    // ... and the defeat pass follows its groups pass after the flag was cleared; its
    // spawn is merged right away.
    CHECK(seen.lines[3] == "island 10 city 10 building 10");
    CHECK(seen.lines[4] == "defeat 1 pending=0");
    // The groups pass of player 1 asked for +1000 as well.
    CHECK(w.players().find(sim::PlayerId{1})->money == 27000);
    CHECK_FALSE(w.players().find(sim::PlayerId{1})->defeat_pending);
    REQUIRE(w.figures().by_id.size() == 1);
    CHECK(w.figures().list.slots[0]->kind == 99);
    CHECK(w.figures().list.slots[0]->owner == sim::PlayerId{1});
}

TEST_CASE("the defeat seam runs only for an alive player and never without the flag") {
    const Kit k = make_kit();
    Log log;
    sim::Seams seams = named_seams();
    seams.defeat.name = "defeat";
    seams.defeat.ctx = &log;
    seams.defeat.fn = [](void* ctx, const sim::TickContext&, sim::SerialWorld&, sim::Player& p) {
        static_cast<Log*>(ctx)->add("defeat " + num(p.index.v));
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    w.advance(10);
    CHECK(log.lines.empty()); // no flag
    sim::WorldTestAccess::players(w).slots[0]->defeat_pending = true;
    sim::WorldTestAccess::players(w).slots[1]->defeat_pending = true;
    sim::WorldTestAccess::players(w).slots[1]->state = sim::PlayerState::Defeated;
    sim::WorldTestAccess::players(w).cursor = 0;
    w.advance(10);
    CHECK(log.lines == std::vector<std::string>{"defeat 0"});
    CHECK_FALSE(w.players().find(sim::PlayerId{0})->defeat_pending);
    // Without a seam the flag is cleared and nothing else happens.
    sim::World plain = make_world_with(k, merge_scenario(k), named_seams());
    sim::WorldTestAccess::players(plain).slots[0]->defeat_pending = true;
    plain.advance(10);
    CHECK_FALSE(plain.players().find(sim::PlayerId{0})->defeat_pending);
}

TEST_CASE("draw telemetry sums the draws per site over every merge and lists the sites in order") {
    const Kit k = make_kit();
    sim::Seams seams = named_seams();
    seams.city_pass[0].name = "drawer";
    seams.city_pass[0].fn = [](void*, const sim::TickContext& tc, sim::Island& is, sim::City& c,
                               sim::Effects&) {
        if (is.index.v == 0 && c.number.v == 0) {
            static_cast<void>(tc.draw.next(sim::DrawSite::SmallFarmAnimal));
            static_cast<void>(tc.draw.next(sim::DrawSite::Event750Island));
            static_cast<void>(tc.draw.next(sim::DrawSite::Event750Island));
        } else if (is.index.v == 3) {
            for (int i = 0; i < 3; ++i) {
                static_cast<void>(tc.draw.next(sim::DrawSite::Ai));
            }
            static_cast<void>(tc.draw.next(sim::DrawSite::Event750Island));
            static_cast<void>(tc.draw.next(sim::DrawSite::Event750Island));
        }
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    const std::uint32_t before = w.rng().cursor();
    lap(w);
    // Island 0 made 1 + 2 draws, island 3 made 3 + 2: 8 in all, summed per site and listed
    // in the order of the DrawSite enumerators (not the order of the merges).
    const sim::SimOutput& out = w.last_output();
    REQUIRE(out.draws.size() == 3);
    CHECK(out.draws[0].site == sim::DrawSite::SmallFarmAnimal);
    CHECK(out.draws[0].count == 1);
    CHECK(out.draws[1].site == sim::DrawSite::Event750Island);
    CHECK(out.draws[1].count == 4);
    CHECK(out.draws[2].site == sim::DrawSite::Ai);
    CHECK(out.draws[2].count == 3);
    CHECK(out.rng_cursor_before == before);
    CHECK(out.rng_cursor_after == (before + 8) % 2048);
}

TEST_CASE("draw telemetry stays in site order when a later merge draws an earlier site") {
    const Kit k = make_kit();
    sim::Seams seams = named_seams();
    seams.city_pass[0].name = "drawer";
    seams.city_pass[0].fn = [](void*, const sim::TickContext& tc, sim::Island& is, sim::City& c,
                               sim::Effects&) {
        if (is.index.v == 0 && c.number.v == 0) {
            static_cast<void>(tc.draw.next(sim::DrawSite::Ai));
            static_cast<void>(tc.draw.next(sim::DrawSite::Ai));
        } else if (is.index.v == 3) {
            static_cast<void>(tc.draw.next(sim::DrawSite::SmallFarmAnimal));
        }
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    lap(w);
    const sim::SimOutput& out = w.last_output();
    REQUIRE(out.draws.size() == 2);
    CHECK(out.draws[0].site == sim::DrawSite::SmallFarmAnimal);
    CHECK(out.draws[0].count == 1);
    CHECK(out.draws[1].site == sim::DrawSite::Ai);
    CHECK(out.draws[1].count == 2);
}

TEST_CASE("player seam draws and effects are counted in the advance telemetry") {
    const Kit k = make_kit();
    sim::Seams seams = named_seams();
    seams.player_groups.name = "groups";
    seams.player_groups.fn = [](void*, const sim::TickContext& tc, sim::SerialWorld&,
                                sim::Player&) {
        static_cast<void>(tc.draw.next(sim::DrawSite::Ai));
    };
    sim::World w = make_world_with(k, merge_scenario(k), seams);
    const std::uint32_t before = w.rng().cursor();
    w.advance(10);
    REQUIRE(w.last_output().draws.size() == 1);
    CHECK(w.last_output().draws[0].site == sim::DrawSite::Ai);
    CHECK(w.last_output().draws[0].count == 2); // players 0 and 1
    CHECK(w.last_output().rng_cursor_before == before);
    CHECK(w.last_output().rng_cursor_after == (before + 2) % 2048);
}

// --------------------------------------------------------------------- the views

TEST_CASE("islands view answers static facts and nothing for what is not there") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, building_test::kit_scenario(), named_seams());
    const sim::IslandsView view(w.islands());
    CHECK(view.count() == 4);
    CHECK(view.present(sim::IslandId{0}));
    CHECK_FALSE(view.present(sim::IslandId{1}));
    CHECK(view.present(sim::IslandId{3}));
    CHECK_FALSE(view.present(sim::IslandId{4}));
    CHECK_FALSE(view.present(sim::IslandId{}));
    CHECK(view.rect(sim::IslandId{0}) == sim::TileRect{100, 100, 64, 64});
    CHECK(view.rect(sim::IslandId{3}) == sim::TileRect{300, 200, 30, 20});
    CHECK(view.rect(sim::IslandId{1}) == sim::TileRect{});
    CHECK(view.climate(sim::IslandId{0}) == 2);
    CHECK(view.climate(sim::IslandId{3}) == 6);
    CHECK(view.climate(sim::IslandId{2}) == 0);
    CHECK(view.island_at(100, 100) == std::optional<sim::IslandId>{sim::IslandId{0}});
    CHECK(view.island_at(163, 163) == std::optional<sim::IslandId>{sim::IslandId{0}});
    CHECK_FALSE(view.island_at(164, 100).has_value());
    CHECK_FALSE(view.island_at(99, 100).has_value());
    CHECK(view.island_at(329, 219) == std::optional<sim::IslandId>{sim::IslandId{3}});
    CHECK_FALSE(view.island_at(330, 219).has_value());
    // A default view holds nothing.
    const sim::IslandsView none;
    CHECK(none.count() == 0);
    CHECK_FALSE(none.present(sim::IslandId{0}));
    CHECK(none.rect(sim::IslandId{0}) == sim::TileRect{});
    CHECK(none.climate(sim::IslandId{0}) == 0);
    CHECK_FALSE(none.island_at(100, 100).has_value());
}

TEST_CASE("players view has no money and answers totally") {
    const Kit k = make_kit();
    sim::ScenarioSpec spec = building_test::kit_scenario();
    spec.players[1].volk = 3;
    spec.players[1].research.assign(65, 0);
    spec.players[1].research[5] = 2;
    sim::World w = make_world_with(k, spec, named_seams());
    sim::WorldTestAccess::players(w).slots[0]->free_build = true;
    sim::WorldTestAccess::players(w).slots[1]->state = sim::PlayerState::Bankrupt;
    const sim::PlayersView view(w.players());
    CHECK(view.present(sim::PlayerId{0}));
    CHECK_FALSE(view.present(sim::PlayerId{2}));
    CHECK(view.volk(sim::PlayerId{0}) == 1);
    CHECK(view.volk(sim::PlayerId{1}) == 3);
    CHECK(view.volk(sim::PlayerId{2}) == 0);
    CHECK(view.researched(sim::PlayerId{1}, 5));
    CHECK_FALSE(view.researched(sim::PlayerId{1}, 6));
    CHECK_FALSE(view.researched(sim::PlayerId{1}, 5000));
    CHECK_FALSE(view.researched(sim::PlayerId{2}, 5));
    CHECK(view.free_build(sim::PlayerId{0}));
    CHECK_FALSE(view.free_build(sim::PlayerId{1}));
    CHECK_FALSE(view.free_build(sim::PlayerId{9}));
    CHECK(view.state(sim::PlayerId{0}) == sim::PlayerState::Alive);
    CHECK(view.state(sim::PlayerId{1}) == sim::PlayerState::Bankrupt);
    CHECK(view.state(sim::PlayerId{9}) == sim::PlayerState::Defeated);
    const sim::PlayersView none;
    CHECK_FALSE(none.present(sim::PlayerId{0}));
    CHECK(none.state(sim::PlayerId{0}) == sim::PlayerState::Defeated);
}

TEST_CASE("serial world answers the const queries of the whole state") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    sim::Effects fx;
    sim::Figure f;
    f.id = sim::FigureId{4};
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    const sim::SerialWorld sw(w.islands(), w.config().layout, w.figures(), w.players(), fx);
    CHECK(sw.island(sim::IslandId{0}) == &*w.islands()[0]);
    CHECK(sw.island(sim::IslandId{1}) == nullptr);
    CHECK(sw.island(sim::IslandId{99}) == nullptr);
    CHECK(sw.island(sim::IslandId{}) == nullptr);
    CHECK(sw.city(cref(0, 0)) == &*w.islands()[0]->cities[0]);
    CHECK(sw.city(cref(0, 5)) == nullptr);
    CHECK(sw.city(cref(2, 0)) == nullptr);
    CHECK(sw.find_building(bid(0, 2)) != nullptr);
    CHECK(sw.find_building(bid(0, 2)) == w.find_building(bid(0, 2)));
    CHECK(sw.find_building(bid(0, 99)) == nullptr);
    CHECK(sw.find_building(bid(8, 1)) == nullptr);
    // building_at and rect_query delegate to the island with the world's layout.
    CHECK(sw.building_at(cref(0, 0), 103, 101) == w.building_at(cref(0, 0), 103, 101));
    REQUIRE(sw.building_at(cref(0, 0), 103, 101) != nullptr);
    CHECK(sw.building_at(cref(0, 0), 103, 101)->id == bid(0, 2));
    CHECK(sw.building_at(cref(0, 0), 150, 150) == nullptr);
    CHECK(sw.building_at(cref(0, 0), 103, 101, 0x01) == nullptr); // the public list only
    CHECK(sw.building_at(cref(7, 0), 103, 101) == nullptr);
    std::vector<sim::BuildingId> ids{bid(9, 9)};
    sw.rect_query(cref(0, 0), sim::TileRect{100, 100, 10, 3}, ~0U, ids);
    std::vector<sim::BuildingId> want;
    w.rect_query(cref(0, 0), sim::TileRect{100, 100, 10, 3}, ~0U, want);
    CHECK(ids == want);
    CHECK(ids.size() == 4); // weaver, farm, small farm, bow maker
    sw.rect_query(cref(7, 0), sim::TileRect{100, 100, 10, 3}, ~0U, ids);
    CHECK(ids.empty());
    CHECK(sw.find_figure(sim::FigureId{4}) != nullptr);
    CHECK(sw.find_figure(sim::FigureId{5}) == nullptr);
    CHECK(&sw.players() == &w.players());
    CHECK(&sw.fx == &fx);
}

// ------------------------------------------------------------ the figure registry

TEST_CASE("figure registry creation follows the group list policy") {
    const sim::SimConfig cfg;
    sim::Rules rules;
    sim::FigureRegistry r = sim::make_figure_registry(cfg);
    CHECK(r.list.slots.size() == 1);
    CHECK(r.list.hint == -1);
    CHECK(r.list.cursor == 0);
    CHECK(r.stamp == 0);
    CHECK(r.expiry.empty());
    CHECK(r.deferred.empty());
    auto fig = [](std::uint64_t id) {
        sim::Figure f;
        f.id = sim::FigureId{id};
        return f;
    };
    // The first fits the fresh slot; the second grows the list 1 -> 10.
    CHECK(sim::add_figure(r, fig(1), cfg, rules) == 0);
    CHECK(r.list.slots.size() == 1);
    CHECK(sim::add_figure(r, fig(2), cfg, rules) == 1);
    CHECK(r.list.slots.size() == 10);
    CHECK(r.list.hint == 1);
    // An invalid id and a duplicate create nothing.
    CHECK(sim::add_figure(r, sim::Figure{}, cfg, rules) == sim::kNoSlot);
    CHECK(sim::add_figure(r, fig(2), cfg, rules) == sim::kNoSlot);
    CHECK(r.by_id.size() == 2);
    // The find helpers.
    REQUIRE(sim::find_figure(r, sim::FigureId{2}) != nullptr);
    CHECK(sim::find_figure(r, sim::FigureId{2})->id == sim::FigureId{2});
    const sim::FigureRegistry& cr = r;
    CHECK(sim::find_figure(cr, sim::FigureId{1}) == &*r.list.slots[0]);
    CHECK(sim::find_figure(cr, sim::FigureId{3}) == nullptr);
    // A stale map entry never answers.
    r.by_id[sim::FigureId{3}] = 5;
    CHECK(sim::find_figure(r, sim::FigureId{3}) == nullptr);
    r.by_id[sim::FigureId{3}] = 1; // the slot holds figure 2
    CHECK(sim::find_figure(r, sim::FigureId{3}) == nullptr);
    r.by_id[sim::FigureId{3}] = 700;
    CHECK(sim::find_figure(r, sim::FigureId{3}) == nullptr);
    r.by_id[sim::FigureId{3}] = static_cast<sim::Slot>(r.list.slots.size()); // one past the end
    CHECK(sim::find_figure(r, sim::FigureId{3}) == nullptr);
    // Rebuilding the map from the list restores it.
    sim::rebuild_figure_index(r);
    CHECK(r.by_id.size() == 2);
    CHECK(r.by_id.at(sim::FigureId{1}) == 0);
    CHECK(r.by_id.at(sim::FigureId{2}) == 1);
    // The configuration decides the first size and the growth.
    sim::SimConfig mod;
    mod.list_initial_size = 3;
    mod.list_min_growth = 20;
    sim::FigureRegistry m = sim::make_figure_registry(mod);
    CHECK(m.list.slots.size() == 3);
    for (std::uint64_t i = 1; i <= 4; ++i) {
        REQUIRE(sim::add_figure(m, fig(i), mod, rules) != sim::kNoSlot);
    }
    CHECK(m.list.slots.size() == 20);
}

TEST_CASE("a world's figure registry is sized by the configuration") {
    const Kit k = make_kit();
    CHECK(sim::World().figures().list.slots.size() == 1);
    CHECK(sim::World(5).figures().list.slots.size() == 1);
    sim::SimConfig cfg;
    cfg.list_initial_size = 3;
    sim::World w = make_world_with(k, merge_scenario(k), named_seams(), cfg);
    CHECK(w.figures().list.slots.size() == 3);
    CHECK(w.figures().list.hint == -1);
    CHECK(w.ids().next_figure == 1);
    CHECK(w.ids().next_event_seq == 1);
    CHECK(w.events().q.empty());
    // The same through the save frame.
    sim::Seams seams = named_seams();
    auto loaded = sim::World::load(w.save(), k.registry, seams);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().figures().list.slots.size() == 3);
}

TEST_CASE("counting draw counts per site, forwards every draw and reports the difference") {
    sim::WorldRng rng = *sim::WorldRng::from_table({11, 22, 33, 44, 55}, 0);
    sim::GlobalDraw global(rng);
    sim::CountingDraw draw(global);
    CHECK(draw.counts == std::array<std::uint32_t, sim::kDrawSiteCount>{});
    const auto before = draw.counts;
    CHECK(draw.next(sim::DrawSite::Ai) == 22);
    CHECK(draw.next(sim::DrawSite::Event300) == 33);
    CHECK(draw.next(sim::DrawSite::Ai) == 44);
    CHECK(draw.counts[static_cast<std::size_t>(sim::DrawSite::Ai)] == 2);
    CHECK(draw.counts[static_cast<std::size_t>(sim::DrawSite::Event300)] == 1);
    CHECK(draw.counts[static_cast<std::size_t>(sim::DrawSite::SmallFarmAnimal)] == 0);
    // A site that is not an enumerator is forwarded but counted nowhere.
    CHECK(draw.next(static_cast<sim::DrawSite>(200)) == 55);
    std::uint32_t total = 0;
    for (const std::uint32_t c : draw.counts) {
        total += c;
    }
    CHECK(total == 3);
    CHECK(rng.cursor() == 4);
    // The records between two readings: sites in enumerator order, none for no draw.
    const std::vector<sim::DrawRecord> between = sim::draw_records_between(before, draw.counts);
    REQUIRE(between.size() == 2);
    CHECK(between[0].site == sim::DrawSite::Event300);
    CHECK(between[0].count == 1);
    CHECK(between[1].site == sim::DrawSite::Ai);
    CHECK(between[1].count == 2);
    CHECK(sim::draw_records_between(draw.counts, draw.counts).empty());
    CHECK(sim::kDrawSiteCount == 7);
}

TEST_CASE("figure registry slot policy is the rules' slot policy") {
    const sim::SimConfig cfg;
    sim::Rules original;
    sim::Rules lowest;
    lowest.slot_policy = sim::SlotPolicy::LowestFree;
    auto fill = [&](const sim::Rules& rules) {
        sim::FigureRegistry r = sim::make_figure_registry(cfg);
        for (std::uint64_t i = 1; i <= 3; ++i) {
            sim::Figure f;
            f.id = sim::FigureId{i};
            sim::add_figure(r, f, cfg, rules);
        }
        r.list.slots[0].reset(); // hint is 2
        r.by_id.erase(sim::FigureId{1});
        sim::Figure f;
        f.id = sim::FigureId{4};
        return sim::add_figure(r, f, cfg, rules);
    };
    CHECK(fill(original) == 3); // the scan starts after the hint 2
    CHECK(fill(lowest) == 0);
}

TEST_CASE("figure registry validation names each broken fact") {
    const sim::SimConfig cfg;
    const sim::Rules rules;
    sim::IdCounters ids;
    ids.next_figure = 10;
    sim::FigureRegistry good = sim::make_figure_registry(cfg);
    for (std::uint64_t i = 1; i <= 3; ++i) {
        sim::Figure f;
        f.id = sim::FigureId{i};
        sim::add_figure(good, f, cfg, rules);
    }
    good.expiry = {sim::FigureId{2}};
    good.deferred = {sim::FigureId{3}, sim::FigureId{1}};
    good.list.slots[1]->created_stamp = 100; // on the expiry list
    good.list.slots[2]->marked_for_delete = true;
    good.list.slots[0]->marked_for_delete = true; // both on the deferred list
    REQUIRE(sim::validate_figures(good, ids) == "");
    struct Row {
        const char* name;
        void (*mutate)(sim::FigureRegistry&);
        const char* expect;
    };
    const std::vector<Row> rows = {
        {"no slot", [](sim::FigureRegistry& r) { r.list.slots.clear(); }, "has no slot"},
        {"hint below", [](sim::FigureRegistry& r) { r.list.hint = -2; }, "figures.list.hint"},
        {"hint beyond", [](sim::FigureRegistry& r) { r.list.hint = 10; }, "figures.list.hint"},
        {"cursor beyond", [](sim::FigureRegistry& r) { r.list.cursor = 10; },
         "figures.list.cursor"},
        {"id zero",
         [](sim::FigureRegistry& r) {
             r.by_id.erase(r.list.slots[0]->id);
             r.list.slots[0]->id = sim::FigureId{0};
             r.by_id[sim::FigureId{0}] = 0;
         },
         "slots[0].id"},
        {"id invalid", [](sim::FigureRegistry& r) { r.list.slots[1]->id = sim::FigureId{}; },
         "slots[1].id"},
        {"id not handed out",
         [](sim::FigureRegistry& r) { r.list.slots[2]->id = sim::FigureId{10}; }, "slots[2].id"},
        {"id twice", [](sim::FigureRegistry& r) { r.list.slots[2]->id = sim::FigureId{1}; },
         "used twice"},
        {"expiry unknown", [](sim::FigureRegistry& r) { r.expiry.push_back(sim::FigureId{7}); },
         "figures.expiry[1]"},
        {"deferred unknown", [](sim::FigureRegistry& r) { r.deferred[0] = sim::FigureId{7}; },
         "figures.deferred[0]"},
        {"expiry twice", [](sim::FigureRegistry& r) { r.expiry.push_back(sim::FigureId{2}); },
         "figures.expiry[1]: figure 2 is on the list twice"},
        {"deferred twice", [](sim::FigureRegistry& r) { r.deferred.push_back(sim::FigureId{3}); },
         "figures.deferred[2]: figure 3 is on the list twice"},
        {"marked but not deferred",
         [](sim::FigureRegistry& r) { r.list.slots[1]->marked_for_delete = true; },
         "slots[1].marked_for_delete: true but the figure is not on the deferred list"},
        {"deferred but not marked",
         [](sim::FigureRegistry& r) { r.list.slots[2]->marked_for_delete = false; },
         "slots[2].marked_for_delete: false but the figure is on the deferred list"},
        {"stamp without expiry", [](sim::FigureRegistry& r) { r.list.slots[0]->created_stamp = 5; },
         "slots[0].created_stamp: the figure has a stamp but is not on the expiry list"},
        {"expiry without stamp",
         [](sim::FigureRegistry& r) { r.list.slots[1]->created_stamp.reset(); },
         "slots[1].created_stamp: the figure has no stamp but is on the expiry list"},
        {"map differs", [](sim::FigureRegistry& r) { r.by_id[sim::FigureId{1}] = 2; },
         "figures.by_id"},
        {"map extra", [](sim::FigureRegistry& r) { r.by_id[sim::FigureId{9}] = 4; },
         "figures.by_id"},
    };
    for (const Row& row : rows) {
        INFO("row " << row.name);
        sim::FigureRegistry r = good;
        row.mutate(r);
        const std::string err = sim::validate_figures(r, ids);
        CHECK(contains(err, row.expect));
    }
    // The edges that are fine: the hint -1 and n - 1, the cursor n - 1, id next_figure - 1.
    sim::FigureRegistry edge = good;
    edge.list.hint = -1;
    edge.list.cursor = 9;
    CHECK(sim::validate_figures(edge, ids) == "");
    edge.list.hint = 9;
    CHECK(sim::validate_figures(edge, ids) == "");
    sim::IdCounters tight;
    tight.next_figure = 4;
    CHECK(sim::validate_figures(good, tight) == "");
    tight.next_figure = 3;
    CHECK(contains(sim::validate_figures(good, tight), "slots[2].id"));
}

// --------------------------------------------------- the stream: ids, figures, events

namespace {

/// The error of World::load with the seams of the saved world (empty on success).
std::string load_with(const Kit& k, const std::vector<std::uint8_t>& bytes, const sim::Seams& s) {
    auto r = sim::World::load(bytes, k.registry, s);
    return r.ok() ? std::string() : r.error();
}

} // namespace

TEST_CASE("ids figures and events scopes have the documented byte layout") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 11, 7, 0, 0), spawn(bid(0, 2), 22, 0, 0, 1)};
    sim::EnqueueEvent ev;
    ev.delay = 25;
    ev.e.kind = sim::EventKind::DelayedHit;
    ev.e.subtype = 2;
    ev.e.value = 9;
    ev.e.tile = sim::TileXY{-3, 4};
    ev.e.target = sim::FigureId{1};
    ev.e.attacker_kind = 2;
    ev.e.attacker = sim::FigureId{2};
    e.events = {ev};
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w);
    // The two figures refer to buildings whose counters were not raised: set them so the
    // world is consistent (the counters belong to the tick).
    payload<sim::Workshop>(w, 0, 1).carriers_out = 1;
    payload<sim::Farm>(w, 0, 2).workers_out = 1;
    REQUIRE(w.check_invariants() == "");
    sim::WriteArchive wa;
    fields(wa, std::as_const(w));
    const std::vector<std::uint8_t> bytes = wa.take();
    const std::size_t ids_at = island_test::flat_offset(w, "ids");
    const std::size_t players_at = island_test::flat_offset(w, "players");
    const std::size_t figures_at = island_test::flat_offset(w, "figures");
    const std::size_t events_at = island_test::flat_offset(w, "events");
    const std::size_t timers_at = island_test::flat_offset(w, "timers_monopoly");
    REQUIRE(ids_at != std::string::npos);
    // ids: u64 next_figure 3, u64 next_event_seq 2, u32 event750_acc 100 and u32
    // monopoly.acc 100 (the one advance of 100 ms; the refresh body ran on it), bool
    // faulted false.
    island_test::Stream ids;
    ids.u64(3);
    ids.u64(2);
    ids.u32(100);
    ids.u32(100);
    ids.u8(0);
    CHECK(players_at - ids_at == ids.v.size());
    CHECK(std::equal(ids.v.begin(), ids.v.end(),
                     bytes.begin() + static_cast<std::ptrdiff_t>(ids_at)));
    // figures: the list (i64 hint 1, u32 cursor 0, u64 slot count 10, per slot a presence
    // byte and the Figure), u16 stamp, the expiry ids, the deferred ids. Phase B of the
    // advance ran over the ten slots once (clamp(50 · 10 / 50, 1, 10) = 10 visits from
    // cursor 0: back at 0) and wrote the stamp 50.
    island_test::Stream figs;
    figs.u64(1);
    figs.u32(0);
    figs.u64(10);
    auto figure = [&](std::uint64_t id, std::uint32_t kind, std::uint32_t owner, std::uint64_t home,
                      std::uint32_t job) {
        figs.u8(1);
        figs.u64(id);
        figs.u32(kind);
        figs.u32(owner);
        figs.u64(home);
        figs.u32(0); // home_counter
        figs.u32(job);
        figs.i32(256 * 101 + 7);
        figs.i32(256 * 102);
        figs.u32(0);  // cargo_goods
        figs.u32(0);  // cargo_88
        figs.u32(0);  // hp
        figs.u32(50); // born
        figs.u8(0);   // created_stamp absent
        figs.u8(0);   // marked_for_delete
    };
    figure(1, 11, 0, bid(0, 1).v, 7);
    figure(2, 22, 1, bid(0, 2).v, 0);
    for (int i = 2; i < 10; ++i) {
        figs.u8(0);
    }
    figs.u16(50);
    figs.u64(0);
    figs.u64(0);
    CHECK(events_at - figures_at == figs.v.size());
    CHECK(std::equal(figs.v.begin(), figs.v.end(),
                     bytes.begin() + static_cast<std::ptrdiff_t>(figures_at)));
    // A corrupt byte in the second figure (its marked_for_delete: 20 bytes of list head,
    // the 59 bytes of slot 0, then 58 bytes into slot 1) is reported with the scope path
    // of that slot.
    const std::size_t marked_at = figures_at + 20 + 59 + 58;
    REQUIRE(bytes[marked_at] == 0);
    std::vector<std::uint8_t> bad = bytes;
    bad[marked_at] = 2;
    sim::World target = make_world_with(k, syn::empty_scenario(), script_seams(&script));
    CHECK(contains(island_test::read_flat(target, bad), "figures/figure[1] (byte"));
    // events: u64 count 1, the key (u64 due 75, u64 seq 1), kind, subtype, value, tile,
    // target, attacker kind, attacker.
    island_test::Stream evs;
    evs.u64(1);
    evs.u64(75);
    evs.u64(1);
    evs.u8(1);
    evs.u32(2);
    evs.u32(9);
    evs.i32(-3);
    evs.i32(4);
    evs.u64(1);
    evs.u32(2);
    evs.u64(2);
    CHECK(timers_at - events_at == evs.v.size());
    CHECK(std::equal(evs.v.begin(), evs.v.end(),
                     bytes.begin() + static_cast<std::ptrdiff_t>(events_at)));
    // The raw hash is the FNV of the stream and the trace sees the same bytes.
    sim::StateHasher fnv;
    fnv.bytes(bytes);
    sim::HashArchive raw(sim::HashOptions{nullptr, 0, true});
    fields(raw, std::as_const(w));
    CHECK(raw.finish() == fnv.value());
}

TEST_CASE("a world with figures and events saves loads and continues") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 11), spawn(bid(0, 2), 22)};
    sim::EnqueueEvent ev;
    ev.delay = 25;
    ev.e.tile = sim::TileXY{3, 4};
    e.events = {ev, ev};
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w);
    payload<sim::Workshop>(w, 0, 1).carriers_out = 1;
    payload<sim::Farm>(w, 0, 2).workers_out = 1;
    sim::WorldTestAccess::figures(w).expiry = {sim::FigureId{2}};
    sim::WorldTestAccess::figures(w).deferred = {sim::FigureId{1}};
    sim::WorldTestAccess::figures(w).stamp = 77;
    sim::WorldTestAccess::figures(w).list.cursor = 4;
    sim::WorldTestAccess::figures(w).list.slots[1]->created_stamp = 1234;
    sim::WorldTestAccess::figures(w).list.slots[0]->marked_for_delete = true;
    sim::WorldTestAccess::figures(w).list.slots[0]->cargo_goods = 9;
    sim::WorldTestAccess::figures(w).list.slots[0]->cargo_88 = 300;
    REQUIRE(w.check_invariants() == "");
    const std::vector<std::uint8_t> bytes = w.save();
    auto loaded = sim::World::load(bytes, k.registry, script_seams(&script));
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World back = std::move(loaded).value();
    CHECK(back.state_hash() == w.state_hash());
    CHECK(back.save() == bytes);
    CHECK(back.check_invariants() == "");
    CHECK(back.figures().by_id == w.figures().by_id);
    CHECK(back.figures().by_id.size() == 2);
    CHECK(back.figures().stamp == 77);
    CHECK(back.figures().expiry == std::vector<sim::FigureId>{sim::FigureId{2}});
    CHECK(back.figures().list.slots[1]->created_stamp == std::optional<std::uint16_t>{1234});
    CHECK(back.figures().list.slots[0]->marked_for_delete);
    CHECK_FALSE(back.figures().list.slots[1]->marked_for_delete);
    CHECK(back.figures().list.slots[0]->cargo_88 == 300);
    CHECK(back.events().q.size() == 2);
    CHECK(back.ids().next_figure == 3);
    CHECK(back.ids().next_event_seq == 3);
    // The flat stream reads back too, and both continue alike.
    sim::World flat = make_world_with(k, syn::empty_scenario(), script_seams(&script));
    CHECK(island_test::read_flat(flat, island_test::flat_stream(w)) == "");
    CHECK(flat.state_hash() == w.state_hash());
    for (int i = 0; i < 20; ++i) {
        w.advance(10);
        back.advance(10);
    }
    CHECK(back.state_hash() == w.state_hash());
}

TEST_CASE("ids figures and events reject hostile streams") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 11), spawn(bid(0, 2), 22)};
    sim::EnqueueEvent ev;
    ev.delay = 25;
    sim::EnqueueEvent ev2 = ev;
    ev2.delay = 30;
    e.events = {ev, ev2};
    script.calls = {e};
    sim::World good = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(good);
    payload<sim::Workshop>(good, 0, 1).carriers_out = 1;
    payload<sim::Farm>(good, 0, 2).workers_out = 1;
    REQUIRE(good.check_invariants() == "");
    struct Row {
        const char* name;
        void (*mutate)(sim::World&);
        const char* expect;
    };
    const std::vector<Row> rows = {
        {"next_figure 0", [](sim::World& w) { sim::WorldTestAccess::ids(w).next_figure = 0; },
         "next_figure"},
        {"next_event_seq 0", [](sim::World& w) { sim::WorldTestAccess::ids(w).next_event_seq = 0; },
         "next_event_seq"},
        {"figure id not handed out",
         [](sim::World& w) { sim::WorldTestAccess::ids(w).next_figure = 2; }, "slots[1].id"},
        {"event seq not handed out",
         [](sim::World& w) { sim::WorldTestAccess::ids(w).next_event_seq = 2; }, "next_event_seq"},
        {"duplicate figure id",
         [](sim::World& w) {
             sim::WorldTestAccess::figures(w).list.slots[1]->id = sim::FigureId{1};
         },
         "used twice"},
        {"hint beyond", [](sim::World& w) { sim::WorldTestAccess::figures(w).list.hint = 10; },
         "figures.list.hint"},
        {"cursor beyond", [](sim::World& w) { sim::WorldTestAccess::figures(w).list.cursor = 10; },
         "figures.list.cursor"},
        {"expiry unknown",
         [](sim::World& w) { sim::WorldTestAccess::figures(w).expiry = {sim::FigureId{9}}; },
         "figures.expiry[0]"},
        {"deferred unknown",
         [](sim::World& w) { sim::WorldTestAccess::figures(w).deferred = {sim::FigureId{9}}; },
         "figures.deferred[0]"},
    };
    for (const Row& row : rows) {
        INFO("row " << row.name);
        sim::World w = good;
        row.mutate(w);
        const std::string err = w.check_invariants();
        CHECK(contains(err, row.expect));
        // The stream reader refuses the same state with the same fact in its message.
        sim::World target = make_world_with(k, syn::empty_scenario(), script_seams(&script));
        const std::string flat = island_test::read_flat(target, island_test::flat_stream(w));
        CHECK(contains(flat, row.expect));
        CHECK(contains(load_with(k, w.save(), script_seams(&script)), row.expect));
    }
}

namespace {

/// The bytes of a slot list that claims `slots` empty slots (hint -1, cursor 0) and
/// carries them, one byte each.
std::vector<std::uint8_t> empty_slot_list(std::uint64_t slots) {
    std::vector<std::uint8_t> bytes;
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(0xff); // hint -1
    }
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(0); // cursor
    }
    for (int i = 0; i < 8; ++i) {
        bytes.push_back(static_cast<std::uint8_t>((slots >> (8 * i)) & 0xffU));
    }
    bytes.resize(bytes.size() + static_cast<std::size_t>(slots), 0);
    return bytes;
}

} // namespace

TEST_CASE("a slot list read is bounded by the memory it would take, not by its bytes") {
    // A slot is one byte on the wire but a whole optional<T> in memory: a 16 MB stream of
    // empty slots would allocate over a gigabyte (F35). The load budget refuses it before
    // any slot is made, for the figures and for the building lists.
    CHECK(sim::kSlotListLoadBytes / sizeof(std::optional<sim::Figure>) < sim::kListSizeBound);
    CHECK(sim::kSlotListLoadBytes / sizeof(std::optional<sim::Building>) < sim::kListSizeBound);
    {
        const std::vector<std::uint8_t> bytes = empty_slot_list(sim::kListSizeBound);
        sim::SlotList<sim::Figure> list;
        sim::ReadArchive r(bytes);
        fields(r, list);
        REQUIRE(r.failed());
        CHECK(contains(r.error(), "load budget"));
        CHECK(list.slots.empty());
    }
    {
        const std::vector<std::uint8_t> bytes = empty_slot_list(sim::kListSizeBound);
        sim::BuildingList list;
        sim::ReadArchive r(bytes);
        fields(r, list);
        REQUIRE(r.failed());
        CHECK(contains(r.error(), "load budget"));
        CHECK(list.slots.empty());
    }
    // A list within the budget reads as before.
    {
        const std::vector<std::uint8_t> bytes = empty_slot_list(1000);
        sim::SlotList<sim::Figure> list;
        sim::ReadArchive r(bytes);
        fields(r, list);
        CHECK_FALSE(r.failed());
        CHECK(list.slots.size() == 1000);
        CHECK(r.remaining() == 0);
    }
}

TEST_CASE("event queue stream refuses unordered keys and unknown kinds") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    sim::EnqueueEvent a;
    a.delay = 10;
    sim::EnqueueEvent b;
    b.delay = 20;
    e.events = {a, b};
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(w);
    std::vector<std::uint8_t> bytes = island_test::flat_stream(w);
    const std::size_t at = island_test::flat_offset(w, "events");
    auto target = [&] { return make_world_with(k, syn::empty_scenario(), script_seams(&script)); };
    REQUIRE(island_test::flat_offset(w, "timers_monopoly") - at == 8 + 2 * 53);
    // Swap the due ticks (60 and 70): the keys are no longer ascending.
    std::vector<std::uint8_t> swapped = bytes;
    swapped[at + 8] = 71;
    sim::World t1 = target();
    CHECK(contains(island_test::read_flat(t1, swapped), "strictly ascending"));
    // A kind that does not exist.
    std::vector<std::uint8_t> kind = bytes;
    kind[at + 8 + 16] = 2;
    sim::World t2 = target();
    CHECK(island_test::read_flat(t2, kind) != "");
    // The intact stream reads.
    sim::World t3 = target();
    CHECK(island_test::read_flat(t3, bytes) == "");
}

TEST_CASE("every field of the figure registry and the event queue moves the hash") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 11), spawn(bid(0, 2), 22)};
    sim::EnqueueEvent ev;
    ev.delay = 25;
    ev.e.kind = sim::EventKind::DelayedHit;
    ev.e.subtype = 2;
    ev.e.value = 9;
    ev.e.tile = sim::TileXY{3, 4};
    ev.e.target = sim::FigureId{1};
    ev.e.attacker_kind = 2;
    ev.e.attacker = sim::FigureId{2};
    e.events = {ev};
    script.calls = {e};
    sim::World base = make_world_with(k, merge_scenario(k), script_seams(&script));
    lap(base);
    payload<sim::Workshop>(base, 0, 1).carriers_out = 1;
    payload<sim::Farm>(base, 0, 2).workers_out = 1;
    const std::uint64_t h0 = base.state_hash();
    using FigMut = void (*)(sim::Figure&);
    const std::vector<std::pair<const char*, FigMut>> figure = {
        {"id", [](sim::Figure& f) { f.id = sim::FigureId{9}; }},
        {"kind", [](sim::Figure& f) { f.kind += 1; }},
        {"owner", [](sim::Figure& f) { f.owner = sim::PlayerId{5}; }},
        {"home", [](sim::Figure& f) { f.home = bid(0, 8); }},
        {"home_counter", [](sim::Figure& f) { f.home_counter = 3; }},
        {"job", [](sim::Figure& f) { f.job += 1; }},
        {"pos.x", [](sim::Figure& f) { f.pos.x += 1; }},
        {"pos.y", [](sim::Figure& f) { f.pos.y += 1; }},
        {"cargo_goods", [](sim::Figure& f) { f.cargo_goods += 1; }},
        {"cargo_88", [](sim::Figure& f) { f.cargo_88 += 1; }},
        {"hp", [](sim::Figure& f) { f.hp += 1; }},
        {"born", [](sim::Figure& f) { f.born += 1; }},
        {"created_stamp present",
         [](sim::Figure& f) { f.created_stamp = std::optional<std::uint16_t>(std::uint16_t{0}); }},
        {"marked_for_delete", [](sim::Figure& f) { f.marked_for_delete = true; }},
    };
    static_assert(sim::field_count<sim::Figure>() == 13);
    CHECK(figure.size() == 14); // the 13 members, the position being two scalars
    std::set<std::uint64_t> seen;
    for (const auto& [name, mutate] : figure) {
        INFO("figure " << name);
        sim::World w = base;
        mutate(*sim::WorldTestAccess::figures(w).list.slots[1]);
        const std::uint64_t h = w.state_hash();
        CHECK(h != h0);
        CHECK(seen.insert(h).second);
        sim::HashTree before, after;
        static_cast<void>(base.state_hash(before, 0));
        static_cast<void>(w.state_hash(after, 0));
        for (std::size_t i = 0; i < before.root.children.size(); ++i) {
            CHECK((before.root.children[i].hash != after.root.children[i].hash) ==
                  (before.root.children[i].name == "figures"));
        }
    }
    using RegMut = void (*)(sim::FigureRegistry&);
    const std::vector<std::pair<const char*, RegMut>> registry = {
        {"hint", [](sim::FigureRegistry& r) { r.list.hint = 0; }},
        {"cursor", [](sim::FigureRegistry& r) { r.list.cursor = 1; }},
        {"stamp", [](sim::FigureRegistry& r) { r.stamp = 1; }},
        {"expiry", [](sim::FigureRegistry& r) { r.expiry.push_back(sim::FigureId{1}); }},
        {"deferred", [](sim::FigureRegistry& r) { r.deferred.push_back(sim::FigureId{2}); }},
        {"slot removed", [](sim::FigureRegistry& r) { r.list.slots[0].reset(); }},
        {"slot added", [](sim::FigureRegistry& r) { r.list.slots.emplace_back(); }},
    };
    for (const auto& [name, mutate] : registry) {
        INFO("registry " << name);
        sim::World w = base;
        mutate(sim::WorldTestAccess::figures(w));
        CHECK(w.state_hash() != h0);
    }
    // The derived map is not hashed.
    sim::World mapped = base;
    sim::WorldTestAccess::figures(mapped).by_id[sim::FigureId{77}] = 3;
    CHECK(mapped.state_hash() == h0);
    // The event queue: its key and every field of the event.
    using EvMut = void (*)(sim::TimedEvent&);
    const std::vector<std::pair<const char*, EvMut>> event = {
        {"kind", [](sim::TimedEvent& t) { t.kind = sim::EventKind::TileCleanup; }},
        {"subtype", [](sim::TimedEvent& t) { t.subtype = 4; }},
        {"value", [](sim::TimedEvent& t) { t.value += 1; }},
        {"tile.x", [](sim::TimedEvent& t) { t.tile.x += 1; }},
        {"tile.y", [](sim::TimedEvent& t) { t.tile.y += 1; }},
        {"target", [](sim::TimedEvent& t) { t.target = sim::FigureId{3}; }},
        {"attacker_kind", [](sim::TimedEvent& t) { t.attacker_kind += 1; }},
        {"attacker", [](sim::TimedEvent& t) { t.attacker = sim::FigureId{3}; }},
    };
    static_assert(sim::field_count<sim::TimedEvent>() == 7);
    CHECK(event.size() == 8); // the tile is two scalars
    for (const auto& [name, mutate] : event) {
        INFO("event " << name);
        sim::World w = base;
        mutate(sim::WorldTestAccess::events(w).q.begin()->second);
        CHECK(w.state_hash() != h0);
    }
    sim::World key = base;
    auto node =
        sim::WorldTestAccess::events(key).q.extract(sim::WorldTestAccess::events(key).q.begin());
    node.key().first += 1;
    sim::WorldTestAccess::events(key).q.insert(std::move(node));
    CHECK(key.state_hash() != h0);
    sim::World seq = base;
    sim::WorldTestAccess::ids(seq).next_event_seq += 1;
    CHECK(seq.state_hash() != h0);
    sim::World nf = base;
    sim::WorldTestAccess::ids(nf).next_figure += 1;
    CHECK(nf.state_hash() != h0);
}

TEST_CASE("check_invariants compares the counters of figures out with the figures") {
    const Kit k = make_kit();
    Script script;
    sim::Effects e;
    e.spawns = {spawn(bid(0, 1), 11), spawn(bid(0, 3), 22, 0x2e, 2)};
    script.calls = {e};
    sim::World w = make_world_with(k, merge_scenario(k), script_seams(&script));
    // Figures exist but the counters are still 0: the invariant names the first building.
    lap(w);
    const std::string err = w.check_invariants();
    CHECK(contains(err, "islands[0].cities[0].index.lists[4].slots[0].payload"));
    CHECK(contains(err, "counter 0 of figures out is 0 but 1 figures refer to it"));
    payload<sim::Workshop>(w, 0, 1).carriers_out = 1;
    const std::string err2 = w.check_invariants();
    CHECK(contains(err2, "slots[2].payload"));
    CHECK(contains(err2, "counter 2 of figures out is 0 but 1 figures refer to it"));
    payload<sim::SmallFarm>(w, 0, 3).figures_out[2] = 1;
    CHECK(w.check_invariants() == "");
    // Too high a counter is just as wrong.
    payload<sim::SmallFarm>(w, 0, 3).figures_out[2] = 2;
    CHECK(contains(w.check_invariants(), "counter 2 of figures out is 2 but 1 figures"));
    payload<sim::SmallFarm>(w, 0, 3).figures_out[2] = 1;
    // A figure of a counter the building does not have is not counted by any counter, and
    // a figure whose home is gone is no error (the building was demolished).
    sim::WorldTestAccess::figures(w).list.slots[0]->home_counter = 1;
    CHECK(contains(w.check_invariants(), "counter 0 of figures out is 1 but 0 figures"));
    sim::WorldTestAccess::figures(w).list.slots[0]->home_counter = 0;
    REQUIRE(sim::WorldTestAccess::remove(w, {cref(0, 0), 4, 0}));
    CHECK(w.check_invariants() == "");
    // The kinds of building without counters never complain.
    sim::World plain = make_world_with(k, merge_scenario(k), named_seams());
    CHECK(plain.check_invariants() == "");
}

TEST_CASE("check_invariants rejects counters that cannot hand out ids and events from the future") {
    const Kit k = make_kit();
    sim::World w = make_world_with(k, merge_scenario(k), named_seams());
    sim::World a = w;
    sim::WorldTestAccess::ids(a).next_figure = 0;
    CHECK(contains(a.check_invariants(), "ids.next_figure: 0"));
    sim::World b = w;
    sim::WorldTestAccess::ids(b).next_event_seq = 0;
    CHECK(contains(b.check_invariants(), "ids.next_event_seq: 0"));
    // 2^64 - 1 is the used-up state: legal, and the counter hands out no more ids (F35).
    sim::World c = w;
    sim::WorldTestAccess::ids(c).next_figure = std::numeric_limits<std::uint64_t>::max();
    CHECK(c.check_invariants() == "");
    sim::World d = w;
    sim::WorldTestAccess::ids(d).next_event_seq = std::numeric_limits<std::uint64_t>::max();
    CHECK(d.check_invariants() == "");
    CHECK(load_with(k, c.save(), named_seams()) == "");
    CHECK(load_with(k, d.save(), named_seams()) == "");
    sim::World e = w;
    sim::TimedEvent te;
    sim::WorldTestAccess::events(e).q.emplace(sim::EventKey{10, 1}, te);
    CHECK(contains(e.check_invariants(), "events: the event (10, 1)"));
    sim::WorldTestAccess::ids(e).next_event_seq = 2;
    CHECK(e.check_invariants() == "");
}

// ----------------------------------------------------------- G1 through advance

namespace {

struct G1 {
    syn::SyntheticTypes types;
    sim::World world;
    static G1 make() {
        auto t = syn::make_synthetic_types();
        REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
        auto w = sim::World::create(syn::g1_scenario(t.value()), t.value().registry);
        REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
        return G1{std::move(t).value(), std::move(w).value()};
    }
};

const sim::Workshop& weaver_of(const sim::World& w) {
    return std::get<sim::Workshop>(w.islands()[0]->cities[0]->index.lists[4].slots[0]->payload);
}
const sim::Farm& farm_of(const sim::World& w) {
    return std::get<sim::Farm>(w.islands()[0]->cities[0]->index.lists[4].slots[1]->payload);
}

void check_golden_line(const std::string& name, std::uint64_t value) {
    std::string line;
    const std::string problem = anno::test::check_golden(name, "S7", value, line);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << line);
    }
    CHECK_MESSAGE(problem.empty(), problem);
}

} // namespace

TEST_CASE("G1 weaver and farm produce at the F2 and F6 rates through the real scheduler") {
    G1 g = G1::make();
    sim::World& w = g.world;
    REQUIRE(w.check_invariants() == "");
    // 20 000 steps of 10 ms (multiplayer style, 5 ticks each), a checkpoint every 5000
    // ticks (1000 steps).
    sim::StateHasher checkpoints;
    std::uint64_t t30000_out = 0;
    for (int step = 1; step <= 20000; ++step) {
        w.advance(10);
        if (step % 1000 == 0) {
            const std::uint64_t tick = w.clock().tick();
            checkpoints.u64(w.state_hash());
            // The farm never has more workers out than max_figures (1).
            CHECK(farm_of(w).workers_out <= 1);
            CHECK(weaver_of(w).carriers_out <= 1);
            // Independent checks at every checkpoint (the goldens are self-baselined):
            // the weaver turns 11 of the 1280 (5 t) of wool into 6 per step, one step per
            // 500 ticks from tick 805 until the wool is gone after 116 steps; the farm turns
            // 80 of its 1280 raw into 37 per step. Whatever the timing, nothing is lost.
            const std::uint64_t w_steps = weaver_of(w).out[0] / 6;
            CHECK(weaver_of(w).out[0] % 6 == 0);
            CHECK(weaver_of(w).in[0] + 11 * w_steps == 1280);
            const std::uint64_t w_expect = std::min<std::uint64_t>(116, (tick - 805) / 500 + 1);
            CHECK(w_steps + 3 >= w_expect);
            CHECK(w_steps <= w_expect + 3);
            const std::uint64_t f_steps = farm_of(w).out / 37;
            CHECK(farm_of(w).out % 37 == 0);
            CHECK(farm_of(w).raw + 80 * f_steps == 1280);
            if (tick == 30000) {
                t30000_out = weaver_of(w).out[0];
            }
            if (tick == 70000) {
                // F2: 116 full steps from 5 t of wool at need 11 and rate 6: out 696, in 4;
                // then "missing input" and blocked.
                CHECK(weaver_of(w).out[0] == 696);
                CHECK(weaver_of(w).in[0] == 4);
                CHECK(weaver_of(w).status.code == sim::Status::MissingInput);
                CHECK(weaver_of(w).blocked);
                // F6: 16 full steps from 5 t of raw material at need 80 and rate 37, then a
                // worker is sent that never returns (D12): out 592, raw 0, worker out.
                CHECK(farm_of(w).out == 592);
                CHECK(farm_of(w).raw == 0);
                CHECK(farm_of(w).workers_out == 1);
                CHECK(farm_of(w).blocked);
            }
        }
        if (step % 4000 == 0) {
            CHECK(w.check_invariants() == "");
        }
    }
    CHECK(w.clock().tick() == 100000);
    // At tick 30000 the weaver has done 60 +- 3 steps (6 per step): the scheduler may delay
    // a step but never loses or doubles one.
    CHECK(t30000_out / 6 >= 57);
    CHECK(t30000_out / 6 <= 63);
    CHECK(t30000_out % 6 == 0);
    // The production list has 10 slots (grown from 1 with the second insert), hint 1.
    const sim::BuildingList& production = w.islands()[0]->cities[0]->index.lists[4];
    CHECK(production.slots.size() == 10);
    CHECK(production.hint == 1);
    // The city is visited every 8th step (the ring: 2 of 16 slots per 5 ticks), at the ticks
    // 5 + 40 j; the last visit not after tick 100000 is j = 2499, tick 99965, whose low 16
    // bits are the shared stamp.
    const sim::City& c = *w.islands()[0]->cities[0];
    CHECK(c.index.stamp == 99965 - 65536);
    CHECK(c.tick_stamp == 99965 - 65536);
    // Two figures were sent: the weaver's carrier at its first step (tick 805: the weaver
    // sits in slot 0, which the city visits at 5, 405, 805), and the farm's worker once its
    // raw material was gone (the 17th step, around tick 8845).
    REQUIRE(w.figures().by_id.size() == 2);
    const sim::Figure& carrier = *sim::find_figure(w.figures(), sim::FigureId{1});
    const sim::Figure& worker = *sim::find_figure(w.figures(), sim::FigureId{2});
    CHECK(carrier.home == sim::make_building_id(sim::IslandId{0}, 1));
    CHECK(carrier.born == 805);
    CHECK(worker.home == sim::make_building_id(sim::IslandId{0}, 2));
    CHECK(worker.born >= 8000);
    CHECK(worker.born <= 9500);
    CHECK(carrier.owner == sim::PlayerId{0});
    CHECK(w.check_invariants() == "");
    // The UI events: a BlockedChanged for each building and nothing from the PRNG.
    const std::vector<sim::SimEvent> ui = w.drain_events();
    std::size_t blocked = 0;
    std::size_t spawned = 0;
    for (const sim::SimEvent& e : ui) {
        blocked += e.kind == sim::SimEventKind::BlockedChanged ? 1 : 0;
        spawned += e.kind == sim::SimEventKind::FigureSpawned ? 1 : 0;
    }
    CHECK(blocked == 2);
    CHECK(spawned == 2);
    CHECK(w.rng().cursor() == 1234); // no draw was made: no 300 s gate before tick 150000
    // Goldens (made by the C++; the semantic asserts above do not depend on them).
    check_golden_line("s7_g1_mp_checkpoints", checkpoints.value());
    check_golden_line("s7_g1_mp_end", w.state_hash());
    check_golden_line("s7_g1_events", hash_events(ui));
}

TEST_CASE("G1 with single player style steps of 1 to 100 ms") {
    G1 g = G1::make();
    sim::World& w = g.world;
    // A fixed LCG chooses the step of each of 2000 advances (1..100 ms).
    std::uint32_t x = 12345;
    for (int step = 0; step < 2000; ++step) {
        x = x * 1664525U + 1013904223U;
        w.advance(1 + (x >> 16) % 100);
        if (step % 500 == 499) {
            REQUIRE(w.check_invariants() == "");
        }
    }
    const std::uint64_t tick = w.clock().tick();
    CHECK(tick > 40000);
    CHECK(tick < 60000);
    // About tick / 500 steps of the weaver (it has not run out of wool yet), never more than
    // 3 off.
    const std::uint32_t steps = weaver_of(w).out[0] / 6;
    CHECK(weaver_of(w).out[0] % 6 == 0);
    CHECK(steps + 3 >= tick / 500);
    CHECK(steps <= tick / 500);
    check_golden_line("s7_g1_sp_end", w.state_hash());
}

TEST_CASE("G1 is deterministic and independent of junk allocations") {
    const G1 a = G1::make();
    G1 b = G1::make();
    G1 c = G1::make();
    sim::World wa = a.world;
    std::vector<std::unique_ptr<std::uint8_t[]>> junk;
    for (int i = 0; i < 3000; ++i) {
        wa.advance(10);
        b.world.advance(10);
        // Allocation noise between the steps of the second run.
        junk.push_back(std::make_unique<std::uint8_t[]>(static_cast<std::size_t>(16 + i % 97)));
        if (i % 7 == 0 && !junk.empty()) {
            junk.erase(junk.begin() + static_cast<std::ptrdiff_t>(junk.size() / 2));
        }
        CHECK(wa.state_hash() == b.world.state_hash());
    }
    // A copy taken half way continues alike.
    for (int i = 0; i < 1500; ++i) {
        c.world.advance(10);
    }
    sim::World half = c.world;
    for (int i = 0; i < 1500; ++i) {
        c.world.advance(10);
        half.advance(10);
    }
    CHECK(c.world.state_hash() == half.state_hash());
    CHECK(c.world.state_hash() == wa.state_hash());
}
