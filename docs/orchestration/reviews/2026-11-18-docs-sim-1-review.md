# DOCS-SIM-1 — local review approved; CI portability correction required

Mac orchestrator, 2026-11-18. Frozen head
`9d4a0e15c01c6f2afe708fa95f6a6c90e648f6b8`, tree
`3454405185904a423c13c718d9eee5ff3fdc81b8`, on `task/DOCS-SIM-1` from
accepted `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`.
Independent read-only reviewer: `/root/review_town_ci_close`.
**Local review APPROVED with no remaining P1/P2. Acceptance waits for exact-head
[CI 44307300031](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44307300031).**
No main landing is implied.

## Reviewed scope and correction

The reviewer independently reconciled 33 direct Rules members and 58 expanded
rows (33+16+4+2+3), 14 command alternatives, 25 class rows, all 15 Seams members,
and 28 scheduler call tokens. It followed actual consumers, presets, hooks and
class overrides: Quarry's None tick does not make the class inert. Three C++
changes are comments only; runtime token streams, all 119 sim fixture blobs,
state/section/PRNG/wire identity and defaults are unchanged.

The first review required changes. Explicit numeric enum initializers were lost
by the checker: Plain=1 and PayloadTag::Workshop=21 still reported agreement.
The correction checks declaration position and raw initializer for RuntimeClass,
PayloadTag and TickKind; class-row positions are explicitly lexical. There are
now 54 enum identities and 197 mechanical rows, not 197 implemented features.
Independent private-copy challenges confirm legal0, each intended drift1 and
restored0. No broad evaluator or generic C++ analyzer was added.

Two misleading descriptions were corrected: enforce_research is an additional
apply.cpp check, separate from construction eligibility; both house updates and
city booking require persisted billed goods. The final evidence pointer was
also corrected. Prior source `c5341f5b8bf5a10e23c715a16a70846a4576a5fe` and its
[CI 44306787614](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44306787614)
remain predecessor evidence, not acceptance of the corrected source.

## Evidence and limits

All eight final implementation/document inputs match the before-gate manifest and
committed blobs at both ends of review; all sixteen documented evidence hashes
match. The authoritative private receipt is
`cache/docs-sim-1/enum-prose-correction-final.log`, SHA256
`65780e52b0083fb2c62a777d36cfd6ef5357376ca8beefc3f279bf9715d53bc3`.
It reports required contract1/1 and checker1/1, no failed, empty, skipped,
incomplete or sanitizer outcomes. The contract's 30 subcases are six legal/restored
controls and 24 intended refusals; they are not 30 CTest passes.

Six unaffected lint/reference checks and the same-build picture contract are
reused from the preliminary focused8/8 plus picture1/1 run. Only the changed
checker/contract pair was repeated. The original dispatch-bypass blind spot and
pre-fix enum controls are retained, not overwritten. See the committed
[evidence note](../evidence/docs-sim-1.md) for receipts and scope.

The checker covers finite declarations/default expressions/mappings and phase
surface, not arbitrary C++ expressions, callee behavior or prose truth. Semantic
columns were independently reviewed against current callers. No new gameplay,
live-window test, balanced economy, original-runtime parity or full local
sanitizer campaign is claimed.

## Hosted follow-up — not accepted yet

Exact-head CI44307300031 completed. Four C++ lanes passed, but Windows reports
2647 selected,2195 executed,1 failed,451 classified skips; failed/empty/incomplete/
sanitizer are1/0/0/0. Its only failed case is sim_capabilities_contract, where the
real inventory omission replacement finds0 matches after two legal controls.
This is an owned contract portability defect requiring correction and re-review.
The source must not be accepted merely because the Mac controls passed.

Separately, build-macos job134812165786 has no runner and no executed step.
GitHub annotations name unavailable hosted runners and macOS arm64 capacity.
That cancellation remains preserved, not a code-failure claim or a pass.
The normal corrected-source push will require a fresh exact-head verdict.
