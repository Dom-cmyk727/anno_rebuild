# WILDLIFE-RE-1c: the hunter's approach, the fur trapper, the wolf (documents only)

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, with the owner's go, to **the Mac executor**. Step 3′
of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` at this packet's release commit. Branch `task/WILDLIFE-RE-1c`, in a **new worktree**. About
3–4 hours.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover and re-checks addresses), one REPORTS entry of about 60 lines, READY FOR REVIEW. Documents only: no
engine code, no tests, no fixtures.

## Start from

- [`docs/re/wildlife.md`](../../re/wildlife.md) §1–7 (1a and 1b, accepted; reviews
  [1a](../reviews/2026-11-12-wildlife-re-1a-review.md), [1b](../reviews/2026-11-12-wildlife-re-1b-review.md)): the
  sweep, creation, the animals' classes, their life, the spawn release, their walk draws, and the flag word at figure
  `+6` read through query slot `+0x8c` (bit value 2 ending, 4 removal guard).
- [`docs/re/hunter.md`](../../re/hunter.md): the hunter tick `0x10069f40`, the target search `0x10071da0(8)`, the
  hunter's job `0xd` (constructor `0x1005c610`, table `0x100c6f90`) and its five steps (pursuit start `0x10057690`,
  `0x10054c90`/`0x10054ce0`, `0x10056b10`, the kill `0x10054760`/`0x100547e0`), and the fur trapper tick
  `0x1006e5d0`, which sends the same job `0xd`.
- The [edition comparison](../../research/he-vs-gog-comparison.md) §4: `tBewohnerWildTierWolf` `0x100c68e0` (104
  slots, base `tBewohnerWildTierNew`), and the History Edition addresses of `tJobWalkRohstWild` (`0x10148c84`, base
  `tJobWalkRohstProd`) and `tProdPelzJaeger` (`0x10151480`, base `tProdPlantage`).

## Read (GOG `AnnoWelt.dll` 2.0.0.5 is the reference; `tools/disasm.py` as before)

1. **The hunter's approach.** Locate `tJobWalkRohstWild` in GOG (table size and call profile against
   `tJobWalkRohstProd`). Decode the pursuit steps' movement slots (`+0xe8`, `+0xd0`, `+0xc8`, `+0xcc`) far enough to
   state what a hunter does when its target moves, dies, or is taken by another hunter, and when it gives up. Every
   world draw (slot `+0xb4`) on the way, in the draw-table form of §1 and §7.
2. **The fur trapper.** Locate `tProdPelzJaeger` in GOG. State what differs from the hunter beyond the goods
   (`PELZTIER` instead of `FLEISCHTIER`): its tick against `0x10069f40`, its target search, its production
   arithmetic, and whether its job `0xd` behaves differently. Every world draw.
3. **The wolf and predators.** List the slots `tBewohnerWildTierWolf` overrides against `tBewohnerWildTierNew`, read
   them, and state what a wolf (and the `RAUBTIER` figure types) does to residents, soldiers or other animals: does it
   attack, kill, or only roam? Every world draw. This answers the owner's scope question, whether predators can be
   left out of the first port; give the facts, not a recommendation dressed as one.

**The History Edition second reading** stays optional; mark it NOT_RUN.

## Deliver

- **New sections in `docs/re/wildlife.md`**, one per read, in the style of §1–7: every claim with its address and an
  evidence tag, no disassembly text, no bytes.
- **The draw rows** for the hunter, the trapper and the wolf, in the same table form, so that 1d can turn §1, §7 and
  these into reference vectors.
- **Open points** as new entries **OQ-583** to **OQ-590** (reserved for this packet). If a read touches OQ-578 (the
  transitive draw closure), add a dated bullet saying what it narrowed; do not rewrite existing text. When a read does
  not settle a rule, record what is known and stop that item. Nothing is invented.

## Gate

Docs only: `git diff --check`, links that resolve, and hosted CI green on your pushed head. Read CI after every push.

## Out of scope

Closing OQ-578 as a whole (the orchestrator sizes it after this packet); the reference script and its vectors
(WILDLIFE-RE-1d); the whaler; any engine code.

## Report

READY FOR REVIEW, about 60 lines: the reads with their addresses; what is settled; the predator facts in two or
three sentences for the owner; the new open points; NOT_RUN; Decisions and reasoning; QUESTIONs.
