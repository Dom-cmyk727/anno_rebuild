# Automatic owned-warehouse freight: original evidence (MAP-ROUTE-1 R1)

Static primary-byte reading, 2026-11-06. No original-runtime observation or route
save import is claimed. Original-derived bytes/listings remain ignored under
`cache/route1/R1`; only findings and our synthetic examples are published.
Tools: read-only PE ranges/vtables with pefile2024.8.26 and Capstone5.0.7, matching
the existing cargo research method. Independent reviewer: expand_e2_review.
The contract/reference/schema still require complete R1 approval before C++.

## Inputs and function boundaries

| Module | SHA256 |
|---|---|
| AnnoWelt.dll | f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2 |
| AnnoGame.dll | ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc |
| AnnoFrame.dll | dd3f36ff564d2685d10795272c4ed3c0b2da32a946c2883fb1158ff439249b91 |
| View.dll | e32fa8a5ff0f3f0b00ef05618daf8309c692daaa57316147eb6cb1f3637733a1 |
| FigurenHeader.dat | 29fa1f0528fb337d9a61e8b73169281fd05bb27efa6df65638580a4c36c468a8 |

The transfer node vtable is AnnoWelt `0x100c6ce0`. Its slot0 returns kind10
(`0x10056320`); +0xc is preparation `0x10055650..0x1005581c`; +0x10 is execution
`0x10055bb0..0x10055f27`; +0x18/+0x1c are property setter/getter55f30/55fb0.
Factory `0x1005cfd0` kind10 allocates16bytes and calls constructor561d0, which
installs this vtable and initializes byte+0xf to1. Do not mistake the first early
RET for the function boundary: 55a70 and55bb0 contain later reachable arms.
Jump tables are data, not instructions of adjacent functions.

Node fields independently confirmed by property jump tables:

| Field | Primary operation | Meaning / distinguishing example |
|---|---|---|
| +4/+6 u16 | property1 copies packed coordinates;55bd3/55bcd load words | stop tile x/y, not ship/dock coordinates |
| +0xa u16 |55f60 setter shr8;55fd6 getter shl8 | stored tons, property4 exposes raw1/256t |
| +0xc u8 |55f4c/55fce | goods, property0 |
| +0xe u8 |55f6e/55fea | direction, property5;0 load,nonzero unload at55c68 |
| +0xf u8 |55f78/55ff2 | grouping marker, property9; not a wait-until-full flag |

## Grouping, hold order and quantities

`0x10055820..0x10055a60` obtains the ship's route container through ship+a4(0),
finds the current linked entry, walks backward through kind10 entries and then
finds the FIRST matching `(x,y,goods,direction)` in that kind10 block. The current
entry must be that first match, otherwise it returns0. It marks current+0xf=0,
later matching entries+0xf=1 and sums property4 across ALL matches in the block.
Intervening different goods/direction do not split the block. This is not merely
adjacent matching-action coalescing. The block ends at another node kind.

`0x10055a70..0x10055ba9` scans the first hold_count_a holds in increasing index.
It sums amounts of ALL matching holds, including full holds. Candidates are
matching nonfull holds then empty holds for loading; matching holds, including
full ones, for unloading. Each category preserves increasing index. Empty holds
are appended only for loading. It does not sort by free room.

`0x10055bb0` resolves the city at the node's stop tile: world+8c(x,y), island+ec
owner index and island+f8 city. It contains no live distance/warehouse-identity
check. Original foreign trade is a separate shared-world arm and is excluded.
Owner property0xf takes a conditional notification/return0 path. Missing island
and city safety is not established:55be3/55c74 dereference their returned pointers
without null guards. The modern contract instead checks stable IDs, authority
and range, and stops recoverably.

Loading subtracts the total already aboard from the grouped requested raw amount
(`55cd8`). Thus load quantity is a desired TOTAL aboard, not an additional amount.
Unloading uses the grouped amount directly (`55e0a..55e47`), not a desired amount
left aboard. For each candidate, it forms `min(remaining, free-in-hold)` when
loading (`55d18..55d31`), or `min(remaining, held)` when unloading (`55e75..55e8a`).
It then divides toward zero by256 (`55d64..55d72`, `55ebd..55ecb`) and writes the
whole-ton request into the shared record. World+100 is the established shared
trade function `0x100ac360`, verified at vtable100ccdd8; calls are55d8a/55ee3.
The actual returned amount reduces remaining. A zero result ends this execution;
it does not try a later hold. Safe stock/hold arithmetic adaptations remain those
of the accepted cargo contract, not the original's loss/wrap defects.

Hand-derived discriminators before reference/C++:

- Aboard1t, Load2t means add1t; an additive interpretation adds2t incorrectly.
- Aboard1t; Loadwood2,Unloadwood1,Loadwood3 in one stop/block: first load targets5t,
  unload removes1t, later duplicate load skips; final4t. Literal sequential
  desired targets would produce3t. There are still THREE preparation waits.
- Need512raw and first matching hold room300raw: caller requests1t, not2t; it
  cannot directly transfer300raw in that first call.
- First matching room128raw: caller requests0t and stops the group on zero shared
  result, even if a later empty hold exists. Likewise first held128raw in unload.
- Store448raw and request2t: accepted load arithmetic floors stock to256raw,
  transfers256, retains192. A positive partial result is not an automatic retry
  until full. Per-call hold redirection remains the shared cargo resolver's rule.

## Controller, preparation delay, arrival and interruption

Ship+0x9c is its route container: getter40ac0 returns that subobject for index0;
constructor4361c installs vtable100c4f4c. Container+30 is5b6c0 (prepare current
node+0xc), +34 is5d1f0 (execute current node+0x10, then advance), +38 is5b740
(current node+14), +4c is5d1a0 (next/wrap for kinds8/10/12). At5d204 the node
execution result is ignored; current.next replaces current, null wraps to first
at5d214. Therefore zero/partial cargo does NOT request waiting for stock/fullness.
5b6c0 recursively skips a preparation failure. Modern bounded continuation and
recoverable invalid-target behavior replace recursion/unsafe retargeting.

The ship order3 path is424d0→408e0→container+34, then423a0 phase3→408b0→+30.
`55650` checks stop geometry and installs a zero-velocity delay before transfer.
Its tail pushes `(remaining=type+64(1),mode0,u16now,pointer0)` to ship+e0=40000;
the word is NOW, not a computed direction.40000 stores remaining+2c, velocityXYZ0,
kind0, speed bits3c23d70a(0.01f), and now+70. This is not HEAD turn-step0.11.
EVERY successful kind10 preparation waits, including later quantity duplicates.

AnnoGame type vtable10021d10+64→1000fd10 reads `[type+2c+4*index]`. Index1 is+30;
loader101f4..10211 takes positive LOGIC[4], else1.0. Original1502 first8 LOGIC
bytes are `0000c0400000803f`: the delay is1.0. All12 original SHIP records have
the same1.0 value. The nominal delay is500ticks at rate0.01 and time scale0.2.
With the documented x87≥53-bit product model, a single500tick call leaves about
2.235e-8 positive residue. A24-bit product may round differently; OQ195's DDraw
control-word effects remain unobserved. Existing FigureType.load_ticks imports
round(500*positive_LOGIC4_or1); reuse500 as the declared integer adaptation.

Original428d0 carries movement and delay surplus across segment boundaries in
the same visit. Its delay starts from the actual sub-visit boundary. Modern
ShipArrived is observed at a visit boundary; a route clock boundary must be
specified explicitly, not claimed as original fractional-time parity.

The route move node kind8 (vtable100c7178) prepares at56870, configuring point
goal/radius6, callback54270 and ship+cc=3fad0. It precedes action nodes. Its
execution+10=5f710 is a no-op. The accepted modern Move navigator replaces that
original automatic movement interface, with order-identified actual arrival
and live cargo-range validation; original route-editor dock/facing selection
is not imported. No original-runtime/collision/float parity is inferred.

Manual stop43350 calls ship+168(1)=42630. That clears order phases and nodes;
order3 specifically calls40920, which releases the active route container pointer
atship+68 while retaining the definition atship+9c. The modern contract's explicit
cancel/reassign/revision and authenticated control rules are adaptations. Manual
cargo's shared function does not itself cancel a route. Modern interaction policy
must be stated and tested rather than attributed to that original function.

## Scope of unresolved original arms

Foreign trade/pricing, other node kinds, original editor packet creation/import,
notification presentation, automatic dock/facing selection and exact f32 delays
are excluded. These findings establish the owned-stop transfer, grouping, nominal
delay and progress rules only. Modern stable references, deferred Move boundary,
work quotas and control/query/persistence are explicit contract decisions. R1
approval must review those decisions and the independent vectors/schema; this
research note alone releases no runtime implementation.
