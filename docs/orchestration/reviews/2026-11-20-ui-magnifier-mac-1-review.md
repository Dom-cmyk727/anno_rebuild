# Review: UI-MAGNIFIER-MAC-1 (HiDPI dev-script coordinates & visibility diagnostics)

Reviewer: Windows orchestrator, 2026-11-20.
Branch: `task/UI-MAGNIFIER-MAC-1`, base `488584f9` (Candidate 10 `95897a55` + `origin/main` `a6468efd` CI pin).
Reviewed commits: code+tests `cbfa8b1a2d8d8da4b7914098c7cf2fafe77c5256` (tree `442178f1b74bbd207fc967973a7ef3cd983d0f99`), report `e2e4b617f6a3e7e748984102ec591e007a096d7b` (tree `e1297ea2b060da61c80e1648bf1afd0233522e19`).
Packet: `docs/orchestration/tasks/ui-magnifier-mac-1.md` on `task/UI-MAGNIFIER-MAC-1`.

**Verdict: ACCEPTED WITH NOTES (exact-head CI `44713929677` 12/12 GREEN; live macOS/Metal confirmation deferred to the Candidate 10+ Mac `GATE-RUN` per packet).**

---

## What Was Checked (directly in diff `488584f9..e2e4b617`, scripts, gate log, and direct binary verification)

1. **`src/app/src/dev_script.cpp` (`drawable_to_window_point`, `L323-330`):**
   - All 7 raw-pixel script verbs (`click_px`, `hover_px`, `ldown_px`, `ldrag_px`, `rclick_px`, `rdrag_px` in `L642-735`, and `wheel N X Y` in `L821-852`) now convert drawable pixels to SDL window points by dividing by `st.density` (`double` division cast to `float`, failing the step on `!(density > 0.0F)`).
   - Matches `DevScript::window_point` (`L465-478`), `sail`/`click-land`/`hover-*` (`L752-766`), `ship_point` (`viewer.cpp:3258-3267`), and real SDL user events consumed by `viewer.cpp` (`* density`). `density == 1.0` and real user mouse input are unchanged.

2. **`src/app/include/anno/app/dev_script.hpp` (`L151-155`) & `src/app/src/viewer.cpp` (`window_points_for_drawable`):**
   - Extracted shared `window_points_for_drawable(drawable_w, drawable_h, density)` (`std::max(1, lround(px / d))`) and applied it across `act.resize` (`viewer.cpp:3360-3363`), `ANNO_VIEW_SIZE` (`viewer.cpp:1132-1134`), and `resize_at_` (`viewer.cpp:1415-1417`).
   - Audited all 36 `resize` directives across `tools/test/viewer_wiring/*.script` (31x `1024 768`, 3x `2542 1351`, `1100 640`, `1280 720`, `960 600`): every script targets drawable pixel dimensions.

3. **Visibility & acquire-backoff diagnostics (`src/app/src/viewer.cpp`, `dev_script.hpp:157-160`, `dev_script.cpp:377-381`):**
   - `Viewer::handle()` (`L1575-1615`) guards `raw_density > 0.0F ? raw_density : 1.0F` and prints/traces `hidden=1` (`minimized`/`occluded`/`hidden`) and `hidden=0` (`restored`/`shown`/`exposed`) on state transitions only (no repeated `EXPOSED` log spam).
   - `back_off()` (`L990-996`) records `retry_backoff 100ms` once on transition into backoff.
   - `script_tick()` (`L3009-3014`) appends `format_no_frame_diagnostic(hidden_, retry_at_.has_value(), drawn_frames_)` to the 30 s no-frame abort message without weakening the timeout or skipping macOS checks.

4. **Sim / Formats / Pins:**
   - `git diff --stat 488584f9..e2e4b617 -- src/sim src/formats fixtures` is empty (`kStateHashVersion = 139`, 0 pin moves, 0 fixture changes). `OQ-795..797` remain unused.

5. **Tests, Negative Control & Gates Verified:**
   - `src/app/tests/dev_script_test.cpp` (`L713-841`): `dev script: density scaling for px, wheel, resize` verifies `density` in `{1.0F, 1.5F, 2.0F}` round-trips for all 7 verbs, real `data/ui/hud.txt` `lupe_minus` `(835, 163)` and `lupe_plus` `(805, 145)` hit-testing through `ui_transform` / `place_hud` / `ui_hit`, `window_points_for_drawable`, and `density == 0.0F` step failure; `dev script: no-frame abort diagnostic` covers all three diagnostic flags.
   - Deliberate failure proof reported in `REPORTS.md`: unscaled `drawable_to_window_point` mutant passed at `1.0F` and failed at `1.5F`.
   - Windows PC MSVC Release + RTX 4090 gate (`cache/gates/ui-magnifier-mac-1-02.log`): all 8 required contracts (`viewer_wiring_ui_magnifier_range`, `viewer_wiring_ui_magnifier_range_stepped`, `live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`) + selection passed (`COUNTS: selected=111 executed=111 failed=0 empty=0 skipped=0 incomplete=0 sanitizer_lines=0`, `VERDICT: PASS`).
   - Direct orchestrator verification on the built Release binary (`build/msvc/src/app/Release/anno_app_input_tests.exe --test-suite=dev_script` with `ANNO1503_GAME_DIR=E:\anno1503\game\app`): `13/13` test cases passed (`439/439` assertions, `0` failed); `clang-format 23.1.3 --dry-run --Werror` clean on all 4 touched C++ files.
   - WSL `tools/check_unix_warnings.sh`: reported clean across GCC 13.3.0 & Clang 18.1.3 (`435` files w/ GPU, `92` w/o GPU).
   - **Exact-head GitHub Actions CI run `44713929677` on `e2e4b617f6a3e7e748984102ec591e007a096d7b`: 12/12 jobs GREEN** (`changes`, `lint`, `cpp-format`, `build-macos`, `test (ubuntu-24.04)`, `test (windows-latest)`, `cpp (linux-gcc)`, `cpp (linux-gcc-release)`, `cpp (linux-clang-asan)`, `cpp (macos-clang)`, `cpp (windows-msvc)`, `gate`).
   - `task/UI-MAGNIFIER-MAC-1` synced at `e2e4b617f6a3e7e748984102ec591e007a096d7b` across `origin`, `gitlab`, and `share`.

---

## Non-Blocking Notes (No Fix Round Required)

1. **Local `run_selected.py` regex missed `anno_app_input_tests.dev script: ...` (closed by CI + direct run):**
   - The packet's own `--select "dev_script|..."` used an underscore while CTest registers doctest names with a space (`anno_app_input_tests.dev script: ...`), so `cache/gates/ui-magnifier-mac-1-02.log` did not include the `anno_app_input_tests` binary (the binaries built at `10:26` were unmodified thereafter). Closed by exact-head CI `44713929677` (all 5 C++ legs ran the full suite green) plus the orchestrator's direct `anno_app_input_tests.exe --test-suite=dev_script` run (`13/13`, `439/439` assertions with game files). Future packets touching `dev_script_test.cpp` should use `dev[ _]script` or `--require-test`.
2. **Minor comment/report wording nits:**
   - `src/app/include/anno/app/dev_script.hpp:48` header doc still reads `drawable of W x H points` (instead of `pixels`).
   - `REPORTS.md` deliberate failure proof cites `--test-case="*display density*"` from before the test case was renamed to `"dev script: density scaling for px, wheel, resize"` for `CheckTestNames.cmake`.
3. **Live macOS/Metal verification boundary (`NOT_RUN`):**
   - Expected per packet (executed on the Windows PC UI lane). Live Metal confirmation of `viewer_wiring_ui_magnifier_range` and `_stepped` at `density = 2.00` occurs in the Mac `GATE-RUN` after Candidate 10 (`95897a55`) is stacked with `UI-MAGNIFIER-MAC-1` (`e2e4b617`).
