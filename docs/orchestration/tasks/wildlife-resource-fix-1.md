# WILDLIFE-RESOURCE-FIX-1 — close the two focused review findings

RELEASED2026-11-18 to EXECUTOR II MAC. About1–2 hours; preserve the frozen
RESOURCE report and first failures. Continue your existing owned
`task/WILDLIFE-RESOURCE-1` branch from3aab2a93680a9edf97ab4c5b0ac932eaae130513;
ordinary commits/push only. No merge/rebase/main push or other-lane changes.

Read CLAUDE, PLAN0.1, testing/evidence-contract.md, testing/README.md, the
[review](../reviews/2026-11-18-wildlife-resource-1-review.md),
`docs/sim/wildlife-resource-initialization.md`, resource_import header/source/tests,
existing reference/contract and the preserved evidence index. Do not restart
reverse engineering or expand the supported placement profile.

## F0 — pin causal checks first

Record both findings and positive/negative proof in the existing acceptance table.
R-F1: document current O(R × F²) validation from the actual call path. Add a small
many-unrelated-facts case that verifies outputs and deterministic work/structure;
avoid wall-clock timing thresholds and unnecessary public instrumentation.
R-F2: use valid ordinary nonresource types0 and a nonzero sibling, both actually
registered. Assert lookup of0 succeeds before exercising evaluate, so the test
cannot fall back to missing type. No zero metadata row is needed for ordinary
placement. Valid climate/geometry must reach the record guard. The sibling must
succeed; zero must return Facts at record0/consumed0 and leave inputs unchanged.

## F1 — one validation/index preparation per evaluator invocation

Replace preceding-prefix duplicate scans with a deterministic sorted ID index
(or equivalent ordered index). Build and validate it once for evaluate; selectors
use indexed lookup without revalidating the table for each resource. Standalone
public select still validates its own invocation. Keep all input ownership,
missing-current/following-fact error timing, record index and consumed-prefix
semantics unchanged. No registry/digest/state fields, static mutable cache,
callbacks or new generalized infrastructure.

Use a minimal internal selector operating on prepared facts. Show the indexed
path handles unsorted input, duplicates, many irrelevant facts and following-id
lookup without changing the20 authored results. Keep the proof bounded and
maintainable; no performance project or sweep over all islands.

## F2 — correct the zero-sentinel regression

Add the F0 positive/negative pair and apply a single private mutation removing
only record.type.v==0 refusal. The correctly registered zero must then reach
placement and fail the intended assertion; the sibling must still execute and
pass. A missing-type refusal, missing facts or malformed setup does not kill
this mutant for the intended reason. Preserve source/binary identities and output.
Canonical sources must be restored/unmodified and the canonical binary rebuilt
before final gates. No repeated unrelated mutants.

## F3 — focused verification and direct reportback

Run canonical focused Debug and ASan/UBSan on the affected helper tests and
reference, requiring its public contract. Run the bounded real-prefix Debug case
once because index preparation/lookup now lies on its path. Reuse unchanged
picture/long-sim/render evidence with explicit identities and justification;
no full gate. Run relevant format/lint/compiler checks. Retain admission and
existing shared-host limits; no process/coordinator interference.

Freeze, ordinary-push your task branch, and send READY FOR RE-REVIEW directly to
the Mac orchestrator: exact head/tree/source, affected gate counts, both causal
proofs, preserved failures, CI run and limits. Report NOT_RUN explicitly. The
orchestrator supplies independent focused re-review; do not spawn reviewers or
self-certify approval. Stop before the next adapter/World/persistence task.
