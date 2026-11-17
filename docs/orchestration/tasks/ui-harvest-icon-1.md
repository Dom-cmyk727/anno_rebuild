# UI-HARVEST-ICON-1: the status icon above a building ("nothing to harvest")

**Released 2026-11-17 (stage A only)** on the owner's go, relayed by the orchestrator: "all of them" for the
he_01 list. Branch `task/UI-HARVEST-ICON-1`, base origin/main `4dfdd9b7`, worktree
`E:\anno1503-worktrees\anno1503-UI-HARVEST-ICON-1`. Stage A is docs only (about 2 h). Stage B (drawing,
2-3 h) is written after A's review.

## Why

The owner's HE recording `he_01.mp4` (8:50-9:13; analysis `E:\1503_test_videos\he_01_work\he_01-report.md`,
crop `c5_icon.png`, sheet `s7_farm.png`) shows a **framed icon (a sheaf) with a vertical red-to-yellow bar
beside it** above the forester. He narrates that there is nothing to harvest yet: trees must reach a growth
stage before they can be cut, and the same holds for most farms. The rebuild draws no status icons above
buildings.

## Stage A: read the rule (docs only)

1. **The icon set:** every status icon the original can show above a building (the sheaf "nothing to
   harvest", and others such as missing raw material, full storage, no road, no workers, fire,
   plague). Give each one's ACCESS name and texture (`ToolGfx/*.inc`), and its tooltip text if any.
2. **The trigger for each:** which production state sets it (production code `production.md` §4, the
   building's activity word, the harvest search), with addresses. Is it a flag on the building, or
   computed when drawing? When does it appear and disappear (any delay or blink)?
3. **The bar:** what the vertical red-to-yellow bar measures (growth progress of the nearest field?
   production progress? stock?), its scale, colours and position, and its update rate.
4. **The drawing:** the position above the building (offset, zoom levels), who draws it (render, AnnoFrame
   or View), whether it shows at every zoom, and for own buildings only?
5. **What the rebuild already has:**
   - the sim's production state (`docs/sim/`, `src/sim` production and harvest code);
   - what the play snapshot already copies.

   Say which parts are an app-side read of state the sim already holds (UI lane) and which need new sim
   state. Those become a proposal for the orchestrator, with a size.
6. **Write it up:**
   - a new section in `docs/re/ui.md` ("Status icons above buildings"), with addresses and one evidence
     row per behaviour;
   - OQs OQ-720..724 (reserved for this packet; ask the UI lane for more);
   - a REPORTS entry with stage B's plan and the sim/UI split.

Rules: GOG code by address (`py -3.14 E:\anno1503-worktrees\anno1503-UI-STATUS-1\cache\ui-status-1\adis.py`),
HE pixels only. If a frame is needed, write an HE recipe for the owner. No code, builds or tests in A.

## Stage B (released 2026-11-17)

Released by the orchestrator after accepting stage A (`13a2eb53`); plan = the stage A fix round 1 entry in
REPORTS. UI lane, read-only app work, about 3 h. **No sim changes**: if a read needs state the snapshot
cannot get read-only, stop and report.

1. `PlaySnapshot::status_icons`: per turn, for the local player's buildings, the icon = the lowest of
   {Zzz (every line switched off), `status.icons` bit 1, bit 2}, all only while the owner's Volk is 1;
   footprint, rotation, height byte. Republished unchanged while equal.
2. The window draws figures 1555 / 1570 / 1572 (1571 mapped for later) at the footprint centre as live
   units (CPU and GPU), frame = (ms / 40) mod 16 (OQ-722 default), the anchor on the top of the
   building's picture (OQ-721 default), own buildings only (OQ-723 default; the orchestrator asks the
   owner separately about a rules option for other players' icons: not built here).
3. The known divergence ("no road" shows icon 1 or 2, OQ-720) is named in the report and in a
   viewer comment.
4. Tests: the payload rule, two-line buildings, the 16-bit due test through the sim's StatusTracker,
   the collect pass (own city only, Volk gate), the frame rule, and the layer's frames against
   `tools/figuren.py`'s decode. Focused gate (`run_selected.py --jobs 8`, log
   `cache\gates\ui-harvest-icon-1-B-01.log`): the new tests, the hud/ui/selection `anno_app_tests`,
   `^viewer_wiring_` and the oracle tests (`live_screen_matches_oracle`, `ui_dynamic_draw_check`,
   `info_panel_gog_compare`, `selected_house_pixel_check`, `ui_panels_match_gog`), all required. WSL
   `check_unix_warnings.sh`, clang-format 23.1.3, a failure proof, a wiring script for the forester's
   no-harvest state and an owner-test recipe side by side with HE `c5_icon.png`.
