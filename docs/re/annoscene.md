# `AnnoScene.dll` — scenario loader, island links, players, player colours

**Status:** first pass (2026-10-17), static analysis only. Modules: `game/app/AnnoScene.dll` (scene loader), with the parts it hands over to in `AnnoWelt.dll` (world, players, units, record I/O) and `AnnoFrame.dll` (game start, colour selection). GOG 2.0.0.5, image base `0x10000000` for all three. Tools: Ghidra 12.1.4 headless (`cache/ghidra/plus/*.c`), `objdump -d`, RTTI resolved by hand (type descriptor → complete-object locator → vtable). Addresses are virtual addresses in the module named in each section. Nothing here is copied code; behaviour is paraphrased. Corpus checks use `tools/sze.py` / `tools/chunks.py` over the 50 `.sze` and the 465 island templates (marked **P**).

Confidence: **confirmed** = read in the decompiled/disassembled function; **likely** = strong inference, not traced end to end; **guess** = hypothesis, also listed under "Open".

Answers in short: OQ-028 — a link naming a `*DUMMY.scp` is replaced at game start by a random real template of the same climate and size (§3–§5). OQ-027 — player type, colour and identity fields of `HEAD`, player 31, unit `attr`, and the `STADT2`, `CREW/ENTRY` and `ESEL` prefixes: §6–§8. Player colours: §9. Long record header: §10. AI profiles (`COMPPROFIL2`), the opponent setup, the hand-over to `CompLogic.dll` and the AI parameter table: §11 (added 2026-10-22, T18; static analysis with `tools/disasm.py` and a capstone linear sweep, no Ghidra on this host).

## 1. Entry points and classes (`AnnoScene.dll`)

- `CreateAnnoSceneInstance` (`0x10001150`, cdecl): `(kind, path, p3, out, p5, seed, p7)`. Kind 0 → class with vtable `0x1003b344` (the game), kind 2 → vtable `0x10031174`, kind 1 → vtable `0x10031420` (object of `0x80f8` bytes). Then slot `+0xBC` (init) and, if a path is given, slot `+0x50` (load) with `(path, seed)`. **Confirmed.** All calls in `AnnoFrame.dll` and `View.dll` use kind 0; no caller of kind 1 was found in the shipped modules.
- Load `0x100025a0` (reached through `0x10008460`, slot `+0x50`): stores the seed at `this+0x14C`, opens the file, accepts root `ANNO` or `ANNOADDON1` (`this+0x168` = 1, `| 2` for `ANNOADDON1`; passed to the world as property 14) and feeds every `SCENE` child to slot `+0x5C`. **Confirmed.**
- Record handler `0x10009cc0` (slot `+0x5C` of kinds 0 and 2), by child name. **Confirmed:**
  - `HEAD`: creates the world through slot `+0x60` (kind 0: `0x10002e40`, which calls `AnnoWelt` `CreateAnnoWeltInstance`) and sets world properties 0, 1, 2, 3, 6, 10, 14 from the 32-byte `HEAD`. If `this+0x14C` (the seed) is not 0, `0x10002e40` also sets world property 8 = seed (§4).
  - `INSELLNK` (`0x10009e0f`): §3.
  - `ROHSTOFF` (`0x1000a076`): 16 bytes into a table of raw-material counts at `this+0x51DC` (20 `u16`).
  - `SCHATZ` (`0x1000a158`): `u16` at `this+0x150` = number of treasures.
  - `ERZE` (`0x1000a19f`): 10 bytes into ore counts (`this+0x5292`…, 9 `u16`) and two more fields; byte 6 → `this+0x528A` (count of "special" islands, §5). Then runs the island selection `0x10009550` (§5); it returns at once when the scenario has no dummy links.
  - `BIGBROTHER`: creates the script object (`AnnoBigBrotherCreateInstance`).
  - Everything else (`SPIELER`, `INSEL5`, `WARETYPE`, …) goes to the world's chunk reader, world slot `+0x78` (§6).
- After the file is read, `0x100025a0` gives every player of type 12 (computer, §6) with an empty name a random name from the `COMPSPIELER_NAME` list, drawn with the world generator (§4). **Confirmed.**

## 2. Island templates and their `INFO` record (`AnnoWelt.dll`)

- `GetAnnoInselFileData` (`0x100b25a0`, export) opens an island file (root `ANNO`), takes width and height from `INSEL4` or from the 12-byte `INSEL5` head and, via `0x100b2770`, copies the 12-byte `INFO` record into a 20-byte summary (`u32 width, u32 height, INFO[0..11]`). **Confirmed.**
- `INFO` bytes used by the selection (meaning of the values from the corpus, **P** over 444 non-dummy templates):

| `INFO` byte | Use in `0x10009550` | Corpus |
|---|---|---|
| 0 | climate, must equal the link's climate | 0 polar, 1 tundra, 2 nord, 3 praerie, 4 dschungel, 5 steppe (matches the folder) |
| 2 | "special" flag (§5) | 0: 285, 1: 157, 2–3: 2 |
| 4 + 5 | capacity, compared with the ores assigned to the link | byte 4 always 0; byte 5 0–39 |
| 8, 9 | must both be 0 | byte 8 = 1 or 2 in 15 templates (13 `*Pirat*`), byte 9 always 0 |

Byte 1 is 0–3 and usually equals byte 2; bytes 6–7 are a `u16` = width · height / 64 (64, 100, 256, 1024 for 64², 80², 128², 256²; P, 444 of 444); bytes 3, 10, 11 are 0. Only bytes 0, 2, 4, 5, 8, 9 are read by the selection (**confirmed**).

## 3. `INSELLNK` handling (`AnnoScene.dll` `0x10009e0f`–`0x1000a073`)

**Confirmed:**
1. Reads the 12-byte head (`u16 x, u16 y, u16 width, u16 height, u32 number`) and the path into an island slot table at `this+0x16C + 0x50 · number` (path at slot `+4`; accessors `0x1000a5f0` set / `0x1000a640` get).
2. If the last 9 characters of the path equal `DUMMY.scp` (case-insensitive, `_stricmp` at `0x10027310`): takes the file name after the last `\`, finds the climate by a case-insensitive prefix match against the table at `0x1003b328` — `po_` 0, `t_` 1, `n_` 2, `pr_` 3, `d_` 4, `s_` 5, `w_` 6 — and appends a 334-byte link record to the list at `this+0x53B8`/`+0x53BC` (count `this+0x53B0`). Record: `+0` x, `+2` y, `+4` width, `+6` height (`u16`), `+8` number (`u32`), `+0xC` climate (`u16`), `+0xE` chosen template name, `+0x112` raw-material marks (20 `u16`), `+0x13A` ore counts (9 `u16`), `+0x14C` special flag, `+0x14D` treasure flag.
3. Otherwise the named file (`Scenes\%s`) is loaded and placed directly (resolver `this+0x154` slot `+0x68`, then world slot `+0x88`). So a link to a non-dummy island places exactly that island. None of the 229 links in the corpus does this (all name dummies, `sze.md`).

## 4. The world random generator (`AnnoWelt.dll`)

- Draw (`0x100a1740`, world slot `+0xB4`; base vtable `0x100CC92C`, game world `0x100CCDD8`): `cursor = (cursor + 1) mod modulus`, return `table[cursor]`. Cursor `u16` at world `+0xFC`, modulus `u16` at `+0xFE`. **Confirmed.**
- `table` (`0x100cb8f8`, `.rdata`, 2048 `u16`) is exactly the first 2048 outputs of the MSVC `rand()` LCG started from seed 1 (`x ← x · 214013 + 2531011`, output `(x >> 16) & 0x7FFF`): all 2048 values match. **Confirmed** (P, compared value by value).
- World base constructor `0x100a3980`: `srand(GetTickCount())`, modulus 2048, cursor = `rand() mod 2048`. **Confirmed.** So without a seed the cursor is time-based.
- World property 8 (`SetProperty` `0x100a3230` → `0x100a32ac`; `GetProperty` `0x100a3380` → `0x100a3402`): set cursor = value mod modulus / read cursor. **Confirmed.** A world reset at `0x100af240` (game-world slot `+0x1C0`) sets the cursor to 0; when it runs relative to a scene load is not traced.
- The seed passed to the scene (`AnnoFrame.dll`):
  - Normal game start `0x10029100` (called from `0x1002a190`) creates the scene with seed = the global `0x1009b978` (call at `0x100291af`). **Confirmed.**
  - `0x1009b978` is set once in the frame constructor `0x100152f0`: `srand(GetTickCount())`, then `rand() mod 2048 + 1` (never 0, so the cursor is always set). **Confirmed.**
  - In network games it is overwritten from network message type 0 (`u16`, receiver `0x10019aae`); the sender `0x100242ac` transmits the local value. **Likely** that the host sends and the clients adopt it (roles not traced).
  - A second start path `0x100206c0` passes a fresh `rand()` instead (`0x1002078a`). **Confirmed**; what this path is for (restart?) is open.
- Consequence: the island choice is reproducible given (seed, candidate list order); the candidate order comes from the cache file (§5), so a network game needs identical `InselCache.txt` content on all machines. **Likely.**

## 5. Island selection at `ERZE` (`AnnoScene.dll` `0x10009550`)

Runs only if dummy links exist. **Confirmed** unless marked:
1. **Candidate list.** Resolves `Scenes\Islands\` (`0x1003d448`) and the cache file `Scenes\Islands\InselCache.txt` (`0x1003d458`). If the cache cannot be opened (`0x10008430`), scans the directory tree (`0x10007c50`) and writes the cache (`0x10007fe0`); then always reads the list from the cache (`0x10008110`). The scan (`FindFirstFileA`/`FindNextFileA`, recursive into all subfolders except `.`/`..`) keeps files whose name ends in `.scp` (case-insensitive), upper-cases the name, drops names containing `PIRAT` and names ending in `DUMMY.scp`, and keeps those for which `GetAnnoInselFileData` succeeds. Cache entry (binary despite `.txt`): 260-byte path, `u16` width, `u16` height, then `INFO` bytes 0, 1, 2, 4, 5, 6, 8, 9 (**likely**, from the `fwrite` sizes). Order = directory enumeration order.
2. **Matching.** For every link, the candidates are the templates with equal width, height and climate (`INFO[0]`) and `INFO[8] = INFO[9] = 0`.
3. **Raw materials** (`ROHSTOFF`, 20 kinds) are distributed one at a time over the links (`0x10009170`: climate compatibility masks in `0x10009010`, preference by size class, random choice among up to 5 best links); a chosen link gets a mark at record `+0x112 + 2·kind`. **Ores** (`ERZE`, 9 kinds) likewise (`0x10009330`, climate table `0x1003af40`), counted into record `+0x13A`. Both use the world generator; up to 25 rounds per kind. **Confirmed** for the loop structure, **likely** for the meaning of the tables.
4. **Capacity filter.** A candidate is dropped for a link if `INFO[4] + INFO[5]` < the link's total assigned ores. Each link records whether its remaining candidates include special (`INFO[2] ≠ 0`, bit 1) and/or normal ones (bit 2).
5. **Links with only one kind**: template = candidate `draw() mod count`; if it is special, the special counter (`ERZE` byte 6, `this+0x528A`) is decremented.
6. **Special marking**: the remaining counter (≥ 0) times, a random link (`draw() mod links`, then linear probing) that has both kinds and is not yet marked is marked.
7. **Links with both kinds**: list the candidates (at most 50) whose special flag agrees with the mark (special if marked and the counter is > 0, normal if unmarked), pick `draw() mod listlen`.
8. The chosen file name goes to record `+0xE`, its special flag to `+0x14C`.
9. **Treasures**: `SCHATZ` times, a random link (`draw() mod links`) not yet flagged gets `+0x14D = 1` (repeats until enough distinct links).
10. **Placement** `0x10008490`: loads each chosen template, places it at the link's position, and puts the assigned ores onto the island's mountain objects (again with the world generator). A link with no candidate left keeps an empty name; it is then **likely** skipped (not traced).

Candidates per (climate, size) used by links (P): 6–69, e.g. nord 128×128: 69, tundra 80×80: 11 (none special), polar 80×80: 10 for 13 links.

**Savegames:** the world writer `0x100a3020` (`AnnoWelt` world slot `+0x7C`) writes one `INSEL5` per island, and no game class writes `INSELLNK` — the only `INSELLNK` writer is `0x100063b0` of the kind-1 class (`0x10031420`, read handler `0x10005750`), which has no caller in the shipped modules. So a savegame stores the chosen islands themselves; the link and the seed are not needed to reload. **Likely** (writer of the game class not followed record by record). Whether the cursor is saved is open.

## 6. Players (`SPIELER`) and `HEAD` (`AnnoWelt.dll`)

- Game-world chunk reader `0x100ae270` (slot `+0x78`): `SPIELER` → reads the 8-byte prefix, looks the player up by index in the player list (`world+0x118`, slot `+0xC`), creates it if missing, and lets the player read its records (player slot `+0x54`). **Confirmed.**
- Player class vtable `0x100c2238`: read `0x1001d240`, write `0x1001e920`, `GetProperty` `0x10020a00`, `SetProperty` `0x10020c80`. **Confirmed.**
- `HEAD` (108 bytes; reader `0x1001d2b9`–`0x1001d430`, writer at the start of `0x1001e920`; **confirmed** for the offsets, meanings as noted):

| Offset | Type | Player field | Meaning |
|---|---|---|---|
| `0x00` | `u32` (read as `u16`) | `+0x78`, property 9 | player type, see below |
| `0x04` | `u32` | `+0x74` | money (**likely**; 10,000 for player 31) |
| `0x08` | char[48] | `+0x24`, property 18 (`0x12`) | name, NUL-terminated |
| `0x38` | 16 bytes | `+0x64`, property 14 | player identity; the game-start code matches it against the 16-byte id of a network participant (**confirmed**), so it is a GUID-like id (non-zero in a few files, P) |
| `0x48` | `u32` | `+0x90` low 23 bits, property 33 | flags |
| `0x4C` | `u8` | `+0xA8`, property 11 | **colour slot** 1–8, `0xFF` = not assigned, 0 = none (§9) |
| `0x4D` | `u8` | `+0x95`, property 23 | ? |
| `0x4E` | `u16` | `+0xA0` (`& 0xFFBF`; ≥ `0x100` read as 0) | flags; 25 = `0x19` for native/AI players (P), the `VOLK` reader sets the same bits |
| `0x5C` | `u8` | bits 3, 2, 4, 5 → `+0x90` bits 24, 27, 29, 30 | flags |
| `0x5E` | `u16` | `+0x96` | ? |
| `0x60` | `u8` | `+0x28D` (7 is read as 8) | ? |

- **Player type** (`HEAD +0`), with the `VOLK` names that occur with it (P) and code that tests it:
  - 8 = human (`SPIELER`); 11 = human slot as well — the game-start and colour code treat 8 and 11 alike (`AnnoFrame` `0x10029100`, `0x1001d180`; **confirmed**); the difference between them is open.
  - 12 = computer opponent (`SPIELER`; gets a random `COMPSPIELER_NAME` if unnamed, §1).
  - 14 = native peoples (`AFRIKANER`, `AZTEKEN`, `BEDUINEN`, `ESKIMOS`, `INDIANER`, `MAUREN`, `MONGOLEN`, `SUEDSEE`); 15 = `PIRATEN`; 16 = `VENECIA`; 17 = `GOTT` / `BANDE`.
  - The `VOLK` reader (`0x1001d657`–`0x1001d753`, type switch table `0x1001e8f4`) sets the people number (`+0x7A`, property 28): 1 for types 8, 11, 12; 11 for type 15; 12 for type 16; otherwise the index of the `VOLK` name in a 20-name list. For types 15, 16 and people 2–12 it also sets the `+0xA0` bits `0x19` and colour 0. **Confirmed.**
  - 2 = player 31 (47 files; 3 campaign files use 8). **P.**
- **Player 31**: always last in the header series, type 2 (or 8), colour 0, name empty or `Player31`; in 9 add-on scenarios it owns soldiers, ships or cities in the state series (P). The game-start code skips index 31 (`0x1F`) in every loop that looks for human slots or assigns colours (`AnnoFrame` `0x10029100`, `0x1001d180`; **confirmed**). That it is the scenario's neutral/scripted owner is **likely**; no code that gives type 2 a specific behaviour was found.

## 7. Units (`SCHIFF`, `SOLDAT`) (`AnnoWelt.dll`)

Reader `0x1001dc7a` (`SOLDAT`) / `0x1001df31` (`SCHIFF`), writer around `0x1001f6d6`–`0x1001f788`. 16-byte prefix, **confirmed**:

| Offset | Writer source | Reader use |
|---|---|---|
| `0x00` `u16` id | unit id (slot `+0x40`) | kept, or replaced by a new id when the load context has flag `0x400` |
| `0x02` `u16` figure | figure entry id (slot `+0x78` → `+0x40`) | figure looked up; unknown figure → record skipped |
| `0x04` `u8` owner | the player's index | **not read**; the unit belongs to the enclosing `SPIELER` block |
| `0x05` `u8` attr | unit slot `+0x68` | set as figure property `0x16` |
| `0x06` `u8` | bit 0 = bit 3 of the unit's state flags | bit 0 set → world slot `+0x1BC` (unit, 0) |
| `0x07` `u8` | — | — |
| `0x08`, `0x0C` `f32` x, y | position minus a load origin | position plus the load origin; height from the terrain (world slot `+0x68`) |

- **`attr` = figure property `0x16`**, stored at figure `+0x0E` (`SetProperty` chain `0x10037e10` → `0x10049ad0` → `0x10035ca0`; getter `0x10034930`). **Confirmed.** At least one figure routine (`0x1004b360`, read at `0x1004b42d`) uses `+0x0E` as a facing in 45° steps: it rotates an offset by `sin`/`cos` of `45° · attr` from the table at `0x100c3148` (`0, 0.707, 1, 0.707, 0, −0.707, −1, −0.707`). So for soldiers (0–7) `attr` is the facing, **likely**; for ships (0–14) a 16-step facing is a **guess** (OQ-027 d).
- Figure property `0xB` is the figure's colour (`+0x90`, `0x10049b18`); a player pushes its colour to all its objects (`0x1001cdf0`). **Confirmed.**

## 8. Nested prefixes (`AnnoWelt.dll`)

- **`STADT2`** (6 bytes; writer `0x1001f3fd`–`0x1001f4bc`, reader `0x1001e294`): `u8` city number on its island, `u8` 0, `u16 x`, `u16 y` — a tile of the city (the writer takes the position of one of the city's buildings). The reader finds the island at (x, y) (world slot `+0x8C`; none → record skipped) and gets or creates city number `byte 0` of this player there (island slot `+0xF4`). **Confirmed.** P: all 272 positions lie on an island; numbers are distinct among the cities of one island.
- **`CREW/ENTRY`** (6 bytes; writer loop `0x1004c65f`–`0x1004c6b4`): `u16` crew member id, `u16` figure, `u8` slot index in the crew, `u8` 0. Empty slots are skipped, so the index can jump. **Confirmed;** P: byte 4 sequential in 393 of 407 crews, byte 5 always 0. (So byte 4 is not an owner.)
- **`ESEL`** (8 bytes; writer `0x10037288`–`0x1003733d`, reader `0x10037012`): `u16` figure (718 in all 29), `u16` id, `u16 x · 10`, `u16 y · 10` (scale `10.0` at `0x100c2220`; written as 0 when the owner's state flag `0x80` is set). **Confirmed;** P: the values equal the carrying unit's tile centre × 10 (e.g. 245.5 → 2455).

## 9. Player colours (`AnnoFrame.dll`, `AnnoWelt.dll`)

- **Stored per player** in `HEAD +0x4C` (§6), in scenarios and savegames (the writer copies `+0xA8` back). Corpus (P): humans 1–8 or `0xFF`, computers 1–8 or `0xFF`, types 2/14/15/16 → 0, type 17 → `0xFF`. **Not** derived from the player index; the viewer's `p + 1` is a stand-in only.
- Player property 11 (`0x10020b2f`): returns `+0xA8`; if that is `0xFF`, returns `0xFF` when the caller asks for the raw value, otherwise the default `+0xA9` (property 60, initialised to `0xFF`). Setting property 11 (`0x10020f91`) stores the value and, unless `0xFF`, recolours all the player's objects (`0x1001cdf0`). **Confirmed.**
- **Single player, colour screen** (`tFrameStartupEinzelFarbauswahl`, vtable `0x1008715c`): slot `+0x68` (`0x1001d700`) resets the chosen colour (global `0x10095240`) to `0xFF`, then `0x1001d180` pre-reads the scenario's player `HEAD`s; if there is exactly one human slot (type 8/11, index ≠ 31) and it has a preset colour, that colour is taken and the screen is skipped. Otherwise the colour buttons' callback `0x1001dfa0` stores the clicked colour in `0x10095240`. **Confirmed** (that the stored value is 1–8 is **likely**; the button-to-colour mapping is not traced).
- **Game start** `0x10029100` (**confirmed** unless marked): a pool of colours 1–8. New single-player game: the first human slot gets the local player's identity, name and colour `0x10095240`, which leaves the pool. Network game (participant list `0x1009c260`, chosen colour at participant `+0x3A`, colour screen `tFrameStartupMultiFarbauswahl`, vtable `0x100874cc`): human slots with a preset colour get the participant who chose that colour; remaining participants fill human slots without colour (**likely**, loop details not followed). Then for every player except 31 whose colour is still `0xFF`: human slot → removed (`0x10029080` deletes its objects and deactivates it); type 12 → the lowest colour still in the pool; any other type → 0. Colours preset in the scenario for computers are not taken out of the pool in this function, so a duplicate colour is possible (**likely**, no other pass seen). A loaded savegame keeps the saved colours.

## 10. Long record header (`AnnoWelt.dll`)

- Reader `0x100b4f50`: reads 6-byte name + `u16`; if the `u16` is `≥ 0xFF00`, its low byte is bits 16–23 of the length and the next `u16` is bits 0–15 (`0x100b4d80` returns `high · 65536 + low`). **Confirmed.** So the marker is a 24-bit length escape: `0xFF00` = high byte 0; `0xFFnn` would mean lengths up to 16 MiB. It is not a version flag, and the reader treats short and long forms alike.
- Writers: `0x100b4e80` writes the short form when the length fits (`< 0xFF00`, high part 0), else the long form `0x100b4df0`. Records written before their size is known reserve 8 bytes (`0x100b5060`) or 10 bytes (`0x100b5080`) and back-patch the header (`0x100b5000` / `0x100b5030`). The `STADT2` writer (`0x1001f3fd`) and the `PROD` writer (`0x1002f23f`) always reserve 10 bytes and back-patch with the long form, whatever the size. **Confirmed.** That explains why every `STADT2` and `STADT2/PROD` in the add-on files is long, even at 22 bytes; the base-game files were written by an older writer that used the short form (**likely**). The reader also still accepts the older record name `STADT` (**confirmed**, string test in `0x1001d240`).

## 11. AI profiles: `COMPPROFIL2`, the opponent setup and the hand-over to `CompLogic.dll`

Modules as named per item; all image bases `0x10000000`. Record layout and corpus: [`../formats/sze.md`](../formats/sze.md) "AI profiles"; the twelve shipped profiles: [`../research/game-files-resurvey.md`](../research/game-files-resurvey.md) §3.

### 11.1 Who reads and writes the chunk
- **The game's scene loader does not read it.** The game class's record handler `0x10009cc0` (§1) has cases for `HEAD`, `INSELLNK`, `ROHSTOFF`, `SCHATZ`, `ERZE` and `BIGBROTHER` only. Everything else goes to the world's chunk reader, and `AnnoWelt.dll` has no `COMPPROFIL2` string (S). That the world reader skips the chunk is therefore **likely** (the skip path itself was not traced).
- **The editor class** (kind 1, vtable `0x10031420`, no caller in the shipped modules, §1) keeps a profile list as a member at `+0x8064`; its record handler sends `COMPPROFIL2` there (`0x100058f5`). **Confirmed:**
  - The list class has vtable `0x100314e0`: read `0x10007110`, write `0x10007320`, count `0x10007440`, add `0x100074c0` (slot `+0x10`).
  - The `DEF` record sits at list `+4` (128 bytes). The `ENTRY` records are a vector of 168-byte elements at `+0x88`/`+0x8c`.
  - The reader builds a default record (§11.2), zeroes it, reads the payload over it and appends it.
  - The writer writes every `ENTRY` (168 bytes), then `DEF` (128 bytes), and wraps them in `COMPPROFIL2`.
  - The same class writes the per-player `COMP` records with the editor's key names (`0x100052b0`: loops over the parameter indices with a presence mask; `SETTRIBUTSPERRE` for index 25, see `sze.md` "COMP").
- **The single-player setup (`AnnoFrame.dll`) reads it straight from the scenario file**: `0x1001e5d0(apply)`, called with `apply = 1` from the colour screen's slot `+0x68` (`0x1001d71b`, §9) and from `0x10022db6`. **Confirmed:**
  1. Opens `Scenes\<folder>\<file>` and accepts root `ANNO` or `ANNOADDON1`.
  2. Clears the GUID of all 7 opponent slots (`0x1001e640`).
  3. Walks the `SCENE` children:
     - `HEAD`: two flags go to the globals `0x1009c068`/`0x1009c069`.
     - `COMPPROFIL2`, each `ENTRY`: default-constructs a record, zeroes it, reads 168 bytes and appends it to the profile vector (`this+0x534`, begin `+0x538`, end `+0x53c`; count byte `+0x544`).
     - `COMPPROFIL2`, `DEF`: if `apply` is set, enables all 7 slots (bytes `+0x94`…`+0x9a` = 1). Then, in player order, copies `ENTRY[DEF[i]]` into the next slot (`+0x9c + k·0xa8`) for every `DEF[i] ≠ −1`. Nothing checks the slot count against 7 or `DEF[i]` against the vector size.
  4. Returns whether any `ENTRY` was read.

### 11.2 The record and the GUID test (`AnnoFrame.dll`, `AnnoScene.dll`)
- **Record constructor**, inlined in the setup object's constructor `0x1001e140`, in the static initialiser of the global slot array `0x100172d0`, and in both readers: byte 0 = 1, bytes 1–3 = 0, columns 1–18 `4 0 2 3 1 18 0 0 15 3 5 5 0 0 60 0 0 0`, GUID `{E861A553-AFA5-4C53-BAEB-6AF500000000}`, name `Default`, page `Default.htm`. **Confirmed.** Both file readers zero the record after constructing it, so a file record never inherits these values.
- **GUID test** `0x10019ff0`: false if the 16 bytes at record `+0x4c` are all zero or all `0xFF`, compared as `u32, u16, u16, 8 × u8`. **Confirmed.** So `+0x4c` is a GUID and "no profile" is an invalid GUID.

### 11.3 The opponent setup screen (`AnnoFrame.dll`)
- **Setup object** (constructor `0x1001e140`, vtable `0x10087234`):
  - 7 opponent slots of 168 bytes at `+0x9c`, with enable bytes at `+0x94`…`+0x9a` (all 1 after construction);
  - the profile vector at `+0x534`;
  - the current slot at `+0x8c` and the profile cursor at `+0x90`.

  **Confirmed.**
- **Arrows** `0x1001f7f0` (previous) and `0x1001f860` (next): step the cursor through the profile list and, while it points at a profile, copy that profile into the current slot. Then `0x1001f8d0` opens `<game dir>\Profiles\Texte\<page>` with the slot's page name (`+0x75`). **Confirmed.** That this shows the character sketch on the screen is **likely** (the HTML view was not followed).
- **Commit** `0x1001fad0`: clears the GUID of every slot whose enable byte is 0, then copies all 7 slots to the global array `0x1009bbd0` (1,176 bytes). **Confirmed.**
- **Network:** `0x1001fd10` runs the commit and, in a network game, sends message 6 with the 1,180 bytes at `0x1009bbd0` (the 7 slots plus the two `HEAD` flags at `0x1009c068`). The message handler `0x10019880` (case 6 at `0x10019a92`) copies them back to `0x1009bbd0`. **Confirmed.** It is the same handler whose case 0 adopts the random seed (§4). So in a network game every machine starts the AI from the same profiles (**likely**: which side sends was not traced, as for the seed).

### 11.4 Hand-over at game start (`AnnoFrame.dll` `0x10028d90`)
Called from the game start `0x10029100` at `0x10029372` and `0x1002967b`. Both calls come before that function's pass that gives each computer player at `0xFF` the lowest free colour (`0x10029b3a`, §9). For every player in index order (**confirmed**):
1. Skip player 31 (`0x10028de2`), every player whose type (property 9) is not 12, and every player whose colour (property 11) is not `0xFF`.
2. Take the player's AI object from player property 44 (`0x2c`). In `AnnoWelt.dll` this is player field `+0xc`: getter `0x10020ab9`, setter `0x10020f27`.
3. Take the next global slot.
4. If the slot's GUID is invalid (§11.2), call AI slot `+4` with parameter 34 and no message (`0x10028e63`). That only sets a flag (§11.5).
5. Otherwise copy columns 1–18 and call the parameter setter (AI vtable slot `+4`, message field `+0x18` = value) 15 times, in this order:

| Call | Column | Parameter | Address |
|---|---|---|---|
| 1 | 1 | 0 `TYP` | `0x10028e8a` |
| 2 | 4 | 3 `HANDEL` | `0x10028e9e` |
| 3 | 5 | 6 `TRANSPORT` | `0x10028eb4` |
| 4 | 2 | 1 `FLAECHE` | `0x10028eca` |
| 5 | 3 | 2 `LAGER` | `0x10028ee0` |
| 6 | 10 | 7 `HAUPTKLIMA` | `0x10028ef6` |
| 7 | 11 | 8 `MAXSTADT` | `0x10028f0c` |
| 8 | 12 | 9 `ZIELGRUPPE` | `0x10028f22` |
| 9 | 9 | 18 `KAMPFTYP` | `0x10028f38` |
| 10 | 16 | 13 `MINBAUMAT` | `0x10028f4e` |
| 11 | 17 | 14 `MINEISEN` | `0x10028f67` |
| 12 | 6 | 4 `PROZENTE` | `0x10028f80` |
| 13 | 8 | 25 `TRIBUTSPERRE` | `0x10028f96` |
| 14 | 18 | 26 `SUSPEND` | `0x10028fac` |
| 15 | 13 (value) and 14 (second value, message `+0x14`) | 26 `SUSPEND` again | `0x10028fc5`–`0x10028fe0` |

- Columns 0, 7 and 15, the name and the page are not passed. **Confirmed.**
- Call 15 overrides call 14 with column 13. Its message shape (two values, at `+0x18` and `+0x14`) is the shape of parameter 17 `CHEATLIMIT`, whose setter stores both (`0x10004768`). The `COMP` key reader also builds this shape for `CHEATLIMIT=a,b` (`0x100016df`–`0x1000170e`). So index 26 instead of 17 is **likely** a slip in the original. With the shipped values (all 0) it changes nothing (OQ-112).
- Because only type-12 players with colour `0xFF` take slots, the slots go to `DEF`'s players in `DEF` order. The corpus has no file where these sets differ (P). A computer player with a preset colour keeps the values of its `COMP` record. So does a scenario without `ENTRY` records: its slots carry no valid GUID, and each computer only gets parameter 34. This assumes the setup reader `0x1001e5d0` ran for the scenario (**likely**). The global slots start out as default records with a valid GUID (`0x100172d0`), and whether any start path skips the reader was not traced.

### 11.5 The AI side: objects, factory, parameter setter (`CompLogic.dll`)
- **Loading:** `AnnoWelt.dll` `0x1009c630` loads `CompLogic.dll` if world property 14 has bit 1 set (`0x1009c681`; the `ANNOADDON1` root sets it, §1), otherwise `CompLogicClassic.dll`. It calls `CompCreateInstance` (`0x100163b0`), which builds the manager (`0x1003d590`, vtable `0x100634f8`). **Confirmed.** This answers the selection half of OQ-018.
- **Per-player AI object:** the player reader for `COMP` (`AnnoWelt.dll` `0x1001e3be`) works in three steps. **Confirmed:**
  1. Reads the 8-byte prefix (type, index).
  2. Calls manager slot `+0x1c` = factory `0x1003dc50` with them and stores the result in player property 44.
  3. Lets the new object read the key text (object slot `+0x18`).
- **The factory switches on the player type** (jump table `0x1003de68`). **Confirmed.** It stores the type at object `+0xa18` and the index at `+0xa14`. Classes by type:

| Type | Constructor | Vtable | Size | Player kind |
|---|---|---|---|---|
| 11 | `0x1004ed60` | `0x100637f0` | `0x2420` | human-slot type (§6) |
| 12, 13 | `0x1003ec80` | `0x10063550` | `0x1d98` | computer opponent; the constructor sets `KAMPFTYP` = 15 (`0x1003ed3f`) |
| 14 | `0x100516a0` | `0x100638b0` | `0x1d74` | native peoples |
| 15 | `0x10053660` | `0x10063a00` | `0x1d9c` | pirates |
| 16 | `0x1004c0c0` | `0x10063680` | `0x1d8c` | Venetians |
| 17 | own case `0x1003de00` | — | — | not followed |

- **Parameter setter** `0x10004550` = vtable slot `+4`, the same in every class above and in the base vtable `0x10063100`. **Confirmed.** It takes `(index, message)` with the value at message `+0x18` and switches over 36 indices (jump table `0x100049e0`). A value outside a case's limits is ignored, and so is an index above 35.
- **Key names come from three independent places** and agree on every index they share. **Confirmed:**
  - the AI's `COMP` key readers `0x10001450` (base) and `0x10040b00` (computer class);
  - the editor's `COMP` writer (`AnnoScene.dll` `0x100052b0`);
  - the script's `SET_PROFILE` parser (`AnnoScene.dll` `0x10019b4e`–`0x10019f3b`, the `COMPCAPS_*` names).

The names are German words (`Lager` storage, `Handel` trade, `Prozente` percent, `Kampftyp` combat type, `Hauptklima` main climate, `Zielgruppe` target group, `Mindest-Baumaterial`/`-Eisen` minimum building material/iron, `Tributsperre` tribute block, `Fläche` area). Read them as labels, not as established meaning (OQ-113).

| Index | `COMP` key | Script name | Setter effect (object field) | Profile column |
|---|---|---|---|---|
| 0 | `TYP` | `COMPCAPS_TYP` | 0–7 only: `+0xa34`; slot `+0x24` (`0x100044b0`) sets `+0xaf0` = `+0xaf4` = 47,500 / 37,500 / 42,500 / 42,500 / 52,500 / 42,500 / 52,500 / 57,500 for 0…7 | 1 |
| 1 | `FLAECHE` | — | `+0xa70` | 2 |
| 2 | `LAGER` | `COMPCAPS_LAGER` | 0–3: `+0xa3c` | 3 |
| 3 | `HANDEL` | `COMPCAPS_HANDEL` | 0–3: `+0xa40` | 4 |
| 4 | `PROZENTE` | `COMPCAPS_PROZENTE` | 5–30: `+0xa44`; floats `+0xa8c` = 1 + v·0.01, `+0xa88` = 1 − v·0.01 (constant `0x10063298`) | 6 |
| 5 | `OPTIONAL` | — | `+0xa6c` | — (OQ-112) |
| 6 | `TRANSPORT` | `COMPCAPS_TRANSPORT` | 0–3: `+0xa48`; non-zero also calls `0x10004460` | 5 |
| 7 | `HAUPTKLIMA` | — | `+0xa4c` = 4 (v = 0), 16 (v = 1), 20 (else) | 10 |
| 8 | `MAXSTADT` | `COMPCAPS_MAXSTADT` | 0–8: `+0xa50` | 11 |
| 9 | `ZIELGRUPPE` | `COMPCAPS_ZIELGRUPPE` | 0–4: `+0xa54` | 12 |
| 10 | `ZIELANZAHL` | — | accepted, nothing stored | — (OQ-112) |
| 11 | — | `COMPCAPS_FIXDIPLOMATIE` | calls `0x10008690(message)` | — |
| 12 | — | `COMPCAPS_ADDDIPLOMATIE` | calls `0x10008690(message)` (same routine; how 11 and 12 differ is not traced) | — |
| 13 | `MINBAUMAT` | `COMPCAPS_MINBAUMAT` | `+0xa60` | 16 |
| 14 | `MINEISEN` | `COMPCAPS_MINEISEN` | `+0xa64` | 17 |
| 15, 16 | — | — | call `0x100060d0` / `0x100061f0` with the message | — |
| 17 | `CHEATLIMIT` (two values `a,b`) | — | `+0xa58` = message `+0x18` (b), `+0xa5c` = message `+0x14` (a) | — (OQ-112) |
| 18 | `KAMPFTYP` | `COMPCAPS_KAMPFTYP` | `+0xa38` = v & 15; then `0x10004df0` (§11.6) | 9 |
| 19–21 | — | — | call `0x10005e50`, `0x10005730`, `0x10005940` | — |
| 22, 23, 27 | — | — | ignored | — |
| 24 | — | `COMPCAPS_SETHELPANLAGE` | type 11 only: `+0xa2c`, then `0x100043e0` | — |
| 25 | `TRIBUTSPERRE` (editor: `SETTRIBUTSPERRE`) | — | `+0xa68` | 8 |
| 26 | `SUSPEND` | `COMPCAPS_SUSPEND` | 0: `+0xa80` = 0, byte `+0x1a89` = 1; else `+0xa80` = 1, byte `+0x1a89` = 0 and slot `+0x20` is called | 18 (then 13, §11.4) |
| 28 | `NATIVEBAUMODE` | — | `+0xafc` | — |
| 29 | `SPEZFLAG` | — | `+0xa84` (flag word) | — |
| 30 | `FIXHAUSANZ` (computer class reader) | — | non-zero: `+0xa74`; 0: a value looked up via `0x1000c3d0`, and `+0xa84` bit 2 set | — |
| 31 | `STADTOFF` (computer class reader) | `COMPCAPS_BAULOGICOFF` | calls `0x10003750(v)` | — |
| 32 | — | `COMPCAPS_PIRATEATTACKALL` | `+0xa84` \|= 8 (value ignored) | — |
| 33 | — | — | slot `+0x50` with the message's first `u16` | — |
| 34 | — | — | `+0xa7c` = 1 (sent for an opponent without a valid profile, §11.4) | — |
| 35 | — | — | type 11 only: `+0xa78` = (v ≠ 0) | — |

The 16 `COMPCAPS_*` names cover indices 0, 2, 3, 4, 6, 8, 9, 11, 12, 13, 14, 18, 24, 26, 31 and 32. Of the 19 profile columns, 15 reach the setter, at 14 indices: columns 13 and 18 both go to index 26, and column 14 only rides along as an unused second value. **Confirmed.**

### 11.6 What some parameters steer (`CompLogic.dll`)
- **`KAMPFTYP` (column 9)** is two 2-bit switches. **Confirmed** for the code paths; that the tasks are military ones is **likely** from the name and from the profiles: the Trader page "detests war" and has 0, the Warrior has 11, the Tactician 15.
  - *Creating tasks:* `0x10003680` acts only for a type-12 AI (`+0xa18` = 12). If bits 0–1 are non-zero and no task of kind 6 exists (`0x100037a0`), it creates one (`0x100037f0`). Bits 2–3 do the same for task kind 8.
  - *Capping tasks:* the planner at `0x10043233` returns at once when bits 2–3 are 0. Otherwise it counts two sorts of entries in the AI's 12-entry task table (`+0x1ad4`, 28 bytes each). It returns early once a count reaches 1 (bits 2–3 = 4 or 8) or 2 (= 12).
  - *Cancelling tasks:* writing the parameter calls `0x10004df0`, which cancels the table entries of the kind whose bits are now 0. Entries without flag bit 1 belong to bits 2–3; entries with it belong to bits 0–1.
  - *Two further tests:* `0x1004354d` tests bit 3 and `0x10043e4a` tests bit 1; not followed.
- **`PIRATEATTACKALL`** (script only, `single34.sze`, 6 uses) sets bit 3 of the flag word `+0xa84`. **Confirmed:** the pirate class (vtable `0x10063a00`, slot `+0x2c` = `0x10055a30`) calls `0x100558b0` three times. That routine walks candidate objects and drops each one that fails the check `0x10028bd0(candidate, TYP)`. When bit 3 is set it skips this check (`0x1005593f`). **Likely:** the pirates then attack every candidate instead of a TYP-filtered set.
- **`TYP` (column 1)** selects the pair `+0xaf0`/`+0xaf4` (table above). Several AI routines pass both values with a time value to `0x1003ea50`, e.g. `0x10042357` (the time comes from `0x1003d240`). That routine divides elapsed time by them while it updates a 32-entry per-player table of signed byte values. **Confirmed** for the arithmetic. **Likely:** a decay rate of the AI's per-player attitude; the time unit is not traced. The field `+0xa34` itself occurs 47 times in the module, among them in the pirate target check above.
- **`PROZENTE` (column 6)** gives the AI two factors, 1 ± v/100. **Confirmed.** A price margin is a **guess** (OQ-113).
- **`SUSPEND`:** a non-zero value sets `+0xa80`, clears the byte `+0x1a89` and calls the AI's slot `+0x20`. **Confirmed.** That this pauses the AI is **likely** from the name. `single41.sze` sets it for 12 computer players by script.

### 11.7 The script route (`SET_PROFILE`, `AnnoScene.dll`)
- The action parser (`0x10019a04`) handles `SET_PROFILE("<player>", "COMPCAPS_<key>", <value>)` and maps the name to the index of §11.5 (`0x10019b4e`–`0x10019f3b`). **Confirmed.** An unknown name gives the error `%s ist ein unbekanntes PROFIL bei Bedingung: %d`.
- The action object (vtable `0x1003b874`) stores the player at `+4`, the value at `+8` and the index at `+0xc`.
- Its execute method `0x10015e80` takes the player's AI object from player property 44 and calls the same setter (slot `+4`). It puts the value in message `+0x18`. For indices 15 and 16 it puts the value in `+0x10` and 25 in `+0x18` instead; no script name maps to 15 or 16. **Confirmed.**
- The shipped scripts use it 22 times (P): `COMPCAPS_SUSPEND` 12 × in `single41`, `COMPCAPS_PIRATEATTACKALL` 6 × in `single34` (value `TRUE`), `COMPCAPS_KAMPFTYP` 2 × each in `S01_02_01` (1, 5) and `S01_07_01` (0, 2).

### 11.8 `CompLogicClassic.dll`
- Its parameter setter `0x10002e50` (jump table `0x100032d0`) has the same 36 cases with the same object fields and the same value limits as `CompLogic.dll`. It differs only in the called routines and in the `SUSPEND` byte (`+0x1a19` instead of `+0x1a89`). **Confirmed.** So the profile columns mean the same in both modules.
- It has the same key strings minus a few (S, `game-files-resurvey.md` §3). Its own readers and its behaviour behind the fields were not compared.

## Open
- §11: `ENTRY` columns 0, 7, 13, 14, 15 and the doubled `SUSPEND` call (OQ-112); what the AI parameters do beyond the setter (OQ-113); the lost `SETTRIBUTSPERRE` values (OQ-114); which machine sends network message 6; the type-17 AI case (`0x1003de00`).
- The meaning of `INFO` byte 2 ("special") and byte 5 (capacity vs. mountain/ore slots); the size classes and climate masks of the raw-material and ore distribution (`0x1003af74`, `0x10009010`, `0x1003af40`).
- What happens to a link without any candidate after the capacity filter (§5 step 10).
- Whether a savegame stores the world random cursor (property 8), and when the reset `0x100af240` runs relative to a scene load.
- Network seed: which side sends message 0; what start path `0x100206c0` is.
- Player types 11 vs 8, and type 2 behaviour; `HEAD` fields `+0x4D`, `+0x5E`, `+0x60`, flags at `+0x48`/`+0x4E`/`+0x5C`.
- Ship `attr` range 0–14: 16-step facing? Trace the ship class's draw/frame selection.
- Colour button → value mapping on the colour screens; whether computer colours can collide with preset ones in practice.
- Whether anything reads `INFO` bytes 6–7 (area / 64) or byte 1.
