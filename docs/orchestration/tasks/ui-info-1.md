# UI-INFO-1: the building info panel's name, preview, costs and upkeep

**RELEASED 2026-11-14** by the UI lane (session "UI Executor", the owner's decision of 2026-11-14: one lane owns the
in-game UI so that it matches the original and the History Edition). Owner of the work: the UI lane's executor
subagent. Size: about 3 hours, one concern. Report READY FOR REVIEW; the UI lane's independent reviewer reviews it.

## Why

- ADR 0004 row 4 ("Building info / placement") is **Partial**: `draw_live_screen_xrgb` draws the panel's plate,
  close button and rotate arrows when `placement_open`, but not its name, preview, material costs, gold cost or
  upkeep, because no measurement pinned their positions (OQ-561; `ui_mock.hpp`, the `placement_open` comment).
- The owner's History Edition screenshots of 2026-11-14 show exactly this panel (Forsthaus: tools 2, wood 0, bricks 0,
  marble 0, gold 150, upkeep 12; Kontor: 5 / 12 / 0 / 0, 350, 15).
- The original GOG captures show it at 1024 × 768, for example
  `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\r2_info_forester_801.png`: a name plank with the
  building's name, a framed preview of the building, four material fields (icon + digits) in two rows, a gold row
  and an upkeep row, each on its own plank.

## Done means

In `--play` at 1024 × 768, clicking any unlocked cell of the construction menu shows the panel as the original does:
name, preview, the four material costs, gold cost and upkeep, drawn from live data (the type registry, never from
`data/ui/construction.txt`), on the CPU path and the GPU overlay path. Measured against at least three GOG oracle
shots, within the UI.1 oracle tolerance (`docs/re/ui.md` §2.8). OQ-561 is closed or narrowed with evidence.

## Base, branch, rules

- Worktree `E:\anno1503-worktrees\anno1503-UI-INFO-1`, branch `task/UI-INFO-1`, from `origin/main` `480fafe7`.
  This packet file is its first commit. Push the branch at each checkpoint (ordinary push; never `main`, never force).
- Read first: `CLAUDE.md`, `docs/testing/evidence-contract.md`, `docs/testing/README.md`, `docs/adr/0004-ui-toolkit.md`
  (rows 3–5, R1, R3), `docs/re/ui.md` §2 (esp. §2.5, §2.8), §6, `data/ui/README.md`, `data/ui/hud.txt`,
  `src/app/include/anno/app/ui_mock.hpp`, `src/app/src/ui_mock.cpp`, `src/app/src/construction_menu.cpp` and their
  tests. Follow the real callers in `viewer.cpp`; change only the draw call there if you must.
- **No sim change.** The UI reads the published snapshot / registry. A new read-only query is allowed if it changes
  no state, stream, hash or draw; name it. Anything else in `src/sim`: stop that step with a QUESTION.
- **Publication (GitHub is public):** no pixels, decoded images, screenshots or UI strings in git. `data/ui/` holds
  only ids, coordinates, sizes, file names and indices. Copy oracle shots you use into this worktree's own
  `cache/` (git-ignored); never edit another worktree.
- **Out of scope:** scaling to the window (UI-WINDOW-1, next in this lane), the Kontor/ship panels, the selected
  house, placement ghosts, cursors, `render/`, depth or zoom work (another lane owns those files).
- Evidence rule: every coordinate cites its shot and method (sprite-sad / glyph match) in `data/ui/` and
  `docs/re/ui.md`; a guess goes to `docs/open-questions.md`, not into code.

## Steps

1. **Measure** on ≥ 3 GOG shots with different digit counts (e.g. `r2_info_forester_801`, `r2_info_market_1301`,
   `f12_info_kontor_1302`, `r2_info_pioneer_house`): the name plank and its text box; the preview frame and where
   the building sprite sits in it (which `Anlagen` frame, zoom and rotation); the four material field planks, icons
   and digit boxes; the gold and upkeep rows. Find the planks' ACCESS ids in `fenster.inc`/`ToolGfx` by sprite-sad.
   Bind the digits by glyph matching (which `.zei` font, which alignment rule: left, right or centred, proved by
   two different digit counts). OQ-561's grey-fill problem: match the digits, not the grey-filled icon.
2. **The name:** which `Texte.dat` section and index gives a type's name; prove it on the shots' names.
3. **Draw** it in `draw_live_screen_xrgb` and `draw_live_screen_rgba` behind `placement_open`, from the live
   registry costs (`cell_info`'s data path). Upkeep for an unbuilt type: use a live source if one exists (OQ-563);
   otherwise a QUESTION with the options, and draw nothing for it.
4. **Tests:** the per-pixel oracle compare (existing pattern) for the panel region on ≥ 3 cells, CPU and GPU; a
   unit test of the digit layout rule for 1-, 2- and 3-digit values; skip with a message without `ANNO1503_GAME_DIR`.
5. **Look at it yourself** (board §1 item 10): run `--play`, open the construction menu, pick the forester, Kontor
   and market, capture the window (existing `window_capture`), put each next to its GOG shot, and say in the report
   what you saw. Captures stay in `cache/`.

## Gates and report

- `cmake --preset msvc` + `cmake --build --preset msvc` in this worktree; the app tests that cover what changed
  (`ctest --preset msvc -R <pattern> -j 16`, `ANNO1503_GAME_DIR=E:\anno1503\game\app`, `ANNO_TEST_THREADS=16`),
  counts classified (executed pass / skip / fail); clang-format 23.1.3 on changed files; `wsl -d Ubuntu -- bash
  tools/check_unix_warnings.sh`; hosted CI green on the pushed head. The whole suite is NOT this packet's gate.
- One entry at the bottom of `docs/orchestration/REPORTS.md` on this branch, ≤ 60 lines: what changed, gate commands
  with classified counts, NOT_RUN, Decisions and reasoning, QUESTIONs. Logs stay in `cache/`.
- No reviewer subagents of your own. Stop at about 6 hours at a clean point and report what is left.
