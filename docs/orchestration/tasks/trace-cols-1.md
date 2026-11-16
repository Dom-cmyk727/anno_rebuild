# TRACE-COLS-1: the window trace and its reader agree on the columns

**Status:** written 2026-11-16 by the Claude Code orchestrator for **the Windows executor**. Queued after
UNIX-WARN-1; released by the standing go when it is reached. **RELEASED** 2026-11-17, after the review of
ZOOM-SMOOTH-3d.

**Base:** `task/ZOOM-BOUNCE-1` `fc1fbff7`, same branch and worktree, new commits (the fix lands with the bounce
packets). About 1 hour. One concern. Never rebase, force-push or push `main`. No reviewer subagents.

## Why

ZOOM-BOUNCE-2 added three columns to `frames.tsv`. `src/app/src/window_trace.cpp` writes `resample_scale`,
`resample_offset_x` and `resample_offset_y` right after `scale`. `tools/bench/window_table.py` lists them at the end
of `COLUMNS` and refuses a file whose header differs (`lines[0].split("\t") != COLUMNS`). The orchestrator's trace
of the owner's wheel test (2026-11-16, this branch's binary) has the writer's order, so the reader refuses every
trace this branch produces. The diagnostics the bounce packets added cannot be read by the project's own tool.

Also for the document: the zoom groups' tile widths are 84, 42, 20, 10 and 5 px, so groups 1 and 2 differ by 2.1,
not 2. A reader that assumes a factor of 2 between groups reports a false jump at every switch between them.

## Do

1. One order, in both files. Keep the writer's order (traces already exist in it) unless you find a reason not to;
   say which you chose.
2. A contract test in `tools/test` that takes the header line the C++ writes (from the source, or from a trace a
   test produces) and feeds it to the Python reader, for `frames.tsv`, `sim.tsv` and the new `wheel.tsv`. Register
   it with CTest.
3. `docs/app/window-trace.md`: the three columns, `wheel.tsv`'s columns, and the tile widths per group.

## Done when

- The new contract test fails on `fc1fbff7` as it is and passes after the fix (show both).
- `window_table.py` reads one real trace of a scripted zoom from this branch's binary without an error; its output
  is in the report.

## Report and gates

About 30 lines, READY FOR REVIEW. Gates: `anno_app_tests`, `viewer_wiring_*`, the workflow tests of `tools/test`
that touch the reader, clang-format 23.1.3, the WSL `-Werror` check on the touched C++ files.
