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

Four positive public-command cases (relocated explicit/default root, comment-only
nondeclaration, restored tree) and 22 intended refusals execute in the final
26-case contract. Each mutation proves its replacement applied exactly once;
changes stay in private copies and the real source/document inputs are verified
unchanged afterward. The omission case removes an actual Rules row from the real
inventory copy. These are subcases of one CTest contract, not 26 CTest passes.

The first dispatch-bypass control exposed a real extractor blind spot: replacing
wire_type's final visit with a constant still agreed. Its retained pre-fix contract
fails with wanted exit 1 / got 0. The corrected bounded extractor requires the
actual visitor dispatch shape, its supported method declarations, and the class
initializer plus its unmodified return shape. The final contract catches that
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
1 payload override; it does not claim 151 implemented gameplay capabilities.

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

Final `cache/docs-sim-1/guarded-capabilities-final.log`, using the public contract
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
