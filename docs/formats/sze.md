# Scenarios (`.sze`)

**Status:** partial — the `SCENE` chunk list, the tagged-record container, players (`SPIELER`), ships and soldiers (`SCHIFF`, `SOLDAT`) and island links (`INSELLNK`) are decoded and re-encode byte-identically (P). The AI profiles (`COMPPROFIL2`) and the per-player AI record (`SPIELER/COMP`) are decoded field by field (D + P, 2026-10-22, T18); four profile columns reach no setter and one only through a doubtful second call (OQ-112). The content of most other records (`STADT2`, `PRIVAT`, …) and of the other `SCENE` children is open (OQ-027, OQ-028). Read by the sim adapter (S14 stage 1, `src/sim/src/scenario_formats.cpp`, `docs/sim/state.md` §10): `HEAD`, the player `HEAD` and `VOLK`, the `STADT2` prefix and `INSELLNK` (as warnings); since T23 also the `STADT2` records `HEAD2`, `WARE`, `ID3`/`ID2` and `PROD/ENTRY` ([Cities](#cities-stadt2), D 2026-10-23).

## Files covered
- Glob: `Scenes/**/*.sze`
- Count in GOG 2.0.0.5: 50 (23 with root `ANNO`, 27 with root `ANNOADDON1`) (P)
- Built on: [`chunk-container.md`](chunk-container.md). Islands (`INSEL5`) are specified in [`insel.md`](insel.md).

All integers little-endian.

## Identification
- Root chunk `ANNO` (base game) or `ANNOADDON1` (add-on), holding exactly one chunk, `SCENE`, which is a generic container (P, all 50).
- `ANNO` vs `ANNOADDON1` differences seen so far: `MONOPOL` vs `MONOPOL2`; `INSELLNK` occurs only in add-on files; the long record header (below) only in add-on files (P).

## `SCENE` children

| Chunk | Files (`ANNO` + `ANNOADDON1`) | Payload size | Content | Evidence |
|---|---|---|---|---|
| `HEAD` | 23 + 27 | 32 | ? OQ-027 | P (size) |
| `MAIN` | 23 + 27 | 8 or 16 | ? | P (size) |
| `GUID` | 23 + 27 | 16 | ? | P (size) |
| `ALIASNAME` | 23 + 27 | 6–128 | ? | P (size) |
| `WARETYPE` | 23 + 27 | 543 | 56 × (`u16` id, NUL-terminated name): the goods table in id order (0 `UNUSED`, 1 `EISENERZ`, … 53 `KANONESHIP`), identical in all 50 files | P ([`../research/game-files-resurvey.md`](../research/game-files-resurvey.md) §4) |
| `SKTIMER` | 23 + 27 | 40 | 10 × `u32` script timers (`TIMER_0..4` state); all zero except one mission | P (resurvey §4) |
| `MONOPOL` / `MONOPOL2` | 23 / 27 | 224 | 28 × (`u32` count, `i32` owner), all `(0, −1)` in the shipped files; the `MonopolyTable` of O-24 | P (resurvey §4) |
| `LIMITS` | 23 + 27 | 4 | `u16` ship cap, `u16` soldier cap (40 / 100 endless, 20 / 100 missions, up to 50 / 400 add-on; ISS-061) | P (resurvey §4) |
| `AUFTRAGTEXT` | 13 + 25 | 19–27 | NUL-terminated file name of the mission page (`.htm` in the scenario folder) | P (resurvey §4) |
| `K_DATA` | 12 + 3 | 172 | ? (campaign files) | P (size) |
| `COMPPROFIL2` | 23 + 27 | 136, 840 or 2248 | [AI profiles](#ai-profiles-compprofil2): 0, 4 or 12 `ENTRY` (168 B: 19 four-byte columns, GUID, name, `Profil<nn>.htm`) and one `DEF` (128 B: profile per player) | P (all 50 parse exactly), D |
| `SPIELER` | 192 + 532 blocks | 104–88,425 | [players](#players-spieler) | P |
| `INSEL5` | 326 + 342 in 44 files | | islands, [`insel.md`](insel.md) | P |
| `INSELLNK` | 0 + 229 in 9 files | 45–50 | [island links](#island-links-insellnk) | P |
| `ROHSTOFF` | 23 + 27 | 16 | ? | P (size) |
| `SCHATZ` | 23 + 27 | 2 | ? | P (size) |
| `ERZE` | 23 + 27 | 10 | ? | P (size) |
| `BIGBROTHER` | 23 + 27 | 174–29,425 | the **scenario script**: `[OBJECT_DEFINE]`, `[AREA_DEFINE]`, `[PLAYERMAPPER_DEFINE]`, `[SCRIPT_KERNEL]` (`INITBLOCK`, `SET`, `IF … THEN { … }`), `[SIGNALS]` (variable state), `[CAMARAPOS]`; interpreter `AnnoBigBrother` in `AnnoScene.dll` | P, S (resurvey §2) |

Order (P, all 50): `HEAD, MAIN, GUID, ALIASNAME, WARETYPE, SKTIMER, MONOPOL|MONOPOL2, LIMITS, [K_DATA], [AUFTRAGTEXT], COMPPROFIL2, SPIELER × n (header series), INSELLNK × n, INSEL5 × n, ROHSTOFF, SCHATZ, ERZE, SPIELER × n (state series), BIGBROTHER`. Every file has `K_DATA` or `AUFTRAGTEXT`, three have both; at least one of `INSELLNK`/`INSEL5` occurs, and in the three files with both kinds the links come first.

## Tagged records

A small record container used inside `SPIELER` and `COMPPROFIL2` and, as it turns out, in `Anlagen.dat` `COLORS/ENTRY` ([`palette.md`](palette.md); this resolves OQ-022 b).

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 6 | ASCII | `name` | `[A-Z][A-Z0-9_]{0,5}`, padded with zero bytes (never garbage) | P (all 29,686 records under `SPIELER`, all `COMPPROFIL2`) |
| 0x06 | 2 | u16 | `length` | payload length, or `0xFF00` = long form | P |
| 0x08 | `length` | | payload | | P |

**Long form:** if `length` is `0xFF00` or more, its low byte holds bits 16–23 of the real length and the `u16` at 0x08 bits 0–15; the payload starts at 0x0A — a 24-bit length escape (D: `AnnoWelt.dll` reader `0x100b4f50`, `docs/re/annoscene.md` §10). In the corpus the high byte is always 0. It occurs in exactly the `ANNOADDON1` files, there in every `STADT2` record (193) and every `STADT2/PROD` record (193), whatever their size (a long-form `PROD` can be 22 bytes); never in an `ANNO` file and never elsewhere (P): the add-on's writers of these two records reserve the long header before they know the length (D: `0x1001f3fd`, `0x1002f23f`). A short-form payload of 65,280 bytes or more cannot be written.

A record sequence is a payload that parses *exactly* into records. In the `Anlagen.dat` colour tables, the "4-byte tag `BODY`, `u16` 0, `u16` length" of `palette.md` is this header: the `u16` 0 is the zero padding of the 6-byte name `BODY\0\0`.

### Nesting

Some records hold a fixed prefix followed by a nested record sequence. The table lists every combination for which **all** instances in the corpus parse exactly (P, `scene_corpus`); all other records are treated as leaves. The same table is `anno_formats::sze::nested_prefix` and `NESTED` in `tools/sze.py`.

| Parent | Record | Prefix bytes | Count | Prefix content |
|---|---|---|---|---|
| `SPIELER` | `SCHIFF` | 16 | 327 | [unit prefix](#units-schiff-soldat) |
| `SPIELER` | `SOLDAT` | 16 | 2,317 | [unit prefix](#units-schiff-soldat) |
| `SPIELER` | `STADT2` | 6 | 272 | `u8` city number on its island, `u8` 0, `u16 x`, `u16 y` (a tile of the town's island) (D, `annoscene.md` §8) |
| `SPIELER` | `DIPL`, `FORSCH` | 0 | 154, 28 | |
| `STADT2` | `PROD` | 0 | 272 | |
| `SCHIFF`, `SOLDAT`, `ENTRY` | `PRIVAT` | 0 | 327, 318, 397 | |
| `PRIVAT` | `CREW`, `ROUTE` | 0 | | |
| `PRIVAT` | `ESEL` | 8 | 83 | `u16` figure, `u16` id, `u16 x·10`, `u16 y·10` (D, `annoscene.md` §8) |
| `CREW` | `ENTRY` | 6 | 1,500 | `u16` id, `u16` figure, `u8` crew slot, `u8` 0 (D, `annoscene.md` §8) |

Records seen under a player (`SPIELER/…`): `HEAD` (108 bytes), `VOLK`, `ALIAS`, `STATIS` (12), `PROFIL`, `QUEST`, `BONUS`, `DISCO`, `DIPL` (`MILIT`, `TRADE`, `WAR`), `FORSCH` (`ENTRY`), `COMP`, `GROUP`, `RECORD`, `VIDEOS`, `SEL_0`…`SEL_7`, `SCHIFF`, `SOLDAT`, `STADT2` (`HEAD2`, `ID3`, `WARE`, `VK`, `EK`, `BAUMAT`, `PROD`, `STAT` or `STAT2` + `STST2`). Under units: `HEAD` (16), `WAREN`, `PRIVAT` (`NAME`, `VERK`, `CREW`, `ESEL`, `ROUTE`, `SBT`), `TASK` (P, `anno-dump sze --full`). The `STADT2` records are in [Cities](#cities-stadt2); the content of the others is open.

## Players (`SPIELER`)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 4 | u32 | `flag` | 0 | P (all 724) |
| 0x04 | 4 | u32 | `index` | player index: 0–30, and 31 in every scenario | P; 31 is skipped by every game-start loop (D), the scenario's neutral/scripted owner (likely, OQ-027 b) |
| 0x08 | | | records | tagged records | P |

The same `u32 flag, u32 id` prefix starts `Anlagen.dat` `COLORS/ENTRY` and `FigurenHeader.dat` `ENTRY`.

**`HEAD`** (108 bytes, header series only; D: `AnnoWelt.dll` `0x1001d2b9`–`0x1001d430`, `docs/re/annoscene.md` §6; `sze::PlayerHead`):

| Offset | Type | Meaning | Evidence |
|---|---|---|---|
| 0x00 | u32 (read as u16) | player type: 8, 11 human; 12 computer; 14 native peoples; 15 pirates; 16 Venetians; 17 god/gang; 2 player 31 | D; P counts below |
| 0x04 | u32 | starting money (10,000 / 25,000 / 30,000 …) | likely |
| 0x08 | char[48] | name, NUL-terminated | D |
| 0x38 | 16 bytes | player identity (matched against network participants) | D |
| 0x48 | u32 | flags | D |
| 0x4C | u8 | **colour slot** 1–8; `0xFF` = assigned at game start; 0 = none | D, P |
| 0x4D–0x60 | | further flags (`docs/re/annoscene.md` §6) | D |

Types × colour over all 724 blocks (P, `scene_corpus::player_heads`): type 2 none 47; type 8 at start 12, none 4, preset 28; type 11 at start 8, preset 1; type 12 at start 33, preset 47; types 14 / 15 / 16 none 80 / 41 / 26; type 17 at start 46. Player 31 is type 2 or 8 with colour 0.

**Colours at game start** (D: `AnnoFrame.dll` `0x10029100`, `0x1001d180`): preset colours are kept; the first human slot gets the colour picked on the colour screen; other human slots still at `0xFF` are removed with their objects; computers at `0xFF` get the lowest colour left in a pool 1–8; other types get none. Implemented for the viewer in `anno_assets::unit::start_colours` (the colour-screen pick is a stand-in: the lowest free colour). Over the corpus this colours 130 players and removes none (P).

Each scenario lists its players in two series (P, all 50):
- **Header series** (before the islands): one block per player, each holding `HEAD` (plus `VOLK`, `ALIAS`, `STATIS`, `PROFIL`, …); player 31 is always last.
- **State series** (after `ERZE`): the same indices in the same order, no block holds `HEAD`; the blocks hold `COMP`, `SEL_0`…`SEL_7`, `SCHIFF`, `SOLDAT`, `STADT2`, `GROUP`. In 22 of the 23 `ANNO` files the state series has no block for player 31; all 27 `ANNOADDON1` files have one.

**`COMP`** (state series; the computer logic's state for this player):

| Offset | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| 0x00 | 4 | u32 | player type, equal to the player's `HEAD` type; selects the AI class | P (282 of 282); D: `AnnoWelt.dll` reader `0x1001e3be` passes it to the AI factory, `CompLogic.dll` `0x1003dc50` switches on it |
| 0x04 | 4 | u32 | player index, equal to the enclosing `SPIELER` index | P (282 of 282); D (same call) |
| 0x08 | | ASCII | NUL-terminated strings `KEY=n` or `KEY=n,m` (decimal) | P (all 282 parse); D: `CompLogic.dll` readers `0x10001450`, `0x10040b00` |

Every player of types 11, 12 and 14–17 has one (282), types 2 and 8 none (P). Each key names one AI parameter, by index ([`annoscene.md`](../re/annoscene.md) §11.5); the reader passes the number(s) to the AI's parameter setter. The shipped records hold the keys `TYP FLAECHE LAGER HANDEL PROZENTE OPTIONAL TRANSPORT HAUPTKLIMA MAXSTADT ZIELGRUPPE [ZIELANZAHL] MINBAUMAT MINEISEN CHEATLIMIT KAMPFTYP [SETTRIBUTSPERRE] SUSPEND` in this order (= parameter index order; 260 + 16 + 6 records, P): they were written by the scenario editor's writer (`AnnoScene.dll` `0x100052b0`, which spells parameter 25 `SETTRIBUTSPERRE`). The AI's own readers and writer spell it `TRIBUTSPERRE` (`CompLogic.dll` `0x10001583`, `0x10040c41`, writer `0x10003c47`). So the editor-written key is ignored on load (D): the reader takes the text up to the last `=` (`strrchr` `0x10059c10`), compares it with `strncmp` (`0x10059bd0`) and skips unknown keys. `CompLogicClassic.dll` has no `SETTRIBUTSPERRE` string either (S). Values: 267 of 276 are 0; `endless02` (players 1, 3–6, natives) has 18 and `S01_04_01` (players 1–3) and `S01_05_01` (player 2) have 1, all lost in the original (P; OQ-114). The AI's writer adds further keys (`NATIVEBAUMODE`, `SPEZFLAG`, `DIPLPLAYER`, `LOGWARE`, `EKVKWARE`, `AREA`, … ; `game-files-resurvey.md` §3).

## Cities (`STADT2`)

A `STADT2` record (or its older name `STADT`, the same reader) in a player's state block is one town: the 6-byte prefix of the [nesting table](#nesting), then nested records that the city reader AnnoWelt `0x1002e170` takes in file order (D; `docs/re/annowelt.md` "Scenario cities"). In the corpus every `STADT2` holds `HEAD2`, `ID3`, `WARE`, `BAUMAT`, `VK`, `EK` and `PROD` (272 each), plus `STAT2` + `STST2` (193, the add-on files) or `STAT` (79) (P). The reader also takes `HEAD`, `ID`, `ID2` and `VOLK` (none in the corpus).

**Position.** The world reader hands the player reader the origin (0, 0) (D `0x100ae2a9`/`0x100ae2ad`); the `STADT2` reader adds the prefix's x, y unless bit 0 of the load context byte `+0x400` is set (`0x1001e2c4`–`0x1001e2fd`; likely clear on the scenario path, OQ-145), finds the island at that tile (world `+0x8c`; none: the record is skipped) and gets or creates the city by its number (island `+0xf4`). The positions inside the records are **island-relative**: the city reader gets the island's rectangle and adds its origin.

| Record | Layout | Meaning | Evidence |
|---|---|---|---|
| `HEAD2` | read into a zero-filled 64-byte buffer | u16 `[0x24]`: the warehouse capacity in tons, stored `<< 8` at city `+0x624`; 0 → 50 t. The other fields: [`HEAD2` byte map](#head2-byte-map) below (T25) | D `0x1002e383`–`0x1002e3e5` |
| `WARE` | length / 8 entries: u16 goods number (the file's numbering), u16 tons, 4 bytes not read | stock of that goods = tons << 8 (city `+0x334 + 12g`); the number is mapped through the load's goods table, built from `WARETYPE` by name; no table: every entry skipped; a mapped 0: skipped | D `0x1002e7ca`–`0x1002e84b`, `0x100a18d0` |
| `ID3` | u32 entries: x = bits 0–14, y = bits 15–29; bit 30 set: 8 mask bytes follow | the city owns the 8 × 8 chunk at (x, y): all 64 tiles, or with a mask the tile (x + i, y + j) where byte j has bit i; written unconditionally and unclipped (island `+0x100` = `0x10085620`) | D `0x1002e709`–`0x1002e7c5`; P: all 272 cities, 14 957 entries (8 739 whole chunks, 6 218 masked) |
| `ID2` | as `ID3`, in world tiles | as `ID3` | D `0x1002e65c`; none in the corpus |
| `ID` | 8-byte entries: u16 x, y, u8 `w \| h << 4`, a radius (0 → 16) | a claim with radius through island `+0xe8` (its shape is not read) | D `0x1002e5a5`; none in the corpus |
| `PROD` | nested records; `ENTRY`: u16 x, u16 y, then the building's own record | one building per `ENTRY` (below); another record name is skipped | D `0x100264a0`, `0x1002671d`; P: 8 561 entries |
| `BAUMAT` | length / 8 entries: u16 goods number (file numbering), u16 tons, 4 bytes not read | `BuildGoods::setting` of that goods = tons << 8 (city `+0x48c + 8g` = `+0x5d4 + 8(g − 0x29)` for goods `0x29`–`0x2d`, the field `SetTradeFlag` mode 9 writes); mapped through the goods table like `WARE` (no table or a mapped 0: skipped); no range check | D `0x1002e862`–`0x1002e8cd`; P: 272 × 5 entries, goods 41–45 in order, non-zero values (10–17 t) in 6 cities |
| `VK`, `EK` | length / 8 entries: u16 `[0]` → entry `+4` (`price`, likely), u16 `[2]` goods (file numbering), u16 `[4]` tons → entry `+8` = tons << 8 (amount, 8.8), `[6]` not read | entry `i` (the loop index, counted also over skipped entries) of the `VK` list (city `+0xdc`, 8 entries) or the `EK` list (`+0x44`, 12 entries), no bound; the goods record gets `+0xb` = `i` and `+0xa` bit 0 = 1 (`VK`) / 0 (`EK`); a mapped goods 0: skipped. `VK` (*Verkauf*) is likely the sell list, so the rebuild's `GoodsSetting::buy` names the bit the wrong way round (OQ-155) | D `0x1002e8e8`–`0x1002e9b9`, `0x1002e9d4`–`0x1002ea9e`; P: `VK` 4/6/8 entries (221/25/26), `EK` 6/8 (221/51), 75 and 76 entries name a goods, none twice in a list, `[6]` = 0 |
| `STAT2` | read into a zero-filled 36-byte buffer | u32 `[0x00]`…`[0x10]` → city `+0x628`…`+0x638` (five of the ten tick counters `+0x628`…`+0x64c` of city tick step h), u16 `[0x20]` → `+0x654`, u8 `[0x22]` → `+0x656`; `[0x14]`, `[0x18]`, `[0x1c]` (written from `+0x63c`…`+0x644`) and `[0x23]` are not read | D `0x1002e525`–`0x1002e5a0`, writer `0x1002f2b4`; P: 193, all 36 bytes |
| `STST2` | raw | city `+0x84c`…`+0x30bf` is zeroed and the record copied into it (likely the statistics screen's sampled history, city tick step i `0x1002d3b0`) | D `0x1002eb2a`–`0x1002eb4b`; P: 193, all 10 356 bytes |
| `STAT` | | skipped like an unknown record | D `0x1002eb4d`–`0x1002eb70`; P: 79, all 20 bytes |
| `HEAD` | read into a zero-filled 64-byte buffer | the older `HEAD2`: u16 `[2]` the owner player (not found → 0, then 31), who gets the city; then `[0]`, `[4]`…, `[0x24]`, `[0x26]`, `[0x27]`, `[0x28]`, `[0x2c]` as in `HEAD2` | D `0x1002e1c9`–`0x1002e37e`; none in the corpus |
| `VOLK` | | read into an uninitialised stack buffer and ignored | D `0x1002eaff`–`0x1002eb16`; none in the corpus |

**`PROD/ENTRY` → building** (D `0x100264a0`). The island resolves the entry's tile to the placement of the building whose footprint holds it (island `+0x84` = `0x10083a90`: the tile's object id, the origin from the tile's in-footprint offset, rotation, phase, height), so type, origin, rotation and height come from the island's `PROD2` copy, not from the entry; an unknown type skips the entry. `Create` (`0x10024010`) builds the building (the height goes to `+0x0f`), the bytes after the position go to the class's `Load` (slot `+0x7c`), then the city claims the footprint tiles that are still neutral (`0x10026685`). The class records, each read into a zero-filled buffer; `[1]` bit 0 (bit i for line i) is the switch (the `Load` calls building slot `+0xc8` with (output goods i of the type, the bit), which books the running-cost change and stores the bit, D, T25; first read as passing the bit through the type's slot `+0xac`):

| Class (`Load`) | Length rule | Bytes |
|---|---|---|
| workshop (`0x10068640`) | ≤ 10; longer: skipped whole | `[2]` output, `[4]` input 0, `[6]` input 1, `[8]` activity |
| farm, fur trapper (`0x1006ef70`) | exactly 10; else skipped whole | `[2]` output, `[4]` raw stock, `[8]` activity (`[6]` not read) |
| mine (`0x1006e310`) | into 10 bytes, no test | `[2]` output, `[8]` activity |
| small farm (`0x1006fc80`) | into 14 bytes, no test | `[2 + 2i]` output i, `[6 + 2i]` raw stock i, `[10 + 2i]` activity i |
| weapons workshop (`0x10069140`) | into 14 bytes, no test | `[2 + 2i]` output i, `[6 + 2i]` input i, `[10 + 2i]` activity i |
| house (`0x10076e60`) | reads 8 + `n` bytes (version 2) or 8 + 2`n` (version 1) or 8 (version 0), whatever the length | header `[1]` residents, `[2]` `n`, `[3]` version (`[0]`, `[4..7]` not read); version 2: `[8 + j]` = satisfaction `s_j` of need `j` (128 = 100 %; the save writes at most 160); version 1: the second byte of each 2-byte pair; [below](#house-and-market-records) |
| market, `MARKTKOMBI` (`0x1006be00`) | nested records | `KONTID`: u16 → building `+0x1e`, the market's number on its island; other names skipped |

P: 8 538 entries sit exactly on a `PROD2` item's origin and 23 on another tile; 7 460 carry class bytes. The `PROD2` items of building types are the tile layer's copy of the buildings: of the 8 810, 212 are second copies of an equal item on the same origin, 8 526 are named by an entry and 72 by none; 34 entries name a `DENKMAL` item (PRODTYPE 289, not a building of the layout) and one a tile without an item (OQ-144).

A `DENKMAL` entry makes `Create` build a base-class object (`0x100734c0`, `0x18` bytes) in the production list, like every PRODTYPE above 192; its bytes are skipped by the base `Load` (`0x10031840`) (D, T25; `docs/re/annowelt.md` "Scenario cities" (j)).

### `HEAD2` byte map

64 bytes (P: all 272), read into a zero-filled 64-byte buffer whatever the record's length (a longer record overruns it, D `0x100b4fd0`). Reader `0x1002e399`–`0x1002e50a`, writer `0x1002ebb0` (D, T25; `docs/re/annowelt.md` "Scenario cities" (g)).

| Offset | Size | Type | City field | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 1 | u8 | `+0x324` | the byte of the sub-object `+0x320` (likely the research topic in progress, OQ-157); the writer stores it as a u16 | D; P: 0 in 271, 17 in 1 |
| 0x01 | 3 | | | not read | P: 0 |
| 0x04 | 32 | char[] | `+0x24` | city name, NUL-terminated; copied up to the NUL with no bound (a name of 32 or more characters runs into the list at `+0x44`); default `New City` | D `0x1002e4c6`–`0x1002e504`, `0x1002fa64`; P: 4–20 characters, 16 names with bytes ≥ `0x80` |
| 0x24 | 2 | u16 | `+0x624` | warehouse capacity in tons (`<< 8`; 0 → 50 t) | D; P: equals the market-count table of city slot `+0xbc` in 259 of 272 (OQ-156) |
| 0x26 | 1 | u8 | `+0x140` | slot count of the `VK` list; 0 keeps the constructor's 4 | D; P: 4/6/8 = the `VK` entry count in all 272 |
| 0x27 | 1 | u8 | `+0xd8` | slot count of the `EK` list; 0 keeps the constructor's 6 | D; P: 6/8 = the `EK` entry count in all 272 |
| 0x28 | 4 | u32 | `+0x60c` | statistic 0 (sales to residents) as shown; stored `× 7` (`(105 · v mod 2³²) / 15`) | D; P: non-zero in 79 |
| 0x2c | 2 | u16 | `+0x620` | city property 1 (slots `+0xc4`/`+0xc8`); meaning open | D; P: 0 |
| 0x2e | 2 | | | not read | P: 0 |
| 0x30 | 4 | u32 | `+0x61c` | ORed into the player bit mask (bit = player index); meaning open (O-40) | D; P: always holds the owner's bit, only it in 181 |
| 0x34 | 2 | u16 | `+0x328` | `<< 8`; the sub-object's `+8` (OQ-157) | D; P: 25 in 1, else 0 |
| 0x36 | 2 | u16 | `+0x32c` | `<< 8`; the sub-object's `+0xc`, also recomputed from the city's `BIBLIOTHEK` buildings (OQ-157) | D; P: 25 in 1, else 0 |
| 0x38 | 4 | u32 | `+0x5fc` | statistic 4 (trade sales), stored `× 7` | D; P: 0 |
| 0x3c | 4 | u32 | `+0x600` | statistic 5 (trade purchases), stored `× 7` | D; P: non-zero in 1 |

The running costs (statistics 1 and 2, `+0x604`/`+0x608`) and the u16 statistics 8, 9, 13, 14 are not saved here.

### House and market records

**House** (`Load` `0x10076e60`, save `0x10077130`; D, T25). The building's group (PIONIER … ARISTOKRATEN) comes from its type, not from the record.

| Offset | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| 0x00 | 1 | u8 | not read; the save writes 0 | D; P: 0 in all 3 283 |
| 0x01 | 1 | u8 | residents; house `+0x2c` = residents · 64, `E` (`+0x2e`) = residents · 128; the city's resident counter is moved from the old to the new count. **Reset:** in a group above PIONIER, fewer residents than half the lower group's `Wohnmax` become the group's own `Wohnmax` | D `0x10076e9e`–`0x10076f31`; P: never below that bound, never above `Wohnmax` |
| 0x02 | 1 | u8 | `n`, the number of need bytes; the save writes the group's need count | D; P: 6 (1 874 houses), 10 (688), 12 (631), 11 (90) |
| 0x03 | 1 | u8 | version: 0 = no need bytes (the rest is skipped), 1 = two bytes per need, any other value = one byte per need; the save writes 2 | D `0x10076f34`, `0x10076f69`; P: 2 in all |
| 0x04 | 4 | | not read; the save writes 0 | D; P: 0 |
| 0x08 | n | u8[n] | version 2: satisfaction `s_j` of need slot `j` (128 = 100 %; the save writes at most 160); stock `+0x30 + 2j` = `trunc(Menge_j · s_j · residents · 128 / 1536)` for `j < min(n, the group's need count)`. After a reset, need 0 gets 128 and every forced need `128 − (r & 31)` with a draw `r` from the world's random sequence, replacing the file's bytes | D `0x1007702b`–`0x10077119`; P: 0–160 |
| 0x08 | 2n | | version 1: the same per need from the **second** byte of each pair | D `0x10076f71`–`0x1007701a`; P: none |

No length test: the header and `n` (or `2n`) bytes are read whatever the record's length (the need bytes go into a 64-byte stack buffer). The `Load` returns 1 on every path.

**Market** (PRODTYPE 33–36; `Load` `0x1006be00`, save `0x1006beb0`; D, T25): a [record sequence](#tagged-records); `KONTID` holds a u16 (2 bytes are read whatever its length), stored at building `+0x1e`; any other record is skipped. `KONTID` is the market's number on its island: when it is 0 after the `Load`, the entry loop gives the market the island's counter (island property `0x11`) and raises the counter (`0x100266a5`–`0x100266f5`); a loaded number is kept (OQ-158). P: 781 markets, each with one 2-byte `KONTID`, values 1–30, never 0, unique per island (225 islands).

Other entry shapes not yet read (P): a nested `SLICE` (278 entries, 8 bytes), a nested `CREW` (251), one byte (579) (OQ-145).

## Units (`SCHIFF`, `SOLDAT`)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `id` | unique among all units of a scenario | P |
| 0x02 | 2 | u16 | `figure` | `FigurenHeader.dat` entry id; ships 1501–1509 (ship figures, colour tables `KleinesSchiff01` etc.), soldiers 251–818 | P (ids), visual (1502 extracted: a small ship) |
| 0x04 | 1 | u8 | `owner` | equals the `index` of the `SPIELER` block that holds the unit | P (all 2,644); the game writes but never reads it: a unit belongs to the block that holds it (D, `annoscene.md` §7) |
| 0x05 | 1 | u8 | `attr` | starting **facing**: ships in 16 steps of 22.5° (0 = −x, 4 = +y, 8 = +x, 12 = −y), soldiers in 8 steps; the loader passes it to the unit's property `0x16` | D: `AnnoWelt.dll` `0x1001e01d`–`0x1001e041` (ships, verified), `0x1001ddad` (soldiers); `docs/re/fire-and-ships.md` §3.3 |
| 0x06 | 2 | u16 | | 0 | P (all) |
| 0x08 | 4 | f32 | `x` | world tile position; a tile centre (`n + 0.5`) for 310 of 327 ships and 2,253 of 2,317 soldiers; `x` 6.5–991.9, `y` 10.5–978.5 | P |
| 0x0C | 4 | f32 | `y` | | P |
| 0x10 | | | records | `HEAD`, `WAREN`, `PRIVAT`, optional `TASK` | P |

Ships per figure: 1501: 29, 1502: 64, 1503: 38, 1504: 42, 1505: 25, 1506: 61, 1507: 21, 1508: 46, 1509: 1 (P). Ships carry a `PRIVAT/NAME` (e.g. `Jana`) (P).

`x`/`y` are kept as raw IEEE 754 bits in `crates/formats` (byte-identical round trip). They are file data, not sim state; the simulation converts them to its fixed-point positions.

## Island links (`INSELLNK`)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 12 | | head | same layout as the `INSEL5` head: `u16 x, u16 y, u16 width, u16 height, u32 number` | P |
| 0x0C | n | ASCII | `path` | NUL-terminated (exactly one NUL, at the end) path relative to `Scenes`, e.g. `islands\polar\po_080x080_dummy.scp` | P (all 229) |

Every path names one of the 21 `INSEL4` `*_dummy.scp` templates (found case-insensitively) whose size equals the link's `width × height` (P, all 229). Island numbers are unique across a scenario's `INSEL5` and `INSELLNK` (P). Six climates occur (`polar`, `tundra`, `nord`, `steppe`, `praerie`, `dschungel`). Six add-on scenarios (the endless games `03`–`08`) have no `INSEL5` at all, so their islands come only from links. At game start the game replaces every link to a `*DUMMY.scp` by a random real template (D: `AnnoScene.dll` `0x10009e0f`, `0x10009550`, `docs/re/annoscene.md` §3–5): candidates are all `Scenes\Islands\**\*.scp` except dummies and names containing `PIRAT` (cached in `InselCache.txt`), with the link's width and height, its climate (`INFO[0]`) and `INFO[8]` = `INFO[9]` = 0; ores and raw materials (`ERZE`, `ROHSTOFF`) are distributed over the links first, and the pick uses the world's random sequence from the per-game seed. Savegames store the chosen islands as `INSEL5`. The dummies themselves are flat grass squares without coast or objects. The viewer draws a deterministic stand-in from the same climate and size (`docs/render/iso.md`, "Scene composition").

## AI profiles (`COMPPROFIL2`)

A [record sequence](#tagged-records): `ENTRY` × n, then exactly one `DEF` (P, all 50). n = 12 in the eight endless games `0010SingleEndlos/01`–`08`, 4 in `0011SingleEndlosAddon/endless03`, 0 in the other 41 files. Each of the twelve shipped profiles is byte-identical in every file that holds it; the records are sorted by name (P). The values are listed in [`../research/game-files-resurvey.md`](../research/game-files-resurvey.md) §3.

Who reads it (D, [`annoscene.md`](../re/annoscene.md) §11):
- The game's scene loader does not: `AnnoScene.dll`'s game record handler `0x10009cc0` has no case for it and the world chunk reader it falls through to has no such name (S: the string is absent from `AnnoWelt.dll`).
- The single-player setup reads it from the scenario file to offer opponents (`AnnoFrame.dll` `0x1001e5d0`). At game start the chosen profiles' columns go to the AI module as parameters (`0x10028d90`).
- The scenario-editor class of `AnnoScene.dll` has a reader and a writer (`0x10007110`, `0x10007320`); that class has no caller in the shipped modules (`annoscene.md` §1).

### `ENTRY` (168 bytes, one computer-player profile)

"Col" is the index of the `u32` (0–18). "Parameter" is the AI parameter index the game-start hand-over passes the column to, with its key in the `COMP` record ([above](#players-spieler)) and, where one exists, the scenario-script name (`COMPCAPS_*`, the `SET_PROFILE` action). Hand-over: `AnnoFrame.dll` `0x10028e73`–`0x10028fe0`. The parameter table with the setter's effect for every index is [`annoscene.md`](../re/annoscene.md) §11.5. "Setter" is what the AI's setter does with the value (D: `CompLogic.dll` `0x10004550`).

| Offset | Size | Type | Col | Parameter | Setter (D) | Shipped values | Evidence (hand-over read) |
|---|---|---|---|---|---|---|---|
| 0x00 | 1 | u8 | 0 | — not passed | — | 1 | constructor sets it to 1 (`AnnoFrame` `0x1001e18a`, `AnnoScene` `0x1000724e`); no reader found (OQ-112) |
| 0x01 | 3 | | | | | 0 | P; the constructor writes 0 |
| 0x04 | 4 | u32 | 1 | 0 `TYP` (`COMPCAPS_TYP`) | 0–7 kept, larger ignored; selects a time interval (§11.6) | 0–7 | `0x10028e8a` |
| 0x08 | 4 | u32 | 2 | 1 `FLAECHE` | stored | 0 | `0x10028eca` |
| 0x0C | 4 | u32 | 3 | 2 `LAGER` (`COMPCAPS_LAGER`) | 0–3 | 1–3 | `0x10028ee0` |
| 0x10 | 4 | u32 | 4 | 3 `HANDEL` (`COMPCAPS_HANDEL`) | 0–3 | 3 | `0x10028e9e` |
| 0x14 | 4 | u32 | 5 | 6 `TRANSPORT` (`COMPCAPS_TRANSPORT`) | 0–3 | 1, 3 | `0x10028eb4` |
| 0x18 | 4 | u32 | 6 | 4 `PROZENTE` (`COMPCAPS_PROZENTE`) | 5–30 kept; the AI derives the factors 1 + v/100 and 1 − v/100 | 18–25 | `0x10028f80` |
| 0x1C | 4 | u32 | 7 | — not passed | — | 0 | OQ-112 |
| 0x20 | 4 | u32 | 8 | 25 `TRIBUTSPERRE` | stored | 0 | `0x10028f96` |
| 0x24 | 4 | u32 | 9 | 18 `KAMPFTYP` (`COMPCAPS_KAMPFTYP`) | low 4 bits kept: two 2-bit combat switches (§11.6) | 0–15 | `0x10028f38` |
| 0x28 | 4 | u32 | 10 | 7 `HAUPTKLIMA` | stored as 4 (v = 0), 16 (v = 1) or 20 (other) | 3 | `0x10028ef6` |
| 0x2C | 4 | u32 | 11 | 8 `MAXSTADT` (`COMPCAPS_MAXSTADT`) | 0–8 | 4–7 | `0x10028f0c` |
| 0x30 | 4 | u32 | 12 | 9 `ZIELGRUPPE` (`COMPCAPS_ZIELGRUPPE`) | 0–4 | 3 | `0x10028f22` |
| 0x34 | 4 | u32 | 13 | 26 `SUSPEND`, second call (see below) | zero / non-zero (pause flag, likely) | 0 | `0x10028fc9` (OQ-112) |
| 0x38 | 4 | u32 | 14 | second value of that call, unused by parameter 26 | — | 0 | `0x10028fc5` (OQ-112) |
| 0x3C | 4 | u32 | 15 | — not passed | — | 3, 60 | OQ-112 |
| 0x40 | 4 | u32 | 16 | 13 `MINBAUMAT` (`COMPCAPS_MINBAUMAT`) | stored | 0 | `0x10028f4e` |
| 0x44 | 4 | u32 | 17 | 14 `MINEISEN` (`COMPCAPS_MINEISEN`) | stored | 0 | `0x10028f67` |
| 0x48 | 4 | u32 | 18 | 26 `SUSPEND` (`COMPCAPS_SUSPEND`), first call | zero / non-zero (pause flag, likely) | 0 | `0x10028fac` |

| Offset | Size | Type | Meaning | Shipped values | Evidence |
|---|---|---|---|---|---|
| 0x4C | 16 | GUID (`u32, u16, u16, u8[8]`) | profile identity; all zero or all `0xFF` = no profile | one distinct GUID per profile | D `AnnoFrame` `0x10019ff0` (the test), `0x10028e52` (its use); P |
| 0x5C | 25 | char[25] | display name, NUL-terminated; not passed to the AI | `Hagen V. Sayn`, … | P |
| 0x75 | 51 | char[51] | character page in `Profiles\Texte\`, NUL-terminated | `Profil01.htm` … `Profil12.htm` | D `AnnoFrame` `0x1001f8d0` opens `%s\Profiles\Texte\%s` with it; P |

- **Columns 13, 14 and 18:** the hand-over sets parameter 26 (`SUSPEND`) from column 18. Its last call then sets parameter 26 again, with column 13 as the value and column 14 as a second value (`0x10028fc5`–`0x10028fe0`). Parameter 26 reads only the first value, so the effective `SUSPEND` comes from column 13. A two-value call fits parameter 17 (`CHEATLIMIT`, the only two-value parameter), so a wrong index in the original is a **guess** (OQ-112). It has two problems: the hand-over would pass `CHEATLIMIT`'s two values in reverse column order (`annoscene.md` §11.4), and it contradicts the key-order reading below. With the shipped zeros all readings give the same result.
- **Unknown columns:** 0, 7 and 15 are not passed on by any code found, and 14 only rides along as an unused second value, so their keys cannot be read from the code; 13 reaches the AI only through the doubtful second call (OQ-112). Two readings disagree:
  - *By order:* every confirmed column follows the order in which the AI's key reader tests the keys (`TYP FLAECHE LAGER HANDEL TRANSPORT PROZENTE OPTIONAL TRIBUTSPERRE KAMPFTYP HAUPTKLIMA MAXSTADT ZIELGRUPPE ZIELANZAHL CHEATLIMIT MINBAUMAT MINEISEN SUSPEND`, `CompLogic.dll` `0x10040b79`–`0x10040d4e`). That order leaves exactly one key for column 7 (`OPTIONAL`) and three values for columns 13–15 (`ZIELANZAHL`, then the two `CHEATLIMIT` values).
  - *By value:* column 15 (3 or 60) looks like the `OPTIONAL=3` / `=60` of the `COMP` records. The files speak against it: three players whose `COMP` record copies their profile (Profil03) in every mapped key have `OPTIONAL=3` where column 15 is 60 (P, OQ-112).

  Both are **guesses**. The same three copies support the mapping of columns 1, 3–6 and 9–12 independently of the code (P).
- **Fill:** after the NUL, the name and page fields are filled with `0xCC` in every shipped record (P). This is the MSVC debug-heap pattern, so the editor saved uninitialised memory (**likely**). A reader must ignore the bytes after the NUL; a round-trip writer keeps them as read.
- **Defaults** (record constructor, `AnnoFrame` `0x1001e140` and `0x100172d0`, `AnnoScene` `0x1000716c`): columns `1 4 0 2 3 1 18 0 0 15 3 5 5 0 0 60 0 0 0`, GUID `{E861A553-AFA5-4C53-BAEB-6AF500000000}`, name `Default`, page `Default.htm`. The default `ZIELGRUPPE` 5 is outside the setter's limit 0–4, so the setter would ignore it. The readers zero the record before filling it, so a record read from a file never carries these defaults.

### `DEF` (128 bytes)

| Offset | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| 0x00 | 4 × 32 | i32[32] | entry `i` = index of the `ENTRY` used for player `i`, −1 = none | D: `AnnoFrame.dll` `0x1001eb8b`–`0x1001ec03`; P |

- **How the game uses it (D):** the setup reader first clears the GUIDs of its 7 opponent slots (`0x1001e640`). It then copies, in player order, the `ENTRY` named by every entry ≠ −1 into the next slot. Neither the number of slots nor the `ENTRY` index is checked. At game start the next slot goes to every player except 31 whose type is 12 (computer) and whose colour is still unassigned (`0xFF`), in index order (`0x10028d90`). A slot without a valid GUID sends parameter 34 instead (`0x10028e63`).
- **Corpus (P):** in all nine files the entries ≠ −1 are exactly the type-12 players with colour `0xFF`. So in these files `DEF[i]` is player `i`'s profile, e.g. `endless03`: players 1, 2, 3 and 10. Entry 0 (the human) is always −1. The 41 files without `ENTRY` have all 32 entries −1. One of them, `0201SingleAddOn/single39.sze`, still has a type-12 player with colour `0xFF` (player 17); it is the one shipped case of a computer player that gets no profile (§11.4 of `annoscene.md`).
- **Limits:** the original has 7 opponent slots and 32 entries (the player count). More than 7 entries ≠ −1 would make it write past its slot array, and an out-of-range entry would read past the profile list (no checks, D). A reimplementation takes both sizes from data and rejects a `DEF` entry that names a missing `ENTRY`.
- **Multiplayer:** the 7 chosen slots are sent to the other machines as network message 6 (1,180 bytes, `0x1001fd10`; receiver `0x10019a92`).

## Reference dump

`anno-dump sze [--full] <file.sze>...` and `python3 tools/sze.py dump [--full] <file.sze>...` print the same bytes (`crates/tools/tests/agree_python.rs`):

```text
# <path>
SCENE edition=<ANNO|ANNOADDON1> children=<n> players=<SPIELER blocks>
LINK x=<x> y=<y> width=<w> height=<h> number=<n> path="<quoted>"     one per INSELLNK, or `LINK BAD`
PLAYER i=<block> index=<index> flag=<flag> records=<n>                or `PLAYER i=<k> [index=.. flag=..] BAD`
  RECORD <name> size=<payload bytes> long=<0|1> hash=<FNV-1a 64 of the payload>
  UNIT <SCHIFF|SOLDAT> id= figure= owner= attr= reserved= x=<f32 bits, 8 hex> y=<…> records=<names>   or `UNIT <kind> BAD`
```

With `--full`, every record that the nesting table marks is followed by its nested records, indented by two spaces per level: `PREFIX <hex>` (if the prefix is not empty), then `<name> size= long= hash=` lines; `NESTED BAD` if a nested sequence does not parse. A file that is not a scenario prints `NOPARSE`. Quoting: printable ASCII except `"` and `\` as is, everything else `\xHH`.

## Open fields
- Long-form header choice, role of player 31, the `STADT2`/`ESEL`/`CREW/ENTRY` prefixes, unit `attr`, record contents — OQ-027
- The `STADT2` records are decoded since T25 (what is left: the meaning of several fields, the other class records of `PROD/ENTRY`, the context bit) — OQ-145, OQ-155..158; entries and `PROD2` items that do not pair — OQ-144
- What an island link resolves to — OQ-028
- `COMPPROFIL2` `ENTRY` columns 0, 7, 14, 15 (unused) and 13 (the doubled `SUSPEND` call) — OQ-112; what the AI parameters do — OQ-113

## Tests
- Unit tests: `crates/formats/src/tagged.rs` (short/long form round trip, malformed input), `crates/formats/src/sze.rs` (scene, player, ship round trip).
- Corpus test: `crates/formats/tests/scene_corpus.rs::scene_corpus` over all 50 `.sze` (needs `ANNO1503_GAME_DIR`): every `SPIELER`, unit, nested sequence of the table and `INSELLNK` re-encodes byte-identically; all figures on this page are asserted there.
- Reference tool output: `anno-dump sze` = `tools/sze.py dump`, corpus and synthetic files (`anno_dump_matches_sze_py_corpus`, `anno_dump_matches_sze_py_synthetic`).
