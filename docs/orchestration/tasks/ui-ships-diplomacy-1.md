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

Order: the evidence stages 1a-1 … 1a-4, then the UI stages 1b … 1g; the sim tasks (1h-*, 1i-*) are the
sim lane's and each is followed by a small UI stage that wires its buttons. (Fix round after the
orchestrator's review 2026-11-16: the first draft's 1a, 1d, 1h and 1i were too large; 1f and 1g needed
answers, OQ-669 and OQ-664, that no earlier stage read.)

**Sub-tabs are UI-TABS-1's.** UI-TABS-1 (branch `task/UI-TABS-1`, in progress) builds the sub-tab
switching of the player, city and options views and the keys D / S / C: `data/ui/hud.txt` gives each
sub-tab a `body`, the app shows the selected sub-tab's body, and D, C, S show the player view at
sub-tabs 1, 2, 3 (`docs/re/ui.md` section 5.4 on that branch: AnnoFrame `0x1000bfc4`, View
`0x1007e9c0`, D; M-GOG `f15_player_menu`, `f15_city_list`, `f15_ship_list`). The stages below build no
tab switching or key handling of their own; their pages are bodies plugged into UI-TABS-1's views. The
ship view's eight sub-tabs (handler `0x10057560` → `0x10057420`, spec §1: the same "index `0x1b3`,
store at `[view+0x5f0]`, show one page" rule as the three views of section 5.4) are added as a fourth
view in UI-TABS-1's data, not as new switching code.

### 1a-1 — Evidence: positions from the lossless HE recordings (2-3 h, no code)

Do: extract the frames and SHOT PNGs of §R below; measure every element of spec §2 at exact scale
(method sprite-565 like `hud.txt`, HE ids) on them; settle the selection gesture, deselect and host tab
(OQ-660) from the narration. Update spec §2 rows from "M-HE ±2 px" to exact.
Accept: every element of S0-S8 and P0-P2d has an exact position on a named PNG (or stays U with a
reason); the reviewer re-measures three elements and gets the same pixels.
HE comparison: the recordings themselves.

### 1a-2 — Evidence: load/unload layout and ship-list rows, code reads (2-3 h, no code)

Do: read in GOG `View.dll` the position source of the ship view's gadgets (OQ-661, OQ-651: the builders
set only sizes), the load/unload tab builder `0x10059d00..0x1005a5c0` (OQ-662) and the ship-list row
update and ship naming (OQ-668).
Accept: each OQ answered with a D row or narrowed with the next step; no build.

### 1a-3 — Evidence: trade-route editor mapping, code reads (2-3 h, no code)

Do: read the route handlers (`0x10028bf0`, `0x10028c90`, `0x1002ac90` / `0x1002aca0`, `0x1002acb0`) to
the route node writes; map "Load I / II, Unload I / II", the goods slots, amounts and `ROUTEAKTIV` onto
`FreightStop` / `FreightAction` / `FreightControl` (OQ-669). 1f needs this.
Accept: as 1a-2.

### 1a-4 — Evidence: treaty button state and the partner list, code reads (2-3 h, no code)

Do: read where the diplomacy page writes the buttons' property `0x1cc` on update and from which relation
bytes of both players; the disable rules; the mood bar `STIMMUNG_PLAYER` rule; the list order (OQ-664).
1g needs this rule (the first draft left it to the later sim stage 1h).
Accept: as 1a-2; the state derivation written as a table in spec §2.11.

### 1b — Ship view chrome, static (2-3 h)

Do: selecting an own ship (the existing selection in `opening_ui.cpp`) shows the ship view in the right
panel under the main tab the original uses (HE: "?" highlighted; OQ-660), header plank + emblem, and the
eight sub-tabs as a fourth view in UI-TABS-1's data with their selected/idle/disabled frames and each
sub-tab's static background as its `body` (`VIEWINFOSCHIFFMAIN_BK`, `…KAMPF_BK`, `…VERKAUFEN_BK`,
`SCHATTENPLATTE` + list rows, `ROUTE_SCHTEENPLATTE` + `ROUTE_HINTERGRUND`, `…KAUFEN_BK`). Click on empty
sea deselects (HE 00:50).
Accept: per sub-tab, the rebuild's 1024 × 768 frame equals the HE PNG of the same sub-tab inside the
static elements (RGB565-exact, `tools/test/ui_panels_compare.py` style), names and figures masked; a click
on each tab selects only that tab (UI-TABS-1's switching); the existing UI oracle tests stay green.
HE comparison: §R steps 2-4 PNGs.

### 1c — Ship status, live (2-3 h)

Do: cargo grid (one slot per hold, `hold_count_a` from the ship's type; goods icon or empty slot; fill bar),
crew grid (children with `crew_slot`), cannon slot (count missing in the sim: draw the slot, count shown only
once the sim has it), ship name (missing: placeholder rule stated, OQ-668), the "Build warehouse" button
(arms the existing founding mode, spec §2.2: type 1302) and "Throw cargo overboard" drawn disabled until
its sim task 1i-1.
Snapshot: add to `ShipView` what spec §4 marks "missing in `ShipView`" (capacity, hold count, crew, health).
Accept: for the ship-start manifest the four holds (wood 50, food 10, tools 50, tools 30) show their icons in
hold order and the empty slots after them; a unit test unloads 10 t and the bar shrinks; "Build warehouse"
enters founding mode exactly as key F does (same `FoundCmd` in `commands.txt`).
HE comparison: §R step 2 (status tab with a full and a partly empty hold).

### 1d-1 — Ship list (2-3 h)

Do: the ship list as the body of the player view's sub-tab 3 (key **S**, UI-TABS-1) and of the ship view's
tab 5: 14 rows, name, route icon, health bar, scroll arrows; row click per OQ-668 (1a-2).
Accept: `f15_ship_list` (GOG) and the HE ship-list PNG reproduced inside the list area with names masked.
(Keys D / S / C and the player view's tab switching are UI-TABS-1's acceptance, not this stage's.)
HE comparison: §R step 5.

### 1d-2 — Player info rows (2 h)

Do: the seven value rows of the player view's sub-tab 0 (`SPIELER_BILANZ_BK` page, spec §2.10: seven text
gadgets) from the player's cities' accounts, summed in the app's view (no sim change; spec §4 "a player
sum missing"); balance red when negative.
Accept: a unit test with two cities of known accounts checks every printed row; HE `HE_UI_1.mp4` 00:44.5's
values reproduced from a matching state, or NOT_RUN with the reason.
HE comparison: §R step 5 (player view, sub-tab 1 from the top).

### 1e — Load / unload tab (2-3 h; needs 1a-2's layout)

Do: wire the tab to the existing `UnloadCmd` / `LoadCmd` (spec §4): hold selection, the four amount balls
(1/5/10/50, default 10, `[view+0x640]`), category tabs and the city goods list; tab greyed when no own
warehouse is in range (`ShipCargo::footprint_in_range`).
Accept: an unload of 10 t from the tools hold and a load of 5 t wood produce the same `commands.txt` lines as
the developer keys; the result line matches the sim's `transferred_88`; HE PNG of the tab matched.
HE comparison: §R step 3.

### 1f — Trade-route editor (3-4 h; needs 1a-3)

Do: route-point list (5 rows, "New entry"), set/delete point, activate toggle, the four action tabs and goods
slots, mapped onto `AssignFreightRouteCmd` / `ControlFreightRouteCmd` by 1a-3's mapping.
Accept: a two-stop route built by clicks equals the route a unit test builds directly; activate/deactivate send
Resume/Suspend; HE PNG of the editor with two stops matched.
HE comparison: §R step 6.

### 1g — Diplomacy list and treaty buttons, read-only (2-3 h; needs 1a-4)

Do: as the body of the player view's sub-tab 1 (key **D**, UI-TABS-1): partner rows (name, player or culture
shield by `Player::kind`, mood bar by 1a-4's rule or a flagged placeholder), "Unknown" rows, scroll arrows,
the action tab row, and the three treaty buttons drawn in the state 1a-4 derived from `Player::relations`,
disabled when no treaty is possible.
Accept: GOG `f15_player_menu` and the HE diplomacy PNGs reproduced (rows masked by name); a unit test
sets each relation byte combination and checks the frame index of each button.
HE comparison: §R steps 8-9.

### Sim tasks of their own (sim lane; spec §4), each followed by a small UI wiring stage

Each sim task reads its original record and world function first, adds the native command, the world rule,
the state it needs and its replay codec entry; the UI stage after it only wires the button (and its panel)
to the command. About 2-4 h each.

- **1h-1** `TreatyCmd{other, kind, value}` with offers as state (AnnoWelt `0x100ad280`; OQ-664). UI: the
  treaty buttons' state machine of spec §2.11 (D).
- **1h-2** `TributeCmd{…}` (record `0x70`, `0x1005dc40`; OQ-665). UI: the demand and pay tribute panels.
- **1h-3** `DiplomaticMessageCmd{other, message}` (record `0x7b`; OQ-666 first decides sim or net layer).
  UI: the message panel.
- **1i-1** `JettisonCmd{ship}` (record `0x6b`, `ship-cargo.md` §8). UI: "Throw cargo overboard".
- **1i-2** `SetFlagCmd{ship, white}` and `ScuttleCmd{ship}` (both net client `+0x88`, MP `0x75`; OQ-663).
  UI: the flag and sink icons.
- **1i-3** `SetPatrolCmd` / `ClearPatrolCmd` and the `StopCmd` equivalence (net client `+0xc0`, MP `0x84`;
  OQ-663). UI: patrol and stop.
- **1i-4** `BoardCmd{ship, target, on}` (handler `0x10060960`; OQ-663). UI: boarding.
- **1i-5** `RepairCmd{ship, on}` and `CannonsCmd{ship, delta}` (shipyard tab; cannon arms of
  `0x100ac360`). UI: the repair tab.
- **1i-6** `OfferShipSaleCmd` / `WithdrawShipSaleCmd` and `BuyShipCmd` (`0x10064f20..0x10065040`,
  `0x10061770`; OQ-667). UI: the sell and purchase tabs.

## §R — HE recordings for the owner (closes the visual gaps)

Set-up: the common one, [`ui-panels-he-recording.md`](ui-panels-he-recording.md) (window 1024 × 768,
OBS lossless, the "Screenshot Output" hotkey for every **SHOT**, test clip first). At every SHOT move the
mouse off the panel first (a cursor over a button hides pixels; hover shots are separate). Say each step
aloud ("now the behaviour tab") so the transcript times it. Total about 15-20 minutes, files `HE_SHIP_R1` …
In this recipe sub-tabs are counted **from 1, top to bottom** (the stages above and the spec count from 0).

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
   happens; press **C**, SHOT; press **D**, SHOT; click the bust tab's first sub-tab (player info), SHOT.
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

Hand-over: the video, the SHOT PNG folder, and a line per SHOT with its step number (the narration is enough).

## Base, branch, gates

Base: main at release time. Branch `task/UI-SHIPS-DIPLO-1<stage>`. Gates per stage: the changed lanes only
(`docs/orchestration/TASKS.md` §1 item 4; owner rule 2026-11-13: refused tests rerun alone, never a whole-gate
rerun); the UI oracle tests; `tools/check_unix_warnings.sh` for code stages. Report READY FOR REVIEW at the
bottom of `docs/orchestration/REPORTS.md`; the UI lane reviews, landing stays with the orchestrator.
