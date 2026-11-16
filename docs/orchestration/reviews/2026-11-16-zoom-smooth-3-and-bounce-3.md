# ZOOM-SMOOTH-3 and ZOOM-BOUNCE-3: the orchestrator's review (2026-11-16)

Both from the Windows executor. CI is green on both reviewed heads.

## ZOOM-SMOOTH-3 (`task/ZOOM-SMOOTH-3` `3526d253`, CI run 43913643801)

**What it did:** measured the window thread's busy time at maximum zoom-out on main `0f46b1b2` (after
ZOOM-SMOOTH-1), twice; added six counts to the `frame hitch:` line (`viewer.cpp`, diagnostics only, printed only with
`ANNO_VIEW_HITCH_MS`). No fix.

**Accepted as measured:**
- The 9 to 18 ms spikes of the owner's session are gone: the worst busy time at maximum zoom-out is 1.3 to 5.1 ms.
- `chunks` (0.1 to 0.3 ms) and `record` (at most 0.1 ms) are closed. Drops, intake and sprite decoding did not show
  above 2 ms, so Do items 2, 3 and 5 are closed without a change.
- `finish` (`wgpuCommandEncoderFinish`) is the one step left: 4.4 to 10.8 ms in the worst frames.
- The control sweep (no camera motion) has 2 of 1,996 frames over 2 ms, at most 3.11 ms, as ZOOM-MEASURE-1 measured.

**Findings:**
- **F1, the count called "visible chunks" is not that.** It is `chunks_built + chunks_missing`: chunks finished in
  that frame plus chunks still being built. At rest it is 0 while hundreds of chunks are drawn. So the six counts
  measure new work in the frame, not how much is drawn. The report's conclusion, "the cost does not scale with
  chunks ... it happens with nothing of the application's own visible", does not follow. What the counts show is
  narrower and still useful: **the `finish` spikes happen in frames with no new work.**
- **F2, the two counts that test the packet's own hypothesis were left out.** The packet said `finish` probably
  grows with draws times passes and asked for draws per pass and bind groups. The report skipped them because "no
  step pointed at them", but `finish` is exactly the step they are for.
- **F3, "inside the driver" is not shown yet.** Not ruled out: a wait on a wgpu lock held by another thread; wgpu
  destroying resources released some frames earlier (the counts look at one frame only); the window thread being
  descheduled. One cheap measurement separates these: the thread's CPU time against the wall time of that one call.
- **F4, the jitter rule is accepted in part.** A frame over 2 ms whose only cost is `finish` or `queue submit`, with
  no new work, is excluded from the 2 ms criterion **up to the 3.1 ms the control sweep shows**. The 4.4 to 10.8 ms
  frames are not excluded: at 500 Hz they drop two to five refreshes, and their cause is not classified.
- The commit trailer names another model than the session's; nothing to fix in history, use the session's own.

**Verdict: ACCEPTED** (measurement and diagnostics), with F1 to F3 as the follow-up
[ZOOM-SMOOTH-3b](../tasks/zoom-smooth-3.md#zoom-smooth-3b-classify-the-finish-spikes). The report's two questions:
RENDER-DIRECT's priority does not change on this evidence (3b decides whether it has to); the classifier goes into
`zoom_sweep_contract.py` only after 3b, with the rule of F4.

## ZOOM-BOUNCE-3 (`task/ZOOM-BOUNCE-1` `fc1fbff7`, CI run 43916778845)

**What it did:** every wheel event of the continuous zoom goes into the window trace (`wheel.tsv`, only when
`ANNO_VIEW_TRACE` runs); a scripted pair of opposite notches 5, 10 and 20 ms apart.

**Accepted as measured:** an inward notch followed by an outward one shows as a short zoom-in of 1.47 % (5 ms, one
frame), 2.40 % (10 ms) and 4.08 % (20 ms). The owner's frame was about 1.5 % for one frame. The zoom code does what
it is told; the hypothesis that the real wheel sent two opposite notches a few milliseconds apart now has a
mechanism with the right size.

**Notes:**
- The match is of size and duration, not proof that the owner's wheel did this. The owner's 20 s wheel test with
  the trace settles it.
- Only the continuous zoom's wheel path is traced; the stepped path is not. Acceptable for this question.
- No filter was added. Whether close opposite notches are filtered is a change of feel and the owner's decision.

**Verdict: ACCEPTED.** It lands with ZOOM-BOUNCE-1 and 2 in a later candidate. Open: the owner's wheel test.
