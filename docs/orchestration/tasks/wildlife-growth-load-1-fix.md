# WILDLIFE-GROWTH-LOAD-1 — focused reference correction

**RELEASED correction, 2026-11-19.** EXECUTOR II MAC. Continue the existing
`task/WILDLIFE-GROWTH-LOAD-1` from reviewed head
`e89409e17b6173c442fddfa959f70de781689678`; preserve previous evidence and CI.
No new gameplay scope, production loader change, main/board merge or rebase.

Read the growth review, `docs/sim/wildlife-growth-load.md`, the reference's
`numbers()` and its CLI contract, and the C++ parser/refusal tests. Common
CLAUDE/evidence/testing rules remain in force. This is one short correction,
not a repeat of the completed loader package.

## Finding and intended behavior

The reference validates an ignored missing-comma tail with syntax-only `TAIL`.
It accepts `Proz: 20 2147483648` and `Proz: 20 -2147483649`. C++ rejects both
under the documented bounded int32 grammar. This is a reference discrepancy,
not evidence that the production loader is wrong or an original-runtime result.

Before editing, retain the smallest public-command reproduction with legal
boundary tails and the two rejected-by-contract tails. Keep initial actual exit
statuses/output. No historical failed gate is to be recreated.

Fix the reference so every numeric token in a discarded tail also satisfies its
signed int32 domain. Preserve comma-stop semantics: the legal tail is validated
then discarded, not appended to the row or staging. Do not weaken C++ or change
generic `text::ints`. No change to production lookup bytes or original assets.

Add paired public reference controls and C++ boundary checks: INT32_MAX and
INT32_MIN tails succeed with the same full-map result as the no-tail control;
one-over/one-under refuse. Include a multi-token discarded tail with an invalid
later token so checking only its first value cannot pass. Assert source/output
preservation on refusal using existing harness conventions. Restoring the old
reference tail check must fail the intended new CLI control while legal cases
still execute; no generic mutation campaign.

## Efficient verification and stopping point

Run the changed public reference contract and its authored agreement. Run only
the affected C++ boundary/refusal cases in canonical Debug and ASan/UBSan, with
required reference contracts. Reuse unchanged production, original-file and
other focused results with an exact dependency explanation; do not repeat the
whole matrix, original sailing/opening or all five previous mutants. Verify that
accepted authored maps and original lookup digest remain unchanged by re-reading
with the corrected reference, not by regenerating expected answers from C++.

Freeze source, format/check changed files, ordinary-push the existing task branch
and send READY FOR FOCUSED RE-REVIEW directly to **Orchestrator II Mac** with
exact head/tree/CI, the before/after counterexamples, corrected reference controls,
affected Debug/ASan counts, reuse justification and NOT_RUN. Stop there. No
reviewer agents or self-approval. The next kernel-binding packet is prepared and
will be released directly after this finding passes focused independent re-review.
Keep other jobs/coordinators and the owner's awake assertion untouched.
