# Isometric projection (island view)

**Status:** draft — rules derived from sprite measurements and visual checks of rendered islands; not yet compared with screenshots of the original or with the `AnnoDisplay.dll` code. Open points in OQ-026.

Implementation: `anno_render::iso` (CPU reference renderer), `anno_render::scene` (scenario composition), `anno-extract view`, `anno1503 --view`.

## Tiles

| Rule | Value | Evidence |
|---|---|---|
| Tile diamond (width × height) per zoom group | 84 × 42, 42 × 21, 20 × 10 | P: flat 1 × 1 `Anlagen.dat` sprites (grass, decals) measure 84 × 42/43, 42 × 21/22, 20 × 10 in groups 0, 1, 2; sprite width ≈ (w + h) · 42 for footprints w × h (e.g. palace 4 × 7: 462 px) |
| Screen position of tile `(x, y)` | top vertex at `((x − y) · tw/2, (x + y) · th/2)`; island x runs right-down, y left-down | visual: islands render with coherent coasts and towns |
| Ground | the tile diamond shows its `GroundCell` ([`insel.md`](../formats/insel.md)); diamond point with tile coordinates `(s, t)` ∈ [0, 1)² shows cell pixel `(32 s, 32 t)` before the cell rotation | P: of the 8 axis assignments × 2 rotation directions, this one has the lowest colour error across shared tile edges on a polar (84.6 vs ≥ 92.4) and a temperate island (87.6 vs ≥ 92.7) |

## Objects

| Rule | Value | Evidence |
|---|---|---|
| Position | the sprite's anchor (`Frame::anchor_x/y`) lies on the centre of the footprint diamond | P: palace 4 × 7: anchor y 243 + half the diamond height 115.5 = 358.5 ≈ sprite height 359 (bottom vertex); flat 1 × 1 sprites have their anchor at the diamond centre (42, 21); visual: towns, rocks and trees stand on their tiles |
| Frame | `rotation · animation frames` (first animation frame); `ANIM` u16 at offset 2 = animation frames; frames per set = animation frames × 1 or × 4 rotations | P: in all sets, total frames / (`ANIM` value) is 1 or 4; visual |
| Draw order | ground first; objects by footprint far corner `x + w + y + h`, then `x` | visual (no wrong overlaps noticed); simplification, ? OQ-026 |
| Invisible objects | entries without sprite sets (ids 1 and 2 on the islands checked) are skipped | P |
| Shadows | not drawn yet | — |

## Elevation
`HEIGHT` (0–208) is not applied by default. Raising flat tiles by `HEIGHT · lift` pixels (tried 1/2 and 1 px per unit) produces terraces and gaps along coasts, so elevation needs sloped tiles (heights per corner) before a scale can be chosen; see OQ-026.

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
