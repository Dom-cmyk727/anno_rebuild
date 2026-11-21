# LAYER-CHECK-1: a test that keeps `formats`, `sim` and `sim_load` free of app, render, SDL and GPU code

**Status:** written and **RELEASED** 2026-11-17 by the Claude Code orchestrator for **the Windows executor**, from
the remaining-work assignment ([remaining-work-2026-11-16.md](../remaining-work-2026-11-16.md), Windows task 7). It
goes ahead of RENDER-DIRECT-1b, which waits for the owner's answer to OQ-096b.

**Base:** `origin/main` at the time of starting (name the commit). Branch `task/LAYER-CHECK-1`, a new worktree on
E:. About 1 to 2 hours. One concern: one test that fails when one of the three libraries reaches outside the layers
it may use. No change to any library's code unless the test finds a real violation (then see "Done when"). Never
rebase, force-push or push `main`. No reviewer subagents.

## Why

`CLAUDE.md` requires `sim` to build and run headless with no SDL or GPU dependency, and it requires the dependency
direction of the former crates. Today nothing checks either. Today's CMake links `anno_sim` to `anno_formats`
only and `anno_sim_load` to `anno_sim` only. A single include of an SDL, WebGPU, render or app header in one sim
file would build fine on a developer's machine and break that rule silently.

## Read first

- `CLAUDE.md` (Ground rules: determinism, separate libraries, the dependency direction);
  `docs/testing/evidence-contract.md`;
- `src/formats/CMakeLists.txt`, `src/sim/CMakeLists.txt`, `src/sim_load/CMakeLists.txt`, and how
  `tools/test/window_trace_columns_test.py` is registered as a CTest test (TRACE-COLS-1, on `task/ZOOM-BOUNCE-1`
  until it lands: copy the pattern, not the file).

## Do

1. **The rule, written once.** For each of `anno_formats`, `anno_sim` and `anno_sim_load`, state:
   - the headers a library source may include: the standard library, its own `anno/<lib>/...`, and the
     `anno/...` of the libraries below it;
   - the targets it may link.
   Test sources (`src/<lib>/tests/`) get their own allowance: doctest, `anno/test_support`, and for the sim tests
   `anno/sim_load`. Everything else is refused, including SDL, `webgpu`/`wgpu`, `anno/render`, `anno/app`,
   `anno/assets`, `anno/net` and `anno/ui`. The rule lives in one file that the test reads; the docs cite that file
   and do not copy it.
2. **The include check.** Scan every `#include` directive in the three libraries' source and header files, with
   `<...>` and `"..."` forms and spaces after `#`. Report a refused include with `file:line`, the header and the
   rule it breaks.
3. **The link check.** Read each target's link libraries from CMake itself, not from a hand-written copy of the
   graph. `file(GENERATE)` with `$<TARGET_PROPERTY:...,LINK_LIBRARIES>` at configure time is one way; name yours
   and why. Check them against the rule.
4. **Register it** as one CTest test (label `workflow`) that runs on Windows and in the Linux and macOS CI jobs.
   Python run with `-I`, as the other tool tests are.
5. **Show it can fail**, in the test itself and without touching the real tree:
   - a synthetic tree with an injected `#include <SDL3/SDL.h>` in a sim source must fail, naming the file and line;
   - a synthetic link list with `anno_render` under `anno_sim` must fail;
   - the real tree must pass.
6. **Docs:** a short section in `docs/testing/README.md` on what the test checks and where the rule file is.

## Done when

- The test passes on the real tree and fails on both synthetic violations, with messages that name the file, the
  line and the rule.
- It runs in CI on all jobs that run the other `workflow` tests; the report names the CI run.
- If the real tree breaks the rule today, the report names each violation. A one-line fix is made in the same
  branch. Anything larger stops with a QUESTION; the rule is never weakened to make the tree pass.

## Report

About 30 lines, READY FOR REVIEW: the rule as written, how the link list is read, the three outcomes of step 5,
any violation found, and the "Decisions and reasoning" list.

## Gates

The new test; `anno_sim_tests` and `anno_formats_tests` only if a fix touches their sources; clang-format 23.1.3
on touched C++. Tell the orchestrator before any test run on this PC that is longer than a minute.
