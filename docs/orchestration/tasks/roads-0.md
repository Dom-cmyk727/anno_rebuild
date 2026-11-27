# ROADS-0 PICTURE-OBJECTS-1: runtime tile objects in the picture

**Released 2026-11-19** (owner "Yes, go" via the orchestrator; ui_10 finding 2). Branch `task/ROADS-0`, base `60a26f0d`
(candidate 10 + BUILDINGS-REF-2), worktree `anno1503-ROADS-0`. About 4-5 h. OQ range: 775-779.

## Why
The sim already places land roads (`Applied`, 5 gold per tile), but the picture's delta carries buildings only
(`BuildingDelta`), so a placed road is `NoVisualChange` and nothing is drawn (BUILDINGSOBJECTS §10.1 gap table, §10.2).
The owner sees no road; trees under a new building stay drawn.

## Do
- The picture takes tile-object deltas next to the building list: the sim thread diffs the island's tile objects
  (road ids with rotation first; design so crops and trees fit later) and the window thread draws a road tile from
  `(id, rotation)` with the existing sprite path (shipped maps already draw loader roads). Tile objects removed by a
  placement (trees, fields, stones; §10.2) disappear from the picture; a demolished footprint shows bare ground.
- `visibility` must report a visual change for road placement (title/`NoVisualChange` logic in `viewer.cpp` ~527).
- No new limits (types, ids data-driven). Keep sim untouched except a read-only delta source if needed; sim lane
  tests only if sim files change.
- COORDINATION: the Windows executor does WATER-SUBMERGED-1 in `gpu.wgsl`/`gpu.cpp` (sea passes). Keep your render
  changes off the sea passes; if a file overlaps, say so in the report. Do not edit `sea_submerged_fs` or the water
  pass setup.
- Tests: a unit test of the tile-object diff; a wiring case (place a road via the dev tool, the picture delta is not
  `NoVisualChange`, a new fact such as `picture.tile_objects`); a pixel check that a placed road tile differs from
  the bare tile; a case that trees under a placed building vanish; failure proof.

## Common rules (all lane packets)
- Reference: `docs/BUILDINGSOBJECTS.md` part B §10 (BUILDINGS-REF-2, evidence tags D/P/L/U) is the standing reference; update it with what you settle (tags), do not contradict it silently. OQ numbers only from your range. Original/HE behaviour is the base; where the owner could perceive a feel change, add a `rules` option defaulting to the original and list it in the report. No original pixels/text/data in git.
- Gate: affected `anno_app_tests`/sim/render cases, `^viewer_wiring_ui_`, the six oracle tests (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`) all required, `tools/test/run_selected.py --jobs 8 --require-test ...`, env as in the UI-CATNAV-1 packet (do NOT set ANNO_DEV_GOODS_TONS for gates); clang-format 23.1.3 (`E:\anno1503-worktrees\.scratch-cfpkg`); one WSL `check_unix_warnings.sh`; failure proof; REPORTS entry; push and mirror (`python -I tools/orch/sync_gitlab.py --repo .`); exact-head CI is the orchestrator's. One heavy job at a time on the PC (check cl/link/WSL first).
- Owner check is in the next integration build with plenty of resources (`ANNO_DEV_GOODS_TONS=2000`, `ANNO_DEV_GOLD=100000000`).
