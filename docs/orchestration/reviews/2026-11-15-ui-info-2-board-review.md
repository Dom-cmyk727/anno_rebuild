# UI-INFO-2: the orchestrator's landing check (2026-11-15)

**The package:** `task/UI-INFO-2` `da29872b`, accepted by the UI lane (its review is on the branch:
`docs/orchestration/reviews/2026-11-15-ui-info-2-review.md`). The info panel's rotate arrows turn the armed type
through the keys' own Rotate/RotateBack (GOG `View.dll` `0x10030440`/`0x10030480`); the preview draws rotation block
r. No sim or render change. CI: 43648095209 (code, all 13 jobs, `gate` included), 43651893192 (report), both green.

**The orchestrator's read of the code:**
- `turn_inputs` handles negative steps without overflow;
- the arrows are hit-tested before the category column, where `rotate_left` overlaps the column's hit width;
- the preview takes `DevInput::rotation()`.
No findings.

## The game-file run (2026-11-15)

**The candidate:** `b400a063`. It is `main` `01a4d540`, then `task/UI-INFO-2` `da29872b` (`9a2f21aa`), then
`task/ZOOM-SMOOTH-1` `e8dcb7cf` (`d1ec754d`, REPORTS.md conflict resolved by keeping both sides), then the board.

**The run:** the canonical call, with the GOG oracle shots, the HE captures and
`ANNO_UI_ROTATION_SHOTS=E:\anno1503-worktrees\ui-oracle-shots\ui-info-2` (19 GOG Wine-runner shots, outside git).
00:57–01:19.
- `COUNTS: selected=2631 executed=2603 failed=0 empty=0 skipped=28 (required-asset=0 platform=3 opt-in=22 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: PASS`. No memory refusals.
- `info_panel_rotation_gog_compare` passed (8.10 s), as did `info_panel_rotation_check` (4.97 s).

**Result:** both stacked packages are covered. Landing waits for the owner's OK. **OQ-623** stays with the owner: in
the original, `.` turns +1 like the right arrow, but ours turns −1. It doesn't block landing.
