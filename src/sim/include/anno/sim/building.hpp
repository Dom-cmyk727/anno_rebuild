// Buildings and their lists (spec: docs/sim/state.md §4.6, §7.3, §9.3, stage S6).
//
// A building is the common part the original's base class holds (D: AnnoWelt.dll
// base constructor 0x100734c0: position, rotation, the flag bytes +0x0e / +0x0f)
// plus the payload of its runtime class: one of the production structs of
// production.hpp, or `Inert` for every class whose tick is not ported (markets,
// public buildings, houses, military, ships, stalls, the base class). Inert
// holds a slot all the same: the scheduler arithmetic divides by the length of
// the list including its null holes, so the order in which buildings tick stays
// faithful while those classes are unported.
//
// `last`, `blocked`, `suspended` and the StatusTracker live in the payload only
// (no second copy). A building belongs to the list its type belongs to
// (ListLayout::list_for); its identity is the BuildingId, its order identity the
// (city, list, slot) of BuildingKey.
#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/slot_list.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The payload of every class without a ported tick (D: the base class 0x100734c0
/// with the no-op tick 0x10001030, and every class whose tick is unported; the
/// tick does not touch `last`, D15). Its members are the common part of the
/// production payloads, so the initial values written at creation (last, status
/// time) are the same for all alternatives.
struct Inert {
    std::uint16_t last = 0;
    bool blocked = false;
    bool suspended = false;
    StatusTracker status;
};

/// Alternative order = the save tag order of PayloadTag (types.hpp), append-only.
/// FurTrapper uses Farm; the class comes from the type.
using Payload = std::variant<Inert, Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine>;

/// The byte +0x0e of a building (D: 0x86 in the base constructor, 0x82 for a
/// house; property 0x2b sets it from a COMP chunk; meaning open, O-38).
inline constexpr std::uint8_t kByte0eDefault = 0x86;
inline constexpr std::uint8_t kByte0eHouse = 0x82;

struct Building {
    BuildingId id;
    TypeId type;
    /// The north-west tile (the original's u16 +6 / +8, widened).
    std::int32_t x = 0, y = 0;
    /// 0..3, flags +0x0c bits 0-1. Bits 6-7 of +0x0c are uninitialised heap in the
    /// original and never read (F2): not modelled.
    std::uint8_t rotation = 0;
    /// D +0x0e (0x82 for houses); meaning open (O-38).
    std::uint8_t byte0e = kByte0eDefault;
    /// D +0x0f (property 4); the original overwrites it from the terrain height
    /// check (0x1007c790, O-27).
    std::uint8_t byte0f = 0;
    Payload payload;
};

/// What create_building needs; the wire form of a command (PlacementWire, S8)
/// converts to it after validation.
struct PlacementRecord {
    TypeId type;
    std::int32_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t byte0f = 0;
};

using BuildingList = SlotList<Building>;

/// D: the object at city +0x144 (vtable 0x100c2e48).
struct BuildingIndex {
    /// D +0x04: the ONE stamp shared by the scheduled lists.
    std::uint16_t stamp = 0;
    /// One list per ListLayout::lists entry, indexed by ListId.
    std::vector<BuildingList> lists;
    // Derived (rebuilt on load, never hashed or saved; check_invariants compares
    // them with a recomputation):
    /// D +6..+0x45: public-need counters, one per service bit; a building of the
    /// layout's public list (Roles::public_list; the original raises them only in
    /// the public block of Create 0x10024e18..0x1002500f and lowers them in Remove
    /// 0x10025810..0x100258b0) whose type has service_mask bit b raises counter b.
    /// The original keeps u16 words and a derived mask "counter != 0" at index +0x50
    /// (recomputed on every change; O-40), which the rebuild derives from these when
    /// a consumer needs it.
    std::vector<std::uint32_t> service_counters;
    /// D +0x46..: houses per group (Create increments index[+0x46+2g]).
    std::vector<std::uint32_t> house_counts;
};

/// An index sized for `cfg`: one fresh list (SimConfig::list_initial_size null
/// elements, D ctor 0x100236a0, A7) per ListLayout list, the stamp 0, the
/// counters zero.
[[nodiscard]] BuildingIndex make_building_index(const SimConfig& cfg);

/// The footprint (w', h') of a type at a rotation: width and height, swapped on
/// an odd rotation (D 0x100279e6, 0x10071c80).
[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> footprint(const BuildingType& type,
                                                                std::uint8_t rotation);

/// A half-open box of tiles in island-local coordinates (0,0 = the island's
/// top-left tile). x1 and y1 are exclusive; the box is empty when x1 <= x0 or
/// y1 <= y0.
struct TileBox {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    [[nodiscard]] bool empty() const { return x1 <= x0 || y1 <= y0; }
};

/// The part of the world rectangle (x, y, w, h) that lies on a layer with
/// origin (lx, ly) and size lw x lh, in layer coordinates. Edges are formed in
/// i64, so a position near the i32 limits cannot wrap.
[[nodiscard]] inline TileBox clip_to_layer(std::int32_t lx, std::int32_t ly, std::uint32_t lw,
                                           std::uint32_t lh, std::int32_t x, std::int32_t y,
                                           std::uint32_t w, std::uint32_t h) {
    TileBox b;
    b.x0 = std::max<std::int64_t>(std::int64_t{x} - lx, 0);
    b.y0 = std::max<std::int64_t>(std::int64_t{y} - ly, 0);
    b.x1 = std::min<std::int64_t>(std::int64_t{x} + w - lx, lw);
    b.y1 = std::min<std::int64_t>(std::int64_t{y} + h - ly, lh);
    return b;
}

/// Whether the whole rectangle (x, y, w, h) lies on the layer (an empty
/// rectangle does not).
[[nodiscard]] inline bool inside_layer(std::int32_t lx, std::int32_t ly, std::uint32_t lw,
                                       std::uint32_t lh, std::int32_t x, std::int32_t y,
                                       std::uint32_t w, std::uint32_t h) {
    return w > 0 && h > 0 && x >= lx && y >= ly && std::int64_t{x} + w <= std::int64_t{lx} + lw &&
           std::int64_t{y} + h <= std::int64_t{ly} + lh;
}

/// The house group of a house type: its PRODTYPE counted from the first number of
/// the layout's house block (97 = PIONIER, group 0, in the original). In the
/// original the factory 0x10024010 uses `kind - 0x61` only to pick the 0x118-byte
/// BGRUPPE record of the group, and the first byte of that record (BGruppeNr) is
/// the counter index (`inc word [city + 0x144 + 0x46 + 2 * al]`, Create
/// 0x10024c13..0x10024c2d; Remove reads the same byte through building slot +0x88):
/// for the shipped BGruppen.dat (PIONIER, SIEDLER, BUERGER, KAUFLEUTE, ARISTOKRATEN
/// in file order) the two are equal, for another order of the file they are not
/// (docs/re/population.md §2). A layout with several blocks mapped to the house
/// list numbers the groups on across them: the k-th such block (k from 0, in block
/// order) holds the groups k * block_size .. (k + 1) * block_size - 1.
/// nullopt when the layout's house list owns no block, the PRODTYPE is not in
/// one or the group is not below SimConfig::house_groups.
[[nodiscard]] std::optional<std::uint32_t> house_group(const SimConfig& cfg,
                                                       std::uint32_t prodtype);

/// The runtime payload of a new building of `type` (§7.3 step 2: every bit zero,
/// F2): the make_* constructor of its class, sized from the type and the config
/// (the small farm's counters come from SimConfig::small_farm_animals), Inert for
/// a class without a ported tick. Mine::deep is set (step 3), the deposit search
/// and the placement stamp are create_building's.
[[nodiscard]] Payload make_payload(const BuildingType& type, const ClassInfo& cls,
                                   const SimConfig& cfg);

/// The structural facts a payload must have for its type (§9.6 "payload matches
/// the type"): the alternative named by ClassInfo::payload, the vector sizes
/// the step functions index by (as make_payload sizes them), a Mine type with an
/// output. Empty when they hold, else one message ("holds Farm, the class
/// Workshop needs Workshop"). `derived` fields (WeaponsWorkshop::unlocked) are
/// not checked.
[[nodiscard]] std::string payload_problem(const Payload& payload, const BuildingType& type,
                                          const ClassInfo& cls, const SimConfig& cfg);

/// The footprint a mine searches from (D 0x1006ddf0): position and rotated size.
[[nodiscard]] MineArea mine_area(const Building& b, const BuildingType& type);

} // namespace anno::sim
