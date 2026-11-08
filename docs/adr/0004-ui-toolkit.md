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

**A GPU path and a CPU reference path, compared pixel by pixel** (CLAUDE.md's own rule). This ADR
first planned the GPU path as a reuse of the existing straight-alpha sprite instance draw
(`gpu.wgsl`'s `sprite_vs`/`sprite_fs`) — wrong once implemented (W4-UI-2, 2026-11-08): that path is
structurally tied to `render::SpriteDraw`'s world-tile-projected sprite records, which cannot carry
`.tex`/`.zei` UI content or fixed screen-space positions. The GPU path actually implemented
composes the same live state into a transparent RGBA canvas on the CPU
(`anno::app::draw_live_screen_rgba`, `ui_mock.hpp`/`.cpp`) and blends it over the already-rendered
frame with a full-screen pass that reuses the existing resample/zoom post-process's vertex stage
(`gpu.wgsl`'s `scale_vs`, new `ui_overlay_fs`). `draw_live_screen_rgba` is a separate function from
the CPU path's own `draw_live_screen_xrgb` — not shared code — but over the same data/ui/ layouts,
live eligibility/counters and digit-rectangle lookups; the two are not independent implementations
of those details, so the real safety net against them drifting apart is each path's own pixel
comparison against the UI.1 oracle (`ui_mock_matches_oracle`, `live_screen_matches_oracle
--renderer gpu`), not independence. The CPU path itself decodes and blits the same elements in
plain C++ (closely related to, but not the same code as, `tools/ui_layout.py`'s own Python
`compose_screen`/`blit_rgba`, which is the **oracle**, not the shipped CPU path — the shipped CPU
renderer is new C++ in `src/render`/`src/app`, checked *against* the oracle, same as every other
CPU-vs-GPU-vs-oracle triple this project already keeps). Exact match is the target on the CPU path;
the GPU path is checked within this project's own established F2.1 tolerance (channel ≤ 48,
fraction ≤ 0.08) rather than exact match, since the full-screen blend pass's own float sampling
(not present in the CPU path at all) is a real, if tiny, source of per-pixel difference — measured
at 28 of ~200,000+ checked pixels (fraction ~0.0001–0.0002), all at sprite-corner pixels, none in
digit/grid content.

## Testing

**The oracle pixel tests, with their masks and negative controls** — already built in UI.1/UI.1.F2
(`tools/ui_layout.py mock`/`compare`, channel 48, fraction 0.08, `data/ui/masks.txt`) — are reused
as the acceptance gate for the *shipped* renderer, not just the Python oracle: H4/C4/V4 run
`compare` against the shipped app's own screenshot, not only against the oracle's own mock, so a
regression in the real renderer is caught the same way a wrong `data/ui/` fact would be. V5's
mutation checks (a wrong cell type, an accepted click on a locked cell, a click that sends no
command, a wrong displayed cost) must each fail a test, per the dispatch.

## Screen tree: bound vs. placeholder (W4-UI-2 M)

Every screen `docs/re/ui.md` section 1's "In-game" table names (19, from a running Citizen-start
endless game), checked against what `--construction-menu on` actually wires up today, not
guessed: "bound" means a real click/key reaches real engine state and draws something live;
"placeholder" means the screen has no handler and no chrome at all yet; "partial" sits between
the two. The "waits on" column separates a true sim-side gap (a feature or aggregate the engine
does not compute yet) from a UI-side one (input routing and chrome this project has not built
yet, with the sim data already available) - conflating the two would misdirect whoever picks up
the next slice.

| # | Screen | Status | What's live now | Waits on |
|---|---|---|---|---|
| 1 | HUD idle | **Bound** | Chrome (`draw_live_screen_xrgb`) and the gold counter (H2), live, every frame | - |
| 2 | Settlement / Kontor panel | Placeholder | Nothing (no click target for "Kontor" exists in `construction_handle` or anywhere else) | UI: a click-routing seam for a non-menu panel. Sim: trade sliders would read `City::trade_lists` (already modelled) once a panel exists |
| 3 | Construction menu, 8 categories | **Bound** | `ConstructionMenu` + `draw_live_screen_xrgb`, all 8 categories (the draw path does not special-case which one); 2 of 8 screenshot-proven pixel-exact against the oracle (public, roads) | - |
| 4 | Building info / placement | Partial | A grid click resolves through `ConstructionMenu::click` to `Arm`/`DevTool::arm` (selects the type; the actual `BuildCmd` is the existing dev-tool path's own next click, on the world tile); `cell_info` already computes cost live from the registry (C3's own data path). No panel chrome, no cost/upkeep numbers are drawn - the panel itself has no screen rectangle in the live window yet | UI: the panel's own drawing slice (C1/C6, independent of fonts - fonts only draw numbers once there is a panel to draw them on). Sim: OQ-563 (an unbuilt type's upkeep has no live source) |
| 5 | Demolish | Partial | The pickaxe/cancel-X click resolves `ToggleDemolish` through `ConstructionMenu::click` and a real `DemolishCmd`-path `ToolInput` | UI: a distinct demolish-mode chrome/cursor (none drawn; today it is state with no visual) |
| 6 | Help / info mode | Placeholder | Nothing | UI only: a tab click handler and its own chrome; no sim gap is known |
| 7 | Military tab (= key D's panel) | Placeholder | Nothing | UI only (same panel as 9, same D collision) |
| 8 | Options tab | Placeholder | Nothing | UI only; client-side settings, no sim data needed |
| 9 | Player list (key D) | Placeholder | Nothing - **another real key collision**: D is already bound to pan right | UI: same collision pattern as 15/17 |
| 10 | Pause overlay (key P) | Placeholder | Nothing (the viewer's own developer controls do not bind P, so the key is free; the original's parchment-over-map chrome is not drawn) | UI only |
| 11-14 | Options load/save/music/speech | Placeholder | Nothing | UI only; load/save touch the existing save/load path, not a new sim feature |
| 15 | Ship list (key S) | Placeholder | Nothing - **a real key collision**: the viewer's own developer controls already bind S to pan | UI: needs its own key or a scheme that resolves the collision (mirrors the B/Esc precedent H1 already set for the construction menu) |
| 16 | City list (key C) | Placeholder | Nothing | UI only |
| 17 | Rating (key Q, full frame) | Placeholder | Nothing - **a real key collision**: the viewer's own developer controls already bind Q to quit | UI: same collision pattern as 15. Sim: whatever statistics the rating screen shows would need their own aggregation, not yet checked for existing equivalents |
| 18 | Selected house | Placeholder | Nothing (the same gap as 4 - this is the info panel for an already-built instance) | Same as 4 |
| 19 | Deselected right panel | Placeholder | Nothing | UI only; an empty-panel default, no sim feature needed |

Pre-game menus (main menu, singleplayer setup, `tools/orig/menus.json`) are out of this table:
`docs/re/ui.md` section 1 itself marks them "not the owner in-game test surface", and `--play`
starts past them - this ADR's whole scope is the in-game UI reachable from a running session.

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

- **The construction menu's lock authority** (the sim's `construction_eligibility`, unchanged by
  this ADR — C2).
- **Any new sim behaviour.** This ADR adds no `src/sim` *behaviour* change; a UI.2 screen that
  needs one stops and asks, per the dispatch's rule 0.2. It does add one new read-only sim query
  (`World::city_population`, wrapping the existing `residents_at_or_above` the stage pass already
  uses, for the HUD's residents counter) — explicitly allowed by rule 0.2 itself ("a new read-only
  sim query is allowed if it changes no state, stream, hash or draw"), not an exception to this
  bullet's own rule against new behaviour.
