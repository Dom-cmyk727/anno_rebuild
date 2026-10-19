// Creating and removing buildings, and the derived indexes that follow them
// (spec: docs/sim/state.md §7.3, §4.6-§4.8, stage S6).
//
// create_building is the single creation path for the scenario loader, the
// commands (S10) and the tests. In the original `Create(rec, slot, now, &out)` is
// a method of the city's BuildingIndex (this = city + 0x144, D: AnnoWelt.dll
// 0x10024010): it constructs the building, initialises it, attaches it to the
// city and inserts it into its list. The order of the steps is the original's
// (§7.3). Nothing here prices a building or checks who may build where: that is
// the command layer (S10); the checks create_building makes itself are the ones
// that keep the state consistent (the type exists, the rotation is valid, the
// footprint is on the island and free of other buildings).
#pragma once

#include <cstdint>
#include <string>

#include <anno/formats/result.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// Creates the building described by `rec` in `city` of `island` at the world
/// tick `now` (its low 32 bits; the loader passes 0) and returns where it sits.
/// The steps, in the original's order (§7.3): validation that touches no state
/// (a known type with a non-zero id: UnknownType; rotation below 4: BadRotation;
/// a type with a list in the layout: NotABuilding; a serial left in the island:
/// SerialExhausted; the footprint on the island: OutOfIsland; no other
/// building's footprint on it: Occupied; a payload that fits the type:
/// BadValue); the payload of the type's class with every bit zero (F2); a mine's
/// deep flag; position, rotation, byte0e (0x82 for the house list, else 0x86) and
/// byte0f; `last` and the status time set to the low 16 bits of `now` (F1: the
/// placement tick in single and multiplayer); a mine's deposit (found at attach,
/// position already set); the id from the island's serial counter, the free-slot
/// policy of the list (§5.2: hint, growth, Rules::slot_policy); the derived
/// indexes (id map, footprint grid, service counters, house counts); the
/// footprint tiles' object := the type id (stand-in, O-5). A rejected creation
/// changes nothing.
///
/// `initial` (the loader's BuildingSpec::payload) replaces the make_* default:
/// stocks, switches and `last` are kept as given (a mine without a deposit still
/// finds one at attach). It must satisfy payload_problem for the type, else the
/// creation is rejected (BadValue).
///
/// The footprint and overlap checks are the rebuild's own (F32, fix): the
/// original relies on the placement rule (0x1007c790) and creates blindly.
/// Preconditions: `cfg`, `rules` valid (SimConfig::validate, Rules::validate),
/// `city` is one of `island.cities`, `island.cache` is current.
[[nodiscard]] formats::Result<BuildingKey, Reject>
create_building(const SimConfig& cfg, const Rules& rules, const TypeRegistry& types, Island& island,
                City& city, const PlacementRecord& rec, std::uint32_t now,
                const Payload* initial = nullptr);

/// Removes the building at `key` (D 0x100255c0): undoes the derived counters,
/// erases the id and the footprint from the cache, clears the footprint tiles'
/// object (stand-in, O-5), empties the list slot (hint, cursor and the vector's
/// length stay) and, when the list is the market list of the layout's roles,
/// raises the owner's defeat_pending flag (D: player byte +0x295; consumer
/// 0x10020550, O-22). Returns false and changes nothing when `key` names no
/// building (another island, no such city or list, a slot out of range or empty).
bool remove_building(const SimConfig& cfg, const TypeRegistry& types, PlayerTable& players,
                     Island& island, const BuildingKey& key);

/// Rebuilds the derived indexes of one island from scratch: the id map, the
/// footprint grids and, in every city, the service counters and house counts.
/// Total: a building whose type is unknown, whose footprint leaves the island or
/// whose slot is out of range is skipped for the part it cannot contribute to
/// (check_invariants reports such a state). Used after a load.
void rebuild_island_index(Island& island, const SimConfig& cfg, const TypeRegistry& types);

/// Empty when the island's derived indexes equal a recomputation; otherwise one
/// message naming the first difference ("cache.by_id: ...", "cities[0].index.
/// service_counters: ...").
[[nodiscard]] std::string island_index_problem(const Island& island, const SimConfig& cfg,
                                               const TypeRegistry& types);

/// Empty when the buildings of the island are inside it, no two footprints share
/// a tile, and every footprint tile carries a non-zero object (the type id,
/// stand-in O-5; §9.6 "footprints are disjoint and inside the island"). Needs the
/// types to size the footprints; a building of an unknown type is skipped (the
/// structural check reports it).
[[nodiscard]] std::string island_footprint_problem(const Island& island, const TypeRegistry& types);

} // namespace anno::sim
