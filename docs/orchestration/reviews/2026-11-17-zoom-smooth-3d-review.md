# ZOOM-SMOOTH-3d: the orchestrator's review (2026-11-17)

`task/ZOOM-SMOOTH-3` `c552c193` (code `451e724a`, `a155e730`), Windows executor. CI green including `gate`
(run 43976954199, all 12 jobs). Files: `section_overlap.hpp` (new), `app_test.cpp`, `viewer.cpp`, `gpu.hpp`,
`gpu.cpp`, `gpu_fluid.hpp`, `gpu_fluid.cpp`. Measurement and diagnostics only.

## What was checked

- **F1 of the 3c review is fixed.** `GpuFrame::render` now takes the three sections' snapshot after `finish`
  returns (`gpu.cpp`, right after `total_ms`, with `sections_read_at_ns` read just after the snapshot); the old
  read before encoding is gone, not kept as a second one.
- **The ring** (`SectionRing`, `gpu_fluid.hpp`, 4 instances, the size a named constant with its reason): `begin`
  clears the slot's end before it publishes the new begin (release), and the reader loads begin then end
  (acquire). A reader that sees the new begin therefore sees end 0 or the new end, never the previous instance's.
  The one race left: a reader can load a slot's old begin and then the end that a new `begin` has just cleared,
  and would report the old instance as still running. That can only **add** an overlap, never hide one, so the
  run's "none" for every slow call is not weakened by it. The app layer's copy of the size is tied to the render
  layer's with a `static_assert`.
- **The overlap decision is a pure function** (`anno::app::overlap_ms`) with a read time for an instance still
  running. The test runs the four cases the packet named against it and against `overlap_ms_old`, which
  reproduces the old formula's blind spot (an end of 0 taken literally); the old one fails exactly the second and
  third case. The report says plainly that the old function reproduces the formula, not the old read-before-call
  timing. Accepted: the timing is fixed in `gpu.cpp`, the formula in the test.
- **The run**, once, alone (process list checked before and after): 63,204 frames; `finish` median 0.18 ms,
  99th percentile 0.64 ms, longest 6.90 ms; 9 calls over 2 ms, 2 over 4 ms (6.90 and 5.18). The packet's rule
  ("if no call exceeds 4 ms, run once more; then stop either way") made one run the end. Every one of the 9 shows
  no overlap with any of the three sections, now with a check that can see them; the worst five ran at 6 to 13 %
  of the cycle yardstick (4.68 to 4.69 million cycles per ms). `finish` costs 0.142 ms per 100 draws, as in 3c
  (0.141, 0.146); `record` stays flat.
- **The WSL check** found a real `-Wnull-dereference` (GCC, `-O3`) in the CPU-only viewer build: the slow-call
  block reads `counts`, which that build never sets. Moving the block behind `ANNO_VIEWER_GPU`, as
  `hitch_tracking_enabled` already is, is the right fix; the second run then found `describe_lookback` unused
  there and moved it too. Hosted CI did not catch either, because CI does not build the viewer without its GPU
  code; that is the warning script's job, now fixed by UNIX-WARN-1.

## Conclusion of the series

ZOOM-SMOOTH-3 (3a to 3d) set out to explain the slow `finish` calls. What is now shown by direct measurement:
- An ordinary `finish` call computes all the time, and its cost grows with the draws: about 0.14 ms per 100
  draws, which is the case for RENDER-DIRECT (fewer, larger draws).
- The rare long calls (4 over 4 ms in 189,478 frames over three runs, longest 13.6 ms) are **waiting, not
  computing**, and **no one of the three named background sections runs during them**. What they wait for, a lock
  in a thread not yet named or the OS scheduler, this method cannot tell apart; the report marks its candidates as
  guesses.

**Residual, recorded and closed:** about one call over 4 ms per 47,000 frames (at 500 Hz, about one in 95 s), a
wait whose owner is unknown. No further packet in this series. RENDER-DIRECT's step-6 rule ("no busy frame above
2 ms") gets this addition when its packet is written: a frame over 2 ms whose `finish` call runs below 20 % of
the cycle yardstick and overlaps no named section is counted separately as a wait, reported with its count, and
is not a busy frame. If such waits later grow, or the owner sees them while zooming, a scheduler-level trace
(Windows Performance Recorder, context switches of the window thread) is the next tool, not more stamps in our
code.

**The executor's question** (a search for a fourth thread or a scheduler trace now?): not now, for the reasons
above.

## Notes, no change needed

- `section_overlap.hpp` uses `std::size_t` without including `<cstddef>` (it arrives through `<array>`).
  Add the include at the next touch.
- Three of the five worst calls had heavy freeing and dropping in the look-back (129/29, 197/76, 89/64), two had
  none. Not a pattern by itself, but if the scheduler trace is ever run, the plane releases are the first thread to
  look at.

## Verdict

**ACCEPTED.** The ZOOM-SMOOTH-3 branch (`c552c193`, 3a to 3d) is ready for a landing candidate on the owner's OK.
It is based on `main` `0f46b1b2`, not on ZOOM-SMOOTH-2; both touch `viewer.cpp` and `gpu.cpp`, so the candidate
merge is checked for conflicts when it is built.
