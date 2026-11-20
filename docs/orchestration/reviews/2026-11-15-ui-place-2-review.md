# UI-PLACE-2, Esc and the right click step back: landing review (2026-11-15)

**Reviewer:** the Claude Code orchestrator (landing review; the UI lane did its own).
**Head:** `task/UI-PLACE-2` `5f29eb93` (code `67359b31`), stacked on UI-PLACE-1 `e2499746`. **CI:** green, `gate` included.
**The owner's answer to OQ-626**, in the UI lane's session: "Yes, like original". **The owner's OK to land**, here:
"Yes, land both" (with FISHER-1e).

## Verdict

**ACCEPTED for landing**, in the stacked candidate with FISHER-1e, after its game-file run.

## What was checked

- **`construction_step_back`** (Esc): the info panel open goes to the grid with nothing armed; else demolish on goes
  off; else the menu closes to the player overview. Each case names the original's Wine-runner shot.
- **A held Esc acts once:** its auto-repeats are swallowed, so it neither steps further nor quits the window.
- **`construction_right_click`:** on the map or the info panel it puts away the armed building and the demolish
  mode and shows the grid; on the grid it closes the menu. A right press that moves beyond the click slop stays
  the window's pan.
- **One case is not captured from the original** and says so in the code: a right click on the grid with a
  building armed is treated like Esc on the grid.
- **New script verbs** `rclick_px`, `rdrag_px`, `key-held`; a new wiring case `viewer_wiring_ui_place_escape`; four
  existing scripts follow the one-step Esc.
- **No sim or render change.** The app only.
