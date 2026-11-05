// MAP-NEEDS-2 N2: the sheep farm (a Farm-payload home whose figure type has the WEIDETIER route,
// job 0x2e) through the real scheduler. Contract: docs/sim/early-needs.md section 7; the arithmetic
// is the hand-derived vector file (tools/early_needs_ref.py). Every number is derived in the
// comment next to it. Nothing names the shipped sheep farm 807 or figure 103: the types are
// data-defined fixtures (harvest_helpers.hpp), the claims are about the figure type's route and the
// building's goods.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "harvest_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

struct Pasture {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::World w;
    sim::TransportStats stats;
    Pasture(std::shared_ptr<const sim::TypeRegistry> registry, sim::World world)
        : types(std::move(registry)), w(std::move(world)) {
        w.set_transport_stats(&stats);
    }
    Pasture(Pasture&& other) noexcept
        : types(std::move(other.types)), w(std::move(other.w)), stats(std::move(other.stats)) {
        w.set_transport_stats(&stats);
    }
    Pasture(const Pasture&) = delete;
    Pasture& operator=(const Pasture&) = delete;
    Pasture& operator=(Pasture&&) = delete;
};

using Tiles = std::vector<std::pair<std::int32_t, std::int32_t>>;

/// A farm of `farm` at (110, 110) on island 0 and ripe `tile` objects at the given tiles. Three
/// tiles at distance 4 from the centre (111, 111): inside the 11 x 11 square and the disc of
/// radius 5.
Pasture make_pasture(sim::TypeId farm, const Tiles& tiles, sim::TypeId tile = kGrass) {
    Rig r = make_rig();
    for (const auto& [x, y] : tiles) {
        r.object(x, y, tile);
    }
    r.place(farm, 110, 110, sim::Farm{});
    auto types = registry();
    return Pasture{types, create(r, types)};
}

const Tiles kThree{{115, 111}, {111, 115}, {107, 111}};

sim::Building* building_of(sim::World& w, sim::TypeId type) {
    sim::City& c = *sim::WorldTestAccess::islands(w)[0]->cities[0];
    for (auto& list : c.index.lists) {
        for (auto& b : list.slots) {
            if (b.has_value() && b->type == type) {
                return &*b;
            }
        }
    }
    return nullptr;
}

sim::Farm& farm_of(sim::World& w, sim::TypeId type) {
    sim::Building* b = building_of(w, type);
    REQUIRE(b != nullptr);
    return std::get<sim::Farm>(b->payload);
}

std::vector<sim::Figure*> animals(sim::World& w, std::uint32_t kind = kSheep) {
    std::vector<sim::Figure*> out;
    for (auto& f : sim::WorldTestAccess::figures(w).list.slots) {
        if (f.has_value() && f->kind == kind) {
            out.push_back(&*f);
        }
    }
    return out;
}

template <class Pred> bool tick_until(sim::World& w, Pred pred, int max_ticks) {
    for (int i = 0; i < max_ticks; ++i) {
        if (pred()) {
            return true;
        }
        w.advance(2);
    }
    return pred();
}

sim::Figure& fig_of(sim::World& w, sim::FigureId id) {
    sim::Figure* g = sim::find_figure(sim::WorldTestAccess::figures(w), id);
    REQUIRE(g != nullptr);
    return *g;
}

sim::FigureId first_animal(sim::World& w) {
    REQUIRE(tick_until(w, [&] { return !animals(w).empty(); }, 2000));
    return animals(w)[0]->id;
}

sim::BuildingKey key_of(const sim::World& w, sim::BuildingId id) {
    const sim::Island& is = *w.islands()[sim::island_of(id).v];
    return is.cache.by_id.at(id);
}

std::uint32_t object_at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[0];
    return is.tiles.object(is.tile_index(x, y));
}

void run_ticks(sim::World& w, int ticks) {
    for (int i = 0; i < ticks / 4; ++i) {
        w.advance(8);
    }
}

} // namespace

TEST_CASE(
    "sheep farm: the figure of route 46 is an animal with the harvest state, grazes, and Farm.raw "
    "gets 768") {
    // Farm (WOLLE from GRAS 6 / 87, one animal of 3 t) and three ripe grass tiles. The first
    // eligible farm step (tick 500) sends the animal with job 0x2e (the figure type's route), cargo
    // goods GRAS (the farm's input 0) and the harvest state owing the 5-tick first micro-step. It
    // cuts three tiles (capacity 768 > amount until the third cut) and delivers 768 once.
    Pasture p = make_pasture(kSheepFarm, kThree);
    sim::World& w = p.w;
    const sim::FigureId id = first_animal(w);
    const sim::Figure& g = fig_of(w, id);
    CHECK(g.job == 0x2e);
    CHECK(g.cargo_goods == goods_id("GRAS"));
    REQUIRE(g.harvest.has_value());
    CHECK(g.harvest->start_delay == 5);
    CHECK(w.check_invariants() == "");
    run_ticks(w, 60000);
    const sim::ForestryStats& fs = p.stats.forestry;
    CHECK(fs.cuts == 3);
    CHECK(fs.raw_credited_88 == 768);
    CHECK(fs.raw_delivered_88 == 768); // one delivery of the whole trip
    CHECK(fs.raw_lost_88 == 0);
    for (const auto& [x, y] : kThree) {
        CHECK(object_at(w, x, y) ==
              kGrassYoung.v); // id - 1, the grown-back stage is not in this fixture
    }
    // The farm turns 768 into 52 wool: 8 full steps (87 -> 6) and one partial (72 raw: q 105, used
    // 71, made 4); 767 consumed, 1 left (vector farmrun R-trip). Nothing else touches the store.
    const sim::Farm& f = farm_of(w, kSheepFarm);
    const auto& led = fs.farms.begin()->second;
    CHECK(led.consumed_88 == 767);
    CHECK(led.made_88 == 52);
    CHECK(f.raw == 1);
    CHECK(f.out == 52);
    CHECK(f.workers_out <= 1); // a starved step may have sent the next (empty-handed) animal
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "sheep farm: target exhaustion ends the trip with what it has, and an empty pasture credits "
    "nothing") {
    SUBCASE("two tiles: 512 credited, 35 wool") {
        // 512 raw: 5 full steps (435), 77 left: q 113, used 76, made 5 -> 35 wool, 1 raw left.
        Pasture p = make_pasture(kSheepFarm, {{115, 111}, {111, 115}});
        run_ticks(p.w, 60000);
        CHECK(p.stats.forestry.cuts == 2);
        CHECK(p.stats.forestry.raw_credited_88 == 512);
        CHECK(p.stats.forestry.raw_delivered_88 == 512);
        CHECK(p.stats.forestry.farms.begin()->second.made_88 == 35);
        CHECK(farm_of(p.w, kSheepFarm).raw == 1);
        CHECK(farm_of(p.w, kSheepFarm).workers_out == 0);
    }
    SUBCASE("no grass: no claim, no credit, the counter is released, the draws go on") {
        Pasture p = make_pasture(kSheepFarm, {});
        std::uint32_t draws = 0;
        for (int i = 0; i < 2500; ++i) {
            p.w.advance(8);
            draws += draws_at(p.w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        }
        CHECK(p.stats.forestry.cuts == 0);
        CHECK(p.stats.forestry.raw_credited_88 == 0);
        CHECK(p.stats.forestry.searches_failed > 0);
        CHECK(farm_of(p.w, kSheepFarm).raw == 0);
        CHECK(draws > 1); // every eligible attempt drew, one animal at a time
        CHECK(p.w.check_invariants() == "");
    }
}

TEST_CASE(
    "sheep farm: a send that creates no figure releases the counter, writes no cargo and still "
    "consumes exactly one draw per attempt") {
    // The figure id space is used up (F35, the refusal of add_figure that a registry state can
    // reach): the scheduler merge gives the counter back, no figure and no cargo exist, and every
    // farm step is an eligible attempt that drew once. (The original's other null paths - flag bits
    // 4/5 and a missing entrance - are not modelled by the spawn; vectors CR-bit4, CR-noent
    // document them.)
    Pasture p = make_pasture(kSheepFarm, kThree);
    sim::WorldTestAccess::ids(p.w).next_figure = std::numeric_limits<std::uint64_t>::max();
    std::uint32_t draws = 0;
    for (int i = 0; i < 2500; ++i) { // 10 000 ticks (advance(8) = 4 ticks)
        p.w.advance(8);
        draws += draws_at(p.w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        REQUIRE(farm_of(p.w, kSheepFarm).workers_out == 0);
    }
    const sim::Farm& f = farm_of(p.w, kSheepFarm);
    CHECK(f.last >= 9500); // about 19 steps of 500 ticks
    CHECK(draws == f.last / 500);
    CHECK(animals(p.w).empty());
    CHECK(p.stats.forestry.cuts == 0);
    CHECK(p.w.check_invariants() == "");
    // Control: the same farm with a usable id space sends an animal.
    Pasture q = make_pasture(kSheepFarm, kThree);
    CHECK(tick_until(q.w, [&] { return !animals(q.w).empty(); }, 2000));
}

TEST_CASE("sheep farm: a route the engine does not implement (FISCHER 45) is refused explicitly") {
    Pasture p = make_pasture(kFisherFarm, kThree);
    std::uint32_t draws = 0;
    for (int i = 0; i < 1250; ++i) {
        p.w.advance(8);
        draws += draws_at(p.w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        REQUIRE(farm_of(p.w, kFisherFarm).workers_out == 0);
    }
    const sim::Farm& f = farm_of(p.w, kFisherFarm);
    CHECK(animals(p.w, kFisher).empty());
    CHECK(draws == f.last / 500); // the draw is taken before the refusal
    CHECK(p.stats.forestry.unsupported_routes == draws);
    CHECK(p.w.check_invariants() == "");
}

TEST_CASE("sheep farm: the home is demolished on the way out, and while carrying") {
    for (const bool carrying : {false, true}) {
        INFO("carrying " << carrying);
        Pasture p = make_pasture(kSheepFarm, {{115, 111}});
        sim::World& w = p.w;
        const sim::FigureId id = first_animal(w);
        const sim::HarvestStep at = carrying ? sim::HarvestStep::ToHome : sim::HarvestStep::ToTree;
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).harvest->step == at; }, 14000));
        const sim::BuildingId home = fig_of(w, id).home;
        REQUIRE(sim::WorldTestAccess::remove(w, key_of(w, home)));
        CHECK(w.check_invariants() == "");
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).expiry_stamp.has_value(); }, 14000));
        CHECK(p.stats.forestry.raw_delivered_88 == 0);
        if (!carrying) {
            CHECK(p.stats.forestry.aborts == 1);
            CHECK(p.stats.forestry.cuts == 0);
            CHECK(object_at(w, 115, 111) == kGrass.v);
        } else {
            CHECK(p.stats.forestry.cuts == 1);
            CHECK(p.stats.forestry.raw_lost_88 == 256); // lost with the figure: no home to credit
            CHECK(object_at(w, 115, 111) == kGrassYoung.v);
        }
        CHECK(w.check_invariants() == "");
    }
}

namespace {

struct Ender {
    sim::FigureId victim;
};

void install_ender(sim::World& w, Ender* e) {
    auto& seam = sim::WorldTestAccess::seams(w).figure_tick;
    seam.name = "test_sheep_ender";
    seam.ctx = e;
    seam.fn = [](void* pv, const sim::TickContext& ctx, sim::SerialWorld& sw, sim::Figure& g) {
        if (g.id == static_cast<const Ender*>(pv)->victim) {
            sw.end_figure(g, ctx.t);
        }
    };
}

} // namespace

TEST_CASE(
    "sheep farm: the public end of an animal releases the counter once and a repeated end is a "
    "no-op") {
    for (const sim::HarvestStep at :
         {sim::HarvestStep::ToTree, sim::HarvestStep::Cut, sim::HarvestStep::ToHome}) {
        INFO("step " << static_cast<int>(at));
        Pasture p = make_pasture(kSheepFarm, {{115, 111}});
        sim::World& w = p.w;
        const sim::FigureId id = first_animal(w);
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).harvest->step == at; }, 14000));
        farm_of(w, kSheepFarm).switched_on = false; // no second animal
        Ender en{id};
        install_ender(w, &en);
        REQUIRE(farm_of(w, kSheepFarm).workers_out == 1);
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).expiry_stamp.has_value(); }, 200));
        CHECK(farm_of(w, kSheepFarm).workers_out == 0); // released once
        CHECK(fig_of(w, id).harvest->step == sim::HarvestStep::Done);
        {
            sim::Figure copy = fig_of(w, id);
            sim::Effects fx;
            CHECK_FALSE(sim::end_figure(copy, 12345, fx)); // repeated end: nothing
            CHECK(fx.releases.empty());
            CHECK(fx.tile_edits.empty());
        }
        run_ticks(w, 1200);
        CHECK(farm_of(w, kSheepFarm).workers_out == 0); // still once
        CHECK(w.check_invariants() == "");
        // The ended cargo is lost history: credited = delivered + lost.
        CHECK(p.stats.forestry.raw_credited_88 - p.stats.forestry.raw_delivered_88 -
                  p.stats.forestry.raw_lost_88 ==
              0);
    }
}

TEST_CASE("sheep farm: save and reload in every step resume like the uninterrupted run") {
    for (const sim::HarvestStep at : {sim::HarvestStep::Start, sim::HarvestStep::ToTree,
                                      sim::HarvestStep::Cut, sim::HarvestStep::ToHome}) {
        INFO("step " << static_cast<int>(at));
        Pasture p = make_pasture(kSheepFarm, {{115, 111}});
        sim::World& w = p.w;
        const sim::FigureId id = first_animal(w);
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).harvest->step == at; }, 14000));
        auto loaded = sim::World::load(w.save(), p.types);
        REQUIRE(loaded.ok());
        sim::World r = std::move(loaded).value();
        CHECK(r.state_hash() == w.state_hash());
        for (int i = 0; i < 2500; ++i) {
            w.advance(20);
            r.advance(20);
        }
        CHECK(r.state_hash() == w.state_hash());
        CHECK(r.save() == w.save());
    }
}

TEST_CASE("sheep farm: five animals share three tiles: every unit is accounted, none twice") {
    Pasture p = make_pasture(kSheepFarm5, kThree);
    sim::World& w = p.w;
    run_ticks(w, 80000);
    const sim::ForestryStats& fs = p.stats.forestry;
    CHECK(fs.cuts == 3); // three tiles, each cut once
    CHECK(fs.raw_credited_88 == 3 * 256);
    CHECK(fs.raw_credited_88 == fs.raw_delivered_88 + fs.raw_lost_88);
    CHECK(fs.raw_lost_88 == 0);
    const sim::Farm& f = farm_of(w, kSheepFarm5);
    const auto& led = fs.farms.begin()->second;
    CHECK(led.consumed_88 + f.raw == fs.raw_delivered_88); // the farm's raw store: nothing else
    CHECK(f.workers_out == 0);
    CHECK(fs.cut_mismatches == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "alternate configuration: a farm of other goods, recipe and figure id uses the same seam") {
    // KORN (rate 20, need 40) from GETREIDE grain tiles with an animal-route figure: 768 raw ->
    // 19 full steps (760) = 380 KORN, 8 raw left (q 25 < 64). Nothing names a sheep or 807.
    Pasture p = make_pasture(kBarnFarm, kThree, kGrain);
    REQUIRE(tick_until(p.w, [&] { return !animals(p.w, kBarnAnimal).empty(); }, 2000));
    CHECK(animals(p.w, kBarnAnimal)[0]->kind ==
          kBarnAnimal); // an id and goods of its own (not 103, not grass)
    run_ticks(p.w, 60000);
    CHECK(p.stats.forestry.cuts == 3);
    CHECK(p.stats.forestry.raw_delivered_88 == 768);
    const auto& led = p.stats.forestry.farms.begin()->second;
    CHECK(led.consumed_88 == 760);
    CHECK(led.made_88 == 380);
    CHECK(farm_of(p.w, kBarnFarm).raw == 8);
    for (const auto& [x, y] : kThree) {
        CHECK(object_at(p.w, x, y) == kStalks.v);
    }
    CHECK(p.w.check_invariants() == "");
}

TEST_CASE("sheep farm: a Farm-tick type whose figure has another route is neither refused nor "
          "harvesting") {
    // R7: route 51 (TRAEGER) is not 0x2d: the send is not refused (the counter is raised, a figure
    // with that job exists), it has no harvest state, and unsupported_routes stays 0.
    Pasture p = make_pasture(kOtherRouteFarm, kThree);
    REQUIRE(tick_until(p.w, [&] { return !animals(p.w, kOtherRoute).empty(); }, 2000));
    const sim::Figure& f = *animals(p.w, kOtherRoute)[0];
    CHECK(f.job == 51);
    CHECK_FALSE(f.harvest.has_value());
    CHECK(farm_of(p.w, kOtherRouteFarm).workers_out == 1);
    CHECK(p.stats.forestry.unsupported_routes == 0);
}

TEST_CASE("sheep farm: the animal limit 20 picks a different tile than the worker limit 0") {
    // R5: the synthetic walk tables cost 1 per step, so every goal's score stays below 20. The
    // animal goes on past each unclaimed goal (20 > score) and takes the LAST goal reached; the
    // worker (limit 0) takes the first. Same tiles, same farm data, only the figure's route
    // differs: the targets must differ.
    const Tiles two{
        {114, 111},
        {116, 111}}; // one near, one far from the farm entrance (outside the 3x3 footprint)
    const auto first_target = [&](sim::TypeId farm, std::uint32_t kind) {
        Pasture p = make_pasture(farm, two);
        sim::World& w = p.w;
        REQUIRE(tick_until(w, [&] { return !animals(w, kind).empty(); }, 2000));
        const sim::FigureId id = animals(w, kind)[0]->id;
        REQUIRE(tick_until(
            w, [&] { return fig_of(w, id).harvest->step == sim::HarvestStep::ToTree; }, 14000));
        return fig_of(w, id).harvest->target;
    };
    const sim::TileXY animal = first_target(kSheepFarm, kSheep);
    const sim::TileXY worker = first_target(kWorkerPasture, kWorker);
    CHECK(animal.x != worker.x);
    CHECK(worker.x == 114);
    CHECK(animal.x == 116);
}

TEST_CASE("sheep farm: the animal's first micro-step is paid across visits, then the search runs") {
    // The harvest state owes 5 ticks at the spawn; 1-tick visits pay them one at a time and nothing
    // is searched or claimed until the debt is paid (docs/sim/forestry.md E-21, the shared body).
    Pasture p = make_pasture(kSheepFarm, kThree);
    sim::World& w = p.w;
    const sim::FigureId id = first_animal(w);
    std::uint32_t last = fig_of(w, id).harvest->start_delay;
    REQUIRE(last == 5);
    int visits = 0;
    while (fig_of(w, id).harvest->start_delay > 0 && visits < 20) {
        w.advance(2); // one tick
        const std::uint32_t d = fig_of(w, id).harvest->start_delay;
        CHECK(d <= last);
        if (d > 0) {
            CHECK(fig_of(w, id).harvest->step == sim::HarvestStep::Start);
            for (const auto& [x, y] : kThree) {
                CHECK_FALSE(w.islands()[0]->tiles.claimed(w.islands()[0]->tile_index(x, y)));
            }
        }
        last = d;
        ++visits;
    }
    CHECK(fig_of(w, id).harvest->start_delay == 0);
    CHECK(visits >= 1);
}

TEST_CASE("sheep farm: a full output store, a raw store above the send bound and the reservation "
          "release") {
    SUBCASE("full output: the delivered raw is kept, nothing is made") {
        // Output at the 7 t capacity (1792): the step reports storage full and consumes nothing,
        // the raw piles up (vector F-full).
        Pasture p = make_pasture(kSheepFarm, kThree);
        farm_of(p.w, kSheepFarm).out = 1792;
        run_ticks(p.w, 60000);
        const sim::Farm& f = farm_of(p.w, kSheepFarm);
        CHECK(f.out == 1792);
        CHECK(f.raw == p.stats.forestry.raw_delivered_88);
        CHECK(p.stats.forestry.farms.begin()->second.made_88 == 0);
        CHECK(p.stats.forestry.raw_delivered_88 == 768);
    }
    SUBCASE("raw above the send bound: no animal is sent and no draw is taken") {
        // 384/6 = 64 < 5742/87 = 66 at the first step (vectors F-send-64/65): not eligible, so no
        // draw either.
        Pasture p = make_pasture(kSheepFarm, kThree);
        farm_of(p.w, kSheepFarm).raw = 5742 + 87;
        std::uint32_t draws = 0;
        for (int i = 0; i < 400; ++i) {
            p.w.advance(8);
            draws += draws_at(p.w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        }
        CHECK(draws == 0);
        CHECK(animals(p.w).empty());
        CHECK(p.stats.forestry.cuts == 0);
    }
}

TEST_CASE(
    "alternate configuration: another figure id and route (ERNTE 41, job 0x29) reaps the grain") {
    // A farm of other goods whose figure has route 41: the worker case (limit 0), another id,
    // another job.
    Pasture p = make_pasture(kBarnWorkerFarm, kThree, kGrain);
    REQUIRE(tick_until(p.w, [&] { return !animals(p.w, kBarnReaper).empty(); }, 2000));
    CHECK(animals(p.w, kBarnReaper)[0]->job == 0x29);
    REQUIRE(animals(p.w, kBarnReaper)[0]->harvest.has_value());
    run_ticks(p.w, 60000);
    CHECK(p.stats.forestry.cuts == 3);
    CHECK(p.stats.forestry.raw_delivered_88 == 768);
    const auto& led = p.stats.forestry.farms.begin()->second;
    CHECK(led.consumed_88 == 760);
    CHECK(led.made_88 == 380);
    CHECK(p.w.check_invariants() == "");
}

TEST_CASE("sheep farm: explicitly moved fixture credits destination statistics") {
    Pasture original = make_pasture(kSheepFarm, kThree);
    Pasture moved(std::move(original));
    REQUIRE(moved.w.transport_stats() == &moved.stats);
    REQUIRE(
        tick_until(moved.w, [&] { return moved.stats.forestry.raw_delivered_88 == 768; }, 100000));
    CHECK(moved.stats.forestry.cuts == 3);
    CHECK(moved.stats.forestry.raw_credited_88 == 768);
    CHECK(original.stats.forestry.raw_delivered_88 == 0);
}
