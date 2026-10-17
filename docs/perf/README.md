# Performance measurements

Goal and targets: [`PLAN.md`](../PLAN.md) §0.3. Every benchmark runs the production code path headless and reports percentiles; results below are kept per date and machine so regressions are visible.

## View frames: `bench_frame`

```sh
cargo run --release -p anno-render --features gpu --example bench_frame -- <game_dir> <file.sze|.scp> \
    [--threads n] [--res 1920x1080,2560x1440,3840x2160] [--groups 0,1,2] \
    [--styles original,enhanced] [--chunks blocking,async] [--renderer cpu,gpu] \
    [--check] [--at dx,dy] [--quick]
```
The preview package contains it as `bench_frame` with `run-benchmark.bat` / `run-benchmark.sh` (writes `benchmark-results.md`).

**What it runs:** the functions the viewer calls every frame, without a window: `FrameRenderer::render` into a `0RGB` buffer (`cpu`), or `GpuFrame::render` into an offscreen texture followed by waiting for the GPU (`gpu`). `--check` compares the first static frame of each configuration between GPU and CPU (pixels differing by more than 2 per channel, maximum difference); `BENCH_DUMP=<dir>` writes both images as raw RGBA. `--at dx,dy` moves the camera from the largest island's centre.

**Camera paths:** per resolution, zoom group, water style and chunk mode, from a cold cache centred on the scenario's largest island:

| path | frames | movement |
|---|---|---|
| cold | 1 | first frame, nothing cached |
| static | 120 | none (animation only) |
| pan slow | 240 | 6 × 3 px per frame |
| pan fast | 120 | 48 × 24 px per frame (new chunks keep appearing) |

The animation clock advances 16 ms per frame. `--quick` divides the frame counts by 4.

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
