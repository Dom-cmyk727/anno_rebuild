# Two-island manual freight: consumer card

This is the executed headless MAP-EXPAND-1 recipe for the existing typed command
API. The Windows lane owns the window adapters and live playtest. Read the
MAP-EXPAND handoff for the exact tested source and terminal gate results.

## Starting state

Use unchanged `data/maps/candidates/ship-start-v2/lan5-ship-a.json`, seed 26031,
1431 × 1431 tiles and 28 islands. Manifest SHA256:
`20d948c97b1801946d71dfa86762e9b0fea5e6cd093314d83cb5b43658b2fd20`.
Content hash:
`e001442a55b8ee0890ec5cd3d23e5a53f05d66fe3a29fdb1c1b47d19a3363d92`.
Original assets are required; the evidence includes the complete 472-file input
inventory. Do not install a different map, rebalance its loadout or grant rights.

The recipe uses state 134 / ECONBK1 / PRNG5 / replay 4 / command 1 / native wrapper
1. Old 133 saves and logs refuse; do not relabel or repair them. Explicitly enable
`city_booking` and retain `save_billed_goods`; keep `house_updates` false. These
are canary settings. Application defaults remain unchanged.

Seat 0 starts with ship 1 (type 1502), crew 2 parented to ship 1 and donkey 3
parented to crew 2, 30,000 gold and no cities. Ordered holds contain 50 t wood,
10 t food, 50 t tools and 30 t tools; each hold has 50 t capacity. Peace, shared
information and no alliances remain in effect. Visibility grants no cargo rights.

Read stable city and warehouse IDs from actual outcomes and snapshots. Warehouse
IDs 1 and 4294967297 below are reference observations. Re-resolve IDs across
turns; do not retain city/building pointers or substitute a nearer rival.

## Commands and observations

Post `sim::Command{PlayerId{0}, body}` through `PlayRunner::post_command`.
A queued receipt is not command acceptance. Match each actual `ShipArrived` to
the ship and order; check the dock, crew and holds. For every transfer inspect
`transferred_88`, the named warehouse stock and the resolved hold.

Ticks below are simulation ticks of 2 ms, using 1000 ms advances. They are not
wall-clock waits. Positions are tile centres in Q24.8 where stated.

| Tick | Existing typed operation | Required result |
|---|---|---|
| 0 → 86000 | `MoveCmd`, ship 1, target `(65920,34432)` = tile `(257,134)` | Actual source arrival, order 1; initial holds unchanged. |
| 86000 | `FoundCmd`, ship 1, type 1302, origin `(254,137)`, click `(255,138)`, cursor rotation 0 | Source island 0, new CityRef and warehouse; pay 350 gold, 12 t wood and 5 t tools aboard; warehouse starts with zero wood. |
| 86000 | `UnloadCmd`, source warehouse, hold 2, tools 41, 2 t; `BuildCmd` 801 at `(238,128)`, rotation 0 | Transfer 512 raw tools, then pay 150 gold and 2 t tools, zero wood. |
| 86000 | `BuildCmd` with the 20 road placements below | Legal roads; pay 100 gold, zero materials. Source wood and tools both zero. |
| 123500 | `LoadCmd`, source, hold 0, wood 42, 1 t | Real harvesting and delivery have supplied 448 raw; debit and credit 256, retain 192. No inherited wood was unloaded here. |
| 123500 → 171000 | `MoveCmd` to `(115840,89472)` = tile `(452,349)` | Actual destination arrival, order 2. |
| 171000 | `FoundCmd` 1302, origin `(455,352)`, click `(456,353)`, cursor rotation 0 | Distinct island 1 and city, owner 0; observed warehouse 4294967297, accepted rotation 2, height 48, owned 4 × 4 footprint. Pay 350 gold, 12 t wood and 5 t tools aboard. |
| 171000 | `UnloadCmd`, destination, hold 2, tools 41, 1 t | Transfer 256 raw tools; source unchanged. |
| 171000 | `BuildCmd` 1310 at `(450,355)`, rotation 0, before unloading wood | `CannotAfford`; full serialized state and payment unchanged. |
| 171000 | `UnloadCmd`, destination, hold 0, wood 42, 1 t; repeat the same build | Transfer 256 raw; build succeeds at height 48, paying 50 gold, 1 t wood and 1 t tools. Source stocks, bills and accounts unchanged by these commands. |
| 171000 → 218000 | `MoveCmd` back to source dock `(257,134)` | Actual return, order 3; normal production continues. |
| 218000 | `LoadCmd`, source, hold 0, wood 42, 2 t | Transfer 512 raw. The 374 raw already made at first export cannot fund this alone; 1834 raw additional wood was made before this load. |
| 218000 → 265000 | `MoveCmd` to destination dock `(452,349)` | Actual arrival, order 4; same ship and crew. |
| 265000 | `UnloadCmd`, destination, hold 0, wood 42, 2 t | Transfer 512 raw, completing a second freight cycle. Ordered holds end at 26 t wood, 10 t food, 37 t tools and 30 t tools. |

The road command's existing placements vector contains type 2520, rotation 0,
byte7 = byte8 = 0 at `(241..253,130)` inclusive, then `(253,131..137)` inclusive.
These are recorded placements, not a new command syntax.

Founding uses `dx = (click_x*256 - actual_ship_x)/256` and the same for y,
with division toward zero, then `dx*dx + dy*dy ≤ 168`.
Cargo also needs the warehouse footprint range and a city-owned dock outline.
Both reference docks satisfy those predicates. Founding range or visibility of a
rival warehouse alone does not authorize transfer. In a live session wait for
actual stock and range, then calculate whole tons and hold room; exact reference
ticks and quantities depend on the unchanged input and advance partition.

## Ledger and visible closure

Construction costs 1000 gold. Two founding commands consume 24 t inherited wood;
the receiving building consumes 1 t produced wood. All 13 t construction tools
come from original cargo, and no food is consumed. The source never receives
inherited wood. Attribution is established by the complete ledger and the public
no-production control; cargo has no invented provenance tag.

Source active city cost is 27, destination cost 20. Each independently charges
1 gold per due 2000 ticks, with its own stamp. Source upkeep totals 90, destination
47; resident income and existing player upkeep are both zero in this scenario.
Final money is `30000 - 1000 - 90 - 47 = 28863`. Final source wood is 2368 raw and
destination wood 512 raw. The integration test additionally closes tree/raw/
producer-output/cart/stock accounting and all other owners' money.

## Windows adapter delta and remaining limits

`OpeningView` exposes stable ship, hold, city and warehouse state. The generic
runner already accepts typed `LoadCmd` and exposes `transferred_88`.
`OpeningUi` and `commands_file.cpp` currently expose Unload, but lack a Load mode
and text token. Windows owns those adapters, their session roundtrip and the live
playtest. This card does not establish that the current window executes it.

Finite tools, household defaults and needs, unverified resources, salt/cloth/
hunting integration and town-stage prerequisites remain. Two freight settlements
do not establish settlers, profitability, food self-sufficiency or a balanced
town. Navigation retains bounded coarse/fine searches, goal-disc and density
semantics, and existing collision/replanning limits. Automated routes, foreign
trade, combat, network multiplayer and late-game scale are outside this proof.

Local synthetic measurements and their raw samples are reported separately in
the handoff. They are not timings for a live Windows session or a replacement
for the original-data voyage.
