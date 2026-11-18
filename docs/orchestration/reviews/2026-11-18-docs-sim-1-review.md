# DOCS-SIM-1 — accepted after independent correction review and exact-head CI

Mac orchestrator, 2026-11-18. Corrected frozen head
`1fab3ca9ec80cb69a119ea7201e4a7332bd1ddf0`, tree
`30fc22789c65f6f855a83715d87b4f0a9624f557`, on `task/DOCS-SIM-1` from
accepted `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`.
Independent read-only reviewer: `/root/review_town_ci_close`.
**ACCEPTED for the bounded inventory/tools scope. Independent review has no
remaining P1/P2; exact-head [CI 44321152729](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44321152729)
passed all 12 jobs, attempt 1.**
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

## Portability correction — independently approved at 1fab3ca9

The original row needle discarded its line ending and appended LF; CRLF source
therefore matched zero times. The correction preserves actual row endings and
writes mutation bytes directly, avoiding Windows text-mode double translation.
Exactly-once replacement and intended missing-row diagnostics remain strict.
The reviewer independently ran the full public contract on private LF and CRLF
copies:36/36 subcases each, with exact byte restoration and unchanged real inputs.
These are10 legal/restored plus26 intended refusal subcases, not36 CTest passes.

Reviewed contract SHA256
cd402f75a70ef5d1c5d47315bd03a6a13e9175c09ebbe8bf1b7bbf5d11e18a39.
The authoritative current local receipt is newline-correction-final.log, SHA256
8252127b206a49e157c98eae75852272031247d647679b787a347b1025cb07e1:
required contract1/1 and checker1/1, zero failures/empty/skips/incomplete/sanitizer.
All eight frozen input hashes agree with committed1fab3ca9. Only the contract
and report/evidence addenda changed from9d4a0e15; earlier semantic review and
unaffected lint/reference/picture evidence stand. No engine campaign repeated.

Local re-review has no blocking findings. Corrected exact-head CI44321152729
remains required for acceptance. KBEWOHN-1 is separately released from already
accepted SIM-LIMITS2f9e4c0b, so the executor can advance gameplay without importing
a pending candidate or calling the earlier failed CI green.

## Final acceptance — exact-head CI green

CI44321152729 completed successfully on1fab3ca9ec80cb69a119ea7201e4a7332bd1ddf0,
all12 jobs successful, attempt1. The orchestrator fetched the run/job records,
verified all five C++ artifact ZIPs against GitHub's SHA256 digests, and re-read
the raw test logs with the canonical verdict reader and their exact inventories.
Each lane actually executes both sim_capabilities entries and the corrected
36-subcase contract, including native Windows. No earlier failed result is
converted into a pass.

| C++ lane | Selected | Executed passes | Skipped |
|---|---:|---:|---:|
| Windows MSVC | 2647 | 2196 | 451 |
| Linux GCC Debug | 2780 | 2298 | 482 |
| Linux GCC Release | 2780 | 2298 | 482 |
| Linux Clang ASan/UBSan | 2781 | 2299 | 482 |
| macOS Clang | 2786 | 2302 | 484 |

Every lane has0 failed,0 empty,0 incomplete and0 sanitizer reports. Each Unix
lane separately passes the required picture contract1/1. Skips are declared
missing original assets, platform cases and opt-ins; no GPU or unknown skips.
These no-assets CI runs are not original-corpus or gameplay-completeness proof.

Verified public artifact identities (run44321152729):

| Lane | Artifact ID | ZIP SHA256 |
|---|---:|---|
| windows-msvc | 14437496774 | `3a72754cf4088665db7712ba00e4fb2655a3de8b2370bfbb4f2de69ec3cf7688` |
| linux-clang-asan | 14437321686 | `97104fee63c19e4d4c73c4f7936c56fecdf1544bde35007b73bd853385581559` |
| linux-gcc-release | 14437155388 | `c2e17815dceb77152ced2bb2c81a4694b75d7daaccb0d17e9b2ab6dcb373a3f4` |
| macos-clang | 14435837582 | `1f732272bc049ecf655a3798cd514ca898eea7fb0e391930e4f2ee1a28e0f85b` |
| linux-gcc | 14435752588 | `355223a8ab765884d45fb3a3aef54a7aa0b0c15f5ee341607e4f0d0b72f8f867` |

The private receipt manifest SHA256 is
426d8a4d34691a884358d6df40635c06e423d89c7ebfc21520c28cd33048ab48.
The first Windows CRLF test failure and Mac runner-capacity cancellation remain
recorded above. No source changes after the reviewed1fab3ca9 candidate were
needed. Landing/stacking belongs to the Windows orchestrator; main was untouched.
KBEWOHN-1 is already active on its separately accepted SIM-LIMITS base and
continues without rebase or intake.
