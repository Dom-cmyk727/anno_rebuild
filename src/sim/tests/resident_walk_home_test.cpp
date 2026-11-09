// MAP-EXTRACT-2 T3-Q2: the resident's walk home is the carriers' (docs/sim/population-live.md 27).
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include <doctest/doctest.h>

#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "iron_pilot.hpp"

namespace {

namespace sim = anno::sim;
using namespace iron_pilot;

TEST_CASE("resident walk home: cost measure on the 7-city fixture (opt-in)") {
    if (std::getenv("ANNO_RESIDENT_MEASURE") == nullptr) {
        MESSAGE("skipping: set ANNO_RESIDENT_MEASURE=1");
        return;
    }
    auto p = make(true, false, 1000);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::TransportStats stats;
    p->w.set_transport_stats(&stats);
    constexpr int kAdvances = 3000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kAdvances; ++i) {
        p->w.advance(kAdvanceMs);
        REQUIRE(!p->w.faulted());
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    const sim::ResidentStats& r = stats.residents;
    const sim::HouseWork tot = stats.population.total();
    std::cerr << "RESIDENT-COST advances " << kAdvances << " wall_ms " << ms << " ms_per_advance "
              << ms / kAdvances << " stand_searches " << r.searches << " stand_failed "
              << r.searches_failed << " arrivals " << r.arrivals << " deliveries " << r.deliveries
              << " delivered_88 " << r.delivered_88 << " lost_88 " << r.lost_88 << " no_path_home "
              << r.no_path_home << " stand_walk_searches " << tot.walk_searches
              << " stand_walk_expansions " << tot.walk_expansions << " state_hash " << std::hex
              << p->w.state_hash() << std::dec << "\n";
}

} // namespace
