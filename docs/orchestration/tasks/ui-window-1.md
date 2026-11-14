# UI-WINDOW-1: the in-game UI at any window size, and clickable

**RELEASED 2026-11-14** by the UI lane (session "UI Executor"). Owner of the work: the UI lane's executor subagent.
Size: about 3–4 hours, one concern (where the UI sits and where a click lands). Report READY FOR REVIEW; the UI
lane's independent reviewer reviews it, then the board orchestrator.

## Why

- Owner playtest 2026-11-13/14: the in-game UI draws only at exactly 1024 × 768 and vanishes otherwise (maximized,
  any other size); "clicks on the UI never reach it. Nothing is clickable"; clicks on the chrome go to the map.
- Cause on main (`src/app/src/viewer.cpp`): `draw_cpu`, `draw_gpu` and `construction_handle` return early unless the
  drawable is exactly 1024 × 768 (`w == 1024 && h == 768`, "R3 still open"); the HUD-panel hit test in
  `selection_handle` has the same guard; the construction menu opens only by key B, never by a click.
- ADR 0004 R3 (the UI scale) is now decided by the owner (2026-11-14): **default 1.0, the original's 1:1 UI; the
  History Edition's scale rule as an option.** The HE rule is reverse-engineered and confirmed by three samples in
  `docs/re/he-ui-scale.md` (branch `task/UI-HE-1`, accepted by the board at `5d0db740`; read it there with
  `git show origin/task/UI-HE-1:docs/re/he-ui-scale.md` until it lands).

## Done means

In `--play` at any window size (and maximized), the HUD, the construction menu and the info panel are drawn and
clickable on the CPU and GPU paths:

- **Default (`ui scale 1.0`):** the UI is drawn 1:1. Right chrome at x = W − 245, top-anchored (HE `View.dll`
  0x3b44c; GOG's 779 at 1024 is the same value); status bar bottom-anchored; at exactly 1024 × 768 the output is
  pixel-identical to today's (the existing oracle tests stay green unchanged).
- **Option `--ui-scale <s>` (1.0..2.0, HE rule):** at 1200 lines or fewer the scale is 1.0; otherwise step down from
  s by 0.1 until the UI keeps at least 1024 × 768 UI pixels; the UI layer is uiW × uiH and is stretched uniformly
  and bilinearly to the window (he-ui-scale.md §1–2). At client 2542 × 1351: s 2.0 → k 1.7003, panel x 2125.4;
  s 1.5 → k 1.5006, panel x 2174.4.
- **Clicks** map back with `trunc((p − off) / k)` (HE `AnnoFrame.dll` 0x22a26) before `ConstructionMenu::click` and
  the HUD hit test. A left click (press and release) on any drawn chrome never reaches the map: no pan, no
  selection, no tool action.
- **The construction tab** (`hud.txt` `tab_construction`, hotkey B) opens and closes the construction menu on a
  click, like key B.

## Base, branch, rules

- Worktree `E:\anno1503-worktrees\anno1503-UI-WINDOW-1`, branch `task/UI-WINDOW-1`, from `83a0adff` (main
  `95eea7de` + UI-INFO-1). This packet file is its first commit. The base already contains UI-INFO-1's code; when
  `origin/main` moves (UI-INFO-1 is landing now through `land/ui-info-1`), merge `origin/main` at your next clean
  checkpoint and re-run the focused tests. Push the branch at each
  checkpoint (ordinary push; never `main`, never force).
- Read first: `CLAUDE.md`, `docs/testing/evidence-contract.md`, `docs/testing/README.md`, `docs/adr/0004-ui-toolkit.md`
  (R1, R3), `docs/re/ui.md` §2, §2.8, §5, `he-ui-scale.md` (above), `data/ui/README.md`, `data/ui/hud.txt`,
  `ui_layout.hpp/.cpp`, `ui_mock.hpp/.cpp`, `construction_menu.hpp/.cpp`, `viewer.cpp` (`handle`,
  `construction_handle`, `selection_handle`, `draw_cpu`, `draw_gpu`) and their tests.
- **The owner's evidence rule (2026-11-14), binding:** when in doubt, check both the original (GOG `game/app`) and
  the History Edition (`E:\SteamLibrary\steamapps\common\Anno 1503 - History Edition`): their files and the code that
  draws the UI, and zoomed visual captures. The original is the bit-identical base; HE behaviour is an option. Every
  rule you implement cites its evidence (file, module + address, or capture); a guess goes to
  `docs/open-questions.md`, never into code as fact.
- **No sim change, no `src/render` change.** The GPU path already blends a full-frame RGBA overlay; produce that
  overlay at the window size in app code (for k ≠ 1, resize the UI layer bilinearly in app code and measure its
  cost per CLAUDE.md "measure every performance change"). If something truly needs `render/`, stop that step with
  a QUESTION.
- **No hardcoded limits:** the 1024 × 768 reference, 245, 0.1, 1.0..2.0 and 1200 are data or config with the
  original/HE values as defaults (`data/ui/` or the app's UI config), each with its evidence.
- **Publication (GitHub is public):** no pixels, screenshots or UI strings in git. HE captures live in
  `E:\anno1503-worktrees\he-ui-captures` (read-only for you, outside git); GOG shots in
  `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\`; copy what you use into this worktree's `cache/`.
  Do not start or drive the HE or the original game; if you need a new capture, ask in the report.
- **Out of scope:** the other three tabs' screens, the minimap's content and clicks (next packets: UI-WINDOW-2 tabs,
  UI-MINIMAP-1), new screens, the preview (OQ-612), zoom/depth/render work (another lane).

## What is known and what is not (register what you depend on)

Known: the right chrome rule (above); the step-down, stretch and click rules; the HE at 1.0 on a 2542 × 1351 window
(capture `he_s10_hud_idle_win.png`): the status bar runs from x 0 to the panel's left edge (W − 245), bottom
row at H − 1, with `fragezeichen` at its right end just left of the panel; below the panel's 768 rows the column is
filled with a picture (stone ledge, chain, posts). GOG `fenster.inc` has `MAX_PIC` id 5, 245 × 524 (`data/ui/inc.txt`),
a likely candidate; the original offered display modes up to 1280 × 1024 (`ui.md` §5), so GOG code must place it.

Not known, each to be settled with evidence or registered as a new OQ in `docs/open-questions.md` in this packet
(next free numbers after OQ-613; the board asked for this):
1. How the status bar fills W − 245 (tiled, stretched, or a repeated middle piece) and where `fragezeichen` and the
   counters sit at other widths. Check GOG `View.dll`'s status bar draw and the HE capture.
2. The panel filler below row 768: which graphic, where, tiled or once (check `MAX_PIC` against the HE capture
   by sprite-sad; check GOG code).
3. Pop-ups (the pause parchment, `MESSAGEBOX_*`): placement at other sizes. HE `View.dll` 0x1f3d4 clamps windows to
   width `Sizex`, height `min(Sizey, 768)` (inferred). If unproven: keep the 1024 × 768 position, flag it, register it.
4. Windows smaller than 1024 × 768: HE clamps its layers up to 1024 × 768 and fits them into the client. Rebuild
   choice: draw the UI layer at 1024 × 768 and fit it down with the same aspect-fit formula; register it.
5. Whether HE's 1200-line rule reads the client height or a listed display mode (only for the option).

## Steps

1. **The transform (pure, tested):** a `UiTransform` from (client W, H, rule, setting) to uiW, uiH, k, offX, offY,
   in float32 with truncation exactly as `he-ui-scale.md` §2 states (`cvttss2si`), and its inverse for clicks.
   Unit tests: the prediction table of he-ui-scale.md, 1024 × 768, 1920 × 1080 (forced 1.0 under the option),
   a window below 1024 × 768, and the round trip of a click through the transform.
2. **Anchoring:** give each `hud.txt` element an anchor (right chrome: x relative to the right edge; status bar row:
   y relative to the bottom; the rest by its evidence), as data. At 1024 × 768 every element keeps its position
   (prove with the existing oracle tests). Fill the status bar and the panel column below 768 per items 1–2.
3. **Draw** through the transform on both paths; remove the `1024 × 768` guards in `draw_cpu` and `draw_gpu`.
4. **Clicks:** `construction_handle` and the HUD hit test in `selection_handle` use the inverse transform; remove
   their guards; the hit area covers all drawn chrome, including the status bar and the filler. The construction
   tab click toggles the menu. A click on the chrome is consumed on press and release (no pan starts).
5. **Option plumbing:** `--ui-scale` (and the matching config key if the app has one), default 1.0, documented in
   the app's help and in ADR 0004 R3 (record the owner's decision there, with the date).
6. **Tests:** existing oracle tests unchanged at 1024 × 768; a test that at 2542 × 1351 / s 1.0 the right chrome's
   pixels equal the 1024 × 768 chrome shifted by W − 1024; a test that at s 2.0 the tab band compares to the GOG
   band resized by k 1.7003 within the bilinear tolerance used in he-ui-scale.md (or tighter); click tests for a
   construction cell and the tab at 1024 × 768, 2542 × 1351 s 1.0 and s 2.0; a test that a click on the status
   bar and on the filler is consumed. CPU and GPU paths.
7. **Look at it yourself** (board §1 item 10): run `--play` at 1024 × 768, maximized, and a small window, with s 1.0
   and s 2.0; capture the window (existing `window_capture`), put the s 1.0 and s 2.0 captures next to
   `he_s10_hud_idle_win.png` / `he_s20_hud_idle_win.png`, click the tab and a cell, and say in the report what you
   saw. Captures stay in `cache/`.

## Gates and report

- `cmake --preset msvc` + `cmake --build --preset msvc` in this worktree; the app tests that cover what changed
  (`ctest --preset msvc -R <pattern> -j 16`, `ANNO1503_GAME_DIR=E:\anno1503\game\app`, `ANNO_TEST_THREADS=16`,
  `ANNO_UI_ORACLE_SHOTS` pointing at your `cache/` copy of the GOG shots), counts classified (executed pass / skip /
  fail); clang-format 23.1.3 on changed files; `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`; hosted CI green
  on the pushed head. The whole suite is NOT this packet's gate.
- One entry at the bottom of `docs/orchestration/REPORTS.md` on this branch, ≤ 60 lines: what changed, gate commands
  with classified counts, NOT_RUN, Decisions and reasoning, the OQs you registered, QUESTIONs. Logs stay in `cache/`.
- No reviewer subagents of your own. Stop at about 6 hours at a clean point and report what is left.
