# Stone import class — WILDLIFE-STONE-1

2026-11-18. D = matching original instructions, P = parsed original files,
E = original routine on authored emulator inputs, C = rebuild test, A = explicit
bounded adaptation, U = unresolved. This contract precedes implementation.
The reviewed [prefix](wildlife-prefix.md) remains the predecessor evidence.
No World/native/default or state identity changes belong here.

## Acceptance and primary predicates

| Requirement / primary evidence | Hand-derived positive | Distinguishing wrong reading | Proof / review |
|---|---|---|---|
| D AnnoGame 0x10008fe2–0x10009109, rotated dimensions and mode 2 | 2 × 2 vertices 49,50,63 / 55,57,60 / 61,62,35 give minimum quantized cell minimum 32 | Origin mean 52, maximum-cell-minimum 48, or unquantized 35 differs | Authored vectors, primary-byte derivation, C++; pending |
| D AnnoGame 0x1000910f–0x1000911a, class check | Flags 0x3a have bit 4 clear, so nonflat terrain does not invoke flatness test | Treating this like ordinary class 4 rejects legal slopes | Nonflat positive; pending |
| D AnnoWelt 0x1007c790–0x1007c955 and AnnoGame 0x10008920 | Full rotated footprint in bounds and compatibility checked on every occupied tile | Origin-only check misses a blocked far corner | All rotations/nonsquare/bounds/occupied controls; pending |
| D AnnoWelt 0x1007c955–0x1007cb3d | Each touched world-aligned 8 × 8 chunk needs one tile that is not an existing overlay anchor | One writable tile anywhere is insufficient across a chunk boundary | Coverage refusal versus partial-write positive; pending |
| D AnnoWelt 0x1007cbdf–0x1007cc17 | Kind group 0x141 bypasses final 32 gate, so computed height 0 can accept | Reusing ordinary final threshold refuses | Low-height positive; pending |
| D AnnoWelt 0x10082184–0x10082289 | Overlay skips existing bit-0x10 object with anchor set; first actually written tile per block becomes anchor | Atomic rejection or clearing anchor after skip differs | Exact object/offset/phase/height/anchor fields; pending |
| D AnnoWelt 0x100822cd–0x100823e2 | Code and OR-mask update every footprint tile, including skipped anchors | Skipping all writes on retained object, or replacing mask, differs | Preserved object/phase/height with changed code and OR-mask; pending |
| D AnnoWelt 0x1008941e–0x1008949d | Stone kind 323 is outside resource kinds 225–256: no variation write/draw | Treating every accepted placement as resource consumes one draw | Supplied cursor/variation unchanged; pending |
| P predecessor boundary 35/22/type 1558 | Process this record in the same unfiltered stream; stop next unsupported record | Skipping records or success without preceding effects is invalid | One original Debug continuation; pending |

## Bounded supported class

D/P: extend only exact flag word 0x3a with kind group
`((kind - 1) & ~31) + 1 == 0x141`, nonzero dimensions/valid rotation.
Content id is never part of the predicate; another authored kind in that group
must take the same path. Other flag classes remain unsupported. Original checked
coordinates/footprint-offset restrictions from the resource contract remain.

D: for mode 2, compute each footprint tile's minimum of four unsigned corner
bytes, clear its low nibble, then take the minimum of those values. This equals
minimum of all footprint-window vertices with low nibble cleared. This is not
an average, maximum, or nearest rounding. Flags 0x3a do not select the separate
class-test bit 4. Group 0x141 bypasses only the final level gate, not bounds,
compatibility or coverage. The existing original `place()` Python reference
already supports these admission rules; do not use BuildCmd `engine_place()`.

D: coverage excludes a tile only when it has a nonzero object whose type has
flag 0x10 **and** its byte-3 anchor bit 0x80 is set. Every touched world chunk
must contain a nonexcluded tile. Original mark storage is a 2 × 2 array with
row stride 2 (0x1007ca4f, 0x1007cb32). Footprint edges above the original block edge also exceed the writer's 3-bit
per-tile offset domain. Larger footprint/touched-chunk domains are explicitly
unsupported here rather than reproduce out-of-array reads. This is a bounded
original-layout domain, not a world or engine-content limit; extending it needs
a separate explicit adaptation. The implementation can derive each chunk's
coverage without a persistent array or island-size cap.

## Representation and write effects

D: original overlay stores its id in the same word at tile +0 as ordinary
placement (0x10082215 versus 0x1008244a); it does not keep a second base object.
Tile storage is island +0x0c with 8-byte stride (0x100821b2–0x100821e6).
Thus the existing TileLayer columns and `place_object_record` represent this
path; no additional transient or saved overlay plane is justified.

D: in each world-aligned block, scan rows then columns; skip an existing overlay
anchor without clearing the new block's first-write marker. Written tiles get
id, origin offsets, rotation, raw phase & 63, computed height, and first-write
anchor. Byte-3 bit 6 and second-word bits 30–31 clear on a write
(0x1008226d, 0x1008227b); these are already zero in this fresh supported scratch
profile. Owner/claim and the other represented packed fields survive. Skipped
tiles retain all those object fields. Independently, every footprint tile gets
the placed type's rotated walk code and OR of sampled mask bits. The existing
lower writer has that exact block/skip/OR behavior; use it unchanged.

D: import wrapper 0x1008ec90 takes bit 31 clear and calls lower writer 0x10082040.
The fresh receiver's +0xc8 remains zero, so 0x1008eccb–0x1008ecd3 bypasses chunk
observer callbacks. Import flag 0x04000000 bypasses the final getter at
0x1008ee33–0x1008ee52. The lower writer's calls are data getters and the already
addressed walk-mask sampler, no world generator. Header water-mask epilogues
remain separate and execute only after a completed GELAENDE header.

## Continuation and limits

The predecessor diagnostic ends before GELAENDE header 35 record 22, type 1558,
kind 323, flags 0x3a, 2 × 2, after 37 accepted resource records and 37 draws.
The continuation must preserve stamp 500/mode 0/supplied draw 7 and the first
resource witness variant 6/height 48/draw 1. Counts and next unsupported boundary
will be recorded after execution. No original-world cursor alignment, complete
World load, storage/consumer integration or lifecycle claim follows.

### Measured continuation (2026-11-18)

P/C: the same raw Tutorial input now processes 258 records, all accepted:
255 resource-class records / 255 resource draws / 3 nonresource stone records /
0 spawns. Four GELAENDE epilogues (33–36) finish; header 37 stops before its
callback at record 1, type 1552, KLIPPE kind 322, flags 0x12, 3 × 3. This next
class remains unsupported. HIRSCH header 71 still has not been reached.
An independent raw-order parse composed with original `placement_ref.place()`
reproduces the counts, boundary and writes (private receipt, no C++ output input).

P/C: predecessor record 35/22 (1558) accepts at (88,72), rotation 0, computed
height 48, four writes, no resource draw and no variation. Later ordinary
resource records replace its top row with types 115/68. Its two bottom cells
retain type 1558, phase 19, height 48, offsets (0,1)/(1,1), rotation 0 and clear
anchors. The real test asserts this final state, not just an Accepted label.
Two further stones (type 1549) use the same facts path; the last has computed
height 0, independently exercising group 0x141's level bypass in original data.

C: all 20 predecessor resource rows remain byte-for-byte unchanged; six authored
stone rows extend the same reference/vector mechanism. Optional authored anchor
inputs enable partial/coverage controls; legacy reference inputs/outputs remain
unchanged. New cases are nonflat min/rounding, a second content-defined kind,
partial overlay, coverage rejection, far-corner occupancy and full bounds.
Nonsquare rotations/chunk boundaries and sparse OR versus replacement are
additional direct C++ controls. A private production-TU wrong-writer overlay
removes only the writer's overlay flag after legal admission; control passes,
mutant fails retained-object/anchor/OR assertions. Canonical source is untouched.

NOT_RUN here: original-runtime/History Edition and fresh original-admission
emulation (the current Python has no unicorn); the new predicates were re-read
from matching DLL instructions and hand-derived before implementation. Existing
emulation evidence for original `place()` is inherited, not relabelled a new run.
