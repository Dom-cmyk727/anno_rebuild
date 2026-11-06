#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>

#include <anno/sim/figure_veto.hpp>
#include <anno/sim/freight.hpp>
#include <anno/sim/ship_cargo.hpp>
#include <anno/sim/ship_orders.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {
namespace {
ApplyResult refuse(Reject r) {
    ApplyResult out;
    out.status = r;
    return out;
}
Reject check_ship(const World& w, PlayerId owner, FigureId id, const Figure*& ship, bool cleanup) {
    ship = find_figure(w.figures(), id);
    if (ship == nullptr)
        return Reject::UnknownFigure;
    if (ship->owner != owner)
        return Reject::NotShipOwner;
    const auto* type = w.types().find_figure_type(ship->kind);
    if (type == nullptr || type->bewohntype != veto::Rules{}.ship_kind || !type->nav || !ship->nav)
        return Reject::NotAShip;
    if (!cleanup && (ship->state_word & w.rules().nav.move_blocked_mask) != 0)
        return Reject::ShipDisabled;
    if (ship->parent.valid())
        return Reject::ShipEmbarked;
    if (!cleanup && ship->expiry_stamp)
        return Reject::BadValue;
    return Reject::None;
}
bool dock_valid(const World& w, const FreightStop& s) {
    return s.dock_x_q8 >= 0 && s.dock_y_q8 >= 0 &&
           s.dock_x_q8 < std::int64_t{w.settings().world_width} * 256 &&
           s.dock_y_q8 < std::int64_t{w.settings().world_height} * 256;
}
Reject definition(const World& w, const std::vector<FreightStop>& stops, bool canonical) {
    if (stops.empty() || stops.size() > w.rules().freight.max_stops)
        return Reject::BadValue;
    std::uint64_t count = 0;
    for (const auto& stop : stops) {
        count += stop.actions.size();
        if (count > w.rules().freight.max_actions || !dock_valid(w, stop) ||
            !stop.warehouse.valid() || serial_of(stop.warehouse) == 0 ||
            island_of(stop.warehouse).v >= w.config().island_slots)
            return Reject::BadValue;
        if (canonical) {
            const auto island = island_of(stop.warehouse);
            if (island.v >= w.islands().size() || !w.islands()[island.v] ||
                serial_of(stop.warehouse) >= w.islands()[island.v]->next_serial)
                return Reject::BadValue;
        }
        std::map<std::pair<FreightDirection, GoodsId>, std::uint64_t> amounts;
        for (const auto& action : stop.actions) {
            if (static_cast<unsigned>(action.direction) > 1 || action.tons > UINT64_MAX / 256)
                return Reject::BadValue;
            if (action.goods == 0 || action.goods >= w.config().warehouse_goods ||
                w.types().goods(action.goods) == nullptr)
                return Reject::UnsupportedGoods;
            const auto key = std::pair{action.direction, action.goods};
            const auto found = amounts.find(key);
            if (found != amounts.end()) {
                if (canonical && action.tons != 0)
                    return Reject::BadValue;
                if (action.tons > UINT64_MAX / 256 - found->second)
                    return Reject::BadValue;
                found->second += action.tons;
            } else
                amounts.emplace(key, action.tons);
        }
    }
    if (12 + stops.size() * 20 + count * 13 > kMaxRecordPayload)
        return Reject::Unencodable;
    return Reject::None;
}
void normalize(std::vector<FreightStop>& stops) {
    for (auto& stop : stops) {
        std::map<std::pair<FreightDirection, GoodsId>, std::size_t> first;
        for (std::size_t i = 0; i < stop.actions.size(); ++i) {
            auto& action = stop.actions[i];
            const auto [at, added] = first.emplace(std::pair{action.direction, action.goods}, i);
            if (!added) {
                stop.actions[at->second].tons += action.tons;
                action.tons = 0;
            }
        }
    }
}
bool stop_clock(FreightPhase p) {
    return p >= FreightPhase::Wait;
}
} // namespace

Reject FreightRoutes::target(const World& w, const Figure& ship, const FreightStop& stop,
                             bool range) {
    const auto result = ShipCargo::destination(w, ship, stop.warehouse, range);
    if (result.status != Reject::None)
        return result.status;
    const auto* city = w.islands()[result.city.island.v]->city(result.city.city);
    for (const auto& action : stop.actions)
        if (action.goods >= city->stock.stock.size())
            return Reject::UnsupportedGoods;
    return Reject::None;
}
ApplyResult FreightRoutes::assign(World& w, const Command& command,
                                  const AssignFreightRouteCmd& cmd, const InAdvance* ctx) {
    const Figure* checked = nullptr;
    if (const auto r = check_ship(w, command.issuer, cmd.ship, checked, false); r != Reject::None)
        return refuse(r);
    if (const auto r = definition(w, cmd.stops, false); r != Reject::None)
        return refuse(r);
    for (const auto& stop : cmd.stops)
        if (const auto r = target(w, *checked, stop, false); r != Reject::None)
            return refuse(r);
    if (checked->freight_generation == UINT64_MAX)
        return refuse(Reject::SerialExhausted);
    FreightState next;
    next.stops = cmd.stops;
    normalize(next.stops);
    next.last_tick = w.clock_.tick();
    // All validation and allocation precede the first authoritative write.
    FreightSlot slot;
    slot.emplace() = std::move(next);
    const auto stopped = ShipOrders::stop_route(w, command, StopCmd{cmd.ship}, ctx);
    if (!stopped.ok())
        return stopped;
    auto& ship = *find_figure(w.figures_, cmd.ship);
    ++ship.freight_generation;
    ship.freight = std::move(slot);
    return {};
}
ApplyResult FreightRoutes::control(World& w, const Command& command,
                                   const ControlFreightRouteCmd& cmd, const InAdvance* ctx) {
    if (static_cast<unsigned>(cmd.control) > 2)
        return refuse(Reject::BadValue);
    const Figure* checked = nullptr;
    // A running Resume is still an idempotent clock no-op, but must be a live
    // eligible ship. Cleanup remains usable on blocked/ended ships.
    if (const auto r =
            check_ship(w, command.issuer, cmd.ship, checked, cmd.control != FreightControl::Resume);
        r != Reject::None)
        return refuse(r);
    if (!checked->freight.has_value())
        return cmd.control == FreightControl::Cancel ? ApplyResult{} : refuse(Reject::BadValue);
    const auto& old = *checked->freight;
    if (cmd.control == FreightControl::Resume) {
        if (old.status == FreightStatus::Running)
            return {};
        if (const auto r = target(w, *checked, old.stops[old.stop_index], false); r != Reject::None)
            return refuse(r);
        auto& route = *find_figure(w.figures_, cmd.ship)->freight;
        route.status = FreightStatus::Running;
        route.last_tick = w.clock_.tick();
        route.last_reject = Reject::None;
        route.nav_failure = 0;
        return {};
    }
    if (cmd.control == FreightControl::Suspend && old.status != FreightStatus::Running)
        return {};
    std::uint64_t credit = old.credit_ticks;
    if (cmd.control == FreightControl::Suspend && stop_clock(old.phase)) {
        const auto now = w.clock_.tick();
        if (now < old.last_tick || now - old.last_tick > UINT64_MAX - credit)
            return refuse(Reject::BadValue);
        credit += now - old.last_tick;
    }
    const auto stopped = ShipOrders::stop_route(w, command, StopCmd{cmd.ship}, ctx);
    if (!stopped.ok())
        return stopped;
    auto& ship = *find_figure(w.figures_, cmd.ship);
    if (cmd.control == FreightControl::Cancel) {
        ship.freight.reset();
        return {};
    }
    auto& route = *ship.freight;
    route.status = FreightStatus::Suspended;
    route.credit_ticks = credit;
    route.last_tick = w.clock_.tick();
    if (route.phase == FreightPhase::Sailing)
        route.phase = FreightPhase::NeedLeg;
    route.expected_order_id = 0;
    route.order_generation = 0;
    return {};
}
void FreightRoutes::manual_takeover(World& w, FigureId id) {
    if (auto* f = find_figure(w.figures_, id))
        f->freight.reset();
}

std::string FreightRoutes::validate(const World& w) {
    for (const auto& slot : w.figures().list.slots) {
        if (!slot)
            continue;
        const auto& f = *slot;
        if (!f.freight.has_value())
            continue;
        const auto bad = [&] {
            return "figure " + std::to_string(f.id.v) + ".freight: invalid continuation";
        };
        const auto& r = *f.freight;
        const auto* type = w.types().find_figure_type(f.kind);
        if (type == nullptr || type->bewohntype != veto::Rules{}.ship_kind || !type->nav ||
            !f.nav || f.parent.valid() || f.expiry_stamp || f.freight_generation == 0 ||
            definition(w, r.stops, true) != Reject::None || static_cast<unsigned>(r.status) > 2 ||
            static_cast<unsigned>(r.phase) > 5 || r.stop_index >= r.stops.size() ||
            r.last_tick > w.clock().tick() || r.credit_ticks > r.last_tick || r.nav_failure > 3 ||
            static_cast<unsigned>(r.last_reject) > static_cast<unsigned>(Reject::CityAccounting))
            return bad();
        const auto& stop = r.stops[r.stop_index];
        const auto actions = stop.actions.size();
        const auto limit = std::min<std::uint64_t>(f.holds.size(), type->nav->hold_count_a);
        if (r.action_index > actions || r.scan_next > limit ||
            r.wait_remaining > type->load_ticks ||
            r.candidate_next > r.matching_holds.size() + r.empty_holds.size())
            return bad();
        if (r.phase == FreightPhase::Sailing) {
            if (r.status != FreightStatus::Running || !f.nav->order ||
                r.order_generation != f.freight_generation ||
                r.expected_order_id != f.nav->order->order_id || r.expected_order_id == 0 ||
                f.nav->order->target_x_q8 != stop.dock_x_q8 ||
                f.nav->order->target_y_q8 != stop.dock_y_q8)
                return bad();
        } else if (r.expected_order_id != 0 || r.order_generation != 0)
            return bad();
        if (r.phase != FreightPhase::Sailing && f.nav->order)
            return bad();
        const bool leg = r.phase == FreightPhase::NeedLeg || r.phase == FreightPhase::Sailing;
        if (leg && (r.action_index != 0 || r.credit_ticks != 0))
            return bad();
        if (r.phase == FreightPhase::CompleteStop && r.action_index != actions)
            return bad();
        if (!leg && r.phase != FreightPhase::CompleteStop && r.action_index >= actions)
            return bad();
        const bool cache = r.phase == FreightPhase::Scan || r.phase == FreightPhase::Transfer;
        if (!cache && (r.scan_next || r.matching_raw || r.remaining_raw || r.candidate_next ||
                       !r.matching_holds.empty() || !r.empty_holds.empty()))
            return bad();
        if (r.phase != FreightPhase::Wait && r.wait_remaining != 0)
            return bad();
        if (r.phase == FreightPhase::Scan && (r.remaining_raw || r.candidate_next))
            return bad();
        if (r.phase == FreightPhase::Transfer &&
            (r.scan_next != limit || r.remaining_raw > stop.actions[r.action_index].tons * 256))
            return bad();
        if (cache && stop.actions[r.action_index].direction == FreightDirection::UnloadAmount &&
            !r.empty_holds.empty())
            return bad();
        if (r.phase == FreightPhase::Scan) {
            const auto& action = stop.actions[r.action_index];
            std::uint64_t sum = 0;
            std::vector<std::uint32_t> matching, empty;
            for (std::uint32_t i = 0; i < r.scan_next; ++i) {
                const auto& h = f.holds[i];
                if (h.goods == action.goods) {
                    if (h.amount_88 > UINT64_MAX - sum)
                        return bad();
                    sum += h.amount_88;
                    if (action.direction == FreightDirection::UnloadAmount ||
                        h.amount_88 < type->capacity_88)
                        matching.push_back(i);
                } else if (action.direction == FreightDirection::LoadTarget && h.goods == 0 &&
                           h.amount_88 == 0)
                    empty.push_back(i);
            }
            if (sum != r.matching_raw || matching != r.matching_holds || empty != r.empty_holds)
                return bad();
        }
        std::set<std::uint32_t> seen;
        for (const auto* indices : {&r.matching_holds, &r.empty_holds}) {
            std::optional<std::uint32_t> prior;
            for (const auto i : *indices) {
                if (i >= r.scan_next || i >= limit || (prior && *prior >= i) ||
                    !seen.insert(i).second)
                    return bad();
                prior = i;
            }
        }
        if (r.status != FreightStatus::Failed &&
            (r.last_reject != Reject::None || r.nav_failure != 0))
            return bad();
    }
    return {};
}
} // namespace anno::sim
