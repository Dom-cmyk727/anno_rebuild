# TOWN-START-1 — local measurement reviewed; CI acceptance blocked

Reviewer: Mac orchestrator, direct independent read-only review, 2026-11-18.
**Verdict: local measurement review passes; NOT ACCEPTED for landing. CI closure is required.**
No outstanding behavioral or evidence finding was found in this bounded measurement.
This is not proof of a sustainable or balanced economy, a Settler upgrade, or an irreducible
engine blocker. No map selection or default-on decision follows from it.

## Frozen identities

- Report head: `156badab35516609e92796e85a4fb0b9314ddac6`, pushed on `task/TOWN-START-1`.
- Final test code: `c9132c39b3edf466be4260fa51a65dc22f7bc564`.
- Corrective experiment: `f36c22fa50cff7d1ade22b804d9216ef5feb8060`.
- Initial experiment: `8415cf82`; accepted setup base `ad360af024543a71e083dc3de714b7f48f354e63`.
- [Failed source CI 44255019025](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44255019025).
- [Final-head CI 44256560982](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44256560982):
  in progress at review. No green outcome is inferred from it.
- Executor evidence: `docs/orchestration/evidence/town-start-1/README.md`, both checkpoint
  TSVs and `sha256.json` at the report head; raw files under `cache/town-start-1/`.

## Independently checked

| Area | Review result |
|---|---|
| Scope and calls | Read the complete helper/test diff, `real_start`, loader call, public-command helpers, living preset, house scoring and stage pass. Only tests and docs changed after setup; no state, rule, fixture, production or build-workflow change. Actual worlds use commands and ordinary Advance, without relocation or grants. The diagnostic modifies only a local House value and checks that World hash is unchanged. |
| Population target | Read the original BGruppen capacity/stage entries and the production stage accumulation. Eight residents per Pioneer house and 125 required residents imply 16 houses. The code divides residents_64 by 64 before the stage comparison. |
| Upgrade interpretation | Re-derived the positive goods threshold from the original next-group table and `(mean + 4) >> 3`: mean must reach 68. The four-term average means cloth alone contributes at most 32. Missing leather alone is not established as the necessary blocker. |
| Determinism and reporting | Independently compared both complete corrective baseline files, then reconciled every column of all 61 rows in each published initial/corrective table against the raw baseline and control records. No discrepancy. Corrective baseline SHA256: `9fd9f51f5973b5f132a93ba8271fb6559bbaa88dfc2d9ef26d972c90df7ee1f1`. |
| Gates | Re-parsed the canonical corrective enabled, default-off and final linkage-control logs with the verdict reader. Enabled: 1 selected/executed, no failures or skips. Default-off and final control: 2 selected, 1 executed, 1 opt-in skip, no failures. The earlier required picture contract passed 1/1. No enabled ASan run is claimed. |
| Provenance | All 59 indexed raw evidence files match their recorded SHA256 and size. Compact committed evidence is about 37 KiB. No absolute host paths appeared in the committed packet diff. Original game files remain private. |
| Reuse | Final code differs from measured code only by a comment and an unnamed-namespace wrapper; independently stripping those reproduces the measured file byte-for-byte. Same-recipe measurement reuse is justified. Setup's full pins campaign is reused for unchanged production/rules/fixtures, not reported as a new run. |
| Warnings | Retained Apple Clang 21 warning check: 1,006 project compilations and two deliberate shadow controls. Both historical compiler labels used Apple Clang; this does not establish local GCC/Clang-on-Linux coverage. |

## What the experiment establishes

The single revised opening reaches 16 capped Pioneer houses and 128 residents after 900,000
post-founding ticks. It does not reach Settlers. At the final checkpoint, 15 houses first fail
goods and one first fails food; goods means are 0–32. Inn and chapel construction occurs only
23.5 and 28 game minutes after founding. Their late and incomplete supply is not a steady-state
measurement. The remaining salt mine waits for 2,096 raw wood units; the chosen alcohol chain
has no potato input and no supported crop-planting action in this opening.

Omitting both food producers leaves no surviving houses or residents and produces 273 food-first
house-samples, versus one in the baseline. This establishes recipe-level sensitivity, not isolated
per-producer attribution: the trajectories and later costs differ. The initial three-house
experiment's food control remains inconclusive and is preserved separately.

The bounded stop is correctly reported as unmet predicates and incomplete supply. No third
opening, enlarged budget, stock injection, production repair or global impossibility claim is needed.

## CI findings and disposition

1. **Town-owned GCC Debug linkage:** `Candidate::p` used an unnamed-namespace type in an
   externally linked helper. `c9132c39` gives the helpers internal linkage. The fix was reviewed,
   rebuilt and locally checked; final-head GCC CI must still confirm the diagnostic is gone.
2. **GCC Release freight build:** `freight_state.hpp:33` reports potential null dereference while
   compiling `freight.cpp`. Those sources and their build/workflow files are unchanged from setup.
3. **Clang ASan build:** libstdc++ 15's `ciso646` diagnostic fails unchanged test-support compilation
   under `-Werror`. This is a build failure, not an executed sanitizer finding.

The latter two need separate base/toolchain triage coordinated with the landing orchestrator.
Unchanged source does not prove a compiler cause or waive the failures. Preserve all first-run
failures; no warning suppression or blind retries. A docs-only green run cannot erase the failed
source builds. Acceptance remains blocked until the relevant exact-head CI is green.

## Next dispatch

Release WILDLIFE-1a independently from main `b5c0ada209d94a0082bc88d793c917c7222e6a21`.
It consumes neither the town measurement nor the setup delta, and does not land either packet.
This sequencing keeps gameplay development moving while CI closure remains explicit. The executor
preserves the town branch/evidence, reports the final CI disposition directly, and starts the pure
helpers only. World integration, owner policies and acceptance remain outside that release.
