# The live ship-start opening (W4-OPENING-1)

A bounded developer playtest UI for the accepted MAP opening: select a ship, sail, found, unload, build, in the
real play window. It is not UI.2, the game menu, network multiplayer, trade routes, combat or renderer
optimisation. For the owner's steps see [`docs/testing/ship-opening-playtest.md`](../testing/ship-opening-playtest.md).

Inputs, fixed pins: Windows `26107db0` (verifier accepted) and Mac `0c401c8d` (MAP-BUILD-1 + MAP-CARGO-1). The
sim commands are consumed as they are: `MoveCmd`, `StopCmd`, `FoundCmd`, `UnloadCmd`, `BuildCmd`. This layer
decides no rule; the sim refuses.

## 1. Architecture

| Piece | Where | Rule |
|---|---|---|
| Launch | `anno1503 --ship-start <manifest.json> --seat N [--resume <save>] [--game-dir D] [--speed 50\|100\|200]` (`cli.cpp`, `main.cpp`) | `sim_load::load_ship_start` on the typed manifest (`opening.cpp`): developer start and developer resources off, no cities, original loadout, peace, shared information. It does not use the legacy fixed-map loader. A bad manifest, seat, missing template or save is a readable error and nothing starts. |
| Identity | `OpeningIdentity` | manifest path, **content hash**, seat, rules digest, registry digest, start hash (and the save a resumed session continued). Written to `session.txt` and to every save. |
| Sim thread | `PlayRunner` | Publishes an immutable `OpeningView` inside every `PlaySnapshot` (`opening_view.hpp`): every sailing ship (stable `FigureId`, owner, kind, `FixedPos`, `dir16`, holds, the carried order's id), the local participant's cities (`CityRef`, name, warehouse `BuildingId` + tile + footprint, stock, capacity), gold, a bounded list of ship events (`ShipArrived`, `ShipRouteFailed`, `ShipStopped` with the order id), and the sim's order-id counter. No `World`, `Figure` pointer or slot leaves the sim thread. A wake is posted every turn of an opening. |
| Window thread | `Viewer` + `OpeningUi` (`opening_ui.cpp`) | The controls are window-free logic. The viewer maps SDL events and the last drawn frame (`FrameProjection`, `frame_projection.cpp`) to them, posts the commands they make to the runner's queue and feeds them the newest snapshot. |
| Ships on screen | `ShipLayer` (`ship_layer.cpp`) + `render::UnitSprite` | Original figure frames of every facing (`assets::unit::facing_sprites`), the owner's colour (`kOriginalPlayerColours[owner mod 8]`, a stand-in for the colour screen), shadows. A per-frame overlay list: the GPU compositor packs it with the scene's own units (`GpuFrame::render(..., live_units)`), the CPU renderer blends it over the finished frame (`blend_units`), after every object and in painter order. A sailing tick changes no chunk, scene generation or island list. |
| Building tool | `DevTool::retarget` | After a founding, the tool and its occupancy index follow the city the published view names. It is derived from the view each time, never assumed at construction. |

Identity of ships, cities and warehouses is the stable id; the controls re-read it from the newest snapshot when
a command is made, and a selection or a target whose id is not in the newest view is dropped with a message.
**Between two turns** (owner playtest 2026-11-03: the ship looked as if it teleported from tile to tile): the window
does not draw the published position. `ShipMotion` (`ship_motion.cpp`, render-side, unit-tested) keeps, per ship, the segment
from where the ship was drawn when a turn arrived to where that turn put it and draws the point of that segment the wall clock
has reached, one publication interval behind and never past the newest turn (no overshoot at a stop; a jump over 6 tiles is
shown at once). The facing follows the direction of that segment and turns by at most one of the sprite's 16 steps per 60 ms;
a ship that does not move shows the sim's own `dir16`. The hit test uses the drawn position (`FrameProjection`); the facts, the
commands and the HUD tile stay on the sim's positions.

**Facing frame.** Frame k of the figure's 16 facing frames shows its bow towards screen compass 45 + 22.5 k degrees (measured on
screenshots of one ship sailing the eight directions, 2026-11-03). In the original's handedness (`docs/render/iso.md`,
"Handedness") `dir16` d heads the same way, so `dir16` d is frame d (`ShipLayer::facing_frame`), as the original draws a scenario
ship's facing `attr`. Measured in the mirrored picture before the handedness fix, the same frames gave `(12 - d) mod 16`. Re-checked
2026-11-03 in the corrected projection: for `dir16` 0, 2, …, 14, frame `facing_frame(d)` of figure 1505 (group 1) beside the
heading's screen direction (`((vy − vx) · tw/2, (vx + vy) · th/2)`); every bow follows its heading (frame 10's bowsprit points
west, frames 0/4/8/12 north-east/south-east/south-west/north-west). A contact sheet of the frames the viewer draws, not a live
recording of a sailing ship; the owner's playtest is the live check.

**The water.** The opening starts with enhanced water on the GPU (`E` toggles). Each moving ship is a source in the wave
simulation (`fluid::ShipWake`, `FluidGpu::set_ships`, `GpuFrame::set_wakes`; render-only): the hull raises the water ahead of it and
lets it fall behind it, the shallow-water equations carry the bow wave and the wake away (a ship faster than the wave speed
leaves a V). No foam trail (owner). The source is 30 HEIGHT units per second per tile/s of speed (3 shows nothing, 10 was too
faint) and fades with the depth (none on a beach, full from 40 deep: `FluidConfig::ship_full_depth`) so the bow wave stays under the breaking limit. Original water
and the CPU renderer show no wake.

**Zooming** (owner playtest 2026-11-03). (1) The wave window used to be replaced whenever the view's span crossed a 64-cell step, and
a new window starts at rest, so a zoom with a moving ship kept restarting the waves (7 windows over a short zoom sweep): the window in
use now stays while it covers the view without being more than 1.6 times too wide, and a zoom out asks for a quarter more than needed
(2 windows over the same sweep; fact `fluid.windows`). (2) The first zoom out to the whole map waited for the chunks of the far
levels, built from thousands of full-detail chunks, and showed islands cut off meanwhile. In the opening the levels with at most 96
chunks (levels 4-8 of the lan5 map) stay built, and `FrameRenderer::prewarm_far_levels` builds them in the background, one chunk at a
time, whenever the view needs nothing; the chunk workers use three quarters of the cores (they run below normal priority). Measured
on the RTX PC: a zoom out right after the start differs from the settled picture by about 3100 pixels one second later, with a
minute's head start by about 300 (the water's own animation).

## 2. Controls

The table is in the playtest card (it is the contract the window implements). Rules that the code enforces and
the tests check:

- **One gesture, one action.** A press is a click only if it travelled less than 5 px (`kClickSlop`) and was not
  cancelled; a release without a press of ours acts on nothing; focus loss, the pointer leaving and Esc cancel a
  press in progress, and its release is then ignored. A drag is a pan: it never selects and never orders.
- **Authority.** Only the local participant's ships are controllable. Another participant's ship can be
  inspected; a click on it makes no command. The issuer of every command is the participant (`OpeningSetup::local`).
- **Modes exclude each other.** Select, founding and unload; the building tool owns the pointer while it is on
  (`B` leaves the modes). Esc cancels the gesture, then the mode, then the selection, then the window's own quit.
- **Orders are sea orders.** A right click makes a `MoveCmd` to the exact Q24.8 sea point under the pixel; land
  (above the waterline) and outside the world are refused locally with a message. The sim still decides.

## 3. Commands and each command's own state

`PlayRunner` reads the sim's order-id counter before a turn and gives each accepted `MoveCmd`, in order, the
next id (the sim's `ApplyResult` carries none; `ShipOrders::move` is the only increment and a test checks the id
against the one the ship carries); it is recorded on the command's outcome (`CommandOutcome::order_id`). The
`transferred_88` of an `UnloadCmd` and the building a `FoundCmd` created (`created`) come from the sim's own
`RecordResult`s. Nothing is inferred from a later snapshot. `OpeningUi::state_of` then derives, per tracked command and only from the outcome
as of the snapshot it holds (`PlayRunner::outcome_at`, never newer than the view):

`queued → accepted, planning → sailing → arrived | route failed | stopped`, or `replaced by a newer order` (the ship
carries a larger order id and no event ended this one), `ship gone`, `refused (reason)`, `outcome lost`.
Acceptance is not arrival; the event is looked up by this order's id, so a replacement never shows the earlier
order's arrival; `(shown)` is added only when a frame that was delivered (CPU: every stage returned true; GPU: the
present succeeded) was drawn from a view at or after the event's tick, otherwise `(not drawn yet)`.

**Founding.** `F` with an own ship selected; the origin is derived with the sim's rule
(`ox = max(0, cx − (w − 1)/2)`, the footprint of the command's rotation) and the advisory states reach
(`dx² + dy² ≤ 168`), island and footprint-in-island, the price and the ship's goods. Terrain, occupancy and
ownership are not checked: the sim refuses with its reason and the world is unchanged. One click is one command;
the mode ends.

**Unloading.** `U`: the target is the nearest own warehouse of the view, or the one clicked. The request is one hold
(index, goods) and a whole-ton quantity, 1 .. the hold's tons rounded up. The advisory (`in range`, `would move`,
`store a/b`) uses the sim's own pure functions (`ShipCargo::footprint_in_range`, one of the two range predicates: the city must also own a tile of the dock outline, which only the sim checks; `ShipCargo::amount_88`); the sim
decides. The result line shows the actual `transferred_88`, whether it was limited, and the hold before and after.

## 4. Save and resume

`F5`: the sim thread captures `World::save()` and the state hash after the next turn's publication
(`PlayRunner::request_capture`), a writer thread (`OpeningSaver`) writes `saves\quicksave-seat<N>.sav` through a
temporary name and a rename, and the title shows the result (`saved ... (turn, tick, hash)` or `SAVE FAILED: ...`).
A failed write leaves the session and the previous save intact. The file is `anno1503-opening-save 1`: text header
lines (manifest hash and name, seat, rules digest, registry digest, turn, tick, state hash, byte count), a blank
line, the world bytes. `--resume <file>` checks, before anything starts: the frame, the manifest hash, the seat,
the rules digest (against a freshly loaded start of the manifest), the registry digest (against the game
files), that the world loads and that its hash is the stated one; a mismatch names which. The islands of the scene
come from the manifest; selections, pending input and the outcome log start empty. The new recording segment is
identified by `resumed-from` / `resumed-hash` lines. There is no in-game load: resume is a launch option.

## 5. Session record and replay

`ANNO_DEV_SESSION=<new dir>` writes `commands.txt`, `hashes.txt`, `outcomes.txt`, `session.txt` and, for a script,
`script.log`. For an opening `commands.txt` uses the ship-start forms (`commands_file.hpp`): every line names its
issuer and nothing is looked up in the world at parse time:

```
turn 8 found <issuer> <ship> <type> <rot> <x> <y> <click x> <click y>
turn 9 move <issuer> <ship> <target x Q24.8> <target y Q24.8>     turn 10 stop <issuer> <ship>
turn 11 unload <issuer> <ship> <warehouse> <hold> <goods> <tons>
turn 14 buildc <issuer> <island> <city> <type> <x> <y> <rot> ...  turn 15 demolishc <issuer> <island> <city> <x> <y> ...
```

`anno1503 --ship-start M --seat N [--resume S] --commands commands.txt --replay-hashes hashes.txt` replays them in a
fresh process on the same start and refuses (before running) a recording whose `session.txt` names another
manifest hash, seat, rules digest or start state. A changed unload quantity or goods diverges at a recorded
checkpoint. The recorded hashes are the state hash after each turn that ran commands and every 25th turn.

## 6. Tests and evidence

| Requirement | Positive proof | Counterexample / negative control | Evidence |
|---|---|---|---|
| Live ships drawn from the sim, no scene rebuild | GPU equals CPU for the overlay (`live_units` parity test, 23 assertions, with a deliberately wrong input that fails); the ship at its published tile is inspected in the screenshots of `opening_session.py` (by eye: no test asserts the sprite's position, facing or colour, and the hit test shares the layer's projection) | `expect fact generation / committed / discarded same` while sailing; a building change commits a generation | `gpu_test.cpp`, `opening_test.cpp` (CPU blend), `opening_sail.script`, `opening_gpu_window.script` |
| Selection by stable id, other owners inspect only | own ship ordered | seat 1's ship: no command; vanished ship dropped; overlapping ships deterministic | `opening_ui_test.cpp`, `opening_controls.script` |
| One gesture, one action; pan never orders | click orders once | right/left drag, focus loss and Esc between press and release: no command | `opening_ui_test.cpp`, `opening_controls.script` |
| Move/stop through the same queue and codec | `commands.txt`, replay agrees | the replacement does not inherit the first order's arrival; refusal shows its reason; a recording of another manifest, seat, rules or start state, or without `session.txt`, is refused before it runs | `opening_test.cpp` (order ids, events, route failure), `opening_session.py` (`the replay refuses a recording ...`) |
| Found, unload, build with the original supplies | gold 30 000 → 29 350, store and hold ledger | water tile and out-of-reach founding refused unchanged; full store `StockFull` | `opening_resume.script` (CPU and RTX) |
| Replay of the window's own commands | stage 1: 36-37 checkpoints and 1 outcome, stage 2: 13-21 checkpoints and 11 outcomes agree in fresh processes (the stage-2 count varies with the run's timing) | one ton fewer, or another goods, diverges at a recorded checkpoint (turn 11 to 66, again run-dependent) | `opening_session.py` |
| Save / resume | resumed world equals the saved one; stage 3 resumes the save written after founding, unloading and building (stores, hold, ship, building tool) | corrupt, truncated, other manifest / seat / rules / hash refused with the reason. Not tested: a registry-digest mismatch, a tick mismatch and a differing island list (they are checked in `load_opening`); the island comparison covers rectangles and index only, not the terrain | `opening_test.cpp`, `opening_session.py` stage 3 (`opening_resume2.script`) |
| The controls' logic is the code under test | mutated retarget / premature success / pan-issues-move / suppressed move / other-owner-controllable fail their scripted checks | `tools/test/opening_mutation_check.py` mutates `opening_ui.cpp` only: the window adapter (`viewer.cpp`) is not mutated | `docs/orchestration/evidence/w4-opening-1/` |

## 7. Limits

See the playtest card. Additionally: the controls and the publication are tested through the production window on
the CPU renderer (SDL's dummy driver) and on the RTX 4090 (Vulkan); native 4K, a Radeon window, physical display
latency, five-seat load and original-runtime fidelity of the ship facings are not claimed. The sprite for a
facing is frame `dir16` of the figure's first sprite set (measured, section 1; `(12 - dir16) mod 16` in the mirrored picture before 2026-11-03); the opening's
colours are a stand-in. Reporting a ship's arrival as "shown" is the delivery of a frame, not the display.

**Waves through a zoom, second round** (owner playtest 2026-11-03). The window-size hysteresis was not enough: the wave
simulation was also off on every derived zoom level (the third group and farther), and a window of other cells per tile (a zoom
across a detail level) still started at rest. The simulation now runs on the derived levels too (the coarsest built group's cells,
halved by the cells-per-pixel bound), and a window switch carries the old waves over: `fluid.wgsl` `resample_state` fills the new
window from the old one, bilinear in the tile plane, where the old one had cells (two cells in from its edge), and from the new
window's own initial state elsewhere and on land (`FluidGpu::carry_waves_over`, on by default; `GpuStats::fluid_resamples`, fact
`fluid.resamples`). Test: `the GPU waves carry over a window switch` (the wake keeps 97 % of its mean elevation over a change from
4 to 2 cells per tile; the control without it keeps 0). Screenshots of the wake at six successive zoom-out notches show it in all.

**Third round** (owner playtest 2026-11-03).
- *Founding.* Of the 1458 clicks within the ship's reach at the session's coast, the sim accepts 84; 1050 are `NoRotationFits`. A
  quarter of the coast sites the terrain allows are taken by objects (measured over every origin of the real start: 25205 pass the
  coast rule, 6346 of them are occupied in every passing rotation), and the preview checked the terrain only, so it said yes where
  the sim said no. The preview now asks the sim thread (`PlayRunner::request_found_probe`, `probe_founding`: Founding::apply's checks
  up to the rotation trials, read-only) and shows its answer a turn later: the warehouse is drawn only where the sim would build it,
  turned the way the sim would turn it, and the title names the reason otherwise (fact `found.sim`). Test: `founding probe, real files`
  (probe and apply agree on all 1458 clicks but 2, where the figure veto after the trials refuses; the veto is not probed).
- *Wake shape.* The gaussian hull reached ahead of the stem, so the water rose in front of the ship. The source is now confined to
  the hull's length: it piles the water up at the bow and lets it fall back at the stern (`FluidConfig::ship_stern_share` 0.6), nothing outside.
- *Waves blinking on repeated zooms.* Every window switch restarted the 250 ms cross-fade from the analytic waves, also when the old
  waves were carried over; a carried switch no longer fades.

**Fourth round** (owner playtest 2026-11-03, with screenshots of the original's Kontor placement).
- *No manual turn.* The owner, from the original: a Kontor has no manual rotation, it aligns with the coastline by itself. `T` no
  longer turns the founding cursor; every `FoundCmd` carries `kFoundingCursorRotation` (0) and the sim's rotation trials turn the
  warehouse to the shore (the first of 0, 1, 2, 3 whose land edge is the beach and whose opposite edge is in the water). Which
  rotation the original's own command carries is not traced (`docs/sim/ship-founding.md` §3.5). *Superseded the same evening:*
  the orchestrator's review traced it (the original's Kontor cursor starts the trials at its own turn byte, set by the turn keys),
  and the owner restored it: `,` (or this window's `T`) turns the founding cursor on, `.` back, the command carries that turn
  and the sim's trials start there (`kFoundingCursorRotation` is the turn before any press). The directions are the original's
  (Wine runner, 2026-11-04, OQ-520: under `.` its cursor's turn went 0, 3, 2, under `,` 2, 3, 0, 1); the first build of the
  key had them the other way round. The warehouse still turns to the coast by itself;
  the cursor decides only where two turns fit (8 of 115 shipped sites). Test `controls: on a corner where two turns fit, the
  founding cursor's turn decides`.
- *Turned and placed wrong: the picture, not the sim.* Every Kontor of the original's scenario files (62) passes the sim's coast
  rule at its stored rotation and origin (test `the original's own warehouses pass the coast rule as stored`), and the session's
  coast at (255,138) has land at −x and water at +x, where only rotation 1 passes. What looked wrong is the renderer: its
  projection is the mirror image of the original's. The original's camera (AnnoFrame `0x100712c0`, Ry =
  [[c,0,−s],[0,1,0],[s,0,c]], a = 135° + 90° per view rotation; AnnoDisplay `0x10011e60` projects with column 0 as a row vector)
  puts world +x to the lower left and +y to the lower right at every view rotation; `tile_top` puts +x to the lower right. In 68
  of the 69 non-square buildings every sprite rotation block fits the transposed footprint on our screen, original towns show
  their fences and hedges as combs, and the Kontor's piles stand along the shore. Drawn in the original's handedness (an
  experiment, not committed) the same tiles show continuous fences and the Kontor's piles in the water. Fixed 2026-11-03 on
  `task/iso-handedness-W4` (from `main`'s `claude/fervent-booth-e22bf1`): the projection everywhere (CPU, GPU, picking, ship facing
  frames; the ground texel orientation re-run is unchanged), `docs/render/iso.md` "Handedness"; nothing is patched per sprite.
- *The overview after a founding.* A founding or a building marks the chunks over its island stale at every zoom level; a stale
  chunk keeps drawing until its replacement arrives, and the background prewarm built missing overview chunks only, so the next
  zoom out rebuilt the island's whole overview stack on demand. The prewarm now also rebuilds a stale resident chunk (test
  `prewarm: a resident overview chunk marked stale is rebuilt in the background`; it fails without the change, and each stale
  chunk is rebuilt once). This is the same work the next zoom out did, done earlier and only while the view needs nothing; it
  reaches down to the island's full-detail chunks. Its cost on the opening map was not measured (the test prints the time for one
  original island). A target whose build fails is given up instead of stopping the prewarm (test `prewarm: a resident chunk that
  cannot be built does not stop the rest`). Whether that is
  all of "water and islands need a moment, islands flicker at the farthest zoom" is not known: the report line of the window
  now says how many frames drew a visible chunk not built yet (and at which level) and how many wave windows were made or
  carried over, so the next log can tell.

**Fifth round** (owner playtest 2026-11-03: zooming while the ship sails works; artifacts at the farthest zoom and, new, after
zooming back in; trees over the founded Kontor). Root causes measured headlessly (the viewer's cache on the opening map through a
zoom out and back in; GPU frames of the same path; a synthetic beach for the waves), then fixed one by one:
- *Trees in the Kontor.* The shipped scenarios hold no vegetation under any of 65,619 buildings (175 Kontors); the original writes
  a building over every footprint tile (AnnoWelt.dll `0x100823ed`), the sim does too, but the picture copied the file's scenery
  unchanged. `apply_island_buildings` drops a scenery item whose anchor lies under a building (flag-0x10 exception as the
  original's, `0x1008218f`); an item anchored outside stays whole (OQ-551). The island's covered tiles are marked once per
  rewrite: a test per item and building took 63 s on `endless02.sze`'s island 1 (1,854 buildings, 84,486 scenery items),
  509 s over the 226 shipped islands with buildings; now at most 10 ms, 148 ms in all (review). Over those islands 14
  scenery items are dropped and 123 anchored under a building are kept (by the flag-0x10 exception, or for a type without a
  flag word; the test does not tell the two apart) (test `the shipped scenarios' islands drop exactly the
  covered scenery`).
- *Open sea built over and over.* 39,000 chunk builds in two seconds at the farthest zoom were open sea around the map: a far
  chunk was reduced from its 4^n children wherever it lay. A chunk outside the scene is now open sea at once (the first 12 s of
  the replay: 1,953 builds, 10,953 before; none while resting at the farthest level).
- *Level 4 never kept.* The viewer kept far levels of at most 96 chunks, assuming 91 at level 4; the map has 98, so every zoom
  through level 4 rebuilt it from group 2 (up to 14 visible chunks missing in the owner's log). `kOpeningResidentLevelChunks`
  = 128 (levels 4-8, 470-665 MiB of cache): the replay then has no frame with a visible chunk missing, out or back in.
- *White stepped patches after zooming in.* The far levels' waves (1-2 cells per tile) carried into the close window
  bilinearly: steps in the lighting a coarse cell wide, the coarse surf as blobs off the shore. Into finer cells the carry is
  now a cubic B-spline without foam (docs/render/iso.md, "A zoom carries the waves").
- *The report line* also counts frames that dissolved chunks in over a stand-in (built, then uploaded), which "not ready"
  did not show.
- Not changed: a chunk reaching the GPU dissolves in over its stand-in (the nearest level magnified) for 150 ms
  (`chunk_fade_ms`); the old wave window draws the finer view for the 10-40 ms its successor takes to prepare; surf over the
  offshore shoals is our simulation's (the original shows lighter water there), a look the owner decides; the mirrored
  projection (the Kontor's turn) is its own task.
- Known, from the reviews (low): `sync_scene` in the compositor is set and reset by hand around the chunk sync (no scope
  guard); the GPU path ignores `build_outside_chunks`; a malformed item outside its island is kept by the tile mask.

**Review follow-ups** (orchestrator review of rounds 1-7, `docs/orchestration/reviews/2026-11-03-w4-opening1-playtest.md` §3).
Each fix has a test that fails without it (mutation checks: the fix reverted, the test run, the file restored).
- *P1, out-of-bounds read.* `update_fluid` raised the wave window's centre by `Lift::scaled(kSeaLevel, 1, group)`, which reads
  `kTile[group]`: past its three entries at every derived level, where the waves run since round 2. It takes
  `ZoomLevels::lift_scaled` now, as the frame uniform does. `GpuStats::fluid_centre` reports the centre; test `the wave window at
  a derived zoom level is centred as at its built ancestor` (levels 3 and 4 against level 2, a large lift: 0.1 tile apart, the
  derived lift's rounding).
- *P2, prewarm give-up.* The prewarm gave its target up whenever the last submit left nothing pending; a child finishing between
  `take_done` and `submit` is skipped by the submit (done, not taken), so the target was given up one frame before it could be
  built. It is given up now only when a job of its plan has failed (`ChunkWorkers::any_failed`, never retried). Test `prewarm: a
  child finished between take_done and submit does not give the target up` (the seam `ChunkWorkers::set_test_before_submit`
  lets the gated child finish inside the gap; the target is rebuilt once).
- *Turn clock.* A new target in the middle of a turn restarted the turning clock; targets that change on every publication held
  the facing back for good. The clock starts only when a ship showing its facing is asked to turn.
- *Chunk count.* The report line's `levels.chunks` truncated its division (negative scene x); it counts with
  `render::visible_chunks` now, as the renderer. The "91 at level 4" above was that count; the resident bound was set from the
  test's 98.
- *Probe drift.* Not changed here: one const probe belongs in `sim` (the MAP lane's), `Founding::apply` calling it before the
  figure veto and `probe_founding` forwarding to it. Until then the real-file comparison (1458 clicks) guards it; it is skipped
  without the game files. The checks it leaves out are listed in its doc (`opening.hpp`), `NotAShip` now among them.
- *Wave carry without a CPU reference.* `FluidGrid::carry_from` is the CPU reference of `resample_state`; test `the GPU wave carry
  over a window switch matches the CPU reference` (GPU against CPU 7e-7 into finer cells, 3e-8 into coarser ones).
- *Wake limit and constants.* `kMaxShipWakes` (16) has its reason and the path to raise it in its comment (a storage buffer); of
  more ships the 16 nearest to the wave window's centre stir the waves (`ShipMotion::wakes`, which also no longer lists ships
  that do not move). The shader's copies of the minimum speed, the stern share and the full depth are `FluidConfig` fields it
  reads from its uniforms (`ship_min_speed`, `ship_stern_share`, `ship_full_depth`).
- *Doc and code.* This document said the wake is full from 32 deep; it is 40 (`ship_full_depth`). The enhanced depth is at most
  16 (below the waterline), so on the maps a ship's source acts at up to 0.4 of its gain: tuned that way by eye.
- *Water override.* An explicit `--water` is kept by the opening (`Args::water_given`, `OpeningHooks::enhanced_water`); without
  it the opening still starts with enhanced water on the GPU.
- *Test gaps.* New: the carried switch keeps the waves at full weight (`GpuStats::fluid_fade`; test `a wave window switch that
  carries the waves over does not fade them in again`); `GpuFrame::set_wakes` reaches the waves (`the ships set on a frame stir
  its simulated waves`); `ShipMotion::wakes`; the founding preview's branches (`controls: the founding preview's advisories, the
  coast rule and the sim's answer`). The real-file founding-sites test reads the preview's heights as the window does
  (`scene_vertex_height`) and requires both verdicts to agree at every origin and rotation (they do: 25,205 coast sites each).
  It showed the window had no height on an island's far column and row, where the sim reads its boundary height 0; the window
  reads it now (test `founding preview heights: the island's far column and row read the sim's boundary`).
- *Re-review of these fixes* (independent, read-only: no P1/P2). Acted on: the P2 test also runs on a synthetic island (CI;
  the game-files case stays), and its gate state outlives the workers with a guard that opens the gate whatever ends the test;
  the ship solver test runs with tuned wake settings and a slow ship as well (a literal left in the shader fails it: three
  mutants); the carry test's bound is 1e-5 (measured 7e-7 and 3e-8) and it requires foam in the old window into finer cells.
  Recorded, not changed: the wake settings are `FluidConfig` fields, not a runtime option (the compositor creates the solver
  with the defaults, as for every other field); a ship slower than `ship_min_speed` still takes a wake slot; the Rust
  reference `crates/render/src/gpu_fluid.rs` loads the same `fluid.wgsl` but binds only 0-4 of its 0-7 bindings (since
  `8cae6f71`, before this branch), so its GPU fluid pipelines cannot validate against it: to drop with the Rust code
  (ADR 0001).
- *Round 7's waterline (H1/H2), after the review.* The original does not reshape the ground when it places buildings (Wine
  runner: the runtime height grids unchanged over 17 placements); it draws corners at 16 as water at a Kontor's piles. With
  16 as water our Kontors gain water at their water edge, but not all the way to the piles the original shows in water
  (hand-over note, section 4). The owner chose it as the default (2026-11-03): terrain below `kShoreHeight` = 16.5 is water
  (the field, the wave depth, the shore distance, the empty-cell bound; the water objects' split under and over the sea stays at
  16), per scene (`Scene::shore_height`), and `--shore 16` draws corners at 16 as land again. On 2026-11-04 the owner
  chose the original's measured sea plane, 26.5, as the default to try (W4-DEPTH-1, `docs/render/depth.md` §2.3;
  `--shore 16.5` for the look before). Why the Kontor still floats (its
  sprite is drawn whole over the terrain and the water, where the original depth-tests every pixel against them) is a renderer
  task of its own (OQ-026 c).
