# FISHER-RE-1c: the water test's corner-height grid (OQ-200)

**Status:** RELEASED 2026-11-13 by the Claude Code orchestrator, with the owner's go, to **the Mac executor**. Step 10′ of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Why.** The owner asked for a fisher port behind a flag (2026-11-13). The orchestrator's check found:
- **The wiring needs a new state number.** Every `sim::Rules` field is written into the saved state
  (`state_fields.hpp`, `fields(Rules)`), and each new one so far took a new state number (`city_booking` "stream133",
  the freight rules "route-only136"). A fisher flag would too. By the owner rule of 2026-11-12, no packet takes a new
  state number until ADR 0006 is accepted. **The wiring (FISHER-1b) waits for ADR 0006,** like the wildlife and whale
  ports.
- **One original rule is still unread, and the port cannot invent it.** The fisher walks only on water cells
  ([`docs/re/fisher.md`](../../re/fisher.md) §3, "sea cells and goals"): the mean of a tile's **four corner heights**,
  read from a grid with a row stride of **width + 1**, against a threshold of 32. But `main`'s `Terrain::height` keeps
  the island file's **width × height** `HEIGHT` bytes. How the original turns those into the width + 1 corner grid is
  **OQ-200**, open since 2026-10-25 (`docs/open-questions.md`). `island.hpp` says the sim reads none of the terrain
  columns yet (OQ-060, "STAND-IN").

This packet reads that one rule, so the next, state-free packet can compute the water cells from the real data.

**Base:** `origin/main` at release. Branch `task/FISHER-RE-1c`. About 2–3 hours.
- **Not on `main` yet:**
  - `docs/re/fisher.md`: read it on `origin/task/FISHER-RE-1a`;
  - `tools/fisher_ref.py`: read it on `origin/task/FISHER-RE-1b`.
- **Merge both into your branch at the start** (ordinary merges). They land on `main` with the next landing.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover and re-checks addresses);
- one REPORTS entry of about 60 lines, READY FOR REVIEW;
- documents and the reference only: no engine code.

## Start from

- **`docs/open-questions.md` OQ-200 and OQ-060.**
- **[`docs/re/transport.md`](../../re/transport.md):**
  - §3.4.13 and the chain it records: initialisation `0x1008fea0..0x1008feb2` and `0x1008ff00..0x1008ff0e`, which
    gives the threshold 32, and the whole-tile closure `0x10092b40..0x10092c94`;
  - §3.4.11–§3.4.12, the cells.
- **`docs/re/fisher.md` §3:** the mode-3 cell builder `0x10090560..0x100906df` reads the corner grid at island `+0x14`,
  stride width + 1; the goal mask is `0x1009226b..0x100922df`.
- **[`docs/formats/`](../../formats/)**, the island format (`insel.md`, "Blocks"), and `tools/insel.py`: the file's
  `HEIGHT` block.

## Read (GOG `AnnoWelt.dll` 2.0.0.5; Capstone or Ghidra, as before)

1. **Who writes island `+0x14`:** the allocation (its size in bytes, as a function of the island's width and height)
   and every writer at load. Follow the island loader from the file's `HEIGHT` block to that buffer.
2. **The mapping, exactly.** For each corner (cx, cy), 0 ≤ cx ≤ width and 0 ≤ cy ≤ height, which file bytes it comes
   from, and how:
   - a copy;
   - a mean, and with what rounding;
   - the border row and column: a copy of the last tile, a constant, or a neighbour island's value;
   - any scale or offset.
3. **Whether anything rewrites it later:** building placement, terrain changes, or a save/load round trip. Say what
   the save holds: the file bytes or the corner grid.
4. **One worked island by hand.** Take an island with fish tiles from the census in `fisher.md` §1, for example from
   `Single_06.sze` or `S01_01_01.sze`. Compute the corner grid of a small window around one tile-11 placement, and
   classify that tile with the threshold 32. A fish tile should come out as water. If it doesn't, that is a finding:
   report it, don't adjust it away.

## Deliver

- **The mapping** in `docs/re/fisher.md`, as a new dated section, or in `transport.md` §3.4.13 as a dated
  bullet if it belongs there, with addresses and evidence tags. No disassembly text and no bytes.
- **OQ-200** answered, or narrowed, with a dated bullet. OQ-060 narrowed where this read settles part of the terrain's
  shape.
- **In `tools/fisher_ref.py`:**
  - a record type `corner_grid`: the file's `HEIGHT` bytes of a small island → the corner grid, as integers;
  - at least three hand-derived vectors in `fixtures/synthetic/sim/fisher_vectors.txt`, the border included;
  - the contract test extended, so a mutant of the border rule fails.
- **A real-file check** (`tools/insel.py`; opt-in, needs the game files). Over every tile-11 placement in the shipped
  scenarios (2,473, `fisher.md` §1), the derived corner grid classifies the tile as water. Give the count and every
  exception.
- **Open points:** new entries **OQ-609 to OQ-612** only. Invent nothing.

## Gate

`TASKS.md` §1 item 4: documents and `tools` only.
- `python -I tools/fisher_ref.py fixtures/synthetic/sim/fisher_vectors.txt` and the contract test, directly;
- both CTest tests through `run_selected.py --select 'fisher_reference'`;
- hosted CI green on your pushed head.

## Out of scope

- **any engine code:**
  - FISHER-1a, next: the water cells as state-free sim functions, not wired;
  - FISHER-1b, after ADR 0006: the wiring behind a flag;
- the History Edition reading.

## Report

READY FOR REVIEW, about 60 lines:
- the reads, with their addresses;
- the mapping;
- the worked island;
- the real-file count and its exceptions;
- OQ-200's new status;
- the gates;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
