# CI-SPEED-1: bound every CI job's time, cache the four Unix C++ builds, and plan the MSVC one

**Draft 2026-11-27** by the orchestrator for the UI lane. The task runs on hosted CI only; it needs no local build or
test. Branch `task/CI-SPEED-1`, base origin/main. One concern, about 2-4 h. Revised after the EGM review (stages 5-7;
reports in `E:\anno1503\cache\egm\ci-speed-1\`, local only).

## Why

**Baseline.** The basis is the 37 most recent full runs before 2026-11-27, counted by the orchestrator. A full run is
one that the `changes` job classifies as code. A docs-only change skips every job except `changes` and `gate`, and
takes about a minute.

Medians per C++ job, in minutes:

| C++ job | Build step | Whole job |
|---|---:|---:|
| `windows-msvc` | 40.6 | 43.9 |
| `linux-clang-asan` | 19.2 | 23.5 |
| `linux-gcc-release` | 12.5 | 15.5 |
| `macos-clang` | 10.0 | 17.1 |
| `linux-gcc` | 8.4 | 13.7 |

Test steps take 1.6-5.5 min, and `apt-get` about 2 min on Linux. Compiling is about 90 % of every C++ job.

**Current state:**
- No C++ job caches its compiles. Only the Rust jobs use `Swatinem/rust-cache`.
- No job has `timeout-minutes`, so a hung job would run for GitHub's 6 hours. This is a precaution: no such run has
  been recorded.
- The `msvc` preset sets no `generator`, so CMake uses its Windows default, the Visual Studio generator. CMake runs
  a compiler launcher (`<LANG>_COMPILER_LAUNCHER`, the way ccache and sccache hook in) only with the Makefile and
  Ninja generators ([CMake docs](https://cmake.org/cmake/help/latest/prop_tgt/LANG_COMPILER_LAUNCHER.html)).

## Goal and non-goals

- **Goal:** the four Unix C++ jobs build faster on a warm cache. That frees runner minutes and lets Linux/macOS
  failures arrive sooner.
- **Goal:** every job has a time limit.
- **Goal:** a measured proposal for caching `windows-msvc`.
- **Non-goal:** shorter whole runs, or a shorter `land.py` wait. `windows-msvc` alone sets a run's length (43.9 min
  median, about twice the slowest Unix job). Shortening it is the follow-up, CI-SPEED-2, designed from step 4's
  numbers.

## Alternatives considered

- **MSVC first, in this packet.** Rejected for now. It needs a CI-only Ninja preset with a vcvars step, or ccache's
  Visual Studio route (ccache copied in as `cl.exe` and given to MSBuild through `CLToolExe`/`CLToolPath`). Either
  one changes how the slowest job builds, so it gets its own packet once there are numbers.
- **sccache instead of ccache on Unix.** Either works through the launcher. ccache needs no server process. If sccache
  is chosen for MSVC later, switching is a one-line change.
- **Cutting test time.** Rejected: tests are 1.6-5.5 min of each job.
- **Larger runners.** Rejected: they cost money, which is the owner's call.
- **Caching `_deps`** (CMake FetchContent's downloads: SDL3, doctest, wgpu-native). Rejected: three hash-pinned
  downloads that take seconds.

## Risks

- **A timeout too tight** fails a slow but healthy run and blocks a landing. Mitigation: the timeouts in step 2 are
  generous.
- **Queue waits count toward `land.py`'s 120 min but not toward a job's timeout.** Mitigation: no job's timeout
  exceeds 90 min.
- **Cache entries could crowd out `main`'s or the Rust caches** within the 10 GB quota. Mitigation: only `main`
  saves, and the size is capped (step 3).
- **A runner image change empties the cache**, so the first run after it is cold. That costs the save step's time
  once; it never fails the job.

## Implementation Plan

1. **Longest times for the timeouts.** Find the longest full run of the last 60 (`gh run list --workflow ci.yml`,
   then `gh run view <id> --json jobs`). Record each job's whole-job minutes in it.
2. **Timeouts on every job.** Set `timeout-minutes` to step 1's time for that job plus about half.
   - At most 90 min for any job.
   - 10 min for `gate`. That touches none of its name, `needs` or logic.
   - State each value and its source.
3. **ccache for linux-gcc, linux-gcc-release, linux-clang-asan and macos-clang.** The build step dominates all four.
   - Set `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` in the CI step, not in a shared preset. Cite
     ccache's docs, and the action's README if you use an action.
   - **Restore on every run; save only on a push to `refs/heads/main`.** A cache entry can be restored only by the
     branch that saved it, and by any branch from the default branch. Every landing builds on a new `land/*` branch,
     so only `main`'s entries help, and branch entries would only crowd them out. Use `actions/cache/restore`
     everywhere and `actions/cache/save` under that condition.
   - Key: OS, compiler, preset and build type, plus the run id. The restore prefix leaves out the run id.
   - Cap the size with `CCACHE_MAXSIZE`. Each `main` push saves a new generation, and GitHub evicts the least recently
     used entries once the repository's 10 GB is full. Size the cap so that two generations of the four jobs, plus
     the Rust caches, fit. Give the sizes in the report.
   - A docs-only push to `main` skips the build jobs and saves nothing. That is expected.
4. **MSVC: measure and propose, don't change.** From one recent `windows-msvc` log:
   - its debug-info and optimisation flags;
   - its configure, build and test minutes.
   Then compare the two routes from "Alternatives considered" with a cited source each: what each needs, its risk,
   and the gain you expect. Do not change the `msvc` preset the owner builds with.

## Do not touch

- The `gate` job's name, `needs` list and classification logic. `tools/orch/land.py` requires the check `gate` on
  the exact landing SHA.
- No `concurrency` / `cancel-in-progress`. `land.py` pushes a commit to a `land/*` branch, waits for that run's
  `gate`, then pushes the same commit to protected `main`. GitHub's protection waits for the newest check run on
  that commit: on 2026-11-14 a second run on the same SHA, still in progress, made `main` refuse with "Required
  status check 'gate' is expected". A cancel rule changes which runs exist and finish, so it needs its own design.
- No pinning of actions to commit SHAs.
- No change to `CMakePresets.json`, compile flags, warnings, sanitizers or the test set. Faster, never weaker.

## Acceptance

- Hosted CI green on the exact head.
- Step 1's longest-run table, the timeouts with their sources, and step 4's MSVC numbers and proposal.
- **Compile commands unchanged.** For one cached job, show that the compile command lines with and without the
  launcher are equal apart from the launcher (from `compile_commands.json` or the verbose build log).
- **No cache saved from the task branch.** Show that the save step was skipped. The branch's first run restores
  nothing, because `main` has not saved a cache yet.
- **The warm path is checked after landing, by the orchestrator:** the first `main` run saves the cache, and the next
  fresh `task/*` branch restores it. The orchestrator records those build-step times against the medians above. If
  the warm build steps are not clearly shorter, the cache comes out again. Until then the speed-up is unmeasured,
  and the report says so.
- Landing goes through `land.py --allow-ci-change`, after the orchestrator has reviewed the workflow diff.
- Report at the bottom of REPORTS.md, with "Decisions and reasoning". Use `/egm:egm-lite` and ponytail full. Start no
  reviewer subagents.
