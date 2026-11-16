# Statistics screens: player statistics, rating, city list, balance pages (UI-STATISTICS prep)

**Status:** research and specification only (2026-11-16, UI-PANELS-PREP). No implementation, no game runs, no
commits. Scratch work (frames, decoded textures, disassembly listings) is under
`cache/prep-statistics/` (git-ignored); nothing original is in this note beyond short labels, ids and numbers.

**Sources.** GOG 2.0.0.5 DLLs (code addresses are GOG, image base `0x10000000`), GOG and History Edition (HE)
`ToolGfx/fenster.tex/.inc`, `Texte.dat` (GOG; HE `Languages/Eng`, `Languages/Ger`), HE owner video `ui_5.mp4`
7:36.88–8:50.45 (1920 × 1080 capture of a 2560 × 1440 desktop, window client y 24..1040), GOG Wine-runner shots
`anno1503-UI-1-F2/cache/orig/ui-1/shots/` (`f15_options_tab`, `f15_rating`, `f15_city_list`).
Per the owner's rule (2026-11-16) the HE is the picture reference, GOG DLLs give the addresses.

**Evidence letters** (as in `ui.md` / `fisher.md`): **D** = disassembled (GOG module + address), **P** = parsed
file, **M-HE** / **M-GOG** = measured on a capture (named), **A** = arithmetic from D/P/M, **U** = unknown (an OQ).

**Coordinates.** All rectangles are in the original's 1024 × 768 frame. HE captures were converted with
`x1024 = (x − 282.6) / 1.3229`, `y1024 = (y − 24) / 1.3229` (the 4:3 frame fills the 1016-pixel-high client;
**A**). The conversion was checked on the four rectangles that the code gives (exit, memory, Stores/Productivity,
view switch): code and HE agree within 5 px (**D** + **M-HE**).

---

## 0. Summary of what was found

1. The screen in `ui_5` is **`tFrameSpielerStatistik`** ("player statistics", an AddOn screen; RTTI in GOG
   `AnnoFrame.dll` and HE `AnnoFrameAddOn.dll`, **P**), not the rating screen. Frame factory
   `AnnoFrame.dll` `0x100661b0`: kind 6 → `tFrameGameStatistik` (rating, `0xa0` bytes, ctor `0x1000f530`),
   kind 8 → `tFrameSpielerStatistik` (`0x100` bytes, ctor `0x10010f20`, vtable `0x10086b24`, init slot
   `0x10011020`) (**D**).
2. It opens with the character key **`-`** (`AnnoFrame.dll` char dispatch `0x1000bfc4`: `'-'` (0x2d) → case
   `0x1000c31a` → frame message `0x29`; `'q'` (0x71) → `0x1000c345` → message `0x28` = rating) (**D**). On a German
   keyboard `-` sits right of `.`, the rotate key the owner was pressing when the screen opened ("Oops, that was
   the statistics menu", 7:37) (**A** + **M-HE** transcript). It also opens from the options panel's third button
   (`ADDON_SPIELERSTATISTIK`, View.dll `0x10079d41`, handler `0x1007a340` sends `0x29`) (**D**).
3. **Its data is a per-city sampled history** that the original keeps in the city object and saves as the city
   record **`STST2`** (10 356 bytes): 60 samples, one every **2 500 ticks = 5 s** of game time, for (a) warehouse
   stock per goods, (b) productivity % per output goods, (c) supply: production and consumption per need goods
   (`AnnoWelt.dll` city tick step i `0x1002d3b0`, copy-out slot `+0xcc` `0x1002da10`) (**D**). The chart reads it
   back through city slot `+0xcc` (**D**). The x axis is 0..300 s (**D**; HE: one marker every 5 s, **M-HE**).
4. **The goods selection is persistent player state:** eight "Memory" slots (I–VIII), each a selected city and a
   list of (goods, flag) records, live in the player object (`+0x370`, `+0x37c + 16·slot`; player vtable slots
   `+0x130`..`+0x13c`) and are saved as the player records **`SEL_0`..`SEL_7`** (AnnoWelt writer `0x1001ec62`)
   (**D**). The frame writes them by calling the player directly, not through a game command (**D**).
5. At most **8 goods** are drawn at once; the line colour is the goods' slot in an 8-entry list (table
   `0x10094fd8`, slot 0 = COLORREF `0x22e2d2` = (210, 226, 34), the yellow of the HE wood line) (**D** + **M-HE**).
6. HE vs GOG: same textures (pixel hashes equal, ids +6), same `ADDONSTAT_1` texts, but HE inserts "Options menu"
   into `TOOLTIP:OPTION_MAIN_ADDON`, so the statistics button's tooltip is line 3 in HE and line 2 in GOG (**P**).
   HE draws the 1024 × 768 frame scaled to the window height, pillarboxed by a grape-vine border (**M-HE**).

---

## 1. Screens, states, opening and closing

| Screen | Class / where | Opens with | Closes with | Pause | Ev. |
|---|---|---|---|---|---|
| Player statistics, view "Stocks & Productivity", mode m[t] | `tFrameSpielerStatistik`, full 1024 × 768 frame, background `WARENSTATISTIK_BK1` | key `-` (WM_CHAR dispatch `0x1000c31a`, only when frame byte `+0x1826` is 0); options panel button 3 (`ADDON_SPIELERSTATISTIK`) | exit button (`ADDON_SPSTAT_EXIT`, handler `0x10014dd0` → `0x100147f0` sets close flag `+8`); a key (HE 8:50.45 closed with the cursor resting still on the chart for 0.9 s, so not by a click, **M-HE**; which key: OQ-671) | HE: no game time passes while open (boat stop 2.78 + 8.42 s = the usual 11.2 s; wood line ends at the opening), **M-HE**; GOG mechanism U (OQ-670) | D, M-HE |
| … mode % (productivity) | same frame, second LAGERSWITCH button | click "Productivity" | click "Stores" | – | D + M-HE (8:32.50–8:33.17) |
| View "Supply" (13 small charts) | same frame, background `WARENSTATISTIK_BK2` (handler `0x10014ae0` → `0x10014410`) | click the right view-switch button | click the left one | – | D + M-HE (8:38.03) |
| Rating | `tFrameGameStatistik`, `STATISTIK_UNTERGRUND` 1024 × 768 | key `q` (`0x1000c345`, message `0x28`); options panel button 2 (`OPTION_BILANZ`, handler `0x1007a310`) | "Continue" plank (`STATISTIK` line 18) | U (OQ-677) | D, M-GOG `f15_rating` |
| City list | player tab page in the right panel (`ui.md` §9.3) | key C, player tab side tab | other tab | no | M-GOG `f15_city_list` |
| Player balance page | `tViewSpieler_Bilanz` (View.dll), `SPIELER_BILANZ_BK` id 1128 (245 × 468) at (779, 301) | default right-panel page / ESC from construction (`ui.md` §3) | other tab | no | P, M-GOG `hud_idle_running` (`data/ui/hud.txt` `player_bk`) |
| City balance page | `tViewInfoStadt_Bilanz` (View.dll), `STADT_BILANZ_BK` id 677 (245 × 468) | city info tab | other tab | no | P (RTTI), `hud.txt` `info_bk` |

The character dispatch also maps `' '`, `+`, `,`, `.`, `C`, `H`, `b c d e h j k m n o p r s v w` to other cases
(jump table `0x1000c4e8`, index table `0x1000c548`) (**D**). The `Ctrl`/`Alt` accelerator table (resource 101 of
`AnnoFrame.dll`, both editions) has no statistics entry; HE reorders it and adds `Ctrl+B` → `0x9c5d` and
`Ctrl+F2` → `0x9c5f` (**P**).

Layout offset by resolution (init `0x10011020`): 1024 × 768 → (0, 0); 1152 → (64, 48); 1280 × 960 → (128, 96);
1280 × 1024 → (128, 128); 800 × 600 → scale `0.78125` (the `8` texture set) (**D** `0x1001106a`–`0x1001112d`).
All positions below are at offset (0, 0).

---

## 2. Player statistics: elements

### 2.1 Graphics (`ToolGfx/fenster.tex`, both editions)

| ACCESS (`fenster.inc`) | GOG id | HE id | Size | Use | Ev. |
|---|---|---|---|---|---|
| `WARENSTATISTIK_BK1` | 1568 | 1574 | 1024 × 768 | background, Stocks & Productivity view | P; D `0x10011081`, `0x10014ba6` |
| `WARENSTATISTIK_BK2` | 1569 | 1575 | 1024 × 768 | background, Supply view | P; D `0x10014c0d` |
| `ADDON_LAGERPROD` | 1574–1577 | 1580–1583 | 103 × 84 | view switch: 2 buttons × (idle, pressed) | P; D `0x100112ca` |
| `ADDON_LISTBUTTON` | 1570–1573 | 1576–1579 | 209 × 95 | city list up / down: 2 × 2 | P; D `0x1001160c` |
| `ADDON_SPSTAT_LAGERSWITCH` | 1600–1603 | 1606–1609 | 104 × 80 | Stores / Productivity: 2 × 2 | P; D `0x100122e5` |
| `ADDON_SPSTAT_EXIT` | 1582–1583 | 1588–1589 | 48 × 47 | exit (red X) | P; D `0x100125e1` |
| `ADDON_SPSTAT_MEMORY` | 1584–1599 | 1590–1605 | 50 × 50 | Memory I–VIII: 8 × (idle, pressed) | P; D `0x100128a9` |
| `ADDON_SPIELERSTATISTIK` | 1580–1581 | 1586–1587 | 94 × 82 | the opening button in the options panel | P; D View `0x10079d4e` |
| `WARENICONS_BAUMATERIAL` / `_GREY` | 755–758 / 759–762 | 760–763 / 764–767 | 50 × 50 | goods 41–44: colour = selected, grey = not | P; M-HE (wood colour id 761 when selected, all others grey) |
| `WARENICONS_BEDARF` / `_GREY` | 727–740 / 741–754 | 732–745 / 746–759 | 50 × 50 | goods 26–39 | P; M-HE |
| `WARENICONS_ROHSTOFF` / `_GREY` | 685–705 / 706–726 | 690–710 / 711–731 | 50 × 50 (the GREY entries 4 and 5 are 50 × 1 placeholders) | goods 1–4, 9–23 | P; M-HE |

Pixel hashes of all ids above are identical in GOG and HE; `HISCORE_UNTERGRUND` (GOG 1510 / HE 1516, rating
end screen) is the only statistics texture whose pixels differ (**P**, `anno-dump tex`).

### 2.2 Gadgets of the Stocks & Productivity view

Rectangles: **D** = from the init code (`+0x50` = size, `+0x5c` = position), **M** = measured on `ui_5` frame at
7:41 (`s_006`) and 8:25 (`s_105`). Tooltips are `Texte.dat` `ADDONSTAT_1` line numbers (the init walks a line
counter from 4, **D** `0x1001122b`); line texts in §2.6.

| Element | Rect (x, y, w, h) | Gadget / handler | Tooltip | Ev. |
|---|---|---|---|---|
| Background | (0, 0, 1024, 768) | `WARENSTATISTIK_BK1` | – | D, M-HE |
| Exit | (37, 27, 48, 47) | `0xb09`, `0x10014dd0` | 10 | D `0x10012658`–`0x1001267e`; M-HE (41, 30) |
| Memory I–VIII | container (37, 99), buttons 50 × 50 in 4 × 2 (I–IV top, V–VIII below) | `0xb00` + 8 × `0xb09` (radio), `0x10014c30` | 11–18 | D `0x10012867`–`0x10012a06`; grid M-HE (40, 101)–(238, 199) |
| Stores / Productivity | container (37, 225, 208, 80); buttons 104 × 80 at x 37 and 141 | `0xb00` + 2 × `0xb09` (radio), `0x10014900` | 8, 9 | D `0x100122a3`–`0x10012425`; M-HE |
| Goods group 0 (goods 41–45, 45 unused) | (288, 26), 4 per row, pitch 50 | 50 × 50 toggles, `0x100149c0` | U (OQ-674) | D `0x10012cbb`–`0x10012d41`; M-HE (288, 25) |
| Goods group 1 (26–40, 40 unused) | (288, 98) | same | U | D; M-HE (288, 97) |
| Goods group 2 (1–4) | (535, 26) | same | U | D; M-HE (534, 25) |
| Goods group 3 (9–25, 24/25 unused) | (535, 76): continues group 2's grid one row lower | same | U | D; M-HE |
| View switch | container (775, 36, 206, 84); left button "Stocks & Productivity", right "Supply", 103 × 84 each | `0xb00` + 2 × `0xb09`, `0x10014ae0` | 4, 5 | D `0x10011282`–`0x10011418`; M-HE (783, 42) |
| City list | (795, 223, 169, 425) | `0xb21` list, select `0x10014880` | – | D `0x10011500`–`0x10011533`; M-HE text "Alle anzeigen..." at (794, 224) |
| List up / down | (776, 121, 209, 95) / (776, 649, 209, 95) | `0xb09`, `0x100148c0` (list msg `0xb`) / `0x100148e0` (`0xc`) | 6 / 7 | D `0x100116a4`–`0x10011713`; M-HE |
| Chart | plot ≈ (65, 332)–(732, 723); y axis at x ≈ 65, x axis at y ≈ 723 | chart object `+0xd4`/`+0xd8` | – | M-HE; position in code not read (U) |
| Axis captions "m[t]"/"%" and "t[s]" | (75, 317, 75, 25) and (635, 693, 75, 25) | 2 × `0xb1c` text gadgets | – | D `0x10012a30`–`0x10012ca2`; M-HE "m[t]" at (77, 320), "t[s]" ends at x ≈ 710 |

Goods shown (**D** + **M-HE**): group 0 tools 41, wood 42, bricks 43, marble 44; group 1 the 14 consumer goods 26–39
(`WARE_BEDARF` order: food … wine); groups 2–3 the 19 raw goods 1–4, 9–23. Goods whose name lookup returns
`UNUSED` (message `0x20` to the game object) are skipped (`0x10012d98`), so stone (5) and marble stone (6) never get
an icon (they are 1-pixel placeholders in `_GREY`, **P**).

Icon state: grey texture = not selected, colour texture = selected (HE wood after the click, ids 761 vs 766 by
SAD, **M-HE**). The pressed state comes from the player's memory slot (`+0x138`, message `0x1ad`, `0x100130ca`,
**D**).

### 2.3 Gadgets of the Supply view

| Element | Rect | Ev. |
|---|---|---|
| Background `WARENSTATISTIK_BK2` | (0, 0, 1024, 768) | D, M-HE |
| 13 small charts, goods 26–31 and 33–39: left column 26, 27, 28, 29, 30, 31, 33; right column 34–39 (right column bottom empty) | column x offset **366** (`add eax, 0x16e`, `0x10012057`, after the 7th row); icon top-left (41, 25) left, (407, 25) right; row pitch ≈ 106.6 | D `0x1001186e`–`0x10012074`; M-HE icon SAD 5.6–15.7 per icon vs `WARENICONS_BEDARF` |
| Goods 32 (medicinal herbs) **has no chart**: `cmp esi, 0x20; je` at `0x1001188e`; 40 is `UNUSED` | – | D + M-HE |
| Each small chart: icon 50 × 50 (colour), y axis "m[t]" 0..10 labelled 0, 2, 4, 6, 8, 10; x axis "t[s]" labelled 30 … 270 step 40; plot ≈ 254 × 45 | M-HE (`s_130`) |
| View switch, city list, list buttons | as in §2.2 (the left part with memory, exit and switch is not shown) | M-HE |

### 2.4 Chart content (Stocks & Productivity)

From the draw functions `0x10013e70` (% axes), `0x10013f60` (m[t] axes) and `0x100140c0` (lines) (**D**):

- **x axis:** min 0, max `60 · interval / 500` = 300 s, tick step 10, axis colour COLORREF `0xff` (red), label colour
  `0xffffff` (`0x10013ea9`–`0x10013f40`, `0x10013fc9`–`0x10014024`). Point *k* of 60 (oldest first) is at
  `x = 59 · interval/500 − (59 − k) · interval · 0.002` s = 0, 5, … 295 s (`0x10014243`, float `0x10086ba4` =
  0.002 = seconds per tick) (**D**). HE: ticks every 29.4 capture px = 10 s, a dark marker on the line every
  15 px = 5 s, the line ends at t ≈ 295 (**M-HE**). Labels: "20 … 280" in steps of 20 (big chart) and "30 … 270"
  step 40 (small charts) are **not** aligned with the ticks (label "20" sits on the 10 s tick, "280" on 280 s);
  their placement rule is OQ-673 (**M-HE**).
- **y axis, m[t]:** min 0, step 10, max = the city's `max capacity seen` (block `+0x106c`, below) for one city.
  For "Alle anzeigen" the code runs `if (ymax < cap_i) ymax += cap_i` over the cities, then divides by their count
  (`0x1001402e`–`0x10014091`) (**D**). HE: 0..70 with Kontor + marketplace (capacity table `[2]` = 70 t,
  `annowelt.md` (g)) (**M-HE** + **A**).
- **y axis, %:** 0..100 step 10 (**D**; **M-HE** 8:32.8).
- **Axis labels:** `ADDONSTAT_1` line 1 "m[t]" or line 2 "%" (`0x100143c7`–`0x100143fc`), line 3 "t[s]" (**D** + **P**).
  `[[` in `Texte.dat` is an escaped `[` (**A**).
- **Series:** one polyline per selected goods, in the selection-list order (max 8). Stocks: y = byte × divisor
  (tons) per city, summed over the cities, divided by the city count when "Alle anzeigen" is selected (an
  **average**, not a total; `0x10014350`–`0x10014371`). Productivity: y = the % byte, same averaging (**D**).
- **Colours:** COLORREF table `0x10094fd8` (slot 0..7): `0x22e2d2`, `0x3f03c6`, `0xbc0707`, `0xc58d13`,
  `0xcdc20e`, `0x84db1d`, `0x065e10`, `0xbb13bd`, read as `0x00BBGGRR` → (210,226,34), (198,3,63), (7,7,188),
  (19,141,197), (14,194,205), (29,219,132), (16,94,6), (189,19,187) (**D**; slot 0 matches the HE wood line
  ≈ (208, 215, 61), **M-HE**). The same 16 bytes are in HE `AnnoFrame.dll` and `AnnoFrameAddOn.dll` (**P**).

### 2.5 Chart content (Supply)

`0x10014410` (**D**, read in outline): per goods 26–39 (not 32) two polylines of 60 points, colours
`0xfaf023` and `0xfffff0` (`0x100147a6`, `0x100147c7`), from the block's production and demand rows, y max =
block `+0x20d8` (§3.1). HE shows **one yellow dashed line** at ≈ 1 t for food and beer and nothing visible for the
others (production presumably 0, on the red axis) (**M-HE**). Which series is which colour, and how `0xfaf023`
becomes yellow on HE while the stocks colours read as `0x00BBGGRR`, is OQ-672.

### 2.6 Texts

`Texte.dat` section **`ADDONSTAT_1`**, 19 lines, identical hash in GOG and HE English (**P**): 0 "Display all…"
(HE German "Alle anzeigen..."), 1 "m[t]", 2 "%", 3 "t[s]", 4 "Stocks & Productivity", 5 "Supply", 6 "Up",
7 "Down", 8 "Stores", 9 "Productivity", 10 "Exit", 11–18 "Memory 1" … "Memory 8".
City names in the list: the cities' names (city property `0x12`, `STAEDTE` list) (**D** `0x100115a1`–`0x10011605`,
message `0x12`); HE "Bad Aussee" (**M-HE**). The first entry "Alle anzeigen..." is drawn red, the city white (**M-HE**;
whether red = selected: OQ-679).
Options button tooltip: `TOOLTIP:OPTION_MAIN_ADDON` line 2 (GOG, 5 lines) / line 3 (HE, 6 lines, "Options menu"
inserted at 2) (**P**; GOG index from the button loop counter, **D** `0x10079d0a`).

### 2.7 Clicks and state

| Action | Effect | Ev. |
|---|---|---|
| Goods icon | `0x10014a40`: if the goods is in the 8-slot list `0x1009b930`, remove it; else put it in the first free slot; if all 8 are taken, refuse and un-press the icon (`0x1ad` with 0). Then player `+0x13c(memory, goods, flag)` and redraw (`0x10014900`) | D |
| Memory I–VIII | `0x10014c30`: memory index → frame `+0xd0`; city list selection ← player `+0x130(memory)`; every icon's pressed state ← player `+0x138(memory, goods)`; the 8-slot list is rebuilt in goods-number order 1..45 from the selected goods | D |
| City list entry | `0x10014880`: frame `+0xc8` ← entry value (−1 = all, else the city index in the player's city list), player `+0x134(memory, value)`, redraw | D (−1 test at `0x10014034`) |
| Stores / Productivity | redraw with m[t] (`0x10013f60` + lines mode 1) or % (`0x10013e70` + mode 0) | D + M-HE |
| View switch | `+0xcc` = 1 (stocks, `BK1`) or 2 (supply, `BK2`, `0x10014410`) | D + M-HE |
| Up / Down | list scroll (messages `0xb` / `0xc`) | D |
| Exit | close flag | D |
| Hover | tooltips (gadget message `0x26`/`0x1c9` with the `ADDONSTAT_1` line) | D; HE hover not recorded (OQ-674) |

On open (ctor `0x10010f20`): the 8-slot list and `+0xd0` (memory 0) are zeroed, view = stocks (`+0xcc` = 1); the
init rebuilds the selection from player memory 0 (`0x10013231`–`0x100132ad`) (**D**). HE: "I" drawn pressed, no
goods selected, empty chart at 7:41 (**M-HE**).

---

## 3. The data: what the original samples

### 3.1 City block `STST2` (AnnoWelt city `+0x84c` … `+0x30bf`, 0x2874 bytes)

Sampler: city tick step i `0x1002d3b0` (called last in the city tick `0x1002d9a0`), three independent countdowns
of **2 500 ticks** (1 tick = 2 ms, `production.md` §1, `population.md` §9) driven by `u16(t − city stamp +0x14)`
(**D**). Each sample shifts its 60 rows by one (oldest dropped) and writes row 59.

| Block offset (city) | Type | Meaning | Ev. |
|---|---|---|---|
| `0x0000` (+0x84c) | u8 | stock divisor = `ceil(capacity_t / 256)` (≥ 1 when capacity > 0) | D `0x1002d3f3`–`0x1002d438` |
| `0x0001` (+0x84d) | u8 | not written by the sampler | U |
| `0x0002` (+0x84e) | u16 | 2500, the stock interval (ticks) | D |
| `0x0004` (+0x850) | 60 × 70 u8 | stock rows; byte g (g < 56) = `trunc(stock_t[g] / divisor)` from city store `+0x334 + 12g` (`>> 8`); bytes 56..69 never written | D `0x1002d465`–`0x1002d484` |
| `0x106c` (+0x18b8) | u16 | largest capacity in tons seen (only grows) | D `0x1002d431`–`0x1002d442` |
| `0x106e` (+0x18ba) | u16 | 2500 | D |
| `0x1070` (+0x18bc) | 60 × 70 u8 | productivity rows, % per output goods g (0 < g < 70) | D `0x1002d4ba`–`0x1002d64b` |
| `0x2098` (+0x28e4) | 64 bytes | not written by the sampler | U |
| `0x20d8` (+0x2924) | u16 | supply y max in tons: `m = (max + 0x80) >> 8`, then `m + (10 − m mod 10)` (always the next multiple of 10 above) | D `0x1002d8d5`–`0x1002d8f3` |
| `0x20da` (+0x2926), `0x249e` (+0x2cea) | u16 | 2500 | D |
| `0x20dc` (+0x2928), `0x24a0` (+0x2cec) | u8 | supply divisor `ceil(ymax / 256)`, ≥ 1 | D `0x1002d8f1`–`0x1002d963` |
| `0x20dd` (+0x2929) | 60 × 16 u8 | production rows, goods 26..41 | D |
| `0x24a1` (+0x2ced) | 60 × 16 u8 | demand rows, goods 26..41 (41 always 0: demand counts 26..40 only) | D |
| (+0x30b0 / +0x30b4 / +0x30b8) | u32 | the three countdowns | D |
| (+0x30bc / +0x30bd) | u8 | set to 59 then incremented after a sample (= 60); meaning U | D |

Rules (**D**, `0x1002d3b0`):
- **Productivity:** for each building of the city list taken through city slot `+0x60` (list `+0x24(1)` / `+0x14(i)`),
  for its type's output lines j = 0, 1 with goods g = type `+0xac(j)`, 0 < g < 70: `A = type +0xc8(j, 0)`
  (display rate), `B = building +0x94(g)` (measured rate; workshop `0x100684e0` = `u16 +0x20 · 60 >> 10`, farm
  `0x1006ef30`, mine `0x1006e2f0`, base `0x10025410` not read); `p = A ? min(100, 100·B / A) : 0`; the byte is the
  mean of p over all such buildings, capped at 100.
- **Production (supply):** sum of `building +0x94(g)` for output goods 0x1a ≤ g < 0x2a.
- **Demand:** for the five population groups i (game `+0x6c` group table, `+0xcc` need records of 4 bytes:
  goods byte, u16 amount; second list of records where a byte `0x4c` (WIRTSHAUS) counts as goods 0x1d ALKOHOL):
  `Σ amount · residents_i / 16` for goods 0x1a..0x28, residents_i = city slot `+0x5c` → slot 0 `(i)`.
- **Rounding:** both supply series `((v + 0x80) >> 8) / divisor`.

This is **likely** what the screen is meant to show: the stocks as tons, productivity as achieved/nominal rate,
supply as tons per minute produced vs consumed (**A**: the rate getters multiply by 60, `production.md` §1;
`Menge` is tons per 100 residents per minute, `population.md` §9).

### 3.2 Read side

The frame enumerates the player's cities (player slot `+0x84(&list, 2)`, list at player `+0x26c + 8·2`) and copies
each city's block with city slot `+0xcc` (`0x1002da10`, `0x2862` bytes from `+0x84c`) (**D**
`0x100141bf`–`0x100141dc`). It never reads live stock: a selection made now shows history up to the last sample.

### 3.3 Player memory slots (`SEL_0` … `SEL_7`)

Player (AnnoWelt, vtable `0x100c2238`; slot `+0x14` returns type `0x1000`, which the frame's slot 10 `0x10013430`
accepts as its data source) (**D**):

| Slot | Code | Data |
|---|---|---|
| `+0x130(m)` | `0x10018870` | `dword [player+0x370][m]`: selected city entry of memory m |
| `+0x134(m, v)` | `0x10018880` | sets it |
| `+0x138(m, g)` | `0x100188a0` | the u16 flag of record `{u16 goods, u16 flag}` with goods g in vector `player+0x37c + 16·m` (0 if absent) |
| `+0x13c(m, g, f)` | `0x10018900` | updates or appends the record |

Saved per player as `SEL_%d` (writer `0x1001ec62`: the dword, then the vector of 4-byte records) (**D**). `sze.md`
lists `SEL_0`…`SEL_7` in the state series (**P**). That these are the statistics memories (and not unit groups) is
**likely** from the matching shapes and slot count; the reader and the corpus contents are OQ-675.

---

## 4. HE vs GOG

| Item | GOG | HE | Ev. |
|---|---|---|---|
| ACCESS ids of statistics textures | 1509, 1568–1603 | +6 (1515, 1574–1609) | P |
| Pixels of those textures | – | identical hashes; `HISCORE_UNTERGRUND` differs | P |
| `ADDONSTAT_1`, `STATISTIK` (English) | hash `b870e064…`, `b22349f9…` | same | P |
| `TOOLTIP:OPTION_MAIN_ADDON` | 5 lines, "Player statistics" = line 2 | 6 lines, "Options menu" inserted, "Player statistics" = line 3 | P |
| Frame size | 1024 × 768 at (0, 0), or centred at 1152/1280, or scaled 0.78125 at 800 × 600 | the 1024 × 768 frame stretched to the client height (×1.3229 in the capture) and centred, grape-vine border left and right | D, M-HE |
| Code | `AnnoFrame.dll` x86 | `AnnoFrame.dll` x86-64 + `AnnoFrameAddOn.dll` x86, both carry the colour table | P |
| Opening key | `-` (D) | screen opened while the owner pressed `.` repeatedly; `-` is the neighbouring key | D + M-HE |
| Supply line colour | `0xfaf023` / `0xfffff0` in code | yellow | OQ-672 |

---

## 5. What the sim must provide

The panel reads only history and per-player selection state; it sends **no gameplay command** in the original
(**D**: every write is a direct player call). Names are from main `0f46b1b2`.

### 5.1 State read

| Quantity | Original | Rebuild today | Status |
|---|---|---|---|
| City stock per goods (8.8) | city `+0x334 + 12g` | `sim::City::stock.stock[g]` (`production.hpp` `CityStock`); app `CityCounters::goods` in `PlaySnapshot::cities` | exists |
| City capacity (8.8) | `+0x624` | `CityStock::capacity` | exists (not in the snapshot) |
| Max capacity seen (t) | block `+0x106c` | – | **missing** |
| Stock history 60 × goods, divisor | block `+0x0000`, `+0x0004` | – | **missing** |
| Building measured rate per output goods | building slot `+0x94(g)` | `Workshop::activity`, `WeaponsWorkshop::activity`, `Farm::activity`, `Mine::activity` exist; the rate formula exists only for the workshop (§3.1) | **partly**: getter missing, farm/mine/base formulas unread (OQ-676) |
| Type display rate per line | type `+0xc8(j, 0)` | `ProductionType::display_rate0/1` (`production.hpp:63`) | exists (argument 2 meaning U) |
| Productivity history 60 × 70 | block `+0x1070` | – | **missing** |
| Residents per group per city | city `+0x5c` slot 0 `(i)` | `World::city_population` (total only) | **partly**: per-group read missing |
| Need records per group (goods, amount; inn → alcohol) | game `+0x6c` group `+0xcc` | `house_groups.hpp` (BGruppen) | exists (check the inn record mapping) |
| Supply history 2 × 60 × 16, y max, divisor | block `+0x20d8`…`+0x24a1` | – | **missing** |
| Sample countdowns (3 × u32, 2 500 ticks) | `+0x30b0..+0x30b8`, stamp `+0x14` | `City::tick_stamp` (D `[city+8]`) exists; countdowns missing | **missing** |
| Save / load of the history | `STST2` raw record | – (legacy import does not read it) | **missing** |
| Player's cities in list order | player `+0x84(…, 2)` | `PlaySnapshot::cities` in (island, city) order | exists; order vs original U (OQ-679) |
| City name | property `0x12` | `City::name` (not filled from files yet) | partly |
| Memory slots: 8 × (city entry, goods flags) | player `+0x370`, `+0x37c` | – | **missing** |
| Paused flag while open | U | `PlaySnapshot::paused`, `SetSpeedCmd` | exists; whether the screen uses it: OQ-670 |

### 5.2 Commands

| Panel action | Original | `sim::Command` today | Needed |
|---|---|---|---|
| Open / close, view, mode, list scroll, memory choice | frame-local | – | none (UI state) |
| Goods toggle | player `+0x13c(memory, goods, flag)` direct | – | **missing** if the memories stay sim state: `SetStatsGoods{player, memory u8 (0..7), goods GoodsId, flag u16}`; else a local UI store (owner decision, OQ-675) |
| City list choice | player `+0x134(memory, value)` direct | – | **missing** likewise: `SetStatsCity{player, memory, city_entry i32 (−1 = all)}` |
| Pause on open | U | `SetSpeedCmd` | OQ-670 |

### 5.3 Snapshot additions (app)

A read-only `CityStatistics` per city in `PlaySnapshot` (block contents as §3.1: divisor, max capacity, the three
row sets, their intervals) and the local player's 8 memory slots. Sampling must be on the sim clock (ticks), never
wall time.

---

## 6. Evidence status per item

| Item | Status |
|---|---|
| Frame classes, factory, open keys `-` / `q`, options buttons and their handlers | D |
| Textures (ids, sizes, states, grey/colour), HE id shift, identical pixels | P (+ M-HE for grey/colour use) |
| Texts `ADDONSTAT_1`, tooltip indices, HE "Options menu" shift | P (+ D for the index counters) |
| Rectangles of exit, memory, Stores/Productivity, view switch, list, list buttons, goods groups, supply columns | D, confirmed M-HE |
| Chart plot rectangle, small-chart geometry, label positions | M-HE only |
| Sampling (interval, length, formulas, block layout) | D (farm/mine rate getters U) |
| Axis ranges, tick step, colours of the stocks lines, averaging over cities | D (+ M-HE for range, ticks, slot-0 colour) |
| Supply series colours | D vs M-HE conflict (OQ-672) |
| Memory slots in the player, `SEL_n` save | D (meaning likely; reader U) |
| Pause, closing key, hover tooltips, red "Alle anzeigen" | M-HE inferred / U |
| Rating screen data rows | U beyond `ui.md` §9.5 and `STATISTIK` texts (OQ-677) |
| City list (C), balance pages | as `ui.md` §9.3 and `hud.txt`; their figures are city statistics 0..14 (`population.md` §8) — not re-read here |
| GOG capture of the player statistics screen | none exists (no Wine-runner shot) |

---

## 7. Open questions (OQ-670 … OQ-679)

- **OQ-670 — Does the player statistics screen pause the game, and how?** HE: no game time passes while it is open
  (boat timing, wood line end; `ui_5-report.md` §2, inferred). GOG: no pause call found in the frame; the frame's
  slot 19 `0x10013cb0` message table (cases at `0x10013d38`) and the frame manager were not read. Needed: the
  mechanism (frame flag vs speed command) and whether multiplayer differs.
- **OQ-671 — Which key closes it?** HE 8:50.45 closed with the mouse still (no click). Candidates: ESC, `-` again,
  any key. Read the frame's key path (slot 19 / the frame manager) or record it on HE (§ recipe in the packet).
- **OQ-672 — Supply series colours and meaning.** Code: two polylines per goods, colours `0xfaf023`, `0xfffff0`
  (`0x100147a6`, `0x100147c7`); HE shows a yellow dashed line ≈ 1 t on food and beer. Which row (production or
  demand) is which colour, how `0xfaf023` is interpreted (the stocks table reads as `0x00BBGGRR`), and whether
  HE changed it.
- **OQ-673 — Axis label placement and font.** Labels 20…280 (step 20) and 30…270 (step 40) do not sit on the
  10-s ticks; "20" is centred on the 10-s tick, "280" on 280 s (HE `s_006`, `s_130`). The rule (label count from
  the width? per-label offset?) and the `.zei` font are not read (chart class in `AnnoDisplay.dll`
  `tDrawLayerDiagramm`, not opened).
- **OQ-674 — Goods icon hover and tooltips.** The goods icons get a tooltip and three texture messages
  (`0x198`, `0x199`, `0x19c`) whose states (idle, hover, pressed, disabled) are not mapped; the goods name source
  (game name lookup kind `0x20` gives the internal name) vs `WARE_*` sections is U. No HE hover in `ui_5`.
- **OQ-675 — Memory slots: meaning and ownership.** Are `SEL_0`…`SEL_7` (player `+0x370`, `+0x37c`) exactly the
  statistics memories (read the loader and the corpus values), what does the flag u16 hold besides 0/1, and
  should the rebuild keep them in the hashed sim state (commands `SetStatsGoods`/`SetStatsCity`) or in a local
  per-player UI store saved beside the game? Owner decision; the original mutates player state directly from
  the UI.
- **OQ-676 — Productivity inputs.** The rate getters `+0x94` of the farm (`0x1006ef30`), mine (`0x1006e2f0`) and
  base class (`0x10025410`), the second argument of type `+0xc8(j, 0)`, which building list city slot `+0x60`
  → `+0x24(1)` enumerates, and the unwritten block bytes (`+0x0001`, `+0x2098..+0x20d7`, `+0x30bc/+0x30bd`).
- **OQ-677 — Rating screen data.** `tFrameGameStatistik` (init `0x1000f5e0`) rows (`STATISTIK` lines 3–16:
  villages, subjects, satisfaction, capital, islands, soldiers defeated/died, ships sunk/lost, enemies, bonus,
  summary) — which player/world values and which score formulas; the player record `STATIS` (12 bytes); HE's
  different `HISCORE_UNTERGRUND`. Out of this packet's first steps; needs its own read.
- **OQ-678 — HE frame presentation.** The source of the grape-vine pillarbox art, whether the frame follows
  `UI_SCALE` or always fits the window height (`he-ui-scale.md` covers the in-game UI, not full frames), and the
  filtering of the stretched frame.
- **OQ-679 — City list semantics.** "Alle anzeigen" shows the **average** over the player's cities (D); confirm on
  HE with two or more cities (and the y-max quirk `if (ymax < cap) ymax += cap`). Also: red text = selected
  entry?, list order (player list vs (island, city)), scroll limits with more than ~25 cities.
