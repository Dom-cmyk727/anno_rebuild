# PINS-1 review (2026-11-13)

**Verdict: FIX ROUND 1, small.** The mechanism is right and well proven: one pins file, a writing update mode
refused under CI, a stable `current/` directory, and `tools/pins.py check` and `regen` through the canonical gate.

Two things are left:
- **F1, a defect:** two pins that an independent oracle holds are filed as ordinary pins, so `regen` would overwrite
  them silently. That breaks the packet's independence rule.
- **F2, the packet's purpose:** the orchestrator dry-ran the real use, the 138 → 139 renumbering, on this PC. `regen`
  rewrote all 44 pins and the 14 recordings correctly. Twelve tests still failed afterwards: hand-written
  expectations that name the version, and no written procedure for the steps the oracles need.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/PINS-1` `95ce43a8`, two commits on `main` `f9e6ea0c`:
- `06ffaa20`: the pins, the recordings and the command;
- `95ce43a8`: the recorder lock and the checked regeneration.

Changed: `golden.hpp`, `golden_test.cpp` (new), `main.cpp`, nine sim test files, `tools/pins.py` and its contract
(new), `run_selected.py` (`--record-fixture`), the rename to `fixtures/synthetic/sim/current/`,
`docs/testing/pins-literals.md` (new), `docs/testing/README.md` and `docs/sim/state.md` §14.3. Hosted CI green on all
12 jobs, `gate` included (run 43458308619).

## What the orchestrator checked

- **The literal count.** At `f9e6ea0c`, `git grep -E "0x[0-9a-f]{16}" src/sim/tests` finds 97 lines in 24 files: the
  report's 103 occurrences, since some lines hold several. The packet's 126/30 was an older count. On the branch, 76
  lines remain, all on the "kept" list.
- **The kept literals, spot-checked in context:**
  - test-generator seeds (`build_terrain_test.cpp` 798, 1575, 1743);
  - the splitmix64 known answer (`sim_test.cpp` 757);
  - FNV-1a basis and known answers ("a", "foobar");
  - codec and archive field values;
  - the ECONBK1 marker, sentinels and ids.

  None moves with the state. There are no 13–15-digit hash literals and no quoted hash strings. Outside
  `src/sim/tests` no state hash is pinned (`opening_test.cpp` uses a made-up value).
- **The writer** (`golden.hpp`):
  - validates rows, and refuses duplicates and malformed values;
  - writes through a temporary file and a same-directory `rename`, sorted by name;
  - takes a directory lock with a bounded wait and never removes another writer's lock;
  - refuses under `CI` in `main()` before any test runs.

  `golden_test.cpp` exercises it on private files, including two concurrent writers.
- **The gate change** (`run_selected.py --record-fixture`):
  - accepted only with local `ANNO_UPDATE_GOLDEN=1`, never under `CI`;
  - only for existing regular files under `fixtures/synthetic/`;
  - excluded from the source-identity check by literal pathspecs, with before and after SHA-256 logged.

  Every other file stays frozen. The workflow contracts show that a code change during a recording still fails with
  9. The normal gate is unchanged when the option is absent.
- **The recordings:**
  - the scenario corpus keeps its comment lines and rewrites only the version header and the rows;
  - SCRIPT2 now replaces the creation hashes in the current rows. Before, it rebuilt them from the historical
    supply129 corpus.

  That is the right reading of "re-record in place".
- **Hosted CI** ran the writer contracts on Windows MSVC as well, so the Windows `rename` over an existing file is
  covered.

## Findings

- **F1: two oracle-held pins are filed as ordinary pins.** `registry_highmap_absent` (`7adcb2c0d02b1a70`) and
  `registry_highmap_present` (`4cbafff0a755efe4`) are the values of `tools/highmap_digest_ref.py`. Its
  `PINNED = (0x7adcb2c0d02b1a70, 0x4cbafff0a755efe4)` is commented "the values src/sim/tests/types_test.cpp pins", and
  CTest `highmap_digest_reference_agrees` checks it.

  Under `regen` the C++ pin would move silently. The Python oracle would still agree with its own copy, and nothing
  would compare the two any more. That is the circular baseline the packet forbids ("never overwrite a value an
  independent oracle holds"). Fix:
  1. **Name both oracle-owned,** for example `oracle_registry_highmap_absent` and `oracle_registry_highmap_present`,
     so `regen` refuses them.
  2. **One copy of each value:**
     - `highmap_digest_ref.py --check` compares its computation with those two rows of `goldens.txt`, as
       `sim_stream.py --check` does, instead of a second copy in `PINNED`;
     - or it keeps `PINNED` and also asserts that `goldens.txt` holds the same values.
  3. **A guard for the class:** a check in `pins_contract_test.py` that no pin outside the oracle-owned names has a
     value that appears in any `tools/*.py`. Today it would have found exactly these two (the orchestrator's scan
     of all 22 moved values: only these two, plus `oracle_synthetic_registry` in `sim_stream.py`, which is already
     read-only).
  4. **The inventory:** correct the two rows in `pins-literals.md`.
- **F2: the renumbering is not yet one command.** The orchestrator's trial below shows that `regen` itself works.
  Twelve tests still fail after it, all hand-written expectations that name the version. Fix:
  1. **The refusal messages follow the constant.** Nine sites compare a refusal message that contains "this build
     reads 138" as a literal; eight failed in the trial:
     - `economy_facts_test.cpp:217`;
     - `freight_state_test.cpp:292`;
     - `integration_identity_test.cpp:59, 139, 209, 273`;
     - `script_canary_test.cpp:370`;
     - `script_runtime_test.cpp:131`;
     - a ninth, `founding_assets_test.cpp:5204`, in a case the trial skipped as opt-in.

     Build that part with `std::to_string(sim::kStateHashVersion)`. The refused version and the rest of the text
     stay literal. `git grep "reads 13" src` finds exactly these nine.
  2. **The version field of hand-written bytes follows the constant:**
     - `sim_test.cpp:910`, the core stream header (`{138, 0, 0, 0, 0x45, …}`);
     - `script_runtime_test.cpp:42`, the native header;
     - `replay_test.cpp:211`, the documented log frame.

     Derive only the version's four bytes from `kStateHashVersion`, little-endian. Every other byte stays written by
     hand, so the layout is still checked independently of the writer.
  3. **`regen` leaves out the oracle comparisons** (`population_oracles_agree`, `integration_stream_reference_*`).
     They can't be regenerated, and in the trial `population_oracles_agree` failed inside `regen` only because it ran
     before the corpus header was rewritten later in the same run. `check` keeps them.
  4. **The summary shows the version.** A pin whose value stays and whose version moves now prints as
     `7adcb2c0d02b1a70 -> 7adcb2c0d02b1a70`. Print `stage version value` on both sides.
  5. **Write the renumbering procedure** in `docs/testing/README.md` "Pins and goldens", as ordered steps, from the
     trial:
     1. **Keep the predecessor.** Before anything is rewritten, copy what the refusal tests will need of the
        outgoing identity into a `legacy<N>` directory, for example the current `g2.log` and a native save.
        `current/` is now rewritten in place, so nothing else keeps it.
     2. **Bump** `kStateHashVersion` and the five `CHECK(sim::kStateHashVersion == N)` tripwires; the tripwires stay
        deliberate.
     3. **Bump the oracles:**
        - `STATE_HASH_VERSION` in `tools/sim_stream.py`, then `--write-holds` and `--write-cargo-rules`;
        - the header's `u32(138)` in `tools/integration_stream_ref.py` (line 115; the docstring names 138 too),
          then `--write`.
     4. **Copy the oracle-owned pins by hand:**
        - the 13 values `sim_stream.py` prints;
        - the version column of the oracle-owned pins it doesn't print (`oracle_core_*`, `oracle_rng_table`,
          `oracle_synthetic_registry`, `oracle_legacy_construction_registry`, and after F1 the two highmap pins,
          checked with `highmap_digest_ref.py`).
     5. **`pins.py regen --area sim`.**
     6. **Add the outgoing identity to the boundary tests** as the newest refused predecessor.
     7. **`pins.py check`.**
  6. **Prove it:** a version-bump dry run on a scratch copy, not committed. Follow the documented steps; the result
     must be `check` with 0 failures. Report the counts. The committed values do not change.

## The orchestrator's renumbering trial (138 → 139, not committed)

**Where:** this PC, MSVC Release, with the game files, in the orchestrator's run worktree at `95ce43a8`.

**The edits by hand:**
- the C++ constant and the five `== 138` tripwires;
- `sim_stream.py`'s constant, then its `--write-holds` and `--write-cargo-rules`;
- the 19 oracle-owned pin lines: 13 values that `sim_stream.py` printed, and 6 version columns only.

**`pins.py regen`**, the full selection, one job, 18:08–18:26:
- **44 pins moved:** 38 values and 6 version-only;
- **re-recorded:** all 13 replay and loop files, plus the corpus;
- **every replay CLI test passed on the new logs in the same run;**
- **13 failed:** the 11 hand-written expectations of F2 items 1 and 2, `integration oracle` (whose fixture
  `integration_stream_ref.py` writes, not bumped in the trial), and `population_oracles_agree` (the ordering of item
  3).

**`pins.py check`** right after, 18:26–18:32:
- 1,661 selected, 1,628 executed, **12 failed:** F2's 11 sites and `integration oracle`;
- 21 skipped: 18 opt-in and 3 "other", the `ship start` path cases, which skip on this PC on `main` as well;
- `population_oracles_agree` passed.

**Logs** (`cache/orch-pins-1/` in that worktree):
- `regen-bump.log`, sha256 `447195ea06ceaa2c…`;
- `check-bump.log`, sha256 `e7a160927010f161…`;
- `bump-trial.diff`.

The worktree was restored afterwards.

So the mechanism carries a renumbering. What is left are expectations outside its reach, and the procedure. Both are
F2.

## Notes (no fix round)

- **Regen builds with one job.** `pins.py` sets `CMAKE_BUILD_PARALLEL_LEVEL` to the test job count, so `regen`
  compiles serially after a header change. Prebuilding `pins_checks` avoids it. A one-line fix is welcome in the fix
  round, but it is not required.
- **The comment rule.** The writer keeps only full comment lines and moves them to the top. An inline comment after a
  pin is lost. The file has none today; the README could say "full-line comments only".
- **The oracle's own fixtures are written with CRLF on Windows** (`sim_stream.py --write-holds` and
  `--write-cargo-rules` open in text mode). This is older than PINS-1 and does not matter on the Mac. It is recorded
  for a Windows renumbering.

## Disposition

FIX ROUND 1 to the Mac executor, through the owner's prompt, on the same branch: F1 and F2. About 2–3 hours.

**Gates:**
- the sim tests that change;
- `pins_contract`, `highmap_digest_reference_agrees` and `population_oracles_agree`;
- F2's dry run, uncommitted;
- one `pins.py check` on the final head;
- hosted CI.

No committed value changes. PINS-1 lands after the fix round is accepted, on the owner's OK.

## Fix round 1 re-review (2026-11-14)

**Verdict: ACCEPTED.** Every item of F1 and F2 is done as asked, and the executor's own renumbering rehearsal found
and fixed a twelfth version-naming expectation that the orchestrator's trial had not reached.

**Reviewed head:** `task/PINS-1` `661cd2a6`, one commit on `95ce43a8`. Hosted CI run 43537097319 on exactly that
commit: 12 of 12 jobs green, `gate` included.

**What the orchestrator checked, in the diff:**
- **F1.**
  - Both highmap pins are renamed `oracle_registry_highmap_absent` and `oracle_registry_highmap_present`, with
    unchanged values, in `goldens.txt` and `types_test.cpp`.
  - `highmap_digest_ref.py --check` now requires its computation, its own `PINNED` and the two named rows of
    `goldens.txt` to agree, and the rows' version to equal `sim_stream.py`'s.
  - `pins_contract_test.py` gains the class guard: no pin outside the oracle-owned names may have a value that is an
    integer constant in any `tools/*.py`. It also checks that the old misfiling is caught.
  - The two rows of `pins-literals.md` name their oracle.
- **F2.**
  - All nine refusal messages build "this build reads N" from `kStateHashVersion`, and the refused versions stay
    literal. `git grep "reads 13" src` finds nothing.
  - In the three hand-written headers (`sim_test.cpp`, `script_runtime_test.cpp`, `replay_test.cpp`) only the four
    version bytes follow the constant. Every other byte is still written by hand.
  - `regen` selects only the sim and replay tests. `check` keeps `population_oracles_agree`, the integration stream
    references and `highmap_digest_reference_agrees`.
  - The summary prints stage, version and value on both sides. A contract test covers a version-only move.
  - `docs/testing/README.md` has the seven-step procedure, starting with keeping the outgoing identity in
    `legacy<N>`.
  - `integration_stream_ref.py`'s own identity contract no longer names 138. It checks the version against
    `sim_stream.py`, and its negative control now proves its mutation applied. This was the twelfth site; the
    orchestrator's trial had not bumped that oracle.

**From the report, on the Mac** (the logs are not readable from this PC):
- The rehearsal (138 → 139, in a scratch copy, uncommitted) moved 42 self-held pins (38 values and 4 versions
  only). Two fewer than the orchestrator's trial, because F1 made two more oracle-owned.
- The final check of the rehearsal: `COUNTS: selected=1662 executed=1644 failed=0 … skipped=18 (… opt-in=18 …)` and
  `VERDICT: PASS`. The first check failed once, on the twelfth site above; that log is kept.
- The committed head's check gave the same counts and `VERDICT: PASS`, and `pins_contract` passed alone.
- Three tests of the rehearsal's `regen` could not be admitted because the sandbox refused the socket to the memory
  coordinator. With the owner's approval on the Mac, only those three were stopped, and each was rerun alone with
  `--jobs 1`. The final check, which passed in full, shows the result is complete.

**Not verified by the orchestrator:** the Mac's logs, and a renumbering trial of the fixed head on this PC. The
landing's game-file run re-runs every test on the merged commit.

**For the landing:**
- `task/PINS-1` is based on `f9e6ea0c` and does not contain `main`'s `95eea7de`. A trial merge conflicts only in
  `REPORTS.md`, as two appends at the end; the orchestrator resolves it in the candidate.
- Since its base, `main` changed `fixtures/synthetic/sim/fisher_vectors.txt` (FISHER-RE-1c), a file the pins do not
  manage.
- It lands on the owner's OK as a candidate with its game-file run, after UI-INFO-1.
