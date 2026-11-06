#pragma once
// Test-only effect oracle: project the two independently specified data writes on an isolated
// pre-command copy. It does not invoke the transfer implementation or alter the genuine world.
#include <limits>

#include <anno/sim/command.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"
namespace cargo_oracle {
namespace sim = anno::sim;
enum class Difference { None, Target, Actor, Quantity, State };
inline Difference load_effect(const sim::World& before, const sim::World& after,
                              sim::PlayerId issuer, const sim::LoadCmd& cmd, sim::CityRef source,
                              std::size_t resolved, std::uint64_t quantity) {
    const auto island = sim::island_of(cmd.warehouse);
    if (island != source.island || island.v >= before.islands().size() ||
        !before.islands()[island.v] || !before.find_building(cmd.warehouse))
        return Difference::Target;
    const auto& index = before.islands()[island.v]->cache.by_id;
    const auto target = index.find(cmd.warehouse);
    if (target == index.end() || target->second.city != source)
        return Difference::Target;
    const auto* city = before.islands()[island.v]->city(source.city);
    if (!city || city->owner != issuer)
        return Difference::Target;
    const auto* ship = sim::find_figure(before.figures(), cmd.ship);
    if (!ship || ship->owner != issuer || resolved >= ship->holds.size())
        return Difference::Actor;
    const auto& hold = ship->holds[resolved];
    if (cmd.goods >= city->stock.stock.size() || quantity > city->stock.stock[cmd.goods] ||
        quantity > std::numeric_limits<std::uint64_t>::max() - hold.amount_88)
        return Difference::Quantity;
    auto expected = before;
    expected.set_transport_stats(nullptr);
    auto& debit = *sim::WorldTestAccess::islands(expected)[island.v]->cities[source.city.v];
    debit.stock.stock[cmd.goods] -= static_cast<std::uint32_t>(quantity);
    auto* credit = sim::find_figure(sim::WorldTestAccess::figures(expected), cmd.ship);
    credit->holds[resolved] = sim::ShipHold{cmd.goods, hold.amount_88 + quantity};
    return expected.save() == after.save() ? Difference::None : Difference::State;
}
} // namespace cargo_oracle
