# W4-NEXT-2: the selected-building panel, the missing UI evidence, test memory, the shared test list

**RELEASED 2026-11-10** by the Claude Code orchestrator at the owner's request, to the Windows executor (Claude
Sonnet 5.5 on the owner's Windows PC). **Start it now:** W4-NEXT-1 is accepted and merged into `main` as `2c4dc295`
([acceptance review](../reviews/2026-11-10-w4-next-1-acceptance.md)).

Work the parts in order: **P → K → W → M → X → E**. Each part ends with a REPORTS entry and a push of its branch.
Re-read this packet and your latest REPORTS entries after any context compaction.

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
