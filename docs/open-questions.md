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
- **Status:** open
- **Area:** formats / figures
- **Question:** What is the exact layout of `FigurenHeader.dat` (ENTRY header, key-value records, per-sprite header), and are the first 4 bytes of a sprite header really big-endian?
- **What we know:** Does not parse with the generic chunk parser; ENTRY contains key-value records (12-byte key + u32) with a differing header, keys include `VERSION2`, `HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`, `PATH`, `COLORS`, `HAEUSER` (GFA §3, PLAN M1.3). Community: sprite header holds height, width, palette id, tag, midpoint, offset, length; "first 4 bytes big-endian" (CL §3, no primary URL recorded). Amsel spec would cover this but is behind a GitLab login (CL §1).
- **How to resolve:** Hex-inspect the file against the community field list; test both endiannesses for plausibility (offsets/lengths within `Figuren*.dat` sizes); obtain Amsel docs bundle (AnnoZone file 9287); locate the reader in Ghidra.

## OQ-003: RLE format of `Figuren*.dat`
- **Status:** open
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
- **Status:** open
- **Area:** RE / architecture
- **Question:** Is the `AnnoGame` server (`CreateAnnoServerInstance`) plus `AnnoFrame` NetClient/NetServer path also used in single-player, i.e. do player actions always travel as messages to a server?
- **What we know:** `AnnoGame.dll` exports `CreateAnnoServerInstance`, imports only KERNEL32; `AnnoFrame.dll` exports NetClient/NetServer and imports WS2_32 (GFA §1). Explicitly flagged as "to be checked" (GFA §1).
- **How to resolve:** Ghidra: call graph from `CreateAnnoNetClientInstance` / `CreateAnnoServerInstance` in a single-player start path (PLAN §7 item 4); map `iAnnoNetClient`/`iAnnoNetServer` vtables (`docs/re/interfaces.md`). If yes, the message catalogue gives the command set for `crates/sim`.

## OQ-006: Simulation tick rate of the original
- **Status:** open
- **Area:** RE / sim / net
- **Question:** At what fixed rate (if any) does the original simulation advance, and is it decoupled from frame rate?
- **What we know:** PLAN M4 leaves the tick rate "e.g. 10–20 Hz, to be decided from M2 findings". Contemporary reports say <15 fps caused more sync errors (CL §4), suggesting coupling to frame rate — not proven.
- **How to resolve:** Locate the tick/update loop in `AnnoWelt.dll`/`AnnoFrame.dll` (PLAN M2); look for timer calls (`GetTickCount`/`timeGetTime`) and divisors.

## OQ-007: x87 floating point in simulation formulas
- **Status:** open
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
- **What we know:** all from corpus statistics (P), `crates/formats/tests/tex_corpus.rs`; layout in `crates/formats/src/tex.rs`.
- **How to resolve:** render textures and compare with in-game screenshots (anchor, key); find the texture loader in `AnnoDisplay.dll`/`View.dll` in Ghidra.

## OQ-022: Constant fields and special entries in `Anlagen.dat` `COLORS`
- **Status:** open
- **Area:** formats / palettes
- **Question:** (a) What is the `u32` at `ENTRY` 0x000 (0 in all 440, like the `.tex` `COLOR` prefix flag)? (b) What is the `u16` 0 between each 4-byte tag (`BODY`, `NAME`) and its `u16` length — the high half of a `u32` length, or a separate field? (c) Do palette entry 0 (a grey in ≥ 439 of 440 tables) and entry 1 (magenta `ff00ff` in 287) have a fixed role, e.g. shadow/transparency key? (d) Which records refer to a colour table `id` (`ANLAGEN`, `TEXTURES`, `Figuren*`)?
- **What we know:** layout and counts from all 440 entries (P), `docs/formats/palette.md`, `crates/formats/tests/colors_corpus.rs`. The same tag + `u16` + `u16` record shape may occur in `ANLAGEN` or `TEXTURES`; not yet probed.
- **How to resolve:** probe `ANNO/ANLAGEN/ENTRY` and `ANNO/TEXTURES` for the same record shape and for `u16`/`u32` values in 1…440; find the colour-table loader in Ghidra (strings `BODY`, `NAME`, `Farbtabellen`).
