# GATE-2a: a refused test is counted as refused and retried alone

**Status:** QUEUED 2026-11-13 by the Claude Code orchestrator for **the Windows executor**, behind
[SHIP-ORDER-1](ship-order-1.md), a bug the owner found in that day's playtest, the zoom measurement and quick
fixes (ZOOM-MEASURE-1, ZOOM-SMOOTH-1), and UI-WINDOW-1 (owner, 2026-11-13; UI-WINDOW-1 moved to the UI lane on
2026-11-14, so GATE-2a no longer waits for it). It is released after those, under
the standing go for the Windows lane. The owner gave the go on the rerun plan with "Yes, all of it". Step 4a of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` at release, branch `task/GATE-2a`, in a new worktree on `E:`. About 2–3 hours. Never rebase or
force-push.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

Gates follow `TASKS.md` §1 item 4 as rewritten on 2026-11-13: this packet changes `tools/test`.

## Why

A test the memory coordinator refuses never runs its own work. It throws
`[anno-test memory] refused estimate=… reason=low-memory stop` (or `wait deadline`) from `reserve_bytes`
(`src/test_support/src/budget.cpp`), and today the gate counts that as `FAILED`, exit 1. So a run that only had
refusals looks red, and executors rerun whole gates to clear it.

The same group of corpus tests is refused in most full runs:
- 13 and 4 refusals on 2026-11-07 ([W4-MAIN-2 review](../reviews/2026-11-07-w4-main-2.md));
- 9 on 11-09 ([W4-UI-2 merge](../reviews/2026-11-08-w4-ui-2-merge.md));
- 7 and 2 on 11-12 ([MAP-INTEGRATE-2](../reviews/2026-11-12-map-integrate-2-review.md),
  [GATE-1b](../reviews/2026-11-12-gate-1b-review.md));
- on 2026-11-13: 6 in the orchestrator's MSVC run ([COORD-FLAKE-1 review](../reviews/2026-11-13-coord-flake-1-review.md),
  "Game-file run") and 3 in PLAY-LIVE-1a's WSL run. That WSL run was then repeated in full on the same commit and
  stopped.

Run alone, they pass: all 6 of 2026-11-13 did, in 7.5 minutes. The owner rule of 2026-11-13 (`TASKS.md` §1 item 4,
`docs/testing/README.md`, "What a packet runs, and refused tests") says a refused test is "refused, not run", and is
rerun alone. This packet makes the tools do that, so nobody has to decide it by hand.

**Not this packet:**
- why tests are refused, and whether the 2 GB stop should wait instead of refuse (TEST-MEM-1, next after GATE-2b);
- `wsl_gate.sh` options (GATE-2b);
- any change to `src/test_support`.

## Do

1. **`tools/test/ctest_verdict.py`: a `refused` class.**
   - **Which tests.** A test whose failure is a refusal is `refused`, not `failed`. Find every refusal text the
     test-support library throws or prints (`reserve_bytes` in `budget.cpp`, and any CPU-budget or coordinator
     equivalent); list them in the report and match those only.
   - **A real failure wins.** A test that also has a failed `CHECK`/`REQUIRE`, a crash, a timeout or a sanitizer line
     stays `failed`. A refusal never hides a defect.
   - **The output:**
     - a `REFUSED: <name>` row;
     - `refused=<n>` in `COUNTS`;
     - a verdict `VERDICT: FAIL refused tests (not run)` with a new exit code **10**.
   - **Precedence:** a failed test (1) and a sanitizer report (4) come before a refusal (10). Keep the order of the
     other codes.
   - **Old logs.** `recorded_verdict` re-reading an old canonical log must still not erase its recorded result; it
     may add the `REFUSED` rows.
2. **`tools/test/run_selected.py`: retry refused tests alone, on by default.**
   - **When.** After the selection's run, run each refused test again, one at a time, in name order:
     `-R <anchored name> -j 1`, under a `== retry refused <name>` header, each with its own verdict row, as the
     required-contract runs already do.
   - **The first run stays in the log, unchanged** (the evidence contract: initial failures and reruns stay
     separate).
   - **The final rows:**
     - `COUNTS (after retry)`;
     - the final `VERDICT`, where a test whose retry passed counts as executed and is listed as
       `RETRIED: <name> passed`;
     - a test refused again stays refused (exit 10);
     - a retry that fails is `failed` (exit 1).
   - **`--no-retry-refused`** turns it off.
   - **The summary line.** `tools/test/wsl_gate.sh` greps `COUNTS|VERDICT` from each lane's output. Make it record the
     final counts and verdict, not the first ones.
3. **Contracts in `tools/test/workflow_test.py`**, in its compiler-free fake CTest project:
   - a refused-only test gives `REFUSED` and exit 10 with `--no-retry-refused`;
   - by default, a retry that passes gives `PASS`, with the first refusal and the retry both in the log;
   - a test refused again gives exit 10;
   - a test with a real failed assertion and a refusal line gives `FAILED` and exit 1;
   - every existing contract still passes.
4. **Docs.** In `docs/testing/README.md`:
   - exit code 10 in the list of exit codes;
   - one sentence in "What a packet runs, and refused tests": the runner now retries refused tests itself.

   `TASKS.md` §1 item 4 is the orchestrator's; don't edit it.

## Proof

- **`python -I tools/test/workflow_test.py`:** all pass, with the count.
- **Real logs, re-read without running anything.** Run `python -I tools/test/ctest_verdict.py <log> --inventory
  <log>.inventory.json` on both logs, and give the `REFUSED` rows and the `COUNTS` line of each. Expected: 6 refused, 0
  failed, then 3 refused, 0 failed. Any other result is a finding: report it, don't adjust it away.
  - `E:\anno1503-worktrees\anno1503-main-gamefile-run\cache\orch-coord-flake-1\canonical-gate.log`: the
    orchestrator's MSVC run of 2026-11-13;
  - `~/anno-gate-logs/anno1503-PLAY-LIVE-1a-gatecheck/wsl-play-live-1a-sim.log` in WSL: the sim lane of
    PLAY-LIVE-1a's first run.
- **One live refusal on MSVC Release.** Run one game-file corpus case with `ANNO_TEST_STOP_MB` set far above the
  machine's free memory, so it is refused, and show the retry refused again, giving exit 10. Then run the same case
  without it, and show `PASS` with no retry section.
- **Hosted CI green** on the head, `gate` included. Read CI after every push.

**Not yours:**
- the full WSL gate and the MSVC game-file run (the orchestrator, at the merge);
- GATE-2b;
- TEST-MEM-1.

## Report

READY FOR REVIEW, about 60 lines:
- the refusal texts matched, each with its source line;
- the verdict and retry behaviour;
- the contracts;
- the real-log and live results;
- CI;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
