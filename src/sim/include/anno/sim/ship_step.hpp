// The ship stepper: movement segments, turns, speed changes and stops (task MAP-NAV-2 package D;
// contract docs/sim/ship-navigation.md 12.4, original docs/re/ship-navigation.md 7.2, 7.3, 8).
// Pure functions over one Figure's NavMotion: no world access, no allocation beyond the node list,
// integers only. The planner (route query and admission) is a callback so the stepper can be tested
// alone and package E can attach the real one.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/ship.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// Why a routed order ended without arriving (docs/sim/ship-navigation.md 12.1).
enum class NavFailReason : std::uint8_t {
    Unreachable = 0,
    BudgetExceeded = 1,
    OutsideWindow = 2,
    ShipBlocked = 3 ///< the ship entered a blocked state (state_word & move_blocked_mask)
};

enum class NavPlanKind : std::uint8_t {
    Nodes,    ///< a plan: the nodes, installed as the route (status 1 with runs)
    Arrived,  ///< status 2: no runs, the order is consumed and the ship idles
    Failed,   ///< the plan failed: the order is cleared, `reason` says why
    Deferred, ///< not planned this visit (quota): the order stays pending
};

struct NavPlan {
    NavPlanKind kind = NavPlanKind::Failed;
    NavFailReason reason = NavFailReason::Unreachable;
    std::vector<NavNode> nodes; ///< Nodes only: turn nodes (before the first run) then one per run
};

/// What the stepper asks when a segment ends, nothing is pending and an unrouted order waits (the
/// original's order handler, slot +0x178). Package E implements it with the route query and the
/// fair admission; the stepper tests use a scripted one.
class NavPlanner {
  public:
    virtual ~NavPlanner() = default;
    [[nodiscard]] virtual NavPlan plan(const Figure& ship) = 0;
};

enum class NavEventKind : std::uint8_t { Arrived, RouteFailed };

struct NavEvent {
    NavEventKind kind = NavEventKind::Arrived;
    std::uint64_t order_id = 0;
    NavFailReason reason = NavFailReason::Unreachable; ///< RouteFailed only
    friend constexpr bool operator==(const NavEvent&, const NavEvent&) = default;
};

/// The per-visit inputs that do not live in the figure.
struct NavStepEnv {
    const NavFacts* facts = nullptr;
    /// The speed factor V (1 .. 100 percent) read at every movement segment start.
    std::uint32_t factor_percent = 100;
};

/// The speed factor V of a ship (docs/re/ship-navigation.md 7.5, with the adaptations of
/// docs/sim/ship-navigation.md 12.8): `L = 100 - trunc(penalty * S / ((n0 + n1) * cap))` when the
/// owner's Volk number equals `rules.volk_number` (S = the first n0 holds' amounts, clamped to
/// (n0 + n1) * cap; a zero divisor gives no cargo term), then
/// `V = trunc((health_floor + trunc(span * hp / max_hp)) * L / 100)`. `health` is the ship's
/// current hit points (NavMotion::health, 0 .. max_hp); nullopt (unknown) uses hp = max_hp, an
/// adaptation (docs/sim/ship-navigation.md 12.14); a value above max_hp is clamped to max_hp. The
/// result is in 1 .. 100 for valid Rules.
[[nodiscard]] std::uint32_t nav_speed_factor(const NavFacts& facts, const NavRules& rules,
                                             std::uint32_t capacity_88,
                                             std::span<const ShipHold> holds,
                                             std::uint32_t owner_volk_number,
                                             std::optional<std::uint32_t> health);

/// One stepper visit at tick `now` (u16 wrap handled): consumes d = u16(now - last) ticks of
/// movement along the segment list, carrying leftover time across segment boundaries inside the
/// visit, and publishes Figure::pos. Events are appended in order. Does nothing when d == 0.
void nav_step(Figure& ship, const NavStepEnv& env, NavPlanner& planner, std::uint16_t now,
              std::vector<NavEvent>& events);

/// The motion part of the original stop (docs/sim/ship-navigation.md 12.1): clears the node list; a
/// moving movement segment keeps only the remainder past its whole 1.0-length chunks and is then
/// sailed out; a stepper turn phase finishes; any other segment becomes the idle segment with
/// remaining = speed = 0.01 unless it already is one with that much or less left. Does not touch
/// the order or the ticket (the commands do).
void nav_stop_motion(Figure& ship);

/// The facing a movement of `dir8` needs (2 * dir8 on the 16-direction circle).
[[nodiscard]] constexpr std::uint8_t nav_facing_of_dir8(std::uint8_t dir8) {
    return static_cast<std::uint8_t>(2U * dir8);
}

} // namespace anno::sim
