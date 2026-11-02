// The ship mover: one registry visit of a navigating ship (MAP-NAV-2 package E). It installs the
// real planner (fair admission, route query over the world's coast index) behind the stepper of
// ship_step.hpp and mirrors the ship's position to its crew through Effects. Dispatch by figure
// kind: the scheduler sends every figure that carries NavMotion here instead of to the figure-tick
// seam or the transport body (docs/sim/ship-navigation.md 4).
#pragma once

#include <cstdint>

#include <anno/sim/figure.hpp>
#include <anno/sim/tick.hpp>

namespace anno::sim {

class World;

struct ShipMover {
    /// One visit at the world tick `ctx.t`. `plans_started` counts the route plans admitted in the
    /// current Advance (the quota of 12.2); the caller owns it and resets it every Advance.
    static void visit(World& w, const TickContext& ctx, Figure& ship, Effects& fx,
                      std::uint32_t& plans_started);
};

} // namespace anno::sim
