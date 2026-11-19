# WILDLIFE-CREATE-BATCH-1 — focused evidence

2026-11-19. READY FOR REVIEW; orchestrator independent review pending.
Source `7f579a185b0420bcac2385b18daa77330e9b90a2`, tree `685dba89020d6e22df64cffbc3e08e71d9b31a88`; later publication changes docs/report only.
Base 41f6cf99e49832a1f5f925683a089360bc60a33a. No board merge/rebase.
[Contract before code and implemented boundary](../../sim/wildlife-create-batch.md).

## Conditional scope and proofs

Completed import/maintenance/materialization plus one source binding produce owned
ordered entries and optional-record updates. Manager Success/Null/provenance and
success-only id/time are supplied, never inferred. Numeric profiles remain supplied.
Source home/prior are not caller-repeated. Missing outcome is not implicit Null;
known zero id/time/high bit/byte11 are distinct from unknown. Success writes only
id/timestamp/occupied/goods. Mixed resources and typed sidecars remain untouched.
The selector domain was fully computed before creation: completed-prefix diagnostics
retain that complete input accounting, not a newly interleaved native draw history.

Authored traversal [1001,1000,7,6,2,1,0,2000] includes distinct successful descriptors
127/60001 with HP 5/17, Null with absent id/time, known-zero Null facts, fresh unknown
fields, occupied/unselected records and resource sidecars. Manager vector is reversed.
Reversing/rematerializing the list proves list order rather than map/type/id order.
Complete optional records and input immutability are compared, and results survive
source owner destruction. The explicit synthetic runtime occupancy/known-field overlay
is labelled; it is not a file-import claim. Later manager token 7 failure retains
exact prefix [1001,1000], including preceding success and Null. Descriptor/source
admission has no executed prefix. Missing id/time and id 65536 refuse as qualified.

Unchanged Tutorial token 21/type 528: source home [111,45], XYZ words
[42dfaaab,3ff5c28f,42375555]; supplied id 321/time 65538 produces saved id 321,
time 70, goods 0, occupancy true, position 47, unknown high bit/byte11 unchanged.
HP 40 from retained independent raw descriptor oracle; prefix 4745, maintenance 0,
selectors 2, suffix [999]. Null preserves the complete original unknown-field record.
Authored nonflat patch [0,25,50,100] has prefix 96 and XYZ
[4204aaab,3f000040,4244aaab], preserving previous independent geometry words.
Empty cleanup derives release headers/home from source; only first matching
category-0 occupancy clears despite saved-id mismatch. Duplicate later match and
unknown-position tail are untouched; a needed unknown position refuses. Repeated
detached end leaves replacement occupancy. Common transition is shared with the
legacy release implementation, and known-input creation uses one shared outcome core.

## Actual-source negative controls

Legal control: four authored cases, 212 assertions pass. Each private mutation
replaces verified exact source text, compiles/links the actual affected TU and runs
those cases. Canonical source/binary identities remained unchanged. No compile or
lookup failure is counted as a semantic kill.

| Variant | Intended semantic result |
|---|---|
| legal | legal pass |
| null-fills-unknown | CHECK( p.chunk == expected ) is NOT correct! |
| wrong-write-target | CHECK( p.chunk == expected ) is NOT correct! |
| reverse-visit-order | CHECK( batch_tokens(p) == std::vector<std::uint64_t>{1001, 1000, 7, 6, 2, 1, 0, 2000} ) is NOT correct! |
| premature-success-publication | REQUIRE_FALSE( missing ) is NOT correct! |
| release-overwrites-untouched | CHECK( ended.value().island.chunks[0].chunk == expected ) is NOT correct! |

Full commands/source/binaries/assertions: cache/wildlife-create-batch-1/causal/,
causal.py, causal-resume.py and causal-assertions.json. First harness stopped on an
exact whitespace-anchor assertion before the fourth mutation; causal.log retains
that infrastructure result. Corrected guard resumed only the two unexecuted cases;
there was no repeated whole campaign. Final clean canonical rebuild precedes gates.

## Build, freeze and gates

Affected production/test TUs compile under AppleClang 21 and GCC 15.3 -O3 -Werror.
GCC first refused unchecked managers[0] in the duplicate-input test with
-Wnull-dereference; managers.at(0) now gives checked access under the same flags.
First diagnostic retained in strict/src_sim_tests_wildlife_create_test.cpp.log;
first source reconstructed from the single changed expression and explicitly named
strict/gcc-first-test-source-reconstruction.cpp (not a contemporaneous snapshot).
Only affected test TU rerun; first three GCC passes reused. GCC shadow negative
control still refuses. Format 23.1.3, lint_sim and diff pass; new spec links resolve.
Debug and ASan target builds pass with zero compiler warnings; inherited linker
warning 'ignoring duplicate libraries: src/sim/libanno_sim.a' remains visible.

Frozen 1039 identities include actual Debug/ASan binaries, source, all 293 fixtures,
compile commands and test workflow. Copies preserved; post-gate verification has no
mismatch. All 293 fixture working bytes match predecessor freeze exactly; no pins,
oracles, fixture expectations, World/state/default/Rules/PRNG changes. Prior 1206
sealed artifacts across 12 manifests verified, including arithmetic/reference receipts.
Untouched arithmetic contracts are reused, not represented as new current-head runs.
Actual Debug binary SHA256 0ac0f84ac0db2ffe5e94e80472845fa2a4615138974b89bac43f4cd8b2711593;
ASan f8ae89db66acf2233ec8633e9ed8fd6099af2862121aea22479f566a98ecf2d3.
Host Mac17,6 M5 Max, 18 logical CPU, 48 GiB, macOS 27.0.1 26A434; AppleClang 21.
Build jobs 2, gate jobs 1, internal threads 2, host memory admission enabled.
Original assets required through ANNO1503_GAME_DIR (local game/app directory);
ANNO_WILDLIFE_CREATE_ORACLE points to retained descriptor-oracle.tsv (SHA256
69bf63f7acb864820fcddff8eda640ab28c1a9d82ecd0abd6f6c2751d11cf073).
Canonical commands/env in cache/wildlife-create-batch-1/gate.py. Each profile runs
two required original-file tests once, then the remaining 13 cases once = 15 unique
executed, zero failed/skipped/empty/incomplete/timeouts/refusals. No rerun needed.

`cache/wildlife-create-batch-1/debug.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```
`cache/wildlife-create-batch-1/asan.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=13 executed=13 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Log SHA256 Debug `6f68affbd10fae62a4fbd2b6b6e223992e7b3c145e9293c87967337e3a59d6cf`, ASan `7d12631ea22997b0899369110d48db1b09cd5a16da6c1514e9ad3d16f55c2e2d`.
Closeout 114 artifacts, manifest SHA256 `75549d6ec0f2c4afbba21c0e392446f89aa9a1e406e1fe958f1e793b35462e7f`.
Hosted exact-head CI is read immediately after the ordinary publication push;
its asynchronous disposition is reported separately, never called green prematurely.

## Decisions and reasoning

- Shared success-only writes avoid fabricating a packed byte from unknown fields;
  the legacy wrapper retains its known-input pre-Null range validation.
- Separate whole-source admission from reached conditional effects, retaining prefix
  evidence without pretending original callbacks execute or native rollback occurs.
- Share bounded cleanup state and coordinate headers; optional resources never become
  fake complete spawn records. Preserve original home-match behavior without id guard.
- Reuse reviewed arithmetic/oracle receipts and gate only changed creation/end seams.

NOT_RUN: full/heavy/opening/pins/scale, Windows/runtime/HE/native allocator/CW/live World,
nonempty cleanup/movement/death, first-action callbacks and LeakSanitizer (detect_leaks=0).
QUESTIONs: none within the finite conditional contract. Native manager and World integration
remain future dependencies; this batch introduces neither. STOP for independent review.

Publication review correction: the optional end adapter preserves resource records;
typed resource sidecars remain unchanged in batch progress outside that adapter.
Absolute asset setup stays private; the gate still uses ANNO1503_GAME_DIR.
Code/tests remain at tested 7f579a185b0420bcac2385b18daa77330e9b90a2; no repeated gates.
