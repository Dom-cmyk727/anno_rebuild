# Main consolidation — 2026-11-05

The owner requested one canonical branch containing completed work and an ordinary
push to GitHub. This consolidation uses `main`; it preserves executor branches,
worktrees, uncommitted work and historical evidence. It does not claim that all
unfinished work, original-game behavior or target-host gates are complete.

## Inputs and boundaries

| Stream | Included input | Scope and limits |
|---|---|---|
| Published main / Windows | `cafa74d5`, including `88968580` W4-MAIN-1 | Existing completed renderer, viewer, verifier and Windows integration. No new unpublished Windows result was available in fetched refs. Current Windows opening/depth work stays separate. |
| First Mac gameplay | Accepted supply through `67158af4`; bounded NEEDS N2 through `8ff6d1ca` | Sailing, founding, cargo, roads, forestry, population behind its existing default-off rule, food/alcohol supply, and synthetic Farm/sheep/wool support. N3 cloth demonstration and subsequent hunting/leather work remain with the executor. |
| Second Mac scripting | UNIT1 `50aa32e3`, integrated supply/script candidate `cafc1a7c` | Strict scenario and unit import, construction permissions, script startup, bounded original endless-program execution, combined headless paths. Unsupported programs/actors continue to refuse. |
| Earlier completed branches | ECON research `a47a169a`/`dfd2c7d3`, T35 Result misuse, T31 monument research, T32.6 house research, UI F2.1/checklist, stand-in terrain height, compiler/CI fixes | Missing commits incorporated selectively. Later established semantics take precedence over obsolete research wording. Paused T10c F6 and rivers S7 WIP are excluded. |

Other old refs were audited against current code. Equivalent or superseded
P2.1 player-mask, T58/T59, nav-C, host-support and CI changes were not blindly
replayed. No original assets, candidate layouts or active map were edited.

## Integration behavior and identity

Current identity is **state 132, SIMINT1, PRNG timeline 5, replay framing 4,
command wire 1**. Both independent stream-130 lines and the second Mac's stream
131 are predecessors, not interchangeable current saves. Real predecessor
recordings and native slices are retained with hashes and provenance. The loader
and replay decoder reject incompatible identities at their actual boundaries.

Current fixtures are under `fixtures/synthetic/sim/consolidated132/`. Historical
fixtures remain historical. The independent Python state transcription checks
its modeled synthetic fields; the remaining recorded and corpus hashes are
explicitly C++ rebaselines supported by semantic and replay checks, not an
independent original-game oracle.

The viewer's production loader now uses the strict scenario context and imports
actors. Scene-only tests use an explicitly named, test-only component projection;
there is no production fallback that silently discards unsupported units.
Script startup failure stops play, remains visible, and cannot be bypassed by a
retry or save/reload. The supported endless scenario retains its script namespace
and actors. Single_01's unsupported donkey task is an explicit refusal.

A review also found a named-return fixture lifetime issue in N2: a World's
statistics pointer could outlive the temporary fixture it pointed into when NRVO
was absent. The fixture now explicitly rebinds on move; a forced-move test checks
both pointer ownership and actual raw credit. A broken move is detected.

## Review and verification

Independent reviewers: `/root/audit_completed_refs` (missing-ref audit, bounded
N2 primary evidence, fixture lifetime); `/root/review_consolidation_plan`
(integration callers, startup failure, persistence identity, viewer component
boundaries, test/evidence corrections). Both source reviews approved code `7b57ebd9`. The coordinator reviewer also
reconciled the final host gate and its inventory; final gate counts follow below.

The first combined Debug gate is retained as a failed diagnostic run. It exposed
stale unsupported-road and scenario-import assumptions, outdated hash/count pins,
and a selector that accidentally included GPU cases without a declared skip
policy. These are corrected explicitly; the failed run is never relabeled green.
Earlier source/probe failures and an invalid stale-binary probe are also retained.
Every subsequent acceptance gate builds explicit targets from its frozen source.

Actual-tree auditing (636 non-coordinator source/tool/build paths) found no further
dropped completed production work. It did expose a coordinator omission: earlier
Windows integration had kept compat3 while importing documentation for MAP repair4.
The consolidation restores compat4, Darwin namespace3, legacy-owner handling,
predecessor-journal fencing and the associated tests/tools, while retaining main's newer uint64 process-ID checks and platform guards. A read-only stack sample
of the delayed import test identified the old client waiting for a compatible
coordinator; no running service was stopped.

The recovered journal-report helper also receives a bounded correctness fix:
truncated, malformed and unreadable input now fails before any totals are printed;
owner PID/start stamps are printed, and parsing is explicitly not a liveness or
recovery verdict. Six public-command contracts and a truncation-check mutant
cover that correction. No journal is changed or retired by this work.

## Integration acceptance table

Raw log names below resolve under `cache/consolidation-2026-11-05/`; the
committed index records their hashes. `external-coordinator.json` resolves in
the committed evidence directory. Reviewers are identified above.

| Requirement | Positive proof | Counterexample / negative control | Evidence | Reviewer disposition |
|---|---|---|---|---|
| Strict playable context and startup | Original endless loader retains actors/context; ordinary startup runs | Unsupported original actors refuse; startup fault prevents commands on retry and reload | Named `play loader:` and `script session: faulted startup` cases in `startup-asan.log`, `final-debug.log` and `final-asan.log` | Integration reviewer approved; no silent component fallback |
| Unambiguous persistence identity | Current populated oracle/native cuts compose script, actors, navigation and animal state | Actual predecessor save/log bytes refuse at the intended boundary, including frame2 | `integration identity: actual predecessor saves and logs refuse at intended boundary`, `integration historical replay frame2 refuses before current parsing`; `final-debug.log`, `final-asan.log`; required integration-stream contract | Integration reviewer approved; old streams remain incompatible |
| N2 fixture statistics lifetime | Explicitly moved fixture credits the destination statistics and real raw store | Removing move rebind fails the ownership assertion while preceding controls execute | `sheep farm: explicitly moved fixture credits destination statistics` in final gates; `move-mutant.log` | N2 reviewer and integration reviewer approved |
| Coordinator union preserves live promises | Actual main-v3 predecessor handoffs execute T5/H7; current recovery and host cases pass | Existing live-holder, dead-owner and predecessor-journal refusal cases distinguish admission outcomes | `final-host.log`, `final-host-asan.log`, `external-coordinator.json` | Coordinator reviewer approved; historical-v4 binary and platform skips remain open |
| Journal evidence fails closed | Valid open-promise report names owner and totals; six CLI contracts pass | Truncated release must fail; removing truncation check fails that assertion while valid-open control passes | `journal-report-applied.log`, `journal-report-mutant.log`; required `journal_report_contract` in both host gates | Coordinator/tool reviewer approved; parsing is not a liveness verdict |

## Remaining acceptance boundaries

This Mac cannot establish Windows/MSVC, Linux warning/sanitizer, physical display,
RTX/Radeon, original runtime, or multiplayer/late-game performance acceptance for
the combined source. Rust layout-test coverage was retained but not executed:
Cargo is unavailable on this host. Existing source-pin results from other hosts
remain historical evidence, not proof of the newly combined source.

Executors finish their currently frozen packages before consuming the new main
baseline. Consolidation does not reset their branches or restart running gates.
Their subsequent integration must preserve or deliberately advance the canonical
save/replay identity and rerun affected seams.


## Final verification at code 7b57ebd9

- `final-focused.log`: mandatory preflight refused the newly restored V4 binary
  skip wording. Fixed to the existing `set ANNO_...=path` contract; no classifier
  relaxation. `final-focused-02.log`: 14 selected, 13 passed, one failed because
  an original player's authored balance equals the developer grant. Replaced
  that inequality with exact authored-balance preservation plus a nonempty,
  distinguishing fixture check; code/test source `7b57ebd9`.
- `final-host.log` at `7b57ebd9`: 96 selected, 90 executed passes, zero failures,
  zero empty/incomplete, four platform skips and two opt-in historical V4 binary
  skips. The actual retained main-v3 binary executes T5 and H7 successfully;
  these are not modeled-only handoffs. The corrected app resource case passes.
- The current scenario-mask import check passes in 12.50 seconds, compared with
  the old compat3 gate's 1500-second connection-wait timeout. The sampled old
  stacks are in `population-wait-sample.txt`; no shared service was stopped.
- `final-format.log`: all 193 changed C++ files pass clang-format23.1.3.
- Full Debug and ASan/UBSan builds include viewer and GPU code. Linker duplicate
  static-library notices are retained; no target-GPU execution is claimed.

- `final-debug.log` at clean `7b57ebd9`: **1606 selected, 1603 executed passes, 3 opt-in skips, 0 failed/empty/incomplete**. Required journal, early-needs, hunter, integration and script contracts pass separately. The selector anchors reference names so render/GPU cases are outside this headless gate. Long sailing/opening and all-scenario simulation corpus cases remain explicitly excluded; their prior source-pin evidence is retained, not claimed rerun.

- `final-asan.log`: **166 selected and executed passes**, zero failed, empty,
  skipped or incomplete cases and zero sanitizer lines. This is the affected
  script/unit/import, identity, N2 and viewer-input selection, not a full-suite
  sanitizer claim. It includes the real viewer core; no physical GPU test is claimed.

- `final-host-asan.log`: **95 selected, 89 executed passes**, four platform and
  two opt-in historical-v4 binary skips, zero failures/empty/incomplete cases and
  zero sanitizer reports. The coordinator reviewer reconciled the provenance,
  inventory and six reporter contracts and approved the bounded closeout.

The committed [evidence index](../evidence/consolidation-2026-11-05/INDEX.md)
contains command/provenance extracts and hashes. Raw logs remain Mac-local under
`cache/consolidation-2026-11-05/`; they are not represented as uploaded artifacts. The final publication is an ordinary fast-forward push to GitHub
`main`; source branches and other executors' worktrees are preserved.

Final closeout review by `/root/review_consolidation_plan` approved the raw gate
reconciliation, acceptance table and ongoing-work boundaries. Its minor evidence
location clarifications are applied in this documentation-only publication commit.
