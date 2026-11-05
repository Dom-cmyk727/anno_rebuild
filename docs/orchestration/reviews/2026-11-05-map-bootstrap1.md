# MAP-BOOTSTRAP-1 local handoff — 2026-11-05

Local candidate only. Building availability facts/query and the ship-start audit
are delivered under independent review. Authoritative player-build enforcement
remains **BLOCKED by the owner's explicit decision**, pending a separate
scenario-permission implementation contract. This is not a completion claim for
B3 or a complete self-supplying settlement.

## Pins and ownership

Accepted base: a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0.
Branch: task/MAP-BOOTSTRAP-1.
Checkout: /Users/dom/Dev/anno1503-worktrees/anno1503-worktrees/anno1503-MAP-BOOTSTRAP-1.
Frozen code/tests/gate source: `4bafd5d00e7f2c0f199f31339350f12d0e7fe191`, clean.
Production code is unchanged since `0ceea121`. The later change preserves historical
checkpoints through a test-only identity projection, not a gameplay alteration.
Canonical workflow source: main 20bd97fd68f2b5fbba582e49d71cadb855346e67.
No merge/rebase/push/publication/default House flip or original-file changes.
No MAP-SUPPLY-1, ECON-BOOK-1 or Windows implementation was imported or changed.

Commits: 15b4ef02 baseline; bec0de61/66223203 original evidence; 1fbd6e87 facts/query;
932f84f2 decisive tests/correction; 0ceea121 budget/prefix;
4bafd5d0 compatibility projection preserving accepted checkpoints/fixtures. Later commits are report
closeout only unless explicitly stated below.

## Result and required next contract

Original availability first honors a per-player exact-type permission override,
then a stage-mask bit, then reviewed research OR alternatives. EVER is bit 0,
not unconditional. Stage 31 has a special output-goods/FORSCHTYPE fallback;
original shipyard1021 has additional alternatives 35/36. Strict malformed fact
rejection is a documented rebuild adaptation. Primary addresses, module hashes,
parser/variant distinctions and separating examples are in re/build-availability.

The content adapter derives validated BuildingType::construction facts. The
pure build_eligibility query reads registry/player facts and an explicitly scoped
permission span; typed refusals distinguish invalid permission context and unknown type/player,
scenario denial, stage lock and research. There is no renderer dependency or
new engine building-id allowlist. The original loader declares original profile
exceptions, and alternate content uses the same interface.

BuildCmd, its atomic/live validation, create_building, scenario import, founding
and automatic transitions are unchanged. A locked BuildCmd still executes if
its other existing conditions pass: the baseline proves this openly. No stale
preview/apply enforcement, availability-refusal nonmutation, effective-type rule
or atomic availability-bypass mutation is claimed. A refused salt placement did
leave the entire save unchanged, a separate existing-placement assertion.

The owner answered: "Keep production wiring blocked until scenario permissions
have a separate implementation contract". That contract must establish ANLTRG
permission state/import/persistence, script/setter effects and initialization,
including associated stage/research changes, before authoritative enforcement. The query accepts an empty permission span as
an explicit no-overrides assumption; it cannot detect missing script context.
It must also specify requested/effective rewrite checks and refusal order. The
query alone does not authorize silently empty permissions on scripted scenarios.

## Compatibility

No new persisted field or draw: published stream 128 / PRNG timeline 5 stay unchanged.
Nondefault construction facts extend registry identity (BVA1 framing); default
synthetic identities and their old recordings/goldens stay unchanged. Original
registries intentionally change identity and refuse old saves/replays against
changed content. Old originals/recordings are retained at their pins. Only the
affected original direct-registry digest assertion changes, independently
recomputed as241 rows / 3948 bytes extending855fce5ed63c1090 to b62eba128fce9259.

Historical K3 checkpoints and all original scenario fixture rows remain unchanged.
A test-only helper clears construction facts in a registry clone, preserves every
other fact, and requires accepted full-registry identity `1bd52ba547c3878a`.
Checkpoint copies are hashed with that reference and never advanced or replayed.
A one-gold state change remains visible; a non-construction cost change fails the
legacy-identity guard. Live saves, recorder checkpoints and replay retain current
registry identity. This preserves the old behavioral oracle while the old save's
content identity remains intentionally incompatible with the new registry.

## Budget and demonstrated prefix

Unchanged cargo per player: 30000 gold, 50 t wood, 80 t tools in separate 50 / 30 t holds,
10 t food, mask 1. Founding spends 350 gold / 12 t wood / 5 t tools. Warehouse capacity 50 t per
ware leaves 25 t tools aboard on unloading. No income or unaccepted production is
assumed; peace/shared visibility/no alliances and optional island sharing remain.

The reproducible narrow chain bill is 4110 gold / 73 t wood / 40 t tools. Add 16 pioneer
houses (capacity lower bound for 125 pioneers) and one base 520 upgrade:
4110 gold / 125 t wood / 41 t tools, plus 5 gold per dirt-road tile. This is not a minimum
layout or sufficiency proof. Initialwood needs 75 t replenishment, but the forester
costs no wood; tools/food remain finite. Later 2C toolmaking cannot bypass its
unlock or missing brick/extraction chains.

Fresh public-command prefix: sail -> found -> unload -> forester -> 25 dirt roads
->small farm. Stop: 29125 gold, 34 t wood, 46 t tools ashore + 25 t aboard, 10 t food. Small farm
raw/output0,0 are creation observations, not dynamic inertness proof. Its missing
worker/animal bodies are a code-derived MAP-SUPPLY-1 dependency at the accepted
base. Salt query is StageLocked; an owned unobstructed attempted salt placement
returns Unsupported with unchanged save. No accepted-world teleport, grant,
stage injection, map installation or island rebalance occurred.

Prefix test passed 17238 assertions in 192.30 s. In-memory replay/save-reload matched.
The exported encoded log was independently replayed by public sim_replay in a
fresh process: 17181 entries, 10/10 checkpoints, final tick 85805, every checkpoint
matched. Ledger tool/independent fixture and full per-building source/material/
availability/status table are in sim/ship-start-bootstrap.

Next chain recommendation: cloth 807 -> 1001 after the matching grazing work is
accepted. Hunter/leather and legal salt extraction remain separate missing
links. Natural source differences may require later manual inter-island resupply.
This handoff does not release or start that next bundle.

## Gates and preserved failures

| Evidence | Source | Selected | Executed passes | Failed | Empty | Skipped | Incomplete | Verdict |
|---|---|---:|---:|---:|---:|---:|---:|---|
| Final broader Debug, debug-final-02 | 4bafd5d0 | 1290 | 1288 | 0 | 0 | 2 opt-in | 0 | PASS |
| Final ASan/UBSan correction, asan-final-02 | 4bafd5d0 | 12 | 12 | 0 | 0 | 0 | 0 | PASS |
| Retained ASan model/callers, asan-final-01 | 0ceea121 | 108 | 108 | 0 | 0 | 0 | 0 | PASS |
| Compatibility canary, compatibility-01 | recorded working diff before 4bafd5d0 | 2 | 2 | 0 | 0 | 0 | 0 | PASS |
| Focused model/budget/registry, focused-03 | recorded working diff before 932f84f2 | 9 | 9 | 0 | 0 | 0 | 0 | PASS |
| Initial prefix batch, prefix-02 | recorded working diff before 0ceea121 | 9 | 8 | 1 | 0 | 0 | 0 | FAIL; prefix itself PASS |
| First broad Debug, debug-final-01 | 0ceea121 | 1289 | 1283 | 1 | 0 | 2 opt-in | 3 | FAIL/status9 |

The final Debug opt-in skips are navigation scale measurements and transport
search statistics. There are no required-asset, platform, GPU or unknown skips
in that selection. Sanitizer lines are zero in both ASan runs. The final Debug
run took 377.44 seconds; the correction sanitizer run took 74.69 seconds.
Counts are CTest entries; the two ASan rows overlap and must not be summed as
unique coverage. Required contracts run separately and do not inflate totals.

Required runner/reference/public-budget contracts executed separately from main
selections; their counts do not inflate selection totals. Shared workflow suite:
21 methods passed. Gate preflights include 24 verdict tests. Exact commands,
source/build identities and inventories are in each raw log. Source/fixtures/
executables/CMakeCache SHA256 manifests: `cache/bootstrap/identities.json`
(first freeze) and `cache/bootstrap/identities-final.json` (final binaries/fixtures).

Preserved attempts: baseline-01 build failure; baseline-02 stock-shape setup failure;
model-build-01/02 compilation mistakes; model-01 test-payload ownership compilation
failure; prefix-01 missing test include compilation failure; prefix-02 whole gate
FAIL: 9 selected, 8 passes, 1 failed (synthetic fixture Unsupported), while its prefix
itself passed. Corrected focused-03 PASS9/9. The first broad run failed on six
old K3 identity pins and has three incomplete corpus workers: sandbox socket
EPERM prevented coordinator access. Automatic approval review initially rejected
stopping them under the workspace running-job rule; the owner then explicitly
authorized stopping only these three owned blocked tests and rerunning. The
final run has host permission, visible admissions and no incomplete outcomes. No failure was overwritten or relabelled.
Five retained mutants reach target assertion failures (ignore requirement, wrong
player, ordinal test, wrong research fallback, wrong original shipyard extra),
with passing legal controls; recipe and raw logs cache/bootstrap/mutations.
Budget wrong-cost fixture control also fails specifically at type 801 cost.

Reuse: fresh prefix data and in-memory replay are retained across test-only
fixture corrections/include movement; actual runtime, original inputs, manifest
and prefix body are unchanged. The broader Debug gate excludes heavy work and
three opening/forestry canaries; the new prefix and fresh encoded replay cover
the changed registry adapter integration. The final gate executes all 50
scenario five-minute runs, the ten extended runs, imported masks and the new
50-file legacy-identity comparison. Prior forestry 448 raw = 1.75 t wood delivery is retained
at its original accepted pin only, not claimed as a new measurement. No rendering,
production kernel, sailing/placement algorithm or src/formats container/sprite
reader changed; registry INFRAMIN record decoding did change.
unaffected rendering/original-runtime campaigns are not repeated. The final
sim corpus was run to validate the affected registry/import identity, not to
calculate construction prices.

NOT_RUN: Windows/MSVC, Linux, original runtime, viewer/GPU/live integration,
full-suite sanitizers, rendering/original-runtime corpora, scale campaigns and
production availability integration. B3 checks depending on enforcement remain BLOCKED/NOT_IMPLEMENTED.

## Independent review

/root/review_original approved B0 at 15b4ef02 and B1 at 66223203 with module-hash
correction subsequently recorded. Independently inspected original bytes/data,
not just notes. /root/review_model reviewed B2 at 1fbd6e87 and requested decisive test
corrections, then approved the focused correction diff committed in932f84f2,
including all five mutant assertions, nonzero FORSCHTYPE and original adapter
facts. Integration/prefix approval is separate. /root/review_budget approved B4 at 0ceea121 after520 attribution/replay-scope
corrections, independently reconciled raw costs and exact prefix budget, and
ran the cheap wrong-cost control. No reviewer authored implementation.

Fresh read-only integration reviewer `/root/review_integration` **APPROVED**
`4bafd5d0` for the local facts/query/bootstrap scope. It independently checked
primary bytes/data, caller/import boundaries, identities, legacy projection,
negative controls, exact budgets and terminal raw gates. No remaining blocker
within that scope. B3 remains owner-blocked, and complete settlement supply is
not demonstrated. The reviewer requested only the current acceptance table and
final documentation closeout, now recorded here; no further broad run is needed
for these prose changes.
