# Ordered eligibility and materialization selection plans

WILDLIFE-MATERIALIZE-PLAN-1, 2026-11-19. M0 contract before code.
[Chunk](wildlife-chunk-plan.md), [source catalogue](wildlife-species-load.md),
[creation fields](wildlife-create-effects.md), [decisions](wildlife-decisions.md),
[primary sweep/helper](../re/wildlife.md#1-fauna-sweep-0x10093480).
Final implementation review pending orchestrator. No actual animal creation.

## Original branch order

D: AnnoWelt 0x10093767–0x10093886 traverses list order. Category slot 0 is
checked first (0x10093780–0x10093784), then spawn occupancy mask 0x40
(0x10093795–0x1009379a), then timestamp (0x100937a0–0x100937a7).
Time byte is low8(time/1000); SUB CL,timestamp followed by JS skips when the
wrapping byte's high bit is set. Equality is eligible; differences 127/128/255
are eligible/ineligible/ineligible. Use unsigned arithmetic/mask then compare
against 128, never host signed-char conversion or an unwrapped timestamp order.

D: only then decode local position and read the actual full object word
(0x100937ad–0x100937ee). The definitions +0x70 result is tested for null at
0x100937f3–0x100937f5. Null skips; it is not a missing-fact fallback. Whole-object
reader precedent: source word 0 is known null only with source/registry absence,
not an unconditional numeric-id rule. This API takes explicit lookup outcomes.
It does not reuse ground-facts property 0x2e/HEAD[0x19] as output-0 goods.

D: type output-0 slot +0xac(0) is read first at 0x100937fd. Goods 0x46 qualifies;
otherwise type kind +0x5c must equal 0x145 (0x10093803–0x10093814). No id+1
ROHSTWACHS substitution here. Helper 0x10092ca0 queries the SAME actual type's
kind for ordinary/cave dispatch (0x10092cfd onward). Supplied cave count is a
resolved provider outcome, not geometry or proof of original callbacks.

Success writes at 0x1009384f/0x10093859/0x1009385c/0x10093871 require real
manager creation: this packet performs NONE of those writes. Spawn list, saved
ID/goods, occupancy, cooldown, unknown high bit and byte 11 remain unchanged.

## Input/order/effect/error table

| Reached stage | Required input | Result / next step |
|---|---|---|
| Ordered record | local token/category | wrong category skips; no other fields read |
| Category 0 | known occupancy | occupied skips; no timestamp/position/tile/source needed |
| Unoccupied | known timestamp | negative wrapping delta skips; no later facts needed |
| Eligible byte gate | known local position | unique supplied tile lookup at that position |
| Lookup | explicit known Null or Present outcome | Null skips; unknown/missing/duplicate refuses |
| Present | known whole object word, unique actual type fact | output-0 goods required first |
| Goods !=0x46 | known actual production kind | non-0x145 skips; no saved goods/cave/source required |
| Qualifying actual type | actual kind (also needed by helper), saved goods | cave requires unique known entrance count; zero is not missing |
| First selector call | original source bytes/entries/profile/climate | prepare immutable owned catalogue once, preserving single API source error order |
| Each selector | raw remaining span | ordinary/cave and bounded goods search unchanged; exact per-call/cumulative selector counts |
| Unsupported selected creation kind | actual selected plan and count | typed noncommittable stop, not an invented original skip |

Unknown input facts refuse at their reached stage; they do not consume a selector
call. Counts cover calls to the pure selector ONLY, not omitted position/height,
allocation, creation or mutable callback draws. Known zero fields stay distinct
from absent fields. No physical position/provider/manager, World or global cursor.

## Catalogue factoring boundary

Prepare original climate-2 row and generic numeric facts once into an immutable
owned catalogue. Keep all 32 slots and unsupported kinds. Select from those owned
facts without rereading/hashing PE images or source records per record. Only the
actually selected kind 49/51 obtains the sealed New capability. At current source,
generic validation for New kinds covers the same fields/order as the selected
New binder; unsupported kinds are never eagerly New-bound. Existing standalone
single-record API still prepares source before selector errors and returns its
existing unsupported disposition. Its owned ring/slot facts/capability remain owned.

A public selected capability cannot be forged from a mutable generic fact: only
the source-prepared catalogue can construct it, from its private validated copies.
This is owned data, not an authenticity token, runtime registry or global cache.
No extra climate, modern registry mapping or source default policy is introduced.

## Hand-derived sequence before implementation

Authored ring: 32 slots default type 60001/kind 51/goods 137; slot 1 is
127/kind 49/goods 138, slot 31 is 503/kind 50/goods 9. Source duplicates preserved.
Time 0; complete input order [100,90,80,70,60,50]:
- token 100 category 3: skip category, all other record fields unknown.
- token 90 occupied: skip occupied, timestamp/position/goods unknown.
- token 80 unoccupied timestamp 1: wrapping delta 255, skip cooldown; later facts absent.
- token 70 time 0/position 1: actual type 41 goods 7/kind 0xe2 disallows; type 42 goods 70/kind 0 supplied as distinct control.
- token 60 time 0/position 2/saved 255: actual type 10 goods 70/kind 0, ordinary selection.
- token 50 time 0/position 3/saved 137: actual type 11 goods 7/kind 0x145, cave entrances 2.
Raw draws [6,33,31,999] give selections for 60 position 2/index1/type 127/inspected 1,
consumed 2; for 50 position-1/index0/type 60001/inspected 2, consumed 1. Complete reasons
[category,occupied,cooldown,actual-type,selected,selected], per-call counts [0,0,0,0,2,1],
cumulative [0,0,0,0,2,3], unused suffix [999]. No spawn field changes.
Append token 40 cave/saved 255 with draw31: actual unsupported503/index31/inspected 1,
consumed 1/cumulative4 then diagnostic stop, retaining both supported prefix selections.
Truncate ordinary second selection after its first draw: exact partial selector
error consumption retained, never original rollback or successful partial batch.

Time-only controls: now0/stamp0 delta 0 eligible; now0/stamp129 delta 127 eligible;
now0/stamp128 delta 128 and now0/stamp1 delta 255 ineligible; now4/stamp250 delta 10
eligible across wrap. Eligible cases use a known-null lookup to need no catalogue.
No whole-tick scheduling or real-time interpretation is inferred.

## Implemented boundary and review evidence

`sim_load::prepare_wildlife_materialization` returns an owned ordered plan or a
failure with diagnostic-only prefix, failing record/stage and unused raw suffix.
It accepts `WildlifeChunkPlan`, supplied sweep time, immutable final tile/type/cave
facts, a source view and raw selector draws. `WildlifeSpeciesCatalogue` owns the
source-prepared ring, numeric slot facts and selector species; selection seals
only the selected supported New kind. The old single-record entry point delegates
to that catalogue, preserving source-before-selector errors and unsupported results.

Tile/type/cave indexes are local integer-key ordered maps; duplicate facts refuse
only when reached. Record-token uniqueness is an adapter identity check, not a
new original spawn field. Null lookup is explicit even if its source word is not
available; a Present lookup with numeric word 0 still requires actual properties.
A known zero cave count reaches the selector's OQ-574 refusal. A missing count
refuses before source binding. Source errors precede selector errors once reached.

The catalogue is prepared lazily at the first eligible selector call and retained
once for the whole batch. A skipped-only batch needs no source. Facts stay fixed
across the batch; this cannot establish mutable original callback behavior.
A creation capability describes validated fields, not native allocation success.
The unchanged input chunk is retained on success and failure; no saved ID,
occupancy, timestamp, goods or unknown-bit writes are inferred from selection.

[Package evidence](../orchestration/evidence/wildlife-materialize-plan-1.md)
records literal sequences, actual-source mutants, compatibility baseline and
frozen focused gates. Independent review remains the orchestrator's handover.
Actual terrain/cave providers, coordinates, creation/manager callbacks, total
runtime draws, native publication, Windows FP and whole-sweep parity remain open.
