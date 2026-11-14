# UI-INFO-1, the board orchestrator's review (2026-11-14)

**Verdict: ACCEPTED for landing.** This is the board's check before landing, on top of the UI lane's own independent
review (`docs/orchestration/reviews/2026-11-14-ui-info-1-review.md` on the branch, ACCEPT WITH FINDINGS, findings 2–8
fixed in fix round F).

Landing waits on three things:
- the owner's decision on the preview (the UI lane's finding 1);
- the owner's landing OK;
- the game-file run of the landing candidate.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/UI-INFO-1` `83a0adff` (code head `9d151067`). It contains `main` `95eea7de`, merged as
`804de618` with no conflicts. CI run 43541629138 on `9d151067` is green in all 12 jobs, `gate` included. `83a0adff`
adds only the UI lane's review note.

## What the orchestrator checked

- **Scope.** The branch changes 22 files:
  - nothing in `src/sim` or `src/render`;
  - in `viewer.cpp`, only the two UI draw calls (CPU and GPU) and the read-only query that feeds them;
  - in `src/app/CMakeLists.txt`, two new tests (`info_panel_pixel_check`, and `info_panel_gog_compare`, which
    skips without `ANNO_UI_ORACLE_SHOTS`).
- **Publication** (GitHub is public). The branch adds no image or binary file, and no game text or pixel data in
  `src/` or `tools/`. Building names are read from `Texte.dat` at run time. The `data/ui/hud.txt` additions hold
  only element names, coordinates, sizes, file names, methods and shot names.
- **The code.**
  - `ConstructionMenu::info_panel_content` only reads the registry.
  - Upkeep is `running_cost[1]`, and only when PROD3 has its two columns. With no value, nothing is drawn
    (finding 5).
  - Every new drawing step stays inside its rectangle:
    - `crop_to_element` cuts INFOEXIT to its 33 × 35 gadget;
    - `clip_to` holds the preview to its 135 × 135 box;
    - the existing blits clip to the canvas.
  - The unproven part of the preview is confined to one function, `preview_placeholder_fit`, which uses integer
    arithmetic only.
- **The open-question numbers.** OQ-612 and OQ-613 are free on `main`, on the board and on every open branch.
  - FISHER-RE-1c's reserved range (OQ-609 to 612) left 611 and 612 unused (its report).
  - So no renumbering is needed. OQ-611 stays an unused number.
- **The evidence.** It rests on the UI lane's independent reviewer, who re-measured with its own scripts:
  - 0 differing pixels for the name, figures and chrome on all 15 GOG shots;
  - the RGB565 reduction is legitimate;
  - the View.dll addresses were re-read with capstone.

  The orchestrator did not repeat the pixel measurements.

## Not verified by the orchestrator

- The focused gate (24 of 24 tests passed) and the WSL warning check. Their logs are in the UI lane's worktree; the
  landing's game-file run re-runs every test on the merged commit.
- Linux and macOS game-file runs, and a History Edition capture of the panel (none exists; OQ-613).

## For the landing

- **The candidate:** `main`, then `task/UI-INFO-1`, then the board. Its game-file run uses the canonical call, plus
  `ANNO_UI_ORACLE_SHOTS` set to a copy of the 15 GOG info-panel shots in the run worktree's `cache/`. With that set,
  `info_panel_gog_compare` actually runs instead of skipping.
- **If the owner picks "empty box":** the UI lane sends a follow-up commit first, and the candidate is built from
  that head.

## Notes

- **The preview (finding 1)** is the owner's decision. The orchestrator agrees with the UI lane's reviewer: keep the
  flagged placeholder.
  - It is closer to the original than an empty box.
  - It lives in one named function.
  - UI-INFO-2 is to read the original's preview code (View.dll from `0x1002fb30`).
- **Hard-coded names and colours.** The four material fields use the goods names `WERKZEUG`, `HOLZ`, `ZIEGEL` and
  `MAMOR`, which View.dll's goods ids `0x29`–`0x2c` ask for. A mod that renames them shows 0 there. Together with
  finding 9 (the text and shadow colours are code constants), this is for the UI lane to move into `data/ui` later.
  It does not block the landing.

## Game-file run of the landing candidate

**The candidate:** `orch/land-ui-info-1` at `411f37eb`: `main` `95eea7de`, then `task/UI-INFO-1` `83a0adff`, then the
board up to `7f9843cd`. Both merges were clean, and no file was changed by both sides. The owner's decisions
(2026-11-14): keep the flagged preview placeholder, and land.

**The run:** in the run worktree on this PC, the canonical call with the game files, 16 jobs, and
`ANNO_UI_ORACLE_SHOTS` set to a copy of the GOG info-panel shots. Every GPU test reports the RTX 4090 (Vulkan).
2026-11-14 08:54–09:06.
- `COUNTS: selected=2588 executed=2556 failed=5 empty=0 skipped=27 (required-asset=0 platform=3 opt-in=21 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`
- `VERDICT: FAIL failed tests`
- **All 5 failures are memory refusals** (`[anno-test memory] refused … reason=low-memory stop`): the run filled
  memory itself, and free memory fell to about 2 GB. None of the five ran:
  - the render scene corpus;
  - the legacy river digests;
  - the chunk draw lists;
  - population A;
  - the scenario corpus.
- **The new tests ran and passed:**
  - `info_panel_gog_compare` ran instead of skipping;
  - `info_panel_pixel_check`;
  - the `info_panel_content` unit test.
- **The 27 skips** are the same as at `95eea7de`.
- **Log:** `cache/orch-land-ui-info-1/canonical-gate.log`, sha256 `dfaa0be5d9377685…`.

**The five refused tests, each rerun alone** (`run_selected.py`, `--jobs 1`, the same build, 09:08–09:19): each run
printed `COUNTS: selected=1 executed=1 failed=0 … skipped=0` and `VERDICT: PASS`, with no refusal.

| Log | Test | Time |
|---|---|---|
| `retry-01.log` (`68daf42d…`) | legacy river shading reproduces the previous enhanced digests | 430 s |
| `retry-02.log` (`cd1e2475…`) | population A: created players carry every independent shipped mask | 1.4 s |
| `retry-03.log` (`9f34916b…`) | chunk draw lists replay to the objects and base planes, every chunk | 74 s |
| `retry-04.log` (`0501359a…`) | scene renders match the recorded digests | 166 s |
| `retry-05.log` (`dd8e76c1…`) | scenario corpus: every .sze loads, creates and runs five minutes | 18 s |

So every test of the candidate ran and passed.

**Landed** 2026-11-14 through `tools/orch/land.py`: `gate` passed on `land/ui-info-1` (run 43575344349), and
`main` moved `95eea7de` → `411f37eb` by fast-forward. GitLab and the share are synced.
