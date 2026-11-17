# MAC-HEAVY-1: one comprehensive, heavy verification run of `main` on the Mac

**Status:** written 2026-11-17 by the Claude Code orchestrator for **the Mac executor**, on the owner's request:
"I also need a new mac job. This can be more comprehensive once this time, it may contain CPU and or RAM heavy
test jobs". Released when the owner pastes the prompt. It runs after GATE-RUN ui-tabs-b has reported, never
alongside it.

**Base:** `origin/main` at the start, at least `4dfdd9b7`. Name the commit and its tree in the report.
- **Branch:** `task/MAC-HEAVY-1`, a new worktree. It holds only the report and evidence files under
  `docs/orchestration/evidence/mac-heavy-1/`.
- **Scope:** verification only. Fix nothing in `src/`, `tools/` or `data/`, change no test, default, golden
  or pin. A finding is reported with its evidence, not fixed.
- **Never** rebase, force-push or push `main`. No reviewer subagents.
- **Size:** one exception to the packet-size cap, on the owner's request. Machine time may be many hours,
  but each stage has a time box. When a stage passes its box, stop that stage, record what ran, and go on
  to the next.

## Why

Every gate so far runs the suite once, in one order, in Release or Debug. Five kinds of fault slip through
that:
- memory and undefined-behaviour faults on the game-file paths: CI has the sanitizers but no game files, and
  the sanitizers have only ever run over the sim on game files;
- order dependence between test cases: CTest runs each case in its own process. One such fault is known:
  `scene_reclaim_test.cpp:56` after `river_shade_test`, through the shared `SceneReclaimer::shared()`
  counter;
- flaky cases that pass once;
- the long opt-in runs nobody has run lately;
- sim invariants across the whole scenario corpus at longer game times.

This PC is busy with GPU work and timing benches, so the Mac with 18 cores and 48 GiB does it.

## Stage 0: the host and the freeze (15 min)

- Record the CPU, logical cores, RAM, free disk, macOS version, Apple Clang version, CMake and Ninja
  versions, and the load average before starting.
- Record the coordinator (`anno_budget_coord`) state: build, compat version, whether one is already running.
- Record every `ANNO_*` variable you set, in the report and at the top of every log.
- Freeze the source: one worktree at the base commit, `git status` clean. A source change mid-run voids
  that stage: `run_selected.py` exits 9.

## Stage 1: the whole game-file suite under ASan and UBSan (time box 6 h)

1. Configure and build with the `asan` preset (`ANNO_SANITIZE=ON`, Debug `-O1 -g`,
   `-fsanitize=address,undefined,float-cast-overflow`; the sim targets add `-fno-sanitize-recover=all`).
2. Run `python3 -I tools/test/run_selected.py --build-dir build/asan --asset-policy required
   --allow-skip gpu --allow-skip other --timeout 10800 --log <log>`, with:
   - `ANNO1503_GAME_DIR` set;
   - `ANNO_TEST_MB_PER_THREAD=2048`: ASan roughly doubles memory;
   - `--jobs 8`.
   Lower `--jobs` if the coordinator refuses work for memory; refused tests rerun alone, never the whole
   gate.
3. Report:
   - both COUNTS/VERDICT lines;
   - `sanitizer_lines`;
   - **every sanitizer report**: kind, the first 8 stack frames, and the test case that hit it;
   - every failure and timeout;
   - every memory refusal or `bad_alloc` retry;
   - wall time.
   A test that passes in Release but fails under ASan is a finding in itself.
4. Say whether LeakSanitizer was active (macOS arm64 may not support it) and the `ASAN_OPTIONS` and
   `UBSAN_OPTIONS` in force.

## Stage 2: order dependence (time box 2 h)

1. In the `release` build, take the doctest binaries from the CTest inventory
   (`ctest --show-only=json-v1`, the commands' executables).
2. Run each binary **whole**, one at a time, with `ANNO1503_GAME_DIR` set:
   - once in default order;
   - three times with `--order-by=rand --rand-seed=<s>` for s = 1, 2, 3, adding
     `--test-suite-exclude=heavy` to the shuffled runs only.
3. Report per binary and order:
   - cases run, failed and skipped, and wall time;
   - for every failure: the seed, the failing case, the assertion, and the cases that ran before it in that
     process (doctest `--list-test-cases` with the same order and seed, or `-s` output).
4. Treat `scene_reclaim_test` after `river_shade_test` as **known**: report it, don't count it as new.
5. A case that fails only in a shuffled order is an order-dependence finding.

## Stage 3: repeat runs (time box 3 h)

1. Run the full game-file suite in the `release` build **three times in a row** through `run_selected.py`
   (`--asset-policy required --allow-skip gpu --allow-skip other`, default `--jobs`).
2. Report the three COUNTS/VERDICT lines and wall times.
3. List every test whose result differs between the runs, with both outcomes and the failing log excerpt. A
   test that passes only sometimes is a flake finding.

## Stage 4: the opt-in long runs (time box 4 h)

In the `release` build, run each of these on its own (CTest `-R` on its case, or the binary with
`--test-case=`). Each runs with its variable set and `ANNO1503_GAME_DIR` set, and reports pass/fail, wall time
and peak RSS (`/usr/bin/time -l`). The switches are in the test files named:

| Variable | Where | Expected |
|---|---|---|
| `ANNO_IRON_CORPUS=1` | `iron_mine_test.cpp` | every original scene, 300 advances |
| `ANNO_IRON_SURVEY=1` | `iron_mine_test.cpp` | site survey |
| `ANNO_PROGRESSION_GAPS=1` | `founding_assets_test.cpp`, `population_live_canary_test.cpp` | 300 game-second settlement runs |
| `ANNO_PROGRESSION_PROBE=1` | `population_progression_test.cpp` | first upgrade tick |
| `ANNO_SIM_SUPPLY_SCALE=1` | `leather_fixture_test.cpp`, `small_farm_resilience_test.cpp` | 100,000-tick runs |
| `ANNO_SIM_TRANSPORT_STATS=1` | `scenario_test.cpp` | 150,000-tick scenarios |
| `ANNO_SIM_NAV_SCALE=<dir>` | `nav_scale_test.cpp` | nav scale; `<dir>` under the worktree's `cache/` |
| `ANNO_PLAY_LIVE_TIMING=1` | `play_test.cpp` | ms per turn (about 61 s) |

- **Before running each one,** read its test and say whether it asserts anything or only prints. Run it
  either way.
- **Then run `ANNO_SIM_SUPPLY_SCALE` and `ANNO_SIM_TRANSPORT_STATS` once more in the `asan` build,** where
  UB in long sim runs halts. Report any difference.
- **Don't run:**
  - `ANNO_IRON_PROBE`/`SCAN`/`TRACE`/`LEDGER`/`PROBE2`, `ANNO_PROBE` and `ANNO_RESIDENT_MEASURE`:
    diagnostics;
  - `ANNO_SALT_PERF`: timing;
  - `ANNO_SCRIPT2_ORIGINAL_CANARY_DIR`: needs a captured directory;
  - `ANNO_TEST_OLD_COORD`, `ANNO_TEST_V4_*`: need older coordinator builds;
  - `ANNO_REQUIRE_GPU`: the play GPU tests are pinned to the Windows PC's adapters.

## Stage 5: sim invariants over the scenario corpus (time box 3 h)

1. Build `anno-sim-replay` in `release` and in `asan`.
2. For **every `.sze` under `$ANNO1503_GAME_DIR/Scenes`**, run
   `anno-sim-replay population <file> --seconds 0,60,300,600`, once without and once with
   `--house-updates`. It exits 1 when `check_invariants` reports a problem at a checkpoint. Run the scenes in
   parallel (`xargs -P 16`) in the release build.
3. Then run the same command list in the `asan` build with `-P 8`. If the time box would be passed, use
   every 4th scene in sorted order and say so.
4. Report:
   - the number of scenes;
   - per mode, how many exited 0, 1, or other, or were refused at load;
   - every scene with exit 1, with the printed problem;
   - every crash and sanitizer report.
5. **Determinism check:** for each scene and mode, the release and asan output must be byte-identical; list
   every scene where they differ, with the first differing line. Push the release outputs
   (`population-release-{plain,houses}.txt`, one block per scene in sorted order) under
   `docs/orchestration/evidence/mac-heavy-1/`. They are our own sim's numbers, not original content. The
   orchestrator compares them with the Windows PC's.

## Stage 6: the atlas measurement across platforms (time box 1 h)

1. In the `release` build, run `bench_frame <game_dir> - --atlas-report atlas_report.tsv` (RENDER-DIRECT-1a's
   whole-corpus walk).
2. Push the TSV under `docs/orchestration/evidence/mac-heavy-1/`, and report its row count and the sha256
   of its byte columns only (state which columns).
3. The orchestrator compares it with the Windows PC's `atlas_report.tsv`: the byte columns must match on
   both platforms.

## Report

Put it at the bottom of `docs/orchestration/REPORTS.md` on `task/MAC-HEAVY-1`, headed **READY FOR REVIEW**,
with:
- base commit and tree; the host record;
- per stage: what ran, the counts, wall time, every finding with its evidence (log path and excerpt), and
  what did not run and why (a time box, a refusal);
- **a findings table:**
  - one row per finding;
  - its kind: sanitizer, order, flake, opt-in failure, invariant, determinism or atlas mismatch;
  - test or scene, first evidence line, reproducible yes/no, and the command that reproduces it;
- "Decisions and reasoning" (board §1 item 12): every choice you made where this packet was silent.

Commit the report and evidence files only and push the branch. The full logs stay in the worktree's `cache/`,
named in the report with their sha256.
