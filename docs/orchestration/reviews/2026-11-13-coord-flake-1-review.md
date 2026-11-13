# COORD-FLAKE-1 review (2026-11-13)

**Verdict: ACCEPTED after one fix round,** with three comment corrections as the branch's closing commit (Round 2,
below). Both causes of the macOS `F3 (D1, deaf)` failures are named, reproduced and fixed:
- the test process wrote `q` to a client that had already exited, and died of `SIGPIPE`;
- that crash hid a second cause: a scheduling stall over 300 ms refused the client early.

`C3` (Windows) has no provable cause. It is not guess-fixed; its checks now print the line the child actually sent,
so its next occurrence names the cause.

## Round 1 (sent 2026-11-13)

**Verdict then: FIX ROUND 1.** The macOS `F3 (D1, deaf)` crash had the right proximate cause and a sound fix. But
one of the two failed runs proved a second, hidden cause, which the fix turned into a visible failure, not a pass.
The packet's mutation proof was missing, and an older source comment still named a disproven cause. C3: no
guess-fix, agreed; the test needed to keep the evidence next time.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed heads:** `task/COORD-FLAKE-1`:
- `a7dedc90`: `src/test_support/tests/child_process.hpp`, `Child::send()`;
- `1a5d0c15`: its REPORTS entry.

Base `task/GATE-1c` `9a958dc1`.

### What the orchestrator checked

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

### Findings

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

### C3, the executor's QUESTION

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

### Disposition

Fix round 1 to the Windows executor, by message, same branch. GATE-1c's landing is independent of this package.

## Round 2 (reported 2026-11-13)

**Reviewed heads:** `task/COORD-FLAKE-1`:
- `a0997aba`: the code for F1, F2, F3 and C3's output;
- `7faf1af4`: its REPORTS addendum;
- `38fc4abc`: one racy check dropped from the new case;
- `0a94c338`: its follow-up note.

Changed files: `budget_f2_test.cpp`, `budget_f3_test.cpp`, `budget_probe.cpp` (a comment), `child_process.hpp` (a
comment) and REPORTS. Merged without conflicts onto `orch/land-gate-1c` `ff122e7e` as `429c0b3a` (branch
`orch/land-coord-flake-1`).

### What the orchestrator checked

- **F1, the early refusal.**
  - **The fix:** the deaf case's `ANNO_TEST_BUDGET_LIVENESS_MS` goes from 300 to 1,000 ms.
  - **The cause, reproduced by the executor.** Suspending the deaf listener for 400 ms once the client has started
    gives `RESULT 0 reason=coordinator not responding`. That is `coord.cpp` `transact()`: `Got::Timeout` with
    `silent_too_long()`.
    - After the fix: 20 of 20 runs pass under the same stall.
    - A 1,200 ms stall still fails with that reason, so the liveness check still works.
  - **The margin.** Run 43082536481's whole case lasted 0.62 s, so the stall that refused it was shorter than that.
    1,000 ms is above it and below the case's 2 s admission bound. It is also the value `budget_f2_test.cpp`'s
    `Scope` already uses.
  - **The case now tests its claim.** On macOS at `0a94c338` (run 43359849075) it passed in 3.33 s. That is the
    deadline path: the 2 s bound, one 1 s liveness period, then setup. An early refusal would end before 2 s.
  - **A correction to round 1.** Round 1 said run 43123068877's 2.23 s fitted the deadline path. It does not:
    - `transact()` sends the cancel at the 2 s deadline and gives up one liveness period later
      (`cancel_deadline = now + liveness()`);
    - so that path cannot end before 2.3 s after the request, and the case's 2.23 s also includes its setup.

    Both runs were early refusals that the crash hid, as the executor's comment says ("in effect 43123068877
    too").
- **F2, the regression case** `Child::send degrades silently once the child has exited`.
  - **What it does:** it starts the probe without a mode, which returns 2 at once, waits for the end of its output,
    then sends `q`. The test process must survive.
  - **The mutation**, run by the executor in the WSL `-O1` ASan build. With the `running()` check and the
    `SIG_IGN` both reverted, the test process dies with rc 141 (`SIGPIPE`) and no output. That is the signature of
    the macOS `INCOMPLETE`. The orchestrator did not rerun it; the mechanism is standard POSIX behaviour, and the
    code matches it.
  - **The racy check, rightly dropped.** The first push asserted `CHECK_FALSE(child.running())` right after
    `wait_eof`, and it failed on `cpp (linux-gcc)` and `cpp (linux-gcc-release)` (run 43351375688).
    - End of output does not mean the child has been reaped yet.
    - The case claims only that `send()` does not kill the caller.
    - That failure was reported normally, so a Linux build survived a write while `running()` still said true.
  - **The comment** on the process-wide, inherited `SIG_IGN`: added.
- **F3:** the 2026-11-10 comment is corrected, and three occurrences are on record: 42820583088, 43082536481 and
  43123068877.
- **C3:** the child's line is kept and given to `INFO` at all 8 sites, so a failure prints it:
  - C1: the holder;
  - C2 (loss): the holder, the CPU holder and the waiter;
  - C2 (mute): the holder and the waiter;
  - C3: the holder and the CPU holder.

  No behaviour changes, and no unlogged `REQUIRE(has(<child>.line(…)…))` is left in the file. C3 stays open.
- **CI:** green on the first try at `38fc4abc` (run 43356495161) and `0a94c338` (run 43359849075), `gate` and the
  macOS jobs included.
- **The executor's local gates:**
  - `anno_test_support_tests` on MSVC Release: 80 selected, 77 executed, 0 failed, 3 skipped (platform);
  - the same in the WSL `-O1` ASan build: 94 selected, 90 executed, 0 failed, 4 skipped (opt-in);
  - `sanitizer_lines` 0 in both.

### Three comment corrections (the branch's closing commit)

Comments only; hosted CI is their gate.
1. **`budget_f3_test.cpp`, the deaf case:** "(reproduced below, GATE-F1-PROOF)" points to nothing in the
   repository. The 400 ms stall was a local run. Name where its result is (the REPORTS addendum), or drop the
   reference.
2. **The same comment:** "the deaf case's own ~2.3 s run time" is about 3.3 s with a 1,000 ms liveness bound (macOS:
   3.33 s).
3. **`budget_probe.cpp`, the correction in `deaf_listener`:** it places `client.send("q\n")` "in `quit_and_wait`".
   The send is in the case body, right before `quit_and_wait(deaf)`. The REPORTS entry says the same; add one
   closing line instead of editing the entry.

### Game-file run

At the candidate `429c0b3a`, in the run worktree on this PC: see the board's STATUS until it is recorded here.

### Disposition

ACCEPTED. The closing commit goes to the Windows executor with the release of its next packet. It lands with
GATE-1c on the owner's OK, as `orch/land-coord-flake-1`, which contains `orch/land-gate-1c`.
