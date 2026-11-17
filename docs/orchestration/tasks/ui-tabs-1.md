# UI-TABS-1: the side panel's sub-tabs, and the player overview reachable again

**Released 2026-11-16** by the UI lane on the owner's go ("Yes, start it"). Branch `task/UI-TABS-1`, base
origin/main `ac02d4a2`, worktree `E:\anno1503-worktrees\anno1503-UI-TABS-1`. Stage A is released; B waits
for the UI lane's review of A and the owner's test.

## Why

The owner's playtest `ui_4.mp4` (2026-11-15, rebuild on main cc7af72c; frame report in the UI lane's
scratchpad, `ui_4-report.md` lines 14-31, 168-256, 290-319) found:

- every **sub-tab** in the column left of the right panel is dead: the player page's 5 (`VIEWINFOPIELERTAB`),
  the "?" page's 8 (`VIEWINFOSTADTTAB`) and the gear page's 5 (`OPTION_TABS`); no highlight, no page change;
- the **player overview** (the start state: the player tab with its first sub-tab and the balance rows,
  `hud_idle_running`) cannot be reached again once left: the bust tab and key D always open the list
  (`UiMockScreen::PlayerList`), and no sub-tab click works;
- the overview's balance rows and the "?" page's value boxes are empty (live figures).

Code on main: `Viewer::select_hud_tab` (`src/app/src/viewer.cpp`) only knows four screens (`Hud`,
`PlayerList`, `Info`, `MainMenu`); the left-click path resolves the four main tabs, the minimap and the
construction menu, and takes every other chrome press without acting. `data/ui/hud.txt` lines 162-205 hold
the three views' static chrome with their sub-tab frames as measured on the GOG shots.

Owner rules: the History Edition is the picture reference (pixels only), the GOG DLLs are for reading code;
the original UI bit-identical is the base; placeholders are allowed where a page's body is not yet built
(owner, 2026-10-30), never invented content; small steps, the owner tests each.

## What is NOT in this packet (it is in a prepared panel packet, branch `task/UI-PANELS-PREP`)

- The bodies of the sub-pages: player view tabs 1-4 (diplomacy, city list, ship list, ships for purchase):
  `ui-ships-diplomacy-1.md` 1d/1g; "?" (city view) tabs 1-7 incl. Sell/Buy: `ui-warehouse-1.md`; gear tabs
  1-4 (load, save, music, speech) and the gear page's icons (chart, book, laurel, door):
  `ui-options-mainmenu-1.md` S2/S4, `ui-statistics-1.md`.
- Tooltips and hover (nothing has them yet; the status bar and help book showed none in `ui_4`): a later
  packet of its own.

This packet builds the **switching** those packets will plug their bodies into, so none of them has to.

## Stage A: sub-tab switching in all three views (2-4 h, code)

1. **Evidence first (read, no guessing).**
   - GOG code: the player view's tab handler `0x1007f730` (View.dll; `docs/re/ui-ships-diplomacy.md` §1
     "Sub-tab switching"), the city view's tab handler (find it the same way as the ship view's
     `0x10057560` → `0x10057420`), the options page's `0x100786a0` (`docs/re/ui-options-mainmenu.md` §2.6).
     For each: which index is stored where, what is shown and hidden, whether a click on the selected tab
     does anything, and which tab a main-tab click or key selects (D, S, C; the bust from the overview;
     the "?" and gear tabs on each visit; does a view remember its last tab?).
   - Frame ids: confirm the idle/selected pairs from the `fenster.inc` ACCESS ranges (from `hud.txt`:
     player tab k idle 1184 + 2k, selected 1185 + 2k; options idle 1476 + 2k, selected 1477 + 2k; the
     city view's 661/662-675 pattern needs checking: `info_subtab_0` is 662).
   - The History Edition as reference: `E:\1503_test_videos\HE_UI_1.mp4` and `ui_5.mp4` (owner's HE runs),
     the lossless captures in `E:\anno1503-worktrees\he-ui-captures\` (also `c_tabs_a.jpg`/`c_tabs_b.jpg`,
     `he_108.0.png`, `he_109.25.png`, `he_111.5.png`), the analyses `he_ui_1_video\` and
     `E:\1503_test_videos\ui_5_work\analysis\ui_5-report.md`. Note per behaviour: frame and timestamp,
     confidence. HE ids differ from GOG ids (`ui-ships-diplomacy.md` §2): match by ACCESS name + index.
   - Write the result to `docs/re/ui.md` as a new §5.4 "The sub-tabs" (addresses, frames, evidence per
     row); anything not evidenced becomes an OQ (next free numbers OQ-641..649 are free; check
     `docs/open-questions.md` on main first) and its default here is the GOG code's behaviour where read,
     else the minimal one (selecting a tab shows it, nothing more).
2. **State.** Replace the four-screen model by a view + selected sub-tab per view (player 0-4, city 0-7,
   options 0-4), counts and frame ids as data in `data/ui/hud.txt` (no fixed arrays in code: CLAUDE.md "No
   hardcoded limits"). The overview is player tab 0, today's `player_list` is player tab 1.
3. **Clicks and keys.** A left press on a sub-tab of the showing view selects it (on press, as the main
   tabs). Keys D/S/C select player tabs as the code read says (spec: D → tab 1, S → tab 3, C → tab 2), only
   in the classic surface (as today's D). The bust main tab and the other main tabs select the tab the code
   read says. The player overview is reachable again (tab 0) by whatever the original uses: the sub-tab
   click at least.
4. **Drawing.** The selected tab draws its selected frame, the others idle, in the CPU and GPU UI paths.
   Bodies: tab 0 of the player view keeps today's `player` panel, tab 1 today's `player_list`, the "?"
   view's tab 0 today's `info`, the options tab 0 today's `main_menu`. Every other tab draws its view's
   background plate and its header/crest only, as a named placeholder (`hud.txt` comment + the packet that
   fills it), unless the GOG shots already measure its body (`f15_ship_list`, `f15_city_list`,
   `f15_options_y360/410/460/510`: if their chrome is already in `hud.txt` or the panel specs give
   measured rectangles with a shot, draw it; else placeholder).
5. **Facts and tests.**
   - A viewer fact for the showing view and its tab (e.g. `hud.view`, `hud.subtab`), printed by the dev
     script.
   - Unit tests for the state transitions (every sub-tab of every view, keys, main tabs, a click on the
     selected tab).
   - A viewer wiring script `tools/test/viewer_wiring/ui_subtabs.script`: start (overview), bust → list,
     sub-tab 0 → overview again, sub-tabs 2/3/4, keys S and C, "?" with its tabs 0..7, gears with 0..4,
     each step asserting the facts.
   - The oracle-shot tests that already cover these screens must still pass unchanged:
     `live_screen_matches_oracle`, `ui_panels_compare` (`tools/test/ui_panels_compare.py`),
     `ui_dynamic_draw_check`, `info_panel_gog_compare`; include them in the focused run.
   - Failure proof: revert the click handling, show which assertions fail.

**Accept A:** every sub-tab of the three views switches with the right selected frame in both paths; the
overview is reachable again; keys D/S/C as the code read says; no existing UI test changes its
expectation; §5.4 written with addresses; OQs for the rest.

**HE comparison for A:** side by side of our tab column on each view against the HE frames named in §5.4
(UI scale 1.0), selected frame position and look.

## Stage B: the overview's live figures (2-3 h, code; after A)

The player overview's balance rows (tab 0, `SPIELER_BILANZ_BK`) and the "?" view's tab 0 value boxes show
live figures as the original: which rows, which source, which font and position (code read +
`hud_idle_running`, `r2_tab_help`, HE captures). Snapshot-only reads (no sim change; a missing sim value
becomes a QUESTION to the orchestrator, not a guess). Written when A is accepted.

## Gates (each stage)

- `cmake --preset msvc`, `cmake --build --preset msvc`, warnings clean.
- `cmake -DSOURCE_DIR=<worktree> -P cmake/CheckTestNames.cmake` (no `* ? [ ] \ ;` in test names).
- Focused `ctest --preset msvc -R <pattern>` with `ANNO1503_GAME_DIR=E:\anno1503\game\app`,
  `ANNO_TEST_THREADS=16`, the GOG shots path and the HE captures path set as the existing UI tests read
  them; counts classified (executed/passed/skipped).
- clang-format 23.1.3 (`E:\anno1503-worktrees\anno1503-UI-INFO-2\cache\cfpkg\bin\clang-format.exe`).
- `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh` (known failure: `src/sim/tests/extensions_test.cpp`).
- `tools/test_ui_layout.py` if `tools/ui_layout.py` changes.
- One REPORTS entry of at most 60 lines at the bottom of `docs/orchestration/REPORTS.md`.
- No original pixels or text in git; no `src/sim`, `src/render`, `src/formats` change.
