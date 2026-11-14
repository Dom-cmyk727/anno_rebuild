# FISHER-1a, the orchestrator's review (2026-11-14)

**Verdict: ACCEPTED for landing.** Landing waits on two things:
- the game-file run of the landing candidate on the Windows PC;
- the owner's landing OK.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/FISHER-1a` `9137eabd`.
- It is based on `main` `f54e36f0`.
- Hosted CI run 43617546036 on `9137eabd` is green in all 12 jobs, `gate` included.

## What the orchestrator checked

- **Scope.** The diff touches 6 files:
  - the new `src/sim/include/anno/sim/fisher.hpp` and `src/sim/src/fisher.cpp`;
  - the new `src/sim/tests/fisher_test.cpp`;
  - `src/sim/CMakeLists.txt`, `tools/lint_sim_allow.txt` and REPORTS.

  No binary file, no change under `fixtures/`, no `World` member, no extension tag, no state number and no wiring.
- **The functions, against `fisher.md` and `fisher_ref.py`.**
  - **`fisher_corner`** answers one logical corner without building a grid. It copies the interior sample, gives 0 on
    the last row and column, and refuses a sample count other than W × H or a corner outside the grid (§5).
  - **`fisher_cell`** computes the unsigned four-corner sum, integer-divided by 4. Water is `mean < threshold`. The
    goal mask is 0 for water and 511 otherwise. Equality is a goal and closed to walking (§3 with its correction).
  - **`fisher_cell_at`** composes the two.
  - **`fisher_search_rect`** gives `x + (w − 1) / 2 − r` and `2r + 1 + (w − 1) mod 2`, narrowed to `u16` and `u8`.
    That is exactly the reference's `& 65535` and `& 255`: no clamp. A zero footprint returns nothing.
    - It accepts `u32` footprints and radii beyond the reference's domain (width 1–255, radius a byte) and wraps them
      the same way. Within that domain the results are identical.
- **The threshold.** `kFisherWaterThreshold = 32`, with its addresses, is the default of a parameter. A test shows that
  33 moves the boundary. The lint allowance names the fact and its address, like the `mason.hpp` entries. Accepted.
- **The tests.**
  - **The vectors:** every `sea_cell` (6), `corner_grid` (5) and `search_rect` (5) vector runs. The other four types
    are skipped by name, and an unknown type fails. The counts are asserted as minimums, and the border means 31 and
    32 must both have run.
  - **Unit cases:** the threshold, the unsigned mean at 255, a 1 × 1 island's borders, refused sizes, and the
    rectangle wrapping at both ends, including a size of 0 from wrapping.
  - **The corpus:** it asserts 50 scenarios, 0 refused, 2,473 fish tiles, 2,473 goals, 619 with a walkable water
    neighbour and 1,854 without. These are the Python census's counts (FISHER-RE-1c round 2).
- **The loader choice, the owner's decision.**
  - Normal `ImportRequired` loading refused 31 of the 50 scenarios on actor kinds the sim doesn't import yet. The
    executor stopped with the packet's QUESTION.
  - The owner chose option 1: the existing `ActorFreeComponent` projection, chosen before every load and never as a
    fallback after a refusal. It is the same production adapter for the embedded HEIGHT samples and placed objects.
  - So the test proves the embedded islands' geometry, not the whole-scenario import. The executor says so, and the
    comment in the test says so too.

## Notes (not blocking)

1. **The 31 refused imports are a real gap for the fisher's live run.** FISHER-1b runs the fisher in a world built
   by the normal loader. Its playable maps must import, or the packet must name the map it plays on.
2. **The four C++ mutants** the report mentions were run by hand, not kept as tests. The vector and unit tests
   reject each of them, as the report describes. Not re-run by the orchestrator.

## Game-file run of the landing candidate (2026-11-14)

**The candidate:** `orch/land-fisher-1a` at `198b5fb0`: `main` `674ecdf4`, then `task/FISHER-1a` `9137eabd`, then the
board up to `2e2f9ca2`. It was built locally and not pushed.
- `9137eabd` predates SAVE-SECTIONS-1b, so the merge met two conflicts where both added lines at the end of a file:
  `tools/lint_sim_allow.txt` and `REPORTS.md`. Both were resolved by keeping both sides. `tools/lint_sim.py`:
  `lint_sim: ok`.

**The run:** the canonical call, with the GOG oracle shots and the HE captures set, 18:44–19:13.
- `COUNTS: selected=2604 executed=2577 failed=0 empty=0 skipped=27 (required-asset=0 platform=3 opt-in=21 gpu=0
  other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: PASS`. Nothing was refused.
- Log sha256 `ba559f82897555fc…`.
- **On this PC's game files** the fisher tests printed:
  - `FISHER_VECTORS sea_cell=6 corner_grid=5 search_rect=5 skipped_other_types=33`
  - `FISHER_CORPUS scenarios=50 refused=0 fish=2473 goals=2473 adjacent_water=619 without_adjacent_water=1854`

## Not verified by the orchestrator

- The executor's runs:
  - the pins check: 1,650 executed, 0 failed;
  - the no-assets gate;
  - the 0-warning dev build;
  - the mutants.

  They were read from the report; the handoff file is on the Mac.
- MSVC with the game files. The landing's game-file run covers it.
