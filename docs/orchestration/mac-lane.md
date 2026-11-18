# Mac lane dispatch and review ledger

Owned by the Mac orchestrator on `orch/mac-lane`, created from `origin/orch/board-next`
`8eae12d1` on 2026-11-17. Authority: [lane handover](mac-lane-2026-11-17.md).
All execution goes to **EXECUTOR II MAC**, one packet at a time. The Windows orchestrator
owns STATUS.md, TASKS.md, candidate stacking and landing; this branch never pushes main.

Owner instruction, 2026-11-17: the executor sends results directly to the Mac orchestrator
for review; keep the overnight loop moving toward gameplay rather than prolonged detail work.
After each frozen report the orchestrator reviews it, records a verdict and releases the next
bounded packet or a focused correction. No owner relay is needed for ordinary results.
Owner policy decisions still go to the owner; silence is not approval. After the gate and short
evidence cleanup, prioritize TOWN-START-1 and the listed wildlife critical path.

| Wave / packet | Status | Head / tree | CI | Review |
|---|---|---|---|---|
| 0.1 [GATE-RUN ui-info-2d-buildcat](tasks/gate-run-ui-info-2d-buildcat.md) | CLOSED with FAIL: two no-frame cases; warning check passed; candidate NOT ACCEPTED | Candidate `95897a55bc939a185766b7207e69d098997cdb1d`; tree `d65099256a598f7ce82d37ea303b81ff87088542` | Exact-head lookup: no run found | [Final failure review](reviews/2026-11-17-gate-ui-info-2d-buildcat-review.md); eight extra GPU skips also need policy reconciliation |
| 0.2 [MAC-HEAVY-1b](tasks/mac-heavy-1b.md) | ACCEPTED: publication correction only; Windows landing pending | `8c7155080fbe3e4544f4d4908518859b8e80c4fe` | [44193521811](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44193521811), exact-head docs gate green | [Acceptance](reviews/2026-11-17-mac-heavy-1b-review.md) |
| 1.1 prerequisite [TOWN-START-SETUP-1](tasks/town-start-setup-1.md) | ACCEPTED; Windows landing pending | Head `ad360af024543a71e083dc3de714b7f48f354e63`; tested code `e71f2dc0` | [44209875238](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44209875238), all 12 jobs success at exact head | [Acceptance](reviews/2026-11-18-town-start-setup-1-review.md) |
| 1.1 [TOWN-START-1](tasks/town-start-1.md) | LOCAL REVIEW PASSES; CI BLOCKED, NOT ACCEPTED | Report `156badab35516609e92796e85a4fb0b9314ddac6`; code `c9132c39`; measured `f36c22fa` | Source [44255019025](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44255019025) failed; final [44256560982](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44256560982) pending | [Review and CI disposition](reviews/2026-11-18-town-start-1-review.md) |
| 1.2 [WILDLIFE-1a](tasks/wildlife-1a.md) | LOCAL REVIEW PASSES; inherited CI closure pending; no World integration | `756e008b8027cf9186b4e79e3ae2e76b1dab4c29`, pure helpers only | [44262049576](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44262049576); not claimed green | [Conditional local review](reviews/2026-11-18-wildlife-1a-review.md) |
| 1.3 [NET-ADR-1](tasks/net-adr-1.md) | ACCEPTED documentation; owner decisions remain open | `cf7109fe14fa729cbb0f1c775fd243cd06b34c26` | [44264630010](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44264630010), exact-head docs CI green | [Acceptance](reviews/2026-11-18-net-adr-1-review.md) |
| 1.4 [MIL-RE-1](tasks/mil-re-1.md) | RELEASED 2026-11-18; bounded military research, no implementation | Main base `b5c0ada209d94a0082bc88d793c917c7222e6a21`; preserve wildlife and NET-ADR | Pending | Pending |
| Parallel [TOWN-CI-CLOSE-1](tasks/town-ci-close-1.md) | LOCAL REVIEW PASSES; hosted CI still required; wildlife untouched | `task/TOWN-CI-CLOSE-1`, source `9edb151c414246d306194a703db71475affbdad4`, based on town report `156badab` | [44266114323](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44266114323), pending | [Repair review](reviews/2026-11-18-town-ci-close-1-review.md); earlier failed runs retained |

## Review and reporting contract

An executor reports READY FOR REVIEW, not accepted. Acceptance requires a frozen reviewed head,
green hosted CI at that exact head, and a Mac orchestrator review file under `reviews/`.
Report accepted packets to the owner as **packet, full head, CI run/link, review path**, with
scope/NOT_RUN qualifications. The Windows orchestrator handles the landing gates and land.py.
For a read-only GATE-RUN, the candidate stays untouched and the receipt review names its exact tree.
Nothing in a queued packet selects a production map, enables living defaults or approves an owner policy.

## Open questions / owner decisions

OQ-800 through OQ-899 are reserved for this lane; none allocated yet. Next: OQ-800.
D-1 map choice and D-2 living defaults remain with the owner. TOWN-START-1 measures candidate A
with explicit test rules only. Any new policy question stops the dependent step and is sent to the owner.
Deleting or rewriting the old public MAC-HEAVY-1 branch is also an owner decision; this dispatch does neither.

## Current execution order

Wave 0 and the setup prerequisite are closed as recorded above. Town's bounded local measurement
is reviewed: 16 capped Pioneer houses / 128 residents, no Settler; exact duplicate traces and a
distinguishing food omission. CI acceptance remains blocked, including two unchanged base build
paths. The town-owned helper linkage correction awaits final-head GCC confirmation. Preserve that
branch and its evidence; no third opening or production repair is authorized in the town packet.

The historical ui-tabs-b PASS receipt is already closed; receiving it again does not interrupt or
replace the town task. Do not rerun either old gate. A newly assigned GATE-RUN still has priority.
Results go directly to this orchestrator for review. WILDLIFE-1a is released without waiting for
town landing: it is independent and starts from the named main pin. No owner relay is required.
This is a scheduling decision, not a CI waiver or acceptance. The landing orchestrator must receive
the freight/GCC Release and ciso646/Clang-ASan failures for separate triage. Never fold an unrelated
repair into wildlife or silently convert a red job into a pass.

Owner instruction, 2026-11-18: take care of those CI blockers in a separate package without
interrupting wildlife. The Mac orchestrator owns TOWN-CI-CLOSE-1 in a separate managed worktree;
the executor continues WILDLIFE-1a. Scope is the reproduced GCC vector-copy warning and the
upstream doctest C++20 header fix, plus validation and independent review. Further diagnostics
exposed a guarded statistics helper, two test-fixture access warnings, and a genuine Mine native
reader bug: an incoming presence flag was used to dereference a still-empty optional. The repair
keeps native bytes/identity unchanged. Hardened recovery passes 28/28; affected Debug and ASan
pass 56/56 at `283e8f9b`; the subsequent minimap test and CI diagnostic changes are separately
checked. GCC Release now gathers independent compiler errors in one pass and still fails on any
error; full build logs are retained. No main push, gameplay
change or automatic intake by wildlife. Town's own linkage correction now passes the final-head
GCC Debug build; its original failure remains preserved.

WILDLIFE-1a is now frozen and independently locally reviewed without blocking findings.
NET-ADR-1 is released as the next bounded, independent documentation packet; the executor
preserves wildlife and does not take pending CI repairs or wire World. This is not an inherited
CI waiver. The Mac orchestrator retains CI closure; network policy remains the owner's decision.

NET-ADR-1 is accepted as documentation at exact green head cf7109fe. Its eight owner answers
remain blank and network implementation stays gated. MIL-RE-1 is released next, independently
of pending CI closure. The CI keep-going run exposed seven further test-only compiler failures;
the reviewed correction compiles all seven under x86 GCC Release warning flags. Focused real-file
and sanitizer gates plus exact-head CI are running; no green result is assumed.
