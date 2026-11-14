# UI-INFO-1 review (UI lane, 2026-11-14)

**Verdict: ACCEPT WITH FINDINGS** on `a2b6c1c9`; findings 2–8 fixed in fix round F, head **`9d151067`**
(CI run 43541629138, every job green incl. `gate`). Finding 1 waits for the owner's decision. The UI lane's
independent reviewer (a subagent with no part in the work) re-measured with its own scripts; the UI lane verified the
fix-round commits, the GPU-test change and CI.

## What the reviewer verified itself

| Claim | Result |
|---|---|
| Name, four material figures, gold, upkeep, chrome | 0 differing px on **all 15** GOG info shots (own composition from raw HEAD, PROD3, Texte.dat and `zei.py`) |
| RGB565 reduction before the compare | legitimate: every pixel of the 22 GOG info shots lies on the rounded 16-bit levels, so no 24-bit difference can hide |
| Chrome fixes (upkeep icon removed, cancel-X 33×35, right arrow 988) | each old variant costs 1,372 / 95 / 1,021 bad px; all three confirmed |
| Centring rule (floor) | rounding up or a 1-px shift costs 258–978 px; floor confirmed |
| View.dll addresses | all confirmed with capstone/pefile (0x1003007f/8d/ba, 0x1002fbac, 0x1002ff95, 0x100301b0..c5, 0x1003026b/d, 0x1008d919 `_itoa`) |
| HE claims | `fenster.inc` ids +5 and pixel-identical; HEAD, PROD3, English names and MainSmall identical for 15/15 |
| No sim/render change; viewer.cpp only at the draw calls; nothing leaked | confirmed |
| Tests real | focused selection 23/23 executed and passed, 0 skipped; a 1-px name shift makes `info_panel_pixel_check` and `info-compare` fail |

Not verified: Linux/macOS game-file runs; Wine's 16-bit conversion vs native Windows.

## Findings

1. **HIGH, owner decision: the preview.** 12 of 15 types differ from GOG by 5,007–11,237 px; the fit is a
   placeholder without evidence (OQ-612). Keeping it is closer to the original than an empty box (without it,
   807, 810 and 1302 exceed the 8 % UI.1 tolerance). The reviewer recommends keeping it, recorded as a known
   deviation from the owner's bit-identical rule. Fix round F split the code into the proven path (a sprite that
   fits the box, unscaled and centred) and one named `preview_placeholder_fit`. **Open: the owner's choice;
   follow-up UI-INFO-2** reads the preview gadget's draw path (View.dll from 0x1002fb30: which sprite sets, the
   float scale, the filter, the anchor, entrance arrows, overdraw, rotation).
2. MEDIUM, GPU test could not reject a wrong panel: **fixed** `ee35c028` (exact outside the 7×7 corner; the control must
   exceed the limit; GPU on the 4090 102,471/102,471, control rejected).
3. LOW, upkeep columns differ on 11 shots, not 7: **fixed** `15a0697f`.
4. LOW, address cited mid-instruction: **fixed** `e507e64e`.
5. LOW, missing upkeep drawn as 0: **fixed** `3fc74191` (draws nothing).
6. LOW, HE View.dll is x86-64, OQ-613's route: **fixed** `5955a92e`.
7. LOW, step-5 "what I saw" missing: **fixed** `9d151067`.
8. LOW, no automated GOG compare: **added** `e7a9ba20` (`info_panel_gog_compare`, skips without
   `ANNO_UI_ORACLE_SHOTS`).
9. FUTURE: text and shadow colours are C++/Python constants; move them into `data/ui`.

Secondary lens (`game-dev:ui-ux-game`, notes only, never a deviation): the rotate arrows do nothing yet and the
preview does not rotate (OQ-612); check whether the original marks unaffordable costs.

## Landing

Lands through the board orchestrator (land.py, the owner's OK, the game-file run) once the owner has decided finding 1.
