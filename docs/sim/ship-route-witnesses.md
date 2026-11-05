# Ship route witnesses re-evaluated under the original route engine (MAP-NAV-2 package C, part d)

Status: report only. No manifest byte, label, selection or placement was changed (`git diff -- data/` of this package is empty).
The 15 saved geometric witness routes of the three `ship-start-v2` candidates (`lan5-ship-a/b/c`) are tool evidence
(`tools/map2_ship.py`: 4-connected exterior-sea walks with Manhattan cost, land = `HEIGHT >= 16` per tile, no hull). They were
never stored in the manifests; the tool computes them (`witness`), and `fixtures/synthetic/sim/nav/candidate_witnesses.txt`
records the 15 results of `python3 -I tools/map2_ship.py check <manifest> --game-dir <dir> --json` (spawn tile, site, water
tile, cost per seat). `docs/sim/ship-navigation.md` section 11 item 4 and `docs/re/ship-navigation.md` section 11 required this
re-evaluation before any selection that relies on a one-tile channel or on shore tiles.

## Method

For each witness the C++ route engine (`plan_route`, class 0, radius 6, no other ships, default limits) runs on the REAL islands
of the candidate (`load_ship_start` of the unchanged manifest, `WorldTileSource` = the original land rules, a `CoastIndex` over the
whole 1431 x 1431 world) from the spawn tile to the centre of the witness water tile, and the returned runs are walked tile by
tile with an independent legality check (`route_world_test.cpp`: every entered tile water, a diagonal step needs both side tiles,
bounds). The land rule is the original's: a tile is water iff `floor(sum of its four corner vertices / 4) <= 19`, with the zero
padding beyond the island, plus the object predicate `((h + base) & 0xff) > 19` with `base = 0` (the sim does not carry the placed
item's height byte, see *Caveats*). The shore band (cost type 3, 50/69 against 10/13) is DERIVED from the island block records by
`CoastIndex` exactly as pass 2 of the original builder does (validated against the builder under emulation, 60 worlds, cell for
cell: `fixtures/synthetic/sim/nav/builder_vectors.txt`); this report was re-run with the derived band (revision 2 of the report: the
first revision ran without any band).

## Result

All 15 witnesses are found (status `Planned`), every route walks legally, none needed a learned coarse mask, and every route ends
within the goal radius, with the derived shore band in the coarse map (315 / 337 / 404 allocated sectors on candidates a / b / c). Fourteen end exactly on the witness water tile.

| cand | seat | island | tool cost (Manhattan) | status | engine steps (8-connected) | runs | end tile | witness water tile | d^2 | witness tile under the original rule |
|---|---|---|---|---|---|---|---|---|---|---|
| a | 0 | 110 | 267 | Planned | 205 | 22 | (107,618) | (107,618) | 0 | water |
| a | 1 | 101 | 103 | Planned | 74 | 9 | (623,754) | (623,754) | 0 | water |
| a | 2 | 8 | 125 | Planned | 111 | 12 | (1118,791) | (1118,791) | 0 | water |
| a | 3 | 106 | 153 | Planned | 88 | 10 | (1036,152) | (1036,151) | 1 | object |
| a | 4 | 9 | 49 | Planned | 46 | 7 | (183,1146) | (183,1146) | 0 | water |
| b | 0 | 0 | 86 | Planned | 86 | 10 | (284,242) | (284,242) | 0 | water |
| b | 1 | 111 | 121 | Planned | 108 | 15 | (786,137) | (786,137) | 0 | water |
| b | 2 | 125 | 143 | Planned | 143 | 16 | (227,718) | (227,718) | 0 | water |
| b | 3 | 103 | 99 | Planned | 86 | 10 | (814,613) | (814,613) | 0 | water |
| b | 4 | 127 | 44 | Planned | 44 | 5 | (1200,456) | (1200,456) | 0 | water |
| c | 0 | 0 | 61 | Planned | 45 | 8 | (262,137) | (262,137) | 0 | water |
| c | 1 | 123 | 52 | Planned | 45 | 7 | (861,595) | (861,595) | 0 | water |
| c | 2 | 114 | 130 | Planned | 108 | 13 | (1006,136) | (1006,136) | 0 | water |
| c | 3 | 3 | 72 | Planned | 72 | 8 | (1120,528) | (1120,528) | 0 | water |
| c | 4 | 5 | 101 | Planned | 87 | 11 | (273,934) | (273,934) | 0 | water |

Effect of the derived shore band (compared with the first revision of this report, which ran with every coarse cell open): the
step counts, the end tiles and the statuses are identical for all 15 witnesses; the run lists changed for three (candidate a seat 1:
8 to 9 runs, a seat 4: 6 to 7, c seat 1: 5 to 7), i.e. the coarse search now bends around band cells that the Manhattan tool ignores,
and the coarse work grew (largest witness query 22,324 coarse pops, was 22,322). No witness is lost to the band.

Differences against the tool:

1. **Length.** The engine's routes are 8-connected: 44 to 205 steps against the tool's 4-connected Manhattan costs 44 to 267
   (equal in four cases: b seat 0, b seat 2, b seat 4, c seat 3). The tool cost is an upper bound of the engine's
   step count in all 15 cases, as expected; it is not a navigation length.
2. **One endpoint moves.** Candidate a, seat 3 (island 106): the witness water tile (1036,151) is **blocked under the original
   rule by an island tile object** (its terrain average alone says water; the object height scalar exceeds the threshold with
   `base = 0`). The route ends on the nearest reachable tile (1036,152), d^2 = 1 (score 2240), inside the radius 6 acceptance. The
   founding site is not affected by this report; whether a ship at (1036,152) is within founding range is a question for the
   founding checks, not for the route engine. This witness needs a re-look by whoever selects among the candidates (the tool
   calls the tile water; the original does not).
3. **No witness depends on a failed channel.** All 15 reach their goal tile or its neighbour, so no witness relies on a passage
   that the original rules close (diagonal-only gaps, enclosed lakes). Whether a witness relies on a one-tile channel or shore
   tiles (which the original allows at higher coarse cost) is not decided by this report: the engine has no hull clearance, the
   tool route is only Manhattan-shortest, and the 8-connected routes above need not pass where the tool's passes.
4. **Index check.** On the three candidates the coarse words built from the builder's block shortcut and the words built by
   scanning every tile are identical (0 cells differ in 12,832 / 14,420 / 16,100 shortcut cells; 364 / 384 / 471 cells are
   classified `Mixed` by one and all-water by the other, with the same packed mask), so the object-across-a-block-edge quirk
   (`route_world_test.cpp`, synthetic case) does not change the coarse map on these inputs.

## Caveats (named, not hidden)

- `base = 0` (an input hook, `RouteTerrainOptions::item_base`; whoever adds the item height byte to the tile record in a later stream version supplies it): the placed item's height byte (`rec[4]`, `state >> 8`) is not carried by the sim (`TileLayer::aux` is a stand-in
  that every loader leaves at 0). The original adds it to the object height; coast buildings carry 48 in the corpus statistics
  (`RA3` section 2.5), so with the real byte more object tiles block than here. The result for witness a/3 can only get stricter.
- The 11 object types with a height map (1020, 1021, 2701-2708, 2740) use their scalar in the adapter; the original uses the map.
- The x87 precision of the original is unknown (CAP-2): 29 types differ by one height unit between 24- and 53-bit precision;
  the port takes the exact value.
- The shore band is derived from the island block records of ALIGNED island blocks (origin and size multiples of 8); the candidates'
  islands are all aligned. Other islands would have no records and no band.
- Zone near the world edge: the world is 1431 tiles wide (not a multiple of 8 or 64); the port treats tiles beyond the edge as land
  (OQ-NAVC-1). The witnesses end at x <= 1200 and z <= 1146 and the seat spawns start at x <= 1108, z <= 680: all more than 200 tiles
  from the east and south edge, so none depends on that choice.
- A chosen candidate would still need the planned `MoveCmd`/stepper packages; this report covers the route query only.

## Reproduction

`ANNO1503_GAME_DIR=<game> ANNO_ROUTE_REAL_OUT=<tsv> anno_sim_tests -tc='route on the real islands*'` writes one TSV row per witness and
per seat-to-seat query. The 15 inputs are in `fixtures/synthetic/sim/nav/candidate_witnesses.txt`.
