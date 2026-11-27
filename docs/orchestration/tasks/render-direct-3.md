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

**3a - the mechanism stage (shadows only), behind a switch, off by default.** **No real object
(building/tree/decoration) leaves the baked path in 3a** - say this plainly in the packet and the
REPORTS entry, not just in a scope footnote. Measured on real game files (CPU-only, built and
run, msvc Release): `object_flags::depth_rule(flags).cls == depth::Class::None` is **100 % shadows
on both named scenes** - Single_01.sze: 31,870 of 95,510 instances are `None`, all 31,870 of them
shadows, 0 are not; lan5.txt: 64,775 of 170,896, same split. Every real object on both scenes is
`Class::Z`. 3a therefore proves the mechanism end to end - the instance buffer, the residency
atlas bind group, the per-chunk scissor, the vertex path, the resolve→upload→place ordering (the
`ensure()`-with-no-device bug found and fixed while measuring this, see REPORTS) - without
delivering the real win yet; 3a2 (below) is where real objects move.
- Reuses `sprite_vs`/`sprite_fs` and `m.sprite_pipe` unchanged (design:
  `docs/render/direct-object-pass-3a.md`) - a new `BindGroup` points the existing pipeline at
  `cb.residency`'s own resident texture instead of the sprite pipe's own never-evicting cache; no
  new WGSL, no new pipeline. Upload the `Class::None` static instances (2c's per-chunk buckets,
  filtered to `None`) as an instance buffer per visible chunk; each chunk's draw call is scissored
  to its own screen rect (a spanning instance is listed in every chunk its `reach()` touches, 2c's
  own bucketing - without a scissor its quad would blend once per chunk it is listed under,
  double-blending at the seam; the baked path avoids this only because each chunk's compute pass
  already clips to its own pixels). Shadows have no depth test either way (`Class::None`'s own
  comment: "shadows; units keep their former drawing") and are flat 37.5 % black regardless of
  draw order among themselves, so filtering them out of the baked chunk's record list and drawing
  them directly instead changes no visible stacking - the proof shape below is sound because of
  this, not despite it.
- **Proof shape (advisor's, ORCH III confirmed):** draw only the filtered `None` buckets with the
  direct pass into a cleared target; compare against a CPU replay of the same filtered records
  (not the full baked frame - this needs no bake-filter change and sidesteps `objects_fs`'s
  base/in-water logic, which `sprite_fs` does not have).
- **Done when (precise):** on `Single_01.sze` and `lan5.txt`, with the switch on, the cleared-target
  direct draw of `None`/shadow instances matches the CPU replay of the same instances within a
  **measured** tolerance (ADR 0007 decision 8) - report the tolerance's actual numbers (max and
  mean per channel, the pixel count over the threshold), the 4090 and the Radeon both named, and
  write them into the ADR before this stage is accepted. Do not guess the numbers first.
- **Seam proof required:** one case with an instance that spans two chunks, drawn once (not
  blended twice) with the scissor in place; the same case with the scissor removed must show the
  double blend, proving the scissor is load-bearing, not redundant.
- A mutation control (one wrong instance in the uploaded buffer) must exceed the tolerance.
- Switch off: 0 digest change anywhere (`--png`, every recorded digest, every existing test).
- Units and ships stay on the old path this stage (a later stage names when they move - see
  below). Size: should fit well under 3 h, now that it is shadows-only with no bake-filter change.

**3a2 - the real win: `Class::Z` objects, with depth and clip, carried into the direct pass.**
- Adds `cb_depth_at`'s per-pixel depth test (against `cb_terrain`/`cb_atlas_z`, as
  `chunk_build_cs` does today) and the per-record clip rect to 3a's instanced draw, so real
  objects (`Class::Z` - the only non-`None` class measured on either scene; `V`/`H` were 0 on
  both) draw through the direct pass instead of the baked chunk. This is where objects actually
  leave the baked path, and where the cross-layer painter-order risk (a direct-drawn object and a
  still-baked one needing the same relative stacking) has to be resolved, not assumed away as it
  could be for shadows in 3a.
- **Done when:** same contract as 3a (measured tolerance, a mutation control, switch off = 0
  digest change), on the same two scenes, now covering `Class::Z` objects with no exclusion list.
- Right after 3a, before 3b (ground/per-pixel planes need a real z-plane to test against, which
  3a2 is what makes available on the direct path). **If this stage comes out over 4 h, split it at
  its first natural seam** - depth first (objects draw correctly against terrain/each other, no
  clip yet), then clip (the local/global canvas split) - rather than shrinking the done-criterion.

**3b - the ground pass and the per-pixel planes (`base`, `field`, `terrain`, `submerged`), same
contract as 3a** (a measured tolerance switch-on, 0 digest change switch-off, a mutation control).

**3c - the ghost and placement preview as instances; retires the patch pipeline; closes OQ-775.**
After 3a2, not 3a alone: a placement ghost is an ordinary building (`Class::Z`), so it needs
3a2's depth/clip, not just 3a's shadows-only mechanism.
- The placement ghost/preview (today's `ObjectsPatch`/`patch_*_fs` shaders, UI-GHOST-2) becomes
  ordinary instances drawn by the direct object pass, instead of a separate patch pipeline with
  its own frame group and texture budget - this is what resolves OQ-775 (above): there is no more
  `patch_submerged_fs` to have a 14-texture ceiling.
- Done when: the ghost/preview draws pixel-identical (within 3a2's own measured tolerance) through
  the ordinary object pass, the patch pipeline (`patch_objects_pipe`/`patch_submerged_pipe`, their
  frame/group layouts) is removed, and OQ-775 is closed in `docs/open-questions.md`.

**3d - UI-RANGE-1 B2's draw-time darkening.** Also after 3a2 (ordinary objects, not shadows, are
what darkens). Input from the UI lane's B2 note, confirmed from the GOG capture (OQ-907): the
original darkens **per object** (trees on lit tiles keep their crowns lit over dark ground), so
per-anchor-tile is right as designed below - not switchable to per-pixel, that hedge is dropped.
- An object whose anchor tile is outside the currently-shown range darkens to 5/8 per channel -
  not baked into the resolved instance, since which tiles are dark changes every frame (the
  player's current selection/hover), while the instance data is only patched on a building
  change. So: `StaticInstance` carries the object's **world tile** alongside the screen anchor it
  already has (one or two more fields, resolved once, from `Prepared::first.x/y` - no extra
  resolve cost). The darkening itself is a draw-time colour multiply against whatever
  `range_view` the frame is currently showing: bit-exact 5/8 in the CPU reference's blit, within
  3a/3b's own measured tolerance in the GPU shader (the same shape `object_in_water`'s per-pixel
  lookup already uses for a different per-pixel condition) - never baked into the instance data,
  and never a reason to patch more often.
- Done when: a lit and a dark object of the same type draw identically except for the 5/8 factor,
  bit-exact on the CPU reference, within tolerance on the GPU; a frame where the shown range
  changes (a different selection) redraws the darkening with no patch of the instance data.

**3e - units and ships move to the instance path.**
- The other thing the old per-chunk object draw still owns once the ghost moves off it (3c); both
  are small, bounded sets per frame, unlike the (patched) static-object buffers, so a separate,
  simpler instance source (not chunk-bucketed, not patched the same way).
- Done when: units/ships draw from this path with the same measured-tolerance/mutation-control/
  switch-off-zero-digest contract as 3a, and the old per-chunk unit draw is removed.

## Common rules
clang-format 23.1.3; WSL `check_unix_warnings.sh`; REPORTS entry per stage (~60 lines); exact-head
CI; PC-quiet check before any GPU/long run - ask for "PC free"; no reviewer subagents (ORCH III
reviews); push per-stage branches, never `main`.
