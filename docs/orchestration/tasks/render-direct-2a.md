# RENDER-DIRECT-2a: per-island instance lists, chunk-culled, built once at load

**Drafted 2026-11-27 by the Windows render executor; approved by ORCH III the same day.** Plan §3 W2a. ADR 0007
decision 2's "draw lists" bullet. Branch `task/RENDER-DIRECT-2a` from `refs/candidates/cand12` (`c5af390c`). About
3-4 h.

## What already exists (checked in code, not assumed)
`Scene::build_island_lists` (`scene.cpp:396`, called once per island in `Scene`'s constructor, `scene.cpp:632-639`)
already builds `IslandLists` per island via `prepare_objects` (`iso.hpp`), in the painter's order, once at load,
immutable (`scene.hpp:205-226`). Today's CPU chunk build (`frame.cpp:331`, `scene.island_lists(...)`) already
reads these lists rather than re-calling `prepare_objects` per chunk. So "the instance list ... built once at
load" is this, already shipped - **not new work**.

## Scope (ORCH III's decision, 2026-11-27)
What's missing is "**culled per chunk**": today's chunk build re-derives per-chunk membership *and* resolves each
object to a drawable `SpriteDraw` (frame lookup, anchor, height, via `place_prepared`) **every time that chunk is
built**, not once. This packet:
- Resolves each static object once at load into its drawable record (frame, anchor, height - `place_prepared`'s
  own result), bucketed per chunk.
- Builds it from the existing `IslandLists` - no second object list.
- That record is what RENDER-DIRECT-3's instance buffers will hold: this is what ADR 0007 decision 2's "culled per
  chunk" means.
- A membership-only index (no per-object resolution) was considered and rejected: RENDER-DIRECT-3 would have to
  redo the resolution anyway, so it isn't done once at load.

**Not in scope:** time-dependent phases (surf's `(3y+5x) mod 32`, river pieces - stay on today's per-frame
`place_prepared` until RENDER-DIRECT-3 decides how the direct pass handles them; name this boundary in the
report); the building-change patch (RENDER-DIRECT-2c/W3); any GPU upload or drawing from the new structure; any
change to the viewer's frame; the 12-bit picture-id widening (W2b PICTURE-IDS-1, a different packet).

## Do
A new per-island, per-group structure built once at `Scene` construction time (alongside `build_island_lists`,
same worker split already used for `ground_grids`/`WaterSlot` per island): for every chunk the island's box
overlaps, the resolved `SpriteDraw` list in painter's order, from `IslandLists`'s `Prepared` entries resolved via
`place_prepared`'s own per-object logic (`FrameTable`/`SpriteCache` lookup, anchor, height) - everything
`place_prepared` computes today, computed once instead of on every chunk build.

## Proof
- A CPU raster built directly from the new structure equals today's chunk raster exactly, on `atlas.md` §9's
  three scenes and `lan5.txt` (the digest-equality contract RENDER-DIRECT-1b used).
- A mutation control: a deliberately wrong anchor or frame in the structure makes that equality check fail (proves
  the check is live, not vacuous).
- Record load time and memory of the new structure, measured in Release.

## Common rules
clang-format 23.1.3; WSL `check_unix_warnings.sh`; REPORTS entry (~60 lines); exact-head CI; PC-quiet check before
any GPU/long run; no reviewer subagents (ORCH III reviews); push `task/RENDER-DIRECT-2a`, never `main`. No build
until "PC free" (reading and writing code only until then).
