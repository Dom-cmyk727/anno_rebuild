# GATE-1b review (2026-11-12)

**Verdict: ACCEPTED.** Each required contract now runs once per gate, and the automatic `picture_matrix_contract` once
instead of in every lane. The default WSL gate's lanes take **4,190 s instead of 4,958 s** (−15 %), with the same tests
and verdicts, all green. One small regression is found and moved into GATE-1c as item 3, so there is no fix round. The
hosted `lint` failure the executor reported is real but not this package's: Rust 1.100.0 reached the runners the same
day. The orchestrator fixed that one line on the landing candidate.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/GATE-1b` `e63145e9`, three commits on `main` `f5ac2382`: `tools/test/run_selected.py`
(+45/−13), `tools/test/workflow_test.py` (two new cases), `tools/test/wsl_gate.sh`, `docs/testing/README.md`, the
REPORTS entry and a CI addendum. Merged as `b81d3fdd` without conflicts; the orchestrator's lint fix on top is
`75a3c779` (branch `orch/land-gate-1b`).

## What the orchestrator checked

- **The change.**
  - `selection()` now leaves the already-run required contracts out of the selection through anchored `-E` names.
  - A selection left with only required contracts writes its own `COUNTS`/`VERDICT: PASS` rows, in `ctest_verdict`'s
    format.
  - `--skip-auto-contract` only drops the automatic UNIX addition; an explicit `--require-test` still works.
  - `wsl_gate.sh` passes it in every lane after `app`, and in the optional GPU lane.
  - The README's new paragraph says which of the three GPU-parity runs covers what.
  - The canonical Windows call (`--select "."`, no required contracts) takes the same path as before.
- **The proof gate, from its own files** (`~/anno-gate-logs/anno1503-GATE-1b-gatecheck/` in Ubuntu):
  - built at `e9be274b` in a separate checkout at `-O1 -g`;
  - configure 17:59, build 371 s, last lane 19:17 (1 h 18 min; GATE-1a's took 1 h 32 min);
  - every lane `VERDICT: PASS`, `sanitizer_lines=0`, no required-asset skips;
  - one `== required contract` line per contract in the whole gate: `picture_matrix_contract` and the app lane's four
    each appear once.

  | Lane | Selected / executed / failed / skipped | Seconds (GATE-1b) | Seconds (GATE-1a) |
  |---|---|---:|---:|
  | app | 310 / 308 / 0 / 2 opt-in | 1,572 | 2,156 |
  | sim | 1,609 / 1,591 / 0 / 18 opt-in | 1,867 | 1,976 |
  | formats | 97 / 97 / 0 / 0 | 165 | 225 |
  | render | 185 / 185 / 0 / 0 | 548 | 505 |
  | test support | 93 / 89 / 0 / 4 opt-in | 38 | 96 |
  | **Total** | | **4,190** | **4,958** |

  The app lane's −4 are the four required contracts, no longer counted twice. The flow-build test passed again; GATE-1c
  still has to find its cause.
- **`python -I tools/test/workflow_test.py`:** 23 tests OK on Windows at `75a3c779`, as the executor reported on
  WSL. On Windows the `--skip-auto-contract` case checks nothing (its body is UNIX-only); the hosted UNIX jobs run it.
- **Hosted CI:**
  - `e9be274b` (the code) green on the first try, `gate` included;
  - `5209c496` and `e63145e9` (REPORTS only) failed `lint`, see below;
  - the candidate `75a3c779`: run 43287409584, `lint` green on Rust 1.100.0 for the whole workspace;
  - the candidate with this review and the GATE-1c release, `db118177`: run 43287945521, every job green on the
    first try, `gate` included.
- **Windows game-file run** at `75a3c779` (the canonical call, game files, the 4090): see "Game-file run" below.

## Finding, moved into GATE-1c (no fix round)

**A `--select` that matches nothing now passes when a `--require-test` is given.** The check reads
`if not selected["tests"] and not required`, so any required contract turns an empty selection into
`selected=0 … VERDICT: PASS`. Before GATE-1b this failed with "selection contains no tests".
- **The orchestrator reproduced it** with the package's own fixture: `--select '^no_such_test$' --require-test pass`
  gives rc 0, while the same select alone fails (rc 5).
- **No current caller is affected:** only the WSL app lane passes required contracts, and its select is a fixed
  pattern.
- **But it is a way to end green for a wrong reason.** That is GATE-1c's theme, so it becomes GATE-1c item 3: fail
  when the selection matched nothing *before* the required contracts were taken out.

## The `lint` failure

The executor was right that the duplicate attribute was old (`f11b9789`, 2026-10-18) and outside its scope, and it
asked before touching it. Its explanation, that a rust-cache false negative let `e9be274b` pass, is wrong. The two job
logs show the cause:
- `e9be274b`'s `lint` ran `rustc 1.99.1`;
- `5209c496`'s ran `rustc 1.100.0` (released 2026-11-09, on the runners since 2026-11-12), whose clippy reports
  `duplicated_attributes` here.

CI installs a floating `stable`, so every branch and every landing would have failed `gate` from that point.
- **The fix:** the orchestrator removed the duplicate line (`75a3c779`), an obvious bug fix in the Rust reference,
  which gets no new features but still has to pass `lint`.
- **The same toolchain adds two new cargo warnings**, `unused dependency anno-sim` in `crates/net` and `crates/ui`. They
  are not fatal under `-D warnings`, so nothing changes now.
- **A later toolchain can do this again.** If it recurs, pin the toolchain in the workflow; the Rust crates go away at
  parity in any case.

## Game-file run

At `75a3c779`, the run worktree on this PC. The C++ sources and CMake files are the same as `5e8a15a0`'s, so the
MSVC build was a no-op. The canonical call ran with the game files, `ANNO_GPU_ADAPTER=4090` and 16 jobs, in 10.5 min.

- **2,563 tests selected and 2,535 executed.** 2,533 passed and 2 were refused for memory (`low-memory stop`): the
  scene-digest render and the scenario corpus run.
- **Skipped:** 20 opt-in, 3 platform, 3 other. They are the same skips as at `c75cddff`; the two extra selected tests
  are WILDLIFE-RE-1d's.
- **The rerun:** the two refused tests, alone with `--jobs 1`, both passed.
- **Logs:** `cache/orch-gate-1b/canonical-gate.log` (sha256 `d43f3667…`) and `rerun-refused.log` (sha256 `4d8c6734…`).

**All 2,535 executed tests pass.**

## Disposition

Lands with `75a3c779`, this review and the GATE-1c release, with the owner's OK. Under the owner's standing go for the
Windows lane, [GATE-1c](../tasks/gate-1c.md) is released on top of this candidate (it needs the lint fix to get a
green CI).
