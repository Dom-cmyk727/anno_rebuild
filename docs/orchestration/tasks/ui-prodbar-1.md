# UI-PRODBAR-1: the productivity bar over a building's placement ghost

**Released 2026-11-17 (stage A only)** on the owner's go, relayed by the orchestrator: "all of them" for the
he_01 list. Branch `task/UI-PRODBAR-1`, base origin/main `4dfdd9b7`, worktree
`E:\anno1503-worktrees\anno1503-UI-PRODBAR-1`. Stage A is docs only (2-3 h). Stage B (drawing, about 3 h)
is written after A's review.

## Why

The owner's HE recording `he_01.mp4` (5:56-6:30; analysis `E:\1503_test_videos\he_01_work\he_01-report.md`,
crops `c4_bar_a.png`, `c4_bar_b.png`, sheet `s6_forester.png`) shows a **10-segment bar, red to green**,
above the forester's placement ghost:
- 10/10 inland (6:05);
- 6/10 near the coast, where the shore cuts its work area (6:15, 6:22).

He narrates that it shows the building's productivity at that spot. The rebuild has no such bar.

## Stage A: read the rule (docs only)

1. **Which buildings show it:** foresters, farms, plantations, fishers, quarries, mines? Find the
   condition in the code (AnnoFrame/View/AnnoWelt: the build cursor `iAnnoCursorBau`, cursor kinds
   `0x100062c0`, and the ghost's draw), and the data field that enables it (Anlagen.dat HEAD bytes or
   the production type).
2. **What it measures:**
   - which tiles count (the work area: the forester's 11 × 11? the type's radius from data?);
   - what makes a tile count (land? free? the right ground for the crop or tree? fertility from
     `FRUCHTBAR` or the `Wachstum.dat` grid, `production.md` 4.3?);
   - how the count scales to 10 segments (rounding);
   - whether it is the same value the production uses later. Name the functions and the addresses.
3. **The drawing:** the sprite or texture (ACCESS name in `ToolGfx/*.inc`), the colour ramp per segment,
   the position relative to the ghost, and when it updates (every mouse move?).
4. **Cross-check with the HE frames:** inland 10/10, coast 6/10. Can the rule reproduce 6/10 at that
   spot? Give an estimate with your reasoning.
5. **What the rebuild already has:**
   - the placement ghost in the app (viewer's dev tool, `docs/re/ui.md` §4);
   - the sim's work areas and fertility (`Terrain::fertility` is the raw FRUCHTBAR column; the rebuilt
     grid is not in the sim, OQ-706).

   Say which parts are an app-side read of data the sim already holds (UI lane) and which need new sim
   state or rules. Those become a proposal for the orchestrator, with a size.
6. **Write it up:**
   - a new section in `docs/re/ui.md` ("The placement productivity bar"), with addresses and one evidence
     row per behaviour;
   - `production.md` if a production rule is read;
   - OQs OQ-715..719 (reserved for this packet; ask the UI lane for more);
   - a REPORTS entry with stage B's plan and the sim/UI split.

Rules: GOG code by address (`py -3.14 E:\anno1503-worktrees\anno1503-UI-STATUS-1\cache\ui-status-1\adis.py`),
HE pixels only. If a frame is needed, write an HE recipe for the owner. No code, builds or tests in A.


## Stage B (released 2026-11-17)

Released by the UI lane after the orchestrator accepted stage A (`6b6758b9`). UI lane, read-only app
work, no sim changes. Stage A's rule: `docs/re/ui.md` 4.1.

- **B1:** `src/app/{include/anno/app,src}/fertility_indicator.*`, pure functions:
  - every gate: the preview valid (`B+0x2e4`), the last ghost, `[B+0x48] < [B+0x50]`, the registry
    lookup, and the projection non-zero, tested after the count;
  - the building check: input 0 in `0x38..0x4b` and HEAD `[0x28]` != 0, from the type registry the app
    already has;
  - the disc (row table, centre cells, clamp, footprint excluded) and trunc(10·good/N);
  - figure 1557's frames and a CPU blend.

  Until a `FertilityTileSource` exists (the sim's crop test and tile-object placement), nothing is
  drawn: the named placeholder `fertility-indicator-needs-sim` (OQ-715). The viewer reports it as fact
  `tool.fertility`, and comments mark both draw sites.
- **B3a:** unit tests now:
  - disc counts 36/52/88, r1 = 0, r2 = 16;
  - 59/88 → 6;
  - 801 has the bar, 810 and 805 don't;
  - each gate on its own;
  - an injected source with a hand count;
  - the sprite (11 frames of 54 × 12);
  - a failure proof.
- **Later:** the climate tree resolver moves to sim S3. B3b (the pixel check against HE) waits for
  S2-S4 and OQ-716.
