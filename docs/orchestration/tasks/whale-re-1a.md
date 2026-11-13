# WHALE-RE-1a: the whaler, its whales and the lamp-oil chain (documents only)

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator, with the owner's go, to **the Mac executor**. Step 5′
of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Why:** 162 of the 317 measured houses stop first on a forced consumer good
([`docs/sim/progression-gaps.md`](../../sim/progression-gaps.md), row 1). Wildlife (leather, pelts) has been read
(WILDLIFE-RE-1a to 1d). Lamp oil (`LAMPENOEL`) needs the whaler, which nobody has read: on `main` its class is inert
(`docs/sim/state.md`, the class table: `15 WALFAENGER | Whaler | None`). The
[project analysis](../reviews/2026-11-12-project-analysis.md) §7.4 says a later whale packet can reuse the wildlife
method; this is its first read. The port comes later, behind a flag, after ADR 0006.

**Base:** `origin/main` (`96f7980c` when this was written). Branch `task/WHALE-RE-1a`. About 3–4 hours. This packet is
on the board branch `origin/orch/board-next` until the next docs landing; read it there.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover and re-checks addresses);
- one REPORTS entry of about 60 lines, READY FOR REVIEW;
- documents only: no engine code, no tests, no fixtures.

## Start from

- [`docs/re/wildlife.md`](../../re/wildlife.md) and [`docs/re/hunter.md`](../../re/hunter.md): the method, the evidence
  tags, and how routes (`tRoute*`) and their jobs (`tJob*`) were mapped. Reuse their findings; don't repeat their
  reads.
- [Edition comparison](../../research/he-vs-gog-comparison.md) §4: the History Edition names
  - `tProdWalfang`, `tBewohnerWal`, `tRouteWal`, `tRouteWalfaenger` and `tOccurrenceWal`;
  - none of them is located in GOG yet.
- **What `main` already records about the whaler:**
  - `docs/sim/state.md`, the class table: ctor `0x100746a0`, object size `0x54`, tick `0x10073720` (unread);
  - `docs/re/production.md`: the tick's stamp gate is 500 (at `0x10073737`);
  - `docs/re/economy-booking.md`: vtable `0x100c9418`, and loader `0x100741a0` (version 1, a 10-byte inner prefix);
  - `docs/re/ship-placement.md`: `WALFAENGER` (PRODTYPE 15) is a coast building, rotated up to four times;
  - `docs/re/ship-founding.md`: whalers may be built on polar islands;
  - goods 22 `WALSPECK` (`docs/sim/state.md`), and `WALFISCH` among the figure kinds in `docs/re/production.md`.
- `analysis/pe_symbols.json`: RTTI and vtables of the original modules.

## Read (GOG `AnnoWelt.dll` 2.0.0.5 is the reference; Ghidra as before)

1. **The data, from the original files** (with `tools/figuren.py`, `tools/anlagen_roads.py` and the raw files, as in
   `docs/sim/gold-gems.md`):
   - the whaler building's record: id, PRODTYPE, footprint, costs, what it makes, its figure;
   - the whaler's own figure (a ship or a walker; its route type) and the whale's figure type;
   - the building that turns `WALSPECK` into `LAMPENOEL`: its id, PRODTYPE and runtime class. Say whether that
     class is already ported on `main` (`docs/sim/state.md`, the class table). If it is, lamp oil lacks only its
     raw material.
   - the market stall that sells lamp oil, and whether it is ported.
2. **The classes in GOG:** locate `tProdWalfang`, `tBewohnerWal`, `tRouteWal`, `tRouteWalfaenger` and
   `tOccurrenceWal` by the comparison's method (RTTI where it exists, otherwise structural slot matches against
   HE86's names). Give each one's vtable and evidence tag, and say how sure each match is.
3. **Where whales come from:**
   - what `tOccurrenceWal` is: a scenario record, a world occurrence, or a sweep like the fauna sweep `0x10093480`;
   - when whales are created and how many;
   - every world-PRNG draw on that path: its position, its modulus, and what it selects;
   - whether there is a cap or a count of living whales;
   - where in the sea they may be.
4. **The whaler's tick `0x10073720`** after its 500 gate:
   - what it checks;
   - what it sends out, and by which route and job objects;
   - how blubber reaches the building's store;
   - every draw it makes;
   - what stops it (no whales in range, a full store).

   The loader `0x100741a0` field by field: extend `economy-booking.md`'s note, and don't edit it.

**The History Edition second reading** (`AnnoWeltAddOn.dll`, "HE86") is optional: the file is on the Windows PC, not
the Mac. Use the comparison's class names as labels, and mark any HE86 reading NOT_RUN.

## Deliver

- **A new `docs/re/whaler.md`** with one section per read above.
  - Every claim carries its address and an evidence tag, as in `wildlife.md`.
  - No disassembly text and no bytes.
  - Link `wildlife.md`, `hunter.md` and `economy-booking.md`; don't edit them.
- **Every draw on the whale and whaler paths as a table** (position, modulus, what it selects, the condition under
  which it happens), exact enough that a later packet can turn it into reference vectors.
- **The list of what a port needs** (classes, data, draws, state), and which of it is already on `main`.
- **Open points** as new entries in `docs/open-questions.md`, numbered **OQ-591 to OQ-598** (reserved for this
  packet). When a read does not settle a rule, record what is known and stop that item. Nothing is invented.

## Gate

Docs only:
- `git diff --check`;
- links that resolve;
- hosted CI green on your pushed head (the docs-only `gate`).

Read CI after every push.

## Out of scope

- the whale's own life after creation (its movement and death), if item 3 does not need it: WHALE-RE-1b;
- the reference script and its vectors: a later packet;
- any engine code (the port, after ADR 0006);
- the HE86 reading.

## Report

READY FOR REVIEW, about 60 lines:
- the reads done, with their addresses;
- what is settled;
- the open points, with their OQ numbers;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
