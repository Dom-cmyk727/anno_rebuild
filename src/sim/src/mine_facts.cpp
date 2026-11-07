#include <variant>

#include <anno/sim/mine_facts.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

std::optional<MineFacts> mine_facts(const World& world, BuildingId id) {
    const Building* b = world.find_building(id);
    if (b == nullptr) {
        return std::nullopt;
    }
    const auto* m = std::get_if<Mine>(&b->payload);
    if (m == nullptr) {
        return std::nullopt;
    }
    MineFacts f;
    f.deep = m->deep;
    f.output_88 = m->out;
    if (const TransportStats* stats = world.transport_stats()) {
        const auto it = stats->forestry.mine_made_88.find(id);
        f.produced_88 = it == stats->forestry.mine_made_88.end() ? 0U : it->second;
    }
    const std::uint32_t island = island_of(id).v;
    const auto& islands = world.islands();
    if (m->deposit && island < islands.size() && islands[island] &&
        *m->deposit < islands[island]->deposits.size()) {
        const Deposit& d = islands[island]->deposits[*m->deposit];
        f.bound = true;
        f.deposit_index = static_cast<std::uint64_t>(*m->deposit);
        f.ore_x = d.x;
        f.ore_y = d.y;
        f.remaining_88 = d.remaining;
        f.reserve = d.reserve;
        const bool setting = world.settings().depletes();
        f.depletes = d.reserve ? (!m->deep && setting) : setting;
    }
    if (!f.bound) {
        f.state = MineState::Unbound;
    } else if (!m->switched_on) {
        f.state = MineState::SwitchedOff;
    } else if (m->blocked) {
        f.state = MineState::Blocked;
    } else {
        f.state = MineState::Working;
    }
    return f;
}

} // namespace anno::sim
