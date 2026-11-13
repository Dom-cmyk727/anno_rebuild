# FISHER-RE-1b review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** `tools/fisher_ref.py` is an independent integer reference for the six record
types, written from `docs/re/fisher.md` only. Every vector the orchestrator worked through by hand agrees with the
document.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/FISHER-RE-1b` `f931354a` (one commit). Changed:
- `tools/fisher_ref.py` (249 lines);
- `tools/fisher_ref_contract_test.py` (187);
- `fixtures/synthetic/sim/fisher_vectors.txt` (42 vectors);
- `CMakeLists.txt` (`fisher_reference_agrees`, `fisher_reference_contract`);
- the REPORTS entry.

Base `main` `961a9d4a`. No engine code and no document change.

## What the orchestrator checked

- **Both scripts run on this PC** (Windows, `python -I`):
  - the reference: "42 vectors, 0 disagreements", exit 0;
  - the contract test: "58 checks passed", with all nine applied source mutants detected.

  Standard library only; the contract test writes only temporary files.
- **Vectors worked by hand against `fisher.md`:**
  - **`converted`:**
    - Anlagen 810: 256/15 × 1,035/256 = 1,035/15, so need 69, and trunc(256/15 + 0.5), so rate 17;
    - Anlagen 3978: 51.2 × 2.5, so need 128, and rate 51.

    Both as §1 says.
  - **`farm_tick` dispatch admission, `384/rate ≥ raw/need` in integers:**
    - 3978: 384/51 = 7 against 1,023/128 = 7, so it dispatches; 1,024/128 = 8, so it refuses;
    - 810: 384/17 = 22 against 1,586/69 = 22, so it dispatches; 1,587/69 = 23, so it refuses.

    The boundaries of §4. A full output still dispatches and still consumes the discarded draw.
  - **`search_rect`:** for an unrotated 3×4 farm with radius 7, origin = x + floor((3 − 1)/2) − 7 = x − 6 and y + 1 − 7
    = y − 6; size = 2·7 + 1 + (3 − 1) mod 2 = 15 by 2·7 + 1 + (4 − 1) mod 2 = 16.
  - **`sea_cell`:** an integer mean below 32 is open (511 free subcells), and 32 is closed. Sum 127 gives mean 31,
    so open; sum 128 gives 32, so closed.
  - **`trip`:** cargo 767 repeats the search, and 768 and above go home.
  - **`cut_match`:** tile 11 matches goods 71 and leaves 10; tiles 10, 3506 and 0 don't match.
- **The decisions in the report are consistent with §2:**
  - `made` is checked against the computed quantity;
  - "dispatch" means a creation attempt, drawing even when the creation later fails;
  - with a maximum of 2, the admission reads raw after the conversion's debit, since the conversion comes before the
    dispatch in the tick.
- **Refusals:** float positions → OQ-603; the transitive draw total → OQ-604. Both exit 2 with their number.
- **Gates:**
  - CTest through `run_selected.py`: 2 selected, 2 executed;
  - hosted CI green on all 12 jobs (run 43404729252).

## Notes (no fix round)

- **A merge note for the orchestrator.** WHALE-RE-1c also adds its CTest pair right after the wildlife pair in
  `CMakeLists.txt`. When both are merged, that conflict is resolved by keeping both pairs.
- **No new open question.** The reserved OQ-605 to OQ-608 are unused.

## Disposition

ACCEPTED. It lands with the next landing, on the owner's OK. The fisher reads 1a and 1b are complete. A flagged fisher
port is the natural next step; whether it needs a new state number, and so has to wait for ADR 0006, is checked
before it is written.
