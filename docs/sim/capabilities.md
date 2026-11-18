# Current simulation capabilities

Source snapshot: accepted SIM-LIMITS-1 candidate `2f9e4c0b` (2026-11-18), not a
claim that this stack is main. This is an inventory of code and its current
production paths, not original-game parity or a balanced playable-town verdict.

A scenario can load supported terrain, cities, buildings and actors; commands can
found, build, trade and sail with explicit refusals. Workshop/farm/mine production,
transport, freight assignments and opt-in living houses/accounting exist. Wildlife
creation, hunter/whaler production, strategic AI and military combat are absent or
unported here. Script compilation is deliberately partial and can prevent an
otherwise supported map from starting. A seam installed by a test does not supply
production gameplay.

## Scope, defaults and proof

The four inventories cover every direct Rules member and its owned nested leaves,
every Body alternative/wire mapping, every RuntimeClass row, all Seams members and
the named calls in Scheduler::advance. SimConfig, WorldSettings, every internal
helper/TODO and original data tables are outside this finite declaration inventory.
Shared NavClassCost is described once at the class_costs[] path. Aggregate rows
remain present as well as their leaves; `{}` means implicit member initialization,
not a separately evaluated default.

Rules{} declared defaults are in the table. They differ from the explicit Living
profile: [apply_rules_profile](../../src/app/src/cli.cpp) enables house_updates and
city_booking only. [main](../../src/app/src/main.cpp) applies the profile then the
explicit booking override; [opening setup](../../src/app/src/opening.cpp) also
selects the profile and loads city_names from the original text table; its explicit booking override follows the profile. The scenario link loader receives the chosen Rules and reads island_candidate_order before creating the world ([resolve_scenario_links](../../src/sim/src/scenario_formats.cpp)). No enhanced proposal in [rules-presets](rules-presets.md) is
turned into policy by this document. The fisher_rules enable is a separate header
extension, not a Rules member: [World::fisher_enabled](../../src/sim/include/anno/sim/world.hpp)
reads it; [CLI](../../src/app/src/cli.cpp) accepts the explicit switch.

Every Rules field is hashed/saved/loaded through
[fields(Rules), fields(NavRules/CargoRules/FreightRules)](../../src/sim/include/anno/sim/state_fields.hpp);
[Rules::validate](../../src/sim/src/config.cpp) and CargoRules::problem reject the
constraints below. [house_updates_problem](../../src/sim/src/house_tick.cpp#L350)
at create/load also requires persisted bills for live houses. Other world/command
invariants remain additional checks.
The named constant expression, rather than an unchecked resolved numeral, is shown
when applicable. Enum declarations are listed separately from validation policy.

Existing proof entry points (not rerun or broadened by this documentation):
[config tests](../../src/sim/tests/config_test.cpp),
[freight framing/load/command tests](../../src/sim/tests/freight_state_test.cpp),
[scheduler arithmetic tests](../../src/sim/tests/scheduler_blocks_test.cpp),
[driver class tests](../../src/sim/tests/driver_classes_test.cpp) (including
`fur trapper sends nothing without prey` and `weapons workshop unlocked from research`),
[fisher vectors/corpus](../../src/sim/tests/fisher_test.cpp), and
[script startup/continuation tests](../../src/sim/tests/script_runtime_test.cpp).
The accepted [SIM-LIMITS evidence](../orchestration/evidence/sim-limits-1.md) is
bounded compatibility proof. See [state](state.md), [production](production.md),
[population-live](population-live.md), [ship-trade-routes](ship-trade-routes.md) and
[core](core.md) for deeper contracts. Code existence, a reachable caller, a selected
rule/preset and demonstrated acceptance scope are distinct facts. No row establishes
live-window, original-runtime, target-host or balanced-economy acceptance.

## Mechanical check and semantic review

`python3 -I tools/sim_capabilities.py` checks the first three columns directly
against finite source blocks. `--root` supports a relocated checkout; `--dump`
prints source-derived JSON for an explicit human edit and never writes. Tests and
CI run read-only. Missing, extra, duplicate and drifted rows refuse. Missing/empty
required files, unsupported declarations, unmapped commands and incomplete class
tables refuse too. Comments are removed with string-aware tokenization.

The two final columns are human descriptions and source references: independently
reviewed, not mechanically proved. The payload override expression is a narrow
review trigger; it does not prove semantics. Call-name/first-seen-order/occurrence checking detects
changes to the declared scheduler surface, not conditional reachability, argument
meaning, control-flow equivalence or function-body correctness. No whole-file hash
is presented as semantic verification. The extractor does not preprocess C++,
resolve aliases, evaluate arbitrary expressions or follow arbitrary call graphs;
syntax changes in its named blocks need a deliberate extractor review. A new
behavior hidden inside an existing callee can leave this check green.

## 1. Rules and qualified owned members

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `rule:slot_policy` | `SlotPolicy` | `SlotPolicy :: Original` | enum choice; Original or LowestFree; active | [SlotList::insert](../../src/sim/include/anno/sim/slot_list.hpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:spatial_order` | `SpatialOrder` | `SpatialOrder :: YXSlot` | reserved/refused except YXSlot; canonical rect ordering | [Island::rect_query](../../src/sim/src/island.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:events_drain_new` | `bool` | `false` | switch; re-read event queue after each dispatch | [pop_due](../../src/sim/include/anno/sim/scheduler.hpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:events_max_dispatch` | `std::uint64_t` | `kEventsMaxDispatchDefault` | limit; at least 1; cap reports rather than hangs | [pop_due](../../src/sim/include/anno/sim/scheduler.hpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:events_flush_after_figures` | `bool` | `false` | switch; second event pass, default off | [Scheduler::advance](../../src/sim/src/scheduler.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:monopoly_every_30s` | `bool` | `false` | switch; periodic refresh rather than original accumulator behavior | [monopoly_refresh](../../src/sim/src/timers.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:player_table_full_tick` | `bool` | `false` | switch; full table visit rather than selected original visit | [player_table_tick](../../src/sim/src/player.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:drought_random_centre` | `bool` | `false` | reserved/refused when true; no implemented random-centre choice | [Rules::validate](../../src/sim/src/config.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:build_atomic` | `bool` | `false` | switch; transactional multi-placement rather than partial success | [build](../../src/sim/src/apply.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:keep_empty_city` | `bool` | `false` | switch; retain otherwise removable empty city; default seam alone never reports empty | [demolish](../../src/sim/src/apply.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:event750_keep_overshoot` | `bool` | `false` | switch; subtract interval rather than reset accumulator | [event750](../../src/sim/src/timers.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:prng_mode` | `PrngMode` | `PrngMode :: Global` | enum choice; Global only; PerIsland reserved/refused | [Rules::validate](../../src/sim/src/config.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:interim_figure_return_ticks` | `std::uint32_t` | `0` | limit/delay; 0 never returns generic interim body; installed figure seam bypasses it | [Scheduler::run_figures](../../src/sim/src/scheduler.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:free_build_cheat` | `bool` | `false` | switch; host-side free-build behavior, not a wire permission | [build](../../src/sim/src/apply.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:enforce_research` | `bool` | `false` | switch; optional additional type research_id check in apply; construction_eligibility scenario/stage/research permissions are independent of this flag | [check_record research gate](../../src/sim/src/apply.cpp#L435); [construction_eligibility](../../src/sim/src/build_availability.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:stat_decay_wide` | `bool` | `false` | switch; wide statistics product instead of u32 wrap | [player_tick](../../src/sim/src/player.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:bankrupt_deadline_wrap_safe` | `bool` | `false` | switch; signed wrap-safe bankruptcy deadline | [player_tick](../../src/sim/src/player.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:timer0_clamp` | `bool` | `false` | switch; clamp overshoot instead of original timer wrap | [timers_tick](../../src/sim/src/timers.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:island_candidate_order` | `IslandCandidateOrder` | `IslandCandidateOrder :: Sorted` | enum choice; Sorted only; Cached reserved/refused | [resolve_island_links](../../src/sim/src/island_links.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:removal_clear_tile` | `bool` | `false` | switch; clear actual occupied footprint tile; original removal default retained | [remove_building](../../src/sim/src/apply.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:founding_types` | `std::vector < std::uint32_t >` | `{ 1302 }` | data; ascending unique nonzero eligible founding building types | [Founding::apply](../../src/sim/src/founding.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:city_names` | `std::vector < std::string >` | `{}` | data; no NUL; empty means no name selection draw | [city creation](../../src/sim/src/city_creation.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:shared_visibility` | `std::vector < std::vector < std::uint32_t > >` | `{}` | data; sorted disjoint groups, at least two players each; world validates player-domain/peace | [shared visibility groups](../../src/sim/src/founding.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav` | `NavRules` | `{}` | owned NavRules aggregate; members below | [ShipOrders::move](../../src/sim/src/ship_orders.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.move_blocked_mask` | `std::uint32_t` | `0x200e` | data; state-word refusal mask; no separate range check | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.plans_per_advance` | `std::uint32_t` | `4` | limit; 1..65535 planning admissions | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.run_cap` | `std::uint32_t` | `10` | limit; 1..255 route run length | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.retry_cap` | `std::uint32_t` | `8` | limit; 1..64 learned-mask retries | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.goal_radius_tiles` | `std::uint32_t` | `6` | limit; 1..32 goal acceptance radius | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.unit_cap` | `std::uint32_t` | `64` | limit; 1..1024 obstacle stamps | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.window_radius_tiles` | `std::uint32_t` | `1024` | limit; 1..8192 search half-window | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.window_blocks` | `std::uint32_t` | `32` | limit; 1..256 coarse window blocks | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.coarse_budget` | `std::uint64_t` | `600000` | limit; 1..2^40 node pops, BudgetExceeded on exhaustion | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.fine_budget` | `std::uint64_t` | `4000000` | limit; 1..2^40 node pops, BudgetExceeded on exhaustion | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.memory_bytes` | `std::uint64_t` | `std :: uint64_t { 64 } << 20` | limit; 1..2^40 planning bytes | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.volk_number` | `std::uint32_t` | `1` | data; 0..65535 selects cargo speed penalty population | [navigation consumer](../../src/sim/src/ship_step.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.health_span_percent` | `std::uint32_t` | `35` | data; 0..100, span + floor <= 100 | [navigation consumer](../../src/sim/src/ship_step.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.health_floor_percent` | `std::uint32_t` | `65` | data; 1..100 and floor * (100 - penalty) >= 100 | [navigation consumer](../../src/sim/src/ship_step.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.cargo_penalty_percent` | `std::uint32_t` | `17` | data; 0..100 and positive minimum speed product | [navigation consumer](../../src/sim/src/ship_step.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.class_costs` | `std::vector < NavClassCost >` | `std :: vector < NavClassCost > ( 4 )` | data; 1..256 class records; shared NavClassCost definition expanded once as [] | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.class_costs[].open_orth` | `std::uint16_t` | `10` | data; positive orthogonal open-cell cost | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.class_costs[].open_diag` | `std::uint16_t` | `13` | data; at least open_orth | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.class_costs[].shore_orth` | `std::uint16_t` | `50` | data; positive orthogonal shore-cell cost | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:nav.class_costs[].shore_diag` | `std::uint16_t` | `69` | data; at least shore_orth | [navigation consumer](../../src/sim/src/ship_mover.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:cargo` | `CargoRules` | `{}` | owned CargoRules aggregate; members below | [ShipCargo](../../src/sim/src/ship_cargo.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:cargo.warehouse_prodtypes` | `std::vector < std::uint32_t >` | `{ 35 }` | data; ascending unique nonzero, empty forbids warehouses | [cargo operation](../../src/sim/src/ship_cargo.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:cargo.dock_half_widths` | `std::vector < std::uint32_t >` | `{ 5 , 5 , 5 , 4 , 3 , 2 }` | data; nonempty, first equals radius, nonincreasing, <= kMaxDockRadius + 1 | [cargo operation](../../src/sim/src/ship_cargo.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:road_require_land_level` | `bool` | `false` | switch; enhanced land-road minimum level, default off | [placement_rule](../../src/sim/src/placement.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:road_preserve_nonroad_pieces` | `bool` | `false` | switch; preserve compatible nonroad pieces, default off | [placement_rule](../../src/sim/src/placement.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:forestry_claim_hygiene` | `bool` | `false` | switch; owned claims/revalidation instead of original tile bit | [harvest_tick](../../src/sim/src/harvest.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:forestry_skip_uncuttable` | `bool` | `false` | switch; exclude targets failing cut predicate | [harvest search](../../src/sim/src/harvest.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:house_updates` | `bool` | `false` | rule-gated; live house/Inn ticks, residents and stage transitions; default off | [tick_building](../../src/sim/src/driver.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:save_billed_goods` | `bool` | `true` | persistence switch; house_updates requires true at create/load and city_booking requires true in Rules validation; no supported unsaved live-house mode | [house_updates_problem](../../src/sim/src/house_tick.cpp#L350); [fields(City)](../../src/sim/include/anno/sim/state_fields.hpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:city_booking` | `bool` | `false` | rule-gated; checked earned income/upkeep; requires save_billed_goods; default off | [city_tick](../../src/sim/src/island_tick.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:freight` | `FreightRules` | `{}` | owned FreightRules aggregate; automatic assignments require a command, not an enable switch | [FreightRoutes](../../src/sim/src/freight.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:freight.max_stops` | `std::uint32_t` | `1024` | limit; 1..wire max_stops, command also checks combined frame | [FreightRoutes](../../src/sim/src/freight.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:freight.max_actions` | `std::uint32_t` | `2048` | limit; 1..wire max_actions, command also checks combined frame | [FreightRoutes](../../src/sim/src/freight.cpp); [Rules::validate](../../src/sim/src/config.cpp) |
| `rule:freight.work_per_visit` | `std::uint32_t` | `64` | limit; positive deterministic per-visit work | [FreightRoutes](../../src/sim/src/freight.cpp); [Rules::validate](../../src/sim/src/config.cpp) |

## 1a. Referenced constants and enum choices

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `constant:CargoRules::kMaxDockRadius` | `std :: uint32_t` | `255` | fixed validation/default expression, not an additional Rules member | [Rules::validate](../../src/sim/src/config.cpp) |
| `constant:kEventsMaxDispatchDefault` | `std :: uint64_t` | `1 ULL << 20` | fixed validation/default expression, not an additional Rules member | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:SlotPolicy.Original` | `position 0; initializer implicit` | `declaration only; validation is reviewed` | supported original free-slot policy | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:SlotPolicy.LowestFree` | `position 1; initializer implicit` | `declaration only; validation is reviewed` | supported opt-in lowest free slot | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:SpatialOrder.YXSlot` | `position 0; initializer implicit` | `declaration only; validation is reviewed` | supported canonical query ordering | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:SpatialOrder.Original0x10027000` | `position 1; initializer implicit` | `declaration only; validation is reviewed` | reserved/refused: original unstable ordering not implemented | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:PrngMode.Global` | `position 0; initializer implicit` | `declaration only; validation is reviewed` | supported single world stream | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:PrngMode.PerIsland` | `position 1; initializer implicit` | `declaration only; validation is reviewed` | reserved/refused: no per-island stream state | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:IslandCandidateOrder.Sorted` | `position 0; initializer implicit` | `declaration only; validation is reviewed` | supported owner-selected template ordering | [Rules::validate](../../src/sim/src/config.cpp) |
| `enum:IslandCandidateOrder.Cached` | `position 1; initializer implicit` | `declaration only; validation is reviewed` | reserved/refused: no cache reader | [Rules::validate](../../src/sim/src/config.cpp) |

| `enum:RuntimeClass.Plain` | `position 0; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Workshop` | `position 1; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.DualWorkshop` | `position 2; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.WeaponsWorkshop` | `position 3; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Farm` | `position 4; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.FurTrapper` | `position 5; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Mine` | `position 6; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.SmallFarm` | `position 7; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Quarry` | `position 8; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Hunter` | `position 9; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Whaler` | `position 10; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Shipyard` | `position 11; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Cannonworks` | `position 12; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Market` | `position 13; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.MarketCombi` | `position 14; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.MarketStall` | `position 15; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Inn` | `position 16; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.PublicService` | `position 17; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Doctor` | `position 18; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.School` | `position 19; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Castle` | `position 20; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.FireBrigade` | `position 21; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.House` | `position 22; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Tower` | `position 23; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.CannonTower` | `position 24; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:RuntimeClass.Count_` | `position 25; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Inert` | `position 0; initializer 0` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Workshop` | `position 1; initializer 1` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.WeaponsWorkshop` | `position 2; initializer 2` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Farm` | `position 3; initializer 3` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.SmallFarm` | `position 4; initializer 4` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Mine` | `position 5; initializer 5` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Market` | `position 6; initializer 6` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.House` | `position 7; initializer 7` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Inn` | `position 8; initializer 8` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:PayloadTag.Quarry` | `position 9; initializer 9` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.None` | `position 0; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.Workshop` | `position 1; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.WeaponsWorkshop` | `position 2; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.Farm` | `position 3; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.FurTrapper` | `position 4; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.Mine` | `position 5; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.SmallFarm` | `position 6; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.Market` | `position 7; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.House` | `position 8; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |
| `enum:TickKind.Inn` | `position 9; initializer implicit` | `declaration only; validation is reviewed` | enum identity including explicit encoded initializer; Count_ is a sentinel, not a class | [types declarations](../../src/sim/include/anno/sim/types.hpp) |

## 2. Commands

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `command:BuildCmd` | `0` | `kWireBuild = 0x64` | active; build; bounds/availability/ownership/placement/payment and partial vs atomic rules | Build UI in play.cpp and opening_ui.cpp; command-file path |
| `command:DemolishCmd` | `1` | `kWireDemolish = 0x68` | active; demolish; owner/tile checks and removal hooks | viewer.cpp/dev_input.cpp; command-file path |
| `command:SetProductionLineCmd` | `2` | `kWireSetProductionLine = 0x74` | active; set_production_line; supports known production payloads, refuses unsupported | no production UI producer established in this inventory |
| `command:SetTradeFlagCmd` | `3` | `kWireSetTradeFlag = 0x7c` | active; set_trade_flag; supported mode/stock and ownership checks | no production UI producer established in this inventory |
| `command:SetTradeSlotCmd` | `4` | `kWireSetTradeSlot = 0x73` | active; set_trade_slot; warehouse/list/domain and ownership checks | no production UI producer established in this inventory |
| `command:SetSpeedCmd` | `5` | `kWireSetSpeed = 0x7a` | active; set_speed; host and multiplayer speed checks | no production UI producer established in this inventory |
| `command:LegacyRawCmd` | `6` | `c.type = dynamic (non-native only)` | reserved/refused by Commands::apply; non-native records can be logged/decoded | not a playable action; no legacy implementation implied |
| `command:FoundCmd` | `7` | `kWireFound = 0xF070` | active; Founding::apply; ship/assets/site/payment/permission checks | opening_ui.cpp and play.cpp founding flow |
| `command:MoveCmd` | `8` | `kWireMove = 0xF071` | active; ShipOrders::move; movement authority/plan-admission refusal | opening_ui.cpp ship-order input |
| `command:StopCmd` | `9` | `kWireStop = 0xF072` | active; ShipOrders::stop; authority/state checks | opening_ui.cpp ship-order input |
| `command:UnloadCmd` | `10` | `kWireUnload = 0xF073` | active; ShipCargo::unload; dock/range/owner/capacity checks | opening_ui.cpp cargo controls and commands_file.cpp |
| `command:LoadCmd` | `11` | `kWireLoad = 0xF074` | active; ShipCargo::load; dock/stock/hold checks | opening_ui.cpp cargo controls and commands_file.cpp |
| `command:AssignFreightRouteCmd` | `12` | `kWireAssignFreight = 0xF075` | active; FreightRoutes::assign; validated framed definition and authority | no production route-editor producer established; public sim commands are tested |
| `command:ControlFreightRouteCmd` | `13` | `kWireControlFreight = 0xF076` | active; FreightRoutes::control; live route-state/authority checks | no production route-editor producer established; public sim commands are tested |

All command rows map [Body / WireType](../../src/sim/include/anno/sim/command.hpp) through [wire_type and decoder](../../src/sim/src/codec.cpp) to [Commands::apply](../../src/sim/src/apply.cpp), with named ship/freight handlers called there. UI notes are deliberately narrower than sim support; a command-file/dev path is not a production panel.

## 3. Building runtime classes

The second column below is a lexical declaration position, not an evaluated enum
number. RuntimeClass, PayloadTag and TickKind declaration positions and raw
initializer identities are checked explicitly in §1a. This catches a newly explicit
numeric index or a changed encoded payload tag without an arbitrary evaluator.

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `class:Plain` | `declaration position 0` | `None / Inert / 0` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Workshop` | `declaration position 1` | `Workshop / Workshop / 500` | active workshop conversion, carrier job 0x33; goods still need inputs and transport | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:DualWorkshop` | `declaration position 2` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:WeaponsWorkshop` | `declaration position 3` | `WeaponsWorkshop / WeaponsWorkshop / 500` | active research-unlocked line conversion and carrier | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Farm` | `declaration position 4` | `Farm / Farm / 500` | active conversion; land harvesting is live; figure 142 route 45 body requires fisher extension; unsupported animal bodies remain refused/inert | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:FurTrapper` | `declaration position 5` | `FurTrapper / Farm / 500` | partial: conversion from raw works; tick explicitly suppresses animal send because fauna/prey search is absent | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Mine` | `declaration position 6` | `Mine / Mine / 500` | active deposit search/production; depletion follows settings; booking is separate | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:SmallFarm` | `declaration position 7` | `SmallFarm / SmallFarm / 500` | active worker/animal sends and conversion; visit/draw/data limits apply | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Quarry` | `declaration position 8` | `None / Inert / 0` | class tick None, but actual payload Quarry and mason job 0x2c claims are live; not an entirely inert class | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Hunter` | `declaration position 9` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Whaler` | `declaration position 10` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Shipyard` | `declaration position 11` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Cannonworks` | `declaration position 12` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Market` | `declaration position 13` | `Market / Market / 500` | active cart send and transport; not autonomous economic balance | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:MarketCombi` | `declaration position 14` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:MarketStall` | `declaration position 15` | `None / Inert / 5000` | class tick inert; resident stall purchases are reached externally in resident_job.cpp | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Inn` | `declaration position 16` | `Inn / Inn / 5000` | rule-gated by house_updates; tavern stock and carrier send live | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:PublicService` | `declaration position 17` | `None / Inert / 500` | class tick inert; service_mask_for_goods/type data feed placement-derived city counters | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Doctor` | `declaration position 18` | `None / Inert / 500` | inert/unported class tick; loaded object/footprint and financial accounting may still exist | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:School` | `declaration position 19` | `None / Inert / 500` | class tick inert; public-need type masks/counters can still be read | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Castle` | `declaration position 20` | `None / Inert / 0` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:FireBrigade` | `declaration position 21` | `None / Inert / 500` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:House` | `declaration position 22` | `House / House / 2500` | rule-gated by house_updates; house_tick and resident jobs live; default no-op | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:Tower` | `declaration position 23` | `None / Inert / 166` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |
| `class:CannonTower` | `declaration position 24` | `None / Inert / 166` | class tick inert/unported; loaded footprint, services/placement or booking are not thereby ruled out | [tick_building](../../src/sim/src/driver.cpp); [original_class_table](../../src/sim/src/types.cpp) |

## 3a. Runtime payload override

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `override:payload_tag_of` | `source expression` | `return type . cls == RuntimeClass :: Quarry && cls . payload == PayloadTag :: Inert ? PayloadTag :: Quarry : cls . payload ;` | Quarry claim payload despite table Inert; exact expression checked as review trigger | [payload_tag_of](../../src/sim/src/building.cpp) |

The class table is a default: TypeRegistry validates configured class/payload/tick mappings. None means only that this class tick returns without touching last. MarketStall purchases run in [resident_job](../../src/sim/src/resident_job.cpp); quarry claims and mason work run in [mason](../../src/sim/src/mason.cpp) and the figure scheduler. Building accounting is selected separately by [city_booking](../../src/sim/src/city_booking.cpp). No inert class row alone proves the object has no external effects.

## 4. Every Seams member

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `hook:city_pass` | `std::vector < Seam < CityPass > >` | `std :: vector < Seam < CityPass >> ( kOriginalCityPassCount )` | conditional installed hooks after building, stage and booking work; null entries empty | [city_tick](../../src/sim/src/island_tick.cpp) |
| `hook:wildlife_sweep` | `Seam < ChunkPass >` | `{}` | conditional installed chunk hook; null advances cursor only, no built-in fauna creation | [island_tick](../../src/sim/src/island_tick.cpp) |
| `hook:object_manager` | `Seam < IslandPass >` | `{}` | conditional installed hook over deposit-count stand-in traversal; null body empty | [object_manager_tick](../../src/sim/src/island_tick.cpp) |
| `hook:drought_tiles` | `Seam < DroughtTiles >` | `{}` | conditional installed strip hook; default tile-effect body absent, surrounding drought state machine lives | [drought_tick](../../src/sim/src/island_tick.cpp) |
| `hook:figure_tick` | `Seam < FigureTick >` | `{}` | conditional installed override for nonship worker bodies; built-in ship branch precedes it, default worker dispatch otherwise | [Scheduler::run_figures](../../src/sim/src/scheduler.cpp) |
| `hook:player_groups` | `Seam < PlayerPass >` | `{}` | conditional installed per-player hook; null empty; player bookkeeping still active | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `hook:defeat` | `Seam < PlayerPass >` | `{}` | conditional installed per-player hook; null empty; no full military defeat implementation claimed | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `hook:ai` | `Seam < Ai >` | `{}` | conditional table-level hook, generated commands immediately apply; null means no built-in strategic AI | [Scheduler::run_ai](../../src/sim/src/scheduler.cpp) |
| `hook:tile_cleanup` | `Seam < EventHandler >` | `{}` | conditional installed timed-event handler; null empty | [Scheduler::run_events](../../src/sim/src/events.cpp) |
| `hook:delayed_hit` | `Seam < EventHandler >` | `{}` | conditional installed timed-event handler; null empty, not combat support | [Scheduler::run_events](../../src/sim/src/events.cpp) |
| `hook:on_placed` | `Seam < PlaceHook >` | `{}` | conditional command/building hook after built-in placement; null empty, not a build stub | [place](../../src/sim/src/apply.cpp) |
| `hook:on_removed` | `Seam < PlaceHook >` | `{}` | conditional command/removal hook; null empty, built-in removal still runs | [demolish](../../src/sim/src/apply.cpp) |
| `hook:placement_rule` | `Seam < PlacementRuleFn >` | `{}` | active built-in placement_rule when null; named installed hook replaces verdict/height | [check_record](../../src/sim/src/apply.cpp) |
| `hook:upgrade_rewrite` | `Seam < Upgrade >` | `{}` | conditional rewrite/revalidation of placement record; null no rewrite | [check_record](../../src/sim/src/apply.cpp) |
| `hook:city_is_empty` | `Seam < CityEmpty >` | `{}` | conditional post-removal city emptiness decision; null false; keep_empty_city can retain city | [demolish](../../src/sim/src/apply.cpp) |

Null functions carry the default name; an installed function needs its own identity. Names are hashed/saved as configuration; function/context pointers are not serialized. The table records the Seams declaration default; World creation sizes city_pass to SimConfig::city_pass_count. The caller references, not the declaration alone, establish whether a null hook has built-in behavior.

## 4a. Scheduler phase and supporting call surface

| Identity | Declared type / index | Initializer / mapping | Current meaning / constraints | Production reader / review anchor |
| --- | --- | --- | --- | --- |
| `phase:w.rng_.cursor` | `Scheduler::advance call surface` | `first-seen 0; sites 6` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.clock_.tick` | `Scheduler::advance call surface` | `first-seen 1; sites 4` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:advance_script` | `Scheduler::advance call surface` | `first-seen 2; sites 1` | script visit before clock/normal phases; failure records fault and stops | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.route_cache_.begin_advance` | `Scheduler::advance call surface` | `first-seen 3; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.out_.events.push_back` | `Scheduler::advance call surface` | `first-seen 4; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.host_events_.push` | `Scheduler::advance call surface` | `first-seen 5; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.clock_.begin` | `Scheduler::advance call surface` | `first-seen 6; sites 1` | clock begin; zero elapsed still visits subsystems | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:global` | `Scheduler::advance call surface` | `first-seen 7; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:draw` | `Scheduler::advance call surface` | `first-seen 8; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:PlayersView` | `Scheduler::advance call surface` | `first-seen 9; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:IslandsView` | `Scheduler::advance call surface` | `first-seen 10; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.fisher_enabled` | `Scheduler::advance call surface` | `first-seen 11; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.islands_.size` | `Scheduler::advance call surface` | `first-seen 12; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:has_value` | `Scheduler::advance call surface` | `first-seen 13; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:island_tick` | `Scheduler::advance call surface` | `first-seen 14; sites 1` | phase 1, in island order; building/city work and built-in stage/booking; effects merged per island | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:draw_records_between` | `Scheduler::advance call surface` | `first-seen 15; sites 2` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:merge` | `Scheduler::advance call surface` | `first-seen 16; sites 2` | deterministic effect merge after each island and final timer/event batch | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:w.clock_.end` | `Scheduler::advance call surface` | `first-seen 17; sites 3` | normal completion and fault exits; records completed clock | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:run_events` | `Scheduler::advance call surface` | `first-seen 18; sites 2` | phase 2 plus optional post-figure second pass (events_flush_after_figures) | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:run_figures` | `Scheduler::advance call surface` | `first-seen 19; sites 1` | phase 3; freight/ship mover then seam or built-in harvest/mason/resident/transport/interim paths | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:player_table_tick` | `Scheduler::advance call surface` | `first-seen 20; sites 1` | phase 4, bookkeeping plus installed player/AI hooks | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:fx.ui.reserve` | `Scheduler::advance call surface` | `first-seen 21; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:timers_tick` | `Scheduler::advance call surface` | `first-seen 22; sites 1` | phase 5 script timers; optional timer0 clamp | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:fx.ui.push_back` | `Scheduler::advance call surface` | `first-seen 23; sites 2` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:monopoly_refresh` | `Scheduler::advance call surface` | `first-seen 24; sites 1` | phase 6; refresh governed by monopoly_every_30s | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:event750` | `Scheduler::advance call surface` | `first-seen 25; sites 1` | phase 7; settings and event750_keep_overshoot apply | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:hit.has_value` | `Scheduler::advance call surface` | `first-seen 26; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |
| `phase:rect` | `Scheduler::advance call surface` | `first-seen 27; sites 1` | supporting call token, not an additional gameplay phase | [Scheduler::advance](../../src/sim/src/scheduler.cpp) |

The numbered gameplay order is islands → events → figures → optional events → players → timers → monopoly → 7.5-minute event. Script evaluation precedes the clock begin; faults short-circuit later phases. First-seen positions and counts above are lexical call sites, not runtime visit counts; control-flow order remains a review obligation. Placement/removal/upgrade/emptiness hooks in §4 run outside this advance surface and remain part of the inventory. [initialize_scripts](../../src/sim/src/scheduler.cpp) also performs explicit startup before command sessions.
