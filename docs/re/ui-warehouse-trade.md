# Warehouse (Kontor) and trade panels of the original

**Status:** research and specification only (UI-PANELS-PREP, 2026-11-16). No engine code, no game runs.
Read-only static analysis of GOG `View.dll` / `AnnoFrame.dll` (pefile + capstone, virtual addresses,
image base 0x10000000), parsed game files of both editions, and measurements on existing GOG Wine-runner
shots and the owner's HE videos. Scratch work (listings, crops, scripts) is in
`cache/prep-warehouse/` of this worktree (git-ignored). Documentation only, on `task/UI-PANELS-PREP`.
Paths under `cache/`, other worktrees' caches and `E:\1503_test_videos` are local evidence on the owner's PC and
are not in git; every D and P claim is reproducible from the cited address or file, the M claims from the named capture.

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
6. All textures of this area are **pixel-identical in HE** (`fenster.tex` ids shifted +5 / +4 / +6 by GOG id range, section 3,
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

**Already in main:** `data/ui/hud.txt` panel `info` (the "?" tab's city view) holds this panel's
page background `info_bk` (`STADT_BILANZ_BK` at (779, 301)), the eight tabs `info_subtab_0..7`
((762, 303 + 46 i), frame 662 on the selected tab 0, 661 + 2i on the others) and the deny button `info_build_stop`
((858, 674), frame 678), all sprite-565 on `r2_tab_help`; UI-TABS-1 switches its sub-tabs
(`docs/re/ui.md` section 5.4 on `task/UI-TABS-1`).

### 2.1 City panel shell (id 0xa3a, constructor 0x1006ae90, vtable 0x100a07d0, builder 0x1006a790)

| Element | Graphic | Rect (GOG 1024 x 768) | Text / data | Action | Evidence |
|---|---|---|---|---|---|
| Goods chooser (hidden) | class 0xa0c, child 9 | U | - | opened by "Add goods" | D 0x1006a7ac..0x1006a7dc |
| Page objects | ids 0xa3b..0xa42, children 0..7 | page origin (779, 244) (A: bk at (779, 301) minus code offset (0, 57)) | - | - | D loop 0x1006a886..0x1006a939 (`0xa3b + i`); M-GOG bk |
| Name plank | `PLANKEGROSS` id 1019, 241 x 48 (class 0xb17) | (783, 252) | - | - | D 0x1006aa5d..0x1006aab5; M-GOG `f15_kontor` sprite-sad 16.34 (text on it) |
| Crest at the plank's right | class 0xb17, gadget 42 x 55, image initially `GRNATIVESCHILDER` | about (975-1024, 245-300) | U | - | D 0x1006aa06..0x1006aa5b; no `GRNATIVESCHILDER`/`GRPLAYERSCHILDER`/`KLPLAYERSCHILDER` frame matches (best 43.9) → OQ-657 |
| City name | text gadget class 0xb1d, 241 x 39, property 0x1c4 = "StadtName", 0x1c6 = 2, font property 5 = 0x19 | on the plank, centred | `City::name` ("Plantau" on `f15_kontor`, "Bad Aussee" on HE 6:45) | - | D 0x1006aaca..0x1006abae; M-GOG, M-HE |
| Tab column | class 0xb02 container 42 x 368, child 3, click handler 0x1006b070 | (762, 303) 42 x 368 | - | switches page | D 0x1006abbf..0x1006ac8c |
| Tab *i* (i = 0..7) | class 0xb0d, 42 x 46, `VIEWINFOSTADTTAB` id 661: **normal frame 661 + 2i, selected 662 + 2i** | (762, 303 + 46 i) | tooltip `TOOLTIP:INFOSTADTTAB` line 2i (pairs: 0 "City status", 2 "List of cities", 4 "Sell goods", 6 "Buy goods", 8 "Raw materials", 10 "Building material", 12 "Consumer goods", 14 "Weapons") | select page i | D 0x1006acd6..0x1006adc0 (image id `+2` per tab at 0x1006adb6, tooltip index `+2` at 0x1006adb2); M-GOG `f15_kontor`: tab 0 selected 662 at (762, 303) mean 4.50; tabs 1-7 normal frames 663..675 at y 349..625, means 4.05-4.53; P `Texte.dat` |

### 2.2 Tab 0, "City status" page (id 0xa3b, ctor 0x1006ca70, vtable 0x100a0a70; draw 0x1006c750, builder 0x1006c7e0)

| Element | Graphic / gadget | Rect (GOG) | Text / data | Action | Evidence |
|---|---|---|---|---|---|
| Background | `STADT_BILANZ_BK` id 677, 245 x 468; holds the row plates and row icons | (779, 301) = page + (0, 57) | - | - | D 0x1006c761..0x1006c7d1 (`push 0x39; push 0` at 0x1006c7cb); M-GOG `f15_kontor` mean 11.1 at (779, 301) against 20.1 at (779, 300) (orchestrator's review 2026-11-16; this prep had measured 20.52 at y 300); main's `data/ui/hud.txt` `info_bk` at (779, 301), sprite-565 on `r2_tab_help`; P (decoded: icons and plates are in the texture) |
| Rows 0..7 | 8 text gadgets class 0xb1c, 195 x 15, children 0..7, tooltip `TOOLTIP:INFOSTADT_STADTMAIN` line i | plates at about y 319, 349, 379, 409, 439, 469, 530, 629 (each about 209 x 22, x about 809); values right-aligned near x 1007 | row 0 "Inhabitants"/"Einwohner", 1 "Warenerträge" (GOG English line 1 reads "Sales revenue"), 2 "Operating costs"/"Betriebskosten", 3 "Purchase costs"/"Einkaufskosten", 4 "Sales revenue"/"Verkaufserlöse", 5 "Military costs"/"Militärkosten", 6 "Balance"/"Stadtbilanz" (red when negative), 7 "Knowledge in city"/"Wissen in der Stadt" | none | D loop 0x1006c810..0x1006c8e8 (`cmp esi, 7`); row y M-GOG approx (visual on 2x crop, ±2 px); M-GOG values 0, 0, 15, 0, 0, 0, -15, 0; M-HE `ui_5` 6:45 values 120, 91, 248, 0, 0, 0, -157, 9 |
| Balance rule | - | - | shown only for row 6 = row 1 - row 2 while rows 3-5 are 0 | - | A: GOG 0 - 15 = -15; HE 91 - 248 = -157. Both captures have rows 3, 4 and 5 at zero, so the signs of purchase costs, sales revenue and military costs are not shown by them: OQ-655 (this row first stated a full rule with those signs; over-claimed, orchestrator's review 2026-11-16) |
| Deny building material | class 0xb05, 112 x 60, child 8, `VIEWINFOSTADTAUSBAUSTOP` id 678: **frame 678 = allowed, 679 = denied** (two tooltip lines set: 8/9 and 10/11 of `STADTMAIN`) | (858, 674) | state = city build-goods setting of goods 0x29 | toggles: sends 0x7c mode 9 (section 2.3) | D 0x1006c8fc..0x1006ca4e, handler 0x1006cb10 → 0x1006caa0; M-GOG `f15_kontor` 678 mean 3.30; `f15_kontor_btn_b` (click (870, 680)) 679 mean 3.30; `f15_kontor_btn_c` (click (920, 680)) back to 678 |

### 2.3 Commands sent from the city panel (D)

The page reaches the command sender by `[page+0x62c]` slot +0x98 (AnnoFrame object, vtable 0x100893ec,
installed at AnnoFrame 0x100608a1). Records are written with sender slot +0x144 (record, length).
The vtable's RTTI name is `tNetClientNet` (P, `.?AVtNetClientNet@@` through the locator at
0x100893ec - 4): the three packers below are the **multiplayer client's**; single player applies
the same action directly through `tNetClient` (OQ-005, `AnnoFrame.dll` 0x100589a0). The byte
layouts are those packers' writes (offsets from the type byte at 0).

| UI action | Sender slot (`tNetClientNet`) | Record | Sim command | Evidence |
|---|---|---|---|---|
| Sell/Buy page: slider moved (handlers below), plus/minus clicked, goods deleted | +0x64 = 0x1005d780 `(list, rec, city)` | 13 bytes: 0 = 0x73, 1 = u8 island, 2 = u8 city, 3 = u8 goods, 4 = u8 index, 5 = u16 amount (whole tons), 7 = u16 price, 9-10 not written, 11 = u8 list, 12 not written | `SetTradeSlotCmd{city, list, index, goods, amount_88 = amount << 8, price}` | D AnnoFrame 0x1005d780..0x1005d7f2 (list `[esp+0x17]`, goods `[esp+0xf]`, amount `[esp+0x11]` with the record at `[esp+0xc]`; index, city and price after the two pushes at `[esp+0x14]`, `[esp+0x16]`, `[esp+0x1b]`; length `push 0xd`); View rec built at 0x10023e0b..0x10023e4c: `{goods, index, u16 trunc(upper slider), u16 trunc(lower slider), 0}` |
| Deny building material | +0x94 = 0x1005ef70 `(mode, goods, value, city)` | 13 bytes: 0 = 0x7c, 1 = u8 island, 2 = u8 city, 3 = u16 mode (9), 5 = u16 goods (0x29), 7-8 not written, 9 = u32 value (0x7fff00 pressed, or 0) | `SetTradeFlagCmd{city, mode 9, goods 0x29, value}` | D View 0x1006cab8..0x1006caee (`neg; sbb; and 0x7fff00`); AnnoFrame 0x1005ef70..0x1005efd6 (mode `[esp+0xf]`, goods `[esp+0x11]`, value `[esp+0x15]` with the record at `[esp+0xc]`, length `push 0xd`) |
| (entry append) | +0xd0 = 0x1005f0b0 `(u32, city)` | 9 bytes: 0 = 0x8a, 1 = u8 island, 2 = u8 city, 3-4 not written, 5 = u32 | `SetTradeSlotCmd{index = kAppendIndex}` | D AnnoFrame 0x1005f0b0..0x1005f0fc (u32 at `[esp+0x11]` with the record at `[esp+0xc]`, length `push 9`); no caller in the VKEK handlers found → OQ-653 |

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
| bk | page background | 243 x 467 | `VKEK_HINTERGRUND` id 855 at page + (2, 57) → (781, 301) (A) | - | - | D 0x100212a1..0x10021311 |
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
| `fenster.tex` ids of this area | 661 `VIEWINFOSTADTTAB`, 677, 678, 855, 856, 860-871, 1019, 1511, ... | by GOG id: **+5 for 6..823** (`VIEWINFOSTADTTAB` 661 → 666, `STADT_BILANZ_BK` 677 → 682, `VIEWINFOSTADTAUSBAUSTOP` 678 → 683); GOG 824, the 22nd `WARENICONS_SOLDAT` frame (803..824), has no HE frame (HE 808..828); **+4 for 825..1487** (`WARENICONS_SCHIFF` 825 → 829, `VKEK_HINTERGRUND` 855 → 859, `PLANKEGROSS` 1019 → 1023); **+6 from 1488** (`OPTION_HELPPLAYER` 1488 → 1494, `FREMDERKONTOR_UNTERGRUND` 1511 → 1517); as `ui-ships-diplomacy.md` §2 and `ui-options-mainmenu.md` §3.2 have it (this row first gave HE ids as the range ends; corrected by the orchestrator's review 2026-11-16) | P `anno-dump inc` of both `fenster.inc` (re-run with `tools/tex.py inc`); GOG 218 ACCESS / 1606 textures, HE 224 / 1612 |
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
| City name | `City::name` (`city.hpp`: drawn from the world's `STAEDTE` list when a city is founded in play; the app loads the list, `opening.cpp`); app `CityView::name` | exists for cities founded in play; empty for cities imported from scenario files (not read from files yet) |
| Crest at the plank | - | - | missing (source U, OQ-657) |
| Row 0 inhabitants | `World::city_population(CityRef)` | `CityCounters::population` (`play.hpp`) | exists |
| Rows 1-5 | `CityAccounts::sales`, `civil_cost`, `trade_purchases`, `trade_sales`, `military_cost` (`city_accounts.hpp`) | none | **missing in view**; which field and which period the original shows per row: OQ-655 |
| Row 6 balance | derived | - | A for row 1 - row 2 only (section 2.2); the terms of rows 3-5: OQ-655 |
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
| Main page bk (779, 301), rows, deny button (858, 674) frames 678/679 | D + M-GOG (button exact 3.30; bk mean 11.1); already in main's `hud.txt` panel `info` |
| Main page row values' sim source and period | U (OQ-655) |
| Balance rule | A, row 6 = row 1 - row 2 only (two captures with rows 3-5 at 0); rows 3-5 U (OQ-655) |
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

The full entries (status, area, question, what is known, how to resolve) are in the register,
[`docs/open-questions.md`](../open-questions.md), under these numbers:

- **OQ-650:** Pixel layout of the Sell / Buy pages (tabs 2 and 3)
- **OQ-651:** Where View.dll's gadget positions come from
- **OQ-652:** Confirm that the HE videos show the plain set at 0.75
- **OQ-653:** Add-goods flow and the goods-to-list link
- **OQ-654:** Slider units and the property 0x1a2
- **OQ-655:** The city-status rows' sources and period
- **OQ-656:** What "Deny building material" does to the sim
- **OQ-657:** The crest at the right end of the city name plank
- **OQ-658:** Foreign Kontor panel and ship trade at a Kontor
- **OQ-659:** Which goods each stock page lists
