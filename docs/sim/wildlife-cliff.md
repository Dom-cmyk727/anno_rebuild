# Cliff import class — WILDLIFE-CLIFF-1

2026-11-18. D = matching original instructions, P = parsed original data,
C = C++ proof, A = bounded API adaptation, U = unresolved. This contract and
its hand-derived acceptance rows precede implementation. Continue the locally
reviewed [stone](wildlife-stone.md) and [ordered prefix](wildlife-prefix.md).
No World/native/default/tag/registry identity change is authorized.

## Acceptance table

| Primary predicate | Hand-derived positive / counterexample | Proof / reviewer |
|---|---|---|
| D AnnoGame 0x10008f90–0x100091ce: low mode 2, class-test bit 4 clear | Flags 0x12 give the same window-minimum & 0xf0 as 0x3a. Vertices 49,50,63 / 55,57,60 / 61,62,35 give 32, not origin mean 52, raw minimum 35 or max cell minimum 48 | Authored numeric vectors and C++; pending |
| D AnnoWelt 0x1007c955–0x1007cb3d; 0x1007cbdf | Same per-world-block coverage and group 0x141 level bypass as stone. Low height 15 -> 0 accepts; each crossed chunk still needs a writable tile | 3 × 3 / nonsquare rotations, kept-anchor control; pending |
| D AnnoGame 0x10008923–0x1000893e reads EXISTING flags | Stone 0x3a has bit 0x20, cliff 0x12 lacks it. A later ordinary kind 225 resource passes over stone but refuses over cliff without a draw | Install both through legal import, then attempt identical resource; intended wrong-bit mutant; pending |
| D AnnoGame 0x10008a14–0x10008a28 checks INCOMING group then existing group | Cliff/stone in group 0x141 may overlap another group 0x141, subject to coverage. Cliff after ordinary resource passes its existing 0x20 shortcut | Paired reverse orders, exact accepted/occupied/coverage outcomes; pending |
| D AnnoWelt 0x10082184–0x100823e2: INCOMING bit 0x10 selects overlay, existing bit 0x10 plus anchor selects skipped object write | Existing cliff anchors retain id/phase/height/offset/anchor while code changes and a sparse walk mask ORs. Other tiles get first actual write per block | Exact touched/untouched fields and input immutability; pending |
| D absence of bit 0x08 on this path | Admission, compatibility, height, wrapper and writer read no flag-0x08 predicate. Walk-code getter reads byte +0x75, mask getter returns +0x88, not type flag word +0x12 | Finite primary spans / targets listed below; pending |
| P real predecessor boundary 37/1/type 1552 | Process in the same HHbb raw stream with stamp 500/mode 0/draw 7, no cliff resource draw; stop next unsupported effect/class | Independent raw-order reference then Debug real adapter; pending |

## Narrow supported path and differences

D: support only exact flags 0x12 or the predecessor 0x3a with kind group
`((kind - 1) & ~31) + 1 == 0x141`. No content-id test. A nonsquare authored
companion must take the same new path without an engine edit. All predecessor
ordinary/stone behavior and the original coverage/offset domain remain.
No flags are rewritten in the registry or normalized when testing existing objects.

D: the height branch masks mode with 3 (0x10008f9f / 0x100090e0), selects
minimum quantized cell minimum for mode 2 (0x10009101–0x10009109), and only
executes class tests when `(combined_flags & 0x1000004) == 4`
(0x10009111–0x1000911a). Both 0x12 and 0x3a select mode 2 and no class test.
Island coverage reads combined bit 0x10, not 0x08; its mark/retained-anchor rule
and final group bypass are identical to the [stone contract](wildlife-stone.md).

D: compatibility first reads the EXISTING object's getter +0x60
(0x10008923–0x10008931), returns true immediately only for bit 0x20. Without it,
an incoming kind-225 resource has group 0xe1, which falls through false at
0x10008a3e. Incoming group 0x141 instead reaches 0x10008a14–0x10008a28 and
requires the existing group to be 0x141. Thus cliff after resource succeeds,
resource after cliff refuses; stone after resource and resource after stone
both pass. Same-group compatibility does not waive coverage: a footprint made
entirely of kept overlay anchors still has no writable chunk mark and refuses.

D: original wrapper 0x1008ec90–0x1008ee59 / lower writer
0x10082040–0x10082b0a use bit 31 / +0xc8 / incoming 0x10 / existing 0x10 and
anchor; none selects a flag-0x08 arm in this fresh observer-inactive import
profile. Existing addressed type vtable 0x1002185c resolves +0x94 to
0x10007dc0–0x10007ea5 (walk-code byte +0x75 and rotation only), +0x84 to
0x10007f10–0x10007f16 (returns +0x88 WALKMASK storage). These do not read
flags word +0x12. The existing mask sampler and writer remain unchanged.
No additional flag-specific PRNG call is introduced by flags 0x12. The internal
writer callbacks still execute; their derived-cache effects are projected out as
specified in the dated correction below, not bypassed by the wrapper.
The missing bit 0x20 is observable in compatibility, so it must not be erased
merely because height and immediate writer selection match.

D/C: overlay stores the same tile object word, no second base-object plane.
Reuse the existing writer, Session ownership/index and overlap predicate.
A cliff is nonresource (group 0x141, outside kinds 225–256): no selector draw,
no variation update. Partial accepted writes remain distinct from a whole
admission refusal. The predecessor's scope excludes larger offsets/mark-layout
semantics and active graphical caches; no storage redesign is implied.

## Continuation boundary

Predecessor input: Tutorial/01Discover01, island ordinal 0 / slot 11,
GELAENDE header 37 record 1, type 1552, KLIPPE kind 322, flags 0x12,
3 × 3 at world (54,61). It follows 258 accepted records / 255 resource draws /
3 stones / 4 completed header epilogues / 0 spawns. Use only the corrected HHbb
layout (type/state/signed offsets); preserve the earlier HbbH failed evidence.
The next measured boundary and counts will be appended after one continuation.

## Measured continuation and causal result

P/C: independent corrected HHbb parsing plus original `place()` and addressed
ordered writes predicts, and the direct original adapter case confirms:
2,387 completed / accepted 2,387 / refused 0 / resource-class 2,333 /
resource draws 2,333 / 29 stones / 25 cliffs / 0 spawns. Nine GELAENDE epilogues
33–41 finish; header 42 is partial and its callback is absent. The next boundary
is island ordinal 0/slot 11, GELAENDE header 42 record 39, type 1512, FELSEN
kind 321, flags 0x8018, 2 × 1 at world (59,90). It is unsupported, not an
original rejection; do not add its different low mode or upper bits here.
HIRSCH header 71 remains unreached.

P/C: former cliff 37/1 accepts at (54,61), rotation 3, computed height 32,
9 writes, no draw/variation (cursor stays 255). Subsequent partial overlays
retain and replace cells within that footprint; all nine final ids, phases,
heights, offsets, rotations and anchors are asserted against independently
parsed ordered-write expectations. The first resource remains variant 6 /
height 48 / one draw; predecessor stone 35/22 remains asserted as well.
Sampled stamp 500/mode 0/draw 7 are supplied inputs, never a full-world cursor.

C: nine hand-derived rows extend the same resource reference (35 total), with
nonflat/low cliff, resource-versus-cliff and stone control, reverse order,
same-group partial writes in both orders, coverage and bounds. Direct C++
checks add 3 × 3 plus nonsquare/all rotations/chunk crossings, sparse OR and
retained fields. No predecessor vector is rewritten.

C: one actual private production-TU mutation changes the existing-object
compatibility test from bit 0x20 to either 0x20 or 0x10. Legal cliff setup and
stone control still execute. It then wrongly accepts the later ordinary resource,
changes object/phase/variation and consumes one draw instead of zero: five
intended assertions fail. Canonical source remains unmodified; the shared
compatibility implementation itself did not need a production change.

The first new Python imported-module mutation check failed because it reused
same-size/timestamp bytecode instead of executing the modified predicate.
The private failure and a bounded reproduction are retained: legal 0, cached
wrong predicate still 0, then actual wrong predicate 1 after cache removal.
The corrected CLI contract uses `-B` from the first child in a fresh temporary
directory, so no bytecode cache is created for later mutation. All 21 controls
then pass with actual output disagreement under the wrong compatibility bit.
This is a test-isolation correction, not an engine behavior change or hidden
negative-control success.

## Writer callback correction (2026-11-19)

D: the prior 0x10082637 endpoint was only part of the writer. The full function
ends at `ret` 0x10082b0a and calls island +0x19c at 0x10082ab9 and +0x188 at
0x10082ad8. Concrete targets are 0x10084150 (derived chunk-height bounds) and
0x1008f2d0 event 0xcb -> 0x1008f2f0 (chunk dirty byte 5 increment). These execute
inside the writer even with the separate wrapper observer bypass. Neither changes
the represented tiles/variation or consumes RNG in the checked profile.

This transient result projects tile/variation/draw effects and omits those derived
chunk-cache updates. A future World/render consumer must obtain their correct
initialization/rebuild under a separate reviewed integration contract before using
them. The original complete writer state is not reproduced. Empty-tile fill
0x100928a0 is separate slot +0x1a0, not the +0x19c refresh. See the full
[addressed correction](wildlife-rock.md#writer-callback-correction-2026-11-19).
