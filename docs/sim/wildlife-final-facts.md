# Original final tile/type facts for wildlife planning

WILDLIFE-FINAL-FACTS-1, 2026-11-19. F0 contract before code.
D = addressed GOG read; P = independent parsed original data; H = authored rule;
C = executed check; U = unresolved. Review pending orchestrator.
[Ground source profile](wildlife-ground-facts.md), [completed first call](wildlife-first-call.md),
[chunk maintenance](wildlife-chunk-plan.md), [ordered selector](wildlife-materialize-plan.md),
[source catalogue](wildlife-species-load.md), [sweep](../re/wildlife.md).

## Fresh descriptor source rule

D: reuse fresh catalogue closure in ground-facts: Game 0x10001df4–0x10001e49,
factory 0x100063a0–0x1000644d kind 0x700, constructor 0x1000c240–0x1000c28f,
base 0x1000b4c0, table 0x10021a88. It installs actual kind getter +0x5c
0x10007d20 (u16 +0x10), and output getter +0xac 0x1000a410–0x1000a424.
The latter masks the requested index to a byte and zero-extends byte
this+0x206+index. The sweep passes index 0 at Welt 0x100937fd; no other index
is exposed here. This is NOT property 0x2e/HEAD[0x19], nor a runtime class.

D: base constructor zeroes kind +0x10 at 0x1000b500 and the TWO output bytes at
0x1000b61e–0x1000b624 (EBX zero at 0x1000b4dc). The derived constructor does
not replace them. WARE arm 0x1000a12a–0x1000a205 reads supplied payload to local
scratch +0x124, iterates NUL strings, queries goods names through game slot +0x48
property 0x20 at 0x1000a18e–0x1000a190, compares ordinals 0..142 in first-match
order and writes matched byte at +0x206+output-index (0x1000a1d8–0x1000a1dc).
It advances output index and stops at 2 (0x1000a1f3–0x1000a1ff). Absent or
present-empty WARE leaves output 0 at 0. A failed name match retains its previous
byte; this binder conservatively refuses unknown names instead of declaring them
successful default provenance. Repeated source entries/records also refuse as
source admission, though the original reuses entries and overwrites fields.
Goods-name getter/table proof is reused from species-load; no new table guessed.

The original input copy is unbounded. This safe profile admits at most 0x440
bytes, the scratch-to-protected-frame interval +0x124..+0x564 (prologue
0x10009240–0x1000926d, restore 0x1000a3a7); required consumed strings must terminate
inside the supplied payload. This is a defensive original-stack boundary, not an
engine goods-name/content cap or a claim the original rejects oversized records.
PRODTYPE admission also bounds its input at 0x50 bytes: input +0x34 must end
before output query header +0x84 (0x1000974f–0x1000977e). The inherited binder
already checks source name termination and exact kind/registry identity.

## Input/default/unsupported table

| Field / boundary | Accepted source | Default / refusal |
|---|---|---|
| Concrete factory | explicit GogAnlagen profile | substituted getters/layouts unsupported |
| Whole word | widened actual object word, sorted/distinct | never mask 14 bits or substitute growing successor |
| Null | word 0 absent from BOTH entries and registry, fresh profile | absent nonzero metadata refuses, never Null |
| Actual kind/HEAD | existing ground binder validates unique HEAD/PRODTYPE and normalized registry pair | same stricter source admission; absent kind is not guessed from modern class |
| Output 0 | first source WARE token via original Game names | absent/present-empty/explicit UNUSED gives proved 0; unknown refuses |
| Registry pair | exact kind/HEAD plus normalized first two output goods | mismatch refuses; modern output vector is a cross-check, never the getter oracle |
| Needed source | only distinct words in requested chunk | unrelated entries not globally validated; duplicate needed definitions refuse |
| Prepared view | complete first-call result's FINAL resource TileLayer | immutable input-stage contract, not World/proven full original load pipeline |
| Chunk address | local chunk x/y relative to aligned island origin, original edge 8 | require whole 8 x 8 in bounds, matching terrain columns; no clipping or neighboring substitution |
| Cave facts | caller-supplied count/provenance only | missing remains unresolved; known zero reaches OQ-574 |

All 64 cells of the requested complete chunk are admitted before materialization.
This is a separate bounded source-admission operation, not an original sweep
predicate or a duplicate eligibility implementation. Metadata for an in-chunk tile
can therefore refuse admission even if a record there would later skip. Unrelated
chunks/types never do. The existing ordered planner itself stays unchanged and
continues reached-field validation. The caller can reuse the owned admitted facts.

## Hand-derived acceptance before code

Authored source: 104 ROHSTWACHS output UNUSED, 105 ROHSTOFF output BAUM (70);
16384 ROHSTOFF output 0; 16488 ROHSTOFF output BAUM; 90001 TIERHOEHLE output 0.
Set HEAD[0x19] to a DIFFERENT value and output slot 1 to a different goods value.
Absent word 0 is explicit Null; Present zero type is a separate companion.
Prepared layer origin (24,40), size 24 x 16, chunk (1,1): world base (32,48),
row stride 24. Place actual 104 at local position 9, 105 at 10, 90001 at 63.
Earlier raw Terrain GFX values deliberately differ. Neighbor cells/types differ
so swapped axes, wrong stride/origin/earlier-layer reads are observable.
Authored records [category skip, actual 104, actual 105, cave] and raw [6,33,999]
give no selection for growing 104, ordinary consumes 2 at actual 105, then missing
cave facts stop with preserved supported prefix/suffix 999, unchanged records.
Explicit supplied cave count 0 reaches OQ-574; no provider is invented.

| Requirement | Positive proof / distinguishing wrong control | Review |
|---|---|---|
| Output source/default | absent/empty/UNUSED/known values, slot 1 and HEAD byte differ | pending |
| Whole actual type | 104 vs 105; 104 vs 16488; zero vs 16384; data-defined 90001 | pending |
| Admission | missing nonzero, duplicate, kind/HEAD/output mismatch after legal control | pending |
| Correct cell | two types plus position 63, nonzero chunk/island origins, non-square stride | pending |
| Composition | ordinary exact choice/count; explicit unresolved/zero cave; unchanged inputs | pending |
| Lifetime/order | destroy source owners, sorted distinct facts, identical repeat | pending |
| Source proof | independent private original-cell/type field oracle plus synthetic missing branch | pending |
| Causal proof | actual wrong substitution/output/cell/null variants after legal control | pending |

No World/state/Rules/tag/default/identity/PRNG/native creation/navigation/FP/graphics
change. Only selector draws are counted, never omitted native callbacks. No live
fauna or complete-sweep claim. At F0 checkpoint C++/new gates are NOT_RUN.

## Implemented interface and stopping boundary

`bind_wildlife_final_types(profile, words, entries, game, registry)` returns owned
actual type facts, explicit null words, entry hashes, WARE presence and Game hash.
It reuses ground-facts ONLY for fresh descriptor kind/HEAD/null admission, discards
its property value and independently decodes the source output bytes. Registry
output vectors cross-check the first two source slots after trailing zero removal;
this normalization never substitutes for the actual getter. Unknown WARE names
are a defensive refusal even though original fresh unmatched slots retain zero.

`bind_wildlife_final_chunk(first_call, chunk_x, chunk_y, profile, entries, game,
registry)` reads final post-fill resource objects, binds distinct words once and
returns all 64 positioned tile outcomes plus copied source provenance. Chunk
coordinates are relative to island origin; row stride is the full island width.
It admits a whole in-bounds original chunk, not partial edge clipping. Terrain
columns establish consistent geometry only; Terrain::gfx is NEVER the object layer.
The public completed result is an input-stage contract, not an unforgeable seal.

Compose by passing `result.facts` unchanged to `prepare_wildlife_materialization`,
with the existing species source/time/draw inputs. Add only explicitly supplied
cave counts when available. This package creates no new planner, eligibility
implementation or provider; source admission and original query order remain
separate. Owned results outlive entries/Game/registry/prepared view owners.

P: bounded scene Tutorial/01Discover01, chunk (10,2), world origin (104,40).
Private Python independently reconstructs GELAENDE footprints and empty-cell fill
from raw HEIGHT/FRUCHTBAR, then decodes actual type kind/output 0 through the Game
name tables. The complete 64-cell field oracle SHA256 is
b6533b5d020db1d48d0c9bb41b1a84e319d5f9031fd061806e50cb74da9a29ed.
Authored records at positions 0/2/63 and draws [6,33,999] give nonqualifying,
selected, nonqualifying, selector consumed 2 and suffix [999]. Time and draw
history are supplied, not recovered from original runtime. This scene contains no
cave: original type 1542 metadata is used in an explicitly synthetic-tile companion
whose missing entrance refuses; no genuine provider result is claimed.

Independent review and frozen gate receipts are in the
[package evidence](../orchestration/evidence/wildlife-final-facts-1.md).
Actual habitat/maintenance inputs, cave provider/coordinates, manager creation,
mutable native callbacks and complete runtime draw ordering remain future seams.
No OQ is closed by binding immutable fields.
