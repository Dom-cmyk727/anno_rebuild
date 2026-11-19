# WILDLIFE-CHUNK-PLAN-1 evidence

2026-11-19, READY FOR REVIEW; final implementation review pending orchestrator.
[Contract, complete authored lists and field reads](../../sim/wildlife-chunk-plan.md).
Base f5f1c9fa7f0eca4d6263147797022ff1c5e6d771; original dispatch
9b4d6c27cbbf61f72e492b44bb1c7acecc9cb8b2. No board/main merge/rebase.
C0 published 4253dfbc0b4b7fff1418baf24f520c27a711bc02, tree
c72a95c0984cb4fc56ff320d8d3eb0af3d35e416. Its CI 44517180198 was queued
when read after that ordinary push; not substituted for final-head acceptance.
Tested implementation ef8fcb48349bf2bc8fe6521a23957e481cf129ec,
tree 70e106988054ae54ab99e5c763aabb7da04ec0bc. Publication adds report/evidence only.

## C0 disposition and primary boundary

[C0 independent review at 077b2cf4](https://github.com/Dom-cmyk727/anno_rebuild/blob/077b2cf41c2746bb2f175ca515c9fd9191f44087/docs/orchestration/reviews/2026-11-19-wildlife-chunk-plan-c0-review.md)
APPROVED the contract clarification only, not unfinished implementation. The
amended C0–C5 packet was read in full, never merged. For consistent immutable
habitat and complete records, distinct candidate collisions C<=retained R and
density D(N)<=N; therefore positive deficit D-R<=N-C. Inserted positions have
already left the candidate vector. Duplicates only increase R; absent category
flags only suppress collisions. Counter overflow is outside supported profile.

The full adapter consequently cannot reach early candidate exhaustion. A typed
noncommittable defensive guard remains; lower-level wildlife_replenish exhaustion
is tested separately with exact consumed 1. No artificial lookup list/density
override/public seam, no full-adapter branch-coverage claim, no whole-game OQ-572
closure or mutable-callback/allocator assertion. All 17 density values and minimal
legal N 4/R 1 collision tested. Reachable bounded draw exhaustion has full diagnostics.

D: GOG Welt 0x10093480–0x10093767 only, SHA256 in original-source.json.
Retention uses dword count and signed deficit comparison at 0x10093618. Widened
counter admits only the original signed 32 safe range; no engine entity cap.
Only category 0 records count; occupied 0x40 bypasses habitat. Rejects unlink and
prepend reuse; other categories preserve relative order. Candidate scan is
row-major odd x/z, 16 cells; supplied outcomes are not actual terrain/type reads.

D: 0x10093628 pops reuse or normal-success allocates 16 bytes. New initializer
writes table, next, category only. Fresh packed bits/time/id/goods/byte 11 are not
invented as zero. 0x10093684 and 0x1009367b–0x100936b2 write goods 255 and low 6
position bits before lookup, preserving high 2. Lookup 0x1007c630 uses category
flag and first category 0-position match, no occupancy filter. Same reserved
record survives collision. Timestamp low8(time/1000), clearing 0x40, prepend and
flag 0 occur only after a successful insertion. High 0x80 stays unknown when fresh.
Cleanup discards leftover reuse and clears only flag 0 iff count 0, not excess trim.
Stop before materialization entry 0x10093767: no species/coordinates/manager/actions.

## Owned implementation and literal proof

`prepare_wildlife_chunk(input, draws)` returns Result<complete plan, failure>.
Failure diagnostic is explicitly named diagnostic_only, carries precise stage,
current chunk/reuse/reservation/candidates/counts/attempts, never a success/rollback.
Initial record positions/occupancy must be known where required; other unknown
fields remain optional. Fresh tokens are distinct caller-local allocation facts,
not World IDs. Insufficient facts are InvalidFacts at reservation, not native
allocator failure. Validation is adapter admission before effects, not original
query timing. No callback framework, pointer image or persistent schema.

C1–C4: seven final authored cases 268 assertions; literal complete ordered snapshot,
all changed/preserved fields and each attempted reservation checked. Main expected
final token order [50,10,30,90,20,40], consumed 4 of [0,0,1,0,999], remaining candidates
[45,47,13,15,25,27,29,31,41,43], reuse/discard empty, retained 5/desired 5.
Reject order A/B reverses to B/A; unoccupied K collides; occupied false-habitat O
is retained. B's collision changes position/goods but retains timestamp 13/ID 303;
same B inserts, then A, then fresh. Fresh ID/high bit/byte 11 remain unknown.
Inputs and unused draw suffix immutable; repeat result equal; owned output survives
all input owners. Local token 0 and known saved ID 0 are distinct from missing values.

Draw prefixes 0..3 each refuse through the actual adapter: B before any attempt;
B after collision with old timestamp; A reserved after B insertion; fresh reserved
after two insertions with all data still unknown. Each partial list/reuse/candidate
vector/count/field is literal, not reconstructed from helper counts. Discard never
occurs on those errors. Missing habitat/invalid fields/ambiguous tokens refuse;
missing fresh allocation facts expose the already completed partial effects.
Zero candidates/all rejected discard in head order and preserve unrelated flags;
excess occupied records never trim or request missing habitat; wrong-category,
first duplicate-position match and category-availability controls distinguish lookup.

The existing density helper is called unchanged. Supplied outcomes lack full
habitat inputs, so wildlife_habitat is not executed. Existing replenish helper
cannot expose intermediate record writes/partial state; composer owns that bounded
loop without changing the accepted public helper. Reject collection reverses once
(linear), avoiding quadratic prepend construction. No performance benchmark claim.

## Actual causal checks and first attempts

Private production-TU variants compile/link before library resolution. Each exact
replacement count asserted; intended semantic assertion and child exit 1 checked.
Canonical source/test/binary identities rechecked unchanged. Legal 7/268 passed.

| Actual wrong implementation | Failed cases | Failed assertions |
|---|---:|---:|
| Append recycle instead of head order | 4 | 17 |
| Append inserted records | 5 | 13 |
| Occupancy-only collision | 6 | 23 |
| Reevaluate occupied habitat | 5 | 29 |
| Trim retained excess | 1 | 2 |
| Invent fresh saved ID 0 | 5 | 11 |
| Erase/shift instead of swap-pop | 5 | 13 |
| Premature timestamp write | 3 | 3 |

No crash/empty-filter or compile failure is counted as a killed mutant. Private
causal.py, actual changed sources, executables, logs and causal-results.json retained.
First build/authored 7/266 passed; source/binary retained. Added known-zero identity
control gives final 7/268 (not 14 unique cases). First causal harness legal control
passed, then stopped BEFORE mutation because clang-format split the reverse call
and the exact replacement assertion found 0 matches. Original script/log/legal
binary retained in causal/; corrected harness/output use causal-corrected/. It
also corrects the prospective newline pattern for swap-pop. No production defect
or canonical gate failure is hidden. First Debug/ASan canonical attempts passed.

## Canonical gates

Cache root cache/wildlife-chunk-plan-1/. Both gates freeze tested ef8fcb48 with
1019 source/fixture/tool/binary entries. Required density/tight-collision/lower-level
scope case executes once separately, then 15 main entries: 16 unique/profile = 7
chunk + 4 release + 2 prefix (raw/serial) + 1 all-family 74 vectors + 2 leaf controls.
Source-bound species/creation implementations and fixtures untouched: prior focused
source evidence reused, not rerun merely because adjacent. No new public reference.

Jobs 1, timeout 180, test threads 2, host memory admission enabled. Game directory
unset and asset-policy unavailable intentionally: ALL checks synthetic, not corpus
proof. No skipped prerequisite occurred. No renderer change/new public evidence
command, so render auto-contract skipped explicitly. ASAN_OPTIONS disables Mac
LeakSanitizer only; ASan/UBSan enabled. No memory refusal/timeout/gate rerun.

`debug.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=15 executed=15 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

`asan.log`:

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=15 executed=15 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Apple Clang 21/GCC 15.3 O3/Werror both affected TUs pass, GCC shadow negative
rejects. clang-format 23.1.3/lint_sim/diff checks pass. Builds 0 compiler warnings;
inherited duplicate anno_sim linker warning remains disclosed. Host M5 Max/18
logical/48 GiB, macOS 27.0.1(26A434), Spotlight active; conservative jobs, no perf claim.

## Receipt identities and limits

110-entry recursive closeout-sha256.json SHA256
b57d0cde2c68a4a8c2744677225777a2ed93c3887a57985ac5b1a9430dec36e9.
Contains actual private original range, C0 CI receipt, scripts/initial control
failure, all variants, strict objects, frozen sources/binaries and gate inventories.

| Receipt | SHA256 |
|---|---|
| Debug gate | 28b5a8365c5aee10c6f55ecbf41a2c555c7d0952df10d4a2be15a24975867a11 |
| ASan gate | 710e8866d0d65f1be8a7014e84a96e71e0e84459c7eb98d5fd3a237f3f02c858 |
| Retained Debug sim | 037b3be86cd5022395411887aaec6e4150d2067da88fff8d40d666c3bf2d803c |
| Retained ASan sim | 23a6695a18719b44792949bda11037b131cee4d6999b1c5eba47dbfa394dd47b |
| 1019 frozen identities | 93ce6f9c2a26de87cb89daa0f2d5e758aafa2eaabaf2a348602291877eebf58a |
| Actual causal commands/results | 8f6d46cf4b5dede33a7aee59152d134aee468c309fea9d94871fcaf8bdd14631 |
| Apple strict | adf51ddf6a8decb268020671ef5f711c619739ee8ced1d3fe8940652fe034742 |
| GCC strict | 1fc526abfd68ff9498a9fcd2431edadafa0e6c8f418bbc87b4285520d449b0a8 |
| Original module/range/tool identity | a6151c24af2a8cb74a15594e7772b04aae54f65f387e67b5e95b29785b8bcb96 |
| 292 unchanged tracked fixtures | 76a3095c0c56ede5bd923b95d39ea3b64114648532b3c5c0941fd06b5bdc322b |
| Predecessor 178/83/209 artifact preservation | 7acc15ced180fa2532b6c44bb63b6d5874343e60a063f06da579c995a301eff4 |

All 1019 frozen entries rechecked after gates; prior 178/83/209 artifact manifests
intact. Both actual final sim binaries retained, not reconstructed. Tracked
fixtures 292 unchanged; ignored Python bytecode is not a new fixture. Public
changes only owned contract/evidence/report, 2 CMake source lists and 3 new C++ files.
No original bytes/rows/listings/assets committed. No World/scheduler/native/save/
Rules/tag/default/identity/PRNG/navigation/raster/graphics change.
NOT_RUN: original/HE runtime, actual terrain queries/native allocation/destruction,
materialization/species/coordinates/manager/first actions, full-profile OQ-572
branch coverage (unreachable under approved assumptions), whole-game OQ-572 closure,
LeakSanitizer, local Windows/WSL and broad/full/heavy/opening/pins. Owner awake
assertion, other jobs/coordinators/original files untouched. Hosted exact-head CI
read after own ordinary push; orchestrator owns terminal closeout and final review.
