# WHALE-RE-1c: the whale and whaler reference and its vectors

**Status:** PROPOSED 2026-11-13 by the Claude Code orchestrator for **the Mac executor**. It is released when the owner
gives the go. Step 7′ of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Why:** a later whale port needs an oracle that is not its own C++. This packet turns the accepted rules and direct
draws of [`docs/re/whaler.md`](../../re/whaler.md) §1–9 into an independent reference with hand-derived vectors, as
[WILDLIFE-RE-1d](wildlife-re-1d.md) did for wildlife (`tools/wildlife_ref.py`). Reviews:
[1a](../reviews/2026-11-13-whale-re-1a-review.md), [1b](../reviews/2026-11-13-whale-re-1b-review.md).

**Base:** `origin/main` at release, branch `task/WHALE-RE-1c`, in a new worktree. About 3 hours.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover);
- one REPORTS entry of about 60 lines, READY FOR REVIEW;
- no engine code: Python, a vectors file and two test registrations only.

## Do

1. **`tools/whale_ref.py VECTORS`.**
   - **Its source:** written from `docs/re/whaler.md` only, in the format of `tools/wildlife_ref.py`: key=value
     records, then `=>` and the expected keys.
   - **Exit codes:** 0 agree, 1 disagree, 2 malformed, missing or zero vectors.
   - **Draw stream in, decisions out:** a record takes the raw world-draw values as a list and reports what they
     select and how many it consumed. Integers only; float positions stay out.

   The record types:
   - **`replenish`** (§3): elapsed and stamp; per slot, empty or occupied, its retry byte and whether creation
     succeeds → the new bytes, which slots attempt creation, the stamp advance. No draw.
   - **`whale_walk`** (§7):
     - inputs: a nearby figure or none, counter `+0xc`, home x/z, the figure's x/z, the world width and height, and
       five draws;
     - outputs: the offsets, the goal before and after the clamp, the new counter, and draws consumed (always 5).
   - **`whale_route`** (§7): initial or later selection, and whether the kind-12 start fails → the next node. No
     draw.
   - **`whale_hp`** (§6): old HP, new HP and the ending bit → whether the ending transition happens.
   - **`whaler_tick`** (§4):
     - inputs: elapsed, the switch, queue active and progress, the selected index, whether the boat is idle, raw,
       capacity, need, rate, output and activity;
     - outputs: launch attempted, hunt dispatched, the conversion's r, debit and output added, and the new activity.
       No draw.
   - **`boat_request`** (§8):
     - inputs: money, the wood and rope stock, queue count and the active byte;
     - outputs: enqueued or refused, and the amounts charged, in stock units.
   - **`ship_attack`** (§9): firing-position count and the draw → the selected index, and draws consumed (1, also
     when the count is 1).
   - **Refusals.** A case the document leaves open is refused with exit 2 and its OQ number, never guessed: sea
     legality or depth (OQ-595), the indirect search and projectile draws (OQ-594).
2. **`fixtures/synthetic/sim/whale_vectors.txt`:** at least two vectors per record type.
   - **Hand-derived from the document,** with the derivation as a comment line above each vector, not produced by
     running the script.
   - **At least one boundary case per type,** for example:
     - retry byte 9 → 10 creates, and 255 wraps to 0;
     - elapsed 2,499 against 2,500;
     - walk counter 3 → 4 against 4 → 3;
     - the goal clamp at the high bound as written;
     - the fifth walk draw that changes nothing;
     - queue progress: the 30th call against the 31st;
     - conversion r 56 → nothing, against r 64;
     - output 2,550 → 2,567 with no clamp;
     - an inactive request still charged;
     - an attack with one firing position that still draws.
3. **`tools/whale_ref_contract_test.py --ref ... --vectors ...`,** in the style of
   `wildlife_ref_contract_test.py`:
   - the real vectors agree;
   - a file with one expected value changed fails with 1;
   - malformed and empty files fail with 2.
4. **Register both** in `CMakeLists.txt` next to the wildlife pair, as `whale_reference_agrees` and
   `whale_reference_contract`.
5. **Where the document is wrong or ambiguous** while you write the reference: don't silently pick one reading. Add a
   dated bullet to `docs/re/whaler.md`, or a new OQ from **OQ-596 to OQ-599** (reserved for this packet).

## Gate

`TASKS.md` §1 item 4: this packet changes `tools` and `CMakeLists.txt` only.
- Run `python -I tools/whale_ref.py fixtures/synthetic/sim/whale_vectors.txt` and the contract test directly.
- Run both CTest tests in a configured build through `tools/test/run_selected.py --select 'whale_reference'`, and give
  the classified counts. No other suite: nothing else changes.
- Hosted CI green on your pushed head. The full matrix runs, since `CMakeLists.txt` changes. Read CI after every
  push.

## Out of scope

- any engine code, or the whale port;
- closing OQ-594 or OQ-595;
- float position vectors;
- the History Edition reading;
- the native WALSPECK records 3501 and 3515.

## Report

READY FOR REVIEW, about 60 lines:
- the record types and vector counts;
- the boundary cases;
- the refused cases, with their OQs;
- any document corrections;
- the gate counts;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
