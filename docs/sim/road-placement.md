# Ordinary land roads through `BuildCmd` and `DemolishCmd`: contract

**Status:** specification for MAP-ROAD-1 R1 (written before the engine change), 2026-11-02. Evidence: [`docs/re/road-placement.md`](../re/road-placement.md). Builds on [`ship-placement.md`](ship-placement.md) (the terrain/height/refusal contract of the ordinary building, its sections 1-3 are not repeated) and [`state.md`](state.md) 5.9 / 8.4.
Oracle: `tools/placement_ref.py` (`road_class`, `engine_place`; vectors in `fixtures/synthetic/sim/placement_vectors.txt`, checked against the original code by `tools/placement_emulate.py vectors`).
Labels: **ORIGINAL** (address in the RE note), **ADAPT** an approved deviation with its reason, **UNKNOWN**.

## 1. Outcome and boundary

An owned city builds and removes the ordinary land roads of the shipped registry through the existing `BuildCmd` / `DemolishCmd`. A road is a **tile object**: it is written into the island's tile layer (object id, offset/rotation bytes, height byte, walk code and mask) and into nothing else.
It allocates no building record and no building serial, joins no building list, is in no derived building index and returns no `BuildingId`. The serial-exhaustion check (`SerialExhausted`) is **skipped for a road**, which takes no serial. Route searches and the market-cart delivery read the tile layer, so they see the change on their next search.
Not in this unit: a `RoadsCmd`, fake building ids, bridges, ramps, piers, walls, plazas, upgrades, trade routes, a connectivity cache, a new pathfinder, UI/render integration, neighbour-dependent shape choice (U-R1).

## 2. Eligibility (`road_class`, parsed facts only)

A registry type is a **land road** iff all of: `prodtype == 193` (`STRASSE`); `flags_word <= 0xffff`; `flags_word & 0x7000 ∈ {0x4000, 0x1000}`; `flags_word & 0x10 == 0`; `flags_word & 3 != 1`; footprint `w, h > 0`.
For the shipped `Anlagen.dat` that is 21 types (13 x `0x4186`, 8 x `0x1186`). No id, name, player, test type or seat appears in the predicate; a data-defined type with those facts is a road without an engine edit. The constants (193, the two class values) are fields of `RoadParams`, a default argument of the rule like `PlacementParams`, not hashed state.
Every other class that has no building list stays `Unsupported` exactly as today (`BRUECKE`, `RAMPE*`, `PIER`, walls, scenery, a `STRASSE` with `F & 3 == 1`): never a default answer. A type with a building list is unchanged.

## 3. Placement rule of one road record (`placement_rule`, extended)

Evaluated for the effective type at execution time, in this order; the first failing step decides.

1. **Class gate:** not a land road and no building list -> `Unsupported` (existing). A land road continues below; `rotation > 3` -> `BadRotation` (existing), footprint not on the island -> `OutOfIsland` (existing `footprint_owner_rule`), tile ownership as for a building -> `TileNotOwned` (existing).
2. **Occupancy.** A tile that carries a building (live, or taken by an earlier building record of an atomic dry run) -> `Occupied`. For every non-zero object id under the footprint: the id must be in the registry (else `Unsupported`) and `site::overlap_with_existing` (ORIGINAL, group `0xc1`: compatible with an existing group `0xc1` piece except a `0xc6` bridge, and with flag-`0x20` scenery) must be compatible, else `Occupied`.
   **ADAPT R-A3:** an existing group `0xc1` piece that is not itself a land road (`RAMPE*`, `PIER`) -> `Occupied` (the original accepts it; replacing an approach or a pier with a land road is not a defined outcome and the UI side is unread, U-R4). A land road over a land road, over scenery, or over nothing is accepted.
3. **Terrain.** The vertex window of the footprint is read through `VertexGrid` (no terrain, placeholder, absent/wrong column or an unreadable vertex -> `Unsupported`, never a height). Per cell `cellmin = min(4 vertices) & 0xf0`, `mx` the raw window maximum, `m = flags_word & 3`:
   `m = 0` result = max cellmin, `m = 2` result = min cellmin, `m = 3` result = `mx & 0xf0` (ORIGINAL `0x10008f90`, A1: the server derives the byte; the wire byte 8 is never read).
   Class `0x1000`: `mx - min(cellmin) > 0x10` -> `TerrainRefused` (ORIGINAL analysis). Class `0x4000`: no slope test (ORIGINAL).
   **ADAPT R-A2 (level floor):** result `< 32` -> `TerrainRefused`. The original accepts a `0x4000` road at once and needs `>= 32` only for class `0x1000`; the road cursor's own test is unread (U-R2) and a server must not accept a road on open sea or at the island's east/south edge column. Evidence: 32,803 of 32,808 shipped class-`0x4000` roads and every class-`0x1000` road have a byte `>= 32` (re section 4). The floor is `PlacementParams::min_level`, the same 32 as for a building.
4. The accepted record's height byte is the result; it is written to the tile record's height byte and is not recomputed later.

Costs, ownership and free-build follow the building path unchanged: affordability reads gold and goods against the state earlier records left, a refusal changes nothing, a charge follows a successful tile write (gold `cost_gold`, goods `cost_goods` from the stock, `build_goods[...].spent`).
The shipped roads cost gold only (5 dirt, 10 cobblestone). **Replacing a road with a road charges the new road's cost in full and refunds nothing for the old one** (ORIGINAL charges every record; no refund is evidenced).

## 4. Result semantics

`RecordResult` for a placed road: `status = None`, `building` is the invalid id (default `BuildingId{}`), and the new `tile_object = true`; `transferred_88` is 0. A caller learns the road from the command it sent. The command is rejected as a whole only when no record was placed (existing policy); a partial drag is accepted and its records tell which.
`on_placed` (the building placement seam) is **not** called for a road (no building to hand over); `upgrade_rewrite` runs as for any record and a road is not rewritten by the default seam.

## 5. Batches: sequential and atomic

* **Sequential (default):** record by record in stream order, each checked against the state the earlier ones left (a road written by record *i* is on the tile layer when record *i+1* is checked).
* **`Rules::build_atomic`:** a dry run validates every record against a scratch budget first and one failure places nothing (`AtomicAbort` for the others). The budget accounts **by record kind, in command order**: a building takes gold, goods, one serial and its footprint tiles (`Occupied` for a later record touching them);
  a road takes gold and goods and **no serial**, and puts a *virtual object* (its type id) on its tile. A later record's occupancy test sees the virtual object through the same `overlap_with_existing` the live path applies to the written tile (so building-over-road is `Occupied`, road-over-road is compatible, road-over-ramp is `Occupied`), and the live and dry runs refuse the same record with the same status.
* The scratch budget never writes the tile layer. The virtual-object test runs inside `check_record` before the placement rule (the rule and an installed `placement_rule` seam take no budget); a seam that replaces the default rule (the D17 stand-in) answers `Occupied` for road-over-road and is a test seam only, as for buildings. Required property: for every command the dry run and the live run decide identically, and a refused command leaves bytes, hash, PRNG, money, goods, buildings, tiles and events unchanged.

## 6. The tile write and its lifetime

The writer is the existing `place_object_record(layer, types, type, x, y, rotation, phase = 0, height)` (no neighbour store was found in the original's placement function; the rest of `City::Place` is not fully enumerated, re section 3); for a `STRASSE` it writes the id, the in-footprint offset, the rotation, the height byte, the **walk code derived from the HEAD pair by `walk_code` and mask 0** (ORIGINAL: code 0 for the (0, 3) types, 1 for 2528, 4 for 2530-2533; all admitted by layer 3), through the `TileLayer` setters. Those setters mark the changed tile for the route index (`TileLayer::route_dirty`), so a ship route index stays equal to a clean rebuild, with no new bookkeeping.
**There is no road connectivity cache and no per-tile global recompute.** A walk search builds its `WalkCells` from the live tile layer on every call; the first search after a change reads the new codes. A road tile is walkable for the market cart (layer 3 admits codes 0, 1, 4) and for a carrier (layer 0); a non-road tile is walkable for layer 0 only (code 2), so the layers keep their respective original rules.

## 7. Removal (`DemolishCmd`)

A tile is removed by the existing non-building branch of `demolish`: the tile must be the city's (`TileNotOwned` otherwise), the tile carries no building, and its object id is non-zero; the clear arm of `0x10082c30` runs as a **stand-in** (`clear_object_record`; ADAPT: the original's arm selection is unread, U-R5): id 0, offset/rotation 0, code 2, mask 0, height byte kept. Nothing else changes: no neighbouring tile, no building record, no serial, no terrain, no money (**no refund**: none is evidenced and none is invented; U-R3).
A second removal of the same tile finds no object: `NoBuildingAtTile` and no change. A building on the tile is removed by the building branch (unchanged); a road and a building never share a tile. The branch clears any object with an id (OQ-201: scenery too); the road claim covers the land-road ids and the unrelated branches keep their behaviour.

## 8. Traffic across a change

* A **new search** (`choose_source`, `path_home`) sees construction, a break (removal) and reconstruction, because it reads the tile layer when it starts.
* A cart that **is already walking** keeps the route it was planned with (`Figure::transport.route`, a list of tile centres; `transport_tick` follows it and does not re-read the tiles). Removing a road under it neither crashes nor moves goods: the cart completes the planned walk (its goods and the home/goal state follow the existing arrival rules) and its **next** search starts from the changed tiles.
  This is an **adaptation by absence**: the original's reaction of a cart to a road that disappears (replan, stop, retry) is **UNKNOWN** and the deferred fine-route/retry subsystem is not introduced here.
* Market carts (layer 3, road-dependent) and workshop carriers (layer 0, open land) keep their own rules; this unit changes neither table.

## 9. Hand-derived vectors (`placement_vectors.txt`, R-01..R-24)

Grid: 10 x 10 island at the origin, flat `0x30` unless stated, vertex column 10 and row 10 are 0. `type 16` = `STRASSE 0x4186`, `17` = `0x1186`, `18` = bridge (198), `19` = ramp (194), `20` = `F & 3 == 1`, `21` = mode 3, `22`/`23` = 2x1 mode 2/0. `E` = emulated original answer, `P` = the reference.

| Pair / edge | Cases | Expected (hand) | Why |
|---|---|---|---|
| Control and floor | flat `0x30` / `0x20` / `0x10` / sea `0` for type 16 | accept 48 / accept 32 / `TerrainRefused` / `TerrainRefused` | R-A2; E accepts the last two (`original accept 16`, `accept 0`) |
| East edge | type 16 at x=9 (vertex column 10 reads 0) | `TerrainRefused`; E accepts byte 0 | R-A2 |
| Slope, class `0x4000` | window `30 30 / 30 40`; `40 40 / 30 30` | accept 48 both | no slope test, min cell minimum |
| Slope, class `0x1000` | step `0x10` / step `0x11` / flat `0x10` | accept 48 / `TerrainRefused` (E same) / `TerrainRefused` (E same) | `mx - lo > 0x10`; floor |
| Mode data | 1x1 mode 3 with one `0x40` vertex; 2x1 mode 2 / mode 0 on cell minima `0x30`, `0x40` | accept 64; accept 48; accept 64 | result by `F & 3` |
| Objects | road over road / scenery / house / bridge / ramp | accept / accept / `Occupied` / `Occupied` (E same) / `Occupied` (R-A3; E accepts) | compat row `0xc1`, R-A3 |
| Classes | bridge type, ramp-PRODTYPE class `0x4000`, `F & 3 == 1` | `Unsupported` x3 | section 2 |
| Bounds / rotation | x = 10; rotation 3 | `OutOfIsland`; accept 48 | R1; 1x1 window |

Through `World::apply` (R2-R4, `road_build_test.cpp`): wrong owner, neutral tile, affordability pair (control builds), duplicate and replacement (charge twice), height byte on the tile, no serial consumed and `next_serial` unchanged, sequential and atomic mixed building/road batches (dry run equals live run), removal and rebuild, save/reload, a second data-defined road type and another owner, and the transport and real-file canaries of R4.

## 10. State identity

No state layout change: the tile object id, offset bytes, height byte, walk code and mask are existing hashed/saved columns of the tile layer; `RoadParams` and `tile_object` are not state. `kStateHashVersion` stays 124. Behaviour changes: a `BuildCmd` naming a land road, `Unsupported` until now, is accepted; a recording that contains one was refused and now diverges (none exists: it was never executable).
`tile_object` is the **future Windows API delta** (an additive field of the output struct `RecordResult`); the wire format and `Reject` do not change.

## 11. Unknowns carried (none is silently substituted)

U-R1 UI id and neighbour-variant choice; U-R2 the road cursor's terrain test (stood in for by R-A2); U-R3 the refund table and the second jump table of `RemoveAt`; U-R4 road over ramp/pier (R-A3); U-R5 the removal arm (clear vs replacement, section 7); the original's reaction of a travelling cart to a vanished road (section 8). Original-runtime capture of a real road build/remove click: NOT_RUN.

## 12. As implemented (MAP-ROAD-1 R2-R4)

* **Code.** `is_land_road` / `RoadParams` and the road class of `placement_rule` (`placement.hpp/.cpp`: window result by mode, slope class, level floor R-A2, ramp/pier refusal R-A3 in the occupancy step); `check_record` / `spend` / `place` in `apply.cpp` (`Checked::tile_object`, `Budget::objects`, the serial check skipped for a road, `place_object_record` instead of `create_building`, `on_placed` not run); `RecordResult::tile_object`. Removal is the unchanged non-building branch of `demolish`.
* **Constants.** 193, `0x4000`, `0x1000`, `0x10` are `RoadParams` defaults (not hashed); the floor is `PlacementParams::min_level` (32).
* **Identity.** No stream-layout change; `kStateHashVersion` stays 124; the golden stream agrees (`tools/sim_stream.py --check`). A recording that names a land road now executes (it was refused as `Unsupported` before).
* **Tests.** `placement_vectors.txt` 75 cases through `placement_rule_test.cpp` (expected values read from the file) and `placement_ref.py` / `placement_emulate.py`; `build_terrain_test.cpp` "road build ..." / "road remove ..." (public `World::apply`, 300-round dry/live property with existing ramp/bridge/road pieces); `transport_test.cpp` "roads by command ..." (unreachable, reachable with goods moved, cut, rebuilt; a walking cart keeps its route and has no way home); `founding_assets_test.cpp` (the real-registry, real-terrain opening: per seat a two-tile drag, a rival attempt, a removal, a repeated removal, a rebuild, with gold, serial, building count, height byte and walk code checked, and a changed legal road command diverging).
* **Negative controls (run, then reverted).** No level floor -> `the terrain decides ...`, `a mixed drag ...`, the vector case fail; a serial taken for a road -> 4 cases fail; the walk record left at code 2 -> 9 cases fail; no payment -> 5 cases fail; each with its legal control passing first.
* **Traffic limit.** A walking cart keeps its route; with the net cut it loads, then has no path home and the job ends with the cargo on the figure (existing step C rule); the original's behaviour is unread (section 8).
