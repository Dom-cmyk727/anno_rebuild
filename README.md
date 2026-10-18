# ANNO 1503 rebuild

A clean-room reimplementation of ANNO 1503 (Max Design / Sunflowers, 2002) for Windows and Linux, written in C++20 (the first version was in Rust and stays as the reference until the C++ port catches up; [ADR 0001](docs/adr/0001-cpp20.md)): faithful to the original where it matters, improved where it helps (see `docs/PLAN.md`). It needs your own copy of the original game files; none are included here.

Current state: an **island viewer**. It reads the original files and draws scenarios and islands isometrically (ground, terrain height, animated sea across the whole map, rivers, surf, trees, buildings, shadows, ships), with an optional real-time wave simulation. Frames are drawn on the GPU (Vulkan, DirectX 12, Metal or OpenGL via wgpu-native) with a CPU fallback. Map areas are prepared on all CPU cores in the background. No gameplay, civilians or sound yet; ships and soldiers are shown where the scenario places them (soldiers in player colours, assigned as at the start of a game; only the colour the human would pick is a stand-in); islands that endless games only link to are shown as stand-ins.

**Latest preview (2026-10-18b):** with `E` (enhanced water) rivers run through their waterfalls and mouths into the sea as one water surface, and rocks and other objects in the water sink in at their base instead of floating on it. Earlier (2026-10-18a): the first preview built from the C++ version (same pictures as before, checked pixel by pixel); with `E` (enhanced water) ships now sit in the water: the bottom of the hull sinks in, foam marks the waterline, and the ships rise, fall and pitch with the simulated waves. Earlier (2026-10-17g): soldiers on the ground in their owner's colour (player colours as the original), and a few building pixels that were cut off; earlier: ships where the scenarios place them, and the endless games open (with stand-in islands of the right climate and size until we know which islands the original picks); earlier: real-time wave simulation on the GPU with `E` (waves shoal, refract around islands, break into moving foam and run up and down the beach); earlier: GPU renderer (≈ 1–2 ms per 4K frame on the dev machine instead of 7–25 ms), about 8× faster map preparation and islands ready when zooming, reworked crashing waves (no white blobs, grid stripes or coastal lines; waves roll in, curl, break and wash up the beach; turquoise shallows; deep water without tile edges), smooth zooming, and a built-in benchmark.

## Test the preview (Windows / Linux)

**You need:** your own installed ANNO 1503 (GOG "A.D." edition), Windows 10/11 or Linux x86_64 with glibc 2.35 or newer (Ubuntu 22.04, Debian 12, Fedora 36 or later), and an up-to-date graphics driver (Vulkan, DirectX 12 or OpenGL; without one the viewer falls back to the slower CPU renderer). Below, *game folder* = the folder that contains `Anlagen.dat`.

**1. Download** [`share/anno1503-preview-2026-10-18b.zip`](https://github.com/Dom-cmyk727/anno_rebuild/raw/main/share/anno1503-preview-2026-10-18b.zip) and unzip it anywhere.

**2a. Windows 10/11:** double-click `start-viewer.bat` and enter your game folder when asked (e.g. `C:\GOG Games\ANNO 1503`).

**2b. Linux (x86_64):**
```sh
cd anno1503-preview-2026-10-18b
chmod +x start-viewer.sh run-benchmark.sh linux/*
./start-viewer.sh "/path/to/ANNO 1503"
```
The game folder can be a Wine install (`…/drive_c/GOG Games/ANNO 1503`) or files extracted from the GOG installer with `innoextract`.

**3. Controls**

| Key / mouse | Action |
|---|---|
| drag, arrow keys, WASD | move the map |
| mouse wheel, `+` / `-` | zoom (3 levels) |
| `E` | enhanced water on/off: wave simulation, ships in the water (an improvement, not in the original) |
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
- With `E` on, zoomed in fully on a ship: do the hull, the waterline foam and the bobbing look right? A screenshot or short recording helps.
- If possible, the same kind of coastline in the original game, for comparison.

**Known limitations:** rivers still look like blocky tile patches (next fix); only water is animated; the enhanced water is an optional improvement, not part of the original, and still being tuned. The builds are cross-compiled on macOS (Windows with MinGW-w64, Linux in an Ubuntu 22.04 container); they have not been run on a Windows or Linux PC by us. `anno-extract` (graphics export) is not ported to C++ yet and is missing from this package.

## For developers

- Plan, status and goals: [`docs/PLAN.md`](docs/PLAN.md); project rules: [`CLAUDE.md`](CLAUDE.md).
- File-format specs: [`docs/formats/`](docs/formats/); rendering rules: [`docs/render/iso.md`](docs/render/iso.md); reverse-engineering notes: [`docs/re/`](docs/re/).
- **C++20 (main line):** `cmake --preset dev && cmake --build --preset dev && ctest --preset dev -j` (`-j` runs the test cases on all cores; presets `release`, `asan`, `msvc` too). Ported so far: every file-format reader, `anno-dump`, the asset decoding ([`docs/render/assets.md`](docs/render/assets.md)), the CPU renderer and the viewer, all pixel-identical to the Rust version. Check `anno-dump` against the Python references: `python3 tools/agree.py --dump build/dev/src/tools/anno_dump/anno-dump [--game-dir <game folder>]`.
- **C++ viewer:** `cmake --preset release && cmake --build --preset release`, then `build/release/src/app/anno1503 --view <file.sze|file.scp> --game-dir <game folder>` (same options as below; `--renderer auto` uses the GPU through wgpu-native when one is usable, `cpu` forces the CPU path). The first configure downloads SDL3 and the wgpu-native library (both pinned, SHA-256 checked; `-DANNO_BUILD_GPU=OFF` builds without the GPU); `-DANNO_SYSTEM_SDL=ON` uses an installed one. On Linux SDL needs the X11/Wayland development packages listed in `.github/workflows/ci.yml`.
- **Rust reference:** build `cargo build --release`; run: `cargo run --release -p anno1503 -- --view <file.sze|file.scp> --game-dir <game folder> [--renderer auto|gpu|cpu]`.
- Performance (Rust; the C++ `bench_frame` is in `docs/perf/README.md`): `cargo run --release -p anno-render --features gpu --example bench_frame -- <game folder> <file.sze>` (method and results: [`docs/perf/README.md`](docs/perf/README.md)).
- Preview package (C++ build): `packaging/package-preview.sh <name>` (Windows with MinGW-w64, Linux in Docker; into `share/`); the Rust one: `packaging/package-preview-rust.sh`.
- Tests that need the game files read `ANNO1503_GAME_DIR` and skip without it: `ANNO1503_GAME_DIR=<game folder> ctest --preset dev -j` (Rust: `cargo test --workspace`).
