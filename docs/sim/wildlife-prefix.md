# Ordered wildlife prefix adapter — WILDLIFE-PREFIX-1

2026-11-18. D = inherited addressed original read, P = parsed original file,
C = rebuild implementation, A = explicit transient API adaptation, U = unresolved.
This is not World publication, persistence, a feature default or a full-world RNG claim.

## Acceptance written before adapter implementation

| Requirement / evidence | Positive expectation | Distinguishing refusal/control | Planned proof / review |
|---|---|---|---|
| Original item interpretation, AnnoWelt 0x10089391–0x100893c8 | 14-bit id, rotation high 2 bits, signed byte offsets added in wide coordinates | id bit 12 retained; -1 offset differs from unsigned 255; overflow refuses before i32 narrowing | Raw formats tests; pending |
| Explicit metadata, AnnoGame 0x1000a410 / 0x1000a4d0; HEAD loader 0x10009485 | Present UNUSED resolves to 0; present goods name and HEAD[0x1a] supply facts | Missing/unknown WARE and truncated HEAD must refuse rather than default | Adapter metadata tests; pending |
| Checked original-image tables, selector 0x10001b30 | Supported image identity and selected climate rows map authored candidate to slot | Wrong stamp/layout, truncated section and row outside bounds refuse | PE reader tests; pending |
| Original order, INSEL5 0x100a2f8c–0x100a2fbc | Preserve island/header/record ordinals, duplicates and present-empty HIRSCH | Unknown intervening header stops at that ordinal; no skipping or slot sorting | Shared composition examples; pending |
| Serial composition, resource 0x10089417–0x1008949d then HIRSCH 0x10094808 | Resource–HIRSCH–resource assigns draws in source order, mode changes goods only, raw stamp /500 through existing helper | Omitted resource draw / resources-first mutation changes named timestamp/cursor | Independent examples + one applied private mutation; pending |
| Allocation-success load domain and chunk prepend 0x10094744–0x10094822 | Effective records retain raw chronology; same chunk visits reverse insertion, across repeated headers too | Do not infer saved live figure ids or allocation-failure behavior | Multiple chunks, mixed flags, explicit stamp/mode provenance; pending |
| Owned bounded session | One scratch copy/fact preparation; legacy evaluator results unchanged on 20 vectors | Failed/unsupported session cannot resume or finish successfully; partial diagnostic explicitly labelled | Existing index/zero controls + session equivalence; pending |
| Real file / limits | First Tutorial resource stays variant 6, height 48, one draw | Stop actual first unsupported type/header, report ordinal and diagnostic partial, never 4,589 inferred draws | One opt-in Debug adapter test; pending |

The original receiver, selector and ordinary admission come from
[resource initialization](wildlife-resource-initialization.md). Raw HIRSCH and
load ordering come from [decoder boundary](wildlife-load-boundary.md) and
[corrected decisions](wildlife-decisions.md). INIT is read at commit 97d893af,
not integrated as another model. Its MAIN fields supply neither sampled stamp
nor mode. Both must be explicit supplied inputs with provenance. This adapter
models only successful original allocations, not their failure recovery.

The incremental resource session is a narrow owner-approved composition mechanism,
not a callback framework. It reuses the existing loop/selector, owns copied
facts/climate/scratch, and borrows grid backing/registry for its bounded lifetime.
The old evaluator remains an atomic copy-in wrapper. A failed session is poisoned;
only an explicitly diagnostic prefix can expose its partial scratch.

Only INFO, HEIGHT and GFXID may pass as inherited no-draw setup in the fresh
observer-inactive profile: INFO loader 0x100897a6–0x10089802;
HEIGHT copy 0x10084870; GFXID 0x10085060–0x10085177 and dirty-only 0xc9/0xca
notifications (resource contract §2). No PROD2/FLUESSE equivalence is assumed.
Every other header or unproved callback stops the prefix. Post-island world
callbacks 0x100a2fc8/0x100a2fd7 remain outside complete-local-island coverage.

No original data arrays, raw records, bytes, listings or personal paths are
published. Synthetic examples/tables are authored; original rows are read at runtime.
Semantic tag ownership (enable/mode, variation plane, effective spawns and load
sites) is proposed only; numeric registration and production wiring remain later work.

## Implemented scope and measured first boundary

C/A: `prepare_wildlife_prefix` takes raw scene bytes, parsed Anlagen entries,
AnnoGame bytes, resolved placement registry, per-source-island sampled u16
stamps, an explicit mode bit and provenance strings, chunk geometry and bounded
u32 draws. Successful return means all supplied local island records completed.
An unsupported/failed prefix is an error alternative carrying an explicitly
diagnostic owned snapshot, boundary and consumed count; it cannot be mistaken
for normal Result success. Fatal prerequisites may have no snapshot. Neither
case is resumable. The resource Session likewise poisons errors and refuses
finish/retry; diagnostic extraction is named separately.

The adapter resolves metadata once per invocation using ordered id/name maps;
each island session owns one fact index and scratch. WARE must be present with
a terminated nonempty first name; explicit UNUSED maps to 0, unknown names and
repeated/missing WARE/HEAD refuse. HEAD must contain the complete word at 0x1a.
The shared private PE32 table view preserves the existing AnnoWelt identity/error
checks and maps data ranges only. AnnoGame supports the GOG COFF stamp 0x44c0c30f,
SizeOfImage 0x2e000, base 0x10000000 and i386 machine. Seven physical climate rows
follow this image's bounded slot region; these are not an engine climate cap.
Other layouts need an explicit addressed profile, not unchecked row reads.

Only leading INFO/HEIGHT/GFXID setup is admitted; later setup is a boundary.
Terrain starts at the proven fresh zero values and is updated only by preceding
checked blocks, never preloaded from a future HEIGHT header. An explicit leading
INFO is required for climate. Fresh unique embedded INSEL5 slots only; a reused
slot stops before its child callbacks. Source indices are zero-based, file-order
island/header/record ordinals. Header offsets retain the parser's local offset,
not a falsely inferred absolute file address. UINT64_MAX header identifies an
outer-island profile boundary. Chunk size defaults to the original 8; other
caller-supplied sizes are an explicit geometry adaptation, not original parity.

D/P/C: Tutorial/01Discover01 stops at island ordinal 0, slot 11, GELAENDE header
35, record 22: type 1558, STEIN kind 323, 2 × 2, flags 0x3a. That is outside the
reviewed 0x09/0x29 domain; no new mode-2/overlay/coverage rule is invented here.
The longest completed prefix has 37 resource records, 37 supplied draws and
0 effective spawn records. Header 34 record 0 remains type 70, variant 6,
computed height 48 and one draw. Real test inputs are explicitly supplied raw
stamp 500/mode 0 with a supplied draw stream, not values decoded from MAIN or
an original runtime PRNG cursor. HIRSCH header 71 has not been reached.
The 4,589 candidates are not an actual-draw total.

Authored resource–HIRSCH–resource draws 4,7,2,31 give resource variants 4/2 and
first jitter base + 7, then base + 31. Batching resources first would assign
base + 2; omitting the first resource advance assigns base + 4. The examples
cover stamps 499/500/65535 (bases 0/1/131), both mode bits, repeated/empty/absent
HIRSCH, duplicates, unsorted island ids, multiple chunks and prepend links.
Existing 20 resource and 74 wildlife examples remain the numeric references;
no parallel selector, jitter formula or world generator is added.

Next code: establish only the admitted placement semantics needed for the first
0x3a stone record (including its overlap/coverage/height and write effects),
then extend this same ordered evaluator and resume the real prefix to its next
unknown boundary. Do not skip nonresource placements: their writes affect later
admission. Complete-local records still exclude the unproved post-island world
callbacks. After source/callback closure, coordinate semantic extension ownership
(enable/mode, variation, effective spawns, ordered load draws) before World/native
publication. No numeric tags or original runtime alignment are claimed.

### GELAENDE epilogue closure (bounded callback clarification)

D: the generic handler branches even a zero-item header from `0x1008935f` to
`0x100894a9`; `0x100894b2` calls island slot +0x198 with the six-byte block
rectangle after adding island origin. The concrete wildlife island vtable
0x100cac2c resolves it to `0x10092b40`–`0x10092c94`, the addressed below-water
pass in [transport §3.4.13](../re/transport.md). This is **not** the per-record
+0x78 observer bypass and must not be omitted just because it draws nothing.

D: `0x10092b4d`–`0x10092bc0` clips that half-open rectangle against island
origin/width/height. `0x10092bf3`–`0x10092c32` reads four unsigned HEIGHT corners,
truncates their sum /4 and compares strictly below the initialized default 32.
`0x10092c40`–`0x10092c4d` ORs 0x1ff0000 into the tile's second word. It changes
only the represented nine walk-mask bits; code, objects, phase, height, other
packed flags, terrain and type metadata are preserved. The only callee is the
pure truncation helper `0x100b5f90`–`0x100b5fb6`, which saves/restores the FPU
control word and writes only its stack result. No world pointer, PRNG or other
callback occurs. This mask is not read by the currently supported mode-1 import
admission, but is an observable scratch-tile effect and is implemented.

C/A: the same session owns this clipped OR, using its existing quad mean and the
existing named default threshold. Legacy `evaluate` has no header input and
retains its old effects; only the raw header adapter calls the epilogue. Empty
header 33 is its first real execution. Independent raw HEIGHT counts give 1,024
closed water cells in its (0,0,32,32) block and 977 in header 34’s adjacent block. A completed header records an engaged
mask-update count (including 0); the partially read header 35 has no epilogue yet.
The earlier real-first.log investigation lacked this mask effect and is
superseded for complete-prefix evidence; its resource/draw counts did not prove
the walk-mask state. Final Debug evidence reruns the affected real boundary.

The API covers supplied local embedded-island child records, not SCENE/world
player/actor or post-island handlers. Their effects are outside the supplied
stream contract and are not assumed zero; complete-local never means complete
scene/world loading. Geometry is widened and overflow-checked on entry; original
malformed 16-bit coordinate wrapping is not asserted as engine behavior.

### Stone successor (2026-11-18)

[WILDLIFE-STONE-1](wildlife-stone.md) supersedes only the predecessor's stone
stopping point: 35/22 is faithfully accepted without a draw, and the same raw
stream stops at 37/1, type 1552 / flags 0x12. It completes 258 records (255
resource draws, 3 stones), 4 header epilogues, 0 spawns. This remains a typed
**diagnostic partial**, with explicit supplied stamp/mode/draws. The original
PREFIX receipts and its first unsupported boundary remain preserved.

### Cliff successor (2026-11-18)

[WILDLIFE-CLIFF-1](wildlife-cliff.md) processes former cliff 37/1 and advances
this same supplied-input diagnostic to GELAENDE 42/39, type 1512 / flags 0x8018.
It completes 2,387 accepted records / 2,333 resource draws / 29 stones / 25 cliffs /
9 header epilogues / 0 spawns. HIRSCH 71 is still unreached. Preserve both earlier
boundary receipts; none proves complete scene initialization or a global cursor.

### Rock successor (2026-11-19)

[WILDLIFE-ROCK-1](wildlife-rock.md) processes former 42/39 and completes all
16 local GELAENDE headers: 4,760 accepted records / 4,589 resource draws /
84 stones / 63 cliffs / 24 rocks / 16 epilogues / 0 spawns. It stops at unknown
header 49, FRUCHTBAR, before applying that header. HIRSCH 71 remains unreached.
This is still a supplied-input diagnostic partial; do not infer zero draws or
unchanged terrain for the new header, or claim original global cursor closure.

## FRUCHTBAR continuation (2026-11-19)

The [fertility header contract](wildlife-fertility.md) extends only ordered local
header preparation. It merges source low nibbles into effective variation,
preserves resource high nibbles, retains the last raw payload in owned Terrain,
and records each completed header. No immediate empty-tile fill is reached.
Historical frozen boundaries above remain predecessor evidence; current execution
continues once through header 49 and stops before BERGWERK2 header 50. Global
post-island callbacks and the previously documented derived-cache integration
remain outside this transient projection.

## BERGWERK2 continuation (2026-11-19)

The [resource-list header contract](wildlife-bergwerk.md) adds owned typed entries,
per-chunk nodes, prepend links and the represented presence bit. This is separate
from OCCUR ore occurrences and quantities; normal scenario/deposit imports remain
unchanged. The same stream processes header 50, then stops before OCCUR header 51.
Historical frozen boundaries remain predecessor evidence; post-island/global
initialization and the existing omitted-cache integration obligations are unchanged.

## OCCUR ore continuation (2026-11-19)

The [bounded ore occurrence contract](wildlife-occur.md) adds typed occurrences,
shared kind-6 list nodes and the reached BERGWERK-mask refresh. The existing
resource-list node/chunk seam is named generically to represent the original
shared chain, with kind-1 BERGWERK fields retained. Other classes/goods stop at
record boundaries. No ordinary loader or mining integration changes. The same
stream processes OCCUR header 51, then stops before ANBAU header 52; global
initialization and existing omitted-cache obligations remain outside scope.

## ANBAU continuation (2026-11-19)

The [ANBAU bitmap contract](wildlife-anbau.md) adds only the ordered scalar/name
load: per-header climate seed/reset, exact byte-name matching against the checked
original-image goods window, owned processed tokens and ignored-tail count.
It changes no prior terrain/resource/occurrence state or draw assignments.
The same stream processes header 52, then stops before DISCOVER header 53.
This does not implement consumer farming rules, other-lane fertility policy,
World publication or post-island initialization.
