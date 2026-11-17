# History Edition recordings for the UI panel packets: one set-up

**DRAFT 2026-11-16** (UI-PANELS-PREP fix round, after the orchestrator's review). One recording set-up
for the owner, used by the recipes of the four draft packets `ui-warehouse-1.md`,
`ui-ships-diplomacy-1.md`, `ui-statistics-1.md` and `ui-options-mainmenu-1.md`. Each recipe lists only
*what* to do; *how* to record is said once, here. The owner records the History Edition (HE) with OBS
(owner rule 2026-11-16: the HE is the picture reference).

## Why one set-up, and why this one

- Only lossless frames serve a bit-identical base. The earlier videos (`HE_UI_1.mp4`, `ui_5.mp4`) are
  lossy 1920 x 1080 recordings of a 2542 x 1351 client, so every position measured on them is
  0.75 x screen and about ±2 px (`docs/re/ui-ships-diplomacy.md` M-HE; `docs/re/ui-warehouse-trade.md`
  section 3).
- At a window of 1200 lines or fewer the HE forces its interface scale to 1.0 (`he-ui-scale.md`
  section 1, HE `AnnoDisplay.dll` RVAs 0x2d943, 0x2da17), and its UI layer is the client size,
  clamped to at least 1024 x 768 (section 2, 0x2cdb4..0x2cdca). A **1024 x 768 client** therefore draws the in-game UI 1:1
  in the original's 1024 x 768 layout, and the full-screen statistics frame, which the HE fits to the
  client height (`docs/re/ui-statistics.md` section 4), at scale 1.0 too. The 800 x 600 menus are
  scaled by 768 / 600 in any HE window (`docs/re/ui-options-mainmenu.md` section 3.1).
- The packets' four earlier set-ups (1920 x 1080 windowed; 1024 x 768 with Steam PNGs; 1080p or
  larger; 2542 x 1351) are replaced by this one.

## Set-up (once, the same for every clip)

1. **HE window:** HE settings (the gears, "Spieleinstellungen"): Anzeigemodus windowed, Auflösung
   **1024 x 768**. If the list does not offer it, take the smallest size it offers and say which at
   the start of the clip (any height of 1200 or less still gives interface scale 1.0).
2. **Sound:** HE music and sound at 0 % (silent-run rule); never drag a volume slider to the right,
   only hover it. Language as in the earlier recordings (German), unless a recipe says otherwise.
3. **OBS source:** a capture of the HE window's client area only (no title bar, no border).
4. **OBS video:** Settings → Video: base (canvas) resolution and output (scaled) resolution both
   **equal to the client size** (1024 x 768), so OBS never rescales; 60 fps.
5. **OBS recording:** Settings → Output → Recording: recording quality **lossless**; Settings →
   Advanced → colour format RGB if OBS offers it for the lossless encoder (else I444, full range;
   say which). A microphone track for the narration.
6. **Exact stills:** bind OBS's "Screenshot Output" hotkey. Where a recipe says **SHOT**, move the mouse
   off the panel, hold still 2 s and press the hotkey; the PNG is written from the canvas, with no
   video encoding in between. Hover states are separate shots with the mouse on the element.
7. **Test clip first (10 s):** record the HUD idle with one SHOT and hand both over. The UI lane checks
   that a static HUD element (for example the construction tab `MAINTAB`) matches its `fenster.tex`
   texture pixel for pixel on the PNG and on a video frame; only then are the recipes recorded. If it
   does not match, the settings above are corrected first (the result goes into this file).
8. **While recording:** narrate each click in one short sentence before making it; move the mouse
   slowly; hold every state 2-3 s with the mouse still; hold each hover 2 s for the tooltip.
9. **Files:** one clip per recipe part, named `E:\1503_test_videos\HE_<packet>_<part>.mkv` (packet
   WH, SHIP, STAT, OPT; part as in the recipe, e.g. `HE_WH_R1.mkv`), the SHOT PNGs in a folder of the
   same name. Keep the raw files; no pixels, frames or texts go into git. The UI lane extracts frames
   with ffmpeg into `cache/`.

## Exceptions

Only where a recipe step says so, a clip is recorded at another window size, with the same OBS
settings and the canvas equal to that client size: the options packet's optional menu-size step and
its interface-scale check (a scale above 1.0 needs more than 1200 lines), and a second size for the
HE's full-screen frame fit (OQ-678). Each such step names its size.
