# ZOOM-SMOOTH-2, drawing during a window drag or resize: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-2` `c86ab749`, base `origin/main` `b9b8df23`.
**CI:** green, `gate` included.

## Verdict

**NOT YET ACCEPTED: two fixes (ZOOM-SMOOTH-2b), then the owner's hands-on proof.** The mechanism is right and well
verified: SDL 3.4.18's live-resize timer calls event watches synchronously inside the Windows modal loop, and only
the Windows backend sends that signal. `frame_step()` is now the one drawing path, guarded against reentrancy. Two
problems block acceptance.

## Findings

- **F1, a data race (CLAUDE.md: no undefined behaviour).** An SDL event watch runs on whichever thread calls
  `SDL_PushEvent`: the sim's wake listener (`viewer.cpp` 1037, from `PlayRunner` in `play.cpp` 1067) and the dev
  script push from other threads. `live_resize_watch` reads `self->drawing_frame_`, a plain `bool`, before it
  checks the event type (`should_draw_from_watch` takes it by value). Each such push reads it while the main thread
  writes it. **Fix:** test the event type first and touch viewer state only for `EXPOSED` with `data1 == 1`; make
  the guard `std::atomic<bool>`, or assert that the drawing watch runs on the window thread. Add a test where a
  second thread pushes events while a watch draw is in progress (TSan in WSL if it builds; else say so).
- **F2, the sim blocks during a drag (the packet: "the sim never blocked").** SDL holds the watch-list lock for the
  whole watch, draw included. `PlayRunner`'s turn publication pushes its wake event under `listener_mu_`, so a sim
  turn waits for a whole frame during a drag. Short is not zero, and the packet asked for zero. **Fix:** the wake
  path must never wait for a draw. One way: while a watch draw runs (an atomic flag), the listener marks the wake
  as pending instead of pushing, and the watch's frame or the next main-loop pass picks it up
  (`batches_waiting_` already keeps `demand()` animating). Prove it with a test that a wake during a watch draw
  returns without waiting.
- **Note, no action:** `frame_step()` calls `demand()` again after the event poll, so the main loop's draw decision
  now sees events from the same pass. That is a small behaviour change from the old loop, not a verbatim move. It is
  harmless (`demand()` is `const`); say so in the report.

## What was checked

- `live_resize.hpp`: true only for `SDL_EVENT_WINDOW_EXPOSED` with `data1 == 1` and not already drawing.
- The watch is added after window creation and removed by an RAII guard on every return path of `run()`.
- Move never reconfigures the surface. Resize still does (OQ-098); debouncing stays the owner's reserved choice,
  rightly not taken.
- Gates: app suites 321 executed, 0 failed; `viewer_wiring_*` 20/20; WSL `-Werror` clean apart from the known
  `extensions_test.cpp`.

## The executor's QUESTIONs

1. **The sim contention** is not accepted as shipped: F2.
2. **The hands-on proof** needs a real window drag, which no dev script can make. The executor could not get GUI
   control, and that refusal is not routed around. It goes to the owner after 2b: 5 s drag, 5 s resize,
   maximise and restore, with `ANNO_VIEW_HITCH_MS=1`, as in ZOOM-MEASURE-1. The executor analyses the trace.
