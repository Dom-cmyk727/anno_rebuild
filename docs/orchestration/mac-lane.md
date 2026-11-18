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
| 1.1 [TOWN-START-1](tasks/town-start-1.md) | ACCEPTED bounded measurement on repaired stack; landing pending | Accepted `9edb151c414246d306194a703db71475affbdad4`; preserved report `156badab`, measured `f36c22fa` | [44266114323](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44266114323), all 12 jobs pass | [Acceptance and preserved failures](reviews/2026-11-18-town-start-1-review.md) |
| 1.2 [WILDLIFE-1a](tasks/wildlife-1a.md) | LOCAL REVIEW PASSES; inherited CI closure pending; no World integration | `756e008b8027cf9186b4e79e3ae2e76b1dab4c29`, pure helpers only | [44262049576](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44262049576); not claimed green | [Conditional local review](reviews/2026-11-18-wildlife-1a-review.md) |
| 1.3 [NET-ADR-1](tasks/net-adr-1.md) | ACCEPTED documentation; owner decisions remain open | `cf7109fe14fa729cbb0f1c775fd243cd06b34c26` | [44264630010](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44264630010), exact-head docs CI green | [Acceptance](reviews/2026-11-18-net-adr-1-review.md) |
| 1.4 [MIL-RE-1](tasks/mil-re-1.md) | ACCEPTED bounded static research; no combat implementation | `20edc775e12a4e8cd73d703d80b8ea0e4bb4e02b` | [44268712248](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44268712248), exact-head docs CI green | [Acceptance](reviews/2026-11-18-mil-re-1-review.md) |
| 1.5 [ANNOSCENE-ORE-2](tasks/annoscene-ore-2.md) | ACCEPTED bounded research after P2 correction; no engine implementation | `b12d22b39caee90429aef77b52659aa2bfedb38e` | [44273143955](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44273143955), exact-head docs CI green | [Acceptance and preserved finding](reviews/2026-11-18-annoscene-ore-2-review.md) |
| 1.6 [OQ-562-1](tasks/oq-562-1.md) | RESUMED 2026-11-18 after stale gate handoff; Q0–Q4 remain active | New task branch from accepted `9edb151c414246d306194a703db71475affbdad4`; no main merge | Pending | Pending |
| Parallel [TOWN-CI-CLOSE-1](tasks/town-ci-close-1.md) | ACCEPTED; isolated repair, main untouched | `9edb151c414246d306194a703db71475affbdad4`; tree `6dd647aac8dbe9e1335acd0659daa9c287ee99c1` | [44266114323](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44266114323), all 12 jobs pass | [Final repair review](reviews/2026-11-18-town-ci-close-1-review.md) |

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

## Dispatch history (pending CI statements below are historical)

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

MIL-RE-1 is accepted at 20edc775, bounded static research only. ANNOSCENE-ORE-2 is released
next under wave 1.5, with the accepted predecessor read from its pinned ref rather than merged.
TOWN-CI-CLOSE-1 now has all focused local receipts and independent review; GCC Debug/Release
and macOS hosted lanes pass. Windows and Clang-ASan remain running, not accepted early.

## Current closeout — 2026-11-18

TOWN-CI-CLOSE-1 and the bounded TOWN-START-1 measurement are ACCEPTED on exact green stack
9edb151c (CI 44266114323, all 12 jobs success). Earlier pending/failure entries above remain
history, not current blockers. No main push; Windows owns stacking and combined-candidate gates.
Wildlife remains locally reviewed with inherited-base CI pending, preserved and untouched.
NET-ADR-1 and MIL-RE-1 are accepted documentation packets. ANNOSCENE-ORE-2 has reported a
frozen docs candidate and is under independent review; its row is updated when review completes.
The owner's keep-awake process remains active. No owner policies or defaults were changed.

ANNOSCENE-ORE-2 review found a P2 in the unassigned-marker quantity projection: after R is
cleared, the original jumps past the take-flag check and uses world property14 bit2. The current
audit wrongly preserves quantity for both settings. A focused read/projection/corpus/docs fix
is released directly; preserve its failed evidence. No next implementation or OQ-562 release yet.

## Latest dispatch — ore correction accepted, OQ-562-1 released

ANNOSCENE-ORE-2 is ACCEPTED at b12d22b3, exact docs CI 44273143955 green. The focused
re-review closes its one P2; the previous correction status above is historical. OQ-562-1
is released directly to EXECUTOR II MAC from accepted repair stack 9edb151c, avoiding the
older baseline's known CI failures. This is an explicitly named new task-branch base, not
an automatic merge/rebase of preserved work and not a main landing. Scope is the three
opening-recipe discrepancies and their causal regressions; no living/default/map change.

### OQ-562-1 recovery dispatch

The executor is idle with a clean task/OQ-562-1 at its 9edb151c base. Its latest turn created
that branch and read the relevant callers, but after compaction returned the old ui-tabs-b /
MAC-HEAVY-1 receipts. No OQ-562 implementation or acceptance exists. Resume the existing
packet and keep a private current-task checkpoint note; do not rerun closed gates. The next
planned packet remains SIM-LIMITS-1, to be refreshed and released only after this checkpoint
is reviewed. No additional lane, merge or source mutation is introduced by this correction.
