# Ship route engine: the C++ port (MAP-NAV-2 package C)

Status: implemented and tested; a pure, stateless library (no world state, no serialization, no clock, no PRNG, no global).
Spec: `docs/sim/ship-navigation.md` sections 5, 12.3, 12.6; evidence: `docs/re/ship-navigation.md` sections 9-12.

## Files

| file | content |
|---|---|
| `src/sim/include/anno/sim/route.hpp` | `RouteLimits`, `RouteCostTable`, `TileSource`, `TileMapSource`, `CoastIndex`, `RouteQuery`, `RouteResult`, `plan_route`, `route_density`, `pack_edge_mask`, `disc_half_widths`, `route_detail::*` (test hooks) |
| `src/sim/src/route_coast.cpp` | validation, tile bitmap source, the 12-bit edge packer, the disc table, the sector index and its incremental rebuild |
| `src/sim/src/route_plan.cpp` | window, start nibble, goal disc, coarse FIFO Dijkstra, goal callback / Offer / candidate tables, corridor, fine Dijkstra, learn table and retries, trace |
| `include/anno/sim/route_world.hpp`, `src/route_world.cpp` | `WorldTileSource`: the original land rules over the sim's islands |
| `tools/ship_nav_emulate.py` | generator of the emulated-original fixtures (needs the private unicorn harness; not run in CI) |

## Behaviour

`plan_route(index, query, limits, costs)`: window = start-centred box of `window_radius_tiles` clipped to the world, origin floored to 8,
at most `max_window_blocks` 64-tile blocks per axis; the start cell gets the nibble flood; the goal point (Q24.8) flags the 3 x 3
cells around its cell (disc table of `(radius + 4) / 8`) and the centre cell gets nibble 0xf; the coarse search labels at discovery
with the exit/entry tests of RF-coarse section 2 and the class costs (straight = orth, turn = diag - orth); at each popped goal cell
the candidates of the 5 order tables are scored `64 R^2 - trunc(64 d^2)` in integers on the Q24.8 goal, offered with the
callback/Offer rules (strict `score > best`, free beats congested, congested never raises `best`); the coarse path plus the missing
corner of every 2 x 2 turn is the corridor; the fine Dijkstra (NW, NE, N, SW, SE, S, W, E; 10/14; no corner cutting; FIFO) floods it;
if the goal tile is unlabelled one connectivity bit of a path cell is cleared (`learn_mask`) and the coarse search repeats, up to
`retry_cap` times; the trace is the run-length list of the labels, runs capped at `run_cap`.

## Verification (what each claim rests on)

- The 24 route-run vectors, 21 offer cases, passage statuses/goal tiles/scores and the 7 shore cases of `fixtures/synthetic/sim/nav`
  (emulated original) reproduce exactly from the C++ output (`route_fixture_test.cpp`).
- New emulated-original fixtures generated for this package from the real packer, nibble and the whole engine under unicorn:
  400 packer vectors, 150 nibble vectors, 159 route vectors (mixed cells, up to the retry cap, sector/block edges, shore band). An
  extended campaign of 550 further random cases (seeds 31-33, `ANNO_ROUTE_EXTRA`) was run against the same oracle once with 0
  mismatches in status, runs and learned count; it is not committed.
- Real terrain: 30 seat-to-seat routes and 15 witness routes on the three candidates, walked with the independent legality check.
- Perturbation tests: a wrong fine order, fine tie rule, fine diagonal cost or coarse tie rule still walks a legal route to the same
  endpoint on hundreds of vectors but fails the exact comparison; corner cutting fails the legality walk.
- Limits: each limit at its exact boundary with a valid control; 1/2/4/7 threads and a shared index give byte-identical results.

## Deviations and named dependencies (nothing guessed)

1. **Q24.8 goal.** The contract takes the goal in Q24.8; the emulation used f32. They agree bit for bit when the goal is exact in
   Q24.8 (all fixtures but one); for 26.6 the score differs by one or two units (`passage land_goal_5_9_inland`, tolerance 2).
2. **Costs are u32** in the port (the original sums in u16 and wraps above 65,535, reachable only on paths of thousands of open cells).
3. **Outside the world is land** (tiles) and mask 0 (cells). The original reads open water for the part of the last 64-tile
   sector beyond the world edge (null sector = 0xfff; kind-10 fill = 8). Candidate worlds are 1431 tiles wide; the window is
   clipped to the world, so only cells of the last partial row/column are involved. Original parity for worlds that are not a
   multiple of 64: UNKNOWN (open question).
4. **Early stops** (output-identical): the coarse flood stops after the last goal cell is popped, the fine flood when the goal
   tile is labelled. The pop counters therefore differ from the original's full flood.
5. **Window too large** is `BudgetExceeded{Window}` (the original answers -2); `OutsideWindow` means no goal-disc cell is inside
   the window. Added statuses: `BudgetExceeded` (-3), `OutsideWindow` (-4), `InvalidQuery` (-5).
6. **Cost classes.** The default table has classes 0..3 with the class-0 values; `shore_enterable` is false for class 3 (its mask
   byte for type 3 is 0 in the exe); class 4 (animals) is not provided and a query with it is `InvalidQuery`. The per-direction
   cost entries of the original table are uniform for ship class 0 and are not modelled.
7. **Learned count.** `RouteWork::learned` is the number of masks applied; when the cap refuses a retry `retry_cap_hit` is set
   (the original resets its own count to 0 on that path; the fixtures' `learned` column shows 0 there).
8. **Shore band (type 3) is derived** from the island block records (`TileSource::block_range`, `WorldTileSource` for aligned island
   blocks) exactly as pass 2 of `0x100aae00`: the cell's sector holds an island block and the cell or one of its 8 neighbours inside
   the world has a record with `32 < 2 max` and `32 > 2 min` (halved heights). 60 worlds from the real builder under emulation match
   cell for cell (`builder_vectors.txt`); explicit `shore_cells` can be added on top (tests, mods). `rebuild_rect` recomputes masks
   for the cells of the rectangle and cost types for them plus a one-cell halo (a record changes its 8 neighbours); it assumes island
   COVERAGE did not change (use a full rebuild for that). Islands that are not aligned blocks have no records and no band.
9. **Object terrain.** `base` (rec[4], the placed item's height byte) defaults to 0 because the sim does not carry it; the 11
   height-map types use their scalar; the exact floor of the f32 value is used (x87 precision CAP-2 unknown).
   `WorldTileSource` uses the block shortcut only for 8-aligned islands whose size is a multiple of 8, else the tiles.
10. **Stamps.** `plan_route` takes stamp tiles (first `unit_cap` used). `local_ship_stamps` (`route_stamps.hpp`) builds them from a
    minimal `ShipSpatialView` (ships per 16 x 16-tile cell): cells overlapped by the radius only, z-cell then x-cell then ascending
    id (ADAPTATION of the registry's insertion order), `d^2 <= r^2` inclusive, `state & 0xa == 0`, ship group, stop at `unit_cap` hits
    (the planning ship and direction-less ships count against the cap, as in the original), stamp = `pos + velocity * 25` truncated (velocity = unit +0x20 = direction x speed x the segment factor, tiles per stepper unit; NOT a normalised heading, MAP-NAV-3 N2)
    exactly in 2^-24 units. The f32 sum of the original can differ from the exact one by a tile only when the point lies within
    about 2^-14 tile of a tile border at positions above 1000 (not modelled).
11. **Test hook in production code.** `route_detail::Perturbation` / `plan_route_perturbed` are compiled into the library (no
    `#define` is allowed in the sim); production code calls `plan_route`, which passes the default.
12. **Goal cell for goals outside the window** is `trunc((x - window origin) / 8)` (the original's `ftol((x - x0) * 0.125)`, DERIVED
    for negative offsets); only fixtures `goal_off_map_west` touches it and agrees.

## What the order tests pin (and one order that is unobservable under the default costs)

`route_unit_test.cpp` ("every wrong order, tie, cost or score rule ...") runs eleven deliberate mutants over 514 emulated vectors
(route_runs, route_emulated, route_ties, route_ties_cost, route_scores); each walks a LEGAL route and each differs from the
original's exact runs / end tile / best score in at least one vector: reversed fine order, LIFO fine ties, fine diagonal cost 20,
LIFO coarse ties, fine NW<->NE, SW<->SE and W<->E swapped, coarse perpendicular turns swapped, coarse start order W,E,N,S, the
straight step tried first, and a rounding score (the truncation `ftol(64 d^2)` is pinned by 120 arbitrary-goal vectors whose `64 d^2`
is not a multiple of 1024, with the engine's own `best`).
**Unobservable under the default costs:** the coarse order "straight step first versus last". A straight step costs `orth` (10)
and a turn `diag - orth` (3), different values, so the heap orders the three children by cost; the discovery order inside one
expansion only ranks children of equal cost, and two nodes of different expansions never interleave their sequence numbers (each
expansion takes a contiguous block). The order is therefore pinned with a patched cost table (`route_ties_cost.txt`: class 0
coarse costs 3 / 6 in the original's table under emulation, a turn costs a straight step) where it does change results.

## Pointers and lifetime (not simulation state)

`CoastIndex` holds a raw pointer to its `TileSource`; `WorldTileSource` holds raw pointers to the islands and the type registry (and
`RouteTerrainOptions::item_base` is an optional callback; the default is the tile's own `TileLayer::item_height`). Both are derived,
never saved, hashed or part of a checksum. A `World` copy, move or load starts with an empty `RouteCache`. In a running world
`RouteCache::sync` keeps the index current: terrain is immutable (a fingerprint rebuilds all if not), and tile-layer dirty
rectangles of object, anchor and height-byte changes are applied with `rebuild_rect` (docs/sim/ship-navigation.md 12.13). Nothing
may change the world while a plan runs on the index.

## Open questions for the orchestrator

- (Resolved by MAP-NAV-3 N3, 12.13) The item height byte (rec[4]) is carried as `TileLayer::item_height` (low byte of `aux`, no stream
  change). Still open: the byte written by island growth (not evidenced).
- Partial last sectors (item 3, `docs/open-questions.md` OQ-NAVC-1).
