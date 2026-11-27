# RENDER-DIRECT-3: the direct object and ground passes

**Drafted 2026-11-27 by the Windows render executor; rewritten the same day per ORCH III's
review.** ADR 0007 step 3. Covers OQ-775 (see below - the reasoning in the first draft was
wrong) and gives decision 8 its tolerance numbers. Branch from `task/RENDER-DIRECT-2c`'s accepted
head. Code/docs only until "PC free".

## What exists to build on
Per island, per group, a chunk-bucketed, patched `vector<StaticInstance>` (2a/2c): `draw.anchor`
(screen pixels), `key` (identity), `painter_key` (paint order), `SpriteDraw::frame`/`height`. 2a
already proved these records equal `objects_draws`' own records chunk by chunk, on real game
files (930k+ and 3.5M assertions, two stacked packets) - so a CPU raster of the same records
through the same `object_record` and blit is equal by construction; a second byte-equality stage
would prove nothing new. The real new work in this step is the **GPU** drawing: uploading these
buffers and drawing from the resident atlas (RENDER-DIRECT-1c/1d) directly, instead of baking a
chunk and drawing that.

## OQ-775, corrected
OQ-775 is not about id width - that was PICTURE-IDS-1's question, already settled (no original or
shipped type needs more than 12 bits; RENDER-DIRECT-3 answers the mod case anyway, see below).
OQ-775 is the **patch pipeline's texture budget**: `patch_submerged_fs` gives no sea over
submerged pixels inside a placement ghost in enhanced water, because extending the patch
pipeline's frame group to call `sea_shade` would need 17 sampled textures (3 over its documented
14-texture ceiling) plus `piece`/`river` uniform wiring it does not have. **This goes away in
stage 3c**, below: once the ghost is drawn as ordinary instances through the same object pass
everything else uses, there is no separate patch pipeline left to have a texture budget at all.

## Staging (each ≤ 4 h)

**3a - the GPU object pass from instance buffers, behind a switch, off by default.**
- Upload the static instances (2c's per-chunk buckets) as an instance buffer per visible chunk;
  draw objects directly from the resident atlas (1c/1d's texture arrays), not a baked chunk.
- **Done when (precise):** on `Single_01.sze` and `lan5.txt`, with the switch on, the GPU object
  layer matches the CPU reference (today's `objects_draws`/replay path) within a **measured**
  tolerance (ADR 0007 decision 8) - report the tolerance's actual numbers (max and mean per
  channel, the pixel count over the threshold), the 4090 and the Radeon both named, and write them
  into the ADR before this stage is accepted. Do not guess the numbers first.
- A mutation control (one wrong instance in the uploaded buffer) must exceed the tolerance.
- Switch off: 0 digest change anywhere (`--png`, every recorded digest, every existing test).
- Units and ships stay on the old path this stage (a later stage names when they move - see
  below).

**3b - the ground pass and the per-pixel planes (`base`, `field`, `terrain`, `submerged`), same
contract as 3a** (a measured tolerance switch-on, 0 digest change switch-off, a mutation control).

**3c - the ghost and placement preview as instances; retires the patch pipeline; UI-RANGE-1 B2's
draw-time darkening.**
- The placement ghost/preview (today's `ObjectsPatch`/`patch_*_fs` shaders, UI-GHOST-2) becomes
  ordinary instances drawn by 3a's own object pass, instead of a separate patch pipeline with its
  own frame group and texture budget - this is what resolves OQ-775 (above): there is no more
  `patch_submerged_fs` to have a 14-texture ceiling.
- **UI-RANGE-1 B2** (an object whose anchor tile is outside the currently-shown range darkens to
  5/8 per channel): not baked into the resolved instance - which tiles are dark changes every
  frame (the player's current selection/hover), while the instance data is only patched on a
  building change. So: `StaticInstance` carries the object's **world tile** alongside the screen
  anchor it already has (one or two more fields, resolved once, from `Prepared::first.x/y` - no
  extra resolve cost). The darkening itself is a draw-time colour multiply against whatever
  `range_view` the frame is currently showing: bit-exact 5/8 in the CPU reference's blit, within
  3a/3b's own measured tolerance in the GPU shader (the same shape `object_in_water`'s per-pixel
  lookup already uses for a different per-pixel condition) - never baked into the instance data,
  and never a reason to patch more often.
- Units and ships move to the instance path in this stage too (named here, not earlier): they are
  the other thing the old per-chunk draw still owns once the ghost moves off it, and both are
  small, bounded sets per frame, unlike the (patched) static-object buffers.

## Common rules
clang-format 23.1.3; WSL `check_unix_warnings.sh`; REPORTS entry per stage (~60 lines); exact-head
CI; PC-quiet check before any GPU/long run - ask for "PC free"; no reviewer subagents (ORCH III
reviews); push per-stage branches, never `main`.
