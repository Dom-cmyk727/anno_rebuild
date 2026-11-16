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
