# UI-INFO-2d: OBJECTPOS in formats, arrows on a placed building's picture, the preview's other sprite sets

**Released 2026-11-17** on the owner's go, relayed by the orchestrator: asked "go for UI-INFO-2d ..., UI-GHOST-1
and UI-BUILDCAT-1, in that order?", the owner answered "Yes". Branch `task/UI-INFO-2d`, based on
`task/UI-INFO-2b` `1a6f5ef5` (accepted, landing). Worktree `E:\anno1503-worktrees\anno1503-UI-INFO-2d`.
About 3-4 h, in three steps; report after each.

**Reference:** `docs/BUILDINGSOBJECTS.md` on `task/BUILDINGS-REF-1` (`cc2c6e80`). Read part C (arrows) first; it is
the owner's standing reference for building work.

## Scope

1. **Move the OBJECTPOS walk into `formats`** (orchestrator, binding).
   - Today `info_preview::entrance_arrows` (`src/app/src/info_preview.cpp`) walks the Anlagen `OBJECTPOS`
     sub-records (6-byte name, u16 size, data; `CONPO2` = 3 bytes per arrow: x and y in thirds of a tile,
     side) from raw bytes inside `src/app`. CLAUDE.md requires original file layouts to be read field by
     field in `formats`.
   - Add an `OBJECTPOS` sub-record reader to `src/formats` (the Anlagen/figuren module that already reads
     `ANLAGEN/ENTRY`), returning `Result`. It decodes `CONPO2` into a list of (x3, y3, side) and keeps unknown
     sub-records (`SCHUSS`, the old `CONPOS` of 1542) as raw bytes.
   - Add a writer for the round trip.
   - Tests:
     - synthetic fixtures;
     - a corpus test over every type in `Anlagen.dat` (184 types carry CONPO2, 852 arrow records,
       BUILDINGSOBJECTS part A). Each round trip is byte-identical; side bytes > 3 (types 261, 1571) and
       CONPOS (1542) are kept, not dropped.
   - Python oracle: `tools/figuren.py` or `tools/ui_layout.py` already decode it, so make `agree.py` agree
     if `anno-dump` prints it.
   - `src/app` then works on the decoded list only. The preview pixels stay identical: the
     `info_panel_gog_compare` stalls stay at 0 px.
2. **Arrows on a placed building's info-panel picture.**
   - The picture composer View `0x1001d760` has 8 callers besides the build panel, and all of them add the
     arrows (BUILDINGSOBJECTS part C §4, OQ-736).
   - Find which rebuild panels show a placed building's picture (the selected-house panel, production
     panels; `selected_house_pixel_check`, `docs/re/ui.md`) and draw the same arrows there, through the
     same code as the build preview.
   - Evidence: HE `c6_farmpanel.png` (a placed Kleine Farm's panel with arrows) in
     `E:\1503_test_videos\he_01_work\`.
   - If a placed panel isn't built yet in the rebuild, say so and skip it.
3. **The preview's other sprite sets (OQ-712).**
   - Compose the type's further sprite sets into the picture as View `0x1001d760` does (the market's and
     Kontor's flags, 801's second set, the type height).
   - Read the composer's set loop by address. Measure against the GOG preview shots: `info_panel_gog_compare`
     for 1301, 1302 and 801 should improve, and no type may get worse.

**Not in scope:**
- Arrows on the map ghost: the original draws none (OQ-713 closed, BUILDINGSOBJECTS part C §2).
- The scaler's pixel method (OQ-612).
- The category column (UI-BUILDCAT-1).
- The ghost's look (UI-GHOST-1).

## Gate and rules

- OQ ids: OQ-725, OQ-750 and OQ-755 (OQ-751..754 belong to another lane); ask the UI lane for more.
- GOG code by address (`py -3.14 E:\anno1503-worktrees\anno1503-UI-STATUS-1\cache\ui-status-1\adis.py`); HE pixels
  only.
- Build (`cmake --preset msvc` if needed), then clang-format 23.1.3
  (`E:\anno1503-worktrees\.scratch-cfpkg`) on the changed files.
- Focused gate with `tools/test/run_selected.py`:
  - the formats tests, the new corpus test, the `anno-dump`/agree tests, the info preview and hud/ui
    `anno_app_tests` cases, `^viewer_wiring_ui_`, and the oracle tests (`live_screen_matches_oracle`,
    `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`,
    `selected_house_pixel_check`, `ui_panels_match_gog`), all required;
  - `--jobs 8`; a test refused for memory reruns alone.
- `check_unix_warnings.sh` in WSL, one run at a time.
- Failure proof per step.
- A REPORTS entry; push and mirror.
- The owner's viewer test: a placed farm's panel with its arrows, and the market preview with its flag.
- Update `docs/BUILDINGSOBJECTS.md` part C with what you learn: the owner's standing rule. If that file
  isn't on your branch yet, put the notes in your REPORTS entry under "for BUILDINGSOBJECTS".
