# MAP-UNIT-IMPORT-1 — orchestrator acceptance

Accepted for the bounded headless slice at
`50aa32e38706ad3c2e4302e8fcaada9f555ba034` on `task/MAP-UNIT-IMPORT-1`.
This resolves the missing-actor dependency for the unchanged original
`endless01` SCRIPT2 public-command canary, not full scenario/campaign support.

The orchestrator inspected the handoff, source/caller and evidence-tool changes,
raw gate terminals, tested source attribution, original/synthetic manifest
hashes and all eight retained encoded artifact hashes. Independent read-only
review `/root/review_unit_import` approved with no actionable defects, examining
validation, finite actor graph, source slots/stable IDs, hints/cursors, cargo,
float conversion, explicit component callers and serialized-state separation.
It also checked retained primary disassembly and crew/cargo fault controls.
No builds, mutations or duplicate long test runs were performed in this review.

| Retained gate | Tested engine/source | Executed passes | Failed/empty/skipped/incomplete |
|---|---|---:|---|
| Affected Debug | `1988471a396185fc8d7652ffb449b902079091fc` | 256 | 0/0/0/0 |
| Affected ASan/UBSan | same | 256 | 0/0/0/0 |
| Isolated ASan opening | same | 1 | 0/0/0/0 |
| Corrected inventory reader | `f437ac732e8855ccc7ebd6e0fc65974ba8218770` | 1 wrapper / 12 internal contracts | 0/0/0/0 |

Sanitizer logs report zero diagnostics. These are separate gates, not unique
test totals. Engine files are unchanged after the affected gates; the reader's
later label correction has its own focused gate and reprocesses saved evidence.
The old SCRIPT2 timeout remains a historical failed run, not a retroactive pass.

Raw logs under the executor's ignored `cache/unit-import1/`:

- `G-debug-01.log`: `361aec5bc19e576597d0e098483686724828f0622e4b31c2419ecbf1f15bfde5`.
- `G-asan-01.log`: `3bd22707febbdb72af9853ef5e4407320e4400b64fc8313c595e40b1d7a38e07`.
- `G-asan-opening-01.log`: `69dd2df72c9581192f0e92595888c6ebabe0c3b0de3911aa890fc009be90f22d`.
- `E-reader-corrected-01.log`: `8ed7c288002d3b327683af7545aeb0959639b20de7718e6539a433835051c84c`.

What is established: normal import of the original five-actor ship/crew closure,
unchanged cargo/position, actual area-triggered wave, public sailing/founding/
unloading/building, native continuation and fresh-process replay. Unsupported
actors fail normal import; component/generated-base exclusions are explicit.
The import-only metadata has no runtime consumer and its effects enter existing
serialized fields. The accepted input remains native 130 / timeline 5.

Limits: 19/50 files pass complete actor import; 14 programs compile and 11 start
normally. Original true-branch coverage is branch 0; synthetic branches remain
separate. Full scenarios, audible playback, target-host builds/runtime, GPU and
multiplayer scale are unclaimed. The review does not publish or integrate code.

Next: [MAP-INTEGRATE-1](../tasks/map-integrate-1.md), combining this exact pin
with accepted supply `67158af4` on a separate local branch owned by the second
Mac. Active cloth/leather work and Windows inputs are excluded.
