// World::check_invariants and the world-level queries (docs/sim/state.md §9.6,
// §4.14, stage S6).
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/create.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/figure.hpp>
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
    // The id counters, the figure registry and the event queue (stage S7).
    // 2^64 - 1 is legal: the counter is used up and hands out no more ids (F35).
    if (ids_.next_figure < 1) {
        return "ids.next_figure: " + std::to_string(ids_.next_figure) + " is not in 1 .. 2^64 - 1";
    }
    if (ids_.next_event_seq < 1) {
        return "ids.next_event_seq: " + std::to_string(ids_.next_event_seq) +
               " is not in 1 .. 2^64 - 1";
    }
    problem = validate_figures(figures_, ids_);
    if (!problem.empty()) {
        return problem;
    }
    // A hold names goods of the registry (the registry is the content; validate_figures sees
    // no registry).
    for (std::size_t s = 0; s < figures_.list.slots.size(); ++s) {
        if (!figures_.list.slots[s].has_value()) {
            continue;
        }
        const std::vector<ShipHold>& holds = figures_.list.slots[s]->holds;
        for (std::size_t h = 0; h < holds.size(); ++h) {
            if (types_->goods(holds[h].goods) == nullptr) {
                return "figures.list.slots[" + std::to_string(s) + "].holds[" + std::to_string(h) +
                       "].goods: " + std::to_string(holds[h].goods) + " is not in the goods table";
            }
        }
    }
    for (const auto& entry : events_.q) {
        if (entry.first.second >= ids_.next_event_seq) {
            return "events: the event (" + std::to_string(entry.first.first) + ", " +
                   std::to_string(entry.first.second) + ") has a sequence number not below " +
                   "ids.next_event_seq " + std::to_string(ids_.next_event_seq);
        }
    }
    // The script timers and the monopoly table (stage S12).
    problem = validate_timers(timers_, config_);
    if (problem.empty()) {
        problem = validate_monopoly(monopoly_, config_);
    }
    if (!problem.empty()) {
        return problem;
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
    // The counters of figures out equal the figures that still have that home: the
    // tick raises the counter, the merge creates the figure (docs/sim/state.md §7.2),
    // and the figure's end releases it and clears its home (end_figure, D 0x1003b6e0),
    // so a figure waiting on the expiry list is not counted.
    std::map<std::pair<BuildingId, std::uint32_t>, std::uint32_t> held;
    for (const std::optional<Figure>& f : figures_.list.slots) {
        if (f.has_value() && f->home.valid()) {
            ++held[{f->home, f->home_counter}];
        }
    }
    for (std::size_t i = 0; i < islands_.size(); ++i) {
        if (!islands_[i].has_value()) {
            continue;
        }
        for (std::size_t c = 0; c < islands_[i]->cities.size(); ++c) {
            if (!islands_[i]->cities[c].has_value()) {
                continue;
            }
            const BuildingIndex& index = islands_[i]->cities[c]->index;
            for (std::size_t l = 0; l < index.lists.size(); ++l) {
                for (std::size_t s = 0; s < index.lists[l].slots.size(); ++s) {
                    const std::optional<Building>& b = index.lists[l].slots[s];
                    if (!b.has_value()) {
                        continue;
                    }
                    const std::uint32_t n = figure_counter_count(b->payload);
                    for (std::uint32_t k = 0; k < n; ++k) {
                        const std::uint32_t counter = *figure_counter(b->payload, k);
                        const auto it = held.find({b->id, k});
                        const std::uint32_t figures = it != held.end() ? it->second : 0U;
                        if (counter != figures) {
                            return "islands[" + std::to_string(i) + "].cities[" +
                                   std::to_string(c) + "].index.lists[" + std::to_string(l) +
                                   "].slots[" + std::to_string(s) + "].payload: counter " +
                                   std::to_string(k) + " of figures out is " +
                                   std::to_string(counter) + " but " + std::to_string(figures) +
                                   " figures refer to it";
                        }
                    }
                }
            }
        }
    }
    // F24 (stage S12), last so that it hides no structural fact: the last advance's event
    // dispatch stopped at Rules::events_max_dispatch with a due event left (a game never
    // reaches the cap). This reads the advance's output, not state: a save and load of
    // the world, or the next advance, clears it.
    if (out_.events_capped) {
        return "events: the last advance stopped its dispatch at Rules::events_max_dispatch (" +
               std::to_string(rules_.events_max_dispatch) + ") with a due event left";
    }
    return {};
}

} // namespace anno::sim
