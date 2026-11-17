# Resident sprite atlases: what they cost, and the layout (RENDER-DIRECT-1a)

Task [`docs/orchestration/tasks/render-direct-1a.md`](../orchestration/tasks/render-direct-1a.md), [ADR 0007](../adr/0007-direct-rendering.md) step 1, first half. Replaces `gpu-chunks.md` section 2's own figure ("about 200 MiB for any original scenario, 330 MiB with enhanced water's copies"): that count came from an uncommitted script, covered only the 31 single-player scenarios, and had no padding, no mips and no packing loss. Everything below is measured with the engine's own code (`bench_frame --atlas-report`, `src/tools/bench_frame/main.cpp`), over the whole scene corpus, marked as a choice, or left as a QUESTION/open question for the owner where the measurement itself does not settle it (§4, §7).

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

## 3. Packing: the shelf packer, four candidates, with and without a mip level

Packer: `anno::render::AtlasResidency` (`residency.hpp`/`.cpp`), the shelf packer `gpu-chunks.md` 4.2-4.3 already specifies and RENDER-DIRECT-1b is to build on — not a new one. One `ensure()` call packs the whole static+water set of a scene/group at once (budget sized so it never has to evict within that call), so the page count is a real packing result, not an area estimate; `ensure()`'s own tallest-first sort makes the layout independent of listing order (confirmed by the existing `residency_test.cpp` case). Four candidates: page size {2048, 4096} px, padding {2, 4} px added to every frame's width and height (both sides) before packing — the dilation ADR 0007 wants; whether 2 px is enough for 1c's actual dilation kernel is for 1b/1c to confirm, not measured here.

Mips do not change the page *count* (mip levels are generated by the GPU from the resident base page, not packed separately). **A full chain is not a usable candidate at this padding**: a mip level *n* needs 2ⁿ px of padding on every packed frame, or its atlas neighbours bleed into it when the GPU samples past the mip's own, shrunk edge; 2 px padding supports exactly one level (2¹ = 2 px), and a genuine chain down to 1×1 for a 2048 px page would need 11 levels — 2¹¹ = 2048 px of padding, absurd. The `mips` column's two values are therefore **none** and **one_level** (+25 % of the base, the exact cost of one half-resolution level: a quarter of the base area) — not "full"; a full chain is not listed because no padding candidate measured supports it (`src/tools/bench_frame/main.cpp`'s own `kOneMipLevelFactor` comment has the derivation). `pack_bytes` is base-or-one-level depending on the `mips` column; `rect_bytes` is always the un-mipped packed-rectangle total, for the fill ratio.

`01_1024_easy_256.sze` group 0, static + water combined, every candidate (M):

| page | pad | mips | pages | pack_bytes | rect_bytes | fill |
|---|---|---|---|---|---|---|
| 2048 | 2 | none | 12 | 192 MiB (201,326,592) | 180.0 MiB | 93.7 % |
| 2048 | 2 | one_level | 12 | 240 MiB (251,658,240) | 180.0 MiB | 93.7 % |
| 2048 | 4 | none | 13 | 208 MiB (218,103,808) | 189.0 MiB | 90.8 % |
| 2048 | 4 | one_level | 13 | 260 MiB (272,629,760) | 189.0 MiB | 90.8 % |
| 4096 | 2 | none | 3 | 192 MiB (201,326,592) | 180.0 MiB | 93.7 % |
| 4096 | 2 | one_level | 3 | 240 MiB (251,658,240) | 180.0 MiB | 93.7 % |
| 4096 | 4 | none | 4 | 256 MiB (268,435,456) | 189.0 MiB | 73.8 % |
| 4096 | 4 | one_level | 4 | 320 MiB (335,544,320) | 189.0 MiB | 73.8 % |

`fill` = rect_bytes / pack_bytes (un-mipped): how much of the allocated pages the packed rectangles actually cover; `1 - fill` is shelf waste, not a measurement error.

**The group mip ladder (why one level is most of the benefit).** The zoom groups themselves already form one: tile widths 84/42/20/10 px (`docs/app/window-trace.md` "Zoom groups and their tile widths"), never shrinking the finer set by much more than 2× (2.1× between groups 1 and 2, plus hysteresis) before the next group takes over. One half-resolution mip level covers nearly all of the minification a zoom between two adjacent groups needs; a second level would matter only right at a group switch, for a small added cost on top of what section 4 already finds tight.

**`scene.sum` rows (F1, groups together).** The same per-scene, per-group frame sets above are also summed, per scene, for **groups 0+1+2** (ADR 0007 decision 2: all three sets stay resident at once) and the adjacent pairs **0+1** and **1+2** (the pair a zoom between two groups needs held together, under the owner's seamless-zoom rule — no upload mid-zoom). Written by the same command run, same `page_size`/`padding_px`/`mips` columns, `group` holding the label instead of a number; the per-group descriptive columns (`static_frames` .. `figures_used_bytes`) are 0 on these rows since they are not meaningful summed. Section 4 uses them.

## 4. Fit against the two GPUs' limits — and the budget: a QUESTION

`--atlas-gpu-limits` (M, `wgpuDeviceGetLimits`, `headless_context`, this PC, 2026-11-17):

| adapter | maxTextureDimension2D | maxTextureArrayLayers | maxBufferSize |
|---|---|---|---|
| NVIDIA GeForce RTX 4090 (Vulkan) | 32,768 | 2,048 | 1 TiB |
| AMD Radeon(TM) Graphics, the iGPU (Vulkan) | 16,384 | 8,192 | 2 GiB |

Both page-size candidates and every layer count measured (1-17) are far inside both limits: the limits are not the binding constraint. `gpu-chunks.md` 4.3 rule 6's own proposed budgets are (OQ-096 is still open): **512 MiB on the 4090, 256 MiB on the iGPU**, for static + water together, figures stated separately (§5). Rule 6 also says the discrete budget may hold every group of a scene at once, while the integrated one may evict *other groups'* frames — so the two devices are checked against different sums: the 4090 against all three groups together (ADR 0007 decision 2); the iGPU against whichever **adjacent pair** a zoom is crossing (decision 2's own consequence, combined with the owner's seamless-zoom rule: nothing may be uploaded mid-zoom, so both groups of the pair must already be resident).

**The RTX 4090 holds all three groups, with one mip level, at every candidate measured.** Worst case across the whole corpus (recommended layout, 2048 px / 2 px padding): groups 0+1+2, one_level, 360 MiB (377,487,360 bytes) on `Scenes/0010SingleEndlos/02_1024_easy_128.sze` — **152 MiB of headroom** inside 512 MiB. Every other candidate's worst (up to 4096/4096 px, one level) stays at or under 400 MiB. **Yes** for the 4090.

**The iGPU does not hold the adjacent pair for the worst scene found, on any candidate measured — even without mips.** The same scene, `02_1024_easy_128.sze`, groups **0+1**, no mips, every candidate:

| page | pad | pack_bytes (0+1, no mips) |
|---|---|---|
| 2048 | 2 | 272 MiB (285,212,672) |
| 2048 | 4 | 288 MiB (301,989,888) |
| 4096 | 2 | 320 MiB (335,544,320) |
| 4096 | 4 | 320 MiB (335,544,320) |

Every one is over the 256 MiB iGPU budget; the smallest found (2048/2 px) is **16 MiB over**, before any mip cost. The other pair, 1+2, is comfortable everywhere (worst 100 MiB with one level). This matches the review's own back-of-envelope estimate from the per-group worst cases (≈272 MiB for 0+1) almost exactly.

**QUESTION (the one the packet asks for when the pair does not fit):**
- **Scene:** `Scenes/0010SingleEndlos/02_1024_easy_128.sze`, zoom groups 0+1 (the pair a zoom between them needs both resident at once, under the seamless-zoom rule).
- **Bytes:** 272 MiB needed (2048 px page, 2 px padding, no mips — the smallest candidate measured) against a 256 MiB iGPU budget: **16 MiB over**, and every other candidate measured is worse (288–320 MiB).
- **Options, not decided here:**
  1. Raise the iGPU budget past 256 MiB (OQ-096 is already open on exactly this number; the iGPU reports 2 GiB of `maxBufferSize`, so 256 MiB was a proposed default, not a hardware ceiling) — cost: revisit OQ-096, no code.
  2. Accept a brief, visible upload on the iGPU for this one scene's worst case — cost: breaks the owner's seamless-zoom rule, the thing this measurement exists to protect.
  3. A packing strategy not measured here (per-island culling instead of whole-scene residency, a reduced-detail fallback on the iGPU, or something else) that keeps the pair under budget — cost: unmeasured design work, for 1b/1c.
  4. Accept that the iGPU does not render this scene (or scenes like it) at full static+water detail at these groups, falling back to a documented reduced mode there — cost: a device-specific degradation, visible to players on integrated graphics.

Not decided here, per the packet's own instruction; stopping with this instead of a "fits" conclusion.

## 5. Figures (ships, carriers, soldiers)

The whole FigurenHeader.dat catalog, scene-independent, measured once (M, the same traversal `sprite_fidelity_test.cpp`'s own pixel-exact test uses — its 241,808-frames-per-group figure for group 0 matches this count exactly, a direct cross-check that the traversal is right):

| group | frames | bytes | player-colourable frames | player-colourable bytes |
|---|---|---|---|---|
| 0 | 241,808 | 1,923.4 MiB (2,017,601,516) | 35,423 | 352.1 MiB (369,259,748) |
| 1 | 241,808 | 484.9 MiB (508,682,544) | 35,423 | 88.4 MiB (92,673,260) |
| 2 | 241,808 | 108.2 MiB (113,479,232) | 35,423 | 19.1 MiB (20,013,820) |

Group 0 alone is 1.9 GiB: **the direct path cannot keep the whole figure catalog resident** (ADR 0007's own premise).

**What `scene.units` can and cannot say.** `scene.units` (populated by `load_scene`, not by `load_placed_scene` — `lan5.txt` and the three `ship-start-v2` candidates are reported as 0 figures used for this reason, not because they truly have none) is the *scenario file's own starting* ships and soldiers only. A running game adds carriers, residents, animals and more ships as it plays, none of which `load_scene`/`scene.units` can see — the loader genuinely cannot tell how many figures a long game accumulates, so this section does not claim to measure that; the packet's own "otherwise say it cannot" applies here.

**The measured maximum of what is knowable from the start units** (deduplicated by `render::FrameKey::unit`, the frame table's own identity, across all 519 scenes × groups): **3,401,840 bytes (3.24 MiB), 74 frames, `Scenes/0201SingleAddOn/single38.sze` group 0.** This is the whole number this measurement can give; it is not a bound on a running game's figure residency need, which is unmeasured. Figures are **not included in the static+water packing** in §3-4 — ADR 0007's own figures/hot class is a separate, **evicting** residency budget (`gpu-chunks.md` 4.2's Static/Hot split, the same split §7 recommends for buildings/water vs. figures), sized later from a running game, not from the start units alone.

## 6. Player colours

**Static (Anlagen.dat) frames: structurally never recoloured (M, C), and a real finding about their pixel content anyway.**

- `anno::render::SpriteCache::get` (`iso.hpp:276-277`) — the only place that decodes a static frame for drawing — has no colour parameter at all; it calls `assets::building::to_rgba(anlagen, entry, group, frame)` (`iso.cpp:322`), which also has no colour parameter (`building.hpp`). `assets::figure::player_coloured`/`recolour` are called only from `assets/src/unit.cpp` (lines 180, 207), the live-unit path, never from `iso.cpp`'s static draw path. So today, 0 static frames carry a player colour — this is the engine's current behaviour, not a property of the files.
- **Decoded anyway, and this is not a player-colour signal.** `bench_frame`'s `static_player_colour_eligibility` decodes every static frame (`assets::building::to_rgba`, the same function `SpriteCache::get` calls) and tests each covered pixel against `figure::recolour`'s own test (`G² > trunc(1.5·(R² + B²))`, green dominance — applied by comparing the decoded colour against `recolour(colour, test_player)` rather than reimplementing the formula): **11,180 of 27,537 non-shadow frames (40.6 %)** have at least one matching pixel, **3,760,786 of 102,530,209 checked pixels (3.67 %)**. That test is a green-dominance test, and building frames are full of ordinary green — grass, trees, fields, painted parts of textures and sprites — so this result shows only that **the decoder's green-dominance test cannot be applied to static frames to find player colour**: it matches ordinary scenery just as readily as any marked colour would, and nothing distinguishes the two from this measurement. It is **not** a finding that a meaningful share of building pixels "would be recoloured" in any useful sense, and not a look-and-feel question for the owner to pick a default for — there is no marking here to turn on or off.
- **A real, separate open question (OQ-690):** whether the *original* ever draws any static object (a flag, for instance) tinted by its owner's colour at all, through a different mechanism than this test can see. OQ-022 already found that the Anlagen.dat COLORS `flag` value 4 means `EffectsMulti`, a player-colour-variant palette kind, used by colour table id 109, `Flagge01_01.PCX` — a flag. Whether any *placed* ANLAGEN object actually uses that table, and whether the original renders it recoloured, is unresolved and out of this task's scope; `docs/open-questions.md` OQ-690 names it for RE or a capture of a placed flag object across players.

**Figures: 69 of 368 FigurenHeader.dat entries** (17 BEWOHNTYPE names, `figure.hpp`'s own `kPlayerColouredTypes`), 35,423 of 241,808 frames at group 0 (section 5's table; ships are not among the coloured types). Each decoded, coloured copy is a separate `SpriteKey`/`FrameKey` (variant folds the colour into the key), so holding every colour used is literally one decoded image per (frame, colour) pair.

**Two ways to hold a figure's player colour (measured bytes of each, not chosen for the owner; both rest on the scenario's *start* units only, §5 — not a running game's full figure population):**
1. **One decoded copy per colour a scene uses.** Bytes = (player-colourable bytes the scenario's start units need) × (distinct colours the scene's players use, up to 8 in the original). Against the measured maximum start-unit need (§5, 3.24 MiB, one colour): at up to 8 colours, ≈ 26 MiB — a figure-class cost, not part of the static+water budget §4 checks.
2. **One uncoloured copy plus a mask, coloured per instance** (T11 section 10 item 3): bytes = the player-colourable bytes the start units need, once, regardless of how many colours are in play, with the colour applied at draw time (a shader uniform per instance, not a GPU-memory cost measured here).

A performance/complexity trade-off (one atlas entry per colour vs. a per-instance shader parameter), not obviously a memory one at the start-unit scale measured — but §5's own point stands: a running game's figure population, and so its real player-colour cost, is unmeasured here. Not decided — the owner's call, once the figure class's own budget (§5) is sized.

## 7. The layout — and what is still open

Page size and padding are measured choices; the mip level and the budget itself are the owner's, not settled here (§4's QUESTION, §4's F2 facts).

| Choice | Value | Reason |
|---|---|---|
| Packer | `AtlasResidency` (shelf) | Already specified (`gpu-chunks.md` 4.2-4.3) and tested; no reason to design a second one for 1b to build on. |
| Page size | 2048 px | Lowest byte cost measured for the worst cases found (section 3), with no benefit found from 4096. **Choice.** Does not by itself resolve §4's QUESTION — no candidate measured does. |
| Padding | 2 px | Lowest byte cost measured, and the only padding this task found that supports even one mip level. Whether 2 px is enough for 1c's actual dilation kernel is **not measured here** — 1b/1c confirm it against the real upload/sample code. **Choice, flagged.** |
| Mip level | **Proposal, not a layout choice** (F2) — one level, not a full chain | Dropping mips changes how a zoom looks (shimmer as a group nears its switch) — a look-and-feel decision, the owner's by CLAUDE.md item 7, not this task's. The facts measured, for the owner to decide with: the groups already form a mip ladder (≤ 2.1× between adjacent groups), so one level does nearly all the useful work; a full chain needs far more padding than any candidate measured (§3); the measured cost of each option is in §4's tables (one level everywhere: 4090 fits with headroom, iGPU's worst pair is already over budget even with *no* mips, so one level only makes it worse; one level on the 4090 alone, none on the iGPU: same iGPU shortfall either way; none: the baseline §4 QUESTION numbers). **Open question:** `docs/open-questions.md` OQ-096b, next to OQ-096. |
| Premultiplied alpha | Not measured here | Changes how pixels are stored, not how many bytes or pages — out of this task's scope (RENDER-DIRECT-1b/1c). |
| Shared vs. separate pages per sprite set (buildings/water/figures) | Separate (Static class: buildings + water; figures their own, evicting class) | Figures (§5) are two to three orders of magnitude larger as a whole catalog, their *running-game* need is unmeasured (§5), and they change per scene by a different mechanism (`scene.units`, not `prepare_objects`); mixing them into one residency pool would let a busy figure scene evict static chunk frames the next island draw still needs. Matches `gpu-chunks.md` 4.2's existing Static/Hot split. **Choice.** |
| Player colours | Not chosen here (§6) | Both storage strategies' bytes rest on the start-units measurement only (§5); a performance/complexity trade-off, not decided. |
| Enhanced water's extra copies | Counted, not pre-allocated specially | Section 2/3's packing already includes them (the smoothed variant is just another distinct `SpriteKey` in the same combined set); no separate budget line needed. |
| Static+water budget on the iGPU (256 MiB), the adjacent-pair fit | **Open — §4's QUESTION, OQ-096b** | The worst scene found (`02_1024_easy_128.sze`, groups 0+1) needs 272 MiB at the cheapest candidate measured, 16 MiB over budget, before any mip cost. Not resolved here; the owner decides among §4's options. |

## 8. No render-path change

This task changed `src/tools/bench_frame/main.cpp` (a new `--atlas-report` mode) and `src/render/tests/residency_test.cpp` (a new unit test); nothing under `src/render/src`, `src/render/include` (other than the header-only `residency.hpp`'s own, pre-existing public API, unchanged), `src/sim`, `src/app/src` (other than `bench_frame`'s own tool), or any digest/picture path was touched. `git diff --stat` against `origin/main` confirms this.
