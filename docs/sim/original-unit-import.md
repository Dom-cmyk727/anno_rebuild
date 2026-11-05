# MAP-UNIT-IMPORT-1 — contract and acceptance ledger

Status: A contract independently APPROVED; B validated import/focused proof begins.
No production implementation or real canary acceptance is claimed at this checkpoint.
Owner direction: 2026-11-05 review follow-up in this chat. Continue the owned
MAP-BOOTSTRAP-1 checkout; branch task/MAP-UNIT-IMPORT-1 from clean
066ef4bb363f3fbcdd7615dee290ac8db5cc5a38, preserving task/MAP-SCRIPT-1 and its
reviewed money/wave/startup/persistence evidence. No merge/push/TASKS/asset edits.

## Scope and stopping criteria

A. Establish the complete unchanged endless01 starting-state prerequisites and
primary import contract before production code or expensive gates.
B. Import actors through the normal scenario adapter into the existing figure,
ship, cargo and parent models; no map-specific generated loadout and no silently
omitted unsupported actor. Validate malformed input and deterministic identity.
C. Check raw→spec→World, duplicate IDs, graph/holds/state/health, native cuts and
replay, with independent package review and separating negative controls.
D. Resume SCRIPT2 F2: original positions/resources, public Move/Found/Build only,
real area signal/event and construction consequence, native/fresh replay.
E. Focused then affected gates and fresh interaction review; declare a justified
long-case timeout before execution. Preserve earlier1800s ASAN failure verbatim.
No unrelated matrix, combat/land mover, new disembark command, script-handler
rewrite or original-asset changes are included. Unknown required semantics block
only their dependent claim; continue independent authorized work and record it.

## Starting inventory and erratum

Read-only scan of all50 original Scenes/*.sze, exact tagged nesting:3473 actors,
2644 top-level SCHIFF/SOLDAT,829 nested actors; no duplicate raw ID within a scene.
Discovery artifact cache/unit-import1/A/raw-inventory.json retains source revision,
file/record hashes, record shapes and original values. It is not an acceptance
canary or an assertion that all records already have implemented semantics.

Unchanged endless01 file SHA256
b289be7fe906264b1d0fe0b2be75c0e68813ed6691128c84cfebb6e4fd4cc8c0 has exactly:

| File/preorder ordinal | Raw ID | Kind | Enclosing owner | Parent ordinal | Crew slot |
|---:|---:|---:|---:|---:|---:|
|0|0|1503|0|none|none|
|1|1|717|0|0|0|
|2|2|718|0|1|none|
|3|3|717|0|0|1|
|4|4|718|0|3|none|

The ship starts at351.5,383.5, prefix facing4, prefix flags0; eight WAREN entries
are present. Cargo/state/identity projection and strict input policy are specified below;
production remains blocked until independent contract approval. Human has no original city; an imported ship is
necessary for founding. The only existing unload command transfers goods, not
crew: soldier-money branches cannot be assumed reachable via public commands.

**Correction to historical SCRIPT2 handoff:** it called the crew records IDs0/1
in slots1/3. The6-byte ENTRY layout documented in annoscene §8 and its writer
1004c688/1004c68d instead gives IDs1/3 and slots0/1. The nested ESEL IDs are2/4.
The missing-actor diagnosis remains valid. This correction changes no reviewed
money/wave/query rule or original asset; the raw boundary audit remains historical.

## Acceptance table (before production changes)

| Requirement | Primary evidence | Positive proof / hand-derived result | Distinguishing negative | Evidence | Review disposition |
|---|---|---|---|---|---|
| Complete actor closure | unchanged scene bytes; recursive tagged records |5 actors with table above;8 ship cargo records |only top-level import; ID/slot swap; donkey attached to ship |raw-inventory.json + raw→World test planned |pending independent derivation|
| Ownership/ordering/IDs | Welt1001dc7a/1001df31 and1004c65f; enclosing-player caller |creation traversal, raw-ID slots/hint and parent links; source0 maps to a serial engine ID starting1; invalid FigureId is UINT64_MAX; Slot/kNoSlot sentinel is UINT32_MAX |prefix-owner wins; grouped ship/soldier reordering; duplicate raw IDs |primary reader/caller trace pending |pending|
| Coordinates/facing | Welt1001ddad/1001e01d; property0x16; existing Q8/nav model |ship351.5,383.5 anddir4; exact checked conversion |x/y swap; ship4→soldier8; nonfinite/wrap; displaced held actor |conversion/parent trace pending |pending|
| State/health | ship HEAD loader10042a40 and attach10041900; soldier HEAD reader pending |source health/state and held bits projected, no invented full-health grant |HEAD bytes treated as direct state; wrong128 scale; HP/crew conflation |primary trace and literal vectors pending |pending|
| Holds | WAREN reader/caller pending; existing ShipHold amount_88 |8 ordered entries; repeated goods retained, source amount exact |sort/merge; whole-ton/raw8.8 confusion; truncated final record |primary trace and literal vectors pending |pending|
| Normal validated import | scenario_formats player state loop and FigureSpec/World::create |populated models before INIT/commands; unchanged source/resources |test-only actors; skip malformed/unknown required actor; partial success |focused adapter tests planned |pending|
| Native/replay | existing native130/frame3 field list/session contracts |same authoritative fields/cuts/IDs/event sequence |lost parent/health/cargo or duplicate Startup |affected native/replay proof planned |pending|
| Real SCRIPT2 canary | entire original source and public command APIs |real ship area→signal/wave; ordinary legal construction |teleport/resource grant/extracted source/query-only build proof |starting prerequisites first; real canary planned |OPEN pending import and usable gameplay path|
| Evidence/gates | permanent evidence contract + canonical runner |frozen identities, exact counts and scoped reuse |timeout promoted pass; unrelated repeats |raw gate files/reviews planned |pending|

## A1 — completed proposed import contract

Primary derivations are independently checked in the existing A review; raw dumps
stay ignored under cache/unit-import1/A and /private/tmp/unit-import-*-review.
The existing source walkthrough's `file order` means creation traversal, not
registry-slot order. No C++ behavior is authorized by an unresolved draft claim.

### Slots, allocation hint and engine identity

World MAIN is zero-filled to16 bytes by Welt100ae2c9..100ae2d7. Its u32 at+4 is
passed to registry resize100980e0 at100ae31e..100ae350 when nonzero; zero keeps the
constructor size. The resize preserves hint/cursor. Constructor10098cd6/10098d06
starts at1 slot, hint−1,cursor0. Existing configured initial-size overrides retain
that established engine configurability; default1 matches the original.

Create actors in original record traversal: player blocks in file order, each
SCHIFF/SOLDAT where encountered, then its required nested actors depth-first in
record order. The factory uses each rawu16 ID as its requested registry slot
(Welt10097347..1009739a). Preserve holes and the resize/growth rule, including
min-growth from the existing configurable figure_min_growth, default100.
Original explicit-slot growth1009734b..1009736b uses the requested raw slot,
not the current length: rawslot+trunc(rawslot/4), raised to default100. Reuse
SlotList::grown_size(rawslot, figure_min_growth), whose existing rawslot+1 floor
also makes configured small-growth domains safe. With currentlength100 and
requestedslot100 the expected length is125, not101; requestedslot9 into1 gives100.
Each successful explicit insertion sets hint to that raw slot; cursor stays0.
Do not sort creation by ID:39 scenes have nonmonotone traversal,44 have holes,
and sorting changes the next allocation in17. Example easy_256 source finalhint3
leads to nextfree4; sorted finalhint9 gives10. A rawchildID may be lower than its
parent (36 examples in5 scenes); source preorder still gives earlier parent indices.

Raw IDs have one scenario-wide namespace across top-level units, CREW and ESEL.
Duplicate raw IDs anywhere refuse before a World is created. Use existing stable
engine FigureId serial allocation in creation order (1..N), separate from slots;
raw0 is a valid original slot, not an invalid ID. The invalid FigureId sentinel is
UINT64_MAX (FigureId has64-bit representation); Slot/kNoSlot is32-bit with
UINT32_MAX sentinel. FigureId0 passes Id::valid() but live-serial registry
validation excludes it; model-created FigureIds start1. Record the rawslot→specindex mapping
while decoding; parent indices resolve through that map/tree, never treat a rawID
as an engine FigureId. Existing native FigureRegistry slots/IDs/parents/hint/cursor
already serialize this entire result; no new native field is needed.

Proposed API: optional original registry initial-count on ScenarioSpec and an
optional requested registry slot on FigureSpec. Existing authored/generated
specs keep their defaults and existing serial/free-slot behavior. Import creates
one figure per inventoried actor; no unknown actor is omitted. An explicit-slot
registry insert reuses the existing indexes and rejects occupied/out-of-domain
slots before mutation. Earlier-parent validation stays valid for source preorder.

### Immutable import facts and existing state

Original LOGIC provides generic maximum HP and hold/crew counts that the live
FigureType model stores only for navigating ships. Retain the required facts in
a separately named, immutable registry-owned import metadata map, derived once
from original figure entries; synthetic builders can declare equivalent facts.
Do not add un-hashed members to FigureType or weaken its all-members-digested rule.
Required absent facts cause a named import refusal.

The metadata is used only at the original→ScenarioSpec boundary, never by a
running or natively reloaded World. Imported hp/state/holds/nav/slot consequences
all reside in existing hashed fields. Consequently this import-only map is
excluded from the existing live-registry digest; changing it changes imported
initial-state identity, not an already-created world's behavior or native reload.
Test that boundary explicitly. Any future runtime consumer would require a
content-digest/policy review first. Native130/frame3 and existing synthetic
recordings remain applicable; actor-imported initial hashes are naturally new.

Use existing Figure.hp for nonship decoded HP, supplied by a new default0
FigureSpec input. Ships retain the existing NavMotion.health field and FigureSpec
health input; their unrelated generic Figure.hp stays its existing0 default.
No new state field, PRNG draw or generic world mutation seam is introduced.

### Supported actor classes and refusal

This dependency closure supports data-defined kinds with original factory class53
(donkey),97 (scout), and113 (ship, requiring supported navigation facts). The
factory map1009a7e8/1009a774 dispatches97 to100374d0/vtable100c3758,53 to10037e70/
vtable100c3a30,113 to the ship constructor;114..128 take its default/no-object
path. The area query's broader ship-class band is a different contract. This is
not a list of special figure IDs or a filename-specific compiler/load path.

Class support alone is insufficient: permit exactly these traced role pairings:
SPIELER/SCHIFF→class113, SPIELER/SOLDAT→class97, ship PRIVAT/CREW/ENTRY→class97,
and a held scout's PRIVAT/ESEL→class53. All other record/class/parent combinations
refuse before creating a playable World, including ships inside SOLDAT/CREW and
class97 inside ESEL. This finite role graph has at most3 actor levels (ship,
scout,donkey). Do not generically recurse through untrusted nested tags. Decoder
calls may follow only this finite role graph, or use an explicit bounded work
stack; unsupported deeper nesting refuses before another actor-decoder call.
The65536 actor budget is not permission for65536 C++ call-stack frames.

Any other actor class, unknown required nested actor or unsupported required
state is an explicit path/offset/type refusal, never a discarded actor with a
successful unchanged-world claim. Previously supported geometry/component
fixtures may explicitly retain their historical actor-free projection, labelled
as such; that must not be the production/default loader or real canary.

Validate complete record framing, duplicates, one supported HEAD per actor,
WAREN stride/slot counts and crew-slot uniqueness, type/goods/owner presence and
acyclic same-owner parent relationships before World creation. Short/trailing/
extra/unknown required records refuse under an explicit modern strict-input
policy; the original often floors lengths/skips errors. Known unported non-actor
fields receive precise typed warnings rather than invented behavior: ship stance
HEAD0 affects deferred hostile avoidance (not presentation); NAME/VERK and auto
route/task effects must retain their documented limits. Nonempty automatic routes
or tasks that require unimplemented behavior refuse; do not claim them executed.

Use configurable import allocation limits outside SimConfig/native
state: default65536 actor/slot safety budget (source IDs are16-bit), with existing
kListSizeBound as hard container ceiling and a documented raise path. Check MAIN
and required growth before allocating; malformed huge MAIN cannot request a
multi-gigabyte Figure array. The widened engine's authored/generated specs keep
their current domains. The import limit is a safety policy, not an original cap.

### Health, state and cargo

Ship HEAD handler10042a40, dispatched by common reader100318f7 slot+0x140:
byte9==0→type maximum; otherwise trunc(maxHP*unsignedByte9/128). Original has no
upper clamp; reject a computed value above the supported nav maximum, rather than
silently clamp it. Literal vectors max12000: byte128→12000,64→6000,1→93,0→12000;
byte129→12093 is an explicit unsupported-domain refusal. Types supply the max.

Scout/donkey HEAD handler100498e0, reached by their independently verified factory
vtables: signedi16 at+8<=0→type maxHP; positive values clamp to max. Max350 with
175→175,0/−1→350,351→350. Endless scouts350 and donkeys400 follow their own type
maxima; ship-ratio decoding here is wrong. Generic HEAD byte1's sign sets/clears
state0x20 (nonnegative sets it). Ship HEAD10 bit0 controls0x100. Ship N2/qualifying-non97-crew
logic controls0x8000; do not copy HEAD bytes as a direct state word.

Base constructor10035ce0 initializes state8 and facing0; its context-pointer
zeroing is not a position initializer (walker coordinates are at+24/+28/+2c). Top-level soldier caller
initial segment through10035260 clears8. Ship attach10041959..1004196c uses
(flags&0xAE)|0x80 and stop(4,1), preserving constructor8; held donkey creation
10036d34..10036d47 takes (scoutFlags&0xAF)|0x80. Owner binding and generic HEAD do
not clear8. Source HEAD1 is negative, so exact immediate loaded state is:
ship0x8000, scouts/donkeys0x88. No imaginary shipstate0 or heldstate0x80 normalization.

Common WAREN reader10031902..100319d0 takes filegoods from lowbyte0, maps through
WARETYPE names (reuse existing goods_remap), takes unsignedu16 tons at+2, shifts
by8 and clamps to the figure type's capacity_88. It writes ordered hold indices;
repeated goods remain distinct. Byte1/tail are not a wider goods ID. Synthetic
vector filegoods41→customGoods90001, tons3,capacity2560→(90001,768), even with
nonzero ignored bytes. Tons5/capacity512→512. Endless01 ship holds are:
41,41,26,42,42,41,43,26, each12800raw=50t, total400t. The two scouts each have two
empty cargo records. Preserve empty slots and full hold order, not a goods sum.

### Positions and facing

Top-level prefix x/y are originalf32 worldtile bits (origin0 in normal scenario
load); reject nonfinite/out-of-world values before conversion. Ship prefix4 is
alreadydir16=4; top-level scout facing uses8directions and converts to existing
dir16 by multiplying by2. Nested records contain no facing, retaining constructor0.
Ship CREW reader10042f88 passes the ship position to each scout before its common
record load. ESEL held reader10036ca0 creates with nullposition, ignores prefix
x/y and uses the parent link. The walker constructor's+14/+18/+1c zeros are type/
world/island context, not position; neither this constructor chain nor the held
reader initializes ownxyz at+24/+28/+2c. Therefore no definite original stored
point0 may be claimed. Source scout positions equal ship351.5,383.5. For a held
donkey, normalize the modern nonspatial cached position to its parent's known
position, using the established parent/carrier model. This explicit normalization
represents effective carried location; it is not an original initialized-xyz fact
or a relocation of a spatial actor. Parent links and existing query parent
exclusion remain authoritative; test that this cached child coordinate never
becomes a spatial area candidate and movement/native reload preserve the hierarchy. Unheld ESEL placement is an explicit unsupported-state refusal until
its separately traced branch is accepted; no held-prefix zero is treated
as a live world spawn point.

Project generic nonnav positions into the established Q8 model by checked
truncation toward zero. For navigating ships retain checked source-derived Q32
position in existing nav.pos_u (Q32×500) as well as its derived Q8 view, using the
existing navigation round-to-nearest-even Q32 policy for sub-Q32 inputs; no
whole-tile rounding or loss of meaningful ship fractions. Signedzero (+0/−0)
canonicalizes to0 before range checks; negative nonzero values refuse. Proposed FigureSpec
input may carry initial Q32 coordinates, validated against its Q8 view/world bounds.
This is input projection into established integer models, not a float in state.
Literal ship351.5/383.5 givesQ8 89984/98176. Distinguish axes, tiny/nonfinite/large
values and a fractional non-Q8 ship point, then native reload retains exact nav
position. Limits and chosen projection are explicit; no original x87 parity claim.

### Early canary prerequisite verdict

Complete actor closure is5, raw IDs0..4, source slots0..4 in100, hint4,cursor0,
serial engine IDs1..5, original ownership0, source position/facing/HP and ordered
cargo as above. The original Human has10000000gold and no city. This is sufficient
to attempt existing sailing/founding/building; it does not prove a route, legal
founding location or build is available. Prove that with a focused normal-path
canary before expensive gates. Do not grant resources or relocate actors.

The existing command variant has Move/Stop/Found/Build and goods Load/Unload,
but no disembark/land-movement command. Therefore real soldier-money branches
remain potentially blocked after import; do not invent such a command to close
the canary. Trigger real ship area/wave behavior and construction with existing
commands where supported, and state exactly which original branches occurred.
All reviewed synthetic money/wave/continuation evidence remains separately valid.

### Contract precision before A review

Prefix owner bytes are ignored in favor of enclosing-player ownership, even in a
synthetic mismatch control. Nonzero top-level prefix activation flags require an
explicit supported rule; this initial closure refuses them (all corpus values0).
MAIN may be absent/short and is zero-filled up to16 like the original; longer
records, duplicate structural definitions and oversized allocation requests refuse.
Empty WAREN entries remain empty even when filegoods0 is not a regular GoodsInfo;
a nonempty unknown/unmapped goods entry refuses. Original load skips unknown
mappings; strict refusal is the declared policy. Zero/empty cargo is not a grant.

Every importer warning identifies actor path/offset and unprojected field. The
projected unit has no synthetic job, home building, resource grant or additional
permissions. Attach/source ownership/parent facts enter the existing model only.
Immediate post-reader flag derivations are static evidence, not an original-runtime
capture or a claim that every later post-load pass was audited. This boundary is
explicit in the primary [RE record](../re/original-unit-import.md).

## A review record

`/root/unit_import_contract_review` APPROVED slot/ownership/traversal/identity/
metadata architecture and input-domain scope at clean6097d30e. Primary inventory,
source-slot rather than file-order semantics, MAIN capacity, allocation hint,
FigureId64/Slot32 distinction, finite role pairings and growth-before-allocation
findings are closed. No B/C code or F2/G acceptance follows from this contract.
State reviewer disposition follows after restoring its separating HEAD flag vectors;
no engine run or production edit has occurred during A.

A closure: `/root/unit_import_state_review` APPROVED state/health/cargo/position
and the import-metadata boundary at clean
`db9b31253916edd805aca1c120165f95fdc8cab0`. All primary findings are fixed:
ID/slot reversal, original slot/hint ordering,64-bit FigureId sentinel, finite role
graph, class113 factory boundary, uninitialized held-ESEL position distinction,
Q32/signedzero vectors and restored independent state-bit controls. The separate
identity reviewer approved6097d30e; subsequent edits restore state vectors and
record dispositions only. B/C implementation, F2, gates and task acceptance remain
unapproved. No engine build/test occurred in A; only static primary and independent
raw inventory/hand derivations. B must establish the early normal-path actor/cargo
prerequisite before an expensive campaign.


## B first implementation checkpoint (not yet accepted)

Normal default loading now attempts the complete supported actor closure through
original_units.cpp. Explicit original registry slots and MAIN allocation shape,
serial FigureIds, generic HP, finer ship Q32 position and immutable import facts
reuse existing native fields. Generated fixed-map/ship-start base reads explicitly
select their already-established actor-free component role; normal files never
fall back to it. Existing full SCRIPT2 handlers were not changed.

First focused gate selects only `UNIT1 original endless01 imported starting
prerequisites`, before movement/build or any broader campaign. It compares file/
program hashes and the five-actor tree, source slots0..4 in100/hint4/cursor0,
source HP/holds and positions, then native round trip. A failure is retained and
corrected before subsequent implementation gates. Two authored structural tests
are also present but not part of this initial single-case selection. No runtime
PASS or B/C approval is claimed before its terminal evidence.

### Reopened/corrected A flag oracle during B review

Instruction reread found a reversed JNE interpretation in earlier A:10042b31
comparesclass97 and10042b34 reachestrue only when NOT equal, following nonzero type
slot+0x9c. Class97 scouts are excluded from satisfying ship0x8000. Attach100419e7
has the same inequality. Current supported CREW admits only97, so only clamped
HEAD8 abstract-crew count can clear8000. Actual HEAD8=0 with two scouts leaves
source shipstate8000, which is outside the movement veto mask200e but still a
persisted authoritative flag.

Affected earlier A8000 approval is withdrawn pending correction review; reviewed
slot/identity, health/cargo/Q32/held88 are unchanged. B-prerequisites-01 atf7c31ed8
retains rawPASS1/1,70assertions, but expected shipstate0 was a wrong oracle and is
not state-fidelity proof. Preserve that log/source; corrected assertions plus
scout-vs-HEAD8 controls and a wrong-qualification mutant must supersede it before
dependent acceptance. No expensive matrix or movement/F2 claim used this oracle.

A second B input-boundary finding: require navigation facts iff the imported
role is Ship. A Builder can legitimately create a class97/53 type with nav for
an authored world; accepting it as an original foot-role would silently install
ship motion/full-health defaults. The importer now refuses that role/type pairing;
a legal foot control and a still-valid authored nav registry distinguish this.

Corrected A8000 contract/code finding is independently closed atbbcb7c53 by
/root/unit_import_state_review. Canonical B-corrected-02 passes5/5 (176assertions),
zero failed/empty/skipped/incomplete/sanitizer, with old runtime/stream contracts
separate. /root/unit_import_contract_review closed the nav-on-foot role finding
at the same pin. Full B/C approval still waits for malformed/vector/mutant proof.

C focused authored vectors now cover HP signed/ratio/overflowdomain, HEAD state
bits, Q32 fractions/ties/signedzero/nonfinite/worldbounds, goods remap90001/cap/
namedzero/unknownempty, held donkey parentcache/index exclusion/native roundtrip,
strict record/role/activation/facing errors, duplicate IDs/slots, missing metadata,
MAIN/actualgrowth/actor allocation budgets and explicit component-only projection.
A new zero-width-world/zero-coordinate counterexample probes a suspected early
canonical-zero bounds bypass. Preserve the first resulting gate, even if this
new assertion exposes it, before correcting the input guard. No broader matrix
or actual movement canary has run at this point.

C-probe-01 at cleanec0d87c7 retainedFAIL:13selected/12executedpasses/1failed,
zero empty/skipped/incomplete/sanitizer. The malformed-input testcase retained
219legal assertions and1fatal acceptance of its final zero-width/zero-coordinate
case. Other12cases, including original prerequisite, passed. The coordinate
helper now rejects empty/over-bound world extents before canonical signedzero;
a dedicated legal512-width vsinvalid0-width control covers both+0/−0. Cargo
width proof now executes a legal byte1=0 control before the distinguishingAB case,
so its intended wrong-width mutant can retain a passing legal region.


### Reopened A import-catalog field during converter review

C-corrected-02 and exact-restoration C-restored-03 each pass14/14 at6a2c6414,
with806assertions and0other outcomes. Crew mutant selected1/failed1 with31legal
assertions/2intendedfailures; goods-width mutant selected1/failed1 with13legal/
1intendedfailure at highbyteAB after the byte1=0 legalcontrol. All mutants restored
byte-identically (original_units.cpp SHA256
16415eb5e9188af855b7773c9a0fa4f265c643c802e3e6be59160c6e84ea2525).

Separate review found production metadata extraction still needed a raw-entry
control: AnnoGame slot+88 mode2 reads+f8 (1000fb60), written at10010352 from AH of
EAX loaded100102f3 at LOGIC+25, so the byte is LOGIC26. LOGIC15 instead feeds+101,
a separate cannon getter (10010274 /1000fcb4). A's adjacent-count field assumption
was incorrect; the affected field approval is withdrawn pending correction.
Actual1503 other-crew capacity6,1502 capacity0,717 capacity0. Existing manual
Builder metadata vectors test configured values, not this raw converter.

A new authored figure-entry case first gives equal15/26 values6/6 as a legal
control, then15/0 to distinguish the fields through TypeRegistry::build and
HEAD8=1 import. Preserve its first failure under the old field mapping before
correcting extraction. Other reviewed health/state/slot/position semantics remain
unchanged; source endless01 HEAD8=0 means the corrected source flag8000 is unaffected.

C-type-catalog-probe-04 at clean32803bba retainedFAIL1selected/1failed,
25passing legal assertions/3intended failures (catalog value, spec flag, World
flag), zero empty/skipped/incomplete/sanitizer. Equal6/6 legal input executed first;
distinguishing15/0 failed at exactly the metadata source/flag consequences. The
extractor now uses44 zero-filled bytes andLOGIC26. Original prerequisite also
asserts1503 counts8/3/6 and717 othercrew0 from the hashed original type input,
so it no longer bypasses converter fidelity with Builder-only declarations.
This is a field correction with no native format/runtime-handler change.

C-converter-corrected-05 at clean d5e98d2f passes15/15,841assertions, zero
failed/empty/skipped/incomplete/sanitizer; reference contracts separate.
/root/unit_import_state_review independently APPROVED inspected B/C at
d5e98d2f3ec99f79e362989e09d02afe41f820ed, closing the withdrawn LOGIC26 A field.
Contract/input reviewer final disposition remains pending. F2/G remain OPEN.

## D early original canary prerequisites (before voyage/integration gates)

| Requirement | Positive proof | Counterexample / negative | Evidence | Review |
|---|---|---|---|---|
| Original ship is usable | unchanged five actors, cargo, INIT, planned water endpoint inside NEGER; accepted founding transaction on planning copy at reachable neutral coast | failed route or affordability blocks dependent voyage; planning never mutates executed world | named route/founding prerequisite case; pending raw gate | pending |
| Real condition and construction | MoveCmd from original position, naturally visited branch and wave; Found/Unload/Build with original stock, exact debits | wrong move/command must diverge; no teleport, grant or script extraction | implementation follows prerequisite and B/C approval | OPEN |
| Continuation/identity | native cuts and encoded fresh-process replay preserve state and event suffix | wrong program/command must refuse/diverge at causal checkpoint | existing wrapper extended only after real path works | OPEN |

The historical SCRIPT2 dependency test is renamed to reflect resolved actor import;
its source/INIT/false-visit assertions remain. Read-only planning scans original
neutral coast geometry and tests transactions on disposable copies only, then
asserts the executed starting state/position are unchanged. A bounded search
refusal is not promoted to proof that no legal site exists. No F2 acceptance yet.
