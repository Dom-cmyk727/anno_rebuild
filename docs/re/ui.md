# In-game UI (original, GOG 2.0.0.5)

**Status:** UI.1 steps 1-8 plus F1.2 cell table (2026-10-30). Reverse-engineering and specification only; no engine code.
**Branch:** `task/UI-1-ingame-ui-re`. Layout data: [`data/ui/`](../../data/ui/) (derived facts; no original pixels or strings). Oracle: `tools/ui_layout.py`. First pass, superseded where this note disagrees: [`ui-and-placement.md`](ui-and-placement.md).

Every claim here names its evidence: a file entry (path, section, key or ACCESS name), a module + address, or a Wine-runner screenshot under `cache/orig/ui-1/` with the steps that reached it. Anything not established is an OQ (OQ-535..549), never a guess written as fact.

## 0. Step 1 — re-check of the evidence sources (2026-10-30)

Packet sources (UI.1 §4) were opened and sampled. Facts that do not hold are listed first.

### 0.1 Packet / first-pass claims that do not hold

| Claim | What the files actually show | Evidence |
|---|---|---|
| Packet: `help/*.htm` has **854** pages | **582** English `.htm` files | `game/app/help`, counted 2026-10-30; already 582 in [`manual-and-help.md`](../research/manual-and-help.md) |
| Packet: read `docs/plan/lan-mvp.md` §1 M3/M4 | **Holds** (the file is at `064b4a2`; a step-1 `git ls-files` was run while this worktree had the T61 branch checked out by mistake). M3 is the in-game UI test; M4 is developer test resources | [`lan-mvp.md`](../plan/lan-mvp.md) §1; W5 in [`lan-endless.md`](../plan/lan-endless.md) is the HUD workstream UI.2 replaces |
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

Reachable from a running single-player endless (Citizen) game. How each screen is reached names the key or tab and the screenshot under `cache/orig/ui-1/shots/` (git-ignored). Canonical set: pass 2 (`r2_*`, `hud_idle_running`), F8 half-speed, no pause. Pass 1 used key **P**; pause draws a parchment overlay that hides the map but not the HUD chrome, so pass 1 shots are not used for pixel tables.

**Pre-game** (menus at 800 × 600; not the owner in-game test surface): intros to main menu to singleplayer to choose game to in game. Coordinates in `tools/orig/menus.json`.

**In-game (priority for UI.2 / owner build tests):**

| # | Screen | How reached | Shot |
|---|---|---|---|
| 1 | HUD idle | `macros.new_game citizen`, F8 | `hud_idle_running` |
| 2 | Settlement selected | click Kontor (about 0.38, 0.47) | `r2_hud_kontor` |
| 3 | Construction menu, 8 categories | **B**, then category column | `r2_construction_*` |
| 4 | Building info / placement | click a grid cell | `r2_info_*` / `r2_place_*` |
| 5 | Demolish | pickaxe click (993, 714) | `r2_demolish` |
| 6 | Help / info mode | help tab (922, 219) | `r2_tab_help` |
| 7 | Military tab (same chrome as key D) | military tab (870, 219) | `r2_tab_military` |
| 8 | Options tab (in-panel) | options tab (973, 219) | `r2_options` |
| 9 | Player list (key D) | key **D** | `r2_player_menu` |
| 10 | Pause overlay | key **P** | pass 1 `options_key_O` (parchment; also shows the options tab) |

**Not captured in this session** (still in the tree, no shot): ship list **S**, city list **C**, rating **Q**, save/load/speech sliders as separate dialogs, diplomacy treaty cycle, in-game messages, a second settlement stage, a marble-cost building selected, Options resolution list (OQ-535).

The military tab and key **D** open the same player-list panel (`r2_tab_military`, `r2_player_menu`). The military-tab tooltip is `Texte.dat` `TOOLTIP:CONTROLFELD` index 2 and index 3 (two identical lines). The options tab is an in-panel menu (`TOOLTIP:OPTION*` family), not a parchment. Pause (**P**) is a parchment dialog over the map.

## 2. HUD at 1024 × 768

Pixel table from `hud_idle_running.png` (1024 × 768 RGB) plus ACCESS sizes. Machine-readable copy: [`data/ui/hud.txt`](../../data/ui/hud.txt). Click centres below that are still `screens.json` fractions times 1024 × 768 (integer nearest); F1.3 replaces those with shot-measured rectangles.

| Element | Pixel pos / size | ACCESS / file | Method | Shot |
|---|---|---|---|---|
| Status / info bar | (0, 733) 746 × 35 | `Data/statusbar.dat` `STATUSBAR` in `toolgfx/statusbar.tex` | first non-water row at x=20 is y=733; height 768-733=35 | `hud_idle_running` |
| Right chrome / panel | (779, 0) 245 × 733 | `fenster.inc` `MAINUNTERGRUND` 245 × 524 | 1024-245=779; x=779 y=100 is stone | `hud_idle_running` |
| Minimap atlas candidate | (779, 0) 245 × 248 | `fenster.inc` `WELTKARTE` | atlas size; the painted map is smaller than 245 × 248 (OQ-536) | `hud_idle_running` |
| Four tabs (click centres) | (819, 219), (870, 219), (922, 219), (973, 219) | `MAINTAB` 61 × 63 | 0.80/0.85/0.90/0.95 × 0.285 | r2 tab shots |
| Category column x | 782 | `B_Baumenue.tex` 60 × 63 | 0.764 × 1024 | `r2_construction_*` |
| Cancel-X | (1009, 730) | - | 0.985 × 0.95 | `r2_info_*` |
| Demolish pickaxe | (993, 714) | `ABRISSICON` 53 × 53 | 0.97 × 0.93, hit in pass 2 | `r2_demolish` |
| Lupe (magnifier, below minimap left) | size 42 × 45 | `LUPE` | visible on idle HUD | `hud_idle_running` |
| Minimap zoom + | click (1004, 16) | `KARTEPLUS` 19 × 21 | top-right of minimap; a minus sits beside it with no `KARTEMINUS` ACCESS name | `hud_idle_running` |

`fenster.inc` `STATUSBAR` id 6 is 780 × 43, a different graphic from the 746 × 35 bar that matches the capture (OQ-536). Candidate placement if that atlas entry were the bar: y = 768-43 = 725, width 780 -- it does not match y=733.

Nameplate y about 269 (0.35 × 768). Idle gold reads 500000; material digits (tools/wood/bricks) are **absent** on the idle bar and **present** once the construction menu is open (`r2_construction_public` vs `hud_idle_running`). Marble as a numbered slot was not observed at the Citizen start (OQ-538).

## 3. Construction menu tree

Machine-readable copy: [`data/ui/construction.txt`](../../data/ui/construction.txt). 148 non-empty cells at the GOG 2.0.0.5 Citizen start (addon tables). Grid click centres from `screens.json` `grid.col_x` / `row_y` (2026-10-25) times 1024 x 768: x = 833, 885, 937, 989; y = 330, 382, 432, 484, 536, 588, 640, 692. Cell 52 px. Empty slots (`id` 1 in the View.dll table) are panel stone, not a `?`.

Convention in `data/ui/`: cell = (column, row) **0-based**, same as `opening.json`. `screens.json` `grid_cells` is [row, col] **1-based** and several icon labels there are wrong (the market-tent at 1-based [1,3] is type 1301).

### 3.1 Where the original assigns types to cells

The assignment is **not** `Anlagen.dat` file order, not `[ANLAGEN]` name order, and not `HEAD` unused bytes. `Texte.dat` `TOOLTIP:BAUMENU_*` even-lines often match `[ANLAGEN]` names but are the tooltip text, not the id table. Empty candidates checked: those four, plus `BGruppen.dat` `[BAUSTUFEN]` (stage names and thresholds only), plus `BAUMENU` / `THUMBNAIL` strings in `AnnoFrame.dll` / `AnnoGame.dll` / `AnnoDisplay.dll` (absent).

**Source of the cell table:** `View.dll` (image base `0x10000000`). Each category setup pushes a `THUMBNAIL*` ACCESS name, a `TOOLTIP:BAUMENU_*` section name, a classic/addon flag, a table pointer and a record count, then calls `0x1002c360` (chrome) and `0x1002c650` (cells). `0x1002c650` walks `count` records of **8 bytes**, reads the first dword as the Anlagen id, skips gadget creation when the id is **1** (`cmp eax, 1` at `0x1002c67b`), else looks up the type and creates the cell gadget (`0x1002c260`). Record index `i` is cell `(i % 4, i / 4)`.

GOG 2.0.0.5 takes the addon branch (`test al, 2`) for military and ornamental: occupancy 17 and 30 match the addon tables (`0x100a6d70`, `0x100a7430`), not the classic tables (military (2,2) empty in classic, 2229 in addon; ornamental 24 vs 32 records). The other six categories are identical on both tables.

| # | Layout key | y (px) | Tooltip section | Thumbnail ACCESS | Table VA | Records | Occupancy |
|---|---|---|---|---|---|---|---|
| 1 | `public` | 325 | `TOOLTIP:BAUMENU_DIVERSES` | `THUMBNAILDIVERSES` | `0x100a7330` | 32 | ULUU LLLU LLLL LLLL LLLL LLLL LLLL L--- (4 U, 25 L) |
| 2 | `roads` | 373 | `TOOLTIP:BAUMENU_WEG` | `THUMBNAILWEG` | `0x100a6eb0` | 12 | UL-- LLL- L--- ---- ---- ---- ---- ---- (1 U, 5 L) |
| 3 | `farms` | 418 | `TOOLTIP:BAUMENU_LANDWIRTSCHAFT` | `THUMBNAILLANDWIRTSCHAFT` | `0x100a7070` | 32 | UULL UUUU LLLL LLLL LLLL LLLL LLLL LLL- (6 U, 25 L) |
| 4 | `workshops` | 463 | `TOOLTIP:BAUMENU_HANDWERK` | `THUMBNAILHANDWERK` | `0x100a6fb0` | 24 | U--- ULLL LL-- LLL- LLL- LLL- ---- ---- (2 U, 14 L) |
| 5 | `coastal` | 508 | `TOOLTIP:BAUMENU_SCHIFF` | `THUMBNAILSEEFAHRT` | `0x100a71d0` | 12 | ULL- UL-- LL-- ---- ---- ---- ---- ---- (2 U, 5 L) |
| 6 | `mines` | 553 | `TOOLTIP:BAUMENU_BERGBAU` | `THUMBNAILBERGBAU` | `0x100a6f10` | 20 | LLL- LLL- LL-- LL-- LL-- ---- ---- ---- (0 U, 12 L) |
| 7 | `military` | 598 | `TOOLTIP:BAUMENU_MILITAR` | `THUMBNAILMILITAR` | `0x100a6d70` | 20 | LLL- LLLL LLL- LLLL LLL- ---- ---- ---- (0 U, 17 L) |
| 8 | `ornamental` | 643 | `TOOLTIP:BAUMENU_WIGWAMS` | `THUMBNAILZIERDE` | `0x100a7430` | 32 | LLLL LLLL LLLL LLLL LLLL LLLL LLLL LL-- (0 U, 30 L) |

Locked-cell occupancy is the grey-`?` pixel signature at those click centres on the `r2_construction_*` shots (RGB-sum mean 465-495, sd 90-120, 20 px window). Counts match the View.dll non-empty records. `TOOLTIP:BAUMENUTAB` is 8 lines, byte-identical to `TOOLTIP:BAUMENU_EX`; `View.dll` `0x1002bb42` / `0x1002c08f` reference them with `VIEWBAUTAB`. Which TAB line is which category button is OQ-539 (not required to type cells).

### 3.2 Lock rule

`Anlagen.dat` `INFRAMIN` is a stage name. `AnnoGame.dll` `0x10009871`-`0x10009908` compares the record name to `INFRAMIN` (`0x10024458`), looks the payload up in the kind-`0x21` table (32 names, `INFRA_EVER` = index 0 at `0x10025f18` / table `0x10024bd0`), and stores the index at type object `+0x23`. Slot `+0x64` (`0x10007d70`) returns that byte. `View.dll` `0x1002c260` reads it when creating a construction cell.

At the Citizen start every non-empty cell with `INFRAMIN = INFRA_EVER` is occupancy **U** (15 cells) and every cell with a later `INFRA_STUFE_*` or `INFRA_STUFE_FORSCH` is occupancy **L** (133 cells). No EVER cell is locked; no later/FORSCH cell is unlocked. Empty records (id 1) are occupancy `-`. A second settlement stage was not reached (OQ-539): the unlock of a later Stufe is the same field, not pixel-verified at a higher mask.

### 3.3 Cell ids (0-based col,row; `-` = empty record id 1)

**public**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 501 | 580 | 1301 | 1310 |
| 1 | 1335 | 1336 | 1303 | 1314 |
| 2 | 1320 | 1325 | 1305 | 1315 |
| 3 | 1346 | 1379 | 1378 | 1317 |
| 4 | 1350 | 1337 | 1326 | 1318 |
| 5 | 1377 | 1376 | 1339 | 1313 |
| 6 | 1386 | 2564 | 2556 | 1316 |
| 7 | 2559 | - | - | - |

**roads**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 2520 | 2500 | - | - |
| 1 | 2550 | 2551 | 2552 | - |
| 2 | 2518 | - | - | - |
| 3 | - | - | - | - |
| 4 | - | - | - | - |
| 5 | - | - | - | - |
| 6 | - | - | - | - |
| 7 | - | - | - | - |

**farms**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 807 | 805 | 820 | 824 |
| 1 | 801 | 104 | 809 | 708 |
| 2 | 808 | 704 | 813 | 736 |
| 3 | 812 | 732 | 815 | 720 |
| 4 | 811 | 712 | 823 | 744 |
| 5 | 819 | 728 | 814 | 724 |
| 6 | 817 | 740 | 818 | 700 |
| 7 | 822 | 716 | 1341 | - |

**workshops**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 1023 | - | - | - |
| 1 | 1001 | 1005 | 1009 | 1027 |
| 2 | 1014 | 1013 | - | - |
| 3 | 1008 | 1030 | 1028 | - |
| 4 | 1018 | 1002 | 1004 | - |
| 5 | 1019 | 1039 | 1035 | - |
| 6 | - | - | - | - |
| 7 | - | - | - | - |

**coastal**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 1302 | 1304 | 1306 | - |
| 1 | 810 | 1020 | - | - |
| 2 | 821 | 1021 | - | - |
| 3 | - | - | - | - |
| 4 | - | - | - | - |
| 5 | - | - | - | - |
| 6 | - | - | - | - |
| 7 | - | - | - | - |

**mines**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 1203 | 1201 | 1218 | - |
| 1 | 1022 | 804 | 803 | - |
| 2 | 1205 | 1216 | - | - |
| 3 | 1017 | 1034 | - | - |
| 4 | 1206 | 1214 | - | - |
| 5 | - | - | - | - |
| 6 | - | - | - | - |
| 7 | - | - | - | - |

**military**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 2220 | 2221 | 2223 | - |
| 1 | 2240 | 2249 | 2234 | 2233 |
| 2 | 2246 | 2230 | 2229 | - |
| 3 | 1029 | 1006 | 1012 | 1038 |
| 4 | 1037 | 1036 | 1010 | - |
| 5 | - | - | - | - |
| 6 | - | - | - | - |
| 7 | - | - | - | - |

**ornamental**

| row | c0 | c1 | c2 | c3 |
|---|---|---|---|---|
| 0 | 2556 | 2672 | 2553 | 1380 |
| 1 | 2559 | 2566 | 2780 | 1381 |
| 2 | 2563 | 2567 | 2554 | 1382 |
| 3 | 2560 | 2568 | 2555 | 1383 |
| 4 | 2561 | 2569 | 1391 | 1384 |
| 5 | 2564 | 1389 | 1390 | 1392 |
| 6 | 1393 | 1395 | 1388 | 1385 |
| 7 | 1330 | 1387 | - | - |

Typed non-empty cells: 148 (U=15, L=133).

Citizen-start **U** cells. All 15 have info-panel shots: eight from pass 2 (`r2_info_*`), seven from F1.2 (`f12_info_*`). Panel costs match HEAD for every captured U cell.

| Layout key | Cell | Id | `INFRAMIN` | gold / tools / wood / bricks / marble / upkeep | Shot |
|---|---|---|---|---|---|
| `public` | 0, 0 | 501 | `INFRA_EVER` | 0 / 0 / 3 / 0 / 0 / 0 | `r2_info_pioneer_house` |
| `public` | 2, 0 | 1301 | `INFRA_EVER` | 250 / 3 / 7 / 0 / 0 / 10 | `r2_info_market_1301` |
| `public` | 3, 0 | 1310 | `INFRA_EVER` | 50 / 1 / 1 / 0 / 0 / 5 | `f12_info_stall_1310` |
| `public` | 3, 1 | 1314 | `INFRA_EVER` | 50 / 1 / 1 / 0 / 0 / 5 | `f12_info_stall_1314` |
| `roads` | 0, 0 | 2520 | `INFRA_EVER` | 5 / 0 / 0 / 0 / 0 / 0 | `r2_info_dirt_road` |
| `farms` | 0, 0 | 807 | `INFRA_EVER` | 220 / 2 / 4 / 0 / 0 / 10 | `r2_info_sheep_807` |
| `farms` | 1, 0 | 805 | `INFRA_EVER` | 140 / 1 / 3 / 0 / 0 / 20 | `r2_info_hunter_805` |
| `farms` | 0, 1 | 801 | `INFRA_EVER` | 150 / 2 / 0 / 0 / 0 / 12 | `r2_info_forester_801` |
| `farms` | 1, 1 | 104 | `INFRA_EVER` | 5 / 0 / 0 / 0 / 0 / 0 | `f12_info_field_104` |
| `farms` | 2, 1 | 809 | `INFRA_EVER` | 250 / 2 / 4 / 0 / 0 / 20 | `f12_info_farm_809` |
| `farms` | 3, 1 | 708 | `INFRA_EVER` | 5 / 0 / 0 / 0 / 0 / 0 | `f12_info_field_708` |
| `workshops` | 0, 0 | 1023 | `INFRA_EVER` | 300 / 2 / 5 / 0 / 0 / 9 | `r2_info_tannery_1023` |
| `workshops` | 0, 1 | 1001 | `INFRA_EVER` | 300 / 3 / 4 / 0 / 0 / 15 | `r2_info_weaver_1001` |
| `coastal` | 0, 0 | 1302 | `INFRA_EVER` | 350 / 5 / 12 / 0 / 0 / 15 | `f12_info_kontor_1302` |
| `coastal` | 0, 1 | 810 | `INFRA_EVER` | 180 / 2 / 6 / 0 / 0 / 20 | `f12_info_fisher_810` |

Type 501 panel upkeep read 5 on `r2_info_pioneer_house`; both `PROD3` u16 are 0 (house type). F1.2 info-panel costs match HEAD for 1310, 1314, 104, 809, 708, 1302 and 810. Type 104 at farms (1,1) is a placeable forest tile (`PRODTYPE` `ROHSTWACHS`, gold 5). Type 708 is a potato field. Clicking a locked `?` in each category (`f12_locked_*`) does not open the info panel.

## 4. Placement and demolish (as the UI presents it)

From the r2 info / place / demolish shots plus the manual:

- Select a cell: info panel (name, isometric preview, costs in tools/wood/bricks/gold, upkeep, rotate arrows) and placement mode; yellow cancel-X at (1009, 730).
- Left click places and stays in mode (`r2_place_*`); ESC / right click / cancel-X leaves.
- Rotate object: `.` / `,` (`game-mechanics.md` section 1). Hold **CTRL** while placing to align the entrance to a street (manual p15). Not re-captured as a dedicated shot.
- Demolish: pickaxe at (993, 714) (`fenster.inc` `ABRISSICON`); land highlight yellow; tooltip key family `TOOLTIP:ABRISS`; cancel-X; cursor class `tCursorAbriss`; graphic `Abreissen_Gebaeude.tex` 73 × 65. Click (0.764, 0.92) missed the pickaxe in pass 2; (0.97, 0.93) hit.

Refusal display and fertility bar: not isolated in a dedicated shot (place shots exist). Sim refusal rule stays OQ-T15-PLACEREFUSE.

## 5. Resolutions

Observed: menus **800 × 600**, in-game **1024 × 768**. ISS-024 (community/patch notes) advises 800 × 600 on weak PCs, so in-game 800 × 600 is plausible. The `8` suffix graphics scale as 800/1024. Full list the Options screen offers, and which set each mode loads: **OQ-535**. UI.2’s own scale (W5) is a rebuild choice, not an original fact.

## 6. `data/ui/` layout format

Documented in [`data/ui/README.md`](../../data/ui/README.md). Derived facts only: ids, coordinates, sizes, file names, ACCESS names, frame/segment indices, `Texte.dat` section+index, Anlagen type ids, hotkey codes. No pixels, no English/German UI strings, no screenshots.

### 6.1 Inventory (step 2, 2026-10-30)

Generated by `python -I tools/ui_layout.py write-inventory --game-dir <game/app> --out data/ui` (byte-identical on a second run).

| File | Counts |
|---|---|
| `data/ui/dat.txt` | 3 `Data/*.dat` files, 18 ACCESS records |
| `data/ui/inc.txt` | 1 210 `ToolGfx/*.inc`, 1 555 ACCESS records, 338 empty `ACCESS` |
| `data/ui/groups.txt` | 6 filename groups (evidence: `[FILE]` pointer or a documented HUD stem) |
| `8` suffix pairs | 551 `.inc` stems have both a plain and an `8` file; 100 plain-only; 8 `8`-only |

`Data/statusbar.dat` `STATUSBAR` is 746 × 35. `fenster.inc` ACCESS `STATUSBAR` id 6 is **780 × 43**. They are different graphics (OQ-536).

`fenster.inc` HUD ACCESS names (plain set, id and size):

| ACCESS | id | size |
|---|---|---|
| `WELTKARTE` | 2 | 245 × 248 |
| `MAINUNTERGRUND` | 4 | 245 × 524 |
| `MAX_PIC` | 5 | 245 × 524 |
| `STATUSBAR` | 6 | 780 × 43 |
| `LUPE` | 7 | 42 × 45 |
| `MAINTAB` | 8 | 61 × 63 |
| `VIEWBAUTAB` | 16 | 42 × 46 |
| `INFOUNTERGRUND_LEER` | 32 | 220 × 466 |
| `INFOUNTERGRUND` | 33 | 220 × 466 |
| `INFOEXIT` | 38 | 33 × 38 |
| `KARTEPLUS` | 40 | 19 × 21 |
| `KARTE_NORDUNG` | 52 | 256 × 133 |
| `NO_KLICK` | 56 | 50 × 50 |
| `ABRISSICON` | 659 | 53 × 53 |

No `KARTEMINUS` ACCESS name. Zoom-minus is not identified in the `.inc` names.

Construction-category thumbnails in `fenster.inc` (all 50 × 50 on the plain set, 39 × 39 on `fenster8`; 39/50 = 0.78). File order is **not** the category-column order:

| ACCESS | id |
|---|---|
| `THUMBNAILLANDWIRTSCHAFT` | 58 |
| `THUMBNAILDIVERSES` | 154 |
| `THUMBNAILWEG` | 241 |
| `THUMBNAILHANDWERK` | 277 |
| `THUMBNAILSEEFAHRT` | 355 |
| `THUMBNAILBERGBAU` | 385 |
| `THUMBNAILMILITAR` | 439 |
| `THUMBNAILZIERDE` | 499 |
| `THUMBNAILZIERDE_ADDON` | 562 |

Eight non-addon thumbnails. Bound to categories in section 3.1 (`View.dll` push of ACCESS name next to `TOOLTIP:BAUMENU_*`). Public uses `THUMBNAILDIVERSES`; ornamental uses `THUMBNAILZIERDE` (addon graphic `THUMBNAILZIERDE_ADDON` at `0x100a865c`). File order in `fenster.inc` is still not the column order.

`8` suffix sizes on these 50 × 50 thumbs are 39 × 39, same 800/1024 ratio as §0.1.

## 7. Capture index

`cache/orig/ui-1/` (git-ignored). Two Wine-runner passes on display `:91`, `WineBackend` silent (`VOLUME` −250, no Pulse). Frame 1024 × 768, `in_game`. One instance; both stopped clean.

| Pass | Dir | Shots | Speed | Notes |
|---|---|---|---|---|
| 1 | `d91-20261030-112327` | 27 | **P** pause | Parchment covers the map; ESC after a cell closed the construction menu, so later category shots were idle HUD. Keep only as pause + options-tab evidence |
| 2 | `d91-20261030-112532` | 32 `r2_*` + `hud_idle_running` | **F8** | Canonical. No ESC between categories; **B** reopened before each info cell. Demolish at (0.97, 0.93) |

Merged `index.json` lists 59 names. Stable copies: `cache/orig/ui-1/shots/`. Look at these:

| Shot | Steps (after `macros.new_game citizen` + F8 unless noted) |
|---|---|
| `hud_idle_running` | none (pass 2 start) |
| `r2_hud_kontor` | click Kontor (0.38, 0.47) |
| `r2_construction_open` | **B** |
| `r2_construction_{public,roads,farms,workshops,coastal,mines,military,ornamental}` | **B**, click category at (0.764, `category_column_y`) |
| `r2_info_pioneer_house` / `r2_place_pioneer_house` | public cell (0,0) |
| `r2_info_market_1301` / `r2_place_market_1301` | public cell (2,0) |
| `r2_info_dirt_road` / `r2_place_dirt_road` | roads cell (0,0) |
| `r2_info_sheep_807` / `r2_place_sheep_807` | farms (0,0) |
| `r2_info_forester_801` / `r2_place_forester_801` | farms (0,1) |
| `r2_info_hunter_805` / `r2_place_hunter_805` | farms (1,0) |
| `r2_info_tannery_1023` / `r2_place_tannery_1023` | workshops (0,0) |
| `r2_info_weaver_1001` / `r2_place_weaver_1001` | workshops (0,1) |
| `f12_info_stall_1310` / `f12_info_stall_1314` | public (3,0) / (3,1) |
| `f12_info_field_104` / `f12_info_farm_809` / `f12_info_field_708` | farms (1,1) / (2,1) / (3,1) |
| `f12_info_kontor_1302` / `f12_info_fisher_810` | coastal (0,0) / (0,1) |
| `f12_locked_{580,2500,820,1005,1304,1203,2220,2556}` | one locked cell per category; no info panel |
| `r2_demolish` | pickaxe (0.97, 0.93) |
| `r2_tab_help` | help tab |
| `r2_tab_military` | military tab |
| `r2_options` | options tab |
| `r2_player_menu` | key **D** |
| pass 1 `options_key_O` | **P** then **O** (parchment + options tab) |

## 8. Implementation-status (for UI.2)

Read-only against `src/sim` at this board (`064b4a2` plus this branch). There is no `src/ui`. Chrome, menus and overlays are **not drawn**. The sim can already apply the matching commands for some of the same verbs.

| UI surface | Original | `src/sim` today |
|---|---|---|
| HUD idle, minimap, tabs, status bar | captured | none (no UI library) |
| Construction menu, banners, `?` locks | captured | none |
| Building info panel (name, preview, costs) | captured | none; costs live in `Anlagen.dat` HEAD via `TypeRegistry` |
| Placement ghost / rotate / cancel-X | captured | `BuildCmd` / `create_building` places; no overlay |
| Demolish pickaxe | captured | `DemolishCmd` + `remove_building` (S10) |
| Pause parchment | captured | none |
| Options tab / player menu / help-info | captured as chrome | none |
| Ship list, city list, trade sliders, messages | not captured | `SetTradeFlagCmd` / `SetTradeSlotCmd` exist; no UI |

Typed construction cells vs sim (Citizen start):

| Id | Cell | Sim |
|---|---|---|
| 501 | public (0,0) pioneer house | `create_building` accepts the type; house population is **not applied** (`HouseRecordNotModelled`; `docs/sim/state.md`) |
| 1301 | public (2,0) market | `create_building`; market payload / city store exist |
| 2520 | roads (0,0) dirt road | `create_building`; walk/road codes in `anlagen_roads` |
| 807 / 805 / 801 | farms sheep / hunter / forester | `create_building`; production stage present |
| 1023 / 1001 | workshops tannery / weaver | `create_building`; production stage present |
| 1310 / 1314 | public (3,0) / (3,1) stalls | `create_building`; stall class exists |
| 104 / 809 / 708 | farms (1,1) / (2,1) / (3,1) | `create_building`; 104 is `ROHSTWACHS` |
| 1302 / 810 | coastal (0,0) Kontor / (0,1) fisher | `create_building`; Kontor / fisher classes exist |
| all L cells | remaining 133 | types exist in `Anlagen.dat`; UI lock (`INFRAMIN` / type `+0x23`) is not enforced in the command layer the way the menu paints `?` |

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
