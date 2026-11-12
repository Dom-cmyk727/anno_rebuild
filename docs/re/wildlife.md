# Wildlife origins: sweep, creation and HIRSCH (WILDLIFE-RE-1a)

2026-11-12. Reference: GOG 2.0.0.5 `AnnoWelt.dll`, SHA-256
`f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2`.
Marks follow [hunter.md](hunter.md): **D** read from original instructions/data;
**P** parsed original content; **U** unresolved. Addresses below are in that DLL
unless another module is named. This extends hunter.md's remaining read B; its
hunter arithmetic and job reads are unchanged. Owner decision: faithful wildlife,
2026-11-12. Status: READY FOR REVIEW.

Ghidra is **NOT_RUN**: this Mac has neither Ghidra nor a working Java runtime.
The repository's `tools/disasm.py` (Capstone/pefile) supplied the instruction
reading. HE86 is **NOT_RUN**, its DLL is unavailable. Class names are labels from
the structural comparison in [edition comparison](../research/he-vs-gog-comparison.md#4-class-names-from-the-history-edition-applied-to-the-gog-module-s),
not a fresh HE equivalence claim. No running-original experiment was performed.

| Read | Acceptance evidence | Distinguishing case | Handover review |
|---|---|---|---|
| Sweep | full `0x10093480`-`0x10093896`, predicate and table helpers | eligible centre with no matching neighbours fails the ordinary threshold | pending orchestrator |
| Creation | full `0x10092ca0`-`0x10093013`, constants and world getters | cave omits position draw but still draws species | pending orchestrator |
| Loader/object | `0x100946c0`-`0x10094873`, both tables and allocation sites | record byte 4 bit 0 changes timestamp draws, not species | pending orchestrator |

## 1. Fauna sweep: `0x10093480`

**D — receiver and scope (`0x10093491`-`0x100934f7`).** `this` is the island base,
not an adjusted interface. Three stack arguments are local tile x, local tile y,
and the time value. The chunk is selected by truncating x/8 and y/8, row stride
`island+0x8c`, sixteen bytes per chunk at `island+0x94`. Chunk bytes 0/1 are its
chunk coordinates; `+0xc` is the intrusive object-list head. The existing cadence
and cursor contract remains [state.md section 12](../sim/state.md): this read
does not redefine `fauna_period` or the island caller's scheduling.

**D — retain or recycle (`0x100934f7`-`0x10093578`).** Traverse list order, caching
each next pointer before modifications. Only objects whose slot 0 returns zero
are counted. For those objects, byte `+8` bit `0x40` marks an occupied spawn
record. Occupied records remain counted without terrain reevaluation. For an
unoccupied record, decode local tile `(chunk.x*8+(p&7), chunk.y*8+((p>>3)&7))`.
If `0x10093030` rejects that tile, unlink it and prepend it to a temporary reuse
list, decrementing the count. This is a count of spawn records, not a count of
all living animal figures.

**D — candidate order (`0x1009357a`-`0x100935de`).** Test sixteen cells, y offsets
1,3,5,7 outermost and x offsets 1,3,5,7 innermost, relative to the supplied x/y.
Append each passing coordinate in that order. No random draw occurs in this scan.

**D — habitat predicate (`0x10093030`-`0x10093472`).** A zero centre object id
fails. Resolve its type through definitions slot `+0x70`; a ROHSTWACHS kind
`0xe2` is evaluated using id+1. The centre must offer output-0 goods `0x46`
through type slot `+0xac(0)`, or have kind `0x145` through slot `+0x5c`.
`0x145` is TIERHOEHLE (the PRODTYPE mapping in state.md); `0x46` is a goods
comparison, not a terrain-height threshold. Every nonempty neighbour in the
eight surrounding cells is resolved with the same growing-type substitution.
The score is `1 + sum(goods0 == 0x46) + sum(kind == 0x145)` over those neighbours.
The left-neighbour implementation incorporates the initial 1 into its branch;
an empty left neighbour still leaves the initial 1. Island property 7 equal to
5 requires score >=1; otherwise score >=4 (`0x1009343e`-`0x10093472`). Thus a
qualifying centre with zero qualifying neighbours passes property-7 value 5 and
fails the other branch. The numeric property is retained here without guessing
its player-facing climate name.

**D — density table (`0x100938a0`-`0x10093963`).** The first call constructs a
127-byte table at `0x100e34e8`; it is generated deterministically, without PRNG.
The initialized values relevant to candidate count n=0..16 are:

| n | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14..16 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| desired spawn records | 0 | 1 | 1 | 1 | 2 | 2 | 2 | 2 | 3 | 3 | 3 | 3 | 4 | 4 | 5 |

The initializer starts segments at 0, 1, 2, 3 and 5 and truncates successive
float accumulators; increments are respectively 1, approximately 1/3, 1/4,
approximately 1/3 and zero. Segments are indices [0,1), [1,4), [4,8), [8,14),
[14,127). The constants are at `0x100c1128`, `0x100c1278`, `0x100c1298`,
`0x100c4bd8`, `0x100c3f5c`, `0x100c128c`, `0x100c4100`.

**D — replenish (`0x100935e0`-`0x10093725`).** Compute desired minus retained
spawn-record count; only a positive difference is replenished. Reuse the head
of the temporary list, otherwise allocate sixteen bytes with vtable
`0x100cac04`. Allocation failure returns from the sweep. Select a candidate
using one world draw modulo the current candidate count, remove it by moving
the last candidate into its slot, and decrement that count. Query the chunk's
category-0 object at that local position (`0x1007c630`). If occupied, retry with
the same allocated/reused object, consuming another draw and candidate. On
success prepend the object to the chunk list, set chunk flag bit 0, write
`+0xe=0x00ff`, timestamp byte `+9=low8(time/1000)`, and clear packed-position bit
`0x40`. The created-record count advances only on successful insertion.

**U — candidate exhaustion.** The retry path has no explicit zero-divisor guard
after it removes the last candidate (`0x100936c5`-`0x100936e6`). Whether shipped
content can exhaust every candidate through category-0 collisions is unproved;
do not invent a fallback selection. See OQ-572.

**D — discard leftovers (`0x1009372b`-`0x10093761`).** Delete unused recycled
objects. Clear chunk flag bit 0 only if the retained/replenished count is zero.
The sweep does not trim an excess of already retained records to the desired
count and does not count all live wildlife through the unit manager.

**D — materialize figures (`0x10093767`-`0x10093886`).** Traverse the resulting
chunk list. Category 0, unoccupied records are eligible when the signed 8-bit
result of `low8(time/1000) - timestamp` is nonnegative. This is a wrapping
timestamp test, not a species/type test or a growing count of animals. Resolve
the tile's actual object type; require output-0 `0x46` or kind `0x145` again.
Call the creation helper with chunk, spawn object, tile type and tile rotation
(tile byte 2 >>6). Unlike `0x10093030`, this final lookup does not substitute
id+1 for a growing type. On success save the returned figure's u16 id at `+0xc`,
set timestamp to low8(time/1000+60), set occupied bit `0x40`, and save the figure
type's goods from slot `+0x80` in `+0xe`. The type-property query `+0x48(0x20)`
also runs after creation; its returned data is unused by the local sweep.
Thus each eligible record creates at most one figure in this invocation, up
to all eligible unoccupied records, rather than a random number of figures.

**U — occupancy release.** This sweep never clears `0x40` on an occupied record.
The figure removal/movement path that releases it is outside these three reads;
the saved id alone does not prove that occupied records always correspond to
live figures. See OQ-573; animal behavior belongs to 1b.

### World draw order for later reference vectors

**D** All listed draws use island's world pointer `+0x20`, world slot `+0xb4`
(`0x100a1740`); cell draws finish before figure-creation draws begin. The helper's
draws must not be omitted merely because only `0x1009366f` appears in the older
draw-site outline.

| Position in processing | Draw address | Modulus/mask | Selects | Condition |
|---|---|---|---|---|
| Replenishment, each candidate attempt | `0x1009366f` | unsigned `% remaining_candidates` | index in compacted row-major candidate vector | deficit positive, allocation/reuse succeeded; repeats after a collision |
| Creation, per eligible record in final list order | `0x10092d8b` | `&3` (mod 4) | one of four within-tile positions below | tile kind is not `0x145` |
| Creation, after position calculation | `0x10092e96` | `&31` (mod 32) | initial index in climate's figure-type table | every helper attempt, even if cave entrance lookup returned zero |
| Loader, file entry order; separate from sweep | `0x10094808` | `&31`, added modulo 256 to time byte | timestamp jitter 0..31 | entry byte 4 bit 0 clear; allocation succeeded |

**D — retry and matching order (`0x10092ecf`-`0x10092f25`).** Species filtering
does not draw again: inspect sequential indices, wrap 31->0, stop on matching
goods or after eighteen inspected entries. Exhausting that bound still uses the
last inspected type, even if its goods do not match. Candidate compaction and
head insertion matter: with candidates A,B,C, draw 1 selects B, leaving A,C;
the next modulo is 2, not 3. New objects are then visited in reverse insertion
order before older objects (`0x1007bd50` prepends; `0x1007bc80` pops reuse head).

## 2. Creation helper: `0x10092ca0`

**D — arguments/coordinates (`0x10092cad`-`0x10092cf9`).** Receiver is island
base. The four arguments, in logical order, are chunk pointer, spawn-object
pointer, tile-type pointer, rotation. World tile coordinates are island origin
`u16 +4/+6` plus chunk byte 0/1 times 8 plus packed object-position x/y.

**D — position (`0x10092cfd`-`0x10092e87`).** For TIERHOEHLE kind `0x145`, ask
type slot `+0x98(rotation,buffer)` for entrances. If nonzero, take the first
entrance and add its float x/y to the origin returned by island slot `+0x84`.
There is no position draw on this branch. For all other admitted types, the
position draw selects offsets `(1/6,1/6)`, `(5/6,1/6)`, `(5/6,5/6)`,
`(1/6,5/6)` in that order. These are the original float operations, not an
integer 1/6 promise: the stored float is approximately 0.1666666716 at
`0x100c1294`, and scale `0x100ca6f8` is 1.0. The subtraction branches calculate
`(tile+1)*scale - scale*(1/6)` rather than adding a separately rounded 5/6.

**D — receiver correction (`0x10092d77`-`0x10092d7b`, `0x10092e7b`-`0x10092e87`).**
The calls at slot `+0xa0` here receive the **island**, passing the float x/y and
returning a height float (`0x10084cc0`), stored as the middle coordinate of the
position vector. They are not calls to the unit manager's creation slot. World
slot `+0xa0` later returns the manager (`0x100a34d0`, world `+0x114`); its slot 0
is the actual figure-creation call at `0x10092f5b` with position-vector pointer,
figure-type pointer, -1 and world property 4 (`0x100a3380`).

**D — type choice (`0x10092e8b`-`0x10092f30`).** Ignore object byte `+9` for
species. Always draw initial index `draw&31` into the u16 table
`0x100ca700 + 2*(32*island.byte[0xc0]+index)`. If object u16 `+0xe==0x00ff`,
use that table type directly. Otherwise use the bounded sequential goods search
described above, comparing each type's slot `+0x80` against the saved goods.
Resolve figure types through definitions slot `+0x6c`. No extra draw is made
for a failed goods comparison (`0x10092ecf`-`0x10092f25`).

**D — failure/post-create (`0x10092f5b`-`0x10093013`).** Negative manager creation
result returns null. Successful cave creation replaces the vector's horizontal
coordinates with the integer target tile for the later property-`0x2f` rectangle;
the earlier creation position was the entrance. The helper then invokes figure
property `0x2f`, its subobject's operation `0x45`, and attaches the island through
figure slot `+0x28`. These operations are downstream of the direct random draws.

**U — absent entrance and transitive behavior.** A cave with zero entrances jumps
directly to species choice, bypassing initialization of the position vector
(`0x10092d1f`-`0x10092d21`). Do not supply guessed coordinates: OQ-574. Figure
construction, property handling and attach may invoke animal-class behavior;
their additional draws are not certified absent by reading this helper. OQ-575
hands that transitive audit to 1b. This table specifies draws directly in the
three read paths, not a proof of the entire animal lifecycle's draw count.

## 3. HIRSCH loader and `tHirschObject`

**D — loader (`0x100946c0`-`0x10094873`).** Receiver is island base. Find HIRSCH;
entry count is payload size >>3. Read eight bytes per entry. If HIRSCH is absent,
delegate to generic island reader `0x10089130`; no fabricated empty record list
is substituted. The loader sets a base timestamp from `u16 island[0x9a]/1000`.

| Entry offset | Read meaning | Evidence (D) |
|---|---|---|
| 0..1 | local tile x, u16 | `0x1009473c`-`0x10094764` |
| 2..3 | local tile y, u16 | `0x10094744`-`0x1009475e` |
| 4 bit 0 | set: timestamp=base; clear: base+(draw&31), byte wrap | `0x100947f3`-`0x10094816` |
| 4 remaining bits, 5 | no use in this loader | full `0x10094721`-`0x10094830` |
| 6..7 | saved goods discriminator, u16; or replaced by sentinel `0x00ff` when queried world mode is set | `0x100947d3`-`0x100947ef` |

**D — no entry type byte.** The former description of byte `+9` as an animal
type is corrected by these instructions: record byte 4 controls timestamp
jitter and record bytes 6..7 constrain goods, while species is drawn later.
World slot `+0x10c(-1)` reads mode bit 0 at world `+0x69cc` without changing it
(`0x100a3790`-`0x100a37bb`). Its player-facing mode meaning remains U, OQ-576.

| 16-byte object offset | Meaning | Evidence (D) |
|---|---|---|
| 0..3 | primary vtable `0x100cac04` | allocations `0x10093648`, `0x10094789` |
| 4..7 | next link, initialized null then linked | `0x10094786`; getter/setter `0x100afa40`/`0x10040230` |
| 8 bits 0..2 | tile x within chunk | `0x1009479b`-`0x100947b6` |
| 8 bits 3..5 | tile y within chunk | `0x100947b9`-`0x100947d0` |
| 8 bit 6 | occupied flag, clear at load, set on figure creation | `0x10094819`-`0x10094822`, `0x1009385c` |
| 8 bit 7 | preserved by these packed-coordinate writes; meaning U | same ranges; OQ-577 |
| 9 | wrapping timestamp in units of 1000 input ticks | `0x100947f3`-`0x10094816`, `0x10093853`-`0x10093859` |
| 10 | list-category byte, initialized zero | `0x1009478f`; getter `0x100952b0` |
| 11 | not initialized or consumed in these paths; meaning U | allocation/read ranges above; OQ-577 |
| 12..13 | created figure's u16 id; not initialized by loader | `0x10093848`-`0x1009384f` |
| 14..15 | goods discriminator or `0x00ff` sentinel | `0x100947de`-`0x100947ef`, `0x10093864`-`0x10093871` |

**D — which table (`0x100cac04`, `0x100cac18`).** Both are primary tables of
separate allocation types, not evidence of a second interface at object+4.
The sixteen-byte spawn object uses `0x100cac04`: slot 0 returns byte `+0xa`,
slots +4/+8 get/set the next link, +0xc is the deleting destructor
`0x100952c0`, +0x10 returns this (`0x1007bb80`). The alternative `0x100cac18`
is installed at `0x1009421f` on a twelve-byte allocation: its category is byte
`+9`, initialized 8 (`0x100946b0`, `0x1009422c`), and its position packs four
bits per coordinate, unlike HIRSCH. Thus `0x100cac04` is the sixteen-byte
HIRSCH class; the HE RTTI name remains a structural label.

**D — route to creation.** The loader prepends objects into the same chunk
`+0xc` list and sets chunk flag bit 0 (`0x10094782`-`0x10094797`). The periodic
sweep's final list traversal passes them to `0x10092ca0` at `0x10093828`.
The loader itself creates no animal figure. Field `+0xc` becomes valid only
after successful materialization; copying it at load would invent state.

## Handover boundaries

Open questions OQ-572..577 in [open-questions.md](../open-questions.md) delimit
the unresolved cases above. OQ-578..579 remain reserved and unused. The next
reads are animal tick/routes/jobs and occupancy release (1b), then trapper,
predators and the independent reference (1c). No engine, test, fixture, default,
PRNG-timeline or save-identity change is part of this packet.
