# ZOOM-SMOOTH-2d, why the drag stalls: review (2026-11-16)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-2` `d80beed7` (trace code `5adba936` and `56be3ce8`; the analysis is docs).
**The run:** the owner, 2026-11-16 09:08, the 2d build with the trace on: a hold, a slow drag, a fast drag.

## Verdict

**ACCEPTED. The cause is found; the fix is released as ZOOM-SMOOTH-2e.**

## The cause

| Hypothesis | Result |
|---|---|
| (a) Windows waits before it enters the move loop | **Wrong.** `WM_ENTERSIZEMOVE` follows the title-bar press within about 1 ms, all three times |
| (b) The redraw timer is starved by move messages | **Confirmed.** `WM_TIMER` is only delivered when no other message waits. Slow drag: 39 move messages per 100 ms, no slot without a timer tick, 63 draws per second. Fast drag: 69 per 100 ms, 19 of 91 slots with no tick at all, 25 draws per second, gaps up to 526 ms |
| (c) Other load on the PC | Not supported: the stalls scale with the move messages in each phase |

All 30 largest frame gaps (526 ms down to 105 ms) are exactly the gap to the previous watch tick; the draw itself
stayed fast and the reentrancy guard never skipped one. During the worst gap `SDL_EVENT_WINDOW_MOVED` kept arriving
every 1 to 2 ms, so a second trigger is available. The executor checked the orchestrator's first-look figures
independently; they matched.

**The owner saw no stutter.** Likely because Windows' compositor moves the last picture with the window, so a
frozen picture inside a fast-moving window is hard to see. The water and the ships do stand still for up to half a
second, so the stall is real.

## ZOOM-SMOOTH-2e (released, about 1 hour)

Same branch. Build the executor's proposal with these decisions:

1. **Trigger:** on `SDL_EVENT_WINDOW_MOVED` in the watch, a pure predicate (time since the watch's last tick, a
   minimum interval; no clock read inside) decides; when it passes, run the **same** sequence the `EXPOSED(1)` path
   runs (`begin_watch_draw`, `frame_step`, `end_watch_draw`, `mark_watch_tick`). No second drawing path.
2. **The interval is 16 ms**, not 30: the packet's target is a gap of about 16 ms, and one frame at 60 Hz. While the
   timer ticks normally the predicate rarely passes; when it is starved, the moves keep about 60 draws per second.
   A named constant with this reason; not tied to the display's refresh rate.
3. **Move only.** Resize is not starved (its gaps are the surface reconfigure, OQ-098, the owner's reserved
   choice). Maximise and restore (121 to 151 ms, one transition each) stay out of this packet.
4. **Thread safety as in 2b and 2c:** the event type is tested first; shared state only through `LiveResizeState`.
   `WINDOW_MOVED` is sent by the window thread; say so and assert or guard it.
5. **Tests that fail on the base:** the predicate; a flood of moves gives at most one draw per interval; a timer
   tick resets the interval.
6. Keep the trace. The owner then repeats the fast drag once on this build; the trace must show no gap far above
   the interval in a fast drag.
