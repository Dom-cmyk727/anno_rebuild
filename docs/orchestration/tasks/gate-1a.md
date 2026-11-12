# GATE-1a: the sanitizer gate at `-O1`

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, with the owner's go, to **the Windows executor**.
Step 1 of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` at this packet's release commit (state 138). Branch `task/GATE-1a`. About 2–3 hours, most
of it machine time.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover), the gate below, one REPORTS entry of about 60 lines, READY FOR REVIEW.

## Why

The [project analysis](../reviews/2026-11-12-project-analysis.md) §2.1 (finding D1): `tools/test/wsl_gate.sh`
builds the ASan/UBSan tree as `CMAKE_BUILD_TYPE=Debug` with no optimisation flag, and a full gate takes 4.3 to 8
hours. The same sim tests under the same sanitizers ran 5.5 to 5.9 times faster at `-O1 -g`, with the same results.

## Do

1. **The WSL gate.** In `tools/test/wsl_gate.sh`, the `cmake -S "$W" -B "$B" ...` call (about line 220), add
   `-DCMAKE_CXX_FLAGS_DEBUG="-O1 -g"`. Keep `CMAKE_BUILD_TYPE=Debug`, so assertions stay on. Leave the C flags as
   they are.
2. **A cached build tree must not keep the old flags.** The script skips configuring when `$B/CMakeCache.txt`
   exists (`--keep-build`, the shared build folder). Make it read the cached `CMAKE_CXX_FLAGS_DEBUG` and, when it
   differs from the wanted value, wipe and reconfigure, the same way the stale-source check just above does. Write
   the flags it built with into the gate's provenance file.
3. **The `asan` preset.** In `CMakePresets.json`, give `asan` the cache variable `CMAKE_CXX_FLAGS_DEBUG` =
   `-O1 -g`. Hosted CI's `linux-clang-asan` job builds this preset, so it speeds up too. Do not edit `.github/`.
4. **Check before relying on it:**
   - the deadlines scaled for `ANNO_SANITIZE` builds still hold (faster is fine; say if any test now depends on
     being slow);
   - no new `-Werror` warning at `-O1` with GCC 13 (the gate's own build) or with Clang (hosted CI).

## Proof

- **One full WSL gate after the change**, at your pushed head, in a separate clean checkout of it (never your live
  worktree; the script's usage says why). Default lanes, no `--gpu-lane`.
- **Compare it with the last recorded full gate,** `w4ui2parte` (2026-11-08, logs in Ubuntu under
  `~/anno-gate-logs/`), instead of running a slow "before" gate:
  - every lane's seconds and the total, side by side;
  - test by test, by name: every test in both runs has the same verdict. Tests new since then (MAP-INTEGRATE-2 and
    later) are listed apart with their own verdicts;
  - `sanitizer_lines` 0.
- **Known failures may stay red:** `a flow build that fails leaves the legacy look, final, built once (game files)`
  and, if they run, the two `play: GPU matches the CPU path` cases. GATE-1c fixes them. Any other failure: stop and
  report it with its test name and assertion.
- **Hosted CI green** on the head, `gate` included. Report `linux-clang-asan`'s duration against its run on `main`
  `fa9d8bbf`.

**Not yours:** the whole MSVC game-file run (the orchestrator, at the merge), the Mac presets (a later step), and
GATE-1b/1c's changes.

## Report

READY FOR REVIEW, about 60 lines: what changed, the gate's lane table (before / after), the verdict comparison, the
known failures, CI, NOT_RUN, Decisions and reasoning, QUESTIONs.
