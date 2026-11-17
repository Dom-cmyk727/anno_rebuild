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
| Category column | (779, click_y−23) 42 × 46, eight gadgets | `VIEWBAUTAB` ids 16, 18, …, 30 (idle); +1 pressed/selected | `View.dll` `0x1002bb4f` copies `VIEWBAUTAB`, loop `cmp ebp, 8` / `add esi, 2`, gadget `0xb0d`, size `push 0x2e; push 0x2a`. Id 16 is the public house-in-circle (decoded). Opaque SAD vs the 29 px drawn strip remains >110 (the 42 px gadget is clipped to the column); the column mask stays. `B_Baumenue.tex` 60 × 63 is a different house glyph, not the column. |
| Banner | (783, 252) 241 × 48 | `PLANKEGROSS` id **1019** | gadget `0xb17` after the eight tabs, size `push 0x30; push 0xf1` = 48 × 241 (`0x1002bd15` string `PLANKEGROSS`). Sprite-sad mean **38.75** on `r2_construction_public` (text sits on the plank). |
| Pickaxe | (970, 710) 53 × 53 | `ABRISSICON` id 659 mean 6.7 on `r2_construction_public`; id 660 mean 6.3 on `r2_demolish` (active) | hidden on idle HUD (mean 95) |
| Cancel-X | (984, 725) 33 × 38 | `INFOEXIT` id 38 mean 11.0 | on the **info / placement** panel only; not on the category grid (search mean 94) |

No scroll control was observed: every category fits in the 4 × 8 grid (max 32 records).

### 2.5 Building info / placement panel

Rewritten by UI-INFO-1 (2026-11-14). Every row below has file or code evidence and a per-pixel check against the GOG shots. **RGB565 reduction:** the GOG shots are a 16-bit frame. A 24-bit colour shows as each channel rounded to 5/6/5 bits and expanded back (`r5 = (r+4)>>3`, `g6 = (g+2)>>2`, `b5 = (b+4)>>3`, then `(v*255 + 15)/31` or `(v*255 + 31)/63`). Derived from `INFOUNTERGRUND` against four shots; with it, the panel plate matches the shots exactly wherever nothing is drawn on it. All "exact" counts below are after this reduction (`tools/ui_layout.py rgb565_shown`).

| Element | Pixel pos / size | Graphic / source | Evidence |
|---|---|---|---|
| Panel background | (804, 302) 220 x 466 | `INFOUNTERGRUND` id 33 | Exact outside the drawn elements on all 15 shots. The cost/upkeep icons and plank outlines are the plate's own pixels (OQ-561 closed). History Edition: same ACCESS, id **38** (HE ids in `fenster.inc` are shifted +5 here), pixel-identical decode |
| Rotate left | (808, 451) 33 x 38 | `INFOROTPFEIL` id 34 | 0 of 1254 differ on three shots. View.dll gadget 33 x 38 (`push 0x26; push 0x21`, 0x1002fd58) |
| Rotate right | **(988, 451)** 33 x 38 | id 36 | 0 of 1254 at x 988; 984 at the old 987 |
| Cancel-X | (984, 725) **33 x 35** | `INFOEXIT` id 38, cut to the gadget | View.dll 0x1002ff95 `push 0x23; push 0x21` (35 x 33). Texture is 33 x 38; its last three rows are not drawn (95 differing pixels when they are, 0 when cut) |
| Upkeep icon | (815, 674) | part of `INFOUNTERGRUND` | Panel alone: 0 of 1478 icon pixels differ. Blitting `B_Betriebskosten.tex` id 1 there (the old live draw) makes 1477 differ, so it is not drawn. The grey-key 12.76 had matched the baked icon |
| Name | box (819, y) 200 wide, text top y 316 | `Texte.dat` `[ANLAGEN]` `<id>:<name>`, first line of the id | Glyph-match on 15 shots: 0 differing pixels for each name. View.dll gadget 200 x 18 (0x1003007f). HE `Languages/Eng/Texte.dat` has the same `[ANLAGEN]` lines; HE root `Texte.dat` and `Languages/Ger` hold German names |
| Tools / wood | boxes (852, 564) / (954, 564), 50 wide | goods `WERKZEUG` 0x29 / `HOLZ` 0x2a | View.dll update 0x10030140: for gadgets 5..8 it asks the type (`vfunc +0xcc`, kind 0) for goods 0x29, 0x2a, 0x2b, 0x2c, keeps `(v + bias) >> 8` (whole tons, 0x100301e2) and prints with `_itoa` radix 10 (0x1008d919). Gadget 50 x 18 (0x1003008d) |
| Bricks / marble | (852, 593) / (954, 593), 50 wide | `ZIEGEL` 0x2b / `MAMOR` 0x2c | same |
| Gold | (852, 624), 145 wide | gold cost | `vfunc +0xcc` kind 1, index 0 (0x1003022d). Gadget 145 x 18 (0x100300ba) |
| Upkeep | (852, 684), 145 wide | PROD3 column **1** (active) | `vfunc +0xcc` kind 2, column 1 (0x1003026b). 15 of 15 shots show column 1; in 11 (807, 805, 801, 809, 1023, 1001, 810, 1301, 1302, 1310, 1314) it differs from column 0 (the last four have column 0 = 0). Type 501 shows **0** (glyph-exact), not the 5 this table used to record. HE `Anlagen.dat` HEAD and PROD3 are identical for all 15 types |
| Preview | box (844, 360) 135 x 135 | `Anlagen.dat` sprite set 0, zoom group 0, frame 0, centred; shadow = silhouette at (+5, +10) in (66, 65, 66) | Gadget 0xb17 135 x 135 (0x1002fbac, 135.0f at 0x1002fb99). Whole box exact on `r2_info_dirt_road`, `f12_info_field_104`, `f12_info_field_708`. Not yet for the other 12 types (OQ-612) |

**Text.** Font `ToolGfx/MainSmall.zei`. In GOG, MainSmall, MainMedium and HelpNormal are byte-identical files, so which name the gadget uses is not visible (HE: `Languages/Eng` fonts are glyph-identical to GOG; HE `ToolGfx` and `Languages/Ger` MainSmall/MainMedium differ). Level colours, opaque: 1 = (255, 255, 255), 2 = (0, 0, 0), 3 = (66, 61, 66) (displayed values; level 3 is RGB565 (8, 15, 8), its 24-bit source is unknown, OQ-031). Layout = `assets::render_text` (advance = glyph width). **Centring rule:** text x = box x + box_w/2 - text_w/2, both halves rounded down; y = text top. Proven by 1-, 2- and 3-digit values and 15 names. View.dll sets gadget property 0x1c6 = 2 and 0x1c4 = the text (default `"00"` at 0x100a8404); reading 0x1c6 = 2 as "centred" is consistent with the measurement but not separately proven. The gadget's own y (18 px high) is not measured; the data stores the text top.

**Per-pixel result** (`python -I tools/ui_layout.py info-compare --type <id> --shot <png> --game-dir <dir>`, panel rectangle 102 520 px, regions: name box, preview box incl. its frame (838, 354) 147 x 152, figures, rest = chrome):

| Type | Shot | Differing px | Where |
|---|---|---|---|
| 2520 dirt road | `r2_info_dirt_road` | **0** | - |
| 104 forest | `f12_info_field_104` | **0** | - |
| 708 potato field | `f12_info_field_708` | **0** | - |
| 801 forester | `r2_info_forester_801` | 6 472 | preview only |
| 1301 market | `r2_info_market_1301` | 6 213 | preview only |
| 1302 Kontor | `f12_info_kontor_1302` | 9 304 | preview only |
| 501, 1310, 1314, 807, 805, 809, 1023, 1001, 810 | their shots | 5 007-11 237 | preview only |

Name, figures and chrome: 0 differing pixels on all 15. The live window (CPU) reproduces the reference exactly (`info_panel_pixel_check`), so these counts are also the window's against GOG for 801, 1301, 1302, 2520.

**HE code:** the HE `View.dll` is PE32+ x86-64 (machine 0x8664), so the GOG 32-bit sequences above cannot appear in it. It contains `INFOUNTERGRUND` twice and 135.0f 21 times, so a string cross-reference is the open route; its panel code was not located (OQ-613).

Click a U cell: info panel + placement mode. Click an L cell: no panel (`f12_locked_*`). Left click on a valid tile places and stays in mode (`r2_place_*`). ESC and a right click leave the placement mode (back to the grid, nothing armed); the cancel-X closes only the panel and the building stays armed (section 2.6, UI-PLACE-1).

**Rotate arrows and the preview per rotation (UI-INFO-2, 2026-11-14).** Static reads of GOG `View.dll` (pefile + capstone, VAs) and a Wine-runner capture of the original (Citizen game, `cache/orig/ui-info-2/shots/` in the packet's worktree, not in git: `u2_501_*` the pioneer house, `u2_2520_*` the dirt road, the pointer on the map after each step).

| Item | Finding | Evidence |
|---|---|---|
| Left arrow | gadget 2 of the panel (class 0xb09, 33 x 38), image `INFOROTPFEIL` id (34), tooltip `[TOOLTIP:BAUMENUINFO]` line 0 ("Rotate left"; HE "Links drehen"), click handler **0x10030440**: `cursor = [this+0x62c]->vfunc 0x7c()`, `v = cursor->get(5)`, `cursor->set(5, (v - 1) & 3)`, then the panel's dirty byte `+0x634` = 1 | builder 0x1002fcec..0x1002fddb (handler pushed at 0x1002fdaf, tooltip index 0 at 0x1002fdc0) |
| Right arrow | gadget 3, image id + 2 (36), tooltip line 1 ("Rotate right"), handler **0x10030480**: `(v + 1) & 3` | 0x1002fdec..0x1002fed8 (handler 0x1002fead, index `edi` = 1) |
| Cancel-X | gadget 0xb, tooltip line 2 ("Close"), handler 0x10030410 | 0x1002ff13..0x1002fff9 |
| Property 5 | the placement cursor's turn byte `+0x46` (setters: ordinary cursor 0x10047e5d, Kontor 0x10049166, base 0x100442df; OQ-520). `AnnoFrame.dll` commands 0xf (−1) and 0x10 (+1) of the jump table at 0x10007d14 change it with the same `& 3` (`& 7` for one cursor kind, 0x100079a6); View.dll 0x100190cd does the same for 0xf/0x10 | `AnnoFrame.dll` 0x1000798a (0xf), 0x100079ba (0x10) |
| Preview redraw | 0x10030330 (dirty byte set) calls 0x100302c0: `rot = ([this+0x62c]->vfunc 0xa0()->vfunc 0xc() + cursor->get(5)) & 3` - the **view turn plus the cursor turn** - and draws the type's object through 0x1001d760(object, rot, 135.0f, 135.0f, 0, 1, 0, 1) | 0x100302e4..0x10030326 |
| Which sprite | 0x1001d760 takes each part's frame through the part's slot 0x6c with direction `rot << 2` (0x1001d0dc, 16 directions per turn); the frame that gives was not traced in code. **Measured instead:** the preview and the map ghost show rotation block `rot` of the type's sprite set - the frame the map draws a placed building of rotation `rot` with (`docs/render/iso.md` "Frame"). The dirt road after 0..4 right clicks and one left click (`u2_2520_start`, `R1`..`R4`, `R4L1`): the whole panel is exact (0 px, RGB565) against the reference at rotation 0, 1, 2, 3, 0, 3 and differs in 1 828-1 995 px at every other rotation. The pioneer house (scaled, OQ-612): 0, then 3, 2, 1, 0 after each left click and 1, 2, 3, 0 after each right click; nearest reference rotation on all 13 shots, and its map ghost shows the same block in each shot (`cache/re/cap_501_ghosts.png`). HE (owner's video) agrees: left 0 → 3 → 2 → 1, right 0 → 1 → 2 → 3 (`c_pion_rot4.jpg`, `d_pion_rot5.jpg`) | `tools/test/info_panel_rotation_gog_compare.py` |
| Size and place per rotation | 0x1001d760 computes the parts' screen rectangle for all four turns (loop 0x1001d797..0x1001d7ff, `rot+3` down to `rot`), keeps the largest width and height, and draws into a canvas of that size with the current turn's rectangle centred in it (0x1001da40..0x1001da82: `x0 + (x1 - x0 - W) / 2`, rounding toward zero). The scale factor is `max(1, W / 135, H / 135)` over that **largest** extent (0x1001db7f..0x1001dbd9; 1.0 at 0x1009a1e0): one scale for all four turns. How the canvas is then placed in the box was not read | the rebuild centres each turn's frame in the box directly (the UI-INFO-1 rule); exact for 2520 at all four turns; OQ-624 |
| Keys | the original's ordinary placement cursor turns **on** under `.` (`u2_501_dot1`/`dot2`: blocks 1, 2; `f13_place_rot_dot` too) and **back** under `,` (`comma1`/`comma2`: 1, 0) - the same way as its right/left arrow. The rebuild's building tool follows this since UI-INFO-2c (owner, 2026-11-15); the ship-start founding mode keeps the Kontor cursor's measured `,` on, `.` back (OQ-520) | OQ-623 (resolved) |

Rebuild: `data/ui/hud.txt` gives `rotate_left` `turn -1` and `rotate_right` `turn 1`; a left click there on the open panel (`ConstructionMenu::click` → `Turn`) sends the developer tool one `RotateBack` or `Rotate` per quarter turn (`turn_inputs`), the inputs the keys send, so the preview (`InfoPanelContent::rotation` = `DevInput::rotation()`), the ghost and the build use one rotation. The preview takes rotation block `rotation` of sprite set 0, group 0, its first animation step (`render::frame_index` at time 0), cached per (type, rotation); the rebuild has no view turn, so the view term is 0. Rotation 0 is frame 0, as before.

### 2.6 Demolish and placement preview

- Demolish: click pickaxe (996, 736). Cursor class `tCursorAbriss`; graphic `Abreissen_Gebaeude.tex` 73 × 65 (that cursor did not SAD-match a 200×200 corner of `r2_demolish`, mean 171: it is a hardware/OS cursor overlay, not a blit at a fixed origin). Land under the cursor is highlighted; `r2_demolish` shows a yellow-green tile. Tooltip family `TOOLTIP:ABRISS` (4 lines).
- Placement preview: `r2_place_pioneer_house` vs `r2_info_pioneer_house` differ in 125 k map pixels (water animation), so that pair does **not** isolate the ghost. `f13_place_land`: yellow house footprint on grass (valid). `f13_place_water`: yellow empty rectangle on water (refused). Sim refusal rule stays OQ-T15-PLACEREFUSE.
- Rotation in placement: key DOT turns the ghost (`f13_place_rot_dot` vs `f13_place_land`); COMMA is the other direction (`f13_place_rot_comma`).
- Road drag: `backend.drag` after selecting dirt road (`f13_road_drag`). The drag on this Citizen island did not leave a painted road (forest / water under the path); the dirt-road info panel stayed open.
- CTRL alignment: the runner's `key` call cannot hold a modifier across a later `click` (keydown/up is one command). Not captured; manual p15 still states it (OQ-546).

**Cancel-X, ESC, right click and the pickaxe (UI-PLACE-1, 2026-11-15).** Two silent Wine-runner passes of the GOG original (Citizen game, F8, public category; `cache/orig/ui-place-1/d96-20261115-174705` and `d96-20261115-174948` in the packet's worktree, not in git; the pointer parked on land at (0.22, 0.28) before every shot, so a ghost or the demolish highlight shows when that mode is on). The cancel-X handler 0x10030410 only posts a message (`[this+0x60c]->vfunc 0x9c(1, 1, &msg)`); what it does was measured:

| Step | Original | Shot |
|---|---|---|
| Pioneer house cell, then the cancel-X | the panel closes to the grid; the house **stays armed**: its ghost stays on the pointer and the grid marks its cell (0, 0) with an orange frame | `p1_a_armed`, `p1_a_after_x` |
| Pickaxe with nothing armed | demolish mode: the pickaxe drawn active, a yellow tile outline under the pointer | `p2_pickaxe_nothing_armed` |
| Pickaxe again | demolish off; no outline, no ghost: nothing armed | `p2_pickaxe_off` |
| Pickaxe with the house armed (after the cancel-X) | demolish on; the house's ghost **and** the yellow outline both show | `p1_b_demolish`, `p1_b_pickaxe_again` |
| Pickaxe again | demolish off; the house's ghost stays (still armed) | `p1_b_pickaxe_third` |
| A cell while demolishing | the cell's panel opens and the demolish mode is **off** (no outline), with or without a building armed before; after the cancel-X the pickaxe shows off | `p1_b_demolish_then_501`, `p1_b_after_x`, `p2_demolish_then_501` |
| ESC in placement | back to the grid, nothing armed (no ghost, no orange frame); the menu stays open | `p1_c_after_esc`, `p2_esc1` |
| ESC in demolish mode | back to the grid, demolish off | `p2_demolish_esc` |
| ESC on the grid | the construction menu closes; the panel shows the player tab (overview with the balance figures) | `p2_esc2` |
| Right click on the map in placement | back to the grid, nothing armed | `p1_d_after_right` |

UI-PLACE-2 (2026-11-15) added three passes (`cache/orig/ui-place-2/d96-20261115-202026`, `-202202`, `-202401` in that packet's worktree; scripts `cache/capture/ui_place_2*_capture.py`):

| Step | Original | Shot |
|---|---|---|
| Right click on the map in demolish mode (nothing armed) | demolish off, the grid stays | `p3_demolish_right` |
| Right click on the info panel's preview in placement | back to the grid, nothing armed | `p3_panel_right` |
| Cancel-X, then ESC (house armed, grid shown) | the menu closes; no ghost | `p3_after_x_esc` |
| House armed, cancel-X, pickaxe on, then ESC | only the demolish mode goes: grid, orange frame and ghost stay | `p4_armed_demolish_esc` |
| ESC again | the menu closes; no ghost | `p4_armed_demolish_esc2` |
| House armed, cancel-X, pickaxe on, then a right click on the map | demolish off **and** the ghost gone; the grid stays (the orange frame stayed in this case) | `p4_armed_demolish_right`, `p4_armed_demolish_right2` |
| Right click on the map, grid open, nothing armed | nothing changes | `p4_grid_right` |
| Help tab, B, public, then ESC | the menu closes to the player overview, not back to the help tab | `p4_help_tab`, `p4_help_b_esc` |
| B with the menu open | nothing changes (B only opens it) | `p5_b_again`, `p5_b_third` |
| Right click on a grid cell ("?"), nothing armed | the menu closes (player overview) | `p5_right_on_cell` |
| House armed, cancel-X, then a right click on the map | ghost and frame gone, the grid stays; a left click on the land then builds nothing | `p5_after_x_right`, `p5_after_x_right_left` |

(`p3_after_x_right`, `p3_grid_right` and `p4_grid_right_on_cell` are not evidence: the menu had already closed, or the script's B was assumed to toggle it.) So ESC goes one step back - info panel → grid with nothing armed; demolish → off, an armed building stays; grid → menu closed with the armed building put away, the player overview showing - while a right click on the map or the info panel puts away everything the pointer carries at once and shows the grid, and a right click on the grid closes the menu. Not captured: a right click on the grid's chrome with a building armed or the demolish mode on, or on other chrome (tabs, status bar); whether the original's right drag pans (the runner's `drag` is left-button only).

Rebuild: since UI-PLACE-1 a cell chosen while demolishing leaves the demolish mode (`DevTool::arm`); the pickaxe works with nothing armed and, turned off again, leaves nothing armed, while a building armed before stays armed under it (`DevTool::toggle_demolish`); the cancel-X already kept the building armed. Since UI-PLACE-2 (owner, 2026-11-15, "like original"; OQ-626): ESC is `Viewer::construction_step_back` (the chain above; a held ESC acts once), a right click (press and release within 4 px) is `Viewer::construction_right_click` (map or info panel: everything put away, the grid; any other chrome while the grid shows: the menu closes, to the player overview - the armed case inferred), and a right drag stays this window's pan. Still different: no orange frame marks the armed cell (OQ-627).

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

### 2.9 Minimap (UI-MINIMAP-1 step A.1)

Static read of the GOG 2.0.0.5 binaries (pefile + capstone, VAs; listings in the packet worktree's `cache/re/`, not in git), 2026-11-16. Module names are the GOG files under `game/app`. "HE" marks a cross-check in the History Edition's 32-bit `AnnoWeltAddOn.dll`, used only to find class names through its RTTI. Confidence: **high** = read instruction by instruction; **medium** = the control flow is read but one link (a caller, a unit, a meaning) is inferred; **low** = a reading that needs a capture.

**Who does what.** The minimap is not a View.dll gadget. `AnnoFrame.dll` draws it with Direct3D 7 into the frame, under the HUD chrome. `WELTKARTE` (id 2) is the frame around it and has a transparent diamond hole. View.dll only creates the six `KARTEPLUS` buttons, and they send frame commands. `AnnoWelt.dll` fills a 256 x 256 byte image of the world. Objects:

| Object | Where | Notes |
|---|---|---|
| Minimap wrapper | `AnnoFrame` frame object `+0x1658` | Non-virtual methods `0x10002800` (set rectangle, load the four `KARTE_NORDUNG` textures), `0x10002920` (attach world), `0x10002970` (hit test), `0x10002990` (follow camera), `0x100029d0` (minimap zoom), `0x100029f0` (screen to world), `0x10002a60` (view turn), `0x10002a70` (main zoom level), `0x10002a80` (add alert), `0x10002ab0` (clear alerts), `0x10002b10` (collect route line and markers), `0x100030f0` (per-frame draw). |
| Minimap renderer | `AnnoFrame`, 0x9228 bytes, ctor `0x100337e0`, init `0x10033280`, at wrapper `+0x24` | Created in `0x10002710` with size 256 x 256 (`push 0x100` twice at `0x100027a5`). |
| World image | `AnnoWelt` `tWeltServer::tKarteObject` (name from HE RTTI); GOG vtable `0x100ccd40` | Created in `0x100aaa53`..`0x100aaa8f` with 256 x 256 (`0x100a40d0(world, 0x100, 0x100)`), stored at world `+0x120`; the world's vfunc `+0xbc` (`0x100a3510`) returns it. |

**Q1 - what draws it, where, and the routine.**

| Finding | Address | Confidence |
|---|---|---|
| The frame layout computes `base` = 1024 if the screen width W > 800, 800 if 640 < W <= 800, else 640; the right chrome's x = W - base/4 + 11 (779 at 1024). | `AnnoFrame` `0x1000a038`..`0x1000a0f4` | high |
| Minimap rectangle passed to the wrapper: pos = (W - base/4 - 3, 12), size = (base/4, base/8). At 1024 x 768: (765, 12) 256 x 128. | `0x1000a2f2`..`0x1000a342` (also `0x1000517b`..`0x100051d9`) | high |
| The renderer then shifts it: W > 800: x +2, y -2 (both edges) → (767, 10)-(1023, 138); 800-class: x +3, y0 -5, y1 -4. It stores the scale s = min-class W / 1024 (1.0 above 800, 0.78125 at 800) and s·88, s·44, s·128, s·64 for the transform. | `0x10031a40`..`0x10031ef1` | high |
| Draw order, every frame (`0x10031490`): (1) parchment quad, texture `+0x91d8`, VB `+0x9208`, a 4-vertex triangle strip; (2) alpha blending on (SRCALPHA / INVSRCALPHA), map quad, texture `+0x91d4`, VB `+0x9204`; (3) an optional scrolling text banner (`data\banner.dat`, `0x10030c90`/`0x10030dc0`); (4) blending off, route line list `+0x9210`; (5) view box line list `+0x920c`. Then the wrapper draws markers and alerts as 2D draw-list items (`0x100030f0`). | `0x10031490`..`0x100317d0`; `0x100030f0`..`0x100034e4` | high |
| The minimap is drawn only while frame flag `+0x16ec` bit 0x20 is set (default flags 0x893f / 0xfb7f have it); command 0x15 XORs the flags. | `0x10005bda`, `0x10008e75`, `0x10009518`, `0x1000772e` | high |
| Pixel check: `KARTE_NORDUNG` id 52 matches `hud_idle_running` best at (768, 11) scaled to 256 x 128 (median abs RGB diff 20 inside the chrome hole; islands lie on top), one pixel off the code's (767, 10). So the parchment reaches 12 px left of the chrome, under its scroll ends. | `cache/re/fitnord.py` | medium (exact rasterisation OQ-628) |

**Q2 - what fills the diamond.**

| Finding | Address | Confidence |
|---|---|---|
| **Parchment:** `KARTE_NORDUNG` ids 52 + turn (four 256 x 133 sprites, the parchment diamond with a compass rose and an "N" in four places) is blitted into a 256 x 256 texture when the view turn changes (else the texture is filled with 0x00ff). Shown with v from 0 to 133/256 (`0.51953125`), squeezed to the rectangle's height. | load `0x10002842`..`0x10002867`; blit `0x10030940`..`0x10030a86`; v `0x10031dd3` | high |
| **Map texture:** 256 x 256, 16-bit. Each byte of the world image is looked up in a 256-entry palette at `+0x9040`; each row is then mirrored left-right. | `0x10031000`..`0x10031418` | high |
| **Byte = owner × 16 + shade.** High nibble: for each map pixel, the world tile at ((x + 0.5)·sx, (y + 0.5)·sy) (sx = world width / 256); its island (world vfunc `+0x8c`), the tile's settlement index (island vfunc `+0xec` = `0x100860c0`, bits 25..28 of the tile record; 0xf = none), the settlement (`+0xf8` = `0x100863e0`); if the settlement's mask (vfunc `+0xc8`(0)) has the viewing player's bit (or the world says everything is visible, vfunc `+0x74`(5) = 0), owner = settlement vfunc `+0xc8`(2); else 9. | `AnnoWelt` `0x100a3ed7`..`0x100a40bb` | high (B2 read below) |
| **B2: what the owner nibble means.** The settlement is the island's city (`island[0x38 + 4·i]`), city vtable `0x100c2f40`. Its `+0xc8` is `0x1002df80`: argument 0 returns `+0x61c`, the players that know the city (bit i = player i; the owner's bit is set when the city gets its owner, `0x1002f44d`, HEAD2 `[0x30]` is ORed in, the founding's registration adds the MILIT-3 peers: the sim's `City::player_mask`); argument 2 returns the byte `+0x16`, the city's colour. `+0x16` is written when the city gets its owner (`0x1002f416`..`0x1002f439`: the owner's property 11 through player `GetProperty` `0x10020a00` case 0xb = `0x10020b2f`, the colour slot `+0xa8`, else `+0xa9`; a value 0xff is stored as 0) and by every recolour (player property 11 setter `0x10020f91` → `0x1001cdf0`, which sends property 0xb to the objects of the player's lists, the city's setter `0x1002f560` case 0xb = `0x1002f5e7`, again 0xff → 0). So **the owner index is the owner's colour slot** (HEAD +0x4C, 1..8 for humans and computers, 0 for natives, pirates and player 31; docs/re/annoscene.md 9), the palette's base colour of that index. The viewer is world vfunc `+0x114` = `0x100a34b0` = `[world + 0xf8]`, the local player set by AnnoFrame `SetPlayer` (docs/re/ship-founding.md). World vfunc `+0x74`(5) = `0x100a33ce` returns `[world + 0x69cd] == 0`; a world reset stores 0 there (`0x100a3a93`), so the mask applies by default; the only setter call found is AnnoScene's kind-3 instance (`0x1000d730`: property 5 = 0, every city shown). The tutorial `02Trade01`: player 2 (human, kind 8) has colour 4 = 00e6ce (teal), player 0 (computer) 2 = ff4400, player 1 (natives) 0; the History Edition shows the human's settlement teal and the other two olive. | city `0x1002df80`, `0x1002f3d0`..`0x1002f46c`, `0x1002f560`; player `0x10020a00`, `0x10020b2f`, `0x10020f91`, `0x1001cdf0`; world `0x100a34b0`, `0x100a3380`, `0x100a3280`, `0x100a3a93` | high; which value the 0xff case takes (an uninitialised stack word picks `+0xa9` or 0xff) is open (OQ-629) |
| Low nibble: the mean island corner height over a 2 x 2-pixel window (world vfunc `+0x5c` = `0x1009db00` → island `+0x98` = `0x10084c20`, the corner byte times 0.04 from table `0x100e2d90`, filled at `0x1007c760`; sea = 0.0), divided back by 0.04, then (v + 8) >> 4. So shade ≈ mean height byte / 16. | `0x100a3cc0`..`0x100a3ece` | high |
| **Palette** (`0x100337e0`): 10 base colours × 16 shades. Entry (c, k) for k = 2..15 = colour c scaled by (k + 16)/31 per channel, packed 1:5:5:5 with the alpha bit set. Entries k = 0, 1 are 0 = transparent, so the sea and the lowest land show the parchment. Base colours (0xRRGGBB, index 0..9): e6aa10, 00bbff, ff4400, fff40a, 00e6ce, ff9e00, 15e015, f839ff, fafafa, b2ab00; index 9 (no owner, the olive land) uses 664400 for k = 2. | `0x100338c0`..`0x10033a31` | high (which player owns which index: OQ-629) |
| **Cached, not per frame.** `tKarteObject::Update(flags, rect)` (`0x100a3cc0`; bit 2 heights, bit 1 owners) rewrites a world rectangle and increments a counter (`+0xc`). Each frame the wrapper compares that counter with its copy and rebuilds the texture only when it changed. | `0x100030f9`..`0x10003125`; `0x100a3ece`, `0x100a40bb` | high (who calls Update and when: OQ-634) |

**Q3 - world to minimap.**

| Finding | Address | Confidence |
|---|---|---|
| The image always covers the whole world in 256 x 256 cells, whatever the world size (cell = world width / 256 tiles across, world height / 256 down). | `0x100aaa6d`, `0x100a3cee`..`0x100a3d10` | high |
| Minimap zoom `z` (`+0x18`, start 1.0, clamp 0.15..1.0) and centre `c` (`+0x1c`, `+0x20`, 0..1). The visible window is [c - z/2, c + z/2] in world fractions, pushed back inside [0, 1] at the edges. `c` follows the camera: `0x10033ad0` sets it from the camera position / world size (the wrapper passes the frame's camera values `+0x1608`/`+0x1610`, negated and swapped). | `0x10033710`, `0x10033ad0`, `0x10002990`, callers `0x1000777c`.. | high (camera axis convention: see `docs/render/iso.md`) |
| A world point (wx, wy) → u = 128·wx/W, v = 128·wy/H; inside the window: du, dv in [-1, 1] across it; a = 45° - 90°·turn; screen x = x0 + s·(128 - 88·(cos a·dv - sin a·du)), y = y0 + s·(64 + 44·(cos a·du + sin a·dv)). With turn 0 the diamond is 2 : 1, half-diagonals 124.5 and 62.2 px at s = 1, centred at (x0 + 128, y0 + 64). | `0x100306f0`..`0x10030927` (points), same math in `0x1002fde0` and `0x10030350` (lines) | high |
| The map quad's texture corners come from the zoom window and are permuted per view turn (switch on `+0x91a8`). | `0x100317e0`..`0x100319f8` | high (exact UV table not transcribed; step B) |
| Screen → world (`0x100333f0`): hit test `0x100332f0` (inside the rectangle and inside its inscribed diamond), then (x + o_x)·1.03, (y + o_y)·1.03 with o = (-6, 0) above 800 and (-5, 2) at 800, rotated back per turn, mapped through the zoom window to world tiles; the minimap then recentres on that point. | `0x100332f0`..`0x100333ea`, `0x100333f0`..`0x100336a0`, offsets `0x10031e4f`/`0x10031eb5` | high |

**Q4 - the view box.**

| Finding | Address | Confidence |
|---|---|---|
| Four line segments (8 vertices, D3D line list), 1 px, colour from floats `0x1009cfe8`..`0x1009cff4` = (1, 1, 1, 1): **opaque white**. | build `0x1002fde0`..`0x10030316`; colour init `0x1002fb00`..`0x1002fb1e`, packed at `0x100327f3`..`0x1003292c`; drawn `0x10031776`.. | high |
| Built around the minimap centre `c` (= the camera), in 0..128 map units: the centre is shifted by f = 332.8 / (W/2) on both axes with signs per view turn, and the corners use e = 2^level · 2200 / W and 0.3·e (W = world width; switches `0x10030320` and `0x10030330`). `level` = the main view's zoom level (0..2, `+0x8ee8`, set by `0x10031a20` from the zoom commands), so the box doubles per zoom-out step. It does not use the screen size. Each segment is clipped to the minimap window (`0x1002fb70`), then transformed like the markers (Q3). | `0x1002ff4d`..`0x100302f8` | high for the inputs, medium for the corner arrangement (step B checks it against captures) |

**Q5 - markers.**

| Finding | Address | Confidence |
|---|---|---|
| Each marker is a small square with corners (x − 1, y − 1) and (x + 1, y + 1) around its transformed point, in its colour (up to 1500 per frame, list cleared each update at `0x10002e43`). | add `0x10033b60`; draw `0x10003135`..`0x1000327f` | high (2 or 3 px wide depends on rasterisation, OQ-628) |
| Own player's figures from list kind 1: dark green 0x00d000; from list kind 0 (skipped when flags & 0xc0 = 0x80): green 0x00ff00. Either is white 0xffffff when its flag 0x10 is set. Colours stored by `0x10002800` at wrapper `+0x14`..`+0x20`. | `0x10002e48`..`0x10002fab`; `0x10002896`..`0x100028ab` | high for the colours; flag 0x10's meaning (selected?): OQ-630 |
| **B2: the lists.** Player vfunc `+0x84` = `0x1001c5a0` returns the head of the list at player `+0x26c + 8·kind`; vfunc `+0x8c` = `0x1001c5e0` adds an object to list `kind`. The `SPIELER` reader (`0x1001d240`) adds each `SOLDAT` with kind 0 (`0x1001ddf3`..`0x1001ddfd`) and each `SCHIFF` with kind 1 (`0x1001e063`..`0x1001e06f`); kind 2 holds the cities (the recolour `0x1001cdf0` walks `+0x27c` with the island). So **own ships are 0x00d000, own soldiers 0x00ff00**. A figure whose vfunc `+0x8c`(4) is non-zero is skipped in both lists. The position passed to the transform is the figure's vfunc `+0x5c` first, then `+0x54` (`0x10002e85`..`0x10002eae`); `+0x54` is x and `+0x5c` the other horizontal coordinate (docs/re/transport.md, docs/re/ship-founding.md), so the map's first axis u is the world's y (OQ-635). The point is truncated (`__ftol`), scaled and offset, and drawn as draw-list item 5 with the corners (x − 1, y − 1), (x + 1, y + 1) (`0x10003135`..`0x10003257`). The GOG shot `construction_open` shows it as a **1 px 3 x 3 outline** in (0, 210, 0) - 0x00d000 in the RGB565 frame - with its centre showing the map. | `0x1001c5a0`, `0x1001c5e0`, `0x1001ddfd`, `0x1001e06f`; `0x10002e48`..`0x10002f9e`; `0x10003135`..`0x10003257` | high |
| Other players' kind-1 figures (players 0..30 except the own): red 0xff3000 - only when a world interface query (vfunc `+0x48`(0xe)) returns GUID {a2414664-1a42-4f88-aae8-c0b27db939f5}. | `0x10002fb1`..`0x100030cd` | high for the code, low for when it is true (OQ-630) |
| **B2:** the query is the world's `GetProperty` (vtable `0x100ccdd8` slot `+0x48` = `0x100aeeb0`) key 0xe = `0x100aef99`: the 16 bytes at world `+0x6938`. Who writes them was not read. | `0x100aeeb0`, `0x100aef99` | high for the read; the writer: OQ-630 |
| Route line: when the own player's selection is a kind-2 object of type 0x71 that the player owns, its route stops (entries 9 and 8) become a polyline (points closer than 6 tiles merged, max 1024), drawn as a line list in (1, 1, 0.38, 0.8): pale yellow, 80 % alpha. | `0x10002b10`..`0x10002e37`; `0x10033a50`; colour `0x1002fb40`..`0x1002fb5e` | high for drawing, medium for "selected ship's trade route" |
| **`MINIMAP_ALERT x y`** (script): AnnoScene parses two values (`0x1001906e`..`0x10019140`), the action (`0x10015980`) calls frame vfunc `+0xb4` = `0x10002ac0` → `0x10002a80` → `0x10033bb0`(x, y, 0): up to 8 alerts. Each shows a 16-frame sprite at its transformed point (frames loaded from graphic group 0x655 at `0x1000a36c`..`0x1000a3b5`, frame = (t / 128) & 15, drawn through the draw list's vfunc `+0xcc`); a 32 x 36 area from (x − 16, y − 32) is copied to a per-alert surface first (`0x10033cb0`). It blinks (drawn while t mod 1000 < 500) and is removed when t ≥ 50 000; t grows by the draw call's time argument. | `0x10003285`..`0x100034bf`, `0x10033c20` | high for the code; the sprite file and the time unit: OQ-633 |
| Nothing else is drawn on the minimap (no cities, no fog). The compass rose is part of the `KARTE_NORDUNG` parchment. | whole draw path above | medium (absence) |

**Q6 - clicks and the six buttons.** View.dll `0x10033f50` (the HUD builder) creates gadgets 1..6 of class 0xb09 with textures 40, 42, 44, 46, 48, 50 (`KARTEPLUS` + 2·(i - 1), `0x100343c9`..`0x10034449`) and handlers from the jump table at `0x10034764`. A handler calls the frame's command sink (`[this+0x62c]` vfunc `+0x5c`(2, cmd, param, 0)); the frame's dispatcher `0x100071b0` takes type 2 at `0x10007514` and jumps through `0x10007d14` on cmd - 3. No `KARTEPLUS` gadget gets a tooltip (no property 0x1c9 in that loop; the tabs below get `TOOLTIP:CONTROLFELD`), and neither GOG nor HE `Texte.dat` has a minimap tooltip section.

| Control (measured, 2.2) | Gadget / handler | Command | Effect | Confidence |
|---|---|---|---|---|
| Magnifier + (id 40) | 1 / `0x10034a50` | 0xa | main view zoom level - 1, not below 0 (closer), `0x10007710`; then the minimap's box level | high |
| Magnifier − (id 42) | 2 / `0x10034a50` | 9 | main view zoom level + 1, not above 2, `0x10007692` | high |
| Top-right + (id 44, (959, 7)) | 3 / `0x10034a20` | 0x1b, param 1 | minimap zoom: z − 0.15 (shows less world, larger), `0x10007537` → `0x10033770` | high |
| Top-right − (id 46, (982, 19)) | 4 / `0x10034a20` | 0x1b, param 0 | minimap zoom: z + 0.15, up to the whole world | high |
| Left chevron (id 48, (965, 140)) | 5 / `0x10034a80` | 0xc | view turn (turn − 1) & 3; recomputes the camera (`0x10003be0`), sets the minimap turn (new parchment frame, rotated map) and recentres it, `0x100077ad` | high (screen direction: OQ-632) |
| Right chevron (id 50, (986, 140)) | 6 / `0x10034a80` | 0xd | (turn + 1) & 3, `0x10007819` | high (screen direction: OQ-632) |
| `LUPE` (id 7) | none | - | No module references the string `LUPE`; the magnifier, its + and −, both arrows and the top + and − are also painted into `WELTKARTE` itself (decoded id 2). Only the six gadgets above react. | high |
| Click on the minimap | frame mouse handlers `0x10006c60` (switch `0x10006df4`) and `0x1000b5c2` (switch `0x1000bb70`) | events 1 and 3 (button down) | if bit 0x20 is on and the point hits the diamond: screen → world (`0x100029f0`), camera centred there (`0x100040f0`, then `0x10003be0`), capture state `+0x16f8` = 2 | high; which event number is the left and which the right button: OQ-631 |
| Drag on the minimap | same | event 0 (move) | while the state is 2 the move does nothing (`0x1000bb8c`[2] = `0x1000bb61`; `0x10006c87`): **no drag-scrolling**; the release (2, 4) only resets the state | high |

Other commands seen in the same switch: 8 sets the main zoom level directly (param 100 / 50 / 25 → level 0 / 1 / 2, `0x1000755d`); 0xb sets the view turn to the param (`0x1000774d`); 3 sets the camera to the tile packed as x | y << 16 (`0x10007885`; fix round F4 below). The View.dll gadget 1/2 handler passes param = (id == 1), which commands 9 and 0xa ignore.

**Q7 - window size.**

| Finding | Address | Confidence |
|---|---|---|
| The minimap is anchored to the right edge: x = W - base/4 - 3 (+2), y = 12 (-2), with `base` from the width class (1024 / 800 / 640). Above 800 wide it is never scaled (s = 1.0); at 800 everything is 0.78125 and the `Fenster8`/`MainMedium8` graphics are loaded. | `0x1000a038`..`0x1000a342`, `0x10031d8e`..`0x10031e17`, `0x1000a5ce`..`0x1000a5f7` | high |
| The view box size depends on the world width and the zoom level only, not on the screen size: in GOG a wider window shows more world but the box stays the same. | `0x1002ff4d`..`0x1003004b` | high (HE may differ; check in A.2) |
| View.dll's HUD builder sets its gadget scale to 0.78125 when the width is 800. | View `0x10033f6a`..`0x10033f7b` | high |

**For the view turn (render packet VIEW-TURN-1, orchestrator 2026-11-16).** What this read found about the turn, gathered: the view turn is one value 0..3 held by AnnoFrame; the chevrons send commands 0xc ((turn − 1) & 3) and 0xd ((turn + 1) & 3), command 0xb sets it to a parameter (dispatcher `0x100071b0`, jump table `0x10007d14`; `0x100077ad`, `0x10007819`, `0x1000774d`); a turn recomputes the camera (`0x10003be0`) and the minimap's turn. The minimap shows it three ways: the parchment frame `KARTE_NORDUNG` id 52 + turn (four orientations, each with the compass rose and "N" turned), the map quad's texture corners permuted per turn (`0x100317e0`..`0x100319f8`), and the angle a = 45° − 90°·turn in the world → minimap transform (`0x100306f0`); clicks are rotated back per turn (`0x100333f0`). Which way each chevron turns on screen is OQ-632. The info panel's preview adds the same view turn (section 2.5, `0x100302c0`). The UI lane builds none of this; its chevrons stay dead until VIEW-TURN-1.

**Corrections to 2.2.** `KARTE_NORDUNG` 52-55 are the minimap's parchment (one per view turn), drawn under the chrome's hole, not unused. The `0xb02` parent of 244 x 63 created after the six buttons (`0x10034509`..`0x100345e3`) is the **tab strip** (four 61 x 63 `MAINTAB` children of class 0xb0d, `0x10034607`..`0x10034745`), not a wood strip under the parchment.

**Rebuild, step B1 (2026-11-16).** `src/app` `minimap.hpp` draws the minimap from `data/ui/minimap.txt` (every number above as data, the original's values as defaults, each line with its address): the world image (Q2, the shade over a 2 x 2-cell window divided by the nominal tile count, the owner at the cell's centre tile), the palette (Q2, shown as the RGB565 frame shows it), the rectangle (Q1/Q7, right-anchored: (767, 10) at 1024, (2285, 10) at 2542), the turn-0 transform (Q3) with u = world y (OQ-635), the view box (Q4, clipped Cohen-Sutherland, 1 px white) and the parchment `KARTE_NORDUNG` 52. The window builds the image once at load (1.0-2.2 ms, 512 x 512 world; composing the raster 0.7-1.0 ms, the first compose 77-86 ms because it is the first use of `fenster.tex`, parsed once for the whole HUD), keeps the composed parchment + map raster until the layer width or the minimap window changes, and redraws only the box per frame; both UI paths (CPU, GPU overlay) draw it first, under `WELTKARTE`. Not yet: owners (all olive; B2, OQ-629), markers, alerts, route, clicks, the zoom buttons, the turn (VIEW-TURN-1). Measured defaults: both quads at the rectangle + (1, 1), the parchment's rows filtered linearly (OQ-628). Checks (crops under `cache/ui-minimap-1/`, not in git): the tutorial `02Trade01` against the History Edition capture `ui_5.mp4` - island outlines agree (land overlap 0.62 in the hole, the settlement and box area left out; 0.00 for the mirrored axis), HE's land is a little darker (mean (105, 104, 19) against (115, 111, 11) in the hole, compressed video); the islands fit best at the drawn offset (overlap 0.61 against 0.52-0.60 one pixel off); the CPU and GPU paths draw the minimap identically (0 of 31,850 pixels differ) and at 2542 x 1351 it is the 1024 picture moved right (0 differ); the parchment against the GOG shot `hud_idle_running` - median 17 sum-RGB at the code's offset, 13 after the RGB565 reduction, 34-41 one pixel off; the GOG shot's land colours are palette entries exactly.

**Rebuild, step B2 (2026-11-16).** Owners, markers and the box's level, all read through the snapshot (the minimap never touches the sim; no sim change). `PlaySnapshot::territory` (`src/app` `territory_view.hpp`) carries every island's tile owners (`TileLayer::owner`) and cities (number, owner, `player_mask`), taken at publication from read-only accessors (35 µs per turn for the tutorial's 3 islands, 39,168 tiles) and republished as the same object while the content does not change (a `version` that grows on change, the rebuild's counterpart of the original's counter, OQ-634); `PlaySnapshot::map_figures` carries the ships (figures with a navigation state, list 1) every turn. `minimap_owner_rule` (`play.hpp`) reads, before the world moves into the runner, the viewer - the first human slot (kind 8 or 11, not `neutral_player`), the slot a new single-player game gives the local identity (`0x10029100`; a ship start's participant when there is one) - and each player's colour slot: the game start's assignment (`assets::unit::start_colours`, the same the units are drawn with), else the HEAD colour the sim keeps (`Player::default_colour`). `minimap_owner_layers` applies the read above per tile; the window rebuilds the image only when the territory version changes (1.1-2.9 ms for the 512-tile tutorial, first build at the first frame). Markers: `minimap_markers` (own list 1, own list 0, others' list 1 only with `marker_others`, at most 1500), drawn as the 3 x 3 outline over the box, colours shown through RGB565. The box's level factor is now 2^level at the built levels (B1 used the tile width ratio, 84 / 20 = 4.2 at group 2). Data: `owner_rule`, `marker_colours`, `marker_flags`, `marker_square`, `marker_others`, `alerts` in `data/ui/minimap.txt`.

Checks (crops under `cache/ui-minimap-1/b2/`, not in git), tutorial `02Trade01` at 1024 x 768 against the History Edition capture `ui_5.mp4` (frame 166 s, aligned by the islands' best overlap at (+1527, +31) of the frame scaled to 2560 x 1440, land IoU 0.70): the human's settlement is teal in both, bounding box x 889-909, y 67-73 ours against 891-907, 68-72 HE (HE's box covers part of it); the computer's and the natives' cities are olive in both; the ship's marker centre (905, 70) ours against the green blob's centroid (905.9, 71.1) HE. CPU and GPU paths draw the minimap identically (0 of 30,240 pixels differ). **The view box:** the box is an axis-aligned rectangle 1.361·e across and 1.264·e down (corners from Q4, 0.972 px per map unit across, 0.486 down), e = 2^level · 2200 / W: for the 512-tile tutorial 5.85 x 5.43 px at level 0, 11.7 x 10.9 at level 1, 23.4 x 21.7 at level 2. The rebuild draws 6 x 6, 12 x 10 and 24 x 22 px (line-centre spans at groups 0, 1, 2). The History Edition (0.75-scale capture, ±1.3 px) shows 4 x 5 frame px = 5.3 x 6.7 px for most of the recording, including the owner's "I can't zoom in any further" (166 s), and 9 x 8 = 12.0 x 10.7 px at 30-33 s: levels 0 and 1 of the formula, so formula and HE agree. B1's larger box came from the start zoom: `--play` opens at zoom group 2 (`main.cpp` `args->group.value_or(2)`, a viewer default), the box at level 2 (then with the 4.2 factor), while HE was at level 0 or 1; where the original starts and where its camera starts is OQ-638.

**Rebuild, step C (2026-11-16): the controls.** Read further for C (AnnoFrame, same listings): the hit test `0x100332f0` works on the point relative to the **wrapper's** rectangle (`0x10002970` and `0x100029f0` subtract `[wrapper+0]`, `[wrapper+4]`, the rectangle the frame passed, (765, 12) at 1024, before the renderer's +2/-2 shift): inside [0, w) x [0, h) and, with t = y when y <= h/2 else h - y, -2t <= x - w/2 < 2t (the inscribed diamond; left corner x 765, right corner 1020, top y 13, bottom 139 at 1024). The inverse `0x100333f0`: X = (x - 6)·1.03, Y = (y + 0)·1.03 (`+0x8f18` = -6, `+0x8f1c` = 0, `+0x8f20` = 0x3f83d70a above 800 px; -5, 2 at 800), D = X - w/2; P = N/h·(Y + D/2), Q = N/h·(Y - D/2) with N = 256 (the renderer's size; `0x1003347e`..`0x100334ce`); turn 0 (jump table `0x100336a4` → `0x100334e1`): x0 = ftol(W·Q/256), y0 = ftol(H·P/256); then through the zoom window (`0x1003357e`..`0x10033685`, the same clamp as `0x10033710`): x = ftol(((x0/W - 0.5)·z + mid of the `+0x20` axis)·W), y likewise with the `+0x1c` axis; finally `0x10033ad0`(y, x) recentres the minimap there (`+0x1c` = y/H: u is the world's y, as OQ-635 chose). The click handler (`0x10006d19`..`0x10006d9d`, the same at `0x1000b989`) puts the camera at (-x·1.0, 0, -y·1.0) (`0x100040f0`; 0x10086320 = 1.0): the tile's integer position, then recomputes it (`0x10003be0`). A press on the rectangle outside the diamond goes on to the map's input (`0x10006db7` → `0x10002340`, state 1; OQ-640). The minimap zoom: `0x10033770`(param) subtracts (param 1) or adds the float 0.15 (`0x10087bd4`) and `0x10033710` clamps to [0.15, 1.0] with the same constant as the minimum. The magnifier buttons' rectangles are not in the code read (View.dll asks a position lookup, vfunc `+0x24`, `0x100343fd`); measured by sprite-SAD on `hud_idle_running`: id 40 at **(796, 135)** mean 1.81, id 42 at **(826, 153)** mean 1.19 (pressed frames 41, 43 at 19.8 and 16.5: idle is drawn), both 19 x 21.

What the rebuild does now (`src/app` `minimap.hpp`, `viewer.cpp` `minimap_press`; data `zoom_range 0.15 1.0 0.15` and `click -6 0 1.03` in `data/ui/minimap.txt`, `lupe_plus` / `lupe_minus` in `data/ui/hud.txt`, not drawn - `WELTKARTE`'s own pixels show them):

| Control | Rebuild | Original |
|---|---|---|
| Left press on the diamond | `minimap_hit` + `minimap_world_at` (turn 0, the current minimap window), the camera centred on that tile at sea level (`centre_on_tile`, then the view's clamp); the window follows the camera every frame, which is the original's recentring. No drag (a move while the press is held does nothing, its release is the UI's). | `0x10006d19`, `0x100332f0`, `0x100333f0`, `0x10033ad0` |
| Right press on the minimap | unchanged: the window's own handling (it pans on a drag, and a right click with the construction menu open is its step back). Whether the original's events 1 and 3 are left and right is OQ-631. | events 1 / 3 |
| Press on the parchment outside the diamond | taken by the UI, nothing happens: `ui_hit` treats `minimap_click_area` (the drawn raster and the wrapper rectangle, x 765..1024 since the fix round (1023 before), y 10..139 at 1024, right-anchored) as chrome, including the strip left of the right column (x 765..778). The packet's choice; the original passes it to the map (OQ-640). | `0x10006db7` |
| Top-right + / − (ids 44 / 46) | `minimap_zoom_step`: z − / + 0.15 in float, clamped to 0.15 .. 1.0; the box grows by 1/z | command 0x1b, `0x10033770`, `0x10033710` |
| Magnifier + / − (ids 40 / 42) | one built level per press (`magnifier_target`): the stepped zoom moves one group (0..2), the continuous zoom eases (`zoom_to`) to the next built level from its target (between levels: the next one in that direction; closer than level 0 only − acts). The viewer's built levels 0..2 are the original's three (tile widths 84, 42, 20); the box's level factor follows (2^level at the levels). | commands 0xa / 9, `0x10007710`, `0x10007692` |
| Chevrons (ids 48 / 50) | taken and dead, waiting for the render lane's **VIEW-TURN-1** (nothing of the turn is built here) | commands 0xc / 0xd |
| Pressed frames (id + 1) | not drawn: the HUD has no pressed state for any `0xb09` gadget yet (follow-up; 2.2's two-frame rule) | `0xb09` |

The minimap's viewer is the play runner's local player whenever one is set: the ship start's participant, or `--player` (FISHER-VIEW-1, merged into the branch for C); otherwise B2's first human slot (the runner's default `developer_player` is not the human in a shipped scenario, OQ-629 (b)). Fact `minimap.viewer`; wiring case `ui_minimap_player` (lan5 `--player 3`: viewer 3, 0 without the change). Checks: `minimap_test.cpp` (hit test corners, the inverse at two windows and two world shapes, its round trip with the drawing transform within 4 tiles, the click area, the zoom steps, the magnifier levels, the HUD hit) and the wiring cases `ui_minimap_player` and `ui_minimap_controls` on lan5 (1431 tiles: a click at (900, 50) puts the camera at tile (410, 464), as computed by hand; the strip outside the diamond moves nothing; the box 9 → 13 px wide at z 1.0 → 0.7; tile width 20 → 42 → 84 → 84 → 42).

**Fix round (2026-11-16, after the orchestrator's History Edition check).** Evidence crops under `cache/ui-minimap-1/fix/` (not in git). The History Edition's lossless capture `he_s10_hud_idle_win.png` (2542 x 1351, UI scale 1.0, `ANNO_UI_HE_CAPTURES`) is the reference for F1 and F2; the recording `ui_5.mp4` (0.75 scale, H.264) only where the capture has nothing.

- **F1, the scroll's rolled left end.** Not drawn before: `fenster.tex` id **3** (34 x 47, the texture after `WELTKARTE` id 2; no ACCESS name; GOG and HE files identical, header word `+0x1a` = -6, meaning unread). It hangs left of the column; without it the chrome stopped at the parchment quad's rectangle (the "rectangular notch"). Measured: sprite-SAD on GOG `hud_idle_running` at **(745, 49)** mean 7.61 (37.9 one pixel off), on the HE capture right-anchored at the same place median 0, mean 0.78. The code that draws it was not found (`WELTKARTE` and id 3 are never looked up by name). Rebuild: `hud.txt` `minimap_curl`, drawn after `WELTKARTE` by the oracle mock, the CPU frame, the GPU overlay and `tools/ui_layout.py`; a press on its rectangle is chrome and does nothing (`ui_hit`, step C's rule; OQ-640).
- **F2, the parchment's grain.** The HE parchment quad is the code's strip: W' = x1 - x0 + 2, H' = y1 - y0 + 2 from the rectangle's corner (`0x10031ef1`..`0x10031f1d`; vertices at x - 0.5, the float 0.5 at `0x10086340`; the map quad's vertex buffer is filled from the same positions, `movsd` at `0x10031fc6`), i.e. **258 x 130 at (767, 10)**, sampled bilinearly. Fit on the HE capture (a bilinear sampler over the parchment's 256 x 133, inside the diamond, islands left out): mean 8.3 sum-RGB for (767, 10) 258 x 130 against 12.3 for B1's (768, 11) 256 x 128 and 10.5-17.7 for the neighbours; a free fit gives scales 0.9928 x 1.0232 (256/258 = 0.9922, 133/130 = 1.0231). At that size the samples fall between texels, which is the HE's smoothness (a one-texel checkerboard keeps 192 of its 200 step in the GOG fit and 85 in the 258-pixel quad). The GOG shot still prefers B1's fit (11.0 against 12.2, RGB565): its D3D7 frame rasterises differently. The colour depth differs as well: the HE capture's minimap colours are 8-bit, palette entry = floor(channel * (k + 16) / 31) (entries (9, 3) (109, 104, 0), (9, 4), (9, 5), (9, 6), (1, 3) exact, 1943 of 2518 saturated pixels exact; none on the RGB565 grid), the map quad point-sampled (hard island edges). Rebuild: `minimap.txt` `parchment_quad 0 0 2 2` and `colour_depth 32` (HE, the defaults); `parchment_quad 1 1 1 1` with `colour_depth 16` is the GOG frame (B1's rows-only filter, 1:5:5:5 palette, markers and parchment in RGB565). On the HE capture's parchment (islands left out) our frame's median difference went from 18 to 1 sum-RGB. The map quad keeps B1's geometry ((768, 11) 256 x 128 and the diamond transform): the code shares the parchment's positions, so the HE probably draws it 258 x 130 too, which the islands' overlap cannot separate (0.8 %; OQ-628). Sure: high for the parchment quad and the 32-bit palette (exact pixels on a lossless capture, matching the code), medium for the map quad.
- **F3, the settlement's lower edge.** No change. Against `ui_5.mp4` at 166 s (aligned as B2), with the HE's view box and ship marker and ours left out: 36 of our teal pixels remain, 22 on HE teal, 13 on HE parchment (12 of them one pixel from HE land); our olive shows the same fringe (48 of 315 on HE parchment, all within 2 px). A tile's shade decides whether a cell is drawn and the owner only its colour, so a teal cell past the land would need a wrong shade, which olive cells would show as well; the fringe is the video's chroma blur and OQ-636, not the owner rule. Sure: medium (a compressed 0.75-scale video; no lossless HE frame of the tutorial).
- **F4, the start camera.** `[CAMARAPOS]` is `CAMERA_MEMORY`'s slot, not a start camera: AnnoScene reads its `x,y` into the script object `+0x64`/`+0x68` (`0x10016f13`..`0x10016f50`), `CAMERA_MEMORY` writes them from the frame's camera (`0x10015a00`: negated, truncated), `CAMERA_RECALL` sends them to the frame as command 3 with x | y << 16 (`0x10015a50`), and the script writer saves them (`0x100161da`); nothing else reads them, and every shipped scenario and save has `0,0`. Command 3 (`0x10007885`) **sets** the camera to that tile (`0x100040f0` with (-x, 0, -y), then `0x10003be0`), like a minimap click; the sentence above calling it a scroll by a delta was wrong. Where the original's start camera and zoom come from is still OQ-638. Measured on `ui_5.mp4`: the first game frame is at about 28.5 s (the menu and loading screen before it); the box is level 1 (B2) and sits on the player's ship marker (box centre about (904, 72) at 1024 against the marker (905, 70)). So the HE starts at zoom level 1 on (or next to) the player's ship; the rebuild's `--play` starts at group 2 (42 / 20 = 2.1, the 2.08 the orchestrator correlated) at the centre of the scene's bounds.
- **F5, the closest zoom** (the orchestrator, 2026-11-16): the rebuild's viewer zooms to 84 x sqrt(2) = 118.8 px tiles (`kZoomEndFactor`, `zoom.hpp`), the History Edition's closest is 84 px (group 0): the 1.42 measured. Not changed here (owner's decision). The box there uses the factor 84 / 118.8 = 0.71 (`minimap_level_factor` beyond the closest level), smaller than the original's level-0 box, which the original never shows.

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

Type 501 panel upkeep reads **0** on `r2_info_pioneer_house` (glyph-exact, UI-INFO-1; an earlier reading of 5 was wrong); both `PROD3` u16 are 0 (house type). F1.2 info-panel costs match HEAD for 1310, 1314, 104, 809, 708, 1302 and 810. Type 104 at farms (1,1) is a placeable forest tile (`PRODTYPE` `ROHSTWACHS`, gold 5). Type 708 is a potato field. Clicking a locked `?` in each category (`f12_locked_*`) does not open the info panel.

## 4. Placement and demolish (as the UI presents it)

Measured rectangles in section 2.5-2.6. Summary:

- Select a U cell: info panel at (804, 302) 220 x 466 plus placement mode; cancel-X `INFOEXIT` at (984, 725) 33 x 38.
- Valid preview: yellow building footprint (`f13_place_land`). Refused: yellow empty rectangle (`f13_place_water`). Left click on valid land places and stays in mode (`r2_place_*`); ESC / right click leave it (nothing armed), the cancel-X closes only the panel and keeps the building armed (section 2.6, UI-PLACE-1).
- Rotate: DOT / COMMA (`f13_place_rot_dot`, `f13_place_rot_comma`). CTRL alignment is in the manual (p15) and was not captured (OQ-546).
- Demolish: pickaxe `ABRISSICON` at (970, 710) 53 x 53 (id 659 idle-in-menu, id 660 while demolishing). Tooltip family `TOOLTIP:ABRISS` (4 lines). Cursor `tCursorAbriss` / `Abreissen_Gebaeude.tex` 73 x 65 as the OS cursor, not a blit. It works with nothing armed and toggles; a cell chosen while demolishing ends the demolish mode (section 2.6, UI-PLACE-1).

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
Section 5.1 (UI-WINDOW-1) now proves the right-chrome and status-bar anchors.

UI.2’s own scale (W5) is a rebuild choice, not an original fact. The History Edition's scale rule
(the `UI_SCALE` setting, its 0.1 step-down, the bilinear stretch, the panel at `Sizex − 245`, the
click mapping) is in `he-ui-scale.md`.

### 5.1 Placement at other window sizes (UI-WINDOW-1, 2026-11-14)

Static reads with pefile + capstone (GOG 32-bit, VAs; HE x86-64, RVAs) and sprite-sad on the HE
captures (`he_s10/15/20_hud_idle_win.png`, 2542 × 1351 client, outside git). The scale rule and
the click mapping are `he-ui-scale.md`; the rebuild's code is `src/app` `ui_transform.hpp`
(transform) and `ui_layout.hpp` `place_hud` / `ui_hit` (placement, hits); the anchors are data
(`data/ui/README.md` "Anchors").

| Part | Rule | Evidence |
|---|---|---|
| Right chrome | x = W − 245, top-anchored, 768 rows | HE `View.dll` 0x3b428..0x3b455 (x = Sizex − 245; 779.0 below 1024); HE captures at 1.0/1.5/2.0 |
| Status bar row | y = H − 35; the rebuild keeps the icons' and counters' 1024 × 768 x (the original lays them out wider at 1152/1280, OQ-620; not implemented) | `gold` at (0, H − 35), sprite-sad 0.0 on `he_s10_hud_idle_win.png`; GOG `AnnoFrame.dll` 0x10063000/0x10063013 width factor 1.18/1.3 on base widths 400/436 (0x100636e3/0x10063725) |
| Status bar graphic | per width: 1152 → STATUSBAR02 (873), 1280 → STATUSBAR03 (1001), any other → STATUSBAR (746) | GOG `AnnoFrame.dll` 0x10063125..0x10063153 (`cmp eax, 0x480` / `0x500`, ids 0x38 / 0x39 / 0 in `statusbaraddon.tex`) |
| Question mark | x = the bar's end: 746 at 1024, W − 279 elsewhere | 873 / 1001 = W − 245 − 34; HE at 2263 = 2542 − 279 (sprite-sad 0.0) |
| Column below 768 | 1152 → FUELLER02 (96 rows), 1280 × 960 → FUELLER01 (192), other 1280 → FUELLER03 (256); 01/02 are 03's top rows | GOG `AnnoFrame.dll` 0x10063304..0x1006331f; decoded pixels byte-identical; HE: FUELLER03 at (W − 245, H − 256), sprite-sad 0.0 |
| Pop-ups | kept at their 1024 × 768 position | unproven, OQ-618 |

What the original never shows (wider than 1280, taller than 1024) is a rebuild adaptation. The
original's code would pick STATUSBAR (746 px) for any width other than 1152/1280 (the rule above),
leaving the bar short of the question mark; the rebuild instead repeats the widest graphic that
fits (1001 px) to W − 279 (OQ-614) - at 2542 × 1351 with seams at x 1000/1001 and 1262 that HE's
capture does not have - and repeats `MAINUNTERGRUND` between the
panel and a bottom-anchored FUELLER03 (OQ-615). HE uses graphics of its own there (`fenster.tex`
`BOTTOM_BAR_TILEABLE`, `BOTTOM_BAR_END`, `SIDE_BAR_TOP`, `SIDE_BAR_TILEABLE`, `MAX_PIC2`; not in
the GOG files). Below 1024 × 768 the UI layer stays 1024 × 768 and is fitted into the window with
HE's formula (OQ-616).

Hits: the right column over the whole height, the status bar row up to the question mark's end,
and the pause parchment while paused are chrome; a left press there and its release never reach
the map. A hit maps back to the 1024 × 768 frame where the reference has a rectangle (the column's
top 768 rows, the bar's first 746 pixels, the question mark) and goes to the construction menu and
the tabs there; the construction tab opens the menu (as key B does) and closes it (as Esc does);
closing the menu either way also turns off a building tool a cell armed.

### 5.2 The status bar counters (UI-WINDOW-1 fix round R2, 2026-11-14)

Evidenced: the materials (tools, wood, bricks, marble) and residents, icons and figures, are
absent before the first founding (`hud_idle_running`) and present on every screen once a city
exists, with or without the construction menu (`r2_player_menu`, `r2_tab_military`,
`r2_construction_military`; HE `he_s10_hud_idle_win.png`). The owner states first-hand that they
show the stock of the island the view is centred on. **Not evidenced:** what the original shows
over open sea and over another player's island (no shot is taken there), the exact centre point
and the update timing - OQ-622.

**Rebuild rule, owner decision 2026-11-14 ("Always show my city", fix round R4):** the five
counters show on every screen while the local player owns a city: the city on the island under
the view's centre when that island holds one of the player's cities, otherwise the player's
first city in (island, city) order. Hidden only before the first founding. (R2's rule hid them
away from the city's island, so a player who did not scroll there never saw them.) The gold
counter is unchanged.

Test note (fix round R5): `live_screen_matches_oracle` (`tools/test/live_screen_compare.py`)
compares `hud_idle` in the state of `hud_idle_running`, before the first founding: a second
launch on the ship start (`lan5-ship-a.json`, seat 0, `live_screen_hud_idle.script`), exact, no
masks for the counter icons. The two construction screens stay on `lan5.txt`, whose developer
city shows the counters their oracle shots draw.

### 5.3 The four tabs (UI-WINDOW-1 fix round R2, 2026-11-14)

The tabs are switches: one is always selected (`MAINTAB` `frame_selected`, the others
`frame_idle`). From the GOG shots (section 1 rows 1 and 6-9, section 7): the start state is the
player tab with its overview panel (`hud_idle_running`); the bust tab and key **D** show the player
list (`r2_tab_military`, `r2_player_menu`); "?" shows the info panel (`r2_tab_help`); the gears tab
and key **O** the main menu (`r2_options`, `options_key_O`); the construction tab and key **B** the
construction menu. A click on the selected tab changes nothing, except the bust from the
overview, which opens the list (`r2_player_menu`). Each panel's static chrome is in `data/ui/hud.txt` (`panel`), each element
matched by ACCESS id on its shot (exact after the RGB565 reduction, or mean abs 0.00-0.35 for the
two big backgrounds); `tools/test/ui_panels_compare.py` checks them. Names, figures, list rows,
the diplomacy cluster and the info emblem are not drawn (UI-TABS-1). Unproven parts (a second
click on a tab, Esc's return, the list background, the crest per player): OQ-621.

**Corrected by section 5.4 (UI-TABS-1, 2026-11-16):** the exception above does not hold. The
bust shows the player view at the sub-tab it showed last (code read below); `r2_tab_military` shows
the list because the runner had pressed key D before it (`actions.jsonl` of run
`d91-20261030-112532`: O, Esc, D, Esc, "?", Esc, bust).

### 5.4 The sub-tabs (UI-TABS-1, 2026-11-16)

Each of the three panel views (the bust's player view, the "?" tab's city view, the gears' options
view) has a column of sub-tabs left of the panel, x 762, sub-tab k at y 303 + 46k, 42 x 46. Code:
GOG 2.0.0.5, static disassembly (pefile/capstone). Frames: `fenster.inc` line 1 (P). Shots: GOG
Wine runner (`cache/orig/ui-1/shots`), exact = fraction of the frame's opaque pixels within 2 of
the shot after the RGB565 reduction, idle / selected frame at each position. History Edition:
`HE_UI_1.mp4` (owner's run), sub-tab crops compared with ours at UI scale 1.0 scaled to the video
(0.75): mean abs per channel against our selected / idle frame.

| Fact | Evidence | Confidence |
|---|---|---|
| Player view: 5 sub-tabs, `VIEWINFOPIELERTAB` idle 1184 + 2k, selected 1185 + 2k | builder View.dll `0x1007f428..0x1007f507` (`cmp esi, 5` at `0x1007f504`; image property `0x198` = base + 2k, `0x197` = 1); ACCESS `VIEWINFOPIELERTAB,1184`, next name at 1194 | D, P |
| City view ("?"): 8 sub-tabs, `VIEWINFOSTADTTAB` idle 661 + 2k, selected 662 + 2k | builder `0x1006ac92..0x1006adc0` (`cmp esi, 8` at `0x1006adb9`, `0x198` = base + 2k); ACCESS `VIEWINFOSTADTTAB,661`, next 677 | D, P |
| Options view: 5 sub-tabs, `OPTION_TABS` idle 1476 + 2k, selected 1477 + 2k | ACCESS `OPTION_TABS,1476`, next 1486; `TOOLTIP:OPTION` 0..4 (`ui-options-mainmenu.md` 2.6) | P |
| A sub-tab press selects it: the handler reads the column's index (property `0x1b3`), stores it at `[view+0x5f0]`, sends message 1 to that page, hides every page and the column, adds the page and then the column on top | player `0x1007f730` -> `0x1007f600`; city `0x1006b070` -> `0x1006af30`; options `0x100786a0` -> `0x100784c0` (also sets the banner text to the page's line, `0x100784f4..0x10078523`) | D |
| A press on the selected sub-tab shows the same page again (no compare with the current index) | the three handlers above | D |
| Before switching, the player handler sends message 6 to page 1, the city handler to pages 2 and 3 | `0x1007f62a`, `0x1006af5e`, `0x1006af75`; meaning not read (OQ-643) | D / U |
| The views are persistent children of the HUD; a main-tab press shows a view again with its column's last selection | main-tab handler `0x10034ab0` -> `0x100335d0`(i): `[hud+0x5f0]` = i, page = vfunc `+0x20`(0xa00, i) = `[hud+0x5e4][i]` (`0x100194c0`), message 1; player view message 1 -> `0x1007f600` (`0x1007e9c0`, `dec eax; je`); city view message 1 -> `0x1006af30` (`0x1006a439..0x1006a44e`); options message 1 only the base handler (`0x100787f0`), its last page stays attached | D |
| ... the options view kept the speech page when key O was pressed later | GOG `f15_key_O` (after `f15_options_y510` and three more clicks): OPTION_TABS 4 selected, exact 1.00 | M-GOG, high |
| Start state: every view at sub-tab 0 | `hud_idle_running` player 0 selected (1.00); `r2_tab_help` city 0; `r2_options` options 0; HE 44.5 s player 0 (11.4 vs 31.4), 107.75 s options 0 (11.0 vs 30.8) | M-GOG, M-HE, high |
| Keys (lower case, the frame's key switch): d -> HUD message 0x10 param 5, s -> 6, c -> 7, o -> 9, b -> 8 | AnnoFrame.dll `0x1000bfc4` (table `0x1000c548` / `0x1000c4e8`): 'd' `0x1000c210`, 's' `0x1000c279`, 'c' `0x1000c29c`, 'o' `0x1000c1ed`, 'b' `0x1000c233` | D |
| Params 4..7 show the player view and forward the param, which selects sub-tab 0, 1, 3, 2 | HUD `0x10033260` (table `0x10033590`: 4..7 -> `0x10033456`/`0x10033485`: main tab 1, then message 0x10 to the player view); player view `0x1007e9c0`, table `0x1007ea3c`: 4 -> `0x1007e9e6` (0), 5 -> `0x1007e9f1` (1), 6 -> `0x1007e9fc` (3), 7 -> `0x1007ea07` (2), via `0x1007eaf0` (sets `0x1b3`, switches) | D |
| ... so D = diplomacy status (1), C = city list (2), S = ship list (3) | GOG `f15_player_menu` (D) 1 selected 1.00, `f15_city_list` (C) 2 1.00, `f15_ship_list` (S) 3 1.00; tab order `TOOLTIP:SPIELERINFOMAIN_TAB_ADDON` (`ui-ships-diplomacy.md` 2.9) | D, M-GOG, high |
| Param 9 (o) shows the options view with its last page; param 3 the "?" view | `0x1003353f`, `0x10033423` | D |
| Esc (VK 0x1b) sends param 2; with the construction tab showing and no panel flag, nothing selected: param 4 = the player view at sub-tab 0; something selected: param 3 = "?" | AnnoFrame `0x1000bd18..0x1000bd51`; View `0x1003334c..0x100333ed` | D (selection branch not modelled, OQ-643) |
| Player view pages: 0 overview (`SPIELER_BILANZ_BK` its own: 0.94 exact on `hud_idle_running`, 0.01 on the list shots), 1 diplomacy status, 2 city list, 3 ship list, 4 ships for purchase; the crest is the view's (1.00 on `f15_ship_list`, `f15_city_list`) | GOG shots; `TOOLTIP:SPIELERINFOMAIN_TAB_ADDON` | M-GOG |
| Options pages 1-4: the main page's icons are page 0's (0.00-0.03 elsewhere); `OPTION_BACKGROUND` 0.18-0.36 exact under them | `f15_options_y360..y510` | M-GOG, medium (OQ-644) |
| HE: one selected sub-tab at our position in every frame sampled | player 104.5 s -> 3 (14.3 vs 27.1), 139.0 s -> 1, 141.2 s -> 2, 142.5 s -> 4; options 108.9 s -> 1, 110.5 s -> 2, 111.75 s -> 3, 112.3 s -> 4, 113.75 s -> 0 (back to the main page); margin 12-17 per channel at every tab | M-HE, high |
| HE: the bust at 01:44.25 showed the ship list although the player view last showed the overview | `HE_UI_1.mp4` 44.0-46.0 s (overview), 104.25 s (ship list) | M-HE; contradicts a pure "last sub-tab" rule: OQ-641 |
| HE: "?" with nothing selected shows only the castle background | `HE_UI_1.mp4` 50.0 s, 142.75 s | M-HE (OQ-642) |

**Rebuild (UI-TABS-1 stage A).** `data/ui/hud.txt`: `view` on the three main tabs, `view`,
`subtab`, `body`, `frame_idle`, `frame_selected` (and `hotkey` D, C, S) on each sub-tab, `hotkey O`
on the gears; the counts are the sub-tab elements. `src/app` `hud_tabs.hpp`: the view showing and
one sub-tab per view; a main-tab press or key O shows a view at its last sub-tab, a sub-tab press
selects it (on the press), D/C/S show the player view at 1/2/3 (classic surface only), Esc on the
construction grid (and the right click there) gives the player view at 0. Both UI paths draw the
selected sub-tab's frame, the others idle, and the selected sub-tab's `body`: player 0 `player`,
1 `player_list`, city 0 `info`, options 0 `main_menu` (unchanged panels); the other pages are
named placeholders (column, crest, and in the options view `OPTION_BACKGROUND`; OQ-644). Facts:
`hud.view`, `hud.subtab`, `ui.tab` (= the body). Checks: `hud_tabs_test.cpp`,
`tools/test/viewer_wiring/ui_subtabs.script`, `tools/test/ui_subtabs_compare.py` (the GPU layer's
composer against the GOG shots above, the window's CPU path against the composer, exact). Not
built: the pages' bodies (the panel packets), tooltips, the header plank with the view's name
(not drawn on any view's page since R2), the "?" view's selection-dependent content (OQ-642).

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
| HUD idle, minimap, tabs, status bar | captured | none (no UI library); the app draws the minimap since UI-MINIMAP-1 B1 and its controls work since C (section 2.9) |
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

Place public (0,0) type 501, ESC, click the house: right panel with portrait and need icons (`f15_selected_house`). Portrait graphic is `ToolGfx/Pioniere.tex` id 1 100 x 108 at **(812, 355)** (grey-key mean 34.2; the live needs digits sit on the sprite). Panel plate: `VIEWINFOPIONIERMAIN_BK` id 974 245 x 467 at **(783, 252)**. `WOHNHAUS_PLATTE` id 883 244 x 469 is the other plate (opaque mean 32 at (779, 244); not the better origin). `TOOLTIP:WOHNHAUSINFO` 3 lines. `VIEWINFOPIONIER_TAB` id 975 42 x 46.

**G6 (2026-11-11 fix review, finding 7): the panel plate's own grey-key mean, 17.8, is not reproduced.** The defined grey-key rule above (§2), run on `f15_selected_house` at this same (783, 252), gives **32.07**, not 17.8 - the same figure on `f22b_help_house` (a second, independent shot of the same panel). Calibrated against two committed *plain* sprite-sad figures on `hud_idle_running` (zoom + id 44 at (959, 7), compass west id 48 at (965, 140)): this method gives 0.962 and 1.029, not the committed 2.885/3.088 either - so the gap is not specific to grey-key or to this one panel, and this session's own implementation is not shown trustworthy enough to use for a new measurement. No element here moves on either the 17.8 or the 32.07 figure; see OQ-567 for what this means for the open panel-id questions.

**H1 (2026-11-11, docs follow-up after the acceptance, finding 1): G6's calibration conclusion above is wrong - correcting it, not the grey-key figure itself.** G6's 0.962/1.029 are the committed 2.885/3.088 **divided by 3**: G6's own script divided every result by 3 regardless of method, which is correct for grey-key but wrong for the plain zoom +/compass west figures, which (§2, H1) are per pixel and never divided by 3. Measured with the plain method's own unit (sum, not divided by 3), on both kept copies of `hud_idle_running`: zoom + gives **2.885** and compass west gives **3.088** - both reproduce the committed figures **exactly**, on both copies. So: the plain method is calibrated (2 of 2 reproduce), and the gap **is** specific to grey-key (and/or to this one panel) after all, not a broader measurement problem. What does not reproduce, still, is the two grey-key figures on `f15_selected_house`: id 974 at (783, 252), this document's own committed 17.8, measures **32.07 per channel (96.21 per pixel)** on both kept copies; the portrait's committed 34.2 (`ToolGfx/Pioniere.tex` id 1 at (812, 355), this section's own opening paragraph above) measures **45.52 per channel (136.56 per pixel)**, also on both kept copies. Script: `cache/h1-sprite-sad-measure.py` (sha256 in the REPORTS entry for this part). No element moves on any of the four figures; OQ-567's own new paragraph below restates the plain ones as calibrated and leaves the grey-key ones open.

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

## 11. Status icons above buildings (UI-HARVEST-ICON-1 stage A, 2026-11-17)

**Scope.** The spinning icon the original puts over a production building that is switched off, lacks
raw material, has a full store or has no road. Trigger HE `he_01.mp4` 8:50-9:13 (`he_01-report.md`, crop
`c5_icon.png`): the owner's "nothing to harvest" icon over the forester. GOG code by address
(`AnnoWelt.dll`, `AnnoGame.dll`); HE frames are pixels only. Labels: **D** disassembly, **P** parsed
file, **H** HE frame, **L** likely (stated reason), **U** open (OQ given). Scripts (in the worktree's
git-ignored `cache/uhi/`, not committed): `sheet.py` (decodes `FigurenHeader.dat` frames with
`tools/figuren.py`), `cmp.py` (HE crop beside frames), `calls.py` (direct-call scan), `anim.py` (`ANIM`
and `BEWOHNTYPE` records).

### 11.1 The icon set: four figures, one per status code (D + P)

`AnnoWelt` `0x10073010(code, on)` takes the figure id from the u16 table at **`0x100c92b0`**, indexed
by the code (`movzx ecx, word [edi*2 + 0x100c92b0]`, `0x10073114`): `1555, 1570, 1572, 1571` (P: the 8
bytes `13 06 22 06 24 06 23 06`). The figures in `FigurenHeader.dat` (P, `tools/figuren.py dump`):

| Code | Figure | `BEWOHNTYPE` | Colour table (palette 1; palette 0 is `Stillegen01_01.PCX` in all four) | Picture (decoded, group 0) | Meaning | Help text (P, `help/*.htm`, English) |
|---|---|---|---|---|---|---|
| 0 | 1555 | `PRODSYMBOL` | `Symbole\Stillegen01_01.PCX` (palette 0) | a spinning orange "Zzz", 20 × 51 | production switched off | `Produktion_stilllegen.htm`: shut down "by ... selecting the "Zzz" symbol" |
| 1 | 1570 | `PRODSYMBOL` | `Symbole\KeinRohstoff01_01.pcx` | a framed sheaf of grain and a green leaf, with an orange question mark spinning on its top right corner, 48 × 43 | raw material missing | `Auslastung.htm`: a "raw material lacking" icon; `Auslastung_F_und_P.htm`: farms "with a low workload" (unsuitable climate, poor soil, too few fields or forest in the service area) |
| 2 | 1572 | `PRODSYMBOL` | `Symbole\KeinLager01_01.pcx` | a framed barrel and a sack, with the spinning question mark, 42 × 45 | storage full | `IH_Lager_Voll.htm`: "a question mark and a full barrel over the building" |
| 3 | 1571 | `PRODSYMBOL` | `Symbole\KeineStrasse01_01.pcx` | a paved tile with the spinning question mark, 58 × 47 | no road (L, see 11.2) | `IH_Betrieb_Strasse_Fehlt.htm`: "the missing road sign ... over the workshop" |

- Each figure has one sprite set of **16 frames per zoom group** (group 0 / 1 / 2: 1570 is 48 × 43,
  24 × 22, 12 × 11; anchor 24,34 / 12,17 / 6,9), format `BSH_RGBV`, sprite-set prefix kind byte 9
  (`tHausAnim`, `annodisplay.md` §5), `ANIM` field = (0, **16, 80**) (P; read as 16 frames and 80 ms, the unit and the real period are U, OQ-722).
  The 16 frames are one
  turn of the question mark (or the Zzz) about its vertical axis; the frame box stays still.
- **No ToolGfx ACCESS name, no tooltip** (P): these are world figures, not HUD textures. No `*.inc`
  under `ToolGfx/` and no `Data/*.dat` names them, and `Texte.dat` has no section for them (a byte
  search for `rohstoff|stilleg|keinlager|keine strasse|prodsymbol` over `ToolGfx/*.inc`,
  `Data/*.dat`, `Texte.dat` finds only goods names and the `PRODSYMBOL` type name in `AnnoGame.dll`).
  The only texts are the active-help conditions `COMPHELP_ROHSTOFFFEHLT`, `COMPHELP_LAGERVOLL`,
  `COMPHELP_STRASSEFEHLT` (`InstantHelpLinks.txt`, read by `AnnoFrame.dll`), which open the help pages
  above; how they are raised is not part of this packet.
- **The HE "vertical red-to-yellow bar" is the question mark seen edge-on** (H, high confidence). The
  HE crop at 8:56 matches frame k = 13 of 1570 best (its yellow top shows in the crop; k = 5 is mostly
  red-orange) in shape and colour (the HE picture is scaled, so not pixel for pixel): a thin
  orange-to-red column at the frame's top right, red at the bottom where the question mark's foot is.
  At 8:52 the cursor hides part of the icon; only the curl of the question mark is visible, which fits
  frames k = 1-3 or 14-15. Other orange icons in the same HE frames (other foresters) were not checked. The sheaf,
  leaf and the stones under it are the same in both HE frames and in every decoded frame (`cmp.py`).
  So **the bar measures nothing**; there is no growth or production bar on this icon. (The 10-segment
  bar over a farm's ghost, `he_01-report.md` 5:56-6:30, is a different thing; figure 1557
  `ERZSYMBOL`/`Fruchtbarkeit.PCX`, 11 frames, is a candidate for it and is not treated here.)
- Other `Symbole\` figures (ore and mine signs 1556, 1560-1564, 1661, whale 1559, volcano 1565, route
  points 1566/1567/1624/1625, flags 1569, road connection 1573/1574, group numbers 1622, experience
  1623) are not status icons: no table entry and no call of `0x10073010` uses them.

### 11.2 The trigger of each icon (D)

The state lives on the building: byte **`+0x0d`**, bits 0-4 = icon bits, bits 5-7 = the current
status code (`production.md` §3; sim: `StatusTracker`). Only three call sites change an icon bit
(direct-call scan of `AnnoWelt.dll`, `calls.py`; indirect calls were not searched):

| Icon | Set by | Cleared by | Evidence |
|---|---|---|---|
| 0 Zzz | the switch helper `0x10072a60(old, new)` (caller pushes new, then old: workshop `0x100684cd`-`0x100684cf`) calls `0x10073010(0, old)` at `0x10072aca`. The helper returns early when `old == new` (`0x10072a6e`), so the icon is on exactly after a switch-off and off after a switch-on. Two-line buildings pass the **combined** state (`0x100690b0`, `0x1006910c`: new = this line or the other line), so the Zzz shows only when every line is off. Reading the byte 1 as "running" is L; it agrees with `+0x0c` bit 3 = (new == 0) at `0x10072ae4`-`0x10072af3`. When the owner gate fails (`0x1007302f`) the bit is not stored at all. | the same call when switched on | D `0x10072a60`-`0x10072b44`, `0x100690b0`, `0x1006910c`; switch callers `economy-booking.md` |
| 1 KeinRohstoff | the status helper `0x10072ed0(code 1, t, repeat, input)` once the condition has lasted `repeat` ticks: `0x10073010(1, 1)` at `0x10072f8b` | `0x10072fb0(0x0e)` (`0x10072ff1`), which acts only when a bit of the mask is set; its callers are the production steps (L for "a step that made something": not every call site's gate was traced, e.g. `0x1006a0d4` is gated by `+0x29 == 0`) | D; `production.md` §4.1 |
| 2 KeinLager | the same helper with code 2 | the same clear | D |
| 3 KeineStrasse | the same helper: a due code 1 whose input is **not** a crop (goods `0x38`-`0x4b`) or an animal (`0x89`-`0x8d`) calls `0x10071f40(input, 0)`; a due code 2 calls `0x10072390(input, 0)`; a **zero** result rewrites the code to 3 (`+0x0d & 0x1f` OR `0x60`, `0x10072f77`-`0x10072f83`) and raises icon 3 instead | the same clear | D for the rewrite; the meaning "no road" is **L**: the figure is `KeineStrasse`, and the help page says the road sign shows when a workshop "has no connection to a road". What `0x10071f40` / `0x10072390` test is **U** (OQ-720) |

Rules around it (D):
- **Repeat intervals** (the condition must last this long before the icon appears; `push` before
  each `call 0x10072ed0`): workshop, two-output and weapons workshops 45 000 t for both codes; farm
  `0x1006ea90` 60 000 t missing input, 45 000 t full; mine 30 000 t missing deposit, 45 000 t full;
  small farm 75 000 t missing input (`0x1006f57c`), 45 000 t full (`0x1006f5f3`); whaler tick
  `0x10073720` 90 000 t missing input (`0x10073a5a`), 45 000 t full (`0x10073ad0`) (the whaler is
  unported, `docs/sim/state.md` Appendix C). Every direct caller passes code 1 or 2 and input index 0;
  code 3 only comes from the rewrite, and nothing sets bit 4.
- **A repeat above 65 535 never falls due.** The due test masks the elapsed time to 16 bits
  (`and ecx, 0xffff`, `0x10072f1a`) and returns while `repeat > elapsed` (`jg`, `0x10072f20`). So the
  small farm's icon 1 **never appears** (its status code is still set; `production.md` §4.5, ISS-150),
  and neither does the whaler's icon 1 (a new case of the same issue; the whaler is unported). Their
  icon 2 (45 000) works. The sim's `StatusTracker::report` keeps the u16 elapsed time and so
  reproduces this.
- **The elapsed time wraps mod 65 536.** If more than `65 536 − repeat` ticks pass between two reports
  of a still-set code, the wrapped elapsed time can fall below `repeat` again and the icon comes later
  (the sim reproduces this too).
- **Priority.** A report with a lower code than the current one is ignored (`0x10072ee3`), so once
  "storage full" is current a later "raw material missing" does not replace it until a production step
  clears both.
- **Owner gate.** `0x10073010` does nothing (and stores no bit) unless `[building+0x10].slot+0x1c` →
  `slot+0xc4(3)` returns 1 (`0x1007302f`). Slot `+0xc4` is `AnnoWelt` `0x1001bea0` (vtable
  `0x100c2238`), which returns the u16 at `[ecx+0x7a]` for argument 3 (D). That this object is the
  player and the field its Volk (human and computer seats) is OQ-033's resolution (L, not re-traced
  here). Who may **see** the icon is a separate question (OQ-723).
- **The forester case (HE).** The forester 801 is a `PLANTAGE` farm whose input is `BAUM` (goods 70 =
  `0x46`, from `forestry.md` §2.1, `0x1006ee06`-`0x1006ee56`, not re-disassembled here), inside the crop range, so its missing-input report never reaches the road
  test: it always shows icon 1, `KeinRohstoff`. The farm reports code 1 when a step has
  `raw << 7 / need0 < 64` (for 801: raw stock below 16 of the 32 needed per step) and the store is not
  full (`production.md` §4.2). Young trees cannot be cut (`forestry.md` §2), so the workers bring
  nothing back, the raw stock stays at 0, and after 60 000 t of this the icon appears; the first step
  that makes wood clears it. This is the owner's "nothing to harvest yet" (H 8:50-9:13 with D).

### 11.3 How it is drawn (D, with open points)

`0x10073010(code, on)` (read instruction by instruction, `0x10073010`-`0x100731d1`):
1. Owner gate (above). Then the building's footprint `B` via its slot `+0x78`: u16 x `B+0`, u16 y
   `B+2`, u8 width `B+4`, u8 height `B+5` (widths read with `and 0xff`, `0x10073129`, `0x10073166`).
   The container `C` = island (`city+0x10` → slot `+0x18`) → slot `+0x118` (`0x1007304a`-`0x10073051`).
2. **Remove the old icon.** If any icon bit was set, `C.slot+8(&B, 8, list)` lists the objects in the
   footprint (the argument 8 is a kind or a list size, not resolved); each whose slot `+0x1c` (its owner) is this building is removed with
   `C.slot+4(obj, 1)` (`0x10073059`-`0x100730a3`).
3. Store the new bit (`+0x0d` bit `code` = `on`, `0x100730a5`-`0x100730cc`).
4. **Create one icon, for the lowest set bit among 0-3** (`0x100730d3`-`0x100730e4`; bit 4 is never
   drawn). So Zzz wins over the others, then KeinRohstoff, KeinLager, KeineStrasse. The figure type is
   `game.slot+0x6c(table[i])` (`0x1007311f`). The call sets or clears bit `code` and then draws the
   lowest set bit, so **the drawn figure need not be the passed code**:
   - switching on (clearing bit 0) while bit 1 is still set shows KeinRohstoff at once;
   - a still-set bit 1 hides a later bit 2 (a code can rise from 1 to 2, the higher code is accepted)
     until `0x10072fb0(0x0e)` clears both;
   - a call with `on = 0` for a code whose bit was clear still re-creates the icon of the lowest set
     bit.
5. Position (floats, x87): x = `B.x + B.w · 0.5`, y = `B.y + B.h · 0.5` (constant `0x100c1288` = 0.5),
   i.e. the **footprint centre**; height = `type.slot+0x58()` + building byte `+0x0f` · **0.04**
   (constant `0x100c92ac`). Type slot `+0x58` is `AnnoGame` `0x10007d50` = `fld [ecx+8]`, a float at
   type `+0x8`; who writes it, and what `+0x0f` holds, is **U** (OQ-721). The 0.04 factor is the one
   that turns a height byte into world height for island items (`annowelt.md`, "Drawing"), so `+0x0f`
   is **L** the building's ground height byte.
6. `C.slot+0(&pos, type, &obj)` creates the object; `obj.slot+0x28(building)` makes the building its
   owner (`0x100731b8`-`0x100731c5`).

Consequences:
- The icon is an **island object** drawn by the world's object pass like any animated figure
  (`tHausAnim` frame selection, `annowelt.md` "Frame selection"): frame = `tick / (ANIM_ms / 2) +
  phase`, i.e. `tick / 40` mod 16 (the divisor is confirmed in `annowelt.md` for island items; that
  the same path draws these objects is L). How long a turn takes depends on the tick unit
  (`annowelt.md` "Tick unit", open): 16 × 40 = 640 ticks, 0.64 s if the tick is a millisecond.
  Whether recreating the object restarts the turn is **U** (OQ-722).
- **Every zoom level** has frames (groups 0-2, P), and nothing in `0x10073010` looks at the zoom; that
  the object pass draws these objects at all three zooms is **L** (it draws other figures there).
  The height source `AnnoGame` `0x10007d50` is slot `+0x58` of the building-type vtables
  `0x1002185c`, `0x10021a88`, `0x10021bbc`; those vtable starts come from a heuristic scan (L), and
  derived overrides of the slot were not excluded.
- **Update rate.** The bits change only inside production steps (≥ 500 t apart) and switch commands.
  While a status is due, the helper sets the status time to `t − repeat` (`0x10072f96`-`0x10072f9b`),
  so every later step re-raises it and `0x10073010(code, 1)` removes and re-creates the icon object
  each step (D). There is no blink and no fade in `0x10073010` or `0x10072ed0`; the draw side is not
  traced (OQ-722, OQ-723).
- **Who draws it:** `AnnoWelt` creates the object; `AnnoDisplay` draws it with the other island
  objects (L: the object is created in the island's object container, and its sprite set is a
  `tHausAnim`). A use of the ids by `View.dll` or `AnnoFrame.dll` was not ruled out: the 32-bit values
  occur in those modules, but 1572 = `0x624` is also a common field offset, and the hits were not
  classified. A per-viewer filter in the draw path is not traced (OQ-723).

### 11.4 What the rebuild already has

| Part | State | Where |
|---|---|---|
| Codes 1 and 2, the icon bits, the repeat time, the priority, the clear | **in the sim** (`StatusTracker`: `code`, `icons`, `time`, `shows_icons`), serialised in the building payloads | `src/sim/include/anno/sim/production.hpp` 90-125, `src/sim/src/production.cpp` 176-205, `docs/sim/production.md` "Status messages" |
| Icon 0 (Zzz) | **derivable** from the existing `switched_on` (per line for two-line buildings) and `shows_icons`: the original's bit 0 equals "switched off" whenever the owner gate passed at the switch | sim fields exist; the sim does not set `icons` bit 0 |
| Code 3 (no road) | **missing**: the rewrite needs the tests `0x10071f40` / `0x10072390`, unread (OQ-720). `Status` has values 0-2 only | `docs/sim/production.md`: "Not modelled" |
| The figures 1555/1570-1572 | the figure decoder exists (`assets::figure`, `anno-extract figuren`) | `docs/formats/figuren.md` |
| The play snapshot | copies **no** status: `PlaySnapshot` (`src/app/include/anno/app/play.hpp` 165-230) has no building list; `status_code` appears only in the developer inspection string (`src/app/src/play.cpp` 784, 798) | - |

### 11.5 Evidence rows

| # | Behaviour | Evidence | Confidence |
|---|---|---|---|
| E1 | code → figure: 0 → 1555, 1 → 1570, 2 → 1572, 3 → 1571 | D `0x10073114`; P table `0x100c92b0` | confirmed |
| E2 | the four pictures and names (Zzz, KeinRohstoff, KeinLager, KeineStrasse) | P `FigurenHeader.dat` colour tables; decoded frames (`sheet.py`) | confirmed |
| E3 | HE's sheaf icon is 1570; its "bar" is the question mark edge-on | H `c5_icon.png` 8:56 vs frame k = 13 (best; k = 5 close), 8:52 only the curl visible (cursor) vs k = 1-3 (`cmp.py`) | high |
| E4 | `ANIM` field (0, 16, 80), `tHausAnim` | P `ANIM`, prefix byte 2 = 9 | confirmed (data); unit and period U (OQ-722) |
| E5 | icon 0 = switched off (two-line buildings: all lines off); not stored on a failed owner gate | D `0x10072a60`, `0x10072aca`, callers `0x100684c0`, `0x100690b0`, `0x1006910c`; `0x1007302f` | confirmed; "1 = running" L |
| E6 | icons 1/2 after `repeat` ticks of the condition (never for a repeat above 65 535: 16-bit elapsed, `0x10072f1a`/`0x10072f20`), cleared by `0x10072fb0(0x0e)` when a mask bit is set ("the next producing step" L) | D `0x10072ed0`, `0x10072fb0`; `production.md` §4.1 | confirmed |
| E7 | code 3 rewrite for non-crop, non-animal inputs and for full stores | D `0x10072f24`-`0x10072f83` | confirmed (control flow); meaning L (OQ-720) |
| E8 | forester → always icon 1 | D crop range `0x10072f51`-`0x10072f67`; P BAUM = 70 | confirmed |
| E9 | one icon at a time, lowest set bit 0-3 first, whatever code was passed | D `0x100730a5`-`0x10073114` | confirmed |
| E10 | footprint centre, height type `+8` (`AnnoGame` `0x10007d50`) + `+0x0f` · 0.04; frames for groups 0-2 | D `0x10073122`-`0x100731b4`; P constants and frames | position confirmed; height inputs U (OQ-721); drawn at all zooms L |
| E11 | removed and re-created on every raise; no blink or fade in `0x10073010`/`0x10072ed0` | D `0x10073059`-`0x100730a3`, `0x10072f96` | confirmed; draw side U (OQ-722) |
| E12 | owner gate `[+0x10].slot+0x1c` → `slot+0xc4(3)` == 1, slot `0x1001bea0` = u16 `[ecx+0x7a]` | D `0x1007302f`, `0x1001bea0`; Volk meaning OQ-033 | gate confirmed; "player Volk" L |
| E13 | small farm and whaler `0x10073720`: icon 1 never raised (75 000 / 90 000 > 65 535), icon 2 after 45 000 t | D `0x1006f57c`, `0x1006f5f3`, `0x10073a5a`, `0x10073ad0`, `0x10072f1a`; ISS-150 | confirmed |
| E14 | who sees it (own buildings only?) | not traced | U (OQ-723) |
| E15 | the help texts name the icons | P `help/Auslastung.htm`, `IH_Lager_Voll.htm`, `IH_Betrieb_Strasse_Fehlt.htm`, `Produktion_stilllegen.htm` | confirmed (text) |
| E16 | the two "no road" tests: candidates by callback `0x100722d0`, a carrier walk search from the building (code 1) or to it from the nearest candidate first within 24 tiles (code 2) | D `0x10071f40`-`0x100722b9`, `0x10072390`-`0x1007285a` (§11.6) | control flow confirmed; self-match U (OQ-720) |

### 11.6 The "no road" tests `0x10071f40` and `0x10072390` (UI-HARVEST-ICON-1 stage C, 2026-11-17)

Read instruction by instruction (`adis.py`, `AnnoWelt.dll`). Both functions take `(goods index, 0)`;
every caller passes index 0 (§11.2). Both return 1 (keep the code), -1 (keep the code) or 0 (rewrite
to code 3). They share the candidate callback `0x100722d0` and the world's land walk searcher. **D**
unless marked.

**Candidate callback `0x100722d0(building, ctx)`** (`ctx[0]` = goods, `ctx[3]` = count, `ctx[4 + 2n]`
= building, `ctx[5 + 2n]` = squared distance):
- type = `building.slot+0x74()`; it is a candidate when `((PRODTYPE − 1) & ~0x1f) + 1 == 0x21`, i.e.
  PRODTYPE 33-64 (the market block HAUPT, MARKT, KONTOR = 33-35 and the PRODTYPEs after them; which of
  36-64 exist as buildings is in `production.md` §3), **or** when the type's **input** 0 or 1
  (`type.slot+0xb0(0)`, `+0xb0(1)`; `production.md` §2.5: `+0xb0` = input goods) equals `ctx[0]`
  (`0x100722e7`-`0x10072316`).
- It stores the building and the squared distance between the footprint centres,
  `(x + w/2 − cx)² + (y + h/2 − cy)²` (u8 halves, `shr 1`), and counts it (`0x10072318`-`0x10072377`).
  It always returns 0 (the enumeration goes on).

**`0x10071f40(i, 0)`, due "raw material missing" (code 1, non-crop, non-animal input):**
1. `g = type.slot+0xb0(i)`, the input goods; `g == 0` → return 1 (`0x10071f61`).
2. The search rectangle: the footprint centre ± the type's radius `type.slot+0x78()`, built like the
   harvest square (`forestry.md` §2.1 item 2; rotation table `0x100722bc`).
3. The city's building enumerator (`city.slot+0x60()` → `.slot+4(0xa, &rect, 0x100722d0, ctx)`) collects
   the candidates in the rectangle. None → return **-1**: the code stays 1 ("raw material missing",
   `0x100720cf`-`0x100720de`).
4. Otherwise one search, with the building's own figure type `type.slot+0xb4(0, 0)` (its carrier):
   - the searcher is the world service `world.slot+0x20(0xc00, 0)` (`transport.md` §3: the land walk
     searcher, world `+0x6aa8`), set up for the island (`.slot+0x50`), the figure's route type
     (`figure type.slot+0xac` → `.slot+0x80`) and the callback `0x10054240` (the first goal reached is
     the result and the search stops; `transport.md` §1.3);
   - the search rectangle (`.slot+0x54(&rect, type.slot+0x78())`);
   - **starts**: the building's entrance points (`type.slot+0x98(rotation, buf)`, 12-byte records, x
     and y added to the building's tile position · `0x100c92a8`, `.slot+0x68(x, y)` each;
     `0x100721c9`-`0x10072248`);
   - **goals**: every candidate's `slot+0xf8(figure type, 0, searcher)` (its entrance goals,
     `transport.md` §3; `0x1007224a`-`0x10072281`);
   - `.slot+0x84()` runs it: a path → return **1** (code 1 stays); no path → return **0** (code 3).
   (`.slot+0xac()` releases the searcher on the success path, `0x10072295`.)

**`0x10072390(i, 0)`, due "storage full" (code 2, any input):**
1. `g = type.slot+0xac(i)`, the **output** goods; `g == 0` → return 1.
2. A fixed rectangle: the footprint ± **24** tiles (`sub ax, 0x18` at `0x100723d3`/`0x100723e1`; the
   size from the rotated footprint + 0x18, `0x100723f9`-`0x10072444`), not the type's radius.
3. The same enumerator and callback with `ctx[0]` = the output goods: candidates are the market-block
   buildings and the buildings whose **input** is this output (the consumers). None → **-1** (code 2
   stays).
4. The candidates are sorted by squared distance (quicksort, `0x100724d5`-`0x100725a4`, helper
   `0x100734f0`). For each one, nearest first (`0x100725d0`-`0x1007282c`), a search **in the other
   direction**:
   - with the **candidate's** figure type (`candidate type.slot+0xb4(0, 0)`), its route type, the
     callback `0x10054240`;
   - the rectangle around the candidate with the candidate type's radius (`.slot+0x54(&rect,
     candidate type.slot+0x78())`);
   - **starts**: the candidate's entrance points (`candidate type.slot+0x98(candidate.slot+0x64())`
     at the candidate's tile `slot+0x54`/`+0x58` · `0x100c92a8`);
   - **goal**: this building's `slot+0xf8(figure type, 0, searcher)`;
   - a path → release, return **1**; no candidate reaches it → return **0** (code 3).

So "no road" is: *a carrier of the building cannot walk from its entrances to any candidate in its
radius* (missing input), or *no market cart or consumer's carrier within 24 tiles can walk to it*
(full store). With no candidate at all the original keeps the plain code 1 or 2.

**Open, narrowed (OQ-720):**
- **Self-match (narrowed, D + L).** The enumerator is the city's building index (`city.slot+0x60` =
  `0x1002dfc0`: `lea eax, [ecx+0x144]`), whose slot `+4` = `0x10023ea0` runs the rectangle query
  `0x100232a0` over the lists whose mask bit is in the kind. Kind `0xa` = mask 8 (production, PRODTYPE
  1-32) | mask 2 (market, 33-64) (`ListLayout::original`, D `0x10023c80`/`0x10023f50`). So the calling
  production building is enumerated too, and in `0x10071f40` it matches its own input: it is a
  candidate at distance 0 and its entrance goals coincide with the search's starts. If the searcher
  answers "found" for a start that is a goal, **a due code 1 never becomes code 3** (L; how the engine
  treats a start on a goal is the open point; goals are dropped only for a building whose flags bits 4/5
  are set, `docs/sim/state.md` §5.9). In `0x10072390` the building is a candidate only when one of its
  inputs is its own output, which no production type has (L, not checked over `Anlagen.dat`), so code 2
  → code 3 works as described.
- Which PRODTYPEs 36-64 exist as buildings (the candidate block beyond 33-35) is a data question
  (`Anlagen.dat` PRODTYPEs, `production.md` §3).
- The constant at `0x100c92a8` (the tile → float factor of the entrance points) and the record layout
  of `type.slot+0x98` are taken from the existing transport reading (`transport.md` §1.3, §3), not
  re-read here.

**Can the rebuild's existing road graph answer it?** Yes, with the existing pieces (L, from the code
reading; no implementation was tried):
- the land walk searcher is ported as `sim::search` (`src/sim/include/anno/sim/walk_search.hpp`:
  starts, goals, a rectangle, a callback with `Stop` = the first goal reached, i.e. `0x10054240`), and
  the transport code already runs the same pattern for the market cart and the workshop carrier
  (`src/sim/src/transport.cpp`: `entrance_points`, `search` with `GoalAction::Stop`;
  `docs/re/transport.md` §1.3, §2, §3);
- entrance points and entrance goals of a building are already produced for those carriers;
- the candidate set needs a city building query over a rectangle by PRODTYPE block and input goods,
  which the city's building lists can answer (ordered, deterministic).

What is new for the sim packet: the two predicates, the 24-tile rectangle and the nearest-first order
of the code-2 test, the rewrite to code 3 in `StatusTracker::report` (a predicate passed by the
caller, so the tracker stays pure), `Status::NoRoad = 3`, and the start-on-goal answer above (it decides whether code 1 can ever become code 3). Size: the
start-on-goal read about 1 h (docs; the ported `sim::search` may already answer it), the sim packet
3-4 h. The search cost per due report (every step
while due, ≥ 500 ticks apart) is bounded by the walk searcher's own limits.
