# WHALE-RE-1b review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** Every table, address and data field the orchestrator re-checked matches. The
packet answers its four reads. The facts that matter for a port:
- **A whale is the old animal class with nine changed slots.**
  - It wanders in a 61-cell box around where it is now, not around its source.
  - It ends only when its health reaches zero (hunting), or on a terminal movement step. It has no age rule: the new
    animals' 25,000 budget must not be copied to it.
- **Each walk start makes exactly five world draws,** even when the path then fails: two offsets of 7 to 11, two signs,
  and a fifth draw whose value is thrown away.
- **The whaling boat is bought by command `0x71`.** It costs 600 gold, 12 wood and 7 rope, charged at once.
  - A request with its active byte at 0 is still charged.
  - A lost boat is never replaced automatically; the player has to ask again.
- **The boat's attack makes a world draw even with one firing position,** before the projectile is created.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/WHALE-RE-1b` `109ec14c`:
- `43d02a99`: the research;
- `109ec14c`: the handover.

Changed: `docs/re/whaler.md` (+277 lines, sections 6–9 and the port list), `docs/open-questions.md` (dated lines on
OQ-593 and OQ-594, new OQ-595) and the REPORTS entry (64 lines). Documents only.

## What the orchestrator re-checked

**Tables, read directly** from GOG `AnnoWelt.dll` and `AnnoFrame.dll` (pefile):
- **Whale table `0x100c5f98` against the old animal `0x100c653c`,** all 101 slots: exactly nine differ, at `+0x50`,
  `+0x7c`, `+0x80`, `+0x94`, `+0xa4`, `+0xc8`, `+0xcc`, `+0x17c` and `+0x190`, with the addresses in §6.
- **Whale route `0x100c7218` against `0x100c7348`,** 22 slots: exactly two differ, `+0x34` (`0x1005d950`) and `+0x4c`
  (`0x1005d920`).
- **Whaler table `0x100c9418`:** `+0xdc` = `0x10073c10` (the check) and `+0xe0` = `0x10073b40` (the enqueue).
- **AnnoFrame dispatch table `0x10060388`:** entry `0x71` = `0x100600be` and entry `0x73` = `0x100600e8`, as §8 says.

**Disassembly** (`tools/disasm.py`, Capstone):
- **The walk start:**
  - world `+0xb4` is called at `0x100586f7` and `0x10058713`, each result `div 5` then `+ 7`;
  - then at `0x1005872f` and `0x10058743`, each a conditional `neg`;
  - the fifth call, at `0x100587c8`, has its result overwritten by the next instruction, so its value is discarded.
- **The ship's attack:**
  - type `+0xa0` gives the firing-position count, of which the low byte is kept;
  - world `+0xb4` is called at `0x1003d616`;
  - then `div` by the count, with the remainder passed to type `+0xa4`.

  §9 cites `0x1003d615`, the argument push one byte before the call. Harmless.

**Data:** `FigurenHeader.dat` at 1511's LOGIC (`0x1a0046b`): money `+0x16` = 600, goods quantities `+0x20`/`+0x21` =
12 and 7.

**Not re-checked** (taken on the executor's reading):
- the cross-module scans for callers of `0x10073b40`;
- the sea-navigation mask arithmetic (`0x10090350`);
- the HP setter's ending path;
- the ship movement callbacks' "no direct draw" ranges in §9.

**CI:** docs-only CI green on `109ec14c` (run 43387178639).

## Notes (no fix round)

- **The draw closure is honestly left open.** The whale's walk and the boat's attack have exact direct draws. The
  shared search expansion, the terrain and projectile callbacks, and the float evaluation are not certified zero:
  OQ-594 is narrowed, and OQ-595 is new. A reference packet can pin the direct draws now. A port must not claim
  PRNG parity for whole whale or boat lifetimes until those are closed.
- **OQ-596 to OQ-602 are unused.**
- **Still open from the 1a review:** the native WALSPECK records 3501 and 3515.

## Disposition

ACCEPTED. It lands with the next docs landing, on the owner's OK. The next read for the Mac lane is
[WHALE-RE-1c](../tasks/whale-re-1c.md): a reference script with hand-derived vectors for the direct draws and the
tick arithmetic, as WILDLIFE-RE-1d was for wildlife. It is released when the owner gives the go.
