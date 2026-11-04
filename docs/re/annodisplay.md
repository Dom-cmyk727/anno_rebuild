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

Which mode each layer uses: §9.2 (static scenery mode C into the cached screen tiles; per frame the `0x163` coast pieces mode C and everything else mode B, depth-sorted).

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

`0x10011e60` (`tDrawSegmentDD2` slot `+0xBC`, add object): projects a world position `(x, y, z)` (float[3]) with the segment's projection (§5), asks the object for its frames for (zoom, rotation `(r + view_rotation) & 0xF`, frame), takes each frame's rectangle from the call above, and inserts a draw item into every 128 × 128 screen tile the rectangle overlaps. **Confirmed** (structure). The world point: for the start Kontor the footprint centre at the item's height (`h · 0.04`), solved from the depth cache (§9.2); for sea pieces the lattice point + the sprite set's prefix offset (§9.3). **Confirmed** (measured).

## 5. Projection (camera matrix)

- `0x1000c380`: projects a world point `(x, y, z)` with a 3 × 3 matrix plus offsets: `screen_x = m00·x + m10·y + m20·z + ox`, `screen_y = H − (m01·x + m11·y + m21·z + oy)`, `depth = m02·x + m12·y + m22·z + oz`. `0x1000fc30` (`tDrawSegmentDD2` slot `+0x74`, `SetMatrix(id, float[16])`): id 4 and 6 store 4 × 4 matrices; id 6 triggers `0x1000c150`, which precomputes per-pixel depth slopes. **Confirmed.** The matrix comes from the caller.
- Caller, **module `AnnoFrame.dll`**: `0x10003560` pushes `SetMatrix(4, …)` and `SetMatrix(6, …)`; the camera matrix is built by `0x10071d70` as `Ry(a) · Rx(b) · S`, matrix product `0x100713a0`, with
  - `0x100712c0`: rotation about the vertical world axis `y`, angle `a = view_rotation · 0x80 + 0xC0` in units of π/256 (`0x1008E168` = π, `0x1008E170` = 1/256) → 135° + 90° per view rotation;
  - `0x10071240`: rotation about `x`, angle `b = 0x2A` = 42 units = 29.53°;
  - `S = diag(s, s, 0.000384615, 1)`, `s = zoom · 59.163303` (`0x1008E1A0`), `zoom` = 1.0 / 0.5 / 0.25 for zoom levels 0 / 1 / 2 (`0x10003c30`); the depth scale is the immediate `0x39C9A634`.
  **Confirmed** (code and constants).
- Numerically (our computation from those constants, view rotation 0): one world unit along the ground axes `x` or `z` moves the screen point by **41.835 px horizontally and 20.620 px vertically** at zoom 0 (half of 20.92/10.31 at zoom 1, a quarter at zoom 2). A tile of one world unit therefore spans a **83.67 × 41.24 px** diamond at zoom 0 (41.8 × 20.6, 20.9 × 10.3), which matches the 84 × 42 / 42 × 21 / 20 × 10 sprite measurements in `docs/render/iso.md` within rounding. One world unit up (`y`) moves the point **51.477 px up** at zoom 0 (25.74, 12.87). The `AnnoWelt` terrain builder uses one world unit per tile (`docs/re/annowelt.md`), so these numbers apply directly to tiles. **Confirmed** (arithmetic on confirmed constants); exact screen rounding (where 83.67 becomes 84) not traced.
- **Handedness** (2026-11-03, re-read for `docs/render/iso.md` "Handedness"): `0x100712c0` writes `M[0][0] = c` (`[edx]`), `M[0][2] = −s` (`+8`, `fchs`), `M[2][0] = s` (`+0x20`), `M[2][2] = c` (`+0x28`), `M[1][1] = M[3][3] = ` the scale argument; `0x10071240` writes `M[1][1] = cb`, `M[1][2] = −sb` (`+0x18`), `M[2][1] = sb` (`+0x24`), `M[2][2] = cb`. The add-object projector `0x10011e60` computes the first screen coordinate as `M[0][0]·x + M[1][0]·y + M[2][0]·z + M[3][0]` from `+0x44`, `+0x54`, `+0x64`, `+0x74` (column 0 of the matrix at `+0x44`, row vectors). Column 0 of Ry · Rx is `(c, 0, s)` (Rx keeps the x axis), so with the world point (tile x, height, tile y) of `AnnoWelt.dll` screen x = k · (cos a · x + sin a · y): at a = 135° (view rotation 0), **+x goes to the screen's left and +y to its right** (both down), 41.835 px per unit. Each further view rotation turns this by 90° and keeps the handedness. A row-0 reading would give unequal x and y coefficients (no symmetric diamond), so the column reading is also the only one consistent with the 84 × 42 sprites. **Confirmed** (code).

## 6. Terrain triangles and screen tiles

- `tTexturesetTriangle` + `tTriangleGouraudZBufferRW<6/7/8>` (`0x10016ae0`, `0x10017000`, `0x10017460`): software rasteriser for screen-space triangles. Vertices are 6 floats (screen x, screen y, depth, then a packed word holding the world tile coordinates, then texture data); a byte of each vertex is used as a Gouraud light value (`<< 5`); depth is tested and written. The texture size template (64/128/256) is chosen from the texture's size (`SelectBrush`, type 0). **Confirmed** (structure); exact vertex field meaning beyond position, depth and light not traced.
- `0x1000e050` (picking): walks the stored triangle meshes, finds the triangle under a screen point with the smallest depth and interpolates the packed world coordinates — confirms that the ground is held as triangle meshes in screen space, not as tile sprites. **Confirmed.**
- `0x1000edd0` / `0x1000d680`: the world is rendered into a grid of 128 × 128 px screen tiles (`0x80`), each with its own item lists; `0x1000d680` draws a tile's lists and, for the `A` formats, sorts items by a float key at item `+0x24` in **descending** order (painter's order, far first given smaller = nearer) before drawing. **Confirmed** (sort and tile size); that the key is the item depth is **likely**.

## 7. Object kinds (`tHaus…` classes)
`0x10025d30` creates the display object for a sprite set from a kind number passed by the caller: 0 `tHaus`, 1 `tHausBaum`, 2 and 6 `tHausGebaude`, 3 `tHausShadow` (`0x10006be0`), 4 `tHausFigur`, 5 `tHausStrasse`, 7 `tHausShip`, 8 `tHausGelaende`, 9 `tHausAnim`, 10 `tHausAnimWasser` (a `tHausAnim` whose only override is class id `0x60A`, `0x10004c40`). **Confirmed.**

The kind is byte 2 of the 16-byte sprite-set prefix (`docs/formats/figuren.md`): `FigurenHeader.dat` sets have 4 (figures) and 7 (ships) among others; `Anlagen.dat` sets 0, 1, 2, 5, 8, 9, 10 (P, 2026-10-16: figures 1,233 × 4, 33 × 7; `Anlagen.dat` 14 × 10, exactly the water entries 2701–2709 and 2801–2802). **Likely** (the value sets match the factory's classes; the read of byte 2 into the factory call is not traced). Water entries and the sea lattice: `annowelt.md`, "Sea surface". Animated frame selection (`tHausAnim` slot `+0x6C`, `0x10004480`: rotation block × `ANIM` frames + frame mod frames; properties `0x136` frames → `+0x18`, `0x137` ms → `+0x1C`, `0x13F` rotation mask → `+0x1F`): `annowelt.md`, "Placed objects". **Confirmed.**

## 8. Colour tables and player colours (2026-10-17)
Found with `tools/ghidra/DecompileAllPlus.java`: the colour-table code is reached only through vtables, which auto-analysis missed.

- **Table kinds** (`tDisplay` vtable `0x1003e438`, slot `+0x60`, function `0x100260e0`; **confirmed**): 0 `tColorTable` (no data), 1 `Gouraud` (96 tables), 2 `GouraudMulti` (144), 3 `Effects` (3), 4 `EffectsMulti` (27). The kind comes from the loaders in `AnnoGame.dll` (`docs/re/annogame.md`): figures 1, or 2 for the player-coloured types; `Anlagen.dat` tables their `flag`, 0 → 3.
- **Player colours** (**confirmed**, data at `0x1003be70`): eight `0x00RRGGBB` words — (0, 187, 255), (255, 68, 0), (255, 244, 10), (0, 255, 176), (255, 138, 0), (47, 254, 41), (248, 57, 255), (255, 244, 222). Colour variant 0 leaves the palette as it is; variants 1–8 use these colours.
- **Green test and recolouring** (`0x10007e60` EffectsMulti, `0x10008980` GouraudMulti, same code; **confirmed** in the disassembly at `0x10007f75`–`0x10008016`): for every one of the 256 palette entries (R, G, B), with `t = trunc(1.5 · (R² + B²))` (x87, `f32` constant at `0x1003c048`): if `G² > t`, then `v = trunc(0.3333·R + 0.3333·G + 0.3333·B)` (`0x1003c044`), `w = min(trunc(3.0 · v), 255)` (`0x1003c040`) and each channel becomes `player_channel · w / 255` (integer division); otherwise the entry stays. So any colour whose green dominates is a marker, not only pure green: 48 entries in the archer palette, 30 in `Flagge01_01` (P, `crates/assets/tests/player_colours.rs`). No index is special; magenta fails the test; transparency comes only from RLE skips.
- **Light and effects** (**confirmed**): Effects tables scale (R, G, B) by a triple / 255 from `0x1003be90` (16-bit values): (255, 255, 255) normal, (160, 160, 170) dark, (367, 367, 367) bright, each channel clamped to 255. Gouraud tables scale by level / 24 (levels 0–31), GouraudMulti by level / 12 (levels 0–15).
- **To 16 bit** (**confirmed**): RGB565 `(R + 4) >> 3`, `(G + 2) >> 2`, `(B + 4) >> 3` with clamping; RGB555 uses `(G + 4) >> 3`. The original therefore shows every sprite colour quantised to 16 bit (OQ-030).
- **Selection at draw time**: `SetTexture` (`0x10014300`) takes the table base from the colour table's slot `+0x64` and a shift from slot `+0x70` (8 for Effects/EffectsMulti, 3 Gouraud, 4 GouraudMulti); a draw item's colour parameter (4th argument of add-object `0x10011e60`, item `+0xC`) selects table `param >> shift` (**confirmed** for the shifts, **likely** for the item field). The layout gives `param = colour << 8 | light`, light `0xC0` = neutral (`0xC0 >> 3` = 24 → 24/24, `0xC0 >> 4` = 12 → 12/12). `AnnoFrame.dll` `0x1004ea60` passes `(colour << 8) | 0xC0` with the colour from property `0xB` of the owner object, for figure kinds 97–128 (**confirmed** for that caller); that property `0xB` is the player's colour slot is **likely**.
- Open: how a player number becomes colour 1–8 (colour-selection screen `tFrameStartupEinzelFarbauswahl`), the in-game draw calls in `AnnoWelt.dll`/`View.dll` that compose the parameter, slots `+0x74`/`+0x78`.

## 9. Depth composition (W4-DEPTH-1, 2026-11-04)
Static reading (capstone, `tools/disasm.py`) plus live reads of the original in the Wine runner (GOG 2.0.0.5, display `:95`, silent, read-only `/proc/<pid>/mem`; Citizen start view, zoom groups 0/1/2, view rotations 0–3). The runs' screenshots and the dumped surfaces stay in git-ignored folders (`cache/orig/depth1/`, the session scratchpad); only numbers are recorded here. Labels as in §0: **confirmed** (code or measured state), **likely**, **guess**.

### 9.1 Depth units (closes the "depth units" item)
- Every depth blitter converts the float depth argument the same way: `fld [esp+…]`, `fmul [0x1003d7b8]` (= 2³² as `f32`), `__ftol` (`0x10030e10`), and keeps bits 16–31 of the low dword (`shr eax, 0x10`): `0x100182f8`–`0x100183e3` (`tBrushAlphaDepthZRW`), `0x10015f63`–`0x10015f73` (`tBrushRasterDepthR`). So **u16 depth = ⌊65536 · depth⌋ mod 65536**; the signed `z` byte is then added in 16 bits (`movsx cx, byte`; `add ecx, base`; `cmp word [buffer], cx; jb skip`, `0x1001848e`–`0x1001849a`), wrapping mod 65536. **Confirmed.**
- The float depth is the camera matrix's column 2: `depth = m02·x + m12·up + m22·z + oz` (§5). Live matrix (segment object, `tDrawSegmentDD2` vtable `0x1003cd10`, matrix at `+0x44`, rows = world x, up, z, offset): column 2 = (−0.00023663246, −0.00018957623, −0.00023663246), `oz` = 0.5, the same at zoom groups 0, 1, 2 and with the sign of the x/z terms turning with the view rotation (rotation 1: (+0.000236…, −0.000190…, −0.000236…)). **Confirmed** (measured). In u16 units: **15.508 per tile** of x or z, **0.4970 per `HEIGHT` unit** of the same world point, **2.0455 per `HEIGHT` unit at a fixed screen pixel** (a horizontal plane raised by one unit; 51.14 per world unit). `0.000384615 · 65536` = 25.21 per world unit along the view axis.
- Brush draw arguments (layer draw `0x1000d3d0`, the item list walk): `(&item+0x10 rect, item+0x24 depth, item+0x20, item+0xC colour)`. Item layout (add-object `0x10011e60`, nodes of 0x28 bytes from the pool at segment `+0x201a00`, `0x10013b70` allocates blocks of 20): `+0`/`+4` links, `+8` frame texture, `+0xC` colour parameter, `+0x10` rect (x0, y0, x1, y1), `+0x20` = projected screen y − y0 (`f32`, the anchor row with its fraction), `+0x24` = the projected depth (`f32`). The `Z`-format blitters do not read the third argument (`0x10015f60`, `0x100182f0`). **Confirmed.**

### 9.2 Screen tiles, the cache and the two passes (closes "which mode per layer")
- The view is drawn in 128 × 128 screen tiles. Each tile record (segment `+0x35c + 0x94 · (col + 14 · row)`) holds three surfaces: `+0x54` the **colour cache** (RGB565), `+0x58` the **depth cache** (u16), `+0x44` a **per-frame depth copy**; and item lists: `+0x64/+0x68` (tail/head; add slot `+0xBC`, `0x10011e60`), `+0x6C` (add slot `+0xC0`, `0x100123c0`), `+0x74` (the shadows of both), `+0x7C` (triangle meshes, add slot `+0xA0`, `0x10012e00`), `+0x84` lines, `+0x8C` labels. The brush's `+0x24` is the depth buffer and `+0x20` the colour target (row pointers `0x100183ab`–`0x100183bb`; the compare reads `[ebp+0x24]`'s rows). **Confirmed** (code and live pointers).
- **Cache rebuild** (`0x1000fdf0`, per dirty tile): depth cache cleared to `0xFFFF` (`0x1000feb9`–`0x1000fec4`), `SetState(1, 1)` (`0x10010005`), then in this order: list `+0x7C` = terrain triangles (`0x1000d470`, triangle brush slot 0), list `+0x64` = static objects (`0x1000d3d0`, walked tail → `+4`, i.e. in reverse insertion order, unsorted), labels `+0x8C` (`0x1000d530`), shadows `+0x74` (`0x1000d420`). The dumped depth cache holds the terrain, trees and the start Kontor with their per-pixel `z` (D = base + z on 80.5 % of a 600-pixel sample of the Kontor's opaque pixels, the rest covered or blended), so static objects **write** depth in this pass: **confirmed** (measured). Which call sets `SetState(6, 1)` before it was not located (likely the frame-start reset `0x100169b0`).
- **Per-frame pass** (`0x100103c0`, every visible tile, every frame): the tile's depth copy and colour cache are locked, `SetState(1, 1)` and `SetState(6, 0)` (`0x10010568`–`0x1001057b`), then `0x1000df50` (shadows from the colour cache, `2Screen`), `0x1000d680` and `0x1000dfc0` (lines). `0x1000d680`: `SetState(6, 1)` (`0x1000d699`), list `+0x6C` drawn in insertion-reverse order **with depth write**; `SetState(1, (flags & 6) != 2)`, `SetState(6, 0)` (`0x1000d74d`–`0x1000d76b`); list `+0x64` collected into an array of (`item+0x24`, item) and quicksorted **descending by depth** (`0x1000d7b6`–`0x1000d855`, far first), then drawn **test-only**; tiles holding `A` formats (mask `0x310`) take a longer path with `SetState(9, …)`. **Confirmed.** The colour cache of a live dump has no sea, ship or flag; the per-frame depth copy has the ship. So moving and animated things are drawn every frame over the cached tiles, depth-tested against the cached terrain and static objects.
- **Which objects go where** (`AnnoWelt.dll` chunk draw `0x1009f870`, `0x1009fb11`–`0x1009fc5d`): the object type's u16 at `+0x10` (`AnnoGame.dll` getter `0x10007d20`, slot `+0x5C`) equal to `0x163` → add slot `+0xC0` (write list), anything else → `+0xBC`. In the running game (type table at world `+0x110`, entries at `+0x214 + 4 · id`, getter `AnnoGame` `0x100037e0`), `0x163` holds exactly entries 2702, 2703, 2704, 2708, 2709 (the coast pieces: mouths, surf `Brandung01`, waves, spray); rivers 2701, 2705–2707 are `0x161`; the sea 2801/2802 `0x162`; the Kontor 1302 `0x23`. **Confirmed** (code and live values). Which pass draws which kind of object (static cache vs. per frame) is decided by the caller's pass flags (`0x1009f870` argument, bits 2/4/8 select the sea `0x1009f4c0`, `0x1009b310` and the placed objects); the mapping of flags to passes is **likely** from the dumps (static: terrain, trees, buildings; per frame: sea, rivers, coast pieces, ships, flags), not traced.
- **Ships.** The ship is in the per-frame depth copy (it writes depth there) and the sea, drawn after it test-only, covers its hull below the water plane (the rudder and keel under the checkerboard in the screenshot). The ship's anchor solves to world height 1.01–1.10 (25–28 `HEIGHT`, bobbing) at tile + (0.60, 0.52). **Confirmed** (measured); the add slot ships use is not traced.

### 9.3 The sea's depth (closes OQ-026 b)
- Sea items (live pool, 49 distinct items at zoom 0, 122 at zoom 1): the anchor's world point solves exactly (residual < 10⁻⁵ world units at zoom 0) to `(tile x + 1.5, 0.13 + 0.01 · (x mod 5) − 0.01357 · (z mod 3), tile z + 1.5)`: the 1.28 of `0x100e3a20` (live value 1.28) plus the 2801 sprite set's prefix float **−1.15** (`HAEUSER/ENTRY` prefix `+8`, `33 33 93 bf`; surf 2708 has +0.34, the Kontor 0) and the documented per-sprite wobble. **Confirmed.** The prefix offset is applied by the type's display-piece query (slot `+0xA4`, `0x1009f650`), not traced inside.
- The sea frames (`BSH_RGBWZ`, 254 × 144 at zoom 0) carry z ≈ −47 at the anchor (−46.9, −47.3, −48.1 at zoom groups 0/1/2, a plane within ±4), i.e. the water surface lies 23 `HEIGHT` above the anchor point: **effective sea plane ≈ 26.4–26.6 `HEIGHT`** (3.25 + wobble + 23). Measured from the dumps (the sea is drawn on the screen pixels with (x + y) even, over every cached surface pixel whose height lies below the plane): the height that best separates covered from uncovered pixels is **26.75 / 27.0 / 26.75 / 26.25** at zoom 0, view rotations 0/1/2/3, **27.0** at zoom 1 and **28.25** at zoom 2 (2.6 %, 2.0 %, 1.7 %, 0.6 %, 2.6 %, 1.9 % misclassified; the remainder is the frames' ripple). **Confirmed** (measured). So the original's water covers the terrain up to about 27, not 16: the painted sand between 16 and 27 lies under the checkerboard water.
- The coast pieces (`0x163`) write depth before the sorted list, so the sea is also tested against them; rivers (`0x161`) are test-only (sorted list). **Confirmed** (code); their effect in a picture not measured.

## Open
- Meaning of the colour-table variant (`colour_param >> shift`, `0x200`-byte tables).
- Which add slot ships and figures use, and the pass flags of `0x1009f870` per pass (§9.2).
- Which call enables depth writing for the cache rebuild (§9.2).
- Open water: what lies below texture-0 tiles: the dumped colour cache shows a blue floor there with the depth left at `0xFFFF` (no terrain triangles), drawn by an unidentified item.
