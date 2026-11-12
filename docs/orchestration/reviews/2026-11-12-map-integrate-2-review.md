# MAP-INTEGRATE-2 review (2026-11-12)

**Verdict: ACCEPTED for landing, pending the Windows game-file run at the merge.** The gameplay code is sound
enough for `main`. The cost of the package was out of proportion to its result. The main causes are the packet's
design and the project's state-identity and fixture scheme, so the follow-ups below are small packets, and the
next state change waits on a redesign of that scheme.

**Reviewer:** the Claude Code orchestrator (Opus 5.5), with an independent read-only expert pass (a fresh agent
briefed as a senior game-engine programmer, given the code, not this review's conclusions). The orchestrator
verified every finding below that is marked "verified" in the code at the reviewed head.

**Reviewed head:** `task/MAP-INTEGRATE-2` `55ce7e71`, branched from `main` `03bf2b3c`. 230 commits, 30 of them the
executor's own (first parent); the rest are the three accepted inputs MAP-EXTRACT-1 (135), MAP-EXTRACT-2 (137) and
MAP-ROUTE-1 (136). Hosted CI green on the head (run 43124177290). A trial merge with `main` conflicts only in
`docs/open-questions.md` and `docs/orchestration/REPORTS.md`; `main` changed nothing in `src/sim` since the base.

The owner asked three questions (2026-11-12). This review answers them in order.

## 1. Did the executor follow the project's goals?

| Goal (CLAUDE.md, PLAN §0) | Verdict | Evidence |
|---|---|---|
| Determinism | **Yes** | No floats, ordered containers, overflow checks, validate-then-commit commands; a test proves a standing mine adds no draws. |
| Explicit serialization | **Mostly** | Field by field. But `goal_col`/`goal_row` are written only for mason jobs, so `field_count<HarvestState>() == 9` (`state_fields.hpp:1197`) no longer matches what a non-mason writes (`:1349`); the guard no longer catches a missing field. |
| No hardcoded limits, moddability | **Partly** | Original values sit in code: quarry and mason constants (`mason.hpp:22-28`), the deposit reserve (`mine_source.cpp:10-11`), ore-symbol type 145 in three files, job ids 0x29/0x2a/0x2c/0x2e. Freight reads the ship kind from a default-constructed `veto::Rules{}` (`freight.cpp:29`, `217`, verified), as manual cargo and founding already do (`ship_cargo.cpp`, `founding.cpp`): the world carries no veto rules, so none of it is configurable yet. |
| Faithful core separate from QoL | **Yes** (owner, 2026-11-12) | A successful manual Move, Stop, Load, Unload or Found cancels a ship's automatic route (`apply.cpp:945-956`). The spec called it "modern control ownership, not original parity" but cites no code for the original's manual click, which it marks unread. **The owner confirmed it is the original's in-game behaviour** (2026-11-12), so it stays always on; the spec is relabelled. |
| Robust engine performance | **Yes** | No new per-tick scan over all buildings, figures or tiles. Freight work per visit is capped; the ore lookup runs at placement and on a short step only. |
| Robust, maintainable codebase | **Partly** | See §2: two write paths for city stock, sim logic reading the UI event channel, and hand-copied bounds that already drifted (verified: `Reject::NoDepositInReach` was appended after `CityAccounting`, but the freight bound at `freight.cpp:222` and `state_fields.hpp:1309` still stops at `CityAccounting`). |

The executor followed the packet faithfully. Where the result misses a goal, the cause is in an input package
(the constants) or in what the packet asked for (the fixture copies, §3).

## 2. An expert game developer's assessment

**What an experienced lead would accept.** The gameplay code is correct and careful: conservation ledgers
(`made == in transit + in stores`), reload-equals-uninterrupted checks at chain boundaries, cost-scaling tests with
operation counters, stale-order protection by order ids and generations, independent Python oracles, and original
addresses cited in the code. The authored cross-feature world (`integration_cross_feature_test.cpp`) is a
proportionate, valuable test. A lead would merge this code.

**What a lead would flag.**

| # | Severity | Finding |
|---|---|---|
| E1 | High (structural) | **One global state number plus full-save replay goldens.** Any layout change, even one optional field, re-records about 1.87 MB in 17 files, and two parallel features can't combine without a new "union" number and a full regeneration (this package). `fixtures/synthetic/sim/` is 33 MB, with a near-identical copy per identity 133–138 plus 10.3 MB of `integrate1/legacy`; tests read only a header's worth of most of it. Integration cost grows with every concurrent feature. |
| E2 | Medium | **Freight writes warehouse stock directly** through `ShipCargo` with a mutable `World&` (`scheduler.cpp:646`, `freight.cpp:699-709`), while carts and masons queue stock changes as effects (`mason.cpp:368`). Deterministic only because the figure loop is serial; it blocks a later parallel figure phase. |
| E3 | Medium | **Freight's sim logic reads the UI event channel** (`fx.ui`, `freight.cpp:496-503`, `537-544`) to see arrivals. A change to UI events could change the simulation. |
| E4 | Medium | **A stale derived index fails the route** (`freight.cpp:626-629`, `664-667`) instead of rebuilding; it is rebuilt only in `StateIO::load`. A future resync or snapshot path that skips the rebuild would make peers diverge. |
| E5 | Medium | **Hand-copied bounds.** The route wire-size formula appears in four places; enum upper bounds are literals and already drifted (verified, see §1). Harmless today; the next appended reject a route stores would make valid saves fail their load checks. |
| E6 | Resolved | **The manual takeover** (§1) looked like non-original behaviour always on. The owner confirmed it is the original's (2026-11-12): no switch needed. |
| E7 | Low | Original values in code (§1), the quarry's payload decided in code while the class table says Inert (`building.cpp:70`), freight fields saved for every figure (about 9 bytes each, walkers included). |
| E8 | Low | A `Deposit&` held across a relookup that can grow the same vector (`production.cpp:771`). Not used after the relookup today (verified); a one-line edit would make it undefined behaviour. |
| E9 | Note | Freight is heavily engineered for a two-stop loop (resumable per-hold scan with saved cursors, a heap index): about 1k lines. Correct, but costly to maintain. |

**On the process, a lead would also say:** 13 hours and nine independent reviewer runs to merge three
already-reviewed branches, with about 30 lines of the executor's own product code, is ceremony, not engineering.
Hosted CI was red on every pushed head from I1 to I4. On the I4 head (run 43038889023) that was `cpp-format`
and five identity-derived sim tests on macOS (`write_log matches the documented frame byte for byte`,
`state_hash relational properties`, `world core stream layout round trip and rejects`, …); the Linux jobs failed on
an apt mirror outage. CI first reported the test failures at about 12:56 on the first 138 head. They were fixed at
18:39, after the Mac whole-suite run at I5 found the same class again. Reading CI would have saved hours.

## 3. Was the outcome worth the time?

**Where the 13 h 19 min went** (from commit times, Mac local time; the packet was released at 11:42):

| Part | From | To | Time | Share |
|---|---|---|---:|---:|
| S, P, I0, I1 (share, preflight, contract, the two merges) | 11:42 | 12:22 | 0:40 | 5 % |
| I2 (state 138, refusals, oracles, golden re-recording) | 12:22 | 13:41 | 1:19 | 10 % |
| I3 (cross-feature proof) | 13:41 | 14:05 | 0:24 | 3 % |
| I4 (re-run every input's real-data proof) | 14:05 | 15:28 | 1:23 | 10 % |
| **I5 (frozen whole-suite gates on the Mac, then CI)** | 15:28 | 23:45 | **8:17** | **62 %** |
| R (OQ-556 from the original's code) | 23:45 | 00:22 | 0:37 | 5 % |
| N (progression gaps; its long runs overlapped I5) | 00:22 | 00:48 | 0:26 | 3 % |
| E (final review, handoff) | 00:48 | 01:01 | 0:13 | 2 % |

I5 was mostly wall-clock test time: the whole Debug suite with game files (2,441 selected, 9 failed on stale
identity expectations, then re-pinned), the whole suite again without game files (once refused by the sandbox,
then rerun), the affected ASan/UBSan selection (323), a GCC 13.3 container build of all 392 `.cpp` files twice,
and a reviewer's reruns.

**What it produced:**

| Output | Value |
|---|---|
| Salt, iron → tools, quarry → bricks and automatic freight combined on one identity, ready for `main` | **High**, but the features were already built and reviewed; this package made them mergeable. |
| `docs/sim/progression-gaps.md`: three towns measured, 317 houses, first failing predicate ranked (goods 162, food 131, materials 18, infrastructure 6) with original addresses and three packet outlines | **High**: the next gameplay packets come from it. |
| OQ-556 answered from the original's code | **Medium** |
| `docs/sim/windows-consumer-delta-138.md` | **Medium**, for the Windows app. |
| The executor's own product code | About 30 lines: the version bump and its comment, test registrations, merge resolutions. |
| Tests | About 1,100 lines, the cross-feature test the most valuable. |
| Fixtures | A seventh 1.9 MB copy of the replay goldens. |
| Docs | About 1,300 lines, 890 of them REPORTS. |

**Verdict: not worth 13 hours.** About 3 hours of real work (the merges, I3, R, N) carried the value; the rest was
evidence ceremony and machine time that the packet demanded.

**The cause is mostly the packet, not the model.** The orchestrator wrote this packet: 521 lines, eleven parts,
an independent reviewer per part (nine reviewer runs), mutants, a whole-suite matrix on the Mac, a full golden
re-recording into a new `current138` directory, and every input's real-data proof re-run. The executor delivered
each of those. A model that writes long reports and re-verifies everything adds to it (890 REPORTS lines, red CI
not read for about five and a half hours), but the same packet would have cost most of this time with any model. The
second cause is E1: a state number change forces the re-recording and re-proof that made I2, I4 and I5 long.

## 4. Smaller packets from here

The owner decided (2026-11-12): fewer reviews inside executor packets, because the orchestrator reviews every
handover anyway; smaller, simpler packets; one step at a time. The rules are on the board
([`TASKS.md`](../TASKS.md), "Owner rule, 2026-11-12"), and the [evidence contract](../../testing/evidence-contract.md)
now names the orchestrator's handover review as the independent review. In short:
- one concern per packet, about 2–4 hours, at most about one page;
- no reviewer subagents in the executor's packet;
- the gate is the build, the tests for what changed and hosted CI; the orchestrator runs the whole suite and the
  game files at the merge;
- one REPORTS entry of at most about 60 lines;
- no new state number until the save-format ADR is accepted.

The queue, one packet at a time (the board's "Current work" has it with its status):

| Step | Who | Packet | Size |
|---|---|---|---|
| 0 | Orchestrator | Land MAP-INTEGRATE-2 (§5) | — |
| 1 | Windows executor | **SIM-LIMITS-1:** the freight save limits from one source (E5, part of E7) | ~2 h |
| 2 | Windows executor | **FIXTURES-TRIM-1:** delete the fixture copies nothing reads (part of E1) | ~2 h |
| 3 | Orchestrator, beside 1–2 | **ADR 0005:** a version per save section and lighter goldens (E1). The owner approves. | — |
| 4 | Either executor | **MAP-MATERIALS-1:** why 18 houses can't pay for their upgrade. Measurement only. | ~2–3 h |
| 5 | Either executor | **MAP-FOOD-1:** why 131 houses lack food (capacity, reach or source). Measurement only. | ~3 h |
| 6 | Either executor | **MAP-GOODS-1a, 1b, 1c:** one supported chain per packet (tobacco, spices, silk cloth) in a real town, with a ledger | ~3–4 h each |
| Later | — | Freight through the effect queue, arrival without `fx.ui`, the index rebuilt on mismatch (E2–E4), after ADR 0005 | M |

## 5. Before landing

1. The orchestrator merges `55ce7e71` into current `main` outside the executor's worktree (two docs conflicts,
   both appends) and runs the full MSVC game-file suite with the RTX 4090 (owner rule of 2026-11-07).
2. The owner's OK, then `tools/orch/land.py`.
3. The shared folder on the Mac (part S) stays an owner action; it does not block landing.
