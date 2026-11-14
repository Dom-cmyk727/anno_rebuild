# ZOOM-SMOOTH-1: a zoom-out finds its chunks ready, on the CPU and on the GPU, under one memory budget

**Status:**
- **Written** 2026-11-14 by the Claude Code orchestrator for **the Windows executor**, on the owner's go ("Yes, write
  it").
- **Revised the same day** after an independent critical review. Its blocking findings were verified in the code
  (below).
- **Released** under the standing go after SHORE-2's review.

It is step 2 of [`zoom-smoothness.md`](../../render/zoom-smoothness.md) §6: a quick win in today's chunk renderer before
[ADR 0007](../../adr/0007-direct-rendering.md)'s direct renderer (RENDER-DIRECT-1…6, next in the queue) replaces it.

**Base:** `origin/main` at release. Branch `task/ZOOM-SMOOTH-1`, a new worktree. About 4 hours. If it runs past 5 h,
stop and report what is done. Never rebase, force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern: readiness on a zoom-out and on a return to a level;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

Measured on `main` at 2560 × 1369 on the RTX 4090 with enhanced water
([`zoom-session-2026-11-14.md`](../../render/zoom-session-2026-11-14.md) F2–F6; the owner's own recording agrees):
- **A fast zoom-out:** up to 58 chunks are missing at once. The islands it reveals arrive as a sliver, then cut by a
  straight line, then translucent, over about 0.4 s.
- **A fast zoom-in:** up to 77 missing at once.

The owner watched it and "could really distinctly see the delayed rendering and artifacts".

**The causes, verified in the code at `f54e36f0`:**

1. **Coarse levels are resident only up to 16 chunks** (`FrameRenderer::resident_level_chunks`). They are built in the
   background only for the ship-start opening (`prewarm_far_levels`, `viewer.cpp`), and one at a time, only when
   nothing else is wanted.
2. **The cache keeps 48 spare chunks** (`spare_chunks`).
3. **The GPU never gets ahead during a zoom.**
   - The compositor uploads ahead only when no visible chunk is waiting and nothing else was uploaded that frame
     (`gpu.cpp:2186`).
   - It uploads only from `margin_keys` and `adjacent_keys`, capped at `GpuLimits::prefetch_chunks` (96 in the
     interactive limits).
   - So a chunk built ahead on the CPU still arrives late on the screen, as a stand-in, then a dissolve.
4. **The limits are four separate numbers:** `spare_chunks`, `kPoolPlanes = 8 * 24` (a constant), `idle_plane_budget_mb`
   and `prefetch_chunks`. There is no single memory budget, and WebGPU reports no memory size (`gpu.hpp`'s
   `atlas_budget_mb` comment).

**Not in this packet, on purpose:**
- **The finer level under the cursor (a fast zoom-in).** It is the most expensive part and the first that RENDER-DIRECT
  throws away. RENDER-DIRECT comes right after ZOOM-SMOOTH-2, so zoom-in misses are reported here but not targeted.
- **Camera jumps (F1).** Only RENDER-DIRECT fixes them.

## Read first

- `CLAUDE.md`: "No hardcoded limits", and "Use modern multi-core CPUs and GPUs" (the window thread never waits; measure
  every performance change);
- `docs/testing/evidence-contract.md` ("known failures remain failures");
- `docs/render/zoom-smoothness.md` §3, §4 B, §6;
- `docs/render/zoom-session-2026-11-14.md`;
- the ZOOM-MEASURE-1 review, notes 1 and 2;
- ADR 0007, to see what not to build twice;
- `docs/render/gpu-chunks.md`, `docs/perf/README.md`;
- `src/render/include/anno/render/frame.hpp`:
  - `spare_chunks`, `resident_level_chunks`, `prewarm_far_levels`, `zoom_anchor`, `max_lookahead_chunks`;
  - `zoomed_view_at`, `lookahead_rect`;
- `src/render/src/frame.cpp`: planning (`kPlanKeys = 512`), the prewarm, `trim`, eviction, `update_chunks`;
- `src/render/src/gpu.cpp`: `sync_chunks`, the prefetch upload (around line 2186), the plane pool, the warm planes;
- `src/render/include/anno/render/gpu.hpp`: `GpuLimits`, `atlas_budget_mb`;
- `src/tools/bench_frame/main.cpp`: `--zoom-sweep`;
- `src/app/src/viewer.cpp`: the worker count, `frames_.zoom_anchor`, the opening's settings.

## Do

1. **First, make the proofs measure what you change.** This is its own commit, before any behaviour change.
   - **One shared configuration function.** The viewer and `bench_frame` must configure `FrameRenderer` and
     `GpuLimits` through the same function, so they can't differ.
     - Today the sweep never sets `zoom_anchor`, `prewarm_far_levels` or `resident_level_chunks`.
     - The sweep uses `(threads + 1) / 2` workers, the viewer `hw * 3 / 4`.
   - **The sweep sets `zoom_anchor`** from its off-centre wheel anchor (25 % / 15 %), as the window does from the
     cursor.
   - **Rerun the "before" numbers** with the fixed sweep. The 2026-11-13 baseline is not comparable.
2. **One memory budget, in bytes.**
   - **What it covers:** every place this packet touches:
     - the CPU chunk cache (spare and resident);
     - the GPU chunk planes, the plane pool and the warm planes;
     - the GPU prefetch residency.

     It replaces `spare_chunks`, `kPoolPlanes`, `idle_plane_budget_mb` and `prefetch_chunks` as limits. Each may stay
     as a share of the budget, with its reason.
   - **A GPU chunk is held twice:** the GPU copy is dropped only when its CPU chunk is evicted (`gpu.cpp`
     `sync_chunks`). Count both sides. On an integrated GPU both sit in system RAM.
   - **Defaults by adapter type, as `atlas_budget_mb` already does** (WebGPU reports no memory size):
     - discrete: a stated figure;
     - integrated or unknown: a stated fraction of `SDL_GetSystemRAM()`;
     - a software adapter: the minimum.

     The original 16-chunk residency must stay reachable as a value. Add a viewer option for the budget.
   - **A safety net:** where wgpu-native exposes them, set its device memory thresholds (D3D12; Vulkan optionally).
     Cite the source.
   - **The byte total is kept incrementally,** not summed over the cache each frame.
3. **Resident coarse levels, on the CPU and the GPU.**
   - **Which levels:** the derived levels whose whole scene fits the budget. Measure whether group 2 at whole-map scale
     fits on `lan5`; it is estimated at about 3 GB per side, so it probably doesn't.
   - **Build and keep them in every session** (`--play`, `--view`, the opening), and never evict them.
   - **Build fast:** the prewarm fills idle workers, not one target at a time.
   - **Upload them to the GPU in the background,** within the budget.
   - **Report "time to resident"** from load.
   - **Measure stale rebuilds** when buildings change on those islands, as in multiplayer.
4. **The zoom-out ring at the current level.**
   - **Where:** the area a zoom-out to the coarse edge of the current level shows. The zoom keeps the anchor's screen
     position, so that area is the view scaled about the anchor (`zoomed_view_at`), not a rectangle centred on the
     cursor.
   - **How far:** derive the extent from the level ratios and the coarse margin, not a literal factor; the levels
     are data.
   - **Order:** by estimated time until visible, from the zoom direction and the pan velocity.
   - **No starvation:** plans are cut at `kPlanKeys = 512` and replaced every frame, so the pan look-ahead must not be
     starved. Pan must not get worse than its baseline.
5. **The GPU gets ahead too.**
   - Route the zoom-out ring and the resident levels into the compositor's upload ahead.
   - Replace the 96-chunk cap with the budget.
   - Keep a stated share of each frame's upload budget for prefetch **even while a visible chunk is waiting**. Measure
     the share; don't guess it.
6. **Eviction is rate-limited:**
   - at most a stated number of bytes per frame;
   - with high and low watermarks.

   So this packet doesn't create the burst drops that ZOOM-SMOOTH-3 would then fight.
7. **The planning cost on the window thread.** The wanted set grows, and planning, sorting, stand-in retention and
   trimming run every frame.
   - Report the `chunks` step's p99 and max before and after.
   - The `chunks` step must not get worse at the 99th percentile.
8. **The opt-in CTest** (ZOOM-MEASURE-1 review, note 1). Known failures stay failures, so:
   - register the sweep's `--control` run (everything resident), which must pass;
   - keep the real profiles report-only, never as a test that always fails.

   Name the variable that enables it in `docs/perf/README.md`. Name the adapter by name, not by index.
9. **Tests:**
   - the wanted set for a view, anchor and zoom direction: which keys, in which order;
   - a resident level is never evicted while a visible chunk is;
   - the budget counts bytes on both sides;
   - eviction respects its per-frame limit;
   - pan keys are not starved by zoom keys.

   The CPU reference path and its digests are unchanged. This packet changes which chunks are built, kept and
   uploaded, never what a chunk looks like.

**Not this packet:**
- the finer-level prefetch (zoom-in);
- stand-ins, dissolves and the open-sea placeholder, which RENDER-DIRECT removes;
- camera jumps;
- the window-thread spikes (ZOOM-SMOOTH-3);
- the window drag (ZOOM-SMOOTH-2);
- the water.

**Stop with a QUESTION if:**
- the default budget would exceed about a quarter of the 4090's memory, or half of the iGPU's share of system RAM;
- a fix needs a change to what a chunk contains;
- a fix touches `src/sim`;
- the packet passes 5 h.

## Proof

All on this PC, with an otherwise idle machine. Check load before each run, and run one at a time. Name the adapters,
the RTX 4090 and the Radeon iGPU.

1. **The sweep, before and after,** both with the fixed sweep from step 1: `lan5`, 2560 × 1369, the 4090, enhanced
   water; legs, wheel, zoompan and pan; plus wheel at 1280 × 800 on the iGPU.
   - **All four counters:** non-ready frames, stand-in draws, dissolves, open-sea draws.
   - **Plus:** most non-ready at once, frames above 2 ms, busy p99 and max, the `chunks` step's p99 and max.
   - **Memory:** CPU and GPU chunk bytes, peak and mean, from the budget's own counter.
2. **The owner-like live session, before and after:** `E:\anno1503-worktrees\zoom-session-2026-11-14\zoom.script`
   with `tools\drive2.py ... --no-capture` and `tools\analyze2.py`, outside git.
   - The script zooms only at the window centre: the dev script's `wheel N` always puts the wheel event at the centre
     (`dev_script.cpp`), and the viewer anchors the zoom at the event's position. **Add an off-centre run:**
     - give `wheel` an optional raw drawable position (`wheel N [X Y]`, like `click_px`), with a parse test;
     - run a copy of the script with its wheel steps at about 25 % / 15 % off centre.
   - Report a table per move.
3. **The target,** once the coarse levels are resident (state the time):
   - **zoom-out moves** (osc, fullout, human) and **returns to a level just left:** 0 on all four counters on `lan5`,
     at the centre and off centre;
   - **zoom-in moves:** reported, not targeted;
   - **jumps:** expected to stay;
   - **if a target isn't met,** list what was missing (level, key, why it wasn't wanted, built or uploaded). Don't
     relax a counter or threshold.
4. **The gates:** the changed render, app and tools lanes, and the new opt-in control test, through the canonical
   runner, with classified counts.
   - A test refused for memory did not run: rerun it alone with `--jobs 1`, with its own log.
   - Run the WSL warning check.
   - Hosted CI must be green on the head, `gate` included.

**Not yours:** the full suite and the game-file run (the orchestrator's, at the landing).

## Report

READY FOR REVIEW, about 60 lines:
- the sweep fix;
- the budget and its defaults;
- the resident levels and time to resident;
- the ring and its order;
- the GPU upload share;
- the before/after tables;
- what still misses, and why;
- the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
