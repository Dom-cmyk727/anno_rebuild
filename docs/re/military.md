# Military: first single-figure order and soldier update read

MIL-RE-1, 2026-11-18. **Static GOG 2.0.0.5 evidence only.** Baseline
`b5c0ada209d94a0082bc88d793c917c7222e6a21`. This answers part of OQ-753's command
and tick question; it does not close combat, AI, formations, healing, theft or boarding.
No original runtime or History Edition execution/comparison was performed.

Evidence: **D** instruction/control-flow read; **P** parsed original data;
**S** structural comparison, including class labels inherited from another edition;
**I** inference explicitly bounded below; **U** unread or unproved. Addresses are in
AnnoWelt unless prefixed Frame (AnnoFrame) or Game (AnnoGame). Reproduction uses the
existing `tools/disasm.py` and private PE/Capstone table/caller probes; Python 3.14.6,
Capstone 5.0.7, pefile 2024.8.26. Raw listings remain private, never fixtures.

| Original input | SHA-256 |
|---|---|
| AnnoWelt.dll | f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2 |
| AnnoFrame.dll | dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91 |
| AnnoGame.dll | ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc |
| FigurenHeader.dat | 29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8 |

## 1. Evidence map and class boundary

The preserved `docs/UNITS.md` Parts C/D2–D3 and OQ-753 at reference pin `95897a55`
were leads, not merged evidence. Earlier unit/import/facing reads are indexed by
[scenario units](../formats/sze.md#units-schiff-soldat),
[original import](original-unit-import.md) and [ship navigation](ship-navigation.md).

| Question | Primary evidence | Observed result / confidence | Remaining dependency |
|---|---|---|---|
| Base soldier identity | `0x10049c30–0x10049cb5`; factory `0x1009a244–0x1009a26f` | D: code 99 allocates 0x98, calls base-ground constructor and installs table `0x100c561c` | S: name tBewohnerSoldat comes from the preserved HE class comparison, not GOG RTTI |
| Ground base | `0x10035ce0–0x10035d35` | D: installs `0x100c35a8` before soldier replacement | Full base behavior not reread |
| Name for code 99 | Game table `0x10024cb8`, element 99 → `0x10025bb4` | D: SCHWERT; sampled 98/100/106/108/110 also rechecked | No broad figure-field reinterpretation |
| Subclasses | constructors `0x1004a660`, `0x1004a4e0`, `0x1004afb0` | D: call `0x10049c30`, replace with `0x100c59e8`, `0x100c5804`, `0x100c5bd8` | Medic/thief/tower full ticks excluded |
| One order | Frame `0x1005d410–0x1005d503` → `0x1005d380–0x1005d403` → `0x100591f0–0x1005921b` | D: command 0x6c, actor lookup then contextual target request | UI selection origin and sender entitlement U |
| Actor and target admission | `0x100489ce`, world slot +0x194 = `0x100a6c80–0x100a6ffa` | D: classifier precedes new order-state writes | All diplomacy/special-target helper semantics U |
| Corresponding update | table +0x74 = `0x10049200–0x10049345` | D: segment integration and ordered dispatch | Geometry/impact/draw closure U |

**D/P — data route is not this order.** Parsing the 368-entry FigurenHeader with
`tools/figuren.py` selected 56 entries across the 15 land-army/thief names listed by the
reference; every selected ROUTETYPE is UNUSED. This is a parsed-data observation,
not proof that no runtime route object is ever created. The current sim maps military
BEWOHNTYPE names in `src/sim/src/types.cpp`, but its original-unit import contract admits
classes 53/97/113, not a land-army implementation
([scope](../sim/original-unit-import.md#supported-actor-classes-and-refusal)).

**D — correction to the reference lead:** factory code 108 at `0x1009a2a4–0x1009a2f7`
allocates 0xac and calls `0x10049c30`, then installs **`0x100cb66c`**. Its final table
is not simply `0x100c561c`. Do not extend the ordinary soldier's tick assumptions to
artillery on the basis of the shared constructor. No HE class-name match for that final
table is established here.

**D/S — relevant table differences only.** Against ground, soldier overrides update +0x74,
order +0xe4, idle +0x174, order dispatch +0x178, segment hook +0x18c. Soldier's additional
+0x1c4/+0x1c0 are `0x10046220`/`0x10045fa0`. At these selected slots, medic shares the
base pair but changes +0x18c to `0x1004a680`; thief changes them to
`0x10049fb0`/`0x10049f40`; tower shares them but changes +0xe4 to `0x1004ba00`.
The comparison is a table read, not a read of those subclass consumers.

## 2. One contextual figure-target order, including its attack branch

**D — producer.** The base and multiplayer client tables `Frame 0x1008914c` and
`0x100893ec` put `0x100591f0` and `0x1005d410` at slot +0x80. The multiplayer producer's
submission buffer contains command kind 0x6c, actor id u16 (figure slot +0x40), target-kind
u8, then the target data. Figure id getter `0x10035be0–0x10035be6` zero-extends object
word +4. For target-kind 2, the producer copies a list of u16 ids; kind 1 copies a
three-component position; kind 4 copies four-byte target records. Only kind 2's single
target is followed here (Frame `0x1005d424–0x1005d4e4`). These describe the submission
buffer, not a newly specified network packet format.

**D — sink and receive.** Producer calls client +0x144 at Frame `0x1005d4f4`, resolved to
`0x10060710`; its non-suppressed branch copies the submission, then calls connection
`[client+0x3c4]` slot +0x18 (`0x1006074c–0x1006078e`). The turn dispatcher
`0x1005ffd0–0x10060385` selects command 0x6c's arm `0x1006007f`, calling decoder
`0x1005d380`. That decoder's body begins actor id u16, kind u8, tail: lookup actor via
world +0xa0 / registry +8; absent actor returns. Kind 2 derives its count as
`(payload_length - 3) >> 1` and calls base `0x100591f0` with tail, count and actor
(Frame `0x1005d383–0x1005d403`). Sender authentication and malformed-payload bounds are
not established by that lookup/count arithmetic. Full socket/turn framing is the separate
[tick-loop read](tick-loop.md#4-single-player-vs-clientserver-oq-005), not re-proved here.

**D — apply order matters.** Base Frame `0x100591f9` calls actor +0xdc **before**
+0xe4 at `0x10059214`, passing a zero flags word to the latter. For the ordinary soldier,
these resolve to `0x10045c30` and `0x100489b0`. Thus a refused new request is not evidence
that the previous action was untouched: the reset path has already run. This frame path
contains an actor id, not an issuer/player ownership proof. The UI selection and network
sender-to-owner edge remain U; no rebuild authentication rule follows from the original.

**D — classifier order, target-kind 2.** Soldier first calls world +0x194; zero returns
without writing the new order bytes (`0x100489c6–0x100489d8`). The world first checks actor
availability (`0x100a5f70`, mask 0x200e), then target registry lookup, self-target shortcut,
missing target, and target property-0 mask 6 (`0x100a6cbb–0x100a6d0f`). Only afterward does
it obtain owners through +0x158 (`0x100a5dc0`, soldier +0x1c returns its +0x64 owner pointer)
and try relationship/special-target branches. A request may classify as something other
than attack; command 0x6c is **not** an unconditional attack opcode.

**D — attack-selected branch.** At `0x100a6e18–0x100a6e6b`, relationship/force predicate,
attacker capability (+0x174 = `0x100a60a0`) and target compatibility
(+0x17c = `0x100a6140`) can yield classifier result 3. Compatibility checks target
presence, flags 6, nonzero type maximum HP, a type callback, and rejects flag classes
0x80/0xc0 (`0x100a6140–0x100a61b7`). The full meaning of all its indirect callbacks is U.
For kind 2 and result 3, setter writes requested state **10**, zero active state/options,
and target id u16 at +0x78 (`0x10048bff–0x10048c1a`). Other classification arms include
walking toward the target's current position; do not conflate them with result 3.

**Worked synthetic discriminator (D-derived, not original runtime).** Actor id 7, kind 2,
one target id 9 has decoder body length 5 and count 1, not count 2. Neither id is a player
id. If the world classifier returns 3, the requested state is 10, not 3. A null actor
executes no base request; a target with flags 4 fails initial admission even though the
later live-target poll below does not itself test that bit. No original payload bytes
are stored with this example.

## 3. Smallest verified update chain and its stop boundary

**D — timing/integration.** Update `0x10049200–0x10049345` forms a zero-extended u16
`now - last(+8)`, multiplies by the f32 constant at `0x100c3270` (nominal 0.2), and stores
the remaining time as f32. Existing world timing is one tick per 2 game ms
([clock read](tick-loop.md#2-how-the-simulation-advances)); nominal one integrator unit
is 5 ticks. Zero speed (+0x40) or zero elapsed time returns without running the body.
Direction components +0x30/+0x34/+0x38, remaining +0x3c, speed +0x40 and position
+0x24/+0x28/+0x2c are f32. It clamps movement to remaining segment length, subtracts used
time, and carries surplus through further segments in the same visit. There is no proved
attack-period constant or integer adaptation in this packet.

**D — segment end order.** At `0x100492ba–0x10049325`, a final flag value 2 takes the
zero-speed/flag-4 registry notification path. Otherwise +0x18c runs first; if it returns
zero, non-null +0x50 takes +0x17c, else nonzero requested state +0x70 takes +0x178,
else +0x174 installs idle. +0x178 calls +0x1c4 then +0x1c0; a zero result clears order
bytes +0x70/+0x71/+0x72 (`0x10046430–0x10046455`). At +0x17c's entry
`0x10048df0–0x10048eac`, a kind-0 node copies its six-word segment and advances current
node +0x50. Other segment kinds and the final registry removal's lifetime effects are U.

**D — order 10 continuation.** `0x10046220` selects requested state as active state when
active +0x71 is zero (`0x10046374–0x100463af`). Its jump table sends active state 10 to
`0x100328e0` on later polls (`0x10046297–0x100462d6`). That poll resolves target id +0x78
live: null, target flag value 2, or signed HP <= 0 returns finished; otherwise continue
(`0x100328e0–0x10032933`). Getter `0x100349b0` sign-extends target word +0x12.
Successful finish clears the matching requested/active/options bytes via
`0x100462aa/0x100462af → 0x1004626c–0x10046276`; the backward branch stays within the function.

**D — planning edge, not complete impact.** Active-state 10 dispatch in
`0x10045fa0–0x100460bb` reaches `0x10032810–0x100328d0`. It checks attacker capability,
builds descriptor via world +0x118 (`0x100a8a10`), merges the route list via +0x11c
(`0x100a8a90`), and calls world +0x124 (`0x100a9ff0`) with target kind 2 and mode 3.
The selected figure-target arm at `0x100aa485–0x100aa64f` performs immediate predicate
+0x190 (`0x100a6b70` → +0x16c `0x100a65e0`) or plans a route, then commits through
`0x100a9490`. Results 3 and -2 make the order helper fail; 0/2 have distinct commit
paths. This is a descriptor/segment path, **not a proved tRouteFight or job id**.
Its geometric callbacks, exact movement-to-impact transition, damage and transitive
world draws remain U; no attack formula or complete attack execution is claimed.

**D/U — persistence and draws.** The 16-byte HEAD writer `0x100499d0–0x10049a40`
writes current HP at +8 and other stance/metadata/position-cell fields; its paired loader
`0x100498e0–0x100499cc` treats signed nonpositive HEAD HP as type maximum, then clamps.
This HEAD writer does not serialize +0x70 order or segment state. Whether another task
record saves them is U; do not infer that the whole save discards orders. No direct world
+b4 draw is present in the bounded setter/integrator/dispatch/poll bodies above. Their
transitive draw total, including planning and impact, is **unproved**, not zero.

### Decisive boundary examples

These are hand-derived projections, not a new simulation oracle. Ten private executable
integer checks distinguish the listed wrong readings; floats/callees were not emulated.

| Predicate / evidence | Boundary and expected | Wrong reading distinguished |
|---|---|---|
| actor mask `0x100a5f84` | flags 2 refuses; flags 1 does not fail this mask | testing only 0x2000 / including bit 0 |
| initial target mask `0x100a6d0d` | flags 4 refuses | testing value 2 only |
| later target poll `0x10032910–0x10032924` | flags 4 with HP 1 continues this poll; HP 0 finishes | reusing mask 6 / strict negative-only HP |
| signed HP `0x100349b0` | raw word 65535 is -1 and finishes | unsigned HP |
| elapsed `0x1004920a–0x10049216` | last 65535, now 0 gives 1; last 0, now 65536 gives 0 | signed subtraction / unlimited elapsed |
| count and state `Frame 0x1005d3cb`; `0x10048c0d` | body length 5 gives count 1; classifier 3 installs order 10 | counting header / equating classifier and order |

Zero elapsed/speed and surplus carry are verified control-flow properties only. Exact x87
rounding, zero-length cycles and long-visit behavior need the later movement contract and
an original-byte execution check before a timed implementation is released.

## 4. Remaining reads and smallest next boundary

OQ-753 remains open. This subset establishes constructors, one contextual order's submission/
receive/setter, attack-selected order 10, update dispatch and target-loss predicate. It does
not settle group/formation orders, AI, healing, theft, siege, damage/death, boarding or saves
of task continuations. HE class names are labels; **HE reading and original runtime NOT_RUN**.

Proposed next bounded reads, estimates rather than commitments:

1. **MIL-RE-2, 2–3 h:** close world +0x16c geometry, mode-3 route commit/segment callbacks and
   the first impact edge for ordinary code 99; identify or explicitly rule out tRouteFight/
   tJobFightBewohner in that chain. Include every reached world draw and timed boundary.
2. **MIL-RE-3, 2–3 h:** exact impact, HP/death and task/save continuation for that one family;
   distinguish direct orders from autonomous target acquisition. No subclass generalization.
3. **Separate 2 h reads:** UI/sender ownership and group/formation selection; medic/thief;
   artillery/tower crew; boarding; AI orders. Size from the first two results, not one army packet.

The smallest future implementation contract currently justified is **a strict decoder and
admission specification for the single-actor kind-2 request**, not executable combat. Before
production execution it needs UI/issuer entitlement, malformed-record policy, complete target
classification, movement/time adaptation, persistence and draw closure. Original u16 object
ids/u8 modes are file/protocol facts; engine ids, capacities, search workloads and type counts
must stay data/config driven. No original temporary-buffer cap is adopted as an engine limit.
