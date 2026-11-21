# FISHER-1f: the sim follows the read for a hut with every entrance on land

**Status:** **Written and RELEASED** 2026-11-16 by the Claude Code orchestrator for **the Mac executor**, after
FISHER-RE-3's review ([review](../reviews/2026-11-16-fisher-re-3-review.md)).

**Base:** `origin/task/FISHER-RE-3` `5901b846` (accepted, not landed), on main `67a960c9`. Branch `task/FISHER-1f`.
About 2 to 3 hours. Never rebase, force-push or push `main`. No reviewer subagents.

## Goal

With the fisher flag on, a type 810 or 3978 hut whose entrances are all closed behaves as
[`fisher.md` §10.2](../../re/fisher.md) reads the GOG original. With the flag off, nothing changes.

## Steps

1. **First measure what the sim does today** on the real site (`Tutorial/02Trade01`, player 2, one Build of type
   810 at (242, 217), rotation 1) and report it against §10.2, stage by stage. Change only what differs.
2. **The rule**, each line from §10.2, with the spec section cited where the code implements it:
   - the worker is dispatched (one discarded draw per eligible attempt, as today);
   - a closed start may expand out to an admitted water cell; no route is promised;
   - outside the hut, a closed entrance is never an admitted goal: the home search fails, no walk home is
     installed, and the position stays where the last catch or search left it;
   - the failed home job credits no raw stock; the cargo is neither delivered nor dropped;
   - the worker waits and is released; the hut's worker slot is freed;
   - a worker already inside the footprint completes without a search and credits its cargo.
3. **Time and float:** §10.2's wait (15.0 at rate 0.01, nominally 7500 ticks) carries the existing float and visit
   qualification (OQ-603). Use the sim's existing integer wait machinery for released workers. If the nominal
   figure cannot be reproduced exactly with it, say so and ask a QUESTION. Do not add float state.
4. **Tests** that fail on the base and pass on the branch:
   - the five `closed_home` vectors of `fixtures/synthetic/sim/fisher_entrance_vectors.txt`, driven through the
     C++ sim;
   - the real site: the worker leaves, catches, fails to return, the raw stock stays 0, the worker is released,
     the slot is free, and a second worker can be sent later;
   - the open control at (243, 216), rotation 0, is unchanged (FISHER-1e's canary still passes);
   - determinism, a save and reload in the middle of the failed trip, and flag-off identity, as in FISHER-1e.
5. **No hardcoded limits; no new saved state** unless the wait needs it. If it does, stop and ask a QUESTION
   before adding a field or a version.
6. **Scope:** types 810 and 3978 (figure 142, route 45) only. Type 3510 (figure 393, route 44) is not touched.

**Done:** the code, the tests, `fisher.md` updated, 0 pins moved with the flag off, the REPORTS entry with exact
gate lines, READY FOR REVIEW. The History Edition is not claimed; FISHER-HE-1 compares the pictures.
