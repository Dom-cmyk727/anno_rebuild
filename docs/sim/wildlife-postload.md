# Wildlife post-island dependency contract

WILDLIFE-POSTLOAD-1, 2026-11-19. **READY FOR REVIEW: bounded research, not
post-island execution or World-publication acceptance.** D = GOG instructions,
P = parsed original files, C = inspected rebuild code, H = hand-derived vector,
U = unread or unresolved binding. No engine/reference implementation is added.

**Fix round 1, 2026-11-19:** the current contract corrects V12's claimed
padding discriminator and the secondary raster's rounding arithmetic. The
original `09614995` contract and `e05d9504` handoff remain frozen historical
evidence; use this corrected contract and the [correction receipt](../orchestration/evidence/wildlife-postload-1-fix-1.md)
for the superseding interpretation. All proposed vectors remain NOT_EXECUTED.

The [local prefix](wildlife-prefix.md) now completes all 73 local headers through
[EVENTINSEL](wildlife-eventinsel.md). That does not execute the three calls below.
The first can create grass/fish tile resources, the second builds navigation and
updates another raster, and the third can change GFXID words. No unchanged
postload draw total, cache-only exemption or global initialization follows.

## 1. Receiver and call-order ledger

D: AnnoWelt `0x100a2ea0` constructs a stack half-open rectangle from the INSEL5
head at `0x100a2f00–0x100a2f4d`: four i32 world endpoints, original u16 origin
plus extent widened for the right/bottom endpoints. After the local loop, EBX is
world and EDI island. At `0x100a2fc0–0x100a2fc8` it pushes island, then the SAME
rectangle pointer. At `0x100a2fd0–0x100a2fd7` it passes that rectangle again. At
`0x100a2fdd–0x100a2fdf` it directly calls an island method with no argument.
There is no return-value test between these calls; `0x100a2fe4–0x100a2ff0`
returns success 1 after all three. This does not guarantee their allocations or
indirect consumers succeeded. The rectangle lives on the caller's stack only.

| Order / site | Receiver, concrete target and arguments | Proved writes / next edge | Closure |
|---|---|---|---|
| 1 / `0x100a2fc8` | WORLD +0x1d4 -> `0x100a1810–0x100a183c`, `(rect, island)` | If island byte +0xc2 is zero: island +0x1a4 -> `0x10084370–0x10084818`; ALWAYS island +0x1a0 -> `0x100928a0–0x10092b32`, same rect | Wrapper closed; reached Tutorial preprocessing skip proved. Fill closed for the addressed standard catalogue profile; fresh first-call catalogue binding narrowed in §10 |
| 2 / `0x100a2fd7` | WORLD +0x1d0 -> `0x100aba70–0x100aba9b`, `(rect)` | WORLD +0xf8 -> `0x100aae00–0x100ab9ed`; then WORLD +0xbc getter, if nonnull object +0xc `(2, rect)` | Concrete standard-world bodies identified; coarse-map/secondary-raster equivalence is not supplied by current prefix |
| 3 / `0x100a2fdf` | ISLAND direct `0x10085290–0x100853a9`, no rectangle argument | Whole-island graphics cleanup, conditional u16 writes at island +0x10 | Complete leaf; no callees or draws |

D: standard derived world table `0x100ccdd8` has +0x1d4=a1810,
+0x1d0=aba70, +0xf8=aae00, +0xbc=a3510. Base `0x100cc92c` has the same
+0x1d4, but +0x1d0 is the `0x10037590` ret-4 stub and +0xf8 is not a builder.
Do not silently choose a table merely because a receiver is called world.
Fresh AnnoScene `0x10002e75–0x10002ea6` passes kind 1/no filename to the Welt
factory. Welt `0x100b2525–0x100b259b` allocates and constructs through af3e0;
`0x100af442` installs ccdd8. Scene's reuse path `0x10002e4e–0x10002e72`
reinitializes an already supplied world; arbitrary substituted worlds are not
proved equivalent. The transient prefix contains NO world receiver today.

## 2. First call: preprocessing gate and empty-cell fill

D: wrapper a1810 reads island +0xc2 at a1815 and calls +0x1a4 only if zero at
a1822–a1829. It calls +0x1a0 unconditionally at a182f–a1834. Both slots resolve
through wildlife `0x100cac2c` to the targets in the ledger. This is not a +0x19c
writer notification. FRUCHTBAR load sets +0xc2=1 at `0x10089565`, regardless of
whether the actual fertility bytes are zero. [Fertility contract](wildlife-fertility.md).
P: the same Tutorial has FRUCHTBAR 49, 16384 bytes, so its completed load implies
+c2 nonzero and the preprocessing branch is NOT reached. The current prefix has
that completion evidence but no explicit c2 field; presence, not byte content,
must drive a future precomputed-ground capability.

D/U: the c2-zero path is NOT a safe default or a cache-only substitute. Full body
84370–84818 scans 16 original city-ring entries and queries building/type metadata
at 8438f–843d6, then catalogue property 0x2e at 84413–8441f. It allocates a W*H
scratch plane, derives per-cell values using GFXID >>2 and type metadata, and
performs a clipped 5x5 climate-kernel accumulation. At 8478d–847ad it merges a
bounded result into the LOW NIBBLE of island +0x18, keeping the high nibble.
It does not write the corner grid. It frees scratch at 84801–8480b and actually
returns at 84818, later than the previously quoted 8480e epilogue start.
Property 0x1c/0x2e providers, climate kernel provenance and their caller data are
not newly closed here. Partial-rectangle scratch-neighbour initialization is
also not established. Unsupported c2-zero must stop, not manufacture fertility.

D: fill 928a0–92b32 clips the half-open world rectangle to the island and visits
rows then columns. It resolves catalogue types 2 and 11 before cell iteration.
Only a WHOLE u16 tile-object word equal to zero enters the fill; do not mask it
to 14 bits. Four unsigned corner bytes, with stride W+1, produce truncated mean.
Reuse the addressed rules in [fisher §6](../re/fisher.md#6-runtime-fish-fill-and-the-real-map-boundary-fisher-re-2-2026-11-15):

- Default mean <16: create type 11 resource in the existing tile, no occurrence,
  figure or HIRSCH record. Mean 16 through 31 remains empty.
- Mean >=32: grass branch at 929e3–92a60, only if `(ground &15)*10 >50`.
  The compare/jbe is UNSIGNED; low nibble 13 also qualifies (130 >50).
- Grass writes type-2 id, clears footprint offset byte, sets computed item height,
  preserves only byte-3 anchor bit 7, replaces walk-code byte and clears nine mask
  bits with `0xfe0000ff` at 92a48. Fish preserves the other 24 second-word bits.
  Neither fill branch modifies the original corner grid or fertility plane.

D: thresholds are runtime initialized, not arbitrary zero-filled PE bytes:
8fea0–8feb2 sets 16*0.04f, 8ff00–8ff0e doubles it, getter conversion truncates
back to 16/32. Standard type-id/height/walk getters are addressed in fisher §6;
parsed type-2/11 HEAD flags 0x29 select the mean-height branch. Their finite named
profile makes no random draw. No claim applies to an unbound substituted catalogue.
C: `fill_fish_resources` in `src/sim/src/fisher.cpp` and `runtime_fish` in
`tools/fisher_ref.py` implement the fish subset, not the grass or preprocessing
branches. The reference explicitly refuses empty land cells needing fertility.
Do not call that subset equivalent to the entire post-island fill.

## 3. Catalogue provenance: a narrowed dependency, not a guessed binding

D: island +0x1c receives world +0x18 at `0x1008c0c9–0x1008c0db`; concrete getter
`0x100a37d0–0x100a37d6` returns world +0x110. This read newly follows the store:
derived world Init calls base `0x1009d820` at aa93c; base Init stores its THIRD
argument into +0x110 at `0x1009d848`. Factory b2568–b2588 forwards its external
fourth argument as that pointer. Scene 2e84–2ea6 supplies Scene +0x154; Scene
constructor `0x10002fc0–0x10002fc6` stores its caller's pointer there.
This closes the pointer-store edge of [OQ-202](../open-questions.md#oq-202-whether-growth-rewrites-the-tiles-walk-record-t334-f5-2026-10-26),
not the identity/construction of the actual catalogue or its type-pointer array.
Direct Scene constructor callers 0x10006ee4/0x10007a24 and actual catalogue-array
population are remaining read edges. No ref-count/lifetime ownership contract
was inferred merely from storing the pointer. Future modern use needs owned or
retained validated facts, not an assumed TypeRegistry substitute.

## 4. Second call: navigation AND another derived raster

D: aba70 first invokes world +0xf8 at aba7b, then +0xbc at aba85. Getter
`0x100a3510–0x100a3516` returns world +0x120. Nonnull invokes object +0xc with
flags 2 and rect at aba96. It is not proven to be a draw-free generic observer.
For the standard initialized world, aaa53–aaa8f allocates the object, installs
`0x100ccd40` at aaa7a and stores it into +0x120 at aaa8f. Its +0xc target is
`0x100a3cc0–0x100a40c5`.

D: coarse builder aae00–ab9ed uses the world island lookup grid (+0x130), island
pointers (+0x6530), world dimensions, and island +0x94 chunk records. Reuse
[ship-navigation §9.2](../re/ship-navigation.md#92-static-map-terrain) for its
8x8 cells/64x64 sectors and connectivity packing. Independently checked at
ab0b8–ab0e7: default threshold 19 >=2*max_half -> all water (0xfff),
19 <2*min_half -> all land (0), else mixed -> world query +0xec and a4130.
Second-pass unrolled neighbours, e.g. ab94b–ab966, require STRICT
`2*min_half <32 <2*max_half` for shore band type 3; equality does not qualify.
Runtime threshold constructors a3b40/a3ba0/a3bf0 establish 16/32 and (1.28f-.5f),
not data copied from uninitialized globals. Connectivity builder/query object
semantics still depend on matching existing navigation source facts.

D: chunk min/max producer `0x10084150–0x10084360` visits 9x9 corner vertices,
then anchored objects in the 8x8 tile block; object descriptor +0x58 height is
converted, masked to a byte, then ADDED to tile base height as an integer at
842ae–842c1. Only the final extrema are halved/stored at 84323/84331. Prior
[writer cache omissions](wildlife-rock.md#writer-callback-correction-2026-11-19)
therefore matter: the navigator reads those bytes at ab0b8/ab0d4. Dirty tracking
alone is not equivalent input. Rebuild exact bounds before classification.

C/H: `WorldTileSource::block_range` in `src/sim/src/route_world.cpp` can recompute
bounds from immutable terrain and anchors; `CoastIndex` in route.hpp/route_coast.cpp
can rebuild the existing coarse index. HOWEVER block_range currently applies
`(object_top_units(...) + base) &0xff` BEFORE the maximum. That differs from the
original chunk-bound producer. Do not confuse it with the distinct original
fine-tile query's BYTE addition at 0x100904e7, which legitimately wraps.
Static hand counterexample V9 below blocks an unconditional equivalence claim.
A later correction must ALSO audit block_range's separate `min(mx,255)` before
halving. Removing the sum mask alone gives max_half 127 for V9, not original
135. Both are mixed, so class-only assertions miss this second numerical mismatch;
assert the exact extrema bytes too. No route repair or compatibility change is
released by this research correction.
No source fix, compatibility choice, shipped-map failure or executed reproduction
is claimed in this packet.

D: flags-2 raster branch a3cc3–a3ed1 obtains world width/height, maps the rectangle
to its raster buckets, sums world +0x5c height samples, normalizes/truncates and
updates object byte plane +0x14. **Corrected from original instructions,
2026-11-19:** at a3e7d–a3e90 the literal arithmetic is
`u = converted &0xff`, `rounded = (u+8)>>4`,
`new_byte = (old_byte &0xf0) | rounded`. There is NO `&0x0f` on rounded.
Since u is 0..255, rounded is 0..16; u 248..255 produces 16, which can SET
bit 4 of the old high nibble. Only a proved u <=247 bound guarantees high-nibble
preservation; the runtime numeric-table domain remains U, so no unconditional
preservation claim is made. Old byte 0 / converted 248 yields 0x10, not 0x00.
It increments object dword +0xc at a3ece even if no bucket loop runs. Flag-1 work
is skipped at a3ed7–a3edc for the actual flags 2; return a40be–a40c5.
World +0x5c is `0x1009db00–0x1009db68`: resolve island and call island +0x98.
Concrete +0x98 is `0x10084c20–0x10084c51`, loading the vertex byte's runtime
float lookup at 0x100e2d90. The producer/profile of that initialized table and
all consumers of raster +0x14/generation +0xc are NOT closed. No renderer-only
or safe-omission claim follows. Keep/rebuild a proven equivalent before any such
consumer or refuse publication. The coarse route index has known consumers in
CoastIndex/route_plan; the extra raster is not that index.

## 5. Third call: whole-island GFXID cleanup

D: leaf `0x10085290–0x100853a9` scans every graphics u16 at island +0x10.
Only `(word &0xff00)==0x2000` qualifies at 852d1–852e0. For each qualifying tile
(x,y), examine vertex coordinates x-1..x+2 and y-1..y+2 (16 samples), using +0x14
stride W+1. Bounds 85303–85313 admit vertex coordinates THROUGH W/H inclusive;
outside is zero. If maximum is zero, write WHOLE graphics word 0 at 8534f–85353.
A positive sample anywhere in this 4x4, even outside the tile's own quad, retains
the word. No threshold 16/32, bit-only graphics test, HEIGHT/resource/variation/
spawn write, allocation, callback or PRNG draw. No rectangle limits this scan.
The extra H+1 padding row is not selected by its explicit bound.

C: prefix terrain owns raw graphics immutably. A future pure cleanup helper must
return a new owned graphics plane/Terrain and recompute its digest once; never
mutate shared const Terrain, drop low UV bits selectively, or preserve the old
digest after changing words. This leaf can be implemented/tested independently,
but actual publication order remains fill -> navigation/raster -> graphics cleanup.
It is not permission to run the third callback early in a live pipeline.

## 6. Hand-derived vectors and acceptance (NOT_EXECUTED)

These authored vectors precede any helper. **No new helper, reference file or
canonical test registration was added.** A compact COMPLETE composition would
require unresolved catalogue bindings, c2-zero inputs, raster numeric/consumer
contracts and navigation equivalence. A helper echoing only the final expected
values would not close those edges. The vectors remain H / NOT_EXECUTED; earlier
fisher/navigation references are reused only within their documented domains,
not rerun or presented as new passes.

| Vector / addressed predicate | Authored input | Hand-derived expected / nearest wrong interpretation |
|---|---|---|
| V1 / a1815–a1834 | c2=0 versus 1; same rect | 0: preprocess then fill. 1: fill only. All-zero FRUCHTBAR still gives c2=1; testing data nonzero is wrong |
| V2 / 92976 / 92a9c | empty; four corners all 15 versus all 16 | 15 -> fish 11, 16 -> unchanged 0. Original object word 0x4000 skips, not masked-empty |
| V3 / 929d9–929f6 | empty, mean 32, fertility low nibble 5 versus 6 versus 13 | 5 -> empty; 6 and 13 -> grass 2. Unsigned 130 >50, not signed negative |
| V4 / 92a25–92a5d | grass branch, mean 32, old aux 0xabcdefaa, code 2 | height update then `(aux &0xfe0000ff) | (2<<8)` -> 0xaa000220; preserving old mask is wrong |
| V5 / 92acb–92adb | fish branch, mean 15, old aux 0xabcdefaa, phase/anchor byte 0xff | aux 0xabcdef0f, phase/anchor 0x80; other 24 aux bits preserved, not grass mask clearing |
| V6 / ab0b8–ab0e7 | (min_half,max_half)=(9,9),(10,10),(9,10) | all-water, all-land, mixed respectively; do not apply raw mean <16 |
| V7 / ab94b–ab966 | shore ranges (15,16),(16,17),(15,17) | false,false,true; strict 32 equality is not straddling |
| V8 / a3ed7 / a3ece | flags 2, valid initialized cache, rect [0,0,0,0), generation 7 | no bucket write, generation 8. Flags 0 -> unchanged 7. Empty rect is not whole callback no-op |
| V9 / 842ae–84331 versus route_world.cpp | Interior chunk: 9x9 vertices all 18, one anchor base 250, descriptor height 0.8f ->20 height units | original max=270 ->half135, min_half9 ->mixed. Engine wrapped sum14 leaves vertex max18 ->half9 ->all-water. Removing only the sum mask still hits min(mx,255), giving half127, NOT135; compare bytes, not only mixed class. STATIC/NOT_EXECUTED, not observed corpus failure |
| V10 / 852d1–85353 | graphic 0x20ab, all legal 4x4 vertices zero | whole word 0, not 0x00ab. Graphic 0x6001 unchanged, despite bit0x2000 |
| V11 / 852eb–85349 | graphic 0x20ab, only vertex(x+2,y+2)=1 | unchanged 0x20ab; a quad/3x3-only implementation incorrectly clears |
| V12 / 85303–8532d | Raw-Terrain 1x1 HEIGHT 0 versus 1, logical border forced zero | graphic 0x20ab ->0 versus retained: a zero/nonzero sanity case ONLY. Nearest-edge clamping repeats a nonnegative sample already in the 4x4, so it is observationally equivalent for this maximum/zero predicate, not distinguished by this case |
| V12a / 85307–85313 | CONDITIONAL low-level validated corner-grid representation: 1x1 tile domain, legal logical vertex (W,0)=1 (or (0,H)=1), all other vertices zero | inclusive <=W/<=H keeps 0x20ab; exclusive <W/<H wrongly clears. NOT_EXPRESSIBLE through raw Terrain's forced-zero border; use only if a separately validated low-level grid contract admits this input, otherwise narrow the dynamic claim |
| V12b / 85311–8532d | CONDITIONAL low-level validated padded-grid representation: all legal vertices zero, physical H+1 padding row has sentinel 1 within the sampled x window | original excludes H+1, so clears; a reader treating H+1 as legal wrongly keeps. Raw Terrain does not expose this padding row: NOT_EXPRESSIBLE there. Test only if the low-level representation admits arbitrary padding, otherwise retain static proof only |
| V13 / a3e7d–a3e90 | old byte 0, converted 247 versus 248 versus 255 | output 0x0f versus 0x10 versus 0x10. Masking rounded to low nibble would wrongly produce 0 for the latter two. Runtime reachability/bound U; H/NOT_EXECUTED |

| Acceptance requirement | Proof delivered / exact residual | Independent disposition |
|---|---|---|
| Three receivers/order/errors | D ledger, standard/base world targets distinguished, stack rectangle and no return checks | pending |
| Reached first-call predicate | D FRUCHTBAR store and wrapper test; P raw Tutorial flag/rectangle derivation | pending |
| Do not assume cache equivalence | D chunk producer/consumer and extra raster; C/H static V9 mismatch | pending |
| Third leaf width/bounds/effects | Complete D body; V10/V11 distinguish exact high byte, whole-word clear and 4x4. V12 is a sanity case, padding/clamping equivalent for this predicate; V12a/b require an explicitly admitted low-level grid, not fabricated raw inputs | pending |
| Raster merge arithmetic | D a3e7d–a3e90 and H V13 distinguish a carry into bit 4; high preservation conditional on proved converted-byte bound <=247 | pending |
| Honest draw closure | Third leaf 0; first named standard fill getters 0; no total postload/global claim while bindings/query edges remain U | pending |
| No unrelated work | Docs only; source/fixtures/CMake/native/defaults untouched; prior C++ evidence retained without rerun | pending |

## 7. Single dependency and ownership table

| Effect / supported inputs | Existing equivalent | Missing input/effect and consumer obligation | Smallest next unit |
|---|---|---|---|
| c2 predicate, valid completed FRUCHTBAR | PrefixHeader fertility completion and raw low nibble available | Model precomputed-ground presence separately from data nonzero. Do not run absent branch with fabricated metadata | Capability input contract |
| c2-zero preprocessing | None established for complete 84370–84818 | City/type properties 0x1c/0x2e, GFXID lookup, climate kernel and full-rectangle scratch preconditions; reads/writes fertility used by grass/farms | Separate bounded read, NOT silently folded into current Tutorial |
| Full grass/fish fill, named standard 2/11 profile | fisher::fill_fish_resources and fisher_ref runtime_fish cover ONLY fish; fisher_corner/VertexGrid cover border geometry | §10 closes the fresh 2/11 profile; immutable fact validation and grass threshold/code/mask implementation remain. Existing fish helper is not whole callback proof | Transient first-call apply, stop before navigation |
| Original +0x94 extrema | WorldTileSource::block_range is a candidate reconstruction | V9 sum-width mismatch AND separate min(mx,255) clamp require audit with exact extrema bytes; Game height metadata and anchor semantics before CoastIndex reads | Focused navigation-input verification/correction contract |
| Coarse sea index | CoastIndex rebuild_rect, WorldTileSource, existing builder vectors | Whole world dimensions/island lookup, consistent lifetime and exact extrema/overlay inputs. Rebuild before route_plan's first read and after reload | Navigation adapter composition after input closure |
| Additional +0x120 raster +generation | No equivalent proven here | Exact byte mask ->round ->OR can set bit 4 for converted bytes >=248; numeric profile/bound and consumers unread. Preserve/rebuild before readers, no cache exemption | Bounded secondary-raster contract |
| Whole-island GFXID cleanup | Raw Terrain gfx plus proven zero-border HEIGHT geometry available | Pure leaf implementation, owned result/digest update once; publish only after ordered preceding effects | State-free graphics finalizer |
| Original global draws/live state | Existing frozen local prefix only | Original catalogue/world initialization and live figure/manager scheduling not established. 4745 is PRE-callback supplied consumption only | Future global-input and owned import contract |

Modern ownership must retain catalogue/terrain inputs for every borrowed query,
keep the projected resource/variation state and raw Terrain separate, and commit
no partial result into World on an unimplemented callback. Derived navigation or
raster data may be rebuilt rather than serialized only once equivalence and
before-first-reader ordering are proved. Nothing here allocates a persistent tag.
State framing 139 and section versions remain unchanged. Future save/replay/
registry/tag registration belongs to a separate contract, coordinated with the
Windows orchestrator; this packet neither reserves a tag nor changes defaults.

## 8. At most three immediate continuation packages

Full live-wildlife integration cannot honestly fit into the next three small
units while these input/cache edges remain. These are bounded sequential units
toward it, with hard stopping points rather than a promise of World readiness:

1. **WILDLIFE-GFX-FINALIZE-1 (about 2 h):** implement the fully proved state-free
   third-call leaf as an UNWIRED owned transformation. Files: `src/sim_load/src/wildlife_prefix.cpp`, its public
   `include/anno/sim_load/wildlife_prefix.hpp`, `src/sim/tests/wildlife_prefix_test.cpp`,
   `tools/wildlife_postload_ref.py`, its `_contract_test.py`,
   `fixtures/synthetic/sim/wildlife_postload_vectors.txt`, root `CMakeLists.txt`
   for the two reference registrations, and this contract. Reuse the existing
   reference CLI/contract pattern, not a new framework. V10/V11 and V12's
   zero/nonzero sanity case must run, plus immutable input/digest identity and
   explicit geometry errors. Do not claim raw-Terrain tests distinguish zero
   padding from nearest-edge clamping: those are observationally equivalent
   here. V12a/b are conditional low-level grid controls only; if that input is
   not admitted, mark them NOT_EXPRESSIBLE and narrow the claim, without expanding
   supported inputs solely to make a test pass. Negative controls: 0x6001, the +2,+2 vertex, nonzero
   height. Stop after isolated leaf/evidence; do not execute it ahead of navigation
   in a published pipeline, change a native field or move pins.
2. **WILDLIFE-POSTLOAD-INPUTS-1 (about 2 h, one catalogue contract):** close
   the direct catalogue producer/type-array binding beyond Scene constructor
   callers for the first-call type-2/11 profile. Files: `docs/re/annowelt.md`,
   `docs/sim/wildlife-postload.md`, `docs/open-questions.md`, and the existing
   raw-data inspection/reference tooling only where needed. Before: OQ-202's
   concrete binding remains U. After: addressed supported descriptor/profile and
   lifetime predicates, independent legal/wrong producer controls. This is a
   contract unit, not a registry rewrite, c2-zero ground port or navigation repair.
   Stop at a remaining catalogue input rather than inventing a default. V9 and
   the secondary-raster obligations remain separately reported blockers.
3. **WILDLIFE-POSTLOAD-FILL-1 (about 2–3 h, released only after 2):** apply FIRST
   callback under explicit valid precomputed-ground/catalogue capabilities. Files:
   `src/sim_load/src/wildlife_prefix.cpp`, its public header,
   `src/sim/src/resource_import.cpp`/public header only where the typed tile session
   needs an owned fill operation, `src/sim/tests/wildlife_prefix_test.cpp`, and
   the reference/contract/vectors established by package 1. Reuse existing fisher
   geometry/fish semantics only where preconditions match. V1–V5 and preserve-
   occupied/zero-area/owned-state/draw-suffix controls must pass. Missing c2 or
   unknown profile refuses before effects. Stop BEFORE +0x1d0 until its exact
   extrema/coarse/raster equivalence is released; no World publication or default.

Navigation/raster composition and subsequently owned World spawn state remain
unreleased dependencies after that boundary. A later World-import packet must
name its owning extension/section with Windows, validate no duplicate load draws,
save/reload/replay the exact owned records/cursor, and prove lifecycle handoffs
without inventing live ids. Materialization/tick/removal and owner living defaults
require their own reviewed releases. This is a concrete dependency, not permission
to pack all of those steps into package 3. No owner policy QUESTION is introduced
by the unread static edges; D-1 map selection/D-2 living defaults remain untouched.

## 9. Verification and research limits

Docs/link/diff checks only. No new helper/reference/CMake/test/fixture changes,
no new C++ run, no original-prefix replay merely to restate its frozen 29 Debug /
29 ASan /1 original passes. Hand vectors V1–V13 (including conditional V12a/b) are NOT_EXECUTED. Raw context
inspection establishes one island, FRUCHTBAR 49 and [24,24,152,152), not output
counts for a callback that has not executed. No new postload corpus or runtime
measurement, complete transitive draw total, original/HE execution or late-game
performance claim. Sparse read edges and conditional closure above are deliberate
scope boundaries. Logs/listings/raw inputs stay private with hash receipts in
this packet's handoff evidence.


## 9. Isolated graphics finalizer — WILDLIFE-GFX-FINALIZE-1

G1 acceptance before C++: the exact predicate/window is §5 (D: AnnoWelt
0x10085290–0x100853a9, freshly re-read). The supported input is an island's
validated raw Terrain, positive tile width/height, HEIGHT and GFXID of exactly
W*H elements, fertility absent or W*H, and matching digest. Geometry is inherited
from TileLayer; multiplication is widened before lengths/indexing. VertexGrid
supplies the existing forced-zero W/H border; outside is zero. No low-level
arbitrary-corner/padded-grid interface is introduced. Conditional V12a/b remain
NOT_EXPRESSIBLE at this interface, not executed controls. Zero padding versus
nearest-edge clamping remains observationally equivalent for the max predicate.

The [12 authored vectors](../../fixtures/synthetic/sim/wildlife_postload_vectors.txt)
were hand-derived before C++, using §5 and V10–V12; the
[reference](../../tools/wildlife_postload_ref.py) never reads expected outputs as
computed answers. This is an isolated leaf, not ordered postload execution.

| Requirement | Distinguishing authored expectation | Executed proof / disposition |
|---|---|---|
| Exact high byte/full-word clear | 0x20ab->0, 0x6001/0x2101 retained, 0x2000->0 | 12 reference/C++ vectors pass; actual-code bit-test mutant fails 6 assertions; review pending |
| 4x4 with unsigned-positive test | target (1,1), sample (3,3)=1 preserves; sample (0,0)=7 preserves | 12 reference/C++ vectors pass; actual-code quad mutant fails 8 assertions; review pending |
| No wider neighbourhood or transposed stride | x+3/y+3 do not preserve; 5x2 asymmetric plane has only one clear | Reference/C++ full-plane comparison passes; review pending |
| Owned immutable channels and digest | nonzero height/fertility survive; changed/no-change, source destruction, idempotence | C++ ownership/immutability/digest/idempotence tests pass; review pending |
| Refusal before writes | absent, malformed column/digest, empty geometry | C++ refusals pass, source unchanged; review pending |
| Unwired and original ordering preserved | production caller search excludes prepare/load/World | Only declaration, definition and test callers; no integration claim; review pending |


G4 disposition: `finalize_wildlife_graphics(const sim::Island&)` returns
`Result<FinalizedWildlifeGraphics, std::string>` with a new owned immutable Terrain
and cleared-word count. It scans the full island in O(W*H), with 16 vertex reads
per qualifying word and one output-plane copy; all other columns are copied once,
and the output digest is computed once. Input shape and digest are checked before
writes. No production caller exists. Input destruction, exact other-channel
retention, deterministic repetition and idempotence are tested. Different/no-change
digests are asserted on the authored cases, not claimed collision-free in general.

The new 12-vector raw-Terrain subset of V10–V12 is executed; §6's broader
hand-derived ledger is historical and does not become an executed postload model.
V12a/b remain NOT_EXPRESSIBLE at this interface. No equivalent-clamping mutant
is claimed. Catalogue binding, c2-zero ground preprocessing, full first-call fill,
second-call navigation/raster inputs and consumers, V9's sum-mask/clamp obligations,
original aggregate draw closure and actual publication order remain unresolved
and unchanged. See the [isolated leaf evidence](../orchestration/evidence/wildlife-gfx-finalize-1.md).

## 10. First-call input capability — WILDLIFE-POSTLOAD-INPUTS-1

2026-11-19, D/P/C/H. [Addressed producer/lifetime read](../re/wildlife-postload-inputs.md).
This supersedes §3's store-only limitation for the **fresh standard first-call
2/11 mean profile only**. It does not close OQ-202 globally or bind arbitrary
reuse receivers. No fill implementation, catalogue rewrite or pipeline wiring is
added. The §9 graphics helper remains UNWIRED.

D: Frame 0x10020108–0x100201d7 creates kind 0x101 through AnnoGame's factory,
checks initialization, and later supplies the global pointer as Scene factory
argument 3 at 0x10020792–0x100207a6. Game constructors install catalogue
0x100215a4 and descriptor 0x10021a88, not an assumed base descriptor. Anlagen.dat
ENTRY keys populate +0x214+4*id through 0x1000329d–0x100032af. The type setter
stores the low u16 id; lookup itself has no bounds/null defense. Keys 2 and 11
and all relevant concrete slots are independently checked. The existing Scene
->world->island chain then binds fill's two lookups. Reuse/injected catalogues
remain a separate validated-input domain, not automatically the same object.

### Capability and ownership boundary (proposed, not implemented)

| Input / fact | Required validation before any effect | Lifetime / modern source |
|---|---|---|
| Precomputed ground | Completed local-header sequence and successful FRUCHTBAR completion trace, including a zero update count; no inference from nonzero bytes. Missing completion refuses c2-zero as unsupported | PrefixHeader::fertility_updates plus PreparedIsland::complete_local_records; retained raw Terrain and owned effective variation |
| Ground geometry/identity | Effective variation exactly W*H; low nibble agrees with last completed raw FRUCHTBAR plane. Raw payload/high nibble is NOT interchangeable with the effective plane | Session's owned variation preserves prior high variant bits; Terrain retains raw fertility. Fill reads low nibble only and preserves the entire effective plane |
| Catalogue/profile binding | Explicit original role ids 2/11 resolved from unique loaded entries, validated source/profile identity; no null or missing lookup fallback | Copy small validated id/flags/mean-profile/walk facts, or retain shared const registry for every query; never borrow local parser buffers |
| HEAD and returned values | Present unique supported HEAD, original zero-extension to 56 bytes, registry flags/head/id agree with parsed facts; flags&3==1. Grass walk code from addressed HEAD encoder/getter at rotation 0 | TypeRegistry owns head_raw, flags_word and TypeId after loader's temporary bytes die. Builder-created arbitrary values are not automatically original-profile evidence |
| HEIGHT and tile geometry | Positive validated dimensions, widened W*H length arithmetic, exact HEIGHT length, matching Terrain digest, identical TileLayer rectangle/origin; clipped half-open signed rectangle | Retain shared const Terrain through all borrowed VertexGrid use; logical W/H border zero, no arbitrary padding interface |
| Occupancy and tile fields | Whole nonzero object word/id skips; no low-14-bit mask. Unknown occupied ids need no descriptor lookup for this callback. Existing per-cell channel lengths must be valid | Owned TileLayer output from one copy; input object/aux/phase/offset/anchor fields untouched on refusal |
| Optional supplied draw suffix | Named concrete profile only; no draw source invoked and consumed count unchanged | No global RNG or pointer to a mutable generator; no aggregate initialization claim |

C: `load_original_registry` owns its temporary raw buffers only during parsing;
`TypeRegistry::build` copies HEAD into owned head_raw and seals immutable types.
The relevant metadata already exists: TypeId, flags_word, head_raw and the
existing walk_code(type, 0). **walk_class is not this walk-code getter**. Existing
resource_metadata validates WARE/selector facts for another purpose and does
not establish these HEAD/registry cross-checks. A small explicit immutable fact
view/capability must add that validation; a bare TypeRegistry reference is not
sufficient evidence. Neither goods lookup nor descriptor float height is needed
by the selected mean branch. No original catalogue array or 4000-type engine cap
is copied into the rebuild.

D/C: original role selectors are addressed constants; their values and metadata
are resolved against loaded entries, not a compiled copy of those entries or a
search for any WARE named grass/fish. Data-defined alternate descriptors can use
the existing parsed-entry / TypeRegistry::Builder seam only with explicit role
binding and the same validated mean/getter semantics. This does not introduce a
mod API or authorize changing the original default selectors. Other height
profiles or unbound concrete behavior are Unsupported, not guessed zero-draw
fallbacks. TypeId/geometry stay widened; original file u16/u32 widths are recorded
at the boundary rather than imposed as engine content limits.

The future apply is one synchronous owned transaction. It validates the complete
capability before writes, clones the tile output once, retains HEIGHT backing,
and exposes no partially successful output on refusal. Terrain/ground/variation
bytes and Terrain digest remain unchanged: this first-call fill writes tile
objects/offsets/phase/aux/code/mask, not raw terrain. Existing TileLayer setters
must maintain dirty chunk digests; compare to independently rebuilt tile fields.
Successful result owns the tile output and retains any referenced immutable
inputs. No borrowed query may survive its owner. No persistent state/tag/version
or default change is decided by this contract.

### Acceptance and next implementation

| Decisive fact | Primary + independent positive / nearest wrong case | Current disposition |
|---|---|---|
| Concrete array/slots | Game 0329d/037e4, table 215a4/21a88; keys 2/11 select offsets 21c/240, not consecutive slots | 8 PE binding checks pass; static, original runtime NOT_RUN |
| Loaded mean profile | HEAD flags 0x29; Game 08fae–08fdf; corners 1,18,53,255 ->81, not 1/255/82 | Existing Python reference checks both raw profiles; forcing mode 0 produces actual 0 instead of 81 |
| Walk producer/default | Game 0940b/091f0/07dc0; shipped first/shape 2/3 ->2, authored 3/0 ->3, 0/0 ->constructor 4 | 8 rotation predictions and 2 authored existing-helper controls pass; not C++ or runtime execution |
| Presence before fill (V1) | Welt 89565/a1815: all-zero completed FRUCHTBAR still skips preprocessing; absent completion refuses modern capability | Prior addressed completion proof reused; new capability rejection test NOT_EXECUTED |
| Occupancy/strict means (V2) | Whole object 0x4000 skips. Means 15/16/31/32 ->fish/empty/empty/grass-eligible | Four scalar height boundaries checked; full fill/occupied write checks NOT_EXECUTED here |
| Unsigned fertility (V3) | Welt 929ee–929f6: low nibbles 5/6/13 ->50/60/130, only latter two >50; high nibble ignored | H; full branch/write model NOT_EXECUTED |
| Different grass/fish writes (V4/V5) | Bound grass code 2: old aux abcdefaa, mean 32 ->aa000220. Fish mean 15 ->abcdef0f and phase ff->80 | H, rechecked against bound getter values; future full-field causal controls, NOT_EXECUTED here |

Next finite implementation files: a focused `wildlife_postload_fill.hpp/.cpp` in
sim_load, its sim test translation unit and CMake source entry; extend a bounded
postload reference/vector/contract only for this proved first-call domain.
Do not wire prepare_wildlife_prefix, normal loader or World in that unit. Controls:
all-zero completed/missing ground; malformed/inconsistent facts rejected before
effects; occupied whole-word preservation; asymmetric quad and 15/16/31/32;
unsigned fertility 5/6/13 including high-bit ground; distinct grass/fish full-word
writes with nonzero old mask/code/phase/anchor; exact draw suffix; owned lifetime,
immutable success/failure and correct dirty digests. An authored grass code 3
and pre-existing fish code/mask must distinguish wrong constant-2 or grass-style
fish writes even though both shipped descriptors' own codes happen to be 2.

Original wrapper ordering remains preprocessing-if-needed ->fill; this capability
admits only the branch with proved precomputed ground. Stop before the separate
navigation/raster callback and whole-island graphics cleanup. c2-zero preprocessing,
V9 sum/clamp, raster consumers, broader catalogue mutation/reuse, global draw
initialization and eventual World/native publication are NOT newly closed.

## 11. Isolated first-call apply — WILDLIFE-POSTLOAD-FILL-1

F1 acceptance before implementation, 2026-11-19. D: rechecked Welt
0x100928a0–0x10092b32/V1–V5 and the bound profile in §10. Extend the existing
reference with 19 HAND-DERIVED full-field fill rows, retaining all 12 GFX rows.
Each cell is object, phase, owner, retained aux, walk code/mask, dx/dy, rotation,
anchor. The engine splits original word bits 8..24 into code/mask columns;
original abcdefaa projects to aux aa0000aa, code ef, mask 1cd. Grass at mean 32
and code 2 yields raw aa000220, or split aux aa000020/code 2/mask 0. Fish mean
15 yields raw abcdef0f, or split aux aa00000f with code/mask unchanged. No raw
walk-code bits are duplicated into the modern aux column.

| Requirement / primary | Positive and nearest wrong interpretation | Planned proof / review disposition |
|---|---|---|
| Presence/provenance, a1815/89565 | Complete zero-valued FRUCHTBAR accepted; absent/incomplete/mismatched scene, header, Terrain or ground refused before writes | Private immutable capability validates against original scene and typed prefix; pending |
| Bound roles/getters, Game 21a88 | Raw HEAD/id/flags agree with registry, mean profile; alternate role 9001 with HEAD code 3 and zero-pair constructor code 4 | Data-defined same seam, not walk_class or constant 2; pending |
| Whole-word occupancy, 92976 | object 0x4000 and existing roles preserve every field | Full-field vectors and actual-code occupancy mutant; pending |
| Means/unsigned ground, 929d9–929f6 | 15/16/31/32, asymmetric 81, low nibble 5/6/13, differing high nibbles | Full-field vectors and actual-code signed-fertility mutant; pending |
| Distinct writes, 92a25–92add | Fish preserves nonzero code/mask; grass clears mask and uses code 3; phase/offset clear, anchor retained | Full-field vectors and wrong fish-writer mutant; pending |
| Owned result/digests | Input success/refusal unchanged, output survives source destruction, repeat/idempotence, independently hashed expected chunk bytes | One owned tile copy after validation; pending |
| Geometry/order | Half-open clipping, empty rectangle/intersection, 1x1 zero border; raw Terrain/ground/spawns/draw cursor retained | Vectors and direct ownership tests; pending |
| Isolation | No production caller and no navigation/raster/GFX call | Static caller search; pending |
