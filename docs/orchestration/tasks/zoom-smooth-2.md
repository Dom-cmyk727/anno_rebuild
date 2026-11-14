# ZOOM-SMOOTH-2: the picture keeps moving while the window is dragged or resized

**Status:**
- **Written** 2026-11-14 by the Claude Code orchestrator for **the Windows executor**.
- **Narrowed the same day** after an independent critical review:
  - this packet is now only the window drag and resize, which doesn't touch the frame renderer and survives
    ADR 0007 (its decision 6);
  - the window-thread spikes at maximum zoom-out moved to [ZOOM-SMOOTH-3](zoom-smooth-3.md).
- **The owner's order** (2026-11-14): fixed right after ZOOM-SMOOTH-1 ("make sure that will be fixed once the executor
  is done with ZOOM-SMOOTH-1").

**Base:** `origin/main` at release. It is independent of ZOOM-SMOOTH-1's code, so no ancestor check is needed. Branch
`task/ZOOM-SMOOTH-2`, a new worktree. About 3 hours. Never rebase, force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern: frames during a window move or resize;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

ZOOM-MEASURE-1 measured a 106 ms hitch during a window drag, 96.7 ms of it inside the `SDL_PollEvent` loop. While the
title bar is held, Windows runs its own modal move/resize loop and the viewer's frame loop doesn't run. A restore from
maximised spent 16 ms in `surface configure`. The owner reads it as stutter.

**What SDL3 offers** (verified by the review in SDL 3.4's source; re-check against the SDL version this build pins):
- On `WM_ENTERSIZEMOVE` SDL starts a `USER_TIMER_MINIMUM` (10 ms) timer.
- Each `WM_TIMER` calls `SDL_OnWindowLiveResizeUpdate`. Without main callbacks, that sends
  `SDL_EVENT_WINDOW_EXPOSED` with `data1 = 1` to the event watchers.
- So a redraw from an event watch runs at most about 64–100 Hz during a drag, not at 500 Hz.

Sources:
- `src/video/windows/SDL_windowsevents.c`;
- `src/video/SDL_video.c`;
- `src/events/SDL_eventwatch.c`;
- `SDL_events.h` (watchers may run on any thread, under the watch list's lock).

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/render/zoom-smoothness.md` §3 RC4, §4 "For the stutter", §5 (the research on the modal loop);
- the ZOOM-MEASURE-1 review (the hitch mechanism);
- `docs/app/window-trace.md` (`ANNO_VIEW_TRACE`);
- `docs/open-questions.md` OQ-098 (surface reconfigure per resize tick, 9.5–16 ms; option 2, debouncing, is the
  owner's decision) and OQ-099 (the offscreen targets regrow, 14–37 ms on the iGPU);
- `src/app/src/viewer.cpp`: the event loop, `SDL_EVENT_WINDOW_EXPOSED`, the frame step, `pending_zoom_`,
  `script_tick`;
- `src/app/src/play.cpp`: the runner pushes its wake event from the sim thread while holding `listener_mu_`.

## Do

1. **Draw during the modal loop.**
   - **The trigger:** an SDL event watch that draws a frame only on `SDL_EVENT_WINDOW_EXPOSED` with `data1 == 1` (a
     live resize or move update).
   - **The path:** through the same frame-step function as the main loop, not a second drawing path.
   - **A reentrancy guard:** a watch never draws while a frame is already being drawn.
   - **Order:** the main loop's order (pending zoom, script tick, draw) must hold inside the watch too. Check every
     test and proof that relies on that order (the dev script, `REPORTS.md` around the W4 window-wiring rounds).
2. **The sim is never blocked.**
   - Watchers run under SDL's watch-list lock. The runner's wake push (`SDL_PushEvent` from the sim thread, under
     `listener_mu_`) waits for that lock.
   - Show that a draw inside a watch cannot stall a sim turn, or stop with a QUESTION and the measured stall.
   - Rendering only reads sim state.
3. **Move and resize are different cases.**
   - **A move** changes no size: no surface reconfigure, no target regrowth.
   - **A resize** reconfigures the surface on every tick (OQ-098) and regrows the offscreen targets (OQ-099).
   - **Fix the move fully.** For the resize, reduce what the code allows, for example deferring the target regrowth to
     the end of the size change.
   - Debouncing the reconfigure is OQ-098 option 2, an owner decision. Don't choose it; report what it would buy.
4. **Linux and macOS.** X11 and Wayland have no modal loop: the watch must draw nothing there, and the normal loop
   keeps running. State it in the code and the docs.
5. **Tests:**
   - **Headless:** under SDL's dummy driver, an `SDL_PushEvent` of `SDL_EVENT_WINDOW_EXPOSED` with `data1 = 1` reaches
     the watcher.
     - Test that it draws once and doesn't reenter.
     - Test that a plain expose (`data1 == 0`) doesn't draw from the watch.
   - **The real modal loop** can't be tested headless. The trace below is its proof.

**Not this packet:** the window-thread spikes at maximum zoom-out (ZOOM-SMOOTH-3), chunk readiness (ZOOM-SMOOTH-1),
anything in the frame renderer.

**Stop with a QUESTION if:**
- the fix needs a second render thread (an architecture choice for the owner);
- a watch draw can stall the sim;
- a fix touches `src/sim` or the frame renderer.

## Proof

All on this PC, with an otherwise idle machine, one run at a time, with the window trace (`ANNO_VIEW_TRACE`) on a real
window.

1. **A 5 s window drag** (move, no resize), before and after.
   - The longest gap between presented frames during the drag, and the presented frame rate.
   - **Target:** a gap no longer than the modal loop's timer interval plus one frame, about 16 ms, against today's
     106 ms. State the rate you measured.
2. **A resize by dragging the window edge for 5 s**, before and after: the same figures, and the time per tick in
   `surface configure` and in the targets.
   - **Target:** stated after measuring.
   - Report the resize separately from the move.
3. **A maximise/restore cycle**, before and after.
4. **The gates:** the changed app lanes and the new watch tests, through the canonical runner, with classified counts.
   - A test refused for memory did not run: rerun it alone with `--jobs 1`.
   - Run the WSL warning check: the watch code must build on Linux with the CI flags.
   - Hosted CI must be green on the head, `gate` included.

**Not yours:** the full suite and the game-file run (the orchestrator's, at the landing).

## Report

READY FOR REVIEW, about 60 lines:
- the mechanism with its SDL source;
- the guard and the order;
- the sim-stall check;
- the move and resize tables;
- what OQ-098 option 2 would buy;
- the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
