# Smooth zoom and camera: root causes and the way out (2026-11-13)

**Why this exists.** The owner's playtest of 2026-11-13 (preview build of `task/PLAY-LIVE-1a` `9e920c18`, RTX 4090,
2560 × 1369, a 500 Hz display, enhanced water) found three problems:
- islands appear a moment *after* a zoom;
- placeholder images fade into the real ones;
- the enhanced water shows artifacts while zooming.

The owner's requirement: *"The scrolling should feel smooth, no further rendering of any sort should appear while
scrolling and islands should be rendered so they do not render AFTER scrolling."*

This note names the causes, with evidence from the owner's own log, the design documents and the code. It then
describes how the problem is solved properly, and in what order.

**Evidence tags** as in [`iso.md`](iso.md):
- **M:** measured, here the window's own statistics lines from the owner's session;
- **C:** code read at `main` `961a9d4a`;
- **D:** a design document;
- **R:** external research, with its source.

## 1. What the owner's log shows (M)

The window prints one statistics block per second. Over the sessions of 2026-11-13:

| Interval (zoom level shown) | Frames that drew a visible chunk **not built yet** | Most at once | Chunks **dissolved in over a stand-in** |
|---|---|---|---|
| level 4, session A | **341 of 860** | 52 | 491 |
| level 4, session B | 216 of 847 | 63 | 302 |
| level 4, session B, earlier | 163 of 970 | 32 | 321 |
| level 0, session A | 87 of 947 / 94 of 960 | 29 | 221 / 305 |
| level 3, session A | 93 of 962 | 8 | 220 |
| level 1, session A | 64 of 941 | 49 | 247 |
| any level, camera at rest | 0 | 0 | 0 |

**Late content happens only while the camera moves,** and it is worst when zooming out to the far levels (3 and 4).

**The frame time is not the problem.** The average busy time per frame is 0.4–1.6 ms at 300–500 fps. But single
frames spike:

| Step on the window thread | Worst seen |
|---|---|
| `sprites` | 8.8 ms |
| `chunks` | 9.2 ms |
| `finish` | 6.5 ms |
| `record` | 5.9 ms |
| `surface configure`, on a resize | 7.8 ms |

On a 500 Hz display a frame has 2 ms, so each such spike drops 2–7 frames in a row: a visible stutter while the camera
moves. Two **0.67–0.69 s hitches** were spent entirely in "handling window events".

## 2. How the renderer works today (D, C)

- **Baked chunks per level.** The static world (ground, buildings, trees) is baked into **512-pixel chunks, one set per
  zoom level**:
  - levels 0–2 are the original's three sprite sets;
  - levels 3 and up are "derived": each chunk is the 2 × 2 box reduction of four chunks of the next finer level
    ([`iso.md`](iso.md), "Continuous zoom, stage 3").
- **Builds and uploads.** A chunk is built on CPU worker threads in **100–250 ms** (`iso.md`, "Pan while zooming"),
  then uploaded **a plane at a time** within 1 ms per frame (`upload_budget_ms`).
- **Continuous zoom resamples one level.** The picture is composed on one level's pixel grid and resampled to the
  window (`scale_fs`). That level is the coarsest whose tile is at least as wide as the zoom asks, with 5 % hysteresis.
  Within one level the composed grid grows up to **2.2 × the window per axis** as you zoom out (`kMaxGridRatio`).
- **Missing chunks.** A visible chunk that is not on the GPU is drawn as a **stand-in**: the same area from another
  level, scaled with nearest-neighbour sampling, and aliased on the first zoom-out. When the real chunk arrives it
  **dissolves in over 150 ms** (`chunk_fade_ms`).
- **Prefetch** ([`frame.cpp`](../../src/render/src/frame.cpp) `FrameRenderer::plan_margin`, C) covers:
  - the visible chunks;
  - a ring of **one chunk** plus a look-ahead in the direction of a *pan* (at most `max_lookahead_chunks` = 3);
  - the view **one level step** away, around the cursor.

  Prefetched chunks are uploaded **one per frame**, and only in frames where no visible chunk waits
  (`prefetch_per_frame` = 1).
- **The cache** keeps the visible and planned chunks plus **48 spares** (`spare_chunks`). Derived levels stay resident
  only when the whole scene fits in **16 chunks** (`resident_level_chunks`).
- **The enhanced water is built per level:**
  - the wave simulation runs on a window of one level's grid, and is replaced on every level or size change with a
    **250 ms cross-fade** from the analytic waves (`wave_fade_ms`);
  - the river flow lattice exists only on built levels; stand-ins and derived levels show the legacy rivers;
  - the object-in-water effect is off on stand-ins and derived levels;
  - the sea pattern is a per-level image.

## 3. Root causes

**RC1. A zoom is a content change, so smoothness depends on predicting and building content in time (C, M).**
- Each level is a different baked picture, built on demand.
- **The prefetch does not cover the zoom-out.** It prepares a pan and one level step. It does not prepare the area
  that opens up while zooming *out within* a level, where the composed grid grows from 1 to 2.2 × the window per axis.
  A pure zoom at the cursor is deliberately not counted as motion, so the velocity look-ahead is about zero. The ring
  is therefore one chunk, and a few wheel notches open more than one chunk per side.
- **Getting a missing chunk onto the screen is slow:**
  - 100–250 ms to build;
  - more for a derived chunk, whose four children must exist first;
  - then a plane-by-plane upload;
  - then the 150 ms dissolve.

  That adds up to the "islands took a second to appear".
- **The small cache** makes returning to a level you just left a rebuild.

**RC2. The fallbacks are visible by design (D).**
- **Stand-ins** are another level scaled, aliased on the first zoom-out, and **dissolves** are 150 ms cross-fades. Both
  are exactly the "rendering after scrolling" the owner rejects.
- They were added in T1d/T1e to replace *open sea* holes. They are better than holes, but a fade is still a visible
  change after the camera stops.

**RC3. The water and the art change at level boundaries (D).**
- **Built levels 0–2 are three different sprite sets** drawn by the original artists at three scales. Switching the
  composed level swaps the art.
- **The enhanced water swaps more at a level change:**
  - the wave simulation window, with a 250 ms cross-fade to analytic waves;
  - simulated against analytic waves, between built and derived levels;
  - the flow lattice against the legacy rivers;
  - the object-in-water effect on against off;
  - a different sea-pattern image.

  These are the enhanced-water "artifacts" while zooming. The log's `wave windows N new` counter rises in the zooming
  intervals, as expected.

**RC4. Window-thread spikes against a 2 ms frame (M, C).**
- On-demand work runs on the window thread in some frames:
  - sprite frames decoded on first use (the T11 note says decoding is on the window thread for the compositor's
    sprites);
  - chunk planning;
  - command recording;
  - the swapchain rebuild on a resize.
- At 500 Hz any frame above 2 ms is a dropped frame.
- The 0.7 s "handling window events" hitches fit the Windows modal loop of a window move or resize. The live-resize
  path is not measured yet.

**The root under all four.** The renderer turns a camera move into *work*: build, upload, fade, resimulate. It then
hides late work with visible fallbacks. A smooth camera needs the opposite: everything the camera can reach is
already resident, and a camera move only changes a transform.

## 4. How it is solved properly

(Section 5 adds the external research. This section is the orchestrator's reading of the design and the evidence.)

**Principle: zoom and pan are a camera transform over GPU-resident content.** Nothing is built, uploaded, faded or
resimulated *because* the camera moved. Work that content needs happens before it can become visible, or
continuously in world space.

Two architectures meet it:

**A. Draw the world every frame from GPU-resident sprite atlases.** This is the standard for modern 2D/2.5D engines;
see section 5.
- **The memory fits** ([`gpu-chunks.md`](gpu-chunks.md) §2, M): every sprite frame of any original scenario, all three
  sets, is about **200 MiB** (330 MiB with enhanced water's copies).
- **The draw count fits.** A scene has at most about 100,000 static instances (`Single_01`: 101,152), with at most
  3,608 per chunk. Instanced drawing of that many quads per frame is small work, even for an integrated GPU.
- **How it works:**
  - per island, a static instance buffer in painter's order, built once at load and patched when a building
    changes;
  - the ground as a tile mesh over a texture array;
  - sprites from mipmapped atlases (premultiplied alpha, padded frames) at whichever original set fits the zoom;
  - a set switch is a texture choice, made *before* the frame, with nothing to build.
- **What it costs:**
  - the current exact "GPU = CPU integer `over`" contract (T11 decision C1) cannot hold for fixed-function blending,
    so the interactive path's tests move to a tolerance against the CPU reference, which stays;
  - the original's depth brushes stay a painter's approximation, as today.

**B. Keep baking, but make readiness guaranteed.** The quick-win path, with the current machinery:
- prefetch the **whole zoom range** reachable from the current view (the far end of the current level and the next
  level, around the cursor);
- size the cache to the GPU, gigabytes on a 4090 and a budget on an integrated GPU, so returning to a level is free;
- keep the coarse derived levels resident;
- upload prefetched chunks as fast as the frame budget allows;
- **never show a stand-in or a dissolve**: hold the zoom at the edge of ready content, as is already done for
  zooming out past an unready coarser level.

B reduces the misses but cannot remove them on a large map at level 0. The ship-start map is 1,431 × 1,431 tiles,
and at level 0 that is thousands of chunks of 3.75 MiB each. A misprediction still shows, as a hold.

**For the water (both A and B):** simulate and shade in **world space**, independent of the level shown. One wave
field and one flow field over the visible world area, sampled by every level, so a zoom changes nothing in the water's
state. Analytic and simulated waves should not swap at level boundaries.

**For the stutter:**
- move every on-demand step off the window thread: decode sprites at load or on workers; plan chunks on workers;
  pre-create resize resources;
- measure the 0.7 s window-event hitches with the window trace before fixing them;
- smooth the zoom ease in log space toward the cursor, as now.

## 5. External research (R)

The research subagent's report of 2026-11-13. Its sources are primary: developer blogs, engine source, talks. Items
marked "unverified" there are left out here.

- **Shipped 2D games draw sprites every frame from resident atlases.** Where they cache, they cache **draw lists or
  geometry, which a zoom does not invalidate**, not pixels.
  - **Factorio:**
    - entities are batched from atlases every frame, about 25,000 sprites at full zoom-out
      ([FFF-251](https://www.factorio.com/blog/post/fff-251));
    - atlases are mipmapped, with padding against mip bleeding
      ([FFF-227](https://factorio.com/blog/post/fff-227));
    - its pixel cache for terrain was **invalidated by every zoom** and was replaced by per-chunk *draw orders*:
      "changing render scale doesn't need to invalidate the new cache", with prefetch and parallel builds
      ([FFF-323](https://factorio.com/blog/post/fff-323), [FFF-333](https://www.factorio.com/blog/post/fff-333)).
  - **RimWorld** prints static things into section meshes. **OpenTTD** generates missing zoom levels of sprites at load
    and keeps an LRU sprite cache. **OpenRA** renders at 1:1 and filters, with the buffer capped at twice the window.
- **Streaming systems hide pop-in; they don't remove it.**
  - Virtual texturing (Crytek, id Tech 5) and web maps (Mapbox) fall back to coarser data, then blend the finer data
    in, exactly as our stand-ins and dissolves do. id Tech 5 admits a "visible snap".
  - Only a working set that is **guaranteed resident ahead of the camera** removes pop-in.
  - WebGPU has no sparse textures, so virtual texturing would be a software indirection.
- **Choose the art set by scale, not by readiness.**
  - Select the finer set at or above screen scale and minify it with mips; add hysteresis at the switch.
  - A blend between two sets as a function of *scale* in a narrow band is stable and reversible.
  - A blend as a function of *time since a chunk became ready* is the anti-pattern we have.
  - StarCraft: Remastered requires its 2× set to equal its 4× set at half scale, so a set switch shows no detail
    change.
- **Water in world space.** Factorio's water shader uses world positions and global time, so a zoom changes nothing.
  A screen-space cache is what a zoom breaks.
- **The direct path's real cost is overdraw × resolution, not the sprite count.**
  - 100,000 sprites drew in about 2 ms of GPU time on GPUs from 2012 to 2019
    ([Cold Bytes Games](https://www.coldbytesgames.com/blog/sprite_pipeline/)).
  - For weak GPUs Factorio uses hull polygons for large sprites, compression, 16-bit targets and a render-scale option
    ([FFF-281](https://www.factorio.com/blog/post/fff-281)).
- **Pixel-art details:**
  - premultiplied alpha, so filtering makes no fringes;
  - dilated atlas padding against mip bleeding;
  - a "fat pixel" filter for fractional magnification, sharp without shimmer;
  - mips or area filtering below 1:1, against moiré;
  - shared integer edges and a pixel-snapped camera, against seams.
- **Frame pacing:**
  - `SDL_PollEvent` blocks inside Windows' modal move/resize loop. SDL3's main callbacks, or an event watch that
    redraws on `SDL_EVENT_WINDOW_EXPOSED`, keep frames coming
    ([SDL wiki](https://github.com/libsdl-org/sdlwiki/blob/826a965410f5316222cde91905df22fac548c3d6/SDL3/AppFreezeDuringDrag.md)).
    This is the likely mechanism of the 0.7 s hitches; to be confirmed.
  - Animate with presentation timestamps, not CPU frame deltas.
  - Damp the zoom in log space with `1 − e^(−λ·dt)`.
- **How a senior rendering engineer proceeds:**
  1. measure: per-frame zones and counters, and a **scripted, deterministic camera sweep** compared frame by frame
     with an "everything resident" reference, where any difference is a readiness bug;
  2. fix correctness that doesn't depend on caching;
  3. make the static content zoom-independent;
  4. move the water to world space;
  5. fix pacing and resize;
  6. only then add optional caches, each of which falls back to the **exact** path, never to a stand-in.

## 6. The plan (orchestrator's recommendation)

**Target architecture (A, with one cache allowed).**
- **Draw the static world directly every frame:**
  - per-chunk sorted instance lists (the painter's order the renderer already computes), built at load on the workers,
    zoom-independent;
  - all three original sprite sets resident in VRAM: premultiplied, padded and mipmapped. About 200–330 MiB; to be
    measured.
- **Choose the set** by scale with hysteresis, optionally with a narrow scale blend.
- **The water** is simulated and shaded in world space.
- **No stand-ins and no dissolves.** A missing instance list blocks that chunk's draw for the frame it takes to build
  it, which is fast because it is a list, not pixels. Pixels are never late.
- **An optional screen-space terrain cache for weak GPUs,** whose miss falls back to the direct path.
- **The CPU reference** rasterises the same instance lists. It is exact at integer scales and within a tolerance at
  fractional ones, and it is the sweep's "everything resident" oracle.
- **What this changes:** T11's "GPU equals the CPU's integer `over` exactly" contract becomes a tolerance on the
  interactive path. That needs an ADR (ADR 0007) and the owner's approval.

**The order, one packet at a time:**

| Step | Packet | What it delivers | Why first |
|---|---|---|---|
| 1 | **ZOOM-MEASURE-1** (Windows) | the scripted camera sweep at 2560 × 1369 and 1280 × 800 on the 4090 and the iGPU; its baseline; a pass criterion (0 frames with non-ready content, 0 dissolves, no busy frame above 2 ms); the live-resize hitch traced | every later step is judged by it |
| 2 | **ZOOM-SMOOTH-1** (Windows), quick wins in today's design | prefetch the zoom range (the far end of the current level and the next level around the cursor), not only a pan ring; a VRAM-sized cache; coarse levels resident; prefetch uploads to the frame budget; sprite decoding and chunk planning off the window thread; the SDL live-resize fix | the owner's next playtest is better within days. It cannot reach 0 on large maps (section 4, B) |
| 3 | **ADR 0007** (orchestrator, owner approves) | direct rendering as the interactive path; the tolerance contract; what stays exact; the removal of stand-ins and dissolves | an architecture change and a test-contract change |
| 4 | **RENDER-DIRECT-1…n** (Windows; about 4–6 packets) | resident mipmapped atlases; zoom-independent instance lists; the direct object and ground passes; set selection by scale; world-space water; a sweep at 0 | the end state |

**Not changed:** the faithful core. This is all rendering, which reads the sim's state and never changes it. The
"original" look stays the default where it is defined; the enhanced water stays opt-in.
