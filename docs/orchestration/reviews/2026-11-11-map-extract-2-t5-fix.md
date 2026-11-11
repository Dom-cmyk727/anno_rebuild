# MAP-EXTRACT-2 T5 fix: the two-reserver reload test (2026-11-11)

**Verdict: ACCEPTED.** The new test executes with the game files, and it catches the unsaved-field mutant that T5's
review found surviving. No P1 or P2. The fix is part of MAP-INTEGRATE-2's input `a5a3bca3`.

**Reviewer:** the Claude Code orchestrator (Opus 5.5). The owner had asked that the orchestrator review this fix
(MAP-EXTRACT-2 final report, part E: "the fix was not re-reviewed (owner instruction: the orchestrator reviews it)").

## What was reviewed

T5's review found hole P2-1: the mutant U1 survived. U1 leaves `Mine::reserved` unsaved, and it survived because the
pilot never had two carriers reserving on one mine at once. Three commits answer it:

| Commit | Change |
|---|---|
| `427b4df6` | New case `iron continuation: two carriers hold reservations on one mine at the reload boundary` in `src/sim/tests/iron_interrupt_test.cpp`. It builds a second smelter (1017 at 360,340) with a legal `BuildCmd`. Its boundary is the first step at which two carriers in step `ToSource` hold `reserved_88 > 0` on the mine. |
| `3f529f24` | `iron-tools.md`: removes the false sentence that the field was recoverable from one carrier's `reserved_88`. Two comment lines in `iron_chain_test.cpp`. |
| `a5a3bca3` | The case no longer asserts after its own skip: `continuation_with` returns whether the chain ran. Without the game files, it had failed on every hosted CI job. |

## Findings from reading the code

- **The boundary is sound.** The predicate counts figure slots, so two holders are two distinct carriers.
  - At that step the case checks `mine.reserved == sum of the holders' reserved_88`. This proves the saved total is
    not one carrier's value.
  - Then `continuation_with` saves, loads, and checks `re.save() == bytes` and an equal `state_hash`.
  - It runs 800 advances on both worlds, and compares `state_hash`, the PRNG cursor and the save bytes.
  - A discriminating control is included: a reload that runs 799 advances must differ.
- **The skip fix is correct.** `claim_sum > 0` is asserted only when the chain ran, and the other callers ignore the
  new return value through `continuation_at`.
- **No P1 or P2 in the code.**
- **P3, test sharpness.** The equality check assumes that only carriers in step `ToSource` hold a reservation. If a
  later change let a carrier keep its claim in another step, the check would fail loudly rather than pass wrongly, so
  it is safe.

## Executed evidence

- **Game-file run on this PC, 2026-11-10,** at `a5a3bca3` (`cache/map-extract-2-ci/run1/`). The case executed and
  passed: 4,296 assertions, 7.14 s, with `ANNO1503_GAME_DIR` set. It is not a skip.
- **Mutant U1, re-run by the orchestrator** in its own review checkout
  (`E:\anno1503-worktrees\anno1503-MAP-EXTRACT-2-ci`, clean at `a5a3bca3`). The script and logs are in
  `cache/t5-review-2026-11-11/`.
  - **Mutant:** line `a.u32(s.reserved);` removed from `fields(Mine)` in `state_fields.hpp`.
  - **The script checks the replacement applied.** Run 1 was invalid: its script assumed LF line endings, the
    checkout has CRLF, so nothing was mutated and all three builds were no-ops. Run 1's logs are kept in
    `run1-invalid/`.
  - **Results, run 2** (2026-11-11 11:24–11:36, MSVC Release, `ANNO1503_GAME_DIR` set):

    | Step | Build | The case | Assertions |
    |---|---|---|---|
    | Control (unchanged) | 0 | passed | 4,296 / 4,296 |
    | Mutant U1 | 0 (the full `anno_sim_tests` rebuild) | **failed** | 2 of 4,296 failed |
    | Restored | 0 | passed | 4,296 / 4,296 |

  - **The two failing assertions:**
    - `iron_interrupt_test.cpp:418`: `re.state_hash() == w.state_hash()` after the 800 advances;
    - `iron_interrupt_test.cpp:420`: `re.save() == w.save()`.

    The checks at load passed, as expected: the field is missing on both the save side and the load side. The
    divergence appears in the continuation, which is what the test is built to catch.
  - **After the restore,** the checkout is clean at `a5a3bca3` (0 status lines).
  - **sha256:**

    | File | sha256 |
    |---|---|
    | `mutate.py` | `5a345cf4...aa831c1` |
    | `run.ps1` | `4c37e54e...7c57734a` |
    | `run-control.log` | `fd9c5eee...600ff08` |
    | `run-mutant.log` | `3677b172...4c4e37` |

## Not done here

- **The second mutant**, the field written but not restored, was not re-run. The executor's logs show it killed by
  both reload cases (`mutants_run15.log`, sha `eb140489...`).
- **The other T5 checks** (continuation at extraction, smelting, delivery and spend; the fresh-process replay; cost)
  were reviewed by the executor's T5 reviewer and are not re-reviewed here.
