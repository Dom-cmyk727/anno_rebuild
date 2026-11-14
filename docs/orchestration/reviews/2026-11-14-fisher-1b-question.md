# FISHER-1b's QUESTION: the goal mask is a blocked mask (2026-11-14)

**Verdict: the executor is right.** The orchestrator re-checked every cited address in GOG `AnnoWelt.dll` with Capstone
(`tools/disasm.py`'s method) on this PC. The port contract is corrected below, and FISHER-1b continues with it.

**The checkpoint:** `task/FISHER-1b` `89922b18` (documents only; docs CI green, run 43632765965).
- RE notes in `fisher.md` §3 and §4.
- The owner's draw-scope decision.
- No engine, reference, vector, fixture or pin change.

## The orchestrator's check

| Address | What it does | Confirmed |
|---|---|---|
| `0x10002b80`–`0x10002b9e` | search interface `+0x50` stores its third argument at `S + 0x2040` and the island at `S + 0x434` | yes |
| `0x10038b91`–`0x10038ba1` | the fisher's setup pushes **3** and calls `+0x50` | yes: mode 3 |
| `0x10059313`–`0x1005932d` | the fisher's target search calls `+0xc8` (that setup), then `+0x78` (the resource goals), then sets the limit `0x14` = 20 | yes |
| `0x10003399`–`0x100033bd` | goal-pop: on a popped tile equal to a stored resource pair, call island `+0x12c(S + 0x2040, x, y)` | yes |
| `0x100033c3`–`0x100033ce` | **`mov ecx, 0x1ff; sub ecx, eax; … or eax, ecx`**: the usable bits are 511 minus the returned mask | yes |
| `0x1000370c`–`0x1000374c` | the callback loop tests each accumulated bit (`and eax, edx; test ax, ax; je`) before calling the callback | yes |
| `0x1009226b`–`0x100922df` | island mode 3: `cmp bl, al; sbb; and 0xfffffe01; add 0x1ff` gives 0 when mean < threshold, 511 otherwise | yes |

The mode-0 arm just before it (`0x10092258`–`0x10092268`) returns a 9-bit sub-cell mask from the cell's high word
(`>> 16 & 0x1ff`). The same consumer inverts it too. So island `+0x12c` answers "which sub-cells are **blocked**",
and the consumer turns that into "which are usable". Which mode-0 bits mean what was not re-read here.

## The corrected contract

- **The island's mode-3 answer is a blocked mask:** 0 below the threshold, 511 at or above it.
  - FISHER-1a's `FisherCell::goal_mask` and the reference's `sea_cell` `free` hold this value under the wrong name.
  - The values are right, and the names change.
- **A fisher's usable resource-goal sub-cells are 511 minus that:** 511 on a water tile (mean < 32), 0 on a tile with
  mean ≥ 32.
- **So a fish tile is catchable only if it is itself a water tile** by the four-corner mean.
- **The orchestrator's FISHER-RE-1c review was wrong** in reading 511 as "a goal". The census numbers stand. Their
  meaning changes:
  - all 2,473 embedded tile-11 placements have a mean of 32–96, so **none is a usable goal as loaded**;
  - the 619 / 1,854 split describes shoreline geometry only.

## What changes in FISHER-1b

1. **Rename, values unchanged.**
   - `FisherCell::goal_mask` → `blocked_mask`, and add the consumer's `usable_mask = 0x1ff − blocked_mask`.
   - The reference's `sea_cell` output `free` → `blocked`, plus `usable`.
   - The vectors' expected keys follow. This is a reviewed oracle edit: same inputs, same raw values, the new key
     added and derived by hand.
   - FISHER-1a's corpus test keeps its counts, with the meaning corrected in its name and comment: 2,473 tiles blocked
     in mode 3, 0 usable.
2. **The search** uses the usable bits, as the original's goal-pop does (`0x100033c3`).
3. **The synthetic bay:** fish tiles 11 on **water** tiles (mean < 32), reachable over walkable water. Also one control
   fish tile with mean ≥ 32, which must never be claimed.
4. **`fisher.md`:** dated corrections in §3 and §4 (the "a closed walking tile can be a fish goal" wording), and a note
   on the FISHER-RE-1c census's meaning.
5. **The draw scope** is the owner's decision, already recorded on the branch:
   - one discarded draw per eligible dispatch attempt;
   - no fisher-body draw;
   - trajectories may diverge between flag on and off.
6. **The time budget** continues from the checkpoint. If the packet passes 4 hours, stop at a step boundary and
   report.

## What this means for real maps (next on the fisher path)

On the shipped maps, the embedded fish tiles are not catchable as loaded. The fisher in the original evidently
catches something, so one of these must hold:
- the runtime fish creation on low empty ground (`0x100928a0`, mean below 16, the lead in `fisher.md` §5) makes the
  catchable fish;
- a later corner writer changes the heights (OQ-610);
- the embedded placements are reached some other way.

**FISHER-RE-2** (a read, about 2–3 h) settles it before FISHER-1c. The real-file canary on `S01_02_01` would
otherwise catch nothing. It also answers whether `lan5`, which has no embedded fish, gets fish at runtime.
