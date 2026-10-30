# In-game UI (original, GOG 2.0.0.5)

**Status:** UI.1 steps 1-8 plus F1.1-F1.6 and F2.1 (2026-10-30). Reverse-engineering and specification only; no engine code. F2.1 oracle compare holds with reduced masks and restated tolerance; remaining acceptance gaps are OQ-535 and OQ-547..549.
**Branch:** `task/UI-1-F2-ui-remaining`. Layout data: [`data/ui/`](../../data/ui/) (derived facts; no original pixels or strings). Oracle: `tools/ui_layout.py`. First pass, superseded where this note disagrees: [`ui-and-placement.md`](ui-and-placement.md).

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
| 2 | Settlement / Kontor panel | click Kontor (0.38, 0.47) | `r2_hud_kontor`, `f15_kontor` |
| 3 | Construction menu, 8 categories | **B**, then category column | `r2_construction_*` |
| 4 | Building info / placement | click a grid cell | `r2_info_*` / `r2_place_*` |
| 5 | Demolish | pickaxe click (996, 736) | `r2_demolish` |
| 6 | Help / info mode | help tab centre (931, 212) | `r2_tab_help`, `f15_help_tab` |
| 7 | Military tab (same chrome as key D) | military tab (870, 212) | `r2_tab_military` |
| 8 | Options tab (in-panel) | options tab (992, 212) or key **O** | `r2_options`, `f15_options_tab` |
| 9 | Player list | key **D** | `r2_player_menu`, `f15_player_menu` |
| 10 | Pause overlay | key **P** | `f15_pause` |
| 11 | Options load | options tab, click (800, 360) | `f15_options_y360` |
| 12 | Options save | options tab, click (800, 410) | `f15_options_y410` |
| 13 | Options music | options tab, click (800, 460) | `f15_options_y460` |
| 14 | Options speech | options tab, click (800, 510) | `f15_options_y510` |
| 15 | Ship list | key **S** | `f15_ship_list` |
| 16 | City list | key **C** | `f15_city_list` |
| 17 | Rating (full frame) | key **Q** | `f15_rating` |
| 18 | Selected house | place type 501, click it | `f15_selected_house` |
| 19 | Deselected right panel | ESC from Kontor, or a miss-click | `f15_ship_guess_0` (`MAX_PIC`) |

**Tried, not reached** (QUESTION / OQ, not silence): Kontor trade sliders (OQ-547); help-page on a building (OQ-548); options statistics, quit confirm, right-side book/crest (OQ-549); diplomacy treaty cycle (no second player at Citizen); in-game `MESSAGEBOX` (none fired); ship panel from a map click (four guesses missed; list is `f15_ship_list`); second settlement stage (OQ-539); Options resolution list (OQ-535).

The military tab and key **D** open the same player-list panel (`r2_tab_military`, `r2_player_menu`). The military-tab tooltip is `Texte.dat` `TOOLTIP:CONTROLFELD` index 2 and index 3 (two identical lines). The options tab is an in-panel menu (`TOOLTIP:OPTION*` family), not a parchment. Pause (**P**) is a parchment dialog over the map. Key **O** opens the same in-panel options as the gear tab (`f15_key_O` after a leftover speech submenu still showed `OPTIONSUBS_TEXTE`; the clean open is `f15_options_tab`).

## 2. HUD at 1024 × 768 (build-test surface, F1.3)

Machine-readable copy: [`data/ui/hud.txt`](../../data/ui/hud.txt). Rectangles are **measured**, not `screens.json` fractions. Method **sprite-sad**: decode the named `.tex` id to RGBA and take the (x, y) of minimum mean absolute RGB error on opaque pixels, inside a stated search box, on a named shot. A match is kept when that mean is below 15 (clean overlay) or is the atlas under live content (minimap / status bar digits, mean 40–50).

### 2.1 Right chrome and tabs

| Element | Pixel pos / size | Graphic | Frames / states | Method / shot | Click / hotkey |
|---|---|---|---|---|---|
| Right chrome | (779, 0) 245 × 733 | `fenster.inc` `MAINUNTERGRUND` 245 × 524 stacked under `WELTKARTE` | always | atlas-right; `MAINUNTERGRUND` sprite-sad (779, 244) mean 82 on `hud_idle_running` (tabs sit on it) | — |
| Minimap atlas | (779, 0) 245 × 248 | `WELTKARTE` id 2 | map paints on top | sprite-sad mean 43.3 `hud_idle_running` | — |
| Construction tab | (779, 181) 61 × 63 | `MAINTAB` ids 8–15 (8 frames) | unselected 8, selected 12 | sprite-sad mean 10.85 on `hud_idle_running`; selected 11.31 on `r2_construction_public` | centre (809, 212); key **B** opens the same menu |
| Military tab | (840, 181) 61 × 63 | `MAINTAB` | unselected 9, selected 13 | **Default HUD draws selected 13** (mean 11.17 on `hud_idle_running`, `f13_hud_cursor_map`, `r2_player_menu`). Unselected 9 mean 10.63 on `r2_construction_public`. Selected with cursor on `r2_tab_military` mean 50. | centre (870, 212); key **D** opens the same player list |
| Help tab | (901, 181) 61 × 63 | `MAINTAB` | unselected 10, selected 14 | unselected mean 9.6 on `hud_idle_running`; selected 14 on `r2_tab_help` (cursor overlap, mean 49.9) | centre (931, 212) |
| Options tab | (962, 181) 61 × 63 | `MAINTAB` | unselected 11, selected 15 | unselected mean 10.46 idle; selected 15 mean 11.84 on `r2_options` | centre (992, 212) |

Tab stride is **61** px from x=779 (the ACCESS width), not the old 51 px fraction spacing. `tGadKlickGfx` uses eight consecutive textures: ids 8–11 unselected, 12–15 selected, one pair per tab in column order. The default right-hand panel (Citizen start, no construction/help/options open) is the player/settlement view, so the military tab is selected on idle. Hover/pressed frames were not separated: the runner has `move` (hover shots `f13_hover_tab_*`) but no held mouse-down command (OQ-544). The military-tab tooltip remains `TOOLTIP:CONTROLFELD` index 2 and 3 (F1.1). That section has **8** lines; the other six indices are OQ-541.

`fenster.inc` `STATUSBAR` id 6 780 × 43 is still not the drawn bar (OQ-536 closed for the 1024 × 768 bar: the drawn one is `Data/statusbar.dat` `STATUSBAR`).

### 2.2 Minimap controls

| Element | Pixel pos / size | Graphic | Notes |
|---|---|---|---|
| Zoom + | (959, 7) 19 × 21 | `fenster.tex` id **44** | sprite-sad mean **2.885** on `hud_idle_running`. |
| Zoom − | (982, 19) 19 × 21 | `fenster.tex` id **46** | sprite-sad mean **1.915** on `hud_idle_running`. To the right of +, not the left. |
| `LUPE` | (805, 132) 42 × 45 | `fenster.inc` `LUPE` id **7** | sprite-sad mean 51.6 opaque / **38.4** with a grey-key (the ACCESS includes a dark halo). |
| Compass west | (965, 140) 19 × 31 | `fenster.tex` id **48** | sprite-sad mean **3.088**. Yellow chevron. |
| Compass east | (986, 140) 19 × 31 | `fenster.tex` id **50** | sprite-sad mean **3.652**. Pair with 48. |

`View.dll` `0x10034388` looks up ACCESS `KARTEPLUS` (id 40), then creates **six** gadgets of class `0xb09` (`cmp edi, 6` at `0x10034500`). After the lookup, `lea esi, [ecx-2]` / `add esi, 2` per gadget: each gadget consumes two consecutive textures (idle, pressed). Sizes: `push 0x15; push 0x13` = 19 × 21 for the first four; one case sets 19 × 31 (`0x1f` × `0x13`) for the chevrons. Drawn idle frames: 44 (+), 46 (−), 48 and 50 (compass). Ids 40 and 42 are 19 × 21 on the wood bar (yellow plus matching the lupe handle, mean 5.4 / 3.6 — treated as the other two `0xb09` gadgets of the group). `KARTE_NORDUNG` ids 52–55 are 256 × 133 and do not fit the 245 px chrome; they are not the small compass. Parent gadget `0xb02` is 244 × 63 (`push 0x3f; push 0xf4` at `0x1003457e`), the wood strip under the parchment.

### 2.3 Status / info bar

Drawn bar: `statusbar.tex` id **0** = `Data/statusbar.dat` `STATUSBAR` 746 × 35 at **(0, 733)** (pixel-row y=733; sprite-sad mean 49.4 because digits and portraits sit on the graphic).

| Element | Pixel pos / size | ACCESS / tex id | Visibility | Shot |
|---|---|---|---|---|
| Gold coin | (0, 733) 33 × 35 | `STATUSBAR_GELDSYMBOL` id 52 | idle and construction | sprite-sad mean 11.6 `hud_idle_running` |
| Tools | (132, 735) 30 × 30 | `STATUSBAR_BAUMAT` id 36 segment 0 | on `r2_construction_public` and `f13_hud_cursor_map`; absent from pass-2 `hud_idle_running` | mean 9.1 `r2_construction_public` |
| Wood | (192, 735) 30 × 30 | id 37 segment 1 | same | mean 8.1 |
| Bricks | (252, 735) 30 × 30 | id 38 segment 2 | same | mean 7.4 |
| Marble | (312, 735) 30 × 30 | id 39 segment 3 | same; digit 0 at Citizen | mean 8.9 |
| Help `?` | (746, 733) 34 × 35 | `STATUSBAR_FRAGEZEICHEN` id 1 (id 2 is the other segment) | always | mean 10.2 `hud_idle_running` |
| Population head | (401, 733) 38 x 35 | `STATUSBAR_EINWOHNER` id 53 | on construction / `f13_hud_cursor_map` / house; absent from pass-2 idle | sprite-sad mean **4.74** `r2_construction_public`; idle mean 55 (empty bar). `STATUSBAR_ICON` id 3 is a fire glyph |
| Goods 0 | (547, 735) 30 x 30 | `STATUSBAR_ROSHTOFFE` id **46** segment 6 | idle and construction | grey-key mean **14.70** (opaque 23.15). F1 searched only BAUMAT x |
| Goods 1 | (587, 735) 30 x 30 | id **48** segment 8 | idle and construction | grey-key mean **11.38** |
| Goods 2 | (627, 735) 30 x 30 | id **51** segment 11 | idle and construction | grey-key mean 18.48; unique winner at that x |

Pass-2 idle vs construction: pixel runs that appear are those four BAUMAT columns (x 132-178, 192-238, 252-291, 312-351) plus `STATUSBAR_EINWOHNER` at 401-438. Marble **is** a numbered slot on `r2_construction_public` (OQ-538 resolved). On the F1.3 idle `f13_hud_cursor_map` the four BAUMAT icons are already present. The three ROSHTOFFE icons sit on idle as well.

`TOOLTIP:STATUSBAR` has 346 lines; `TOOLTIP:STATUSBARICON` has 6. Line-to-slot bind: OQ-541.

### 2.4 Construction menu chrome

Grid (confirmed by `NO_KLICK` id 56, mean **12.927** on every locked public cell, mean ~190 on every U cell): origin **(808, 305)**, stride **52**, sprite **50 × 50**. Click centre = origin + 25 → (833, 330) for (0,0), which happens to equal the old fraction centres. Spec is the rectangle, not the fraction. Empty records (id 1) are panel stone, not `NO_KLICK`.

| Element | Pixel pos / size | Graphic | Shot / method |
|---|---|---|---|
| Locked cell | origin + (col,row)×52, 50 × 50 | `NO_KLICK` id 56; id 57 is the next 50 × 50 (pressed). A locked cell does **not** show its `THUMBNAIL` (mean 230 vs `NO_KLICK` 12.9 on public (1,0)). | `r2_construction_public` |
| Unlocked cell icon | same origin, 50 × 50 | `fenster.tex` id = category `THUMBNAIL*` ACCESS id + **3 × record_index** (`record_index = row*4+col`, empty id-1 slots included) | gadget `0xb0c` 50 × 50 in chrome builder `0x1002c360`. Public (2,0) type 1301 = 154+6 = **160** mean 7.98; (3,0) **163** mean 9.59; (3,1) **175** mean 9.77; workshops (0,1) weaver = 277+12 = **289** mean 10.0. Three consecutive frames per record (idle, hover, pressed). Locked cells keep the textures in the file but draw `NO_KLICK` on top. |
| Category column | (779, click_y−23) 42 × 46, eight gadgets | `VIEWBAUTAB` ids 16, 18, …, 30 (idle); +1 pressed/selected | `View.dll` `0x1002bb4f` copies `VIEWBAUTAB`, loop `cmp ebp, 8` / `add esi, 2`, gadget `0xb0d`, size `push 0x2e; push 0x2a`. Id 16 is the public house-in-circle (decoded). Opaque SAD vs the 29 px drawn strip remains >110 (the 42 px gadget is clipped to the column); the column mask stays. `B_Baumenue.tex` 60 × 63 is a different house glyph, not the column. |
| Banner | (783, 252) 241 × 48 | `PLANKEGROSS` id **1019** | gadget `0xb17` after the eight tabs, size `push 0x30; push 0xf1` = 48 × 241 (`0x1002bd15` string `PLANKEGROSS`). Sprite-sad mean **38.75** on `r2_construction_public` (text sits on the plank). |
| Pickaxe | (970, 710) 53 × 53 | `ABRISSICON` id 659 mean 6.7 on `r2_construction_public`; id 660 mean 6.3 on `r2_demolish` (active) | hidden on idle HUD (mean 95) |
| Cancel-X | (984, 725) 33 × 38 | `INFOEXIT` id 38 mean 11.0 | on the **info / placement** panel only; not on the category grid (search mean 94) |

No scroll control was observed: every category fits in the 4 × 8 grid (max 32 records).

### 2.5 Building info / placement panel

| Element | Pixel pos / size | Graphic | Notes |
|---|---|---|---|
| Panel background | (804, 302) 220 × 466 | `INFOUNTERGRUND` id 33 mean 29.5 (preview and costs sit on it). Empty variant `INFOUNTERGRUND_LEER` id 32 mean 75.5 at the same origin (worse). | `r2_info_pioneer_house` |
| Rotate left | (808, 451) 33 × 38 | `INFOROTPFEIL` id 34 mean 10.6 | keys **DOT** / **COMMA** (`game-mechanics.md` §1) also rotate |
| Rotate right | (987, 451) 33 × 38 | id 36 best of 34–37 at that slot, mean 55 (preview overlap) | same |
| Cancel-X | (984, 725) 33 × 38 | `INFOEXIT` id 38 | leaves placement |
| Cost row | tools / wood / bricks / marble / gold on the panel | Gold coin is `B_Baukosten.tex` id 1 (32 × 29). Materials are the same `STATUSBAR_BAUMAT` ids 36–39 already bound on the bar. Opaque SAD on the panel is >100 because those sprites carry a grey fill; the decoded coin/hammer/wood match the drawn glyphs. | costs as numbers match HEAD (F1.2) |
| Upkeep | (815, 674) 40 x 37 | `B_Betriebskosten.tex` id 1 (coin with orange arrow) | grey-key mean **12.76** on `r2_info_pioneer_house`; type 501 panel shows 5 vs PROD3 0 (house) |
| Name | above the preview | `Texte.dat` `[ANLAGEN]` id line (not stored here) | |

Click a U cell: info panel + placement mode. Click an L cell: no panel (`f12_locked_*`). Left click on a valid tile places and stays in mode (`r2_place_*`). ESC / right click / cancel-X leaves.

### 2.6 Demolish and placement preview

- Demolish: click pickaxe (996, 736). Cursor class `tCursorAbriss`; graphic `Abreissen_Gebaeude.tex` 73 × 65 (that cursor did not SAD-match a 200×200 corner of `r2_demolish`, mean 171: it is a hardware/OS cursor overlay, not a blit at a fixed origin). Land under the cursor is highlighted; `r2_demolish` shows a yellow-green tile. Tooltip family `TOOLTIP:ABRISS` (4 lines).
- Placement preview: `r2_place_pioneer_house` vs `r2_info_pioneer_house` differ in 125 k map pixels (water animation), so that pair does **not** isolate the ghost. `f13_place_land`: yellow house footprint on grass (valid). `f13_place_water`: yellow empty rectangle on water (refused). Sim refusal rule stays OQ-T15-PLACEREFUSE.
- Rotation in placement: key DOT turns the ghost (`f13_place_rot_dot` vs `f13_place_land`); COMMA is the other direction (`f13_place_rot_comma`).
- Road drag: `backend.drag` after selecting dirt road (`f13_road_drag`). The drag on this Citizen island did not leave a painted road (forest / water under the path); the dirt-road info panel stayed open.
- CTRL alignment: the runner's `key` call cannot hold a modifier across a later `click` (keydown/up is one command). Not captured; manual p15 still states it (OQ-546).

### 2.7 What is still fraction-based

Category-column **click** y values in `construction.txt` remain the 2026-10-25 centres (they hit). Their rectangles are bound in F2.1 (`VIEWBAUTAB` 42 x 46, clipped to 29 px). `LUPE`, zoom minus, compass, `STATUSBAR_EINWOHNER` and the three `STATUSBAR_ROSHTOFFE` slots are bound (sections 2.2-2.3). Hover shots bind the remaining tooltip indices (OQ-541) once the tooltip box is measured against `Texte.dat` line lengths; that comparison is not in this step's oracle.

Nameplate y about 269. Idle gold reads 500000.

### 2.8 Oracle mock and per-pixel compare (F1.4, restated F2.1)

`python -I tools/ui_layout.py mock --layout data/ui --screen <name> --game-dir <game/app> --out cache/ui-mock` decodes the named `.tex` ids with `tools/tex.py` and blits them at the `hud.txt` rectangles. F2.1 also blits unlocked-cell `THUMBNAIL` icons, `PLANKEGROSS`, zoom minus, `LUPE` and the compass pair. `compare --screen <name> --shot <png>` writes `{name}_mock.png`, `{name}_diff.png` and `{name}_compare.txt` under `cache/ui-mock/` (git-ignored). `validate --game-dir` also checks every `hud.txt` tex id exists in its `.tex`, every construction `tooltip_section` in `Texte.dat`, and every cell Anlagen id in `Anlagen.dat`.

Tolerance, restated from the F2.1 spread (`COMPARE_CHANNEL` / `COMPARE_BAD_FRACTION` in `tools/ui_layout.py`): a pixel is bad when any RGB channel differs by more than **48**; the compare fails when the bad fraction of unmasked pixels exceeds **0.08**. Correct screens sit at 0.0164-0.0480 after the portrait-mask shrink. Negative controls (must fail): public mock vs roads shot 0.1968; whole-grid one-cell shift 0.1901; pickaxe rectangle wiped (`pick_fraction` 0.983). Masks: `data/ui/masks.txt` plus, for idle HUD only, the settlement panel (779, 244) 245 x 489. Unlocked cells are no longer extra-masked. The category column stays masked (`VIEWBAUTAB` 42 x 46 is clipped to 29 px). The old 346 px portraits mask is now `einwohner_digits` (439, 733) 108 x 35.

Screens and the named shots they are compared against:

| Screen | Shot | unmasked | fraction | mean SAD | pass |
|---|---|---|---|---|---|
| `hud_idle` | `hud_idle_running` | 53760 | 0.0207 | 23.70 | 1 |
| `construction_public` | `r2_construction_public` | 157565 | 0.0189 | 22.81 | 1 |
| `construction_roads` | `r2_construction_roads` | 157565 | 0.0480 | 42.35 | 1 |
| `construction_farms` | `r2_construction_farms` | 157565 | 0.0170 | 21.02 | 1 |
| `construction_workshops` | `r2_construction_workshops` | 157565 | 0.0304 | 32.40 | 1 |
| `construction_coastal` | `r2_construction_coastal` | 157565 | 0.0455 | 41.21 | 1 |
| `construction_mines` | `r2_construction_mines` | 157565 | 0.0361 | 36.22 | 1 |
| `construction_military` | `r2_construction_military` | 157565 | 0.0307 | 32.26 | 1 |
| `construction_ornamental` | `r2_construction_ornamental` | 157565 | 0.0164 | 21.69 | 1 |

`hud_idle` blits military frame 13 (the measured default) and omits BAUMAT (those four icons are absent from pass-2 `hud_idle_running`; they are present on `r2_construction_*` and `f13_hud_cursor_map`). Construction mocks blit BAUMAT ids 36-39 and construction tab frame 12.

No screen exceeded 0.08. Residual bad pixels (channel > 48, still under the fraction) and the reason they are not a layout error:

| Where | Why (evidence) |
|---|---|
| `LUPE` halo at (805, 132) | ACCESS 7 includes a dark halo; grey-key mean 38.4. |
| Empty construction cells and 2 px gutters (stride 52 minus sprite 50) | Mock fills them from `MAINUNTERGRUND` id 4. Worst on categories with many empties (`roads` 0.0480, `coastal` 0.0455). |
| Locked-cell edges | `NO_KLICK` id 56 mean 12.9 on cell interiors (section 2.4). |
| Banner text | `PLANKEGROSS` mean 38.75; the name string is live. Mask (800, 258) 200 x 22. |
| Category column | Masked 29 x 400 at (779, 300). `VIEWBAUTAB` is bound from `View.dll` `0x1002bb4f` but the 42 x 46 gadget is clipped. |
| `STATUSBAR_EINWOHNER` count | Mask (439, 733) 108 x 35. The head at (401, 733) is compared on construction. |
| Status-bar right of x=779 | `STATUSBAR` is 746 × 35; chrome below the pickaxe is panel stone. |
| BAUMAT on idle | Pass-2 `hud_idle_running` does not draw ids 36–39. |

## 3. Construction menu tree

Machine-readable copy: [`data/ui/construction.txt`](../../data/ui/construction.txt). 148 non-empty cells at the GOG 2.0.0.5 Citizen start (addon tables). Grid origin (808, 305), stride 52, sprite 50 x 50 (`NO_KLICK` id 56, mean 12.927 on every locked public cell). Click centre = origin + 25: x = 833, 885, 937, 989; y = 330, 382, 432, 484, 536, 588, 640, 692. Empty slots (`id` 1 in the View.dll table) are panel stone, not a `?`.

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

Measured rectangles in section 2.5-2.6. Summary:

- Select a U cell: info panel at (804, 302) 220 x 466 plus placement mode; cancel-X `INFOEXIT` at (984, 725) 33 x 38.
- Valid preview: yellow building footprint (`f13_place_land`). Refused: yellow empty rectangle (`f13_place_water`). Left click on valid land places and stays in mode (`r2_place_*`); ESC / right click / cancel-X leaves.
- Rotate: DOT / COMMA (`f13_place_rot_dot`, `f13_place_rot_comma`). CTRL alignment is in the manual (p15) and was not captured (OQ-546).
- Demolish: pickaxe `ABRISSICON` at (970, 710) 53 x 53 (id 659 idle-in-menu, id 660 while demolishing). Tooltip family `TOOLTIP:ABRISS` (4 lines). Cursor `tCursorAbriss` / `Abreissen_Gebaeude.tex` 73 x 65 as the OS cursor, not a blit.

Sim refusal rule stays OQ-T15-PLACEREFUSE.

## 5. Resolutions (F1.6)

Main-menu Options (graphics) at menu frame **800 × 600**, `f16_options_graphics` (Wine `d91-20261030-133242`, `macros.press main_menu options` -> `options_graphics`). Five listed modes, top to bottom, with a tick on **1024 × 768**:

| # | Mode (px) | Tick on `f16_options_graphics` |
|---|---|---|
| 1 | 800 × 600 | no |
| 2 | 1024 × 768 | yes |
| 3 | 1152 × 864 | no |
| 4 | 1280 × 960 | no |
| 5 | 1280 × 1024 | no |

A details toggle and a Back plank sit below the list (`STARTUP:OPTIONS` has 8 lines; line-to-plank bind not made). Seven clicks on the planks did not move the tick (`f16_opt_click_0..6`).

In-game HUD and construction at those modes were **not** captured: the Wine runner's virtual desktop is fixed 1024 × 768 and `tools/orig/wine/agent.py` records that the game never switches display modes there. The owner's Windows harness is out of scope (packet: never the desktop). Which ToolGfx set (plain vs `8`) each mode loads is still **OQ-535**. Size ratio of paired files remains 800/1024 = 0.78125, so the `8` set is the candidate for 800 × 600 in-game, not a pixel check.

Anchoring at the captured 1024 × 768 in-game frame (section 2):

| Band | Rule at 1024 × 768 | Evidence |
|---|---|---|
| Right chrome | x = W − 245, width 245 | `WELTKARTE` / `MAINUNTERGRUND` width; x=779 |
| Status bar | y = H − 35, 746 × 35 at (0, 733) | `Data/statusbar.dat` `STATUSBAR`; pixel-row |
| Construction grid | origin (W − 216, 305), stride 52, sprite 50 | 808 = 1024 − 245 + 29; `NO_KLICK` |
| Tabs | y = 181, stride 61 from x = W − 245 | `MAINTAB` 61 × 63 |

Those subtractions are identities at 1024 × 768, not a verified rule at 800 / 1152 / 1280.

UI.2’s own scale (W5) is a rebuild choice, not an original fact.

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

`cache/orig/ui-1/` (git-ignored). Four Wine-runner passes on display `:91`, `WineBackend` silent (`VOLUME` −250, no Pulse). In-game frame 1024 × 768; menus 800 × 600. Each instance stopped clean.

| Pass | Dir | Shots | Speed | Notes |
|---|---|---|---|---|
| 1 | `d91-20261030-112327` | 27 | **P** pause | Parchment covers the map; ESC after a cell closed the construction menu, so later category shots were idle HUD. Keep only as pause + options-tab evidence |
| 2 | `d91-20261030-112532` | 32 `r2_*` + `hud_idle_running` | **F8** | Canonical. No ESC between categories; **B** reopened before each info cell. Demolish at (0.97, 0.93) |
| 3 | `d91-20261030-132532` | 31 `f15_*` | **F8** | Screen-tree remainder. Silent. |
| 4 | `d91-20261030-133242` | 9 `f16_*` | menus | Main-menu Options graphics. Frame 800 x 600. Silent. |

Merged `index.json` is append-only across passes. Stable copies: `cache/orig/ui-1/shots/`. Look at these:

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
| `f13_hud_cursor_map` | cursor on the map, F1.3 idle |
| `f13_hover_tab_{construction,military,help,options}` | move onto each MAINTAB, no click |
| `f13_hover_zoom_plus` / `f13_hover_lupe_guess` | minimap + / lupe guesses |
| `f13_hover_cat_{public,roads,farms}` / `f13_hover_cell_501` / `f13_hover_cell_locked` | construction hover |
| `f13_hover_pickaxe` / `f13_hover_rotate_left` / `f13_hover_rotate_right` / `f13_hover_cancel_x` | chrome hover |
| `f13_place_land` / `f13_place_water` | valid yellow footprint vs refused yellow rectangle |
| `f13_place_rot_dot` / `f13_place_rot_comma` | DOT / COMMA in placement |
| `f13_road_drag` | dirt-road drag |
| `r2_demolish` | pickaxe (0.97, 0.93) |
| `r2_tab_help` | help tab |
| `r2_tab_military` | military tab |
| `r2_options` | options tab |
| `r2_player_menu` | key **D** |
| pass 1 `options_key_O` | **P** then **O** (parchment + options tab) |
| `f15_kontor` | click Kontor (0.38, 0.47) |
| `f15_kontor_btn_{a,b,c,d}` | click lower Kontor icons (820-970, 680); no trade panel opened |
| `f15_ship_guess_0..3` | map clicks near the ship; all missed (`MAX_PIC`) |
| `f15_ship_list` / `f15_city_list` / `f15_rating` | keys **S** / **C** / **Q** |
| `f15_player_menu` / `f15_hover_diplo_*` / `f15_diplo_click_hand` | key **D**, hover/click diplomacy cluster |
| `f15_options_tab` | options tab |
| `f15_options_y{360,410,460,510}` | options column x=800, those y |
| `f15_pause` | key **P** (speech submenu leftover on the right) |
| `f15_help_tab` / `f15_help_on_kontor` | help tab, then click Kontor |
| `f15_placed_house` / `f15_selected_house` | **B**, public (0,0), click land, ESC, click the house |
| `f16_main_menu` | skip intros |
| `f16_options_graphics` | `macros.press main_menu options` |
| `f16_opt_click_0..6` | clicks on the Options planks; tick stayed on 1024 x 768 |

Pass 3: Wine `d91-20261030-132532`, 31 `f15_*` shots, silent, `watchdog_stopped`, leftover_pids empty.
Pass 4: Wine `d91-20261030-133242`, 9 `f16_*` shots, silent, `watchdog_stopped`, leftover_pids empty.

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
| Ship list, city list, rating, pause, options load/save/music/speech | captured F1.5 | none |
| Kontor trade sliders, help-page on a building, options statistics/quit | not opened (OQ-547..549) | `SetTradeFlagCmd` / `SetTradeSlotCmd` exist; no UI |

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

## 9. Rest of the screen tree (F1.5)

Wine pass 3 `d91-20261030-132532`. Clicks are rectangle centres. No original strings stored; text families are `Texte.dat` section names.

### 9.1 Settlement / Kontor

Click Kontor (0.38, 0.47): right panel with nameplate and a goods column (`f15_kontor`, same as `r2_hud_kontor`). Text families `TOOLTIP:INFOSTADT*` (`INFOSTADTTAB` 16 lines, `STADTMAIN` 12, `WAREN_EIN-VERKAUF` 6). `INFOUNTERGRUND` id 33 does not SAD-match this panel (best mean 78.7). Four clicks at (820-970, 680) did not open a buy/sell slider (OQ-547).

### 9.2 Options submenus

Options tab (992, 212) / `MAINTAB` frame 15. Main list `f15_options_tab` / `r2_options`. `TOOLTIP:OPTION` 5 lines, `TOOLTIP:OPTION_MAIN` 4, `TOOLTIP:OPTION_MAIN_ADDON` 5.

| Click (x, y) | Panel | Shot | Text / ACCESS |
|---|---|---|---|
| (800, 360) | load list | `f15_options_y360` | `TOOLTIP:OPTION_LOAD` (1 line); `OPTION_LADEN` id 1490 94 x 81 (no SAD < 40) |
| (800, 410) | save list + name plank | `f15_options_y410` | `TOOLTIP:OPTION_SAVE` (2); `OPTION_EINGABEPLANKE` id 1508 239 x 52; `OPTION_SPEICHERN` id 1500 |
| (800, 460) | music ticks + slider | `f15_options_y460` | `TOOLTIP:OPTION_MUSIC` (1); `OPTION_SLIDER` id 1470 24 x 35; `OPTION_HACKERL` id 1496 14 x 11 |
| (800, 510) | speech ticks | `f15_options_y510` | `OPTIONSUBS_TEXTE` (5 lines) |
| (973, 710) door | quit | not opened | `OPTION_EXIT` id 1492 at **(927, 660)** 92 x 100, mean **11.23** on `r2_options`. Click while speech was still open was a no-op (`f15_options_exit`). OQ-549 |

Statistics from this menu and the right-side book/crest were not hit (OQ-549). Rating **Q** is a different full-frame screen (9.5).

### 9.3 Player, ship list, city list, diplomacy

Key **D** / military tab: player rows + a diplomacy icon cluster (`f15_player_menu`). `TOOLTIP:SPIELERINFOMAIN` 6, `SPIELERINFOTAB` 8, `SPIELERINFO_VERTRAG` 24. Hover on the cluster (`f15_hover_diplo_hand`) shows a treaty tooltip; click does not open a second-player treaty panel at Citizen (no discovered AI).

Key **S**: same chrome, one occupied ship row (`f15_ship_list`). `TOOLTIP:SCHIFF_LISTE` 3 lines. Map clicks at four water points missed the hull (`f15_ship_guess_*` = `MAX_PIC` id 5 empty panel).

Key **C**: same chrome, one occupied city row (`f15_city_list`). `TOOLTIP:INFOSTADT_LISTEN` 3 lines.

### 9.4 Pause

Key **P**: parchment over the map, HUD chrome stays (`f15_pause`). `ToolGfx/Pause.tex` has **0** textures. The parchment is `fenster.inc` `MESSAGEBOX_BK` id **1512** 579 x 386 at **(222, 191)** (the geometric centre of 1024 x 768; opaque mean 27.2, grey-key 26.4; the ACCESS stores a dark field around the scroll). "Continue game" is `MESSAGEBOX_BUTTONS` id 1513 244 x 67 at **(286, 500)**, mean **11.61**. Leave with **P** again. Shot has a leftover speech submenu on the right from the options probe.

### 9.5 Rating

Key **Q**: full 1024 x 768 overlay. `STATISTIK_UNTERGRUND` id 1509 at (0, 0), sprite-sad mean **23.41** on `f15_rating`. `Texte.dat` `STATISTIK` 19 lines. Leave control: a plank at the bottom-left (ACCESS not bound).

### 9.6 Help / info on a building

Help tab selects `MAINTAB` frame 14 and shows an info-mode cursor (`r2_tab_help`, `f15_help_tab`). Clicking the Kontor afterwards opened `MAX_PIC`, not a help page (`f15_help_on_kontor`). `TOOLTIP:HELPMAIN` 7 lines. OQ-548.

### 9.7 Selected house

Place public (0,0) type 501, ESC, click the house: right panel with portrait and need icons (`f15_selected_house`). Portrait graphic is `ToolGfx/Pioniere.tex` id 1 100 x 108 at **(812, 355)** (grey-key mean 34.2; the live needs digits sit on the sprite). Panel plate: `VIEWINFOPIONIERMAIN_BK` id 974 245 x 467 at **(783, 252)**, grey-key mean **17.8**. `WOHNHAUS_PLATTE` id 883 244 x 469 is the other plate (opaque mean 32 at (779, 244); not the better origin). `TOOLTIP:WOHNHAUSINFO` 3 lines. `VIEWINFOPIONIER_TAB` id 975 42 x 46.

### 9.8 Deselect

After ESC from Kontor, or a miss-click, the right panel is `MAX_PIC` id 5 245 x 524 (the castle illustration on `f15_ship_guess_0`). Default new-game idle is the settlement view with military tab frame 13, not `MAX_PIC`.

## 10. Glossary (new in this note)

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
| Statistik | rating / statistics | Full-frame `STATISTIK_UNTERGRUND` id 1509; `Texte.dat` `STATISTIK` 19 lines; key **Q** |
| Gadget / `tGad*` | gadget | AnnoFrame widget classes (`tGadKlick`, `tGadSlider`, …) |
