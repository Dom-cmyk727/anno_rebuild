// The ship mover: one registry visit of a navigating ship (MAP-NAV-2 package E). It installs the
// real planner (fair admission, route query over the world's coast index) behind the stepper of
// ship_step.hpp and mirrors the ship's position to its crew through Effects. Dispatch by figure
// kind: the scheduler sends every figure that carries NavMotion here instead of to the figure-tick
// seam or the transport body (docs/sim/ship-navigation.md 4).
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <anno/sim/figure.hpp>
#include <anno/sim/route.hpp>
#include <anno/sim/route_cache.hpp>
#include <anno/sim/tick.hpp>

namespace anno::sim {

class World;

/// The node list of a route (12.7): turn nodes before the first run only, then one movement node
/// per run, from the facing at plan time. Exposed for the `plannodes` vectors.
[[nodiscard]] std::vector<NavNode> nav_nodes_of_runs(const std::vector<RouteRun>& runs,
                                                     unsigned facing, const NavFacts& facts);

struct ShipMover {
    /// The velocity of the current segment as the stamp consumes it (`unit +0x20`): direction *
    /// speed * the segment's captured factor, in 2^-32 tile per stepper unit, {x, z}; {0, 0} for a
    /// turn, idle or other non-movement segment. Truncated toward zero (error < 2^-32 tile, 25
    /// times that in the stamp offset).
    [[nodiscard]] static std::array<std::int32_t, 2> stamp_velocity_q32(const NavSegment& seg);

    /// The stamps the planner hands the route engine for `ship`: the look-ahead points of the
    /// other ships near it (route_stamps.hpp). Public so tests can read what a moving world
    /// really produces.
    [[nodiscard]] static std::vector<RouteStamp> local_stamps(const World& w, const Figure& ship,
                                                              NavWork* work = nullptr);

    /// One visit at the world tick `ctx.t`. `plans_started` counts the route plans admitted in the
    /// current Advance (the quota of 12.2); the caller owns it and resets it every Advance.
    static void visit(World& w, const TickContext& ctx, Figure& ship, Effects& fx,
                      std::uint32_t& plans_started);
};

} // namespace anno::sim
