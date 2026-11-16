# In-game options and the main menu (original GOG 2.0.0.5 and History Edition)

**Status:** research and specification only (UI-PANELS-PREP, 2026-11-16). No engine code, no game
runs, no commits. Static reads of the GOG 32-bit DLLs (pefile + capstone, VAs), parsed ToolGfx and
`Texte.dat` files of both editions, sprite matches on existing GOG Wine shots and on frames taken
from the owner's HE recording `HE_UI_1.mp4`. Scratch work (dumps, disassembly listings, extracted
frames) is in `cache/prep-options-menu/` (git-ignored). No pixels or frames are in git; text is
cited by section and index, with at most a short label.

Owner rule (2026-11-16): the **History Edition is the picture reference**, the GOG DLLs give the
addresses, the game files of both editions are used. The original UI, bit-identical, is the base;
HE behaviour and rebuild changes come later as options. Where HE pictures and GOG code differ, this
note states it as a finding (section 3).

Related notes: [`ui.md`](ui.md) (HUD chrome, the gear tab's first capture, pause in 9.4, capture
index in 7), [`he-ui-scale.md`](he-ui-scale.md) (HE interface scale), [`tick-loop.md`](tick-loop.md)
§6 (speed keys), [`../formats/user-dat.md`](../formats/user-dat.md) (profile), 
[`../testing/original-wine.md`](../testing/original-wine.md) §2.2, §2.5, §6.

## Evidence letters

| Letter | Meaning |
|---|---|
| **D** | disassembled code, module + address (GOG VAs unless "HE"; HE are RVAs) |
| **P** | parsed original file (path, section/ACCESS/id) |
| **M-GOG** | measured on a GOG capture (file + coordinates; Wine runner, exact pixels) |
| **M-HE** | measured on an HE capture (file + coordinates; video frames are 0.75 × screen and JPEG/H.264, so ±2 screen px) |
| **A** | arithmetic, derived or inferred from the items it names |
| **U** | unknown: an open question (section 6) |

Capture locations (outside git):
- GOG in-game: `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\` (`r2_options`,
  `f15_options_tab`, `f15_options_y{360,410,460,510}`, `f15_pause`, `f16_main_menu`,
  `f16_options_graphics`, `f16_opt_click_*`), 1024 × 768 in-game, 800 × 600 menus at the top-left.
- GOG menus 2026-11-16: `E:\anno1503-worktrees\anno1503-UI-PAN-1\cache\orig\fisher-he-1\d96-20261116-081030\shots\`
  (`x_main_menu`, `x_singleplayer`, `x_choose_game`).
- HE: `E:\1503_test_videos\HE_UI_1.mp4` (analysis `E:\anno1503-worktrees\he-ui-captures\he_ui_1_video\HE_UI_1-analysis.md`,
  crops `c_tabs_a.jpg`, `c_tabs_b.jpg`, `crops\overlay_146.6.png`, `full4\f0046.jpg`); frames extracted
  for this note: `cache/prep-options-menu/he_{29.5,108.0,109.25,111.5,144.0}.png` (video time in s,
  1920 × 1080 video of a 2560 × 1440 screen; HE window client 2542 × 1351 at video (7, 24)).
- `E:\1503_test_videos\ui_5.mp4` (report `ui_5_work\analysis\ui_5-report.md`): main menu and
  single-player menu only; no options pages.

## 1. Screens and how they open and close

### 1.1 Main menu tree (outside a game)

The menus are full-screen 800 × 600 frames, `AnnoFrame.dll` classes `tFrameStartup*` (RTTI in
`analysis/pe_symbols.json`, P). Every frame's art is in `ToolGfx\Startup.tex` + `Startup.inc` (P).

| Screen | Class (RTTI) | Init function (D) | Background (Startup id GOG / HE) | Opened by | Closed by |
|---|---|---|---|---|---|
| Main menu | `tFrameStartupSplash` (A: the only frame with `STARTUP:SPLASH`) | 0x1002aab0 | `STARTUP` 20 / 20 | after the intros (two Bink videos, skipped with ESC or a click; `menus.json` `intros`, M-GOG) | Quit plank (program ends, A) |
| Single player | `tFrameStartupServerSpielTyp` (A: text `STARTUP:SERVERSPIELTYP`) | 0x10028060 | `SERVERSPIELTYP` 50 / 54 | main menu, Singleplayer (sub-frame 0, D 0x1002b25c) | Back plank (generic close 0x1001dfc0, D) |
| Choose game (New game / Campaign / Load game lists) | `tFrameStartupSzenarioAuswahl` | 0x1002e2c0 | `SZENARIOAUSWAHL` 61 / 65 | single player New game / Campaign / Load game (D 0x10028460 / 0x100284a0 / 0x100284e0 set list type 3 / 4 / 5 in global 0x1009baf0) | Back (0x1001dfc0), OK (0x1002ece0) |
| Mission text | `tFrameStartupAuftragsText` | ref 0x10016946 | `AUFTRAGSTEXT` 376 / 380 | Choose game, Mission plank (handler 0x1002ea40 runs sub-frame 0, D 0x1002eac1) | Back (`STARTUP:AUFTRAGSTEXT` 0) |
| Flag choice ("castle") | `tFrameStartupEinzelFarbauswahl` | ref 0x1001d981 | `FARBAUSWAHLEINZEL_BURG` 72 / 76 (+ `_HIMMEL`, `_FAHNEN`) | Choose game OK (M-HE: castle screen at 00:38.6 with "Zurück / Einstellungen" = `STARTUP:FARBWAHL_EINZEL` 0 / 1, P) | Back; picking a flag starts the game (`STARTUP:FARBWAHL_EINZEL` 2, P) |
| Endless customize | `tFrameStartupEndlosCustomize` | ref 0x1001ed06 | `STARTUP_ENDLOSCUSTOMIZE` 3 / 3 | flag choice, Customize (`FARBWAHL_EINZEL` 1, A) | OK / Start (`STARTUP:ENDLOSCUSTOMIZE` 6 / 7) |
| Profile | `tFrameStartupProfile` | 0x10026fd0 | `PROFILEVERWALTUNG` 383 / 387 | main menu, Profile plank (sub-frame 2, D 0x1002b1e4) | OK / back (0x1001dfc0) |
| Options (graphics) | `tFrameStartupOption` | 0x10025c90 (gadgets); background drawn in 0x10025ac0 | `OPTION_BK` 492 / 608 | main menu, Options plank (sub-frame 3, D 0x1002b377) | Back plank only; ESC does not leave (`menus.json` `options_graphics._note`, M-GOG) |
| Multiplayer | `tFrameStartupMultiPlayer` | (not traced) | `STARTUPMULTIPLAYER` 34 / 38 | main menu, Multiplayer (sub-frame 1, D 0x1002b2a7); shown only when `MULTIPLAYER` = 1 in GOG (section 2.1) | Back |
| Credits | Bink player (`tFrameStartupBink`, A) | handler 0x1002b2f0 | — | main menu, Credits: plays `Videos\1008.bik` (string 0x10095ed8, D) | end of video / ESC (U) |
| Continue game | — | handler 0x10028520 | — | single player, Continue game: loads `LastGame.sav` of the active profile (D, section 2.3) | — |

Sub-frame calls are the frame's vtable `+0xa8`(n); their result is checked by the caller
(single player: 0 = redraw the main menu, 1 = vtable `+0xb4` (leave the menu loop, A: start the
game), 2 = byte `+0x18c` set; D 0x1002b256..0x1002b29e).

### 1.2 In-game options (the gear tab)

| State | How opened | How closed | Evidence |
|---|---|---|---|
| Options main page (tab 0) | gear tab (992, 212) or key **O** | another main tab; ESC / second click: U (OQ-621 in `ui.md`) | `ui.md` §1 rows 8, §5.3 (M-GOG `r2_options`, `options_key_O`) |
| Load (tab 1) | left sub-tab at (762, 349) | another sub-tab | M-GOG `f15_options_y360`; M-HE `he_109.25.png` |
| Save (tab 2) | sub-tab (762, 395) | another sub-tab | M-GOG `f15_options_y410`; M-HE `c_tabs_b.jpg` panel 1 |
| Music (tab 3) | sub-tab (762, 441) | another sub-tab | M-GOG `f15_options_y460`; M-HE `he_111.5.png` |
| Speech (tab 4) | sub-tab (762, 487) | another sub-tab | M-GOG `f15_options_y510`; M-HE `c_tabs_b.jpg` panel 3 |
| HE "Spieleinstellungen" overlay | HE only: the gears button on the main page | X at the top right | M-HE `crops\overlay_146.6.png` (02:26.2–02:28.0) |
| Pause | key **P** | key **P** or the "Continue game" button | `ui.md` §9.4 (M-GOG `f15_pause`); manual p4 via `tick-loop.md` §6 |
| Game speed | keys F5 / F6 / F7 / F8 = 100 / 200 / 400 / 50 %; 400 % refused for a net client | — | D `AnnoFrame.dll` 0x100071b0 (`tick-loop.md` §6); key names from the manual p4 |
| Quick save | key **F10** writes `SaveGame\<profile>\Lastgame.sav` | — | M (Wine, `original-wine.md` §2.5); string `Lastgame.sav` AnnoFrame 0x10094c20, refs 0x10007b44, 0x1000cc11 (D) |

There is **no game-speed control and no graphics setting in the in-game options** of either
edition: the tooltips of the five sub-tabs are `TOOLTIP:OPTION` 0..4 = main menu, load, save,
music, speech (P), and no `OPTION_*` ACCESS name in `fenster.inc` is a speed control (P). Speed is
keys only (D). HE adds a sixth `TOOLTIP:OPTION` line and the gears button (section 3).

## 2. Elements per screen

Rectangles are (x, y) w × h in the frame's own pixels: 800 × 600 for menus, 1024 × 768 for the
in-game panel (panel x = W − 245, `ui.md` §5.1). Gadget classes: `0xb09` click button with a
2-frame graphic (idle, hover; the `+0x4c`(0x1aa, 1) attribute after the texture, A), `0xb1c` text
gadget, `0xb17` static graphic, `0xb00`/`0xb21`/`0xb22` list-type gadgets (A from use). In the
menu frames, the setter vtable `+0x50` is size(w, h) and `+0x5c` is position(x, y), `+0x8c` the
click handler (D: every row below).

### 2.1 Main menu (`AnnoFrame.dll` 0x1002aab0)

Planks are made in a loop over gadget ids 1, 3, 5, 7, 9, 11 (graphic) and 2, 4, …, 12 (text),
jump table 0x1002b188 (D). Graphic = `PLANKEN01` base + 2 · i (GOG ids 21..32, HE 25..36; 2 frames
each, P). Text = `STARTUP:SPLASH` line i (P), font attribute `+0x4c`(0x1c6, 2) (D).

| i | Plank | Graphic rect (D) | Text rect (D) | Startup id GOG / HE | Text | Click handler (D) | Action (D) |
|---|---|---|---|---|---|---|---|
| 0 | Profile | (202, 79) 420 × 75 | (200, 79) 420 × 75 | 21 / 25 | `STARTUP:SPLASH` 0 "Profile: %s", formatted with the active profile name (0x1002ad5a, global 0x1009c27c) | 0x1002b1d0 | sub-frame 2 (profile), then re-formats the name |
| 1 | Singleplayer | (238, 205) 312 × 52 | (258, 205) 292 × 52 | 23 / 27 | `SPLASH` 1 | 0x1002b240 | sub-frame 0 |
| 2 | Multiplayer | (271, 259) 275 × 51 | (271, 259) 275 × 51 | 25 / 29 | `SPLASH` 2 | 0x1002b2a0 | sub-frame 1 |
| 3 | Credits | (271, 312) 274 × 56 | (274, 312) 274 × 56 | 27 / 31 | `SPLASH` 3 | 0x1002b2f0 | Bink `Videos\1008.bik` |
| 4 | Options | (265, 370) 280 × 61 | (265, 367) 280 × 61 | 29 / 33 | `SPLASH` 4 | 0x1002b370 | sub-frame 3 |
| 5 | Quit | (301, 505) 216 × 61 | (301, 500) 216 × 61 | 31 / 35 | `SPLASH` 5 | 0x1002b390 | vtable `+0xb4` (leave, no confirm box, A) |

- **Multiplayer hide:** a static `0xb17` with `MP_HIDE` (GOG 33, HE 37; 275 × 51) at (266, 259)
  (D 0x1002af7b..0x1002b029). `MULTIPLAYER` is read from the frame's registry key at 0x10028860 and
  `value != 0` is written to a gadget attribute (D 0x1002887f..0x100288b1); which gadget: U (OQ-680).
  M-GOG `x_main_menu`: no Multiplayer plank, stone patch at y 259..310. M-HE `full4\f0046.jpg`:
  "Mehrspieler" shown (HE shows it without a registry change, A).
- **Version text:** text gadget id 13 at (80, 570) 800 × 30, colour attribute `0x1c5` = 0x414031,
  text = `HKLM\Software\Max-Design\ANNO1503\` `ADDON_VERSION`, else the literal "Unknown"
  (D 0x1002b076..0x1002b166). M-GOG `x_main_menu`: "Unknown" at about (80, 575). M-HE `he_29.5.png`:
  the HE build number at menu (80, 574) (A from the scale below).
- **Hover:** a plank shows its second frame under the cursor (M-HE: golden outline on Credits at
  00:27.25, `HE_UI_1-analysis.md` A1). The press frame is not separate (2 frames per plank, P).
- Centres agree with `tools/orig/menus.json` `main_menu` (fractions × 800 × 600; e.g. options
  0.662 · 600 = 397, rect centre 400; quit 531 vs 535) (A).

### 2.2 Single player (`AnnoFrame.dll` 0x10028060)

`PLANKEN_SPIELTYP` (GOG 51..60, HE 55..64), text `STARTUP:SERVERSPIELTYP` 0..4 (P).

| Plank | Graphic and text rect (D) | Handler (D) | Action (D) |
|---|---|---|---|
| New game | (240, 279) 318 × 58 | 0x10028460 | list type 3 → choose game; byte 0x1009c4e4 = 1 |
| Campaign | (266, 340) 282 × 52 | 0x100284a0 | list type 4 → choose game |
| Load game | (267, 396) 281 × 56 | 0x100284e0 | list type 5 → choose game |
| Continue game | (267, 455) 281 × 56 | 0x10028520 | copies `LastGame.sav` into the start name (global 0x1009c0a0), asks the file object 0x1009c618 `+0xc` for it under the profile path; missing → disables the plank (`+0x4c`(0x193, 0)); present → result 2 (start) |
| Back | graphic (89, 522) 211 × 63, text (89, 522) 211 × 60 | 0x1001dfc0 | close |

Centres agree with `menus.json` `singleplayer` (A). M-HE `d_menu_30_44.jpg`: same five planks
with the hanging "ANNO 1503" sign (the background art, A).

### 2.3 Choose game (`AnnoFrame.dll` 0x1002e2c0)

| Element | Rect (D) | Graphic | Text / handler (D) |
|---|---|---|---|
| Title | (241, 109) 310 × 24 | — | `STARTUP:SZ_AUSWAHL` 3 "Choose game" |
| List box (`0xb22`) | (248, 206) 303 × 232 | on the background | rows: scenarios / saves; headings `SZ_AUSWAHL` 4..9 |
| Scroll up / down (`0xb09`) | (597, 413) and (597, 446), 39 × 31 | `PFEILE_SZENARIOAUSWAHL` GOG 68..71 / HE 72..75 | 0x100148c0 / 0x100148e0 |
| Mission | graphic (289, 484) 230 × 63, text (288, 485) | `PLANKEN_SZENARIOAUSWAHL` 2nd pair | 0x1002ea40 → mission text frame |
| OK | (535, 508) 197 × 69 | 3rd pair (197 × 66 texture) | 0x1002ece0 |
| Back | graphic (58, 513) 209 × 61, text (58, 510) | 1st pair | 0x1001dfc0 |

- Rows are 24 px high (M-GOG `x_choose_game`: rows at y 241, 265, 289, …; `menus.json` rows
  0.562..0.722 step 0.04 · 600 = 24, A). Each row: name, 1–3 stars, a round red icon
  (M-GOG/M-HE); the icons are `SCENARIOB_SPIELDAUER` (9 × 21 × 21), `SCENARIOB_SCHWIERIGKEIT`
  (4 × 30 × 21), `SCENARIOB_SPIELERMIN` / `SPIELERMAX` (9 each, 22 × 21) (P); which icon is which
  column: U (OQ-685).
- GOG list (M-GOG `x_choose_game`, list type 3): heading `SZ_AUSWAHL` 4 "Tutorials" with three
  tutorials, heading 8 "Continuous games/Add-on" with Citizen, Baron, Viscount, Count, Marquess; the
  list scrolls. HE German list (M-HE `crops\menu_spiel_auswaehlen_37.5s.png`): Einführungsspiele (3),
  Endlosspiele (3: Prunkbaumeister, Großgrundbesitzer, Meisterstratege), AddOn Szenarien (1). The
  selected row is drawn in red (M-HE). Campaign list (type 4): one entry "Nova Fora" (M-GOG
  `original-wine.md` §6; M-HE 00:33). Load list (type 5): `*.sav` (string 0x10095f20, D ref U).
- Folder names are not in `AnnoFrame.dll` (P: no `0010SingleEndlos`-style string); list paths are
  formatted from `%s\Scenes\%s`, `%s\scenes\%s%s`, `SaveGame` (strings 0x10095424..0x10095454, P).
  Heading-to-folder rule: U (OQ-685).

### 2.4 Main-menu Options (`AnnoFrame.dll` 0x10025c90; graphics only)

| Element | Rect (D) | Graphic (GOG / HE Startup id) | Text | Handler / state (D) |
|---|---|---|---|---|
| Mode list (container) | (271, 66) 276 × 290 | — | — | 0x10026190 → 0x10025bd0: reads the selection (`+0x48`(0x1b3)) and stores it as `SCREENMODE` (0x10017620) |
| Mode i = 1..5 | text at x 274, y = 64, 125, 180, 237, 299 (i = 3, 4, 5 are special cases; i = 1, 2 use 61 · i + 3), 276 × 58 | `PLANKEN_OPTION` 493 + 2(i − 1) / 609 + 2(i − 1) | `STARTUP:OPTIONS` i − 1 ("800x600" … "1280x1024") | enabled only if the display supports w × h: display `+0x90`(w, h), mode table 800×600, 1024×768, 1152×864, 1280×960, 1280×1024 at 0x10025da8..0x10025de4 |
| Details | graphic (247, 402) 313 × 63, text (247, 398) | 503 / 619 | `STARTUP:OPTIONS` 5 or 6 | 0x10026180 → 0x10025c10: flips byte `+0x80`, shows line 5 when the new value is non-zero, else 6, stores it as `DETAILS` (0x10017640) |
| Back | graphic (301, 505) 216 × 61, text (301, 501) | 505 / 621 | `STARTUP:OPTIONS` 7 | 0x1001dfc0 |

- Init (0x10025b60): selection = `SCREENMODE` (0x10017590, values > 4 become 0), byte `+0x80` =
  (`DETAILS` == 0), then 0x10025c10 flips it once and writes it back (D). So the stored value and the
  line shown are tied by that double flip; which line means "details on" in play: U (OQ-681).
- M-GOG `f16_options_graphics`: five modes, tick on 1024 × 768; seven clicks did not move the tick
  (`f16_opt_click_0..6`, `ui.md` §5). Consistent with the support check disabling the other modes
  on the Wine runner's fixed 1024 × 768 desktop (A; OQ-681).
- Registry: `SCREENMODE` and `DETAILS` are `REG_DWORD` read with `RegQueryValueExA` and written with
  `RegSetValueExA` on the handle at 0x1009c50c (D 0x10017590..0x1001765d), the same handle that
  holds `MULTIPLAYER` (D 0x1002886c), which the Wine runner sets under
  `HKCU\Software\Max-Design\Anno1503` (M-GOG, `original-wine.md` §2.4). So the key is that one (A).

### 2.5 Profile (`AnnoFrame.dll` 0x10026fd0)

| Element | Rect (D) | Graphic (GOG Startup) | Text (`STARTUP:PROFILE`) | Handler (D) |
|---|---|---|---|---|
| New profile | (82, 177) 282 × 61 | `PLANKEN_PROFILEVERWALTUNG` 385/386 | 0 | 0x10027960 |
| Edit profile | (82, 240) 282 × 62 | 387/388 | 1 | 0x10027980 |
| Delete profile | graphic (82, 304), text (82, 300), 282 × 68 | 389/390 | 2 | 0x100279a0 |
| OK | graphic (116, 512), text (86, 507), 282 × 62 | 391/392 (217 × 62 texture) | 3 | 0x1001dfc0 |
| "Active Player: %s" | width 800, height 33, y 75 | — | 4 | — |
| Profile list | (406, 193) 225 × 156 | `PROFIL_EINGABEFELD` 384 (309 × 105) for the name entry (P) | — | — |
| List arrows | (664, 246) and (664, 279), 39 × 31 | `PROFIL_PFEILE` 393..396 | — | 0x100148c0 / 0x100148e0 |

The active profile is `AKTPLAYER` (string, written at 0x10027468 with `RegSetValueExA`, D), the
profile folder `%s\SaveGame\%s` (0x10095c54, D; `user-dat.md`).

### 2.6 In-game options panel (`View.dll`)

Positions of the `View.dll` gadgets come from a layout table indexed by gadget number (vtable
`+0x24`(gadget, index), D 0x10079cf7), not from immediates, so all rectangles here are **measured**
on GOG shots (exact pixels) and checked on HE frames. All graphics are `ToolGfx\fenster.tex`
(P; HE ids in section 3.3).

**Frame (all five pages)**

| Element | Rect | GOG id | Evidence |
|---|---|---|---|
| Banner `PLANKEGROSS` with the page name | (783, 252) 241 × 48 | 1019 | `data/ui/hud.txt` (M-GOG `r2_construction_public`); text = `OPTIONSUBS_TEXTE` page index (`View.dll` 0x100784f4, D) |
| Sub-tab k = 0..4 | (762, 303 + 46k) 42 × 46 | `OPTION_TABS` idle 1476 + 2k, selected 1477 + 2k | M-GOG `r2_options` (tab 0 = 1477, others 1478/1480/1482/1484, `hud.txt`); `f15_options_y360` tab 1 = 1479 (mean 7.2/ch); `y410` tab 2 = 1481 (12.9) |
| Sub-tab tooltips | — | — | `TOOLTIP:OPTION` 0..4 (D 0x1007829e); handler 0x100786a0 (D 0x10078280) |

**Page 0, main ("Main menu"; HE "Optionen")** — `View.dll` 0x10079c90 builds three `0xb09`
buttons and one `0xb05` toggle, tooltips `TOOLTIP:OPTION_MAIN_ADDON` (D 0x10079cac).

| Element | Rect (M-GOG `r2_options`, `hud.txt`) | GOG id | Hit size (D) | Tooltip | Handler (D) | Action (D) |
|---|---|---|---|---|---|---|
| Background | (779, 301) 245 × 467 | `OPTION_BACKGROUND` 1475 | — | — | — | — |
| Exit (door) | (927, 660) 92 × 100 | `OPTION_EXIT` 1492/1493 | 92 × 109 | `OPTION_MAIN_ADDON` 0 | 0x1007a370 | frame `+0x58`(0x10) (what 0x10 does: U, OQ-683) |
| Rating (laurel) | (934, 533) 69 × 61 | `OPTION_BILANZ` 1486/1487 | 69 × 62 | 1 | 0x1007a310 | frame attribute `+0x4c`(0x28, 0) (opens the rating screen, A: same screen as key Q, `ui.md` §9.5) |
| Player statistics (chart) | (793, 348) 94 × 82 | `ADDON_SPIELERSTATISTIK` 1580/1581 | 94 × 82 | 2 | 0x1007a340 | frame attribute `+0x4c`(0x29, 0) (statistics screen; ui_5: the game pauses while it is open, M-HE `ui_5-report.md` §2) |
| Active help (toggle `0xb05`) | (936, 455) 64 × 55 | `OPTION_HELPPLAYER` 1488/1489 (2 states) | — | 3 / 4 (on / off) | 0x1007a300 → 0x1007a210 | toggles; persisted as profile `HELPPLAYER` (A, OQ-689) |

M-HE `he_108.0.png` (gear tab, page 0), GOG sprites scaled 0.75, positions relative to the matched
background (video (1729, 250) = UI (779, 301)): statistics UI (794, 348) mean 14.6/ch; rating
(935, 533) 14.4; exit (928, 660) 14.6; active help (936, 454) with the **second** frame (HE 1495,
26.5/ch vs 36.1 for the first): the HE footage shows active help off (A). So HE keeps the GOG
positions; it adds the gears (section 3.2).

**Page 1, load** — M-GOG `f15_options_y360`; M-HE `he_109.25.png`, `c_tabs_a.jpg` panel 4.

| Element | Rect (M-GOG) | GOG id | Notes |
|---|---|---|---|
| Background with the list frame | (779, 301) 245 × 467 | `OPTION_BK_SAVELIST` 1499 | mean 7.8/ch |
| List scroll up / down | (868, 303) 87 × 20; (871, 494) 84 × 25 | `OPTION_SCROLLER` 1504, 1506 (+1 = hover/pressed, A) | means 3.7 / 3.3 |
| Load button | about (916, 605) 94 × 81 | `OPTION_LADEN` 1490/1491 | mean 20 (weak: greyed state with no row selected, M-HE "large greyed load picture") |
| Rows | inside the frame; one row "Lastgame" in both editions | — | M-HE / M-GOG; tooltip `TOOLTIP:OPTION_LOAD` 0 |

The list is the profile's `*.sav` files (`View.dll` 0x1007c617 `"%s\%s\%s\%s"` with `SaveGame`,
`FindFirstFileA`, D), each row named by the save's `ALIASNAME` chunk, read under `ANNO` /
`ANNOADDON1` → `WELT` → `ALIASNAME` (0x1007c44d..0x1007c4c8, D). `CompareFileTime` is imported
and used near the list code (0x1007c54a, D); the order rule: U (OQ-682).

**Page 2, save** — M-GOG `f15_options_y410`; M-HE `c_tabs_b.jpg` panel 1.

| Element | Rect (M-GOG) | GOG id | Notes |
|---|---|---|---|
| Background, list, scrollers | as page 1 | 1499, 1504, 1506 | — |
| Name entry plank | (785, 553) 239 × 52 | `OPTION_EINGABEPLANKE` 1508 | mean 2.25/ch; ref 0x1007cff7 (D) |
| Save button | about (916, 608) 94 × 83 | `OPTION_SPEICHERN` 1500/1501 | mean 19 (state U); ref 0x1007ced3 |
| Delete button | not located (56 × 69) | `OPTION_LOESCHEN` 1502/1503 | ref 0x1007ce33; M-HE: two icons below the plank |
| Tooltips | — | — | `TOOLTIP:OPTION_SAVE` 0 "Save game", 1 "Delete saved game" |

A new save gets a **GUID file name**: `CoCreateGuid` (import 0x1009a1c4, called 0x1007c209) then
`"%x-%x-%x-%x%x%x%x%x%x%x%x.sav"` (0x1007c286, D); the typed name goes into the save as
`ALIASNAME` (A from the list reader). `MESSAGEBOX` 17 "Overwriting of savegames in Singleplayer
only!" exists (P); when it shows: U (OQ-682).

**Page 3, music** — M-GOG `f15_options_y460`; M-HE `he_111.5.png`, `c_tabs_b.jpg` panel 2.

| Element | Rect (M-GOG) | GOG id | Notes |
|---|---|---|---|
| Background | (779, 301) 245 × 467 | `OPTION_BACKGROUND_LIST` 1498 | — |
| Song rows with ticks | first tick (994, 349) 19 × 17 | `OPTION_HACKERL` 1497 (1496 = 14 × 11 other state) | 18 rows = `Music\normal\*` (18 files, both editions, P; `Data\music.dat` `GRUPPE_DEFAULT` `normal\*`); profile `SONGS` 18 × (id, enabled) (P) |
| List scrollers | (868, 303), (871, 494) | 1504, 1506 | — |
| Volume slider track | (825, 585) 159 × 32 | `OPTION_SLIDER` 1474 | mean 7.0 |
| Slider plus cap | (984, 582) 24 × 35 | 1472 (1473 = other frame); minus cap 1470/1471 (A from HE `FensterAddOn.dat` names `LautstaerkenreglerMinus/Plus`) | knob `SLIDER_KNUPPEL` (0x1007b4bb, D) |
| Play-song icon | (954, 635) 51 × 55 | `OPTION_MUSIKICONS` 1494/1495 | tooltip `TOOLTIP:OPTION_MUSIC` 0 |

Volume: slider handler 0x1007b9c0 (D 0x1007b555) reads the slider value (`+0x48`(0x1a0)) on
release and calls 0x1007aa30(v): music object `+0x64`(10 · v), then `+0x88`(10 · v, −2500)
(D 0x1007aa52..0x1007aa77). With the profile `VOLUME` range 0 .. −250 (`user-dat.md`, P + experiment),
the value sent is 0 .. −2500, i.e. DirectSound hundredths of a dB down to −25 dB (A). Which
`VOLUME` i32 is music: U (OQ-688).

**Page 4, speech** — M-GOG `f15_options_y510`; M-HE `c_tabs_b.jpg` panel 3.

| Element | Rect (M-GOG) | GOG id | Notes |
|---|---|---|---|
| Category rows with ticks | a tick at (989, 601) | `OPTION_HACKERL` 1497 | 11 rows = `SPEECHGROUP` 0..10 (P; refs 0x1007dd39, 0x1007df17); profile `SPEECH` bit mask (default 0xffffffff, P) |
| Volume slider | track (825, 655) 159 × 32, plus cap (984, 652) | 1474, 1472 | ref 0x1007e313 (D); handler not traced |

### 2.7 Pause

Key **P** (manual p4, D key table 0x100071b0 per `tick-loop.md`). A parchment `MESSAGEBOX_BK`
(GOG fenster 1512, HE 1518; 579 × 386) at (222, 191) and one button `MESSAGEBOX_BUTTONS` 1513
(244 × 67) at (286, 500) (`ui.md` §9.4, M-GOG `f15_pause`). The HUD chrome stays live. Texts:
`MESSAGEBOX` 24 "Pause!" and 16 "Continue game" exist (P); that the box uses them: A (OQ-684).
In multiplayer `MESSAGEBOX` 6 "Game is paused, please wait." (P).

### 2.8 Where settings live

| Setting | Edition | Store | Evidence |
|---|---|---|---|
| Display mode index 0..4 | GOG | `HKCU\Software\Max-Design\Anno1503` `SCREENMODE` DWORD | D 0x10017590 / 0x10017620; key A (2.4) |
| Details | GOG | same key, `DETAILS` DWORD | D 0x100175e0 / 0x10017640 |
| Multiplayer menu | GOG | same key, `MULTIPLAYER` DWORD | D 0x10028860; M-GOG Wine |
| Active profile | both | same key, `AKTPLAYER` string | D 0x100263f3 et al.; P `user.reg` |
| Version text | GOG | `HKLM\Software\Max-Design\ANNO1503\` `ADDON_VERSION` | D 0x1002b0d4 |
| Music playlist, music + speech volume, speech categories, active help, profile GUID | both | `SaveGame\<profile>\user.dat` chunks `SONGS`, `VOLUME`, `SPEECH`, `HELPPLAYER`, `USGUID` | P (`user-dat.md`); writer `AnnoScene.dll` refs 0x10001c0c..0x10002022 (D) |
| Saves | both | `SaveGame\<profile>\<GUID>.sav`, `Lastgame.sav` (F10, Continue), `Lastgame-ServerDown.sav` (MP) | D (2.6, 2.2); M-GOG Wine |
| HE window, resolution, vsync, cursor, UI scale, wheel, edge scroll, music + sound volume | HE | `HKCU\Software\Ubisoft\Anno1503HistoryEdition` `SCREEN_WIDTH`, `SCREEN_HEIGHT`, `SCREEN_TOP`, `SCREEN_LEFT`, `SCREEN_WINDOWED`, `SCREEN_WINDOW_BORDER`, `CURSORSET`, `UI_SCALE`, `INVERT_MOUSE_WHEEL`, `ENABLE_MOUSE_SCROLLING`, `ENABLE_VSYNC`, `MUSIC_VOLUME`, `SOUND_VOLUME` | P: strings HE `Anno1503.exe` file offsets 0x4940..0x4a80 and `AnnoDisplay.dll` 0xf5ef08..0xf5f020; `he-ui-scale.md` §1 (UI_SCALE also persists elsewhere, OQ there) |

## 3. HE vs GOG

### 3.1 Main menu and menu frames

1. **Fit:** HE draws the original 800 × 600 menu **uniformly scaled to the client height and
   centred**. M-HE `he_29.5.png` (client 2542 × 1351 = video 1906 × 1013 at (7, 24)): the GOG
   `STARTUP` background (id 20, pixel-identical in HE, P) matches best at scale 1.69 video px per
   menu px (mean 10.3/ch; 1.68: 18.0, 1.70: 19.5) at video (284, 24); 1.69 / 0.75 = 2.253 screen px
   ≈ 1351 / 600 = 2.2517 (A). The Singleplayer plank (id 23) at the same scale lands at menu
   (237.9, 204.7) vs code (238, 205). So the menu layout of section 2 holds in HE unchanged (A).
2. **Side art:** HE `Startup.tex` adds `RESOLUTION_BORDER_LEFT` 401 × 600, `_TOP` 788 × 386,
   `_RIGHT` 400 × 600, `_BOTTOM` 788 × 386 (HE ids 21..24, P; `Data\StartupAddon.dat` source
   list). M-HE: pillars and lion rings continue beyond the 800-px art left and right. Placement rule
   at other aspect ratios: U (OQ-687).
3. **Ids:** every HE `Startup.tex` id from `PLANKEN01` on is GOG + 4 (P: `anno-dump inc` both),
   pixels identical except `PROGRESSBAR` frame (GOG 464 × 75, HE 448 × 56) and the language
   variants `ENTERTAINMENT_ANNOPLANKE_{Ger,Fra,Ita,Nl,Spa,Eng,EngUS,Jap}` (HE 480..607; GOG has one
   set, 476..491) (P, pixel hashes).
4. **Multiplayer** is visible in HE without the GOG registry switch (M-HE); HE ships MP scenarios
   (`he-vs-gog-comparison.md` §3.5).
5. **Version line** shows the HE build number instead of "Unknown" (M-HE vs M-GOG).
6. **Main-menu Options in HE:** not in any HE capture. `he-ui-scale.md` §Open says the scale is set
   with "the main-menu slider", so HE's main-menu Options probably opens HE settings, not the
   resolution list of 2.4 (A). **Finding to confirm (OQ-680).**

### 3.2 In-game options

1. **Gears button (HE only):** `OPTION_GAMESETTINGS` (HE fenster 1492/1493, 62 × 51; source
   `Optionen\Optionen01_01.BMP` in HE `Data\FensterAddOn.dat`, P). M-HE `he_108.0.png`: idle frame
   at UI ≈ (938, 372) (video (1848, 303), mean 14.1/ch). Tooltip "Optionen" (M-HE 02:26.15) =
   HE `TOOLTIP:OPTION_MAIN_ADDON` 2, a line **inserted** in HE (P: GOG lines Exit, Rating, Player
   statistics, Active help ON, OFF; HE Exit, Rating, Options, Player statistics, Active help ON, OFF).
   So HE's code reads statistics and active-help tooltips one index later than GOG (A).
2. **Overlay:** the gears open "Spieleinstellungen" (M-HE `overlay_146.6.png`): section
   Grafikeinstellungen (Anzeigemodus, Monitor (greyed), Auflösung, V-Synch, Cursor,
   Interface-Skalierung) and Steuerung (Mausrad umkehren, Scrollen mit Maus aktivieren), a greyed
   "Bestätigen" button, X to close, a keyboard icon. Modern art, not ToolGfx (A); its code is the HE
   online overlay in `AnnoDisplay.dll` (`onlineoverlay::Anno2GameSettings`, `CSettings*` classes,
   `SoundSettingsList`, P strings 0xf5ede8..0xf76300). Whether there is a sound page: U (OQ-686).
3. **Texts:** `OPTIONSUBS_TEXTE` 0 is "Main menu" in GOG and "Options menu" / "Optionen" in HE;
   HE adds line 5 "Options" there and in `TOOLTIP:OPTION` (P, English and German HE files). Only
   five sub-tabs are visible in HE (M-HE), so the purpose of the sixth line: U (OQ-689).
4. **Ids:** HE `fenster.tex` = GOG + 4 for `OPTION_SLIDER` .. `OPTION_BILANZ` (1470..1487 →
   1474..1491) and + 6 from `OPTION_HELPPLAYER` on (1488.. → 1494..); pixels identical (P, hashes).
   The rebuild's layout data must name ACCESS + index, not a raw id, or carry both (A).
5. **Layout:** apart from the gears, the HE page-0 icons, tabs and the four sub-pages sit where GOG
   has them (M-HE vs M-GOG, 2.6). HE renders at UI scale 1.0 in that footage.
6. **Song names:** HE shows "Capturetrip" for `Music\normal\Capture-trip.WAV` (M-HE); the GOG
   display of that row is not captured (OQ-688).

### 3.3 Resolution handling

GOG: five fixed display modes, chosen in the main-menu Options (2.4), in-game UI 1:1 (`ui.md` §5).
HE: any window or fullscreen size from the HE overlay (`SCREEN_*`), in-game UI scaled by
`UI_SCALE` (`he-ui-scale.md`), menus letterboxed (3.1). The rebuild's base is the GOG behaviour;
HE's fit and settings are options (owner rule).

## 4. What the sim must provide (and the app, for saving, loading and audio)

State each screen reads. "missing" = nothing in `src/` today (main 0f46b1b2, read 2026-11-16).

| Screen reads | Needed value | Current name | Status |
|---|---|---|---|
| Speed (keys, no UI) | current speed percent, single vs multiplayer | `sim::WorldSettings::speed_percent` (`settings.hpp:50`); `sim::is_original_speed`, `single_player_step_ms`, `multiplayer_step_ms` (`clock.hpp:81..99`); `app::PlayRunner(world, speed_percent)` (`play.hpp:298`) | exists; app sets it only from `--speed 50\|100\|200` (`cli.cpp:123`), 400 % refused there |
| Pause | paused flag | `app::PlayRunner::set_paused` (`play.hpp:306`), `Viewer::toggle_pause` on **Space** (`viewer.cpp:1358`) | exists (app-level, no sim command in single player, matches `clock.hpp:75..78`); key P, parchment and button missing |
| Load list rows | per save: display name, file, time | — | missing (saves have no display name: the `World::save()` frame, `world.hpp:282`, `state.md` §9.5, holds no alias) |
| Save page | the typed name; whether overwrite is allowed | — | missing |
| Continue game | newest `Lastgame` save of the active profile exists | — | missing |
| Music page | 18 songs (name, id, ticked), current song, volume | — | missing (no audio code in `src/`) |
| Speech page | 11 category bits, volume | — | missing |
| Active help toggle | on/off | — | missing |
| Rating / player statistics buttons | the screens they open | — | missing (other packets; statistics screen pauses the game, M-HE ui_5) |
| Main menu | active profile name, version text, multiplayer shown | — | missing |
| Profile screen | profile list, active profile | `tools/orig/userdat.py` reads `user.dat` (tool only) | missing in the engine |
| Main-menu Options | display mode index, details flag, supported modes | window size: `app` `ui_transform.hpp` / `ui_layout.hpp` (`ui.md` §5.1) | settings store missing |
| Choose game | scenario list with headings, stars/icons, mission text | scenario loading: `sim::ScenarioSpec` (`scenario.hpp`), `sim_load::load_ship_start` (`opening.cpp`), `GameSetup` (`game_setup.hpp`) | list, headings, icons, mission frame missing |
| Flag choice / customize | seat colour; computer players, pirates, disasters on/off | `sim::Seat::colour`, `sim::PirateLevel` (`game_setup.hpp:17..36`) | colour exists; computer players on/off and disasters on/off missing (`STARTUP:ENDLOSCUSTOMIZE` 0..5) |

Commands and actions the screens send:

| Action | Existing | Must carry (if missing) |
|---|---|---|
| Set speed (F5 / F6 / F7 / F8) | `sim::SetSpeedCmd{percent}` (`command.hpp:88`, wire 0x7a) | app key mapping missing; **conflict:** the rebuild's opening session uses **F5 for quick-save** (`docs/app/ship-opening.md` §4), the original F5 = 100 % and F10 = quick-save (owner decision) |
| Pause / resume | `PlayRunner::set_paused(bool)` | P key + pause box (app); MP: host stops turns (`clock.hpp:77`) |
| Quick save (F10) | `OpeningSaver::request()` (`opening_session.hpp`), writes `quicksave-seat<N>.sav` | app: profile folder, name `Lastgame`, F10 |
| Save with a name | `World::save()` + `OpeningSaver::write_file` | app `SaveGame(profile, display name)` → file with a unique name (original: GUID) and the name inside; refuse in MP (MESSAGEBOX 17, A) |
| Delete save | — | app `DeleteSave(file)` |
| Load save (in game or menu) | `sim::World::load(bytes, types)` (`world.hpp:273`); `--resume` (`cli.cpp:90`) for the opening | app `LoadSave(file)` that replaces the running world and restarts the runner |
| Continue game | — | app: load the profile's `Lastgame` save |
| Exit to menu / quit | window close only | app `LeaveGame(confirm)` → main menu; `Quit` |
| Music: toggle song, play song, volume | — | app audio: `SetSongEnabled(id, bool)`, `PlaySong(id)`, `SetMusicVolume(v in 0..−250)`; persisted to the profile |
| Speech: toggle category, volume | — | app: `SetSpeechCategory(bit, bool)`, `SetSpeechVolume(v)`; the sim side needs **message events** to voice (sim `HostEventQueue`, `world.hpp:226`, carries UI events; speech categories map onto them: U) |
| Active help on/off | — | app/profile flag |
| Display mode, details | — | app settings (`SCREENMODE`, `DETAILS` equivalents) |
| Start game from choose game | `sim_load` loaders, `GameSetup` | app: `StartScenario(path, seat colour, customize flags)` |

No new **sim** command is needed for options or the main menu except what speed already has:
everything else is app state (profile, saves, audio, settings) or game setup before the sim exists
(A).

## 5. Evidence status per item

| Item | Status |
|---|---|
| Main-menu planks: rects, ids, texts, handlers | **D** (0x1002aab0); centres **M-GOG**; layout **M-HE** at scale 1.69 |
| MP hide, version text | **D**; **M-GOG** / **M-HE**; MP gadget binding **U** (OQ-680) |
| Single-player planks | **D** (0x10028060); **M-GOG** centres |
| Choose-game layout | **D** (0x1002e2c0); rows **M-GOG**; heading/folder rule **U** (OQ-685) |
| Main-menu Options (GOG) | **D** (0x10025c90); **M-GOG** `f16_options_graphics`; details meaning **U** (OQ-681) |
| Profile screen | **D** (0x10026fd0); not captured (GOG or HE) |
| Credits | **D** (Bink 1008); not captured |
| Mission text, flag choice, customize | class + texts **P**/**D** refs; HE castle screen **M-HE**; layout not read |
| In-game page 0 | rects **M-GOG** + **M-HE**; handlers **D** (0x10079c90); exit action **U** (OQ-683) |
| Load / save pages | rects **M-GOG**; file naming and list reader **D**; order, overwrite **U** (OQ-682) |
| Music / speech pages | rects **M-GOG**; volume path **D**; VOLUME order **U** (OQ-688) |
| Pause box | **M-GOG** (`ui.md` 9.4); text binding **A** (OQ-684) |
| Speed keys | **D** (`tick-loop.md` §6) + manual |
| Settings stores | **D** / **P**; HE registry **P** |
| HE gears + overlay | **M-HE**, **P**; overlay content beyond one frame **U** (OQ-686) |
| HE main-menu Options | **U** (OQ-680) |
| HE menu side art placement | **P** textures; rule **U** (OQ-687) |

## 6. Open questions

- **OQ-680** HE main-menu "Optionen": does it open the original resolution list (2.4) or the HE
  settings overlay, and where is the interface-scale slider `he-ui-scale.md` mentions? Also which
  gadget the GOG `MULTIPLAYER` value shows/hides (0x10028860) and whether HE bypasses the check.
  Needs: HE recording R1 (section "Recordings" in the packet); GOG: trace 0x10028860's caller.
- **OQ-681** GOG main-menu Options: what `DETAILS` changes in play, and which stored value means
  "on" (the init flips once, 0x10025b60 → 0x10025c10). Are modes other than 1024 × 768 disabled on
  the Wine desktop by the support check (display `+0x90`)? Needs: trace `DETAILS` reader
  AnnoFrame 0x1000cc9f; a Windows-harness check (owner's call).
- **OQ-682** In-game load/save: list order (`CompareFileTime` at 0x1007c54a), the default name in
  the plank, the name length limit, when `MESSAGEBOX` 17 shows, whether delete asks first, whether
  saving over a selected row reuses its GUID file.
- **OQ-683** Exit door: what frame call `+0x58`(0x10) does (confirm box `MESSAGEBOX` 21–23?), whether
  it returns to the main menu or ends the program, and whether it writes `Lastgame.sav`. (Overlaps
  `ui.md` OQ-549.)
- **OQ-684** Pause box texts (`MESSAGEBOX` 24 / 16 assumed), whether the gear tab or any options
  page pauses the game (the statistics screen does, M-HE ui_5), and whether P works while a menu
  page is open.
- **OQ-685** Choose game: which `Scenes` folder feeds which heading (`SZ_AUSWAHL` 4..9), row order,
  the meaning and column of the `SCENARIOB_*` icons (the round red icon), and why GOG shows five
  endless rows (Citizen..Marquess) under heading 8 "Continuous games/Add-on" while
  `0011SingleEndlosAddon` holds three `.sze` (P, `game/app/Scenes`).
- **OQ-686** HE overlay: full item list (is there a sound page, `SoundSettingsList` P), whether
  `MUSIC_VOLUME` / `SOUND_VOLUME` in the HE registry replace or mirror the profile `VOLUME`, what
  "Bestätigen" applies, and where the values persist (`he-ui-scale.md` §1 found the registry is not
  the whole story).
- **OQ-687** HE menu fit: exact scale and filter (measured 2.25 screen px per menu px at a
  1351-px client, A), placement of `RESOLUTION_BORDER_*` at other aspect ratios, and whether a wide
  but short client is ever width-limited.
- **OQ-688** Music/speech: which `VOLUME` i32 is music (the music slider path is 0x1007b9c0 →
  0x1007aa30; the speech path at 0x1007e313 is not traced), whether a slider move writes `user.dat`
  at once (observed for both, `user-dat.md`), and how song names are made from file names
  ("Capture-trip" shown as "Capturetrip" in HE).
- **OQ-689** HE's sixth `TOOLTIP:OPTION` / `OPTIONSUBS_TEXTE` line "Options": a hidden sixth page or
  the gears page's banner? Also confirm that profile `HELPPLAYER` is the active-help toggle (name
  match only, A).
