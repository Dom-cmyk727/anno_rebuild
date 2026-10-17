# ANNO 1503 rebuild

A clean-room reimplementation of ANNO 1503 (Max Design / Sunflowers, 2002) in Rust, for Windows and Linux: faithful to the original where it matters, improved where it helps (see `docs/PLAN.md`). It needs your own copy of the original game files; none are included here.

Current state: an **island viewer**. It reads the original files and draws scenarios and islands isometrically (ground, terrain height, animated sea, rivers, surf, trees, buildings, shadows), with optional crashing waves. No gameplay, ships, people or sound yet.

## Test the preview (Windows / Linux)

**You need:** your own installed ANNO 1503 (GOG "A.D." edition). Below, *game folder* = the folder that contains `Anlagen.dat`.

**1. Download** [`share/anno1503-preview-2026-10-17c.zip`](https://github.com/Dom-cmyk727/anno_rebuild/raw/main/share/anno1503-preview-2026-10-17c.zip) and unzip it anywhere.

**2a. Windows 10/11:** double-click `start-viewer.bat` and enter your game folder when asked (e.g. `C:\GOG Games\ANNO 1503`).

**2b. Linux (x86_64):**
```sh
cd anno1503-preview-2026-10-17c
chmod +x start-viewer.sh linux/*
./start-viewer.sh "/path/to/ANNO 1503"
```
The game folder can be a Wine install (`…/drive_c/GOG Games/ANNO 1503`) or files extracted from the GOG installer with `innoextract`.

**3. Controls**

| Key / mouse | Action |
|---|---|
| drag, arrow keys, WASD | move the map |
| mouse wheel, `+` / `-` | zoom (3 levels) |
| `E` | crashing waves on/off (an improvement, not in the original) |
| `Space` | pause the water animation |
| `Esc`, `Q` | quit |

**4. Other maps (optional):** pass a scenario or island file as the second argument, e.g.
`start-viewer.bat "C:\GOG Games\ANNO 1503" "C:\GOG Games\ANNO 1503\Scenes\Islands\Nord\N_128x128_0049_1B_Indianer.scp"`

**Please report back**
- Whether it starts; if not, the exact error text.
- `benchmark-results.md` from `run-benchmark.bat` / `./run-benchmark.sh "/path/to/ANNO 1503"` (1–3 minutes, no window).
- The fps shown in the window title (it also names the renderer: GPU or CPU) and a few of the timing lines printed in the console every 2 seconds, per zoom level with `E` off and on.
- A short screen recording of the viewer with `E` on and off, ideally at a coastline zoomed in.
- If possible, the same kind of coastline in the original game, for comparison.

The builds are cross-compiled on macOS and have not been run on Windows or Linux by us yet.

## For developers

- Plan, status and goals: [`docs/PLAN.md`](docs/PLAN.md); project rules: [`CLAUDE.md`](CLAUDE.md).
- File-format specs: [`docs/formats/`](docs/formats/); rendering rules: [`docs/render/iso.md`](docs/render/iso.md); reverse-engineering notes: [`docs/re/`](docs/re/).
- Build: `cargo build --release`; run: `cargo run --release -p anno1503 -- --view <file.sze|file.scp> --game-dir <game folder>`.
- Performance: `cargo run --release -p anno-render --features gpu --example bench_frame -- <game folder> <file.sze>` (method and results: [`docs/perf/README.md`](docs/perf/README.md)).
- Preview package: `packaging/package-preview.sh <name>` (cross-compiles Windows and Linux builds into `share/`).
- Tests that need the game files read `ANNO1503_GAME_DIR` and skip without it: `ANNO1503_GAME_DIR=<game folder> cargo test --workspace`.
