# ANNO 1503 Rebuild — project rules for Claude Code

Read `docs/PLAN.md` before doing anything. It holds the phased plan, acceptance criteria and the current status. If you are the **executor session**, also read `docs/orchestration/TASKS.md` at the start of every task (the orchestrator session updates it) and report in `docs/orchestration/REPORTS.md`. `docs/research/` holds the evidence gathered so far (community landscape, game-file analysis). Update `docs/PLAN.md` status when a milestone is reached.

## What this project is
A clean-room reimplementation of ANNO 1503 (Max Design / Sunflowers, 2002; GOG "A.D." 2.0.0.5 build, binaries dated 2006-07-21, 32-bit x86, MSVC 6) as a native 64-bit application for **Windows and Linux**, written in **C++20** (owner decision 2026-10-17, `docs/adr/0001-cpp20.md`; the earlier Rust code in `crates/` is the reference implementation until every part is ported, then removed). Goal: the same game, stable, with robust deterministic multiplayer, quality-of-life features and modern performance. macOS is a development host only, never a target.

The original game files are required at runtime and are **never** committed or redistributed. They live in `game/app/` (git-ignored).

## Ground rules
- **Specs and oracles are the source of truth.** Specs in `docs/` and the language-neutral oracles (the Python references in `tools/`, the documented `anno-dump` text output, checked by `tools/agree.py`) define layouts, formulas and invariants; code must not be the only place one is written down. New code is C++20 (`src/`); the Rust code in `crates/` is consulted only where a spec is silent (then fix the spec) and gets no new features. No layout-hiding magic (macros, struct dumps) in `formats` or `sim`.
- **Evidence only.** Every format field, constant or behaviour you implement must trace to one of: a parsed original file, a disassembled function (note module + address), or a documented community source (cite URL). Guesses go into `docs/open-questions.md`, not into code comments stated as fact.
- **Determinism first.** Simulation code (`src/sim`) uses integer/fixed-point arithmetic only, a seeded PRNG owned by the sim, no wall-clock time, no `std::unordered_map`/pointer-order iteration (use ordered containers), no float in state that gets checksummed, `/fp:strict` / `-ffp-contract=off`, no undefined behaviour (UBSan/ASan in CI), no global mutable state. Rendering, audio and UI are consumers of sim state and never mutate it.
- **Separate libraries by responsibility** (see layout below), with the dependency direction of the former crates. `sim` must build and run headless with no SDL/GPU dependency.
- **Explicit serialization.** Original formats are read field by field with fixed-width types (`anno/formats/bytes.hpp`: `u16_le`, `u32_le`, …) from `std::span<const std::uint8_t>`. No `reinterpret_cast` of file data into structs, no struct dumps, no `size_t`/`long` in file layouts. Parsers return `anno::formats::Result<T, E>`, no exceptions in `formats`/`sim`. Little-endian unless proven otherwise (FigurenHeader entries start big-endian per community notes — verify).
- **No hardcoded limits.** Scalability and moddability are goals (`docs/PLAN.md` §0.1). Original counts and sizes (types, map size, players, zoom levels, ids, special entries like the sea sprite) are data or config with the original values as defaults, never fixed arrays, narrow id types or `const` limits in `sim`/`render`/`assets`/`net`/`ui`. `formats` may mirror fixed file layouts exactly; widen when data enters the engine. Any new limit needs a comment with its reason and a path to make it configurable.
- **Use modern multi-core CPUs and GPUs.** Per-pixel and per-sprite work belongs on the GPU (the GPU compositor, `docs/render/`; WebGPU via `webgpu.h` per ADR 0001, wgpu in the Rust reference); content building and other heavy CPU work run in parallel on all cores without shared locks on hot paths; the window thread never waits for content. Keep a CPU reference path and check the GPU against it. Measure every performance change (`docs/perf/README.md`, PLAN §0.3).
- **Keep the faithful core separate from improvements.** Original rules live in `sim`; QoL changes are opt-in features behind a `rules` config, never silent edits to the original behaviour.
- **Tests before refactors.** Every parser gets a round-trip test (parse → write → byte-identical) and a corpus test over all files of that type in `game/app`.
- **Do not touch `game/`.** Read-only. Derived data goes to `build/`, `target/` or `cache/` (git-ignored).
- **Reverse engineering** happens with Ghidra (headless or GUI) on Linux/macOS, optionally driven by `rea` (MCP). On a Windows host `rea`'s Ghidra path rejects 32-bit PE — do not try. Record findings in `docs/re/<module>.md` with addresses, not in code.
- **Language:** English for code, docs, commits. Keep the original German domain terms as identifiers where they are the canonical names (`Anlagen`, `Figuren`, `Welt`, `Insel`, `Waren`, `BGruppen`), with a glossary in `docs/glossary.md`.
- **Commits:** small, one concern each, conventional-commit prefixes (`feat(formats): …`, `re(annowelt): …`, `docs: …`).

## Repository layout
```
anno1503/
  CLAUDE.md
  Cargo.toml               # Rust reference workspace (removed after parity)
  CMakeLists.txt, CMakePresets.json   # C++20 build (presets: dev, release, asan, msvc)
  src/
    formats/               # original file formats (all ported; anno::formats::*)
    assets/                # decoded RGBA images, player colours, scenario units (anno::assets::*)
    test_support/          # game-dir and file helpers for tests
    tools/anno_dump/       # anno-dump (text output = the oracle contract)
    render/                # isometric renderer: CPU path, GPU compositor (wgpu-native), wave simulation
    app/                   # anno1503 binary: scene loading, --png, SDL3 window
    sim/                   # deterministic simulation core (docs/sim/core.md)
    tools/bench_frame/     # frame benchmark (docs/perf/README.md)
    (net, ui follow — ADR 0001 port order)
  fixtures/synthetic/      # our own small test files shared by all implementations
  crates/                  # Rust reference implementation during the port (no new features)
    formats/               # original file formats: chunk container, Anlagen, Figuren, tex/inc, zei, sze, scp, Data/*.dat
    assets/                # decoded, engine-ready assets (sprites, palettes, fonts, islands) built from `formats`
    sim/                   # deterministic game simulation, headless, no I/O
    net/                   # lockstep networking: command queue, tick sync, checksums, resync
    render/                # isometric renderer, sprite batching (Rust reference: wgpu)
    ui/                    # menus, HUD, input → commands
    app/                   # binary: wires everything together
    tools/                 # CLI: `anno-dump`, `anno-extract` (`anno-diff`, differential tests vs. the original, planned)
  docs/
    PLAN.md                # phased plan + status (this is the hand-over document)
    research/              # community-landscape.md, game-files-analysis.md
    formats/               # one spec per format as it is confirmed
    re/                    # Ghidra findings per module
    testing/checklist.md   # open checks only a person can do (kept current)
    glossary.md
    open-questions.md
  tools/*.py               # Python references (chunks, tex, figuren, insel, sze, text, zei) + agree.py
  tools/ghidra/            # Ghidra headless scripts
  analysis/pe_symbols.json # imports/exports/RTTI of all original modules
  game/                    # original game files (git-ignored)
```

## Environment notes
- Dev host right now: Apple Silicon MacBook (Apple Clang, CMake + Ninja from Homebrew). CI builds Windows (MSVC), Linux (GCC; Clang with ASan/UBSan). macOS only as a build check, never as a release target.
- The original game cannot run on this host (32-bit x86 Windows). Differential tests against the original need an x86 Windows machine or Wine on x86 Linux; design them so they can run later from recorded fixtures (`fixtures/`).
- Build: `cmake --preset dev && cmake --build --preset dev && ctest --preset dev -j` (every doctest case is its own CTest test, so `-j` — all cores, CMake ≥ 3.29, else `-j <n>` — runs the long corpus cases side by side; corpus cases also split their files over `ANNO_TEST_THREADS` threads, default min(cores, 4); the count is also capped by free memory (RAM and commit headroom) at 1 GB per thread, `ANNO_TEST_MB_PER_THREAD` changes or, at 0, disables that cap; workers wait for memory between items, at most `ANNO_TEST_MEM_WAIT_S` = 120 s, and an item that hit `std::bad_alloc` is re-run up to `ANNO_TEST_BAD_ALLOC_RETRIES` = 5 times, 0 = off; each such wait, retry or cut prints a `[anno-test memory]` line). Cases in the doctest suite `"heavy"` (whole-scene corpus cases that need well over 1 GB at 16 threads) take all CTest slots and run alone (`anno_add_doctest`; `ctest -L heavy` / `-LE heavy`). Every preset writes the console output, including the output of failed tests, to `build/<preset>/Testing/ctest-output.log`; the output of every test, passed ones and their `[anno-test memory]` lines included, is in `build/<preset>/Testing/Temporary/LastTest.log` (both overwritten by the next run: copy them before re-running when a test failed; or pass `-O <file>`). The Rust reference still builds with the toolchain in `~/.cargo/bin` (`. "$HOME/.cargo/env"`).
- **Tests that need original files** read `ANNO1503_GAME_DIR` (e.g. `ANNO1503_GAME_DIR=$PWD/game/app ctest --preset dev -j`) and skip with a message when it is unset; set to a folder without `Anlagen.dat`, they fail with one clear message (`anno::test::game_dir`, `tools/agree.py`). Never add original files to `fixtures/` or anywhere else in git; CI runs without them.
- **Linux/macOS warnings before a push** (MSVC does not report most of them): `tools/check_unix_warnings.sh` compiles every `.cpp` under `src/` with the CI's flags at `-O3 -Werror` with GCC 13 and Clang 18, viewer and GPU code on, then the app once without `ANNO_VIEWER_GPU` (on Windows: `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`; headers from `build/msvc/_deps`; Clang is skipped with a message when none is found).
- `innoextract` and `pefile` are available in the Linux VM used for analysis; Ghidra needs a JDK 17+ there.

## Definition of done for any task
1. Code compiles on all CI targets without warnings (warnings are errors), `clang-format` 23.1.3 applied, ASan/UBSan clean.
2. Tests added or extended, including corpus tests where a format is involved; `tools/agree.py` agrees for every ported `anno-dump` command.
3. `docs/` updated: format spec, RE note, or PLAN status — whichever applies.
4. No original asset or binary added to git (`git status` shows nothing under `game/`).
