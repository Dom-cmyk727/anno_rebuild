# WHALE-RE-1a review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** Every address and data field the orchestrator re-checked matches. The packet
answers its four reads. The facts that matter for a port:
- **The oil chain is already ported downstream of the blubber.** The oil workshop 1028 is a plain `HANDWERK`
  workshop, which `main` already runs. The lamp-oil stall 1317 sells through the existing stall purchase path.
- **What is missing is the whaler itself and its whales:**
  - the whaler building's tick, its requested-boat queue and its whaling ship (figure 1511, route 45);
  - the whale sources. These are `WALSYMBOL` occurrences in the scenarios, which `main`'s scenario loader skips
    today.
- **Whales are deterministic.** Each source keeps three whale slots. Every 2,500 ms an empty slot counts up, and at
  10 a whale (figure 1202) is created at the source's centre. No world-PRNG draw is involved.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/WHALE-RE-1a` `df63c97e`:
- `a6dc0854`: the research;
- a merge of `main` `babd1da0`.

Changed: `docs/re/whaler.md` (new, 374 lines), `docs/open-questions.md` (OQ-591 to OQ-594) and the REPORTS entry
(61 lines). Documents only.

## What the orchestrator re-checked

**Disassembly**, with `tools/disasm.py` (Capstone, the reader the executor used) on GOG `AnnoWelt.dll`, SHA-256
`f220a878…e6e2`, the same file the document names:
- **Constructor `0x100746a0`:**
  - it calls the Farm constructor `0x1006f160`;
  - it zeroes output `+0x18`, `+0x1a`, activity `+0x1e`, raw `+0x1c`, and the queue bytes `+0x44` and `+0x45`;
  - it installs table `0x100c9418` at `0x100746c8`;
  - it sets switch `+0x22` = 1, the launch offsets `+0x48` = 2.0f and `+0x4c` = 6.5f, and the selected index
    `+0x50` = -1.

  All as written.
- **The tick `0x10073720`:**
  - **gate:** `cmp cx, 0x1f4` at `0x10073737`, then the stamp += 500 once, then switch `+0x22`;
  - **boat queue:** head active byte `+0x3c`; progress `+0x3e` below `0x6400` += `0x355`. On crossing, type
    `0x5e7` (1511) is resolved and `0x10073d50` called. The head is removed whether or not that succeeded;
  - **activity decay:** trunc(a·127/128);
  - **hunt dispatch:**
    - index ≠ -1, the boat's `+0xa8` returns 0, and raw `+0x1c` below the type's capacity (`+0xc0`);
    - `0x10071da0(8)`, route `0x2d` (45) through `+0xa4`, the first job's target set with operation 2, then the
      route installed through `+0xac`;
  - **conversion:**
    - output ≥ capacity blocks it;
    - r = min(128, raw·128/need) with need from `+0xe4`, and r < 64 takes the missing-input arm;
    - debit trunc(r·need/128), and a zero debit returns;
    - the output rate comes from `+0xe0`.
  - The hunt check comes before the output-full check, as the document says.
- **Replenishment `0x10066e50`:**
  - elapsed ≥ `0x9c4` (2,500), stamp += 2,500 once;
  - three pointer slots at `+0x28`, with retry bytes at `+0x34 + i`, each incremented, creating at ≥ 10;
  - creation passes the centre `+0x10`/`+0x18` and id -1;
  - on success the pointer is stored and the byte zeroed.

  No draw.
- **Creation `0x10067000`:**
  - height from the global `0x100e2a50`;
  - figure type `0x4b2` (1202), created through the manager's `+0xa0`;
  - home property `0x2f`, then the route requested through `+0xa4` and installed through `+0xac`.
- **The `WHALE` loader:**
  - the entry count is size / 6 (multiplication by `0xaaaaaaab`, shift 2);
  - the island-property gate is `push 0x10`, island `+0xc4`, skip when nonzero, at `0x10066c82`–`0x10066c90`.
- **The building loader `0x100741a0`:** reads one version byte and compares it with 1.

**Data**, with `tools/anlagen_roads.py` on this PC's `Anlagen.dat`:

| Record | Footprint | Bricks / wood / tools | Gold | Radius | Flags | HP | Capacity byte | Figure | Makes ← from |
|---|---|---|---:|---:|---|---:|---:|---:|---|
| whaler 821 | 4×4 | 5 / 5 / 5 | 500 | 60 | `0x2087` | 5,000 | 10 (2,560) | 1511 | WALSPECK ← WALFISCH |
| oil workshop 1028 | 3×3 | 5 / 5 / 5 | 500 | 20 | `0x84` | 6,000 | 5 (1,280) | 203 | LAMPENOEL ← WALSPECK |
| stall 1317 | 1×1 | 0 / 1 / 1 | 50 | 4 | `0x84` | 2,500 | 5 | 0 | LAMPENOEL, no input |

All as in `whaler.md` §1.

**`main`:** `src/sim/src/scenario_formats.cpp` keeps only ore occurrences (`bewohntype != 145` → `continue`, with the
comment "SCHATZ, WALSYMBOL, VULKANSYMBOL…"). So the whale sources are dropped at import, as §3 says.

**Not re-checked** (taken on the executor's reading):
- the slot-count comparisons of the class tables (64/75, 92/101, 20/22);
- the launch-candidate search `0x10073d50`;
- the boat-header field offsets of the loader;
- the `Single_08.sze` sample offsets;
- the figure records 1202, 1511 and 1559.

**CI:** docs-only CI green on `df63c97e` (run 43384161780): links 11 of 11, `git diff --check` clean.

## Notes (no fix round)

- **Ghidra and the History Edition reading are NOT_RUN**, as in the wildlife packets: there is no Java on the Mac,
  and the DLL is on the PC. The class names are therefore the comparison's labels, matched by structure; OQ-591
  keeps that open.
- **The prose is dense:** numbers run into words ("kind147", "need16 ->17"). It is readable with the tables, so it is
  not a finding. A later edit may add the spaces.
- **Open points, all well scoped:**
  - OQ-591: the History Edition name confirmation;
  - OQ-592: what island property 16 means, and where whale sources may be placed;
  - OQ-593: who requests a whaling boat, and what happens when that fails;
  - OQ-594: the indirect draw and float closure on the boat's path.

  OQ-595 to OQ-598 are unused.
- **Found in passing, not read:** two native records, Anlagen 3501 (PIONIER) and 3515 (PLANTAGE), also make
  WALSPECK, from EISFISCH. Whether they are reachable in the GOG game is open; a later read should settle it before
  the port assumes the whaler is the only source of blubber.

## Disposition

ACCEPTED. It lands on `main` with the next docs landing, on the owner's OK. The next read for the Mac lane is
WHALE-RE-1b (the whale's own life, the boat request, and the open callbacks). It is released when the owner gives the
go.
