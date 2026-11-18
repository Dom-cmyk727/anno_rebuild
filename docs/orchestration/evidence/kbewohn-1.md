# KBEWOHN-1 — frozen acceptance evidence, 2026-11-18

Status: **READY FOR REVIEW**. Independent acceptance is pending the Mac orchestrator's
review. Accepted source base: `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`.
The packet at `39a0420643df1081642f549dc32616dd183b88b6` releases this independent
base; accepted DOCS-SIM is reference material only, with no merge/tool intake.
The pushed head/tree and exact-head CI receipt accompany the handoff, avoiding a
self-referential report commit. No main/board/rebase/force push or other-lane edit.

## Acceptance table

| Requirement | Positive proof | Distinguishing counterexample | Evidence | Review |
| --- | --- | --- | --- | --- |
| Sparse original identities | 57 independently read names, exact numbers through public parser; asset id 4000000000 | HIRSCH omitted or changed to 50; TRAEGER shifted to 16 | primary.json; four new figure identity cases; controls.json | pending |
| Zero and unknown policy | WUESTE is engaged optional 0; absent/empty/UNUSED/unknown import as 0 | unknown spelling must not be a known named zero | figure identity spelling case; primary accessor/loader read | pending |
| Corpus and consumer preservation | 368 records / 50 names; 272 corrected, 96 unchanged; every other field and predicate equal | wrong nonzero identity fails exact-value assertion, despite being nonzero | field-audit.json; Debug/ASan concrete readers | pending |
| Content compatibility | old original-registry native/replay accepted before, refused after at registry check | legal old registry control first; corrected registry specifically refuses | identity-before.log / identity-after.log; C++ identity test | pending |
| Bounded fixture regeneration | only 50 creation hashes + 7 economy checkpoint hashes; 118 other files unchanged | both prechecks fail before writing; nonwriting check passes after | fixtures-after-audit.json; producer receipts below | pending |

## Primary evidence and limits

See [the addressed primary note](../../re/annogame.md#bewohntype-identity-completion--2026-11-18-kbewohn-1).
AnnoGame.dll SHA256 `ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc`.
Pointer range `0x10024cb8` to exclusive `0x10024fb8`: 192 slots, 57 unique named
entries, 135 null pointers, no duplicate named aliases. The old map had 20 real
names and one ignored empty sentinel: 37 omitted semantic names, not 36.
`SCHIFFWARE = 7`, hole 8–16, `TRAEGER = 17`; `HIRSCH = 49`, `RAUBTIER = 50`;
previous controls `ESEL = 53`, `SHIP = 113`. The table physically names `WUESTE`
at index 0, while number-to-name accessor `0x10013a20`–`0x10013b33` returns
`UNUSED` for input 0/null/outside range. Do not conflate those facts.
Property 0x1f at `0x10003426` calls that accessor at `0x10003432`; figure loader
`0x1000ff6d`–`0x10010019` scans candidate numbers and matches case-sensitively.
The adapter preserves its unknown/absent-name policy; original unknown-name
rejection is not inferred. No original bytes, instruction listings or records are committed.

Installed FigurenHeader: 368 inspected, 368 known, 0 legitimate-zero entries,
0 absent, 0 unknown, 50 distinct names. Seven table names are unused in this copy:
WUESTE, HASE, KROKODIL, MAEHERSPEZ, PELIKAN, VERKAEUFER, VULKAN.
Thirty newly recognized labels account for the 272 corrected records.
All other FigureType fields and nested navigation facts agree before/after.
No added name enters 97–128 or equals 53, 145, 146 or 150. Existing predicates:

| Reader | Preserved test |
| --- | --- |
| types.cpp | hull 113–128, navigation 113, ware 145 |
| founding.cpp | ship 113, veto 97–128, crew 108–112 |
| original_units.cpp | ship 113, scout 97, held donkey 53 |
| ship_cargo.cpp / freight.cpp | ship 113 |
| placement.cpp / mine_source.cpp / scenario_formats.cpp | ore 145 |
| script_runtime.cpp | 97–128; soldiers 97–112, ships 113–128, crew 108–112 |

Assets' separate player-colour string table is untouched. This audit covers the
shipped mappings and current predicates, not arbitrary custom rules/mod predicates.

## Compatibility and fixture manifest

Complete original registry: `5560dbe117c8900b` → `da762f6991f0f832`.
Both actual baseline native save and replay accept with the original registry,
then refuse with the corrected registry at the existing registry-digest checks.
Native diagnostic names the header/type registry; replay says
`replay refused: type registry digest differs from the log's`.
No field, section, global, PRNG, wire or wrapper version/default changes.
No old original-data recording is described as byte-identical; no migration or
header substitution is used.

| Changed file | Before SHA256 | After SHA256 | Exact delta |
| --- | --- | --- | --- |
| `fixtures/synthetic/sim/current/scenario_corpus.txt` | `5127ee709709b637fcda451c625637b9decce0c07223f125bd6c12bad69f04a6` | `b5d5f36ee8512a37e29f07d77ef91d05a87de07b38827a4d8c468dbaa0f63668` | 50 create_hash values only |
| `fixtures/synthetic/sim/current/loop_checkpoints.txt` | `4710d524faa914f483eff4f6c3495b59d5c897f36245b973a60163a5dea2df76` | `3178eb9fc6a88d7f2b0374720b2207fa26d6b1e2c7988aea606a652b3a292014` | 7 checkpoint hashes only; entry/tick unchanged |

All 120 baseline fixture files were hashed; 118 are byte-identical.
`pins.txt` remains `c6c5cb7cba3d1fc95151bdf1e0e83d8ad4bc112b5be7386fb6b1430f0146a1bf`: 0 pins moved.
Historical fixtures and every other current replay/log/oracle file remain unchanged.
The Tutorial economy loop also loads the full original registry; its 7 state hashes
change transitively. Its physical/accounting/native/replay assertions pass unchanged.
The corpus test explicitly proves original-data **components**; it uses the existing
component projection and is not a claim that all 50 complete gameplay programs run.
Its historical row comparison excludes only create_hash. The independent private
before/after audit likewise verifies that only that field changes in all 50 rows.

Exact authorized producer selector (`pins.py regen`, then `check`, jobs 1):

```text
^anno_sim_tests\.(scenario corpus original-data components: every \.sze loads, creates and runs five minutes|economy loop: complete Tutorial production sales upkeep shortage paid recovery and native cuts)$
```

Final Debug selector is the following plus the four read-only
population/integration/highmap comparisons; `figure_digest_reference_agrees` and
`picture_matrix_contract` run separately as required contracts:

```text
^(anno_sim_tests\.(figure (facts|digest|identities)|UNIT1 |SCRIPT2 area |ore objects:|salt mine: (the legal site|the refusal matrix)|salt X5: (the mine facts|the lookup visits|the cost of)|scenario OCCUR ore objects:|N0 controls:|freight commands: live authority|salt registry:|registry builds all 921 entries|registry digest changes with every field|registry digest:)|figure_digest_reference_agrees$|anno_sim_tests_lint_sim_(passes|self_test)$)
```

Commands are recorded in each canonical log. Both producer operations use
`tools/pins.py --build-dir build/dev --select <above> --jobs 1 --timeout 1800`;
no default full pins selection or oracle writer ran. Debug uses target
`anno_sim_tests`, jobs 2, threads 2 and required original assets. ASan uses the same
consumer selector, jobs 2, threads 2, ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 and
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1; admission stays enabled.

## Exact canonical receipts

All paths below are under ignored `cache/kbewohn-1/`; each log retains its own
inventory, command/provenance and terminal GATE_END. Executed means executed passes.

```text
first-consumers.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=4 executed=3 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
identity-rig-corrected.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
pins-producers-before.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=2 executed=0 failed=2 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
corpus-host-before.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=0 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
producers-regen.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=2 executed=2 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
producers-after.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=2 executed=2 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
focused-dev.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=54 executed=54 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
focused-asan.log
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=50 executed=50 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Producer console summaries (path-normalized labels; raw private logs are unchanged):

```text
golden re-recorded: scenario_corpus.txt (changed)
golden re-recorded: loop_checkpoints.txt (changed)
PINS regen: PASS; 0 pins moved; 2 fixture byte changes; scope=partial; selection=^anno_sim_tests\.(scenario corpus original-data components: every \.sze loads, creates and runs five minutes|economy loop: complete Tutorial production sales upkeep shortage paid recovery and native cuts)$; evidence=cache/kbewohn-1/producers-regen.log
PINS check: PASS; 0 pins moved; 0 fixture byte changes; scope=partial; selection=^anno_sim_tests\.(scenario corpus original-data components: every \.sze loads, creates and runs five minutes|economy loop: complete Tutorial production sales upkeep shortage paid recovery and native cuts)$; evidence=cache/kbewohn-1/producers-after.log
```

## Controls, preserved failures and evidence reuse

- Three counted private copies of actual production types.cpp compile/link, then fail
  the intended parser assertion: missing HIRSCH → 0 (wanted 49), wrong nonzero → 50,
  sparse TRAEGER → 16 (wanted 17). SHIP 113 / ESEL 53 / WUESTE 0 remain legal in each;
  the unmodified control passes 5/5. Mutation compile/link exit 0; probe exits 1.
  Real source SHA is identical before/after all overlays; clean normal rebuild follows.
- `build-dev-first.log`: 5 owned test compile errors (four unparenthesized ternary
  messages and initializer-list deduction). Corrected in `build-dev-second.log`.
- `first-consumers.log`: 3/4 new cases pass; the identity rig's fresh World::create
  fails because direct parser registry and Builder differ in walking defaults.
  Both identity controls now use Builder; isolated corrected 1/1 and both final gates pass.
  Production validation was not weakened.
- `pins-producers-before.log`: economy runs, 62,616 assertions pass and exactly 7
  expected hashes fail; corpus never enters its body, waiting in Budget::session_for
  because sandbox loopback admission gets EPERM. CTest times out at 1800.02 seconds.
  The original result remains FAIL. No running test/coordinator/awake process was stopped.
- `corpus-host-before.log`: retry only that unexecuted corpus, host permissions/jobs 1.
  Fifty expected create_hash assertions fail, 6,501 others pass, 306.01 seconds.
  Each full row differs only in create_hash; the failure is retained before authorized regen.
- GCC private warning control initially cannot compile the test TU: doctest mount is
  one directory too deep (NOT_RUN). Corrected include mount alone, then both changed
  translation units pass GCC 15.3 Release -O3 and strict -Werror flags without diagnostics.
- After the final sanitizer run, only the existing ROUTETYPE documentation block moved
  above its own table. `comment-only-delta.json` proves every nonempty non-line-comment
  source line identical; other three frozen inputs unchanged. No semantic source/test
  changes occurred, and sanitizer coverage is reused as authorized, not rerun.
- Final formatting: clang-format 23.1.3 dry-run and diff check pass. Local builds have
  0 compiler warnings; inherited duplicate-library linker warnings remain disclosed.

## Frozen source and binary identities

| Source path | Final SHA256 |
| --- | --- |
| `src/sim/include/anno/sim/types.hpp` | `c2f6963b5f85e0fcdd55b7ae4a72c6d5f58733cef7849ce507957e73db4532fe` |
| `src/sim/src/types.cpp` | `264dcd4dc1660d9f91aaccb3313ac71e49348278eb7059889b8bd5f6f96d11aa` |
| `src/sim/tests/figure_facts_test.cpp` | `09c80c62972b4a0813a92b5afca22a8a6777a97f67c327813f723785be2e72df` |
| `docs/re/annogame.md` | `b14024d941b67d2909512016519d8090429d4fed6f550c5ed2ebf40ff2c5b10f` |

ASan types.cpp SHA before the comment move: `eed9ab0ae107972daf221c547eb04f908df23fe8d434f99180a11fa2884e35d2`.

`focused-dev.log` executable identities:

- `host Python 3.14 executable`: `07919b0fbf82742c24420d7dcadbd459f61bc1feb638eca02b81dccab2daf4d5`.
- `build/dev/src/sim/anno_sim_tests`: `16e8c75ae7cf1399a56f42183d8d18821ba0ef399348c717cc5ba8f6dbcd234b`.

`focused-asan.log` executable identities:

- `build/asan/src/sim/anno_sim_tests`: `55299f41cb22df5ed78ac0a621208ba2604224233bff215bb9fa8a36302c0f42`.
- `host Python 3.14 executable`: `07919b0fbf82742c24420d7dcadbd459f61bc1feb638eca02b81dccab2daf4d5`.

Host: Apple M5 Max, 18 logical CPUs, 48 GiB (51,539,607,552 bytes), macOS 27.0.1,
Apple Clang 21.0.0. GCC 15.3 warning control is x86 compilation, not Linux runtime.
No new dependency installation; original game files read-only. Canonical provenance
records the accepted base plus dirty diff identity; final source hashes above and the
handoff commit bind those measured inputs to the committed candidate.

## Raw evidence SHA256s

| Private path under cache/kbewohn-1 | SHA256 |
| --- | --- |
| `first-consumers.log` | `acb5e80bed07a022ec3a8445a6081adb43c3475e47211ce690abc5ee8fc858ad` |
| `identity-rig-corrected.log` | `9cb5a381205f78ed74609b2352218d9e34b4b7a165d00df7ff7db2f3cdb8beb7` |
| `pins-producers-before.log` | `b799beea534509aa2fc3d967205d628aee2305e40241bb6fb6c72e3988a9456c` |
| `corpus-host-before.log` | `8f0f5138474aeda7693282f5af8cb9aded2f84846ba1f8bfc23fd008e63a92ec` |
| `producers-regen.log` | `37c1639477bf23a2e1cde288d66cfa7afa6bc2c5545be58dd102542bc4b82331` |
| `producers-after.log` | `2ffba3231e42a6c1801fac192e53eb3f75b6f58ea5cd50743fa21020d1e00412` |
| `focused-dev.log` | `8dbbf52f84ee9c5c9dd18e847ff90028bb547313a4e437e428473de55d8caa9f` |
| `focused-asan.log` | `b7bff316ae02aa8cdf4616bf5b235315daaea046ccb13c6c17a826df3ee12072` |
| `primary.json` | `66114b37eb11502a52ef318b54b5f1936f42e88f23fd3662ca03ec07f175103d` |
| `field-audit.json` | `22d77ab0a64f0b1e5eabefdd23ccfdb4a73ff1df486002fe9a45f177a3dd0f92` |
| `controls.json` | `7a434b01d5157253e91a44c086cc830ea5f00d32379742aa331b08997b4f58a2` |
| `corpus-before-delta.json` | `f3d851dbd28bf2c17bb48b12ceddc025e9b0d4178d5b9b99b7b7da20c0635c86` |
| `fixtures-before.json` | `da3a2a2f3248e70ce440bdf0793ef8fa61d623fa74f30dcd8115b7467b58cd12` |
| `fixtures-after-audit.json` | `3899ad2dcfe6fa8d8ae0fc91e649d5289239ff0bca5db3322806f4859f48e86c` |
| `identity-before.log` | `2525afb0e293a56a9630b7873bdc44dc1e0b498b430c646544f4da2e99408e51` |
| `identity-after.log` | `c23668e48e14f11bdb3ea684f1252772a686bd4e33364d61efbd9a186e634a2d` |
| `frozen-inputs.json` | `9724b5a1f6b4d44c8087a6ed9c1be856a5a7aa53c3c777d9ac66022ec566689f` |
| `final-source-inputs.json` | `a2ba8c8896165d3547682acb7d8a4291d3214fdea3d9b43f7b69edf9ff34a6ae` |
| `comment-only-delta.json` | `eec58180d684bb69b870b0de15d844a7b51bb137079fdb742b2eeffa8d6333cc` |
| `gcc-release-results.log` | `1ad87e1c9ae0206120e8a469ad996c5b6dfcd764e41f270432190e5fb3fb7fdf` |
| `gcc-test-corrected/gcc-release-results.log` | `7bfb1722d83c25394fd75bdf862123ecdf3cc5bf9dc67be11b115c38433f05f6` |
| `build-dev-first.log` | `e7c51cacef158f55dd9051c752668ca306441f1b36625c42c1b53ffd93cd449e` |
| `build-dev-second.log` | `bde822fbf57a28b197951212aab03184d256efb802123e213f82add8fbf08f48` |
| `build-asan.log` | `57790d6dced5250239661e53b17d969f5ced175987bda6cf5e87aa5e13644a13` |
| `host.json` | `2960c6144b7ddf59ef4008174625d44d4c9563895cf5e274877803e47b762839` |

## Decisions and reasoning / remaining limits

- Keep explicit sparse source-format semantic values in the existing Named-table style;
  neither compacted index nor asset id identifies a class. No figure-count cap is added.
- Use exact values and the optional helper to distinguish legitimate zero from fallback;
  retain existing unknown-name import policy and document the original accessor nuance.
- Audit concrete numeric readers before claiming preservation. Correct classification
  does not implement wildlife spawn, movement, hunting, predators or combat.
- Regenerate only the two measured original-registry producers, through existing writers,
  then nonwriting check; never change independent oracle values or unrelated fixtures.
- Preserve failures and retry only an unexecuted admission-blocked case with host access.
- NOT_RUN: native Windows/WSL runtime with originals, original/History Edition execution,
  live wildlife, full renderer/sailing/town campaigns, LeakSanitizer, full two-compiler
  warning sweep. Targeted GCC controls and exact-head CI are distinct evidence.
- CI without originals establishes no original-file corpus coverage; local receipts do.
- QUESTIONs: none blocking. Independent review and landing remain the orchestrator's.
