# UI-WINDOW-1 review (UI lane, 2026-11-14)

**Verdict: ACCEPT WITH FINDINGS** on `d9121492`; R2/R3 below, current head **`7cddd25f`**; findings 1–7 fixed in fix round R1, head **`e107a8f6`**
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

## Fix rounds R2 and R3 (owner playtest of `e107a8f6`, "fix then land")

The owner's playtest found three issues. The tabs only toggled the construction menu. The preview had no entrance
arrows and could not be rotated (that is OQ-612, follow-up UI-INFO-2). The status-bar counters were hidden
outside construction. R2 (`36b86873`) makes the four tabs switches, each showing its panel's static chrome. It also
makes the counters follow the local player's city on the island under the view centre. The UI lane's independent
reviewer re-checked R2 on its own: **ACCEPT WITH FINDINGS**.

| Check (reviewer, on its own) | Result |
|---|---|
| Selected tab per GOG shot | bust 13 on `hud_idle_running` / `r2_tab_military` / `r2_player_menu`; ? 14 on `r2_tab_help`; gears 15 on `r2_options`; construction 12 on `r2_construction_military`. The code follows it |
| Panel ACCESS ids re-matched (1185, 1187, 1007, 1407, 667, 1477/1478/1480, 1580) | each the unique best match |
| Counters rule | sea-level pick at the drawable centre, island rectangle (they cannot overlap, `island.cpp` 729-757), first city of the local player. Hidden over sea and over a city-less island; open as OQ-622 |
| Snapshot | built on the sim thread from const accessors only; no sim state written; the window reads the immutable snapshot |
| Texture cache `f9a49436` | keyed by (path, id), no palette input; the in-place blit equals the crop; 1024 x 768 oracle tests unchanged |
| Breaks | a moved tab fails `ui_tabs`; counters shown over sea fail `ui_counters_centred` |

Findings, all **fixed in R3** (`7cddd25f`):
1. MEDIUM. Clicks on the sub-tab strip x 762..778 reached the map. Fixed in `0555dee2`: the shown panel's
   elements count as chrome. `ui_tabs` clicks the strip on all four panels and fails on the old hit test.
2. MEDIUM. The panel compare had one 8 % bound per panel, so a moved or wrong small sprite passed. Fixed in
   `a6346d1f`: a bound per element (2 %, `menu_bk` 5 %). Both of the reviewer's breaks now fail (70.8 %, 61.0 %).
   The info shot's hover tooltip is masked as live text (84 x 18 px).
3. LOW. The statistics icon was drawn over the main menu's tabs. Fixed in `8b203d56`; main-menu bad pixels
   went from 1,665 to 0.
4. LOW. The no-op wording was wrong: the bust tab from the overview opens the list. Fixed in `d13add5f`.
5. and 6. LOW. The publish stats cleared one vector and read the clock while off. Fixed in `919c59fc`.
7. LOW. The selected-tab frame was untested. Fixed in `a6346d1f`: the four tabs are in each panel's compare.

The UI lane checked R3 itself: hosted CI run 43604656130 on `7cddd25f` passed every job, `gate` included. No file
under src/sim, src/render, crates or game changed. Focused run on this PC, with the game dir, the GOG shots and
the HE captures: 9 of 9 executed and passed (`ui_tabs`, `ui_panels_match_gog`, `ui_counters_centred`,
`ui_menu_close_disarms`, `ui_window_click_s10/s20`, `ui_window_matches_he`, `ui_mock_matches_oracle`,
`ui placement`). The executor's own focused gate was 98 of 98.

Measurements (R2, lan5): snapshot build median 41-47 to 47-49 us. Turn p95 is noisy, with the same 10 ms spikes
before and after. "construction ui" median: CPU 1024 x 768 1.98 to 0.36 ms; CPU 2542 x 1351 3.72 to 0.59 ms;
GPU 2.13 to 0.55 ms and 4.10 to 0.93 ms. The reviewer read these from the stderr artifacts and did not re-run
them.

Still open, not blocking:
- OQ-621: second tab clicks, Esc's return, the list background, the crest per player.
- OQ-622: the original over open sea or a foreign island.
- The panels' live names and figures (UI-TABS-1).
- The preview's arrows and rotation (UI-INFO-2).
- NOT_RUN: GPU tests as CTest, Linux/macOS game-file runs, the full suite.

## Landing

Lands through the board orchestrator (land.py, the owner's OK, the game-file run).
