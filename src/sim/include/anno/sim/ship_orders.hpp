// MoveCmd and StopCmd (MAP-NAV-2 package E; docs/sim/ship-navigation.md 2, 12.1, 12.2).
#pragma once

#include <anno/sim/apply.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/reject.hpp>

namespace anno::sim {

class World;

struct ShipOrders {
    /// Internal route cleanup: same ownership/nav/parent checks as Stop, but may
    /// stop a blocked ship. Public Stop retains its existing refusal order.
    static ApplyResult stop_route(World&, const Command&, const StopCmd&, const InAdvance*);
    /// Validates (atomically: a refusal changes no state) and applies a MoveCmd: the motion part of
    /// the stop, a new order identity, and a planning ticket (kept when the ship already holds
    /// one).
    static ApplyResult move(World& w, const Command& command, const MoveCmd& cmd,
                            const InAdvance* in_advance);
    /// The same checks, the motion part of the stop, and the order part: order and ticket removed,
    /// a ShipStopped event when an order was cancelled.
    static ApplyResult stop(World& w, const Command& command, const StopCmd& cmd,
                            const InAdvance* in_advance);
};

} // namespace anno::sim
