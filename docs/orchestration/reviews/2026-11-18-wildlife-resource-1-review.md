# WILDLIFE-RESOURCE-1 — independent review, 2026-11-18

**CHANGES REQUIRED: two bounded P2 corrections.**

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
