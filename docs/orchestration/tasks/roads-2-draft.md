# ROADS-2 SIM-ROADSHAPE: road pieces turn and join (DRAFT for ORCH III's review)

**Owner's report (via ORCH III, 2026-11-27):** "i can lay a 'corner' in terms of road tiles but they are all in the same
rotation, i.e. the roads pointing to the left should also be rotated to the left. rotation aligns with construction
direction automatically, which is the goal". **Cause:** `make_road_drag_cmd` (`src/sim/include/anno/sim/road_drag.hpp`)
gives every record one `rotation` and the armed straight id; the original's reshaper (island slot `+0x80` = `AnnoWelt
0x1007cc40`) is not built.

**Evidence already in hand:** `docs/BUILDINGSOBJECTS.md` part B §10.1 items 7-8: the four-neighbour mask (N=1, E=2, S=4,
W=8) maps to a piece and a rotation (straight 10/5, corner 6/3/9/12, T 14/7/11/13, end 2/1/8/4, cross 15), dirt id =
cobblestone id + 20, neighbours whose shape changes are appended to the record list (`0x1007ece0`), their heights
recomputed (`0x1007e154`). Corpus: 28,233 dirt + 13,967 cobblestone tiles of 50 shipped `.sze` (the BUILDINGS-REF-2
script). Out of scope (ROADS-5): ramps 2510-2515/2530-2535, the slope junction 2509/2529, bridges and their 20-tile scan
(OQ-764, OQ-765).

**Recommendation: split, about 3-4 h each.**

## ROADS-2a: the reshaper as pure sim functions (sim, no wiring)

- `road_shape(mask) -> {piece, rotation}` and `road_shape_id(family_base, piece, variant)` (data: the piece table from
  §10.1 item 8 as `RoadShapeTable` with the original ids as defaults; dirt = cobblestone + 20 as data, `0x1007efcf`).
- `reshape_records(path records, neighbour lookup) -> records`: for each path tile its mask from the path plus the
  existing road tiles, its piece and rotation; then every existing orthogonal road neighbour whose mask changes,
  appended with its new piece (the original's order: path first, `0x1007e8b0` lists then world). Pure: reads a
  `std::function`/span neighbour view, writes nothing. Straight pieces keep their cosmetic variant (the app's
  `RoadVariantDeck` choice) when the shape stays straight.
- **Oracle and corpus:** a Python reference in `tools/` (the §10.1 table) and a corpus test over the 50 `.sze`: for every
  road tile in 2500-2508/2520-2528, the predicted (piece, rotation) from its neighbours equals the shipped tile in at
  least 97 % (the dominant cells hold 90-100 %; the rest are hand-edited or AI-placed, listed by count, not hidden).
  C++ = oracle on every tile.
- **Unit cases:** an L (straight legs turned, one corner), a T, a cross, an end, a lone tile, laying next to an existing
  straight (the neighbour becomes a T or corner), and the dirt/cobblestone families. Failure proof.
- No state, no pins.

## ROADS-2b: wired (sim apply + check_build_path + ghost)

- Under `road_rules.enabled` only (header tag 2), **without a new field**: a new flag would change tag 2's bytes for
  every `road_rules` save. Default off = today's behaviour, byte for byte.
- The kind-9 road commit runs `reshape_records` against the island before the per-tile checks, so cost, territory, the
  no-downgrade rule and "free same family" see the shaped records and the rewritten neighbours (neighbours of the same
  family are free, §10.1 item 5). `World::check_build_path` runs the same function, so the ghost (`plan_road_ghost`)
  shows the shaped pieces: preview == commit (the ROADS-3 parity check extended to shapes).
- Shape pieces are slope class (`0x1186`): they need level 32 and a slope of at most `0x10` (§10.1 item 6). A path whose
  corner falls on water: the original's handling is OQ-765; the packet refuses it at that tile (the prefix commit stops
  there) and labels it ADAPTATION until read.
- **Proof:** the owner's L, T and cross through the real window (`viewer_wiring_ui_roads_shape`, from the default start
  view, dev facts with the placed ids and rotations), the pixel check of the ghost against the committed tiles, the sim
  lanes. If any pin, golden or replay moves, stop and ask (ORCH III's rule).
- **Owner test:** drag an L, a T, a cross; lay a road beside an existing one; check that the tiles turn and join.

## Open questions

From the UI lane's range: OQ-900 the order in which the original appends neighbour rewrites when two path tiles share a
neighbour (it affects only the record order, not the result, L); OQ-901 whether a re-shaped straight keeps its
cosmetic variant (2520-2524) or draws a new one. Both close by reading `0x1007ece0` and `0x1007ef60` in 2a.
