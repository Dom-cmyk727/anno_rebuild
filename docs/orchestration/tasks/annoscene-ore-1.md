# ANNOSCENE-ORE-1: the ore and treasure types of island placement, recorded with our own evidence

**Status:** **Written and RELEASED** 2026-11-16 by the Claude Code orchestrator for **the Mac executor**. It follows
the co-developer's notes of 2026-11-15 and our reply of 2026-11-16 (`to-codev/reply-2026-11-16.md`).

**Base:** `origin/main` `4b104d3a` or later. Branch `task/ANNOSCENE-ORE-1`. About 1.5 hours. A read of files with a
reference check: no engine code, no pins. Never rebase, force-push or push `main`. No reviewer subagents.

## Why

[`annoscene.md`](../../re/annoscene.md) §5 step 10 has the ore table at `AnnoScene 0x1003AF24` (kind 0 → `0x67D`,
1 → `0x619`, 2 → `0x61A`, 3 → `0x61B`, 4 → 0, 5 → `0x61C`, 6 → `0x618`) and lists "what these ids are" and "the
meaning of `INFO` byte 5" as open. The co-developer reported that the ids are figure types of `FigurenHeader.dat`
and that the island's "objects" are its `OCCUR` markers, and that `INFO` byte 5 is the number of ore markers. His
notes are a lead, not evidence: his game files are a different build from ours. The orchestrator checked the first
claim once on our files (the six ids share one `BEWOHNTYPE` record; 1560 and 1661 share one `WARE` record; 1550 and
1551 share one `BEWOHNTYPE` record). This packet records it properly.

## Steps

1. **The ids, from our files (P):** for 1560 to 1564, 1661, 1550 and 1551 in `FigurenHeader.dat`, read `BEWOHNTYPE`
   and `WARE` with the project's reader and state what each names. Give the mapping from ore kind 0 to 6 to goods.
2. **The objects (P, and D where the note already has it):** show that the island objects the placement walks are the
   `OCCUR` `ENTRY2` markers: which field holds the figure type, and how that fits the slots step 10 reads
   (`+0x84`, `+0x78`, `+0x28`). If the link cannot be shown from files and the existing read, say what is missing.
3. **`INFO` bytes 4 and 5 (P):** over every template the candidate list uses on our copy, count the ore markers and
   compare with byte 5; report byte 4. State the number of templates, how many agree, and every exception.
4. **Our counts against his:** we have 465 `.scp` files below `Scenes/Islands` and counted 429 candidates; he reports
   464 and 428. Report our numbers again from the files and say which file accounts for a difference if it can be
   seen from our side. Do not assume his numbers.
5. **The note:** update `annoscene.md` §5 step 10 and its open list, with the evidence letters; name the
   co-developer's note as the lead. Add or extend a Python reference check that reads the ids' records and the
   marker counts from the game files (skipped without them), with its contract test.
6. **What stays open, listed plainly:** a marker that gets no ore, the tons of a placed deposit, the treasure's spot
   scan. Do not read them in this packet.

**Done:** the note, the reference check passing, the REPORTS entry of about 40 lines, READY FOR REVIEW. No original
bytes in git: names, ids, counts and hashes only.
