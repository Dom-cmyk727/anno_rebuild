# UI-KEYS-1: the original's game keys (F5-F8 speed, P pause, F10 quick-save) — draft for review

UI lane, 2026-11-27. Stage S5 of `ui-options-mainmenu-1.md`, owner decision 2026-11-16 ("like the original": F5-F8
speed, P pause, F10 quick-save; the ship opening's quick-save moves from F5 to F10). Base `refs/candidates/cand13`.
About 3 h. OQ-906, OQ-908.

## The original's bindings (D, `AnnoFrame.dll` key handler `0x100071b0`, read for this draft)

The handler switches on the event kind (`[esp+0x114] & 3`). Each key posts a frame command through the frame's
slot `+0x5c(2, cmd, arg, 0)`, and a third switch (`0x10007514`, table `0x10007d14`, on cmd − 3) executes it.

| Key | Event branch, case | Posts | The command does | Evidence |
|---|---|---|---|---|
| F5 `0x74` | kind 1 + `0x80` (key down), switch `0x100071e0` (table `0x10007c2c`/`0x10007c00`, key − 0x11), case `0x10007252` | cmd `0x16`, arg 100 | case `0x10007a31`: `[frame+0x1820]` slot `+0x88(arg ≠ 100)` (OQ-906), then `0x10005f90(arg · 0.01)` = SetSpeed | D |
| F6 `0x75` | same, `0x1000726f` | `0x16`, 200 | same | D |
| F7 `0x76` | same, `0x1000728f` (checks net client `+0xfc`: refused for a net client, `tick-loop.md`) | `0x16`, 400 | same | D |
| F8 `0x77` | same, `0x100072c1` | `0x16`, 50 | same | D |
| F10 `0x79` | key down, `0x10007309` | cmd `0x12` | case `0x10007b38`: sets `[frame+0x1828]` = 1 and saves `Lastgame.sav` (string `0x10094c20`, ref `0x10007b44`) | D |
| F9 `0x78` | key down, `0x100072de`, only when `[frame+0x1828]` is set | cmd `0x13` | the quick-load, most likely (it needs a quick-save first) | D (gate); L (load) |
| P `0x50` | kind 0 (the other key event), switch `0x100073b2` (table `0x10007cc4`/`0x10007c98`, key − 0x2c), case `0x10007441`, only when `[frame+0xc]` is set | cmd `0x15`, arg `0x80` | case `0x1000772e`: flips bit `0x80` of the frame flags `[frame+0x16ec]`, then `0x10004290(0x10000)`; how that reaches `SetSpeed(0)` and the pause box: OQ-908 | D; P = pause from the manual p4 and `f15_pause` (M) |

The same kind-0 switch also binds `,` `.` (turn), Delete, the digits, A, F, S, Print and F10 (cmd `0xc`). Those keys
are out of scope.

## The rebuild today

- **Speed:** `sim::SetSpeedCmd` exists (wire `0x7a`). `apply.cpp` `set_speed` accepts the host only and only original
  speeds (`is_original_speed`; 400 in single player only), and writes `WorldSettings::speed_percent`.
- **But the runner ignores a speed change:** `PlayRunner` fixes `step_ms_` once in its constructor from
  `multiplayer_step_ms(speed)`, which knows only 50/100/200. A posted SetSpeedCmd changes the settings, not the turn
  length.
- **Pause:** `PlayRunner::set_paused` (app level, no sim command in single player; `clock.hpp`), toggled by
  **Space** (`viewer.cpp`, a rebuild key).
- **F5:** the ship opening's quick-save (`sdl_input.cpp` `OpeningKey::Save`, `ship-opening.md` §4). Two scripts use
  it (`opening_resume`, `opening_sail`).
- **P:** the dev tool's "previous type" while it is on (B; `sdl_input.cpp:25`, Shift+P fast). One script uses it
  (`r1_held_publication`).

## Plan (app only; no new sim command)

1. **Keys, in `--play` and the opening:**
   - F5/F6/F7/F8 post `SetSpeedCmd` 100/200/400/50 through the runner's command queue (refusals show in the title as
     every refusal does);
   - P toggles pause (the existing `toggle_pause`; Space stays as the rebuild's extra);
   - F10 is the opening's quick-save; F5 no longer saves.
2. **The runner follows the speed:** after each turn it reads `world.settings().speed_percent` and recomputes the
   turn length at the turn boundary. Since the speed comes from the command stream, a replay follows the same
   changes. **400 % (single player):** the runner is turn-based (the multiplayer model, `multiplayer_step_ms` =
   percent / 10), while the original's single player steps per frame (`single_player_step_ms`). **Decision needed:**
   (a) 400 → 40 ms per step (percent / 10 extended; ADAPTATION, labelled), or (b) F7 refused until a single-player
   clock exists. **Recommendation (a).**
3. **Clash, P:**
   - the original binds P to pause whatever the cursor (the case checks only `[frame+0xc]`);
   - the dev tool's previous type moves from P to **M** (Shift+M fast; M is unbound);
   - `dev_script` gets `m`, and `r1_held_publication` uses it.
4. **Facts:** `game.speed` (the world's percent), `game.paused`.

## Tests

- A unit case per key: event → the command (or the pause call).
- A sim-free runner case: a SetSpeedCmd turn changes `step_ms` at the next turn, and 400 follows the decision.
- **Wiring `ui_keys`** from the default start view:
  - F8, F5, F6, F7 each give `game.speed` 50/100/200/400 (the four speed commands applied);
  - P: `game.paused` 1 and no turn published over N frames (`expect published` unchanged); P again: turns resume.
- **Opening scripts:**
  - `opening_resume` / `opening_sail` switch to `key f10`;
  - a check that F5 in the opening writes no save and changes the speed instead.
- **Failure proofs:** the old F5 binding restored makes the F5 check fail; the runner not following the speed makes
  the step check fail.
- **Gates:** app lanes, `^viewer_wiring_`, opening, replay lanes (the speed now changes turn length on replay); WSL.

## Not in it

- **The pause box** (`MESSAGEBOX_BK` at (222, 191), "Continue game" at (286, 500), pixel-exact against `f15_pause`):
  proposed as UI-KEYS-2, so this packet stays one concern.
- **F9 quick-load:** with S6b (load in game).
- **The speed indicator** behind `[frame+0x1820]`: OQ-906.
- **Multiplayer pause:** `clock.hpp`'s host-stops-turns; the LAN path already has its own.

## Owner test

```
anno1503 --play data\maps\lan5.txt --game-dir E:\anno1503\game\app --construction-menu on --layout data\ui
```

Press F8, F5, F6, F7: the game runs at 50 / 100 / 200 / 400 %. P pauses and P resumes. In the ship opening, F10 saves
and F5 changes the speed.

## Open questions

- **OQ-906:** what `[frame+0x1820]` slot `+0x88(speed ≠ 100)` shows: a speed indicator, most likely.
- **OQ-908:** how P's flag `0x80` at `[frame+0x16ec]` reaches `SetSpeed(0)` and the pause box.
- **Decision for ORCH III:** the 400 % step, (a) or (b) above.
