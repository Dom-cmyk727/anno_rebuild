// Where a Build command may place a building (spec: docs/sim/state.md §8.4 steps 2
// and 3, §12 O-27, §13 D17; stage S10).
//
// Two rules, both read-only over one island (MAP-BUILD-1 C: the second is now the contract of
// docs/sim/ship-placement.md; the stand-in is only an explicit, named test seam):
//  - the footprint owner rule of the command layer (D: AnnoFrame.dll 0x10058890, op 6
//    of the apply function 0x100589a0): the footprint lies on the island and its tiles
//    belong to the target city or to nobody, at least one of them to the city;
//  - the placement rule (AnnoWelt.dll 0x1007c790 through island slot +0x6c: bounds,
//    overlap, terrain class, height; docs/re/ship-placement.md): placement_rule below.
//    Seams::placement_rule replaces it; its default (a null function) IS this rule.
// Neither prices a building or touches state; create_building (create.hpp) keeps its
// own consistency checks (F32).
#pragma once

#include <cstdint>

#include <anno/sim/building.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/seams.hpp>
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

/// The decision and the authoritative item height of one record (docs/sim/ship-placement.md
/// section 3). Read-only, total, O(footprint + vertex window).
struct PlacementResult {
    /// None = accepted.
    Reject reject = Reject::None;
    /// Record byte 8 = Building::byte0f = the aux low byte of every footprint tile; 0 unless
    /// accepted.
    std::uint8_t height = 0;
    friend bool operator==(const PlacementResult&, const PlacementResult&) = default;
};

/// The data of the placement rule, with the original's values as defaults (nothing here is a
/// limit of the engine; a mod or a rules profile can pass another). Path to configurability: a
/// SimConfig/Rules field of the same name would feed this struct; it is a parameter of the rule
/// now so that no hashed state moves with it (docs/sim/ship-placement.md section 3, "Registry
/// facts").
struct PlacementParams {
    /// The smallest accepted terrain level of an ordinary land type: trunc(2 * 0.64f / 0.04f) = 32
    /// (AnnoWelt 0x1007cbeb-0x1007cc03).
    std::uint32_t min_level = 32;
    /// The vertex values of a flat window are multiples of this (the 0xf0 quantisation of
    /// AnnoGame 0x10008f90: `v & 0xf0`, whose equality with the maximum needs a multiple of 16).
    std::uint32_t level_quantum = 16;
    /// PRODTYPE group exempt from min_level (0x1007cbe4: the group 0x141, PRODTYPE 321..352).
    std::uint32_t exempt_group = 0x141;
    /// Ordinary land: the flag word without ignored_flag_bits equals this (bit 4: the height class
    /// test runs; AnnoGame 0x10008f90, 0x1000911-0x10009116) ...
    std::uint32_t ordinary_class = 0x0004;
    /// ... and the bit the rule does not read (0x80; seen in the shipped words 0x0084 and 0x0004).
    std::uint32_t ignored_flag_bits = 0x0080;
    /// The flag bit whose coverage test is not decoded: always Unsupported (0x1007c955-0x1007cb3d).
    std::uint32_t coverage_flag = 0x10;
};

/// The facts that make a type an ordinary land road (docs/sim/road-placement.md section 2). A
/// default argument like PlacementParams: no hashed state moves, and a data-defined type with
/// these facts is a road without an engine edit.
struct RoadParams {
    /// PRODTYPE of the land roads (STRASSE; types.cpp name table, Named{193}).
    std::uint32_t prodtype = 193;
    /// The class bits (flag word & 0x7000) of a land road: the road class (accepted at once by the
    /// original) and the slope class (mx - lo > 0x10 refused; AnnoGame 0x10008f90).
    std::uint32_t road_class = 0x4000;
    std::uint32_t slope_class = 0x1000;
    /// The slope limit of the slope class.
    std::uint32_t slope_limit = 0x10;
};

/// Whether `t` is an ordinary land road: PRODTYPE `params.prodtype`, a 16-bit flag word whose class
/// bits are the road or the slope class, no coverage bit 0x10, a mode other than the single-quad
/// average (flag word & 3 != 1) and a non-empty footprint. Facts of the parsed type only.
[[nodiscard]] bool is_land_road(const BuildingType& t, const RoadParams& params = {});

/// The placement rule of a normal Build record for the EFFECTIVE type (after any upgrade
/// rewrite), docs/sim/ship-placement.md section 2 (R1-R6); oracle tools/placement_ref.py
/// (engine_place). Order: the class gate (R3: ordinary land `(F & ~0x80) == 0x4`, coast
/// `(F & 0x7017) == 0x2007`, anything else, bit 0x10 or a zero footprint is Unsupported, as is a
/// flag word above 16 bits and a rotation above 3), bounds in 64 bits (R1, OutOfIsland), the
/// objects under the footprint through site::overlap_with_existing (R2: Occupied; an object id the
/// registry does not know is Unsupported), then the vertex window of VertexGrid (R4 ordinary:
/// one common multiple of 16, at least min_level unless the type's group is exempt, else
/// TerrainRefused; R5 coast: site::coast_height_rule, no level test). A grid that cannot answer
/// (no terrain, placeholder, absent or wrong-sized column, an unreadable vertex) is Unsupported:
/// never an accept and never a height of 0. Building tiles carry the building's type id, so a
/// building under the footprint is answered by R2 (or, when the types are replaceable, by the
/// F32 check of create_building).
/// A land road (is_land_road) is a tile object and goes the road way of docs/sim/road-placement.md
/// section 3: occupancy with the extra refusal of an existing group-0xc1 piece that is no land road
/// (R-A3), then the window result by the flag word's mode (0 max, 2 min, 3 window max of the cell
/// minima), the slope test of the slope class, and the level floor `params.min_level` (R-A2).
[[nodiscard]] PlacementResult placement_rule(const Island& island, const TypeRegistry& types,
                                             const BuildingType& effective,
                                             const PlacementRecord& rec,
                                             const PlacementParams& params = {});

/// The D17 stand-in as a placement result (height 0): Occupied when a tile of the footprint
/// carries a non-zero object, None otherwise. It reads neither the flag word nor the terrain
/// (OQ-052). Kept for the explicit, named test seam kStandInPlacementSeamName; it is NOT the
/// default of Seams::placement_rule.
[[nodiscard]] Reject stand_in_placement_rule(const Island& island, const BuildingType& type,
                                             const PlacementRecord& rec);

/// The seam name of the stand-in: the identity the header hash sees.
inline constexpr const char* kStandInPlacementSeamName = "stand_in_placement";

/// Seams::PlacementRuleFn form of stand_in_placement_rule (ctx and the registry unused).
[[nodiscard]] PlacementResult stand_in_placement_seam_fn(void* ctx, const Island& island,
                                                         const TypeRegistry& types,
                                                         const BuildingType& type,
                                                         const PlacementRecord& rec);

/// The stand-in installed deliberately: `s.placement_rule = stand_in_placement_seam();`. For suites
/// that isolate another subsystem from terrain; the name keeps the choice visible in the header
/// hash. Never the default.
[[nodiscard]] inline Seam<Seams::PlacementRuleFn> stand_in_placement_seam() {
    return {kStandInPlacementSeamName, &stand_in_placement_seam_fn, nullptr};
}

} // namespace anno::sim
