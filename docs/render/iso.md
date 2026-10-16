# Isometric projection (island view)

**Status:** draft — rules derived from sprite measurements and visual checks of rendered islands; not yet compared with screenshots of the original or with the `AnnoDisplay.dll` code. Open points in OQ-026.

Implementation: `anno_render::iso` (CPU reference renderer), `anno-extract view`.

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

## Visual checks (2026-10-16)
`anno-extract view`: `Polar/Po_064x064_0008_Eskimo.scp` (igloo village, rocks, ice floes, trading post, wrecks), `Nord/N_128x128_0001_4B_Piraten.scp` (forest, mountain ridges, village), `0201SingleAddOn/single32.sze` island 17 (walled city with plazas, palace, farms; 12,316 objects drawn, 0 failed).

## Tests
- `iso::tests::projection`; `extract.rs::view_corpus_samples` (no failed objects, canvas size, non-empty image; needs `ANNO1503_GAME_DIR`).
