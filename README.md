# ANNO 1503 rebuild

A clean-room reimplementation of ANNO 1503 (Max Design / Sunflowers, 2002) in Rust, for Windows and Linux: faithful to the original where it matters, improved where it helps (see `docs/PLAN.md`). It needs your own copy of the original game files; none are included here.

Current state: an **island viewer**. It reads the original files and draws scenarios and islands isometrically (ground, terrain height, animated sea across the whole map, rivers, surf, trees, buildings, shadows, ships), with optional crashing waves. Frames are drawn on the GPU (Vulkan, DirectX 12 or Metal via wgpu) with a CPU fallback; map areas are prepared on all CPU cores in the background. No gameplay, civilians or sound yet; ships and soldiers are shown where the scenario places them (soldiers in player colours; which player gets which colour is a stand-in); islands that endless games only link to are shown as stand-ins.

**Latest preview (2026-10-17g):** soldiers on the ground in their owner's colour (player colours as the original), and a few building pixels that were cut off; earlier: ships where the scenarios place them, and the endless games open (with stand-in islands of the right climate and size until we know which islands the original picks); earlier: real-time wave simulation on the GPU with `E` (waves shoal, refract around islands, break into moving foam and run up and down the beach); earlier: GPU renderer (≈ 1–2 ms per 4K frame on the dev machine instead of 7–25 ms), about 8× faster map preparation and islands ready when zooming, reworked crashing waves (no white blobs, grid stripes or coastal lines; waves roll in, curl, break and wash up the beach; turquoise shallows; deep water without tile edges), smooth zooming, and a built-in benchmark.

## Test the preview (Windows / Linux)

**You need:** your own installed ANNO 1503 (GOG "A.D." edition) and an up-to-date graphics driver (Vulkan or DirectX 12; without one the viewer falls back to the slower CPU renderer). Below, *game folder* = the folder that contains `Anlagen.dat`.

**1. Download** [`share/anno1503-preview-2026-10-17g.zip`](https://github.com/Dom-cmyk727/anno_rebuild/raw/main/share/anno1503-preview-2026-10-17g.zip) and unzip it anywhere.

**2a. Windows 10/11:** double-click `start-viewer.bat` and enter your game folder when asked (e.g. `C:\GOG Games\ANNO 1503`).

**2b. Linux (x86_64):**
```sh
cd anno1503-preview-2026-10-17g
chmod +x start-viewer.sh run-benchmark.sh linux/*
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

The window title shows the renderer (GPU or CPU) and the frames per second; the console prints a timing line every 2 seconds.

**4. Benchmark:** Windows: double-click `run-benchmark.bat`; Linux: `./run-benchmark.sh "/path/to/ANNO 1503"`. Runs 1–3 minutes without a window and writes `benchmark-results.md` next to the script.

**5. Other maps (optional):** pass a scenario or island file as the second argument, e.g.
`start-viewer.bat "C:\GOG Games\ANNO 1503" "C:\GOG Games\ANNO 1503\Scenes\Islands\Nord\N_128x128_0049_1B_Indianer.scp"`

**Please report back**
- Whether it starts; if not, the exact error text.
- `benchmark-results.md` (step 4).
- The renderer and fps shown in the window title and a few console timing lines, per zoom level with `E` off and on.
- Whether panning and zooming feel smooth, and a short screen recording of a coastline zoomed in with `E` on and off.
- If possible, the same kind of coastline in the original game, for comparison.

**Known limitations:** rivers still look like blocky tile patches (next fix); only water is animated; the enhanced water is an optional improvement, not part of the original, and still being tuned. The builds are cross-compiled on macOS; the GPU path has not been run on Windows or Linux by us yet.

## For developers

- Plan, status and goals: [`docs/PLAN.md`](docs/PLAN.md); project rules: [`CLAUDE.md`](CLAUDE.md).
- File-format specs: [`docs/formats/`](docs/formats/); rendering rules: [`docs/render/iso.md`](docs/render/iso.md); reverse-engineering notes: [`docs/re/`](docs/re/).
- Build: `cargo build --release`; run: `cargo run --release -p anno1503 -- --view <file.sze|file.scp> --game-dir <game folder> [--renderer auto|gpu|cpu]`.
- Performance: `cargo run --release -p anno-render --features gpu --example bench_frame -- <game folder> <file.sze>` (method and results: [`docs/perf/README.md`](docs/perf/README.md)).
- Preview package: `packaging/package-preview.sh <name>` (cross-compiles Windows and Linux builds into `share/`).
- Tests that need the game files read `ANNO1503_GAME_DIR` and skip without it: `ANNO1503_GAME_DIR=<game folder> cargo test --workspace`.
