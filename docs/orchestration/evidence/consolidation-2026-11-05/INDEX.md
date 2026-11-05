# Consolidation evidence — 2026-11-05

Frozen accepted code: `7b57ebd968445bdb8ab8f0ad3e565632a7df24ee`.
The publication commit adds documentation only. See the
[consolidation review](../../reviews/2026-11-05-consolidation.md).

| Gate | Selected | Executed passes | Failed | Skipped |
|---|---:|---:|---:|---:|
| Debug sim/formats/input/replay/references | 1606 | 1603 | 0 | 3 opt-in |
| Affected ASan/UBSan, viewer enabled | 166 | 166 | 0 | 0 |
| Debug host/coordinator and original-resource case | 96 | 90 | 0 | 4 platform, 2 opt-in |
| ASan/UBSan host/coordinator | 95 | 89 | 0 | 4 platform, 2 opt-in |

All four have zero empty/incomplete cases and zero sanitizer reports. Required
contracts run separately; their counts are not added to selected-case totals.
The four rows overlap: they are not a count of unique tests.

`gate-summary.json` preserves exact commands, source/workflow provenance, hashes,
terminal verdicts and contract/selection counts extracted from raw logs.
`MANIFEST.sha256` hashes retained raw logs and selection inventories relative to
`cache/consolidation-2026-11-05/` in the main checkout. These raw artifacts are
Mac-local and git-ignored; the index is not a claim they were uploaded to GitHub.
`external-coordinator.json` separately identifies the actual predecessor binary
used by the handoff tests, since inherited executable variables are not captured
in the canonical runner's normal provenance.

The initial `combined132-debug.log` is FAILED: 1625 selected, 1602 executed
passes, 13 failures and 10 skips. It includes stale integration assertions,
a population coordinator-connection timeout, and a scene/render timeout from an
over-broad selector. Subsequent gates correct the assumptions and anchor the
headless selector; they do not establish the excluded rendering case passed.
`final-focused.log` failed the skip-message preflight; `final-focused-02.log`
failed an original-player-balance assumption. Both corrections are covered by
the final gates. Earlier probes and failed/invalid attempts remain in the cache.

The reporter's legal control and truncation mutant, the forced-move N2 mutant,
formatting and prior focused startup gates have separate retained logs. Reference
fixture generation is generation evidence, not an acceptance run. Target-host,
original-runtime, long-corpus and late-game coverage limits remain in the review.
