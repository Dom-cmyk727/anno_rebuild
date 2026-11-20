# FISHER-RE-3, a fisher hut with every entrance on land: review (2026-11-16)

**Reviewer:** the Claude Code orchestrator.
**Head:** `task/FISHER-RE-3` `5901b846`, base main `67a960c9`. A read with reference vectors. **CI:** green.

## Verdict

**ACCEPTED as the GOG read.** The History Edition picture comparison the owner asked for is **not done and not
claimed**; it is the next work, and it needs two things first (below).

## What the read says

1. **The case can arise.** The coast rule for the fisher types checks corner heights of the footprint's window, not
   the entrances: the highest corner 48, every corner of the shore edge exactly 48, and at least half the opposite
   edge's corners below 32 (`AnnoGame 0x10008ea0`–`0x10008f81`). So a hut can stand with both entrances on land.
   A real site: `Tutorial/02Trade01`, player 2, type 810 at (242, 217), rotation 1, entrance means 32 and 48.
2. **The worker is still sent, and can still leave and catch.** Dispatch does not look at the entrances; a closed
   start can expand out to a neighbouring water cell.
3. **It cannot come home.** A closed entrance is never admitted as a goal from another tile (layer 2's entry for
   land is zero, `AnnoWelt 0x100d4947`). The home job fails; the fallback returns nothing for its kind
   (`0x1005e280`); the hut's raw stock is **not** credited. The worker stops where it was, waits (15.0 at rate
   0.01, nominally 7500 ticks) and is released; the cargo is lost.
4. **A correction to `fisher.md` §3:** the home job's kind is 4, not 8.
5. **The census is corrected:** all 50 maps hold 92 fisher placements; 87 have one open entrance; the 5 with none
   are all type 3510, whose worker is a land figure (393, route 44), a different case.

## Checked against the binaries (`objdump`, same hashes)

- `AnnoGame 0x10008ea0`–`0x10008f81`: `and 0xf0` then `cmp 0x30`; each shore vertex `cmp 0x30`; opposite vertices
  counted when below `0x20`; accepted when the count reaches half. As the note says.
- `AnnoWelt 0x10057ad0`: returns 4.
- `AnnoWelt 0x1005e280`: kind 7 returns the retry node, anything else null.
- `AnnoWelt 0x1003af69`–`0x1003af96`: velocity zeroed, rate `0x3c23d70a` (0.01), then the release call.
- `AnnoWelt 0x100d4940`: bytes `ff ff ff ff ff 00 00 00`; entry 7 is zero.
- The oracle: 66 vectors, 96 contract checks, 21 mutants killed (CI).

## What stays open, as the executor states

- **The History Edition: NOT_RUN.** The Mac has no History Edition binaries. No parity is claimed.
- A live original build at that site is not measured.
- Whether a failed trip survives an original save (the temporary worker's save path) is unproved.
- OQ-603 (float and Y parity) and OQ-604 (indirect draws) stay open.

## What the picture comparison needs (the executor's QUESTION)

1. **The rebuild's rule:** the sim does not yet follow this read for the closed case. FISHER-1f (code, Mac).
2. **A viewer that can show it:** `--play` creates the world with default extensions, so the fisher rule cannot be
   switched on in the viewer, and there is no stock or time readout for a capture. A small app step.
3. **The captures:** the History Edition and the GOG original at the capture card's site and its open control, then
   the rebuild, compared frame by frame. On the Windows PC.
