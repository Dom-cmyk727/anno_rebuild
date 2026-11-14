# UI-HE-1 review (2026-11-14)

**Verdict: ACCEPTED, docs only.** The UI lane's note `docs/re/he-ui-scale.md` explains how the History Edition
scales the in-game UI to the window. It is the evidence for ADR 0004 R3 and for the UI lane's next packet,
UI-WINDOW-1.

It lands on the owner's OK. As a docs-only change it needs no game-file run.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/UI-HE-1` `5d0db740`, two commits on `main` `95eea7de`:
- `734641ec`: the static RE;
- `5d0db740`: the three visual samples and the 1200-line rule.

Hosted CI is green on both: run 43574478483 and run 43576587493.

## What the orchestrator checked

- **Scope and publication.** Only two files change:
  - the new `docs/re/he-ui-scale.md`;
  - a two-line pointer in `docs/re/ui.md` §5.

  There are no HE bytes, pixels or captures in git. The captures are in `E:\anno1503-worktrees\he-ui-captures`,
  outside the repo.
- **The rule, recomputed by hand from the note's own steps.** In the step-down loop, `uiW = trunc(W / s)`,
  `uiH = trunc(H / s)`, and `s` drops by 0.1 while the UI is under 1024 × 768. Then `k` is the smaller of `W / uiW`
  and `H / uiH`, and the side panel is at `uiW − 245`. At 2542 × 1351:
  - **`UI_SCALE` 2.0:** `s` steps down to 1.7, the UI is 1495 × 794, `k = 2542 / 1495 = 1.7003`, and the panel is
    at `1250 · k = 2125.4`;
  - **`UI_SCALE` 1.5:** the UI is 1694 × 900, `k = 1.5006`, and the panel is at 2174.4;
  - **`UI_SCALE` 1.0:** 1:1, the panel at 2297.

  All three equal the note's predictions, and the captures' best fits (1.000, 1.500, 1.700, with x 2297, 2174 and
  2125). Without the 1200-line rule, 1920 × 1080 would give 1.4, as the note says.
- **The evidence is graded honestly.** Sites the UI lane disassembled itself are marked **re-checked**. Things not
  proven are in "Open": the status bar and minimap anchoring, where the scale value persists, and whether the
  1200-line rule reads the client height or the display mode.
- **The original stays the base.** §5 keeps GOG's 1:1 UI at 1024 × 768 as the faithful core, and HE's scale as an
  option on top, as the owner's rule of 2026-11-14 says.

## Notes

- **The open points stay in the note's own "Open" list.** When UI-WINDOW-1 depends on one of them (the status bar,
  minimap and pop-up anchoring, or the 1200-line question), its packet registers it as an OQ in
  `docs/open-questions.md`.
- **The default** is the owner's call. The rebuild's own UI scale can follow HE's rule, or stay at the original's
  1:1. ADR 0004 R3 records it when UI-WINDOW-1 is written.

## Disposition

ACCEPTED. It lands on the owner's OK as a docs-only candidate: `main`, then `task/UI-HE-1`, then the board, through
`land.py`, after PINS-1.
