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
- Flat raised tiles (tried first) produce terraces and gaps; per-corner slopes remove them (visual).
- Rasterisation (implementation): each triangle is filled in screen space, every pixel centre inside it once (small edge tolerance so shared edges leave no cracks), with the texture position interpolated from the corners (a planar triangle projects affinely, so this is exact). Replaced an oversampled forward mapping on 2026-10-17: a 1920 × 1080 view went from 477 ms to 25 ms.
- Open water (texture 0) and textures 1–10 are not part of the original's terrain mesh (D: `AnnoWelt.dll` `0x1009b920`).

## Water
| Rule | Value | Evidence |
|---|---|---|
| Sea surface | animated sprites of `Anlagen.dat` entry 2801 (`MEER`, `Meer\Meer01_001.tga`, 64 `BSH_RGBWZ` frames, 3 × 3 footprint) | D: `AnnoWelt.dll` `0x1009f4c0`; P: entry 2801 as described |
| Lattice | one sprite at tile `(x, y)` with `y` even and `x ≡ 2·((y/2) mod 2) (mod 4)` | D: `0x1009f4c0` |
| Height | sea level 1.28 world units = 32 `HEIGHT` units (≈ 65.9 px at zoom 0), plus a small per-sprite offset (`(x mod 5)·0.01 − (y mod 3)·0.01357` world units, not applied) | D: `0x100E3A20`, set at `0x1009d010` |
| Frame | rotation 0; animation step `(time / 50 ms + (5x + 3y) mod 32) mod 16`: phase from the original, speed from the sprite's `ANIM` record (16 frames × 50 ms) | D: phase formula `0x1009f4c0`; the original's tick unit is not traced, so the `ANIM` duration is used |
| Transparency | `BSH_RGBWZ` is a 50 % checkerboard, so the sea floor shows through | D: brush `0x100161c0`; that this produces the water look is likely |
| Where | 8 × 8 tile chunks with a "sea" bit (`+0x32`, not traced). **Approximation:** inside an island, chunks containing a non-land tile (corner average ≤ 19, the original's land classifier `0x100900c0`); outside all islands, everywhere | ? OQ-026 b |
| World lattice | lattice and phase use **world** tile coordinates (island tile + `INSEL5` position), so the sea continues seamlessly between islands. Lattice and phase repeat after 16 tiles in `x` and `y` (shift `(p, p)` changes the phase by `8p`, `(p, −p)` by `2p`, both ≡ 0 mod 32 for `p = 16`), so the sea at one animation step is a periodic image of 16·tw × 16·th px | world vs. island-local coordinates in the original: not traced (OQ-026 b); periodicity: follows from the formulas |
| Occlusion | the original depth-tests per pixel. **Approximation:** a water pixel is drawn only where the terrain under it (projected back onto the water plane) is below sea level; outside the islands' tile areas it is always drawn | ? |
| Open-water ground | texture 0 has no terrain in the original, so nothing is known about what shows through the checkerboard there. **Approximation:** a flat colour = the average of the most common sea-floor texture at the lowest `HEIGHT` of the scene's textured tiles (`scene::sea_floor_colour`; `Single_01.sze`: `Meer_Meer02.bmp`, RGB 34, 95, 135), falling back to the sea sprite's average colour. With the checkerboard over it the open sea averages RGB (24, 92, 140) vs. (28, 89, 138) measured in the original (see "Reference"); the deep textured floor next to it gives (25, 89, 139) | ? OQ-026 b |
| Floor → open water | per pixel, an openness `o` blends "sprite over the island's sea floor" into "sprite over the open-water colour": `o` = share of open-water tiles among the 4 × 4 tiles around each height corner (bilinear inside a tile), raised to 1 over the last 8 tiles before the island border (`sea::BORDER_FADE_TILES`) so that every island's floor texture fades into the shared open sea. Replaced (2026-10-17) the gap fill of open-water tiles, which switched at tile edges and showed as a staircase along coasts, and removed the visible island rectangles | implementation choice (approximation, OQ-026 b) |
| Surf, rivers | ordinary placed objects from `FLUESSE`: rivers 2701/2705–2707, waterfalls and river mouths 2702–2704, surf 2708 (`Brandung01`: an irregular water patch, 54 frames × 80 ms). Each is drawn at its own height byte (surf mostly 0, under the sea surface; rivers at their level); water objects below sea level are drawn before the sea, the rest after it. Surf phase `(3y + 5x) mod 32`. The original places no surf or other coast objects at runtime, so sparse surf is faithful | D: `0x1009f870`, `0x1007aa40` (`docs/re/annowelt.md`); P (counts, heights) |

Visual check 2026-10-16: `Dschungel/D_064x064_0001_0B.scp` at zoom groups 0 and 2 — clean waterline along the beach, shallows and reefs visible through the surface, no sawtooth coast.

## Animation and layers
- A sprite set's `ANIM` record gives the animation: bytes 2–3 frames per animation, bytes 4–5 milliseconds per frame (bit 25 of the `u32` at 4 is a flag of unknown meaning). Frames per set = animation frames × 1 or × 4 rotations (P, all sets). Frame shown = `rotation · frames + (time / ms + phase) mod frames`; phase = the item's phase bits (surf: `(3y + 5x) mod 32`), `(5x + 3y) mod 32` for the sea. The original divides its tick by `ms / 2` (D: `0x100a03c0`); the tick unit is not traced, so the renderer uses the `ANIM` ms per frame (possibly half the original speed).
- Only the water layer (sea, rivers, waterfalls, surf) is animated so far; other animated objects (e.g. mills, fires) show their first frame.
- Layers (`scene::Layers`), in order: ground; water objects below sea level; the sea surface (whole region); water objects at or above sea level; other objects. Ground and objects are static per zoom group.
- Frame pipeline (`anno_render::frame`, used by the viewer and the benchmark): static data (ground, objects, the per-pixel sea field `sea::WaterField`) is built per 512 × 512 px chunk by background workers and cached. The workers take jobs from a priority list that every frame rewrites (visible chunks, then a ring of one chunk around the view, then the same area in the neighbouring zoom groups); a chunk not built yet shows open sea. A frame then composes ground → water objects below sea level (at the frame time) → sea (per-pixel lookup into `sea::SeaPattern`, the periodic sea image of each animation step, rendered once per zoom group) → water objects above sea level → objects.
- Two compositors with the same rules: the GPU (`anno_render::gpu`, wgpu, shader `gpu.wgsl`; chunks as textures, sea pattern as texture arrays, water objects from a sprite atlas, two render passes) and the CPU (`FrameRenderer::render`, row bands on all cores). Integer pixel positions and `textureLoad` keep the GPU exact: identical to the CPU within 1 per channel for the original water (`bench_frame --check`). Measurements: [`docs/perf/README.md`](../perf/README.md).
- The viewer uses the GPU when available (`--renderer auto|gpu|cpu`), presents with vsync and shows the renderer and frames per second in the title; while the window is occluded it retries every 100 ms instead of spinning. The CPU fallback redraws at the display refresh rate. `Original` water changes image once per 50 ms animation step, as the sprite's `ANIM` record; `Enhanced` cross-fades between consecutive steps.
- Shadows are 37.5 % black drawn over the destination (= 5/8 brightness on opaque pixels), so they also work on a transparent object layer.

## Reference: gameplay video of the original (2026-10-17)
Source: YouTube video `2I-T_8SAFuU` (Anno 1503 History Edition — the 2020 re-release of the same engine and data; differences from GOG 2.0.0.5 are possible), 7:50–12:00 at 1920 × 1080, analysed locally; no frames are kept in the repository.
- **No breaking waves.** Coasts go from sand to translucent shallow water (sea floor visible, lighter near the shore) to deep blue; no foam lines on beaches, only a ship's wake. Confirms that the faithful water has no breakers and that `WaterStyle::Enhanced` is an addition.
- **Water animates:** in a static camera shot, water pixels change ≈ 4 × as much as land pixels between frames 0.2 s apart (land change = compression noise).
- **Loop period ≈ 0.8 s:** frame-to-frame difference of a water patch is smallest at 24 video frames (0.8 s at 30 fps) and largest around 12–16 frames. That matches 16 frames × 50 ms from the sea sprite's `ANIM` record, i.e. one animation step per `ANIM` ms, and rules out the double speed (0.4 s) that `tick / (ms / 2)` with a millisecond tick would give (OQ-026).
- **Colour:** open sea averages RGB (28, 89, 138) in the video, ours (37, 107, 142) at zoom 0 — the original is darker and less green. Candidate cause: the colour parameter `0xC0` passed with every water sprite (selects a colour-table variant, not applied yet; `docs/re/annodisplay.md`). The original's water texture also looks finer and more sparkling.

## Enhanced water (not in the original, opt-in)
Off by default; `E` in the viewer, `--water enhanced` for `--png`. Applies to sea pixels only, so cached ground and objects are unaffected. Not derived from the original (which has no breakers, see "Reference"); the owner's target is crashing waves on coasts and open sea (PLAN §0.2). Tuning lives in `iso::Waves::DEFAULT` (to become configuration); `gpu.wgsl` mirrors it.

Reworked 2026-10-17 after the colleague's review recording, which showed white blobs forming a dash pattern at zoom group 2 ("Rasterstreifen"), thin lines parallel to the coast, angular coastal bands and a muddy shallow band. Current model (`iso::wave_shade` → light, foam, tint):
- Open water: slow swell from two drifting layers of smooth value noise (brightness ± 7 %); no whitecaps.
- Coast (inside 3 tiles of the shore): crest phase `distance / 1.4 + t / 3000 ms + 0.8 · bend + 0.5 · stagger` (two value-noise fields of different scale) — crests travel towards the beach, bent by slow noise, and arrive as separate pieces with calm water between them (segment noise gates them fully, 2026-10-17 owner feedback: one continuous crest line along the whole coast looked drawn). Crests brighten the water (no dark faces, so no contour lines); a white curl runs along the steep front just before breaking (1.6–2.6 tiles); inside 1.6 tiles the crest becomes mottled foam; inside 0.7 tiles a swash sheet runs up the beach after each crest and fades.
- Waterline: the original's waterline follows its terrain triangles, so it is straight between tile corners and steps along tile edges where the height changes within a tile (owner: "at water level you never find straight lines", except at buildings). Enhanced water decides where water is from a smooth surface instead: a cubic B-spline through the corner heights (no overshoot) plus fixed value noise of ±2 `HEIGHT` units (`sea::smooth_height`, `Waves::shore_noise`), with a soft edge of ±0.35 units (`Waves::shore_edge`). Water may thus cover the lowest beach pixels or leave the shallowest floor dry; buildings and other objects are drawn on top as before. The signed depth over that surface is stored per pixel (`WaterField::depth`, 1/4 units around 128) next to a flag for the original's waterline (`WaterField::flags`); the original mode uses only the flag.
- Deep water hides the sea floor: the floor fades into open water from 10 to 28 `HEIGHT` units below sea level (`iso::deep_water`, over the smooth surface of the waterline). The terrain height is smooth across tiles, so this removes the tile-shaped edges between the original's floor textures (shallows, sand, deep sea) that showed as a stepped boundary between near-island water and deep sea (owner feedback 2026-10-17). The original mode keeps the original floor.
- Shallow water within 2.2 tiles of the shore is tinted towards turquoise (RGB 0.36, 0.74, 0.76, up to 30 %).
- Detail level: fine foam texture fades out with the zoom (`iso::wave_detail`, 1 at group 0, 0 at group 2), so nothing aliases into patterns.
- Shore distance: per island, a chamfer distance field (in tiles) from every height-grid corner at or above sea level, smoothed by two 3 × 3 box-blur passes (removes the octagonal kinks), interpolated per pixel and stored in 1/32 tiles (`sea::SHORE_SCALE`; 7.97 tiles = open sea).
- The original's surf sprites (`ANLAGEN` 2708) are not drawn in enhanced mode; its own surf replaces them (they showed as dark diamonds over the tinted shallows).
- Smooth animation: the sea cross-fades between consecutive sprite steps (the original jumps every 50 ms).
- Smooth surface: each sea pixel averages the 2 × 2 pattern pixels at its position (premultiplied alpha), so the sprite's 50 % checkerboard (`BSH_RGBWZ`), which showed as fine diagonal hatching on high-resolution displays, becomes a continuous surface; the floor still shows through at 50 %.
- Deterministic: a pure function of position and time (value noise from an integer hash), so `--png --time` is reproducible.
- Evaluation: the GPU evaluates it per pixel; the CPU path on a grid of 4 / 2 / 2 px (groups 0 / 1 / 2, `sea::SHADING_STEP`), interpolated per pixel, so foam edges differ slightly between the two.
- Not yet: spray above the water at impact, wave interaction with rocks and ships; shore distance across islands (each island's field ends at its rectangle).
- Tests: `iso::tests::wave_noise_is_deterministic_and_bounded`, `crates/app/tests/view.rs::view_png_enhanced_water_is_opt_in`; visual comparison with `bench_frame --check` dumps (`BENCH_DUMP`).

## Scene composition (scenario view)
`anno_render::scene`; used by `anno1503 --view` (window and `--png`).

| Rule | Value | Evidence |
|---|---|---|
| Island position | island tile `(0, 0)` = world tile `(Island::x, Island::y)` from the `INSEL5` head; world tiles use the tile projection above, so the island's local screen coordinates are shifted by the top vertex of world tile `(x, y)` | P: head fields ([`insel.md`](../formats/insel.md)); visual: the 15 islands of `Single_01.sze` lie apart with no overlap, polar islands at the top and bottom edge |
| Island rectangle | the rectangle of its tile diamonds (as `Canvas::for_island`) plus a margin above of 504 / 252 / 126 px in groups 0 / 1 / 2 | P: largest `anchor_y` of any `Anlagen.dat` frame (entry 1330); an anchor lies on a footprint centre at least half a tile below the top vertex. Must grow once elevation raises tiles (OQ-026 a) |
| Island order | by far corner `x + w + y + h`, then `x` (back to front) | simplification as for objects; only matters where sprites rise into a neighbour's rectangle (OQ-026 f) |
| Composition | each island is drawn into a temporary canvas covering only its overlap with the target region, then composited source-over (straight alpha) | implementation choice |
| Templates (`.scp`) | one island at world tile `(0, 0)` (head position 0) | P |
| Open sea | the sea surface covers the whole region, between islands too (see "Water") | approximation (OQ-026 b) |

Sizes measured with `anno1503 --view … --png`: `Single_01.sze` is 16,960 × 8,886 px at group 2 (15 islands, 69,256 objects drawn, 11,717 invisible, 0 failed) and 71,232 × 37,296 px at group 0, too large for one image. The window therefore renders the static layers lazily in chunks (`anno_render::frame`); `--png` refuses scenes above 4 GiB of RGBA.

## Visual checks (2026-10-16)
`anno-extract view`: `Polar/Po_064x064_0008_Eskimo.scp` (igloo village, rocks, ice floes, trading post, wrecks), `Nord/N_128x128_0001_4B_Piraten.scp` (forest, mountain ridges, village), `0201SingleAddOn/single32.sze` island 17 (walled city with plazas, palace, farms; 12,316 objects drawn, 0 failed). `anno1503 --view --png`: `0200Single/Single_01.sze` group 2 (all 15 islands in place, temperate islands in the middle, polar islands at the top and bottom).

## Tests
- `iso::tests::projection`; `scene::tests` (layout, bounds, order, composition on synthetic data); `crates/app/tests/view.rs::view_png_scenario` (`Single_01.sze` group 2: 15 islands drawn, 0 failed objects; needs `ANNO1503_GAME_DIR`); `extract.rs::view_corpus_samples` (no failed objects, canvas size, non-empty image; needs `ANNO1503_GAME_DIR`).
