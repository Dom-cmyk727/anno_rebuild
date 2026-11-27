# UI-PAUSE-1: the pause box's text and its "Continue game" button

- **Status:** draft 2026-11-27 by the orchestrator for the UI lane, from the lane's proposal. Revised after the EGM
  review, stages 5-7 (reports in the main checkout's `cache/egm/ui-pause-1/`, not in git).
- **Branches:** part A builds on `task/UI-KEYS-1` (`0595a0fe`, the pause box) alone. Part B adds a merge of
  `task/UI-PLANK-1` (`c5e6c432`, the text helpers `text_section` and `draw_centred_text`); it conflicts with UI-KEYS-1
  only in `docs/orchestration/REPORTS.md`, so keep both sides. Neither base is on `main` yet. "Lands" means the
  orchestrator accepts a part at handover; it reaches `main` after its bases do.
- **Size:** 3-4 h, including the two 1 h research limits (steps 1 and 4).

## Why

On `main`, Space pauses (`Viewer::toggle_pause()`), and the paused view already draws and hit-tests the parchment.
UI-KEYS-1 adds P and its wiring cases, and draws the pause box (`pause_parchment` and `pause_button` in
`data/ui/hud.txt`). The box shows no text, and a click on its button does nothing. In the GOG shot `f15_pause` the
box shows text, not identified yet (step 4), and a button labelled "Continue game" (`MESSAGEBOX_BUTTONS` id 1513,
`docs/re/ui.md` §9.4).

## Goals and non-goals

- **Part A:** a left click on the button does what the original's button does. Three outcomes count:
  - its handler is traced to an address and it resumes, like P;
  - or it is wired to P's path, as a labelled assumption in OQ-908, when no handler is found within the limit;
  - or the handler does something else, and the lane stops and reports (step 1).
- **Part B:** the box's text is drawn with 0 differing pixels against `f15_pause` (step 7). The label is drawn the
  same way, or shown to be part of sprite 1513 already (step 5).
- **Not part of this packet:**
  - P's own pause and resume stay as UI-KEYS-1 left them;
  - Esc or Enter on the box;
  - proving what the original does with a click on the parchment outside the button. Step 2 swallows it, as a
    labelled assumption;
  - the speech submenu in the shot's right panel (left over from the options probe, ui.md §9.4);
  - pressed or hover frames of the button (no capture of a held press, OQ-544);
  - windows larger than 1024 x 768. The parchment keeps its 1024 x 768 position (OQ-618), and the click rectangle
    follows the same mapping, but that is not checked separately.

## Alternatives and risks

- **Rejected: wiring the click to `toggle_pause()` without research.** The evidence rule (CLAUDE.md) forbids it; it
  stays only as the limited fallback of step 1.
- **Rejected: a generic message-box gadget.** It means more code, and nothing else uses one yet.
- **Rejected: a new text helper, or a string in the data.** UI-PLANK-1's helpers exist, and data/ui holds keys only.
- **Risk: the text source is a hypothesis.** It may come from `Texte.dat`'s `MESSAGEBOX` section, but no in-game
  message box has been seen to use it (ui.md line 126).
- **Risk: wrapped text.** If the text needs line wrapping or a layout of several lines, stop and report. There is no
  wrap engine in this packet.
- **Risk: `viewer.cpp`.** The render lane's RENDER-DIRECT-2c hook is in the same file.

## Implementation Plan

Disassembly reads run in WSL or Linux (Ghidra rejects 32-bit PE on a Windows host, CLAUDE.md). Parsed original
files count as evidence too. The PC is shared: ask the orchestrator for "PC free" before the first build. Line
numbers below are `main`'s; on the base, find the same places by the code quoted.

**Part A: the button.**
1. **Find what the button does.** The pause box is drawn from the original's message-box sprites (`MESSAGEBOX_BK`,
   `MESSAGEBOX_BUTTONS`; ui.md §9.4), so look for the message box's button handler and the command it sends. Record
   its address in `docs/re/ui.md`. Background: P sends AnnoFrame cmd `0x15` with argument `0x80`. Cmd `0x15` XORs its
   argument into the frame flags `[frame+0x16ec]`, so P flips bit `0x80` (`docs/orchestration/tasks/ui-keys-1-draft.md`
   on `task/UI-KEYS-1`). ui.md line 379: the minimap draws only while bit `0x20` of those flags is set, so check the
   button's argument too.
   - If it sends the same flip on a press, it resumes as P does.
   - If it acts on release, or does something else, stop before step 2 and send the orchestrator what you found.
     Part A is then re-planned.
   - If it is not found within about 1 h, wire it on the press, like the minimap and the tabs, as a labelled
     assumption in OQ-908.
   Create the OQ-908 entry in `docs/open-questions.md` in this step. It is UI-KEYS-1's open question "how P's flag
   `0x80` reaches `SetSpeed(0)` and the pause box". Steps 4 and 5 add to it.
2. **The click.** In `Viewer`'s left-press handler (`viewer.cpp`), directly after
   `const double px = ..., py = ...;` and before `minimap_press`, add:
   `if (paused_at_) { if (inside_tab("pause_button")) <P's resume call>; return true; }`.
   - Move the existing `inside_tab` lambda (it reads hud.txt's `pos` and `size`, right edge exclusive) above that
     line. Do not hard-code coordinates.
   - `<P's resume call>` is exactly what `case SDLK_P` calls on `task/UI-KEYS-1`.
   - Every other press while paused is swallowed, so a click on the parchment where it overlaps `tab_construction`
     no longer opens the construction menu. Record this in OQ-908 as an assumption: a modal message box.
3. **Test:** a wiring script, `tools/test/viewer_wiring/ui_pause.script`, registered in `src/app/CMakeLists.txt` like
   UI-KEYS-1's `ui_keys` case, with the same renderer and arguments. The script:
   - presses P;
   - clicks beside the button inside the parchment (600, 533): `game.paused` stays `1`;
   - clicks at the tab overlap (790, 200): `game.paused` stays `1`, and the construction menu stays shut;
   - clicks the button's centre (408, 533): `game.paused` becomes `0`.
   `game.paused` is UI-KEYS-1's fact; add no new one.

**Part B: the text.**
4. **Check the button sprite.** Does `MESSAGEBOX_BUTTONS` id `1513` already contain the label's pixels (glyph-match
   of the label's string against the sprite)? Some panel graphics contain their icons already: the upkeep icon is
   part of `INFOUNTERGRUND` (ui.md §2.5). If the label is baked in, draw no label, and search only for the box's text
   in step 5.
5. **Find the strings.** `Texte.dat` has a `MESSAGEBOX` section (ui.md line 62). Search it for the box's text (and
   the label, if step 4 did not find it baked in), and confirm by glyph-match on `f15_pause`. Use disassembly only
   if several keys match. Add the source to OQ-908. Timebox: about 1 h.
   - If no key matches, stop part B and report.
   - If the text needs wrapping, several lines, or a font, colour or alignment that UI-PLANK-1's helpers cannot
     express, stop and report.
6. **Draw.** Add hud.txt entries `pause_text` (and `pause_label`, if needed), with `text_section <SECTION>:<KEY>`
   as the `plank_text` entry does, plus the font file and position. In `ui_mock.cpp`, draw them with
   `draw_centred_text` after `blit_el("pause_button")` in both paused blocks: `draw_live_screen_xrgb` and
   `draw_live_screen_rgba_into`. If UI-PLANK-1's helpers duplicate main's `centred_text_x` path, report it; do not
   refactor here. Never put the string itself in hud.txt (data/ui rule, ui.md line 1163).
7. **Test:** extend row 10 of `tools/test/ui_dynamic_draw_check.py`. It already captures the live paused window and
   requires an exact CPU match for `pause_parchment` and `pause_button`. Add the text (and the label) to its
   expected image. Its deliberately wrong input, without the text, must be rejected. This test fails when the C++
   draw is removed.

If part B stops, report what you found; part A still stands.

## Files

- `docs/re/ui.md` and `docs/open-questions.md` (OQ-908): modify.
- `src/app/src/viewer.cpp`: modify, the click path only.
- `tools/test/viewer_wiring/ui_pause.script`: create. `src/app/CMakeLists.txt`: modify (its registration).
- Part B: `data/ui/hud.txt`, `src/app/src/ui_mock.cpp` and `tools/test/ui_dynamic_draw_check.py`: modify.
- `docs/orchestration/REPORTS.md`: append the report.

Nothing in `src/render` or `src/sim`. The render lane also edits `viewer.cpp` (the RENDER-DIRECT-2c hook), so limit
this packet's `viewer.cpp` change to the click path.

## Acceptance

- **Part A:** `ui_pause` and UI-KEYS-1's wiring cases (P still pauses and resumes) pass on msvc. The button's address
  is in ui.md, or the assumption is in OQ-908. A stop report from step 1 is also an accepted result.
- **Part B:** `ui_dynamic_draw_check.py` passes with the text (and label) as an exact CPU match, and rejects its wrong
  input. Or step 4 shows the label is baked in, or step 5 stopped with a report.
- Each new check is shown failing when its line of code is removed.
- WSL `check_unix_warnings.sh`, and hosted CI green on the exact head.
- **Owner check:** in the viewer, P shows the box with its text, matching `f15_pause`. A click on "Continue game"
  resumes the game, and a click elsewhere on the box does nothing.
- Report at the bottom of `docs/orchestration/REPORTS.md`, with "Decisions and reasoning", the egm-lite checklist
  path and its baseline commit.
