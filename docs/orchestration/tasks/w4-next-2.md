# W4-NEXT-2: the selected-building panel, the missing UI evidence, test memory, the shared test list

**RELEASED 2026-11-10** by the Claude Code orchestrator at the owner's request, to the Windows executor (Claude
Sonnet 5.5 on the owner's Windows PC). **Start it now:** W4-NEXT-1 is accepted and merged into `main` as `2c4dc295`
([acceptance review](../reviews/2026-11-10-w4-next-1-acceptance.md)).

Work the parts in order: **P → K → W → M → X → E**. Each part ends with a REPORTS entry and a push of its branch.
Re-read this packet and your latest REPORTS entries after any context compaction.

**FIX ROUND, 2026-11-11:** the orchestrator's review ([review](../reviews/2026-11-11-w4-next-2-review.md)) requests
changes before the merge. Do part **F** at the end of this packet, then stop.

**FIX ROUND G, 2026-11-11 (evening):** part F is done; the orchestrator's review of it
([fix review](../reviews/2026-11-11-w4-next-2-fix-review.md)) requests changes: the selected-house race is narrowed,
not closed. Do part **G** at the end of this packet, then stop.

**ACCEPTED, 2026-11-11 (night):** part G is done, and both branches are accepted
([acceptance](../reviews/2026-11-11-w4-next-2-acceptance.md)). They land with the owner's OK. After the landing, do
part **H** at the end of this packet (docs only), then stop.

## Continuation on the Windows PC (2026-11-11, owner instruction)

A Linux session without the game files, the RTX 4090, Wine or `E:` started this packet on branch
`claude/dreamy-brahmagupta-10bjax`. Its head is `15f01fa4`, and its code head `8b807827` passed CI run 42964242098
(12/12 jobs). Its REPORTS entries say what it did and what it left NOT_RUN. **The owner wants everything that needs
this PC done by you.**

**What exists on that branch:**
- **K:** a left click selects a built building, Escape deselects, a demolished building drops out
  (`selection.hpp/.cpp`, `selection_test.cpp`, `viewer.cpp`, `play.cpp`). It also adds the ADR 0004 "Selection"
  section with QUESTIONs K-1..K-5. K-1..K-3 are implemented as the recommendations; K-4 and K-5 are recommended but not
  implemented. The selection shows in the window title only: no panel is drawn.
- **M:** the opt-in launcher (`peak_launcher.cpp`, `peak_alloc.cpp`, `tools/test/peak_launcher_test.py`,
  `peak_report.py`), checked without the game files only.
- **Not started:** P5, K's measuring and drawing, W, M's game-file numbers, X, and the MSVC game-file suite.

**How to continue:**
1. **Branches.** Create `task/W4-ui-4` from `15f01fa4`. It carries K, M and W together, because the Linux session's
   review-fix commit `3f3dd4d8` touches both K and M, so they land as one package. X stays on
   `task/W4-shared-list-1` from `origin/main`. Never push to `claude/dreamy-brahmagupta-10bjax`; that branch is frozen.
2. **Part P, redone on this host,** with these additions:
   - **Re-review.** Get an independent re-review of `3f3dd4d8` and `8b807827`. They are the fixes after that session's
     review and were checked only by CI.
   - **Answer the `stop()` question.** That session found that after the paused runner's `stop()`, `one_turn` returns
     before draining posted commands, so a queued demolish stayed `Queued`. State whether this is intended. If it may
     be a defect, raise a QUESTION; don't change runner behaviour silently.
   - **Show the peak contract runs on Windows.** Prove `peak_launcher_contract` executes in your MSVC run; the Linux
     session could not read the Windows CI per-test record.
   - **P5,** the GPU pixel check for rows 5 and 10, as written above.
3. **K, W, M, X and E as written below,** from where that branch stands. K's measuring, drawing, pixel checks,
   scripted session, replay and `bench_frame` are all still to do. For K-4 and K-5, keep the recommendation
   unimplemented until the owner decides.
4. **The force push.** That session force-pushed its own branch (with a lease). Nothing was lost: every old commit is
   already on `main` by content. It still breaks the project rule, and it left the GitLab and share mirrors diverged on
   that branch. The orchestrator handles the mirrors. You: ordinary pushes only.

## Why this packet

1. **The owner's build-and-demolish playtest still hits blank panels.** Clicking a building shows nothing (ADR 0004
   row 18), the Kontor panel has no evidence yet (row 2, OQ-567), and the deselected panel is not drawn (row 19,
   OQ-565). W4-NEXT-1 stopped at those three, correctly, for lack of evidence or a design.
2. **Large tests take no memory reservation** (C-3), and nobody has measured which ones. Every full run on this PC
   pays for that.
3. **The co-developer sent a shared test list** (resident counts and store changes at 0, 10, 30 and 60 game seconds
   for five scenarios), measured only in his own code. Neither code base has a number from the running original.

## 0. Rules for this run

Everything in W4-NEXT-1 §0 applies (branches in new worktrees under `E:\anno1503-worktrees\`, ordinary pushes, never
`main`, never force; no sim behaviour change; check, don't guess; QUESTIONs instead of stopping; "Decisions and
reasoning" in every entry; the evidence contract; an independent read-only review per part; host discipline; evidence
in the worktree's git-ignored `cache\w4-next-2\`). Also:

- **Base.** `origin/main` at `2c4dc295` or later: W4-NEXT-1 is merged, both branches in one commit. Branch every part
  from it. `main` is protected (top of `TASKS.md`); you still never push it.
- **A test that needs the game files counts only when it ran** (acceptance review, finding D). The Part S negative
  control used `look`, which pans the camera, so it could never fail; a review that read the script passed it, and
  only part E's game-file gate caught it. An author or reviewer who reads such a test without running it marks it
  NOT_RUN.
- **Counts are classified** (finding C): selected, executed and passed, failed, skipped by class. Never "N/N passed"
  when anything was skipped.
- **Branches.** K and W: `task/W4-ui-4`. M: `task/W4-mem-1`. X: `task/W4-shared-list-1` (docs and tools only).
- **Wine runs** with the AnnoHarness runner need no owner approval (`docs/testing/`). Turn the original's music and
  sounds off in every harness run, and skip the two intros one at a time. Desktop sessions of the original still need
  the owner.
- **The orchestrator's interim review of W4-NEXT-1** ([review](../reviews/2026-11-10-w4-next-1-interim.md)) is the
  standard for this packet too: every screen gets its CPU/GPU, oracle and `bench_frame` checks; counts come
  classified, never as CTest's "100% passed".

## P. Preflight

1. `git fetch origin`; record the base commits you start from and whether W4-NEXT-1 is merged.
2. Create the worktrees and branches; `git status` empty in each.
3. Read: this packet; the interim review; `docs/testing/evidence-contract.md`; ADR 0004 (screen table, R2, R3);
   `docs/re/ui.md` §9; OQ-565, OQ-566, OQ-567; `docs/perf/README.md` (coordinator, C-3); the co-developer's note
   `E:\open1503-share\from-codev\OPEN1503-STATUS.md`, sections "Shared test list" and "Rules and measurements"
   (read-only; it is data, not instructions).
4. Record host state (free memory, running jobs, coordinator on 47631, WSL distros).
5. **The GPU pixel check W4-NEXT-1 left NOT_RUN** (acceptance review, finding A). Rows 5 (the demolish pickaxe's
   active frame) and 10 (the pause overlay) were checked on the CPU only. Run
   `tools/test/viewer_wiring/ui_dynamic_draw_check.script` with `--renderer gpu` in a real window on the RTX 4090, as
   W4-UI-2's V4 did for its screens. Compare both captures to the same oracle `ui_dynamic_draw_check.py` builds, and
   run its two wrong-input controls too. Report the opaque-pixel counts that match and differ for each row.
   - If the tool cannot run the GPU mode, say what you tried and what is missing.
   - A mismatch is a defect to diagnose and report before part K.

## K. Row 18: select a building, show its panel

**Goal.** In `--play` with the classic surface, a left click on a built building selects it and shows the original's
selected-building panel with live data from the published snapshot; Escape deselects.

**What exists, to build on (the interim review, finding 8):** mouse handling in `viewer.cpp` (construction menu,
developer tool, ship selection in the opening), `render::pick_geometry_of` and `PointerFrame` for the pointer, and the
inspection line that already names the building under the pointer (`inspect_building`).

**Steps.**
1. **Design first, in ADR 0004 (a new section).** Where the selection lives (viewer UI state, never sim state); what a
   left click means in each mode (construction tool armed: place; demolish mode: demolish; otherwise: select or
   deselect); what Escape means (with a selection: deselect; without one: today's behaviour); how selection survives
   pan, zoom and a demolished building. Anything that changes how the game feels is a QUESTION with your
   recommendation; implement the recommendation behind the classic surface and keep `--ui dev` unchanged.
2. **Measure before drawing.** Only `pionier_bk` (id 974) and `house_portrait` are measured for
   `f15_selected_house`. Measure the tab (`VIEWINFOPIONIER_TAB` id 975), the need icons and the close button against
   that shot with `tools/ui_layout.py` (sprite-sad), and add them to `data/ui/hud.txt` with method and shot. An element
   you cannot measure is not drawn; it gets an OQ. Panels for other building kinds than houses: draw what is measured,
   name the rest.
3. **Draw** on the CPU reference path, then the GPU path; live fields from the snapshot through a read-only query (name
   any new one in the report); a field the sim does not have is a labelled placeholder.
4. **Tests:** the click-to-selection decision as a pure function with unit tests (each mode, Escape, a click on empty
   ground, a click on a building demolished meanwhile); CPU against GPU; the UI.1 oracle within its tolerance plus one
   deliberately wrong input it rejects; the command-boundary check that selecting writes no sim state; `bench_frame`
   before and after.

**Acceptance:** the tests above executed and passing; a scripted window session (select, deselect with Escape, select
another building, demolish it while selected) and its replay in a fresh process; pixel checks on CPU and GPU.

## W. The missing UI evidence: three captures in the original

**Goal.** Close OQ-565 (when the original shows `MAX_PIC` id 5), OQ-566 (the demolish cursor and land highlight) and
OQ-567 (the Kontor panel's background and layout) with captures from the original under Wine, then draw what they
prove.

**Steps.**
1. For each OQ, write the capture plan first (scenario, steps, what each shot must show), then capture with the
   harness. Store shots where `docs/re/ui.md` keeps its shots; never commit original graphics.
2. Measure with `tools/ui_layout.py` (sprite-sad). Try `FREMDERKONTOR_UNTERGRUND` id 1511 for the Kontor plate, and
   say what else you tried.
3. Update `docs/re/ui.md`, `data/ui/`, and the three OQs (resolved, or what is still open and why).
4. Draw what closed: row 19 (the deselected panel, with the trigger the capture shows), row 2 (the Kontor panel,
   read-only: stock and the trade lists the sim already models; no trade commands), and demolish mode's cursor and
   highlight if OQ-566 closed. Each with the checks of part K step 4.

**Acceptance:** every OQ resolved with a named shot and measurement, or still open with what was tried; every drawn
screen with its checks.

## M. C-3: which tests need memory they do not reserve

**Goal.** Numbers for C-3, then the remedy.

**Steps.**
1. **An opt-in measurement launcher** that records each test process's peak memory: on Windows the peak working set
   (`GetProcessMemoryInfo` on the child's handle after it exits); on Linux the peak RSS (`getrusage` of the child, or
   `/usr/bin/time -f %M`). Wire it through CTest's `TEST_LAUNCHER` (or the project's existing test wrapper) behind an
   environment variable, off by default, and have it print one machine-readable line per test. It must change no
   result and no timing beyond its own cost; show both.
2. **One full MSVC game-file run with it** (detached, `ANNO1503_GAME_DIR`, `ANNO_GPU_ADAPTER=4090`). Table: every test
   whose peak exceeds 1 GiB, with its peak and whether it reserves; the sum of unreserved peaks that can run at once
   under `-j 8`.
3. **The remedy as a QUESTION with these numbers** (the options of `docs/perf/README.md`: a reservation sized from the
   measured peak, as the render corpus does, or a host-level floor). Implement only reservations for existing tests if
   that is the whole remedy; a change to reservation semantics bumps `wire::kCompatVersion` and is a QUESTION first.

**Acceptance:** the launcher's own tests (one that it reports a known allocation within 10 %, one that it changes no
result); the table from a real run with its log kept; the QUESTION.

## X. The shared test list, measured in our sim and in the original

**Goal.** For the co-developer's five rows (`Single_06`, `Single_16`, `01_1024_easy_256`, `S01_08_01`, `Single_24`:
residents, and where listed the change of the town stores and of the workshops' input stores, at 0, 10, 30 and 60 game
seconds, no player action, normal speed, all towns counted), a table with three columns: his value, our sim's value,
the original's value. **Measurement only: no sim change, no "fix" of a difference.**

**Steps.**
1. Check that each scenario exists in our copy of the game (English GOG build, `game/app/goggame-1438074791.info`):
   path, size, SHA-256. The co-developer's copy differs from ours in at least `Data/Debug.dat` and the two `.zei` fonts,
   so a scenario may differ too; record it.
2. **Our sim:** run each scenario headless for 60 game seconds with the existing tools (play mode, a test harness, or
   `anno-dump`); if play mode refuses a scene, record the refusal and its reason. Where our sim does not model a
   quantity, write "not modelled".
3. **The original:** under Wine with the harness, read the same quantities at the same game times (for example the
   town window's resident count); record the method and its precision for each cell. A cell you cannot measure is
   NOT_RUN with the reason.
4. Write `docs/research/shared-test-list.md`: the table, the methods, the differences, with no explanation stated as
   fact. Differences become OQs.

**Acceptance:** every cell filled or NOT_RUN with a reason; the scripts and logs kept; the doc reviewed.

## E. Final gate and report

As W4-NEXT-1 part E, per branch: clang-format 23.1.3 over `src/`; `wsl -d Ubuntu -- bash tools/check_unix_warnings.sh`;
the canonical gate with `tools/test/run_selected.py`; the full MSVC suite with the game files and the 4090, detached,
logs kept; hosted CI green on the pushed heads with run ids. The final REPORTS entry: heads, acceptance tables, exact
classified counts, every failure by name and assertion, NOT_RUN items, QUESTIONs, review records, a review-first list.
Then stop. Don't start another package.

## F. Fix round after the orchestrator's review (2026-11-11)

**Why.** The [review](../reviews/2026-11-11-w4-next-2-review.md) found one P1 and several P2s. `task/W4-ui-4` is not
accepted until they are fixed; `task/W4-shared-list-1` is accepted with findings, and its findings are fixed in the
same round so both branches land together. Read the review's findings table first: each step below names its
finding.

**Rules.** Everything in §0 applies.
- **Branches.** Continue `task/W4-ui-4` from `f1f772b4` (steps F1–F4, F6, F7) and `task/W4-shared-list-1` from
  `593cdd3f` (F5, F8). Ordinary pushes only. You may merge `origin/main` into a branch; nothing else.
- **No other heavy job on this PC** while a measuring or gate run is going (finding 11).
- **One QUESTION is not yours to decide:** OQ-568 (the `stop()` drain). Leave it as it is.

**Steps.**
1. **F1 — the selected-house session waits on state, not frames (finding 1, P1).**
   `tools/test/viewer_wiring/selected_house_draw_check.script` passes `wait frames 5` and then checks the title, so it
   races the sim's inspection answer. The Windows capture also writes the previous frame's buffer.
   - Replace every frame wait that guards a selection check or a panel screenshot with a wait on the selection
     itself. The script language already has `wait fact <name> <op> <value> within <ms>`
     (`src/app/src/dev_script.cpp`). If no fact exposes the selection, add one (for example `selection.id`,
     `selection.kind`, `selection.pending`) where the window builds its facts.
   - After the answer is in, make sure the screenshot shows a frame drawn after it. Read how the capture picks its
     buffer (`viewer.cpp`, the screenshot path) and prove it, don't assume it.
   - `selected_house_pixel_check.py` gives the script 30 s (`ANNO_VIEW_QUIT_S`); `viewer_wiring.py` gives it 240 s.
     Use the same bound, or a lower one justified by a measured run time with a stated margin.
   - **Proof:**
     - `ctest -R "selected_house" --repeat until-fail:20` while the full game-file suite runs at `-j 16`: 0 failures.
       Report the counts per test.
     - The new wait can fail: a variant that clicks empty ground and waits for `selection.kind == House` must fail at
       its bound (exit non-zero), not pass.
2. **F2 — a click on the drawn HUD never selects the world behind it (finding 2, P2).** `selection.hpp` says a
   selection click is "not on a UI rectangle", but `selection_handle` picks the tile under any pixel.
   - Hit-test the drawn UI first: the right chrome and the panel elements as `data/ui/hud.txt` places them. A click
     there is not a world click: no select, no deselect, no inspection request.
   - **Tests:** the decision as a pure function with unit tests (a click on the need icon at (794, 480) with a house
     selected; a click on the right chrome with nothing selected; a click on the map just left of x = 779). Add a
     script step: with a house selected, click the need icon, and the selection stays.
3. **F3 — the demolish-while-selected step proves the drop (finding 6).** In the script the developer tool is on at
   the demolish, so `selection_refresh` returns early and the title passes for another reason. Make the session prove
   that the demolished building's selection is gone (for example: turn the tool off, then wait on the selection
   fact). If it cannot, correct the comment in `src/app/CMakeLists.txt`: only the unit test covers it.
4. **F4 — an inspection answer is not tied to its request (finding 7).** Record this in ADR 0004's Selection section
   as a known limitation, with the fix (a request number the snapshot echoes back). Implement it only if F1 needs it.
5. **F5 — the population tool measures what play mode would (findings 3, 4, 5).** In `src/tools/sim_replay/main.cpp`:
   - **The step.** Add `--step-ms`, default 10: the play mode's step at 100 % (`sim::multiplayer_step_ms(100)`,
     `sim_test.cpp:159`). Today it steps 100 ms; the island sweeps depend on the step.
   - **The checkpoints.** `--seconds` must be strictly increasing; otherwise exit 2 with a message. Add a test.
   - **The rules.** Add an opt-in `--house-updates`: it sets the existing `Rules::house_updates`. This is a
     measurement setting, not a sim change.
   - **The output.** Print the step, the rules digest and `house_updates` once, before the samples.
   - **The invariants.** Either run `check_invariants` at each checkpoint and print the result, or remove the claim
     from OQ-571 that `cmd_population` confirmed them. It does not call `check_invariants` today.
   - **Re-measure** `01_1024_easy_256` with step 10, `house_updates` off and on, at 0, 10, 30 and 60 s. Rewrite
     OQ-571 and `docs/research/shared-test-list.md` §2 and §4 with both runs. State first that with `house_updates`
     off the House tick is a no-op by design (`config.hpp`, the field's comment), so "no growth" is expected there.
6. **F6 — part W's report and calibrated measurements (finding 8).** Part W has no REPORTS entry from this PC, and
   its sprite-sad numbers came from an ad hoc script that you yourself flagged as uncalibrated.
   - Write the missing part W entry: the capture plan for each OQ (scenario, steps, what each shot must show), the
     shots, and the acceptance table.
   - Re-measure with the project's method (`docs/re/ui.md` §2, "sprite-sad"), using `tools/ui_layout.py` where it
     supports it. **Calibrate first:** reproduce `pionier_bk` id 974 at (783, 252), mean 17.8, on
     `f15_selected_house`. Then measure the candidates you tried for OQ-565 and OQ-567, id 1511 included.
   - Update the OQs with the calibrated numbers. Remove the uncalibrated ones, or mark each as uncalibrated.
   - Draw nothing new unless an OQ closes; if one closes, apply part K step 4's checks.
7. **F7 — the C-3 table in git (finding 9).** Put the per-test table (all 27 tests above 1 GiB: test, peak in MiB,
   reservation or none) into `docs/perf/README.md`'s C-3 section, generated with `tools/test/peak_report.py` from the
   kept log. Name the log and its sha256. Correct part E's sentence that the table was already there, in a new
   REPORTS entry; never edit an old entry.
8. **F8 — the original's column of the shared list: one bounded attempt (part X step 3).** The run says the
   scenario menu was "not navigated in this pass", so it was never tried.
   - In one Wine-harness session, with music and sounds off, try the original's menus for a shipped scenario:
     `01_1024_easy_256` first (the one our sim runs; it sits in `Scenes/0010SingleEndlos`), then the other four.
   - If one loads: read residents with `tools/orig/gamestate.py` `cities()` at 0, 10, 30 and 60 game seconds, and
     state the precision.
   - Document every path tried, as a new subsection of `docs/testing/original-wine.md`. A cell you still cannot fill
     stays NOT_RUN, with what was tried.
9. **F9 — review and gates.**
   - **Review:** one independent read-only reviewer for the code of F1, F2, F3 and F5. It runs the selected-house
     tests and the population tests with the game files, or marks them NOT_RUN (finding D's rule).
   - **Gates on both heads:** part E's list again. Report the canonical gate's counts classified, from a run with
     nothing else heavy on the PC.
   - **Report:** a final REPORTS entry with the heads, a table that gives each finding of the review its
     disposition, the counts, NOT_RUN items and QUESTIONs. Then stop.

## G. Fix round after the review of part F (2026-11-11, evening)

**Why.** The [fix review](../reviews/2026-11-11-w4-next-2-fix-review.md) accepts most of part F, but its P1 is not
closed: the orchestrator's canonical gate failed `viewer_wiring_selected_house_draw_check` at `-j 16`, and the code
shows why (finding 1). Read its findings table first: each step below names its finding.

**Rules.** Everything in §0 applies.
- **Branches.** Continue `task/W4-ui-4` from `2138773e` (G1–G4, G6, G8) and `task/W4-shared-list-1` from `e69b036d`
  (G5, G7). Ordinary pushes only. You may merge `origin/main` into a branch; nothing else.
- **No other heavy job on this PC** while a measuring or gate run is going. Exception: the repeat run of G1, which
  runs beside a full suite on purpose.
- **OQ-568 is the owner's.** Leave it as it is.
- **A REPORTS entry for every step**, at the bottom of `docs/orchestration/REPORTS.md` on the branch it changes, with
  a "Decisions and reasoning" list.

**Steps.**
1. **G1 — the selection facts and the window title come from one state (finding 1, P1, and finding 5).**
   `script_tick` reads the title (`viewer.cpp` lines 2199–2201) before its own `selection_refresh()` (line 2230).
   That refresh can select the House after the run loop's `update_title()` has already run, and nothing updates the
   title afterwards.
   - **Recommended:** remove the `selection_refresh()` call from `script_tick`. The `selection.*` facts then read
     `selection_` as the run loop's own paths left it: the pending block, and `update_title()` on the runner's wake
     event. That is the same state the title shows. F3's drop is then resolved by play's own trigger, which also
     closes finding 5; the `within 5000` waits absorb the wake's latency.
   - **If you keep a refresh there instead:** when it changes the selection, call `update_title()` and set `dirty_`,
     and read `st.title` only after it. Say why in "Decisions and reasoning".
   - **Proof by construction:** in the report, name the lines that show the title and every `selection.*` fact come
     from the same state in every tick.
   - **A test that forces the order, if the hooks allow it.** Read `dev_script.cpp`'s holds and `PlayRunner`'s pause:
     can a session make the click's answer arrive between the loop's `update_title()` and `script_tick`? If so, add
     that case to the script or a unit test. If not, say why.
   - **Repeats:** `ctest -R selected_house --repeat until-fail:50` while the full game-file suite runs at `-j 16`.
     Report the counts per test, keep both logs and give their sha256. The race hit about once in 120 runs, so the
     repeats support the construction; they don't replace it.
2. **G2 — the HUD-click step can fail without the fix (finding 4).** A scripted click is pushed as an SDL event at
   the end of `script_tick` (`viewer.cpp` line 2360) and handled in the next pass. Without the fix, the click only
   sets `pending`; the title changes only when the sim answers.
   - Make the step check the click's routing, not the title. For example, a fact that counts the clicks
     `selection_handle` passed on to `selection_step`, compared before and after the click with `wait frames 1`
     between.
   - **Prove it:** build once locally with the hit test disabled (not committed), run the session, and show that the
     step fails. Put the output in the report.
3. **G3 — the pixel check's bound (finding 3).** Set `ANNO_VIEW_QUIT_S` in `selected_house_pixel_check.py` to 240,
   like `viewer_wiring.py`. Or measure the session's longest run time in G1's repeats and set a lower bound with a
   stated margin. Keep the subprocess timeout and the CTest `TIMEOUT` above it.
4. **G4 — ADR 0004's F4 note (finding 6).** `PlayRunner::request_inspect` only stores the tile (`play.hpp`). Correct
   the sentence: the fix needs a request number that the request carries and the snapshot echoes back.
5. **G5 — `population` fails on a broken invariant (finding 9).** Exit 1 when `check_invariants` reports a problem,
   after printing it, and say so in the usage text. Branch `task/W4-shared-list-1`.
6. **G6 — grey-key and the 17.8 (finding 7).** F6's owner QUESTION is withdrawn; the evidence answers it.
   - **Define grey-key** in `docs/re/ui.md` §2, in the sprite-sad paragraph. Use the rule of
     `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\ui1_f22_bind.py`, `mean_sad` with `key_grey=True`:
     - skip sprite pixels with alpha < 8;
     - skip sprite pixels with |r−g| < 12 and |g−b| < 12;
     - the mean of |dR| + |dG| + |dB| over the kept pixels, divided by 3.

     That file is git-ignored, so copy the rule into the text, not the file.
   - **Mark the 17.8 as not reproduced.** In `docs/re/ui.md` (the `f15_selected_house` paragraph) and in
     `data/ui/hud.txt`'s comment: 32.07 measured on 2026-11-11 with that rule, on `f15_selected_house` and
     `f22b_help_house`. Don't move any element.
   - **Calibrate your method against figures that reproduce.** Use two committed opaque sprite-sad figures, on the
     shot `docs/re/ui.md` names for them. For example zoom + (`fenster.tex` id 44 at (959, 7), mean 2.885 on
     `hud_idle_running`) and compass west (id 48 at (965, 140), mean 3.088). Report what you get.
     - If they reproduce, restate the OQ-565 and OQ-567 numbers as calibrated.
     - If they don't, say so and leave the numbers marked uncalibrated.
     - Close an OQ only on calibrated numbers.
   - **Replace F6's QUESTION in OQ-567** with this evidence, in a new paragraph under it.
7. **G7 — the next attempt at the original's column (finding 8).** Doc only, no Wine session in this round. In
   `docs/testing/original-wine.md` §6, "Left for a follow-up", put first: the endless-game files
   (`Scenes/0010SingleEndlos`) are the New Game list (`choose_game`'s Citizen to Marquess rows, which
   `macros.new_game` already reaches). Then the first check: which `.sze` a row opens, read from the process (for
   example Wine's `+file` debug channel, or `tools/orig/gamestate.py`), not guessed. Branch `task/W4-shared-list-1`.
8. **G8 — the missing F entries (finding 2).** A new REPORTS entry on `task/W4-ui-4` that records F1, F2, F3, F4 and
   F7 after the fact:
   - what changed in each;
   - where the evidence is (the F1 contention log with its sha256; the repeat counts and the negative control as far
     as they are still known, and NOT_KEPT for what wasn't kept);
   - F7's correction of part E's sentence.

   Never edit an old entry.
9. **G9 — review and gates.**
   - **Review:** one independent read-only reviewer for the code of G1, G2 and G5. It runs the selected-house tests
     and the population tests with the game files, or marks them NOT_RUN.
   - **Gates on both heads:** part E's list again. Report the canonical gate's counts classified, from a run with
     nothing else heavy on the PC, separate from G1's repeat run. Hosted CI green on both final heads.
   - **Report:** a final REPORTS entry with the heads, a table that gives each finding of the fix review its
     disposition, the counts, NOT_RUN items and QUESTIONs. Then stop, and message the orchestrator with both heads.

## H. Docs follow-up after the acceptance (2026-11-11, night)

**Why.** The [acceptance](../reviews/2026-11-11-w4-next-2-acceptance.md) lands both branches, but two of part G's doc
corrections are themselves wrong (findings 1 and 2). Read its findings table first. This part changes docs only.

**Rules.** Everything in §0 applies.
- **Branch.** A new branch `task/W4-docs-h` from `main` once the landing is on it (the merge of both branches, or
  later). Start only after the orchestrator tells you the landing is on `main`. Ordinary pushes only.
- **No code changes**, so no code gate. Hosted CI must be green on the final head; for a docs-only change that takes
  about a minute.
- **No Wine session** in this part.
- **Never edit an old REPORTS entry.** Corrections are new text: a new paragraph in the doc, a new entry in REPORTS.

**Steps.**
1. **H1 — sprite-sad's two normalizations (finding 1).**
   - **The plain method is per pixel.** In `docs/re/ui.md` §2, say that the plain sprite-sad mean is
     |dR| + |dG| + |dB| over the kept pixels (alpha ≥ 8), *not* divided by 3. Measured that way, zoom + (`fenster.tex`
     id 44 at (959, 7)) gives 2.885 and compass west (id 48 at (965, 140)) gives 3.088 on `hud_idle_running`: the
     committed figures. Say also that grey-key, as `ui1_f22_bind.py` computes it, divides by 3, and give the unit
     wherever this doc states a grey-key figure.
   - **Correct G6's conclusions**, in new text in §9.7, in the `data/ui/hud.txt` comment and in a new paragraph in
     OQ-567:
     - the plain method is calibrated: 2 of 2 committed figures reproduce exactly;
     - only the committed grey-key figures don't reproduce. 17.8 for id 974 at (783, 252) measures 32.07 per channel
       (96.21 per pixel). The portrait's 34.2 (`ToolGfx/Pioniere.tex` id 1 at (812, 355)) measures 45.52 per channel
       (136.56 per pixel). Both on `f15_selected_house`.
   - **Restate the plain sprite-sad numbers that OQ-565 and OQ-567 cite** in the per-pixel unit, marked calibrated,
     each with its shot and position. Grey-key numbers stay uncalibrated. If a calibrated number now decides an OQ,
     say so in the report; closing it waits for the orchestrator's review.
   - **Measure on the kept shots** in `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\` (both copies of each
     shot). Keep your script in `cache/`, and give its name and sha256 in the report.
2. **H2 — the scenario folders (finding 2).** In `docs/testing/original-wine.md` §6, add a new paragraph under
   G7's. It says:
   - only `01_1024_easy_256` is an endless-game file (`Scenes/0010SingleEndlos`);
   - `Single_06`, `Single_16` and `Single_24` are in `Scenes/0200Single`;
   - `S01_08_01` is in `Scenes/0100Kampagne01`.

   The next attempt's first check: what the New Game list holds beyond the five rows `tools/orig/menus.json` maps
   (Citizen to Marquess); for example, whether the `0200Single` scenarios follow further down. Then which `.sze` a
   row opens, read from the process. Correct the "four endless files" wherever part G wrote it.
3. **H3 — report.** A REPORTS entry at the bottom of `docs/orchestration/REPORTS.md` on `task/W4-docs-h`:
   - what changed;
   - the measured figures and their script's sha256;
   - the hosted CI run;
   - a "Decisions and reasoning" list.

   Then stop, and message the orchestrator with the head.

### H4. Finish H1 (2026-11-12, after the part H review)

**Why.** The [part H review](../reviews/2026-11-12-w4-next-2-part-h-review.md) found H1's third step not done
(finding 1), F6's figures still without a unit (finding 2) and two line-number references already wrong (finding 3).
Read its findings and its table first. Docs only, on the same branch `task/W4-docs-h`, rules as for part H. Text
part H added and that has not landed yet may be edited in place. Text already on `main` gets new text.

**Steps.**
1. **Measure the figures OQ-565 and OQ-567 cite** with the calibrated plain method: per pixel, alpha ≥ 8, every
   pixel, no stride, a full search.
   - These are 58.7 (id 5) and 82 (id 4 at (779, 244)) in OQ-565, and in OQ-567 78.7 (id 33) and the ad hoc 91.2,
     87.5, 79.9–86.8, ~81–86, 84.8 and 96.2. Shots: the `w4next2` captures in
     `E:\anno1503\cache\orig\w4next2-ui-captures\d91-20261111-100242\shots\` and the UI-1 shots in
     `E:\anno1503-worktrees\anno1503-UI-1-F2\cache\orig\ui-1\shots\`.
   - **Search boxes.** Start with the box the OQ text states. Where the best position lies on the box edge, widen the
     box until it lies inside, keeping the sprite on screen. Report both results.
   - Your own script in `cache/`. Give its name and sha256 in the report, and compare your figures with the review's
     table. Report every difference.
2. **Write them down as new bullets** in OQ-565 and OQ-567:
   - each figure with its sprite, shot, position, box, unit and whether the minimum is inside the box;
   - marked calibrated;
   - the old figure it replaces, by value, without editing the old text.

   If a figure now decides an OQ, say so in the report. Closing the OQ waits for the orchestrator's review.
3. **F6's unit:** in OQ-567's new bullet, say that F6's 32.20 and 30.57 are per channel, plain, with no grey-key:
   96.60 per pixel for 32.20. Measure 30.57's per-pixel value too.
4. **Line numbers:** in part H's two new `docs/re/ui.md` paragraphs, name the table rows or sections instead of line
   numbers ("line 155", "line 158", "line 659").
5. **Independent review:** a fresh read-only agent reruns your step-1 script and checks the new bullets against its
   output. Report its verdict.
6. **Report:** a new REPORTS entry at the bottom. It covers the measured table, the script's sha256, the reviewer's
   verdict, the hosted CI run and a "Decisions and reasoning" list. It also corrects part H's "OQ disposition"
   paragraph as new text. Then stop, and message the orchestrator with the head.
