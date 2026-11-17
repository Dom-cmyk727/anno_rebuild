# Resident sprite atlases: what they cost, and the layout (RENDER-DIRECT-1a)

Task [`docs/orchestration/tasks/render-direct-1a.md`](../orchestration/tasks/render-direct-1a.md), [ADR 0007](../adr/0007-direct-rendering.md) step 1, first half. Replaces `gpu-chunks.md` section 2's own figure ("about 200 MiB for any original scenario, 330 MiB with enhanced water's copies"): that count came from an uncommitted script, covered only the 31 single-player scenarios, and had no padding, no mips and no packing loss. Everything below is measured with the engine's own code (`bench_frame --atlas-report`, `src/tools/bench_frame/main.cpp`), over the whole scene corpus, or is marked as a choice.

Evidence tags as `gpu-chunks.md`: **M** measured (here), **C** code read at `file:line`, **?** open question.

## 1. The command

`bench_frame <game_dir> <file> --atlas-report <out.tsv> [--atlas-gpu-limits]` (a mode of `bench_frame`, not a second tool: it reuses `load_any_scene`, `prepare_objects`, `animation`/`frame_index` and `AtlasResidency` directly, so there is no second implementation of scene loading, object preparation or packing to drift from the real one). `<file>` **"-"** walks the whole corpus (`<game_dir>/Scenes` recursively, plus the repo's `data/maps/lan5.txt` and the three `data/maps/candidates/ship-start-v2/*.json` candidates, resolved against the current working directory — run it from the repo root); any other `<file>` measures only that one scene, for a quick check. `--atlas-gpu-limits` additionally opens a real GPU device per adapter (step 6) — the orchestrator's go was asked for and given before this ran on the shared PC.

Decodes nothing it does not need: static and water frame sizes come from `formats::figuren::Frame::width`/`height` (file headers); only the figures a scene's units actually use are decoded (`scene.units`, already populated by `load_scene`), and only the whole-Anlagen.dat player-colour check (§5) decodes pixels, because that question is specifically about pixel content.

**Run** (M, native MSVC, `game/app` the shared game directory): 519 scenes (515 `.sze`/`.scp` under `Scenes/`, `lan5.txt`, 3 `ship-start-v2` candidates) in 7m12s wall, 0 refused. `--atlas-gpu-limits` (separately, one scene): the two GPUs below in a few more seconds.

## 2. Static and water frames, per scene and group

Columns measured per scene × group (0/1/2): distinct static frames and their bytes (dedup by `(ANLAGEN id, group, rotation-block step)`, `SpriteCache`'s own granularity), static *instances* (placed objects, not distinct frames — reuse is high: a scene can place the same building hundreds of times), distinct water frames/bytes (non-shadow and shadow counted separately), and the enhanced water's extra copies.

**Full animation, not one time sample (M, C).** A placed object's frame cycles through `frame_index`'s formula (`iso.cpp:350-357`): `step = (time_ms/ms + phase) % n` when `ms > 0` (animated: every residue 0..n-1 appears as time passes, independent of `phase`), or the single constant `phase % n` when `ms == 0` (no animation — one frame, not `n`). `bench_frame`'s `add_object_frames` enumerates exactly this set by calling the same public `animation()`/`frame_index()` functions `place_prepared` itself uses (iso.hpp), not a second implementation of the selection: this is why water objects contribute every frame of their cycle, and why a non-water entry that happens to carry real animation (`ms > 0`) is counted the same way — more complete than the superseded script, which only expanded animation for water.

**Enhanced water's extra copies (M, C).** `Scene::sprites_in` (`scene.cpp:927-932`) gives a water (not shadow) frame the smoothed variant only when style is `Enhanced`; `build_island_lists` (`scene.cpp:396-431`) excludes the surf sprite (`kSurfAnlage`) and river/waterfall pieces (`kRiverAnlagen`, `kRiverFallAnlagen`) from the enhanced lists — rivers become the procedural river surface and surf is drawn by the sea itself, so their frames are never doubled. `bench_frame` mirrors this exact exclusion (the same three constants, both public) to count only the frames that really get a second, smoothed copy; shadows never do (the smoothing only ever applies to non-shadow frames).

**Worst case across every page/padding/mip combination (M):** `Scenes/0010SingleEndlos/01_1024_easy_256.sze`, group 0 — 1,611 distinct static frames (101.9 MB) over 125,427 instances, 504 distinct water frames (77.7 MB, no shadows), 54 enhanced-water frames eligible for a smoothed copy (8.3 MB). One live ship (113 KB).

## 3. Packing: the shelf packer, four candidates, with and without mips

Packer: `anno::render::AtlasResidency` (`residency.hpp`/`.cpp`), the shelf packer `gpu-chunks.md` 4.2-4.3 already specifies and RENDER-DIRECT-1b is to build on — not a new one. One `ensure()` call packs the whole static+water set of a scene/group at once (budget sized so it never has to evict within that call), so the page count is a real packing result, not an area estimate; `ensure()`'s own tallest-first sort makes the layout independent of listing order (confirmed by the existing `residency_test.cpp` case). Four candidates: page size {2048, 4096} px, padding {2, 4} px added to every frame's width and height (both sides) before packing — the dilation ADR 0007 wants; whether 2 px is enough for 1c's actual dilation kernel is for 1b/1c to confirm, not measured here. Mips do not change the page *count* (mip levels are generated by the GPU from the resident base page, not packed separately); they add roughly **4/3** of the base layer bytes (the geometric series 1 + 1/4 + 1/16 + ... for a square power-of-two texture — a formula, not a measurement, and marked as one in the TSV's own column names: `pack_bytes` is base-or-mip depending on the `mips` column, `rect_bytes` is always the un-mipped packed-rectangle total).

`01_1024_easy_256.sze` group 0, static + water combined, every candidate (M):

| page | pad | mips | pages | pack_bytes | rect_bytes | fill |
|---|---|---|---|---|---|---|
| 2048 | 2 | none | 12 | 192 MiB (201,326,592) | 180.0 MiB | 93.7 % |
| 2048 | 2 | full | 12 | 256 MiB (268,435,456) | 180.0 MiB | 93.7 % |
| 2048 | 4 | none | 13 | 208 MiB (218,103,808) | 189.0 MiB | 90.8 % |
| 2048 | 4 | full | 13 | 277.3 MiB (290,805,077) | 189.0 MiB | 90.8 % |
| 4096 | 2 | none | 3 | 192 MiB (201,326,592) | 180.0 MiB | 93.7 % |
| 4096 | 2 | full | 3 | 256 MiB (268,435,456) | 180.0 MiB | 93.7 % |
| 4096 | 4 | none | 4 | 256 MiB (268,435,456) | 189.0 MiB | 73.8 % |
| 4096 | 4 | full | 4 | 341.3 MiB (357,913,941) | 189.0 MiB | 73.8 % |

`fill` = rect_bytes / pack_bytes (un-mipped): how much of the allocated pages the packed rectangles actually cover; `1 - fill` is shelf waste, not a measurement error.

## 4. Fit against the two GPUs' limits — and the budget

`--atlas-gpu-limits` (M, `wgpuDeviceGetLimits`, `headless_context`, this PC, 2026-11-17):

| adapter | maxTextureDimension2D | maxTextureArrayLayers | maxBufferSize |
|---|---|---|---|
| NVIDIA GeForce RTX 4090 (Vulkan) | 32,768 | 2,048 | 1 TiB |
| AMD Radeon(TM) Graphics, the iGPU (Vulkan) | 16,384 | 8,192 | 2 GiB |

Both page-size candidates and every layer count measured (3-13) are far inside both limits: the limits are not the binding constraint. `gpu-chunks.md` 4.3 rule 6's own proposed budgets are (OQ-096 is still open): **512 MiB on the 4090, 256 MiB on the iGPU**, for static + water together, figures stated separately (section 5).

**The worst case under the recommended layout (page 2048, padding 2 px, no mips — §6) is not `01_1024_easy_256.sze`** (that scene is the worst only at 4096/4px/no-mip, a candidate this doc does not recommend): under 2048/2px/no-mip it is **`Scenes/0010SingleEndlos/02_1024_easy_128.sze`**, group 0 — **208 MiB (218,103,808 bytes)**, leaving **48 MiB of headroom** inside the 256 MiB iGPU budget (512 MiB − 208 MiB = 304 MiB headroom on the 4090). The same scene's own live-unit figures (§5) need 113 KB, trivially inside that headroom. Groups 1 and 2 are far smaller (64 MiB and 16 MiB worst case). **Mips do not fit this layout's own worst scene on the iGPU**: 2048/2px/full-mip for `02_1024_easy_128.sze` is 277.3 MiB (290,805,077 bytes), already over the 256 MiB budget — every mip-including candidate measured meets or exceeds 256 MiB for one worst scene or another (table in section 3 for `01_1024_easy_256.sze`: 256 MiB and 277.3 MiB; the global worst across every combination, 268,435,456 bytes = exactly 256 MiB, is itself a mip or a 4096/4px/no-mip row with **zero headroom** — not an acceptable fit, named here instead of silently used as "the" number). On the 4090 (512 MiB) every candidate, mips included, fits with headroom (worst measured: 357,913,941 bytes = 341.3 MiB).

**Fits, with headroom, on the chosen layout (section 6); mips are a separate, device-specific choice** — not a QUESTION, since a layout exists that fits both budgets with room to spare.

## 5. Figures (ships, carriers, soldiers)

The whole FigurenHeader.dat catalog, scene-independent, measured once (M, the same traversal `sprite_fidelity_test.cpp`'s own pixel-exact test uses — its 241,808-frames-per-group figure for group 0 matches this count exactly, a direct cross-check that the traversal is right):

| group | frames | bytes | player-colourable frames | player-colourable bytes |
|---|---|---|---|---|
| 0 | 241,808 | 1,923.4 MiB (2,017,601,516) | 35,423 | 352.1 MiB (369,259,748) |
| 1 | 241,808 | 484.9 MiB (508,682,544) | 35,423 | 88.4 MiB (92,673,260) |
| 2 | 241,808 | 108.2 MiB (113,479,232) | 35,423 | 19.1 MiB (20,013,820) |

Group 0 alone is 1.9 GiB: **the direct path cannot keep the whole figure catalog resident** (ADR 0007's own premise). What a scene actually needs is far smaller: `scene.units` (populated by `load_scene`, not by `load_placed_scene` — `lan5.txt` and the three `ship-start-v2` candidates are reported as 0 figures used for this reason, not because they truly have none) gives the exact frames a scenario's ships and soldiers use, deduplicated by `render::FrameKey::unit` (the frame table's own identity, `frame_table.hpp`). Measured across the whole corpus: the busiest scene needs a few hundred KB to a few MB of figures (the worst scene in section 4 needs 113 KB); most need far less. This is well inside the headroom section 4 found, so figures do not change the fit conclusion, but they are **not included in the static+water packing** above — ADR 0007's own figures/hot class is a separate residency budget (`gpu-chunks.md` 4.2's Static/Hot split), and RENDER-DIRECT-1b decides how it is packed.

## 6. Player colours

**Static (Anlagen.dat) frames: structurally never recoloured (M, C), and a real finding about their pixel content anyway.**

- `anno::render::SpriteCache::get` (`iso.hpp:276-277`) — the only place that decodes a static frame for drawing — has no colour parameter at all; it calls `assets::building::to_rgba(anlagen, entry, group, frame)` (`iso.cpp:322`), which also has no colour parameter (`building.hpp`). `assets::figure::player_coloured`/`recolour` are called only from `assets/src/unit.cpp` (lines 180, 207), the live-unit path, never from `iso.cpp`'s static draw path. So today, 0 static frames carry a player colour — this is the engine's current behaviour, not a property of the files.
- **Decoded anyway** (`bench_frame`'s `static_player_colour_eligibility`, using `assets::building::to_rgba` — the same function `SpriteCache::get` calls, so the decode path is the real one, not a reimplementation): of the whole Anlagen.dat catalog's 27,537 non-shadow frames, **11,180 frames (40.6 %) have at least one pixel** that `figure::recolour`'s own test (`G² > trunc(1.5·(R² + B²))`, applied by comparing the decoded colour against `recolour(colour, test_player)` rather than reimplementing the formula) would match — **3,760,786 of 102,530,209 checked pixels (3.67 %)**. This is not nothing: a meaningful share of building pixels would be recoloured if the test were ever applied to them, even though nothing in the engine does that today. Flagged here as a finding, not acted on (changing what gets recoloured is a look/feel decision for the owner, CLAUDE.md item 7).

**Figures: 69 of 368 FigurenHeader.dat entries** (17 BEWOHNTYPE names, `figure.hpp`'s own `kPlayerColouredTypes`), 35,423 of 241,808 frames at group 0 (section 5's table; ships are not among the coloured types). Each decoded, coloured copy is a separate `SpriteKey`/`FrameKey` (variant folds the colour into the key), so holding every colour used is literally one decoded image per (frame, colour) pair.

**Two ways to hold a figure's player colour (measured bytes of each, not chosen for the owner):**
1. **One decoded copy per colour a scene uses.** Bytes = (player-colourable bytes a scene's units actually need) × (distinct colours the scene's players use, up to 8 in the original). For the worst scene found (section 4), the figures actually used are 113 KB for one colour; at up to 8 colours, worst case ≈ 904 KB — still trivial against the headroom in section 4.
2. **One uncoloured copy plus a mask, coloured per instance** (T11 section 10 item 3): bytes = the player-colourable bytes a scene's units need, once, regardless of how many colours are in play, with the colour applied at draw time (a shader uniform per instance, not a GPU-memory cost measured here).

Both fit comfortably inside the headroom section 4 found for every scene measured; this is a performance/complexity trade-off (one atlas entry per colour vs. a per-instance shader parameter), not a memory one — the owner's call, not decided here.

## 7. The layout

| Choice | Value | Reason |
|---|---|---|
| Packer | `AtlasResidency` (shelf) | Already specified (`gpu-chunks.md` 4.2-4.3) and tested; no reason to design a second one for 1b to build on. |
| Page size | 2048 px | Fits every measured scene/group with headroom (section 4); 4096 needs fewer pages but the same or more bytes for the worst cases measured (section 3), with no benefit found. **Choice.** |
| Padding | 2 px | Fits with more headroom than 4 px (section 3) for every scene measured. Whether 2 px is enough for 1c's actual dilation kernel is **not measured here** — 1b/1c confirm it against the real upload/sample code. **Choice, flagged.** |
| Mip chain | None, for the static+water atlas | Every full-mip-chain candidate measured meets or exceeds the 256 MiB iGPU budget for some worst scene (section 4); the no-mip 2048/2px layout fits both budgets with headroom. **Choice**, driven by the iGPU measurement — the owner may still choose mips on the 4090 alone (512 MiB, fits with headroom even with mips) as a device-specific quality option; not decided here. |
| Premultiplied alpha | Not measured here | Changes how pixels are stored, not how many bytes or pages — out of this task's scope (RENDER-DIRECT-1b/1c). |
| Shared vs. separate pages per sprite set (buildings/water/figures) | Separate (Static class: buildings + water; figures their own class) | Figures (section 5) are two to three orders of magnitude larger as a whole catalog and change per scene by a different mechanism (`scene.units`, not `prepare_objects`); mixing them into one residency pool would let a busy figure scene evict static chunk frames the next island draw still needs. Matches `gpu-chunks.md` 4.2's existing Static/Hot split. **Choice.** |
| Player colours | Not chosen here (section 6) | Both storage strategies fit; it is a performance/complexity trade-off, not a memory one. |
| Enhanced water's extra copies | Counted, not pre-allocated specially | Section 2/3's packing already includes them (the smoothed variant is just another distinct `SpriteKey` in the same combined set); no separate budget line needed. |

## 8. No render-path change

This task changed `src/tools/bench_frame/main.cpp` (a new `--atlas-report` mode) and `src/render/tests/residency_test.cpp` (a new unit test); nothing under `src/render/src`, `src/render/include` (other than the header-only `residency.hpp`'s own, pre-existing public API, unchanged), `src/sim`, `src/app/src` (other than `bench_frame`'s own tool), or any digest/picture path was touched. `git diff --stat` against `origin/main` confirms this.
