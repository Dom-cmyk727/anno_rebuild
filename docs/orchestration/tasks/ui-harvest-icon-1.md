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
