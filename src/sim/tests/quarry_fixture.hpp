// The quarry fixture (MAP-EXTRACT-2 part Q3): the iron pilot (endless02 island 1, owner 8) with the
// imported mason and quarry removed by a labelled setup step, then quarries and masons built by
// public commands, with scoped ledgers (stone in 8.8 raw units, bricks) whose unexplained terms
// must be zero. Shared by quarry_test.cpp.
#pragma once

#include <algorithm>
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
#include <anno/sim/mason.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "iron_pilot.hpp"
#include "test_must.hpp"
#include "world_access.hpp"

namespace quarry_fix {

namespace sim = anno::sim;
using namespace iron_pilot;

inline constexpr sim::GoodsId kStein = 5, kMarmorstein = 6, kZiegel = 43, kMamor = 44;

struct Site {
    std::uint32_t type;
    std::int32_t x, y;
    std::uint8_t rot = 0;
};

struct Fix {
    World pilot;
    sim::TransportStats stats;
    std::vector<sim::BuildingId> masons, quarries; // every id ever built (stats persist by id)
    // what vanished with a demolished mason (named terms)
    struct Retired {
        std::uint64_t raw = 0, out = 0, hands_stone = 0;
    } retired;
    std::uint64_t bricks0 = 0, spent_bricks_88 = 0;
    explicit Fix(World&& p) : pilot(std::move(p)) {}
};

inline const sim::Building* building(const sim::World& w, sim::BuildingId id) {
    return id.valid() ? w.find_building(id) : nullptr;
}

/// The standard layouts (all legal on the pilot, measured): the one-mason layout and the two-mason
/// layout around one quarry.
inline constexpr Site kMasonA{804, 358, 350}, kQuarryA{1201, 362, 355};
inline constexpr Site kQuarryB{1208, 361, 357}, kMasonB1{804, 358, 352}, kMasonB2{804, 364, 354};

/// Demolishes the imported mason and quarry (labelled setup: from here every stone and brick of
/// the pilot comes from what the test builds), then builds `quarries` and `masons`.
inline std::unique_ptr<Fix> start(const std::vector<Site>& quarries,
                                  const std::vector<Site>& masons, bool isolate = true,
                                  std::uint32_t extra_flags = 0) {
    auto p = make(true, false, 1000);
    if (!p) {
        return nullptr;
    }
    auto f = std::make_unique<Fix>(std::move(*p));
    sim::World& w = f->pilot.w;
    if (extra_flags != 0) {
        // labelled setup: stage bits the pilot's owner has not reached (the marble stage 13)
        test_must::must(sim::WorldTestAccess::players(w).find(kPlayer))->flags |= extra_flags;
    }
    if (isolate) {
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city_of(w).index.lists)
            for (const auto& b : l.slots)
                if (b && (b->type.v == 804 || b->type.v == 1201))
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    }
    for (const Site& s : quarries) {
        const sim::ApplyResult r = w.apply(build_cmd(s.type, s.x, s.y, s.rot));
        REQUIRE_MESSAGE(r.ok(), "quarry " << s.type << ": " << sim::reject_name(r.status));
        f->quarries.push_back(r.records.at(0).building);
    }
    for (const Site& s : masons) {
        const sim::ApplyResult r = w.apply(build_cmd(s.type, s.x, s.y, s.rot));
        REQUIRE_MESSAGE(r.ok(), "mason " << s.type << ": " << sim::reject_name(r.status));
        f->masons.push_back(r.records.at(0).building);
    }
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    w.set_transport_stats(&f->stats);
    f->bricks0 = city_of(w).stock.stock[kZiegel];
    return f;
}

inline void step(Fix& f, int n) {
    for (int i = 0; i < n; ++i) {
        f.pilot.w.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!f.pilot.w.faulted(), f.pilot.w.fault());
    }
}

template <class P> int step_until(Fix& f, int max, P pred) {
    for (int i = 1; i <= max; ++i) {
        step(f, 1);
        if (pred())
            return i;
    }
    return -1;
}

/// The live mason workers (job 0x2c, not ended) homed at one of the fixture's masons (the imported
/// mason is demolished by the setup, but a worker it sent may still be on its way).
inline std::vector<const sim::Figure*> workers(const Fix& fx) {
    std::vector<const sim::Figure*> out;
    for (const auto& f : fx.pilot.w.figures().list.slots)
        if (f && f->harvest.has_value() && sim::mason_job(f->job) && !f->expiry_stamp.has_value() &&
            std::find(fx.masons.begin(), fx.masons.end(), f->home) != fx.masons.end())
            out.push_back(&*f);
    return out;
}

inline std::size_t workers_in(const Fix& fx, sim::HarvestStep s) {
    std::size_t n = 0;
    for (const sim::Figure* f : workers(fx))
        n += f->harvest->step == s ? 1U : 0U;
    return n;
}

/// Live workers on their way to or waiting at a quarry (they hold a claim).
inline std::size_t claimants(const Fix& fx) {
    return workers_in(fx, sim::HarvestStep::ToTree) + workers_in(fx, sim::HarvestStep::Cut);
}

inline const std::vector<std::uint8_t>& claims_of(const sim::World& w, sim::BuildingId q) {
    static const std::vector<std::uint8_t> none;
    const sim::Building* b = building(w, q);
    const auto* qp = b != nullptr ? std::get_if<sim::Quarry>(&b->payload) : nullptr;
    return qp != nullptr ? qp->claims : none;
}

inline std::size_t set_claims(const sim::World& w, sim::BuildingId q) {
    std::size_t n = 0;
    for (const bool c : claims_of(w, q))
        n += c ? 1U : 0U;
    return n;
}

struct Terms {
    // stone (goods STEINE or MARMORSTEIN), 8.8 raw
    std::uint64_t lifted = 0, hands = 0, lost = 0, delivered = 0, raw_now = 0, consumed = 0;
    // bricks (goods ZIEGEL or MAMOR)
    std::uint64_t made = 0, out_now = 0, brick_hands = 0, brick_lost = 0, unloaded = 0, store = 0;
    std::int64_t u_lift = 0, u_raw = 0, u_bricks_flow = 0, bricks_other = 0;
};

inline Terms terms_of(Fix& f, sim::GoodsId stone = kStein, sim::GoodsId brick = kZiegel) {
    Terms t;
    sim::World& w = f.pilot.w;
    const sim::TransportStats& st = f.stats;
    const auto got = [](const auto& m, const auto& k) -> std::uint64_t {
        const auto it = m.find(k);
        return it == m.end() ? 0U : it->second;
    };
    for (const sim::BuildingId q : f.quarries)
        t.lifted += got(st.forestry.quarry_lifted_88, q);
    for (const sim::BuildingId m : f.masons) {
        t.lost += got(st.forestry.mason_lost_88, m);
        t.delivered += got(st.forestry.mason_delivered_88, m);
        if (const auto it = st.forestry.farms.find(m); it != st.forestry.farms.end()) {
            t.consumed += it->second.consumed_88;
            t.made += it->second.made_88;
        }
        if (const sim::Building* b = building(w, m)) {
            const auto& farm = std::get<sim::Farm>(b->payload);
            t.raw_now += farm.raw;
            t.out_now += farm.out;
        }
    }
    t.raw_now += f.retired.raw;
    t.out_now += f.retired.out;
    for (const auto& fig : w.figures().list.slots) {
        if (!fig || fig->expiry_stamp.has_value() || fig->resident)
            continue;
        if (fig->cargo_goods == stone && fig->harvest.has_value() && sim::mason_job(fig->job))
            t.hands += fig->cargo_88;
        if (fig->cargo_goods == brick)
            t.brick_hands += fig->cargo_88;
    }
    t.brick_lost = got(st.cart_lost, brick);
    t.unloaded = got(st.unload_offered, brick);
    t.store = city_of(w).stock.stock[brick];
    const auto I = [](std::uint64_t v) { return static_cast<std::int64_t>(v); };
    // stone lifted from the quarries = in the workers' hands + lost with them + delivered
    t.u_lift = I(t.lifted) - I(t.hands) - I(t.lost) - I(t.delivered);
    // delivered to the raw stores = the stores' change (they start at 0) + consumed by the steps
    t.u_raw = I(t.delivered) - (I(t.raw_now) + I(t.consumed));
    // bricks made = in the output stores + in the carts' hands + lost + unloaded by the carts
    t.u_bricks_flow = I(t.made) - I(t.out_now) - I(t.brick_hands) - I(t.brick_lost) - I(t.unloaded);
    // the store: change = unloaded - spent by the test's BuildCmds - other consumers (reported)
    t.bricks_other = I(t.unloaded) - I(f.spent_bricks_88) - (I(t.store) - I(f.bricks0));
    return t;
}

inline void require_closed(const Terms& t, const char* label) {
    INFO("ledger " << label);
    CHECK(t.u_lift == 0);
    CHECK(t.u_raw == 0);
    CHECK(t.u_bricks_flow == 0);
}

inline void say(const char* label, const Terms& t) {
    std::cerr << "QUARRY " << label << " lifted " << t.lifted << " delivered " << t.delivered
              << " raw " << t.raw_now << " consumed " << t.consumed << " made " << t.made << " out "
              << t.out_now << " unloaded " << t.unloaded << " store " << t.store << " other "
              << t.bricks_other << " u " << t.u_lift << "," << t.u_raw << "," << t.u_bricks_flow
              << "\n";
}

} // namespace quarry_fix
