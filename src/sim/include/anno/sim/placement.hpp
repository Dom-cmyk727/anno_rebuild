// Where a Build command may place a building (spec: docs/sim/state.md §8.4 steps 2
// and 3, §12 O-27, §13 D17; stage S10).
//
// Two rules, both read-only over one island:
//  - the footprint owner rule of the command layer (D: AnnoFrame.dll 0x10058890, op 6
//    of the apply function 0x100589a0): the footprint lies on the island and its tiles
//    belong to the target city or to nobody, at least one of them to the city;
//  - the placement rule, whose original (AnnoWelt.dll 0x1007c790 through island slot
//    +0x6c: terrain, coast, mountain, overlap, height) is unread (O-27). Its stand-in
//    (D17) refuses a footprint that covers a tile object. Seams::placement_rule
//    replaces the stand-in.
// Neither prices a building or touches state; create_building (create.hpp) keeps its
// own consistency checks (F32).
#pragma once

#include <anno/sim/building.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The footprint owner rule of Build (§8.4 step 2; D 0x10058890). The footprint is
/// `footprint(type, rec.rotation)` from the north-west tile (rec.x, rec.y). Returns:
/// OutOfIsland when a footprint tile is not on the island; TileNotOwned when a tile
/// belongs to a city other than `city`, when the tiles that belong to a city belong to
/// two different ones, or when every tile is neutral (SimConfig::neutral_owner: the
/// original resolves an all-neutral footprint to the city numbered neutral_owner,
/// which does not exist); None otherwise.
[[nodiscard]] Reject footprint_owner_rule(const Island& island, const SimConfig& cfg,
                                          const BuildingType& type, const PlacementRecord& rec,
                                          CityNo city);

/// The default placement rule (D17 stand-in for D 0x1007c790, O-27): Occupied when a
/// tile of the footprint carries a non-zero object (scenery, a growing item, or the
/// object a building writes on its own tiles, O-5), None otherwise. The type's flag
/// word (0x2000 coast, 0x1000 mountain, 0x4000 road or bridge; I) is not read, so the
/// stand-in accepts placements the original refuses (OQ-052). The signature is the
/// seam's (Seams::placement_rule) without its context. Precondition: the footprint
/// lies on the island (footprint_owner_rule); tiles outside it are not read.
[[nodiscard]] Reject stand_in_placement_rule(const Island& island, const BuildingType& type,
                                             const PlacementRecord& rec);

} // namespace anno::sim
