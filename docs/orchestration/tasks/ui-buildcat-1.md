# UI-BUILDCAT-1: draw the construction menu's category column

**Released 2026-11-17, URGENT, on the owner's direct order in the UI lane's session:** "the side panel in the
construction menu is STILL missing, fix now". It was already approved as the third of the owner's "Yes" list, and is
moved ahead of UI-GHOST-1 stage B. Branch `task/UI-BUILDCAT-1`, base origin/main `4dfdd9b7`, worktree
`E:\anno1503-worktrees\anno1503-UI-BUILDCAT-1`. About 2-3 h. One concern: the category column.

**Reference:** `docs/BUILDINGSOBJECTS.md` (task/BUILDINGS-REF-1 `cc2c6e80`), part B §1 and §6 (the build menu and
which cursor each cell arms). Read it, and update it if you learn something (owner's standing rule).

## The gap

- **The owner's tests:** `ui_6`, `ui_9` (2:13-2:39, HE side by side at 2:30) and now again. The HE construction menu shows a
  column of eight category tabs left of the building grid. The rebuild draws none, although clicks on the column work.
- **The spec** (`docs/re/ui.md` line ~194): category column at (779, click_y − 23), 42 × 46, **eight gadgets**,
  `VIEWBAUTAB` ids 16, 18, …, 30 idle, +1 pressed/selected. `View.dll` `0x1002bb4f` copies `VIEWBAUTAB`, loop
  `cmp ebp, 8`.
  - `data/ui/construction.txt` holds the eight categories with their click y (325, 373, 418, 463, 508, 553, 598, 643).
  - `data/ui/hud.txt` has only `element viewbautab_0` at (779, 302) and `click category_column_x 782`.
- **Why the tests never caught it:** `docs/re/ui.md` line ~333 masks the column 29 × 400 at (779, 300) in the
  comparisons ("the 42 × 46 gadget is clipped"). Read that note before you change the mask.

## Do

1. **Read the original by address:**
   - View `0x1002bb4f` and its loop: the eight gadgets' positions, the texture id per category (16 + 2k?), which frame
     marks the selected category, and the hover/pressed state if any;
   - the visible part: is the gadget clipped to 29 px by the panel, and what draws over it;
   - the column's hit width and hit test (OQ-714, which moves here).
2. **Draw all eight tabs** in the CPU and GPU UI paths, whenever the construction menu shows:
   - the selected category's frame;
   - positions and ids as data in `data/ui/hud.txt` (or derived from `construction.txt`'s eight rows; no fixed
     count in code, per CLAUDE.md "No hardcoded limits").
3. **Fix the hit test** to the original's if the code read shows it differs from today's, so clicks on grid column 0
   (x 808-820) go to the grid, not the column (OQ-714).
4. **Tests:**
   - unmask the column in the oracle comparisons where the GOG shots show it (`r2_construction_*`), so
     `live_screen_matches_oracle` and `ui_panels_match_gog` check it;
   - a viewer wiring case that selects each category and reads the selected tab;
   - a hit-test case at the column/grid boundary;
   - a failure proof: the column check fails with the tabs not drawn.
5. **Gate:**
   - build, then clang-format 23.1.3 (`E:\anno1503-worktrees\.scratch-cfpkg`);
   - `run_selected.py --jobs 8`: the hud/ui/construction/selection `anno_app_tests`, `^viewer_wiring_ui_`,
     `^viewer_wiring_construction`, and the oracle tests (`live_screen_matches_oracle`, `ui_dynamic_draw_check`,
     `info_panel_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`, `ui_mock_matches_oracle`), all
     required;
   - WSL `check_unix_warnings.sh`, one run at a time;
   - a REPORTS entry; push and mirror.
6. **The owner's viewer test, with DEFAULT rules** (the developer city): open the construction menu and click through
   all eight categories. Compare with the HE frame `ui_9` 2:30 (`E:\1503_test_videos\ui_9_work\`).

OQ ids: OQ-759..763 (OQ-714's answer is recorded in OQ-714 itself). GOG code by address
(`py -3.14 E:\anno1503-worktrees\anno1503-UI-STATUS-1\cache\ui-status-1\adis.py`); HE pixels only. Never start the
original game or HE.
