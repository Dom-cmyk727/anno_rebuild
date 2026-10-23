# `AnnoScene.dll` — scenario loader, island links, players, player colours

**Status:** first pass (2026-10-17), static analysis only. Modules: `game/app/AnnoScene.dll` (scene loader), with the parts it hands over to in `AnnoWelt.dll` (world, players, units, record I/O) and `AnnoFrame.dll` (game start, colour selection). GOG 2.0.0.5, image base `0x10000000` for all three. Tools: Ghidra 12.1.4 headless (`cache/ghidra/plus/*.c`), `objdump -d`, RTTI resolved by hand (type descriptor → complete-object locator → vtable). Addresses are virtual addresses in the module named in each section. Nothing here is copied code; behaviour is paraphrased. Corpus checks use `tools/sze.py` / `tools/chunks.py` over the 50 `.sze` and the 465 island templates (marked **P**).

Confidence: **confirmed** = read in the decompiled/disassembled function; **likely** = strong inference, not traced end to end; **guess** = hypothesis, also listed under "Open".

Answers in short: OQ-028 — a link naming a `*DUMMY.scp` is replaced at game start by a random real template of the same climate and size (§3–§5). OQ-027 — player type, colour and identity fields of `HEAD`, player 31, unit `attr`, and the `STADT2`, `CREW/ENTRY` and `ESEL` prefixes: §6–§8. Player colours: §9. Long record header: §10. AI profiles (`COMPPROFIL2`), the opponent setup, the hand-over to `CompLogic.dll` and the AI parameter table: §11 (added 2026-10-22, T18; static analysis with `tools/disasm.py` and a capstone linear sweep, no Ghidra on this host). Script kernel (OQ-120 `&&`/`||`, OQ-121 missing `;`): §12.

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
1. Reads the 12-byte head (`u16 x, u16 y, u16 width, u16 height, u32 number`) and the path into an island slot table at `this+0x16C + 0x50 · (number & 0xFFFF)` (`0x10009e58`–`0x10009e74`; 256 entries, `0x16C`–`0x516C`, cleared by the constructor's loop at `0x10007a35`; path at slot `+4`; accessors `0x1000a5f0` set / `0x1000a640` get).
2. If the last 9 characters of the path equal `DUMMY.scp` (case-insensitive, `_stricmp` at `0x10027310`; the pointer is `path + strlen − 9`, `0x10009eb1`): takes the file name after the last `\` (`strrchr`, `0x10009ec9`; the whole path when there is none), finds the climate by a case-insensitive prefix match (`_strnicmp` `0x10028330` with the prefix's own length) against the table at `0x1003b328` — `po_` 0, `t_` 1, `n_` 2, `pr_` 3, `d_` 4, `s_` 5, `w_` 6, first match wins — and appends a 334-byte link record at the tail of the list at `this+0x53B8`/`+0x53BC` (count `this+0x53B0`, `0x10009f84`–`0x10009fd3`). A name that matches no prefix keeps climate 0 (the record is zero-filled and the climate word set to 0 at `0x10009f06` before the loop). Record: `+0` x, `+2` y, `+4` width, `+6` height (`u16`), `+8` number (`u32`), `+0xC` climate (`u16`), `+0xE` chosen template name, `+0x112` raw-material marks (20 `u16`, 0 or 1), `+0x13A` ore marks (9 `u16`, 0 or 1; §5 step 3), `+0x14C` special flag (the chosen template's `INFO[2]`), `+0x14D` treasure flag.
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
- Consequence: the island choice is reproducible given (seed, candidate list order). In a single-player game the candidate order is the cache file's (the directory enumeration order, §5 step 1); in a network game the scene takes a list built into `AnnoScene.dll` instead, so every machine has the same candidates in the same order without the cache (§5 step 1, **confirmed**).

## 5. Island selection at `ERZE` (`AnnoScene.dll` `0x10009550`)

Read instruction by instruction in round r (2026-10-23, T9 S14 stage 2, `tools/disasm.py`; **D** = disassembled). The scene records that feed it (`0x10009cc0`, **D**):
- `ROHSTOFF` (`0x1000a076`–`0x1000a155`): the record is read into a zero-filled 16-byte buffer (`0x1000a08a`–`0x1000a0a2`), then bytes 0–9 go to the raw-material table at `this+0x51DC` (20 `u16`, zeroed by the constructor `0x10007a86`–`0x10007aa1`) as **kinds 1, 2, 4, 5, 6, 7, 8, 9, 10, 11** (byte 0 → `+0x51DE`, 1 → `+0x51E0`, 2 → `+0x51E4`, 3 → `+0x51E6`, 4 → `+0x51E8`, 5 → `+0x51EA`, 6 → `+0x51EC`, 7 → `+0x51EE`, 8 → `+0x51F0`, 9 → `+0x51F2`). Kinds 0, 3 and 12–19 are never set from the file; bytes 10–15 are not read.
- `SCHATZ` (`0x1000a158`–`0x1000a19c`): the record is read straight into the `u16` at `this+0x150` (treasures).
- `ERZE` (`0x1000a19f`–`0x1000a275`): a zero-filled 10-byte buffer; bytes 0, 1, 2, 3 → ore kinds 0–3 (`+0x5292`, `+0x5294`, `+0x5296`, `+0x5298`), byte 4 → kind **5** (`+0x529C`), byte 5 → kind **6** (`+0x529E`); kind 4 (`+0x529A`) and kinds 7, 8 are never set from the file. Byte 6 → `+0x528A` (the special count), bytes 7 and 8 → `+0x528C`, `+0x528E` (read only through the accessor slots `0x1000a5b0`–`0x1000a5e3`, not by the selection); byte 9 is not read. Then `0x10009550` runs (`0x1000a25c`), so only the `ROHSTOFF`, `SCHATZ` and `INSELLNK` records **before** `ERZE` take part (P: that is their order in all 50 files).

The selection (**D** unless marked; "draw" = the world generator, world slot `+0xB4` = `0x100a1740`, a zero-extended `u16`, `div` unsigned):
1. **Candidate list.** Returns at once when the link list is empty (`0x10009562`–`0x10009576`). Resolves `Scenes\Islands\` (`0x1003d448`) and the cache file `Scenes\Islands\InselCache.txt` (`0x1003d458`). If the cache cannot be opened (`0x10008430`, `fopen "rb"`), scans the directory tree (`0x10007c50`) and writes the cache (`0x10007fe0`); then always reads the list (`0x10008110`). The scan (`FindFirstFileA "*"`/`FindNextFileA`, recursive into every directory except `.`/`..` at the position the directory entry comes in) keeps files whose name ends in `.scp` (`_stricmp` on the last 4 characters, `0x10007e16`), upper-cases the **file name** (ASCII `islower` → −0x20, `0x10007e35`–`0x10007e5a`; the folder part keeps its case), drops names containing `PIRAT` (`strstr`, `0x10007e60`) and names ending in `DUMMY.scp` (`0x10007e82`), and keeps those for which `GetAnnoInselFileData` (AnnoWelt `0x100b25a0`) succeeds: root `ANNO` and at least one `INSEL4` or `INSEL5` child, whose head gives width and height (the `INSEL4` branch `0x100b2645`–`0x100b26b5` reads `u16` at +0 and +2; the `INSEL5` branch `0x100b26ba` reads the 12-byte head at `0x100b26ea` and takes +4 and +6, `0x100b26f3`–`0x100b2717`) and whose `INFO` record `0x100b2770` copies into the summary (it zeroes the 12 bytes only when it finds a record, `0x100b27d3`, and returns 1 also when there is none, so a template without `INFO` keeps the zero-filled summary; every later `INSEL4`/`INSEL5` child is read the same way, so the last size wins and an `INFO` stays until a later one replaces it); entries are appended at the tail, the count is `this+0x63C8`. Cache entry (`0x10007fe0`, binary despite `.txt`; the reader `0x10008110` mirrors it field for field): 260-byte path, `u16` width, `u16` height, then `INFO` bytes 0, 1, 2, 4, 5, 6, 8, 9 (one byte each). In memory an entry is 280 bytes: `u32` width, `u32` height, `INFO[0..11]`, the path.
   - **Network games use a built-in list instead** (`0x10008187`–`0x100082ad`): when `this+0x53B4` is set, the reader ignores the cache and builds the list from tables in `AnnoScene.dll` `.data`: 428 entries, 80-byte paths at `0x10031554` (e.g. `Dschungel/D_064X064_0001_0B.SCP`), `u16` width/height at `0x10039B14`, the eight `INFO` bytes at `0x1003A1C4` (8 per entry). `this+0x53B4` is the second argument of the scene's slot `+0xA0` (`0x1000a4a0`), which `CreateAnnoSceneInstance` (`0x100011eb`) calls with its 7th parameter; the game start `0x10029100` passes its third argument, which its caller `0x1002a3f0` computes as `[0x1009B958] != 0`, the network client object (`tConnectClientSock`, vtable `0x10088F1C`, created at `0x10015f62`). The second start path `0x100206c0` passes 0. P (compared entry by entry with the game directory): the 428 entries are exactly the templates the scan keeps, **sorted bytewise by `<Folder>/<UPPER-CASE NAME>`**, with equal width, height and `INFO` bytes, minus one file, `Dschungel/D_080X080_0015_2BA_SUEDSEE.SCP`, which the scan keeps (429 on disk).
   - Order in a single-player game: the directory enumeration order, which on NTFS is alphabetical by the upper-cased name ("With the NTFS file system and CDFS file systems, the names are usually returned in alphabetical order", Microsoft, `FindFirstFileA` documentation, https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea), i.e. the same order as the built-in list; on FAT it is the order the files were written.
2. **Matching** (`0x1000964c`–`0x1000972b`, candidates outer, links inner): a candidate joins a link's list (appended at its tail, so each list keeps the candidate order) when `INFO[0]` = the link's climate, width = the link's width, height = the link's height, `INFO[8]` = 0 and `INFO[9]` = 0. The per-link lists are at `this+0x53C8 + 8 · i` (head, tail), room for 512 links (`0x63C8 − 0x53C8 = 8 · 512`).
3. **Raw materials, then ores.** Per-link byte counters (512 bytes on the stack each) start at 0.
   - Raw (`0x10009731`–`0x1000979c`): for round 0..24, for kind 0..19 with `raw[kind] ≠ 0`: `i = 0x10009170(round, kind, rawcount)`; if a link was chosen: `rawcount[i]++`, `best_raw = max(best_raw, rawcount[i])` (a byte), `raw[kind]--`.
   - `0x10009170`: for every link in list order whose raw mark of this kind is 0 and for which `0x10009010(kind, climate)` holds: score = position score of the link's **width** in row `round` of the table at `0x1003AF74` (4 `u32` per round: rounds 0: 256, 128, 0, 0; 1: 256, 128, 80, 80; 2–5: 80, 128, 256, 64; 6–24: 64, 80, 128, 256; a width at position p scores 4 − p, a later equal entry overwrites an earlier one, no match scores 0) + 1 if `rawcount[i] < round`. The best score is the maximum (starting from 0); the first five links (in list order) with that score form a list; `i = list[draw mod len]`; the link's raw mark `+0x112 + 2 · kind` = 1. No link → −1 and **no draw**.
   - `0x10009010(kind, climate)`: `1 << kind` tested against the climate's mask: 0 → `0x18000`, 1 → `0xF009`, 2 → `0xF949`, 3 → `0xF06B`, 4 → `0xF6B9`, 5 and 6 → `0xF04D`; climate > 6 → no (jump table `0x1000908c`). These are the values of AnnoWelt's monopoly masks (`docs/sim/state.md` §3).
   - Ores (`0x1000979e`–`0x10009803`): for round 0..24, for kind 0..8 with `ore[kind] ≠ 0`: `i = 0x10009330(round, kind, orecount, rawcount, best_raw)`; if chosen: `orecount[i]++`, `ore[kind]--`.
   - `0x10009330`: for every link whose ore mark of this kind is 0, for which `0x100090b0(kind, climate)` holds and `0x10009130(marks, kind)` allows it: score = the position score of its width in row `round` of the table at `0x1003B144` (rounds 0: 256, 128, 0, 0; 1: 256, 128, 80, 80; 2–14: 128, 256, 80, 64; 15–24: 64, 80, 128, 256), where a match at position p scores `(4 − p) + byte[0x1003B324 + 4 − p]` = **116, 6, 3, 1** for p = 0..3 (the byte for p = 0 is the first byte of the prefix string `po_` at `0x1003B328`, 0x70, read past a 4-byte table 0, 0, 1, 3; a later equal entry overwrites), + 1 if `orecount[i] < round`, + 1 if `best_raw − rawcount[i] > 2` (signed). Then as for raw materials (first five with the best score, `draw mod len`), mark `+0x13A + 2 · kind` = 1. So each link gets each kind at most once; the record holds marks, not counts.
   - `0x100090b0(kind, climate)`: kind 6 tests bit 0 (`0x100090b4`), any other kind bit `kind`; only the low byte is tested (`test al, imm8`): climate 0 → none, 1 → `0x05`, 2 → `0x25`, 3 → `0x01`, 4 → `0x03`, 5 and 6 → `0x0B` (jump table `0x10009114`). Kinds 7 and 8 never pass.
   - `0x10009130(marks, kind)`: false if for some e in 0..8 the link already has ore e and the byte at `0x1003AF40 + 7 · kind + e` is `0xFF` (a 7-byte stride read 9 wide). Excluded pairs (kind: already-marked kinds): 0: 0, 8; 1: 1–6; 2: 2, 7, 8; 3: 0–6, 8; 4: 1–6; 5: 2, 5, 8; 6: 1–6; 7, 8: none.
4. **Capacity filter** (`0x10009805`–`0x10009927`): for each link, each candidate of its list with `INFO[4] + INFO[5]` < the number of the link's ore marks is removed from the list; every kept one counts and ORs the value 1 into the link's kinds byte when `INFO[2] ≠ 0` (special), else the value 2 (`0x100098f6` / `0x100098fb`); 3 = both kinds.
5. **Links with one kind** (`0x1000992d`–`0x10009a1d`): `special = ERZE byte 6`; for each link in list order with count ≠ 0 and kinds ≠ 3: template = list[`draw mod count`]; `special--` if its `INFO[2] ≠ 0`; record `+0x14C` = `INFO[2]`, `+0xE` = its path.
6. **Special marking** (`0x10009a23`–`0x10009a91`): `special < 0` → 0; `special` times: `d = draw mod n` (n = links); probes d, then `(d ≥ n ? 0 : d) + 1` and so on, n + 1 probes in all, for a link with kinds = 3 and no mark yet; a hit within the first n probes is marked. (After the last link the probe looks at index n, which never qualifies, and then wraps to 1: link 0 is probed only as the first index.)
7. **Links with both kinds** (`0x10009a93`–`0x10009c34`): for each link in order with count ≠ 0 and kinds = 3: list the candidate indices, in list order and at most 50 (`cmp dl, 0x32`, the index stored as a byte), whose `INFO[2]` agrees: with `special > 0` a special candidate if the link is marked, a normal one if it is not; with `special ≤ 0` only normal candidates of unmarked links. Template = list[`draw mod len`]; `special--` if special; record as in step 5. (A marked link always finds special candidates while `special > 0`; `len` = 0 would divide by zero and cannot occur, because at most `special` links are marked and each marked link takes one.)
8. Step 5 and 7 write the chosen name to `+0xE` and its `INFO[2]` to `+0x14C`.
9. **Treasures** (`0x10009c3a`–`0x10009c9d`): `d = draw mod n` is drawn **before** the `SCHATZ` count is tested (so one draw also with 0 treasures); while the count is not 0: the link at d gets `+0x14D = 1` and the count drops if it was not flagged; `d = draw mod n` again. With more treasures than links the loop never ends (P: at most 15 treasures for 16–28 links).
10. **Placement** `0x10008490`, then `0x1000a3d0` frees the lists (no draw). For each link in list order:
    - An empty chosen name skips the link (`0x100084f4`–`0x100084fd`): no island. **D** (was "likely").
    - World slot `+0x88` (name, &(x, y), `number & 0xFFFF`) loads the template and places it; island property 7 = the link's climate (`0x10008532`–`0x10008545`); island property `0x12` = the file name after the last `\`; island property 11 = `0xF009` | (1 << k for every raw mark k) (`0x100085a2`–`0x100085cf`).
    - Ores (`0x100085d5`–`0x10008784`): the island's object container (island `+0x28`, slot `+0x118`) is walked; objects whose slot `+0x84` returns 1..6 and whose slot `+0x78(4)` returns 0 are collected. For ore kinds 0..6 with a mark, while unused objects remain: `d = draw mod count`, linear probe (with wrap to 0) to an unused one; the object's slot `+0x28` gets the type that the scene's resolver slot `+0x6C` returns for the id in the table at `0x1003AF24` (kind 0 → `0x67D`, 1 → `0x619`, 2 → `0x61A`, 3 → `0x61B`, 4 → 0, 5 → `0x61C`, 6 → `0x618`), and the deposit container (island `+0x24`, slot `+0x10` = AnnoWelt `0x100785a0`) visits the deposits in a 15 × 15 tile rectangle around the object and hands each (and the object container) to the deposit's `0x1007c0d0`. The remaining objects get slot `+0x74(1, 0)`, `+0x8C(+0x88(1, 1))` and the same deposit visit. **Not read further:** what slots `+0x84`, `+0x78`, `+0x28` of these objects are (mountains, by the shape), what the ids are (`0x618`–`0x61C` are not all `Anlagen.dat` ids: 1560–1562 are, 1563, 1564 and 1661 are not), what `0x1007c0d0` stores in the deposit, and the scale constant `0x100433C4` (in `.data`, 0 in the file).
    - Native settlements (`0x10008848`–`0x10008b20`): for each of the island's 16 city slots with a city whose object passes property `0x21`, the people (bits 2..10 of property `0x1C`) is looked up, a native player of type 14 is found or made (world `+0xa4` → slot 0 `(0xe, −1, &p)`, `0x1000890b`; no player: no names), and, when the scene holds the name lists, a player name is drawn from `NATIVE_SPIELER` (`this+0x63D0`, draw at `0x10008a23`, to the player's property `0x12`) and then a city name from `NATIVE_STADT` (`+0x63CC`, draw at `0x10008aa3`, to the city), each only when its list exists: `draw mod (count / 8) + (people − 2) · (count / 8)`. The lists are read in `0x1000a4a0`. **Not read further** (the player creation and the property calls).
    - Treasure (`0x10008b25`–`0x10008bb5`): with `+0x14D` set, object id `0x60E`; for climates 4, 5 and 6 one more draw, and `draw & 3 == 0` makes it `0x60F`; `0x10008cc0(island, &pos)` scans the island for a spot (one more draw, `& 7`, when it found any, `0x10008e9a`); then the object is created through the island's object container. **Not read further** (the scan).
    - With a native settlement made, the island's rectangle is registered (world slot `+0xBC`, no draw).

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
  4. Calls the commit `0x1001fad0` (`0x1001ec26`, §11.3), which copies the slots to the global array `0x1009bbd0`.
  5. Returns whether any `ENTRY` was read.

### 11.2 The record and the GUID test (`AnnoFrame.dll`, `AnnoScene.dll`)
- **Record constructor**, in the setup object's constructor `0x1001e140` (through the helper `0x1001e300`), in the static initialiser of the global slot array `0x100172d0`, and in both readers: byte 0 = 1, bytes 1–3 = 0, columns 1–18 `4 0 2 3 1 18 0 0 15 3 5 5 0 0 60 0 0 0`, GUID `{E861A553-AFA5-4C53-BAEB-6AF500000000}`, name `Default`, page `Default.htm`. **Confirmed.** Both file readers zero the record after constructing it, so a file record never inherits these values.
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
- Call 15 overrides call 14 with column 13. Its message shape (two values, at `+0x18` and `+0x14`) is the shape of parameter 17 `CHEATLIMIT`, whose setter stores both (`0x10004768`). That index 26 is a slip for 17 is a **guess** (OQ-112), and two things speak against it:
  - the `COMP` key reader puts `CHEATLIMIT=a,b` as a at `+0x14` and b at `+0x18` (`0x100016e8`, `0x10001703`), so the hand-over would carry column 14 = a and column 13 = b, against the column order;
  - the key-order reading of §11.4's columns (`sze.md` "AI profiles") puts `ZIELANZAHL` at 13 and `CHEATLIMIT` at 14–15.

  With the shipped values (all 0) it changes nothing.
- Because only type-12 players with colour `0xFF` take slots, the slots go to `DEF`'s players in `DEF` order. In the nine files with `ENTRY` records these sets are the same (P). A computer player with a preset colour keeps the values of its `COMP` record. So does a scenario without `ENTRY` records: its slots carry no valid GUID, and each computer only gets parameter 34. One shipped file is such a case: `single39.sze` has no `ENTRY` and a type-12 player with colour `0xFF` (player 17, P). This assumes the setup reader `0x1001e5d0` ran for the scenario (**likely**); it ends with the commit to the global slots itself (§11.1 step 4). The global slots start out as default records with a valid GUID (`0x100172d0`), and whether any start path skips the reader was not traced.

### 11.5 The AI side: objects, factory, parameter setter (`CompLogic.dll`)
- **Loading:** `AnnoWelt.dll` `0x1009c630` loads `CompLogic.dll` if world property 14 has bit 1 set (`0x1009c681`; the `ANNOADDON1` root sets it, §1), otherwise `CompLogicClassic.dll`. It calls `CompCreateInstance` (`0x100163b0`), which builds the manager (`0x1003d590`, vtable `0x100634f8`). **Confirmed.** This answers the selection half of OQ-018.
- **Per-player AI object:** the player reader for `COMP` (`AnnoWelt.dll` `0x1001e3be`) works in three steps. **Confirmed:**
  1. Reads the 8-byte prefix (type, index).
  2. Calls manager slot `+0x1c` = factory `0x1003dc50` with (type, index, the player's Volk number `word [player+0x7a]` = property 28 (§6), 1) (`0x1001e423`–`0x1001e434`) and stores the result in player property 44.
  3. Lets the new object read the key text (object slot `+0x18`).
- **The factory switches on the player type** (jump table `0x1003de68`). **Confirmed.** It stores the type at object `+0xa18` and the Volk number at `+0xa14` (`0x1003dd89`, `0x1003dd8f`); for types 12 and 13 it first sets the Volk number to 1 (`0x1003dcb4`–`0x1003dcba`). The index goes to the new object's slot 0 (`0x1003dd95`–`0x1003dd9f`). Classes by type:

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
| 0 | `TYP` | `COMPCAPS_TYP` | 0–7 only: calls slot `+0x24` (`0x100044b0`), which stores `+0xa34` and sets `+0xaf0` = `+0xaf4` = 47,500 / 37,500 / 42,500 / 42,500 / 52,500 / 42,500 / 52,500 / 57,500 for 0…7. The pirate class overrides slot `+0x24` (`0x10054710`), so these intervals do not hold for pirates | 1 |
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
| 24 | — | `COMPCAPS_SETHELPANLAGE` | type 11 only: `+0xa2c`; then, for every type, `0x100043e0` (`0x100048ed` → `0x100048fc`) | — |
| 25 | `TRIBUTSPERRE` (editor: `SETTRIBUTSPERRE`) | — | `+0xa68`; read only by the getter (`0x10004b23`) and by the gate `0x100057a0` (below) | 8 |
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
  - *Capping tasks:* the planner `0x10043210` first returns if the global `[0x100692d8]` is below 4. At `0x10043233` it returns when bits 2–3 are 0. Otherwise it makes two counts over the AI's 12-entry task table (`+0x1ad4`, 28 bytes each): one of entries, one of distinct values of the entry byte `+8`. It returns early once a count reaches 1 (bits 2–3 = 4 or 8) or 2 (= 12).
  - *Cancelling tasks:* writing the parameter calls `0x10004df0`, which cancels the table entries of the kind whose bits are now 0. Entries without flag bit 1 belong to bits 2–3; entries with it belong to bits 0–1.
  - *Two further tests:* `0x1004354d` (inside the same planner) tests bit 3, and `0x10043e4a` tests bit 1; not followed.
- **`PIRATEATTACKALL`** (script only, `single34.sze`, 6 uses) sets bit 3 of the flag word `+0xa84`. **Confirmed:** the pirate class (vtable `0x10063a00`, slot `+0x2c` = `0x10055a30`) calls `0x100558b0` three times. That routine walks candidate objects and drops each one that fails the check `0x10028bd0(candidate, TYP)`. When bit 3 is set it skips this check (`0x1005593f`). **Likely:** the pirates then attack every candidate instead of a TYP-filtered set.
- **`TYP` (column 1)** selects the pair `+0xaf0`/`+0xaf4` (table above; not for pirates, whose class overrides the slot). Several AI routines pass both values with a time value to `0x1003ea50`, e.g. `0x10042357` (the time comes from `0x1003d240`). That routine divides elapsed time by them while it updates a 32-entry per-player table of signed byte values. **Confirmed** for the arithmetic. **Likely:** a decay rate of the AI's per-player attitude; the time unit is not traced. The field `+0xa34` itself occurs 47 times in the module, among them in the pirate target check above.
- **`PROZENTE` (column 6)** gives the AI two factors, 1 ± v/100. **Confirmed.** A price margin is a **guess** (OQ-113).
- **`TRIBUTSPERRE` (column 8)** is tested only as zero / non-zero, in the gate `0x100057a0` (`0x100057a3`). **Confirmed:**
  - non-zero: the gate returns 0 and does nothing;
  - zero: it calls `0x10026c30` with the AI's own player number (`+0xa30`), a target player and a message, then `0x100087e0`, which stamps the current time (`0x1003d240`) into the AI's per-player table (`+0xaf8`, via `0x1003e680`), and returns 1.
  - The other writes are the setter (`0x1000469d`) and four constructors (`0x1000108f`, `0x1003ec9c`, `0x100516c6`, `0x100536d2`).
  - *Callers:* `0x100425c5`, `0x10042679`, `0x100429d7`, `0x10042a0e`, `0x10042b1c`, which sit in the computer class's code range, and `0x10054a6f` in the pirate class's range. None is in the native class's range. Which class each calling function belongs to was not traced through the vtables.
  - *Reading:* the message amounts set before the first two calls (300 or 2,000 at `0x10042590`–`0x1004259e`; 800 at `0x10042650`) look like gold. With the name, the gated action is **likely** a tribute demand to another player (OQ-113).
- **`SUSPEND`:** a non-zero value sets `+0xa80`, clears the byte `+0x1a89` and calls the AI's slot `+0x20`. **Confirmed.** That this pauses the AI is **likely** from the name. `single41.sze` sets it for 12 computer players by script.

### 11.7 The script route (`SET_PROFILE`, `AnnoScene.dll`)
- The action parser (`0x10019a04`) handles `SET_PROFILE("<player>", "COMPCAPS_<key>", <value>)` and maps the name to the index of §11.5 (`0x10019b4e`–`0x10019f3b`). **Confirmed.** An unknown name gives the error `%s ist ein unbekanntes PROFIL bei Bedingung: %d`.
- The action object (vtable `0x1003b874`) stores the player at `+4`, the value at `+8` and the index at `+0xc`.
- Its execute method `0x10015e80` takes the player's AI object from player property 44 and calls the same setter (slot `+4`). It puts the value in message `+0x18`. For indices 15 and 16 it puts the value in `+0x10` and 25 in `+0x18` instead; no script name maps to 15 or 16. **Confirmed.**
- The shipped scripts use it 22 times (P): `COMPCAPS_SUSPEND` 12 × in `single41`, `COMPCAPS_PIRATEATTACKALL` 6 × in `single34` (value `TRUE`), `COMPCAPS_KAMPFTYP` 2 × each in `S01_02_01` (1, 5) and `S01_07_01` (0, 2).

### 11.8 `CompLogicClassic.dll`
- Its parameter setter `0x10002e50` (jump table `0x100032d0`) has the same 36 cases with the same object fields and the same value limits as `CompLogic.dll`. It differs only in the called routines and in the `SUSPEND` byte (`+0x1a19` instead of `+0x1a89`). **Confirmed.** So the profile columns mean the same in both modules.
- It has the same key strings minus a few (S, `game-files-resurvey.md` §3). Its own readers and its behaviour behind the fields were not compared.

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
- §11: `ENTRY` columns 0, 7, 14, 15 (unused) and 13 (the doubled `SUSPEND` call) (OQ-112); what the AI parameters do beyond the setter (OQ-113); the lost `SETTRIBUTSPERRE` values (OQ-114); which machine sends network message 6; the type-17 AI case (`0x1003de00`).
- The meaning of `INFO` byte 2 ("special") and byte 5 (capacity vs. mountain/ore slots), and what the 20 raw-material and 9 ore kinds are (the tables of §5 step 3 are read; their names are not).
- §5 step 10: the mountain objects (slots `+0x84`, `+0x78`, `+0x28`), the ore type ids of `0x1003AF24`, the deposit update `0x1007c0d0`, the native settlement setup and the treasure spot scan `0x10008cc0`.
- Whether a savegame stores the world random cursor (property 8), and when the reset `0x100af240` runs relative to a scene load.
- Network seed: which side sends message 0; what start path `0x100206c0` is.
- Player types 11 vs 8, and type 2 behaviour; `HEAD` fields `+0x4D`, `+0x5E`, `+0x60`, flags at `+0x48`/`+0x4E`/`+0x5C`.
- Ship `attr` range 0–14: 16-step facing? Trace the ship class's draw/frame selection.
- Colour button → value mapping on the colour screens; whether computer colours can collide with preset ones in practice.
- Whether anything reads `INFO` bytes 6–7 (area / 64) or byte 1.
- Script step (§12): what the step's argument is and how many IFs a step evaluates in practice; whether `+0x3c` is written anywhere else; whether evaluating a condition changes state (it would make `&&`'s skip and `||`'s full evaluation observable).
