# Test and evidence acceptance contract

Owner instruction, 2026-10-31. This is the common policy for every executor and
reviewer, including MAP and rendering. Read it at task start and before reporting
a package complete. It supersedes historical instructions that count CTest
passes as executed tests. It does not change current task scope or authorize
merges, pushes, coordinator changes, asset changes, or new gameplay rules.

## Adoption without interrupting work

Let a running build or test finish. Preserve its original result. At the next
package boundary, read this contract and apply it to the remaining work. Do not
restart a round, reset another worktree, replace its runner during a run, kill a
coordinator, or rerun valid measurements merely to adopt these instructions.

An older worktree may use the canonical gate from the main checkout with an
explicit `--source-root` and its own `--build-dir`. The gate records both the
workflow and tested-source identities. Do not mix uncommitted changes or
cherry-pick another executor's implementation to obtain a testing fix. Integrate
the shared workflow in a dedicated commit at an ordinary integration boundary.

## Before implementation

For each package, write a small acceptance table:

| Requirement | Positive proof | Counterexample / negative control | Evidence | Reviewer disposition |
|---|---|---|---|---|
| Example: stale work cannot become visible | newest generation drawn | an older completion arrives last | named test and raw result | pending |

State the acceptance scope and the stopping criteria first. A new event, parser,
reference comparison or concurrency transition needs a check that fails when its
meaning is violated. A test title or a list of passing tests is not that proof.
Keep fidelity to the original, an explicit adaptation, and an unresolved rule
separate. Never weaken a tolerance to hide an unexplained discrepancy.

For a decisive decoded predicate, record its exact primary evidence, a
hand-derived expected result and the smallest input that distinguishes the
plausible wrong interpretation in this existing table. Verify bit position,
width, mask, units and rounding where applicable. A previously documented fact
newly relied on by the package needs the same check. The reviewer independently
derives that result from the original bytes/data or the applicable API/caller;
agreement among notes, implementation and a reference sharing those notes does
not establish the underlying fact. Confirm the distinguishing assertion fails
under the specific wrong interpretation while a legal control still executes.

For a timed transition, establish the caller's supported interval range and
check progress at its smallest positive interval, zero elapsed time, and a
partitioned delay with surplus time. Preserve any partial delay across visits
and save/reload where required. Compare the isolated transition under its
contract; the entire simulation need not be invariant to advance partitioning.
A typical frame interval or a documented assumption cannot justify failure on
a supported input. Such failure requires a correction or an explicitly approved
behavioral adaptation, rather than a wording-only disposition.

These are targeted checks within the existing package review, before broad
gates. Record their result and the reviewed scope in the same acceptance table;
an unchecked claim remains unverified. No additional reviewer layer, standalone
report, generic testing framework or repeated unaffected matrix is required.

## Mandatory execution workflow

Use `tools/test/run_selected.py` for acceptance gates; `run_selected.sh` preserves
the older positional interface. Direct CTest runs remain useful for investigation
but are not a completion verdict by themselves. Commands are in
[README.md](README.md).

The gate checks test-name safety and verdict-reader contracts before building or
running the requested selection. It inventories the exact selection, checks
required contract tests by exact name, and reconciles terminal outcomes. No
selected tests, a missing required contract, an empty doctest child, a truncated
log, a sanitizer report or a missing required prerequisite fails the gate.

For rendering/benchmark evidence, require `picture_matrix_contract` on hosts
where that contract is supported, regardless of the main selector. If a platform
cannot run it, record that limitation and use an appropriate supported host;
never silently treat an absent contract as executed. New evidence tools must add
their public-command contract checks to the required list for their gate.

Report selected, executed passes, failed, empty, skipped and incomplete outcomes
separately. Skips are classified as required assets, platform, opt-in, GPU, or
unknown. The doctest filter's excluded cases are not prerequisite skips; zero
assertions alone are not an empty test. Optional skips need an explicit gate
policy. CI has no original assets and declares that policy; such a run never
establishes corpus coverage. Preserve CTest failures even if the log parser finds
no additional defect. Known failures remain failures, identified by stable test
names and assertions, not just changing test numbers.

## Reliable tests and measurements

- Enforce required thread ordering with gates, latches or observed states, not
  sleeps or an assumption that a worker is still running. Release gates on every
  exit before joining their owners. A watchdog bounds a broken test; it must not
  satisfy the condition the test is checking.
- Establish that a reference is complete and the compared region is relevant
  and non-empty. A fresh rebuild, CPU/GPU parity and original-game fidelity are
  three different claims. Use a deliberately wrong input/state to demonstrate
  that an important oracle can distinguish the intended defect.
- Validate evidence semantics as well as syntax. Correlate the same command,
  island, generation, frame and surface epoch where applicable. Reject impossible
  causal relationships, malformed rows and non-finite required measurements;
  do not silently drop them. Legitimate coalesced, offscreen, idle and unfinished
  work must be represented without inventing completion.
- Keep evidence validity, recording completeness, scenario correctness and
  achievement of a performance target separate. Check artifact writes and child
  exit statuses. A failed or partial run cannot acquire a success marker.
- Measure actual cadence and drawable pixels. Requested 60 Hz is not observed
  60 Hz; CPU submission and present-call return are not physical display time.
  Record sample counts, unavailable values and the limits of the workload.
- Record source revision plus relevant dirty/untracked inputs, tool revision,
  build configuration, executable and fixture identities, commands and host
  conditions. Freeze those identities for review. Generated report counts must
  reconcile with raw outcomes. Retain the first failed run and label superseding
  evidence instead of overwriting it.

## Efficient order and shared-host discipline

Run cheap negative/contract checks, focused regressions and one end-to-end
canary before a matrix or broad sanitizer gate. Include a canary for a materially
different path such as a larger drawable size. A failed run should answer a
specific question: isolate the cause before repeating it. Revalidate saved data
with corrected readers; remeasure only when inputs, behavior or usable evidence
changed. Reuse unaffected gates with an explicit dependency justification.

Inspect current CPU, memory, disk and host contention; use existing calibrated
limits and keep admission control enabled. Coordinate expensive work rather than
stopping another executor or the owner's apps. Own and clean up only your own
processes, temporary files and display-awake assertion. Historical hardware notes
are not a statement of the current host's capacity.

## Independent review and completion

**Owner instruction, 2026-11-12: for executor packets, the orchestrator's review
at handover is the independent review.** The executor does not start reviewer
subagents of its own, per part or at the end, unless its packet asks for one. It
reports the package READY FOR REVIEW with its acceptance table and evidence, never
"approved" or "complete"; the orchestrator's review decides. The rest of this
section describes that review.

Every substantive package requires an independent, read-only reviewer before it
is accepted. It must inspect the
frozen diff, relevant callers/tests and raw evidence, not just the author's
summary. For evidence tools it hand-derives a small expected answer and challenges
a negative case; for concurrency it examines alternate legal orderings and
failure cleanup. It does not implement, commit, delegate its review or duplicate
long test campaigns. Use the review-agent skill when available; otherwise follow
this read-only protocol. An executor never self-certifies completion.

The author fixes actionable findings and obtains re-review. Record reviewer
identity, reviewed revision, inspected scope, findings/dispositions and residual
gaps. A final integration review checks interactions between packages. Critical
behavioral defects must be fixed; unrelated historical issues become explicit
follow-ups with an owner and acceptance condition, not silent waivers.

Finish with the acceptance table, exact executed counts, preserved failures,
code/documentation heads and remaining limits. Stop when the assigned package is
ready for review. Do not attach another optimization or infrastructure project
to closeout. A locally reviewed unit is not full cross-platform acceptance or
proof of late-game performance. Separately reviewed gameplay work can proceed
without describing pending renderer fidelity or target-host gates as passed.
