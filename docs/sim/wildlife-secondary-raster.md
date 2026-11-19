# Secondary postload raster contract — 2026-11-19

WILDLIFE-RASTER-CONTRACT-1. Research only; no production raster, route, World,
identity or default change. D = original instructions, H = independent hand
derivation, C = executed private model, U = not proved. Review pending.

## Acceptance and examples before executable comparison

| Requirement | Primary proof / hand expectation | Discriminating control | Status |
|---|---|---|---|
| Numeric producer | Welt 0x1007c760–0x1007c780 repeatedly adds f32 0.04, stores 384 floats | Byte-height query uses only entries 0..255; establish startup ordering, not a dumped table | D closed; runtime FP qualification below; review pending |
| Exact low merge | Welt 0x100a3e7d–0x100a3e90; converted 247 ->15, 248/255 ->16 | Old 0x80 becomes 0x90 for 248/255, not 0x80 | D/H/C; wrong mask fails 4 rows; review pending |
| Retained high bits | Old 0xa5 with converted 32 gives 0xa2 (arithmetic-only owner value) | Wrong low-nibble mask of rounded value must fail carry, ordinary control still agrees | D/H/C; review pending |
| Ordinary original-profile sample | Scale 1, one height-100 corner and three sea samples: table[100] = f32 4.0, normalized conversion truncates to 25, shade 2 | Full four height-100 samples give converted 100, shade 6 | H/C under explicit FP profile; review pending |
| Bucket and empty semantics | World 1024 / raster 256 gives scale 4; rect [5,6,9,10) selects [1,1,3,3) buckets | Empty [0,0,0,0) selects none but generation 7 ->8 under flags 2 | D/H/C; wrong empty increment fails 1 row; review pending |
| Nonaligned empty rectangle | Same scale, rect [1,1,1,1) still selects bucket [0,0,1,1) | No early generic empty-rectangle return is allowed | D/H/C; review pending |
| Border normalization | Upper sample endpoints clamp to world size; denominator remains nominal 4*sx*sy, never actual sample count | Last scale-1 corner pixel has one sample, not four | D/H/C; review pending |
| Reachable converted-byte wrap | World 800, raster 256, bucket (6,6), scale 3.125: 7 × 7 height-255 samples / nominal 39.0625 -> converted 319, byte 63, rounded 4 | Do not clamp conversion to 255 | D/H/C supported authored sample profile, not an original runtime observation; review pending |
| First publication reader | Frame attach reads raw plane pointer and generation, then regenerates minimap texture without needing a world tick | Trace actual pointer/generation use; no renderer-only/cache exemption claim | D reader and publication edges closed; broader indirect consumers explicitly bounded below; review pending |

H: merge-only thresholds 7/8 ->0/1, 23/24 ->1/2, 247/248 ->15/16;
converted 256 wraps to 0 before rounding. The last case is arithmetic-only until
its sample profile is separately established. Old high nibble survives by OR,
but bit 4 can additionally be set. Flags 0/2 give generation delta 0/1;
both bit arms give delta 2. These expectations precede the private model.
An arithmetic scratch check corrected the initial shade-100 transcription from
7 to 6 before executable comparison: (100 + 8) / 16 truncates to 6. This was
a hand-expectation typo, not an observed original or engine defect.

## 1. Numeric source, initialization and qualification

D: AnnoWelt 0x10084c20–0x10084c51 converts world coordinates to an island corner
index, reads an UNSIGNED byte and returns the float at runtime table
0x100e2d90 + 4*byte. World 0x1009db00–0x1009db68 resolves the island from the
world's chunk lookup; no island returns float zero at 0x1009db4f. This is a
vertex sample, not a tile mean, object-top query or coarse-navigation record.

D: initializer thunk 0x1007c750 ->0x1007c760–0x1007c780 loads zero, keeps an
x87 accumulator, stores it as f32, adds the f32 constant at 0x100c9c20 and
continues at 4-byte offsets below 0x600. It creates 384 entries, NOT merely 256.
Byte-indexed callers here use only entries 0..255. The constant is exactly
5368709/134217728 (the f32 labelled 0.04); it is not the rational 1/25.
Store rounding does not reload/round the retained accumulator after each addition.

D: the thunk's pointer is in the DLL initializer array at 0x100d0d64. Entry
0x100b6b78 invokes process-attach CRT setup 0x100b6a9f before user DllMain
0x100b22b0. At 0x100b6b27 CRT calls 0x100b921a, whose 0x100b9234–0x100b923e
walks [0x100d0000,0x100d0f20) through 0x100b931e–0x100b9337, invoking nonnull
entries in ascending pointer-slot order. Thus this production table initializer
precedes the later world factory and per-island postload callbacks; it is not a
late island-load reconstruction from uninitialized data.

D: before that array, 0x100b921a–0x100b9223 invokes the nonnull hook stored at
0x100dd490, initially 0x100b5f30. Its 0x100b5f3f calls 0x100b6e12, setting
controlfp value 0x10000 under mask 0x30000. The mapping at 0x100ba502–0x100ba516
turns that into x87 precision bits 0x200 (53-bit). It leaves rounding-control
bits outside that mask unchanged. The only resolved direct call to that controlfp
wrapper is this reset; this narrows the earlier CAP-2 startup unknown, not every
runtime/thread control-word question.

D/search scope: the complete executable-section and HIGHLOW relocation scans
over [0x100e2d90,0x100e3390) resolve 38 indexed/absolute memory references;
the only resolved store is 0x1007c768. The thunk's only resolved absolute
address use is the initializer-array slot; its direct edge goes to 0x1007c760.
No runtime table-configuration writer was found. These scans do not prove the
absence of arbitrary computed aliases, external writes or injected code. DLL
reload runs its initializer again; world create/load does not itself write the
table in the inspected paths. No adjustable table scale was established.

H/C profile: the private models assume 53-bit arithmetic, nearest-even rounding
and the explicitly observed f32 stores. At that precision i*constant for i<384
is exactly representable before each f32 table store. Table[100] rounds to 4.0;
table[255] is 10695475/1048576. Runtime raster sums store f32 after EACH sample,
so integer sum-of-height-bytes divided by count is not a proved substitute.
U/CAP-2: an observation/closed caller contract for the active rounding/precision
at 0x100a3cce, and any later changes on the load thread, remains needed before
claiming a generally exact integer implementation. The helper selecting truncation
for integer conversion restores the prior control word; it does not settle the
earlier floating arithmetic. This packet chooses no replacement arithmetic policy.

**FP follow-up, 2026-11-19:** the [two-checkpoint audit](../re/wildlife-raster-fp.md)
now separates table initialization from first-load callback precision/rounding.
Four independent sensitivity pairs change results, and one pair of distinct
profiles cancels to the same output. Full original CW/thread closure remains U;
the [bounded Windows card](../testing/wildlife-raster-fp-capture.md) records both
contexts without mutating the FPU. The earlier 31 modeled checks remain valid
under their named profile, not promoted to original-runtime evidence.

## 2. Flags-2 update, rectangle and generation

D: concrete table 0x100ccd40 has slots +0/+4 returning raster dimensions
(0x100afa40/0x100afa50); +8 returns plane +0x14 (0x100a3cb0); +0xc updates
(0x100a3cc0); +0x10 returns generation +0xc (0x100b4b10); +0x14 destroys
(0x100a3c90). These are a resolved interface, not a name inferred from a string.
The HE-derived label tWeltServer::tKarteObject is already documented in
[UI §2.9](../re/ui.md); no HE binary was read here.

D: standard world Init allocates 24 bytes for this object at
0x100aaa53–0x100aaa80, passes world plus raster 256 × 256 to 0x100a40d0,
and stores it at world +0x120 at 0x100aaa8f. Its own dimensions are parameters,
not an engine cap. 0x100a40d0–0x100a412b retains world at +0x10, dimensions
at +4/+8, allocates their product bytes at +0x14, fills ALL bytes with 0x90
and sets generation +0xc to zero. Allocation failure returns the original error,
not a partially initialized published raster.

D: 0x100a3cc3 tests flag bit 2 first. World slots +0x54/+0x58 supply W/H;
the callback stores `sx = f32(W / raster_width)`, `sy = f32(H / raster_height)`
at 0x100a3cee–0x100a3d10. For nonnegative valid rectangle [x0,y0,x1,y1),
0x100a3d14–0x100a3d74 derives bucket bounds:

```text
bx0 = trunc(x0 / sx); bx1 = trunc((x1 + trunc(sx) - 1) / sx)
by0 = trunc(y0 / sy); by1 = trunc((y1 + trunc(sy) - 1) / sy)
```

The divisions use the stored f32 scales and existing truncation helper. There is
no generic ceil, explicit bucket clamp or input-rectangle clipping INSIDE this
method. Caller-provided bounds therefore matter. The private legal profile uses
0 <= x0 <= x1 <= W and corresponding Y, positive dimensions and scales at least
1; other input domains need their original caller precondition, not a guessed
clamp. In particular a logically empty unaligned rectangle can select a bucket.

D: 0x100a3dad–0x100a3e21 computes sample starts `trunc(bx*sx)`,
`trunc(by*sy)` and exclusive ends `min(trunc((bx+2)*sx), W)`,
`min(trunc((by+2)*sy), H)`. Upper sample endpoints ARE clipped to W/H at
0x100a3e01–0x100a3e1f, unlike the bucket rectangle itself. Rows Y are outer,
columns X inner (0x100a3e25–0x100a3e4c); no sample is taken outside those ends.
Each world height-query result is added to the previous f32 sum, then stored
back to f32 at 0x100a3e38–0x100a3e3f. Zero samples leave zero sum.

D: 0x100a3e56–0x100a3e6e divides that sum by nominal `sy*sx*4`, then by
the f32 constant 0.04 at 0x100cccd8, and truncates through 0x100b5f90. The
nominal denominator does NOT shrink at borders. Bucket sample cardinality can
vary with fractional scale; hence even uniformly bounded height bytes do not
prove converted <=255. The authored 800-square example yields 319, byte 63.
Use signed conversion behavior/low byte as read; this is not an unsigned byte
saturation. Arbitrary invalid/nonfinite/out-of-range conversions are not licensed.

D: 0x100a3e77–0x100a3e90 writes
`u = converted & 255; r = (u + 8) >> 4; new = (old & 240) | r`.
u is 0..255; r is 0..16. Thus all old high bits remain set and bit 4 can
additionally become set; the high nibble is not necessarily invariant. There
is no final nibble mask. Legal old 0x80 is attainable through the other arm's
colour 8 assignment (existing [UI owner proof](../re/ui.md), §2.9), not only an
arbitrary byte injection. A later flags-2 update of height 248/255 can change
that to 0x90. Fresh 0x90 already has bit 4 set: that initial value alone would
hide the wrong-mask defect. Old 0xa5 in the table is arithmetic-only, not a
claimed valid shipped owner colour.

D: 0x100a3ece increments the u32 generation once even when bucket loops never
execute. Then 0x100a3ed7 tests bit 1. That other arm samples the centre tile's
city/visibility/colour, retains the LOW nibble and replaces the high nibble at
0x100a4076–0x100a4081; its own increment is at 0x100a40bb. Existing UI §2.9
addresses the city/colour rules; they are not reimplemented here. Flags 0/4
execute neither arm, flags 1/2 one arm, flags 3/0xff both, in order 2 THEN 1.
Two arms mean generation +2 modulo 2^32; flag-1 work may overwrite a flag-2
carry bit. Thus combined flags are not equivalent to retaining a flags-2 result
unchanged. The current standard postload second callback supplies exactly 2
(Welt 0x100aba70–0x100aba9b).

D: the plane is freed by 0x100a3c40–0x100a3c60, invoked by the deleting
destructor 0x100a3c90–0x100a3cab. World cleanup paths at
0x100a38c4–0x100a38d5 and 0x100af336–0x100af347 call that slot and clear
world +0x120. The borrowed world/plane cannot outlive those owners. Rebuilding
pixels in place differs from replacing their allocation for a retaining reader.

## 3. First readers and safe publication obligations

| Producer/input or trigger | First resolved reader / observable effect | Lifetime and ordering |
|---|---|---|
| Corner plane -> height table -> flags 2 | Welt 0x1009db00/0x10084c20 ->raster 0x100a3cc0 | HEIGHT/world/island/table alive during synchronous update; no object/fine/coarse shortcut substitution |
| Raster plane +0x14 | Slot +8 at 0x100a3cb0; AnnoFrame 0x10002947–0x10002951 stores its pointer in minimap renderer +0x10 | Borrowed, not copied/refcounted by this getter; must bind after final initialization and before old world destruction |
| Raster generation +0xc | Slot +0x10 at 0x100b4b10; Frame attach 0x10002939–0x10002944 caches generation MINUS ONE | Forces a subsequent refresh; this observer exists before a gameplay tick is needed |
| World attach -> immediate paint | Frame 0x10008eb6–0x10008ebf ->0x10002920 ->0x100028e0–0x10002910 ->0x10031000 | Attach refreshes dimensions and paints immediately if texture/plane exist; not deferred to first sim update |
| Per-frame refresh | Frame 0x100030f0–0x10003130 compares generation, calls 0x10031000 on mismatch, then draws 0x10031490 | Updates last observed generation after repaint; same plane pointer is retained |
| Plane byte -> palette/texture | Frame 0x100310af–0x1003111c and unrolled body use full unsigned byte as palette index at renderer +0x9040 | Both shade and high-bit carry can affect picture; not a nibble-only consumer |
| Load publication | Frame load-return 0x100291af–0x100291db obtains loaded world then calls SetWorld; resolved SetWorld 0x10008e90 attaches minimap | Original first reads are after scene load returns; attach itself does not require a World update call |
| Runtime changes | Welt 0x100aba85–0x100aba96 passes flags 2 after coarse rebuild; 0x100a1a7e–0x100a1a98 and 0x100a2802/0x100a2b20/0x100a2e2c paths pass 0xff; 0x100a1522–0x100a1531 passes 1 | Generation reflects addressed writes, not just a later bulk reload; city/visibility and height updates share storage |

D: null Frame attach stores a null world but skips rebinding the old plane;
draw begins with a null-world guard (0x100030f9–0x100030fe). A new nonnull
attach obtains the new plane. A rebuild may retain bytes but cannot hand the UI
a dangling old allocation or silently omit generation invalidation.

The carry does not guarantee an immediate visible pixel difference: the existing
Frame palette proof (UI §2.9, constructor 0x100338c0–0x10033a31) makes shade 0/1
transparent for each base colour. The carry-only outputs 0x80 and 0x90 both
have shade 0. Their distinct retained high bits can affect a LATER nonzero-shade
update, however; no permission to discard that byte difference follows.

Search coverage: complete executable-section slot +0xbc call sweep in Welt,
Frame, Game, Scene, View and Display; all candidate contexts retained. Most
same-offset calls are unrelated interfaces, NOT extra raster readers. Concrete
Frame 0x10002931/0x10003106 are resolved through the world getter and matching
raster methods; Welt 0x100aba85 is its update path. Getter relocation hits at
0x100cc9e8 and 0x100cce94 identify world tables, not all indirect consumers.
Other modules (including CompLogic), computed-vtable calls, escaped plane-pointer
aliases and generic save/serialization consumers were NOT exhaustively closed.
Finding the minimap reader therefore does not prove renderer-only ownership,
safe omission, equivalence to CoastIndex or a cache serialization exemption.

Reconstructible numerical input is the initialized scalar table plus retained
HEIGHT and final sampled island lookup. Flags-2 output also needs the previous
high bits; a blank 0x90 plane is not equivalent to arbitrary prior city colours.
For first publication, reproduce the original high-bit initialization/update
sequence or retain its proven result, build final low bits and generation before
binding the minimap. For later writes retain the same backing or rebind every
reader, and invalidate by the proven generation protocol. Full save/load cache
policy remains a separate integration decision, not inferred from this read.

## 4. Executed checks and next bounded implementation

C: private exact-dyadic Python model executes 31 rows under the declared
precision-53/nearest profile; all agree. Ten merge rows, six sample rows with
separate merge checks (12 checks), three rectangle/generation rows, five flags
generation rows and one u32 wrap row reconcile to 31. Wrong final nibble mask
fails 4 rows including legal-model height-248/255 samples; ordinary controls
still pass. Skipping generation for an aligned empty rectangle fails 1 row;
other rows pass. Each wrong branch is selected explicitly and its disagreeing
output retained. A separate private C++ float-store transcription agrees on all
six sample/count rows, including 319 and the clipped one-sample border.

These are direct MODELS, not original x86 execution, sim acceptance or CTest
counts. No original runtime/HE pixels were captured. Both transcriptions can
agree and still be conditional on the stated floating environment; they do not
close CAP-2 by self-comparison. V8's aligned empty transition and V13's
discriminating merge predicate are now modeled (the latter with prior colour 8;
V13's literal old-zero rows remain static). Broader flags-1 owner state and original callback execution
remain NOT_EXECUTED. No public reference code or fixture is needed for this
research packet; private sources, commands and receipts are retained.

Proposed next small package: resolve the active FP condition at the callback
entry (0x100a3cce) and table-initialization profile, preferably a bounded Windows
control-word/table observation, or prove the caller condition statically. Then
an isolated sim_load helper may take validated dimensions/rectangle, immutable
HEIGHT/island lookup, a validated numeric profile and an owned prior byte plane/
generation; return owned updated bytes/generation with explicit refusal on
unsupported geometry/profile. Candidate files: new sim_load
wildlife_secondary_raster.hpp/.cpp, one focused test, existing postload reference
only if independently extended with its CLI contract. No hardcoded raster cap;
256 is the standard input default, not a library limit. Validation must cover
counts/stride, normalized bounds and conversion domain before any allocation/
loop. Preserve per-add f32 rounding by a proved integer/dyadic equivalent; do
not replace with an unproved byte mean or assume universal precision 53.

Regression/control cases: carry with prior legal even colour, unchanged ordinary
merge, actual component wrap at fractional scale, 2 × 2 sampling and order,
nominal border denominator, aligned AND unaligned empty rectangles, generation
wrap, flags separated, ownership/rebind and earliest reader refresh. Flags-1
ownership/visibility reconstruction must have its own validated inputs before
full shared-storage composition. A new output layout is not authorized here;
any persisted/default semantic changes still require compatibility coordination.

Critical path: settle FP profile -> isolated raster proof -> coordinated NAV
arithmetic repair/identity decision -> validated postload composition in original
order (fill, extrema/coarse AND secondary raster, final GFX) -> publish only after
all dependencies/first readers are safe -> separately released live fauna/hunter
integration and real gameplay proof. The first fill and GFX helpers stay UNWIRED;
this list releases none of those implementations.
