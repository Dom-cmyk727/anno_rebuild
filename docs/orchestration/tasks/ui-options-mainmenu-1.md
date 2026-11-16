# UI-OPTIONS-MAINMENU-1 (draft): the in-game options and the main menu

**DRAFT 2026-11-16** (UI-PANELS-PREP). Not released. The UI lane releases one stage at a time; each stage
is one concern of about 2–4 hours and its own packet when released. Evidence:
[`docs/re/ui-options-mainmenu.md`](../../re/ui-options-mainmenu.md) (the spec; section numbers below refer to
it), [`docs/re/ui.md`](../../re/ui.md), [`docs/re/he-ui-scale.md`](../../re/he-ui-scale.md).

## Why

The rebuild starts from the command line and has no main menu, no options pages, no named saves, no load
from inside a game and no audio. The original's gear tab (save, load, music, speech, rating, statistics,
active help, exit) and its 800 × 600 main menu (profile, single player, choose game, options, credits,
quit) are specified to rectangle, texture id, text line and handler (spec §2). Owner rules: the History
Edition is the picture reference, the GOG DLLs give the addresses, the original UI bit-identical is the
base, HE behaviour and changes are options; HE sound stays at 0 %.

## Owner decisions needed before the stages that use them

1. **F5:** the original sets 100 % speed with F5 and quick-saves with F10 (spec §1.2); the rebuild's ship
   opening quick-saves with F5 (`docs/app/ship-opening.md` §4). Proposal: original keys (F5–F8 speed,
   F10 quick-save, P pause) in play; the opening's F5 moves to F10. Needed for S5.
2. **Menu fit:** the original draws the 800 × 600 menus at the top-left of an 800 × 600 mode; HE scales
   them to the client height and centres them with border art (spec §3.1). Proposal: HE fit as the
   default window behaviour of the menus (the original needs a display-mode switch the rebuild does not
   do), original placement as an option. Needed for S3.
3. **Save files:** the original names saves by GUID with the display name inside (spec §2.6). Proposal:
   keep that scheme for the rebuild's own save frame (unique file, alias inside). Needed for S6.

**Owner's answers (2026-11-16, in the UI lane's session):**
1. F5: **like the original** - F5-F8 speed, F10 quick-save, P pause; the ship opening's quick-save moves
   from F5 to F10.
2. Menu fit: **neither of the two proposals** - "find and identify a modern state of the art solution that
   will adjust to the users resolution". S3 therefore starts with a short design study: how current games
   and engines fit fixed-size art menus to any resolution and aspect ratio (resolution-independent layout,
   anchoring, scaling rules, filtering, safe areas, very wide and very tall screens), with a recommendation
   for this menu and its assets, put to the owner before S3 is built. The original's 800 x 600 art stays
   the base picture.
3. Save files: **like the original** - GUID file names, the display name inside the save.
4. Statistics memory slots (`docs/re/ui-statistics.md`, OQ-675): **in the saved game**, as the original
   (player state, saved and loaded with the game, synchronised in multiplayer).

## Stages

Every stage: no sim change unless it says so; layouts are data (`data/ui/`, ACCESS name + index, never a
raw id only, because HE ids differ: spec §3.1 item 3, §3.2 item 4); no pixels, frames or UI strings in git;
evidence cited per rule; guesses go to `docs/open-questions.md`.

| Stage | Concern | Needs | Acceptance test | HE comparison |
|---|---|---|---|---|
| **S0** | Evidence still missing (no code) | owner HE recordings R1–R3 (below) | spec §6 OQ-680, 683, 684, 686, 687, 689 answered from the footage or narrowed; spec updated with M-HE rows and timestamps | the recordings are the comparison |
| **S1** | Menu layout data | spec §2.1–2.5 | `data/ui/menus.txt` (new) with main menu, single player, choose game, options, profile: every rect, Startup ACCESS + index, text section + line, handler name; a Python oracle check that each element's sprite matches the GOG shots `x_main_menu`, `x_singleplayer`, `x_choose_game`, `f16_options_graphics` at its rect (exact or mean ≤ 1/ch) | each rect checked on `he_29.5.png` / HE recording frames after the scale of spec §3.1 (mean ≤ 15/ch, as measured there) |
| **S2** | Options-panel layout data | spec §2.6 | `data/ui/hud.txt` gets the four sub-pages (`panel options_load` … `options_speech`) with every element of spec §2.6; `tools/test/ui_panels_compare.py` passes on `f15_options_y{360,410,460,510}`; the two weak matches (load 1490, save 1500 buttons) resolved or kept as OQ | the same elements on `he_108.0.png`, `he_109.25.png`, `he_111.5.png` (0.75 scale) within ±2 UI px; HE gears button (≈ 938, 372) added as an HE-only element behind the HE option |
| **S3** | Draw the main menu tree | S1; decision 2 | app screen state machine (main menu → single player → choose game → flag → start; profile; options; credits placeholder; quit); hover = second frame; CPU path compared pixel-exact with the GOG shots at 800 × 600; clicks hit per spec rects | window 2542 × 1351: menu at scale H/600 centred, compared with `he_29.5.png` (same tolerance as S1); border art per OQ-687 or flagged |
| **S4** | Draw the gear tab's five pages | S2 | `--play`: gear tab and key O open page 0; the sub-tabs switch pages; tooltips from `TOOLTIP:OPTION*`; CPU/GPU compare against the four GOG shots (masks only over live text and list rows) | side-by-side with `c_tabs_a/b.jpg` crops at UI scale 1.0 |
| **S5** | Speed and pause | decision 1 | F5/F6/F7/F8 post `SetSpeedCmd` 100/200/400/50 (400 single-player only, `is_original_speed`); P toggles `PlayRunner::set_paused` and shows the pause box (`MESSAGEBOX_BK` at (222, 191), button at (286, 500)); "Continue game" resumes; tests: key → command, pause → no turn published, box drawn pixel-exact against `f15_pause` (masked map) | recording R2's pause and speed segment (box position at scale 1.0, any HE speed indicator) |
| **S6** | Named saves and the load/save pages | S4, decision 3 | app: save with a display name into `<profile dir>/`, list (name, time) on both pages, delete, load in game (replaces the world, restarts the runner, hash equals the saved one), F10 = `Lastgame` save; tests: save → list → load round trip equal hash; delete; corrupt file refused with a reason; MP refusal per OQ-682 | R2's load/save segment: row look, default name, order, confirm boxes |
| **S7** | Main-menu flow functions | S3, S6 | Continue game loads the newest `Lastgame`; Load game lists saves; New game starts a scenario through the existing loaders with the chosen flag colour; Quit; profile create/select stored as the rebuild's own profile file (fields of `user-dat.md`); tests per action | R1's flow end to end |
| **S8a** | Audio core and music page | S4; an audio library choice (ADR) | music from `Data/music.dat` `GRUPPE_DEFAULT`, 18 tracks, ticks and volume persisted in the profile (`SONGS`, `VOLUME` semantics: 0 = full, −250 = min, −25 dB at min per spec §2.6); default volume for test runs 0 (silent rule); tests with a null audio device | R2 music segment (names, ticks, slider ends) |
| **S8b** | Speech page | S8a; sim message events | 11 categories (`SPEECHGROUP`) as a bit mask, volume; which sim events map to which category registered as OQ if not traced | R2 speech segment |
| **S9** | Main-menu Options and HE settings as options | S3; OQ-680/686 answered | original page: 5 modes (supported ones enabled), details toggle, Back, stored in the rebuild's settings; HE overlay behaviour (UI scale, window mode) as a rules/app option, not original | R1 options segment |

Gates per stage: `cmake --preset msvc`, `cmake --build --preset msvc`, the focused `ctest --preset msvc -R
<pattern>` with `ANNO1503_GAME_DIR` and the GOG shots path, counts classified; clang-format 23.1.3;
`wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`; one REPORTS entry ≤ 60 lines. The whole suite is not a
stage gate.

## Recordings the owner can make (HE, closes the visual gaps)

Settings for every recording: HE at **Interface-Skalierung 1.0**, windowed 2542 × 1351 as in `HE_UI_1`
(or fullscreen 2560 × 1440; say which at the start); OBS at the monitor's native resolution, 60 fps,
lossless or high bitrate; **HE music and sound stay at 0 %** (do not drag any volume slider to the right;
only hover them); narrate what you click before you click it. Move the mouse slowly and hold still about
2 s on every hover so the tooltip and hover frame are visible. Name the files `HE_OPT_R1.mp4` etc. in
`E:\1503_test_videos\`.

**R1 – main menu tree (about 6 min).**
1. Start the game, wait on the main menu 5 s without moving the mouse (clean frame, version line).
2. Hover each plank top to bottom (Profil, Einzelspieler, Mehrspieler, Credits, Optionen, Beenden), 2 s each.
3. Profil → profile screen: hold 5 s; hover each button; back without changing anything.
4. **Optionen** → hold 5 s; scroll through everything it shows; hover each entry; leave without
   confirming a change (OQ-680, OQ-686). If it is the HE settings window, open every page it has.
5. Credits → 5 s → ESC (say whether ESC or a click ends it).
6. Einzelspieler → hover the five planks; **Spiel laden** → hold 5 s on the list, hover a row, Zurück;
   **Kampagne** → hold 3 s, Zurück.
7. **Neues Spiel** → hold 5 s; scroll the list to its end one arrow click per second, then back to the
   top (OQ-685); click one row of each heading (2 s each); **Auftrag** → 5 s → Zurück.
8. OK → castle screen: hold 5 s, hover each flag; **Einstellungen** → hold 5 s, click every toggle once
   and back again (narrate the texts), Zurück; pick a flag → loading screen → game (stop when in game).
9. Optional, 30 s: the same main menu with the window at 1920 × 1080 and at 1280 × 1024 (resize via
   the HE settings), to show the border art at other aspect ratios (OQ-687).

**R2 – in-game options (about 6 min), Endlosspiel, unpaused unless a step says P.**
1. Click the gear tab; hold 3 s. Hover each of the five sub-tabs (2 s) and each page-0 icon:
   statistics chart, gears, map/help, laurel, door (2 s each).
2. Click the map/help icon twice (on, off), 2 s after each (active help states).
3. **Laden:** hold 3 s; hover the arrows and the big button; click a row; hold 2 s.
4. **Speichern:** hold 3 s; click the name plank, type `Test1`, click the save icon; hold 3 s on the list
   (order, name); select `Test1`, click the delete icon, record any question box, answer it; hold 2 s.
   Then save once more over an existing row and record the box (OQ-682).
5. **Musik:** hold 3 s; scroll the list to the end; untick and retick one song; hover the play icon;
   hover the slider (do not drag). **Sprachausgabe:** the same.
6. Page 0 → **door**: record the question box (OQ-683); answer No.
7. Press **P**: hold 5 s on the pause box; click its button. Press P again and try the gear tab while
   paused (OQ-684).
8. Press F8, F5, F6, F7, F5 with 5 s between them, narrating each; note any on-screen change.
9. Last: press F10 (quick save), open Laden and show the list; then door → Yes, and record where it goes.

**R3 – scale check (about 1 min).** In HE settings set Interface-Skalierung 1.5, return to the game,
open the gear tab and page Speichern, hold 5 s; set it back to 1.0.

Stop each recording on the last step; keep the raw files. The UI lane extracts frames and measures them
(the spec's method in §2.6 and §3.1: sprite match against the GOG textures scaled by the frame's
factor, positions relative to a matched background).
