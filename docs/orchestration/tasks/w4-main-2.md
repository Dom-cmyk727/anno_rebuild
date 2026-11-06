# W4-MAIN-2: the Windows lane onto main, main's real-file tests, Windows host gates, live Load

**READY / RELEASED 2026-11-07. Owner: the Windows executor. The Claude Code orchestrator wrote this packet.**

It starts as soon as the W4-SETTLEMENT-1 fixes from the
[2026-11-07 review](../reviews/2026-11-07-w4-settlement-1-w7.md) are committed, with no stop in between. Work through
the parts in order: **S → A → B → C → D → E**. The orchestrator reviews everything in depth the next morning. Until
then, every part ends with an ordinary push of its branch and a REPORTS entry marked **REVIEW PENDING**.

Re-read this packet and your latest REPORTS entries after any context compaction, before you continue.

## 0. Rules for this run

### 0.1 No stops

**Don't wait for a review or a reply.** If a step needs a decision that belongs to the owner or an orchestrator:
- write it as a **QUESTION** in that part's REPORTS entry: what is needed, the options, and your recommendation with
  its evidence;
- skip only the work that depends on it;
- continue with the next independent step.

Stop only at E, or when nothing independent is left.

Push the branch at every checkpoint. Add a short progress line to REPORTS each time: part, step, head, time.

### 0.2 Check every shell's output

Shells have run for hours without producing output.
- **The 2026-11-06 hang:** a job backgrounded inside a `wsl.exe` call died, and a wait loop with no cap waited
  forever (`f1dc0baf`).
- **The 2026-11-06 gate run:** three sim tests were refused after waiting 1,800 s because other jobs held the memory.

Therefore:
- **Long commands.** Run anything that may take more than about two minutes:
  - through the tool's own background mode, never with `&`, `nohup` or `setsid` inside a `wsl.exe` call;
  - with stdout and stderr in a log file under the evidence folder (0.5);
  - with a time limit: the tool's timeout, or `timeout` in WSL.
- **Read the result before the next step.** Read the exit code and the last lines of the log. Empty output is not
  success, and a missing completion line (`GATE_END`, CTest's summary) is a failure to explain.
- **Check every long job at least every 15 minutes:**
  - the log's size and the time of its last line;
  - that the process is alive and its CPU time grows. In WSL use `ps -eo pid,etime,time,pcpu,cmd`; on Windows,
    `Get-Process` with `CPU`.

  A single corpus test can run 60–75 minutes without writing a line: population A and both scenario-corpus cases took
  3,800–4,300 s each in `w7final2`. There, the growing CPU time is the sign of progress.
- **What counts as stuck.** Treat a job as stuck after 20 minutes with neither log growth nor CPU growth, or after
  twice its known duration. Then:
  1. record the process tree, the last 50 log lines, free memory and the memory coordinator's last lines;
  2. stop only your own processes;
  3. write it in REPORTS and rerun that step once.

  If it sticks a second time, record a QUESTION and go on.
- **Known durations** (from `w7final2`, while nothing else ran):

  | Step | Time |
  |---|---|
  | WSL ASan build | 211 s |
  | app lane | 7,966 s |
  | sim lane | 6,403 s |
  | formats lane | 1,110 s |
  | render lane | 2,095 s |
  | tsupport lane | 249 s |

  Every wait loop has a cap.
- **Never report a job as "still running" without a fresh check** of the time, the log lines and the CPU time.
- **One heavy job at a time.** Don't run two full gates at once. While WSL lanes run, do only light work: reading,
  editing, single-target MSVC builds, focused tests. Other sessions use this PC too (Codex worktrees under
  `C:\Users\altin\.codex`): never stop their processes.

### 0.3 Evidence, not guesses

- **When in doubt, check the original game files**, read-only in `E:\anno1503\game\app`:
  - `Anlagen.dat` and `BGruppen.dat`;
  - `Texte.dat` (`python -I tools/text.py`);
  - `help/*.htm`;
  - the scenes (`python -I tools/sze.py dump`).

  Further sources:
  - **Disassembly**, as in `docs/re/`. WSL `objdump` works; `tools/disasm.py` needs `pefile`, which Windows Python
    lacks. Record module and address.
  - **The Wine runner**, which is pre-authorized. Keep music and sounds off, and skip the two intros one at a time.

  The order of trust is: files and disassembly, then the owner's first-hand statements, then community sources.
- **Every number, rule or expected value in code or tests needs a source:** a parsed file, an address, a measurement
  you made (with its command) or a cited document. Never change an expected value only because the output changed;
  explain the difference first.
- **If you do guess, record it** in a **Guesses** table in that part's REPORTS entry:
  - where (file:line or step);
  - what you assumed;
  - why there was no evidence (what you checked);
  - how you chose (your reasoning);
  - what would confirm or refute it.

  A guess that reaches code also gets an entry in `docs/open-questions.md` and a code comment pointing to it. An
  unrecorded guess is a defect.
- Never copy original text, images or listings into git.

### 0.4 Boundaries

- **Pushing.** Push only your own task branches: ordinary pushes, no force, no rebase of a pushed branch.
- **Main.** Never push to or merge into `main`; the orchestrator merges after review.
- **Merging.** Exactly one merge is authorized, in B1. Merge no other lane's branch and no moving `main`.
- **Board files.** Don't edit `TASKS.md` or `STATUS.md`; your reports go to `REPORTS.md` on your branch.
- **No product changes:**
  - no feature-default changes;
  - no state, identity, replay or command version changes;
  - no change to the strict scene loader or to the construction-eligibility rule.
- **Files.** Don't touch `game/`, and don't delete evidence.
- **Worktrees.** No game-files junction in a worktree, because it makes the WSL gate's tree dirty. Set
  `ANNO1503_GAME_DIR=E:\anno1503\game\app`.

### 0.5 Evidence folder

Keep every log in `E:\anno1503-evidence\w4-main-2\` (`/mnt/e/anno1503-evidence/w4-main-2/` in WSL). It lives outside
every worktree and is not in git.
- After each MSVC CTest run, copy `build/msvc/Testing/ctest-output.log` and `build/msvc/Testing/Temporary/LastTest.log`
  there under new names; the next run overwrites them.
- The WSL gate writes there after review item 4's fix.

## S. Finish the W4-SETTLEMENT-1 fixes

1. **Commit review items 1–8 on `task/W4-settlement-1`**, one commit each, as listed in the review.
2. **Focused checks at the S head, with the game files:**
   - the MSVC build;
   - `anno_app_tests`, `anno_app_input_tests` and every `viewer_wiring_*` test, including
     `viewer_wiring_settlement_activity`, in the live window on the RTX 4090;
   - `opening_session_replay`;
   - clang-format 23.1.3 on the changed files.
3. **Push**, and report "S done, REVIEW PENDING".
4. **Don't start the old-base W7 rerun** from the earlier follow-up. That branch is never merged as it is; the single
   W7 in Part C, at the adopted head, replaces the rerun. If one is already running, let it finish, keep and report its
   logs, and start no second one on the old base.

## A. MAIN-REALFILES-1: main's out-of-date real-file tests

**Setup.** Create a new worktree `E:\anno1503-worktrees\anno1503-MAIN-REALFILES-1` on a new branch
`task/MAIN-REALFILES-1` from `main` `dbb22306126c447104457a1bab20e447970f1a58`. Configure its own `build/msvc` there.

### A0. Baseline at plain main

Build everything, then run `ctest --preset msvc -j 16` with the game files. List every failure with its exact test
name and assertion.

Expect at least the 18 that the review reproduced at `96775da4`, which `dbb22306` does not change. main has 91
commits the Windows lane never ran, so there may be more.

### A1. Classify every failure

For each failure, record:
- the cause commit (`git log -S`, or a bisect when needed);
- whether the test is out of date or the product is wrong;
- the evidence.

| Tests | Cause (commit, 2026-11-05) |
|---|---|
| `play: a turn with no commands publishes no building changes`; `play: GPU matches the CPU path on one scene` (and its Radeon iGPU twin) | The strict loader (`f7c31ed8`) refuses `Single_01.sze`. |
| `founding sites, real files: the original's own warehouses pass the coast rule as stored` | Same loader: only 11 of the 50 original scenes load in play mode, so it sees 4 warehouses, not 62. |
| `developer resources: a Build too expensive for the start is refused without them` | Eligibility (`d2be23aa`): the dearest building is stage-locked, giving `ConstructionDenied` (48) before `CannotAfford` (16). |
| The two `legal workload ... (game files)` cases and the 11 `picture GPU` cases that build through the workload | Eligibility: the workload picks buildings of later stages than the start allows. |

**Product defects.** Fix an obvious bug, such as a crash or a wrong index. Anything that changes how the game plays is
a QUESTION.

### A2. Fixes, one commit per cause

**(a) The play-mode scene tests:** `building_delta_test.cpp:85` and `play_gpu_test.cpp:87` (both cases). Replace
`Single_01.sze` with a scene that loads through `load_play`. Choose it by measurement, not by name:
- **The 11 that load:** the seven `0010SingleEndlos` presets other than `02_1024_easy_128` and `08_800_heavy_128`,
  plus `0011SingleEndlosAddon/endless01` and `endless03`, and `Tutorial/01Discover01` and `02Trade01`.
- **Measure each:** count its buildings with `play_buildings`, and check what is inside the view or island each test
  compares.
- **Choose** the scene that keeps every assertion meaningful: buildings in the compared view, and non-zero counts
  where the test counts something. Put the measurement table in REPORTS.
- **Leave two tests unchanged.** `play_load_test`'s refusal case and `sim_replay`'s refusal test pin the strict loader
  on purpose.
- **Don't touch the `load_scene` users.** Tests that use `load_scene`, the view path, still pass.

**(b) Founding sites** (`opening_test.cpp:970`). The test's claim is about the original files' warehouses, not about
play loading. Read all 50 scenes through the explicit, test-only actor-free path that the sim tests use
(`src/sim/tests/original_component.hpp`):
- `OriginalActorProjection::ActorFreeComponent`;
- an unsupported whole script set aside only after its refusal has been shown.

Then re-measure. On 2026-11-03 the count was 62 warehouses, each accepted at its stored rotation. If you get another
number, find the reason in the files before writing any number down. The helper stays test-only; if it moves, it
keeps its "never in production loading" comment.

**(c) Developer resources** (`commands_file_test.cpp:247`):
- Pick the dearest building that `World::construction_eligibility(developer player, type)` allows, and expect
  `CannotAfford`.
- Add a check that a stage-locked building is refused with `ConstructionDenied` before any cost check, so the new rule
  stays covered.

**(d) The legal workload** (`src/app/src/legal_workload.cpp`, the candidate pools near line 213):
- Add the sim's eligibility as one more read-only prefilter, like the footprint-owner and placement prefilters already
  there.
- Count the excluded types per kind in the workload's summary.
- Refuse to build the workload if a pool ends up empty.
- Rerun the 11 `picture GPU` cases without changing their assertions.

**Don't add a developer "unlock all stages" switch.** Whether the owner should be able to place later-stage buildings
with the developer tool is the owner's decision: write it as a QUESTION.

### A3. Verify

1. **The fixed tests pass:** the 18, plus anything else A0 found and you fixed. The GPU cases run on the 4090, and on
   the Radeon iGPU where the test asks for it.
2. **Negative control.** Disable the eligibility prefilter in your working tree, without committing: the legal
   workload must then fail with `ConstructionDenied`. Record the result.
3. **The full MSVC CTest** with the game files: name and explain every remaining failure.
4. **clang-format 23.1.3** over the changed files.
5. **The GCC 13 / Clang 18 `-Werror` check:** `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`.

### A4. Push

Push `task/MAIN-REALFILES-1` and write the REPORTS entry, REVIEW PENDING. The orchestrator merges it into main after
review; you don't.

## B. W4-MAIN-2: the Windows lane on main

**Setup.** Create a new worktree `E:\anno1503-worktrees\anno1503-W4-main-2` on a new branch `task/W4-main-2` from
`dbb22306`.

### B1. Two merges, by hash

1. Merge the A4 head of `task/MAIN-REALFILES-1`. A fast-forward is expected.
2. Merge the S head of `task/W4-settlement-1`. **This packet authorizes exactly this one merge.**

   Expect conflicts only in `docs/orchestration/REPORTS.md`: keep both sides' entries, in date order. Other files:
   - the MAIN-CI-1 intake hunks are identical on both sides and merge cleanly;
   - `founding_assets_test.cpp` has a one-line message change on the W4 side.

   Resolve any other conflict with evidence and list it.

### B2. Check the merged tree

- **What may differ.** `git diff dbb22306 HEAD --stat` may show only the W4 app, tools and docs delta, plus Part A.
- **What must not change:** nothing under `fixtures/`, no state or identity pin.
- **Identity.** It stays state134 / ECONBK1 / PRNG5 / replay4 / command1, and the integration identity tests pass.

### B3. MSVC gate

1. Run the MSVC build and the full CTest with the game files. Compare with A3: there must be no new failure.
2. In the live window on the 4090, run every `viewer_wiring_*` test, including `settlement_activity`, and
   `opening_session_replay`.

### B4. Push

Push `task/W4-main-2` and write the REPORTS entry, REVIEW PENDING.

## C. W4-HOST-134: one W7 and the Windows host gates at the B head

### C1. The WSL ASan/UBSan gate

Run all five lanes once, with a new tag and the logs in the evidence folder. Add `--gpu-lane` if lavapipe is
installed, and say so if it isn't. Follow the job checks in 0.2 for the whole run.

### C2. The render case, 20 times

Run `a flow build that fails leaves the legacy look, final, built once (game files)` 20 times in that same build, and
report the passes out of 20.

If it fails at all, find the cause by reading the code. The review's hypothesis: a chunk of the margin ring or of a
neighbouring zoom group finishes between `take_done` and `submit`, so `busy()` is false while a result waits.

Then fix the test's settle condition in its own commit, with a deterministic reproduction if you can. The evidence
contract allows gates, latches or observed states here, never sleeps.

### C3. Warnings and format

Run `tools/check_unix_warnings.sh` (GCC 13 and Clang 18, `-Werror`) and clang-format 23.1.3 over `src/`.

### C4. Report every lane

For every lane, report:
- selected tests;
- executed passes;
- failures;
- skips by class;
- tests not run, each named with its reason;
- sanitizer reports.

For every failure, give its name and assertion, and whether it also fails at plain `dbb22306`. To find out, run only
that test there.

**What C counts as.** C is the W7 for the adopted W4-SETTLEMENT-1 and the Windows host evidence for state134 main:
REVIEW PENDING.

## D. W4-EXPAND-LIVE-1: the Load adapter and the live two-island recipe

**Setup.** Create a new branch `task/W4-expand-live-1` from the B head. Under the concurrency rule in 0.2, light D work
may start while C's WSL lanes run; D's own gates run after C. The input is the consumer card
[`docs/testing/manual-expansion.md`](../../testing/manual-expansion.md).

### D1. Wire Load like Unload

Trace how Unload is wired end to end:
- `OpeningMode::Unload` and `OpeningKey::Unload` in `opening_ui.cpp`;
- its command-file or session token (the card names `commands_file.cpp`; find where it actually lives);
- the session save and resume roundtrip.

Add Load the same way, posting the sim's existing `LoadCmd`. It is a developer adapter, not the original's warehouse
trade panel (UI.1 F2.2, OQ-547): label it so in the UI text and the docs.

### D2. The card's canary settings

The card's settings are `city_booking` on, `save_billed_goods` kept and `house_updates` off. Allow them only through
an explicit developer or session option; the application defaults stay as they are.

If the window can't set them without changing a default, record a QUESTION and keep D headless.

### D3. Tests

- **The token:** it parses and roundtrips.
- **Save and resume:** a session saved with a pending Load resumes correctly.
- **The recipe:** the card's recipe, replayed through the app's command path with the card's exact inputs and hashes,
  reaches the card's results:
  - each actual arrival;
  - each transfer, as `transferred_88`;
  - `CannotAfford` before the wood is unloaded;
  - final money 28,863;
  - final wood of 2,368 raw at the source and 512 raw at the destination.

  Don't rebalance, change the map or grant rights.

### D4. The real window

Write a `viewer_wiring` script that drives Load in the real window on the 4090. Then write a playtest card for the
owner: the steps, and what they should see at each one.

### D5. Gates

Run the MSVC app tests and the `viewer_wiring` tests with the game files. Then run the WSL app lane under ASan; only
`src/app` changed, so say that this is the dependency justification.

Push, and write the REPORTS entry, REVIEW PENDING.

## E. Final report and cleanup

**The report.** At the top of the last REPORTS entry, write a summary:
- every branch with its head;
- what ran and with what result;
- every failure that remains;
- the combined Guesses table;
- every QUESTION;
- a "review first" list of three to five items for the orchestrator.

**Cleanup.**
- Leave no gate or test process of yours running, and check that none is.
- Leave the WSL build trees as the WSL disk rules say.
- List your worktrees in the report. Don't delete one that holds evidence not yet in the evidence folder.

**Push** every branch, then stop.
