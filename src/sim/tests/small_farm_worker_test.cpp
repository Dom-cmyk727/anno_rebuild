// MAP-SUPPLY-1 S2: the small farm's line-0 worker (job 0x2a) through the shared harvest mechanism
// (docs/sim/small-farm-supply.md 2.3, 6). Fixture: harvest_helpers.hpp's synthetic registry (the
// small farm 9330 takes GETREIDE and GRAS; 9331 takes KARTOFFEL and GRAS), nothing seeded: the raw
// and output stores start at zero, the fields are placed tile objects (the cut turns a ripe object
// into its young predecessor and nothing regrows in the fixture's horizon).
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "harvest_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

struct Farmland {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::World w;
    sim::TransportStats stats;
};

Farmland make(const std::function<void(Rig&)>& fill) {
    Rig r = make_rig();
    fill(r);
    auto types =
        registry(3 * 256); // the shipped small-farm worker carries 3 t (LOGIC byte 0x11 = 3)
    Farmland f{types, create(r, types), {}};
    f.w.set_transport_stats(&f.stats);
    return f;
}

const sim::SmallFarm& farm(const sim::World& w, std::size_t island, std::uint32_t serial) {
    const sim::Building* b = w.islands()[island]->find_building(
        sim::make_building_id(sim::IslandId{static_cast<std::uint32_t>(island)}, serial));
    REQUIRE(b != nullptr);
    return std::get<sim::SmallFarm>(b->payload);
}

std::uint32_t tile_object(const sim::World& w, std::size_t island, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[island];
    return is.tiles.object(is.tile_index(x, y));
}

template <class Pred> bool run_until(sim::World& w, Pred pred, int max_advances = 6000) {
    for (int i = 0; i < max_advances && !pred(); ++i) {
        w.advance(10);
    }
    return pred();
}

const sim::Figure* worker_of(const sim::World& w) {
    for (const auto& s : w.figures().list.slots) {
        if (s.has_value() && s->job == 0x2a && !s->expiry_stamp.has_value()) {
            return &*s;
        }
    }
    return nullptr;
}

/// Save, load, same hash; then both continue 2000 ticks and still agree.
void reload_agrees(const Farmland& f) {
    auto loaded = sim::World::load(f.w.save(), f.types);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == f.w.state_hash());
    CHECK(loaded.value().check_invariants() == "");
}

} // namespace

TEST_CASE("small farm worker: three ripe fields are cut, credited to raw 0 and consumed, the "
          "counter is released") {
    // The farm at (110,110) (3 x 3) has radius 3: the window x 108..114, y 108..114. Three GETREIDE
    // fields in it, outside its footprint (x 110..112, y 110..112). A trip is three cuts of 6500
    // ticks plus the walks.
    Farmland f = make([](Rig& r) {
        r.object(113, 111, kGrain);
        r.object(113, 112, kGrain);
        r.object(111, 113, kGrain);
        r.place(kSmallFarm, 110, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
    });
    std::uint32_t steps = 0;
    for (int i = 0; i < 12000; ++i) { // 120 000 ms = 60 000 ticks: 120 gate steps of 500 ticks
        f.w.advance(10);
        steps += draws_at(f.w.last_output(), sim::DrawSite::SmallFarmAnimal);
    }
    const sim::ForestryStats& fs = f.stats.forestry;
    CHECK(fs.cuts == 3);
    CHECK(fs.cuts_failed == 0);
    CHECK(fs.raw_credited_88 == 768);  // 3 cuts * 256
    CHECK(fs.raw_delivered_88 == 768); // the whole trip, to raw 0 (the potato slot)
    CHECK(fs.raw_lost_88 == 0);
    CHECK(fs.searches_failed >= 1); // the next worker finds only young fields
    for (const auto [x, y] : std::vector<std::pair<int, int>>{{113, 111}, {113, 112}, {111, 113}}) {
        CHECK(tile_object(f.w, 0, x, y) == kStalks.v); // id - 1, the ROHSTWACHS predecessor
    }
    const sim::SmallFarm& sf = farm(f.w, 0, 1);
    // 768 raw at 128 per full step: six steps of 13 = 78 made; nothing left, nothing lost
    CHECK(sf.raw[0] == 0);
    CHECK(sf.raw[1] == 0);
    CHECK(sf.out[0] == 78);
    CHECK(sf.figures_out[0] == 0); // released exactly once per worker
    CHECK(worker_of(f.w) == nullptr);
    // one SmallFarmAnimal draw per gate step with line 1 on: 60000 / 500 = 120 (the first gate runs
    // at tick 500; advance(10) is 5 ticks, so the 12000 advances end at tick 60000 and run 120
    // gates)
    CHECK(steps == 119); // measured: the last gate is not reached, buildings are visited on the
                         // island pass schedule
    CHECK(f.w.check_invariants() == "");
    reload_agrees(f);
}

TEST_CASE("small farm worker: a failed search ends the worker at home and credits nothing") {
    Farmland f = make([](Rig& r) {
        r.place(kSmallFarm, 110, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
    });
    REQUIRE(run_until(f.w, [&] { return farm(f.w, 0, 1).figures_out[0] >= 1; }));
    REQUIRE(run_until(f.w, [&] {
        return farm(f.w, 0, 1).figures_out[0] == 0 && f.stats.forestry.searches_failed >= 1;
    }));
    CHECK(f.stats.forestry.cuts == 0);
    CHECK(f.stats.forestry.raw_delivered_88 == 0);
    CHECK(farm(f.w, 0, 1).raw[0] == 0);
    CHECK(f.w.check_invariants() == "");
    reload_agrees(f);
}

TEST_CASE("small farm worker: the home destroyed with the worker out, in each phase") {
    for (const bool after_first_cut : {false, true}) {
        Farmland f = make([](Rig& r) {
            r.object(112, 111, kGrain);
            r.object(113, 112, kGrain);
            r.object(111, 113, kGrain);
            r.place(kSmallFarm, 110, 110,
                    sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
        });
        // phase 1: the worker has claimed a field and is walking or cutting (no cargo yet);
        // phase 2: it already carries one cut (256).
        if (after_first_cut) {
            REQUIRE(run_until(f.w, [&] {
                const sim::Figure* w = worker_of(f.w);
                return w != nullptr && w->cargo_88 >= 256;
            }));
        } else {
            REQUIRE(run_until(f.w, [&] {
                const sim::Figure* w = worker_of(f.w);
                return w != nullptr && w->harvest && w->harvest->step != sim::HarvestStep::Start;
            }));
        }
        const std::uint64_t carried = worker_of(f.w)->cargo_88;
        sim::Command demolish;
        demolish.issuer = sim::PlayerId{0};
        demolish.body = sim::DemolishCmd{sim::CityRef{sim::IslandId{0}, sim::CityNo{0}},
                                         {sim::TileXY{110, 110}}};
        REQUIRE(f.w.apply(demolish).ok());
        CHECK(f.w.check_invariants() == "");
        reload_agrees(f); // the worker outlives its home: a valid state
        REQUIRE(run_until(f.w, [&] { return worker_of(f.w) == nullptr; }));
        CHECK(f.stats.forestry.aborts + f.stats.forestry.raw_lost_88 > 0);
        CHECK(f.stats.forestry.raw_delivered_88 == 0);  // nothing reaches a farm that is gone
        CHECK(f.stats.forestry.raw_lost_88 == carried); // history, not stock
        // no claim is left on any field
        for (const auto [x, y] :
             std::vector<std::pair<int, int>>{{113, 111}, {113, 112}, {111, 113}}) {
            const sim::Island& is = *f.w.islands()[0];
            CHECK((is.tiles.aux(is.tile_index(x, y)) & sim::kTileClaim) == 0);
        }
        CHECK(f.w.check_invariants() == "");
        reload_agrees(f);
    }
}

TEST_CASE("small farm worker: two farms contend for two fields, each field is cut once") {
    Farmland f = make([](Rig& r) {
        r.object(113, 112, kGrain);
        r.object(113, 113, kGrain);
        r.place(kSmallFarm, 110, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
        r.place(kSmallFarm, 114, 110,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm)->prod));
    });
    REQUIRE(run_until(f.w, [&] { return f.stats.forestry.cuts == 2; }, 8000));
    REQUIRE(run_until(
        f.w, [&] { return worker_of(f.w) == nullptr || f.stats.forestry.raw_delivered_88 == 512; },
        8000));
    CHECK(f.stats.forestry.cuts == 2);
    CHECK(f.stats.forestry.cuts_failed == 0);
    CHECK(f.stats.forestry.raw_credited_88 == 512);
    CHECK(tile_object(f.w, 0, 113, 112) == kStalks.v);
    CHECK(tile_object(f.w, 0, 113, 113) == kStalks.v);
    CHECK(f.w.check_invariants() == "");
    reload_agrees(f);
}

TEST_CASE("small farm worker: another goods, type id and owner use the same behaviour") {
    // Island 1 belongs to player 1: a 9331 farm (ALKOHOL from KARTOFFEL) with two ripe potato
    // fields; the island-0 farm of player 0 has grain and must not be touched.
    Farmland f = make([](Rig& r) {
        r.object(208, 106, kPotato, 1);
        r.object(208, 107, kPotato, 1);
        r.object(113, 111, kGrain);
        r.place(kSmallFarm2, 205, 105,
                sim::make_small_farm(registry(3 * 256)->find(kSmallFarm2)->prod), 1);
    });
    REQUIRE(run_until(f.w, [&] { return f.stats.forestry.cuts >= 2; }, 8000));
    REQUIRE(run_until(f.w, [&] { return f.stats.forestry.raw_delivered_88 == 512; }, 12000));
    const sim::SmallFarm& sf = farm(f.w, 1, 1);
    CHECK(f.stats.forestry.raw_credited_88 == 512);
    CHECK(f.stats.forestry.raw_delivered_88 == 512);
    CHECK(sf.raw[1] == 0);
    CHECK(tile_object(f.w, 1, 208, 106) == kPotatoYoung.v);
    CHECK(tile_object(f.w, 1, 208, 107) == kPotatoYoung.v);
    CHECK(tile_object(f.w, 0, 113, 111) == kGrain.v); // the other owner's island: untouched
    // the worker is the farm's own: no figure of the other city carries its cargo
    for (const auto& s : f.w.figures().list.slots) {
        if (s.has_value() && s->job == 0x2a && !s->expiry_stamp.has_value()) {
            CHECK(sim::island_of(s->home).v == 1);
        }
    }
    CHECK(f.w.check_invariants() == "");
    reload_agrees(f);
}

#include "tutorial_town.hpp"

TEST_CASE("small farm worker on original data: the Tutorial scenario's island-7 farm cuts real "
          "potatoes") {
    // MAP-SUPPLY-1 S2, original scenario objects only (nothing seeded, no relocation): the small
    // farm 809 at (261,344) of island 7 (city 1, player 0) has 32 ripe potato objects (709) in its
    // 7 x 7 window (docs/sim/small-farm-supply.md 1). Rules::house_updates stays false.
    const auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World w = tutorial_town::tutorial_world(*o, false);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    sim::Island* isl = nullptr;
    for (auto& i : sim::WorldTestAccess::islands(w)) {
        isl = i && i->index.v == 7 ? &*i : isl;
    }
    REQUIRE(isl != nullptr);
    const sim::Building* fb = isl->building_at(w.config().layout, sim::CityNo{1}, 261, 344);
    REQUIRE(fb != nullptr);
    REQUIRE(fb->type.v == 809);
    const sim::BuildingId fid = fb->id;
    const auto count_potatoes = [&](std::uint32_t id) {
        std::uint32_t n = 0;
        for (int y = 342; y <= 348; ++y) {
            for (int x = 259; x <= 265; ++x) {
                n += isl->tiles.object(isl->tile_index(x, y)) == id ? 1U : 0U;
            }
        }
        return n;
    };
    const std::uint32_t ripe0 = count_potatoes(709), young0 = count_potatoes(708);
    CHECK(ripe0 == 32);
    const auto farm_now = [&]() -> const sim::SmallFarm& {
        const sim::Building* b = isl->find_building(fid);
        REQUIRE(b != nullptr);
        return std::get<sim::SmallFarm>(b->payload);
    };
    // the first worker's trip: three cuts, the credit to raw 0, then the line makes alcohol from it
    for (int i = 0; i < 6000 && farm_now().out[0] < 78; ++i) {
        w.advance(100);
    }
    const sim::SmallFarm& sf = farm_now();
    MESSAGE("tick " << w.clock().tick() << " ripe " << count_potatoes(709) << " young "
                    << count_potatoes(708) << " raw " << sf.raw[0] << "," << sf.raw[1] << " out "
                    << sf.out[0] << "," << sf.out[1] << " figs " << sf.figures_out[0]);
    // the cuts of THIS farm are the changes inside its window (the sink counts every farm of the
    // world)
    CHECK(count_potatoes(709) == ripe0 - 3);
    CHECK(count_potatoes(708) == young0 + 3);
    CHECK(sf.out[0] == 78); // 768 raw / 128 = 6 full steps of 13
    CHECK(sf.raw[0] == 0);
    CHECK(stats.forestry.raw_lost_88 == 0);
    CHECK(w.check_invariants() == "");
}
