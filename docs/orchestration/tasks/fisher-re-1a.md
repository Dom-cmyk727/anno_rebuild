# FISHER-RE-1a: the fisher's route 45 and the fish it catches (documents only)

**Status:** ACCEPTED 2026-11-13 ([review](../reviews/2026-11-13-fisher-re-1a-review.md)); released with the owner's go to **the Mac executor**. Step 8′ of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Why:** food is the largest single blocker in the progression measurement. 131 houses fail first on food
([`docs/sim/progression-gaps.md`](../../sim/progression-gaps.md), row 2), and one reason is named there: "the fisher
route 0x2d is a **typed refusal**".
- The shipped fisher farms 810 and 3978 send figure 142 on route 45 (job `0x2d`), which in the original "shares the
  animal case" ([`docs/sim/early-needs.md`](../../sim/early-needs.md), item 10).
- `main` refuses to create such a figure, and the progression measurement says to keep that refusal "unless its
  original body is read" (its step 3).

The whale reads have just mapped a route-45 class: `tRouteWalfaenger` `0x100c72a8`, built by `0x1005df70`, with its
four shared hunt jobs and a return job ([`docs/re/whaler.md`](../../re/whaler.md) §2, §4, §9). This packet reads what
the fisher does with route 45, so a later port can lift the refusal on evidence.

**Base:** `origin/main` at release. Branch `task/FISHER-RE-1a`. About 3–4 hours.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover and re-checks addresses);
- one REPORTS entry of about 60 lines, READY FOR REVIEW;
- documents only: no engine code, no tests, no fixtures.

The gate follows `TASKS.md` §1 item 4: docs only, so hosted CI.

## Start from

- `docs/re/whaler.md`:
  - §2: the classes;
  - §4: the whaler's dispatch, which uses route 45 on a ship;
  - §9: the boat's callbacks.

  WHALE-RE-1b and 1c are accepted and land with the next landing. Until then, read them on `origin/task/WHALE-RE-1b`
  and `origin/task/WHALE-RE-1c`.
- [`docs/re/hunter.md`](../../re/hunter.md) and [`docs/re/wildlife.md`](../../re/wildlife.md) §8: the shared
  approach, fire, corpse-walk and take jobs.
- [`docs/re/production.md`](../../re/production.md) and [`docs/sim/state.md`](../../sim/state.md): the Farm class
  (ctor `0x1006f160`, tick `0x1006ea90`; FISCHER has its table overwritten to `0x100c2ac4` with the same tick), which
  `main` already runs for other farms.

## Read (GOG `AnnoWelt.dll` 2.0.0.5; Capstone or Ghidra, as before)

1. **The data** (`tools/figuren.py`, `tools/anlagen_roads.py`):
   - Anlagen 810 and 3978: PRODTYPE, inputs and outputs, capacity, figure count, figure;
   - figure 142: its kind, route type, capacity and whether it is a walker or a boat;
   - what it catches. Name every fish source the data has: a figure type, an occurrence (as `WALSYMBOL` was for the
     whale), or tiles. Say which of these the shipped scenarios contain, and whether `main` imports them.
2. **The fisher's class:**
   - what the FISCHER table `0x100c2ac4` overrides against the Farm table;
   - how its tick dispatches figure 142 on route 45, which is the "animal case";
   - every draw on that path.
3. **Figure 142's route 45:**
   - its class and its `+0xa4` route factory: is it `0x1005df70` (`tRouteWalfaenger`), or another route-45 class?
   - its jobs in order;
   - what it targets, and how a catch is credited to the farm's store and becomes food;
   - how it returns;
   - every draw. Cite the whale and hunter reads wherever the code is shared, and don't re-read it.
4. **What stops it:** no fish in range, a full store, no sea. Say what the original does in each case.

## Deliver

- **A new `docs/re/fisher.md`**, one section per read, with addresses and evidence tags as in `whaler.md`. No
  disassembly text and no bytes. Link `whaler.md`, `hunter.md` and `wildlife.md`; don't edit them.
- **Every draw on the fisher's path as a table** (position, modulus, what it selects, the condition), or a stated
  "none" with the address range read.
- **The list of what a port needs to lift the route-45 refusal,** and which of it is already on `main`. Include the
  fish sources' import.
- **Open points** as new entries **OQ-603 to OQ-610**, reserved for this packet. Nothing is invented: when a read does
  not settle a rule, record what is known and stop that item.

## Gate

Docs only:
- `git diff --check`;
- links that resolve;
- hosted CI green on your pushed head.

Read CI after every push.

## Out of scope

- any engine code (lifting the refusal is a later, flagged port);
- a reference script;
- the History Edition reading (NOT_RUN on the Mac);
- the hunter and wildlife port.

## Report

READY FOR REVIEW, about 60 lines:
- the reads done, with their addresses;
- what is settled;
- the draw table;
- the port list;
- the open points, with their OQ numbers;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
