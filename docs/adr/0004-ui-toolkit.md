# ADR 0004: The UI toolkit

- **Status:** Proposed
- **Date:** 2026-11-07

## Decision

UI.2 (the original's HUD and menus, placeholders where the sim has none) draws with **our own
small widget set, drawn by the GPU compositor, using the original's fonts and `ToolGfx` textures**
(owner decision D4, `docs/plan/lan-mvp.md` §4 W5). No third-party UI library, no immediate-mode GUI:
the widget tree is a thin, data-driven layer over the existing render/app code, not a new
subsystem with its own event loop or its own GPU pipeline.

### Why this shape, not a general-purpose toolkit

The original's screens are not freely laid out: every element's position, size and source texture
is a measured fact (`data/ui/`, `docs/re/ui.md`), and UI.1.F2's own gates (`tools/ui_layout.py
mock`/`compare`) already check a rendered frame against those facts pixel by pixel. A general
widget toolkit (retained-mode or immediate-mode) would add a layout engine UI.2 does not need —
every rectangle is already known — and would duplicate the GPU atlas/sprite-draw path the
compositor already has for exactly this job (`docs/render/gpu-chunks.md` §4, the existing
`Atlas`/`atlas_alloc` RGBA8Unorm array texture and the straight-alpha sprite-instance draw,
`gpu.cpp`). Reusing that path keeps one atlas, one upload budget and one CPU-reference-vs-GPU
fidelity contract for both the world and the chrome, instead of two.

## Widgets

A widget is a **data record, not a C++ class hierarchy**: `data/ui/hud.txt` and
`data/ui/construction.txt` already describe every element (`element <name> pos X Y size W H file
<tex> access <ACCESS> tex_id <id> method <sprite-sad|inc-size|atlas-right|view-dll>`) at the
**1024 × 768 reference**. UI.2 reads the same files at runtime (through the existing
`tools/tex.py`-equivalent C++ decoder already used by `anno_dump`/the asset pipeline) and turns
each `element` into a draw call: a rectangle, a decoded texture id and, for the eight construction
categories and the 148 cells, a small per-instance index (category row, cell column/row) computed
from `construction.txt`'s own grid/category records, never a hardcoded id in `src/`.

**No coordinate lives in code.** A screen is a flat list of elements plus the handful of
*generated* ones `data/ui/` already documents as derived, not stored (the category column's eight
cells from `construction.txt`'s grid; the 148 unlocked-cell icons from the same grid plus
`cell_icon_tex_id`'s `THUMBNAIL* + 3×record_index` formula, §3.3 of `docs/re/ui.md`). Adding a
screen or an element is a `data/ui/` edit plus, where the source is not yet an existing `.tex`
decode, a new `access`/`tex_id` row — never a new C++ type.

**Anchoring** follows `docs/re/ui.md` §5's identities at the 1024 × 768 reference (right chrome
`x = W − 245`; status bar `y = H − 35`; construction grid origin `(W − 216, 305)`, stride 52;
tabs `y = 181`, stride 61 from `x = W − 245`) and the scale factor from R3 below. These are
stated as identities *at 1024 × 768*, not yet verified at other window sizes (`docs/re/ui.md` §5's
own caveat); H3 and F2.3's owner checklist close that gap.

## Input routing

SDL3 events go to the UI first, then to the world: the viewer's existing `SDL_PollEvent` loop
(`viewer.cpp`) gains a UI-dispatch call before `dev_tool_step`/the world's own key handling. A UI
widget that consumes an event (a click inside its rectangle, a key its screen binds) stops it
there; everything else falls through unchanged to the existing camera/dev-tool/world input exactly
as today. **The developer tool keeps its own key** (unchanged, per the dispatch's own rule 0.7):
it is not a UI.2 screen and this ADR does not route its toggle through the new widget dispatch.

## Text

Rendered with the original's `.zei` bitmap fonts (already parsed, `anno::formats`), from
Windows-1252 strings loaded from `Texte.dat` at runtime (never a string literal in `src/`, per
CLAUDE.md's format-spec rule and the dispatch's own "no original... UI strings... in git"). A
screen names its `Texte.dat` section/line in `data/ui/`, exactly as `construction.txt` already
names `tooltip_section` per category; the glyphs are decoded and blitted the same way an element's
texture is, through the same atlas.

## Atlases

The `ToolGfx`/`fenster`/`statusbar`/`B_*` textures UI.2 draws from are uploaded **once**, into the
compositor's existing sprite atlas (`docs/render/gpu-chunks.md` §4), not a second, UI-only atlas:
one atlas, one eviction policy (none, for chrome — UI frames are static and few compared to the
world's sprite set, so they sit in the atlas's existing "static" allocation class and are never
candidates for the "hot" class's LRU). **No texture is uploaded per frame**: a UI element's frame
id is requested once per screen-open (or once ever, for the always-on HUD), exactly like a chunk's
first-use upload; V6 measures this directly (no upload counted across idle frames).

## The command boundary

The UI is a **consumer**: it reads the published, read-only snapshot (`OpeningView`/`PlaySnapshot`,
already how the developer tool and `DevScript` read state) and sends commands through the existing
command path (`BuildCmd`, `DemolishCmd`, and whichever of `LoadCmd`/`UnloadCmd`/`FoundCmd` a screen
exposes) — the same `PlayRunner::post_command` entry every other consumer uses. **It never mutates
sim state directly**: no widget holds a reference into `World`, and V3 is a test that opening,
browsing and closing every menu without issuing a command leaves the state hash unchanged.

## Rendering

**A GPU path and a CPU reference path, compared pixel by pixel** (CLAUDE.md's own rule): the GPU
path blits atlas-resident UI textures with the existing straight-alpha sprite instance draw
(`gpu.wgsl`'s `sprite_vs`/`sprite_fs`); the CPU path decodes and blits the same elements in plain
C++ (closely related to, but not the same code as, `tools/ui_layout.py`'s own Python
`compose_screen`/`blit_rgba`, which is the **oracle**, not the shipped CPU path — the shipped CPU
renderer is new C++ in `src/render`/`src/app`, checked *against* the oracle, same as every other
CPU-vs-GPU-vs-oracle triple this project already keeps). Exact match is the target (as the world's
existing `over`/atlas-texel contract already achieves, §5(c) of `gpu-chunks.md`): UI sprites are
not blended against moving water or ships, so there is no float-rounding source to tolerate in the
first place.

## Testing

**The oracle pixel tests, with their masks and negative controls** — already built in UI.1/UI.1.F2
(`tools/ui_layout.py mock`/`compare`, channel 48, fraction 0.08, `data/ui/masks.txt`) — are reused
as the acceptance gate for the *shipped* renderer, not just the Python oracle: H4/C4/V4 run
`compare` against the shipped app's own screenshot, not only against the oracle's own mock, so a
regression in the real renderer is caught the same way a wrong `data/ui/` fact would be. V5's
mutation checks (a wrong cell type, an accepted click on a locked cell, a click that sends no
command, a wrong displayed cost) must each fail a test, per the dispatch.

## QUESTION R2: the default surface in `--play`

- **(a) The original UI on, with the developer tool on its key — recommended.** It is the owner's
  chosen test surface (M3 decision, 2026-10-30, `docs/plan/lan-mvp.md` §1); every future
  build/demolish playtest should exercise the real path by default, not a developer-only one.
- **(b) The developer tool, as today, with the original UI on a switch.** Lower risk while UI.2 is
  new (a bug in the UI never blocks a playtest that only needs the dev tool), at the cost of the
  owner having to remember to switch for the tests that matter.

Until answered: **(a) implemented behind `--ui classic|dev` (default `classic`, i.e. the
recommendation)**, so the owner can get (b)'s behaviour with one flag without a second build. Noted
on the playtest card (K).

## QUESTION R3: the UI scale at 1440p and 4K

The original draws at 1024 × 768; a modern display is commonly 2560 × 1440 or 3840 × 2160.

- **Integer factors 2 / 3.** Sharp (nearest-neighbour on crisp original pixel art), but 2× of
  1024 × 768 is 2048 × 1536 (black bars on a 2560 × 1440 or wider display) and 3× is
  3072 × 2304 (same on 4K) — the chrome does not fill the screen's native resolution.
- **A fractional factor** (e.g. 2560/1024 = 2.5, 3840/1024 = 3.75). Fills the screen exactly, at
  the cost of resampling the original's pixel art (soft edges on the `.tex` sprites and the `.zei`
  glyphs) — the same trade-off `docs/re/ui.md` §5 already flags for the five in-game resolutions
  the original itself offers, just continuous instead of five discrete choices.
- **A player setting with a default.** Either of the above, exposed as a option instead of fixed;
  more implementation surface (a settings screen, a saved preference) for a decision neither of
  the two options above removes.

No screenshots at 2560×1440/3840×2160 exist yet to compare sharpness directly (this ADR is written
before H renders a frame); H3 will attach them once the HUD exists. **Recommendation, provisional
on those screenshots: the fractional factor**, so the chrome always fills the window at the cost of
some softness on text/sprite edges — consistent with how most modern games scale a fixed-reference
UI, and avoidable later behind a player setting (the third option) if the softness proves
objectionable. Implemented behind a switch (R1's "scale factor from R3" is read from one
configuration value, not hardcoded), so changing the default later is a one-line change, not a
redesign.

## What this does not decide

- **Which screens exist and what each placeholder means** (M).
- **The construction menu's lock authority** (the sim's `construction_eligibility`, unchanged by
  this ADR — C2).
- **Any new sim behaviour.** This ADR adds no `src/sim` change; a UI.2 screen that needs one stops
  and asks, per the dispatch's rule 0.2.
