# COORD-FLAKE-1: the two test-coordinator flakes, cause first

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator, under the owner's standing go for the Windows lane, to
**the Windows executor**. Step 3a of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/task/GATE-1c` at `9a958dc1` (accepted), branch `task/COORD-FLAKE-1`, in a new worktree. About 2–3 hours. GATE-1c lands on `main` with the owner's OK; once it has, merge `origin/main` in. Never
rebase or force-push.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover), one REPORTS entry of about 60 lines, READY FOR REVIEW. A flake is closed by its mechanism, never by
reruns.

## Why

Two coordinator tests fail in hosted CI now and then, and each failure costs a rerun and a check per landing
([project analysis](../reviews/2026-11-12-project-analysis.md) D5;
[GATE-1a review](../reviews/2026-11-12-gate-1a-review.md), Notes):

- **`F3 (D1, deaf)`** (`src/test_support/tests/budget_f3_test.cpp`, about line 521), on **macOS**. It ends
  `INCOMPLETE` (the runner found no status for the case, `tools/test/ctest_verdict.py:143`), with no failed
  assertion. Runs 43123068877 and 43082536481, job `cpp (macos-clang)`. Their artifacts `test-evidence-macos-clang`
  still exist. The analysis counts five such failures on 2026-11-11 and 12.
- **`C3: a replacement coordinator does not admit against its predecessor's promises`**
  (`src/test_support/tests/budget_f2_test.cpp`, about line 453), on **Windows**. Run 43218940794, attempt 1, job
  `cpp (windows-msvc)`. Its artifact `test-evidence-windows-msvc` from that attempt still exists (the run has two).

## Do

1. **Evidence first.** Download the artifacts (`gh run download <id> -n <name>`), and find each case's own output: its
   last lines, which `want`/`line_with` deadline or `CHECK` failed, and for `deaf` why the case has no status. A test
   process killed at a ctest timeout, a crash, or output cut off are different causes. Search the other CI runs since
   2026-11-10 for more occurrences, and say how many you found.
2. **Name the mechanism** for each test, with that evidence and a reading of the code it exercises (`coord.cpp`,
   `coord_server.cpp`, `budget.cpp`, the probe `budget_probe.cpp`). Then reproduce it locally under a stated condition
   (CPU load beside the case, a `-O0` build, a short `--repeat until-fail`). If a case cannot be reproduced, say so and
   keep its fix to what the evidence proves.
3. **Fix the cause.** If it is in the test (a deadline shorter than the work it waits for on a slow runner, a race
   in the test's own setup), fix the test's logic, not just a number. A larger deadline is acceptable only where
   the evidence shows the work legitimately takes that long, and the report gives the measured time. If it is in the
   coordinator or the client, fix the product code and keep the wire and `kCompatVersion` unchanged unless the fix
   needs otherwise (then stop with a QUESTION).
4. **Proof:** each fixed case passes repeatedly under the condition that made it fail (state the condition and the
   count), and the mutation that recreates the cause still fails it.

## Proof

- The evidence, the mechanism and the repeat counts above.
- The coordinator tests (`ctest -R anno_test_support_tests`, or the canonical runner selecting that suite) on MSVC
  Release and in the WSL `-O1` sanitizer build: classified counts, `sanitizer_lines` 0.
- **Hosted CI green** on the head, `gate` included, macOS jobs among them. Read CI after every push. A macOS job that
  ends with no steps (no runner) is infrastructure: rerun it alone and say so.

**Not yours:** the full WSL gate and the MSVC game-file run (the orchestrator, at the merge); TEST-MEM-1.

## Report

READY FOR REVIEW, about 60 lines: per test, the evidence, the mechanism, the fix, the repeat counts; the gates and CI;
NOT_RUN; Decisions and reasoning; QUESTIONs.
