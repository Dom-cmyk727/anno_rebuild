# RENDER-DIRECT-1a: what the resident sprite atlases need, measured, and their layout fixed

**Status:** written and **RELEASED** 2026-11-17 by the Claude Code orchestrator for **the Windows executor**,
from the remaining-work assignment ([remaining-work-2026-11-16.md](../remaining-work-2026-11-16.md), Windows
task 5) and [ADR 0007](../../adr/0007-direct-rendering.md) step 1, first half.

**Base:** `origin/main` at the time of starting (name the commit). Branch `task/RENDER-DIRECT-1a`, a new worktree
on E:. About 2 to 3 hours. One concern: measure, with the engine's own code, what keeping every sprite the
direct renderer draws resident on the GPU costs, and fix the atlas layout in a document. **Measurement and
document only:** no change to any render path, default, digest or picture. Never rebase, force-push or push
`main`. No reviewer subagents.

## Why

ADR 0007 draws the world every frame from resident atlases of all three sprite sets, with premultiplied alpha,
dilated padding and mips. The one number it rests on, "about 200 MiB for any original scenario, 330 MiB with the
enhanced water's copies", comes from a scratch Python script that was never committed (`gpu-chunks.md` section
2), covers only the 31 single-player scenarios, has no padding, no mips and no packing loss, and marks the 330 MiB
as an estimate. The LAN target's own map (`lan5`) and the endless scenarios with linked islands were never
counted. RENDER-DIRECT-1b (packing on the CPU) and 1c (upload) need a layout that is decided on measured numbers.

## Read first

- `CLAUDE.md`; `docs/testing/evidence-contract.md`;
- [ADR 0007](../../adr/0007-direct-rendering.md) (whole);
- `docs/render/gpu-chunks.md` sections 2, 4 and 9 (the earlier count, the residency rules, the limits);
- `src/render/include/anno/render/residency.hpp` (`AtlasResidency`), `frame.hpp` (the frame table),
  `iso.hpp` (`prepare_objects`, `SpriteKey`, `SpriteCache`);
- `src/tools/bench_frame/main.cpp` `load_any_scene` (loads `.sze`, `.scp`, a fixed-map `.txt` and a ship-start
  `.json` through the app's own loaders).

## Do

1. **One command** that writes the table, using the engine's own scene loading and object preparation, not a
   second implementation: a mode of `bench_frame` (for example `--atlas-report <out.tsv>`) or a small tool next to
   it; say which and why. It decodes nothing it does not need; frame sizes come from the headers where that is
   enough, and decoded pixels only where a figure needs them (player colour, alpha).
2. **The scenes:** every `.sze` and `.scp` under `Scenes/` (0010SingleEndlos, 0011SingleEndlosAddon,
   0100Kampagne01, 0200Single, 0201SingleAddOn, 0500MultiEndlos, 0700Multi, Tutorial), `data/maps/lan5.txt`,
   and the three candidates in `data/maps/candidates/ship-start-v2/`. A scene the loader refuses is listed with
   the reason, not left out silently.
3. **Per scene and group (0, 1, 2), the columns:** static frames and their bytes; water-object frames (every
   animation frame) and their bytes; the enhanced water's extra copies, measured from the code that makes them,
   no longer estimated; static instances; and, for the atlas, the bytes and the number of pages after packing at
   **page 2048 and 4096**, **padding 2 and 4 px**, **mips: none and the full chain**, with the packer that 1b will
   use (shelf or better: name it). Area sums are not packing; the page count comes from a real packing run.
4. **The figures** (ships, carriers, animals, soldiers): the whole figure frame set per group, in frames and bytes,
   once (it does not depend on the scene). The direct path cannot keep it all resident; say how much of it a
   scene actually uses if the loader can tell (its units and ships), otherwise say it cannot.
5. **Player colours:** how many static and figure frames carry player-colour pixels, as the decoder marks them,
   and the bytes of the two ways to hold them: one copy per colour a scene uses, or one copy with a colour mask
   and a per-instance colour (T11 section 10 item 3). Measure; do not choose for the owner if both fit.
6. **The two GPUs' limits:** `maxTextureDimension2D`, `maxTextureArrayLayers` and `maxBufferSize` of the RTX
   4090 and the Radeon iGPU through wgpu (a few seconds each; tell the orchestrator before you open a GPU device,
   as before).
7. **`docs/render/atlas.md` (new):** the measured table (condensed; the TSV is generated, not committed), and the
   layout with a reason for each choice: page size, padding, mip count, premultiplied alpha, whether the three
   sets share pages or not, the hot class (water objects, figures) and the static class, player colours, and the
   enhanced water's copies. Every number cites the command that made it. What is a choice and not a measurement
   is called a choice.

## Done when

- One command reproduces the table; its run over all scenes is in the report (time and peak memory too), and a
  scene that could not be loaded is named with its reason.
- The worst scene, with the layout chosen, fits **512 MiB on the RTX 4090 and 256 MiB on the iGPU** (the defaults
  `gpu-chunks.md` 4.3 rule 6 proposes; the owner's OQ-096 is still open), static and water classes together, with
  the figures' share stated separately; or the report stops with a QUESTION that names the scene, the bytes and
  the options.
- A unit test without game files checks the packing count on synthetic sizes (known page counts for a few
  hand-made sets, padding included).
- No render output changes: the render and app lanes' digests are untouched (the diff touches no render path;
  say so with the file list).

## Report

About 50 lines, READY FOR REVIEW: the command, the condensed table (worst scene per group, the LAN map, the
median), the figures' totals, player colours, the two GPUs' limits, the chosen layout with its reasons, and the
"Decisions and reasoning" list. Guesses are marked as guesses.

## Gates

The changed lanes (`bench_frame`'s own tests and the new unit test), the tool's game-file run, clang-format
23.1.3, the WSL `-Werror` check on the touched C++ files. A test refused for memory is rerun alone. Tell the
orchestrator before any GPU or test run on this PC, and wait for the go.
