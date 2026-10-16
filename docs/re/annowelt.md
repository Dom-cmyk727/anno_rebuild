# `AnnoWelt.dll` — island heights and terrain mesh

**Status:** first pass (2026-10-16), static analysis only. Module: `game/app/AnnoWelt.dll` (GOG 2.0.0.5, 966,656 bytes, image base `0x10000000`). Tools: Ghidra 12.1.4 headless, `objdump -d`. Confidence labels as in [`annodisplay.md`](annodisplay.md). Behaviour paraphrased, no code copied.

## Island record loader
`0x10089130` reads the island records (`HEIGHT`, `GFXID`, `GELAENDE`, `FRUCHTBAR`, `FLUESSE`, `PROD2`, `BERGWERK2`, `OCCUR`, `ANBAU`, …; string table at `0x100DCF1C` ff.). `HEIGHT` and `GFXID` blocks (6-byte block header + data, as in `docs/formats/insel.md`) are handed to island vtable slots `+0xAC` and `+0xB4`. **Confirmed.**

## Height grid is per corner
- The island object keeps the heights at `+0x14` as a byte grid with row stride **width + 1**, i.e. a (width + 1) × (height + 1) corner grid. **Confirmed**: `0x100900c0`, `0x10090350`, `0x10090560` read the four bytes `(x, y)`, `(x+1, y)`, `(x, y+1)`, `(x+1, y+1)` with that stride, average them (`sum >> 2`) and compare against a level to classify a tile.
- Island vtable (`0x100CA19C`) slot `+0xAC` = `0x10084870` copies a `HEIGHT` block into that grid, clamping the block's right/bottom edge to width + 1 / height + 1. **Confirmed** (clamp); together with the stride this means `HEIGHT[x, y]` is the height of **corner (x, y)** of the corner grid, and the `width × height` record leaves the last corner row and column at their initial value. **Likely** (copy loop body not read instruction by instruction; initial value of the extra row/column not traced).
- Tile `(x, y)` uses corners `(x, y)`, `(x+1, y)`, `(x, y+1)`, `(x+1, y+1)` (see mesh builder below). **Confirmed.**

## Height → world units
`0x1009ad40` (static initialiser) fills a 256-entry float table at `0x100E35E0` with `table[h] = h · 0.04` (start `0x100C1128` = 0.0, step `0x100CB844` = 0.04). Neighbouring initialisers derive `0.64` (= 16 · 0.04), `10.24` (= 256 · 0.04), `1.28`, `0.93`, `0.78` from the same step (`0x1009ac60`…`0x1009ad10`); their use is not traced. **Confirmed.**

With the camera in [`annodisplay.md`](annodisplay.md) §5 (one world unit up = 51.477 px at zoom 0), one `HEIGHT` unit raises a vertex by **0.04 · 51.477 ≈ 2.06 px at zoom 0** (≈ 1.03 px at zoom 1, ≈ 0.51 px at zoom 2). The corpus maximum `HEIGHT` 208 is 8.32 world units ≈ 428 px at zoom 0. **Confirmed** arithmetic on confirmed constants; not yet checked against a screenshot.

## Terrain mesh builder
`0x1009b920` builds the ground meshes for an 8 × 8 tile chunk from a per-chunk 9 × 9 array of 4-byte corner records at `+0x204`: `u16` `GFXID` of the tile at `+0`, corner height at `+2`, a byte used as the vertex light value at `+3`. `0x1009e740` fills `+0`/`+2` from the island's `GFXID` and corner-height grids (9th column and row from the neighbouring corners); where `+3` is filled is not traced. **Confirmed** (layout as used by both functions).

- Tiles are grouped by ground texture = **high byte of `GFXID`**; only tiles with texture **> 10** get triangles. Texture 0 (open water) and ids 1–10 are not part of the terrain mesh, so open water is drawn by some other path (OQ-026 b). **Confirmed** (the `> 10` test on the `GFXID` high byte); the water path itself is not found.
- Vertex = (world x = tile x + island x offset, world up = `table[corner height]`, world z = tile y + island y offset, light byte, two further floats from `0x1007b030`/`0x1007b0f0` — presumably texture coordinates from the `GFXID` cell bits, **guess**). The scale factor for x and z is `0x100CB840` = 1.0: **one world unit per tile**. **Confirmed.**
- A tile's corners are emitted as shared vertices (index reuse when neighbouring corners match); the triangle split of each tile quad is not yet read. Open.

## Open
- Which diagonal splits a tile quad into two triangles, and the texture-coordinate mapping (`0x1007b030`, `0x1007b0f0`).
- Initial value of the last corner row/column; whether `HEIGHT` blocks at the island edge ever cover them.
- Source of the per-corner light byte (slope shading?).
- How open water and texture ids 1–10 are drawn.
- The world position passed to `AnnoDisplay` for buildings and figures (callers of `tDrawSegmentDD2` slot `+0xBC`).
