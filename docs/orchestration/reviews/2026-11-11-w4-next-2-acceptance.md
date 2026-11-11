# W4-NEXT-2 acceptance, after fix round G (2026-11-11)

**Verdict: ACCEPTED WITH FINDINGS.** Fix round G ([packet](../tasks/w4-next-2.md), part G) closes the
[fix review](2026-11-11-w4-next-2-fix-review.md)'s P1: the selection facts and the window title now come from one
state, and the orchestrator checked that against the code. The other code fixes (G2, G3, G5) hold too. Two of G's doc
corrections are themselves wrong (findings 1 and 2). They are Low and docs-only and don't block landing. Part H of
the packet fixes them after the landing.

**Reviewers:**
- **The Claude Code orchestrator (Opus 5.5):** the code and docs of both branches, the executor's evidence, its own
  measurements for G6, and its own acceptance run of the merge (below).
- **The executor's independent reviewer** (a fresh agent that rebuilt both worktrees and ran the tests): ACCEPT on
  G1, G2 and G5. Its report is kept in the executor's session, not in git. It found that G1's report overstated how
  few places call `update_title()`; the executor corrected that in a new entry.

**Reviewed heads:**

| Branch | Head | Since the fix review | Parts |
|---|---|---|---|
| `task/W4-ui-4` | `b483c4de` | `2138773e` | G1–G4, G6, G8, G9, two CI addenda |
| `task/W4-shared-list-1` | `b2808533` | `e69b036d` | G5, G7 |

Hosted CI: `b2808533` green (run 43082553745); on `task/W4-ui-4`, `406adc84` green (run 43086586507). The two later
commits there are docs-only. On `e37f909f` (run 43091869311) the Windows test step logged `VERDICT: PASS`, and only
the artifact upload failed on a GitHub error. The run for `b483c4de` (43099636789) was still queued at review time.
The hosted check that counts is the `gate` that `land.py` waits for on the merge commit.

## What holds, checked against the code

- **G1, the P1.** `script_tick` no longer calls `selection_refresh()`. Between its `SDL_GetWindowTitle` read and the
  `selection.*` facts, nothing changes `selection_`. The executor's argument needs one more step, which the
  orchestrator checked:
  - **The title tracks every change the facts show.** `selection_` changes in only two places:
    - `selection_handle()`, which ends a consumed step with `update_title()` in `handle()` (`viewer.cpp` lines
      1178–1180). Its unconsumed steps (a tool's click, or Escape or an off-map click with nothing selected) leave
      the state unchanged (`selection.cpp`, `selection_step`).
    - `selection_refresh()`, which runs only inside `update_title()`, before the title is set.
  - **A click's `pending` alone doesn't change the title.** The pending block's `update_title()` picks up the
    answer.
  - **So when `script_tick` runs, the title and the facts describe the same `selection_`.** A wake event pushed
    meanwhile is handled only in the next pass.
- **Finding 5 of the fix review.** It goes away with G1. F3's demolish step now depends on the wake event, as play
  does, and passed in every run below.
- **G2.** `selection_clicks_` is incremented after every early return of `selection_handle` (the inactive tool, the
  press, a drag, the HUD hit test). Script events are pushed at the end of a tick and handled at the start of the
  next pass, so `expect fact selection.clicks same` sees a click that reached `selection_step`. The step doesn't
  depend on timing. The executor's build with the hit test disabled failed it (`selection.clicks is '2', it was '1'`).
- **G3.** `ANNO_VIEW_QUIT_S` is now 240, as in `viewer_wiring.py`, with a 260 s subprocess timeout under CTest's 300.
- **G4.** ADR 0004 now says that `request_inspect` only stores the tile.
- **G5.** `cmd_population` prints every checkpoint, then returns 1 if any `check_invariants()` was non-empty. No
  scenario at hand breaks an invariant, so the exit-1 path wasn't run live; the reviewer noted that too.
- **G6's definition.** The grey-key rule in `docs/re/ui.md` §2 is the rule from `ui1_f22_bind.py`, and it gives
  32.07 on both shots. Its conclusions about calibration are wrong; see finding 1.
- **G8.** The F1–F4 and F7 records are backfilled. Where evidence wasn't kept, they say NOT_KEPT.

## Findings

| # | Severity | Finding | Evidence |
|---|---|---|---|
| 1 | Low | **G6's calibration conclusion is wrong: the plain method reproduces, only the grey-key figures don't.** The committed plain sprite-sad figures are a per-pixel mean, \|ΔR\|+\|ΔG\|+\|ΔB\| over the kept pixels, not divided by 3. Measured that way, zoom + (`fenster.tex` id 44 at (959, 7)) gives **2.885** and compass west (id 48 at (965, 140)) **3.088** on both kept copies of `hud_idle_running`, exactly the committed figures. The executor's 0.962 and 1.029 are the same numbers divided by 3 (ratio 3.000 ± 0.001). So: (a) `docs/re/ui.md` §2's new paragraph states the plain method's normalization wrongly ("divided by 3, same as the plain method otherwise"); (b) §9.7, the `hud.txt` comment and OQ-567's G6 paragraph conclude the opposite of the evidence ("the gap is not specific to grey-key"). What doesn't reproduce is the committed **grey-key** figures: 17.8 for id 974, and also the portrait's 34.2 (`Pioniere.tex` id 1 at (812, 355): 45.52 per channel, 136.56 per pixel, on both kept copies of `f15_selected_house`). The project's docs mix two normalizations: `ui1_f22_bind.py` divides by 3, the plain figures don't. | The orchestrator's check with the same pixel rules (alpha ≥ 8; grey-key as in §2). Shots in `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\` (not in git). |
| 2 | Low | **G7 calls three non-endless targets endless.** Only `01_1024_easy_256` is in `Scenes/0010SingleEndlos`. `Single_06`, `Single_16` and `Single_24` are in `Scenes/0200Single` (the shared list's own table), and `S01_08_01` is in `0100Kampagne01`. `original-wine.md` §6 says "the other four endless-game files" sit in the New Game list, and the executor's message says "4 of 5 target files" are endless. `tools/orig/menus.json` maps only `choose_game`'s five difficulty rows (Citizen to Marquess). Whether the New Game list also holds the `0200Single` scenarios (further down, say) isn't checked. | `docs/research/shared-test-list.md`, the file table; `game/app/Scenes/`; `tools/orig/menus.json`. |
| 3 | Note | The recurring macOS `F3 (D1, deaf)` coordinator flake hit a third time (run 43082536481). It is still the first review's finding 12, a follow-up of its own. | The CI addendum in REPORTS. |

**Still open, unchanged:** OQ-565, 566, 567, 568 (the owner's), 569, 570 and 571; the shared list's original column
(NOT_RUN); the C-3 reservations (the owner schedules them); the CI flakes (first review, finding 12).

## Disposition

- **Land both branches** with the owner's OK: the merge below, plus this review.
- **Part H** (packet), docs only, after the landing, on a new branch from `main`. It corrects findings 1 and 2 in new
  text (no re-gate of code needed):
  - finding 1: the normalization of both methods in `docs/re/ui.md` §2, and the G6 conclusions in §9.7, `hud.txt`
    and OQ-567;
  - finding 2: the scenario folders in `original-wine.md` §6, and what the New Game list holds as the first thing to
    check.
- **Finding 3** stays with the CI-flake follow-up.

## The merge and the orchestrator's acceptance run

**The merge, built here.** In the orchestrator's run worktree
(`E:\anno1503-worktrees\anno1503-main-gamefile-run`, branch `orch/w4-next-2-merge-g`), on top of `main` `088dcc63`:
- `eedb7600` merges `task/W4-ui-4` `b483c4de`, with no conflict;
- `c944aff7` merges `task/W4-shared-list-1` `b2808533`. Both branches appended to `docs/open-questions.md` and
  `docs/orchestration/REPORTS.md`, so both blocks were kept, `W4-ui-4`'s first. Every new heading of both branches
  appears exactly once.
- **33 files change against `main`.** None is under `src/sim`, `src/render`, `src/formats` or `src/assets`.

**The run.** MSVC Release with the game files and the RTX 4090 (`ANNO_GPU_ADAPTER=4090`, `ANNO_TEST_THREADS=16`),
22:05 to 22:22. Nothing else ran on the PC apart from the repeat run below, which ran beside the gate on purpose:
these are the conditions under which the fix review's run failed. The coordinator spoke compatibility version 4
(build `208a2c085d5db934`).
- **The canonical gate**, `tools/test/run_selected.py --select "." --asset-policy required --allow-skip other
  --jobs 16`: **VERDICT: PASS.**

  | Selected | Executed | Passed | Failed | Skipped | Required-asset skips | Incomplete |
  |---|---|---|---|---|---|---|
  | 2,390 | 2,378 | 2,378 | 0 | 12 | 0 | 0 |

  - **Skipped:** the same 12 as before: 6 opt-in, 3 platform, and 3 `ship start` rows.
  - **Log:** `cache/orch-w4-next-2-g/canonical-gate.log`, sha256
    `cb6340f61c16d05e1b383cfbb43ce5149dc500ec817c86774934beb30f205827`.
- **The repeats:** `ctest -R selected_house --repeat until-fail:20 -j 3`, in a second ctest process during the
  gate's first 5.5 minutes.

  | Test | Passed | Failed |
  |---|---|---|
  | `viewer_wiring_selected_house_draw_check` | 20 | 0 |
  | `selected_house_session_replay` | 20 | 0 |
  | `selected_house_pixel_check` | 20 | 0 |

  **Log:** `cache/orch-w4-next-2-g/repeat-selected-house.log`, sha256
  `62028a1fcf58c691195d873d3c6dbe344b7cf5e1d4f9097c05d0e5b4ffd9bb64`. These support the construction above; they
  don't replace it.
- **The negative control** (`cache/orch-w4-next-2-g/negative_click_ground.script`, the fix review's script): it builds
  the house, clicks the free tile next to it and waits for a House selection. It failed at its bound, as it must:
  `FAIL line 22 (wait): not over after 5000 ms (… selection.kind is '')`, exit 1. So the fact wait still fails when
  the selection doesn't come.

The `cache/` logs are git-ignored and stay on this PC.
