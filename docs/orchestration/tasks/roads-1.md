# ROADS-1 SIM-ROADS-1: the original's road drag rules in the sim

**Released 2026-11-19** (owner "Yes, go" via the orchestrator). Branch `task/ROADS-1`, base `60a26f0d`, worktree
`anno1503-ROADS-1`. About 4 h. OQ range: 780-784. Sim lane: determinism rules apply (integer only, ordered containers,
no globals).

## Rules (BUILDINGSOBJECTS §10.1, addresses AnnoFrame 0x1004b290 and the items named there)
1. `road_path(start, end, cap = 40)`: L path, longer axis first, y first on a tie, cut at 40 tiles (not extended to the
   pointer). Pure helper, unit-tested.
2. Per-tile check in path order, stopping at the first failure: same island as the first tile; every tile is territory
   (owner nibble != neutral, not "at least one tile"); one city, owned by the commanding player; `City::Build`
   accepts; figure veto; no downgrade (dirt over existing cobblestone refused).
3. Cost: re-laying within the same family (cobblestone over cobblestone, dirt over dirt) is free; otherwise 5 gold per
   dirt tile / 10 per cobblestone tile (data from the type, not literals), charged until the first unaffordable tile;
   bridges always pay.
4. Run command: apply stops at the first refusal and commits the valid prefix (kind 9, OQ-043). Use the existing
   command/wire structures; do not widen `kStateHashVersion` unless state changes (state versioning per ADR 0006,
   state.md §9.7: if you must change it, say why).
5. The 40-tile cap, the per-tile territory rule and the no-downgrade rule are original defaults in the `rules` config
   (names in the report); the old behaviours stay available only as documented options.
6. Not in scope: reshaping (ROADS-2), bridges/ramps (ROADS-5), UI (ROADS-3), picture (ROADS-0).

## Report
List **every pin, golden or hash that moves** (sim state hash, scenario goldens, placement corpus reference
`tools/placement_ref.py`) with before/after and why; the sim lanes (determinism, `^sim_`, placement corpus, agree.py
as applicable) are in the gate, including the Linux GCC check.
Tests: unit tests per rule with the evidence address; a corpus-style case over road records; failure proof.

## Common rules (all lane packets)
- Reference: `docs/BUILDINGSOBJECTS.md` part B §10 (BUILDINGS-REF-2, evidence tags D/P/L/U) is the standing reference; update it with what you settle (tags), do not contradict it silently. OQ numbers only from your range. Original/HE behaviour is the base; where the owner could perceive a feel change, add a `rules` option defaulting to the original and list it in the report. No original pixels/text/data in git.
- Gate: affected `anno_app_tests`/sim/render cases, `^viewer_wiring_ui_`, the six oracle tests (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`, `info_panel_rotation_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`) all required, `tools/test/run_selected.py --jobs 8 --require-test ...`, env as in the UI-CATNAV-1 packet (do NOT set ANNO_DEV_GOODS_TONS for gates); clang-format 23.1.3 (`E:\anno1503-worktrees\.scratch-cfpkg`); one WSL `check_unix_warnings.sh`; failure proof; REPORTS entry; push and mirror (`python -I tools/orch/sync_gitlab.py --repo .`); exact-head CI is the orchestrator's. One heavy job at a time on the PC (check cl/link/WSL first).
- Owner check is in the next integration build with plenty of resources (`ANNO_DEV_GOODS_TONS=2000`, `ANNO_DEV_GOLD=100000000`).
