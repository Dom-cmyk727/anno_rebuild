# TRACE-COLS-1: the orchestrator's review (2026-11-17)

`task/ZOOM-BOUNCE-1` `bf5cc055` (code `f82bf42d`), Windows executor. Files: `tools/bench/window_table.py`,
`tools/bench/window_table_test.sh`, `tools/test/window_trace_columns_test.py` (new), `CMakeLists.txt`,
`docs/app/window-trace.md`. No C++ source touched.

## What was checked

- **One order.** The writer's order is kept (`resample_*` right after `scale`), as the packet preferred, because
  traces in that order already exist. `window_table.py` reads every row by name (`dict(zip(COLUMNS, f))`), so
  reordering `COLUMNS` changes nothing else in the reader. `WHEEL_COLUMNS` is new; `wheel.tsv` had no list on the
  reader's side.
- **The contract test, run by the orchestrator** on copies of the two sources: with the branch's
  `window_trace.cpp` and `window_table.py` it passes, 3 of 3; with `fc1fbff7`'s it fails (frames: order;
  wheel: no `WHEEL_COLUMNS`), sim passes, as the report says. It reads the header literals out of the C++
  source, not a third copy. It is registered as `window_trace_columns_contract`, label `workflow`.
- **`window_table_test.sh`** had the old order in its synthetic rows and in two `sed` patterns; both fixed. That
  test does not run on Windows (a known limit). It is CTest `window_table_contract` in the Linux and macOS jobs
  of CI, which therefore decide it.
- **A real trace** from the branch's binary (1,820 frames, 46 turns, the wheel header) reads without an error.
  It covers groups 2 to 4 only, so the switch between groups 1 and 2 is documented from the source (`kTile`,
  `ZoomLevels::with_derived`), not seen in a trace. That is enough for this packet.
- **`docs/app/window-trace.md`:** the columns in the writer's order, `wheel.tsv`'s eight columns with their
  meaning, and the tile widths per group (84, 42, 20, 10, 5 px; 42 to 20 is the one factor of 2.1).

## Note, no change needed

- The contract test finds a header by the text after `<<` up to the next `;`. A header split across two
  statements would be read in part and fail loudly, not pass wrongly, which is the right way round.

## Verdict

**ACCEPTED once CI is green on `bf5cc055`** (running at review time; its Linux and macOS jobs run
`window_table_contract`). Then the bounce branch (ZOOM-BOUNCE-1 to 3 and TRACE-COLS-1) is ready for a landing
candidate on the owner's OK. The assignment lands it before the UI lane binds keys in `viewer.cpp`.
