# ECON-BOOK-1 orchestrator acceptance — 2026-11-06

Accepted within the bounded headless, opt-in scope after independent source,
primary-evidence, persistence and raw-gate review. Executor candidate `467373d2`
contains production/test source frozen at `03bb4a5c`; later executor changes are
documentation only. Orchestrator `bac3eb37` adds one test and no production change.
See the [executor handoff](2026-11-06-econ-book1.md) for package-level details.

## Review and targeted correction

| Scope | Reviewer | Result |
|---|---|---|
| Scheduler, effect barriers, actual treasury preflight, faults and default-off behavior | `/root/review_consolidation_plan` | No P1/P2 production finding; identified the missing simultaneous nonzero city/player upkeep control |
| Lifecycle, ownership, atomic Build preflight, current-selector cost history and original call paths | `/root/audit_completed_refs` | No P1/P2; independently checked primary DLL instruction sets and all26 audited statistic getters |
| Price parser, identity/oracle, gate inventories and real loop/replay | `/root/review_econ_evidence` | No P1/P2; independently recomputed all765 ledger rows and reconciled Debug/ASan artifacts |
| Added combined-upkeep test | `/root/review_consolidation_plan` | Approved `bac3eb37`; focused execution recorded below |

The missing B2 control is now a production-scheduler test: city cost28 charges1,
while separate player cost14 plus remainder1 charges1. The booking-disabled arm
isolates the player charge. The test also checks ledger ordering, repeated-zero
advance, subsequent player remainders14/13, unchanged owner cost buckets and
native continuation. This is coverage of correct behavior, not a claim that the
executor's implementation had a production double-charge defect.

## Evidence and reuse

The executor's broad Debug result remains **1390 selected,1384 executed passes,
6 failures**, not a clean broad pass. All six fixture corrections and the added
identity case have corrected Debug **63/63**; ASan/UBSan **1388/1388**. Raw log
hashes, inventory and clean-source pins were independently verified. Required
preflights are separate from these totals. Original failures remain preserved.

Orchestrator gates use the canonical runner at clean `bac3eb37`, explicit source
and build paths, original assets and admission enabled. The selected booking
suite includes the new combined-upkeep case; required checked-booking, populated
stream and picture-matrix contracts run separately. Both configurations build
all consumers, including viewer/GPU code. This is compile coverage, not an actual
GPU/window test. Linker duplicate-library notices are retained.

| Local gate at `bac3eb37` | Selected | Executed passes | Failed / empty / skipped / incomplete | Sanitizer lines |
|---|---:|---:|---|---:|
| Debug booking suite |16|16|0 / 0 / 0 / 0|0|
| ASan/UBSan booking suite |16|16|0 / 0 / 0 / 0|0|

Both gates are terminal PASS. Required contracts are separate; the two rows
cover the same16 cases and are not summed. Formatting and diff checks pass.
[Committed gate receipts and retained-log hashes](../evidence/econ-book1-acceptance/).


No unchanged long production/replay campaign was repeated: the orchestrator's
only code delta is a test. The executor's reviewed runtime evidence therefore
retains its original source attribution. New local raw logs and inventories are
under `cache/econ-book1-orchestrator/`; executor raw logs remain in its owned
checkout under `cache/econ-book1/B7/`. Committed evidence receipts identify them;
these ignored raw artifacts are not claimed uploaded to GitHub.

## Gameplay result and limits

Original Tutorial import retains its three actors and complete script. Production,
resident purchases, city booking, upkeep, public demolition/paid rebuild and
recovery execute with exact ledgers. Owner0 earns1020, pays5805 upkeep and250
construction, moving50000→44965. The result proves an operating economy, not
profitability, balancing or a ship-founded self-sufficient town. Genuine replay
matches7/7 checkpoints; a legal changed demolition diverges causally.

House/research changes intentionally retain original historical cost accounting;
a later current-selector subtraction can refuse underflow. The documented
whole-city cleanup boundary can fault after a named removal succeeds. These are
explicit consumer constraints, not hidden repairs or silent clamping.

City booking and house updates remain independently default-off. Canonical
publication adopts state133 / ECONBK1 / PRNG5 / replay4 / command1 / save1.
The published state-132 replay corpus under
`fixtures/synthetic/sim/consolidated132/` remains intact; retained predecessor
save/log controls reject incompatibility. Root text fixtures intentionally
updated for133 are identified in the executor report.
Existing executor gates keep their pinned identities; no running branch is reset
or automatically merged. Later branches consume this baseline at a checkpoint.
No Windows/Linux target-host, original-runtime, GPU, full-repository sanitizer,
multiplayer-scale or live-window acceptance is inferred from these Mac results.

Final focused closeout reviewer `/root/review_consolidation_plan` approved
`bac3eb37`, both terminal16-case gates and their evidence reuse. The publication
commit adds documentation only and does not change the tested runtime or test.
