# Hunter (JAGDHAUS) and wildlife: what is read and what is not (MAP-NEEDS-2 N0, 2026-11-05)

Module: `AnnoWelt.dll` (sha256 `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`), disassembly `cache/map2-research4/veto/AnnoWelt.dll.full.asm` (local, ignored). Marks: **D** read from bytes, **P** parsed from the files, **U** unread.

## Facts
- **D** `JAGDHAUS` (PRODTYPE 5, type 805) has its own runtime class; its tick is `0x10069f40` (gate `(u16)(t - [esi+0xa]) >= 0x1f4` = 500, `0x10069f5d`; `last += 500` at `0x10069f68`). `types.cpp` maps it to `TickKind::None`; the building is inert in the sim.
- **P** Type 805: 3x3, radius 8, input `FLEISCHTIER` (goods 137), outputs `NAHRUNG` (26) and `TIERHAEUTE` (18), per step in 57 -> out 43 / 26 (8.8), capacity 1024/1024, time 768/1280 /128, max figures 1, figure type 108 (ROUTETYPE index 12, layer 2, capacity 256). The fur trapper 824 (`PELZJAEGER`, `PELZTIER` -> `PELZE`) sends the **same figure type 108 / route 12**; the engine's `FurTrapper` tick is ported but never sends (no target: `driver.cpp`, O-25).
- **D** (shape only) `0x10069f40` after the gate: returns when both line bytes `[+0x28]`/`[+0x29]` are 0; decays the activity words `[+0x20]`/`[+0x22]` by `x*127>>7`; returns when flag bits 4/5 of `[+0xc]` are set; computes a rate `bl` from the raw store `[+0x24]` (u16) and the type property pair `0x44f`; produces into `[+0x18]`/`[+0x1a]` with the line shares `[+0x2a]` and `128 - [+0x2a]`; updates the blocked flag bit 3; at the end, when `[+0x2c]` (workers out) is 0 and a raw-stock test passes (`0x180 / type slot +0xe0` against `[+0x24] / slot +0xe4`), asks `0x10071da0(8)` for a target and creates a figure with job `0xd` through the building's slot `+0x118`. **Not decoded:** the exact production arithmetic, which unit the raw store counts, the figure creation arguments.
- **D** `0x10071da0(8)` is the radius search "nearest animal of the input goods" (`docs/re/production.md` section 4.2a); body **U**.
- **P** The scenario islands carry a `HIRSCH` record (the string at `0x100dcf24` in AnnoWelt; island templates `.scp` have none): 668 records in the 50 `.sze`, 111,352 bytes = 13,919 entries of 8 bytes; per island 15 .. 1,570 entries, many scenario islands 0 (`cache/map-needs2/N0/n0_scenes.txt`).
- **D** The reader around `0x100946c0` (loads `HIRSCH`, count = size >> 3) creates one 16-byte object per entry (vtable `0x100cac04`, init `0x1007bd50`), puts it in the island's 8x8 cell grid (`[ebp+0x94]`, row stride `[ebp+0x8c]`), fills `[obj+8]` (cell column/row bits), `[obj+9]` (a type byte: from the entry or a draw of the world generator `[ebp+0x20]` slot `+0xb4` masked `& 0x1f` plus a base) and `[obj+0xe]`. These are **map objects of the island, not figures**.
- **Documented** (`docs/sim/state.md` section 12, O-12, O-25): wildlife objects and their "fauna sweep" (`WildlifeSweep`) are unported; the sim creates no animals.
- **P** Figure types 501-505 and 514-517 have ROUTETYPE index 62 (layer 4), 506-509 route 46 (layer 2): candidates for animal figures; their relation to `HIRSCH` objects is **U**.

## Unread (exact questions)
1. Does `HIRSCH` entry layout (8 bytes: position within the 8x8 cell, type) map to `FLEISCHTIER` goods 137, and which function reads an animal's goods (the target search `0x10071da0(8)`)?
2. The body of job `0xd` (hunter figure): target claim, kill, return, which outputs are credited where, and the cleanup.
3. The fauna sweep: do hunted animals respawn, move, or only exist from load?
4. The production arithmetic of `0x10069f40` (units of `[+0x24]`, property `0x44f`).
