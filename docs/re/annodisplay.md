# `AnnoDisplay.dll` — sprite blitters, brush selection, projection

**Status:** first pass (2026-10-16), static analysis only. Module: `game/app/AnnoDisplay.dll` (GOG 2.0.0.5, 344,064 bytes, image base `0x10000000`). Tools: Ghidra 12.1.4 headless (auto-analysis + decompiler), `objdump -d`, RTTI recovered by hand from MSVC complete-object locators (Ghidra's RTTI analyser did not name the vtables in this build). Addresses are virtual addresses in this module unless another module is named. Nothing here is copied code; behaviour is paraphrased.

Confidence: **confirmed** = read directly in the decompiled/disassembled function; **likely** = strong inference from code structure or names, not traced end to end; **guess** = hypothesis, also listed under "Open" at the end.

MSVC template arguments in the RTTI names use the mangled number encoding (`$0`–`$9` = 1–10, `$0DP` = 0x3F, `$0PP` = 0xFF): `tLineBrush<5,6,5>` = RGB565 target, `<5,5,5>` = RGB555, `tTriangleGouraudZBufferRW<6|7|8>` = texture size 2⁶/2⁷/2⁸ (64/128/256). Every blitter exists twice (555 and 565); the 565 variant is selected when the draw-texture byte at `+0x68` is 0, the 555 one when it is 1 (confirmed from the table offsets below).

## 1. Format string → brush type code

`0x10003240` (sprite-set record reader, handles `BODY2`, `ANIM`, `SOUND`, `SOUND2`, `TEXTURES`, `SHADOW`, `OUTLINE`). For each 36-byte frame header in `TEXTURES` it compares the 12-byte format name and passes a type code to the texture object (vtable slot `+0x68`); it also stores property `0x17` = (width, height) and property `5` = (anchor x, anchor y) from the header. **Confirmed.**

| Format | Type code |
|---|---|
| `BSH_RGB` | `0x0D` |
| `BSH_RGBZ` | `0x0E` |
| `BSH_RGBH` | `0x0F` |
| `BSH_RGBV` | `0x10` |
| `BSH_RGBAZ` | `0x11` |
| `BSH_RGBZM` | `0x12` |
| `BSH_RGBW` | `0x13` |
| `BSH_RGBWZ` | `0x14` |
| `BSH_RGBAH` | `0x15` |
| `BSH_RGBAV` | `0x16` |
| `BSH_ALPHA` | `0x17` |
| `BSH_OUTLN` | `0x18` |

Other codes used by the brush switch: `0x00` = textured triangle (terrain), `0x0C` = flat line, `0x19` = 16-bit direct-colour brush (`tBrushNorm16`). `0x1000acd0` (texture `SetType`) flags codes `0x11`, `0x15`, `0x16` (the `A` formats) with `+7 = 4`; the layer draw (`0x1000d680`) treats exactly these three specially (bit mask `0x310` over `code − 0x0D`). **Confirmed.**

## 2. Brush objects and selection

- `0x10014410` (`tDrawTexture` vtable slot `+0x7C`): allocates one instance of every brush class and stores it in a table inside the draw-texture object (offsets `+0x6C`…`+0x128`; vtable → class from RTTI). **Confirmed.**
- `0x100143e0` (slot `+0x6C`, `SetState(i, v)`): writes `state[i]` at `+0x30 + 4i` and re-selects the brush. Relevant states: 1 (`+0x34`) depth test on/off, 5 (`+0x44`) type code from §1, 6 (`+0x48`) depth write on/off, 7/8 (`+0x4C`/`+0x50`) global alpha enable/value, 9 (`+0x54`) variant for `RGBAV`. `0x100169b0` resets to depth test on, depth write on, type `0x0C`. **Confirmed.**
- `0x10013eb0` (`SelectBrush`): switch on the type code in three modes. **Confirmed** (mode names are ours):

| Type | Mode A: no depth (`+0x34 = 0`) | Mode B: depth test, no write (`+0x48 = 0`) | Mode C: depth test + write (default) |
|---|---|---|---|
| `RGB`, `RGBV` | `tBrushNorm<FF>`; with global alpha `tBrushAlphaRaster` | `tBrushVertZR` | `tBrushVertZRW` |
| `RGBH` | as `RGB` | — (no brush) | `tBrushHoriR` |
| `RGBZ` | `tBrushDepthAlpha<FF>` | `tBrushDepthR` | `tBrushDepthZRW` |
| `RGBZM` | `tBrushDepthAlpha<FF>` | — | `tBrushDepthMeltZRW` |
| `RGBAZ` | `tBrushDepthAlpha<3F>` | `tBrushAlphaDepthZR` | `tBrushAlphaDepthZRW` |
| `RGBWZ` | `tBrushRasterDepth` | `tBrushRasterDepthR` | `tBrushRasterDepthRW` |
| `RGBAH` | `tBrushAlphaA` | `tBrushAlphaHoriZR` | `tBrushAlphaHoriZR` |
| `RGBAV` | `tBrushAlphaA` | `tBrushAlphaVertZR` or `tBrush2ScreenAlphaVertZR` (state 9) | — |
| `ALPHA` | — | `tBrush2ScreenShadR` | `tBrushShadR` |
| triangle (`0`) | — | — | `tTriangleGouraudZBufferRW<6/7/8>` by texture size 64/128/256 |

"—" = falls to the default brush (`tDrawTexture::IOutputBase`, whose draw slot `0x100149c0` does nothing), i.e. the frame is not drawn in that mode. `BSH_RGBW` and `BSH_OUTLN` have no brush in any mode.

Which mode the game uses for which layer is not traced (likely: mode C for static scenery rendered into the cached screen tiles, mode B for things drawn on top without disturbing the depth buffer, mode A for UI — **guess**, see Open).

Brush draw call (vtable slot `+0x0C`), as called from the layer code at `0x1000d3d0`/`0x1000d64c`: `(rect* {x0, y0, x1, y1}, float depth, float depth_step, u32 colour_param)`. Each blitter converts `depth` (and `depth_step`) with `__ftol` to 16.16 fixed point and uses the high 16 bits as the sprite's depth; `colour_param >> shift` selects one 256-entry `u16` colour table (`0x200` bytes) from a base pointer (palette variant, e.g. player colour or light — meaning of the variants not traced). **Confirmed.**

## 3. RLE blitters

All blitters share the row grammar already documented in `docs/formats/figuren.md`: per row `(skip, count, data)` runs, `skip ≥ 0xFE` ends the row (`0xFE`) or the frame (`0xFF`). Common behaviour, **confirmed** in `0x10014ca0` (`tBrushVertZRW` 555), `0x100178d0` (`tBrushDepthZRW` 555), `0x100182f0` (`tBrushAlphaDepthZRW` 555), `0x1001b570` (`tBrushAlphaA` 555), `0x10019f80` (`tBrushShadR` 555), `0x100161c0` (`tBrushRasterDepthRW`):

- Destination is a 16-bit (555/565) colour surface plus a parallel `u16` depth buffer with its own pitch.
- **Clipping (OQ-025):** rows above the clip rectangle are skipped by parsing; within a row the run start is clipped against the clip rectangle's left edge and the run length against its right edge; once `x` reaches the right clip edge the rest of the row is skipped. The **frame width is never consulted**. Runs that end past the frame `width` are therefore drawn at their encoded positions, i.e. to the right of the frame box on screen, limited only by the viewport/clip rectangle. They are not wrapped and not cut at the frame edge.
- Depth comparison: a pixel is drawn when `pixel_depth <= depth_buffer[x]` (unsigned 16-bit), so smaller = nearer. Write brushes (`…RW`, `…ZRW`) store `pixel_depth` into the buffer when they draw; read-only brushes (`…R`, `…ZR`) leave it.

### Per-pixel depth (`Z` formats) — the `z` byte (OQ-023 c)
`tBrushDepthZRW` (`0x100178d0`), `tBrushDepthR`, `tBrushAlphaDepthZR(W)`, `tBrushDepthMeltZRW`, `tBrushRasterDepth*`: for each `[z, index]` pair, `pixel_depth = sprite_depth + (signed 8-bit) z`. The `z` byte is a **signed per-pixel depth offset** relative to the sprite's base depth (−128…+127 in depth-buffer units). The sheep's `255` bytes are −1. **Confirmed.** (How depth-buffer units relate to world distance is not established — Open.)

### Depth ramp (`V`, `H` and shadow brushes)
`tBrushVertZRW` (`0x10014ca0`), `tBrushHoriR` (`0x10015220`), `tBrushShadR` (`0x10019f80`): no per-pixel `z`; the depth starts at the sprite depth and grows by `depth_step` per row (16.16 fixed point). **Confirmed.**

### `V` vs `H` (OQ-023 e)
In mode C, `RGBV` (and `RGB`) use `tBrushVertZRW` (depth test **and write**), `RGBH` uses `tBrushHoriR`: same row ramp and depth test, but **no depth write**. `RGBAH` likewise uses the read-only `tBrushAlphaHoriZR`. **Confirmed** (code). Reading of the letters: `V` = vertical (standing sprite that occludes later sprites), `H` = horizontal (flat decal that is occluded but never occludes) — **likely**, from the class names `Vert`/`Hori` and the write behaviour.

### Alpha level (OQ-023 d)
In the depth brushes the count byte of `A` formats is `level << 6 | count` (count masked with `0x3F`). **Confirmed** in `tBrushAlphaDepthZR` (`0x10018890`), `tBrushAlphaVertZR` (`0x100193f0`), `tBrush2ScreenAlphaVertZR` (`0x100199a0`):

| `level` | Result |
|---|---|
| 0 | 25 % source + 75 % destination (helper `0x1001f4e0`; inline in `0x10018890`) |
| 1 | 50 % / 50 % (`0x1001f5a0`, `0x1001f410`) |
| 2 | 75 % source + 25 % destination (`0x1001f650`, `0x1001f470`) |
| 3 | opaque copy (`0x1001c620`, `0x1001c970`) |

Blending is done per channel with shift-and-mask on the 16-bit colour (e.g. 555: `(a >> 1 & 0x3DEF) + (b >> 1 & 0x3DEF)`; quarter steps with masks `0x739C`/`0x7BDE`). Exceptions, **confirmed**:
- `tBrushAlphaDepthZRW` (`0x100182f0`, `RGBAZ` in mode C) and `tBrushAlphaHoriZR` (`0x10018e70`): only `level == 1` is blended 50 %; levels 0, 2 and 3 are drawn opaque. In the ZRW brush the blended pixels are depth-tested but do not write depth; opaque ones write.
- Mode A (`tBrushAlphaA` `0x1001b570`, `tBrushDepthAlpha<3F>` `0x1001b060`): the per-run level is ignored; a global alpha from state 8 applies to the whole sprite (`0x40` → 25 %, `0x80` → 50 %, `0xC0` → 75 % source, anything else opaque; the brush is set to `0x100` when global alpha is off).

So: level 3 = opaque (matches the gondola observation), 0/1/2 = 25/50/75 % coverage.

### Shadows (`BSH_ALPHA`, OQ-026 d)
`tBrushShadR` (`0x10019f80`, mode C) and `tBrush2ScreenShadR` (`0x1001a560`, mode B). Runs carry no pixel bytes (`skip, count` only). For each covered pixel that passes the depth test (row ramp, no depth write) the colour becomes `dst/2 + dst/8` per channel (masks `0x3DEF` and `0x0C63` for 555, `0x7BDE`/`0x6318` in the single-pixel tail), i.e. **5/8 of the destination brightness** (shadow darkens by 37.5 %). **Confirmed.** `tBrushShadR` reads and writes the same surface, so overlapping shadows from different sprites compound. `tBrush2ScreenShadR` reads from a second surface (offset `+0x28`) and writes to the screen surface (`+0x24`), so a shadow is always computed from the unshadowed source and overlapping shadows do not compound (**confirmed** that it reads one buffer and writes another; that the source is an unshadowed copy is **likely**).

### `M` in `BSH_RGBZM` (OQ-023)
`tBrushDepthMeltZRW` (`0x10017db0`): per-pixel `z` as for `RGBZ`. If the pixel passes the depth test and its depth is more than one unit nearer than the buffer (`depth + 1 < buffer`), it is drawn opaque; if it is equal or one unit nearer, it is blended 50 % with the destination. Depth is written in both cases. **Confirmed.** Effect: soft seams where the sprite intersects existing geometry at the same depth ("Melt"; used for cliffs per `docs/formats/anlagen.md`). Name reading **likely**.

### `W` in `BSH_RGBWZ` (OQ-023)
`tBrushRasterDepthRW` (`0x100161c0`), `tBrushRasterDepthR` (`0x10015f60`), `tBrushRasterDepth` (`0x10015510`): `[z, index]` pairs with per-pixel depth as above, but **each stored pixel is drawn at every second screen column**: `skip` is doubled, each pair advances the destination by two pixels, and the starting column of each row is offset by a checkerboard phase `(x0 ^ y) & 1` that toggles every row. The stored rows are therefore half the frame width, and the frame is drawn as a 50 % screen-door (checkerboard) pattern over the full width. **Confirmed** (code). Data check (P, 2026-10-16): the first 1,016 distinct `BSH_RGBWZ` ranges of `Anlagen.dat` zoom group 0 have rows ending at ≈ width/2 (e.g. width 262 → 128). Consequence for the decoder: a `WZ` row must be expanded ×2 with the checkerboard phase, not decoded at 1 byte-pair per pixel. The `W` presumably stands for water (rivers, `Fluss01.PCX`) — **guess**.

## 4. Sprite placement and the anchor

`0x1000ac80` (`tTextureHaus` slot `+0x70`, screen rectangle of a frame): `x0 = round(screen_x) − anchor_x`, `y0 = round(screen_y) − anchor_y`, `x1 = x0 + width`, `y1 = y0 + height`, where `anchor_x`/`anchor_y` are read as **signed 16-bit** (`short`). **Confirmed.** So the anchor is the pixel of the frame that lands on the projected world point of the object. P (2026-10-16): 735 of 731,121 `FigurenHeader.dat` frames have an anchor component ≥ `0x8000`, i.e. negative as `i16` (none in `Anlagen.dat`); the spec currently lists both as `u16`.

`0x10011e60` (`tDrawSegmentDD2` slot `+0xBC`, add object): projects a world position `(x, y, z)` (float[3]) with the segment's projection (§5), asks the object for its frames for (zoom, rotation `(r + view_rotation) & 0xF`, frame), takes each frame's rectangle from the call above, and inserts a draw item into every 128 × 128 screen tile the rectangle overlaps. **Confirmed** (structure); which world point the game passes for a building or figure (footprint centre, corner, height) is decided in the caller (`AnnoWelt.dll`) and not traced — Open.

## 5. Projection (camera matrix)

- `0x1000c380`: projects a world point `(x, y, z)` with a 3 × 3 matrix plus offsets: `screen_x = m00·x + m10·y + m20·z + ox`, `screen_y = H − (m01·x + m11·y + m21·z + oy)`, `depth = m02·x + m12·y + m22·z + oz`. `0x1000fc30` (`tDrawSegmentDD2` slot `+0x74`, `SetMatrix(id, float[16])`): id 4 and 6 store 4 × 4 matrices; id 6 triggers `0x1000c150`, which precomputes per-pixel depth slopes. **Confirmed.** The matrix comes from the caller.
- Caller, **module `AnnoFrame.dll`**: `0x10003560` pushes `SetMatrix(4, …)` and `SetMatrix(6, …)`; the camera matrix is built by `0x10071d70` as `Ry(a) · Rx(b) · S`, matrix product `0x100713a0`, with
  - `0x100712c0`: rotation about the vertical world axis `y`, angle `a = view_rotation · 0x80 + 0xC0` in units of π/256 (`0x1008E168` = π, `0x1008E170` = 1/256) → 135° + 90° per view rotation;
  - `0x10071240`: rotation about `x`, angle `b = 0x2A` = 42 units = 29.53°;
  - `S = diag(s, s, 0.000384615, 1)`, `s = zoom · 59.163303` (`0x1008E1A0`), `zoom` = 1.0 / 0.5 / 0.25 for zoom levels 0 / 1 / 2 (`0x10003c30`); the depth scale is the immediate `0x39C9A634`.
  **Confirmed** (code and constants).
- Numerically (our computation from those constants, view rotation 0): one world unit along the ground axes `x` or `z` moves the screen point by **41.835 px horizontally and 20.620 px vertically** at zoom 0 (half of 20.92/10.31 at zoom 1, a quarter at zoom 2). A tile of one world unit therefore spans a **83.67 × 41.24 px** diamond at zoom 0 (41.8 × 20.6, 20.9 × 10.3), which matches the 84 × 42 / 42 × 21 / 20 × 10 sprite measurements in `docs/render/iso.md` within rounding. One world unit up (`y`) moves the point **51.477 px up** at zoom 0 (25.74, 12.87). The `AnnoWelt` terrain builder uses one world unit per tile (`docs/re/annowelt.md`), so these numbers apply directly to tiles. **Confirmed** (arithmetic on confirmed constants); exact screen rounding (where 83.67 becomes 84) not traced.

## 6. Terrain triangles and screen tiles

- `tTexturesetTriangle` + `tTriangleGouraudZBufferRW<6/7/8>` (`0x10016ae0`, `0x10017000`, `0x10017460`): software rasteriser for screen-space triangles. Vertices are 6 floats (screen x, screen y, depth, then a packed word holding the world tile coordinates, then texture data); a byte of each vertex is used as a Gouraud light value (`<< 5`); depth is tested and written. The texture size template (64/128/256) is chosen from the texture's size (`SelectBrush`, type 0). **Confirmed** (structure); exact vertex field meaning beyond position, depth and light not traced.
- `0x1000e050` (picking): walks the stored triangle meshes, finds the triangle under a screen point with the smallest depth and interpolates the packed world coordinates — confirms that the ground is held as triangle meshes in screen space, not as tile sprites. **Confirmed.**
- `0x1000edd0` / `0x1000d680`: the world is rendered into a grid of 128 × 128 px screen tiles (`0x80`), each with its own item lists; `0x1000d680` draws a tile's lists and, for the `A` formats, sorts items by a float key at item `+0x24` in **descending** order (painter's order, far first given smaller = nearer) before drawing. **Confirmed** (sort and tile size); that the key is the item depth is **likely**.

## Open
- Which mode (A/B/C) each layer uses (static scenery vs. moving figures vs. UI); trace callers of `SetState(1|6, …)` (`tDrawTexture` slot `+0x6C`) in `AnnoFrame.dll`/`AnnoScene.dll`.
- Depth units: how the float depth passed to the blitters relates to the camera depth (`0.000384615` per unit) — needed to scale the per-pixel `z` offsets against neighbouring sprites. Trace the depth argument in `0x10011e60` → item `+0x20/+0x24` → `0x1000d3d0`.
- Meaning of the colour-table variant (`colour_param >> shift`, `0x200`-byte tables).
- Which world point the game passes for buildings, figures and ships (`AnnoWelt.dll` callers of the segment's add-object slot).
- How open water is drawn (terrain mesh excludes texture ids ≤ 10, see `annowelt.md`); candidates: `tHausAnimWasser` objects or a separate layer — **guess**.
