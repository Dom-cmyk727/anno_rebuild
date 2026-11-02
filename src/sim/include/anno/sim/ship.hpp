// Ship motion state helpers (task MAP-NAV-2 package B; contract docs/sim/ship-navigation.md 12).
// This unit owns the construction and the validation of the persisted NavMotion of a navigating
// ship. The movement rules themselves (stepper, planner, commands) come in later packages.
#pragma once

#include <cstdint>
#include <string>

#include <anno/sim/config.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The turn speed and the idle/stop speed of the original, 0.01f, as q32: f32(0.01) * 2^32 is the
/// integer 42949672 (AnnoWelt 0x3c23d70a at 0x10043383; docs/sim/ship-navigation.md 12.4).
inline constexpr std::uint32_t kNavIdleSpeedQ32 = 42949672U;

/// One lattice length unit and the diagonal step length in q32 (L_d = 11863283 * 2^9, above 2^32).
inline constexpr std::uint64_t kNavUnitQ32 = std::uint64_t{1} << 32;
/// The scale of lengths and positions inside the integrator: q32 * 500 (one tick is 1/5 stepper
/// unit and V is a percent, so no division is ever needed; 12.4).
inline constexpr std::uint64_t kNavScale = 500;

/// A movement node longer than this is cut into a 1.0 chunk and the rest (AnnoWelt 0x1003e930:
/// `1.0 + f32(1e-4) < node.length`). f32(1e-4) has the mantissa 0xD1B717 and exponent -14, i.e.
/// 13743895 / 2^37, so the sum is 2^32 + 429496.72 in q32 and the integer test is
/// `length_q32 > 2^32 + 429496`. A segment may therefore be installed whole with up to this much
/// more than 1.0 remaining.
inline constexpr std::uint64_t kNavChunkLimitQ32 = (std::uint64_t{1} << 32) + 429496;

inline constexpr std::uint64_t kNavDiagStepQ32 = std::uint64_t{11863283} << 9;

/// The most pending nodes a ship may hold. A route of the supported window has far fewer (8 turn
/// nodes + runs of at most run_cap steps over a window of window_radius_tiles); the bound keeps a
/// hostile or corrupt save from allocating without limit. Path to make it configurable: derive it
/// from NavRules (2 * window_radius_tiles + 8). Routes are produced since package C; the planner
/// reports a route above this bound as BudgetExceeded, so the constant is only a corrupt-save
/// allocation bound.
inline constexpr std::size_t kNavMaxNodes = 1U << 16;

/// The initial motion state of a navigating ship at `pos` (Q24.8): idle (the original's
/// `remaining = speed = 0.01`, AnnoWelt 0x10040000), factor 100, no order, `last` = `tick_low`.
[[nodiscard]] NavMotion make_nav_motion(FixedPos pos, std::uint16_t tick_low);

/// Places a CARRIED navigating figure at `pos` (the carrier wrote its position): the fine position
/// becomes the Q24.8 position exactly and the residue is cleared, so Figure::pos stays equal to
/// floor(pos_u). The only way a position reaches a figure that does not sail on its own.
void nav_place(Figure& f, FixedPos pos);

/// Empty when the navigation state of every figure is consistent with its type and the counters:
/// present exactly for navigating types, nav_class below the Rules table, segment and node ranges,
/// `Figure::pos` equal to the floor of pos_u, residue rules, orders and tickets below the counters
/// and unique. Needs the registry (types) and the rules.
[[nodiscard]] std::string validate_navigation(const FigureRegistry& registry, const IdCounters& ids,
                                              const TypeRegistry& types, const Rules& rules);

} // namespace anno::sim
