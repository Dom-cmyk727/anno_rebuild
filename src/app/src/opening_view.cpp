#include <algorithm>

#include <anno/app/opening_view.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/tick.hpp>

namespace anno::app {

const ShipView* OpeningView::ship(sim::FigureId id) const {
    // `ships` is in FigureId order.
    const auto it = std::lower_bound(ships.begin(), ships.end(), id,
                                     [](const ShipView& s, sim::FigureId v) { return s.id < v; });
    return it != ships.end() && it->id == id ? &*it : nullptr;
}

const CityView* OpeningView::city(sim::CityRef ref) const {
    for (const CityView& c : cities) {
        if (c.ref == ref) {
            return &c;
        }
    }
    return nullptr;
}

const ShipEventView* OpeningView::event_for(sim::FigureId ship, std::uint64_t order_id) const {
    for (const ShipEventView& e : events) {
        if (e.ship == ship && e.order_id == order_id) {
            return &e;
        }
    }
    return nullptr;
}

void ShipEventLog::append(const sim::SimEvent& event) {
    ShipEventView v;
    switch (event.kind) {
    case sim::SimEventKind::ShipArrived:
        v.kind = ShipEventKind::Arrived;
        break;
    case sim::SimEventKind::ShipRouteFailed:
        v.kind = ShipEventKind::RouteFailed;
        break;
    case sim::SimEventKind::ShipStopped:
        v.kind = ShipEventKind::Stopped;
        break;
    default:
        return;
    }
    v.tick = event.tick;
    v.ship = event.figure;
    v.player = event.player;
    v.order_id = event.order_id;
    v.reason = event.code;
    v.seq = next_++;
    kept_.push_back(v);
    if (kept_.size() > OpeningView::kMaxEvents * 2) {
        // Amortised trimming: the newest kMaxEvents stay.
        kept_.erase(kept_.begin(),
                    kept_.end() - static_cast<std::ptrdiff_t>(OpeningView::kMaxEvents));
    }
}

std::vector<ShipEventView> ShipEventLog::newest() const {
    const std::size_t n = std::min(kept_.size(), OpeningView::kMaxEvents);
    return std::vector<ShipEventView>(kept_.end() - static_cast<std::ptrdiff_t>(n), kept_.end());
}

namespace {

/// The city's warehouse: the first building (list order) whose type's PRODTYPE the cargo rules
/// accept.
std::optional<WarehouseView> warehouse_of(const sim::World& world, const sim::City& city) {
    const auto& prodtypes = world.rules().cargo.warehouse_prodtypes;
    for (const sim::BuildingList& list : city.index.lists) {
        for (const std::optional<sim::Building>& b : list.slots) {
            if (!b) {
                continue;
            }
            const sim::BuildingType* type = world.types().find(b->type);
            if (type == nullptr ||
                !std::binary_search(prodtypes.begin(), prodtypes.end(), type->prodtype)) {
                continue;
            }
            const auto [w, h] = sim::footprint(*type, b->rotation);
            WarehouseView v;
            v.id = b->id;
            v.type = b->type;
            v.tile = sim::TileXY{b->x, b->y};
            v.w = w;
            v.h = h;
            v.rotation = b->rotation;
            return v;
        }
    }
    return std::nullopt;
}

} // namespace

OpeningView collect_opening_view(const sim::World& world, sim::PlayerId local,
                                 const ShipEventLog& events, std::uint64_t turn) {
    OpeningView out;
    out.turn = turn;
    out.tick = world.clock().tick();
    out.local = local;
    if (const sim::Player* player = world.players().find(local)) {
        out.gold = player->money;
    }
    const sim::FigureRegistry& registry = world.figures();
    for (const auto& entry : registry.by_id) {
        const sim::Figure* f = sim::find_figure(registry, entry.first);
        if (f == nullptr || !f->nav.has_value() || f->parent.valid()) {
            continue;
        }
        ShipView s;
        s.id = f->id;
        s.owner = f->owner;
        s.kind = f->kind;
        s.pos = f->pos;
        s.dir16 = f->dir16;
        s.holds.reserve(f->holds.size());
        for (const sim::ShipHold& h : f->holds) {
            s.holds.push_back({h.goods, h.amount_88});
        }
        if (f->nav->order) {
            s.order_id = f->nav->order->order_id;
            s.routed = f->nav->order->routed;
            s.target_x_q8 = f->nav->order->target_x_q8;
            s.target_y_q8 = f->nav->order->target_y_q8;
        }
        out.ships.push_back(std::move(s));
    }
    const auto& islands = world.islands();
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (!islands[i]) {
            continue;
        }
        for (const std::optional<sim::City>& city : islands[i]->cities) {
            if (!city || city->owner != local) {
                continue;
            }
            CityView c;
            c.ref = sim::CityRef{islands[i]->index, city->number};
            c.owner = city->owner;
            c.name = city->name;
            c.warehouse = warehouse_of(world, *city);
            c.stock_88 = city->stock.stock;
            c.capacity_88 = city->stock.capacity;
            out.cities.push_back(std::move(c));
        }
    }
    out.events = events.newest();
    out.events_next = events.next();
    out.next_order_id = world.ids().next_order_id;
    return out;
}

} // namespace anno::app
