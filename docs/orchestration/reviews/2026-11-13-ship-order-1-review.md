# SHIP-ORDER-1 review (2026-11-13)

**Verdict: ACCEPTED as "not reproduced".** No code change. The packet asked to reproduce first, then name the
mechanism, and to stop with a QUESTION when it could not be reproduced. The executor did exactly that.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/SHIP-ORDER-1` `e2c3e4e8`. Only `docs/orchestration/REPORTS.md` changed (+68 lines). CI green,
`gate` included (run 43421981516).

## What the orchestrator checked

- **The matrix:** 8 scripted variants of `opening_controls.script`. Every variant ordered a real `MoveCmd`, which was
  accepted, and the ship arrived. The variants:
  - the contract's own;
  - `--rules living`;
  - 1024 × 768;
  - the GPU on the RTX 4090, whose stderr matches the owner's `window: drawable 1024x768 px … density 1.00`;
  - a resize to 2560 × 1369 mid-session, then ordered again;
  - the GPU at 1024 × 768 and speed 200 with no `--rules`;
  - original water.
- **PLAY-LIVE-1a is not involved.** `git diff 961a9d4a 9e920c18` is empty over `viewer.cpp`, `opening_ui.cpp/.hpp`,
  `ship_motion.cpp`, `ship_layer.cpp` and `src/sim/src`. The orchestrator re-ran that diff. **PLAY-LIVE-1a may land.**
- **The named candidate mechanism exists.** `OpeningUi::pointer_up` treats a right press and release that travelled
  5 pixels or more as a pan, not an order (`opening_ui.cpp:191`, `kClickSlop`), and gives no message. Scripted input
  has no travel, so the contract cannot exercise it.
- **A second candidate, from the orchestrator.** Another seat's ship can be selected (blue ring) but never ordered.
  The title says "(inspect only)" (`opening_ui.cpp:123`, `:446`), but at 1024 × 768 the title is cut off.

## Questions to the owner

1. A screenshot of the title right after an order that does nothing.
2. Did the mouse move between pressing and releasing the right button?
3. Was the ring around the selected ship yellow (yours) or blue (another seat's)?

The executor's question about Windows display scaling is answered by the owner's log: density 1.00.

## Disposition

Closed until the owner's answers point to a cause; then it is reopened. The Windows lane continues with ZOOM-MEASURE-1.
