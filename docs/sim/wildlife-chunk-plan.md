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
| Reused fields | B/A preserve ID303/101, high bit and byte11; goods becomes255 | clearing ID/high bit differs |
| Fresh fields | fresh token50, saved ID/high bit/byte11 unknown, time known only on insertion | fresh ID0 is not evidence |
| Candidate order | row-major odd samples, swap-pop with last | erase/shift gives different second choice |
| Excess retained | retain all, no draws or trimming | density is target minimum maintenance, not cap |
| Cleanup | discard unused reuse only on success; clear flag0 iff final count0 | failure retains reuse/reservation diagnostics |
| Exhaustion | exact consumed count, distinct error, no modulo0 | reachability under consistent chunk profile requires disposition below |
| Ownership | immutable inputs/draw suffix, complete owned output and repeat determinism | no borrowed record pointers |

## Primary field/order closure before code

D: AnnoWelt 0x100934f3–0x10093578 initializes a dword retained counter,
caches next before mutation, increments for category0 then skips habitat if
packed byte +8 mask0x40 set. Rejected unoccupied records unlink via0x1007bd00
and prepend reuse via0x1007bd50; counter decrements. Other categories untouched.
The desired byte is zero-extended at0x100935f5–0x100935f7; subtraction uses
32-bit dword count at0x10093604–0x10093618 and signed jle. Adapter uses a widened
counter but refuses counts outside INT32_MAX before arithmetic, not an engine cap.

D: 0x1009357a–0x100935de scans y1,3,5,7 outside x1,3,5,7 and appends passing
coordinates. These 16 original sample positions are algorithm geometry only.
Supplied habitat outcomes are explicit; this API has no full tile/type/neighbour
facts with which to execute wildlife_habitat. Reuse the accepted density function.

D: 0x10093628–0x1009365a pops reuse head or normal-success allocates16 bytes;
fresh initializer writes vtable, null next and category0 only. Packed byte,
timestamp, saved ID, goods and byte11 are not initialized there. No allocator
failure policy, destructor or callback closure is modeled.
D: 0x1009366f draws once per candidate attempt; unsigned modulo current candidate
count at0x10093677. BEFORE lookup: goods+0xe=0xff at0x10093684; position low6 bits
replaced while packed high2 bits retained at0x1009367b–0x100936b2; decrement count
and copy last candidate over selected at0x100936c5–0x100936d1. Lookup0x1007c630
returns first category0-position match if chunk category bit0 enabled; nonnull
alone retries at0x100936e4–0x100936e6, regardless of record occupancy. The old
sweep note's phrase 'If occupied' means an existing record here, not mask0x40.
D: 0x100936ec–0x10093725 prepends successful reserved record, sets flag0, clears
packed0x40 and writes low8(time/1000) timestamp. ID/high bit/byte11 remain untouched.
D: 0x1009372b–0x10093761 discards leftover reuse and clears only chunk flag0 if
count0; no trim of excess retained. Stop before0x10093767 figure materialization.

## Complete authored main fixture (before implementation)

Record notation: token/category/local packed position/occupied/high bit/time/ID/goods/byte11.
Input order A10(cat0,pos2,occfalse,hightrue,time11,id101,goods7,byte11=9),
N90(cat3,uninspected fields), K20(cat0,pos9,occfalse,highfalse,time12,id202,goods8),
B30(cat0,pos4,occfalse,hightrue,time13,id303,goods9,byte11=204),
O40(cat0,pos18,occtrue,highfalse,time14,id404,goods10,byte11=77).
A/B habitat false, K true, O false; candidate eligibility first14 row-major cells
true, final2 false. Initial flags0x8101, time259000 ->timestamp3, fresh tokens[50].
Retained2, desired5; survivors[N90,K20,O40], reuse[B30,A10]. Candidate packed values:
[9,11,13,15,25,27,29,31,41,43,45,47,57,59]. Draws[0,0,1,0,999]:
- B30 draw0%14 picks9, collision K20; candidates head becomes59; B keeps time13/id303.
- same B30 draw0%13 picks59, inserts with time3/goods255/hightrue/id303/byte11=204.
- A10 draw1%12 picks11, inserts with time3/goods255/hightrue/id101/byte11=9.
- fresh50 draw0%11 picks57, inserts with time3/goods255/occfalse; ID/high bit/byte11 unknown.
Final list [fresh50,A10,B30,N90,K20,O40]; flags0x8101; count5; draws4; unused999.
Remaining candidates [45,47,13,15,25,27,29,31,41,43]; reuse/discard empty.
With only first draw supplied, diagnostic retains chunk[N90,K20,O40], reuse[A10],
reserved B30(pos9,goods255,time13,id303),13 candidates and consumed1; no cleanup.

## QUESTION C0 — candidate-exhaustion proof scope

With consistent per-cell habitat and complete category0 records, every candidate
collision has a retained/counting record. Desired density<=eligible candidate count;
positive deficit=desired-retained therefore cannot exceed available noncollision
cells. Under that invariant an OQ-572 guard is defensive but unreachable through
the supported full chunk adapter. Duplicates only increase retained count further.
Do not invent inconsistent lookup lists or a density override to force that path.
Proposed disposition: retain a defensive named noncommittable OQ-572 guard, prove
it using the existing lower-level replenish contract, and explicitly document
unreachability for this consistent normal-success chunk profile. Await orchestrator
clarification before claiming C4's chunk-level exhaustion case established.
