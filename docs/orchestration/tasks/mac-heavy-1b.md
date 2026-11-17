# MAC-HEAVY-1b — make the reviewed evidence safe and small to publish

**Wave 0.2; assigned to EXECUTOR II MAC, 2026-11-17.** Execution is released only after
GATE-RUN ui-info-2d-buildcat (wave 0.1) has finished and reported. No overlap with that gate.
About one hour; documentation/evidence packaging only. No reviewer subagents.

## Read and pin

Read `CLAUDE.md`, TASKS.md owner rules and section 1, `docs/testing/evidence-contract.md`,
`docs/testing/README.md`, `docs/orchestration/mac-lane-2026-11-17.md`, the original
`tasks/mac-heavy-1.md`, and its report/findings at
`ce35bb7b979eca2f1720e9ad1e04088a762449d8` on `origin/task/MAC-HEAVY-1`.
The reviewed measurements used source `4dfdd9b7ce51b9ad04970da0b247294345f05ac7`, tree
`e218b8f5dae9e7e133e8f803618ba6e14d355286`. Do not relabel them as measurements of a newer base.

Create `task/MAC-HEAVY-1b` from freshly fetched `origin/main`; record that base. Use an owned
worktree. Do not merge/cherry-pick the old evidence commit: excluded files must never enter
the new branch's commits. Preserve the old branch, its local raw logs and its worktree.
The owner decides any deletion/history cleanup of the already-public branch.

## Small steps

1. Inventory the old evidence and retain byte-identical originals in ignored
   `cache/mac-heavy-1/`, including full failures, ordered prefixes and per-case outcomes.
   Verify SHA-256 before and after copying; never overwrite an existing different artifact.
2. Prepare only findings, README, methods, compact stage/host/acceptance summaries and a
   SHA-256 index under `docs/orchestration/evidence/mac-heavy-1/`. Retain MH-001 through
   MH-006, the scale shortfall, exact source/build identities and all NOT_RUN limitations.
   The full atlas TSV, stage 1/3 outcomes, stage 2 matrix, raw per-case corpus records,
   generated command registries and verbose orchestration artifacts belong in cache.
   Preserve compact numerical corpus/atlas comparison summaries and the raw-byte hashes.
3. Replace every absolute host path in the material to be committed, including JSON,
   excerpts, commands, executable identities and the new REPORTS entry, with a repo-relative
   path. External tools/content roots use explicit environment-variable placeholders.
   Explain the path mapping once; do not edit preserved raw bytes to manufacture old hashes.
   A transformed summary gets its own new hash, distinct from the raw artifact's hash.
4. Reconcile summaries with the retained evidence. MH-001 is an assertion without a sanitizer
   diagnostic, not an ASan memory error; isolated success does not erase it. Order failures
   do not establish a root cause. Corpus load refusals are not executed invariant passes.
   Check links/index entries after moving bulk artifacts; name cached files as local evidence,
   not publicly downloadable links. Keep the committed evidence at or below 1 MiB; if the
   essential summary cannot fit, report QUESTION rather than silently exceeding the budget.
5. Inspect the complete diff against the recorded base and every new commit for host paths,
   original game content and accidental bulk files. Record file count and total evidence bytes.
   Append one concise READY FOR REVIEW entry to REPORTS with Decisions and reasoning.
   Commit and ordinary-push only `task/MAC-HEAVY-1b`; read hosted CI for that exact final head.

## Acceptance and stopping

| Requirement | Proof / control |
|---|---|
| Clean publication | Added/changed text and each new commit contain no absolute host paths; raw artifacts remain private cache |
| Small evidence | Tracked evidence byte total <= 1,048,576; atlas/per-case matrix files absent from every new commit |
| Findings preserved | Original six findings, counts, failure dispositions and raw hashes reconcile; no claim of a new heavy run |
| Correct identity | Original tested head/tree separate from packaging base/head; hosted CI green on pushed head |

Do not rerun the expensive suite: no code/test/tool changes are authorized. Local verification is
the inventory, path/content scan, hash reconciliation and diff checks; docs-only hosted CI applies.
Do not edit shared boards, production code, tests, pins, game files or defaults. Never push main,
force-push, delete a remote branch or stop another job. Stop READY FOR REVIEW with head, CI URL/run,
evidence bytes, checks, preserved failures and NOT_RUN. The Mac orchestrator reviews the frozen packet.
