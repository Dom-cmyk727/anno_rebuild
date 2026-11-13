# FISHER-RE-1b: the fisher reference and its vectors

**Status:** ACCEPTED 2026-11-13 ([review](../reviews/2026-11-13-fisher-re-1b-review.md)); released with the owner's go to **the Mac executor**. Step 9′ of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Why:** [FISHER-RE-1a](fisher-re-1a.md) ([review](../reviews/2026-11-13-fisher-re-1a-review.md)) showed that the
fisher reuses the forester's harvest machinery, plus a few special parts. A flagged fisher port is now a candidate.
Food stops 131 houses ([`progression-gaps.md`](../../sim/progression-gaps.md), row 2). Such a port needs an oracle
that is not its own C++. This packet turns [`docs/re/fisher.md`](../../re/fisher.md) into an independent integer
reference, as [WHALE-RE-1c](whale-re-1c.md) did for the whale.

**Base:** `origin/main` at release, branch `task/FISHER-RE-1b`, in a new worktree. About 2–3 hours.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW;
- no engine code: Python, a vectors file and two test registrations only.

## Do

1. **`tools/fisher_ref.py VECTORS`.**
   - **Its source:** written from `docs/re/fisher.md` only, in the format of `tools/whale_ref.py`: key=value records,
     then `=>` and the expected keys.
   - **Exit codes:** 0 agree, 1 disagree, 2 malformed, missing or zero vectors.
   - **Draw stream in, decisions out:** integers only.

   The record types:
   - **`farm_tick`** (§2):
     - inputs: elapsed and stamp, the switch, suspend flags, made, worker count and maximum, raw, need, rate, output,
       capacity, activity, and the draws;
     - outputs: advanced, r, debit, output added, the new activity, dispatched, and draws consumed (the discarded
       dispatch draw, 1 or 0).
   - **`converted`** (§1): T0 and I0, given as exact fractions → need and output per step.
   - **`search_rect`** (§3): farm x, y, width, height and radius → the rectangle's origin and size.
   - **`sea_cell`** (§3): four corner heights → open or closed, and the free goal subcells (0 or 511).
   - **`trip`** (§3): cargo after a cut, and the capacity → repeat the search, or go home.
   - **`cut_match`** (§1, §3): tile id → whether it matches goods 71, and the tile it leaves behind.
   - **Refusals.** A case the document leaves open is refused with exit 2 and its OQ number, never guessed: float
     positions and continuations (OQ-603), the transitive draw total (OQ-604).
2. **`fixtures/synthetic/sim/fisher_vectors.txt`:** at least two vectors per record type.
   - **Hand-derived from the document,** with the derivation as a comment line above each vector, not produced by
     running the script.
   - **At least one boundary case per type,** for example:
     - elapsed 499 against 500;
     - for 3978, raw 1,023 dispatches and raw 1,024 does not;
     - for 810, raw 1,586 against 1,587;
     - a full output that still dispatches and draws;
     - raw 63 against raw 64 for 3978;
     - a cargo of 767 against 768;
     - corner heights 31 against 32;
     - tile 11 against tile 10 and 3506;
     - the unrotated 3×4 farm's rectangle, origin (x − 6, y − 6) and size 15×16.
3. **`tools/fisher_ref_contract_test.py --ref ... --vectors ...`,** in the style of `whale_ref_contract_test.py`:
   - the real vectors agree;
   - a file with one expected value changed fails with 1;
   - malformed and empty files fail with 2;
   - at least three source mutants that recreate a wrong reading fail. Examples: no draw at a full output; a clamp
     on output; water at a mean of ≤ 32 instead of < 32.
4. **Register both** in `CMakeLists.txt`, next to the whale pair, as `fisher_reference_agrees` and
   `fisher_reference_contract`.
5. **Where the document is wrong or ambiguous** while you write the reference: don't silently pick one reading. Add a
   dated bullet to `docs/re/fisher.md`, or a new OQ from **OQ-605 to OQ-608** (reserved for this packet).

## Gate

`TASKS.md` §1 item 4: this packet changes `tools` and `CMakeLists.txt` only.
- Run `python -I tools/fisher_ref.py fixtures/synthetic/sim/fisher_vectors.txt` and the contract test directly.
- Run both CTest tests through `tools/test/run_selected.py --select 'fisher_reference'` in a configured build, and
  give the classified counts. No other suite.
- Hosted CI green on your pushed head (the full matrix runs). Read CI after every push.

## Out of scope

- any engine code, or the fisher port;
- closing OQ-603 or OQ-604;
- float positions and movement;
- the History Edition reading;
- ice fishing (3506, EISFISCH).

## Report

READY FOR REVIEW, about 60 lines:
- the record types and vector counts;
- the boundary cases;
- the mutants;
- the refused cases, with their OQs;
- any document corrections;
- the gate counts;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
