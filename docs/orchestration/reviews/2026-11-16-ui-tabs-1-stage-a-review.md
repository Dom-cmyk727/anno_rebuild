# UI-TABS-1 stage A: the orchestrator's review (2026-11-16)

`task/UI-TABS-1` `a79499cf` (five commits on the packet `6d20d16e`, from main `ac02d4a2`), the UI lane. App and data
only: `hud_tabs.hpp` and `.cpp` (new), `viewer.cpp`, `ui_mock`, `ui_layout`, `hud.txt`, tests and wiring scripts,
`docs/re/ui.md` section 5.4, OQ-641 to 644.

## Checked

- **The code read.** Two of its addresses disassembled by the orchestrator in the GOG `View.dll`: the player
  view's sub-tab loop ends with `cmp esi, 5` at `0x1007f504`, the city view's with `cmp esi, 8` at `0x1006adb9`,
  and both step the frame by 2 per sub-tab (`add edi, 2`), which is the idle and selected pair the note describes.
- **No count in code.** The views, their sub-tab counts, frames, keys and bodies are `hud.txt` data; `hud_views`
  refuses a layout with a gap or repeat in the indices, a sub-tab without frames or body, a view without a sub-tab,
  or a key claimed twice. The switching (`HudTabs`) holds no SDL and no drawing.
- **The switching rule.** A main tab shows its view at the sub-tab it showed last; a sub-tab press selects it; the
  keys D, C and S go to player sub-tabs 1, 2 and 3; Esc on the construction grid gives the overview. Each rule
  carries its address.
- **The changed expectation is justified.** "The bust from the overview opens the list" (fix round R2 of
  UI-WINDOW-1) rested on a shot taken after key D, as the runner's action log shows; the code re-shows the last
  sub-tab. Three wiring scripts were changed for that reason and say so; the four oracle-shot tests are unchanged.
- **Tests.** `hud_tabs_test.cpp` (257 lines), the wiring scripts `ui_subtabs` and `ui_subtabs_draw`, the picture
  comparison `ui_subtabs_compare` against 10 GOG shots with a negative control; the lane's focused set 79 of 79
  with the oracle folders set; a named failure proof (reverting the press handling fails `ui_subtabs` at line 24).

## Notes

- **N1.** In the classic surface the letter keys that `hud.txt` binds are taken by the panel, so S (ship list)
  no longer pans there, as D already did not. That is the original's behaviour; `--ui dev` keeps both as pan keys.
- **N2.** OQ-641 stays open: the History Edition recording shows the ship list on a bust click after the ship
  view's tabs were used, which the rule "re-show the last sub-tab" would explain; not proven.
- **N3.** The History Edition check is from a compressed video only; the bodies of most sub-tabs are named
  placeholders (OQ-644), by the packet's design.

## Verdict

**ACCEPTED, once CI is green on `a79499cf`** (running at review time). Next: the owner tests stage A in the viewer;
stage B only after that. It lands with the next candidate, with the owner's OK.
