// MAP-EXTRACT-2 T4 (interruption and lifetime) and T5 (continuation, replay, cost) of the iron
// chain on the endless02 pilot (docs/sim/iron-tools.md). Needs ANNO1503_GAME_DIR; skips with a
// message without it.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/mine_facts.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "iron_chain_fixture.hpp"
#include "iron_pilot.hpp"

namespace sim = anno::sim;
using namespace iron_pilot;
using namespace iron_chain;

namespace {

/// Advances until `pred` holds (checked after every advance); the number of advances, or -1.
template <class Pred> int step_until(Chain& c, int limit, Pred pred) {
    for (int i = 1; i <= limit; ++i) {
        c.pilot.w.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!c.pilot.w.faulted(), c.pilot.w.fault());
        if (pred()) {
            return i;
        }
    }
    return -1;
}

const sim::Mine& mine_state(const Chain& c) {
    return std::get<sim::Mine>(test_must::must(c.pilot.w.find_building(c.mine))->payload);
}

TEST_CASE("iron interruption: probe (opt-in measure)") {
    if (std::getenv("ANNO_IRON_PROBE2") == nullptr) {
        MESSAGE("skipping: set ANNO_IRON_PROBE2=1");
        return;
    }
    // (1) does a carrier lift from the mine that reaches both ores?
    for (const std::pair<int, int> site :
         {std::pair{371, 354}, std::pair{371, 355}, std::pair{370, 356}}) {
        auto c = start_chain(Arm::Real, false, site);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        step_chain(*c, 1000);
        const Terms t = terms_of(*c);
        std::cerr << "SITE " << site.first << "," << site.second << " mine_made " << t.mine_made
                  << " ore_loaded " << t.ore_loaded << " smith_made " << t.smith_made << "\n";
    }
    // (2) a reservation on the mine when the smelter is demolished during its carrier's walk
    auto c = start_chain(Arm::Real, false);
    if (!c) {
        return;
    }
    const int k = step_until(*c, 3000, [&] {
        for (const auto& f : c->pilot.w.figures().list.slots)
            if (f && !f->expiry_stamp && f->home == c->smelter &&
                f->transport.step == sim::TransportStep::ToSource && f->transport.reserved_88 > 0)
                return true;
        return false;
    });
    std::cerr << "RESERVED walk found after " << k << " reserved " << mine_state(*c).reserved
              << "\n";
    demolish_role(*c, 0);
    step_chain(*c, 400);
    std::cerr << "RESERVED after demolition + 400 advances: " << mine_state(*c).reserved << "\n";
}

std::uint64_t notices(sim::World& w, std::uint32_t code) {
    std::uint64_t n = 0;
    for (const sim::SimEvent& e : w.drain_events())
        n += e.kind == sim::SimEventKind::Notice && e.code == code ? 1U : 0U;
    return n;
}

std::uint64_t homed_eisen(const Chain& c, sim::BuildingId home) {
    std::uint64_t ore = 0, eisen = 0, tools = 0;
    homed_hands(c.pilot.w, home, ore, eisen, tools);
    return eisen;
}

/// The nearly spent deposit: a labelled edit that leaves `visible_raw` for a shallow mine.
void leave_visible(Chain& c, std::uint32_t visible_raw) {
    const auto dep = mine_state(c).deposit;
    REQUIRE(dep.has_value());
    sim::WorldTestAccess::islands(c.pilot.w)[1]->deposits.at(*dep).remaining =
        sim::kDepositReserve + visible_raw;
}

TEST_CASE("iron interruption: a shallow deposit exhausted in the depleting setting idles with one "
          "notice and a neighbour ore keeps the mine working") {
    constexpr std::uint32_t kVisible = 4200; // 200 full steps of 21 raw
    struct Run {
        std::uint64_t notice_15b = 0, made_edit = 0, made_last_block = 0;
        std::size_t deposits = 0;
        std::optional<std::size_t> bound;
    };
    const auto run = [&](std::pair<int, int> site) -> std::optional<Run> {
        auto c = start_chain(Arm::Real, false, site);
        if (!c)
            return std::nullopt;
        REQUIRE(sim::WorldTestAccess::settings(c->pilot.w).depletes());
        step_chain(*c, 500);
        static_cast<void>(c->pilot.w.drain_events());
        leave_visible(*c, kVisible);
        Run r;
        const std::uint64_t made0 = terms_of(*c).mine_made;
        std::uint64_t before_last = 0;
        for (int block = 0; block < 6; ++block) {
            before_last = terms_of(*c).mine_made;
            step_chain(*c, 500);
            require_closed(terms_of(*c), "exhaustion");
            r.notice_15b += notices(c->pilot.w, 0x15B);
        }
        const Terms t = terms_of(*c);
        r.made_edit = t.mine_made - made0;
        r.made_last_block = t.mine_made - before_last;
        r.deposits = isle(c->pilot.w).deposits.size();
        r.bound = mine_state(*c).deposit;
        return r;
    };
    const auto idle = run({366, 353});  // reaches the ore (375,353) only
    const auto neigh = run({371, 354}); // reaches (375,353) and (379,361)
    if (!idle || !neigh) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // only the spent ore in reach: the mine takes exactly what was left, raises the notice 0x15B
    // once (deep ore remains under the reserve), then idles: no rebinding, nothing more is made
    CHECK(idle->notice_15b == 1);
    CHECK(idle->made_edit == kVisible);
    CHECK(idle->made_last_block == 0);
    CHECK(idle->deposits == 1);
    CHECK(idle->bound == std::optional<std::size_t>{0});
    // the control: a second ore in reach is bound by the short step (its deposit materialised), no
    // notice, and the mine keeps working in the last block
    CHECK(neigh->notice_15b == 0);
    CHECK(neigh->deposits == 2);
    CHECK(neigh->bound == std::optional<std::size_t>{1});
    CHECK(neigh->made_edit > kVisible);
    CHECK(neigh->made_last_block > 0);
}

TEST_CASE("iron interruption: missing wood stalls the smelter and the toolsmith and restoring it "
          "resumes them") {
    auto c = start_chain(Arm::Real, false);
    if (!c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = c->pilot.w;
    step_chain(*c, 1000);
    require_closed(terms_of(*c), "running");
    REQUIRE(terms_of(*c).smith_made > 0);
    // labelled interruption: the foresters demolished by command, the store's wood and both wood
    // inputs emptied
    sim::City& city = *test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city));
    {
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city.index.lists)
            for (const auto& b : l.slots)
                if (b && b->type.v == 801)
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    }
    const auto set_wood = [&](std::uint32_t store, std::uint32_t input) {
        city.stock.stock[kHolz] = store;
        for (const sim::BuildingId id : {c->smelter, c->smith})
            for (auto& l : city.index.lists)
                for (auto& b : l.slots)
                    if (b && b->id == id)
                        std::get<sim::Workshop>(b->payload).in.at(1) = input;
    };
    set_wood(0, 0);
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    step_chain(*c, 1000); // the stocks in the pipeline drain
    require_closed(terms_of(*c), "draining");
    const Terms a = terms_of(*c);
    step_chain(*c, 1000);
    const Terms b = terms_of(*c);
    require_closed(b, "stalled");
    // stalled: nothing more is smelted or forged, the ore waits (the mine fills and stops)
    CHECK(b.smelter_made == a.smelter_made);
    CHECK(b.smith_made == a.smith_made);
    CHECK(b.tools_unloaded == a.tools_unloaded);
    // the control: wood is given back (labelled edit) and both resume
    set_wood(60000, 1536);
    step_chain(*c, 1500);
    const Terms r = terms_of(*c);
    require_closed(r, "resumed");
    CHECK(r.smelter_made > b.smelter_made);
    CHECK(r.smith_made > b.smith_made);
}

TEST_CASE("iron interruption: the toolsmith demolished with EISEN in its carrier's hands and then "
          "rebuilt") {
    auto c = start_chain(Arm::Real, false);
    if (!c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = c->pilot.w;
    const int k = step_until(*c, 4000, [&] { return homed_eisen(*c, c->smith) > 0; });
    REQUIRE_MESSAGE(k > 0, "the carrier never held EISEN");
    require_closed(terms_of(*c), "before the interruption");
    demolish_role(*c, 1);
    // valid at once, before any tick: invariants, a closed ledger with the lost cargo NAMED, and a
    // save/load round trip
    CHECK(w.check_invariants().empty());
    CHECK(c->retired.hands_eisen > 0);
    require_closed(terms_of(*c), "at the instant of the interruption");
    {
        const auto bytes = w.save();
        auto loaded = sim::World::load(bytes, c->pilot.types, {});
        REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
        CHECK(loaded.value().save() == bytes);
        CHECK(loaded.value().state_hash() == w.state_hash());
    }
    step_chain(*c, 1500);
    const Terms stalled = terms_of(*c);
    require_closed(stalled, "stalled after the demolition");
    // the lost term, named: the carrier's cargo of the instant ended with the figure and the E4
    // ledger counted it exactly once as lost EISEN
    CHECK(stalled.eisen_lost == c->retired.hands_eisen);
    step_chain(*c, 500);
    const Terms still = terms_of(*c);
    require_closed(still, "still stalled");
    CHECK(still.smith_made == stalled.smith_made); // no toolsmith, no WERKZEUG
    CHECK(still.tools_unloaded == stalled.tools_unloaded);
    // rebuilt by a public command on the same site: a new building with a new id; the chain resumes
    const sim::ApplyResult r = w.apply(build_cmd(1019, 366, 350, 2));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    c->smith = r.records.at(0).building;
    c->smiths.push_back(c->smith);
    step_chain(*c, 3000);
    const Terms resumed = terms_of(*c);
    require_closed(resumed, "resumed after the rebuild");
    CHECK(resumed.smith_made > still.smith_made);
    CHECK(resumed.tools_unloaded > still.tools_unloaded);
}

TEST_CASE(
    "iron interruption: the timed edge of an unload - a demolition one advance before or after "
    "it") {
    // the advance in which the toolsmith's carrier delivers its EISEN: its cargo goes from > 0 to 0
    int unload_at = -1;
    {
        auto c = start_chain(Arm::Real, false);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        std::uint64_t prev = 0;
        for (int i = 1; i <= 4000 && unload_at < 0; ++i) {
            step_chain(*c, 1);
            const std::uint64_t now = homed_eisen(*c, c->smith);
            if (prev > 0 && now == 0)
                unload_at = i;
            prev = now;
        }
        REQUIRE_MESSAGE(unload_at > 1, "no delivery seen");
    }
    for (const int offset : {-1, 0}) {
        INFO("demolition at advance " << unload_at + offset << " (the unload is advance "
                                      << unload_at << ")");
        auto c = start_chain(Arm::Real, false);
        REQUIRE(c);
        step_chain(*c, unload_at + offset);
        const bool carrying = homed_eisen(*c, c->smith) > 0;
        CHECK(carrying == (offset < 0));
        require_closed(terms_of(*c), "before the edge");
        demolish_role(*c, 1);
        CHECK(c->pilot.w.check_invariants().empty());
        const Terms at = terms_of(*c);
        require_closed(at, "at the edge");
        if (offset < 0) {
            CHECK(c->retired.hands_eisen > 0); // the cargo was still in the hands: lost with them
        } else {
            CHECK(c->retired.hands_eisen == 0); // it had been delivered ...
            CHECK(c->retired.smith_in_eisen >
                  0); // ... into the input, which vanished with the building
        }
        step_chain(*c, 300);
        require_closed(terms_of(*c), "after the edge");
        CHECK(terms_of(*c).eisen_lost == c->retired.hands_eisen); // named, counted once
    }
}

TEST_CASE(
    "iron interruption: a demolished smelter releases the reservation its carrier held on the "
    "mine") {
    auto c = start_chain(Arm::Real, false);
    if (!c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const int k = step_until(*c, 4000, [&] {
        for (const auto& f : c->pilot.w.figures().list.slots)
            if (f && !f->expiry_stamp && f->home == c->smelter &&
                f->transport.step == sim::TransportStep::ToSource && f->transport.reserved_88 > 0)
                return true;
        return false;
    });
    REQUIRE_MESSAGE(k > 0, "no reserving carrier seen");
    CHECK(mine_state(*c).reserved > 0); // the claim is held
    demolish_role(*c, 0);
    CHECK(c->pilot.w.check_invariants().empty());
    step_chain(*c, 400);
    CHECK(mine_state(*c).reserved == 0); // released exactly once: nothing left on the mine
    require_closed(terms_of(*c), "claims");
    // the control: a rebuilt smelter lifts from the mine again
    const sim::ApplyResult r = c->pilot.w.apply(build_cmd(1017, 367, 346, 0));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    c->smelter = r.records.at(0).building;
    c->smelters.push_back(c->smelter);
    const std::uint64_t loaded0 = terms_of(*c).ore_loaded;
    step_chain(*c, 2000);
    require_closed(terms_of(*c), "rebuilt");
    CHECK(terms_of(*c).ore_loaded > loaded0);
}

TEST_CASE("iron interruption: the same slicing reproduces the state exactly and another slicing "
          "differs within a bound") {
    auto a1 = start_chain(Arm::Real, false);
    auto a2 = start_chain(Arm::Real, false);
    auto b1 = start_chain(Arm::Real, false);
    auto b2 = start_chain(Arm::Real, false);
    if (!a1 || !a2 || !b1 || !b2) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // the same 150 s of game time cut two ways: 1,500 x 100 ms, and 1,500 x (0 + 1 + 99 ms)
    for (Chain* c : {a1.get(), a2.get()})
        for (int i = 0; i < 1500; ++i)
            c->pilot.w.advance(100);
    for (Chain* c : {b1.get(), b2.get()})
        for (int i = 0; i < 1500; ++i) {
            c->pilot.w.advance(0);
            c->pilot.w.advance(1);
            c->pilot.w.advance(99);
        }
    for (Chain* c : {a1.get(), a2.get(), b1.get(), b2.get()}) {
        REQUIRE_MESSAGE(!c->pilot.w.faulted(), c->pilot.w.fault());
        require_closed(terms_of(*c), "sliced");
    }
    CHECK(a1->pilot.w.clock().tick() == a2->pilot.w.clock().tick());
    CHECK(a1->pilot.w.clock().tick() == b1->pilot.w.clock().tick());
    // the same slicing: identical, byte for byte
    CHECK(a1->pilot.w.state_hash() == a2->pilot.w.state_hash());
    CHECK(a1->pilot.w.save() == a2->pilot.w.save());
    CHECK(b1->pilot.w.state_hash() == b2->pilot.w.state_hash());
    CHECK(b1->pilot.w.save() == b2->pilot.w.save());
    // another slicing is another run (the zero-time and 1 ms advances change when figures are
    // visited): the states differ, the chain's output within 15 % (a measured band)
    CHECK(a1->pilot.w.state_hash() != b1->pilot.w.state_hash());
    const Terms ta = terms_of(*a1), tb = terms_of(*b1);
    std::cerr << "SLICING a made " << ta.smith_made << " b made " << tb.smith_made << "\n";
    CHECK(tb.smith_made * 100 >= ta.smith_made * 85);
    CHECK(tb.smith_made * 100 <= ta.smith_made * 115);
}

} // namespace
