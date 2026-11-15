# FISHER-1d, the runtime fish fill and the 1b follow-ups: review (2026-11-15)

**Reviewer:** the Claude Code orchestrator, reading the code and tests itself (no subagents).
**Head:** `task/FISHER-1d` `e006a45b`. Tested source `db74a7cb`; the closeout diff to `e006a45b` is docs only
(REPORTS and `fisher.md`).
**CI:** run 43695930886 on `e006a45b`, every job green, `gate` included.
**The branch carries:** FISHER-RE-2 (`568c8788`), FISHER-1c's checkpoint (`eb40d75c`), FIXTURES-TRIM-1 (`7aaa3c85`,
merged as `66508d93`) and the new FISHER-1d commits.

## Verdict

**ACCEPTED, with two findings.** The runtime fish fill is ported behind the existing default-off flag, the five 1b
findings are fixed, and the script inventory answers the owner's option 1 honestly: it shows that the S01_02_01 canary
is much further away than the two city objects.

## What was checked

- **B1, the candidate count** (`harvest.cpp`): `candidates.insert(t)` now comes before the fisher's usable-mask
  test, as the cited `0x100592f4`–`0x100592fc` (count) and `0x100033c3` (goal-pop) order says.
  - Only the fisher branch has a `continue` between the old and new positions, so forestry and every other harvest
    job are unchanged.
  - A new test with truly no fish asserts 0 candidates and 0 popped nodes. The blocked-only rig keeps its search.
- **The fill** (`fill_fish_resources`, `fisher.cpp`):
  - it clips the area to the island and runs row-major;
  - it skips any cell with a nonzero object id, and fills a cell whose four-corner mean is strictly below 16;
  - it writes object 11, footprint offsets 0, rotation 0, phase 0 and the mean as the item height;
  - it keeps the anchor, owner, claim and walk data;
  - it takes no draw and adds no state. Off, or without a full HEIGHT column, it writes nothing.
- **Where it runs:**
  - at the end of `build_island` in `World::create`, after objects and buildings;
  - in `clear_object_record`, over the cleared footprint, but only when a terrain pointer is passed. Every caller
    passes it only with the flag on: demolish, city removal, building removal and house removal.
  - Placement, ticks and native reload do not refill, as the RE says.
- **B2:** a rig whose first entrance stands on land (mean 32) reaches the water, claims and cuts a fish, and comes back
  through a second entrance on water (mean 31), delivering 768.
- **B3:** the rotated launch now checks a hand-derived literal. The mode-0 test is renamed for what it shows: that the
  default and the explicit Land selection agree.
- **B4:** the mid-trip reload now runs to worker release and asserts the 768 delivery. Two runs from `create` give
  equal hashes and equal saves after a completed trip.
- **B5:** with the flag on, the refusal message names job 0x2d; with it off, it does not.
- **T2:** the stale `goldens.txt` mentions are corrected. The 14 recordings are byte-identical to the trim.
- **Compatibility:** state 139, every section version and header tag 1 are unchanged. The full pins selection
  (1,693 selected, 1,675 executed, 18 opt-in skips) moved 0 pins.
- **The census:** the C++ test over all 50 maps matches the RE-2 count fixture: 4,908,270 cells, 179,016 of them on
  S01_02_01.
- **The new threshold literal 16** is in `lint_sim_allow.txt` with its address, as a parameter default.

## Findings

1. **F1, a return through only closed entrances is not proved.** The executor's first rig had both entrances on land.
   The fisher reached the fish and cut it, but did not deliver within the bound. The accepted rig has one entrance on
   water. A real coastal fishery may have all its entrances on land. That would be the same gap as 1b's B2, one step
   later on the way home. The executor named this and invented no movement rule, which is right. It must be read
   in the original (how the walk home ends on a closed entrance) before any real-map claim. It goes into the next
   fisher code packet.
2. **F2, the canary is much further away than option 1 assumed.** The script inventory (`fisher.md` §8) shows that
   S01_02_01 needs more than the two `OBJECT_STADT` bindings:
   - five PLAYER queries;
   - eleven action names the compiler refuses today.

   Its INIT block begins with one of them (`QUESTLOG_SHOW`). So the proposed SCRIPT-OBJECTS-1 (3–4 h) is real
   progress, but it does not unblock the canary. A script-language plan would have to size the rest first.
   **That is an owner decision** (below). The orchestrator did not re-read the original addresses for the binding;
   SCRIPT-OBJECTS-1's step 1 does that.

## The owner's decision (OQ-625, again)

The owner chose option 1. Its own inventory now shows the cost:
- **Option 1a, keep option 1:** SCRIPT-OBJECTS-1 first, then a script-language plan for the other queries and
  actions, then the canary. The real-map fisher proof waits for all of it.
- **Option 1b:** do SCRIPT-OBJECTS-1 as a script step on its own merits, and meanwhile allow option 2's narrower
  canary, without the script, for the fisher. It does not claim the original map works.

## Landing

Candidate `7bc3ea79` is `b47f77e4` (the seven tested packages) with `task/FISHER-1d` `e006a45b` merged `--no-ff`.
- The `open-questions` and REPORTS conflicts were resolved by keeping both sides. One FIXTURES-TRIM-1 line was put
  back in its own section with the new number.
- Its `src/sim`, `tools` and `fixtures/synthetic/sim` equal the branch's.

Its game-file run is in progress. Landing waits for the owner's OK.
