# ZOOM-SMOOTH-3: no slow frame at maximum zoom-out (the window thread's own spikes)

**Status:**
- **Written** 2026-11-14 by the Claude Code orchestrator for **the Windows executor**. It was split out of ZOOM-SMOOTH-2
  after an independent critical review.
- **The owner's order** (2026-11-14): the slow frames are fixed right after ZOOM-SMOOTH-1, so this packet follows
  ZOOM-SMOOTH-2, before RENDER-DIRECT.
- **It starts by measuring again.** ZOOM-SMOOTH-1 changes the counts these steps grow with. If a spike is gone after
  ZOOM-SMOOTH-1, close that part with the measurement and don't fix it.

**Base:** `origin/main` **after ZOOM-SMOOTH-1 has landed**. Check that its merge is an ancestor; if not, stop and report.
Branch `task/ZOOM-SMOOTH-3`, a new worktree. About 3 hours. Never rebase, force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern: the window thread's busy time per frame;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

At 500 Hz every frame above 2 ms drops a refresh. The owner's zoom session
([`zoom-session-2026-11-14.md`](../../render/zoom-session-2026-11-14.md) F7, run 3, no capture) found single frames of
12–18 ms, all at or just after maximum zoom-out:

| Step | Time | Note |
|---|---|---|
| `sync drops` | 14.9 ms | GPU chunk planes beyond the pool are freed on the window thread (`gpu.hpp`) |
| `record` | 10.7–13.7 ms | |
| `finish` | 9.1–9.4 ms | run 2 |
| `chunks` | 17.0 ms | run 2 |

**What the review found in the code:**
- **The existing reaper is the wrong place for GPU releases.** It runs at lowered priority (`frame.cpp`
  `ChunkWorkers::reap`) and frees CPU chunks only. `gpu.cpp`'s warm-plane note records that a lowered-priority
  thread holding wgpu-hal's allocator lock inverts priority on the window thread.
- **A release may not save time at all.** In wgpu-core, `Queue::submit` runs `device.maintain`, which destroys
  resources no longer needed by the user but still used in the command stream. So a chunk drawn in recent frames may be
  destroyed inside the window thread's submit, whoever released it.
  - Source: wgpu `wgpu-core/src/device/queue.rs`. Check the version this build pins.
- **`chunks_ms` is not intake.** It covers the whole of `update_chunks` plus the `shown_over_stand_in` loop
  (`gpu.cpp` around the `chunks_ms` laps); `take_done` only moves vectors.
- **`record` and `finish` probably grow with draws × passes:**
  - every draw is recorded in four passes, five with the original water;
  - a stand-in can take up to 81 parts (`kMaxStandInReduction` = 8);
  - every open-sea chunk outside the scene still costs a uniform, a bind group and a draw.

  ZOOM-SMOOTH-1 changes all of these, which is why this packet measures first.
- **Sprite decoding on the window thread** (`decode_ms` of `sprites_ms`, up to 8.8 ms in §1 of `zoom-smoothness.md`)
  is the first item of RC4. Neither earlier packet covers it.

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/render/zoom-smoothness.md` §1, §3 RC4, §4 "For the stutter";
- the ZOOM-MEASURE-1 review, note 2 (driver submission jitter: decide how the busy criterion treats it, on evidence);
- `docs/perf/README.md` (the frame steps, the 2026-10-19 fixes);
- `src/render/include/anno/render/gpu.hpp` (`GpuStats`);
- `src/render/src/gpu.cpp`: `sync_chunks`, the plane pool, the warm planes, recording, finish, submit;
- `src/render/src/frame.cpp`: `update_chunks`, `ChunkWorkers::reap`.

## Do

1. **Measure first, at the base (after ZOOM-SMOOTH-1).**
   - **Split the timers so each step is one thing:** `chunks` into intake, planning, trimming and the stand-in
     loop; `record` into its passes.
   - **Add counts to the `frame hitch:` line:** visible chunks, draws per pass, bind groups, stand-in parts, open-sea
     chunks, chunks dropped, planes freed, sprites decoded.
   - Run the live session (below) and the sweep. Every step that still shows above 2 ms at maximum zoom-out is a
     work item; every one that doesn't is closed with its numbers.
2. **Drops, if they still show.**
   - Release on the window thread under a per-frame byte or count budget, or from a releaser thread at **normal**
     priority. Not the lowered reaper.
   - Show where the cost actually lands, including inside `queue submit` (wgpu's maintain).
   - Size the pool from ZOOM-SMOOTH-1's memory budget, not a new constant.
3. **Chunk intake and planning, if they still show.**
   - A per-frame budget as a field with its reason, scaled to the frame interval, with a guarantee of progress: at
     least one item per frame.
   - Show that ZOOM-SMOOTH-1's readiness numbers don't get worse.
4. **`record` and `finish`, if they still show.**
   - **If the fix is small,** do it. For example: don't draw open-sea chunks outside the scene in every pass; skip
     passes a chunk doesn't need; cap stand-in parts.
   - **If the fix needs fewer textures per frame** (a texture array or atlas for chunks, RENDER-DIRECT's ground),
     stop with a QUESTION. Attach the measurement.
5. **Sprite decoding.**
   - Decode sprite frames on workers (or at load) and hand them to the window thread ready for upload.
   - Measure `decode_ms` before and after, at the first zoom-out and the first visit to each level.
6. **Tests:**
   - drops never free more than the budget on the calling thread;
   - the intake budget guarantees progress;
   - decoding happens off the window thread: a test seam or a counter, not a timing.

**Not this packet:** the window drag (ZOOM-SMOOTH-2), readiness (ZOOM-SMOOTH-1), stand-ins, dissolves, camera jumps
(RENDER-DIRECT), the water.

**Stop with a QUESTION if:**
- a fix needs the chunk texture layout changed;
- a fix touches `src/sim`;
- the packet passes 4 h.

## Proof

All on this PC, with an otherwise idle machine, one run at a time. Name the adapters.

1. **The live session, before and after:** `E:\anno1503-worktrees\zoom-session-2026-11-14\zoom.script` with
   `tools\drive2.py ... --no-capture` and `tools\analyze2.py` (outside git). Per move:
   - worst busy and frames above 2 ms;
   - the steps and counts of the five slowest frames.
2. **The target is judged by total busy time, not by which step is largest,** so moving cost into another step doesn't
   pass.
   - **At maximum zoom-out:** no frame above 2 ms busy that the application causes.
   - **Driver jitter:** frames whose time is only driver submission jitter are classified on evidence, by the rule
     this packet settles for the ZOOM-MEASURE-1 note.
   - If a step can't reach it without RENDER-DIRECT, that is the QUESTION in step 4.
3. **The sweep:** `zoom_sweep_contract` before and after, per profile. Busy p99 and max, and frames above 2 ms.
4. **The gates:** the changed render and app lanes through the canonical runner, with classified counts.
   - A test refused for memory did not run: rerun it alone with `--jobs 1`.
   - Run the WSL warning check.
   - Hosted CI must be green on the head, `gate` included.

**Not yours:** the full suite and the game-file run (the orchestrator's, at the landing).

## After the landing (the orchestrator, owner's request 2026-11-14)

When ZOOM-SMOOTH-1, 2 and 3 are on `main`, the orchestrator:
1. starts the enhanced viewer for the owner's OBS recording (`tools\drive_manual.py`, `lan5`, maximised);
2. analyses the video thoroughly against the 2026-11-14 baseline;
3. reruns the scripted session.

## Report

READY FOR REVIEW, about 60 lines:
- the measurements first: what was closed and what was fixed;
- each fix with its cause;
- the before/after tables;
- the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
