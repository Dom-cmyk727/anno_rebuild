// ECON-BOOK-1 B4. Boundary setup is synthetic and explicit; real assets live in
// economy_loop_test.cpp. Expected arithmetic comes from reviewed hand vectors.
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/city_booking.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/tick.hpp>

#include "command_helpers.hpp"

namespace {
namespace sim = anno::sim;
using namespace command_test;
constexpr auto hi = std::numeric_limits<std::int64_t>::max();

sim::synthetic::SyntheticTypes priced(sim::SimConfig cfg = {}) {
    auto t = build_types();
    auto goods = sim::original_goods();
    goods[26].prices = sim::GoodsPrices{std::vector<std::uint32_t>(7, 256), {0, 5, 10}};
    sim::TypeRegistry::Builder b;
    sim::HouseGroup group;
    group.number = 0;
    group.wohnmax = 8;
    group.no_upgrade = true;
    group.no_downgrade = true;
    group.needs = {{26, true, 41}};
    std::vector<sim::HouseGroup> groups(cfg.house_groups, group);
    for (std::uint32_t i = 0; i < cfg.house_groups; ++i)
        groups[i].number = i;
    b.config(cfg).goods(goods).house_groups(groups).walk_tables(t.registry->walk_tables());
    for (auto type : t.registry->types()) {
        if (type.id == t.weaver)
            type.running_cost = {2, 28};
        b.add(type);
    }
    auto made = b.build();
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    t.registry = made.value();
    return t;
}
sim::Rules enabled(bool houses = false) {
    sim::Rules r;
    r.city_booking = true;
    r.house_updates = houses;
    return r;
}
std::vector<sim::CityBookingLedger> bookings(const sim::World& w) {
    std::vector<sim::CityBookingLedger> out;
    for (const auto& e : w.last_output().events)
        if (e.booking)
            out.push_back(*e.booking);
    return out;
}
std::int64_t money(const sim::World& w, std::uint32_t owner = 0) {
    return w.players().find(sim::PlayerId{owner})->money;
}
sim::ScenarioSpec scenario(const sim::synthetic::SyntheticTypes& t, bool same_owner = false,
                           bool separate_island = false) {
    auto spec = two_city_spec(t);
    if (same_owner)
        spec.islands[0].cities[1].owner = sim::PlayerId{0};
    if (separate_island) {
        auto is = spec.islands[0];
        is.slot = sim::IslandId{3};
        is.x0 += 200;
        is.objects.clear();
        is.deposits.clear();
        is.cities = {is.cities[1]};
        for (auto& b : is.cities[0].buildings)
            b.x += 200;
        is.owner_runs = {{1, 64U * 64U}};
        spec.islands[0].cities.resize(1);
        spec.islands[0].owner_runs = {{0, 64U * 64U}};
        spec.islands.push_back(is);
    }
    return spec;
}
sim::CityRef second(bool separate) {
    return {sim::IslandId{separate ? 3U : 0U}, sim::CityNo{1}};
}

} // namespace

TEST_CASE("city booking: all independently pinned checked arithmetic vectors") {
    std::ifstream f(std::string(ANNO_FIXTURES_DIR) + "/sim/econbook1/booking_engine_vectors.txt");
    REQUIRE(f.good());
    std::set<std::string> ids;
    for (std::string raw; std::getline(f, raw);) {
        if (raw.empty() || raw[0] == '#')
            continue;
        std::istringstream in(raw);
        std::string id, arrow, fault;
        sim::SimConfig cfg;
        sim::CityAccounts a;
        std::uint32_t tick = 0, stamp = 0;
        std::int64_t cash = 0, expected_money = 0;
        std::size_t count = 0;
        in >> id >> tick >> stamp >> cfg.economy.period >> cfg.upkeep_slices >>
            cfg.stat_decay_num >> cfg.stat_decay_den >> cash >> a.civil_cost >> a.military_cost >>
            a.sales >> a.trade_sales >> a.trade_purchases >> count;
        CAPTURE(id);
        REQUIRE(ids.insert(id).second);
        REQUIRE(count < 100);
        a.stamp = static_cast<std::uint16_t>(stamp);
        std::vector<sim::BookingRow> rows(count);
        for (auto& r : rows)
            in >> r.price >> r.surcharge >> r.billed;
        int stepped = 0;
        std::uint64_t income = 0, upkeep = 0, sales = 0, trades = 0, purchases = 0;
        in >> arrow >> stepped >> fault >> expected_money >> stamp >> income >> upkeep >> sales >>
            trades >> purchases;
        REQUIRE_FALSE(in.fail());
        REQUIRE(arrow == "=>");
        std::string extra;
        REQUIRE_FALSE(static_cast<bool>(in >> extra));
        const auto r = sim::calculate_city_booking(cfg, tick, a, rows, cash);
        CHECK(r.stepped == static_cast<bool>(stepped));
        CHECK(std::string(sim::accounting_fault_name(r.fault)) == fault);
        CHECK(r.money == expected_money);
        CHECK(r.accounts.stamp == stamp);
        CHECK(r.income == income);
        CHECK(r.upkeep == upkeep);
        CHECK(r.accounts.sales == sales);
        CHECK(r.accounts.trade_sales == trades);
        CHECK(r.accounts.trade_purchases == purchases);
        CHECK(r.accounts.civil_cost == a.civil_cost);
        CHECK(r.accounts.military_cost == a.military_cost);
    }
    CHECK(ids.size() == 32);
}

TEST_CASE("city booking: actual scheduler bills once signed balances costs and owner isolation") {
    for (bool house_rule : {false, true}) {
        auto t = priced();
        sim::SimConfig cfg;
        cfg.city_ring_div = 1;
        auto w = make_world(t, scenario(t), enabled(house_rule), {}, cfg);
        auto& c = city_mut(w, kCity0);
        auto& other = city_mut(w, kCity1);
        CHECK(c.accounts.civil_cost == 28);
        CHECK(other.accounts.civil_cost == 28);
        c.billed[26] = 300;
        c.billed[25] = 77;
        other.billed[26] = 10;
        c.accounts.sales = 105;
        c.accounts.trade_sales = 210;
        c.accounts.trade_purchases = 315;
        sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->money = -100;
        const auto p0up = w.players().find(sim::PlayerId{0})->upkeep_a;
        w.advance(3998);
        CHECK(bookings(w).empty());
        CHECK(c.billed[26] == 300);
        w.advance(2);
        REQUIRE_FALSE(w.faulted());
        CHECK(c.billed[26] == 0);
        CHECK(c.billed[25] == 77);
        CHECK(other.billed[26] == 0);
        CHECK(money(w) == 199);
        CHECK(money(w, 1) == 25009);
        CHECK(c.accounts.sales == 404);
        CHECK(c.accounts.trade_sales == 208);
        CHECK(c.accounts.trade_purchases == 312);
        CHECK(c.accounts.stamp == 2000);
        CHECK(w.players().find(sim::PlayerId{0})->upkeep_a == p0up);
        const auto led = bookings(w);
        REQUIRE(led.size() == 2);
        CHECK(led[0].income + led[1].income == 310);
        CHECK(led[0].upkeep + led[1].upkeep == 2);
        c.billed[26] = 4;
        w.advance(0);
        CHECK(c.billed[26] == 4);
        CHECK(bookings(w).empty());
        const auto saved = w.save();
        auto loaded = sim::World::load(saved, t.registry);
        REQUIRE(loaded);
        loaded.value().advance(4000);
        w.advance(4000);
        CHECK(loaded.value().save() == w.save());
        CHECK(loaded.value().last_output().events == w.last_output().events);
        CHECK(c.billed[26] == 0);
        CHECK(money(w) == 202); // +4 -1, not +304
        CHECK(w.check_invariants().empty());
    }
}

TEST_CASE(
    "city booking: shared owner last city overflow faults at merge without reset and legal retry") {
    for (bool separate : {false, true}) {
        auto t = priced();
        auto spec = scenario(t, true, separate);
        // No city upkeep: isolate the gross-credit order. Nonempty producer remains.
        sim::SimConfig cfg;
        cfg.upkeep_slices = 100;
        auto w = make_world(t, spec, enabled(), {}, cfg);
        auto& a = city_mut(w, kCity0);
        auto& b = city_mut(w, second(separate));
        a.billed[26] = 1;
        b.billed[26] = 1;
        b.accounts.sales = 105;
        sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->money = hi - 1;
        const auto pre = w.save();
        w.advance(4000);
        REQUIRE(w.faulted());
        CHECK(w.fault().find("credit") != std::string::npos);
        CHECK(money(w) == hi);
        CHECK(a.billed[26] == 0);
        CHECK(a.accounts.stamp == 2000);
        CHECK(b.billed[26] == 1);
        CHECK(b.accounts.stamp == 0);
        CHECK(b.accounts.sales == 105);
        auto led = bookings(w);
        REQUIRE(led.size() == 2);
        CHECK(led[0].income == 1);
        CHECK(led[1].fault == sim::AccountingFault::Credit);
        CHECK(led[1].income == 0);
        CHECK(led[1].money_before == hi);
        CHECK(led[1].money_after == hi);
        const auto stopped = w.save();
        auto loaded = sim::World::load(stopped, t.registry);
        REQUIRE(loaded);
        loaded.value().advance(4000);
        CHECK(loaded.value().save() == stopped);
        auto retry = sim::World::load(pre, t.registry);
        REQUIRE(retry);
        // A legal paid build from the unfaulted pre-cut leaves gross headroom.
        auto res = retry.value().apply(build(0, kCity0, {at(t.weaver, 125, 110)}));
        REQUIRE(res.ok());
        retry.value().advance(4000);
        REQUIRE_FALSE(retry.value().faulted());
        CHECK(city(retry.value(), second(separate)).billed[26] == 0);
        CHECK(money(retry.value()) == hi - 299);
    }
}

TEST_CASE("city booking: prefixes distinguish queued bill money fault before and after booking") {
    auto t = priced();
    sim::Seams s;
    s.city_pass[0].name = "econ_prefix";
    s.city_pass[0].fn = [](void*, const sim::TickContext&, sim::Island& is, sim::City& c,
                           sim::Effects& fx) {
        if (c.number.v != 0)
            return;
        const sim::CityRef a{is.index, sim::CityNo{0}}, b{is.index, sim::CityNo{1}};
        fx.billed_deltas.push_back({a, 26, 7}); // after city0, before city1
        fx.billed_deltas.push_back({b, 26, 11});
        fx.money.push_back({sim::PlayerId{0}, 20});
    };
    auto w = make_world(t, scenario(t, true), enabled(), s);
    const auto initial = money(w);
    city_mut(w, kCity0).billed[26] = 3;
    w.advance(4000);
    REQUIRE_FALSE(w.faulted());
    CHECK(city(w, kCity0).billed[26] == 7);
    CHECK(city(w, kCity1).billed[26] == 0);
    const auto led = bookings(w);
    REQUIRE(led.size() == 2);
    CHECK(led[0].income == 3);
    CHECK(led[0].money_before == initial);
    CHECK(led[1].income == 11);
    CHECK(led[1].money_before == initial + 22);
    CHECK(money(w) == initial + 32);
    // A fault queued in city0's AFTER seam permits city0 and blocks city1.
    s.city_pass[0].name = "econ_fault_after";
    s.city_pass[0].fn = [](void*, const sim::TickContext&, sim::Island&, sim::City& c,
                           sim::Effects& fx) {
        if (c.number.v == 0) {
            sim::FaultRecord f;
            f.accounting = sim::AccountingFault::CostUnderflow;
            fx.faults.push_back(f);
        }
    };
    auto f = make_world(t, scenario(t), enabled(), s);
    city_mut(f, kCity0).billed[26] = 3;
    city_mut(f, kCity1).billed[26] = 11;
    f.advance(4000);
    REQUIRE(f.faulted());
    CHECK(city(f, kCity0).billed[26] == 0);
    CHECK(city(f, kCity1).billed[26] == 11);
    CHECK(bookings(f).size() == 1);
}

TEST_CASE("city booking: smallest elapsed intervals odd surplus word wrap and default off") {
    auto t = priced();
    sim::SimConfig cfg;
    cfg.city_ring_div = 1;
    auto w = make_world(t, scenario(t), enabled(), {}, cfg);
    city_mut(w, kCity0).billed[26] = 1;
    for (int i = 0; i < 3999; ++i)
        w.advance(1);
    CHECK(w.clock().ms == 3999);
    CHECK(w.clock().tick() == 1999);
    CHECK(city(w, kCity0).billed[26] == 1);
    auto saved = w.save();
    auto cut = sim::World::load(saved, t.registry);
    REQUIRE(cut);
    w.advance(1);
    cut.value().advance(1);
    CHECK(w.save() == cut.value().save());
    CHECK(city(w, kCity0).billed[26] == 0);
    CHECK(city(w, kCity0).accounts.stamp == 2000);
    auto whole = make_world(t, scenario(t), enabled(), {}, cfg);
    city_mut(whole, kCity0).billed[26] = 1;
    whole.advance(3999);
    whole.advance(1);
    CHECK(money(whole) == money(w));
    CHECK(city(whole, kCity0).accounts.sales == city(w, kCity0).accounts.sales);
    // Visit contract intentionally books once for a long delay, never catches up.
    whole.advance(12001);
    CHECK(city(whole, kCity0).accounts.stamp == 8000);
    CHECK(bookings(whole).size() == 2);
    whole.advance(0);
    CHECK(bookings(whole).empty());
    auto off = make_world(t, scenario(t));
    const auto m = money(off);
    city_mut(off, kCity0).billed[26] = 100;
    off.advance(4000);
    CHECK(bookings(off).empty());
    CHECK(money(off) == m);
    CHECK(city(off, kCity0).billed[26] == 100);
    CHECK(city(off, kCity0).accounts.civil_cost == 0);
    CHECK(city(off, kCity0).accounts.stamp == 0);
}

TEST_CASE("city booking: create switch duplicate failed and removal maintain history once") {
    auto t = priced();
    auto w = make_world(t, scenario(t), enabled());
    auto output = t.registry->find(t.weaver)->output_goods.at(0);
    const sim::Command off{sim::PlayerId{0}, sim::SetProductionLineCmd{{110, 110}, output, false}};
    const sim::Command on{sim::PlayerId{0}, sim::SetProductionLineCmd{{110, 110}, output, true}};
    REQUIRE(w.apply(off).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 2);
    REQUIRE(w.apply(off).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 2);
    REQUIRE(w.apply(on).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 28);
    const auto pre = w.save();
    auto denied = w.apply(build(0, kCity0, {at(t.weaver, 110, 110)}));
    REQUIRE_FALSE(denied.ok());
    CHECK(w.save() == pre);
    REQUIRE(w.apply(build(0, kCity0, {at(t.weaver, 125, 110)})).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 56);
    city_mut(w, kCity0).billed[26] = 2; // purchase survives producer removal until booking
    REQUIRE(w.apply(demolish(0, kCity0, {{125, 110}})).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 28);
    CHECK(city(w, kCity0).billed[26] == 2);
    REQUIRE(w.apply(demolish(0, kCity0, {{125, 110}})).ok()); // absent duplicate, no subtraction
    CHECK(city(w, kCity0).accounts.civil_cost == 28);
    auto saved = w.save();
    auto loaded = sim::World::load(saved, t.registry);
    REQUIRE(loaded);
    CHECK(city(loaded.value(), kCity0).accounts.civil_cost == 28);
    loaded.value().advance(4000);
    CHECK(money(loaded.value()) == money(w) + 1);
    // Labelled malformed historical counter: all refused operations are atomic.
    city_mut(w, kCity0).accounts.civil_cost = 0;
    const auto bad = w.save();
    CHECK(w.apply(off).status == sim::Reject::CityAccounting);
    CHECK(w.save() == bad);
    const auto removed = w.apply(demolish(0, kCity0, {{110, 110}}));
    REQUIRE(removed.records.size() == 1);
    CHECK(removed.records[0].status == sim::Reject::CityAccounting);
    CHECK(w.save() == bad);
}

TEST_CASE("city booking: every supported production selector imported switched and removed") {
    auto t = priced();
    auto g = sim::original_goods();
    const auto bow_outputs = t.registry->find(t.bow_maker)->output_goods;
    for (auto out : bow_outputs)
        g[out].forsch = 7;
    sim::TypeRegistry::Builder builder;
    builder.goods(g).walk_tables(t.registry->walk_tables());
    for (auto ty : t.registry->types()) {
        ty.running_cost = {2, 28};
        if (ty.id == t.iron_mine)
            ty.statistic_kind = 2;
        builder.add(ty);
    }
    auto built = builder.build();
    REQUIRE(built);
    t.registry = built.value();
    auto spec = scenario(t);
    auto& bs = spec.islands[0].cities[0].buildings;
    bs.clear();
    std::vector<sim::TypeId> ids{t.weaver,    t.farm,       t.fur_trapper,
                                 t.iron_mine, t.small_farm, t.bow_maker};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = ids[i];
        b.x = 104 + static_cast<int>(i) * 6;
        b.y = 110;
        bs.push_back(b);
    }
    auto w = make_world(t, spec, enabled());
    // Four ordinary active selectors (workshop, farm, trapper, small farm),
    // research-locked weapons inactive, and an active kind2 mine.
    CHECK(city(w, kCity0).accounts.civil_cost == 4 * 28 + 2);
    CHECK(city(w, kCity0).accounts.military_cost == 28);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto* ty = t.registry->find(ids[i]);
        for (auto good : ty->output_goods) {
            REQUIRE(w.apply({sim::PlayerId{0},
                             sim::SetProductionLineCmd{
                                 {104 + static_cast<int>(i) * 6, 110}, good, false}})
                        .ok());
        }
    }
    CHECK(city(w, kCity0).accounts.civil_cost == 10);
    CHECK(city(w, kCity0).accounts.military_cost == 2);
    // Enabling a locked weapon is legal, yet its financial getter is still false.
    const auto tile = sim::TileXY{134, 110};
    REQUIRE(
        w.apply({sim::PlayerId{0}, sim::SetProductionLineCmd{tile, bow_outputs[0], true}}).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 10);
    // Labelled simulation of the already-supported research grant's state mutation:
    // historical city totals remain untouched; same-state switch is not a repair.
    sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->research.state[7] = 2;
    REQUIRE(
        w.apply({sim::PlayerId{0}, sim::SetProductionLineCmd{tile, bow_outputs[0], true}}).ok());
    CHECK(city(w, kCity0).accounts.civil_cost == 10);
    const auto history = w.save();
    auto refused = w.apply(demolish(0, kCity0, {tile}));
    REQUIRE(refused.records.size() == 1);
    CHECK(refused.records[0].status == sim::Reject::CityAccounting);
    CHECK(w.save() == history); // current selector28 exceeds historical bucket10
    // Native reload retains that history rather than scanning and repairing it.
    auto load = sim::World::load(history, t.registry);
    REQUIRE(load);
    CHECK(load.value().save() == history);
    // Removing the other five inactive buildings, then restoring the research
    // premise, removes exactly each contribution and leaves both buckets zero.
    for (std::size_t i = 0; i < ids.size() - 1; ++i) {
        auto r = w.apply(demolish(0, kCity0, {{104 + static_cast<int>(i) * 6, 110}}));
        REQUIRE(r.records[0].status == sim::Reject::None);
    }
    sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->research.state[7] = 0;
    auto r = w.apply(demolish(0, kCity0, {tile}));
    REQUIRE(r.records[0].status == sim::Reject::None);
    CHECK(city(w, kCity0).accounts.civil_cost == 0);
    CHECK(city(w, kCity0).accounts.military_cost == 0);
    CHECK(w.players().find(sim::PlayerId{0})->upkeep_a == 0);
    CHECK(w.players().find(sim::PlayerId{0})->upkeep_b == 0);
}

TEST_CASE("city booking: cost preflight overflow equal-selector costs and city cleanup") {
    auto t = priced();
    auto spec = scenario(t);
    auto w = make_world(t, spec, enabled());
    city_mut(w, kCity0).accounts.civil_cost = std::numeric_limits<std::uint64_t>::max();
    const auto full = w.save();
    const auto cash = money(w);
    const auto rng = w.rng().cursor();
    auto r = w.apply(build(0, kCity0, {at(t.weaver, 125, 110)}));
    REQUIRE(r.records.size() == 1);
    CHECK(r.records[0].status == sim::Reject::CityAccounting);
    CHECK(w.save() == full);
    CHECK(money(w) == cash);
    CHECK(w.rng().cursor() == rng);
    sim::BuildingCost a{1, 14, true, 0}, b{1, 14, false, 0};
    sim::CityAccounts history;
    history.civil_cost = 0;
    CHECK_FALSE(sim::change_city_cost(history, a, false));
    CHECK(a.amount == b.amount);
    sim::Seams seams;
    seams.city_is_empty.name = "econ_cleanup";
    seams.city_is_empty.fn = [](void*, const sim::City&) { return true; };
    auto rules = enabled();
    rules.keep_empty_city = false;
    auto clean = make_world(t, spec, rules, seams);
    REQUIRE(clean.apply(demolish(0, kCity0, {{120, 110}})).ok());
    CHECK(clean.islands()[0]->city(sim::CityNo{0}) == nullptr);
    CHECK(clean.islands()[0]->city(sim::CityNo{1}) != nullptr);
    CHECK(clean.check_invariants().empty());
    auto failed = make_world(t, spec, rules, seams);
    city_mut(failed, kCity0).accounts.civil_cost = 0;
    const auto before = money(failed);
    // The zero-cost farm removal completes; cleanup of the remaining weaver
    // refuses before deleting it or neutralising the city tiles.
    REQUIRE(failed.apply(demolish(0, kCity0, {{120, 110}})).ok());
    REQUIRE(failed.faulted());
    CHECK(failed.building_at(kCity0, 120, 110) == nullptr);
    CHECK(failed.building_at(kCity0, 110, 110) != nullptr);
    CHECK(money(failed) == before);
    CHECK(city(failed, kCity0).accounts.civil_cost == 0);
}

TEST_CASE("city booking: legal arithmetic control") {
    sim::SimConfig cfg;
    sim::CityAccounts a;
    a.civil_cost = 14;
    a.military_cost = 14;
    const std::vector<sim::BookingRow> rows{{50, 0, 300}, {70, 5, 100}};
    const auto r = sim::calculate_city_booking(cfg, 2000, a, rows, 0);
    REQUIRE(r.stepped);
    CHECK(r.fault == sim::AccountingFault::None);
    CHECK(r.income == 88);
    CHECK(r.upkeep == 1);
    CHECK(r.money == 87);
    CHECK(r.accounts.sales == 88);
}

TEST_CASE(
    "city booking: production gross overflow queued saturation faults and no city remainder") {
    auto t = priced();
    auto w = make_world(t, scenario(t), enabled());
    auto& c = city_mut(w, kCity0);
    c.billed[26] = 1;
    sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->money = hi;
    w.advance(4000);
    REQUIRE(w.faulted());
    CHECK(money(w) == hi);
    CHECK(c.billed[26] == 1);
    CHECK(c.accounts.stamp == 0);
    CHECK(c.accounts.sales == 0);
    // Gross MAX+1 fails even though MAX+1-upkeep1 would fit.
    REQUIRE(bookings(w).size() == 1);
    CHECK(bookings(w)[0].fault == sim::AccountingFault::Credit);
    auto spec = scenario(t);
    sim::Seams seams;
    seams.object_manager.name = "econ_bill_prefix_sat";
    seams.object_manager.fn = [](void*, const sim::TickContext&, sim::Island& is,
                                 sim::Effects& fx) {
        fx.billed_deltas.push_back({{is.index, sim::CityNo{0}}, 26, 10});
    };
    auto sat = make_world(t, spec, enabled(), seams);
    city_mut(sat, kCity0).billed[26] = std::numeric_limits<std::uint32_t>::max() - 2;
    sat.advance(4000);
    REQUIRE_FALSE(sat.faulted());
    REQUIRE(bookings(sat).size() == 2);
    CHECK(bookings(sat)[0].income == 4294967295ULL);
    CHECK(city(sat, kCity0).billed[26] == 0);
    // A queued pre-city fault blocks all bookings, but not already queued
    // ordinary bill effects. No reset or money commit can escape the failure.
    seams.object_manager.name = "econ_fault_before";
    seams.object_manager.fn = [](void*, const sim::TickContext&, sim::Island& is,
                                 sim::Effects& fx) {
        fx.billed_deltas.push_back({{is.index, sim::CityNo{0}}, 26, 7});
        sim::FaultRecord f;
        f.accounting = sim::AccountingFault::Type;
        fx.faults.push_back(f);
    };
    auto before = make_world(t, spec, enabled(), seams);
    const auto cash = money(before);
    city_mut(before, kCity0).billed[26] = 1;
    before.advance(4000);
    REQUIRE(before.faulted());
    CHECK(bookings(before).empty());
    CHECK(city(before, kCity0).billed[26] == 8);
    CHECK(money(before) == cash);
    auto repeat = make_world(t, spec, enabled());
    city_mut(repeat, kCity0).accounts.civil_cost = 14;
    const auto start = money(repeat);
    for (int i = 0; i < 15; ++i) {
        repeat.advance(4000);
        REQUIRE_FALSE(repeat.faulted());
    }
    CHECK(money(repeat) == start);
    CHECK(city(repeat, kCity0).accounts.civil_cost == 14);
}

TEST_CASE("city booking: equal-valued selectors still subtract history and script fault precedes "
          "booking") {
    auto t = priced();
    sim::TypeRegistry::Builder b;
    b.walk_tables(t.registry->walk_tables());
    for (auto type : t.registry->types()) {
        if (type.id == t.weaver)
            type.running_cost = {14, 14};
        b.add(type);
    }
    auto types = b.build();
    REQUIRE(types);
    t.registry = types.value();
    auto w = make_world(t, scenario(t), enabled());
    city_mut(w, kCity0).accounts.civil_cost = 0;
    const auto cut = w.save();
    const auto out = t.registry->find(t.weaver)->output_goods[0];
    CHECK(w.apply({sim::PlayerId{0}, sim::SetProductionLineCmd{{110, 110}, out, false}}).status ==
          sim::Reject::CityAccounting);
    CHECK(w.save() == cut);
    auto spec = scenario(t);
    spec.script_context = sim::ScriptContext::Pending;
    spec.script_source = "[OBJECT_DEFINE]\n[AREA_DEFINE]\n[PLAYERMAPPER_DEFINE]\n\"ME\" "
                         "0\n[SCRIPT_KERNEL]\nINITBLOCK(){MONEY_ADD(\"ME\",5); "
                         "TIMER_SET(1,7);}\n[SIGNALS]\n[CAMARAPOS]\n0,0\n";
    spec.script_limits.work = 1;
    auto f = make_world(t, spec, enabled());
    city_mut(f, kCity0).billed[26] = 12;
    const auto before = f.save();
    const auto cash = money(f);
    f.advance(4000);
    REQUIRE(f.faulted());
    CHECK(f.clock().tick() == 0);
    CHECK(city(f, kCity0).billed[26] == 12);
    CHECK(city(f, kCity0).accounts.stamp == 0);
    CHECK(money(f) == cash);
    CHECK(bookings(f).empty());
    CHECK(f.save() != before); // sticky fault is state
}

TEST_CASE("city booking: atomic build preflights aggregate costs before any record mutation") {
    auto t = priced();
    auto rules = enabled();
    rules.build_atomic = true;
    const auto command = build(0, kCity0, {at(t.weaver, 125, 110), at(t.weaver, 127, 110)});
    auto good = make_world(t, scenario(t), rules);
    const auto cash = money(good);
    REQUIRE(good.apply(command).ok());
    CHECK(city(good, kCity0).accounts.civil_cost == 84);
    CHECK(money(good) == cash - 600);
    CHECK(good.building_at(kCity0, 125, 110) != nullptr);
    CHECK(good.building_at(kCity0, 127, 110) != nullptr);
    auto bad = make_world(t, scenario(t), rules);
    city_mut(bad, kCity0).accounts.civil_cost = std::numeric_limits<std::uint64_t>::max() - 40;
    const auto before = bad.save();
    const auto rng = bad.rng().cursor();
    const auto ids = bad.islands()[0]->next_serial;
    auto refused = bad.apply(command);
    CHECK(refused.status == sim::Reject::AtomicAbort);
    REQUIRE(refused.records.size() == 2);
    CHECK(refused.records[0].status == sim::Reject::AtomicAbort);
    CHECK(refused.records[1].status == sim::Reject::CityAccounting);
    CHECK(bad.save() == before);
    CHECK(bad.rng().cursor() == rng);
    CHECK(bad.islands()[0]->next_serial == ids);
    CHECK(bad.building_at(kCity0, 125, 110) == nullptr);
    CHECK(bad.building_at(kCity0, 127, 110) == nullptr);
    // Default non-atomic drag behavior remains record-by-record, including costs.
    rules.build_atomic = false;
    auto partial = make_world(t, scenario(t), rules);
    city_mut(partial, kCity0).accounts.civil_cost = std::numeric_limits<std::uint64_t>::max() - 40;
    const auto r = partial.apply(command);
    REQUIRE(r.ok());
    CHECK(r.records[0].status == sim::Reject::None);
    CHECK(r.records[1].status == sim::Reject::CityAccounting);
    CHECK(city(partial, kCity0).accounts.civil_cost ==
          std::numeric_limits<std::uint64_t>::max() - 12);
}

TEST_CASE("city booking: full u32 elapsed domain and exact word alias through public advance") {
    auto t = priced();
    auto w = make_world(t, scenario(t), enabled());
    city_mut(w, kCity0).billed[26] = 7;
    w.advance(std::numeric_limits<std::uint32_t>::max());
    REQUIRE_FALSE(w.faulted());
    CHECK(w.clock().ms == 4294967295ULL);
    CHECK(w.clock().tick() == 2147483647ULL);
    CHECK(city(w, kCity0).accounts.stamp == 65535);
    CHECK(city(w, kCity0).billed[26] == 0);
    CHECK(bookings(w).size() == 2);
    city_mut(w, kCity0).billed[26] = 2;
    const auto cash = money(w);
    auto loaded = sim::World::load(w.save(), t.registry);
    REQUIRE(loaded);
    loaded.value().advance(1);
    w.advance(1);
    CHECK(loaded.value().save() == w.save());
    CHECK(w.clock().tick() == 2147483648ULL);
    CHECK(bookings(w).empty());
    CHECK(city(w, kCity0).billed[26] == 2);
    CHECK(money(w) == cash);
    w.advance(3998);
    REQUIRE_FALSE(w.faulted());
    CHECK(city(w, kCity0).billed[26] == 0);
    CHECK(city(w, kCity0).accounts.stamp == 1999);
    CHECK(money(w) == cash + 1);
    auto alias = make_world(t, scenario(t), enabled());
    city_mut(alias, kCity0).billed[26] = 3;
    const auto initial = money(alias);
    alias.advance(131072);
    REQUIRE_FALSE(alias.faulted());
    CHECK(alias.clock().tick() == 65536);
    CHECK(bookings(alias).empty());
    CHECK(city(alias, kCity0).billed[26] == 3);
    CHECK(city(alias, kCity0).accounts.stamp == 0);
    CHECK(money(alias) == initial); // intentional original word-age alias, no catch-up
}
