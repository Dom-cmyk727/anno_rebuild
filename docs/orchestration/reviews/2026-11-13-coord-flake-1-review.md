# COORD-FLAKE-1 review, round 1 (2026-11-13)

**Verdict: FIX ROUND 1.** The macOS `F3 (D1, deaf)` crash has the right proximate cause and a sound fix. But one
of the two failed runs proves a second, hidden cause, which the fix turns into a visible failure, not a pass. The
packet's mutation proof is missing, and an older source comment still names a disproven cause. C3: no guess-fix,
agreed; the test needs to keep the evidence next time.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed heads:** `task/COORD-FLAKE-1`:
- `a7dedc90`: `src/test_support/tests/child_process.hpp`, `Child::send()`;
- `1a5d0c15`: its REPORTS entry.

Base `task/GATE-1c` `9a958dc1`.

## What the orchestrator checked

- **The code.**
  - `send()` skips the write when `running()` says the child has exited, and sets `SIGPIPE` to ignored once, before
    its first write.
  - `running()` reaps the child. `Child` has no exit-status getter, and `stop()` honours `reaped_`, so reaping
    early loses nothing.
  - 11 `send("q…")` call sites in `src/test_support/tests` share the helper.
- **The mechanism, from the code.**
  - In `budget_probe.cpp` `reqwait`, a refused reservation prints `RESULT` and returns at once; only an admitted
    one waits for `q`.
  - The deaf case then calls `client.send("q\n")` (`budget_f3_test.cpp:550`). If the client has already exited,
    nothing reads its stdin pipe any more.
    - The test process left `SIGPIPE` at its default, so that write killed it.
    - The pipe has no other reader. The parent closes its copy of the read end after the spawn, and with
      `ANNO_TEST_BUDGET_SPAWN=0` the client starts no coordinator that could inherit it.
- **The evidence, from both artifacts** (`test-evidence-macos-clang`). Test #76 ends `SIGPIPE***Exception` with no
  doctest output, and the runner classifies it `INCOMPLETE`:

  | Run | Branch | Ends after |
  |---|---|---:|
  | 43123068877 | `main` (`7babb1f2`) | 2.23 s |
  | 43082536481 | `task/W4-ui-4` (`481a8b6a`) | **0.62 s** |

  Both commits have the base's test and probe code.
- **CI:** green on the first try for `a7dedc90` (run 43341713953) and `1a5d0c15` (run 43345320469), `gate` and the
  macOS jobs included.

## Findings

- **F1: run 43082536481 ended after 0.62 s, so its client was refused early, for another reason.**
  - **Why 0.62 s matters.** The case gives the client a 2 s admission bound (`client_vars(…, "2")`). After that
    bound, the client sends its cancel and waits one more liveness period (300 ms). That path ends at about 2.3 s,
    as in run 43123068877. A `RESULT` within about half a second must come from a path that refuses before the
    deadline.
  - **Where those paths are.** In `coord.cpp` `transact()`, two paths refuse early:
    - `Got::Timeout` with `silent_too_long()`: no frame for more than `ANNO_TEST_BUDGET_LIVENESS_MS=300`, while the
      deaf listener sends a keepalive every 100 ms. A stall of about 300 ms of either process on a hosted runner is
      enough. The result is "coordinator not responding".
    - `Got::Dead`: the deaf process died. The result is "coordinator lost".
  - **What the SIGPIPE hid.** Either reason passes `REQUIRE(RESULT 0)` and fails `CHECK(has(result, "wait
    deadline"))`. The `send` that followed killed the process before doctest printed that failure.
  - **What remains after `a7dedc90`.** Such a run now fails visibly. The flake is not closed.
  - **Asked:**
    - name which path it was;
    - reproduce it under a stated condition (for example, suspend the deaf listener with `Child::suspend()` for
      about 400 ms once the client printed `WAITING`; that should give `RESULT 0` well before 2 s);
    - fix the test's logic so a scheduling stall cannot end the case before it exercises its claim ("wait deadline,
      no server reply"). Any larger liveness bound needs its margin measured or argued in the report; the F2 file
      uses 1,000 ms.
- **F2: the mutation proof is missing (packet, Do item 4).**
  - **Why it can be done here.** The report says the crash could not be reproduced locally because it is
    POSIX-only. But WSL is POSIX: on Linux, a write to a pipe without a reader raises `SIGPIPE` just as on macOS. A
    deterministic case is cheap.
  - **Asked:**
    1. Add a regression case in `src/test_support/tests`. It starts a child that exits at once: `anno_budget_probe`
       without a mode returns 2. It waits for end of output (`wait_eof`), then calls `send("q\n")`, and the case
       must still be running afterwards.
    2. Run it in WSL with the two new parts of `send()` reverted, and show that the test process dies of `SIGPIPE`.
       With the fix in place it passes.
    3. Extend the comment by one sentence: `SIG_IGN` is process-wide and inherited by every child spawned after the
       first `send()`. The project's children already ignore `SIGPIPE` (`startup_sockets()`), so nothing changes for
       them.
- **F3: the 2026-11-10 comment in `budget_probe.cpp` (`deaf_listener`, `ee504b75`) names a disproven cause.**
  - **What it says.** It attributes run 42820583088's `SIGPIPE at test #76` to the listener process.
  - **Why that is wrong.** ctest reports the signal of the test process it started, not of that process's
    children. That run's 2.54 s fits the send-after-exit path, and the crash came back twice after that fix.
  - **Asked:**
    - correct the comment, keeping the listener's `startup_sockets()` call as hardening;
    - count three occurrences in the report: 42820583088, before `ee504b75`, then the two above.

## C3, the executor's QUESTION

No guess-fix: agreed. The evidence gap is in the test itself.
- **What the failure hides.** `REQUIRE(has(holder.line(10000ms), "READY 1"))` prints only `false`, never the line
  the holder sent. The holder's stdout and stderr share one pipe.
- **What a fast non-READY first line can be.** Each names a different cause:
  - `READY 0 reason=<r>`: a refusal;
  - `EXCEPTION <what>`: the library threw (exit 3);
  - nothing: the holder crashed or closed its output.

**Asked:**
- in `budget_f2_test.cpp`, keep the line and `INFO` it at each check of this form: 8 `REQUIRE(has(<child>.line(…),
  …))` sites, plus the `WAITING` ones if they differ;
- no behaviour change;
- C3 stays open, documented in the report, and is reopened when it recurs with that output.

## Disposition

Fix round 1 to the Windows executor, by message, same branch. GATE-1c's landing is independent of this package.
