# In-game UI: ship panels and diplomacy (UI-PANELS-PREP, ships and diplomacy area)

2026-11-16. Research and specification only (no code, no game run; documentation on `task/UI-PANELS-PREP`). Read-only against main
`0f46b1b2`. Visual reference: **History Edition** (owner rule 2026-11-16); code addresses: **GOG 2.0.0.5**
DLLs; game files of both editions. Nothing original is stored here beyond ACCESS names, ids, section
names and short labels; captures and scratch scripts stay in `cache/prep-ships-diplomacy/` (git-ignored).
Paths under `cache/`, other worktrees' caches and `E:\1503_test_videos` are local evidence on the owner's PC and
are not in git; every D and P claim is reproducible from the cited address or file, the M claims from the named capture.

## Evidence letters

| Letter | Meaning |
|---|---|
| **D** | GOG instructions read (module + address). `View.dll` unless another module is named. |
| **P** | Parsed original file: `ToolGfx/fenster.inc/.tex` (GOG and HE), `Texte.dat` (GOG; HE `Languages/Eng` and `Ger`). |
| **C** | The game's own `Manual.pdf` (GOG `game/app`), pages named; paraphrased, never quoted at length. |
| **M-HE** | Measured on the owner's HE video `E:\1503_test_videos\HE_UI_1.mp4` (1920 × 1080 recording of a 2542 × 1351 client, UI scale 1.0, video px = 0.75 screen px). Frames extracted at the named video second. Method below. |
| **M-GOG** | Measured on a GOG Wine-runner shot (`E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\`), 1024 × 768, exact scale. |
| **A** | Arithmetic from other rows (named). |
| **S** | Structure read with **D**, meaning inferred (from the tooltip line, the icon or the manual); not a decoded rule. |
| **T** | The rebuild's own code at `0f46b1b2`. |
| **U** | Unknown; an open question names it. |

**Coordinates.** All positions are in the **1024 × 768 reference layer** (`data/ui/hud.txt`: right chrome at
x = 779, top-anchored; HE moves the chrome to x = W − 245, `docs/re/he-ui-scale.md`). A gadget **size** from
code is `push h; push w; call [gadget+0x50]` (the convention `docs/re/ui.md` §2.2/§2.4 already uses).

**M-HE method** (`cache/prep-ships-diplomacy/scripts/hematch.py`): decode the HE `fenster.tex` sprite (HE ids),
scale it by 0.75 (bilinear), masked least-squares match in the frame's right panel; anchor = the construction
tab `MAINTAB` (HE id 13) whose 1024 position is (779, 181) (`hud.txt`): `x_ref = 779 + (x_video − x_tab)/0.75`,
`y_ref = 181 + (y_video − y_tab)/0.75`. The anchor matched at video (1729..1730, 160) on every frame used.
Precision about **±2 px** (video compression and scaling); RMS per channel is given where it decides a
match (≤ 25 good, 25-35 usable, > 35 rejected). **M-GOG method** (`scripts/gogmatch.py`): same masked match at
scale 1 on the GOG shot, with the fraction of opaque pixels equal after RGB565 reduction.

## 0. What already holds (do not redo)

- `docs/re/ui.md` §1 rows 9/15/16 and §9.3: key **D** / bust tab = player list (`r2_player_menu`,
  `f15_player_menu`), key **S** = ship list (`f15_ship_list`), key **C** = city list; the treaty cycle and a
  ship panel were **not reached** in the GOG runner (Citizen start has no discovered partner; four map clicks
  missed the ship). §5.3: tab switching; the list rows and the diplomacy cluster are not drawn (UI-TABS-1).
- `data/ui/hud.txt`: panel `player_list` chrome is measured on GOG (sub-tabs, scroll arrows, crest).
- `docs/re/ship-cargo.md` §2/§11: the load/unload click chain (`0x1005a7f0`, `0x1005a8d0`), the amount
  buttons `[view+0x640]` = 1/5/10/50, throw overboard `0x10062840`, the record and the MP type `0x6a`.
- `docs/re/ship-trade-routes.md`: the route node fields and the world-side route execution.
- `docs/re/game-mechanics.md` §1 (manual p4-5): keys **S** ship list, **D** player menu, **C** city list,
  **W** fly flag, **CTRL+W** throw cargo overboard (C).

## 1. Screens, states, how they open and close

The right panel (245 × 524 under the minimap, `hud.txt` `mainuntergrund`) hosts two **views** for this area:
the **ship view** (a selected ship) and the **player view** (the bust tab). Each view has a header (plank +
name + emblem) and a **sub-tab column** left of the panel at x = 762 that reaches over the map (`hud.txt`
`player_subtab_*`; `docs/re/ui.md` §5.3).

| # | Screen | Opens by | Evidence | Closes by |
|---|---|---|---|---|
| S0 | Ship view, header + 8 sub-tabs | selecting an own ship: drag-select rectangle (HE 00:46.0), click (U) | M-HE (`HE_UI_1-analysis.md` 00:46.2); D builder `0x10056e90..0x10057384` | clicking empty sea (HE 00:50.0: the "?" tab with only `MAX_PIC` remains), another selection, a main tab (U) |
| S1 | Ship status (tab 0) | default tab on selection (HE 00:46.2, 01:02.7) | M-HE; D `0x100631b1..0x10063af8` | another sub-tab |
| S2 | Ship behaviour (tab 1) | tab 1 | M-HE 00:58.6; D `0x100603b1..0x10060878` | another sub-tab |
| S3 | Ship repair (tab 2) | tab 2; **greyed** away from a shipyard (HE) | M-HE greyed icon; C p17 (repair and cannons at a shipyard) | — |
| S4 | Load / unload (tab 3) | tab 3; **greyed** without an own warehouse in range (HE before the Kontor) | M-HE greyed; D `0x10059d00..0x1005a5c0`; `ship-cargo.md` | — |
| S5 | Sell ship (tab 4) | tab 4 | M-HE 00:59.6; D `0x10064451..0x10064cb8` | — |
| S6 | Ship list (tab 5) | tab 5 | M-HE 00:59.9/01:01.2; D `0x10061f21..0x100622d8` | — |
| S7 | Ships for purchase (tab 6) | tab 6 (empty list in HE 01:01.0) | M-HE; D `0x100610d1..0x10061738` | — |
| S8 | Automatic trade route (tab 7) | tab 7 | M-HE 01:01.6; D `0x100266d8..`, `0x10027f01..`, `0x1002a1b1..` | — |
| P0 | Player view, header + 5 sub-tabs | bust main tab (`hud.txt` `tab_military`), key **D** (to P2), key **S** (to P4) | M-GOG `r2_player_menu`, `f15_ship_list`; C p4-5; D `0x1007f200..0x1007f560` | another main tab |
| P1 | Player info (tab 0, balance rows) | start state of the bust tab (`hud_idle_running`) | M-GOG; M-HE 00:44.5 | — |
| P2 | Diplomacy status (tab 1): partner list + action tabs | key **D**, the bust tab from P1 (`ui.md` §5.3) | M-GOG `r2_player_menu`; M-HE 02:19-02:20 | — |
| P2a | Treaty status (action tab 0): three treaty buttons | default action tab | M-GOG; M-HE 140.2 s; D `0x100846dc..0x1008497a` | — |
| P2b | Demand tribute (action tab 1) | action tab 1 (greyed in both captures) | D `0x100819a1..`; C p20 | — |
| P2c | Pay tribute (action tab 2) | action tab 2 (greyed) | D `0x100839d1..`; C p20 | — |
| P2d | Send message (action tab 3) | action tab 3 (greyed) | D `0x10082e51..0x10083130` | — |
| P3 | City list (tab 2) | key **C** | M-GOG `f15_city_list`; out of this area | — |
| P4 | Ship list (tab 3) | key **S** | M-GOG `f15_ship_list` (tab 3 selected); M-HE 01:44.5, 02:18 | — |
| P5 | Ships for purchase (tab 4) | tab 4 (empty in HE 02:22) | M-HE | — |
| X1 | Foreign warehouse trade (`FREMDERKONTOR_UNTERGRUND`, `KONTORFREMD` purchase/sales) | a ship at another player's warehouse (U) | D string at `0x10086f31`; C p21-25 | U |
| X2 | Pirate warehouse (`PIRTAT_*`, protection money / assignment) | a ship at the pirates' stronghold, click their warehouse | C p20; D `0x100483de`, `0x10048cb2` | U |

**Finding (M-HE, U for GOG):** in HE the ship view is hosted under the **"?" main tab**, which turns orange
while a ship is selected (00:46.2-00:57, analysis §B3); deselecting leaves the "?" tab with `MAX_PIC`. The GOG
runner never reached a selected ship (`ui.md` §1), so whether GOG does the same is OQ-660.

**Sub-tab switching (D).** Ship view: the 8 tab gadgets share the click handler `0x10057560` → `0x10057420`:
it reads the clicked gadget's index (property `0x1b3`), stores it at `[view+0x5f0]`, shows sub-panel
`[view+0x5e4][index]` and hides the others (`0x10057471..0x10057531`). The player view's 5 tabs use handler
`0x1007f730` (not read further). The diplomacy action tabs and the route action tabs use the shared handler
`0x10041a50` (D `0x10080867`, `0x100267be`).

## 2. Elements per screen

Ids: **GOG** / **HE**. HE ids are GOG + 4 for GOG 825..1487, + 6 from 1488 on (HE inserts `OPTION_GAMESETTINGS`),
+ 5 for 6..823 (P: both `fenster.inc` parsed; HE drops GOG frame 824 of `WARENICONS_SOLDAT`). **Pixels:** 1604 of
the 1606 GOG frames equal their HE frame (texture pixel hashes, P); the two that differ (`WARENICONS_SOLDAT`
frame 21, `HISCORE_UNTERGRUND`) are outside this area. Every texture below is therefore pixel-identical in both
editions; only the id differs.

### 2.1 Ship view header and tab column (S0) — D `0x10056f1d..0x1005736d`

| Element | Graphic (GOG / HE, size) | Gadget (class, size) | Position (ref) | Text / data | Click / tooltip |
|---|---|---|---|---|---|
| Plank | `PLANKEGROSS` 1019 / 1023, 241 × 48 | `0xb17`, 241 × 48 (D `0x10057058`) | (782, 252) M-HE rms 38; GOG analogue (783, 252) `hud.txt` | — | — |
| Emblem | `GRNATIVESCHILDER` **+1** = 1422 / 1426, 45 × 52 (D `0x10056fa9`, `inc edi` `0x10056fe9`) | `0xb17`, 42 × 55 (D `0x10057001`) | (980, 249) M-HE rms 36 | ship emblem (constant) | — |
| Ship name | font (U which `.zei`) | `0xb1d` "SchiffsName", 241 × 39 (D `0x100570de`, `0x10057102`) | U (on the plank, HE shows it centred) | the ship's name; names exist in `Texte.dat` `SHIPS` (107 lines, P); assignment U (OQ-668) | — |
| Tab column | — | `0xb02`, 42 × 368 = 8 × 46 (D `0x100571d5`) | x 762 (A from tabs) | — | handler `0x10057560` |
| Tab i (0..7) | `VIEWINFOSCHIFFTAB_ADD` 924+2i / 928+2i idle, +1 selected (16 frames, 42 × 46) | `0xb0d`, 42 × 46 (D `0x100572db`); image `0x198` = base + 2i, `0x197` = 1, index `0x19c` = i (D `0x100572fc..0x10057335`) | (762, 303 + 46 i): M-HE tab 0 selected 929 at (761.7, 302.3) rms 21; tabs 1, 4-7 idle at y 349, 486, 533, 579, 625 rms 14-29 | — | tooltip `0x1c9` = `TOOLTIP:SCHIFFTAB_ADDON` line i (D `0x1005733d..0x1005735e`, section loaded at `0x10056f2d`) |

Tab order = `TOOLTIP:SCHIFFTAB_ADDON` (8 lines, P): 0 ship status, 1 ship behaviour, 2 repair, 3 load/unload,
4 sell ship, 5 ship list, 6 ships available for purchase, 7 automatic trade routes. HE icons in that order
(analysis §B5): "?", white flag on a ship, crossed tools (greyed), map (greyed), sails + coin, sails + "?",
coin + "?", orange arrow (M-HE). Greyed tabs 2 and 3 match no frame well (rms 37-40): drawn with a disabled
overlay (S; which overlay is U, OQ-661). The non-addon `VIEWINFOSCHIFFTAB` (14 frames) and
`TOOLTIP:SCHIFFTAB` are used by the **shipyard** view (2 tabs, `TOOLTIP:WERFTTAB`, D `0x10070259..0x1007046f`),
not by the ship view.

### 2.2 Ship status (S1) — D `0x100631b1..0x10063af8`

| Element | Graphic (GOG / HE) | Gadget | Position (ref) | Data | Click / tooltip |
|---|---|---|---|---|---|
| Background | `VIEWINFOSCHIFFMAIN_BK` 973 / 977, 245 × 467 | image (D `0x100631b1`) | (779, 301) A (same slot as `player_bk`); M-HE (777.7, 299.7) rms 35 | — | — |
| Cargo grid | — | `0xb03`, 216 × 108 (D `0x100633c5`) | origin (806, 302), stride 54 × 54, 4 per row: M-HE empty slot (row 1, col 2) at (913.7, 355.7) rms 13.5; A | — | — |
| Cargo slot | goods icon (`WARENICONS_*`, U which family and offset); empty = `VKEK_WARENFACH_LEER` 871 / 875, 50 × 50 | `0xb18`, 50 × 50, one per hold up to the hold count (loop `0x10063574`) | grid | hold goods; HE draws a vertical fill bar at the slot's right edge, red/orange (M-HE crop) | tooltip `TOOLTIP:WARENICONS` (D `0x10063451`); click U |
| Separator | `VIEWINFOSCHIFFMAIN_SEPERATOR` 981 / 985, 174 × 13 | `0xb17`, 178 × 13 (D `0x10063a93`) | first at (828, 416) M-HE rms 27; second ≈ y 554 (crop, U) | — | — |
| Crew grid | — | `0xb03`, 216 × 108 (D `0x100635f6`) | origin (806, 434): M-HE empty crew slot col 2 at (913.7, 434.3) rms 13.7 | soldiers/units aboard; HE: 2 soldiers + 1 empty slot | — |
| Crew slot | unit icon; empty `VKEK_WARENFACH_LEER`; bar `ENERGYBALKEN_GRUEN` 1336 / 1340 | `0xb12`, 50 × 50, **12** gadgets (D `cmp esi, 0xc` `0x100637e4`) | grid | health bar per unit (S, green in HE) | handler `0x10063c00` per slot (S: act on that unit; U) |
| Cannon slot | goods icon of goods `0x35` (D `push 0x35` `0x1006386c`, property `0x1bf`) | `0xb18`, 50 × 50 | ≈ (806, 567) from the crop, U | cannons aboard (`ship slot +0x8c(5)`, `ship-cargo.md` §11.6) | tooltip `TOOLTIP:SCHIFFMAIN` line 0 "%d Cannons on board" (P) |
| Build warehouse | `MAKEKONTORICON` 982/983 / 986/987, 107 × 111 | `0xb09` id 3, 107 × 111 (D `0x10063966..0x10063994`) | (912, 649) M-HE rms 20 | — | handler `0x10063ba0`: arms the frame's placement cursor with type `0x516` = 1302 (Kontor) and switches the view (`[view+0x9c](0xc, 1, 0)`) (D); tooltip `SCHIFFMAIN` line 1 (S: the per-gadget line counter at `0x100639f4`) |
| Throw cargo overboard | `MAKEKONTORICON` 984/985 / 988/989, 80 × 96 | `0xb09` id 4, 80 × 96 (D `0x1006397e..0x10063990`) | (802, 660) M-HE rms 9.3 | — | handler `0x10063b90` → `0x10062840` (overboard record, goods `0x8e` = all; `ship-cargo.md` §8); key **CTRL+W** (C); tooltip `SCHIFFMAIN` line 2 |

### 2.3 Ship behaviour (S2) — D `0x100603b1..0x10060878`, tooltips `TOOLTIP:SCHIFFKAMPF` (D `0x10060473`)

`VIEWINFOSCHIFFKAMPF_ICONS` GOG 987..994 / HE 991..998 = four pairs (idle, other state): 987/988 84 × 65,
989/990 92 × 82, 991/992 59 × 69, 993/994 75 × 82 (P). `TOOLTIP:SCHIFFKAMPF` (14 lines, P): 0/1 patrol off,
2/3 patrol on, 4/5 strike flag, 6/7 fly flag, 8/9 stop, 10/11 sink ship, 12 cancel boarding, 13 start boarding.

| Element | Graphic (GOG / HE) | Gadget, handler (D) | Position (ref), M-HE 58.6 s | Meaning |
|---|---|---|---|---|
| Background | `VIEWINFOSCHIFFKAMPF_BK` 986 / 990, 245 × 465 | image | (778, 302) rms 24 | — |
| Stop (hand) | 991 / 995, 59 × 69 | `0xb09` 59 × 69, handler `0x10060a00` → `[client+0xc0](x,0,0,0,0)` after `0x1005fd30`/`0x1005fd90` | (846, 401) rms 10.7 | S: the 59 × 69 gadget gets this handler (D `0x100606a4..0x100606bc`), the hand icon is that size (M-HE), tooltip "Stop" |
| Boarding | `ENTERHACKEN` 1566/1567 / 1572/1573, 92 × 69 | `0xb05` toggle 92 × 69, handler `0x10060960` | (835, 474) rms 33 | start/cancel boarding (S; HE draws an orange X over it) |
| Flag | 989 / 993, 92 × 82 | `0xb05` toggle 92 × 82, handler `0x10060990` → `[client+0x88](0, 0x100, …)` | (835, 576) rms 11.6 | fly / strike white flag (S, size match; key **W** fly flag, C) |
| Patrol | 987 / 991, 84 × 65 | `0xb05` toggle 92 × 69, handler `0x100608d0`: on → `0x1005fc00` (sets `[view+0x634]` = 1, arms a pick mode; S: choose patrol points), off → `0x1005fbc0` + `[client+0xc0](…, 1, …, 1, 0x30000)` | (839, 672) rms 14.8 | patrol on / off (S) |
| Sink ship | 993 / 997, 75 × 82 | `0xb09` 75 × 82, handler `0x10060a50` → `[client+0x88](0xb, 0, 0, ship)` | (935, 677) rms 15.5 | scuttle (S; C p17 "BEHAVIOR OF SHIP tab … SINK SHIP") |

Client slots (D, AnnoFrame): single player `tNetClient` vtable `0x1008914c`; multiplayer `tNetClientNet`
vtable `0x100893ec`: `+0x88` → `0x1005dd30` packs type **`0x75`**; `+0xc0` → `0x1005f200` packs type **`0x84`**.
Which original verbs the orange X marks (state "off" or "disabled") is OQ-663.

### 2.4 Load / unload (S4) — D `0x10059d00..0x1005a5c0`; no capture in either edition

| Element | Graphic (GOG / HE) | Gadget, handler (D) | Data / text |
|---|---|---|---|
| Hold slots | goods icon, frame `WARENRAHMEN` 835 / 839 | `0xb18` 50 × 50 (`0x10059d64`), tooltip line per slot | ship holds (`ship-cargo.md` §2: the selected hold = property `0x1b3` of `[[view+0x5d4]]`) |
| Unload arrow | `VIEWINFOSCHIFF_MENGENPFEILE` 1003.. / 1007.. (34 × 44/45) | `0xb09` 32 × 46, thunk `0x1005aa30` → `0x1005a7f0` (`ship-cargo.md` §2) | `SCHIFF_HANDELALL` line 0 "Unload goods" |
| Load arrow | same family, next frame pair | `0xb09` 32 × 46, thunk `0x1005aa20` → `0x1005a8d0` | line 2 "Load goods" |
| Amount selector | `VIEWINFOSCHIFF_MENGENBALL` 995..1002 / 999..1006 (4 pairs) | `0xb02` 152 × 53 holding 4 × `0xb0d` 38 × 52 (`cmp eax, 4`), handler `0x1005aa10` → `0x1005a5c0` ([view+0x640] = 1/5/10/50) | line 4 "Determine amount of load" |
| City goods list | goods icons by category; category tabs `TOOLTIP:SCHIFF_HANDELEIGEN` (resources, building material, consumer goods, weapons; D `0x1005afe5`) | U | the warehouse city's stock (selection read by `0x1005b2c0`, `ship-cargo.md` §11.1) |
| City plank | `PLANKEKLEIN` 1018 / 1022, 214 × 48 + emblem `GRNATIVESCHILDER` | `0xb1c` "StadtName" 214 × 48 (D `0x1005a462..0x1005a4d3`) | the target city's name |

Positions: **U** (OQ-662). Tab enabling (greyed without a city in range) is the only guard against a null
city (`ship-cargo.md` §11.1, OQ-CARGO-1/5).

### 2.5 Sell ship (S5) — D `0x10064451..0x10064cb8`, tooltips `TOOLTIP:SCHIFF_VERKAUF` (6 lines)

| Element | Graphic (GOG / HE) | Gadget, handler (D) | Position (ref), M-HE 59.6 s |
|---|---|---|---|
| Background | `VIEWINFOSCHIFFVERKAUFEN_BK` 962 / 966, 245 × 466 | image | (778, 301) rms 35 |
| Ship picture | U (figure sprite?) | `0xb17` 200 × 200 (`0x10064513`) | U |
| Price slider | `SLIDER_KNUPPEL` 845 / 849 on a bar `SCHIFFVERKAUFPLUSMINUS` 969 / 973, 159 × 37 | `0xb15` (`0x1006465e`), handler `0x10064f30` | bar (835, 594) rms 15.0 |
| Minus / plus | `SCHIFFVERKAUFPLUSMINUS` 965/966 / 969/970 and 967/968 / 971/972, 24 × 37 | `0xb09` 24 × 37 ×2, handlers `0x10064fd0`, `0x10065040` | minus (810, 594) rms 29; plus (994, 594) rms 20.5 |
| Price text | font | `0xb1c` 100 × 22 (`0x10064a54`) | U (HE shows coin + "0") |
| Offer / withdraw | `SCHIFFVERKAUFFLIP` 963/964 / 967/968, 54 × 64 | `0xb05` toggle 54 × 64, handler `0x10064f20` | (960, 698) rms 12.8 |

Meaning (C p18): set the price, then "offer ship for sale"; sold when the price is right. Who buys: U (OQ-667).

### 2.6 Ship lists (S6 = P4) — D `0x10061f21..0x100622d8` (one builder: the only `SCHATTENPLATTE` reference)

| Element | Graphic (GOG / HE) | Gadget (D) | Position (ref) |
|---|---|---|---|
| Shadow plate | `SCHATTENPLATTE` 1011 / 1015, 212 × 404 | image | (806, 320) M-GOG `f15_ship_list` rms 25; M-HE 61.2 s (805.7, 319.7) rms 19 |
| List | rows `DIPLISTE_UNTERGRUND` 1129 / 1133 (12 frames, 210 × 28) | `0xb22` list, 210 × 392 = **14 rows** × 28 (`0x10062102`); tooltips `TOOLTIP:SCHIFF_LISTE` | row 0 at (806, 320): M-GOG row 1 (806, 348) rms 4.3; M-HE (804.3, 319.7) rms 8.5. HE shows 14 slots (1 used) |
| Row: name | font | text | own ship drawn red in HE (selected or own: U) |
| Row: route icon | `SCHIFF_ROUTE` 1434 / 1438 (32 × 28; frame 1435 34 × 25) | from `0x100619ac` | (973, 320) M-GOG rms 4.0, 0.556 RGB565-equal; M-HE (973.7, 319.7) rms 18.7 |
| Row: health bar | `SCHIFF_LISTENENERGY` 940..961 / 944..965 (22 frames, 4 × 2 .. 4 × 23) | from `0x100619ef` | green bar left of the icon (M-HE crop); position U |
| Up / down | `VIEWINFOSCHIFF_SCROLL_PFEILE` 1007/1008 up 87 × 20, 1009/1010 down 86 × 25 | `0xb09` × 2, 86 × 25, handlers `0x1007b970` / `0x1007b990` (shared with the diplomacy and route lists) | up (871, 300), down (871, 715) M-GOG rms 4.2 / 3.6 |

Key **S** shows this list in the **player view, sub-tab 3** (M-GOG `f15_ship_list`: `VIEWINFOPIELERTAB`
1191 selected at (762, 441) rms 4.6; tab 0 idle 1184 at (762, 303)); the ship view's tab 5 shows the same
list (M-HE 01:01.2). Row click: U (OQ-668).

### 2.7 Ships for purchase (S7 = P5) — D `0x100610d1..0x10061738`, tooltips `TOOLTIP:SCHIFF_KAUFEN` (buy ship, price, pay)

`VIEWINFOSCHIFFKAUFEN_BK` 970 / 974 (245 × 466); plank `PLANKEGROSS` gadget 241 × 48; name `0xb1c` 241 × 38;
picture `0xb17` 200 × 200; `WALFAENGER_SCHIFFBAUSTART` gadget 172 × 38; price text `0xb1c` 115 × 15; buy
button `SCHIFFKAUF_ICON` 971/972 / 975/976 (80 × 55) as `0xb09` 83 × 56, handler `0x10061770` (all D).
HE shows an empty list (M-HE 01:01.0, 02:22). Positions U.

### 2.8 Automatic trade route (S8) — tooltips `TOOLTIP:SCHIFF_ROUTEMAIN` (14), `_ROUTETAB` (8), `_ROUTEACTION` (10); `ROUTETEXTS` "New entry", "No warehouses"

| Element | Graphic (GOG / HE) | Gadget, handler (D) | Position (ref), M-HE 62.0 s |
|---|---|---|---|
| Upper plate | `ROUTE_SCHTEENPLATTE` 1202 / 1206, 213 × 151 | image (`0x1002a1b1`) | (806, 321) rms 32 |
| Route-point list | rows `DIPLISTE_UNTERGRUND`, 210 × 140 = **5 rows** | `0xb22` (`0x1002a38d`, size `0x1002a4ba`); first row "New entry" (`ROUTETEXTS` 0) | under the up arrow; HE: "Neuer Eintrag" + 4 empty |
| Up / down | `ROUTE_SCROLL_PFEILE` 1204..1207 / 1208..1211 (86 × 22, 86 × 27) | `0xb09` 86 × 27, `0x1007b970`/`0x1007b990` | up (874, 298) rms 14.7; down (871, 465) rms 16.3 |
| Set route point | `ROUTENPUNKT_SETZEN` 1214.. / 1218.. (41 × 37, 4 frames) | `0xb09` 41 × 37, handlers `0x1002ac90`/`0x1002aca0` (D `0x1002a824..0x1002a8d5`) | (924, 485) rms 19 and (976, 485) rms 15.5 (two gadgets: set / delete route point, S from `ROUTEMAIN` lines 10-13) |
| Activate route | `ROUTEAKTIV` 1208/1209 / 1212/1213, 53 × 50 | `0xb05` toggle 53 × 50, handler `0x1002acb0` | (806, 476) rms 17.7 (HE: anchor + orange arrow) |
| Action tabs | `ROUTENACTIONS_TAB` 1194..1201 / 1198..1205 (4 × 2, 55 × 58) | `0xb02` 220 × 58 with 4 × `0xb0d` 55 × 58, handler `0x10041a50` (D `0x100267be..0x100268ee`) | y 524, x 803 / 914 / 968 measured (rms 21-26); stride 55 (A) |
| Action panel | `ROUTE_HINTERGRUND` 1203 / 1207, 219 × 179 | `0xb03` 216 × 216 (`0x10027fd6`) | (780, 642) rms 37 (weak) |
| Goods slots | `VKEK_WARENFACH_LEER`, `WARENRAHMEN`, bar `MENGENBALKEN` 1222 / 1226 (13 × 50) | `0xb09` 50 × 50 × 4 (`cmp esi, 4`), `0xb1b` 13 × 50 bars | bar (888, 652) rms 19.5 |
| Load/unload arrows | `VERLADEPFEILE` 1210/1211 / 1214/1215 (31 × 34/35) | `0xb1b` 31 × 41 | (915, 658) rms 17.3 |
| Amount arrows | `SLIDER_MENGENPFEILE` 1212/1213 / 1216/1217 (25 × 21) | `0xb09` 23 × 21 / 23 × 44, handler `0x10028bf0` | (998, 646) rms 32 |
| Delete goods | `ROUTENPUNKT_VERWERFEN` 1218..1221 / 1222..1225 (36 × 43, 42 × 45) | `0xb09` 43 × 54, handler `0x10028c90` | (934, 714) rms 18.9; (974, 713) rms 18.7 |

Action tab labels (`SCHIFF_ROUTETAB`, P): Load I, Load II, Unload I, Unload II. HE shows the four tiles with
a barrel/sack and a green down or grey up arrow, the first selected (M-HE crop). What "I" and "II" select is
OQ-669. World side: route node fields `+0xc` goods, `+0xe` direction, `+0xa` tons (`ship-trade-routes.md`).

### 2.9 Player view header and tabs (P0) — D `0x1007f200..0x1007f560`

| Element | Graphic (GOG / HE) | Gadget (D) | Position (ref) |
|---|---|---|---|
| Plank | `PLANKEGROSS` | `0xb17` 241 × 48 (`0x1007f21c`) | (783, 252) `hud.txt` (M-GOG); M-HE (781.7, 251.7) |
| Player name | font | `0xb1c` "Spielername" 241 × 38 (`0x1007f29a..0x1007f2be`) | on the plank |
| Crest | `GRPLAYERSCHILDER` 1403 + colour index / 1407 + i (8 × 42 × 54) | — | (982, 249) M-GOG rms 3.5 (id 1407 = index 4 on that start); index rule OQ-621 |
| Tab column | — | `0xb02` 42 × 276 (`0x1007f379`), handler `0x1007f730` | x 762 |
| Tab i (0..4) | `VIEWINFOPIELERTAB` 1184+2i / 1188+2i idle, +1 selected | `0xb0d` 42 × 46, `cmp esi, 5` (`0x1007f504`) | (762, 303 + 46 i) M-GOG `hud.txt`; M-HE: selected tab 0 on 44.5 s, tab 1 on 140.2 s, tab 3 on 138.0 s, all at x 761-763 |

Tab order and tooltips = `TOOLTIP:SPIELERINFOMAIN_TAB_ADDON` (5 lines, the only one of the two sections the
code references: D `0x1007f0ff`): 0 player info, 1 diplomacy status, 2 city list, 3 ship list, 4 ships
available for purchase. The column is 276 = 6 × 46 tall for 5 tabs (D): one slot unused (U).

### 2.10 Player info (P1) — D `0x1007ff21..0x100800ad`

`SPIELER_BILANZ_BK` 1128 / 1132 at (779, 301) (`hud.txt` `player_bk`, M-GOG; M-HE (777.7, 301) rms 21). **Seven**
text gadgets `0xb1c` 195 × 15: the loop runs edi = 0..6 (`xor edi, edi` at `0x1007ffd7`, `inc edi; cmp edi, 6; jle`
at `0x100800c7`) (D; this prep first read six, corrected by the orchestrator's review 2026-11-16). Labels
`TEXT:SPIELER_DIPLOMATIE` (6 lines: inhabitants, market stand revenue, operating costs, purchase costs, sales
revenue, military budget), tooltips `TOOLTIP:INFOSTADT_SPIELERBILANZMAIN` / `_STADTMAIN`. HE shows the same
seven value rows (the seventh = balance, red when negative; M-HE 00:44.5: 0, 0, 0, 0, 0, 17, −17), so there is
no edition difference here.

### 2.11 Diplomacy status (P2) — list D `0x10085a80..0x1008605f`, actions D `0x10080600..0x10080a05`

| Element | Graphic (GOG / HE) | Gadget, handler (D) | Position (ref) |
|---|---|---|---|
| Shadow plate | `SCHATTENPLATTE_KLEIN` 1145 / 1149, 212 × 181 | image (`0x10085b21`, in the list builder) | (804, 319) M-GOG `f15_player_menu` rms 22.5 |
| Partner list | rows `DIPLISTE_UNTERGRUND`, 210 × 168 = **6 rows** × 28 | `0xb22` (`0x10085cf6`, size `0x10085e33`), tooltips `TOOLTIP:SPIELERINFOMAIN` (choose player, up, down) | rows at (804, 320 + 28 k): M-GOG empty row (804, 460) rms 19.8; M-HE (804.3, 459.7) rms 17.7 |
| Row: name | font | text | HE 140.2 s: row 0 in red (selected or hovered, U); "Unknown" string at D `0x1008579c` (S: an undiscovered partner) |
| Row: shield | `KLPLAYERSCHILDER` 1393.. / 1397.. (player) or `KLNATIVESCHILDER` 1411.. / 1415.. (culture) | chosen by the partner kind: `cmp eax, 8/0xa/0xb/0xc` and `0xf/0x10/0x11` (D `0x10085100..0x100854ad`; player kinds `docs/sim` `Player::kind`: 8/11 human, 12 computer, 14 natives, 15 pirates, 16 Venice) | (974, 320 + 28 k) M-HE: rows 0-4 = `KLNATIVESCHILDER` HE 1416, 1419, 1420, 1418, 1417, rms 14-19 |
| Row: mood bar | `STIMMUNG_PLAYER` 1349.. / 1353.. (44 frames, 4 × 2 .. 4 × 23, two colour sets of 22) | from `0x10085260` | U (C p18: empty = neutral, green good, red hostile) |
| Up / down | up `VIEWINFOSCHIFF_SCROLL_PFEILE`, down `DIPLISTE_SCROLL_PFEILE` 1143 / 1147 (84 × 25) | `0xb09` 86 × 25, `0x1007b970`/`0x1007b990` (D `0x10085f38`, `0x10085f9d`) | up (871, 300), down (871, 494) `hud.txt` (M-GOG); M-HE (869.7, 299.7) / (872.3, 493) |
| Action tabs | `DIPACTIONS_TAB` 1146..1153 / 1150..1157 (4 × 2, 55 × 58) | `0xb02` 220 × 58 + 4 × `0xb0d` 55 × 58 (`0x10080930`), image `0x198`, `0x199`, tooltip `TOOLTIP:SPIELERINFOTAB` line 2i, handler `0x10041a50` | y 525, x 804 / 859 / 914 / 969 M-GOG (rms 27-42: drawn disabled); M-HE same grid |

Action tabs (P, `SPIELERINFOTAB`): 0 treaty status, 1 demand tribute, 2 pay tribute, 3 send message. In both
captures the action tabs and treaty buttons are drawn **disabled** (M-GOG rms 27-42; M-HE hatched, analysis
§B4): GOG Citizen has no discovered player; HE Großgrundbesitzer lists five **cultures** (native shields),
with which no treaty is possible (C p18). **Finding:** the manual says natives are not entered into the
list (C p18), the HE capture shows five native rows (M-HE); the code has a native-shield branch (D). The
manual sentence is not the rule (OQ-664).

**Treaty buttons (P2a)** — gadget class `0xb24` ×3 (D `0x10084700..0x1008497a`), tooltips
`TOOLTIP:SPIELERINFO_VERTRAG` (24 lines = 12 states × 2):

| Button | Graphic (GOG / HE) | Size (D) | Position (ref) M-GOG / M-HE | Handler |
|---|---|---|---|---|
| Trade agreement (kind 0) | `DIPACTIONS_HANDEL` 1158..1161 / 1162..1165, 70 × 78 | 70 × 78 | (821, 594) rms 31.5 / (821.7, 594.3) rms 25.6 | `0x10084a80` |
| Military alliance (kind 1) | `DIPACTIONS_BEISTAND` 1154..1157 / 1158..1161, 70 × 78 | 70 × 78 | (820, 676) rms 31.9 / (821.7, 677) rms 25.8 | `0x10084b20` |
| War / peace (kind 2) | `DIPACTIONS_KRIEG` 1162..1165 / 1166..1169, 120 × 129 | 120 × 129 | (898, 606) rms 29.8 / (899, 605) rms 24.6 | `0x10084bc0` |

State machine (D: each handler reads property `0x1cc` = state 0..3 of its button, jump tables `0x10084b10`,
`0x10084bb0`, `0x10084c50`, sends, then writes the new state):

| Kind | State 0 | State 1 | State 2 | State 3 |
|---|---|---|---|---|
| Trade / alliance | send 1 → state 1 ("offer") | send 0 → 0 ("withdraw offer") | send 3 → 3 ("accept") | send 0 → 0 ("cancel") |
| War / peace | send 3 → 3 ("declare war") | send 3 → 3 ("withdraw peace offer") | send 0 → 0 ("accept the offered peace") | send 1 → 1 ("offer peace") |

The labels follow `SPIELERINFO_VERTRAG` line order (offer, withdraw, accept, cancel; declare war, withdraw
peace offer, peace offered, offer peace) — S. The four frames per button are the four states (S). Send:
`0x10084a20(kind, value)` → net client `[frame+0x98]` slot **+0x7c**(kind, value, local player
`[view+0x618]`, partner `[world players][view+0x630]`) (D). Single player: AnnoFrame `0x10059e50` → world
slot `+0x104` = AnnoWelt **`0x100ad280`** (D). Multiplayer: `0x1005dbe0` packs **type `0x6f`**, 8 bytes:
`u8 0x6f, u8 kind, u8 from (player +0x40), u8 to, u32 value` (D). The relation bytes the world keeps are
`Player::relations` {trade, milit, war, credit} (T; `ship-founding.md` §15.1); how a button's state 0..3 is
derived from both players' bytes is OQ-664.

**Demand tribute (P2b)** — `TRIBUTEFORDERUNG_BK` 1167 / 1171 (213 × 178), tooltips
`TOOLTIP:SPIELERINFO_FORDERUNGEN` (money amount, goods amount); money slider (`SLIDER_KNUPPEL`, `SLIDER_FUEHRUNG`,
`SLIDER_MINUS`/`PLUS`), goods box (`VKEK_WARENFACH_LEER`, `WARENRAHMEN`, `WARENVKEKBEZEICHNUNG`) with its amount
slider (`SLIDER_MENGENPFEILE` 25 × 44, `MENGENBALKEN`) (D `0x100819a1..0x1008237c`). Send `0x10080d10`: net
client slot **+0xac** (two call shapes, `0x10080d84` and `0x10080dc7`) → MP `0x1005dc40` **type `0x70`**, 9 bytes
(D; field meaning OQ-665). C p20: money via the top slider; goods via an empty box, type, then the slider.

**Pay tribute (P2c)** — `TRIBUT_RAHMEN` 1170 / 1174 (213 × 31), tooltips `TOOLTIP:SPIELERINFO_VERBINDLICHKEIT`
(pay money tribute, demand money tribute), button `TRIBUTZAHLEN` 1168/1169 / 1172/1173 as `0xb09` 205 × 59,
handler `0x10084180`, plus a goods display and `BALKENWOHNHAUS` (D `0x100839d1..0x10083eb2`). The pay action
`0x10084080` reads the partner's demand (`partner slot +0xa4(0, local, &rec)`) and sends it back through slot
`+0xac` with flag 1 (D). C p20: goods tribute is paid by unloading at the partner's warehouse with an unarmed
ship flying the white flag.

**Send message (P2d)** — `MELDUNGSPLATTE` 1171 / 1175 (177 × 173); five buttons `MELDUNGS_ICONS` as `0xb09`
27 × 26 with labels `0xb1c` 158 × 26 (`cmp esi, 4`), handler `0x10083140`; texts `DIPLOMATIEMESSAGES` 0-4
(threat of war, moratorium, ingratiate, truce, insult; line 5 "Reserved" unused) (D `0x10082e51..0x10083070`).
Send `0x100830d0`: net client slot **+0xb0**(0x14, message index, partner, local) → MP `0x1005e460` **type
`0x7b`** (D). Receiving side: OQ-666.

## 3. HE versus GOG

| Item | GOG | HE | Evidence |
|---|---|---|---|
| ACCESS names, frame counts and sizes in this area | as §2 | identical | P (both `fenster.inc`/`.tex` parsed) |
| Texture ids | GOG | GOG + 4 (825..1487), + 6 (≥ 1488) | P |
| Texture pixels | — | identical in all frames of this area | P (pixel hashes) |
| Texts (`Texte.dat` sections of §2) | English | `Languages/Eng` section hashes identical; `Languages/Ger` same line counts | P |
| Code | View.dll 32-bit, addresses above | View.dll 64-bit; references the same ACCESS and tooltip names (each string present once) | P (strings); no HE addresses read |
| Ship view host | not captured | the "?" main tab is highlighted while a ship is selected | M-HE; OQ-660 |
| Disabled drawing | treaty buttons/action tabs rms 27-42 vs their frames | hatched overlay, rms 25-35 | M-GOG, M-HE; overlay source OQ-661 |
| Layout at a larger window | 1024 × 768 only | right chrome x = W − 245, scale 1.0 at ≤ 1200 lines | `he-ui-scale.md` |

No difference of layout was found between the editions: every element measured on both (ship list, player
tabs, diplomacy list, treaty buttons, scroll arrows) sits within ±2 px of the same reference position.

## 4. What the sim must provide

State the panels read. "T" names the rebuild's current field (`src/sim` or the app snapshot
`src/app/include/anno/app/opening_view.hpp`); **missing** means neither has it.

| Panel / element | Original source (D/S) | Rebuild today (T) |
|---|---|---|
| Selected ship identity | view `+0x630` (`ship-cargo.md`) | `ShipView::id` (`sim::FigureId`) |
| Ship owner (authority, colours) | figure owner | `Figure::owner`, `ShipView::owner` |
| Ship type / class (header, tab enabling) | figure type | `Figure::kind`, `ShipView::kind`; `FigureType` |
| Ship name (header, lists) | `SHIPS` texts; assignment U | **missing** (no name on `Figure`) |
| Holds: goods, amount (status, load tab, list) | ship `+0xb0 + 12 i` | `Figure::holds` (`ShipHold{goods, amount_88}`), `ShipView::holds` |
| Hold count, per-hold capacity (empty slots, fill bar) | type `LOGIC[0x13]`, `LOGIC[0x11] << 8` | `FigureType::nav->hold_count_a`, `FigureType::capacity_88` (sim); **missing in `ShipView`** |
| Crew / units aboard (12 slots) and their health | children of the ship; `LOGIC[0x14]` = second count (S) | `Figure::parent`, `Figure::crew_slot`, `Figure::hp`; `NavFacts::hold_count_b`; **missing in `ShipView`** |
| Cannons aboard / maximum | ship slot `+0x8c(5)`, type slot `+0x88(2)` | **missing** |
| Ship health (list bar, speed) | figure HP | `Figure::hp`, `NavMotion::health`, `NavFacts::max_hp`; **missing in `ShipView`** |
| Current order / route state (list icon, route tab) | route container ship `+0xa4` | `NavMotion::order`, `Figure::freight` (`FreightState`: stops, status, phase, stop_index); `ShipView::order_id`, `routed`, target; **freight missing in `ShipView`** |
| Patrol on/off and patrol points | U | **missing** |
| White flag state | U | **missing** |
| Boarding state | U | **missing** |
| Sale offer and price | U | **missing** |
| Ships offered for purchase | U | **missing** |
| Target city of load/unload (in range, name, stock) | `0x1005d8c0` | `ShipCargo::footprint_in_range` (pure, sim); `CityView{ref, name, warehouse, stock_88, capacity_88}` |
| Player name, crest/colour index | player record | `Player::name` (bytes), `Player::colour_slot`; app: **missing** (only `OpeningView::local`, `gold`) |
| Player list for diplomacy (discovered partners, kinds) | world player list; `Player` property 7 | `Player::discovery`, `Player::kind`; **missing in snapshot** |
| Relation per pair (button state) | player `+0x1ac + 6 · other` | `Player::relations` (`Relation{other, trade, milit, war, credit}`), `Player::relation(which, other)` |
| Pending offers (state 1/2 of a button) | U (OQ-664) | **missing** (no offer record) |
| Mood towards the local player (bar) | U | **missing** |
| Tribute demand record (amount, goods) | partner slot `+0xa4` | **missing** |
| Player balance rows | U | partly: city accounts (`city_accounts.hpp`); a player sum **missing** |

Commands the panels send:

| Click | Original (D) | Rebuild command |
|---|---|---|
| Unload goods | client `+0x60`, flag 1; MP `0x6a` | `UnloadCmd{ship, warehouse, hold, goods, tons}` (exists) |
| Load goods | client `+0x60`, flag 0; MP `0x6a` | `LoadCmd{ship, warehouse, hold, goods, tons}` (exists) |
| Amount 1/5/10/50 | view-local `[view+0x640]` | UI state only (no command) |
| Build warehouse | arms placement cursor type 1302 | UI mode, then `FoundCmd{ship, placement, click}` (exists) |
| Throw cargo overboard | client `+0x5c`; MP `0x6b` | **missing**: `JettisonCmd{ship}` (all holds, goods `0x8e`), flotsam object kind `0x654` (`ship-cargo.md` §8) |
| Stop | client `+0xc0` (MP `0x84`) (S) | `StopCmd{ship}` (exists; equivalence to the original record is U) |
| Move (right click on sea) | MP `0x6c` packed by AnnoFrame `0x1005d410` = `tNetClientNet` slot `+0x80`; single player `0x100591f0` (D here and `ship-navigation.md` §8) | `MoveCmd{ship, target_q8}` (exists) |
| Patrol on (pick points) / off | `0x1005fc00` mode; client `+0xc0` (MP `0x84`) | **missing**: `SetPatrolCmd{ship, points[]}` / `ClearPatrolCmd{ship}` |
| Fly / strike flag | client `+0x88` (MP `0x75`) | **missing**: `SetFlagCmd{ship, white: bool}` |
| Sink ship | client `+0x88(0xb, …)` (MP `0x75`) | **missing**: `ScuttleCmd{ship}` |
| Start / cancel boarding | handler `0x10060960` | **missing**: `BoardCmd{ship, target, on}` (target selection U) |
| Repair start/cancel, cannons ± (at a shipyard) | shipyard tab; C p17 | **missing**: `RepairCmd{ship, on}`, `CannonsCmd{ship, delta}` (cannon arms of `0x100ac360`, `rec[6]`) |
| Offer / withdraw sale, price | `0x10064f20`, `0x10064f30..0x10065040` | **missing**: `OfferShipSaleCmd{ship, price}` / `WithdrawShipSaleCmd{ship}` |
| Buy ship | `0x10061770` | **missing**: `BuyShipCmd{offer}` |
| Route: set/delete point, goods, amounts, activate | route handlers §2.8 | `AssignFreightRouteCmd{ship, stops[{warehouse, dock, actions[{direction, goods, tons}]}]}`, `ControlFreightRouteCmd{ship, Suspend/Resume/Cancel}` (exist); mapping of "I/II" OQ-669 |
| Treaty button | client `+0x7c`; MP `0x6f` {kind, from, to, value} | **missing**: `TreatyCmd{other, kind (trade/alliance/war), value (0/1/3)}`; the issuer is the host-stamped `from` |
| Demand / pay tribute | client `+0xac`; MP `0x70` | **missing**: `TributeCmd{other, mode demand/pay, gold, goods, tons}` (field layout OQ-665) |
| Send message | client `+0xb0(0x14, i, to, from)`; MP `0x7b` | **missing**: `DiplomaticMessageCmd{other, message 0..4}` (`state.md` lists `0x7b` with chat as net layer: decide sim vs net, OQ-666) |

`docs/sim/state.md` §8.3 keeps the original `0x6a/0x6b/0x6f/0x70/0x75/0x7b/0x84` as `LegacyRawCmd`
(refused): every **missing** row needs a native command and a sim rule before its button can act.

## 5. Evidence status per item

| Item | Status |
|---|---|
| Inventory of graphics (names, ids, sizes, frames) both editions | P |
| Tab orders, tooltips, labels | P + D (section references) |
| Gadget classes, sizes, counts, handlers of S0-S8, P0-P2d | D |
| Treaty state machine and its command record | D (labels S) |
| Behaviour tab: which icon is which verb | S (gadget size ↔ handler D, icon size ↔ position M-HE) |
| Positions: ship header, tabs, status, behaviour, sell, ship list, route, diplomacy list and treaty buttons | M-HE ±2 px; ship list, player tabs, diplomacy list, arrows, treaty buttons also M-GOG |
| Positions: load/unload tab, purchase tab, tribute and message panels, text boxes | U |
| Selection gesture, deselect, host main tab | M-HE (GOG U) |
| Disabled/greyed drawing | M (fact), source U |
| Row contents of lists (text colour, icons, bar heights) | M-HE partly, rule U |
| All world-side rules behind the missing commands | U (not read in this prep) |

## 6. Open questions (OQ-660 .. OQ-669)

- **OQ-660 — Ship view host and selection.** HE shows the ship view under the "?" main tab (highlighted) and a
  drag-select rectangle; GOG never reached a selected ship. Does a single click select; does a ship-list row
  click select and centre (key **J** "jump to active object", C p4); what Esc/deselect does; does selecting
  a ship switch away from the bust or construction tab? Needs: HE recording (packet §R) and the GOG view
  switch read from `[view+0x9c]` callers.
- **OQ-661 — Exact rectangles and disabled drawing.** HE positions are ±2 px from a compressed 0.75 video;
  GOG has no ship-view shot. Needed: lossless HE screenshots at 1024 × 768 or windowed scale 1.0 of every
  ship tab and the player tabs; the gadget positions (not set in the builders read, `0x50` sets only sizes:
  the position source — a later layout call or the parent — is unread, OQ-651); the disabled/hatched overlay;
  the cannon slot and second separator. (The player info's seventh row is no longer open: the code builds
  seven text gadgets, §2.10.)
- **OQ-662 — Load/unload tab layout.** Never captured (greyed before a Kontor in HE; unreached in GOG). Where
  the hold slots, arrows, amount balls, category tabs and city goods list sit; which goods a category shows
  (`ship-cargo.md` OQ-CARGO-5).
- **OQ-663 — Behaviour verbs.** The records behind client `+0x88` (MP `0x75`) and `+0xc0` (MP `0x84`) for stop,
  sink, flag, patrol; what the orange X marks (state off, or not allowed); the patrol-point pick mode
  (`0x1005fc00`, `[view+0x634]`); boarding target selection (`0x10060960`); the world functions behind them.
- **OQ-664 — Treaty button state and the partner list.** How property `0x1cc` (0..3) is computed from
  `Player::relations` of both players (and pending offers, which the rebuild has no record for); when buttons
  are disabled (natives, pirates, undiscovered "Unknown", missing trade agreement for an alliance, C p19); the
  mood bar `STIMMUNG_PLAYER` (44 frames) rule and colours; why the HE list shows five cultures although C p18
  says natives are not listed; list order.
- **OQ-665 — Tribute.** Field layout of the `0x70` record (9 bytes: goods, bit-packed mode/flags, u32 amount,
  two player ids), the slider ranges, the world function, how a goods tribute is fulfilled by unloading
  (C p20), and the AI's answer.
- **OQ-666 — Diplomatic messages.** Effect of `0x7b` (five canned messages) on the receiving player/AI and its
  display (`MELDUNGS_ICONS` 12 frames, 27 × 28); whether it belongs to the sim (AI mood) or only to the net
  layer (`state.md` puts `0x7b` with chat).
- **OQ-667 — Selling and buying ships.** Price slider range and step, `SCHIFFVERKAUFFLIP` states, who buys an
  offered ship and when (AI, Venetians), the purchase list's source, the price, and the two commands' records.
- **OQ-668 — Ship list rows and ship names.** Row text colour (HE: red), `SCHIFF_ROUTE` frame 0 (32 × 28) vs
  frame 1 (34 × 25) meaning (on route / idle), `SCHIFF_LISTENENERGY` height = health share (S), list order,
  which ships are listed (own only?), row click; how a new ship gets its name from `SHIPS` (107 lines).
- **OQ-669 — Trade-route editor mapping.** What "Load I / Load II / Unload I / Unload II" select (`SCHIFF_ROUTETAB`),
  how the route-point list, the four goods slots, `VERLADEPFEILE`, amounts and `ROUTEAKTIV` map onto the
  original route nodes (`ship-trade-routes.md`) and onto `FreightStop`/`FreightAction`/`FreightControl`; where a
  route point is set (map click on a warehouse?) and the 16 text gadgets (`cmp esi, 0x10` at `0x100286ea`).

## 7. Sources and scratch

- GOG `View.dll` SHA-256 `e32fa8a5…` (as `ship-cargo.md`), `AnnoFrame.dll`, `AnnoWelt.dll`: read with
  `E:\anno1503-worktrees\anno1503-UI-INFO-2\cache\re\pedis.py` and the condensers in
  `cache/prep-ships-diplomacy/scripts/` (`cond.py`, `cond2.py`, `xref.py`, `vt.py`, `findvt.py`).
- Parsed dumps: `cache/prep-ships-diplomacy/{gog,he}_fenster_{inc,tex}.txt`, `gog_texte.txt`,
  `he_{eng,ger}_texte.txt`; tables `scripts/acc_table.py`, `scripts/pixcmp.py`.
- HE frames: `cache/prep-ships-diplomacy/he_frames/f_<second>.png` (44.5, 47.0, 58.6, 59.6, 61.2, 62.0, 138.0,
  140.2, …) from `HE_UI_1.mp4`; matches `scripts/hematch.py` (configs `m1.json`, `m2.json`).
- GOG shots: `f15_ship_list.png`, `f15_player_menu.png` matched with `scripts/gogmatch.py`.
- Manual: GOG `game/app/Manual.pdf` pages 4-5, 17-20.
