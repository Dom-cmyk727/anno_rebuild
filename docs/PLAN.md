# ANNO 1503 Rebuild — Implementation Plan

Status legend: `[ ]` open · `[~]` in progress · `[x]` done. Update this file as work lands.

## 0. Starting point (verified 2026-10-16)

- Source material: GOG "Anno 1503 A.D." 2.0.0.5, extracted with innoextract to `game/app/` (6,040 files, ~1.5 GB). Königsedition-level build; all game DLLs dated 2006-07-21, 32-bit, MSVC 6 (see `docs/research/game-files-analysis.md`).
- Original architecture: COM-like modules behind `Create…Instance` exports. Likely simulation core: `AnnoWelt.dll` (967 KB), `AnnoGame.dll` (server), `AnnoScene.dll` (scenario/savegame). Rendering: `AnnoDisplay.dll` (DirectDraw). Networking: `AnnoFrame.dll`, custom UDP over Winsock (no DirectPlay), with an "Async aufgetreten" desync path. RTTI class names survive in AnnoFrame, View, AnnoDisplay (`analysis/pe_symbols.json`).
- Formats: one generic chunk container (12-byte name + u32 LE size + payload, nested) covers `.sze`, `.scp` (root only), `.tex`, `.inc`, `.zei`, `Anlagen.dat`. `tools/chunks.py` parses these and is the reference. `FigurenHeader.dat` uses key-value records inside ENTRY and needs its own reader. `Figuren*.dat` are raw RLE sprite data indexed by the header. `Data/*.dat`, `Texte.dat`, `BGruppen.dat` are plain text.
- Community: no usable open-source reimplementation exists. Most useful prior work: Amsel (millimarg, XML format specs, currently behind GitLab login; docs bundle on AnnoZone file 9287), DatEditor/Muto1503 (Azukee, `Anlagen.dat` parser + MinHook hooks, archived at Software Heritage). See `docs/research/community-landscape.md`.
- Decisions: C++20 since 2026-10-17 (`docs/adr/0001-cpp20.md`, superseding the start in Rust, ADR 0000; the Rust code is the reference until the port reaches parity); targets Windows + Linux x86_64; first visible milestone is formats + asset viewer; English docs; faithful core separated from QoL.

## 0.1 Design goals: no hardcoded limits, scalability, moddability (owner decision 2026-10-16)

The engine must not hardcode limits. Scalability and moddability are explicit goals, alongside faithfulness.

- **Original limits are data, not code.** Counts and sizes from the original are defaults in data or config, never array sizes, fixed-width ids or `const` limits in the engine. Examples: number of building/figure types, island and map size, player count, goods types, zoom levels, rotations, animation frames, sea level, colour tables. The faithful ruleset reproduces the original values; mods and the QoL `rules` config may raise them.
- **Formats vs. engine.** `crates/formats` keeps mirroring the original files exactly (a 12-bit `ANLAGEN` id in an island item, `u16` sizes, three zoom groups in the volumes); these are facts about the files. Engine-side types (`sim`, `render`, `assets`) must not inherit them as limits: widen ids and sizes when data enters the engine, and look up content by id/name in tables rather than by fixed indices.
- **Content is loaded, not compiled in.** Buildings, figures, goods, production chains, textures and scenarios come from data the engine reads (original files first, mod overlays later) through one content registry. Special entries the engine needs (e.g. the sea sprite, `ANLAGEN` 2801) are referenced by configurable ids or names with the original values as defaults.
- **Scales with content and map size.** No quadratic or fixed-capacity structures over all objects; spatial data in chunks; rendering and simulation cost proportional to what is visible or active; large maps, many islands and many players are a target, not an afterthought.
- **Mod support is part of the architecture.** Plan for a mod format (overlay of original data plus new assets) and a load order; keep determinism. In multiplayer every peer must have identical content (content hash in the lockstep handshake, M4).
- **Existing code to revisit** (written before this goal was set): `anno_render::iso::{TILE, SEA_ANLAGE, SEA_LEVEL, HEIGHT_LIFT}` and `scene::MARGIN` are constants; three zoom groups and four rotations are assumed in places. Move them into a render/content config with the original values as defaults when the content registry exists (M1.6/M3).
- **Review rule:** every new `const` limit, fixed-size array or narrow id type in `sim`, `render`, `assets`, `net` or `ui` needs a reason in a comment and a path to make it configurable; prefer data-driven values.

## 0.2 Design goal: improve, not only rebuild (owner decision 2026-10-17)

The project rebuilds ANNO 1503 faithfully **and** improves the engine beyond the original. Improvements are first-class goals, not afterthoughts.

- **Visual target set by the owner:** water that looks smoother and livelier than the original, with **crashing waves on the open sea and along coastlines**. The original has no breakers (confirmed in a gameplay video, `docs/render/iso.md` "Reference"); the current `WaterStyle::Enhanced` (breakers rolling onto beaches, open-water swell) is the first step and is preferred by the owner over the original look. 2026-10-17: breaking waves added (shoaling, collapse into whitewater, foam burst on impact, backwash; whitecaps on open-water swell crests). Next: spray at impact, rocks and ships, smoother open water, tuning against the colleague's viewer recording.
- **How improvements are built:** as separate, selectable layers next to the faithful one (render styles, `rules` config), so the original behaviour stays reproducible and testable (CLAUDE.md "faithful core separate from improvements"). Which variant is the default is a per-feature owner decision; for water the owner leans towards the enhanced look.
- **Faithful mode stays faithful (owner decision 2026-10-17):** improvements such as hiding the sea floor in deep water apply only to the enhanced style; the original mode keeps the original look even where it shows the original's tile-shaped floor textures.
- **Evidence still applies to the faithful parts;** improvements are documented as such (what, why, tuning), with their own tests.

## 0.3 Design goal: smooth, measured performance (owner decision 2026-10-17)

Smooth gameplay at a high frame rate that scales with map size, player count and multiplayer is a requirement, and it is proven by measurement, not assumed.

- **Targets (first draft, to be refined):** the frame time of the view stays under 16.7 ms (60 fps) at p99 at 1920 × 1080 and under 16.7 ms at p95 at 3840 × 2160 on a 4-core x86 desktop, in every zoom group and water style; no frame above 50 ms while panning; the simulation tick (M3) costs a fixed, measured share of the frame budget for the largest scenario with 8 players.
- **Measured, reproducibly:** every performance-relevant subsystem gets a headless benchmark that runs the production code path and reports percentiles (p50/p95/p99/max) and per-stage times. First one: `crates/render/examples/bench_frame.rs` (view frames along scripted camera paths, configurable resolution, threads, zoom, water style, chunk mode). Method and results: [`docs/perf/README.md`](perf/README.md). The viewer shows fps in its title and prints the same stage breakdown.
- **Built for modern multi-core CPUs and GPUs (owner decision 2026-10-17):** per-pixel and per-sprite work runs on the GPU (`anno_render::gpu`, wgpu: Vulkan, DirectX 12, Metal); content building, decoding and (later) simulation preparation run on all CPU cores without shared locks on hot paths (read-mostly caches behind read-write locks or per-thread data, work queues with priorities instead of FIFOs); the window thread never waits for content. The CPU compositor stays as reference and fallback, and every GPU stage is checked against it (`bench_frame --check`).
- **Rendering never blocks on content:** static content is built in the background and cached; a frame only composes and animates.
- **Simulation scales independently of rendering:** fixed tick decoupled from frame rate (M4), cost proportional to active entities, measured per tick with the same percentile reporting, including N headless peers (M4 test harness).
- **Robustness:** benchmarks and their result tables are kept in `docs/perf/`; a regression of more than 20 % at p95 needs a reason in the commit.

## 0.4 Design goal: asset fidelity, nothing lost (owner decision 2026-10-17)

Every graphic and design asset of the original must come through the rebuild complete and unchanged, down to the smallest details (feet, armour, weapons, flags, shadows, animation frames).

- **Decode, never redraw:** assets are decoded from the original files at runtime; nothing is approximated by hand. Decoders are byte-exact (round-trip tests over the whole corpus) and their pixel output is verified.
- **Verify every asset, twice:** (1) per frame: decoded pixel count = covered pixels in the RLE stream, no truncation/clipping (frames that overrun their width, e.g. 51 + 398 `Anlagen` frames, are drawn unclipped as the original), every palette index mapped, every frame of every rotation and animation present; (2) visually and by pixel diff against reference captures of the original game (History Edition / GOG under Wine) for a stratified sample of every asset class — buildings, figures (soldiers with weapons and armour, civilians, animals), ships, UI, effects — with a written checklist of small details per class. Status 2026-10-17: (1) done for every `Anlagen.dat` and figure frame at all zoom groups, pixel-exact through the renderer (`crates/render/tests/sprite_fidelity.rs`); (2) open.
- **Recorded, not trusted:** results go into `docs/assets/verification.md` (asset class, count, method, deviations); any deviation is a bug or an open question, never silently accepted. Improvements (e.g. enhanced water) are layers on top of the faithful asset, never edits to it.

## 0.5 Known issues of the original (2026-10-17)

[`docs/research/original-issues.md`](research/original-issues.md) lists the bugs, crashes, desync causes and design flaws of ANNO 1503 reported in patch notes and by the community (ISS-001…, each with sources and a rebuild implication: FIX, RULE, FAITHFUL or N/A). Every milestone that touches an affected system checks the list and records how each issue is handled; the multiplayer findings (desyncs from complexity and OS differences, fire spread) feed directly into M3/M4.

## 1. Milestone M0 — Repository and tooling skeleton

Goal: a workspace that builds on all targets with CI, before any game code.

- [x] `cargo new --lib` for each crate in the layout from `CLAUDE.md`; workspace `Cargo.toml` with shared lints (`#![deny(unsafe_code)]` in `sim`, `net`, `formats`).
- [x] CI (GitHub Actions): build + test matrix `ubuntu-latest`, `windows-latest`; `macos-latest` build-only. `cargo clippy -D warnings`, `cargo fmt --check`.
- [x] `crates/tools`: `anno-dump` CLI skeleton (`anno-dump chunks <file>` printing the chunk tree, matching `tools/chunks.py` output). Add a test that both agree on 10 sample files.
- [x] `docs/glossary.md`, `docs/open-questions.md`, `docs/formats/README.md` (template per format: magic, layout table, evidence, open fields).
- [x] `fixtures/` with a small, hand-picked set of original files for tests **is not allowed** (redistribution). Instead: tests locate `game/app` via `ANNO1503_GAME_DIR` env var and skip with a clear message when absent. Document in CLAUDE.md.

Status M0 (2026-10-16): `[x]` done. First CI run (GitHub Actions run 38924950460) green on all jobs: lint, test ubuntu/windows, build macOS.
- Workspace: 8 crates (`anno-formats`, `anno-assets`, `anno-sim`, `anno-net`, `anno-render`, `anno-ui`, `anno-tools`, binary `anno1503`); edition 2024; shared `[workspace.lints]`; `#![deny(unsafe_code)]` in formats/sim/net. No licence chosen yet (open decision for the owner).
- `anno_formats::chunk` reader (zero-copy, mirrors `tools/chunks.py` incl. its name pattern and depth-6 heuristic, raw 12-byte name field kept for the M1.1 writer). Writer/round-trip remain M1.1.
- `tools/chunks.py tree` and `anno-dump chunks` print the same tree format; `crates/tools/tests/agree_python.rs` diffs them on 10 sample files.
- `crates/formats/tests/corpus.rs`: all 50 `.sze`, 1214 `.tex`, 1213 `.inc`, 11 `.zei`, `Anlagen.dat` parse exactly; 465 `.scp` parse root `ANNO`, 444 with trailing bytes. Passing locally with `ANNO1503_GAME_DIR=game/app`.
- `cargo check` passes for `x86_64-pc-windows-msvc` and `x86_64-unknown-linux-gnu` from macOS; clippy `-D warnings` and fmt clean.
- Repo: https://github.com/Dom-cmyk727/anno_rebuild (public), branch `main`.

Acceptance: `cargo test --workspace` green on CI without the game directory; locally with `ANNO1503_GAME_DIR` set, the chunk corpus test passes on all `.sze/.tex/.inc/.zei` and `Anlagen.dat`.

## 2. Milestone M1 — Formats and asset viewer

Goal: render an original island/scenario from the original files on screen. This proves the asset pipeline and gives everything else a visual check.

### M1.1 Chunk container (`crates/formats::chunk`)
- [x] Port `tools/chunks.py` semantics: lenient 12-byte name (NUL-terminated, trailing garbage allowed), u32 LE size, recursive container detection, exact-fit rule.
- [x] Zero-copy reader (`&[u8]` slices), plus a writer. Round-trip test: parse → write → byte-identical for every file in the corpus.
- [x] Handle the `.scp` trailing bytes: parse root chunk, expose the remainder as `trailing: &[u8]`; open question logged.
- [x] Spec: `docs/formats/chunk-container.md`.

Status M1.1 (2026-10-16): done. All 2,954 container files round-trip byte-identically (`chunk_corpus`). Findings: `.scp` trailer is 60 bytes in exactly the `INSEL5` islands (OQ-001); 22 name fields carry post-NUL bytes (OQ-008); `INSEL5` has a 12-byte prefix before its sub-chunks, so generic detection treats it as a leaf (OQ-013, to solve in M1.5).

### M1.2 Palettes and textures (`.tex`, `.inc`, `Anlagen.dat/COLORS`)
- [x] Decode `COLOR` (palette) and `TEXTURE` chunks in `.tex`; `ACCESS` in `.inc`. Determine pixel format from `Textures/debug.tex` and `ToolGfx/Felder.tex` (32×32 ground tiles per drkohler). Evidence: compare against drkohler's GFX Extractor output if obtainable; otherwise validate by rendering and eyeballing known UI textures.
- [x] Export to PNG via `anno-extract tex <file> <outdir>` for visual verification.
- [x] Spec: `docs/formats/tex.md`, `docs/formats/palette.md`.

Status M1.2 (2026-10-16): `[x]` decoders, specs, reference dump, PNG export, visual check and byte-identical RLE round trip done. Remaining unknowns (OQ-021, OQ-022) do not block decoding and move on with M1.3/M2.
- Done: `anno_formats::tex` (palettes, `TEXTURE` HEAD/BODY/NAME, RLE + raw decode) and `anno_formats::inc` (`ACCESS` records). `tex_corpus`/`inc_corpus` pass on all 1,214 `.tex` and 1,213 `.inc`: 1,427 palettes, 5,176 textures (5,028 `BSH_RGB` RLE, 1 `BSH_RGBH` RLE, 147 `TEX_RGB` raw ground tiles), every `HEAD.palette_id` resolves, every texture decodes to exactly `width × height`, every `.inc` number is a `TEXTURE` id in the matching `.tex`.
- Evidence summary: `COLOR`/`TEXTURE` payload = 8-byte prefix (`u32`, `u32 id`) + sub-chunks (so generic detection treats them as leaves, OQ-013). Palette = 256 × `[B, G, R, 0]`; byte order from sea tiles (`Meer*.bmp` in `Felder.tex` average B=122 G=77 R=33). RLE: per row `(skip, count, count × index)` runs, `0xFE` end of row, `0xFF` end of image.
- Specs written: `docs/formats/tex.md` (incl. the `anno-dump tex/inc` reference format), `docs/formats/palette.md`.
- Reference dump: `anno-dump tex [--full]` / `anno-dump inc` and the independent Python reader `tools/tex.py` print the text format specified in `tex.md`; `agree_python.rs` compares them on synthetic files (runs on CI) and on every corpus `.tex`/`.inc` (`anno_dump_matches_tex_py_corpus`, needs `ANNO1503_GAME_DIR`). `COLOR/BODY` must be exactly 1,024 bytes.
- PNG export: `anno-extract tex <file.tex> <outdir>` (RGBA via `anno_assets::texture::to_rgba`, `index.txt` per directory); `extract.rs` checks PNGs decode back exactly.
- `Anlagen.dat/COLORS`: 440 colour tables decoded (`anno_formats::colors`, `anno-dump colors`, `tools/tex.py colors`); layout from `anno-dump probe` + two read-only scans on the corpus, recorded in `palette.md`. Unknowns: OQ-022.
- Corpus run 2026-10-16 on the dev Mac (`cargo test --workspace --no-fail-fast` with `ANNO1503_GAME_DIR`): all corpus and agreement tests pass, incl. Rust = Python on every `.tex`, `.inc` and `Anlagen.dat` `COLORS`, and PNG round trip on the sample files. First run failed `colors_corpus` on the flag of table 109 (= 4), now recorded.
- Visual check 2026-10-16 on the dev Mac (game files present): `Felder.tex` sea tiles (`Meer01.bmp`) render blue, grass green, sand/sea transitions correct; `fenster.tex` UI (1024×768 menu backdrop, minimap frame, building icons) renders coherently with no row shift; the minimap frame's centre is alpha 0 (RLE gap). Confirms BGR palette order and the RLE grammar. `ToolGfx/Activate your ship.tex` exports nothing because it is an empty 16-byte container (one of the 338 empty files), not a decode failure.
- RLE round trip: `anno_formats::tex::encode_rle` and `tools/tex.py encode_rle` reproduce all 5,029 RLE bodies byte for byte; encoder rules in `tex.md`. `.tex` files are now covered byte-identically at the container, `HEAD` and pixel level.
- Open (non-blocking): unknown fields in OQ-021/OQ-022.

### M1.3 Figures / sprites (`FigurenHeader.dat`, `Figuren000..200.dat`)
- [x] Reader for `FigurenHeader.dat`: ENTRY records with 12-byte keys + u32 (`VERSION2`, `HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`, `PATH`, `COLORS`, `HAEUSER`, …). Per community notes each sprite header holds height, width, local palette id, tag, midpoint, data offset and length; first four bytes big-endian — **verify** against the file before trusting.
- [x] RLE decoder for `Figuren*.dat` sprite data. Hypothesis from mdcii/anno16_docs: 8-bit indexed, RLE with transparency runs, external palette. Confirm shape by decoding a known sprite (e.g. figure id 203 = carrier per AnnoZone) and rendering it.
- [x] Shadow/alpha maps ("BSH_ALPHA" greyscale per community) — decode once base sprites work.
- [x] `anno-extract figuren` dumps all sprites as PNG with an index JSON (id, size, midpoint, palette).
- [~] Spec: `docs/formats/figuren.md`. Cross-check with Amsel's spec once obtained (contact millimarg — see §7).

Status M1.3 (2026-10-16): `[~]` reader, decoder, shadows, reference dump, PNG export and spec done; record semantics and the Amsel cross-check open.
- Index: `anno_formats::figuren` reads `FigurenHeader.dat` completely — 368 figure types (`ENTRY`: 8-byte prefix + chunk records), 0–3 colour tables each (`Anlagen.dat` table layout), 1,288 sprite sets in `HAEUSER` (1,047 with `SHADOW`), 731,121 frame headers (36 bytes: size, colour table, format, anchor, volume offset/length) in three zoom groups of 243,707. Everything little-endian; the community "big-endian" claim does not hold (OQ-002 resolved). Byte-identical round trip of the whole file.
- Volumes: `Figuren<g><nn>.dat` concatenated per zoom group; frame ranges tile each group exactly. The root `ANNO` size counts header file + all volumes; three trailing `TEXTURES` headers hold per-group size and frame count.
- Pixels: `.tex` row RLE with 1 byte (`BSH_RGBV`, `BSH_RGBH`, `BSH_RGBAV`), 2 bytes `[z, index]` (`BSH_RGBZ`, `BSH_RGBAZ`) or 0 bytes (`BSH_ALPHA` shadow masks) per pixel; `A` formats carry a 2-bit level in the count byte (OQ-003 resolved). All 655,842 distinct frame ranges decode.
- Reference dump: `anno-dump figuren [--full]` / `anno-dump frames` = `tools/figuren.py dump/frames` (synthetic on CI, corpus incl. the 743,672-line full dump and one entry per format).
- PNG export: `anno-extract figuren <game_dir> <outdir> <entry id>…` with a text `index.txt` (same convention as `anno-extract tex` instead of JSON). Visual check 2026-10-16: mower (101), sheep (103), galleon (1501, zoom groups 0 and 2), gondola (1510) and shadow masks render correctly. Entry 203 is `BEWOHNTYPE TRAEGER` (carrier, colour table `Träger01.pcx`, 8 sprite sets) and renders as a carrier, matching the AnnoZone id — the community figure ids apply to this build.
- Open (non-blocking for M1.6): meaning of `z`, `level`, `H`, anchors and of the records `VERSION2`, `HEAD`, `LOGIC`, `BODY2`, `ANIM`, `SOUND2`, `GFXDATA` (OQ-023); frame pixel re-encoder; Amsel cross-check (human task, §7).

### M1.4 Buildings (`Anlagen.dat/ANLAGEN`, 921 ENTRY)
Status M1.4 (2026-10-16): `[~]` file structure, graphics and reference dump done; gameplay records open.
- `anno_formats::anlagen` parses `Anlagen.dat` completely: 440 global colour tables, 921 `ANLAGEN` entries (ids 1–3998; figure entry layout; byte-identical re-encoding), 1,031 sprite sets (110 with shadow), three inline pixel volumes with 9,633 frames each that the frame ranges tile exactly. `ANLAGEN` `COLORS` = list of global table ids (OQ-022 d). New frame formats `BSH_RGBWZ`, `BSH_RGBZM` (2 bytes `[z, index]`) and `BSH_RGBAH` (1 byte + level); 51 frames have rows that overrun their width and decode only with clipping (OQ-025).
- `anno-dump anlagen [--full]` = `tools/figuren.py anlagen`; `anno-extract anlagen` renders entries. Visual check: palace, castle, cliff, river piece.
- Open: meaning of `HEAD`, `PRODTYPE`, `PROD3`, `WARE`, `ROHSTOFF`, the masks, `INFRAMIN`, `OBJECTPOS`, `BAUSAMPLE`, `ENTRANCE`, …; join with `Texte.dat` names; how scenario objects reference `ANLAGEN` ids (needed to place buildings in M1.6).
- [~] Decode ENTRY layout (size, footprint, gfx ids, production fields). Sources: Azukee's `AnlagenParser.cs` (class names `TAnlage*`, `THaus*` from IDA) via Software Heritage; `anno-1503-easy-player-boost/build_mod.py` for confirmed field offsets (production interval, coverage radius, `VermehrTab`).
- [ ] Join with `Texte.dat` `[ANLAGEN]` id→name for a readable dump (`anno-dump anlagen`).
- [~] Spec: `docs/formats/anlagen.md`.

### M1.5 Islands and scenarios (`.scp`, `.sze`)
- [~] Island: `INSEL4/5`, `INFO`, `HEIGHT`, `GFXID`, `GELAENDE`, `PROD2`, `FRUCHTBAR`, `BERGWERK2`, `OCCUR`, `ANBAU`, `DISCOVER`, `KONTORID`, `STAEDTE`, `LOCKED`. Start with `HEIGHT`/`GFXID`/`GELAENDE` (grid arrays; dimensions from file name `Po_064x064_…` and `INFO`).
- [~] Scenario: `SCENE/{HEAD, MAIN, GUID, ALIASNAME, WARETYPE, SKTIMER, MONOPOL2, LIMITS, COMPPROFIL2, ROHSTOFF, SCHATZ, ERZE, BIGBROTHER, AUFTRAGTEXT, SPIELER, INSEL5, INSELLNK}`. Root `ANNO` vs `ANNOADDON1` gates add-on content.
- [ ] Versioned chunk names (`INSEL4`→`INSEL5`, `PROD2`, `MONOPOL2`) → loader supports both where both occur in the corpus.

Status M1.5 (2026-10-16): `[~]` islands structurally done; scenario records, plain-text data and record semantics open.
- Islands: `anno_formats::insel` reads all 465 `.scp` templates (444 `INSEL5`, 21 legacy `INSEL4` where the records are root siblings) and the 668 islands in the 50 `.sze`. `INSEL5` head = world position, size, island number. `HEIGHT` (u8) and `GFXID` (u16) are stored in 32 × 32 blocks, row of blocks by row of blocks; assembled grids re-encode byte-identically for every island. `FRUCHTBAR` is a plain `width × height` byte grid. The island size comes from the head; two templates are misnamed (`D_080x080_0022/0023` are 64 × 64). Spec: `docs/formats/insel.md` (replaces the planned `scp.md`).
- Reference dump: `anno-dump insel [--full]` = `tools/insel.py dump` on synthetic files (CI) and all 515 files.
- Visual check: an island's `HEIGHT` grid renders as a coherent island without block seams.
- Ground (2026-10-16): `GFXID = texture << 8 | u << 5 | v << 2 | r` selects a rotated 32 × 32 cell of a 256 × 256 `Textures/Felder.tex` texture; texture 0 = open water (OQ-024 a). Found while starting M1.4: the `Anlagen.dat` mappings were rejected, the bit split against `Felder.tex` ids matched 13.4 M of 13.4 M non-water tiles except one 36-tile patch of the missing texture 205. `anno-extract insel` renders islands top-down; `Single_01.sze`'s main island and a polar template look right. The M1.6 viewer is no longer blocked for terrain; open water needs its own rendering.
- Placed objects (2026-10-16): `GELAENDE` (scenery), `PROD2` (built objects) and `FLUESSE` items place `Anlagen.dat` objects — `u16 id | rotation << 14`, `u16 state`, `u8 x, u8 y`; footprint from `ANLAGEN` `HEAD` bytes 0–1, swapped for odd rotations. `Island::objects`, `AnlagenFile::footprint`; corpus test `objects_corpus`; a top-down overlay shows a walled town with roads on `single32.sze`. With ground + objects the M1.6 viewer has all island content it needs; still missing for drawing are the isometric projection rules (tile size, `HEIGHT` scale, sprite anchors vs. footprints).
- Scenario (2026-10-17): `anno_formats::tagged` reads the record container inside `SPIELER` and `COMPPROFIL2` (6-byte NUL-padded name + `u16` length; long form `0xFF00` + `u16` in every add-on `STADT2`/`PROD`); the `Anlagen.dat` colour tables use the same header (OQ-022 b resolved). `anno_formats::sze` decodes players (two series per scenario), ships/soldiers (`u16 id, u16 figure, u8 owner, u8 attr, u16 0, f32 x, f32 y` + records: 327 ships of figures 1501–1509, 2,317 soldiers) and `INSELLNK` (island head + path of an `INSEL4` dummy template, 229 links in 9 add-on files, six endless games have only links). Corpus test `scene_corpus` re-encodes every level byte-identically; `anno-dump sze [--full]` = `tools/sze.py dump` on all 50 files and synthetic ones. Spec `docs/formats/sze.md`; open: OQ-027 (fields), OQ-028 (what a link resolves to).
- Open: other `SCENE` records (`HEAD`, `MAIN`, …) and the content of player records, plain-text data files, item `state` and duplicate items, the other island records (OQ-024), `.scp` trailer (OQ-001).
- [~] Plain-text data: parsers for `Data/BGruppen.dat` (goods, prices), `Wachstum.dat`, `wegspeed.dat`, `Felder.dat`, `Texte.dat`. These are INI-like with `Key: value` lines and `[SECTION]` headers.
  - 2026-10-17: `anno_formats::text` reads all of them at the line level (Windows-1252, CR LF, `;` comments, `[SECTION]`, `Key: value`, comma fields; byte-identical round trip) and `Texte.dat` by section (118 sections; `[ANLAGEN]` = names of 326 `Anlagen.dat` ids; `[SHIPS]` 107 ship names, `[STAEDTE]` 160 town names). `anno-dump keyed`/`texte` = `tools/text.py`. Record layouts confirmed (`text::split_records`, exact decimals with `text::fixed`): 56 goods with prices, 5 population groups with 47 needs, 31 building stages, 146 growth grids keyed by `Felder.tex` texture id (6 × 6 = the ground cells), 5 walk masks. Spec `docs/formats/data-text.md`. Open: field meanings and one malformed row (OQ-029).
- [~] Specs: `docs/formats/sze.md`, `docs/formats/data-text.md` (both partial; `scp.md` replaced by `insel.md`).

### M1.6 Viewer (`crates/render`, `crates/app`)
Status M1.6 (2026-10-17): `[~]` offline isometric renderer and interactive viewer work, ships included; figures (people, soldiers), player colours and other animations open.
- Endless games (2026-10-17): islands that a scenario only links to (`INSELLNK`, six endless games have nothing else) are drawn as stand-ins — a real template of the linked climate and size, chosen by island number — because the linked dummies are flat grass squares (owner decision; OQ-028). All nine scenarios with links render, 0 objects failed (test `view_png_island_links`).
- Player colours (2026-10-17): traced in `AnnoDisplay.dll`/`AnnoGame.dll` (`docs/re/annodisplay.md` §8, `docs/re/annogame.md`): eight colours, a green-dominance test on every palette colour, brightness-scaled replacement, 69 figure types; implemented bit-exactly (`anno_assets::figure::recolour`, corpus test `player_colours`); soldiers now show their owner's colour (owner `p` → colour `p + 1`, a stand-in). OQ-022 a resolved. New decision: reproduce the original's 16-bit colour (OQ-030)?
- Soldiers and fidelity (2026-10-17): soldiers (`SPIELER/SOLDAT`, 2,317 in the corpus) stand on the terrain at their position with shadows, drawn in their island's object order; `Single_24.sze` draws 531 soldiers and 10 ships. Single-pixel fidelity checked at zoom 0, 1, 2 (`docs/render/iso.md`, "Fidelity"): all 28,518 building and 725,424 figure frames reach the canvas pixel-exact; 1,695 building pixels past the frame width that were clipped are now drawn as the original does; GPU = CPU within 1 per channel. Found: soldiers' player-colour markers (pure green) are not remapped yet (OQ-027 f).
- Ships (2026-10-17): the viewer draws every ship of a scenario (`SPIELER/SCHIFF`, `anno_assets::unit`) at its world position on the sea surface, from its `FigurenHeader.dat` figure (set 0, frame `attr mod 16`); only the frames used are read from the `Figuren*.dat` volumes. All ten `0200Single` scenarios: every ship drawn (31), 0 objects failed. `--png` takes `--at <x>,<y> --size <w>x<h>` for close-ups. Open: whether `attr` is the facing (OQ-027 d), player colours, draw order against island sprites (OQ-026 f), soldiers and other figures.
- Wave simulation (2026-10-17, owner: parametric waves still too uniform): GPU shallow-water simulation with wetting and drying in a camera-local ring-buffer window (cost independent of map size, render-only so lockstep multiplayer is unaffected), CPU reference solver checked against the GPU; simulated run-up, breaking and backwash (`docs/render/iso.md`, "Wave simulation").
- Rivers and waves (2026-10-17, owner review): waterline at `HEIGHT` 16 with a per-pixel depth test against the visible terrain (the sea had covered painted sand, causing straight beach edges); rivers depth-tested against the terrain, so they meander inside their painted beds instead of showing as tile diamonds; enhanced coastal waves rebuilt as directional swell with height profile, depth-driven breaking and swash.
- GPU compositor and water rework (2026-10-17, after the colleague's second recording: 20–49 fps at 4K on his Linux PC, islands slow to appear when zooming, blob whitecaps and coastal lines in the enhanced water): chunk building scales across cores (8× faster: per-chunk tile culling, object culling before decoding, read-write sprite cache, cached island data, priority job list, prefetch of the neighbouring zoom groups); GPU compositor `anno_render::gpu` (wgpu 30) used by the viewer with CPU fallback, identical to the CPU path within 1 per channel; at 4K on the dev Mac 1–2 ms per frame (p50) instead of 7–25 ms; enhanced water reworked (`docs/render/iso.md`); the preview package carries the frame benchmark (`run-benchmark.bat/.sh`).
- Performance and sea (2026-10-17, after the colleague's feedback "performance horrible, no fps, void between islands, rivers bad, water choppy" and his viewer recording): ground rasterised as two triangles per tile (477 → 25 ms per 1080p view); new frame pipeline `anno_render::frame` shared by viewer and benchmark (background chunk workers, per-pixel sea from a periodic pre-rendered pattern, row bands on all cores); sea everywhere on a world lattice (no void between islands); open-water floor colour from the scene's deepest sea-floor texture (open sea now RGB 24, 92, 140 vs 28, 89, 138 in the original); coastal staircase and island rectangles removed; fps in the window title, display-rate redraw, cross-faded enhanced water. Benchmark and results: `docs/perf/README.md`. Open: rivers (hard tile diamonds and seams), GPU path if weak hardware misses the targets.
- Static analysis (2026-10-16, Ghidra 12.1.4 headless; `docs/re/annodisplay.md`, `docs/re/annowelt.md`): projection constants, per-corner heights (0.04 world units per `HEIGHT` unit ≈ 2.06 px at zoom 0), blitter semantics (signed `z` depth offsets, alpha levels, 5/8 shadows, `H`/`V`/`M`/`W` brushes, no width clipping), signed anchors. Applied: `BSH_RGBWZ` column expansion and `i16` anchors in `formats`; elevation with per-corner slopes at the original scale, alpha blending and shadows in `anno_render::iso`.
- Coast and water (2026-10-16, after review of the window viewer): ground cells corrected from 1/8 to 1/6 of the texture and tiles split into two triangles (both from `AnnoWelt.dll`), removing the coast seams; sea surface from the original's `Meer01` sprites at sea level (`iso::draw_water`, with documented approximations). Open: surf and river objects (`FLUESSE`), water animation, terrain lighting.
- Rivers, surf and animation (2026-10-16): `FLUESSE` objects (rivers, waterfalls, surf) drawn with the sea; the water layer animates from the sprites' `ANIM` records in the viewer (static ground/object layers cached, water redrawn every 50 ms, Space pauses) and via `--png --time`. Fixed a renderer slowdown from per-chunk work over whole islands (ground culling). Open: other animated objects, terrain lighting, open-water look of river channels.
- Placed-object heights and phases from the item `state` (2026-10-16, `AnnoWelt.dll`): objects stand at their own height; surf lies under the sea surface; the original places no coast objects at runtime. Opt-in enhanced water added (breakers, swell; not original).
- `anno_render::iso` (CPU) + `anno-extract view <game_dir> <file> <island> <out.png> [<group> [<lift>]]` render one island: ground from `Felder.tex` cells, `GELAENDE` + `PROD2` objects from `Anlagen.dat` with anchors on footprint centres, back-to-front order. Rules and evidence: `docs/render/iso.md`. Checked on a polar village, a forested island and a walled city (12,316 objects, none failed).
- Viewer (2026-10-16): `anno1503 --view <file.sze|file.scp> [--game-dir <dir>] [--group N] [--png <out.png>]`. `anno_render::scene` places every island at its `INSEL5` world position (rules in `docs/render/iso.md`, "Scene composition"); the window (`winit` 0.30.13 + `softbuffer` 0.4.8, CPU blit) pans by drag/arrows/WASD and switches zoom groups 2/1/0 by wheel or `+`/`-`, rendering 1,024 px chunks lazily on all cores. `--png` is headless; `Single_01.sze` at group 2: 15 islands, 69,256 objects drawn, 0 failed (integration test `crates/app/tests/view.rs`).
- Open (2026-10-16 list, partly done since): `HEIGHT` needs sloped tiles before its scale can be calibrated (OQ-026); figures; island draw order (OQ-026 f).
- [~] Window (winit + softbuffer, CPU) with the isometric ground renderer (`Felder.tex`, `GFXID`); `HEIGHT` not applied yet; wgpu later.
- [x] Draw placed buildings, ships and soldiers from a `.sze` scenario (buildings from `Anlagen.dat`, units from Figuren sprites); draw order (painter's by iso depth); camera pan/zoom. Ships and soldiers done 2026-10-17; civilians/animals appear only at runtime (sim).
- [x] `app --view <file.sze|file.scp>` as the entry point. No gameplay.

- [x] **Language gate (end of M1):** decided 2026-10-17: **C++20** ([ADR 0001](adr/0001-cpp20.md)). Port order: skeleton → `formats` + `anno-dump` (checked against the Python oracles) → `assets`/`render`/`app` → `sim` (M3) and `net` (M4) in C++ only. The Rust code stays as the reference until each part has a C++ counterpart.
  - [x] Skeleton (2026-10-17): CMake ≥ 3.25 with presets `dev`, `release`, `asan`, `msvc`; warnings as errors; doctest 2.4.11 (pinned, SHA-256 checked); `.clang-format`; CI jobs for Linux GCC, Linux Clang + ASan/UBSan, Windows MSVC and a clang-format check.
  - [x] `formats` + `anno-dump` (2026-10-17): every reader (chunk, tex/inc, palettes and COLORS, figures and frames, Anlagen, islands, tagged records, scenarios, plain-text data, fonts) ported to `src/formats`, 36 doctest cases (20.4 M assertions with the corpus). `anno-dump` has all twelve commands; `tools/agree.py` finds 38 of 38 cases byte-identical with the Python references on the synthetic fixtures and the whole corpus (CTest `anno_dump_agrees_with_python`).
  - [x] `assets` (2026-10-17): textures, figure and building frames, player colours and the start-of-game colour assignment, scenario units, top-down island ground in `src/assets`; pixel-identical to the Rust reference on the whole corpus (five FNV-1a digests over 991,503 images, `docs/render/assets.md`).
  - [x] `render` CPU path (2026-10-18): `iso`, `sea`, `scene`, `frame` (chunk cache, background chunk workers, row-band composition) in `src/render`; every scene render and frame digest equals the Rust renderer's (`docs/render/iso.md`, "Fidelity"), sprite fidelity counts repeated.
  - [x] `app` viewer (2026-10-18): `src/app` — scene loading (island links as stand-ins, ships and soldiers), `--png` (own PNG writer), SDL3 window (CPU path, ≈ 4 ms per 2560 × 1600 frame on the dev Mac, `docs/perf/README.md`); same command line as the Rust viewer, `--png` output pixel-identical.
  - [ ] `render` GPU path (WebGPU via `webgpu.h`; implementation open, ADR 0001) with the GPU-vs-CPU check, the wave simulation (`fluid`), `bench_frame`; then preview packages from the C++ build and removal of the Rust crates.

Acceptance M1: `Scenes/0200Single/*.sze` scenarios render recognisably (terrain + buildings + ships). Every parser has corpus + round-trip tests. Specs exist for each format with evidence column filled.

Status M1 (2026-10-17): acceptance met; language gate decided (C++20), C++ formats port complete. All ten `0200Single` scenarios render with terrain, buildings, ships and soldiers (0 failed objects, every unit drawn); every parser (chunk, tex/inc, palettes, figures, Anlagen, islands, scenarios, tagged records, plain-text data) has corpus and round-trip tests and a Python oracle with identical output; specs exist for each format (`docs/formats/`), open fields linked to OQs. Fonts (`.zei`) added 2026-10-17 (`docs/formats/zei.md`); M2 static analysis started 2026-10-17 (tick loop, production, population, fire/ships, scenario loader).

## 3. Milestone M2 — Reverse engineering of the simulation core

Goal: enough understanding of `AnnoWelt`/`AnnoGame`/`AnnoScene` to implement economy, population and production faithfully. Runs in parallel with M1.3–M1.5.

- [ ] Ghidra project on the Linux VM (JDK 17+, headless analyzer). Import all 11 game modules; apply `analysis/pe_symbols.json` names (exports, RTTI) with a Ghidra script; save as `re/ghidra/` (git-ignored except scripts and exported notes).
- [ ] Optional: wire `rea` (morluto/rea, MCP) to that Ghidra instance for agent-driven decompilation. Pin the `rea` version; Linux/macOS host only.
- [ ] Map the interface vtables (`iAnnoFrame`, `iAnnoNetClient`, `iAnnoNetServer`, `iObjectGame`, `IConnectClient`) → `docs/re/interfaces.md`.
- [ ] Locate and document: tick/update loop; goods production (`Anlagen` production interval → output); population growth (`Wachstum`, `VermehrTab`); taxes/balance; road/path speed (`wegspeed.dat`); fire spread (known desync hotspot); ship movement. One note per subsystem in `docs/re/`, each with module + addresses + recovered pseudocode.
- [ ] For each recovered formula, write a differential-test fixture spec (inputs → expected outputs) so it can be executed later against the original on x86 Windows (`anno-diff`).
- [ ] Evidence from the add-on debug build: `FileGame.dll` has a PDB path and source-file names; check whether it holds symbols or asserts that name functions.

### M2.1 Savegame corpus (Spielstände) — needs the human with an x86 Windows/Wine install
Scenario files show the *start* state only; savegames show how the running game stores live state (stocks, residents, carriers, timers, PRNG state, per-player data). They are the main ground truth for M3's state model and for differential tests.
- [ ] **Human task:** run the original GOG 2.0.0.5 build and create Spielstände following a protocol, each with a short note of what was done and the values shown in the UI (gold, population per level, warehouse stock, game time):
  - Baseline: start a known scenario (e.g. `Scenes/0010SingleEndlos/00_1024_easyentry_01.sze`), save immediately; save again after N minutes with no input (pure tick progression).
  - Single-action pairs: save → one action → save (place one building, demolish it, change tax, load/unload a ship, found a settlement). One change per pair so a diff isolates its bytes.
  - One mid/late-game save with a grown city and trade routes; one add-on (`ANNOADDON1`) save; one multiplayer save if the game allows it.
- [ ] Locate where the game writes saves and in which format (OQ-020); confirm whether it is the same chunk container as `.sze`.
- [ ] Storage: savegames stay out of git (they embed original scenario/island data); keep them in `saves/` (git-ignored) and commit only the protocol notes and findings in `docs/re/savegames.md` (file name, size, SHA-256, what was done, UI values).
- [ ] `anno-diff save <a> <b>`: chunk-aware diff of two saves (changed chunks, then byte ranges), to map fields from the single-action pairs.
- [ ] Cross-reference changed fields with the `SaveGame`/`RepairSavegame` code paths in `AnnoScene.dll`/`AnnoWelt.dll` (Ghidra).

Acceptance M2: `docs/re/` has notes for tick loop, production, population, and fire; each with addresses and a fixture spec. No gameplay code yet.

Status M2 (2026-10-17): acceptance met for the static part — tick loop, production, population and fire each have a note with addresses and a fixture spec; executing the fixtures against the original needs the x86 host (M2.1, human task). Static analysis of all seven game modules (`tools/ghidra/DecompileAllPlus.java`, which also finds vtable-only functions). Notes with addresses, confidence labels and fixture specs:
- `docs/re/tick-loop.md`: frame-coupled loop, 1 tick = 2 game ms, speed 50–400 %, multiplayer 10 ms steps in 100 ms turns, MD5 state check every 60 s with snapshot resync; the simulation PRNG is a 2,048-entry table equal to MSVC `rand()` from seed 1 (verified), cursor seeded per game. OQ-005, OQ-006 resolved, OQ-007 mostly.
- `docs/re/production.md`: buildings step every 500 ticks (verified), `PROD3` turned into integer rates at load, input-limited output, fields growing by stages, fertility from `Wachstum.dat`, market carts by fill level; `wegspeed.dat` is never read.
- `docs/re/population.md`: `BGruppen.dat` loader and layouts, houses update every 2,500 ticks (verified), mood −2…+2 from food/goods/infrastructure, growth by `VermehrTab`, up/downgrade rules, taxes every 2,000 ticks; `Priority` is parsed but unused.
- `docs/re/annoscene.md`: island links (OQ-028 resolved), player `HEAD` and colours, unit and record prefixes, the 24-bit long record header.
- `docs/re/fire-and-ships.md`: fire (ignition risk from house counts every 12,500 units, damage ignition, spread every 9th burn step to one random side, extinguishing only by firefighters — fire stations do not lower ignition or spread), plague, volcano; ships (float movement over segments, turning phases, speed from hit points and load, `ROUTE`/`TRADE` layout); unit `attr` = starting facing (OQ-027 d resolved, verified). All randomness draws from the world table; only figure motion uses floats.
Integer-only update paths except figure movement (floats): the rebuild's sim can be exactly deterministic. Fixture execution needs the x86 host (M2.1).

## 4. Milestone M3 — Deterministic simulation (`crates/sim`)

Goal: headless simulation that loads a scenario and advances by ticks; faithful to recovered rules.

- [ ] State model: world, islands, tiles, buildings (Anlagen instances), figures (carriers, ships, residents), players, goods stockpiles. All integer/fixed-point. `#![deny(unsafe_code)]`, no floats in checksummed state.
- [ ] Command enum = the only way to mutate state (build, demolish, set production, trade route, move ship, …). Commands carry player id and target tick.
- [ ] Seeded PRNG inside `sim` (e.g. `rand_pcg`), single source of randomness; expose state hash per tick (`blake3` or FNV over canonical serialization).
- [ ] Implement subsystems in the order recovered in M2: production → carriers/roads → population/needs → taxes → ships/trade → fire → combat (last).
- [ ] Savegame: load original `.sze` scenarios and original savegames (M2.1 corpus) into state; write our own save format (versioned, explicit layout) and, if feasible, write original-compatible `.sze` for interop (stretch).
- [ ] Replay: record commands + seed; `anno-replay` CLI re-runs and compares per-tick hashes. Bisect helper for first divergent tick.

Acceptance M3: a 1-hour headless run of an endless scenario produces identical hashes across Windows and Linux builds and across two runs. Production/population numbers match differential fixtures where executed.

## 5. Milestone M4 — Lockstep multiplayer (`crates/net`)

Goal: 2–8 players, robust against the known failure modes of the original (undetected asyncs, OS/fps-dependent divergence, state-size blowup).

- [ ] Transport: UDP with reliability layer (e.g. `laminar`/`quinn`/custom) — decision ADR in `docs/adr/0001-transport.md`. LAN discovery + direct IP; lobby later.
- [ ] Protocol: commands only, never state. Each command is scheduled for tick `now + delay`; all peers execute the same commands on the same tick. Per-tick state hash exchange; mismatch = detected desync with the exact tick.
- [ ] Join-in-progress: late joiner receives a full save + command backlog (OpenTTD model).
- [ ] Desync recovery: on mismatch, host snapshot re-sync; log command history + hashes for post-mortem (`anno-diff desync`).
- [ ] Tick pacing independent of frame rate: fixed sim tick (e.g. 10–20 Hz, to be decided from M2 findings) decoupled from render loop.
- [ ] Test harness: run N headless peers in-process with simulated latency/loss (`netsim` tests), assert hash equality over 10k ticks.

Acceptance M4: 4 headless peers with 150 ms simulated latency and 2 % loss stay in sync for 10k ticks; an injected divergence is detected within one tick and recovered.

## 6. Milestone M5 — Playable game and QoL

- [ ] UI: menus, build menu, HUD, info panels (reuse original UI textures from `ToolGfx`); input → commands.
- [ ] Audio: WAV samples, ADPCM music (`Music/*`), Bink videos via `ffmpeg` or skip at first (videos are optional).
- [ ] AI opponents: port `CompLogic` behaviour or write a new AI behind the same command interface (decision in M2 after inspecting CompLogic.dll).
- [~] Enhanced water rendering (breakers rolling onto beaches, open-water swell) — implemented 2026-10-16 as an opt-in render option (`E` / `--water enhanced`), faithful water stays the default (`docs/render/iso.md`).
- [ ] QoL behind `rules`/`ui` feature flags: resolution/UI scale, pause-in-MP vote, build queue, trade-route editor, speed settings. Faithful mode must remain selectable.
- [ ] Packaging: Windows (MSVC) and Linux (AppImage/tarball); first-run locates the GOG install or asks for `game/app`.

## 7. Immediate next actions (do these first)

1. ~~M0 in full.~~ Done 2026-10-16.
2. ~~M1.1 chunk container with corpus + round-trip tests.~~ Done 2026-10-16.
3. ~~M1.2 palette + texture decode; PNG export; look at the output.~~ Done 2026-10-16.
4. Start M2 Ghidra setup in parallel; first target `AnnoWelt.dll` tick loop and `AnnoFrame.dll` `CreateAnnoNetClientInstance` call graph.
5. Human task: create the savegame corpus following M2.1 once an x86 Windows/Wine install is available.
6. Human task (not Claude Code): contact millimarg (AnnoZone) for Amsel spec access; download AnnoZone file 9287; pull Azukee's DatEditor from Software Heritage into `docs/research/external/` (check licence before copying code — none stated, so treat as reference only).

## 8. Risks and how the plan handles them

| Risk | Mitigation |
|---|---|
| Sprite/figure format harder than expected | Amsel docs bundle; mdcii BSH decoder as structural analogue; render-and-look validation |
| Recovered formulas use x87 float, results differ on x64 | Reimplement in fixed-point; differential fixtures define tolerance; faithful ≠ bit-identical float |
| No x86 Windows to run the original | Differential fixtures are specs first, executed later; static RE does not need it |
| Scope creep into engine features | Milestones gate on acceptance criteria; QoL only in M5 behind flags |
| Legal | No assets or binaries in repo; clean-room notes cite evidence; GOG install required |
