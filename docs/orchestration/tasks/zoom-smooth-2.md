# ZOOM-SMOOTH-2: no slow frame at maximum zoom-out, and the picture keeps moving while the window is dragged

**Status:** written 2026-11-14 by the Claude Code orchestrator for **the Windows executor**. The owner's order
(2026-11-14): ZOOM-SMOOTH-2 is released right after ZOOM-SMOOTH-1 ("make sure that will be fixed once the executor is
done with ZOOM-SMOOTH-1"), before GATE-2a. Step 2, second half, of
[`zoom-smoothness.md`](../../render/zoom-smoothness.md) §6: the window-thread causes (§3 RC4).

**Base:** `origin/main` **after ZOOM-SMOOTH-1 has landed** (both touch the frame renderer). Check that ZOOM-SMOOTH-1's
merge is an ancestor of `origin/main`; if not, stop and report. Branch `task/ZOOM-SMOOTH-2`, a new worktree. About
3 hours. Never rebase, force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern: the window thread's time per frame;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

The owner's display runs at 500 Hz, so every frame above 2 ms drops at least one refresh. The owner's zoom session of
2026-11-14 ([`zoom-session-2026-11-14.md`](../../render/zoom-session-2026-11-14.md), F7; run 3, no capture) found
single frames of 12–18 ms, all at or just after maximum zoom-out (zoom group 4). The viewer's own step timing names
the step each time:

| Frame | The step that took the time | What the step is (`gpu.hpp`) |
|---|---|---|
| 18.2 ms | `sync drops` 14.9 ms | dropped chunks: their planes go to the plane pool, which keeps a bounded number, and **the rest is freed on the window thread** |
| 15.4, 12.6, 14.4 ms | `record` 12.8, 10.8, 13.7 ms | recording the three passes |
| (run 2) | `finish` 9.1–9.4 ms | finishing the command buffer |
| (run 2) | `chunks` 17.0 ms | taking finished chunks from the workers |

**The window drag.** ZOOM-MEASURE-1 measured a 106 ms hitch during a window drag, 96.7 ms of it inside the
`SDL_PollEvent` loop. That is Windows' modal move/resize loop: while the title bar is held, Windows runs its own message
loop and the viewer's frame loop doesn't run. A restore from maximised spent 16 ms in `surface configure`.

The owner reads both as stutter. ADR 0007's direct renderer changes the chunk path later, but the owner wants these
gone now, after ZOOM-SMOOTH-1.

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/render/zoom-smoothness.md` §3 RC4 and §4 "For the stutter";
- `docs/render/zoom-session-2026-11-14.md` F7;
- the ZOOM-MEASURE-1 review (the hitch mechanism, and note 2 on submission jitter);
- `docs/app/window-trace.md`;
- `docs/perf/README.md` (the frame steps, `frame hitch:` lines, the 2026-10-19 stutter fixes: the reaper thread,
  upload slicing);
- `src/render/include/anno/render/gpu.hpp` (`GpuStats` and its steps);
- the chunk sync and drop path, the plane pool, and the existing reaper (`frame.hpp` `reaper_`);
- `src/app/src/viewer.cpp`: the event loop, `SDL_EVENT_WINDOW_EXPOSED`, and `measure`.

## Do

**Cause first, one step at a time.** For each step, measure what it does at maximum zoom-out before changing it:
visible chunks, draws, bind groups, chunks dropped, planes freed, chunks received. Put these counts into the
`frame hitch:` line where they are missing, so a slow frame names its count as well as its time.

1. **Drops.** No GPU object is freed on the window thread in a frame.
   - Freed planes go to the existing reaper, or are released over several frames under a time budget.
   - If wgpu-native requires a release on the device's thread, cite the source and use the budget instead.
   - Size the plane pool from a measurement, not a guess.
2. **Chunk intake.** Taking finished chunks from the workers stays under a per-frame time budget; the rest waits for
   the next frame. State the budget and show that ZOOM-SMOOTH-1's readiness numbers don't get worse.
3. **`record` and `finish` at maximum zoom-out.** Find which count grows there: draws, bind groups, pieces, stand-in
   cover, or the wave pass.
   - **If the fix is small within today's design,** do it: for example, skip invisible or open-sea pieces, or stop
     re-creating a buffer every frame.
   - **If it needs fewer textures per frame** (one texture array or atlas for the chunks, which is RENDER-DIRECT's
     ground), **stop with a QUESTION.** Attach the measurement, and don't build RENDER-DIRECT twice.
4. **The window drag and resize.** The picture keeps updating while the window is dragged or resized.
   - Use what SDL3 provides for live resize on Windows, for example drawing from an event watch on
     `SDL_EVENT_WINDOW_EXPOSED` during the modal loop. Cite SDL3's source or documentation for the mechanism you use.
   - Move `surface configure` work off the critical path where possible, for example pre-created or deferred
     resources.
   - The sim is never affected: rendering only reads sim state.
5. **Tests:**
   - a unit test that a drop frees nothing on the calling thread beyond the budget;
   - a unit test of the intake budget;
   - for the drag, the window trace (`ANNO_VIEW_TRACE`) on a real window is the proof. A headless test of an event
     watch is enough if one exists in SDL's dummy driver; say which.

**Not this packet:** chunk readiness (ZOOM-SMOOTH-1), stand-ins and dissolves, camera jumps (RENDER-DIRECT), the water.

**Stop with a QUESTION if:**
- a fix needs the chunk texture layout changed (RENDER-DIRECT);
- a fix would touch `src/sim`;
- the drag fix would need a second render thread. That is an architecture choice for the owner.

## Proof

All numbers on this PC, with an otherwise idle machine, one run at a time.

1. **The owner-like live session, before and after.** Use `E:\anno1503-worktrees\zoom-session-2026-11-14\zoom.script`
   with `tools\drive2.py ... --no-capture` and `tools\analyze2.py`, outside git. Report per move: worst busy,
   frames above 2 ms, gaps above 6 ms, and the steps of the five slowest frames.
2. **The targets:**
   - **no frame above 2 ms busy** whose largest step is `sync drops` or `chunks`;
   - **`record` and `finish` at maximum zoom-out** below 2 ms, or a QUESTION with the measurement;
   - **a 5 s window drag and a maximise/restore cycle,** traced: the longest gap between presented frames during the
     drag is stated, against today's 106 ms. Aim for one display interval of the modal loop's redraw rate, and state
     what that rate is.
3. **The sweep:** `zoom_sweep_contract` (opt-in) before and after. Busy p99 and max, and frames above 2 ms, per profile.
4. **The gates:** the changed render and app lanes through the canonical runner, with classified counts.
   - A test refused for memory did not run: rerun it alone with `--jobs 1`, with its own log.
   - Run the WSL warning check.
   - Hosted CI must be green on the head, `gate` included.

**Not yours:** the full suite and the game-file run (the orchestrator's, at the landing).

## After the landing (the orchestrator, owner's request 2026-11-14)

When ZOOM-SMOOTH-2 is on `main`, the orchestrator:
1. starts the enhanced viewer for the owner's OBS recording: `lan5`, maximised, logged with clock time by
   `tools\drive_manual.py`;
2. analyses the video thoroughly against the 2026-11-14 baseline;
3. runs the scripted session again.

## Report

READY FOR REVIEW, about 60 lines:
- each step's cause and fix;
- the before/after tables;
- the drag trace;
- the budgets;
- the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
