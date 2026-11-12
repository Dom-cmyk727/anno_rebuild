# WILDLIFE-RE-1b: the animals' own life (documents only)

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, with the owner's go, to **the Mac executor**. Step 2′
of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` at this packet's release commit. Branch `task/WILDLIFE-RE-1b`, in a **new worktree** (not the
1a or MAP-INTEGRATE-2 one). About 3–4 hours.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover and re-checks addresses), one REPORTS entry of about 60 lines, READY FOR REVIEW. Documents only: no
engine code, no tests, no fixtures.

## Start from

- [`docs/re/wildlife.md`](../../re/wildlife.md) (1a, accepted in
  [its review](../reviews/2026-11-12-wildlife-re-1a-review.md)): the sweep, the creation helper, the `HIRSCH` loader,
  and the open questions OQ-573 and OQ-575, which this packet takes over.
- The [edition comparison](../../research/he-vs-gog-comparison.md) §4: the animal class tables in GOG
  (`tBewohnerWildTier` `0x100c653c`, `tBewohnerWildTierNew` `0x100c66fc`, the sibling `tBewohnerZivilist`
  `0x100c6a98`), and the History Edition addresses of the route and job classes not yet located in GOG.
- [`docs/re/hunter.md`](../../re/hunter.md): route `HIRSCH` is index 62, and the "job switch" `0x1003b0e0` creates
  route objects (`tRoute*`, 22 slots) whose steps are job objects (`tJob*`, 10 slots).

## Read (GOG `AnnoWelt.dll` 2.0.0.5 is the reference; `tools/disasm.py` as in 1a)

1. **The overridden slots.** Compare `tBewohnerWildTier` (101 slots) and `tBewohnerWildTierNew` (104) with
   `tBewohnerZivilist` and other descendants of `tBewohnerBoden` to list the slots the animal classes override. Read
   the overridden ones: the tick, movement, death, the two flag bits of `+0x8c` (bit 1, value 2; bit 2, value 4)
   that the hunter's kill step tests, and every world draw (world slot `+0xb4`).
2. **Construction and attach (OQ-575).** For an animal, what the manager's creation slot 0 (called at
   `0x10092f5b`), figure property `0x2f` (`0x10092fe2`), operation `0x45` (`0x10092ff9`) and the island attach
   through figure slot `+0x28` (`0x10093002`) do, and every world draw they make, directly or through callees.
3. **The spawn record's release (OQ-573).** Which path clears spawn byte `+8` bit `0x40` or the saved figure id:
   death, removal, a kill by the hunter, or leaving the chunk. Say whether a record can stay occupied after its
   animal is gone.
4. **Routes and jobs.** Locate `tRouteHirsch` (base `tRouteWildTier`), `tRouteWildTier`, `tJobSteheWildTier` and
   `tJobWalkRandom` in GOG, by table size and call profile, starting from what `0x1003b0e0` creates for route 62.
   Read how an animal chooses where to stand and where to walk. Count every draw of a random walk exactly: its place
   in the tick, its modulus, what it selects.

**The History Edition second reading** stays optional (its DLL is on the PC, not the Mac); mark it NOT_RUN.

## Deliver

- **New sections in `docs/re/wildlife.md`**, one per read above, in 1a's style: every claim with its address and an
  evidence tag, no disassembly text, no bytes.
- **The animal's draw table**, in the same form as 1a's: position in the tick or call, modulus, what it selects,
  condition. Together with 1a's table it must give every draw from an animal's creation to its removal.
- **OQ-573 and OQ-575:** add a dated bullet to each saying what this read settled or narrowed (don't rewrite their
  existing text). New open points as entries **OQ-578** to **OQ-585**, reserved for this packet. When a read does not
  settle a rule, record what is known and stop that item. Nothing is invented.

## Gate

Docs only: `git diff --check`, links that resolve, and hosted CI green on your pushed head. Read CI after every push.

## Out of scope

The hunter's approach (`tJobWalkRohstWild`), the fur trapper, the wolf class and predators (all WILDLIFE-RE-1c); the
reference script and its vectors (WILDLIFE-RE-1d); any engine code.

## Report

READY FOR REVIEW, about 60 lines: the reads done, with their addresses; what is settled; the state of OQ-573 and
OQ-575; the new open points; NOT_RUN; Decisions and reasoning; QUESTIONs.
