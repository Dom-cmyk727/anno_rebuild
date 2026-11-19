# WILDLIFE-HABITAT-LOAD-1 evidence

2026-11-19, READY FOR REVIEW; independent review pending orchestrator.
[Field/default/admission contract and literal maps](../../sim/wildlife-habitat-load.md).
Base 454bc2ea96e58481d3b3b48875ef04a5f64ee449, locally reviewed; hosted acceptance
separate. Dispatch 4a56865ea3ab4151946e8fd51cdc8e6bbd4a61ca read in full, not merged.
Tested implementation 5244e570b76eecb0ecf10943bcb7dc578f074bb6,
tree a5e84058ebfb9e5172fa2459124730e73f0b21a7. Publication adds docs only.

## Original predicate and property-7 source

D: Welt 0x10093030–0x10093472 re-read in full. Actual whole u16 centre word zero
skips BEFORE descriptor lookup at 0x10093056–0x10093061. Nonzero actual kind 0xe2
substitutes actual word+1 once (0x10093088–0x1009309e); resolved goods 0x46 OR
kind 0x145 qualifies centre (0x100930a0–0x100930c8). Every neighbour independently
skips zero, otherwise resolves its own growing successor. Offset order is
L,R,NW,N,NE,SW,S,SE. Score is 1 plus EACH goods and EACH kind match, so a double
match contributes 2. Left folds the initial 1 into its branch, including empty
left (0x100930da–0x10093142); other branches 0x10093142–0x1009343e.

D: habitat 0x10093444 pushes property 7, calls island +0xc4. Primary table
0x100cac2c +0xc4 ->0x10087c30. Getter subtracts 7, uses first jump at table
0x10087c98 ->0x10087c4c, zero-extending island byte +0xc0 through 0x10087c54.
INFO loader keeps unchanged this in ESI at 0x1008914c; 0x100897b6–0x100897ee
clears scratch, reads payload and writes byte 0 to SAME +0xc0. This establishes
PreparedIsland::climate's exact source as property 7, not a guessed climate name.
Property value 5 uses inclusive score>=1; every other value inclusive score>=4
(0x1009344c–0x1009346b). Neither habitat nor these getters draws RNG.

Private listing, exact table targets and source identity retained; module SHA256
f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2.
No original bytes/listings/scripts are committed. Existing final-type getter
Game 0x1000a410/output byte +0x206 and fresh defaults are reused unchanged.

## Owned implementation and explicit source-admission boundary

New bind_wildlife_habitat_chunk takes completed first-call result PLUS scene,
chunk coordinates and original metadata/profile/registry/Game. It verifies scene
SHA, fresh INSEL5 slot/origin/dimensions, strict complete leading INFO and matching
prepared property. A caller numeric climate claim alone cannot supply property 7.
Missing/mismatched/late INFO refuses; no absent-as-zero. Source pairing is not an
unforgeable seal for a public mutable C++ result or proof of the full original load.

Only a complete interior 8 x 8 chunk plus one-tile halo is supported. Checked
origin/stride/column sizes/world coordinates and halo bounds; no clipping, edge
wrapping or invented outside-island emptiness. These are adapter boundaries, not
new engine map limits or claims about native border behavior. Read FINAL resource
TileLayer objects, not earlier Terrain::gfx. Return owned 100 row-major source
cells, original/resolved words, 64 ordered neighbour-index traces and outcomes,
source hashes/INFO ordinal/coordinates. No borrowed pointers escape the call.

Dependency discovery collects nonzero actual words and registry-indicated growing
successors once, rejects overflow, then calls accepted final-type binder once on
the distinct set. Every source kind/HEAD/output/registry pair is validated before
publication; registry kind is discovery only, not substituted as a source oracle.
Raw zero is never resolved, even if source type 0 is defined qualifying. Each
nonzero growing cell stores actual and resolved words separately, no tile rewrite.
No 14-bit mask, shipped-ID whitelist, global cache or per-cell catalogue scans.

All decisions call unchanged wildlife_habitat. No duplicated score/eligibility
rule. Source admission is before maintenance and may reject metadata an original
occupied-record branch would never query; it is explicitly NOT original effect
order. prepare_wildlife_habitat_maintenance fills the existing inputs from derived
outcomes and delegates to unchanged prepare_wildlife_chunk. Records, time, allocation
profile, fresh local tokens and raw draws remain explicit caller context. Existing
typed diagnostic partials remain noncommittable; no native allocator or rollback.

## Literal and independent-source proof

Final 3 synthetic cases pass 1286 assertions. Eight neighbours are individually
located at exact coordinates and growing 104 ->105 changes each threshold result;
original 104 and source cells remain unchanged. Full-word/widened metadata reuses
final-facts regressions. Source zero may be defined double-qualifying but is never
bound and yields false; zero is different from unavailable actual/successor data.
Centre growing, scores 1/3/4, inclusive threshold, double-kind+goods match and
property 5/2 distinguished. Source INFO byte 1 is a decoy 5 while byte 0 is 2;
prepared mismatch, exact source mismatch, bad INFO, unavailable metadata,
missing successor, border and overflow all refuse after legal controls.

Composition fixture nonzero origin (24,40), non-square width 32/height 24,
interior chunk (1,1), halo base (31,47), outside-halo opposite-edge decoy.
Complete true candidate positions [9,11,13,15,25,27,29,31]; other 56 false.
Occupied token 10 at false position 2 stays, rejected unoccupied 20 at 4 recycles,
unoccupied 30 at 9 stays, unrelated 90 remains. R 2/desired 3. Draws [0,0,999]
collide at 9 then insert SAME 20 at 31, final token order [20,10,30,90], consumed 2,
unused 999, candidate remainder [29,11,13,15,25,27]. Original ID/high bit kept,
goods 255/time byte 3 changed only on correct stages. One-draw prefix refuses
with reserved 20/position 9/goods 255/OLD timestamp 12, exact survivors/remainder;
no successful partial chunk or impossible full-profile exhaustion constructed.
Owned result survives scene/prepared/registry/source owner destruction, repeated
binding matches, and every prepared object and input snapshot stays unchanged.

P: original Tutorial/01Discover01 first-call view genuinely executed under authored
stamp 500/mode 0/prefix draws 7, not original clock/PRNG history. Private Python
independently reconstructs GELAENDE footprints and HEIGHT/FRUCHTBAR fill, resolves
Game/Anlagen fields and INFO byte 0=2. Region (chunk 10,2) base (104,40) has 100
source cells, including 2 empty; no growing or double match here. Authored companions
supply those discriminating branches without altering original content.
100 original/resolved/kind/goods field rows and all 64 habitat results checked:
region SHA 5c9ea575bb934faba1443664f3bdc6af90bfea6891d96a30b03b3574784c4ee0;
habitat SHA 22c133fe0767858fa6a42c437a06e10175b75a55f4075d31e9bdfd9acacc288f.
Positions 39/47 qualify; only odd-sample candidate 47, desired 1. Authored empty
records/normal-success token 5000/time 259000/draws [17,999] insert at 47, consumed
1 even at modulus 1, timestamp 3/goods 255, unknown fresh ID/high bit/byte 11
remain unknown, suffix 999. Repeat matches; all 16384 prepared objects unchanged.
Original case passes 16481 assertions, not a native fauna or all-island canary.

## Actual causal checks and verification scope

Legal 3-case/1285-assertion epoch precedes the final overflow-message assertion.
Six actual production-TU variants compile/link before archive resolution and exit
1 through intended assertions. Exact replacements and unchanged canonical source/
test/binary identities checked. Private sources/binaries/commands/first failures retained.

| Wrong production | Failed cases | Failed assertions |
|---|---:|---:|
| Suppress habitat growing substitution | 1 | 34 |
| Collapse independent contributions into OR | 1 | 1 |
| Threshold 3 instead of 4 | 1 | 9 |
| Read INFO byte 1 instead of byte 0 | 1 | 1 |
| Transpose halo address axes/stride | 2 | 39 |
| Unavailable successor becomes all false success | 1 | 1 |

Double/threshold mutants change actual unchanged sim leaf privately, not a mock
or copied test algorithm. Other variants change actual new binder. No crash,
compilation error, empty filter or scripted expected-output edit counted as a kill.
After these checks only tests strengthened: a valid max-ID source record is added
and successor overflow must name its exact refusal, rather than any failure.
Production source unchanged. The 6-variant matrix is not repeated for that stronger
assertion; final 3/1286 pass, test TU rebuilt under both strict compilers and final
Debug/ASan binaries rebuilt. Source/test epoch snapshot retained. No failed cheap
or canonical attempt; earlier evidence receipts remain their original histories.

## Frozen canonical gates

19 unique/profile: 4 habitat-load + 7 chunk + 4 final-facts + 1 materialization
actual-only regression + 1 all-family vector check + 2 leaf controls. Two original
cases required separately by exact inventoried names, then 17 main. New original
halo/property/results and old final-field original oracles are explicitly set.
Game directory set, asset-policy required; jobs 1, threads 2, timeout 180, host
memory admission enabled. No refused/timed-out/rerun/skipped/empty/incomplete
case or sanitizer diagnostic. Render auto-contract omitted for this non-rendering
package without a public evidence CLI. ASan/UBSan on, Mac LeakSanitizer off.

`cache/wildlife-habitat-load-1/debug.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=17 executed=17 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```
`cache/wildlife-habitat-load-1/asan.log`:
```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=17 executed=17 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

1025 frozen source/fixture/tool/compile-command/binary identities verified after
both first-attempt gates. 292 tracked fixtures unchanged. Prior final128/
portability50/materialize114/chunk110 receipt artifacts still match, no overwrite.
Apple Clang 21 and GCC 15.3 O3/Werror affected 2 TUs pass, final strengthened test
TU rechecked alone; shadow warning negative rejects. Format 23.1.3/lint/diff/links
pass; 0 compiler warnings. Inherited duplicate anno_sim archive linker warning
disclosed. Host M5 Max/18 logical CPUs/48 GiB/macOS 27.0.1 (26A434).
Unchanged source/binder/first-call evidence reused, not broad/full/heavy/opening/pins.

## Hash receipts

| Receipt | SHA256 |
|---|---|
| debug.log | 57fe13760417e146c5238f9c15bd5f985ff80b703904e3a317a1ac929c88b8fe |
| asan.log | 0ebe8ea850f05646944e67cc4614400c916397cfa99bcc3d67332c2abe7aa1bd |
| frozen-source-binaries.json | c128681b9490d658012c8c438e2d03ecd366394b628880e0eb2f0858ba46ae28 |
| frozen/dev-anno_sim_tests | c06f890165dac5a22be50e23bba8f52e7d98716b9ab2ff39f9d9033a259efed9 |
| frozen/asan-anno_sim_tests | 1bc87206ef39037fa3795a8abea0ac574093a8a4388a7c34a067a8df5b1b421d |
| oracle.py | e5a5cba9f56f255f21c01b20fee17f3045ce5e6b32600bf83583d21694f8f05f |
| habitat-oracle-metadata.json | 3dd530e1de1bd7bd58f26c60c5af9b8d15f134f786669549c6928a70e7d49b52 |
| original-getter.json | 743b6fc1bbf7c4a1f4dce24c00ea1f7fd1fb750732310ae35c8db415e52ebdf4 |
| causal/causal-results.json | 4f33f680ec3c1f765f18e9b9836a115d6ffd279e322595ddfd65fdd51fb3e816 |
| strict-apple.log | c55a9f82bf0625b2513e17c8f681f19441406754872a60f6c5d365ab2c3d6cae |
| strict-gcc.log | ffbc794a08c75824fb942d2513b79ccd80c142a45d1fa5d8ac7b567d6707cbfb |
| strict-final-test-apple.log | 09229230f16b9941fa946c4dde31815887b9914f97c29b017f9cc3f44fd42904 |
| strict-final-test-gcc.log | 7f7ae2197df87a5984aee3cefffbdb5bb3370c5ffe8eaa2e9eac8886bffcf569 |
| predecessor-preservation.json | a552bd233cc4636c64912233af69707ef380e28dbb45118534b968f3cb85c84a |
| unchanged-tracked-fixtures.json | 76a3095c0c56ede5bd923b95d39ea3b64114648532b3c5c0941fd06b5bdc322b |

Private closeout 113 artifacts; manifest SHA256 `3a297bc17a0554e6cd0e0fe9766fe75e682978aaa654ee3fa816244236ab7277`.
Exact publication CI read immediately after ordinary push; orchestrator owns
independent review and terminal hosted acceptance. Prior failed/pending heads
are not relabelled green by this local descendant proof.

NOT_RUN: native island border policy, live manager/allocation, actual source record
and global PRNG/timing history, complete sweep/mutable callbacks, species/position/
cave provider/creation/first action, original/HE runtime, local Windows/WSL,
LeakSanitizer, broad/full/heavy/opening/pins. No World/scheduler/save/Rules/tag/
default/identity/global PRNG/navigation/FP/raster/graphics change or OQ closure.
QUESTIONs: none in the completed interior fresh-prepared source profile. Next
unresolved seam is original record/allocation/time/geometry/live-manager context,
not another guessed property or outside-island terrain. STOP for review.
