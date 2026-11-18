# WILDLIFE-PREFIX-1 — frozen review evidence

READY FOR REVIEW. Source `1e061493cdfa479b9725a16339f0497aba27d880`, tree `41141a030d28af1f7cbd9dca3a67c83048b69e2c`.
Publication adds this evidence and the REPORTS entry only; exact pushed head/tree and CI are supplied in the direct handoff.
Independent review remains pending with the Mac orchestrator. No executor reviewer agents.

## Acceptance and scope

The [contract](../../sim/wildlife-prefix.md) was written before implementation. D = addressed original read, P = parsed original data, C = C++ proof, A = explicit API adaptation. Local child-header completion never means complete scene/World loading.

| Requirement | Positive / negative evidence | Disposition |
|---|---|---|
| Checked raw fields | C++ high id 0x3123, signed -1/-128 and overflow checks; legacy interpretation unchanged | locally exercised; review pending |
| Explicit metadata | Present UNUSED admits goods 0; missing/unknown/unterminated/repeated WARE or short HEAD refuse | locally exercised; review pending |
| Runtime PE tables | Authored differing climate/remap rows; wrong image, truncated/out-of-section rows and physical row 7 refuse | locally exercised; review pending |
| Source chronology | R-H-R, duplicate records, unsorted island slots, repeated/empty/absent HIRSCH; explicit stamps 499/500/65535 and modes 0/1 | locally exercised; review pending |
| Cursor assignment | Draws 4,7,2,31 produce variants 4/2 and jitter base+7/base+31; one applied omission mutant fails cursor/timestamp while legal control runs | discriminating proof; review pending |
| Ownership/error semantics | 20 resource-vector session/wrapper equivalence, indexed facts, borrowed input lifetime, poisoned retry/finish and typed diagnostic-only partial | locally exercised; review pending |
| Header mask effect | Mean 31 versus 32, clipped/repeated OR, packed-field preservation and input immutability; empty header executes | locally exercised; review pending |
| Real file | Unfiltered island stops at header 35 record 22; first variant/height/draw and completed empty-header mask effects asserted | diagnostic partial only; review pending |

## Intake identities

Approved-local RESOURCE-FIX base `0627532f330dc898b80941ea31d9dafe87980aa4`. Exact-head CI remains a separate acceptance condition. Decoder/helper are locally reviewed dependencies, not retroactively accepted CI: their runs 44344197686/44354548420 had terminal no-runner failures. Only REPORTS append and the two CMake source-list registrations conflicted; retained both sides. No semantic source conflict, main merge, rebase or other intake.

| Original pin | Local pin |
|---|---|
| `f1e245d2769306451d4c2079aadf6a64be93defd` | `d2650639c94dd543e1e8e0295fc3d2113bd2ba80` |
| `8b22eb49e087b487f380fe5243e4c5bf05c9af99` | `8557b9cec69570185c8888df369c0143cd63e451` |
| `159495df976ea028041a70e4bdb0959f1954a9df` | `9d6bd710172ab72430946636ef109e537b755d3d` |
| `07da5001bab75ae20c22cb436e9fb7750e0e611f` | `7275046b5d1a898d55addebfa3d69b813a161687` |
| `7f58b9f25d09b0c12a61de2a56e67da45abe41d4` | `9b17586401f2c3db6d08f5a0cc21a2bdc13e0788` |
| `5705678c8e90febb9cbc7357e8c4670a516ee78e` | `3537f0943bc91f590991ada9f13761af1f0ca911` |
| `73edb24426e2477f6700755cced9c2ba18cadc4c` | `54ffff1fcde387640b515bf43ed618fd916bcab3` |
| `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `7366c8fa8e912d55f1614a5b99a92fbd575fec3a` |

At post-intake `7366c8fa`, all 11 executable/helper/vector inputs equal their upstream pin byte for byte:

| Path | Upstream | Intake blob | SHA256 |
|---|---|---|---|
| `src/formats/include/anno/formats/insel.hpp` | `f1e245d2769306451d4c2079aadf6a64be93defd` | `078118c7ae377d08862040469e8098cb0b5eff9e` | `d8280ee2fd94e93fef155da443d56f67bd21c468fa940d933ec63275c1da74cb` |
| `src/formats/src/insel.cpp` | `f1e245d2769306451d4c2079aadf6a64be93defd` | `e76477b959f4af694aa11bee63ee283e58431038` | `7921b34fbc0d80608ce3b05701f0eae31b46de99fcfea34afacf3b3db2b2b648` |
| `src/formats/tests/hirsch_test.cpp` | `f1e245d2769306451d4c2079aadf6a64be93defd` | `bc0bc1a2943fc83df3457e61b66bf0ae840168a7` | `954a6919b30bb1a5997925064890c0322ab0964deac5d80312abd01e7d6cfb42` |
| `tools/wildlife_load_ref.py` | `f1e245d2769306451d4c2079aadf6a64be93defd` | `b83d6675690c7f73ba2f4f9fc3380bc675c8e807` | `573ebc64bbec97a48a68589fe1b52aad0184d16d9453e18763706bf0882f3171` |
| `tools/wildlife_load_ref_contract_test.py` | `f1e245d2769306451d4c2079aadf6a64be93defd` | `3c2ba3f4b6df1553e15f94856a23a44455eefdfb` | `c6cf0481e1a7aa76f6e1354d6910ff07f1864323d17430f370a8e45ce7d9c634` |
| `src/sim/include/anno/sim/wildlife.hpp` | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `dddb60d9c3f238fab2203ccaa5dd479a62372374` | `349929540743eb6c8cb8ef731833a63bedf8a1800fab71ab2033e966656f2f01` |
| `src/sim/src/wildlife.cpp` | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `9143814636cd212a3a77a1f88cb1d2e7e1b4343e` | `806e1f288ad383c51524d3eb3958eab4890c27f11dd3deb81b169d1be2376dbe` |
| `src/sim/tests/wildlife_test.cpp` | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `4ae7287ca9ef5094315d5144522b5413e4ee2c36` | `18a050c45c53011466e3218d27a2c782d8500861513662e53d61e40f056332f8` |
| `tools/wildlife_ref.py` | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `3c47a1e03b66d46e002cd0d6f047b8201b9b71f7` | `155cd2374ca06a796d4473daf326864a66d6485bdba688465922859ac23c72fb` |
| `tools/wildlife_ref_contract_test.py` | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `84ee82091f1032c66eedaa4054fdc06423575cc0` | `acbcf59b90d28c845594dde2f0b7d0265e5125ddbadc05b0d2f756afa0677109` |
| `fixtures/synthetic/sim/wildlife_vectors.txt` | `533ee3fc96a560f1c3189f3a4d32ce113abff65a` | `da32b760dc0128fc41e1386351c769a9f6746964` | `1f6a8f8104ef2c200c1d40b6e288323ca7290da4d4f6f65cd4e28908d906ea77` |

## Original boundary and callback closure

Tutorial/01Discover01: island ordinal 0, slot 11, GELAENDE header 35 record 22, type 1558, STEIN kind 323, flags 0x3a, 2 × 2. The unsupported predicate is the evaluator's admitted 0x09/0x29 flags domain, before bounds/admission/write/draw for that record. This is not an original-game rejection. Counts are 37 completed object records / accepted 37 / rejected 0 / resource-class 37 / resource draws 37 / combined supplied draws 37 / spawns 0. Actual HIRSCH header 71 was not reached; 4,589 candidates are not a draw total.

Header 34 record 0 retains type 70, variant 6, computed height 48 and draw count 1. Sampled stamp 500, mode 0 and repeated supplied draw 7 are explicit test inputs, not decoded MAIN values or the original world generator/cursor.

D: zero-item branch 0x1008935f reaches epilogue 0x100894a9; call 0x100894b2 uses concrete island vtable 0x100cac2c slot +0x198 -> 0x10092b40–0x10092c94. Its only callee 0x100b5f90–0x100b5fb6 truncates with restored FPU state. The pass clips the block, compares unsigned quad mean strictly below default 32, and ORs 0x1ff0000 into the second word. No World/PRNG callback occurs. [Transport §3.4.13](../../re/transport.md) and the contract retain addressed rationale. Independent HEIGHT census gives header 33: 1,024 updates; header 34: 977. Both are asserted by the final Debug real test. Header 35 stops before its epilogue.

The private initial `real-first.log` is explicitly superseded mask-incomplete investigation, not proof of complete scratch state. The finite callback closure was authorized separately by the orchestrator; no new stone admission/lifecycle read was attached. Private primary listings stay out of git.

## Gates and preserved failures

Final canonical Debug/ASan source was clean `1e061493` before and after each gate; provenance, commands, inventory, binaries and CMake cache identity are embedded in the logs. Each begins with name-safety and 25-case verdict preflight. Four required contracts run separately: public adapter contract, resource_import_reference_contract, wildlife_reference_contract, wildlife_load_reference_contract. Each is 1 selected / 1 executed / all other categories 0. They are excluded from the main count, not counted twice.

### `cache/wildlife-prefix-1/debug-combined-final.log`

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=32 executed=32 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

### `cache/wildlife-prefix-1/asan-combined-final.log`

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=30 executed=30 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Debug main 32/32 includes the original adapter case and the original AnnoWelt walk-table reader case; ASan main 30/30 excludes those two asset cases and covers synthetic parsing/bounds/composition. No failures, timeouts, memory refusals, empty, incomplete, skipped or sanitizer reports in either final selection. Reused the unchanged parent picture contract (recorded in RESOURCE-FIX evidence); this packet changes no render/picture inputs and runs no render suite.

Original compile failure `build-status-01.log` was a doctest OR-expression decomposition error; corrected to an inspectable Boolean. It remains preserved. `session-synthetic-final.log` and `callback-boundary-final.log` are earlier focused checks, superseded where covered by the final clean-source gates.

One private mutation only: exactly one `out.consumed += effect.value().draws` was changed to `+= 0`, compiled and linked ahead of the actual adapter archive. Legal control exit 0, mutant exit 1: consumed 2 instead of 4 and stamp-500 first timestamp 5 instead of 8. 37 intended assertion failures / 224 passes in that one mutant test, no compile or infrastructure failure. Canonical source untouched; `cursor-identities.json` retains both source/binary/control/output hashes. Final canonical Debug binary matches that legal receipt.

Seven changed translation units pass strict -O3 -Werror on Apple Clang 21 and cached GCC 15.3 (network none, read-only source, 2 CPUs / 6 GiB); GCC shadow negative control refuses. clang-format 23.1.3 dry-run, lint_sim and diff-check pass. Targeted compiler checks are not a full warnings sweep. Dev/ASan builds retain the inherited Apple duplicate-library linker warning; compiler diagnostics are zero.

All 276 tracked fixtures/synthetic files byte-equal post-intake 7366c8fa; no pin/hash/golden/recording changes. This is a byte audit, not a full pins.py run. The authorized helper intake changed wildlife_vectors.txt relative to RESOURCE-FIX; do not misclassify that inherited edit as an adapter fixture change.

Host: Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1, Apple Clang 21, CMake 3.31.10, Ninja 1.13.2. jobs 1, test threads 2, 2,048 MiB/thread, admission enabled; no concurrent test/build campaign at final preflight. ASAN detect_leaks=0/halt_on_error=1, UBSAN halt_on_error=1/print_stacktrace=1. GPU not used. Parent display-awake assertion preserved.

## Source, binary, input and log identities

Full commands remain in canonical provenance and private compiler/cursor recipes. Private cache paths are relative to the owned checkout. SHA receipts are identities, not copies of original content.

| Artifact | SHA256 |
|---|---|
| `cache/wildlife-prefix-1/debug-combined-final.log` | `ac0850afe0077e2487f5529288cb0b339ad451879153bb8a144a71fbb505099b` |
| `cache/wildlife-prefix-1/asan-combined-final.log` | `b85c4a7e16d8ea0317af874a83f4ff94659efea03c3d3e24bca83ca0e0c44073` |
| `cache/wildlife-prefix-1/debug-combined-final.log.inventory.json` | `c189581cf1de59347edf356ba51e843510ea1389e5d41f349a8f2990e7c3f046` |
| `cache/wildlife-prefix-1/asan-combined-final.log.inventory.json` | `ccf2977f4f238d2791756e848ca7844d31b52f8dfff99241932c8fa2b7193c82` |
| `cache/wildlife-prefix-1/cursor-proof.log` | `eb01a4629b237b14eba06d34648fecce0a2b59c351a673e28b667281cb3a3897` |
| `cache/wildlife-prefix-1/cursor-legal.log` | `a323107e92017c206416a0c9c07a32e1d427916d7080e8db06d252dd56e9445d` |
| `cache/wildlife-prefix-1/cursor-mutant.log` | `bd156ba06846d042ec946ff56c7e0a52df036b3bbe1c69800a7e52fb3572d29b` |
| `cache/wildlife-prefix-1/cursor-identities.json` | `bc33d86d1f16a8e757cf1fe184cc4c0ca05ff99ed2ffdf8df1c4b4404593f1cd` |
| `cache/wildlife-prefix-1/clang-warnings.json` | `a7e607e7a3095cd35f2ce33f39feca6f652c822ebc15aef50b2337925364402a` |
| `cache/wildlife-prefix-1/gcc-warnings.console` | `55803d7640a97e2207ab0e29677e1475852c536593e8d17ed32abab0c3c79374` |
| `cache/wildlife-prefix-1/source-identities.json` | `3d560e62910594360c5c0fc120eb3e84f928ba2044d7173e949379be99e424bf` |
| `cache/wildlife-prefix-1/intake-file-identities.json` | `bdf491eabe7a79f0e1c9c0c45e812bb57ad14f8b1327fb38a763e1735a6b4f46` |
| `cache/wildlife-prefix-1/fixture-audit.json` | `b07b01231132ef17a4d72806ded983550341b8c9027e1710f2a0d60dcf94322e` |
| `cache/wildlife-prefix-1/original-input-identities.json` | `0567409ceeebd31f153b69cf4ae5a2f52f8afe06d20186e668dd2674a0d67dc8` |
| `cache/wildlife-prefix-1/host.txt` | `9676488c9f30b15558bded4006d07684daa1ace58f2a685fd536b9ed6788b337` |
| `cache/wildlife-prefix-1/real-first.log` | `7e2432a8253c31d07f6de60739ef500d2ac75d5948c674a6fc7bb21e9e6eb734` |
| `cache/wildlife-prefix-1/real-water-expected.json` | `f7cdcfff30480d79ff3d25a49051b5365b85fff06767b1095ddfe239880b8f7b` |
| `cache/wildlife-prefix-1/rebuild-dev-canonical.log` | `deb4cce352f42a2938f97068c402aa9cfd69c0fd763631983195902bdea2fe86` |
| `cache/wildlife-prefix-1/build-asan-final.log` | `aa0fd247c4a9f1d9f30a30d24bc64db648b71f0532ac168f03d1079e8b93700a` |
| `build/dev/src/sim/anno_sim_tests` | `818eb9d8886a64a5faa470fa55d8228dc030f16d641e815460a8d26470fd0717` |
| `build/dev/src/formats/anno_formats_tests` | `a3b4e263b91f6b660d3ab02a69ce0738780527bb25e84fb5fd375fe8656a0a7e` |
| `build/asan/src/sim/anno_sim_tests` | `0aa4e1f509101a511cf0f9266a45171f2a3ee6161eae97545904e4a5b635ef04` |
| `build/asan/src/formats/anno_formats_tests` | `e85820d614f7edcd2d13c59c1f0b0e6a265f104fdd3eadb87e99ec36527dc883` |

Original runtime inputs:

| Relative game input | SHA256 |
|---|---|
| `AnnoGame.dll` | `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc` |
| `AnnoWelt.dll` | `f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2` |
| `Anlagen.dat` | `b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75` |
| `Scenes/Tutorial/01Discover01.sze` | `225ed50b005743e00d8d3ffe18667d4565dc781ef45bd6d9b767fd2fe056c0f6` |

Changed-source receipt:

| Path | SHA256 |
|---|---|
| `docs/sim/wildlife-prefix.md` | `774bd590a691a3691edb6c105e40d40a29b7e554be45b725a8984a71df2036a7` |
| `src/formats/CMakeLists.txt` | `534f0203db585679e7b5d8e6220b1c7e5ecf71b1949dc3d1879a0c32c684ee71` |
| `src/formats/include/anno/formats/annogame.hpp` | `78f218032ee7211774b4ee98e01ec625571d6f7f69f7b3c77e742b8e7c8fbf8d` |
| `src/formats/include/anno/formats/insel.hpp` | `06d5d839491e42f019b9a0583d7d9964f28ec4432902ab7a9898d302283025d1` |
| `src/formats/src/annogame.cpp` | `19953cc40a71e256e57b5d737f15691e18352668b003f6c413f412a175594fc1` |
| `src/formats/src/annowelt.cpp` | `0df472027d5a63e639af49774ce236784f6fcf12bf54745b4b2f9a5874c4acfd` |
| `src/formats/src/insel.cpp` | `ff26381fde524f6cfba925c8db0ece3dbbc880e8fd84a1c76e0aa3cb5300c3b3` |
| `src/formats/src/pe_table_view.hpp` | `d4fb513564b0101a7c0b19de8dca286794e4ed79b6d4c4583107843b47a48508` |
| `src/sim/CMakeLists.txt` | `034e64e85a8df551a5db98dab3daffe6226963bd07a209d54240f5ebd04d4ec2` |
| `src/sim/include/anno/sim/resource_import.hpp` | `d560a4c9199dc2b1d8066987e276be59353abbde73070ed7363bbe84f9ed04f0` |
| `src/sim/src/resource_import.cpp` | `08fceccc57c72930ad8a1d9f0cff651cf95999a15b50b1cbd201f2406ba36752` |
| `src/sim/tests/resource_import_test.cpp` | `ad7a73263210e6b0ba31bcc679d845be7fce16b1a7e3e1d7bc78562aeac2e3b8` |
| `src/sim/tests/wildlife_prefix_test.cpp` | `b76e0650fa9298a64adb36a1b93a007ad2dd9affe8290da61c030a073f7ecdcc` |
| `src/sim_load/CMakeLists.txt` | `c291874cacf9dfb09f91ced4f31ca23f7fbd0237db6b99a2b54be5f56d591c82` |
| `src/sim_load/include/anno/sim_load/wildlife_prefix.hpp` | `2a95079dae33eec454dcdc100a152b3f49b49ea84b9857a7628a0dd4273ac025` |
| `src/sim_load/src/wildlife_prefix.cpp` | `951591998f81f0b03048321df07615ffb1b571c6064388c658fb035bcc70c5ab` |

## Decisions and reasoning

- Reuse the reviewed selector and HIRSCH/load helpers via one narrow owned Session; avoid full-map copies/fact rebuilds per raw record or batching draws out of source order.
- Require explicit sampled stamp/mode provenance and a bounded supplied stream; do not infer absent runtime inputs from MAIN or claim whole-world alignment.
- Model the necessary header mask effect even though it draws nothing; preserve the earlier incomplete investigation rather than silently promoting it.
- Stop at type 1558 before any unsupported writes/draws. Do not skip a nonresource record that could change later admission.
- Keep semantic persistence proposals separate: header/rules owns enable+mode; island owns variation and effective spawn order/links; ordered loader owns load draw-site provenance. No numeric tags reserved, no serialization written.
- Reuse unaffected parent evidence, retain real-file execution in Debug, and run synthetic error paths in ASan; no unrelated long corpus/render/sailing campaign.

NOT_RUN: full-world/normal loader publication, actual original/HE runtime, Windows/WSL game-file suites, complete fauna sweep/materialization/lifecycle/transitive draws, full pins/warnings suites, LeakSanitizer, original global cursor alignment and unproved SCENE/player/actor/post-island callbacks. CI is not called green until its exact run finishes successfully.

QUESTIONs: none required to review this bounded transient slice. Next release must establish exact 0x3a stone admission/write semantics, then continue this same prefix to its next boundary. World/native/default integration and numeric extension ownership remain unreleased.
