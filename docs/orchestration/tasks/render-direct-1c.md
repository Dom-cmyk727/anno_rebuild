# RENDER-DIRECT-1c: the atlas pages on the GPU, proven exact by readback and by sampling

**Status:** written 2026-11-17 by the Claude Code orchestrator for **the Windows executor**. Released once
RENDER-DIRECT-1b is accepted.

**Base:** `origin/task/RENDER-DIRECT-1b` at its accepted head. Merge `origin/main` into it if `main` moved; never
rebase. Branch `task/RENDER-DIRECT-1c`, a new worktree. About 4 hours. One concern: the pages that
`build_atlas_pages` makes are uploaded to the GPU and proven to read and sample back exactly. **Nothing in the
viewer's frame changes:** the chunk path, the pipelines that draw the world, `--png` and every digest stay as
they are, and no flag is turned on. Never push `main`. No reviewer subagents.

## Why

ADR 0007 stage 1 is "the atlases, and VRAM measured". RENDER-DIRECT-1a fixed the layout and the budgets;
RENDER-DIRECT-1b builds the pages on the CPU and proves them equal to the decoder. The GPU copy is the last
step of stage 1:
- `remaining-work-2026-11-16.md` lines 75-76: "the atlases on the GPU";
- `atlas_pages.hpp`: "RENDER-DIRECT-1c uploads and draws from these pages".

The draw passes come later (stages 2-3: instance lists, then the direct object and ground passes). This packet
gives them a texture whose every texel, at both levels, is known to equal the CPU pages, and a placement-to-UV
rule proven by sampling. Then a later mismatch can only be in the passes.

## Do

### 1. Upload the pages: a new unit, not a change to the chunk compositor

Upload one `AtlasPages` into one 2D texture array:
- RGBA8, `page_size` × `page_size` × layers, 2 mip levels;
- `queue.writeTexture` per layer and level;
- texture binding usage and copy-source usage (for the readback below).

**The format:** read how the existing sprite atlas texture is created (`new_atlas`, `gpu.cpp`) and use the same
format family, so later passes blend the same way. Say which format and why. The pages hold premultiplied
alpha already: no conversion on upload, and the data must not pass through an sRGB conversion.

**Limits, from the device and never a constant:**
- `maxTextureArrayLayers`;
- `maxTextureDimension2D` against `page_size`;
- the budget: `atlas_budget_bytes_before_sprite` minus what the existing sprite atlas holds, as
  `cb_atlas_budget_bytes` computes it today.

A set of pages that does not fit is refused with a named error giving the bytes needed and the bytes allowed.
It is never truncated.

**Which groups:** a pure, tested function `resident_atlas_groups(adapter_type, zoom_group, group_count)` gives
the zoom groups whose static+water pages are resident (`docs/render/atlas.md` section 7 and OQ-096/096b,
decided):
- every group on a discrete adapter;
- the adjacent pair a zoom crosses on any other adapter.

Name the pair rule you implement, with its source. No streaming or eviction yet: one call uploads the pages for
the groups that function names.

### 2. Readback: every texel, both levels, both adapters

Copy every layer and both levels back (texture to buffer, `mapAsync`). Compare them byte for byte with
`AtlasPages::level0` and `level1`; the row padding of the copy is removed before comparing. Exact equality, no
tolerance.

Run it:
- on the synthetic page sets of `atlas_pages_test.cpp`;
- on `data/maps/lan5.txt` (static+water, all groups on the 4090, the pair on the Radeon);
- on `Scenes/0010SingleEndlos/02_1024_easy_128.sze`, groups 0+1, the worst pair, 340 MiB.

Run it on **both adapters of this PC**:
- `ANNO_GPU_ADAPTER=RTX`;
- the Radeon iGPU, the way `play_gpu_test.cpp`'s Radeon case selects it.

The log must name the adapter in each case. A run on any other adapter is no result.

### 3. Sampling: the placement-to-UV rule, proven exact

A small offscreen pipeline in a test draws every placement of a page set:
- one quad per frame, at its own size, into an RGBA8 target;
- once at level 0 from `x, y, width, height`;
- once at level 1 from `mip_x, mip_y, mip_width, mip_height`;
- with a **nearest** sampler and UVs computed from the placement at texel centres;
- **no blending**: the target receives the sampled texel.

Each drawn rectangle must equal the CPU page's rectangle exactly. That proves the UV rule a later pass will
use. Write the rule as one shared function, so the passes reuse it and don't re-derive it. Run it on both
adapters, on the synthetic set and on `lan5.txt`.

### 4. Measure: VRAM and upload time

On this PC in **Release**, on a quiet machine (ask the orchestrator for the slot), for `lan5.txt` and
`02_1024_easy_128.sze` on both adapters, report:
- texture bytes allocated against `atlas.md`'s figure for that scene;
- upload wall time: writeTexture through a queue fence;
- readback time.

### 5. Docs

- `docs/render/atlas.md`: a section "The upload" (format, limits, groups rule, refusal, the UV rule and where it
  lives);
- `docs/perf/README.md`: an entry with the numbers;
- any guess goes into `docs/open-questions.md` (the next free OQ number: check the register and the open
  branches; UI-STATUS-1 holds 700-709).

## Not in this packet

- No change to `GpuFrame::render`, the chunk build, the viewer, `--png` or any digest.
- No draw pass, instance list, streaming, eviction or Hot (figure) class upload.
- **The `collect` step being single-threaded** (85-92 % of page-build time, 1b's finding) is not fixed here. It is
  queued as its own packet.

## Acceptance

- Readback is byte-exact on both adapters for every case in step 2.
- The sampling test is exact on both adapters.
- The budget refusal and `resident_atlas_groups` have unit tests, without a GPU.
- Nothing else changes: the existing GPU tests, `--png` digests and the `play: GPU matches the CPU path` cases
  pass unchanged.
- **The RTX 4090 gate:** `anno_render_gpu_tests` from the msvc build, with the log naming the adapter.
- The WSL `-Werror` check is clean; clang-format is applied; CI is green on the final head.
- Report at the bottom of `docs/orchestration/REPORTS.md`, READY FOR REVIEW, with "Decisions and reasoning".
