# ZOOM-SMOOTH-1 and 2 packets: an independent critical review, and what changed (2026-11-14)

**Asked for by the owner** (2026-11-14). The owner wanted a critical review of the zoom assessment and plan, and of
whether it follows the project's guidelines: modern, robust, flexible, performant and scalable.

**Who reviewed:** a reviewer subagent with no part in writing the packets. It read the packets, CLAUDE.md, PLAN §0,
`zoom-smoothness.md`, ADR 0007, the zoom-session findings, the ZOOM-MEASURE-1 review and the code at `main` `f54e36f0`,
plus the SDL 3.4 and wgpu-core sources online. It was read-only and ran nothing.

**Its verdicts:** ZOOM-SMOOTH-1 needs rework; ZOOM-SMOOTH-2 needs rework and should be split.

**The orchestrator checked** the blocking findings in the code before changing anything.

## Blocking findings, checked by the orchestrator

| # | Finding | Checked in the code |
|---|---|---|
| B1 | The proofs couldn't see the change: the sweep never sets `zoom_anchor`, `prewarm_far_levels` or `resident_level_chunks`, and uses a different worker count; the live script zooms at the window centre only | **confirmed:** `bench_frame/main.cpp` sets none of the three and uses `(threads + 1) / 2` workers (lines 1288, 1447) against the viewer's `hw * 3 / 4` (`viewer.cpp:1006`); `frames_.zoom_anchor` is set from the cursor in the viewer (`viewer.cpp:2597`) only; the dev script's `wheel` is centred |
| B2 | Chunks prefetched on the CPU still miss the GPU: the compositor uploads ahead only when no visible chunk waits and nothing else was uploaded, only from margin and adjacent keys, at most 96 | **confirmed:** `gpu.cpp:2186` (`!waiting && uploads == 0`), `prefetch_chunks = 96` in `GpuLimits::interactive()` |
| B3 | Moving GPU releases to the existing reaper would cause priority inversion, and wgpu may destroy recently used resources inside `queue submit` anyway; the target "no slow frame whose largest step is drops" could be gamed | **confirmed:** the reaper lowers its priority (`frame.cpp` `ChunkWorkers::reap`); `gpu.cpp`'s warm-plane note records the inversion; the wgpu-core point is cited from its source and is to be checked against the pinned version |

## Should-fix findings, taken over

- **S1, the wanted-set order and geometry:**
  - pan must not starve under `kPlanKeys = 512`;
  - order by estimated time until visible;
  - the zoom-out area is the view scaled about the anchor (`zoomed_view_at`), not a rectangle centred on the cursor;
  - no literal 2.2.
- **S2, the budget:**
  - one byte budget for every limit (`spare_chunks`, `kPoolPlanes`, `idle_plane_budget_mb`, `prefetch_chunks`);
  - a chunk is held on both the CPU and the GPU;
  - defaults by adapter type, as `atlas_budget_mb` already does, since WebGPU reports no memory size;
  - wgpu's memory thresholds as a safety net;
  - eviction rate-limited per frame.
- **S3:** fill the resident levels with idle workers, report "time to resident", and measure stale rebuilds under
  edits.
- **S4:** the planning cost grows with the wanted set, so report the `chunks` step's p99 and max.
- **S5:** known failures stay failures. The opt-in CTest registers the passing `--control` run; the real profiles stay
  report-only.
- **S6, S7:** split the `chunks` timer before budgeting it; `record` and `finish` probably grow with draws × passes.
- **S8, the SDL3 live-resize mechanism:**
  - a 10 ms timer sends `EXPOSED` with `data1 = 1`, so a drag can reach about 64–100 Hz, not 500 Hz;
  - the watch runs under SDL's watch-list lock, which the sim's wake push needs;
  - it needs a reentrancy guard and the main loop's order;
  - move and resize are different cases (OQ-098, OQ-099);
  - a headless test via `SDL_PushEvent` is possible;
  - Linux has no modal loop.
- **S9:** sprite decoding on the window thread was silently dropped.

## What changed

- **[ZOOM-SMOOTH-1](../tasks/zoom-smooth-1.md), rewritten:**
  - the sweep fixed first, in its own commit, with "before" numbers rerun;
  - one byte budget;
  - resident coarse levels on the CPU and the GPU;
  - the zoom-out ring about the anchor;
  - the GPU upload share kept even while a chunk is waiting;
  - rate-limited eviction;
  - all four counters;
  - an off-centre live run (`wheel N [X Y]`).

  The finer-level prefetch (zoom-in) is deferred to RENDER-DIRECT, which comes right after; it was the most
  expensive part and the first thrown away.
- **[ZOOM-SMOOTH-2](../tasks/zoom-smooth-2.md), narrowed** to the window drag and resize, with the SDL3 mechanism, its
  pitfalls, move and resize as separate cases, and a target of about 16 ms.
- **[ZOOM-SMOOTH-3](../tasks/zoom-smooth-3.md), new:**
  - the window-thread spikes, measured again after ZOOM-SMOOTH-1;
  - timers split first;
  - releases on the window thread under a budget or on a normal-priority thread;
  - sprite decoding off the window thread;
  - judged by total busy time;
  - a QUESTION if the fix is RENDER-DIRECT's.
- **The owner's order holds:** ZOOM-SMOOTH-1 → 2 → 3 → RENDER-DIRECT. The owner's OBS recording check moves to after
  ZOOM-SMOOTH-3, when all three are on `main`.

## Not verified

The reviewer's runtime estimates were not checked:
- `lan5` group 2 at about 3 GB per side;
- the 4K ring size;
- how much of the far-zoom counts are open-sea chunks;
- the Windows timer's real rate during a drag.

The packets make the executor measure each one.
