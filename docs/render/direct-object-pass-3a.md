# RENDER-DIRECT-3 stage 3a: the GPU object pass, design

Written before any WGSL is committed — a shader's syntax and semantics only get checked when a
real device creates the module, which needs a build this host did not have free while this was
written.

**Revised from the first draft of this note**, after reading `sprite_vs`/`sprite_fs`
(`gpu.wgsl:1019-1224`) and their pipeline construction (`gpu.cpp`) in full rather than assuming a
new shader or a new pipeline was needed: those two entry points already do exactly 3a's job —
quad from an instance record, `textureLoad` the atlas by `(src + local, layer)`, discard on
`.a == 0`, the shadow-flag/river-tolerance special cases — and their instance layout (`dst:
vec2<i32>` / `size: vec2<u32>` / `src: vec2<u32>` / `layer_shadow: vec2<u32>`, 32 bytes,
`gpu.cpp:3188-3193`) already matches `DirectObjectInstance`'s fields almost field-for-field.
**3a needs no new WGSL and no new pipeline** ("Pipeline wiring" below) — only a new `BindGroup`
pointing the existing `m.sprite_pipe` at `cb.residency`'s resident texture instead of the sprite
pipe's own never-evicting cache (`m.atlas`, `atlas_place()`/`SpriteKey`, `gpu.cpp:2431`). WGSL
binds by group/binding number, not by a fixed resource identity, and `m.atlas_layout`
(`gpu.cpp:3105-3106`) constrains only the binding's *type*, not which texture fills it — so the
same pipeline can draw from either atlas depending only on which bind group is set at draw time.
The two atlases stay separate (rejected merging them): `m.atlas` is a small, low-cardinality,
never-evicting cache sized for ships/water sprites/the ghost overlay; `cb.residency` is sized and
evicted for a whole map's worth of distinct building/tree frames (RENDER-DIRECT-1c/1d's own
budget/eviction policy) — pointing 3a's draw at the second one, not merging the two, is what lets
static-object frames evict under their own budget without disturbing the sprite pipe's.

## What's already resolved, CPU-side (no new design needed here)
`direct_object_instances()` gives, per visible chunk, a `vector<DirectObjectInstance>`:
`{ObjectRecord record, AtlasPlace atlas}`. `ObjectRecord` is `{x, y, frame, flags, clip, colour,
depth}` (`chunk_draws.hpp`) — `x`/`y` already the scene position of the frame box's top-left
corner (anchor minus the frame's signed anchor), `flags` already carrying the depth class/write/
skip-half bits (`object_flags::depth_bits`). `AtlasPlace` is `{layer, x, y, width, height}`
(`residency.hpp`) in the resident atlas already bound as `cb_atlas`'s texture (RENDER-DIRECT-1c/
1d). Painter's order is the buckets' own order (2c's `painter_key`); the shader does no sorting.

## Instance buffer: exactly `sprite_vs`'s own 32-byte layout (`gpu.cpp:3188-3193`), filled from
`DirectObjectInstance` — no new layout, no new attribute list:

| `@location` | WGSL type | field | from |
|---|---|---|---|
| 0 | `vec2<i32> dst` | `(x, y)` | `record.x, record.y` |
| 1 | `vec2<u32> size` | `(width, height)`, high 16 bits of each 0 | `atlas.width, atlas.height` |
| 2 | `vec2<u32> src` | `(atlas_x, atlas_y)`, high 16 bits of each 0 | `atlas.x, atlas.y` |
| 3 | `vec2<u32> layer_shadow` | `(layer, flags)` | `atlas.layer, record.flags` |

High halves of `size`/`src` are the existing "reduced" path (`sprite_fs`'s `frame_size.x != 0u`
branch, a scaled-down mip used elsewhere) — 3a always writes 0 there ("ordinary instances have 0
in the high halves", `sprite_vs`'s own comment), so that branch is simply never taken for direct
instances; nothing to build for it. `record.colour`/`record.depth`/`record.clip` are not in the
buffer: `colour` is always 0 today (reserved, section 3.1); `clip` only matters for the
chunk-build replay's local-canvas compositing, which this painter-order draw does not do (see
"Depth" below); `depth` only decides whether `objects_draws` emits a record at all, already
decided CPU-side before the instance list is built, same as the baked path today.

**Open point for implementation, not guessed here:** `sprite_fs`'s `flags` bit layout (bit 0 =
`iso::SHADOW`, bit `0x10000` = river tolerance, bit `0x20000` = ship-in-water) must be checked
against `object_flags`'s own bit layout (`chunk_draws.hpp`, `depth_bits`/`depth_rule`) before
`record.flags` is written into `layer_shadow.y` as-is — if the two don't already share bit
positions for `SHADOW`, 3a packs only the bits `sprite_fs` actually reads (shadow), not the raw
`object_flags` word, and leaves the depth-class bits unused this stage (see "Depth" below).

## Coordinate space
`record.x`/`y` are scene pixels from `prepare_objects`' own frame, already how the CPU reference
positions a blit (`Scene::objects_draws`). `sprite_vs`'s own `to_clip(vec2<f32>(dst) + quad_corner(i)
* vec2<f32>(dsize))` already does the scene-pixel-to-clip-space transform objects need (ships use
exactly this) — nothing new to write.

## Fragment shader
`sprite_fs` as it stands (`gpu.wgsl:1194-1224`): `textureLoad(atlas, in.src + local, in.layer_shadow.x,
0)`, discard on `.a == 0`, the shadow bit. Reused unchanged — the "new" atlas binding is a pipeline
construction detail (which texture `atlas` is bound to), not a shader change. No `cb_atlas_z`
involvement (that plane is the chunk-build pass's own, 3b's concern).

## Depth / occlusion
Confirmed by grepping `gpu.cpp` for `DepthStencil`/`depthStencil`: **zero hits anywhere in this
codebase.** There is no real GPU depth buffer today; every depth-ordering is either (a) baked
per-pixel into a chunk texture by the compute pass (`depth::draw`'s override bits), or (b) simple
painter-order alpha blending (ships, water sprites, the ghost) — exactly `sprite_pipe`'s existing
blend state (`alpha`, `gpu.cpp:3184-3187`). 3a is case (b): one draw per visible chunk, instances
in the bucket's existing painter order (2c's `painter_key`), no shader-side depth test added. This
is the "measured tolerance" ADR 0007 decision 8 already anticipates, not a new problem to solve.
`record.flags`' depth-class bits are not used by 3a's draw at all (the fragment shader does not
test or write depth); they stay available on `ObjectRecord` for 3b, when a real per-pixel ground
plane exists to occlude against.

## Pipeline wiring — smaller than the first revision above: no new pipeline at all
`m.atlas_layout` (`gpu.cpp:3105-3106`) binds only a `texture_2d_array<f32>` by type — it does not
pin which texture. `m.sprite_pl` is built from `{frame_layout, empty_layout, atlas_layout}`
(`gpu.cpp:3128-3133`) and `m.sprite_pipe` from that layout (`gpu.cpp:3221-3222`); neither names
`m.atlas` anywhere in their own construction — only the *bind group* bound at draw time
(`m.atlas->bind.get()`, `gpu.cpp:3759`) does. The chunk-build pass already has its own
`WGPUTextureView` for the resident atlas (`atlas_view`, `gpu.cpp:729`, inside the same scratch
struct as `cb.residency`). So **3a needs only one new `BindGroup`**, built from `m.atlas_layout`
and that existing `atlas_view` — `m.sprite_pl`/`m.sprite_pipe` are reused completely unchanged.
At draw time: `SetPipeline(m.sprite_pipe)`, `SetBindGroup(2, <the new bind group>, ...)` in place
of `m.atlas->bind.get()`, `SetVertexBuffer(0, <the new per-chunk instance buffer>)`, one draw per
visible chunk (`instance_count = bucket.size()`, the existing non-indexed-quad-via-`vertex_index`
trick). Behind the switch named in the packet (`render-direct-3.md`): off issues none of these
draw calls, so every existing digest is unaffected by construction.

**Chunk-spanning instances must be scissored, not just drawn per bucket (advisor-caught, fixed
here before any code).** `bucket_by_chunk_into` (`chunk_instances.cpp:44-56`) pushes the same
global instance into *every* chunk its `reach()` rect spans — correct for the baked path, where
each chunk's own `clip`/bounds check (`chunk_build_cs`, `sx/sy` against `cb.origin`/`cb.size`)
confines that chunk's compute pass to its own pixels. A direct, unclipped quad draw has no such
confinement: drawing the same instance's full quad once per chunk bucket it appears in would
blend a spanning object's shadow/overlap twice, and a later chunk's draw would redraw over an
earlier chunk's already-composited pixels, breaking painter order at the seam. **Fix: each
chunk's draw call sets a scissor rect to that chunk's own screen-space rectangle** (already known
- chunks are a fixed grid) before issuing its instances, reproducing the baked path's per-chunk
confinement exactly, with no change to painter order or to which instances are listed per chunk.

## Open scope decision, not mine to make: depth-class objects and `clip`
**Correction to this note's first draft**, which wrongly said depth was "already decided
CPU-side before the record list is built" — advisor caught this against code already read.
`chunk_build_cs` does a real **per-pixel** depth test: `cb_depth_at` from `cb_terrain` at every
pixel (`gpu.wgsl:1551-1557`), then per covered pixel compares against `cb_records[rec + 6]`
(the record's base depth) by `cls` (`gpu.wgsl:1630` onward), using `cb_atlas_z` for the frame's
own z-plane. `record.clip` (`gpu.wgsl:1598-1605`) also discards pixels outside a per-record clip
rect, for the chunk-build's local/global canvas compositing - a second per-pixel behavior this
note's instance buffer (deliberately dropping `colour`/`depth`/`clip`, above) does not reproduce.
3a's instanced draw, as scoped above, has **no per-pixel depth test and no clip rect** - it is a
flat painter-order blend only. Two ways to close this gap, and the choice is ORCH III's/the
owner's, not this note's:
(a) **scope 3a's "measured tolerance" pass to objects whose `depth_rule.cls == Class::None`**
    (no depth class, no clip) on `Single_01.sze`/`lan5.txt` first, and widen to depth-tested/
    clipped objects only in 3b once a GPU-resident ground/z-plane exists to test against; or
(b) **3a itself carries `record.depth`/the clip rect into the instance buffer and the fragment
    shader does the same per-pixel test `chunk_build_cs` does**, against the baked chunk's own
    existing `cb_atlas_z`-style z-plane - more shader work this stage, but one tolerance
    measurement instead of two scoped passes.
Either way, report which objects (by `depth_rule.cls`) the first measured-tolerance number
actually covers, so "matches within tolerance" is not silently measured on a subset.

## A shortcut, named as the rules ask
Building the instance buffer fresh every frame from `direct_object_instances()` conflicts with
ADR 0007 decision 1 ("a camera move is a transform, never a content change") if read literally -
the instance *data* does not change on a camera move, only which chunks are visible, so rebuilding
it every frame is wasted CPU work, not a correctness problem. `shortcut: rebuilds every frame
instead of caching per chunk; upgrade when profiling shows it matters - cache per chunk (keyed
like 2c's buckets already are), invalidate only on a patch (`patch_chunk_instances`) or on
`AtlasResidency::take_evicted()`.`

## What this does not cover
Ground/per-pixel planes (3b), the ghost (3c, still on the patch pipeline until then), units/ships
(3e, already on `sprite_pipe` — left alone this stage), 3d's darkening (needs the world-tile field
not yet added to `StaticInstance`, queued for 3d itself per `render-direct-3.md`).
