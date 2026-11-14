# UI-INFO-2 review (UI lane, 2026-11-15)

**Verdict: ACCEPT** on `60b47fbe` (code `50eabe92`, RE `8c68cf4c`, report `60b47fbe`; base main `01a4d540`).
The UI lane checked the package itself (no reviewer subagent; the board's review at handover is the independent one).

| Check | Result |
|---|---|
| Diff scope | `construction_menu`, `ui_layout` (`turn` key), `ui_mock` (preview per rotation), `viewer.cpp` (Turn routing, two facts, `info.rotation`), `hud.txt`, tests, scripts, `ui.md` 2.5, OQ-612/623/624, ADR 0004 one line. Nothing under src/sim, src/render, crates, game. `ui_mock.cpp` only includes `render/iso.hpp` to use `frame_index` (read, not changed) |
| One rotation | an arrow click sends the keys' own `Rotate`/`RotateBack` to `DevInput`; the preview reads `DevInput::rotation()`, so preview, ghost and build cannot disagree (the Kontor's coast turn stays its own, `tool.turn`) |
| Arrow direction | `hud.txt` `turn -1` / `turn 1` with View.dll handlers 0x10030440 `(v-1)&3` / 0x10030480 `(v+1)&3`; the Wine-runner shots show left 3,2,1,0 and right 1,2,3,0 |
| Keys vs arrows (OQ-623) | UI lane zoomed the preview box of `u2_501_start`, `dot1`, `dot2`, `comma1`, `R1`, `L1`: `dot1` and `R1` show the same view, `comma1` after `dot2` returns to `dot1`'s view, `L1` is a different one. So in the original `.` = right arrow = +1 for an ordinary building; the rebuild's `.` is −1. Left for the owner (a feel change), recorded as OQ-623 |
| Rotation 0 unchanged | every existing oracle test passes with no new mask and no tolerance change |
| Focused run on this PC | branch head rebuilt; `ctest -R` with game dir, GOG shots, HE captures and `ANNO_UI_ROTATION_SHOTS` set: **25 of 25 executed and passed, 0 skipped** (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_pixel_check`, `info_panel_rotation_check`, `info_panel_rotation_gog_compare`, `ui_panels_match_gog`, `ui_window_matches_he`, `ui_mock_matches_oracle`, `ui placement`, the rotate-arrow unit test, every `viewer_wiring_ui_*` and `viewer_wiring_construction_*`). Executor's own: 34 of 34 |
| Hosted CI | run 43651893192 on `60b47fbe` success; 43648095209 on `50eabe92` all 13 jobs green incl. `gate` |
| Failure proofs | executor: each new test fails on `01a4d540` (unit 11 of 38 assertions; wiring stops at `ui.info.rotation`; the rotation captures differ by 1 828–1 995 px) |

Gate note for the board: `info_panel_rotation_gog_compare` skips without `ANNO_UI_ROTATION_SHOTS`. The 19 GOG
Wine-runner shots are copied (outside git) to `E:\anno1503-worktrees\ui-oracle-shots\ui-info-2\`; set the variable
to that folder in the game-file run.

Not verified / open: the WSL `-O1` sanitizer lane and the full suite (NOT_RUN here); whether the original fires the
arrow on release (the rebuild acts on press like its other chrome); the Unix warning script's one failure is
`src/sim/tests/extensions_test.cpp` (missing define in the script, not this branch).

Follow-ups: UI-INFO-2b (green entrance arrows; one scale for all four turns, OQ-612/624); the category column's
42 px hit width also covers grid column 0's first 13 px on the plain grid (older than this branch, an obvious bug
for UI-INFO-2b's packet); OQ-623 per the owner's answer.
