# First post-island fill: catalogue producer and type profile

WILDLIFE-POSTLOAD-INPUTS-1, 2026-11-19. READY FOR REVIEW. GOG 2.0.0.5;
D = primary instructions, P = parsed original file/PE table, C = rebuild source,
H = hand derivation, U = outside the proved binding. No original runtime executes.
This narrows [OQ-202](../open-questions.md#oq-202-whether-growth-rewrites-the-tiles-walk-record-t334-f5-2026-10-26)
for the [postload first call](../sim/wildlife-postload.md#10-first-call-input-capability--wildlife-postload-inputs-1),
not all growth, editor, reuse or global initialization paths.

## 1. Producer to consumer

All addresses in the module column. D/P: the constructors, calls and concrete
slots below establish a fresh standard path, rather than merely a stored pointer.

| Module / addresses | Producer or consumer | Established effect / boundary |
|---|---|---|
| AnnoFrame 0x10020230–0x1002027d, call 0x1002024e | Fresh initialization calls 0x10020100 | Negative return takes cleanup/release, not successful load |
| AnnoFrame 0x10020100–0x100201d7 | Create and initialize catalogue | Passes kind 0x101 and address of global 0x1009c268 through 0x10077a62; checks factory, slot +0x50 and slot +0x60 returns before continuing |
| AnnoGame 0x10013760–0x100137a0; 0x10007650–0x10007676 | Factory and concrete constructor | Kind 0x101 allocates 0x6418 bytes, constructs through 0x100065c0 ->0x10003830, then installs table 0x100215a4. Null allocation returns failure; no initialized catalogue inferred from it |
| AnnoGame 0x10003830–0x10003886 | Base initialization | Zeroes the pointer array at catalogue +0x214, originally 4000 entries. This is original layout, not a modern content limit |
| AnnoGame table 0x100215a4 +0x50/+0x54; 0x10007400–0x1000749e / 0x100073a0–0x100073fc | Init/load dispatch | +0x50 calls 0x10005b30, whose entry calls 0x10001070. That passes flags 0xff through +0x54; derived loader calls 0x100024a0, whose bit-1 arm calls +0xb8=0x100020a0 |
| AnnoGame 0x100020a0–0x100020f7; 0x10001d10–0x1000206b | Original file reader | Resolves Anlagen.dat (string 0x10024174), opens it, scans ANNO/ANLAGEN/ENTRY. Eight-byte ENTRY prefix supplies key at +4. Lookup +0x70 happens before allocation; an existing key reuses its descriptor and receives subsequent records |
| AnnoGame 0x10001e0e–0x10001e49; 0x100063a0–0x1000644d | Descriptor factory | Missing ENTRY uses catalogue +4 with kind 0x700; actual factory allocates 0x294 bytes and calls 0x1000c240, not the base factory's 0x244-byte variant. Failure stops the reader |
| AnnoGame 0x1000c240–0x1000c28f; table 0x10021a88 | Actual type | Calls base 0x1000b4c0, then installs 0x10021a88. Relevant +0x40/+0x68/+0x94 are 0x1000a660 / 0x10008f90 / 0x10007dc0 |
| AnnoGame 0x10005f00–0x1000609d; 0x10003260–0x100032b0 | Type-array insertion | Kind 0x700 falls through to base insert, which stores descriptor at +0x214+4*key and returns key. No bounds check is present on this direct store; unsupported modern input must not copy that unsafe behavior |
| AnnoGame 0x1000a670–0x1000a679; 0x1000a660–0x1000a666 | Type identity | Reader passes returned key to type +0x3c; setter stores low u16 at type +4. Getter +0x40 zero-extends that u16. Array key and stored id have distinct widths; both agree for 2 and 11 |
| AnnoGame 0x10001f45–0x10001f63; 0x1000b887–0x1000b8b0 | Record dispatch | Actual descriptor +0x70 parser forwards non-HAEUSER records to 0x10009240; unsupported read return takes failure. This is not TypeRegistry execution |
| AnnoFrame 0x1002078f–0x100207b0 / 0x10029188–0x100291b9 | Scene construction | Passes the global catalogue as THIRD argument to CreateAnnoSceneInstance via 0x10077a56, with distinct output pointer as fourth argument; rejects negative result |
| AnnoScene 0x10001150–0x1000124f; 0x10006ec0–0x10006faf / 0x10007a00–0x10007af4 | Scene factory and constructors | Factory forwards its third argument to the selected constructor; both constructors pass it to base 0x10002fc0. Allocation or setup failure is not successful construction |
| AnnoScene 0x10002fc0–0x10002ff9 / 0x10002e84–0x10002ea6 | Stored input / world factory | Stores pointer at Scene +0x154, later forwards it to world creation; no catalogue copy or retain inferred |
| AnnoWelt 0x100b2568–0x100b2588 / 0x1009d848 / 0x100a37d0–0x100a37d6 | World binding | Factory forwards external fourth argument, base Init's third argument becomes world +0x110; world +0x18 returns it |
| AnnoWelt 0x1008c0c9–0x1008c0db / 0x100928a8–0x100928c3 | Island binding / fill | Island +0x1c receives that catalogue, then fill uses +0x70 with 2 and 11 before scanning cells |
| AnnoGame 0x100037e0–0x100037eb | Bound lookup leaf | Returns pointer at +0x214+4*id, without range/null validation or draw. Keys 2/11 select offsets +0x21c/+0x240, not adjacent entries |

D/U: reuse is not implicitly the same proof. Frame 0x10028c80–0x10028cad can
replace the global from an attached kind-0x100 object. Frame 0x100584e0–0x10058507
and View 0x10078bc4–0x10078bf7 pass a caller object's +0x18 result to the Scene
factory. Those are explicit alternative suppliers; their arbitrary concrete
receivers are not newly closed. Scene 0x10002e4e–0x10002e72 can also reinitialize
an already supplied world. The new contract requires the validated profile,
not a claim that every such input has the fresh standard table.

## 2. Fields needed by first-call fill

D/P: these are consumer facts, not a compiled copy of the type catalogue.

| Fact, units / width | Original producer | First-call consumer and proved value |
|---|---|---|
| Id / u16 getter, separate u32 file key | ENTRY prefix, insert and setter above | Fill grass +0x40 at Welt 0x10092a01, fish +0x40 at 0x10092aa8; writes 2 /11 respectively |
| Flags / unsigned u16 | Game HEAD reader zeroes 56-byte local buffer at 0x10009348–0x1000935a; HEAD[0x1c] stored into type +0x12 at 0x100093bd–0x100093d7 | Actual +0x68 getter tests flags&3==1 at 0x10008f97–0x10008fac. Both raw flags 0x29, hence mean branch |
| Mean height / unsigned bytes to integer | Existing corner plane/stride, not descriptor float +0x58 | Game 0x10008fae–0x10008fdf sums four unsigned corners and truncates /4. Both branches use this getter with zero extra arguments. Range 0..255; no quantization to a multiple of 16 |
| Base walk code / u8 | Constructor type +0x75=4 at Game 0x1000b555; HEAD[0x2f]/[0x30] encoding at 0x1000940b–0x1000942b via 0x100091f0–0x10009223 | Grass calls +0x94(0) at Welt 0x10092a39–0x10092a40. Both shipped first/shape=2/3 ->code 2; first<8 returns first, even though shape is nonzero |
| Rotation-specific walk / low returned byte | Game 0x10007dc0–0x10007ea2 | Rotation 0; base code 2 bypasses high-bit rotation table. Getter draws nothing. Fish does NOT call it, even though its own raw code also happens to be 2 |
| Precomputed ground presence / byte +0xc2 | Welt FRUCHTBAR completion 0x10089565, independent of payload being zero | Wrapper 0x100a1815–0x100a1834 skips preprocessing only with that presence. Empty cells' ground low nibble at 0x100929e3–0x100929f6 is unsigned: 5 fails, 6 and 13 pass |
| No further descriptor property | Complete Welt 0x100928a0–0x10092b32 | Resolves types, then queries only id, selected height and grass walk code. No goods, live quantity, footprint expansion, HIGHMAP, object WALKMASK or descriptor float-height query on this mean-profile path |

P: type 2 HEAD length 52, type 11 length 56; both footprint 1x1, flags 0x29,
walk inputs 2/3. Ids/HEAD uniqueness are checked independently from Anlagen.dat.
Their WARE names identify GRAS/FISCH, but fill selects literal ids, not a goods
search. No goods-name lookup is needed by this callback. Extra descriptor fields
are not assigned an invented role. Original fixed array/object sizes above are
format/runtime facts, not engine caps.

D/U: the parser's property setter can change flags (Game 0x1000a83a–0x1000a845,
property 0x466), and catalogue insert/remove can replace slots. The supported
contract is a validated immutable snapshot, not an assertion that all original
catalogues remain immutable. The base record reader 0x10009240–0x1000a3bf and postprocess
0x10002520–0x10002dbf were inspected for the loader chain; this does not establish transitive initialization/draw
closure or every later metadata mutation. Those wider claims remain outside scope.

## 3. Lifetime and execution limits

D: Scene +0x154, world +0x110 and island +0x1c are stored raw pointers along
the addressed chain. Type attach (Game 0x1000bf20–0x1000bf47 ->0x1000a680–0x1000a6a7)
stores its kind-0x100 parent at type +0xc; it does not make the type independent.
Frame 0x1001fff1–0x10020015 releases its scene before the catalogue and clears
the global. Catalogue +8 goes through Game 0x10003750–0x1000375e to deleting
thunk 0x10007630–0x1000764b, then 0x100075f0–0x10007629 ->0x10006450–0x10006551
->0x10003760–0x100037cc. That base loop removes/detaches/releases each nonnull
type. Borrowed type pointers therefore must not outlive or race catalogue teardown.
This is an ownership dependency, not a claim of a ref-count protocol.

D: first-call zero-draw closure now applies to the fresh bound mean profile:
lookup 0x100037e0, id 0x1000a660, selected height 0x10008fae–0x10008fdf and
walk 0x10007dc0–0x10007ea2 are finite draw-free leaves; fill itself has no draw.
It does not extend to catalogue creation, c2-zero preprocessing, load callbacks,
second-call navigation/raster, a substituted profile or global initialization.

P/H: existing independent Python helpers were used, without C++ output, for 12
height predictions, 8 rotation predictions, 2 actual wrong-profile disagreements
and 2 authored walk controls. Corners 1,18,53,255 sum 327 ->81; 15,16,31,32 sum
94 ->23. All-zero walk inputs retain constructor 4; authored first=3/shape=0
returns 3. These distinguish a different height mode, rounding and constructor
fallback. They are Python/static checks, not original runtime or full fill
execution. Hand-derived grass/fish write vectors remain NOT_EXECUTED here.
