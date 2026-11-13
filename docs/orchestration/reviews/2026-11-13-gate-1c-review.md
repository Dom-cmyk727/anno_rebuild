# GATE-1c review (2026-11-13)

**Verdict: ACCEPTED after one fix round.** The three ways a gate could end green, or red, for the wrong reason are
closed:
- The flow-build test's flake has a named cause and a fix that checks only what the test claims.
- A missing *named* GPU adapter is a classified skip, while "no GPU at all" still fails under `ANNO_REQUIRE_GPU`.
- A `--select` that matches nothing fails again.

Round 1 found two weak checks and one wrong report line. All three are fixed.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed heads:** `task/GATE-1c`:
- `3b311a8e`: the code of the first handover;
- `9e73f27b`: a merge of `main` `96f7980c`;
- `afcdc8ba`: its REPORTS entry;
- `d06ccc92`: the fix round's code;
- `1eb301bb` and `9a958dc1`: its addendum.

Changed files: `src/render/tests/river_flow_corpus_test.cpp`, `src/render/tests/play_gpu_test.cpp`,
`tools/test/run_selected.py`, `tools/test/workflow_test.py`, `tools/test/wsl_gate.sh` and REPORTS. Merged without
conflicts onto the board branch as `a6e0141d` (branch `orch/land-gate-1c`).

## What the orchestrator checked

- **Item 1, the flow-build test (`a flow build that fails leaves the legacy look, final, built once`).**
  - **The cause matches the code.** The old check summed `update_chunks`'s `built` over 60 frames. That count also
    includes the margin and neighbour-group chunks (`plan_margin`), which the settle loop above it never waited
    for. Under `-O0` or load, one of them could still arrive inside the window.
  - **The fix checks only the test's own claim:** no visible chunk is rebuilt. No sleep or frame count was raised.
- **Item 2, the named-adapter GPU cases.** A host with a GPU under another name (WSL lavapipe, CI) now skips them as
  `gpu`.
- **Item 3, the empty selection.** The orchestrator's repro (`--select '^no_such_test$'`, with and without
  `--require-test pass`) now ends rc 5 both ways, with "selection contains no tests". A selection of only required
  contracts still passes.
- **`python -I tools/test/workflow_test.py`:** 24 tests, `OK`, on Windows.
- **The WSL gate with `--gpu-lane`**, from its own files (`~/anno-gate-logs/anno1503-GATE-1c-gatecheck/`):
  - built at `3b311a8e`, in a separate checkout, at `-O1 -g` (372 s);
  - every lane ends `VERDICT: PASS` with `sanitizer_lines=0`;
  - the GPU lane skipped exactly the two named-adapter cases, both as `gpu`;
  - the later fix round changes two test bodies only and was rechecked by the executor in the same build (below).

  | Lane | Selected / executed / failed / skipped | Seconds |
  |---|---|---:|
  | app | 310 / 308 / 0 / 2 opt-in | 1,599 |
  | sim | 1,609 / 1,591 / 0 / 18 opt-in | 1,901 |
  | formats | 97 / 97 / 0 / 0 | 169 |
  | render | 185 / 185 / 0 / 0 | 424 |
  | test support | 93 / 89 / 0 / 4 opt-in | 38 |
  | GPU (lavapipe, `ANNO_REQUIRE_GPU=1`) | 116 / 114 / 0 / 2 gpu | 1,898 |

- **Hosted CI** green on every head, `gate` included:
  - `3b311a8e`, `9e73f27b`, `afcdc8ba`;
  - `1eb301bb` (with `d06ccc92`): run 43330747049, every job on the first try, the macOS jobs among them;
  - `9a958dc1` (REPORTS only) and the landing candidate: `land.py` waits for their `gate`.

## Fix round 1 (sent 2026-11-12, fixed 2026-11-13)

- **F1: the first fix compared chunk addresses.** A rebuild frees the replaced chunk (`request_chunks` → `discard`),
  and the next `make_shared` can reuse its address, so two rebuilds could look like none.
  - **Fixed:** the check compares `Chunk::serial`, which is process-unique.
  - **Proof:**
    - 50 more `-O0` Debug runs clean, 130 in all, under the condition that failed 3 of 40 before;
    - a real replacement of one visible chunk inside the window (`insert_chunk`) fails the check (`67 == 8`).
- **F2: the first fix dropped the "no GPU at all" failure.** The packet asked to keep it.
  - **Fixed:** under `ANNO_REQUIRE_GPU`, an empty adapter list fails; only a list without the named adapter skips.
  - **Proof:**
    - the 4090 and the Radeon still pass (MSVC Release);
    - WSL lavapipe still skips as `gpu` under `ANNO_REQUIRE_GPU=1`;
    - **no GPU at all, live** (`9a958dc1`; the orchestrator suggested the method):
      - in the WSL `-O1` ASan build at `d06ccc92`, the cases ran inside a private user and mount namespace
        (`unshare -r -m`), with `/dev/null` bind-mounted over the Vulkan, EGL and GL loader libraries;
      - `list_adapters()` came back empty there;
      - with `ANNO_REQUIRE_GPU=1` both cases failed with "no GPU device at all"; without it both skipped;
      - the mounts ended with the process.
- **F3: one REPORTS line claimed a change to an existing workflow test.** That assertion is GATE-1b's own
  (`e9be274b`). Corrected.
- **The executor's own rechecks:**
  - the flow-build case and both `play_gpu_test` cases in the WSL `-O1` ASan build, `sanitizer_lines` 0;
  - `workflow_test.py` 24/24;
  - the render suite with game files on MSVC Release.

## Game-file run

At the candidate `a6e0141d`, in the run worktree on this PC. The MSVC build recompiled the two changed test files
only. The canonical call ran with the game files, `ANNO_GPU_ADAPTER=4090` and 16 jobs, in 13 min.

- **2,563 tests selected, 2,537 executed, 0 failed.** No memory refusals this time, so nothing needed a rerun.
- **Skipped:** 20 opt-in, 3 platform, 3 other. These are the same skips as GATE-1b's run, so no GPU case skipped.
- **The three changed cases passed:**
  - the flow-build case (4.6 s);
  - `play: GPU matches the CPU path on one scene` on the 4090 (7.5 s);
  - the same on the Radeon iGPU (6.4 s).
- **Log:** `cache/orch-gate-1c/canonical-gate.log` (sha256 `4592a2a9…`).

**All 2,537 executed tests pass.** Every later commit on the candidate changes only docs, so this run stands for it.

## Disposition

Lands as `orch/land-gate-1c` with the owner's OK. That branch holds the board with this review and the COORD-FLAKE-1
release, plus a `--no-ff` merge of `task/GATE-1c` `9a958dc1`; its code is the game-file-tested `a6e0141d`'s.

Under the owner's standing go for the Windows lane, [COORD-FLAKE-1](../tasks/coord-flake-1.md) is released, based on
`task/GATE-1c` `9a958dc1`.
