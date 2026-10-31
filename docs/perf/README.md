# Performance measurements

Goal and targets: [`PLAN.md`](../PLAN.md) §0.3. Every benchmark runs the production code path headless and reports percentiles; results below are kept per date and machine so regressions are visible.

## View frames: `bench_frame`

```sh
cargo run --release -p anno-render --features gpu --example bench_frame -- <game_dir> <file.sze|.scp> \
    [--threads n] [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2] \
    [--styles original,enhanced] [--chunks blocking,async] [--renderer cpu,gpu] \
    [--check] [--at dx,dy] [--quick]
```
C++: `build/release/src/tools/bench_frame/bench_frame <game_dir> <file> [same options]` (`--renderer gpu` and `--check` need a build with `ANNO_BUILD_GPU`, the default). The preview package contains the Rust one as `bench_frame` with `run-benchmark.bat` / `run-benchmark.sh` (writes `benchmark-results.md`).

**What it runs:** the functions the viewer calls every frame, without a window: `FrameRenderer::render` into a `0RGB` buffer (`cpu`), or `GpuFrame::render` into an offscreen texture followed by waiting for the GPU (`gpu`). `--check` compares the last static frame of each configuration between GPU and CPU (pixels differing by more than 2 per channel, maximum difference); the C++ one also reports GPU errors, a failed read-back, and frames that still lacked chunks (async mode: open sea there, not comparable). `BENCH_DUMP=<dir>` writes both images as raw RGBA. `--at dx,dy` moves the camera from the largest island's centre.

**Camera paths:** per resolution, zoom group, water style and chunk mode, from a cold cache centred on the scenario's largest island:

| path | frames | movement |
|---|---|---|
| cold | 1 | first frame, nothing cached |
| static | 120 | none (animation only) |
| pan slow | 240 | 6 × 3 px per frame |
| pan fast | 120 | 48 × 24 px per frame (new chunks keep appearing) |

The animation clock advances 16 ms per frame. `--quick` divides the frame counts by 4.

**Zoom path (C++ only):** `--zoom k` replaces the four paths by one that behaves like fast mouse-wheel zooming. After 60 paced warm-up frames the zoom group changes every `k` frames (2, 1, 0, 1, 2, … around the window centre) for `--zoom-frames` frames (default 240), each frame paced to at least `--pace` ms (default 2, a 500 Hz display) so that the chunk workers and the wave-simulation setup get real time. As in the viewer, a zoom step waits until the target group's sea pattern is ready (`GpuFrame::group_ready`). The frame time of this path is the window thread's CPU time (the viewer never waits for the GPU; the wait is in its own column). Frames slower than `--spike` ms (default 8) are listed with the time of every phase: chunk receipt (`chunks`), sea pattern upload (`sea`), chunk uploads (`chunk sync`, of it texture creation `create` and texel writes `write`), sprite decoding (`decode`) and atlas packing, wave simulation (`fluid`), command recording and submission (`encode`), GPU wait. `--limits default|interactive` selects `GpuLimits{}` or `GpuLimits::interactive()` (what the viewer uses: time-budgeted chunk uploads, wave-simulation window set up in the background); `--settle ms` renders paced frames at the start of every path set so that background work has finished (for `--check`; the animation clock stands still meanwhile). The viewer has the same hook: `ANNO_VIEW_ZOOM_SCRIPT=<frames>,<seconds>[,enhanced]` zooms one group every `<frames>` drawn frames and quits after `<seconds>`; `ANNO_VIEW_LIMITS=default` runs it with the library defaults for comparison.

**Chunk modes:**
- `blocking`: missing chunks are built inside the frame, as headless renders do. This shows what chunk building costs.
- `async`: chunks are built by background workers on half the threads, as in the viewer. Missing chunks show open sea. Frames run back to back without the 16 ms wait a real display has, so workers get less time than in the viewer and `missing` is an upper bound.

**Columns:**

| column | meaning |
|---|---|
| p50 / p95 / p99 / max | frame-time percentiles in ms |
| ≤16.7 ms | share of frames that fit a 60 Hz frame |
| built | chunks rendered (blocking) or received (async) |
| chunk ms | mean time per frame spent on chunks |
| missing | mean visible chunks shown as open sea (async) |
| stage 1–4 | mean time per stage in ms — CPU: ground, water objects, sea, objects; GPU: uploads, sprites, encode, GPU wait |

**Simulating weaker PCs:** `--threads 4` limits frame stages and workers to 4 threads. Per-core speed still differs from the target hardware, so measure on the colleague's Windows/Linux PC as well (the viewer prints the same stage breakdown to stderr every 2 s and shows fps in its title).

## Results

### 2026-10-19 — river flow on the GPU and its cost tiers (stage S6), Windows 11, Ryzen 9 9800X3D, RTX 4090 and the Radeon iGPU, MSVC Release
Method: `bench_frame <game> x --river-cost` (a synthetic 1024 x 1024 view of pure river pixels through the real `sea_fs`, every layer active, GPU timestamps), `bench_frame ... --river-pass` (the real view with the legacy shimmer and with the flow, the sea pass timed with timestamps), `bench_frame --zoom 6` for the window thread; `ANNO_GPU_ADAPTER` picks the adapter (`--list-adapters`). Baseline: the build of the commit before this stage, recorded the same day.
- **Cost per million river pixels** (ms; the tier on top of the legacy shimmer): RTX 4090 (Vulkan) legacy 0.041, Low +0.016, Medium +0.026, High +0.040; Radeon iGPU (Vulkan) legacy 8.11, +2.30 / +4.03 / +6.06; iGPU (DirectX 12) legacy 9.52, +2.05 / +3.78 / +5.82. Gate 1.0 ms added: High on the RTX 4090 (discrete adapters are not measured), Low (the floor) on the iGPU.
- **Real view** (the owner's canyon, 2494 x 1355, group 0 / 1 / 2, 470,675 / 281,215 / 77,524 river pixels): the flow adds 0.025 / 0.015 / 0.004 ms at High on the RTX 4090 (sea pass 0.09 / 0.09 / 0.15 ms) and 3.25 / 2.11 / 0.52 ms on the iGPU (sea pass 7.4 / 7.3 / 25.6 ms: the iGPU is over a 2 ms frame without the flow).
- **Window thread** (`--zoom 6`, `--limits interactive`, async chunks, enhanced, p50 / p95 / p99 / max ms), before against after: `Single_01.sze` 2560 x 1369 group 0 0.3 / 1.2 / 1.8 / 19.0 against 0.4 / 1.4 / 2.6 / 3.2, group 1 0.3 / 1.2 / 1.7 / 1.9 against 0.3 / 1.3 / 1.8 / 1.8, group 2 0.3 / 1.4 / 1.8 / 3.8 against 0.4 / 1.5 / 1.9 / 2.4; the owner's canyon group 1 0.3 / 1.0 / 1.6 / 2.8 against 0.3 / 1.0 / 1.8 / 2.1; 3840 x 2160: p50 0.4 against 0.5, p99 1.8-2.5 against 2.1-2.7, occasional 6-27 ms outliers at group 2 in both builds while other jobs ran. The chunk bind groups have three more entries (0.1 ms at the median at 4K); the worst busy frame stays in the low milliseconds. Details: `docs/render/rivers.md` section 16.3.
- **Startup measurement (review of S6):** the automatic tier on a non-discrete adapter runs on the window thread once in `GpuFrame::create`; it now has a wall-clock budget (`GpuLimits::river_measure_budget_ms`, 500 ms) and a measurement the driver cannot time never picks the dearest tier. The Radeon iGPU needs about 0.2 s; a slower adapter ends at the budget with Low.

### 2026-10-19 — river flow build (stage S4), Windows 11, Ryzen 9 9800X3D, MSVC Release

`river_qa --flow` (one island at a time, single thread per island; `docs/render/rivers.md` section 14): `RiverFlow::build` over the 106 islands with rivers (72 templates + the 13 scenarios): p50 11 ms, p95 44 ms, max 60 ms (the gate: p95 <= 0.3 s, none above 1 s); the tile-space raster `Scene::river_mask` (sprite covers at group 0): p50 30 ms, p95 84 ms, max 96 ms. Largest island 55,923 wet cells, 17-98 CG iterations, 5-7 multigrid levels, residual <= 1e-5. Both run on workers (nothing on the window thread yet); the zoom benchmark rows are unchanged (`bench_frame --zoom 6`, `Single_01.sze`, 2560 x 1369, enhanced: p50 0.3 ms, p95 1.4-1.5, p99 1.7-1.9 at all three groups, before and after).

### 2026-10-19 — river flow lattice and shading (stage S5), Windows 11, Ryzen 9 9800X3D, RTX 4090, MSVC Release
Method: `bench_frame ... --river-flow legacy|flow` (CPU path), a test that times `render_chunk` with the lattice off and on, `bench_frame --zoom 6 --renderer gpu --limits interactive` for the window thread. Baseline: the build of the commit before this stage, recorded the same day.
- **Chunk build (`render_chunk`, a river chunk of `D_256x256_0045_5B`, the flow built before):** without / with the lattice 55.9 / 56.5 ms (group 0), 79.9 / 80.2 ms (group 1), 82.6 / 83.4 ms (group 2): +1.1 / +0.4 / +1.0 %. The apron (two 1-px ground probes) is 0.7 / 0.9 / 1.2 % of the chunk's ground pass (limit 5 %). The flow itself (raster 30-90 ms, solve 10-60 ms per island, section 14.3) is built once per island by background threads; a chunk built before it is ready draws the legacy look and is rebuilt.
- **CPU frame (`D_256x256_0045_5B`, 1280 x 720, group 1, enhanced, 16 threads):** static p50 22.5 ms legacy, 25.4 ms flow (sea stage 17.3 -> 20.4 ms); pan slow p50 22.5 / 24.5 ms. The first frame of a blocking run includes the island's flow build (about +100 ms and the same in legacy mode, which also bakes). Original water: unchanged (6.8 ms static before and after).
- **GPU path (window thread), `Single_01.sze`, 2560 x 1369, interactive limits, `--zoom 6`:** the GPU path does not read the lattice yet (S6), so the numbers do not move: p50 / p95 / p99 / max of the window thread 0.3 / 1.1 / 1.9 / 18.5 (a one-off outlier, 3.0 and 3.3 in two repeats) at group 0, 0.3 / 1.1 / 1.8 / 1.9 at group 1, 0.3 / 1.1 / 1.9 / 2.6 at group 2; before 0.3 / 1.6 / 3.2 / 3.8, 0.3 / 1.5 / 1.6 / 1.8, 0.3 / 1.3 / 1.9 / 2.4 (the flow builders and chunk workers never block it: `render_chunk` of the workers does not wait for a flow).

### 2026-10-19 — zoom stutter, Windows 11, Ryzen 9 9800X3D (16 threads), RTX 4090 (Vulkan), commit 359b1ce + this work

**Report (owner, enhanced water, 2560 × 1369 on a 500 Hz display):** average frames 0.3–0.7 ms but a worst frame of 100–132 ms in every report window that contains a zoom-group change; an earlier Linux log (RX 7900 XTX) showed 60–103 ms after chunk bursts.

**Reproduction** (`bench_frame --zoom 6 --chunks async --renderer gpu --res 2560x1369`: GPU, a zoom step every 6 frames at a 2 ms pace, `01_1024_easy_256.sze`, camera on the largest island; window-thread CPU time per frame in ms; the machine was shared with other jobs, so tails of a few ms are noise):

| water | state | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| enhanced | before | 6.7 (incl. GPU wait) | 137.5 | 167.7 | 608 (sea pattern build, a bench artefact) |
| enhanced | after, library defaults (wave window built in the frame) | 1.0 | 184.5 | 316.7 | 511 |
| enhanced | after, `GpuLimits::interactive()` | 0.3–0.6 | 1.0–3.3 | 2.0–6.1 | 3.7–27 |
| original | before | 0.4 | 2.1 | 8.4 | 12 |
| original | after | 0.4 | 1.0 | 1.9 | 3.4 |

**Causes, found with per-phase timers (`GpuStats`, the slow-frame table of the zoom path):**
1. **The wave simulation window was set up on the window thread (enhanced water): 95–145 ms at every zoom group change.** `FluidGpu::update` created the window of the new group and filled it inside the frame: depth sampling and the initial swell state of about 10⁶ cells on all cores, then 20 MB of `push_back`ed bytes and a grid of fresh textures. Cells per tile are 16 / 8 / 4 per group, so every switch rebuilds it. These are the 100–132 ms of the owner's log.
2. **Chunk uploads on the window thread: 7–25 ms per frame after a burst** (6 chunks per frame): 0.8 ms per chunk converting its planes texel by texel (`*o++ = …` loops), 0.35 ms writing the texels, plus five textures created and later freed per chunk.
3. **Evicted chunks were freed on the window thread: 10–11 ms** (`FrameRenderer::trim` destroying a dozen chunks of about 4 MB).
4. **The sea pattern of the target zoom group was built or uploaded on first use: 100–1100 ms** (`Scene::sea` blocks in `call_once`; `GpuFrame` uploaded all steps of the current group at once). The viewer renders the patterns on threads from the start, so this only hit a zoom in the first seconds.
5. **The `worst` figure was not honest.** It was the compositor's own timer plus `wgpuSurfacePresent`: acquiring the surface texture (where vsync waits), creating its view, the event loop and everything between frames were not in it, and nothing measured the gap between frames.

**Fixes** (the window thread never waits for content; CPU reference path, digests and the original water are untouched):
- `FluidGpu` background mode (`GpuLimits::background_fluid`, on in `GpuLimits::interactive()`): a new window is computed on a background thread straight into the textures' byte layout and written to the GPU in budgeted slices (`upload_budget_ms` 2 ms and 16 MiB per frame); a recentre computes its strips the same way. The previous window keeps simulating until the new one is complete; where it does not cover the view the shader uses the analytic waves (`fluid_inside`). A request for another group or size cancels the job in flight, so only the last one is finished. Finished and cancelled jobs are freed off the window thread, and the grid a window switch replaces is kept as the next window's textures (a window has about the same number of cells at every group, so no texture is created at zoom time). Blocking mode (the default) is unchanged; a test (`gpu_test.cpp`, "the wave solver sets its window up in the background without blocking frames") compares the background window with the CPU reference (max difference 0) at the first window, after a group change and after a recentre, and `bench_frame --check` gives the identical GPU image in both modes.
- Chunks are uploaded as they are: the vectors' bytes are the texel bytes (`Chunk::field` now holds the four sea-field planes interleaved, built by the chunk workers), so the window thread converts nothing. Uploads stop after `upload_budget_ms` (1.0 in `interactive()`, at least one chunk per frame). The planes of dropped chunks are pooled and reused instead of destroyed and created (the driver allocated and freed device memory blocks, sporadic 10–20 ms stalls). Evicted CPU chunks go to a reaper thread of `ChunkWorkers` (`ChunkWorkers::discard`).
- The viewer delays a zoom step until the target group's sea pattern is rendered and on the GPU (`GpuFrame::group_ready`, only noticeable in the first seconds); sea steps of the other groups are uploaded within the frame's time budget.
- The viewer's `worst` is now the window thread's busy time over the whole of `draw()` (surface acquire excluded and reported on its own as the wait for the display), plus the worst gap between the starts of two consecutive frames; every gap over 30 ms is logged with the phases of the previous frame (`frame hitch: …`).

**Result in the real window** (`anno1503 --view …/01_1024_easy_256.sze`, enhanced water, window 1280 × 800 on the 500 Hz display, scripted zoom: one step every 3–40 frames for 16–60 s):

| limits | frames per second | hitches > 30 ms | worst busy frame | worst gap |
|---|---|---|---|---|
| library defaults (`ANNO_VIEW_LIMITS=default`) | 108–183 | 226 in 16 s | 50–122 ms | 53–164 ms |
| `interactive()` | 494–501 | 1–2 (the first frame after the start: pipelines, sea pattern, first chunks, 36–44 ms) | 1.1–4.4 ms | 4.3–5.2 ms (11–16 ms in some windows while other jobs ran, once 125 ms between two frames outside `draw()` while the machine was busy) |

At 3840 × 2160 the bench shows p50 0.5–1.2, p95 1.6–4.9, p99 2.6–9.3 ms and a maximum of 6.5–27 ms. What is left there is command recording (`wgpuCommandEncoderFinish` takes 2–9 ms with about 45 chunks of 5 textures each) and GPU memory events while other processes load the card; neither is specific to zooming.

Not done: the first enhanced frame still creates the wave solver's pipelines on the window thread (3–9 ms, once); after a zoom change the coast shows the analytic waves for 0.1–0.4 s until the new simulation window is ready (listed as a decision for the owner in PLAN §7).

### 2026-10-18 — C++ GPU compositor (wgpu-native 29) vs Rust (wgpu 30), Apple M5 Pro (Metal), `Single_01.sze`
`--res 3840x2160 --styles original,enhanced --chunks async --renderer gpu`, all zoom groups, run one after the other. Every C++ row fits 60 Hz in 98–100 % of frames (p99 ≤ 17.4 ms, max ≤ 19.4 ms). p50 in ms, Rust → C++:

| group | water | static | pan slow | pan fast |
|---|---|---|---|---|
| 0 | original | 2.2 → 3.2 | 1.1 → 2.8 | 1.8 → 4.0 |
| 0 | enhanced | 2.4 → 2.7 | 2.4 → 2.7 | 4.7 → 5.4 |
| 1 | original | 1.2 → 1.7 | 1.2 → 1.7 | 1.2 → 4.0 |
| 1 | enhanced | 2.9 → 4.0 | 2.9 → 4.0 | 5.3 → 6.3 |
| 2 | original | 1.2 → 1.6 | 1.1 → 1.5 | 0.9 → 1.4 |
| 2 | enhanced | 4.5 → 5.2 | 4.6 → 5.2 | 5.5 → 6.5 |

The CPU stages (uploads, sprites, encode) are equal within 0.1 ms; the whole difference was in stage 4, waiting for the GPU (e.g. group 0, original, pan slow: 2.6 against 0.9 ms). **Cause** (second review, measured): wgpu-native 29 uses wgpu-hal 29.0.3, whose Metal fence wait checks the command buffer in a loop with `thread::sleep(1 ms)` (`metal/device.rs`); wgpu-hal 30 (the Rust reference) waits on a condition variable. Patching the Rust build back to the 1 ms loop reproduced the gap. `GpuFrame::wait` now spins on non-blocking polls until `wgpuQueueOnSubmittedWorkDone` answers (`wait_idle`, `src/render/src/wgpu.cpp`). The viewer never waits per frame (vsync paces it), so only the benchmark's stage 4 and `--check` were affected. After the change, 3 runs each, medians, same options (machine load ≈ 8 from other work during both), p50 (stage 4) in ms, Rust → C++:

| group | water | static | pan slow | pan fast |
|---|---|---|---|---|
| 0 | original | 1.7 (1.6) → 1.7 (1.9) | 1.4 (1.1) → 1.5 (1.4) | 2.2 (1.9) → 2.6 (2.2) |
| 0 | enhanced | 2.5 (2.7) → 2.8 (2.9) | 2.7 (2.4) → 2.9 (2.8) | 5.3 (4.1) → 6.6 (5.0) |
| 1 | original | 1.3 (1.0) → 1.4 (1.2) | 1.4 (1.0) → 1.5 (1.2) | 1.3 (1.0) → 1.5 (1.3) |
| 1 | enhanced | 3.2 (3.0) → 3.6 (3.4) | 3.4 (3.0) → 3.9 (3.5) | 7.2 (6.1) → 7.0 (6.1) |
| 2 | original | 1.5 (1.2) → 1.4 (1.1) | 1.4 (1.1) → 1.4 (1.1) | 1.2 (1.0) → 1.2 (1.0) |
| 2 | enhanced | 6.2 (5.6) → 6.0 (5.5) | 7.1 (6.3) → 7.1 (6.4) | 8.1 (7.3) → 8.0 (7.0) |

C++ is now within 0.5 ms of Rust except group 0 enhanced pan fast (+1.3 ms); not investigated further. The first C++ version uploaded chunks byte by byte (about 3 M `push_back`s per 512² chunk, up to 6 per frame) and showed p95 of 14–19 ms and p99 up to 25 ms in static and panning rows; filling the upload buffers in place removed those spikes (numbers above).

### 2026-10-18 — C++ `bench_frame` vs Rust, Apple M5 Pro, `Single_01.sze`
Both benchmarks with the same options, run one after the other: `--threads 8 --res 1920x1080 --chunks blocking,async --quick`, all zoom groups, both water styles; one other core was busy with a test run during both. The C++ benchmark loads the scene as the viewer does (5 ships added; same largest island). p50 / p99 in ms, Rust → C++ (selection; the full tables come from the commands above):

| group | water | chunks | path | Rust p50 / p99 | C++ p50 / p99 |
|---|---|---|---|---|---|
| 0 | original | async | static | 3.0 / 3.9 | 2.6 / 3.0 |
| 0 | original | async | pan fast | 4.9 / 5.1 | 3.5 / 4.4 |
| 0 | enhanced | async | pan fast | 9.7 / 11.8 | 9.0 / 10.4 |
| 1 | original | blocking | pan fast | 3.6 / 24.8 | 2.8 / 26.4 |
| 1 | enhanced | async | static | 18.0 / 23.0 | 17.0 / 18.3 |
| 2 | original | async | static | 3.3 / 3.7 | 2.7 / 3.3 |
| 2 | original | async | pan fast | 4.2 / 5.5 | 3.6 / 4.4 |

Over all 42 non-cold rows, C++ p50 is 0.71–0.98 × Rust's (median 0.90). By stage: sea 0.4–4.3 ms lower in C++; ground and objects within ±0.2 ms; water objects between 0.5 ms lower and 0.6 ms higher. Cold frames with blocking chunk building are about the same at groups 0 and 1 (C++ 39–56 ms, Rust 40–53 ms) but slower in C++ at group 2 (58 / 71 ms against 44 / 60 ms; chunk time 54 / 54 ms against 41 / 42 ms). Not yet explained; one candidate is that the C++ scene has the 5 ships (the Rust benchmark builds its scene without units), which adds an objects layer to open-sea chunks. The pixels of the two scenes are therefore not identical. Single runs with background load: differences of a few tenths of a millisecond are noise. To do: load units in the Rust benchmark or not in the C++ one, then repeat several runs.

### 2026-10-18 — C++ viewer, CPU path, Apple M5 Pro (18 cores), `Single_01.sze`
`anno1503 --view` (C++, SDL3 window, release build, vsync at 120 Hz, window 2560 × 1600 pixels; chunk workers on 9 threads, frames on 18), average of the 2 s report after the chunks are built, original water:

| group | frame avg / worst (ms) | ground | water objects | sea | objects |
|---|---|---|---|---|---|
| 2 | 3.9 / 4.5 | 0.3 | 1.5 | 1.8 | 0.2 |
| 1 | 4.4 / 7.6 | 0.3 | 1.6 | 2.3 | 0.2 |
| 0 | 4.5 / 4.9 | 0.3 | 1.7 | 2.3 | 0.2 |

Same pipeline and pixels as the Rust CPU path (`docs/render/iso.md`, "Fidelity"); comparable to its 1080p/18-thread numbers above for 2 × the pixels. The C++ `bench_frame` (section above) measures panning and other resolutions; not measured yet on Windows/Linux.

### 2026-10-17 (4) — wave simulation at 16 / 8 / 4 cells per tile

GPU, async chunks, enhanced water, after 2 s warm-up, p50 / p99 ms: 3840 × 2160: 2.9–5.0 / ≤ 12.6; 1920 × 1080: 1.2–1.6 / ≤ 8.4 (window sized to the view, `dt` 1/120 s).

### 2026-10-17 (3) — wave simulation, Apple M5 Pro (Metal), `Single_01.sze`

3840 × 2160, GPU, async chunks, after 2 s warm-up, p50 ms: original water 1.8–3.0, enhanced water with the 1024² wave simulation 2.0–3.7 (≈ +1 ms); p99 ≤ 13 ms in all cases.

### 2026-10-17 (2) — GPU compositor, Apple M5 Pro (18 cores, Metal), `Single_01.sze`, commit 04f49c4

After the colleague's second recording (Linux, 4K, high-end PC, CPU compositor: 20–49 fps, 17–47 ms per frame, islands slow to appear when zooming). Viewer mode (`async` chunks), frame time in ms, p50 / p99 (max), ranges over zoom groups 0–2:

| renderer | resolution | water | static | pan fast |
|---|---|---|---|---|
| GPU | 1920×1080 | original | 0.4–0.6 / ≤ 2.5 (10.1) | 0.3–0.7 / ≤ 3.8 (4.2) |
| GPU | 1920×1080 | enhanced | 0.4–0.6 / ≤ 2.9 (3.0) | 0.4–0.6 / ≤ 2.7 (3.0) |
| GPU | 3840×2160 | original | 0.8–1.2 / ≤ 7.0 (8.0) | 0.7–1.1 / ≤ 5.8 (6.5) |
| GPU | 3840×2160 | enhanced | 1.1–1.3 / ≤ 6.0 (7.6) | 1.1–1.2 / ≤ 5.3 (6.5) |
| CPU, 4 threads | 3840×2160 | original | 8.7–13.6 / ≤ 14.9 | 10.5–13.1 / ≤ 18.6 |
| CPU, 4 threads | 3840×2160 | enhanced | 14.1–35.7 / ≤ 42.3 | 22.3–35.4 / ≤ 38.7 |

- `--check`: the GPU frame equals the CPU frame within 1 per channel for the original water at every zoom group and resolution; with enhanced water only foam edges differ (per-pixel vs. grid evaluation).
- The worst GPU frames come from uploading newly built chunks (at most 12 per frame); waiting for the GPU is ≈ 0.5–2 ms at 4K.
- Chunk building (blocking, 40 chunks at 4K): 186 ms → 24 ms with 18 threads after removing lock contention and per-chunk whole-island work (one thread: 228 ms, 5.7 ms per chunk).
- The reworked enhanced water is more expensive on the CPU fallback (30–42 ms at 4K with 4 threads); with a GPU it costs < 0.5 ms.
- Still to measure: the colleague's PC (`run-benchmark` in the 2026-10-17b+ package), Windows/DirectX 12, integrated GPUs.

### 2026-10-17 — Apple M5 Pro (18 cores), `Single_01.sze`, commit a8cebc0

Full tables (both chunk modes, all paths) are produced by the command above; summary of the viewer's mode (`async`), frame time in ms, p50 / p99 (max):

| threads | resolution | water | static | pan fast | worst stage |
|---|---|---|---|---|---|
| 18 | 1920×1080 | original | 2.5–2.7 / ≤ 5.3 | 1.5–3.7 / ≤ 5.1 (5.6) | water objects 1.6–2.5 |
| 18 | 1920×1080 | enhanced | 2.6–3.1 / ≤ 7.9 | 4.5–5.6 / ≤ 8.4 (9.1) | sea 1.7–4.0 |
| 18 | 3840×2160 | original | 6.5–8.4 / ≤ 10.7 | 5.1–9.0 / ≤ 12.5 (12.9) | water objects 3.9–5.1 |
| 18 | 3840×2160 | enhanced | 6.7–17.6 / ≤ 24.4 | 12.4–20.5 / ≤ 24.1 (24.3) | sea up to 17.0 (group 2) |
| 4 | 1920×1080 | original | 3.1–3.3 / ≤ 4.6 | 3.0–4.8 / ≤ 6.8 (7.2) | sea 2.1–2.9 |
| 4 | 1920×1080 | enhanced | 3.2–4.2 / ≤ 12.6 | 6.8–12.9 / ≤ 16.2 (18.2) | sea up to 11.3 |
| 4 | 3840×2160 | original | 10.8–15.5 / ≤ 19.2 | 10.9–15.5 / ≤ 22.2 (22.3) | sea up to 10.2, water objects up to 6.4 |
| 4 | 3840×2160 | enhanced | 11.3–37.6 / ≤ 44.7 | 22.0–43.9 / ≤ 48.1 (48.3) | sea up to 40.6 |

Ranges span zoom groups 0–2. Findings:
- **Chunk building no longer causes hitches.** With `blocking` chunks the same paths have p95/p99 of 25–64 ms and cold frames of 75–230 ms (each 512² chunk then cost ≈ 12 ms on one core; corrected from "5–7 ms" — see the later section); with background workers the worst pan-fast frame at 1080p is 5.6 ms (18 threads) / 7.2 ms (4 threads).
- **Before this work** (2026-10-16 viewer): a 1080p view took ≈ 500 ms of ground rendering per new chunk area and 8–30 ms of sea sprites per frame, redrawn at most every 50 ms.
- **Against the targets (PLAN §0.3):** 1080p meets them in every case, including 4 threads. 4K with 18 threads meets them for original water; with 4 threads it misses at p99 for original water (≤ 22 ms) and clearly for enhanced water (≤ 48 ms).
- **Next costs to cut:** (1) enhanced wave shading (`sea` stage; per grid point several noise and trigonometric evaluations) — cheaper wave function, coarser grid at group 2, or the GPU; (2) water objects (rivers, surf) are re-blitted as sprites every frame (up to 6 ms at 4K) — cache them per animation step like the sea; (3) chunk workers compete with frame threads while panning (`static` p99 > p50 while the ring is built) — lower worker priority. A GPU compositor (wgpu) would remove the per-pixel stages from the CPU entirely; decide after (1)–(3) are measured on the colleague's PC.

### 2026-10-20 - stand-in chunks after a zoom step (T1, OQ-077), Windows 11, Ryzen 9 9800X3D, RTX 4090 (Vulkan), MSVC Release, branch task/T1-zoom-standin

Method: `bench_frame <game_dir> <game_dir>/Scenes/0010SingleEndlos/01_1024_easy_256.sze --zoom 6 --renderer gpu --chunks async --limits interactive --res 2560x1369 --styles enhanced`, window-thread CPU time per frame. Before = the build of main (`7d24231` plus docs), after = this branch (commit `57e316e`, with the per-part source choice). Three runs of each, alternated. The machine was shared with two other tasks' builds and tests: CPU load at the start of the runs 13-38 % (first pair run 85 % in one case), and the spread between runs of the same build is larger than any difference found. Medians of the three runs, p50 / p95 / p99 / max in ms (min-max of the three runs in brackets for p50 and max):

| group | before | after |
|---|---|---|
| 0 | 0.4 [0.4-0.4] / 1.8 / 2.9 / 8.6 [3.8-25.9] | 0.5 [0.5-0.5] / 2.1 / 5.3 / 19.2 [4.5-31.6] |
| 1 | 0.4 [0.4-0.9] / 1.6 / 2.0 / 5.5 [2.3-14.5] | 0.6 [0.5-0.7] / 2.2 / 2.7 / 3.5 [3.2-17.6] |
| 2 | 0.4 [0.4-0.6] / 2.0 / 3.4 / 7.0 [5.2-15.2] | 0.7 [0.5-0.7] / 2.7 / 9.1 / 19.1 [3.5-34.4] |

Read with care: p50 of the after runs is 0.1-0.3 ms above the before runs in all three groups, and p99 and max are higher in the median, but the third after run (0.5 / 2.1 / 3.2 / 4.5; 0.5 / 1.8 / 2.6 / 3.2; 0.5 / 1.6 / 2.4 / 3.5) is as quiet as the before runs, and the slow frames of the after runs are the fluid/encode and upload `write` stages that the before runs show too. A same-binary comparison (`--stand-ins off` against `on`, four alternated runs each, one off run unusable, the other tasks still running, load 19-82 %) shows no cost: p50 off 0.5-0.8 ms (median 0.6), on 0.5-0.8 ms (median 0.5); medians of p95 off 2.4 / on 2.0, p99 off 7.6 / on 4.0. So the p50 gap between the before and after builds is not the feature (a different build and moment on a loaded machine); a small cost cannot be excluded either. The per-frame cost of the feature is O(visible chunks not on the GPU) lookups plus one draw per source-chunk part per pass, only while chunks are missing. No slow frame is tied to a stand-in draw.

What the zoom path shows (printed by `bench_frame` after the table): frames with chunks not built 52 of 240 (group 0 start, 43 drew stand-ins), 48 (group 1, 34 drew stand-ins); of the first frames right after a zoom step with chunks not built, 8 of 9 and 6 of 8 drew stand-ins (earlier run: 16 of 16 for the group 2 start); the others had no source chunk on the GPU for the missing area (zooming out shows area nobody has seen yet). `--stand-ins off` gives the old behaviour in the same binary. PNG pairs of the first frames after zoom steps: `C:/Users/altin/Pictures/anno-review/T1/` (`first_frame_after_zoom_out_*.png`: the island is open sea before, the scaled island after; `all_before` and `all_after`: the first eight frames after zoom steps of one run each; frames that were complete in both runs differ only by the animation).

### 2026-10-20 - T1b step 1: every window-thread step of a GPU frame is timed, Windows 11, Ryzen 9 9800X3D, RTX 4090 (Vulkan), MSVC Release, branch task/T1-zoom-standin

**Instrumentation (permanent).** `GpuStats::steps()` lists the steps of `GpuFrame::render` laid end to end (chunk receipt, `ensure_under`, sea upload, `visible_chunks`, `sync_chunks` split into scan / drops / real uploads / placeholders, sprites, command encoder, wave simulation, uniforms and frame bind groups, draw list with the stand-in cover, piece buffer write, pass recording, `finish`, error-scope pop, `wgpuQueueSubmit`, command buffer release); their sum is `total_ms` (a doctest checks it, and the drop and placeholder counters). The viewer adds its own steps (draw prologue with the surface configuration, surface view, render exit, present, tail) and every `frame hitch:` line prints the steps of the previous and of this frame with their sum and "busy minus sum", which is 0.00 ms in every frame measured. **That zero is by construction, not evidence:** the steps are laps of one clock, so they add up to the frame; `sync scan` and `release` are residuals (sync time minus its named parts, submit lap minus the submit parts) and take whatever is left. The sum shows that no interval inside `render()` is unnamed, and the residuals are printed in every hitch line; it does not show that everything the owner sees is inside `render()`. Independent checks: the viewer's wall clock (busy = `draw()` start to end minus the acquire wait; the gap between frame starts; the time from the end of one frame to the start of the next, with the time in window events) and, in the doctest, an outside stopwatch around `render()`. (the surface acquire is the only part of a frame outside the steps: it is the wait for the display). Every report window also prints the steps of its worst frame. `ANNO_VIEW_HITCH_MS` lowers the 30 ms threshold of the hitch line. `bench_frame` lists the steps of the slow zoom frames (`--spike ms`), with the number of dropped GPU chunks, planes freed on the window thread, placeholders and whether the piece buffer grew.

**Where a dropped GPU chunk goes.** Not to a reaper thread. `GpuFrame` drops a chunk in `Impl::drop_chunk`: its planes go to `plane_pool` (at most `kPoolPlanes` = 192, reused by later uploads); planes the pool has no room for, the chunk's uniform buffer and its bind group are released on the window thread by `chunks.erase` (`planes_freed` counts the planes). Only the CPU-side cache goes to the reaper (`FrameRenderer::trim` -> `ChunkWorkers::discard`, with background workers). No dropped plane was freed in any run below (`freed 0`).

**What the steps show (viewer, 1280 x 800, `D_256x256_0045_5B`, scripted zoom `ANNO_VIEW_ZOOM_SCRIPT=6,16..20` and wheel messages posted to the window; `bench_frame --zoom 6 --renderer gpu --chunks async --limits interactive --styles enhanced`, stand-ins on and off in the same binary, 3-4 alternated runs per case; CPU load 3-26 % from the owner's browser and music player, no build or other benchmark running):**
- The 26-39 ms frames did not reproduce after a zoom step: the worst frames of 12 bench runs on the owner's island (1280 x 800 and 2560 x 1440, group 0 start) are 2.0-4.4 ms and no frame exceeded 8 ms apart from the first frame of the program; the viewer runs (scripted and wheel) show worst frames of 1-6 ms after the first frame.
- The first frame of the program: sea pattern upload 24-31 ms in the viewer (`ensure_sea` uploads all steps of the start group in the frame), 14-20 ms sea plus 27-34 ms wave simulation creation in `bench_frame` (frame 0, 44-50 ms).
- Window resize: `draw prologue` 6.9 ms (surface reconfiguration).
- `wgpuQueueSubmit` 2-6 ms in single frames (5.4, 5.9, 6.1, 4.9 ms), also with stand-ins off.
- The largest variable cost after a zoom step is **texture creation for the first chunk uploads** (`sync uploads`, of it `create`): `Single_01.sze` 2560 x 1369, first frame after the first zoom step to group 1, stand-ins on: 14.0 ms (1 chunk, `create` 13.1), 8.3 ms (3 chunks, `create` 6.5), 4.2 ms (3 chunks, `create` 1.0), none in 2 of 5 runs; 3 of 4 stand-ins-on runs of a second series against 0 of 4 off runs (weak: the frame in which the first group-1 chunk is uploaded differs). The upload time budget (1 ms) cannot cut it short because the first chunk of a frame always goes. This is the step that holds up to 13 ms of a frame; with enhanced rivers a chunk has three more planes, so more than that is possible, but 26-39 ms did not show on this machine.
- Placeholders (`upload_chunk(key, nullptr)`): 12-18 in the first frame, 0.13-0.32 ms in total; the piece buffer never grew mid-run after the first frame; stand-in cover under 0.1 ms; drops under 0.2 ms.

**T1b step 2: what was fixed, and what the numbers say (same binary, `ANNO_BACKGROUND_PRIORITY=normal` and `bench_frame --prewarm 0` give the old behaviour; `bench_frame --zoom 6 --renderer gpu --chunks async --limits interactive --styles enhanced --zoom-frames 480`; the machine was NOT idle: 16-68 % CPU load from the owner's other programs, no build or other benchmark running, the load is written next to the runs).**
- **Texture creation of the first uploads into a zoom group** (the step named in step 1): on `Single_01.sze` 2560 x 1369 the first frame after the zoom into group 1 spent 10-19 ms in `sync uploads` (`create` 10-17 ms for 2-3 chunks) in about 4 of 6 runs; a view's worth of chunks (24 at this size) is needed at every group, and the CPU cache holds the chunks of the groups before, so no plane comes back from a drop. `GpuLimits::prewarm_chunks` (24 in `GpuLimits::interactive()`, 0 elsewhere; `bench_frame --prewarm n`) lets a background thread (below normal priority) keep that many chunks' planes ready (five 512 x 512 textures, about 4 MB per chunk, 96 MB) and refill as uploads take them; the window thread takes with `try_lock` and never waits. After the change: the group-1 frame is not among the frames above 6 ms in 6 of 6 runs (the version with a one-shot fill, which the first group consumed completely, still showed it: that is why the pool refills). Test: "pre-created chunk planes are taken by the first uploads, and an early end joins the thread".
- **A window thread that is descheduled.** With the texture creation gone, the remaining frames above 8 ms (about 1 per 8 runs of 3 x 480 frames) were 8-11 ms in steps that are a few microseconds of map and set work otherwise (`visible` 9.9, `sync scan` 8.8, `chunks` 9.3 ms, also `finish` 8 and `queue submit` 8-14 ms), in different steps and different frames: the chunk builders (half the cores), the wave setup and the reaper ran at the window thread's priority. All background threads now run below normal (`thread_priority.hpp`, the helper river flow builders already used); `ANNO_BACKGROUND_PRIORITY=normal` switches it off for a measurement. Alternated, `Single_01.sze` 2560 x 1369, 8 runs each, worst frame of the row per group: normal priority 7.3, 11.7, 6.6, 6.4, 6.2, 5.8 ms in 6 of 24 rows; below normal never above 6.4 ms in 24 rows (rows in ms: g0/g1/g2). A weak result (one frame of 8 runs, loaded machine), kept because it is cheap and has no look-and-feel stake.
- A chunk replaced by its rebuild (the river flow arrived) is freed on the reaper thread like the evicted ones (it was freed inside `chunks`).
- **Acceptance table** (`D_256x256_0045_5B`, enhanced, GPU async interactive, groups 0/1/2 start, 480 frames each, 5 alternated runs): worst frame per row, ms, load 16-46 % (1280 x 800) and 19-40 % (2560 x 1440). 1280 x 800: below normal 2.1-2.7 (all 15 rows), normal 2.0-6.2. 2560 x 1440: below normal 2.2-3.2 in 14 of 15 rows, one frame of **12.9 ms** (frame 13 of the group-2 start, not reproduced in 8 further runs, load 30 %); normal 2.2-6.5. So the acceptance (no frame above 8 ms after a zoom step on the owner's island) holds in 29 of 30 rows; the 30th is named only as one unexplained frame.
- **Not fixed, named:** the very first frame of the program (sea upload 24-31 ms, wave simulation creation 27-34 ms; excluded by the acceptance); `wgpuQueueSubmit` 8-14 ms in single enhanced frames (not seen with original water in 6 x 480 frames, still seen with enhanced water at normal priority; cause not found: GPU or driver work behind the submit, not something the window thread does); the viewer ignored `--water enhanced` (only `--png` used it), so the owner's check command started in original water until `E` was pressed; fixed in the finalize entry below.
- The owner's own 26-39 ms frames did not reproduce on this machine either before or after; the changes above remove the two causes that did show here. To name the rest on the owner's machine: `ANNO_VIEW_HITCH_MS=8` in enhanced mode (key `E`), paste the `frame hitch:` lines.
- Also seen once in 40 runs: the `fluid` step at 9.4 ms (frame 111, group 2; the wave simulation's budgeted upload slice and step recording on the window thread, before the priority change). Not investigated further.

### 2026-10-20 — T1b step 3: the artifacts after a zoom step in enhanced water (measured, not fixed)

`D_256x256_0045_5B`, enhanced, GPU async interactive, 1280x800 and 2560x1440, start group 1, `bench_frame --zoom 25|100 --zoom-dump <dir> --zoom-dump-after 0,1,2,3,5,8,12,20` (new: the frames a given number of frames after each step, `zoom_<step>_f<frames>_g<group>_<w>x<h>.rgba`; the run also prints "transients per zoom step", wall time since the step until the last frame that still had missing chunks, stand-ins, or a wave window being prepared). Machine load 20-37 % (the owner's other programs), nothing building. Evidence PNGs: `C:\Users\altin\Pictures\anno-review\T1b\`.

| 2560x1440, zoom step every 25 frames (about 48 ms) | chunks missing | stand-ins drawn | wave window being prepared |
|---|---|---|---|
| first visit of a group (steps 0, 2, 4, 6, 10) | 25 frames / 46-48 ms (the whole interval) | same | 20-25 frames / 39-48 ms |
| revisit of a built group | - | - | 18-20 frames / 36-39 ms |
| 1280x800, first visit | 11-25 frames / 25-46 ms | same | 9-10 frames / 14-15 ms |
| 1280x800, revisit | - | - | 6-9 frames / 9-15 ms |
| 2560x1440, zoom step every 100 frames, first group-0 visit | 37 frames / 69 ms | same | 19 frames / 40 ms |

Suspects of the task text:
- **(a) rivers on a stand-in use the legacy shimmer, then flip to flow shading: real, small.** Between a stand-in frame and the built frame the river pixels differ by 2.5-8.6 (sum over channels) against 1-3 for the animation alone; legacy against flow on whole-island renders at group 1 and 2 differ in 0.003-0.006 % of the pixels by more than 30 (about 4,000 pixels of 60 million, the rivers of this island are thin). At the default strength the flip is hard to see next to (c). Not changed: drawing the flow on a stand-in needs the source chunk's lattice scaled on the GPU and the same on the CPU reference (`stand_in_chunk` builds pixels, not a lattice), which breaks "GPU = CPU" for stand-ins unless both are written; a cross-fade needs the arrival time per chunk. Offered as an owner decision (OQ-091).
- **(b) wave window rebuilt after a zoom: real, visible on open sea.** The window job is in flight for 9-25 frames (14-48 ms) at every step, also on revisits. While it runs the previous window keeps its place; on the zoom-out to group 2 the open sea shows a diamond of animated waves (the old window) against the flat analytic sea outside it, and the diamond is gone 5 frames later (`b_wave_window_*.png`; it appears with `--stand-ins off` too, so it is not the stand-ins). Not fixed (OQ-076, owner decides cross-fade / snap / hold).
- **(c) nearest-neighbour stand-ins next to built neighbours: the most visible one.** Frame 0 after the zoom into group 0: 16 % of the pixels differ from the settled frame by more than 60 (sum over channels), mean 25.7; the blocky 2x areas stay for 11-37 frames (25-69 ms) where chunks have to be built, 2-4 frames where they only wait for the upload. With `ANNO_VIEW_STAND_INS=off` the same frames show open sea instead: 65 % of the pixels differ by more than 60 at frame 0 and 25 % until the chunks land (`c_*` PNGs), so the stand-ins are the smaller artifact of the two. With `--water original` the stand-ins are equally blocky (`c_original_water_*`), the river and wave differences are gone. Pixels no built group covers (the area outside the previous view) are open sea in both modes.

### 2026-10-20 - T1b finalize: viewer water fix, bench on a machine that was not idle, PNG pairs regenerated

**The machine was not idle and could not be made idle.** The owner's game (`FC27`) ran for the whole session at 50-82 % total CPU load (minimum over two minutes of polling: 50 %); it was not stopped (the owner's process). Every number below is therefore a loaded-machine number, with the load next to it, and the acceptance item "idle-machine bench table" is **not met**. To redo it: close the game and repeat the commands below; nothing else needs to change.

`D_256x256_0045_5B`, enhanced, `bench_frame --zoom 6 --renderer gpu --chunks async --limits interactive --styles enhanced --res <res> --zoom-frames 480 --stand-ins off|on`, one binary (`task/T1-zoom-standin`, commit of this entry), five alternated runs per resolution, load at the start of each run 57-82 % (1280 x 800) and 62-74 % (2560 x 1440). Window-thread CPU time per frame, medians over five runs of p50 / p95 / p99 / max in ms; in brackets the number of the five runs whose max exceeded 8 ms (and the largest max):

| res | group | stand-ins off | stand-ins on |
|---|---|---|---|
| 1280x800 | 0 | 0.4 / 1.7 / 3.5 / 6.4 [0] | 0.5 / 2.1 / 3.5 / 5.7 [1, 15.9] |
| 1280x800 | 1 | 0.4 / 1.6 / 2.9 / 6.2 [2, 9.5] | 0.5 / 1.7 / 2.8 / 5.2 [0] |
| 1280x800 | 2 | 0.4 / 1.8 / 3.0 / 5.8 [0] | 0.5 / 1.7 / 3.1 / 6.0 [2, 10.9] |
| 2560x1440 | 0 | 0.5 / 2.0 / 3.6 / 6.6 [0] | 0.8 / 2.6 / 4.5 / 7.7 [2, 12.8] |
| 2560x1440 | 1 | 0.6 / 1.9 / 3.1 / 7.3 [2, 9.0] | 0.7 / 2.4 / 3.5 / 6.5 [1, 8.3] |
| 2560x1440 | 2 | 0.6 / 2.0 / 3.3 / 6.9 [0] | 0.7 / 2.4 / 3.4 / 6.7 [1, 10.5] |

Reading: with stand-ins off the same binary also has runs with a max above 8 ms (4 of 30 runs with off, 7 of 30 with on), so a max above 8 ms is the loaded machine, not the feature; the difference is inside the run-to-run spread (max 1.9-15.9 ms). The one consistent difference is **p50: +0.1 ms at 1280 x 800 and +0.1-0.3 ms at 2560 x 1440** with stand-ins on (the draw list and the cover are built every frame in which a stand-in is possible; no per-stand-in cost was found). That cost is real but small. The earlier p99/max regression (group 0 2.9/8.6 -> 5.3/19.2, group 2 3.4/7.0 -> 9.1/19.1) is **not reproduced**: here p99 is 2.8-4.5 ms, p99 on against off differs by at most 0.9 ms (2560 x 1440, group 0), and large maxima appear with stand-ins off too (one off run had a frame with `finish` at 10 ms and nothing else slow). It is therefore attributed to the loaded machine only in the sense of "not reproduced"; an idle run is still needed to close it.

**PNG pairs regenerated** after the per-part source choice (the earlier pairs predate it): `C:\Users\altin\Pictures\anno-review\T1\v2_first_frame_after_zoom_in_g0_{before,after}.png` (56 % of the pixels differ by more than 30 summed over channels: before = open sea, after = stand-ins), `v2_first_frame_after_zoom_out_g1_{before,after}.png` (22 %), `v2_first_frame_after_zoom_out_g2_{before,after}.png` (0 %: the group-2 chunks were already built, nothing to cover; kept to show a revisit is unaffected). Made with `bench_frame ... 01_1024_easy_256.sze --zoom 6 --res 2560x1369 --groups 2 --zoom-frames 60 --stand-ins off|on --zoom-dump <dir> --zoom-dump-after 0,3,20`, before = off, after = on, same binary.

**Fixes of this step.** (1) The viewer starts in the water style of `--water` (`run_viewer` took none; the owner's check command always started in original water, so owner observations "in enhanced mode" came from pressing `E`). (2) The thread that pre-creates chunk planes was meant to stop when the device refuses a texture, but the null check written for it can never fire under wgpu-native (a failed creation returns a non-null handle with an error id); corrected in T1c: it compares the uncaptured-error counter before and after each creation and stops at the first change (no error scope: the scope stack is per device and shared with the window thread). Not covered by a test: no way to make the device refuse a 512 x 512 texture in a unit test. (3) The doctest "the steps of a GPU frame add up..." gets an outside stopwatch around `render()`. Mutation checks redone for the prewarm tests: removing `++planes_warm_taken` fails `CHECK( st.planes_warm >= 1 )` in under a second; setting the refill target to 0 fails `warm_planes_ready() == 10` after the test's own bounded 10 s wait; neither stalls. The 30-minute stall a reviewer saw was not in these tests (every wait in them is bounded to 10 s) and is not reproduced. A mutation that drops only the `h` comparison in `take_warm` cannot be caught through the public API (all chunk planes are `kChunk` x `kChunk`), so it is not tested; the comparison is defence against a future second plane size.

### T1c (2026-10-20): review fixes to the stand-in branch

- **Pre-creation thread (items 1, 2, 5).** It runs at normal priority and creates one plane per quiet frame (a token from `render()` when the frame uploaded nothing and made no placeholder; unpaced only until the first `render()`); it detects a refused texture by the uncaptured-error counter delta (the earlier null check could not fire under wgpu-native; no error scope: the scope stack is per device and shared with the window thread); its target is `prewarm_chunks * 5` minus the pooled planes, within `GpuLimits::idle_plane_budget_mb` (256); the viewer sizes `prewarm_chunks` from the window in chunks (never below 24). Switches: `ANNO_VIEW_PREWARM=off`, `ANNO_BACKGROUND_PRIORITY=normal`.
- **Retention (item 3).** `FrameRenderer::stand_in_retain` is empty unless stand-ins are on (`GpuLimits::stand_ins`, set on the renderer by `render()`, or the CPU switch) and only for visible chunks not built: with stand-ins off the eviction is the one from before the feature.
- **Placeholders (item 4): not changed.** Measured in T1b: 12-18 placeholders in the first frame cost 0.13-0.32 ms in total (about 0.02 ms each), so they are not the hitch. Removing them is wrong as well: a placeholder draws the sea pattern through the chunk shader (a GPU-vs-CPU test of the open-sea case failed by up to 38 per channel when no placeholder was drawn). A slab would need a change of the chunk bind group layout for no measured gain.
- **Console write (item 7).** The per-frame `chunk(s) received` line is gone (count in the periodic report). The owner's terminal is where this matters; the development runs wrote to a pipe. Owner check: run the viewer with the zoom as before and compare `worst` in the periodic lines.

#### T1c idle-machine bench (replaces the loaded-machine table of T1b)

`D_256x256_0045_5B`, enhanced, RTX 4090 + Ryzen 9800X3D, `bench_frame --zoom 6 --renderer gpu --chunks async --limits interactive --styles enhanced --res <res> --zoom-frames 480 --stand-ins on|off --prewarm 0|24`, one binary (`task/T1-zoom-standin` after the T1c commits, before this entry), five rounds, every round running both resolutions and the four configurations in turn. Nothing else of ours was running; the owner's game was closed. CPU load read just before each run (`Win32_Processor.LoadPercentage`, which includes the reading itself and background programs): 2-36 %: 36 of the 40 runs below 20 % (most 7-18 %), four at 27, 31, 35 and 36 %. That is quiet but not the "below 10 %" the task asks for, so read the numbers as an idle machine with a few percent of background noise. Window-thread CPU time per frame, medians over five runs of p50 / p95 / p99 / max in ms; "runs with max > 8 ms" counts the five runs (worst max in brackets):

| res | group | stand-ins | prewarm | p50 / p95 / p99 / max (medians of 5) | runs with max > 8 ms (worst max) |
|---|---|---|---|---|---|
| 1280x800 | 0 | off | off | 0.2 / 0.4 / 1.5 / 2.0 | 0 of 5 (3.0) |
| 1280x800 | 0 | off | on | 0.3 / 0.4 / 1.5 / 2.0 | 0 of 5 (2.2) |
| 1280x800 | 0 | on | off | 0.3 / 0.5 / 1.5 / 2.2 | 0 of 5 (2.3) |
| 1280x800 | 0 | on | on | 0.3 / 0.5 / 1.6 / 2.2 | 0 of 5 (2.3) |
| 1280x800 | 1 | off | off | 0.3 / 0.5 / 1.5 / 2.5 | 0 of 5 (2.6) |
| 1280x800 | 1 | off | on | 0.3 / 0.5 / 1.5 / 2.2 | 0 of 5 (2.3) |
| 1280x800 | 1 | on | off | 0.3 / 0.5 / 1.5 / 2.2 | 0 of 5 (2.6) |
| 1280x800 | 1 | on | on | 0.3 / 0.5 / 1.5 / 2.2 | 0 of 5 (2.4) |
| 1280x800 | 2 | off | off | 0.3 / 1.7 / 2.0 / 2.5 | 0 of 5 (6.1) |
| 1280x800 | 2 | off | on | 0.3 / 1.6 / 1.9 / 2.5 | 0 of 5 (2.6) |
| 1280x800 | 2 | on | off | 0.3 / 1.7 / 2.0 / 2.4 | 0 of 5 (3.2) |
| 1280x800 | 2 | on | on | 0.3 / 1.7 / 2.0 / 2.4 | 0 of 5 (2.4) |
| 2560x1440 | 0 | off | off | 0.4 / 1.0 / 1.7 / 2.7 | 0 of 5 (2.9) |
| 2560x1440 | 0 | off | on | 0.4 / 1.0 / 1.8 / 2.4 | 0 of 5 (4.4) |
| 2560x1440 | 0 | on | off | 0.4 / 1.2 / 2.0 / 3.0 | 0 of 5 (3.4) |
| 2560x1440 | 0 | on | on | 0.4 / 1.2 / 1.9 / 2.6 | 0 of 5 (2.8) |
| 2560x1440 | 1 | off | off | 0.4 / 0.9 / 1.7 / 2.5 | 0 of 5 (6.2) |
| 2560x1440 | 1 | off | on | 0.4 / 1.0 / 1.8 / 3.0 | 1 of 5 (12.6) |
| 2560x1440 | 1 | on | off | 0.4 / 1.2 / 2.0 / 2.7 | 0 of 5 (2.9) |
| 2560x1440 | 1 | on | on | 0.4 / 1.2 / 1.9 / 2.6 | 0 of 5 (6.1) |
| 2560x1440 | 2 | off | off | 0.4 / 1.0 / 1.8 / 2.0 | 0 of 5 (2.8) |
| 2560x1440 | 2 | off | on | 0.4 / 1.0 / 1.8 / 2.3 | 0 of 5 (2.8) |
| 2560x1440 | 2 | on | off | 0.4 / 1.1 / 2.0 / 3.1 | 0 of 5 (5.8) |
| 2560x1440 | 2 | on | on | 0.4 / 1.1 / 2.0 / 2.7 | 0 of 5 (3.1) |

- Acceptance "no busy frame above 8 ms after a zoom step": 119 of 120 runs meet it (max 2.0-6.2 ms); the one miss is 12.6 ms (2560x1440, group 1, stand-ins off, prewarm on), not reproduced in the other four runs of that configuration. The first frame of the program is outside the bench.
- The earlier loaded-machine regression (group 0 p99/max 2.9/8.6 to 5.3/19.2 ms, group 2 3.4/7.0 to 9.1/19.1 ms) is **not present**: all medians of p99 are 1.5-2.0 ms and of max 2.0-3.1 ms. It was load.
- Cost of stand-ins: p50 unchanged (0.3 / 0.4 ms); p95 +0.0 at 1280x800, +0.2 ms at 2560x1440 groups 0 and 1 (1.0 to 1.2 ms), max within noise.
- Prewarm on vs off: no difference outside noise in these medians. It is kept because it targets the first uploads into a group (10-19 ms texture creation seen before); this bench starts after the pre-created planes are ready (`pre-created planes ready: 120`), i.e. it measures the steady state the thread leaves, not the first seconds. The viewer's first zoom into an uncached group on the owner's machine is the case still to be checked by the owner (A/B with `ANNO_VIEW_PREWARM=off`).
- Raw outputs: scratch files of the session (`bench/r<round>_<res>_si<on|off>_pw<0|24>.txt`, `loads.txt`), not in the repository.

### 2026-10-20 - T1d: pan while zooming (margin, prefetch, cross-fade)

**What the owner saw:** artifacts only when moving through the map while zooming. **Reproduced** with `bench_frame --zoom 60 --zoom-pan 4,2,400` (the view walks 4 x 2 scene pixels per frame, about 1600 px/s at 2.5 ms frames, reversing every 400 frames, a zoom step every 60 frames): with the T1c code, visible chunks with no source in any zoom group (open sea) in 9-86 of 1600 frames, runs up to 325 ms, and single frame pairs where 14-30 % of the pixels jump by more than 40 levels.

**Cause, measured.** (1) A chunk takes 100-250 ms on one worker thread (blocking cold frame: 12 chunks 118-247 ms with 16 threads), so what the ring of one chunk does not cover arrives late; the queue was ordered by key, not by nearness; the neighbouring groups were only built once the current one was nearly done, and never uploaded, so a zoom step had no stand-in source for the part of the new view that had been off screen. (2) A chunk replacing its stand-in swapped in one frame. The 4.2 ms window-event frame and the `queue submit` spikes of 3.9-5.0 ms named in the board are **not** explained: `SDL_SetWindowTitle` per zoom step costs 0.03 ms (worst 0.09 ms), a wheel event does no per-event work that grows with anything (`group_ready` is a map lookup); they did not reproduce here.

**Changes** (commits `T1d 5`, `T1d 1`, `T1d 2` of the branch): the margin around the view grows in the direction of motion by speed x (2 x the measured build time + 0.1 s), at most `FrameRenderer::max_lookahead_chunks` (3) chunks (`lookahead_rect`); the view a zoom step would show in each neighbouring group gets the same margin, always, queued behind the current group and nearest-first to the view as it will be; the compositor uploads those built chunks when no visible chunk waits (`GpuLimits::prefetch_chunks` 96, one per frame); a chunk that replaces a stand-in or the open sea dissolves in over `GpuLimits::chunk_fade_ms` (150 interactive, 0 elsewhere) on the animation clock (ground and sea pipelines blend by the piece record's `transparent`, terrain switches half-way, objects scale their alpha). Switches for comparing: `bench_frame --prefetch 0 --fade 0` is the T1c behaviour. (Changed in T1e, next section: the fade runs on a steady clock and starts only over a placeholder an earlier frame drew; only the fading draws blend; the "open sea" columns below count whole chunks without any source and miss holes in a stand-in's cover.)

**Measured** on `D_256x256_0045_5B`, enhanced, GPU async chunks, interactive limits, RTX 4090 + Ryzen 9800X3D, three runs per cell (rounds alternating the two configurations), CPU load read before each run 0-29 % (mostly 4-15 %). "pop" = the share of pixels that change by more than 40 levels between two consecutive frames of one group, the previous frame shifted by the pan (`--pop on`; the water's own motion stays below it); worst single frame pair per run.

| res | pan | config | group | p50 / p95 / p99 / max ms (median of 3) | worst max | frames with open sea (median) | longest open-sea run ms (worst) | worst pop % of pixels (median / worst) | pairs above 0.5 % (median) |
|---|---|---|---|---|---|---|---|---|---|
| 1280x800 | 0,0 | T1c (no prefetch, no fade) | 0 | 0.2 / 0.4 / 2.3 / 3.0 | 151.6 | 8 | 16 | 17.91 / 18.69 | 9 |
| 1280x800 | 0,0 | T1c (no prefetch, no fade) | 1 | 0.3 / 0.5 / 2.5 / 4.2 | 4.6 | 11 | 72 | 11.98 / 12.44 | 18 |
| 1280x800 | 0,0 | T1c (no prefetch, no fade) | 2 | 0.3 / 0.5 / 2.5 / 4.3 | 4.5 | 0 | 0 | 10.10 / 10.10 | 16 |
| 1280x800 | 0,0 | T1d defaults | 0 | 0.3 / 0.6 / 2.4 / 3.0 | 130.4 | 0 | 0 | 0.46 / 0.52 | 0 |
| 1280x800 | 0,0 | T1d defaults | 1 | 0.3 / 1.8 / 2.5 / 4.3 | 667.3 | 0 | 0 | 0.46 / 0.49 | 0 |
| 1280x800 | 0,0 | T1d defaults | 2 | 0.3 / 0.6 / 1.5 / 2.5 | 4.0 | 0 | 0 | 0.46 / 0.48 | 0 |
| 1280x800 | 4,2 | T1c (no prefetch, no fade) | 0 | 0.3 / 1.8 / 2.5 / 4.3 | 4.3 | 33 | 73 | 27.86 / 30.50 | 64 |
| 1280x800 | 4,2 | T1c (no prefetch, no fade) | 1 | 0.3 / 1.7 / 2.6 / 5.9 | 5.9 | 19 | 67 | 19.22 / 19.83 | 51 |
| 1280x800 | 4,2 | T1c (no prefetch, no fade) | 2 | 0.3 / 2.0 / 4.0 / 4.4 | 4.5 | 9 | 13 | 12.35 / 17.14 | 86 |
| 1280x800 | 4,2 | T1d defaults | 0 | 0.3 / 1.8 / 2.3 / 4.3 | 5.6 | 0 | 0 | 2.11 / 2.70 | 14 |
| 1280x800 | 4,2 | T1d defaults | 1 | 0.3 / 2.3 / 3.8 / 4.4 | 5.7 | 0 | 0 | 1.48 / 1.76 | 9 |
| 1280x800 | 4,2 | T1d defaults | 2 | 0.3 / 2.1 / 3.7 / 4.3 | 5.9 | 0 | 0 | 1.54 / 2.35 | 12 |
| 2560x1440 | 0,0 | T1c (no prefetch, no fade) | 0 | 0.4 / 0.7 / 2.1 / 3.6 | 3.6 | 15 | 94 | 11.63 / 14.46 | 21 |
| 2560x1440 | 0,0 | T1c (no prefetch, no fade) | 1 | 0.4 / 0.7 / 2.1 / 3.6 | 657.3 | 7 | 1783 | 10.25 / 10.25 | 17 |
| 2560x1440 | 0,0 | T1c (no prefetch, no fade) | 2 | 0.4 / 0.7 / 2.1 / 3.6 | 3.7 | 0 | 0 | 7.18 / 7.18 | 18 |
| 2560x1440 | 0,0 | T1d defaults | 0 | 0.4 / 1.0 / 2.1 / 2.6 | 2.8 | 0 | 0 | 1.38 / 1.39 | 7 |
| 2560x1440 | 0,0 | T1d defaults | 1 | 0.4 / 1.1 / 2.1 / 3.0 | 3.6 | 0 | 0 | 1.40 / 1.41 | 6 |
| 2560x1440 | 0,0 | T1d defaults | 2 | 0.4 / 1.2 / 2.1 / 2.7 | 2.7 | 0 | 0 | 1.27 / 1.28 | 6 |
| 2560x1440 | 4,2 | T1c (no prefetch, no fade) | 0 | 0.5 / 2.1 / 3.4 / 4.7 | 5.1 | 48 | 267 | 14.30 / 14.30 | 89 |
| 2560x1440 | 4,2 | T1c (no prefetch, no fade) | 1 | 0.5 / 2.1 / 3.3 / 6.7 | 9.3 | 31 | 77 | 18.02 / 18.02 | 88 |
| 2560x1440 | 4,2 | T1c (no prefetch, no fade) | 2 | 0.5 / 2.1 / 3.3 / 5.9 | 6.7 | 23 | 325 | 18.01 / 18.01 | 89 |
| 2560x1440 | 4,2 | T1d defaults | 0 | 0.6 / 1.9 / 2.8 / 4.8 | 4.9 | 0 | 0 | 1.48 / 1.56 | 18 |
| 2560x1440 | 4,2 | T1d defaults | 1 | 0.6 / 2.0 / 2.8 / 4.5 | 4.5 | 0 | 0 | 1.34 / 1.34 | 15 |
| 2560x1440 | 4,2 | T1d defaults | 2 | 0.5 / 1.9 / 2.7 / 4.7 | 4.7 | 0 | 0 | 1.44 / 1.48 | 16 |

- **No pan:** open sea gone (0 frames; T1c: 8-15 frames, runs up to 94 ms, one 1783 ms run at 2560x1440 group 1 while the machine was at 20 % load); pop worst 10-18 % of pixels in one frame pair falls to 0.5 % (1280x800) and 1.4 % (2560x1440, bigger chunks arriving per frame).
- **Pan 4,2 (about 1600 px/s):** open sea gone in all 36 runs; pop worst 12-31 % falls to 1.3-2.7 %; pairs above 0.5 % fall from 51-89 to 9-18. **Stated threshold:** no more than 3 % of the pixels jump between two frames of one group at this pan, 1.5 % without pan.
- **Frame time:** window-thread max 4.3-5.9 ms in every median cell (acceptance 8 ms), p99 at most 3.8 ms; the T1d defaults add 0.1-0.2 ms at p95 for the prefetch uploads. Single-frame outliers of 130-667 ms occurred in four no-pan runs (two per configuration, so not caused by T1d; all with the per-frame read-back of `--pop`, which stalls the GPU queue) and are in the "worst max" column.
- **Not covered:** a pan of 12,6 px per frame (about 5000 px/s) still shows open sea (hundreds of frames): chunk builds, 100-250 ms each, cannot keep up with a view crossing 1280 px in a quarter of a second; faster chunk building is the remaining lever (not in this task).
- **Items not done:** 3 (rivers on a stand-in: the owner reports rivers "perfect"; river pixels are a small share of the pop) and 4 (wave window: 14-48 ms per zoom step, not reported by the owner; the dumped-frame metric shows no jump left that is not a chunk swap) were conditional on frames still showing them after items 1, 2 and 5; they do not, at the metric's resolution. Left open as OQ-091 / OQ-076.

### 2026-10-20 - T1e: no dissolve on a plain pan; open sea by area; the margin's memory

RTX 4090 + Ryzen 9800X3D, enhanced water, GPU async chunks, interactive limits, CPU load 0-8 % before each run, branch `task/T1d-zoom-artifacts` at `245f666` (the review commits after it change counting and tests only). One run per cell unless said.

**Plain pan (item 1).** `bench_frame --zoom 0 --zoom-pan dx,dy,400 --zoom-frames 1600` (a pan with no zoom step). "fade" = frames after the first second in which a chunk dissolves in; T1d dissolved every chunk entering at the edge (reviewers' finding, not measured on T1d).

| scene | res | group | pan (px/frame, about px/s) | p50 / p95 / p99 / max ms | frames with open sea | fade frames |
|---|---|---|---|---|---|---|
| canyon `D_256x256_0045_5B` | 1280x800 | 1 | 4,2 (1600) | 0.3 / 1.7 / 1.8 / 3.7 | 0 | 0 |
| canyon, `--prefetch 0` | 1280x800 | 1 | 4,2 | 0.2 / 0.4 / 0.7 / 2.3 | 2 | 0 |
| canyon | 2560x1440 | 0 / 2 | 4,2 | 0.3-0.4 / 0.7-0.8 / 0.9-1.0 / 2.1-2.7 | 0 / 0 | 0 / 0 |
| canyon | 2560x1440 | 0 / 2 | 8,0 (3200) | 0.3-0.4 / 0.7-0.8 / 0.9-1.0 / 2.3-3.1 | 0 / 0 | 0 / 0 |
| canyon | 2560x1440 | 0 / 2 | 12,6 (5000) | 0.4 / 0.8-0.9 / 1.1 / 2.5-6.5 | 53 / 0 | 151 / 0 |
| `Single_24.sze` | 2560x1440 | 0 / 2 | 4,2 / 8,0 / 12,6 | 0.3-0.4 / 0.8-0.9 / 1.0-1.1 / 2.2-3.3 | 0 in all six | 0 in all six |

The prefetch uploads cost p95 at 1280x800 (0.4 → 1.7 ms: one chunk upload per idle frame while the margin moves); max stays below 8 ms everywhere. The canyon at 5000 px/s, group 0, is build latency (no source built in any group, "0 built on the CPU"): those chunks show open sea, then dissolve; the T1f / T11 tasks are about that. The reviewers' concern that the margin and the neighbouring groups share `prefetch_chunks` (96) and could starve the margin at 2560x1440 is not seen in these runs.

**Open sea by area (item 7).** `open_sea_chunks` now counts a chunk when the stand-in parts leave any of its visible area uncovered, `open_sea_px` sums that area. Pan while zooming (`--zoom 60 --zoom-pan 4,2,400 --zoom-frames 1600`, the T1d table's rows), canyon, 1280x800:

| group | p50 / p95 / p99 / max ms | frames with open sea (of 1600) | longest run | open-sea pixels in all | worst share of the view | fade frames |
|---|---|---|---|---|---|---|
| 0 | 0.3 / 1.8 / 2.7 / 4.9 | 201 | 60 frames / 118 ms | 13.2 M | 18.2 % | 261 |
| 1 | 0.3 / 2.4 / 3.4 / 5.7 | 94 | 60 / 114 ms | 1.5 M | 4.2 % | 237 |
| 2 | 0.3 / 1.7 / 2.4 / 4.2 | 74 | 35 / 68 ms | 1.6 M | 6.1 % | 320 |

So T1d's "open sea 0 frames" held chunk-wise only: holes in the stand-in cover (parts of the new view with no built source in any group, mostly after a step to group 0 while panning) show open sea for up to one zoom interval. It is not a T1e regression: group 0 with the margin taken out of the prefetch (the T1d order, a temporary build) gave 210-212 frames / 12.4-15.2 M px in two runs, the T1e code 130-240 frames / 4.0-8.0 M px, a larger cap (`--prefetch 200`) 195-230 frames. Every missing part was unbuilt on the CPU ("0 built"): build latency again (T1f, T11). `Single_24.sze` at 2560x1440 with a zoom every 60 frames: 289-364 frames, up to 18-27 % of the view in the worst frame (next table).

**The margin's memory (item 8).** CPU chunk cache sampled after every frame (`Chunk::bytes`, capacities), `Single_24.sze`, group 0, pan while zooming as above:

| res | pan | `--lookahead-chunks` | peak MB | mean MB | peak chunks | frames with open sea | worst share of the view |
|---|---|---|---|---|---|---|---|
| 1280x800 | 4,2 | 3 | 495 | 383 | 133 | 62 | 2.1 % |
| 1280x800 | 4,0 | 3 | 439 | 359 | 117 | 6 | 2.7 % |
| 2560x1440 | 4,2 | 3 | 672 | 511 | 191 | 347 | 26.9 % |
| 2560x1440 | 4,0 | 3 | 677 | 536 | 185 | 293 | 18.4 % |
| 1280x800 | 4,2 | 2 | 490 | 382 | 131 | 99 | 2.8 % |
| 1280x800 | 4,0 | 2 | 443 | 362 | 118 | 2 | 2.3 % |
| 2560x1440 | 4,2 | 2 | 711 | 529 | 193 | 364 | 26.2 % |
| 2560x1440 | 4,0 | 2 | 634 | 528 | 171 | 289 | 17.4 % |

The counted worst case at 2560x1440 (all chunks land chunks of 4.3 MB) is 0.8 / 1.1 / 1.5 GB standing / one axis / diagonal; the largest scene stays at 0.44-0.71 GB peak because sea chunks are small. Cap 2 against 3 differs by less than the run-to-run noise in memory (most of the cache is the 48 spare chunks and the neighbouring groups' views); the viewer's default stays 3, `--lookahead-chunks 2` exists for the owner.

**Blend cost (item 3)** is not measured: the opaque pipelines are back for every draw but the fading ones (one pipeline switch per pass when a chunk fades), so the per-frame cost is the pre-T1d one by construction.

### 2026-10-21 - T1f item 1: chunk build profile, Windows 11, Ryzen 9 9800X3D (8 cores, 16 threads), MSVC Release, branch task/T1f-chunk-build-speed (0f2583b)

**Method.** `bench_frame <game> <scene> --chunk-profile 1,16 --groups 0,1,2`. It builds the 96 chunks nearest the centre of the largest island (60 at group 2, the whole island) with `render_chunk` as the viewer's workers do (no ships), but it waits for the river flow, and its threads run at normal priority (the viewer's workers run below normal and do not wait). Each configuration runs twice: once on a freshly loaded scene (cold: sprite decodes, the island water and the river flow solve happen on first use) and once more on the same scene (warm). Per-stage times come from `anno/render/profile.hpp`. The stages are exclusive, so they add up to the chunk's time (less than 1 % falls outside them). Background load was 8-27 % (desktop apps; the machine was not idle). Two runs agree within 3 %. All chunks contain land.

**Warm, one thread: mean ms per chunk and the stages (mean ms; share of the chunk)**

| scene | group | chunk mean / p95 | sea field | river sprites + fall masks + river blend + pack | terrain | flow + apron | **field + terrain + flow** | ground (+ px) | objects |
|---|---|---|---|---|---|---|---|---|---|
| canyon `D_256x256_0045_5B` | 0 | 50.1 / 66.3 | 24.2 | 6.3 | 3.6 | 0.2 | **34.2 (68 %)** | 7.0 (14 %) | 8.7 (17 %) |
| canyon | 1 | 44.3 / 75.7 | 21.4 | 7.0 | 3.1 | 0.3 | **31.8 (72 %)** | 6.3 (14 %) | 6.0 (14 %) |
| canyon | 2 | 28.2 / 76.5 | 10.8 | 6.8 | 1.8 | 0.3 | **19.7 (70 %)** | 4.2 (15 %) | 4.2 (15 %) |
| `Single_24.sze` | 0 | 44.2 / 57.6 | 21.8 | 4.2 | 3.5 | 0.1 | **29.7 (67 %)** | 6.7 (15 %) | 7.7 (17 %) |
| `Single_24.sze` | 1 | 39.3 / 56.8 | 19.9 | 4.8 | 3.1 | 0.3 | **28.1 (71 %)** | 5.9 (15 %) | 5.1 (13 %) |
| `Single_24.sze` | 2 | 25.2 / 69.8 | 10.1 | 5.4 | 1.8 | 0.2 | **17.5 (69 %)** | 4.0 (16 %) | 3.6 (14 %) |

**Threads and the cold scene (ms per chunk, chunks per second)**

| scene | group | 1 thread warm p95 | 16 threads warm mean / p95 | 16 threads cold p95 / max | 16 threads warm chunks/s |
|---|---|---|---|---|---|
| canyon | 0 | 66.3 | 92.0 / 122.8 | 247.7 / 248.2 | 156 |
| canyon | 1 | 75.7 | 81.5 / 130.2 | 251.3 / 252.1 | 180 |
| canyon | 2 | 76.5 | 53.6 / 139.9 | 251.6 / 252.2 | 274 |
| `Single_24.sze` | 0 | 57.6 | 85.1 / 105.1 | 185.7 / 186.7 | 176 |
| `Single_24.sze` | 1 | 56.8 | 76.3 / 112.0 | 188.0 / 188.9 | 194 |
| `Single_24.sze` | 2 | 69.8 | 47.8 / 117.2 | 178.5 / 179.0 | 285 |

On the canyon island at group 0, the warm mean per chunk is 50 ms on 1 thread, 51 on 2, 54 on 4, 61 on 8 and 92 on 16. That is 20, 39, 73, 124 and 156 chunks/s.

**What this says**
- **The field, terrain and flow stages are 67-72 % of a chunk.** The sea field alone (`sea::water_field`) is 35-49 %. It does several spline evaluations per pixel: `tile_ground`, then `river_point` or the beach slope (4 more `smooth_height` or `river_level` taps). That is about 90 ns per pixel at group 0. The river stages (sprites, fall masks, the chamfer blend) add 4-7 ms, and `terrain_at` per pixel adds 2-4 ms. This confirms the orchestrator's main suspect. Item 2's target (at most 2 ms together) means removing about 30 ms per chunk at group 0.
- **The ground pass is 14-16 % of a chunk** (`ground_with_probe` plus the conversion to `Chunk::ground`), below the 20 % of T11's C2 rule, so the ground pass (2b) stays after item 2. After item 2 the chunk would be about 18 ms at group 0, of which the ground would be about 40 % and the objects about 48 %.
- **Sprite decoding is not a cost.** It is 0.1-0.7 ms per chunk on a cold scene and 0 warm. The objects (cull and blit) are 13-17 %; T11 moves them to the GPU.
- **"100-250 ms per chunk" (T1d) is the 16-thread cold case.** One thread builds a chunk in 25-50 ms (p95 57-77). With 16 threads on 8 cores each chunk takes 1.6-2 times longer: SMT shares a core between two threads (8 threads: 1.2 times). Throughput still grows to 156-285 chunks/s. On a cold scene, the chunks with rivers also wait for the island's river flow solve: 84-151 ms on one thread, and every other builder blocks in its `call_once`. This shows as `flow` (23-37 ms mean at 16 threads cold) and as the 180-250 ms maxima. The viewer does not wait (`wait_for_flow` false): it delivers such chunks without the flow lattice and **builds them a second time** when the flow is ready. That is extra work in the first seconds, worth removing in item 3 (deliver the lattice separately).
- **Not measured here:** the workers' below-normal priority against the window thread, and the queue. A chunk's latency in the viewer is its build time plus its place in the queue divided by throughput: 40 chunks queued take about 250 ms at 160 chunks/s.
- **Compared with 2026-10-17** (5.7 ms per chunk on one thread, Apple M5 Pro, `Single_01.sze`, before the enhanced sea field): the field, terrain and flow stages added since then account for the growth.
### 2026-10-21 - T1f item 2: exact shortcuts in the field and terrain stages (branch task/T1f-chunk-build-speed, 9b14840 for the numbers; 84ec121 adds only tests and named constants)

**What changed.** Every output byte is identical: the digests are unchanged, and there is a golden digest from before the change in CI. Exactness was the owner's condition; the proof is in `docs/render/iso.md` "Water field: empty cells".
- **Empty cells:** once per island, the tiles whose every position provably gets the empty water-field texel (no river nearby, and no sea in the block or every spline corner ≥ 31.1) are marked, and `water_field` writes the empty texel there without evaluating it. On the game's islands 37 % of all tiles are empty.
- **Noise:** `height_noise` without the second noise plane, whose weight is exactly 0 at the integer z it uses.
- **Samplers:** `smooth_height` and the noise sampled through per-tile and per-cell caches, with the same arithmetic.
- **Terrain:** built per tile (`terrain_plane`) instead of `terrain_at` per pixel.
- **River masks:** the fall, mouth and bed masks created and blurred only when a river sprite pixel reaches the chunk.

**One thread, warm, mean ms per chunk** (same bench and chunks as item 1; background load 13-18 %)

| scene | group | chunk before → after | field + terrain + flow before → after | sea field | river stages | terrain | ground (+ px) | objects |
|---|---|---|---|---|---|---|---|---|
| canyon | 0 | 50.1 → **29.1** | 34.2 (68 %) → 13.4 (46 %) | 24.2 → 6.5 | 6.3 → 4.3 | 3.6 → 2.4 | 7.0 | 8.6 |
| canyon | 1 | 44.3 → 28.7 | 31.8 (72 %) → 16.4 (57 %) | 21.4 → 9.0 | 7.0 → 4.9 | 3.1 → 2.2 | 6.3 | 6.0 |
| canyon | 2 | 28.2 → 19.5 | 19.7 (70 %) → 11.2 (57 %) | 10.8 → 5.3 | 6.8 → 4.5 | 1.8 → 1.2 | 4.2 | 4.1 |
| `Single_24.sze` | 0 | 44.2 → **21.4** | 29.7 (67 %) → 7.0 (33 %) | 21.8 → 2.7 | 4.2 → 1.7 | 3.5 → 2.4 | 6.7 | 7.7 |
| `Single_24.sze` | 1 | 39.3 → 23.0 | 28.1 (71 %) → 11.9 (52 %) | 19.9 → 7.0 | 4.8 → 2.5 | 3.1 → 2.2 | 5.9 | 5.1 |
| `Single_24.sze` | 2 | 25.2 → 16.6 | 17.5 (69 %) → 8.9 (53 %) | 10.1 → 4.4 | 5.4 → 3.1 | 1.8 → 1.2 | 4.0 | 3.6 |

**Against the targets** (owner, 2026-10-21: field + terrain at most 25 % of a chunk, a canyon land chunk at group 0 under 25 ms on one thread): **both missed.**
- The canyon chunk at group 0 is 29.1 ms; `Single_24` is 21.4 ms.
- Field + terrain is 33-57 % of a chunk.

**What is left of the field is the sea and the coast inside the island grids:**
- **Sea and coast pixels:** their depth byte needs the exact smooth height (spline plus noise) at every pixel, and it never saturates (at most 202). The beach band needs four more samples for the slope. No exact shortcut removes this; it is about 30-40 ns per pixel.
- **River stages:** the blurs and chamfer distances run over the whole chunk where rivers meet the sea (up to 21 ms in single chunks).
- **Remaining exact options:** blur and chamfer only within the bounding box of the river pixels plus their radius, about 2-3 ms on river chunks; SIMD for the band pixels.
- **The full fix** is T11 §10.1 (the field on the GPU).

**16 threads after the change: not measured yet.** During the run the machine was at 47-77 % load from a game, which invalidates the numbers, so they are not reported. To be rerun on an idle machine.

### 2026-10-21 - T11 stage 1: the CPU builder "build the list, replay it" against `main`, Windows 11, Ryzen 9 9800X3D (16 threads), MSVC Release

**Method.** `bench_frame <game> <scene> --chunk-profile 1,16 --groups 0,1,2` on `main` (f583e9d) and on `task/T11-gpu-chunks` (the stage 1 code with the unit-key fix), one run each, same machine, not idle (background load of desktop apps), so differences under about 5 % are noise. Warm rows (the same scene after a cold pass). The branch builds every chunk's objects as a draw list and replays it (`render_chunk`, `ChunkBuilder::Cpu`).

**Warm: objects stage and chunk mean (ms per land chunk), main -> branch**

| scene | group | threads | objects stage | chunk mean |
|---|---|---|---|---|
| canyon `D_256x256_0045_5B` | 0 | 1 | 8.67 -> 6.95 (-20 %) | 29.9 -> 28.1 (-6 %) |
| canyon `D_256x256_0045_5B` | 0 | 16 | 12.81 -> 11.37 (-11 %) | 51.2 -> 50.3 (-2 %) |
| canyon `D_256x256_0045_5B` | 1 | 1 | 6.02 -> 5.29 (-12 %) | 29.3 -> 29.1 (-1 %) |
| canyon `D_256x256_0045_5B` | 1 | 16 | 9.12 -> 8.25 (-10 %) | 52.1 -> 50.0 (-4 %) |
| canyon `D_256x256_0045_5B` | 2 | 1 | 4.19 -> 3.97 (-5 %) | 20.1 -> 20.1 (+0 %) |
| canyon `D_256x256_0045_5B` | 2 | 16 | 6.75 -> 6.34 (-6 %) | 36.5 -> 35.1 (-4 %) |
| `Single_24.sze` | 0 | 1 | 7.78 -> 6.22 (-20 %) | 22.1 -> 20.7 (-6 %) |
| `Single_24.sze` | 0 | 16 | 11.86 -> 10.28 (-13 %) | 39.4 -> 37.8 (-4 %) |
| `Single_24.sze` | 1 | 1 | 5.24 -> 4.69 (-10 %) | 23.7 -> 23.5 (-1 %) |
| `Single_24.sze` | 1 | 16 | 8.02 -> 7.59 (-5 %) | 42.7 -> 43.6 (+2 %) |
| `Single_24.sze` | 2 | 1 | 4.14 -> 3.48 (-16 %) | 21.2 -> 17.0 (-20 %) |
| `Single_24.sze` | 2 | 16 | 5.70 -> 5.68 (-0 %) | 30.1 -> 30.1 (+0 %) |

**Result.** The objects stage is not slower anywhere: 0.4-20 % faster, most at group 0 on one thread (-20 %), because the keel of a frame is now computed once per frame in the table instead of once per blit; with 16 workers the gain shrinks to 0-13 %, which is the per-draw shared lock on the frame table and the memory traffic showing, but still below `main`. The chunk mean moves by -6 % .. +2 %. The 5 % threshold of the review is not exceeded, so the two suspected costs (the bins computed for the `Cpu` builder, ids per draw under the shared lock) stay as they are; they are cheap next to the keel saving. Re-measure when stage 2a changes the builder.

### 2026-10-21 - T1f item 2b and the 16-thread rerun of the item 1 / item 2 tables, Windows 11, Ryzen 9 9800X3D, MSVC Release

**Method.** `bench_frame <game> <scene> --chunk-profile 1,16 --groups 0,1,2`, four builds run one after the other on an idle machine (3-5 % background load, no game running): item 1 (0f2583b, no shortcuts), item 2 (bd380ab, exact empty cells and terrain), `main` daaa849 (item 2 plus T11 stage 1, the baseline of 2b) and item 2b (this branch). Warm rows, mean ms per land chunk; "river" is river sprites + fall masks + river blend (the stages 2b touches). One run each; differences under about 5 % are noise.

| scene | group | 16 threads, chunk mean: item 1 / item 2 / main / **2b** | 16 threads p95: item 1 / item 2 / main / **2b** | river stages, 16 threads: item 1 / item 2 / main / **2b** | 1 thread chunk mean: item 1 / item 2 / main / **2b** | river stages, 1 thread: item 1 / item 2 / main / **2b** |
|---|---|---|---|---|---|---|
| canyon | 0 | 92.7 / 50.7 / 50.6 / **47.0** | 125.6 / 95.1 / 100.0 / **94.1** | 9.41 / 6.08 / 6.44 / **3.96** | 52.1 / 29.8 / 28.1 / **26.4** | 6.18 / 4.07 / 4.04 / **2.32** |
| canyon | 1 | 82.0 / 51.0 / 50.0 / **48.3** | 125.3 / 99.1 / 98.1 / **87.7** | 10.27 / 6.68 / 6.47 / **3.99** | 45.4 / 29.7 / 28.7 / **26.6** | 6.82 / 4.67 / 4.64 / **2.53** |
| canyon | 2 | 50.9 / 35.7 / 35.6 / **30.9** | 129.6 / 102.8 / 104.4 / **69.0** | 10.37 / 6.07 / 6.15 / **2.95** | 28.7 / 20.2 / 19.8 / **17.4** | 6.71 / 4.34 / 4.25 / **1.93** |
| `Single_24` | 0 | 84.9 / 39.1 / 39.1 / **37.6** | 108.6 / 79.1 / 79.6 / **76.9** | 8.12 / 2.74 / 2.74 / **2.13** | 45.4 / 22.0 / 20.6 / **20.2** | 4.01 / 1.49 / 1.46 / **1.14** |
| `Single_24` | 1 | 76.3 / 43.6 / 41.8 / **40.3** | 106.9 / 74.5 / 72.2 / **59.4** | 8.74 / 3.66 / 3.49 / **2.07** | 40.6 / 23.5 / 23.2 / **21.8** | 4.73 / 2.23 / 2.21 / **1.19** |
| `Single_24` | 2 | 48.0 / 29.7 / 30.4 / **27.6** | 120.5 / 82.0 / 84.0 / **56.4** | 8.84 / 4.02 / 4.15 / **1.99** | 25.8 / 16.8 / 17.1 / **15.3** | 5.29 / 2.83 / 2.96 / **1.24** |

**Result.** Item 2b takes the river stages from 2.7-6.4 ms to 2.0-4.0 ms at 16 threads (1.5-2.1 times less; 4.3 to 1.9 ms on one thread at group 2 of the canyon island) and the chunk mean by 3-14 %, most at group 2 where the river share is largest; the p95 at 16 threads falls most (canyon group 2: 104 to 69 ms, `Single_24` group 2: 84 to 56 ms), because the worst chunks are the river chunks. The 16-thread rows of the item 1 table (never measured idle before): 85-93 ms per chunk at group 0 against 39-51 ms after item 2, i.e. item 2 halved the chunk time also under SMT load, as it did on one thread. The 16-thread chunk time stays 1.7-1.9 times the one-thread time (SMT), as before.

### 2026-10-21 - T3 stage 3 fix 1: the level is composed at scale <= 1 (cost of the finer grid), Windows 11, Ryzen 9 9800X3D, MSVC Release, branch task/T3-continuous-zoom

`bench_frame <game> D_256x256_0045_5B.scp --renderer gpu --styles enhanced --res 1280x800,2560x1440 --groups 0,1,2 --scale-cost group:scale[:filter],...` (new): the grid of zoom group `group` composed for the window and resampled at `scale`, 60 frames after everything shown is built and uploaded; **GPU ms** are timestamps from the start of the first pass to the end of the last (the resample pass included: `GpuFrame::frame_gpu_ms`, new), p50 and max. Enhanced water, canyon island, 16 threads. The old rule composed on the level nearest in ratio (scale 0.69 .. 1.45, worst grid at 0.69); the new one at scale <= 1 (0.45 .. 1: worst grid at 0.95 x tile ratio: 0.475 between groups 0 and 1 and 1 and 2 / 2 and 3 for the ratio 2, 0.452 for 1 and 2). "Chunks shown" are the chunks the grid overlaps, "cached" the chunks in the cache, "missing" and "dropped" the sums over the 60 frames (a visible chunk trimmed would show as missing).

**Adapter 0, NVIDIA GeForce RTX 4090 (Vulkan)**

| window | point | grid | chunks shown | cached | missing | dropped | GPU ms p50 | GPU ms max |
|---|---|---|---|---|---|---|---|---|
| 1280x800 | old worst: group 1 at 0.69 | 1920x1216 | 12 | 52 | 0 | 0 | 0.14 | 0.14 |
| 1280x800 | new worst: group 1 at 0.452 | 2880x1792 | 24 | 69 | 0 | 0 | 0.65 | 2.53 |
| 1280x800 | new worst: group 2 at 0.475 | 2752x1728 | 24 | 83 | 0 | 0 | 0.68 | 2.46 |
| 2560x1440 | old worst: group 1 at 0.69 | 3776x2112 | 40 | 92 | 0 | 0 | 0.79 | 2.78 |
| 2560x1440 | old worst: group 2 at 0.69 | 3776x2112 | 40 | 121 | 0 | 0 | 0.96 | 2.27 |
| 2560x1440 | new worst: group 1 at 0.452 | 5696x3200 | 84 | 181 | 0 | 0 | 2.16 | 3.57 |
| 2560x1440 | new worst: group 2 at 0.475 | 5440x3072 | 84 | 200 | 0 | 0 | 2.81 | 4.69 |
| 2560x1440 | magnified, area: group 0 at 1.414 | 1856x1024 | 12 | 52 | 0 | 0 | 0.13 | 0.13 |

**Adapter 1, AMD Radeon(TM) Graphics, the iGPU (Vulkan)**

| window | point | grid | chunks shown | cached | missing | dropped | GPU ms p50 | GPU ms max |
|---|---|---|---|---|---|---|---|---|
| 1280x800 | scale 1, group 0 (reference) | 1344x832 | 12 | 60 | 0 | 0 | 3.55 | 4.26 |
| 1280x800 | old worst: group 1 at 0.69 | 1920x1216 | 12 | 90 | 0 | 0 | 8.05 | 8.66 |
| 1280x800 | old worst: group 2 at 0.69 | 1920x1216 | 12 | 72 | 0 | 0 | 9.83 | 10.56 |
| 1280x800 | new worst: group 1 at 0.452 | 2880x1792 | 24 | 160 | 0 | 0 | 17.37 | 22.39 |
| 1280x800 | new worst: group 2 at 0.475 | 2752x1728 | 24 | 96 | 0 | 0 | 30.99 | 33.51 |
| 2560x1440 | scale 1, group 0 (reference) | 2624x1472 | 18 | 88 | 0 | 0 | 13.39 | 14.07 |
| 2560x1440 | old worst: group 1 at 0.69 | 3776x2112 | 40 | 210 | 0 | 0 | 31.84 | 34.79 |
| 2560x1440 | old worst: group 2 at 0.69 | 3776x2112 | 40 | 140 | 0 | 0 | 72.14 | 75.97 |
| 2560x1440 | new worst: group 1 at 0.452 | 5696x3200 | 84 | 378 | 0 | 0 | 101.75 | 105.88 |
| 2560x1440 | new worst: group 2 at 0.475 | 5440x3072 | 84 | 252 | 0 | 0 | 194.63 | 198.68 |
| 2560x1440 | magnified, area: group 0 at 1.414 | 1856x1024 | 12 | 60 | 0 | 0 | 7.82 | 8.14 |

Reading: the cost follows the composed pixels (4.9 x at the worst point against 2.2 x before) and, in enhanced water, the sea shading per pixel, which is the iGPU's limit already at scale 1 (3.6 ms at 1280x800, 13.4 ms at 2560x1440). **On the RTX 4090 no frame passes 4.7 ms (the 8 ms line) at either window size; on the Radeon iGPU the worst point breaks it at both sizes (31 ms at 1280x800, 195 ms at 2560x1440 in group 2), while the old rule's worst point already broke it at 2560x1440 (72 ms) and was at 9.8 ms at 1280x800.** No chunk shown was missing or dropped in 60 frames (the cache held 1.5-4.5 x the chunks shown). Group 2 costs most because its sea shading per grid pixel is the dearest. `--zoom-max-upscale 1.45` restores the old grid for weak GPUs; an automatic choice from the measured cost is the follow-up the orchestrator may want (not built).

### 2026-10-21 - T3 stage 3 fix 2: derived chunks are reduced from the cached chunks of the level above, Windows 11, Ryzen 9 9800X3D (16 threads), MSVC Release, branch task/T3-continuous-zoom

`bench_frame <game> <file> --fill-time --derive old|cache --threads n --res 1920x1080` (new, CPU workers only): a 1920x1080 window at group 2 on the island is built until its chunks are cached (the prefetch goes on in the background, as in the viewer), then the same window is shown at levels 3, 4 and 5 in turn, each time starting at once; **ms until every chunk the window shows is cached**. `old` = every derived chunk builds its descendants from nothing and drops them (the first stage 3 build; `FrameRenderer::derive_from_cache = false`), `cache` = reduced from the cached children, the missing ones built and cached. 12 chunks shown at every step.

| scene | threads | rule | group 2 ms | level 3 ms | level 4 ms | level 5 ms | zoom-out total (3 + 4 + 5) |
|---|---|---|---|---|---|---|---|
| canyon island | 1 | old | 868 | 1339 | 1588 | 1659 | 4586 |
| canyon island | 1 | cache | 790 | 731 | 279 | 123 | 1133 |
| canyon island | 16 | old | 260 | 655 | 728 | 339 | 1722 |
| canyon island | 16 | cache | 352 | 60 | 52 | 51 | 163 |
| `Single_24` | 1 | old | 550 | 1135 | 1429 | 1678 | 4242 |
| `Single_24` | 1 | cache | 546 | 730 | 307 | 184 | 1221 |
| `Single_24` | 16 | old | 265 | 451 | 497 | 512 | 1460 |
| `Single_24` | 16 | cache | 236 | 91 | 89 | 13 | 193 |

The first zoom-out is 3.5-4 x faster on one thread and 7.5-10 x on 16 (to 163 / 193 ms in all, the window thread never waits). The time falls with each level because the level above is cached from the step before and the prefetch of the neighbouring levels has built part of the children meanwhile. The cost: the cache holds the intermediate chunks (1311 chunks at level 5 on one thread on the canyon island, 221 on 16 threads where the trim has run: mostly open-sea chunks around the island that the prefetch ring of the derived levels asks for, a few KB each; `cached` is the cache size after the step, builds in all 2227-2548 over the whole run, counting these). Test: a zoom-out sweep from group 2 to level 5 on the canyon island builds no key twice while it stays cached, with workers and without (`render_corpus_test.cpp`).

### 2026-10-21 - T3 stage 3 items (a)-(h): the continuous zoom sweep on both GPUs, Windows 11, Ryzen 9 9800X3D (16 threads), MSVC Release, branch task/T3-continuous-zoom

`bench_frame <game> D_256x256_0045_5B.scp --renderer gpu --limits interactive --derived 5 --groups 0,1,2 --styles enhanced --res <WxH> --zoom-sweep <s>` (new): the continuous zoom as the viewer runs it, canyon island, enhanced water, interactive limits (1 ms upload budget, 250 ms wave fade, pre-created planes by `prewarm_plan`): tile width 42 out to the smallest level (0.5), in to the largest zoom (118.8), out again and back to 42, **6 s per leg** (24 s, 12000 frames at 500 Hz pacing), the target eased (120 ms), a pan of 300 screen pixels per second reversing every leg, the cursor off centre as anchor. Levels 0-2 are built, 3-7 derived. **Window thread ms** = the busy time of `GpuFrame::render` (what the viewer's busy time is made of); **GPU ms** = timestamps from the first pass to the end of the resample pass. Cold start: nothing is cached at the beginning.

**Adapter 0, NVIDIA GeForce RTX 4090 (Vulkan), 1280x800** (frames above 8 ms: **1**, the first frame):

| level | scale | frames | window thread ms p50 / p95 / p99 / max | GPU ms p50 / max | chunks shown (max) | cache MB (max) |
|---|---|---|---|---|---|---|
| 0 | 0.48 - 1.39 | 1170 | 0.41 / 0.87 / 2.58 / 2.83 | 0.46 / 3.21 | 28 | 192 |
| 1 | 0.45 - 1.00 | 1854 | 0.47 / 1.26 / 2.23 / 8.39 | 0.32 / 1.39 | 35 | 613 |
| 2 | 0.48 - 1.00 | 1693 | 0.49 / 1.65 / 2.80 / 3.88 | 0.31 / 5.47 | 30 | 456 |
| 3 | 0.48 - 1.00 | 1703 | 0.45 / 0.76 / 1.46 / 2.85 | 0.35 / 2.38 | 30 | 258 |
| 4 | 0.48 - 1.00 | 1698 | 0.45 / 0.67 / 0.92 / 2.36 | 0.41 / 2.66 | 28 | 243 |
| 5 | 0.48 - 1.00 | 1701 | 0.46 / 0.73 / 0.98 / 2.48 | 0.42 / 2.30 | 28 | 201 |
| 6 | 0.48 - 1.00 | 1698 | 0.47 / 0.98 / 1.14 / 2.33 | 0.42 / 2.06 | 28 | 201 |
| 7 | 0.81 - 1.00 | 483 | 0.36 / 0.50 / 0.72 / 2.34 | 0.33 / 1.46 | 12 | 201 |

**RTX 4090, 2560x1440** (frames above 8 ms: **0**): window thread p50 0.5 - 0.9 ms, p99 1.2 - 3.9 ms, max 7.3 ms (level 2); GPU p50 0.4 - 0.8 ms, max 4.6 ms; 84 chunks shown, cache up to 1124 MB at level 1.

**Adapter 1, AMD Radeon(TM) Graphics, the iGPU (Vulkan), 1280x800**: window thread p50 0.3 - 0.5 ms, p99 0.6 - 2.1 ms, max 2.8 ms at every level but one frame (the first, 22 ms: the start group's sea steps are uploaded in it: `sea 20.15`); GPU p50 15 - 33 ms (enhanced water on the iGPU, 3.6 ms at scale 1 and 8 - 10 times the RTX's at every point: see fix 1), max 117 ms.
**iGPU, 2560x1440** (0.4 s per leg, 800 frames: the frames take 80 - 100 ms of GPU each, so the zoom is far faster than real time): window thread p50 0.9 - 2.5 ms, p99 2.3 - 22 ms; 3 frames above 8 ms: the first (`sea` 20.7 ms) and two at level 1 whose time is `record` 17.5 - 18.5 ms, the wait of the command recording on a GPU that is 300 ms behind (a frame queue, not window-thread work); GPU p50 77 - 107 ms.

**What the sweep found and what was fixed** (before, window thread max): `sync uploads` 10.9 ms (the owner's log), 16 ms on the iGPU: **a chunk uploaded a plane at a time under the 1 ms budget** (7 frames per chunk instead of 1; the picture is identical); `draws` 28 - 40 ms and `chunks` 8 - 11 ms at the deep levels: **stand-in sources limited to 8 x finer** (a level-7 chunk from level 0 was 16384 spans per frame); `chunks` 11 ms: **the chunk plan looks at 512 keys per frame**; `under` 15 - 36 ms on the iGPU: **offscreen targets made at the largest size seen** (+25 %, reused down to a third, viewport and scissor on the passes; an `under` texture is no longer made every 64 pixels of grid growth) and, found by the validation of that change, **a resample at scale 1 and offset 0 cut the rounded-up grid wrongly** (it was squeezed into the output whole; now cut); `fluid` 12 - 31 ms: **the wave window's new grid is made on the job's thread**; `sea` 22 - 28 ms of the first frame: **the sea patterns are built from the start of `run()`** (item (d); the upload of the start group's steps remains, 5 ms on the RTX 4090, 20 ms on the iGPU, once, in the first frame).

**Pre-created planes (item (h), OQ-093), MB held** (`prewarm_plan`, 3.75 MiB per chunk, the chunks of the largest composed grid plus a ring of one chunk; the viewer's budget is 1 GiB on a discrete adapter, 256 MiB on the iGPU):

| window | chunks wanted | RTX 4090 (1 GiB): chunks / MB | iGPU (256 MiB): chunks / MB |
|---|---|---|---|
| 1280x800 | 63 | 63 / 236 | 63 / 236 |
| 2560x1440 | 150 | 150 / 562 | 68 / 255 |
| 3840x2160 | 260 | 260 / 975 | 68 / 255 |

A resize recomputes the target (`GpuFrame::set_prewarm_chunks`); the fixed 24 chunks (94 MB) the pre-creation held before were less than a third of the 2560x1440 window's grid.

**Wave cross-fade cost (item (g), OQ-076)**: `Single_24`, group 1, enhanced water, GPU ms of the frame with the weight of the simulated waves held at 1 / 0.5 / 0 (`bench_frame --wave-fade-hold k --scale-cost 1:1.0`): RTX 4090 0.03 / 0.03 / 0.03 ms at 1280x800 and 0.14 / 0.16 / 0.14 ms at 2560x1440; iGPU 1.54 / 1.70 / 1.59 ms and 8.75 / 9.70 / 8.48 ms. The second shading of the sea is paid only during the 250 ms after a window switch: at most +0.02 ms (RTX 4090) and +1 ms (iGPU, 2560x1440) in those frames.

**Not measured / limits**: the iGPU's 2560x1440 sweep is 15 x faster than real time (its GPU time per frame, not the window thread, is the limit); the real-time zoom of the owner on the iGPU at that size is GPU-bound (fix 1 cost table). Frames above 8 ms that remain on the window thread: the first frame (sea upload) of each run; nothing else on the RTX 4090.

### 2026-10-21 - T3 follow-up: the owner's frame log (points (c)-(e) and the memory), Windows 11, Ryzen 9800X3D (16 threads), RTX 4090 (adapter 0, Vulkan), MSVC Release, branch task/T3-followup

**Method.** `ANNO_VIEW_ZOOM_SCRIPT=2,<seconds>,enhanced` (the viewer's test hook: one group step every 2 drawn frames, 2 -> 1 -> 0 -> 1 -> 2 ..., the stderr report is the frame log), `ANNO_GPU_ADAPTER=0`, `Single_24.sze`, enhanced water. "1280x800" is the window as created; "maximized" is the viewer's own window given `SW_MAXIMIZE` by `ShowWindowAsync` a few seconds after the start (2560 x 1369 on this desktop, the owner's size). One run at a time. The runner is not in git (scratch). Numbers are the report lines: `frame avg / worst` (window thread busy), the worst frame's steps and the step that the report names. Frames above 30 ms (`frame hitch`) did not occur in any run.

| run | worst frame, whole run | the worst frame's steps (ms) | largest `fluid` | largest `draw prologue` / `surface configure` | largest `sync drops` | largest `sync scan` |
|---|---|---|---|---|---|---|
| before, 1280x800, 40 s (`3cf7ca8`, the viewer as at `792afa3`) | 6.4 | sea 2.35, fluid 1.96 | 2.10 | 0.00 | 0.00 | 0.00 |
| after, 1280x800, 40 s (`d081d4d`) | 7.1 | sea 3.25, fluid 2.15 (the recentre upload) | 2.15 | 0.07 | 0.00 | 0.00 |
| before, maximized, 100 s | 11.2 (resize) | draw prologue 10.10 (the surface configure) | 3.19 | 10.10 | 0.79 | 0.21 |
| after, maximized, 100 s (`d081d4d`) | 11.1 (resize) | surface configure 9.49 | 1.95 | 9.49 (own step) | 0.00 | 0.32 |

The maximized runs had 12 and 50 reports respectively (the number of reports depends on the run's frame rate); the maxima are over all of them.

**Owner's points, as measured and explained**
- **(a) `sync scan 6.72 ms` at group 1 -> 2 after maximizing.** Not reproduced in the 100 s maximized runs (largest `sync scan` 0.21 ms before, 0.32 ms after; the zoom hook passes through group 1 -> 2 repeatedly). The part of `sync_chunks` that could cost that is the release of upload holds: `drop_staged` and the finished upload's `erase` dropped the last reference to a chunk on the window thread (about 1 ms per chunk, untimed, so it counted in the scan). The holds now go to the chunk workers (`FrameRenderer::release`); `sync drops` fell from 0.79 to 0.00 in these runs. Whether the owner's 6.72 ms was that is not proved: his frame is the test to repeat with this build.
- **(b) `fluid` on the window thread, 1.6-1.9 ms every few frames and 3.91 ms once.** Measured with temporary timers (not committed) in `FluidGpu::update` and `prepare_window`, on 2560 x 1369: the step recording and the sim steps take 0.01-0.1 ms; the rest is (i) destroying a finished job (its grid is tens of megabytes of textures), 0.5-1.3 ms, now freed on a background thread (`15d7a3f`); (ii) `apply_job`, the upload of a recentred window's strips in one step: 1.3-1.9 ms, **not changed**: spreading it changes what the sim sees for a few frames (OQ-097, a T14 item or a separate small task). Largest `fluid` 3.19 -> 1.95 ms.
- **(c) the resize frame, `draw prologue 9.18 ms`.** The prologue held the surface reconfigure: `wgpuSurfaceConfigure` rebuilds the swapchain in the driver, 9.5 ms here. It is its own step now (`d081d4d`). The planes for the new window are pre-created (OQ-093), the offscreen targets are made in `render` (their step is small on this resize). The reconfigure cannot be made ahead on the window thread, because the size is known only at the resize: OQ-098 (debounce the reconfigure during a live resize, owner's choice).
- **(d) the first frame.** Unchanged (accepted).
- **(e) gaps of 10-23 ms with a busy time of 1-2 ms.** In these runs the worst gaps were at the resize (busy 11 ms, the reconfigure) and at the first frame; the other gaps were 3-7 ms with busy 1-3 ms. The display wait is the present path and is OQ-094; not changed here.

**Memory of the offscreen targets (decision: a rule and its measure, not a number).** `GpuFrame::offscreen_bytes()` is the sum of `under` (6 bytes per texel: RGBA8 and R16Uint terrain) and `scene` (4 bytes), the sizes the code allocates. The largest target is made for the largest grid the views compose: at the smallest scale 0.95 / 2.1 the grid is 2.21 x the window per axis and the +25 % of `ensure_under` makes 2.77 x, so about 7.7 x the window's area. Worst points, computed from those sizes (not sampled on the adapters):

| window | grid target (texels) | `under` + terrain | `scene` | total |
|---|---|---|---|---|
| 1280 x 800 | 3545 x 2216 = 7.9 M | 47 MB | 31 MB | about 78 MB |
| 2560 x 1440 | 7091 x 3989 = 28.3 M | 170 MB | 113 MB | about 283 MB |
| 3840 x 2160 | 10637 x 5983 = 63.6 M | 382 MB | 255 MB | about 637 MB |

Given back after `GpuLimits::offscreen_shrink_ms` (3000 ms) of smaller views (`f2e5fce`; test: a target shrinks after its timer and is kept with the timer off, RTX 4090). The iGPU shares system memory, so the 2560 x 1440 and 3840 x 2160 figures matter there: OQ-099 asks the owner for a budget. The sizes have not been measured on the iGPU.

**Gates for this branch.** MSVC: the full build is warning-free (`cmake --build --preset msvc`, zero warnings). clang-format 23.1.3 clean on every changed file. GCC 13.3 `-O3 -Werror` (the CI flags, WSL): viewer, main, still, cli, the app tests, gpu, gpu_fluid, frame, the GPU tests and resample compile clean; one sign conversion in a new test was found and fixed (`5d1dcc7`). GPU tests named by adapter: `anno_render_gpu_tests` with `ANNO_GPU_ADAPTER=0` (NVIDIA GeForce RTX 4090, Vulkan) and `ANNO_GPU_ADAPTER=1` (AMD Radeon(TM) Graphics, Vulkan), the full binary on both: 78 of 78 cases pass on each, with the game directory. The two new cases are `released upload holds are freed by the chunk workers, not by the caller` and `offscreen targets shrink after the views have been small for offscreen_shrink_ms`. The second failed on the iGPU at first: `GpuFrame::create` measures the river cost there with a 1024 x 1024 frame, which leaves a 1280 x 1280 `under` target, so the first 800 x 500 view was already below a third of it and the timer ran out before the next call. The test now uses a 2000 x 1200 view that cannot fit that target. Mutation check: with `shrink_due` returning false, the test fails (RTX 4090).

### 2026-10-21 - T3 follow-up: offscreen target re-grow (scene grid floored, not dropped), Windows 11, Ryzen 9800X3D, MSVC Release, branch task/T3-followup

The `scene` grid is never freed outright: when idle it shrinks only toward the window size (+25 %), so the first zoom-out after a shrink re-grows it from the window. Measured with a GPU test (`the scene grid is kept at the window floor when idle`) that renders a 2560 x 1440 window at the smallest composed scale (0.95 / 2.1, grid 2.21 x the window), lets the targets shrink to the window floor (`offscreen_shrink_ms` 100 ms for the test), then renders the zoomed-out frame again and reads the `under` step (the texture (re)allocation of `under`, terrain and `scene`).

| adapter | re-grow (`under` step) | worst `offscreen_bytes()` |
|---|---|---|
| 0 — NVIDIA GeForce RTX 4090 (Vulkan) | 0.02 ms | 285 MB |
| 1 — AMD Radeon(TM) Graphics (Vulkan) | ~14 ms (13.8, 13.9, 14.1, 15.1 over four runs) | 285 MB |

The iGPU re-grow (allocating ~280 MB of shared-memory textures) is over the 2 ms a frame can spare, so `GpuLimits::offscreen_shrink_ms` defaults to **0 (never shrink)**: the targets are kept at their largest until the owner sets a memory budget (OQ-099). On the RTX 4090 the re-grow is negligible; the shrink could be enabled there alone if the memory mattered.

### 2026-10-22 - T11 stage 2a: GPU-built chunk objects/base (compute pass), Windows 11, Ryzen 9800X3D, branch task/T11-gpu-chunks, commit of the stage-2a integration

`bench_frame <game_dir> D_256x256_0045_5B.scp --renderer gpu --res 2560x1440 --groups 0,1,2 --styles original --chunks async --quick`, `--chunk-build cpu` against `--chunk-build gpu`, RTX 4090 (adapter 0, Vulkan), idle machine. The GpuList builder has the CPU worker emit only the draw list (no sprite blits) and the compositor's compute pass fill the objects and base planes; the Cpu builder replays the list with the blitter on the worker.

| path (group 0) | p50 ms | p95 ms | p99 ms | max ms | cold ms | stage 1 (cold) | stage 4 (cold) |
|---|---|---|---|---|---|---|---|
| Cpu builder, cold | - | - | - | - | 119.5 | 40.6 | 75.7 |
| GpuList builder, cold | - | - | - | - | 56.5 | 15.3 | 38.8 |
| Cpu builder, static | 1.2 | 4.5 | 4.6 | 4.6 | | | |
| GpuList builder, static | 1.1 | 3.6 | 55.6 | 55.6 | | | |

- The cold frame at group 0 (24 chunks of the canyon island) roughly halves, 119.5 to 56.5 ms: the objects stage (stage 1) and the objects part of stage 4 move off the CPU. The per-chunk CPU build time ("chunk ms") drops (the worker builds only the list); fewer chunks show as "missing" in the first static frames.
- The 55.6 ms p99/max spike in the GpuList "static" run is the one-time lazy creation of the compute pipeline plus the first chunk-atlas allocation on the first GpuList frame (`cb_init` / `cb_grow_atlas`); a warm-up frame hides it. A later commit should create the pipeline in `GpuFrame::create` under the limits, as the other pipelines are.
- Interactive frames (static/pan) stay sub-millisecond at p50 and under the 2 ms 500 Hz budget at p95 on both builders.

**Not yet measured (remaining 2a):** per-chunk GPU time of the compute pass with GPU timestamps on adapter 0 (RTX 4090) and adapter 1 (AMD Radeon, Vulkan), the `bin_px` (16/32/64) sweep chosen from it, and the 1280 x 800 / 2560 x 1440 p50/p95/p99/max on both adapters with the pan-and-zoom script. These need timestamp writes around the dispatch (not yet added) and the derived-level path for the zoom script.

### 2026-10-22 b - T11 stage 2a follow-ups: pan-and-zoom matrix, bin_px sweep, resident atlas (B4)

Follow-up to the section above (review item 4 / C1 condition). Idle machine (Ryzen 9800X3D), MSVC Release, branch `task/T11-gpu-chunks`. Scene `D_256x256_0045_5B.scp` (the canyon island), group 0, original water, GPU renderer. `bench_frame <game> <scene> --renderer gpu --chunk-build cpu|gpu --res 1280x800,2560x1440 --groups 0`, `ANNO_GPU_ADAPTER` 0 (RTX 4090, Vulkan) and 1 (AMD Radeon integrated, Vulkan). The `async` path (the interactive one: the margin is uploaded ahead, chunks never block the window thread) pan rows:

| adapter | builder | res | path | p50 ms | p95 ms | p99 ms | max ms |
|---|---|---|---|---|---|---|---|
| 0 RTX 4090 | Cpu | 1280×800 | pan fast | 0.4 | 0.5 | 0.8 | 0.8 |
| 0 RTX 4090 | GpuList | 1280×800 | pan fast | 0.4 | 0.6 | 1.0 | 2.7 |
| 0 RTX 4090 | Cpu | 2560×1440 | pan fast | 0.7 | 0.9 | 1.3 | 1.3 |
| 0 RTX 4090 | GpuList | 2560×1440 | pan fast | 0.8 | 1.9 | 3.7 | 4.3 |
| 0 RTX 4090 | GpuList | 2560×1440 | pan slow | 0.8 | 1.2 | 2.9 | 5.6 |
| 1 Radeon iGPU | Cpu | 1280×800 | pan fast | 3.5 | 5.4 | 6.1 | 6.3 |
| 1 Radeon iGPU | GpuList | 1280×800 | pan fast | 3.5 | 5.9 | 7.9 | 7.9 |
| 1 Radeon iGPU | Cpu | 2560×1440 | pan fast | 10.3 | 14.1 | 15.4 | 15.8 |
| 1 Radeon iGPU | GpuList | 2560×1440 | pan fast | 11.0 | 15.2 | 22.4 | 23.2 |

- Interactive (async) frames are within the frame budget on both adapters and both resolutions for both builders; GpuList is within a millisecond or two of Cpu on the window thread (the GPU does the objects work instead). The iGPU's cost here is the water/sea compositor (stage 4), not the chunk build (stage 1 is 0.1-0.2 ms).
- The `blocking` path (chunks built on the window thread, worst case) shows where GpuList helps: the cold group-0 frame's compositor stage (stage 4) drops from ~36 ms (Cpu) to ~6 ms (GpuList) at 1280×800 because the window thread uploads a draw list instead of 3.9 MB of texels per chunk; the per-chunk build on the worker (stage 1) is similar for both since both emit the same list.

**`bin_px` 16/32/64 sweep** (`--bin-px`, GpuList, 2560×1440, group 0, blocking cold + pan): the frame-level times are flat within noise on BOTH adapters (RTX 4090 cold chunk-stage 42-44 ms, iGPU 41-43 ms; pan-fast chunk ms ~4-5 ms; no monotonic trend across 16/32/64). The per-pixel bin walk is not the frame bottleneck — the cold frame is dominated by the CPU worker's list + ground/field/terrain build, and interactive frames are GPU-bound elsewhere. **Choice: keep `bin_px` 32** (the default), since 16 and 64 give no measurable benefit. Isolating the compute dispatch to tune `bin_px` more finely needs a per-chunk GPU timestamp (below).

**Resident chunk atlas** (new `GPU chunk atlas:` summary line, `--zoom-pan`): the canyon at group 0, 2560×1440 holds a **peak ~11.8 MiB** of static chunk frames resident, with **0 CPU fallbacks**, far under the OQ-096 budget (256 MiB integrated / 512 MiB discrete, now covering both atlases together). Single_24 at group 0 is the same ~11.8 MiB (the visible static frame set of one view).

**Still open (one 2a measurement):** the GPU time of the compute **dispatch per chunk** in isolation, with timestamp writes around `cb_encode`, on both adapters. The frame-level `bin_px` sweep above stands in for the tuning (no difference), but the isolated dispatch cost is not yet instrumented; it is the remaining B4 item.

### 2026-10-23 - T14 stage 1: the compositor's cost per pass on the Radeon iGPU, and the wave window bounded at far zoom, Windows 11, Ryzen 9800X3D (16 threads), MSVC Release, branch task/T14-weak-gpu

**Method.** `bench_frame <game> D_256x256_0045_5B.scp --renderer gpu --styles original,enhanced --res 1280x800,2560x1440 --derived 5 --scale-cost 0:1.0,1:1.0,2:1.0,1:0.452,2:0.475,3:0.475,5:0.475,7:0.475 --cost-frames 60 --threads 16 [--cost-png dir]`, `ANNO_GPU_ADAPTER=1` (AMD Radeon(TM) Graphics, Vulkan, the integrated adapter) and `0` (NVIDIA GeForce RTX 4090, Vulkan), canyon island, the default `GpuLimits` (blocking chunks and wave window, as headless), the animation clock advancing 16 ms per frame (a 60 Hz display: 1.9 wave-simulation steps per frame). Points: scale 1 at the three built groups, the T3 worst far points (group 1 at 0.452, group 2 at 0.475, the far end of a level: grid 2.2 x the window per axis) and the derived levels 3, 5, 7 at 0.475. 60 frames per point after the chunks are built and uploaded. **GPU ms per pass** (new, `GpuLimits::gpu_timing`, `GpuFrame::pass_gpu_ms`): a timestamp at the start and the end of each pass: `fluid` (the wave simulation's compute pass, enhanced water at groups 0-2; it was outside the timed span before this stage, so T3's figures did not count it), `terrain`, `under` (ground and water objects below the waterline), `sea` (the sea shading), `objects` (water objects above the waterline, the cached objects, ships; with timing on, recorded as a pass of its own over the sea with LoadOp_Load: the same picture, GPU test, and the same frame time within 0.1 ms, measured with the split switched off: 3.34 / 3.43, 13.70 / 13.76, 16.52 / 16.55, 51.46 / 51.31 ms at four points), `resample`, and `frame` (from the first pass's start to the last one's end). **Before** = 8313b62 (the timestamps, nothing bounded), **after** = f7112b0 (the wave window bounded). The tables' runs finished at 03:16 (before) and 03:32 (after) local time and the PNG runs at 03:39, all before the PC's standby at 03:41 and 04:41-05:23 (not affected); the split check below ran after 05:23. The full per-pass tables (p50 / p95 / p99 / max of every pass, both adapters, both sizes, both water styles, before and after) are in [`t14-passes.md`](t14-passes.md).

**Where the iGPU's time goes (1280 x 800, enhanced water, p50 ms, before):**

| point | grid | fluid | terrain | under | sea | objects | resample | frame |
|---|---|---|---|---|---|---|---|---|
| group 0 at 1 | 1344x832 | 11.01 | 0.67 | 1.11 | 2.36 | 0.87 | 0.60 | 16.68 |
| group 2 at 1 | 1344x832 | 13.61 | 0.69 | 1.08 | 3.25 | 0.93 | 0.61 | 20.21 |
| group 1 at 0.452 | 2880x1792 | 42.06 | 2.62 | 2.45 | 10.58 | 4.20 | 1.53 | 63.51 |
| group 2 at 0.475 | 2752x1728 | 50.31 | 2.23 | 2.23 | 25.05 | 3.38 | 1.46 | 84.56 |
| level 7 at 0.475 | 2752x1728 | - | 0.62 | 1.23 | 40.46 | 0.46 | 1.30 | 44.11 |

**Reading, pass by pass (item 2 of the stage):**
- **What the passes hold.** The river shading (flow lattice or legacy shimmer) and the coast (shore distance, swash, foam, the waterline against the terrain) are shaded per pixel inside `sea`; there is no separate coast or river pass. The GPU chunk build (`chunk_build_cs`, T11) and the derived levels' reduce pass are their own submissions when a chunk arrives (at most `gpu_chunks_per_frame` / the upload budget per frame) and run in no frame of a settled view, so they have no figure here (T11's open per-dispatch measurement).
- **`fluid` grows with the world area shown, not with pixels: bounded.** The window covers the view's tiles at the group's cells per tile (16 / 8 / 4), so on the composed grid of a far point it has 2.17 M cells against 0.59-0.69 M at scale 1 (1280 x 800; 4.19 M, the 2048² cap, against 1.64-1.81 M at 2560 x 1440), and the pass costs about 9-13 ns per cell and step on the iGPU, linear in the cells (42 / 50 ms at the far points). At 2560 x 1440 the far points need about 2500 cells per side and hit the window's 2048 cap, so before the bound the view's corners lay outside the window and showed the analytic waves; the bounded window (1344 / 1408 cells per side) covers the whole view. Bound: `GpuLimits::fluid_max_cells_per_px`, the cells per screen pixel along x, default 0.25 (`fluid::kDefaultMaxCellsPerPx`, reason: the built groups' density at scale 1 is 0.19-0.20, so they keep their cells down to scale 0.8 and are halved once below; iso.md "Wave simulation", "Bounded at far zoom"); off where one cell per tile is still denser (never at the built levels with the default). Applied on an adapter that is not discrete in the runs below (f7112b0); since 587ca4c on every adapter (OQ-130 decided, addendum at the end of this section).
- **The composed-grid passes (`terrain`, `under`, `sea`, `objects`, `resample`) grow with the grid, i.e. with the world area between two level switches,** up to about 5 x the window's pixels at the far end of a level (2880 x 1792 against 1280 x 800: 5.0 x, the T3 bound with the grid's rounding; the grid stays O(window)). Their cost per grid pixel is constant for a given content; bounding the grid further is the composed-pixel budget of stage 2 (item 3 of the T14 entry: `--zoom-max-upscale` from the measured cost per pixel), not this stage.
- **`sea` costs about 9 ns per deep-sea pixel on the iGPU in enhanced water** (a separate run, level 7 at scale 1 with `--wave-sim off --cost-frames 10`, a view of open sea: 10.0 ms for 1.12 M grid pixels; land pixels return early). Zoomed out, the view holds more sea: group 2 at 0.475 has 5.3 ns per grid pixel, the derived levels 7.6-8.5 (36-40 ms), group 1 at 0.452 (mostly land) 2.0. That grows with the share of sea in the view, not with the area per pixel: the shading per pixel is the same at every zoom. A cheaper sea shading tier is stage 2. Original water: 1.1 ns per pixel.
- **The rest:** no other pass grows with the area shown. The chunk build (`chunk_build_cs`) and the reduce pass run in their own submissions when a chunk arrives (bounded per frame by `gpu_chunks_per_frame` / the upload budget), not per frame of a settled view, and are not in these tables.

**Before -> after, the Radeon iGPU (GPU ms p50 / p95 / p99 / max; the full table per pass in t14-passes.md):**

| window | water | point | grid | fluid before | fluid after | frame before | frame after |
|---|---|---|---|---|---|---|---|
| 1280x800 | enhanced | group 0 at 1 | 1344x832 | 11.01 / 11.05 / 11.06 / 11.06 | 10.95 / 10.99 / 11.29 / 11.33 | 16.68 / 16.82 / 16.91 / 19.33 | 16.65 / 16.80 / 17.20 / 19.82 |
| 1280x800 | enhanced | group 1 at 1 | 1344x832 | 11.01 / 11.04 / 11.06 / 11.13 | 10.95 / 10.98 / 11.01 / 11.08 | 17.34 / 17.47 / 17.51 / 17.64 | 17.22 / 17.41 / 17.47 / 17.57 |
| 1280x800 | enhanced | group 2 at 1 | 1344x832 | 13.61 / 13.67 / 13.69 / 13.88 | 13.60 / 13.64 / 13.66 / 14.03 | 20.21 / 20.39 / 20.48 / 22.74 | 20.14 / 20.32 / 23.35 / 23.39 |
| 1280x800 | enhanced | group 1 at 0.452 | 2880x1792 | 42.06 / 42.23 / 42.27 / 42.27 | **13.64** / 13.70 / 13.70 / 14.29 | 63.51 / 63.81 / 63.84 / 66.38 | **35.21** / 35.57 / 36.12 / 41.69 |
| 1280x800 | enhanced | group 2 at 0.475 | 2752x1728 | 50.31 / 50.55 / 50.60 / 50.62 | **17.17** / 17.22 / 17.28 / 17.29 | 84.56 / 85.15 / 85.18 / 87.82 | **51.60** / 51.78 / 51.88 / 54.79 |
| 1280x800 | enhanced | level 3 / 5 / 7 at 0.475 | 2752x1728 | - | - | 43.5-44.1 / 43.7-44.8 / 43.7-44.9 / 45.1-47.3 | 43.5-44.1 / 43.7-44.8 / 43.8-44.9 / 45.1-47.2 |
| 1280x800 | original | groups 0-2 at 1 | 1344x832 | - | - | 3.32-3.57 / 3.39-3.63 / 3.41-3.64 / 3.66-4.14 | 3.35-3.57 / 3.45-4.91 / 3.47-4.99 / 3.98-5.29 |
| 1280x800 | original | group 1 at 0.452 / group 2 at 0.475 | 2880x1792 / 2752x1728 | - | - | 15.12 / 16.32 / 18.12 / 19.15; 14.30 / 14.54 / 17.22 / 17.76 | 15.25 / 16.74 / 18.03 / 22.73; 14.34 / 16.60 / 18.98 / 22.26 |
| 1280x800 | original | level 3 / 5 / 7 at 0.475 | 2752x1728 | - | - | 9.03-13.59 / 9.29-13.73 / 9.63-13.77 / 12.85-14.36 | 9.01-13.58 / 9.57-13.68 / 9.78-14.20 / 13.59-16.83 |
| 2560x1440 | enhanced | groups 0-2 at 1 | 2624x1472 | 31.10-40.76 (p50) | 30.70-40.76 (p50) | 51.35-69.89 / 51.93-70.32 / 51.94-70.41 / 51.95-74.14 | 51.13-69.89 / 51.46-70.20 / 51.47-70.31 / 51.81-70.57 |
| 2560x1440 | enhanced | group 1 at 0.452 | 5696x3200 | 92.82 / 93.82 / 93.94 / 93.99 | **41.05** / 41.85 / 41.94 / 41.97 | 206.93 / 209.34 / 214.21 / 216.40 | **158.39** / 159.79 / 159.88 / 164.82 |
| 2560x1440 | enhanced | group 2 at 0.475 | 5440x3072 | 106.13 / 107.21 / 107.25 / 111.39 | **53.17** / 53.90 / 54.05 / 54.23 | 311.44 / 317.06 / 318.98 / 320.84 | **260.72** / 263.83 / 266.58 / 267.00 |
| 2560x1440 | enhanced | level 3 / 5 / 7 at 0.475 | 5440x3072 | - | - | 156.4-157.4 (p50), max 160.3-164.8 | 156.6-157.9 (p50), max 159.9-164.3 |
| 2560x1440 | original | groups 0-2 at 1 / far points / levels 3-7 | | - | - | 12.1-12.7 / 49.7-55.0 / 33.1-38.2 (p50) | 12.2-12.7 / 49.7-54.9 / 33.2-38.0 (p50) |

The wave window at the far points: 2.17 M -> 0.69 M cells at 1280 x 800 (as at group 2's scale 1), 4.19 M -> 1.81 M (group 1) and 1.98 M (group 2) at 2560 x 1440 (4 -> 2 cells per tile at group 2, 8 -> 4 at group 1). Scale 1 and the points without the simulation are unchanged within noise (the code path is the same there).

**RTX 4090 (adapter 0):** every frame at every point is at most 5.6 ms GPU before and after (the worst: group 1 at 0.452, 1280 x 800, enhanced, 4.85 -> 4.16 ms p50, of it the simulation 2.02 -> 1.44 ms; at f7112b0 the code path there was the same before and after, the bound then being off on a discrete adapter, so the differences between the two runs, up to 0.7 ms either way, are the card's clock states; with the bound on every adapter see the addendum). With the bound forced on (`--fluid-cells-per-px 0.25`): the simulation at the far points 0.08-0.46 ms. The 8 ms line holds on the RTX 4090 at both sizes.

**Item 3, the target (no GPU frame above 8 ms on the iGPU at 1280 x 800 at any zoom): missed.** After stage 1 the worst frame at 1280 x 800 is **51.6 ms p50 / 54.8 ms max** (group 2 at 0.475, enhanced water: 43.6 ms over the line), the derived levels 44 ms (enhanced), and original water 15.3 ms p50 / 22.7 ms max (group 1 at 0.452); even scale 1 is 16.7-20.2 ms with enhanced water and 3.4-3.6 ms with original water. **Left for stage 2, by size at the worst point:** the sea shading (25 ms there, 36-40 ms on the derived levels: a cheaper deep-sea tier), the wave simulation at its scale-1 cost (11-17 ms at 60 Hz pacing: a lower bound or off on the iGPU, `fluid_max_cells_per_px` is the switch), the composed grid's 4.4-5 x pixels (terrain + under + objects + resample 8.9 ms there with original water: the composed-pixel budget). At 2560 x 1440 the worst frame is 260.7 ms p50 (group 2 at 0.475, enhanced) and 55 ms with original water.

**What the bench's fixed clock hides.** The bench advances the animation 16 ms per frame. The viewer advances it by the real frame time, and the simulation steps 1/120 s at a time, up to 12 per frame: where a step costs about as much as the 8.3 ms it simulates (7-9 ms on the iGPU at the bounded far points, 22-26 ms unbounded), the real-time frame grows until it reaches 12 steps per frame (group 2 at 0.475 bounded: about 34 + 12 x 8.9 = 140 ms; unbounded about 350 ms). Stage 2's simulation tier has to bring the cost per step well under 8.3 ms on the iGPU (OQ-130).

**PNGs** (the picture of the last timed frame of every point, `--cost-png`): `C:\Users\altin\Pictures\anno-review\T14\before\a<adapter>\` and `...\after\a<adapter>\` (canyon island, every point and style above); the ten `0200Single` scenarios and the canyon island at group 2 at 0.7 and 0.475 and group 1 at 0.452, 1280 x 800, enhanced water, with the bound off (`...\far\a<adapter>\cells0\`) and at 0.25 (`...\far\a<adapter>\cells0.25\`), both adapters; differences amplified 8 x in `...\far\diff\`, 4 x crops in `...\far\crops\`. Up to 1.5 % of a far frame's pixels differ by more than 8 levels between the two, all at coasts and in shallow water (a rerun of one build: under 0.1 %); OQ-130.

**Addendum 2026-10-23 (OQ-130 decided: the bound on every adapter, 587ca4c).** The owner chose the option that scales best in dense late-game multiplayer; `GpuLimits::fluid_max_cells_per_px` defaults to 0.25 on every adapter (0 = off). RTX 4090 (adapter 0, Vulkan), the same command at 587ca4c, the far points and scale 1 with the default bound on, GPU ms p50 / p95 / p99 / max (run at 06:43, after the standby):

| window | water | point | grid | fluid | sea | frame |
|---|---|---|---|---|---|---|
| 1280x800 | enhanced | group 0 at 1 | 1344x832 | 0.05 / 0.05 / 0.05 / 0.05 | 0.02 / 0.03 / 0.03 / 0.03 | 0.14 / 0.15 / 0.15 / 0.15 |
| 1280x800 | enhanced | group 2 at 1 | 1344x832 | 0.18 / 0.19 / 0.19 / 0.19 | 0.08 / 0.08 / 0.08 / 0.09 | 0.39 / 0.39 / 0.39 / 0.39 |
| 1280x800 | enhanced | group 1 at 0.452 | 2880x1792 | 0.23 / 0.23 / 0.23 / 0.23 | 0.32 / 0.32 / 0.33 / 0.33 | 0.88 / 0.89 / 0.90 / 1.44 |
| 1280x800 | enhanced | group 2 at 0.475 | 2752x1728 | 0.30 / 0.30 / 0.30 / 0.30 | 0.48 / 0.49 / 0.50 / 0.50 | 1.09 / 1.10 / 1.10 / 1.11 |
| 1280x800 | original | group 1 at 0.452 | 2880x1792 | - | 0.05 / 0.05 / 0.05 / 0.05 | 0.19 / 0.19 / 0.19 / 0.19 |
| 1280x800 | original | group 2 at 0.475 | 2752x1728 | - | 0.05 / 0.05 / 0.05 / 0.05 | 0.17 / 0.18 / 0.18 / 0.18 |
| 2560x1440 | enhanced | group 0 at 1 | 2624x1472 | 0.45 / 0.46 / 0.46 / 1.03 | 0.25 / 0.25 / 0.25 / 0.25 | 1.06 / 1.07 / 1.07 / 1.62 |
| 2560x1440 | enhanced | group 2 at 1 | 2624x1472 | 0.59 / 0.60 / 0.60 / 1.09 | 0.34 / 0.34 / 0.34 / 0.34 | 1.29 / 1.30 / 1.30 / 1.79 |
| 2560x1440 | enhanced | group 1 at 0.452 | 5696x3200 | 0.50 / 0.50 / 0.50 / 0.50 | 1.15 / 1.16 / 1.16 / 1.16 | 2.73 / 2.74 / 2.74 / 2.75 |
| 2560x1440 | enhanced | group 2 at 0.475 | 5440x3072 | 0.48 / 0.48 / 0.48 / 0.48 | 1.45 / 1.46 / 1.46 / 1.48 | 2.60 / 2.61 / 2.66 / 2.98 |
| 2560x1440 | original | group 1 at 0.452 | 5696x3200 | - | 0.51 / 0.51 / 0.51 / 0.51 | 1.54 / 1.54 / 1.54 / 1.54 |
| 2560x1440 | original | group 2 at 0.475 | 5440x3072 | - | 0.42 / 0.42 / 0.42 / 0.42 | 1.20 / 1.20 / 1.20 / 1.20 |

The wave window on the RTX 4090 now has the iGPU's cells (0.69 M at the 1280 x 800 far points, 1.81 / 1.98 M at 2560 x 1440). Every frame is under 3 ms; the absolute values move with the card's clock state between runs (the scale-1 rows with the same window as before read 0.14-1.3 ms here against 0.47-4.1 ms in the first run), so the comparison is within this table, not across runs.

## T45 — `World::load` and `run_turn` (2026-10-27)

`anno-sim-replay join` on stream 114, age 30 000 ticks (600 calls to `World::advance(100)` from tick 0). One timed turn is `run_turn` with `step_ms` 10 and `steps` 10, which is 100 game ms and 50 ticks. A tick is 2 game ms. The clock is `steady_clock`. The hash is outside the timer. Host: AMD Ryzen 7 9800X3D, 8 cores, 16 logical processors, Windows, MSVC 19.51 Release, `threads=16`. Load repetitions 20 after 2 warm-up loads. Replay samples 150 (5 repetitions of 30 turns) after 2 warm-up sequences. Raw logs stayed in the worktree `cache\` and are not committed. The budget reading is ADR 0003 §13. The model defaults were not changed.

`World::load` is the in-memory save only. The phases below are one sample each. The registry and template samples include their status line (`island templates 429`). They are not the load distribution.

| Scenario | io µs | registry µs | templates µs | parse µs | create+age µs | save µs | save bytes |
|---|---:|---:|---:|---:|---:|---:|---:|
| `Scenes/0100Kampagne01/S01_07_01.sze` | 8 947 | 832 759 | 253 483 | 26 555 | 393 920 | 30 868 | 11 474 170 |
| `Scenes/0011SingleEndlosAddon/endless02.sze` | 25 002 | 826 977 | 201 932 | 57 606 | 241 427 | 54 178 | 20 349 455 |
| `Scenes/0010SingleEndlos/03_1024_med_256.sze` | 195 | 829 847 | 198 558 | 37 820 | 82 658 | 34 271 | 12 906 634 |

T41's save sizes at the same age were 11 474 169, 20 349 454 and 12 906 633 bytes. Each remeasured file is one byte larger. The fragment counts stay 9 659, 17 130 and 10 865. The extra byte was not attributed to a field. The T41 hash medians were not remeasured.

| Scenario | `World::load` µs min / median / p95 / max | `run_turn` scripted µs min / median / p95 / max | scripted turns/s, sum |
|---|---:|---:|---:|
| `S01_07_01` | 24 335 / 26 807 / 27 702 / 27 977 | 232 / 659 / 1 627 / 1 966 | 1 317 |
| `endless02` | 42 858 / 46 004 / 51 401 / 54 239 | 234 / 397 / 925 / 2 222 | 1 915 |
| `03_1024_med_256` | 25 018 / 27 647 / 31 175 / 32 570 | 171 / 217 / 330 / 635 | 4 245 |

The empty workload's slowest sample is `endless02` at 2 278 µs (438 turns/s). Encoded command bytes, the same on every scenario: empty 0, one `SetSpeed` 16, synthetic burst 76 commands and 1 216 bytes. The burst is over the 1 200-byte cap. It is not a play distribution. Packet overhead was labeled and not added. Detail, the empty-workload rows, and the labeled model result are in ADR 0003 §13.

## T10c — one-worker render baseline (2026-10-28)

Measured on the unchanged head `416993fa6a6f9dc821455df5d414cff5833ac7f0`, before the scheduler change. Host: AMD Ryzen 7 9800X3D, 16 logical processors, Windows, 33408942080 bytes physical RAM, commit limit 108272488448 bytes. Physical RAM is the binding limit; commit headroom stayed above 55 GiB. The sampler is `tools/sample_memory.ps1` at 1 Hz (available physical memory, available commit, process working set, private bytes, peak pagefile, thread count). It stops the diagnostic run below 2147483648 bytes of available physical memory or available commit. That threshold was not hit. The CSV logs stay in the worktree `cache\` and are not committed. Peak pagefile is `PeakPagefileUsage`. A 1 Hz private sample can miss a sub-second spike, so the high-water is the peak.

### Legacy river, one internal worker

`anno_render_tests --test-case="*legacy river shading*" --test-suite=heavy --no-colors`, `ANNO_TEST_THREADS=1`, game directory set. Result: 1 passed, 0 failed, 146 skipped, 47 assertions, exit 0. Sampled elapsed 297.646 s. The meta clock runs from 10:42:12.898 to 10:47:11.339 +01:00.

| quantity | bytes |
|---|---:|
| free physical at start | 16507674624 |
| minimum available physical | 8656285696 |
| process peak private | 8044425216 |
| maximum sampled private | 8044388352 |
| minimum available commit | 59670302720 |
| maximum commit charge | 48602185728 |

The thread count was 4 during load and 1 while the later renders ran. The minimum of the private series is the process exiting, not a between-item floor. Stderr had no `[anno-test memory]` line: one worker never waited under the old harness.

### Labeled full-map CPU renders

`anno1503 --view <scenario> --group 2 --renderer cpu --png <cache file>`, one fresh process each. The PNG files were deleted after the run. Additional bytes are the peak private minus the maximum private in the first 1.5 s. Single_24 finishes inside that window, so its additional figure is only the pagefile high-water above the highest 1 Hz sample.

| run | image | pre-render private | peak private | additional | bytes/pixel | elapsed s | min available physical |
|---|---|---:|---:|---:|---:|---:|---:|
| Single_01 enhanced legacy | 16960×9170 | 991899648 | 7620612096 | 6628712448 | 42.622 | 38.891 | 9663873024 |
| Single_01 original | 16960×9170 | 993218560 | 7620579328 | 6627360768 | 42.613 | 17.485 | 9676578816 |
| Single_01 enhanced flow | 16960×9170 | 993366016 | 7620018176 | 6626652160 | 42.609 | 38.926 | 10433630208 |
| Single_10 enhanced legacy | 17440×9131 | 1003626496 | 7608279040 | 6604652544 | 41.475 | 35.841 | 10673950720 |
| Single_14 enhanced legacy | 14080×9291 | 892149760 | 6191976448 | 5299826688 | 40.513 | 27.690 | 13276516352 |
| Single_24 enhanced legacy | 7200×2811 | 949936128 | 1268973568 | 319037440 | 15.763 | 5.170 | 16972197888 |

Single_01 group 2 is 16960 × 9170 = 155523200 pixels. The scheduler's per-pixel rate is 43, the integer ceiling of the Single_01 enhanced-legacy ratio 42.622. `43 × 155523200 = 6687497600`, which is 58785152 bytes above that run's additional private. Original and flow on the same map matched the enhanced peak within a few megabytes, so the rate is not lowered for original water. The other maps were lower per pixel. A group-1 region on its own was not measured.

The legacy-river process peak is 423813120 bytes above the fresh Single_01 `--png` peak. That gap is memory the harness already held (figuren volumes, Anlagen, Felder). The reservation is the additional render bytes. Free memory already excludes what is resident, so those bytes are not reserved again.

Two full-map renders, about twice the 6628712448-byte additional, do not fit in the 16507674624 bytes free at the start of the one-worker run once 4 GiB of headroom is kept. The headroom default is 4 GiB because that run's minimum available physical memory was 8656285696 bytes: one large render did not need a lower headroom to be admitted.

## T10c F1 — setup plus the drawn rectangle (2026-10-28)

The per-pixel rate and the setup high water below replace the 43 bytes/pixel figure in the section above. They are the constants in `src/test_support/include/anno/test/budget.hpp` on head `f2ded2141905d761245c0703a7be614c8cafc251`. Sampler `tools/sample_memory.ps1` at 200 ms. Peak is `PeakPagefileUsage`. Viewer `build/msvc/src/app/Release/anno1503.exe`, one fresh process per run, CPU renderer, enhanced legacy water. CSVs stayed in the worktree `cache/`.

| run | pixels | peak private | subtracted high water | additional | bytes/pixel |
|---|---:|---:|---:|---:|---:|
| Single_01 group 2, 16960×9170 | 155523200 | 7620632576 | 530382848 | 7090249728 | 45.590 |
| Single_24 group 2, 7200×2811 | 20245320 | 1268985856 | 356696064 | 912289792 | 45.062 |

`kCpuRenderBytesPerPixel` is 46, one byte above the higher ratio. `46 × 155523200 = 7154067200`. `kCpuRenderSetupBytes` is 530382848, the private peak of a 1600×900 group-1 view of Single_01 at tile 800,400. The full-map run's private peak at 0.729 s, before that image grew, was 529973248. Three other 1600×900 views peaked between 529661952 and 530006016. None of those region views raised a high water above the scene load by a separate full-image amount. The reservation is still setup plus 46 bytes for every pixel of the rectangle that is drawn.

A Single_01 group-2 reservation is `530382848 + 7154067200 = 7684450048` bytes. Two of those plus the 4294967296-byte headroom are 19663867392 bytes, which is above the 16507674624 bytes free at the start of the one-worker baseline. The second job waits or is refused. It is not admitted beside the first.

On this head a one-worker legacy-river run (`ANNO_TEST_THREADS=1`, 2026-10-28) was refused before the picture: estimate 7560404608, available 6967738368, pending 0, resident 0, reason `exceeds capacity`. Available physical at that decision was 11262695664 bytes. The same case on the pre-F1 head finished in 297.646 s at peak private 8044425216 (about 7.49 GiB). That comparison is a refusal against a completed run, because today's free memory was below estimate plus headroom. The combined with-game CTest on this head is in `docs/orchestration/REPORTS.md` (1061 passed, 6 failed, every failure `exceeds capacity`). Minimum available physical during that run was 5099106304; the maximum was 13402222592.

## T10c F2 — the corpus path measured, coherent admission (2026-10-28)

Head `231807807de14f4abf57d13257c9b75dd0784a61` (clean), coordinator build `bb12b56e5c8930cb`, MSVC Release, host as above (33408942080 bytes physical, commit limit 108272488448). Sampler `tools/sample_memory.ps1` at 1 Hz. `ANNO_TEST_MEASURE_ITEMS=1` samples the test process's private bytes every 5 ms and prints each item's reservation, its private bytes at admission and the peak while it ran; the highest 5 ms sample was within 20480 bytes of the process's `PeakPagefileUsage`. Logs in the worktree `cache/f2/` (not committed).

### Admission

The coordinator measures; clients only report (`src/test_support/src/coord_server.cpp`). A grant uses free memory read after the last event that lowered the pending bytes (a release, a client that left with holds, residency that became visible): each such event starts an epoch, and a reading of an older epoch does not count. The process budget is measured at decision time: `GlobalMemoryStatusEx` on Windows, `/proc/meminfo` on Linux, and from WSL both the VM and the Windows host through a PowerShell meter process. A WSL request is bounded by the VM and by the host, a Windows request by the host. Over capacity is only a request above a domain's total (physical memory and commit limit, or `MemTotal`) minus the headroom; any other shortfall waits, bounded by `ANNO_TEST_ADMIT_WAIT_S` (default 1800 s), with a progress line every 30 s. Every promise is journaled before the client hears of it; a replacement coordinator counts its predecessor's open promises as fully pending until their owners are gone.

### The corpus path, one worker

The 40 renders of `legacy river shading reproduces the previous enhanced digests` (5 scenarios × 4 renders × 2 river modes), measured first on the 46 bytes/pixel model (head `62ad87d` plus the measurement edit, 2026-10-28):

| item | pixels | reserved (old model) | growth from admission |
|---|---:|---:|---:|
| Single_01 g2 | 155523200 | 7684450048 | 4475035648 |
| Single_10 g2 | 159244640 | 7855636288 | 3842318336 |
| Single_14 g2 | 130817280 | 6547977728 | 3097452544 |
| Single_06 g2 | 32403280 | 2020933728 | 818515968 |
| Single_24 g2 | 20239200 | 1461386048 | 508313600 |
| any g1 region | 1440000 | 596622848 | at most 108085248 |

The catalog (Felder, Anlagen, figure header and volumes) the first item loads stays at about 743718912 bytes for the rest of the call. Above the item's own scene, Single_01 is (4475035648 − 108085248) / 155523200 = 28.08 bytes per pixel; the fresh-viewer figure of 46 overstates this path by 1.6×. That run spent 387 of 662 s waiting for admission at about 12 GB free. The corpus test now reserves 256 MiB per item (2.4× the 108085248 maximum), 36 bytes per pixel (1.28× the 28.08 maximum) and 1 GiB more while the catalog is not loaded (1.44× its 743718912). The headroom stays 4096 MiB. Single_01 group 2 is now 5867270656 bytes (with the catalog 6941012480).

Checked on the committed head, one worker, both heavy cases:

| case | items | worst growth / reservation | largest non-picture growth | refusals | process peak private |
|---|---:|---:|---:|---:|---:|
| legacy river (47 assertions) | 40 | 0.763 (Single_01 g2) | 108085248 | 0 | 5226291200 |
| scene digests (45 assertions) | 44 | 0.763 (Single_01 g2, 28.08 bytes/pixel) | 120647680 | 0 | 5223342080 |

### Legacy river: one worker against budgeted overlap

Same head, same 47 assertions, both runs with `ANNO_TEST_MEASURE_ITEMS=1`, free memory 17–19 GB at the start:

| run | elapsed s | admissions | admitted while another was open | most open | refusals | peak private | min available physical |
|---|---:|---:|---:|---:|---:|---:|---:|
| `ANNO_TEST_THREADS=1` | 266.369 | 44 | 0 | 1 | 0 | 5226291200 | 13854380032 |
| `ANNO_TEST_THREADS=4` | 186.351 | 44 | 42 | 4 | 0 | 10401538048 | 9155538944 |

Budgeted overlap is 30.0 % less wall time than one worker. The largest pending sum at an admission in the overlapping run was 12322760448 bytes (other items' unallocated reservations), and available physical stayed 4.86 GB above the headroom. For the record, the pre-F1 one-worker baseline (P1 head `416993fa`, old buffers) was 297.646 s at peak private 8044425216; F1's four-worker 246.1 s was on the 46-byte model with different free memory and is not this head.

### Memory used outside the budget

During the first one-worker run on the committed head an owner application held 11.46 GiB private and free memory fell to about 4.3 GB. The first item (6843933440 bytes) waited 1212 s with `pending=0 resident=0 reason=waiting` and a line every 30 s, was not admitted on any older reading and not refused, and was admitted once free memory returned (15851536384). Its items then took 267.9 s. That run's 1479.8 s elapsed is that wait, not the render cost; the timing pair above was taken afterwards.

## T10c F3 — compatibility version and no inherited descriptors (2026-10-28)

**Compatibility, not build id.** A test coordinator and its clients share one host budget when they speak the same compatibility version, `wire::kCompatVersion` (`src/test_support/src/coord_wire.hpp`), whatever their build ids. The build id (a hash of the coordinator's sources, `ANNO_COORD_BUILD`) is provenance: every gate logs it with the compatibility version and the incarnation (`[anno-test memory] coordinator build=… compat=… incarnation=…`).

**When the version is bumped.** Bump `kCompatVersion`, and say why here, for any change of the wire format or of the reservation, journal, residency or recovery semantics that an older peer would not reproduce. Do not bump it for provenance, diagnostics, test hooks or code that changes no semantics. The value is 3 since F4: a successor issues no hold id that appears in the journal it replayed, and an older successor can reuse one and lose a promise across two restarts. A test pins it (`F3 (D1, pin)`).

**What a mismatch does.**
- Same version, another build: served; both reservations count against the one budget (`F3 (D1): two compatible builds share one busy coordinator`).
- Another version, or a gate that asks for the exact build (`ANNO_TEST_BUDGET_EXACT_BUILD=1`): refused while the coordinator is busy. The requester waits, retrying with backoff, bounded by `ANNO_TEST_ADMIT_WAIT_S`; it ends with `wait deadline` and an infrastructure diagnostic (both builds, both versions, "busy"), or with `cancelled`. It never stops the busy peer and never opens a second budget.
- A coordinator that is idle when refused steps aside. The requester's next attempt finds nothing listening and starts its own coordinator on the same port: the handover. A promise journaled by the predecessor and still open (its owner alive) is counted by the successor as an orphan until the owner exits (`F3 (D1, crash handover)`).

**Test-only hooks.** `ANNO_TEST_BUDGET_CLIENT_BUILD` (the build id a client sends), `ANNO_TEST_BUDGET_CLIENT_COMPAT` (the version a client sends), `ANNO_TEST_BUDGET_EXACT_BUILD=1` (the exact-build opt-in), `ANNO_TEST_BUDGET_JOURNAL` (the journal a coordinator this process starts uses), `anno_budget_coord --compat-version N`, `ANNO_TEST_COORD_LEAK_FD=<path>` (POSIX coordinator: at start, after it closes descriptors 3..255, opens the path without close-on-exec and keeps it open; case `F4 (N4)`), `ANNO_TEST_PROBE_LEAK_FD=<path>` (POSIX `anno_budget_probe`: opens the path without close-on-exec and moves it to descriptor 256 or above, so the coordinator's start-up close does not hide the client spawn's closefrom; case `F4 (N4b)`). Production runs set none of them.

**No inherited descriptors (D2).** Linux: every coordinator socket is close-on-exec (`SOCK_CLOEXEC`, `accept4(…, SOCK_CLOEXEC)`), every pipe too, and every helper spawn closes everything above stdio (`posix_spawn_file_actions_addclosefrom_np` on glibc 2.34 and later, one `addclose` per descriptor elsewhere). Windows: the listener and client sockets are created `WSA_FLAG_NO_HANDLE_INHERIT`, accepted sockets have the inherit flag cleared, and the process creations use `bInheritHandles = FALSE`. Evidence in `cache\f3\exp`: Winsock sockets are inheritable by default; `addclosefrom_np` closes an inherited socket; `addclose` of an unopened descriptor succeeds.

**Silent is not foreign.** A connection that is accepted and answers no hello within `ANNO_TEST_BUDGET_LIVENESS_MS` is "not ready" and is retried within the admission bound. Only a reply that is not a coordinator's frame is "speaks another protocol".

### F3 timing (not a speed claim)

No render or simulation code changed in F3, so no speed-up is claimed. The legacy-river digest case (legacy river shading*, 4 workers, cache\f2\run_legacy.ps1) was timed twice on the F3 head to check that the coordinator change did not slow the test path:

- run A: 216.9 s, with three "still waiting" lines (about 90 s of budget wait; 16.7 GB free at the start, about 3.4 GB available against a 6 GB request while owner applications ran). Not comparable.
- run B: 205.4 s, no budget wait.
- The F2 baseline of 186.4 s was not reproduced on this host state. The interleaved A/B of T10c F4 (below) measured the F2 code and the F4 head side by side on one host state.

Both runs pass the case (47 assertions).

## T10c F4 — unique hold ids, cancel of every waiter (2026-10-28)

**E1, hold ids.** A successor issues no hold id that appears in any `A` or `R` line of the journal it replayed. One server-wide `u64` counter is seeded at `max_hold_id + 1` (`coord_server.cpp`). Uniqueness is required per budget; a single counter needs no per-budget map, and a `u64` will not overflow in a test run. The journal line format is unchanged.

**Compatibility version 3.** `kCompatVersion` is 3 for that recovery rule: an older successor numbers a budget's holds from 1 and can reuse an id, losing a journaled promise across two restarts. Stale F2/F3 binaries are refused by version.

**E2, cancel bound.** `Budget::session_for` reads `cancel_gen` before taking `connect_mu`. After the lock, if there is still no session and the generation changed, it returns `cancelled`. The wait ends within one connection attempt (at most `ANNO_TEST_BUDGET_LIVENESS_MS`) plus one pause (at most `ANNO_TEST_BUDGET_RECHECK_MS`). A `cancel()` issued before a thread enters `session_for` does not end that later wait (same rule as F3).

**E4, interleaved A/B** of the 4-worker legacy-river digest case (`cache\f4\ab\run_legacy_ab.ps1`, ports 47851 / 47852). A is F2 `43cd10f` (scratch `cache\f4\ab\f2-src`, coordinator `a969992d481424ed` protocol=2). B is this F4 tree (coordinator `cee55b5e74257911` compat=3).

| run | elapsed_s | exit | cases | assertions | still-waiting lines | wait notes | counted | free_kb at start | coordinator |
|---|---|---|---|---|---|---|---|---|---|
| A1 | 223.734 | 0 | 1 / 1 | 47 / 47 | 3 | 3 × "for 30 s" | no | 16895944 | build=a969992d481424ed protocol=2 |
| B1 | 205.784 | 0 | 1 / 1 | 47 / 47 | 3 | 3 × "for 30 s" | no | 17098608 | build=cee55b5e74257911 compat=3 |
| A2 | 197.070 | 0 | 1 / 1 | 47 / 47 | 0 | none | yes | 17241036 | build=a969992d481424ed protocol=2 |
| B2 | 204.759 | 0 | 1 / 1 | 47 / 47 | 0 | none | yes | 17260440 | build=cee55b5e74257911 compat=3 |
| A3 | 193.437 | 0 | 1 / 1 | 47 / 47 | 0 | none | no | 16939468 | build=a969992d481424ed protocol=2 |
| B3 | 199.742 | 0 | 1 / 1 | 47 / 47 | 2 | 2 × "for 30 s" | no | 16973616 | build=cee55b5e74257911 compat=3 |

B/A to three decimals: A1/B1 205.784 / 223.734 = 0.920; A2/B2 204.759 / 197.070 = 1.039; A3/B3 199.742 / 193.437 = 1.033. A1/B1 does not count (three `still waiting` lines in each run). A2/B2 counts (no wait, no other test). A3/B3 does not count (B3 logged two `still waiting` lines). Rule (one-sided): the F4 head may be at most 5 % slower than the F2 code in every counted pair, and a pair with a budget wait does not count, because its time measures the host's free memory. A3/B3 waited, so the verdict rests on A2/B2 alone. **No throughput regression measured:** at least one pair counts and every counted pair has B/A ≤ 1.050 (A2/B2 = 1.039). Logs in `cache\f4\ab\legacy-*`.

## MAP-2 repair — owners on macOS, detached coordinator streams (2026-10-31)

Context: the first macOS support commit (`8002ae8`) made the coordinator start and measure memory on a Mac, but its process identity was Linux's. A review reproduced three defects; the first two are in the coordinator and are described here. (They are not "test harness only": the first changes what a production coordinator admits.)

**Ownership of a journaled promise (`coord_wire.hpp`, `coord_server.cpp`).** A promise in the journal names its owner as `{namespace, pid, start stamp}`. On macOS the namespace was 2 (Linux), the stamp came from `/proc` and was 0, and `owner_alive` looked for `/proc/<pid>/stat`, which does not exist: a live owner counted as stopped. Observed: capacity 100, client A holds 50, the coordinator restarts on the same journal, client B asks for 80 and is admitted with `pending=0`. Now:
- **Namespace 3 is Darwin** (`kNamespaceDarwin`); 1 is Windows, 2 is Linux. The stamp is the `proc_pidinfo` start time in microseconds, which with the pid names one process (a reused pid has another stamp). `ESRCH` and a zombie are stopped; any other error is unknown. This is the reviewed MAC-1 source (`4cc0a3a`, `870a527`, `837fb5f`), with namespace 3 added.
- **Fail closed.** An owner whose stamp is 0 on either side, whose namespace this host cannot judge (Windows on a Mac, anything unknown), or whose liveness cannot be established is *unknown* and its promise is kept. A missing `/proc` is never evidence of death.
- **Older records.** (a) `8002ae8` wrote namespace 2 with stamp 0: not reinterpreted, kept (it can only be removed by deleting the journal once its owners are known to be gone). (b) The MAC-1 coordinators (`4cc0a3a`) wrote namespace 2 with a microsecond Darwin stamp. On a Mac such a record (namespace 2, stamp ≥ 10^15 µs, i.e. after 2001-09-09, where a Linux `starttime` is about 10^7 clock ticks a day) is judged by Darwin; this keeps the journal under `~/.cache/anno1503`, which every executor on the host shares, from accumulating promises no coordinator will ever release. Any other namespace-2 record on a Mac is not touched.
- **`kCompatVersion` stays 3.** Nothing changes for Linux or Windows peers (a coordinator that meets namespace 3 does not know it and keeps the orphan, which is the safe direction), and the macOS coordinators that existed before could not recover live owners at all, so there is no working older behaviour to be compatible with. A bump would also have refused every client of a coordinator that another executor on the same Mac may be running on the shared port. The pin test `F3 (D1, pin)` is unchanged.
- CPU tokens are journaled with the same owner triple (`C`/`D` lines) and go through the same `owner_alive`; case O4 covers them.

**Detached coordinator streams (`coord.cpp`).** `spawn_local` kept the starter's stdin, stdout and stderr. The coordinator outlives the client that starts it (it exits after 30 s idle), so whatever read the client's output waited for end-of-file until then: measured 32.0 s for a client that finished in 2 s; the coordinator's `LISTENING` line appeared in the client's output. The detached coordinator now gets `/dev/null` for 0, 1 and 2 (every file action is checked; a failed one means no start), on every POSIX host; descriptors above 2 are closed as before and Windows is unchanged (`DETACHED_PROCESS`, no handle inheritance). A coordinator that a test starts itself uses `Child::start` with its own pipes and keeps its output (case S2). After: the same pipeline takes 2.0 s; a piped `ctest` of a 19.1 s test takes 19.2 s.

**Native harness on macOS.** `sibling()` and `module_directory()` use `proc_pidpath`; cases that need WSL or `/proc` (`wsl_available()` is false on Darwin) skip with their own message and are listed as capability skips, not passes. One case (`two file clients cannot reserve the same free bytes`) depended on a coordinator being warm because `try_reserve` makes a single attempt; it now brings the coordinator up explicitly.

**Regressions** (`tests/budget_f5_test.cpp`, ports 47850–47858, journals under `temp_directory_path()/anno1503-f5`, bounded waits, every child owned by a `Child`, cleanup of a started coordinator by the step-aside handshake on its port, never by name): O1 live holder through a restart and release on exit, O2 reused pid, O3 unattributable records, O4 CPU tokens, O5 Darwin namespace and the earlier stamp format, S1 EOF while the coordinator lives, S2 foreground output, S3 the captured pipeline. With the old Darwin behaviour restored (every owner stopped) O1, O4 and O5 fail.

## MAP-2 repair 2 — compatibility 4, the verification gate (2026-10-31)

Amendment to the section above: its line "`kCompatVersion` stays 3" was wrong for the shared coordinator and is superseded here. The earlier text is kept as history.

**Defect.** A compatibility-3 coordinator (W4/MAC-1, source `c869fb7`, `anno_budget_coord` sha256 `92c30840…6262f4`, built from the `abc1f8c` test_support tree) accepts a namespace-3 client under compatibility 3, replays the record `A <budget> 1 50 3 <pid> <µs stamp> 1` as an owner it cannot judge, and keeps it forever. Reproduced with real binaries, isolated ports and journals: holder killed, 24 tries for 80 all `claim=0 pending=50` (`cache/map2-repair2-evidence/06-mixed-old-successor.txt`; the same run with the pre-change MAP-2 successor admits on try 1, `06-mixed-map2-successor-control.txt`). The older coordinator also *rewrites* what it replays and drops every line it does not recognise, so a marker or new record type in a shared journal cannot protect it.

**Policy (compat 4).**
1. `kCompatVersion` is 4. A compat-3 coordinator and a compat-4 client refuse each other by the existing handshake: an idle coordinator steps aside; a busy one is waited for, bounded by `ANNO_TEST_ADMIT_WAIT_S`, and the client ends with `infrastructure blocker: … compatibility version 3 … compatibility version 4 … busy with N client(s) … incompatible compatibility version`. The busy coordinator's own holder is untouched (real binaries, `10-mixed-rejected.txt` (b)).
2. The default journal of version 4 is `coord-<port>-v4.journal`; versions 3 and below keep `coord-<port>.journal`. An older coordinator never opens, replays or rewrites a file that holds namespace-3 owners. Verified: an older coordinator started on the same HOME leaves the v4 journal byte-identical (`10-mixed-rejected.txt` (d)).
3. The first start of a version-4 default journal (it does not exist yet) adopts the open promises of the unsuffixed journal, read-only. A live owner of the previous coordinator keeps its reservation; a dead one, a reused pid, is released by the usual rules; a zero-stamp or foreign-OS record is kept. Later starts do not adopt again. An explicit `--journal` or `ANNO_TEST_BUDGET_JOURNAL` names the file and adopts nothing; a version-4 coordinator reading an older coordinator's explicit journal recovers it (real binaries, `09-mixed-old-to-new.txt`: `claim=1` on try 1).

**Kept:** live-owner reservations, conservative treatment of unattributable/zero-stamp records, recovery of attributable legacy Darwin records, bounded waits and diagnostics, one shared admission budget (no second budget, no bypass, no coordinator stopped but an idle one that is asked to step aside by the existing handshake).

**Rollout limits (not universal interoperability).**
- Do not point an older coordinator at a version-4 journal with `--journal`: it keeps namespace-3 owners forever. The default names prevent it; nothing prevents an explicit path.
- If a version-4 coordinator stops while a version-4 holder lives and an older coordinator then starts, the older one does not know that holder's reservation (it reads another file; `orphans=0`). Its own clients are not protected from that holder until a version-4 coordinator runs again. Version-4 clients are refused by it, so the exposure is the older clients only.
- An older coordinator that is idle when a version-4 client arrives steps aside; one that is busy must finish. On the shared port 47631 the W4 executor's compat-3 clients meet a version-4 coordinator the same way (refused, or it steps aside when idle). Merging this to a tree that also contains compat-3 clients means one of the two waits for the other's idle exit (30 s) or its work.
- Adoption removes nothing from the older journal, so records released after adoption stay in it; an older coordinator started later judges them by its own rules (dead owners are released by it too).

**Tests** (`tests/budget_f5_test.cpp`, ports 47860–47867): T1 modeled compat-3 coordinator vs compat-4 client, idle and busy; T2 journal names; T3 adoption of a live memory holder, its exit, no second adoption, legacy file byte-identical; T4 CPU token, reused pid, unattributable record; T5 a real older binary steps aside (opt-in `ANNO_TEST_OLD_COORD=<path>`; skipped, and reported as an opt-in skip, otherwise). T1–T4 use this build started with `--compat-version 3` as the "older coordinator" (handshake and journal name modeled, not the older recovery code); the mixed-binary evidence is the real W4 binary through `tools/test/mixed_coordinator_check.sh` and T5. Mutation checks (`08-mutations.txt`): without the version in the journal name T2, T3, T4 fail; without adoption T3, T4 fail; with compat 3 T1–T4 and the F3 pin fail.

**Verification gate.** `tools/test/run_selected.sh <build> <target|-> <regex> <timeout> <log> [jobs]` (adapted from the W4 runner, `3d2fb9d`) builds, runs `test_names_are_selectable` as a mandatory preflight, runs the selection, and `tools/test/ctest_verdict.py` reads the log per test. It fails (exit) on: a failed build (2), a failed preflight (5), a failed test (1), zero selected tests (8), a sanitizer report (4), a test that needs the original files and skipped for want of `ANNO1503_GAME_DIR` (6), a doctest child that executed zero cases (7). A doctest "N skipped" is the filter excluding the other cases of the binary and is never counted; a printed "skipping" is an early-return skip and is listed as required-asset, platform (WSL, Windows-only) or opt-in (an `ANNO_…=1` switch) and does not count as executed. Bounded negative checks (`03-negative-checks.txt`): unknown target 2; nothing selected 8 (the first version of the wrapper counted its preflight and passed an empty selection; fixed); a `;` in a name 5; game dir unset 6; synthetic logs with an empty child and a sanitizer line fail. The earlier name defect: the O1 name held a `;`, CTest registered it as two tests that ran zero cases each and passed (`01-o1-names.txt`, `01-o1-zero-case.txt`); selected whole, the body runs (`01-o1-real-body.txt`: 1 case, 25 assertions).
