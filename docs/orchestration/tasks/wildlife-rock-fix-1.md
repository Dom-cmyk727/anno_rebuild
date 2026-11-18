# WILDLIFE-ROCK-FIX-1 — correct the writer callback evidence

**RELEASED, 2026-11-19.** EXECUTOR II MAC. Focused documentation correction on
`task/WILDLIFE-ROCK-1` at `81e8f03f3bf882afe00272c27a8f1e0989f56283`.
Do not merge/rebase or begin FRUCHTBAR implementation. No production defect was
established by this finding. Preserve all runtime code, tests, fixtures and gates.
Target about one hour, not a new reverse-engineering campaign.

Read the common rules/evidence contract and the review at
`docs/orchestration/reviews/2026-11-19-wildlife-rock-1-review.md` on the dispatch
branch; then the specific affected passages in `docs/re/transport.md`,
`docs/sim/wildlife-stone.md`, `wildlife-cliff.md`, `wildlife-rock.md` and their
frozen evidence reports. Fetch/read the board files without merging its branch.

## F0 — correct the concrete target mapping

The independent primary reviewer re-read the matching AnnoWelt bytes:

- Writer `0x10082040` continues to `ret` at `0x10082b0a`, not `0x10082637`.
- Concrete island vtable `0x100cac2c`, slot `+0x19c`, pointer at `0x100cadc8`,
  resolves to `0x10084150`. Writer calls that slot at `0x10082ab9`.
- Slot `+0x1a0`, pointer at `0x100cadcc`, resolves to `0x100928a0`.
  The transport note attributing this empty-tile fill routine to `+0x19c` is stale.
- Slot `+0x188` resolves to `0x1008f2d0`; writer invokes it at `0x10082ad8` with
  event `0xcb`. Its table index2 reaches `0x1008f2f0`, incrementing chunk byte5.
  The separate World `+0xbc` branch belongs to event `0xcc`, not this invocation.
- Reached `+0x19c` refresh computes chunk-height bounds at island `+0x94`, chunk
  record bytes2/3, using terrain extrema and anchored item height. Its concrete
  type `+0x58` getter is AnnoGame `0x10007d50`; float conversion is the already
  addressed pure helper. It does not change represented tile/variation state or
  consume a random draw.

Verify these finite targets against original bytes or retained primary excerpts;
record addresses and module hash. Preserve raw listings privately. Do not copy
original listings/tables into public git. No broad function/caller survey.

## F1 — accurate projected-state scope

Correct the narrow transport slot attribution and the newly relied-on wildlife
claims that the complete writer ends at `0x10082637`, or that fresh-profile observer
bypass removes ALL post-write work. Distinguish the wrapper observer bypass from
the two callbacks that DO execute inside the writer.

State explicitly: this transient prefix projects tile/variation/draw effects;
it currently omits these derived chunk-cache updates. This omission does not
change the reviewed projection or current counts. Before a future World/render
consumer uses those caches, their required initialization/rebuild must be provided
under a separate reviewed integration contract. Do not call the full original
function a no-op or its complete state reproduced.

The unrelated empty-tile fill routine at `+0x1a0` remains separate; do not implement
it or assume the forthcoming FRUCHTBAR header has no such effect. Preserve the
real prefix boundary and all numerical evidence. Add a dated correction/addendum
rather than silently rewriting historical hash receipts. If an evidence file's
recorded source hash describes the old frozen source, retain and label it as such.

Update only the named claims and current report/evidence needed to reconcile them.
No generalized documentation cleanup, persistent-state decision or code changes.
If the bounded verification contradicts the reviewer, report the exact bytes and
stop rather than broadening the task.

## F2 — freeze and report

Run cheap documentation/link/diff checks and confirm source/tests/fixtures are
byte-identical to `26899502`. No C++ build, sanitizer, original-prefix rerun,
mutation campaign or full gate is needed for prose-only corrections. Explicitly
reuse the valid frozen-source gates; retain first failures/history unchanged.

Commit and ordinary-push the same task branch. Send READY FOR FOCUSED RE-REVIEW
directly to Orchestrator II Mac: head/tree, exact new CI, changed files, addressed
mapping, correction disposition and confirmation that runtime bytes did not change.
Do not wait for CI to submit the correction. No executor review subagents or
self-approval. The orchestrator rechecks this narrow finding and releases the next
fertility-header task directly if resolved. Main/other lanes/coordinators untouched.
