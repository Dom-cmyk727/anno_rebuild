# UI-MINIMAP-1: the minimap, drawn and working like the History Edition's

**Status:** written 2026-11-16 by the UI lane on the owner's request for a new job per lane (relayed by
the orchestrator). Branch `task/UI-MINIMAP-1` from main `4b104d3a`.

**Owner's rule (2026-11-16):** the reference is the **History Edition**, judged on pixels; the GOG build is
for reading code. Bit-identical UI is the base (owner, 2026-11-14); changes come later as options.

**What is wrong today** (owner test `ui_4.mp4`, 2026-11-15): the minimap is a see-through hole onto the
world drawn under the panel; its +/−, magnifier and rotate buttons do nothing.

**What the History Edition shows** (`E:\anno1503-worktrees\he-ui-captures\he_ui_1_video\c_minimap6.jpg`,
the owner's `ui_5.mp4`): a parchment with the whole world as a diamond; islands olive, the sea pale
parchment with faint lines and a compass rose at the top; a white box for the main view (its size follows
the zoom); small markers (a green ship dot). Below: a magnifier with + and −, two rotate arrows; at the top
right a + and a −. Rectangles of the chrome are measured (`data/ui/hud.txt`, `docs/re/ui.md` 2.1-2.2:
`WELTKARTE` atlas (779, 0) 245 × 248, `KARTEPLUS` gadgets, `LUPE`, compass chevrons).

## Step A: evidence (no rebuild code)

1. **GOG code read** (View.dll, static; `docs/re/ui.md` gives the gadget creation at `0x10034388`): the
   minimap gadget's draw routine - what fills the diamond (per-tile colour source and palette for land,
   coast, water; the parchment), the world-to-minimap transform (scale, origin, diamond orientation), the
   view box (colour, thickness, how its corners come from the camera), the markers (ships, other players,
   alerts: `MINIMAP_ALERT`), the compass rose; the click handlers of the minimap itself and of its six
   `0xb09` gadgets (+, −, two bar ids, two chevrons) and the `LUPE`. Findings with addresses into
   `docs/re/ui.md` (a new section "Minimap"); guesses into `docs/open-questions.md`.
2. **History Edition pixels** from the owner's recording (recipe below): what each control does, the
   view box per zoom level, a click and a drag on the minimap, the markers. Event table with frames.
3. Report: what the rebuild must draw and do, every unknown named.

## Step B: draw it (after A is reviewed)

The diamond's content from the sim's islands and the world size, the view box from the camera, ship and
player markers from the snapshot; drawn into the existing chrome at the measured rectangle, at any window
size (UI-WINDOW-1's transform). Pixel comparison against the History Edition frames at matched views.

## Step C: make it work (after B)

Left click (and drag, if HE does) on the minimap moves the main view; the buttons as HE does them. The
rotate arrows (chevrons) turn the whole view (GOG commands 0xc / 0xd, `docs/re/ui.md` 2.9). That is the
render lane's packet **VIEW-TURN-1** (orchestrator, 2026-11-16): in this packet the chevrons stay dead,
marked as waiting for it, and nothing of the turn is built here.

**Data, not limits** (orchestrator 2026-11-16, CLAUDE.md): the original's 256 x 256 world image, its
10 colours x 16 shades, the up to 8 alert sprites and the minimap zoom steps 0.15 to 1.0 come in as
defaults (data/config) that a larger map or a mod can change, never as fixed arrays or constants.

**Step A.1 result** (2026-11-16): `docs/re/ui.md` 2.9 and OQ-628..634 - AnnoFrame draws the minimap under
the `WELTKARTE` chrome's hole from AnnoWelt's 256 x 256 owner/shade image; the buttons are main zoom,
minimap zoom and view turn; a click centres the camera, no drag.

**Fix round result** (2026-11-16, after the orchestrator's History Edition check; `docs/re/ui.md` 2.9 "Fix round"): F1 the scroll's rolled left end (`fenster.tex` id 3 at (745, 49)) is drawn in every UI path and is chrome; F2 the parchment is the History Edition's 258 x 130 bilinear quad with 32-bit colours (`minimap.txt` `parchment_quad`, `colour_depth`; the GOG fit selectable); F3 no change (the fringe is the shade rule's, olive shows it too); F4 `[CAMARAPOS]` is `CAMERA_MEMORY`'s slot, the HE starts at level 1 on the player's ship (OQ-638 open); F5 (orchestrator) the viewer's closest zoom is 84 x sqrt(2) = 118.8 px (`kZoomEndFactor`), the HE's 84 px: the 1.42; unchanged, the owner decides.

## Owner recipe for the History Edition recording (step A.2)

History Edition, interface scale 1.0, window maximised; OBS at native 2560 × 1440 (Settings → Video:
base = output resolution), 60 fps, high quality. Any running game (e.g. "Trade & Diplomacy" or a
continuous game). About 3 minutes, narrated:
1. Hover over each minimap control for 2 s (tooltips): top-right +, top-right −, magnifier +, magnifier −,
   left rotate arrow, right rotate arrow.
2. Click each once, 3 s apart, and undo it (− after +, the other arrow after an arrow).
3. Left-click three different places on the minimap, 3 s apart; right-click once on it; then hold the left
   button on the minimap and drag slowly.
4. Zoom the main map through all levels with the mouse wheel and scroll it a little with the keys, so the
   white box shows how it follows.

**Rules:** no original pixels or code text in git (captures stay outside the repository); small steps,
each reported to the orchestrator; landing stays with the orchestrator.
