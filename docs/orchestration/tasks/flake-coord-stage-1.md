# FLAKE-COORD-STAGE-1: F4 and F3 (D2) start the WSL coordinator binary without compiling it

- **Status:** draft 2026-11-28 by the orchestrator for the render lane. EGM-reviewed, stages 5-7 (reports in the
  main checkout's `cache/egm/flake-coord-stage-1/`, not in git).
- **Branch:** `task/FLAKE-COORD-STAGE-1`, base origin/main.
- **Size:** one concern, about 2-3 h (the proofs are most of it).

## The failure

In candidate 13's full run (`docs/orchestration/reviews/2026-11-27-candidate-13-review.md` §3; candidate 13 ran in
the `cand12` worktree), `anno_test_support_tests.F4 (N4)` failed with `REQUIRE(false)`. Its log said:

```text
nohup: failed to run command '.../cand12/cache/coord/anno_budget_coord-208a2c085d5db934': No such file or directory
```

The case passed alone and in 20 of 20 looped runs. By then the binary existed.

## Cause, as read by the orchestrator

- On Windows, CMake does not build the WSL coordinator binary. `ensure_wsl_coordinator`
  (`src/test_support/src/coord.cpp`) compiles it on first use with `wsl.exe -d Ubuntu -- g++ ...`, into
  `<worktree>/cache/coord/anno_budget_coord-<build id>`, under the mutex `Local\anno1503-coord-build`. The build id
  is the hash of the coordinator's sources.
- F4 (N4) computes the same path itself (`wsl_coord_binary()` in `budget_f4_test.cpp`) and starts it with `nohup`,
  without the compile step. So on Windows, F4 (N4) fails whenever `cache/coord/` has no binary for the current
  build id and no other client compiled one first: in a fresh worktree, or after any change to the coordinator's
  sources.
- **The same defect, latent in F3 (D2)** (`budget_f3_test.cpp`): it computes a third copy of the path and restarts
  the binary with `nohup` too. It passes today because its client hold usually starts, and so compiles, the
  coordinator first. A leftover coordinator already listening on its port breaks that: the connection succeeds and
  nothing compiles.
- On Linux and macOS, CMake's `anno_stage_budget_coord` copies the binary beside the test executable, so the defect
  is Windows-only.

**Rejected alternative:** a CMake step that compiles the WSL binary at build time. It would tie the Windows build to
WSL, and running the doctest binary directly would still need the runtime step.

**Not in this packet:** the fixed test ports (`test_ports.hpp`) are the same in every worktree. So if two worktrees
ran F3 or F4 at once, one could stop the other's test coordinator. One lane at a time on the PC prevents that; a
per-worktree port band would be its own packet.

## What to do

0. **Confirm the cause** from candidate 13's logs (`cache/gates/cand13-full-01.log` in
   `E:\anno1503-worktrees\cand12`, and that worktree's `build/msvc/Testing/Temporary/LastTest.log`). It holds if:
   - each case ran in its own process (`run_selected.py`);
   - no client that compiles the binary (F4 N3, F3 D2's hold, any shared-budget test) finished before N4 started;
   - the path in the `nohup` line equals the one `coord.cpp` compiles to (same root, same build id).
   If any of these fails, stop and send the orchestrator what you found.
1. **One staging step, Windows-only.** In `coord.cpp`, factor the compile part of `ensure_wsl_coordinator` into:
   ```cpp
   std::optional<std::filesystem::path> stage_wsl_coordinator();
   ```
   - It returns the binary's Windows path, or `std::nullopt` on any failure.
   - Define it in `anno::test`, outside the anonymous namespace where `ensure_wsl_coordinator`, `hex64` and
     `to_wsl_path` live today.
   - Declare it in `src/test_support/include/anno/test/budget.hpp` next to `coordinator_build`, inside
     `#if defined(_WIN32)`.
   - `ensure_wsl_coordinator` calls it, then launches as before: its launch code, including its `to_wsl_path(binary)`,
     stays unchanged.
   - `budget.hpp` is part of the build-id hash (`ANNO_COORD_SOURCES`), so the head's binary gets a new name under
     `cache/coord/`. That is expected.

   While factoring, fix two weaknesses:
   - **Atomic publish.** Compile to `<binary>.tmp-<GetCurrentProcessId()>` (pass the temp path to g++'s `-o` through
     `to_wsl_path`). Then `std::filesystem::rename` it to the final name, using the `std::error_code` overloads.
     - If g++ fails or the rename fails, remove the temp file and return `std::nullopt`.
     - A temp file left behind by a killed client is harmless and accepted.
     - Today a compile cut off mid-write leaves a partial file that every later caller takes as built.
   - **The lock's outcomes:**
     - `WAIT_OBJECT_0` or `WAIT_ABANDONED`: the lock is held; proceed. Log the abandoned case. Atomic publish means
       no partial final file can exist.
     - `WAIT_TIMEOUT`, `WAIT_FAILED`, or `CreateMutexW` returning null: return `std::nullopt` without compiling.
     - Call `ReleaseMutex` only when the lock is held; call `CloseHandle` on every path with a handle.
     - Today a timed-out wait compiles without the lock and then releases a mutex it does not hold, and a null
       handle compiles with no lock at all.
2. **Use it in F4 (N4) and F3 (D2).**
   - **F4:** `wsl_coord_binary()`'s Windows branch calls `stage_wsl_coordinator()` and converts with `as_wsl_path`.
     Its non-Windows branch (`sibling(...)`) stays.
   - **F3:** the `#if defined(_WIN32)` path block in D2 is replaced the same way.
   - **In both,** before the `pkill`/`nohup`:
     `INFO("staging the WSL coordinator in cache/coord failed"); REQUIRE(staged);`.
3. **Failure proof.** Run it only after the orchestrator sends "PC free" and you take the PC.
   - **Why stop the coordinators first:** Windows cannot delete a binary that a WSL process is running, and a live
     coordinator on F3's port makes the hold connect to it and never reach `nohup`.
   - **Stop only this tree's coordinators:** use `pkill -f` with this tree's own `cache/coord/` path, never a pattern
     that also matches another tree's coordinator, such as the shared one on port 47631.
   - **"Without the fix"** is a test binary built in a scratch worktree at the base commit. The test finds its worktree
     from `__FILE__`, so for that binary, empty and match the scratch worktree's `cache/coord/`. **"With the fix"** is
     this worktree's head build.
   - **F4:** empty `cache/coord/`, then run only the case `F4 (N4)` (its full doctest name, as `run_selected.py`
     prints it). Without the fix the `nohup` error reproduces; with the fix it passes.
   - **F3:** empty `cache/coord/`. Copy a compatible `anno_budget_coord` outside that folder (for example to `/tmp`),
     and start it in WSL with `nohup <copy> 27815 any >/dev/null 2>&1 &` (`kPortMeter`). A plain listener cannot
     stand in: the hold refuses a foreign protocol before `nohup`. Then run only `F3 (D2)`. Without the fix the
     `nohup` error reproduces; with the fix it passes. Stop the copy after each run (`pkill -f /tmp/<copy>`).
   - **Loops:** with the fix only, run each case 10 times, emptying `cache/coord/` before each run; for F3, restart
     and stop the copy around each run. The bar is 10 of 10.
   - **The g++-failure path:** make the compile fail once, for example with an unwritable temp location. Show that no
     final binary appears, that the temp file is gone, and that the caller fails with the message from step 2.
   - **The lock-timeout path is checked by review only.** Forcing a 120 s timeout in a test would cost more than it
     proves.

## Files

- `src/test_support/src/coord.cpp`, `src/test_support/include/anno/test/budget.hpp`: modify.
- `src/test_support/tests/budget_f4_test.cpp`, `src/test_support/tests/budget_f3_test.cpp`: modify.
- `docs/orchestration/REPORTS.md`: append the report.

No change to the coordinator's protocol, `kCompatVersion` or reservation semantics.

## Acceptance

- The failure proofs of step 3: before/after for F4 (N4) and F3 (D2), 10 of 10 each with the fix, and the
  g++-failure path.
- `anno_test_support_tests` passes on msvc, changed lanes only, with the PC taken by "PC free" from the
  orchestrator.
- WSL `check_unix_warnings.sh` passes, and hosted CI passes on the exact head commit.
- Report at the bottom of `docs/orchestration/REPORTS.md`, with "Decisions and reasoning", the egm-lite checklist
  path and its baseline commit.
