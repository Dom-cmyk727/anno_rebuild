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

**Grey-key (G6, 2026-11-11 fix review, finding 7).** A second sprite-sad variant, used where the sprite itself carries a solid-grey fill the live shot does not reproduce pixel-for-pixel (a plate under live text/icons, not the plate's own edges): skip a sprite pixel with alpha < 8 (transparent, as the plain method already does); *also* skip a sprite pixel where |R−G| < 12 **and** |G−B| < 12 (a near-grey/desaturated fill pixel, kept out of the comparison instead of counted as error); the mean is |ΔR| + |ΔG| + |ΔB| over the kept pixels, divided by 3, same as the plain method otherwise. A "grey-key mean" figure in this document uses this rule; a plain "sprite-sad mean"/"opaque mean" figure does not.

**H1 (2026-11-11, docs follow-up after the acceptance, finding 1): the two methods use different units, and G6's own sentence above is wrong about that.** The **plain** method's mean (the "sprite-sad mean"/"opaque mean" figures through this document, e.g. the §2.1 table's zoom + and compass west rows) is **per pixel**: |ΔR| + |ΔG| + |ΔB| over the kept pixels, summed, **not divided by 3**. Measured that way on both kept copies of `hud_idle_running` (`E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\`, the `shots/` copy and the `d91-20261030-112532/shots/` copy): zoom + (`fenster.tex` id **44** at (959, 7)) gives **2.885**, compass west (id **48** at (965, 140)) gives **3.088** - both reproduce the committed figures exactly. The **grey-key** method, as `ui1_f22_bind.py`'s own `mean_sad` computes it, *does* divide by 3 (a **per-channel** mean) - that part of the sentence above is correct for grey-key, but wrong when it implies the plain method also divides by 3. Wherever this document states a grey-key figure, the unit is per channel unless said otherwise; a plain sprite-sad figure is per pixel. See §9.7's new paragraph below for what this means for the one grey-key figure (id 974) that still does not reproduce.

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
| Category column | tab k at **(762, 303 + 46k)** 42 × 46, one per category (eight) | `VIEWBAUTAB` id 16 + 2k idle, 17 + 2k selected (the open category) | `View.dll` `0x1002bb4f` copies `VIEWBAUTAB`; per tab (loop `0x1002bb93`..`0x1002bc8e`, `inc ebp; cmp ebp, 8`) a gadget `0xb0d` of size `push 0x2e; push 0x2a` (`0x1002bbea`, 42 × 46), property `0x198` = texture (`esi`, `add esi, 2` at `0x1002bc88`), `0x1c9` = the k-th `TOOLTIP:BAUMENUTAB` line (`0x1002bc6e`). **UI-BUILDCAT-1, sprite-565 exact:** all eight tabs 1879/1879 px (tab 0 1880/1880) at those positions on `f13_construction_open`, `f13_hover_cat_{public,roads,farms}`, `r2_place_pioneer_house`, `r2_info_pioneer_house`, `r2_construction_open`; on each `r2_construction_<c>` the seven other tabs exact and the clicked one in its selected frame under the hover tooltip (rows click_y−23..click_y−8). No hover frame (hovering farms leaves id 20). Not clipped: the tabs stick out left of the panel (x 762..778 over the map), like the sub-tab columns of the other views (§5.4). The old row (779, click_y−23) measured only the part right of the panel edge. `B_Baumenue.tex` 60 × 63 is a different house glyph, not the column. Hit area: the gadget rectangle, so the column ends at x 803 and grid column 0 (808..857) is the grid's (answers OQ-714 with the measured position; the gadget class's hit-test slot itself was not read). |
| Banner | (783, 252) 241 × 48 | `PLANKEGROSS` id **1019** | gadget `0xb17` after the eight tabs, size `push 0x30; push 0xf1` = 48 × 241 (`0x1002bd15` string `PLANKEGROSS`). Sprite-sad mean **38.75** on `r2_construction_public` (text sits on the plank). |
| Banner text (UI-PLANK-1 A, 2026-11-27) | over the plank, gadget 241 × 39 | text gadget class `0xb1c` | Created right after the plank (D): `0x1002bd7e` class `0xb1c`, size `push 0x27; push 0xf1` (`0x1002bdc3`), property `0x1c6` = 2 (`0x1002bdd7`, the centred style of the info panel's names), parent `[this+0x5c0]` (`0x1002bdee`), initial text `0x1c4` = `"00"` (the constant at `0x100a8404`, `0x1002bdfa`-`0x1002be23`, overwritten before it shows, L). **The text is the open category's name:** the tab handler `0x1002bf90` reads the selected tab from the tab group (`[this+0x5d4]` property `0x1b3`, `0x1002bfa9`), stores it at `[this+0x5f0]` (`0x1002c07e`), takes list `TOOLTIP:BAUMENU_EX` (string `0x100a8434`) from the text service (`[this+0x568]` slot `+0x88`, then `+0x5c`), entry `+8(index)`, and sets `0x1c4` on `[[this+0x5d4]+8]` (`0x1002c0b8`). It is the only reader of that list in `View.dll`, so the text changes only through this handler: on a tab click, and from the panel-message case at `0x1002ad8e` (after `0x1001cbe0` and `0x1002b0a0`), which re-sets it from the selected tab (which message that is: OQ-904). Arming a building or the road leaves it (HE `he_01` `c7` 3:57: "Öffentl. Gebäude" with the market house armed). GOG `Texte.dat` `[TOOLTIP:BAUMENU_EX]`, tab 0..7: Public buildings, Squares, Streets, Farms, Plantations, Workshops, Coastal buildings, Pits, Mines, Military installations, Ornamentals (the same eight lines as `[TOOLTIP:BAUMENUTAB]`, OQ-539). Font: OQ-905. |
