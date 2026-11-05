// MAP-NEEDS-2 N3: sheep -> wool -> cloth -> a 1314 stall -> households on ORIGINAL data
// (docs/sim/early-needs.md sections 5, 7, 12). The slice: Tutorial/02Trade01.sze, island 7, city 1
// of player 0 (the SUPPLY-1 town). IMPORTED original producers (found by their goods, not by id):
// the two sheep farms (output WOLLE), the weaver (WOLLE -> STOFFE), the market halls, the food
// stall and 13 houses. The cloth stall is built by a PUBLIC BuildCmd of the stall type found by its
// goods (output STOFFE). WOLLE, STOFFE, LEDER and TIERHAEUTE are 0 in the store at tick 0
// (asserted); no figures exist at tick 0 (asserted). Rules::house_updates is on in the test world
// only. Every counter is scoped to this town's buildings; the other cloth town of the scenario
// (island 3, STOFFE 7680 and its own stall) is outside every sum.
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "cloth_town.hpp"
#include "island7_town.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
using namespace tutorial_town;
using namespace island7;
using namespace cloth_town;

} // namespace

TEST_CASE("cloth canary: the unmodified original, zero-start: new wool becomes new cloth and "
          "households buy it") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // Target: player 0, island 7, city 1. Imported producers: 2 sheep farms (output WOLLE), 1
    // weaver (WOLLE -> STOFFE); the stall type is built by BuildCmd (a public command). Starting
    // stocks (asserted in start_cloth): WOLLE, STOFFE, LEDER, TIERHAEUTE 0 in the store; no figure
    // exists; the 8 pioneer houses hold no cloth, the 5 settler houses hold 2,525-3,050.
    auto A = start_cloth(*o, true, 0, true);
    Cloth& c = *A;
    REQUIRE(c.stall_at.has_value());
    CHECK(c.stall_at->x == 306);
    CHECK(c.stall_at->y == 339);
    check_paid(c, *o->types->find(c.stall_type), *c.pay_before, *c.pay_after);
    std::cerr << "PIN start farm raw " << c.farm_raw0 << " out " << c.farm_out0 << " weaver in "
              << c.weaver_in0 << " out " << c.weaver_out0 << "\n";
    constexpr std::uint64_t kEnd = 300000;
    run_to(c, kEnd);
    CHECK(c.w.check_invariants() == "");
    const Ledger l = ledger_of(c, 0);
    // ---- every unexplained term is zero ----
    CHECK(l.u_farm == 0); // wool made = still in the farms + taken from them
    CHECK(l.u_wool == 0); // wool taken = weaver input consumed + still in the weaver + in carriers'
                          // hands + in the store
    CHECK(l.u_cloth_made == 0); // cloth made = still in the weaver + taken from it
    CHECK(l.u_cloth == 0);      // cloth taken = on carts + in the store + bought by households
    CHECK(l.cart_lost == 0);    // no legitimate loss occurred (no clip of a full store)
    // the grass: every cut of the 35 candidate tiles credited 256 raw; the farms hold or consumed
    // all of it (the rest is in the animals' hands, and a worker that ended without delivery would
    // show up as a positive loss term)
    const Regrowth g = regrowth_of(c);
    const std::int64_t grass_credited = 256 * std::int64_t(g.cut_events);
    const std::int64_t grass_lost =
        grass_credited + std::int64_t(c.farm_raw0) -
        std::int64_t(l.raw_consumed + l.raw_in_farms + grass_in_hands(c));
    CHECK(grass_lost >= 0);
    CHECK(grass_lost <= std::int64_t(c.stats.forestry.raw_lost_88));
    // ---- measured pins (labelled: this town, this schedule of advance(100) steps) ----
    std::cerr << "PIN grass credited " << grass_credited << " consumed " << l.raw_consumed
              << " in farms " << l.raw_in_farms << " hands " << grass_in_hands(c)
              << " tiles tracked " << c.last_obj.size() << " world raw_credited "
              << c.stats.forestry.raw_credited_88 << " delivered "
              << c.stats.forestry.raw_delivered_88 << " lost " << c.stats.forestry.raw_lost_88
              << "\n";
    std::cerr << "PIN first_store " << c.first_cloth_store_tick << " wool_made " << l.wool_made
              << " weaver_made " << l.weaver_made << " consumed " << l.weaver_consumed << " store "
              << l.cloth_store << " taken " << l.cloth_taken << " out " << l.weaver_out
              << " grass_lost " << grass_lost << " cuts " << g.cut_events << " tiles "
              << g.tiles_cut << " regrow n " << g.n << " min " << g.min << " max " << g.max << "\n";
    // ---- who gets it: derived from the scene, nothing forced ----
    std::uint32_t pioneers_zero = 0, pioneers_rose = 0, others_rose = 0;
    for (const auto& [id, hw] : c.houses) {
        if (hw.stoffe0 == 0) {
            ++pioneers_zero;
            pioneers_rose += hw.rose ? 1U : 0U;
        } else {
            others_rose += hw.rose ? 1U : 0U;
        }
    }
    std::cerr << "PIN houses zero-start " << pioneers_zero << " rose " << pioneers_rose
              << " others rose " << others_rose << " total " << c.houses.size() << "\n";
    CHECK(c.houses.size() == 13);
    CHECK(pioneers_zero == 8);
    CHECK(pioneers_rose == 8); // every house that held no cloth got some from the new supply
    CHECK(others_rose == 1);
    CHECK(houses_reached(c) == 9);
    CHECK(c.first_cloth_store_tick == 99750);
    CHECK(l.wool_made == 3187);
    CHECK(l.weaver_consumed == 2761);
    CHECK(l.weaver_made == 1506);
    CHECK(l.weaver_out == 72);
    CHECK(l.cloth_loaded == 1434);
    CHECK(l.cloth_store == 348);
    CHECK(l.cloth_taken == 1086);
    CHECK(grass_lost == 0);
    CHECK(g.cut_events == 188);
    CHECK(g.tiles_cut == 35);
    // Renewal: every cut tile grew back, after 43,950 to 49,400 ticks (measured over 156 regrowths;
    // the nominal `S << k` flips of P/2 = 30,000 ticks are stretched by the chunk visits,
    // docs/re/production.md 4.3). The pool lasts through repeated cycles of the same 35 tiles
    // inside this run; no claim beyond 300,000 ticks.
    CHECK(g.n == 156);
    CHECK(g.min == 43950);
    CHECK(g.max == 49400);
}

TEST_CASE(
    "cloth causal control: labelled stock in both arms, the producers removed in the control") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // LABELLED setup, named as such: 2 t of cloth (512 raw) written into the store in the two arms
    // that carry it. The unmodified zero-start proof is the previous test case. None of this is a
    // ship-start demonstration.
    constexpr std::uint32_t kS0 = 512;
    constexpr std::uint64_t kEnd = 200000;
    auto B = start_cloth(*o, true, kS0, true); // treatment + labelled stock
    auto C = start_cloth(*o, true, kS0,
                         false); // control (the two sheep farms and the weaver demolished) + stock
    auto D = start_cloth(*o, true, 0, false); // control WITHOUT stock: vacuous
    auto E = start_cloth(
        *o, false, 0,
        true); // treatment WITHOUT the public stall build: the new cloth reaches no house
    run_to(*B, kEnd);
    run_to(*C, kEnd);
    run_to(*D, kEnd);
    run_to(*E, kEnd);
    std::cerr << "PIN B taken " << taken(*B) << " store " << B->city->stock.stock[kStoffe]
              << " C taken " << taken(*C) << " store " << C->city->stock.stock[kStoffe]
              << " houses " << houses_reached(*C) << " D taken " << taken(*D) << "\n";
    // the control demonstrably runs and consumes the whole labelled stock, then stops (nothing
    // comes in)
    CHECK(taken(*C) == kS0);
    CHECK(C->city->stock.stock[kStoffe] == 0);
    CHECK(houses_reached(*C) >= 2);
    CHECK(C->w.check_invariants() == "");
    // the treatment buys beyond that stock: the surplus is the new supply; the benefit is the
    // difference
    CHECK(taken(*B) > kS0);
    CHECK(taken(*B) > taken(*C));
    CHECK(taken(*B) == 1017); // measured pins: 505 units bought beyond the labelled stock
    CHECK(taken(*B) - taken(*C) == 505);
    CHECK(B->city->stock.stock[kStoffe] == 210);
    CHECK(houses_reached(*C) == 6);
    const Ledger lb = ledger_of(*B, kS0);
    CHECK(lb.u_farm == 0);
    CHECK(lb.u_wool == 0);
    CHECK(lb.u_cloth_made == 0);
    CHECK(lb.u_cloth == 0);
    CHECK(lb.cart_lost == 0);
    // a control with no stock is vacuous: nothing to buy, nobody reached: why the labelled stock
    // exists
    CHECK(taken(*D) == 0);
    CHECK(houses_reached(*D) == 0);
    CHECK(D->city->stock.stock[kStoffe] == 0);
    // without the public stall build the same producers still make cloth (it reaches the store) but
    // no house gets any: the only stall type in the town sells food and salt, so the BuildCmd of
    // the cloth stall is what connects them
    CHECK(E->city->stock.stock[kStoffe] > 0);
    CHECK(taken(*E) == 0);
    CHECK(houses_reached(*E) == 0);
}

TEST_CASE(
    "cloth interruption and recovery: the weaver demolished, then rebuilt by public commands") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto R = start_cloth(*o, true, 0, true);
    Cloth& c = *R;
    constexpr std::uint64_t T1 = 200000, T2 = 280000, T3 = 400000;
    run_to(c, T1);
    const sim::Building* wb = c.isl->find_building(c.weaver);
    REQUIRE(wb != nullptr);
    const sim::TileXY at{wb->x, wb->y};
    const std::uint8_t rot = wb->rotation;
    const sim::TypeId wtype = wb->type;
    const sim::BuildingId old_weaver = c.weaver;
    const sim::Workshop& ow = std::get<sim::Workshop>(wb->payload);
    const std::uint64_t old_out_lost = ow.out[0], old_in_lost = ow.in[0]; // go with the building
    const std::uint64_t taken1 = taken(c), store1 = c.city->stock.stock[kStoffe];
    const auto old_ledger = c.stats.forestry.workshops.at(old_weaver);
    // wool the old weaver's carriers hold at the moment of the demolition: lost with them (the
    // carriers end with their home)
    std::uint64_t carrier_wool_lost = 0;
    for (const auto& f : c.w.figures().list.slots) {
        if (f && !f->expiry_stamp.has_value() && f->home == old_weaver &&
            f->cargo_goods == kWolle) {
            carrier_wool_lost += f->cargo_88;
        }
    }
    REQUIRE(c.w.apply(cmd(c, sim::DemolishCmd{c.ref, {at}})).ok());
    CHECK(c.isl->find_building(old_weaver) == nullptr);
    run_to(c, T2);
    // the gap: the old weaver's ledger is frozen, the households buy the remaining store and then
    // nothing more
    CHECK(c.stats.forestry.workshops.at(old_weaver).made_88 == old_ledger.made_88);
    CHECK(old_ledger.made_88[0] ==
          895); // measured pins of this run (zero-start, 200,000 ticks before the demolition)
    CHECK(taken1 == 744);
    CHECK(store1 == 151);
    CHECK(old_out_lost == 0);
    CHECK(old_in_lost == 4);
    CHECK(carrier_wool_lost == 342);
    CHECK(taken(c) == taken1 + store1); // 895: the remaining store is sold, nothing new comes in
    CHECK(c.city->stock.stock[kStoffe] == 0);
    const std::uint64_t taken_gap = taken(c);
    // the restoration: a paid public command at the same site
    CHECK_FALSE(c.w.rules().free_build_cheat);
    const PaySnap pay0 = pay_snap(c, *o->types->find(wtype));
    const auto res =
        c.w.apply(cmd(c, sim::BuildCmd{c.ref, {sim::PlacementWire{wtype, at.x, at.y, rot, 0, 0}}}));
    REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    check_paid(c, *o->types->find(wtype), pay0, pay_snap(c, *o->types->find(wtype)));
    const sim::Building* nb = c.isl->building_at(c.w.config().layout, c.city->number, at.x, at.y);
    REQUIRE(nb != nullptr);
    const sim::BuildingId new_weaver = nb->id;
    run_to(c, T3);
    CHECK(c.w.check_invariants() == "");
    const auto& wl = c.stats.forestry.workshops;
    REQUIRE(wl.count(new_weaver) == 1);
    CHECK(wl.at(new_weaver).made_88[0] == 984); // production resumed (measured)
    CHECK(taken(c) == 1433);
    CHECK(c.city->stock.stock[kStoffe] == 236);
    CHECK(taken(c) > taken_gap); // households buy new cloth again
    CHECK(c.city->stock.stock[kStoffe] > 0);
    // the reconciliation across both weavers: what went with the demolished building is the
    // legitimate loss
    const Ledger l = ledger_of(c, 0);
    CHECK(l.u_farm == 0);
    std::uint64_t cloth_loaded = 0, made = old_ledger.made_88[0],
                  consumed = old_ledger.consumed_88[0];
    for (const sim::BuildingId id : {old_weaver, new_weaver}) {
        cloth_loaded += c.stats.loaded_from.count(id) ? c.stats.loaded_from.at(id) : 0U;
    }
    made += wl.at(new_weaver).made_88[0];
    consumed += wl.at(new_weaver).consumed_88[0];
    const sim::Workshop& nw = std::get<sim::Workshop>(c.isl->find_building(new_weaver)->payload);
    std::uint64_t cart = 0, wool_cargo = 0;
    for (const auto& s : c.w.figures().list.slots) {
        if (s && !s->expiry_stamp.has_value() && s->job == sim::kJobMarketCart &&
            s->cargo_goods == kStoffe) {
            cart += s->cargo_88;
        }
        if (s && !s->expiry_stamp.has_value() && s->job == sim::kJobWorkshopCarrier &&
            s->cargo_goods == kWolle) {
            wool_cargo += s->cargo_88;
        }
    }
    CHECK(std::int64_t(made) == std::int64_t(old_out_lost + cloth_loaded + nw.out[0]));
    CHECK(std::int64_t(cloth_loaded) ==
          std::int64_t(cart + c.city->stock.stock[kStoffe] + taken(c)));
    CHECK(std::int64_t(l.wool_loaded) ==
          std::int64_t(consumed + old_in_lost + carrier_wool_lost + nw.in[0] + wool_cargo +
                       c.city->stock.stock[kWolle]));
    std::cerr << "PIN R made_old " << old_ledger.made_88[0] << " taken1 " << taken1 << " store1 "
              << store1 << " taken_gap " << taken_gap << " new made "
              << wl.at(new_weaver).made_88[0] << " taken3 " << taken(c) << " store3 "
              << c.city->stock.stock[kStoffe] << " old_out_lost " << old_out_lost << " old_in_lost "
              << old_in_lost << " carrier_wool_lost " << carrier_wool_lost << "\n";
}
