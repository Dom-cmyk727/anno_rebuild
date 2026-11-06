# W4-SETTLEMENT-1: W7 review, 2026-11-07

**CHANGES REQUESTED. The package is not accepted.** Reviewed head `384df19d` on `task/W4-settlement-1`, against base
`3b74201f`. The Claude Code orchestrator reviewed the intake and the evidence. An independent read-only review agent
reviewed the branch's code. The orchestrator re-checked that agent's four most severe findings at the head.

| Part | Verdict |
|---|---|
| MAIN-CI-1 intake `b7be2a1d` | **Accepted.** Matches main's repair line for line. |
| W7 WSL gate run `w7final2` | **Not accepted.** The raw logs no longer exist, three lanes fail, three sim tests never ran. |
| Branch code (`59201fb1` and the inherited W4-OPENING-CONTROLS-FLAKE delta) | **Changes requested.** One blocker, several should-fix findings. |
| The 18 MSVC failures | **Not caused by this package.** They come from `main` and reproduce there (below). |

## The intake

**Twelve files carry exactly main's changes.** Comparing the changed lines of `d1a2e191..92e4817b` with
`2f2b45ac..b7be2a1d` file by file:
- `script_runtime.cpp`, `script_canary2.py` and `budget_f3_test.cpp` are identical;
- so are the nine sim test files.

**The one difference is in `src/sim/CMakeLists.txt`.** The intake keeps the SCRIPT2 skip property and leaves out the
economy-loop hunk. Those tests don't exist on a state132 base, so leaving the hunk out is correct.

## The 18 MSVC failures belong to main

The 18 tests fail at `main` `96775da4` with the same messages. That run used a fresh MSVC Release build of
`anno_app_tests` and `anno_render_gpu_tests`, with the original game files. In the same filters, two other tests
passed in each binary. Hosted CI has no game files, so it skips all 18 tests. Two commits from 2026-11-05 cause them.

**1. The strict scene loader.** `f7c31ed8` refuses original actors it cannot yet represent. main's own test expects
`Single_01.sze` to be refused with `players[7].SCHIFF[0].PRIVAT.CREW.ENTRY[0].PRIVAT.ESEL.TASK: unsupported unit
record/state` (`play_load_test.cpp`). Several older tests still load that scene or walk all scenes:
- `play: a turn with no commands publishes no building changes`;
- `play: GPU matches the CPU path on one scene`, and the same test on the Radeon iGPU;
- `founding sites, real files: the original's own warehouses pass the coast rule as stored`. It finds 4 warehouses,
  not the 62 measured on 2026-11-03.

**How many original scenes still load.** Of the 50 `.sze` files in `Scenes`, 11 load in play mode: seven endless
presets, `endless01`, `endless03` and two tutorials. The other 39 are refused for one of these reasons:
- an unsupported unit record or state;
- an `ESEL` placement without a holder;
- unsupported script actions such as `QUESTLOG_SHOW`;
- unsupported script object definitions.

The 39 are 13 campaign, 10 single, 12 add-on single and 4 others. This is the intended typed refusal, but the owner
cannot open those scenes in the window on main.

**2. The construction-eligibility check.** `d2be23aa` (SCRIPT1) refuses a building whose stage the player has not
reached, returning `ConstructionDenied` (48). The fixed LAN map starts at the first stage, as the original does.
- The developer-resources test picks the dearest building of all, and that building is stage-locked. It expects
  `CannotAfford` (16) and gets 48.
- The legal workload chooses later-stage buildings, so its builds are refused. This affects the two `legal
  workload` cases.
- The same refusal affects the 11 `picture GPU` cases that build through that workload. Each fails on
  `REQUIRE(e.accepted)` or `build.accepted`.

**These tests are out of date; the product is not broken.** The fix is in the tests:
- the developer-resources test and the legal workload pick buildings the start stage allows;
- the scene tests use a scene that still loads, such as an endless preset;
- the founding-sites test counts warehouses through a path that does not need the actors, or over the scenes that
  load, with a new measured count.

This is main's repair. It does not belong in this package.

## The render-lane failure

`a flow build that fails leaves the legacy look, final, built once (game files)` failed with `rebuilt == 0` as
`1 == 0`. No file under `src/render` differs between `3b74201f`, this branch and `main`.

**Likely cause, from reading the code (not confirmed by a rerun).** `FrameRenderer::update_chunks` counts every chunk
it receives. That includes the margin ring and the neighbouring zoom groups, not only the visible chunks.
- The test's "settled" condition checks only the visible chunks and `busy()`.
- `busy()` is false when a job finishes between `take_done` and `submit`. `frame.cpp` describes that case (review
  2026-11-03, P2).
- So one finished margin chunk can arrive during the 60-frame "nothing rebuilds" loop, and the ASan slowdown makes
  that more likely.

The test would then need a better settle condition, such as two quiet frames in a row, or should count only rebuilds.
A repeated run of this one case, for example 20 times, settles it.

## The W7 run's evidence

- **The raw logs are gone.** `tools/test/wsl_gate.sh` writes its logs to `$W/cache/gates`, inside the worktree under
  test. That folder is ignored by git, so `git worktree remove` deleted it with the diagnostic worktree. No
  `wsl-w7final2*` file exists any more. The lane table in REPORTS can't be checked against raw output. The evidence
  contract requires the reviewer to inspect raw evidence.
- **Three sim tests did not run.** The memory-budget coordinator refused them with `wait deadline`. A refusal is not
  a pass, and the report does not name the three tests.
- **The script's exit status does not reflect the lanes.** It ends with `cat "$S"`, so a run with three failing lanes
  exits 0.

## Findings on the branch

Fix the first eight before the next W7 run. Item 9 is follow-up. Line numbers are at `384df19d`.

1. **Blocker: clang-format 23.1.3 fails on the new code.** There are 12 new violations:
   - seven in `src/app/src/play.cpp`, in the payload formatting near lines 737–751;
   - three in `src/app/include/anno/app/play.hpp`, lines 123–125;
   - two in `src/app/src/viewer.cpp`, lines 1917–1918.

   The cpp-format CI job fails on them.
2. **`wait turns` still races; the fix does not do what it says** (`89fe7c25`, inherited from
   W4-OPENING-CONTROLS-FLAKE). At `viewer.cpp:1839`, `st.facts_turn` is set from `st.snapshot`, the live feedback
   read. It should come from `opening_snapshot_`, which the comment above it and `dev_script.hpp` describe.
   - The three green reruns don't prove the fix.
   - Fix it, and add a test in which the facts' turn and the live turn differ.
3. **The inspection line can show the wrong building.** `viewer.cpp:1765` shows `snapshot->inspection` without
   checking that its tile is the hovered tile. While paused, no turn is published and `note_behind` (`play.cpp:1067`)
   carries the old inspection forward. Hovering another building or empty ground then keeps the old line, against
   step 5 of the playtest card.
4. **The Proof B test never runs in the WSL gate.** The app lane's regex (`wsl_gate.sh:200`) leaves out
   `viewer_wiring_settlement_activity`.
5. **The gate script's exit status.** It should fail when any lane fails or a required lane is skipped. `--gpu-lane`
   without lavapipe currently ends as `rc=skip` with exit 0.
6. **The gate keeps its evidence inside the worktree it tests.** It should write to a folder outside `$W` and refuse
   to overwrite an existing summary. A reused or colliding tag currently overwrites it, and the default tag is
   `%H%M%S`. `docs/testing/README.md` should say where the logs live.
7. **The build folder has no lock, but the cleanup deletes it on every exit.** The trap is registered before the
   free-space, dirty-tree and `.git` checks. A second invocation that refuses at once still deletes `$B` while another
   gate is running, and the stale-cache wipe does the same. Take a `flock` on the build folder before registering the
   trap.
8. **The `rm -rf` target is unchecked.** `--build-dir` accepts `$HOME`, the repository or a relative path, which is
   then resolved again after `cd "$W"`. Resolve the path with `realpath` and require it under `~/anno-build`.
9. **The tests prove little about the feature.**
   - `settlement_activity.script` checks only constructor constants (`residents=5`, type 501). It has no negative
     step, and `contains residents=5` also matches 50.
   - No `anno_app_tests` case covers `inspect_building` or its formatters for workshop, farm, small farm, mine and inn.
   - The script skips without game files, so CI never runs it.

**Nits:**
- the House comment calls the need stock "satisfaction";
- the Market line hides `carts_out`;
- the inspection tile is not cleared when the dev tool is switched off;
- `dev_script.hpp:172` is now out of date, and the `inspection.*` facts are not documented;
- the gate does not record its own revision;
- `-j 12` and the game folder are hard-coded;
- the `--keep-build` text is wrong;
- the `heavy` exclusion of the GPU lane is not documented.

## Checked and sound

- **The inspection is a pure consumer.** It uses only const accessors, with no mutable caches and no random-number
  generator. It runs on the sim thread after the turn and reaches the window as `shared_ptr<const>` inside the
  snapshot, under its mutex. It has no effect on the state hash or replay.
- **The displayed fields are correct.** They match their 8.8 and plain-count definitions. The hand-derived checks
  are `tons(384) = 1.50t`, `tons(300) = 1.17t` and 320/64 = 5. A mutation that shows the raw 1/64 value fails the
  script, so that test is weak but not empty.
- **`founding_assets_test` and `ctest_verdict_test.py`** are correct, and their tests mean what they say.
- **Parts of `wsl_gate.sh` are correct:**
  - the autocrlf override comes before the dirty check;
  - Windows gitdir paths are translated correctly;
  - the stale-cache check also wipes an empty or corrupt cache entry.

## What happens next

**Windows executor, on `task/W4-settlement-1`:**
1. Fix items 1–8, each as its own commit.
2. Rerun all five W7 lanes once. The gate keeps its logs outside the gate worktree.
3. Repeat the render case above 20 times in that build.
4. Name every test that did not run.

The 18 MSVC failures stay listed by name as known failures from main.

**Owner:** the playtest card after item 3 is fixed.

**main:** the 18 out-of-date real-file tests need a separate repair (proposed as MAIN-REALFILES-1).
