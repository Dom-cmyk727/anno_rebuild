# ZOOM-SMOOTH-1: the zoom finds its chunks ready: coarse levels resident, zoom-range prefetch, a memory-sized cache

**Status:** written 2026-11-14 by the Claude Code orchestrator for **the Windows executor**, on the owner's go ("Yes,
write it") after the owner's zoom session. It is released under the standing go after SHORE-2's review. It is step 2 of
[`zoom-smoothness.md`](../../render/zoom-smoothness.md) §6: quick wins in today's chunk renderer, before
[ADR 0007](../../adr/0007-direct-rendering.md)'s direct renderer (RENDER-DIRECT-1…6) replaces it.

**Base:** `origin/main` at release. Branch `task/ZOOM-SMOOTH-1`, a new worktree. About 3–4 hours. Never rebase,
force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern: readiness while zooming;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

The owner's requirement (2026-11-13): no rendering of any sort may appear while zooming, and islands are drawn before
they come into view. The owner watched the scripted session of 2026-11-14 and "could really distinctly see the delayed
rendering and artifacts". Measured on `main` at 2560 × 1369 on the RTX 4090 with enhanced water
([`zoom-session-2026-11-14.md`](../../render/zoom-session-2026-11-14.md), F2–F6; the owner's own recording agrees):
- **zooming out fast:** up to 58 chunks missing at once. The islands the zoom-out reveals arrive as a sliver, then cut
  by a straight vertical line, then translucent, over about 0.4 s;
- **zooming in fast:** up to 77 missing at once;
- **zooming slowly (8 notches/s):** up to 29 missing on the first visit to a level.

ZOOM-MEASURE-1's sweep counts the same thing (its review's baseline: on the `lan5` wheel and zoompan profiles, 593–881
non-ready frames and up to 45,485 stand-in draws).

**The causes this packet removes (§3 RC1):**
- the prefetch covers a pan and one level step, not the area a zoom-out opens within a level;
- only 48 spare chunks are kept;
- coarse levels stay resident only up to 16 chunks (`FrameRenderer::resident_level_chunks`);
- they are built in the background only for the ship-start opening (`prewarm_far_levels`, `viewer.cpp`).

**What this packet cannot fix (§4 B):** a camera jump into an unbuilt area (F1), and a level-0 view anywhere on a large
map. ADR 0007 fixes both. This packet is judged by how much it cuts against the baseline, not by reaching 0
everywhere. The owner's acceptance of the zoom, with the counters at 0, comes with RENDER-DIRECT.

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/render/zoom-smoothness.md` §3, §4 (B), §6;
- `docs/render/zoom-session-2026-11-14.md`;
- the ZOOM-MEASURE-1 review, "Notes for ZOOM-SMOOTH-1";
- ADR 0007, to see what not to build twice;
- `docs/render/gpu-chunks.md` (the cache and the memory figures);
- `docs/perf/README.md` (the zoom sweep);
- `src/render/include/anno/render/frame.hpp`:
  - `spare_chunks`, `resident_level_chunks`, `prewarm_far_levels`, `max_lookahead_chunks`, `lookahead_rect`;
  - `update_chunks` and its eviction;
- the GPU compositor's upload budget (`GpuLimits::interactive()`);
- `src/app/src/viewer.cpp` around `frames_.prewarm_far_levels` and `kOpeningResidentLevelChunks`.

## Do

1. **Coarse levels resident in every session.**
   - Build the levels whose whole scene fits a residency budget in the background at load, below normal priority,
     as `prewarm_far_levels` already does for the opening. Do this for `--play` and `--view` too, and never evict
     them.
   - The budget replaces the fixed 16 chunks:
     - one memory figure in MiB, from a new viewer option and its `FrameRenderer` field;
     - its default on a discrete GPU large enough that `lan5` keeps every level from the whole map down to group 2
       resident, if it fits;
     - its default on the integrated GPU stated and measured.
   - **No hardcoded limit** (CLAUDE.md): the original 16 stays reachable as a value.
   - A stale resident chunk (an edit) is rebuilt as today.
2. **Prefetch the zoom range, not only the pan ring.** Each frame, the wanted set also includes:
   - **at the current level:** the area a zoom-out to the next coarser level would show, which is the composed grid at
     its largest (up to about 2.2 × the window per axis), around the zoom anchor (the cursor);
   - **at the next finer level:** the chunks under the current view around the cursor, which a zoom-in shows next.

   Order the builds by their likely use: visible first, then the zoom-out ring at the current level, then the finer
   level under the cursor, then the pan look-ahead. State the order and why.
3. **A cache sized to memory, not to 48 chunks.** `spare_chunks` becomes part of the same memory budget, so returning
   to a level you just left finds it built.
   - Report the peak and mean chunk memory on the 4090 and on the iGPU for the sweep.
   - Report the time to first frame and the background build time with and without the change. Load must not get
     visibly slower: the window thread never waits.
4. **Uploads.** Prefetched chunks go to the GPU under the existing per-frame upload budget, nearest-to-use first. Don't
   raise the budget unless the sweep shows uploads are the bottleneck; if you do, state the measurement.
5. **Register `tools/test/zoom_sweep_contract.py` in CTest as an opt-in test** (ZOOM-MEASURE-1 review, note 1). It is
   skipped with exit 77 unless an environment variable asks for it, so it never turns `gate` red. Name the variable in
   `docs/perf/README.md`.
6. **Tests:**
   - unit tests of the wanted set for a given view, zoom and anchor (which keys, in which order);
   - a test that a resident level is never evicted under memory pressure while a visible chunk still is;
   - a test of the budget: bytes, not chunk counts.

   The CPU reference path and its digests are unchanged. This packet changes which chunks are built and kept, never
   what a chunk looks like.

**Not this packet:**
- stand-ins, dissolves and the open-sea placeholder: they stay as they are, and RENDER-DIRECT removes them;
- camera jumps;
- the window-thread spikes at maximum zoom-out (F7: recording commands 10–14 ms, `finish` 9 ms, `sync drops` 15 ms);
- the SDL live-resize hitch;
- the water.

The spikes and the live-resize hitch are **ZOOM-SMOOTH-2**, queued.

**Stop with a QUESTION if:**
- the residency budget at its default would take more than about a quarter of the 4090's memory or half of the iGPU's
  shared budget;
- the wanted set needs a change to what a chunk contains;
- a fix would touch `src/sim`.

## Proof

All numbers on this PC, with an otherwise idle machine. Check load before each run, and run one at a time.

1. **The sweep, before and after,** on `main` at your base and on your head:
   - `bench_frame --zoom-sweep` (ZOOM-MEASURE-1's profiles) on `lan5` at 2560 × 1369 on the RTX 4090, with enhanced
     water: legs, wheel, zoompan and pan;
   - the same wheel profile at 1280 × 800 on the iGPU (adapter 1);
   - one row per profile: frames, non-ready frames, most non-ready at once, stand-in draws, dissolves, frames above
     2 ms, busy p99 and max, and peak chunk memory.
2. **The owner-like live session, before and after.** This is the same session the owner watched.
   - **The files:** `E:\anno1503-worktrees\zoom-session-2026-11-14\zoom.script` and
     `tools\drive2.py ... --no-capture`. They are outside git, and you may copy them.
   - **The run:** the real window, maximised, `--play data/maps/lan5.txt --water enhanced`, five map sections.
   - **The table:** per move (jump, osc, fullout, fullin, flick, human, panzoom), using `tools\analyze2.py` on each
     run directory: non-ready frames, most at once, dissolves, and worst busy.
   - The 2026-11-14 run 3 is the "before" if your base is still `f54e36f0`'s renderer.
3. **The target:**
   - after the coarse levels are built (state how long that takes), the **osc, fullout, fullin and human moves show
     0 non-ready frames and 0 dissolves** on `lan5`;
   - jumps are expected to stay;
   - if a move doesn't reach 0, list what was still missing (level, key, why it wasn't wanted or built yet). Don't
     relax a counter or a threshold.
4. **The gates:** the changed render and app lanes, and `zoom_sweep_contract` opt-in, through the canonical runner, with
   classified counts.
   - A test refused for memory did not run: rerun it alone with `--jobs 1`, with its own log.
   - Run the WSL warning check (`tools/check_unix_warnings.sh`).
   - Hosted CI must be green on the head, `gate` included.

**Not yours:** the full suite and the game-file run (the orchestrator's, at the landing).

## Report

READY FOR REVIEW, about 60 lines:
- the budget and its defaults;
- the wanted-set order;
- the before/after sweep table and live-session table;
- what still misses, and why;
- the memory and load-time figures;
- the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
