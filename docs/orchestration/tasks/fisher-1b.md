# FISHER-1b: the fisher walks on water, catches fish and brings them home, behind a default-off rules flag

**Status:**
- **Written** 2026-11-14 by the Claude Code orchestrator for **the Mac executor**.
- **Released** when the owner pastes its prompt.
- **ADR 0006's first real feature:** its flag lives in an extension tag, so with the flag off every save, hash and pin
  stays byte for byte as it is.
- **The path to gameplay** (owner, 2026-11-14): FISHER-1a (the geometry) → **FISHER-1b** (the behaviour, proven on a
  synthetic island) → FISHER-1c (the real-file canary: a shipped map where fishers deliver food).

**Base:** `origin/main` **after FISHER-1a has landed**. Check that `git merge-base --is-ancestor 9137eabd origin/main`
succeeds; if not, stop and report. Branch `task/FISHER-1b`, a new worktree. About 4 hours. Never rebase, force-push or
push `main`.

**Rules:** the owner rule of 2026-11-12 on the board ([`TASKS.md`](../TASKS.md)):
- one concern;
- no reviewer subagents;
- one REPORTS entry of about 60 lines, READY FOR REVIEW.

**If it passes 4 hours,** stop at the end of the current step, commit what passes, and report what is left. The
orchestrator splits it.

## Why

Food from the fisher is the first missing need for 131 houses in the measured towns
([`progression-gaps.md`](../../sim/progression-gaps.md)). The reads are complete (FISHER-RE-1a–1c,
[`docs/re/fisher.md`](../../re/fisher.md)). FISHER-1a gave the state-free geometry (`fisher.hpp`). This packet makes the
fisher run, behind a flag that is off by default.

## Read first

- `CLAUDE.md` (determinism, no hardcoded limits, faithful core separate from improvements);
- `docs/testing/evidence-contract.md`;
- [ADR 0006](../../adr/0006-save-sections.md), items 1–3 and line ~101 (the fisher behind a default-off flag);
- `docs/sim/state.md` §9.8 (the extension tag registry and the registration interface);
- `docs/re/fisher.md`:
  - §2: the dispatch, with its one discarded draw, and the special creator `0x10067e60` (the first rotated entrance);
  - §3: route A/B/C, the target search, the mode-3 sea cells and goals, movement, catch, return and receipt;
  - §4: the stops, the draw ledger, and "Main versus a later faithful port";
- `docs/re/forestry.md` §§2.5–2.13 and `docs/sim/small-farm-supply.md` §2.3 (the shared machinery the fisher reuses).

## Where the code is (`main` at the base; check the line numbers)

**Refusals to lift, only with the flag on:**
- `figure.hpp` `farm_figure_route`: `0x2d` → `Unsupported`;
- `driver.cpp` `consume` (about lines 530–563): the refused send, `forestry.unsupported_routes`, and `s.harvest`
  only for Worker, Animal and Mason routes;
- `figure.hpp` `harvest_state_job` (0x29, 0x2a, 0x2e, 0x2c) and `figure.cpp` `validate_figures`: harvest state on
  any other job is refused;
- `figure.hpp` `harvest_search_limit`: 20 only for the animal job (`0x2e`). The fisher's limit is also 20 (§3,
  `0x1005932d`);
- `types.cpp` `figure_type_from_entry`: the `kBewohn` table has no FISCHER, so its kind is 0. Fix it only if the fisher
  needs the kind, and say why.

**The machinery to reuse:**
- `harvest.cpp`: `harvest_tick`, `Body::search`, `claim`, `cut`, `start_home`;
- `HarvestState` (`figure.hpp`);
- `scheduler.cpp` (the tick dispatch and the tile-edit merges);
- `transport.cpp` `path_home`.

**The cells.** `WalkCells` (`walk_cells.hpp`/`.cpp`) has one cell builder, `WalkCells::chunk` (around line 439), the
original's mode 0. The mode-3 sea cells go next to it. The start floods use `cells.record()`.

**The figure state is generic and already serialized:** `fields(HarvestState)` and `fields(Figure)` in
`state_fields.hpp`. A fisher needs no new per-figure field. If you find one it does need, stop with a QUESTION.

## Do

1. **The flag, as the first real extension (state.md §9.8).**
   - Reserve **tag 1** in the **header** section, name `fisher_rules`. The orchestrator has reserved it on the board.
     Add its row to the §9.8 registry: tag, section, name, this packet, implemented, payload fields.
   - The payload is one byte: 0 = off (the default), 1 = on. Any other value fails `validate`.
   - The flag's state lives in `ExtensionContext`. A rules input sets it at `World::create`. `fields(Rules)` and the
     Rules digest don't change. **The flag off must write today's bytes.**
   - Fix the two `world.hpp` comments from the SAVE-SECTIONS-1b review:
     - the 139 history line's missing spaces ("versions1" and others);
     - `field_list_canary` should say that a new member goes into its section's field list, with that section's
       version bumped, or into the extension context.
2. **The mode-3 sea cells** (§3, `0x10090560`–`0x100906df` and `0x1009226b`–`0x100922df`, through FISHER-1a's
   `fisher_cell`).
   - **Walkable:** a tile is walkable for layer 2 (cell `0x00000fff`) when its mean is below the threshold, and closed
     (`0x00070000`) otherwise.
   - **Goals:** the goal mask is 511 or 0.
   - Add this as a cell-building mode of `WalkCells`, next to mode 0, and the matching `record()` for the start floods.
   - **Mode 0 must not change:** its cells, costs and the order of their builds stay as they are. The pins prove it.
3. **Dispatch and launch** (§2), only with the flag on:
   - **The dispatch:** the farm's one discarded draw stays where it is, flag on or off, as today.
   - **The launch:** the fisher spawns at the building's **first rotated ENTRANCE** vector plus its position (the
     special creator `0x10067e60`), not at `spawn_position`.
   - **The cargo goods** are input 0, FISCH (71).
   - **No retry:** a launch that can't route is not retried locally.
4. **Search, catch, return** (§3), only with the flag on:
   - **The search** covers `fisher_search_rect`'s rectangle: unclaimed tiles whose output is FISCH 71 (so only tile
     11), with mode-3 cells, starts at the entrances inside the home footprint, the limit-20 callback, and the claim.
   - **Zero candidates** returns before any path setup. **A failed search** takes the home fallback (§4).
   - **The catch** is the shared wait, then cut 11 → 10, with +256 cargo only on success. Repeat while cargo is below
     the capacity (768); otherwise go home.
   - **The return and receipt:** the shared `0x10057ae0` searches the home's entrances, and the receipt adds the
     cargo to the farm's raw stock, a `u16` add without a clip. The farm tick converts it; nothing credits food
     directly.
   - **Movement** keeps the segment's Y (§3, `0x10038730`). The rebuild's positions have no Y, so say what that
     means here (OQ-603).
   - **Which cells the return path uses** for a figure on water is not stated in `fisher.md`. Settle it from
     `0x10057ae0` with `tools/disasm.py`, and record it in §3 with addresses. If you can't settle it, stop with a
     QUESTION.
5. **The draws** (§4's draw ledger). The fisher's own bodies take **no** direct draw. Flag on or off, the number and
   order of draws are the same as today. Check it in a test.
6. **No hardcoded limits.** The capacity, the search limit, the threshold and the radius come from data or the
   defaults FISHER-1a set, never new constants.

**Not this packet:**
- a real-file canary on a shipped map (FISHER-1c);
- the inner bank tiles (OQ-609);
- fish on maps that have none (see "Note for the owner" below);
- any change with the flag off.

**Stop with a QUESTION if:**
- a pin, golden or save would move with the flag off;
- a fisher needs a new persistent per-figure field;
- mode 0's cells or costs would change;
- the return path's cell mode can't be settled;
- the work passes 4 hours.

## Proof

1. **Flag off = today.** `pins.py check` on the final head: no pin moves. The fixtures diff is empty.
2. **The flag as an extension:**
   - flag on, save → load → save gives the same bytes;
   - the header digest changes and the other nine don't;
   - a payload of 2 is refused;
   - flag off writes no extension bytes.
3. **The behaviour, on a synthetic island** (extend `tests/harvest_helpers.hpp`: heights for a small bay, a FISCHER
   farm with route 45 and capacity 768, and fish tiles 11 on shallow goal tiles beside walkable water):
   1. The fisher launches at the first rotated entrance.
   2. It finds and claims a fish tile, waits, cuts 11 → 10 and gains 256. Three cuts make 768, and then it goes home.
   3. The farm's raw stock rises by the cargo, the farm tick turns it into food, and the counter is released.
   4. **No fish:** nothing launches past the search, and the fallback is taken.
   5. **No water path:** no claim is set.
   6. **The draws** are the same with the flag on and off (item 5 above).
   7. **Determinism:** two runs give the same hash. A save mid-trip loads and finishes the trip with the same hash
      as the uninterrupted run.
4. **Mode 3 against mode 0:** on the same island, a land worker's paths are unchanged, and the fisher's cells are
   exactly `fisher_cell`'s answer, tile by tile.
5. **The gates:**
   - the changed sim tests through the canonical runner, with the game files and classified counts;
   - a test refused for memory did not run: rerun it alone with `--jobs 1`;
   - the 0-warning `dev` build;
   - hosted CI green on the head, `gate` included.

**Not yours:** the full suite and the game-file run on the Windows PC.

## Note for the owner (not for the executor)

**`lan5` has no fish.** Its five island templates hold no tile 11 or 10 and no fisher farm. So the fisher will do
nothing on the LAN map until fish exist there.
- `fisher.md` §5 notes a lead: runtime fish creation on low empty ground (`0x100928a0`).
- That is a separate read (FISHER-RE-2) and an owner decision.

## Report

READY FOR REVIEW, about 60 lines:
- the flag and its extension;
- the mode-3 cells;
- launch, search, catch and return;
- the return path's cell mode, with addresses;
- the draws;
- the tests, the `pins.py check` lines and the gate lines as printed;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
