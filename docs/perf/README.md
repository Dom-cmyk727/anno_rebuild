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

Method: `bench_frame <game_dir> 01_1024_easy_256.sze --zoom 6 --renderer gpu --chunks async --limits interactive --res 2560x1369 --styles enhanced`, window-thread CPU time per frame, five runs of each build alternated. Before = the build of `7d24231` (main); after = this branch. The machine was shared with two other builds and test runs (CPU load 11-100 % at the start of the runs), so the first four pairs are used (run 5 of both builds ran at 100 % load and is not comparable). Medians of the four runs, p50 / p95 / p99 / max in ms:

| group | before | after |
|---|---|---|
| 0 | 0.4 / 1.6 / 2.1 / 3.4 | 0.5 / 1.9 / 3.4 / 10.6 |
| 1 | 0.5 / 1.7 / 2.8 / 5.0 | 0.55 / 1.8 / 2.7 / 4.7 |
| 2 | 0.5 / 1.7 / 3.3 / 7.7 | 0.55 / 1.8 / 2.4 / 5.2 |

Run 5 (both builds at 100 % load): before 1.8 / 22.7 / 66 / 109 (group 0), after 0.8 / 6.3 / 26 / 49. The max of group 0 after (6.7-15.8 ms in three of four runs against 2.7-4.0 before, one 66 ms before) is the one-chunk upload write (the `write` column, 7.9 ms: the driver's texture write, which the before runs show in other groups as well); no new kind of slow frame appeared, and no slow frame is tied to a stand-in draw. The cost of the feature per frame is O(visible chunks not on the GPU) lookups plus one draw per source-chunk part per pass, only while chunks are missing.

What the zoom path shows (printed by `bench_frame` after the table): frames with chunks not built 53 of 240 (group 0 start), 48 (group 1), 96 (group 2); of the first frames right after a zoom step with chunks not built, 8 of 9, 7 of 8 and 16 of 16 drew stand-ins; the others had no source chunk on the GPU for the missing area (zooming out shows area nobody has seen yet). `--stand-ins off` gives the old behaviour in the same binary. PNG pairs of the first frames after zoom steps: `C:/Users/altin/Pictures/anno-review/T1/` (`first_frame_after_zoom_out_*.png`: the island is open sea before, the scaled island after; `all_before` and `all_after`: the first eight frames after zoom steps of one run each; frames that were complete in both runs differ only by the animation).
