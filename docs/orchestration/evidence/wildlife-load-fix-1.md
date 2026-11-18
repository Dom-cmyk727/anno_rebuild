# WILDLIFE-LOAD-FIX-1 — frozen local evidence, 2026-11-18

READY FOR REVIEW; independent review is the Mac orchestrator's. No wildlife
initializer, population, complete tick, hunting or original-runtime parity is claimed.

## Intake and identity

Accepted base `2f9e4c0bf84cf09998a43c7ff8098fa1522f3a4e`.
Intake head `5705678c8e90febb9cbc7357e8c4670a516ee78e`.
Corrected source head `73edb24426e2477f6700755cced9c2ba18cadc4c`, tree `7e5879ce387b39e4568c1106674704f5fce758d2`.
The final report commit and its exact hosted CI run are supplied in the direct
handoff; no pass is inferred from a queued, cancelled or unavailable runner.

| Authorized preserved commit | New commit |
|---|---|
| 75525fcc | 8b22eb49 |
| 56762e42 | 159495df |
| 64ed5655 | 07da5001 |
| 054fd196 | 7f58b9f2 |
| 756e008b | 5705678c |

Only append conflicts occurred in REPORTS and the lint allowance list. Both
accepted and incoming bytes were retained exactly (resolution JSON hashes below).
The helper header/source/test and decision contract matched preserved 756e008b
byte-for-byte before correction. No KBEWOHN, DOCS or decoder source was taken in.

## Decisive evidence and correction

D: GOG AnnoWelt SHA256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`,
`0x100946e9`–`0x10094705`: zero-extended u16, multiplier `0x10624dd3`,
high product shifted by 5; sign correction is zero in this domain. This equals
floor(raw / 500) for all 65,536 inputs. Raw 499 → 0; 500 → 1; 1000 → 2;
33000 → 66; 65535 → 131. Raw 500 is the smallest / 1000 discriminator.
Physical time units remain unproved; the separate sweep arithmetic was not edited.

`wildlife_load_base(u16) -> u32` uses clear / 500 and consumes no draw.
`wildlife_load` retains its already-divided API, admits 0–131, refuses 132 before
any draw, and preserves flag/goods semantics. Base 65 + jitter 31 = 96 is ordinary;
maximum 131 + 31 = 162. High bases cover jitter off/on and draw exhaustion.

The independent Python reference and C++ adapter run 74 hand-derived vectors:
density 4, habitat 5, replenish 3, create 3, load 7, load_raw 5, animal 13,
walk 4, route_old 3, wolf 19, prey 8. Python contract: 36 checks.
Other nine helper families and their 62 vector rows are byte-identical to 756e008b.
See [primary note](../../re/wildlife.md#3-hirsch-loader-and-thirschobject) and
[decision contract](../../sim/wildlife-decisions.md).

## Preserved red and targeted negative controls

F0 ran before changing the helper/reference/adapter. The named boundary test
failed exactly 6 assertions at bases 66/131; 10 passed, including base 65 and
invalid 132 controls. This is a product assertion failure, not a build error/crash.

Scratch overlays compile the actual production source with the actual C++ tests;
no tracked production mutation occurred. Each unique replacement applied once.
Each mutant compiled successfully, failed its named assertion, and independently
passed the unchanged legal control. The clean control passed 3 cases / 65,580
assertions. Ordinary doctest filter exclusions are not prerequisite skips.

| Fault | Compile / probe / legal exit | Intended failure |
|---|---|---|
| Restore cap 65 | 0 / 1 / 0 | legal no-jitter/jitter base 66/131 and exhaustion fault; 6 failures |
| Raw divisor 1000 | 0 / 1 / 0 | raw 500 expected 1; plus exhaustive domain (65,040 assertion failures) |
| Admit 132 | 0 / 1 / 0 | REQUIRE_FALSE(invalid.ok()); 1 failure, no invalid error access |

The eight earlier unrelated production mutants retain their original evidence in
[wildlife-1a](wildlife-1a/README.md); no repeated historical campaign is claimed.

## Canonical gates

All commands use the branch's canonical `run_selected.py`; jobs 1, test threads 2,
build parallel 2, normal memory admission with host socket access. Game files are
set; no skip allowance was needed. ASan/UBSan halt on error; LeakSanitizer is off
for this host. No timeout, memory refusal, incomplete outcome or sanitizer line.
Debug/ASan selector: `^anno_sim_tests[._](wildlife |lint_sim_)`, required
wildlife_reference_agrees, wildlife_reference_contract and picture_matrix_contract.
Each profile runs 6 helper cases and 2 lint cases, plus the three required checks.

### before-fix-red.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=0 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
```

### debug-focused.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=8 executed=8 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

### asan-focused.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=8 executed=8 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

### pins-partial-01.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=8 executed=8 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

### pins-partial-02.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=3 executed=3 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
```

Both pins selections are explicitly **partial**, not full checks. Their exact
selectors and `PINS check` summaries are in the console logs. First selection:
6 helper cases + recorder replay + byte-for-byte log frame; second: three
core hash/empty-world/native-layout controls. Each requires pins_contract 1/1.
Both report 0 pins moved and 0 fixture byte changes during the check.

## Scope, warnings and limits

119 tracked sim fixtures compared before/after; only wildlife_vectors.txt changed
as intended. No recording or pin regeneration. State 139, section versions 1,
serialization, registry, defaults, scheduling and draw sites are untouched.
The accepted-base production diff is only helper/header plus their CMake object
registration. No external helper callers, module initializer, or helper symbol
in the rebuilt replay consumer. No long sailing/render/corpus workload ran.

GCC 15.3 x86 container and Apple Clang 21 compile both changed C++ translation
units at -O3 with the shared strict warning flags and -Werror, 0 diagnostics.
GCC's deliberate shadow control is rejected. The bounded container has read-only
source, no network, 2 CPUs and 6 GiB; it is a compile check, not Linux runtime.
clang-format 23.1.3, lint 70 self-tests, diff and file links pass. The canonical
builds retain the existing duplicate-library linker warning; no compiler warning.
Host: Apple M5 Max, 18 logical CPUs, 48 GiB, macOS 27.0.1; Apple Clang 21,
CMake 3.31.10 and Ninja 1.13.2. Host conditions/commands in frozen private logs.

NOT_RUN: original/HE runtime, native Windows/WSL game-file suites, full warning
sweep, complete lifecycle/transitive draws, World integration and LeakSanitizer.
Global load order, mode provenance, physical time units and OQ-578/582 remain for
later work. No blocking policy QUESTION is needed for the proven / 500 repair.
The separately reviewed raw decoder f1e245d2 is not incorporated; its CI runner
availability is the orchestrator's separate closure, not a source defect here.

## Private receipt index

Raw logs remain under `cache/wildlife-load-fix-1/`; paths below are relative to
that owned cache. No original listings, bytes, data tables or personal paths
are included in this report.

| Receipt | SHA256 |
|---|---|
| `before-fix-red.log` | `5b3cc1bfb38ed2c5da17099258f953bc422a78df7fadcee38f5b09b8ae7edcf7` |
| `debug-focused.log` | `46e5daf46daf14eb622a0484a879f6eff046b1058587456f98b7e2844bc34112` |
| `asan-focused.log` | `2284f232aaf253a53451aca723d6a04fd67c6ec7cd9c2abd1f327473943f438a` |
| `pins-partial-01.log` | `af35db4b7109567a8e5bbaea0268a87a19133267f9bde3cc9cf896aaf9db6f5b` |
| `pins-partial-02.log` | `8b0e67bf310b2f3c17e96fdbb84015c91cf2ee80ef906230d220c40d104e4b1f` |
| `before-fix-red.log.inventory.json` | `23b6ed95be49544ecf2e4d1cc2b22fb2c78cc2976665b6b85f49652790eaa6b6` |
| `debug-focused.log.inventory.json` | `0d7646bb47f27db4873abca56140595bd8a96fafad0e35d08a85268f4260d9cc` |
| `asan-focused.log.inventory.json` | `82b2cbfc957e3c38ec368401425f6f77afb07ff8291666ba9b446b132a3b3948` |
| `pins-partial-01.log.inventory.json` | `95730beb604dd216f4ae4cb81aa026b39c49766b5a3899aef542bbad55d62720` |
| `pins-partial-02.log.inventory.json` | `63af7e6a71343940e2e1cbeddc4953254f2b424e82005279cb2e1e28f6c6f08e` |
| `pins-partial-01.console` | `5291ebe94650cfe1cb9744f3f4d8693a2a7000d38ed984a676286ea56b176aa2` |
| `pins-partial-02.console` | `83fd8f5fcb2a31028cf70c4d5173412eca202488a0e99db731131de184fe9b1a` |
| `intake.json` | `7bf0d6929a735ca01bf31c8673a95916fe9fec0c16d85bef4138e2ae1f8662b7` |
| `intake-reports-resolution.json` | `0bd3465082b2d0e4b93e176b7ee43cb93eae7ef93da1620f6363faafb6c04c3c` |
| `intake-lint-resolution.json` | `6df5f69729af3e8a12858a89aba96b4941a53b15d62c313dd69eceb6179329bb` |
| `scope.json` | `28c18a144ee303bb912e9f9811d3ea02ca8d3d9d919e2528723fcb99145fd16e` |
| `fixtures-before.json` | `37451f2dea0f3854205ba4bc4498930beff2be10f0eb7c71f233b357ca7d47a4` |
| `fixtures-after.json` | `72c6174b58dc96f9469ae66256e14d24eb9b45e40cf5e5b1b2abddff55ff310f` |
| `frozen-inputs.json` | `9f372f09fce25ae0d8e74f55c489d2da322f9353a9afd6617eabc4d1e8920224` |
| `mutants.json` | `a3633a617253b33ad43f355f550bcd9eb9bef00b756733c2f2c861a54eaf5cd2` |
| `control-probe.log` | `2b120e7c30e1bbf5d33f034eb8c833006d5bad6e600312868fdb373fae11a0fd` |
| `old_cap-probe.log` | `a151b50267be0b0f77543771b83ef3373003057fb63760db8b230dc0f2a3bf7a` |
| `wrong_divisor-probe.log` | `9f056aaad11070fff961fbe6a0c55def6b239fafdcf5d72df943f886f8de4c68` |
| `allow_132-probe.log` | `a02f3147686c7791de2037eede7c31e7fb084b76734283de2bd0eb71d97cb1e2` |
| `reference.log` | `c91489ded83450e6f13054f99dbf14fd5aacd8fd8c0ba2e2a61ee601f7331233` |
| `reference-contract.log` | `39322f5fe1f64069cde18cb8826be09c5851af83cc9fa44e034bba66b6332fac` |
| `lint.log` | `c90ca6eba541545a82beb573bfcbcc47531df735edd4ff68c814ab50296e6948` |
| `lint-self-test.log` | `25d5dbd23f182403d85606da5bdd4ceb5f592c1fa1516ac9754f8c8c9a303903` |
| `gcc.console` | `060c032926db79387dfcef3f3d7c79ba3ff6115cdb1ffed52bfe605183da00f1` |
| `gcc/shadow-control.log` | `8c3299d67a4781f90afba52ddbbaaf4296c5666d0c46ffe825ca42f50573dea9` |
| `clang-warnings.json` | `f9dba87a0eec06212a9b7c82302d4730f2bbbeaac30152c8b1636899626b2c8d` |
| `format.log` | `b612ba740becdd7e9615e62612c61cdeb064e15aff898827e5f08971732998af` |
| `diff-check.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `links.log` | `5a0d2b0778d30c37590f81640e87cf64bbcac87c688369e7572ea66de3786e2a` |
| `runtime-helper-symbols.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `initializer-symbols.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `host.json` | `6945b37f90d64268d815c7f88a556aeb1f28eacf41693ab89c95d7291aed5714` |

Decisions and reasoning:
- Separate raw conversion from the existing divided-input API so no caller's units change silently.
- Use independent manual boundaries and original multiply/shift arithmetic, not C++-generated expected values.
- Preserve the initial assertion failure and all inherited receipts; supersede only the wrong load contract.
- Reuse unrelated families/mutants and accepted-base runtime evidence because there is no runtime caller or state change.
- Stop before tags, ScenarioSpec, World import, materialization or hunting; the orchestrator reviews this frozen slice.
