# W4-WIN-VERIFY — direct acceptance review, 2026-11-02

No findings. The two verifier defects are closed for the assigned scope.
W4-OPENING-1 is released with the exact inputs below. This is acceptance of the
bounded verifier correction and permission to begin the live opening integration;
it is not acceptance of an opening that has yet to be implemented.

## Reviewed identities

| Identity | Commit |
|---|---|
| Published Windows candidate, documentation/evidence head | `26107db089f558bc374208a4f414ca70f562e6f0` |
| Runtime/C++ and full focused gate source | `f047165c67bc26c1498c5201c014db83553c6f6f` |
| Final tools/tests | `3c808780c4e5e59d26375151c3ff8215c4c68602` |
| Accepted Mac opening integration input | `0c401c8d084cfd9feb8aec846595291f87f22cce` |

The orchestrator directly reviewed the source changes from `dad27da7` through
the final candidate, their surrounding parse/execution/verdict paths, tests and
portable evidence. The `f047165c` to `3c808780` increment changes only legacy
revalidation's line splitting and public-wrapper contracts; it changes neither
the C++ producer nor the wrapper's execution path. That increment is included
in this direct review. The final candidate's remaining changes are documentation
and portable evidence. No additional review cycle is required for that increment.

GitHub remote refs were checked: the Windows task branch points to `26107db0`.
The Mac branch points to later accepted loading publication `0aed8ec0`; the
opening input remains its exact ancestor `0c401c8d`, not that moving tip.

## Findings closed

1. **Hidden child sanitizer reports.** `viewer_wiring.py` retains stdout and
   stderr and scans both using `ctest_verdict.SANITIZER`. A report fails the
   wrapper even when the child exits zero, or writes the expected SDL failure
   diagnostics. Paths, line numbers and excerpts reach the gate. Scanning is
   performed before watchdog, exit-status and session verdicts. The default
   `UBSAN_OPTIONS` adds fatal diagnostics while preserving a caller's override;
   the stream check is the actual acceptance condition.
2. **Incomplete script identity.** `DevScript::parse` records the canonical
   parsed script's step count and FNV-1a identity, including waits, line numbers
   and all arguments. Execution records contain every argument. The Python
   verifier recomputes the identity from the invoked script bytes and checks the
   complete ordered execution trace. A changed later coordinate, expectation
   argument or wait is refused. Cross-language vectors cover parser normalization
   and byte handling. Legacy traces remain explicitly reduced evidence and are
   refused for a new successful wrapper run; ambiguous revalidation names all
   matching scripts rather than upgrading old evidence.

The retained before/after reproductions show the clean control passing and the
five defective cases failing for their intended reasons. The positive controls
for changed scripts also pass with their own identities. Thus rejection is not
merely a blanket failure of the new parser or wrapper.

## Verification and evidence limits

- **Orchestrator rerun on macOS:** the exact final portable public-wrapper suite
  passes **41/41** unittest methods, exit zero, in about 6.7 seconds. This uses
  fixture children and does not exercise a Windows window or GPU.
- **Executor's retained native gate:** **35 selected, 35 executed passes**, zero
  failed/skipped/sanitizer reports at `f047165c`, repeated at final tools `3c808780`.
- **Executor's retained viewer-enabled WSL ASan/UBSan gate:** **35/35**, zero
  failed/skipped/sanitizer reports at `f047165c`. The final portable contract was
  rerun as **1/1 CTest entry** after `3c808780`. Reusing the real viewer gate is
  justified by the inspected increment; these are distinct gate revisions.
- GCC 13.3 and Clang 18.1.3 warnings checks are retained as clean. The RTX
  `boundary_exit_in_hold` canary completed with runtime script identity and
  retained diagnostic streams. These are executor results supported by the
  [portable evidence subset](../evidence/w4-win-verify/INDEX.md), not target-host
  runs independently performed by the orchestrator on this Mac.
- The stream-reconciliation extract records zero recognized sanitizer lines in
  both channels for the real viewer cases. Expected SDL diagnostic counts remain
  separate. Full logs are Windows-local; portable extracts cannot establish
  exhaustive independent inspection of the archive.
- The cleared-environment workflow sidecars identify runner `b57c6f0d` and the
  relevant workflow file hashes, matching main `92d78ae`. The original WSL
  runner-identity block was contaminated by Git environment variables and is
  superseded by those sidecars. The workflow checkout has 523 CRLF-dirty paths
  under WSL; the tested source is separately recorded clean at `f047165c`.
  These are distinct checkout identities, not a clean-workflow claim.

The earlier completed two-case serial sanitizer corpus and unaffected adapter
matrix are retained under the previous acceptance's scope. Their relevant
production dependencies did not change in this verifier correction. Do not
repeat those campaigns merely to close the verifier. The old zero-file stderr
scan still proves nothing; old traces do not gain full script identity now.

The recorder is trusted diagnostic instrumentation, not authentication against a
forged log. Sanitizer output redirected by an external `log_path` is outside
these captured streams; no such repository setting was used. CPU dummy-window
runs do not prove GPU output. Native 4K, Radeon in a real window, physical display
latency, large-city/complex-economy performance and the historical reclaimer
occurrence's unobserved cause remain outside this acceptance.

## Next dispatch

Independent documentation/evidence-consistency reviewer
`/root/review_windows_dispatch` approved this release after correcting the
workflow/source checkout distinction above. It verified that the Mac task suffix
and I1–I6 plus their stopping criteria remain byte-identical. Source validation
and the local contract rerun were performed directly by the orchestrator, not
delegated to that documentation reviewer.

Start W4-OPENING-1 in [TASKS.md](../TASKS.md), packages I1–I6. Use a Windows-owned
`task/W4-opening-1` branch from exact Windows `26107db0`, with an ordinary merge
of exact Mac `0c401c8d` authorized only into that integration branch. Preserve
both source branches. No main merge/push, rebase or force push is authorized for
the executor.

The deliverable is a usable real-window opening: select a ship, sail, found,
unload and build with original supplies, with scoped target-host checks,
independent package reviews, replay controls and owner launch/playtest
instructions. The newer Mac loading work and active MAP-CARGO-3 correction stay
separate. The owner must still perform their personal playtest after delivery.
