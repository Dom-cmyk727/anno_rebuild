# Running acceptance gates

Read the [evidence contract](evidence-contract.md). Use Python 3.8+ and CMake/CTest
on PATH. On this Mac, the existing tool environment is
`cache/mac-tools/venv/bin`; no new environment or game-file installation is needed.

Cheap checks before configuring/building:

```sh
python3 -I tools/test/run_selected.py --preflight-only
python3 -I tools/test/workflow_test.py
```

Focused local gate (set `ANNO1503_GAME_DIR` when the cases need original files):

```sh
python3 -I tools/test/run_selected.py --build-dir build/dev \
  --target anno_app_tests --select '^anno_app_tests\.' \
  --jobs 8 --timeout 1800 --log cache/gates/app-01.log
```

The gate runs test-name and verdict preflights before its optional build, retains
CTest as the scheduler, and checks every selected terminal result against CTest's
JSON inventory. The existing positional interface is also supported:

```sh
bash tools/test/run_selected.sh build/dev anno_app_tests '^anno_app_tests\.' \
  1800 cache/gates/app-02.log 8
```

Use `--config Release` with the MSVC build directory. Use `--label heavy` or
`--exclude-label heavy` for the existing labels, and choose jobs using the host's
measured budget. The workflow does not change admission control or coordinators.

Require a package-specific public-command contract before the main selection:

```sh
python3 -I tools/test/run_selected.py --build-dir build/dev \
  --select '^anno_app_tests\.picture' --require-test picture_matrix_contract \
  --log cache/gates/picture-01.log
```

On UNIX, a checkout containing `src/tools/bench_picture/CMakeLists.txt` requires
`picture_matrix_contract` automatically, unless the call passes `--skip-auto-contract`
(for a gate that already ran it in an earlier call against the same build, so it is
required and run exactly once per gate, not once per lane: `tools/test/wsl_gate.sh`
requires it only in its first lane and passes `--skip-auto-contract` in every later
one). Each required contract must be registered, executed and successful, with no
skips, and runs exactly once: a test that is both `--require-test`'d and matched by
`--select` is left out of the main selection (its own required-contract run already
produced its verdict row), so its outcome does not inflate, or appear twice in, the
main selection's counts. A selection made only of such already-run required
contracts still ends with a clear PASS, not a "selection contains no tests" failure.
This main branch does not yet contain W4's benchmark: its trace/table semantic fixes
remain with that executor.

## Safe adoption by existing worktrees

Do not switch runners during an existing run. At the next checkpoint, an older
checkout can invoke the canonical script without copying files or integrating
another task branch:

```sh
python3 -I /absolute/path/to/main/tools/test/run_selected.py \
  --source-root /absolute/path/to/owned-worktree \
  --build-dir /absolute/path/to/owned-worktree/build/dev \
  --select '^anno_sim_tests\.' --jobs 8 \
  --log /absolute/path/to/owned-worktree/cache/gates/sim-01.log
```

The source must match `CMAKE_HOME_DIRECTORY` in that build's cache. The canonical
workflow and tested source identities are recorded separately. Existing
`<build-dir>.run.lock` locks are respected; another owner's lock is never removed.
Logs and inventory sidecars use new paths and are never overwritten. This changes
neither worktree's source, branch nor coordinator state. The optional `--target`
build is only for the explicitly supplied, owned build directory.

Each saved gate ends with one authoritative `GATE_END` status. A missing or
contradictory completion record fails re-reading. A test-body pass cannot erase a
later source-identity check failure or CTest failure when the log is re-read.

## Gate policies and outcomes

`--asset-policy required` is the default: an absent original-file prerequisite
fails the gate. CI and an explicitly designated no-assets run use
`--asset-policy unavailable`, with `ANNO1503_GAME_DIR` unset. Required-asset skips
are then permitted but remain skips, never corpus passes.

Platform and opt-in skips are recorded separately. An unexpected GPU skip fails
unless the gate explicitly declares `--allow-skip gpu`; unknown skip reasons fail
unless `--allow-skip other` is explicitly justified in the acceptance record.
Neither option changes a product assertion or CTest's own failure status.

The verdict prints `selected`, `executed` (executed **passes**), `failed`, `empty`,
`skipped` by class, and `incomplete`. A testcase's ordinary doctest filter exclusions
are ignored. Counts are CTest entries, not assertions or nested Python unittest
methods: a Python wrapper can run synthetic checks and skip asset subcases. Its
entry passing does not establish corpus coverage. A gate containing only allowed
skips still fails because it establishes no executed pass. Known failures remain
nonzero. For old logs, classification can be
repeated without executing tests:

```sh
python3 -I tools/test/ctest_verdict.py cache/gates/app-01.log \
  --inventory cache/gates/app-01.log.inventory.json
```

Record any allowed skip policy when re-reading. Without an inventory, a historical
log can expose unfinished started tests, but cannot prove which never-started
tests were originally intended. Do not manufacture that missing evidence.

Exit codes: 0 accepted under the declared policy; 1 test/CTest failure; 2 build
failure; 4 sanitizer; 5 workflow/preflight/infrastructure failure; 6 disallowed
prerequisite skip; 7 empty doctest child; 8 no test outcomes; 9 incomplete or
contradictory evidence. A zero selection is refused before execution as workflow
failure. Initial failures and corrected reruns stay separate.

## Workflow maintenance

The verdict reader and its initial 14 contracts were adapted from MAP commit
`8204b6b`, and the multiline name guard from W4 commit `e8af4b6`. Extend these
shared files instead of maintaining branch-specific verdict rules. The additional
contracts exercise missing results, malformed registration, required preflights,
skip policies, failed builds, immutable logs and real CTest invocation using a
tiny compiler-free project. They start no game, renderer or budget coordinator.

After a workflow change, run both contract suites and source preflight. CI uses
the same gate on Linux, Windows and macOS; an unexecuted CI platform remains an
external validation limitation. Require independent read-only review, preserve
the review record, and keep gameplay/rendering changes in their own packages.

## The WSL ASan/UBSan gate

`tools/test/wsl_gate.sh` is the one committed WSL (Ubuntu, GCC 13) viewer-enabled
ASan+UBSan gate (W4-MAIN-1 review F5 / W4-OPENING-CONTROLS-FLAKE step 0), used for
every WSL lane from now on instead of a fresh, gitignored `cache/` script per gate
(see `docs/testing/wsl-gate-root-cause-2026-11-05.md` for why that pattern kept
rediscovering the same two bugs). It takes the WSL path of a *separate* worktree
(never a live executor worktree, never the owner's `E:\anno1503` checkout):

```sh
wsl -d Ubuntu -- bash tools/test/wsl_gate.sh /mnt/e/anno1503-worktrees/<name> --tag <tag>
```

It applies the `core.autocrlf` override before checking the tree is clean (refusing
to gate a dirty checkout), never runs `anno_render_gpu_tests` as part of the
ASan lane (WSL's only adapter, llvmpipe, cannot pass those tests' named-adapter
checks; CI deliberately keeps "full ASan" and "full GPU-adapter parity" apart), and
records each lane's elapsed time in its gate summary file. Pass `--gpu-lane` to
additionally run the GPU-adapter suite on Mesa lavapipe with `ANNO_REQUIRE_GPU=1`
(the same setup `docs/testing/gpu-tests.md` and CI's `linux-gcc` job use), as a
separate, explicit lane, never folded into the ASan one.

**Which run covers what (GATE-1b).** Three different runs each cover a different
slice of GPU-adapter parity, and none of them is a substitute for another:
hosted CI's `linux-gcc` job runs the GPU-adapter parity suite on Mesa lavapipe,
without the original game files, on every push; the Windows game-file run (the
orchestrator's MSVC pass with `ANNO1503_GAME_DIR` set) is the one that exercises
the real adapters (the 4090 and the Radeon iGPU); `wsl_gate.sh --gpu-lane` runs
the same suite on WSL's own lavapipe for local debugging only — useful when
changing GPU-adapter code on this host, but it duplicates hosted CI's coverage
rather than adding to it, so it is never required for a package gate and the
default (omitting it) is the package gate's normal mode.

**Where the logs live.** Every gate run's summary (`wsl-<tag>.txt`) and per-lane
`.log`/`.out` pair land under `~/anno-gate-logs/<worktree-basename>/`, never inside
the worktree under test: an earlier run's raw evidence (`w7final2`) was lost when
its diagnostic worktree was removed, because the gate used to write into that
worktree's gitignored `cache/gates/`, and `git worktree remove` takes the whole
tree with it (W4-SETTLEMENT-1 W7 review, 2026-11-07). The script refuses to
overwrite an existing summary file; pass a distinct `--tag` for a rerun against
the same worktree in the same minute. `--build-dir`, if given explicitly, must
resolve (via `realpath`) to `~/anno-build` or a path under it — anything else is
refused before it is ever handed to the end-of-run `rm -rf`. The script exits
non-zero if any lane fails, or if a lane that was requested (such as `--gpu-lane`
with no lavapipe ICD installed) could not run at all; a reviewer or CI step can
rely on the exit code alone, without re-deriving the verdict from the log text.
