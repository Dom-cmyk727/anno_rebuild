# ZOOM-SMOOTH-1c: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/ZOOM-SMOOTH-1c` `96c00844` (code `8f4b6ebe`), built on `task/ZOOM-SMOOTH-1b` `d71bbfbb`.
**CI:** run 43673519369, green, `gate` included.

## Verdict

**ACCEPTED as a partial: F1, F2, F3 and the pan question.** Step 4, the ring, was not started. It becomes its own
packet, **ZOOM-SMOOTH-1c2**, ahead of 1d (the orchestrator's decision under the packet-size rule; it answers the
executor's QUESTION). Now that the pan question is settled, 1b and 1c can land together.

## What was checked

- **F1** (`is_resident_level`):
  - the share is summed from the farthest level down to `g`;
  - levels under the `resident_level_chunks` floor don't draw on it;
  - the answer is monotone (a finer level fits only if every coarser one does).
  The new test covers two levels that each fit alone but not together. The per-call loop is over the levels only, so
  it costs nothing.
- **F2:** `system_ram_bytes()` uses `GlobalMemoryStatusEx` / `sysconf`, so `bench_frame --zoom-sweep` now sizes the
  budget like the viewer. The iGPU wheel rerun gave 12 frames over 2 ms, 3 non-ready, 0 stand-ins and 26 dissolves,
  better than both historical numbers.
- **F3:** `max_evict_bytes_per_frame` defaults to 64 chunk estimates. The reason is in the code (it bounds a single
  burst of discards, for example after the budget is lowered).
- **The pan "regression":**
  - The unmodified `task/ZOOM-SMOOTH-1` binary, rerun three times, gave 102, 127 and 105 frames over 2 ms on pan.
  - 1c gave 100, 102 and 106.
  - legs, wheel and zoompan reproduce their recorded baselines on that same binary.
  So pan's recorded 30 is not reproducible today; it is not a code regression. **The 1-baseline's pan busy figure is
  void.** Later packets compare pan's busy tail against the rerun (about 100 to 127), not against 30. The readiness
  counters are unaffected.
- `--resident-prewarm on|off` was added to `bench_frame` to isolate the prewarm. It is a measurement option only.

## ZOOM-SMOOTH-1c2, the ring (released 2026-11-15, about 4 h)

**Branch:** `task/ZOOM-SMOOTH-1c2`, from `task/ZOOM-SMOOTH-1c` `96c00844`. **Spec:** `tasks/zoom-smooth-1.md`, step 4
and its step-9 tests.

1. **The zoom-direction signal**, the first sub-step: FrameRenderer gets the zoom direction and the anchor, kept apart
   from the pan velocity (`motion_centre` leaves the anchor shift out on purpose).
2. **The ring's geometry:** a view scaled about the anchor within one zoom group (`zoomed_view_at` only jumps between
   groups).
3. **The ring:** its requests are ordered by time-to-visible and stay inside the budget.
4. **The invariant "pan is never starved",** as a test: the ring's requests come after every chunk of the visible view
   and of the pan lookahead.

**Measure** the four 4090 modes and the iGPU wheel against the 1c numbers. Stop and report if the 2 to 4 h box is
exceeded.
