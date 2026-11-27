# Orchestrator Independent Review — `GATE-2a` (`ACCEPTED WITH NOTES`)

- **Date:** 2026-11-21
- **Reviewer:** Windows Orchestrator (`coordinator`, independent read-only audit)
- **Worker:** Swarm worker `win-exec-gate` (`OPEN1503 Windows Orchestration`)
- **Branch / Worktree:** `task/GATE-2a` in `E:/anno1503-worktrees/anno1503-GATE-2a`
- **Base / Merge Ancestry:** `origin/main` `a6468efd81f4fa0502bbb8a3ab9056c0934acb53` (`merge-base --is-ancestor` `rc=0`)
- **Code / Test / Docs Commit:** `7d2797a1a375a4e44bcc9dbfaac40a236914aff3`
- **Report (`REPORTS.md`) `HEAD`:** `74f087ab3ba0cb79cf3f06131977b93c56e68aac`
- **Remote Sync (`origin` / `gitlab` / `share`):** all `74f087ab3ba0cb79cf3f06131977b93c56e68aac`
- **Exact-Head GitHub Actions CI:** Run `44957444089` — `completed success` (`12/12` jobs green: `changes`, `test (windows-latest)`, `lint`, `cpp-format`, `cpp (linux-clang-asan)`, `test (ubuntu-24.04)`, `build-macos`, `cpp (linux-gcc)`, `cpp (windows-msvc)`, `cpp (macos-clang)`, `cpp (linux-gcc-release)`, `gate`)
- **Verdict:** **`ACCEPTED WITH NOTES`**

---

## Verified Strengths

1. **Scope & Boundary Compliance (`7 files changed, 654 insertions(+), 71 deletions(-)`):**
   - Modified only `docs/orchestration/REPORTS.md`, `docs/testing/README.md` (exit code `10` + single retry sentence), `tools/test/ctest_verdict.py`, `tools/test/ctest_verdict_test.py`, `tools/test/run_selected.py`, `tools/test/workflow_test.py`, and `tools/test/wsl_gate.sh`.
   - Zero edits to `src/` (including `src/test_support/`), `fixtures/`, `docs/orchestration/TASKS.md`, `docs/orchestration/STATUS.md`, or `E:/anno1503`. Working tree is clean.

2. **Refusal Classification, Precedence & Real-Failure Protection (`ctest_verdict.py`):**
   - `REFUSAL = re.compile(r"\[anno-test memory\] refused estimate=\d+ reason=\S")` matches all three memory-admission exception throw sites verified across `src/` (`budget.cpp:710` `reserve_bytes` called by `reserve_cpu_render` `721` & `reserve_cpu_setup` `725`; `parallel.hpp:247` `parallel_indexed`; `render_corpus_test.cpp:260` `reserve_picture`), while excluding stderr `admit_line("refuse", ...)` telemetry, silent `CpuToken{}` returns in `acquire_cpu`, coordinator startup diagnostics (`budget.cpp:238`), and generic domain strings such as `"command refused"`.
   - `is_refused_failure()` requires `status == "Failed"`, `refused=True`, zero `sanitizer`/`real_failure`/`failed_asserts`, and `cases > 0` when doctest ran. `CHECK`/`REQUIRE*`/crash/timeout/sanitizer/missing doctest summary/non-refusal throw stays `failed`.
   - Exit-code precedence `9 > 8 (zero selected) > 1 (failed) > 4 (sanitizer) > 10 (refused) > 6 (disallowed skip) > 7 (empty) > 8 (no executed) > 0 (PASS)` matches `gate-2a.md`.
   - `recorded_verdict()` splits `== retry refused <name>` sections via `extract_retry_sections()` and `combine_retries()`, preserves historical `GATE_END` failure status when re-reading old logs, and validates `COUNTS (after retry)` + `RETRIED: <name> passed`.

3. **Runner Retry & WSL Summary (`run_selected.py`, `wsl_gate.sh`):**
   - `run_selected.py` leaves the initial selection log unmodified, retries each refused test serially in sorted name order (`-R <anchor(name)> --no-tests=error -V --timeout <t> -j 1`) under `== retry refused <name>`, honours `--no-retry-refused`, emits `COUNTS (after retry)` and `RETRIED: <name> passed`, and updates effective `GATE_END ctest_rc` when initial selection has no non-refusal failures, incomplete results, or problems.
   - `wsl_gate.sh` (`LF`-only, `18905` bytes, `bash -n` exit `0`) uses `lane_summary()` (`awk` taking the final `^COUNTS` and `^VERDICT` lines from `.out`) so multi-pass retry output records final post-retry totals.

4. **Independently Re-Run Contracts & Real-Log Re-Reads:**
   - Orchestrator direct execution: `python -I tools/test/ctest_verdict_test.py` (`Ran 29 tests`, `OK`), `python -I tools/test/workflow_test.py` (`Ran 35 tests`, `OK`), `python -I tools/test/run_selected.py --preflight-only` (`PREFLIGHT: PASS`), and WSL `bash -n tools/test/wsl_gate.sh` (`rc=0`).
   - Orchestrator direct re-reads with `ctest_verdict.py`:
     - `canonical-gate.log`: `rc=1` (`VERDICT: FAIL recorded gate status=1`), 6 `REFUSED:`, `COUNTS: selected=2564 executed=2532 failed=0 refused=6 empty=0 skipped=26 (required-asset=0 platform=3 opt-in=20 gpu=0 other=3) incomplete=0 sanitizer_lines=0`, `VERDICT: FAIL refused tests (not run)`.
     - `wsl-play-live-1a-sim.log` (WSL): `rc=1` (`VERDICT: FAIL recorded gate status=1`), 3 `REFUSED:`, `COUNTS: selected=1609 executed=1588 failed=0 refused=3 empty=0 skipped=18 (required-asset=0 platform=0 opt-in=18 gpu=0 other=0) incomplete=0 sanitizer_lines=0`, `VERDICT: FAIL refused tests (not run)`.
   - Verified worker cache logs (`cache/gate-2a/`): live MSVC Release `ANNO_TEST_STOP_MB=999999` refusal proof with `--no-retry-refused` (`live-refusal-no-retry.log`, `7a8c56fb8354`, `rc=10`), default retry (`live-refusal-retry.log`, `b504810d8bff`, `-j 1` retry refused again -> `GATE_END status=10`, re-read `rc=10`), normal pass (`live-normal-pass.log`, `ecbbd343d817`, `rc=0`), live contracts (`live-contracts.log`, `8082ca6799bb`, `3/3` executed), and deliberate failure precedence (`delib-failure-precedence.txt`, `248a378b9b71`, `rc=1` and `rc=4`).

---

## Non-Blocking Notes

1. **Full WSL / Full MSVC Game-File Gates Deferred to Integration:** Per `gate-2a.md`, the full WSL `-O1` sanitizer gate and full MSVC Release game-file run were out of packet scope and run at merge/integration. Note also that local `cache/gate-2a/` proof logs live in the gitignored worktree cache until `task/GATE-2a` is landed/cleaned up (`REPORTS.md` and `ctest_verdict` CI artifacts remain tracked).
2. **`TASKS.md` §1 item 4 wording:** Per `gate-2a.md`, `docs/orchestration/TASKS.md` was left untouched by the executor; historical/board text can be aligned when updating main/landings if needed.
