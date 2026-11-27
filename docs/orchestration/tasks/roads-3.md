# ROADS-3 UI-ROADS-1: press-hold L-drag road building in the menu

**Written 2026-11-19, release after ROADS-0 and ROADS-1 are accepted** (owner "Yes, go"; stack on them: merge both
into this branch as one clean merge commit first). Branch `task/ROADS-3`, worktree `anno1503-ROADS-3`. About 3-4 h.
OQ range: 785-789.

## Do (BUILDINGSOBJECTS §10.1 items 1, 2, 9, 10)
- In the menu-armed tool (`DevTool::arm` with 2520/2500): press stores the start tile; while held, every move
  recomputes the L path with ROADS-1's `road_path` (40-tile cap) and shows the live ghost of the whole path; release
  commits the valid prefix (kind 9); the type stays armed; right click disarms.
- The affordable valid prefix is drawn normally, the unaffordable tail blinks (reuse the GHOST-1 accumulator); no
  radius outline; no fertility bar; on refusal with no valid record the yellow empty rectangle (0xffff00) on the
  hovered tile (D `0x10041050`).
- Same random variant byte per tile for the preview and the commit (§10.1 item 3): a deterministic UI-side choice
  that is not in sim state.
- Tests: wiring case (press, move, release; `tool.armed` stays, path length, validity facts), pixel check of the
  ghost path, failure proof; `viewer_wiring_ui_ghost*` stay green.

## Common rules (all lane packets)
- Reference: `docs/BUILDINGSOBJECTS.md` part B §10 (BUILDINGS-REF-2, evidence tags D/P/L/U) is the standing reference; update it with what you settle (tags), do not contradict it silently. OQ numbers only from your range. Original/HE behaviour is the base; where the owner could perceive a feel change, add a `rules` option defaulting to the original and list it in the report. No original pixels/text/data in git.
- Gate: affected `anno_app_tests`/sim/render cases, `^viewer_wiring_ui_`, the six oracle tests (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`) all required, `tools/test/run_selected.py --jobs 8 --require-test ...`, env as in the UI-CATNAV-1 packet (do NOT set ANNO_DEV_GOODS_TONS for gates); clang-format 23.1.3 (`E:\anno1503-worktrees\.scratch-cfpkg`); one WSL `check_unix_warnings.sh`; failure proof; REPORTS entry; push and mirror (`python -I tools/orch/sync_gitlab.py --repo .`); exact-head CI is the orchestrator's. One heavy job at a time on the PC (check cl/link/WSL first).
- Owner check is in the next integration build with plenty of resources (`ANNO_DEV_GOODS_TONS=2000`, `ANNO_DEV_GOLD=100000000`).
