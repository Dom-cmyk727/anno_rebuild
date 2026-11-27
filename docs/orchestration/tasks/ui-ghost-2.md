# UI-GHOST-2: ghost hides objects under the footprint, refusal rectangle, occupied-footprint outline

**Written 2026-11-19, release after ROADS-0 is accepted** (owner "Yes, go"; stack on ROADS-0). Branch
`task/UI-GHOST-2`, worktree `anno1503-UI-GHOST-2`. About 2-3 h. OQ range: 790-794.

## Do (ui_10 findings 7, 9; BUILDINGSOBJECTS §10.2)
- While a building ghost is shown, trees/fields/stones under its footprint are not drawn (HE 9:45, 9:58); after
  placing, those under the footprint are gone, the rest reappear. Use ROADS-0's tile-object picture path; a ghost
  cancel restores them.
- A refused ghost draws the yellow empty rectangle where HE does (check the GOG draw `0x10041050` and HE frames).
- The `Occupied` refusal: show which footprint blocks (the occupied building's footprint outlined) so a refusal on
  free-looking land is explained (the Kontor's 4x4 footprint is larger than its sprite, OQ-769). This is an
  ADAPTATION, label it; default off unless the original shows it (then default on).
- Tests: wiring case (ghost over a tree tile hides it; cancel restores), pixel check, failure proof; GHOST-1
  tests stay green.

## Common rules (all lane packets)
- Reference: `docs/BUILDINGSOBJECTS.md` part B §10 (BUILDINGS-REF-2, evidence tags D/P/L/U) is the standing reference; update it with what you settle (tags), do not contradict it silently. OQ numbers only from your range. Original/HE behaviour is the base; where the owner could perceive a feel change, add a `rules` option defaulting to the original and list it in the report. No original pixels/text/data in git.
- Gate: affected `anno_app_tests`/sim/render cases, `^viewer_wiring_ui_`, the six oracle tests (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`) all required, `tools/test/run_selected.py --jobs 8 --require-test ...`, env as in the UI-CATNAV-1 packet (do NOT set ANNO_DEV_GOODS_TONS for gates); clang-format 23.1.3 (`E:\anno1503-worktrees\.scratch-cfpkg`); one WSL `check_unix_warnings.sh`; failure proof; REPORTS entry; push and mirror (`python -I tools/orch/sync_gitlab.py --repo .`); exact-head CI is the orchestrator's. One heavy job at a time on the PC (check cl/link/WSL first).
- Owner check is in the next integration build with plenty of resources (`ANNO_DEV_GOODS_TONS=2000`, `ANNO_DEV_GOLD=100000000`).
