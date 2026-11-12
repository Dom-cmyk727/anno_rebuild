# GATE-1a review (2026-11-12)

**Verdict: ACCEPTED.** The WSL sanitizer gate now builds at `-O1 -g`, and a kept build tree can no longer keep the old
flags. One full gate proves it: **1 h 32 min from configure to the last lane, against 4.3 to 8 hours before**, with
every common test's verdict unchanged and no sanitizer finding. It is also the first full gate on record to end
green. No fix round.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/GATE-1a` `b0e9428e`, three commits on `main` `cc70ba9a`: `tools/test/wsl_gate.sh` (+24/−4),
`CMakePresets.json` (the `asan` preset), the REPORTS entry and a CI addendum. Merged with `main` `0ac461d8` as
`9df05d9a`; the one conflict was an append in `REPORTS.md`, both entries kept.

## What the orchestrator checked

- **The change.** `wsl_gate.sh` passes `-DCMAKE_CXX_FLAGS_DEBUG="-O1 -g"` and keeps `CMAKE_BUILD_TYPE=Debug`. Before the
  cache-exists shortcut it reads the cached `CMAKE_CXX_FLAGS_DEBUG` and wipes and reconfigures on a mismatch, like the
  stale-source check above it, inside the build folder the script already confines to `$HOME/anno-build`. It writes
  the flags it built with into the provenance file. The `asan` preset gets the same cache variable; the `msvc` preset is
  unchanged, and `.github/` is not touched.
- **The proof gate, from its own files** (`~/anno-gate-logs/anno1503-GATE-1a-gatecheck/` in Ubuntu):
  - built at `3170ab35` in a separate checkout; the provenance reads `-O1 -g` back from the cache;
  - configure 11:56, build 437 s, the last lane done 13:28;
  - every lane `VERDICT: PASS`, `sanitizer_lines=0`, no required-asset skips (the game files were present);

  | Lane | Selected / executed / failed / skipped | Seconds now | Seconds at `w4ui2parte` (`-O0`) |
  |---|---|---:|---:|
  | app | 314 / 312 / 0 / 2 opt-in | 2,156 | 8,410 |
  | sim | 1,609 / 1,591 / 0 / 18 opt-in | 1,976 | 4,856 |
  | formats | 97 / 97 / 0 / 0 | 225 | 1,133 |
  | render | 185 / 185 / 0 / 0 | 505 | 4,980 |
  | test support | 93 / 89 / 0 / 4 opt-in | 96 | 247 |
  | **Total** | | **4,958** | **19,626** |

  Lane times add up to the reported 4.0×. About 190 tests were added since the baseline, so the like-for-like factor
  is higher; the two canaries the analysis named, run alone, are 5.46× and 5.50×.
- **Verdicts by name** (the executor's comparison, not repeated here): the same as `w4ui2parte` for every common test,
  except `a flow build that fails leaves the legacy look, final, built once (game files)`, which passed this time. That
  is an observation, not a fix: GATE-1c still has to find its cause.
- **Hosted CI** green on all three heads (runs 43204641640, 43218940794 attempt 2, 43229138004). Attempt 1 of the
  second failed only `cpp (windows-msvc)` on `anno_test_support_tests.C3: a replacement coordinator does not admit
  against its predecessor's promises`, confirmed from the job log; a rerun of that job alone passed.
- **No game-file run is needed for this landing.** The MSVC build and its test set are unchanged (the `msvc` preset and
  every source are untouched, and `wsl_gate.sh` is not part of that run), so its result is the one recorded for
  `c75cddff`.

## Notes (no fix round)

- **Hosted CI gets no faster.** `linux-clang-asan` now spends 26 m 46 s building (was 14 m 01 s) and 3 m 52 s testing
  (was 15 m 12 s), so the job takes about as long as before. The gain is local: the WSL gate, where a build is reused
  between runs, and the Mac.
- **The test-coordinator flakes now hit two tests on two hosts:** `F3 (D1, deaf)` on macOS (five times on 2026-11-11
  and 12) and `C3` on Windows. Each costs a rerun and a check per landing. The orchestrator moves COORD-F3-1 up the queue
  and widens it to both tests.
- **One comment is slightly off:** the script calls the old cached flags "empty", but GCC's default is `-g`. The
  comparison wipes the cache either way. No change needed.

## Disposition

Lands as `9df05d9a` plus this review, with the owner's OK. Next on the Windows lane: GATE-1b (a required contract runs
once; no local lavapipe lane by default), released only with the owner's go.
