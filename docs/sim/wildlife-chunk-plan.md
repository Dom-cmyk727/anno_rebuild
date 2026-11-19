# Owned chunk maintenance before materialization

WILDLIFE-CHUNK-PLAN-1, 2026-11-19. C0 preimplementation contract.
Reference [sweep §1](../re/wildlife.md#1-fauna-sweep-0x10093480),
[decisions](wildlife-decisions.md), [prefix](wildlife-prefix.md),
[release](wildlife-release-effects.md), [species plan](wildlife-species-load.md).
Independent package review pending orchestrator. One supplied local chunk only,
stopping at AnnoWelt 0x10093767, not a complete sweep/World publication.

## Inputs, effects and error boundary

Inputs: ordered owned records with caller-local tokens, category, known local
position/occupancy where read, optional high packed bit, timestamp, saved figure
ID, saved goods and opaque byte 11; chunk flags; explicit resolved per-cell
habitat outcomes for one original 8×8 chunk; raw supplied draws; sweep time;
normal-success allocation profile with distinct supplied fresh record tokens.
Tokens are plan bookkeeping, never allocated FigureIds. Unknown native fields
remain optional, not assumed zero. No terrain query/provider/allocator execution.

Output: complete ordered final chunk records/flags; retained/recycle order,
reservation/attempt/insertion trace, leftover discarded records and remaining
candidate vector; exact supplied draw/fresh-token counts. Successful result alone
reaches materialization entry. Failure is a distinct noncommittable diagnostic
with phase, current list/reuse/reserved/candidates and exact consumed counts;
no rollback or partial-success claim. Missing facts refuse, never default false.

| Requirement | Literal hand expectation | Wrong sibling / evidence |
|---|---|---|
| Retain occupied without habitat | occupied O remains even with false supplied habitat | reevaluation must fail |
| Recycle order | initial A,N,K,B,O ->survivors N,K,O; reuse B,A | append instead of prepend differs |
| Category lookup | existing unoccupied K at candidate9 collides while wrong category does not | occupancy-only collision filter differs |
| Same reservation across retry | B chooses9 (collision), then59 (insertion), old timestamp13 until insertion | re-reserve/change timestamp differs |
| Reused fields | B/A preserve ID303/101, high bit and byte 11; goods becomes255 | clearing ID/high bit differs |
| Fresh fields | fresh token50, saved ID/high bit/byte 11 unknown, time known only on insertion | fresh ID0 is not evidence |
| Candidate order | row-major odd samples, swap-pop with last | erase/shift gives different second choice |
| Excess retained | retain all, no draws or trimming | density is target minimum maintenance, not cap |
| Cleanup | discard unused reuse only on success; clear flag 0 iff final count 0 | failure retains reuse/reservation diagnostics |
| Exhaustion | exact consumed count, distinct error, no modulo0 | reachability under consistent chunk profile requires disposition below |
| Ownership | immutable inputs/draw suffix, complete owned output and repeat determinism | no borrowed record pointers |

## Primary field/order closure before code

D: AnnoWelt 0x100934f3–0x10093578 initializes a dword retained counter,
caches next before mutation, increments for category 0 then skips habitat if
packed byte +8 mask 0x40 set. Rejected unoccupied records unlink via 0x1007bd00
and prepend reuse via 0x1007bd50; counter decrements. Other categories untouched.
The desired byte is zero-extended at 0x100935f5–0x100935f7; subtraction uses
32-bit dword count at 0x10093604–0x10093618 and signed jle. Adapter uses a widened
counter but refuses counts outside INT32_MAX before arithmetic, not an engine cap.

D: 0x1009357a–0x100935de scans y 1,3,5,7 outside x 1,3,5,7 and appends passing
coordinates. These 16 original sample positions are algorithm geometry only.
Supplied habitat outcomes are explicit; this API has no full tile/type/neighbour
facts with which to execute wildlife_habitat. Reuse the accepted density function.

D: 0x10093628–0x1009365a pops reuse head or normal-success allocates 16 bytes;
fresh initializer writes vtable, null next and category 0 only. Packed byte,
timestamp, saved ID, goods and byte 11 are not initialized there. No allocator
failure policy, destructor or callback closure is modeled.
D: 0x1009366f draws once per candidate attempt; unsigned modulo current candidate
count at 0x10093677. BEFORE lookup: goods+0xe=0xff at 0x10093684; position low 6 bits
replaced while packed high 2 bits retained at 0x1009367b–0x100936b2; decrement count
and copy last candidate over selected at 0x100936c5–0x100936d1. Lookup0x1007c630
returns first category 0-position match if chunk category bit0 enabled; nonnull
alone retries at 0x100936e4–0x100936e6, regardless of record occupancy. The old
sweep note's phrase 'If occupied' means an existing record here, not mask 0x40.
D: 0x100936ec–0x10093725 prepends successful reserved record, sets flag 0, clears
packed 0x40 and writes low8(time/1000) timestamp. ID/high bit/byte 11 remain untouched.
D: 0x1009372b–0x10093761 discards leftover reuse and clears only chunk flag 0 if
count 0; no trim of excess retained. Stop before 0x10093767 figure materialization.

## Complete authored main fixture (before implementation)

The C0 list/field expectations precede implementation. The table also expands
N's uninterpreted supplied test fields; no native category-3 layout is inferred.

| Label | Local token | Category | Position | Occupied | High bit | Time | Saved ID | Goods | Byte 11 |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| A | 10 | 0 | 2 | false | true | 11 | 101 | 7 | 9 |
| N | 90 | 3 | unknown | unknown | true | 33 | unknown | 999 | 1 |
| K | 20 | 0 | 9 | false | false | 12 | 202 | 8 | unknown |
| B | 30 | 0 | 4 | false | true | 13 | 303 | 9 | 204 |
| O | 40 | 0 | 18 | true | false | 14 | 404 | 10 | 77 |

A/B habitat false, K true, O false; candidate eligibility first 14 row-major cells
true, final 2 false. Initial flags 0x8101, time 259000 ->timestamp 3, fresh tokens [50].
Retained 2, desired 5; survivors [N,K,O], reuse [B,A]. Candidate packed positions:
[9,11,13,15,25,27,29,31,41,43,45,47,57,59]. Draws [0,0,1,0,999]:
- B draw 0%14 picks 9, collision K; candidate head becomes 59; B keeps time 13/ID 303.
- Same B draw 0%13 picks 59, inserts time 3/goods 255/high bit true/ID 303/byte 11 = 204.
- A draw 1%12 picks 11, inserts time 3/goods 255/high bit true/ID 101/byte 11 = 9.
- Fresh token 50 draw 0%11 picks 57, inserts time 3/goods 255/occupied false;
  saved ID/high bit/byte 11 remain unknown.
Final list [fresh 50,A,B,N,K,O]; flags 0x8101; count 5; draws 4; unused suffix [999].
Remaining candidates [45,47,13,15,25,27,29,31,41,43]; reuse/discard empty.
With only first draw supplied, diagnostic retains chunk [N,K,O], reuse [A],
reserved B (position 9,goods 255,time 13,ID 303), 13 candidates and consumed 1;
no cleanup. A later draw-exhaustion boundary after both reused insertions reserves
fresh token 50 with all data fields unknown and zero further draw consumption.

## QUESTION C0 — candidate-exhaustion proof scope

With consistent per-cell habitat and complete category 0 records, every candidate
collision has a retained/counting record. Desired density<=eligible candidate count;
positive deficit=desired-retained therefore cannot exceed available noncollision
cells. Under that invariant an OQ-572 guard is defensive but unreachable through
the supported full chunk adapter. Duplicates only increase retained count further.
Do not invent inconsistent lookup lists or a density override to force that path.
Proposed disposition: retain a defensive named noncommittable OQ-572 guard, prove
it using the existing lower-level replenish contract, and explicitly document
unreachability for this consistent normal-success chunk profile. Await orchestrator
clarification before claiming C4's chunk-level exhaustion case established.

## C0 disposition (2026-11-19)

The orchestrator's independent focused review APPROVED the consistent-profile
reachability argument at dispatch 077b2cf41c2746bb2f175ca515c9fd9191f44087:
[C0 review](https://github.com/Dom-cmyk727/anno_rebuild/blob/077b2cf41c2746bb2f175ca515c9fd9191f44087/docs/orchestration/reviews/2026-11-19-wildlife-chunk-plan-c0-review.md).
C <= R and D(N) <= N imply positive D-R <= N-C. New insertions use positions
already removed from candidates; duplicates increase R and absent category flags
only suppress collisions. No full-adapter early-exhaustion branch coverage is
claimed; no whole-game OQ-572 closure. Keep the defensive typed error and existing
lower-level proof, plus all 17 density values and minimal legal N 4/R 1 collision.
Reachable draw exhaustion still exercises complete partial-state diagnostics.
The C0 question above remains historical; this disposition resolves its test scope.

## Implementation profile and complexity

`prepare_wildlife_chunk` receives explicit local 8×8 supplied habitat outcomes,
not full center/neighbour facts, and therefore does not execute wildlife_habitat.
It calls unchanged wildlife_density. Existing wildlife_replenish cannot expose
pre-insertion record writes or partial candidate/reservation state; the composer
owns that small stepful loop without changing the accepted helper API/behavior.
Rejected records collect in traversal order and reverse once, equivalent to
head-prepend with linear work. Successful insertion uses at most the original
local density target, never trims retained excess. No unbounded quadratic reject
list construction or engine entity cap is introduced.

Optional local position/occupancy/high bit and saved fields preserve knownness.
Initial category 0 requires position in 0..63 and known occupancy; occupied records
need no retention habitat fact. Unoccupied queries and all 16 candidate cells
must have explicit outcomes, even known false. Unrelated category fields are
preserved without interpreting them as a spawn. Habitat admission is checked
before phase effects, an adapter domain check, not original query timing/rollback.
Fresh local tokens are caller-supplied successful allocation facts, distinct from
all input tokens; insufficient facts return InvalidFacts at reservation, never
an invented native allocation failure. Each fresh reservation has unknown data
fields until an evidenced write. Bit 0x80 remains unknown after clearing 0x40.
Returned attempt records are after goods/position writes and BEFORE time/occupancy
writes; full failures expose partial stage/list/reuse/reserved/candidates/counts.
Only Result success represents the complete pre-materialization plan. No actual
native destructor, registration, World state or global draw history is executed.
