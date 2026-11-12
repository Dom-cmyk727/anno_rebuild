# WILDLIFE-RE-1a review (2026-11-12)

**Verdict: ACCEPTED.** The three reads are complete and careful: every claim has an address, the draw order is a
table the reference script can use, and what the reads do not settle is written down as OQ-572 to OQ-577 instead of
guessed. Every address the orchestrator re-checked matches the disassembly. No fix round.

**Reviewer:** the Claude Code orchestrator (Opus 5.5). Under the owner rule of 2026-11-12 this is the package's
independent review; the executor ran no reviewer of its own.

**Reviewed head:** `task/WILDLIFE-RE-1a` `a6c15797`, one commit on `main` `8336d59f`: `docs/re/wildlife.md` (new,
243 lines), `docs/open-questions.md` (OQ-572 to OQ-577), the REPORTS entry. Documents only, as the packet asked.
Hosted CI green on the head (run 43210924809). Committed at 12:38, seven minutes after the packet reached `main`.

## What the orchestrator re-checked

`tools/disasm.py` (Capstone, the same reader the executor used) on GOG `AnnoWelt.dll`, SHA-256 `f220a878…e6e2`:

| Claim in `wildlife.md` | Instructions | Result |
|---|---|---|
| The candidate draw `0x1009366f`: world slot `+0xb4`, `div` by the remaining candidate count | `0x1009366a`–`0x10093677` | matches |
| The last candidate moves into the chosen slot, the count drops, a category-0 collision (`0x1007c630`) retries with a new draw and no zero check (OQ-572) | `0x100936c5`–`0x100936e6` | matches |
| `+0xe = 0x00ff` written on each attempt; the position packed as bits 0–2 / 3–5 with bits 6–7 kept | `0x10093684`, `0x1009367b`–`0x100936b2` | matches |
| On insertion: chunk flag bit 0 set, occupied bit `0x40` cleared, timestamp byte `+9` written | `0x100936f5`–`0x10093712` | matches |
| The habitat threshold: island property 7 equal to 5 needs a score of at least 1, else at least 4 | `0x1009343e`–`0x10093472` | matches |
| On creation: figure id at `+0xc`, timestamp `+60`, occupied bit set, goods from type slot `+0x80` into `+0xe` | `0x1009384f`–`0x10093871` | matches |
| The helper's position draw `0x10092d8b`: `& 3`, then a four-way jump table | `0x10092d86`–`0x10092d9d` | matches |
| The species draw `0x10092e96`: `& 0x1f` into the u16 table `0x100ca700 + 2 × (32 × island[0xc0] + index)` | `0x10092e8b`–`0x10092eb8` | matches |
| The loader's draw `0x10094808`: skipped when entry byte 4 bit 0 is set, else `& 0x1f` added to the base byte | `0x100947f3`–`0x10094816` | matches |
| `0x100cac18` belongs to a different object: 4-bit packed coordinates, byte `+9` = 8 | `0x10094218`–`0x1009422c` | matches |

**Not re-checked** (taken on the executor's reading): the density table's float constants and segment values, the
neighbour scoring's per-direction branches, the four within-tile float offsets, and the rest of the object field
table. WILDLIFE-RE-1c turns the draw order into reference vectors; that is where these get exercised.

## Notes (no fix round)

- **The open questions are well cut.** OQ-573 (who clears the occupied bit) and OQ-575 (draws inside the animal
  constructor and attach) belong to WILDLIFE-RE-1b, and the packet for 1b should name them.
- **Ghidra and the History Edition reading are NOT_RUN** (no Java on the Mac; the DLL is on the PC). The Capstone
  reader was enough for these three functions.
- **Workspace:** the handover link pointed into the Mac's old MAP-INTEGRATE-2 worktree. The branch is right; the
  next prompt asks for a new worktree per packet.
- **The time:** minutes, against MAP-INTEGRATE-2's 13 hours, for a package of the right size.

## Disposition

Lands as is, with this review, with the owner's OK. Next on the Mac lane: WILDLIFE-RE-1b (the animal figures, their
routes and jobs, the hunter's approach, with OQ-573 and OQ-575), released only with the owner's go.
