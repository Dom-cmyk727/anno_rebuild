// MAP-ECON-1 E2: the harvest worker's immutable facts, its persisted state, the claims and the farm
// dispatch draw (docs/sim/forestry.md 2, 2.1, 3.1, 3.6, 3.7; docs/re/forestry.md 1, 2.1, 2.2).
// Every expected value is derived in the comment next to it from the cited evidence, not from the
// C++.
#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "harvest_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

bool contains(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

sim::Farm& farm_at(sim::World& w, std::size_t island, sim::TypeId type) {
    sim::City& c = *sim::WorldTestAccess::islands(w)[island]->cities[0];
    for (auto& list : c.index.lists) {
        for (auto& b : list.slots) {
            if (b.has_value() && b->type == type) {
                return std::get<sim::Farm>(b->payload);
            }
        }
    }
    FAIL("no farm of type " << type.v);
    throw 0;
}

/// Advances 10 ms (5 ticks) at a time until `pred` holds, at most `max_advances` times; returns
/// the FarmWorkerDispatch draws seen on the way.
template <class Pred>
std::uint32_t advance_until(sim::World& w, Pred pred, int max_advances = 4000) {
    std::uint32_t draws = 0;
    for (int i = 0; i < max_advances && !pred(); ++i) {
        w.advance(10);
        draws += draws_at(w.last_output(), sim::DrawSite::FarmWorkerDispatch);
    }
    REQUIRE(pred());
    return draws;
}

/// The forester's worker (kWorker): the fixtures may hold a market's cart too.
sim::Figure& first_figure(sim::World& w) {
    for (auto& f : sim::WorldTestAccess::figures(w).list.slots) {
        if (f.has_value() && f->kind == kWorker) {
            return *f;
        }
    }
    FAIL("no figure");
    throw 0;
}

const sim::Figure* only_figure(const sim::World& w) {
    const auto all = figures(w);
    REQUIRE(all.size() == 1);
    return all[0];
}

} // namespace

TEST_CASE("harvest facts: the ROUTETYPE table names the jobs the transport already uses") {
    // AnnoGame 0x10024fb8 (read for E2): index 42 MAEHER, 41 ERNTE, 51 TRAEGER, 52 MARKTLER.
    CHECK(sim::routetype_by_name("MAEHER") == 0x2a);
    CHECK(sim::routetype_by_name("ERNTE") == 0x29);
    CHECK(sim::routetype_by_name("MARKTLER") == sim::kJobMarketCart);     // 52 = 0x34
    CHECK(sim::routetype_by_name("TRAEGER") == sim::kJobWorkshopCarrier); // 51 = 0x33
    CHECK(sim::routetype_by_name("WEIDETIER") == 46);
    CHECK(sim::routetype_by_name("UNUSED") == 0);
    CHECK(sim::routetype_by_name("") == 0);
    CHECK_FALSE(sim::routetype_by_name("FOERSTER").has_value()); // not in the table: a load error
    // The job factory's harvest constructor (0x1003b480 -> 0x1003b255) serves exactly 0x29, 0x2a.
    CHECK(sim::harvest_job(0x29));
    CHECK(sim::harvest_job(0x2a));
    CHECK_FALSE(sim::harvest_job(0x2b));
    CHECK_FALSE(sim::harvest_job(sim::kJobMarketCart));
    CHECK_FALSE(sim::harvest_job(0));
    CHECK(sim::kHarvestStartDelay == 5); // 0.001 / 0.001 = 1.0 time unit at 0.2 per tick
    CHECK(sim::kTileClaim == 0x20000000U);
}

TEST_CASE("harvest facts: forester 801, worker 102 and the tree table from the original files") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set (a required-asset skip, not a pass)");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    const sim::TypeRegistry& t = *loaded.types;
    const sim::GoodsId baum = *t.goods_by_name("BAUM");
    const sim::GoodsId holz = *t.goods_by_name("HOLZ");
    CHECK(baum == 70);
    CHECK(holz == 42);
    // docs/re/forestry.md 1.1 (P): PLANTAGE, 3 x 3, 150 gold, radius 5; PROD3: rate 14, need 32
    // (E-1), capacity 5 t = 1280, one figure type, max 2 out, figure 102.
    const sim::BuildingType* f = t.find(sim::TypeId{801});
    REQUIRE(f != nullptr);
    CHECK(f->prodtype == 2);
    CHECK(f->cls == sim::RuntimeClass::Farm);
    CHECK(f->width == 3);
    CHECK(f->height == 3);
    CHECK(f->cost_gold == 150);
    CHECK(f->action_radius == 5);
    CHECK(f->input_goods == std::vector<sim::GoodsId>{baum});
    REQUIRE(!f->output_goods.empty());
    CHECK(f->output_goods[0] == holz);
    CHECK(f->prod.output_per_step.at(0) == 14);
    CHECK(f->prod.input_per_step.at(0) == 32);
    CHECK(f->prod.capacity.at(0) == 1280);
    CHECK(f->prod.max_figures.at(0) == 2);
    CHECK(f->prod.figure_ids.at(0) == 102);
    // docs/re/forestry.md 1.2 (P): ROUTETYPE MAEHER (0x2a), speed 0.01 -> round(0.01f * 0.2f *
    // 2^24) = 33554, capacity LOGIC[0x11] = 1 -> 256, layer LOGIC[0x10] = 2, wait 13.0 -> 6500.
    const sim::FigureType* w = t.find_figure_type(102);
    REQUIRE(w != nullptr);
    CHECK(w->route == 0x2a);
    CHECK(w->speed_q24 == 33554);
    CHECK(w->capacity_88 == 256);
    CHECK(w->layer == 2);
    CHECK(w->load_ticks == 6500);
    // The other data: the market cart and the carrier carry their existing job codes as routes,
    // and all 18 MAEHER figures of the file get 0x2a (P, FigurenHeader.dat).
    CHECK(t.find_figure_type(220)->route == sim::kJobMarketCart);
    CHECK(t.find_figure_type(203)->route == sim::kJobWorkshopCarrier);
    std::uint32_t maeher = 0;
    for (const sim::FigureType& ft : t.figure_types()) {
        maeher += ft.route == 0x2a ? 1U : 0U;
    }
    CHECK(maeher == 18);
    // E-19 (P): the tree transitions derived by the registry rule WARE[0] = BAUM, then the cut's
    // predicate PRODTYPE(id - 1) = ROHSTWACHS (226), regrowing when the young type's stage count
    // HEAD[0x10] is above 1: 46 entries, 45 cuttable, 44 regrowing, 259 the stump, 261 uncuttable.
    std::uint32_t trees = 0, cuttable = 0, regrowing = 0;
    std::vector<std::uint32_t> stumps, uncuttable;
    for (const sim::BuildingType& bt : t.types()) {
        if (bt.prodtype != 225 || bt.output_goods.empty() || bt.output_goods[0] != baum) {
            continue;
        }
        ++trees;
        const sim::BuildingType* young = t.find(sim::TypeId{bt.id.v - 1});
        if (young == nullptr || young->prodtype != 226) {
            uncuttable.push_back(bt.id.v);
            continue;
        }
        ++cuttable;
        if (young->head_raw.at(0x10) > 1) {
            ++regrowing;
        } else {
            stumps.push_back(bt.id.v);
        }
    }
    CHECK(trees == 46);
    CHECK(cuttable == 45);
    CHECK(regrowing == 44);
    CHECK(stumps == std::vector<std::uint32_t>{259});
    CHECK(uncuttable == std::vector<std::uint32_t>{261});
}

TEST_CASE(
    "farm dispatch: the worker's job, cargo goods and fresh harvest state, one draw per send") {
    // A forester (rate 14, need 32, max 2, raw 0) sends at its first step: raw 0 -> ratio 0 < 64,
    // status 1, nothing made; 0 < 2 workers and 384/14 = 27 >= 0/32 -> send (E-2). The send draws
    // once before it (D 0x1006edf3) and spawns the worker with job = route 0x2a, cargo goods = the
    // farm's input 0 (BAUM, D 0x1006ee06) and the harvest state Start with the 5 ticks owed.
    // Two trees keep both workers out (E3: a treeless worker would return at once, E-20).
    Rig r = make_rig();
    r.object(115, 111, kTree);
    r.object(111, 115, kTree);
    r.place(kForester, 110, 110, sim::Farm{});
    sim::World w = create(r, registry());
    const std::uint32_t cursor0 = w.rng().cursor();
    sim::Farm& farm = farm_at(w, 0, kForester);
    const std::uint32_t draws = advance_until(w, [&] { return farm.workers_out == 1; });
    CHECK(draws == 1);
    CHECK(w.rng().cursor() == (cursor0 + 1) % w.rng().table().size());
    const sim::Figure* f = only_figure(w);
    CHECK(f->kind == kWorker);
    CHECK(f->job == 0x2a);
    CHECK(f->cargo_goods == goods_id("BAUM"));
    CHECK(f->cargo_88 == 0);
    REQUIRE(f->harvest.has_value());
    CHECK(f->harvest->step == sim::HarvestStep::Start);
    CHECK(f->harvest->start_delay == 5);
    CHECK(f->harvest->wait == 0);
    CHECK(f->harvest->route.empty());
    CHECK(f->transport.step == sim::TransportStep::Start); // the transport state is unused
    CHECK(w.check_invariants() == "");
    // The second step (500 ticks later) is eligible too (1 < 2 out): one more draw and worker.
    const std::uint32_t draws2 = advance_until(w, [&] { return farm.workers_out == 2; });
    CHECK(draws2 == 1);
    // With two out, no further step is eligible: no draw for a while (E-17's (0,0,2)); both are
    // busy for far longer (walk + 6500 ticks).
    std::uint32_t later = 0;
    for (int i = 0; i < 600; ++i) { // 3000 ticks: six farm steps
        w.advance(10);
        later += draws_at(w.last_output(), sim::DrawSite::FarmWorkerDispatch);
    }
    CHECK(later == 0);
    CHECK(farm.workers_out == 2);
    CHECK(figures(w).size() == 2);
}

TEST_CASE("farm dispatch: ineligible steps draw nothing, a full store and a refused send draw") {
    const auto types = registry();
    struct Case {
        const char* name;
        sim::Farm farm;
        bool exhaust_ids = false;
        std::uint32_t want_draws_per_step = 0; // over the first step
        std::uint32_t want_figures = 0;
    };
    sim::Farm off;
    off.switched_on = false;
    sim::Farm suspended;
    suspended.suspended = true;
    sim::Farm rich; // raw 928: ratio 128, consumes 32 -> 896, 896/32 = 28 > 27: no send (E-3)
    rich.raw = 928;
    sim::Farm lean; // raw 927 -> 895, 895/32 = 27 <= 27: send (E-3, the boundary's other side)
    lean.raw = 927;
    sim::Farm full; // out 1280 = the capacity: status 2, still an eligible attempt (E-5)
    full.out = 1280;
    const std::vector<Case> cases = {
        {"switched off", off, false, 0, 0},
        {"suspended", suspended, false, 0, 0},
        {"raw 928", rich, false, 0, 0},
        {"raw 927", lean, false, 1, 1},
        {"output full", full, false, 1, 1},
        // (two workers out: covered by the previous case, which reaches it with real figures)
        {"refused send (id space used up, F35)", sim::Farm{}, true, 1, 0},
    };
    for (const Case& c : cases) {
        INFO(std::string(c.name));
        Rig r = make_rig();
        r.place(kForester, 110, 110, c.farm);
        sim::World w = create(r, types);
        if (c.exhaust_ids) {
            // add_figure refuses a figure whose id is invalid: the merge releases the raised
            // counter again (F35). The attempt was eligible, so the draw happened all the same.
            sim::WorldTestAccess::ids(w).next_figure = std::numeric_limits<std::uint64_t>::max();
        }
        sim::Farm& farm = farm_at(w, 0, kForester);
        const std::uint16_t last0 = farm.last;
        std::uint32_t draws = 0;
        // Exactly one farm step: advance until `last` moves (the gate at 500 ticks).
        for (int i = 0; i < 400 && farm.last == last0; ++i) {
            w.advance(10);
            draws += draws_at(w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        }
        REQUIRE(farm.last != last0);
        CHECK(draws == c.want_draws_per_step);
        CHECK(figures(w).size() == c.want_figures);
        if (c.exhaust_ids) {
            CHECK(farm.workers_out == 0); // the F35 release
        }
    }
}

TEST_CASE("farm dispatch: the small farm's line-0 worker (job 0x2a) is a harvest worker carrying "
          "input 0") {
    // MAP-SUPPLY-1 S2. Job 0x2a is shared (docs/sim/forestry.md 2,
    // docs/sim/small-farm-supply.md 2.3): the small farm sends its line-0 worker with the literal
    // job 0x2a once line 0's raw stock is gone (here at once: the fixture's raw stock is empty).
    // Its home is a SmallFarm payload, and the spawn gives it the harvest state and the farm's
    // input goods 0 as cargo goods (D 0x1006f78d-0x1006f7c2).
    Rig r = make_rig();
    r.place(kSmallFarm, 110, 110, sim::make_small_farm(registry()->find(kSmallFarm)->prod));
    sim::World w = create(r, registry());
    const auto counter = [&]() -> std::uint32_t {
        for (const auto& list : w.islands()[0]->cities[0]->index.lists) {
            for (const auto& b : list.slots) {
                if (b.has_value() && b->type == kSmallFarm) {
                    return std::get<sim::SmallFarm>(b->payload).figures_out.at(0);
                }
            }
        }
        return 0;
    };
    advance_until(w, [&] { return counter() >= 1; }, 20000);
    const sim::Figure* worker = nullptr;
    for (const sim::Figure* f : figures(w)) {
        if (f->job == 0x2a) {
            worker = f;
        }
    }
    REQUIRE(worker != nullptr);
    CHECK(worker->kind == kWorker);
    REQUIRE(worker->harvest.has_value());
    CHECK(worker->cargo_goods == registry()->find(kSmallFarm)->input_goods.at(0));
    CHECK(w.check_invariants() == "");
}

TEST_CASE("farm dispatch: another data-defined harvester and owner, another route") {
    // Engine behaviour comes from the data: a grain farm of player 1 on island 1 whose figure type
    // has ROUTETYPE ERNTE (0x29) gets a harvest worker carrying GETREIDE; a farm whose figure has
    // the WEIDETIER route (46) sends a job-46 animal WITH a harvest state (MAP-NEEDS-2 N2: the
    // animal job 0x2e of a Farm home, limit 20); nothing names 801 or 102.
    Rig r = make_rig();
    r.place(kGrainFarm, 203, 103, sim::Farm{}, 1);
    r.place(kHerdFarm, 110, 110, sim::Farm{});
    sim::World w = create(r, registry());
    sim::Farm& grain = farm_at(w, 1, kGrainFarm);
    sim::Farm& herd = farm_at(w, 0, kHerdFarm);
    const std::uint32_t draws =
        advance_until(w, [&] { return grain.workers_out == 1 && herd.workers_out == 1; });
    CHECK(draws == 2);
    const auto all = figures(w);
    REQUIRE(all.size() == 2);
    const sim::Figure* reaper = all[0]->kind == kReaper ? all[0] : all[1];
    const sim::Figure* herder = all[0]->kind == kHerder ? all[0] : all[1];
    REQUIRE(reaper->kind == kReaper);
    REQUIRE(herder->kind == kHerder);
    CHECK(reaper->job == 0x29);
    CHECK(reaper->owner == sim::PlayerId{1});
    CHECK(reaper->cargo_goods == goods_id("GETREIDE"));
    REQUIRE(reaper->harvest.has_value());
    CHECK(reaper->harvest->start_delay == 5);
    CHECK(herder->job == 46);
    REQUIRE(
        herder->harvest.has_value()); // N2: the animal job of a Farm home carries the harvest state
    CHECK(herder->harvest->start_delay == 5);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("harvest state: save, load and save are byte-identical in every step, both rule sets") {
    for (const bool hygiene : {false, true}) {
        INFO("forestry_claim_hygiene " << hygiene);
        sim::Rules rules;
        rules.forestry_claim_hygiene = hygiene;
        Rig r = make_rig();
        r.object(115, 112, kTree);
        r.place(kForester, 110, 110, sim::Farm{});
        const auto types = registry();
        sim::World w = create(r, types, rules);
        sim::Farm& farm = farm_at(w, 0, kForester);
        advance_until(w, [&] { return farm.workers_out == 1; });
        REQUIRE(only_figure(w) != nullptr);
        sim::Figure& f = first_figure(w);
        REQUIRE(f.harvest.has_value());
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        const sim::Slot tree = is.tile_index(115, 112);
        const auto claim = [&](bool on) {
            is.tiles.set_aux(tree, on ? (is.tiles.aux(tree) | sim::kTileClaim)
                                      : (is.tiles.aux(tree) & ~sim::kTileClaim));
            if (hygiene) {
                if (on) {
                    is.claims[sim::TileXY{115, 112}] = f.id;
                } else {
                    is.claims.clear();
                }
            }
        };
        struct Shape {
            const char* name;
            sim::HarvestState h;
            std::uint32_t cargo;
            bool claimed;
        };
        sim::HarvestState delay;
        delay.start_delay = 3; // two ticks of the first delay consumed (E-21)
        sim::HarvestState out;
        out.step = sim::HarvestStep::ToTree;
        out.start_delay = 0;
        out.target = sim::TileXY{115, 112};
        out.route = {sim::FixedPos{111 * 256 + 213, 111 * 256 + 128},
                     sim::FixedPos{115 * 256 + 128, 112 * 256 + 128}};
        out.next = 1;
        out.along = 1000;
        sim::HarvestState cut;
        cut.step = sim::HarvestStep::Cut;
        cut.start_delay = 0;
        cut.target = sim::TileXY{115, 112};
        cut.wait = 4500;
        sim::HarvestState home;
        home.step = sim::HarvestStep::ToHome;
        home.start_delay = 0;
        home.route = {sim::FixedPos{115 * 256 + 128, 112 * 256 + 128},
                      sim::FixedPos{111 * 256 + 213, 111 * 256 + 128}};
        home.next = 1;
        const std::vector<Shape> shapes = {{"first delay", delay, 0, false},
                                           {"outbound", out, 0, true},
                                           {"cut wait", cut, 0, true},
                                           {"carrying", home, 256, false}};
        for (const Shape& s : shapes) {
            INFO(std::string(s.name));
            f.harvest = s.h;
            f.cargo_88 = s.cargo;
            claim(s.claimed);
            REQUIRE(w.check_invariants() == "");
            const std::vector<std::uint8_t> bytes = w.save();
            auto back = sim::World::load(bytes, types);
            REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
            CHECK(back.value().save() == bytes);
            CHECK(back.value().state_hash() == w.state_hash());
            const sim::Figure* g = only_figure(back.value());
            REQUIRE(g->harvest.has_value());
            CHECK(*g->harvest == s.h);
            CHECK(back.value().islands()[0]->tiles.claimed(tree) == s.claimed);
            CHECK(back.value().islands()[0]->claims == is.claims);
            // Every harvest field and the claim reach the hash (perturbations).
            const std::uint64_t h0 = w.state_hash();
            f.harvest->start_delay ^= 1U;
            CHECK(w.state_hash() != h0);
            f.harvest->start_delay ^= 1U;
            f.harvest->wait ^= 1U;
            CHECK(w.state_hash() != h0);
            f.harvest->wait ^= 1U;
            claim(!s.claimed);
            CHECK(w.state_hash() != h0);
            claim(s.claimed);
            CHECK(w.state_hash() == h0);
        }
    }
}

TEST_CASE("harvest state: validators refuse impossible states and never repair valid ones") {
    const auto types = registry();
    const auto make = [&](bool hygiene) {
        sim::Rules rules;
        rules.forestry_claim_hygiene = hygiene;
        Rig r = make_rig();
        r.object(115, 112, kTree);
        r.place(kForester, 110, 110, sim::Farm{});
        r.place(kMarket, 120, 120, std::nullopt);
        sim::World w = create(r, types, rules);
        sim::Farm& farm = farm_at(w, 0, kForester);
        advance_until(w, [&] { return farm.workers_out == 1; });
        REQUIRE(w.check_invariants() == "");
        return w;
    };
    const auto worker = [](sim::World& w) -> sim::Figure& { return first_figure(w); };
    const auto set_claim = [](sim::World& w, bool on) {
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        const sim::Slot i = is.tile_index(115, 112);
        is.tiles.set_aux(i, on ? (is.tiles.aux(i) | sim::kTileClaim)
                               : (is.tiles.aux(i) & ~sim::kTileClaim));
    };
    const auto to_tree = [](sim::Figure& f) {
        f.harvest->step = sim::HarvestStep::ToTree;
        f.harvest->start_delay = 0;
        f.harvest->target = sim::TileXY{115, 112};
    };
    // A load refuses what check_invariants refuses (save.cpp): checked on the save bytes too.
    const auto load_problem = [&](const sim::World& w) {
        auto back = sim::World::load(w.save(), types);
        return back.ok() ? std::string() : back.error();
    };

    SUBCASE("local facts of the job") {
        struct Bad {
            const char* name;
            void (*mutate)(sim::Figure&);
            const char* text;
        };
        const std::vector<Bad> bad = {
            {"harvest on another job", [](sim::Figure& f) { f.job = 0x33; },
             "only the harvest jobs"},
            {"delay above 5", [](sim::Figure& f) { f.harvest->start_delay = 6; }, "is above 5"},
            {"delay owed after a start",
             [](sim::Figure& f) {
                 f.harvest->step = sim::HarvestStep::ToHome;
                 f.harvest->start_delay = 2;
             },
             "ticks still owed"},
            {"delay owed with cargo", [](sim::Figure& f) { f.cargo_88 = 256; }, "ticks still owed"},
            {"Cut without a wait",
             [](sim::Figure& f) {
                 f.harvest->start_delay = 0;
                 f.harvest->step = sim::HarvestStep::Cut;
                 f.harvest->target = sim::TileXY{115, 112};
             },
             "a wait is owed exactly in Cut"},
            {"a wait outside Cut",
             [](sim::Figure& f) {
                 f.harvest->start_delay = 0;
                 f.harvest->wait = 7;
             },
             "a wait is owed exactly in Cut"},
            {"Done but alive",
             [](sim::Figure& f) {
                 f.harvest->start_delay = 0;
                 f.harvest->step = sim::HarvestStep::Done;
             },
             "has not ended"},
            {"route next out of range",
             [](sim::Figure& f) {
                 f.harvest->start_delay = 0;
                 f.harvest->step = sim::HarvestStep::ToHome;
                 f.harvest->route = {sim::FixedPos{1, 1}, sim::FixedPos{2, 2}};
                 f.harvest->next = 3;
             },
             "is not in 1 .. 2"},
        };
        for (const Bad& b : bad) {
            INFO(std::string(b.name));
            sim::World w = make(false);
            b.mutate(worker(w));
            const std::string p = w.check_invariants();
            CHECK(contains(p, b.text));
            CHECK(contains(load_problem(w), b.text));
        }
    }
    SUBCASE("an ended worker is Done: every end path finishes the job (CLOSE-1)") {
        // docs/sim/forestry.md 2.1: Done <=> ended. An ended figure (stamp, on the expiry list,
        // home none, its counter back) whose job is still on the way out is refused; the same
        // figure in Done is the valid control.
        sim::World w = make(false);
        sim::Figure& f = worker(w);
        to_tree(f);
        set_claim(w, true);
        REQUIRE(w.check_invariants() == "");
        f.home = sim::BuildingId{};
        f.expiry_stamp = 1;
        sim::WorldTestAccess::figures(w).expiry.push_back(f.id);
        farm_at(w, 0, kForester).workers_out = 0;
        CHECK(contains(w.check_invariants(), "but the figure has ended"));
        CHECK(contains(load_problem(w), "but the figure has ended"));
        f.harvest->step = sim::HarvestStep::Done;
        CHECK(w.check_invariants() == "");
        CHECK(load_problem(w) == "");
    }
    SUBCASE("a home that resolves must be a Farm; a demolished home is valid") {
        sim::World w = make(false);
        sim::Figure& f = worker(w);
        // The market is building serial 2 on island 0 (the forester is 1).
        const sim::BuildingId market = sim::make_building_id(sim::IslandId{0}, 2);
        REQUIRE(w.islands()[0]->find_building(market) != nullptr);
        const sim::BuildingId farm = f.home;
        f.home = market;
        CHECK(contains(w.check_invariants(), "is not a Farm- or SmallFarm-payload building"));
        // A home id that no longer resolves (demolished): valid in Start and on the way, never
        // repaired. (The counter fact only counts existing buildings.)
        f.home = sim::make_building_id(sim::IslandId{0}, 77); // never issued, resolves to nothing
        sim::WorldTestAccess::islands(w)[0]->next_serial = 78;
        farm_at(w, 0, kForester).workers_out = 0;
        CHECK(w.check_invariants() == "");
        to_tree(f);
        set_claim(w, true);
        CHECK(w.check_invariants() == "");
        const std::vector<std::uint8_t> bytes = w.save();
        auto back = sim::World::load(bytes, types);
        REQUIRE(back.ok());
        CHECK(back.value().save() == bytes);
        static_cast<void>(farm);
    }
    SUBCASE("faithful: any claim pattern loads and re-saves unchanged; no owner map") {
        sim::World w = make(false);
        sim::Figure& f = worker(w);
        // A stray bit (no worker targets the tile).
        set_claim(w, true);
        CHECK(w.check_invariants() == "");
        std::vector<std::uint8_t> bytes = w.save();
        auto back = sim::World::load(bytes, types);
        REQUIRE(back.ok());
        CHECK(back.value().save() == bytes);
        CHECK(back.value().islands()[0]->tiles.claimed(
            back.value().islands()[0]->tile_index(115, 112)));
        // A worker in ToTree whose target is unclaimed.
        set_claim(w, false);
        to_tree(f);
        CHECK(w.check_invariants() == "");
        bytes = w.save();
        back = sim::World::load(bytes, types);
        REQUIRE(back.ok());
        CHECK(back.value().save() == bytes);
        // An owner entry under the faithful rules is refused.
        sim::WorldTestAccess::islands(w)[0]->claims[sim::TileXY{115, 112}] = f.id;
        CHECK(contains(w.check_invariants(), "owner entries under the faithful rules"));
        // Another bit of aux above the height byte is refused (only bit 29 is modelled).
        sim::WorldTestAccess::islands(w)[0]->claims.clear();
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        is.tiles.set_aux(is.tile_index(115, 112), 0x40000000U);
        CHECK(contains(w.check_invariants(), "other than the claim bit 29"));
    }
    SUBCASE("enhanced: stray bits and orphan entries are refused, a lost claim (E-iii) loads") {
        sim::World w = make(true);
        sim::Figure& f = worker(w);
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        // A stray bit: refused (E-ii).
        set_claim(w, true);
        CHECK(contains(w.check_invariants(), "has no owner entry (a stray claim)"));
        CHECK(contains(load_problem(w), "a stray claim"));
        // The owned claim of a worker on its way: valid (E-i).
        to_tree(f);
        is.claims[sim::TileXY{115, 112}] = f.id;
        CHECK(w.check_invariants() == "");
        const std::vector<std::uint8_t> bytes = w.save();
        auto back = sim::World::load(bytes, types);
        REQUIRE(back.ok());
        CHECK(back.value().save() == bytes);
        // An entry whose owner does not target the tile: refused (E-i).
        f.harvest->target = sim::TileXY{116, 112};
        CHECK(contains(w.check_invariants(), "no live harvest worker in ToTree/Cut targeting"));
        f.harvest->target = sim::TileXY{115, 112};
        // An entry without the bit: refused.
        set_claim(w, false);
        CHECK(contains(w.check_invariants(), "an owner entry without the claim bit"));
        // E-iii: the worker still targets the tile, but its claim was invalidated (no bit, no
        // entry): a valid transitional state.
        is.claims.clear();
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("farm dispatch: a treeless forester sends, draws and releases at every step (E-20)") {
    // No tree in the square: the worker's first job run (5 ticks after its spawn) finds no
    // candidate, goes to step C inside the footprint (no walk), delivers 0 and ends; its end
    // releases the counter (D 0x1003af00 -> 0x1003b6e0), long before the next farm step 500 ticks
    // later, which is therefore eligible again: one draw and one send per step.
    Rig r = make_rig();
    r.place(kForester, 110, 110, sim::Farm{});
    sim::World w = create(r, registry());
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    sim::Farm& farm = farm_at(w, 0, kForester);
    std::uint32_t steps = 0, draws = 0;
    for (int i = 0; i < 1000; ++i) { // 5000 ticks: about ten farm steps
        const std::uint16_t last = farm.last;
        w.advance(10);
        steps += farm.last != last ? 1U : 0U;
        draws += draws_at(w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        REQUIRE(w.check_invariants() == "");
    }
    CHECK(steps >= 9);
    CHECK(draws == steps);
    CHECK(stats.forestry.searches_failed >= steps - 1); // the last one may still owe its delay
    CHECK(stats.forestry.cuts == 0);
    CHECK(farm.raw == 0);
    CHECK(farm.workers_out <= 1);
}

TEST_CASE("harvest state: hand-written bytes of a populated job and an ordered owner map") {
    // MAP-ECON-1-CLOSE-1 C2: the stream-126 layout of docs/sim/forestry.md 2 and docs/sim/state.md,
    // transcribed here byte by byte, independently of fields(): little-endian fixed widths, an enum
    // one byte, a sequence/map count u64, an optional one presence byte (archive.hpp's rules). The
    // values are nonzero and pairwise distinct so that a swapped, omitted or narrowed field moves
    // the bytes; they are a layout probe, not a valid job (a wait and a route never coexist).
    using Bytes = std::vector<std::uint8_t>;
    sim::HarvestState h;
    h.step = sim::HarvestStep::Cut; // 2
    h.start_delay = 0x01020304U;
    h.wait = 0x0A0B0C0DU;
    h.target = sim::TileXY{-3, 0x00112233};
    h.route = {sim::FixedPos{0x180, -0x102}, sim::FixedPos{0x7F000001, 0x5A00}};
    h.next = 1;
    h.along = 0x55667788U;
    const Bytes job = {
        0x02,                                           // step: Cut, u8
        0x04, 0x03, 0x02, 0x01,                         // start_delay u32 0x01020304
        0x0D, 0x0C, 0x0B, 0x0A,                         // wait u32 0x0A0B0C0D
        0xFD, 0xFF, 0xFF, 0xFF,                         // target.x i32 -3
        0x33, 0x22, 0x11, 0x00,                         // target.y i32 0x00112233
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // route: count u64 2
        0x80, 0x01, 0x00, 0x00,                         //   [0].x i32 0x180
        0xFE, 0xFE, 0xFF, 0xFF,                         //   [0].y i32 -0x102
        0x01, 0x00, 0x00, 0x7F,                         //   [1].x i32 0x7F000001
        0x00, 0x5A, 0x00, 0x00,                         //   [1].y i32 0x5A00
        0x01, 0x00, 0x00, 0x00,                         // next u32 1
        0x88, 0x77, 0x66, 0x55,                         // along u32 0x55667788
    };
    REQUIRE(job.size() == 1 + 4 + 4 + 8 + 8 + 2 * 8 + 4 + 4);
    sim::WriteArchive wa;
    fields(wa, std::as_const(h));
    CHECK(wa.data() == job);
    // The wrong transcriptions the assertion must reject: start_delay and wait swapped, `along`
    // omitted, the route count as a u32.
    Bytes swapped = job;
    std::swap_ranges(swapped.begin() + 1, swapped.begin() + 5, swapped.begin() + 5);
    const Bytes omitted(job.begin(), job.end() - 4);
    Bytes narrow = job;
    narrow.erase(narrow.begin() + 21, narrow.begin() + 25);
    CHECK(wa.data() != swapped);
    CHECK(wa.data() != omitted);
    CHECK(wa.data() != narrow);
    // The hand bytes read back to the same job, consuming exactly them.
    sim::HarvestState back;
    sim::ReadArchive r(job);
    fields(r, back);
    REQUIRE_MESSAGE(!r.failed(), r.error());
    CHECK(r.remaining() == 0);
    CHECK(back == h);
    // In a Figure: the optional is the last field, a presence byte 0 without the job and 1 then the
    // job's bytes with it (everything before it unchanged).
    sim::Figure fig;
    fig.id = sim::FigureId{0x21};
    fig.kind = kWorker;
    fig.job = 0x2a;
    fig.cargo_88 = 0x100;
    sim::WriteArchive without;
    fields(without, std::as_const(fig));
    fig.harvest = h;
    sim::WriteArchive with;
    fields(with, std::as_const(fig));
    Bytes expect = without.data();
    // The harvest presence byte is followed by the resident presence byte (stream 128, absent),
    // then, since route-only136, u64 freight_generation 0 and the freight presence byte 0
    // (docs/sim/state.md "Route-only136").
    const Bytes freight_tail(8 + 1, 0x00);
    REQUIRE(expect.size() >= 2 + freight_tail.size());
    REQUIRE(Bytes(expect.end() - static_cast<std::ptrdiff_t>(freight_tail.size()), expect.end()) ==
            freight_tail);
    expect.resize(expect.size() - freight_tail.size());
    REQUIRE(expect.back() == 0x00);
    REQUIRE(expect[expect.size() - 2] == 0x00);
    expect.pop_back();
    expect.back() = 0x01;
    expect.insert(expect.end(), job.begin(), job.end());
    expect.push_back(0x00); // resident absent
    expect.insert(expect.end(), freight_tail.begin(), freight_tail.end());
    CHECK(with.data() == expect);

    // Island::claims (enhanced owners): u64 count, then per entry i32 x, i32 y, u64 worker id, in
    // ascending TileXY order (x, then y). The keys are chosen so that y-first order differs.
    Rig rig = make_rig();
    rig.place(kForester, 110, 110, sim::Farm{});
    const sim::World w = create(rig, registry());
    sim::Island is = *w.islands()[0];
    REQUIRE(is.claims.empty());
    // Where the map sits in the island's stream: just before the scope "tiles" (the empty map is
    // its 8-byte count 0 there). The trace locates it; the bytes under test are hand-written.
    sim::TraceArchive tr;
    fields(tr, std::as_const(is));
    std::uint64_t off = 0;
    const sim::TraceEntry* prev = nullptr;
    bool found = false;
    for (const sim::TraceEntry& e : tr.entries()) {
        if (e.kind == sim::Prim::ScopeOpen && e.name == "tiles") {
            found = true;
            break;
        }
        off += e.width;
        prev = &e;
    }
    REQUIRE(found);
    REQUIRE(prev != nullptr);
    REQUIRE(prev->kind == sim::Prim::Count);
    REQUIRE(prev->value == 0);
    const Bytes empty = tr.bytes_written();
    REQUIRE(off >= 8);
    is.claims[sim::TileXY{4, 3}] = sim::FigureId{0x0A};
    is.claims[sim::TileXY{-2, 7}] = sim::FigureId{0x1112131415161718ULL};
    is.claims[sim::TileXY{4, -6}] = sim::FigureId{0x0000000200000001ULL};
    const Bytes owners = {
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // count u64 3
        0xFE, 0xFF, 0xFF, 0xFF, 0x07, 0x00, 0x00, 0x00, // (-2, 7)
        0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, //   worker 0x1112131415161718
        0x04, 0x00, 0x00, 0x00, 0xFA, 0xFF, 0xFF, 0xFF, // (4, -6)
        0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, //   worker 0x0000000200000001
        0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, // (4, 3)
        0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //   worker 0x0A
    };
    const auto splice = [&](const Bytes& middle) {
        Bytes out(empty.begin(), empty.begin() + static_cast<std::ptrdiff_t>(off - 8));
        out.insert(out.end(), middle.begin(), middle.end());
        out.insert(out.end(), empty.begin() + static_cast<std::ptrdiff_t>(off), empty.end());
        return out;
    };
    sim::WriteArchive full;
    fields(full, std::as_const(is));
    CHECK(full.data() == splice(owners));
    // Rejected transcriptions: y-first order (4, -6), (4, 3), (-2, 7); the id as a u32.
    Bytes y_first(owners.begin(), owners.begin() + 8);
    y_first.insert(y_first.end(), owners.begin() + 24, owners.end());
    y_first.insert(y_first.end(), owners.begin() + 8, owners.begin() + 24);
    Bytes id32 = owners;
    id32.erase(id32.begin() + 20, id32.begin() + 24);
    CHECK(full.data() != splice(y_first));
    CHECK(full.data() != splice(id32));
}
