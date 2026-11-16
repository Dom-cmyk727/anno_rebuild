# Warehouse (Kontor) and trade panels of the original

**Status:** research and specification only (UI-PANELS-PREP, 2026-11-16). No engine code, no game runs.
Read-only static analysis of GOG `View.dll` / `AnnoFrame.dll` (pefile + capstone, virtual addresses,
image base 0x10000000), parsed game files of both editions, and measurements on existing GOG Wine-runner
shots and the owner's HE videos. Scratch work (listings, crops, scripts) is in
`cache/prep-warehouse/` of this worktree (git-ignored).

**Owner rule (2026-11-16):** the History Edition (HE) is the visual reference, the GOG build is for
reading code, the game files of both editions are used. The original UI is the bit-identical base;
changes come later as options.

**Evidence letters** (as in `ui.md` and `fisher.md`): **D** = disassembled code (module + address);
**P** = parsed original file (path, section/ACCESS/id); **M-GOG** / **M-HE** = measured on a capture
(file, coordinates, method); **A** = arithmetic or derived from D/P/M; **U** = unknown (an OQ below).
Rectangles are screen pixels of the named capture. Sprite-sad = minimum mean absolute RGB error of
the decoded texture's opaque pixels (`ui.md` §2); GOG shots are RGB565 frames, so an exact sprite
gives about 3-7, a sprite under live text 15-45; HE video frames are lossy (about 15-30 for a match).

Short labels are quoted from `Texte.dat` (GOG = English, HE root = German); longer text is cited by
section and line index only.

## 0. Summary of findings

1. **The Kontor panel is the city panel** (`tStadtInfo`, View.dll factory id **0xa3a**, builder
   0x1006a790). Clicking the own Kontor (GOG) or the marketplace (HE) opens it. It has a name plank
   and a column of **eight tabs**; each tab shows one page object (D, M-GOG, M-HE).
2. **The buy/sell sliders are on tabs 3 and 4**, not on the first page: tab index 2 "Sell goods" /
   "Waren verkaufen" (page id 0xa3d) and tab index 3 "Buy goods" / "Waren einkaufen" (page id 0xa3e).
   The four OQ-547 clicks at y = 680 hit the first page's **"Deny building material" toggle**, which
   they switched on (`f15_kontor_btn_b`) and off again (`f15_kontor_btn_c`) (D, M-GOG). OQ-547 is
   therefore answered for *where* the sliders are; their pixel positions still need a capture
   (OQ-650).
3. **OQ-155 answered by code:** the "Sell goods" page sends and reads **list 1** (city slot +0x68(1) =
   the `VK` list at city +0xdc), the "Buy goods" page **list 0** (`EK`, +0x44). The sim's
   `GoodsSetting::buy` (true for list 1) therefore names the **sell** list (D).
4. A slider change sends **command 0x73** (`SetTradeSlotCmd`): `{island, city, goods, index,
   u16 amount (tons, upper slider "Amount remaining in storage"), u16 price (lower slider "Current
   price"), list}` through AnnoFrame command slot +0x64 = 0x1005d780 (D). This fixes O-14's field
   meaning: `@4` = amount in whole tons (the sim stores `<< 8`), `@6` = price.
5. The "Deny building material" toggle sends **command 0x7c mode 9** for goods **0x29 only** with
   value 0x7fff00 (on) or 0 (off), through command slot +0x94 = 0x1005ef70 (D).
6. All textures of this area are **pixel-identical in HE** (`fenster.tex` ids shifted +4 / +5 / +6,
   `fenster8` ids unshifted); HE English texts are byte-identical to GOG, HE German texts have the
   same line counts (P). **The HE videos show the plain set, recorded at 0.75:** `HE_UI_1.mp4` and
   `ui_5.mp4` are 1920 x 1080 recordings of a 2542 x 1351 client (video px = 0.75 screen px;
   `ui-ships-diplomacy.md` M-HE, `ui-options-mainmenu.md` capture list), and the HE draws the
   in-game UI at scale 1.0 at 1200 lines or fewer (`he-ui-scale.md` section 1). The GOG plain-set
   positions times 0.75 predict the measured tab, plank and deny button within 2 px (section 3).
   This prep first read them as the `8` set; that is retracted (orchestrator's review 2026-11-16).
   Confirming the 0.75 fit is OQ-652 (A, M-HE).

## 1. Screens and states, how they open and close

| # | Screen | Opened by | Closed by | Evidence |
|---|---|---|---|---|
| S1 | City panel, tab 0 "City status" (main page) | left click on the own Kontor (GOG (0.38, 0.47) on the Citizen map); left click on the own marketplace (HE `ui_5.mp4` 6:41.23, panel "Bad Aussee") | ESC or a miss-click: right panel shows `MAX_PIC` (id 5); another main HUD tab | M-GOG `f15_kontor`, `r2_hud_kontor`, `f15_ship_guess_0` (`ui.md` §9.1, §9.8); M-HE `ui_5.mp4` 6:41.23-6:49.32 |
| S2 | City panel, tab 1 "List of cities" / "Stadtliste" | click tab 1 | as S1 | D page id 0xa3c (builder 0x1006dac0); no capture (U) |
| S3 | City panel, tab 2 "Sell goods" / "Waren verkaufen" (VK, list 1) | click tab 2 | as S1 | D page id 0xa3d (builder 0x1001e950); no capture (U, OQ-650) |
| S4 | City panel, tab 3 "Buy goods" / "Waren einkaufen" (EK, list 0) | click tab 3 | as S1 | D page id 0xa3e (builder 0x10021380); no capture (U, OQ-650) |
| S5 | Goods chooser (four category tabs) over S3/S4 | "Add goods" button on S3/S4 (handler 0x10025790) | choosing a goods, or U | D chooser id 0xa0c is the city panel's child 9 (0x1006a7ac..0x1006a7dc); no capture (U, OQ-653) |
| S6-S9 | City panel tabs 4-7: "Raw materials" / "Rohstoffe", "Building material" / "Baumaterialien", "Consumer goods" / "Bedarfsgüter", "Weapons" / "Waffen": warehouse stock per goods | click the tab | as S1 | D page ids 0xa3f, 0xa40, 0xa41, 0xa42 (builders 0x1006cdc0, 0x1006b3f0, 0x1006ba90, 0x1006e2e0); no capture (U) |
| S10 | Foreign Kontor panel ("Einkauf"/"Verkauf" of another city) | U (likely a click on a Kontor not owned by the player) | U | D panel id 0xa36 (builder 0x10086f20) is created next to the city panel by the right-panel host (0x1003a8b9); OQ-658 |
| S11 | Ship trade pages (load/unload at an own Kontor, trade at a foreign Kontor) | ship panel sub-tabs, ship at a Kontor (U) | U | D ids 0xa51, 0xa53-0xa58, 0xa6e (section 2.7); HE sub-tabs seen, not opened at a Kontor (`HE_UI_1-analysis.md` B5) |
| S12 | Ship trade-route editor | ship panel "arrow" sub-tab | as ship panel | D ids 0xa5a (route list), 0xa5c (route actions); M-HE `HE_UI_1.mp4` 01:01.6 ("Neuer Eintrag" = `ROUTETEXTS` line 0) |

The tab switch is D: the tab group (gadget class 0xb02) calls 0x1006b070 → 0x1006af30, which reads the
selected index (property 0x1b3), stores it at panel +0x5f0, sets the chosen page visible and hides
the others (page vector at panel +0x5e4). No keyboard shortcut into these tabs was found (U).
Key **C** opens a different panel (`f15_city_list`: tab-1 sprite-sad 50, no match), not tab 1 (M-GOG).

## 2. Elements per screen

Texture ids are GOG `ToolGfx/fenster.tex` (plain set); HE ids in section 3. Sizes are the texture's
(P) and, where different, the gadget's (D: gadget slot +0x50 `(w, h)`). Gadget classes are View.dll
factory ids (factory 0x10019d60: `id - 0xa06` → byte table 0x1001c870 → jump table 0x1001c61c).
Property ids written through gadget slot +0x4c: 0x198 image ACCESS name, 0x197 frame count step,
0x19c index, 0x19d owner page, 0x1c4 text, 0x1c6 alignment, 0x1c9 tooltip line, 0x1a1 slider range
(D; meaning read from use, A).

**Gadget positions are not set by the View.dll builders** (no position call in 0x1001e950..0x10025a00
and 0x1006a790..0x1006f6a0 except two unrelated sites); AnnoFrame builders do call slot +0x5c with
`(x, y)` (e.g. 0x1001ef47). Where the View.dll positions come from is OQ-651. Positions below are
therefore **measured** (M) or derived from a measured anchor plus a code offset (A).

### 2.1 City panel shell (id 0xa3a, constructor 0x1006ae90, vtable 0x100a07d0, builder 0x1006a790)

| Element | Graphic | Rect (GOG 1024 x 768) | Text / data | Action | Evidence |
|---|---|---|---|---|---|
| Goods chooser (hidden) | class 0xa0c, child 9 | U | - | opened by "Add goods" | D 0x1006a7ac..0x1006a7dc |
| Page objects | ids 0xa3b..0xa42, children 0..7 | page origin (779, 243) (A: bk at (779, 300) minus code offset (0, 57)) | - | - | D loop 0x1006a886..0x1006a939 (`0xa3b + i`); M-GOG bk |
| Name plank | `PLANKEGROSS` id 1019, 241 x 48 (class 0xb17) | (783, 252) | - | - | D 0x1006aa5d..0x1006aab5; M-GOG `f15_kontor` sprite-sad 16.34 (text on it) |
| Crest at the plank's right | class 0xb17, gadget 42 x 55, image initially `GRNATIVESCHILDER` | about (975-1024, 245-300) | U | - | D 0x1006aa06..0x1006aa5b; no `GRNATIVESCHILDER`/`GRPLAYERSCHILDER`/`KLPLAYERSCHILDER` frame matches (best 43.9) → OQ-657 |
| City name | text gadget class 0xb1d, 241 x 39, property 0x1c4 = "StadtName", 0x1c6 = 2, font property 5 = 0x19 | on the plank, centred | `City::name` ("Plantau" on `f15_kontor`, "Bad Aussee" on HE 6:45) | - | D 0x1006aaca..0x1006abae; M-GOG, M-HE |
| Tab column | class 0xb02 container 42 x 368, child 3, click handler 0x1006b070 | (762, 303) 42 x 368 | - | switches page | D 0x1006abbf..0x1006ac8c |
| Tab *i* (i = 0..7) | class 0xb0d, 42 x 46, `VIEWINFOSTADTTAB` id 661: **normal frame 661 + 2i, selected 662 + 2i** | (762, 303 + 46 i) | tooltip `TOOLTIP:INFOSTADTTAB` line 2i (pairs: 0 "City status", 2 "List of cities", 4 "Sell goods", 6 "Buy goods", 8 "Raw materials", 10 "Building material", 12 "Consumer goods", 14 "Weapons") | select page i | D 0x1006acd6..0x1006adc0 (image id `+2` per tab at 0x1006adb6, tooltip index `+2` at 0x1006adb2); M-GOG `f15_kontor`: tab 0 selected 662 at (762, 303) mean 4.50; tabs 1-7 normal frames 663..675 at y 349..625, means 4.05-4.53; P `Texte.dat` |

### 2.2 Tab 0, "City status" page (id 0xa3b, ctor 0x1006ca70, vtable 0x100a0a70; draw 0x1006c750, builder 0x1006c7e0)

| Element | Graphic / gadget | Rect (GOG) | Text / data | Action | Evidence |
|---|---|---|---|---|---|
| Background | `STADT_BILANZ_BK` id 677, 245 x 468; holds the row plates and row icons | (779, 300) = page + (0, 57) | - | - | D 0x1006c761..0x1006c7d1 (`push 0x39; push 0`); M-GOG mean 20.52 (RGB565 + text); P (decoded: icons and plates are in the texture) |
| Rows 0..7 | 8 text gadgets class 0xb1c, 195 x 15, children 0..7, tooltip `TOOLTIP:INFOSTADT_STADTMAIN` line i | plates at about y 319, 349, 379, 409, 439, 469, 530, 629 (each about 209 x 22, x about 809); values right-aligned near x 1007 | row 0 "Inhabitants"/"Einwohner", 1 "Warenerträge" (GOG English line 1 reads "Sales revenue"), 2 "Operating costs"/"Betriebskosten", 3 "Purchase costs"/"Einkaufskosten", 4 "Sales revenue"/"Verkaufserlöse", 5 "Military costs"/"Militärkosten", 6 "Balance"/"Stadtbilanz" (red when negative), 7 "Knowledge in city"/"Wissen in der Stadt" | none | D loop 0x1006c810..0x1006c8e8 (`cmp esi, 7`); row y M-GOG approx (visual on 2x crop, ±2 px); M-GOG values 0, 0, 15, 0, 0, 0, -15, 0; M-HE `ui_5` 6:45 values 120, 91, 248, 0, 0, 0, -157, 9 |
| Balance rule | - | - | row 6 = row 1 - row 2 - row 3 + row 4 - row 5 | - | A: GOG 0 - 15 = -15; HE 91 - 248 = -157 (both captures; trade rows 0) |
| Deny building material | class 0xb05, 112 x 60, child 8, `VIEWINFOSTADTAUSBAUSTOP` id 678: **frame 678 = allowed, 679 = denied** (two tooltip lines set: 8/9 and 10/11 of `STADTMAIN`) | (858, 674) | state = city build-goods setting of goods 0x29 | toggles: sends 0x7c mode 9 (section 2.3) | D 0x1006c8fc..0x1006ca4e, handler 0x1006cb10 → 0x1006caa0; M-GOG `f15_kontor` 678 mean 3.30; `f15_kontor_btn_b` (click (870, 680)) 679 mean 3.30; `f15_kontor_btn_c` (click (920, 680)) back to 678 |

### 2.3 Commands sent from the city panel (D)

The page reaches the command sender by `[page+0x62c]` slot +0x98 (AnnoFrame object, vtable 0x100893ec,
installed at AnnoFrame 0x100608a1). Records are written with sender slot +0x144 (record, length).

| UI action | Sender slot | Record (bytes after the type byte) | Sim command | Evidence |
|---|---|---|---|---|
| Sell/Buy page: slider moved (handlers below), plus/minus clicked, goods deleted | +0x64 = 0x1005d780 `(list, rec, city)` | 0x73, u8 island, u8 city, u8 goods, u8 index, u16 amount (whole tons), u16 price, 2 bytes not written, u8 list (13 bytes) | `SetTradeSlotCmd{city, list, index, goods, amount_88 = amount << 8, price}` | D AnnoFrame 0x1005d79d..0x1005d7f2; View rec built at 0x10023e0b..0x10023e4c: `{goods, index, u16 trunc(upper slider), u16 trunc(lower slider), 0}` |
| Deny building material | +0x94 = 0x1005ef70 `(mode, goods, value, city)` | 0x7c, u8 island, u8 city, u16 mode = 9, u16 goods = 0x29, u32 value = 0x7fff00 (pressed) or 0 | `SetTradeFlagCmd{city, mode 9, goods 0x29, value}` | D View 0x1006cab8..0x1006caee (`neg; sbb; and 0x7fff00`), AnnoFrame 0x1005efa7 |
| (entry append) | +0xd0 = 0x1005f0b0 `(u32, city)` | 0x8a, u8 island, u8 city, u32 (9 bytes) | `SetTradeSlotCmd{index = kAppendIndex}` | D AnnoFrame 0x1005f0cd; no caller in the VKEK handlers found → OQ-653 |

After each send the page sets a 1 s timer (`GetTickCount + 1000` at page +0x570, e.g. 0x10023e5b) and
marks itself dirty (+0x649 = 1) (D); the meaning of the timer gate 0x10019400 is U.

### 2.4 Tabs 2 and 3, "Sell goods" / "Buy goods" (ids 0xa3d / 0xa3e)

The two pages are one layout with different handlers (`sum_sell.txt` / `sum_buy.txt` differ only in
handler addresses, D). Classes: vtable 0x1009c150 (sell, ctor case 0x1001b515) and 0x1009c08c (buy,
ctor case 0x1001b632); draw slot +0x54 = 0x10021290 (shared), builder slot +0x90 = 0x1001e950 (sell)
/ 0x10021380 (buy), update slots +0x9c/+0xa0 = 0x10020b80/0x100207b0 (sell), 0x10022f90/0x10022bc0
(buy).

| Child | Gadget | Size | Graphic | Tooltip (`TOOLTIP:INFOSTADT_WAREN_EIN-VERKAUF`) | Handler sell / buy | Evidence |
|---|---|---|---|---|---|---|
| bk | page background | 243 x 467 | `VKEK_HINTERGRUND` id 855 at page + (2, 57) → (781, 300) (A) | - | - | D 0x100212a1..0x10021311 |
| stock bar | drawn by the page | 150 x 3 | `LAGERMENGENBALKEN` id 860 | - | - | D 0x10021314..0x1002136b (position U) |
| 0 | text 0xb1c, default "00" (0x100a8284), alignment 1 | 150 x 20 | - | - | - | D 0x1001ea5f..0x1001eb14 |
| 3 | tooltip area 0xb15 | U | - | line 0 "Amount remaining in storage" / "Im Lager verbleibende Menge" | - | D 0x1001eb30..0x1001eb8e |
| upper slider | 0xb17 track + knob | track 158 x 25 | `SLIDER_MITTELTEILOBEN` 862, knob `SLIDER_KNUPPEL` 845 (20 x 39, 2 frames) | - | move 0x10023e90 / 0x10024f60; value formatter 0x10025110 | D 0x1001ebb8..0x1001ed1c; range property 0x1a1 = (0, 100.0f), later (0, capacity) (0x10023571 city slot +0x78 = capacity, 0x1002358c) |
| 1 | upper minus 0xb09 | 21 x 32 | `VK_SLIDER_MINUS` 863 (2 frames) | - | 0x10024040 / 0x10024de0 | D 0x1001ed38..0x1001ee46 |
| 2 | upper plus 0xb09 | 21 x 32 | `VK_SLIDER_PLUS` 865 (23 x 32, 2 frames) | - | 0x10023d10 / 0x10025180 | D 0x1001ee72..0x1001ef80 |
| 4 | text 0xb1c "00" | 130 x 20 | - | - | - | D 0x1001f049..0x1001f0fb |
| 7 | tooltip area 0xb15 | U | - | line 2 "Current price" / "Aktueller Preis" | - | D 0x1001f117..0x1001f17f |
| lower slider | 0xb17 track + knob | 158 x 25 | `SLIDER_MITTELTEILUNTEN` 861 + `SLIDER_KNUPPEL` | - | move 0x100241c0 / 0x10025300 | D 0x1001f19b..0x1001f2da; range (0, 150.0f) (0x43160000 at 0x1001f221); property 0x1a2 = 2 (upper: 0), meaning U (OQ-654) |
| 5 | lower minus 0xb09 | 26 x 37 | `SLIDER_MINUS` 848 | - | 0x10024040 / 0x10024de0 | D 0x1001f2f6..0x1001f404 |
| 6 | lower plus 0xb09 | 23 x 37 (texture 28 x 37) | `SLIDER_PLUS` 850 | - | 0x10023d10 / 0x10025180 | D 0x1001f430..0x1001f53e |
| goods grid | container 0xb03 | 216 x 162 (arrangement of the 50 x 50 cells U) | empty cell `VKEK_WARENFACH_LEER` 871 (50 x 50), marks `WARENVKEKBEZEICHNUNG` 833 (20 x 19, 2 frames), selection `WARENRAHMEN` 835 (50 x 50) | - | select 0x100243e0 / 0x100254b0 | D 0x1001f60b..0x1001f937 |
| goods cell | class 0xb0f | 50 x 50 | `WARENICONS_*` frame of the goods (View export `LinkTO_GetWarenGfxid`) | line 3 "%d0t %s0" | - | D 0x1001f7e0..0x1001f8f5; cell props 0x1bf = goods, 0x1bc = city slot +0x78 (capacity, +0x624) >> 8, 0x1c0 = city slot +0x7c(goods) >> 8 (likely the stock, A) (update 0x10020a05..0x10020a74) |
| 9 | "Add goods" 0xb09 | 45 x 54 | `VKEK_HINZUFUEGEN_VERWERFEN_NEW` 856 (4 frames) | line 4 "Add goods" / "Ware hinzufügen" | **0x10025790 (both)** | D 0x1001f96f..0x1001fa89 |
| 10 | "Delete goods" 0xb09 | 45 x 54 | same texture, next frames | line 5 "Delete goods" / "Ware löschen" | 0x100246c0 / 0x100258a0 | D 0x1001faa5..0x1001fb86 |

Data and rules read from the handlers (D unless marked):

- The page binds to the clicked building (+0x63c) and its city = building slot +0x18 (+0x640)
  (0x100203f3..0x10020407).
- **List per page:** sell handlers push 1, buy handlers push 0 to city slot +0x68 and to sender slot
  +0x64 (sell: 0x10023dc5 / 0x10023e48 in 0x10023d10, 0x100242a2 / 0x10024325 in 0x100241c0; buy:
  0x10024e95 / 0x10024f18 in 0x10024de0, 0x100250c3 in 0x10024f60). City slot +0x68 returns +0x44 for 0
  and +0xdc otherwise (`annowelt.md`, OQ-155). → Sell = list 1 = `VK`; Buy = list 0 = `EK`.
- **Visible slot count** = list slot +4 (0x10020445 → page +0x64c): the `HEAD2` slot counts, default
  4 (`VK`) and 6 (`EK`) (`sze.md` table, 0x26/0x27).
- **Index** sent = the position of the selected goods in the list (loop 0x10023dde..0x10023dfb).
- **Add goods** (0x10025790): finds the first cell whose goods (0x1bf) is 0; if none, nothing happens
  (+0x648 = 0xff). Else it selects that cell, gives the goods chooser (+0x650) the page and the city
  and shows it (+0x638 = 1). The command sent after a goods is chosen is not traced (OQ-653).
- **Delete goods** (0x100246c0): sends through sender slot +0x64 for the selected entry, then resets
  the cell (0x100247cb.. sets 0x198/0x1c0) (D; the record's goods value U, likely 0 = clear, A from the
  sim's "goods 0 clears an entry").
- Slider values are floats; the record takes `trunc` (CRT 0x1008c128) (D).

### 2.5 Goods chooser (id 0xa0c, vtable 0x1009d084, builder 0x10030b10)

Four category tabs `CHOOSEWAREN_TAB` id 836 (55 x 58, 8 frames = 4 tabs x 2; D string at
0x10030cd8); the chooser's tooltip section was not read (U). Category pages (factory id → constructor;
builder with its icon set): 0xa0d → 0x10032820 (builder 0x10032420, `WARENICONS_ROHSTOFF`), 0xa0e →
0x100317b0 (0x10031440, `WARENICONS_BAUMATERIAL`), 0xa0f → 0x10031fa0 (0x10031c20,
`WARENICONS_BEDARF`), 0xa10 → 0x100330f0 (0x10032d70, `WARENICONS_WAFFEN`) (D factory cases
0x1001a746..0x1001a815; constructor-to-builder pairing by address order, A). Layout, open/close: U
(OQ-653).

### 2.6 Tabs 4-7, warehouse stock by category (ids 0xa3f, 0xa40, 0xa41, 0xa42)

Each page: one grid container 0xb03 216 x 378, cells class 0xb10 50 x 50 with the category's
`WARENICONS_*` frames, tooltip `TOOLTIP:WARENICONS` ("%d0t %s0"), marks `WARENVKEKBEZEICHNUNG`; the
raw-materials page special-cases the goods names `STEINE` and `MAMORSTEIN` (0x1006cfe7, 0x1006d02a)
(D builder 0x1006cdc0; the others analogous by strings). Icon frame counts: raw 21, building 4,
consumer 14, weapons 8 (P). Which goods each page lists: OQ-659.

### 2.7 Neighbour panels (inventory only)

| Panel | Id | Builder | Graphics | Texts | Evidence |
|---|---|---|---|---|---|
| Tab 1 city list | 0xa3c | 0x1006dac0 | list 0xb22 210 x 392 on `DIPLISTE_UNTERGRUND`; up/down 86 x 25 `VIEWINFOSCHIFF_SCROLL_PFEILE` (handlers 0x1007b970, 0x1007b990) | `TOOLTIP:INFOSTADT_LISTEN` 0 "City list", 1 "Up", 2 "Down" | D |
| Foreign Kontor | 0xa36 | 0x10086f20 (+ 0x100867e0) | bk `FREMDERKONTOR_UNTERGRUND` id 1511 at page + (0, 56); plank, crest, name as 2.1; grids 216 x 108 and 216 x 162 of 50 x 50 cells (class 0xb18) | `KONTORFREMD` 0 "Purchase"/"Einkauf", 1 "Sales"/"Verkauf", 2 "%d0t of %s0 cost %d1", 3 "%d0t %s0 for other goods" | D (0x10086f9b `push 0x38; push 0`) |
| Ship: unload/load all | 0xa6e | 0x1004ab80 | - | `TOOLTIP:SCHIFF_HANDELALL` 0/1 "Unload goods", 2/3 "Load goods", 4/5 "Determine amount of load" | D |
| Ship: trade at a Kontor | 0xa51, 0xa53 | 0x1005e680 (uses `KONTORFREMD`), 0x10059af0 (`StadtName`) | `WARENRAHMEN`, `VKEK_WARENFACH_LEER` | `SCHIFF_HANDELALL` | D |
| Ship: goods categories | 0xa54 | 0x1005af10 | `CHOOSEWAREN_TAB` | `TOOLTIP:SCHIFF_HANDELEIGEN` (8 lines, 4 categories) | D |
| Ship: sale slots | 0xa55, 0xa57 | 0x1005cbb0, 0x1005c0d0 | `VERKAUFSFACH_LISTENBUTTON` 1012 (86 x 28) | - | D |
| Ship: other trade pages | 0xa56, 0xa58 | 0x1005b6c0, 0x1005d350 | `WARENICONS_*` | `TOOLTIP:WARENICONS` | D |
| Route list | 0xa5a | 0x1002a240 | `ROUTE_SCHTEENPLATTE` 1202, `ROUTEAKTIV` 1208, `ROUTE_SCROLL_PFEILE` 1204, `ROUTENPUNKT_SETZEN` 1214 | `TOOLTIP:SCHIFF_ROUTEMAIN` (14), `ROUTETEXTS` 0 "New entry", 1 "No warehouses" | D; M-HE `HE_UI_1` 01:01.6 |
| Route actions | 0xa5c | 0x10027f90 (draw 0x10027ef0 `ROUTE_HINTERGRUND`) | `VERLADEPFEILE` 1210, `SLIDER_MENGENPFEILE` 1212, `MENGENBALKEN` 1222, `ROUTENPUNKT_VERWERFEN` 1218, `ROUTENACTIONS_TAB` 1194 (0x10026600) | `TOOLTIP:SCHIFF_ROUTEACTION` (10), `TOOLTIP:SCHIFF_ROUTETAB` "Load I/II", "Unload I/II" | D; M-HE four load tiles at 01:01.6 |
| Ship "Kontor errichten" | 0xa6c / 0xa49 | 0x10047420 / 0x10063240 | `MAKEKONTORICON` 982 | - | D; M-HE `HE_UI_1` 01:29.6 |
| Diplomacy trade treaty | 0xa61 | 0x100846c0 | `DIPACTIONS_HANDEL` 1158 (70 x 78, 4 frames) | - | D |

The right-panel host creates the city panel 0xa3a as a child (0x1003a7d6) and the foreign Kontor panel
0xa36 as child 1 (0x1003a8b9); which selection shows which child is U (OQ-658).

## 3. HE vs GOG

| Item | GOG | HE | Evidence |
|---|---|---|---|
| `fenster.tex` ids of this area | 661 `VIEWINFOSTADTTAB`, 677, 678, 855, 856, 860-871, 1019, 1511, ... | +5 for ids up to `WARENICONS_SOLDAT` 808 (`VIEWINFOSTADTTAB` 666, `STADT_BILANZ_BK` 682, `VIEWINFOSTADTAUSBAUSTOP` 683), +4 from `WARENICONS_SCHIFF` (829) to 1438 (`VKEK_HINTERGRUND` 859, `PLANKEGROSS` 1023), +6 from 1517 (`FREMDERKONTOR_UNTERGRUND`) | P `anno-dump inc` of both `fenster.inc`; GOG 218 ACCESS / 1606 textures, HE 224 / 1612 |
| Pixels | - | every frame of the 66 ACCESS names checked is pixel-identical (hash) to GOG; exception: `WARENICONS_SOLDAT` has 22 frames in GOG, 21 in HE (the next HE name starts one earlier) | P `cache/prep-warehouse/scripts/acc.py` |
| `fenster8` (small set) ids | same as plain | **unshifted** (661, 677, 678, 1019 as GOG) | P HE `fenster8.inc` |
| Graphic set on screen | plain set at 1024 x 768 (tab 42 x 46) | plain set at UI scale 1.0, chrome at x = W - 245 (`he-ui-scale.md`); both owner videos are 0.75 recordings of a 2542 x 1351 client placed at video (7, 24). Prediction `x_video = 7 + 0.75 (x_ref + 1518)`, `y_video = 24 + 0.75 y_ref` against the positions measured in this prep (`ui_5.mp4` frame 405 s): tab 0 (1717.0, 251.3) vs (1716, 251) (the review's prediction: 1716.8, 251.5); plank (1732.8, 213.0) vs (1731, 212); deny button (1789.0, 529.5) vs (1787, 528); page bk (1729.8, 249.8) vs (1725, 245), a match of the smaller `8`-set sprite. The `8` set (0.78125 of the plain size) scored better than the **unscaled** plain set (tab 18.06 vs 43.7) because its size is close to 0.75 x plain; the 0.75-scaled plain set was not matched in this prep | A, M-HE; retracts this prep's "8 set at 1920 x 1080" (orchestrator's review 2026-11-16); OQ-652 |
| Layout | section 2 | the same positions (row above); no HE-specific layout found | A; OQ-652 |
| Texts | English `Texte.dat` | `Languages/Eng/Texte.dat` byte-identical sections (`KONTORFREMD`, `INFOSTADTTAB`, `STADTMAIN`, `WAREN_EIN-VERKAUF` hashes equal); root `Texte.dat` German, same line counts | P `anno-dump texte` |
| Code | 32-bit View.dll | x86-64 View.dll; contains the same ACCESS and section names (`VKEK_*`, `VK_SLIDER_*`, `TOOLTIP:INFOSTADT*`, `KONTORFREMD`, `FREMDERKONTOR_UNTERGRUND`, route sections) | P string scan; HE code not disassembled |
| Panel content | city status rows | same eight rows and deny button, name "Bad Aussee" | M-HE `ui_5` 6:41-6:49 |

## 4. What the sim must provide

State the panel reads (sim names from `src/sim`, app names from `src/app`):

| Panel item | Sim state today | App view today | Status |
|---|---|---|---|
| Which city a clicked Kontor / market belongs to | `World::find_building`, `Building` in `Island::cities[..].index`; `CityRef` | `CityView::warehouse` (first warehouse only, `opening_view.hpp`) | **missing in view**: building id → `CityRef` for any Kontor and market of the player; for S10 also for foreign cities |
| City name | `City::name` | `CityView::name` | exists |
| Crest at the plank | - | - | missing (source U, OQ-657) |
| Row 0 inhabitants | `World::city_population(CityRef)` | `CityCounters::population` (`play.hpp`) | exists |
| Rows 1-5 | `CityAccounts::sales`, `civil_cost`, `trade_purchases`, `trade_sales`, `military_cost` (`city_accounts.hpp`) | none | **missing in view**; which field and which period the original shows per row: OQ-655 |
| Row 6 balance | derived | - | A (section 2.2) |
| Row 7 knowledge | none (city +0x32c research sub-object, OQ-157) | none | **missing** |
| Deny toggle state | `City::build_goods[0x29 - build_goods_first].setting` (`BuildGoods`) | none | missing in view |
| Sell list / buy list | `City::trade_lists[1]` / `[0]` of `TradeEntry{goods, amount_88, price}` | none | missing in view; **`GoodsSetting::buy` names the sell list** (rename or document; OQ-155 answered) |
| Visible slots per list (4 / 6, file 0x26 / 0x27) | none (read, not kept, T26) | none | **missing**: `City` needs the two slot counts (planned "with the trade stage", OQ-155 note) |
| Goods stock and capacity | `CityStock::stock`, `CityStock::capacity` | `CityView::stock_88`, `capacity_88`; `CityCounters::goods` | exists |
| Goods per category page, icon per goods | `GoodsMetadata` (BauFirst/IdSec only) | - | **missing** (OQ-659) |
| Foreign city's trade lists and prices (S10) | `City::trade_lists` of any city; `City::player_mask` | only local cities | missing in view |

Commands the panel sends:

| UI action | Sim command | Status |
|---|---|---|
| Slider / plus / minus on Sell or Buy | `SetTradeSlotCmd{city, list (1 sell, 0 buy), index, goods, amount_88 = tons << 8, price}` (`command.hpp`, `apply.cpp set_trade_slot`) | exists; the original lowers an entry's amount to the capacity (city slot +0xbc, `annowelt.md`), the sim stores it unclamped: check |
| Delete goods | `SetTradeSlotCmd{..., goods = 0}` (clears) | exists (record goods value: A, OQ-653) |
| Add goods (after the chooser) | `SetTradeSlotCmd` at a free index, or append (`kAppendIndex`, original 0x8a) | exists; which one the original sends: OQ-653 |
| Goods-to-list link (`GoodsSetting`) | `SetTradeFlagCmd{mode 0/1, goods, value = index}` | exists; whether the UI or the 0x73 handler sets it: OQ-653 |
| Deny building material | `SetTradeFlagCmd{city, mode 9, goods 0x29, value 0x7fff00 or 0}` | exists (effect on upgrades: OQ-656) |
| Ship unload / load at own Kontor | `UnloadCmd`, `LoadCmd` (`ship-cargo.md`) | exists |
| Trade route edit / on-off | `AssignFreightRouteCmd`, `ControlFreightRouteCmd` (`freight_state.hpp`) | exists |
| Buy from / sell to a foreign Kontor by ship | - | **missing**: must carry ship, foreign warehouse `BuildingId`, goods, tons, direction; price and payer derived by the sim from the foreign city's list (OQ-658) |
| Trading by AI/free traders against the lists | - | **missing sim rule** (no trade stage): without it rows 3 and 4 stay 0 |

## 5. Evidence status per item

| Item | Status |
|---|---|
| City panel = Kontor click result; 8 tabs; tab frames 661+2i / 662+2i; tab rects | D + M-GOG (exact, mean ≤ 4.53) |
| Tab tooltips and page per tab | D + P |
| Plank rect | M-GOG (mean 16.3, text on it) |
| Crest graphic | U (OQ-657) |
| Main page bk (779, 300), rows, deny button (858, 674) frames 678/679 | D + M-GOG (button exact 3.30) |
| Main page row values' sim source and period | U (OQ-655) |
| Balance rule | A (two captures) |
| Sell = list 1 (VK), Buy = list 0 (EK) | D |
| 0x73 / 0x7c / 0x8a record layouts | D |
| Slider ranges (amount 0..100 then capacity; price 0..150) | D (property meaning A) |
| Sell/Buy page gadget sizes, textures, tooltips, handlers | D + P |
| Sell/Buy page positions | U except bk (A) (OQ-650, OQ-651) |
| Goods chooser, category pages, foreign Kontor, ship trade pages | D inventory; positions and flows U |
| HE texture identity and id shifts | P |
| HE graphic set (plain set at UI scale 1.0; the videos are 0.75 recordings) | A + M-HE (three positions within 2 px of the prediction); to confirm (OQ-652) |
| HE positions of the main page | M-HE (video, ±2 px) = GOG positions x 0.75 |

## 6. Open questions (OQ-650..OQ-659)

## OQ-650: Pixel layout of the Sell / Buy pages (tabs 2 and 3)
- **Status:** open.
- **Area:** UI / original.
- **Question:** Rectangles of the two sliders (track, knob travel), the four plus/minus buttons, the
  two value texts, the goods grid and its cells, the Add and Delete buttons, the stock bar; text
  format of the values; how an empty list and an empty cell look.
- **What we know:** sizes, textures, tooltips and handlers (D, section 2.4); the background
  `VKEK_HINTERGRUND` should sit at (781, 300) on GOG 1024 x 768 (A). No capture of either edition.
- **How to resolve:** HE recording R2 and R3 (packet), then sprite-sad of each texture on the frames.

## OQ-651: Where View.dll's gadget positions come from
- **Status:** open.
- **Area:** UI / original code.
- **Question:** The View.dll builders set size, image, tooltip and handler but no position; where is
  each child's (x, y) defined (slot index + a table, a parent layout pass, the gadget base class)?
- **What we know:** AnnoFrame builders set positions with gadget slot +0x5c `(x, y)` (0x1001ef47);
  View.dll builders add children with slot +0x24 `(gadget, index)` only; page backgrounds are placed by
  `[obj+8](x, y)` in the draw slot (0x1006c7cb, 0x1002130b).
- **How to resolve:** read the gadget base class (0x100188c0 constructor, 0x10018b90 message
  handler) and slot +0x24 of the panel class; check whether the index selects a rect table.

## OQ-652: Confirm that the HE videos show the plain set at 0.75
- **Status:** open, narrowed 2026-11-16 by the orchestrator's review (related: OQ-535).
- **Area:** UI / HE.
- **Question:** Do the owner's HE videos show the city panel in the plain graphic set at UI scale
  1.0, recorded at 0.75 (2542 x 1351 client in a 1920 x 1080 video)?
- **What we know:** 0.75 x the plain-set positions fits the tab, plank and deny button within 2 px
  (section 3); this prep's first reading ("the `8` set at 1920 x 1080") compared the 0.78125-size
  `8` set with the unscaled plain set and is retracted. `he-ui-scale.md`: UI scale 1.0 at 1200
  lines or fewer, chrome at x = W - 245.
- **How to resolve:** match the 0.75-scaled plain set on `ui_5.mp4` 405 s and `HE_UI_1.mp4` 91 s
  (expected: at least as good as the `8` set's means); or measure a lossless recording made at the
  common set-up (`docs/orchestration/tasks/ui-panels-he-recording.md`), where the plain set must
  match 1:1.

## OQ-653: Add-goods flow and the goods-to-list link
- **Status:** open.
- **Area:** UI / original code.
- **Question:** After "Add goods" and a choice in the chooser (id 0xa0c), which command is sent (0x73
  at the free index, or 0x8a append through sender slot +0xd0)? Does the UI send 0x7c mode 0/1, or does
  the 0x73 handler set the goods record itself? What goods value does Delete send?
- **What we know:** 0x10025790 opens the chooser for the first empty cell; the chooser callback region
  0x10023480..0x1002369b calls helper 0x100249f0 and city slot +0x78; no slot +0xd0 call was found in
  0x10021290..0x10025a00.
- **How to resolve:** read 0x100249f0 and the chooser's selection handler; read AnnoWelt's 0x73
  apply for the goods-setting write.

## OQ-654: Slider units and the property 0x1a2
- **Status:** open.
- **Area:** UI / original code.
- **Question:** What does property 0x1a2 (0 on the amount slider, 2 on the price slider) select
  (step, display as percent, gold)? Is the price slider's 0..150 a gold price per ton or a percentage
  of the base price (`Preis` in `BGruppen.dat`)? What do the two "00" texts print?
- **What we know:** ranges (0, 100.0) and (0, 150.0) (D); the amount range is later set from the
  capacity (D, likely); corpus prices 1..136 (`annowelt.md`).
- **How to resolve:** read the slider class (factory 0xb17 → 0x1000fa60) property handler and the
  formatter 0x10025110; HE recording R2 with narration of the numbers shown.

## OQ-655: The city-status rows' sources and period
- **Status:** open.
- **Area:** UI / simulation.
- **Question:** Which city field (and which period: per minute, running, last cycle) does each of rows
  1-5 show, and where does row 7 "knowledge" come from?
- **What we know:** `CityAccounts` holds the five money statistics (ECON-BOOK-1); HE 6:45 shows
  91 / 248 for a 120-inhabitant town (one sample). The page update functions (slots +0x9c / +0xa0 of
  vtable 0x100a0a70: 0x1006bfe0, 0x1006c070) were not read.
- **How to resolve:** read those update functions; HE recording R1 holding the panel open for 60 s.

## OQ-656: What "Deny building material" does to the sim
- **Status:** open.
- **Area:** simulation.
- **Question:** The toggle sends 0x7c mode 9 for goods 0x29 (tools) only, value 0x7fff00. Does the
  original's mode-9 handler (AnnoWelt 0x1002dbd0) apply it to the other build goods, and how does a
  reserve of 32 767 t on tools stop house upgrades (population-live.md section 5)?
- **What we know:** D section 2.3; the sim stores `build_goods[g - first].setting` for the one goods.
- **How to resolve:** read 0x1002dbd0's mode-9 branch and the upgrade check; HE recording R4.

## OQ-657: The crest at the right end of the city name plank
- **Status:** open.
- **Area:** UI / original.
- **Question:** Which texture is drawn in the 42 x 55 gadget at the plank's right (a house picture on
  `f15_kontor`), and what selects its frame (settlement stage, player)?
- **What we know:** the builder sets `GRNATIVESCHILDER` first (0x1006aa06); no frame of
  `GRNATIVESCHILDER`, `GRPLAYERSCHILDER` or `KLPLAYERSCHILDER` matches (best 43.9).
- **How to resolve:** read the city panel's update for property 0x198 on that gadget; search other
  `ToolGfx` files for the picture.

## OQ-658: Foreign Kontor panel and ship trade at a Kontor
- **Status:** open.
- **Area:** UI / original, simulation.
- **Question:** When is panel 0xa36 shown instead of the city panel? Which of its grids is
  "Purchase" and which "Sales"; what do the cells and the price texts show? Which ship pages (0xa51,
  0xa53-0xa58, 0xa6e) appear at an own vs a foreign Kontor, and which commands do they send?
- **What we know:** inventory in section 2.7 (D).
- **How to resolve:** HE recording R5 (another player's or a trader's Kontor, ship docked there);
  read the right-panel host's selection switch around 0x1003a7d6.

## OQ-659: Which goods each stock page lists
- **Status:** open.
- **Area:** UI / data.
- **Question:** The membership and order of goods on tabs 4-7 and in the chooser (raw 21, building 4,
  consumer 14, weapons 8 icon frames) and the goods → icon frame map (`LinkTO_GetWarenGfxid`,
  0x10088220 / 0x100881c0).
- **What we know:** D section 2.6; the raw page special-cases `STEINE`, `MAMORSTEIN`.
- **How to resolve:** read 0x10088220 and the category builders' loops; HE recording R3 (each tab
  held 3 s with a known stock).
