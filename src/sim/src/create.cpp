// Creating and removing buildings (docs/sim/state.md §7.3, stage S6).
#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <anno/sim/create.hpp>
#include <anno/sim/walk_cells.hpp>

namespace anno::sim {

namespace {

/// The grid of list `list` of the island, sized on first use. Returns false when
/// the list id is beyond what the layout names (nothing to index).
std::vector<std::uint32_t>& grid_of(IslandIndexCache& cache, const SimConfig& cfg,
                                    const TileLayer& tiles, ListId list) {
    const std::size_t lists = std::max<std::size_t>(cfg.layout.lists.size(), std::size_t{list} + 1);
    if (cache.grids.size() < lists) {
        cache.grids.resize(lists);
    }
    std::vector<std::uint32_t>& g = cache.grids[list];
    if (g.empty()) {
        g.assign(static_cast<std::size_t>(tiles.tile_count()), 0);
    }
    return g;
}

/// Writes `serial` on every tile of the box (island-local coordinates).
void fill_box(std::vector<std::uint32_t>& grid, std::uint32_t width, const TileBox& box,
              std::uint32_t serial) {
    for (std::int64_t y = box.y0; y < box.y1; ++y) {
        for (std::int64_t x = box.x0; x < box.x1; ++x) {
            grid[static_cast<std::size_t>(y * width + x)] = serial;
        }
    }
}

/// Whether any grid of the island has a building on a tile of the box.
bool box_occupied(const IslandIndexCache& cache, std::uint32_t width, const TileBox& box) {
    for (const std::vector<std::uint32_t>& g : cache.grids) {
        if (g.empty()) {
            continue;
        }
        for (std::int64_t y = box.y0; y < box.y1; ++y) {
            for (std::int64_t x = box.x0; x < box.x1; ++x) {
                if (g[static_cast<std::size_t>(y * width + x)] != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

/// Adds (+1) or removes (-1) a building of `ty` to the derived counters of a
/// city. A counter that is already 0 stays 0 on removal; an index beyond a
/// counter table is skipped (a corrupt size is check_invariants' to report). The
/// service counters count the buildings of the layout's public list only (D:
/// Create 0x10024e18..0x1002500f and Remove 0x10025810..0x100258b0 handle them in the
/// public block); the house counts count the buildings of the House class.
void count_type(std::vector<std::uint32_t>& service, std::vector<std::uint32_t>& houses,
                const SimConfig& cfg, const BuildingType& ty, bool add) {
    const std::uint32_t bits = std::min<std::uint32_t>(cfg.service_bits, 32);
    const std::optional<ListId> list = cfg.layout.list_for(ty.prodtype);
    const bool public_building = list.has_value() && *list == cfg.layout.roles.public_list;
    for (std::uint32_t b = 0; public_building && b < bits && b < service.size(); ++b) {
        if ((ty.service_mask >> b & 1U) == 0) {
            continue;
        }
        if (add) {
            ++service[b];
        } else if (service[b] > 0) {
            --service[b];
        }
    }
    if (ty.cls == RuntimeClass::House) {
        if (const std::optional<std::uint32_t> g = house_group(cfg, ty.prodtype)) {
            if (*g < houses.size()) {
                if (add) {
                    ++houses[*g];
                } else if (houses[*g] > 0) {
                    --houses[*g];
                }
            }
        }
    }
}

/// Whether the class of a building has a status-time slot that the placement stamp
/// writes (D: the slot +0x120 behind property 0x23 is `ret 4`, 0x10037590, for the
/// base, house, market and public vtables; only the production classes store a time:
/// workshop +0x22, farm / fisher / trapper +0x20, mine +0x1e, and classes 9, 12 and 17
/// at +0x28, 0x1006ff40). Of the Inert classes that is DualWorkshop (PRODTYPE 12).
bool has_status_time(const Payload& payload, RuntimeClass cls) {
    // A market (T24) keeps the Inert rule it had as an unported class: no evidence
    // that its Create writes the status time.
    return (!std::holds_alternative<Inert>(payload) && !std::holds_alternative<Market>(payload)) ||
           cls == RuntimeClass::DualWorkshop;
}

/// Sets `last` (D: property 0x23, 0x10073380, always writes u16 +0x0a) of any
/// payload alternative and the status time of the classes that have the slot.
void stamp_placement(Payload& payload, RuntimeClass cls, std::uint16_t value) {
    const bool with_time = has_status_time(payload, cls);
    std::visit(
        [value, with_time](auto& p) {
            p.last = value;
            if (with_time) {
                p.status.time = value;
            }
        },
        payload);
}

} // namespace

City make_city(CityNo number, PlayerId owner, const SimConfig& cfg) {
    City c;
    c.number = number;
    c.owner = owner;
    c.stock.stock.assign(cfg.warehouse_goods, 0);
    c.index = make_building_index(cfg);
    c.goods.assign(cfg.warehouse_goods, GoodsSetting{});
    c.build_goods.assign(cfg.build_goods_count, BuildGoods{});
    c.trade_lists.assign(cfg.trade_lists, std::vector<TradeEntry>{});
    return c;
}

formats::Result<BuildingKey, Reject> create_building(const SimConfig& cfg, const Rules& rules,
                                                     const TypeRegistry& types, Island& island,
                                                     City& city, const PlacementRecord& rec,
                                                     std::uint32_t now, const Payload* initial) {
    // 1. Validation: no state is touched until it has passed.
    const BuildingType* ty = types.find(rec.type);
    if (ty == nullptr || rec.type.v == 0) {
        // Type id 0 is "no type" (Build skips it, §8.4); a building with id 0
        // would also leave a zero object on its footprint tiles.
        return Reject::UnknownType;
    }
    if (rec.rotation > 3) {
        return Reject::BadRotation;
    }
    const std::optional<ListId> list = cfg.layout.list_for(ty->prodtype);
    if (!list.has_value() || *list >= city.index.lists.size()) {
        return Reject::NotABuilding;
    }
    if (island.next_serial == 0 || island.next_serial == 0xffffffffU) {
        return Reject::SerialExhausted;
    }
    const auto [fw, fh] = footprint(*ty, rec.rotation);
    const TileLayer& tiles = island.tiles;
    if (!inside_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw,
                      fh)) {
        return Reject::OutOfIsland;
    }
    const TileBox box =
        clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw, fh);
    if (box_occupied(island.cache, tiles.width(), box)) {
        return Reject::Occupied;
    }
    const ClassInfo& cls = types.class_info(ty->cls);
    if (initial != nullptr && !payload_problem(*initial, *ty, cls, cfg).empty()) {
        return Reject::BadValue;
    }
    // A given mine names a deposit of this island or none (validate_cities' fact).
    if (initial != nullptr) {
        if (const auto* mine = std::get_if<Mine>(initial)) {
            if (mine->deposit.has_value() && *mine->deposit >= island.deposits.size()) {
                return Reject::BadValue;
            }
        }
    }

    // 2.-3. The payload of the class, every bit zero (F2); a mine's deep flag
    // (D: type attach, slot +0x28, before the position).
    Building b;
    b.payload = initial != nullptr ? *initial : make_payload(*ty, cls, cfg);
    if (initial == nullptr) {
        // The registry guarantees that make_payload fits the type (TypeRegistry::
        // build), so this cannot fail today; it keeps "a rejected creation changes
        // nothing, an accepted one keeps the invariants" self-contained should
        // make_payload and payload_problem ever diverge. Nothing is touched yet.
        if (!payload_problem(b.payload, *ty, cls, cfg).empty()) {
            return Reject::BadValue;
        }
        if (auto* mine = std::get_if<Mine>(&b.payload)) {
            mine->deep = mine_is_deep(ty->prod, ty->research_id);
        }
    }
    // 4. Position, rotation, the flag bytes (D: the house base constructor 0x10075c60
    // sets 0x82, and only the factory's House class kinds 97..101 run it; every other
    // class gets the plain constructor 0x100734c0 with 0x86).
    b.type = rec.type;
    b.x = rec.x;
    b.y = rec.y;
    b.rotation = rec.rotation;
    b.byte0e = ty->cls == RuntimeClass::House ? kByte0eHouse : kByte0eDefault;
    b.byte0f = rec.byte0f;
    // 5. The placement tick (D: property 0x23 -> 0x10073380 -> slot +0x120, F1).
    if (initial == nullptr) {
        stamp_placement(b.payload, ty->cls, static_cast<std::uint16_t>(now));
    }
    // 6. Attach to the city: a mine finds its deposit, position already set (D
    // slot +0x128).
    if (auto* mine = std::get_if<Mine>(&b.payload)) {
        if (!mine->deposit.has_value() && !ty->prod.outputs.empty()) {
            mine->deposit =
                find_deposit(island.deposits, ty->prod.outputs[0], mine->deep, mine_area(b, *ty));
        }
    }
    // 7. The id and the slot (the free-slot policy of §5.2).
    const std::uint32_t serial = island.next_serial;
    b.id = make_building_id(island.index, serial);
    BuildingList& target = city.index.lists[*list];
    const Slot slot =
        target.insert(std::move(b), SlotParams{cfg.list_min_growth, rules.slot_policy});
    if (slot == kNoSlot) {
        return Reject::ListFull;
    }
    island.next_serial = serial + 1;

    // 8. The derived indexes, and the footprint tiles' object and walk record.
    const BuildingKey key{CityRef{island.index, city.number}, *list, slot};
    island.cache.by_id[make_building_id(island.index, serial)] = key;
    fill_box(grid_of(island.cache, cfg, island.tiles, *list), tiles.width(), box, serial);
    count_type(city.index.service_counters, city.index.house_counts, cfg, *ty, true);
    // The same writer as the tile objects (0x10082040). Buildings are created
    // after them, so a building overwrites scenery under it. PlacementRecord
    // has no phase (BuildingSpec carries type, origin, rotation and byte0f):
    // the stored phase is 0, the reset of byte 3 (0x1008c81e). `b` was moved
    // into the slot; the placed copy still holds x, y and rotation.
    const Building& placed = *target.slots[slot];
    static_cast<void>(
        place_object_record(island.tiles, types, *ty, placed.x, placed.y, placed.rotation, 0));
    // 9. The shared tail (residents, running-cost statistics): no stored state yet
    // (O-9, O-23).
    return key;
}

bool remove_building(const SimConfig& cfg, const TypeRegistry& types, PlayerTable& players,
                     Island& island, const BuildingKey& key) {
    if (key.city.island != island.index) {
        return false;
    }
    City* city = island.city(key.city.city);
    if (city == nullptr || key.list >= city->index.lists.size()) {
        return false;
    }
    BuildingList& list = city->index.lists[key.list];
    if (key.slot >= list.slots.size() || !list.slots[key.slot].has_value()) {
        return false;
    }
    const Building& b = *list.slots[key.slot];
    const BuildingType* ty = types.find(b.type);
    // Undo the derived counters, erase the id and the footprint, clear the tiles.
    if (ty != nullptr) {
        count_type(city->index.service_counters, city->index.house_counts, cfg, *ty, false);
        const auto [fw, fh] = footprint(*ty, b.rotation);
        const TileLayer& tiles = island.tiles;
        const TileBox box =
            clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), b.x, b.y, fw, fh);
        if (key.list < island.cache.grids.size() && !island.cache.grids[key.list].empty()) {
            std::vector<std::uint32_t>& grid = island.cache.grids[key.list];
            const std::uint32_t serial = serial_of(b.id);
            for (std::int64_t y = box.y0; y < box.y1; ++y) {
                for (std::int64_t x = box.x0; x < box.x1; ++x) {
                    std::uint32_t& cell = grid[static_cast<std::size_t>(y * tiles.width() + x)];
                    if (cell == serial) {
                        cell = 0;
                    }
                }
            }
        }
        for (std::int64_t y = box.y0; y < box.y1; ++y) {
            for (std::int64_t x = box.x0; x < box.x1; ++x) {
                const std::uint64_t idx = static_cast<std::uint64_t>(y * tiles.width() + x);
                island.tiles.set_object(idx, 0);
                island.tiles.set_walk_code(idx, 2);
                island.tiles.set_walk_mask(idx, 0);
            }
        }
    }
    island.cache.by_id.erase(b.id);
    list.remove(key.slot);
    if (key.list == cfg.layout.roles.market_list) {
        if (Player* owner = players.find(city->owner)) {
            owner->defeat_pending = true;
        }
    }
    return true;
}

namespace {

/// The derived data of an island, recomputed from its buildings.
struct IndexData {
    IslandIndexCache cache;
    struct Counters {
        std::vector<std::uint32_t> service, houses;
    };
    /// The counters of the PRESENT cities, by slot: an absent slot gets none, so the
    /// memory follows the cities the stream holds and not city_slots x (service_bits +
    /// house_groups) (F33; city_slots, house_groups and service_bits may all be at
    /// their bounds in a hostile header).
    std::map<std::uint32_t, Counters> per_city;
    std::uint64_t buildings = 0; // live buildings in the lists
};

IndexData compute_index(const Island& island, const SimConfig& cfg, const TypeRegistry& types) {
    IndexData d;
    for (std::size_t c = 0; c < island.cities.size(); ++c) {
        if (!island.cities[c].has_value()) {
            continue;
        }
        IndexData::Counters& counters = d.per_city[static_cast<std::uint32_t>(c)];
        counters.service.assign(cfg.service_bits, 0);
        counters.houses.assign(cfg.house_groups, 0);
        const City& city = *island.cities[c];
        for (std::size_t l = 0; l < city.index.lists.size(); ++l) {
            const BuildingList& list = city.index.lists[l];
            for (std::size_t s = 0; s < list.slots.size(); ++s) {
                if (!list.slots[s].has_value()) {
                    continue;
                }
                const Building& b = *list.slots[s];
                ++d.buildings;
                d.cache.by_id[b.id] = BuildingKey{CityRef{island.index, city.number},
                                                  static_cast<ListId>(l), static_cast<Slot>(s)};
                const BuildingType* ty = types.find(b.type);
                if (ty == nullptr) {
                    continue;
                }
                count_type(counters.service, counters.houses, cfg, *ty, true);
                const auto [fw, fh] = footprint(*ty, b.rotation);
                const TileLayer& tiles = island.tiles;
                const TileBox box = clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(),
                                                  tiles.height(), b.x, b.y, fw, fh);
                if (box.empty()) {
                    continue;
                }
                fill_box(grid_of(d.cache, cfg, tiles, static_cast<ListId>(l)), tiles.width(), box,
                         serial_of(b.id));
            }
        }
    }
    return d;
}

} // namespace

void rebuild_island_index(Island& island, const SimConfig& cfg, const TypeRegistry& types) {
    IndexData d = compute_index(island, cfg, types);
    island.cache = std::move(d.cache);
    for (auto& [c, counters] : d.per_city) {
        island.cities[c]->index.service_counters = std::move(counters.service);
        island.cities[c]->index.house_counts = std::move(counters.houses);
    }
}

std::string island_index_problem(const Island& island, const SimConfig& cfg,
                                 const TypeRegistry& types) {
    const IndexData d = compute_index(island, cfg, types);
    if (d.cache.by_id.size() != d.buildings) {
        return "cache.by_id: the lists hold " + std::to_string(d.buildings) +
               " buildings but only " + std::to_string(d.cache.by_id.size()) +
               " distinct ids (two buildings share an id)";
    }
    if (d.cache.by_id != island.cache.by_id) {
        return "cache.by_id: " + std::to_string(island.cache.by_id.size()) +
               " entries, a recomputation from the lists gives " +
               std::to_string(d.cache.by_id.size()) + " (or different places)";
    }
    // A grid that was never used may be empty or absent; compare non-empty ones.
    const std::size_t lists = std::max(d.cache.grids.size(), island.cache.grids.size());
    for (std::size_t l = 0; l < lists; ++l) {
        const std::vector<std::uint32_t> none;
        const std::vector<std::uint32_t>& have =
            l < island.cache.grids.size() ? island.cache.grids[l] : none;
        const std::vector<std::uint32_t>& want = l < d.cache.grids.size() ? d.cache.grids[l] : none;
        if (have == want) {
            continue;
        }
        // A grid of zeros equals an absent one (a list that held a building once).
        const auto all_zero = [](const std::vector<std::uint32_t>& g) {
            return std::all_of(g.begin(), g.end(), [](std::uint32_t v) { return v == 0; });
        };
        if ((have.empty() && all_zero(want)) || (want.empty() && all_zero(have))) {
            continue;
        }
        return "cache.grids[" + std::to_string(l) + "]: differs from a recomputation";
    }
    for (std::size_t c = 0; c < island.cities.size(); ++c) {
        if (!island.cities[c].has_value()) {
            continue;
        }
        const BuildingIndex& idx = island.cities[c]->index;
        const auto found = d.per_city.find(static_cast<std::uint32_t>(c));
        if (found == d.per_city.end()) {
            continue; // compute_index makes an entry for every present city
        }
        const IndexData::Counters& counters = found->second;
        if (idx.service_counters != counters.service) {
            return "cities[" + std::to_string(c) +
                   "].index.service_counters: differ from a recomputation";
        }
        if (idx.house_counts != counters.houses) {
            return "cities[" + std::to_string(c) +
                   "].index.house_counts: differ from a recomputation";
        }
    }
    return {};
}

std::string island_footprint_problem(const Island& island, const TypeRegistry& types) {
    const TileLayer& tiles = island.tiles;
    std::vector<std::uint8_t> seen(static_cast<std::size_t>(tiles.tile_count()), 0);
    for (std::size_t c = 0; c < island.cities.size(); ++c) {
        if (!island.cities[c].has_value()) {
            continue;
        }
        const City& city = *island.cities[c];
        for (std::size_t l = 0; l < city.index.lists.size(); ++l) {
            const BuildingList& list = city.index.lists[l];
            for (std::size_t s = 0; s < list.slots.size(); ++s) {
                if (!list.slots[s].has_value()) {
                    continue;
                }
                const Building& b = *list.slots[s];
                const BuildingType* ty = types.find(b.type);
                if (ty == nullptr) {
                    continue;
                }
                const std::string where = "cities[" + std::to_string(c) + "].index.lists[" +
                                          std::to_string(l) + "].slots[" + std::to_string(s) + "]";
                const auto [fw, fh] = footprint(*ty, b.rotation);
                if (!inside_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), b.x, b.y,
                                  fw, fh)) {
                    return where + ": the footprint is not inside the island";
                }
                const TileBox box = clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(),
                                                  tiles.height(), b.x, b.y, fw, fh);
                for (std::int64_t y = box.y0; y < box.y1; ++y) {
                    for (std::int64_t x = box.x0; x < box.x1; ++x) {
                        const auto idx = static_cast<std::size_t>(y * tiles.width() + x);
                        if (seen[idx] != 0) {
                            return where + ": the footprint shares tile (" +
                                   std::to_string(x + tiles.x0()) + ", " +
                                   std::to_string(y + tiles.y0()) + ") with another building";
                        }
                        seen[idx] = 1;
                        if (tiles.object(idx) == 0) {
                            return where + ": the footprint tile (" +
                                   std::to_string(x + tiles.x0()) + ", " +
                                   std::to_string(y + tiles.y0()) + ") carries no object";
                        }
                    }
                }
            }
        }
    }
    return {};
}

} // namespace anno::sim
