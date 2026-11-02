// UnloadCmd (see ship_cargo.hpp, docs/sim/ship-cargo.md).
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

ApplyResult ShipCargo::unload(World& w, const Command& command, const UnloadCmd& cmd,
                              const InAdvance*) {
    const TypeRegistry& types = *w.types_;
    const CargoRules& cargo = w.rules_.cargo;
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
    // 5-7: the warehouse and its city, found by id in the island that the id names (no scan).
    const IslandId island_id = island_of(cmd.warehouse);
    if (!island_id.valid() || island_id.v >= w.islands_.size() ||
        !w.islands_[island_id.v].has_value()) {
        return refuse(Reject::NoBuildingAtTile);
    }
    Island& island = *w.islands_[island_id.v];
    const Building* building = island.find_building(cmd.warehouse);
    if (building == nullptr) {
        return refuse(Reject::NoBuildingAtTile);
    }
    const BuildingType* btype = types.find(building->type);
    if (btype == nullptr || !std::binary_search(cargo.warehouse_prodtypes.begin(),
                                                cargo.warehouse_prodtypes.end(), btype->prodtype)) {
        return refuse(Reject::NotAWarehouse);
    }
    const auto key = island.cache.by_id.find(cmd.warehouse);
    if (key == island.cache.by_id.end()) {
        return refuse(Reject::NoBuildingAtTile);
    }
    City* city = island.city(key->second.city.city);
    if (city == nullptr) {
        return refuse(Reject::UnknownCity);
    }
    if (city->owner != command.issuer) {
        return refuse(Reject::NotCityOwner);
    }
    // 8: range, against the live state. The ship tile is the truncated position (Q24.8 >> 8).
    const std::int64_t tx = ship->pos.x >> 8, ty = ship->pos.y >> 8;
    const auto [fw, fh] = footprint(*btype, building->rotation);
    if (!footprint_in_range(tx, ty, building->x, building->y, fw, fh, cargo)) {
        return refuse(Reject::OutOfRange);
    }
    {
        // The warehouse's city must own one of the tiles of the disc outline (24 reads).
        bool near_city = false;
        const std::vector<std::uint32_t>& half = cargo.dock_half_widths;
        for (std::size_t k = 0; k < half.size() && !near_city; ++k) {
            const std::int64_t wk = half[k], kk = static_cast<std::int64_t>(k);
            const std::int64_t xs[4] = {tx - wk, tx + wk, tx - wk, tx + wk};
            const std::int64_t ys[4] = {ty - kk, ty - kk, ty + kk, ty + kk};
            for (int i = 0; i < 4 && !near_city; ++i) {
                if (xs[i] < INT32_MIN || xs[i] > INT32_MAX || ys[i] < INT32_MIN ||
                    ys[i] > INT32_MAX) {
                    continue;
                }
                const Slot idx = island.tile_index(static_cast<std::int32_t>(xs[i]),
                                                   static_cast<std::int32_t>(ys[i]));
                near_city = idx != kNoSlot && island.tiles.owner(idx) == city->number.v;
            }
        }
        if (!near_city) {
            return refuse(Reject::OutOfRange);
        }
    }
    // 9: the hold index, below the ship's hold count (the type's, when it has navigation facts).
    std::size_t hold_limit = ship->holds.size();
    if (ship_type->nav.has_value()) {
        const std::uint64_t declared =
            std::uint64_t{ship_type->nav->hold_count_a} + ship_type->nav->hold_count_b;
        hold_limit = static_cast<std::size_t>(std::min<std::uint64_t>(hold_limit, declared));
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

} // namespace anno::sim
