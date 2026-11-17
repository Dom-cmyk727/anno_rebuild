# UI-INFO-2b: the build preview's entrance arrows and one scale for all four turns

**Released 2026-11-17** on the owner's go, relayed by the orchestrator: the owner answered "all of them"
to the he_01 packet list, in order. Branch `task/UI-INFO-2b`, base origin/main `4dfdd9b7`, worktree
`E:\anno1503-worktrees\anno1503-UI-INFO-2b`. About 3 h; one concern: the preview picture.

## Why

- The owner's tests `ui_6.mp4` (2:32, 2:58) and `ui_7.mp4`: "entrance arrows still missing" in the build
  panel's picture. Rotation itself works (UI-INFO-2).
- His HE recording `he_01.mp4` shows them. The orchestrator's analysis is
  `E:\1503_test_videos\he_01_work\he_01-report.md`, with these crops:
  - `c7_marketpanel.png`: the Markthaupthaus at 3:57, with green arrows on the picture, the rotate arrows
    and the costs (tools 3, wood 7, bricks 0, stone 0, gold 250, upkeep 10);
  - `c6_farmpanel.png`: the Kleine Farm at 8:05;
  - `s3_market.png`, `s4_ghost_times.png`: arrows and rotation on the ghost.
- The 2026-11-14 HE video analysis: the green entrance arrows turn with the picture through the 4 turns
  (`E:\anno1503-worktrees\he-ui-captures\he_ui_1_video\`, `c_pion_rot4.jpg`).

## Scope (from UI-INFO-2's "next packet UI-INFO-2b", `docs/orchestration/tasks/ui-info-2.md` on main)

1. **Entrance arrows on the preview.**
   - Read in GOG `View.dll` how the preview composes them: the preview builder `0x1001d760` (OQ-624) and
     the panel builder `0x1002fb30..0x10030138`.
   - Find which sprite and texture they come from, where each arrow sits (from the building's entrance
     tiles in `Anlagen.dat`/`Figuren`), and how they turn with the rotation.
   - Draw them in the CPU and GPU UI paths, for every rotation.
   - Whether the map ghost's arrows use the same data: note it, and draw them on the ghost only if the
     code read shows the same path. Otherwise make it a QUESTION for the UI lane.
2. **One scale for all four turns (OQ-612, OQ-624).**
   - `0x1001d760` scales by `max(1, W/135, H/135)` over the largest screen rectangle of the parts across
     all four turns, and composes into a W × H canvas before the box.
   - Use that rule instead of the per-turn fit, so a scaled type keeps its size when turned.
   - Where the picture sits at rotations 1-3 (OQ-624): read it, or keep today's rule and leave the OQ
     open with what you found.
3. **The category column's hit width.** The 42 px `VIEWBAUTAB` hit rectangles overlap grid column 0
   (x 808-820). Read the original's hit test and fix it if the code shows a narrower one.
4. **Cross-check** the info panel's figures against `c7`/`c6`: costs, gold, upkeep and the farm's two
   outputs. Report any differences; fix them only if they are in this packet's code.

**Not in scope:**
- The scaler's pixel method for sprites over 135 (OQ-612's placeholder stays unless the code read
  settles it cheaply).
- Tooltips (OQ-541).
- The range display (UI-RANGE-1).
- The productivity bar (UI-PRODBAR-1).

## Evidence and tests

- GOG code by address; HE pixels only. If a picture detail needs a new HE capture, write a short recipe
  for the owner; never start HE yourself.
- Tests:
  - the existing preview checks (`info_panel_gog_compare`, the rotation shots under
    `ANNO_UI_ROTATION_SHOTS`) must not get worse; record any improvement;
  - a new check that the arrows are drawn for a type with entrances at all four turns, against the GOG
    rotation shots if they show arrows, otherwise against the HE crops with a stated tolerance;
  - a wiring case for the hit width, if it changes.
  - Failure proof for the arrows check.
- Gate: the focused UI gate with every oracle-shot test executed (`live_screen_matches_oracle`,
  `ui_dynamic_draw_check`, `info_panel_gog_compare`, `selected_house_pixel_check`,
  `ui_panels_match_gog`, the rotation tests), 0 skipped. Then `check_unix_warnings.sh` (one run at a
  time) and CI green.
- The owner's test: the build menu's preview for the Markthaupthaus, Kleine Farm and Pionierhaus at all
  four turns, side by side with `c7`/`c6`.
- Next free OQ numbers: check main and the open branches. UI-STATUS-1 and ISLAND-CROPS-1 use up to
  OQ-708, use OQ-710..714 (reserved for this packet; ask the UI lane for more).
