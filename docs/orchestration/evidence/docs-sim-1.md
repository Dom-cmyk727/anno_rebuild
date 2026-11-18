# DOCS-SIM-1 verification checkpoint — 2026-11-18

Accepted base `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`; board packet read at
`395d6feab90facbc786eb134e4285f8dc0297882`, without merging the board or main.
Source branch `task/DOCS-SIM-1`. Independent review and exact-head CI are pending
at this publication checkpoint. No new gameplay or default decision is included.

## Acceptance and scope

| Requirement | Positive evidence | Distinguishing negative | Review |
| --- | --- | --- | --- |
| Rules membership/defaults | 33 direct, 58 expanded rows; archive guards independently total 33+16+4+2+3 | new field, changed default/constant/enum order, unsupported array declaration | pending |
| Commands | 14 Body alternatives/wire mappings, exact bounded visitor dispatch | new alternative, changed wire value/visitor, bypassed dispatch | pending |
| Classes | 25 class rows with payload/tick/gate and Quarry override | changed tick/payload, unsupported row or trailing table mutation | pending |
| Hooks/phases | all 15 Seams members and 28 named/supporting advance call tokens | omitted hook, added hook/phase | pending |
| Input/doc validity | actual relocated source/doc tree passes both explicit and default root | missing/extra/duplicate row, missing/empty source, unsupported syntax | pending |
| CI integration | both CTest entries discovered and executed | public contract checks actual exit/diagnostic; no internal-helper-only proof | pending |
| Human descriptions | capability rows cite production callers and deeper proof contracts | mechanical columns explicitly do not certify reachability/gating semantics | independent source review required |

Pre-review controls: four positive public-command cases (relocated explicit/default root, comment-only
nondeclaration, restored tree) and 22 intended refusals execute in the final
26-case contract. Each mutation proves its replacement applied exactly once;
changes stay in private copies and the real source/document inputs are verified
unchanged afterward. The omission case removes an actual Rules row from the real
inventory copy. These are subcases of one CTest contract, not 26 CTest passes.

The first dispatch-bypass control exposed a real extractor blind spot: replacing
wire_type's final visit with a constant still agreed. Its retained pre-fix contract
fails with wanted exit 1 / got 0. The corrected bounded extractor requires the
actual visitor dispatch shape, its supported method declarations, and the class
initializer plus its unmodified return shape. The pre-review 26-case contract catches that
bypass and a class-table mutation outside the initializer for the intended reason.
No engine defect or engine correction follows from this tools finding.

## Current capability interpretation

See [capabilities](../../sim/capabilities.md). Rules{} defaults are separate from
Living (house_updates/city_booking), explicit booking overrides, city-name data
loading, and the separate fisher header extension. None ticks are not class-wide
no-op proof: Quarry claims, resident stall purchases, public service counters and
booking have external callers. Three narrow source comments correct House/Quarry
claims; C++ token comparison against the accepted base is equal for all 3 files.

The archive member list independently reconciles 33 direct Rules identities;
existing field_count guards give NavRules 16, NavClassCost 4, CargoRules 2,
FreightRules 3. Seams' independent seam_names list reconciles all 15 members.
Body has 14 alternatives; original_class_table has all 25 RuntimeClass entries in
order. A total of 151 mechanical rows includes 8 enum choices, 2 constants and
1 payload override at that checkpoint; it does not claim 151 implemented gameplay capabilities.
The independently reviewed correction below adds explicit identities for all runtime enums.

The checker supports only its named current C++ blocks; it does not preprocess,
evaluate arbitrary expressions, infer callee behavior or prove arbitrary control
flow. Normalized declarations, default expressions, mappings and scheduler call
names/first-seen order/site counts are checked. Semantic columns require review.
An unchanged callee name can conceal a behavior change and remain mechanically
green. The small payload expression is a review trigger, not a general analyzer.
No generated table is compared only with itself and no whole-file hash is used as
semantic coverage. --dump prints JSON for an explicit human edit, never writes.

## Canonical receipts

Initial focused tools/lint/reference selection, `cache/docs-sim-1/focused-dev.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=8 executed=8 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

This executes the two inventory checks, lint plus its self-test, population
oracle, both integration stream references and highmap reference. The two initial
inventory receipts are superseded by the final pair below after a proof-link,
first-seen-order and bounded dispatch-guard refinement. The six unchanged lint/
reference receipts and same-build picture contract are reused, not rerun.

Pre-review `cache/docs-sim-1/guarded-capabilities-final.log`, using the public contract
as --require-test and the checker as the main selection; picture contract reused
with --skip-auto-contract:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

No memory refusal, empty result, skip, sanitizer report or incomplete result.
clang-format 23.1.3 dry-run succeeds for all three comment-only C++ files;
git diff --check and all new/changed document links pass. CMake dev configuration
succeeds; discovery contains sim_capabilities_agree and sim_capabilities_contract.
All 119 tracked sim fixture blob identities match base, including pins and native/
replay recordings. No fixture or oracle value is regenerated. State 139, section
versions 1, PRNG 5 and command/replay/native identities/defaults are unchanged.

NOT_RUN: broad C++ build/suite, ASan campaign, original-file corpus/town/visual
acceptance, Windows/WSL execution; none is required locally by this tools/docs
packet. Prior evidence is not promoted into broader gameplay coverage. Hosted CI
will be read at the pushed exact head and reported directly for review overlap.

## Private evidence SHA256

- `cache/docs-sim-1/focused-dev.log`: `3b553189e9f622dd33a4d78c90eafb2ceb994a05fe996a1e5c47f39f0acee53a`.
- `cache/docs-sim-1/guarded-capabilities-final.log`: `7d1cff221243a4d807d8893207cb49455d35bf9019bb2d5380b95079d55191cb`.
- `cache/docs-sim-1/contract-final-26.log`: `36c5cb8115bd0ed25ced6d3c922446451f3d3ec10c41a667370f620cf76dd177`.
- `cache/docs-sim-1/wire-dispatch-control-before-fix.log`: `a1ab1b0f006f797f2266bc68272101c1b40e7797ca44863ca521e79ac7d513bd`.
- `cache/docs-sim-1/check-final-guarded.log`: `7194d6c5a54241e6b88f3682ff1925cce0dec07ac88ccca740c0bae048d25152`.
- `cache/docs-sim-1/discovery.json`: `1c32dfd3e8f014ebe3c2e610203832ee6889f084799b4e26543e7c2d3a1e33f4`.
- `cache/docs-sim-1/frozen-initial-eight.json`: `859aaf8be45e09c0f40933768cb027a1c8893e1c46b8599bad98d62d58c8cdf0`.
- `cache/docs-sim-1/frozen-final.json`: `3415a5caf59fe6a4dbdf2474b3e70017af53fc9e162de605984b7fb53a9cbf85`.
- `cache/docs-sim-1/fixture-identity.json`: `f5c1b2120cb360b6dcc10165e9bd8526d5c4b8fce660b96804edebb6cc2be7bc`.
- `cache/docs-sim-1/format.log`: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

## Independent-review correction — current frozen checkpoint

Review of predecessor `c5341f5b8bf5a10e23c715a16a70846a4576a5fe` found P2:
RuntimeClass numeric initializers and PayloadTag encoded tags were discarded.
The private public-CLI reproductions change Plain to Plain = 1 and Workshop's
payload tag from 1 to 21 independently; both formerly returned agreement/exit 0.
Those observations remain in enum-review-before-fix.log, with exactly-once
replacement checks. This was a checker defect, not an engine change.

The correction inventories declaration position plus raw initializer identity
for RuntimeClass, PayloadTag and TickKind as well as the four Rules enum types.
It evaluates no arbitrary expression. Class row positions are now explicitly
lexical, not resolved enum numbers. There are 54 enum rows and 197 total mechanical
rows; the four core categories retain 58 / 14 / 25 / 15 identities, plus 28 call tokens.
The corrected public contract has 30 cases: 6 legal/restored controls and 24
intended refusals. Both enum faults now produce their exact drift diagnostics,
with an additional legal restored run after each.

Two reviewed prose corrections are also applied: enforce_research is the extra
check_record research_id test at apply.cpp:435, independent of construction_eligibility
permissions; house_updates_problem at create/load requires save_billed_goods,
as city_booking does in Rules validation. No supported unsaved live-house mode
is implied.

The latest authoritative checker/contract receipt is
`cache/docs-sim-1/enum-prose-correction-final.log`; earlier similarly named logs
are preserved predecessor measurements, not this correction's inputs. The
pre-gate SHA256 manifest frozen-enum-prose-before-gate.json matches all 8 current
source/doc/comment inputs after the final run. The committed blobs will be
reconciled to this manifest before push.

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Only the affected checker/contract was rerun. The unchanged six lint/reference
checks and same-build picture receipt from focused-dev.log remain reused.
No engine token, fixture, pin or identity change. Initial CI 44306787614 on the
predecessor is retained uncancelled; corrected exact-head CI remains pending.

- `cache/docs-sim-1/enum-review-before-fix.log`: `317a2c2a8c7a8884ccf82711b983804e3ed0a75f3e58df414fdc6d286f1a04b0`.
- `cache/docs-sim-1/enum-review-controls.py`: `c97c9e841aa2a0a1a1c7ba841ef87825356f6b19c0797bed578409202f5e2d33`.
- `cache/docs-sim-1/contract-corrected-30.log`: `fba60d6baad690e5922a95a1481ff1a06e79c406bd2078b9196939bc6dbb1bed`.
- `cache/docs-sim-1/enum-prose-correction-final.log`: `65780e52b0083fb2c62a777d36cfd6ef5357376ca8beefc3f279bf9715d53bc3`.
- `cache/docs-sim-1/frozen-enum-prose-before-gate.json`: `538dd971f31343f563ba92cf4fda3b60dc0bed762d32692e48a792101b89e7a2`.
- `cache/docs-sim-1/source-inventory-corrected.json`: `24cde650552ca61a63b618dfd71fbe0cc8a65be5a131663ed8720c500185d54f`.

## Windows CI portability correction — current review checkpoint

Exact predecessor CI [44307300031](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44307300031)
on reviewed head 9d4a0e15 builds but fails only sim_capabilities_contract in the
Windows test lane: `real inventory omission: replacement must apply exactly once,
found 0`. The first two relocated legal controls passed. Public artifact
14430724547 was downloaded and matched its published ZIP SHA256 before extraction.

```text
COUNTS: selected=2647 executed=2195 failed=1 empty=0 skipped=451 (required-asset=423 platform=7 opt-in=21 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
```

A private relocated copy with every actual checker input and both Python tools
converted to pure CRLF reproduces exactly that exit 1 and message; its original
input bytes remain intact. The defect was owned mutation infrastructure: the row
needle used splitlines() then appended LF while read_bytes/decode retained CRLF.
The contract now uses splitlines(keepends=True) for both real-row omission needles
and write_bytes for mutations, avoiding Windows text output's CRLF-to-CRCRLF
translation. Every existing exact-one-replacement and intended-diagnostic check
is retained; finally blocks also assert exact byte restoration.

Six additional public-command cases cover LF and CRLF independently: legal, real
row omission with exact drift/exit 1, and byte-restored legal. The full corrected
contract passes 36 cases (10 legal/restored, 26 intended refusals), both on the
normal source inputs and on the separate all-CRLF relocated copy. Original real
source/doc inputs are never mutated. No production checker or capability semantics
change; among the eight frozen inputs only the contract test differs from 9d4a0e15.

Latest authoritative affected gate: cache/docs-sim-1/newline-correction-final.log
(required public contract then checker; same-build picture reused):

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

newline-correction-before-gate.json matches all eight inputs after that run.
The prior enum/prose and other local receipts remain preserved, with the contract
receipt superseded only by this portability correction. Existing unchanged lint/
reference checks remain reused; no engine, eight-check, original voyage, broad
suite or sanitizer rerun. All fixtures/pins/defaults/identities remain unchanged.

The prior hosted build-macos cancellation occurred before any step because no
arm64 runner was acquired (orchestrator-verified annotations); it is distinct from
this real owned Windows failure. Both results remain retained. Focused re-review
is required before ordinary push; corrected-head CI and native Windows execution
remain pending, not implied by the Mac's CRLF reproduction.

- `cache/docs-sim-1/windows-ci-failure-14430724547.zip`: `f9e530b3b7d4512924429c79198cbd944bfe71fe48a6d8b583f8dfda49140e18`.
- `cache/docs-sim-1/windows-ci-failure-14430724547.log`: `19385fede43125ad4412b58a8a6d38d3765b2c96d9648528e4343e07819a8011`.
- `cache/docs-sim-1/reproduce-crlf-contract.py`: `fd103df50518f2bc53d8a8b63552e50ec91fd11364301b1ff5e9ead2af70a8bf`.
- `cache/docs-sim-1/crlf-contract-before-fix.json`: `944c30d9fd62f21008577c7be1860f704116af001875a556ab90df1085a7f9f2`.
- `cache/docs-sim-1/crlf-contract-after-fix.json`: `003bd55e3fc371889467baa11dcb04b02d03d3cee9f392da728a76b90f84957b`.
- `cache/docs-sim-1/newline-contract-direct.log`: `34c29744a57a46f50c17af20e928f2df7a62f2926b92ed8db3d5d857ba171c89`.
- `cache/docs-sim-1/newline-correction-before-gate.json`: `5764f32ac1c951a48bc8f93f0bb17319d2675f2a6fbbb29ed2991162b641ed80`.
- `cache/docs-sim-1/newline-correction-final.log`: `8252127b206a49e157c98eae75852272031247d647679b787a347b1025cb07e1`.

Focused CRLF correction review: independently approved at contract SHA256
`cd402f75a70ef5d1c5d47315bd03a6a13e9175c09ebbe8bf1b7bbf5d11e18a39`.
Reviewer independently ran all 36 cases on LF and CRLF inputs and verified exact
byte restores, all eight frozen hashes and the final canonical 1+1 receipts.
Corrected exact-head hosted CI remains pending; no further local campaign required.
