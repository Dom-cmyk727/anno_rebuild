# WHALE-RE-1b: the whale's life, the boat request and the open callbacks (documents only)

**Status:** PROPOSED 2026-11-13 by the Claude Code orchestrator for **the Mac executor**. It is released when the owner
gives the go. Step 6′ of the queue on the board ([`TASKS.md`](../TASKS.md), "Current work").

**Why:** [WHALE-RE-1a](whale-re-1a.md) ([review](../reviews/2026-11-13-whale-re-1a-review.md)) read:
- where whales come from;
- the whaler's tick, its boat queue, launch and hunt dispatch;
- the lamp-oil chain.

Three things a port still needs are unread:
- what a whale does once it exists;
- who asks for a whaling boat (OQ-593);
- the callbacks on the boat's path that may draw (OQ-594).

**Base:** `origin/main` at release. Branch `task/WHALE-RE-1b`. About 3–4 hours.

**Rules:** the owner rule of 2026-11-12 on the board:
- one concern;
- no reviewer subagents (the orchestrator reviews your handover and re-checks addresses);
- one REPORTS entry of about 60 lines, READY FOR REVIEW;
- documents only: no engine code, no tests, no fixtures.

The gate follows `TASKS.md` §1 item 4: docs only, so hosted CI.

## Start from

- [`docs/re/whaler.md`](../../re/whaler.md):
  - §2, the class tables: whale `0x100c5f98`, its route `0x100c7218`, the boat route `0x100c72a8` and the ship
    `0x100c4fc0`;
  - §3, creation and the slot release through cleanup `0x10050d70`;
  - §4, the boat queue `0x10073b40` and launch `0x10073d50`.

  Extend it in new sections; don't rewrite what is accepted.
- [`docs/re/wildlife.md`](../../re/wildlife.md) §4 and §7, the animals' tick, movement, death and walk draws. The
  whale shares 92 of 101 slots with the old animal class `0x100c653c`, so read only the slots that differ.

## Read (GOG `AnnoWelt.dll` 2.0.0.5; Capstone or Ghidra, as before)

1. **The whale `0x100c5f98`:**
   - its nine slots that differ from `0x100c653c`, each with its address and what it does;
   - its tick: how it moves in the sea, how fast, and where it may go (depth, island bounds, its source's area);
   - what ends it: hunted, old age, or something else;
   - every world-PRNG draw on these paths: position, modulus, what it selects, the condition. Where a shared animal
     routine already read in `wildlife.md` is reused, cite it, and don't re-read it.
2. **The whale's route `0x100c7218`:**
   - its two slots that differ from `0x100c7348`;
   - its jobs, and the draws they make.
3. **The boat request (OQ-593):**
   - every caller of the enqueue `0x10073b40`, in AnnoWelt and in the other modules (AnnoGame, AnnoFrame, View);
   - the command or UI path that reaches it;
   - what it charges;
   - what happens when it refuses: count ≥ 1, no money, no goods;
   - whether a lost boat is ever requested again automatically.
4. **The open callbacks (OQ-594):** the boat's movement and path callbacks on route 45 (the return job's start
   `0x10056870` and the ship class's movement slots).
   - Say for each whether it draws: yes with the row, or no with its address range read.
   - Close OQ-594 where the read settles it, and narrow it where it does not.

## Deliver

- **New sections in `docs/re/whaler.md`**, one per read, with addresses and evidence tags as in 1a. No disassembly
  text, no bytes.
- **The draw table extended**, so that the whale's life and the boat's path have every draw, or a stated "none" with
  the range read.
- **The port list** in §5 updated.
- **Open points:** new entries **OQ-595 to OQ-602**, reserved for this packet. Update OQ-593 and OQ-594 with dated
  lines; don't rewrite them. Nothing is invented: when a read does not settle a rule, record what is known and stop
  that item.

## Gate

Docs only:
- `git diff --check`;
- links that resolve;
- hosted CI green on your pushed head.

Read CI after every push.

## Out of scope

- the reference script and its vectors (a later packet);
- the History Edition reading (NOT_RUN on the Mac);
- any engine code (the port, after ADR 0006);
- the native WALSPECK records 3501 and 3515 (a separate question in the 1a review).

## Report

READY FOR REVIEW, about 60 lines:
- the reads done, with their addresses;
- what is settled;
- the draw table's new rows;
- the open points, with their OQ numbers;
- NOT_RUN;
- Decisions and reasoning;
- QUESTIONs.
