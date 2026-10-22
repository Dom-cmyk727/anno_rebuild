# `AnnoScene.dll` — scenario loader, island links, players, player colours

**Status:** first pass (2026-10-17), static analysis only. Modules: `game/app/AnnoScene.dll` (scene loader), with the parts it hands over to in `AnnoWelt.dll` (world, players, units, record I/O) and `AnnoFrame.dll` (game start, colour selection). GOG 2.0.0.5, image base `0x10000000` for all three. Tools: Ghidra 12.1.4 headless (`cache/ghidra/plus/*.c`), `objdump -d`, RTTI resolved by hand (type descriptor → complete-object locator → vtable). Addresses are virtual addresses in the module named in each section. Nothing here is copied code; behaviour is paraphrased. Corpus checks use `tools/sze.py` / `tools/chunks.py` over the 50 `.sze` and the 465 island templates (marked **P**).

Confidence: **confirmed** = read in the decompiled/disassembled function; **likely** = strong inference, not traced end to end; **guess** = hypothesis, also listed under "Open".

Answers in short: OQ-028 — a link naming a `*DUMMY.scp` is replaced at game start by a random real template of the same climate and size (§3–§5). OQ-027 — player type, colour and identity fields of `HEAD`, player 31, unit `attr`, and the `STADT2`, `CREW/ENTRY` and `ESEL` prefixes: §6–§8. Player colours: §9. Long record header: §10. Script kernel (OQ-120 `&&`/`||`, OQ-121 missing `;`): §12.

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

## 12. The script kernel: statement reader, condition parser, evaluation (`AnnoScene.dll`)

T19, 2026-10-22. Static analysis with capstone (`tools/disasm.py` and range dumps), Windows host. The reader and the parser lie outside the O-16 op range (`0x10011ed0`–`0x10013dab`, `docs/sim/state.md`). **D** = read in the disassembly at the address given; **P** = checked over the 50 shipped scripts with `tools/bigbrother.py` and a scratch emulation of the reader below (not committed). Answers OQ-120 and OQ-121; spec: `docs/formats/bigbrother.md` §3.4.

**Answers in short.**
- **`&&` and `||` have equal rank and group to the right:** `a op1 b op2 c` is `a op1 (b op2 c)`. The first connective takes the whole rest of the condition as its right operand. So `A && B || C` is `A && (B || C)`, not C's `(A && B) || C`.
- **Evaluation:** left operand first. `&&` skips its right operand when the left is false; `||` evaluates both.
- **A block statement without `;`** runs on to the next `;` anywhere in the kernel text. The text in between is read into it and lost. In the shipped scripts that loses one action: `MINIMAP_ALERT( 640, 950 )` in S01_09_01.

**Text helpers (D).** All C-runtime calls are statically linked:
- `_stricmp` `0x10027310`, `strncmp` `0x10027550`, `strstr` `0x10027640`, `strchr` `0x100273f0`, `strpbrk` `0x10028020`, `strncpy` `0x100280f0`, `isalnum` `0x10027716`, `isalpha` `0x100276c0`.
- **skip** `0x1000b150`: stops at a letter or digit or at one of `" ; [ ( & | < > - ! =`. It passes over everything else: blanks, line ends, `{`, `}`, `)` and `,`. A `$` comment is skipped to the end of its line or to a `[` (`0x1000b210`, `0x1000b1f0`).
- **word** `0x1000b280`: copies letters, digits and `_ | & < > ! =`. So `&&`, `||` and `==` are words when blanks surround them.
- **first connective** `0x1000b4f0`: the earlier of `strstr(p, "&&")` and `strstr(p, "||")`.
- **`strpbrk` wrappers:** `{` `0x1000b4b0`, `}` `0x1000b4d0`, `;` `0x1000b530`.
- **THEN finder** `0x1000b420`: `strncmp(…, "THEN", 4)` at every position, accepted only before the first `{`.
- The delimiter table `0x1003d888`–`0x1003d8a0` is these helpers' strings.
- **Error messages** (message box `0x10031144`) appear only while the script object's byte `+4` is set; the refusals happen either way.

**The kernel reader** `0x1001c9e0`, called by the section reader at `0x10016fb6`. It gets a copy of the kernel text up to the next `[` (`0x1000b3f0`, call at `0x10016f76`). **D:**
1. **`SCHUETTIINITSIG`.** If the signal does not exist, it is created with the value 1 (`0x1001c9f0`–`0x1001ca64`).
2. **Main loop** (`0x1001ca8f`–`0x1001cac8`, `0x1001d21d`): skip, word, skip. Without progress the reader moves one byte on. The word is compared with `_stricmp`:
   - `IF` (`0x1001cad9`);
   - `INITBLOCK` (`0x1001ce13`);
   - `SET` (`0x1001cf30`);
   - `ELSE` (`0x1001d1c4`): a lone `ELSE` reports `"ELSE hat keine zugehörigest IF"` (`0x1003f824`) and the reader goes on.

   Any other word, and a top-level `;`, is passed over without a message.
3. **`IF`** (`0x1001cae9`):
   - **Count:** it increments the condition counter `+0x74`, the `%d` of the error messages.
   - **`THEN`:** without one (`0x1000b420`), the reader reports `"Kein THEN bei Bedingung: %d"` (`0x1003f804`) and returns 0, which refuses the whole kernel.
   - **Condition:** the text between `IF` and `THEN` is copied. Each LF becomes a blank (`0x1001c9c0`; the skip passes over CR), and the copy goes to the condition parser (`0x1001cb55`); a null result refuses the kernel.
   - **Block bounds:** the block is the first `{` and the first `}` after `THEN` (`0x1001cb76`, `0x1001cb7e`). Neither is matched for nesting or skipped in comments.
   - **Statements** (`0x1001cbb0`–`0x1001cc7b`). From the current point:
     1. skip; at or past the `}`, the block ends;
     2. otherwise the statement is the text up to `strpbrk(p, ";")` (`0x1001cbca`), searched in the rest of the kernel text, not in the block. No `;` at all reports `"FunktionsBlock nicht oder falsch definiert bei Bedingung: %d"` (`0x1003f7c4`) and refuses the kernel;
     3. the copy goes to the action reader `0x100170d0` (`0x1001cc1e`). Null refuses the kernel. The action joins the condition's THEN list (condition slot `+0x18`, `0x1001cc3c`);
     4. the next statement starts after that `;`.
   - **Resume point:** the reader goes on after the block's `}` in every case (`0x1001cb9c`).
   - **`ELSE`** (`0x1001cc81`–`0x1001cdb5`): the reader reads the next word; on `ELSE` the same loop fills the ELSE list (slot `+0x20`), and the reader resumes at that block's `}`.
   - **The IF** joins the list `+0x30`; the count is at `+0x38` (`0x1001cdbc`–`0x1001ce01`).
4. **`INITBLOCK`** (`0x1001ce09`–`0x1001cf21`): the same block loop; the actions go to the list `+0x14`.
5. **`SET`** (`0x1001cf26`–`0x1001d16c`):
   - **Name:** the next word, at most 34 characters (`0x1001cf84`, `"Name … zu lang"` `0x1003f93c`). It must not exist yet (`0x1003f864`).
   - **Value:** if a `=` follows, the next word must be `TRUE` or `FALSE` (`0x1001d076`, `0x1001d093`). Anything else reports `"Unbekannter Operator %s!"` (`0x1003e700`) and refuses the kernel; an empty value reports `0x1003f8f8`. Without `=` the value is FALSE (`0x1001cfd7`).
   - **End:** the reader goes on at the next word after the value. A `;` is not looked for, so a top-level `SET` without one loses nothing.

**The action reader** `0x100170d0`. It reads the name, then compares it with each action name in turn (`GOTO_SCREEN` first, `0x10017116`). Each branch reads its own arguments: `(` skipped by one byte, strings by `0x1000b370`, numbers by `strtol` (`0x1000b2e0`, `0x1000b310`), commas by one byte. The branch then builds the action and returns; the text after the last argument is never looked at. **D** for the four branches the corpus needs:
- `SET_FOR_BUY` (`0x1001c2ea`–`0x1001c5e2`);
- `SET_PROFILE` (`0x10019a1d`–`0x10019f92`);
- `SET_DIPLOMACY` (`0x1001bcdc`–`0x1001be8d`);
- `QUESTLOG_DRAW` (`0x100180c9`): it takes no argument, so `QUESTLOG_DRAW;` and `QUESTLOG_DRAW();` are the same.

**The condition parser** `0x10022bf0` (`this` = the script object; argument = the text). Loop (`0x10022c37`): skip, word, skip. **D:**
1. **Word `&&`** (`0x10022c52`) or **`||`** (`0x10022c6e`):
   - without a left operand it reports `"Syntaxfehler : Unbekannter Befehl bei Bedingung: %d"` (`0x1004018c`) and returns 0;
   - otherwise the **whole rest of the text** is copied and parsed by a recursive call (`0x10022fbd` / `0x10023091`). That is the right operand;
   - the node (`0x28` bytes; vftable `0x1003b6f8` at `+0x24`) gets the left operand at `+0x14`, the right at `+0x18` and the connective (0 = `&&`, 1 = `||`) through slot `+0x28` (`0x1001c8d0`, byte `+0x1c`);
   - **the function returns at once.**
2. **`(`** (`0x10022c84`): the matching `)` is found by counting depth (`0x10022c9a`–`0x10022cb1`); none reports `"Syntaxfehler : ')'"` (`0x100401c0`). The inside (with the `)`) is parsed recursively (`0x10022d01`) and becomes the current left operand.
3. **Otherwise a term:**
   - its arguments are the text up to the first connective after it (`0x1000b4f0` at `0x10022d22`), or to the end;
   - by the word: `PLAYER` `0x1001d650`, `AREA` `0x100201a0`, `OBJECT` `0x10021280`, `TIMER_0`…`TIMER_4` `0x10022610` (index as argument), `PLAYTIME` `0x100227b0`; any other word is a variable, `0x10022940`;
   - the term becomes the current left operand (`0x10022f56`), and the loop goes on at the connective.

So the grammar is `cond = term [ ("&&" | "||") cond ]`, `term = "(" cond ")" | primary`. There is no rank between the connectives; the right side is everything after the first one. P: the emulation of this parser over the 50 scripts gives the same terms as `tools/bigbrother.py` in all 308 conditions.

**Evaluation** (`0x10010290`: vftable slot 1 of the connective, through the thunk `0x1001c840`; argument = the world). **D:**
1. **Left operand:** its slot 1 is evaluated first.
2. **`&&` with a false left:** the function returns false at once (`0x100102a8`–`0x100102b4`). The right operand is not evaluated, and neither block runs.
3. **Otherwise** the right operand is evaluated (`0x100102c1`), also for `||` with a true left. The two values are combined.
4. **Result:** true runs slot 2, the THEN list (`0x10010070`); false runs slot 3, the ELSE list (`0x100100a0`).

**Leaves** do the same:
- **Variable leaf** (`0x10010840`, vftable `0x1003bb58`): `== FALSE` sets a negate flag at parse time (`0x10022aee`), and a bare name means `== TRUE`.
- **Then** THEN or ELSE runs (`0x10010885` / `0x10010890`).

Only the IF's top node has actions in its lists, so the THEN/ELSE runs of inner nodes do nothing.

**Consequence:** an `IF a && … THEN … ELSE …` runs neither block while `a` is false. P: the three shipped `ELSE`s (single33) all start with `CHKPARTn &&`.

**Whether evaluating a condition changes state** (for example, consumes an event) was not traced. So it is open whether the skipped right operand of `&&`, or the always-evaluated one of `||`, matters.

**The step** `0x10016b70` (vftable `0x1003b7b0` slot `+0x20`, the call in AnnoFrame `0x1005fe2e`; argument at `+0x70`). **D:**
1. **Pause:** the step returns at once while the global byte `0x10043438` is set.
2. **First call** (IF cursor `+0x40` empty): if `SCHUETTIINITSIG`'s value is non-zero, the INITBLOCK actions run (list `+0x14`, slot 1 with the world `+0x78`). The signal is then set to 0 (`0x10016b90`–`0x10016bbe`), and the cursor starts at the first IF.
3. **The IFs:** each call evaluates `n` of them round-robin from the cursor, wrapping at the end (`0x10016bf9`–`0x10016c1e`). `n = min(count, ((arg − [+0x3c]) · count + 125) / 250)` (`0x10016bcc`–`0x10016bf3`); a negative value gives `count`.
4. **`+0x3c`:** among the direct writes, only the constructor `0x100160b0` writes it (0).

So the INITBLOCK runs once, before any IF, in the first step, and only while `SCHUETTIINITSIG` is TRUE. That is what the flip `TRUE` → `FALSE` in the quicksaves shows (OQ-123 item 4). What the argument is (AnnoFrame adds it to `+0xa24`, `0x1005fe31`) and how many IFs a step evaluates in practice is open.

**The shipped scripts under this reading** (P; `python -I tools/bigbrother.py quirks <files>`):
- **The 28 mixed conditions** (10 files) all have the shape `guard && … && alt1 || alt2 || …`. With one parenthesised group, Single_01's shape is `MISSION_FLAG == FALSE && (… DEAD && … DEAD && … DEAD) || … > 2999`.
- **What the interpreter reads:** `guard && (… && (alt1 || alt2 …))`. The guards cover every alternative.
- **Against C precedence** each of the 28 is a different function: the alternatives after the first `||` would fire without the guard. For example, the Single_01 rule would fire again after `MISSION_FLAG` is set.
- **Read as the authors meant:** the one-shot flags (`ONETIME`, `PARTn`, `END_FLAG`, `DEADEND`) guard all alternatives.
- **Statements without `;`:**
  - `SET_PROFILE( "CG_ENEMY", "COMPCAPS_KAMPFTYP",  5  )` (S01_02_01) reads on to the `;` of the commented-out `$ATTACK_OBJECT(…);` a few lines below, inside its own block. Only comments are swallowed, so nothing is lost;
  - `SET DEADEND_ABFRAGE = FALSE` (S01_08_01) is a top-level `SET`. Nothing is lost;
  - `SET_DIPLOMACY( "CG2", "HUMAN", -127 )` (S01_09_01, the last statement of the INITBLOCK) reads on past the `}` to the `;` of `SET TEIL1_1 = FALSE;`. The INITBLOCK resumes at its `}`, so that `SET` is read again normally. Nothing is lost;
  - `SET_FOR_BUY( "HEILKRAEUTER", "INDIANS", 30 )` (S01_09_01) reads on to the `;` of the next statement, `MINIMAP_ALERT( 640, 950 );`, in the same THEN block. **That minimap alert is lost**; the THEN block has 8 actions, not 9.

## Open
- The meaning of `INFO` byte 2 ("special") and byte 5 (capacity vs. mountain/ore slots); the size classes and climate masks of the raw-material and ore distribution (`0x1003af74`, `0x10009010`, `0x1003af40`).
- What happens to a link without any candidate after the capacity filter (§5 step 10).
- Whether a savegame stores the world random cursor (property 8), and when the reset `0x100af240` runs relative to a scene load.
- Network seed: which side sends message 0; what start path `0x100206c0` is.
- Player types 11 vs 8, and type 2 behaviour; `HEAD` fields `+0x4D`, `+0x5E`, `+0x60`, flags at `+0x48`/`+0x4E`/`+0x5C`.
- Ship `attr` range 0–14: 16-step facing? Trace the ship class's draw/frame selection.
- Colour button → value mapping on the colour screens; whether computer colours can collide with preset ones in practice.
- Whether anything reads `INFO` bytes 6–7 (area / 64) or byte 1.
- Script step (§12): what the step's argument is and how many IFs a step evaluates in practice; whether `+0x3c` is written anywhere else; whether evaluating a condition changes state (it would make `&&`'s skip and `||`'s full evaluation observable).
