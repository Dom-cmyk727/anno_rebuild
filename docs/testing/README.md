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
`picture_matrix_contract` automatically. Each required contract must be registered,
executed and successful, with no skips. Its outcomes do not inflate the main
selection's counts. This main branch does not yet contain W4's benchmark: its
trace/table semantic fixes remain with that executor.

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

## Coordinator ports on Linux and WSL

The budget coordinator and its tests listen on fixed ports 47631..47920. On Linux
(and WSL) these lie inside the kernel's ephemeral range (32768-60999), so under
`ctest -j` an outgoing connection of another test can hold one of them as its local
port; the coordinator then prints `BIND-FAILED <port>` and a different coordinator
case fails each run. CI reserves the range before testing; do the same on a Linux
or WSL test host (until reboot):

    sudo sysctl -w net.ipv4.ip_local_reserved_ports=47600-47999

Reproduced on 2026-11-07 by narrowing the ephemeral range onto the F3 ports while
25 client connections were held: F3 (D1) failed with `BIND-FAILED 47810` and
`47814`; with the reservation the clients got 48000+ and the cases passed. macOS
and Windows hand out 49152-65535 and are not affected.

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
