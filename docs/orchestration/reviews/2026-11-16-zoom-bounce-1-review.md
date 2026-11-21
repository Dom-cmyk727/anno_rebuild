# ZOOM-BOUNCE-1, one frame at the wrong scale: review (2026-11-16)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-BOUNCE-1` `1b7d722d` (test and script `0df9b53e`), base main `4b104d3a`. **CI:** green, `gate` included.

## Verdict

**ACCEPTED as "not reproduced in the view state". No zoom change was made, as the packet required.** One check on
the recording decides whether anything more is done.

## What was done

- A dev script with the owner's wheel sequence at the same window position (1080, 635) and size (2560 x 1369): 50
  repeats, 800 wheel events, 240 reversal legs, **45,825 drawn frames**, read through the existing `ANNO_VIEW_TRACE`.
- Looked for the reported signature: a group that flips for one frame, or a one-frame spike in the eased tile
  width. **None**, also while up to 101 stand-in dissolves were running.
- The discrete zoom path (`pending_zoom_`) is not taken on this map (continuous zoom on the GPU), so it is ruled out.
- A regression test kept: the group sequence through `zoom_advance` over 9 notch delays and 6 frame rates
  (19,617 checks): the easing itself cannot bounce.

## The orchestrator's hypothesis, to be checked on the recording

The owner's log for that session counts **exactly one not-ready frame** in the interval that ends at video 16.10 s
("1 of 600 frames drew a visible chunk not built yet (at most 5 at once, zoom group 0)") and none in the next. The
flagged frame is at 16.17 s; the alignment is good to about 0.05 s. Five stand-in chunks in one frame would look
like "one frame at a different scale" in those regions. The executor's checks read the view state, not the
picture, so they could not flag a stand-in frame.

The UI lane, which has the recording's frames, is asked whether the difference in that frame is confined to a few
regions.
- **If it is:** the "bounce" is the known stand-in remainder of ZOOM-SMOOTH-1, which RENDER-DIRECT removes
  (ADR 0007). ZOOM-BOUNCE-1 closes.
- **If the whole frame differs:** instrumentation inside `FrameRenderer` follows as its own packet.

## Note on the gates

Two full render-suite attempts timed out on unrelated tests under memory pressure; the nine tests passed alone. The
packet changes no render code, so the render suite was not needed: gates cover the changed lanes.

## Update, the same day: the hypothesis is refuted by the recording

The UI lane extracted frames 966 to 973 exactly and compared them.

- **The difference is over the whole map**, not in a few regions: 6,695 of 6,696 blocks differ between frames 969
  and 970, evenly in every band. Frames 969 and 971 are nearly identical.
- **Frame 970 is the whole view zoomed in by about 1.5 % around the cursor**, as sharp as its neighbours, with no
  seams and no patches; the next frame is back at the old scale. It is not a copy of any frame 966 to 973.
- **No capture fault:** no tear, no duplicated frame.

So the one not-ready frame in the log does not explain it. It is a real one-frame overshoot of the zoom that was
presented: the view is steady before and after, and one frame is 1.5 % closer. The orchestrator's stand-in idea was
wrong.

## ZOOM-BOUNCE-2 (released, about 1.5 hours)

The view is steady at zoom group 0, the closest level, before and after the odd frame. That points at a transient
at the zoom limit rather than at the easing between levels, which ZOOM-BOUNCE-1's sweep cleared. Leads to test, not
to assume:
1. **A wheel notch inwards while already at the closest zoom.** If the notch is applied for one frame before the
   limit clamps it, one frame is drawn slightly closer and the next is back. The owner was wheeling in and out
   quickly and reached group 0 just before; one notch too many is likely. ZOOM-BOUNCE-1's script followed the group
   sequence and may never have sent a notch at the limit.
2. **The first frame of a zoom-out from the limit**, drawn with the new target but the old anchor or the other
   way round.
3. **The scale that is presented**, not only the view state: log per frame the scale and origin handed to the frame
   renderer and the resample factor, and compare them with the view's.

Steps: extend the script with notches at both zoom limits and with notches that arrive within a few frames of each
other; look for a frame whose presented scale differs from both neighbours by more than 0.5 %; fix at the cause;
a test that fails before the fix. If it still does not reproduce, say so and stop.

## ZOOM-BOUNCE-2: reviewed, NOT REPRODUCED again (`bf35b89c`, code `9b53a862`; CI green)

- The trace now also records the resample scale and offset that are handed to the frame renderer. Over 40 repeats,
  960 notches and **21,775 presented frames**, driven hard to each zoom limit with notches back to back: no frame
  whose presented scale differs from its neighbours, no difference between the view's scale and the presented one
  above 0.05 %, no group flicker, the drawable steady at 2560 x 1369.
- Lead 1 is false by construction: the limit clamp runs inside every zoom call, and a new test sends 20 inward
  notches at the limit with no frame between them; the target never passes the limit.
- **A fact found on the way:** the closest zoom of the rebuild is a tile width of 84 times the square root of 2,
  118.79 px (`kZoomEndFactor`, `zoom.hpp`). The original's closest level is 84 px. That is the 1.42 the
  orchestrator measured against the owner's History Edition recording: **the rebuild zooms in further than the
  original.** Whether that stays the default is the owner's choice.

## Where this stands: a new hypothesis, to be tested with the owner's real wheel

Two scripted attempts with synthetic wheel events found nothing. The odd frame is the whole view about 1.5 % closer
around the cursor, steady before, back afterwards, and the owner was about to wheel outwards. Hypothesis, not a
finding: **the input itself** held one inward tick just before the outward ones, either the finger's or the wheel
encoder's bounce. The view eases towards the new target for a few milliseconds, the next tick reverses it, and at
about 300 drawn frames per second one captured frame of 60 shows it. The viewer would then be showing what the
wheel sent.

No log of the owner's session holds the wheel events, so this cannot be decided from what exists.

**ZOOM-BOUNCE-3 (queued after ZOOM-SMOOTH-3, about 45 minutes):** trace every wheel event (time, the raw and the
integer deltas, the direction flag, the pointer, the target before and after) into the existing window trace; a
scripted inward tick followed by outward ticks 5, 10 and 20 ms later, to show what the picture does. Then the owner
wheels in and out quickly for about 20 seconds with the trace on. If opposite ticks a few milliseconds apart are
there, the cause is the input, and whether to filter them is the owner's choice (a feel change). If they are not,
the per-layer instrumentation inside the frame renderer is next.
