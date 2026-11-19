# UNIX-WARN-1: the Linux warning check ends green again

**Status:** written 2026-11-16 by the Claude Code orchestrator for **the Windows executor**. Queued after
ZOOM-SMOOTH-3c; released by the standing go when it is reached.

**Base:** `origin/main` at the time of starting. Branch `task/UNIX-WARN-1`, a new worktree on `E:`. About 1 hour.
One concern. Never rebase, force-push or push `main`. No reviewer subagents.

## Why

`tools/check_unix_warnings.sh` fails on `src/sim/tests/extensions_test.cpp`: the script does not pass the define
`ANNO_EXTENSION_ORACLE_FILE` that CMake passes (`src/sim/CMakeLists.txt:53`). Eight reports since 2026-11-14 call
this "the known gap" and go on. A check that is always red hides the next real warning.

## Do

1. Make the script compile that file as CMake does. Prefer one place that both read (for example a small list of
   per-file defines the script parses out of the CMake file, or a file both include) over a second hand-written
   copy; say which you chose and why.
2. Give the script a negative control it runs itself: a scratch file with a deliberate warning (a shadowed
   variable) must make it fail, and the script reports that the control failed as expected.
3. Note the fix in `docs/testing/README.md`, in the section that names the script.

## Done when

- The script exits 0 on the branch under GCC 13 and Clang 18 in WSL, viewer and GPU code on, and once without
  `ANNO_VIEWER_GPU`, as before.
- The negative control makes it exit non-zero when the script's handling of it is removed (show both runs).
- No source file under `src/` changes.

## Report and gates

About 30 lines, READY FOR REVIEW, with both runs' last lines. Gates: the script itself; clang-format is not
involved. Do not run WSL while a gate runs on this PC (ask if unsure).
