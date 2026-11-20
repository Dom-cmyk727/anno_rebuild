# ZOOM-SMOOTH-2b, the data race and the sim wait: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-2` `c4b3d5b7` (code `716973bf`), on `c86ab749`.
**CI:** green, `gate` included.

## Verdict

**F1 ACCEPTED. F2 NOT YET: one side effect, test evidence that does not run the real code, and a gate on a stale
binary (ZOOM-SMOOTH-2c, about 1 h).** The owner's hands-on proof waits for 2c, because the local `anno1503.exe` is
still the pre-2b binary.

## F1, the data race: fixed

`live_resize_watch` tests the event's own type first and reads the guard, now `std::atomic<bool>`, only for
`EXPOSED` with `data1 == 1`, which only the window thread sends. Correct.

## F2, the sim wait: three findings

- **F3, a side effect outside drags.** The listener defers on `drawing_frame_`, which is set during **every**
  `frame_step()`, the main loop's ordinary draws included. In an ordinary draw the window thread holds no SDL lock,
  so a push would not wait; the deferral buys nothing there. It costs something: a wake without a batch (a command
  outcome, an opening's ships) that arrives during an ordinary draw is no longer an event. The loop's idle test
  (`viewer.cpp`, `!animating && !(dirty_ && can_draw)`) does not look at `pending_wake_`, so the loop can sit in
  `SDL_WaitEventTimeout` for up to 100 ms before the next `frame_step()` picks the wake up. Before 2b it woke at
  once. **Fix:** defer only while a *watch* draw runs: a separate atomic set in `live_resize_watch` around its
  `frame_step()` call, first thing after the type test. The main loop's draws push as before.
- **F4, the tests mirror the code instead of running it.** F1's and F2's tests rebuild the watch and the listener as
  lambdas inside the test file. They would still pass if `viewer.cpp` were wrong. **Fix:** move the decision into
  `live_resize.hpp` as one small type (the two atomics, `begin/end` of a watch draw, `defer_wake()`,
  `take_pending()`), used by `viewer.cpp` and by the tests. Drop the "under 1 ms" wall-clock check; "no push while
  a watch draw runs" and "the wake is picked up afterwards, never lost" are the assertions. Add the lost-wake case:
  a wake between the flag's last read and its clear is either pushed or taken, never neither.
- **F5, the gate ran on a stale binary.** `viewer_wiring_*` passed against the pre-2b `anno1503.exe`, because the
  link failed (`LNK1104`: the file is held by the viewer the executor started for 2's proof attempt, which it was
  refused permission to close). That refusal stands, and the orchestrator does not close it either. The owner
  closes that window; then the executable is relinked and `viewer_wiring_*` rerun.
- **The residual window, accepted and to be stated:** the sim can still read the flag as clear after SDL has taken
  its watch lock and before the watch sets the flag. With F3's fix that is a few instructions, not a frame. Removing
  it entirely needs a helper thread, which is not worth it here. The comment in `viewer.cpp` must say this instead
  of "never".
- **TSan NOT_RUN:** accepted as stated; the project has no TSan build.
- **The recipe:** `ANNO_GPU_ADAPTER = "0"` does not say which adapter that is on this PC. Use the value the gates
  use (`4090`), and give the path of the worktree as it really is.
