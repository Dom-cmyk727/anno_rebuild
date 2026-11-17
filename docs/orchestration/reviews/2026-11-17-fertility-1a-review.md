# Review: FERTILITY-1 stage 1a (placement class for single-quad tile objects)

Reviewer: orchestrator (Claude Code), 2026-11-17. Head `3637c288` on `task/FERTILITY-1a`, base
origin/main `4dfdd9b7`. CI run 44106292573: green on that head.

**Verdict: ACCEPTED.**

## What was checked

- **The rule matches the read.** `Class::TileObject` is selected by the type's own flag word `& 3 == 1`
  before the ordinary test, as `AnnoGame 0x10008fa1` branches before any window or class test. The
  height is the floor of the four-vertex mean of the record's origin tile, and the level test is
  skipped for the classes in `level_exempt_classes` and group `0x141`. Every constant cites its address;
  the 32 now traces to the runtime globals of `0x1007b340`/`0x1007b3a0`.
- **No shipped non-tile type reaches the new branch.** The shipped words of the other classes (`0x84`,
  `0x04`, `0x2087`, `0x4186`, `0x4386`, `0x4087`, `0x1886`, `0x1186`, `0x1386`, `0x3096`) all have
  `& 3 != 1`, and the land-road test still runs first. A `BuildCmd` never reaches the class
  (`apply.cpp` step 1), so no command result moves.
- **The fixture changes are complete and explained:** R-20 (a data-defined STRASSE `0x4185`) and the
  `0x0085` predicate row, both `F & 3 == 1`; no golden, replay, scenario or state hash moved.
- **The corpus fixture holds counts and FNV digests of our own results only**, no file content (the
  `fisher_runtime_census.txt` precedent). C++ = oracle = file on 2,266/2,266 lines, run locally with
  the game files; CI skips it without them, as designed.
- **Failure proof:** the ceiling mutation fails 3 cases and 1,299 corpus lines; the exemption mutation
  is caught by the vectors, which the report correctly says the corpus cannot catch.
- **Gate:** run_selected 446 executed, 0 failed, 2 opt-in skips; WSL GCC/Clang clean; CI green.

## Nits (non-blocking, fix in stage 1b)

- `placement.cpp`'s comment above the `TileObject` test was reflowed by clang-format into
  "flag word &" / "3" / "== 1" on three lines; rewrite it so the expression stays on one line.
- `level_exempt_classes` is read by the tile-object branch only, although `0x1007cbc2` applies to every
  class in the original. That is documented in the header and harmless for the shipped words; say so in
  `docs/sim/ship-placement.md` §2a too.

## NOT_RUN (accepted as stated)

`placement_emulate.py` re-run (no `unicorn`), the WSL `-O1` sanitizer lane, macOS. The landing
candidate's PC lanes and the Mac's full run cover the rest.
