# The two rules presets (T36)

`RulesPreset` (`game_setup.hpp`) is `Enhanced` or `Original`. `Enhanced` is the
zero value and the default (D11: the enhanced rules are the default, and the
original, bugs included, is the option). This table is the value of every
`Rules` field (`config.hpp`, from the `Rules` struct) and of the two render
options the setup names: water style and river flow.

The original column is today's default, which is the faithful value, with the
D number or the OQ. The enhanced column is a proposal. An obvious bug gets the
fix as that proposal. A question of how the game feels is marked in the owner
column and is not decided here. Two fields are the same on both presets
because they are not a feel switch: the dispatch cap, and the free-build
cheat. `island_candidate_order` is the same because the owner already decided
it.

`Original0x10027000`, `PerIsland`, `drought_random_centre = true` and `Cached`
are reserved. `Rules::validate` rejects them today, so an enhanced proposal
cannot be one of those until it exists.

## The table

| Field | Original (today's default) | Enhanced proposal | Evidence | Owner |
| --- | --- | --- | --- | --- |
| `slot_policy` | `Original` | not decided | D1, OQ-036. Visit order of buildings. | yes, feel |
| `spatial_order` | `YXSlot`. This default is the canonical order. The original's unstable quicksort is the reserved enumerator `Original0x10027000`, which `validate` rejects (O-7). | not decided | D2, OQ-037. | yes, feel |
| `events_drain_new` | `false` | not decided | D3, OQ-038. | yes, feel |
| `events_max_dispatch` | `kEventsMaxDispatchDefault` (2^20) | the same cap | F24. A safety limit, not a feel switch. The original had no cap. | no |
| `events_flush_after_figures` | `false` | not decided | D4, OQ-039. | yes, feel |
| `monopoly_every_30s` | `false` | not decided | D5, OQ-040. The refresh runs on the first advance only. | yes, feel |
| `player_table_full_tick` | `false` | not decided | D6, OQ-041. | yes, feel |
| `drought_random_centre` | `false` (`true` is rejected) | not decided | D7, OQ-042. Reserved until a random centre is specified. | yes, feel |
| `build_atomic` | `false` | not decided | D8, OQ-043. | yes, feel |
| `keep_empty_city` | `false` | not decided | D9, OQ-044. | yes, feel |
| `event750_keep_overshoot` | `false` | not decided | D10, OQ-045. | yes, feel |
| `prng_mode` | `Global` (`PerIsland` is rejected) | not decided | D11, OQ-046. | yes, feel |
| `interim_figure_return_ticks` | `0` | not decided | D12, OQ-047. Stand-in for figures other than the cart and the carrier. | yes, feel |
| `free_build_cheat` | `false` | `false` | F13. A cheat, never taken from the wire. Both presets leave it off. | no |
| `enforce_research` | `false` | not decided | O-30. Placement without the research is what the default allows. | yes, feel |
| `stat_decay_wide` | `false` | not decided | D21, OQ-057, open, owner decides. The u32 product wraps from 41 297 763. The fix would be the wide product. | yes, feel |
| `bankrupt_deadline_wrap_safe` | `false` | not decided | D22, OQ-057, open, owner decides. Near tick 2^32 the grace period wraps. The fix would keep it. | yes, feel |
| `timer0_clamp` | `false` | not decided | F5, OQ-133. Owner 2026-10-23: stay on the original bug, the clamp is the optional fix. That decision is the `Rules` default. Whether the enhanced preset turns the clamp on is still a feel question (script timing). | yes, feel |
| `island_candidate_order` | `Sorted` | `Sorted` | OQ-150, decided 2026-10-23. `Cached` stays rejected. | no, already decided |
| water style | `WaterStyle::Original` (`iso.hpp`; `cli.hpp` default) | not decided | PLAN §0.2: the owner leans toward the enhanced water. That lean is not a lock for this preset. | yes, feel |
| river flow | the original look is `RiverFlowMode::Legacy`. The viewer's default is `Flow` (`cli.hpp`). | not decided | `iso.hpp`, `cli.hpp`. The two facts differ, so the original column keeps both. | yes, feel |

## Gaps: FIX or RULE issues with no Rules option

Scanned the implication column of `docs/research/original-issues.md`. A row is
here when that column is tagged `FIX` or `RULE` (including `FIX + RULE`,
`RULE/data`, `RULE (QoL)`, and a `FIX` that is still a candidate). Rows tagged
only `FAITHFUL` or `N/A` are not gaps of this table. None of the rows below is
a field of `Rules`. Many are engine work (64-bit, crashes, multiplayer), not
a switch a preset would turn.

| Id | Tag | Gap |
| --- | --- | --- |
| ISS-001 | FIX | Tutorial and scenario exit must not crash. |
| ISS-002 | FIX | Demolish must drop every reference to the building. |
| ISS-003 | FIX | Radius queries must stay inside the map. |
| ISS-004 | FIX | No fixed object pools. Unit caps stay data (see ISS-061). |
| ISS-005 | FIX | Save loading is a parser that refuses a bad file. |
| ISS-006 | FIX | Data files are checked on load. |
| ISS-008 | FIX | A cutscene decode error skips the cutscene. |
| ISS-009 | FIX | One save loader for single player and multiplayer. |
| ISS-010 | FIX | Autosave writes a temp file and then renames it. |
| ISS-011 | FIX | The renderer does not depend on one vendor. |
| ISS-012 | FIX | The screensaver stays off during play. |
| ISS-013 | FIX | Resolution, UI scale and window mode are settings. |
| ISS-016 | FIX | Water shaders are tested with driver post-processing on. |
| ISS-017 | FIX | A saved window position is checked and falls back. |
| ISS-018 | FIX | Linux is a target, including a 1280×800 case. |
| ISS-019 | FIX | Build and data-set identity are visible. |
| ISS-020 | FIX | A per-tick performance budget. |
| ISS-021 | FIX | The sim scales without quadratic scans. |
| ISS-022 | FIX | Lockstep stays in budget on the slowest peer. |
| ISS-023 | FIX | Autosave runs off the window thread; the interval is a setting. |
| ISS-030 | FIX | Multiplayer is in scope. |
| ISS-031 | FIX | Lockstep does not depend on the operating system. |
| ISS-032 | FIX | Per-tick checksums and a resync. |
| ISS-033 | FIX | Disasters draw on the sim PRNG. |
| ISS-037 | FIX + RULE | Desync recovery is in scope. The scenario list is data. |
| ISS-038 | FIX | Computer players in a network game run inside the sim. |
| ISS-039 | FIX | A save's identity lives in the file. |
| ISS-040 | FIX | Save, load, save is byte-identical. |
| ISS-041 | FIX | Derived caches follow the content hash. |
| ISS-042 | RULE (QoL) | Quicksave and quickload, and a reason when saving is off. |
| ISS-043 | FIX | A failed write reports the path; the save directory is configurable. |
| ISS-044 | RULE | Autosave interval, including off. Screenshots use a timestamped name. |
| ISS-050 | FIX | Placement rejects a footprint that a unit or a ship occupies. |
| ISS-052 | FIX | Gate state is part of the plan a reload must reproduce. |
| ISS-055 | FIX candidate | A possible computer-player bug, still to be confirmed by disassembly. |
| ISS-056 | FIX | Tribute, if disassembly shows a bug. Diplomacy transitions should be explicit. |
| ISS-057 | FIX | One function enforces unit caps for every player. |
| ISS-060 | FIX | One grid pathfinder, and a unit is not left with no tile. |
| ISS-061 | RULE/data | Ship and soldier caps come from the scenario (`LIMITS`), not from a constant. |
| ISS-062 | FIX | A ship position has to be a water tile, with an unstick. |
| ISS-063 | FIX | The 1.05 combat items. Auto-target stays faithful unless a rule is added later. |
| ISS-064 | FIX | Repair resumes when the materials arrive. |
| ISS-065 | RULE (QoL) | Escort, patrol and matched speed as opt-in orders. |
| ISS-067 | RULE (QoL) | Unload in a batch, and a type filter on the selection. |
| ISS-068 | RULE (QoL) | A group of one unit is allowed. |
| ISS-072 | FIX | Capacity checks live in one place. |
| ISS-073 | FIX | A compound unit moves as one. |
| ISS-074 | RULE (QoL) | A warehouse can be a stop of its own. |
| ISS-080 | FIX | A harvest that skips fields. |
| ISS-083 | FIX | Population counters wider than 16 bits. |
| ISS-084 | FIX | Scoreboard counters that do not overflow their buffer. |
| ISS-086 | FIX | Productivity display, UI only. |
| ISS-088 | FIX | Shortage notices after a house is downgraded and raised again. |
| ISS-090 | FIX | The fertility indicator. The road-arrow rule stays faithful. |
| ISS-092 | RULE (QoL) | Full statistics in the base mode as well. |
| ISS-100 | RULE (QoL) | Tooltips and hotkeys. Picking uses the sprite mask. |
| ISS-102 | RULE (QoL) | Less walking for the scout. |
| ISS-103 | FIX | A war declaration asks for confirmation. |
| ISS-104 | RULE | Separate volume sliders and a per-sound cooldown. |
| ISS-105 | FIX | The missing Moor music path, fixed in the data layer. |
| ISS-108 | FIX | IMA ADPCM WAV playback. |
| ISS-110 | FIX | Objectives do not depend on the order they complete. |
| ISS-111 | RULE (QoL) | A warning when a scenario cannot be finished, and a restart. |
| ISS-113 | FIX | An editor, later, for modding. |
| ISS-120 | FIX | A price floor, so a ship cannot be sold and bought back at 1. |
| ISS-121 | RULE | A stronger computer player, faithful unless chosen. |
| ISS-131 | FIX | No account and no online login for single player or LAN. |
| ISS-133 | RULE | Languages are data. |
| ISS-135 | RULE | Faithful-glitch switches are cosmetic only. |

## Questions for the owner

Not decided here. Each one changes how the game feels, or the evidence does
not pick a value.

- Every row above whose owner column says "yes, feel", including whether the
  enhanced preset turns `timer0_clamp` on. The 2026-10-23 decision stays the
  `Rules` default (`false`).
- `stat_decay_wide` and `bankrupt_deadline_wrap_safe` (OQ-057, still open).
- Water style. The lean in PLAN §0.2 is not a lock.
- River flow, given that the original look and the viewer default differ.
- River gain (`4`, OQ-083), river speed (`MedianCell`, OQ-065) and the river
  gap join (`8`, OQ-068). They are viewer tunings, not rows of this table,
  and they have no preset.
- Which human money amount a start-money bullet maps to (OQ-221), and what
  the three addon pages are on the six axes (OQ-220).
- Whether pirate `COMP TYP` (measured 2..7) should replace the three
  description words (OQ-222). The setup enum is the three words.
