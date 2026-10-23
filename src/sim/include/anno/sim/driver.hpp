// The driver: calling the production step functions from the building scheduler
// (spec: docs/sim/state.md §7.1, §7.2; stage S7 drove the workshop and the farm, stage
// S11 the weapons workshop, the fur trapper, the mine and the small farm).
//
// tick_building is what the original's building slot +0x8c does for each non-null
// slot the scheduler reaches: it refreshes the inputs the step derives from other
// state (the status icons), calls the step function of the building's class and
// consumes its StepResult. The step functions themselves are production.hpp's and
// are unchanged; this file is the only place that knows which payload alternative a
// class uses and how a StepResult becomes counters, spawn requests and UI events.
#pragma once

#include <cstdint>

#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// The research input of a weapons workshop (docs/sim/state.md §7.1; D: AnnoWelt.dll
/// 0x10069580-0x10069640): sizes `w.unlocked` to the lines of the payload (one per
/// entry of `w.out`) and sets line j when the Forsch research id of its output goods
/// (`type.output_goods[j]`, GoodsInfo::forsch, OQ-034) is 0 or researched by `owner`
/// (PlayersView::researched: bit 1 of the research state byte). A player that is not
/// there has researched nothing. `unlocked` is derived and not saved, so it is empty
/// after a load; tick_building calls this before every weapons_workshop_step.
void refresh_unlocked(WeaponsWorkshop& w, const BuildingType& type, const TypeRegistry& types,
                      const PlayersView& players, PlayerId owner);

/// The counter of figures out of `home_counter` of a payload: Workshop and
/// WeaponsWorkshop `carriers_out`, Farm `workers_out` (counter 0 each), SmallFarm
/// `figures_out[home_counter]`; nullptr for a counter the payload does not have
/// and for the payloads without counters (Mine, Inert).
[[nodiscard]] std::uint32_t* figure_counter(Payload& payload, std::uint32_t home_counter);
[[nodiscard]] const std::uint32_t* figure_counter(const Payload& payload,
                                                  std::uint32_t home_counter);
/// How many counters a payload has: 1 for Workshop, WeaponsWorkshop and Farm, the
/// size of `figures_out` for SmallFarm, 0 for Mine and Inert.
[[nodiscard]] std::uint32_t figure_counter_count(const Payload& payload);

/// The output store `out_index` of a payload in 8.8 tons (Workshop, WeaponsWorkshop
/// and SmallFarm `out[out_index]`, Farm and Mine `out` for index 0); nullptr beyond
/// the stores and for Inert.
[[nodiscard]] std::uint32_t* output_store(Payload& payload, std::uint32_t out_index);

/// The player-visible blocked flag of any payload.
[[nodiscard]] bool payload_blocked(const Payload& payload);

/// The building with this id for a writer: the island's id map, checked against the
/// list slot like Island::find_building; nullptr when it is gone or the id names
/// another island.
[[nodiscard]] Building* find_building_mut(Island& island, BuildingId id);

/// Where a figure sent by `b` starts: the building's north-west tile in Q24.8. A
/// STAND-IN (the original's start position is a float from the unit constructor,
/// O-47, and figures do not move before stage S12).
[[nodiscard]] FixedPos spawn_position(const Building& b);

/// One building's tick (D: building slot +0x8c). The type, the class and the owner
/// come from the context; the step function of the class runs on the payload, then
/// `consume` turns its result into counters, spawn requests and UI events. Per tick
/// kind (docs/sim/state.md §7.1):
///  - Workshop, Farm: workshop_step, farm_step;
///  - WeaponsWorkshop: refresh_unlocked (the owner's research), then
///    weapons_workshop_step;
///  - FurTrapper: fur_trapper_step on the Farm payload; a due trapper is not sent,
///    because the prey search (D 0x10071da0) finds no animal: the sim has no wildlife
///    objects (O-12, O-25);
///  - Mine: mine_step over the island's deposits, the footprint by rotation
///    (mine_area) and the world's depletion setting (WorldSettings::depletes, the
///    add-on edition);
///  - SmallFarm: small_farm_step with a DrawRef over the context's DrawSource
///    (DrawSite::SmallFarmAnimal) and SimConfig::small_farm_animals.
/// Returns without effect when the type is unknown, the payload is not the class's
/// alternative (check_invariants and the loader rule both out: the release-mode
/// behaviour of a corrupt copy is "do nothing", never an exception), or the class has
/// no ported tick (TickKind::None: inert, `last` untouched, D15).
void tick_building(const TickContext& ctx, Island& island, City& city, Building& b, Effects& fx);

/// The consequences of one step (docs/sim/state.md §7.2), in this order:
///  - blocked_changed: UI BlockedChanged {owner, building, 1 or 0, x, y} (the original
///    notifies island slot +0x168; nothing in the sim reads it, O-26);
///  - message: UI StatusMessage {owner, building, status};
///  - notice != 0: UI Notice {owner, building, notice} (mines: 0x19 / 0x15b);
///  - made: nothing (the output stays in the building's store; carriers that move it
///    to the city stock arrive with transport);
///  - send_figure: the home counter is raised HERE, in the tick, and a SpawnRequest
///    {kind = the type's first figure id, job (0x0d for the fur trapper, 0x2a for
///    the small farm's line-0 worker, else 0 = unknown), home_counter 0} is queued;
///    the figure itself is created by the merge;
///  - send_animal (small farm): counter animal_kind - small_farm_animals.base is
///    raised and a SpawnRequest {kind = animal_kind, job 0x2e, home_counter = that
///    index} is queued; a kind outside the counters sends nothing (F21).
/// The raise-in-tick / spawn-in-merge split keeps the counter and the registry
/// consistent (check_invariants asserts equality).
void consume(const TickContext& ctx, const Island& island, const City& city, Building& b,
             const BuildingType& type, const StepResult& r, Effects& fx);

} // namespace anno::sim
