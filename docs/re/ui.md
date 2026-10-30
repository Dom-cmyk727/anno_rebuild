# In-game UI (original, GOG 2.0.0.5)

**Status:** UI.1 in progress (2026-10-30). Reverse-engineering and specification only; no engine code.
**Branch:** `task/UI-1-ingame-ui-re`. Layout data: [`data/ui/`](../../data/ui/) (derived facts; no original pixels or strings). Oracle: `tools/ui_layout.py` (from step 6). First pass, superseded where this note disagrees: [`ui-and-placement.md`](ui-and-placement.md).

Every claim here names its evidence: a file entry (path, section, key or ACCESS name), a module + address, or a Wine-runner screenshot under `cache/orig/ui-1/` with the steps that reached it. Anything not established is an OQ (OQ-535..549), never a guess written as fact.

## 0. Step 1 — re-check of the evidence sources (2026-10-30)

Packet sources (UI.1 §4) were opened and sampled. Facts that do not hold are listed first.

### 0.1 Packet / first-pass claims that do not hold

| Claim | What the files actually show | Evidence |
|---|---|---|
| Packet: `help/*.htm` has **854** pages | **582** English `.htm` files | `game/app/help`, counted 2026-10-30; already 582 in [`manual-and-help.md`](../research/manual-and-help.md) |
| Packet: read `docs/plan/lan-mvp.md` §1 M3/M4 | **File is not in this tree** at board `064b4a2` (`git ls-files docs/plan/` is only `lan-endless.md`) | PLAN §0.7 points at [`lan-endless.md`](../plan/lan-endless.md); M3/M4 remain in PLAN §4–5; W5 is the HUD/menus workstream that UI.2 replaces for the in-game UI |
| First pass: construction menu has a short category list, with leftover “ranch / other” | **Eight** categories, top to bottom | German manual [M 12–13] via [`playbook.md`](../research/playbook.md) §1; `tools/orig/screens.json` `construction_menu.categories` (measured 2026-10-25); `Texte.dat` sections `TOOLTIP:BAUMENU_*` (one family per category, listed in §0.4) |
| First pass: bottom bar is gold, tools, wood, bricks, civilians, soldiers, `?` | Manual’s info bar also names **marble**; first pass omitted it | [M 15] via playbook §1; `Data/statusbar.dat` ACCESS `STATUSBAR_BAUMAT` `Segments: 4, 1` (four building-material icons) and `STATUSBAR_ROSHTOFFE` (typo in the file) `Segments: 12, 1`. Which four materials map to which segment: OQ-538 |
| First pass HUD coordinates as fractions of 1024 × 768 | Fractions are a first measurement, **not pixel positions**. Later `screens.json` still stores fractions | `ui-and-placement.md` §1; `tools/orig/screens.json`. Pixel positions and anchoring: steps 3–4 |
| Packet: `8` suffix variants “another resolution? 8-bit?” left open | **Not 8-bit.** Every sampled `TEXTURE/HEAD.unknown_06` is `8` on **both** the plain and the `8` file. Pixel sizes of paired files sit at **800/1024 = 0.78125** | `fenster.tex` first texture 245 × 248 vs `fenster8.tex` 191 × 194 (191/245 = 0.780); `statusbar.tex` bar 746 × 35 vs `statusbar8.tex` 583 × 27 (583/746 = 0.782); `Abreissen_Gebaeude` 73 × 65 vs 57 × 51; `B_Baumenue` 60 × 63 vs 47 × 49. Fonts: `MainSmall.zei` 25 rows vs `MainSmall8.zei` 13 (`zei.md`). Which mode selects the `8` set: OQ-535 |
| `tools/orig/opening.json` grid cells vs `screens.json` | Two conventions: opening is `[column, row]` **0-based**; screens.json is `[row, col]` **1-based** | `opening.json` `"_cell"`; `screens.json` `grid_cells._src`. Layout in `data/ui/` will use one convention and name it |
| First pass: weaver needs colony growth before it is placeable | **Does not hold** (already corrected in that note) | `INFRAMIN = INFRA_EVER`; weaver 1001 in `opening.json`; orchestrator correction 2026-10-22 in `ui-and-placement.md` §2 |

### 0.2 Claims that still hold

- In-game frame in the Wine runner and on the Windows harness is **1024 × 768**; menus before a game are **800 × 600** at the top-left of the virtual desktop (`original-wine.md` §2.1; `original-harness.md`).
- Four right-hand tabs exist (construction, military, help/info, options/game menu). Approximate centres in `screens.json` `hud.main_tabs` (fractions; not yet pixels).
- Construction menu opens with **B**, leaves with **ESC** (manual p4–5, `game-mechanics.md` §1, `opening.json`).
- Category column x ≈ 0.764, 4 × 8 grid of 52 px cells at 1024 × 768 (`screens.json` `grid`, measured 2026-10-25).
- Citizen-start costs in `opening.json` match `Anlagen.dat` HEAD for those ids (playbook §14; Wine 2026-10-22).
- `game/` is read-only; screenshots stay under `cache/`.
- Packet ToolGfx count **2 431** holds: 1 211 `.tex` + 1 210 `.inc` + 9 `.zei` + 1 `.ok` (counted 2026-10-30). Global `.tex`/`.inc` counts in `tex.md` (1 214 / 1 213) include `Textures/` as well.

### 0.3 `Data/*.dat` texture and access tables

Only three keyed files describe UI (or UI-adjacent) textures. The others in `Data/` are goods, growth, walk masks, terrain, fonts or a Bink video (`data-text.md`).

| File | `[FILE] Name` | ACCESS names (file text, including the `ROSHTOFFE` typo) | Role |
|---|---|---|---|
| `Data/statusbar.dat` | `"toolgfx\statusbar.tex"` | `STATUSBAR`, `STATUSBAR_FRAGEZEICHEN` (Segments 2,1), `STATUSBAR_ICON` (33,1), `STATUSBAR_BAUMAT` (4,1), `STATUSBAR_ROSHTOFFE` (12,1), `STATUSBAR_GELDSYMBOL`, `STATUSBAR_EINWOHNER`, `STATUSBAR_BALKEN`, `STATUSBAR02` (873 × 35), `STATUSBAR03` (1001 × 35), `FUELLER01`–`03` | HUD info bar. Three bar widths sit in one file: 746, 873, 1001 × 35. Which width is drawn at 1024 × 768: step 4 |
| `Data/htmlpic.dat` | `"toolgfx\PK_Kleidung.tex"` | `image001.pcx` (700 × 567) | One help illustration, not the HUD |
| `Data/ScapeIcon.dat` | `"textures\ScapeIcon.tex"` | `MONEY`, `BAUMAT`, `BGRUPPE`, `RASTER01` | In-world / view icons, not the HUD chrome |
| `Data/Felder.dat` | `"textures\felder.tex"` | (no `Access`) | Terrain; not UI |

Most HUD chrome is **not** in `Data/*.dat`. It is in `ToolGfx/*.tex` + `*.inc`. Example: `ToolGfx/fenster.inc` ACCESS names (first eight, same ids in `fenster8.inc`): `WELTKARTE` 2, `MAINUNTERGRUND` 4, `MAX_PIC` 5, `STATUSBAR` 6, `LUPE` 7, `MAINTAB` 8, `VIEWBAUTAB` 16, `INFOUNTERGRUND_LEER` 32. `fenster.tex` holds **1 606** textures; `fenster.inc` names **218** of them. The `STATUSBAR` in `fenster.inc` (texture id 6) is a different object from `Data/statusbar.dat` ACCESS `STATUSBAR` (746 × 35 in `statusbar.tex`). Which one is drawn: OQ-536.

### 0.4 `ToolGfx/` and the `8` suffix

| Set | `.tex` stems | `.inc` stems | `.zei` |
|---|---|---|---|
| Plain (no `8`) | 651 | 651 | 5 (`HelpNormal`, `MainLarge`, `MainMedium`, `MainSmall`, `Startup`) |
| `8` suffix | 560 | 559 | 4 (same fonts except `Startup`) |
| Both a plain and an `8` file | 552 | 551 | 4 |
| Plain only | 99 | 100 | `Startup` |
| `8` only | 8 | 8 | none |

Paired files share ACCESS names and texture **ids**; only pixel sizes change. Empty containers exist (`Aufstiegsbedingungen.tex` has 0 textures; PLAN already notes empty ToolGfx files). Named HUD/construction files sampled: `fenster`, `statusbar`, `statusbaraddon`, `B_Baumenue`, `B_Baukosten`, `Abreissen_Gebaeude`, `Aufstiegsbedingungen`.

### 0.5 Texts

- `Texte.dat`: 118 sections (`data-text.md`). UI families: `STARTUP:*` (pre-game), `TOOLTIP:BAUMENU*` (construction), `TOOLTIP:STATUSBAR` / `STATUSBARICON`, `TOOLTIP:OPTION*`, `TOOLTIP:HELPMAIN`, `TOOLTIP:SPIELERINFO*`, `TOOLTIP:INFOSTADT*`, `TOOLTIP:PRODINFO`, `TOOLTIP:WOHNHAUSINFO`, `TOOLTIP:SCHIFF*`, `TOOLTIP:WERFT*`, `TOOLTIP:FESTUNG*`, `TOOLTIP:ABRISS`, `MESSAGEBOX`, `STATISTIK`, `OPTIONSUBS_TEXTE`. Construction tooltip sections by name: `BAUMENUTAB`, `BAUMENUINFO`, `BAUMENU_EX`, `BAUMENU_WEG`, `BAUMENU_LANDWIRTSCHAFT`, `BAUMENU_HANDWERK`, `BAUMENU_SCHIFF` (+ `_ADDON`), `BAUMENU_BERGBAU`, `BAUMENU_MILITAR` (+ `_ADDON`), `BAUMENU_DIVERSES` (+ `_ADDON`), `BAUMENU_WIGWAMS` (+ `_ADDON`). Mapping those names onto the eight banners is step 5 (do not guess from the German words alone).
- Layout stores **section + index** (text keys), never the English strings.
- `help/*.htm`: 582 pages. `InstantHelpLinks.txt`: 2 871 bytes. `Manual.pdf` in `game/app` is the English 39-page copy; the owner’s German 77-page manual is not in git ([`manual-and-help.md`](../research/manual-and-help.md)).
- `Profiles/Texte`: 4 096 bytes present; not parsed in this step.

### 0.6 `Anlagen.dat`

Construction-menu cells, costs and `INFRAMIN` / `BAUSTUFEN` unlocks live here (`anlagen.md`, `production.md`, `population.md`). The first pass’s Citizen-start names that are still “not verified” stay unverified until step 5 matches each grid cell to a type id. Existing OQs that this UI work must not close by guessing: OQ-T15-HOUSEGROW, OQ-T15-PLACEREFUSE, OQ-T15-SERVICE.

### 0.7 Code (RTTI, no new disassembly yet)

`analysis/pe_symbols.json`, `AnnoFrame.dll` RTTI (MSVC names):

- Frames: `tFrameGame`, `tFrameGameFull`, `tFrameGameStatistik`, `tFrameSpielerStatistik`, `tFrameStartup*` (profile, options, LAN, campaign, colour pick, splash, Bink, …).
- Gadgets: `tGadget`, `tGadKlick`, `tGadKlickGfx`, `tGadKlickAnim`, `tGadStatic`, `tGadStaticGfx`, `tGadStaticClip`, `tGadText`, `tGadEdit`, `tGadSlider`, `tGadFlip`, `tGadFlipGfx`, `tGadListe`, `tGadListeEx`, `tGadPermanent`.
- Cursors: `tCursorAnlage`, `tCursorAnlageStrasse`, `tCursorAnlageKontor`, `tCursorAnlageBergwerk`, `tCursorAnlageMauer`, `tCursorAnlageRohstoff`, `tCursorAbriss`, `tCursorAbrissBewohner`, `tCursorInfo`, …
- Other: `tEventStatusbar`, `iAnnoDrawEventStatusbar`, `iAnnoCursorBau`, `CreateAnnoStatusbarInstance`.

`AnnoDisplay.dll` RTTI is blitters, not window names. `View.dll` exports `LinkTO_GetWareToolTipText`, `LinkTO_GetWarenGfxid`. Data first; disassembly where data is silent (packet). This host does not run `rea` on 32-bit PE.

### 0.8 Running original

Wine runner is pre-authorized and must stay silent (`original-wine.md` §2.2). Drive with `tools/orig/macros.py` / `live.py` / `menus.json`. Do not use the dgVoodoo2 desktop harness without asking. Captures: step 3 → `cache/orig/ui-1/`.

### 0.9 First-pass “not verified” rows (`ui-and-placement.md`)

| First-pass item | Disposition |
|---|---|
| Fourth public building at Citizen start | Still unverified as an Anlagen id. `screens.json` later labels cells “coin-I building”, “stall”, “market tent” from icons only. Step 5 / OQ-537 |
| Sheep / grain / potato farm ids | Unverified. `opening.json` has sheep 807 at farms cell `[0,0]` (0-based col,row). Grain farm 811 `INFRA_STUFE_3B` is a data claim for step 5, not a pixel claim |
| Public-service radius overlay | Already OQ-T15-SERVICE; out of UI.1 scope except as a drawn overlay if captured |
| House growth without a market | OQ-T15-HOUSEGROW; not a UI layout fact |
| Placement refuse vs place-unsupplied | OQ-T15-PLACEREFUSE; UI.1 records the refusal **display**, not the sim rule |

## 1. Screen tree

Filled from step 3 onward. Reachable from a running single-player endless (Citizen) game. How each screen is reached will name the key or tab and the screenshot.

**Pre-game** (menus at 800 × 600; not the owner’s in-game test surface, listed for completeness): intros → main menu → singleplayer → choose game → in game. Coordinates in `tools/orig/menus.json`.

**In-game (priority for UI.2 / owner build tests):**

1. HUD idle (minimap, four tabs, nameplate, statistics column, status bar).
2. Construction menu: eight categories, every cell, banner, cost/info panel, lock `?`.
3. Placement mode (ghost, rotate, cancel-X, valid / refused).
4. Demolish tool (`tCursorAbriss`, `Abreissen_Gebaeude.tex`, `TOOLTIP:ABRISS`).
5. Building / settlement info panel (click a placed object).
6. Minimap controls (`+`/`−`, rotate arrows; `fenster.inc` ACCESS `LUPE`, `WELTKARTE`).

**Then:** military tab; help/info; options / game menu and every submenu; settlement, statistics, trade; ships; diplomacy; messages and dialogs.

## 2. HUD at 1024 × 768

Pixel table: after step 4. Until then the measured fractions in `tools/orig/screens.json` `hud` are the working click map, not the spec.

| Element | First-pass fraction | Data / ACCESS so far | Pixel pos / size | Notes |
|---|---|---|---|---|
| Minimap | 0.78–0.98 ×, 0.02–0.22 y | `fenster.inc` `WELTKARTE` id 2; first `fenster.tex` texture 245 × 248 | — | Frame size 245 × 248 is larger than the 0.20 × 0.20 fraction box (~205 × 154). Do not assume they are the same sprite. OQ-536 |
| Four tabs | y ≈ 0.285; x ≈ 0.80 / 0.85 / 0.90 / 0.95 | `MAINTAB`, `VIEWBAUTAB` | — | |
| Status bar | bottom | `statusbar.dat` / `statusbar.tex`; also `fenster.inc` `STATUSBAR` | bar widths 746 / 873 / 1001 × 35 | OQ-536, OQ-538 |
| Construction category column | x ≈ 0.764 | `B_Baumenue.tex` 60 × 63 | y in `screens.json` `category_column_y` | Eight icons |

## 3. Construction menu tree

Step 5. Working click map: `tools/orig/screens.json` `construction_menu` and `tools/orig/opening.json` `menu`. Category order (manual + 2026-10-25 measurement):

| # | Layout key | Banner in `screens.json` | `category_column_y` | Tooltip section name (not yet bound) |
|---|---|---|---|---|
| 1 | `public` | Public buildings | 0.423 | `TOOLTIP:BAUMENU_EX` ? OQ-537 |
| 2 | `roads` | Squares, Streets | 0.486 | `TOOLTIP:BAUMENU_WEG` ? |
| 3 | `farms` | Farms, Plantations | 0.544 | `TOOLTIP:BAUMENU_LANDWIRTSCHAFT` ? |
| 4 | `workshops` | Workshops | 0.603 | `TOOLTIP:BAUMENU_HANDWERK` ? |
| 5 | `coastal` | Coastal buildings | 0.661 | `TOOLTIP:BAUMENU_SCHIFF` ? |
| 6 | `mines` | Pits, Mines | 0.720 | `TOOLTIP:BAUMENU_BERGBAU` ? |
| 7 | `military` | Military installations | 0.779 | `TOOLTIP:BAUMENU_MILITAR` ? |
| 8 | `ornamental` | Ornamentals | 0.837 | `TOOLTIP:BAUMENU_DIVERSES` ? |

The `?` in the last column means the German section name has not been matched to a banner by a capture yet. Grid: 4 columns × 8 rows, cell 52 px, `col_x` / `row_y` in `screens.json`. Lock rule (`?` cell): `INFRAMIN` vs city stage is the working hypothesis from the first pass; verify in step 5 (two settlement stages or data + disassembly).

Verified Citizen-start cells (cost panel + `Anlagen.dat`, `opening.json`):

| Layout key | Cell (opening.json, col,row 0-based) | Anlagen id | Name key (not the string) |
|---|---|---|---|
| `farms` | 0, 1 | 801 | `Texte.dat` `[ANLAGEN]` id 801 |
| `farms` | 1, 0 | 805 | 805 |
| `farms` | 0, 0 | 807 | 807 |
| `workshops` | 0, 0 | 1023 | 1023 |
| `workshops` | 0, 1 | 1001 | 1001 |
| `public` | 2, 0 | 1301 | 1301 |
| `roads` | 0, 0 | 2520 | 2520 |

## 4. Placement and demolish (as the UI presents it)

From the first pass plus the manual, still to be pixel-specified in step 4:

- Select a cell → info panel (name, isometric preview, costs, rotate arrows) and placement mode; yellow cancel-X ~ (0.985, 0.95).
- Left click places and stays in mode; ESC / right click / cancel-X leaves.
- Rotate object: `.` / `,` (`game-mechanics.md` §1). Hold **CTRL** while placing to align the entrance to a street (manual p15).
- Demolish: pickaxe in the right-hand menu (`playbook.md` §1); cursor class `tCursorAbriss`; graphic `Abreissen_Gebaeude.tex` 73 × 65.

Refusal display and fertility bar: capture in step 3, specify in step 4. Sim refusal rule stays OQ-T15-PLACEREFUSE.

## 5. Resolutions

Observed: menus **800 × 600**, in-game **1024 × 768**. ISS-024 (community/patch notes) advises 800 × 600 on weak PCs, so in-game 800 × 600 is plausible. The `8` suffix graphics scale as 800/1024. Full list the Options screen offers, and which set each mode loads: **OQ-535**. UI.2’s own scale (W5) is a rebuild choice, not an original fact.

## 6. `data/ui/` layout format

Documented in `data/ui/README.md` from step 2. Derived facts only: ids, coordinates, sizes, file names, ACCESS names, frame/segment indices, `Texte.dat` section+index, Anlagen type ids, hotkey codes. No pixels, no English/German UI strings, no screenshots.

## 7. Capture index

`cache/orig/ui-1/` (git-ignored). Table filled in step 3.

## 8. Implementation-status (for UI.2)

Step 8. Per menu entry and per building cell: whether `src/sim` has the feature today (implemented / placeholder). Read-only against `src/sim` and the board.

## 9. Glossary (new in this note)

| Term | Literal | Meaning here |
|---|---|---|
| BauMenü / `BAUMENU` | construction menu | Right-hand build menu; `Texte.dat` `TOOLTIP:BAUMENU_*`; `B_Baumenue.tex` |
| Abreißen / `ABRISS` | tear down | Demolish; `Abreissen_Gebaeude.tex`; cursor `tCursorAbriss` |
| Aufstiegsbedingungen | promotion / upgrade conditions | `Aufstiegsbedingungen.tex` (empty in this build) |
| Statusbar | status / info bar | Bottom HUD bar; `Data/statusbar.dat` |
| Fenster | window | `ToolGfx/fenster.tex`, the large HUD chrome atlas |
| Lupe | magnifying glass | `fenster.inc` ACCESS `LUPE` (zoom control graphic; role confirmed in step 4) |
| Weltkarte | world map | `fenster.inc` ACCESS `WELTKARTE` (minimap frame candidate) |
| Füller | filler | `statusbar.dat` `FUELLER01`–`03`, large fills 245 × 192 / 96 / 256 |
| Gadget / `tGad*` | gadget | AnnoFrame widget classes (`tGadKlick`, `tGadSlider`, …) |
