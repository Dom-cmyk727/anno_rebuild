# UI-WAREHOUSE-1 (draft): the Kontor / city panel and its trade pages

**DRAFT 2026-11-16** (UI-PANELS-PREP). Not released. Spec: [`docs/re/ui-warehouse-trade.md`](../../re/ui-warehouse-trade.md)
(cited below as "spec §n"). Owner rules that apply: HE is the visual reference, the GOG DLLs give the
addresses, the original UI is the bit-identical base (owner 2026-11-14 / 2026-11-16); one concern
per packet, about 2-4 hours each, no executor-side reviewer subagents (TASKS.md owner rule
2026-11-12); evidence contract `docs/testing/evidence-contract.md`.

## Why

Clicking the own Kontor opens the city panel (spec §1, S1). The rebuild has no such panel; the only
Kontor trade today is the developer keys U / L (`opening_ui.hpp` `OpeningKey::Unload` / `Load`, "no
original warehouse-trade panel, OQ-547"). The sim already has the trade-list state and both commands
(`City::trade_lists`, `GoodsSetting`, `BuildGoods`, `SetTradeSlotCmd`, `SetTradeFlagCmd`); the panel is
the missing way to use them. OQ-547's sliders are found (tabs 2 and 3, spec §0 item 2) and OQ-155 is
answered by code (sell = list 1, spec §0 item 3).

## Stages

Each stage is its own packet (or its own report checkpoint), in this order. "HE compare" names the
footage the stage needs; recordings are in the recipe at the end, recorded at the common set-up
([`ui-panels-he-recording.md`](ui-panels-he-recording.md)).

**Sub-tabs are UI-TABS-1's.** UI-TABS-1 (branch `task/UI-TABS-1`, in progress) builds the sub-tab
switching of the player, city and options views: `data/ui/hud.txt` gives each sub-tab a `body`, and
the app shows the selected sub-tab's body (`docs/re/ui.md` section 5.4 on that branch; city view:
handler `0x1006b070` → `0x1006af30`, D). Main's `hud.txt` panel `info` already holds the city view's
background, eight tabs and deny button (spec §2, "Already in main"). The stages below therefore build
no tab switching of their own: each page is a `body` plugged into UI-TABS-1's city view (city
sub-tab 0 = panel `info`, 2 = Sell, 3 = Buy).

### W1a-1: positions, sliders and the add-goods flow, code reads only (about 2-3 h, no app code)

Read in GOG `View.dll` / `AnnoWelt.dll` and update the spec with addresses:

1. Where a View.dll child gadget gets its position (OQ-651): gadget base 0x100188c0 / 0x10018b90
   and any later layout pass (the panel's slot +0x24 = 0x10019830 only stores the child pointer,
   spec OQ-651).
2. Slider class 0xb17 (constructor 0x1000fa60): properties 0x1a1, 0x1a2, 0x1a5, 0x193 and the
   formatter 0x10025110 (OQ-654).
3. The add-goods flow: 0x100249f0, the chooser's selection handler, the 0x73 apply in AnnoWelt
   (does it set the goods record, does any UI path call sender slot +0xd0 = 0x8a) (OQ-653).

**Acceptance:** every answered OQ has a D row (module + address) in the spec; nothing is guessed;
whatever stays open stays an OQ with the next step written down. No build, no game run.
**HE compare:** none.

### W1a-2: city status, crest, deny effect and the goods per page, code reads only (about 2-3 h)

1. The city-status update functions 0x1006bfe0 / 0x1006c070: rows 1-5 field and period, the
   balance sum with the signs of rows 3-5, row 7 (OQ-655).
2. The crest gadget's frame source (OQ-657); the mode-9 branch of 0x1002dbd0 (OQ-656).
3. Which goods each stock page and the chooser list, and the goods → icon frame map
   (`LinkTO_GetWarenGfxid` 0x10088220 / 0x100881c0, the category builders' loops) (OQ-659). W1e
   needs this map; it was missing from the first draft's reads (orchestrator's review 2026-11-16,
   order gaps).

**Acceptance:** as W1a-1. **HE compare:** none.

### W1b: HE measurement (about 3 h, after the owner's recordings R1-R5)

Extract frames (ffmpeg) of each state in R1-R5 into `cache/` and sprite-sad every texture of spec §2
against them with the HE plain `fenster` set: at UI scale 1.0 the HE draws it 1:1, and the older
videos showed it at 0.75, not the `8` set (spec §3, OQ-652 narrowed by the orchestrator's review).
Write the rectangles of the elements main's `hud.txt` does not have yet (Sell / Buy pages, goods
chooser, stock pages, city name and crest) into a new `data/ui/city_panel.txt` in the `data/ui/`
format (README there).

**Acceptance:** every element of S3, S4 and S5 (the goods chooser, which W1g draws) has a rectangle
with method, file and frame time, and S1's elements agree with `hud.txt` panel `info`; on the
lossless frames of the common set-up the textures match 1:1 (mean below 5 per channel; a higher
value is reported with its cause); GOG shots below 15; `tools/ui_layout.py` validation passes on the
new file; S6-S9 if R4 is recorded.
**HE compare:** R1-R5.

### W1c: open the city view from a Kontor or market, name and crest (about 2-3 h)

A left click on one of the player's Kontors or markets shows UI-TABS-1's city view ("?" main tab)
at sub-tab 0 for **that building's city**; ESC or a miss-click closes it (to `MAX_PIC`, as `ui.md`
§9.8). This stage brings the lookup the click needs: building id → `CityRef` for the player's
Kontors and markets, published in the app's view (no sim rule change; spec §4). (The first draft
opened the panel here but brought the lookup only in W1d; order gap, review 2026-11-16.) On the
plank: the city name (`City::name`) and the crest placeholder (OQ-657 flagged default). The
background, tabs and deny button are main's panel `info` (static values, zeros).

**Acceptance:** per-pixel compare against GOG `f15_kontor` (RGB565 reduction, `ui.md` §2.5): plank
0 differing pixels outside the name text; name glyph-exact ("Plantau"); a click on a second city's
Kontor shows that city's name (unit test on the lookup); HE: the R1 frame with the HE set, 1:1 as
in W1b. Existing UI oracle tests unchanged.
**HE compare:** R1 frames.

### W1d: live data on the city-status page (about 2-3 h)

Publish per city in the app's view (no sim rule change): `CityAccounts` fields for rows 1-5,
`World::city_population` (row 0), the deny state (`City::build_goods[0x29 - build_goods_first].setting
!= 0`) (spec §4). Row 6 = row 1 - row 2
only while rows 3-5 are 0 (spec §2.2); with a non-zero row 3, 4 or 5 the balance is drawn from
the original's sum only after W1a-2 or R1 settles their signs (OQ-655), else a flagged default named
in the report. Row 7 stays a flagged placeholder until OQ-157 / OQ-655.

**Acceptance:** a unit test drives a sim world with known accounts and checks every row's printed
number; text right-aligned as the original (glyph-exact on `f15_kontor`'s "15" and "-15", red for a
negative balance); HE R1's numbers reproduced only if W1a-2 settled the period (else NOT_RUN).
**HE compare:** R1 (row values over 60 s).

### W1e: Sell / Buy pages drawn with live lists (about 3-4 h; after W-SIM-1)

Pages S3 (list 1, `VK`) and S4 (list 0, `EK`) as the bodies of UI-TABS-1's city sub-tabs 2 and 3:
background, value texts, both sliders with knob positions from the entry, plus/minus, goods grid
with the visible slot count from the sim (W-SIM-1), cell icons from W1a-2's map, selection frame,
stock bar, Add / Delete.

**Acceptance:** for a city with known entries, the drawn page equals the HE R2 / R3 frames element by
element (1:1 as in W1b), knob x equal within 1 px for three recorded values; empty and full lists
match their recorded states.
**HE compare:** R2, R3.

### W1f: clicks and commands on the Sell / Buy pages and the deny toggle (about 2-3 h)

Plus / minus / slider drag / delete → `SetTradeSlotCmd{city, list 1 sell or 0 buy, index, goods,
amount_88 = tons << 8, price}`; deny toggle → `SetTradeFlagCmd{mode 9, goods 0x29, value 0x7fff00 or
0}`. Tab switching is UI-TABS-1's. The developer keys U / L stay.

**Acceptance:** viewer-wiring scripts (as `tools/test/viewer_wiring/`) click each control and assert
the posted command's fields equal the original's record (spec §2.3), including list 1 for the
"Sell goods" tab; a replay of the session gives the same world hash; refused commands show no state
change; the 1 s send throttle (spec §2.3) is either reproduced or listed as a deviation.
**HE compare:** R2 narration (numbers after each click) for the step sizes.

### W1g: the goods chooser and "Add goods" (about 3 h)

Draw the goods chooser (spec §2.5: four category tabs `CHOOSEWAREN_TAB`, the category pages with
their `WARENICONS_*`, membership from W1a-2) at W1b's rectangles; "Add goods" opens it for the first
empty cell (0x10025790), a choice sends the command W1a-1 found (0x73 at the free index or the
append, OQ-653). (The first draft's W1f wired Add through a chooser no stage drew; split out by the
review 2026-11-16.)

**Acceptance:** the chooser equals the R2 frames with it open (1:1 as in W1b); a viewer-wiring script
adds wood and asserts the posted command against W1a-1's record; a full list refuses Add as the
original (+0x648 = 0xff, no command).
**HE compare:** R2 (chooser part).

### Sim tasks of their own (sim lane; not UI stages)

- **W-SIM-1 (before W1e):** `City` keeps the two visible slot counts of the trade lists (`VK` 4,
  `EK` 6 by default; file fields 0x26 / 0x27 of `HEAD2`, read but not kept today, spec §4 and OQ-155
  note), loaded from scenario files and saved. W1e needs them and must not grow sim state itself
  (orchestrator's review 2026-11-16, lane boundary).
- Further sim work this panel exposes (separate packets): rename or document `GoodsSetting::buy` (it
  names the sell list); clamp an entry's amount to the capacity as the original's city slot +0xbc;
  the traders' buy/sell rule that uses the lists (rows 3 / 4 stay 0 without it); a foreign-Kontor
  trade command (spec §4, OQ-658).

### Out of scope here (separate packets, ship-panel lane)

- UI: the foreign Kontor panel (0xa36), the ship trade pages (0xa51-0xa58, 0xa6e) and the route
  editor (0xa5a, 0xa5c) — spec §2.7 inventory; the city list (tab index 1, the second tab) and the
  stock pages (tab indexes 4-7) may join as bodies of UI-TABS-1's city sub-tabs if R4 is recorded.

Tab numbering: the spec uses 0-based tab indexes (index 2 = "Sell goods"); the recipe below counts
tabs 1-8 from the top as the owner sees them (tab 3 = "Sell goods").

## HE recording recipe for the owner

Purpose: close OQ-650, 652-656, 658, 659 visually. Total about 15 minutes of footage, five short
clips (one file each, `HE_WH_R1` … `HE_WH_R5`).

**Set-up:** the common one, [`ui-panels-he-recording.md`](ui-panels-he-recording.md) (window
1024 x 768, OBS lossless, SHOT hotkey, test clip first); game speed normal.
A running settlement with a Kontor, a market, at least 2 t of wood and tools in store, ideally a
second settlement (endless game "Großgrundbesitzer" after unloading the ship, or the introduction game
"Handel & Diplomatie"). Say aloud what you click and the numbers you see.

1. **R1 city status (about 2 min).** Click the own Kontor. Hold 60 s without touching anything
   (values may change). Hover each of the 8 rows for 2 s (tooltips). Hover each of the 8 tabs for
   2 s. Click the bottom picture (residents and building material) once, hold 3 s, click again,
   hold 3 s. Press ESC. Then click the marketplace, hold 3 s, ESC.
2. **R2 "Waren verkaufen" (about 4 min).** Click the Kontor, then tab 3 (crates with the up arrow).
   Hold 3 s. Hover, 2 s each: upper slider, its minus and plus, lower slider, its minus and plus, every
   goods cell, "Ware hinzufügen", "Ware löschen". Click "Ware hinzufügen", hold 3 s, click each of the
   four category tabs (2 s each), pick wood. Click the upper plus 5 times (1 s apart), the minus 2
   times, saying the number each time; drag the upper knob fully right, then to the middle; the same
   for the price slider. Add goods until all cells are full; click a cell; click "Ware löschen".
3. **R3 "Waren einkaufen" (about 3 min).** The same on tab 4 (crates with the down arrow), filling
   all cells.
4. **R4 stock tabs and city list (about 1 min).** Tabs 5-8, 3 s each, hover 2-3 icons each (tooltips
   "…t …"). Tab 2 (city list), hover up/down, click the second city if there is one.
5. **R5 foreign Kontor and ship trade (about 4 min).** With an AI player or a trader present: click
   their Kontor, hold 5 s. Send a ship there; when docked, open each ship sub-tab 3 s; buy one goods,
   sell one goods, saying amount and price. Then at the own Kontor with the ship docked: open the ship
   trade tabs, unload and load 1 t each. Ship route tab: set two route points with one load and one
   unload setting.

R6 is dropped: OQ-652 no longer asks which graphic set the HE draws (spec §3); R1 at the common
set-up confirms the plain set 1:1.

## Decisions and reasoning (draft)

- Tabs 2 / 3 are drawn and wired before the stock tabs because they hold the only player-controlled
  trade state (spec §0 item 2).
- No position is invented: until W1a / W1b give rectangles, elements without a measured rectangle are
  not drawn (or drawn with a flagged default named in the report).
- The sim's field naming (`GoodsSetting::buy`) is not changed by the UI packets; the UI maps "Sell
  goods" to list 1 by the D evidence and the sim lane decides the rename.
- Fix round after the orchestrator's review (2026-11-16): one recording set-up for all four panel
  packets; W1a split in two (W1a-2 adds the goods map W1e needs); the Kontor click and its
  building-to-city lookup in the same stage (W1c); the goods chooser in its own stage (W1g); the
  slot counts as the sim task W-SIM-1; sub-tab switching left to UI-TABS-1.
