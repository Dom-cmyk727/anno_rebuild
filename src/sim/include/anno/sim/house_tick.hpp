// The live House update (MAP-POP-LIVE-1 L4; docs/sim/population-live.md sections 1, 6, 7, 12, 19):
// the wiring of the kernel (house_kernel.hpp), the spawn block (resident_job.hpp) and the effect
// handlers (house_transition.hpp) into the building scheduler, and the city's stage pass. Only
// with Rules::house_updates (default false: the dispatch stays a no-op and every accepted
// baseline is unchanged).
//
// Serial: both entry points write the player table and the figure registry directly through
// TickContext::serial, in the visit order of the building scheduler, so the PRNG draws and the
// player-mask changes interleave with the other buildings exactly as the original's single thread
// does. The island phase of World::advance runs the islands one after the other; a parallel island
// phase would have to move this path into the merge phase first (docs/sim/population-live.md
// section 19).
#pragma once

#include <string>

#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// D 0x10029610: the stage pass gate, u16(t - city.stage_stamp) >= 2500. Original default; the
/// configurable path is a SimConfig field (not added in this task: the schema stays at stream 128).
inline constexpr std::uint16_t kStagePassPeriod = 2500;
/// D 0x10029610: the player-mask rule compares only the low 23 bits of the player's mask with the
/// city mask (the 23-bit quirk, kept, X16).
inline constexpr std::uint32_t kStageCompareMask = 0x7fffff;

/// The taken-or-skipped House tick of building `b` (a House of class House with the payload
/// House). Skipped gate: nothing changes. Otherwise, in the original's order: the kernel's gate,
/// E', need loop and mood, the spawn block (ONE world draw at DrawSite::HouseWalkerTarget when the
/// condition holds, before the send and before any variant draw), then apply_house_effects
/// (commit of last/E/stock/residents, cap or floor, UpgradeCheck/Upgrade, Downgrade, Remove). A
/// typed fault goes to `fx.faults` (the world stops at the merge). `b` may be gone on return.
void house_tick(const TickContext& ctx, Island& island, City& city, Building& b,
                const BuildingType& type, Effects& fx);

/// The city's stage pass (D 0x10029610), after the building ticks of the city update: gate, stamp
/// (also on the skip path of the stage loop), the stage bits and the player-mask rule. Does
/// nothing with Rules::house_updates off.
/// `island` only names the city in the diagnostic records (PopulationStats); it changes nothing.
void stage_pass(const TickContext& ctx, City& city, Effects& fx, IslandId island = IslandId{});

/// Empty when the registry fits the live House update (P-2, section 12): the House class gate
/// period is in 1..65535, every house group has a need, group 0 sets NoDowngrade, and every House
/// type names a group of the table. Names the class, the group or the type. Always empty with
/// Rules::house_updates off.
[[nodiscard]] std::string house_updates_problem(const SimConfig& cfg, const Rules& rules,
                                                const TypeRegistry& types);

/// The text of a fault record (typed names and ids, no pointers).
[[nodiscard]] std::string fault_message(const FaultRecord& fault);

} // namespace anno::sim
