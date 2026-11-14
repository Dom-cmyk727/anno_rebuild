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
