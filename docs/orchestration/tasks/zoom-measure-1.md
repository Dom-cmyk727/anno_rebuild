# ZOOM-MEASURE-1: a camera sweep that measures what the owner sees

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator to **the Windows executor**, with the owner's go ("Yes,
both", on the plan in [`docs/render/zoom-smoothness.md`](../../render/zoom-smoothness.md) §6). It is released after
SHIP-ORDER-1, under the standing go for the Windows lane. Step 1 of that plan.

**Base:** `origin/main` at release, branch `task/ZOOM-MEASURE-1`, in a new worktree. About 2–3 hours. Never rebase or
force-push.

**Rules:** the owner rule of 2026-11-12:
- one concern;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

**Measurement only:** no renderer behaviour changes. Gates follow `TASKS.md` §1 item 4: the lanes the change reaches,
plus hosted CI.

## Why

The owner requires that zooming and moving the camera show no rendering of any sort after the move. Islands must not
appear late, and nothing may fade in. The renderer's per-second statistics line confirms the failure while the camera
moves:
- up to 341 of 860 frames drew a visible chunk not built yet;
- up to 491 chunks dissolved in over a stand-in;
- single frames above the 2 ms of a 500 Hz display;
- two 0.7 s hitches in "handling window events".

The root causes are in `zoom-smoothness.md` §3. Before anything is fixed, this packet makes the failure **measurable
and repeatable**, so ZOOM-SMOOTH-1 and the later direct renderer can be judged by numbers, not by eye.

## Do

1. **Extend the existing sweep, don't start a new tool.** `bench_frame --zoom-sweep` (`docs/perf/README.md`, "T3 stage
   3 items (a)–(h)") already runs the viewer's continuous zoom with the interactive limits. Add **per-frame readiness
   counters** to its output, read from the compositor's and frame renderer's own state:
   - visible chunks not built or not uploaded;
   - stand-in draws;
   - chunks in a dissolve;
   - placeholder (open-sea) draws;
   - wave-window switches and frames inside a wave fade;
   - the busy time and its steps.

   The window's per-second statistics line already counts some of these. Reuse those counters and don't duplicate the
   logic.
2. **A sweep that matches how the owner plays:**
   - the fixed map `data/maps/lan5.txt` (`--play`), and the ship-start map
     `data/maps/candidates/ship-start-v2/lan5-ship-a.json`;
   - enhanced water on and off;
   - wheel notches at a human rate (state it, for example 8 notches per second), from the default zoom out to the
     minimum and in to the maximum, anchored at the cursor off centre;
   - fast pans in both directions, and a zoom during a pan;
   - **at 2560 × 1369 and 1280 × 800, on the RTX 4090 (adapter 0) and the Radeon iGPU (adapter 1)**.

   Keep the existing 6 s legs as a second, slow profile.
3. **A pass criterion, as a test that reports today's failure.** Add a contract, opt-in because it needs game files
   and a GPU, that runs the sweep and fails unless every frame has:
   - 0 visible non-ready chunks;
   - 0 stand-in draws;
   - 0 dissolves in progress;
   - a busy time of at most 2 ms (state the warm-up frames excluded and why).

   It will fail today. Report its numbers as the **baseline**, and don't relax it.
4. **The window-event hitch.** With the window trace (`ANNO_VIEW_TRACE`, `docs/app/window-trace.md`) on a real window,
   measure a window drag and a maximise/restore. Name the mechanism. The research points at SDL's blocking event poll
   inside Windows' modal move/resize loop (`zoom-smoothness.md` §5); confirm or refute it with the trace. Fix nothing
   here; ZOOM-SMOOTH-1 does.

## Proof

- **The baseline table:** per map, water, resolution and adapter:
  - frames;
  - frames with non-ready content;
  - the most non-ready chunks at once;
  - stand-in draws;
  - dissolves;
  - wave switches;
  - busy-time p50, p99 and max;
  - frames above 2 ms.
- **The contract** fails today with those numbers, and a hand-made "everything resident" control passes it. Everything
  resident means the cache warmed over the whole sweep path first, if that is possible for the small map. If it is
  not, say why.
- **The hitch's mechanism,** from the trace.
- Hosted CI green.

**Not yours:**
- any fix (ZOOM-SMOOTH-1);
- ADR 0007 (the orchestrator);
- the full WSL gate and the game-file run (the orchestrator, at the merge).

## Report

READY FOR REVIEW, about 60 lines:
- the counters added;
- the sweep profiles;
- the baseline table;
- the contract;
- the hitch's mechanism;
- the gates and CI;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
