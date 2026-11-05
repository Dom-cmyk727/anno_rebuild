# W4-OPENING-1 evidence (text only)

Branch `task/W4-opening-1`, created from the Windows publication `26107db089f558bc374208a4f414ca70f562e6f0`;
the accepted Mac pin `0c401c8d084cfd9feb8aec846595291f87f22cce` is merged by the ordinary merge `870fc83` (both are
ancestors of the head; neither source branch was touched). **Code head of every gate below: `d129bba6b4daf64cde72f84c37ec52ce918cde03`.**
Commits after it add documentation, this evidence and one change to the owner launcher `tools/play_ship_opening.ps1` (it restores
`ANNO_SAVE_DIR` when the game exits; parse-checked, and run once by hand after the change: the window opened on the RTX 4090 with the seat's ship, the variable was not left set; no gate runs it); no source, test, CMake or test-script change.

Everything under `extracts/` is a copy or an **EXTRACT** (marked in `extract-table.md`) of a git-ignored original under
`cache/gates/` of the Windows checkout `E:\anno1503-worktrees\anno1503-W4-opening-1`; the sha256 is of the original file as
it is on disk. No game data, screenshot, save or binary is in git. The files contain local paths and the local user name.
The owner's own playtest is **NOT_RUN** (see `docs/testing/ship-opening-playtest.md`; launch with `tools/play_ship_opening.ps1`).

## Gates at the code head `d129bba` (canonical runner `tools/test/run_selected.py`, workflow checkout `6b683ea`, source dirty 0)

| Lane | Host | Selected | Executed | Failed / empty / incomplete | Skipped | Verdict | Extract |
|---|---|---:|---:|---|---|---|---|
| app + input + alloc + 7 required contracts (`viewer_wiring_contract`, `window_tools_contract`, `viewer_wiring_opening_controls`, `opening_session_replay`, `viewer_wiring_cpu_delivery_failures`, `viewer_wiring_r1_held_publication`, `viewer_wiring_r4_occupancy`) | native MSVC Release | 248 | 246 | 0 / 0 / 0 | 2 (opt-in probes) | PASS | `gate-native-app-*` |
| sim + formats (`ship manifest`, reference agreements) | native MSVC Release | 1056 | 1051 | 0 / 0 / 0 | 5: 2 opt-in, 3 `other` = the symlink rows of `ship_start_load_test` (Windows, no symlink privilege); `--allow-skip other` | PASS | `gate-native-sim-*` |
| RTX GPU tests (`anno_render_gpu_tests`, includes the live-ship parity tests) | native, RTX 4090 Vulkan, one test at a time (`RESOURCE_LOCK gpu`) | 107 | 107 | 0 / 0 / 0 | 0 | PASS | `gate-native-rtx-*` |
| warnings, `tools/check_unix_warnings.sh`: GCC 13.3 and Clang 18.1 at `-O3 -Werror`, viewer + GPU on (295 files) and viewer only (64 files) | WSL Ubuntu | - | - | all four variants clean | - | PASS | `gate-wsl-warnings.txt` |
| app + input + alloc + the five wiring contracts selected on WSL (`viewer_wiring_contract`, `_r1_held_publication`, `_r4_occupancy`, `_cpu_delivery_failures`, `_opening_controls`) + `opening_session_replay` (`window_tools_contract` is native only; the runner also ran `picture_matrix_contract`), ASan + UBSan, viewer + GPU code on | WSL Ubuntu, GCC 13 Debug | 247 | 245 | 0 / 0 / 0, sanitizer lines 0 | 2 (opt-in probes) | PASS | `gate-wsl-app-verdict.txt` |
| sim (`ship start`, `unload`, `founding`, `sailed founding`, `build after founding`, `build on the real`, `cargo`), ASan + UBSan | WSL Ubuntu | 81 | 81 | 0 / 0 / 0, sanitizer lines 0 | 0 | PASS | `gate-wsl-sim-verdict.txt` |

Provenance note for the WSL lanes: the wrapper file (`gate-wsl-identities.txt`) records the workflow checkout with the Git overrides cleared
(`6b683ea`; its `dirty 535` is the WSL view of CRLF files). The runner's own JSON header inside the lane log shows workflow root `/mnt/e/anno1503`
with head `d129bba` because the lane runs with `GIT_DIR` pointed at the source worktree (the script says so); source dirty 0 holds in both.
The WSL build is the `wsl-build-config.txt` configuration (Debug, `ANNO_SANITIZE=ON` = AddressSanitizer + UBSan, viewer and GPU code on, GCC 13).

The three symlink-row tests skipped natively (`a base that is not a content-relative path is refused…`, `a content-relative
base loads, also through links that stay inside`, `the content path rules, as a function`) were **executed and passed** in the WSL sim lane.

Direct runs (no runner provenance; head and executable hash recorded in `exe-identity-gated.txt`: head `d129bba`,
`anno1503.exe` sha256 `694a5c23…`):

| Run | Result | Extract |
|---|---|---|
| `tools/test/opening_session.py --gpu` on the RTX 4090 (1280×720 drawable pixels): select, sail 550 tiles, save; resume, two refused foundings, founding, 5 unloads incl. the full-store refusal, 2 builds; **stage 3**: resume of the built city; replays in fresh processes (stage 1: checkpoints and 1 outcome agree; stage 2: checkpoints and 11 outcomes agree); one ton fewer / another goods diverge; **six refused recordings** (other manifest, seat, rules, start state, no `session.txt`, not a ship-start session) | `opening_session: PASS`, 21 checks | `rtx-session.txt` |
| `viewer_wiring.py --gpu` `opening_gpu_window` (turn, move, zoom, pan, resize, held presents) and `opening_controls` | both PASS | `rtx-wiring-scripts.txt` |
| the same `opening_session.py` on the CPU renderer (SDL dummy driver), run at the code of `d129bba` before its CMake timeout line | PASS | `cpu-session-checks.txt` |
| `tools/test/opening_mutation_check.py` (5 mutants of `opening_ui.cpp`, run alone, tree clean before and after) | 5 of 5 killed, each at the assertion that names the defect | `mutation-check.txt` |
| window caption read from the real RTX window at launch (nothing selected, tool off); the caption with a selected ship, a city or the building tool on was **not** read | 191 characters, starting `mode select \| gold 30000 \| no ship selected — anno1503 — …` | `caption-check.txt` (transcribed from the run's output) |

The mutation check rebuilds `anno1503.exe`; after restoring the source (`git status` empty before and after, checked by hand, not logged) the
executable was rebuilt (sha256 `26fc0ae4…`, a relink of the same sources; this is the executable the launcher runs on this PC) and
`viewer_wiring_contract` and `viewer_wiring_opening_controls` were rerun on it: PASS (`post-mutation-wiring-rerun.txt`). The mutants cover the
controls logic (`opening_ui.cpp`), not the window adapter (`viewer.cpp`).

## What was NOT run, and limits that are not claimed away

- NOT_RUN: the owner's playtest; native 4K; a Radeon (iGPU) window; physical display latency; five-seat load; original-runtime fidelity of the
  ship facings (the facing frame is an assumption of `docs/render/iso.md`); Linux/macOS targets other than the WSL gates above.
- No test asserts where a ship sprite is drawn, which way it faces or its colour (screenshots were inspected by eye); the GPU test proves the
  overlay equals the scene's own drawing and fails on a wrong input. The pointer picks ships with the same projection the sprite is drawn with.
- The caption order (command state, advisory, pointer first) is by construction and was read once in the real window (idle); no test asserts it.
  **With the building tool on, the tool's line comes after the opening's line and the window name, about 300 characters in: the card tells the
  owner to maximise the window for the build step. Moving the tool line forward is a small code change not made here (it would rerun every gate).**
- In unload mode with a ship selected `-`/`+` set the quantity instead of zooming (the wheel still zooms). The `src/app/CMakeLists.txt` comment says
  "thirteen replays" where the session has 10 replays and 3 windows (13 processes). Stage 3 proves stores, hold, ship and the building tool, not a count of buildings.
- Save/resume: not tested are a registry-digest mismatch, a tick mismatch and a differing island list (checked in `load_opening`); the island
  comparison covers rectangles and index, not terrain.
- Replay does not check the recorded speed; a different speed shows as a hash divergence.
- Known accepted limits: the selection ring in enhanced water, derived-zoom GPU test gap, screenshot freshness, `window_matrix.py`,
  `dev_session_capture.py` and `window_state_case.py` still default to the old lan5 camera `1052,444`, per-turn cost at scale, the owner colour is a stand-in.
- The `break` added to `route_plan.cpp` `finish_trace` for an unreachable null tile would end a walk silently if it were ever reached.

## Failed, invalid and superseded runs (kept in `cache/gates/`, not in git)

| Run | Why it does not count |
|---|---|
| `native-n1-FAILED-*`, `native-n2-FAILED-*` | gate preflight refused test names containing a semicolon; log tag reused (`File exists`) |
| `native-n1b`, `n1c` (head `dd02efc`) | 20 existing app tests failed under the merged terrain rule (fixed by `f66f920`) |
| `native-n2c` | the runner failed at `git diff --binary HEAD` of the workflow checkout (cause not established; the lane had been stopped by hand); not rerun at that head |
| `native-n4a/n4c` (head `f66f920`) | superseded: the warnings fixes changed sim, formats and app sources |
| `native-n5a-INVALID-stale-binaries-killed` | started before `build/msvc` was rebuilt; killed |
| `native-n6a/n6c/n6g` (heads `ae7dfcc`, `aa69b6a`) | passed, superseded by the lanes above after later source changes |
| `FAILED-sdl-config-wsl-w1*`, `FAILED-gcc-werror-wsl-w2*` | WSL SDL console config; 60 GCC `-Werror` findings |
| `wsl-w3`, `wsl-w4` | `.run.lock` left by a stopped run; warnings failed (`ANNO_REPO_DIR` undefined in the script; null-dereference findings, several already at the Mac pin) |
| `wsl-w9` | `viewer_wiring_opening_controls` checked state before the first view was published (ASan startup); sim lane invalid ("tested source changed during selection") |
| `wsl-w10` | `opening_session_replay` hit the 300 s window watchdog under ASan; its sim lane passed 81/81 but the next lane replaced it |
| `wsl-w11-SUPERSEDED-killed` | stopped when the review fixes changed the tree |
| `mutation-final-SKIPPED-no-game-dir.log` | exit 77: `ANNO1503_GAME_DIR` was not set |
| `gpuwin-d129-FAILED-dir-exists.log` | output folder reused; rerun in a fresh folder |

## Reviews

- Package reviews of I1–I4/I5 (three independent read-only reviewers): findings folded into `a13f4af`, `14ed2b5`, `dd02efc`.
- **Final integration review** (fresh read-only reviewer, head `aae874f`), 14 findings, dispositions:

| # | Finding | Disposition |
|---|---|---|
| 1 | ASan app lane without a verdict at any head | resolved: `wsl-w12` app 247/245 and sim 81/81 PASS at `d129bba`; the opening session got a sanitizer time budget in CMake |
| 2 | caption: the important part came last | fixed (`status_line` order, opening line first in the caption); read once in the real window; not asserted by a test |
| 3 | replay refusal of a mismatched recording had no test | fixed: six refusal checks in `opening_session.py` and a unit test of the identity round trip. The first run of the new checks failed three of the four field checks (and the manifest check passed for the wrong reason) because Python wrote CRLF into `session.txt` and the parser kept the CR; the parser now tolerates CRLF |
| 4 | evidence-column errors in `ship-opening.md` | fixed |
| 5 | nothing asserts ship placement / facing / colour | accepted and stated (above and in `ship-opening.md` §6) |
| 6 | provenance split over three heads | all gates rerun at `d129bba`; executable hashes recorded |
| 7 | symlink skips must be named | named; executed and passed in WSL |
| 8 | mutation check: weak spots, run alone | rerun alone at the final head; the controls-only coverage is stated |
| 9 | resume claims vs tests | stage 3 added (resume of the built city); remaining gaps stated |
| 10 | card vs code | `Tab` in founding mode and `-`/`+`/`Page Up`/`Page Down` for the quantity (the `[` `]` keys need AltGr on a German keyboard) added to the card and the code; the `B` message without a city reworded |
| 11 | launch script | saves default to `saves\` of the checkout (the environment variable is restored when the game exits); comment on the worktree game-folder search; adapter note in the card |
| 12 | racy first expectation of `opening_gpu_window`; refusal reasons unpinned | fixed (`wait fact … within 60000`; `NoRotationFits`, `OutOfRange` pinned) |
| 13 | hard-coded copies of sim rules (`founding_reach_sq`, four rotations, visibility policy) | the reach cites its spec row; the visibility limit is in the card |
| 14 | sim guards | behaviour-preserving on valid paths; the `break` caveat is stated above |

- **Focused re-review of the fixes** (fresh read-only reviewer, head `521bbbc`): 1 blocker, 11 notes; areas 1 (caption reorder), 2 (key collisions),
  3 (replay refusals, CR parser, stage 3), 4 (scope) and 5 (numbers and hashes against the files) clean. Dispositions:

| # | Finding | Disposition |
|---|---|---|
| 1 | BLOCKER (usability): with the building tool on, the tool's own line comes after ~300 characters of caption | not a regression (it was last before as well). Card step 5 now says to maximise the window and names this as a limit; moving the tool line forward is a code change that would rerun every gate and is left for the owner's first playtest to confirm. **Open for the orchestrator to accept or schedule** |
| 2 | replay numbers in `ship-opening.md` match no log | fixed: ranges, run-dependent |
| 3 | WSL provenance header (`6b683ea`, dirty 535 vs the runner JSON) unexplained | explained above |
| 4 | claims with no retained file (caption, density, post-mutation rerun, tree clean, sanitizer flags) | the caption transcript, the post-mutation rerun and the WSL build configuration are now extracts; "density 1.00" removed; "tree clean" says it was checked by hand |
| 5 | `+`/`-` zoom keys are quantity keys in unload mode | stated in the card; the wheel zooms; other modes and the building tool keep the zoom keys |
| 6 | held-key repeat of the quantity keys is untested | accepted (the script `key` verb cannot repeat) |
| 7 | stage 3 does not count buildings | stated |
| 8 | CMake comment says "thirteen replays" | stated (a comment; not changed after the gates) |
| 9 | CRLF explanation overstated | reworded |
| 10 | the launcher's environment variable outlives the run | fixed: restored in `finally` |
| 11 | WSL wiring contracts listed too broadly | corrected |
| 12 | status wording / missing section | this section |

## Findings outside this package that the Mac lane should know

Checked at the Mac pin `0c401c8` (a temporary worktree) and independent of this work, `tools/check_unix_warnings.sh` (GCC 13 `-O3 -Werror`) failed on
`src/sim/src/route_plan.cpp` (two `-Wnull-dereference`), `src/sim/src/figure.cpp` (`remove_figure`), `src/sim/tests/founding_test.cpp` and
`unload_test.cpp`. Not checked at the pin but fixed in the merged tree: `route_objects_test.cpp` and `founding_assets_test.cpp` (null-dereference /
maybe-uninitialized false positives), and Clang 18 (libstdc++) rejected `json::Value` (a `std::pair<std::string, Value>` member vector with an
incomplete type). They are fixed here with guards that change no behaviour on a valid path (`ae7dfcc`); the warnings script also gained the
`-DANNO_REPO_DIR` define the merged tests read.

The extract list with the original hashes is in `extract-table.md`.

The owner's playtest rounds of 2026-11-03 after the candidate (rounds 1-7), the branch state and the round 7 findings for the
next executor: `owner-playtest-2026-11-03-handover.md`.
