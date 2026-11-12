# Windows consumer delta, state 137 + route-only 136 -> 138

Status: MAP-INTEGRATE-2 final consumer card, 2026-11-12. This document combines the accepted
state-137 extraction card with the automatic-freight consumer contract. It describes the public
simulation interface a later Windows UI adapter must consume; it does not implement that adapter.

## 1. Accepted pins and identity

State 138 is the exact coordinated union of:

- automatic freight source `49ffcee66f5bdb678e4e165112abacccd84e727a`, merged by
  `d91e4a7a79cb0752f75e75f4386db38d30a89d5b`;
- salt, iron/tools, Quarry/bricks/marble source
  `a5a3bca35c781231ab8d52205a5a1117b4cce23f`, merged by
  `67272f109e77e5c9ea0cefee57389890d813e5aa`.

The identity constants are state stream **138** (`src/sim/include/anno/sim/world.hpp:176-186`),
PRNG timeline **5** (`world.hpp:187-202`), replay frame **4**
(`src/sim/include/anno/sim/replay.hpp:37-39`), command wire **1**
(`src/sim/include/anno/sim/command.hpp:25-27`) and save frame **1** (`world.hpp:184-186`). The
schema remains ECONBK1. Freight adds no draw site; Quarry retains payload tag 9.

The Windows consumer must not reinterpret a 134, 135, route-only 136 or extraction-only 137 save or
recording as combined state. `World::load` is the bounds-checked entry point
(`world.hpp:252-260`). Every non-138 state is refused; actual 134-137 native saves and recordings
are covered by `combined identity138: valid native and replay artifacts 134 through 137 refuse at
the intended boundary` and `integration identity: actual predecessor saves and logs refuse at
intended boundary` (`src/sim/tests/integration_identity_test.cpp`).

## 2. Quarry payload: exhaustive visitors must handle it

`Building::payload` has ten alternatives and ends in `Quarry`
(`src/sim/include/anno/sim/building.hpp:114-117`). Its public state is the Inert-compatible prefix
plus one 0/1 claim byte per type entrance (`building.hpp:51-66`); its stable save tag is 9
(`src/sim/include/anno/sim/types.hpp:87-104`). A claim means a mason worker has reserved that
entrance while walking to or waiting at it. It does not mean the quarry depletes or holds goods.

A consumer that visits the payload must add a `Quarry` arm. It may resolve the building through
`World::find_building(BuildingId)` (`src/sim/include/anno/sim/world.hpp:349-351`) and read
`Quarry::claims`, `status`, `suspended` and `blocked`; it must not mutate them. The current diagnostic
app already proves the exhaustive visitor and exact text through `inspect_building: quarry,
hand-derived status line` (`src/app/tests/inspect_building_test.cpp`). Creation and native
continuation are exercised by:

- `quarry placement: a BuildCmd creates a quarry with no claims and no deposit`;
- `quarry claims Q-A1: two masons and one 1208 (one entrance) never share the entrance`;
- `quarry continuation Q-7: a reload mid-walk, mid-wait and mid-return equals the run`
  (`src/sim/tests/quarry_test.cpp`).

Quarries use their own HIGHMAP placement table when `PlacementParams::quarries` is true
(`src/sim/include/anno/sim/placement.hpp:83-96`). Stage/research-locked iron mines and quarries
answer `Reject::ConstructionDenied`; unsupported gold/gem mountain types answer
`Reject::Unsupported` (`src/sim/include/anno/sim/reject.hpp:30-38,102-116`). A UI should query
`World::construction_eligibility(PlayerId, TypeId)` before presentation, while still treating the
actual `BuildCmd` result as authority (`world.hpp:278`). The focused refusal controls are `salt
mine: the refusal matrix with a legal neighbour per row` and `quarry placement: a BuildCmd creates
a quarry with no claims and no deposit`.

## 3. Mine and quarry read-only presentation

`mine_facts(World, BuildingId)` returns binding, ore tile, remaining 8.8-ton stock, reserve/deep/
depletion facts, output and derived state (`src/sim/include/anno/sim/mine_facts.hpp:15-57`). There is
no dedicated Exhausted enum. For a bound, depleting mine, the consumer derives exhaustion from the
remaining/deep/reserve rule; `MineState::Blocked` alone can also mean a full output store.
`mine_site_deposit_in_reach` is a read-only warning helper, not command authority
(`mine_facts.hpp:59-68`). A mine with no source is accepted, charged and created unbound, matching
the original; the reserved `Reject::NoDepositInReach` value is never emitted
(`reject.hpp:110-116`).

The source and interruption behavior is covered by `iron interruption: a shallow deposit exhausted
in the depleting setting idles with one notice and a neighbour ore keeps the mine working`, while
Quarry-to-mason-to-store-to-`BuildCmd` is covered by `quarry brick proof Q-5 Q-8: stone becomes
bricks in the store, spent by a BuildCmd` (`src/sim/tests/iron_interrupt_test.cpp`,
`src/sim/tests/quarry_test.cpp`).

## 4. Automatic freight command surface

The later UI submits only authenticated public commands:

- `AssignFreightRouteCmd { ship, stops }`;
- `ControlFreightRouteCmd { ship, Suspend | Resume | Cancel }`.

They are append-only alternatives in `Command::Body` (`src/sim/include/anno/sim/command.hpp:156-170`)
with native wire tags `0xF075` and `0xF076` (`command.hpp:180-208`). A stop carries the exact stable
warehouse `BuildingId`, Q24.8 dock coordinates and ordered actions. `LoadTarget` means desired total
aboard in the first eligible holds; `UnloadAmount` means the requested removal. Quantities are
whole tons in the command and exact 8.8 tons in balances
(`src/sim/include/anno/sim/freight_state.hpp:16-53`).

The issuer, ship owner, stable target, dock, goods, hold limits and range are checked by the sim. A
nearer visible warehouse grants no authority. A rebuilt warehouse has a new ID and requires a new
assignment. Canonical grouping folds same-direction/same-goods actions into their first slot; source
shortage, a full destination and unavailable holds skip rather than waiting indefinitely. These
contracts and their atomic refusals are executed by `freight commands: live authority canonical
grouping deep copies and atomic refusals` (`src/sim/tests/freight_state_test.cpp`).

After assignment, the host issues ordinary advances. It must not synthesize an arrival or transfer.
`World::freight_route(FigureId)` is the read-only current-state query
(`src/sim/include/anno/sim/world.hpp:285-290`). The status and phase enums distinguish Running,
Suspended, Failed and NeedLeg/Sailing/Wait/Scan/Transfer/CompleteStop
(`freight_state.hpp:16-53`). Command acceptance is not arrival; arrival is not transfer; a cycle may
be unproductive.

## 5. Freight observations and manual takeover

Freight output is `SimEventKind::Freight` with an optional `FreightObservation`
(`src/sim/include/anno/sim/tick.hpp:326-342,358-396`). The observation identifies the actual signal,
route generation, stop/action, warehouse, goods, direction, amount, phase/status, refusal reason,
navigation failure and completed cycles. The Windows adapter should correlate those fields and
display Pending/Arrived/Transferred/Skipped/Paused/Resumed/Cancelled/Failed/Cycle/ManualTakeover;
the host event queue is observation output, never an authority input.

Suspend pauses the route and sailing. Resume revalidates the current target and excludes paused
time. Cancel removes automatic control. Successful public Move, Stop, Found, Load or Unload takes
manual control; a refused manual command leaves the assignment unchanged. These boundaries are
executed by:

- `freight lifecycle: arrival authority public replacement and stale revision predicate`;
- `freight lifecycle: pause resume native continuation and cancel after a partial commit`;
- `freight takeover: accepted public Move Load Unload take control and refusals do not`;
- `freight takeover: a refused Found preserves assignment and a successful public Found cancels it`
  (`src/sim/tests/freight_lifecycle_test.cpp`, `freight_state_test.cpp`, `founding_test.cpp`).

Failed navigation and missing/foreign/out-of-range targets are recoverable route status, not host
permission to edit state. Cancel remains available for cleanup; a replacement warehouse needs a new
assignment. The recovery cases are covered by `freight failure: unreachable leg recovery uses
reassignment and blocked cleanup stays public` and `freight lifecycle: missing rebuilt target
needs a new stable assignment and ending clears route`.

## 6. Combined saved state and compatibility obligations

State 138 appends the three `FreightRules` bounds (`src/sim/include/anno/sim/config.hpp:562-566,693`)
and, per figure, `freight_generation` plus optional `FreightState` after resident state
(`src/sim/include/anno/sim/figure.hpp:348-356`; serialized at
`src/sim/include/anno/sim/state_fields.hpp:1356-1358`). It retains Quarry payload tag 9 and the
mason-only goal sub-cell bytes. The exact combined layout is summarized beside the state constant
(`world.hpp:176-183`).

Do not add a Windows-only cache to the canonical state, change command tags, coerce an old archive,
or infer a route from UI selections. Host widgets keep only presentation state. The sim owns route
definition, continuation, transfer, target identity and manual takeover. `freight state: every
future field reaches hash and roundtrip and rules digest`, `freight native: corrupt continuation
refuses and real134 cannot enter138`, and `integration authored cross-feature: mine tools freight
quarry and exact reload` execute those boundaries (`src/sim/tests/freight_state_test.cpp`,
`integration_cross_feature_test.cpp`).

## 7. Verified real-world consumer path and remaining Windows work

The accepted canary manually sails and founds both settlements, builds original type 801 plus 20
roads, collects renewable wood, then assigns the original Human ship. `LoadTarget 29 t` at the
source and `UnloadAmount 3 t` at the destination move two positive 768-raw deliveries while
restoring the inherited 26 t ship baseline after delivery. The receiving city then pays for an
unchanged type-1310 `BuildCmd`. Five native continuation cuts cover source sailing, source wait,
outbound sailing, destination wait and return sailing. Honest and changed-command fresh-process
replays remain distinct. The executable proof is `freight original voyage: produced wood funds two
automatic cycles and receiving build`, plus its honest and changed fresh-process
consumers in `src/sim/tests/freight_original.inc`.

A later Windows UI package still must:

1. expose typed route assignment/control without inventing another command format;
2. retain stable warehouse IDs and validated Q24.8 docks returned by the actual world;
3. present query state and correlated observations, including shortage, failure and pause;
4. preserve ordinary manual controls and show when they take over;
5. handle `Quarry` in every exhaustive payload visitor and optionally present mine/quarry facts;
6. refuse old saves/recordings with the sim's exact diagnostic rather than offering migration.

Windows/MSVC compilation, live window/input/GPU behavior and the Windows original game-file run are
not established by this Mac card. They remain the orchestrator's post-candidate work.

## Decisions and reasoning

- Kept the Windows layer a consumer of public commands, queries and observations. It does not
  become a second scheduler or cargo authority.
- Named stable IDs and event generations because location, visibility and a generic ship arrival
  are insufficient authorization.
- Carried Quarry's append-only payload contract forward instead of flattening it back to `Inert`;
  claim bytes are required for exact continuation.
- Kept old-state refusal explicit. State 138 is a coordinated union, not an alias for either 136 or
  137.
- Tied every public surface to an executable test and left Windows runtime work honestly NOT_RUN.
