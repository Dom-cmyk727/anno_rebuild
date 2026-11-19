# FISHER-VIEW-1 (FISHER-HE-1 step B), the viewer can show the fisher rule: landing review (2026-11-16)

**Reviewer:** the Claude Code orchestrator (landing review; the UI lane did its own).
**Head:** `task/FISHER-VIEW-1` `eb11f0c6`; it contains main `4b104d3a` by a merge, so a landing merge has the branch's
own tree. App code and test tooling only.

## Verdict

**ACCEPTED for landing.** It is the first landing under the owner's rule of 2026-11-16: the Mac runs the full
game-file suite on `eb11f0c6`; the Windows PC runs the GPU tests and the app lanes with the game files.

## What was checked

- **`--fisher-rules on|off`** (`--play` only, default off): the world is created with `ExtensionContext::fisher_rules`;
  the sim rule stays behind that flag. `World::create` gets the serial executor explicitly, which is its default,
  so nothing else changes.
- **`--player <n>`** (`--play` only): the developer tool's, the snapshot's and `--commands`' player; a shipped
  scenario seats its human elsewhere than slot 1 (`02Trade01`: player 2).
- Both switches are refused with `--view` and `--ship-start`, and written into `session.txt`.
- **The readout:** a Farm's output, raw stock and workers out as script facts next to the game time, and in the
  title line.
- **Tests** (game files): the open control with the switch on (a worker out, output above 0 by game 48.4 s, raw
  3.00 t after three catches); the same build with the switch off (all 0 at 60 s); the closed site with the switch
  on (a worker out, release with stock 0 by build + 19,720 ticks, a second worker by build + 19,890). On main the
  window refuses the new switches.
- **Whether a released fisher is drawn is not touched** (FISHER-RE-4 is open).
- No sim or render change.

## Step C: the settings for the owner's clean History Edition run

Prepared by the UI lane and given to the owner: windowed and maximised on the 2560 x 1440 monitor, interface scale
1.0, the closest zoom, the starting orientation, normal speed, no other screen, the tutorial's welcome text left
open (closing it moves the camera), the camera placed on the site before building and not touched, the hut not
selected during the run, the recording at 2560 x 1440 and 60 fps from before the build to at least 2:30 after, two
fresh games (the closed site, then the open control). The rebuild's matching view: `--play` on `02Trade01` with
`--player 2 --fisher-rules on --group 0` and the same client size, registered on the background.
