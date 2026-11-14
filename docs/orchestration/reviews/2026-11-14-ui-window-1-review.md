# UI-WINDOW-1 review (UI lane, 2026-11-14)

**Verdict: ACCEPT WITH FINDINGS** on `d9121492`; findings 1–7 fixed in fix round R1, head **`e107a8f6`**
(contains main `f54e36f0`; CI run 43591693332, all 12 jobs green incl. `gate`). The UI lane's independent reviewer
(a subagent with no part in the work) re-measured and re-disassembled on its own; the UI lane checked the fix round,
the scope and the F1 evidence itself.

Owner decision 2026-11-14 on ADR 0004 R3, verbatim: "1.0, HE rule optional". Recorded in ADR 0004 R3.

## What the reviewer verified itself

| Claim | Result |
|---|---|
| Transform = HE `AnnoDisplay` 0x2ccf0 step-down, 0x5a961 fit, `AnnoFrame` 0x22a26 click inverse, float32 + truncation; 1200-line rule (0x2d943/0x2da17) only under the option | own numpy model matches `anno_ui_mock` on 17 sizes (incl. 1023x767, 640x480, 999x1333, 3841x2161); default is identity at every size ≥ 1024 x 768 |
| GOG per-width picks `AnnoFrame.dll` 0x10063125 (status bar) / 0x10063304 (filler) | confirmed (1152 → 0x38, 1280 → 0x39, else 0; filler 0x3b / 0x3a / 0x3c) |
| `statusbar.tex` 55..59 = `statusbaraddon.tex` 56..60; FUELLER01/02 = FUELLER03's top 192/96 rows | confirmed, identical decodes and palettes |
| 2542 x 1351, s 1.0 vs HE capture | gold symbol, question mark, bottom 256 filler rows, tab band: 0.00 mean abs; remaining differences are live content (counters, city panel) or the registered adaptations (OQ-614/615) |
| 1024 x 768 bit-identity | every existing oracle test passes unchanged |
| Tests real | focused 36/36 executed and passed; a 1-px question-mark shift fails `ui placement` and `ui_window_matches_he` |
| No sim/render change, no pixels in git | confirmed |

## Findings and fix round R1

1. MEDIUM, `--ui-scale` stretch off at the bottom (uniform k; last client row black): **fixed** `3bd296a2`. HE fits
   the UI texture into the client at `AnnoDisplay` 0x5c9dd..0x5cb2a with `trunc(x·0.5 + 0.5)` margins, so a gap
   under a pixel vanishes and the quad covers the whole client (per-axis W/uw, H/uh); clicks keep the uniform k
   (getters 0x18940/0x18950, `AnnoFrame` 0x22a26). UI lane re-disassembled 0x5c9dd..0x5cb2a and 0x18940..0x18960.
   Tab band, gold symbol, question mark vs HE at s 1.5 and 2.0: mean abs 0.04–0.06 (bound 0.25); last row drawn;
   1:1 default unchanged. The ≤ 1 px draw/click difference is OQ-619.
2. MEDIUM, closing the menu (tab or Esc) left the building tool armed: **fixed** `76b84cfd`; test
   `viewer_wiring_ui_menu_close_disarms` fails on the old viewer.
3. LOW, status-bar comment hid that the original picks STATUSBAR at other widths: **fixed** `fef60e57`; tiling the
   1001-px bar is labelled the rebuild's adaptation (OQ-614), seams at x 1000/1001 and 1262.
4. LOW, OQ-617 narrowed (HE reads the chosen display mode, not the client height): **fixed** `22b6d940`.
5. LOW, the original widens the status-bar content at 1152/1280 (`AnnoFrame` 0x10063000/0x10063013, factor
   1.18/1.3): **registered** OQ-620 `224b73e2`; not implemented.
6. LOW, limits and lost presses: **fixed** `7c517361` (reference size from `hud.txt`, `--ui-scale` range from
   `UiScaleRule`, a lost press is dropped on the next press, focus loss or window leave).
7. LOW, test gaps: **fixed** `7c517361` (`viewer_wiring_ui_chrome_press`: a chrome drag does not pan, a map drag
   does). GPU at other sizes: checked by hand (1280x1024: 16 of 214,130 px differ, max 22; 900x600 identical;
   1920x1080), no CTest (a GPU window needs a desktop): **NOT_RUN** automated.

Performance: the GPU path reuses its UI canvas (`7be8ee0d`, "construction ui" median 5.27 → 4.63 ms). Follow-ups in
REPORTS: the non-1:1 paths recompose the whole layer each frame; `UiLayerScaler` compares about 4.7 MB per frame.

## Owner choices recorded (not blocking)

- OQ-614/615: use HE's own status-bar / column graphics when an HE install is present, or keep the GOG adaptation.
- OQ-616: how a window smaller than 1024 x 768 should look (today the UI is fitted into it; the map is not scaled).

Not verified: Linux/macOS game-file runs; the original at 1152/1280 (no captures); pop-up placement (OQ-618).

## Landing

Lands through the board orchestrator (land.py, the owner's OK, the game-file run).
