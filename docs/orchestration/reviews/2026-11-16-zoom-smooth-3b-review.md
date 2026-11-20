# ZOOM-SMOOTH-3b: the orchestrator's review (2026-11-16)

`task/ZOOM-SMOOTH-3` `73f06bd2` (code `5d0e71a1`), Windows executor. CI green including `gate` (run 43935275085).
Diagnostics only, as asked: `gpu.hpp`, `gpu.cpp`, `thread_priority.hpp`, `viewer.cpp`.

## Accepted

- **The counts (items 1 and 2).** The hitch line prints chunks built, missing and drawn under their real names;
  `GpuStats` has `chunks_visible`, `draw_calls` and `bind_group_sets`, counted where the passes are recorded.
- **The ordinary cost of `finish` grows with the draws.** Over 3,673 live and 3,728 control frames at maximum
  zoom-out (109 to 689 draws a frame) the correlation is 0.85 and 0.77. `finish`: median 0.32 and 0.46 ms, 99th
  percentile 1.16 and 1.19 ms, longest 2.32 and 1.72 ms. This is the first measured reason for RENDER-DIRECT in the
  budget: at 500 Hz the frame has 2 ms, and one step takes over 1 ms of it at the 99th percentile.
- **The look-back (item 4)** is in place; no slow call occurred to exercise it.
- **The threads that call wgpu off the window thread (item 6), from the code:** the plane pre-creation thread
  (`gpu.cpp` `start_prewarm`: textures and views), the fluid job thread (`gpu_fluid.cpp` `prepare_window` through
  `make_grid`: textures, views, a bind group), and the fluid job's background release
  (`free_job_in_background`: releases a grid's texture, view and bind group). The chunk builders, the reaper and the
  scene reclaimer call no wgpu function. This is the most useful result of the packet: none of the six counts of
  ZOOM-SMOOTH-3 sees a new fluid window or a pre-created plane, and both take the device allocator's lock.

## Findings

- **F1, the spikes were not looked for where they were found.** ZOOM-SMOOTH-3 saw the 4.4 to 10.8 ms calls in its
  live session over five locations and in two runs. 3b measured two new sessions of 5 and 8 s at one place. "Not
  reproduced" therefore says little. The same session has to be run again with the new counts.
- **F2, the CPU-time question is still open, and the tool for it was set aside for a weak reason.** `GetThreadTimes`
  moves in steps of 15.6 ms, so it cannot say anything about a 5 ms call; reported honestly. The packet named
  `QueryThreadCycleTime` first. It was rejected because cycles cannot be converted to time reliably. No conversion
  is needed: the same call's cycles per millisecond in ordinary frames of the same run are the yardstick. A slow
  call with far fewer cycles per millisecond was waiting; one with the same was computing.
- **F3, a useless system call on every frame.** `thread_cpu_time_ms()` runs twice per frame in `submit`, whether or
  not the hitch line is on, and on Windows its result is known to be meaningless. The packet said "behind the
  existing hitch switch". Fix before this branch lands.
- **F4, a correlation without its line.** The slope and intercept (milliseconds per 100 draws) are what
  RENDER-DIRECT needs to predict its gain. They are in the logs and not in the report.
- **F5, measurements and tests ran at the same time.** Eight tests failed "under memory contention from my own
  concurrent capture". A capture that disturbs the tests is also disturbed by them. Say for each distribution
  whether anything else was running, and do not run both at once.

## Verdict

**ACCEPTED as diagnostics** (items 1, 2, 4 and 6). **Items 3 and 5 are not answered**; they go to
[ZOOM-SMOOTH-3c](../tasks/zoom-smooth-3.md#zoom-smooth-3c-catch-a-slow-finish-in-the-act), after ZOOM-SMOOTH-2f.
The branch lands after 3c (F3 is fixed there). The report's questions: the background-thread log is worth building,
because the packet for it is small and RENDER-DIRECT's packets are not written yet; `kSlowFinishMs` stays 2.0 until
the contract script encodes the rule.
