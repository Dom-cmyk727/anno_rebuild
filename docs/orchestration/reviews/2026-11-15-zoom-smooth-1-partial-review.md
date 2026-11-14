# ZOOM-SMOOTH-1, partial submission: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-1` `e8dcb7cf` (merged with `main` `01a4d540`).
**CI:** run 43650255790, green, `gate` included.

## Verdict

**ACCEPTED as a partial (steps 1, 7 and 8).** Steps 2 to 6 and step 9's tests that depend on them are split into
three packets (below). Landing it needs the owner's OK and a game-file run.

## What was checked

- **The code diff** (10 files, +299/−21):
  - **`chunk_worker_threads`, `FrameConfig`/`apply_frame_config` and `zoom_anchor_at`** (`frame.hpp`/`frame.cpp`).
    They are the viewer's own expressions moved into one place; the viewer's behaviour is unchanged (`hw * 3 / 4`
    floored at 1, the same anchor formula). `bench_frame --zoom-sweep` now uses them. This fixes finding B1: the
    sweep used `(threads + 1) / 2` workers and never set the anchor or the resident levels.
  - **`frame_config_test.cpp`:** tests all three, with no GPU and no game files.
  - **Step 7:** the per-level table gains the chunks step's p99 and max over every frame. The `baseline:` line the
    contract parses is unchanged.
  - **Step 8:** `zoom_sweep_control_readiness`, opt-in (`ANNO_ZOOM_SWEEP_PROOF=1`, exit 77 otherwise), `gpu` label,
    `RESOURCE_LOCK gpu`. The busy-time check is dropped only under `--readiness-only`; the threshold is not
    relaxed. The `ANNO1503_GAME_DIR` override fixes a real worktree bug.
- **No renderer or GPU behaviour changed.** Only the proof harness, a reported metric and an opt-in test.
- **Gates (REPORTS):**
  - MSVC Release with game files, render and app: 7 refusals, each `VERDICT: PASS` when rerun alone;
  - the GPU gate on the 4090: 117/117;
  - the full "before" sweep baseline on the fixed harness (4090 legs/wheel/pan/zoompan, iGPU wheel).
  - NOT_RUN: the iGPU GPU gate (reasoned in the report).

## Stopping after step 1, 7 and 8

The executor judged steps 2 to 6 one coupled redesign, with the review's own risks inside it:
- B2: the prefetch gate `!waiting && uploads == 0` in `gpu.cpp`;
- B3: priority inversion against the reaper, and an unverified wgpu-core point.

Stopping there and reporting is what the packet asks.

## The split (orchestrator's decision, packet size rule of 2026-11-12)

Each is about 2 to 4 h, on its own branch. The spec is `tasks/zoom-smooth-1.md`'s steps as written, with the review
findings named there.

1. **ZOOM-SMOOTH-1b:**
   - steps 2 and 3, the byte budget over CPU and GPU, and the resident levels chosen by it;
   - step 6, rate-limited eviction, which works inside the same budget;
   - step 9's tests for these.
2. **ZOOM-SMOOTH-1c:** step 4, the zoom-out ring about the anchor, ordered by time-to-visible, inside the budget; its
   step-9 tests.
3. **ZOOM-SMOOTH-1d:** step 5, the GPU upload share kept while chunks wait (B2), with B3's priority inversion and the
   wgpu-core version point checked first; its step-9 tests and the packet's full target on `lan5`.

The order after that is unchanged: ZOOM-SMOOTH-2, ZOOM-SMOOTH-3, then RENDER-DIRECT.
