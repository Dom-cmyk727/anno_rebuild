# WHALE-RE-1c review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** `tools/whale_ref.py` is an independent integer reference for the seven record
types, written from `docs/re/whaler.md`. Every vector the orchestrator worked through by hand agrees with the
document. The one vector that depended on a reading the document did not state, negative money, matches the original:
the gold check is a signed comparison.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/WHALE-RE-1c` `c0553b48`:
- `4a037c90`: the reference, its vectors, its contract test and the CMake lines;
- `77cd5a58`: negative money refused as insufficient;
- `c0553b48`: the REPORTS count.

Changed: `tools/whale_ref.py` (280 lines), `tools/whale_ref_contract_test.py` (168),
`fixtures/synthetic/sim/whale_vectors.txt` (44 vectors), `CMakeLists.txt` (`whale_reference_agrees`,
`whale_reference_contract`), a 28-line dated addendum to `docs/re/whaler.md`, and the REPORTS entry. Base `main`
`961a9d4a`. No engine code.

## What the orchestrator checked

- **Both scripts run on this PC** (Windows, `python -I`):
  - the reference: "44 vectors, 0 disagreements", exit 0;
  - the contract test: "46 checks passed".

  The scripts import only the standard library. The contract test writes only temporary files, and runs the
  reference and its seven source mutants.
- **Vectors worked by hand against `whaler.md`:**
  - **`whale_walk`:**
    - draws 0, 4, 1, 0 give offsets (-7, +11);
    - counter 3 with a nearby figure uses it and becomes 4; counter 4 uses home and becomes 3;
    - the goal clamps to the inclusive high bound 100;
    - home (2, 3) with (-11, -7) clamps to (0, 0);
    - the fifth draw changes nothing, yet is consumed: 5 draws every time.
  - **`whaler_tick`:**
    - 24,737 + 853 = 25,590, under 25,600, so no launch; the next visit reaches 26,443 and launches;
    - raw 7 gives r 56, so nothing converts; raw 8 gives r 64, debit 8 and 8 output;
    - activity 128 → 127 + 16·8 = 255;
    - output 2,550 + 17 = 2,567, with no clamp;
    - a full output still hunts, because the raw-stock check comes first;
    - raw equal to capacity does not hunt, and still converts;
    - the u16 wraps: 65,535·127/128 + 16,000 → 15,487, and 65,530 + 1,000 → 994.
  - **`replenish`:**
    - 65,000 + 2,500 wraps to 1,964;
    - a retry byte of 9 creates, 255 wraps to 0, an occupied slot is untouched;
    - elapsed 7,500 still advances once.
  - **`boat_request`:**
    - 600 gold, 3,072 and 1,792 stock units exactly pass; one unit less of any of them refuses;
    - a full queue refuses after the affordability check passes;
    - an inactive request is still charged.
  - **`ship_attack`:** a count of 1 still consumes its draw, 14 mod 5 = 4, and a count of 255 against 2^32 − 1
    gives 0.
- **One reading checked in the original.** `0x10073c10` compares the price with the player's money as `cmp ebp,
  eax` / `jle`, a signed comparison. So negative money is refused, as vector `negative_money` and commit `77cd5a58`
  say.
- **Refusals:**
  - sea legality and depth → OQ-595;
  - indirect search and projectile draws → OQ-594.

  Both exit 2 with their number; nothing is guessed.
- **The executor's gates:**
  - CTest through `run_selected.py --select whale_reference`, in a configured `dev` build: 2 of 2;
  - hosted CI green on all 12 jobs (run 43389926686).

## Notes (no fix round)

- **A merge note for the orchestrator.** The branch's base has only WHALE-RE-1a's `whaler.md`. WHALE-RE-1b and 1c
  both append to the end of `whaler.md` and `REPORTS.md`, so the merge has two textual conflicts. They are resolved
  by keeping both texts verbatim, 1b's §6–9 first, then 1c's addendum. The addendum already cites 1b's sections.
- **The `whaler_tick` record takes two supplied facts:** whether prey was found, and the launch slot. They stand in
  for the search and the boat creation, which the reference does not model. The addendum says so. This is the right
  boundary while OQ-594 and OQ-595 are open.
- **No new open question.** The reserved OQ-596 to OQ-599 are unused.

## Disposition

ACCEPTED. It lands with PLAY-LIVE-1a and WHALE-RE-1b (the owner chose to batch WHALE-RE-1b with PLAY-LIVE-1a), on
the owner's OK. The whale reads 1a to 1c are complete; the port waits for ADR 0006.
