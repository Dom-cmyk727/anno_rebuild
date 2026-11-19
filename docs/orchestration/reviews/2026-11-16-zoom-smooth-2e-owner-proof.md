# ZOOM-SMOOTH-2e: the owner's fast drag (2026-11-16, evening)

**Build:** `task/ZOOM-SMOOTH-2` `46fd1f2b` (the code of `c164eb52`), the executor's own worktree build, started by
the orchestrator with `ANNO_VIEW_LIVE_RESIZE_TRACE=1` and `ANNO_VIEW_HITCH_MS=1`, `--play data/maps/lan5.txt --water
enhanced`, RTX 4090. The owner dragged the window by its title bar, flicking it back and forth. The log stays
outside git (`cache/drag-test/live-resize-trace-2e-191502.log`). A first run recorded no drag (only the click on the
close button) and was discarded.

| Measure | 2d (before the fix) | 2e |
|---|---|---|
| Drag | fast | 16.9 s, 51,452 px of window path, up to 19,766 px/s in a 100 ms window |
| Draws in the drag | | 1,155 (990 from the timer, 165 from the move trigger) |
| Gap between draws, median | | 15.9 ms |
| Gap between draws, 99th percentile | 136.3 ms | 24.4 ms |
| Gap between draws, longest | 526.1 ms | 30.9 ms |
| Gaps over 33 ms | many | 0 |
| A draw's own duration | | median 0.60 ms, longest 1.50 ms |

- **MET.** With the move trigger the longest gap in a fast drag is two timer intervals; before it was half a
  second. The timer was starved in one 100 ms interval of 158 and the move trigger covered it.
- **One 66 ms gap at the start of the drag**, between the last ordinary frame and the first draw inside the move
  loop. It is the entry into Windows' move loop (the click on the title bar), once per drag, and not what 2e
  changes. Noted, not a finding against 2e.
- **Open before landing (small):** the 2d review asked that the move trigger's thread be asserted or guarded. 2e
  argues it in a comment for Windows from the SDL source. A guard costs one comparison and holds on every
  platform: [ZOOM-SMOOTH-2f](../tasks/zoom-smooth-2.md#zoom-smooth-2f-guard-the-move-triggers-thread).

**Verdict: the owner's proof for the drag is MET.** ZOOM-SMOOTH-2 (2, 2b, 2c, 2d, 2e) lands after 2f, with the
owner's OK. Maximise and restore (121 to 151 ms) were never in this packet and stay open.

## ZOOM-SMOOTH-2f: the orchestrator's review (2026-11-16)

`task/ZOOM-SMOOTH-2` `d33432aa` (code `5b5df274`): `live_resize.hpp`, `viewer.cpp`, `live_resize_test.cpp`.

- **The guard is where it was asked for.** `live_resize_watch` compares the calling thread with the id taken in
  `run()` before the watch is installed, for both triggers, after the event-type test and before the first read of
  the watch's state. Another thread's call is counted and returns; the count is printed at exit only when it is not
  0. The id is written before `SDL_AddEventWatch`, so no watch call can read it earlier.
- **F1 (note): the test's name contradicts its body.** It says "no second thread, both ids synthetic" and starts a
  real second thread to get a real id, which is the better test. Correct the name at the next touch of the file.
- **F2 (accepted): the test covers the decision, not the call site.** Removing the guard from `viewer.cpp` fails no
  test, because the watch needs a `Viewer`. Accepted for this guard: it is a defensive check whose counter reports
  at exit, the decision is one comparison, and the owner's fast drag was measured on the code before it (the guard
  changes nothing for the window thread's own calls: the suite's counts are 2e's).
- **Gates as reported:** `anno_app_tests`, `anno_app_input_tests` and `viewer_wiring_*` 352 of 352; clang-format
  clean; the WSL warning check clean on the touched files.

**Verdict: ACCEPTED, once CI is green on `d33432aa`** (run 43946504718, queued at review time). ZOOM-SMOOTH-2 (2 to
2f) is then ready for a landing candidate; landing needs the owner's OK.
