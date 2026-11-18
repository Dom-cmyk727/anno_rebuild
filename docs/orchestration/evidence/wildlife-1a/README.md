# WILDLIFE-1a pure-helper candidate

READY FOR REVIEW / CI BLOCKED. Exact base b5c0ada209d94a0082bc88d793c917c7222e6a21; contract 75525fcc; tested code 56762e421914851e669e3de0345b7311e10af51c. The [frozen contract](../../../sim/wildlife-decisions.md) names all ten decisions, addressed evidence, units/widths and hand-derived boundaries. The later report commit changes docs only. No town/setup changes or moving-main merge are included.

## Acceptance evidence

| Requirement | Actual proof | Counterexample / negative control | Disposition |
|---|---|---|---|
| Ten typed pure helpers | wildlife.hpp/.cpp; WildlifeResult carries value/error and consumed count | Invalid domains, exhausted input, OQ-572/574/585 typed refusals | Local Debug/ASan pass; independent review pending |
| Existing independent oracle | Unchanged Python: 65 vectors agree; 31 contract checks pass | Existing malformed/empty/changed-expected contract controls | Required reference tests pass |
| Actual C++ comparisons | 65/65, all 10 kinds, every specified output including list order/draw count | Adapter rejects unknown/missing/empty records; changed expected output disagrees | Debug and ASan pass |
| Exact boundary decisions | Hand-derived cases in frozen contract | 8 isolated source mutations fail the intended vector assertion; legal control remains green | All 8 killed, none a build failure/crash |
| No runtime dependency | No World/clock/PRNG/I/O, runtime caller, initializer or serialization entry | Symbol/section checks and exact old-runtime diff | Reuse proof passes |
| No identity movement | 119 sim fixtures unchanged; existing config/state/RNG paths untouched | Two disjoint partial pins selections, required pins_contract | 0 moves / 0 byte changes, explicitly partial |

Per-kind executed vector counts: animal 13, create 3, density 4, habitat 5, load 3, prey 8, replenish 3, route_old 3, walk 4, wolf 19. Their sum is 65, not 65 CTest entries. The C++ adapter calls the helpers from given inputs, never Python and never expected fields. Expected keys may be a subset as the reference allows; all specified keys are compared. A missing kind/row or unknown kind fails rather than silently skipping it. Helper errors preserve successful draw consumption before refusal. Unused trailing draws remain unused.

Creation/prey mappings are caller-supplied data with widened ids, not a global registry or a compiled-in climate table. The original 16-cell sweep, 8 neighbours, 32-entry index ring and 3 walk candidates describe the addressed local algorithms, not world/entity/player caps. Completion receives post-action quantities and callee outcomes. Load receives an already-divided timestamp base. Walk returns sixteenth radii and angle indices only. Replenishment selection order is not the later reverse prepend visitation order.

## Negative controls and first failures

The [control index](negative-controls.tsv) records the mutated source SHA256 and target record. `cache/wildlife-1a/mutations.json` and the one-off private script record exact single replacements, clean source, build/test receipts and restoration. Every replacement matched exactly once. Each mutant compiled, ran the actual C++ helpers, produced an intended assertion failure in the vector case, and retained an executed legal control. Each canonical mutant gate is selected 2 / executed 1 / failed 1 / skipped 0, exit 1. These are deliberate failing gates, not reported as passes.

| Mutant | Discriminating record/output |
|---|---|
| density_14 | after_jump desired: wrong 4, expected 5 |
| collision_draw | collision consumed: wrong 1, expected 2 |
| swap_pop | swap_order chosen/remaining: stable erase differs from swap-pop |
| facing_wrap | wide_byte_wrap angles: missing byte wrap changes all three indices |
| attempt_wrap | doubled_and_wrap attempt: wrong 255, expected 0 |
| idle_one_draw | one_variant consumed: wrong 0, expected 1 |
| quantity_before_draw | threshold_above consumed: wrong 0, expected 1; next remains 27 |
| predator_503_extended | short_excludes_extra admitted: wrong 1, expected 0 |

The clean helper SHA256 after restoration is aa8e6e66456904f5d9eec68123b0e046d3243e176a0a4eac17eefb123e3b3d4b. It was rebuilt before final focused checks. Generic vector equality diagnostics print `{?}` for vector values, but the logged record/output key and exact source replacement identify each intended assertion; no numeric result is inferred from that formatter.

The first uncommitted dev compile failed on missing validation-expression parentheses under -Werror. It was corrected before the 56762e42 checkpoint; both logs are retained. That compile is not a passed gate or mutant kill, and its uncommitted source preimage was not separately hashed. Final dev and sanitizer target builds have no compiler warning; existing duplicate-library linker warnings are retained.

## Gates and reuse

[Exact receipts](gate-receipts.md) retain every COUNTS/VERDICT line and the PINS summaries, with log names. All acceptance runs use run_selected.py, jobs 1 and normal host admission. ANNO1503_GAME_DIR is set for the required picture contract, not because the helpers read game files. ANNO_TEST_THREADS=4. Mutant source changes and restores were strictly sequential; no commit or other source edit occurred while a canonical gate ran.

- Initial Debug controls: required picture 1/1, then vectors/legal controls 2/2.
- Restored focused Debug: both required Python references 1/1 each, four C++ tests 4/4; unchanged picture contract/binary explicitly reused from the first call.
- Focused ASan/UBSan: picture and both references 1/1 each, four C++ tests 4/4; zero skips, incomplete results, memory refusals, retries or sanitizer diagnostics.
- Partial pins 01: six helpers/replay tests pass; required pins_contract 1/1; 0 moved pins / 0 changed fixture bytes.
- Disjoint partial pins 02: core_hash v1 golden, G0 empty world and core stream layout (3/3); pins_contract 1/1; 0 moved / 0 bytes. This supplements named-pin coverage without rerunning the first slice. Neither selection is a full pins check or a corpus campaign.
- Warning checker once: exit 0; 1,010 project compilations and 2 rejected shadow controls, all four passes clean. Both historical compiler labels used actual Apple Clang 21.0.0. Preserved Mac launchers supply Bash 3 mapfile/BSD xargs behavior and the CMake-detected -Wno-c2y-extensions; no source suppression. Native GCC 13/Clang 18 local checks are NOT_RUN.
- clang-format 23.1.3 and git diff --check pass. No regen, pin edit, default flip, tag, state/version or fixture change.

Unchanged full-campaign evidence is explicitly reused from the preserved atlas-layers game-file gate at 4dfdd9b7ce51b9ad04970da0b247294345f05ac7: required picture 1/1; main selected 2,761 / executed 2,718 / failed 0 / skipped 43 (platform 9, opt-in 29, GPU 2, other 3). The exact log SHA256 and dependency checks are in private reuse-proof.json and the evidence index. From that commit to this packet's b5c0ada2 base, sim, sim_load, formats, test_support and sim fixtures are byte-identical. This packet modifies no existing runtime implementation: only adds an unwired helper object/header and tests. The object has no module initializer; bench_picture and sim_replay contain no wildlife helper symbol. No runtime code references the helpers. These facts justify reuse of the unaffected sim campaign; they do not claim that later UI changes or the new helpers were tested by that historical run.

Host: Apple M5 Max, 18 logical CPUs, 48 GiB RAM, macOS 27.0.1, Apple Clang 21.0.0, CMake 3.31.10/Ninja 1.13.2. Separate build/wildlife-dev and build/wildlife-asan retain town binaries and logs untouched. ASan uses -O1 -g, detect_leaks=0:halt_on_error=1; UBSAN halt/stacktrace on. LeakSanitizer is unsupported here and NOT_RUN, not certified by zero diagnostics.

## CI and limits

Checkpoint [CI 44258806734](https://github.com/Dom-cmyk727/anno_rebuild/actions/runs/44258806734) was read immediately after pushing 56762e42. It repeats the unchanged GCC Release freight_state.hpp:33 null-dereference build diagnostic and the Clang/libstdc++ 15 ciso646 C++20-header build diagnostic. These are build failures, not executed sanitizer results or evidence that the helper tests failed. The macOS C++ job later failed its test step with exit 1; its public annotation contains ordinary contract controls but does not identify the real first assertion. That failure remains unclassified pending the orchestrator’s full-log triage, not assumed to be a helper or inherited defect. Remaining C++ jobs were active at the report snapshot. Exact final publication CI is supplied in the chat handoff; no green result or CI waiver is inferred. The orchestrator owns a separate CI-blocker package; no fix pin has been imported here.

NOT_RUN: Windows game-file tests, native Linux warning toolchains, History Edition/running-original comparison, complete fauna sweeps, scheduling, spawning, animal movement/hunting/predation, transitive draw totals (OQ-578), float/x87 positions (OQ-582), and unresolved lifecycle/reachability. OQ-572/574/585 are explicit refusals, not closed questions. No World integration or WILDLIFE-1b begins in this candidate. Independent review and acceptance belong to the orchestrator; CI must be green before landing.
