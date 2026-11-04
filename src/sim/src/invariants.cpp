// World::check_invariants and the world-level queries (docs/sim/state.md §9.6,
// §4.14, stage S6).
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <anno/sim/create.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/ship.hpp>
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

std::string World::resident_problem() const {
    // MAP-POP-LIVE-1 L1 (docs/sim/population-live.md section 3 "Ownership"): a house's walker slot
    // names a live figure that holds exactly that slot; a figure that holds a house slot is named
    // by it; a resident job has SimConfig::resident_cargo_slots cargo slots of known goods, and an
    // empty slot (goods 0) carries no amount. Refused, never repaired.
    const auto island_ptr = [&](IslandId id) -> const Island* {
        return id.valid() && id.v < islands_.size() && islands_[id.v].has_value() ? &*islands_[id.v]
                                                                                  : nullptr;
    };
    for (std::size_t s = 0; s < figures_.list.slots.size(); ++s) {
        if (!figures_.list.slots[s].has_value()) {
            continue;
        }
        const Figure& f = *figures_.list.slots[s];
        const std::string where = "figures.list.slots[" + std::to_string(s) + "]";
        if (f.resident.has_value()) {
            const ResidentState& r = *f.resident;
            if (r.cargo.size() != config_.resident_cargo_slots) {
                return where + ".resident.cargo: " + std::to_string(r.cargo.size()) +
                       " slots, SimConfig.resident_cargo_slots is " +
                       std::to_string(config_.resident_cargo_slots);
            }
            for (std::size_t k = 0; k < r.cargo.size(); ++k) {
                const ResidentCargo& c = r.cargo[k];
                if (c.goods >= config_.warehouse_goods) {
                    return where + ".resident.cargo[" + std::to_string(k) +
                           "].goods: " + std::to_string(c.goods) +
                           " is not below SimConfig.warehouse_goods";
                }
                if (c.goods == 0 && c.amount_88 != 0) {
                    return where + ".resident.cargo[" + std::to_string(k) +
                           "]: an empty slot (goods 0) carries an amount";
                }
            }
            if (r.next > r.route.size() + 1) {
                return where + ".resident.next: " + std::to_string(r.next) +
                       " is beyond the route (" + std::to_string(r.route.size()) + " waypoints)";
            }
        }
        if (f.resident.has_value() && !f.expiry_stamp.has_value() && !f.home.valid()) {
            return where + ".resident: a walker that has not ended has no home (a house walker "
                           "holds its house's walker slot until its end)";
        }
        if (f.home.valid()) {
            const Island* is = island_ptr(island_of(f.home));
            const Building* b = is != nullptr ? is->find_building(f.home) : nullptr;
            const House* h = b != nullptr ? std::get_if<House>(&b->payload) : nullptr;
            if (h != nullptr &&
                (f.home_counter >= h->walkers.size() || h->walkers[f.home_counter] != f.id)) {
                return where + ": it holds walker slot " + std::to_string(f.home_counter) +
                       " of house " + std::to_string(f.home.v) + " which does not name it";
            }
            if (f.resident.has_value() && b != nullptr && h == nullptr) {
                return where + ".resident: the home " + std::to_string(f.home.v) +
                       " is not a House-payload building";
            }
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
            for (const BuildingList& list : islands_[i]->cities[c]->index.lists) {
                for (const std::optional<Building>& b : list.slots) {
                    const House* h = b.has_value() ? std::get_if<House>(&b->payload) : nullptr;
                    if (h == nullptr) {
                        continue;
                    }
                    for (std::size_t k = 0; k < h->walkers.size(); ++k) {
                        if (!h->walkers[k].valid()) {
                            continue;
                        }
                        const Figure* f = find_figure(figures_, h->walkers[k]);
                        if (f == nullptr || f->home != b->id || f->home_counter != k) {
                            return "islands[" + std::to_string(i) + "].cities[" +
                                   std::to_string(c) + "] house " + std::to_string(b->id.v) +
                                   ".walkers[" + std::to_string(k) + "]: figure " +
                                   std::to_string(h->walkers[k].v) +
                                   " is not a live figure holding that slot";
                        }
                    }
                }
            }
        }
    }
    return {};
}

std::string World::harvest_problem() const {
    // MAP-ECON-1 E2 (docs/sim/forestry.md 2, 2.1): the harvest facts that need the buildings, the
    // tiles or the rules. Refused, never repaired. No fact requires a currently existing home: a
    // worker whose Farm was demolished keeps the unresolved id until its next arrival.
    const auto island_ptr = [&](IslandId id) -> const Island* {
        return id.valid() && id.v < islands_.size() && islands_[id.v].has_value() ? &*islands_[id.v]
                                                                                  : nullptr;
    };
    // The worker of every claim target in ToTree/Cut, for the enhanced facts.
    std::map<std::pair<IslandId, TileXY>, std::vector<FigureId>> targeting;
    for (std::size_t s = 0; s < figures_.list.slots.size(); ++s) {
        if (!figures_.list.slots[s].has_value() || !figures_.list.slots[s]->harvest.has_value()) {
            continue;
        }
        const Figure& f = *figures_.list.slots[s];
        const HarvestState& h = *f.harvest;
        const std::string where = "figures.list.slots[" + std::to_string(s) + "].harvest";
        if (f.home.valid()) {
            const Island* is = island_ptr(island_of(f.home));
            const Building* b = is != nullptr ? is->find_building(f.home) : nullptr;
            if (b != nullptr && !std::holds_alternative<Farm>(b->payload)) {
                return where + ": the home " + std::to_string(f.home.v) +
                       " is not a Farm-payload building";
            }
        }
        if (h.step == HarvestStep::ToTree || h.step == HarvestStep::Cut) {
            const Island* is = f.home.valid() ? island_ptr(island_of(f.home)) : nullptr;
            if (is == nullptr || !is->tiles.contains(h.target.x, h.target.y)) {
                return where + ".target: (" + std::to_string(h.target.x) + ", " +
                       std::to_string(h.target.y) + ") is not a tile of the home's island";
            }
            if (!f.expiry_stamp.has_value()) {
                targeting[{island_of(f.home), h.target}].push_back(f.id);
            }
        }
    }
    for (std::size_t i = 0; i < islands_.size(); ++i) {
        if (!islands_[i].has_value()) {
            continue;
        }
        const Island& is = *islands_[i];
        const std::string p = "islands[" + std::to_string(i) + "].claims";
        if (!rules_.forestry_claim_hygiene) {
            if (!is.claims.empty()) {
                return p + ": " + std::to_string(is.claims.size()) +
                       " owner entries under the faithful rules (the original stores no owner)";
            }
            continue;
        }
        // Enhanced (E-i): every entry names an existing worker in ToTree/Cut that targets the
        // tile, and the tile's bit is set. (E-iv: a map holds one entry per tile.)
        for (const auto& [tile, worker] : is.claims) {
            const std::string e =
                p + "[(" + std::to_string(tile.x) + ", " + std::to_string(tile.y) + ")]";
            const Slot idx = is.tile_index(tile.x, tile.y);
            if (idx == kNoSlot) {
                return e + ": not a tile of the island";
            }
            if (!is.tiles.claimed(idx)) {
                return e + ": an owner entry without the claim bit";
            }
            const auto it = targeting.find({IslandId{static_cast<std::uint32_t>(i)}, tile});
            if (it == targeting.end() ||
                std::find(it->second.begin(), it->second.end(), worker) == it->second.end()) {
                return e + ": owner " + std::to_string(worker.v) +
                       " is no live harvest worker in ToTree/Cut targeting the tile";
            }
        }
        // Enhanced (E-ii): no stray bit. (E-iii, a worker without an entry, is a valid
        // transitional state after an invalidation.)
        for (std::uint64_t t = 0; t < is.tiles.tile_count(); ++t) {
            if (!is.tiles.claimed(t)) {
                continue;
            }
            const TileXY xy{is.tiles.x0() + static_cast<std::int32_t>(t % is.tiles.width()),
                            is.tiles.y0() + static_cast<std::int32_t>(t / is.tiles.width())};
            if (!is.claims.contains(xy)) {
                return p + ": the claim bit of tile (" + std::to_string(xy.x) + ", " +
                       std::to_string(xy.y) + ") has no owner entry (a stray claim)";
            }
        }
    }
    return {};
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
        problem = house_updates_problem(config_, rules_, *types_);
    }
    if (problem.empty()) {
        problem = settings_.validate();
    }
    // The player table, the islands with their tiles, cities and buildings.
    if (problem.empty()) {
        problem = validate_players(players_, config_);
    }
    if (problem.empty()) {
        problem = shared_visibility_problem(rules_, players_, config_);
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
    problem = validate_navigation(figures_, ids_, *types_, rules_);
    if (!problem.empty()) {
        return problem;
    }
    problem = figure_index_problem(figures_);
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
    problem = harvest_problem();
    if (!problem.empty()) {
        return problem;
    }
    problem = resident_problem();
    if (!problem.empty()) {
        return problem;
    }
    if (!fault_message_.empty()) {
        // MAP-POP-LIVE-1 L4 (P-1): a recorded fault of the live population update is reported
        // until the world is replaced; the text is a diagnostic (not saved).
        return "world fault: " + fault_message_;
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
