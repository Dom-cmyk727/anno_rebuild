# FISHER-RE-1a review (2026-11-13)

**Verdict: ACCEPTED, no fix round.** Every table, address and record the orchestrator re-checked matches. The read
changes what lifting `main`'s refusal of route `0x2d` takes:
- **The fisher is not a boat that hunts.**
  - Figure 142 is a *walking worker* (kind 4) on a three-node resource route: search, timed harvest, return.
  - It cuts **fish tiles**: Anlagen 11, which turns into 10 and regrows to 11 through the ordinary growth rule.
  - These are the same machinery as the forester and the route-`0x2e` animals that `main` already runs.
- **What a port must add:**
  - the launch at the first rotated entrance;
  - a sea search over "mode 3" cells: a corner-height mean below 32 is water;
  - movement that keeps its height;
  - one world draw at dispatch whose value is thrown away;
  - the fisher's figure kind.

  Fish tiles are already imported (2,473 placements in 24 scenarios). No fish population and no new importer are
  needed.

**Reviewer:** the Claude Code orchestrator (Opus 5.5); the package's independent review under the owner rule of
2026-11-12.

**Reviewed head:** `task/FISHER-RE-1a` `6d57bb12`. Changed: `docs/re/fisher.md` (new, 356 lines),
`docs/open-questions.md` (OQ-603, OQ-604) and the REPORTS entry (61 lines). Documents only.

## What the orchestrator re-checked

Read directly from GOG `AnnoWelt.dll` (pefile and Capstone) and this PC's `Anlagen.dat`:
- **The fisher's table `0x100c2ac4` against Farm `0x100c8ae0`,** 75 slots: exactly two differ.
  - `+0x110`: `0x100254e0` against `0x1006f140`;
  - `+0x118`: the creator `0x10067e60` against `0x10072d20`.
- **The kind-4 worker table `0x100cb4d8` against the worker table `0x100c4950`,** 101 slots: exactly four differ,
  at `+0x7c`, `+0x80`, `+0xc8` (the search, `0x10038ad0`) and `+0x17c` (the movement, `0x10038730`).
- **The route factory `0x1003b0e0`:**
  - job 45 minus 13 indexes the byte table `0x1003b480`, which gives 6;
  - entry 6 of the jump table `0x1003b450` is `0x1003b314`, the resource-route arm;
  - job 65 gives selector 10, as §3 says.
- **The dispatch draw:** at `0x1006ee06` the Farm tick calls the world's `+0xb4`, and the next instruction overwrites
  the result, so its value is discarded. The call at `0x1006ee15` is the type's list getter, not a draw.
- **The data:**
  - Anlagen 810 and 3978: both FISCHER, 3×4, radius 7, flags `0x2087`, capacity 5 (1,280), figure 142, NAHRUNG ←
    FISCH;
  - Anlagen 11 is a ROHSTOFF tile with WARE FISCH, and 10 its ROHSTWACHS stage.

**Not re-checked** (taken on the executor's reading):
- the mode-3 cell builder and its threshold of 32;
- the creator's launch position;
- the dispatch admission `384/rate ≥ raw/need`;
- the scenario census (2,473 tile-11 placements, no fish occurrences);
- the movement body.

**CI:** docs-only CI green on `6d57bb12` (run 43401635634).

## Notes (no fix round)

- **The full store does not stop the fisher.** Output at capacity blocks conversion, but dispatch has its own
  admission rule. §4 gives the exact boundaries, raw 1,023 against 1,024 for 3978, and a port must keep them.
- **The draw closure is honestly left open** (OQ-604), as for the whale. The direct draws are exact. The
  callbacks of the manager, the type, the search and loading are not certified zero.
- **OQ-605 to OQ-610 are unused.**
- **For the board, a port candidate.** With this read, a flagged fisher port, default off, looks feasible on the
  existing harvest machinery. It may also need no new state, because it reuses the route-`0x2e` worker state. That
  has to be checked before it is queued, since no new state number is allowed before ADR 0006.

## Disposition

ACCEPTED. It lands with the next landing (PLAY-LIVE-1a, WHALE-RE-1b and 1c) on the owner's OK.
