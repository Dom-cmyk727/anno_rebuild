# W4-NEXT-1 acceptance review (2026-11-10)

**Verdict: ACCEPTED.** Both branches are merged into `main` (`fc126fbe`, `2c4dc295`). Five findings, none
blocking; three of them are carried into W4-NEXT-2.

**Reviewer:** the Claude Code orchestrator (Opus 5.5). This review follows the
[interim review](2026-11-10-w4-next-1-interim.md).

**Reviewed heads:** both branches start from base `bd760031`.

| Branch | Head | Parts |
|---|---|---|
| `task/W4-coord-1` | `2cf6f4c2` | C |
| `task/W4-ui-3` | `c7ad492f` | P, S, U, E |

**Read:** both branches' REPORTS entries, the fix commits `ee504b75`, `d163f806` and `deef348a`, the C-2 change in
`coord.cpp`, and the CI runs listed below. The packet changes no file under `src/sim`, `src/render`, `src/formats`
or `src/assets` (checked on the merge diff).

## The interim review's nine findings

| # | Finding | Disposition | How it was checked |
|---|---|---|---|
| 1 | `--view` lost S/D/Q | **Fixed** (`d163f806`). The rule is now `cli.hpp::classic_key_surface(has_play_hooks, ui)`, and the window calls it. Its test has 4 checks, 2 of which fail against the old `hooks == nullptr` shape. | Read the diff and the test. CI on `d163f806` (run 42866827783) is green, all jobs. |
| 2 | `F3 (D1, deaf)` died with SIGPIPE on macOS | **Fixed** (`ee504b75`): `deaf_listener` now calls `startup_sockets()`, as the real coordinator and client paths do. | `cpp (macos-clang)` passed on 3 pushes since the fix: runs 42863102776, 42866978698, 42882232866. The repeated-run proof on a POSIX host was not run. |
| 3 | C-2's link to the recorded stall | **UNPROVEN, reported as such.** The journal is balanced, and the snapshot is 22 min into a 3,600 s bound, before any cancel. The analysis is in `docs/perf/README.md`. | Accepted as the review allowed: a verified mitigation, not a proven cause. |
| 4 | F4 (N3) and H1 log what they received | **Done.** F4 (N3) already logged it. H1 now logs through `asks(..., &seen)`. | Read the diff. |
| 5 | C-3 | **NOT_RUN by design.** Assigned to W4-NEXT-2 part M. | — |
| 6 | Rows 5 and 10 lacked checks | **CPU done:** `ui_dynamic_draw_check` gives an exact match, and its wrong-input control is rejected on both rows. `bench_frame` is a general regression run only. **GPU not run** (finding A). | Read the report and the CMake registration. The check skips without the game files (exit code 77). |
| 7 | Row 19 not drawn | **Not drawn, reported as such.** Waits on OQ-565. | — |
| 8 | Row 18's premise was wrong | **Corrected** in a new REPORTS entry that cites the existing click paths. Row 18 is W4-NEXT-2 part K. | Read the entry. |
| 9 | The port-band comment and the evidence location | **Fixed:** the comment is corrected, and the game-file run's logs are copied to `cache\w4-next-1\coord-gamefile-run\`. | Read the report. |

## CI, checked here

Part C asked for five consecutive clean runs on `cpp (linux-gcc)`, `cpp (linux-gcc-release)` and
`cpp (linux-clang-asan)`, with re-runs not counting. All five passed the three jobs on their first attempt:

| # | Run | Commit |
|---|---|---|
| 1 | 42816371389 | `511ca98e` |
| 2 | 42816475646 | `810b7f38` |
| 3 | 42820583088 | `3be3aaae` |
| 4 | 42848576936 | `60955b0c` |
| 5 | 42852934257 | `30986aac` |

- **Run 3 has a re-run.** Its attempt 1 was fully green. A later re-run, attempt 2, is the macOS SIGPIPE failure of
  finding 2.
- **Runs 1 and 2 failed `cpp-format`.** That job is outside the criterion; it was fixed in `3be3aaae`.
- **coord-1's final head** `2cf6f4c2` (run 42882232866): all jobs green.
- **ui-3's final head** `c7ad492f` (run 42882369060): `cpp (linux-gcc)` and `cpp (linux-clang-asan)` failed, both on
  `anno_test_support_tests.V3`. That is the coordinator flake: this branch lacks C-1. See finding B.

## Findings

| | Severity | Finding | Action |
|---|---|---|---|
| A | Medium | **The GPU pixel check for rows 5 and 10 was not run.** W4-UI-2's V4 shows how: run the draw-check script with `--renderer gpu` on a real window and compare it to the same oracle. The report calls the GPU check manual-only, which is true, but does not say it was not done. | W4-NEXT-2 part P runs it. |
| B | Low | **ui-3's final CI run is missing from its report's table.** It was red on V3, the known flake. | The gate on the merge commit is the check (below). |
| C | Low | **"2,359/2,359 passed" is repeated in the final report.** Classified, that run is 2,359 selected, 2,347 executed and passed, 0 failed, 12 skipped (interim review). | W4-NEXT-2 reports classified counts only. |
| D | Process | **An independent review missed a defect it could have caught by running the test.** The Part S fix `a9628bd0` used `look`, which pans the camera, so its negative control could never fail. The review read the script but did not run it; only part E's canonical gate, which ran it with the game files, found it. | W4-NEXT-2 rule: a reviewer runs every game-file-gated test it accepts, or marks it NOT_RUN. |
| E | Low | **The final mirror sync** after `c8cfd41a` was owed. | `tools/orch/land.py` syncs on landing. |

## Merge

- **`fc126fbe`** merges `task/W4-coord-1`, `--no-ff`.
- **`2c4dc295`** merges `task/W4-ui-3`, `--no-ff`. The only conflict was `REPORTS.md`, where both branches appended.
  Both blocks are kept, coord-1 first. ui-3's Part P entry is byte-identical to coord-1's and is kept once.
- **Where:** on `main` in the orchestrator's checkout, not in an executor worktree.

## Gates at the merge commit `2c4dc295`

- **CI `gate` (run 42907360973): green, all 12 jobs.** That includes the three Linux jobs that failed on the V3 flake
  at ui-3's head, now that C-1 is in. Landed with `tools/orch/land.py`.
- **Game-file run (owner rule of 2026-11-07): PASSED, before the push.**
  - **Setup:** MSVC Release with the game files and the RTX 4090 (`ANNO_GPU_ADAPTER=4090`, `ANNO_TEST_THREADS=8`,
    `ctest -j 8`). Detached, in `E:\anno1503-worktrees\anno1503-main-gamefile-run` at `2c4dc295`, clean tree.
  - **Time:** 22:42 to 22:56, 800 s real. No stall. The coordinator spoke compatibility version 4.
  - **Counts, classified by `tools/test/ctest_verdict.py`:**

    | Selected | Executed and passed | Failed | Skipped | Required-asset skips |
    |---|---|---|---|---|
    | 2,367 | 2,355 | 0 | 12 | 0 |

    The 12 skips: 6 opt-in, 3 platform, and 3 symlink rows that need a Windows privilege this host lacks.
  - **The verdict tool says FAIL**, only because the run declared no policy for that last skip class; the earlier runs
    recorded it the same way.
  - **`LastTestsFailed.log` in the build tree is stale** (2026-11-09 05:27, from the `96de6bca` run). This run failed
    nothing.
  - **Logs:** `cache\main-gamefile-2c4dc295\run\`.

**`main` at `2c4dc295` is healthy** under the owner rule: CI green, and the game-file run passed.

## Carried into W4-NEXT-2

- **Part P:**
  - finding A, the GPU check for rows 5 and 10;
  - finding D's rule.
- **Part K:** row 18.
- **Part W:**
  - rows 19 and 2;
  - OQ-565, OQ-566 and OQ-567.
- **Part M:** C-3.
- **Optional:** the `deaf` test's repeated run on a POSIX host (`ctest -R deaf --repeat until-fail:50`), if it ever
  flakes.
