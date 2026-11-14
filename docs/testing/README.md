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

## What a packet runs, and refused tests (owner, 2026-11-13)

- **A packet runs the suites its change reaches**, on MSVC Release with the game files and in the WSL `-O1` sanitizer
  build, plus hosted CI. The table of which change reaches which lane is in
  [`TASKS.md`](../orchestration/TASKS.md) §1 item 4. Run a lane with `run_selected.py --select` and the lane's regex
  from `tools/test/wsl_gate.sh`. Unaffected suites are reused from `main`, with a one-line reason.
- **The full WSL gate below and the full MSVC game-file run** are the orchestrator's, once per landing at the merge.
- **A refused test did not run.** `[anno-test memory] refused … reason=low-memory stop` (or `wait deadline`, or a
  coordinator refusal) is thrown before the test's own work starts, so it is neither a failure nor a pass.
  - Rerun only the refused tests, alone (`--jobs 1`), and keep both runs in the evidence.
  - Never rerun a whole gate or lane on the same commit. It only repeats tests whose evidence already stands, and a
    full WSL gate costs well over an hour.
- **After a fix,** rerun the failed tests and the lanes the fix reaches; the rest of the earlier evidence stands.

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

## Pins and goldens

PINS-1 (2026-11-13), ADR 0006 step1: no state, behaviour or expected hash change.
The single current directory is `fixtures/synthetic/sim/current/`; archived directories
are unchanged. Named pins remain in `goldens.txt` (`name stage stream_version hex64`).
See the [complete literal disposition](pins-literals.md).

With a configured dev build and the original files available:

```sh
ANNO1503_GAME_DIR=/path/to/game/app python3 -I tools/pins.py check
ANNO1503_GAME_DIR=/path/to/game/app python3 -I tools/pins.py regen --area sim
```

`--build-dir` and `--config Release` support other configured builds. `pins_checks`
builds only the sim tests and replay consumer. The default selection is the sim
suite, replay CLI consumers and the independent stream/highmap comparisons;
`pins_contract` is a required contract. It uses `run_selected.py` and retains the
classified inventory/log under `cache/gates/`. No unrelated renderer suite runs.
`--select REGEX` is an explicit **partial** diagnostic selection, not the full check.
Check defaults to four jobs; regeneration to one, serializing replay/corpus writers.
`--jobs` changes the check test ceiling; build parallelism follows CMake/the existing
`CMAKE_BUILD_PARALLEL_LEVEL` environment, independently; regen requires one test job to avoid
concurrent corpus writers. Admission control stays enabled. `check` clears inherited
record mode; `regen` sets `ANNO_UPDATE_GOLDEN=1` and is refused under CI.

The existing `golden.hpp` writer updates changed self-baselines via a same-directory
temporary file and atomic replacement, sorted by name. A per-file directory lock
serializes writers with a bounded wait; an abandoned lock is reported, never removed
by another writer. Malformed/duplicate pins fail instead of being silently discarded.
The CLI prints each moved pin once (old/new stage, version and value), then recordings actually written,
including those whose bytes remain identical. A failed gate remains nonzero.

Oracle-owned pins (`oracle_*`, empty_world/empty_registry and the existing
G0/G1/S5/S6 stream checkpoints) are **read-only even in record mode**. Their values
must be compared/reviewed against the independent oracle. `sim_stream*.py` and
`*_ref.py` are never invoked in a writing mode; moving their fixture paths changes
no formula or oracle value. Historical fixtures are never recording outputs.

Record mode explicitly declares each current output with `--record-fixture PATH`.
The canonical gate accepts only existing regular files under `fixtures/synthetic/`,
only with local `ANNO_UPDATE_GOLDEN=1`, never under CI; it records before/after SHA256s.
Every other tracked/untracked source remains frozen. This is a recording verdict,
not a substitute for a subsequent normal `check`. Public workflow contracts prove
that an extra code mutation still fails, and that code paths cannot be excluded.

### Deliberate state renumbering (PINS-1 fix1, 2026-11-14)

This is an explicit identity change, not part of an ordinary regeneration. Do it
only in a packet authorizing the bump. A rehearsal uses a separate scratch copy;
never commit its changed versions or generated139 values for PINS-1. Full-line
comments are preserved; inline comments after pin rows are not retained by the writer.

1. **Keep the outgoing identity first.** Before rewriting current files, retain
   `current/g2.log` and its embedded native initial save as
   `fixtures/synthetic/sim/legacy<N>/g2.log` and `g2.save`. For the documented
   replay-v4 envelope, the following read-only extraction uses no current decoder:

   ```python
   from pathlib import Path
   import struct
   base = Path('fixtures/synthetic/sim')
   data = (base/'current/g2.log').read_bytes()
   assert data[:4] == b'A3RP' and struct.unpack_from('<I', data, 4)[0] == 4
   outgoing = struct.unpack_from('<I', data, 12)[0]
   legacy = base / ('legacy' + str(outgoing))
   legacy.mkdir(exist_ok=False)
   (legacy/'g2.log').write_bytes(data)
   at = 76  # magic, three u32, seven u64, PRNG timeline u32
   count, = struct.unpack_from('<Q', data, at); at += 8
   for _ in range(count):
       size, = struct.unpack_from('<Q', data, at); at += 8 + size
   size, = struct.unpack_from('<Q', data, at); at += 8
   assert 0 < size <= len(data) - at
   (legacy/'g2.save').write_bytes(data[at:at+size])
   ```

2. **Bump the C++ identity deliberately.** Change `kStateHashVersion` in
   `world.hpp` and the five `CHECK(sim::kStateHashVersion == N)` tripwires in
   config_test, ship_nav_state_test, resident_state_test, sim_test and ship_state_test.
   Keep those tripwires literal. Refusal messages and only the four version bytes
   of hand-written headers now follow the constant; no other expected byte derives
   from the production writer.
3. **Update the independent transcriptions separately.** Bump `STATE_HASH_VERSION`
   in `tools/sim_stream.py`, then run its `--write-holds` and `--write-cargo-rules`.
   Change the header's `u32(138)` in `tools/integration_stream_ref.py` (and its
   docstring) to the newly authorized version, then run that script with `--write`.
   These are explicitly reviewed oracle edits, never actions performed by pins.py.
   The oracle contract cross-checks the version against sim_stream.py, keeps the
   header tail literal, and mutates the actual version bytes for its negative control.
4. **Hand-copy oracle-held expectations.** Run `python -I tools/sim_stream.py`
   and copy its13 printed values and the new version into the corresponding
   `current/goldens.txt` rows. Change only the version column of the other
   oracle-owned rows (`oracle_core_*`, `oracle_rng_table`,
   `oracle_synthetic_registry`, `oracle_legacy_construction_registry`, and
   `oracle_registry_highmap_absent/present`). Check the latter values with
   `python -I tools/highmap_digest_ref.py --check`; never derive them from C++.
5. **Regenerate self-baselines and recordings.** With the game files set, run
   `python -I tools/pins.py regen --area sim` in the configured build (or specify
   `--build-dir`/`--config`). Regen selects the sim and replay tests, excluding
   standalone oracle comparisons until recording has updated the corpus header.
   It still refuses any oracle-owned pin mismatch inside a sim test. Its summary
   shows `stage version value` on both sides, including version-only changes.
6. **Extend the boundary tests.** Add the actual `legacy<N>/g2.save` and `g2.log`
   to the newest-predecessor case in `integration_identity_test.cpp`, keeping
   older predecessors. Add N to that case's explicit mutated-version controls and
   update its descriptive identity/control name. Assert the native's exact old
   version refusal and replay's state-identity refusal, beside the new valid control.
7. **Finish with check.** Run `python -I tools/pins.py check` with the same game
   files/build. Check retains population_oracles_agree, integration_stream_reference_*
   and highmap_digest_reference_agrees. It must end with zero failures; quote each
   COUNTS/VERDICT line, distinguish skips, and retain source/diff, log hashes and
   the moved-pin/recorded-file counts. A successful regen alone is not acceptance.
