# ZOOM-BOUNCE-1: one frame at the wrong scale during a zoom

**Status:**
- **Written** 2026-11-15 by the Claude Code orchestrator for **the Windows executor**. QUEUED after ZOOM-SMOOTH-2c.
- **Source:** the owner's UI test on main `cc7af72c` (2026-11-15), recorded as `ui_4` and analysed frame by frame by
  the UI lane.

**Base:** `origin/main` at release. Branch `task/ZOOM-BOUNCE-1`, a new worktree. About 2 hours. Never rebase,
force-push or push `main`.

**Rules:** the owner rule of 2026-11-12 ([`TASKS.md`](../TASKS.md)): one concern, no reviewer subagents, one REPORTS
entry of about 60 lines, READY FOR REVIEW.

## What was seen

- During a wheel zoom **out from zoom group 0 to 1**, one frame is drawn at a different scale or position between
  two frames that match each other: video frame 970 (16.17 s), game time about 32270 ms. Frames 969 and 971 differ
  by a mean of 3.8 grey levels; frame 970 differs from both by 31.
- The zoom sequence around it, all at one cursor position (window pixels about 1080, 635 of 2560 x 1369), was
  group 0, 1, 2, 1, 0, **1**, 2, 3, 4 between video 14.1 s and 16.6 s. The title showed group 1 only at 16.27 s, so
  the frame is inside the eased animation, before the group digit changed.
- The other frames the scan flagged in zooms were ordinary animation. No frame hitch was logged.
- The viewer ran `--play data\maps\lan5.txt --construction-menu on` on the 4090, enhanced water, maximised.
- The log: `cache/owner-ui-test-20261115-164504/stderr.log` in the game-file worktree
  (`E:\anno1503-worktrees\anno1503-main-gamefile-run`). Video time = game ms / 1000 minus 16.10 s.

The cause is not known. It may be the renderer, the present, or the capture.

## Steps

1. **Reproduce without the owner:** a dev script with the same wheel sequence at the same position and size
   (`wheel N X Y`), repeated many times. Log, per drawn frame, the eased scale and anchor the frame *asked* for and
   the scale and origin each layer was *drawn* with (chunks, stand-ins, sea, sprites, UI). A frame where they
   disagree, or where the scale is not monotonic inside one zoom step, is the bug.
2. **If it reproduces:** find the cause and fix it at the cause. Likely places, to check and not to assume: the
   frame where the picture's level switches inside an eased step; a pending zoom step applied one frame late
   (`pending_zoom_`); the anchor recomputed with the old group's tile size for one frame.
3. **If it does not reproduce** in 200 repeats: say so, keep the logging behind a debug switch, and stop. Do not
   change the zoom on a guess. The owner's recording then needs a capture-side check instead.
4. **A test** that fails before the fix: the per-frame scale of a scripted zoom step is monotonic and every layer
   uses the frame's one scale and origin.
5. **Gates:** the render and app suites, GPU on the 4090, WSL `-Werror`, CI.

**Done:** the cause named with evidence, the fix and its test; or an honest "not reproduced" with what was tried.
