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
- **Implementation status (stage S7):** the 300 s event, its one draw per island per gate and the faithful centre (the first building of the market list of the first city that has one, floored to the chunk edge) are implemented (`start_drought`, `scheduler.cpp`). The option is a **reserved value**: `Rules::validate` rejects `drought_random_centre == true` ("reserved: not implemented (OQ-042)", like `prng_mode == PerIsland`), because the spec does not say what a random centre is (which tiles, how many draws, which draw site, whether it must be on a city's land), and every answer changes the PRNG timeline. An accepted option that did nothing would let a host or a peer believe it changed the game (S7 review round). The owner decides before it is implemented; the rejection is lifted then. The drought machine itself (growth of the area, the plateau, the retreat) is stage S13; a drought the event starts keeps its start record until then. With more than 32 city slots no drought is started at all (spec O-55: the area's city mask is a `u32`).

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

Source: `docs/render/rivers.md` §12 (rows D1..D16) and the hydraulics stage S4 (2026-10-19). Rule (owner): a deviation is **fix** when it is obviously unintended (removed, documented in `rivers.md`); **decision-needed** when it could change the look or feel: the default stated in `rivers.md` §12 is implemented behind an option and listed here. Every river speed is a model output ("plausible, not original"): the original data carries no discharge, no slope and no flow direction (`rivers.md` 1.2).

## OQ-065: River speed scale: reference speed, soft floor, cap, bank profile (rivers.md D1)
- **Status:** open, owner decides (no visible effect until the shading stage S5)
- **Area:** render / rivers / flow
- **What:** the flow field is a model with tuning values `RiverConfig::out_speed` 0.9 tiles/s (0.63 m/s), `speed_floor` 0.12, `speed_cap` 3.0, `fall_speed` 2.5, bank roughness `bank_roughness` 4 and `bank_width_tiles` 0.30 (the margin runs at 0.2 of the core), bed smoothing `bed_sigma_tiles` 0.75, depth range 1.5-12 units. Measured on the 106 islands of the corpus (`river_qa --flow`): the median speed of an island is 0.12-0.48 (typically 0.21; the floor is 0.12, because most of a network is tributary where the speed grows with the area upstream), the 95th percentile 0.19-2.5 (typically 0.94), the falls 2.5, the mean 0.14-0.69.
- **What differs from rivers.md:** the Normalised discharge sets `out_speed` at the fastest cross-section that still passes half of the river's discharge (`RiverConfig::out_flux_share`), not at the sea faces: the original's mouth sprites are broad blobs (up to 15 tiles of sea face against a river 1.4 tiles wide), and measuring there made the river proper 2-3 times too fast and pinned half of all cells on the cap (first measurement, same island: median speed 2.8 tiles/s). A straight channel still has `out_speed` at its outlet.
- **Consequence of the default reference (review S4):** only the fastest cross-section that carries at least half of the discharge gets `out_speed`; everything upstream scales down with the area drained, so most of a network sits at or near the floor. Over the 106 islands the floor sets the speed of 17-90 % of the cells of an island (median island 38 %, 42 % of all wet cells; 23 islands above 50 %; `D_256x256_0045_5B`: 73.5 %, its median is exactly 0.12), the cap of at most 0.4 %. On a 20 px tile (zoom group 2) 0.12-0.33 tiles/s is 2.4-6.6 px/s (5-13 px/s at group 1): slow flow that compresses the contrast between narrows and wide shallows. Any pinch that passes half of the discharge and is at least 4 cells wide also sets the scale (rivers.md 14.2).
- **Option implemented:** `RiverConfig::reference = Reference::MedianCell` (viewer option follows with S8; `river_qa --flow --reference median [--median-speed T]`): the median cell of every river gets `median_speed` (0.4 tiles/s). Over the same 106 islands: the median island speed 0.40-0.66 (typically 0.43; 0.12-0.50 with the default), the 95th percentile 1.2-3.0 (median 1.8; 1.1), floor 15-41 % of the cells (median 20 %), cap up to 10.5 % (median 0.4 %; 7 islands above 5 %). Side-by-side contact sheets of the default and this option, same islands: `C:\Users\altin\Pictures\anno-rivers\S4\after_review` and `option_median_reference` (not in git); the S5/S6 sheets show both on the drawn river before the owner picks.
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

## OQ-069: Ponds and lakes (rivers.md D7)
- **Status:** open, owner decides
- **Area:** render / rivers / flow
- **What:** water bodies of fewer than 12 cells (1/6 tile^2) are not solved: a static eddy of `pond_swirl` 0.03 tiles/s in a direction from smooth noise, so a pond is not frozen. Larger bodies are solved like rivers (a lake with a pseudo outlet).
- **Why it may change the feel:** tiny puddles move slightly; set 0 for still water.
- **Faithful default implemented:** swirl on, 0.03 tiles/s, threshold `pond_cells` 12.
- **Rules option:** `RiverConfig::pond_swirl`, `pond_cells`.

## OQ-070: Mouth-sprite basins in the flow raster (review S4)
- **Status:** open, owner decides (no visible effect until the shading stage S5)
- **Area:** render / rivers / flow
- **What:** the original's river-mouth sprites are broad blobs (up to about 6 x 3 tiles; roughly 10 % of the wet cells of `N_256x256_0002_2B`), and most of that area lies behind cliffs and forest in the render, so it is never visible. The whole blob is a wet flow domain: it runs at fall speed where the surface slope calls for it (the level falls steeply over it), it counts in the discharge bands, and five islands have more than 5 % of their cells at 2.35-2.5 tiles/s only because of it. The two lobes of a mouth fan point at each other on the short sea-contact stretch.
- **Why it may change the feel:** fast, converging flow at every mouth, and the look of the mouth fan in the sea blend.
- **Faithful default implemented:** the blob is part of the domain, as drawn by the shipped enhanced water (the gap fix and the mouth look are unchanged); `transition_passes` softens the speed across it (fix).
- **Rules option:** none yet; candidates: limit the mouth-fan speed to the river speed, or drop wet cells that the depth test hides from the flow domain (raster only, nothing drawn changes). Decide with the S5/S6 sheets of `D_256x256_0045_5B` and the `Single_01` mouths in front of you.
