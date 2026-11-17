# UI-MINIMAP-2: the magnifier reaches the mouse wheel's furthest zoom-out

**Written 2026-11-17** by the UI lane. The orchestrator assigned it to the UI lane ("UI logic over the
levels the renderer already exposes"). Branch `task/UI-MINIMAP-2`, base origin/main `09315620`, worktree
`E:\anno1503-worktrees\anno1503-UI-MINIMAP-2`. **Not released: it waits for the owner's go.** It also
waits for the PC to be free for builds; the orchestrator says when.

## Why

In his viewer test `ui_6.mp4` (2026-11-17, 0:47-1:43; transcript in `E:\1503_test_videos\ui_6_work\`),
the owner found that the magnifier's minus stops before the furthest zoom-out the mouse wheel reaches. He
wants them aligned: "That means we might have one or two further zoom stages on the magnifier. Rather have
one too many than too few."

Code on main:
- `Viewer`'s `lupe_plus`/`lupe_minus` handling (`src/app/src/viewer.cpp`, the block commented "The
  magnifier's + and -") calls `magnifier_target` (`src/app/src/minimap.cpp`) over the **built** levels'
  tile widths only (84, 42, 20), so the furthest press lands on level 2.
- The wheel's continuous zoom goes on to `Viewer::zoom_min_tile()`: `--min-zoom`, else the width at which
  the whole scene fits the window, never above level 2's. Below level 2 the renderer draws derived levels
  (`--max-derived-levels`).

So the renderer already shows every width the magnifier needs; this packet changes only which widths the
magnifier steps to.

This is a deliberate change from the original, made by the owner. The original's magnifier clamps to
levels 0..2 (AnnoFrame `0x10007710`, `0x10007692`; `docs/re/ui.md` §2.9). That clamp stays in the
**stepped** zoom mode (`--zoom-mode stepped`, the original's three groups), unchanged. The new stages exist
only in the **continuous** zoom, which is already the rebuild's extension. No rules option unless the
owner asks for one.

## The change (one stage, about 2 h, code)

1. **The stages.** In continuous mode, the magnifier's stages are:
   - the built levels' tile widths;
   - then, past the narrowest built level, one stage per halving (the built levels are roughly factor 2
     apart: 84, 42, 20) while the width stays above `zoom_min_tile()`;
   - then `zoom_min_tile()` itself as the last stage, so the last minus lands exactly on the wheel's
     furthest zoom-out, even when the last hop is short. The owner prefers one stage too many to one too
     few.

   When `zoom_min_tile()` is not below the narrowest built level (a small scene, or a large window), there
   are no extra stages, as today. The minimum depends on the window size, so compute the stages at the
   press, not once.
2. **Code.**
   - Extend `magnifier_target` to take a minimum width, or add a sibling that builds the stage list and
     reuses today's "next stage in that direction" choice, so a view zoomed by the wheel to between two
     stages still moves to the next one.
   - Plus walks back through the same stages.
   - Keep the function pure (no viewer state) and documented in `minimap.hpp`. Write no stage count into
     the code (CLAUDE.md "No hardcoded limits"); the halving factor is the levels' own ratio or a named
     default with a comment.
3. **Facts.** The zoom target the existing facts already report (check `zoom.*` in the dev script facts),
   plus `zoom.min_tile`, if it is not printed yet, so a script can compare the two.
4. **Tests.**
   - Unit tests in `src/app/tests/minimap_test.cpp`:
     - stages with a minimum below level 2 (several halvings, then the exact minimum);
     - a minimum just below a halving (a short last hop is still a stage);
     - a minimum at or above level 2 (no extra stages, today's results);
     - plus from the minimum back to level 0;
     - a start between two stages.
   - Today's checks that expect `nullopt` past level 2 change only where a minimum below level 2 is
     passed.
   - A viewer wiring script `tools/test/viewer_wiring/ui_magnifier_range.script`, continuous mode:
     - press minus until nothing changes, and check the zoom target equals `zoom.min_tile`;
     - press plus until nothing changes, and check level 0;
     - in stepped mode, minus stops at level 2 as before.
   - Failure proof: the first check fails on today's code.
5. **Gate.** The focused UI gate with the minimap and zoom tests (`minimap_*`, `viewer_wiring_ui_minimap*`,
   the new script, `live_screen_matches_oracle`, `ui_dynamic_draw_check`), with every listed test executed
   and 0 skipped. CI green.
6. **The owner's test.** In the viewer: zoom out with the magnifier's minus until it stops, then with the
   wheel; the wheel must not go further. Then back in with plus.

## Not in this packet

- New zoom levels in the renderer. If a stage needs a width the renderer cannot draw, stop and report:
  that goes to the render lane.
- The minimap's own zoom (+/- top right): the owner called it "perfectly fine the way it is".
- The view turn (chevrons): the render lane's VIEW-TURN work.
