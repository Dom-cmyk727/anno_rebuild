# ZOOM-SMOOTH-2, the owner's hands-on proof: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-2` `3205965c` (the analysis; code unchanged since `e47b9a3c`).
**The run:** the owner, 2026-11-15 20:22, the 2c build, 4090, `lan5`, enhanced water, `ANNO_VIEW_HITCH_MS=1`:
wait, drag, resize, maximise, restore, close. An earlier one-hour trace was idle (the owner had not yet done the
test; the orchestrator assumed it from the closed window) and is not evidence.

## Verdict

**The target is NOT MET for the drag. ZOOM-SMOOTH-2 stays open; it does not land yet.**

| Phase | Result | Against the target (about 16 ms) |
|---|---|---|
| Untouched window | mean 2.0 ms, worst 3.2 ms | met |
| Resize | mean 6.9 ms, worst 31.9 ms; every large gap is the surface reconfigure (OQ-098) | close; much better than before |
| Drag | p50 2.0, p90 16.0, p99 85.0, **worst 298.4 ms**; the large gaps have under 1.2 ms of work and 0 ms of event handling | **not met** |
| Maximise | 150.6 ms, 98.5 % in window events | unchanged; not in this packet's fix |
| Restore | 120.9 ms, 98.3 % in window events | unchanged; not in this packet's fix |
| Two gaps after a resize gesture ends | 114.5 and 158.3 ms, in window events | the hand-back to the normal loop, not covered |

## What the drag result means

During the large drag gaps nothing ran on the window thread at all: no draw, no event handling. So the watch
was not called for up to 298 ms. The trace cannot say why. The executor did not guess, which is right. Hypotheses
to test, none proved:
1. **The hold before the move loop.** Windows waits on a title-bar press before it enters its move loop; until
   `WM_ENTERSIZEMOVE` SDL has not armed its timer.
2. **Timer starvation.** `WM_TIMER` is only delivered when the message queue is otherwise empty. A fast mouse may
   keep the modal loop busy with move messages, so SDL's 10 ms timer rarely fires. A resize also raises size
   events, which would explain why it is smooth and the drag is not.
3. **Other load.** One single-core process ran elsewhere. Unlikely to explain 298 ms on a 16-thread CPU, but not
   ruled out.

## Also found

- `events_ms_` can exceed the frame's own gap (5763 ms against 18 ms): the main loop's event timer keeps its start
  from before the modal loop. A diagnostic bug, to fix in 2d.

## Next: ZOOM-SMOOTH-2d (released, about 1.5 h, diagnostic)

Same branch. No behaviour change except the `events_ms_` fix.
1. Behind `ANNO_VIEW_LIVE_RESIZE_TRACE=1`, log with a monotonic timestamp: every watch call that passes the type
   test; every `SDL_EVENT_WINDOW_MOVED` and `RESIZED` the watch sees; each watch draw and each one skipped by the
   guard; and, through `SDL_SetWindowsMessageHook` if the pinned SDL has it, `WM_ENTERSIZEMOVE`, `WM_EXITSIZEMOVE`
   and counts of `WM_TIMER`, `WM_MOVING` and mouse-move messages per 100 ms inside the modal loop.
2. Fix `events_ms_` so a watch draw inside the poll does not leave a stale start.
3. A unit test for whatever is testable without a window (the trace's bookkeeping); gates as before.
4. The owner then makes one short run: press and hold the title bar 2 s without moving, drag slowly 5 s, drag
   fast 5 s, close. The trace answers hypotheses 1 and 2 directly. Report what it shows, per hypothesis, and
   propose the fix; do not build the fix in this step.
