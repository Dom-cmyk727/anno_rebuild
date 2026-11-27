# UI-RANGE-1 B: draw the range (DRAFT for ORCH III's review, from stage A)

Stage A's findings: `docs/re/ui.md` 12. **Recommendation: split into B1 and B2** (each about 3-4 h). One packet would
touch the app's selection logic, the CPU reference picture and the GPU compositor's ground and object passes, and it
needs a factor that is not measured yet (OQ-796). B2 touches render files, so it is serialised with the render lane.

**Before either:** the owner's decision on OQ-795. If a GOG capture shows no darkening, the darkening is a History
Edition addition, and showing it is an owner choice (the original UI is the base, owner rule 2026-11-14). Proposal: the
faithful default follows the GOG answer; HE's darkening is drawn when GOG has it too, or behind a UI option if not.

## B1: which tiles are lit (app, CPU reference; about 3 h)

- **Trigger (H, `ui.md` 12.1 rows 1, 4, 5):** `range_city(selection, armed tool, pointer tile, TerritoryView)` returns one
  city or none:
  - a selected Kontor or market (PRODTYPE 33-35 group, BUILDINGSOBJECTS part D P2): its city;
  - an armed territory-bound building (the types City::Place claims a disc for, part D P2) or an armed road: the city
    that owns the tile under the pointer, else none (L, OQ-796);
  - otherwise none.
- **Lit set:** island tiles with `owner == range_city`; every other tile of that island **and of every other island**
  (sea tiles inside each island rectangle included, `ui.md` 12.1 row 2) is dark; the open sea outside the island
  rectangles is never dark.
- **Draw (CPU reference path only):** multiply the ground and every object anchored on a dark tile by the factor from
  OQ-796 (measured on `he_01` 3:28 vs 4:19 in this packet; one value, data with its source in `data/ui/`).
- **Tests:** the pure `range_city`/lit-set function in unit cases (selected Kontor, combined Kontor+market, armed market,
  armed road, nothing selected, a second city on the island); a wiring script from the **default start view** (owner
  rule) with dev facts `ui.range_city` and the lit-tile count; a failure proof.

## B2: the GPU path (render; about 3-4 h)

- A per-island owner mask (the `TerritoryView` owner grid, one byte per tile) uploaded when the published territory
  changes, plus one uniform (`range_city`, or none = no darkening). The ground pass and the object pass multiply by the
  factor where the tile's owner differs; sea tiles inside the island rectangle get it in the sea pass.
- **Proof:** GPU equals the CPU reference within the oracle's tolerance on a scene with a selected Kontor; HE pixel
  comparison of the lit/dark edge on `he_01` 3:28 within the mask; no change with `range_city` none (pixel-identical to
  today); a perf measurement (`docs/perf/README.md`).
- **Not in B:** the ghost's range outline (done by UI-GHOST-1, part B §5.1); its HE green colour (GOG yellow is the
  faithful default, decided 2026-11-17); the farm's work-area square (`he_01` 8:05, a separate packet).
