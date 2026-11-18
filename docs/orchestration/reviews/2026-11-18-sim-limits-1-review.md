# SIM-LIMITS-1 — local review approved; exact-head CI pending

Mac orchestrator, 2026-11-18. **LOCAL REVIEW APPROVED, NOT YET ACCEPTED.**
Final head `c4f52d286b6589eb86467d8681c3f03af35a4a43`, tree
`1d404c29e3ce591cd98ca0f0bea08df21577b59b`; code/spec/tests
`3387108059ed05562555d90145275580c150cd7e`. The later commit changes two documentation
files only. Exact-head [CI 44294408513](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44294408513)
is the remaining acceptance condition. No green result is presumed.

## Reviewed scope

Independent read-only reviewer `/root/review_town_ci_close` approved the frozen source,
tests, negative controls, local gate receipts and final committed evidence with no P1/P2.
The orchestrator also read the changed consumers, arithmetic, tests and contract.

- Stored rejection codes preserve the existing valid domain; NoDepositInReach remains
  reserved/refused. Unknown codes also refuse. The new exhaustive classification, not
  the pre-existing name switch, catches the appended-unclassified-code control.
- Navigation codes remain 0..3, including the existing zero/no-failure representation.
- Shared framing is 12 + 20*stops + 13*actions, bounded before arithmetic can overflow.
  Actual codec, typed-command, Rules and native consumers distinguish all six legal/
  illegal hand-derived boundary cases. Command stops remain20 bytes; native stops24.
- HarvestState's truthful7+2 layout guard preserves common and conditional mason bytes.
  Handwritten ordinary bytes, independent integration bytes, nonzero mason round-trip,
  added-member control and omitted-conditional-byte control cover distinct obligations.
- All six deliberately broken overlays fail for their intended reason while legal controls
  pass. Production headers were not altered by the overlays. Eight positive probe checks
  pass; these are not added to canonical test counts.

No production behavior, supported bytes, state139/section1/PRNG5, command/replay/native
identity, defaults, maps, assets or other-lane changes. All119 tracked sim fixture files
match the accepted base; no pin, golden or recording is regenerated.

## Receipts and provenance

| Local gate | Selected | Executed | Failed / empty / skipped / incomplete | Sanitizer lines |
|---|---:|---:|---|---:|
| Focused Debug |55|55|0 / 0 / 0 / 0|0|
| Same selection ASan/UBSan |55|55|0 / 0 / 0 / 0|0|

Each also executes required picture contract1/1 and mandatory preflights. The selection
includes real-file automatic freight, actual consumer/byte cases, four relevant integration
cases, sim_stream.py --check and both integration references, without writing mode.

The gates ran on frozen uncommitted inputs, not falsely attributed to a then-existing clean
commit. Independent reconciliation matches all11 committed input blobs, the tracked diff,
the initially untracked shared header and both gate receipts. Recorded executables still
match their hashes. The final documentation's eleven published cache hashes and the frozen,
fixture and selector manifest hashes reconcile with the retained files.

| Gate log | SHA256 |
|---|---|
| focused-dev.log | cb8988581366d7556ca6f59a26705fcc8668f18a44f9bea93f0861ac7430f493 |
| focused-asan.log | 6640b8b760d5b58ca2c99ead3964e49271a53752f2b9af1c88537a799fe21781 |
| controls.json | 66d797a939c9595f8eb56a61c313efa249a8797cea3bb00d212f33f025967592 |

All five changed translation units pass x86 GCC15.3 Release warning-as-error compilation.
This is a compiler control, not native Linux execution. The initial CAPTURE-arity build
failure and inherited Apple duplicate-library linker warning remain disclosed. No memory
refusal, timeout or runtime/sanitizer failure was erased. Source/spec/tests and evidence
are in the task branch's docs/orchestration/evidence/sim-limits-1.md and bottom REPORTS entry;
raw logs remain private in cache/sim-limits-1/.

No further local campaign is needed. Original-runtime, live window/GPU, native target-host
original-file runs, LeakSanitizer and full-suite/town coverage remain NOT_RUN. Hosted CI lacks
original game assets and cannot extend the local corpus claim. Acceptance and DOCS-SIM-1
release will be recorded only after the named run succeeds. Main landing remains Windows-owned.
