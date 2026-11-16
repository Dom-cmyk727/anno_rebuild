# UI-SHIPS-DIPLO-1 (draft): the ship view and the diplomacy screens

**DRAFT 2026-11-16, not released.** Prepared by the UI-PANELS-PREP research pass. Spec:
[`docs/re/ui-ships-diplomacy.md`](../../re/ui-ships-diplomacy.md) (sections §N below refer to it). The UI lane
releases the stages one at a time (owner rule 2026-11-12: one concern, about 2-4 h per packet; owner tests
each step in the viewer before the next). Stages marked **sim** need a sim packet first (the Mac lane plans
them from spec §4); the UI stage after it only wires the button.

## Why

- The rebuild selects ships and has load/unload/found/stop keys in the developer ship-start mode
  (`docs/app/ship-opening.md`), but no original ship view, no ship list rows and no diplomacy screens. The
  player list chrome is drawn (UI-WINDOW-1 R2), its rows, the diplomacy cluster and the treaty buttons are not
  (`docs/re/ui.md` §5.3, UI-TABS-1).
- Spec §2 gives the original's gadgets (D), the graphics of both editions (P: pixel-identical, ids shifted)
  and HE-measured positions (±2 px); §4 lists what the sim lacks.

## Ground rules for every stage

- HE is the picture reference; GOG `View.dll` gives the addresses; game files of both editions. Bit-identical
  original UI first; differences later as options (owner 2026-11-14/16).
- Positions go into `data/ui/hud.txt` (new panels `ship_status`, `ship_behaviour`, `ship_sell`, `ship_list`,
  `ship_route`, `player_info`, `diplomacy`, …) with method and shot named; a ±2 px video measurement is
  marked as such and replaced by an exact one when a lossless HE screenshot exists.
- No new limit: list lengths (14 ship rows, 6 partner rows, 5 route rows, 8 tabs, 12 crew slots) come from the
  layout data, not constants; the hold count comes from the ship's type.
- No original pixels, frames or texts in git; captures under `cache/` or `E:\anno1503-worktrees\he-ui-captures`.
- Evidence contract (`docs/testing/evidence-contract.md`): executed passes, skips and NOT_RUN named apart.

## Stages

### 1a — Evidence: lossless HE captures and the missing layout reads (2-3 h, no code)

Do: run the owner's recordings of §R below through a frame/screenshot extraction; measure every element of
spec §2 at exact scale (method sprite-565 like `hud.txt`, HE ids) on lossless screenshots; read in GOG
`View.dll` where gadget positions are set (spec OQ-661: the builders set only sizes) and the load/unload tab
layout (OQ-662). Update spec §2 rows from "M-HE ±2 px" to exact, and close or narrow OQ-660, -661, -662, -668.
Accept: every element of S0-S8 and P0-P2d has an exact position on a named HE screenshot (or stays U with a
reason); the reviewer re-measures three elements and gets the same pixels.
HE comparison: the screenshots themselves.

### 1b — Ship view chrome, static (2-3 h)

Do: selecting an own ship (the existing selection in `opening_ui.cpp`) shows the ship view in the right panel
under the main tab the original uses (HE: "?" highlighted; OQ-660), header plank + emblem + 8-tab column with
the right selected/idle/disabled frames, and the selected sub-tab's static background (`VIEWINFOSCHIFFMAIN_BK`,
`…KAMPF_BK`, `…VERKAUFEN_BK`, `SCHATTENPLATTE` + list rows, `ROUTE_SCHTEENPLATTE` + `ROUTE_HINTERGRUND`,
`…KAUFEN_BK`). Tab clicks switch sub-tabs (spec §1, handler `0x10057420`: one index, others hidden). Click on
empty sea deselects (HE 00:50).
Accept: per sub-tab, the rebuild's 1024 × 768 frame equals the HE screenshot of the same sub-tab inside the
static elements (RGB565-exact, `tools/test/ui_panels_compare.py` style), names and figures masked; a click on
each tab selects only that tab; the existing UI oracle tests stay green.
HE comparison: §R steps 2-4 screenshots.

### 1c — Ship status, live (2-3 h)

Do: cargo grid (one slot per hold, `hold_count_a` from the ship's type; goods icon or empty slot; fill bar),
crew grid (children with `crew_slot`), cannon slot (count missing in the sim: draw the slot, count shown only
once the sim has it), ship name (missing: placeholder rule stated, OQ-668), the "Build warehouse" button
(arms the existing founding mode, spec §2.2: type 1302) and "Throw cargo overboard" drawn disabled until 1i.
Snapshot: add to `ShipView` what spec §4 marks "missing in `ShipView`" (capacity, hold count, crew, health).
Accept: for the ship-start manifest the four holds (wood 50, food 10, tools 50, tools 30) show their icons in
hold order and the empty slots after them; a unit test unloads 10 t and the bar shrinks; "Build warehouse"
enters founding mode exactly as key F does (same `FoundCmd` in `commands.txt`).
HE comparison: §R step 2 (status tab with a full and a partly empty hold).

### 1d — Ship lists and the player view tabs (2-3 h)

Do: the ship list (ship view tab 5 and player view tab 3, key **S**): 14 rows, name, route icon, health bar,
scroll arrows; the player view's five tabs (key **D** → tab 1, key **S** → tab 3, key **C** → tab 2), the
player info rows from the city accounts. Row click per OQ-668 once 1a has settled it.
Accept: `f15_ship_list` (GOG) and the HE ship-list screenshot reproduced inside the list area with names masked;
keys D/S/C select the right tab in the classic surface.
HE comparison: §R steps 5 and 7.

### 1e — Load / unload tab (2-3 h; needs 1a's layout)

Do: wire the tab to the existing `UnloadCmd` / `LoadCmd` (spec §4): hold selection, the four amount balls
(1/5/10/50, default 10, `[view+0x640]`), category tabs and the city goods list; tab greyed when no own
warehouse is in range (`ShipCargo::footprint_in_range`).
Accept: an unload of 10 t from the tools hold and a load of 5 t wood produce the same `commands.txt` lines as
the developer keys; the result line matches the sim's `transferred_88`; HE screenshot of the tab matched.
HE comparison: §R step 3.

### 1f — Trade-route editor (3-4 h; needs OQ-669 settled in 1a)

Do: route-point list (5 rows, "New entry"), set/delete point, activate toggle, the four action tabs and goods
slots, mapped onto `AssignFreightRouteCmd` / `ControlFreightRouteCmd`.
Accept: a two-stop route built by clicks equals the route a unit test builds directly; activate/deactivate send
Resume/Suspend; HE screenshot of the editor with two stops matched.
HE comparison: §R step 6.

### 1g — Diplomacy list and treaty buttons, read-only (2-3 h)

Do: partner rows (name, player or culture shield by `Player::kind`, mood bar placeholder until OQ-664),
"Unknown" rows, scroll arrows, the action tab row, and the three treaty buttons drawn in the state derived
from `Player::relations` (rule from OQ-664), disabled when no treaty is possible.
Accept: GOG `f15_player_menu` and the HE diplomacy screenshots reproduced (rows masked by name); a unit test
sets each relation byte combination and checks the frame index of each button.
HE comparison: §R steps 8-9.

### 1h — sim: diplomacy commands (Mac lane; spec §4)

`TreatyCmd{other, kind, value}`, `TributeCmd{…}`, `DiplomaticMessageCmd{other, message}` with the world rules
(AnnoWelt `0x100ad280` and the tribute/message functions, OQ-664..666), offers as state, replay codec entries.
Then a UI stage wires the buttons (state machine of spec §2.11, D) and the tribute/message panels.

### 1i — sim: ship verbs (Mac lane; spec §4)

`JettisonCmd`, `SetFlagCmd`, `SetPatrolCmd`/`ClearPatrolCmd`, `ScuttleCmd`, `BoardCmd`, `RepairCmd`,
`CannonsCmd`, `OfferShipSaleCmd`/`WithdrawShipSaleCmd`, `BuyShipCmd`, each with its original record and world
function first read (OQ-663, OQ-667). Then UI stages wire the behaviour and sell tabs.

## §R — HE recordings for the owner (closes the visual gaps)

Setup (once): HE options → windowed, resolution **1024 × 768** (or any height ≤ 1200, which forces UI scale
1.0), sounds and music off. Record the window with OBS at 60 fps, lossless or high quality, **and** press the
Steam screenshot key (F12, uncompressed PNG enabled in Steam settings) at every "SHOT" below, with the mouse
moved off the panel first (a cursor over a button hides pixels; hover shots are separate). Say each step aloud
("now the behaviour tab") so the transcript times it. Total about 15-20 minutes. In this recipe sub-tabs are
counted **from 1, top to bottom** (the stages above and the spec count from 0).

1. **Start** (2 min): Endless game with **computer opponents** (custom endless setup), so that diplomacy has
   players, not only cultures. Note the scenario and settings aloud.
2. **Ship status** (2 min): select your ship by a **single left click** (say it), SHOT; then deselect by
   clicking open sea, SHOT; reselect by drag-select, SHOT. Hover each cargo slot, the cannon slot, "Build
   warehouse", "Throw cargo overboard" for 2 s each (tooltips).
3. **Load / unload** (3 min): found the Kontor, keep the ship in range. Open tab 4 (map icon), SHOT; click each
   goods category tab, SHOT each; click each amount ball (1, 5, 10, 50), SHOT once; select a hold, click
   unload, SHOT; select wood in the Kontor list, click load, SHOT. Move the ship out of range, SHOT (greyed).
4. **Behaviour** (2 min): tab 2, SHOT; hover each of the five icons 2 s; click the white flag on and off, SHOT
   after each; click patrol on, then click two sea points if it asks, SHOT; patrol off. Do **not** click sink.
5. **Lists** (1 min): ship view tab 6 (sails + "?"), SHOT; press **S**, SHOT; click the ship's row, say what
   happens; press **C**, SHOT; press **D**, SHOT.
6. **Trade route** (4 min): tab 8 (arrow), SHOT; set a route point at your Kontor (say how), add wood
   "Load I" 10 t, SHOT; click Load II, Unload I, Unload II tiles, SHOT each; set a second point, activate the
   route, SHOT; deactivate, delete one point, SHOT.
7. **Sell / buy** (1 min): tab 5 (sails + coin), SHOT; move the price slider to the middle, SHOT; hover the
   offer button (do not offer); tab 7 (coin + "?"), SHOT.
8. **Diplomacy after contact** (3 min): sail until a computer player's city is discovered. Bust tab → tab 2
   (scroll), SHOT; click a computer player's row, SHOT; hover the three treaty buttons 2 s each; click "offer
   trade agreement", SHOT; click it again (withdraw), SHOT.
9. **Tribute and messages** (2 min): action tab 2 (demand tribute), SHOT; move the money slider, click an
   empty goods box, SHOT; action tab 3 (pay tribute), SHOT; action tab 4 (messages), SHOT, hover each of the
   five lines; send "Ingratiate" once, SHOT, and say whether the player answers.

Hand-over: the video, the PNG folder, and a line per SHOT with its step number (the narration is enough).

## Base, branch, gates

Base: main at release time. Branch `task/UI-SHIPS-DIPLO-1<stage>`. Gates per stage: the changed lanes only
(`docs/orchestration/TASKS.md` §1 item 4; owner rule 2026-11-13: refused tests rerun alone, never a whole-gate
rerun); the UI oracle tests; `tools/check_unix_warnings.sh` for code stages. Report READY FOR REVIEW at the
bottom of `docs/orchestration/REPORTS.md`; the UI lane reviews, landing stays with the orchestrator.
