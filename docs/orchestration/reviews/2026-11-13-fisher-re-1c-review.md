# FISHER-RE-1c review (2026-11-13)

**Verdict: ACCEPTED after one fix round** (round 2, at the end). Round 1's verdict follows.

**Round 1 verdict: FIX ROUND 1, small.**
- **The read is right.** HEIGHT's width × height bytes are copied unchanged into the width + 1 corner grid, and the
  right and bottom border corners stay 0. That answers OQ-200's load mapping.
- **The census asked the wrong question,** and the fault is upstream, the orchestrator's included. The fisher walks
  on cells whose corner mean is **below** the threshold, and harvests tiles whose mean is **at or above** it. So "0 of
  2,473 fish tiles are water" is the expected result, not a contradiction.
  - FISHER-RE-1a's prose had the goal mask's sense backwards.
  - FISHER-RE-1b's reference encoded it that way.
  - The orchestrator accepted both.

**Reviewer:** the Claude Code orchestrator (Opus 5.5).

**Reviewed head:** `task/FISHER-RE-1c` `1d6d289b`:
- `docs/re/fisher.md` §5 (+140 lines);
- OQ-609 and OQ-610;
- `tools/fisher_ref.py` with its `corner_grid` record and the `--check-fish-water` corpus check;
- 49 vectors, 71 contract checks, 11 mutants.

CI green on all 12 jobs (run 43428548264).

## What the orchestrator checked in the original (GOG `AnnoWelt.dll`, Capstone)

- **The walking cells,** mode 3, `0x10090560`:
  - at `0x10090689` the four-corner mean (`bl`) is compared with the threshold byte (`al`, from `[0x100e34dc]`);
  - `sbb`, `and 0xfff90fff`, `add 0x70000` give `0x00000fff` when **mean < threshold** (code 0, walkable for layer
    2) and `0x00070000` otherwise (code 7, closed).

  This is as `fisher.md` §3 states.
- **The goal mask,** mode 3, `0x1009226b`–`0x100922df`:
  - the same mean against the same `[0x100e34dc]` threshold;
  - `sbb`, `and 0xfffffe01`, `add 0x1ff` give **0 when mean < threshold** and **511 when mean ≥ threshold**.

  So a tile is a fisher goal only when it is **not** water. §3's "returns 0 below 32 and 511 otherwise" is right; its
  next words, "below-water goals have all nine free subcells", are the wrong way round.
- **The executor's own worked island fits the corrected rule.** In `Single_06.sze`, island 12:
  - the fish tile (69,66) has mean 32, so it is a goal;
  - its neighbour (70,66) has mean 18, so it is water.

  That is exactly a fish tile reached from water.
- **The corpus result fits it too:** every fish tile's mean is 32–96, so all 2,473 are goals.

## Findings

- **F1: the census criterion.**
  - **Replace** "the fish tile classifies as water" with the rule the code implies:
    1. the fish tile is a **goal**: mean ≥ threshold, so goal mask 511;
    2. at least one **walkable water cell**, mean < threshold, lies within reach of it.

       State the neighbourhood you check, 8-neighbours or the searcher's goal radius, and why.
  - **Report:** both counts over the 2,473, and every exception.
  - **OQ-609:** close it if the exceptions are none or are explained, otherwise narrow it to the real exceptions.
- **F2: the reference's `sea_cell` record** (on `main` since `f9e6ea0c`) gives `free = 511` below the threshold and 0
  at or above it. That is inverted.
  - Correct it to `open = (mean < threshold)` and `free = 511 if mean ≥ threshold else 0`.
  - Re-derive the four `sea_cell` vectors by hand.
  - Add a contract mutant that inverts it again.
- **F3: the prose.** Correct `fisher.md` §3's sentence with a dated note, without rewriting the rest; this review gives
  the addresses. Check §4's water and goal wording for the same slip.
- **Kept as is:**
  - §5's mapping and its addresses;
  - OQ-200, answered;
  - OQ-610, later writers;
  - the `0x100928a0` note on runtime fish creation on low empty ground, a real lead for the port.

## Disposition

Fix round 1 to the Mac executor, through the owner's prompt, on the same branch.

## Round 2 (reported 2026-11-13)

**Reviewed head:** `e7e909ff` ("distinguish fisher goals from water approach cells"). CI green on all 12 jobs (run
43439928949).

- **F2, fixed.** The six `sea_cell` vectors now read `open = 1, free = 0` below the threshold and `open = 0,
  free = 511` at or above it, as the original's two functions give. A contract mutant inverts it again and fails.
- **F3, fixed.** `fisher.md` §3 and §4 carry dated corrections with both address ranges. A fish goal and the water
  cell used to approach it are distinct cells.
- **F1, the census redone.** The orchestrator re-ran the scripts on this PC:
  - the reference: 49 vectors, 0 disagreements;
  - the contract test, with the branch's `tools/`: 75 checks passed;
  - the corpus check on this PC's game files: **50 scenarios, 2,473 fish tiles, all 2,473 goals, 619 with a walkable
    water cell among their eight neighbours, 1,854 without.**
- **What the 1,854 mean.** They sit inside fish banks, every neighbour shallow. The fisher reaches a bank's edge tiles
  from deeper water. Whether the inner tiles are ever reached is open: the searcher's own reach, a regrowth that
  exposes them, or never. OQ-609 is narrowed to exactly that, and it is the fisher port's question.
- **A note:** the corpus check exits 1 as long as there are exceptions. It is an opt-in census, not a CTest gate, so
  that is fine. A port's canary should assert the 619 and report the 1,854.

**ACCEPTED.** It lands with the next landing, on the owner's OK. OQ-200 is answered. The fisher reads are complete:
- **next on the Mac:** PINS-1 (ADR 0006);
- **next for the fisher:** FISHER-1a, the water cells and goal mask as unwired sim functions.
