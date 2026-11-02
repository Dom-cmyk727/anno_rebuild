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
Interpolation is not used: a ship is drawn at its published position (10 turns a second).

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
next id; it is recorded on the command's outcome (`CommandOutcome::order_id`), with `transferred_88` of an
`UnloadCmd` and the building a `FoundCmd` created (`created`) from the sim's own `RecordResult`s. Nothing is
inferred from a later snapshot. `OpeningUi::state_of` then derives, per tracked command and only from the outcome
as of the snapshot it holds (`PlayRunner::outcome_at`, never newer than the view):

`queued → accepted, planning → sailing → arrived | route failed | stopped`, or `replaced by a newer order` (the ship
carries a larger order id and no event ended this one), `ship gone`, `refused (reason)`, `outcome lost`.
Acceptance is not arrival; the event is looked up by this order's id, so a replacement never shows the earlier
order's arrival; `(shown)` is added only when a frame that was delivered (CPU: every stage returned true; GPU: the
present succeeded) was drawn from a view at or after the event's tick, otherwise `(not drawn yet)`.

**Founding.** `F` with an own ship selected; the origin is derived with the sim's rule
(`ox = max(0, cx − (w − 1)/2)`, the footprint of the cursor's rotation) and the advisory states reach
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
| Live ships drawn from the sim, no scene rebuild | the ship at the published position in the CPU frame and the RTX window; GPU equals CPU for the overlay | picture counters unchanged while sailing; a building change commits a generation | `opening_session.py` screenshots, `live_units` parity test, `opening_workload` counters |
| Selection by stable id, other owners inspect only | own ship ordered | seat 1's ship: no command; vanished ship dropped; overlapping ships deterministic | `opening_ui_test.cpp`, `opening_controls.script` |
| One gesture, one action; pan never orders | click orders once | right/left drag, focus loss and Esc between press and release: no command | `opening_ui_test.cpp`, `opening_controls.script` |
| Move/stop through the same queue and codec | `commands.txt`, replay agrees | the replacement does not inherit the first order's arrival; refusal shows its reason | `opening_test.cpp` (order ids, events, route failure), `opening_session.py` |
| Found, unload, build with the original supplies | gold 30 000 → 29 350, store and hold ledger | water tile and out-of-reach founding refused unchanged; full store `StockFull` | `opening_resume.script` (CPU and RTX) |
| Replay of the window's own commands | 36 / 13 checkpoints, 1 / 10 outcomes agree in fresh processes | one ton fewer, or another goods, diverges | `opening_session.py` |
| Save / resume | resumed world equals the saved one | corrupt, truncated, other manifest / seat / rules / hash refused with the reason | `opening_test.cpp` |
| The window's wiring is the code under test | mutated retarget / premature success / pan-issues-move fail their scripted checks | see the report | mutation record |

## 7. Limits

See the playtest card. Additionally: the controls and the publication are tested through the production window on
the CPU renderer (SDL's dummy driver) and on the RTX 4090 (Vulkan); native 4K, a Radeon window, physical display
latency, five-seat load and original-runtime fidelity of the ship facings are not claimed. The sprite for a
facing is the frame `dir16` of the figure's first sprite set (assumed in `docs/render/iso.md`); the opening's
colours are a stand-in. Reporting a ship's arrival as "shown" is the delivery of a frame, not the display.
