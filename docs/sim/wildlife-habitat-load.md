# Source-backed habitat for one interior chunk

WILDLIFE-HABITAT-LOAD-1, 2026-11-19. H0 contract before implementation.
D = addressed original GOG read, P = independently decoded original file,
H = hand-derived authored input, C = executed control, U = unresolved.
[Decisions](wildlife-decisions.md), [final metadata](wildlife-final-facts.md),
[first-call stage](wildlife-first-call.md), [maintenance](wildlife-chunk-plan.md),
[primary habitat](../re/wildlife.md#1-fauna-sweep-0x10093480).
Review pending orchestrator. No runtime fauna, allocation or whole-world cursor.

## Decisive original predicates and property source

D: Welt 0x10093056–0x10093061 tests the WHOLE u16 centre word against zero
BEFORE descriptor lookup. Empty centre returns false, even if source type 0 is
defined. Nonzero centre gets actual descriptor +0x70, kind +0x5c; raw 0xe2
ROHSTWACHS instead resolves original word+1 at 0x10093088–0x1009309e. No 14-bit
mask or recursively repeated substitution. Resolved output 0 equal 0x46 OR kind
0x145 admits the centre (0x100930a0–0x100930c8).

D: eight neighbours in order left, right, northwest, north, northeast,
southwest, south, southeast. Each zero word skips its lookup. Each nonzero growing
type substitutes its OWN word+1; compare resolved goods and kind independently.
Score is 1 plus every goods 0x46 match plus every kind 0x145 match. A double match
adds 2. Left branch folds the initial 1 into its arithmetic (0x100930da–0x10093142);
empty left retains 1. Remaining seven offsets/branches span 0x10093142–0x1009343e.
The original reads neighbours by full island stride, including other chunks.

D: 0x10093444 pushes property 7 then calls island +0xc4. Primary island table
0x100cac2c +0xc4 points to 0x10087c30. Its subtract-7 switch at 0x10087c34–0x10087c3c
uses first jump-table entry 0x10087c98 ->0x10087c4c; that arm zero-extends byte
island+0xc0 (0x10087c4c–0x10087c54). INFO loader saves its unchanged this as ESI at 0x1008914c. The arm
0x100897b6–0x100897ee clears its
scratch, reads payload, copies first byte to that SAME +0xc0. Therefore this is the
actual stored INFO byte 0, not an assumption from a player-facing climate name.
Existing prepared prefix stores leading INFO payload[0] as `PreparedIsland::climate`.
The source binder verifies that against the selected source INFO and source SHA.
No independent numeric property argument is accepted.

D: property 7 equals 5 requires score >=1, otherwise score >=4, inclusive comparisons
0x1009344c–0x1009346b. The unchanged pure wildlife_habitat implements those facts.
Habitat performs no PRNG draw. It does NOT share materialization's actual-only rule.

## Field/effect/default/refusal contract

| Field | Accepted source / effect | Default or refusal |
|---|---|---|
| Completed prepared view | existing first-call FINAL resource tiles | stage contract, not World/full-load certificate |
| Property 7 | exact scene SHA/selected island/leading INFO byte 0 and matching prepared value | missing/unsafe/mismatched INFO or prepared value refuses, never guessed |
| Region | one original 8 x 8 chunk plus 1-tile halo, final objects and full island stride | complete interior only; border/overflow/unaligned geometry refuses |
| Zero word | empty before descriptor lookup | no source type 0 needed or evaluated |
| Nonzero actual | validated final-facts binder full word/kind/output 0 | missing/ambiguous source unavailable, never false/empty |
| Growing actual | exactly one checked word+1 descriptor | missing successor/overflow refuses; no wrapping or tile rewrite |
| Needed set | all nonzero actual and needed successor descriptors once | registry discovers dependency set; source kind/HEAD/output cross-check validates before publication |
| Leaf input | original word plus resolved goods/kind, eight ordered neighbours | centre OR, neighbour independent double contribution; no new Boolean rule |
| Output | owned region/source provenance, 64 complete cell decisions/outcomes | source admission failure is separate from original retention behavior |
| Maintenance | unchanged prepare_wildlife_chunk, explicit records/time/allocation/tokens/draws | diagnostic partial remains noncommittable; no native manager or whole-world draw claim |

Raw scene pairing verifies selected geometry and complete leading INFO under the
already supported first-call profile. This does not rerun or certify the whole
original load pipeline, nor authenticate an otherwise mutable C++ result.
All region metadata is admitted before any leaf/maintenance effect. That can
refuse before an original occupied record would bypass habitat; this is SOURCE
ADMISSION, never described as original reached-field order. Neither the habitat
leaf nor maintenance implementation is duplicated or modified.

## Hand-derived acceptance before code

Authored source types reuse final-facts 104 growing(output 0), 105 ordinary(output
70), 90001 cave(kind 325/output 0); add 90002 both(goods 70/kind 325). Type 0 may
be deliberately defined qualifying but all raw zero cells remain empty. Use a
non-square prepared layer origin (24,40), width 32/height 24, chunk (1,1), world
base (32,48), halo from world (31,47) through (40,56). Earlier raw GFX/edge decoys
differ from the final object plane. All centre/neighbour positions checked.

For a qualifying centre: no matching neighbour score 1, two ordinary matches
score 3 (false at property 2), three score 4 (true), one double plus one ordinary
score 4 (true), lone double score 3 (false). Property 5 admits score 1; raw centre
0 is always false. A growing centre/each of eight growing neighbours must resolve
105 once, while original 104 stays in provenance and in the final tiles.

Composition fixture property 5 has exactly candidates [9,11,13,15,25,27,29,31]
true; other 56 cells false. Records in order: token 10 occupied at false position
2; token 20 unoccupied at false position 4; token 30 unoccupied at true position
9; token 90 category 3. Recycle token 20; retain 10/30/90. R 2, N 8, desired 3.
Draws [0,0,999]: position 9 collides with 30, then position 31 inserts SAME token
20 with old ID/high bit retained, goods 255, timestamp low8(259000/1000)=3.
Final token order [20,10,30,90], consumed 2, candidate remainder
[29,11,13,15,25,27], no leftovers. Prefix of 1 draw refuses with reserved token 20
at position 9/goods 255/OLD timestamp 12, exact survivors/candidate remainder;
no successful partial plan/rollback or invented early full-profile exhaustion.

| Requirement | Distinguishing expected result / wrong control | Review |
|---|---|---|
| Property source | INFO[0] 2 vs INFO[1] 5; prepared mismatch refuses; source value 5 works | pending |
| Centre/zero | raw empty false even qualifying defined type 0; growing centre resolves once | pending |
| Each neighbour | eight offsets individually identified, growing successor changes goods | pending |
| Score/threshold | scores 1/3/4, double match, property 5 vs 2 | pending |
| Geometry | non-square stride/nonzero origin, cross-chunk halo and edge decoys; borders refuse | pending |
| Source knownness | missing actual/successor/overflow after legal control refuses, no false fallback | pending |
| Maintenance | full 64 outcomes then literal lists/collision/draw prefix above | pending |
| Ownership | destroy scene/registry/metadata/prepared owners, same owned result and immutable inputs | pending |
| Source proof | one original completed view, independent raw region/property/64-result oracle | pending |
| Causal proof | actual wrong growing/double/threshold/property/address/missing implementations | pending |

At H0: new binder/tests/gates NOT_RUN. No state/default/identity/tag/Rules/global
PRNG/cave geometry/navigation/FP/raster/graphics change or OQ closure.

## Implemented boundary and independently decoded case

`bind_wildlife_habitat_chunk(first_call, scene, chunk_x, chunk_y, profile,
entries, game, registry)` returns owned halo/source facts, property provenance,
64 ordered decisions and a complete optional-knownness habitat array. Scene SHA,
single fresh INSEL5 identity/geometry, complete leading INFO and prepared property
are checked. Leading repeated INFO follows source order/last value; missing or
late INFO refuses under the existing completed first-call profile.

All nonzero actual words plus required successors are collected using the registry
for dependency discovery only. `bind_wildlife_final_types` validates every requested
actual/successor kind, HEAD, output and source/registry pair once before publication;
no result can succeed using an unverified registry kind. Only habitat uses the
successor; returned cells retain original and resolved words separately. Raw zero
is never requested, including a defined qualifying source type 0. There is no
second Boolean habitat algorithm: all decisions call unchanged wildlife_habitat.

`prepare_wildlife_habitat_maintenance(bound, records, time, allocation, tokens,
draws)` fills the existing inputs from the owned derived outcomes, then delegates
to unchanged prepare_wildlife_chunk. Its existing typed success/diagnostic-only
failure and count semantics are retained. Records/time/successful-allocation/tokens/
draws are explicit local context, not imported manager state or global PRNG history.
Public owned results remain mutable C++ values, not unforgeable native credentials.

P: original Tutorial/01Discover01 chunk (10,2), base (104,40), property 7 is INFO
byte 0 =2. Independent Python reconstructs the final raw-source region and resolves
original metadata, deriving all 100 region fields and all 64 habitat Booleans.
Region SHA256 5c9ea575bb934faba1443664f3bdc6af90bfea6891d96a30b03b3574784c4ee0;
habitat SHA256 22c133fe0767858fa6a42c437a06e10175b75a55f4075d31e9bdfd9acacc288f.
Only positions 39 and 47 qualify; the odd-sample candidate is 47, N 1/desired 1.
Authored empty records/time 259000/fresh token 5000/raw draws [17,999] insert at
47, consume 1 even with modulus 1, preserve unused 999 and unknown fresh fields.
This real region contains 2 empty cells, no growing object and no double match;
the authored companions distinguish those branches without changing original data.

[Package evidence](../orchestration/evidence/wildlife-habitat-load-1.md) records
actual mutants, frozen gates, original getters and exact source/receipt hashes.
Independent review remains pending orchestrator. No border-policy inference,
actual cave provider/coordinates/creation/manager action or full sweep is claimed.
