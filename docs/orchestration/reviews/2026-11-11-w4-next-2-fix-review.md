# W4-NEXT-2 fix round review (2026-11-11)

**Verdict: CHANGES REQUESTED.** Fix round F ([packet](../tasks/w4-next-2.md), part F) fixes most of the
[first review](2026-11-11-w4-next-2-review.md)'s findings, but its P1, the selected-house session racing the sim, is
narrowed, not closed. The orchestrator's acceptance run failed `viewer_wiring_selected_house_draw_check` in the
canonical gate, and the code shows why (finding 1). Fix round G (packet, part G) fixes it, together with the
smaller findings below. Both branches then land together.

**Reviewers:**
- **The Claude Code orchestrator (Opus 5.5):** the code and docs of both branches, the executor's evidence, and its
  own acceptance run of the merge (below).
- **The executor's independent reviewer** (a fresh agent that built both worktrees and ran the selected-house and
  population tests itself): ACCEPT on F1, F2, F3 and F5. Its report is kept in the executor's session, not in git.
  It did not find finding 1.

**Reviewed heads:**

| Branch | Head | Since the first review | Parts |
|---|---|---|---|
| `task/W4-ui-4` | `2138773e` | `f1f772b4` | F1–F4, F6, F7, F9 |
| `task/W4-shared-list-1` | `e69b036d` | `593cdd3f` | F5, F8 |

Hosted CI is green on `05291e02` (run 43065140217; `2138773e` adds only the F9 report) and on `e69b036d` (run
43065156973).

## What holds, checked against the code

- **F1's mechanism, apart from finding 1.** Every selection check in `selected_house_draw_check.script` waits on a
  `selection.*` fact (`viewer.cpp`, `script_tick`). Each screenshot after such a wait gets `wait frames 1`. That
  closes the capture gap: `wait frames` counts drawn frames (`DevScriptState::frames = drawn_frames_`,
  `dev_script.cpp` line 454), and `draw()` runs after `script_tick()` in the same loop pass. So a screenshot reads
  a buffer drawn after the answer, or the wait fails at its limit; it never passes silently.
- **The new wait can fail.** The orchestrator's negative control
  (`cache/orch-w4-next-2-fix/negative_click_ground.script`, not in git) builds the house, clicks the free tile next
  to it and waits for a House selection. It failed at its bound, as it must: `FAIL line 22 (wait): not over after
  5000 ms (… selection.kind is '')`, exit 1.
- **F2, HUD click-through.** `point_in_hud_panel` tests `hud.txt`'s own `panel` rectangle (779, 0) 245 × 733.
  `selection_handle` consumes a click inside it before any pick or inspection request, and also clears `drag_`.
  The unit cases cover the review's three points and the panel's edges.
- **F3, the demolish drop.** The step posts `local demolish` with the developer tool off, so `selection_refresh`'s
  drop logic resolves the title, not the tool's hover. Finding 5 qualifies what this proves.
- **F5, the population tool.**
  - `--step-ms` defaults to 10, the play mode's step.
  - `--house-updates` is opt-in.
  - `--seconds` must be strictly increasing (exit 2, tested).
  - The header prints the step, the rules digest and `house_updates`.
  - `check_invariants` runs and prints at every checkpoint.
  - The re-measurement of `01_1024_easy_256`: flat with `house_updates` off; 230 → 332 residents between 30 and
    60 s with it on.
- **F7, the C-3 table.** All 27 rows are in `docs/perf/README.md`, 18 of them without a reservation, with the log's
  name and sha256.
- **F4** is in ADR 0004 as a known limitation, as the packet allowed. Finding 6 corrects one sentence of it.

## Findings

| # | Severity | Finding | Evidence |
|---|---|---|---|
| 1 | **P1** | **The selected-house session still races the sim, now inside `script_tick`.** `script_tick` copies the window title into the script's state (`viewer.cpp` lines 2199–2201), then calls the new `selection_refresh()` (line 2230). When the sim publishes the click's answer between the run loop's `update_title()` (the pending block, line 1038) and that call, the refresh selects the House and `selection.kind` becomes `House`, but the title isn't updated. `wait fact` then passes and `expect title selected House` fails in the same tick. Nothing updates the title afterwards either: the pending block no longer runs, so only a wake event would. It happened in the orchestrator's canonical gate at `-j 16`, at the second selection (script line 81): the title read `… CPU — tick 1400, game 2800 ms …` without "selected". | `cache/orch-w4-next-2-fix/canonical-gate.log`, test #855. |
| 2 | Process | **No REPORTS entries for F1, F2, F3, F4 or F7.** The packet asked for one per step. F9's report points to "F1's own report", which doesn't exist. F1's proof (the per-test counts of the 20 repeats and the failing negative control) is in no committed record, and the repeat run's log wasn't kept. F7's correction of part E's sentence is in `docs/perf/README.md`, not in a REPORTS entry. | `git diff f1f772b4 2138773e -- docs/orchestration/REPORTS.md`: only part W, F6 and F9 are new. |
| 3 | P3 | **The pixel check's 30 s bound is unchanged and unjustified.** F1 asked for 240 s, or a lower bound justified by a measured run time with a margin. | `selected_house_pixel_check.py:133`, `ANNO_VIEW_QUIT_S="30"`. |
| 4 | P3 | **The F2 script step can't fail reliably.** Without the fix, a click on the panel only sets `pending`, and `selected` stays until the sim answers (`selection.cpp`, the `Click` case). So `wait frames 2` then `expect title selected House` passes whenever the answer is slower than two frames: under load, exactly when it matters. Nothing showed that the step fails without the fix. | Script lines after `click_px 810 495`. |
| 5 | P3 | **The per-tick `selection_refresh()` in `script_tick` exists only in scripted sessions.** In play, the drop after a demolish comes from `update_title()` on the runner's wake event (`viewer.cpp`, the wake case of `handle`). So F3's session passes even if that trigger broke. The fact wait (`within 5000`) already tolerates the wake's latency. The call also changes `selection_` without setting `dirty_`, which the pending path does. It is the cause of finding 1. | `viewer.cpp` `script_tick`; the run loop's pending block. |
| 6 | Low | **ADR 0004's F4 note misstates the API.** It says `PlayRunner::request_inspect` "already hands out a fresh request each call". It only stores the tile under a mutex. | `play.hpp`, `request_inspect`. |
| 7 | Low | **F6's "no artifact on this PC reproduces 17.8" is incomplete, and its new owner QUESTION is answerable from evidence.** `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\ui1_f22_bind.py` implements grey-key: it skips sprite pixels with \|r−g\| < 12 and \|g−b\| < 12 and averages per channel. It measures exactly this pair (`VIEWINFOPIONIERMAIN_BK 783,252`). Run by the orchestrator, that method gives **32.07** on both `f15_selected_house` and `f22b_help_house`; `ui1_f21_bind4.json` kept 86.98 (a per-pixel sum) at (778, 274). So grey-key is a desaturation key, as the executor's 32.07 at a spread of 10–15 already showed. What fails is the committed 17.8 (`docs/re/ui.md` line 655, `data/ui/hud.txt`): no kept artifact reproduces it. | The orchestrator's check, the same code as that script's `mean_sad`. |
| 8 | Low | **F8 tried only the "campaign" menu.** The first target, `01_1024_easy_256`, is an endless-game file (`Scenes/0010SingleEndlos`). The endless games are the ordinary New Game list: `choose_game`'s Citizen to Marquess rows, which `macros.new_game` already reaches. Whether a row always loads that file wasn't checked. | `tools/orig/menus.json`; `docs/testing/original-wine.md` §6. |
| 9 | Low | **`population` prints an invariant violation but exits 0.** | `cmd_population`'s `sample`. |
| 10 | Note | F8's Wine session (17:39–17:43) ran during the F1 contention run (17:11–17:47). Extra load doesn't weaken a contention proof. The canonical gate ran afterwards (17:53–18:07). | Log timestamps in the executor's `cache\w4-next-2\`. |

**Still open, unchanged:** OQ-565, 566, 567, 568 (the owner's), 569, 570 and 571; the shared list's original column
(NOT_RUN); the C-3 reservations (the owner schedules them); the CI flakes (first review, finding 12).

## Disposition

- **Fix round G** (packet, part G) covers findings 1 to 9. Finding 7 replaces F6's owner QUESTION with work; the
  owner has nothing to answer there.
- **Finding 10** needs nothing.

## The merge and the orchestrator's acceptance run

**The merge, built here and not landed.** In the orchestrator's run worktree
(`E:\anno1503-worktrees\anno1503-main-gamefile-run`, branch `orch/w4-next-2-merge-f`), on top of `main` `3815dd76`:
- `f67e8675` merges `task/W4-ui-4`, with no conflict;
- `69c18c9a` merges `task/W4-shared-list-1`. Both branches appended to `docs/open-questions.md` and
  `docs/orchestration/REPORTS.md`, so both blocks were kept, `W4-ui-4`'s first. Every heading of both branches is
  there once.
- **32 files change against `main`.** None is under `src/sim`, `src/render`, `src/formats` or `src/assets`.

**The run.** MSVC Release with the game files and the RTX 4090 (`ANNO_GPU_ADAPTER=4090`, `ANNO_TEST_THREADS=16`).
Nothing else ran on the PC apart from the repeat run below, which was there on purpose. The run took 18:52 to 19:05. The coordinator spoke compatibility version 4
(build `208a2c085d5db934`).
- **The canonical gate**, `tools/test/run_selected.py --select "." --asset-policy required --allow-skip other
  --jobs 16`, with the three selected-house tests repeating beside it (next item):

  | Selected | Executed | Passed | Failed | Skipped | Required-asset skips |
  |---|---|---|---|---|---|
  | 2,390 | 2,377 | 2,376 | **1** | 12 | 0 |

  - **Failed:** `viewer_wiring_selected_house_draw_check`, finding 1.
  - **Skipped:** the same 12 as before: 6 opt-in, 3 platform, and 3 `ship start` rows.
  - **Log:** `cache/orch-w4-next-2-fix/canonical-gate.log`, sha256
    `19cb3ada41fc44b9a9655c07d3774794b6f0474fbc74b5277549c4ebfd196710`.
- **F1's proof, reproduced:** `ctest -R selected_house --repeat until-fail:20 -j 3`, in a second ctest process
  while the gate ran.

  | Test | Passed | Failed |
  |---|---|---|
  | `viewer_wiring_selected_house_draw_check` | 20 | 0 |
  | `selected_house_session_replay` | 20 | 0 |
  | `selected_house_pixel_check` | 20 | 0 |

  - **Log:** `cache/orch-w4-next-2-fix/repeat-selected-house.log`, sha256
    `9f5cb313e4a9adb288a1aba8ed9a72eaa70a248274a3b9c41550cc791cd09199`.
  - **What it shows:** 60 passes beside one failure in the gate. Counting the executor's 60, the race hit about
    once in 120 runs, so 20 repeats can't show it fixed. Part G asks for a proof by construction.
- **The negative control:** failed at its bound, as it must (see "What holds").

**The earlier merge candidate** `49161d1c` (`land/w4-next-2`, CI run 43032818230): Windows, macOS, format and lint
passed. The three Linux jobs stopped in `apt-get install` (exit 100; the Azure Ubuntu mirror didn't resolve) before
compiling anything, so its Linux CI is NOT_RUN, not failed. The branch is deleted; the commit stays on
`orch/w4-next-2-merge` in the run worktree.
