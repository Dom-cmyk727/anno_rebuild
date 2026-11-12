# WILDLIFE-RE-1a: where the animals come from (documents only)

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, with the owner's go, to **the Mac executor**. Step 1′
of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Owner decision, 2026-11-12:** wildlife is ported faithfully (option 1). This packet and 1b/1c read the original so
that the port (WILDLIFE-1, later) invents nothing.

**Base:** `origin/main` at this packet's release commit. Branch `task/WILDLIFE-RE-1a`. About 3–4 hours.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover and re-checks addresses), one REPORTS entry of about 60 lines, READY FOR REVIEW. Documents only: no
engine code, no tests, no fixtures.

## Start from

- [`docs/re/hunter.md`](../../re/hunter.md): its sections B and "remaining reads" already give the outline of the
  sweep, the creation helper's terrain tests and the loader's shape. Don't repeat those reads; extend them.
- [Project analysis](../reviews/2026-11-12-project-analysis.md) §7.2–7.3 and the
  [edition comparison](../../research/he-vs-gog-comparison.md) §4–5: the History Edition's class names mapped onto
  GOG addresses (`tHirschObject@tInsel` at `0x100cac04` or `0x100cac18`, the animal classes).
- [`docs/sim/state.md`](../../sim/state.md): the sweep's cadence (`fauna_period`, `SweepState.fauna_x/fauna_y`) and
  the draw site `WildlifeSweep` (`0x1009366f`, "state dependent count").

## Read (GOG `AnnoWelt.dll` 2.0.0.5 is the reference; Ghidra as before)

1. **The fauna sweep `0x10093480`, in full:**
   - what it does per chunk, and which cells it visits;
   - how many animals it makes, and when;
   - every world-PRNG draw: its position in the sweep, its modulus, and what the result selects;
   - the terrain tests (`0x46`, `0x145`) and the table `0x100e34e8`;
   - whether there is a cap or a count of living animals;
   - what the division by 1000 computes.
2. **The creation helper `0x10092ca0`:** its arguments, the figure type it chooses, the position (the float x/y it
   hands to the manager's slot `+0xa0`), and whether it draws.
3. **`tHirschObject` and the `HIRSCH` loader `0x100946c0`:**
   - the 8-byte entry and the 16-byte object, field by field;
   - how the entry's type byte becomes a figure type (`docs/re/hunter.md` lists the route-`HIRSCH` figure types);
   - which of the two 5-slot tables (`0x100cac04`, `0x100cac18`) is the class, and how its objects reach
     `0x10092ca0`.

**The History Edition second reading** (`AnnoWeltAddOn.dll`, the analysis's "HE86") is optional here: the file is
on the Windows PC, not the Mac. Use the comparison's class names as labels; mark any HE86 reading NOT_RUN.

## Deliver

- **A new `docs/re/wildlife.md`** with one section per read above. Every claim carries its
  address and an evidence tag as in `hunter.md`. No disassembly text and no bytes. Link `hunter.md`; don't edit it.
- **The sweep's draw order as a table** (position, modulus, what it selects, the condition under which it happens),
  exact enough that WILDLIFE-RE-1c can turn it into `tools/wildlife_ref.py` vectors.
- **Open points** as new entries in `docs/open-questions.md`, numbered from **OQ-572** to **OQ-579** (reserved for
  this packet). When a read does not settle a rule, record what is known and stop that item. Nothing is invented.

## Gate

Docs only: `git diff --check`, links that resolve, and hosted CI green on your pushed head (the docs-only `gate`).
Read CI after every push.

## Out of scope

The animal figures' tick, routes and jobs, the hunter's approach (WILDLIFE-RE-1b); the trapper, predators and the
reference script (1c); any engine code (WILDLIFE-1).

## Report

READY FOR REVIEW, about 60 lines: the reads done, with their addresses; what is settled; the open points with their
OQ numbers; NOT_RUN; Decisions and reasoning; QUESTIONs.
