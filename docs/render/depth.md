# Per-pixel depth (W4-DEPTH-1)

**Status:** design, 2026-11-04. Evidence in [`docs/re/annodisplay.md`](../re/annodisplay.md) §9 (code and live dumps of
the original's screen-tile caches in the Wine runner). Linked from [`iso.md`](iso.md).

The original composes its picture with a 16-bit depth buffer. This renderer drew sprites whole in painter's order and
approximated the depth test only for the sea (a shore height) and for rivers (a tolerance). The owner's playtest of
2026-11-03/04 showed the difference: the original's Kontor stands in the ground, its piles are cut by the sloping bank
and covered by the water, ours stands on top of everything.

## 1. The original's depth model (evidence)

| Part | Rule | Evidence |
|---|---|---|
| Units | u16 depth = ⌊65536 · d⌋ mod 65536, d = 0.5 − 0.00023663246 · (x + z) − 0.00018957623 · up, world units (tile x, `HEIGHT` · 0.04, tile y), view rotation 0; smaller = nearer | D: `0x100182f8`–`0x100183e3` (2³², `__ftol`, bits 16–31); M: the live camera matrix, the same at every zoom group |
| Scale | 15.508 per tile of x or y; 0.4970 per `HEIGHT` unit of the same point; 2.0455 per `HEIGHT` unit at a fixed screen pixel | arithmetic on the above |
| Test | a pixel is drawn when its depth ≤ the buffer (16-bit unsigned compare); pixel depth = sprite base + signed `z` (16-bit wrap) | D: `0x1001848e`–`0x1001849a` |
| Sprite base | ⌊65536 · d(anchor point)⌋; anchor point = the object's world point + its sprite set's prefix offset (dx, up, dz) at `HAEUSER/ENTRY` +4…+12 | D: `0x10011e60`; M: the start Kontor's depth cache = base + z with the anchor at its footprint centre and height 48 · 0.04; the sea's items at the lattice point + (0, −1.15, 0) |
| `Z` formats (`RGBZ`, `RGBAZ`, `RGBZM`, `RGBWZ`) | base + z per pixel | D: §3 |
| `V` formats (`RGB`, `RGBV`, `RGBAV`) | the base at the frame's anchor row, + 0.24135 per row below it (the vertical-surface slope 0.00018958 / 51.477 · 65536 at zoom group 0; × 2, × 4 at groups 1, 2) | D: `0x10014ca0` starts at depth − anchor row · slope, the slope (`0x1004d2d8`, 0.0 in the file) written at `0x1000c302` by `0x1000c150` from the projections of (0, 10, 0) and (0, 0, 0), the brush step +4 from the same variable (`0x10015f10`); M: brush +4 = `0x3dc9` / 65536 at group 0 (independent review, 2026-11-04) |
| `H` formats (`RGBH`, `RGBAH`) | the same mechanism with the flat-ground slope: the flat plane through the anchor; tested, never written | D: `0x10015220` with the slope `0x1004d2e0`, written at `0x1000c2b4` from the projections of (10, 0, 11) and (10, 0, 10), one step along the ground (−0.752 per row at zoom group 0 from the live matrix) |
| Write | opaque pixels of `Z`/`V` formats write; `RGBAZ` level-1 (50 %) pixels test only; `RGBZM` writes both its opaque and its melted pixels; `H` formats and shadows never write | D: §3 |
| Static scenery | terrain triangles, then the placed objects (trees, buildings, ground pieces, roads), drawn into 128-px cached screen tiles with test and write | D: `0x1000fdf0`; M: the depth cache holds the terrain, the trees and the Kontor |
| Per frame | over the cached tiles, every frame: the coast pieces 2702–2704, 2708, 2709 (type class `0x163`) with test and write, then the sea, rivers, ships and figures sorted by base depth, far first, test only (ships write into the per-frame copy) | D: `0x100103c0`, `0x1000d680`, `AnnoWelt` `0x1009fb14`; M: no sea in the colour cache, the ship in the per-frame depth copy |
| Sea | anchor (tile + 1.5, 0.13 + 0.01 · (x mod 5) − 0.01357 · (y mod 3), tile + 1.5); frame `z` ≈ −47 at the anchor: a water plane at about 26.5 `HEIGHT`, drawn as a checkerboard over every cached pixel that lies below it | D, M: measured 26.25–28.25 over zoom groups 0–2 and view rotations 0–3 |

Consequences: the Kontor's piles disappear behind the sloping bank (the terrain wrote nearer depths) and under the water
(the sea plane is nearer than the piles' lower parts); a ship's hull under the plane shows the checkerboard; grass and
trees that are farther than a building's pixels are hidden by it whatever their draw order.

## 2. What this renderer does (design)

### 2.1 Units and one integer depth function
Depth is kept as `D256`, 256 × the original's u16 unit, in `i32` without the original's 16-bit wrap (an adaptation:
the original's wrap never happens on its maps, and large maps must not wrap either). It is computed from a point's
**scene screen row and height**, not from its world position: at view rotation 0 a point (X, Y, h) lies at scene
y = (X + Y) · th/2 − h · L (`tile_top`, `Lift`), so X + Y = (y + h · L) / (th/2) and

  D = 32768 − (A / th) · 2y − ((2A · L / th + B) / 64) · h64,  A = 15.50794 (65536 · 0.00023663246), B = 0.4969627 (65536 · 0.00018957623 · 0.04), h64 = 64 · h.

One function evaluates it in integers (8 fractional bits on the constants, `depth.hpp`), the same on the CPU and in
WGSL, so the CPU reference and the GPU agree bit for bit: `D256 = 8388608 − floor((CA · y2 + CB · h64) / 256)` with
`y2` = twice the scene row (+1 for a pixel centre), `CA = round(256 · 256 · A / th)` and `CB = round(256 · 4 · (2A · L
/ th + B))` per zoom group. The GPU evaluates it chunk-relative (the origin's part precomputed on the CPU), which is
the same value. Evaluating at the screen point keeps a sprite's depth consistent with where this renderer draws it
(its anchor is truncated to whole pixels); the scale differs from the original's by our 84 × 42 tile (0.4 % / 1.8 %).

- **Terrain:** D256 of the pixel centre at the terrain height seen there (`Chunk::terrain`, h · 64); no terrain =
  no depth (the original's `0xFFFF` clear).
- **Sprite base:** D256 of the anchor point (2 · anchor row, h64 = 64 · the item's height byte). The sprite-set prefix
  offset is not applied to land objects (their first sets have none, except a few listed in `annodisplay.md` §9.3).
- **Pixel depth:** `Z` formats base + 256 · z; `V` formats base + the vertical-surface step (B / L per pixel) times
  the rows from the frame's anchor row; `H` formats the flat plane through the anchor, evaluated at the pixel centre
  with the terrain's own function (at(2y + 1, h64 of the anchor)): a decal at the height of flat ground then has
  the ground's depth exactly and passes the `<=` test, as the original's coplanar decals do. The mechanism is the
  original's (the flat-ground slope); the value is this renderer's ground, −0.7385 per row at group 0 (−2 · ca: a
  42-px tile) against the original's −0.752 (its projection, 20.62 px per half tile), so that a decal ties with
  our terrain, and the sampling at the pixel centre is ours. A decal is hidden where the terrain under it rises
  above its item's height plane: the mechanism, but which height the original passes for a road is not traced (the
  item height byte is assumed, as for every object; OQ-026).

### 2.2 Static scenery (the chunk objects layer)
Every object draw of a chunk (CPU `replay`, the headless `Scene::render`, the GPU `chunk_build_cs`) tests each pixel
against a per-pixel depth that starts as the terrain's and takes the writes of earlier draws:

| Format | Test | Writes |
|---|---|---|
| `RGBZ`, `RGBZM` | yes | every drawn pixel |
| `RGBAZ` | yes | every drawn pixel except level 1 (50 %) |
| `RGBV` | yes | every drawn pixel |
| `RGBAV`, `RGBH`, `RGBAH` | yes | never |
| shadows (`ALPHA`), units (soldiers, ships) | no (unchanged) | never |

The draw order stays the painter's order of the chunk lists (far corner sum, then x): with the test, opaque pixels
come out as the original's whatever the order; blended pixels follow the order, as the original's per-frame list
does. Not reproduced (unchanged looks, follow-ups): `RGBAZ` levels 0 and 2 are drawn opaque by the original's writing
brush, `RGBZM`'s melt blend, depth for shadows and units, and the order of the original's cache lists.

### 2.3 Water over the scene
The sea is a horizontal plane at the **shore height** H (`Scene::shore_height`, `--shore`, default 16.5 = the current
look). Where the scene's depth at a pixel lies behind the plane (terrain or a written object pixel), the sea is drawn
over it. Over terrain this is exactly the existing shore rule (at a fixed pixel, depth falls with height), so the
ground does not change. Over objects it is new: the chunk build marks each object pixel whose final depth lies behind
the plane in a `submerged` plane (one byte per pixel), and the compositor draws the sea a second time, after the
objects, on those pixels only: the sea sprite's checkerboard over the object, without the open-water fade (our
stand-in for the floor of texture-0 water, not the original's). Original water only; Enhanced water keeps its own
`object_in_water`. The original's plane
is ≈ 26.5 (`annodisplay.md` §9.3): `--shore 26.5` shows it; the default is the owner's call. Rivers, surf and
waterfalls keep their height + 10 test (`kRiverTolerance`) in this package: their frames' `z` (≈ −22 at the anchor,
+10.7 `HEIGHT`) is what the tolerance approximates.

### 2.4 Cached chunks, derived levels, cost
- `Chunk::submerged` (u8 per pixel, empty when no pixel is submerged); on the GPU an `R8Uint` plane like `base`
  (+256 KiB per Cpu-built chunk that has a submerged pixel, and per GpuList chunk with depth on: the compute pass
  cannot know beforehand). Derived levels (the zoomed-out tiers) do not derive it, as they do not derive
  `base`: there the objects stay pictures over the water.
- The sprite atlases get a parallel `R8Uint` plane of the frames' z bytes; records carry the base depth in their
  reserved `depth` word and the format's depth class in `flags`.
- The CPU builds the depth plane per chunk from the terrain plane it already has; the GPU builds it in the same
  compute pass (one i32 per pixel in registers). Measurements: `docs/perf/README.md`, "W4-DEPTH-1".
- Limit: the GPU's `V` ramp multiplies in i32 (WGSL has no i64); it equals the CPU's i64 product while
  |row − anchor row| · sv < 2³¹, i.e. up to 32 324 rows from the anchor at group 2 (sv 66 435). Every frame of
  the original data is far inside; a modded frame beyond it would wrap on the GPU only. Path to lift it: split the
  product into high and low halves in the shader.
- Switch: `Scene::depth_test` (`--depth off` for the former painter-only picture, a comparison and fallback; kept by
  a new scene generation as `shore_height` is).
