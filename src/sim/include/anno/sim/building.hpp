// Buildings and their lists (spec: docs/sim/state.md §4.6, §7.3, §9.3, stage S6).
//
// A building is the common part the original's base class holds (D: AnnoWelt.dll
// base constructor 0x100734c0: position, rotation, the flag bytes +0x0e / +0x0f)
// plus the payload of its runtime class: one of the production structs of
// production.hpp, or `Inert` for every class whose tick is not ported (public
// buildings, houses, military, ships, stalls, the base class; markets have their own
// payload since T24). Inert
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
    /// Stream133: minimal financial selector for unported bodies. nullopt means
    /// unsafe/truncated original financial input; booking-enabled worlds refuse it.
    /// This does not implement production or enable new switch commands.
    std::optional<bool> financial_active = true;
};

/// The state of a quarry (MAP-EXTRACT-2 part Q, docs/sim/quarry-bricks.md Q3.1; D vtable
/// 0x100c2614). The original's class is a stub with no tick and a writer that saves 0 bytes; the
/// only state this engine keeps is the claim set of DECIDED Q-A: one flag per ENTRANCE of the type
/// (the original's byte +0x18 holds one bit per entrance, 8 at most; here the count is the type's
/// entrance count, no hardcoded 8). A flag is set at a mason worker's step-A choice and cleared at
/// the end of its wait or at its abort; goal registration skips a flagged entrance. The members
/// before `claims` are those of `Inert` in the same order (a quarry is Inert in every other
/// respect).
struct Quarry {
    std::uint16_t last = 0;
    bool blocked = false;
    bool suspended = false;
    StatusTracker status;
    std::optional<bool> financial_active = true;
    std::vector<bool> claims;
};

/// The state of an Inn (D vtable 0x100c96d8): the Inert prefix plus the tavern stock (+0x18;
/// delivered by `+0xc0` = 0x100756d0, offered by `+0xa4` = 0x10075940, debited by the bill
/// 0x100756e0), the taken statistic (+0x1a, raised by the bill, decayed by the tick) and the
/// carriers on their way (the original's two pointer slots +0x1c / +0x20; here a counter like the
/// workshop's, so the number of active carriers is data-limited by the type's max figures,
/// docs/sim/population-live.md 23). MAP-POP-CLOSE-2 K1: `tavern` and `taken` are the original's u16
/// values held in a u32 field: every operation wraps modulo 65536 exactly as the original's 16-bit
/// adds (inn.hpp), and the invariant check refuses a value above 0xffff. `last` is the gate stamp
/// of the tick 0x100755f0.
struct Inn {
    std::uint16_t last = 0;
    bool blocked = false;
    bool suspended = false;
    StatusTracker status;
    std::uint32_t tavern = 0;
    std::uint32_t taken = 0;
    std::uint32_t carriers_out = 0;
};

/// The state of a house (docs/sim/state.md §4.7c, docs/re/population.md §11.2; D house
/// object, vtable 0x100c9958). The group is not a field: it follows from the type
/// (house_group) and the registry's group table. Mood is computed, not stored; the
/// walkers are the figure registry's; the original's mood flags (+0x0c bits 4/5) are
/// omitted because nothing writes them yet (owner question 7).
struct House {
    /// +0x0a. The constructor writes 0 and property 0x23 the creation time; P1 leaves
    /// it 0 and nothing reads it before P2.
    std::uint16_t last = 0;
    /// +0x2c, residents * 64 (the original's i16, widened: CLAUDE.md "no hardcoded
    /// limits"; the constructor writes 5 * 64).
    std::int32_t residents_64 = 5 * 64;
    /// +0x2e, the original's u16 `E` (the decayed resident sum the satisfaction is
    /// scaled by), widened; the constructor writes 5 * 128. Not in the original's save
    /// (Load writes residents << 7); our own save keeps it.
    std::uint32_t e = 5 * 128;
    /// +0x30 + 2j, the stock of need j of the house's group (the original's u16,
    /// widened, 16 slots; here one entry per need of the group, as the group table
    /// says).
    std::vector<std::uint32_t> stock;
    /// MAP-POP-LIVE-1 L1 (stream 128): +0x18..+0x24, the walker slots (D: 4 in the original; here
    /// one per walker the TYPE allows, the type's +0xb8(0), validated against the registry). An
    /// invalid id is an empty slot; a figure that holds slot k has `home` = this building and
    /// `home_counter` = k (World::check_invariants).
    std::vector<FigureId> walkers;
};

/// Alternative order = the save tag order of PayloadTag (types.hpp), append-only.
/// FurTrapper uses Farm; the class comes from the type.
using Payload = std::variant<Inert, Workshop, WeaponsWorkshop, Farm, SmallFarm, Mine, Market, House,
                             Inn, Quarry>;

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
    /// D +0x1e (property 0xe), the KONTID of a market: the island's Kontor counter value taken when
    /// it was founded, or the loaded record's. 0 for every other building. ADAPT: u32.
    std::uint32_t kontor_id = 0;
    Payload payload;
};

/// What create_building needs; the wire form of a command (PlacementWire, S8)
/// converts to it after validation.
struct PlacementRecord {
    TypeId type;
    std::int32_t x = 0, y = 0;
    std::uint8_t rotation = 0;
    std::uint8_t byte0f = 0;
    /// The market KONTID to store (Building::kontor_id): a loaded record's, or the Kontor counter's
    /// old value when the founding path builds the warehouse. 0 = none.
    std::uint32_t kontor_id = 0;
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

/// The payload alternative a building of `type` must hold: the class's PayloadTag, except that a
/// Quarry-class type with the Inert pairing of the class table holds the Quarry payload (the claim
/// set, docs/sim/quarry-bricks.md Q3.1), which leaves the class table and the registry digest
/// unchanged.
[[nodiscard]] PayloadTag payload_tag_of(const BuildingType& type, const ClassInfo& cls);

/// The runtime payload of a new building of `type` (§7.3 step 2: every bit zero,
/// F2): the make_* constructor of its class, sized from the type and the config
/// (the small farm's counters come from SimConfig::small_farm_animals), Inert for
/// a class without a ported tick. Mine::deep is set (step 3), the deposit search
/// and the placement stamp are create_building's. A house holds one stock entry per
/// need of its group (house_need_count).
[[nodiscard]] Payload make_payload(const BuildingType& type, const ClassInfo& cls,
                                   const SimConfig& cfg, const TypeRegistry& types);

/// The number of needs of the group a house type belongs to: the group is
/// house_group(cfg, type.prodtype) and the needs come from the registry's group
/// table; 0 for a type that is no house, a registry without a group table and a
/// group the table does not hold.
[[nodiscard]] std::size_t house_need_count(const BuildingType& type, const SimConfig& cfg,
                                           const TypeRegistry& types);

/// The structural facts a payload must have for its type (§9.6 "payload matches
/// the type"): the alternative named by ClassInfo::payload, the vector sizes
/// the step functions index by (as make_payload sizes them), a Mine type with an
/// output. Empty when they hold, else one message ("holds Farm, the class
/// Workshop needs Workshop"). `derived` fields (WeaponsWorkshop::unlocked) are
/// not checked.
[[nodiscard]] std::string payload_problem(const Payload& payload, const BuildingType& type,
                                          const ClassInfo& cls, const SimConfig& cfg,
                                          const TypeRegistry& types);

/// The footprint a mine searches from (D 0x1006ddf0): position and rotated size.
[[nodiscard]] MineArea mine_area(const Building& b, const BuildingType& type);

} // namespace anno::sim
