# WILDLIFE-RESOURCE-1 — independent review, 2026-11-18

**Current disposition: ACCEPTED at0627532f after correction review and exact-head CI.**

Initial review at3aab2a93: CHANGES REQUIRED, two bounded P2 corrections, preserved below.

- Reviewed head: `3aab2a93680a9edf97ab4c5b0ac932eaae130513`.
- Tree: `95326782e8a61ad6efd53b8f27727f697f585b9d`.
- Source: `2f65d5b87c424e4f6115b5b0639521b7d8f152ec`.
- CI: [44368707576](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44368707576), pending at handoff; not an acceptance claim.
- Independent read-only reviewers: `/root/review_docs_sim_dispatch` (primary
  semantics/code), `/root/review_town_ci_close` (tests/evidence), Mac orchestrator
  `/root` (frozen diff, receipts, scope and dispatch).

## Findings

### R-F1 / P2 — metadata validation repeats quadratic work

`src/sim/src/resource_import.cpp:24` scans all preceding facts for each duplicate
check. `evaluate()` first validates the whole table, then its call to public
`select()` at line148 repeats that check for each accepted resource. Lookups also
scan the table. Cost is O(R × F²) for R resources and F metadata rows, without a
small-F API restriction. This violates the project's scalable data-driven goal.

Prepare one invocation-local sorted metadata index, reject duplicates once and
use indexed lookups in a validated internal selector. Public standalone `select`
retains its own validation. Preserve missing-fact timing and consumed prefix.
A cheap many-unrelated-facts structural/work-count proof is sufficient; no broad
benchmark campaign or persistent registry cache is needed.

### R-F2 / P2 — zero-id test passes before reaching the new guard

`src/sim/tests/resource_import_test.cpp:281` changes a record to type0 but retains
a registry containing only104. The old `!type` branch already rejects it; removing
the new zero-sentinel guard leaves the claimed regression green. TypeRegistry
can contain an ordinary type0, which makes the explicit guard necessary.

Use a registry containing ordinary type0 and a legal nonzero sibling. Use valid
geometry and ordinary nonresource records so no missing selector facts can mask
the result. Prove the sibling succeeds and zero gets Facts without changing input.
Remove only the zero guard and show that intended refusal assertion fails while
the sibling control still passes. This repairs the claimed proof, not a request
for a general mutation campaign.

## Verified evidence and accepted limits

All37 indexed receipt hashes and the identities receipt match; all119 pre-existing
sim fixtures remain byte-identical. Final affected Debug and ASan receipts cover
3/3 main plus1/1 required reference contract each. Earlier bounded real-prefix and
picture checks are explicitly reused, not presented as final-head reruns. The
first real-file failure from missing output_goods remains preserved; explicit raw
UNUSED presence now supplies the transient fact instead of an empty-vector default.

Actual original bytes support the concrete catalogue receiver and finite selector:
kind225 decrement, kind226 following goods/current tag, remap draw masking and
first-match row selection. The bounded ordinary admission, unsigned origin mean,
group0x141 exception, import writer flags and fresh observer bypass were checked.
No other blocking semantic finding was identified in that inspected scope.
The independent reference executes20 authored vectors and16 public checks.

The real-file result remains one admitted prefix resource, one supplied draw,
variant6 and height48. It does not prove a complete map cursor. The distinct
variation plane and scratch-owned transactional API are explicit; no World,
registry digest, native persistence, tags or defaults changed.

## Correction and next boundary

Release only the focused [fix packet](../tasks/wildlife-resource-fix-1.md).
Re-review changed code and the two causal proofs before local approval; final
acceptance also needs exact corrected-head CI. Do not repeat unaffected long
sailing/render/corpus campaigns.

After correction review, the next useful integration unit is the original-file
metadata and ordered-prefix adapter: retain raw order/duplicates and explicit
UNUSED, then compose resource and HIRSCH processing up to the first unsupported
record/callback. Do not skip that boundary or claim the4589 candidates all draw.
Storage/tag coordination precedes any production World cursor or persistence change.

## Focused correction re-review — 2026-11-18

**APPROVED locally; exact-head CI acceptance pending.** Both R-F1 and R-F2 are
closed at `0627532f330dc898b80941ea31d9dafe87980aa4`, tree
`365703dd42cb46db3a306465c81a459599f5d336`, tested correction source
`754ef6a01fb6ed416cc4468513625d3a2486d2f1`. Final delta is report/evidence only.
CI: [44372186862](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44372186862).

Same independent reviewers inspected the frozen correction and its actual tests/
raw evidence; no remaining blocking findings. There was no repeated primary-
research or broad-gate campaign.

- R-F1: one invocation-local sorted copy, adjacent duplicate validation and
  lower_bound lookup; evaluate calls only the validated internal selector.
  Standalone select validates its own invocation. Work is O(F log F + R log F).
  Missing current/following facts still fail after the accepted draw; malformed
  metadata fails before it. Input order is preserved in the caller's storage.
- The unsorted2 required plus4096 unrelated facts case executes16 resources and
  observes one32768-byte index payload. Allocation proves copy ownership/count,
  not comparison complexity; the documented complexity comes from code structure.
  Duplicate and missing-following controls execute with the expected consumed prefix.
- R-F2: both ordinary0/800 registry entries are explicitly found. Legal800 succeeds
  first;0 refuses through its sentinel guard. The actual relinked C++ doctest
  mutant removes only that guard and fails REQUIRE_FALSE(zero), with zero accepted
  and the legal sibling still passing. No earlier setup rejection kills it.
- All25 fix receipt hashes, identities and120 parent fixture blobs were checked.
  Canonical Debug6/6 plus required1/1 and ASan5/5 plus required1/1 are clean-source
  correction gates, with no skips, empty/incomplete cases or sanitizer reports.
  Debug reruns the actual bounded prefix once on the new index path. The first
  rig failure remains preserved; the corrected setup copies before vector assign.

The original resource review's bounded semantic approval now stands with these
corrections. This does not prove full-map scheduling, original runtime, production
World integration, persistence or defaults. Exact corrected-head CI must turn
green before landing acceptance.

Next local dependency integration may compose these reviewed helpers with the
reviewed raw decoder/load helper on a separately named task branch. Preserve
pending CI dispositions and intake identities; no candidate becomes accepted by
being cherry-picked into that stack.

## RESOURCE acceptance receipt — 2026-11-18

Exact corrected head0627532f330dc898b80941ea31d9dafe87980aa4 has terminal
SUCCESS in CI44372186862. Root independently checked the public-API run identity
and all12 completed successful jobs. Run receipt SHA256
2c9a0c4659d0412846b3af345ff08a2eb7571ad466b20a43e84802a649db808e;
complete job inventory SHA256
7d5054765ea18f9e29a4d5c520a2ebb8cf7a5edeb3c2665ebec9ff92ba61e248.
With the focused independent correction review, the bounded RESOURCE candidate is
ACCEPTED for normal Windows-owned stacking/landing. No landing performed here.
