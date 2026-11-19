# UI-PLACE-1, demolish mode and the pickaxe: landing review (2026-11-15)

**Reviewer:** the Claude Code orchestrator (landing review; the UI lane did its own).
**Head:** `task/UI-PLACE-1` `e2499746` (code `eb0bd06a`), on main `cc7af72c`. **CI:** green, `gate` included.
**The owner's go for the packet:** "Yes, fix both", given in the UI lane's session after the `ui_4` findings.

## Verdict

**ACCEPTED for landing; landing waits for the owner's OK here and a game-file run.**

## What was checked

- **The evidence came first.** Two Wine-runner passes of the GOG original showed that its cancel-X keeps the building
  armed, so the reported "X does not cancel" was already faithful. Only `docs/re/ui.md` 2.6 changed for it, with
  the measured table (shot names only; the shots stay in `cache/`).
- **`DevTool::arm`:** a menu cell clicked while demolishing leaves the demolish mode, as the original does.
- **`DevTool::toggle_demolish`:** the pickaxe works with nothing armed and, turned off again, leaves nothing armed;
  with a building armed it toggles demolish and keeps the building. `demolish_only_` is cleared on every path that
  turns the tool off, and `retarget` settles it.
- **`viewer.cpp`:** the pickaxe click calls `toggle_demolish`, which sends no command, as before. A new script fact
  `tool.mode`.
- **No sim or render change.** The app only.
- **Tests:** a unit test that fails 3 of 9 assertions without the arm fix; a new wiring case
  `viewer_wiring_ui_place_demolish` (game files); two cut-down scripts fail on main at the two bugs.
- **Open, the owner's:** OQ-626 (the original's Esc and right click go back to the grid with nothing armed) and
  OQ-627 (the orange frame on the armed cell).
