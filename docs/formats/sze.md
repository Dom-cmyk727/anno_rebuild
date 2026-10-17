# Scenarios (`.sze`)

**Status:** partial — the `SCENE` chunk list, the tagged-record container, players (`SPIELER`), ships and soldiers (`SCHIFF`, `SOLDAT`) and island links (`INSELLNK`) are decoded and re-encode byte-identically (P). The content of most records (`HEAD`, `STADT2`, `PRIVAT`, …) and of the other `SCENE` children is open (OQ-027, OQ-028).

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
| `WARETYPE` | 23 + 27 | 543 | ? | P (size) |
| `SKTIMER` | 23 + 27 | 40 | ? | P (size) |
| `MONOPOL` / `MONOPOL2` | 23 / 27 | 224 | ? | P (size) |
| `LIMITS` | 23 + 27 | 4 | ? | P (size) |
| `AUFTRAGTEXT` | 13 + 25 | 19–27 | ? | P (size) |
| `K_DATA` | 12 + 3 | 172 | ? | P (size) |
| `COMPPROFIL2` | 23 + 27 | 136, 840 or 2248 | tagged records `DEF`, `ENTRY` | P (all 50 parse exactly) |
| `SPIELER` | 192 + 532 blocks | 104–88,425 | [players](#players-spieler) | P |
| `INSEL5` | 326 + 342 in 44 files | | islands, [`insel.md`](insel.md) | P |
| `INSELLNK` | 0 + 229 in 9 files | 45–50 | [island links](#island-links-insellnk) | P |
| `ROHSTOFF` | 23 + 27 | 16 | ? | P (size) |
| `SCHATZ` | 23 + 27 | 2 | ? | P (size) |
| `ERZE` | 23 + 27 | 10 | ? | P (size) |
| `BIGBROTHER` | 23 + 27 | 174–29,425 | text script with `[OBJECT_DEFINE]`, `[AREA_DEFINE]`, `[PLAYERMAPPER_DEFINE]` sections | P (inspection) |

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

Records seen under a player (`SPIELER/…`): `HEAD` (108 bytes), `VOLK`, `ALIAS`, `STATIS` (12), `PROFIL`, `QUEST`, `BONUS`, `DISCO`, `DIPL` (`MILIT`, `TRADE`, `WAR`), `FORSCH` (`ENTRY`), `COMP`, `GROUP`, `RECORD`, `VIDEOS`, `SEL_0`…`SEL_7`, `SCHIFF`, `SOLDAT`, `STADT2` (`HEAD2`, `ID3`, `WARE`, `VK`, `EK`, `BAUMAT`, `PROD`, `STAT` or `STAT2` + `STST2`). Under units: `HEAD` (16), `WAREN`, `PRIVAT` (`NAME`, `VERK`, `CREW`, `ESEL`, `ROUTE`, `SBT`), `TASK` (P, `anno-dump sze --full`). Their content is open.

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

## Units (`SCHIFF`, `SOLDAT`)

| Offset | Size | Type | Name | Meaning | Evidence |
|---|---|---|---|---|---|
| 0x00 | 2 | u16 | `id` | unique among all units of a scenario | P |
| 0x02 | 2 | u16 | `figure` | `FigurenHeader.dat` entry id; ships 1501–1509 (ship figures, colour tables `KleinesSchiff01` etc.), soldiers 251–818 | P (ids), visual (1502 extracted: a small ship) |
| 0x04 | 1 | u8 | `owner` | equals the `index` of the `SPIELER` block that holds the unit | P (all 2,644); the game writes but never reads it: a unit belongs to the block that holds it (D, `annoscene.md` §7) |
| 0x05 | 1 | u8 | `attr` | ships 0–14, soldiers 0–7; figure property `0x16`, used as a facing in 45° steps by one routine | D (`annoscene.md` §7); soldiers' facing likely, ships' 16-step facing a guess (OQ-027 d) |
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
- What an island link resolves to — OQ-028

## Tests
- Unit tests: `crates/formats/src/tagged.rs` (short/long form round trip, malformed input), `crates/formats/src/sze.rs` (scene, player, ship round trip).
- Corpus test: `crates/formats/tests/scene_corpus.rs::scene_corpus` over all 50 `.sze` (needs `ANNO1503_GAME_DIR`): every `SPIELER`, unit, nested sequence of the table and `INSELLNK` re-encodes byte-identically; all figures on this page are asserted there.
- Reference tool output: `anno-dump sze` = `tools/sze.py dump`, corpus and synthetic files (`anno_dump_matches_sze_py_corpus`, `anno_dump_matches_sze_py_synthetic`).
