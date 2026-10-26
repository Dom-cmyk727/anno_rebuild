# The twelve endless presets (T36)

The original's endless lane is twelve scenario files, nine in
`Scenes/0010SingleEndlos` and three in `Scenes/0011SingleEndlosAddon`. This
page records what those files hold. It copies no sentence from a description
page and no byte from a scenario.

## How each value was read

`anno-dump sze` (the worktree build of the same sources, and the main build at
the same commit) prints the edition, the `SCENE` child count, the `SPIELER`
count and the `INSELLNK` count. It does not print world size, `HEAD` fields,
party kind, money, `LIMITS`, climates or goods. Those come from a field read
of the same files, with the layouts already specified:

- Root chunk `ANNOADDON1` on all twelve, so the scene is the addon edition.
  `read_head` then sets `prop14 = 3`. That is not a `HEAD` byte
  (`scenario_formats.cpp`).
- `SCENE/HEAD` is 32 bytes (`docs/formats/sze.md`, `settings.hpp`). `prop0` is
  the world kind (0 = endless). `prop1` is the byte at `0x0A` (the reader
  stores 1 when the byte is 0; here the byte is 1). `prop2` is the byte at
  `0x09` (O-29: difficulty-like, not equated to the page's easy / medium /
  heavy words). `prop3` is the u16 at `0x0C` (2550 = no feature on every
  file). `prop6` is the u16 at `0x10`: bit `0x02` volcano off, `0x04` drought
  off, `0x08` city events off, `0x80` the 7.5 min event off; bit `0x20` is
  CompLogic-only. `prop10` is the byte at `0x0B` (tax 0..2). Bytes `0x0E`,
  `0x0F` and `0x12..31` are 0 on every file. `prop5`, `prop7_offset`, `prop9`
  and `prop11`..`prop13` are not in `HEAD`.
- `LIMITS` is two u16: ship cap, soldier cap (`sze.md`).
- A party is a `SPIELER` block whose records include `HEAD` (108 bytes). Kind
  is the u16 at `HEAD+0x00`, money the u32 at `HEAD+0x04` (`sze.md`: likely
  the starting money), colour the byte at `HEAD+0x4C` (1..8 a colour, `0xFF`
  assigned at start, 0 none). Kinds (`docs/re/annoscene.md` §6): 8 and 11
  human, 12 computer, 14 natives, 15 pirates, 16 Venetians, 17 god or gang.
  Index 31 kind 2 is the neutral party (likely, OQ-027 b). The other
  `SPIELER` blocks are the state series (no `HEAD`). `anno-dump`'s player
  count is both series, so it is twice the header-party count on every file.
- `INSELLNK`: u16 x, y, width, height and a u32 island number, then a
  NUL-terminated path (`sze.md`). Climate is the path prefix after the last
  backslash (`annoscene.md` §3): `po_` polar, `t_` tundra, `n_` nord, `pr_`
  praerie, `d_` dschungel, `s_` steppe, `w_` wueste. No `w_` occurs.
- `INSEL5`: the same 12-byte head, then chunk records. Climate is `INFO[0]`
  with the names for 0..5 (`docs/formats/insel.md`). Its area is not added to
  the link area.
- Area share is integer. Basis points are `area * 10000 / world_area`,
  truncated. Pairwise rectangle overlap is 0 on every file, the union equals
  the sum, and no rectangle puts a tile outside the world. Island numbers are
  unique across that file's links and `INSEL5` blocks.
- `AUFTRAGTEXT` is the NUL-terminated description file name (`sze.md`). The
  match is that name, in the same scene directory. Two pages carry a stale
  title; the match does not use the title.
- `WARETYPE` is the same 56 entries on all twelve (sha256 prefix
  `c28dc27a28c4`). City stock is `STADT2` / `WARE`: u16 goods number, u16
  tons, four unread bytes. The tons below are that u16, not shifted. Ship and
  soldier `WAREN` occur (payload sizes 24, 32 or 64) and stay unread
  (OQ-027). No separate human start-goods record was found.
- A pirate's state-series `COMP` is `TYP=<n>` (`annoscene.md` §6, column 0,
  range 0..7). The number is recorded. It is not the page's three pirate
  words (OQ-222).

The description levels are the bullet values on the nine base pages, not the
paragraph above them. `800 med 128_91008.htm` is the one page whose paragraph
uses a stronger money wording than its bullet; the table uses the bullet.
`1024 med 128_91005.htm` is the one page whose paragraph says average-sized
while the bullet says normal-sized; the table uses the bullet.

Party cells use H human, C computer, V Venetians, P pirates, N natives, G god
or gang, X the index-31 neutral. `c` is the colour byte. A name is "empty" or
"set"; the string is not copied. Money is the header `HEAD+0x04` value.

## The twelve rows

| Preset | Match | HEAD that varies | Parties | Limits | Islands | Levels | Money and goods | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `00_1024_easyentry_01.sze` | `00_1024_easyentry_01.htm`. The page title names Endlos01 and `1024 easy 256_91002`; the body names this file. | 1024×1024; prop2=0; prop6=38 (volcano, drought, bit 0x20); prop10=2 | H11 500000 c5 name empty; C12 ×2 10000 c2 and c3 names empty; V16 10000 c0 name set; X 10000 c0 name empty. No pirate, no natives. | ships 40, soldiers 100 | 16 links: 64×64 ×1, 80×80 ×5, 128×128 ×10. Climates polar 3, tundra 3, nord 2, praerie 3, dschungel 3, steppe 2. Area 199936/1048576 (1906 bp). 4 INSEL5, not in that share: 256×256 ×3, 128×128 ×1; dschungel 1, nord 2, steppe 1; area 212992 (2031 bp). | islands many; size large; ores numerous large; fertility very high; pirates none; money more than enough. Extra bullet, this file only: peaceful computer players. | Human header money 500000. City stock, player 0 (the human): NAHRUNG 50, WERKZEUG 50, HOLZ 50. WAREN unread: one 64-byte, two 32-byte. | This file. Method above. anno-dump: 16 links, 10 SPIELER. |
| `01_1024_easy_256.sze` | `1024 easy 256_91002.htm` (title Endlos01). | 1024×1024; prop2=0; prop6=38; prop10=2 | H11 50000 c255 name empty; C12 ×3 40000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=2; X 10000 name set. | 40 / 100 | 27 links: 256×256 ×4, 128×128 ×8, 80×80 ×10, 64×64 ×5. dschungel 7, nord 5, polar 3, tundra 3, praerie 4, steppe 5. Area 477696/1048576 (4555 bp). 1 INSEL5: 80×80 nord, area 6400 (61 bp). | many; large; large ore deposits; very high; pirates not very aggressive; lots of money. | Human 50000. City stock is player 5 (the Venetian), not the human: NAHRUNG 50, LEDER 50, STOFFE 50. WAREN unread: four 32-byte. | This file. anno-dump: 27 links, 14 SPIELER. |
| `02_1024_easy_128.sze` | `1024 easy 128_91001.htm` (title Endlos02). | 1024×1024; prop2=0; prop6=38; prop10=2 | H11 45000 c255 name empty; C12 ×3 40000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=2; X 10000 name set. | 40 / 100 | 28 links: 128×128 ×13, 80×80 ×13, 64×64 ×2. nord 5, dschungel 7, steppe 6, praerie 5, polar 2, tundra 3. Area 304384/1048576 (2902 bp). 1 INSEL5: 80×80 nord, area 6400 (61 bp). | many; normal-sized; large ore deposits; very high; pirates not very aggressive; lots of money. | Human 45000. City stock player 5 (Venetian): NAHRUNG 50, LEDER 30. WAREN unread: four 32-byte. | This file. anno-dump: 28 links, 14 SPIELER. |
| `03_1024_med_256.sze` | `1024 med 256_91006.htm` (title Endlos03). | 1024×1024; prop2=1; prop6=32 (bit 0x20 only); prop10=1 | H11 30000 c255 name empty; C12 ×4 20000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=3; X 10000 name set. | 40 / 100 | 27 links, same size histogram and same climate histogram as `01`. Area 477696/1048576 (4555 bp). No INSEL5. | many; large; sufficient number of ore deposits; high; aggressive pirates; sufficient amount of money. | Human 30000. No city WARE. WAREN unread: five 32-byte. | This file. anno-dump: 27 links, 16 SPIELER. |
| `04_1024_med_128.sze` | `1024 med 128_91005.htm`. The page title still says `1024 easy 128_91005`. | 1024×1024; prop2=2; prop6=32; prop10=1 | H11 30000 c255 name empty; C12 ×4 20000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=4; X 10000 name set. | 40 / 100 | 28 links, same size histogram and same climate histogram as `02`. Area 304384/1048576 (2902 bp). No INSEL5. | many; normal-sized; sufficient number of ore deposits; high; aggressive pirates; sufficient amount of money. | Human 30000. No city WARE. WAREN unread: five 32-byte. | This file. anno-dump: 28 links, 16 SPIELER. |
| `05_800_Med_128.sze` | `800 med 128_91008.htm` (title Endlos05). | 800×800; prop2=2; prop6=0; prop10=1 | H11 30000 c255 name empty; C12 ×3 20000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=4; X 10000 name set. | 40 / 100 | 23 links: 128×128 ×10, 80×80 ×11, 64×64 ×2. nord 4, polar 2, tundra 2, praerie 5, steppe 5, dschungel 5. Area 242432/640000 (3788 bp). No INSEL5. | few; normal-sized; sufficient number of ore deposits; high; aggressive pirates; sufficient amount of money. | Human 30000. No city WARE. WAREN unread: four 32-byte. | This file. anno-dump: 23 links, 14 SPIELER. |
| `06_1024_heavy_256.sze` | `1024 heavy 256_91004.htm` (title Endlos06). | 1024×1024; prop2=2; prop6=0; prop10=0 | H11 25000 c255 name empty; C12 ×4 15000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=5; X 10000 name set. | 40 / 100 | 27 links, same size histogram and same climate histogram as `01`. Area 477696/1048576 (4555 bp). No INSEL5. | average number; large; moderate number of ore deposits; varied; aggressive pirates; little money. | Human 25000. No city WARE. WAREN unread: five 32-byte. | This file. anno-dump: 27 links, 16 SPIELER. |
| `07_1024_heavy_128.sze` | `1024 heavy 128_91003.htm` (title Endlos07). | 1024×1024; prop2=3; prop6=0; prop10=0 | H11 20000 c255 name empty; C12 ×4 15000 c255 names set; V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=6; X 10000 name set. | 40 / 100 | 28 links, same size histogram and same climate histogram as `02`. Area 304384/1048576 (2902 bp). No INSEL5. | average number; normal-sized; moderate number of ore deposits; varied; aggressive pirates; little money. | Human 20000. No city WARE. WAREN unread: five 32-byte. | This file. anno-dump: 28 links, 16 SPIELER. |
| `08_800_heavy_128.sze` | `800 heavy 128_91007.htm` (title Endlos08). | 800×800; prop2=3; prop6=0; prop10=0 | H11 20000 c255 name empty; C12 ×3 20000 c255 names set (the computers have the same header money as the human); V16 10000 c0 name set; P15 10000 c0 name set, COMP TYP=5; X 10000 name set. | 40 / 100 | 25 links: 128×128 ×4, 80×80 ×13, 64×64 ×8. nord 5, dschungel 6, polar 2, tundra 2, praerie 5, steppe 5. Area 181504/640000 (2836 bp). No INSEL5. | few; normal-sized; moderate number of ore deposits; varied; aggressive pirates; little money. | Human 20000. No city WARE. WAREN unread: four 32-byte. | This file. anno-dump: 25 links, 14 SPIELER. |
| `endless01.sze` | `AUFTRAGText_Endless_01.htm`. The page names that file. | 800×800; prop2=0; prop6=134 (volcano, drought, 7.5 min event); prop10=2 | H8 10000000 c255 name empty; N14 ×5 10000 c0 names set; V16 10000 c0 name empty; X 10000 c0 name empty. No computer, no pirate. | ships 50, soldiers 400 | 0 links. 23 INSEL5: 256×256 ×2, 128×128 ×11, 80×80 ×10. dschungel 2, nord 4, praerie 5, steppe 7, tundra 3, polar 2. Area 375296/640000 (5864 bp). | not stated (OQ-220). The page does not use the six bullets. | Human 10000000. City stock on natives 1..5, tons: p1 WOLLE 50, NAHRUNG 50, WERKZEUG 20, HOLZ 20, ZIEGEL 20; p2 EDELSTEINE 90, SEIDE 90, NAHRUNG 88, WERKZEUG 42, HOLZ 46, ZIEGEL 10; p3 GOLD 39, TABAK 60, NAHRUNG 59, GEWUERZE 30, SCHMUCK 60, WERKZEUG 35, HOLZ 28, ZIEGEL 8; p4 NAHRUNG 30, LAMPENOEL 10, WERKZEUG 32, HOLZ 16, ZIEGEL 30; p5 EDELSTEINE 50, NAHRUNG 30, GEWUERZE 30, WERKZEUG 50, HOLZ 38, ZIEGEL 52. WAREN unread: one 64-byte. | This file. anno-dump: 0 links, 16 SPIELER. |
| `endless02.sze` | `AUFTRAGText_Endless_02.htm`. The page names that file. | 1024×1024; prop2=2; prop6=0; prop10=2 | H8 300000 c255 name empty; N14 ×5 with header money 11197, 11181, 10869, 10999, 12837, all c0 names set; P15 20000 c0 name set, COMP TYP=7; V16 7779 c0 name set; C12 100000 c8 name set; X 10000 c0 name empty. | 50 / 400 | 0 links. 30 INSEL5: 400×400 ×1, 256×256 ×3, 128×128 ×23, 80×80 ×2, 64×64 ×1. nord 3, praerie 10, polar 2, tundra 4, steppe 10, dschungel 1. Area 750336/1048576 (7155 bp). | not stated (OQ-220). | Human 300000. City stock (tons) on p1 NAHRUNG 17, HEILKRAEUTER 55, HOLZ 18, ZIEGEL 12; p2 (pirate) NAHRUNG 69, ALKOHOL 70, WERKZEUG 50, HOLZ 50, ZIEGEL 20; p3 WOLLE 21, NAHRUNG 33, STOFFE 43, HOLZ 18, ZIEGEL 12; p4 EISEN 42, NAHRUNG 39, PELZE 20, HOLZ 16, ZIEGEL 12; p5 WALSPECK 42, NAHRUNG 26, LAMPENOEL 48, HOLZ 18, ZIEGEL 12; p6 NAHRUNG 54, GEWUERZE 34, HOLZ 12, ZIEGEL 7; p8 (computer) EISENERZ 50, GOLD 50, SALZSTEIN 50, WOLLE 20, HOPFEN 20, TIERHAEUTE 20, HOLZKOHLE 190, NAHRUNG 50, LEDER 20, SALZ 20, ALKOHOL 20, STOFFE 20, WERKZEUG 190, HOLZ 190, ZIEGEL 190, MAMOR 50, SCHWERT 20, BOGEN 20, ARMBRUST 20, MUSKETE 20, RUESTUNG 50, LANZE 20, AXT 20, KANONESHIP 10. WAREN unread: two 64-byte, three 32-byte, one 24-byte. | This file. anno-dump: 0 links, 20 SPIELER. |
| `endless03.sze` | `AUFTRAGText_Endless_03.htm`. The page's heading is Expertenmaske, not this file name. | 800×800; prop2=3; prop6=0; prop10=0 | H8 23000 c255 name empty; C12 ×4 50000 c255 names set (indexes 1, 2, 3, 10); P15 ×2 10000 c0 names set, COMP TYP=2 (index 4) and TYP=6 (index 24); G17 10000 c255 name empty (index 5); N14 ×5 10000 c0 names set; V16 10000 c0 name empty (index 30); X 10000 c0 name empty. | ships 30, soldiers 150 | 0 links. 12 INSEL5: 256×256 ×2, 128×128 ×6, 80×80 ×2, 64×64 ×2. dschungel 3, polar 1, tundra 1, nord 3, praerie 2, steppe 2. Area 250368/640000 (3912 bp). | not stated (OQ-220). | Human 23000. City stock: p4 (pirate) NAHRUNG 50, ALKOHOL 50, WERKZEUG 50, HOLZ 50, ZIEGEL 50; p18 (native) NAHRUNG 50, ALKOHOL 20, WERKZEUG 50, HOLZ 50, ZIEGEL 50; p24 (pirate) NAHRUNG 100, ALKOHOL 100, WERKZEUG 50, HOLZ 50, ZIEGEL 50. WAREN unread: five 32-byte. | This file. anno-dump: 0 links, 30 SPIELER. |

Common to every row, so not repeated: edition `ANNOADDON1` (`prop14 = 3`), `prop0 = 0`, `prop1 = 1`, `prop3 = 2550`, `HEAD` length 32, the unread `HEAD` tail bytes 0, no overlapping island rectangle, no tile outside the world, island numbers unique in the file.

## What this does not decide

- The three addon pages do not state the six levels (OQ-220).
- A money bullet is not one human `HEAD+0x04` amount. "Lots" is 50000 (`01`) and 45000 (`02`). "Little" is 25000 (`06`) and 20000 (`07`, `08`). "Sufficient" is 30000 (`03`, `04`, `05`). "More than enough" is 500000 (`00` only). The addon human amounts 10000000, 300000 and 23000 are outside those four bullets, and those pages do not state money (OQ-221).
- Pirate `COMP TYP` on the base files is 2 for both "not very aggressive" pages and 3, 4, 5 or 6 for pages that all say "aggressive". `prop6` is 38 on the no-pirate page and on both "not very aggressive" pages, so it does not encode the three words (OQ-222).
- `prop2` is not a clean easy / medium / heavy scale (O-29): the medium 128 and the heavy 256 files are both `prop2 = 2`.
