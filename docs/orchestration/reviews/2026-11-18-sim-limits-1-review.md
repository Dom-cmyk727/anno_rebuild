# SIM-LIMITS-1 — accepted

Mac orchestrator, 2026-11-18. **ACCEPTED at the reviewed final head below.**
Final corrected head `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`, tree
`13e75fe49859ceca795fd6f3813e48b4da094310`; code/spec/tests remain
`3387108059ed05562555d90145275580c150cd7e`. The first documentation checkpoint was
`c4f52d286b6589eb86467d8681c3f03af35a4a43`; its failed lint run is retained below.
The correction changes only the narrow allowlist and evidence/report addenda.
Exact-head [CI 44298551123](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44298551123)
completed successfully at this exact head: all 12 jobs passed.

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
original game assets and cannot extend the local corpus claim. The exact-head CI acceptance condition is now satisfied. Main landing remains Windows-owned.


## Hosted CI finding and focused correction

Linux GCC Debug builds successfully but fails anno_sim_tests_lint_sim_passes on seven new
constexpr_count entries: head_bytes, stop_bytes, action_bytes, max_stops, max_actions and the
two HarvestState member counts. Its canonical result is 2778 selected, 2295 executed passes,
1 failed, 482 declared skips, no empty/incomplete/sanitizer outcomes. The local 55-case
selection omitted this cheap lint check; no local lint success is claimed retroactively.

Artifact14422993528 was downloaded and checked against GitHub's published ZIP SHA256
bffb44df26b2fbeab0824af8c34083543189f5bf0832a9f2113241207f06a221. It remains failed evidence.
The reviewed constants describe existing wire/layout invariants, not new gameplay capacities.
Use the existing exact-entry/reason allowlist mechanism, verify canonical lint and its
self-test, and prove an unrelated fixed gameplay cap is still caught. Do not weaken the
checker or broadly allow a file. Independent focused review and a new exact-head CI success
are required. Unchanged production/test local receipts remain reusable; no broad rerun.


## Focused correction approved at the committed head

Independent review approves the seven exact path/declaration allowlist entries. The checker
implementation is unchanged. Canonical lint and self-test pass 2/2 with zero failed, empty,
skipped, incomplete or sanitizer outcomes. A private source copy passes, rejects one unrelated
fixed gameplay cap with the intended constexpr_count finding, then passes after restoration.
No broad exemption or source-level evasion was introduced.

The committed allowlist SHA256 is
abc5b3221e922f011d2c7dc7614e3f4387d343b9862e7d8dd6d045fa7eb8aae4, matching the reviewed
file and lint-gate provenance. All eleven prior frozen inputs and all runtime receipts remain
unchanged. The reviewer reconciled the committed correction and documentation; no P1/P2 or
further local campaign remains. Corrected exact-head CI alone gates acceptance and dispatch.


## Acceptance and direct next-task release

The independently reviewed source and evidence are accepted within the bounded compatibility
maintenance scope. Hosted CI confirms all 12 jobs succeeded at the exact final head; it does
not establish original-runtime, live-window or additional original-file coverage.

DOCS-SIM-1 is released directly to EXECUTOR II MAC from this accepted candidate head. The
executor preserves SIM-LIMITS-1 and all earlier branches, works through the reviewed inventory
packet, ordinary-pushes only its task branch and reports READY FOR REVIEW directly to the
Mac orchestrator. No main push, production/default change or owner-policy decision follows.


## Final hosted receipts

The orchestrator downloaded all five final test artifacts and matched each ZIP to its
GitHub-published SHA256. Every canonical verdict is PASS. Compared with the preserved
failed predecessor, each lane has the same selected/skipped counts and exactly one more
executed pass: the corrected lint case. No prior failure is relabelled or hidden.

| Lane | Selected | Executed | Skipped | Artifact | ZIP SHA256 |
|---|---:|---:|---:|---|---|
| linux-clang-asan | 2779 | 2297 | 482 | 14427216188 | 2996b85009f6c14e001b9c9db041292ff77afc40b253b969a264101c3f83bb15 |
| linux-gcc | 2778 | 2296 | 482 | 14425288915 | c569ff86bea37ff5601846bb83eea7c94ed1256a05e9ca81576985116e468405 |
| linux-gcc-release | 2778 | 2296 | 482 | 14425946769 | 12fc18e1e86e4e7291509d17cd179923ec12b497260c1ab9e55a311863586954 |
| macos-clang | 2784 | 2300 | 484 | 14425801269 | ce8c11210c6fc4f22f7a4f94aba3b35ab861ea9e6b070282a1739a5837f1e023 |
| windows-msvc | 2645 | 2194 | 451 | 14427048673 | 332c6f8243ecdffec0def33ee9235b07b1dd3ad5945b44fb31616fd02709c036 |

Skipped cases retain their required-asset/platform/opt-in labels; they are not executed passes.
The four Unix lanes additionally ran the picture contract 1/1; Windows has no such extra row.
All final pending-CI statements above describe earlier review checkpoints. Acceptance is final
for this bounded packet; the preserved failed run remains failed.
