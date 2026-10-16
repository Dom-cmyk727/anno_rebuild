# Isometric projection (island view)

**Status:** draft — rules derived from sprite measurements, visual checks and static analysis of the original ([`docs/re/annodisplay.md`](../re/annodisplay.md), [`docs/re/annowelt.md`](../re/annowelt.md)); not yet compared with screenshots of the original. Open points in OQ-026.

The original renders differently in structure: ground is a mesh of Gouraud-shaded, depth-buffered textured triangles, and sprites are depth-tested per pixel (`z` bytes are depth offsets). This renderer is a painter's-algorithm approximation that uses the same projection constants.

Implementation: `anno_render::iso` (CPU reference renderer), `anno_render::scene` (scenario composition), `anno-extract view`, `anno1503 --view`.

## Tiles

| Rule | Value | Evidence |
|---|---|---|
| Tile diamond (width × height) per zoom group | 84 × 42, 42 × 21, 20 × 10 | P: flat 1 × 1 `Anlagen.dat` sprites (grass, decals) measure 84 × 42/43, 42 × 21/22, 20 × 10 in groups 0, 1, 2; sprite width ≈ (w + h) · 42 for footprints w × h (e.g. palace 4 × 7: 462 px) |
| Screen position of tile `(x, y)` | top vertex at `((x − y) · tw/2, (x + y) · th/2)`; island x runs right-down, y left-down | visual: islands render with coherent coasts and towns |
| Ground | the tile diamond shows its `GroundCell` ([`insel.md`](../formats/insel.md)): tile position `(s, t)` shows `GroundCell::texel` — a 1/6 × 1/6 cell of the 256 × 256 texture, rotated `r` quarter turns | P: of the 8 axis assignments × 2 rotation directions, this one has the lowest colour error across shared tile edges on a polar (84.6 vs ≥ 92.4) and a temperate island (87.6 vs ≥ 92.7) |

## Objects

| Rule | Value | Evidence |
|---|---|---|
| Position | the sprite's anchor (`Frame::anchor_x/y`) lies on the centre of the footprint diamond, raised by the item's own height byte (`PlacedObject::height`) | P: palace 4 × 7: anchor y 243 + half the diamond height 115.5 = 358.5 ≈ sprite height 359 (bottom vertex); flat 1 × 1 sprites have their anchor at the diamond centre (42, 21); visual: towns, rocks and trees stand on their tiles |
| Frame | `rotation · animation frames` (first animation frame); `ANIM` u16 at offset 2 = animation frames; frames per set = animation frames × 1 or × 4 rotations | P: in all sets, total frames / (`ANIM` value) is 1 or 4; visual |
| Draw order | ground first; objects by footprint far corner `x + w + y + h`, then `x` | visual; simplification of the original's per-pixel depth buffer (D: `AnnoDisplay.dll` §3), ? OQ-026 |
| Invisible objects | entries without sprite sets (ids 1 and 2 on the islands checked) are skipped | P |
| Anchor | signed 16-bit; the frame pixel that lands on the projected object point | D: `AnnoDisplay.dll` `0x1000ac80` |
| Shadows | the set's `SHADOW` frame with the same index, own anchor, drawn before the sprite; each covered pixel becomes `dst/2 + dst/8` (5/8 brightness), shadows compound | D: `0x10019f80` (`tBrushShadR`); the original also depth-tests them |
| Alpha | `A` formats: level 0/1/2/3 = 25/50/75/100 % opacity | D: `0x10018890` etc.; simplification: in the original's depth-writing mode `BSH_RGBAZ` blends only level 1 |

## Elevation
- `HEIGHT[x, y]` is the height of tile corner `(x, y)`; tile `(x, y)` spans corners `(x, y)`, `(x+1, y)`, `(x, y+1)`, `(x+1, y+1)` (D: `AnnoWelt.dll` corner grid with stride width + 1, `0x100900c0`, `0x10084870`; the corner reading of `HEIGHT[x, y]` is likely). Each tile is two triangles split along the fixed diagonal from corner `(x, y+1)` to `(x+1, y)` (D: `AnnoWelt.dll` `0x1009b920`, indices A,B,C and C,B,D); the renderer interpolates height inside each triangle. The original also shades each corner from the terrain normal (D: `0x100a03c0`, light vector not traced) — not reproduced.
- One height unit = 0.04 world units (D: `AnnoWelt.dll` table `0x100E35E0`); one world unit up = 51.477 px at zoom group 0 (D: `AnnoFrame.dll` camera `0x10071d70`: scale 59.1633, tilt 29.53°). `iso::HEIGHT_LIFT` = 2.059 px per unit at group 0 (scaled by tile width for groups 1, 2). The same camera gives 83.67 × 41.24 px per tile at zoom 0, consistent with the 84 × 42 sprites.
- Flat raised tiles (tried first) produce terraces and gaps; per-corner slopes remove them (visual). Ground samples also cover the pixel below them to close one-row gaps on steep slopes.
- Open water (texture 0) and textures 1–10 are not part of the original's terrain mesh (D: `AnnoWelt.dll` `0x1009b920`).

## Water
| Rule | Value | Evidence |
|---|---|---|
| Sea surface | animated sprites of `Anlagen.dat` entry 2801 (`MEER`, `Meer\Meer01_001.tga`, 64 `BSH_RGBWZ` frames, 3 × 3 footprint) | D: `AnnoWelt.dll` `0x1009f4c0`; P: entry 2801 as described |
| Lattice | one sprite at tile `(x, y)` with `y` even and `x ≡ 2·((y/2) mod 2) (mod 4)` | D: `0x1009f4c0` |
| Height | sea level 1.28 world units = 32 `HEIGHT` units (≈ 65.9 px at zoom 0), plus a small per-sprite offset (`(x mod 5)·0.01 − (y mod 3)·0.01357` world units, not applied) | D: `0x100E3A20`, set at `0x1009d010` |
| Frame | rotation 0; animation step `(time / 50 ms + (5x + 3y) mod 32) mod 16`: phase from the original, speed from the sprite's `ANIM` record (16 frames × 50 ms) | D: phase formula `0x1009f4c0`; the original's tick unit is not traced, so the `ANIM` duration is used |
| Transparency | `BSH_RGBWZ` is a 50 % checkerboard, so the sea floor shows through | D: brush `0x100161c0`; that this produces the water look is likely |
| Where | 8 × 8 tile chunks with a "sea" bit (`+0x32`, not traced). **Approximation:** chunks containing a non-land tile (corner average ≤ 19, the original's land classifier `0x100900c0`) | ? OQ-026 b |
| Occlusion | the original depth-tests per pixel. **Approximation:** a water pixel is drawn only where the terrain under it (projected back onto the water plane) is below sea level, and only inside the island's tile area | ? |
| Open-water ground | texture 0 has no terrain in the original, so nothing is known about what shows through the checkerboard there. **Approximation:** texture-0 ground and the checkerboard gaps above it are drawn in the average colour / neighbouring pixel of the sea sprite (`iso::water_colour`) | ? OQ-026 b |
| Surf, rivers | ordinary placed objects from `FLUESSE`: rivers 2701/2705–2707, waterfalls and river mouths 2702–2704, surf 2708 (`Brandung01`: an irregular water patch, 54 frames × 80 ms). Each is drawn at its own height byte (surf mostly 0, under the sea surface; rivers at their level); water objects below sea level are drawn before the sea, the rest after it. Surf phase `(3y + 5x) mod 32`. The original places no surf or other coast objects at runtime, so sparse surf is faithful | D: `0x1009f870`, `0x1007aa40` (`docs/re/annowelt.md`); P (counts, heights) |

Visual check 2026-10-16: `Dschungel/D_064x064_0001_0B.scp` at zoom groups 0 and 2 — clean waterline along the beach, shallows and reefs visible through the surface, no sawtooth coast.

## Animation and layers
- A sprite set's `ANIM` record gives the animation: bytes 2–3 frames per animation, bytes 4–5 milliseconds per frame (bit 25 of the `u32` at 4 is a flag of unknown meaning). Frames per set = animation frames × 1 or × 4 rotations (P, all sets). Frame shown = `rotation · frames + (time / ms + phase) mod frames`; phase = the item's phase bits (surf: `(3y + 5x) mod 32`), `(5x + 3y) mod 32` for the sea. The original divides its tick by `ms / 2` (D: `0x100a03c0`); the tick unit is not traced, so the renderer uses the `ANIM` ms per frame (possibly half the original speed).
- Only the water layer (sea, rivers, waterfalls, surf) is animated so far; other animated objects (e.g. mills, fires) show their first frame.
- Layers per island, in order: ground; water at time `t`; other objects (`scene::Layers`). The viewer caches ground and objects per chunk and redraws the water layer for the visible area every 50 ms (about 20–30 ms per frame on the dev Mac, 2026-10-16).
- Shadows are 37.5 % black drawn over the destination (= 5/8 brightness on opaque pixels), so they also work on a transparent object layer.

## Enhanced water (not in the original, opt-in)
Off by default; `E` in the viewer, `--water enhanced` for `--png`. Applies to sea pixels only (water layer), so cached ground and objects are unaffected. Constants are tuned by eye and are not derived from the original.
- Shore distance: per island, a chamfer distance field (in tiles) from every height-grid corner at or above sea level, interpolated per pixel on the water plane.
- Breakers: crest phase `distance / 1.4 + t · ω` (crests move towards the shore, parallel to the coast); sharp crest front with a foam trail; crests broken into slowly drifting segments; foam grows within 4.5 tiles of the shore; swash on the last 0.6 tile. Foam pixels also fill their checkerboard neighbour.
- Swell: two slow crossing sine waves modulate the brightness of open water (± 14 %).
- Cost: ≈ 12 % more for a full scenario render (`Single_01.sze`, group 2).
- Tests: `crates/app/tests/view.rs::view_png_enhanced_water_is_opt_in`.

## Scene composition (scenario view)
`anno_render::scene`; used by `anno1503 --view` (window and `--png`).

| Rule | Value | Evidence |
|---|---|---|
| Island position | island tile `(0, 0)` = world tile `(Island::x, Island::y)` from the `INSEL5` head; world tiles use the tile projection above, so the island's local screen coordinates are shifted by the top vertex of world tile `(x, y)` | P: head fields ([`insel.md`](../formats/insel.md)); visual: the 15 islands of `Single_01.sze` lie apart with no overlap, polar islands at the top and bottom edge |
| Island rectangle | the rectangle of its tile diamonds (as `Canvas::for_island`) plus a margin above of 504 / 252 / 126 px in groups 0 / 1 / 2 | P: largest `anchor_y` of any `Anlagen.dat` frame (entry 1330); an anchor lies on a footprint centre at least half a tile below the top vertex. Must grow once elevation raises tiles (OQ-026 a) |
| Island order | by far corner `x + w + y + h`, then `x` (back to front) | simplification as for objects; only matters where sprites rise into a neighbour's rectangle (OQ-026 f) |
| Composition | each island is drawn into a temporary canvas covering only its overlap with the target region, then composited source-over (straight alpha) | implementation choice |
| Templates (`.scp`) | one island at world tile `(0, 0)` (head position 0) | P |
| Open sea | not drawn: transparent in `--png`; the window fills it with the island-water placeholder colour of `anno_assets::island::WATER` | — (OQ-026 b) |

Sizes measured with `anno1503 --view … --png`: `Single_01.sze` is 16,960 × 8,886 px at group 2 (15 islands, 69,256 objects drawn, 11,717 invisible, 0 failed) and 71,232 × 37,296 px at group 0, too large for one image. The window therefore renders the scene lazily in 1,024 × 1,024 px chunks (all cores) and caches them per zoom group; `--png` refuses scenes above 4 GiB of RGBA.

## Visual checks (2026-10-16)
`anno-extract view`: `Polar/Po_064x064_0008_Eskimo.scp` (igloo village, rocks, ice floes, trading post, wrecks), `Nord/N_128x128_0001_4B_Piraten.scp` (forest, mountain ridges, village), `0201SingleAddOn/single32.sze` island 17 (walled city with plazas, palace, farms; 12,316 objects drawn, 0 failed). `anno1503 --view --png`: `0200Single/Single_01.sze` group 2 (all 15 islands in place, temperate islands in the middle, polar islands at the top and bottom).

## Tests
- `iso::tests::projection`; `scene::tests` (layout, bounds, order, composition on synthetic data); `crates/app/tests/view.rs::view_png_scenario` (`Single_01.sze` group 2: 15 islands drawn, 0 failed objects; needs `ANNO1503_GAME_DIR`); `extract.rs::view_corpus_samples` (no failed objects, canvas size, non-empty image; needs `ANNO1503_GAME_DIR`).
