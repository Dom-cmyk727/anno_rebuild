#include <anno/sim/mine_source.hpp>

#include <anno/sim/production.hpp>

namespace anno::sim {

DepositInit deposit_load(std::uint32_t amount_tons, GoodsId ore_goods, bool reserve) {
    DepositInit d;
    d.reserve = reserve;
    d.remaining = (amount_tons & 0xffffU) << 8;
    if (ore_goods >= 1 && ore_goods <= 8 && reserve && d.remaining < kDepositReserve) {
        d.remaining = 2000U * kTon;
    }
    return d;
}

std::optional<std::size_t> find_ore_in_reach(const Island& island, const TypeRegistry& types,
                                             const BuildingType& t, const MineArea& area, bool deep,
                                             OreLookupStats* stats) {
    if (stats != nullptr) {
        ++stats->lookups;
    }
    if (!t.has_prod || t.prod.outputs.empty()) {
        return std::nullopt;
    }
    const std::string& goods = t.prod.outputs[0];
    const std::optional<GoodsId> ore_goods = types.goods_by_name(goods);
    if (!ore_goods) {
        return std::nullopt;
    }
    std::optional<std::size_t> best_ore;
    std::uint32_t best_remaining = 0;
    for (std::size_t i = 0; i < island.ore_objects.size(); ++i) {
        const OreObject& ob = island.ore_objects[i];
        if (stats != nullptr) {
            ++stats->ore_objects_examined;
        }
        if (!deposit_in_reach(area, ob.x, ob.y)) { // the cheap geometry first
            continue;
        }
        if (stats != nullptr) {
            ++stats->in_reach;
        }
        const FigureType* ft = types.find_figure_type(ob.type);
        if (ft == nullptr || ft->bewohntype != 145 || ft->ware != goods) {
            continue;
        }
        if (stats != nullptr) {
            ++stats->ware_matches;
        }
        Deposit prospective{goods, ob.x, ob.y, 0, true};
        const DepositInit init = deposit_load(ob.amount, *ore_goods);
        prospective.remaining = init.remaining;
        prospective.reserve = init.reserve;
        for (const Deposit& d : island.deposits) {
            if (stats != nullptr) {
                ++stats->deposits_examined;
            }
            if (d.goods == goods && d.x == ob.x && d.y == ob.y) {
                prospective = d; // the CURRENT state of an existing deposit
                break;
            }
        }
        const std::uint32_t score = deposit_remaining(prospective, deep);
        if (!best_ore || score > best_remaining) {
            best_ore = i;
            best_remaining = score;
        }
    }
    return best_ore;
}

std::optional<std::size_t> bind_mine_deposit(Island& island, const TypeRegistry& types,
                                             const Building& b, const BuildingType& t,
                                             bool deep, OreLookupStats* stats) {
    const std::optional<std::size_t> best_ore =
        find_ore_in_reach(island, types, t, mine_area(b, t), deep, stats);
    if (!best_ore) {
        return std::nullopt;
    }
    const std::string& goods = t.prod.outputs[0];
    const GoodsId ore_goods = *types.goods_by_name(goods);
    const OreObject& ob = island.ore_objects[*best_ore];
    for (std::size_t d = 0; d < island.deposits.size(); ++d) {
        if (stats != nullptr) {
            ++stats->deposits_examined;
        }
        const Deposit& dep = island.deposits[d];
        if (dep.goods == goods && dep.x == ob.x && dep.y == ob.y) {
            return d;
        }
    }
    const DepositInit init = deposit_load(ob.amount, ore_goods);
    island.deposits.push_back(Deposit{goods, ob.x, ob.y, init.remaining, init.reserve});
    return island.deposits.size() - 1;
}

} // namespace anno::sim
