# ZOOM-SMOOTH-3c: the orchestrator's review (2026-11-16)

`task/ZOOM-SMOOTH-3` `7bec1780` (code `cc643706`, `08b683c7`), Windows executor. CI green including `gate` on both
commits. Measurement and diagnostics: `gpu.hpp`, `gpu.cpp`, `gpu_fluid.hpp`, `gpu_fluid.cpp`,
`thread_priority.hpp`, `viewer.cpp`.

## Accepted

- **The spikes were reproduced** in the session that showed them (five locations, lan5, enhanced water, the
  maximised window), twice, 63,111 and 63,163 frames. `finish`: median 0.18 ms, 99th percentile 0.67 and 0.64 ms,
  longest 13.63 and 3.77 ms; 2 calls over 4 ms in all (13.63, 4.84), 28 over 2 ms. Nothing else ran during either
  capture by the process list before and after (the owner's UI test viewer was waited out).
- **The cycle yardstick works.** The median of the ordinary calls is 4.68 to 4.69 million cycles per ms in both
  runs, which is the CPU's clock: ordinary `finish` calls compute the whole time. The five worst calls ran at 3 to
  15 % of it. **They were not computing.** That is established.
- **The line for RENDER-DIRECT:** `finish` costs 0.146 and 0.141 ms per 100 draws (intercept about 0.07 ms) in the
  two runs; `record` does not grow with the draws. At the 689 draws 3b saw at maximum zoom-out that is about
  1.1 ms of the 2 ms frame at 500 Hz. This answers the report's question: it supports RENDER-DIRECT's priority,
  because fewer, larger draws cut it directly.
- **F3 of the 3b review is fixed** (the CPU measure runs only with `ANNO_VIEW_HITCH_MS` set), and the WSL check
  found and fixed a real unused-function error in the CPU-only build.

## Finding

- **F1, the overlap check cannot see the case it is for.** `GpuFrame::render` reads the three sections' begin and
  end stamps (`gpu.cpp` lines 3403 to 3408 on the branch) about 400 lines **before** the frame's `finish` call
  (line 3792). A section that starts while `finish` runs is therefore not in the snapshot. A section still running
  at the snapshot has a new begin and the previous run's end, and `overlap_ms` (`viewer.cpp` around line 3534)
  then computes `min(finish_end, old_end) <= max(finish_at, new_begin)` and reports no overlap. Only one instance
  of each section is kept. So "no overlap with any of the three named sections, by direct measurement" is not
  shown, and with it the step from "not computing" to "descheduled". **What the data shows: the worst calls were
  waiting, for a lock or for the scheduler; behind which, if any, of the three sections is open.**
- **Note:** the orchestrator's reviewer subagent may have been running during a capture, as the report says
  honestly; one light thread on 16 is not a likely cause of a 13 ms wait, but the repeat in 3d runs alone.

## Verdict

**ACCEPTED as measurement and diagnostics, with F1 fixed in [ZOOM-SMOOTH-3d](../tasks/zoom-smooth-3.md#zoom-smooth-3d-the-overlap-check-reads-after-the-call)**
(about 1 hour) before the branch lands. 3d is the last packet of this series: whatever its run shows, the residual
(2 calls over 4 ms in 126,274 frames) is then recorded with its classification and closed. RENDER-DIRECT step 6's
acceptance ("no busy frame above 2 ms") will need a rule for a frame whose only cost is such a wait; that is
written into its packet, from 3d's result.
