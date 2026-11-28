# UI-PAUSE-2: the pause box's text, button label and placement match the original

- **Status:** draft 2026-11-28 by the orchestrator for the UI lane; needs the orchestrator's EGM review before
  release. Follow-up to UI-PAUSE-1 (landed in candidate 14, `main` `d3f7bd3c`); the owner found the mismatch
  ("it doesnt match our current state", relayed by ORCH III, 2026-11-28).
- **Branch:** `task/UI-PAUSE-2`, base origin/main.
- **Size:** one concern (the pause box), about 2-3 h.

## The finding

Evidence in `E:\1503_test_videos\`:
- `he_pause_menu_2026-11-28.webp`: the owner's History Edition shot, German;
- `ours_pause_d3f7bd3c_paused.png`, `ours_pause_d3f7bd3c_running.png`: our build, `--play lan5 --ui classic
  --construction-menu on`, maximised to 2576 x 1408, P pressed.

| | HE (owner's shot) | ours, `d3f7bd3c` |
|---|---|---|
| Text line | "Pause !" | "Player bankrupt!" |
| Button label | "Weiterspielen" (Continue game) | "Back" |
| Placement | parchment centred in the play area at that window size, map visible around it | top left, near the 1024 x 768 coordinates (222, 191), unscaled |
| Inside the box | text near the vertical middle, button lower left | text higher |

## Cause of the text, as read by the orchestrator (verify it)

`data/ui/hud.txt` has `pause_text ... text_section MESSAGEBOX text_index 27` and `pause_label ... text_index 19`.
`text_index` is 0-based (`ui_layout.hpp`). In the GOG English `Texte.dat`, section `MESSAGEBOX` (0-based) holds:
- 16 "Continue game"
- 19 "Back"
- 24 "Pause!"
- 27 "Player bankrupt!"

Both indices are 3 too high. The section just before it, `STARTUP:MESSAGEBOX`, has exactly 3 lines. **Hypothesis:** the original's string ids 27 and 19 count across both sections, so the original's id table starts 3 lines earlier than our section index. That fits both numbers, but nobody has read it in the original's text loader.

## What to do

0. **Prove the offset before you change a number.**
   - Find where the original turns MESSAGEBOX string ids 27 and 19 into text: the show function `0x1000c6a0`, from
     UI-PAUSE-1's research, and the text loader it calls.
   - Then do one of two things:
     - confirm that the ids count from `STARTUP:MESSAGEBOX`, and record that in `docs/re/ui.md` §9.4 with the
       addresses;
     - or find the real rule.
   - **Check the other `text_section MESSAGEBOX` users:** grep `hud.txt` and the code for every other MESSAGEBOX
     index taken from the original's ids. If the offset is a rule, they are wrong too; list them and fix them in this
     packet.
   - **If the offset does not hold,** stop and report it.
1. **Fix the text and the label** at their source:
   - Either correct the two indices, or make `text_section MESSAGEBOX` resolve the original's ids. Choose the one
     that matches step 0's rule, and say why.
   - The result must be "Pause!" and "Continue game" in GOG English, and "Pause !" and "Weiterspielen" in the German
     files. The co-dev has the German GOG build; if you cannot read a German `Texte.dat`, check the German strings
     against the owner's HE shot and say so.
2. **Placement.**
   - **The rule:** owner decision 2026-11-13, ADR 0004 R3: the UI scales to the window.
   - **Find out whether the original centres the box at large window sizes**, or only HE does:
     - GOG runs at fixed 1024 x 768, so it can only show the box at that size; HE is the reference for live tests
       (owner rule 2026-11-16).
     - HE's scaling rule is UI-HE-1's (`docs/re/ui.md`).
   - **Place the parchment, the text and the button as one unit**, centred in the play area (the window minus the HUD
     chrome, as HE shows it). Keep the 1024 x 768 positions of §9.4 as the unit's internal layout.
   - **If HE's rule differs from simple centring,** follow HE and record the rule.
3. **Layout inside the box.** After steps 1-2, compare the text's and the button's positions inside the parchment
   with HE and with GOG's `f15_pause` at 1024 x 768. Fix any offset that the wrong string length does not explain.
4. **`docs/re/ui.md` line 98** says the pause parchment "hides the map", but HE shows the map around it, and §9.4
   gives a 579 x 386 box. Correct the sentence, saying what each edition shows.
5. **Tests:**
   - extend the pause row of `tools/test/ui_dynamic_draw_check.py` so it checks the two strings' content;
   - extend `tools/test/viewer_wiring/ui_pause.script` so it checks the box's position at a window size other than
     1024 x 768;
   - the existing click proofs must still pass at their coordinates.

## Acceptance

- The step 0 evidence: addresses and the rule, in `docs/re/ui.md` §9.4.
- A screenshot of our build paused at 2576 x 1408 and at 1024 x 768, next to the owner's HE shot, under
  `C:\Users\altin\Pictures\anno-review\UI-PAUSE-2\`.
- The tests of step 5 pass on msvc (only the changed lanes), WSL `check_unix_warnings.sh` passes, and hosted CI
  passes on the exact head.
- The owner checks the pause box in the viewer before landing.
- The report follows the 2026-11-28 owner rules: a numstat line, no check edited just to make it pass, at most 2
  fix rounds.

## Files

- `data/ui/hud.txt`, and the `text_section` resolver if step 1 needs it (`src/app/src/ui_layout.cpp`);
- the pause drawing in `src/app/src/ui_mock.cpp` and the click hit-test in `src/app/src/viewer.cpp` (they must use
  the same placement);
- `docs/re/ui.md`;
- `tools/test/ui_dynamic_draw_check.py`, `tools/test/viewer_wiring/ui_pause.script`;
- `docs/orchestration/REPORTS.md`.
