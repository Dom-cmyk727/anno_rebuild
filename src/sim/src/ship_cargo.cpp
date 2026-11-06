// UnloadCmd and LoadCmd (see ship_cargo.hpp, docs/sim/ship-cargo.md).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include <anno/sim/building.hpp>
#include <anno/sim/figure_veto.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/ship_cargo.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

ApplyResult refuse(Reject r) {
    ApplyResult out;
    out.status = r;
    return out;
}

/// 0 when `p` lies in [lo, lo + length - 1], else the distance to the nearer edge tile.
std::int64_t axis_distance(std::int64_t p, std::int64_t lo, std::int64_t length) {
    const std::int64_t hi = lo + length - 1;
    if (p >= lo && p <= hi) {
        return 0;
    }
    return std::min(p < lo ? lo - p : p - lo, p < hi ? hi - p : p - hi);
}

/// Whether `city_number` owns one of the tiles of the disc outline around the ship tile (tx, ty)
/// (the unload's and the load's test, section 5: 24 reads).
bool city_owns_outline_tile(const Island& island, std::uint32_t city_number, std::int64_t tx,
                            std::int64_t ty, const CargoRules& cargo) {
    const std::vector<std::uint32_t>& half = cargo.dock_half_widths;
    for (std::size_t k = 0; k < half.size(); ++k) {
        const std::int64_t wk = half[k], kk = static_cast<std::int64_t>(k);
        const std::int64_t xs[4] = {tx - wk, tx + wk, tx - wk, tx + wk};
        const std::int64_t ys[4] = {ty - kk, ty - kk, ty + kk, ty + kk};
        for (int i = 0; i < 4; ++i) {
            if (xs[i] < INT32_MIN || xs[i] > INT32_MAX || ys[i] < INT32_MIN || ys[i] > INT32_MAX) {
                continue;
            }
            const Slot idx = island.tile_index(static_cast<std::int32_t>(xs[i]),
                                               static_cast<std::int32_t>(ys[i]));
            if (idx != kNoSlot && island.tiles.owner(idx) == city_number) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

std::uint64_t ShipCargo::amount_88(std::uint32_t tons, std::uint64_t hold_amount_88,
                                   std::uint64_t free_88) {
    return std::min({std::uint64_t{tons} * 256, hold_amount_88, free_88});
}

bool ShipCargo::footprint_in_range(std::int64_t tx, std::int64_t ty, std::int64_t x0,
                                   std::int64_t y0, std::int64_t w, std::int64_t h,
                                   const CargoRules& rules) {
    const std::vector<std::uint32_t>& half = rules.dock_half_widths;
    if (half.empty()) {
        return false;
    }
    const std::int64_t dy = axis_distance(ty, y0, h);
    if (dy >= static_cast<std::int64_t>(half.size())) {
        return false; // dy > radius
    }
    const std::int64_t dx = axis_distance(tx, x0, w);
    return dx <= static_cast<std::int64_t>(half[static_cast<std::size_t>(dy)]);
}

CargoDestination ShipCargo::destination(const World& w, const Figure& ship, BuildingId warehouse,
                                        bool range) {
    const auto bad = [](Reject r) { return CargoDestination{r, {}}; };
    const auto island_id = island_of(warehouse);
    if (!island_id.valid() || island_id.v >= w.islands().size() || !w.islands()[island_id.v])
        return bad(Reject::NoBuildingAtTile);
    const auto& island = *w.islands()[island_id.v];
    const auto* building = island.find_building(warehouse);
    if (building == nullptr)
        return bad(Reject::NoBuildingAtTile);
    const auto* type = w.types().find(building->type);
    const auto& cargo = w.rules().cargo;
    if (type == nullptr || !std::binary_search(cargo.warehouse_prodtypes.begin(),
                                               cargo.warehouse_prodtypes.end(), type->prodtype))
        return bad(Reject::NotAWarehouse);
    const auto key = island.cache.by_id.find(warehouse);
    if (key == island.cache.by_id.end())
        return bad(Reject::NoBuildingAtTile);
    const auto* city = island.city(key->second.city.city);
    if (city == nullptr)
        return bad(Reject::UnknownCity);
    if (city->owner != ship.owner)
        return bad(Reject::NotCityOwner);
    if (range) {
        const std::int64_t tx = ship.pos.x >> 8, ty = ship.pos.y >> 8;
        const auto [fw, fh] = footprint(*type, building->rotation);
        if (!footprint_in_range(tx, ty, building->x, building->y, fw, fh, cargo) ||
            !city_owns_outline_tile(island, city->number.v, tx, ty, cargo))
            return bad(Reject::OutOfRange);
    }
    return {Reject::None, key->second.city};
}

ApplyResult ShipCargo::unload(World& w, const Command& command, const UnloadCmd& cmd,
                              const InAdvance*) {
    const TypeRegistry& types = *w.types_;
    const veto::Rules vrules; // the ship class is the founding command's (BEWOHNTYPE 113)
    // Step 0 (the issuer is a known player) is Commands::apply's. 1: the request.
    if (cmd.tons == 0) {
        return refuse(Reject::BadValue);
    }
    // 2-4: the ship.
    const Figure* ship = find_figure(w.figures_, cmd.ship);
    if (ship == nullptr) {
        return refuse(Reject::UnknownFigure);
    }
    const FigureType* ship_type = types.find_figure_type(ship->kind);
    if (ship_type == nullptr || ship_type->bewohntype != vrules.ship_kind) {
        return refuse(Reject::NotAShip);
    }
    if (ship->owner != command.issuer) {
        return refuse(Reject::NotShipOwner);
    }
    const auto dest = destination(w, *ship, cmd.warehouse, true);
    if (dest.status != Reject::None)
        return refuse(dest.status);
    City* city = w.islands_[dest.city.island.v]->city(dest.city.city);
    // 9: the hold index, below the cargo hold count. With navigation facts that is hold_count_a
    // only (original 0x100ac476: valid cargo indices end at a - 1; hold_count_b is not a transfer
    // bound, MAP-CARGO-3 C1); it is also capped by the holds the figure actually carries. A type
    // without navigation facts has no hold count: the established fallback is the carried holds
    // (an adaptation, not an original rule; the load refuses such a type with NoCargoFacts).
    std::size_t hold_limit = ship->holds.size();
    if (ship_type->nav.has_value()) {
        hold_limit = static_cast<std::size_t>(
            std::min<std::uint64_t>(hold_limit, ship_type->nav->hold_count_a));
    }
    if (cmd.hold >= hold_limit) {
        return refuse(Reject::BadIndex);
    }
    // 10-11: the goods.
    if (cmd.goods == 0 || cmd.goods >= city->stock.stock.size()) {
        return refuse(Reject::UnsupportedGoods);
    }
    const ShipHold& hold = ship->holds[cmd.hold];
    if (hold.goods != cmd.goods || hold.amount_88 == 0) {
        return refuse(Reject::HoldGoodsMismatch);
    }
    // 12: room in the store.
    const std::uint64_t room = city->stock.free_88(cmd.goods);
    if (room == 0) {
        return refuse(Reject::StockFull);
    }
    // The plan: x > 0 because tons, the hold amount and the room are all non-zero.
    const std::uint64_t x = amount_88(cmd.tons, hold.amount_88, room);

    // Commit: nothing below can fail. The credit is the exact planned amount (refused otherwise,
    // before the hold is touched), then the hold is debited by exactly that amount.
    if (!city->stock.credit(cmd.goods, x)) {
        return refuse(Reject::AtomicAbort); // unreachable: x <= room
    }
    ShipHold& debited = find_figure(w.figures_, cmd.ship)->holds[cmd.hold];
    debited.amount_88 -= x;
    if (debited.amount_88 == 0) {
        debited.goods = 0; // the emptied-hold normal form (0x1003b9a0)
    }
    ApplyResult out;
    RecordResult rr;
    rr.building = cmd.warehouse;
    rr.transferred_88 = x;
    out.records.push_back(rr);
    return out;
}

std::optional<std::size_t> ShipCargo::resolve_hold(const std::vector<ShipHold>& holds,
                                                   std::size_t limit, std::uint32_t capacity_88,
                                                   std::size_t preferred, GoodsId goods) {
    const auto room_of = [capacity_88](const ShipHold& h) -> std::uint64_t {
        return h.amount_88 >= capacity_88 ? 0 : std::uint64_t{capacity_88} - h.amount_88;
    };
    if (preferred < holds.size() && holds[preferred].goods == goods &&
        room_of(holds[preferred]) >= 256) {
        return preferred;
    }
    std::optional<std::size_t> candidate;
    std::uint64_t best = 0;
    for (std::size_t k = 0; k < std::min(limit, holds.size()); ++k) {
        const ShipHold& h = holds[k];
        if (h.goods == 0 && h.amount_88 == 0) {
            if (!candidate.has_value()) {
                candidate = k;
            }
        } else if (h.goods == goods) {
            const std::uint64_t room = room_of(h);
            if (room > 0 && best < room) {
                best = room;
                candidate = k;
            }
        }
    }
    return candidate;
}

ApplyResult ShipCargo::load(World& w, const Command& command, const LoadCmd& cmd,
                            const InAdvance*) {
    const TypeRegistry& types = *w.types_;
    const veto::Rules vrules; // the ship class is the founding command's (BEWOHNTYPE 113)
    // Step 0 (the issuer is a known player) is Commands::apply's. 1: the request.
    if (cmd.tons == 0) {
        return refuse(Reject::BadValue);
    }
    // 2-4: the ship.
    const Figure* ship = find_figure(w.figures_, cmd.ship);
    if (ship == nullptr) {
        return refuse(Reject::UnknownFigure);
    }
    const FigureType* ship_type = types.find_figure_type(ship->kind);
    if (ship_type == nullptr || ship_type->bewohntype != vrules.ship_kind) {
        return refuse(Reject::NotAShip);
    }
    if (ship->owner != command.issuer) {
        return refuse(Reject::NotShipOwner);
    }
    const auto dest = destination(w, *ship, cmd.warehouse, true);
    if (dest.status != Reject::None)
        return refuse(dest.status);
    City* city = w.islands_[dest.city.island.v]->city(dest.city.city);
    // 9: the cargo facts. A type without navigation facts has no hold count: refused, not guessed.
    if (!ship_type->nav.has_value()) {
        return refuse(Reject::NoCargoFacts);
    }
    // 10: the preferred hold lies below the cargo hold count (hold_count_a only, not _b).
    const std::size_t limit = static_cast<std::size_t>(
        std::min<std::uint64_t>(ship->holds.size(), ship_type->nav->hold_count_a));
    if (cmd.hold >= limit) {
        return refuse(Reject::BadIndex);
    }
    // 11: the goods.
    if (cmd.goods == 0 || cmd.goods >= city->stock.stock.size()) {
        return refuse(Reject::UnsupportedGoods);
    }
    // 12-14: the plan, from the live hold and store, before any write.
    const std::uint32_t capacity = ship_type->capacity_88;
    const std::optional<std::size_t> chosen =
        resolve_hold(ship->holds, limit, capacity, cmd.hold, cmd.goods);
    if (!chosen.has_value()) {
        return refuse(Reject::HoldsFull);
    }
    const std::uint64_t held = ship->holds[*chosen].amount_88;
    const std::uint64_t room = held >= capacity ? 0 : std::uint64_t{capacity} - held;
    if (room == 0) {
        return refuse(Reject::HoldsFull);
    }
    const std::uint64_t available = std::uint64_t{city->stock.stock[cmd.goods]} / 256 * 256;
    if (available == 0) {
        return refuse(Reject::StockShort);
    }
    const std::uint64_t x = amount_88(cmd.tons, room, available); // > 0: all three are non-zero

    // Commit: nothing below can fail. The debit is the exact planned amount (refused before the
    // hold is touched), then the hold is credited by exactly that amount; held + x <= capacity.
    if (!city->stock.debit(cmd.goods, x)) {
        return refuse(Reject::AtomicAbort); // unreachable: x <= floor_T(stock)
    }
    ShipHold& credited = find_figure(w.figures_, cmd.ship)->holds[*chosen];
    credited.goods = cmd.goods;
    credited.amount_88 = held + x;
    ApplyResult out;
    RecordResult rr;
    rr.building = cmd.warehouse;
    rr.transferred_88 = x;
    out.records.push_back(rr);
    return out;
}

} // namespace anno::sim
