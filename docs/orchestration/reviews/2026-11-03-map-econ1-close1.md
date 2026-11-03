# MAP-ECON-1-CLOSE-1 — acceptance, 2026-11-03

**No findings. ACCEPTED for the local headless scope.** The public-end defect
from the [forestry review](2026-11-03-map-econ1.md) is closed. The full forestry
unit now has accepted normal opening and termination behavior; another forestry
correction/review round is not assigned.

## Accepted identities

| Input | Exact pin |
|---|---|
| Termination fix and regression tests | `1e9c770e6f0a44d84aa40fb5b04bad1818875a92` |
| Independent bytes, alternate goods/owner and walk controls | `44096b840bde4760e9b0520d1a9afaa1b0ec7288` |
| Clean gate source and contract | `e6b1083c4a967bb4b1f792e557929e271d0602ad` |
| Final candidate/report | `f40a76022c609092156c457509f0c49e7905f96e` |
| Clean gate workflow | `f41e87da4cb8958ed1069d034980d1ca796cda15` |

Post-gate f40a7602 changes REPORTS only. The accepted stream is **126**, PRNG
timeline **4**, and the command wire is unchanged. CLOSE-1 changes internal end
handling, effects and validation, not those formats or draw rules.

## Direct review and independent checks

Reviewer: **Codex orchestrator `/root`, 2026-11-03**. Root directly inspected the
entire closeout diff and relevant end/sync/merge/expiry, ownership, validation,
persistence and normal-completion callers. No source review was delegated. The
candidate worktree remained clean. Agent `/root/review_evidence_workflow`
reconciled saved logs and provenance as bookkeeping; it did not review source.

`finish_harvest` runs before `release_home` for a non-Done harvest job. It leaves
target/cargo as history, normalizes terminal state, and issues a target-specific
Release effect while the home still names the island. The merge applies that
effect only under enhanced hygiene and only to the matching owner. The stamp-
set seam path uses the same normalization; repeated ending and normal Done
completion are no-ops for this helper. No world scan or loader repair is added.
Non-harvest end behavior remains unchanged.

Root rebuilt the **original independent reviewer probe** against the fixed
production libraries. It now passes **42/42 assertions** under faithful and
enhanced rules. Its before-end positive, same-seam reload, after-end validity,
reload and enhanced claim-release checks all pass. The earlier corrected probe
failed six of those assertions. Outputs remain under main's ignored
`cache/review-map-econ1-e2-e6/end-probe-close1.out`; no broad test suite was
repeated by root. The current Debug executable SHA-256 matches the frozen D2
identity, so this check did not use the retained mutant binary.

The committed regressions cover all four active states, both rules, public end
and direct stamp synchronization, repeated end, expiry, newer-owner protection,
normal completion and non-harvest controls. The 96-assertion fail-first result
and five deliberate cleanup faults are retained. Root inspected the literal
49-byte populated HarvestState transcription and the non-empty ordered owner
map: independent field widths/order/values, optional presence, counts, route
points and ids are checked. The other-goods/other-owner fixture exercises
actual consumption and delivery, not just spawn metadata. The exact and
fractional walk cases pin the previously accepted integer approximation.

## Gate evidence and reuse

The canonical reader and identical saved inventories reconcile:

| Gate | Selected | Executed passes | Failed / empty / skipped / incomplete | Sanitizer lines |
|---|---:|---:|---|---:|
| Debug D2 | 202 | 202 | 0 / 0 / 0 / 0 | 0 |
| ASan/UBSan A2 | 202 | 202 | 0 / 0 / 0 / 0 | 0 |

Both additionally require and execute the forestry reference (**83 vectors**)
and public contract (**17 methods**). Required assets were enabled. Selection
explicitly excludes the long openings. Logs and inventories are in the
candidate's ignored `cache/map2-econ1-close1/`.

- Debug test executable SHA-256:
  `475e110ff36573331de595a33533b0f48b2535ec4377cbc95096bcf6dfa720a6`.
- ASan test executable SHA-256:
  `9352460c4130af00682ccb4670715463f326af6f8b1b2afc0995154c21fce528`.

The prior D1/A1 evidence, real opening's **448 wood units = 1.75 t** of new
warehouse receipts, and **28/28 fresh-process replay checkpoints** are retained
at their original pins. Recomputed recording hashes match their saved E5
identities; altered-command divergence remains entry 17187. They were not
rerun. Reuse is justified: the default harvest body reaches Done before ending,
the new helper leaves Done untouched, the new Release/loss effects are absent
on that path, and the stricter validator does not mutate state. Original
harvest/transport bodies, serialized fields and draw timeline are unchanged.

The report describes two independent approvals and their inspected scope/pin,
but still omits unique reviewer identifiers/retained receipts. Those approval
descriptions remain author-reported; this direct root review supplies a distinct
independent acceptance. If the old identifiers are readily available, append
them during the next ordinary report. Missing metadata does not require another
source review, gate or archive-reconstruction project.

## Limits and next work

This accepts the generic worker mechanism and the demonstrated forestry chain,
not all grain/plantation economies. Faithful generic ending can retain the claim
bit as specified; enhanced cleanup is opt-in. The finer-route gap, faithful
replaced-tile behavior and sub-tick arrival approximation remain documented.
Windows/MSVC, Linux/WSL warnings and sanitizers, original-runtime observations,
GPU, large multiplayer/economy performance and broad crop-chain fidelity remain
NOT_RUN or unverified as recorded. macOS is development evidence only.

Windows keeps exact `0c401c8d084cfd9feb8aec846595291f87f22cce` and its current
W4-OPENING-1 scope. Publishing this accepted Mac branch does not change that
integration input or merge gameplay into main.

The next Mac assignment is **MAP-POP-READY-1** in TASKS: reconcile existing
population prerequisites and draft the smallest bounded immutable-input/house-
calculation contract. It reuses earlier audits rather than starting another
forestry repair or broad research campaign. Live house ticks and unreviewed
prerequisite integration are not released by this acceptance.
