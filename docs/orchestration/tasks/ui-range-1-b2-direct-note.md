# UI-RANGE-1 B2 in RENDER-DIRECT-3: drawing the range darkening per object — note for the decision

UI lane, 2026-11-27. Input for where B2 (the range display's drawing, `ui-range-1-b-draft.md`) lands: in today's chunk
path, or as part of RENDER-DIRECT-3's direct passes (ADR 0007).

**What B2 must draw** (`docs/re/ui.md` 12, B1 in code):
- every tile and every object of an island that does not belong to the lit city, multiplied by about 5/8 per channel
  (OQ-796, GOG capture);
- the sea inside the island rectangle too;
- the open sea and the lit city unchanged;
- the lit city chosen by `range_city` (B1, `range_view.hpp`): the armed type under the pointer wins, else the selected
  Kontor/market; neutral gives none.
- It changes on selection, on arming, on pointer moves across a city boundary, and when territory is published (a
  new Kontor or market).

**Why per object, not per chunk.** Today's chunks are baked images. Darkening in them means re-baking every chunk of the
island at every change of `range_city`. A pointer moving across two cities with a road armed would re-bake an island
per crossing, which is the stand-in/dissolve churn ADR 0007 removes. In the direct passes, each ground tile and
each object instance is drawn every frame, so the darkening is one multiply in the fragment stage with no rebuild.

**Shape in RENDER-DIRECT-3:**
1. **Inputs:**
   - the per-island owner grid, one byte per tile: `TerritoryView`'s owners, already in `PlaySnapshot::territory`,
     uploaded only when the published territory changes (B1's `range_dark` reads the same grid);
   - one uniform `lit_owner`: the lit city's owner value, or none.
2. **Ground pass:** the fragment looks up its tile's owner, and darkens where `lit_owner` is set and the owner differs.
3. **Object pass:** each instance carries its anchor tile (RENDER-DIRECT-2's lists place instances by tile; not checked here); the instance's owner is read
   once per instance (vertex stage) from the same grid. A building straddling the boundary is lit or dark as a whole,
   by its anchor tile. **Settled (OQ-907, P):** GOG darkens per object - in the UI-RANGE-1 A capture the lit/dark
   boundary follows the tree crowns' outlines, not tile edges.
4. **Sea pass:** sea pixels inside an island rectangle use the tile under them, which needs the same lookup as the
   ground; open sea is never dark.
5. **Factor:** data (`data/ui/`, 5/8 with its source), applied after the per-pixel depth (ADR 0007 "Added after
   acceptance"), so the darkening does not change what is in front.
6. **CPU reference:** the same multiply in the CPU raster of the instance lists (RENDER-DIRECT-2's), so GPU = CPU holds
   within ADR 0007's stated tolerance.

**Cost:** one byte per tile per island in VRAM (the island's tile count, a few hundred KB at most), one uniform, and one
texture fetch per fragment in two passes. Measure it with the rest of RENDER-DIRECT-3 (`docs/perf/README.md`).

**Options for the orchestrator:**
- **(a)** B2 as a small step inside RENDER-DIRECT-3 (render lane), with the UI lane's B1 facts as its test hooks.
  About 2 h on top of the passes.
- **(b)** B2 on today's chunk path now (re-bake on change): visible sooner, but thrown away at RENDER-DIRECT-6 and
  costly with a road armed.
- **(c)** a stopgap overlay quad per dark tile on the GPU overlay: no re-bake, but it darkens the objects of lit tiles
  that overlap dark ones, and the reverse (wrong at boundaries).

**Recommendation: (a).** B1 already exposes `ui.range_city` and `ui.range_dark_tiles`, so the render lane's pass can be
checked against them from the existing wiring script.
