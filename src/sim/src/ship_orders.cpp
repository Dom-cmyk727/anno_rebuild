#include <anno/sim/ship_orders.hpp>
#include <anno/sim/ship_step.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

ApplyResult refuse(Reject r) {
    ApplyResult out;
    out.status = r;
    return out;
}

/// The checks both commands share, in the order of the contract table (12.1): 1 the figure exists,
/// 2 the issuer owns it, 3 it navigates, 4 its state allows an order. Check 5 (the target, MoveCmd
/// only) and 6 (not a crew child) follow in the callers.
Reject check_ship(const World& w, const TypeRegistry& types, const Rules& rules, PlayerId issuer,
                  FigureId id, const Figure*& out) {
    const Figure* ship = find_figure(w.figures(), id);
    if (ship == nullptr) {
        return Reject::UnknownFigure;
    }
    if (ship->owner != issuer) {
        return Reject::NotShipOwner;
    }
    const FigureType* type = types.find_figure_type(ship->kind);
    if (type == nullptr || !type->nav.has_value() || !ship->nav.has_value()) {
        return Reject::NotAShip;
    }
    if ((ship->state_word & rules.nav.move_blocked_mask) != 0) {
        return Reject::ShipDisabled;
    }
    out = ship;
    return Reject::None;
}

} // namespace

ApplyResult ShipOrders::move(World& w, const Command& command, const MoveCmd& cmd,
                             const InAdvance*) {
    const Figure* checked = nullptr;
    if (const Reject r = check_ship(w, *w.types_, w.rules_, command.issuer, cmd.ship, checked);
        r != Reject::None) {
        return refuse(r);
    }
    // 5. The target is inside the world: 0 <= x < width tiles, the same for y (Q24.8).
    const std::int64_t width_q8 = std::int64_t{w.settings_.world_width} * 256;
    const std::int64_t height_q8 = std::int64_t{w.settings_.world_height} * 256;
    if (cmd.target_x_q8 < 0 || cmd.target_y_q8 < 0 || cmd.target_x_q8 >= width_q8 ||
        cmd.target_y_q8 >= height_q8) {
        return refuse(Reject::BadValue);
    }
    if (checked->parent.valid()) {
        return refuse(Reject::ShipEmbarked); // 6
    }
    // The counters are spent only after every check; a counter at its maximum is refused.
    if (w.ids_.next_order_id == ~std::uint64_t{0} ||
        (!checked->nav->ticket.has_value() && w.ids_.next_ticket == ~std::uint64_t{0})) {
        return refuse(Reject::SerialExhausted);
    }
    Figure& ship = *find_figure(w.figures_, cmd.ship);
    nav_stop_motion(ship); // the motion part of the stop (12.1)
    NavMotion& m = *ship.nav;
    m.order = NavOrder{w.ids_.next_order_id++, false, cmd.target_x_q8, cmd.target_y_q8};
    if (!m.ticket.has_value()) {
        m.ticket = w.ids_.next_ticket++; // a replacement keeps its ticket and so its place (12.2)
        note_ticket(w.figures_, std::nullopt, m.ticket);
    }
    return {};
}

ApplyResult ShipOrders::stop(World& w, const Command& command, const StopCmd& cmd,
                             const InAdvance* in_advance) {
    const Figure* checked = nullptr;
    if (const Reject r = check_ship(w, *w.types_, w.rules_, command.issuer, cmd.ship, checked);
        r != Reject::None) {
        return refuse(r);
    }
    if (checked->parent.valid()) {
        return refuse(Reject::ShipEmbarked); // 6
    }
    Figure& ship = *find_figure(w.figures_, cmd.ship);
    nav_stop_motion(ship);
    NavMotion& m = *ship.nav;
    if (m.order.has_value()) {
        SimEvent e;
        e.kind = SimEventKind::ShipStopped;
        e.tick = w.clock_.tick();
        e.player = ship.owner;
        e.figure = ship.id;
        e.order_id = m.order->order_id;
        w.host_events_.push(e);
        if (in_advance != nullptr) {
            w.out_.events.push_back(e);
        }
    }
    m.order.reset();
    note_ticket(w.figures_, m.ticket, std::nullopt);
    m.ticket.reset();
    return {};
}

} // namespace anno::sim
