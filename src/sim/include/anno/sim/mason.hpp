// The stonemason's worker job (job 0x2c, ROUTETYPE STEINMETZ) and the quarry as a stock source
// (MAP-EXTRACT-2 part Q; contract docs/sim/quarry-bricks.md Q1.2, F1 to F4, the review corrections
// and the DECIDED Q-A claim set; Q3.1/Q3.2 for the delta).
//
// One call works through u16(t - fig.stamp) elapsed ticks like the harvest job it shares its state
// with: the saved first micro-step, step A (collect the mask-8 candidates in the home's square,
// register the goal points of every unclaimed entrance, run the walk search; the FIRST qualifying
// goal popped stops it; claim that entrance), the walk, step B (the wait of the figure type, then
// clear the claim and add 1 t to the cargo once if a building stands at the stored tile), step C
// (walk home, credit the home's raw store unconditionally). It draws nothing.
#pragma once

#include <cstdint>
#include <optional>

#include <anno/sim/figure.hpp>
#include <anno/sim/tick.hpp>

namespace anno::sim {

/// The BuildingIndex list mask of the mason's collector query (D `push 8`, 0x10059ccf).
inline constexpr std::uint32_t kMasonMask = 0x8;

/// What a quarry answers for the stock of any goods (D 0x100708a0 `mov eax, 0x100`): 1 t, forever.
inline constexpr std::uint32_t kQuarryStock88 = 0x100;

/// The cargo one trip adds (D 0x10055462 `cargo.slot+0xc(0x100)`), 1 t, once per trip.
inline constexpr std::uint32_t kMasonTripCredit88 = 0x100;

/// The home's need gate (D 0x10072870, the base priority): steps of stock = raw / input per step;
/// below 30 -> 0x80, below 60 -> 0x60, else no candidate (nullopt; also nullopt for a need of 0).
[[nodiscard]] std::optional<std::int32_t> mason_priority(std::uint32_t raw, std::uint32_t need);

/// The candidate score (D 0x100728f0): `priority + stock * 3 * 64 / capacity`, capacity = the
/// CANDIDATE's output-0 capacity (review P2). nullopt for a capacity of 0 (the original's idiv
/// would trap; the candidate is skipped).
[[nodiscard]] std::optional<std::int64_t>
mason_candidate_score(std::int32_t priority, std::int64_t stock, std::int64_t capacity);

/// One call of the mason body on `fig` (a job-0x2c figure with `harvest`). Returns false, changing
/// nothing, for a figure whose type the registry does not know.
bool mason_tick(const TickContext& ctx, SerialWorld& world, Figure& fig);

} // namespace anno::sim
