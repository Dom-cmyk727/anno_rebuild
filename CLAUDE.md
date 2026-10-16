# ANNO 1503 Rebuild — project rules for Claude Code

Read `docs/PLAN.md` before doing anything. It holds the phased plan, acceptance criteria and the current status. `docs/research/` holds the evidence gathered so far (community landscape, game-file analysis). Update `docs/PLAN.md` status when a milestone is reached.

## What this project is
A clean-room reimplementation of ANNO 1503 (Max Design / Sunflowers, 2002; GOG "A.D." 2.0.0.5 build, binaries dated 2006-07-21, 32-bit x86, MSVC 6) as a native 64-bit application for **Windows and Linux**, written in **Rust**. Goal: the same game, stable, with robust deterministic multiplayer, quality-of-life features and modern performance. macOS is a development host only, never a target.

The original game files are required at runtime and are **never** committed or redistributed. They live in `game/app/` (git-ignored).

## Ground rules
- **Keep a C++20 port possible.** Rust is the current language, but a C++-oriented co-developer may continue the project and a C++20 migration is a live option (`docs/adr/0000-implementation-language.md`). Specs in `docs/` and language-neutral oracles (`tools/chunks.py`, documented `anno-dump` text output) are the source of truth; code must not be the only place a layout, formula or invariant is written down. No Rust-only magic (serde derives, layout-hiding macros) in `formats` or `sim`.
- **Evidence only.** Every format field, constant or behaviour you implement must trace to one of: a parsed original file, a disassembled function (note module + address), or a documented community source (cite URL). Guesses go into `docs/open-questions.md`, not into code comments stated as fact.
- **Determinism first.** Simulation code (`crates/sim`) uses integer/fixed-point arithmetic only, a seeded PRNG owned by the sim, no wall-clock time, no HashMap iteration order (use `BTreeMap`/`IndexMap`), no float in state that gets checksummed. Rendering, audio and UI are consumers of sim state and never mutate it.
- **Separate crates by responsibility** (see layout below). `sim` must build and run headless with no SDL/wgpu dependency.
- **Explicit serialization.** Original formats are read field by field with fixed-width types via `byteorder`/`nom`-style readers. No `transmute`, no struct dumps, no `usize` in file layouts. Little-endian unless proven otherwise (FigurenHeader entries start big-endian per community notes — verify).
- **Keep the faithful core separate from improvements.** Original rules live in `sim`; QoL changes are opt-in features behind a `rules` config, never silent edits to the original behaviour.
- **Tests before refactors.** Every parser gets a round-trip test (parse → write → byte-identical) and a corpus test over all files of that type in `game/app`.
- **Do not touch `game/`.** Read-only. Derived data goes to `target/` or `cache/` (git-ignored).
- **Reverse engineering** happens with Ghidra (headless or GUI) on Linux/macOS, optionally driven by `rea` (MCP). On a Windows host `rea`'s Ghidra path rejects 32-bit PE — do not try. Record findings in `docs/re/<module>.md` with addresses, not in code.
- **Language:** English for code, docs, commits. Keep the original German domain terms as identifiers where they are the canonical names (`Anlagen`, `Figuren`, `Welt`, `Insel`, `Waren`, `BGruppen`), with a glossary in `docs/glossary.md`.
- **Commits:** small, one concern each, conventional-commit prefixes (`feat(formats): …`, `re(annowelt): …`, `docs: …`).

## Repository layout
```
anno1503/
  CLAUDE.md
  Cargo.toml               # workspace
  crates/
    formats/               # original file formats: chunk container, Anlagen, Figuren, tex/inc, zei, sze, scp, Data/*.dat
    assets/                # decoded, engine-ready assets (sprites, palettes, fonts, islands) built from `formats`
    sim/                   # deterministic game simulation, headless, no I/O
    net/                   # lockstep networking: command queue, tick sync, checksums, resync
    render/                # wgpu/SDL2 isometric renderer, sprite batching
    ui/                    # menus, HUD, input → commands
    app/                   # binary: wires everything together
    tools/                 # CLI: `anno-dump`, `anno-extract`, `anno-diff` (differential tests vs. original)
  docs/
    PLAN.md                # phased plan + status (this is the hand-over document)
    research/              # community-landscape.md, game-files-analysis.md
    formats/               # one spec per format as it is confirmed
    re/                    # Ghidra findings per module
    glossary.md
    open-questions.md
  tools/chunks.py          # Python reference parser for the chunk container (keep in sync with crates/formats)
  analysis/pe_symbols.json # imports/exports/RTTI of all original modules
  game/                    # original game files (git-ignored)
```

## Environment notes
- Dev host right now: Apple Silicon MacBook. Rust builds natively here for iteration; CI builds for `x86_64-pc-windows-msvc` and `x86_64-unknown-linux-gnu`. Add `aarch64-apple-darwin` to CI only as a build check, never as a release target.
- The original game cannot run on this host (32-bit x86 Windows). Differential tests against the original need an x86 Windows machine or Wine on x86 Linux; design them so they can run later from recorded fixtures (`fixtures/`).
- Rust toolchain via rustup in `~/.cargo/bin` (installed without editing shell profiles; `. "$HOME/.cargo/env"` or add it to PATH).
- **Tests that need original files** read `ANNO1503_GAME_DIR` (e.g. `ANNO1503_GAME_DIR=$PWD/game/app cargo test --workspace`) and skip with a message when it is unset. Never add original files to `fixtures/` or anywhere else in git; CI runs without them.
- `innoextract` and `pefile` are available in the Linux VM used for analysis; Ghidra needs a JDK 17+ there.

## Definition of done for any task
1. Code compiles on all CI targets, `cargo clippy -- -D warnings` clean, `cargo fmt` applied.
2. Tests added or extended, including corpus tests where a format is involved.
3. `docs/` updated: format spec, RE note, or PLAN status — whichever applies.
4. No original asset or binary added to git (`git status` shows nothing under `game/`).
