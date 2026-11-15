# ZOOM-SMOOTH-1c2, the zoom-out ring: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-1c2` `619f88f6`, built on `task/ZOOM-SMOOTH-1c` `96c00844`.
**CI:** green, `gate` included.

## Verdict

**ACCEPTED, with one condition carried into ZOOM-SMOOTH-1d.** The ring is correct and cheap on the 4090, but it
measured no readiness gain, and on the iGPU it costs busy time:
- the zoom-out readiness counters are within noise of 1c on every mode and on both adapters;
- iGPU wheel frames over 2 ms went from 12 to 19–24, a real gap over three reruns.

The likely reason it buys nothing yet: the chunks it asks for wait behind the GPU upload gate (B2), which is 1d's
work. So 1d measures the ring on and off once the upload share is in. If the ring still buys nothing, it is removed
before landing.

## What was checked

- **`plan_margin`:**
  - the ring is rank 3, after visible (0), the pan lookahead (1) and the neighbouring groups (2);
  - it is added only while `zoom_direction < 0` with an anchor, so the sort alone keeps pan from being starved;
  - `plan_jobs`' cutoff drops it first.
- **`ring_view_at`:**
  - it scales the view about the anchor by the group-to-coarser-group tile ratio over the margin;
  - it keeps the anchor's fractional position;
  - at the coarsest level it returns the view unchanged.
- **The zoom direction** is kept apart from `velocity()`, as `motion_centre` requires.
- **The starvation invariant test:** 60 rounds, 61 assertions. The test view was widened to 3x3 chunks, because a
  one-chunk view can never show ring-only content at this island's ratio (about 2.1x). The executor found this
  and reasoned it in the report.
- **Gates:**
  - render+app: 493 executed, 0 failed;
  - GPU on the 4090: 117/117;
  - WSL `-Werror`: clean apart from the known `extensions_test.cpp`.
- **Pan:** 113 frames over 2 ms, inside the unmodified binary's 100–127 (the 1c review voided the 30).

## ZOOM-SMOOTH-1d (released 2026-11-15, about 4 h)

**Branch:** `task/ZOOM-SMOOTH-1d`, from `task/ZOOM-SMOOTH-1c2` `619f88f6`. **Spec:** `tasks/zoom-smooth-1.md` step 5
and its step-9 tests, with the 2026-11-14 review's B2 and B3.

1. **First, check B3:** the priority inversion against the reaper, and the wgpu-core version point. Report both
   before changing the gate.
2. **Step 5:** keep a GPU upload share while chunks wait. This replaces the prefetch gate `!waiting && uploads == 0`
   in `gpu.cpp` (B2).
3. **A ring switch:** `bench_frame --zoom-ring on|off`, plus the viewer equivalent as a debug env var. Measure all four
   4090 modes and the iGPU wheel with the ring on and off.
   - If the ring still buys no readiness, remove it.
   - If it helps on the 4090 but costs the iGPU, report both; that trade is not yours to make.
4. **The packet's full target on `lan5`:** 0 on all four counters for zoom-out moves and returns, centre and off
   centre, reported per mode. Zoom-in is reported only.
