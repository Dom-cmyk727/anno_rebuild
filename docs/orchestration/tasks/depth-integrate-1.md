# DEPTH-INTEGRATE-1: buildings sit in the ground and the water; the sea at the original's level

**Status:** QUEUED 2026-11-13 by the Claude Code orchestrator for **the Windows executor**. It comes right after
ZOOM-MEASURE-1, ahead of the zoom fixes (owner, 2026-11-13: "Yes, depth first"). The owner's playtest of 2026-11-13 is its depth check.

**Base:** `origin/main` at release. Merge `origin/task/W4-depth-1` (`5eaef168`) into a new branch
`task/DEPTH-INTEGRATE-1`, in a new worktree. About 3 hours. Never rebase or force-push.

**Rules:** the owner rule of 2026-11-12:
- one concern;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

Gates follow `TASKS.md` §1 item 4: render and app change, so the render and app lanes, the GPU tests on the 4090 and
the Radeon, and hosted CI.

## Why

**The owner, on the ship-start Kontor (2026-11-13 playtest, screenshot):**
- "the placement of the kontor is still not really correct. … first, sea level needs to be raised";
- "you can see the left side of the building, that also needs to be embedded into the ground just like the side to
  the back".

**W4-DEPTH-1** ([review](../reviews/2026-11-04-w4-depth1.md)) built exactly that on 2026-11-04 and has waited since for
the owner's depth check:
- **per-pixel sprite depth** against the terrain and the water, as the original's depth brushes do
  ([`docs/render/depth.md`](../../render/depth.md) on its branch);
- **one integer depth function** shared by the GPU and the CPU (2,742 chunks, 0 pixels differing);
- **`--depth on|off`:** off restores the former digests exactly;
- **the sea plane** measured in the original at about **26.5 `HEIGHT`**. The branch's last commits made 26.5 the
  default shore.

**Owner decisions recorded with this packet (2026-11-13):**
- **depth on** by default, the original's behaviour;
- **the sea at the original's level,** the 26.5 plane.

The review's caveat: at 26.5, today's coarse sea-region rule shows **stepped water edges on wide beaches**. That is
SHORE-2's job, the next packet, and not this one.

## Do

1. **Merge `origin/task/W4-depth-1` into a branch from `origin/main`.**
   - The trial merge on 2026-11-13 found code auto-merging and three append-only document conflicts:
     `docs/open-questions.md`, `docs/orchestration/REPORTS.md` and `docs/perf/README.md`. Keep both sides.
   - Main's render code moved a lot since 2026-11-04, so **prove the merged code**, don't trust the auto-merge.
2. **Bring the branch's own claims up to today's main:**
   - **GPU = CPU:** the depth function on both, over a real-file chunk corpus, 0 pixels differing, or the stated
     tolerance with the reason;
   - **`--depth off`:** equals `main`'s current digests exactly;
   - **depth on:** the scene digests re-pinned once, with the reason in the commit message;
   - **the shore default is 26.5,** and `--shore 16.5` gives the old water.
3. **The owner's case, before and after:** the ship-start Kontor at the owner's spot (candidate A, seat 0, found at
   `(255,138)` as in [`ship-opening-playtest.md`](../../testing/ship-opening-playtest.md) step 3).
   - Show PNGs with depth off and on, and at shore 16.5 and 26.5, at group 1 or 2.
   - Point out in each picture where the piles meet the water and where the land-side and left walls meet the
     ground.
4. **The playtest card:** update the ship-start card to say this build has the depth and the original's water level,
   and what to look at.

## Proof

- **The pictures of Do item 3,** saved under `C:\Users\altin\Pictures\anno-review\DEPTH-INTEGRATE-1\`.
- **The render and app lanes** on MSVC Release with the game files and in the WSL `-O1` build, and the GPU tests on
  both adapters: classified counts.
- **Hosted CI green.**

**Not yours:**
- the per-pixel sea edge (SHORE-2);
- the direct renderer (ADR 0007): its passes must carry this depth, and the ADR now says so;
- the full gates (the orchestrator, at the merge).

## Report

READY FOR REVIEW, about 60 lines:
- the merge and its conflicts;
- the proofs, depth off against today's digests and GPU = CPU;
- the pictures;
- the gates and CI;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
