// ECON-BOOK-1 B5. Complete normal original Tutorial import; original
// positions, stocks, prices, costs and supported whole program are retained.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/transport.hpp>

#include "evidence_out.hpp"
#include "island7_town.hpp"

namespace {
namespace sim = anno::sim;
using tutorial_town::Original;
constexpr sim::CityRef town{sim::IslandId{7}, sim::CityNo{1}};
const sim::City& city7(const sim::World& w) {
    const auto* c = w.islands()[7]->city(sim::CityNo{1});
    REQUIRE(c);
    return *c;
}
const sim::Building& at(const sim::World& w, int x, int y) {
    const auto* b = w.building_at(town, x, y);
    REQUIRE(b);
    return *b;
}
sim::World original_economy(const Original& o) {
    auto loaded = anno::sim_load::load_scenario_file(o.dir / "Scenes" / tutorial_town::kTutorial,
                                                     o.dir, o.templates, *o.types, o.cfg, {});
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    REQUIRE_FALSE(loaded.value().loaded.spec.figures.empty());
    sim::Rules r;
    r.city_booking = true;
    r.house_updates = true;
    auto made = sim::World::create(loaded.value().loaded.spec, o.types, o.cfg, r);
    REQUIRE_MESSAGE(made, (made ? "" : made.error()));
    auto w = std::move(made).value();
    REQUIRE(w.initialize_scripts().empty());
    REQUIRE(w.script().state.context == sim::ScriptContext::Ready);
    return w;
}
void export_bytes(const std::string& name, const std::vector<std::uint8_t>& bytes) {
    const char* dir = std::getenv("ANNO_ECON_OUT_DIR");
    if (!dir || !*dir)
        return;
    const auto error = sim::test::write_evidence_file(
        std::filesystem::path(dir) / name,
        std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    REQUIRE_MESSAGE(error.empty(), error);
}
void export_text(const std::string& name, const std::string& text) {
    export_bytes(name, {text.begin(), text.end()});
}
} // namespace

TEST_CASE("economy loop: original Tutorial prerequisites and complete actor import") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto strict = anno::sim_load::load_scenario_file(
        o->dir / "Scenes" / tutorial_town::kTutorial, o->dir, o->templates, *o->types, o->cfg, {});
    REQUIRE(strict);
    MESSAGE("strict Tutorial actors: " << strict.value().loaded.spec.figures.size());
    CHECK(strict.value().loaded.spec.figures.size() == 3);
    auto w = original_economy(*o);
    const auto& c = city7(w);
    REQUIRE(at(w, 261, 344).type == sim::TypeId{809});
    REQUIRE(at(w, 301, 351).type == sim::TypeId{1335});
    const auto& farm = std::get<sim::SmallFarm>(at(w, 261, 344).payload);
    CHECK(farm.out[0] == 0);
    CHECK(farm.raw[0] == 0);
    CHECK(c.stock.stock[29] == 0);
    CHECK(std::get<sim::Inn>(at(w, 301, 351).payload).tavern == 0);
    CHECK(c.stock.stock[41] >= 512);
    CHECK(c.stock.stock[42] >= 1024);
    CHECK(w.players().find(c.owner)->money >= 250);
    CHECK(c.accounts.civil_cost > 0);
    CHECK_FALSE(w.rules().free_build_cheat);
    CHECK_FALSE(w.players().find(c.owner)->free_build);
    std::size_t houses = 0;
    for (const auto& list : c.index.lists)
        for (const auto& b : list.slots)
            if (b && std::holds_alternative<sim::House>(b->payload))
                ++houses;
    CHECK(houses == 13);
    for (auto id : {sim::TypeId{809}, sim::TypeId{1335}, sim::TypeId{1310}}) {
        const auto* t = o->types->find(id);
        REQUIRE(t);
        REQUIRE(t->running_cost.size() == 2);
        MESSAGE("original type " << id.v << " costs " << t->running_cost[0] << ","
                                 << t->running_cost[1]);
    }
    MESSAGE("original town owner="
            << c.owner.v << " climate=" << w.islands()[7]->climate << " tax=" << w.settings().prop10
            << " money=" << w.players().find(c.owner)->money << " food=" << c.stock.stock[26]
            << " tools=" << c.stock.stock[41] << " wood=" << c.stock.stock[42]
            << " civil=" << c.accounts.civil_cost << " military=" << c.accounts.military_cost);
    CHECK(w.check_invariants().empty());
}

namespace {
using Balances = std::map<std::uint32_t, std::int64_t>;
Balances balances(const sim::World& w) {
    Balances out;
    for (const auto& p : w.players().slots)
        if (p)
            out[p->index.v] = p->money;
    return out;
}
std::vector<std::uint8_t> city_finances(const sim::City& c) {
    sim::WriteArchive a;
    auto accounts = c.accounts;
    auto bills = c.billed;
    sim::fields(a, accounts);
    a.u32(static_cast<std::uint32_t>(bills.size()));
    for (auto& n : bills)
        a.u32(n);
    return a.take();
}
std::uint64_t sold(const sim::TransportStats& stats, sim::BuildingId inn) {
    const auto it = stats.residents.inn_taken_by_building.find(inn);
    return it == stats.residents.inn_taken_by_building.end() ? 0 : it->second;
}
std::uint64_t cargo(const sim::World& w, sim::GoodsId good) {
    std::uint64_t n = 0;
    for (const auto& f : w.figures().list.slots)
        if (f && !f->expiry_stamp && f->cargo_goods == good && f->home.valid() &&
            w.islands()[7]->find_building(f->home))
            n += f->cargo_88;
    return n;
}
struct Audit {
    std::map<sim::CityRef, std::vector<std::uint32_t>> before_bills;
    std::map<sim::CityRef, sim::CityAccounts> before_accounts;
    Balances before_money;
    std::map<sim::GoodsId, std::uint64_t> initial_pending, booked_bills;
    std::map<std::uint32_t, std::uint64_t> income, upkeep;
    std::uint64_t count = 0, positive = 0, target_income = 0, target_upkeep = 0,
                  target_alcohol_income = 0, target_alcohol_units = 0;
    std::ostringstream rows;
    explicit Audit(const sim::World& w) {
        rows << "tick\tisland\tcity\towner\tnumerator\tincome\tupkeep\tbefore\tafter\tpriced_"
                "bills\n";
        for (const auto& is : w.islands())
            if (is)
                for (const auto& c : is->cities)
                    if (c)
                        for (std::uint32_t g = w.config().billed_goods_first;
                             g <= w.config().billed_goods_last; ++g)
                            initial_pending[g] += c->billed[g];
    }
    void before(const sim::World& w) {
        before_bills.clear();
        before_accounts.clear();
        before_money = balances(w);
        for (const auto& is : w.islands())
            if (is)
                for (const auto& c : is->cities)
                    if (c) {
                        const sim::CityRef ref{is->index, c->number};
                        before_bills[ref] = c->billed;
                        before_accounts[ref] = c->accounts;
                    }
    }
    void after(const sim::World& w) {
        auto expected = before_money;
        for (const auto& event : w.last_output().events)
            if (event.booking) {
                const auto& b = *event.booking;
                REQUIRE(b.fault == sim::AccountingFault::None);
                REQUIRE(before_bills.contains(b.city));
                const auto& bills = before_bills.at(b.city);
                const auto& old = before_accounts.at(b.city);
                const auto& is = *w.islands()[b.city.island.v];
                const auto* c = is.city(b.city.city);
                REQUIRE(c);
                CHECK(c->owner == b.owner);
                std::uint64_t numerator = 0, alcohol_numerator = 0;
                std::ostringstream terms;
                for (std::uint32_t g = w.config().billed_goods_first;
                     g <= w.config().billed_goods_last; ++g) {
                    const auto* info = w.types().goods(g);
                    REQUIRE(info);
                    const auto price = info->prices ? info->prices->climate[is.climate] : 0;
                    const auto surcharge =
                        info->prices ? info->prices->tax[w.settings().prop10] : 0;
                    numerator += (std::uint64_t{price} + surcharge) * bills[g];
                    if (g == 29)
                        alcohol_numerator = (std::uint64_t{price} + surcharge) * bills[g];
                    booked_bills[g] += bills[g];
                    if (bills[g])
                        terms << g << ":" << price << "+" << surcharge << "*" << bills[g] << ",";
                }
                // Independent ordinary-domain arithmetic, not the engine calculator.
                const auto gross = (numerator + 128) / 256,
                           charge = (old.civil_cost + old.military_cost) / 15;
                CHECK(b.income == gross);
                CHECK(b.upkeep == charge);
                CHECK(b.money_before == expected.at(b.owner.v));
                expected[b.owner.v] += static_cast<std::int64_t>(gross);
                expected[b.owner.v] -= static_cast<std::int64_t>(charge);
                CHECK(b.money_after == expected.at(b.owner.v));
                CHECK(c->accounts.sales == old.sales * 104 / 105 + gross);
                CHECK(c->accounts.trade_sales == old.trade_sales * 104 / 105);
                CHECK(c->accounts.trade_purchases == old.trade_purchases * 104 / 105);
                CHECK(c->accounts.stamp == static_cast<std::uint16_t>(w.clock().tick()));
                CHECK(b.rows == 15);
                ++count;
                positive += gross > 0 ? 1U : 0U;
                income[b.owner.v] += gross;
                upkeep[b.owner.v] += charge;
                if (b.city == town) {
                    target_income += gross;
                    target_upkeep += charge;
                    target_alcohol_units += bills[29];
                    target_alcohol_income += gross - (numerator - alcohol_numerator + 128) / 256;
                }
                rows << event.tick << '\t' << b.city.island.v << '\t' << b.city.city.v << '\t'
                     << b.owner.v << '\t' << numerator << '\t' << gross << '\t' << charge << '\t'
                     << b.money_before << '\t' << b.money_after << '\t' << terms.str() << '\n';
            }
        CHECK(balances(w) == expected); // zero unexplained treasury residual, each owner, each step
    }
    void check_bills(const sim::World& w, const sim::TransportStats& stats) {
        for (std::uint32_t g = w.config().billed_goods_first; g <= w.config().billed_goods_last;
             ++g) {
            std::uint64_t pending = 0;
            for (const auto& is : w.islands())
                if (is)
                    for (const auto& c : is->cities)
                        if (c)
                            pending += c->billed[g];
            const auto it = stats.residents.billed_by_goods.find(g);
            const auto purchases = it == stats.residents.billed_by_goods.end() ? 0 : it->second;
            CHECK(booked_bills[g] + pending == initial_pending[g] + purchases);
        }
    }
};
void native_cut(const sim::World& w, const Original& o, const std::string& label) {
    const auto bytes = w.save();
    export_bytes("cut-" + label + ".save", bytes);
    auto loaded = sim::World::load(bytes, o.types);
    REQUIRE_MESSAGE(loaded, (loaded ? "" : loaded.error()));
    auto a = w;
    auto b = std::move(loaded).value();
    a.set_transport_stats(nullptr);
    b.set_transport_stats(nullptr);
    (void)a.drain_events();
    (void)b.drain_events();
    CHECK(a.save() == b.save());
    for (int i = 0; i < 20; ++i) {
        a.advance(100);
        b.advance(100);
        REQUIRE_FALSE(a.faulted());
        REQUIRE_FALSE(b.faulted());
        CHECK(a.last_output().events == b.last_output().events);
        CHECK(a.drain_events() == b.drain_events());
        REQUIRE(a.last_output().draws.size() == b.last_output().draws.size());
        for (std::size_t k = 0; k < a.last_output().draws.size(); ++k) {
            CHECK(a.last_output().draws[k].site == b.last_output().draws[k].site);
            CHECK(a.last_output().draws[k].count == b.last_output().draws[k].count);
        }
        CHECK(a.last_output().rng_cursor_before == b.last_output().rng_cursor_before);
        CHECK(a.last_output().rng_cursor_after == b.last_output().rng_cursor_after);
    }
    CHECK(a.save() == b.save());
    CHECK(a.check_invariants().empty());
    MESSAGE("native cut " << label << " tick=" << w.clock().tick()
                          << " exact state/events/draws continued");
}
struct Homes {
    struct Watch {
        std::uint32_t slot = 0, initial = 0, previous = 0;
    };
    std::map<sim::BuildingId, Watch> entries;
    std::set<sim::BuildingId> initially_empty_served, recovered;
    Homes(const sim::World& w) {
        for (const auto& list : city7(w).index.lists)
            for (const auto& b : list.slots)
                if (b) {
                    const auto* h = std::get_if<sim::House>(&b->payload);
                    if (!h)
                        continue;
                    auto g = sim::house_group(w.config(), w.types().find(b->type)->prodtype);
                    REQUIRE(g);
                    auto view = w.types().population_view(*g, w.config());
                    REQUIRE(view);
                    const auto slot =
                        view.value().derived->lookup[w.types().population_roles().tavern_goods];
                    entries[b->id] = {slot, h->stock[slot], h->stock[slot]};
                }
    }
    void observe(const sim::World& w, bool after_recovery) {
        for (auto& [id, watch] : entries) {
            const auto* b = w.islands()[7]->find_building(id);
            if (!b)
                continue;
            const auto* h = std::get_if<sim::House>(&b->payload);
            if (!h || watch.slot >= h->stock.size())
                continue;
            if (h->stock[watch.slot] > watch.previous) {
                if (watch.initial == 0)
                    initially_empty_served.insert(id);
                if (after_recovery)
                    recovered.insert(id);
            }
            watch.previous = h->stock[watch.slot];
        }
    }
};
} // namespace

TEST_CASE("economy loop: complete Tutorial production sales upkeep shortage paid recovery and "
          "native cuts") {
    auto o = tutorial_town::load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto w = original_economy(*o);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    Audit audit(w);
    Homes homes(w);
    const auto initial_money = balances(w);
    std::map<sim::FigureId, std::pair<std::uint32_t, sim::PlayerId>> original_actors;
    for (const auto& f : w.figures().list.slots)
        if (f)
            original_actors[f->id] = {f->kind, f->owner};
    REQUIRE(original_actors.size() == 3);
    const auto inn = at(w, 301, 351).id, original_farm = at(w, 261, 344).id;
    const auto initial_food = city7(w).stock.stock[26];
    const auto initial_cost = city7(w).accounts.civil_cost;
    REQUIRE(initial_cost == 354);
    REQUIRE(city7(w).accounts.military_cost == 0);
    REQUIRE(city7(w).stock.stock[29] == 0);
    REQUIRE(std::get<sim::SmallFarm>(at(w, 261, 344).payload).out[0] == 0);
    REQUIRE(std::get<sim::SmallFarm>(at(w, 261, 344).payload).raw[0] == 0);
    REQUIRE(std::get<sim::Inn>(at(w, 301, 351).payload).tavern == 0);
    for (const auto& p : w.players().slots)
        if (p) {
            REQUIRE(p->upkeep_a == 0);
            REQUIRE(p->upkeep_b == 0);
        }
    auto started = sim::Recorder::start(w);
    REQUIRE(started);
    auto rec = std::move(started).value();
    const std::vector<std::uint64_t> cps{1950, 2000, 150000, 190000, 270000, 310000, 510000};
    std::size_t cp = 0;
    const sim::Command demolish{sim::PlayerId{0}, sim::DemolishCmd{town, {{261, 344}}}};
    const sim::Command rebuild{sim::PlayerId{0},
                               sim::BuildCmd{town, {{sim::TypeId{809}, 261, 344, 0, 0, 0}}}};
    bool trip_cut = false, removed = false, rebuilt = false, changed_command_legal = false;
    std::uint64_t sold_at_removal = 0;
    sim::BuildingId new_farm;
    std::optional<sim::World> control;
    sim::TransportStats control_stats;
    std::optional<Audit> control_audit;
    std::optional<Homes> control_homes;
    auto record_command = [&](const sim::Command& cmd, std::int64_t gold) {
        const auto before = balances(w);
        std::map<sim::CityRef, std::vector<std::uint8_t>> unrelated;
        for (const auto& is : w.islands())
            if (is)
                for (const auto& c : is->cities)
                    if (c && sim::CityRef{is->index, c->number} != town)
                        unrelated[{is->index, c->number}] = city_finances(*c);
        const auto result = rec.submit(cmd);
        REQUIRE_MESSAGE(result.ok(), sim::reject_name(result.status));
        REQUIRE(result.records.size() == 1);
        REQUIRE(result.records[0].status == sim::Reject::None);
        auto expected = before;
        expected[0] -= gold;
        CHECK(balances(w) == expected);
        for (const auto& [ref, bytes] : unrelated)
            CHECK(city_finances(*w.islands()[ref.island.v]->city(ref.city)) == bytes);
    };
    while (w.clock().tick() < 510000) {
        if (w.clock().tick() == 150000 && !removed) {
            sold_at_removal = sold(stats, inn);
            REQUIRE(sold_at_removal > 0);
            CHECK(homes.initially_empty_served.size() >= 3);
            const auto& made = stats.forestry.small_farms.at(original_farm);
            const auto& farm = std::get<sim::SmallFarm>(at(w, 261, 344).payload);
            CHECK(made.made_88[0] > 0);
            CHECK(made.made_88[0] == farm.out[0] + city7(w).stock.stock[29] +
                                         std::get<sim::Inn>(at(w, 301, 351).payload).tavern +
                                         sold_at_removal + cargo(w, 29));
            MESSAGE("first real alcohol: made=" << made.made_88[0] << " sold=" << sold_at_removal
                                                << " empty households served="
                                                << homes.initially_empty_served.size());
            auto alternate = sim::World::load(w.save(), o->types);
            REQUIRE(alternate);
            const auto changed_result =
                alternate.value().apply({sim::PlayerId{0}, sim::DemolishCmd{town, {{259, 352}}}});
            REQUIRE(changed_result.ok());
            REQUIRE(changed_result.records[0].status == sim::Reject::None);
            changed_command_legal = true;
            record_command(demolish, 0);
            removed = true;
            CHECK(city7(w).accounts.civil_cost == initial_cost - 20);
            native_cut(w, *o, "after-removal");
        }
        if (w.clock().tick() == 270000 && !rebuilt) {
            CHECK(sold(stats, inn) == sold_at_removal);
            CHECK(city7(w).stock.stock[29] == 0);
            CHECK(std::get<sim::Inn>(at(w, 301, 351).payload).tavern == 0);
            CHECK(cargo(w, 29) == 0);
            auto loaded = sim::World::load(w.save(), o->types);
            REQUIRE(loaded);
            control = std::move(loaded).value();
            control->set_transport_stats(&control_stats);
            control_audit.emplace(*control);
            control_homes.emplace(*control);
            const auto tools = city7(w).stock.stock[41], wood = city7(w).stock.stock[42];
            REQUIRE(w.players().find(sim::PlayerId{0})->money >= 250);
            REQUIRE(tools >= 512);
            REQUIRE(wood >= 1024);
            record_command(rebuild, 250);
            rebuilt = true;
            new_farm = at(w, 261, 344).id;
            CHECK(new_farm != original_farm);
            CHECK(tools - city7(w).stock.stock[41] == 512);
            CHECK(wood - city7(w).stock.stock[42] == 1024);
            CHECK(city7(w).accounts.civil_cost == initial_cost);
            native_cut(w, *o, "after-paid-rebuild");
        }
        audit.before(w);
        rec.advance(100);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
        audit.after(w);
        homes.observe(w, rebuilt);
        if (control) {
            control_audit->before(*control);
            control->advance(100);
            REQUIRE_MESSAGE(!control->faulted(), control->fault());
            control_audit->after(*control);
            control_homes->observe(*control, true);
        }
        if (!trip_cut)
            for (const auto& f : w.figures().list.slots)
                if (f && f->resident && f->resident->step == sim::ResidentStep::ToHome &&
                    f->home.valid() && w.islands()[7]->find_building(f->home) &&
                    !f->resident->cargo.empty() &&
                    std::any_of(
                        f->resident->cargo.begin(), f->resident->cargo.end(),
                        [](const auto& item) { return item.goods != 0 && item.amount_88 > 0; })) {
                    native_cut(w, *o, "resident-purchased-trip");
                    trip_cut = true;
                    break;
                }
        if (cp < cps.size() && w.clock().tick() == cps[cp]) {
            rec.checkpoint();
            if (cp < 2)
                native_cut(w, *o, cp == 0 ? "before-booking" : "after-booking");
            ++cp;
        }
        (void)w.drain_events();
        if (control)
            (void)control->drain_events();
    }
    REQUIRE(trip_cut);
    REQUIRE(removed);
    REQUIRE(rebuilt);
    REQUIRE(cp == cps.size());
    REQUIRE(control);
    CHECK(sold(control_stats, inn) == 0);
    CHECK(sold(stats, inn) > sold_at_removal);
    CHECK(homes.recovered.size() >= 3);
    CHECK(control_homes->recovered.empty());
    CHECK(stats.forestry.small_farms.at(new_farm).made_88[0] > 0);
    audit.check_bills(w, stats);
    control_audit->check_bills(*control, control_stats);
    CHECK(audit.positive > 0);
    CHECK(audit.target_income > 0);
    CHECK(audit.target_alcohol_income > 0);
    CHECK(audit.booked_bills[29] > 0);
    CHECK(audit.target_alcohol_units > 0);
    for (const auto& [id, facts] : original_actors) {
        const auto* f = sim::find_figure(w.figures(), id);
        REQUIRE(f);
        CHECK(f->kind == facts.first);
        CHECK(f->owner == facts.second);
    }
    CHECK(audit.target_upkeep > 0);
    for (const auto& [owner, cash] : initial_money)
        CHECK(w.players().find(sim::PlayerId{owner})->money ==
              cash + static_cast<std::int64_t>(audit.income[owner]) -
                  static_cast<std::int64_t>(audit.upkeep[owner]) - (owner == 0 ? 250 : 0));
    CHECK(w.check_invariants().empty());
    CHECK(control->check_invariants().empty());
    const auto log = sim::write_log(rec.log());
    REQUIRE(log);
    export_bytes("economy.log", log.value());
    auto changed = rec.log();
    std::size_t changed_entry = 0;
    CHECK(std::count_if(changed.entries.begin(), changed.entries.end(), [](const auto& e) {
              return e.kind == sim::ReplayEntry::Kind::Commands;
          }) == 2);
    for (std::size_t i = 0; i < changed.entries.size(); ++i)
        if (changed.entries[i].kind == sim::ReplayEntry::Kind::Commands) {
            auto& d = std::get<sim::DemolishCmd>(changed.entries[i].commands[0].body);
            d.tiles = {{259, 352}};
            changed_entry = i;
            break;
        }
    REQUIRE(changed_entry > 0);
    REQUIRE(changed_command_legal); // public command accepted at actual tick150000
    const auto negative = sim::write_log(changed);
    REQUIRE(negative);
    export_bytes("economy-changed-command.log", negative.value());
    export_text("ledger.tsv", audit.rows.str());
    export_text("control-ledger.tsv", control_audit->rows.str());
    std::ostringstream summary;
    summary << "complete normal Tutorial import; original actors3; no grants/bill "
               "writes/teleports/price overrides\n"
            << "initial_food=" << initial_food
            << " inherited; alcohol farm/raw/store/Inn initially0\n"
            << "bookings=" << audit.count << " positive=" << audit.positive
            << " target_income=" << audit.target_income << " target_upkeep=" << audit.target_upkeep
            << " target_alcohol_marginal_income=" << audit.target_alcohol_income
            << " target_booked_alcohol_units=" << audit.target_alcohol_units
            << " global_booked_alcohol_units=" << audit.booked_bills[29]
            << "\nfirst_sold=" << sold_at_removal << " final_sold=" << sold(stats, inn)
            << " no-rebuild_additional_sold=" << sold(control_stats, inn)
            << " recovered_households=" << homes.recovered.size()
            << " control_recovered_households=" << control_homes->recovered.size() << "\n";
    for (const auto& [owner, cash] : initial_money)
        summary << "owner=" << owner << " before=" << cash << " income=" << audit.income[owner]
                << " upkeep=" << audit.upkeep[owner] << " construction=" << (owner == 0 ? 250 : 0)
                << " after=" << w.players().find(sim::PlayerId{owner})->money << " residual=0\n";
    export_text("summary.txt", summary.str());
    MESSAGE(summary.str());
    std::ifstream pins(std::string(ANNO_FIXTURES_DIR) +
                       "/sim/econbook1/current134/loop_checkpoints.txt");
    REQUIRE(pins.good());
    std::vector<std::string> expected;
    for (std::string line; std::getline(pins, line);)
        if (!line.empty() && line[0] != '#')
            expected.push_back(line);
    REQUIRE(expected.size() == cps.size());
    std::ostringstream actual;
    std::size_t checkpoint = 0;
    for (std::size_t index = 0; index < rec.log().entries.size(); ++index) {
        const auto& e = rec.log().entries[index];
        if (e.kind != sim::ReplayEntry::Kind::Checkpoint)
            continue;
        std::ostringstream row;
        row << index << ' ' << cps[checkpoint] << ' ' << std::hex << std::setfill('0')
            << std::setw(16) << e.hash;
        CHECK(row.str() == expected[checkpoint]);
        actual << row.str() << '\n';
        ++checkpoint;
    }
    REQUIRE(checkpoint == 7);
    export_text("checkpoints.txt", actual.str());
}
