# UI-CATNAV-1: a category tab returns the construction menu to its grid

**Released 2026-11-19** on the orchestrator's go ("an obvious bug fix"), from the owner's test `ui_10.mp4`
(report `E:\1503_test_videos\ui_10_work\ui_10-report.md`, finding 4). Branch `task/UI-CATNAV-1`, base candidate 10
`95897a55` (it has the eight category tabs), worktree `E:\anno1503-worktrees\anno1503-UI-CATNAV-1`. About 2 h.
One concern: what a click on a category tab does while a building's panel is showing.

**Reference:** `docs/BUILDINGSOBJECTS.md` part B §6 (the placement UI flow); `docs/re/ui.md` §4 and §2.4.

## What the owner saw (ui_10, 2:34-3:05 and 3:49-4:02)

- A Dirt road, Forester's hut or Sheep farm is armed, so the panel shows its picture, costs and rotate arrows.
- He clicks a category tab in the column left of the panel (the eight tabs from UI-BUILDCAT-1): the tab highlights,
  **but the panel keeps showing the armed building.** To get the grid back he has to press the X (the cancel
  plank). His words: "it should return to this ... I should not have to click a board or whatever that's called
  to get to that actual menu."
- "As soon as I click on road while having the Forester's hut selected it should leave that window, basically the
  same function as the abort."

## What the original does (HE, pixels only, ui_10 10:03-10:13)

After "I switch to roads", the panel shows the road grid ("on the right side the road to select") **while the
building stays armed** (the ghost is still the building). Picking a cell in the new grid arms that type. Read the
original's code before implementing: `docs/re/ui.md` §4 (the placement UI), View.dll `0x1002bb4f` (the category
gadgets) and its click handler, and the cursor input handlers in AnnoFrame (`0x10047470` and the others). Settle:
1. Does a tab click show the grid for that category and keep the armed type?
2. Does it disarm (the abort) when the new category's grid is shown? HE shows the building still armed.
3. The tab already showing: does a second click on it do anything?

## Do

- Implement the original's rule (HE as the picture reference). Where the code read and HE agree, that is the
  behaviour; where one is unread, follow HE and mark the code comment "HE-observed, GOG unread" with an OQ
  (OQ-773..774).
- The panel logic is in `construction_menu.hpp/.cpp` and `viewer.cpp` (`construction_menu_open_`, the category
  click path). Tests:
  - a unit test of the menu's state transitions: armed type + tab click, grid shown, armed type kept or cleared as
    decided;
  - a wiring case: arm the pioneer house, click each tab, read `ui.category`, `ui.category_tab`, the panel kind
    (a new fact `ui.panel` = `grid` / `info` if none exists) and `tool.armed` (check what the dev tool exposes);
  - a pixel check (`viewer_wiring_ui_buildcat` and `ui_panels_match_gog` must stay green).
  - Failure proof.
- Gate: the hud/ui/construction/selection `anno_app_tests` cases, `^viewer_wiring_ui_`, the six oracle tests
  (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`,
  `selected_house_pixel_check`, `ui_panels_match_gog`), all required, `--jobs 8`; clang-format 23.1.3
  (`E:\anno1503-worktrees\.scratch-cfpkg`); one WSL `check_unix_warnings.sh`; a REPORTS entry. One heavy job at a
  time on the PC (check cl, link and WSL first).
- The owner's check is in the next integration build, with plenty of resources: arm any building, click each tab,
  arm a road, click another tab.
