# ADR 0007: The camera moves over resident content; the world is drawn directly every frame

- **Status:** Proposed, 2026-11-13, by the Claude Code orchestrator. The owner approves or changes it. The owner's go of
  2026-11-13 ("Yes, both") covers *drafting* it, and the measurement and quick fixes that come first. It does not cover
  the change itself.
- **Date:** 2026-11-13
- **Evidence:** [`docs/render/zoom-smoothness.md`](../render/zoom-smoothness.md): the root causes, with the owner's log,
  the design documents and the code read, and the external research with sources.

## Context

- **The owner's requirement** (playtest of 2026-11-13): zooming and moving the camera must feel smooth, and "no
  further rendering of any sort should appear while scrolling". Islands must be drawn before they come into view, not
  after.
- **Why the renderer cannot meet it.** It bakes the static world into 512-pixel chunks, **one set per zoom level**,
  built on demand by CPU workers (100–250 ms each) and uploaded under a frame budget
  ([`iso.md`](../render/iso.md), [`gpu-chunks.md`](../render/gpu-chunks.md)). A missing chunk is drawn as a stand-in,
  which is another level scaled, and then dissolves in. So every camera move turns into work, and late work shows as a
  fade.
- **The enhanced water is rebuilt per level:** the wave window, the flow lattice, the object-in-water effect and the
  sea pattern. So a level change also changes the water.
- **What the owner's log shows while zooming:**
  - up to 341 of 860 frames with a visible chunk not built yet;
  - up to 491 dissolves;
  - frames above the 2 ms of the owner's 500 Hz display.
- **How shipped games solve it:**
  - shipped 2D games (Factorio, RimWorld, OpenTTD) draw sprites **every frame from GPU-resident atlases**, and cache
    only draw lists or geometry, which a zoom does not invalidate;
  - fallback-and-fade systems hide pop-in but cannot remove it: virtual texturing at Crytek and id Software, and web
    maps;
  - only content that is guaranteed resident ahead of the camera removes it.
- **Our data fits that approach:**
  - every sprite frame of any original scenario, all three sets, is about **200 MiB**, 330 MiB with enhanced water's
    copies (`gpu-chunks.md` §2, measured);
  - a scene has at most about **100,000 static instances** (`Single_01`: 101,152).

  Both are small for a GPU, the integrated one included.

## Decision (proposed)

1. **A camera move is a transform, never a content change.** Nothing is built, uploaded, faded or resimulated
   *because* the camera moved.
2. **The interactive renderer draws the static world directly every frame:**
   - **sprites** from **resident atlases** of all three original sprite sets, with premultiplied alpha, dilated
     padding and mip levels;
   - **draw lists:** per island, the instance list in the painter's order the renderer already defines (`iso.hpp`
     `prepare_objects`: per island by `(x + w + y + h, x)`, islands in layout order), built **once at load** on the
     workers, culled per chunk and patched when a building changes;
   - **the ground** as a tile mesh over a texture array;
   - **the per-pixel planes** the compositor needs (`base` for the object-in-water effect, `field`, `terrain`) written
     by the same passes, not baked.
3. **The art set is a function of scale,** chosen before the frame: the finer set at or above screen scale, minified
   with mips, with hysteresis at the switch. An optional narrow blend band driven by *scale* is allowed. A fade driven
   by *time* or readiness is not.
4. **No stand-ins and no dissolves.** A chunk's instance list is guaranteed at load. If one is ever missing, that
   chunk is drawn from the island lists directly, never from another level.
5. **The enhanced water lives in world space:**
   - one wave simulation and one flow field over the world area around the view, at a fixed world resolution,
     independent of the level shown;
   - sampled by world position and mip-filtered when minified.

   A zoom changes nothing in its state. The "original" water stays as it is.
6. **The window thread does no on-demand work while the camera moves:**
   - decoding, planning and resize resources happen at load, on workers, or ahead of time;
   - the window keeps rendering during Windows' modal move/resize loop (SDL3 main callbacks or an event watch).
7. **An optional screen-space terrain cache** may be added for weak GPUs, and only one whose miss falls back to the
   direct path. A miss may cost time, never correctness.
8. **The test contract changes on the interactive path.**
   - **Today:** T11 requires GPU = CPU integer `over` exactly. Fixed-function blending cannot meet that (T11 decision
     C1).
   - **The CPU reference stays, and moves:** it rasterises the **same instance lists**. The GPU must match it within
     a stated tolerance per channel at integer scales, and within a stated tolerance and pixel share at fractional
     scales. The exact numbers are set by RENDER-DIRECT-3's measurement and written into this ADR before it is
     accepted.
   - **The headless `--png` path, its digests and `anno-dump` stay exact,** on the CPU.
   - **The camera sweep of ZOOM-MEASURE-1 is the acceptance test:**
     - 0 frames with non-ready content;
     - 0 stand-ins;
     - 0 dissolves;
     - no busy frame above 2 ms on the RTX 4090 at 2560 × 1369;
     - a stated budget on the iGPU.

## Consequences

- **The baked chunk path** (T1d/T1e stand-ins, dissolve, derived levels) leaves the interactive default when the
  direct path passes the sweep. It stays behind a flag until then, and is removed after one release with no use.
- **VRAM:** about +33 % for the mips over the resident sets, so about 270–440 MiB for an original scenario. Mods and
  larger maps need an atlas residency budget; the T11 LRU stays for that.
- **Weak GPUs.** Overdraw at high resolution, not the sprite count, is the cost. The mitigations are hull polygons for
  large sprites, a render-scale option and nearest-finer mip selection, each measured on the iGPU.
- **Draw order:**
  - the painter's order stays the approximation it is today (OQ-026: the original's per-pixel depth brushes are not
    reproduced either way);
  - sprites that cross chunk borders are sorted per island, not per chunk, so the order does not change at chunk
    borders.
- **Art-set switch:** if the three original sets differ in detail at a switch, the change is visible at that scale
  only, the same at every visit, and it never comes later than the zoom. Whether to blend in a narrow band is decided
  on the build, by the owner.
- **The enhanced water's look will differ slightly** from today's per-level look. It is opt-in and not original, so
  this is a QoL-layer change, shown to the owner on a build before it becomes the default.

## Work (each one packet, after the quick fixes ZOOM-MEASURE-1 and ZOOM-SMOOTH-1)

1. **RENDER-DIRECT-1:** resident atlases of all three sets with premultiplied alpha, dilated padding and mips. Measure
   VRAM per scenario.
2. **RENDER-DIRECT-2:** per-island instance lists at load and their patching on a building change. The CPU raster of
   the lists equals today's CPU chunk for a static scene, exactly.
3. **RENDER-DIRECT-3:** the direct object and ground passes, and the per-pixel planes. Measure the tolerance against
   the CPU reference, and write it into this ADR.
4. **RENDER-DIRECT-4:** the art set chosen by scale, with hysteresis, and the optional scale blend shown to the owner.
5. **RENDER-DIRECT-5:** the enhanced water in world space.
6. **RENDER-DIRECT-6:** the direct path made the default, stand-ins and dissolves removed from it, and the sweep at 0.

## Questions for the owner

1. **Do you accept the test contract in decision 8?** Exact on the CPU and for `--png`; a stated tolerance for the
   GPU's interactive frames.
2. **At an art-set switch: a hard switch with hysteresis, or a narrow blend band?** It can be decided on a build at
   RENDER-DIRECT-4.
3. **Should the old baked path stay as a selectable option after the switch,** for comparison, or be removed?

## Not decided here

- The original's per-pixel depth brushes (OQ-026).
- Figures drawn every frame (PLAY-LIVE-1c draws them with the existing sprite pass; this ADR makes that pass the main
  one).
- The UI scale (ADR 0004 R3, decided 2026-11-13: scale to the window).
