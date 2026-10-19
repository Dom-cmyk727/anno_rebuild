// World::check_invariants and the world-level queries (docs/sim/state.md §9.6,
// §4.14, stage S6).
#include <string>
#include <vector>

#include <anno/sim/create.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

const Building* World::find_building(BuildingId id) const {
    const IslandId isl = island_of(id);
    if (isl.v >= islands_.size() || !islands_[isl.v].has_value()) {
        return nullptr;
    }
    return islands_[isl.v]->find_building(id);
}

const Building* World::building_at(CityRef city, std::int32_t x, std::int32_t y,
                                   std::uint32_t list_mask) const {
    if (!city.island.valid() || city.island.v >= islands_.size() ||
        !islands_[city.island.v].has_value()) {
        return nullptr;
    }
    return islands_[city.island.v]->building_at(config_.layout, city.city, x, y, list_mask);
}

void World::rect_query(CityRef city, const TileRect& rect, std::uint32_t list_mask,
                       std::vector<BuildingId>& out) const {
    if (!city.island.valid() || city.island.v >= islands_.size() ||
        !islands_[city.island.v].has_value()) {
        out.clear();
        return;
    }
    islands_[city.island.v]->rect_query(config_.layout, city.city, rect, list_mask, out);
}

std::string World::check_invariants() const {
    // The configuration and what is built from it (§3.1): a state is only
    // meaningful against a valid configuration.
    std::string problem = config_.validate();
    if (problem.empty()) {
        problem = rules_.validate();
    }
    if (problem.empty()) {
        problem = validate_seams(seams_, config_);
    }
    if (problem.empty()) {
        problem = types_->validate_against(config_);
        if (!problem.empty()) {
            problem = "registry does not fit the configuration: " + problem;
        }
    }
    if (problem.empty()) {
        problem = settings_.validate();
    }
    // The player table, the islands with their tiles, cities and buildings.
    if (problem.empty()) {
        problem = validate_players(players_, config_);
    }
    if (problem.empty()) {
        problem = validate_islands(islands_, config_, settings_, *types_);
    }
    if (!problem.empty()) {
        return problem;
    }
    // Relational facts: a city's owner is a player of the table.
    for (std::size_t i = 0; i < islands_.size(); ++i) {
        if (!islands_[i].has_value()) {
            continue;
        }
        const std::string p = "islands[" + std::to_string(i) + "]";
        for (std::size_t c = 0; c < islands_[i]->cities.size(); ++c) {
            const std::optional<City>& city = islands_[i]->cities[c];
            if (city.has_value() && players_.find(city->owner) == nullptr) {
                return p + ".cities[" + std::to_string(c) + "].owner: player " +
                       std::to_string(city->owner.v) + " is not in the player table";
            }
        }
    }
    // Footprints, then the derived indexes (the second reads what the first
    // guarantees).
    for (std::size_t i = 0; i < islands_.size(); ++i) {
        if (!islands_[i].has_value()) {
            continue;
        }
        problem = island_footprint_problem(*islands_[i], *types_);
        if (!problem.empty()) {
            return "islands[" + std::to_string(i) + "]." + problem;
        }
    }
    for (std::size_t i = 0; i < islands_.size(); ++i) {
        if (!islands_[i].has_value()) {
            continue;
        }
        problem = island_index_problem(*islands_[i], config_, *types_);
        if (!problem.empty()) {
            return "islands[" + std::to_string(i) + "]." + problem;
        }
    }
    return {};
}

} // namespace anno::sim
