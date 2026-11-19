# WILDLIFE-GROWTH-LOAD-1 — Mac orchestrator review, 2026-11-19

**CHANGES REQUESTED: one P2 reference-contract discrepancy.** Production review
is approved within its bounded domain; the packet is not accepted until the
reference correction receives focused re-review and exact-head CI succeeds.

- Head `e89409e17b6173c442fddfa959f70de781689678`;
  tree `4e8d67737226ec6d3ec9f4a71a44c234fa4f0858`.
- Final tested source `0dc52331c39dd63be33eee691e4adb00b56acfda`;
  first Debug source `6e980f7931c40869fb38c3fd35d7c38dd83517db`.
- [Exact-head CI 44475449514](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44475449514),
  in progress at review; its status cannot waive the finding below.

## P2 — discarded numeric tails lack reference range checks

`tools/wildlife_growth_ref.py`, `numbers()` around line 42, validates a discarded
missing-comma tail using syntax-only TAIL matching. The independent evidence
reviewer reproduced reference acceptance of `Proz: 20 2147483648` and
`Proz: 20 -2147483649`. C++ rejects both through its bounded int32 parser, as the
public supported-domain contract requires. Existing overflow checks only covered
the first parsed value and missed this branch.

Fix only the reference and targeted controls. Add legal INT32_MIN/MAX tails,
one-under/over refusals and a bad later token in a multi-token discarded tail.
Legal tails must remain discarded with unchanged lookup output. Preserve the
initial counterexample and demonstrate that the old reference fails the new
public-command control. Do not widen the C++ domain or alter generic text::ints.
[Focused correction packet](../tasks/wildlife-growth-load-1-fix.md) is released.

## Other reviewed findings and retained evidence

Primary/code reviewer `/root/review_docs_sim_dispatch` independently approved
section/default/256-word staging, Id and batch lifetimes, comma-stop retention,
low-byte and packed/file-order behavior, bounded grammar refusals and owned
source diagnostics. No blocking production findings. Generic text semantics
remain unchanged, and no original city/scene provenance is claimed.

Evidence reviewer `/root/review_town_ci_close` verified reference-first commit
`076170fa`, 13 authored full maps with nonzero staging, all legal production-TU
setups and five intended numerical mutant failures. The legal run has 972
assertions; wrong-source failures are 1/2/2/7/1. Real-file comparison uses a new
256-word independent reference, not the old oversized probe as its sole oracle.

The CHECK-to-REQUIRE correction is accurately reported: corrupt oracle fails
without agreement text; legal sibling agrees. Debug 13 plus two contracts at
`6e980f79`, corrected asset case 1/1 at `0dc52331`, and reuse of 12 unaffected
cases reconcile. ASan runs all 13 plus two contracts at the corrected source.
Root verified 105 manifest hashes and all 30 published rows, and confirmed final
delta is documentation only. No gates repeated during review.

## Next work

The next kernel source-binding packet has passed independent scope review but
is NOT RELEASED until this focused correction is approved. It will decode an
owned selected 25-byte kernel from supplied original-module bytes, with bounded
layout validation, authored PE controls and an independent real-file extraction.
No module execution, public original coefficient tables, scene/city/fill/World
integration or identity change. Owner navigation decision and Windows raster
capture remain separate. Executor reports the correction directly here; main
and all previous evidence remain untouched.


## Focused correction approved; hosted acceptance remains open

Independent evidence reviewer `/root/review_town_ci_close` APPROVED the correction
at `fe4355fd719fad0ecafbe9322788f4b7d885a7af`, tested source
`464e49350a8a6208469d34bd03630c515e7e5335`, with no remaining local findings.
P2 is closed. Legal discarded tails remain discarded; all overflow positions
refuse. The exact old reference fails the new intended control after legal cases.
The 39 CLI controls and 13 authored maps agree. Focused Debug and ASan/UBSan each
pass 3 main cases plus one required contract, no failures/empty/skips/sanitizers.
Production is unchanged; prior unaffected evidence reuse is justified. Reviewer
independently confirmed the original 16,384-byte lookup remains identical.
Root verified all 62 manifest hashes and 27 published receipt rows; no gate repeated.

[Corrected-head CI 44478146499](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44478146499)
is still pending at this publication. Jobs receipt SHA-256:
`2c6281c62196d213d9dd82d8e5e5eed82c7fecbe3ed84e167d00e5aad8ad1be0`.
The earlier e89409e1 run 44475449514 has a FAILED Linux Clang ASan **test** step,
job 135394843716; build passed. Public annotations contain selected log excerpts
but do not identify the actual failing test/cause. That failure remains unresolved,
not waived as infrastructure or attributed to the corrected Python predicate.
The prior successful local sanitizer result is not a substitute for hosted CI.

This is LOCAL REVIEW APPROVED, not ACCEPTED. Root permits independent stacked
continuation to [WILDLIFE-KERNEL-LOAD-1](../tasks/wildlife-kernel-load-1.md) on
fe4355fd while hosted CI/old failure triage remain separate. No production
navigation correction, version allocation or live wildlife wiring is released.


## Terminal exact-head CI — accepted correction

Corrected head `fe4355fd719fad0ecafbe9322788f4b7d885a7af` is now **ACCEPTED**
within its independently reviewed scope: all 12 jobs, including five C++ lanes
and aggregate gate, succeeded in CI 44478146499 at that exact head. Terminal
jobs receipt SHA-256:
`ae5f46a5c06dd6bf847aa44da5ffceb3f7b4cb77475ff516257c96b4e7574cdd`.
No retry or repeated local gate. This does not explain or erase the earlier
44475449514 ASan test failure, whose underlying assertion remains UNKNOWN.
