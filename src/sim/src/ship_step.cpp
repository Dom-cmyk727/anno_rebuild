#include <algorithm>
#include <array>

#include <anno/sim/ship_step.hpp>
#include <anno/sim/wide.hpp>

namespace anno::sim {

namespace {

// dir8 code -> unit step on (x, y); y is the original's z. 0 W, 1 SW, 2 S, 3 SE, 4 E, 5 NE, 6 N, 7
// NW (docs/re/ship-navigation.md 4, 7.3; dir16 = 2 * dir8).
constexpr std::array<int, 8> kDx = {-1, -1, 0, 1, 1, 1, 0, -1};
constexpr std::array<int, 8> kDy = {0, 1, 1, 1, 0, -1, -1, -1};

/// The diagonal axis component c = 11863283 / 2^24 of the original's f32 0x3F3504F3 (12.4).
constexpr std::uint64_t kDiagNumerator = 11863283;
constexpr unsigned kDiagShift = 24;

/// Hard stop on the loop of one visit: every iteration either spends budget, pops a node or
/// changes the segment, so it terminates; this only bounds a corrupt state's time (kept valid).
constexpr std::uint32_t kMaxIterations = 1U << 20;

constexpr std::uint32_t kStateMoving = 0x1; // state_word bit 0 (docs/re/ship-navigation.md 7.7)

void install_idle(Figure& f, std::uint64_t remaining_x500) {
    NavMotion& m = *f.nav;
    m.seg = NavSegment{NavSegKind::Idle, 0, 100, kNavIdleSpeedQ32, remaining_x500};
    m.res24[0] = m.res24[1] = 0;
    f.state_word &= ~kStateMoving;
}

void publish(Figure& f) {
    const NavMotion& m = *f.nav;
    f.pos.x = static_cast<std::int32_t>(wide::floor_div(m.pos_u[0], kNavPosUnitsPerQ8));
    f.pos.y = static_cast<std::int32_t>(wide::floor_div(m.pos_u[1], kNavPosUnitsPerQ8));
}

/// Advances the position by `step` units (q32/500 of length) along the current movement segment.
void apply_step(NavMotion& m, std::uint64_t step) {
    const NavSegment& g = m.seg;
    if (g.kind != NavSegKind::Move || step == 0) {
        return;
    }
    const int dx = kDx[g.dir8], dy = kDy[g.dir8];
    if ((g.dir8 & 1U) == 0) {
        m.pos_u[0] += static_cast<std::int64_t>(step) * dx;
        m.pos_u[1] += static_cast<std::int64_t>(step) * dy;
        return;
    }
    const int sign[2] = {dx, dy};
    for (int axis = 0; axis < 2; ++axis) {
        bool carry = false;
        const wide::U128 sum =
            wide::add(wide::mul(step, kDiagNumerator), wide::U128{0, m.res24[axis]}, carry);
        const std::uint64_t magnitude = wide::shr(sum, kDiagShift).lo;
        m.res24[axis] = static_cast<std::uint32_t>(sum.lo & ((std::uint64_t{1} << kDiagShift) - 1));
        m.pos_u[axis] += static_cast<std::int64_t>(magnitude) * sign[axis];
    }
}

/// Leftover budget in the old segment's rate units converted to the new segment's rate: the
/// leftover time is leftover / old_rate ticks, the new budget is that times new_rate, floored
/// (12.4).
std::uint64_t convert_budget(std::uint64_t leftover, std::uint64_t old_rate,
                             std::uint64_t new_rate) {
    if (leftover == 0 || old_rate == new_rate) {
        return leftover;
    }
    wide::U128 q;
    std::uint64_t r = 0;
    if (!wide::divmod(wide::mul(leftover, new_rate), old_rate, q, r)) {
        return 0;
    }
    return q.hi != 0 ? ~std::uint64_t{0} : q.lo;
}

std::uint64_t rate_of(const NavSegment& g) {
    return std::uint64_t{g.speed_q32} * g.factor_percent;
}

/// The next node becomes the current segment (AnnoWelt 0x1003e930).
void start_node(Figure& f, const NavStepEnv& env) {
    NavMotion& m = *f.nav;
    const NavNode node = m.nodes.front();
    m.res24[0] = m.res24[1] = 0;
    if (node.kind == NavSegKind::Turn) {
        // The planner's turn node (kind 3): the facing is set from the node, it lasts one turn
        // step.
        f.dir16 = node.facing & 15U;
        m.seg = NavSegment{NavSegKind::Turn, 0, 100, kNavIdleSpeedQ32, node.length_q32 * kNavScale};
        m.nodes.erase(m.nodes.begin());
        f.state_word &= ~kStateMoving;
        return;
    }
    const unsigned want = nav_facing_of_dir8(node.dir8);
    const unsigned ds = (want - f.dir16) & 15U;
    const unsigned d = std::min(ds, 16U - ds);
    if (d >= 3) {
        // The stepper's own turn phase: one 1/16 step (a reversal turns -1), the facing changes
        // now, the node stays for the next start.
        f.dir16 = static_cast<std::uint8_t>((f.dir16 + (ds >= 8 ? 15U : 1U)) & 15U);
        m.seg = NavSegment{NavSegKind::TurnPhase, 0, 100, kNavIdleSpeedQ32,
                           std::uint64_t{env.facts->turn_step_q32} * kNavScale};
        f.state_word |= kStateMoving;
        return;
    }
    f.dir16 = static_cast<std::uint8_t>(want);
    std::uint64_t chunk = node.length_q32;
    if (node.length_q32 > kNavChunkLimitQ32) { // 1.0 + f32(1e-4), see ship.hpp
        chunk = kNavUnitQ32;
        m.nodes.front().length_q32 -= kNavUnitQ32;
    } else {
        m.nodes.erase(m.nodes.begin());
    }
    m.seg = NavSegment{NavSegKind::Move, node.dir8, env.factor_percent, node.speed_q32,
                       chunk * kNavScale};
    f.state_word |= kStateMoving;
}

/// The segment-end action. Returns true when the visit must stop (planning deferred).
bool end_segment(Figure& f, const NavStepEnv& env, NavPlanner& planner,
                 std::vector<NavEvent>& events) {
    NavMotion& m = *f.nav;
    m.res24[0] = m.res24[1] = 0;
    if (!m.nodes.empty()) {
        start_node(f, env);
        return false;
    }
    if (m.order.has_value() && !m.order->routed) {
        publish(f); // the planner reads the ship where it is
        NavPlan plan = planner.plan(f);
        switch (plan.kind) {
        case NavPlanKind::Deferred:
            install_idle(f, 0); // order pending, no segment: the next visit asks again
            return true;
        case NavPlanKind::Nodes: {
            const auto first_move = std::find_if(plan.nodes.begin(), plan.nodes.end(), [](auto& n) {
                return n.kind == NavSegKind::Move;
            });
            if (first_move != plan.nodes.end()) {
                // Commit (0x100a92a0 then 0x1003f910): the nodes become the list, the first one the
                // current segment with remaining 0, the facing is set directly to the first
                // movement (bypassing the turn rule), no position is written.
                m.nodes = std::move(plan.nodes);
                m.order->routed = true;
                m.ticket.reset();
                f.dir16 = nav_facing_of_dir8(first_move->dir8);
                install_idle(f, 0);
                f.state_word |= kStateMoving;
                return false;
            }
            [[fallthrough]]; // a plan without movement is "no runs"
        }
        case NavPlanKind::Arrived:
            events.push_back(NavEvent{NavEventKind::Arrived, m.order->order_id, {}});
            m.order.reset();
            m.ticket.reset();
            break;
        case NavPlanKind::Failed:
            events.push_back(NavEvent{NavEventKind::RouteFailed, m.order->order_id, plan.reason});
            m.order.reset();
            m.ticket.reset();
            break;
        }
        install_idle(f, kNavUnitQ32 * kNavScale);
        return false;
    }
    if (m.order.has_value()) {
        // A routed order with nothing pending: the last segment finished.
        events.push_back(NavEvent{NavEventKind::Arrived, m.order->order_id, {}});
        m.order.reset();
        m.ticket.reset();
    }
    install_idle(f, kNavUnitQ32 * kNavScale);
    return false;
}

} // namespace

std::uint32_t nav_speed_factor(const NavFacts& facts, const NavRules& rules,
                               std::uint32_t capacity_88, std::span<const ShipHold> holds,
                               std::uint32_t owner_volk_number,
                               std::optional<std::uint32_t> health) {
    std::uint64_t cargo_loss = 0;
    if (owner_volk_number == rules.volk_number) {
        const std::uint64_t total_cap =
            std::uint64_t{facts.hold_count_a + facts.hold_count_b} * capacity_88;
        if (total_cap != 0) {
            std::uint64_t sum = 0;
            const std::size_t n0 = std::min<std::size_t>(facts.hold_count_a, holds.size());
            for (std::size_t i = 0; i < n0; ++i) {
                sum = std::min<std::uint64_t>(total_cap,
                                              sum + std::min(holds[i].amount_88, total_cap));
            }
            cargo_loss = (std::uint64_t{rules.cargo_penalty_percent} * sum) / total_cap;
        }
    }
    const std::uint64_t level = 100 - cargo_loss;
    // trunc(span * hp / max_hp); an unknown health is the full-health term (12.14). max_hp of a
    // loaded ship type is 1 .. 65535 (types.cpp), a zero is guarded like a missing divisor.
    std::uint64_t health_term = rules.health_span_percent;
    if (health.has_value() && facts.max_hp != 0) {
        health_term = std::uint64_t{rules.health_span_percent} *
                      std::min<std::uint64_t>(*health, facts.max_hp) / facts.max_hp;
    }
    const std::uint64_t v = (std::uint64_t{rules.health_floor_percent} + health_term) * level / 100;
    return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(v, 1, 100));
}

void nav_step(Figure& f, const NavStepEnv& env, NavPlanner& planner, std::uint16_t now,
              std::vector<NavEvent>& events) {
    NavMotion& m = *f.nav;
    const std::uint16_t d = static_cast<std::uint16_t>(now - m.last);
    if (d == 0) {
        return;
    }
    std::uint64_t budget = std::uint64_t{d} * rate_of(m.seg);
    for (std::uint32_t iteration = 0; budget != 0 && iteration < kMaxIterations; ++iteration) {
        if (m.seg.remaining_x500 > budget) {
            apply_step(m, budget);
            m.seg.remaining_x500 -= budget;
            budget = 0;
            break;
        }
        const std::uint64_t step = m.seg.remaining_x500;
        apply_step(m, step);
        const std::uint64_t leftover = budget - step;
        const std::uint64_t old_rate = rate_of(m.seg);
        if (end_segment(f, env, planner, events)) {
            break; // deferred: the leftover time is not carried over
        }
        budget = convert_budget(leftover, old_rate, rate_of(m.seg));
    }
    m.last = now;
    publish(f);
}

void nav_stop_motion(Figure& f) {
    NavMotion& m = *f.nav;
    m.nodes.clear();
    NavSegment& g = m.seg;
    const std::uint64_t whole = kNavUnitQ32 * kNavScale;
    if (g.kind == NavSegKind::Move) {
        // remaining -= trunc(remaining) * 1.0: the remainder past the whole chunks is sailed out.
        g.remaining_x500 %= whole;
        if (g.remaining_x500 == 0) {
            m.res24[0] = m.res24[1] = 0;
        }
        return;
    }
    if (g.kind == NavSegKind::TurnPhase) {
        return; // the trim is a no-op on a turn phase (12.1); it finishes, then the ship idles
    }
    if (g.kind == NavSegKind::Idle && g.remaining_x500 <= kNavIdleSpeedQ32 * kNavScale) {
        return; // already the short idle (or a zero-length segment): not re-armed (12.1)
    }
    install_idle(f, kNavIdleSpeedQ32 * kNavScale);
}

} // namespace anno::sim
