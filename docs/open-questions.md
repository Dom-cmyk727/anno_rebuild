# Open questions

Log of hypotheses and unknowns. Per `CLAUDE.md`, guesses go here, never into code comments stated as fact. Each entry gets a stable number; close an entry by setting Status to `resolved` with a link to the evidence (format spec, `docs/re/` note, commit), never by deleting it.

Sources abbreviated: GFA = `docs/research/game-files-analysis.md`, CL = `docs/research/community-landscape.md`, PLAN = `docs/PLAN.md`. All file facts refer to GOG 2.0.0.5.

---

## OQ-001: Trailing bytes after the root chunk in `.scp` files
- **Status:** open
- **Area:** formats / chunk container / islands
- **Question:** What is the data after the end of the root chunk in `.scp` island templates, and is it needed for loading islands?
- **What we know:** All 465 `.scp` files parse with `tools/chunks.py` up to the end of the root chunk; 444 of them carry additional bytes after it (GFA §2–3). Format unknown.
- **How to resolve:** Dump trailing lengths and look for correlation with island size (`Po_064x064_…` file names, `INFO`); check whether the bytes are another chunk sequence with a different header; find the `.scp` loader in `AnnoWelt.dll` (`GetAnnoInselFileData`) in Ghidra. Parser must keep the bytes (`trailing`) for round-trip meanwhile (PLAN M1.1).
- **Update 2026-10-16:** the trailer is always exactly 60 bytes and occurs in exactly the 444 files whose root contains `INSEL5`; the 21 `INSEL4` files have none (P: `anno-dump chunks` over all 465 `.scp`). Reader/writer keep it verbatim (`docs/formats/chunk-container.md`).

## OQ-002: `FigurenHeader.dat` record layout and claimed big-endian first 4 bytes
- **Status:** resolved 2026-10-16 — all little-endian; root `ANNO` → `FIGUREN` → 368 `ENTRY` (8-byte prefix + ordinary chunk records), sprite sets in `HAEUSER`, 36-byte frame headers in `TEXTURES` (width, height, colour table, format, anchor, offset, length), three volume headers at the file end. The "big-endian first 4 bytes" claim does not hold for this build. Spec: `docs/formats/figuren.md`; byte-identical round trip in `figuren_corpus`. Remaining unknowns moved to OQ-023.
- **Area:** formats / figures
- **Question:** What is the exact layout of `FigurenHeader.dat` (ENTRY header, key-value records, per-sprite header), and are the first 4 bytes of a sprite header really big-endian?
- **What we know:** Does not parse with the generic chunk parser; ENTRY contains key-value records (12-byte key + u32) with a differing header, keys include `VERSION2`, `HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`, `PATH`, `COLORS`, `HAEUSER` (GFA §3, PLAN M1.3). Community: sprite header holds height, width, palette id, tag, midpoint, offset, length; "first 4 bytes big-endian" (CL §3, no primary URL recorded). Amsel spec would cover this but is behind a GitLab login (CL §1).
- **How to resolve:** Hex-inspect the file against the community field list; test both endiannesses for plausibility (offsets/lengths within `Figuren*.dat` sizes); obtain Amsel docs bundle (AnnoZone file 9287); locate the reader in Ghidra.

## OQ-003: RLE format of `Figuren*.dat`
- **Status:** resolved 2026-10-16 — `.tex` row RLE (`skip, count, data`, `0xFE`/`0xFF`) with 1 byte per pixel (`V`, `H`, `AV`), 2 bytes `[z, index]` (`Z`, `AZ`) or none (`BSH_ALPHA` shadows = coverage masks); `A` formats put a 2-bit level in the top bits of the count byte. External palettes per figure type (`COLORS`). All 655,842 distinct frame ranges decode; visual check on mower, sheep, galleon, gondola and shadows. Spec: `docs/formats/figuren.md`. Meaning of `z`, `level`, `H`: OQ-023.
- **Area:** formats / figures / rendering
- **Question:** How is sprite pixel data in `Figuren*.dat` encoded (run types, transparency, row termination, bit depth), and how are shadows (`BSH_ALPHA`) stored?
- **What we know:** 16 files, ~280 MB, raw data without header, indexed by `FigurenHeader.dat` (GFA §2). Hypothesis: 8-bit indexed RLE with transparency runs and external palette, by analogy with 1602 BSH (mdcii, anno16_docs) (PLAN M1.3) — 1602 formats are not proven for 1503 (CL §3). Shadows reportedly greyscale `BSH_ALPHA` (CL §3). `AnnoDisplay.dll` RTTI shows blitters for 555/565, alpha, shadow, Z (GFA §1). A₂X Graphics Decoder (GPLv3) decodes these (CL §1).
- **How to resolve:** Decode a known sprite (e.g. figure id 203 = carrier per AnnoZone) and render it; cross-check with Amsel spec or A₂X decoder output; disassemble `tBrush2Screen…` blitters in `AnnoDisplay.dll`.

## OQ-004: Pixel format of `TEXTURE` chunks and `COLOR`/`COLORS` palettes
- **Status:** resolved 2026-10-16 — 8-bit indexed pixels (`BSH_RGB` row RLE, `TEX_RGB` raw), palettes 256 × `[B, G, R, 0]` in both `.tex` `COLOR` and `Anlagen.dat` `COLORS`; `ACCESS` = `"<source>,<texture id>"` records. Specs: `docs/formats/tex.md`, `docs/formats/palette.md`. The community note "palettes are PCX without header, 4-byte entries" fits: every `COLORS` table names a `Farbtabellen\*.pcx` source. Remaining unknowns moved to OQ-021/OQ-022.
- **Area:** formats / textures / palettes
- **Question:** What pixel format do `TEXTURE` chunks in `.tex` use (indexed vs. 16-bit 555/565 vs. other), what is the layout of `COLOR` (`.tex`) and `COLORS` (`Anlagen.dat`, 440 ENTRY), and what does `ACCESS` in `.inc` contain?
- **What we know:** `.tex`/`.inc` parse fully as chunks (GFA §3). Community: palettes are "PCX" without header, 4-byte entries (CL §3). Renderer has 555/565 blitters (GFA §1). drkohler's GFX Extractor reads `Felder.tex` (32×32 ground tiles) (CL §2, PLAN M1.2).
- **How to resolve:** Inspect `Textures/debug.tex` and `ToolGfx/Felder.tex`; try candidate decodings and export PNG; compare with GFX Extractor output if obtainable.

## OQ-005: Does single-player use the internal client/server split?
- **Status:** resolved 2026-10-17
- **Update 2026-10-17:** no — single-player applies player actions to the world directly (`tNetClient`, `AnnoFrame.dll` `0x100589a0`); multiplayer (`tNetClientNet`) encodes the same actions as commands in 100 ms turns and applies them through the same function (`docs/re/tick-loop.md` §4).
- **Area:** RE / architecture
- **Question:** Is the `AnnoGame` server (`CreateAnnoServerInstance`) plus `AnnoFrame` NetClient/NetServer path also used in single-player, i.e. do player actions always travel as messages to a server?
- **What we know:** `AnnoGame.dll` exports `CreateAnnoServerInstance`, imports only KERNEL32; `AnnoFrame.dll` exports NetClient/NetServer and imports WS2_32 (GFA §1). Explicitly flagged as "to be checked" (GFA §1).
- **How to resolve:** Ghidra: call graph from `CreateAnnoNetClientInstance` / `CreateAnnoServerInstance` in a single-player start path (PLAN §7 item 4); map `iAnnoNetClient`/`iAnnoNetServer` vtables (`docs/re/interfaces.md`). If yes, the message catalogue gives the command set for `crates/sim`.

## OQ-006: Simulation tick rate of the original
- **Status:** resolved 2026-10-17
- **Update 2026-10-17:** no fixed rate in single-player: each frame advances the world by trunc(dt × speed + 0.5) game ms (capped at 100); one simulation tick = 2 game ms (`AnnoWelt.dll` `0x100ae040`, `ms >> 1`, verified in the disassembly). Multiplayer: fixed 10 ms steps, 10 steps per 100 ms turn. Buildings step every 500 ticks, houses every 2,500 (verified: `0x100680b0`, `0x10076230`), taxes every 2,000 (`docs/re/tick-loop.md`, `production.md`, `population.md`).
- **Area:** RE / sim / net
- **Question:** At what fixed rate (if any) does the original simulation advance, and is it decoupled from frame rate?
- **What we know:** PLAN M4 leaves the tick rate "e.g. 10–20 Hz, to be decided from M2 findings". Contemporary reports say <15 fps caused more sync errors (CL §4), suggesting coupling to frame rate — not proven.
- **How to resolve:** Locate the tick/update loop in `AnnoWelt.dll`/`AnnoFrame.dll` (PLAN M2); look for timer calls (`GetTickCount`/`timeGetTime`) and divisors.

## OQ-007: x87 floating point in simulation formulas
- **Status:** mostly resolved 2026-10-17
- **Update 2026-10-17:** the update path of the world clock, the PRNG, production, growth, population and taxes is integer-only; floats appear in the step-size computation (speed factors give exact integer steps), at load time (`PROD3`, `BGruppen.dat` `Menge` × 40.96, rounded to integers) and in figure movement (speed, positions; `__ftol` at `0x100b5f90`) — the remaining determinism risk (`docs/re/tick-loop.md` §6, `production.md`, `population.md`). Open: figure movement in detail, x87 precision/rounding mode.
- **Area:** RE / sim / determinism
- **Question:** Which simulation formulas (production, growth, taxes, movement, fire) use floating point in the original, and what fixed-point equivalent reproduces them within acceptable tolerance?
- **What we know:** MSVC 6 build uses x87 (80-bit intermediates); x64 uses SSE2, so results may differ (GFA §4). Project rule: no floats in checksummed sim state (CLAUDE.md). PLAN §8: "faithful ≠ bit-identical float".
- **How to resolve:** During M2, flag every x87 instruction sequence in recovered sim functions; write differential fixtures; define tolerance per formula.

## OQ-008: Trailing garbage in 12-byte chunk names
- **Status:** open
- **Area:** formats / chunk container
- **Question:** What do the bytes after the NUL terminator in the 12-byte chunk name contain (uninitialised memory, previous name, meaningful data), and must they be preserved verbatim for a byte-identical round trip?
- **What we know:** Names are NUL-terminated and "the rest can be garbage" (GFA §3, `tools/chunks.py`). Round-trip tests require byte-identical output (CLAUDE.md, PLAN M1.1).
- **How to resolve:** Histogram the post-NUL bytes across the corpus; if not reproducible, store the raw 12 bytes in the parsed chunk and write them back unchanged. Check the writer in the original (`SaveGame` in `AnnoScene.dll`) for how it fills the name buffer.
- **Update 2026-10-16:** 22 of 17,529 name fields have non-zero bytes after the NUL (14 `.tex`, 6 `.zei`, 2 `.inc`; names `TEXTURE`, `ANNO`, `COLOR`, `CHARSET`, `HEAD`, `BODY`); the rest are zero-padded. The writer stores the raw 12 bytes, so round trip no longer depends on the answer (`chunk_corpus` asserts the counts).

## OQ-009: Differences between `ANNO` and `ANNOADDON1` roots
- **Status:** open
- **Area:** formats / scenarios
- **Question:** Beyond gating add-on content, how do `.sze` files with root `ANNOADDON1` (27) differ structurally from root `ANNO` (23) — extra chunks, different versions, different field layouts?
- **What we know:** Root counts per GFA §2; community says the root switches add-on content (CL §3). DatEditor handles "ADDON chunks" in `Anlagen.dat` (CL §1).
- **How to resolve:** Diff the chunk-path histograms (`tools/chunks.py` `walk`) of both groups; compare same-named chunks' sizes.

## OQ-010: Versioned chunk names (`INSEL4`/`INSEL5`, `PROD2`, `MONOPOL2`, `BERGWERK2`, `COMPPROFIL2`, `VERSION2`)
- **Status:** open
- **Area:** formats / scenarios / islands
- **Question:** Do earlier versions (`INSEL4`, and possibly unsuffixed `PROD`, `MONOPOL`, …) occur in the GOG corpus, and how do the layouts differ between versions?
- **What we know:** Both `INSEL4` and `INSEL5` occur (GFA §3). Suffix numbers suggest format evolution the loader must cover (GFA §3, PLAN M1.5). Which older names actually occur is not recorded.
- **How to resolve:** Count every chunk name across the full corpus; for names with multiple versions, compare payload sizes/structure; find version dispatch in the loader (Ghidra).

## OQ-011: Symbols in the `FileGame.dll` debug build
- **Status:** open
- **Area:** RE
- **Question:** Does `FileGame.dll` (debug build, Jan 2004) contain assert strings, source file names or other symbol information that names functions or structures shared with the release modules?
- **What we know:** PDB path `E:\Sources\Anno1503\AddOn\Projects\FileGame\Debug\FileGame.pdb`; export `CreateFileGame`; 369 KB (GFA §1). The PDB itself is not shipped (not listed in GFA). PLAN M2 lists this as a task.
- **How to resolve:** `strings`/Ghidra pass over `FileGame.dll`; record findings in `docs/re/filegame.md`.

## OQ-012: `Data/Debug.dat` is a Bink file
- **Status:** open
- **Area:** formats / assets
- **Question:** What is `Data/Debug.dat` (a Bink video per its magic), and does the game load it at all?
- **What we know:** Identified as Bink (`BIKi`) (GFA §2). Purpose unknown.
- **How to resolve:** Search for the string `Debug.dat` in all modules (Ghidra/strings); play it with ffmpeg to see content.

## OQ-013: Generic container detection is heuristic
- **Status:** open
- **Area:** formats / chunk container
- **Question:** Is there an explicit rule (known set of container names, flag, version) for which chunks contain sub-chunks, or can a leaf payload accidentally parse as chunks?
- **What we know:** `tools/chunks.py` treats a payload as a container if it parses exactly into ≥1 valid chunks with names matching `^[A-Z][A-Z0-9_]{1,11}$`, up to depth 6 (`tools/chunks.py`). This is a heuristic, not evidence of the original's rule.
- **How to resolve:** Build the list of container chunk names from the corpus; check that no leaf name is ever classified as container; confirm against the loader in Ghidra. The Rust parser should then use an explicit container-name table. Until then `anno_formats::chunk` deliberately mirrors the same heuristic (and the depth limit `MAX_DEPTH = 6`) so it agrees with `tools/chunks.py`; example: in `Po_064x064_0008_Eskimo.scp` the 20,459-byte `INSEL5` payload does not parse as a chunk sequence, because it starts with 12 bytes that are not a chunk header (`00 00 00 00 40 00 40 00 00 00 00 00`) followed by `INFO`, `HEIGHT`×4, `GFXID`×4, `GELAENDE`×4, `FRUCHTBAR`, `FLUESSE`… chunks. Hypothesis (unverified): the two `u16` 0x40 are the 64×64 island size from the file name. Container detection therefore needs per-chunk knowledge of such prefixes (M1.5).

## OQ-014: `.tex` vs `.inc` count mismatch
- **Status:** open
- **Area:** formats / textures
- **Question:** Why are there 1,214 `.tex` but only 1,213 `.inc` files — which `.tex` has no `.inc`, and is `.inc` optional?
- **What we know:** Counts per GFA §2–3.
- **How to resolve:** Pair files by stem and list the unpaired one; check how the loader handles a missing `.inc`.

## OQ-015: `Anlagen.dat` ENTRY layout
- **Status:** open
- **Area:** formats / buildings
- **Question:** What is the field layout of the 921 `ANLAGEN` ENTRY records (size, footprint, graphics ids, production fields) in the GOG build?
- **What we know:** Chunk structure parses (GFA §2–3). Community sources: Azukee DatEditor `AnlagenParser.cs` (classes `TAnlage*`, `THaus*`), easy-player-boost `build_mod.py` field offsets (production interval, coverage radius, `VermehrTab`) — the latter targets the History Edition files (`Anlagen.dat/.classic/.addon`) and is untested (CL §1). Original function name `parseAnlageReal` (CL §1).
- **How to resolve:** Retrieve DatEditor from Software Heritage (reference only, no licence); verify each offset against GOG `Anlagen.dat` and `Texte.dat [ANLAGEN]` names; locate `parseAnlageReal` equivalent in Ghidra.

## OQ-016: Encoding and grammar of plain-text data files
- **Status:** open
- **Area:** formats / data
- **Question:** What exact syntax (comments, sections, `Key: value`, `Id` blocks, numeric formats) and character encoding do `Data/*.dat` and `Texte.dat` use?
- **What we know:** Described as INI-like plain text with `[SECTION]` headers and `Key: value` or `id:Text` lines (GFA §2, PLAN M1.5); `BGruppen.dat` uses `Id` blocks (CL §3). Encoding not recorded.
- **How to resolve:** Survey all 12 `Data/*.dat` files plus `Texte.dat`; detect non-ASCII bytes and decide codepage (likely Windows-1252, unverified); write grammar into `docs/formats/data-text.md`.

## OQ-017: Sources of non-determinism in the original (fire spread, mass actions)
- **Status:** open
- **Area:** RE / sim / net
- **Question:** Why do fire spread and large-scale clearing/planting desync, and how does the original detect an "Async"?
- **What we know:** Player reports (History Edition) of desyncs during fire spread and mass clearing/planting (CL §4); original MP had undetected asyncs, OS-mix desyncs (CL §4); string "Async aufgetreten! …" in `AnnoFrame.dll` (GFA §1).
- **How to resolve:** Ghidra: find the code referencing the Async string and what it compares; inspect fire-spread code for unseeded/global RNG or float use (PLAN M2).

## OQ-018: CompLogic vs. CompLogicClassic selection
- **Status:** open
- **Area:** RE / AI
- **Question:** When does the game load `CompLogic.dll` vs. `CompLogicClassic.dll`, and how do their behaviours differ?
- **What we know:** Both export `CompCreateInstance`; described as add-on vs. classic variant (GFA §1). Removing them disables AI (Venedig mod, CL §2).
- **How to resolve:** Find the `LoadLibrary` call sites in `AnnoFrame.dll`/`AnnoGame.dll`; diff the two DLLs' function sets.
- **Update 2026-10-22 (T18, selection answered):**
  - *Selection (D):* `AnnoWelt.dll` `0x1009c630` loads `CompLogic.dll` when world property 14 has bit 1 set (`0x1009c681`; set by an `ANNOADDON1` scenario root), otherwise `CompLogicClassic.dll`.
  - *Shared interface (D):* both export the same `CompCreateInstance`. Their per-player parameter setters have the same 36 cases with the same fields and value limits (`CompLogic` `0x10004550`, `CompLogicClassic` `0x10002e50`).
  - *Still open:* how their AI behaviour behind those fields differs (`docs/re/annoscene.md` §11.5, §11.8).

## OQ-019: Role of `Max_Sound.dll` and `DrvMgt.dll`
- **Status:** open
- **Area:** RE
- **Question:** Does `Max_Sound.dll` use DirectSound via COM, and what does `DrvMgt.dll` (exports Setup/Remove) do?
- **What we know:** Both roles marked "presumably"/"not verified" in GFA §1. `DrvMgt.dll` dated January 2004 like `FileGame.dll`.
- **How to resolve:** Check COM CLSIDs/IIDs referenced in `Max_Sound.dll`; inspect `DrvMgt.dll` exports and strings. Low priority.

## OQ-020: Location and format of original savegames
- **Status:** open
- **Area:** formats / savegame
- **Question:** Where does the GOG 2.0.0.5 build write Spielstände, with which extension, and are they the same chunk container as `.sze` (root `ANNO`/`ANNOADDON1`) with additional live-state chunks?
- **What we know:** `AnnoScene.dll` exports `SaveGame`, `AnnoWelt.dll` exports `RepairSavegame` (`analysis/pe_symbols.json`). No savegame exists in the extracted install.
- **Partly answered (T16, 2026-10-22, Wine runner; `docs/testing/original-wine.md` §2.5):**
  - F10 writes `SaveGame/<profile>/Lastgame.sav` in the game folder;
  - it is the same chunk container, root `ANNOADDON1`, with `WELT`, `MAIN`, `GUID`, `ALIASNAME`, `WARETYPE`, `SPIELER` × n, `INSEL5` × n, `SKTIMER`, `MONOPOL2`, `LIMITS`, `BIGBROTHER`;
  - `MAIN` `u32` +0 = game time in whole seconds.
  Open: the extension and folder of named saves, the meaning of `MAIN` +4, and the field-by-field comparison with the scenario a game started from.
- **How to resolve:** create saves with the original (PLAN M2.1), run `anno-dump chunks` on them, compare chunk sets with the scenario they started from.

## OQ-021: Unknown fields in `.tex` entries and `TEXTURE/HEAD`
- **Status:** open
- **Area:** formats / textures
- **Question:** What do these fields mean: (a) `HEAD` 0x06 (`u16`, always 8, plausibly bits per pixel); (b) `HEAD` 0x14 (4 bytes, `ff 00 ff 00` = magenta in 5,169 of 5,176 textures, `ff 94 00 00` ×5, `00 00 00 00` ×2, plausibly a transparency key); (c) `HEAD` 0x18/0x1A (two `u16`, often `(width/2, height)` but out of bounds for 1,207 textures, plausibly an anchor point); (d) the `H` in `BSH_RGBH` (one texture, `Textures/debug.tex`); (e) the first `u32` of the 8-byte `COLOR` prefix, 0 except for 10 palettes (ids 32, 73–75, 98 in `ToolGfx/fenster.tex` and `fenster8.tex`) where it is 2.
- **What we know:** all from corpus statistics (P), `crates/formats/tests/tex_corpus.rs`; layout in `crates/formats/src/tex.rs`. Visual check 2026-10-16 (b): transparency comes from RLE skips alone — 299 exported textures (every 8th of `ToolGfx/fenster.tex`, all of `Textures/Felder.tex` and `debug.tex`) contain no opaque `ff00ff` pixel, so decoding does not need the key; its use (e.g. a key for the original's blitter) is still unknown.
- **How to resolve:** render textures and compare with in-game screenshots (anchor, key); find the texture loader in `AnnoDisplay.dll`/`View.dll` in Ghidra.

## OQ-022: Constant fields and special entries in `Anlagen.dat` `COLORS`
- **Status:** open — (a), (b) resolved 2026-10-17; (c) partly (no index is special for player colours)
- **Update 2026-10-17 (a) resolved:** the `flag` is the colour-table kind passed to `AnnoDisplay.dll`, 0 meaning 3 (`Effects`); 4 = `EffectsMulti`, the player-colour variants, used by `Flagge01_01.PCX` (D: `AnnoGame.dll` `0x10004e60`, `docs/re/annogame.md`).
- **Area:** formats / palettes
- **Question:** (a) What is the `u32` at `ENTRY` 0x000? It is 0 in 439 tables and 4 in one, id 109 `Farbtabellen\Flagge01_01.PCX` ("Flagge" = flag); compare the `.tex` `COLOR` prefix, which is 2 on 10 palettes in `ToolGfx/fenster*.tex` (OQ-021 e). Hypothesis, unverified: a palette-kind marker, e.g. a player-colour remap table for flags. (b) What is the `u16` 0 between each 4-byte tag (`BODY`, `NAME`) and its `u16` length — the high half of a `u32` length, or a separate field? (c) Do palette entry 0 (a grey in ≥ 439 of 440 tables) and entry 1 (magenta `ff00ff` in 287) have a fixed role, e.g. shadow/transparency key? (d) Which records refer to a colour table `id` (`ANLAGEN`, `TEXTURES`, `Figuren*`)?
- **What we know:** layout and counts from all 440 entries (P), `docs/formats/palette.md`, `crates/formats/tests/colors_corpus.rs`. The same tag + `u16` + `u16` record shape may occur in `ANLAGEN` or `TEXTURES`; not yet probed.
- **How to resolve:** probe `ANNO/ANLAGEN/ENTRY` and `ANNO/TEXTURES` for the same record shape and for `u16`/`u32` values in 1…440; find the colour-table loader in Ghidra (strings `BODY`, `NAME`, `Farbtabellen`).
- **Update 2026-10-17 (b) resolved:** `BODY`/`NAME` + `u16` 0 + `u16` length is the tagged-record header of the scenario players: a 6-byte NUL-padded name and a `u16` length. The `u16` 0 is the padding of `BODY\0\0`; there is no 32-bit length (P, 29,686 records in all 724 `SPIELER`; `docs/formats/sze.md`, "Tagged records").
- **Update 2026-10-16 (d) resolved for `ANLAGEN`:** each `ANLAGEN/ENTRY` `COLORS` record is a list of 1–3 `u32` ids of these 440 tables (0 = unused slot), and building frames select `COLORS[frame.palette]` (P, `anlagen_corpus`; `docs/formats/anlagen.md`).
- **Update 2026-10-16 (d):** `FigurenHeader.dat` carries its own colour tables per figure type (same `ENTRY` layout, ids from 0 per entry), and frames select them by per-entry index (`docs/formats/figuren.md`), so figures do not refer to the 440 `Anlagen.dat` tables by id. Whether `ANLAGEN`/`TEXTURES` in `Anlagen.dat` do is still open.

## OQ-023: Unknown records and fields in `FigurenHeader.dat` and frame pixels
- **Status:** open
- **Update 2026-10-16 (static analysis, `docs/re/annodisplay.md`):** (c) `z` = signed 8-bit per-pixel depth offset relative to the sprite depth, smaller = nearer (confirmed); (d) `level` 0/1/2/3 = 25/50/75 % blend / opaque, with brush-dependent exceptions (confirmed); (e) `H` = depth-tested but not depth-writing (flat decal), `V` = writing (confirmed; letter reading likely); `M` = melt blend at equal depth; `W` = every-second-column checkerboard (confirmed; decoder fixed); (f) anchors are signed 16-bit and mark the frame pixel placed on the projected object point (confirmed). Still open: (a), (b), the world point the game passes for an object.
- **Area:** formats / figures / rendering
- **Question:** (a) Contents of the entry records `VERSION2` (24 bytes), `HEAD` (16/20), `LOGIC` (28–44, contains `1.0f`), `SCHUSSPOS`, `OBJECTPOS`, and meaning of the strings `BEWOHNTYPE`, `ROUTETYPE`, `FORSCHTYPE`, `WARE`, `BAUWARE`. (b) The 16-byte sprite-set prefix (259 distinct values, trailing bytes look like uninitialised memory) and the records `BODY2` (44), `ANIM` (24), `SOUND2` (128), `GFXDATA` (84, holds a `.tga` source path). (c) What `z` (first byte of `Z` pixel pairs) encodes — per-pixel depth for occlusion? The sheep's values are nearly all 255. (d) What the 2-bit `level` of `A` formats encodes — 4 alpha steps? In the gondola level 3 dominates (2,195 of 2,322 pixels) and drawing all levels opaque looks right, so 3 is probably opaque; levels 0–2 are unconfirmed. (e) The `H` in `BSH_RGBH` (24 frames; also one `.tex` texture, OQ-021 d). (f) Anchor semantics: `(width/2, height−4)` for walking figures, the waterline for ships, outside the frame for 783 frames — draw origin relative to the tile?
- **What we know:** layout, counts and decodability from the whole corpus (P), `docs/formats/figuren.md`, `crates/formats/tests/figuren_corpus.rs`. `AnnoDisplay.dll` has blitters for alpha, shadow and Z (GFA §1).
- **How to resolve:** Ghidra: the figure loader (strings `HAEUSER`, `GFXDATA`, `BSH_RGBAZ`) and the `tBrush2Screen…` blitters in `AnnoDisplay.dll` for `z`, `level` and anchors; statistics over `ANIM`/`BODY2` against frame counts (e.g. 128 frames = 8 directions × 16 steps?); render scenes in the M1.6 viewer and compare with screenshots.
- **Update 2026-10-22 (in-game help, `docs/research/manual-and-help.md` §1.4):** (a) part of `LOGIC`, **likely**: `0x1c` = damage × 10 (equal to the help for 10 of 14 units); `0x12`/`0x27` = ranges in 1/8 field (every help range equals one of them); `0x14` = unit slots (the crews of war machines exactly as the manual gives them; the troops a ship carries). **Guess:** `0x16` = price, `0x15` = upkeep per minute. `BAUWARE` = the goods a unit needs (sword and armour for the swordsman, P).

## OQ-024: What island `GFXID` values refer to, and the other island records
- **Status:** open
- **Update 2026-10-16 (item `state`):** resolved — `height << 8 | phase` (`docs/formats/insel.md`, "Placed objects"). Duplicate items and the remaining 115 overlapping `PROD2` tiles stay open.
- **Update 2026-10-16 (b) partly resolved:** `GELAENDE`/`PROD2`/`FLUESSE` items are placed `Anlagen.dat` objects: `u16 id | rotation << 14`, `u16 state`, `u8 x, u8 y` (block-relative top-left); footprint from `ANLAGEN` `HEAD` bytes 0–1 (`docs/formats/insel.md`, "Placed objects"). Open: `state` (mostly `0x3000`), why 97,041 `GELAENDE` and 485 `PROD2` items are exact duplicates, and the remaining 115 overlapping `PROD2` tiles.
- **Update 2026-10-16 (a) resolved:** `GFXID = texture << 8 | u << 5 | v << 2 | r` — a 32 × 32 cell of a 256 × 256 `Textures/Felder.tex` texture with `r` quarter turns; texture 0 = open water. Evidence: corpus statistics and top-down rendering (`docs/formats/insel.md`, "Ground"). Still open from (a): how open water is drawn (no texture 0 exists in any `.tex`), and texture 205 (36 tiles, absent from `Felder.tex`). The `Anlagen.dat` hypothesis below was a dead end for ground; it still holds for buildings (M1.4).
- **Area:** formats / islands / rendering
- **Question:** (a) What does a `GFXID` tile value (`u16`, 0–52,685) select — which graphic, from which file, with which rotation/variant? This blocks drawing terrain in the M1.6 viewer. (b) Layout of the 6-byte items in `GELAENDE`, `PROD2`, `FLUESSE` and of `INFO` (12 bytes), `BERGWERK2`, `OCCUR`, `ANBAU`, `DISCOVER`, `KONTORID`, `DUERRE`, `LOCKED`, `NAME`, `HIRSCH`, `EVENTINSEL`, `STAEDTE`. (c) `HEIGHT` units (0–208) and `FRUCHTBAR` values (0–10).
- **What we know:** grids and block layout (P), `docs/formats/insel.md`. Rejected for (a), measured over all 1,133 islands on 2026-10-16: `GFXID >> 5` as a `FigurenHeader.dat` id (ids exist, but the figures are civilians, mowers, explosions); `GFXID` as an `Anlagen.dat` `ANLAGEN` id (0.26 M of 6.5 M non-zero tiles) or `GFXID >> 5` as one (2.5 M); `GFXID` inside `[BODY2 u32 0, + frame count)` of an `ANLAGEN` sprite set (1.7 M of 7.5 M; only 159 distinct bases, max end 2,795). Low 5 bits are mostly 0–11. `Anlagen.dat` holds 921 `ANLAGEN` entries (ids 1–3998) with `HAEUSER` sprite sets in the figure layout, and three `TEXTURES` pixel volumes (`u32 group`, `u32 9633`, RLE data), so the terrain graphics are most likely there (M1.4).
- **How to resolve:** decode `Anlagen.dat` `ANLAGEN`/`HEAD`/`BODY2`/`GFXDATA` (M1.4) and look for a field whose ranges cover the `GFXID` values; Ghidra: the island loader (`GetAnnoInselFileData` in `AnnoWelt.dll`) and where `GFXID` is used to pick a sprite.

## OQ-025: Frame rows that run past the frame width
- **Status:** resolved 2026-10-16 — the blitters never consult the frame width; overrunning pixels are drawn to the right of the frame box, clipped only by the viewport (`docs/re/annodisplay.md` §3). `decode_frame_clipped` drops them for fixed-size frame images; see `docs/formats/anlagen.md`.
- **Area:** formats / rendering
- **Question:** 51 distinct `Anlagen.dat` frame ranges (`BSH_RGBAZ` 26, `BSH_RGBZ` 19, `BSH_RGBZM` 6) have rows whose runs end 1–16 pixels beyond the frame `width`, although their row count and end marker are exact. Does the original clip those pixels, draw them past the frame edge (into the next sprite column on screen), or wrap them into the next row?
- **What we know:** grammar and counts (P), `docs/formats/anlagen.md`, `anlagen_corpus`. `figuren::decode_frame_clipped` drops them for now. No figure frame overruns.
- **How to resolve:** Ghidra: the RLE blitters in `AnnoDisplay.dll` (`tBrush2Screen…`), check whether they bound `x` by the frame width; or compare a rendered frame with an in-game screenshot.

## OQ-026: Isometric projection details not yet calibrated
- **Status:** open
- **Update 2026-10-19 (river gaps):** the river height byte is the bed's lowest corner (`D_256x256_0045_5B`: 152 of 184 pieces; the banks lie 16 units higher), which makes the original's "terrain at most height + 10" test the painted bed's contour; enhanced rivers follow the painted bed (the sprites' own coverage) and an interpolating bed surface, tuned by `kBedEdge`, `kBedDepth`, `kRiverBankNoise` (`docs/render/iso.md`, "River gaps"). Open: whether the original's per-pixel `z` offsets make its bed contour differ from `height + 10` on the triangulated terrain; the stray single sprite pixels at a bed's rim that pass the depth test.
- **Update 2026-10-17 (rivers):** river sprites are now depth-tested against the terrain seen at each pixel; water shows where the terrain is at most the item height + 10 units, fitted to the painted riverbed. Open: the original's exact test (per-pixel `z` of `BSH_RGBWZ` frames) and the meaning of the per-item sub-offsets in the runtime chunk storage (`0x1009f870`: `pos = cell − (b & 7, (b >> 3) & 7)`).
- **Update 2026-10-17 (waterline):** the visible waterline is at `HEIGHT` 16, not at the sprites' 32: the painted ground textures change from water to sand between 16 and 17 (P), the land classifier threshold is 19 (D), surf items lie at 0 or 16 (P). The renderer now clips the sea at 16 against the terrain point visible at each pixel (a reproduction of the depth test); this removed the sea over painted sand and most straight beach edges. Still open: how the original clips sprites drawn at 1.28 world units at the 16 contour (per-pixel `z` offsets of `BSH_RGBWZ`?), and what `2 ·` in `2 · 16 · 0.04` stands for.
- **Update 2026-10-17 (open sea, colleague feedback):** the sea is now drawn everywhere, on a world-coordinate lattice, from a periodic pre-rendered image (`crates/render/src/sea.rs`). Open water (texture 0) shows the sea checkerboard over the colour of the scene's most common deepest floor texture (`Meer_Meer02.bmp` in `Single_01.sze`); the result, RGB (24, 92, 140), is close to the original's (28, 89, 138), so the colour parameter `0xC0` may matter less than assumed. Each island's floor fades into open water over 8 tiles towards its border. Still unverified against the original: whether open water has a floor texture, whether the lattice uses world coordinates, and what the original shows between islands (needs screenshots). The gap fill that made rivers and coasts tile-blocky is gone; rivers still show hard tile diamonds and seams (see the river note below).
- **Update 2026-10-17 (reference video):** the sea's loop period in a History Edition gameplay video is ≈ 0.8 s, matching one animation step per `ANIM` ms (16 × 50 ms) — the current timing. The original's open sea is darker (RGB 28, 89, 138 vs ours 37, 107, 142); likely the unapplied colour parameter `0xC0` (`docs/render/iso.md`, "Reference").
- **Update 2026-10-16 (animation):** sea, rivers and surf animate from their `ANIM` records; the original's tick unit, per-object animation phase and the height at which water objects are drawn are not traced (approximations in `docs/render/iso.md`, "Water", "Animation and layers"). Rivers whose channel tiles are open water (texture 0) look tile-blocky because of the open-water fill approximation.
- **Update 2026-10-16 (b, water):** the open sea is drawn as animated sprites of `Anlagen.dat` entry 2801 on a staggered lattice at sea level 32 `HEIGHT` units (`docs/re/annowelt.md`, "Sea surface"); implemented in `iso::draw_water`. Still open: how the per-chunk sea bit is set, what shows under open water (texture 0), the animation tick unit, and surf/river objects (`FLUESSE`). Tile triangles (B–C diagonal) and 6 × 6 ground cells confirmed and implemented; per-corner light formula known, light vector not traced.
- **Update 2026-10-16:** (a) resolved by static analysis — heights per corner, one unit = 0.04 world units ≈ 2.06 px at zoom 0 (`docs/re/annowelt.md`, `docs/render/iso.md`); still open: triangle split and the per-corner light byte. (b) open water and ground textures 1–10 are not in the terrain mesh (`AnnoWelt.dll` `0x1009b920`); their drawing path is not found. (d) shadows = 5/8 brightness, depth-tested, not depth-writing (confirmed); default animation frame still open.
- **Area:** rendering
- **Question:** (a) How `HEIGHT` (0–208) raises the ground: pixels per unit, per-corner heights and slopes. (b) How open water (texture 0) is drawn. (c) Exact draw order for overlapping multi-tile objects and for objects vs. raised ground. (d) Which animation frame is shown at rest, and how shadows (`SHADOW` sets, `BSH_ALPHA`) are blended. (e) Whether the anchor rule holds for every footprint (castle 8 × 8: anchor + half diamond = 621 vs sprite height 616). (f) Draw order between the islands of a scenario (only matters where tall sprites rise into a neighbour's rectangle) and what the original shows between islands.
- **What we know:** tile size, tile placement, ground cell mapping and the anchor rule from sprite measurements and renders (`docs/render/iso.md`). Flat raised tiles with lift 1/2 or 1 px per unit show terraces and gaps, so (a) needs slopes.
- **How to resolve:** screenshots of the original at known camera positions (needs an x86 Windows/Wine host); Ghidra: the terrain and sprite blitters in `AnnoDisplay.dll`/`View.dll`.

## OQ-027: Scenario player and unit fields
- **Status:** mostly resolved 2026-10-17 — (a), (c), (d), (f) resolved; (b) likely; (e) partly
- **Update 2026-10-17 (d) resolved:** `attr` is the starting facing — ships 16 steps of 22.5°, soldiers 8 directions (D: `AnnoWelt.dll` `0x1001e01d`–`0x1001e041`, verified; `docs/re/fire-and-ships.md` §3.3). Open: how facing plus view rotation selects the sprite frame.
- **Update 2026-10-17 (`docs/re/annoscene.md`):** (a) the long header is a 24-bit length escape that the add-on writers reserve before knowing the length (D). (b) player 31 is skipped by every game-start loop; neutral/scripted owner likely. (c) `STADT2`, `CREW/ENTRY`, `ESEL` prefixes decoded (`docs/formats/sze.md`, "Nesting"). (d) `attr` is figure property `0x16`, used as a facing in 45° steps (soldiers likely; ships' 16 steps a guess). (f) colours are stored per player in `HEAD +0x4C` and assigned at game start (`sze.md`, "Players"). (e) `HEAD` fields decoded; `WAREN`, `PRIVAT`, `TASK`, `STADT2` contents open.
- **Area:** formats / scenarios
- **Question:** (a) Why `ANNOADDON1` files write every `STADT2` and `STADT2/PROD` record with the long header (`0xFF00` + `u16` length), even 22-byte ones, while `ANNO` files never do — a writer version flag, or a length that may exceed `u16` in the add-on's own saves? (b) What player 31 is (present in every scenario, last in the header series, missing from the state series in 22 of 23 `ANNO` files): nature/neutral, pirates, or the "no owner" slot? (c) The prefixes before nested records: `STADT2` 6 bytes (`u16` 0–8, then two `u16` that look like tile coordinates on the 1024 maps, e.g. 655, 476), `CREW/ENTRY` 6 bytes (looks like `u16 id`, `u16 figure`, `u8 owner`, `u8` — the start of a unit prefix), `ESEL` 8 bytes. (d) Unit `attr` (byte 0x05): ships 0–14, soldiers 0–7. Ship sprite sets have 16 frames per zoom group, soldiers' presumably 8 directions, so it may be the facing; unverified. (e) Contents of `HEAD`, `WAREN`, `PRIVAT`, `TASK`, `STADT2` and the `SCENE` children other than `SPIELER`/`INSEL5`/`INSELLNK`. (f) Player colours: ship entries carry only two colour tables (e.g. 1505: `MittleresHandelsschiff01.PCX` and `Schiffe\MittleresHandelsschiffKaputt01.pcx`, damaged), so sails and flags must be recoloured per owner at runtime; candidate: the one `Anlagen.dat` table with flag 4, `Flagge01_01.PCX` (OQ-022 a). **Update 2026-10-17 (f) traced:** the rule, the eight colours and which figures get them are confirmed (`docs/re/annodisplay.md` §8, `docs/re/annogame.md`) and implemented (`anno_assets::figure::recolour`). Still open: the colour the human picks on the colour-selection screen (the viewer uses the lowest free colour; the rest of the game-start assignment is traced, `docs/re/annoscene.md` §9, and implemented as `start_colours` — the earlier stand-in "player `p` → colour `p + 1`" is gone), the colour parameter of buildings (flags on `Anlagen.dat` table 109), and ships' flags (separate flag figures?). Earlier note: soldier palettes (figure entries 700–724) and flag palettes (`Turmflagge01`, `Schiffsflagge01_01`, `Ausschleusflagge01`, `Anlagen.dat` table 109 `Flagge01_01`) hold ramps of pure green `(0, g, 0)` (g = 255, 239, 222, … 107) at palette indices that differ per palette; drawn unchanged they show as neon-green sleeves, trousers, hat bands and flags (P, rendered at zoom 0–2). So the marker is the colour value, not an index range; the remap rule and the player colours themselves are not traced yet. Ship palettes have no pure green; a few smoke and grass palettes have isolated pure greens (g = 1, 8, 16, …) that are presumably not markers.
- **What we know:** layouts and counts (P), `docs/formats/sze.md`, `crates/formats/tests/scene_corpus.rs`.
- **How to resolve:** (a) look for the record writer in `AnnoScene.dll` (strings `STADT2`, `PROD`); (b)–(e) Ghidra on the scenario loader (`AnnoScene.dll` exports `AnnoScene`, `SaveGame`) and the unit classes; (d) also visually: draw ships with `attr` as facing and compare with screenshots of the scenario start.

## OQ-028: What an island link (`INSELLNK`) resolves to
- **Status:** resolved 2026-10-17
- **Update 2026-10-17:** a random real template of the same climate and size replaces each dummy at game start (`docs/formats/sze.md`, "Island links"; `docs/re/annoscene.md` §3–5). The viewer keeps a deterministic stand-in (no game seed).
- **Area:** formats / scenarios / rendering
- **Question:** Every `INSELLNK` names one of the 21 `INSEL4` `*_dummy.scp` templates of the same size (polar, tundra, nord, steppe, praerie, dschungel). Does the game place that dummy island, or pick a (random, seeded?) island template of the same climate and size at game start, as the "dummy" name and the endless-game scenarios (six of them have only links) suggest? If random, which seed and which candidate list?
- **What we know:** layout and corpus facts (P), `docs/formats/sze.md`, "Island links". The dummies are complete `INSEL4` islands, but flat grass squares without coast or objects (rendered 2026-10-17), so a replacement at game start is likely. Every climate/size pair used by a link has 6–80 real templates. Viewer stand-in until resolved: a real template of that climate and size chosen by island number (owner decision 2026-10-17; `docs/render/iso.md`, "Scene composition").
- **How to resolve:** Ghidra: who reads `INSELLNK` (`AnnoScene.dll`/`AnnoWelt.dll`, export `GetAnnoInselFileData`); compare two starts of the same endless scenario in the original (needs an x86 host).

## OQ-029: Record structure and meaning of the plain-text data files
- **Status:** open
- **Area:** formats / data / sim
- **Question:** (a) `BGruppen.dat`: which key starts a record in `[WAREN]`, `[BGRUPPE]`, `[BAUSTUFEN]`; units of `Preis`, `Menge`, `Priority`, `Force`, `Stufe`, `BauFirst`/`BauLast`, `Bonus`, `PreisLevel`, `Forsch`, `Wohnmax`. (b) `Wachstum.dat`: what the 6 × 6 `Proz` grid per ground texture `Id` means (growth chance per ground cell?), and `Klima`, `Bridge`. (c) `wegspeed.dat`: the `Ob`/`Un` pairs and the eight `Mask` bit rows (walkability per slope class, per the German comments). (d) `Highmap.dat` `Tab` rows per `Id`. (e) Does the game tolerate the `Felder.dat` line `Pos     0, 0` (no colon) or is the file build-only? Which of the `[FILE]/[COLOR]/[TEXTURE]` files are read at runtime at all, rather than by the texture build tool? (f) How the game addresses the plain `Texte.dat` lists (by position within the section?). (g) `Wachstum.dat` line 765 lacks a comma (`100, 100, 100   10,    20,  50`): does the original read six numbers (whitespace also separates), five, or stop?
- **What we know:** the line grammar, counts and record layouts (P), `docs/formats/data-text.md`, `crates/formats/tests/text_corpus.rs`; `[ANLAGEN]` maps `Anlagen.dat` ids to names (P); `Wachstum.dat` ids are `Felder.tex` texture ids and its 6 × 6 grids match the 6 × 6 ground cells per texture (P), which makes (b) a per-cell value.
- **How to resolve:** Ghidra: the text-file reader (strings `Preis`, `Proz`, `WALKMASK`, `BGRUPPE`, `[ANLAGEN]`, `SHIPS`) and its callers; differential saves (PLAN M2.1) for prices and growth.

## OQ-030: 16-bit colour of the original
- **Status:** resolved 2026-10-17 — owner decision: the 24-bit palette colours are the reference; the 16-bit quantisation is not reproduced.
- **Area:** rendering / fidelity
- **Question:** the original converts every palette colour to RGB565 (or RGB555) before drawing (`AnnoDisplay.dll`, `docs/re/annodisplay.md` §8): red and blue lose their lowest 3 bits, green its lowest 2, with rounding `(c + 4) >> 3` / `(g + 2) >> 2`. The rebuild draws the 24-bit palette colours, so it is slightly smoother than the original ever looked (differences of up to 4 per channel, visible as banding in gradients in the original). Should the faithful mode reproduce the 16-bit quantisation (an output-stage option), or is the 24-bit palette the reference?
- **What we know:** the conversion formulas (D). The palettes themselves are 24-bit (P).
- **How to resolve:** owner decision; then, if wanted, a `rules`/render option `colour_depth = 16 | 24` applied when palette colours are converted.

## OQ-031: Font header sizes and glyph levels
- **Status:** open
- **Area:** formats / fonts / UI
- **Question:** (a) what the two `u16` at the start of a `.zei` `HEAD` mean (`size_a` 10–31, `size_b` 13–31; the glyph row count equals `size_b` in four fonts and is 4–6 more in the others); (b) how the game draws the three pixel levels 1–3 (text colour, shadow, highlight? blended?); (c) why the two `Textures/Scape*.zei` have bytes after the NUL in their chunk name fields (`e0 e1 e2 e3`, `u32 16`) — a different tool version?
- **What we know:** layout and counts (P), `docs/formats/zei.md`.
- **How to resolve:** Ghidra: the font loader and text blitter (`AnnoDisplay.dll`/`AnnoFrame.dll`, strings `CHARSET`, `.zei`); screenshots of the original UI text.

## OQ-032: x87 precision when the original loads `Anlagen.dat`
- **Status:** open
- **Area:** simulation / determinism
- **Question:** which x87 precision control is in effect while `AnnoGame.dll` converts the `PROD3` floats (`0x10009627`–`0x10009717`): the MSVC default of 53 bits, or 24 bits if a Direct3D device was created earlier without `D3DCREATE_FPU_PRESERVE`?
- **What we know:** exact emulation over all 921 entries shows 53- and 64-bit precision give the same integers. 24-bit precision changes two of them: `need1` of 1010 (6 → 7) and 3979 (3 → 4) (`docs/sim/production.md`, "Load-time conversion"). The rebuild uses the 53-bit values.
- **How to resolve:** read the FPU control word at that point under a debugger (fixture F1 in `docs/re/production.md`), or check whether any `_control87` call or Direct3D device creation happens before the scenario load.

## OQ-033: Owner check before building status icons
- **Status:** resolved 2026-10-18 — owner slot `+0xc4(3)` returns the player's Volk (people) number (`player+0x7a`, world property `0x1c`); the icon path proceeds only for Volk 1, which human and computer-opponent seats (player kinds 8, 11, 12) have; natives, pirates and Venice have another. Rebuild: `StatusTracker::shows_icons` = owner Volk == 1, supplied by the player state (`docs/sim/state.md` §4.5, §7.1; `docs/sim/production.md`).
- **Area:** simulation / UI
- **Question:** `AnnoWelt.dll` `0x10073010` (status icon on/off) proceeds only if the building's city owner returns 1 from slot `+0xc4(3)` (`0x1007302f`). The same owner "value 3" appears in `docs/re/fire-and-ships.md`. What is it — human player, the local player, a difficulty setting? Without it, a building's status code is never cleared by production (`0x10072fb0` needs a shown icon).
- **What we know:** the control flow (D); `docs/sim/production.md`, `StatusTracker::shows_icons`.
- **How to resolve:** trace the owner class's slot `+0xc4`; observe icons on AI buildings in the original.

## OQ-034: Goods requirement of the weapons workshop lines
- **Status:** resolved 2026-10-18 — `b` is the goods record's `Forsch` research id (`BGruppen.dat` `[WAREN]`, research id kind `0x38`); the bit is bit 1 ("researched") of the player's research state byte (player `+0xb0` table, set by the FORSCH/ENTRY flag bit 0). Goods with `b ≠ 0`: SCHWERT and RUESTUNG 1, LANZE 3, BOGEN 4, AXT 6, ARMBRUST 9, MUSKETE 12, KANONESHIP 38. Rebuild: `WeaponsWorkshop::unlocked` = `Forsch == 0 || researched(owner, Forsch)` from the owner's research state (`docs/sim/state.md` §7.1, Appendix A).
- **Area:** simulation
- **Question:** the `HANDWERKWAFFEN` tick (`AnnoWelt.dll` `0x10069550`) lets line *j* produce only if a byte `b` for its output goods is 0 or the owner reports bit 1 for it. The byte is at offset `0x18 + 26·g` of the table returned by owner slot `+0x6c` → slot `+0x08`. The bit is from owner slot `+0xbc` → slot `+0x08(b)`. What are `b` and the bit — a research (Forschung) id and "researched", or something else? Which goods have `b ≠ 0`?
- **What we know:** the control flow (D, `docs/re/production.md` §4.1a). Every line of the three weapons workshops (bows, crossbows, muskets, ship cannon, axes, lances) is a weapon of the military tree. The rebuild takes the result as an input (`WeaponsWorkshop::unlocked`).
- **How to resolve:** trace who fills the per-goods table (26-byte records) and the owner's slot `+0xbc` object; observe in the original whether a newly built bow maker makes crossbows before research.

## OQ-035: Deposit depletion setting, reserve flag and start amounts
- **Status:** open (a) resolved 2026-10-18: world property 14 bit 1 is the `ANNOADDON1` edition flag (AnnoScene `0x1000265e`–`0x1000268a` sets it from the root name; AnnoWelt `0x10063686` reads it), so depletion exists only in the add-on edition and is not an owner option; residual doubt: `[deposit+8]` is the island. (b) and (c) remain open.
- **Area:** simulation
- **Question:** three things about ore deposits (`AnnoWelt.dll` class `0x100637a0`, `docs/re/production.md` §4.4):
  - (a) the deposit's extraction (`0x10063670`) uses anything up only if world setting 14 has bit 1 set (`deposit+8 → +0x18 → +0x74(0x0e) & 2`). Which game option is that (likely "resources run out"), and what is its default?
  - (b) the reserve flag `+0x30` is 1 when a deposit is created. Who clears it? While it is set, deep mines never use the deposit up.
  - (c) deposits start with 2,000 t (`0x7d000`). Do scenario or savegame records set other amounts, and how is a deposit's goods (iron, gold, salt, gems) stored?
- **What we know:** the extraction and lookup rules (D). The rebuild takes the setting as an input (`depletes`) and starts deposits at 2,000 t with the flag set.
- **How to resolve:** trace the writers of `+0x28`/`+0x30` (savegame load, island object factory `0x10094e00`) and world slot `+0x74`; observe in the original whether a deep ore mine ever runs out.

---

# Decisions for the owner (M3 state model)

Source: `docs/sim/state.md` §13 (rows D1..D18) and the decision-needed items of the four M3 reverse-engineering reports (building creation, group lists, world tick, command set, 2026-10-18). Rule (owner): a deviation is **fix** when it is obviously unintended (removed in the rebuild and documented, no switch) and **decision-needed** when it could change the feel of the game. For every decision-needed item below the **faithful behaviour is the implemented default**; the alternative is an opt-in field of the `Rules` config (`SimConfig`/`Rules` are hashed, so peers with different options diverge visibly at tick 0). Nothing here has been decided; each entry waits for the owner. Entry order = D1..D18 of the spec.

## OQ-036: Reuse of freed building-list slots (spec D1)
- **Status:** open, owner decides
- **Area:** simulation / economy order
- **What:** each city keeps its buildings in six lists. A freed slot is reused only when the wrap-around scan from the last stored slot + 1 reaches it (not the lowest free slot); lists never shrink or compact; the scheduler's `n` counts every hole (AnnoWelt `0x10026a00`, `0x10026b30`, `0x100255c0`, `0x10023780`-`0x10023a4d`).
- **Why it may change the feel:** the order in which buildings tick (who claims goods first, tie-breaks) depends on the whole create/remove history, so compaction or lowest-free reuse changes economy micro-timing.
- **Faithful default implemented:** replicated exactly (`SlotPolicy::Original`; `SlotList::insert` in `slot_list.hpp`, tested against a naive transliteration of the original's loop, M3 stage S2).
- **Rules option:** `Rules::slot_policy` (`LowestFree` opt-in; compaction is not offered).

## OQ-037: Order of rect queries and `FindNext` (spec D2; RE "creation")
- **Status:** open, owner decides
- **Area:** simulation / determinism
- **What:** the six spatial sets are sorted lazily with an unstable quicksort keyed only on y and removed by swap-with-last; rect queries and `FindNext` return that set order (`0x10027000` sort, `0x100255c0`/`0x10026c70` swap-remove, `0x10023380` query order, `0x10023f50` mask order 1, 4, 8, 0x10, 0x20, 2). Ties and post-removal order are implementation-defined.
- **Why it may change the feel:** if market catchment, service coverage or stall detaching iterate a rect query, the choice among candidates depends on this accidental order; no faithful order is reproducible without transcribing the quicksort and its lazy-sort timing.
- **Faithful default implemented:** not possible; the default is a canonical total order `(list in rect_order, y, x, id)`, a documented deviation. The original order is a reserved value.
- **Rules option:** `Rules::spatial_order` (`YXSlot` default; `Original0x10027000` reserved, rejected by `Rules::validate()` until someone transcribes the sort, spec O-7). The owner says whether that transcription is worth doing.

## OQ-038: Events enqueued in front of the saved next node (spec D3)
- **Status:** open, owner decides
- **Area:** simulation / events
- **What:** the timed-event pop loop saves the next node before dispatch, so an event a handler enqueues in front of it waits for the next advance, one behind it is seen at once (`0x100add90`).
- **Why it may change the feel:** timing of cleanup and damage events depends on queue order and step size.
- **Faithful default implemented:** replicated (including the saved-next quirk).
- **Rules option:** `Rules::events_drain_new` (process everything due, including new events).
- **Guard (fix, not a switch):** one pop handles at most `Rules::events_max_dispatch` events (default 2^20, a limit) and then stops and reports it; the rest stays queued for the next advance. Without a cap, `events_drain_new` with a handler that always enqueues a delay-0 event never ends (the default loop can be kept running the same way by enqueuing behind the saved node). Below the cap the behaviour is identical. Related entry: OQ-056 (cancel and unlink order).

## OQ-039: Delayed hits land one step late (spec D4)
- **Status:** open, owner decides
- **Area:** simulation / combat timing
- **What:** delayed hits are enqueued with delay 0 during the unit (figure) phase, after the queue was processed in that advance, so they land in the next advance: 5-20 ms later in multiplayer, up to 100 ms in single player (`0x1003593e`, `0x100add90`).
- **Why it may change the feel:** damage timing differs between SP and MP step sizes; a variable-step port changes results.
- **Faithful default implemented:** replicated.
- **Rules option:** `Rules::events_flush_after_figures` (second event pass after the figure phase).

## OQ-040: The 30 s monopoly refresh runs only once (spec D5)
- **Status:** open, owner decides
- **Area:** simulation / scenario goals
- **What:** the MONOPOL tracker refresh (`0x1009d2a0`) runs on the first advance only, because its accumulator (`world+0xe8`) advances only inside the gated body (`0x1009d2a0`-`0x1009d2ce`). Almost certainly meant to run every 30 s.
- **Why it may change the feel:** fixing it makes monopoly owners and acquisition times change during play; monopoly-based scenario goals or AI behaviour that read the table would behave differently.
- **Faithful default implemented:** the table is frozen after the first advance, as in the original.
- **Rules option:** `Rules::monopoly_every_30s`.

## OQ-041: Player table ticks a fraction of the players per advance (spec D6; RE "world-tick")
- **Status:** open, owner decides
- **Area:** simulation / economy timing
- **What:** `min(d*32/50 + 1, 32)` players are ticked per advance (`0x1009c4d0`), `d` in ticks of 2 ms: 2, 4 and 7 players per advance for the multiplayer steps of 5, 10 and 20 ms, all 32 only from d >= 49 (about 98 ms, a single-player step near its cap). The AI ticks every advance.
- **Why it may change the feel:** upkeep and bankruptcy timing per player depend on step size, so SP and MP differ.
- **Faithful default implemented:** replicated (the cursor schedule).
- **Rules option:** `Rules::player_table_full_tick` (all players every advance, or a fixed per-tick schedule).

## OQ-042: The 300 s drought event (spec D7; RE "world-tick")
- **Status:** open, owner decides
- **Area:** simulation / random events
- **What:** the 300 s gate draws one PRNG value per island per gate even for islands without climate or cities (`0x100939b5`), and the 300 s event's drought is always centred on the first market/Kontor-group building of the lowest-numbered city, never random (`0x10093a10`, single caller: the 300 s event). Scripted droughts from AnnoScene are a different path (see OQ-054, "scripted disasters").
- **Why it may change the feel:** the draw order must stay for determinism; randomising the centre or disabling droughts changes where and how often droughts hit.
- **Faithful default implemented:** replicated (draw kept, centre fixed).
- **Rules option:** `Rules::drought_random_centre` (position of the 300 s event only; the draw is kept).
- **Implementation status (stage S7):** the 300 s event, its one draw per island per gate and the faithful centre (the first building of the market list of the first city that has one, floored to the chunk edge) are implemented (`start_drought`, `scheduler.cpp`). The option is a **reserved value**: `Rules::validate` rejects `drought_random_centre == true` ("reserved: not implemented (OQ-042)", like `prng_mode == PerIsland`), because the spec does not say what a random centre is (which tiles, how many draws, which draw site, whether it must be on a city's land), and every answer changes the PRNG timeline. An accepted option that did nothing would let a host or a peer believe it changed the game (S7 review round). The owner decides before it is implemented; the rejection is lifted then. The drought machine itself (growth of the area, the plateau, the retreat) is stage S13; a drought the event starts keeps its start record until then. With more than 32 city slots no drought is started at all (spec O-55: the area's city mask is a `u32`). **Since stage S13 (2026-10-23):** the machine runs (`drought_tick`, read from `0x1008aeb0`: 11 200 ticks out, 120 000 of plateau, 11 200 back with the defaults), the city mask has `ceil(city_slots / 32)` words (O-55 closed), and the constructor floors the centre; what the drought does to the tiles and the cities is still a seam (OQ-140), so a drought changes no tile yet.

## OQ-043: Drag build continues after a failed record (spec D8; RE "commands")
- **Status:** open, owner decides
- **Area:** simulation / building UX
- **What:** op 6 (`0x64`) continues with the next record when one fails (not enough money or material, blocked tiles) and spends money and goods record by record, so a drag-placement can end partially built; op 9 (`0x87`, editor) stops at the first failure (`0x10058a6d`, `0x10058b3b`).
- **Why it may change the feel:** this is the feel of dragging a road or wall with insufficient funds.
- **Faithful default implemented:** build as many as can be paid, in stream order.
- **Rules option:** `Rules::build_atomic` (all-or-nothing).

## OQ-044: Demolish removes the whole city when it is empty (spec D9; RE "commands")
- **Status:** open, owner decides
- **Area:** simulation / building UX
- **What:** demolish removes the whole city (and its stock) from the island when city slot `+0x78` (`0x1002f7f0`, predicate unread) returns 0 after a tile was demolished, with no confirmation (`0x10058f80`). Probably the last Kontor/warehouse.
- **Why it may change the feel:** a gameplay-visible loss of a city; the owner may want a guard.
- **Faithful default implemented:** hook only; the predicate is unread, so by default no city is ever removed (`Seams::city_is_empty` = false). The faithful predicate must be transcribed first.
- **Rules option:** `Rules::keep_empty_city` (opt-out reserved).

## OQ-045: The 7.5 min accumulator discards its overshoot (spec D10)
- **Status:** open, owner decides
- **Area:** simulation / random events
- **What:** the event 7.5 min accumulator is reset to 0, so the overshoot is discarded (`0x100ae16e`, `0x100ae140`) and the schedule of its PRNG draws depends on step size.
- **Why it may change the feel:** under variable single-player steps the repair (`acc -= 450000`) shifts when the event's draws happen and therefore every later random outcome.
- **Faithful default implemented:** discard (`acc = 0`).
- **Rules option:** `Rules::event750_keep_overshoot`.

## OQ-046: One global PRNG versus per-island streams (spec D11)
- **Status:** open, owner decides
- **Area:** simulation / determinism / performance
- **What:** the original consumes one world PRNG in sequential island order (`0x100a1740`). Per-island streams over the shared table (cursor = splitmix64 mix of a seed and the island slot) would make island ticks pure and parallel without prediction.
- **Why it may change the feel:** it changes random outcomes (events, small-farm animal rolls); same distribution, different sequence.
- **Faithful default implemented:** one global stream. `PerIsland` is **design-only**: a reserved `Rules` value that `Rules::validate()` rejects until stage S16 builds the per-island cursor state (field list, hash scope, save entry, header seed).
- **Rules option:** `Rules::prng_mode` (reserved).

## OQ-047: Figures never return in the interim (spec D12, stand-in)
- **Status:** open, owner decides
- **Area:** simulation / economy pacing
- **What:** in the original, carriers, workers and animals walk and return; the trip length is unread (unit slot `+0x74`). Until figure movement is ported a spawned figure never returns, so its counter stays raised.
- **Why it may change the feel:** it directly limits how many carriers and workers a building can have out, and therefore economy pacing, until figures are ported.
- **Faithful default implemented:** never returns (stand-in; inert).
- **Rules option:** `Rules::interim_figure_return_ticks` (0 = never; N = return after N ticks).

## OQ-048: Figure positions as fixed point; floats on the wire (spec D13; RE "commands")
- **Status:** open, owner decides
- **Area:** simulation / determinism
- **What:** figure positions and speeds are x87 floats in the original (`0x10059220`), with no oracle; several commands carry f32 (`0x7a` speed, `0x6e` spawn x/y and scale; `0x1005e230`, `0x1005d510`). The port uses Q24.8 fixed point, integer speeds and a speed in integer percent.
- **Why it may change the feel:** path and arrival ties and speeds may differ from the original; the format is the owner's call before figures are ported.
- **Faithful default implemented:** not possible (floats cannot be in checksummed state); Q24.8 is the stand-in.
- **Rules option:** none (a fixed-point format choice, spec O-47).

## OQ-049: Original command encoding versus plain lists (spec D14; RE "commands")
- **Status:** open, owner decides
- **Area:** net / replays
- **What:** the `0x64` group codec makes followers inherit `rec+7` from the group head and merges records only on equal type and `rec+8`; followers are relative to the head, at most 15, |d| <= 127 (`0x1005c8cf`, `0x1005cc07`, `0x1005e230`). The port uses plain widened lists on the wire.
- **Why it may change the feel:** no game rule changes; it only decides whether original-format replay import is wanted.
- **Faithful default implemented:** plain lists; the original codec exists only as an optional replay-import decoder (`legacy::decode_0x64`).
- **Rules option:** none (a tool choice).

## OQ-050: Unported class tick bodies are inert (spec D15, stand-in)
- **Status:** open, owner decides
- **Area:** simulation / buildings
- **What:** houses, markets, public buildings, military, hunters, ships, doctors, schools and the castle do something after their gate in the original; the bodies are unread (`0x10076230`, `0x100702d0`, `0x10070c62` and the other class ticks). The gate periods (500, 2500 houses, 5000 stalls and the inn, 166 towers and cannon towers; the compare constants of every class tick are read, `docs/sim/state.md` Appendix C) are carried as data only; nothing invents a body.
- **Why it may change the feel:** the hashed `last` fields and the future timeline of those classes move once real bodies arrive.
- **Faithful default implemented:** inert (`TickKind::None`, `last` untouched).
- **Rules option:** `ClassInfo::tick` per class (data).

## OQ-051: Unported subsystems are inert hooks (spec D16, stand-in)
- **Status:** open, owner decides
- **Area:** simulation / determinism
- **What:** the city passes (including random events), the fauna sweep, the AI, the drought tile effects and the event handler bodies draw from the PRNG and mutate state in the original (`0x1002ba80`, `0x1009366f`, CompLogic, `0x1008aeb0`, `0x100adae0`, `0x100adb80`). They are inert hooks at the exact original call positions and the PRNG timeline is recorded as short (`kPrngTimelineVersion`, hashed, in the replay header).
- **Why it may change the feel:** random events, wildlife and computer opponents are absent until ported, and the PRNG cursor diverges from the original at the first would-be draw.
- **Faithful default implemented:** inert hooks.
- **Rules option:** `Seams::*` (named, hashed).

## OQ-052: Placement validity is a stand-in (spec D17, stand-in)
- **Status:** open, owner decides
- **Area:** simulation / building UX
- **What:** the island placement check (terrain, water/coast/mountain via the type flag word, overlap rule) is unread (`0x1007c790` through island slot `+0x6c`, AnnoGame `0x10008920`, `0x10008f90`). The default rule is: inside the island, owner tiles, no overlap.
- **Why it may change the feel:** Build can accept placements the original refuses (mines off mountains, fishers inland).
- **Faithful default implemented:** the stand-in rule (the best available, not the original).
- **Rules option:** `Seams::placement_rule`.

## OQ-053: Deposit goods and amounts are placeholders (spec D18, stand-in)
- **Status:** open, owner decides
- **Area:** simulation / scenarios
- **What:** deposit goods and amounts are assigned by the scenario generator (`0x10078960` BERGWERK2, OQ-035 b/c). Default: scenario-given goods, 2000 t, reserve flag; depletion only in the add-on edition (OQ-035 a).
- **Why it may change the feel:** mine yields and run-out times in loaded scenarios are placeholders until the generator is read.
- **Faithful default implemented:** the placeholder values above.
- **Rules option:** `ScenarioSpec::DepositSpec` (data).

## OQ-054: Open research items from the M3 reverse-engineering reports (not blockers)
- **Status:** open (compact list; each item has an extension point or a default in `docs/sim/state.md` §12, listed there as O-n)
- **Area:** reverse engineering
- **Building creation (RE "creation")**
  - No direct caller of the building-index `Attach` (`0x100259c0`, vtable `0x100c2e48` slot `+0x28`) / `Detach` (`+0x2c`, `0x100260f0`) found: who passes an explicit slot (O-2).
  - Meaning of type slot `+0xd8` (32-bit mask feeding index counters `+6..+0x45`), type slot `+0xcc(2, x)` (running cost?), building slot `+0xc4(0x8e)` per class and building slot `+0x90` values other than 1 (O-9).
  - `RemoveAt` tail not read: PRODTYPE switch `0x10029540`, goods `0x51`-`0x56` handlers (`0x1002c740`, `0x1002cb20`, `0x1002c090`, `0x1002c9c0`, `0x1002d2a0`), message record for owner value `0x2c`, island slot `+0x7c` tile clearing, the third `RemoveAt` argument (O-5).
  - `Place` pre/post `Create` details: market/Kontor in-place replacement (`0x100279a0` branch `0x10027b62`, FindByTile mask 2, island slot `+0x150`, city slots `+0xc0/+0xbc`), land claim via island slot `+0xe8` for PRODTYPE `0x21`-`0x23`, well handler `0x1002cf60`, PRODTYPE `0x44/0x49/0x4e` hooks, tile-list fill for p `0x81/0x103`, island slot `+0x78` (O-4).
  - Meaning of record bytes `rec[7]`/`rec[9]`, building `+0x0f`, `+0x0e` (property `0x2b`, COMP chunk), `+0x0d` status bits beyond `docs/re/production.md`; bits 6-7 of `+0x0c` are never read (O-38).
  - City slot 0 (`0x1002f600`) increments byte `[city+0x17]`: user unknown; city byte `+0x16` and u32 `+4` only partly known (O-40).
  - Other building classes' `Load` slots (`+0x7c`): do they restore `+0x0a`/status time? Only the workshop `Load` (`0x10068640`) was read (O-39).
  - Whether island clear / city destroy compacts or rebuilds lists: only the list destructor `0x10023610` was found (O-48).
  - Unit/figure registry id allocation versus the cosmetic hook (island `+0x28` decor container): assumed separate.
  - The MP check that `tNetClientNet+0x10` is never set elsewhere relies on a scan of stores to `[reg+0x10]`; a store through an unusual addressing form could have been missed (O-50).
- **Group lists and placement (RE "groups")**
  - Which AnnoWelt/AnnoFrame/View code tests the type flag word (`+0x12` bits `0x2000` coast, `0x1000` mountain, `0x4000` road/bridge) and the per-tile WALKMASK/SHOTMASK/OWNMASK (`+0x88/+0xe8/+0x148`) against island terrain; the type-side helpers (`0x10008f90`, `0x10008920`) are read, their AnnoWelt callers are not (O-27).
  - Consumers of HEAD `[0x02]`, `[0x0b]`, `[0x0c]`, `[0x12]`, `[0x13]`, f32 `[0x14]`, `[0x1a/0x26/0x28/0x2a]`, `[0x20]`, `[0x24]/[0x25]/[0x2e]`, `[0x2f..0x30]`, `[0x32]` (looks like a text id, ~1600-1700, unverified) (O-21).
  - Where a building's research id (`+0xec`) gates placement or UI availability, and against which player state (only the deep-iron-mine use is known) (O-30).
  - Argument 2 of `PlaceBuilding` (city slot `+0xac`), i.e. the initial `last` of player-placed buildings: stored as time in `last`/status time, the dispatcher that supplies it was not traced (inferred: current tick); the command-to-`PlaceBuilding` path for `0x64`-`0x67`/`0x87` is open (`docs/re/tick-loop.md` names `0x1005c7f0`, which is a float helper).
  - Meaning of player slot `+0x9c(7)` (cost waiver in `PlaceBuilding`) and `+0x68` (assumed spend money); whether anything checks that the city can afford the goods before `PlaceBuilding` (O-9).
  - Source of deposit goods: who writes `[deposit+4]` and the mapping of the 9 ERZE kinds to goods ids 1-6 and others; amounts per deposit (OQ-035, O-37).
  - Consumers of the secondary per-group sets (`+0x178/+0x190/+0x130/+0x160/+0x148/+0x118`) and of the live counts at list `+8`; unread tick bodies: JAGDHAUS `0x10069f40`, WERFT `0x10074e90`, KANONENBAU/MILITAR `0x1006cc40`, WALFAENGER `0x10073720`, SCHLOSS `0x1006f1d0`, public buildings `0x1006b4e0/0x100755f0/0x100676b0/0x10067b50/0x10070c50/0x1006ab50`, MARKTKOMBI `0x1006c2e0`, houses `0x10076230` (O-23, OQ-050).
  - Whether the stream loader `0x100264a0` can be handed type ids of groups >= `0xc0` (it would create default-class objects in the production list; the rebuild refuses them).
- **World tick (RE "world-tick")**
  - CompLogic AI tick (profiler section 6, every advance from the player table update): PRNG draws, floats, determinism across peers (O-1; overlaps OQ-018).
  - Player group objects (class `0x1900`, 30 per player, tick `0x10062a40`): what they do after their 500-tick gate, where `[+0x330]` is updated, who creates them (O-20).
  - Consumers of the MONOPOL table (28 entries: 20 raw materials + 8 others; likely the BigBrother MONOPOL victory condition); dynamic check that `world+0xe8` never advances (O-24, O-16).
  - AnnoScene handler of message event `0x15a` (the 7.5 min event) and what gameplay it triggers (O-16).
  - Drought tile-level effects (object ids `0xe2/0xe3`, +2/-1, island slots `+0x140/+0x188`, message `0x17/0xce`) and chunk flag bit 1 consumers (O-15).
  - Island 15000-tick sweep `0x10093480`: spawn rules, table `0x100e34e8`, exact draw count; whether the 2000-tick growth `0x1008b5f0` draws via callees (O-12, O-13).
  - Island object manager traversal inside and across chunks (`0x10095770` bitmap scan, `0x10095ad0`) and each object's slot `+0x6c` tick (fire, plague, volcano, deposits), with PRNG order; the original is a do-while over a tile cursor (O-12).
  - **Island message manager** (`[island+0x30]`, `0x100793b0`): item tick bodies (slot `+0x60`, removal helper `0x10079270`) unread, so PRNG use and state writes are not established (O-52).
  - **Scripted disasters**: the drought-area constructor `0x10087280` (island slot `+0x15c`) is called by AnnoScene (`0x10013870`, `0x10013a1a`, handlers `0x100137d0`..`0x10013a2a`) with caller-supplied x, y and length (script disaster DROUGHT, corroborated by `docs/re/fire-and-ships.md` l.53); island files carry a DUERRE record; same family: the other script disasters (O-51).
  - Unit registry phase A (circular sector list, `0x10097f10`), unit slot `+0x74` (figure movement/jobs), destruction routine `0x10097520` (O-8, O-17).
  - Timed events: whether the enqueue `0x100adef0` is ever called with subtype 1 (payload: three `f32` at `+0x18`/`+0x1c`/`+0x20`, `0x100adf92`..`0x100adfa4`); the kind-0 sites (`0x10063cf1`, `0x100943d5`) pass 4 and the kind-1 site `0x1003593e` passes its subtype in a register (`ebx`), so its callers must be read. `TimedEvent` has no field for subtype 1; one is added if it is reachable (spec §4.10).
  - Meaning of world properties 0, 1, 2, 3, 11-13 (UI/AI side), property 6 bits 0, 4, 5, 6, 8-15, and property 5 (default true; SP toggle at AnnoFrame `0x10006fa0`) (O-29).
  - SKTIMER load (`0x1009cdd0`): does it restore the last-tick field; consumers of the countdown in scripts (O-31).
  - Building index `Add` (`0x100259c0`) free-slot hint update and `Remove` (`0x100260f0`) exact policy (O-2).
  - Player defeat processing `0x10020550` and who sets `[+0x295]`; meaning of player `+0x1ac` tables (slot `+0xc8`) and `+0x2e8/+0x308` blocks (O-22).
  - Exact saved layout of the ENTRY event records and how the kind-0 loader maps delta/value (`0x100ada30`) (O-32).
  - World `+0x69e0` dword (`0x640014` set by Reset) and the 32-dword table at `+0x6948` (zeroed in the ctor, no other use found) (O-33).
- **Commands (RE "commands")**
  - Controller / player list commands `0x80`-`0x86` and `0x90` (controller kinds 1..5 and 8/0xb: floats, u16 id lists, army/ship groups, patrol positions, computer-player input?); only dispatch and the ownership check in player slot `+0xd8` (`0x1001b090`) are read (O-42).
  - Exact semantics of `0x6c`, `0x6d`, `0x72`, `0x75` (kinds 3..11), `0x8c`, `0x8f`, `0x96`, `0x97`, `0x70` (kinds 1/2), `0x6f`; which building classes override slots `0xdc/0xe0` (`0x71`) and `0xc8` (`0x74`) (vtables `0x100c24e0`, `0x100c8848`, `0x100c9418`, `0x100c9598`) (O-42).
  - What `0x98` does in SP (city slot `+0xd0` = `0x1002c0e0`, building-at-tile fix-up), which UI action sends it, and whether it must exist in the C++ MP at all (O-43).
  - The UI veto hook `[client+0x124]/[client+0x128]` (ops 2, 6, 8, 9): who installs it, whether it is non-null when a remote turn is executed, whether a 0 return can differ between clients (potential desync source) (O-44).
  - Apply op 9 (`0x87`) in full (island slot `+0xa0`, `+0x80`, `+0x84`, city slot `+0xb0`): preview/blueprint versus editor auto-tiling; what op 2 (`0x65`, unit slot `+0xc0`) does for the object class it is aimed at (O-45).
  - City slot `+0xa0` branch details: the PRODTYPE `0x43/0x48` upgrade rule (helpers `0x1002c090/0x1002c740`), the `0x40000000` flag set by `0x100279a0`, what `[city+0x61c]` means (O-28, O-40).
  - Exact turn packet header in the client queue (5-byte header stripped before the ring), ring semantics of `0x1005b7a0` (release), the `+0xdea` recording/replay mode; whether the server stops forwarding when `Step` is paused (speed 0) (O-46).
  - Whether the factory `0x10024010` or building slot `+0x120` (called by `SetProperty 0x23`) does anything with `last` besides storing it; only the store of `(u16)now` is confirmed (O-50).
  - The 4-byte quantities in `0x6e` (float scale const `0x100893e0`, divisor `0x10089140`) and the unit-position fixed-point conversion (O-47).
  - UI call sites per command type were recovered only for placement/demolish (tCursorAbriss, tCursorAnlageDefaultEdit, tCursorFigur/StadtPos); the other mappings are inferred from the handlers.
  - `0x73` clear path (goods == 0) and the meaning of the u16 fields `@4/@6` (amount versus price/limit) in the Kontor trade-list entry (O-14).
  - The Kontor goods byte `city+12g+0x0b` is ONE signed byte, the trade-list index (read in the S6 review round: setter `0x1002dbd0`, getter `0x1002db53`, constructor `0x1002fa11`); still open: whether the value a player's UI sends to `0x7c` always equals the entry index, the range of the build-goods setting (`0x7c` mode 9, `[city+0x48c+8k]`) and how a Kontor level changes the warehouse capacity (`+0x624`) (O-14, O-57).
  - Whether the class `Load` that follows `Create` for a building of a saved game (`0x100264a0`) keeps a mine's saved deposit or searches again (O-58).
  - The derived 32-bit "service counter != 0" mask at building-index `+0x50` that the original recomputes on every counter change, and the u16 width of the counters at `+6+2b` (O-40).
  - What the type reader does with a `PROD3` record of another size than 68 bytes (branch `0x100094d2`-`0x1000972f` reads the 68-byte layout; every entry of Anlagen.dat has exactly 68), and with a `HEAD` record longer than the 56-byte buffer (`0x10009351`-`0x10009365`; none exists). The rebuild treats the first as "no production data" and the second as a cut, and refuses a ticking class without production data (state.md F25, F26).
- **How to resolve:** each item names the address to read; none blocks M3 (every one has a seam or an inert default in `docs/sim/state.md` §12).

## OQ-055: Small farm animal kind: `draw & 7` versus `draw % size` (spec D19)
- **Status:** open, owner decides
- **Area:** simulation / determinism / moddability
- **What:** the original picks the kind of a small farm's animal with `table[draw & 7]` from a table of 8 entries (AnnoWelt `0x1006f810`-`0x1006f838`, table `0x100c8d48`). The rebuild keeps the table as data and indexes it with `draw % kind_by_draw.size()`, so a modded table of any size works.
- **Why it may change the feel:** for the original table (8 entries, a power of two) the two are identical, so nothing changes in the faithful game. For a custom table whose size is not a power of two, a literal mask would leave entries unreachable or out of range; the modulus spreads the draw over all entries. Nothing in the original data can show a difference.
- **Faithful default implemented:** `draw % size()` (bit-identical to `& 7` for the 8 original entries). Always one draw per step with line 1 on and the farm not suspended.
- **Rules option:** `Rules::small_farm_kind_mask` (reserved: `Modulo` default; `Mask7` would use `draw & 7` and require a table of at least 8 entries), added only if the owner wants a literal mask for custom tables.

## OQ-056: Which timed event a cancel removes, and when the running one leaves the queue (spec D20)
- **Status:** open, owner decides
- **Area:** simulation / events
- **What:** (1) `cancel_tile` (`0x100ade40`) walks the queue in order and removes the first event whose kind and tile (two u16 words) match and whose stored subtype equals the argument, and only when the argument is 4 (`0x100ade79` `cmp ebx, 4 / jne`); with another subtype it never removes anything. No original caller uses another subtype. (2) `pop_due` (`0x100add90`) copies the payload to the stack (`0x100addb3`), saves the next node (`0x100addc4`), unlinks and frees the node (`0x100addd1`..`0x100addf5`) and only then calls the kind's handler (`0x100adae0` kind 0, `0x100adb80` kind 1; other kinds are dropped), so a handler that cancels its own tile finds nothing. Both were first tagged inferred in the S2 spec and are disassembled (the S2 review read them).
- **Why it may change the feel:** kind-0 subtype-4 events on one tile exist with delays 50 and 25 000 (hit/explosion cleanup and building destruction), so which of two equal-tile events a cancel removes (first, last, all) decides which rubble or cleanup timer survives; and a handler that wants to cancel its own tile only works if the event is still queued.
- **Faithful default implemented:** exactly the original: first match in queue order (kind, tile, subtype 4), the running event already out of the queue (`EventQueue::cancel_tile`, `pop_due`; tests pin both).
- **Rules option:** none reserved. Added only if the owner wants an alternative (cancel all matches, last match, any subtype, keep the running event queued until its handler returns).

## OQ-057: 32-bit arithmetic of the player tick: the statistics decay and the bankruptcy deadline (spec §1.1, F-time)
- **Status:** open, owner decides
- **Area:** simulation / players
- **What:** (1) every upkeep period the statistics `stat_a` and `stat_b` of a player become `u32(stat * 104) / 105` (AnnoWelt `0x1001b840`); the product is formed in 32 bits, so it wraps from `stat >= 41 297 763` (2³² / 104) and the statistic then falls to a fraction of its value (50 000 000 gives 8 619 359 instead of 49 523 809). What the two statistics measure, and whether they can reach that size, is unread (their consumers are not ported). (2) The bankruptcy deadline is `u32(t + 150000)` and the test is `t > deadline` (unsigned, `0x1001b900`): during the 150 000 ticks before tick 2³² (about 99 game days of play) the deadline wraps below `t`, so the player is declared bankrupt at the next visit, or onto 0, which is read as "not running" and set again.
- **Why it may change the feel:** (1) a wrapping statistic is a visible glitch if it is ever reached; widening the product to 64 bits would remove it but changes every statistic above the limit. (2) only after 99 game days; the same class as the 32-bit tick (F-time) that the gates keep.
- **Faithful default implemented:** both wrap exactly as the original's registers (tested: `player statistics decay by 104 over 105 in u32 once per upkeep period`, `player bankruptcy starts the grace period at the limit and ends it after`).
- **Rules option:** reserved (S4 review round, spec D21 and D22), both off by default and part of the header stream: `Rules::stat_decay_wide` (the decay product in 64 bits, capped at `0xFFFFFFFF`) and `Rules::bankrupt_deadline_wrap_safe` (the grace period over the u32 wrap: signed 32-bit difference, a deadline of exactly 2^32 stored as 1). Tests: `rules stat_decay_wide computes the decay in 64 bits and caps it`, `rules bankrupt_deadline_wrap_safe keeps the grace period across the u32 wrap`.

## OQ-058: The meaning of the player state byte (+0x28d): 1, 7 and 8 (spec §4.5)
- **Status:** open (evidence gap; no behaviour decision yet)
- **Area:** simulation / players
- **What:** the byte at player `+0x28d` is not a three-value enum in the original. The property setter (AnnoWelt `0x1001be0d..0x1001be1f`) stores any value and treats 8 as sticky (`cmp byte [edi+0x28d], 8` unless the new value is 7), the record loader (`0x1001d37a..0x1001d382`) stores the record byte and maps 7 to 8, and the player tick tests only "non-zero" (`0x1001b925`, `0x1001b927`). The rebuild names 0 (Alive), 2 (Bankrupt) and 3 (Defeated), keeps every other value as it is (read and written verbatim) and treats it as "not alive". What 1, 4 to 7 and 8 mean (scenario states such as "not yet started" or "left the game" are a guess) is unread; the scenario adapter (S14) and the setter will need it.
- **Why it may change the feel:** a scenario player in state 8 never goes bankrupt and its defeat flag stays set; the rebuild matches that, but it cannot yet say which scripted states exist.
- **Faithful default implemented:** exactly the above (tests: the open state bytes round-trip and every non-zero value skips bankruptcy and defeat).
- **Rules option:** none needed (no alternative behaviour is proposed).

## OQ-059: Constructor stores that the player model does not carry (spec §4.5)
- **Status:** open (evidence gap)
- **Area:** simulation / players
- **What:** the player constructor (AnnoWelt `0x10021600..0x10021836`) also stores `+0xa6 = 0xffff` and `+0x28c = 0x1f`, and the research object constructor (`0x10019190`) initialises `obj+0xc8` (a word, player `+0x178`) and 0x30 bytes of `0x80` at `obj+0xca..+0xfa` (player `+0x17a..+0x1aa`). None of them is in `Player` or `ResearchState`; their consumers are unread, so if they are queue or order state they are missing from the stream. The 65 initial research state bytes come from a table at `0x100e0bf6` (stride 8); it was read and is all zero, so "all entries zero" is correct. The player tick does not read any of the unmodelled fields. Also unmodelled: the player table tick (`0x1009c4d0`) brackets its AI call with two calls through `[esi+0x90]` (vtable `+0xb0`, then the object's vtable `+4` with argument 6 before and `+8` after; `0x1009c5a4..0x1009c5d5`), the same shape as the phase brackets of the world advance; read as a profiler or phase bracket with no state change, callee unread, so S8 must not lose them if they turn out to matter.
- **Why it may change the feel:** only if a consumer outside the player tick uses them (research queue, player colour mapping).
- **Faithful default implemented:** not modelled; `docs/re/players.md` has the addresses.
- **Rules option:** none.

## OQ-060: The shape of the island terrain (spec O-53)
- **Status:** open (evidence gap)
- **Area:** simulation / islands
- **What:** the rebuild's `Terrain` is three columns (`height` u8, `gfx` u16, `fertility` u8) kept as given, hashed by a digest and carried in the save frame. That shape is a stand-in, not a finding: which per-tile layers the original island object holds besides the 8-byte tile records, and how large they are, is not established (the placement check `0x1007c790` and the growth sweep read terrain, water and fertility facts; `docs/formats/insel.md` has the island file records they come from). No simulation code reads any of it yet, so nothing depends on the shape.
- **Why it may change the feel:** only once a consumer (placement S6/S10, growth S7) reads it; a wrong shape then changes which tiles accept a building or grow a crop.
- **Faithful default implemented:** the three columns as given, no required length; the scenario adapter (S14) decides what it fills them with.
- **Rules option:** none needed (no behaviour reads it).

## OQ-061: Island and city-ring members the model does not carry (spec O-54)
- **Status:** open (evidence gap)
- **Area:** simulation / islands
- **What:** the island property setter (AnnoWelt `0x10087b90`, jump table `0x10087c04`) stores key 8 at `+0xc1` (byte), key `0xb` at `+0xac` (dword), `0xe` at `+0xa6` (word), `0xf` at `+0xa8` (word), `0x10` at `+0xc3` (byte) and `0x11` at `+0xc6` (word, default 1); the reset zeroes `+0xb0..+0xbf`, `+0xc2` and `+0xc4`. The city ring also has `+0x40` (dword, the last added slot) and `+0x47` (signed byte, the highest used slot, initialised to `0xff`; `0x100967d0`, `0x100968b3`, `0x100969bb`), which are derivable from the city table. A fresh tile record is `word@0 = 0`, `byte@2 = 0`, `byte@3 = 0`, `dword@4 = 0x1e000200` (`0x1008c80d..0x1008c821`), so besides the owner nibble it carries bit `0x200` that `TileLayer::aux` (default 0) does not (spec O-11). No consumer of any of these was found.
- **Why it may change the feel:** only if a consumer outside the modelled sweeps uses them; unknown until read.
- **Faithful default implemented:** not modelled (zero or derived); kept out of the stream until a consumer is read, because every member added later is a stream version bump. The offsets are in `docs/re/islands.md`.
- **Rules option:** none.

## OQ-062: Which buildings a rect query returns (spec O-56, formerly D23)
- **Status:** closed (read in the S6 review round); only the result order stays a decision (OQ-037)
- **Area:** simulation / buildings
- **What:** the city's rect query (AnnoWelt `0x10023f50`, used for the market catchment, coverage and the drought centre) walks six y-sorted sets in a fixed list order (masks 1, 4, 8, 0x10, 0x20, 2) and calls `0x10023380` for each set. That function binary-searches the first building with `y >= rect.y - 8` (`0x100230d0`, `lea ebx, [edx - 8]`), walks while the building's y is not beyond `rect.y + rect.h - 1` and accepts a building iff `y + h > rect.y`, `x <= rect.x + rect.w - 1` and `x + w > rect.x`: the footprint overlaps the rectangle. The rebuild's `Island::rect_query` does exactly that (clipped to the island, once per building).
- **What differs from the original:** the result order (OQ-037, spec D2) and the 8-row start bound (a footprint taller than 8 rows whose top lies above `rect.y - 8` is missed by the original; the rebuild finds it, F3).
- **Faithful default implemented:** footprint overlap = the original. Test: `rect query canonical order and list masks`.
- **Rules option:** none for the membership; the place for the original's order is the reserved `Rules::spatial_order = Original0x10027000` (spec O-7, `Rules::validate` rejects it until it is implemented).

## OQ-063: What a building leaves behind on its tiles (spec O-5, D24)
- **Status:** open, owner decides (a stand-in until the original is read)
- **Area:** simulation / buildings / island tiles
- **What:** when a building is created the rebuild writes its type id into the `object` of every footprint tile, and when it is removed it writes 0 there, so whatever object the tile held before (a tree or crop of the growth sweep, an ore mark, scenery) is lost for good; after a demolition the tile is empty (object 0), not what it was before the building. The original keeps a building in its own tile record; what its `Create` and `RemoveAt` do with the tile object (island slot `+0x7c` tile clear, PRODTYPE switch `0x10029540`) is unread (O-5).
- **Why it may change the feel:** a demolished building could leave the growth sweep (stage S13) free to regrow trees on the tiles earlier than the original, or nothing at all where the original restores the old object; both are visible on the map.
- **Faithful default implemented:** object := type id on create, := 0 on remove (the stand-in; nothing else is known).
- **Rules option:** none added yet, because the original's behaviour is unknown; the reserved place is a rule that restores the previous object (it would need the object stored per footprint tile in the derived cache).

## OQ-064: The figure kind of a building without a figure id (spec O-8, O-59)
- **Status:** open, owner decides (a stand-in until the original is read)
- **Area:** simulation / buildings / figures
- **What:** when a workshop or farm step sends a figure, `consume` raises the home counter and queues a spawn whose kind is the first entry of the type's `figure_ids`. If the type has no entry (an empty list) or the entry is 0 (the file's "none"), the rebuild still raises the counter and spawns a figure of kind 0. What the original does for a building with no figure id is unread (the send path, the unit constructor and the job table, O-8).
- **Why it may change the feel:** a modded or damaged type table would make a building send an invisible kind-0 carrier that holds a counter for ever (until figures are ported, S12), and the building stalls at its `max_figures`; sending nothing would let the building keep working without a carrier.
- **Faithful default implemented:** the stand-in described above (counter raised, kind-0 figure spawned); the shipped Anlagen.dat has a figure id for every ticking class, so no original data reaches it.
- **Rules option:** none added yet. The reserved alternatives are to send nothing (no counter raised) or to reject a ticking class without a figure id at registry build time (`validate_against`); either would be a `Rules` option or a registry check, never a silent change.

---

# Decisions for the owner (rivers)

Source: `docs/render/rivers.md` §12 (rows D1..D16), the hydraulics stage S4 and the shading stage S5 (2026-10-19; OQ-071 to OQ-083 below the S4 entries cover the rows not listed before). Rule (owner): a deviation is **fix** when it is obviously unintended (removed, documented in `rivers.md`); **decision-needed** when it could change the look or feel: the default stated in `rivers.md` §12 is implemented behind an option and listed here. Every river speed is a model output ("plausible, not original"): the original data carries no discharge, no slope and no flow direction (`rivers.md` 1.2).

## OQ-065: River speed scale: reference speed, soft floor, cap, bank profile (rivers.md D1)
- **Owner look 2026-10-20:** the viewer (not the library) now defaults to `Reference::MedianCell` (`--river-speed median|fastest`), together with gain 4 (OQ-083); the owner called it "much better". Library defaults stay as above.
- **Status:** open, owner decides (visible since the shading stage S5, CPU path; GPU in S6)
- **Area:** render / rivers / flow
- **What:** the flow field is a model with tuning values `RiverConfig::out_speed` 0.9 tiles/s (0.63 m/s), `speed_floor` 0.12, `speed_cap` 3.0, `fall_speed` 2.5, bank roughness `bank_roughness` 4 and `bank_width_tiles` 0.30 (the margin runs at 0.2 of the core), bed smoothing `bed_sigma_tiles` 0.75, depth range 1.5-12 units. Measured on the 106 islands of the corpus (`river_qa --flow`): the median speed of an island is 0.12-0.48 (typically 0.21; the floor is 0.12, because most of a network is tributary where the speed grows with the area upstream), the 95th percentile 0.19-2.5 (typically 0.94), the falls 2.5, the mean 0.14-0.69.
- **What differs from rivers.md:** the Normalised discharge sets `out_speed` at the fastest cross-section that still passes half of the river's discharge (`RiverConfig::out_flux_share`), not at the sea faces: the original's mouth sprites are broad blobs (up to 15 tiles of sea face against a river 1.4 tiles wide), and measuring there made the river proper 2-3 times too fast and pinned half of all cells on the cap (first measurement, same island: median speed 2.8 tiles/s). A straight channel still has `out_speed` at its outlet.
- **Consequence of the default reference (review S4):** only the fastest cross-section that carries at least half of the discharge gets `out_speed`; everything upstream scales down with the area drained, so most of a network sits at or near the floor. Over the 106 islands the floor sets the speed of 17-90 % of the cells of an island (median island 38 %, 42 % of all wet cells; 23 islands above 50 %; `D_256x256_0045_5B`: 73.5 %, its median is exactly 0.12), the cap of at most 0.4 %. On a 20 px tile (zoom group 2) 0.12-0.33 tiles/s is 2.4-6.6 px/s (5-13 px/s at group 1): slow flow that compresses the contrast between narrows and wide shallows. Any pinch that passes half of the discharge and is at least 4 cells wide also sets the scale (rivers.md 14.2).
- **Option implemented:** `RiverConfig::reference = Reference::MedianCell` (viewer option follows with S8; `river_qa --flow --reference median [--median-speed T]`): the median cell of every river gets `median_speed` (0.4 tiles/s). Over the same 106 islands: the median island speed 0.40-0.66 (typically 0.43; 0.12-0.50 with the default), the 95th percentile 1.2-3.0 (median 1.8; 1.1), floor 15-41 % of the cells (median 20 %), cap up to 10.5 % (median 0.4 %; 7 islands above 5 %). Side-by-side contact sheets of the default and this option, same islands: `C:\Users\altin\Pictures\anno-rivers\S4\after_review` and `option_median_reference` (not in git); the S5/S6 sheets show both on the drawn river before the owner picks.
- **Seen on the drawn river (review of S5, the owner's window of `D_256x256_0045_5B` at group 1):** 55 % of the river samples are at the soft floor (speed under 0.15 tiles/s; median 0.138, p95 0.82, p99 1.34, none above 2 tiles/s), about 5 px/s at group 1, so most of the river drifts nearly still and the contrast of narrows against wide shallows lives in the 0.8-1.3 tiles/s tails. Together with the quiet default light (OQ-083: the temporal change of a river pixel is the legacy's, 1.9 levels per 250 ms) this does not read as "faster in narrows, slower in wide shallows" at the defaults. Review of S6 on the GPU frames of the same window: median screen speed 3.2 px/s, 86 % of the river pixels move under 6 px/s, so narrows and wide reaches hardly differ on screen. Not decided here: `Reference::MedianCell` (median cell 0.4 tiles/s) and `--river-gain 4` are the options to compare on the sheets (`C:\Users\altin\Pictures\anno-rivers\S5\sheets`, gain 4 at group 1).
- **Why it may change the feel:** rivers slower or faster overall; with the floor at 0.12 the slow tributaries still move.
- **Faithful default implemented:** the values above (`rivers.md` §2.4 defaults, `Reference::FastestSection`); there is no original behaviour to be faithful to.
- **Rules option:** `RiverConfig` (all fields); the viewer option follows with S8.

## OQ-066: Discharge model (rivers.md D2)
- **Status:** open, owner decides
- **Area:** render / rivers / flow
- **What:** `Normalised` (default: every river reaches `out_speed` at its reference section, so a creek and a large river look alike there and differ along their course), `Absolute` (a fixed lateral inflow `inflow_rate` 0.003 tiles/s per tile^2: long rivers are faster; the value is a first estimate that makes `D_256x256_0045_5B` reach about 0.9 at its main stem, not yet calibrated by contact sheets) and `Sourced` (inflow only at detected heads, `source_speed` 0.25 tiles/s times the cross-section; the heads are the local maxima of the distance to the outlets, so the divide of a braid also counts as a head: a limitation of the heuristic).
- **Why it may change the feel:** which rivers look fast; whether tributaries start slow or at a constant speed.
- **Faithful default implemented:** `Normalised`.
- **Rules option:** `RiverConfig::discharge`; contact sheets of the three on the same islands come with S5/S6 (rivers.md Q6).

## OQ-067: Rivers without sea contact: where they drain (rivers.md D14)
- **Status:** open, owner decides
- **Area:** render / rivers / flow
- **What:** a connected piece of river without a face to the sea (136 of the 257 rivers over the 106 islands of the corpus, 81 of 159 in the 72 island templates; `river_qa --flow` and the corpus test list every island) has no outlet. It gets a pseudo outlet at its lowest surface level, then lowest bed (cells of a river-mouth sprite win over both): the cell and its neighbours within two steps (5 cells in a channel one cell wide, up to 13 in an open reach); the direction of its flow is therefore a guess where the data has flat tiers (95 % of the pieces have the sea level height). Most of them are the pieces left over where the data itself breaks (below). The "flows down the distance to the outlets" figure of `river_qa --flow` is measured against that same guess for them, so it cannot show a wrong-way stretch; over the rivers with a sea face it is 86.5-100 % (median island 99.6 %, column "downstream sea %"). The visible risk, once S5 draws, is flow running towards the head on a third of the rivers. The list of all 257 rivers with bounding boxes and pseudo outlets (`river_qa --flow --flow-rivers FILE`): `C:\Users\altin\Pictures\anno-rivers\S4\rivers_all.md` (not in git); the pseudo outlets are magenta in the contact sheets.
- **Seen on the drawn river (review of S5):** in the owner's scene `D_256x256_0045_5B` the arrow picture of the sea-less pieces shows two streams flowing towards each other at a hidden junction (the left chain's lower stretch: the upstream part runs down to the left, the downstream part runs up towards it) and a dead-end pond that receives inflow from the bottom and turns towards its closed top; in the real frame this reads as water running into a wall. The sea-connected course elsewhere (a U-shaped meander in Tundra, an S-bend in Praerie, a hairpin in Steppe) follows one consistent line, so this is specific to the sea-less pieces. Crop of the left chain with the arrows: `C:\Users\altin\Pictures\anno-rivers\S5\owner_quiver_lower_left_chain.png` (the full window: `S5\light\gain1\owner_D0045_g1\D_256x256_0045_5B_g1_quiver.png`; the basin at the bottom right of the crop is OQ-070).
- **Seen on the GPU frames (review of S6, the same window):** at the three pseudo outlets the quiver shows arrows that point downstream upstream of the outlet and back against the course at it (a wrong-way stretch of one to two river widths), and these spots are the fastest water of the scene: 46 % of the pixels above 20 px/s sit in 8 of 44 coarse cells. The contrast of narrows against shallows that the owner asked for is therefore dominated by pseudo-outlet artefacts rather than geometry. At the default gain 1 they are hardly visible; they will show once the gain is raised (OQ-083). **The sea-less rivers of `D_256x256_0045_5B` (`river_qa --flow --flow-rivers`, tile coordinates; bounding box x, y, w, h and the pseudo outlet):** river 0, 122 cells, box 154.6, 53.5, 2.1 x 1.5, outlet 156.6, 53.8 (the cliff-ending reach near the middle left); river 4, 1652 cells, box 137.9, 111.4, 15.1 x 6.6, outlet 149.0, 115.8 (the left chain); river 5, 526 cells, box 154.0, 116.0, 5.0 x 3.6, outlet 154.9, 116.9 (bottom). **Do not raise the river gain by default before this is decided**; look at the sheets with `sea_less_speed_scale` 0 (still water) and 0.3 next to the default 1, together with OQ-083.
- **Why it may change the feel:** a stretch that flows the wrong way round (towards its head) next to a data gap.
- **Faithful default implemented:** lowest level, then lowest bed, mouths first (`RiverFlow::kPseudo` marks the cells).
- **Rules option:** `RiverConfig::sea_less_speed_scale` (1 = flow as solved, default; 0 = still water; every cell of such a river carries `RiverFlow::kSeaLess` so that a renderer can fade it). Before S5 shows the flow by default: review the list with the owner and consider showing flow only on sea-connected and fall-linked rivers until this and OQ-068 are decided. Further candidates, not implemented: join the piece to the nearest river within 1-2 tiles (see OQ-068), or choose the end nearest to a sea-connected river.

## OQ-068: Gaps in the river data (rivers.md D13)
- **Status:** open, owner decides
- **Area:** render / rivers / data
- **What:** where the painted bed of two river pieces does not touch (a few tiles apart, a cliff between them, or bed pieces at different tiers without a fall) the raster has two rivers. Fall and mouth pieces link the rivers that meet inside their footprint: a straight channel of at most `RiverConfig::bridge_tiles` 2 tiles, flagged `kBridge` (this is a **fix**, not a decision: "falls and mouths are the links of a river", rivers.md 1.2, the sprite alpha alone leaves the cliff foot disconnected; it only matters for the flow raster, the drawn water is unchanged). Gaps elsewhere are left as the data has them: `RiverMask::bridges` counts only the fall links and the rest shows up as separate rivers in the corpus table.
- **Why it may change the feel:** a river cut in two by a gap flows to two outlets; with a patch (`bridge_tiles` for every piece pair) it would flow as one river.
- **Faithful default implemented:** log and list, never patch (rivers.md D13) beyond the fall and mouth links.
- **Rules option:** `RiverConfig::bridge_tiles` (0 turns the fall links off too); a general patch is not implemented.
- **Owner look 2026-10-20 (fix + decision):** on the canyon island the visible gaps were not separate pieces but the one-corner-wide painted trench: corners at level + 1 (the original's depth test, `z + 10`) fail the stricter water test, so the water broke along touching pieces. `RiverConfig::gap_patch_tiles` (viewer `--river-gap-patch`, default 8; library default 0 = data as is) now (1) deepens every trench corner inside a piece to `level - kGapBedDepth` (5) in the water's own copy of the heights and (2) joins pieces up to that many tiles apart (Kruskal, shortest first; interpolated level, carved bed; only the 4 tier changes z80 to z32 of this island, 5.4 to 6 tiles apart, under cliffs). The owner: "looks good". Other islands still to be checked (`river_qa`).
- **Survey of all river islands 2026-10-20 (T6, docs/render/rivers.md 13.1):** 72 island templates have river pieces; patch 8 against 0 gives no regression (no island with more components, split, lost or gap pixels), gap pixels 94 -> 0 (group 0), components 363 -> 339 (group 1, 8 islands improve), but enhanced river water outside the painted bed grows from 5.8 % to 15.3 % of the original river pixels (a thin fringe at bends and a few-tile tail at open river ends, up to 19.5 % of an island's original river pixels; 0 of 444 scenes failed to load; the task text's 106 islands was not reproduced: 72 templates, or 112 islands placed in 36 of 50 scenarios). Owner decides: keep 8, a smaller value, or deepen only real gap cells. No default changed.
- **Owner decision 2026-10-20:** deepen only the real gap cells and join only facing bed pieces (no tails at open ends), as a new `RiverConfig` mode; the viewer keeps `--river-gap-patch 8` until that mode exists and matches patch 8's connectivity with patch 0's spill. Task T6c in `docs/orchestration/TASKS.md`.

## OQ-069: Ponds and lakes (rivers.md D7)
- **Status:** open, owner decides
- **Area:** render / rivers / flow
- **What:** water bodies of fewer than 12 cells (1/6 tile^2) are not solved: a static eddy of `pond_swirl` 0.03 tiles/s in a direction from smooth noise, so a pond is not frozen. Larger bodies are solved like rivers (a lake with a pseudo outlet).
- **Why it may change the feel:** tiny puddles move slightly; set 0 for still water.
- **Faithful default implemented:** swirl on, 0.03 tiles/s, threshold `pond_cells` 12.
- **Rules option:** `RiverConfig::pond_swirl`, `pond_cells`.

## OQ-070: Mouth-sprite basins in the flow raster (review S4)
- **Status:** open, owner decides (visible since the shading stage S5, CPU path; GPU in S6)
- **Area:** render / rivers / flow
- **What:** the original's river-mouth sprites are broad blobs (up to about 6 x 3 tiles; roughly 10 % of the wet cells of `N_256x256_0002_2B`), and most of that area lies behind cliffs and forest in the render, so it is never visible. The whole blob is a wet flow domain: it runs at fall speed where the surface slope calls for it (the level falls steeply over it), it counts in the discharge bands, and five islands have more than 5 % of their cells at 2.35-2.5 tiles/s only because of it. The two lobes of a mouth fan point at each other on the short sea-contact stretch.
- **Why it may change the feel:** fast, converging flow at every mouth, and the look of the mouth fan in the sea blend.
- **Faithful default implemented:** the blob is part of the domain, as drawn by the shipped enhanced water (the gap fix and the mouth look are unchanged); `transition_passes` softens the speed across it (fix).
- **Rules option:** none yet; candidates: limit the mouth-fan speed to the river speed, or drop wet cells that the depth test hides from the flow domain (raster only, nothing drawn changes). Decide with the S5/S6 sheets of `D_256x256_0045_5B` and the `Single_01` mouths in front of you.


The taste options D1b-D16 of `rivers.md` §12 that earlier stages did not list, and the decisions of stage S5 (2026-10-19). "Not built" means the feature itself comes with a later stage: its default is **off / the faithful behaviour**, so there is nothing to switch yet; the option is named here so the owner can decide before it is built.

## OQ-071: Picard refinement of the Manning form (rivers.md D1b)
- **Status:** open, owner decides (not built)
- **Area:** render / rivers / flow
- **What:** the solver uses the linear conveyance form (one SPD solve, `K ~ h^(5/3)`); the true Manning form `K = k_M h^(5/3) / (nu sqrt(|grad phi| + eps))` by Picard iteration (`RiverConfig::picard_iterations`, guarded, falls back per river) is ill-conditioned exactly where the data is flat.
- **Why it may change the feel:** slightly different speed split at braids and falls; at best a refinement of a model that has no original to compare with.
- **Faithful default implemented:** linear conveyance (`picard_iterations` 0). **Rules option:** `RiverConfig::picard_iterations`, to be built in S7.

## OQ-072: Standing waves at falls and fast reaches (rivers.md D3)
- **Status:** open, owner decides (not built, S7)
- **Area:** render / rivers / shading
- **What:** Froude-number light bands (E12, E13) where the flow is supercritical, i.e. at the foot of falls and rapids.
- **Why it may change the feel:** more visible structure at falls. **Faithful default:** off (`RiverConfig::standing_waves`, not built).

## OQ-073: Which objects are flow obstacles (rivers.md D4)
- **Status:** open, owner decides (not built, S7)
- **Area:** render / rivers / flow
- **What:** rocks and cliffs in the water would deflect the flow and shed a wake; the data has no object categories (`WALKMASK` meaning unknown), so the rule is a footprint test (<= 16 tiles, >= 50 % wet) AND a short id list (`RiverConfig::obstacle_ids`, candidates 1502-1508, 1549-1551, 1572-1577, 1580, 1581, unverified) to be reviewed on contact sheets.
- **Faithful default:** no obstacles until the list is reviewed.

## OQ-074: Whitewater at falls and rapids (rivers.md D5)
- **Status:** open, owner decides (partly built)
- **Area:** render / rivers / shading
- **What:** owner 2026-10-18: no foam on rivers. S5 keeps the legacy slope rule for the rapids (`smoothstep(5, 14, slope)`, brighter and livelier light, no foam, no white) and advects it along the flow; Froude-driven streaks and an aeration tint toward (0.62, 0.82, 0.90) come with S7.
- **Faithful default implemented:** no foam, never white. **Rules option:** the rapids layer is `LayerConfig::rapids` in `RiverConfig::layers` (amplitude, scale, period) and `RiverConfig::rapids_base`.

## OQ-075: River width and edge (rivers.md D6)
- **Status:** open, owner decides (not built; the shipped gap fix is the default)
- **Area:** render / rivers / mask
- **What:** the drawn river keeps the original's width (`kBedEdge` 0.5, see `iso.md` "River gaps"); a wider soft edge (`edge_threshold` 0.40: shallow lobes filled, rivers a little wider than the sprites) belongs to the per-piece mask of S3, which was not built (S4 reused the shipped mask).
- **Faithful default:** the original's width.

## OQ-076: After a zoom change: analytic waves, cross-fade, snap or hold (rivers.md D8)
- **Status:** decided 2026-10-21 (owner: "yes" to the cross-fade): a 250 ms cross-fade from the analytic waves into the simulated window after a group switch; built in T3 stage 3 (`TASKS.md`).
- **Area:** render / gpu / zoom
- **What:** see `docs/perf/README.md` "2026-10-19 — zoom stutter": for 0.1-0.4 s after a zoom change the coast shows the analytic waves; a 250 ms cross-fade (default of the design), a snap, or holding the previous window are the alternatives. Rivers are not affected: they never use the wave simulation window.
- **Faithful default:** analytic waves, no cross-fade yet.

## OQ-077: Missing chunks: open sea or a stand-in from the nearest built zoom group (rivers.md D9)
- **Status:** decided 2026-10-21 (owner: "confirm"): stand-ins stay on by default; open sea remains `ANNO_VIEW_STAND_INS=off`.
- **Area:** render / frame / gpu
- **What:** a chunk that is not built yet draws open sea; the design option is the nearest built group's chunk. A chunk that waits for a river flow already draws the legacy river look on the correct mask (S5), not open sea.
- **Implemented (T1):** a visible chunk that is not on the GPU yet (still being built, or waiting for its upload) draws the nearest zoom group's chunks that are, scaled by the compositor's shader to the tile size shown (`GpuLimits::stand_ins`, default on; viewer `ANNO_VIEW_STAND_INS=off`, `bench_frame --stand-ins off` for open sea). The ratio is derived from `kTile` per axis (2 between groups 0 and 1, 2.1 between 1 and 2), nearest neighbour at pixel centres, the same integer rule in `render::stand_in_source` (CPU reference, `FrameRenderer::stand_ins`, off by default so the digests stay) and in `gpu.wgsl` (`Piece`). Rivers on a stand-in use the legacy look on the stand-in's mask; sea, water objects and ships are drawn at the group shown as always. Details and limits: `docs/render/iso.md` "Stand-in chunks". Open for the owner: whether the scaled content (blurry when enlarged, aliased when shrunk) is preferred to open sea, and whether a cross-fade into the built chunk is wanted (not built). Where the nearest group lacks a source chunk, the next group fills that part (per part, `stand_in_cover`); only what no group covers is open sea.
- **Faithful default:** open sea (`stand_ins` off).

## OQ-078: Legacy shimmer or flow shading by default in Enhanced water (rivers.md D10)
- **Status:** confirmed 2026-10-21 (owner: "looks good, leave as is"): flow on by default.
- **Area:** render / rivers / viewer
- **What:** flow is the default of Enhanced water (`--river-flow flow`, key `R`); `--river-flow legacy` (also `bench_frame`) keeps the shimmer that slides along fixed axes, bit-identical to the previous renderer (test). Since stage S6 (2026-10-19) the GPU compositor, the viewer's default renderer, draws the flow as well (R key and `--river-flow legacy` switch to the old shimmer on both paths); how visible the flow is at the default strength is OQ-083.
- **Faithful default implemented:** flow on. **Rules option:** `--river-flow`, `Layers::river_flow`, `FrameRenderer::river_flow`.

## OQ-079: RiverQuality Low / Medium / High (rivers.md D11)
- **Status:** open, owner decides (built and measured in S6, `rivers.md` 16; the definitions of the tiers are OQ-086, what an adapter below the gate gets is OQ-085)
- **Area:** render / gpu / rivers
- **What:** High on a discrete adapter; on any other adapter the highest tier whose measured cost **added to the legacy shimmer** is <= 1.0 ms per 1 M river pixels (`GpuLimits::river_gate_ms_per_mpx`), else Low. Measured 2026-10-19 (synthetic 1 M pixels of pure river, group 1, shear-free = every layer active): RTX 4090 (Vulkan) adds 0.016 / 0.026 / 0.040 ms per Mpx for Low / Medium / High (legacy shimmer 0.041 in total); the Ryzen iGPU ("AMD Radeon(TM) Graphics", Vulkan) adds 2.30 / 4.03 / 6.06 on a legacy cost of 8.11, the same iGPU through DirectX 12 adds 2.05 / 3.77 / 5.82 on 9.52. So the automatic choice is High on the RTX 4090 and Low on the iGPU. Override: `GpuLimits::river_quality`, `bench_frame --river-quality`, the viewer's `ANNO_RIVER_QUALITY=auto|low|medium|high`.
- **Faithful default:** as stated.

## OQ-080: Thalweg shift in bends (rivers.md D12)
- **Status:** open, owner decides (not built, S7)
- **Area:** render / rivers / flow
- **What:** potential flow is fastest at the inner bank of a bend, the opposite of nature; the optional correction (E14, `thalweg_shift`) moves the maximum to the outer bank. **Faithful default:** off; the inner-bank maximum is documented and tested.

## OQ-081: River plume: momentum from mouths into the wave simulation (rivers.md D15)
- **Status:** open, owner decides (not built)
- **Area:** render / rivers / sea
- **What:** the river would push the sea's wave simulation at its mouth. **Faithful default:** off (render-only). The mouth blend (`kRiverBlend`, `kb`) is unchanged, as the owner likes it.

## OQ-082: Tier 1: a dynamic shallow-water window over the same lattice (rivers.md D16)
- **Status:** open, owner decides (not built, later)
- **Area:** render / rivers / flow
- **What:** a runtime Saint-Venant solver would overwrite the lattice; the contract (3.3) does not change. **Faithful default:** not built.

## OQ-083: Strength of the river light and cycle times (new, stage S5; rivers.md 2.8, 15)
- **Owner look 2026-10-20:** the viewer defaults to `--river-gain 4` (library `light_gain` stays 1); the owner called the result "much better" together with the median-cell speed (OQ-065). Further tuning is open.
- **Status:** decided 2026-10-21 (owner: "looks good, leave as is"): the default strength and cycle times stay.
- **Area:** render / rivers / shading
- **What:** the layer table is calibrated to the legacy shimmer's light variance (standard deviation 0.018, brightness +-1.8 %) for a uniform flow at full wave detail, so that the average river brightness does not change. On real rivers the glints fade with the zoom group's wave detail (by design, the legacy shimmer has no such fade) and the shear fade removes layers near the banks, so the default is **quieter than the legacy shimmer at groups 1 and 2**: the standard deviation of the river light, flow over legacy, is 0.78-0.98 at group 0 (mean 0.89), 0.51-0.59 at group 1 (0.56) and 0.42-0.50 at group 2 (0.45) on the five QA islands (corpus test "the light of the flow shading against the legacy shimmer on real rivers"); the mean change of a river pixel over 250 ms is the same for legacy and flow in the owner's window (1.88 against 1.91 levels, 2.28 at gain 4). The motion is there (features move with the solved velocity: Q5 flat stems median angle error 4-9 degrees, speed ratio 0.90-1.07; all windows: the prediction beats standing still in 83 %, 63 % in the owner's canyon where the shear fade has taken most of the light) but at the default the flow is hardly visible as flow (below), and at groups 1 and 2 rivers are calmer than before. **Since S6 the viewer's GPU renderer draws the same light; that the viewer draws the flow does not mean the owner sees it: the headline goal "visibly flowing rivers" is not met at the defaults.** Review of S6, GPU frames of the owner's window (`D_256x256_0045_5B`, group 1): S6 changes at most 9-14 levels of 255 per river pixel and before/after at 1:1 are indistinguishable; the flow light has a standard deviation of 1.09 levels (p99 3.1, max 5.5); the non-advected in-place animation of the sea pattern changes 3.7 levels rms per 250 ms, 5.6 times the temporal change of the flow light (0.66); the median screen speed is 3.2 px/s and 86 % of the river pixels move under 6 px/s; a feature drifts u*T = 0.22 tile (about 9 px) per phase cycle against a noise feature of about 0.87 tile (36 px), so the two-phase crossfade reads as boiling in place rather than as drift; even at gain 4 the structure is faint. Block matching on GPU frames over 250 ms: 81 % of 21 windows within 45 degrees of the solved direction, median angle error 22 degrees, speed ratio 0.54, 52 % beating standing still (over 300 ms: ratio 0.25, 63 % within 45 degrees). At group 2 the light sd is 0.85-0.9 levels, which posterises to +-1 level. **Candidates for the owner, none changes the default (nothing is built except the gain):** default gain 3-4 (but see OQ-067: do not raise it before the pseudo outlets are decided); `Reference::MedianCell` or a lower speed floor so that narrows and wide reaches differ on screen (OQ-065); damping the static sea-sprite animation inside river pixels; longer layer periods or a faster flow-to-feature ratio (more drift per cycle); for group 2 a larger amplitude. Contact sheets and sequences: `C:\Users\altin\Pictures\anno-rivers\S6` (`frames\` gain 1 and 4; `decision\` gain 1 / 3 / 4 side by side). `RiverConfig::light_gain` (`--river-gain x`, `river_qa --shade-gain x`) multiplies the ripple layers (not the rapids layer): 1 (default, calibrated), 4 shows the structure clearly (sheets show both). Candidates, not implemented: calibrate the amplitude per zoom group on real river pixels, or let it compensate the detail fade; a lattice stride of 2 px at group 2, whose 0.2 x 0.4 tile texels are as wide as the banks of a river one tile wide (the slow layers then fade across the whole river). The cycle times 2400 / 1600 / 800 / 640 ms are the design's (they divide the hour). The design's stretch of the layers along the flow is not built (it aliased, rivers.md 15.7).
- **Why it may change the feel:** how visible the flow is against how much the river's look changes from the original/legacy, and how calm distant rivers are.
- **Faithful default implemented:** `light_gain` 1 (calibrated table, no compensation of the fades). **Rules option:** `RiverConfig::light_gain`, `layers`, `lattice_stride`.

## OQ-084: Flow lattices on the GPU path before the GPU port (new, review of S5)
- **Status:** resolved in S6 (2026-10-19): the GPU compositor reads the lattice, so the bake is no longer waste; `GpuFrame` uploads a chunk again when it was rebuilt with its flow (`Chunk::serial`, `rivers.md` 16.2).
- **Area:** render / rivers / viewer
- **What:** until S6 the viewer's GPU renderer did not read the flow lattice, but the flow builders ran and every river chunk baked a lattice (+0.4-1.1 % of the chunk time, 100-180 ms of low-priority CPU at start, 13-19 MB for 3-4 river islands); a chunk built before its island's flow was built twice. Both ends are closed. The option "skip the bake while the GPU compositor is active" is moot and not built.

## OQ-085: What an adapter gets that cannot afford even RiverQuality::Low (new, stage S6; rivers.md D11, 16)
- **Status:** open, owner decides
- **Area:** render / gpu / rivers
- **What:** the gate of the automatic tier (OQ-079) is 1.0 ms per million river pixels added to the legacy shimmer. On the Ryzen iGPU even Low adds 2.3 ms per Mpx (2.05 through DirectX 12), so the automatic choice there is Low, the floor, which is above the gate. The whole sea pass of that iGPU is 7.3 ms at 2494 x 1355 (legacy look, group 1, the owner's canyon; 3.4 Mpx, 281,000 of them river), so the iGPU misses a 2 ms frame with or without the flow shading; the flow adds 0.9 / 1.5 / 2.1 ms (Low / Medium / High) in that view at group 1 and 1.3 / 2.2 / 3.3 ms at group 0 (`bench_frame --river-pass`). River pixels are 2-8 % of the views measured. Options: Low on such an adapter (default, implemented: the owner gets the flow everywhere), or a fourth outcome, the legacy shimmer (`RiverFlowMode::Legacy`) when even Low is over the gate. Not built: a mode that falls back by itself.
- **Why it may change the feel:** the rivers' look on weak GPUs (flow against the old sliding shimmer).
- **Faithful default implemented:** Low on an adapter that cannot afford it (the flow stays on); the legacy look stays one key away (`R`).

## OQ-086: What Medium and Low mean (new, stage S6; rivers.md D11, 16)
- **Status:** open, owner decides
- **Area:** render / gpu / rivers
- **What:** rivers.md 4 said "Low drops the light taps, Medium drops H (the whitewater streaks)". The streaks only exist where the surface falls steeply (falls and rapids), so dropping them saves nothing on an ordinary reach. Built: **Medium** drops every layer that fades with the zoom group's wave detail (the glints and the whitewater streaks: 2 of the 4 default layers; at group 2 they have no amplitude anyway, so Medium equals High there); **Low** is Medium with one noise tap per phase instead of two (the light is the noise scaled by `kLowTapGain` 1.8 and the layer scale instead of the noise's slope: the same standard deviation, measured within 3 % (0.97 of Medium's) and asserted within 12 % by the test, but a softer look without the emboss relief). Measured cost per Mpx on the RTX 4090 / the iGPU (Vulkan): High 0.040 / 6.06 ms above the legacy look, Medium 0.026 / 4.03, Low 0.016 / 2.30.
- **Why it may change the feel:** what a weak GPU shows (no glints, no relief), not what a strong one shows (High is the reference look and bit-identical to the CPU path within 2/255).
- **Faithful default implemented:** as built. **Rules option:** `RiverQuality`, `RiverParams::quality`, `kLowTapGain`.

## OQ-087: The CPU path rounds down, the GPU rounds to nearest (new, stage S6; rivers.md 16.3)
- **Status:** open, owner decides
- **Area:** render / rivers / parity
- **What:** `rivers.md` Q2 asks for river pixels within 2/255 of the CPU path on 99.9 %. The flow shading itself meets it exactly: GPU minus legacy against CPU minus legacy is within 2/255 on 100 % of the pure river pixels of all five QA islands, zoom groups, three times each (largest difference 2), and the synthetic shader test agrees within 1. The **whole** pixel agrees on roughly 99.1-99.97 % (99.44-99.97 % in the windows of the corpus test, 99.146 % on the Steppe island at group 1 in the review's own windows; largest difference 3) because the CPU path truncates after every step of the sea (`sea::unpremultiply`/`over`, `lerp4`, `shade_apply`'s `sat_cast`) where the GPU keeps floats and rounds once: a level or two on every sea pixel, the legacy river look and open water included (legacy mode of the same views: 99.28 %). Changing the CPU path to round changes every enhanced digest, and the original water if `lerp4` or `over` are touched, so S6 does not.
- **Why it may change the feel:** nothing visible (1-3 levels of 255); it is a tolerance question and a digest re-record.
- **Faithful default implemented:** unchanged; tests assert the flow shading alone at 99.9 % / max 4 and the whole pixel at 99 % / max 4 (measured 99.4-99.97 %, max 3); the statistic leaves out the river pixels with an object over them (38 % of the owner's canyon at group 1: 107,988 of 281,215), where the GPU sinks objects standing in water and the CPU path does not: the flow shading alone is compared there too (97.6-100 % within 2/255, largest difference 11; asserted at 97 % / 12), the whole pixel is not. **Alternative:** round in the enhanced paths of `shade_sea_flow` only (enhanced digests re-recorded once, original water untouched).

## OQ-088: Which GPU the viewer renders on (new, stage S6; rivers.md D11)
- **Status:** open, owner decides
- **Area:** viewer / gpu
- **What:** the viewer asks for the high-performance adapter compatible with its window (the RTX 4090), which presents across to the monitor on the iGPU; the river tier is then High (OQ-079). `ANNO_GPU_ADAPTER` (a number of `bench_frame --list-adapters`, or a part of the name) selects another adapter for the headless tools and tests only, because presentation support of a chosen adapter to the window is not checked. This machine lists: 0 RTX 4090 (Vulkan), 1 AMD Radeon(TM) Graphics (Vulkan, integrated), 2 RTX 4090 (DirectX 12), 3 the iGPU (DirectX 12), 4 Microsoft Basic Render Driver (software), 5 RTX 4090 (OpenGL).
- **Correction 2026-10-21 (owner: the 500 Hz monitor is physically connected to the RTX 4090; checked with `EnumDisplayDevices`):** both monitors are driven by the RTX 4090: `\\.\DISPLAY2` 2560x1440 @ 500 Hz, primary, at (0, 0), and `\\.\DISPLAY1` 1920x1080 @ 144 Hz at (-1920, 0); the Radeon iGPU drives no monitor. The earlier note came from `Win32_VideoController`, whose `CurrentRefreshRate` and resolution put the 500 Hz mode on the AMD entry: a WMI quirk, not the topology. So the viewer on the RTX 4090 presents to its own monitor: there is no cross-GPU copy, and rendering on the iGPU would be the case that copies.
- **Why it may change the feel:** frame pacing on the 500 Hz display, power use.
- **Faithful default implemented:** the high-performance adapter, as before S6. **Alternative:** a viewer option to render on the integrated GPU that drives the monitor (and the tier gate then picks Low).

## OQ-089: Run the GPU parity tests in CI (new, review of S6)
- **Status:** closed (T13, 2026-10-22; second executor)
- **Area:** ci / render / tests
- **What:** the GPU parity tests of `gpu_test.cpp` already ran on Linux CI before T13 (main run 39758843314, linux-gcc ran 79 GPU cases; the other cpp jobs run them too, on adapters not named in the logs), but only on whatever adapter each runner happened to have. What T13 changes is the `linux-gcc` job: it is pinned to Mesa's software Vulkan driver (lavapipe) so the tests run on a deterministic software adapter instead of skipping or depending on the runner's hardware, and `ANNO_REQUIRE_GPU=1` is set so a missing adapter fails the job instead of skipping. The other cpp jobs keep their existing warning-on-skip behaviour.
- **Answer:** CI installs Mesa's software Vulkan driver (lavapipe) in the `linux-gcc` job only (`mesa-vulkan-drivers`, `libvulkan1`, `vulkan-tools` for a `vulkaninfo --summary` line in the log), sets `WGPU_BACKEND=vulkan` and `VK_ICD_FILENAMES` to the lvp ICD (`/usr/share/vulkan/icd.d/lvp_icd.json`) so wgpu-native picks lavapipe, and sets `ANNO_REQUIRE_GPU=1` for that job's test step so a missing adapter fails instead of skipping. The "GPU tests need an adapter" step now fails on `linux-gcc` and keeps warning on the other jobs. The `macos-clang` job already runs the tests on Metal; its flaky automatic-tier test was made deterministic: the automatic choice runs with no wall-clock budget (`river_measure_budget_ms = 0`, unlimited) so it cannot run out after the baseline, and the tier is asserted as `choose_river_quality(frame's own river_costs(), gate)` plus `costs.size() >= 1` baseline-first. A new `choose_river_quality` unit test pins the rule on hand-made cost tables (gate -1 → Low, huge gate → High, mixed, empty, cut after baseline, Medium expected). No tolerance was loosened; a lavapipe parity difference is reported as BLOCKED/QUESTION, not renderer-fixed. Test time added: test step 75 s → 135 s, GPU case sum 62 s → 117 s, lavapipe install about 21 s (run 39753758943). See `.github/workflows/ci.yml`, `src/render/tests/gpu_test.cpp`, and `docs/testing/gpu-tests.md`.
- **Why it may change the feel:** nothing visible; build time and CI complexity (one extra package install on `linux-gcc`; the GPU parity tests now run there deterministically).
- **Faithful default implemented:** as above; the soft-adapter running note is in `docs/testing/gpu-tests.md`.
- **Follow-up (2026-10-23, orchestrator, owner's go):** the `windows-msvc` job no longer runs the GPU cases (`ctest -LE gpu`; the executable carries the label `gpu`). On the hosted Windows runner wgpu-native picks Microsoft's WARP software rasterizer, so the cases did not skip but ran one at a time at about 50 s each, 43 of the job's 54 minutes. linux-gcc on lavapipe stays the deterministic GPU parity job; the MSVC build of the GPU code is still checked; the owner's 4090 runs the cases locally. Details in `docs/testing/gpu-tests.md` "Test time".

## OQ-090: Flow lattice textures per river chunk and the plane pool (new, review of S6)
- **Status:** open, owner decides
- **Area:** render / gpu / perf
- **What:** a river chunk now creates 8 textures instead of 5 on the window thread when its planes are not pooled, and the plane pool grew from 120 to 192 planes (`kPoolPlanes` 8 x 24, up to about 72 MB more of pooled 1 MB planes). A/B runs (baseline build against the working tree, 2560 x 1369, `--zoom 6`, 9 pairs): p50 0.3 to 0.4 ms every time, p95 and p99 equal, max outliers of 8-20 ms in both builds (one 16.7 ms `create` for 2 chunks at a group change, the baseline has the same class of stall at 17.3 ms, so it cannot be attributed to S6). Whether to trade pool memory and pre-created lattice textures for fewer stalls is a tuning call.
- **Why it may change the feel:** nothing visible; occasional frame-time outliers at zoom changes versus memory.
- **Faithful default implemented:** as built (documented, within the budget). **Options:** pre-create a few flow lattice planes at startup, or lower the cap; re-measure with 20 or more repetitions first.

## OQ-091: Rivers on a stand-in: legacy look, scaled flow, or cross-fade (new, T1b step 3)
- **Status:** decided 2026-10-21 (owner: "leave"): legacy look on stand-ins stays.
- **Area:** render / gpu / frame / rivers
- **What:** a stand-in chunk draws its river with the legacy shimmer and flips to flow shading when the real chunk lands. Measured (`docs/perf/README.md`, T1b step 3) as small at the default river gain: the river pixels move by 2.5-8.6 summed over channels for 2-37 frames. Options: leave (default), sample the source chunk's flow lattice scaled (GPU one-line change in `sea_fs`, but the CPU reference `stand_in_chunk` then needs a scaled lattice too), or cross-fade the swap (needs a per-chunk arrival time).
- **Faithful default:** legacy look on stand-ins.

## OQ-092: What blocky stand-ins should look like (new, T1b step 3)
- **Status:** closed 2026-10-21 (owner: "seems resolved now, haven't seen any anymore in my last test", with the T1e dissolve and the continuous zoom of T3). Reopen if T11 changes the picture.
- **Area:** render / gpu
- **What:** stand-ins enlarge the nearest built group by nearest neighbour, so for 25-70 ms after a zoom into an unbuilt group (first visit, 2560x1440) they are blocky next to sharp built chunks. Options: keep (default), bilinear sampling of the source, or cross-fade into the built chunk (T2/T3 smoothing steps). Open sea instead of a stand-in is `ANNO_VIEW_STAND_INS=off`.
- **Faithful default:** nearest neighbour.

## OQ-093: Pre-created chunk planes: how many, and from what (new, T1b finalize)
- **Status:** decided 2026-10-21 (owner agrees): derive the pre-created planes from the view (visible chunks of the largest shown group times planes per chunk), bounded by a VRAM budget in `GpuLimits`; built in T3 stage 3 as its own commit.
- **Area:** render / gpu / perf
- **What:** `GpuLimits::prewarm_chunks` is 24 in `GpuLimits::interactive()` (about 96 MB of 512 x 512 textures kept ready by a background thread, tuned on a 2560 x 1369 view at one group). A larger view, a 4K window or a zoom range wider than three groups needs more; the shortfall falls back to texture creation on the window thread (1-13 ms each), as before. Nothing derives the number from the window size yet. Options: derive it from the view (visible chunks of the largest group x planes per chunk, bounded by a VRAM budget), or leave the fixed default and expose it in the viewer config.
- **Faithful default:** 24, a `GpuLimits` field, 0 outside the interactive limits.

## OQ-094: Frame hitches that are in the display path, not in the frame (new, T1b finalize)
- **Status:** open, needs the owner's machine
- **Area:** app / render / perf
- **What:** the owner's 26-39 ms busy frames did not reproduce in the development runs of this branch, and every step of `GpuFrame::render` is now named, so the window thread's own work is covered. What `bench_frame` cannot see is the surface acquire, SDL's present and the display itself (it has no window). The memory notes say the 500 Hz monitor is attached to the AMD iGPU while rendering runs on the RTX 4090, so every presented frame is copied across GPUs by the driver, a path that can stall for tens of milliseconds without any work in our code. The `frame hitch:` line separates it: "waiting for the display" (the acquire) against "busy" and the steps. If a hitch shows as a long wait for the display and a small busy time, the cause is outside the frame. Isolation for the owner: run the viewer once with the monitor on the 4090 (or the iGPU output unused) and once as now, and compare the hitch lines.
- **Two corrections (T1c).** (1) "busy" includes the `present` step (`vkQueuePresentKHR` / `wgpuSurfacePresent`). On NVIDIA Vulkan with the window composited on the iGPU, the copy and vsync wait can land inside that call instead of in the surface acquire, so a display stall can show as a large `present` step inside busy. A large `present` with small other steps is the display path, not our work. (2) The hitch line's own console write (and the periodic report's) happens on the window thread after the frame it describes, so its cost lands in the end-to-start gap of the next frame, not in any step. Since T1c nothing is printed per frame for received chunks (before, `zoom group N: k chunk(s) received` was written once per frame after present, outside every measured step, and is the reviewer's best explanation of the 26-39 ms frames: a synchronous console write on a loaded machine); the count is in the periodic report.
- **Correction 2026-10-21 (owner: the 500 Hz monitor is physically connected to the RTX 4090; checked with `EnumDisplayDevices`):** both monitors are driven by the RTX 4090: `\\.\DISPLAY2` 2560x1440 @ 500 Hz, primary, at (0, 0), and `\\.\DISPLAY1` 1920x1080 @ 144 Hz at (-1920, 0); the Radeon iGPU drives no monitor. The earlier note came from `Win32_VideoController`, whose `CurrentRefreshRate` and resolution put the 500 Hz mode on the AMD entry: a WMI quirk, not the topology. The cross-GPU copy this question supposed does not exist on this machine, so its isolation run is moot. Hitches with a long display wait and a small busy time are then the 4090's own present and vsync path (or the DWM with two monitors at 500 and 144 Hz): the next check is the viewer fullscreen or on the 500 Hz monitor alone, against the hitch lines as now.
- **Faithful default:** nothing changes in the code.

## OQ-095: Continuous zoom: default mode, speed and the reset key (new, owner proposal 2026-10-20)
- **Status:** items 1-3 decided 2026-10-21 (owner: "I agree with your recommendation"): continuous is the default, 1.1 per notch, 120 ms, `Home` / `0` back to group 1; stepped stays as `--zoom-mode stepped`. Item 5 decided 2026-10-21 (owner, on the T3 stage 3 fix 1 build, after comparing area, bilinear and the old nearest look: "bilinear works very well"): **bilinear** magnifies wherever the picture is enlarged (above group 0's tile width, or a finer level bridged while it loads); below scale 1 the picture is composed on the finer level and reduced by box (fix 1), so nearest is gone from the default path; `ANNO_VIEW_RESAMPLE` keeps the A/B. Item 4 open (minimum zoom; as built: the whole scene fits the window, `--min-zoom` overrides).
- **Area:** app / render (look and feel; no sim state)
- **What:** the owner proposed replacing the three instant zoom steps by a continuous zoom "with a lower, smoother zooming haptic and speed, with a key to return to default zoom level". The original game has three fixed zoom levels (`kTile` 84/42/20, keys and wheel step between them). Task T3 in `docs/orchestration/TASKS.md` implements a continuous scale drawn from the nearest built group, eased wheel zoom, a zoom-out range beyond group 2, and keeps a stepped mode.
- **Owner decides:** (1) default mode of the viewer: continuous or stepped (the original); (2) wheel factor per notch (T3 default 1.1) and easing time (default 120 ms); (3) the reset key (`Home` or `0`) and what "default zoom" is (group 1 scale proposed); (4) minimum zoom-out (whole scene fits the window proposed); (5) bilinear or nearest for scales above 1 (PNG pairs from T3). The owner picks after the first T3 build.
- **Owner look 2026-10-21 (T3 stages 1-2 build, canyon island, 4090):** "zoom feels very smooth. no artifacts at all now while zooming and moving." As built: wheel notch 1.1, 95 % of the way in 120 ms, `Home` / `0` back to group 1's width, groups switch at the geometric middle with 5 % hysteresis, nearest above scale 1 and box below, range half a group beyond groups 0 and 2 (more zoom-out is stage 3).
- **Orchestrator recommendation:** make `continuous` the viewer default with the values as built (1.1 / 120 ms / `Home` and `0` / group 1 as the default zoom), keep `--zoom-mode stepped` as the faithful option, decide item 5 (nearest vs bilinear above 1) on the PNG pairs stage 3 produces, and item 4 (minimum zoom) with stage 3's design note.
- **Faithful default:** the stepped mode reproduces the original's three levels; the faithful core (sim) is untouched either way.

## OQ-097: Recentred wave-simulation strips are uploaded in one step on the window thread (new, T3 follow-up)
- **Status:** open, a small change with a state question; the executor's measure is in docs/perf/README.md (2026-10-21, T3 follow-up)
- **Area:** render (enhanced water, GPU) / perf
- **What:** when the wave window moves (a pan, a zoom step inside a group), the cells that came into view are written by `FluidGpu::apply_job` in one call for all pieces: 1.3-1.9 ms on the window thread on the RTX 4090 at 2560 x 1369 (the owner's log: `fluid` 1.6-1.9 ms every few frames, 3.9 ms once). The whole-grid upload, the first window of a group, is already spread over frames (`upload_budget_ms`). Spreading the recentred strips the same way is a change of what the simulation sees: until every strip is written, the window's grid mixes cells of the old and the new window coordinates for a few frames at its edge.
- **Options:** (1) keep the single step and accept 1.3-1.9 ms in those frames (today); (2) spread the strips over frames and hold the window's origin at the old one until the last strip is in, so the sim is coherent throughout (the moving window then lags by the upload time); (3) write the strips from the job thread into a staging texture and copy on the window thread (a copy is cheap, the write is not). Option 3 has the least visible effect; it needs a staging buffer per strip size.
- **Owner decides:** nothing visible changes with (2) or (3) except the timing of the wave's first appearance at a new edge (about one frame). If it is wanted, the executor implements it as T14 item (iGPU compositor cost) or a separate small task.
- **Faithful default:** no sim change: the simulation is the enhanced water's, not the original's.

## OQ-098: A resize reconfigures the surface on the window thread (new, T3 follow-up)
- **Status:** open, measured; the fix is not small
- **Area:** app / render (resize)
- **What:** on a resize the viewer calls `wgpuSurfaceConfigure`, which rebuilds the swapchain in the driver: 9.5 ms on the RTX 4090 at 2560 x 1369 (`surface configure` in the frame log, the owner's resize frame of 8.35-11 ms). The composited targets are not part of that step (the `under` step of the resize frame is small, the grid's targets are made in `render`, and the offscreen targets have room for the larger window: `fits`). The planes for the larger window are pre-created (OQ-093), so the resize frame is no longer a plane upload.
- **Options:** (1) keep it: one frame of 10 ms per resize; (2) debounce the reconfigure while the window size changes (SDL resize events arrive in bursts): frames keep the old swapchain, drawn into a scaled picture, until the size is stable (about 100 ms); (3) configure from a background thread, if the driver allows it (wgpu-native does not promise it for a surface in use).
- **Owner decides:** (2) changes what the window shows during a live resize for about 100 ms (a scaled picture, not a blank); the owner's choice.
- **Faithful default:** no sim or look change.

## OQ-099: Offscreen targets at 4K in the zoomed-out view, and their return (new, T3 follow-up)
- **Status:** open, the owner decides on the memory budget
- **Area:** render (GPU memory; the iGPU shares system memory)
- **What:** the compositor's offscreen targets are made for the largest composed grid they have seen (2.77 x the window per axis, see iso.md "Derived levels in use"): measured 285 MB at 2560 x 1440 (`offscreen_bytes()`), computed about 640 MB at 3840 x 2160, when zoomed all the way out. **Measured 2026-10-21 (T3 follow-up):** re-growing those targets (the first zoom-out frame after a shrink, 2560 x 1440) costs **0.02 ms on the RTX 4090 and about 14 ms on the Radeon iGPU** (the iGPU allocates ~280 MB of shared-memory textures). 14 ms is over the 2 ms a frame can spare, so **`offscreen_shrink_ms` now defaults to 0 (never shrink):** the targets are kept at their largest. The `scene` grid is also never freed outright; if shrinking is enabled it floors at the window size (+25 %).
- **Options:** (1) keep the targets at their largest (today's default, 0): up to 285 MB at 2560 x 1440 held on the iGPU's shared memory; (2) a memory budget in `GpuLimits` (as OQ-096 does for the sprite atlas): above it the targets are made for the view and the grid is drawn at the level above; (3) enable `offscreen_shrink_ms` on the discrete adapter only (0.02 ms there), off on the iGPU; (4) cut the 2.77 x to the grid's real size (the +25 % of `ensure_under` is for a zoom that keeps widening).
- **Owner decides:** the budget (a number for 256 MB on an integrated adapter and 1 GiB on a discrete one, as the prewarm budget already uses) and whether the grid may be drawn one level up when the budget is full; or enabling the shrink per adapter class.
- **Faithful default:** no look change while the budget holds.

## OQ-096: GPU-built chunks: the sprite atlas budget (new, T11 stage 0, C3)
- **Status:** open, owner decides (a limit default)
- **Area:** render (GPU memory)
- **What:** T11 keeps sprite frames resident on the GPU per scene (`docs/render/gpu-chunks.md` section 4). WebGPU reports no memory size, so the budget is a config value. Measured: every original scenario's frames for all three groups fit in 197 MiB (330 MiB estimated with enhanced water's smoothed copies); one chunk needs at most 7.8 MiB.
- **Proposed default:** `GpuLimits::atlas_budget_mb` 512 on a discrete adapter, 256 on an integrated one, clamped to the device's texture array and buffer limits; above the budget the least recently used atlas layer is emptied, and a chunk whose frames cannot fit at all is built on the CPU and counted.
- **Owner decides:** accept 512 / 256, or another rule (for example a share of the adapter's reported VRAM where a backend exposes it). Decide after the stage 2a owner check, when the measured resident MiB are in the perf README.
- **Faithful default:** no look-and-feel stake; the original keeps all sprites in system memory.

## OQ-100: Up to 8 players: how the world grows above 4 players (new, owner decision 2026-10-22, PLAN §0.6 / M4.1)
- **Status:** **decided by the owner 2026-10-22** (all items: 1 as answered, 2-6 as recommended, 7 with a stricter floor of 99 %). Implementation: PLAN M4.1, spec `docs/sim/world-scaling.md` (to be written).
- **Area:** sim (world setup), net (handshake), UI (game setup)
- **Evidence:**
  - The original has no world generator: an endless world is a scenario with a fixed size and fixed island links, and only the templates, raw materials, ores and treasures are drawn at start (`docs/re/annoscene.md` §3, §5).
  - Its nine endless worlds (`Scenes/0010SingleEndlos`) are 1024 × 1024 tiles (seven of them) or 800 × 800 (two), with 16–28 links covering 19–46 % of the area. They hold one human and 2–4 computer players, plus Venetians, pirates and the neutral player 31.
  - The multiplayer scenario folders are empty in this build (multiplayer never shipped, ISS-030, ISS-031; 2–8 players were planned, ISS-034).
  - `Single_24.sze` has eight parties (one human, seven computer players) on a 512 × 512 map.
  - Read on 2026-10-22 with `tools/sze.py` (header `HEAD` world size, `INSELLNK` links, `SPIELER/HEAD` player types).
- **Questions, each with the recommendation:**
  1. **Who counts as a player:** **DECIDED by the owner 2026-10-22: every competing party, human or computer** (not Venetians, pirates, natives or the neutral player). Note: the original's endless worlds already hold up to 5 competing parties (1 human + 4 computer) at 1024 tiles without growing; the owner's baseline of 4 is stricter, which is fine (more room per player).
  2. **World area (DECIDED as recommended):** grows in proportion to the player count above 4: area(n) = area(template) × n / 4, so the edge grows by √(n / 4). For the 1024 template that is about 1145, 1254, 1355 and 1448 tiles for 5-8 players, and 800 → 894 … 1131. *Recommended*, because it keeps the land per player of the 4-player world.
  3. **Island links (DECIDED as recommended):** added in proportion too (links(n) = links(template) × n / 4, rounded up), keeping the template's climate bands and its size mix (64, 80, 128 and 256 tiles). The original endless worlds lay the climates out north to south: the median island centre lies at 0.08 of the world height for polar, 0.21 tundra, 0.35 nord, 0.49 steppe, 0.52 prairie and 0.77 jungle. There is no desert link in them. Measured over their 229 links on 2026-10-22. *Recommended.* Alternative: keep the island count and only enlarge the gaps (not fair: same land, more players).
  4. **Layout (DECIDED: extend the template):** extend the template (keep the original links where they are, add bands of new links and new start regions around them) or generate a whole new layout. *Recommended: extend*, so a 5-player world still looks like the original world it came from, and the faithful 4-player world is literally its core.
  5. **Raw materials, ores, treasures (DECIDED as recommended)** (`ROHSTOFF`, `ERZE`, `SCHATZ`): scale with the number of links (the original distributes them per link). *Recommended.*
  6. **Natives, pirates, Venetians (DECIDED as recommended):** natives scale with islands (they live on islands); one pirate and one Venetian party stay as in the template. *Recommended*; the owner may want more pirates in a big world.
  7. **Fairness tolerance: DECIDED by the owner 2026-10-22: every player gets at least 99 % of the 4-player share** of land, resources and room. This applies to each measure separately: land by climate and size class, reachable raw materials and ores, and distance to the nearest rival and to free islands. My recommendation was 90 %.
     - The 4-player share is the template world's total divided by 4.
     - For whole-number measures (kinds of raw material or ore within reach), 99 % means not one fewer than the baseline.
     - The generator therefore needs a deterministic, bounded search: seeded retries with a stated limit. If no layout meets the floor, world setup fails with a message naming the measure that fell short; it never quietly hands out an unfair world.
     - The spec measures how far the original worlds themselves are from even (they are not symmetric), so the baseline is defined on the template, not on one player's corner of it.
- **Faithful default:** up to 4 players nothing changes. The scaling is a rule (PLAN §0.2) whose parameters are configuration; authored missions keep their own setup.

## OQ-101: What turns chapels into churches and schools into universities (new, tutorial series 2026-10-22)
- **Status:** open, RE
- **Area:** sim (buildings, needs)
- **Evidence:**
  - `Anlagen.dat` (P) has two chapel types and two school types, next to the big church 1325 and university 1379:
    - chapel 1320 (`WARE KAPELLE`, `INFRA_STUFE_1C`) and 1321 (same footprint and cost, `WARE KIRCHE`, `2F`);
    - school 1346 (`SCHULE`, `2B`) and 1347 (`HOCHSCHULE`, `3C`).
  - The community series (`docs/research/tutorial-videos.md` §3.2):
    - the chapels became churches the moment a church was built (V7 06:16);
    - the school took the university's look and function the moment the university was built, although the city had passed `3C` long before (V13 03:10);
    - later, a newly placed chapel served aristocrats as a church (V14 12:46).
- **Questions:**
  1. Is the swap 1320 → 1321 / 1346 → 1347 triggered by the stage, by the first 1325 / 1379 built, or by both?
  2. Does it apply to every chapel or school of the city, the island or the player?
  3. Does the chapel slot build 1321 directly once the trigger has happened?
  4. Where in `AnnoWelt` is it done? (A pass like the house upgrade, `population.md` §5?)
- **Faithful default:** whatever the original does; needed before public buildings enter the sim.
- **Update 2026-10-22 (community FAQ):** forum posters report that one university or church on the **island** upgrades every school or chapel there, whatever their coverage, and that demolishing it loses the upgrade; the cathedral counts as a church (`docs/research/community-faq.md` §4). That answers questions 1–2 as community evidence: the trigger is the big building, with island scope. Still to confirm in the code or by running the original.
- **Update 2026-10-22 (manual and in-game help):** both shipped documents say **service area**, not island: chapels in a church's service area become stone chapels that count as churches [H/Kapelle, H/Kirche], and schools in a university's service area can research everything [H/Forschung, M p. 50]. That contradicts the FAQ's island scope. T15 test: one chapel inside and one outside a new church's area. RE: the place-side-effect hooks (O-4) and the upgrade rewrite (O-28) (`manual-and-help.md` §2, §5).

## OQ-102: How houses get market-stand goods and infrastructure (new, tutorial series 2026-10-22)
- **Status:** open, RE (extends the `population.md` "Open" items on the market class and on infrastructure delivery)
- **Area:** sim (supply)
- **Evidence:**
  - Market stands (PRODTYPE group 5, e.g. 1310 food and salt, 1314 cloth and leather) have action radius 4 (P).
  - The series:
    - residents walk to a stand and buy there (V2 01:45);
    - a stand with exits on all sides serves residents from all sides (V2 01:36);
    - the innkeeper walks to the market hall to fetch alcohol, so a tavern should stand next to one (V2 05:41, V11 05:15);
    - new districts need their own stands (V9 07:42);
    - a citizen house outside the church's drawn circle still had its church need met (V10 03:52).
- **Questions:**
  1. Is the house's walker task `0x35` the resident going to a stand? Is coverage the walker's reach, a footprint overlap or the drawn circle?
  2. Do stands and the tavern hold their own stock, refilled by their own walker from the city store, or do they draw on the city store directly?
  3. How do infrastructure needs (`0x4c`–`0x5a`) reach the house stock?
- **Faithful default:** the original's rule, once traced.
- **Update 2026-10-22 (community FAQ):** the house's reach counts, not the stall's radius. Stalls sell from the island store. The tavern keeper physically fetches alcohol within the tavern's radius. Reach at the edge is irregular, which suggests walker reach (`community-faq.md` §4).

## OQ-103: Which walkers need roads (new, tutorial series 2026-10-22)
- **Status:** **decided 2026-10-24 (owner): the rule comes from the original's code, analysed thoroughly to be 100 % sure (T30); the working rule of 2026-10-22 below is the hypothesis T30 checks clause by clause.** Was: open, RE
- **Area:** sim (transport, pathing)
- **Evidence:**
  - The market cart (job `0x34`) searches the road net (`production.md` §5.2, D).
  - The series:
    - sheep farms need no road inside a weaving hut's radius, because the weaver's carrier fetches the wool (V1 03:34);
    - the stonemason's worker walks up to the quarry without a road (V3 01:58);
    - the forester needs a road, "or nothing is collected" (V1 00:58);
    - aristocrat houses can do without roads (V14 31:05).
- **Questions:**
  1. Do the workshop carrier (job `0x33`) and the farm worker walk over open terrain within the building's radius?
  2. Which buildings need a road to the market hall (producers whose goods go to the city store)? Do houses need one at all?
  3. Are paved roads faster? (V14 34:46 calls them cosmetic; `wegspeed.dat` is not read, `production.md` §5.4.)
- **Faithful default:** the original's rule, once traced.
- **Update 2026-10-22 (owner, working rule):**
  - houses need roads connected to markets;
  - services must reach a house by road (the fire brigade, for example);
  - a road connects only at a building's **specific entrance tile**, not at any edge tile.

  The manual and the community FAQ agree on the entrance rule. FAQ posters from 2002–2004 claim that houses and the fire brigade need no roads; that **contradicts the owner** and may describe an older patch. The owner's rule is the working rule. T15 decides the mechanism with per-house reads, the same houses with and without a road to a market (PR #16 review). See `community-faq.md` §3.
- **Update 2026-10-23 (T24, what the code does):** the walk layer of a figure type is `LOGIC[0x10]` (D, `docs/re/transport.md` §3); the tables the layer selects are not read (OQ-160). The stand-in:
  - layer 3 walks on road tiles only (HEAD flag `0x4000`, P: 35 of the 921 types); P: the only figure type with layer 3 is the market cart 220;
  - every other layer walks on any tile that holds no building (roads included), among them the workshop carrier 203 and the native cart 390 (layer 0).

  So a producer feeds the market only when one of its footprint tiles touches the market's road net inside the market's area, and a workshop's carrier reaches its sources over open land. The goal is a footprint tile, not the entrance tile the owner's rule names (the entrance points are read, `BuildingType::entrances`, and used only for the spawn position; OQ-161). Houses, services and other walkers are not touched by T24. If the owner's rule differs (carriers on roads too, or the entrance tile as the only goal), the owner picks the default; the rule becomes a `Rules` option.
- **Update 2026-10-22 (in-game help):** entrances MUST face a street, or market wagons cannot pick up goods. A street connection "shortens the way" of inhabitants and workers, so they can also walk without one [H/Strassenanschluss]. Cobblestone is described as "considerably faster" for inhabitants and wagons [H/Pflasterstrasse]; that is the design intent against question 3. `wegspeed.dat` is a slope walk mask, not a speed table. Decide by timing a wagon round trip (`manual-and-help.md` §2, §4).

## OQ-104: Player controls missing from the command list (new, tutorial series 2026-10-22)
- **Status:** open, spec (S8 command set and later)
- **Area:** sim (commands), net, UI
- **Evidence (series):**
  - lock building materials per city in the market hall's "?" panel, which stops all upgrades (V3 03:28, V10 06:33, V12 02:34);
  - stop and start a building (V10 09:13);
  - the "plant" button, which fills a farm's or forester's radius with fields or trees (V1 01:10), and also turns the small farm to alcohol (V2 02:52);
  - Kontor buy orders (good, amount, price limit) and sell orders (stock level to keep, minimum price) (V2 03:24, V6 01:30, V10 02:22);
  - automatic trade routes with load and unload per Kontor (V5 04:04);
  - trade treaty and alliance in the diplomacy page (V4 02:43, V14 27:39);
  - unloading a scout and sending it over the mountains to find deposits (V5 08:14);
  - choosing an aristocrat house's look by cycling the build preview (V14 22:39).
- **Questions:**
  1. Where does the original keep the material lock (savegame `STADT2` `BAUMAT`, `production.md` §5.3?), and how does the upgrade check read it?
  2. Which of these controls are commands in the original's turn stream (`tick-loop.md`, command numbers `0x64`–`0x97`)?
  3. How is a placed house's look chosen: by the client's preview, or by the simulation's PRNG when the command executes? In lockstep the preview must never decide it silently; the choice must travel in the command.
- **Faithful default:** every control the original has, as a command. The look is carried in the build command or drawn by the sim.
- **Update 2026-10-22 (community FAQ):** the material lock is per island. A house's look is cycled by clicking the build tool on unbuildable ground, so the client chooses it and the build command must carry it. The add-on's V key plants the active farm's fields (`community-faq.md` §4).

## OQ-105: Plague and the medicus (new, tutorial series 2026-10-22)
- **Status:** open, RE
- **Area:** sim (events)
- **Evidence:**
  - From our RE (`fire-and-ships.md` §2.4, D):
    - the outbreak chance grows with inhabitants above 500;
    - plague spreads within 2 tiles and cures itself with 25 % chance per 9-step cycle;
    - the house state SEUCHE is bit `0x10`.
  - House flag bits 4/5 force a bad mood; where they are set is open (`population.md` §4.1, "Open").
  - The series:
    - the first plague came after the 1,500-inhabitant message (V13 07:02);
    - the medicus' apothecary walks to the house and cures it (V13 07:17);
    - houses out of the medicus' reach fell (V14 20:50);
    - some downgraded houses "stay afloat as long as no plague comes" (V14 33:29).
- **Questions:**
  1. Is the plague bit the same as house flag bit 4 (forced mood −1)?
  2. How does the medicus (1337, radius 24, uses herbs) choose its targets, and how fast does it act?
  3. Does the "255 = never" table entry apply to plague as it does to fire? If so, the first outbreak needs ≥ 1,524 inhabitants.
- **Faithful default:** the original's rule once traced, behind the "disasters off" rule like fire.
- **Update 2026-10-22 (community FAQ):** the medicus cures only houses in its area and some residents die first; "fast healing" shortens that; bathhouses reportedly lower the plague chance (unchecked) (`community-faq.md` §4).
- **Update 2026-10-22 (in-game help):** fire causes are a volcano, a chance event, war, and "large, crowded settlements of wooden buildings"; dense pioneer and settler cities raise the fire danger. The doctor needs herbs; "faster healing" means more house calls at once. Bandits climb the walls and rob inhabitants, and a court reduces them. Riots come from a lasting lack of a good or long unhappiness. Earthquakes mostly damage houses (`manual-and-help.md` §2, §3).

## OQ-106: Wildlife and pastures (new, tutorial series 2026-10-22)
- **Status:** open, RE
- **Area:** sim (figures, production yield)
- **Evidence:**
  - The series:
    - game for the hunter appears only where there is forest (V1 02:05);
    - deer eat the sheep's grass, so pastures without forest yield more (V10 01:08).
  - The hunter (805, input `FLEISCHTIER`) and the sheep farm (807, input `GRAS`) are in `production.md` §2.1 and §4.2. How wild animals spawn and graze is not traced.
- **Questions:**
  1. Where and how often do wild animals spawn, and does it depend on forest tiles?
  2. Do wild animals use up `GRAS` objects, competing with the sheep?
  3. Which figures carry `PELZTIER` for the fur trapper? (The series names arctic foxes, hares and bears, V14 10:03.)
- **Faithful default:** the original's rule, once traced.
- **Update 2026-10-22 (community FAQ):** game comes from forests and grazes anywhere there is grass. The fur trapper takes white-furred animals, and leopards and tigers in the jungle at half yield (`community-faq.md` §4).

## OQ-107: Research and knowledge (new, tutorial series 2026-10-22)
- **Status:** open, RE (related to O-30 in `docs/sim/state.md`)
- **Area:** sim (research)
- **Evidence:**
  - Files and RE (P, D):
    - the player research table in `docs/sim/state.md`: 65 entries, researchable and researched bits, a `u16` progress each;
    - the `Wissen:` lines in `BGruppen.dat` `[BAUSTUFEN]`, which nothing reads yet (`2B` 20, `2E` 25, `3B` 50, `3D` 70, `4C` 90; `population.md` §1).
  - The series:
    - settlers who attend school earn knowledge points;
    - the well costs 5 knowledge, and after it the fire brigade costs 5 (V3 09:17);
    - the university offers civil and military research, for example the large ore mine (V14 03:59);
    - "fast healing" and the court can be researched (V14 33:38).
- **Questions:**
  1. How do schools and universities turn pupils into knowledge? At what rate, and what do the `Wissen` lines mean?
  2. Where are the research costs and their order (the well before the fire brigade) stored?
  3. Is research per player? It is sim state that must be checksummed if it gates building.
- **Faithful default:** the original's costs and order, as data.
- **Update 2026-10-22 (answers question 1 in part):** the `Wissen:` lines are the **knowledge cap** per stage. The FAQ's cap table (20 / 25 / 50 / 70 / 90 at 50 / 170 settlers, 200 / 600 citizens, 750 merchants) equals `BGruppen.dat` `2B`/`2E`/`3B`/`3D`/`4C` exactly (P). A library adds 10; the number of schools does not matter. Research costs per item are in the FAQ's research trees, not yet checked against the data (`community-faq.md` §2, §4).
- **Update 2026-10-22 (manual and in-game help):** starting research uses up **all** accumulated knowledge [H/Wissen]. One library raises the cap, more do not. Research costs knowledge and gold. Each area starts with one key technology. X fields are university-only [M p. 48-50]. The help lists about 36 items in four areas, the checklist for the 65 player research slots. A search of four DLLs for two FAQ cost pairs found no cost table (`manual-and-help.md` §2).

## OQ-108: Trade with the free trader and computer players (new, tutorial series 2026-10-22)
- **Status:** open, RE
- **Area:** sim (trade, AI), net
- **Evidence:**
  - Our RE (D):
    - the city's sell and buy lists `VK`/`EK` and the trade flag per good (`production.md` §5.3);
    - trade sales and purchases in city statistics 4 and 5 (`population.md` §8).
  - The series:
    - a Venetian ship fills a buy order for 40 tools at 52 (base price 60); 51 still works (V2 03:24, V3 02:21);
    - a sell order sells wood above 50 t at 5 (base 8), and salt above 100 t at 23 (base 30) (V6 01:30, V10 02:22);
    - computer players come and buy what is on sale. That lengthens the relations bar until an alliance can be offered (V9 02:02, V14 27:18);
    - trading with a computer player needs a trade treaty, which can be offered once its city is discovered (V4 02:43).
- **Questions:**
  1. How does the free trader choose which orders to fill, how often, and within which price bounds?
  2. How do computer players decide to buy from a human, and how does trade change relations?
  3. Which of this is simulation state? All of it, if it moves goods or money.
- **Faithful default:** the original's rules, once traced.
- **Update 2026-10-22 (community FAQ):**
  - Venetians always sell tools, and otherwise relay other players' orders; they sail a fixed circuit; a coastal warehouse is needed;
  - the AI trades only from its main island;
  - trade visits raise relations;
  - the slider sets the stock to keep.

  Stall prices in the FAQ match `BGruppen.dat` at tax index 2 (`community-faq.md` §2, §4).
- **Update 2026-10-22 (manual and in-game help):** the price slider's arrow is green, yellow or red [M p. 43, H/Verkauf_Waren], so the UI computes an acceptance band per good; its function in `AnnoFrame.dll` is the shortest way to the trader's limits. The Venetians refuse a buy price that is too low, a sell price that is too high, goods they cannot deliver, and goods they do not want. An alliance needs a trade treaty and grows with trade volume. Five scouts in a computer player's territory is the tolerated maximum (`manual-and-help.md` §3).

## OQ-109: Fixtures over time: the original's step size depends on the frame rate (new, T16 2026-10-22)
- **Status:** open, test design (orchestrator)
- **Area:** testing (fixtures, `anno-diff`), sim
- **Evidence:**
  - Single player advances the world once per frame by `min(trunc(dt · speed + 0.5), 100)` game ms (`tick-loop.md` §2).
  - The Wine runner renders at 380–405 fps, so its steps are 2–3 ms. The mix changes between runs: dt 3 ms 57 % / 2 ms 36 % at about 380 fps, but 2 ms 54 % / 3 ms 46 % / ≥ 4 ms 0.5 % at 405 fps (`t16-timing3`, 26,344 frames at 100 %; `docs/testing/original-wine.md` §3). Windows with dgVoodoo2 steps differently (fps not recorded in T15).
  - At 50 % the rate follows the frames exactly through the rounding (Σ steps = (Σ dt + odd frames) / 2). It was 611.8 game ms/s at 380 fps and 593.8 at 405 fps, against 523 on Windows.
  - Parts of the simulation depend on the step size, e.g. the island chunk sweep per call `min(chunks · Δtick / 2000 + 1, chunks)` (`tick-loop.md` §2) and the 50 % rounding.
  - New games are seeded from `GetTickCount` (`tick-loop.md` §3). The computer players' cities differed between Wine runs (capacities 90/70/50 t in one run, 70/70/70 t in another); only the player's start (50 t, tools 50, wood 50) was the same every time.
- **Question:** How should fixtures that compare state *over time* (production, population, trade) handle this?
  1. record every frame's step (the T16 timing poller already does) and feed the same steps to `src/sim` in the S9 replay;
  2. pin the original's frame rate (Wine has no DirectDraw frame limiter; a CPU quota is inexact; a hook would not be read-only);
  3. compare only quantities that do not depend on the step size.
  Starting from a fixed savegame instead of a new game removes the seed difference in any case.
- **How to resolve:** run the same opening from the same savegame twice in the Wine runner and once on Windows, and compare the sampler's per-minute city stores.

## OQ-110: The original's multiplayer: hosting from a savegame (new, T16 2026-10-22)
- **Status:** open, try
- **Area:** testing (fixtures F5, F7, F8 in `tick-loop.md`; OQ-T15-MP), net
- **Evidence (Wine runner, two instances on one host):**
  - `HKCU\Software\Max-Design\Anno1503` `MULTIPLAYER` = 1 (`REG_DWORD`) shows the hidden Multiplayer button (`community-landscape.md`). The string sits in `AnnoFrame.dll` next to the startup splash code.
  - The LAN client lists the LAN server's session ("NoName (Unknown)") and joins it ("Please wait …"); no virtual network was needed.
  - The host's New game list is empty (no multiplayer scenarios ship: `Scenes/0500MultiEndlos` and `0700Multi` are empty, ISS-030), and Load game is empty in a fresh copy.
- **Question:** Does the host's Load game accept a single-player save placed in its copy's `SaveGame` folder and start a two-player game? If yes, F5, F7 and F8 can be measured.
- **How to resolve:** seed a quicksave from an unattended run into the host's disposable copy and retry.

## OQ-111: Profile `user.dat`: volume order and scale, speech bits (new, T16 2026-10-22)
- **Status:** open
- **Area:** formats (`docs/formats/user-dat.md`), audio
- **Question:**
  1. Which of the two `VOLUME` values is music and which is speech? Both sliders were moved together.
  2. How does the slider value (0 … −250) map to DirectSound attenuation, and is −250 silence?
  3. Which `SPEECH` bit is which category?
  4. What does `HELPPLAYER` switch?
  5. Does the game choose the active profile from the registry value `HKCU\Software\Max-Design\Anno1503` `AKTPLAYER`? The first start wrote `"AKTPLAYER"="ANNOnymous"` to the prefix's `user.reg`, and the string sits next to the profile code's strings in `AnnoFrame.dll` and `1503Startup.exe` (`user-dat.md`); the reader is not traced.
- **How to resolve:** move one slider at a time and diff the file. For the scale, trace the reader of the `VOLUME` chunk. For `AKTPLAYER`, trace the references to the string in `AnnoFrame.dll` (`tFrameStartupProfile`), or set it to a second profile's name in a run's prefix and see which profile the game opens.

## OQ-112: `COMPPROFIL2` profile columns the hand-over does not use, and its doubled `SUSPEND` call (new, T18 2026-10-22)
- **Status:** open
- **Area:** formats (`.sze`), AI
- **Question:**
  1. What are `ENTRY` columns 0, 7, 14 and 15?
  2. Is column 13 really meant as `SUSPEND`, or as one of the two `CHEATLIMIT` values?
  3. Does any start path hand the static default profile to a computer player?
- **Evidence:**
  - *Hand-over (D):* the only code found that passes profile columns on is the game-start hand-over `AnnoFrame.dll` `0x10028d90`. It passes columns 1–6, 8–13, 16–18 as AI parameters (`docs/re/annoscene.md` §11.4). Column 0 (a byte, 1 in all records), 7 (0 in all) and 15 (3 or 60) are not passed, and no other reader was found.
  - *The last call (D):* it sends parameter 26 (`SUSPEND`) a second time with column 13 as the value and column 14 as a second value (`0x10028fc5`–`0x10028fe0`). This overrides column 18. The two-value shape is that of parameter 17 `CHEATLIMIT` (setter `CompLogic.dll` `0x10004768`).
  - *Shipped values (P):* columns 13, 14 and 18 are 0 in all twelve shipped profiles, so both readings behave the same.
  - *Two readings of the unused columns (guess):*
    - *Order:* every confirmed column follows the AI key reader's test order (`CompLogic.dll` `0x10040b79`–`0x10040d4e`). That would give 7 = `OPTIONAL`, 13 = `ZIELANZAHL`, 14–15 = `CHEATLIMIT`.
    - *Value:* column 15 (3/60) looks like the `OPTIONAL=3`/`=60` of the scenarios' `COMP` records. The files speak against it (P): in `02` player 3, `04` player 1 and `05` player 3 the `COMP` record copies the player's profile (Profil03) in every mapped key, yet has `OPTIONAL=3` where column 15 is 60. Every player that `DEF` gives a profile has `OPTIONAL=3` in the eight `0010` games, and `OPTIONAL` takes the values 2, 3, 6, 10 and 60 across the corpus.
  - *Independent support for the mapping (P):* the same three exact copies match the hand-over's columns 1, 3, 4, 5, 6, 9, 10, 11 and 12 to the keys `TYP`, `LAGER`, `HANDEL`, `TRANSPORT`, `PROZENTE`, `KAMPFTYP`, `HAUPTKLIMA`, `MAXSTADT`, `ZIELGRUPPE`.
  - *Against the slip reading (D):* the `COMP` key reader stores `CHEATLIMIT=a,b` as a at message `+0x14` and b at `+0x18` (`CompLogic.dll` `0x100016e8`, `0x10001703`), so the hand-over would carry column 14 = a and column 13 = b, against the column order. The key-order reading also puts `ZIELANZAHL` at 13.
  - *Default profile (D):* the global slots start as default profiles with a valid GUID (`AnnoFrame.dll` `0x100172d0`). The setup reader `0x1001e5d0` clears its own slots and ends by committing them to the global array (`0x1001ec26` → `0x1001fad0`), but not every game-start path was traced.
- **How to resolve:**
  - *The original's own tools:* look for a profile editor screen that edits these columns (the 1503 editor is not shipped; `AnnoScene.dll`'s editor class has no caller).
  - *Ghidra on Linux:* search `AnnoFrame.dll`/`AnnoGame.dll` for further readers of a 168-byte record.
  - *Differential run (T16 runner):* patch a copy of an endless scenario with distinct values in columns 7, 13, 14, 15, start it, save, and read the computer players' `COMP` records (`CHEATLIMIT=`, `OPTIONAL=`, `ZIELANZAHL=`, `SUSPEND=`).
- **Faithful default:** the reimplementation passes exactly what `0x10028d90` passes, including the second `SUSPEND` call from column 13.

## OQ-113: What the AI parameters steer (new, T18 2026-10-22)
- **Status:** open, RE
- **Area:** AI (`CompLogic.dll`), sim
- **Question:** What do the numbered AI parameters do inside the computer logic, beyond the field the setter writes?
  - `TYP` (time intervals 37,500–57,500 per type);
  - `FLAECHE`, `LAGER`, `HANDEL`, `PROZENTE` (factors 1 ± v/100), `OPTIONAL`, `TRANSPORT`, `HAUPTKLIMA` (stored as 4/16/20);
  - `MAXSTADT`, `ZIELGRUPPE`, `MINBAUMAT`, `MINEISEN`, `CHEATLIMIT`, `TRIBUTSPERRE`;
  - `FIXDIPLOMATIE` vs. `ADDDIPLOMATIE` (both call `0x10008690`);
  - `SETHELPANLAGE` (type 11 only), `BAULOGICOFF` (`0x10003750`);
  - the unnamed indices 15, 16, 19–21, 33–35.
- **Evidence (D, `docs/re/annoscene.md` §11.5–11.6):**
  - *Setter:* `CompLogic.dll` `0x10004550`, shared by all AI classes. Every index, its range check and its object field are known.
  - *Traced further:* `KAMPFTYP` (combat-task switches and caps, `0x10003680`, `0x10043233`, `0x10004df0`), `PIRATEATTACKALL` (the pirates skip their target check, `0x1005593f`) and `SUSPEND`.
  - *Hints:* the profile pages (`Profiles/Texte/Profil01..12.htm`) describe each archetype in words. They hint at meanings but are not evidence for a formula.
- **How to resolve:**
  - *Ghidra on Linux:* trace the readers of each field (`+0xa34` … `+0xafc`) with decompiled `CompLogic.dll`.
  - *Differential runs:* run one profile with one parameter changed and compare AI behaviour (T16 runner).

  This is the AI work of PLAN M5 and O-1 in `docs/sim/state.md`.

## OQ-114: Scenario `COMP` records spell `SETTRIBUTSPERRE`, which the AI ignores (new, T18 2026-10-22)
- **Status:** decided 2026-10-22 (owner: "faithful default for OQ-114, ignore the misspelt key")
- **Area:** formats (`.sze`), AI
- **Question:** Should the reimplementation honour the tribute-block values the scenario authors set, which the original drops?
- **Decision:** no. The reimplementation ignores `SETTRIBUTSPERRE` exactly as the original does; only `TRIBUTSPERRE` reaches parameter 25.
- **Evidence:**
  - *Who writes what (D):* the editor writes parameter 25 as `SETTRIBUTSPERRE` (`AnnoScene.dll` `0x100052b0`). The AI's readers know only `TRIBUTSPERRE` (`CompLogic.dll` `0x10001583`, `0x10040c41`). Unknown keys are skipped (`strncmp` up to the last `=`). `CompLogicClassic.dll` has no `SETTRIBUTSPERRE` string (S).
  - *Affected values (P):* 267 of the 276 shipped values are 0. `endless02` sets 18 for five native peoples, `S01_04_01` sets 1 for players 1–3, `S01_05_01` 1 for player 2. The original never applies them.
  - *Other routes:* a profile's column 8 still reaches the parameter at game start (0 in all profiles), and the AI's own saves use `TRIBUTSPERRE`.
  - *What honouring it would change (D + P, `annoscene.md` §11.6):* the AI tests the value only as zero / non-zero, in one gate (`CompLogic.dll` `0x100057a0`) called from the computer and pirate classes' code, not from the native class's.
    - Five of the six affected players are native peoples (type 14: endless02's five, S01_05_01's player 2), so for them it would most likely change nothing.
    - The other three are the computer opponents of `S01_04_01` (type 12: O'Reilly, Madrugada, Pelés). Honouring the key would stop their gated action, likely tribute demands. That mission's script handles tribute itself (`SETTRIBUTE`, `PAID_TRIBUTE_TO`).
- **How to resolve:** decided (above). What the parameter does is part of OQ-113.
- **Faithful default:** ignore the key (chosen).

## OQ-120: Precedence of `&&` and `||` in the scenario script (new, T17 2026-10-22)
- **Status:** resolved 2026-10-22 (T19). `&&` and `||` have equal rank and group to the right (D, AnnoScene condition parser `0x10022bf0`): the first connective takes the whole rest of the condition as its right operand, so `A && B || C` is `A && (B || C)`.
  - **Evaluation** (`0x10010290`): left operand first; `&&` skips its right operand when the left is false, `||` evaluates both. A top `&&` with a false left runs neither THEN nor ELSE.
  - **The 28 shipped conditions** are all `guard && … && alt1 || alt2 …`, so the guards cover every alternative. Each differs from the C reading, and the Single_01 rule does not fire again.
  - **Where:** `docs/re/annoscene.md` §12, `bigbrother.md` §3.4; `tools/bigbrother.py` `grouped`/`evaluate`/`branch`.
  - **Still open:** whether evaluating a condition changes state, which would make the skip observable (OQ-123 item 5).
- **Area:** formats (`docs/formats/bigbrother.md` §3.4), script engine
- **Evidence:** 28 of the 308 `IF` conditions in 10 shipped scripts mix `&&` and `||` in one group without parentheses (P), e.g. Single_01: `MISSION_FLAG == FALSE && (…) || PLAYER "HUMAN" INHABITANTCOUNT "BUERGER" > 2999`. The interpreter keeps `&&` and `||` in one delimiter table (`AnnoScene.dll` `0x1003d898`/`0x1003d89c`, S).
- **Question:** Does `&&` bind tighter than `||` (as in C), do both group left to right, or something else? The answer decides what these missions test; with C precedence the Single_01 rule fires again after the mission flag is set.
- **How to resolve:** read the condition parser around the delimiter table (AnnoScene, the script ops `0x10011ed0`–`0x10013dab`, O-16) with `tools/disasm.py`.

## OQ-121: Script statements without `;` (new, T17 2026-10-22)
- **Status:** resolved 2026-10-22 (T19). In a block, a statement is the text up to the next `;` anywhere in the kernel text (D, AnnoScene `0x1001cbca`, `strpbrk`). The action's reader ignores what follows its arguments, so every statement that starts before that `;` is lost. The reader resumes after the block's `}`, so a `;` past it loses nothing. A top-level `SET` does not look for `;` (`0x1001cf26`–`0x1001d16c`).
  - **Shipped cases** (P):
    - `SET_FOR_BUY(…)` in S01_09_01 swallows `MINIMAP_ALERT( 640, 950 );`: that alert never runs in the original;
    - the other three lose nothing (`SET_PROFILE` runs on to a commented-out `;`, `SET_DIPLOMACY` runs on past its `}`, `SET DEADEND_ABFRAGE` is top level).
  - `QUESTLOG_DRAW;` = `QUESTLOG_DRAW();` (`0x100180c9`).
  - **Where:** `docs/re/annoscene.md` §12, `bigbrother.md` §3.4; `tools/bigbrother.py` `read_on`.
  - **Owner decision 2026-10-23:** an opt-in `rules` option restores the lost alert; the default stays faithful (lost). The C++ script engine port implements both.
- **Area:** formats (`bigbrother.md` §3.4)
- **Evidence:** four shipped statements end without `;` (P):
  - `SET_PROFILE(…)` (S01_02_01);
  - `SET DEADEND_ABFRAGE = FALSE` (S01_08_01);
  - `SET_DIPLOMACY(…)` and `SET_FOR_BUY(…)` (S01_09_01).

  Each of the four ends its line. `QUESTLOG_DRAW;` has no parentheses (single42). All of them ship in the release; whether the missions play as intended was not checked.
- **Question:** Does the interpreter end a statement at the closing parenthesis or value, or does it read the next statement as part of it, losing or merging an action?
- **How to resolve:** the statement reader in AnnoScene (O-16 range); or play S01_09_01 in the Wine runner (T16) to the rule and check whether both actions take effect.

## OQ-122: Script define numbers, `[CAMARAPOS]`, the `STARTUP` section (new, T17 2026-10-22)
- **Status:** open
- **Area:** formats (`bigbrother.md` §3.1–3.6)
- **Evidence (P):**
  - `OBJECT_FIGUR "N" n` and `OBJECT_STADT "N" n` carry one number; `OBJECT_ANLAGE "N" x,y,t` three (t an `Anlagen.dat` type);
  - `[AREA_DEFINE]` lines carry four numbers in 0–1025 whose first pair is not always the smaller;
  - `[CAMARAPOS]` is `0,0` in every file and in seven quicksaves (T16).
- **Evidence (S):** the reader knows a seventh section `STARTUP` (`0x1003e628`) that no shipped script uses.
- **Question:**
  1. Is the figure number the unit id of the `SPIELER` record, and what is the city number?
  2. Are the area numbers two corners in tiles?
  3. What does `[CAMARAPOS]` hold?
  4. What does `STARTUP` contain?
- **How to resolve:**
  - match `OBJECT_FIGUR` numbers against the unit ids in the same `.sze` (`tools/sze.py`);
  - match areas against the map;
  - trace the readers next to the section strings.

## OQ-123: What each script action and condition does (new, T17 2026-10-22)
- **Status:** open, RE
- **Area:** script engine, sim (lockstep), `bigbrother.md` §4–§6
- **Evidence:** the vocabulary, argument types and observed value ranges are in `bigbrother.md` (P). The split into state-changing and presentation actions is read from the names (likely). 62 of the 164 script names of the interpreter's tables are used by no shipped script (S).
- **Question:**
  1. Each handler's effect and the meaning of its numeric arguments (e.g. `WON`/`LOST`'s second number, `SET_DIPLOMACY`'s −128/−127/0/30);
  2. the role of every unused name (action or condition: read from the block it stands in);
  3. which conditions read state that is not synchronised in multiplayer (by its name, `SELECTED_CURSORCONSTRUCTION` likely asks about the local build cursor);
  4. ~~what `SCHUETTIINITSIG` marks~~ (T19, D: the first step runs the `INITBLOCK` while it is TRUE and then clears it, AnnoScene `0x10016b70`; `annoscene.md` §12). Still open: whether a savegame stores the variables' current values (the format strings suggest it; the quicksaves had no variables), and whether `[SIGNALS]` is applied before the first step;
  5. (T19) whether evaluating a condition changes state, for example consumes an event. If it does, `&&`'s skipped right operand and `||`'s always-evaluated one are observable (`0x10010290`). Also how many `IF`s one step evaluates: the step `0x10016b70` takes `min(count, (arg · count + 125) / 250)` per call, round-robin; what the argument is was not traced.
- **How to resolve:** walk the dispatch from the name tables to the handlers in AnnoScene (`0x10011ed0`–`0x10013dab`, O-16) and record each in `bigbrother.md`; the port of the engine (O-34) needs the list. For item 4, quick-save a mission with variables and an `INITBLOCK` in the Wine runner (T16).

## OQ-124: An intermittent crash in AnnoGame's table getter (new, T16 2026-10-22)
- **Status:** open
- **Area:** testing (Wine runner), RE (`AnnoGame.dll`)
- **Evidence:**
  - One of three long Wine runs died at about 4.45 game minutes with a read fault at `AnnoGame.dll 0x100037E4`. That is the getter `0x100037E0`, which returns `[this + 0x214 + 4·i]` (`docs/testing/original-wine.md` §2.6).
  - The run was a Citizen game: the scripted opening of `tools/orig/opening.py`, then the sampler.
  - The other two runs (12 and 30 minutes) were the same kind of game and did not crash.
- **Question:** Which caller passes the bad index or object, and is it a bug of the original (also on Windows) or of Wine?
- **How to resolve:**
  - list the callers of `0x100037E0` and find which could pass an index ≥ 4001 or a stale object;
  - repeat long runs to measure how often it happens, with a register dump (`WINEDEBUG=+seh` or winedbg's auto mode with a log);
  - on Windows, the same game under the T15 harness.

## OQ-125: Who the free-build cheat rule covers (spec F13, O-9; new, T9 S10 2026-10-23)
- **Status:** resolved 2026-10-23 by the owner: option (a), the rule stays session-wide as built (default off).
- **Area:** simulation / building UX (a cheat switch)
- **What:** in the original a client may set the free-build flag `0x2000000` in the flags word of a Build record (op 6 passes the payload's flags to Build/Place; AnnoFrame `0x1005c7f0`, `0x100589a0`), and the owner slot `+0x9c(7)` waives the cost inside Place (O-9). The rebuild carries no build flags on the wire (F13). Since S10:
  - `Rules::free_build_cheat` (hashed in the header, default **off**) waives the cost of every Build of the session, for every player: no affordability check, no charge, nothing counted as spent;
  - `Player::free_build` (state, default false; only a scenario or a save sets it) waives it for that player's own builds, with or without the rule.
- **Why it may change the feel:** with the rule on nobody pays for buildings; a narrower rule (only some players) is a different cheat.
- **Faithful default implemented:** off: every build costs what Anlagen.dat says, as for any client of the original that does not send the flag.
- **Options:** (a) keep the rule session-wide (as built); (b) let it waive only the costs of players whose `free_build` is set, so a host can grant it per player (then the rule would be "honour `Player::free_build`" and the owner's waiver of O-9 would need the rule); (c) drop the rule.

## OQ-126: Build and Demolish details the rebuild had to fill (new, T9 S10 2026-10-23)
- **Status:** open, RE (not blockers; each has a stand-in in `docs/sim/state.md` §15 "S10 as implemented")
- **Area:** simulation / commands
- **Questions:**
  1. The owner slot `+0x9c(7)` waiver: does it skip the affordability check (`0x100275d0`), the charge in Place (`0x100279a0`), or both? The rebuild skips both and counts nothing as spent (I: "cost waiver", O-9).
  2. City removal (D9: `0x10058f80` after city slot `+0x78` = `0x1002f7f0` reports empty; O-48): what happens to the removed city's tiles (the rebuild makes them neutral, so that every tile owner is a present city or neutral, §9.6), whether each building goes through `RemoveAt` (`0x10028fd0`, with its handlers and the market list's defeat flag; the rebuild runs `remove_building` per building without the Remove seam), and whether the predicate runs after every tile or only after a tile that removed something (the rebuild: after every tile while the city exists).
  3. A Demolish tile owned by the city with a building of another city on it (only a scenario can place one): the rebuild leaves it (`NoBuildingAtTile`) instead of clearing the tile object; what does the original's FindByTile / island slot `+0x7c` path do?
  4. The record head of the original's Demolish (`0x68`) in the compact multiplayer form: `docs/sim/state.md` §8.5 gives its run-length tile list `{u16 x, u16 y, u8 n, n bytes}` with signed nibbles but not what precedes it (flags, island, city?), so only `legacy::decode_0x64` exists. Read the packer next to `tNetClientNet` slot `+0x54` (`0x1005cae0`).
  5. Whether the Place and Remove handlers (O-4, O-5) draw from the PRNG when a command runs them between two advances (single player); the rebuild gives them a context at the clock's tick over the world PRNG.
  6. Two rules of the manual (`docs/research/playbook.md` §3) that S10 neither implements nor knows the home of: "dragging a frame [with the pickaxe] clears only plants and roads" [M 28], and "nothing can be built or demolished with enemy units near" [M 27]. The first may be the UI's (it decides which tiles go into the 0x68 command) or the sim's (RemoveAt skipping buildings on a multi-tile command); the second needs units (not ported) and is either a UI check or one in Place / RemoveAt. The rebuild's Demolish removes a building on every tile of a command, and Build has no unit check. Read the cursor code (`tCursorAbriss`) and the two functions.
  7. Figures whose home building was removed (Demolish, city removal) stay in the registry; their counters are gone with the building, so a release does nothing (§7.2) and §9.6 does not check the home. What the original's RemoveAt tail does with the units of a building (O-5) is unread.
- **How to resolve:** read the named functions; the Wine harness (`docs/testing/original-wine.md`) can confirm 1 and 2 by watching money, stock and the tile owners around a free build and the demolition of a last Kontor.

## OQ-127: Kontor trade-list entries: clear, append and the goods byte (new, T9 S10 2026-10-23; narrows O-14)
- **Status:** open, RE
- **Area:** simulation / trade
- **What the rebuild does until it is read (own design, N):**
  - `0x73` with goods 0 clears the entry in place (goods, amount and price 0; the list keeps its length);
  - `0x8a` (append, `kAppendIndex`) with goods 0 is refused (`BadValue`);
  - when an entry changes goods, the goods setting that named it for the old goods is unlinked (`list_index` = -1), as the original's entry removal writes `0xff` into the goods byte (`0x100285f4`, `0x10028655`);
  - `0x7c` modes 0 and 1 accept only -1 (`0xffffffff`) or the index of the entry of list `mode` that holds the goods (`BadValue` otherwise), because the loader refuses any other state (§9.6, `validate_trade`); the original stores the low byte of any value (`0x1002dbd0`).
- **Question:** does the original remove an entry (shifting the later entries and rewriting their goods bytes) or clear it in place; what `0x8a` with goods 0 does; whether the value the UI sends to `0x7c` is always the entry's index (O-57).
- **Also open (O-10):** the capacity of a list. The command layer has no limit: every accepted append grows the hashed and saved state by one entry, so a peer can grow it by one entry per command. If the original has a capacity, it becomes a `SimConfig` value with that default.
- **How to resolve:** read the `0x73` / `0x8a` handlers behind the dispatcher `0x1005ffd0` and the city methods they call, next to the removal paths above.

## OQ-128: What world property 5 does (new, T9 S11 2026-10-23; narrows O-29)
- **Status:** open, RE
- **Area:** simulation / island objects
- **What we know:** `docs/sim/state.md` §3 said property 5 is read "by the mine deposit search". It is not: the mine tick `0x1006def0` and the deposit search `0x100787c0` (read instruction by instruction, `docs/re/production.md` §4.4) do not read it. A scan of AnnoWelt for `push 5` followed by `call [reg+0x74]` (the world property getter, the same shape as the deposit's property-14 read at `0x1006368b`) finds 20 sites, among them three island-object methods next to the deposit class (`0x10063235`, `0x100634bc`, `0x100638f3`). Each of these first tests the bit of a player index (from slot `+0xd4` of the argument) in the object's mask `+0x2c`. When that bit is clear and property 5 is non-zero, it skips the rest. In `0x10063210..0x1006325f` the rest calls `0x10064250` only when the deposit's remaining amount `+0x28` or its reserve flag `+0x30` is non-zero. The setting is default true and an SP toggle at AnnoFrame `0x10006fa0` (O-29). Likely reading: something like "objects stay hidden until a player has discovered them". This reading is not verified.
- **What the rebuild does:** stores and hashes `WorldSettings::prop5`; nothing in the sim reads it (the mine needs no input from it).
- **How to resolve:** read the three methods and their callers (which vtable slot they are, who calls them per player), and the other 17 sites.

## OQ-129: Fur trappers send no trapper until the island has wildlife (new, T9 S11 2026-10-23; O-12, O-25)
- **Status:** resolved 2026-10-23 by the owner: "just stay idle until the animals are added". No stand-in; the trapper waits for the fauna port.
- **Area:** simulation / production
- **What the rebuild does (faithful):** the fur trapper's tick (`0x1006e5d0`) sends a trapper only with a target from the radius search `0x10071da0(8)`: the nearest animal of the input goods. Animals are wildlife island objects, which the sim does not create yet: their spawns come from the fauna sweep `0x10093480` (O-12, the unported `WildlifeSweep` draw site). So the search finds nothing, no trapper leaves, and a fur trapper produces only from the raw stock it starts with. In a played game, fur trappers therefore stay idle until the fauna is ported.
- **Question for the owner:** keep this until the fauna sweep is ported (the default; nothing is guessed), or add an opt-in `rules` stand-in before that (e.g. a trapper that always finds prey)? A stand-in would change the PRNG-free production numbers and is not the original.

## OQ-130: The wave simulation's far-zoom bound on a discrete adapter, and the iGPU's remaining cost (new, T14 stage 1 2026-10-23; OQ-128/129 left free for T9 S11, which ran in parallel)
- **Status:** decided 2026-10-23 for the bound (the stage 2 part below stays open). Owner, verbatim: "The option that scales best in dense late game game play on high player multiplayer matches". Orchestrator's reading: the bound on every adapter. Built in 587ca4c: `GpuLimits::fluid_max_cells_per_px` defaults to 0.25 on every adapter, 0 turns it off; the value stays 0.25.
- **Area:** render (enhanced water, the coastal wave simulation; `GpuLimits::fluid_max_cells_per_px`, `fluid::bounded_cells_per_tile`, iso.md "Wave simulation")
- **What:** the wave window covers the world area shown, so below scale 1 of the continuous zoom its cells grow about as 1 / scale² (3.7 x at the far end of a level for a 1280 x 800 window). T14 stage 1 bounds the cells per screen pixel: above 0.25 (the built groups have 0.19-0.20 at scale 1) the cells per tile are halved, i.e. one halving from scale 0.8 (0.76 for groups 0 and 1) down to the far end. On the Radeon iGPU this takes the simulation from 42 / 50 ms to 13.6 / 17.2 ms per frame at the far points at 1280 x 800 (93 / 106 to 41 / 53 ms at 2560 x 1440; docs/perf/README.md "T14 stage 1"). **What changes on screen:** only at coasts and in shallow water, where the run-up line and the relief are resolved by cells twice as large (as large on screen as group 2's cells at scale 1): up to 1.5 % of the pixels of a far-zoom frame differ by more than 8 levels (the open sea and the land are untouched; a rerun of the same build differs by under 0.1 %). Seen at 4 x magnification as slightly softer foam and shallows; at 1 x hardly. **At 2560 x 1440 also the corners:** there the unbounded window at the far points needs about 2500 cells per side and is cut to the 2048 cap, so the view's corners showed the analytic waves (straight swell lines, analytic surf); the bounded window covers the whole view, so on the iGPU the corners now show the simulated sea (`...\far\crops\a1_canyon_2560_L1_s0.452_corner.png`, left before, right after; full frames `C:\Users\altin\Pictures\anno-review\T14\before\a1\` and `...\after\a1\`, `*_2560x1440_enhanced_L1_s0.452.png` and `*_L2_s0.475.png`).
- **Default as first built (f7112b0, superseded by the decision):** automatic: the bound (0.25) on an adapter that is not discrete, where the unbounded window made a far-zoom frame 64-311 ms; **none on a discrete adapter** (the look as before; the RTX 4090 spends 0.2-2 ms on the unbounded simulation there).
- **PNGs (before = bound off, after = bound 0.25; ten `0200Single` scenarios and the canyon island, 1280 x 800, enhanced water, group 2 at 0.7 and 0.475, group 1 at 0.452, both adapters):** `C:\Users\altin\Pictures\anno-review\T14\far\a<adapter>\cells0\` (before) and `...\cells0.25\` (after); amplified differences in `...\far\diff\`, 4 x crops of the largest difference in `...\far\crops\` (`a1_Single_06_L1_s0.452_coast.png`, `a0_Single_06_L1_s0.452_coast.png`: left before, right after).
- **Owner decides:** (1) whether the discrete adapter takes the bound too (the cost there is small either way; one rule for all adapters is simpler and the far zoom then looks the same everywhere); (2) the value: 0.25 halves once and would halve again (1 cell per tile) only below scale 0.4, which the levels never reach; 0.2 would halve group 2 just below scale 1 and again below 0.5 (1 cell per tile at its far end).
- **Also for stage 2 (not decided here):** even bounded, the iGPU's frame at 1280 x 800 stays above 8 ms at every zoom with enhanced water: the simulation at scale 1 (11-14 ms at the bench's 16 ms per frame, 5.5-7 ms per step of 1/120 s), the deep-sea shading (about 9 ns per sea pixel on the iGPU: 25 ms on group 2 at 0.475, 36-40 ms on the derived levels) and the composed grid (up to 4.9 x the window's pixels). With a step costing 7-9 ms on the iGPU at the far points, about the 8.3 ms it simulates, the viewer's real-time frame there runs into the cap of 12 steps per frame (group 2 at 0.475: 34 + 12 x 8.9 = about 140 ms); the bench's fixed 16 ms per frame does not show that. These are the quality tiers of T14 stage 2.
- **Faithful default:** no stake (the wave simulation is not in the original).

## OQ-131: How a figure is removed: phase D's path and the unit destructor (new, T9 S12 2026-10-23; O-8, D12)
- **Status:** resolved by RE 2026-10-23 (T21, `docs/re/annowelt.md` "Unit registry: creation, removal and the figure destructor"); **fixed 2026-10-23 (T22)**: the four points below are built as the original's (`docs/sim/state.md` §15 "T22 as implemented", stream version 109). None of them was a feel question.
- **Area:** simulation / figures
- **What the original does (AnnoWelt.dll, all D):**
  - **The class.** Carriers, workers and small-farm animals are one unit class: constructor `0x1003b7c0`, vtable `0x100c4950`, `BEWOHNTYPE` codes `TRAEGER` 17, `MAEHER` 1, `WEIDETIER` 2 and `JAEGER` 3 through the factory `0x1009a110`.
  - **The destructor does not do the removal.** Slot `+8` (`0x10031db0` → `0x10054080` → `0x10050e00` + operator delete) frees only the unit itself. It does not null the registry entry, does not unlink the expiry or deferred list, and does not release the home building.
  - **Step 1, at the figure's end: release.**
    - The end behaviour `0x1003af00` calls slot `+0x110` = `0x1003b6e0`.
    - That calls `home→slot +0x2c(unit)` (workshop `0x100687b0`, weapons workshop `0x100692d0`, farm and fur trapper `0x1006f070`, small farm `0x1006fe40`). It nulls the building's pointer to the figure, which is the counter, and clears the unit's home `+0x78`. A second release changes nothing.
  - **Step 2: onto the expiry list.**
    - When the last step set by the end behaviour is used up, the tick `0x10034760` calls Remove(unit, 0) (registry slot `+4` = `0x100974b0`, at `0x10034844`).
    - Remove unlinks the unit from its sector cell (`0x10097d80`) and appends its vector entry to the expiry list `+0x48`.
    - The unit's stamp (property 0x23 = unit `+8`, written after every active step at `0x1003488e`) freezes at that tick.
  - **Step 3, 7500 ticks later: destroy.**
    - Phase C destroys the unit once `u16(t − stamp) ≥ 7500`.
    - `0x10097520` nulls the vector entry (index from the unit's u16 slot `+4`), calls the destructor and lowers the live count `+0x24`. The list node was unlinked by phase C.
    - The slot can be reused only after that.
  - **Phase D is never on this path.** `0x10097720` and phase D read the unit's slot `+0x40` as a record pointer, and this class returns its u16 slot index (`0x10035be0`). The deferred list holds the pooled sector records of registry slot `+0x38` (`0x10097560`), not figures.
  - **Twice on the deferred list:** possible by the code, never for a figure.
    - `0x10097720` with flag 0 appends with no membership test and without unlinking the record from its sector cell.
    - The list's append `0x10096f40` does not deduplicate; a second append leaves the record's links pointing to itself.
    - Who marks pooled records is not traced (OQ-134).
  - **When the home building is demolished.** The workshop, weapons workshop, farm and small farm destructors (`0x10068800`, `0x10069320`, `0x1006f0c0`, `0x1006fec0`) detach their figures, which **live on** without a home; their later release does nothing. Three other classes destroy their figures at once (`0x100713e0`, `0x10066f90`, `0x1006db40`, not production classes).
  - **Slot policy (A6).**
    - The free-slot search `0x10099130` is the building list's scan from hint + 1 with wrap.
    - Its minimum growth is **100** (`0x10099185`–`0x100991a1`), where the building list's is 10. A fresh registry holds one zero entry (`0x10098cd6`).
    - Slots marked in the reserved bitmap `+0x80` are skipped (OQ-134).
- **What the rebuild did (S12 stand-in), and the change (all four done in T22, 2026-10-23):**
  1. **Release at the end, not at the destroy.** *Done (T22: `end_figure`, `destroy_figure` without a release, `ReleaseCounter::figure`).* Before: `Scheduler::destroy_figure` releases the home counter in phases C and D, 7500 ticks after the end for an expiring figure. A seam that emits `ReleaseCounter` at the end is then released twice; `release_counter` stops at 0, so another figure of the same building would be under-counted.
     - **Change:** the release belongs to the figure's end (the D12 rule's or a seam's own `ReleaseCounter`, in the tick where the figure ends). `Figure::home` is cleared with it (the original clears `+0x78`), so a second release does nothing. `destroy_figure` stops releasing.
     - The counter invariant of `check_invariants` (§9.6) then counts only figures that still have a home.
  2. **The D12 interim return goes through the expiry list, not the deferred list.** *Done (T22; the deferred list, `marked_for_delete`, `mark_figure_for_delete` and phase D dropped).* Before: `mark_figure_for_delete` puts the figure on `deferred`, and phase D frees its slot in the same advance.
     - **Change:** at the return, release the counter (point 1), set the stamp to `u16(t)` and append the figure to `expiry`. Phase C frees the slot at the first advance with `u16(t' − stamp) ≥ 7500`.
     - The slot stays occupied for those 7500 ticks, which changes later slot numbers and the phase-B order. The goldens of tests that turn the rule on move.
     - No figure path appends to `deferred`. Keep the list empty for figures (`validate_figures` can require it) until the pooled sector objects are modelled (O-17), or drop it with `mark_figure_for_delete`.
  3. **The registry's minimum growth is 100.** *Done (T22: `SimConfig::figure_min_growth`).* Before: `add_figure` uses `SimConfig::list_min_growth` (10).
     - **Change:** a separate `SimConfig::figure_min_growth = 100` (data, the original's default; validated `>= 1`). It changes the vector length `n`, so the phase-B visit count `clamp(d·n/50, 1, n)` and the cursor wrap, and every golden with figures moves.
     - The scan itself (A6) is confirmed. So is `list_initial_size` 1 for the registry.
  4. **What the stamp means.** *Done (T22: renamed `expiry_stamp`, before the S15 freeze).* `Figure::created_stamp` held the tick of the figure's end in the original, the tick at which the frozen `+8` stops; it is not the spawn tick. The field works as specified (present = on `expiry`).
     - **Change:** rename it to `expiry_stamp` with S15's stream freeze. The bytes are unchanged; it is a documentation and API change only.
- **Confirmed as built:**
  - phase C's u16 test `≥ 7500` from the front of the list;
  - the stamp written after phase B, also when phase B is skipped;
  - a removed slot keeps the list's length, hint and cursor;
  - the id is never reused;
  - a release for a building that is gone does nothing (the homeless figure of a demolished production building);
  - the live count `+0x24` is derived.
- **Not modelled and not needed for figures:** the unit's u16 slot cap (65 536 slots in the original). The rebuild's wider slot type is the "no hardcoded limits" widening.

## OQ-132: Does the original's savegame keep the monopoly and 7.5 minute accumulators? (new, T9 S12 2026-10-23; D5, D10, F7)
- **Status:** RE answered 2026-10-23 (T21, `docs/re/annowelt.md` "World save and load: the accumulators"): **no, neither is saved**, and neither is the script timers' last tick nor the world clock. **Decided by the owner 2026-10-23** (asked: exact save/load as the default, with the original's load behaviour as an opt-in `rules` option, as T21 recommended; owner: "agreed"): the default stays exact save/load (the stream keeps the clock, both accumulators and the timers' last tick; resync snapshots and replays need it). This is a stated exception to the faithful-default rule of `docs/sim/state.md` §0, by the owner's decision. An opt-in `Rules` option that follows the original on loading a savegame (monopoly table recomputed on the first advance, the 7.5 minute schedule restarted, the timers' base and the tick re-set) is still to be built (S15, or the stage that implements savegame loading; OQ-135 decides whether the original always loads into a new world object).
- **Area:** simulation / save
- **What the original does (AnnoWelt.dll, all D):**
  - The world Save (`0x100ae700`) writes the chunks `WELT`, `MAIN`, `GUID`, `ALIASNAME`, `WARETYPE`, `SPIELER`, the islands, `COUNTDOWN`, `SKTIMER`, `MONOPOL2`, `LIMITS` and `K_DATA`. Load is `0x100ae270`.
  - **The monopoly accumulator, world `+0xe8`, is in no chunk.** `MONOPOL2` (`0x1009d130` and `0x1009d080`) carries only the 28 entries `{time, player}`. Load zeroes and refills those entries and leaves the accumulator as it was; only the world constructor zeroes it (`0x100a39a8`).
  - **The 7.5 minute accumulator, `+0x69e4`, is in no chunk.** `LIMITS` is the 4 bytes at `+0x69e0`. Clear (`0x100af28e`) and the constructor zero it; Load never sets it.
  - **SKTIMER saves the five active flags and counters** (`0x1009ce60`, `0x1009cdd0`), **not its last tick** `+4`. A new SKTIMER starts at 0.
  - **The world clock is not saved.** `MAIN` carries the game seconds, which Load puts into the offset `+0x69d4`, so property 7 continues. The tick itself restarts from the world object's value: 0 after the constructor or Clear. That is why the buildings' `last` is 0 after a load (F1).
  - **After a load into a new world object** (the case when AnnoScene creates the world, `0x10002e40`; whether every load does so is OQ-135):
    - the monopoly body runs once on the first advance, so the table is recomputed from the loaded state. An unchanged owner keeps its saved time; a changed owner gets the current game seconds. The original's rule is therefore "once per session", not "once per game";
    - the 7.5 minute event's schedule restarts: the first one 450 000 ms after the load;
    - the script timers continue with their saved counters.
  - **On a world object that is cleared and re-initialised instead:** the 7.5 minute schedule restarts as well, but the monopoly accumulator keeps the previous game's value.
- **What the rebuild does:** it saves and hashes all of them: `ids` gets `event750_acc` and `monopoly.acc`, `timers_monopoly` the `last_tick`, `clock_rng` the clock (`docs/sim/state.md` §9.3), as F7 saves both kinds of timed events. A save and load never changes the outcome, which the multiplayer resync snapshot and the replay rely on.
- **Asked of the owner (2026-10-23, answered in the status line above):** exact save/load as the default with the original's load behaviour as an opt-in `rules` option, or the original's behaviour as the default for savegames. The world tick does not need to restart either way (F1 covers the building stamps).
- **How to resolve:** implement the opt-in option (S15, or the savegame-loading stage, with OQ-135) and test both readings.

## OQ-133: Script timer 0: a countdown that overshoots 0 (spec F5) (new, T9 S12 2026-10-23)
- **Status:** resolved 2026-10-23 by the owner: "Stay on the original bug with optional fix". The default replicates the wrap; `Rules::timer0_clamp` (off by default) is the fix. Implemented in the S12 fix-up.
- **Area:** simulation / script
- **What the original does:** timer 0 of SKTIMER counts down by the ticks of each advance and stops only when the subtraction gives exactly 0 (`0x1009cd51` `sub` / `jne`). A step that overshoots (the counter left is smaller than the step: this depends on the step sizes, and single-player steps follow the frame time, so it can happen in any game; with exact 100 ms steps and a counter that is a multiple of 50 ticks it does not) wraps the u32 counter to about 4.29 billion ticks, so the timer runs on for about 99 days of game time and does not fire.
- **What the rebuild does (since the owner's decision):** by default the original's wrap (the timer stops, with a `TimerExpired` UI event, only on an exact landing); with `Rules::timer0_clamp` the F5 fix (clamp at 0, stop, `TimerExpired`). Nothing reads the timers before the script engine is ported (O-16, OQ-054), so today the difference is invisible. (Before the decision the code had the fix as the default, `docs/sim/state.md` §13 F5 tagged "fix".)
- **Asked of the owner (2026-10-23, answered above):** the F5 fix or the original wrap as the default.

## OQ-134: The unit registry's pooled path: who uses the deferred list and the reserved slots (new, T21 2026-10-23; O-17)
- **Status:** open, RE (no stand-in needed: no figure the rebuild models takes this path)
- **Area:** simulation / figures
- **What we know (AnnoWelt.dll, `docs/re/annowelt.md` "Unit registry"):**
  - Registry slot `+0x38` (`0x10097560`) creates pooled objects. They live in the phase-A sector cells only, not in the vector: their record comes from the pool `+0x68`, and their property 0x23 is the world tick. They are removed through slot `+0x40` (`0x10097720`): at once, or with flag 0 onto the deferred list `+0x58`, which phase D empties through `0x10097770`.
  - Read as written, the deferred path has two hazards:
    - `0x10097720` appends a record without a membership test and without unlinking it from its sector cell; a second mark makes its links point to itself;
    - deleting the **last** deferred record makes `0x10097770` treat registry `+0x50` (the expiry list's tail word) as an empty sector cell and overwrite it.
  - Registry slot `+0x58` (`0x100981d0`) sets bits in a 65 536-bit bitmap at `+0x80`; the creation path `0x10097250` skips those slots.
- **Open:**
  - which classes are created through slot `+0x38` (call sites `0x10038375`, `0x1003943f`, `0x10039497`, `0x10097aa3`, `0x10097b66`, `0x10097d47`, …);
  - who marks them with flag 0, and whether the two hazards are ever reached;
  - who reserves slots, and when (a load-time mechanism is likely: the saved registry length, `MAIN`, OQ-135).
- **How to resolve:** read the callers above and the indirect callers of registry slots `+0x40` and `+0x58`. This is needed when the ambient animals and effects (O-17) are modelled.

## OQ-135: Which world object a savegame load runs on, and where the unit registry's state goes in a save (new, T21 2026-10-23; OQ-132)
- **Status:** open, RE
- **Area:** simulation / save
- **What we know:**
  - **The world's functions.** Load (`0x100ae270`) writes neither the monopoly accumulator, the 7.5 minute accumulator, the clock nor the timers' last tick (OQ-132). Init (world slot `+0x50`, `0x100aa910`) builds a new SKTIMER, unit registry and player table. Clear (`+0x1c0`, `0x100af240`) zeroes the clock and the 7.5 minute accumulator but not the monopoly accumulator, which only the world constructor `0x100a3980` zeroes.
  - **AnnoScene.** `0x10002e40` creates a new world instance (`CreateAnnoWeltInstance`, `0x10002ea6`) only while the scene's flag `+0x164` is 0; otherwise it re-initialises the world it has. `0x100029f0` takes the world from its caller and sets the flag.
  - **The registry.** `MAIN` carries the unit registry's vector length, and Load resizes the vector to it (`0x100980e0`). The registry's stamp `+0x20`, cursor `+0x22` and expiry list were not found in the world's chunks. A registry made by Init starts with all three at 0 and empty. Load hands a `SCHIFF` chunk to registry slot `+0x18` (`0x100987a0`, not read).
- **Open:**
  - whether a savegame load always runs on a new world object. That decides whether the monopoly table is recomputed after a load (OQ-132).
  - where the units are saved (likely inside the island or `SPIELER` chunks), and whether a unit waiting on the expiry list is saved and comes back on it.
- **How to resolve:**
  - read the callers of world slots `+0x78` (Load), `+0x50` (Init) and `+0x1c0` (Clear) in AnnoScene and AnnoFrame (`0x100584a8` calls Clear), and the unit save inside the island and player chunks;
  - or one run of the original under the T16 harness, reading world `+0xe8` before and after a load.

## OQ-140: What a drought does to the tiles and the cities (spec O-15) (new, T9 S13 2026-10-23)
- **Status:** open, RE (the machine runs; its effects are a seam that does nothing)
- **Area:** simulation / disasters
- **What we know (AnnoWelt.dll `0x1008aeb0`, read in S13; `docs/sim/state.md` §5.8):** every strip of the drought visits the chunks of one side of its ring, and in each chunk the eight tiles `table[step .. step + 7]` of the 64 in-chunk offsets at `0x100ca0b8`. Expanding: at step 0 the chunk record's flag bit 2 is set and the owners of four sample tiles join the area's city mask (ported: the mask is real since S13); each city of the samples then gets an AI message 0x11 (when its player has an AI object, player property 44) and a world event 0x17 at the chunk; every tile with an object type is tested, a type of kind 0xe2 (the kind is the PRODTYPE: 226 ROHSTWACHS) first taken as the next type (`id + 1`): when the type's class (slot `+0xac(0)`) is in 0x38..0x44 and island slot `+0x140(x, y, type)` returns 1, the tile's object id goes up by 2 with event 0xce. Retreating: the flag bit is cleared, and only a tile of kind 0xe3 (227 ROHSTDUERR) goes back (type `id - 1`, twice while the kind stays 0xe3) when island slot `+0x140` returns 0, with event 0xce.
- **Open:** what island slot `+0x140` = `0x100817c0` tests (a placement or fertility check?), which object types have the class 0x38..0x44 in `Anlagen.dat` (the kinds 0xe2 / 0xe3 are known: AnnoGame 0x10007d20 returns the PRODTYPE, so they are the `ROHSTWACHS` / `ROHSTDUERR` objects such as 712..715; a field becomes its dry variant and back), what the AI does with message 0x11, and what event 0x17 shows (an alert or a minimap mark, like fire's 0x1c).
- **What the rebuild does (stand-in):** `Seams::drought_tiles` is called once per strip with the strip (rectangle, ring, phase, step, direction), default none; so a drought spreads, waits and retreats on schedule but changes no tile, sends no message and posts no UI event. The growth of fields goes on as if there were no drought.
- **Why it matters:** this is the gameplay effect of the drought (crops dry up and recover); until it is ported a drought is invisible. Faithful default once read: the original's rule; no owner decision is expected unless the reading shows a defect.
- **How to resolve:** read `0x100817c0` and the type kinds and classes of the growing objects in `Anlagen.dat` (`tools/` corpus), then implement the seam's default body with a differential fixture (T15) of a drought over a field.

## OQ-141: Where a scenario's buildings get their city, and where the island's tile owners and number come from (new, T9 S14 2026-10-23; O-36)
- **Status:** the buildings and the tile owners **resolved 2026-10-23 (T23, D)**; the island number stays likely (`+0x40` not followed). The pairs that do not match are OQ-144, the city records left are OQ-145.
- **Answered (T23, AnnoWelt, `docs/re/annowelt.md` "Scenario cities"):** the buildings come from the city's `PROD/ENTRY` (`0x100264a0`): each entry's island tile resolves through island `+0x84` = `0x10083a90` to the `PROD2` item whose footprint holds it, and `Create` (`0x10024010`) builds that type at that origin, then the class `Load` reads the entry's bytes. The tile owners come from the city's `ID3` (`0x1002e709`–`0x1002e7c5` → island `+0x100` = `0x10085620`: 8 × 8 chunks, optional row mask, unconditional) plus the footprint claim after each building (`0x10026685`, neutral tiles only). Implemented in T23 (`docs/sim/state.md` §10 "As implemented in T23").
- **Area:** simulation / scenario loading
- **What we know (AnnoWelt.dll):**
  - The island record loader `0x10089130` reads `PROD2` as a block of 6-byte items and hands each item, with the flag `0x4000000`, to island slot `+0x78` = `0x10082040` (`0x1008957d`–`0x10089676`): the item is written into the tile layer chunk by chunk. That function's branch on the object's class (`0x10082909`, tested with `0x40000000`) is not taken for this flag, and no city lookup or building creation was seen among its direct calls (its indirect calls `+0x19c`, `+0x188`, `+0xf8` and island `+0x16c`, called for object kind `0xc6` at `0x10082652`–`0x10082667`, were not all followed). **Likely:** `PROD2` places tile objects only, and the live building objects of a scenario come from the city's own records (`STADT2/PROD`, O-36; the stream loader `0x100264a0` calls `Create(rec, -1, 0)` per record, `docs/sim/state.md` §10).
  - No island record is decoded as tile owners (`STAEDTE`, `KONTORID` and the rest are sizes only, OQ-024).
  - A scenario's `INSEL5` is read by the world reader `0x100a2ea0` (reached from `0x100ae270` at `0x100ae6cb`): the head at `0x100a2efd`, the rectangle (x, y, x + w, y + h) at `0x100a2f12`–`0x100a2f4d`, `number & 0xFFFF` looked up in the island table (`0x100a2f38`–`0x100a2f5c`); a missing island is made by world `+0x88` = `0x100a24d0(0, &rect, number)` through island `+0x1c4(&rect, number)` (`0x100a2623`), which snaps the rectangle down to multiples of 8 (`0x100a253e`–`0x100a2586`), and `0x100a19a0`–`0x100a19d4` stores it at `+0x6530 + 4 · slot` with slot = the low byte of island slot `+0x40`. That `+0x40` returns the number given to `+0x1c4` is not followed. A second `INSEL5` with a number already in the table has its records read into that island. (`0x100a1ff2`, cited here first, is in the island *file* loader `0x100a1da0`, which does not use the head's position or number; corrected by the S14 review.)
- **What the rebuild did (stage 1 stand-in, replaced in T23):** a `PROD2` item of a building type became a `BuildingWithoutCity` warning (8 810 in the corpus) and was not placed; every tile was neutral (`TileOwnersUndecoded`). Still: the island slot is the head's `number` (P: unique per scenario, at most 101). Stated deviations: a second `INSEL5` with a number in use is refused (the original merges its records into the first; no scenario does it), and an island position that is not a multiple of 8 stays as written with an `UnalignedIsland` warning (the original snaps it down; none in the corpus).
- **Why it matters:** the shipped scenarios' towns (houses, production, markets) do not exist in the rebuild until this is answered, so their economies do not run.
- **How to resolve:** decode `STADT2/PROD` (the record the city's building list is read from: `0x100264a0`'s caller and the class `Load` per building) and the tile-owner storage (the island's tile records as `0x10082040` and the city creation write them); then map `PROD` entries to `BuildingSpec` with their payloads and the owners to `owner_runs`. A differential fixture (T15) of a loaded scenario's building lists would confirm the order.

## OQ-142: Which placed item a tile holds when items overlap, and which tiles a multi-tile object covers (new, T9 S14 2026-10-23; OQ-024, O-53)
- **Status:** open, RE
- **Area:** simulation / scenario loading
- **What we know:** `GELAENDE` footprints overlap on many tiles and exact duplicate items occur (insel.md "Placed objects"); in the 50 scenarios the adapter meets 3 956 items on a tile that already holds another (top-left tiles) and 50 784 exact duplicates (50 996 since T23, which counts the 212 second copies of `PROD2` building items on the same origin as duplicates too). `0x10082040` iterates the 8 × 8 chunks of an item's footprint and writes tile records under conditions that were not read instruction by instruction.
- **What the rebuild does (stand-in):** one `TileObjectSpec` per item on its top-left tile; the first item on a tile stays (`ObjectTileTaken` / `DuplicateObject` warnings for the later ones); the other tiles of a footprint carry nothing. The sim reads tile objects for growth (fields are 1 × 1), so the stand-in does not change what runs today.
- **How to resolve:** read `0x10082040`'s tile writes (which fields of the 8-byte tile record, which tiles of the footprint, the order rule for an occupied tile) and model the tile layer's object field accordingly (O-53).

## OQ-143: The people number of native, god and gang players (types 14 and 17) (new, T9 S14 2026-10-23)
- **Status:** **mapped 2026-10-24 (T26)**: the adapter maps a `VOLK` name through the 14-name table (`volk_name_index`) to `PlayerSpec::volk`, with `flags_a0 |= 0x19` and colour 0 for indices 2..12; the 173 corpus `UndecodedPeople` go to 0 (a name not in the table still warns). `docs/sim/state.md` §10 "As implemented in T26". Resolved (T25, D) below.
- **Answered (T25, `docs/re/annowelt.md` "Scenario cities" (i)):** the reader asks the game object for name kind `0x1c` (AnnoGame `0x100033c0` → `0x10013d80`, table `0x10024c50`): `0 GOTT`, `1 SPIELER`, `2 MAUREN`, `3 BEDUINEN`, `4 MONGOLEN`, `5 AFRIKANER`, `6 AZTEKEN`, `7 ESKIMOS`, `8 INDIANER`, `9 SUEDSEE`, `10 HOEHLEN`, `11 PIRATEN`, `12 VENECIA`, `13 BANDE`; indices 14–19 are null and never match. Types 8/11/12 → people 1, 15 → 11, 16 → 12 (with `+0xa0 |= 0x19`, colour 0); every other type → the index, with the bits and colour 0 for indices 2–12. P: all 373 corpus names match; type 14 gets 2–9 with the bits, type 17 `GOTT` 0 / `BANDE` 13 without, type 2 (player 31) `SPIELER` 1 / `GOTT` 0 without and `MAUREN`/`BEDUINEN`/`AZTEKEN` 2/3/6 with the bits. The colour is already 0 for every player that gets the bits; the bits add `0x19` to 6 players whose `HEAD` lacks it.
- **Area:** simulation / players
- **What we know:** the `VOLK` reader (AnnoWelt `0x1001d657`–`0x1001d753`, switch `0x1001e8f4`) sets people 1 for types 8, 11, 12, 11 for 15 and 12 for 16; for the other types it takes the index of the `VOLK` name in a 20-name list (`docs/re/annoscene.md` §6), and for people 2–12 also sets `+0xA0` bits `0x19` and colour 0. The list itself is not traced. The reader matches the name against the list first (`0x1001d69e`–`0x1001d6f7`) and sets nothing at all when it does not match, for every type; so even the people of types 8/11/12/15/16 depend on the name being in the list (P: the names with those types are `SPIELER`, `PIRATEN`, `VENECIA`; the adapter takes them as matching).
- **What the rebuild does:** an `UndecodedPeople` warning per such player (173 in the corpus); the people number stays 1 and the bits and colour as `HEAD` gave them.
- **How to resolve:** dump the 20-name table the reader indexes and map the names of the corpus (`AFRIKANER`, `AZTEKEN`, … `GOTT`, `BANDE`) to their indices; then set people, the bits and the colour as the reader does.

## OQ-144: City entries and `PROD2` items that do not pair: monuments, a tile without an item, items without an entry (new, T23 2026-10-23; OQ-141)
- **Status:** the monuments **RE'd 2026-10-23 (T25, D)** but **not mapped in T26 (blocked, see OQ-166)**: creating them as the production-list buildings T25 (j) describes would, done as `ListLayout` data, reclassify the >192 tile objects (`ROHSTWACHS` 226, `ROHSTDUERR` 227) as buildings and move the goldens; `EntryWithoutBuildingItem` stays ~34. The items without an entry and the tile without an item stay open (`0x10082040`, OQ-142).
- **Answered (T25, `docs/re/annowelt.md` "Scenario cities" (j)):** `Create` (`0x10024010`) builds a PRODTYPE above `0xa1` (here 289) as the base class (`0x100734c0`, `0x18` bytes, vtable `0x100c92b8`, `0x10024612`) and puts every PRODTYPE above 192 (and 1–32) into the production list (BuildingIndex `+0xd8`, `0x10025014`); the base `Load` (`0x10031840`) skips the entry's bytes; the footprint claim runs, the counter step does not. So each of the 34 `DENKMAL` entries creates an object in the production list and raises its `n`. A layout rule for these (a list for PRODTYPE > 192, data in `ListLayout`) is a decision of the mapping task.
- **Area:** simulation / scenario loading
- **What we know (P, the 50 `.sze`):** of the 8 561 `PROD/ENTRY` records, 34 sit on a `PROD2` item of PRODTYPE 289 (`DENKMAL`; ids 3207 in 27 entries and 3312 in 7), a type outside the layout's building blocks 1..192 (`docs/sim/state.md` §10 step 1: such a type is not a building here), and one sits on a tile that no `PROD2` item covers. The original's entry loop (D `0x100264a0`) skips an entry only when the tile's type lookup fails (`0x10026599`); a `DENKMAL` type is known, so `Create` (`0x10024010`) runs on it. `docs/sim/state.md` §10 step 1 notes that a PRODTYPE above 192 makes the original create a default-class object in the production list (which changes that list's `n`); whether these 34 entries take that path is not confirmed. Of the 8 810 `PROD2` items of building types, 212 are second copies of an equal item on the same origin (the adapter drops them as `DuplicateObject`) and 72 are named by no entry: in the original they stay tile-layer items without a building object (likely; nothing else in the city reader creates buildings).
- **What the rebuild does:** an entry without a building item → `EntryWithoutBuildingItem` (the message names the PRODTYPE of a `PROD2` item on the tile), skipped; an item without an entry → `BuildingItemWithoutEntry`, not placed (neither as a building nor as a tile object).
- **Why it matters:** monuments are part of a town's look and possibly of its status; if the original keeps them as city objects, our towns lack them, and a tile-layer item without a building that we drop leaves its tiles empty.
- **How to resolve:** read `Create`'s factory branch for a PRODTYPE above 192 (the class and the list it uses for 289–291) and the tile placement of a `PROD2` item without an entry (`0x10082040`, OQ-142); then either give the layout a list for monuments (data, `ListLayout`) or place them as tile objects.

## OQ-145: The `STADT2` records T23 does not map, the house and market records, and the context bit (new, T23 2026-10-23; O-36)
- **Status:** **narrowed again 2026-10-24 (T26)**: `BAUMAT` and `VK`/`EK` are now mapped into `City` (the `HEAD2` slot counts are read, not kept) (state.md §10 "As implemented in T26"); the house population, the market number and the statistics are read but not kept (no state field; OQ-165, OQ-158); `STAT2`/`STST2`/`STAT` and the city `VOLK` are consumed with their own warning kinds. Left: the context bit, the `SLICE`/`CREW`/one-byte entry records, and the meanings below. Split off: OQ-155 (trade-list naming), OQ-156 (the capacity recompute), OQ-157 (the research sub-object), OQ-158 (the market counter), OQ-165 (the house population state). Narrowed 2026-10-23 (T25): the records are read (D, byte maps in `docs/formats/sze.md` "Cities").
- **Answered (T25, `docs/re/annowelt.md` "Scenario cities" (f)–(h)):** house `Load` (residents, need count, version, one satisfaction byte per need; a reset rule that draws from the world sequence, never taken in the corpus) and market `Load` (`KONTID`); `HEAD2` (all 64 bytes), `HEAD`, `BAUMAT` (`BuildGoods::setting`), `VK`/`EK` (the two trade lists, `GoodsSetting`), `STAT2` (time counters, three of them not restored), `STST2` (a raw block, likely the statistics history), `STAT` (skipped), the city `VOLK` (ignored). The switch call: building slot `+0xc8(output goods i, bit)`; the bit is stored unchanged and the running-cost change is booked (the T23 reading through `+0xac` was wrong). The counter step gives a market with `KONTID` 0 the island's next number. The early exit makes the player reader return 0 (the rest of the block is not read); the `Load`s read here always return 1.
- **Left (T25):** the context byte `+0x400`; the `SLICE` (278), `CREW` (251) and one-byte (579) entry records and their classes; what the `STAT2` time counters `+0x628`…`+0x64c` time and `+0x654`/`+0x656` count; city `+0x620` (property 1) and the player mask `+0x61c` (O-40); what the `STST2` block holds sample by sample.
- **Area:** simulation / scenario loading
- **What we know (D AnnoWelt, `docs/re/annowelt.md` "Scenario cities"):** each class `Load` passes its switch bit through slot `+0xac` of the object at building `+0x14` before slot `+0xc8` stores it (the adapter takes the bit itself: likely); the entry loop `0x100264a0` also runs an object-in-rect pass (`0x100265ec`–`0x10026662`), a counter step (`0x1002668b`–`0x100266f5`) and ends early on a failed `Create` or `Load` (`0x10026732`), none modelled. The city reader `0x1002e170` also takes `HEAD`, `STAT2`, `VOLK`, `STST2`, `STAT`, `BAUMAT`, `VK` and `EK` (in the corpus: `BAUMAT`, `VK`, `EK` in all 272 cities, `STAT2` + `STST2` in 193, `STAT` in 79); `HEAD2` sets more city fields than the capacity (`+0xd8`, `+0x140`, `+0x328`, `+0x32c`, `+0x5fc`…`+0x620`); the house `Load` `0x10076e60` and the market `Load` `0x1006be00` were not read, nor the other classes with bytes (167 `UndecodedBuildingRecord` warnings in the corpus, once per scenario and class). The `STADT2` reader drops the prefix position when bit 0 of the load context byte `+0x400` is set (`0x1001e2c4`–`0x1001e2fd`); the context comes from the scene loader and is not traced (likely clear: with it set every city would be looked up at (0, 0), and all 272 prefix positions lie on island tiles, P).
- **What the rebuild does:** each such record → `UnknownRecord` once per scenario and name (scope `cities`); a building record of an unmapped class → `UndecodedBuildingRecord`, the class's default stays; the prefix position is always used.
- **Why it matters:** `VK`/`EK` are likely the town's sell and buy settings and `BAUMAT` its build goods (names only), `STAT*` its statistics; the house record likely holds residents and supply; a shipped town starts without them.
- **How to resolve:** read each record's handler in `0x1002e170` (the switch after the name pushes) and the two `Load`s, then map them to `City::goods`, `City::build_goods`, `City::trade_lists` and the house payload as each becomes part of the sim.

## OQ-150: The order of the island templates a scenario's links are resolved from (new, T9 S14 stage 2 2026-10-23; O-35) — decided
- **Status:** decided 2026-10-23 (owner): `Sorted` stays, tested against the original later (T15); `Cached` reserved (rejected by `validate`)
- **Area:** simulation / scenario loading
- **What we know (AnnoScene.dll, `docs/re/annoscene.md` §5 step 1, read instruction by instruction):** the selection takes the links' candidates in list order, and which candidate a draw picks depends on that order. A **single-player** game reads the list from `Scenes\Islands\InselCache.txt`, which the first game on a machine writes from a directory scan (`0x10007c50`, `FindFirstFileA`), so the order is the file system's enumeration order (on NTFS alphabetical by the upper-cased name, Microsoft's `FindFirstFileA` documentation; on FAT the order the files were written). A **network** game ignores the cache and takes a list built into `AnnoScene.dll` (`0x10008187`–`0x100082ad`, chosen by the 7th argument of `CreateAnnoSceneInstance`, which the game start passes as "a network client exists", `AnnoFrame` `0x1002a3e8`–`0x1002a3f8`): 428 templates in bytewise order of `<Folder>/<UPPER-CASE NAME>`. Compared entry by entry with this installation (P): the built-in list is the scan's 429 templates in that order minus `Dschungel/D_080X080_0015_2BA_SUEDSEE.SCP`.
- **What the rebuild does (default):** `Rules::island_candidate_order = Sorted`: the scan of the game directory, sorted bytewise by the upper-cased path below `Scenes/Islands` (`/`-separated). That is the network list's order, the single-player order on NTFS, and the same on every machine, so a network game needs no cache exchange. `Cached` (the machine's own `InselCache.txt` order) is a reserved value that `Rules::validate` rejects.
- **Consequence:** with the same seed the rebuild picks what the original's network game would pick only if the candidate set is the built-in one; here it holds the one template more (the SUEDSEE island), which shifts the picks of dschungel 80 × 80 links. The single-player original on NTFS uses the same 429 in the same order. Neither original choice is reproducible across installations (a FAT drive, a modded folder), so no differential fixture can pin it.
- **Decision (owner, 2026-10-23):** `Sorted` over the game directory stays; it is tested against the original later (T15). The options were: keep `Sorted` (default), or (a) drop the templates the network list does not have (reading the list from `AnnoScene.dll` at run time, never committing it), or (b) implement `Cached` for single-player fidelity on odd file systems.

## OQ-151: What the link placement does beyond placing the template: ores on mountains, native settlements, treasures (new, T9 S14 stage 2 2026-10-23; O-37)
- **Status:** open, RE
- **Area:** simulation / scenario loading
- **What we know (AnnoScene.dll `0x10008490`, `docs/re/annoscene.md` §5 step 10):** after placing a template the placement collects the island's objects (island `+0x28`) whose slot `+0x84` returns 1..6 and slot `+0x78(4)` returns 0 and puts each assigned ore kind 0..6 on one of them with a draw (`draw mod count`, linear probe), giving it the type the resolver slot `+0x6c` returns for the id table at `0x1003AF24` (`0x67D`, `0x619`, `0x61A`, `0x61B`, 0, `0x61C`, `0x618`); the deposits in a 15 × 15 rectangle around each such object are updated through `0x1007c0d0`. Native settlements of the template get a city and a player name drawn from `NATIVE_STADT` / `NATIVE_SPIELER`; a treasure flag creates object `0x60E` (or `0x60F`, one more draw on climates 4..6) at a spot `0x10008cc0` scans for (one more draw).
- **What the rebuild does:** the island is placed; the ores become `LinkOresUnplaced` warnings (189 in the corpus) and no deposit is made, the template's `SPIELER` blocks `LinkTemplatePlayers` (33), the treasures `LinkTreasureUnplaced` (102). The draws are the unported load sites `LinkOrePlace`, `LinkNativeName`, `LinkTreasureKind`, `LinkTreasurePlace` (PRNG timeline version 2), so the cursor after a load is short of the original's by them.
- **How to resolve:** read the object class behind slots `+0x84`/`+0x78`/`+0x28` (mountains by the shape) and `0x1007c0d0` (what a deposit takes from its mountain: the goods of `[deposit+4]` slot `+0x80`, O-37), map the ids of `0x1003AF24` to goods, read the native setup and `0x10008cc0`; then port the draws with a timeline bump.

## OQ-152: What island property 11 (the raw-material mask a link island gets) governs (new, T9 S14 stage 2 2026-10-23)
- **Status:** open, RE
- **Area:** simulation / islands
- **What we know:** the placement sets island property 11 = `0xF009` | (1 << k for each raw-material kind k the selection marked) (`0x100085a2`–`0x100085cf`); the 20 kinds and the climate masks of the selection (`0x10009010`) are the values of the monopoly masks (`SimConfig::monopoly_climate_mask`, AnnoWelt `0x1009d2a0`), so the kinds are the 20 raw materials of the monopoly table. Who reads property 11, and what an INSEL5 island of a scenario has there, is not traced.
- **What the rebuild does:** the marks are in the resolution (`ResolvedLink::raw_marks`, printed by `anno-sim-replay scenario`) and a `LinkRawMaterials` warning per placed link (229 in the corpus); the sim has no island field for them.
- **How to resolve:** find the readers of island property 11 (the planting rules or the fertility check, e.g. what a farm may grow on the island) and where an INSEL5 island's value comes from; then add the field with a stream bump.

## OQ-153: A city on a link island (new, T9 S14 stage 2 2026-10-23)
- **Status:** open, low priority (no case in the corpus)
- **Area:** simulation / scenario loading
- **What we know:** the original places the link islands while it reads the `ERZE` record (`0x1000a25c`), before the state series of `SPIELER` blocks, so a `STADT2` there could name a tile of a link island. The rebuild resolves and places the links after `load_scenario` has read the whole file, so such a city finds no island and becomes `CityOffIsland` (and is skipped).
- **What the rebuild does:** as said; P: 0 `CityOffIsland` warnings in the 50 scenarios (every `STADT2` lies on an `INSEL5` island).
- **How to resolve:** if a scenario needs it, let `place_island_links` re-run the `STADT2` lookup for the cities that found no island.

## OQ-154: Does anything draw between the scene's HEAD and ERZE, and when does the world reset run? (new, T9 S14 stage 2 2026-10-23; O-35)
- **Status:** open, RE
- **Area:** simulation / PRNG timeline
- **What we know:** the original creates the world at `HEAD` and sets its cursor to the seed (world property 8, `docs/re/annoscene.md` §4); the island selection draws at `ERZE` (`0x1000a25c`). Between them the file holds `WARETYPE`, `SKTIMER`, `MONOPOL`/`MONOPOL2`, `LIMITS`, `COMPPROFIL2`, the header-series `SPIELER` blocks and the `INSEL5` islands, which go to the world's chunk reader (world slot `+0x78`); their readers were not read for draws. One vtable call at slot `+0xb4` right after the player `HEAD` reader (AnnoWelt `0x1001d538`) is on the player object, so probably not the world draw, but it is not resolved. The world reset `0x100af240` sets the cursor to 0; when it runs relative to a scene load is open (§4). The template load inside the placement (world slot `+0x88`) is not traced for draws either.
- **What the rebuild does:** the selection's draws start at `spec.seed` mod the table length (`resolve_scenario_links`), as if nothing drew between `HEAD` and `ERZE`.
- **How to resolve:** read the readers of those records and `0x1001d538`'s target for `+0xb4` calls; find the callers of `0x100af240` around a game start (AnnoFrame `0x10029100`).

## OQ-155: The two trade lists: which is sale and which purchase, and the entry field `+4` (new, T25 2026-10-23; O-14, OQ-145)
- **Status:** open (the naming is likely; `+4` needs RE). **Note (round w, T27b):** 39 shipped cities carry `HEAD2` trade-list slot counts other than the constructor defaults 4/6 (per T26's corpus); T26 did not keep them (no `City` field), and today the list length happens to equal the entries read. A `City::trade_list_slot_counts` field (or a per-list slot-count byte) comes with the trade stage, not before (owner, 2026-10-24 w): one stream bump, together with the trade stage's other fields.
- **Area:** simulation / trade
- **What we know (D AnnoWelt, `docs/re/annowelt.md` "Scenario cities" (g)):** `VK` fills the list at city `+0xdc` (8 entries, slot count `+0x140`, default 4) and sets bit 0 of the goods record's `+0xa`; `EK` fills `+0x44` (12 entries, count `+0xd8`, default 6) and clears it; city slot `+0x68` returns `+0x44` for 0 and `+0xdc` otherwise, so the bit is the list number. Entry: `+4` u16 (`VK`/`EK` `[0]`), `+6` goods, `+8` u32 amount (8.8, lowered to the capacity by `0x10028400`). The loop index is not checked against the list's 8 or 12 entries.
- **The question:** `VK` is *Verkauf* (sale) and `EK` *Einkauf* (purchase) by name, so the rebuild's `GoodsSetting::buy` (true for bit 0 = 1) names the `VK` list: is `buy` the wrong way round? What does `+4` hold (the price, as `docs/sim/state.md` calls it; P: 1–136 in the 42 entries with a value), and what do the trade routines do with `+4` and `+8`?
- **What the rebuild does:** `trade_lists[0]`/`[1]` and `GoodsSetting::buy` as in `docs/sim/state.md` §4.7; the scenario lists are not loaded yet.
- **How to resolve:** read the Kontor trade function that uses the lists (`0x100ac360` books trade sales and purchases, `population.md` §8) and the UI's labels for the two lists; then rename the field (same bit, no behaviour change) and map `[0]` to `price` or its real name.

## OQ-156: When the warehouse capacity and the trade slot counts are recomputed from the markets (new, T25 2026-10-23; OQ-145)
- **Status:** open, RE
- **Area:** simulation / scenario loading, Kontor
- **What we know (D AnnoWelt, `docs/re/annowelt.md` "Scenario cities" (g)):** city slot `+0xbc` (`0x10028400`) counts the buildings of the list selected by `0x21` (likely the market list) and takes the highest type byte `+0x23` among them; capacity = the u16 table at `0x100dcc84`, entry min(count, 124), << 8 (0, 50, 70, 90, 110, …, 900 t); stocks and trade amounts are lowered when it shrinks, and the slot counts become (EK, VK) = (6, 4), (8, 6) or (8, 8) for `+0x23` < 4, < 11, else. The Build path calls it after `Create` (`0x10027c5d`); the scenario path (`0x1002e170`, `0x100264a0`, `Create`) does not. P: `HEAD2 [0x24]` equals the table entry for the number of `KONTID` records in 259 of the 272 cities; the 13 others hold 60, 70, 100, 120 or 190 t for 2–16 markets.
- **The question:** is slot `+0xbc` called after a scenario or savegame load (then `HEAD2`'s capacity and slot counts would be overwritten for those 13 cities), and what is type byte `+0x23` (which Anlagen field, which types have which value)?
- **Why it matters:** a loaded town's warehouse capacity and number of trade slots.
- **How to resolve:** find the callers of `0x10028400` (direct and through the vtable slot) on the load path; read where AnnoGame fills type `+0x23` (the `HEAD` loader `0x10009334`–`0x100094cd`).

## OQ-157: The city sub-object `+0x320`: research topic, points and the libraries (new, T25 2026-10-23; OQ-145)
- **Status:** open, RE (the reading is likely)
- **Area:** simulation / research
- **What we know (D AnnoWelt, `docs/re/annowelt.md` "Scenario cities" (g)):** the sub-object (vtable `0x100c2edc`, constructor `0x10027460`) holds a signed byte `+4` (city `+0x324`, `HEAD2 [0]`), a tick stamp `+6`, `+8` (city `+0x328`, `HEAD2 [0x34]` << 8), `+0xc` (city `+0x32c`, `HEAD2 [0x36]` << 8) and the owner at `+0x10`. Every 250 ticks, while `+4` ≠ 0, it calls slot 0 `(+4, 0x100)` of the owner's player slot `+0xbc` object and clears `+4` when that object's slot 8 returns bit value 2. Its slot 0 (`0x10027280`) starts a topic: two values from the owner object's slot `+0x10`, the first handed to player slot `+0x68`, the second subtracted from `+8`. City `+0x32c` is recomputed from the city's `BIBLIOTHEK` buildings (`0x1002d2fd`–`0x1002d33b`). P: `HEAD2 [0]` = 17 and `[0x34]` = `[0x36]` = 25 in one city each.
- **The question:** is player slot `+0xbc` the research object (the `FORSCH` records), `+4` a research id (kind `0x38`), `+8` research points and `+0xc` the libraries' rate? What do slot 0's two values and player slot `+0x68` mean?
- **How to resolve:** read the player slot `+0xbc` object and its slots 0, 8 and `+0x10`, the function ending at `0x1002d345` and its caller, and player slot `+0x68`.

## OQ-158: The island's market counter (property `0x11`) and loaded `KONTID` numbers (new, T25 2026-10-23; OQ-145)
- **Status:** open, RE
- **Area:** simulation / scenario loading
- **What we know (D AnnoWelt, `docs/re/annowelt.md` "Scenario cities" (f)):** a market whose `KONTID` is 0 after its `Load` gets the island's property `0x11`, and the property is raised by one (entry loop `0x100266a5`–`0x100266f5`, Build path `0x10027c63`–`0x10027c9d`); a loaded non-zero number does not touch the counter. P: every market of the corpus has a non-zero number, unique on its island.
- **The question:** where does the island's property `0x11` come from on a load (an island record such as `KONTORID`, OQ-024, or 0)? If it starts below the loaded numbers, the next market built gets a number already in use: does anything key on `KONTID` (the city function around `0x1002893b` reads property `0xe` of the buildings in a rectangle)?
- **How to resolve:** read island property `0x11` in the island's property getter and setter and in the island record loader `0x10089130`; read the consumer at `0x1002893b`.

## OQ-160: Which tiles a cart and a carrier may enter: the walking layers (new, T24 2026-10-23; OQ-103, O-8)
- **Status:** **decided 2026-10-24 (owner): the walk rule comes from the original's code, analysed thoroughly to be 100 % sure (T30: the static decode and a check in the running original); the stand-in stays until T33 implements the decoded rule.** Was: open, RE; the road rule is the owner's to confirm (OQ-103)
- **Area:** simulation / transport
- **What we know (AnnoWelt, `docs/re/transport.md` §3, D):**
  - the figure's searcher takes the walking layer from the figure type's byte `+0xff` (`LOGIC[0x10]`, AnnoGame `0x1000faf0`);
  - the layer selects the table `0x100d1140 + 0x1c00 · layer` (engine `+0x430`);
  - the cells come from the island's tiles through island slot `+0x128` in mode 0, which reads a 9-bit mask per tile (likely the `WALKMASK` of the tile's object; every `ANLAGEN` entry has one, P 921 of 921).

  P: figure 220 (the market cart) has layer 3, the only one; 287 figure types have layer 0, among them the carrier 203 and the native cart 390.
- **Update 2026-10-23 (T27, `docs/re/transport.md` §3.1; corrected after the round v review 2026-10-24):** D: the layer tables lie in initialised `.data`, `0x1c00` bytes per layer, five used (the sixth block is zero, P); the setters are `0x1000dd20` (`[engine+0x430]`) and `0x1000f010` (`[+0x1030]` of an object with another vtable); the base-relocation table has exactly these two absolute references to the tables and none into them, so a run-time write could only go through the stored pointers (not searched: "not written at run time" is **likely**). The cell a tile gives the search is `table[mask] | byte << 16` (`0x1008ff70`, loop `0x10090050`–`0x1009007b`, the u16 table `0x100c1af8`). P: the blocks hold 42–87 distinct byte values each, so their entries are not plain flags and the `0xff` counts (316 / 409 / 561 / 75 / 613) do not rank the layers. Still **open:** the meaning of an entry. The search functions `0x10003d90` / `0x10004890` do not read `+0x430`; the reader is `0x1000daf0` (vtable `0x100c18b0`; its twin `0x1000ed40`, vtable `0x100c1a30`): the next addresses (T30 step 2).
- **What the rebuild does (stand-in, `transport.cpp` `walk_rule`):**
  - layer `kRoadLayer` = 3 walks on road tiles only (a tile whose object type has the HEAD flag `0x4000`);
  - every other layer walks on tiles without a building (no object, or an object whose type has no building list) and on roads;
  - the home's footprint and the candidates' footprints may always be entered.

  A tile is a whole tile; the original's 3 × 3 sub-cells and the walk masks are not used.
- **Terrain (T27 step 7; corrected after the round v review):** the stand-in ignores water and rivers. **Rivers are in the sim state:** the islands' `FLUESSE` items are tile objects (D: `scenario_formats.cpp`, the GELAENDE/FLUESSE/PROD2 item loop; `scenario.cpp` writes each `TileObjectSpec` with `set_object` on its footprint's top-left tile only); their types (2701.., P per the review: 2701–2707, no road flag) have no list, so their `walk_class` is 0 and an open-land walker crosses them. Open sea carries no tile object, so the state cannot tell it from open land. T27 step 7 had read "water carries object 0 like open land" for both, which is false for rivers. **Owner decision 2026-10-24:** no stand-in river rule; T30 step 5 reads how the original treats the river tiles (which footprint cells, bridges), T33 implements it with the decoded walk rule. See OQ-170.
- **Update 2026-10-24 (T30, partial, `docs/re/transport.md` §3.2–§3.5):** D: the reader `0x1000daf0` builds a 16-byte stack buffer from two tables (`[this + 0x430]` the primary layer table, `[this + 0x434]` a secondary) indexed by two u16s of the cell (`cell[0]` from the mapped 9-bit mask, `cell[2]` from the byte at bits 8–15), then calls a vtable method `[edx + 0xec]` on object `[this + 0x858]` with the buffer. The yes/no decision lives in that method — the **next address to read**, OQ-175. The relocation table confirms the five blocks have **no absolute reference inside** them, so run-time writes (if any) go through the stored pointer (`[engine + 0x430]` / `[+0x1030]`). Named-road PRODTYPEs: STRASSE, RAMPE/LOW/HIGH, BRUECKE, PIER; 11 of 37 named roads carry `0x1000` instead of `0x4000`, every named-road WALKMASK is 12 zero bytes. T30 step 7(a) (memory compare after a scenario loads) and the vtable method of §3.3 settle the mask semantics.
- **How to resolve:** read the vtable method `[edx + 0xec]` on `[this + 0x858]` named in §3.3 (next address); then compare the five layer blocks with the running game's memory (step 7(a), needs the figure-position reader of T30b for the full step 7 check). Until then the layer value and the rule stay in `transport.cpp` (`kRoadLayer`).
- **T29's top-15 frontier type ids (the first tiles for T30 step 5 to check):** `S01_07_01` — type 0 (no object; empty tiles), 69 / 355 / 2 / 409 (prodtype 225, flags 0x29, walk_class 0; likely trees/decorations), 2527 "Dirt road" / 2507 "Cobblestone street" / 2526 "Dirt road" (prodtype 193, flags 0x1186, walk_class 0 under the stand-in; this is OQ-171), 2550 "Marketplace" (prodtype 291, walk_class 0), 540 "Citizen's house" / 807 "Sheep farm" / 811 "Grain farm" / 1005 "Weaving mill" / 801 "Forester's hut" / 1379 "University" (flags 0x84, walk_class 2 buildings). `single36` — type 0, then 3100–3109 (native tribe tile objects, walk_class 2) with 2 and 71 / 368 (prodtype 225 trees) and 503 / 509 "Pioneer house". The top 15 of each run cover the frontier a cart meets most often: open tiles, trees/decorations, "road-like" tiles the stand-in classifies non-road, and building footprints. See REPORTS.md T29 for the counts.

## OQ-161: How the original's search expands, and which nodes are goals (new, T24 2026-10-23; O-8)
- **Status:** open, RE
- **Area:** simulation / transport
- **What we know (D, `docs/re/transport.md` §3):**
  - the search runs over 4 × 4-cell blocks made on demand;
  - its callback is called on a goal node (`0x10003d90`), its best score starts at 0 every run (`0x10003dbf`), and it stops when the callback answers 0 (`0x10003210`);
  - the goals are the candidates' entrance points (`0x10072be0`);
  - the path is a list of `{direction, count}` pairs in thirds of a tile (`0x10005890`).

  The node expansion (order, costs, ties) and the disc fill's arithmetic (`0x10003780`) are not read in full.
- **What the rebuild does (stand-in, `transport.cpp` `search`):** a Dijkstra over the area's tiles (the candidate square cut to the island, the disc of `disc_half_widths`, D):
  - costs 5 straight and 7 diagonal;
  - neighbours in the order of the segment builder's direction codes (`0x1002ff20`);
  - ties in insertion order;
  - no diagonal past a blocked orthogonal neighbour;
  - the starts are the home's footprint tiles;
  - the goals are the candidates' footprint tiles (not their entrance points);
  - a goal is scored once per search (its other tiles score the same under the strict `<`) and never walked through.

  The work per search is bounded by the area: at most `(2r + 2)²` tiles, `r` the home type's action radius (P: at most 24, so 2 500 tiles); measured on `single36`: carts read 909 tiles per search on average, at most 2 046; carriers at most 237.
- **Consequence:** with several candidates at different distances the original may score them in another order than the stand-in, and so pick another source when the early stop fires; the route's tiles and length can differ.
- **How to resolve:** read the expansion of the engine at world `+0x6aa8` (vtable `0x100c1424`) and the entrance goals; then replace the stand-in with a stream and timeline bump.

## OQ-162: The walk in integers instead of the original's x87 floats (new, T24 2026-10-23; D13) — owner
- **Status:** **decided 2026-10-24 (owner): the float walk** — the original's x87 walk reproduced bit-exactly, emulated in integer code with the floats' bit patterns in the state, so no native float enters hashed state (T34, ADR 0002); the integer walk stays until T34 stage 2. Was: open, owner decision (the default is the integer walk; no float may enter hashed state)
- **Area:** simulation / transport
- **What the original does (D, `docs/re/transport.md` §4):** a figure's time is `u16(t − stamp) · 0.2f` in x87 floats; the walk moves `time · speed` along f32 segments of `count / 3` tiles (diagonals × `1.41421354f`); the rest of the time after an action goes on to the next in the same call; the position is three floats.
- **What the rebuild does (`transport.cpp` `walk`, `figure_type_from_entry`):**
  - the speed is precomputed per figure type as `round(f32 · 0.2f · 2^24)` in double precision (2^-24 tile per tick; the cart 53 687, the carrier 40 265);
  - a route is a list of tile centres in Q24.8;
  - a diagonal tile is 23 726 566 / 2^24 long;
  - the progress is integer;
  - the time left after an arrival is carried at a tick's resolution (`dist / speed`, rounded down);
  - the load wait is `round(f32 · 500)` ticks.
- **Consequence:** a trip's length in ticks can differ from the original's by rounding (a tick per action or less for these speeds), and a figure walks tile centre to tile centre, not along the original's sub-cell path (OQ-161). The hash stays platform independent.
- **Decision for the owner:** keep the integer walk (default), or ask for a fixed-point model of the x87 sequence once differential fixtures (T15) show the difference matters.

## OQ-165: The house population into the state — residents, stocks and the reset rule (new, T26 2026-10-24; OQ-145)
- **Status:** open, blocked (needs the population model and a state field) — decision for the orchestrator/owner. Owner 2026-10-24: maybe both (per house and in the city); to be settled by analysing the original's files (T32 step 3).
- **Area:** simulation / scenario loading, population
- **What we know:** a `STADT2` house record (D AnnoWelt `0x10076e60`, `docs/re/annowelt.md` "Scenario cities" (f), `docs/re/population.md` §2/§5) carries `[1]` residents, `[2]` need count `n`, `[3]` version, and per need a satisfaction byte `s_j` (version 2) or the second byte of a pair (version 1); the stock is `trunc(Menge_j · s_j · residents · 128 / 1536)` with `Menge_j` from the group's `BGRUPPE` record, and a reset rule (group above pioneers and residents below half the lower group's `Wohnmax`) draws from the world sequence.
- **Why T26 could not map it:** a house building in the rebuild is the `Inert` payload (`building.hpp`), with no resident or stock field; the sim holds no `BGruppen` group data (`Menge`, `Wohnmax`, the need table — the population model is not ported (no stage in `state.md` §15 yet; PLAN M3 orders it after transport)); and T26's rule is **no new state field** (a field is a stream bump the transport stage owns). The reset rule also cannot be evaluated without `Wohnmax`, so it cannot even emit a per-house warning.
- **What the rebuild does (T26):** the house record is read and consumed (so it is not `UndecodedBuildingRecord`), its residents counted for the economy diagnostic (`LoadedScenario::houses`/`houses_with_residents`/`resident_sum`), and a `HouseRecordNotModelled` warning emitted once per scenario. The `s_j → stock` formula (`house_need_stock`), the version-0/1/2 decode (`house_satisfaction`) and the reset predicate (`house_reset_applies`) are implemented and unit-tested as pure functions for the population stage. **Nothing is drawn** (the corpus PRNG timeline does not move); `World::create` does not draw for a non-pioneer house either (no `SetGroup`).
- **How to resolve:** when the population model is ported (no stage in `state.md` §15 yet; PLAN M3 orders it after transport), give the house payload a residents field and the group table its `Menge`/`Wohnmax`, then apply the formula and the reset (a new draw site = a `kPrngTimelineVersion` bump) and re-record the goldens in that commit. Decision owed: whether a house carries its own residents/stocks in the state (a field) or they are derived from the city's resident counters.

## OQ-166: Monuments (PRODTYPE > 192) as buildings without moving the goldens (new, T26 2026-10-24; OQ-144)
- **Status:** open, blocked under T26's constraints — RE / design. Owner 2026-10-24: monuments are decorations a player earns in a running game, endless games included, for reaching a threshold (example: the population satisfied for 30 minutes in a row; not confirmed); T31 analyses how the original handles them before any code; option 1 (the scenario path) waits for T31.
- **Area:** simulation / scenario loading
- **What we know:** T25 (j) read that the scenario `PROD/ENTRY` loader's `Create` (`0x10024010`) builds a PRODTYPE > 192 (e.g. `DENKMAL` 289) as a base-class object in the production list, even though the interactive Place skips Create for ≥ 193 (`class_for_prodtype`, `0x10027b32`: those are tile objects — roads, walls, monuments, rocks, water, ruins). So >192 is a building **only on the scenario-load path**, not in general.
- **Why T26 could not do it as "ListLayout data":** `list_for`/`class_for_prodtype` return "not a building" for ≥ 193, and several >192 PRODTYPEs are genuine tile objects (`ROHSTWACHS` 226, `ROHSTDUERR` 227) that the synthetic goldens place as growing objects. Making `list_for(>192)` return the production list reclassifies those as buildings and moves the goldens; extending/altering `block_to_list` or `default_list` changes a hashed layout field and moves **all** goldens. Both break T26's rule that only `scenario_corpus.txt` is re-recorded.
- **What the rebuild does (T26):** a `DENKMAL`/>192 entry stays `EntryWithoutBuildingItem` (the ~34 corpus monuments are not created).
- **How to resolve:** a scenario-adapter-only monument path — `create_building` accepting a forced list for an entry whose PRODTYPE is a monument (the base class, `Inert`, tick `None`, footprint claimed) — so the general building classification and the tile-object goldens are untouched; or model the monuments as tile objects. Either way the corpus `create_hash` of the files with monuments moves, so it is re-recorded in that commit.

## OQ-171: How many of the stand-in's "unreached" are the "road-like" tile objects without `kRoadFlag` (new, T29 2026-10-24; OQ-160)
- **Status:** open, measurement observation
- **Area:** simulation / transport
- **Why:** T29 ran 150 000 ticks of `S01_07_01` and `single36` with every `choose_source` call classified; the top-15 frontier type ids include three "road-like" tile objects — `2507` "Cobblestone street", `2526` "Dirt road", `2527` "Dirt road" (all `prodtype=193`, `flags=0x1186`, `walk_class=0` under the stand-in). Their aggregate frontier count in `S01_07_01` is ~30 000 of 138 000 non-empty counts: a cart's search sees them as blocked, although their names say road. Either the game's `kRoadFlag` has an extra bit the T27 derivation does not read, or these tiles are not road per the engine (then players must still make an actual road), or the walking-layer tables (T30 step 4) admit them under their own rule, not through `kRoadFlag`.
- **Update 2026-10-24 (T30 step 5, `docs/re/transport.md` §3.2):** D: of 37 named-road entries in `Anlagen.dat`, **26 carry `0x4000` (kRoadFlag) and 11 carry `0x1000` instead** (bits 12 and 14 never set together across the 921 entries). Ids `2505–2508` (STRASSE Cobblestone street), `2517, 2536–2537` (BRUECKE Stone/Wooden bridge), `2525–2528` (STRASSE Dirt road) have flags `0x1186`/`0x1386`. All 37 named-road WALKMASK records are 12 zero bytes, so named-road walkability is not stored per cell on the type: it must come from the HEAD `flags_word` and/or the walking-layer tables. (counts corrected in T30b.1; T30 wrote 36 and 10) **Open:** whether `0x1000` marks a tile walkable or not for layer 3 in the real rule (T30 step 2's vtable call `[edx + 0xec]` on `[this + 0x858]` is the decision point, §3.3; T30 step 7(a) is a cheaper decider — compare the five 7168-byte blocks in the running game's memory with the DLL image).
- **How to resolve:** T30 step 4 (the per-layer rule from the engine blocks and masks) and T30 step 5 (the tile side: which bits each tile kind writes); if the walking-layer admits them, T33 inherits the behaviour; if not, the frontier count is a measurement of a game mechanic rather than a bug.
- The tool `tools/anlagen_roads.py` reproduces the table (T30b.1).

## OQ-172: `edge` counts in the stand-in search of `S01_07_01` (new, T29 2026-10-24; OQ-160)
- **Status:** open, measurement observation
- **Area:** simulation / transport
- **Why:** 1 594 of the 13 592 `unreached` cart/carrier searches in `S01_07_01`'s 150 000-tick run ended with reached tiles on the search grid's border (`edge` = 11.7 % of `unreached`). An "area cut" where the home's radius-8 disc stops before the candidate's road reaches it is therefore a minority cause. In `single36` it is 304 / 556 (`edge` = 54 %), so the share depends on the scenario (smaller towns, more carts that *could* reach a goal if the area were wider).
- **How to resolve:** T30 steps 2–6 settle whether the original's search uses the same area shape (the radius table of `0x10002a60`), the same stop test, and the same corner rule; if yes, "area too small for this town" is a feature; if the original walks further, the stand-in's area is a defect.

## OQ-173: The `corner` share: 32 525 corner refusals in 13 592 unreached searches of `S01_07_01` (new, T29 2026-10-24; OQ-161)
- **Status:** open, measurement observation
- **Area:** simulation / transport
- **Why:** the stand-in's rule "no diagonal passes a blocked orthogonal neighbour" refuses ~2.4 steps per `unreached` search in `S01_07_01` (781 refusals over 556 unreached in `single36`, ~1.4 per search). The step itself is own design — T27 added it as the common guard against corner cutting around buildings; the original's rule is open (OQ-161). The share is high enough that a different rule (e.g. "the diagonal is allowed if either orthogonal is passable") would reach some of the frontier.
- **How to resolve:** T30 step 2 (the engine's expansion rule in `0x10003d90` / `0x10004890`); T33 inherits.

## OQ-174: Senders and goals without a road 4-neighbour: a per-scenario minority (new, T29 2026-10-24; OQ-103)
- **Status:** open, measurement observation
- **Area:** simulation / transport
- **Why:** `home_no_road` 1 669 / 39 161 (4.3 %) and `goal_no_road` 4 219 (per candidate per search) in `S01_07_01`'s run; `home_no_road` 3 270 / 17 059 (19 %) and `goal_no_road` 7 585 in `single36`. The owner's rule (OQ-103) says a sender needs a road to a market; the stand-in enforces nothing of the sort (a cart that leaves the home footprint tile goes straight to the first road in the area, which may be nowhere). The counts show the senders in the shipped scenarios are mostly road-connected, so a strict "road-at-the-entrance" rule from T30 step 4 would narrow unreached, not widen it, if it comes with the original's road-reaching rule.
- **How to resolve:** T30 steps 2 and 7 (the engine's rule and the owner's three clauses); T33 inherits.


## OQ-175: What the layer-table reader's vtable method decides, and what the secondary table (`[this + 0x434]`) is (new, T30 2026-10-24; OQ-160)
- **Status:** open, RE / the next address of T30 step 2
- **Area:** simulation / transport, pathing
- **What we know (D, `docs/re/transport.md` §3.3):** the reader `0x1000daf0` builds a 16-byte stack buffer of 4 dwords from two tables (`[this + 0x430]` primary, `[this + 0x434]` secondary; indexed by two u16s `p[0]` and `p[2]` of the cell, read 8 bytes apart on each table) and calls the vtable method `[edx + 0xec]` on object `[this + 0x858]` with five arguments, `([this + 0x85c], 0, &buffer, cell + 4, 8)` (corrected by the orchestrator 2026-10-24 aa: the first version listed four and `this + 0x85c`). The function returns `ret 4`, so it has one argument (the cell pointer); it does not itself decide walkability.
- **The question:**
  - who writes the pointer at `[this + 0x434]`, and what does it point to (another walking-layer, a shot-mask table, or a different structure)? `xref.py` finds no write to `[reg + 0x434]` that fits this object type.
  - what does the vtable method `[edx + 0xec]` do with the two 8-byte spans from each table — compare them, mask them, bool-combine them?
- **How to resolve:** disassemble each vtable at `[reg + 0x858]`'s class whose +0xec is unique; the engine at world `+0x6aa8` (vtable `0x100c1424` / its +4 vtable `0x100c14e4`) is one candidate; the other is the twin engine (vtable `0x100c1a3c`, used by `0x1000ed40` / `0x1000f010`'s `[+0x1030]`). Also read who constructs an object with `[+0x858]` set — `0x1000ebb0` installs the primary / secondary vtables but reads `[eax+0x858]` from an argument on the stack, so the caller supplies that engine. Then compare the found method against the mode-0 cell builder (`0x1008ff70`) to see how `p[0]` (mapped mask) and `p[2]` (the byte) index the layer blocks together.
- **Update 2026-10-24 (T30 round y, `docs/re/transport.md` §3.6):** deeper RE after the owner asked for a thorough investigation. (a) The reader is a **family of at least three variants** with different `this` layouts: `0x1000daf0` (vtable `0x100c18b0`, sub-obj at `+0x858`), `0x1000ed40` (`0x100c1a30`, sub-obj at `+0x434`), `0x1000f9d0` (sub-obj at `+0x1030`, +0x40 pair-delta, `·8` index stride). The twin engine (`0x100c1a30`) is **embedded at `+0x23a4` of a larger outer engine** (constructor `0x10012079`), with the sub-object field written as `[outer + 0x27d8] = arg`. (b) The u16 table `0x100c1af8` has **33 distinct outputs over 512 inputs** (counted); the outputs are **byte offsets into the 0x1c00 layer block, not aligned to 8 or 16 bytes** (`0x6c`, `0x82`, `0xee` are odd offsets). `0xfff` dominates with 123 of 512 masks mapping to it, likely "blocked / default". (c) The layer block is addressed by that mapped **cell-type offset**, not by raw mask or by uniform entry size; real data density is ≈ 33 × 8 = 264 bytes of 8-byte spans per 7168-byte block. (d) The second u16 `p[2]` is the byte at bits 8–15 of the tile record's second dword (the neighbourhood or orientation code), handed as a parallel index into a secondary table. **Still open:** the body of `[edx + 0xec]` (the yes/no), and no direct write to `[reg + 0x858]` was found in the sweep (likely set by a later init call on the engine, not its constructor); the sub-object's class is still open. Of the 76 `call [reg + 0xec]` sites in `AnnoWelt.dll`, four match this family's push-pattern (`0x1000db4a`, `0x1000ed9a`, `0x1000fa33`, `0x1000faed`); the others look like unrelated virtuals.

**T30b.2 update (2026-10-24).** Constructor tracing: xref.py displacement `0x858` finds exactly one hit — `1000db27` (`mov ecx, [eax + 0x858]` in `0x1000daf0`), a read only, no writes. An E8 opcode sweep finds exactly one direct caller of the constructor: `0x100aabfd: call 0x1000eb40` (the real start; `0x1000ebb0` is mid-instruction). The constructor installs vtable `0x100c18b4` at `[this + 0]`, vtable `0x100c18b0` at `[this + 4]`, stores `[this + 0x85c] = eax` (caller stack value at `[esp + 0x14]`, beyond the 8 `ret` bytes), and stores `[this + 0x14] = edx` — it does **not** write `[this + 0x858]`. Vtable check: `0x100c18b0` slot +0 = `0x1000daf0` (the reader), `0x100c18b4` slot +0 = `0x100b4a20` (not the reader). Since `0x1000daf0` is slot +0 of `0x100c18b0` at offset `+4`, the reader's `this` = `original_this + 4`, so `[reader_this + 0x858] = [original_this + 0x85c]` = the constructor's `eax`, an untraceable caller-stack value statically. The vtable on `[reader_this + 0x858]` is unknown, so the method at `vtable + 0xec` cannot be read. Chain stops at caller `0x100aabfd` → constructor `0x1000eb40`. Next address: the value at `[original_this + 0x85c]` at runtime (Wine memory read, T30b.4). Only one caller exists, so there are no further callers to try.

## OQ-176: What `eax` holds at constructor `0x1000eb40` (`0x1000eb91`) (new, T30b.2)
- **Status:** open, RE
- **Area:** simulation / transport, pathing
- **The question:** at `0x1000eb91` the constructor does `mov eax, [esp + 0x14]`, which reads beyond its 8 `ret` argument bytes into the caller's stack frame at `0x100aabfd`. This `eax` is stored at `[original_this + 0x85c]` = `[reader_this + 0x858]` and is therefore the sub-object pointer for the yes/no vtable call. Its value cannot be determined statically. Dynamic analysis (Wine memory read at the `0x100aabfd` call context, or memory-compare of the object after construction in T30b.4) would resolve it.

## OQ-177: The vtable on `[reader_this + 0x858]` and the yes/no method at `vtable + 0xec` (new, T30b.2)
- **Status:** open, RE
- **Area:** simulation / transport, pathing
- **The question:** once `eax_value` (= `[reader_this + 0x858]`) is known from OQ-176, what is the vtable at `[eax_value]`, and what method does `vtable + 0xec` point to? That method is the yes/no walkability decision (OQ-175). Candidate vtables to check: the twin engine's vtable `0x100c1a3c` and the world engine's `0x100c1424`/`0x100c14e4`, but the correct one depends on `eax_value`.

## OQ-178: Whether readers 2 and 3 are separate classes or `this` adjustments (new, T30b.2)
- **Status:** open, RE
- **Area:** simulation / transport, pathing
- **The question:** readers 2 (`0x1000ed40`, sub-object at `+0x434`, sub-arg at `+0x438`) and 3 (`0x1000f9d0`, sub-object at `+0x1030`, sub-args at `+0x2394`/`+0x2398`, `·8` stride, `+0x40` pair delta) have different argument patterns and `this` offsets from reader 1 (`+0x858`/`+0x85c`). Are they genuinely different classes with different vtables, or `this` adjustments of the same class? The shared `call [reg + 0xec]` pattern suggests a common base class; the vtable at `+0xec` on each sub-object's class determines whether the yes/no method is shared.

## OQ-170: Where the transport walk could learn water, river and mountain from (new, T27 2026-10-23; OQ-160, O-53)
- **Status:** open, RE / design
- **Area:** simulation / transport, terrain
- **Why:** the T27 stand-in walk rule (OQ-160) classifies a tile only by its object type's walk class (road, building, or nothing). River pieces are tile objects in the state (`FLUESSE`, OQ-160 "Terrain") but their types have no list and no road flag, so they count as open land and a carrier crosses them; open sea has no object at all, so the state cannot tell it from land. How often this changes a trip in the shipped scenarios was not measured.
- **The candidates for the information, in order of fidelity:**
  1. The original's own answer: the walking-layer cells (OQ-160): `table[mask] | byte << 16` per tile, looked up in the layer's block. How water, shore and mountain tiles come out there is **open** until the reader `0x1000daf0` is read (T30 steps 2 and 5).
  2. For rivers, the state already has the tile objects (`FLUESSE`, with the type's footprint); which of a piece's footprint cells block which layer is the original's rule (T30 step 5). For the sea and other terrain: the island tile record's terrain bits (`docs/re/islands.md`, the tile record's second dword), mapped to a sim terrain class.
  3. A derived per-tile terrain class in the sim state built at load from the island's `gfx`/`height` (a new field → a `kStateHashVersion` bump; only if 1 and 2 cannot be read).
- **What the rebuild does meanwhile:** nothing — the walk ignores terrain (stated in OQ-160); by the owner's decision of 2026-10-24 no stand-in rule is added; T33 implements what T30 reads.
- **How to resolve:** T30 steps 2 and 5, then T33; decide then whether a sim terrain field is still needed for the sea.
