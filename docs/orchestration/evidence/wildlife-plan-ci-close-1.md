# WILDLIFE-PLAN-CI-CLOSE-1 evidence

2026-11-19, READY FOR REVIEW. Independent focused review and exact-head hosted
MSVC acceptance pending orchestrator. No gameplay change or local Windows claim.
Base 1c027079fd0d431fae1613df839f1bde9c38d2bc; dispatch
d6cc1cb2f38404aefc0d773beff3cf563bceb330 read in full, not merged.
Tested implementation 1b078d95b5a84dd42bc80b4f6e314e7bb80238af,
tree 3c4f8ca2071399c8e122922a796017b2816703f4. Publication delta is docs only.
[Chunk contract](../../sim/wildlife-chunk-plan.md),
[materialization contract](../../sim/wildlife-materialize-plan.md).

## Narrow correction and acceptance

| Requirement | Check | Result / limitation |
|---|---|---|
| Preserve goods value and original write order | RHS `std::uint16_t{255}`; erase type wrappers and compare original token sequence | Exact numeric values/order retained |
| Avoid narrowing optional assignment instantiation | Explicit matching uint16_t production RHS, matching uint8_t/uint16_t test RHS/constructor arguments | Local compile passes; hosted MSVC supplies target proof |
| Keep knownness and all existing semantics | Unchanged chunk/materialization controls | 14 cases per profile pass |
| Preserve old failures | Public annotations and job step JSON retained separately | MSVC source failure distinct from apt setup failure |
| No warning suppression/state/schema/fixture change | Three-file code diff, lint, fixture hashes | 292 tracked fixture hashes unchanged |

Production: wildlife_chunk.cpp goods assignment changes only literal 255's type.
Same value, before the same position write and collision lookup. No cast of an
unknown value, storage widening, reordered effect, suppression or changed predicate.
Tests: 26 explicitly typed narrow optional literal sites in wildlife_chunk_test.cpp
(9 assignments and 17 optional byte-11 arguments) and 3 in materialization tests.
All are unambiguous int-to-narrow-optional template instantiations. The typed
wrappers preserve their literal values, absent optionals and test expectations.
Whitespace-insensitive token comparison after removing only wrappers equals the
base source in all 3 changed TUs; type-only-diff.json retains the check/counts.
No species/source API was changed; its approved evidence is reused.

## Exact predecessor failures, kept as failures

CHUNK head 95d2eb0f / CI44522148816 is terminal FAIL, not accepted:
- MSVC job135553390406 fails build. Public annotation reports
  `optional(223,65): error C2220: the following warning is treated as an error`,
  `warning C4244: '=': conversion ... possible loss of data`, with `_Ty=int`
  and `_Ty=uint16_t`. xutility also reports int-to-unsigned-short instantiations.
- The public annotation has no source backtrace. The goods assignment is a
  demonstrated matching instantiation, not proved the only failed site by that
  annotation alone. A local Apple/GCC pass does not reproduce or close MSVC.
- Linux GCC Release job135553390583 fails its apt dependency-install step exit 100,
  BEFORE configuration/build/tests. No source verdict. The package/network cause
  is unknown; no outage or dependency root cause is inferred from exit 100.
- Other jobs succeeded; aggregate gate failed. Full job/step/annotation JSON retained.

MATERIALIZE CI44528637255 was not cancelled or rerun. Snapshot during this closeout:
MSVC job135575879320 now completed failure with the same optional int-to-uint16_t
C2220/C4244 mechanism (MSVC 14.51 vs CHUNK 14.52). Annotations retained separately.
Other unfinished source jobs remain in progress in that snapshot; no invented
terminal aggregate or success. Detailed authenticated job logs were not fetched;
claims here use public annotations and per-step statuses only.

## Focused checks

First cheap direct controls: 13 cases, 537 assertions, all pass. Doctest excluded
cases are filter exclusions, not prerequisite skips. No mutation matrix rerun.
Affected strict Apple Clang 21 and GCC 15.3 O3/Werror: 3 TUs each pass;
GCC shadow-warning negative control rejects. Format 23.1.3, sim lint and diff pass.
Debug/ASan affected targets build, 0 compiler warnings; inherited duplicate
anno_sim archive linker warning remains disclosed. No whole warning/test suite.

Frozen inventory 1022 source/fixture/tool/binary/compile-command identities verified
unchanged after both gates. Required exact source-case name inventoried first.
Selection: 7 chunk + 7 materialization = 14 unique per profile, including 1 separately
required bounded original-source case and 13 main synthetic controls.
Canonical runner; jobs 1, threads 2, timeout 180, game directory and independent
species oracle explicitly set; memory admission enabled. No refused/timed-out
case, no rerun or cancellation. ASan/UBSan on, Mac LeakSanitizer off explicitly.
Render auto-contract omitted: no renderer or public evidence CLI changed.

`cache/wildlife-plan-ci-close-1/debug.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```
`cache/wildlife-plan-ci-close-1/asan.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

First canonical attempts pass, no failed/empty/skipped/incomplete case or sanitizer
report. Actual binaries retained; no inferred target compiler acceptance.
Host Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1 (26A434).
Original files read only by the existing bounded source case, not committed.
Predecessor CHUNK 110 and MATERIALIZE 114 retained artifacts still match; old
frozen receipts were not overwritten to represent the correction.

## Hash receipts

| Receipt | SHA256 |
|---|---|
| debug.log | a1bb992ce89ceae0e96027f02b1ec9d3e649041f4b0e39d0bd23e224cdd2feb6 |
| asan.log | d590357d6719437024bb367a5cc12aec36ba6275456692a2113365de36cf7c43 |
| frozen-source-binaries.json | 8543b8e56487320cf1d9a9d635dae872ad9dec9ec003f7ec0d330d36d89aaede |
| frozen/dev-anno_sim_tests | 3b92b54deb5f643acc9f0e509618e3a1fcb008bbba2b08e80dc9801828bac679 |
| frozen/asan-anno_sim_tests | f65cc21ef87546f7f1067c7e481502bf07711b3a000d909cfd4160b7dd07bc53 |
| type-only-diff.json | 185c69bf01fadcd5156b7e3723c570d72826896dd18deb6ca5cfa8035c2dabd1 |
| chunk_annotations.json | 28ab52a201d66f6938cdbf26bc548873ec6940629c50dba780b9c4b320584525 |
| chunk_release_annotations.json | 314f57cfd4e5dd708b5a0bc02ec1b83b016c2003fdb2f5fd9d7d393eae4ea20d |
| chunk_jobs.json | f77010cbe2934c1d947227822f2c0d6eca2e649f25c3a4d6a03591dbe427b7d6 |
| materialize_jobs-latest.json | f27f34016a9297062c8b03a0dafbc0c95a854f1a5d8447d8cacd0b77c6c3465a |
| materialize_msvc_annotations.json | 447cda2ac617bf86fb35e482e030ce76c78e7a59774506e0e798cecd4f6a403f |
| predecessor-preservation.json | 65520ff1ab2cee3190d4fd3ae452bf48a2527db80e8cb0f0811a18bd92c3a2fa |

Private closeout 50 artifacts, manifest SHA256 `eebf8fc8008fbc2f1e6674a9f492378740c6335b011eed54bd85e476cb67d49d`.
Hosted correction CI is read immediately after ordinary push; exact-head MSVC
and all 12 jobs remain the orchestrator's finite terminal closeout. Queue time is
not a reason to repeat passing local gates or cancel an inherited run.

NOT_RUN: local MSVC/Windows/WSL, full/heavy/opening/pins suites, repeated mutation
matrix, LeakSanitizer, native/original/HE runtime. No World/state/default/identity/
PRNG/navigation/FP/graphics/asset change. No new open question or guessed setup fix.
QUESTIONs: none for this finite matching-type correction. STOP pending review.
