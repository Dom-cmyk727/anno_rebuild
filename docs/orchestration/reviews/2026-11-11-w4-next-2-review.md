# W4-NEXT-2 review (2026-11-11)

**Verdict: CHANGES REQUESTED.** `task/W4-ui-4` is not accepted: one P1, a test that races the sim, and two P2s.
`task/W4-shared-list-1` is accepted with findings. Its OQ-571 text misstates what was measured, so it is fixed in the
same round, and both branches land together afterwards. The fix round is part F of the
[packet](../tasks/w4-next-2.md).

**Reviewers:**
- **The Claude Code orchestrator (Opus 5.5):** the reports, docs, evidence and CI, and the findings below.
- **An independent read-only code reviewer** (a fresh agent with no context from the executor): the code of both
  branches. It ran nothing. Its verdict: REJECT for `task/W4-ui-4` until the P1 is fixed, ACCEPT-WITH-FINDINGS for
  `task/W4-shared-list-1`.
- **The orchestrator checked every finding marked "verified" against the code itself.**

**Reviewed heads:**

| Branch | Head | Base | Parts |
|---|---|---|---|
| `task/W4-ui-4` | `f1f772b4` | `6b077529` | P, K, W, M, E |
| `task/W4-shared-list-1` | `593cdd3f` | `0144fe85` | X |

**What changes on the merge:** 28 files. No file under `src/sim`, `src/render`, `src/formats` or `src/assets`
changes (checked on the merge diff).

## What holds

- **P:**
  - The re-review of `3f3dd4d8` and `8b807827` is sound.
  - The `stop()` question is raised as OQ-568, with no behaviour change.
  - `peak_launcher_contract` runs and passes in an MSVC build.
  - P5, the GPU check for rows 5 and 10, which W4-NEXT-1 left NOT_RUN: exact match on both rows, and both
    wrong-input controls are rejected (514 of 2,809 and 5,279 of 8,492 pixels differ).
- **K:**
  - The selection is window state only: the sim thread reads the World in `fill_inspection` and
    `answer_inspection_while_paused`, and changes no state, hash or command.
  - CPU and GPU agree: 0 of 78,400 pixels differ.
  - The UI.1 oracle still matches: 9 of 9 screens, and 3 of 3 live screens.
  - The session's fresh-process replay agrees, and its mutated recording disagrees at the right turn.
  - The pixel check's wrong-input comparisons can fail (code reviewer).
- **M:**
  - The launcher is off by default. With `ANNO_TEST_PEAK_MEMORY` unset, every test is registered as before.
  - On Windows, every handle is closed on every path, and exit codes pass through, crash codes included.
  - On Linux, `wait4` and `PDEATHSIG` are used, and a fatal signal is re-raised.
  - The numbers are real, from a full game-file run:
    - 27 tests above 1 GiB, 18 of them with no reservation;
    - the largest is 8,715 MiB, with no reservation;
    - the worst sum of unreserved peaks under `-j 8` is 18,281 MiB.
- **X:**
  - The five scenarios are identified by size and SHA-256.
  - Four refuse to load, each with its named reason.
  - The tool's tests can fail, and they skip correctly without the game files.

## Findings

| # | Severity | Finding | Verified |
|---|---|---|---|
| 1 | **P1** | **The selected-house session races the sim.** `selected_house_draw_check.script` does `click`, `wait frames 5`, then `expect title selected House` and `screenshot`. The selection resolves only when the sim publishes an answer, so a starved sim thread under `ctest -j 16` fails the check. Also, wait, expect and screenshot run in one `script_tick`, and the capture writes the previous frame's buffer, so a passing title can come with a screenshot without the panel. The pixel check gives the script 30 s; `viewer_wiring.py` gives it 240 s. Three gate tests run this script. The executor's own full run saw `selected_house_pixel_check` fail once under `-j 16`. | Script lines 31–34 and 59–61; `viewer.cpp` run loop (answer, then `script_tick`, then draw); `selected_house_pixel_check.py:133`. |
| 2 | **P2** | **A click on the drawn HUD selects the world behind it.** `selection.hpp` says a selection click is "not on a UI rectangle", but `selection_handle` picks the tile under any pixel through `DevTool::pick`. With a house selected, a click on its need icon at (794, 480) deselects it or selects a hidden building. | `selection_handle` has no hit test. |
| 3 | **P2** | **OQ-571's "no growth" is guaranteed by the rules it ran with.** `cmd_population` uses a default `SimConfig`, where `Rules::house_updates` is false. `config.hpp` says that setting "keeps the dispatch an exact no-op". None of OQ-571's three explanations names it. The OQ also says the invariants were "confirmed by cmd_scenario/cmd_population both", but `cmd_population` never calls `check_invariants`. | `config.hpp:675`; `cmd_population`. |
| 4 | P2 | **`population` steps 100 ms; the play mode steps 10 ms at 100 %** (`multiplayer_step_ms(100) == 10`, `sim_test.cpp:159`). The island sweeps take the step as input, so "normal speed" is not what was measured. The tick arithmetic itself is right. | `sim.cpp:146`; `island_tick.cpp`, `sweep_chunks`. |
| 5 | P3 | `--seconds 60,10` prints `t=10s` at the 60-second tick: the list is not checked for order. | Code reviewer. |
| 6 | P3 | **The demolish-while-selected step doesn't test the drop.** The developer tool is on at that point, so `selection_refresh` returns early and the title passes because the tool's hover inspection is empty. The comment in `src/app/CMakeLists.txt` claims end-to-end coverage that only the unit test gives. | `selection_refresh` returns early while the tool is on. |
| 7 | P3 | **An inspection answer is not tied to its request.** The snapshot echoes only the tile. An answer to an earlier request for the same tile can resolve a new click wrongly. | Code reviewer. |
| 8 | Medium | **Part W has no REPORTS entry from this PC and no capture plans,** and its measurements used an ad hoc script, not the packet's tool (`tools/ui_layout.py`, sprite-sad as `docs/re/ui.md` §2 defines it). The executor saw that the script was uncalibrated (it read 96.2 where the committed value is 17.8) and said so in the OQs. That was honest, but the numbers in the OQ-565 and OQ-567 updates can't be used. | `git show b367ee00` changes `open-questions.md` only. |
| 9 | Low | **The C-3 per-test table is not in git.** Part E says it is in `docs/perf/README.md`; only the summary is. The log it comes from is kept on this PC. | `docs/perf/README.md`, C-3 section. |
| 10 | Process | **An independent review covered only K's draw step.** P, W, M and X were self-reviewed. This review covers the code of all parts, but no reviewer re-ran W's captures. | Part E, "Review records". |
| 11 | Low | **M's first measuring run ran beside other heavy work on this PC:** 15 tests were refused for low memory. The second run was clean. | M's report. |
| 12 | Follow-up | **Two CI flakes in tests this package doesn't touch,** each seen more than once on its branch: `F3 (D1, deaf)` incomplete on `macos-clang` (runs 42990604521 and 43009381041), and `scene_picture_publish_test.cpp:323`, `outstanding == 0` got 1, on `linux-gcc` (run 43004484477). The W4-NEXT-1 acceptance asked for the `deaf` test's repeated run once it flakes again; it has. | The run ids in part E. |

**Also open, not findings:**
- OQ-568 (the `stop()` drain) is the owner's QUESTION. The executor recommends (a), leave it, and the orchestrator
  agrees, adding (c): document it in `PlayRunner::stop()`.
- OQ-569 and OQ-570 (row 18's other need icons and its tab) wait on new evidence.
- The C-3 remedy (18 reservations) waits for the owner to schedule it.

## Disposition

- **Fix round F** (packet, part F) covers findings 1 to 9.
- **Finding 10** is closed by part F's reviewer, who runs the tests it accepts.
- **Finding 11** is a rule in part F: no other heavy job during a measuring or gate run.
- **Finding 12** becomes its own follow-up after this package. It is not this package's defect.
- **Part X's original column:** part X step 3 asked for the original's values. The executor didn't try the menus, so
  part F step 8 makes one bounded attempt.

## Merge candidate, built here and not landed

The orchestrator merged both heads on top of `main` `03bf2b3c`, in its own run worktree
(`E:\anno1503-worktrees\anno1503-main-gamefile-run`), not in an executor's:
- `ec4a3d32` merges `task/W4-ui-4`, with no conflict;
- `49161d1c` merges `task/W4-shared-list-1`. The only conflict was `docs/open-questions.md`, where both branches
  appended: OQ-568 to 570 first, then OQ-571.

The candidate won't land. After part F, the orchestrator merges the fixed heads again.

**Game-file run of the candidate `49161d1c`.** This checks that the rest of the package passes beside `main`; it is
not an acceptance.
- **Setup:** MSVC Release with the game files and the RTX 4090 (`ANNO_GPU_ADAPTER=4090`, `ANNO_TEST_THREADS=8`,
  `ctest --preset msvc -j 8`), in the clean run worktree, with nothing else heavy on the PC.
- **Time:** 14:53 to 15:07, 825 s real. The coordinator spoke compatibility version 4.
- **Counts,** classified by `tools/test/ctest_verdict.py` on a `-V`-shaped copy of `LastTest.log`:

  | Selected | Executed and passed | Failed | Skipped | Required-asset skips |
  |---|---|---|---|---|
  | 2,387 | 2,375 | 0 | 12 | 0 |

  - **The 12 skips are the same as at `2c4dc295`:** 6 opt-in, 3 platform, and 3 symlink rows that need a Windows
    privilege this PC lacks. The verdict tool says FAIL only because the run declared no policy for that last class.
- **The new tests executed and passed,** with output, not skipped:
  - `viewer_wiring_selected_house_draw_check`, `selected_house_session_replay`, `selected_house_pixel_check`;
  - the selection and inspection unit cases;
  - `peak_launcher_contract`, `peak_report_contract`;
  - the three `population` cases.

  Finding 1's race did not hit in this run at `-j 8`. That doesn't clear it: the executor's own run hit it at `-j 16`.
- **One more data point for finding 3.** `anno_sim_replay_population_endless_01` runs `endless01.sze`: its residents
  stay at 474 while the store total rises from 1,213 to 1,243 tons over 60 s. With `house_updates` off, production
  runs but houses don't change.
- **Logs:** `cache\main-gamefile-w4-next-2\`. sha256 of `LastTest.log`: `4db87c15...8d2ea0`.
- **CI on the candidate** (`land/w4-next-2`, run 43032818230) was still running when this was written. Its result
  goes into the fix round's acceptance.
