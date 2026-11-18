# TOWN-START-1 bounded measurement

Status: READY FOR REVIEW / CI BLOCKED. This is a measurement, not a gameplay repair or an accepted landing. The [frozen plan](../../../sim/town-start-1.md) preserves the initial recipe and the single corrective opening separately. The measured corrective source is `f36c22fa50cff7d1ade22b804d9216ef5feb8060`, on accepted setup `ad360af024543a71e083dc3de714b7f48f354e63`. The later source correction `c9132c39` changes only helper linkage: removing its one comment and unnamed-namespace wrapper makes the file byte-identical to the measured recipe. The explicit linkage-only proof and final rebuilt binary hash are in cache. The enabled experiment is reused, not rerun; subsequent publication edits are documentation only.

## Scope and acceptance evidence

| Requirement | Positive proof | Negative/control proof | Evidence | Review |
|---|---|---|---|---|
| Normal ship opening only | Sail, Found, Unload, Build and road commands; normal Advance | All refusals retained; no world patch | Raw COMMAND/TRANSFER rows | Orchestrator pending |
| Original setup, explicit rules | Unchanged candidate A, seat 0, seed 26031; house_updates, city_booking and fisher_rules selected before creation | Default-off opt-in gate skips; no defaults change | Plan, metadata, default-off logs | Pending |
| Derived population target | Stage 3 requires 125 whole people; Pioneer capacity 8; target 16 | Initial three-house town has only 24 people and chapel refuses | POPULATION_TARGET, both checkpoint tables | Independently checked by orchestrator; final disposition pending |
| Determinism | Two fresh worlds produce byte-identical full traces and 61 hashes in each experiment | Full trace comparison, not only final digest | run-01 and run-02 baseline pairs | Pending |
| Food sensitivity | Corrective baseline retains 128 people; omission ends with none | Food-first house-samples 1 versus 273; pure diagnostic food-zero check also asserts unchanged World hash | Both arm traces and summary | Pending |
| No identity movement | Production, rules, fixtures and pins unchanged from accepted setup | Git delta restricted to measurement tests and docs | pins-reuse.json and setup full gate | Pending |

## Results and units

The budget is exactly 900,000 ticks after founding, 30 game minutes. Normal visits are 100 ms / 50 ticks. Founding occurs at absolute tick 85,850; the final tick is 985,850. Checkpoints include founding and every 15,000 ticks. A first Settler observation would have 50-tick resolution; none occurred. Raw quantities labelled `_88` or `raw_88` are goods units with 256 per ton; residents_64 has 64 per person. The tables contain own-run measurements, not original data tables.

- [Initial 61-checkpoint table](run-01-checkpoints.tsv): 3 houses / 24 people at the end. Baseline food 883 raw, tools 11,264, wood 5,888, bricks 0, money 27,953. All three capped houses fail goods. Chapel Build at tick 100,850 refuses ConstructionDenied. Omitting only the farm also ends with food 883 and no food-first samples: **INCONCLUSIVE** for production dependence. The initial traces and failures are preserved.
- [Corrective 61-checkpoint table](run-02-checkpoints.tsv): 16 houses / 128 people, all capped, no Settler. Final money 24,648; tools 4,608 raw (18 tons); wood 1,744; bricks 0; food 2,182. First failures: goods 15 houses, food 1. Food scores 105–128, goods means 0–32, infrastructure 0 in every house. These are projected predicates on the actual houses, not counts of scheduler upgrade attempts.
- The inn is built at absolute tick 790,850 (23.5 game minutes after founding), chapel at 925,850 (28 minutes). Only 6.5 and 2 game minutes respectively remain for their effects. Chapel need stock reaches 15 houses; inn need stock is 0 in all 16 at the end. Late construction and incomplete supply must not be described as a long steady-state experiment.
- Forced goods are an average, not a conjunction. AnnoWelt UpgradeCheck `0x10075dd0`, [population-live section 5](../../../sim/population-live.md), maps `(mean + 4) >> 3` through BedarfTab. The parsed next-group table first becomes positive at index 9, requiring mean at least 68. The four terms here are leather, cloth, salt and inn. Cloth alone contributes at most 32; observed means 0–32 fail. This does **not** prove leather alone, or wildlife, is mandatory.
- Remaining salt-mine build waits for wood: 1,744 available / 3,840 required, shortage 2,096 raw, with 4,608 tools / 1,280 required. Quarry and brickmaker wait for stage 5 while city mask is 15 (stages 0–3). Stage 5 requires 50 Settlers by the parsed table, not more Pioneer population. No global no-site refusal or denied Build occurs in the corrective baseline.
- The chosen SmallFarm food line operates; its sampled potato raw and alcohol output remain 0. A read-only `insel.py` inventory finds 0 young type 708 and 0 ripe type 709 objects in the source template for island 0 (source SHA256 in the index). [Small-farm-supply section 1](../../../sim/small-farm-supply.md#1-the-registry-records-p-anlagendat-figurenheaderdat) identifies potatoes as the alcohol input. Crop flags 0x29 are outside the currently supported placement classes in placement.cpp, so this opening cannot plant that missing crop through an existing Build command. This is a known missing action for the **chosen alcohol chain**, not proof that every possible alternative chain is impossible. No fields or alcohol were injected.
- Corrective omission arm removes both 809 and 810 from the declared priorities. It still builds 16 houses, but finishes with 0 surviving houses/residents and food 0. Food-first failures rise from 1 to 273 house-samples. The observation is a recipe-level sensitivity result; trajectories, later building timings and expenses diverge. It is not a per-producer mass-balance attribution.
- The flag-enabled creation path has 420,795 fish cells / 420,374 usable, with 38,271 / 38,271 on island 0. These are **flag-enabled runtime-fill** counts, not a census of unmodified embedded scenario fish or a flag-off claim.

The exact bounded stopping result is unmet house predicates plus incomplete production/service supply within the declared budget. A unique irreducible engine blocker is **not established**. No third opening, extended budget, alternative-chain search or production repair is included.

## Reproduction and receipts

Use the configured dev build, real files via `ANNO1503_GAME_DIR`, and `ANNO_TEST_THREADS=4`. Commands are run from the repository root with the usual CMake/Ninja tool PATH. Memory admission stays enabled; one test job; timeout 1,800 seconds.

```sh
python3 -I tools/test/run_selected.py --build-dir build/dev \
  --select '^anno_sim_tests\.(town start measurement:|ship start: facing)' \
  --asset-policy required --jobs 1 --timeout 1800 \
  --log cache/town-start-1/default-off-02.log
ANNO_TOWN_START=1 ANNO_TOWN_START_OUT=<fresh-absolute-output-directory> \
python3 -I tools/test/run_selected.py --build-dir build/dev \
  --select '^anno_sim_tests\.town start measurement:' \
  --asset-policy required --skip-auto-contract --jobs 1 --timeout 1800 \
  --log cache/town-start-1/enabled-02.log
```

The second command reuses the required picture contract from the first call against the same build. The enabled CTest entry contains three complete world runs; it counts as **one executed test**, not three. Corrective test wall time: 279.72 seconds; initial test: 274.07 seconds. No timeout, memory refusal, invariant failure, world fault or sanitizer diagnostic was reported by these Debug gates. This is not an ASan execution claim.

Default-off log before the linkage correction, required contract then main selection, verbatim:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=2 executed=1 failed=0 empty=0 skipped=1 (required-asset=0 platform=0 opt-in=1 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

After the linkage-only correction, `default-off-linkage-01.log` again reports selected 2 / executed 1 / opt-in skipped 1 / failed 0 and VERDICT PASS. It reuses the unchanged picture binary/contract; the dev test target rebuilt without compiler warnings. The earlier full warning script applies to recipe source f36c22fa; this final compile is its narrow affected-source follow-up.

Corrective enabled log, verbatim:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Local warning checker ran once, exit 0: 1,006 project compilations and 2 rejected shadow controls; all four passes clean. Both historical compiler labels used actual Apple Clang 21.0.0, with the preserved Bash 3 mapfile/BSD xargs launchers and CMake-detected -Wno-c2y-extensions. No source suppression was added. clang-format 23.1.3 and git diff --check pass. The dev target compiled without compiler warnings; its linker emitted the existing duplicate-library warning.

Full pins reuse is explicitly authorized by the packet. Accepted setup tested code `e71f2dc0294d1c578e044bc0310972d83144af9b` has 1,691 passes / 18 opt-in skips / 0 failed, required pins contract 1/1. Its exact line is retained in REPORTS. No changed production, rule, fixture or pin dependency requires that 86-minute campaign to run again. This is **reused evidence**, not a new full pins execution.

Host: Apple M5 Max, 18 logical CPUs, 48 GiB RAM, macOS 27.0.1, Apple Clang 21.0.0, CMake 3.31.10, Ninja 1.13.2. No other gate/build/test was observed at corrective launch. Run 02 launch load average was not recorded; inherited run 01 load metadata was removed rather than mislabelled. Owner caffeinate was preserved.

## CI boundary and NOT_RUN

Checkpoint [CI 44255019025](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44255019025) is red: `cpp (linux-gcc-release)` fails compilation of freight.cpp with `freight_state.hpp:33:8: error: potential null pointer dereference [-Werror=null-dereference]` (three diagnostics). The Clang sanitizer build also fails before tests in unchanged test_support sources: libstdc++ 15 ciso646:49 emits a C++20 deprecation error under -Werror. GCC Debug also exposed a town-owned subobject-linkage error in Candidate::p (Origin from an unnamed namespace). The helper scope was corrected in c9132c39, with the narrow gate above; the first failure remains in history and hosted CI must verify the fix. These are build failures, not executed test failures or sanitizer findings. Public annotations and job metadata remain in cache. The freight sources and build/workflow files are unchanged from accepted setup. This establishes an unchanged path, **not** a compiler root cause or waiver. The orchestrator directed preservation of the red result, no freight repair/suppression/blind retry, and READY FOR REVIEW / CI BLOCKED closeout. A subsequent docs-only CI result cannot erase that code-checkpoint failure.

NOT_RUN: Windows game-file measurement, running original/History Edition comparison, new enabled ASan measurement, native GCC 13/Clang 18 local warnings, long-term equilibrium and alternative openings. No original tables, script text, asset bytes or pixels are published. Raw traces, metadata, logs and the private summary reader remain under `cache/town-start-1/`; [SHA256 index](sha256.json) identifies the frozen evidence. Independent acceptance remains with the orchestrator.
