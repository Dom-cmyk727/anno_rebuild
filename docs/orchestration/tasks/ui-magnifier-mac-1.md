# UI-MAGNIFIER-MAC-1: HiDPI (density != 1.0) dev-script coordinates & first-frame visibility diagnostics

**Released 2026-11-20** (Windows Orchestrator; Candidate 10 unblock).
- **Lane & Host:** **Windows PC UI Executor** — runs on this Windows 11 PC (MSVC Release + RTX 4090 + WSL `check_unix_warnings.sh`). You are **NOT** a Mac executor; live macOS/Metal verification remains explicitly `NOT_RUN` until the Mac re-checks the updated candidate.
- **Branch / Base:** `task/UI-MAGNIFIER-MAC-1` from Candidate 10 `95897a55` (`refs/remotes/candidates/ui-info-2d-buildcat`) merged with `origin/main` `a6468efd` (`ubuntu-24.04` CI pin).
- **Worktree:** `E:/anno1503-worktrees/anno1503-UI-MAGNIFIER-MAC-1`.
- **Size / OQ range:** ~2–3 hours (stop and checkpoint if approaching 6 hours). **OQ-795..797** reserved.
- **No reviewer subagents** (owner rule 2026-11-12: orchestrator review at handover is the independent review). Report `READY FOR REVIEW`, never "approved".

## Why (Candidate 10 Mac Gate Blocker)
Candidates 6–10 (`95897a55`, 16 accepted packages) cannot land because two UI magnifier tests failed on macOS Metal (`density = 2.00`):
1. **Post-script zoom failure (`MAC-GPU-WINDOW-1`, `origin/orch/mac-lane:docs/orchestration/reviews/2026-11-19-mac-gpu-window-1-review.md`):** 4/4 runs presented frames (7,287 successful acquires), `resize 1024 768` yielded a `2048x1536` drawable at `density 2.00`, and then:
   - `viewer_wiring_ui_magnifier_range` failed at line 28: `zoom.target` stayed `20000` (expected `10000`) after `click_px 835 163`.
   - `viewer_wiring_ui_magnifier_range_stepped` failed at line 29: `view.tile_w` stayed `20` (expected `42`) after `click_px 805 145`.
2. **Pre-script 30 s no-frame abort (`2026-11-17-gate-ui-info-2d-buildcat-review.md`):** In the earlier 4-job Mac full gate, both cases aborted with `no frame was drawn within 30000 ms`. `MAC-GPU-WINDOW-1` could not distinguish `hidden_` from acquire/backoff because `viewer.cpp` mutates `hidden_` without logging or tracing visibility transitions.

## Code Pointers to Trace & Verify (Check, Do Not Guess)
1. **Script resize vs density (`src/app/src/viewer.cpp`):** compare `act.resize` (`viewer.cpp:3342-3345`, raw `SDL_SetWindowSize(window_, w, h)`) against `ANNO_VIEW_SIZE` (`viewer.cpp:1129-1135`) and `resize_at_` (`viewer.cpp:1415-1422`), both of which divide by `density` to target drawable pixels. Check whether `resize W H` in `dev_script.cpp` / existing scripts means drawable pixels or window points, and audit every script using `resize`.
2. **Script `*_px` events vs density (`src/app/src/dev_script.cpp`, `src/app/src/viewer.cpp`):** compare `DevScript::window_point` (`dev_script.cpp:445-456`, divides drawable px by `density`) against `click_px`, `hover_px`, `ldown_px`, `ldrag_px`, `rclick_px`, `rdrag_px`, and `wheel N X Y` (`dev_script.cpp:620-790`), which emit raw `(*x, *y)` SDL coordinates that `viewer.cpp` (`1701, 1707, 1735, 1749, 2015, 2056, 2362, 2454`) then multiplies by `density`. Check `pan` / `drag` / `st.window_w/h` consistency too.
3. **First-frame visibility / backoff diagnostics (`src/app/src/viewer.cpp`):** inspect `hidden_` (`viewer.cpp:1429, 1590-1600, 3615`) and `retry_at_` / no-frame deadline handling. Add minimal diagnostic output (trace note and/or stderr summary on visibility transitions and on a no-frame abort naming `hidden_`, `retry_at_`, and drawn frame count) so any recurrence attributes the gate directly. Do **not** weaken the 30 s timeout, skip on macOS, or invent a display/focus policy.

## Do
1. Write a compact acceptance table before editing (positive proof + distinguishing negative control/counterexample for each requirement).
2. Trace and document the exact coordinate contract (`script -> SDL event -> viewer.cpp -> ui_hit_at -> minimap_press`) and fix the verified `density != 1.0` defect(s) cleanly across `dev_script.cpp` and/or `viewer.cpp` without breaking `density == 1.0` or real user mouse input.
3. Add deterministic unit/harness tests covering non-1.0 density (e.g. `density = 2.0F` and fractional `1.5F`):
   - `DevScript` `click_px` / `*_px` / `wheel N X Y` (and `resize` scaling helper if extracted) produce coordinates that map back to the exact intended drawable pixel after `viewer`'s `* density` conversion;
   - Magnifier `-` `(835, 163)` and `+` `(805, 145)` hit-test accurately at `density = 2.0F` for a `1024x768` drawable request;
   - Visibility/backoff diagnostic check (e.g. `hidden_` transition recorded/reported).
4. Run a deliberate failure proof: revert the fix (or apply a targeted mutant), confirm the new `density = 2.0F` test fails with the exact missed-click / wrong-drawable symptom, then restore and re-verify green.
5. Keep `src/sim`, `src/formats`, and `fixtures/synthetic/sim/` untouched (0 state/pin/golden changes; state remains 139).

## Gates (Windows PC — Mandatory Evidence Contract)
- Check no competing `cl.exe`, `link.exe`, `ctest`, or WSL compile is running before heavy jobs. Keep admission control enabled.
- Build (`cmake --build --preset msvc --config Release`) warning-free (`/W4 /WX`).
- Run canonical gate via `python -I tools/test/run_selected.py --build-dir build/msvc --config Release --jobs 8` with:
  - `ANNO1503_GAME_DIR=E:\anno1503\game\app`
  - `ANNO_UI_HE_CAPTURES=E:\anno1503-worktrees\he-ui-captures`
  - `ANNO_UI_ROTATION_SHOTS=E:\anno1503-worktrees\ui-oracle-shots\ui-info-2`
  - `ANNO_UI_ORACLE_SHOTS` set to this PC's GOG UI shots folder (verify it is set so none of the six oracle tests skips; do **NOT** set `ANNO_DEV_GOODS_TONS`)
  - `ANNO_GPU_ADAPTER=RTX` (or `4090` per `TASKS.md` §1 item 4) for GPU tests
  - Required tests (`--require-test`): `viewer_wiring_ui_magnifier_range`, `viewer_wiring_ui_magnifier_range_stepped`, `live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`
  - Selection (`--select`): affected `anno_app_tests` / `anno_app_input_tests` (`dev_script|minimap|magnifier|hud|ui |ui_|construction|selection`) plus `^viewer_wiring_ui_`
- Formatting: `clang-format` 23.1.3 (`E:\anno1503-worktrees\.scratch-cfpkg\bin\clang-format.exe --dry-run --Werror`) clean on touched files.
- Linux warnings: one `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh` clean (GCC 13 + Clang 18, with and without GPU; negative controls caught).
- Report: append a <=60-line `READY FOR REVIEW` entry to `docs/orchestration/REPORTS.md` (exact head/tree, root cause, acceptance table, classified `COUNTS`/`VERDICT` lines, failure proof, Decisions & reasoning, explicit `NOT_RUN` including live macOS/Metal confirmation).
- Publish: ordinary-push `task/UI-MAGNIFIER-MAC-1` to `origin`, run `python -I tools/orch/sync_gitlab.py --repo E:/anno1503`, check exact-head GitHub Actions CI, and STOP.
