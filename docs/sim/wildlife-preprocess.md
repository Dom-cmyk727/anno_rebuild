# Ground preparation before island fill

Research contract, 2026-11-19, WILDLIFE-PREPROCESS-CONTRACT-1. D = decoded GOG
2.0.0.5 instructions/data; P = parsed original file; H = hand derivation;
C = executed private conditional model; U = unproved. Addresses name AnnoWelt
unless explicitly marked AnnoGame. No original execution or HE parity is claimed.
This does not release implementation. See [postload](wildlife-postload.md),
[fertility import](wildlife-fertility.md), [resource import](wildlife-resource-initialization.md)
and the separate [raster FP investigation](../re/wildlife-raster-fp.md).

## 1. Gate, effects and concrete caller boundary

D: WORLD wrapper `0x100a1810–0x100a183c` takes `(rect, island)`; it tests island
byte +0xc2, calls island +0x1a4 only when zero, then calls +0x1a0 regardless.
The concrete preparation target is `0x10084370–0x10084818` (island table
`0x100cac2c`, entry `0x100cadd0`). It takes one borrowed half-open world
rectangle, not a tile point. The prepared marker is NOT set by this body.
FRUCHTBAR load sets it at `0x10089565`, even for all-zero contents; constructor
`0x1008cb41–0x1008cbc7` initializes it to zero. Reset
`0x1008c745–0x1008c7f1` also clears it (BL=0); it is not a
permanent once-prepared marker across island reset. The selected Tutorial's completed FRUCHTBAR
still suppresses preparation; this research broadens a different branch.

D: first, `0x1008438f–0x100843ed` visits the original 16 city-ring pointers.
Then `0x100843ef–0x1008440d` allocates W*H scratch bytes. Null returns through
`0x1008480e–0x10084818`, without numerical plane writes or a scratch release;
city getters have already run. The normal path frees scratch at
`0x10084801–0x1008480b`, then returns with ret-4 at `0x10084818`.
No success Boolean or rollback is provided to the wrapper: fill still follows
an allocation failure. Allocator `0x100b5f80–0x100b5f8d` delegates to
`0x100b72c3`; the heap arm `0x100b73be–0x100b73d6` uses HeapAlloc with flags 0,
not zero initialization. Other CRT small-block arms are not a zeroing contract.

D: output writes are only the low nibble of island +0x18 at
`0x1008478d–0x100847ad`. No HEIGHT/corner, GFXID, object/walk record, high nibble,
owner, generation or +0xc2 write occurs in this body. It changes persistent
terrain information; temporary scratch does not make it a cache-only operation.
Save `0x1008a3b0–0x1008a3f0` calls preparation when +0xc2 is zero, constructing
the full island rectangle from origin +4/+6 and extent +8/+0xa, then emits
FRUCHTBAR (`0x1008a3f6–0x1008a466`). Saving can therefore prepare the live plane.

D: resolved WORLD +0x1d4 load call sites are `0x100a1f9d`, `0x100a20b8` and
`0x100a2fc8`. Their rectangles are built from the loaded island header's origin
and dimensions (`0x100a1eec–0x100a1f1c`, `0x100a2010–0x100a203f`,
`0x100a2f0e–0x100a2f4b`). Freshly created matching islands give full-island
coverage; reused/substituted islands require an explicit geometry match.
The area-clear path instead invokes fill directly at `0x1008376e–0x10083796`;
it is not evidence of a partial preparation call. Slot-offset matches at
`0x10036777`/`0x10048331` belong to a two-argument interface; `0x100462e0`
has a different receiver and is not bound here to an island. Stack operands
with displacement +0x1a4/+0x1d4 are not virtual calls. U: this bounded scan of
Welt does not certify arbitrary external or substituted interface callers.

## 2. Inputs and their producers

| Input / lifetime | Addressed source and interpretation |
|---|---|
| Island geometry and planes | D: unsigned u16 W/H at +8/+0xa; origin +4/+6; +0xc eight-byte object records; +0x10 u16 GFXID; +0x18 effective byte plane (`0x100843ef–0x100844d2`). Exact backing lengths and positive dimensions are future validation prerequisites. |
| City prewalk | D: +0x38 array, 16 entries; nonnull city +0x60 -> `0x1002dfc0`, returning city+0x144 building index. Index table `0x100c2e48`: +0x24 -> `0x10023af0`, kind 0x21 count arm `0x10023b15–0x10023b2f`; +0xc -> `0x10023a90–0x10023a9d`, ordered pointers at +0x108. Only first nonnull building per city is queried through +0x74 then type property 0x1c. |
| Property 0x1c | D: common Game type getter `0x1000a860`, selected arm `0x1000a8d8`, copies DWORD +0x1d8 to output+4. That value is overwritten by the following catalogue property before any numerical use (`0x10084413–0x100844b5`). D: six checked production tables (base, workshop, farm, mine, fisher, trapper: 0x100c92b8/0x100c8010/0x100c8ae0/0x100c8990/0x100c2ac4/0x100c2bf0) share building +0x74 -> 0x100a3cb0–0x100a3cb3, returning stored type pointer +0x14. U: other dynamic building/type profiles are not certified. Empty rings require no such calls; nonempty profiles must bind rather than erase the prewalk. |
| Growth lookup | D: Game catalogue `0x10007530`, property 0x2e arm `0x10007566–0x10007578`, returns borrowed byte map +0x640c. Game `0x100073c4–0x100073e4` loads Data/Wachstum.dat through `0x10006a80`. `0x10006cee–0x10006d0a` allocates/zeros a 16,384-byte map; `0x10006d19–0x10006ddb` stores the low byte of each parsed signed integer, indexed by packed GFX word >>2. Unassigned cells remain zero. Catalogue owns this buffer, with release at `0x10007350`; callers borrow it only while catalogue lives. |
| Lookup packing | D: texture Id low byte contributes bits 8–15, row low 3 bits contribute bits 2–4, column low 3 bits contribute bits 5–7; low rotation bits are excluded, then shift right 2 (`0x10006d19–0x10006db1`). This is an original packed format, not a modern content-count cap. Consumer reads unsigned byte at `GFXID >> 2` (`0x10084514–0x10084527`). |
| Growth parser caveat | D: staging integers are cleared once per WACHSTUM section (`0x10006b4b–0x10006b6b`); Id resets row count, not staging (`0x10006be3–0x10006bfd`). Size changes dimensions (`0x10006c3c–0x10006c64`); each Proz row writes only parsed entries (`0x10006ca3–0x10006cc4`). Number-list parser `0x10016360–0x100163b9` continues only at a comma after integer/whitespace processing (`0x10016300`, `0x100161e0`). Missing commas do not imply zero-fill or whitespace-separated values. P: the supplied file has a short parsed row at line 765; retain previous staging entries. See existing OQ-029(g), not an invented repaired data rule. |
| Object blend metadata | D: resolve WHOLE u16 object via catalogue +0x70, then type +0x5c (`0x1008452b–0x10084546`). Group test admits raw kinds 225–256. Successful property 0x2e with nonzero value participates. Common Game arm `0x1000a979–0x1000a991` zero-extends byte +0x77; HEAD byte 0x19 populates it (`0x10009334–0x100094cd`, [production](../re/production.md)). Resource getter `0x1000c0c0` only specializes property 0x17 and forwards these properties to the common getter. |
| Climate | D: unsigned byte +0xc0; default zero at `0x1008cb8e`. INFO comparison `0x100897a6` uses string `0x100dcedc`; raw load `0x100897b6–0x100897ee` stores INFO[0]. Not inferred from a filename. |
| Kernel | D: image-initialized read-only .rdata at `0x100ca008`; consumer selects 25 bytes at base+25*climate (`0x10084670–0x10084698`). Coefficients are unsigned bytes. The private seven-profile extraction covers the selectors 0–6 documented at AnnoScene 0x1003b328 /
[link selection](../re/annoscene.md); it is not proof of the table's total length or permission to index every possible INFO byte. A supplied profile must establish membership/25 readable bytes. No public copy of original weights. |

## 3. Exact numerical contract and partial-rectangle hazard

D: `0x10084429–0x100844a4` intersects the world rectangle with island bounds,
then converts to local coordinates. Each phase scans y ascending, then x ascending.
Only phase-one rectangle cells are initialized (`0x100844ec–0x100845ff`).
Let p be the unsigned lookup byte. For a proved successful resource getter v>0,
`0x10084567–0x1008458b` uses floor((p+3v)/4) when p<v, otherwise
floor((v+3p)/4). Otherwise retain p. Store b=floor((p+5)/10) as a byte
(`0x1008458f–0x100845b5`). With proved byte providers b is 0–26, not silently
capped at 10; the multiplication/sums here fit signed 32-bit arithmetic.

D: if the centre scratch byte is zero, skip the output write entirely
(`0x10084661–0x10084666`): old low nibble survives, regardless of neighbours.
Otherwise visit its 5×5 neighbourhood, clipped to ISLAND bounds, and accumulate
in 32 bits, in row/column order:

    s = sum(floor(unsigned_kernel[dy+2,dx+2] * b[x+dx,y+dy] / 12))
    out = (old & 0xf0) | min(10, s)

D: each product is divided BEFORE it is added (`0x10084721–0x10084741`), not
one division after summation. Clamp/merge: `0x10084773–0x100847ad`. The kernel
position remains aligned to the centre; no edge normalization or kernel recentering
occurs (`0x1008469c–0x1008470d`). All numerical operations in the body and the
bound byte-property arms are integer operations: no world-PRNG call or FP
arithmetic. This is separate from the later raster's unresolved runtime FP modes.
U: this statement excludes arbitrary derived getters and CRT allocation hooks;
they are not certified transitively draw-free by an absent direct call.

D/U: allocation covers W*H, but phase two clips neighbours to the island,
not to the initialized rectangle. A nonzero centre in a proper partial rectangle
can read uninitialized, in-allocation neighbours. No zero halo is established.
Full matching load/save rectangles initialize every subsequently read byte.
An empty intersection does no cell work but still runs prewalk/allocation/query
and release. No general full-island API precondition was found; this is a
supported-caller boundary, not an explanation for arbitrary partial invocations.

## 4. Hand-derived acceptance examples (before private model)

These use authored kernels/bytes, not extracted original tables. All unspecified
kernel entries are zero; K(0,0) means its centre. Old output a7 is hex 0xa7.
H expectations precede the private model. C results and wrong-rule controls are
recorded in [evidence](../orchestration/evidence/wildlife-preprocess-contract-1.md).

| Row / primary rule | Authored input | Hand result / distinguishing wrong reading |
|---|---|---|
| Q1–Q4 / 8458f–845b5 | p=0,4,5,255; no object | b=0,0,1,26; signed byte 255 gives a different result. |
| B1/B2 / 84567–8458b | (p,v)=(20,100) or (100,20), admitted group | blended p=80 and b=8 in both; reversing which operand has weight 3 fails one branch. |
| B3 / 84541–8454b | p=20,v=100, raw kind 224 | no blend, b=2; admitting all kinds incorrectly yields 8. |
| S1 / 84721–847ad | 1×1 full, b=2,K(0,0)=6,old=a7 | s=1,out=a1; high nibble a retained. No edge normalization. |
| S2 / division position | 2×1 full,b=[1,1],K(0,0)=K(1,0)=6,old=a7 at x=0 | s=0+0=0,out=a0; aggregate division gives a1. |
| S3 / centre guard | 2×1 full,b=[0,2],K(1,0)=12,old=a7 at x=0 | a7 unchanged; summing despite zero centre gives a2. |
| S4 / zero contributions | 1×1 full,b=1,all K=0,old=a7 | a0; retaining a7 because sum=0 is wrong. |
| S5 / clamp and width | 1×1 full,b=26,K(0,0)=255,old=a7 | floor(6630/12)=552,clamped=10,out=aa; this clamped case alone does not distinguish byte-product wrap. |
| S6 / climate profile boundary | Same b=2,1×1; supplied centre weight 6 versus 12 | a1 versus a2. Validate selected profile; no universal climate-byte bounds claim. |
| S7 / edges, stride, immutable scratch | 2×1 full,b=[2,3],K(-1,0)=24,K(0,0)=6,K(1,0)=12,old=[a7,b7] | outputs [a4,b5]: left 1+3, right 4+1. Cropping keeps kernel alignment; reading rewritten output instead of scratch fails. |
| S8 / partial input | Same island, rectangle covers x=0 only,b[0]=2 | neighbour b[1] uninitialized: no original deterministic result asserted; proposed helper REFUSES. Zero-fill is not a reference answer. |
| S10 / product width | 1×1 full,b=26,K(0,0)=10,old=a7 | floor(260/12)=21,clamped=10,out=aa; byte-product wrap yields floor(4/12)=0,out=a0. |
| S9 / empty intersection | Disjoint world rectangle,old=[a7,b7] | no plane writes; no claim that original allocation/query are skipped. |

Static original rows are NOT_EXECUTED. Authored model checks exercise the decoded
integer contract only; they do not execute original load/save callbacks.

## 5. Narrow implementation proposal and remaining bindings

Propose, do not implement: a state-free helper in new
`src/sim_load/src/wildlife_ground_prepare.cpp`, with a distinct capability in
`src/sim_load/include/anno/sim_load/wildlife_ground_prepare.hpp`, beside the
existing postload fill. Bind validated positive dimensions,
matching origin/planes and a full-island rectangle; owned/retained lookup and
selected 25-byte kernel; whole-word type lookup with proved byte providers;
and either an empty city ring or independently bound nonempty getter chain.
The original fixed ring/map layouts are decoding facts, not engine content caps.
Use widened geometry/length checks and checked allocation; no arbitrary maximum.

Return an owned low-nibble-updated effective plane, preserving raw FRUCHTBAR,
high nibbles, metadata and inputs. Publish only on success; reject missing
profiles, invalid lengths and unsupported partial rectangles before any mutation.
That transactional refusal is an explicit defensive adaptation: the original
wrapper continues fill after failed allocation. Do not fabricate a completed
FRUCHTBAR header or set +0xc2 merely because this method returned.

Focused future tests in `src/sim/tests/wildlife_postload_fill_test.cpp`: the rows above,
asymmetric full planes and world-origin clipping, input lifetime, missing profile,
allocation/length refusal and immutability, plus existing fill composition and a
FRUCHTBAR-present skip control. C: the current
`prepare_wildlife_postload_fill` in `src/sim_load/src/wildlife_postload_fill.cpp`
requires a completed FRUCHTBAR payload (lines 59–70). An absent-column prepared
plane CANNOT be fed through that validator unchanged. A later packet would need
a distinct validated preparation capability and a narrowly shared private fill
body/overload in that file and its public header. Keep the existing
FRUCHTBAR-present validator and accepted results intact; never fabricate a
header to bypass it. Numerical fill can be shared after binding the same role
facts, but this composition is not implemented or released here. No change to
the accepted selected-Tutorial path is necessary; World/scheduler wiring stays out.

U / QUESTION for later scope: certify any nonempty-city building/type profile beyond the six
addressed +0x74 tables and common property arms before accepting that capability. The numerical
result being overwritten is not proof that arbitrary dispatch has no side effects.
Partial original invocations and other climate values likewise need their own
evidence; this packet offers explicit refusal boundaries rather than guesses.

## 6. Acceptance and review disposition

| Requirement | Positive / counterexample | Actual evidence | Independent disposition |
|---|---|---|---|
| Presence gate and precise writes | c2=0 prepares; completed zero FRUCHTBAR skips; high nibble retained | D wrapper/loader/body; H/C S1,S4; original callback NOT_RUN | pending orchestrator review |
| Catalogue/type provenance | Bound byte properties and six +0x74 tables; refuse unmatched providers | D table/getter receipts; P growth file; no engine binding implemented | pending |
| Arithmetic and clipping | Full 1×1/2×1; aggregate/guard/width/merge wrong outputs differ | H/C 17 positive rows, 6 wrong-rule checks; static-only world clipping/callers | pending |
| Scratch initialization | Full rectangle initializes all reads; proper partial cannot promise values | D two-pass body; C S8 refusal model; original unsafe partial NOT_RUN | pending |
| Climate source and ownership | INFO[0], image kernel, retained catalogue buffer | D/P finite source receipts; unknown climate membership stays U | pending |
| Scope and compatibility | Docs only; no state/default/production/fixture changes | Diff/link/hash checks in evidence; selected Tutorial unchanged | pending |
