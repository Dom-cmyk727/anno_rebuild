# MAP-INTEGRATE-1 — bounded combined local candidate

2026-11-05. **I0–I6 COMPLETE LOCAL CANDIDATE for the bounded headless integration.**
Owned branch `task/MAP-INTEGRATE-1` in the existing MAP-BOOTSTRAP-1 checkout.
This record covers stream131/SIMINT1 only. No main, moving MAP, or active NEEDS
branch was merged, and nothing was pushed, rebased or changed in another lane.

## Inputs, source and merge decisions

| Role | Pin |
|---|---|
| Dispatch and full I0–I6 assignment, read from Git objects | `cafa74d51ee6b03d514cf197a4e5b1c80ab2219e` |
| Clean accepted UNIT1 starting head | `50aa32e38706ad3c2e4302e8fcaada9f555ba034` |
| Sole accepted supply merge input | `67158af4d29d87fed8686dc6542ca2a9f4b1f75c` |
| Actual common ancestor | `a50aec3a6b0f6bbeb90fee5525c757f2b58b4da0` |
| I0 contract, first merge parent | `c41ea1fe8fe0a47d0ac250153b9b35a8e5c4e97c` |
| One authorized local merge, second parent supply67158af4 | `a81d6c9540ca75933eaa1515d6edbd209e13195d` |
| Broad Debug source, including real canaries | `aa06d6dbe0df8e48f0adc61e8b0716916ae76d48` |
| Corrected tests/style and final affected sanitizer source | `cafc1a7c047536f83fe537572a571662c41910f3` |

Original UNIT/SCRIPT branches remain preserved. Local TASKS.md is byte-identical
to UNIT1; the updated assignment was fetched/read rather than overwritten.
Input inventory and primary contract are in [gameplay-integration.md](../../sim/gameplay-integration.md)
and ignored `cache/integrate1/I0/`:151 UNIT paths,53 supply paths,17 overlaps.

Semantic resolutions retain both sides: strict original actors before INIT;
raw slot holes/MAIN capacity/allocator hint separate from serial IDs; nested
parent/holds/HP/fine navigation; SmallFarm crop/animal0x2e validation and configured
raw-slot credit; one counter release and policy-specific claims; Pending script
construction barrier and Ready restore; original paid permission checks; explicit
component projections; data/config roles and default `house_updates=false`.
Five supply production-only implementations remain exact accepted input bodies.
CMake retains the union104 sim sources, rather than UNIT99 or supply90.
No new script handler, actor class, movement, balancing or default policy change.

## Compatibility and independent oracles

| Boundary | Current contract | Historical treatment |
|---|---|---|
| Native state |131, schema `SIMINT1\0` = `0x0031544e494d4953` |128, supply129, SCRIPT1-schema129, UNIT130 refused at intended identity with valid envelopes/checksums |
| Save envelope / command wire |1 /1 |unchanged layouts; no automatic migration |
| Replay envelope |4, explicit native version/schema |frames2/3 refused before body execution |
| Script policy |2 |program/namespace/limits mismatch reasons retained independently |
| PRNG timeline |5 |no new site/phase/classification; supply activates existing sites, state identity changes |

Public replay, per-entry diagnostic hashes and tree lookup validate identity even
on direct checkpoint-free ReplayLog objects. The initial native save must also
match current envelope and inner header. Suffix replay remains legal: supplied
live state need not equal the log's initial state. Config/Rules/registry/program
identity failures remain separately observable with otherwise current inputs.

136 retained files match exact original Git blobs under
`fixtures/synthetic/sim/integrate1/legacy/manifest.json`. Four exact native slices
and their original log versions/checksums are in `predecessors.json`:128/frame2,
supply129/frame2, SCRIPT1-schema129/frame2 and UNIT130/frame3. Both129 histories
remain separately named. Historical SCRIPT references explicitly use retained
published128 dependencies; old inputs were neither overwritten nor relabelled.

`tools/sim_stream.py` independently transcribes current header/player/script
scope. `tools/integration_stream_ref.py` independently encodes populated
header/player/figures/script fields and hierarchical digests: sparse ship37,
crew2, donkey7, animal8, distinct IDs/hint/cursor, nested holds/navigation and a
validated Ready program with nontrivial signals/permissions. It is a structural
oracle, not a complete playable world. Its public CLI has four positive/negative
contracts. The complete legal world and continuation proof are I3 below.

Current fixtures contain11 new frame4 logs,41 golden rows and50 creation hashes.
The23 generation children each executed1/1; generation itself is not acceptance.
Scheduler/header/populated stream bytes have independent reference comparisons;
other checkpoint/session values are C++ rebaselines with semantic controls.
Physical corpus composition uses accepted supply totals102/222/42 plus UNIT's
one restored researched producer, yielding103/223/43; no pre-supply absolute
baseline was substituted. Full physical corpus later executed successfully.


| Frozen input | SHA256 |
|---|---|
| `goldens.txt` | `6a0da7bb9e48873b28a2d19200c8a12040cfd3b4e3c5d82c181812631d3c05c1` |
| `scenario_corpus.txt` | `9b5ba8756e533935c96ad7d86b40fc6e43d82f07f9a666eebb3c8a0db8320eb7` |
| `populated_fields.txt` | `2030dda2dc0fffde6d40ab80e5bcb9d8241ebe3fef2f76e0bb8e7d8e009651cc` |
| `predecessors.json` | `6738a48b49900996dc64e65058400c992ed629bc547ac8d4b612cabb34301e9d` |
| `manifest.json` | `c927fa62f5d9e81ac54a23ccb81e8d1594db420eec74e8187480b342a7e6d10c` |


| Newly recorded current replay | Bytes | SHA256 |
|---|---:|---|
| `cargo_load.log` |80789 | `3e628cf7b352a86b46c20c81bde955b9d8d2c76e9925d768f1d105c0a34a8fde` |
| `cargo_load_wrong.log` |80789 | `32928debb46504d0c9be590dfb5b6d816a165bd0dc5b60bee6099ab0c65cca70` |
| `cargo_unload.log` |80664 | `65d69267ddd79d314602b3f2809062a23d1088351a47bf5983a61b4fe5651b66` |
| `cargo_unload_wrong.log` |80664 | `9a12754250b6afeec81ab29b5416da243c5ab2bfd93c4947ebc3fdb19553bca1` |
| `g2.log` |121504 | `b30d3b067d8d16f201c7f85c48323a1aad889d4ae04989daa06f3521cfd18921` |
| `g3.log` |143831 | `c8f0f694b543b84d93c6b4d6ec2c608e9e872b270b962514087a41b8fd1d59bc` |
| `g4.log` |191763 | `670179baba66f171174231ce1e5dfa5e2afbcbc7c50a6bc266dbf2a0bfd12304` |
| `g5.log` |326811 | `c37ee3a4bd95aa9d6b97a4d76f6eed8aab4c2f93f9b818b0c5929cc4178f4231` |
| `g6.log` |112757 | `b740ff944317666c10e3d840f22dc37f1c1f43bc6cd40a858480a5daee9bd165` |
| `m2_found.log` |293669 | `31b6dbd6eaecbe19ac3253a376e061f09d505be2d956ef80a17f3fc555aa9ef5` |
| `m2_found_wrong.log` |293669 | `79f3c8556138624fe5db28b1ce7938f79c6f6e6fb20ef49a942628144a6cb1c3` |

## Acceptance table and package review ledger

| Package | Positive proof and distinguishing counterexample | Evidence / independent disposition |
|---|---|---|
| I0 input contract | Exact pins, semantic overlap/identity inventory; original Tutorial809 permission prerequisite predicted from raw flags, then checked publicly before long runs | `/root/integrate1_contract_review` APPROVED `c41ea1fe` |
| I1 semantic merge | Both strict import/startup and crop/animal harvest paths retained; no silent fallback, ID/slot conflation or Farm-only validator | `/root/integrate1_merge_review` APPROVED `a81d6c95` |
| I2 identity | Current legal native/direct/encoded replay; actual four predecessors, wrong schema and valid-checksum old initial save refused; own-reason content/Rules controls | final17/17 Debug at `392d3989`; `/root/integrate1_identity_review` APPROVED after oracle-signal/actual-old-log findings fixed |
| I3 joint fixture | Public INIT once, permission allow/deny, owner/terrain/cost checks, sparse nested actors, real dispatch/harvest, both claim rules, true uninterrupted/native continuation, exact events/PRNG/ledgers | initial Debug22/22 and ASAN22/22; corrected Debug13/13 and ASAN13/13, overlapping selections; `/root/integrate1_joint_review` APPROVED `12424236` |
| I4 original script and supply | Whole unchanged endless01 voyage/wave/founding/unload/build, denied-copy and changed-command/program controls; supply crop/alcohol/food/household/recovery with live no-producer control and separate normal-loader refusal | all decisive Debug entries PASS at `aa06d6db`; `/root/integrate1_realdata_review` APPROVED through runtime-equivalent `cafc1a7c` |
| I5 gates | Frozen source, mandatory exact contracts, broad Debug failures retained, affected corrected checks, independent refs/format/lint, admission enabled | `/root/integrate1_gate_review` APPROVED `cafc1a7c`, including exact716 terminal/inventory reconciliation and reviewed scoped Debug reuse |
| I6 final interactions | Fresh independent source/artifact/primary-fact audit; final terminal gates and this handoff must reconcile | `/root/integrate1_final_review` APPROVED runtime `cafc1a7c` / documentation `0e10a303754e58f421c75c65ce91ca8280faf1e3`; no actionable finding |

I3 primary resource review corrected authored703 (drought/unused) to original
GRAS2 -> growing1; potatoes709 -> growing708. Original Anlagen SHA256
`b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75`.
Both lines now require positive consumption/production and loaded equality.
The selector was corrected to plural `small farm animals:` to execute all six
animal cases.120 named animal draws occur over60000ticks; no crop-dispatch draw
is invented. Repeated public end succeeds once, then returns false without
counter/claim damage. Four intended mutants fail1/1 each at slot-hint/raw1/
Pending-permission/repeated-end assertions; clean source hashes were restored
and rebuilt. Initial probe/build/test failures remain preserved in ignored cache.

The cheap original Tutorial prerequisite was **Allowed**: complete supported
empty182-byte script retained and Ready, player0 flags0x3f include INFRA_EVERbit0,
no809 override. Paid public Demolish/Build809 at261,344 costs250gold/512raw tools/
1024raw wood. Recovery needs no authored permission override. Some older supply
notes saying scripts/INFRAMIN are ignored describe their historical input only.

## Real original-script and supply results

Unchanged endless01 uses strict normal import, five original actors, original
positions/resources and complete2488-byte program. Planning copies are separate.
The first public voyage to224,672 triggers area branch0/source1068 and wave317
at tick48000; a second voyage to350,405, founding, unloading and construction
place HANDWERK1001 at318,401/island2/city0,
ID8589934594. Source scenario SHA256
`b289be7fe906264b1d0fe0b2be75c0e68813ed6691128c84cfebb6e4fd4cc8c0`;
program SHA256 `cac0aeb9c14dfd5c0f9b18c664e5282dff57096e2938f93c67391e3c3f8a47d0`.
No source-world relocation, loadout grant, stripped kernel or asset edit.

| Original ship ledger | Value |
|---|---|
| Total cargo |400t initially;17t founding;383t remaining;200t transferred,183t aboard;7t build,193t city inventory |
| Gold |10,000,000 ->9,999,650 ->9,999,350 |
| Final tools |95t aboard +47t city =142t |
| Final wood |38t aboard +46t city =84t |
| Final food |50t aboard +50t city =100t |
| Final brick |0 aboard +50t city =50t |

Debug fresh wrapper PASS169.84s: producerPID14780 and replay workerPID15457,
3988/45 assertions, distinct processes.411 normal entries/198checkpoints/one
Startup; legal alternate changes only entry407 and diverges at following408.
Changed program has valid native digest and refuses for program identity.
Final state hash11670688377534699116. Actual manifest:
`cache/unit-import1/replays/fresh-q3p2ygeo/manifest.json`, SHA256
`b7ca1a6c88cd2cd8e022da102316bc8574d3ea3947b859358a4b92a2381a9e06`.

| Debug fresh artifact | Bytes | SHA256 |
|---|---|---|
| normal.log |10170427 |0e6398b4a1eee0b2e7847858f48a18ce25151ba260c51ec66b3223e6bc6a0de5 |
| wrong-command.log |10170427 |118d7c188131aa679e1a38993d765c4b0fadde766e941f840060ffb299be748c |
| wrong-program.log |10170422 |feeeef96609779e9492ab2e0525def72deda7577aa481a078030f4b776997339 |
| expected.txt |54 |93aed0f4379fe13e2723dbaf339b64153c6ce3fc3d9041dfb729d19d66eb9faf |

Supply at150000ticks:24crop cuts,6144raw consumed,624alcohol made,312farm output
and312Inn sold, zero alcohol in flight; four initially empty tavern households
receive deliveries. Grazing food1183made,270out,913loaded,314inflight; explicit
conservation/purchase assertions reconcile stocks. Dedicated CLOSE2 comparison
starts both stores256 and scopes exactly four target-town farms: with farms
1183made/756bought/end99, control237bought/end19 and zero target-farm food.
Recovery after paid809 rebuild sells936 vs312control, tavern9726 vs1957, with
nine recipient households. Native cuts at crop800/grazing1450/raw14100/cart8600/
resident8050/Inn95700 continue against uninterrupted worlds. Component projection
is declared before loading; separate normal Single24 actor refusal and unsupported
whole-script compile refusal run, rather than silently dropping either.

Current exported supply replay also ran via three fresh Debug CLI processes at
consumer `cafc1a7c`, producer `aa06d6db`. Normal8609entries/7of7checkpoints/final
430000 reproduced, exit0. Changed demolition diverges at entry3803/tick190000
and changed rebuild at6206/tick310000, each exit1 at its intended checkpoint.
Exact commands/input/executable/output SHA256s are in
`cache/integrate1/I5-supply-cli/manifest.json`; no new producer campaign was run.

## Gate attribution and preserved failures

All listed acceptance gates use main's canonical runner with explicit owned
source/build, original assets required, admission enabled and serialized own
expensive work. Host48GiB/18logical CPUs/~1.5TiB free; concurrent first-Mac/main
jobs were observed and left alone. No own display-awake process was created. Build tools: CMake3.31.10, Apple
Clang21.0.0; formatting uses clang-format23.1.3.
Raw logs, inventories, executable identities and source/workflow records are
under `cache/integrate1/`. Required preflight contracts are separate, never added
to the selected headline. A gate's passes are CTest entries, not assertion counts.

| Gate / source | Selected | Executed passes | Failed | Empty / skipped / incomplete / SAN lines | Outcome |
|---|---:|---:|---:|---|---|
| I2-debug-01 `84e75a21` |16 |16 |0 |0/0/0/0 |PASS |
| I2-debug-02 `8f63c094` |17 |17 |0 |0/0/0/0 |PASS |
| I2-debug-03 `392d3989` |17 |17 |0 |0/0/0/0 |PASS |
| I3-debug-01 `16c0dece` |22 |22 |0 |0/0/0/0 |PASS |
| I3-asan-01 `16c0dece` |22 |22 |0 |0/0/0/0 |PASS |
| I3-debug-02 `12424236` |13 |13 |0 |0/0/0/0 |PASS |
| I3-asan-02 `12424236` |13 |13 |0 |0/0/0/0 |PASS |
| I5-debug-01 `aa06d6db` |1517 |1513 |4 |0/0/0/0 |FAIL preserved |
| I5-debug-corrected-01 `cafc1a7c` |55 |55 |0 |0/0/0/0 |PASS |
| I5-asan-01 `cafc1a7c` |716 |716 |0 |0/0/0/0 |PASS |

Broad Debug workflow `cafa74d5`; corrected Debug workflow `f5c151d4`; affected
ASAN workflow `6ce354cb`. Exact full pins/commands are in first provenance lines.
Broad log SHA256 `597e892ff8c48a36e3038f450ad53599bcd0da7d865255c3577198cea2b0f667`;
corrected log SHA256 `fe6545d01c64ea760f60c8ece893789ebc1231f2de2a19b260fdda295d2b339d`.
The broad failure is never relabelled PASS or combined into a fictitious clean
1517 rerun. The unaffected1513 successful entries retain reviewed evidence;
55 corrected/focused entries rerun all four failures and affected identity cases.


| Tested executable | SHA256 |
|---|---|
| Broad Debug sim, aa06d6db | `5929f6b01f98e851ec7927834cfce78113330d3062a004310c15e9260d4e8daf` |
| Corrected Debug sim, cafc1a7c | `cf26326c9b28cd5b94062cdb2082b990e351241077fdae813cb90459b64a32d8` |
| Debug formats | `62f8af39e13708fd295f57568c9988a852b47b3e5696494888ef3bc24b479d83` |
| ASAN anno_formats_tests | `94597fc68afd0a78161626300bc06e280e3acafec4686fbc596a7303ca5959b6` |
| ASAN anno_sim_tests | `a5ec91c050981ee87dd037cb82f6328b5262e856a91424485118dd36391f58d7` |
| Fresh supply CLI, cafc1a7c | `a1b3f2c11233c5b3560444625f0e79df032cb7c79034dee736048c1ac2a10167` |

The four broad failures were two handwritten old schema literals, stale absolute
research counts and a published128 test reading the now-supply129 root. Fixes
change test expectations, preserve activated-target/total assertions and bind
causal+1 counts to exact accepted supply data. The128 test checks11 independently
obtained archived Git SHA256s, old outer-frame refusal and actual native refusal.
Subsequent runtime file differences are formatting/comments with exact Clang
tokens/directives preserved. The harvest title split has the same C++ phase6
string value; its raw token list differs, which the proof explicitly records.
Reviewer approved reuse of unchanged runtime/corpus/canary checks.

Formatting first failed on six files and remains retained. Seven-file fix then
40/40 changed C++ files pass clang-format23.1.3, exact per-file SHA/command/version/
exit receipt `I5-format-receipt.json`; lint passes. Empty success format log alone
is not claimed as proof. Token/directive equivalence is in `I5-format/equivalence.json`.
No runtime correction was smuggled through formatting.

Affected ASAN plan selects changed import/script/native/replay/worker/animal/
production/transport/house/population paths, all nine integration tests, actual
formats and all relevant reference/CLI contracts. Jobs1, default3600s; original
fresh wrapper has explicit measured1500s (prior UNIT581.97s). Full physical corpus
is current Debug, while50creation/50raw-unit/parser checks are also selected under
ASAN. This is affected coverage, not a full-suite sanitizer claim.

Selection correction: broad Debug accidentally also reran the unchanged isolated
sailed founding opening, PASS662.93s; initial exclusion had named only forester
opening. The exact sailed prefix is excluded from ASAN. Prior UNIT130 isolated
ASAN opening is reused for unchanged generated-map behavior, with declared3600s;
its actual accepted raw result is2247.82s. Bootstrap prefix current Debug192.94s
is recorded without a new ASAN claim: all its Advances precede the farm build, so
it uniquely exercises no changed supply worker path. Prior historical SCRIPT2
592 selection remains FAIL (591 passes plus1800s timeout); no timeout became pass.

Final affected ASAN actual selection is716/716 PASS,4430.90s, with26/26
required contracts separately PASS. Log SHA256
`4563c25b8be52b13592f33656897f6b63d096c707539de083ecf2e741504b73a`;
inventory SHA256 `4a85b131f40a1464d736bc21e161932d8f63c3a23206c74d8ab2fe1a77afefd0`.
The source remained clean at `cafc1a7c` throughout. Canonical workflow6ce files
remain byte-identical after the orchestrator's later main update; the five-file
SHA receipt is `I6-workflow-equivalence.json`.

ASAN original fresh wrapper PASS587.38s, producerPID62447/workerPID63740,
3988/45 assertions, both exits0. Manifest
`cache/unit-import1/replays/fresh-453fov6b/manifest.json` SHA256
`a3fa3e12d3e531928f7e63a7892895c1ea08f59e051f19fbc43fedcb7f10df2f`. All four artifact sizes/SHA256s exactly equal the
Debug table above: unchanged original script, actors and ledgers reproduce across
these two Mac build configurations. That is not a cross-platform acceptance run.
ASAN K4/CLOSE2/recovery/native-cuts/recorded-replay pass55.54/37.63/67.09/34.79/
79.47s respectively, with the same scoped causal assertions.

## Future consumers, limits and stopping boundary

These are **two original-data proofs plus an authored joint mechanics fixture**,
not an unchanged ship-start balanced town. OQ491, ship-founded alcohol feasibility,
new cloth/hunting and unsupported actors/scripts remain separate work. No runtime
capture proves differential fidelity, actual audio playback or rendering here.
Windows/MSVC, Linux/GCC/Clang-Werror, cross-platform one-hour M3, multiplayer,
full-suite sanitizer and late-game/performance acceptance are NOT_RUN.

Windows must keep its existing pin unless its owner releases a change. A future
consumer should use this exact local candidate with explicit current identity,
retained archived refusal controls and original-data resource/config prerequisites;
Mac development-host compile is not Windows/Linux acceptance.

First Mac NEEDS reconciliation belongs to the orchestrator: start from accepted
supply67158af4 and the completed accepted NEEDS2 range only, inspect household/
production/shared serialization/test conflicts, retain sparse actors, startup/
permissions/worker claims and configured raw slots. Do not incorporate active N3.
A newly composed identity must update native/replay headers, independent oracles,
current fixtures and refusal controls while retaining exact131 artifacts. Numeric
stream agreement alone grants no compatibility. Reuse unaffected verified outcomes
with exact provenance; run affected joint/needs/native/replay cases and required
contracts. This executor did not merge or retest that lane. Canonical main consolidation was subsequently published by the orchestrator at
`944cec89c1670df2a6090909eed094311de9a3dc`, tested code `7b57ebd9`, with
132/SIMINT1/timeline5/frame4/wire1. Its TASKS canonical-consolidation section was
read without changing this checkout. It includes source through `cafc1a7c` and
bounded accepted N2 `8ff6d1ca`, excluding N3. Its separate
[consolidation review](https://github.com/Dom-cmyk727/anno_rebuild/blob/944cec89c1670df2a6090909eed094311de9a3dc/docs/orchestration/reviews/2026-11-05-consolidation.md) owns those results; this131
record does not certify132 or change Windows pin/release authority. Remaining
delta from the included `cafc1a7c` pin is documentation/evidence only. The
orchestrator must preserve canonical132 code, fixtures and identity when taking
that delta; no runtime merge or repeated moving-main test campaign is requested.

Current reference/fixture/helper reporter code is also pinned by `cafc1a7c`;
canonical verdict/selection workflow pins are separately listed above. Historical
UNIT1 corrected publisher `f437ac732e8855ccc7ebd6e0fc65974ba8218770` and its
source `1988471a396185fc8d7652ffb449b902079091fc` remain historical, not current
integration executions.

Final code/test head is `cafc1a7c`; later commits change documentation only.
Reviewed documentation head is `0e10a303754e58f421c75c65ce91ca8280faf1e3`;
the following documentation-only commit records final I6 approval. Both I5 and
I6 independently reconciled716 unique terminal passes/exact inventory,26 separate
required preflights and original fresh-process artifact equality. I6 additionally
checked all nine documentation files, both input ranges, all nine integration/
16 UNIT/six animal/two supply-chain/seven resilience entries, three full Debug
corpus outcomes and workflow Git-blob equivalence. No actionable finding remains.
Required bounded packages I0–I6 are satisfied; no unresolved contract dependency
remains within this packet. The NOT_RUN and gameplay limits above remain open.
Stop after reviewed local I0–I6 handoff; no push or main integration is released
for this executor. The later canonical publication was performed separately by
the orchestrator under the owner's authorization.
