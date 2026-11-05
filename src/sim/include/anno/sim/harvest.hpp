// The harvest job (jobs 0x29/0x2a of a farm-family home) as a default body of the serial figure
// phase (MAP-ECON-1 E3; contract docs/sim/forestry.md 3.2, 3.5; evidence docs/re/forestry.md).
//
// One call works through u16(t - fig.stamp) elapsed ticks: the saved first micro-step (5 ticks,
// consumed once across visits), then step A (search the home's square for an unclaimed tile whose
// object's WARE[0] is the cargo goods, walk search on the figure's layer, claim the first goal
// popped, walk there), step B (the type's load wait, then clear the claim and cut the tile:
// `id -> id - 1`, phase `& 0x80`, +256 cargo on success; a capacity above the cargo restarts at
// step A), step C (walk home, hand the cargo to the home's raw store). The body reads the world
// through the SerialWorld and a per-call TileOverlay, so later transitions of the same call see
// its own earlier claim/cut writes; it writes only its own figure and its Effects (tile_edits in
// issue order, the delivery as StoreDelta::Input), merged right after the call (C2). It draws
// nothing.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/figure.hpp>
#include <anno/sim/tick.hpp>

namespace anno::sim {

/// The cargo a successful cut credits: the cut arrival's `cargo.slot+0xc(0x100)` (D AnnoWelt
/// 0x10054530), 1 t in 8.8. An original constant of the step, not a limit.
inline constexpr std::uint32_t kHarvestCredit88 = 0x100;

/// One call of the harvest body on `fig` (a figure with `harvest`). Returns false, changing
/// nothing, for a figure whose type the registry does not know (no speed, capacity, layer or
/// wait); the caller keeps the default behaviour then.
bool harvest_tick(const TickContext& ctx, SerialWorld& world, Figure& fig);

/// Whether the tile object type `id` passes the harvest candidate predicate for `goods`: the type
/// exists and its WARE[0] is `goods` (AnnoWelt 0x10092320 mode 0), and, when `skip_uncuttable`,
/// the cut's own predicate holds (`harvest_cuttable`).
[[nodiscard]] bool harvest_candidate_type(const TypeRegistry& types, std::uint32_t id,
                                          GoodsId goods, bool skip_uncuttable);
/// The step-A callback (D 0x10058a40) as a pure function: does the search go on after an unclaimed
/// goal of this score? (`limit > score`, the unsigned 16-bit score at node+8 = the open list's ordering
/// score, D 0x10004918.) A claimed tile never stops it and is never taken.
[[nodiscard]] constexpr bool harvest_search_goes_on(std::uint16_t limit, std::uint16_t score) {
    return limit > score;
}
/// A goal node of the search in pop order, for the pure form of the choice below.
struct HarvestGoalNode {
    std::int32_t x = 0, y = 0;
    std::uint16_t score = 0;
    bool claimed = false;
};
/// The tile the callback ends with: the last unclaimed goal taken, the search stopping at the first one
/// with `limit <= score`. Nullptr-like (nullopt) when every goal is claimed.
[[nodiscard]] std::optional<TileXY> harvest_pick_tile(std::uint16_t limit,
                                                      const std::vector<HarvestGoalNode>& nodes);

/// The cut's predicate (AnnoWelt 0x10087340): `id != 0` and the type `id - 1` exists with
/// PRODTYPE 226 (ROHSTWACHS).
[[nodiscard]] bool harvest_cuttable(const TypeRegistry& types, std::uint32_t id);

} // namespace anno::sim
