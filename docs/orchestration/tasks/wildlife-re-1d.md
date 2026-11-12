# WILDLIFE-RE-1d: the wildlife reference and its vectors

**Status:** RELEASED 2026-11-12 by the Claude Code orchestrator, with the owner's go, to **the Mac executor**. Step 4′
of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Base:** `origin/main` at this packet's release commit. Branch `task/WILDLIFE-RE-1d`, in a **new worktree**. About
3–4 hours.

**Rules:** the owner rule of 2026-11-12 on the board. One concern, no reviewer subagents (the orchestrator reviews
your handover), one REPORTS entry of about 60 lines, READY FOR REVIEW. No engine code: Python, a vectors file and two
test registrations only.

## Why

The wildlife port (WILDLIFE-1, later) needs an oracle that is not its own C++. This packet turns the draw tables and
rules of [`docs/re/wildlife.md`](../../re/wildlife.md) §1–10 into an independent reference with hand-derived vectors,
in the style of [`tools/hunter_ref.py`](../../../tools/hunter_ref.py) and its contract test.

## Do

1. **`tools/wildlife_ref.py VECTORS`.** Written from `docs/re/wildlife.md` only. Key=value records, then `=>` and the
   expected keys, exit codes 0 agree / 1 disagree / 2 malformed, missing or zero vectors (as `hunter_ref.py`).
   **Draw stream in, decisions out:** a record takes the raw world-draw values as a list and reports what they select
   and how many it consumed. The reference does not reimplement the world generator. Record types:
   - `density`: candidate count n → desired spawn records (§1 table, no draw);
   - `habitat`: centre and neighbour facts, island property 7 → pass or fail;
   - `replenish`: the compacted candidate list, the deficit, the occupied cells, the draws → the chosen cells, the
     draws consumed (a collision retries with a new draw);
   - `create`: cave or not, climate byte, that climate's 32-entry type table, the saved goods, the draws → position
     index (no draw for a cave), species, draws consumed;
   - `load`: entry byte 4, base time, world mode bit, entry goods, the draws → timestamp and saved goods;
   - `animal`: the new animal class's completion, override and fallback transitions (§7) → next state, draws consumed;
   - `walk`: attempt byte, behaviour, facing, the draws → for each of the three candidates the radius in sixteenths
     and the angle-table index (integers only; the float positions stay out, OQ-582);
   - `route_old`: the old animal route's kind-3 completion → next kind, draws consumed;
   - `wolf`: the wolf's completion, fallback and override transitions (§10) → next state, draws consumed;
   - `prey`: predator type id, prey type id and kind → admitted or not (the mask-8 kind rule and both id lists).
   A case the documents leave open is refused with exit 2 and its OQ number, never guessed: candidate exhaustion
   (OQ-572), a cave without an entrance (OQ-574), zero-HP prey (OQ-585).
2. **`fixtures/synthetic/sim/wildlife_vectors.txt`.** At least two vectors per record type, **hand-derived from the
   document** (write the derivation as a comment line above each vector), not produced by running the script. Each
   record type has at least one boundary case. Examples: n = 13 against 14 in the density table, a collision that
   costs a second draw, the angle index's byte wrap, an idle start whose modulus is 1 and still draws, a draw consumed
   before the quantity override replaces the state, prey 503 admitted only from the short list.
3. **`tools/wildlife_ref_contract_test.py --ref ... --vectors ...`** in the style of `hunter_ref_contract_test.py`:
   the real vectors agree; a vectors file with one expected value changed fails with 1; malformed and empty files fail
   with 2.
4. **Register both** in `CMakeLists.txt` next to `hunter_reference_agrees` / `hunter_reference_contract`, as
   `wildlife_reference_agrees` and `wildlife_reference_contract`.
5. **Where the document is wrong or ambiguous** while you write the reference: don't silently pick one reading. Add a
   dated bullet to `docs/re/wildlife.md` or a new OQ from **OQ-587** to **OQ-590** (reserved for this packet).

## Gate

- `python -I tools/wildlife_ref.py fixtures/synthetic/sim/wildlife_vectors.txt` and the contract test, directly.
- Both CTest tests in a configured `dev` build, through `tools/test/run_selected.py --select 'wildlife_reference'`,
  classified counts.
- Hosted CI green on your pushed head (the full matrix runs, since `CMakeLists.txt` changes). Read CI after every push.

## Out of scope

Any engine code or wildlife port (WILDLIFE-1); closing OQ-578; float position vectors (OQ-582); the whaler.

## Report

READY FOR REVIEW, about 60 lines: the record types and vector counts, the boundary cases, the refused cases with their
OQs, any document corrections, the gate counts, NOT_RUN, Decisions and reasoning, QUESTIONs.
