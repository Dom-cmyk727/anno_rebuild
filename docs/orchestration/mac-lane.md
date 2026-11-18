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
| 1.1 [TOWN-START-1](tasks/town-start-1.md) | RELEASED directly, 2026-11-18; measurement only | Accepted setup base `ad360af024543a71e083dc3de714b7f48f354e63`; local stack authorized | Pending new head | Reuse setup full pins for unchanged production/fixtures |
| 1.2 [WILDLIFE-1a](tasks/wildlife-1a.md) | WRITTEN; queued after 1.1 review; explicit release required | Pure helpers only; to cut from fetched main | Pending | Pending |

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

Finish the exact candidate gate first. Then package MAC-HEAVY-1b without repeating its old heavy runs.
TOWN-START-1 is the next gameplay measurement, released separately after the packaging checkpoint.
An unrelated gate failure is preserved/escalated, never silently converted into a pass or repaired here.
