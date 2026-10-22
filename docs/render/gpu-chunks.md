# GPU-built chunks: the sprite passes on the GPU (T11)

**Status 2026-10-22 b: stage 2a built incl. the review follow-ups (the exact binned compute pass for objects and base; the residency-driven chunk atlas sharing the OQ-096 budget with the sprite atlas; dirty-only frame/keel upload and pooled scratch; the eager pipeline/atlas create; the derived-level `reduce_quad` wiring over GPU children; the atlas-overflow CPU fallback; the `GpuList` builder behind `ANNO_VIEW_GPU_CHUNKS` / `--chunk-build`; section 11 "Stage 2a as built"). Stage 1 built before it. Remaining in 2a: the isolated per-chunk GPU-dispatch timestamp (the `bin_px` frame-level sweep and the iGPU pan/zoom matrix are measured, `docs/perf/README.md`).** Written for task T11 (`docs/orchestration/TASKS.md`), owner decision 2026-10-20 "tier 3 now". Companion to [`iso.md`](iso.md) ("Objects", "Animation and layers", "Fidelity", "Stand-in chunks") and [`../re/annodisplay.md`](../re/annodisplay.md). Once a stage lands, the matching rows of `iso.md` are updated and this page keeps the record formats, the residency rules and the test contract.

Evidence tags as in [`rivers.md`](rivers.md): **M** measured (here: counted with a script over the original files, section 2), **P** parsed from the original files, **D** disassembly, **C** code read at `1900132` (file:line), **E** estimate (to be replaced by a measurement in the stage named), **T** tuning value, **?** open question (section 12).

Correction to the task text: `iso.md` has no section "Blitting". The blitting rules this design reproduces are `iso.md` "Objects" (lines 19-27), "Fidelity" (209-219) and the CPU blitter itself (`iso.hpp:238-282` `blit_sprite`, `iso.cpp:15-33` `over`); the original's brushes are `annodisplay.md` §3.

## 0. Summary

| | Today | With T11 |
|---|---|---|
| Who makes a chunk's `ground`, `objects`, `base` planes | a `ChunkWorkers` thread composites them on the CPU (`frame.cpp:162-246`), the window thread uploads 3.9 MB of texels per chunk (`gpu.cpp:699-819`) | the worker builds a **draw list** (fixed-width records, section 3); the window thread uploads the list (≤ 115 KB, M) and records a GPU pass that writes the same planes |
| What stays on the CPU for now | | `field`, `terrain` (until the ground pass, 5.3), `flow`, the culling and the painter's order |
| Sprite frames on the GPU | the atlas grows one layer at a time and never evicts (`gpu.cpp:929-959`); frames are decoded on the window thread on first use (`gpu.cpp:978-999`) | a scene-wide residency with an MB budget and layer-granular LRU (section 4); decoding on the workers |
| Downstream | | unchanged: `ChunkKey`, the `serial` rule, the compositor passes, stand-ins, prefetch, dissolve all see "a chunk is textures on the GPU" |
| CPU path, `--png`, digests, `anno-dump` | CPU compositing | unchanged; the CPU builder becomes "build the list, replay it with `blit_sprite`" so both builders consume the same list (stage 1 proves it bit for bit) |

Main design decision (section 5, **? C1**): the objects and base planes are built by a **binned compute pass that repeats the CPU's integer `over` exactly**, not by a blending render pass. A blending pass cannot match the CPU within 1 per channel where translucent pixels and shadows compound (the CPU floors per step, fixed-function blending rounds; the error grows with each layer), and it cannot repeat the per-island local canvas and its `base` reset rule. The ground plane (opaque, last writer wins) is a render pass.

## 1. What exists (C)

| Piece | Where | Relevant facts |
|---|---|---|
| Chunk | `frame.hpp:40` `kChunk = 512`; `frame.hpp:47-79` `struct Chunk` | planes `ground` (0RGB u32), `objects` (straight-alpha RGBA), `field` (RGBA8), `terrain` (u16, 1/64 HEIGHT), `base` (u8), `flow` lattice; `serial` identifies a build |
| Chunk build | `frame.cpp:162-246` `render_chunk_data` | ground = `Scene::ground_with_probe` over the water colour (192-198); objects = `Scene::render(rect, group, objects_only)` (199-205); field from the probe (206-222); terrain from the probe (223-231); flow (238-244) |
| Workers | `frame.hpp:99-135`, `frame.cpp:304-350` | priority list rewritten every frame (`submit`), results via `take_done`, frees on a reaper thread |
| Objects composition | `scene.cpp:763-880` `render_with_terrain` | per island a **local canvas** = island bounds ∩ chunk rect (797), objects drawn into it, then `composite` onto the chunk canvas (843). Sprites are clipped to the island's rectangle |
| `composite` | `scene.cpp:94-122` | `over` per pixel; `base` copied from the local canvas where the local pixel's alpha ≠ 0 (a later island's shadow-only pixel resets `base` to 255) |
| Painter's order | `iso.hpp:287-292` `prepare_objects`, `scene.cpp:896-933` `draw_island_objects` | per island by `(x + w + y + h, x)`, stable; soldiers interleaved by their 1 × 1 key; each object's shadow before it (`iso.hpp:599-613`); islands in layout order |
| Placement | `iso.hpp:553-613` `place_prepared` | anchor from the footprint centre and the height byte; frame = `rotation · anim + (t / ms + phase) mod anim`, static layer at t = 0 |
| Blitter (the oracle) | `iso.hpp:238-282` `blit_sprite`, `iso.cpp:15-33` `over`, `iso.cpp:58-62` `put` | decoded RGBA image, straight-alpha integer `over` with **floor** division; shadow pixels become `kShadow` (0, 0, 0, 96) (`iso.hpp:91`); `base` = rows above the column's lowest opaque row, 255 for columns that do not reach `anchor_y − base_band` |
| What the decoded image carries | `assets/figure.hpp:31` `kLevelAlpha = {64, 128, 191, 255}`; `iso.md:213` | alpha levels of `A` formats, the `WZ` checkerboard (alpha 0 columns), rows past the frame width (image widened, OQ-025); signed `i16` anchors (`annodisplay.md` §4) |
| What the blitter does **not** do | `iso.md:5`, `iso.md:23`, `annodisplay.md` §3 | per-pixel `z` depth, `M` melt, `H`/`V` depth-write differences: the renderer is a painter's approximation (OQ-026). T11 reproduces the blitter, not the original's depth brushes (record field reserved, 3.1) |
| Ground | `iso.cpp:134-173` `raster_triangle`, `iso.cpp:177-246` `draw_ground_grids` | two triangles per tile split along B-C; f32 edge functions with tolerance `−1e-3 · area`; pixel centres; texel `cell.texel(q(s), q(t), 4096)` from 147 textures of 256² (P: `anno-dump tex Felder.tex`); tiles back to front by `(x + y, x)`, later pixels overwrite (opaque) |
| GPU atlas | `gpu.cpp:496-510` `Atlas`, `893-924` `new_atlas`, `929-959` `atlas_alloc`, `963-976` `pack_image` | RGBA8Unorm 2D array, `GpuLimits::atlas_size` 2048 (`gpu.hpp:148`), shelf packing, grows by one layer and copies all old layers, no eviction; full = sprites dropped with one message |
| GPU sprites | `gpu.cpp:36` `kInstanceBytes = 32`; `gpu.cpp:1065-1087` `pack_sprites`; `gpu.cpp:1353-1365` blend and vertex layout; `gpu.wgsl:837-863` `sprite_vs`, `984-1013` `sprite_fs` | instance = dst (i32 × 2), size, atlas xy, layer, flags (bit 0 shadow, 8-15 height, 16 depth-tested, 17 ship); straight-alpha `SrcAlpha / OneMinusSrcAlpha` blending; used per frame for water objects and ships only |
| GPU chunk | `gpu.cpp:312-331` `Plane`, `Chunk`; `gpu.cpp:384-391` the five plane formats | BGRA8 ground, RGBA8 objects, RGBA8Uint field, R16Uint terrain, R8Uint base; usages TextureBinding, CopyDst, CopySrc (`gpu.cpp:735-737`, `445-446`): **no RenderAttachment** |
| Upload | `gpu.cpp:825-891` `sync_chunks` | per frame at most `uploads_per_frame` (6) within `upload_budget_ms` (1.0 interactive), the first always goes; placeholders for missing visible chunks; plane pool and pre-created planes |

## 2. Measured numbers (M)

**Method.** A counting script (`count_frames.py`, scratch, not committed) uses the Python references `tools/insel.py` and `tools/figuren.py` and repeats `prepare_objects` (dedup of identical items) and `place_prepared` (set 0, the frame set of the group, frame index at t = 0 with the item's phase, surf phase `(3y + 5x) mod 32`, the shadow frame with the same index). "Static" = objects that are not water objects (kind byte 10), the chunk objects layer. "Water" = water objects, every animation frame of the item's rotation block (drawn per frame by the compositor, not baked). Bytes = frame box `w × h × 4` (the atlas texel bytes, `gpu.cpp:973`); covered pixels = sum of the RLE run lengths. Per chunk: distinct static frames and instances whose frame box (plus the 16 px overrun slack, `iso.hpp:314`) touches a 512 px chunk; positions from `tile_top` and `Lift::scaled` (`iso.cpp:78-87`). Check: `Single_01.sze` gives 11,717 invisible items, as `iso.md:207`; 69,864 drawable items against 69,256 drawn there (0.9 % apart; the script does not drop items whose frame index or group set is missing). Original-water variants only: enhanced water adds a 2 × 2 smoothed copy of each non-shadow water frame (`iso.hpp:205-207`), up to doubling the water column. Linked islands (`INSELLNK`, the endless scenarios) are not counted.

| Scene | Group | Static frames | Static MiB | Water frames | Water MiB | Union MiB | Static instances (whole scene) | Frames per chunk median / max | MiB per chunk median / max | Instances per chunk median / max |
|---|---|---|---|---|---|---|---|---|---|---|
| `Single_24.sze` (2 islands) | 0 | 509 | 46.2 | 192 | 26.6 | 72.8 | 31,751 | 40 / 87 | 1.0 / 5.8 | 142 / 296 |
| | 1 | 509 | 11.6 | 192 | 6.7 | 18.2 | 31,751 | 56 / 151 | 0.4 / 2.6 | 364 / 869 |
| | 2 | 509 | 2.8 | 192 | 1.6 | 4.5 | 31,751 | 86 / 241 | 0.1 / 1.1 | 685 / 2,786 |
| `D_256x256_0045_5B.scp` (canyon) | 0 | 329 | 18.0 | 392 | 60.0 | 78.0 | 29,278 | 55 / 99 | 2.1 / 6.8 | 128 / 277 |
| | 1 | 329 | 4.5 | 392 | 15.1 | 19.6 | 29,278 | 92 / 158 | 0.9 / 2.3 | 409 / 865 |
| | 2 | 329 | 1.1 | 392 | 3.7 | 4.8 | 29,278 | 121 / 204 | 0.3 / 0.7 | 1,062 / 2,603 |
| `Single_01.sze` (15 islands) | 0 | 675 | 37.6 | 288 | 44.2 | 81.7 | 101,152 | 38 / 108 | 1.1 / 6.6 | 102 / 319 |
| | 1 | 675 | 9.4 | 288 | 11.0 | 20.5 | 101,152 | 55 / 161 | 0.4 / 2.2 | 218 / 971 |
| | 2 | 675 | 2.3 | 288 | 2.8 | 5.0 | 101,152 | 68 / 224 | 0.1 / 0.7 | 402 / 3,542 |

Over all 31 scenarios of `0200Single` and `0201SingleAddOn` at group 0: union median 59.8 MiB, maximum 149.9 MiB (`single30.sze`, 26 islands: 840 static + 632 water frames; all three groups together 196.7 MiB), most static frames 1,464 (`single36.sze`, 116.0 MiB); at most 129 distinct frames and 7.8 MiB of frames in one chunk; at most 3,608 instances in one chunk (`single36.sze`, group 2). The static covered pixels of a scene are 0.4-16 M per group, a few percent of the 78 M of all building frames at group 0 (`iso.md:212`).

Consequences used below: (1) the whole frame set of any original scenario, all groups, fits in about 200 MiB (330 MiB with enhanced water's smoothed copies, E): an LRU is not needed for the original content at the proposed budgets, it is needed for mods, larger maps and the moving figures (section 10: 241,808 figure frames per group, 145.6 M covered pixels at group 0, `iso.md:212`); (2) one chunk's frames always fit in one frame's upload budget (7.8 MiB < 16 MiB); (3) a chunk's list is at most 3,608 × 32 B = 115 KB, against 3.9 MB of texels uploaded per chunk today.

## 3. (a) The draw list

A chunk built for the GPU carries, instead of `ground`, `objects` and `base` pixels, a `ChunkDraws` value made by the worker. All records are fixed width, little-endian, written field by field with the `Bytes` writer pattern (`gpu.cpp:41-54`), never struct dumps (CLAUDE.md). Ids are `u32` indices into tables whose size is data (PLAN 0.1); positions are `i32` scene pixels.

### 3.1 Objects record (32 bytes, one per sprite draw)

| Offset | Type | Field | Meaning |
|---|---|---|---|
| 0 | i32 | `x` | scene x of the frame box's left edge = anchor x − frame `anchor_x` (signed; `SpriteDraw::rect`, `iso.hpp:304`) |
| 4 | i32 | `y` | scene y of the top edge = anchor y − `anchor_y` |
| 8 | u32 | `frame` | scene frame id (4.1): selects the decoded image (its size is the image's, which may be wider than the frame box, OQ-025), `anchor_y` and the column keel table |
| 12 | u32 | `flags` | bit 0 shadow (the pixel becomes `kShadow`, no `base` write); bit 1 tracks `base` (non-shadow, chunk built with bases, `scene.cpp:771-774`); bits 8-15 the object's height byte (as `pack_sprites`, kept for the depth-tested water case and debugging); other bits 0 |
| 16 | u32 | `clip` | index into the chunk's clip table (3.3): the island segment the draw belongs to |
| 20 | u32 | `colour` | colour variant; 0 = the decoded image as is. Reserved for an indexed atlas with per-instance colour tables (section 10); player colours are part of the frame today (`iso.md:205`: recoloured at decode) |
| 24 | i32 | `depth` | reserved, 0: the original's sprite depth for a later per-pixel depth test (`annodisplay.md` §3, OQ-026); unused by T11 |
| 28 | u32 | — | 0 (pads to 32 = `kInstanceBytes`, the existing sprite stride, `gpu.cpp:36`) |

**Order is the list order.** The worker emits records in exactly the order the CPU blits: islands in layout order, per island `place_prepared` over `objects.other` (shadow before its object), soldiers merged in by `draw_island_objects`' rule. No GPU-side sort: the painter's order key is the record index, and stage 1 proves the list by replaying it (7.1). The water/above-water split stays as today: water objects are not in the chunk list (`Objects::NotWater`); the compositor draws them per frame (`gpu.cpp:1517-1522`), and the chunk keeps `water_below` / `water_above`.

### 3.2 Ground record (32 bytes, one per tile that can touch the chunk)

| Offset | Type | Field | Meaning |
|---|---|---|---|
| 0 | i32 | `ox` | scene x of the tile's top vertex at height 0 (island offset + `tile_top`, `iso.cpp:78-81`) |
| 4 | i32 | `oy` | scene y of the same point |
| 8 | u32 | `texture` | layer in the ground texture array (147 textures of 256², P; 0 = open water) |
| 12 | u32 | `cell` | the `GroundCell` bits (u, v, rotation `r`; `insel.md` "Ground", lines 61-71) widened |
| 16 | u32 | `h01` | corner heights A (x, y) and C (x+1, y), u16 each (HEIGHT units; `iso.hpp:121-124` `corners`, clamped at the island edge) |
| 20 | u32 | `h23` | corner heights B (x, y+1) and D (x+1, y+1) |
| 24 | u32 | `clip` | clip table index (the island) |
| 28 | u32 | — | 0 |

Order: islands in layout order, tiles in `draw_ground_grids`' order `(x + y, x)` (`iso.cpp:204-212`). The record holds what the rasteriser needs and no more: the lift per unit and the tile size are frame-wide uniforms of the group.

### 3.3 Per-chunk tables

- **Clip table**: one rectangle per island segment, `{x0, y0, x1, y1}` i32 = island bounds ∩ chunk rect (the CPU's local canvas, `scene.cpp:797`). A pixel outside its record's clip rectangle is not drawn (the CPU clips there). Ships are not in GPU chunks (`FrameRenderer(…, false)`), so there is no unclipped segment.
- **Bins** (objects only, for the compute pass of 5.1): the chunk is cut into bins of `bin_px` × `bin_px` pixels (default 32, T: 256 bins per chunk); per bin the indices of the records whose box (image size, not frame size) overlaps it, in list order, as CSR (`offsets[bins + 1]`, `indices[]`, u32). Built on the worker. Size E: instances × bins touched; the largest frame (682 × 651 at group 0, P) touches 22 × 21 bins, typical lists 10-40 KB per chunk.
- **Frame set**: the sorted distinct frame ids of the list (residency, 4.3).

## 4. (b) Atlas residency

### 4.1 Frame ids and the frame table (CPU, per scene)

- A scene-wide table interns every frame a draw list or the compositor references: key = `SpriteKey` (`iso.hpp:204`: Anlagen id, variant, shadow, group, index) for buildings, and a stable unit-frame key (figure entry, set, group, frame, colour) for soldiers (today units are keyed by `const Rgba*`, `gpu.cpp:504-506`, which is not stable across scenes and not a residency key). Value: frame header data (`anchor_y`, box size), the decoded image (`shared_ptr`, from `SpriteCache`, decoded **on the worker** that first references it; today decoding happens on the window thread, `gpu.cpp:987`), and the per-column keel row (lowest opaque row per column, as `blit_sprite` computes per blit, `iso.hpp:249-257`), computed once with the image.
- Interning is a read-mostly map behind a shared mutex, as `SpriteCache` (`iso.hpp:214-224`); ids are dense `u32`.

### 4.2 GPU side

- **Frame table buffer** (storage, 32 bytes per frame id): resident flag, layer, atlas x, y, image w, h, `anchor_y`, keel offset. Non-resident = flag 0. Lists never change when frames move: they name ids, not atlas places.
- **Keel buffer** (storage, u16 per column of each resident frame; the largest frame is 682 columns).
- **Atlas**: the existing RGBA8Unorm array texture, layers of `atlas_size`² (16 MiB at 2048). Two allocation classes in separate layers: **hot** (water-object animation frames, later moving units: used every frame) and **static** (chunk frames: used only while a chunk pass is recorded). This keeps the LRU of the static class clean.
- **Growth**: by doubling the layer count up to the budget (one GPU copy of the old layers per doubling, `new_atlas`'s copy, `gpu.cpp:900-913`), instead of one layer at a time (which copies all layers at every step).

### 4.3 Rules

1. **Lazy per scene, like a glyph cache.** A frame is uploaded when a chunk scheduled for its pass needs it (4.4) or a compositor sprite draws it. After a scene loads, quiet frames upload the static frame set of the current group in the background (≤ 150 MiB, section 2), so that cold chunk builds rarely wait for frames.
2. **Never with holes.** A chunk's pass is recorded only when every frame of its frame set is resident. While frames are missing, the chunk keeps its placeholder or stand-in (the existing path) and its frames are uploaded first.
3. **LRU at layer granularity.** Each layer has `last_used` = the frame counter of the last pass or sprite draw that read a frame in it. When an upload does not fit the budget, the least recently used layer of the class that is not pinned is emptied: its frames are marked non-resident in the frame table, its shelf cursor reset. No repacking, no copies.
4. **Pinning.** Layers holding frames of chunk passes recorded this frame and of this frame's sprite draws are pinned for the frame. A baked chunk does not pin anything: once its planes are written, its frames are only needed again for a rebuild.
5. **Budget overflow.** If a single chunk's frame set does not fit the budget after evicting every unpinned static layer (cannot happen at the defaults: 7.8 MiB per chunk at most, M), that chunk is built on the CPU (section 8) and a counter (`GpuStats::chunks_cpu_fallback`) says so.
6. **Budget**: `GpuLimits::atlas_budget_mb` (section 9). Default from the adapter: 512 MiB on a discrete adapter, 256 MiB otherwise, clamped to `maxTextureArrayLayers · atlas_size² · 4` and to `maxBufferSize`. Reason: WebGPU reports no memory size; the largest original scenario needs 197 MiB for all groups (330 MiB E with enhanced water's smoothed copies), so the discrete default never evicts original content and the integrated default only evicts other groups' frames. **? C3** (a limit default: owner).

### 4.4 Where the bytes go per frame

The window thread writes decoded images into the atlas within `atlas_upload_bytes_per_frame` (16 MiB, the same budget as the wave window's `upload_bytes_per_frame`, `gpu_fluid.hpp:63`) and the shared `upload_budget_ms`; the first frame of a waiting visible chunk always goes (the rule of `sync_chunks`, `gpu.cpp:858-862`). Visible chunks' frames first, then prefetch chunks', then the background warm-up.

## 5. (c) The passes

All chunk passes are recorded into the frame's own command encoder before the compositor's passes (`gpu.cpp:1540`), so a chunk built in frame N is drawn in frame N. The compositor is unchanged: it binds the chunk's planes as today (`upload_chunk`'s bind group, `gpu.cpp:801-815`).

### 5.1 Objects and base: a binned compute pass (recommended, **? C1**)

One dispatch per chunk, one invocation per pixel (workgroups of 8 × 8). Each invocation walks its bin's record list in order and repeats the CPU exactly:

```
acc = (0,0,0,0); base = 255                      // the chunk canvas (blank, scene.cpp:770)
loc = (0,0,0,0); lbase = 255; seg = none         // the island's local canvas
for i in bin(p):
    r = record[i]
    if r.clip != seg:                            // next island: composite the local canvas
        if seg != none: composite(acc, base, loc, lbase)
        loc = 0; lbase = 255; seg = r.clip
    if p outside clip[r.clip] or outside r's image box: continue
    t = atlas texel of r.frame at p - (r.x, r.y); if t.a == 0: continue
    over_u32(loc, r.shadow ? (0,0,0,96) : t)     // iso.cpp:15-33, integer, floor division
    if r.track_base: lbase = keel_contact(r.frame, column) ? min(keel - row, 254) : 255
composite(acc, base, loc, lbase)                  // over + base copy where loc.a != 0 (scene.cpp:94-122)
write acc, base
```

- **Exact.** `over` in `u32` (the largest product, 255³, fits), the same floor divisions, the atlas texel converted back with `round(v · 255)` (exact for unorm8). The result is the CPU's bit for bit: tolerance 0 on `objects` and `base`.
- **Why not the blending pass the task names.** With `SrcAlpha / OneMinusSrcAlpha` (or premultiplied `One / OneMinusSrcAlpha`), each blend rounds where the CPU floors, and the stored 8-bit value is rounded again; one translucent layer stays within 1, but compounding shadows and shadows over translucent sprites (alpha 64/128/191, `assets/figure.hpp:31`) drift by up to 1 per layer, and a straight-alpha objects plane over a transparent pixel needs a premultiplied target plus an unpremultiply in `objects_fs` (error up to 255 / (2a) per channel at low alpha before compositing). It also cannot repeat the local canvas per island (the clipping is a scissor, but the `composite` step and the `base` reset of a later island's shadow-only pixels need an intermediate target per island). Both points are what the synthetic scene tests (7.3). The blending pass stays the fallback design if C1 is decided the other way; it would need the tolerance written as "≤ number of translucent layers" on the objects plane.
- **Writes.** The pass writes two storage buffers (RGBA8 packed in `u32`, 1 MiB; `base` u8 packed four per `u32`, 256 KiB) that are then copied into the existing `RGBA8Unorm` objects plane and `R8Uint` base plane (`wgpuCommandEncoderCopyBufferToTexture`, rows of 2048 and 512 bytes, both multiples of 256). `R8Uint` is not a storage-texture format in core WebGPU, and copying keeps the plane formats and the compositor's shader unchanged. Scratch buffers are pooled per chunk in flight.
- **Cost E.** 262,144 invocations per chunk, each walking 10-40 records (overdraw at group 2: 3,608 instances × about 400 px box area / 262,144 px ≈ 5.5 per pixel, bins add the box-but-not-pixel cases). Measured in stage 2 with the GPU timestamps (`GpuLimits::gpu_timing`) on the RTX 4090 and the Radeon iGPU.

### 5.2 Ground (and terrain): a render pass

- One instanced draw per chunk, one instance per ground record, into the ground plane (BGRA8) and, as a second target, the terrain plane (R16Uint), in a render pass whose load op clears the ground to the water colour (`frame.cpp:191-198`: ground over `scene.textures.water()`) and terrain to `kNoTerrain`. The scissor is the chunk; per-island clipping is a per-instance test (the clip table).
- **Coverage the CPU's way.** The vertex shader draws the tile's screen bounding box (both triangles); the fragment shader evaluates the CPU's two triangles with its f32 edge functions, tolerance and order (`iso.cpp:134-173`, A-B-C then C-B-D, the later one wins), discards outside both, interpolates `(s, t)` as the CPU, picks the texel with the same `q()` and `GroundCell::texel` rule, `textureLoad` from the ground array (147 × 256² × 4 = 36.75 MiB, resident for the scene). Later tiles overwrite earlier ones by primitive order (opaque, no blending). The terrain output is `round(height · 64)` from the corner heights interpolated over the triangle, as `Scene::terrain_at` (`frame.cpp:223-231`).
- **Exactness.** The CPU path builds with `/fp:strict` / `-ffp-contract=off` (CLAUDE.md); whether the shader compiler contracts `a * b + c` into an FMA in WGSL is not under our control (**? C5**). Expected: identical except pixels whose edge function or texel coordinate lies within an f32 rounding of a boundary. Tolerance proposal: ground within 1 per channel on all but a counted, reported number of edge pixels (target 0), terrain within 1 (1/64 unit).
- **Dependency (? C2).** The CPU still needs the ground probe for the water field (`frame.cpp:192`, `206`), so until the field stops being computed per chunk from the probe (T1f item 2: per island and group, sliced), moving the ground to the GPU saves only the texel fetches and writes, not the rasterisation. Proposed order: objects first (stage 2a); ground after T1f item 2, or earlier if the T1f profile shows the ground stage large on its own.

### 5.3 Planes and usages

The ground and terrain planes need `RenderAttachment` usage, the objects and base planes `CopyDst` (already set). The pool and the pre-created planes (`gpu.cpp:445-446`, `735-737`) get `RenderAttachment` added for the two formats. The `field` plane and the flow lattice are uploaded as today.

## 6. (d) Scheduling

- **Worker** (`ChunkWorkers::run`, `frame.cpp:304-337`): with the GPU builder, `render_chunk` produces the field, terrain (until 5.2 lands), flow, `water_below/above`, and the `ChunkDraws` (records, clip table, bins, frame set), and decodes the frames of the set that are not decoded yet. No pixels of the objects layer. E: culling and `place_prepared` over an island's objects plus binning, well under a millisecond per chunk (measured in stage 1 with the T1f stage timers).
- **Window thread** (`GpuFrame::render` → `sync_chunks`): for each key that needs an upload (the existing order: visible, then prefetch), if the CPU chunk carries draws: (1) upload missing frames of its set within the frame's atlas byte and time budget; (2) when its set is resident and the per-frame chunk budget allows, take planes (pool, pre-created, or new), write its records, bins and clip table into a per-frame staging buffer, and record the compute pass, the copies and the ground pass into the frame's encoder; (3) the chunk becomes `real` with the CPU chunk's `serial`. Nothing waits: no fences, no map, no read-back.
- **Budget.** `GpuLimits::gpu_chunks_per_frame` (default 6, the same as `uploads_per_frame`: the existing bound on how many chunks change per frame) and the existing `upload_budget_ms` (the CPU time of steps 1-2; the first chunk always goes). GPU time per chunk is measured in stage 2; if it is large on the iGPU, a `gpu_chunk_budget_ms` from the timestamps of earlier frames bounds it (**E**, only if needed).
- **Memory traffic E.** Per chunk 32-140 KB of records, bins and tables instead of 3.9 MB of texels; atlas uploads only on first use. The CPU cache holds lists instead of pixels (about 1/10 of a land chunk's bytes), which also lowers T1e item 8's memory bound for this builder.
- **Latency target.** T11's acceptance (a visible chunk on screen within 20 ms of its request on the 4090) needs the field and flow stages small too: T11 removes the sprite stages, T1f item 2 the field stages. Stage 3 measures both together.

## 7. (e) The CPU reference and the tests

### 7.1 Stage 1: the list is proven before any shader

- `render_chunk` with the CPU builder becomes "build the `ChunkDraws`, replay it": a `replay(draws, frame table, canvas)` that walks the records in order with `blit_sprite`, opening a local canvas per clip segment and `composite`-ing it, exactly as `render_with_terrain` does. The CPU path, `--png`, `anno-dump`, `render_corpus_test.cpp` digests and `sprite_fidelity_test.cpp` must stay **unchanged**: every digest equal is the proof that the list carries everything (order, clip, shadows, bases, soldiers).
- Tests: per group, every chunk of `Single_01.sze`, `Single_24.sze` and `D_256x256_0045_5B.scp`: replayed planes equal the old `render_chunk` planes byte for byte (needs the game files, skips cleanly otherwise); the records' bytes are checked against the field layout (as `frame_uniform_bytes`, `gpu.hpp:254-255`); residency unit tests (LRU order, pinning, budget overflow → CPU fallback counter, frame ids stable across evictions) without a GPU.

### 7.2 Stage 2: GPU = CPU

- For each group, every chunk of the three scenes: the GPU-built `objects` and `base` planes read back equal the CPU planes exactly (C1 compute pass), `ground` and `terrain` within the tolerance of 5.2, `field` and `flow` identical (still uploaded from the CPU). The existing frame-level checks (`gpu_test.cpp` 18 views within 1 per channel; stand-in tests) run with both builders.
- Mutation checks of the new code (as the 2026-10-18 review did, `iso.md:215`): floor → round in `over_u32`, a dropped clip test, shadow after its object, no `base` reset on a shadow-only composite, an off-by-one bin, the frame table's `anchor_y` ignored, the `colour` / `depth` fields read as non-zero; each must fail a test.

### 7.3 The synthetic scene (no game files; runs wherever a GPU or llvmpipe is available)

Hand-made frames (`Rgba` images and headers) and records, built in the test, covering what the original corpus never sends through the GPU sprite path (`iso.md:215`: shadowed and translucent sprites were mutation survivors):

1. A shadow over a translucent sprite (alpha 64, 128, 191) and a translucent sprite over a shadow, on transparent and on opaque objects pixels.
2. Three compounding shadows on one pixel (`tBrushShadR` compounds, `annodisplay.md` §3; `kShadow` three times).
3. A checkerboard frame (alpha 0 / 255 alternating per column with the row phase toggling, the decoded `WZ` pattern, `annodisplay.md` §3).
4. Negative anchors (`anchor_x`, `anchor_y` < 0 as `i16`; `annodisplay.md` §4: 735 figure frames), so the box lies right of / below the anchor.
5. An image wider than its frame box (rows past the width, OQ-025, `iso.md:213`).
6. Boxes crossing the chunk edge and an island's clip edge; two islands overlapping, the second contributing only a shadow over the first's object (the `base` reset to 255).
7. A soldier interleaved between objects by its key; frames on two atlas layers; an eviction of a static layer between two chunk builds that reuse a frame.

Expected values come from the CPU replay (7.1) of the same records; for 1-2 also from a hand-computed `over` sequence in the test, so the reference itself is pinned.

## 8. (f) The fallback switch

- `FrameRenderer::chunk_builder` = `Cpu` (default) | `GpuList`. `Cpu` is today's behaviour; `GpuList` makes the workers produce draw lists. Without a GPU (headless, CI without an adapter, the CPU path, `--png`, the digests) the builder is `Cpu`; a `GpuFrame` refuses `GpuList` chunks only if it cannot create the compute pipeline (then it switches the renderer to `Cpu` once and says so).
- The viewer sets `GpuList` with the GPU renderer once the owner accepted it (stage 2 owner check); until then the default stays `Cpu` and `ANNO_VIEW_GPU_CHUNKS=on` turns it on; afterwards `ANNO_VIEW_GPU_CHUNKS=off` turns it off. `bench_frame` gets the same switch. Name: the task's `--chunks gpu` collides with the existing `--chunks blocking,async`; proposed `--chunk-build cpu|gpu` (**? C4**).
- Per chunk: budget overflow (4.3 rule 5) builds that chunk on the CPU (the worker is asked for a `Cpu` build of the key; the `serial` rule replaces the placeholder when it arrives).

## 9. (g) Limits as configuration (PLAN 0.1)

| Value | Where | Default | Reason |
|---|---|---|---|
| `atlas_size` | `GpuLimits` (exists) | 2048 | WebGPU guarantees 8192 (`maxTextureDimension2D`); 2048 holds the largest frame (682 × 651, P) and keeps a layer at 16 MiB, the LRU unit |
| `max_atlas_layers` | `GpuLimits` (exists) | 0 = device `maxTextureArrayLayers` | device limit |
| `atlas_budget_mb` | `GpuLimits` (new) | 512 discrete / 256 other (4.3 rule 6) | holds every original scenario's frames for all groups (197 MiB, M); **? C3** |
| `atlas_upload_bytes_per_frame` | `GpuLimits` (new) | 16 MiB | the wave window's per-frame upload bound; above the largest per-chunk set (7.8 MiB, M) |
| `gpu_chunks_per_frame` | `GpuLimits` (new) | 6 | as `uploads_per_frame`: bounds what changes on screen per frame and the encoder's size |
| `bin_px` | `FrameRenderer` (new) | 32 | T: 256 bins per chunk; smaller bins shorten the per-pixel lists and lengthen the index list; measured in stage 2 |
| Record fields | section 3 | `u32` ids, `i32` positions, `u16` heights | widened from the files' `u8`/`u16`/12-bit ids; a `u32` frame id limits a scene to 4·10⁹ distinct frames (GPU indices are 32-bit) |
| `chunk_builder` | `FrameRenderer` (new) | `Cpu` until the owner accepts | section 8 |

No fixed arrays: the clip table, the bins, the frame table and the keel buffer are sized per chunk or per scene; the ground texture array is sized by `Felder.tex`.

## 10. (h) What moves later (named, not built in T11)

1. **Sea field and terrain planes from island data** (a compute shader per island and group from `HEIGHT`, the shore distance and the river data), after T1f item 2 defines the island-level fields on the CPU as the reference. Then the per-chunk probe disappears and a chunk is "records + island fields" only.
2. **Moving units from the sim** (carriers, civilians, animals, ships, soldiers): the same objects record (3.1), built per frame from sim state instead of per chunk, drawn by the existing per-frame sprite pipeline into the frame (as ships today, `gpu.cpp:1677-1686`) with the hot atlas class and the same residency. Their order against static objects needs the depth key (`depth` field) or a per-frame interleave with the chunk's objects; that is a design of its own (OQ-026 c/f).
3. **Indexed atlas**: frames stored as palette index + alpha level (RG8, half the bytes of RGBA8) with per-instance colour tables (`colour` field; `annodisplay.md` §8 player colours and light tables), instead of one recoloured RGBA frame per player.
4. **The original's per-pixel depth** (`z` offsets, `M` melt, `H`/`V` write rules; `annodisplay.md` §3) as an opt-in fidelity mode using the `depth` field and a depth plane, only after the depth units are traced (`annodisplay.md` "Open").

## 11. Stages and acceptance (from the task, mapped)

| Stage | Content | Evidence |
|---|---|---|
| 1 `DONE` (2026-10-21, see "Stage 1 as built" below) | frame ids, frame table, residency (CPU model), `ChunkDraws` builder, CPU replay | 7.1: digests unchanged, replay = old planes, residency unit tests |
| 2a | compute pass for objects + base, scheduling, `GpuList` builder behind the switch | 7.2, 7.3; `bench_frame` per-stage numbers; **owner check** (canyon island: fast pan, wheel zoom; `Single_24.sze`) |
| 2b | ground (+ terrain) render pass | 7.2 with the 5.2 tolerance; after T1f item 2 or by the profile (C2) |
| 3 | cache and prefetch take GPU-built chunks, stand-ins from them, 5000 px/s pan, cold frame | `docs/perf/README.md` table: chunk latency CPU vs GPU per group, resident atlas MiB, p50/p95/p99/max at 1280 × 800 and 2560 × 1440, RTX 4090 and Radeon iGPU |

### Stage 1 as built (2026-10-21, CPU only)

- **Frame table** (`FrameTable`, `frame_table.hpp`): scene-wide interning of the frames a list names, dense `u32` ids that never change across evictions; decode on the thread that first references a frame; header, image and per-column keel computed once. Shared mutex like `SpriteCache`.
- **Draw list** (`ChunkDraws`, `chunk_draws.hpp`): 32-byte little-endian object records written field by field, clip table, sorted frame set, CSR bins (`bin_px` 32 by default). The ground record format is defined and layout-tested; there is no ground builder yet (stage 2b).
- **Builder and replay**: `Scene::objects_draws` emits records in the CPU blit order (islands in layout order, shadow before its object, soldiers merged by their key, ships as an unclipped tail). `replay()` draws them with `blit_sprite_keel`, the same per-island local canvas and `composite()` rule as `render_with_terrain`. `render_chunk` builds the list and replays it (`chunk_builder` switch, default Cpu); output is byte-identical to the old path.
- **Residency model** (`AtlasResidency`, `residency.hpp`): pure CPU class, layers of `atlas_size`^2 texels under a byte budget, Static and Hot classes in separate layers, shelf packing, layer-granular LRU, per-frame pinning, an upload byte cap (`Deferred`), `Overflow` as the CPU-fallback counter. The budgets are `GpuLimits::atlas_budget_mb`, `atlas_upload_bytes_per_frame`, `gpu_chunks_per_frame` (defaults and reasons in `gpu.hpp`); the compositor does not read them yet.
- **Tests**: `chunk_draws_corpus_test.cpp` replays every chunk of `Single_01`, `Single_24` and the canyon island at all three groups, ships on and off, and compares objects and base planes with `Scene::render` (equal), the binning against a brute-force list, and that every clip entry lies inside its chunk; `residency_test.cpp` covers the model. All 67 render tests matching the stage 1 filter pass on the msvc Release build.
- **Not done in stage 1**: any GPU work (2a), the ground pass (2b), the cache and prefetch taking GPU-built chunks (3).

### Stage 2a: the derived-level decision (before coding)

Written before the code, per the task. T3 stage 3 derives a coarser level's chunk by reducing its
four children 2x (`reduce_chunks_2x`, `chunk_reduce.hpp`). Today that derivation runs **on the
workers** over the cached CPU children's pixels (`frame.cpp:279 render_derived_chunk`) and the
result is uploaded like any chunk. With the `GpuList` builder a built chunk's `objects` and `base`
live only in GPU textures (the compute pass writes them, 5.1), so a group-2 GpuList chunk has **no
CPU `objects`/`ground` pixels** and the CPU `reduce_chunks_2x` cannot run over it without a
read-back.

The engine already reduces on the GPU: `Impl::reduce_quad` (`gpu.cpp:1080`) runs `reduce_fs`,
binding each child's **GPU plane textures** (`src[q]->bind`) and writing the derived chunk's
textures; `chunk_reduce.hpp` pins `reduce_fs` == the CPU `reduce_chunks_2x` exactly. (The read-back
helper `GpuFrame::reduce_chunks_2x`, `gpu.cpp:2722`, uploads CPU children and reads the result back
— a test oracle, not a per-frame path.) That pass is not yet wired into the live chunk flow; the
viewer's derived chunks come from the CPU workers today.

**Decision (option a of the task):** with `GpuList` a derived chunk is produced **on the window
thread by `reduce_quad` over the children's GPU textures**, into the frame's submission — not by the
CPU `render_derived_chunk`. This meets both constraints: **no read-back on the window thread**
(`reduce_quad` reads child textures and writes new ones) and **no second build of any group-2
chunk** (each built chunk is produced once by the compute pass; its textures are then read by the
reduce). The children are resident before the parent by the existing depth-first `plan_jobs` order
(children are jobs before the parent, `frame.cpp:879`); a derived chunk whose four children are not
all resident keeps its stand-in, exactly as a built chunk whose frames are not all resident (4.3
rule 2). `base` and `flow` stay un-derived (`chunk_reduce.hpp`). The ground/field/terrain planes of a
GpuList child are still CPU-uploaded in 2a, so they are ordinary GPU textures on the child's bind
group and `reduce_quad` reduces them unchanged alongside the compute-pass `objects`.

Rejected: CPU `reduce_chunks_2x` over GpuList children needs a read-back (forbidden); building the
reduced range on the CPU in GpuList mode is a second builder and rebuilds the children (forbidden).

**Test (7.2 extension).** For each scene, the derived levels the viewer uses (`ZoomLevels` beyond
`built_count`): a GpuList derived chunk's planes, read back, equal the CPU `render_derived_chunk` of
the same key within the reduce tolerance (objects/ground within 1 per channel — the alpha-weighted
mean; field/terrain exact), with the group-2 children built by both builders. A mutation (wrong
quadrant origin, a child's bind group swapped in, the reduce skipped) must fail it.

**Built 2026-10-22 b (PR #17).** `build_derived_gpu` / `gpu_chunk_for_reduce` in `gpu.cpp` wire this:
`sync_chunks` detects a derived level under `GpuList` and reduces the four GPU children with
`reduce_quad` (recursing for a derived child). The test (`gpu_chunk_build_test.cpp`) compares the
composed GpuList derived frame to the Cpu builder's at derived groups 3 and 4 (0 bytes differ by
more than 1); the three mutations above were applied locally and each failed its `maxd <= 1` /
`over1 == 0` CHECK (reverted, not committed).

### Stage 2a as built (2026-10-22)

- **The compute pass** (`gpu.wgsl` `chunk_build_cs`, 5.1): one `@workgroup_size(8, 8)` invocation
  per chunk pixel walks its bin's records and repeats `replay_binned` (`chunk_draws_model.hpp`,
  the pinned spec of the pass) exactly — integer `cb_over` in u32 with floor division, the
  per-island local canvas and its `base` reset, the keel contact. It writes two storage buffers
  (objects as RGBA8 packed in a u32 per pixel; base one byte per pixel, four per u32 via a
  race-free `atomicOr` into a buffer cleared with `ClearBuffer`) that are copied into the existing
  RGBA8Unorm objects plane and R8Uint base plane. The pass uses only group 0 (bindings 10-19), so
  its pipeline is one bind group; the compositor's passes are unchanged. Tolerance 0.
- **Residency and atlas** (`gpu.cpp` `ChunkBuild`): the stage-1 `AtlasResidency` drives a static
  sprite atlas **of its own** (its own RGBA8Unorm 2D array), separate from the per-frame sprite/ship
  atlas, so the merged compositor's sprite path is untouched. **This is an accepted deviation from
  4.2's one texture, two classes** (orchestrator decision 2026-10-22, PR #17): separate textures are
  simpler and risk-free for the Hot path; frames never overlap between the two (chunk lists never
  name water objects or ships), so nothing is duplicated; folding the two is revisited in stage 2b
  when the sprite path moves. The atlas grows by doubling, **clamped to the residency's layer cap so
  the grow's transient old+new peak stays within budget** (G3), copies counted in
  `GpuStats::chunk_atlas_grows`. The **OQ-096 budget covers BOTH atlases together** (G3): the chunk
  atlas budget subtracts the per-frame sprite atlas's current footprint each frame
  (`AtlasResidency::set_budget`), so the two never exceed 512 MiB discrete / 256 MiB other. `ensure()`
  Overflow builds that chunk **on the CPU** (`cb_cpu_fallback`: the draw list replayed on the window
  thread, `GpuStats::chunks_cpu_fallback`), not a placeholder. A scene-wide GPU frame table (32
  bytes/id) and an append-once keel buffer carry what the pass reads; **only the dirty frame-table
  entries and the appended keel tail are uploaded** per frame, counted in the atlas upload budget
  (G1). The seven per-chunk scratch buffers and the compute bind group are **pooled in `cb` and
  reused** across builds (G2; safe because builds are sequential and queue-ordered). The base/objects
  copy asserts `bytesPerRow % 256 == 0` (G4).
- **Scheduling** (6): `sync_chunks` builds a `GpuList` chunk's objects/base in its own submit during
  the sync (as `reduce_quad` and atlas growth already do), before the compositor passes; the window
  thread never fences, maps or reads back. Bounded by `uploads_per_frame` / `upload_budget_ms` (the
  existing gate) and `atlas_upload_bytes_per_frame` (the first built chunk always goes). Both the
  visible and the prefetch paths build `GpuList` chunks. The compute pipeline and the first atlas
  layer are created in **`GpuFrame::create`** (not lazily on the first GpuList frame; removes the
  first-frame hitch).
- **Derived levels** (the committed decision below, item 1): a derived level under `GpuList` is
  reduced on the window thread by `reduce_quad` over the four GPU children (`build_derived_gpu` /
  `gpu_chunk_for_reduce`), each child built once by the compute pass, no read-back. Far-zoom objects
  now match the CPU `render_derived_chunk` reference (reduce tolerance).
- **The switch** (8): `ANNO_VIEW_GPU_CHUNKS=on` in the viewer and `--chunk-build cpu|gpu` in
  `bench_frame`; default `Cpu`. No compute pipeline → `cb.failed`, the chunk builds on the CPU.
- **Tests** (`gpu_chunk_build_test.cpp`, `gpu_test.cpp`): `GpuFrame::build_chunk_planes` equals
  `replay()` byte for byte for every chunk of `Single_01`, `Single_24` and the canyon island at all
  three groups (2738 chunks, 605,564 records, 0 mismatches, RTX 4090 Vulkan); the synthetic 7.3
  scenes (shadows over translucent, three compounding shadows pinned by hand, the WZ checkerboard,
  the base contact branch, a wide image, chunk/clip edges, the two-island base reset, ignored
  colour/depth) each GPU = CPU = the binned model; the composed frame is byte-identical with both
  builders on `Single_24` and the canyon island at every group; **a GpuList derived chunk's composed
  frame equals the Cpu builder's `render_derived_chunk` within the reduce tolerance (0 bytes differ
  by more than 1) at derived groups 3 and 4** (7.2 extension); the **GPU = CPU stand-in test runs
  with both the Cpu and GpuList builders** (stage-0 addition 3), each stating its stand-in source; a
  no-GPU string check ties the WGSL bindings to `gpu.cpp`. **Mutation evidence** (B3, applied locally,
  not committed): each 7.2 mutation fails a named CHECK — floor→round, dropped clip, no base reset,
  ignored anchor_y, used colour/depth, off-by-one bin (`gpu_chunk_build_test.cpp` lines 359/417 and
  the corpus `mismatches == 0`); wrong quadrant origin, child bind group swapped, reduce skipped (the
  derived test's `maxd <= 1` / `over1 == 0`).
- **Measured** (`docs/perf/README.md`, 2026-10-22 and 2026-10-22 b): the cold frame at group 0
  halves (119.5 → 56.5 ms); interactive (async) pan p50/p95/p99/max at 1280×800 and 2560×1440 on the
  RTX 4090 (adapter 0) and the AMD Radeon iGPU (adapter 1) for both builders are within the frame
  budget (GpuList within 1-2 ms of Cpu on the window thread); the `bin_px` 16/32/64 sweep is flat on
  both adapters (**bin_px 32 kept**); the resident chunk atlas peaks ~11.8 MiB (canyon group 0) with
  0 CPU fallbacks. **Remaining:** the isolated GPU time of the dispatch per chunk (timestamp writes
  around `cb_encode`).

## 12. Decisions and open questions (stage 0 review of 2026-10-21 folded in)

- **C1 DECIDED (orchestrator): the exact binned compute pass** for objects and base (5.1): tolerance 0, the per-island composite and the `base` reset repeated. The blending render pass stays the documented fallback only (fixed-function blending rounds where the CPU floors; compounding shadows and the per-island local canvas cannot be repeated by a blend state). Conditions: the per-pixel walk is measured on the Radeon iGPU as well as on the RTX 4090 in stage 2a with GPU timestamps, and `bin_px` is tuned from that measurement, not guessed.
- **C2 DECIDED (orchestrator): objects first (stage 2a).** The ground pass (2b) follows T1f item 2, unless the T1f profile shows the ground stage above 20 % of the chunk time on its own (it was 14-16 %).
- **C3 = OQ-096 (owner, a limit default, `docs/open-questions.md`):** `atlas_budget_mb` 512 discrete / 256 integrated, or another rule (4.3 rule 6). Decided after the stage 2a owner check, when the resident MiB are in the perf README.
- **C4 DECIDED (orchestrator): `--chunk-build cpu|gpu`** in the viewer and the bench, `ANNO_VIEW_GPU_CHUNKS` as the interim switch; the default flips to `gpu` with the GPU renderer only after the stage 2a owner check.
- **C5 DECIDED (orchestrator): measure** whether WGSL compilers contract the ground's edge functions (FMA) on the 4090 (Vulkan) and the iGPU; a counted edge-pixel tolerance on the ground is acceptable only with the count in the test and in the README, otherwise 0.
- **Stage 0 additions** (review of 2026-10-21): (1) frame decoding on the workers (done in stage 1); (2) the initial atlas layer count is sized from the scene's frame set at load, so the doubling copy of a large atlas rarely happens in a frame, and if it does it is recorded in `GpuStats` (stage 2a); (3) the stand-in CPU reference and the GPU = CPU stand-in tests run with both builders; with `GpuList` the CPU cache holds lists, not pixels, so the stand-in source for the CPU reference comes from a read-back or the test runs the CPU builder, and the test says which (stage 2a); (4) this section (done).
