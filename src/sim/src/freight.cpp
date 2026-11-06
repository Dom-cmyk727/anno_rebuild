#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>

#include <anno/sim/figure_veto.hpp>
#include <anno/sim/freight.hpp>
#include <anno/sim/ship_cargo.hpp>
#include <anno/sim/ship_mover.hpp>
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
                             bool range, GoodsId goods) {
    const auto result = ShipCargo::destination(w, ship, stop.warehouse, range);
    if (result.status != Reject::None)
        return result.status;
    const auto* city = w.islands()[result.city.island.v]->city(result.city.city);
    if (goods != 0 && goods >= city->stock.stock.size())
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
    observe(w, ship, FreightSignal::Assigned, nullptr, ctx);
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
        const auto& stop = old.stops[old.stop_index];
        const auto goods =
            old.action_index < stop.actions.size() ? stop.actions[old.action_index].goods : 0;
        if (const auto r = target(w, *checked, stop, false, goods); r != Reject::None)
            return refuse(r);
        auto& route = *find_figure(w.figures_, cmd.ship)->freight;
        route.status = FreightStatus::Running;
        route.last_tick = w.clock_.tick();
        route.last_reject = Reject::None;
        route.nav_failure = 0;
        observe(w, *find_figure(w.figures_, cmd.ship), FreightSignal::Resumed, nullptr, ctx);
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
        observe(w, ship, FreightSignal::Cancelled, nullptr, ctx);
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
    observe(w, ship, FreightSignal::Paused, nullptr, ctx);
    return {};
}
void FreightRoutes::manual_takeover(World& w, FigureId id, const InAdvance* ctx) {
    if (auto* f = find_figure(w.figures_, id); f != nullptr && f->freight.has_value()) {
        observe(w, *f, FreightSignal::ManualTakeover, nullptr, ctx);
        f->freight.reset();
    }
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

namespace anno::sim {
namespace {
void heap_swap(FreightHoldIndex& index, std::size_t a, std::size_t b) {
    std::swap(index.rooms[a], index.rooms[b]);
    index.positions[index.rooms[a].hold] = static_cast<std::uint32_t>(a);
    index.positions[index.rooms[b].hold] = static_cast<std::uint32_t>(b);
}
void heap_up(FreightHoldIndex& index, std::size_t at, FreightWork* work) {
    while (at != 0) {
        const auto parent = (at - 1) / 2;
        if (work)
            ++work->heap_steps;
        if (!(index.rooms[at] < index.rooms[parent]))
            break;
        heap_swap(index, at, parent);
        at = parent;
    }
}
void heap_down(FreightHoldIndex& index, std::size_t at, FreightWork* work) {
    for (;;) {
        const auto left = at * 2 + 1;
        if (left >= index.rooms.size())
            return;
        auto child = left;
        if (left + 1 < index.rooms.size()) {
            if (work)
                ++work->heap_steps;
            if (index.rooms[left + 1] < index.rooms[left])
                child = left + 1;
        }
        if (work)
            ++work->heap_steps;
        if (!(index.rooms[child] < index.rooms[at]))
            return;
        heap_swap(index, at, child);
        at = child;
    }
}
void heap_insert(FreightHoldIndex& index, std::uint32_t i, const ShipHold& hold, GoodsId goods,
                 std::uint32_t capacity, FreightWork* work) {
    if (hold.goods != goods || hold.amount_88 >= capacity)
        return;
    const auto at = index.rooms.size();
    index.positions[i] = static_cast<std::uint32_t>(at);
    index.rooms.push_back({UINT64_MAX - (std::uint64_t{capacity} - hold.amount_88), i});
    heap_up(index, at, work);
}
void heap_erase(FreightHoldIndex& index, std::uint32_t i, FreightWork* work) {
    const auto at = index.positions[i];
    if (at == kNoSlot)
        return;
    index.positions[i] = kNoSlot;
    if (std::size_t{at} + 1 == index.rooms.size()) {
        index.rooms.pop_back();
        return;
    }
    index.rooms[at] = index.rooms.back();
    index.rooms.pop_back();
    index.positions[index.rooms[at].hold] = at;
    if (at != 0 && index.rooms[at] < index.rooms[(at - 1) / 2])
        heap_up(index, at, work);
    else
        heap_down(index, at, work);
}
void index_scan(FreightHoldIndex& index, std::uint32_t i, const ShipHold& hold, GoodsId goods,
                std::uint32_t capacity, FreightWork* work) {
    index.positions.push_back(kNoSlot); // one initialized position per saved scan unit
    heap_insert(index, i, hold, goods, capacity, work);
    if (hold.goods == 0 && hold.amount_88 == 0)
        index.empty.push_back(i);
}
void index_start(Figure& ship, std::size_t holds, bool load) {
    auto& index = ship.freight.index();
    index.reset();
    index.generation = ship.freight_generation;
    index.stop = ship.freight->stop_index;
    index.action = ship.freight->action_index;
    if (load) {
        index.rooms.reserve(holds);
        index.positions.reserve(holds);
        index.empty.reserve(holds);
    }
    // Logical sizes are zero: reserve allocates raw trivial buffers, without a
    // hidden H-element initialization/copy. Subsequent pushes occur under Scan.
    ship.freight->matching_holds.reserve(holds);
    ship.freight->empty_holds.reserve(holds);
}
bool index_current(const Figure& ship, const FreightHoldIndex& index) {
    const auto& r = *ship.freight;
    return index.generation == ship.freight_generation && index.stop == r.stop_index &&
           index.action == r.action_index && index.scanned == r.scan_next;
}
std::optional<std::uint32_t> indexed_hold(const Figure& ship, std::uint32_t preferred,
                                          GoodsId goods, std::uint32_t capacity,
                                          const FreightHoldIndex& index) {
    const auto& hold = ship.holds[preferred];
    if (hold.goods == goods && hold.amount_88 <= capacity && capacity - hold.amount_88 >= 256)
        return preferred;
    if (!index.rooms.empty())
        return index.rooms.front().hold;
    if (index.empty_next < index.empty.size())
        return index.empty[index.empty_next];
    return std::nullopt;
}
bool resource_refusal(Reject r) {
    return r == Reject::StockShort || r == Reject::StockFull || r == Reject::HoldsFull ||
           r == Reject::HoldGoodsMismatch;
}
void clear_action(Figure& ship) {
    auto& r = *ship.freight;
    r.scan_next = 0;
    r.matching_raw = 0;
    r.remaining_raw = 0;
    r.candidate_next = 0;
    r.matching_holds.clear();
    r.empty_holds.clear();
    ship.freight.index().reset();
}
void next_action(Figure& ship, std::uint32_t wait) {
    auto& r = *ship.freight;
    clear_action(ship);
    ++r.action_index;
    if (r.action_index == r.stops[r.stop_index].actions.size())
        r.phase = FreightPhase::CompleteStop;
    else {
        r.phase = FreightPhase::Wait;
        r.wait_remaining = wait;
    }
}
} // namespace
void FreightRoutes::observe(World& w, const Figure& ship, FreightSignal signal, Effects* fx,
                            const InAdvance* ctx, Reject reason, std::uint64_t amount,
                            std::uint64_t order) {
    if (!ship.freight.has_value())
        return;
    const auto& r = *ship.freight;
    const auto& stop = r.stops[r.stop_index];
    FreightObservation o;
    o.signal = signal;
    o.generation = ship.freight_generation;
    o.stop = r.stop_index;
    o.action = r.action_index;
    o.warehouse = stop.warehouse;
    o.status = r.status;
    o.phase = r.phase;
    o.amount_88 = amount;
    o.completed_cycles = r.completed_cycles;
    o.reason = reason;
    o.nav_failure = r.nav_failure;
    if (r.action_index < stop.actions.size()) {
        o.goods = stop.actions[r.action_index].goods;
        o.direction = stop.actions[r.action_index].direction;
    }
    SimEvent event;
    event.kind = SimEventKind::Freight;
    event.tick = w.clock_.tick();
    event.player = ship.owner;
    event.figure = ship.id;
    event.building = stop.warehouse;
    event.order_id = order ? order : r.expected_order_id;
    event.code = static_cast<std::uint32_t>(signal);
    event.freight = o;
    if (fx)
        fx->ui.push_back(std::move(event));
    else {
        w.host_events_.push(event);
        if (ctx)
            w.out_.events.push_back(std::move(event));
    }
}
void FreightRoutes::rebuild_indexes(World& w) {
    for (auto& slot : w.figures_.list.slots) {
        if (!slot || !slot->freight.has_value())
            continue;
        auto& ship = *slot;
        const auto& r = *ship.freight;
        if (r.phase != FreightPhase::Scan && r.phase != FreightPhase::Transfer)
            continue;
        const auto* type = w.types_->find_figure_type(ship.kind);
        const auto full = static_cast<std::size_t>(
            std::min<std::uint64_t>(ship.holds.size(), type->nav->hold_count_a));
        const auto& action = r.stops[r.stop_index].actions[r.action_index];
        const bool load = action.direction == FreightDirection::LoadTarget;
        index_start(ship, full, load);
        auto& index = ship.freight.index();
        const auto limit =
            r.phase == FreightPhase::Scan ? r.scan_next : static_cast<std::uint32_t>(full);
        if (load)
            for (std::uint32_t i = 0; i < limit; ++i) {
                index_scan(index, i, ship.holds[i], action.goods, type->capacity_88, nullptr);
                ++w.freight_work_.cold_scans;
            }
        index.scanned = limit;
    }
}
void FreightRoutes::visit(World& w, const TickContext& ctx, Figure& ship, Effects& fx,
                          std::uint32_t& plans_started) {
    auto& work = w.freight_work_;
    ++work.visits;
    const auto begin = fx.ui.size();
    // Always reuse the accepted mover, including idle stamps/crew merging. A
    // newly issued leg follows this visit and cannot retroactively sail elapsed time.
    ShipMover::visit(w, ctx, ship, fx, plans_started);
    if (!ship.freight.has_value())
        return;
    auto& r = *ship.freight;
    if (r.status != FreightStatus::Running)
        return;
    const auto* type = w.types_->find_figure_type(ship.kind);
    const auto now = ctx.tick;
    const auto fail = [&](Reject reason, std::uint8_t nav = 0) {
        const InAdvance in{&ctx, nullptr};
        (void)ShipOrders::stop_route(w, {ship.owner, StopCmd{ship.id}}, StopCmd{ship.id}, &in);
        if (r.phase == FreightPhase::Sailing || r.phase == FreightPhase::NeedLeg) {
            r.phase = FreightPhase::NeedLeg;
            r.expected_order_id = 0;
            r.order_generation = 0;
            r.credit_ticks = 0;
        }
        r.status = FreightStatus::Failed;
        r.last_reject = reason;
        r.nav_failure = nav;
        observe(w, ship, FreightSignal::Failed, &fx, nullptr, reason);
    };
    if (r.phase == FreightPhase::Sailing) {
        // Consume only this mover call's authenticated order events, never the
        // host queue or an externally published notification.
        std::optional<SimEvent> completed;
        for (std::size_t i = begin; i < fx.ui.size(); ++i) {
            const auto& e = fx.ui[i];
            if (e.figure == ship.id && e.order_id == r.expected_order_id &&
                r.order_generation == ship.freight_generation &&
                (e.kind == SimEventKind::ShipArrived || e.kind == SimEventKind::ShipRouteFailed)) {
                completed = e;
                break;
            }
        }
        r.last_tick = now;
        if (!completed)
            return;
        if (completed->kind == SimEventKind::ShipRouteFailed) {
            fail(Reject::BadValue, static_cast<std::uint8_t>(completed->code));
            return;
        }
        const auto order = r.expected_order_id;
        r.expected_order_id = 0;
        r.order_generation = 0;
        r.credit_ticks = 0;
        r.phase =
            r.stops[r.stop_index].actions.empty() ? FreightPhase::CompleteStop : FreightPhase::Wait;
        r.wait_remaining = r.phase == FreightPhase::Wait ? type->load_ticks : 0;
        observe(w, ship, FreightSignal::Arrived, &fx, nullptr, Reject::None, 0, order);
    } else if (stop_clock(r.phase)) {
        if (now < r.last_tick || now - r.last_tick > UINT64_MAX - r.credit_ticks) {
            fail(Reject::BadValue);
            return;
        }
        r.credit_ticks += now - r.last_tick;
        r.last_tick = now;
    }
    if ((ship.state_word & w.rules_.nav.move_blocked_mask) != 0) {
        fail(Reject::ShipDisabled);
        return;
    }
    if (r.phase != FreightPhase::NeedLeg) {
        const auto& live_stop = r.stops[r.stop_index];
        const auto goods =
            r.action_index < live_stop.actions.size() ? live_stop.actions[r.action_index].goods : 0;
        ++work.target_checks;
        if (const auto reason = target(w, ship, live_stop, true, goods); reason != Reject::None) {
            fail(reason);
            return;
        }
    }
    for (std::uint32_t used = 0; used < w.rules_.freight.work_per_visit; ++used) {
        ++work.steps;
        const auto& stop = r.stops[r.stop_index];
        if (r.phase == FreightPhase::NeedLeg) {
            ++work.target_checks;
            if (const auto reason = target(w, ship, stop, false); reason != Reject::None) {
                fail(reason);
                return;
            }
            const auto moved = ShipOrders::move(w, {ship.owner, MoveCmd{}},
                                                {ship.id, stop.dock_x_q8, stop.dock_y_q8}, nullptr);
            if (!moved.ok()) {
                fail(moved.status);
                return;
            }
            r.phase = FreightPhase::Sailing;
            r.expected_order_id = ship.nav->order->order_id;
            r.order_generation = ship.freight_generation;
            r.last_tick = now;
            r.credit_ticks = 0;
            observe(w, ship, FreightSignal::Pending, &fx, nullptr);
            return;
        }
        if (r.phase == FreightPhase::Wait) {
            const auto spent = std::min<std::uint64_t>(r.credit_ticks, r.wait_remaining);
            r.credit_ticks -= spent;
            r.wait_remaining -= static_cast<std::uint32_t>(spent);
            if (r.wait_remaining)
                return;
            ++work.target_checks;
            if (const auto reason = target(w, ship, stop, true, stop.actions[r.action_index].goods);
                reason != Reject::None) {
                fail(reason);
                return;
            }
            r.phase = FreightPhase::Scan;
            index_start(ship,
                        static_cast<std::size_t>(
                            std::min<std::uint64_t>(ship.holds.size(), type->nav->hold_count_a)),
                        stop.actions[r.action_index].direction == FreightDirection::LoadTarget);
            continue;
        }
        if (r.phase == FreightPhase::Scan) {
            auto& index = ship.freight.index();
            if (!index_current(ship, index)) {
                fail(Reject::BadValue);
                return;
            }
            const auto limit = std::min<std::uint64_t>(ship.holds.size(), type->nav->hold_count_a);
            const auto& action = stop.actions[r.action_index];
            if (r.scan_next < limit) {
                const auto i = r.scan_next;
                const auto& hold = ship.holds[i];
                if (hold.goods == action.goods) {
                    if (hold.amount_88 > UINT64_MAX - r.matching_raw) {
                        fail(Reject::BadValue);
                        return;
                    }
                    r.matching_raw += hold.amount_88;
                    if (action.direction == FreightDirection::UnloadAmount ||
                        hold.amount_88 < type->capacity_88)
                        r.matching_holds.push_back(i);
                } else if (action.direction == FreightDirection::LoadTarget && hold.goods == 0 &&
                           hold.amount_88 == 0)
                    r.empty_holds.push_back(i);
                if (action.direction == FreightDirection::LoadTarget)
                    index_scan(index, i, hold, action.goods, type->capacity_88, &work);
                ++r.scan_next;
                ++index.scanned;
                ++work.hold_scans;
                continue;
            }
            const auto raw = action.tons * 256;
            r.remaining_raw = action.direction == FreightDirection::LoadTarget
                                  ? (raw > r.matching_raw ? raw - r.matching_raw : 0)
                                  : raw;
            r.phase = FreightPhase::Transfer;
            continue;
        }
        if (r.phase == FreightPhase::Transfer) {
            auto& index = ship.freight.index();
            const auto& action = stop.actions[r.action_index];
            if (!index_current(ship, index)) {
                fail(Reject::BadValue);
                return;
            }
            const auto total = r.matching_holds.size() + r.empty_holds.size();
            if (r.remaining_raw == 0 || r.candidate_next == total) {
                if (r.candidate_next == 0 || r.remaining_raw != 0)
                    observe(w, ship, FreightSignal::Skipped, &fx, nullptr);
                next_action(ship, type->load_ticks);
                continue;
            }
            const auto selected = r.candidate_next < r.matching_holds.size()
                                      ? r.matching_holds[r.candidate_next]
                                      : r.empty_holds[r.candidate_next - r.matching_holds.size()];
            const auto& hold = ship.holds[selected];
            const auto bound = action.direction == FreightDirection::UnloadAmount
                                   ? hold.amount_88
                                   : (hold.amount_88 < type->capacity_88
                                          ? std::uint64_t{type->capacity_88} - hold.amount_88
                                          : 0);
            const auto tons = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(std::min(r.remaining_raw, bound) / 256, UINT32_MAX));
            if (tons == 0) {
                observe(w, ship, FreightSignal::Skipped, &fx, nullptr, Reject::BadValue);
                next_action(ship, type->load_ticks);
                continue;
            }
            ApplyResult transfer;
            std::optional<std::uint32_t> chosen;
            ShipHold before;
            if (action.direction == FreightDirection::LoadTarget) {
                ++work.resolver_queries;
                chosen = indexed_hold(ship, selected, action.goods, type->capacity_88, index);
                if (chosen) {
                    before = ship.holds[*chosen];
                    transfer = ShipCargo::load_impl(
                        w, {ship.owner, LoadCmd{}},
                        {ship.id, stop.warehouse, selected, action.goods, tons}, nullptr, *chosen);
                } else
                    transfer.status = Reject::HoldsFull;
            } else {
                chosen = selected;
                before = hold;
                transfer = ShipCargo::unload(
                    w, {ship.owner, UnloadCmd{}},
                    {ship.id, stop.warehouse, selected, action.goods, tons}, nullptr);
            }
            if (!transfer.ok()) {
                if (resource_refusal(transfer.status)) {
                    observe(w, ship, FreightSignal::Skipped, &fx, nullptr, transfer.status);
                    next_action(ship, type->load_ticks);
                    continue;
                }
                fail(transfer.status);
                return;
            }
            // The shared primitive committed exactly actual. There is no
            // fallible operation between that effect and canonical continuation.
            const auto actual = transfer.records[0].transferred_88;
            r.remaining_raw -= actual;
            ++r.candidate_next;
            ++work.transfers;
            if (action.direction == FreightDirection::LoadTarget) {
                heap_erase(index, *chosen, &work);
                heap_insert(index, *chosen, ship.holds[*chosen], action.goods, type->capacity_88,
                            &work);
                // A load only fills the earliest empty when no matching room is
                // available. Maintain the cursor here; never scan stale entries.
                if (before.goods == 0 && before.amount_88 == 0)
                    ++index.empty_next;
            }
            ++work.index_updates;
            observe(w, ship, FreightSignal::Transferred, &fx, nullptr, Reject::None, actual);
            continue;
        }
        if (r.phase == FreightPhase::CompleteStop) {
            const bool wrap = std::size_t{r.stop_index} + 1 == r.stops.size();
            if (wrap && r.completed_cycles == UINT64_MAX) {
                fail(Reject::SerialExhausted);
                return;
            }
            if (wrap) {
                ++r.completed_cycles;
                observe(w, ship, FreightSignal::Cycle, &fx, nullptr);
            }
            r.stop_index = wrap ? 0 : r.stop_index + 1;
            r.action_index = 0;
            r.phase = FreightPhase::NeedLeg;
            r.credit_ticks = 0;
            r.last_tick = now;
            // The next primitive may issue Move now, but movement itself starts
            // at a later visit. No old credit reaches that future destination.
            continue;
        }
    }
}
} // namespace anno::sim
