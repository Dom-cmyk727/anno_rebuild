# W4-UI-2: the original's HUD and construction menu in the live window

**READY / RELEASED 2026-11-07. Owner: the Windows executor. The Claude Code orchestrator wrote this packet at the
owner's request.** It starts right after W4-MAIN-2 part E is reported, with no stop in between.

Work through the parts in order: **P → F → R → H → C → M → V → K → E**. Re-read this packet and your latest REPORTS
entries after any context compaction, before you continue.

## Why this packet

- **The owner decided how they test.** On 2026-10-30 (decision M3, `docs/plan/lan-mvp.md` §1) the owner chose to test
  building and demolishing **through the original's in-game UI**, reverse-engineered with high precision.
  - Every menu and submenu exists.
  - An entry whose feature the sim lacks is a placeholder that leads nowhere until it exists.
- **That UI does not exist yet.**
  - UI.1, the spec, was accepted and merged on 2026-10-30 (`data/ui/`, `docs/re/ui.md`, `tools/ui_layout.py`).
  - Its follow-up UI.1.F2 stopped at "F2.1 IN PROGRESS" on 2026-10-30. The branch got one more commit on 10-31.
  - ADR 0004 was never written, so UI.2 never started.
  - Every playtest since has used the developer tool, which is "a test tool... not UI.2" (`dev_tool.hpp`).
- **It serves both MVP checkpoints.** MVP-A needs a basic HUD, and the owner's build-and-demolish checks need the
  construction menu (`lan-mvp.md` §4 W5, §5).

**Done means:**
- the owner opens `--play`, sees the original's HUD, opens the construction menu, picks a building in any of the eight
  categories, places it and demolishes it with the pickaxe, all through the real command path;
- locked and unaffordable entries behave as the original's do;
- every other menu entry exists as a placeholder;
- the HUD and the menu match the UI.1 oracle within its tolerance, on the CPU and GPU paths;
- a session replays in a fresh process.

## 0. Rules for this run

### 0.1 Branches and merges

- **F (UI.1.F2.1)** continues on `task/UI-1-F2-ui-remaining` from `origin`'s head `750eace0`.
  - Your old worktree `E:\anno1503-worktrees\anno1503-UI-1-F2` is at `a2619e29`, one commit behind; update it with
    an ordinary fast-forward.
  - F changes no `src/` file.
- **R through K** go on a new branch `task/W4-ui-2`, created from the head W4-MAIN-2 part E reports: the D branch
  `task/W4-expand-live-1` after its fixes, which contains B and S. Record that start commit.
- **Authorized merges into `task/W4-ui-2`:**
  1. `task/UI-1-F2-ui-remaining`, once F is reported. It is based on the 2026-10-30 board, so resolve the
     `docs/` conflicts and record every resolution.
  2. `origin/main`, but only after an orchestrator writes in STATUS that the W4-MAIN-2 parts are on `main`, or that
     another consolidation you need has landed. Record both parents and every resolution.

  No other merge, no rebase, and no other lane's branch.
- **Push** your branches at every checkpoint, with ordinary pushes only. Never push `main`, and never force.

### 0.2 Identity and the sim

- **This packet changes no sim behaviour.** The UI is a consumer: it reads the published snapshot and sends commands
  through the existing command path (W4.4: `BuildCmd`, `DemolishCmd`). It never mutates sim state.
- **The identity stays the one your base carries:** state134 / ECONBK1 / PRNG5 / replay4 / command1.
- **A new read-only sim query is allowed** if it changes no state, stream, hash or draw. An example is the eligibility
  of every type for one player in one call. Name it in the report.
- **Anything else in `src/sim`:** QUESTION first.

### 0.3 Don't stop: QUESTIONs

- Don't wait for a reply. If a step needs a decision from the owner or an orchestrator, write it as a **QUESTION** in
  that part's REPORTS entry: what is needed, the options, and your recommendation with its evidence.
- Then skip only the work that depends on it, and continue with the next independent step.
- Stop only at E, or when nothing independent is left.
- **Owner questions this packet expects:**
  - **R2:** is the original UI the default in `--play`?
  - **R3:** the UI scale at 1440p and 4K.

  Until the owner answers, implement the recommended option behind a switch, and say so on the playtest card.

### 0.4 Publication: GitHub is public

- Never put original pixels, decoded images, UI strings, screenshots or oracle shots into git.
  - `data/ui/` holds only ids, coordinates, sizes, file names and indices (its README).
  - Shots stay in `cache/orig/ui-1/shots` (git-ignored).
- The UI loads textures (`ToolGfx/*.tex`, `.inc`), fonts (`.zei`) and texts (`Texte.dat`) from the player's game
  folder at runtime.
- Tests that need them skip with a message when `ANNO1503_GAME_DIR` is unset (`anno::test::game_dir`).

### 0.5 Shells, memory and evidence

- **Evidence goes outside every worktree:** `E:\anno1503-evidence\w4-ui-2\` (and `w4-main-2\` for part P). Copy the
  WSL gate logs there from `~/anno-gate-logs/` before any worktree is removed.
- **Long commands:**
  - run them in the tool's background mode, never with `&`, `nohup` or `setsid` inside `wsl.exe`;
  - give each a time limit and a log;
  - read the exit code and the last lines before the next step;
  - check a long job at least every 15 minutes.
- **Memory refusals** (`low-memory stop`, `wait deadline`) mean the test did not run. Report it as "not executed
  (memory refusal)", never as a timing or flaky failure, and rerun it at lower `-j`.
  - This morning's review found 13 render refusals at `-j 16` that the report attributed to timing.
- **The WSL GPU lane runs on lavapipe and is slow:** at 17:24 on 2026-11-07, `w4main2c1`'s GPU lane had run for
  about two hours with one test process and was still running.
  - Run the UI's pixel tests natively on the RTX 4090 and on the Radeon iGPU.
  - Use the WSL GPU lane once, at E.
- **Text files:** edit them with Edit/Write or `python -I`. Never rewrite them with PowerShell 5.1 `Get-Content` or
  `Set-Content`, which mangle UTF-8.

### 0.6 The original game

- **Wine-runner captures are pre-authorized:**
  - run them silent: music and sounds off;
  - skip the two intros one at a time.
- **A desktop session of the original needs the owner's OK.** Write the steps as an owner checklist instead.

### 0.7 Out of scope

- **Gameplay changes,** and the Mac lanes' chains and branches. This includes salt, tools, the quarry and routes.
  - When the first Mac's mine work reaches `main`, the mine cell's deposit warning (`mine_site_deposit_in_reach`) is a
    later consumer task.
- **The lobby and network UI** (W3/W5), the main menu, and the trade and route screens beyond placeholders.
- **W4-SHORE-2** (not released), and **W4-DEPTH-1** (it waits on the owner's depth check).
- **The developer tool's removal:** it stays, behind its key.

## P. Finish W4-MAIN-2 first (unchanged)

Complete the follow-ups from this morning's [review](../reviews/2026-11-07-w4-main-2.md), in its order:
1. **D's fixes:** commit the scripted `l` key; fix the "pending Load" test so it saves before it applies the Load.
2. **C1 to its end.** At 17:24 its GPU lane was still running.
   - **Its CPU render lane ended FAIL at 15:22,** 184 of 185 executed: the known case `a flow build that fails leaves
     the legacy look, final, built once (game files)`. The 20× repeat in C2 is meant to settle that case.
   - Copy every log from `~/anno-gate-logs/anno1503-W4-main-2-gatecheck/` into `E:\anno1503-evidence\w4-main-2\`.
   - Name every test that did not run.
3. **The owner-rule game-file run at `main` `5e7b13c6`,** if no one has run it yet. Do it in a clean checkout, with
   the full `ctest --preset msvc`, the game files and the 4090.
   - Record the counts and every failure, with its attribution.
4. **The rest of C, then D5, D6 and E,** as written in the review.
   - E's report is the start signal for this packet.

**Standing duty for the whole run:** if an orchestrator asks for the owner-rule game-file run at a `main` commit, it
comes first. Pause this packet at a clean checkpoint, run it, report it, then continue.

## F. Close UI.1.F2.1, the graphics of the build-test surface (no `src/` change)

The F2 block on the board (TASKS.md, "UI.1.F2", round dt) stays binding. Your 2026-10-30 progress report already
covers:
- the cell icon (`THUMBNAIL*` + 3 × record index);
- the `VIEWBAUTAB` column;
- the zoom minus;
- `LUPE` and the compass;
- the status-bar portraits;
- the pause parchment;
- the tolerance, restated to fraction 0.08 with three negative controls;
- OQs 540, 542, 543, 544 and 545, closed.

**F1. Close what F2.1 still lacks,** checking each item of the F2.1 list against the block:
- the info panel's cost and upkeep icons;
- the pressed frames, captured as pixels if the gadget code alone leaves them unproved;
- the selected-house panel's origin;
- every remaining mask that existed only for an unbound graphic.

**F2. Deliver the table the block asks for:** every element of the build-test surface, bound or open, each open one
with its reason and OQ.

**F3. Check the owner checklist.** `750eace0` added the F2.3 desktop capture checklist. Confirm that it carries the
OQ-535 predictions for 800 × 600, 1152 × 864, 1280 × 960 and 1280 × 1024.

**F4. Gates:**
- the UI.1 gates;
- `check-files`, and `validate --game-dir`;
- `mock` and `compare` for the idle HUD and all eight categories, with the negative tests.

Report it as `## UI.1.F2 — F2.1 closure — ... — status: REVIEW PENDING`, then push. F2.2, the remaining reachable
screens, is optional work for idle time at the end (E).

**R onward does not wait for F's review.** This replaces the board's UI.2 precondition "F2.1 accepted". Changes that
review asks for go onto `task/UI-1-F2-ui-remaining` and are merged into `task/W4-ui-2` again.

## R. ADR 0004: the UI toolkit (written, then implemented)

The owner already decided D4: our own small widget set, drawn by the GPU compositor, with the original's fonts and
`ToolGfx` textures (`lan-mvp.md` §4 W5). The ADR records that decision and its design.

**R1. Write `docs/adr/0004-ui-toolkit.md`** with status Proposed. It covers:
- **Widgets:** the widget model and the layout, read from `data/ui/` at the 1024 × 768 reference with the anchoring
  rules of `docs/re/ui.md` §5. No coordinate lives in code.
- **Input routing:** SDL3 events go to the UI first, then to the world. The developer tool keeps its own key.
- **Text:** rendered with the `.zei` fonts, from Windows-1252 strings loaded from `Texte.dat` at runtime.
- **Atlases:** the textures are uploaded once into atlases; no upload per frame.
- **Rendering:** the GPU path and a CPU reference path, compared pixel by pixel (CLAUDE.md).
- **The command boundary:** the UI only reads snapshots and sends commands.
- **Testing:** the oracle pixel tests, with their masks and negative controls.

**R2. QUESTION for the owner: the default surface in `--play`.**
- **(a)** the original UI on, with the developer tool on its key: **recommended**, because it is the owner's chosen
  test surface;
- **(b)** the developer tool, as today, with the original UI on a switch.

**R3. QUESTION for the owner: the UI scale.** The original draws at 1024 × 768; R12 asks for up to 4K.
- **The options:** integer factors 2 and 3, a fractional factor, or a player setting with a default.
- **For each option:** screenshots at 2560 × 1440 and 3840 × 2160, and the sharpness trade-off.
- **Then your recommendation.**

## H. The HUD (UI.2, slice 1)

**H1. Draw the HUD from `data/ui/hud.txt`.** That is every element at the 1024 × 768 reference: the status bar, gold,
the residents, the build-material counters (`STATUSBAR_BAUMAT`), the right chrome and its tabs, and the minimap frame
with its controls.
- Use the original textures from the game folder, through the existing `.tex`/`.inc` and `.zei` parsers.

**H2. The values come from the published snapshot, read-only:**
- the player's gold;
- the residents;
- the build goods (HOLZ, WERKZEUG, ZIEGEL, MAMOR) of the city the original shows.

`docs/re/ui.md` §2.3 says which city that is. Where the spec is silent, write an OQ, choose a labelled developer
default, and put it on the card.

**H3. Layout at other window sizes:**
- use the anchoring rules of `ui.md` §5;
- apply the scale from R3, or the recommended option behind a switch;
- OQ-535's predictions wait for the owner's capture (F3).

**H4. Pixel tests:**
- the idle HUD at 1024 × 768 with fixed values, against the UI.1 oracle mock (`tools/ui_layout.py mock`/`compare`,
  channel 48, fraction 0.08, `masks.txt`);
- on the CPU path, on the GPU path on the 4090, and on the Radeon iGPU;
- with the three negative controls: a wrong element, a one-element shift and a missing element must each fail.

## C. The construction menu, with placement and demolish (UI.2, slice 2)

**C1. Build the menu from `data/ui/construction.txt`:**
- the eight categories, the 148 cells (`View.dll` tables), and the cell icons (`thumb_tex + 3 × record_index`);
- the category column, the banner and the info panel;
- `NO_KLICK` on locked cells.

No type id in code: everything comes from the data.

**C2. Locks: the sim's eligibility is the authority.** It is the check that refuses stage-locked types: in code,
`construction_eligibility(PlayerId, TypeId).allowed()`, the call the legal workload uses.
- Compare it, over all 148 cells, with the original's UI lock rule in `ui.md` §3.2 (INFRAMIN against INFRA_EVER).
- Report every cell where they differ, as an OQ with its evidence. Don't change the sim.

**C3. The info panel:**
- the gold and build-goods costs, from the registry (HEAD bytes 3–6 and the gold cost);
- the upkeep, where the snapshot carries it; otherwise blank, with an OQ.

An unaffordable entry shows as the original shows it (`ui.md` §2.5).

**C4. Placing:**
- **Arming:** choosing a cell arms placement.
- **The preview** applies the sim's placement rule read-only, as the developer tool does today.
- **Rotation and cancel** use the original's keys (`ui.md` §4).
- **A click sends `BuildCmd`** through the same command path as the developer tool. Drag where the original drags,
  as for roads (`docs/sim/road-placement.md`).
- **A refusal is shown with its reason.** Use the original's message where `Texte.dat` maps it at runtime; otherwise
  a short text of our own for the `Reject` code, plus an OQ.

**C5. Demolishing:** the pickaxe sends `DemolishCmd`, with the original's cursor and confirmation behaviour
(`ui.md` §2.6, §4).

**C6. What the UI never does:**
- grant gold or goods: developer resources stay a developer option (M4);
- bypass a refusal.

Mines and quarries are refused by the sim on this base; show that refusal.

## M. Every other menu: placeholders

- **Create every remaining screen and submenu** of the screen tree (`ui.md` §1): each tab of the right chrome, the
  Kontor, the options, the statistics, the diplomacy and the messages.
- **Each opens its frame** where the graphic is bound, and otherwise a labelled placeholder frame.
- **It leads nowhere,** and no entry fakes a feature the sim lacks.
- **List them in a table:** screen, bound or placeholder, the sim feature it waits for.

## V. Proof

**V1. A window session** through the production SDL path, driven by `DevScript`, which gains clicks on UI rectangle
centres from `data/ui/`:
1. open the construction menu and switch categories;
2. place a building of each implemented class you can afford: a house, a forester, a road and a market stall;
3. watch each appear in the picture;
4. demolish one with the pickaxe;
5. click a locked cell: no command is sent;
6. try an unaffordable build: the refusal is shown;
7. quick-save, resume, continue.

**V2. A replay** of the session's command log in a fresh headless process, with matching outcomes and hashes. A
changed command must diverge at a recorded checkpoint.

**V3. The consumer boundary:**
- a test that the UI only reads `const` snapshot data and sends commands;
- the state hash is unchanged by opening, browsing and closing menus without a command.

**V4. Pixel tests:**
- the HUD and all eight categories, on CPU, the 4090 and the iGPU;
- the negative controls;
- one placeholder frame.

**V5. Mutation checks.** Each of these must make a test fail:
- a cell mapped to a wrong type;
- a locked cell that accepts a click;
- a click that sends no command;
- a wrong cost shown.

**V6. Performance** (PLAN §0.3, `docs/perf/README.md`): the UI's cost per frame at 1024 × 768, 2560 × 1440 and
3840 × 2160 on the 4090, with `bench_frame`.
- Give p50 and p95, with and without the UI.
- Check that no texture is uploaded per frame.

## K. The owner's playtest card

Write `docs/testing/ui-2-playtest.md`:
- how to start (`--play`, the scene, developer resources if needed);
- what to try: every category, place, rotate, cancel, demolish, a locked cell, an unaffordable building;
- what each placeholder means;
- which defaults wait for the owner's answers to R2 and R3, and how to switch them;
- how to report a mismatch with the original: a screenshot, the window size, the action.

## E. Final gate, review and stop

- **One fresh, independent, read-only reviewer** for the combined branch: the code, the consumer boundary, the pixel
  evidence and the claims.
- **Frozen source, then:**
  - the full `ctest --preset msvc` with the game files, the GPU tests on the 4090 and the iGPU included;
  - the WSL gate, all lanes once, logs outside the worktree;
  - clang-format 23.1.3;
  - `tools/check_unix_warnings.sh`;
  - CI on the pushed head: all ten jobs.
- **Name** every test that did not run, and why.
- **The REPORTS entry:**
  - a verdict per part;
  - the element and screen tables;
  - the R2/R3 questions with your recommendations;
  - a "Decisions and reasoning" list;
  - the evidence paths;
  - status **REVIEW PENDING**.
- **Then:** push, and stop with a clean tree and no jobs running.
- **With time left,** F2.2's reachable screens through the silent Wine runner, as a separate REVIEW PENDING
  checkpoint.

## If something blocks

- If F stalls on an element, mark it open with its reason and continue to R. The HUD and the menu draw a labelled
  stand-in for an unbound graphic, and the pixel test masks it explicitly.
- If C2 shows large disagreements between the sim's eligibility and the original's lock rule, keep the sim as the
  authority, list every case, and continue.
