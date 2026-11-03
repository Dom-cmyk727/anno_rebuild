# MAP-POP-READY-1 — review, correction and implementation release

**Accepted as a planning/implementation input after the correction below.**
The readiness task is complete. The next assignment is the larger
**MAP-POP-IMPL-1** bundle in TASKS; its package reviews unlock subsequent work
without intermediate orchestrator handoffs. Final orchestrator review occurs
once, on the combined implementation candidate.

## Reviewed identities and correction

| Input | Exact pin |
|---|---|
| Accepted forestry baseline | `f40a76022c609092156c457509f0c49e7905f96e` |
| Executor's corrected population draft | `dc9491727929a95c4f7907e8b4bd453029a9abbe` |
| Executor report | `b77860e25d460450916d70ed4b25822d1618a8c2` |
| Orchestrator's reviewed documentation correction | `2f876af5a44fc3395819134e79a9595a13d9f7d9` |

The executor changed only the two population documents and REPORTS; the report
commit changes REPORTS only. No implementation, format, golden, map or asset
change, C++ build, sanitizer or runtime capture is claimed.

**[P2, corrected] Size the goods lookup by its index domain.** The original
`population-inputs.md` §3 said the vector length was the need count n, contrary
to §2.9. A shipped six-need group indexes goods such as WIRTSHAUS 76, so a
six-entry table is incorrect. Original AnnoGame `0x10014afc–0x10014b09`
initializes 96 goods-domain entries with the need-count sentinel, then
`0x10014b1d–0x10014b1f` indexes by goods id. Root corrected the construction
step to use the registry goods count as length and n only as sentinel. C11 now
correctly gives a goods-domain-sized, zero-filled lookup for zero needs, not an
empty lookup unless the goods domain itself is empty. Root also corrected the
definition of accepted: scoped acceptance does not imply merging into main.

The original read-only review finished before these edits. Root then authored
only this small documentation correction on the stopped task branch.
Independent reviewer `/root/review_evidence_workflow` approved that separate
correction against b77860e2 after checking the original post-pass excerpt and
the three-file scope. No new executor repair/review turn was required.

## Direct review and evidence

Reviewer: Codex orchestrator `/root`, 2026-11-03. Root read the full new
readiness/input documents, the report, and relevant current importer, registry
and House dispatch. The prerequisite inventory is correct: P1 data is present,
the full scenario player-mask import and per-goods/derived-group inputs are
missing, and House dispatch remains a deliberate no-op. Old T32.6/P2.1 refs
exist but are not ancestors; their mere presence does not accept their code.

Root independently inspected original instruction spans for the tick and mood,
player-mask reconstruction, Ware endpoint update and group post-pass. It also
checked the Create prologue and stamp call: three SEH pushes, 0xbc local bytes
and four saved registers place `[esp+0xe4]` at argument 3 at the cited call.
The callers supplying that argument remain unread, as reported.

Original identities match the packet:

- AnnoWelt: `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
- AnnoGame: `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
- BGruppen: `63e876fd3afedca214b3d9cc45f022f2c595f473839690e96f34e12ed7849e3b`.

A separate root script read the original stage pointers and BGruppen records,
not the executor's discarded model: 56 Ware rows, 23 BauFirst keys, 4 IdSec
keys; all five groups' n/endpoints/forced masks/tavern aliases agree, as do the
need-stage indices (maximum 21). Primary instructions confirm unsigned u16
gate/last increment, toward-zero smoothing, masked decay, spawn threshold,
mood precedence and the signed below-64 comparison. Root artifacts are ignored
under `cache/review-map-pop-ready1/`; originals were not edited or committed.

The executor's independent-review claims and original scratch scripts cannot
be fully reconstructed from the named evidence: the report gives a reviewer
role and reviewed revision, but no durable id or scratchpad path. They remain
author-reported. This direct review provides separate evidence for acceptance;
no archive-reconstruction or broad RE campaign is needed.

## Binding decisions for MAP-POP-IMPL-1

These settle routine design/domain choices for the larger implementation. Put
them into the branch contract in P0 and have that internal checkpoint reviewed;
do not request another owner handoff for choices already stated here.

1. **Own saves/resync stay exact (OD-1).** Preserve saved last and all other
   authoritative state. No reset option or user-load/resync split is assigned.
   Original-format import remains its distinct documented compatibility path.
2. **Create timing stays evidence-only (OD-2).** Read the named caller paths in
   the parallel readiness subtask. Do not change construction stamps or enable
   live ticks based on a guess. Its unresolved edge does not block metadata or
   the pure calculation API.
3. **Use widened, checked arithmetic (OD-3).** Keep the existing widened state
   representation; no new legacy-wrap Rules option. Use sufficiently wide
   intermediates and explicit typed outcomes when a requested output cannot be
   represented. Original-width numbers may be labelled diagnostics, not values
   silently committed over widened results.
4. **Undefined/unsupported calculations are explicit (OD-4).** A negative
   smoothed E, a reached zero divisor, invalid score index or unavailable required
   input produces a typed NotEvaluated/domain result, not successful unchanged
   output, a guessed clamp, a wrap or a native fault. Preserve defined outputs
   separately where useful. Zero E is not a blanket error: mood returns zero
   before reading tables, while the earlier needs loop faults only if a selected
   need reaches its divide. No live skipping/clamping policy is chosen here.
5. **Previous-group capacity is explicit (OD-5).** Read it from the preceding
   group where one exists. The pure API can take an explicit value, including
   zero, for synthetic/mod inputs. When a path actually needs it and none is
   available, report that input as unavailable. Do not read adjacent memory or
   silently assert that an untraced original layout accident is established.
6. **Use configurable population goods roles (OD-6).** Approve the four named
   SimConfig roles/range with original defaults; hash/serialize that config.
   Derive tables using the actual config. Bind an immutable derived view to its
   registry/config and reject mismatches, or key it by both; never reuse tables
   built under different roles. Parent may choose the physical cache arrangement
   during P0 without another permission turn. Empty non-population registries
   remain supported; population-specific cross-checks must not break them.
7. **Approve the declared parser domain.** Unknown stage/goods names, stage
   indices outside the current 32-bit mask, UNUSED as an active need, and zero
   demand are explicit diagnostic refusals for new population inputs. These are
   safe-domain adaptations, not claims about untraced original error handling.
   Stage names 0–31 may use the checked original mapping, as route names do.
   Keep the original defaults for absent metadata. Do not impose goods<=95,
   needs<=16/255, infrastructure<=32 or other original-count limits.
8. **The first need is positional in the original mood calculation.** The
   function reads slot 0 unconditionally when E is nonzero; do not silently
   reorder needs or add a hardcoded NAHRUNG-id branch. An empty group still has
   valid derived metadata; its calculation domain must explicitly handle a
   missing required first slot. The below-64 correction applies only on the
   downgrade arm, after the below-floor test. The removal arm bypasses it
   (`0x10076547–0x10076551`). Add a distinguishing guard case before porting it.
9. **One identity change for the unpublished combined input pipeline.** Reserve
   stream 127 from the current 126 baseline for the full mask, metadata and
   config changes. If newer owned work has already used it, select the next
   unused branch version in P0 and record why. PRNG timeline stays 4 and command
   wire stays unchanged. Derived caches and a pure function need no additional
   version bump. Old-version tests must use correctly resealed current-layout
   bytes beside a valid positive control.

## Larger execution boundary

The bundle implements original-scenario mask import, goods metadata/config,
derived groups, an independent numeric reference, a typed pure House kernel,
and a real-registry/scenario-to-calculation integration canary. It also performs
the bounded live-readiness reads in parallel. Every internal substantive package
gets read-only independent review and focused fixes; approved packages continue
automatically. The executor returns after the final combined review/gates, not
after each component. A new semantic conflict stops its dependent work only.

**House dispatch remains a no-op in this bundle.** The original's ordinary
towns reach walker spawn/PRNG and lifecycle branches; silently omitting those
would misrepresent a live tick. No walkers, taxes, transitions, unlocks or fake
live integration are released. The real-input canary checks numeric results
outside authoritative state and must not count an unchanged hash as proof of
calculation. This boundary delivers substantial engine code while keeping the
next live-population decision explicit.

Windows stays on exact `0c401c8d084cfd9feb8aec846595291f87f22cce`. Publishing
the corrected documentation and later Mac implementation branch does not merge
main or alter that integration input. Cross-platform, original-runtime, GPU and
scale claims remain limited to actual executions; no C++ test was needed to
accept this documentation-only packet.

## Dispatch consistency review

Independent reviewer `/root/review_windows_dispatch` approved the root-authored
TASKS implementation bundle and this release record with no actionable findings.
Its scope was document consistency, binding decisions, actionable package/proof
boundaries, resolving read paths/pins and Windows preservation, not review of
the executor's original candidate source. It verified all seven W4 task sections
byte-identical to the saved pre-dispatch board. This is separate from the
independent correction review above. No C++ build or test result is claimed for
either documentation review.
