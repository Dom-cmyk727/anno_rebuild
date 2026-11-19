# Imported mixed records through local wildlife planning

WILDLIFE-IMPORT-PLAN-1, 2026-11-19. I0 contract before implementation.
D = addressed GOG read; P = independent raw-file decode; H = authored derivation;
C = executed control; U = unresolved. Independent review pending orchestrator.
[Prefix](wildlife-prefix.md), [BERGWERK](wildlife-bergwerk.md),
[OCCUR](wildlife-occur.md), [first call](wildlife-first-call.md),
[habitat](wildlife-habitat-load.md), [maintenance](wildlife-chunk-plan.md),
[final fields](wildlife-final-facts.md), [selection](wildlife-materialize-plan.md).

## Shared native list and flags

D: HIRSCH 0x10094744–0x10094797 indexes island+0x94 by stride+0x8c,
addresses chunk+0xc and calls prepend 0x1007bd50–0x1007bd60. This function installs
new head and calls node+8 to set its next to OLD head. HIRSCH table 0x100cac04
has category byte +0xa initialized 0 at 0x1009478f, getter 0x100952b0.
Presence bit 1 is ORed at 0x10094797, not inferred from category number.

D: BERGWERK 0x10077f74–0x10077f87 uses SAME chunk+0xc/prepend and ORs 2.
Kind 1 is its +0xb byte (constructor 0x1007c290–0x1007c2cc), not spawn bytes.
OCCUR 0x10095242–0x10095255 uses SAME head/prepend, stores category 6 at +0xa
and ORs 0x40. Resource payload layouts differ; they never become spawn fields.
Existing spawn/resource chains are category projections, NOT two native lists.

The represented-presence mask is therefore 0x43 regardless of which represented
categories are present in this chunk. Derive each bit's value from actual imported
nodes, including known absence. Supply all other flag bits explicitly, with an
engaged optional u16 and provenance string; zero is valid. Reject any overlap
with 0x43 even if a represented category is absent. No unknown native flag is
silently initialized or claimed proved. This is conditional context, not state.

## Field/error/source contract

| Field or stage | Owned meaning / validation | Unsupported or missing |
|---|---|---|
| Prepared source | completed first-call, exact scene/ordinal/slot/aligned geometry | incomplete/mismatched profile refuses |
| Chronology | source header/record and per-entry insertion ordinal | absent/ambiguous/wrong source/duplicate mapping refuses |
| Spawn | one imported PrefixSpawn per HIRSCH record, projected chain checked | broken/cyclic/omitted/duplicate links refuse |
| BERG node | source entry plus established y/x multi-chunk insertion ordinal | footprint/membership/offset/index mismatch refuses |
| Occurrence node | occurrence CREATION source, exactly one kind-6 node | repeated non-creating loads never allocate; corrupt indices refuse |
| Mixed order | sort chronological insertions, then head-prepend order | never concatenate categories or sort positions/types |
| Local token | unique chronological mapping, not World/Figure ID | duplicate mapping/fresh-token collision refuses |
| Category 0 known fields | local packed position, occupied=false, loaded timestamp/goods | saved ID/high bit/byte 11 stay unknown; no load draw replay |
| Category 1/6 fields | category only in spawn snapshot, typed payload sidecar keyed by token | offsets/rotation/mask/ore state never decoded as spawn fields |
| Full flags | derived represented bits OR disjoint supplied non-presence value | disengaged value/empty provenance/overlap refuses, context retained |
| Planning stages | imported -> habitat -> maintenance -> final fields -> selection | owned completed artifacts and child diagnostics, never success/rollback |
| Draws | named imported-prefix, maintenance and selector-only quantities | separate caller spans, no inferred global history/omitted callbacks |

Validate source header identity/counts and each projected graph once with bounded
iterative construction, not recursive graph walks or a whole-island scan per node.
Use source chronology to construct expected next/head relationships; invalid input
is source admission, not a native malformed-list validation rule. Preserve repeated
source records/entries. A later OCCUR load may reference an earlier occurrence but
its source is NOT another insertion. Preserve that ignored load's provenance too.

Every result/error carries the supplied flag context. Pipeline failures keep any
completed imported chunk/sidecars, habitat and maintenance result, plus the precise
child stage/failing source record or local token and consumed quantities. No
successful partial sweep, changed caller state or fake native rollback. No prefix
implementation/provenance/schema rewrite is assumed necessary.

## Literal fixture before code

Authored island origin (24,40), width 32/height 24, interior chunk (1,1), index 5.
Final dense foliage (105) occupies the 10 x 10 halo except local position 5,
which becomes nonqualifying grass. Property 7 is source INFO 2. HIRSCH source
records stand at positions 9 and 11, BERGWERK anchor local (15,14), rotation 1,
OCCUR at local (14.5,13.5), duplicate load of that occurrence, then later HIRSCH
at position 5. The 4 x 3 resource footprint visits chunks [5,6,9,10] in y/x order,
with nibble offsets [0x67,0x6f,0xe7,0xef]. Only its first node belongs to chunk 5.

Chronological global represented-node tokens: first spawns 0,1; resource nodes
2,3,4,5; occurrence 6; later spawn 7. Duplicate occurrence load has NO token.
Mixed target head order [7,6,2,1,0], categories [0,6,1,0,0]; spawn subsequence
[7,1,0], resource subsequence [6,2]. Context flags 0x8000 yield flags 0x8043.
Prefix has 99 resource draws plus one first-spawn jitter, imported-prefix 100;
loaded first timestamp 8, others 1, mode 1 preserves authored saved goods (0/137/0).

Maintenance at authored time 10000 retains two imported spawns, recycles 7,
keeps resources, desired 5 from 16 candidates. Draws [0,1,0,0,0,999] collide at
9 and 11, then insert SAME 7 at 63 and fresh 1000/1001 at 59/57. Final head order
[1001,1000,7,6,2,1,0], maintenance consumed 5, unused 999. New time byte 10 makes
all five spawns eligible. Selector draws [6,0] repeated five times plus 999 choose
five ordinary slot-0 species, consumed 10, suffix 999; categories 6/1 skip.
Sidecars unchanged. Unsupported slot 31 on first selection retains actual choice
and count then stops; one maintenance draw keeps collision partial. These three
draw domains must never be reported as a complete native cursor.

| Requirement | Distinguishing literal/negative control | Review |
|---|---|---|
| Mixed chronology | [7,6,2,1,0] vs group concatenation/coordinate sort | pending |
| Multi-chunk/duplicate load | one BERG target node, one occurrence despite two loads | pending |
| Payload/knownness | kind 1/6 typed sidecars; spawn zero known vs unknown saved ID | pending |
| Graph admission | broken/cyclic/wrong heads, missing source, corrupt occurrence index | pending |
| Flags | engaged 0 accepted; absent/empty provenance/0x43 overlap refused | pending |
| Stage accounting | prefix 100/maintenance 5/selector 10, exact failure domains | pending |
| Ownership | all inputs survive unchanged, output outlives source, repeat order equal | pending |
| Original | actual nonempty imported spawn chunk plus independently decoded order | pending |
| Causal | actual group/chronology/duplicate-load/zero-ID/draw-replay mutants | pending |

At I0 all new code/gates are NOT_RUN. No World/state/default/identity/tag/Rules/
global PRNG/native manager/coordinates/creation/navigation/FP/graphics claim or change.

## Implemented admission and proof boundary

Local tokens reserve [0, source_token_count) across the entire preparation, including
other chunks; supplied fresh tokens may not collide with that namespace. The
mixed list represents the three supported imported categories only; it is not a
claim to reconstruct omitted native runtime categories or every chunk cache bit.
Sidecars own accepted prefix payloads; this adapter does not replay floating-point
ore loading or validate arbitrary post-import mutations as native behavior.

The pipeline checks the species climate against the prepared source property and
Game bytes against the type-binding source. This is explicit source admission,
not original effect order. Existing stage implementations remain unchanged.

P/H: original Tutorial/01Discover01, chunk (10,2), contains one imported spawn:
local token 21, HIRSCH header 71/record 0, position 47, timestamp 8, saved goods 0.
Independent source chronology has 177 represented insertions (156 spawn, 20 BERG,
1 occurrence); imported-prefix count 4745. Authored time 10000 and flags 0x8000
retain this spawn; maintenance consumes 0, selectors [6,0,999] consume 2.
The independent raw species rows are 501/137, 501/137, 528/0: saved goods 0 thus
matches slot 2 after 3 inspections, selecting 528. An initial test expectation
incorrectly treated zero as unmatched; only those three expectations were corrected,
with the original failure and independent derivation retained in private cache.
This original chunk lacks interleaving; the literal mixed-header companion proves it.

C: legal authored control executes, and five actual private source mutants reject
group concatenation, forward chronology, duplicate-load insertion, invented saved
ID zero, and replayed maintenance draws. The first draw-replay control did not
compile due to its unused selector parameter; its corrected control explicitly
marks that parameter unused and then fails assertions. No failed compilation is
counted as a killed semantic mutant. Canonical gate results live in the handoff.
