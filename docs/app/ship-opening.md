# The live ship-start opening (W4-OPENING-1)

Status: **implementation in progress** on `task/W4-opening-1`. This is a bounded developer playtest UI
for the accepted MAP opening (ship start, sail, found, unload, build), not UI.2, the game menu,
network multiplayer, trade routes, combat or renderer optimisation. Owner launch and play steps are in
[`docs/testing/ship-opening-playtest.md`](../testing/ship-opening-playtest.md).

Inputs (fixed integration pins, see the report): Windows `26107db0` (verifier accepted) and Mac
`0c401c8d` (MAP-BUILD-1 + MAP-CARGO-1). The sim commands are consumed as they are: `MoveCmd`,
`StopCmd`, `FoundCmd`, `UnloadCmd`, `BuildCmd`. Nothing here decides a rule; the sim refuses.

## 1. Architecture

| Piece | Where | Rule |
|---|---|---|
| Launch | `anno1503 --ship-start <manifest.json> --seat N [--resume <save>] [--game-dir ..]` | `sim_load::load_ship_start` through the typed manifest; developer start and developer resources off; no legacy fixed-map loader. Manifest bytes, content hash, rules digest, seed and the seat are recorded in the session. |
| Sim thread | `PlayRunner` (unchanged ownership) | Publishes an immutable `OpeningView` inside each `PlaySnapshot`: ships, the local participant's cities and stock, gold, and a bounded list of ship events. No `World`, pointer or registry slot leaves the sim thread. |
| Window thread | `Viewer` + `OpeningUi` | `OpeningUi` is window-free logic fed with abstract input and the published view; it makes commands and one status line. The viewer maps SDL events and the drawn frame to it. |
| Ships on screen | `ShipLayer` + `render::UnitSprite` overlay | Original figure frames (FigurenHeader entry of the ship's kind), 16 facings, player colour, shadows. Moving ships are an overlay list handed to the CPU and GPU frames per frame; no scene generation, chunk or island rebuild happens for a sailing tick. |

Identity of ships and targets is always the stable `FigureId` / `BuildingId` / `CityRef`, re-read from the
newest snapshot when a command is made. A selection or a pending target whose id is absent from the
newest snapshot is dropped with a message.

## 2. Controls

Existing bindings are kept: `W A S D`/arrows pan, wheel zoom, `+`/`-`/`0`, right-drag pans, `E`, `R`, `Space` pause,
`B` building tool and its keys (`N P T X`), `Esc`/`Q` quit. New bindings only use free keys.

| Input | Mode | Effect |
|---|---|---|
| left click (moved < 5 px) on a ship | select | selects it (own ship: controllable; other participant's: inspect only). Click on empty sea or land: deselect. |
| left drag | select | pans as before (≥ 5 px; the press is consumed once, no selection on release) |
| right click (moved < 5 px) | select | `MoveCmd` for the selected **own** ship to the sea point under the pointer. Land, outside the world or no own selected ship: a labelled refusal, no command. |
| right drag | any | pans as before; never issues an order |
| `H` | select | `StopCmd` for the selected own ship (`S` stays a pan key) |
| `F` | select → found | founding mode for the selected own ship. The warehouse sprite and footprint follow the pointer; `T` rotates (the cursor's initial rotation); left click sends `FoundCmd` for the clicked tile; the sim decides. |
| `U` | select → unload | unload mode: the target is the nearest own warehouse in the newest snapshot (left click on a warehouse picks another own one); `Tab` next hold; `[` `]` quantity −1 / +1 ton (`Shift`: 10); `Enter` sends one `UnloadCmd` for exactly that hold and quantity. |
| `B` | any | the existing building tool; it follows the founded city. `Esc` cancels its press first. |
| `F5` | any | quick-save at the next turn boundary (file written off the window thread) |
| `Esc` | any | cancels the current gesture, then the mode, then the selection, then (nothing left) quits |
| focus lost / pointer leaves | any | cancels an incomplete gesture (no order is issued on a later release) |

One click is consumed by exactly one mode. Founding, unload and the building tool are exclusive: entering one
leaves the others. The window title is the HUD (there is no text renderer yet): mode, selected ship (id, owner,
cargo), the command's own state and the real refusal.

## 3. Acceptance table (evidence contract)

| Requirement | Positive proof | Counterexample / negative control | Evidence | Disposition |
|---|---|---|---|---|
| Live ships drawn from the sim, no scene rebuild | ship overlay frame shows the ship; a sailing run leaves the picture's commit/rebuild counters unchanged | counters grow when a building changes | `ship_layer`, `opening_scene` tests; work counters | pending |
| Selection by stable id; other owners inspect only | own ship selected and ordered | other owner's ship selected: no order; stale id dropped | `opening_ui` tests | pending |
| Move/stop through the same queue and codec | accepted order, arrival event correlated by `order_id` | refused order shows its reason; a replacement does not inherit the first order's arrival | `opening_ui` / `opening_session` tests | pending |
| Found, unload, build with original supplies | headless adapter run and one real-window script | wrong owner, out of range, full store, bad terrain, stale selection | scripted sessions | pending |
| Replay of the window's own commands | fresh process agrees at every checkpoint | changed unload quantity diverges | `--replay-hashes`, session files | pending |
| Save / resume | resumed world equals the saved one | corrupt file leaves the running session intact | `opening_session` tests | pending |

Limits are recorded in the report and the playtest card; this is not full multiplayer, native-4K, late-game
performance or original-runtime fidelity acceptance.
