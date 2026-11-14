# UI-INFO-2: the info panel's rotate arrows turn the building

**RELEASED 2026-11-14** by the UI lane (session "UI Executor") on the owner's choice ("UI-INFO-2 first").
Owner of the work: the UI lane's executor subagent. Size: about 3–4 hours, one concern (the placement rotation as
the info panel shows and changes it). Report READY FOR REVIEW; the UI lane reviews it, then the board orchestrator.

## Why

- Owner playtests 2026-11-14 (`ui_1.mp4`, `ui_3.mp4`): "Cant rotate the buildings in the rotation window yet." The
  arrows left and right of the info panel's preview do nothing; the preview always shows one view.
- Cause on main `01a4d540`: `rotate_left` / `rotate_right` (`data/ui/hud.txt` lines 52–54, click centres 142–143)
  are drawn but have no click action (`construction_menu.cpp`); the preview draws Anlagen sprite set 0, zoom group 0,
  frame 0 whatever the rotation (`ui_mock.cpp` `preview_sprite`, ~124). Only the keys T / `,` / `.` turn the
  placement (`dev_input.hpp` `Rotate`/`RotateBack`, `sdl_input.hpp`).
- HE evidence (owner's video `HE_UI_1.mp4`, analysis copied to
  `E:\anno1503-worktrees\he-ui-captures\he_ui_1_video\HE_UI_1-analysis.md` with its crops; §1, §2 01:57–02:03,
  §3 B7, §4(a)): the arrows carry the tooltips "Links drehen" / "Rechts drehen"; each click turns the Pionierhaus
  picture by one 90° step to a **different drawn view, not a mirror** (four views: `c_pion_rot4.jpg`, sequence
  `d_pion_rot5.jpg`); the map ghost afterwards plausibly shows the panel's last view (not proven).

## Done means

1. A left click on `rotate_left` / `rotate_right` (press and release on the arrow, at any window size and UI scale
   — reuse UI-WINDOW-1's transform and chrome consumption) turns the armed type's rotation by one step, in the
   direction the original uses for that arrow. The keys and the arrows change **one** rotation state: the preview,
   the map ghost and the placed building agree after any mix of keys and clicks.
2. The preview shows the armed type at the current rotation, with the sprite the original picks for that view
   (sprite set / zoom group / frame per rotation, from evidence). At rotation 0 the output is unchanged: every
   existing oracle test (`info_panel_gog_compare`, `live_screen_matches_oracle`, `ui_dynamic_draw_check`, the
   `ui_*` viewer-wiring tests) stays green with no new mask and no looser tolerance.
3. Every rule cites evidence; what cannot be settled is registered in `docs/open-questions.md` (next free OQ
   number after the highest on main) and the code carries a flagged default, as UI-INFO-1 did.

**Out of scope (next packet UI-INFO-2b, write down what you learn about them):** the green entrance arrows in the
preview; the scaler for sprites larger than the 135 × 135 box (OQ-612's placeholder stays); tooltips (OQ-541);
the founding (Kontor) cursor's own rotation rule (`opening_ui.hpp` `kFoundingCursorRotation`, owner rule
2026-11-03) — if the Kontor is armed, the arrows may turn the cursor's turn exactly as `,`/`.` do there, nothing
more.

## What to settle, with evidence

1. **Which arrow is which direction.** GOG `View.dll` builds the arrows at 0x1002fd58 (gadget 33 × 38) inside the
   panel builder 0x1002fb30..0x10030138; find each gadget's click handler and what it changes (the cursor's rotation
   byte +0x46, `AnnoFrame` 0x10049340 / 0x100493e0, is what the keys change — `opening_ui.hpp` ~101). Cross-check
   against the key mapping in `sdl_input.hpp` (~39: the rotation byte went 0, 3, 2 under `.` and 2, 3, 0, 1 under
   `,`) and the tooltip lines in `Texte.dat` (find the "Links drehen" / "Rechts drehen" keys).
2. **Which sprite each rotation shows in the preview.** Read the 0xb17 preview gadget's draw (0x1002fb30 on, its
   update 0x10030140) for how the rotation selects the Anlagen sprite (set, group, frame or a direction offset);
   cross-check with how the map draws a building at rotation r (`render`/`assets` code that resolves Anlagen
   sprites by rotation — read it, do not change it). Visual checks: GOG shots `f13_place_rot_dot.png`,
   `f13_place_rot_comma.png`, `f13_hover_rotate_left.png`, `f13_hover_rotate_right.png` against `f13_place_land.png`
   (do their panels show a turned preview?); the HE crops `c_pion_rot4.jpg` / `d_pion_rot5.jpg` (four Pionierhaus
   views, video-scaled — use them for which view comes next, not for pixels). Zoom in on every capture you use.
3. **Where the rotation lives.** Today the window keeps it in `DevInput::rotation_` (window thread). Keep one owner;
   the arrows send the same `Rotate`/`RotateBack` the keys send, unless the evidence says otherwise.
4. If an unscaled type's rotated preview can be checked pixel-exact against a GOG shot, add it to
   `info_panel_gog_compare` (or a sibling test). If no GOG shot shows a turned preview and you need one, you may
   capture it with the AnnoHarness Wine runner (pre-authorized; music and sounds off, skip the intros one at a time,
   `docs/testing/`); do not start or drive the History Edition or the original on the Windows desktop — if you need
   such a capture, ask in the report.

## Base, branch, rules

- Worktree `E:\anno1503-worktrees\anno1503-UI-INFO-2`, branch `task/UI-INFO-2`, from main `01a4d540`
  (UI-WINDOW-1 landed). This packet is its first commit. Push the branch at each checkpoint (ordinary push; never
  `main`, never force). If `origin/main` moves, merge it at a clean checkpoint and re-run the focused tests.
- Read first: `CLAUDE.md`, `docs/testing/evidence-contract.md`, `docs/testing/README.md`, `docs/re/ui.md` §2.5–2.6
  and §5.2, OQ-612, `data/ui/hud.txt`, `construction_menu.hpp/.cpp`, `ui_mock.hpp/.cpp` (preview), `dev_input.hpp`,
  `sdl_input.hpp`, `viewer.cpp` (`construction_handle`, the info-panel inputs), their tests and
  `tools/test/viewer_wiring.py` (scripts in `tools/test/viewer_wiring/`).
- **The owner's evidence rule (2026-11-14), binding:** when in doubt, check both the original (GOG `game/app`, code
  and files) and the History Edition (`E:\SteamLibrary\steamapps\common\Anno 1503 - History Edition`, files; its
  `View.dll` is x86-64, OQ-613) and zoom in on visual captures. The original is the bit-identical base.
- **No sim change, no `src/render` change.** A rotation already exists in placement commands; this is UI wiring and
  the preview. If something truly needs `sim` or `render`, stop that step with a QUESTION.
- **No hardcoded limits:** the rotation count (4), arrow ids and any per-rotation sprite offset are data or come from
  `Anlagen.dat`, with the original values as defaults and their evidence.
- **Publication (GitHub is public):** no pixels, screenshots or UI strings in git. GOG shots:
  `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\`; HE captures and the video analysis:
  `E:\anno1503-worktrees\he-ui-captures\` (read-only for you). Copy what you use into this worktree's `cache/`.

## Steps

1. RE items 1–3 above; write the findings into `docs/re/ui.md` §2.5 (arrows, preview per rotation) with addresses.
2. Click action for the two arrows in `construction_menu` (+ `viewer.cpp` routing), sharing the keys' rotation.
3. The preview at the current rotation (`ui_mock.cpp`), CPU and GPU paths, cached per (type, rotation).
4. Tests: a unit test of the arrow → rotation mapping; a viewer-wiring script that arms a type, clicks each arrow
   (at 1024 × 768 and at one other size), checks a new fact (e.g. `ui.info.rotation`) and that the map ghost's
   rotation matches; a preview test at rotations 0..3 for one unscaled type (pixel-exact against a GOG shot where one
   exists, else against the reference decode of the evidenced sprite). Each new test must fail on main `01a4d540`
   (say how you proved it).
5. **Look at it yourself** (board §1 item 10): `--play data\maps\lan5.txt --construction-menu on --game-dir
   E:\anno1503\game\app --layout data\ui`, arm the Pionierhaus and a stand, click both arrows four times each,
   capture the window, put the four views next to `c_pion_rot4.jpg`, and say in the report what you saw.

## Gates and report

- `cmake --preset msvc` + `cmake --build --preset msvc`; focused tests with `ANNO1503_GAME_DIR=E:\anno1503\game\app`,
  `ANNO_TEST_THREADS=16`, `ANNO_UI_ORACLE_SHOTS` (your `cache/` copy of the GOG shots) and `ANNO_UI_HE_CAPTURES` set:
  **every** oracle-shot UI test (`live_screen_matches_oracle`, `ui_dynamic_draw_check`, `info_panel_gog_compare`,
  `ui_panels_match_gog`, `ui_window_matches_he`, `ui_mock_matches_oracle`, `ui placement`) plus the
  `viewer_wiring_ui_*` tests and the tests you added — counts classified (executed pass / skip / fail). clang-format
  23.1.3 on changed files; `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`; hosted CI green on the pushed head.
  The whole suite is not this packet's gate.
- One entry at the bottom of `docs/orchestration/REPORTS.md` on this branch, ≤ 60 lines: what changed, gate commands
  with classified counts, NOT_RUN, Decisions and reasoning, the OQs you registered, what you learned for UI-INFO-2b
  (entrance arrows), QUESTIONs. Logs stay in `cache/`.
- No reviewer subagents of your own. Wait for long jobs on their own PID or output file, never on "no python
  running". Stop at about 5 hours at a clean point and report what is left.
