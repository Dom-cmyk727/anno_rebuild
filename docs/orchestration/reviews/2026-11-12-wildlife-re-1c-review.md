# WILDLIFE-RE-1c review (2026-11-12)

**Verdict: ACCEPTED.** The hunter's approach, the fur trapper and the predators are read with addresses, and the
predator facts the owner asked for hold up in the code: predators chase, kill and eat other wildlife, and they cannot
select residents or soldiers. Every address the orchestrator re-checked matches. No fix round.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/WILDLIFE-RE-1c` `1ee3f4d5`, one commit on `main` `e154fea5`: `docs/re/wildlife.md` §8–10
(+361 lines), a dated bullet on OQ-578, new OQ-583 to OQ-586, the REPORTS entry. Additions only, documents only, in a
new worktree. Hosted CI green (run 43221164040). Committed at 13:47, 19 minutes after the packet reached `main`.

## What the orchestrator re-checked

| Claim | Where | Result |
|---|---|---|
| The wolf table `0x100c68e0` overrides exactly five of WildTierNew's 104 entries: `+0x168`, `+0x18c`, `+0x194`, `+0x198`, `+0x19c` | both tables, all 104 entries | matches |
| The wolf constructor installs `0x100c68e0` | `0x1005377b` | matches |
| The prey search asks the unit manager with mask 8 and at most 256 results | `0x10052f0f`–`0x10052f34` | matches |
| The manager skips flags `0x0a` and keeps a figure only if `mask & (1 << ((kind − 1) / 16))`; mask 8 is kinds 49–64 | `0x10098513`–`0x1009854a` | matches |
| In AnnoGame's kind-name table `0x10024cb8`, kinds 49–54 are HIRSCH, RAUBTIER, HASE, KROKODIL, ESEL, WAL; the resident (ZIVILIST) is kind 33 and the soldiers (SCHWERT … KATAPULT) kinds 97–111, outside the mask | `AnnoGame.dll` | matches |
| Prey lists: all predators `117, 122, 129, 135, 502, 504, 505`; predators 116/126/131/201/529 also `118, 119, 128, 130, 120, 123, 127, 132, 137, 141, 501` | u16 tables `0x100c68d0`, `0x100c68b8` | matches; all 18 ids are on `hunter.md`'s list of route-HIRSCH figure types |
| The nearby-figure hook's "interaction" call is a stub returning zero | `0x10001050` | matches |
| Wolf idle draw `0x100534bb` (parity) and walk-completion draw `0x100534e4` (`% 7`) | `0x100534b6`–`0x100534c7`, `0x100534df`–`0x100534f1` | matches |
| The fur trapper's factory installs `0x100c2bf0` after its base constructor | `0x10024209`–`0x1002420e` | matches |

**Not re-checked:** the hunter's pursuit slots and their geometry, the trapper's 73-of-75 slot comparison and
production figures, the corpse and damage arithmetic, and the claim that the hunter and trapper ticks make no direct
world draw.

## Predators: the facts, and the orchestrator's recommendation

**The facts (D/P).** All seven RAUBTIER figure types use the wolf class. A wolf searches for the nearest animal of
its prey list, chases it, damages it (170, 80 or 40 per hit), and eats the corpse. Prey are only wildlife. Residents
and soldiers are outside its search by kind, and the one call that looked like an interaction with nearby figures is
a stub. So predators are an ecological rule, not a threat to the player's people.

**Recommendation (orchestrator): include predators in the first wildlife port.** They are the same class family (five
overrides on the new animal class), so the extra code is small. Leaving them out would change how much quarry hunters
find and the world's random sequence, which a faithful port (option 1) is meant to reproduce. The decision is the
owner's.

## Notes (no fix round)

- **A useful separation:** the class the comparison called `tJobWalkRohstWild` turns out to be the animal's own feeding
  walk, not the hunter's pursuit (structural label, OQ-583).
- **OQ-578 and the port.** Three reads have now narrowed it; what remains are shared callbacks and runtime type
  interfaces. Proposal: settle it by measurement when the port exists. WILDLIFE-1's acceptance compares world-draw
  counts with the running original on the Windows Wine runner over a scenario with animals, hunters and predators. A
  read packet follows only for a disagreement. The orchestrator writes this into WILDLIFE-1's packet.
- **The open points are well cut:** target-id reuse (OQ-584), zero-HP prey type 130 (OQ-585), paused prey after an
  interrupted attack (OQ-586).

## Disposition

Lands as is, with this review, with the owner's OK. Next on the Mac lane: WILDLIFE-RE-1d (`tools/wildlife_ref.py`
with hand-derived vectors from the draw tables of §1, §7 and §8–10), released only with the owner's go. The owner's
decision on predators is listed in STATUS.
