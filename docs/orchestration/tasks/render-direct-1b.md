# RENDER-DIRECT-1b: the resident atlas pages built on the CPU, exact to the pixel

**Status:** written and **RELEASED** 2026-11-17 by the Claude Code orchestrator for **the Windows executor**, from
the remaining-work assignment ([remaining-work-2026-11-16.md](../remaining-work-2026-11-16.md), Windows task 6),
[ADR 0007](../../adr/0007-direct-rendering.md) step 1, second half, and the owner's decision on OQ-096 and OQ-096b
(2026-11-17). Starts when LAYER-CHECK-1 is accepted and the orchestrator lifts the hold on test runs.

**Base:** `origin/task/RENDER-DIRECT-1a` at its accepted head (`5fb98192` or later), because this packet builds on
its `atlas.md` and its `bench_frame` mode. That branch is not on `main` yet. Merge `origin/main` into your branch
when the current landing batch is on `main`; never rebase. Branch `task/RENDER-DIRECT-1b`, a new worktree on E:.
About 4 hours. One concern: the pages the direct renderer will upload, built on the CPU, proven equal to the
decoder's output texel for texel. **Nothing is uploaded, wired into the viewer or drawn** (RENDER-DIRECT-1c does
that); no frame digest moves. Never force-push or push `main`. No reviewer subagents.

## What is decided (do not reopen)

- **Layout** (`docs/render/atlas.md` section 7, accepted): pages of 2048 x 2048 RGBA8; 2 px padding on every side of
  every frame; the shelf packer `AtlasResidency`; separate page classes for static+water and for figures.
- **The owner, OQ-096 and OQ-096b, 2026-11-17:** asked "Raise it to 384 MiB, one mipmap level on both cards (my
  recommendation)", the owner answered "your recommendation". So: one mip level on both adapters; the budget defaults
  are 512 MiB on a discrete adapter and 384 MiB otherwise.
- **ADR 0007 decision 2:** premultiplied alpha, dilated padding, mips.
- **Player colours on figures:** one decoded copy per (frame, colour) a scene uses, as the engine keys them today
  (`SpriteKey` / `FrameKey` with the colour in the variant). The mask-per-instance way stays a possible later
  optimisation; it is not this packet.

## Read first

- `CLAUDE.md`; `docs/testing/evidence-contract.md`;
- `docs/render/atlas.md` (whole), ADR 0007 decisions 2, 3 and 6;
- `src/render/include/anno/render/residency.hpp` (the packer), `iso.hpp` (`SpriteCache`, `prepare_objects`),
  `scene.cpp` `sprites_in` (the enhanced water's smoothed copies), `assets::building::to_rgba`, the unit path in
  `assets/src/unit.cpp`;
- `src/render/src/gpu.cpp` `cb_atlas_budget_bytes` (line 1436 on `main`), `frame.hpp` lines 93 to 102.

## Do

1. **The budget default in code.** `cb_atlas_budget_bytes` uses 256 MiB for a non-discrete adapter; make it 384
   (OQ-096 decided), with its comments in `gpu.cpp` and `frame.hpp`. Pull the default into a small pure function
   with a unit test (discrete 512, integrated 384, other 384, an explicit `atlas_budget_mb` wins, the layer clamp
   applies). If that cannot be done without touching the draw path, say why.
2. **The page builder** (a new `render` unit, for example `atlas_pages.hpp/.cpp`; name it and say why). Input: a
   frame set (ids, the decoded straight-alpha RGBA from the engine's own decoders, sizes) and the class. Output:
   - pages of 2048 x 2048 RGBA8, **premultiplied**: `c' = (c * a + 127) / 255` per colour channel, integer, alpha
     unchanged. If the engine already has a rounding rule for premultiplying, use that one and cite it;
   - each frame at the place `AtlasResidency` gives its padded slot (frame + 2 px on every side), the frame at
     offset (2, 2) inside the slot;
   - **the padding dilated:** each padding texel is a copy of the nearest texel of the frame (clamp to edge),
     after premultiplying;
   - **one mip level per page:** 1024 x 1024, each texel the 2 x 2 box of level 0 with the sum rounded half up
     (`(s + 2) / 4` per channel), in premultiplied space;
   - per frame: layer, x, y, width, height of the frame itself (not the slot), and its level-0 texel rectangle.
   - No second packer; no float in the texel math.
3. **Built in parallel, deterministic.** Decoding and page filling run on worker threads (the existing pool or
   one per core), with no shared lock on the per-texel path. **The pages' bytes do not depend on the thread
   count**: a test builds the same set with 1 and with N threads and compares a hash of every page.
4. **The proof, texel for texel**, computed from the decoder and not from the page:
   - every frame's rectangle equals the decoder's output, premultiplied by the rule;
   - every padding texel equals the clamp-to-edge rule;
   - every mip texel equals the box rule applied to the expected level 0;
   - texels outside every slot are 0.
   As a unit test on synthetic frames (no game files; include a frame of size 1 x 1, odd sizes, a frame exactly
   filling a page after padding, and alpha 0 and 255 edges), and as a game-file test over three scenes:
   `data/maps/lan5.txt`, `Scenes/0010SingleEndlos/02_1024_easy_128.sze` (the worst scene of 1a) and one campaign
   scene. Each scene runs groups 0, 1 and 2, static+water with the enhanced water's copies, plus the figures of
   its start units. Put the game-file case in the `heavy` suite if its memory or time needs it.
5. **Cost on this PC:** a `bench_frame` mode (for example `--atlas-build <scene|->`) that builds all groups'
   pages for one scene or the corpus and reports per scene the wall time, the bytes of the pages with their mip
   level, and peak memory. Run it for the worst scene, `lan5.txt` and the corpus once, and report the worst and
   the median. This is the load-time cost ADR 0007 decision 6 moves to load; there is no threshold yet, so report
   it and do not optimise blind.
6. **Docs:** `docs/render/atlas.md` gets a section "The pages" with the byte format, the premultiply rounding,
   the dilation, the mip rule and the thread rule, each with the code that does it; `gpu-chunks.md` rule 6 cites
   the code's default.

## Done when

- The default is 384 MiB for a non-discrete adapter in code, tested.
- The texel proof passes on the synthetic set and on the three scenes, including the figures, and the thread
  test shows equal hashes.
- The bench numbers are in the report, with the command.
- No draw path changed: the diff touches no shader, no draw call and no compositor code (say so with the file
  list). The render and app lanes' digests do not move.

## Report

About 50 lines, READY FOR REVIEW: the unit and its interface, the rounding rules, the three outcomes of step 4,
the thread test, the bench table, any frame the proof found unequal (with its id and first texel), and the
"Decisions and reasoning" list.

## Gates

`anno_render_tests` (the changed lane; `scene_reclaim_test`'s known order failure is not yours: run it alone if
it shows), the new game-file case, the budget test. The GPU tests touched by the default (ask first, then run
`anno_render_gpu_tests` on the RTX 4090 with `ANNO_GPU_ADAPTER=RTX`), clang-format 23.1.3, and the WSL `-Werror`
check on the touched files. Tell the orchestrator before any GPU or test run on this PC, and wait for the go.
