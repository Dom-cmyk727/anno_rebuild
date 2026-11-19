# FLAKE-RESIZE-1: the intermittent live-resize test failure

**Released 2026-11-17** by the orchestrator to the Windows executor. Branch `task/FLAKE-RESIZE-1`, base
origin/main. One concern, about 2-3 h.

## The failure

- `anno_app_input_tests`, case "the watch never reads its own shared guard for an event of another type -
  a second ..." (`src/app/tests/live_resize_test.cpp`, ZOOM-SMOOTH-2b), failed once in CI's
  `linux-clang-asan` job: run 44076929673, on UI-STATUS-1's merge, which does not touch the file.
- Read that run's failed log first (`gh run view 44076929673 --log-failed`): the assertion, its values and
  any sanitizer output.

## What to do

1. **Reproduce.** Run the case in a loop under the same conditions as CI where you can: the WSL Clang 18
   ASan/UBSan Debug build, and with other tests running alongside (ctest `-j` and `--repeat until-fail:N`).
   State how many iterations ran and how many failed.
2. **Find the cause**, not a retry. Likely suspects: a timing assumption, shared state between cases, an
   ordering that depends on the scheduler, or a data race (run it under TSan if ASan shows nothing).
3. **Fix it** so that the test checks the same contract deterministically. Do not loosen what it proves; if
   the product code has the race, fix the product code, and say so.
4. **Proof:** the loop that reproduced it now passes N times (N at least 10x the iterations needed to see it
   fail); a failure proof that the fixed test still catches the bug it was written for.
5. **If it cannot be reproduced** in about 1 h: record the loop counts, the log reading and the most likely
   cause in the report, and stop. The orchestrator decides the next step.

Second item, only if time remains: `viewer_wiring_r4_occupancy` was also seen to fail once on this PC
during a gate. Run it 50 times in a loop; report the count. Fix only if it fails.

## Acceptance

- The changed lanes (app input, live resize, wiring) on msvc and the WSL ASan build; `check_unix_warnings.sh`;
  CI green on the exact head.
- Report at the bottom of REPORTS.md with the loop counts, the cause (with evidence) and "Decisions and
  reasoning".
