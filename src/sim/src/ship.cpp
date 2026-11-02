#include <map>
#include <set>
#include <string>

#include <anno/sim/ship.hpp>
#include <anno/sim/wide.hpp>

namespace anno::sim {

NavMotion make_nav_motion(FixedPos pos, std::uint16_t tick_low) {
    NavMotion m;
    m.seg.kind = NavSegKind::Idle;
    m.seg.speed_q32 = kNavIdleSpeedQ32;
    m.seg.remaining_x500 = kNavIdleSpeedQ32 * kNavScale; // remaining = speed = 0.01 after a load
    m.seg.factor_percent = 100;
    m.last = tick_low;
    m.pos_u[0] = std::int64_t{pos.x} * kNavPosUnitsPerQ8;
    m.pos_u[1] = std::int64_t{pos.y} * kNavPosUnitsPerQ8;
    return m;
}

std::string validate_navigation(const FigureRegistry& registry, const IdCounters& ids,
                                const TypeRegistry& types, const Rules& rules) {
    std::set<std::uint64_t> orders, tickets;
    for (std::size_t s = 0; s < registry.list.slots.size(); ++s) {
        if (!registry.list.slots[s].has_value()) {
            continue;
        }
        const Figure& f = *registry.list.slots[s];
        const std::string where = "figures.list.slots[" + std::to_string(s) + "]";
        const FigureType* type = types.find_figure_type(f.kind);
        const bool navigates = type != nullptr && type->nav.has_value();
        if (navigates != f.nav.has_value()) {
            return where + ".nav: " + (navigates ? "missing" : "present") + " for figure type " +
                   std::to_string(f.kind) + (navigates ? ", which navigates" : ", which does not");
        }
        if (!navigates) {
            continue;
        }
        const NavFacts& facts = *type->nav;
        const NavMotion& m = *f.nav;
        if (facts.nav_class >= rules.nav.class_costs.size()) {
            return where + ".nav: figure type " + std::to_string(f.kind) + " has nav_class " +
                   std::to_string(facts.nav_class) + ", Rules.nav.class_costs holds " +
                   std::to_string(rules.nav.class_costs.size());
        }
        const NavSegment& g = m.seg;
        if (g.factor_percent < 1 || g.factor_percent > 100) {
            return where + ".nav.seg.factor_percent: " + std::to_string(g.factor_percent) +
                   " is not in 1 .. 100";
        }
        if (g.kind != NavSegKind::Move && (g.factor_percent != 100 || g.dir8 != 0)) {
            return where + ".nav.seg: a turn or idle segment has factor 100 and dir8 0 (V = 100)";
        }
        {
            const std::int64_t edge = std::int64_t{kWorldEdgeBound} * 256;
            if (f.pos.x < -edge || f.pos.x > edge || f.pos.y < -edge || f.pos.y > edge) {
                return where + ".pos: outside the world bound of " +
                       std::to_string(kWorldEdgeBound) + " tiles";
            }
        }
        switch (g.kind) {
        case NavSegKind::Move:
            if (g.dir8 > 7) {
                return where + ".nav.seg.dir8: " + std::to_string(g.dir8) + " is not in 0 .. 7";
            }
            if (g.speed_q32 == 0 || g.speed_q32 >= kMaxNavSpeedQ32) {
                return where + ".nav.seg.speed_q32: outside the supported domain";
            }
            if (g.remaining_x500 > kNavUnitQ32 * kNavScale) {
                return where + ".nav.seg.remaining_x500: a movement chunk is at most 1.0 length";
            }
            if (f.dir16 != 2 * g.dir8) {
                return where + ".nav: a movement segment needs the facing 2 * dir8 (an odd or "
                               "different facing is invalid)";
            }
            break;
        case NavSegKind::Turn:
        case NavSegKind::TurnPhase:
            if (g.speed_q32 != kNavIdleSpeedQ32) {
                return where + ".nav.seg.speed_q32: a turn segment runs at 0.01";
            }
            if (g.remaining_x500 > std::uint64_t{facts.turn_step_q32} * kNavScale) {
                return where + ".nav.seg.remaining_x500: above the type's turn step";
            }
            break;
        case NavSegKind::Idle:
            if (g.speed_q32 != kNavIdleSpeedQ32 || g.remaining_x500 > kNavUnitQ32 * kNavScale) {
                return where + ".nav.seg: the idle segment runs at 0.01 with at most 1.0 left";
            }
            break;
        }
        if (f.dir16 > 15) {
            return where + ".dir16: above 15";
        }
        if (m.nodes.size() > kNavMaxNodes) {
            return where + ".nav.nodes: " + std::to_string(m.nodes.size()) +
                   " pending nodes exceed the bound " + std::to_string(kNavMaxNodes);
        }
        for (std::size_t i = 0; i < m.nodes.size(); ++i) {
            const NavNode& n = m.nodes[i];
            const std::string at = where + ".nav.nodes[" + std::to_string(i) + "]";
            if (n.kind == NavSegKind::Move) {
                // A run of 1 .. run_cap whole steps; chunking (12.4) cuts whole 1.0 units off the
                // front of the length, so an orthogonal length stays a whole number of units and a
                // diagonal one only stays within the run bound.
                const std::uint64_t step = (n.dir8 & 1U) != 0 ? kNavDiagStepQ32 : kNavUnitQ32;
                if (n.dir8 > 7 || n.facing != 0 || n.speed_q32 == 0 ||
                    n.speed_q32 >= kMaxNavSpeedQ32 || n.length_q32 == 0 ||
                    ((n.dir8 & 1U) == 0 && n.length_q32 % kNavUnitQ32 != 0) ||
                    n.length_q32 > step * rules.nav.run_cap) {
                    return at + ": a movement node is out of range (1 .. run_cap steps of its "
                                "direction, facing 0)";
                }
            } else if (n.kind == NavSegKind::Turn) {
                if (n.facing > 15 || n.dir8 != 0 || n.length_q32 != facts.turn_step_q32 ||
                    n.speed_q32 != kNavIdleSpeedQ32) {
                    return at + ": a turn node must carry facing < 16, the type's turn step and "
                                "speed 0.01";
                }
            } else {
                return at + ": a node is a movement or a turn";
            }
        }
        if (m.order.has_value()) {
            if (m.order->order_id == 0 || m.order->order_id >= ids.next_order_id) {
                return where + ".nav.order.order_id: not below ids.next_order_id";
            }
            if (!orders.insert(m.order->order_id).second) {
                return where + ".nav.order.order_id: duplicated across ships";
            }
            // 12.2: an unrouted order waits for its planning slot and holds the ticket; a routed
            // one has consumed it and carries the plan.
            if (!m.order->routed && (!m.ticket.has_value() || !m.nodes.empty())) {
                return where + ".nav: an unrouted order needs its ticket and has no nodes";
            }
            if (m.order->routed && m.ticket.has_value()) {
                return where + ".nav.ticket: a routed order has consumed its ticket";
            }
        } else if (m.ticket.has_value() || !m.nodes.empty()) {
            return where + ".nav: a ticket or pending nodes without an order";
        }
        if (m.ticket.has_value()) {
            if (*m.ticket == 0 || *m.ticket >= ids.next_ticket) {
                return where + ".nav.ticket: not below ids.next_ticket";
            }
            if (!tickets.insert(*m.ticket).second) {
                return where + ".nav.ticket: duplicated across ships";
            }
        }
        for (int axis = 0; axis < 2; ++axis) {
            if (m.res24[axis] >= (1U << 24)) {
                return where + ".nav.res24: not below 2^24";
            }
            const bool diagonal_moving =
                g.kind == NavSegKind::Move && (g.dir8 & 1U) != 0 && g.remaining_x500 > 0;
            if (m.res24[axis] != 0 && !diagonal_moving) {
                return where + ".nav.res24: non-zero outside a diagonal movement segment";
            }
            const std::int64_t bound = std::int64_t{1} << 61;
            if (m.pos_u[axis] <= -bound || m.pos_u[axis] >= bound) {
                return where + ".nav.pos_u: outside +-2^61";
            }
            const std::int64_t published = wide::floor_div(m.pos_u[axis], kNavPosUnitsPerQ8);
            const std::int32_t have = axis == 0 ? f.pos.x : f.pos.y;
            if (published != have) {
                return where + ".nav: Figure::pos does not equal floor(pos_u) on axis " +
                       std::to_string(axis);
            }
        }
    }
    return {};
}

} // namespace anno::sim
