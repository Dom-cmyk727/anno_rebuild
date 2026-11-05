// The House level transitions and removal (MAP-POP-LIVE-1 L3; docs/sim/population-live.md sections
// 1 steps 5-6, 5, 6, 7, 12, 16). Separately testable production-path operations: UpgradeCheck
// (AnnoWelt 0x10075dd0), Upgrade (0x1002cbe0), Downgrade (0x1002cdd0), SetGroup (0x10076b90) and
// the removal (island +0xcc -> 0x10028fd0 -> 0x100255c0), and apply_house_effects, which maps the
// kernel's result (house_kernel.hpp) to them in the original order. NOT wired into the tick (L4):
// the House dispatch of the driver is unchanged and Rules::house_updates stays false.
//
// Integer only; the only draws are the variant draw (DrawSite::HouseVariant, BEFORE SetGroup) and
// the first-assignment draws (DrawSite::HouseFirstAssignment, one per forced need in need order). A
// fault is typed (HouseFault) and happens BEFORE anything is spent, drawn or changed; an operation
// either completes or changes nothing.
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/house_kernel.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

struct HouseWork; // transport.hpp (diagnostic counters, not sim state)

/// Why a transition could not be performed (population-live.md section 7, P-1/P-3: crafted or
/// modded data fails loudly, never as "not eligible").
enum class HouseFault : std::uint8_t {
    None,
    NoSuchHouse,       ///< the key names no building, or a building that is no House
    UnknownType,       ///< the house's type is not in the registry
    MissingGroup,      ///< a type's house group is not in the group table (or has no first need)
    StockSizeMismatch, ///< the house stock does not have one entry per need of its group
    ZeroDivisor,       ///< E == 0 or a Menge of 0 where a satisfaction is computed
    MissingSlot,       ///< a forced need of the next group has no slot in the current group
    FootprintMismatch, ///< the new type's footprint differs from the old one (section 16)
    MissingVariant,    ///< a variant type of the new type does not exist or has another group
    IdSecCycle,        ///< an IdSec chain does not end within goods_count steps (P-3)
    Overflow,          ///< a result does not fit the widened field
    NotEvaluated,      ///< the kernel result was NotEvaluated (apply_house_effects only)
    NoSerialContext,   ///< the live update ran without TickContext::serial (L4)
    GroupNotAdjacent,  ///< the new type's group is not old group +1 (upgrade) / -1 (downgrade)
    CountMismatch,     ///< the city's house count of the old group is zero (a broken derived index)
    BadSuccessor,      ///< the ladder successor exists, is no regular successor and is not declared
};
[[nodiscard]] const char* house_fault_name(HouseFault fault);

/// SetGroup's argument is the ADJACENT group record: cur + 1 on upgrade, cur - 1 on downgrade (D
/// 0x1002cd0e..0x1002cd77: cur + 0x118; 0x1002ceb1..0x1002cf06: cur - 0x118).
[[nodiscard]] constexpr bool house_group_adjacent(std::uint32_t old_group, std::uint32_t new_group,
                                                  bool upgrade) {
    return upgrade ? new_group == old_group + 1 : old_group >= 1 && new_group == old_group - 1;
}

/// The regular successor of a House type (MAP-POP-CLOSE-1 C3, the P-14 replacement): the ladder's
/// next type is a House of the next group with the same footprint. Pure on the two types.
[[nodiscard]] bool house_successor_is_regular(const SimConfig& cfg, const BuildingType& cur,
                                              const BuildingType& next);

/// What UpgradeCheck makes of a House type's ladder successor (D 0x1002e000 only needs the next
/// type to exist and its materials to be in store; it never checks its kind or group -
/// population.md 17(c)). Ladder: a regular successor. EndOfLadder: no next type exists (the
/// original returns 0). Special: the successor exists but is irregular and the type is declared
/// (BuildingType:: special_successor); the engine does not replay the original's unchecked type
/// swap, it is a named not-eligible outcome. Malformed: irregular and undeclared - a content error,
/// never "not eligible".
enum class HouseSuccessor : std::uint8_t { Ladder, EndOfLadder, Special, Malformed };
[[nodiscard]] HouseSuccessor house_successor(const SimConfig& cfg, const TypeRegistry& types,
                                             const BuildingType& type);

/// The type ladder: `id - id % stride + stride` and `id - id % stride - stride`, else `+ 1`
/// (D: AnnoWelt 0x1002cc22 `div 0x14`; 0x1002ce21-0x1002ce3b). nullptr = no such type.
struct TypeLadder {
    const BuildingType* next = nullptr;
    const BuildingType* previous = nullptr;
};
[[nodiscard]] TypeLadder house_type_ladder(const SimConfig& cfg, const TypeRegistry& types,
                                           const BuildingType& type);

/// Property 0x32 (D: AnnoGame 0x1000a860 reads type +0x7c / +0x7d, written at 0x10009477 /
/// 0x10009482 from HEAD byte 11): count = HEAD[11] & 0x3f, step = HEAD[11] >> 6.
struct VariantFacts {
    std::uint32_t count = 0;
    std::uint32_t step = 0;
};
[[nodiscard]] VariantFacts house_variant_facts(const BuildingType& type);

/// idx = r mod count (unsigned); the variant type id is `base id + idx * step`.
[[nodiscard]] inline std::uint32_t house_variant_index(const VariantFacts& facts, std::uint16_t r) {
    return facts.count == 0 ? 0 : static_cast<std::uint32_t>(r % facts.count);
}

/// The build-material amounts (8.8 tons) of `base` per build-goods slot k
/// (SimConfig::build_goods_*): the entries of BuildingType::cost_goods; every other slot is 0. Size
/// build_goods_count.
[[nodiscard]] std::vector<std::uint32_t> house_material_amounts(const SimConfig& cfg,
                                                                const BuildingType& base);
/// D 0x1002c030: every non-zero amount_k must satisfy amount_k <= store[g] - build_goods[k].setting
/// (signed 64-bit; `setting` is the contract's reserve). The cumulative `spent` is never read.
[[nodiscard]] bool house_materials_available(const SimConfig& cfg, const City& city,
                                             std::span<const std::uint32_t> amounts);

/// Upgrade's spend (after a successful gate): spent[k] += amount_k, store[g] -= min(amount_k,
/// store[g]).
void house_spend_materials(const SimConfig& cfg, City& city,
                           std::span<const std::uint32_t> amounts);

/// UpgradeCheck's score part (D 0x10075dd0; section 5). `cur` is the house's group, `next` the next
/// group (both from TypeRegistry::population_view), `e` and `stock` the house's.
struct UpgradeScores {
    bool eligible = false;
    HouseFault fault = HouseFault::None;
    std::int64_t food = 0, mean = 0, infra = 0; ///< the three scores (cap 128; infra starts 160)
    std::int64_t f = 0, b = 0, i = 0;           ///< the three table entries
};
[[nodiscard]] UpgradeScores upgrade_scores(const PopulationView& cur, const PopulationView& next,
                                           std::uint32_t e, std::span<const std::uint32_t> stock);

/// SetGroup (D 0x10076b90) as a function of the old group (null = first assignment), the new group,
/// E, residents * 64 and the old stock. `draw` is used only by a first assignment (one draw per
/// forced need, DrawSite::HouseFirstAssignment). On a fault nothing was drawn: a first assignment
/// checks its supported domain (E, residents and every worst-case stock fit) before the first draw
/// (MAP-POP-CLOSE-1 G3).
struct SetGroupResult {
    HouseFault fault = HouseFault::None;
    std::vector<std::uint32_t> stock;
    std::uint32_t e = 0;
    std::int32_t residents_64 = 0;
    std::uint32_t draws = 0;
};
/// The supported domain of a FIRST assignment into `new_group` (MAP-POP-CLOSE-1 G3/G1): None, or
/// Overflow when E = Wohnmax << 7, residents_64 = Wohnmax << 6 or a worst-case stock (sat = 128 for
/// every slot whose ware a forced need below infra_end draws, and E * Menge / 12 for slot 0) would
/// leave its width. Group number 0 never draws and keeps the constructor numbers: always None.
/// Pure and independent of the draws, so a caller can refuse before drawing, charging or taking an
/// id. house_set_group calls this very function.
[[nodiscard]] HouseFault house_first_assignment_domain(const PopulationView& new_group,
                                                       std::size_t goods_count);

[[nodiscard]] SetGroupResult house_set_group(DrawSource& draw, std::span<const GoodsInfo> goods,
                                             const PopulationView* old_group,
                                             const PopulationView& new_group, std::uint32_t e,
                                             std::int32_t residents_64,
                                             std::span<const std::uint32_t> stock);

/// The resident counter of `group` and all higher groups, derived from the houses of `city`
/// (population-live.md section 6, OQ-165): the sum of trunc(residents_64 / 64). F-3: the original's
/// incremental counter differs in the below-64 correction (P-5); this is the corrected value.
[[nodiscard]] std::int64_t residents_at_or_above(const SimConfig& cfg, const TypeRegistry& types,
                                                 const City& city, std::uint32_t group);

/// The same counters for every group at once, one pass over the city's buildings: entry g is
/// residents_at_or_above(g) (size = the registry's group count). The stage pass uses it so its
/// cost does not grow with the number of stage records.
[[nodiscard]] std::vector<std::int64_t>
residents_at_or_above_all(const SimConfig& cfg, const TypeRegistry& types, const City& city);

/// What the operations need besides the island and the key.
struct HouseEnv {
    const SimConfig& cfg;
    const Rules& rules;
    const TypeRegistry& types;
    PlayerTable& players;
    DrawSource& draw;
    FigureRegistry& figures;
    std::uint32_t t = 0; ///< the world tick (the expiry stamp of an ended walker)
    /// Diagnostic only (MAP-POP-LIVE-1 L5): the house's operation counters, or null.
    HouseWork* work = nullptr;
};

enum class HouseOutcomeKind : std::uint8_t {
    NoChange, ///< nothing to do (no effect, no next/previous type, not eligible)
    Upgraded,
    UpgradeRefusedMaterials, ///< eligible, but the materials gate failed: nothing spent or drawn
    Downgraded,
    Removed,
    Fault,
    NotEligibleTerminal, ///< C3: no successor type exists (end of the ladder): named, not a fault
    NotEligibleSpecial,  ///< C3: a declared special successor: named, not a fault
};

struct HouseOutcome {
    HouseOutcomeKind kind = HouseOutcomeKind::NoChange;
    HouseFault fault = HouseFault::None;
    NotEvaluatedReason not_evaluated = NotEvaluatedReason::None;
    TypeId from, to;
    std::uint32_t old_group = 0, new_group = 0;
    /// The counter operations in the original's order: sub(old_group, counter_sub) BEFORE the swap,
    /// add(new_group, counter_add) AFTER SetGroup (residents read after SetGroup).
    std::int64_t counter_sub = 0, counter_add = 0;
    bool variant_drawn = false;
    std::uint16_t variant_draw = 0;
    std::uint32_t variant_index = 0;
    std::uint32_t first_assignment_draws = 0;
    std::uint32_t walkers_detached = 0; ///< removal: walkers whose home became none (not ended)
    UpgradeScores scores;               ///< filled by apply_house_effects' UpgradeCheck arm
    bool check_ran = false;             ///< K3 diagnostics: the check read check_e / check_stock
    std::uint32_t check_e = 0;
    std::vector<std::uint32_t> check_stock;
};

/// Upgrade (0x1002cbe0, immediate). Materials gate first (a failure changes nothing), then in the
/// original's order: spend, variant draw, counter sub, house count, type swap, SetGroup, counter
/// add, house count, footprint re-stamp (0x1002c790 -> 0x1008ec90 -> 0x10082040, section 16).
[[nodiscard]] HouseOutcome upgrade_house(const HouseEnv& env, Island& island,
                                         const BuildingKey& key);
/// Downgrade (0x1002cdd0): no materials; the variant draw of the previous type before SetGroup.
[[nodiscard]] HouseOutcome downgrade_house(const HouseEnv& env, Island& island,
                                           const BuildingKey& key);
/// Removal: the house's walkers end through the standard figure end path (home none, expiry list,
/// the walker slot released once), then remove_building (derived counters, footprint, tiles).
[[nodiscard]] HouseOutcome remove_house(const HouseEnv& env, Island& island,
                                        const BuildingKey& key);

/// Steps 5-6 of the House tick (population-live.md section 1) for an evaluated kernel result:
/// commits the kernel's last, E, stock and residents (a NotEvaluated result is a typed Fault, a
/// Skipped one NoChange), then applies the deferred effect: UpgradeCheck -> scores (a table entry
/// <= 0 is NoChange) -> next type and materials -> upgrade_house; Downgrade -> downgrade_house;
/// Remove -> remove_house. The below-64 correction exists only on the Downgrade arm (it is in
/// `kernel.residents_64`). Spawn (step 3) is the caller's.
[[nodiscard]] HouseOutcome apply_house_effects(const HouseEnv& env, Island& island,
                                               const BuildingKey& key,
                                               const HouseKernelResult& kernel);

} // namespace anno::sim
