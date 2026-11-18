# Ordered resource import prefix — WILDLIFE-RESOURCE-1

2026-11-18. D = matching original instructions/data, P = parsed original files,
C = current rebuild, A = explicit evaluator API adaptation, U = not proved.
This contract precedes C++ implementation. It covers one concrete, transient
scratch evaluator, not World import, native state, full original RNG alignment,
animal creation or gameplay defaults. It takes no numeric tag or registry field.

## 1. Finite original receiver and selector

D: GOG module hashes are retained with the private primary receipts; all addresses
below name their module. AnnoFrame `0x10020108`–`0x10020114` requests
CreateAnnoServerInstance kind 0x101 through thunk `0x10077a62` / import 0x10086044.
AnnoGame `0x10013774`–`0x10013789` constructs `0x10007650`, which installs
catalogue vtable `0x100215a4` at `0x10007663`. AnnoFrame
`0x10029199`–`0x100291af` passes that catalogue global into the scene constructor;
AnnoScene stores it at `+0x154` (`0x10002fc0`–`0x10002fc6`) and passes it to world
initialization. AnnoWelt `0x1009d848` stores that argument at world `+0x110`;
world `+0x18` getter `0x100a37d0` returns it. Island initialization stores it at
`+0x1c` (`0x1008c0c9`–`0x1008c0db`). This closes the concrete receiver chain for
this load path, rather than choosing among four matching vtables by appearance.

D: catalogue `+0x88` is AnnoGame `0x10001b30`–`0x10001c78`. It has no world
pointer or additional PRNG draw. Direct type getters `+0x5c`, `+0xac(0)` and
`+0xf8(0)` return kind, byte goods 0 (`0x1000a410`–`0x1000a424`) and a u16 selector
tag (`0x1000a4d0`–`0x1000a4db`). The latter is HEAD u16 offset 0x1a: the HEAD
buffer begins at stack +0x34, `0x10009485` reads stack +0x4e and `0x1000948a`
stores type +0x7e. No unproved semantic name is assigned to that HEAD field.

1. Start candidate id = source type id. Kind 225 with goods 0 in inclusive 56–75
   decrements candidate id (`0x10001b56`–`0x10001b69`). Kind 226 instead reads
   goods 0 of the following id, without changing candidate id (`0x10001b6b`–`0x10001b7e`).
2. When that goods value is 70, read the **original current type's** selector tag.
   Tags 104/105, 201/202 and 404/405 use the respective climate rows at
   0x1002129c, 0x1002130c and 0x1002122c. Each has 8 u16 entries per climate;
   index = low byte of supplied draw & 7 (`0x10001b84`–`0x10001c35`). These are
   tag values, not a whitelist of content type ids. Other tags retain candidate id.
3. Search the selected climate's 16-u16 row at 0x1002114c for the candidate;
   first match returns 0–15, no match returns 0 (`0x10001c35`–`0x10001c78`).
   The helper takes authored/resolved rows as transient inputs, not original arrays
   compiled into source. Climate count and type ids are not engine caps.

D: GELAENDE applies this only to **kind values 225–256**, not type ids
(AnnoWelt `0x1008941e`–`0x1008942b`). Successful placement occurs first, then one
world draw at `0x10089468`, then the selector. An invariant selector result still
costs that draw. The origin byte in the separate island `+0x18` W × H plane
becomes `(old & 15) | ((variant & 15) << 4)` (`0x1008944f`–`0x1008949d`).
The plane is not growth phase and is not spare TileLayer aux bits.

## 2. Supported admission and placement domain

D: original import passes caller flags 0 to island `+0x6c` at `0x100893f3`.
It does not use the rebuild's BuildCmd eligibility gate. This bounded evaluator
supports the ordinary one-quad mode with type flag words 0x09 or 0x29: low mode
1, no coverage/overlay, no class-specific height test. Other classes explicitly
refuse as unsupported; they do not become successful zero-draw records.
Type ids and kind values are data, including a second independently defined type.

For this domain, original `0x1007c790` checks the full rotated footprint bounds,
then current-scratch overlap via the type compatibility predicate. Reuse
`site::footprint_inside` and `site::footprint_occupancy` after validating existing
types. Reuse their documented original compatibility, never `placement_rule` or
`engine_place`. Unknown type/fact semantics are errors, not a fabricated Boolean.

D: type one-quad height analysis averages the four unsigned origin corners,
truncating / 4 (AnnoGame `0x10008fa1`–`0x10008fd9`); the final ordinary threshold
is 32 (AnnoWelt `0x1007cbeb`–`0x1007cc03`), except group 0x141 bypasses
this final gate at `0x1007cbdf` (preserved for nonresource inputs). Mean 31
rejects and 32 accepts in the ordinary group. Even for
a wider footprint this mode samples the origin quad, while bounds and overlap
cover the whole footprint. VertexGrid supplies the proven zero border row/column.
The computed mean replaces descriptor byte 8; a file-supplied height is not trusted.

D: accepted import passes 0x04000000 to `+0x78` (`0x10089404`–`0x10089417`).
Wrapper `0x1008ec90` calls the ordinary writer `0x10082040` when bit 31 is clear;
it does not select a different placement algorithm for 0x04000000. The lower
writer tests bit 31 at `0x100820d2`; type flag 0x10 selects overlay, absent here.
Reuse `place_object_record` for id, footprint offsets/rotation/anchor, walk code,
mask, **phase = raw state & 63**, and the admission-derived height. It writes no
variation plane. Nonresource placement leaves that plane alone.

Fresh tile-only profile: constructor `0x10094c60` → `0x1008f460` zeros word +0xc8
at `0x1008f46e`. Earlier HEIGHT/GFXID notifications 0xc9/0xca dispatch through
`0x1008f438` to `0x1008f312`, updating chunk dirty bytes, not +0xc8. The GFXID
copy `0x10085060`–`0x10085177` makes no PRNG draw. With +0xc8 zero and load bit 31
clear, wrapper `0x1008eccb`–`0x1008ecd3` skips the active chunk-representation
callbacks. With 0x04000000 it also skips the final type-flags getter
(`0x1008ee33`–`0x1008ee52`). Active graphical chunk caches/observers are outside
this scratch evaluator; no general callback draw closure is claimed for them.

## 3. Concrete transient API and errors

Inputs: immutable TileLayer, matching readable VertexGrid, separate variation
bytes of exactly tile_count length, immutable TypeRegistry for placement facts,
explicit transient selector metadata (id, goods-0 byte, HEAD tag word), resolved
climate rows, ordered original records (type, world origin, rotation, raw state),
and bounded raw u32 draws. No input is deduplicated or mutated. The caller
resolves the climate and supplies only that climate’s rows; selection cannot
silently fall back to another climate. Following-id
metadata is required when the selector actually reads it; missing facts refuse.
No registry field/digest changes are needed to carry these transient facts.

Output owns a copied TileLayer, separate variation vector, per-record accepted
or ordinary-rejected dispositions and exact consumed count/order. Later records
read prior accepted changes. Only an accepted resource consumes a draw and updates
its origin variation byte; rejected and nonresource records consume none.

A: malformed geometry, missing/invalid facts, unsupported semantics or exhausted
draws return an error naming the record and consumed prefix, without a partial
successful result. The input stays unchanged because all writes were scratch-owned.
This transactional API is not a claim that the original loader rolls back.
Counts are bounded by input spans and actual tile geometry, never a world cap.
The supported lower-writer domain has nonnegative world origins, all addressed
world tiles at most INT32_MAX and footprint offsets at most UINT16_MAX. The
original source coordinates are unsigned. Negative-origin placement is not
claimed: the existing writer starts signed block iteration toward zero and can
omit a negative leading cell. Unproved wider domains refuse;
there is no silent truncation.

## 4. Proof and integration boundary

Before C++, author independent selector/ordered-prefix examples, including two
draw-sensitive outcomes and a second content-defined resource. Numeric expectations
come from this addressed contract and original place(), never engine_place or C++.
Controls distinguish a skipped accepted draw, wrong high/low-nibble write, use of
raw file height, phase corruption, and later admission ignoring previous writes.

P/D: the bounded real proof ends after Tutorial/01Discover01's header 34 first
resource: type 70 / kind 226, local (59,29), island origin (24,24), corners 48,
climate 2. Its selector tag is 0 and following id supplies GRAS; candidate 70 is
at first variation slot 6 in that runtime-read climate row. Hence one draw is
consumed even though variant is 6 for different supplied draw values. Only this
verified prefix is claimed; 4,589 candidates are not an actual-draw census.

No World caller, default switch, persistence layout, numeric tag, registry digest
or pin changes belong here. The next integration must coordinate storage for this
separate plane and spawn records, consume the proved prefix before jitter, retain
feature-off identity, and resume native state without rerolling. Resource deletion,
growth, rendering, actors, fauna materialization and complete global load order
remain separate work.

API failure timing: geometry/row-shape/duplicate metadata/unknown initial objects
refuse before work. Missing current/following selector metadata or a required
tag row refuse at the selector, **after** the accepted record consumed its draw;
`Error::consumed` reports that prefix. Exhaustion occurs before consuming an
unavailable draw. No partial Output is returned in either case.

Production metadata dependency: the current registry drops WARE `UNUSED`, so an
empty `output_goods` is not proof of the original catalogue byte. The bounded
real test requires the raw parsed WARE field to be present and exactly `UNUSED`
(`UNUSED = 0`, [Anlagen format](../formats/anlagen.md)). It supplies that explicit
transient fact without changing registry decoding or its digest. The following
id's goods must also be present/resolved. Future production integration must
provide these facts explicitly rather than default an empty/missing field to 0.

See [original placement](../re/ship-placement.md),
[terrain view](../../src/sim/include/anno/sim/terrain_view.hpp),
[helper API](../../src/sim/include/anno/sim/resource_import.hpp) and
[authored vectors](../../fixtures/synthetic/sim/resource_import_vectors.txt).

## 5. Reviewed correction: prepared metadata (2026-11-18)

C/A: prepare a private id-sorted copy once per evaluator invocation. Validate
climate row shape/ids and adjacent metadata duplicates once, then use binary
lookup in the internal selector. Metadata work is O(F log F + R log F) for F
facts and R accepted resource records, instead of repeated O(R F²) validation.
The standalone public selector prepares/validates its own invocation. This is
transient indexing only; caller order and input ownership stay unchanged.
Missing current/following metadata still refuses after the accepted draw, while
invalid/duplicate facts refuse before work. Original selection/placement/draw
semantics and every supported-domain restriction above remain unchanged.

A bounded many-facts test uses the existing test-only allocation probe to
distinguish one owned index from zero (the old scan) or per-resource rebuilding;
production exposes no counters. Standard sort and binary-search structure bound
comparison work without a wall-clock threshold. The zero-sentinel regression
registers both ordinary id 0 and its legal nonzero sibling, so missing-type
refusal cannot substitute for the explicit zero guard.

## 6. Narrow stone continuation (2026-11-18)

[WILDLIFE-STONE-1](wildlife-stone.md) adds exact flags 0x3a / kind group 0x141
as a separate evidenced admission arm: mode-2 minimum quantized window height,
full overlap plus per-world-block coverage, then the existing overlay writer.
Its nonresource placements draw nothing and leave the variation plane alone.
The ordinary 0x09/0x29 contract and selector are unchanged. Other classes remain
explicit unsupported boundaries; this does not extend BuildCmd or World import.

## 7. Narrow cliff continuation (2026-11-18)

[WILDLIFE-CLIFF-1](wildlife-cliff.md) adds exact flags 0x12 / kind group 0x141
to the same mode-2 coverage/overlay arm. Original flags remain intact: a cliff's
missing existing-object bit 0x20 makes later ordinary resources incompatible,
while same-group overlays may pass. No compatibility predicate, selector,
variation, header callback, ordinary/stone path or World wiring is changed.
