# ZOOM-SMOOTH-2c, the narrowed wake deferral: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-2` `05aab883` (code `e47b9a3c`), on `c4b3d5b7`.
**CI:** green, `gate` included.

## Verdict

**The code is ACCEPTED. The packet closes with the owner's hands-on proof**, which is the only evidence for the
about-16-ms target. Landing waits for that proof and the owner's OK.

## What was checked

- **F3:** `LiveResizeState` keeps two flags. The reentrancy guard is set for every `frame_step()`; the watch-draw
  flag only around the watch's own `frame_step()` call, set first thing after the event-type test. `defer_wake()`
  reads only the watch-draw flag, so an ordinary main-loop draw pushes its wake as before 2b.
- **F4:** `viewer.cpp` and the tests use the one type. The tests no longer rebuild the decision; the wall-clock
  check is gone. New: defer only during a watch draw; a two-thread lost-wake test that ends drained.
- **The lost-wake order, read by hand:** a deferred wake is taken by the next `frame_step()`, which is the next
  watch tick during a drag or the main loop's call when the modal loop returns. One path remains where a wake
  without a batch can wait for the loop's 100 ms idle timeout: the sim reads the flag as set, is preempted past
  the drag's end and past the main loop's next `frame_step()`, and only then stores the pending flag. That needs
  a preemption of several milliseconds at one exact point, delays a redraw and loses nothing. Accepted.
- **The residual push-and-wait window** is stated on the class as bounded, not "never". Accepted in the 2b review.
- **F5:** the executable was relinked after the owner closed the viewer; `viewer_wiring_*` 20/20 on the new binary.
- **Gates:** app suites 324 executed, 0 failed; WSL `-Werror` clean apart from the known `extensions_test.cpp`.
- **NOT_RUN, accepted as stated:** TSan (no such build in the project); the hands-on proof.

## The hands-on proof

The owner drags, resizes, maximises and restores a real window with `ANNO_VIEW_HITCH_MS=1`; the executor reads the
trace. A "before" exists from the owner's UI test on main `cc7af72c`: a 123 ms gap at a maximise.
