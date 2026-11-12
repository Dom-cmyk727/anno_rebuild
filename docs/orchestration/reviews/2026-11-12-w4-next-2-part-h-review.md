# W4-NEXT-2 part H review (2026-11-12)

**Update, 2026-11-12: round H4 ACCEPTED** ([below](#round-h4-accepted)); part H lands as one change.

**Verdict: CHANGES REQUESTED (small, docs only).** Part H ([packet](../tasks/w4-next-2.md), part H) corrects both
findings of the [acceptance](2026-11-11-w4-next-2-acceptance.md), and every figure it states reproduces. One of
H1's three steps was not done, though, and the report says it was (finding 1). Round H4 below finishes it on the same
branch; then part H lands as one change.

**Reviewer:** the Claude Code orchestrator (Opus 5.5): the diff, the executor's script (run here), the scenario
folders, and its own measurements of the figures OQ-565 and OQ-567 cite. The report names no independent read-only
review of part H; H4 asks for one.

**Reviewed head:** `task/W4-docs-h` `48044b6d` (two commits on `main` `7babb1f2`; 5 files, +139, docs only). Hosted CI
green on both commits: `f99c7679` (run 43123758319) and `48044b6d` (run 43128594919).

## What holds

- **H1's figures.** The orchestrator ran `cache/h1-sprite-sad-measure.py` (sha256 `150eff60…126e4`, matching the
  report). On both kept copies of each shot:
  - zoom + (`fenster.tex` id 44 at (959, 7)) gives 2.885 and compass west (id 48 at (965, 140)) gives 3.088, plain,
    per pixel, on `hud_idle_running`: the committed figures;
  - id 974 at (783, 252) gives 32.07 per channel (96.21 per pixel), grey-key, on `f15_selected_house`;
  - the portrait (`Pioniere.tex` id 1 at (812, 355)) gives 45.52 per channel (136.56 per pixel), grey-key.
- **H1's unit statements** in `docs/re/ui.md` §2 and §9.7, the `data/ui/hud.txt` comment and OQ-567's new bullet are
  correct: the plain method is per pixel and grey-key divides by 3. G6's 0.962/1.029 are 2.885/3.088 divided by 3.
- **H2.** `game/app/Scenes/` holds `0010SingleEndlos/01_1024_easy_256.sze`, `0200Single/Single_06.sze`,
  `Single_16.sze`, `Single_24.sze` and `0100Kampagne01/S01_08_01.sze`, as the new paragraph says. Widening the next
  attempt's first check (whether the New Game list reaches `0200Single` at all) is right.
- **No old text edited.** Every correction is new text, as the packet asked.

## Findings

| # | Severity | Finding |
|---|---|---|
| 1 | Low | H1's third step was not done, and the report says it was. |
| 2 | Low | OQ-567 still calls F6's 32.20 and 30.57 "plain" with no unit. They are per channel. |
| 3 | Note | Two of H1's new paragraphs cite `docs/re/ui.md` by line number, and the numbers are already wrong. |

**1. The restatement of OQ-565's and OQ-567's own figures (Low).** The packet asked to restate the plain sprite-sad
numbers that OQ-565 and OQ-567 cite, in the per-pixel unit, calibrated, each with its shot and position. The report's
"OQ disposition" says the figures these OQs cite are zoom + and compass west. They are not; those two appear only in
G6's calibration paragraph. The OQs' own figures are:
- **OQ-565:** 58.7 for `fenster.tex` id 5 (`MAX_PIC`) on `missclick_forest` (ad hoc, stride-4), and the committed 82
  for `MAINUNTERGRUND` id 4 at (779, 244) on `hud_idle_running`.
- **OQ-567:** the committed 78.7 for `INFOUNTERGRUND` id 33 (`docs/re/ui.md` §9.1, no position given); the ad hoc,
  stride-4/6 figures 91.2 (id 1511), 87.5 (id 677), 79.9–86.8 (id 1128), ~81–86 (id 1023), 84.8 (id 4) and 96.2
  (id 974).

The orchestrator measured them with the calibrated plain method: per pixel, alpha ≥ 8, every pixel, a full search over
the box the OQ text states. Script: the orchestrator's scratchpad, not kept; H4 measures them again with its own.

| Figure cited | Sprite | Shot | Box searched | Calibrated result |
|---|---|---|---|---|
| 58.7 (OQ-565) | id 5, 245 × 524 | `missclick_forest` | x 765–795, y 230–260 | **59.51** at (779, 242), inside the box |
| 82 (OQ-565) | id 4 | `hud_idle_running` | at (779, 244) | **82.40**: the committed figure reproduces |
| 78.7 (OQ-567) | id 33, 220 × 466 | `f15_kontor`, `r2_hud_kontor` | x 765–800, y 220–300 | 93.28 at (800, 299), **on the box edge**: not reproduced in this box |
| 91.2 (OQ-567) | id 1511 | `click_kontor_building` | x 750–830, y 210–290 | 83.38 at (800, 289), **on the box edge** |
| 87.5 (OQ-567) | id 677 | `click_kontor_building` | same | 88.57 at (779, 290), **on the box edge** |
| 79.9–86.8 (OQ-567) | id 1128 | `initial_view` | same | 78.22 at (779, 290), **on the box edge** |
| ~81–86 (OQ-567) | id 1023 | `click_kontor_building` | same | 86.63 at (830, 290), **on the box edge** |
| 84.8 (OQ-567) | id 4 | `click_kontor_building` | same | 85.11 at (830, 290), **on the box edge** |

The `w4next2` shots are in `E:\anno1503\cache\orig\w4next2-ui-captures\d91-20261111-100242\shots\`. The UI-1 shots are
in `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\`.

None of these decides an OQ. `MAX_PIC`'s 59.51 stays supportive at the position the ad hoc pass found. Every Kontor
candidate's minimum lies on the edge of the box OQ-567 states, so that box does not contain the best fit, and the
figures say nothing yet. The docs still carry the uncalibrated numbers, though, and the report's claim is wrong.

**2. F6's unit (Low).** OQ-567's F6 bullet reports "plain opaque-pixel mean **32.20** (112,547 px)" and **30.57**
without the overlays. Both are per channel: the orchestrator measures id 974 at (783, 252) on `f15_selected_house`,
plain with no grey-key, at 96.60 per pixel, 32.20 per channel, n = 112,547. H1 asked for the unit wherever a figure is
stated.

**3. Line numbers (Note).** In `docs/re/ui.md`, §2's H1 paragraph cites "line 155's zoom + and line 158's compass
west". Those are `main`'s numbers. With the paragraph's own two lines they become 157 and 160, and 158 is zoom −.
§9.7's H1 paragraph cites "line 659", the branch's number, which is 657 on `main`. Name the table rows instead.

## Disposition

Round H4 (the [packet](../tasks/w4-next-2.md), part H4) goes to the Windows executor by message, on the same branch
`task/W4-docs-h`. Then part H lands as one change, with this review, with the owner's OK.

## Round H4: accepted

**Verdict: ACCEPTED.** All three findings are fixed.

**Reviewed head:** `task/W4-docs-h` `1f1d3889`, three commits on `48044b6d`, docs only:
- one new H4 bullet each in OQ-565 and OQ-567;
- the line references fixed in part H's own two `docs/re/ui.md` paragraphs;
- a REPORTS entry.

**What the orchestrator checked:**
- **The script.** `cache/h4-sprite-sad-measure.py` has the sha256 the report gives (`d5fba05e…ab82e`). It uses the plain
  method (per pixel, alpha ≥ 8, no stride, not divided by 3) and keeps every candidate fully on screen.
- **Every figure, by an independent implementation.** The orchestrator rewrote the same searches with numpy: same
  boxes, same clamp. Every figure, position and pixel count matches the H4 bullets exactly:
  - OQ-565: 59.51 at (779, 242) and 82.40;
  - id 33: 93.28, then 92.61 at (801, 298) on both shots;
  - ids 1511, 677, 1128: 85.99, 88.57 and 78.22 in the stated box, then 72.35, 61.56 and 53.33 at (779, 300);
  - id 1023: 93.32, then 88.08 at (779, 303);
  - id 4: 90.47 at (779, 244);
  - F6: 96.60 per pixel = 32.20 per channel (n = 112,547), and 91.70 = 30.57 without the two overlays (n = 99,247).
- **No better placement.** A search over the whole right part of the screen (x 600–779, y 0–300) finds the same
  minima for ids 677, 1128 and 1511.
- **The cited text.** `docs/re/ui.md` §2 states both bars (below 15; 40–50 for the atlas under content). The §2.1 table
  has the zoom + and compass west rows. §9.7's opening paragraph gives the portrait's 34.2.
- **Nothing on `main` edited.** `main` holds none of part H's paragraphs, so the two in-place fixes touch only
  unlanded text. Everything else is new text.
- **The independent review.** A fresh read-only agent reran the script: ACCEPT.
- **Hosted CI green on all three commits:**
  - `aee04720`, run 43155260486;
  - `5fe93868`, run 43158805168, attempt 2. Attempt 1 failed only `cpp (macos-clang)`: `anno_test_support_tests.F3
    (D1, deaf)` INCOMPLETE, the known coordinator flake (the first review's finding 12), confirmed from the job log;
  - `1f1d3889`, run 43164531292.

**This review's own error.** Finding 1's table gives 83.38, 86.63 and 85.11 for ids 1511, 1023 and 4 on
`click_kontor_building`. Those positions put part of the 245-wide sprite off the 1024 × 768 screen, and the
orchestrator's script compared only the part on screen. H4's on-screen constraint is right for a panel plate, so H4's
figures replace those three.

**Two leads for OQ-567's next look.** These are the orchestrator's own measurements. Neither decides anything; they are
noted in OQ-567 too.
1. **Bottom-aligned placement.** The Kontor candidates' minima at (779, 300) and (779, 303) put the plate at the
   screen's bottom-right corner. `MAINUNTERGRUND` id 4 sits the same way at (779, 244), since 244 + 524 = 768. So the
   boundary is plausibly the plate's real draw position, not a search cut short.
2. **Id 974's committed position.** F6's (783, 252) puts 4 of id 974's 245 columns past the screen's right edge.
   On `f15_selected_house`:
   - grey-key at (779, 252) gives 31.69 per channel;
   - a grey-key full search over x 740–779, y 200–301 finds **23.15 per channel at (778, 300)**, bottom-aligned
     again;
   - neither is the committed 17.8.

**Disposition:** part H, with H4, lands as one change with this review, with the owner's OK.
