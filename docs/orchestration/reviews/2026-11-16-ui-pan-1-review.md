# UI-PAN-1, a click no longer pans the map: landing review (2026-11-16)

**Reviewer:** the Claude Code orchestrator (landing review; the UI lane did its own).
**Head:** `task/UI-PAN-1` `65a9632c`, on main `67a960c9`. **CI:** green, `gate` included.
**The owner's answer**, in the UI lane's session (2026-11-16): "Yes, no click pan".

## Verdict

**ACCEPTED for landing; landing waits for the owner's OK here and a game-file run.**

## What was checked

- **The cause was found first.** The map shift in the owner's `ui_4` recording was a right click's jitter, since
  the right button pans by design; a left click in demolish mode does not pan.
- **The change** (`viewer.cpp`): a pan starts only when the pointer has left the 4 px click slop of the press, and
  then covers the whole way from the press, so the point under the pointer still follows it. A press that stays
  inside the slop moves nothing.
- **A feel change, decided by the owner**, with no original evidence either way; recorded in ADR 0004 K-2 and
  `docs/app/game-shell.md`. It is app input, not a sim rule.
- **Tests:** a new wiring case `viewer_wiring_ui_click_no_pan` (it includes the recording's demolish right click)
  fails on main's app code at its first jittery click.
- **One commit holds code and report** under a docs title (the executor's commit step failed and a rewrite was
  blocked). The landing merge's message names the change.
- **No sim or render change.**
