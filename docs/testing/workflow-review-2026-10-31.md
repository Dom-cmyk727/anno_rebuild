# Shared evidence workflow: implementation and review

Owner request: make the evidence safeguards permanent without interrupting the
MAP and rendering executors. Implementation is confined to the main checkout's
instructions, testing tools, CMake registration and CI workflow. No production
C++, coordinator, game assets, candidate map or active executor worktree changed.

## What is enforced

- Source test-name and verdict contracts run before the optional build/selection.
- CTest remains the scheduler; selection inventory is saved and reconciled with
  actual terminal outcomes. Empty, skipped, failed and incomplete tests are not
  counted as executed passes.
- Prerequisite policy is explicit, including CI's unavailable original assets.
- Required package contracts run separately before the selected tests. On UNIX,
  the rendering benchmark contract is mandatory when that source exists.
- Logs are new files, build locks are respected, source/binary/workflow identities
  are recorded, and source changes during a run fail its gate.
- A single final gate record preserves post-test failures when evidence is read
  again. Missing or contradictory completion records fail.
- Agent instructions require discriminating negative controls, deterministic
  synchronization, independent review and a finite package acceptance scope.

The existing MAP verdict reader was reused and extended, rather than replaced
with a separate classifier. The existing W4 multiline name guard was adopted.
The Python launcher is a portable CTest wrapper, with a shell compatibility entry
point. It does not implement scheduling, memory admission or coordinator policy.

## Verification

Evidence directory: `cache/evidence-workflow-2026-10-31/` (ignored).

| Check | Result | Evidence / boundary |
|---|---|---|
| Final verdict contracts | 24/24 | `verdict-final-v2.log`; final completion-record correction included |
| Public workflow contracts, Ninja | 21/21 | `workflow-single-final.log`; compiler-free CTest project |
| Public workflow contracts, Ninja Multi-Config | 21/21 | `workflow-multi-final.log`; Release configuration supplied |
| Source preflight | pass | `preflight-final.log`; final reader extension tested separately above |
| Independent reviewer | No findings after corrections | `/root/review_evidence_workflow`; inspected all workflow, CMake/CI and policy changes; independently ran 24 verdict and 21 public workflow contracts |
| Static checks | pass | Python 3.8 grammar, shell syntax, CI YAML syntax, `git diff --check` |
| Native Windows/Linux and hosted CI | NOT_RUN | Multi-config testing on macOS does not replace native platform execution |

The synthetic contracts launch no game, GPU or budget coordinator. No C++ suite
or existing performance matrix was rerun. `reviewed-files.json` records file
hashes for the reviewed implementation; this note is documentation only.

## Review corrections

1. Plain CMake prerequisite messages plus CTest `Skipped` initially lost their
   asset classification. The reader now preserves it; a real CTest skip fixture
   and a classification contract cover the case.
2. The synthetic project initially omitted a configuration on multi-config
   generators. It now uses Release; both single- and multi-config runs passed.
3. Re-reading a log initially could erase a gate failure detected after passing
   test bodies. The final gate record and public re-read regression prevent it.
   Canonical all-skipped success is also rejected by the same execution rule as
   a live gate. Historical logs remain classification-only where closeout or
   inventory evidence never existed.

## Adoption and remaining scope

The tracked AGENTS.md now forwards to the shared CLAUDE.md rules and evidence
contract, replacing an untracked copy with stale budget instructions. Its earlier
bytes were retained in the ignored evidence directory. A workspace-level
AGENTS.md forwards existing worktrees to the same contract at their next package
checkpoint. It is a local adoption aid, not part of the repository or a promise
that an already-running agent has reloaded its instructions.

No ongoing job should be restarted for this update. Older branches can use the
canonical gate with explicit source/build paths, then integrate the tooling at
their normal integration boundary. The task board's assignments were not changed.

W4's trace semantics and malformed frame-row fix remain with its active executor;
this change makes their contract checks mandatory but does not duplicate or take
over that implementation. Original-game fidelity, target-host checks and
late-game performance keep their existing acceptance requirements.
