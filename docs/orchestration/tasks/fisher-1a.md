# FISHER-1a: the fisher's water cells, goal mask and search rectangle as state-free sim functions

**Status:**
- **Written** 2026-11-14 by the Claude Code orchestrator for **the Mac executor**.
- **Released** when the owner pastes its prompt.
- **Place on the path to gameplay** (owner, 2026-11-14: "make sure we are heading towards game development
  progress"): SAVE-SECTIONS-1a → 1b → **FISHER-1a** → FISHER-1b, the fisher working in the game. Food from the fisher is
  the first missing need for 131 houses in the measured towns
  ([`progression-gaps.md`](../../sim/progression-gaps.md)).

**Base:** `origin/main` at release. Branch `task/FISHER-1a`, a new worktree. About 2–3 hours. Never rebase, force-push
or push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

**This packet adds no state.** No new `World` member, no extension tag, no state number, no pin moves. The functions
are pure: they read their arguments and return values. Nothing calls them from the scheduler yet; FISHER-1b wires
them.

## Why

The fisher (figure 142, route 45, farms 810 and 3978) walks on water cells and harvests fish tiles (resource tile 11)
from them. The reads are done and accepted (FISHER-RE-1a, 1b, 1c; [`docs/re/fisher.md`](../../re/fisher.md)). The
integer reference `tools/fisher_ref.py` and its 49 vectors define the rules. This packet moves the parts that need no
state into `src/sim`, checked against that reference and over the real maps, so FISHER-1b only has to wire them.

## Read first

- `CLAUDE.md`;
- `docs/testing/evidence-contract.md`;
- `docs/re/fisher.md`:
  - §1, the farms and the fish tile;
  - §3, "target", "specialized search" and "sea cells and goals", **with the dated correction of 2026-11-13** (the goal
    mask's sense);
  - §5, the HEIGHT corner grid;
- the FISHER-RE-1c review (`docs/orchestration/reviews/2026-11-13-fisher-re-1c-review.md`): the corrected rule and the
  corpus counts;
- `tools/fisher_ref.py`: the records `sea_cell`, `corner_grid` and `search_rect`, and `--check-fish-water`;
- `fixtures/synthetic/sim/fisher_vectors.txt`;
- `docs/open-questions.md` OQ-603 (float positions), OQ-609 (the inner bank tiles), OQ-610 (later corner writers);
- `src/sim/include/anno/sim/island.hpp` (`Terrain`: `height` holds the W × H HEIGHT samples).

## Do

1. **The corner grid** (`fisher.md` §5). A function from an island's width, height and its W × H HEIGHT samples to the
   logical (W + 1) × (H + 1) corner grid:
   - interior corners copy the samples unchanged;
   - the last column and the last row are 0;
   - no mean, scale, offset, edge replication or lookup into a neighbouring island.

   A function that answers one corner without building the grid is fine too, if the tests cover both edges.
2. **The cell rule** (`fisher.md` §3 with its correction; `AnnoWelt.dll` `0x10090560`–`0x100906df` and
   `0x1009226b`–`0x100922df`). From a tile's four corners:
   - **mean** = the sum of the four unsigned bytes, integer-divided by 4;
   - **walkable water** (mode 3, layer 2): mean < threshold;
   - **goal mask:** 0 when mean < threshold, 511 when mean ≥ threshold.

   A tile with mean equal to the threshold is a goal and is not walkable.
3. **The threshold.** The original computes it at initialization: byte(trunc(`[0x100e34dc]` / f32(0.04))) = 32
   (`0x1008fea0`/`0x1008ff00`).
   - It is a value of the original, not a limit. Keep 32 as a named default with its addresses, passed into the cell
     rule as a parameter, so a rules option or a mod can change it later.
   - Where it lives is your call (a `SimConfig` field or a parameter with a documented default), with the reason in
     "Decisions and reasoning". If `tools/lint_sim.py` needs an allowance, write it like the `mason.hpp` entries in
     `tools/lint_sim_allow.txt`: the fact and its address.
4. **The search rectangle** (`fisher.md` §3, "target", `0x10059210`–`0x10059398`). From the farm's origin, footprint
   (w, h) and radius r:
   - origin x + floor((w − 1) / 2) − r, and likewise for y;
   - size 2r + 1 + (w − 1) mod 2, and likewise for h.

   An unrotated 3 × 4 farm with radius 7 gives origin (x − 6, y − 6) and size 15 × 16. Integer inputs only. Float
   positions stay refused, as the reference refuses them (OQ-603). The original stores the origin in `u16` and the
   size in bytes: say in the code what the rebuild does at a map edge or with a radius whose size doesn't fit, and
   test it. Don't invent a clamp the reference doesn't have; if the reference is silent, stop with a QUESTION.
5. **No hardcoded limits** (CLAUDE.md). No map size, island size or radius is fixed; every size comes from the
   arguments.

**Not this packet:**
- any wiring into the scheduler, the farm tick, the figure or its route;
- any state, extension tag or pin;
- the farm tick, `converted`, `trip` and `cut_match` records (FISHER-1b, with the wiring);
- the inner bank tiles (OQ-609) and the later corner writers (OQ-610).

**Stop with a QUESTION if:**
- a function needs state or a `World` member;
- the sim's loaded world doesn't give you the tile-11 placements and the HEIGHT samples for the corpus test below,
  without adding state;
- a rule here disagrees with `fisher_ref.py` or its vectors.

## Proof

1. **The vectors.** A C++ test reads `fixtures/synthetic/sim/fisher_vectors.txt` and checks every `sea_cell` (6),
   `corner_grid` (5) and `search_rect` (5) vector against the new functions.
   - It skips the other record types by name, and fails if any of the three types has fewer vectors than these
     counts. An empty selection is a failure, not a pass.
   - The border means 31 and 32 are in the vectors; check that both are among those the test ran.
2. **Unit cases of your own:** the threshold as a parameter (a different value moves the boundary), the corner grid's
   last row and column, a 1 × 1 island, and the edge behaviour from step 4.
3. **The corpus test over the game files** (the C++ counterpart of `fisher_ref.py --check-fish-water`; the FISHER-RE-1c
   review asked a port's canary for it):
   - Load the 50 shipped scenarios through the sim's own scenario loader, the path the game uses.
   - For every tile-11 placement: it is a goal (mean ≥ 32), and count those with at least one in-bounds walkable
     water tile among their eight neighbours.
   - **Assert** 50 scenarios, 2,473 fish tiles, all 2,473 goals, and **619** with a water neighbour.
   - **Report** the 1,854 without one (OQ-609) as a count, not a failure.
   - It reads `ANNO1503_GAME_DIR` and skips with a message when unset, as the other corpus tests do.
   - If your counts differ from the Python census, stop with a QUESTION and both lists of differences.
4. **The gates.** The new and changed sim tests through the canonical runner with the game files, with classified
   counts.
   - A test refused for memory did not run: rerun it alone with `--jobs 1`.
   - `pins.py check` on the final head: no pin moves.
   - The 0-warning `dev` build.
   - Hosted CI green on the head, `gate` included.

**Not yours:** the full suite and the game-file run on the Windows PC (the orchestrator's, at the landing).

## Report

READY FOR REVIEW, about 60 lines:
- the functions and their signatures;
- where the threshold lives, and why;
- the edge behaviour of the search rectangle;
- the vector counts per type, and the corpus counts;
- the gate lines as printed, and the `pins.py check` lines;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
