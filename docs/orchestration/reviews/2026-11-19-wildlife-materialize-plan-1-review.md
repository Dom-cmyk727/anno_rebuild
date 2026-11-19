# WILDLIFE-MATERIALIZE-PLAN-1 — independent review

2026-11-19. Bounded source semantics and evidence reviewed; inherited portability
correction required before acceptance. Exact-head hosted CI still in progress.

- Published head `1c027079fd0d431fae1613df839f1bde9c38d2bc`, tree
  `692d2b5212c77c4b1c90055b07ad17d41f11cd88`.
- Tested source `49c9ddb3066ba7f71cd4e6e29170fb11bf3a7130`, tree
  `103d57f73ff7344dc8d1bbf6066e163b2dcdffab`; later delta report/evidence only.
- Base `95d2eb0f4bf09c559413ef380fba3fd4281939ff`.
- CI [44528637255](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44528637255)
  pending at review. It inherits the preceding chunk's MSVC build issue below.
- Task-branch contract `docs/sim/wildlife-materialize-plan.md`; evidence
  `docs/orchestration/evidence/wildlife-materialize-plan-1.md`.

## Scope checked

The owned planner preserves record order and reached-field validation: category,
occupancy, signed wrapping-byte timestamp, full actual object lookup, output-0 goods
and actual kind. It does not perform habitat's growing-type substitution. Null
lookup differs from a missing fact; known zero differs from absence. Unsupported
selected creation kinds retain their exact choice/count and stop diagnostically.
No spawn field is changed on either success or failure.

An owned immutable source catalogue is bound lazily once per batch. Its selected
New capability is constructed from validated private facts, with the old single-
record entry point preserving source-before-selector behavior. Selector offsets
and unused suffix are explicit; counts do not certify omitted native callback,
position, allocation or creation draws. Supplied final tile/cave facts remain
assumptions, not original runtime terrain or cave-provider proof.

## Independent reviews

Primary reviewer `/root/review_docs_sim_dispatch` APPROVED the conditional
selector-only scope with no additional actionable semantic findings. It checked
actual DLL predicate order, byte subtraction, actual-type lookup, reached-field
knownness, catalogue factoring, ownership and cumulative partial accounting.
It also approved the finite CI-close scope. The hosted portability blocker remains
separate from this bounded semantic conclusion.

Evidence reviewer `/root/review_town_ci_close` APPROVED the tests and evidence with no
actionable findings. All 114 closeout receipts and 1023 frozen identities match.
It independently derived deltas 0/127/128/255/10, mixed skip/ordinary/cave order,
cursor 2 then 3 and suffix999. Seven production mutants fail intended assertions
after a legal six-case/269-assertion control. Known-versus-missing and growing-type
counterexamples are substantive. The retained pre-factor 22-assertion control uses
source/header bytes matching the accepted predecessor; old API checks run again
after factoring.

The root independently verified all 114 manifest entries. Private manifest
`cache/wildlife-materialize-plan-1/closeout-sha256.json` SHA256:
`ee51700bf2c70829113627b7debb87ed027728df01bc4d5a52754a118918631a`.
Original files/listings remain private and all 292 tracked fixtures unchanged.

Debug coverage is 29 unique executed passes across two runs, NOT a clean first
whole gate. The first run passed its one new real-file case, then failed workflow
validation because a required test name was wrong. That failed log is preserved.
The corrected run executes only the previously unexecuted 28. ASan/UBSan executes
29 unique cases with no reports. Original prerequisites and both independent
source oracles were set and required. No skip, timeout or memory-refusal is hidden.

## Portability finding and disposition

The preceding chunk's exact-head CI44522148816 finished with MSVC build failure
and a separate Linux Release dependency-install failure. MSVC check135553390406
reports C4244/WX through std::optional, int to uint16_t; the new goods assignment
uses an int literal matching that mechanism. The public annotation has no source
backtrace, so this is strongly implicated, not proved the sole diagnostic site.
The narrow correction is an explicitly typed value, preserving
255 and write order, plus any same-cause newly exposed conversion sites. Do not
silence warnings or change gameplay/schema to obtain a green build.

Linux Release check135553390583 failed apt installation with exit100, before any
source build or test. The exact dependency/network cause is not established by
that annotation. It is neither a compiler failure nor a passed source gate.
The old failing runs remain permanent evidence; a descendant pass must not be
reported as a green old head.

[WILDLIFE-PLAN-CI-CLOSE-1](../tasks/wildlife-plan-ci-close-1.md) is the next finite
correction. Preserve the reviewed materialization source while reviewing; release
its tiny delta with focused validation and exact-head hosted MSVC proof. No broad
repair campaign, long opening rerun or repeated mutation matrix is required.

No original/HE runtime, live creation, geometry/provider, full sweep, native
manager, first action, World/state/default/identity/global PRNG or graphics parity
is claimed. Local Windows/WSL, LeakSanitizer and broad/full/heavy/pins were NOT_RUN.
Main and other lanes remain untouched.
