# ANNO 1503 Rebuild — Implementation Plan

Status legend: `[ ]` open · `[~]` in progress · `[x]` done. Update this file as work lands.

## 0. Starting point (verified 2026-10-16)

- Source material: GOG "Anno 1503 A.D." 2.0.0.5, extracted with innoextract to `game/app/` (6,040 files, ~1.5 GB). Königsedition-level build; all game DLLs dated 2006-07-21, 32-bit, MSVC 6 (see `docs/research/game-files-analysis.md`).
- Original architecture: COM-like modules behind `Create…Instance` exports. Likely simulation core: `AnnoWelt.dll` (967 KB), `AnnoGame.dll` (server), `AnnoScene.dll` (scenario/savegame). Rendering: `AnnoDisplay.dll` (DirectDraw). Networking: `AnnoFrame.dll`, custom UDP over Winsock (no DirectPlay), with an "Async aufgetreten" desync path. RTTI class names survive in AnnoFrame, View, AnnoDisplay (`analysis/pe_symbols.json`).
- Formats: one generic chunk container (12-byte name + u32 LE size + payload, nested) covers `.sze`, `.scp` (root only), `.tex`, `.inc`, `.zei`, `Anlagen.dat`. `tools/chunks.py` parses these and is the reference. `FigurenHeader.dat` uses key-value records inside ENTRY and needs its own reader. `Figuren*.dat` are raw RLE sprite data indexed by the header. `Data/*.dat`, `Texte.dat`, `BGruppen.dat` are plain text.
- Community: no usable open-source reimplementation exists. Most useful prior work: Amsel (millimarg, XML format specs, currently behind GitLab login; docs bundle on AnnoZone file 9287), DatEditor/Muto1503 (Azukee, `Anlagen.dat` parser + MinHook hooks, archived at Software Heritage). See `docs/research/community-landscape.md`.
- Decisions: Rust (C++20 kept open for a co-developer, review at the M1 → M3 gate, `docs/adr/0000-implementation-language.md`); targets Windows + Linux x86_64; first visible milestone is formats + asset viewer; English docs; faithful core separated from QoL.

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
- [ ] Decode `COLOR` (palette) and `TEXTURE` chunks in `.tex`; `ACCESS` in `.inc`. Determine pixel format from `Textures/debug.tex` and `ToolGfx/Felder.tex` (32×32 ground tiles per drkohler). Evidence: compare against drkohler's GFX Extractor output if obtainable; otherwise validate by rendering and eyeballing known UI textures.
- [ ] Export to PNG via `anno-extract tex <file> <outdir>` for visual verification.
- [ ] Spec: `docs/formats/tex.md`, `docs/formats/palette.md`.

Status M1.2 (2026-10-16): `[~]` decoders done, PNG export and specs open.
- Done: `anno_formats::tex` (palettes, `TEXTURE` HEAD/BODY/NAME, RLE + raw decode) and `anno_formats::inc` (`ACCESS` records). `tex_corpus`/`inc_corpus` pass on all 1,214 `.tex` and 1,213 `.inc`: 1,427 palettes, 5,176 textures (5,028 `BSH_RGB` RLE, 1 `BSH_RGBH` RLE, 147 `TEX_RGB` raw ground tiles), every `HEAD.palette_id` resolves, every texture decodes to exactly `width × height`, every `.inc` number is a `TEXTURE` id in the matching `.tex`.
- Evidence summary: `COLOR`/`TEXTURE` payload = 8-byte prefix (`u32`, `u32 id`) + sub-chunks (so generic detection treats them as leaves, OQ-013). Palette = 256 × `[B, G, R, 0]`; byte order from sea tiles (`Meer*.bmp` in `Felder.tex` average B=122 G=77 R=33). RLE: per row `(skip, count, count × index)` runs, `0xFE` end of row, `0xFF` end of image.
- Open: `anno-extract tex <file> <outdir>` PNG export (needs `png` crate in `crates/tools`) and a visual check; specs `docs/formats/tex.md` + `palette.md` (facts above, field table in `crates/formats/src/tex.rs`); `anno-dump` subcommand with a documented text format for the C++ oracle; RLE re-encoder for byte-identical texture round trip; unknown fields in OQ-021.

### M1.3 Figures / sprites (`FigurenHeader.dat`, `Figuren000..200.dat`)
- [ ] Reader for `FigurenHeader.dat`: ENTRY records with 12-byte keys + u32 (`VERSION2`, `HEAD`, `BEWOHNTYPE`, `ROUTETYPE`, `LOGIC`, `PATH`, `COLORS`, `HAEUSER`, …). Per community notes each sprite header holds height, width, local palette id, tag, midpoint, data offset and length; first four bytes big-endian — **verify** against the file before trusting.
- [ ] RLE decoder for `Figuren*.dat` sprite data. Hypothesis from mdcii/anno16_docs: 8-bit indexed, RLE with transparency runs, external palette. Confirm shape by decoding a known sprite (e.g. figure id 203 = carrier per AnnoZone) and rendering it.
- [ ] Shadow/alpha maps ("BSH_ALPHA" greyscale per community) — decode once base sprites work.
- [ ] `anno-extract figuren` dumps all sprites as PNG with an index JSON (id, size, midpoint, palette).
- [ ] Spec: `docs/formats/figuren.md`. Cross-check with Amsel's spec once obtained (contact millimarg — see §7).

### M1.4 Buildings (`Anlagen.dat/ANLAGEN`, 921 ENTRY)
- [ ] Decode ENTRY layout (size, footprint, gfx ids, production fields). Sources: Azukee's `AnlagenParser.cs` (class names `TAnlage*`, `THaus*` from IDA) via Software Heritage; `anno-1503-easy-player-boost/build_mod.py` for confirmed field offsets (production interval, coverage radius, `VermehrTab`).
- [ ] Join with `Texte.dat` `[ANLAGEN]` id→name for a readable dump (`anno-dump anlagen`).
- [ ] Spec: `docs/formats/anlagen.md`.

### M1.5 Islands and scenarios (`.scp`, `.sze`)
- [ ] Island: `INSEL4/5`, `INFO`, `HEIGHT`, `GFXID`, `GELAENDE`, `PROD2`, `FRUCHTBAR`, `BERGWERK2`, `OCCUR`, `ANBAU`, `DISCOVER`, `KONTORID`, `STAEDTE`, `LOCKED`. Start with `HEIGHT`/`GFXID`/`GELAENDE` (grid arrays; dimensions from file name `Po_064x064_…` and `INFO`).
- [ ] Scenario: `SCENE/{HEAD, MAIN, GUID, ALIASNAME, WARETYPE, SKTIMER, MONOPOL2, LIMITS, COMPPROFIL2, ROHSTOFF, SCHATZ, ERZE, BIGBROTHER, AUFTRAGTEXT, SPIELER, INSEL5, INSELLNK}`. Root `ANNO` vs `ANNOADDON1` gates add-on content.
- [ ] Versioned chunk names (`INSEL4`→`INSEL5`, `PROD2`, `MONOPOL2`) → loader supports both where both occur in the corpus.
- [ ] Plain-text data: parsers for `Data/BGruppen.dat` (goods, prices), `Wachstum.dat`, `wegspeed.dat`, `Felder.dat`, `Texte.dat`. These are INI-like with `Key: value` lines and `[SECTION]` headers.
- [ ] Specs: `docs/formats/scp.md`, `docs/formats/sze.md`, `docs/formats/data-text.md`.

### M1.6 Viewer (`crates/render`, `crates/app`)
- [ ] wgpu + SDL2 (or winit) window, isometric tile renderer for ground (`Felder.tex`) using `HEIGHT`/`GFXID` from one `.scp`.
- [ ] Draw placed buildings from a `.sze` scenario using Figuren sprites; correct draw order (painter's by iso depth); camera pan/zoom.
- [ ] `app --view <file.sze|file.scp>` as the entry point. No gameplay.

- [ ] **Language gate (end of M1):** review ADR 0000 with the co-developer before any `sim` code is written: stay in Rust, or port `formats` to C++20 against the existing oracles/corpus facts and continue there.

Acceptance M1: `Scenes/0200Single/*.sze` scenarios render recognisably (terrain + buildings + ships). Every parser has corpus + round-trip tests. Specs exist for each format with evidence column filled.

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
- [ ] QoL behind `rules`/`ui` feature flags: resolution/UI scale, pause-in-MP vote, build queue, trade-route editor, speed settings. Faithful mode must remain selectable.
- [ ] Packaging: Windows (MSVC) and Linux (AppImage/tarball); first-run locates the GOG install or asks for `game/app`.

## 7. Immediate next actions (do these first)

1. ~~M0 in full.~~ Done 2026-10-16.
2. ~~M1.1 chunk container with corpus + round-trip tests.~~ Done 2026-10-16.
3. M1.2 palette + texture decode; PNG export; look at the output.
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
