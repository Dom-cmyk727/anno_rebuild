// The iron chain fixture (MAP-EXTRACT-2 T3 to T5): the pilot with a new shallow mine, the imported
// smelter and toolsmith, the market carts and the store, with scoped ledgers whose unexplained
// terms must be zero. Shared by iron_chain_test.cpp (T3) and iron_interrupt_test.cpp (T4, T5).
#pragma once

#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "iron_pilot.hpp"

namespace iron_chain {

namespace sim = anno::sim;
using namespace iron_pilot;

inline constexpr sim::GoodsId kEisenerz = 1, kEisen = 15, kWerkzeug = 41, kHolz = 42;

/// The first site of a window where `type` is answered with `want`, or nullopt.
inline std::optional<std::pair<std::int32_t, std::int32_t>>
find_site(const sim::World& base, std::uint32_t type, sim::Reject want) {
    for (std::int32_t y = 338; y <= 366; ++y) {
        for (std::int32_t x = 356; x <= 388; ++x) {
            sim::World w = base;
            if (w.apply(build_cmd(type, x, y)).status == want) {
                return std::pair{x, y};
            }
        }
    }
    return std::nullopt;
}

enum class Arm { Real, NoMine, NoSmelter, NoWood, NoMarkets, FullStore };

struct Chain {
    World pilot;
    sim::TransportStats stats;
    sim::BuildingId mine, smelter, smith; // the current ones (the last of the lists below)
    // every id a role has had (an interruption may replace a building; the ledger stats persist by
    // id)
    std::vector<sim::BuildingId> smelters, smiths;
    // What vanished with a demolished building, named (T4): its stores, and the cargo of the
    // figures homed at it at that instant (they end with it).
    struct Retired {
        std::uint64_t smelter_in_ore = 0, smelter_out_eisen = 0, smith_in_eisen = 0,
                      smith_out_tools = 0;
        std::uint64_t hands_ore = 0, hands_eisen = 0, hands_tools = 0;
    } retired;
    // the baselines of the terms the fixture owns (preloaded and imported)
    std::uint64_t smelter_in_ore0 = 0, smith_in_eisen0 = 0;
    std::uint64_t store_ore0 = 0, store_eisen0 = 0, store_tools0 = 0, store_holz0 = 0;
    std::uint64_t tools_spent = 0; // by the test's own BuildCmds
    std::pair<std::int32_t, std::int32_t> build_site{0,
                                                     0}; // a legal 1019 site, found before any edit
    explicit Chain(World&& p) : pilot(std::move(p)) {}
};

inline const sim::Building* building(const sim::World& w, sim::BuildingId id) {
    return id.valid() ? w.find_building(id) : nullptr;
}

/// `tools_zero`: the labelled setup "no WERKZEUG in the store" (a named fixture edit, family B), so
/// that a later BuildCmd can only be paid by what the chain delivered.
inline std::unique_ptr<Chain> start_chain(Arm arm, bool tools_zero,
                                          std::pair<std::int32_t, std::int32_t> mine_site = {366,
                                                                                             353}) {
    auto p = make(true, false, 1000);
    if (!p) {
        return nullptr;
    }
    auto c = std::make_unique<Chain>(std::move(*p));
    sim::World& w = c->pilot.w;
    sim::City& city = *test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city));
    // labelled setup in every arm: the new mine is the only ore source, no EISEN in the store
    city.stock.stock[kEisenerz] = 0;
    city.stock.stock[kEisen] = 0;
    if (tools_zero) {
        // a legal site for the later BuildCmd, found while the store still pays for it
        const auto site = find_site(w, 1019, sim::Reject::None);
        REQUIRE(site.has_value());
        c->build_site = *site;
    }
    c->smelter = test_must::must(find_type(w, 1017))->id;
    c->smith = test_must::must(find_type(w, 1019))->id;
    c->smelters = {c->smelter};
    c->smiths = {c->smith};
    if (arm == Arm::NoSmelter) {
        sim::DemolishCmd dem;
        dem.city = kCity;
        dem.tiles = {
            {test_must::must(find_type(w, 1017))->x, test_must::must(find_type(w, 1017))->y}};
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
        c->smelter = sim::BuildingId{};
        c->smelters.clear();
    }
    if (arm == Arm::NoMarkets) {
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city.index.lists)
            for (const auto& b : l.slots)
                if (b && w.types().find(b->type) != nullptr &&
                    w.types().find(b->type)->cls == sim::RuntimeClass::Market)
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    }
    if (arm != Arm::NoMine) {
        const sim::ApplyResult r = w.apply(build_cmd(1205, mine_site.first, mine_site.second));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        c->mine = r.records.at(0).building;
    }
    if (arm == Arm::NoWood) {
        // the wood sources and stores of the chain are emptied: the foresters demolished, the store
        // and both buildings' wood inputs zeroed (labelled setup of this arm)
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city.index.lists)
            for (const auto& b : l.slots)
                if (b && b->type.v == 801)
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        const sim::ApplyResult dr = w.apply(sim::Command{kPlayer, dem});
        REQUIRE_MESSAGE(dr.ok(), sim::reject_name(dr.status) << " tiles " << dem.tiles.size());
        city.stock.stock[kHolz] = 0;
        for (const sim::BuildingId id : {c->smelter, c->smith}) {
            for (auto& l : city.index.lists)
                for (auto& b : l.slots)
                    if (b && b->id == id)
                        std::get<sim::Workshop>(b->payload).in.at(1) = 0;
        }
    }
    if (tools_zero) {
        // labelled setup AFTER the mine is paid: from here only the chain can put WERKZEUG in
        city.stock.stock[kWerkzeug] = 0;
    }
    if (arm == Arm::FullStore) {
        city.stock.stock[kWerkzeug] = city.stock.capacity;
    }
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    w.set_transport_stats(&c->stats);
    if (const sim::Building* b = building(w, c->smelter)) {
        c->smelter_in_ore0 = std::get<sim::Workshop>(b->payload).in.at(0);
    }
    if (const sim::Building* b = building(w, c->smith)) {
        c->smith_in_eisen0 = std::get<sim::Workshop>(b->payload).in.at(0);
    }
    const sim::City& cc = city_of(w);
    c->store_ore0 = cc.stock.stock[kEisenerz];
    c->store_eisen0 = cc.stock.stock[kEisen];
    c->store_tools0 = cc.stock.stock[kWerkzeug];
    c->store_holz0 = cc.stock.stock[kHolz];
    return c;
}

inline void step_chain(Chain& c, int n) {
    for (int i = 0; i < n; ++i) {
        c.pilot.w.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!c.pilot.w.faulted(), c.pilot.w.fault());
    }
}

struct Terms {
    std::uint64_t mine_made = 0, mine_out = 0, ore_loaded = 0, ore_hands = 0, ore_lost = 0,
                  smelter_in_ore = 0, smelter_cons_ore = 0, ore_store = 0;
    std::uint64_t smelter_made = 0, smelter_out = 0, eisen_hands = 0, eisen_lost = 0,
                  smith_in_eisen = 0, smith_cons_eisen = 0, eisen_store = 0;
    std::uint64_t smith_made = 0, smith_out = 0, tools_hands = 0, tools_lost = 0,
                  tools_unloaded = 0, tools_store = 0;
    std::uint64_t wood_smelter = 0, wood_smith = 0, holz_store = 0; // the chain's takes, not closed
    std::int64_t u_mine = 0, u_ore = 0, u_eisen = 0, u_tools_flow = 0, tools_other = 0;
};

inline Terms terms_of(Chain& c) {
    Terms t;
    sim::World& w = c.pilot.w;
    const sim::TransportStats& st = c.stats;
    const auto got = [](const auto& m, const auto& k) -> std::uint64_t {
        const auto it = m.find(k);
        return it == m.end() ? 0U : it->second;
    };
    const sim::City& city = city_of(w);
    if (c.mine.valid()) {
        t.mine_made = got(st.forestry.mine_made_88, c.mine);
        t.ore_loaded = got(st.loaded_from, c.mine);
        if (const sim::Building* m = building(w, c.mine))
            t.mine_out = std::get<sim::Mine>(m->payload).out;
    }
    const auto ledger = [&](const std::vector<sim::BuildingId>& ids, std::uint64_t& in_now,
                            std::uint64_t& cons, std::uint64_t& made, std::uint64_t& out,
                            std::uint64_t& wood_take) {
        for (const sim::BuildingId id : ids) {
            if (const sim::Building* b = building(w, id)) {
                const auto& ws = std::get<sim::Workshop>(b->payload);
                in_now += ws.in.at(0);
                out += ws.out.at(0);
                wood_take += ws.in.at(1);
            }
            const auto it = st.forestry.workshops.find(id); // the stats outlive the building
            if (it != st.forestry.workshops.end()) {
                cons += it->second.consumed_88.at(0);
                made += it->second.made_88.at(0);
                wood_take += it->second.consumed_88.at(1);
            }
        }
    };
    ledger(c.smelters, t.smelter_in_ore, t.smelter_cons_ore, t.smelter_made, t.smelter_out,
           t.wood_smelter);
    ledger(c.smiths, t.smith_in_eisen, t.smith_cons_eisen, t.smith_made, t.smith_out, t.wood_smith);
    // what vanished with demolished buildings is a named history term, not an unexplained one
    t.smelter_in_ore += c.retired.smelter_in_ore;
    t.smelter_out += c.retired.smelter_out_eisen;
    t.smith_in_eisen += c.retired.smith_in_eisen;
    t.smith_out += c.retired.smith_out_tools;
    for (const auto& f : w.figures().list.slots) {
        if (!f || f->expiry_stamp.has_value() || f->owner != city.owner || f->resident)
            continue;
        if (f->cargo_goods == kEisenerz)
            t.ore_hands += f->cargo_88;
        if (f->cargo_goods == kEisen)
            t.eisen_hands += f->cargo_88;
        if (f->cargo_goods == kWerkzeug)
            t.tools_hands += f->cargo_88;
    }
    // the cargo of a figure whose home was demolished stays in the hands until the figure's own
    // tick ends its job, where the E4 ledger counts it as lost (cart_lost); `retired.hands_*` only
    // RECORD what was in flight at the instant (never added: that would count it twice)
    t.ore_lost = got(st.cart_lost, kEisenerz);
    t.eisen_lost = got(st.cart_lost, kEisen);
    t.tools_lost = got(st.cart_lost, kWerkzeug);
    t.tools_unloaded = got(st.unload_offered, kWerkzeug);
    t.ore_store = city.stock.stock[kEisenerz];
    t.eisen_store = city.stock.stock[kEisen];
    t.tools_store = city.stock.stock[kWerkzeug];
    t.holz_store = city.stock.stock[kHolz];
    const auto I = [](std::uint64_t v) { return static_cast<std::int64_t>(v); };
    // ore: made = in the mine + lifted; lifted = in the hands + lost + into the smelter (input now
    // - input at start + used) + into the store (change)
    t.u_mine = I(t.mine_made) - I(t.mine_out) - I(t.ore_loaded);
    t.u_ore = I(t.ore_loaded) - I(t.ore_hands) - I(t.ore_lost) -
              (I(t.smelter_in_ore) - I(c.smelter_in_ore0) + I(t.smelter_cons_ore)) -
              (I(t.ore_store) - I(c.store_ore0));
    // EISEN: made = in the smelter's output + in the hands + lost + into the toolsmith (input now -
    // input at start + used) + into the store (change)
    t.u_eisen = I(t.smelter_made) - I(t.smelter_out) - I(t.eisen_hands) - I(t.eisen_lost) -
                (I(t.smith_in_eisen) - I(c.smith_in_eisen0) + I(t.smith_cons_eisen)) -
                (I(t.eisen_store) - I(c.store_eisen0));
    // WERKZEUG: made = in the toolsmith's output + in the hands + lost + unloaded by market carts
    t.u_tools_flow =
        I(t.smith_made) - I(t.smith_out) - I(t.tools_hands) - I(t.tools_lost) - I(t.tools_unloaded);
    // the store: change = unloaded - spent by the test's BuildCmds - other consumers (houses'
    // upgrades): the drain is the unexplained remainder, REPORTED (never asserted to be zero)
    t.tools_other = I(t.tools_unloaded) - I(c.tools_spent) - (I(t.tools_store) - I(c.store_tools0));
    return t;
}

inline void require_closed(const Terms& t, const char* label) {
    INFO("ledger " << label);
    CHECK(t.u_mine == 0);
    CHECK(t.u_ore == 0);
    CHECK(t.u_eisen == 0);
    CHECK(t.u_tools_flow == 0);
}

/// The cargo, per goods, of the figures homed at `id` right now (they end when it is demolished).
inline void homed_hands(const sim::World& w, sim::BuildingId id, std::uint64_t& ore,
                        std::uint64_t& eisen, std::uint64_t& tools) {
    for (const auto& f : w.figures().list.slots) {
        if (!f || f->expiry_stamp.has_value() || f->home != id || f->resident)
            continue;
        if (f->cargo_goods == kEisenerz)
            ore += f->cargo_88;
        if (f->cargo_goods == kEisen)
            eisen += f->cargo_88;
        if (f->cargo_goods == kWerkzeug)
            tools += f->cargo_88;
    }
}

/// Books what the demolition of the smelter (role 0) or toolsmith (role 1) will take with it, then
/// demolishes it by a public command. The ledger stays closed through the named `retired` terms.
inline void demolish_role(Chain& c, int role) {
    sim::World& w = c.pilot.w;
    const sim::BuildingId id = role == 0 ? c.smelter : c.smith;
    const sim::Building* b = building(w, id);
    REQUIRE(b != nullptr);
    const auto& ws = std::get<sim::Workshop>(b->payload);
    if (role == 0) {
        c.retired.smelter_in_ore += ws.in.at(0);
        c.retired.smelter_out_eisen += ws.out.at(0);
    } else {
        c.retired.smith_in_eisen += ws.in.at(0);
        c.retired.smith_out_tools += ws.out.at(0);
    }
    homed_hands(w, id, c.retired.hands_ore, c.retired.hands_eisen, c.retired.hands_tools);
    sim::DemolishCmd dem;
    dem.city = kCity;
    dem.tiles = {{b->x, b->y}};
    REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
}

inline void say(const char* label, const Terms& t) {
    std::cerr << "CHAIN " << label << " mine_made " << t.mine_made << " ore_loaded " << t.ore_loaded
              << " smelter_made " << t.smelter_made << " smith_made " << t.smith_made
              << " tools_unloaded " << t.tools_unloaded << " tools_store " << t.tools_store
              << " tools_other " << t.tools_other << " holz_store " << t.holz_store
              << " wood_smelter " << t.wood_smelter << " wood_smith " << t.wood_smith << " u "
              << t.u_mine << "," << t.u_ore << "," << t.u_eisen << "," << t.u_tools_flow << "\n";
}

} // namespace iron_chain
