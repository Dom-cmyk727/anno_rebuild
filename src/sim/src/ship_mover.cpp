#include <algorithm>

#include <anno/sim/route.hpp>
#include <anno/sim/route_cache.hpp>
#include <anno/sim/route_stamps.hpp>
#include <anno/sim/ship_mover.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

constexpr std::array<int, 8> kDirX = {-1, -1, 0, 1, 1, 1, 0, -1};
constexpr std::array<int, 8> kDirZ = {0, 1, 1, 1, 0, -1, -1, -1};

RouteLimits limits_of(const NavRules& n) {
    RouteLimits l;
    l.window_radius_tiles = static_cast<std::int32_t>(n.window_radius_tiles);
    l.max_window_blocks = n.window_blocks;
    l.goal_radius_tiles = static_cast<std::int32_t>(n.goal_radius_tiles);
    l.run_cap = n.run_cap;
    l.retry_cap = n.retry_cap;
    l.unit_cap = n.unit_cap;
    l.coarse_budget = n.coarse_budget;
    l.fine_budget = n.fine_budget;
    l.memory_bytes = n.memory_bytes;
    return l;
}

RouteCostTable costs_of(const NavRules& n) {
    RouteCostTable t;
    for (std::size_t i = 0; i < n.class_costs.size(); ++i) {
        const NavClassCost& c = n.class_costs[i];
        RouteClassCosts k;
        k.open_orth = c.open_orth;
        k.open_diag = c.open_diag;
        k.shore_orth = c.shore_orth;
        k.shore_diag = c.shore_diag;
        k.shore_enterable =
            i != 3; // the original's class 3 has no shore mask (docs/sim/ship-route-engine.md)
        t.classes.push_back(k);
    }
    return t;
}

/// The sailing ships around a point, answered from the registry's ship grid (figure.hpp
/// `ship_cells`): a cell lookup is O(log cells + ships in the cell), independent of the number of
/// unrelated figures. Counts every entry it examines in `work`.
class RegistryShips final : public ShipSpatialView {
  public:
    RegistryShips(const FigureRegistry& reg, NavWork* work) : reg_(reg), work_(work) {}
    void ships_in_cell(std::int32_t cell_x, std::int32_t cell_z,
                       std::vector<LocalShip>& out) const override {
        if (work_ != nullptr) {
            ++work_->stamp_cells;
        }
        const auto it = reg_.ship_cells.find({cell_z, cell_x});
        if (it == reg_.ship_cells.end()) {
            return;
        }
        for (const FigureId id : it->second) {
            if (work_ != nullptr) {
                ++work_->stamp_entries;
            }
            const Figure* f = find_figure(reg_, id);
            if (f == nullptr || !f->nav.has_value() || f->parent.valid()) {
                continue;
            }
            LocalShip s;
            s.id = f->id.v;
            s.x_q8 = f->pos.x;
            s.z_q8 = f->pos.y;
            s.state_word = f->state_word;
            s.ship_group = true;
            s.has_direction = true;
            const auto v = ShipMover::stamp_velocity_q32(f->nav->seg);
            s.vel_x_q32 = v[0];
            s.vel_z_q32 = v[1];
            out.push_back(s);
        }
    }

  private:
    const FigureRegistry& reg_;
    NavWork* work_;
};

/// The route nodes of the plan (12.7): turn nodes before the first run only, then one movement node
/// per run. A diagonal run is `steps * L_d` long, an axis run `steps` units.
std::vector<NavNode> nav_nodes_of_runs_impl(const std::vector<RouteRun>& runs, unsigned facing,
                                            const NavFacts& facts) {
    std::vector<NavNode> nodes;
    const int want = 2 * runs.front().dir;
    int diff = (want - static_cast<int>(facing)) & 15;
    if (diff > 8) {
        diff -= 16;
    }
    if (std::abs(diff) > 2) {
        const int step = diff > 0 ? 1 : -1;
        int f = static_cast<int>(facing);
        for (int i = 0; i < std::abs(diff); ++i) {
            NavNode t;
            t.kind = NavSegKind::Turn;
            t.facing = static_cast<std::uint8_t>(f & 15);
            t.length_q32 = facts.turn_step_q32;
            t.speed_q32 = kNavIdleSpeedQ32;
            nodes.push_back(t);
            f += step;
        }
    }
    for (const RouteRun& r : runs) {
        NavNode n;
        n.kind = NavSegKind::Move;
        n.dir8 = r.dir;
        n.length_q32 = std::uint64_t{r.steps} * ((r.dir & 1U) != 0 ? kNavDiagStepQ32 : kNavUnitQ32);
        n.speed_q32 = facts.speed_q32;
        nodes.push_back(n);
    }
    return nodes;
}

class WorldPlanner final : public NavPlanner {
  public:
    WorldPlanner(const World& w, RouteCache& cache, const NavRules& rules, const NavFacts& facts,
                 std::uint32_t& started)
        : w_(w), cache_(cache), rules_(rules), facts_(facts), started_(started) {}

    NavPlan plan(const Figure& ship) override {
        const NavRules& rules = rules_;
        // Fair admission (12.2 rule 2): admitted iff the pending tickets older than ours are fewer
        // than the quota left in this Advance.
        const std::uint64_t mine = ship.nav->ticket.value_or(0);
        const std::uint64_t left =
            rules.plans_per_advance > started_ ? rules.plans_per_advance - started_ : 0;
        // The tickets older than ours are the smallest members of the ticket set: it is enough to
        // look at the first `left` of them (bounded by the quota, not by the registry).
        std::uint64_t older = 0;
        for (const std::uint64_t t : w_.figures().tickets) {
            ++cache_.work.admission_steps;
            if (t >= mine || ++older >= left) {
                break;
            }
        }
        if (older >= left) {
            return NavPlan{NavPlanKind::Deferred, {}, {}};
        }
        ++started_;

        const CoastIndex& index = cache_.get(w_);
        const RouteLimits limits = limits_of(rules);
        RouteQuery q;
        q.start_x = ship.pos.x >> 8;
        q.start_z = ship.pos.y >> 8;
        q.goal_x_q8 = ship.nav->order->target_x_q8;
        q.goal_z_q8 = ship.nav->order->target_y_q8;
        q.nav_class = facts_.nav_class;
        q.stamps = ShipMover::local_stamps(w_, ship, &cache_.work);
        const RouteResult r = plan_route(index, q, limits, costs_of(rules));
        NavPlan out;
        switch (r.status) {
        case RouteStatus::Planned:
            if (r.runs.empty() || r.runs.size() + 8 > kNavMaxNodes) {
                out.kind = NavPlanKind::Failed;
                out.reason = NavFailReason::BudgetExceeded;
                return out;
            }
            out.kind = NavPlanKind::Nodes;
            out.nodes = nav_nodes_of_runs(r.runs, ship.dir16, facts_);
            return out;
        case RouteStatus::Arrived:
            out.kind = NavPlanKind::Arrived;
            return out;
        case RouteStatus::BudgetExceeded:
            out.kind = NavPlanKind::Failed;
            out.reason = NavFailReason::BudgetExceeded;
            return out;
        case RouteStatus::OutsideWindow:
            out.kind = NavPlanKind::Failed;
            out.reason = NavFailReason::OutsideWindow;
            return out;
        case RouteStatus::Unreachable:
        case RouteStatus::InvalidQuery:
            break;
        }
        out.kind = NavPlanKind::Failed;
        out.reason = NavFailReason::Unreachable;
        return out;
    }

  private:
    const World& w_;
    RouteCache& cache_;
    const NavRules& rules_;
    const NavFacts& facts_;
    std::uint32_t& started_;
};

SimEvent event_of(const NavEvent& e, const Figure& ship, std::uint64_t tick) {
    SimEvent s;
    s.kind =
        e.kind == NavEventKind::Arrived ? SimEventKind::ShipArrived : SimEventKind::ShipRouteFailed;
    s.tick = tick;
    s.player = ship.owner;
    s.figure = ship.id;
    s.code = static_cast<std::uint32_t>(e.reason);
    s.order_id = e.order_id;
    return s;
}

} // namespace

std::array<std::int32_t, 2> ShipMover::stamp_velocity_q32(const NavSegment& seg) {
    if (seg.kind != NavSegKind::Move || seg.dir8 >= 8) {
        return {0, 0};
    }
    // unit (2^-24) * speed (2^-32 tile per stepper unit) * factor / 100 / 2^24: the product is
    // < 2^24 * 2^28 * 100 < 2^59, a truncation toward zero loses < 2^-32 tile (x 25 in the stamp).
    const std::int64_t unit = (seg.dir8 & 1U) != 0 ? 11863283 : (std::int64_t{1} << 24);
    const std::int64_t mag = unit * static_cast<std::int64_t>(seg.speed_q32) *
                             static_cast<std::int64_t>(seg.factor_percent) / (100 * 16777216);
    return {static_cast<std::int32_t>(kDirX[seg.dir8] * mag),
            static_cast<std::int32_t>(kDirZ[seg.dir8] * mag)};
}

std::vector<RouteStamp> ShipMover::local_stamps(const World& w, const Figure& ship, NavWork* work) {
    const FigureType* type = w.types_->find_figure_type(ship.kind);
    const NavRules& rules = w.rules_.nav;
    LocalStampQuery sq;
    sq.x_q8 = ship.pos.x;
    sq.z_q8 = ship.pos.y;
    sq.self_id = ship.id.v;
    sq.radius_q8 = static_cast<std::int32_t>(
        type != nullptr && type->nav.has_value() ? type->nav->stamp_radius_88 : 1280U);
    sq.world_w = static_cast<std::int32_t>(w.settings().world_width);
    sq.world_h = static_cast<std::int32_t>(w.settings().world_height);
    return local_ship_stamps(RegistryShips(w.figures(), work), sq, limits_of(rules));
}

std::vector<NavNode> nav_nodes_of_runs(const std::vector<RouteRun>& runs, unsigned facing,
                                       const NavFacts& facts) {
    return nav_nodes_of_runs_impl(runs, facing, facts);
}

void ShipMover::visit(World& w, const TickContext& ctx, Figure& ship, Effects& fx,
                      std::uint32_t& plans_started) {
    const FigureType* type = w.types_->find_figure_type(ship.kind);
    if (type == nullptr || !type->nav.has_value() || !ship.nav.has_value()) {
        return;
    }
    const NavFacts& facts = *type->nav;
    NavMotion& m = *ship.nav;
    const std::optional<std::uint64_t> ticket_before = m.ticket;
    std::vector<NavEvent> events;
    // A ship that entered a blocked state drops its order and ticket (12.2 rule 1).
    if (m.order.has_value() && (ship.state_word & w.rules_.nav.move_blocked_mask) != 0) {
        events.push_back(
            NavEvent{NavEventKind::RouteFailed, m.order->order_id, NavFailReason::ShipBlocked});
        m.order.reset();
        m.ticket.reset();
        nav_stop_motion(ship);
    }
    const Player* owner = w.players_.find(ship.owner);
    NavStepEnv env;
    env.facts = &facts;
    env.factor_percent = nav_speed_factor(facts, w.rules_.nav, type->capacity_88, ship.holds,
                                          owner != nullptr ? owner->volk : 1);
    // The derived coast index follows every object change since the last visit (a command or
    // island growth), so a plan reads the authoritative state and not the history of a cache.
    w.route_cache_.sync(w);
    WorldPlanner planner(w, w.route_cache_, w.rules_.nav, facts, plans_started);
    const FixedPos before = ship.pos;
    nav_step(ship, env, planner, static_cast<std::uint16_t>(ctx.t), events);
    for (const NavEvent& e : events) {
        fx.ui.push_back(event_of(e, ship, ctx.tick));
    }
    note_ticket(w.figures_, ticket_before, m.ticket);
    if (ship.pos.x != before.x || ship.pos.y != before.y) {
        note_ship_moved(w.figures_, ship, before);
        // The crew is written from the carrier: every descendant of this ship (scouts aboard, the
        // donkey of a scout, any deeper chain) takes its position through the effects buffer. The
        // walk is iterative over the registry's child index, so its work is the size of the
        // carried subtree, not of the registry.
        std::vector<FigureId> stack;
        const auto push_children = [&](FigureId parent) {
            const auto it = w.figures_.children.find(parent);
            if (it != w.figures_.children.end()) {
                for (auto c = it->second.rbegin(); c != it->second.rend(); ++c) {
                    stack.push_back(*c);
                }
            }
        };
        push_children(ship.id);
        while (!stack.empty()) {
            const FigureId id = stack.back();
            stack.pop_back();
            ++w.route_cache_.work.crew_visits;
            fx.figure_moves.push_back(FigureMove{id, ship.pos});
            push_children(id);
        }
    }
}

} // namespace anno::sim
