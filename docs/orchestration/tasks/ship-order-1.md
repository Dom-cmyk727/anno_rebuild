# SHIP-ORDER-1: a ship can be selected but not ordered in the owner's playtest (bug, cause first)

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator to **the Windows executor**, ahead of GATE-2a. It is a
regression the owner found in today's playtest; obvious bugs are fixed without a decision (owner rule).

**Base:** `task/PLAY-LIVE-1a` `9e920c18` (accepted, and not yet on `main`), branch `task/SHIP-ORDER-1`, in a new
worktree. About 2–3 hours. Never rebase or force-push.

**Rules:** the owner rule of 2026-11-12:
- one concern;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

Gates follow `TASKS.md` §1 item 4 (the lanes the fix reaches). **A flake or a bug is closed by its mechanism.**

## What the owner saw (2026-11-13)

- **The build:** the preview build of `9e920c18`, in `E:\anno1503-worktrees\anno1503-main-gamefile-run`.
- **The command:**

  ```
  anno1503.exe --ship-start data\maps\candidates\ship-start-v2\lan5-ship-a.json --seat 0 --speed 200 --rules living --game-dir E:\anno1503\game\app
  ```
- **The window size.** `ANNO_VIEW_SIZE=1024x768` was still set from an earlier session in the same PowerShell. The log
  shows `window: drawable 1024x768 px`; the window was later maximized to 2560×1369.
- **"I can select but cannot command the ship anymore, it won't move at all."** The owner had ordered ships in the
  W4-OPENING-1 playtest of 2026-11-03.

**What is known:** `tools/test/viewer_wiring/opening_controls.script` step 5 gives a real right-click order and checks
its acceptance and arrival. That contract passed today:
- in the orchestrator's MSVC game-file run (code `429c0b3a`, `main`'s);
- in your app lanes at `0b6f94dd`.

So the order path works under the contract's conditions, and something in the owner's conditions differs.

## Do

1. **Reproduce first, under the owner's conditions,** with scripted input (the contract's script, or a copy). Vary
   one factor at a time, and record the title's command state (`move #n: …`, a refusal reason) and the sim's facts:
   - `--rules living` against no `--rules`;
   - `--speed 200` against the contract's speed;
   - a drawable of 1024×768 against the contract's size, and against a maximized 2560×1369;
   - the GPU against the CPU renderer, and enhanced water on and off.

   Do the same on `main` `961a9d4a`'s code, to know whether PLAY-LIVE-1a is involved.
2. **Name the mechanism** from that evidence and the code: input routing, the HUD hit test, the command path, a
   refusal from the sim, or something else.
   - If it cannot be reproduced, say so with the matrix you ran, and stop with a QUESTION for the owner (a screenshot
     of the title after an order).
   - Don't guess a fix.
3. **Fix the cause.**
   - If it is in PLAY-LIVE-1a's code, fix it on your branch, and the orchestrator lands both together.
   - If the owner's conditions need a test the contract does not cover, extend the contract, or add a case, so the
     failure would have shown up there.
4. **Proof:**
   - the failing condition passes;
   - the mutation that recreates the cause fails it;
   - the contract still passes.

## Report

READY FOR REVIEW, about 60 lines:
- the reproduction matrix;
- the mechanism;
- the fix;
- the proof;
- the gates and CI;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
