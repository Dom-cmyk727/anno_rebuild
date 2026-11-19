# Transient BERGWERK2 resource-list import

WILDLIFE-BERGWERK-1, 2026-11-19. Scope: the next ordered local header in
[the wildlife prefix](wildlife-prefix.md), not deposits/extraction or World
publication. D = GOG AnnoWelt instructions, P = parsed original file, C = authored
execution. Independent review pending. Primary listings stay private.

## Contract established before code

D: island dispatcher `0x10089d37–0x10089d7b` calls island +0x24 container
slot +0x18, then only updates consumed bytes and returns. Fresh construction
`0x1008c24b–0x1008c257` uses +0x1ac -> `0x1008be60`, constructor
`0x10078c80–0x10078cf1`, table `0x100c9b10`, reader `0x10078960–0x10078aa3`.
Container origin/size are copied from island slots +0x50/+0x54/+0x58/+0x5c.
This is NOT island +0x28 OCCUR/container `0x100caf34`, an ore occurrence,
quantity, reserve, goods, placed marker or a mine's later extraction binding.
See [salt §24.2](salt-extraction.md#242-the-load-order-a2) and
[mine follow-ups](../re/mine-deposit-followups.md). Accepted ANNOSCENE-ORE-2 at
`b12d22b39caee90429aef77b52659aa2bfedb38e` likewise distinguishes these objects;
its assignment/quantity algorithms are not part of this header.

D: the payload is the existing small tagged-record sequence, not generic
12-name/u32 chunks: reader `0x100b4f50–0x100b4fbc` reads a 6-byte name and u16
length, with the existing 24-bit long form for length >= 0xff00. The header loop
`0x100789b0–0x10078a4b` processes source order, skips unrecognized names, and
handles ENTRY via `0x100b4fd0–0x100b4fef`. There is no count/capacity field.
Each supported ENTRY is one u32 LE. At `0x100789ef–0x10078a2e`:

- local x = bits 0–11; local y = bits 12–23; rotation = bits 24–25;
- upper bits 26–31 are ignored, not flags/quantity; coordinates add the island
  u16 origin with u16 wrap, then widen into the prepared model;
- insertion input has world u16 x/y, u8 rotation and mask 0x10. Unknown tagged
  records skip their payload, not a semantic unknown ENTRY value.

D: insertion `0x10077e30–0x10077fc9` uses footprint 3 × 4 for rotations 0/2,
4 × 3 for 1/3 (table `0x10077fcc`). It visits world-aligned 8 × 8 chunks in y/x
order from floor(x/8)*8 / floor(y/8)*8 while base < footprint end. The chunk
indices are C truncating division `(base - island_origin) / 8`, not assumed
local floor division; matrix width/height are ceil(island dimension / 8), proved
by allocation `0x1008c2ed–0x1008c39e`. Defensive checks reject an out-of-matrix
index before an ENTRY effect; no original clipping or safe malformed I/O claim.
The 8 is original chunk geometry, not an island/object-count capacity.

D: per visited chunk, allocation-success constructor `0x1007c290–0x1007c2cc`
creates a 12-byte resource node: next +4 initially null; +8 packs signed nibble
anchor offsets `(x-base_x)&15 | ((y-base_y)&15)<<4`; +9 is rotation; +0xa mask
is overwritten to 0x10; +0xb kind is 1. `0x10077f74–0x10077f87` sets that mask,
prepends through `0x1007bd50–0x1007bd60` / concrete +8 setter
`0x10040230–0x10040237`, and ORs chunk byte +8 with 2. Fresh chunk constructor
`0x1007c730–0x1007c742` / `0x1007c710–0x1007c728` clears head and flag word.
The represented resource-list presence bit is distinct from omitted derived
height/dirty caches in the [corrected ROCK contract](wildlife-rock.md).

D: duplicates append new nodes and prepend each to the same chunk; no reset,
merge, deduplication, object factory, tile/variation write, callback or world
PRNG call is reached in this finite chain. Allocation failure is an original
failure, not success; the transient API retains its existing allocation-success
domain. Post-island callbacks remain outside the ordered prefix.

## Acceptance written before implementation

| Requirement / evidence | Hand-derived control | Status |
|---|---|---|
| ENTRY widths / rotation / upper bits | local (7,6), origin (16,24), rotation 1 with upper 6 bits set -> world (23,30), never quantity/flags | pending |
| Chunk footprint / offsets | That 4 × 3 footprint crosses 4 chunks: indices 0,1,2,3; offset bytes 0x67,0x6f,0xe7,0xef | pending |
| Prepend and duplicate chronology | Repeat ENTRY in later header -> 8 nodes, heads 4,5,6,7 with next 0,1,2,3, not overwrite/dedup | pending |
| Tagged format reuse | Short/long ENTRY, ignored other tag, empty header, authored encode/decode byte round-trip | pending |
| Defensive errors / ownership | Short ENTRY, corrupt long lengths, trailing fragment, out-of-matrix coordinate; no completed marker, prior prefix retained, output owns values | pending |
| Surrounding effects / no draws | Resource–BERGWERK2–fertility–BERGWERK2–HIRSCH preserves two supplied draw assignments and all tile/variation fields | pending |
| Causal production mutation | Legal public adapter control versus wrongly reading x from the 12-bit y field | pending |
| Original once | Independently parse 9 entries -> 20 nodes in 8 chunks, preserve earlier prefix; stop OCCUR header 51 | pending |

## Representation and defensive adaptation

Use existing `formats::tagged` parse/write helpers; no parallel format framework.
Prepared output owns decoded entries, source ordinals, nodes, heads and the
resource-presence bit. These are transient resource-list effects, not native
state, ore tonnage or a persistent side cache. Original signed nibble packing,
rotation and per-chunk prepend are represented. Chunk geometry comes from island
dimensions using original 8-cell format semantics, independent of the configurable
spawn-head chunk geometry. Other original chunk flag bits/caches remain outside
this representation; `resource_flags` denotes only the BERGWERK presence bit.
No normal scenario loader, mine, registry, default, tag or identity changes.

Exact ENTRY length 4, checked complete tagged sequence and checked matrix indices
are defensive admission rules; original unchecked/truncated I/O is not emulated.
A malformed sequence refuses before its header effects; an invalid later ENTRY
retains prior accepted entries as diagnostic partial, never complete success.

## Authored pre-gate result

Three public-adapter cases pass 400 assertions, including short/long tagged
round-trip, upper-bit masking, all rotations, duplicate prepend, malformed
lengths, signed chunk indexing and source ownership. A private actual adapter-TU
mutation changes x extraction to the y field after the same legal setup. The
named world-x/offset assertions fail, not metadata or prerequisite setup. The
canonical source remains unchanged and its normal target was rebuilt. The
private harness footer retains an inherited whole-byte-copy label; its first
line, actual one-expression replacement and named failures establish this is
only the wrong-axis mutant. No second mutation campaign is implied.
