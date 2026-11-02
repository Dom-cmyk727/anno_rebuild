# W4-WIN-VERIFY: commands, identities and the acceptance matrix

Text-only. `INDEX.md` lists every extract with the sha256 of its git-ignored original under `cache/` of the Windows checkout.

## Identities

- Branch `task/W4-3-live-buildings`. Published parent: `dad27da7ba0ee26abdd3b1de61adaddbfba05ec7` (code tree `8367dc0`, tools `37da91b`).
- Correction commits: `beb36d89b607dc835cf01406fe9189f4c7721ebb` (V1, V2) and `f047165c67bc26c1498c5201c014db83553c6f6f` (review fixes). **Both focused gates and the canary ran
  at `f047165`, clean tree** (`dirty 0` in `gate-native-identities.txt`, `gate-wsl-identities.txt`, `canary-rtx-identity.txt`). Commits after `f047165` change only `docs/`.
- Workflow: the canonical runner of `E:\anno1503` at `b57c6f0d0c747ecc41755b438d9377df38d2b7ac`, clean. `git diff b57c6f0 origin/main` is empty for `tools/test`, `cmake`, `CMakeLists.txt`
  (`origin/main` is `92d78ae`). Runner file hashes, native and WSL identical: `run_selected.py` `f87fc882…`, `ctest_verdict.py` `b10bfc37…`, `ctest_verdict_test.py` `7fc05a93…`,
  `CheckTestNames.cmake` `6b27d264…`.
- Windows build `build/win-a29b079` (Release, MSVC): `anno1503.exe` `cc2a388e…`, `anno_app_input_tests.exe` `297d88f4…`. WSL build `~/anno-build/w4-asan-viewer`
  (g++-13, Debug, ASan+UBSan, viewer and GPU library on, `SDL_UNIX_CONSOLE_BUILD=ON`): `anno1503` `bd41fe09…`, `anno_app_input_tests` `90ffa207…`.
- Tools: Python 3.14 (the interpreter in the build's CMake cache; the `python3` on PATH here is the Microsoft Store alias without Pillow/tkinter).

## The two findings and their reproduction

| Probe (public wrapper, fixture child exits 0, complete record) | `dad27da` tools | corrected tools |
|---|---|---|
| clean complete record | PASS | PASS |
| stderr only: UBSan `runtime error:` | PASS | FAIL (sanitizer diagnostics, file:line printed) |
| stderr only: ASan report | PASS | FAIL |
| stdout only: UBSan line | PASS | FAIL |
| invoked `look 1 999`, log of `look 1 2` | PASS | FAIL (identity mismatch + record mismatch) |
| invoked `wait frames 3`, log of `wait frames 2` | PASS | FAIL (identity mismatch only: a wait leaves no ran record) |

(`repro-before-dad27da-tools.txt`, `repro-after-corrected-tools.txt`, `repro-harness.py`.)

## What the correction is

- **V1.** `tools/test/viewer_wiring.py` keeps the window's stdout and stderr (`<out>.stdout.txt`, `<out>.stderr.txt`), scans both with `ctest_verdict.SANITIZER` (the one
  definition; no second pattern), prints every hit as `channel path:line: excerpt` (a long line is cut around the match) after echoing the session log, and fails although the exit
  status is 0. The scan comes before the watchdog, exit-status, expected-stderr and session checks. The child also gets `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` unless the caller
  or `--env` set it (defence in depth). The expected `CPU frame delivery failed (SDL_…)` lines are not sanitizer lines and are still required by `--expect-stderr`.
- **V2.** `DevScript::parse` records `script identity: steps=N fnv1a64=HEX` as the first `script.log` line: FNV-1a 64 over `LINE VERB ARG...\n` for every parsed step (waits and every
  argument; comments and whitespace gone). `ran line N: VERB ARG...` now carries all arguments. `viewer_session.py` recomputes the identity from the invoked script's bytes (LF lines,
  `#` comments, ASCII-whitespace words, as the parser reads them) and requires it to be the one identity line that opens the record, then compares the ordered full-argument trace.
  The log is read byte for byte. A log without the identity line is LEGACY: the wrapper refuses it; `--revalidate` prints `LEGACY` (with an AMBIGUOUS marker when several scripts fit) and
  says only the first-argument trace was checked. Accidental-mismatch detection only: a window that forged a matching log would pass.

## Focused checks

| Check | Result |
|---|---|
| `viewer_wiring_contract_test.py` (public CLI, fixture child) | 41 tests, OK (was 14); `window_tools_contract_test.py` unchanged, 19 OK |
| DevScript suite, native (`anno_app_input_tests -ts=dev_script`) | 9 cases, 297 assertions, 0 failed (includes the new identity case) |
| Python mutants on scratch copies (`mutation-python-summary.txt`) | control OK; 23 mutants each killed by the intended test(s): no scan, stdout not scanned, stdout discarded, private pattern, scan skipped beside `--expect-stderr`, no UBSAN default, default overriding the caller, identity comparison removed, identity from first arguments only, waits dropped, identity not required, legacy accepted by the wrapper, legacy reported VALID, ran trace first-argument-only, ran compare first-argument-only, log read with splitlines, long line cut before the match, log echo removed, identity line allowed anywhere, ambiguous tag dropped, only the first match printed, CR not stripped, FAIL lines dropped on the missing-identity path, log decoded with replace |
| C++ producer mutants, in place with `git checkout` restore (`mutation-cpp-summary.txt`) | control 9/9; identity not pushed, waits dropped, later arguments dropped, line numbers dropped, ran records first-argument-only: each fails the new doctest case at the intended assertions; tree clean and rebuilt after |
| Cross-language vectors | the same five scripts and constants are asserted by the C++ case and by `test_the_python_identity_agrees_with_the_cpp_vectors`; each constant derived apart from the code (FNV-1a vector `"a"` = `af63dc4c8601ec8c` also checked) |

## Gates (canonical runner; selected / executed passes / failed / empty / skipped / incomplete)

Selection: `^anno_app_input_tests\.` (the affected input/DevScript tests) plus `viewer_wiring_contract`, `viewer_wiring_r1_held_publication`, `viewer_wiring_r4_occupancy`,
`viewer_wiring_cpu_delivery_failures`, each also `--require-test`. Admission control on; the wrapper's session directories were kept (`TEMP`/`TMPDIR` under the gate folder).

| Gate | Result |
|---|---|
| Native MSVC (`gate-native-verdict.txt`) | 35 / 35 / 0 / 0 / 0 / 0, `sanitizer_lines=0`, PASS (1.4 minutes) |
| WSL viewer-enabled ASan+UBSan (`gate-wsl-verdict.txt`) | 35 / 35 / 0 / 0 / 0 / 0, `sanitizer_lines=0`, PASS; the three viewer cases ran against the instrumented `anno1503` |
| WSL warnings (`gate-wsl-warnings.txt`) | GCC 13.3 and Clang 18.1.3 `-O3 -Werror`, viewer and GPU on, and without GPU: clean |
| RTX 4090 real window, `boundary_exit_in_hold --gpu --max-seconds 15` | PASS in 3.9 s through the corrected wrapper: identity line recorded by the window (`steps=11`), trace validated, stdout empty, stderr 236 bytes (adapter line `NVIDIA GeForce RTX 4090 (Vulkan)`) with no sanitizer line (`canary-rtx-*`) |

Each required viewer case appears twice in the retained session folders: the runner runs a required contract once before the main selection and again in it (the counts above are the
main selection's).

**Stream reconciliation** (`stream-reconciliation.txt`, from the retained `<out>.stderr/stdout.txt`, not from the gate summary): in all six real-window sessions per lane (three cases, each run twice) the
sanitizer-pattern count is 0 on both channels; `cpu_delivery_failures` has exactly 3 `CPU frame delivery failed` lines per session (update, submit, present: the expected
diagnostics) in both lanes; every `script.log` opens with the identity line (`cpu_delivery_failures` `3c8e1149…`, 56 steps; `r1` `7b7afefb…`, 56; `r4` `b8d09275…`, 64) and validates
against its script (`revalidation-of-new-traces.txt`). The WSL stderr files are longer (frame-hitch and picture notices at ASan speed) and hold no sanitizer text. This is the first
evidence in which child streams are part of the gate's verdict; the earlier zero-file scan of `/tmp/viewer_wiring_*` (W4-WIN-FINAL) found no files and **provides no evidence**; the
three reruns with kept stderr remain the earlier evidence.

## Retained results, not repeated

The two full-corpus cases (5 303.65 s and 2 374.47 s under the 8 000 s sanitizer timeout), the viewer-enabled ordinary lane (217/215), the native MSVC lane (220/218) and the 106-case RTX/Radeon
gates are W4-WIN-FINAL results at `dad27da`/`37da91b`, reused. Reason: this package changes `viewer_wiring.py`, `viewer_session.py`, the contract test, `DevScript::parse`/the `ran` record text
(`src/app/src/dev_script.cpp`, `dev_script.hpp`) and its test; no `render`, `sim`, `formats`, `assets` or `anno_app_core` source, no timeout or heavy classification, and no corpus scenario changed
(`git diff dad27da..HEAD --stat`). The corpus cases run no DevScript.

## Retained W4-WIN-FINAL traces

`revalidation-of-w4-win-final-traces.txt`: the three committed raw traces (stages canary, exit-in-hold canary, CPU delivery) print `LEGACY`, exit 0, with the statement that only the ordered
first-argument trace was checked. They are not upgraded to a verified full identity and their historical results are kept with their existing pins.

## WSL workflow identity (correction, no old work rerun)

The W4-WIN-FINAL WSL logs' `workflow` block shows the task head because the gate script exported `GIT_DIR`/`GIT_WORK_TREE` for the task worktree (needed because a worktree's `.git` file
holds a Windows path that WSL cannot follow), which also redirected the runner's `git -C /mnt/e/anno1503`. Recovered: the runner checkout `E:\anno1503` = `/mnt/e/anno1503` was at `b57c6f0` from
`2026-11-01 20:27 +0100` (its reflog) through every W4-WIN-FINAL run on 2026-11-02, clean on Windows (`dirty=0`); `tools/test` there is identical to `origin/main` `92d78ae`; the runner files' hashes above
are those of that checkout. In the new WSL run the overrides are applied to the source only (`env GIT_DIR=… GIT_WORK_TREE=…` for the source's Git view and the runner process) and the workflow's
revision, status and file hashes are recorded beforehand with the overrides cleared (`gate-wsl-identities.txt`). The shared runner was not changed, so its own log block for the WSL run still shows
the source head as `workflow` (`"workflow": {"root": "/mnt/e/anno1503", "head": "f047165…"}`); read the workflow identity from `gate-wsl-identities.txt`. Seen from WSL the workflow checkout lists 523
`dirty` paths because WSL's Git has no `core.autocrlf`; Windows lists none.

## Limits

Headless CPU/SDL-dummy sessions say nothing about the GPU path rendering. The canary is one short real-window session on the RTX 4090; no Radeon window, native 4K, scale, latency or broad performance run
was assigned. The sanitizer scan sees what the window writes to stdout and stderr; reports sent elsewhere (`log_path=`) are not seen (nothing in the repository sets it). UBSan stays recoverable
in the app targets; the fatal option is defence in depth. The identity records the window's own parse: a child that forges a log passes (accidental-mismatch detection only). The historical
reclaimer occurrence stays an uncertainty; the submit-only and single-foreign-scene mutants were not added (not assigned).
