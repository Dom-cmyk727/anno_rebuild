# Wildlife: origins and animal life (WILDLIFE-RE-1a / 1b)

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

## 1a handover boundary (retained context)

OQ-572..577 in [open-questions.md](../open-questions.md) delimit the original
1a gaps. Sections 4–7 below take over animal life and occupancy release in 1b.
The accepted 1a read and its [review](../orchestration/reviews/2026-11-12-wildlife-re-1a-review.md)
remain the source for sections 1–3. Hunter approach, trapper and predators belong
to 1c; the reference script belongs to 1d. Neither packet changes engine state.

## 1b acceptance scope

The following is an addressed GOG read, not a port or an original-runtime
experiment. Independent address verification is pending the orchestrator.
Unresolved transitive calls remain explicit rather than contributing zero draws.

| Requirement | Positive proof | Distinguishing case | Disposition |
|---|---|---|---|
| Animal class/control selection | factory `0x1009a4a1`, tables `0x100c653c`/`0x100c66fc` | generic route 62 returns null; Old's factory builds it | D; Old runtime reachability U, OQ-580 |
| Flags and spawn release | HP setter `0x100349c0`, island callback `0x1008bc80` | ending 2 with guard 4 clear differs from terminal removal; no saved-id match | D for inspected paths; bypass callers U, OQ-579 |
| Construction effects and draws | `0x10097250`, `0x10052c10`, property/cargo/attach handlers | `0x200` adds quantity; first walk occurs after creation | D for bodies; transitive total U, OQ-578 |
| Exact walk draw order | `0x10058360`, both candidate loops, decoded state jump tables `0x10051a80`/`0x10051c4c` | six draws even on failed search; %5 follows walk, parity follows idle | D direct sites; shared callback closure U, OQ-578 |

## 4. Animal tables, tick, movement and death (1b, 2026-11-12)

**D — comparison boundary.** Compare the 101 entries of Boden `0x100c35a8`,
WildTier `0x100c653c` and Zivilist `0x100c6a98`, and the 104 entries of
WildTierNew `0x100c66fc`. The constructor `0x10035ce0` installs Boden; the
animal constructors `0x10050db0` and `0x10052c10` replace it. The following is
the complete set of offsets at which either animal table differs from Boden.
An equal pointer means inherited code, not a fresh animal implementation. Names
remain the structural labels of the edition comparison; HE re-reading is NOT_RUN.

| Slot offset | Boden | WildTier | WildTierNew | Zivilist |
|---|---|---|---|---|
| `+0x1c` | `0x10001060` | `0x10052d10` | `0x10052d10` | `0x10053f90` |
| `+0x28` | `0x10035c10` | same | `0x10052a30` | `0x10053fa0` |
| `+0x48` | `0x10035c70` | `0x10050d30` | `0x10052b00` | same as Boden |
| `+0x4c` | `0x10035ca0` | `0x10050d00` | `0x10052aa0` | same as Boden |
| `+0x74` | `0x10034760` | same | `0x10051750` | same as Boden |
| `+0x7c` | `0x100332b0` | same | `0x10051000` | `0x10053a10` |
| `+0x84` | `0x10001080` | `0x10050c00` | `0x1003b6d0` | `0x10053f40` |
| `+0x88` | `0x10034af0` | `0x100508b0` | `0x10052440` | same as Boden |
| `+0x8c` | `0x10034b10` | `0x100508e0` | `0x10052570` | same as Boden |
| `+0xa4` | `0x10037e90` | `0x10050c10` | `0x10001080` | `0x10053e30` |
| `+0xc8` | `0x10037e90` | `0x100507e0` | `0x10052330` | `0x10053ad0` |
| `+0xcc` | `0x10034df0` | `0x10050910` | `0x10052640` | same as Boden |
| `+0xdc` | `0x10035170` | same | `0x100518b0` | same as Boden |
| `+0xfc` | `0x10001030` | same | `0x10052190` | same as Boden |
| `+0x100` | `0x10031d90` | same | `0x100521f0` | same as Boden |
| `+0x110` | `0x100afde0` | `0x10050d70` | `0x10052b40` | `0x10053f50` |
| `+0x114` | `0x10037580` | same | `0x10052d20` | same as Boden |
| `+0x164` | `0x10099840` | same | `0x10052270` | same as Boden |
| `+0x168` | `0x10034940` | same | `0x10051a90` | same as Boden |
| `+0x170` | `0x10054080` | same | `0x100537f0` | same as Boden |
| `+0x174` | `0x10034350` | `0x10050ce0` | `0x10050ce0` | `0x10053d20` |
| `+0x178` | `0x10034380` | same | `0x10051c60` | same as Boden |
| `+0x17c` | `0x100343c0` | same | `0x10051cc0` | same as Boden |
| `+0x194` | absent | absent | `0x100519d0` | absent |
| `+0x198` | absent | absent | `0x100519f0` | absent |
| `+0x19c` | absent | absent | `0x10051b00` | absent |

**D — flags are a word at figure `+6`, not at figure `+0x8c`.** Slot `+0x8c`
is a query: argument 0 falls through to `0x10034b10`, which returns that word
(animal dispatchers `0x100508e0`, `0x10052570`). Bit 1/value 2 is the ending
state; bit 2/value 4 is the terminal-removal guard. They are distinct. A figure
can be ending with value 2 set and value 4 clear, the window used by the
[hunter's existing kill read](hunter.md). Other query arguments include 7
(behavior byte `+0x78`), 8 (target id `+0x74`, or New's herd lookup), and New's
12 (configuration dword `+0xa0`) and 13 (word `+0xa4`), at
`0x100508e0`–`0x10050905`, `0x10052570`–`0x100525db`.

**D — New tick (`0x10051750`–`0x100518a3`).** With a nonzero rate at `+0x40`,
convert the wrapping u16 difference between input time and stamp `+8` by the
float constant at `0x100c3270` (approximately 0.2). Consume the smaller of
available motion time and the segment's remaining time `+0x3c`; integrate
position `+0x24/+0x28/+0x2c` using velocity `+0x30/+0x34/+0x38`, retaining
surplus for another boundary. At a boundary, ending bit 2 takes priority: stop
rate, set terminal guard 4 if clear, ask world `+0xa0` for the manager and call
manager `+4(this,0)`. Otherwise call `+0x18c`, then `+0x17c` when a motion
segment exists, `+0x178` when requested action `+0x84` is nonzero, or `+0x174`
when neither exists. This body has no direct world draw. The Old table uses
Boden's tick `0x10034760`–`0x100348a5`, whose action boundary drives route/job
objects instead of New's three extra virtual methods. No fixed “one draw per
tick” rule follows from either loop.

**D — New motion and interruption.** `0x10052640`–`0x10052899` asks the supplied
searcher for a path, copies its start position, builds motion descriptors and
uses type speed multiplied by figure `+0x9c`. `0x10051cc0`–`0x100520fa`
consumes those descriptors, switches animation/moving flags, interpolates height
through island `+0xa4`, and advances the linked segment pointer. Before this it
subtracts 8 from carried quantity; an empty result applies damage 5 through
`+0x9c`. It also decrements `+0xb0` by the wrapping u16 time difference and
sets HP to zero if the budget is exhausted (`0x10051cc0`–`0x10051d43`). The
constructor initializes this budget to 25000 (`0x10052cba`–`0x10052d02`);
it is a segment-consumption budget, not an established wall-clock lifespan.
A terminal motion descriptor sets ending bit 2 and invokes `+0x110`
(`0x10051e03`–`0x10051edb`). None of these bodies directly draws from world
`+0xb4`; the shared searcher closure remains OQ-578.

**D — supporting New overrides.** `0x100518b0`–`0x100519c2` interrupts motion
and snaps its continuation; `0x10051a90`–`0x10051afa` cancels action, releases
a claimed stand for states 25/26 via `0x100330b0`, clears state and route pointers
and copies position to the motion origin. `0x10052190`–`0x100521e1` rescales
moving velocity/rate when the speed multiplier changes; `0x100521f0` retrieves
it. `0x10052270`–`0x10052324` and `0x10052330`–`0x10052432` set the search
rectangle around home `+0x68/+0x6a`, radius byte `+0x11`, or current position
when configuration bit `0x10` is set. Old's corresponding search setup is
`0x100507e0`–`0x100508a1` and path conversion `0x10050910`–`0x10050b66`.
These bodies contain no direct world draw. `0x10052d10` returns island;
`0x10052d20` returns the state block at `+0x84`.

**D — setters and display.** Old `0x100508b0` and New `0x10052440` set the
behavior byte/target; New also handles stopping/restarting motion, configuration
and herd membership (`0x100525e0`). New's display override
`0x10051000`–`0x1005118c` uses the CRT random helper `0x100b68a9` for sound
selection at `0x10051107` and `0x10051120`, not the world's `+0xb4` stream.
These display draws must not be inserted into a world-PRNG reference sequence.
Configuration/herd activation by external callers remains OQ-581.

**D — death is cleanup before registry removal.** Shared HP setter
`0x100349c0`–`0x10034a8e` cancels action, zeros velocity, installs a 15.0
remaining-time descriptor at rate approximately .01, clears moving bit 1,
sets ending bit 2, selects action `0x11`, and calls virtual `+0x110` immediately.
It does not set guard 4 there. `0x10050ce0` applies damage 3 at an exhausted
animal action; `0x10034ac0` subtracts damage from HP and dispatches the HP
setter. Normal cleanup releases the spawn before the later tick's removal
window ends. No direct world draw occurs in these death bodies.

## 5. Construction and attach: OQ-575

**D — selected implementation (`0x10097250`–`0x100974ac`,
`0x1009a110`–`0x1009a755`).** Manager table `0x100ccd74` slot 0 creates and
slot `+0x60` selects the class. The latter dispatches on type kind (type slot
`+0x50`), using `0x1009a7e8` and `0x1009a774`. Kinds 49 and 51 select
`0x1009a4a1`, allocate `0xbc` and call `0x10052c10`: **WildTierNew**, not Old.
This does not assert that wolf/predator kinds use the same constructor. Old's
`0x10050db0` constructor installs `0x100c653c`; its runtime reachability is
not established by this selected branch (OQ-580).

**D — initial state (`0x10035ce0`–`0x10035d35`,
`0x10052c10`–`0x10052d02`).** Base sets flags 8, stamp zero, action `0x13`,
zero velocity, small positive motion remaining/rate, and null world/type/island/
route pointers. New initializes requested state `+0x84=27`, current state
`+0x85=0`, behavior `+0x78=4`, target `+0x74=-1`, radius `+0x11=7`, speed
multipliers 1, configuration `+0xa0=4`, count/list fields zero and the budget
25000. It exposes its carried-goods object at `+0x6c` through
`0x1003b6d0`; Old's equivalent is `+0x68` through `0x10050c00`.
No world draw occurs in either constructor or the Boden constructor.

**D — quantity, not capacity.** Cargo table `0x100c3978` slot `+0xc` is
`0x1002ff50`: add its argument to the u16 quantity at subobject `+6`.
New's constructor adds `0x200`. Type attach `0x10052a30`–`0x10052a9a` sets
type pointer/HP and, for type ids 128, 129, 529 or 1206, adds another `0x600`.
Thus those types start with `0x800` after both operations, not a `0x600`
capacity limit. Cargo slot 0 (`0x1002ff40`) writes the goods byte at `+4`;
`+4/+8` (`0x10037560`/`0x10037570`) retrieve goods/quantity, and `+0x10`
(`0x1002ff70`) subtracts with a floor at zero. These operations do not draw.

**D — manager and helper order.** Manager obtains a free id, allocates the
class, attaches type and world through figure `+0x28`, assigns id, sets
property `0x23` (the time stamps, base handler `0x10035ca0`), copies the
creation vector through `0x100348b0`, and links its registry/spatial entries
(`0x10097250`–`0x100974ac`). It does not start the animal's tick in this
body. After return the helper sets property `0x2f`, cargo goods `0x45`, then
island (`0x10092f5b`–`0x10093013`). New's property setter
`0x10052aa0`–`0x10052af5` copies the rectangle's anchor u16 x/y to
`+0x68/+0x6a` and to the current tile at `+0x64/+0x66`; Old's
`0x10050d00`–`0x10050d2b` saves its home at `+0x64/+0x66`. New's
`0x10052a30` delegates world/island attach to `0x10035c10`–`0x10035c67`:
class `0xf00` stores world `+0x18`, class `0x1300` stores island `+0x1c`.
The property getters `0x10050d30` and `0x10052b00` return a one-tile rectangle
at the saved home, not the animal's current moving position.

**D/U — draw conclusion, bounded.** There is no additional world draw in
those manager/constructor/property/cargo/attach bodies on the selected branch.
The first six direct walk draws belong to a later action start, not creation
(`0x10051c2f`, `0x100519f0`, `0x10051480`). This narrows OQ-575. A complete
transitive certificate still needs the actual runtime type-interface bindings
and the shared callbacks described in OQ-578; a local absence of `+0xb4` does
not prove every virtual callee pure. In particular, the AnnoGame figure-type
tables `0x10021d10`/`0x10021e8c`/`0x10021f58` expose the simple HP getter
`0x1000fce0`, but their `+0x64` signature (`0x1000fd10`) alone does not explain
all animation-duration calls from AnnoWelt. Do not silently identify interfaces
by an equal slot offset across modules. AnnoGame addresses in this paragraph
refer to **AnnoGame.dll**, not AnnoWelt.dll.

## 6. Spawn release: OQ-573

**D — release callback (`0x1008bc80`–`0x1008bde5`).** The in-game island table
`0x100cac2c`, slot `+0x148`, receives the animal. Get its property-`0x2f`
home rectangle and type kind. Kinds 49..64 take the animal-family arm
(`0x1008bd05`–`0x1008bd0d`). Optional follower detachment precedes lookup
when configuration query 12 has bit 2. Derive the chunk using home minus island
origin divided by 8, and within-chunk coordinates from the home coordinates'
signed remainder by 8. Call category-0 lookup `0x1007c630`. If found, clear
spawn byte `+8` bit `0x40` at `0x1008bdd9`. **No saved-id comparison and no
write to spawn `+0xc` occur in this callback.** Timestamp and saved goods are
also retained. Lookup is by home position, not the animal's current chunk.

**D — paths that reach it.** Animal cleanup slot `+0x110` is Old
`0x10050d70`–`0x10050daa` or New `0x10052b40`–`0x10052c01`. Both release
motion storage and route/job state, invoke island `+0x148` while island is
nonnull, then clear the island pointer. New also detaches its follower list.
Shared HP death calls this slot immediately (`0x10034a7e`–`0x10034a88`);
New terminal motion calls it at `0x10051ec9`–`0x10051edb`. Ordinary position
integration in `0x10051750` does not invoke this callback on crossing a chunk.
Do not make leaving the home chunk itself a release rule.

**D — immediate deletion is different (`0x100974b0`–`0x10097553`).** Manager
`Remove(figure,1)` unlinks spatial storage and directly destroys the registry
object. It does not first invoke figure `+0x110`. The common deleting wrapper
`0x10031db0` calls `+0x170`; Old's `0x10054080` reaches destructor
`0x10050e00`, New's `0x100537f0` reaches `0x10053810`–`0x100538c1`.
Those destructors free motion/list storage without the island release call.
Consequently deleting a still-attached live animal this way can leave its
spawn occupied. This is a path-level counterexample, not evidence that an
ordinary unmodified game reaches it for an unended animal (OQ-579).

**D — hunter distinction.** The already-read hunter kill path
`0x100547e0` checks ending bit 2 set and terminal guard 4 clear before its
immediate manager removal; its victim's HP-death cleanup has already released
the spawn. Preserve that order from [hunter.md](hunter.md), without adding a
second id-based release in the kill step. A stale saved id can remain after
normal release; it is not the occupancy predicate (`0x1008bdd9`,
`0x100934f7`–`0x10093578`). OQ-573 is settled for these paths; whole-game
reachability of bypass deletion remains OQ-579.

## 7. Routes, stand selection and exact walk draws

**D — route 62 is class-specific.** Generic factory `0x1003b0e0` uses the byte
map `0x1003b480` and target table `0x1003b450`. Route 62 maps to default
`0x1003b43a`, returning the zero initialized at `0x1003b0ff`; routes 63/64 do
likewise. Old's own `+0xa4` at `0x10050c10`–`0x10050cac` accepts 0 or
62..64, allocates `0x5c`, calls base constructor `0x1005e390`, then installs
`0x100c646c`. New's `+0xa4` is the null stub `0x10001080`; its ordinary life
uses the internal action states below. This corrects the premise that the
generic switch itself constructs HIRSCH.

| Structural label (D: GOG table/call profile; U: fresh HE comparison) | GOG table | Identification |
|---|---|---|
| `tRouteWildTier` | `0x100c7348`, 22 slots | installed by `0x1005e390`; transition slot `+0x34=0x1005e2b0` |
| `tRouteHirsch` | `0x100c646c`, 22 slots | installed by animal factory `0x10050c8e`; same as base except deleting destructor `+0x44=0x10050cb0` |
| `tJobWalkRandom` | `0x100c64ec`, 10 slots | step `route+0x50`; start `0x10058450`, job kind 12 (`0x10058670`) |
| `tJobSteheWildTier` | `0x100c64c4`, 10 slots | step `route+0x58`; kind 3 (`0x10054e90`), start `0x10054ea0`, completion `0x10055060` |

**D/U — labels and stand jobs.** The last table differs from the generic
kind-3 table `0x100c70f0` at start/completion, retaining eight identical entries;
its completion consumes `0x100` carried quantity (`0x10055060`). This is a
structural identification, not an HE RTTI proof (OQ-580). The route's other
steps are `+0x40` table `0x100c6514`, kind 7, start `0x100594b0`, and
`+0x48` table `0x100c4858`, kind 11, start `0x100553e0`. Kind 7 finds a
habitat destination; kind 11 waits using type `+0x64(1,7,time,0)`. Kind 3
waits using `+0x64(3,0,time,0)` (`0x10054ea0`). Do not label all three
“standing”: one is a search/move and two are timed actions.

**D — Old route control (`0x1005e390`–`0x1005e44a`,
`0x1005e2b0`–`0x1005e37f`).** The embedded linked nodes are route `+0x10`
(kind 7), `+0x1c` (kind 11), `+0x28` (kind 12), `+0x34` (kind 3).
Kind 7 completion advances to kind 11; kind 11 returns to kind 12; kind 12
advances to kind 3. At kind 3 completion, nonempty carried quantity returns
to kind 12 without a draw; empty quantity draws once and selects kind 7 for
odd, kind 12 for even. Generic start `0x1005b6c0` invokes the chosen step;
failed start calls route fallback `0x1005e280`, which sends a failed kind-7
search to kind 12. The kind-12 callbacks `0x100588a0`/`0x100588b0` reset the
walk-attempt byte to zero at completion/cleanup, not at each candidate.

**D — habitat targets (`0x100594b0`–`0x100595d5`,
`0x10051190`–`0x100513d5`).** Both Old's kind-7 start and New state 26 ask
island `+0x10c(rect,0,goods,buffer)` for unclaimed resource tiles in their
search rectangle. For the in-game island this is `0x10092320`:
its category-0 arm scans clipped rows then columns, resolves each nonempty
object's output-0 goods, and rejects tile dword `+4` bit `0x20000000`
(`0x100923ca`–`0x1009246d`). It does not randomly choose one coordinate.
Candidates go to the searcher; a found goal is marked through island
`+0xc0(x,y,0,1)` (`0x10059597`, `0x10051399`). That operation writes the
claim bit (`0x100878a5`–`0x100878ec`); completion/cancel clears it
(`0x100545c0`, `0x100330b0`). New behavior byte 5 with a valid target id
additionally filters candidates by the minimum squared-distance band
(`0x1005121d`–`0x10051359`); it does not add a direct world draw there.
The pathfinder's goal/tie semantics and transitive draw closure are OQ-578.

**D — New action machine (`0x100519d0`–`0x10051c4b`).** Requested/current
bytes are `+0x84/+0x85`, with the behavior copy at `+0x86`. Current 0 adopts
the request (request 25 begins at 26). State 25 is a stand/resource action
(`0x10032ef0`/`0x10032f30`), 26 is habitat search, 27 random walk and 28
idle. Its completion releases the tile claim, calls island `+0x110` for the
resource operation and adds `0x100` carried quantity only on success
(`0x10032f30`–`0x10032f96`). Completion of 26 goes to 25 without a draw;
completion of 25 or 28
uses `(~draw & 1) | 26`, so odd selects 26 and even selects 27. Completion
of 27 draws modulo 5, selecting 28 on zero and 26 otherwise. If 26 was
selected but carried quantity is greater than twice the truncated third of
type quantity, override it to 27 (`0x10051bf6`–`0x10051c25`); the preceding
draw is still consumed. A failed start falls from 26 to 27, from 27 to 28,
then to zero (`0x100519d0`, `0x100519f0`); the next start follows the
completion selection in the same boundary call `0x10051c60`.

**D — walk candidates (`0x10058410`–`0x10058525`,
`0x10051480`–`0x10051733`, helper `0x10058360`–`0x100583ff`).** Prepare
three candidates. Descriptor min/max radii are 2.5/4.5; angular half-span is
`trunc(256*s)` where s=.5 while the attempt byte is <2, otherwise 1
(Old job `+4`, New figure `+0x79`; setters `0x10052200`/`0x10052230`).
For each candidate draw radius first, angle second. Radius is
`min + (draw % trunc(16*(max-min)))/16`, with the original float stores.
Angle index is `(uint8((facing+2)<<6) + draw%(2*half_span+1) - half_span)&511`:
the byte wrap before widening at `0x100583c6`–`0x100583cf` matters. Output
uses table[index] for x and table[index+128] for z, each multiplied by radius.
This is **six world draws**, not one chosen direction, before search can fail.
The attempt byte increments after all candidates are supplied.

**D — vector tables and bounds.** New uses `0x100e10f0`, Old `0x100e1b50`.
Initializers `0x10050fb0`–`0x10050ffe` and `0x100541f0`–`0x1005423e`
produce 512 float sine entries with x87 `fsin`, accumulating a constant step,
then copy entries 384..511 to entries 512..639. The copy source is destination
minus `0x200` bytes (`0x10050fe8`, `0x10054228`), not a wrap to entries 0..127.
Thus the `index+128` lookup must preserve this tail rather than silently replace
it with a mathematical cosine. They draw no world randomness. New adds the offsets to current position (or the
resolved target's position for behavior 5) and constrains candidates to the
radius-7 home/current search rectangle: below its low edge reset to low+3,
at or beyond its high edge reset to high-4 (`0x10051583`–`0x100516bc`).
This is not ordinary saturation to the nearest edge. Old adds to current
position without this New clamp (`0x100584c8`–`0x100584e9`). New behavior
7 first draws modulo 6; remainder 1 doubles both radii, producing modulus 64
instead of 32 for each radius draw (`0x1005150c`–`0x10051549`). It does not
change candidate count or angular modulus. Exact cross-host x87 reference
vector reproduction is not established by a Python/libm substitution (OQ-582).

### Animal world-draw order (read scope and gaps explicit)

**D — ordering.** These rows extend section 1's sweep/creation table. All world
calls below resolve world from figure `+0x18` or the Old job's figure world;
slot `+0xb4` is `0x100a1740`. Completion precedes start, and candidate order is
0,1,2. A failed start can therefore consume walk draws and then an idle draw
in one boundary; the outer tick can process several boundaries with surplus.
The table enumerates direct sites in the animal/route/job bodies read here.
**U — it is not yet a certified total for arbitrary transitive callbacks**:
OQ-578 is the explicit remaining closure, not an assumed zero row.

| Position in tick/call | Draw address | Modulus/mask | Selects | Condition |
|---|---|---|---|---|
| New completion, before next start | `0x10051b77` | `(~draw &1)\|26` | 26 for odd, 27 for even | successful state-25 completion or state-28 completion |
| New walk completion, alternative to preceding row | `0x10051b9c` | `%5` | 28 if zero, otherwise 26 | state 27 completes |
| New walk start, before candidate loop | `0x10051515` | `%6` | double radii if remainder 1 | behavior copy is 7 |
| Walk candidate 0, radius | `0x10058382` | `%32`, or `%64` after double | radius in sixteenth units above min | every candidate generation |
| Walk candidate 0, angle | `0x100583b2` | `%257` or `%513` | centered angular-table index | attempt byte <2 or >=2 respectively |
| Walk candidate 1, radius then angle | same two sites | same respective moduli | second candidate | always after candidate 0 |
| Walk candidate 2, radius then angle | same two sites | same respective moduli | third candidate | always after candidate 1 |
| New idle start | `0x10051441` | `%n`, n=1 + available variants 5/6 | animation variant passed to type timing query | state 28 starts, including fallback from failed walk; n can be 1, still draws |
| Old kind-3 completion | `0x1005e35d` | `&1` | kind 7 for odd, kind 12 for even | carried quantity is zero after completion; nonempty skips draw |
| Indirect construction/type/search/callback closure | **U — OQ-578** | **not certified** | do not replace with zero | until actual receiver tables/callees are established |

**D — distinguishing arithmetic checks (hand-derived, not running-original
measurements).** From `0x10058382`/`0x100583b2`, ordinary attempt 0 uses
moduli 32 then 257 three times; after the byte reaches 2 it uses 32 then 513.
A radius remainder 31 gives 4.4375, not 4.5. For facing 0, half-span 128 and
angle remainder 0, index is 0; remainder 256 gives index 256. With half-span
256 and remainder 0, index is 384 after the mask, not a negative index.
Table entry 512 copies entry 384, not entry 0 (`0x10050fe8`).
New completion draw 0 selects 27; draw 1 selects 26 (`0x10051b77`). An
idle modulus of 1 still advances the world stream once (`0x10051441`). These
cases distinguish common wrong readings; executable vectors remain 1d.

## 1b handover boundaries

OQ-573 is narrowed to concrete cleanup versus immediate-deletion paths; OQ-575
has concrete constructor/handler effects and no additional direct draws.
New OQ-578..582 in [open-questions.md](../open-questions.md) retain transitive
closure, bypass-deletion reachability, Old/class-label reachability, external
behavior/herd activation and float vector reproduction. OQ-583..585 are unused.
HE and running-original validation are NOT_RUN; independent address re-check is
pending the orchestrator. No engine code, test, fixture, save identity or default
rule is changed. The direct-site draw table must not be advertised as a complete
lifecycle reference until OQ-578 is resolved.
