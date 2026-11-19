# Wildlife postload navigation inputs — 2026-11-19

WILDLIFE-NAV-INPUTS-1; research and a private executable investigation only.
Production routing, identity, defaults and the unwired first-call/graphics
helpers remain unchanged. D = original instructions, C = executed private C++,
H = independent hand arithmetic, U = not established.

## Acceptance before the probe

| Claim | Independent expected result | Discriminating check | Disposition |
|---|---|---|---|
| Component masking precedes addition | D Welt 0x100842b6/0x100842bb: `(trunc(height / f32(.04)) & 255) + base` | Component 256 must become 0 before adding 16 | C agrees; removing the component mask fails this row; review pending |
| No sum mask or final clamp | D Welt 0x1008431f–0x10084331: halve the full maximum, store its low byte | Sums 255/256/270/510 give maximum halves 127/128/135/255 | C canonical mismatch at 256/270/510; review pending |
| Legal low-terrain companion to V9 | HEAD-derived 1 × 1 KLIPPE, flags 0x12; import on corners 18 computes base 16, height component 254 gives sum 270 | Registry build and real import must accept before querying; never inject the decisive anchor/base | C parsed registry and real import accepted all six object rows; review pending |
| Ordinary and equality controls | Vertices 19 halve to 9 (all water); 20 to 10 (all land). Range 18..32 does not strictly straddle 32; 18..34 does | Assert bytes, coarse class and shore separately | C seven controls agree; review pending |
| Scope | Canonical mismatch is a failed investigation, not a green acceptance gate | Private corrected TU demonstrates causality; no runtime edit committed | C canonical 3 mismatches, corrected 0; review pending |

H: flat-18 V9's previously proposed base 250 cannot be obtained from the fresh
mode-2 import on those corners: its quantized minimum is 16. That precise setup
has not been proved reachable. Use the legal companion above instead, preserving
the decisive original sum 270. It predicts min/max halves 9/135 and mixed;
current sum wrapping leaves vertex maximum 18, halves 9/9 and all water.
Removing only the sum mask predicts 9/127: class mixed but still wrong bytes.
These expectations precede the private execution. This is an authored metadata
profile, not a claim that shipped content contains the overflow case.

## 1. Original producer and bounded geometry contract

D: AnnoWelt 0x10084150–0x100841bc clips the supplied world rectangle to the
island rectangle, converts its ends to local coordinates; 0x100841bc–0x10084202
rounds the starting local coordinates down to multiples of 8. Subsequent starts
advance by 8 until the clipped exclusive ends (0x1008432a–0x10084353). A partial
UPDATE rectangle therefore selects whole local blocks; it does not clip the
inner sample loops. This is different from a partial island dimension.

D: 0x10084208–0x10084259 initializes min = 255, max = 0 and scans exactly
9 × 9 unsigned vertices using stride W + 1, including the far row/column.
0x1008425b–0x100842eb then scans 8 × 8 tile records, using tile stride W.
Eligibility is record byte 3 bit 0x80 (0x10084287), independently of the
per-tile height-map query. The catalogue lookup takes the record's object word
at 0x10084294; the accepted setup guarantees a nonempty, resolved object.
There is no flags/class test on this producer's object-height arm.

D: 0x100842a0 calls the descriptor's VIRTUAL slot +0x58, not a float at object
offset +0x58. The concrete common tables Game 0x1002185c and 0x10021a88 both
resolve that slot to 0x10007d50–0x10007d53, reading the float at descriptor +8.
Game 0x10009372–0x10009384 copies HEAD[0x14..0x17] there. Welt 0x100842a3
divides by f32 0.04 (constant 0x100c9c20, bit pattern 0x3d23d70a); helper
0x100b5f90–0x100b5fb6 selects truncation for integer conversion and restores
the old control word. The probe uses finite nonnegative fields with unambiguous
truncated quotients; it makes no claim about invalid x87 conversions or every
possible precision-control profile.

D/H: 0x100842ae–0x100842c1 computes
`candidate = (converted & 255) + (tile_word_1 & 255)` and updates only max.
Both addends are 0..255, so candidate is 0..510. After the vertex and object
passes, max is 0..510 and min is 0..255. The logical right shifts and byte stores
at 0x1008431f–0x10084331 give `max_half = max >> 1` (0..255) and
`min_half = min >> 1` (0..127). No saturation is needed, and no sum mask occurs.
Fine navigation is a distinct consumer: Welt 0x10090350–0x100905a8 uses the
legitimate BYTE addition at 0x100904e7. Do not change that rule.

D: island Init 0x1008c090–0x1008c0c5 stores the supplied origin and dimensions
without aligning them. It allocates zeroed (W + 1) × (H + 2) corner storage at
0x1008c18a–0x1008c1d0. The HEIGHT mapping remains the accepted
[vertex contract](../re/ship-navigation.md#92-static-map-terrain): W × H samples,
zero right column and two bottom rows. Chunk storage uses ceil(W / 8) ×
ceil(H / 8) records of 16 bytes (0x1008c2ed–0x1008c398). Nevertheless the
producer's inner loops have no partial-block clip. Arbitrary nonmultiple-of-8
islands' final scans are **not** proved safe/equivalent by that allocation alone;
there is no assumed object padding or invented edge clamp in this contract.

C: the existing adapter only exposes records when origin and dimensions are
8-aligned, and the whole world cell belongs to one island with a valid VertexGrid.
The probe uses a 24 × 24 island at (8,8), world 40 × 40, target cell (1,1).
It is the smallest square aligned extent in this rig that gives this cell and
its in-island neighbours wholly interior vertex ranges, avoiding an accidental
border-induced shore band in the high-terrain control. Full final border blocks
do include forced-zero vertices: the separate 16 × 16 border control returns
halves 0/9. Misaligned-origin and width-17 controls return no block record and
fall back to tile classification; they are checked adapter behavior, not a new
claim of original equivalence for those layouts.

## 2. Legal executable counterexample and exact bytes

C: private `probe.cpp` creates authored HEAD/PRODTYPE entries and passes them
through TypeRegistry::build; the resulting id 700, kind 322, 1 × 1 extent,
flags and full HEAD are checked before placement. It uses the real
resource_import::evaluate on empty tiles and immutable Terrain/VertexGrid.
Every row must accept its one record with zero draws; the real writer must set
object 700, anchor 1, offsets 0 and the hand-predicted base. No setter fabricates
the decisive object, anchor or base. The objects are queried through the explicit
owned-island WorldTileSource interface, with no item_base override, default
objects_block/block_shortcut and live registry/backing throughout the query.
This proves the adapter/transient import domain, not World::create, original
runtime spawning, a shipped-content failure, or full scenario publication.

H/C: decimal labels below name exact f32 inputs. Quotients were independently
derived as rational numbers from those bits before C++ execution, retained in
`hand-f32.json`; neither expected halves nor classes came from C++ outputs.
Flags 0x12 mode 2 yields base 16 on flat 18. The sum-510 row uses flags 0x09
mode 1 on flat 255, yielding base 255; all other object rows use flat 18.
D/H: the reused legal admission proof is [CLIFF](wildlife-cliff.md): Game
0x10009048–0x10009109 quantizes the mode-2 corner minimum (18 & 0xf0 = 16),
while mode 1 reads the origin quad at 0x10008fa1–0x10008fdf (255 here).
Welt 0x1007cbdf bypasses the ordinary minimum-32 gate for kind group 0x141;
the accepted empty 1 × 1 footprint supplies coverage. Writer 0x10082260 stores
the overlay height and sets the first written tile's anchor. Hence this is not
merely an unchecked combination of scalar, registry flags and tile fields.

| Row / f32 bits | Component / base / original sum | Expected halves / class / own-cell shore | Canonical observed | Sum-mask-only correction |
|---|---|---|---|---|
| Low control, 0x3da3d70a | 2 / 16 / 18 | 9/9, water, no | Agrees | Agrees |
| 255, 0x4118f5c3 | 239 / 16 / 255 | 9/127, mixed, yes | Agrees | Agrees |
| 256, 0x4119999a | 240 / 16 / 256 | 9/128, mixed, yes | 9/9, water, no | 9/127, mixed, yes |
| Legal V9 companion, 0x41228f5c | 254 / 16 / 270 | 9/135, mixed, yes | 9/9, water, no | 9/127, mixed, yes |
| 510, 0x41233333 | 255 / 255 / 510 | 127/255, land, no | 127/127, land, no | 127/127, land, no |
| Component wrap, 0x4123d70a | 256 becomes 0 / 16 / 16 | 9/9, water, no | Agrees | Agrees |

C: canonical fresh-TU execution: 13 rows, 3 mismatches, exit 1, setup refusals 0.
This is a preserved FAILED INVESTIGATION, not an acceptance PASS. The earlier
archive-linked execution agrees with it. A private source copy fixing the sum
mask only still has 3 wrong-byte rows, exit 1. Fixing component masking and
removing final saturation gives 13 agreements, exit 0; subsequently removing
the required component mask fails the component-wrap control, exit 1.
Replacement counts, compile/link commands and source/binary hashes are retained.
Canonical runtime files never changed. The private candidates are not a released
repair and no wrong-behavior regression was committed.

D/H/C: builder 0x100ab0b8–0x100ab0e7 doubles stored max/min halves, tests
`19 >= 2*max_half` first (all water), then `19 < 2*min_half` (all land), else
queries tiles and packs connectivity. Halving means a flat raw 19 is stored as
9/9 and classified water; flat 20 as 10/10 and land. Raw 31 and 32 give 15/15
and 16/16, both land. The shore condition at 0x100ab94b–0x100ab966 is STRICT
`2*min_half < 32 < 2*max_half`. A lone far-corner vertex raises a block from
flat 18 to range 18..32 (9/16, mixed but no shore) or 18..34 (9/17, mixed
and shore). These seven ordinary/far-corner controls agree. The separate four
geometry controls also agree. No CTest COUNTS/VERDICT is claimed for these
private tools; no suite was needed for documentation-only changes.

## 3. Callers, lifetime and compatibility

| Consumer | Existing behavior / impact of a correction | Evidence |
|---|---|---|
| WorldTileSource::classify_cell | Wrong low maximum can select all-water shortcut instead of mixed and bypass tile connectivity | route_world.cpp block_range/classify_cell; executed 256/270 rows |
| CoastIndex construction / rebuild_rect | Caches exact halves, derives strict shore type 3 and connectivity; correction can change route costs as well as masks | route_coast.cpp range_at/derived_shore/build_cell; executed own-cell shore outputs |
| RouteCache | Derived index, never saved/hashed; source borrows Island/Registry/HEIGHT. Rebuilds on immutable fingerprints or tile route-dirty rectangles | route_cache.hpp/.cpp, island.cpp set_aux/set_foot_anchor/set_object |
| ShipMover::WorldPlanner / plan_route | Production route choice consumes CoastIndex after sync_once; different masks/costs can change route, failure, travel and later command outcomes | ship_mover.cpp WorldPlanner::plan and ShipMover::visit; route_plan.cpp |
| Save/hash/replay | Cache absence from stream does not preserve semantic replay compatibility; NavMotion nodes, segments, position/residue/order are authoritative, saved/hashed | world.hpp field_list_canary, state_fields.hpp fields(NavMotion), replay.hpp |
| Tests / explicit tools | Explicit WorldTileSource callers include route/founding/script canaries and opening tests; changed exact ranges need independent expected values | source caller scan, not a rerun of those lanes |

No RNG call occurs in block_range; no new draw site follows from the arithmetic
repair. A changed route can still alter later simulation timing/decisions and
therefore downstream draws/trajectory. No claim that all real maps diverge, or
that none do, is established by this bounded synthetic probe. A save that resumes
an already stored route may initially retain it; rebuilding/replanning can differ.
Command payload layout need not change for identical commands to produce a
different future. Matching registry digest alone cannot detect a code-semantic
change to the same metadata.

Composition prerequisite: hold the validated descriptor profile and immutable
HEIGHT alive with the final first-call filled tile plane, including anchor and
item-height bytes. Recompute original extrema from that plane before constructing
or rebuilding the coarse index. A descriptor edit must invalidate/rebuild the
index too; the current registry is immutable, so in-place edits are outside its
contract. Never reuse an index borrowing a moved/replaced Island: copied/moved
World caches start empty. WorldTileSource/CoastIndex must not outlive or race
mutation of these owners. Postload §11 and isolated graphics remain UNWIRED.

Representation is sufficient for the proved scalar profile: HEAD retains f32
bits, tiles retain unsigned base/anchor and VertexGrid provides byte vertices.
Finite supported conversion yields the original unsigned component; use widened
integer max 0..510 before halving. No new persisted extrema are needed. General
metadata/nonfinite conversion and unaligned/partial-island equivalence remain
outside this proof. Fine-tile height maps remain the already documented distinct
adapter limitation. None warrants widening this research packet.

The second callback also updates the world +0x120 raster, reached via world
+0xbc. Its numerical input table/consumers remain unresolved in
[postload §4](wildlife-postload.md#4-second-call-navigation-and-another-derived-raster).
A correct CoastIndex alone does not establish complete second-callback behavior;
do not compose first-call, navigation and final graphics into a World yet.

## 4. One bounded repair proposal; owner/Windows decision required

Proposed WILDLIFE-NAV-REPAIR-1: edit only route_world.cpp's chunk candidate to
mask the converted component before addition and remove final max saturation;
correct route_world.hpp's chunk formula; add permanent independent expectations
to route_world_test.cpp for the six object rows, equality/far-corner controls and
cache rebuild behavior; update the navigation contract/reference where relevant.
Do not change object_tile_blocks or fine-tile arithmetic. Use the existing test
and reference mechanisms, not another framework. Run affected route arithmetic,
coast/dirty-cache/ship-order regressions and one bounded real-data canary, plus
strict warnings and CI. Full landing/Windows runs stay with the orchestrator.

QUESTION — semantic compatibility options for owner/Windows coordination:

1. Recommended: faithful base correction under a coordinated affected section
   version/identity decision, refusal boundaries and reviewed pin/replay delta,
   following [ADR 0006](../adr/0006-save-sections.md). Preserve the historical
   behavior only as evidence. The exact affected identity allocation is a separate
   contract; this packet assigns no section version or global number.
2. Keep current default and isolate corrected behavior behind a coordinated
   optional rule/tag; that needs two runtime behaviors, off-identity and save/
   replay tests. It preserves current default trajectories but retains known
   nonfaithful arithmetic until enabled. No tag is reserved here.
3. Apply only to the unwired transient postload projection for now and defer
   production correction. This avoids today's route semantics changing but leaves
   two extrema implementations and cannot establish equivalence to RouteCache.

Option 1 is preferred because this is a demonstrated original-rule discrepancy,
not an enhancement; silently changing default semantics under unchanged identity
would hide replay risk. No engine fix, pin update, identity edit or World wiring
is authorized by this recommendation. The orchestrator must coordinate the choice
before releasing the repair. Secondary raster work remains a separate packet.

For that decision, header is the concrete candidate semantic boundary because it
already carries Rules/NavRules; a peer/save must distinguish the routing behavior
before a new ship plans, even if no route is stored yet. Figures contains the
affected NavMotion state but no new layout is proposed. Coordination must settle
the boundary rather than treating the absence of a layout change as an exemption.
Depending on that choice, the repair's identity follow-up reaches world.hpp's
section table, state.md, the two independent stream references and existing
identity/refusal tests, with reviewed pins only if authorized. These are release
prerequisites, not edits made here; no second global state number is proposed.
