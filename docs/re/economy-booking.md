# ECON-BOOK-1: booking inputs and accounting ownership

Research checkpoint, 2026-11-05. Static reads only, no original runtime execution.
This extends the reviewed arithmetic oracle; it is NOT an engine-ready contract.
Original modules are read-only from the main checkout's game/app. Raw bounded
range dumps are in ignored `cache/economy-booking/B/`. Some exploratory dumps
start before a known instruction boundary or continue into a jump table; only
explicit instruction ranges and decoded table entries below are evidence.

## Identity and resolved dispatch

AnnoWelt SHA-256: f220a878510f740c4cfb7224ade0a260e9a0fd7e99113e1201e7a37c0dfa6ae2.
AnnoGame SHA-256: ac721220afd27ebca5fd2ece6a8ff452289b9d7458c4e091c68adea2012db7bc.
AnnoScene SHA-256: 5b521f9541332846d32b57c93eaaee9c9d8de2ac2404cd75da0fe93e0e36fd38.
The player vtable at 0x100c2238 has +0x60 = 0x1001cfc0, +0x64 =
0x1001cfd0 and +0x68 = 0x1001cfe0. Directly read table words close the
previous virtual-dispatch gap: booking adds and subtracts the player's +0x74
money through those two ordinary dword arithmetic routines.

World property 10: getter 0x100a3380 has jump-table entry 10 at
0x100a345c+40 pointing to 0x100a3419; it zero-extends byte +0x69cf.
Setter 0x100a3230's entry 10 points to 0x100a32d6 and stores the argument's
low byte. **The setter does not validate 0..2.** The three price-level columns
are a data layout and the UI's modulo-three behavior is prior evidence, not a
validation guarantee. Modern load/config validation must reject an out-of-range
column rather than reproduce an out-of-record read.

Island property 7: island vtable 0x100cac2c, slot +0xc4 = 0x10087c30.
The switch subtracts 7; its first table entry at 0x10087c98 points to
0x10087c4c, which zero-extends byte +0xc0. AnnoScene 0x10008532..45 sends
the loaded link value to island setter +0xbc(7, value). The source field's
complete decode still needs a direct cross-check against the INSEL link parser
before claiming all original load paths' climate mapping. Existing documentation
labels that field climate; the getter alone does not prove its semantic name.

## Price loading, as read from AnnoGame

The [WAREN] branch clears 0x270 dwords (96 records of 26 bytes),
0x10014c2d..3b. For a selected good:

- `Preis` string 0x10026230: 0x10014e4d..88 parses an integer, takes its low
  word, and fills seven words at record +4 through +0x10.
- `PreisLevel` string 0x10026224: 0x10014eca..0x10014f15 parses a list and
  writes its low words successively from record +0x12. The loop is bounded by
  parsed list length, **not by three**; malformed input can overwrite later
  fields. A bounded modern reader must reject extra columns explicitly.
- `Bonus` / `Malus` strings 0x10026214 / 0x1002620c: both enter
  0x100151e5; the Malus arm negates the parsed amount at 0x1001523a..40.
  Climate comparisons set an index 0..6; 0x10015417..20 adds the amount's
  low word to record +4+2*index. `|` at 0x1001543b..3e repeats the climate.
  An unknown name leaves index -1, so the original corrupts earlier fields.
- The climate name table 0x10024ca0 reads, in order: POLAR, TUNDRA, NORD,
  PRAERIE, DSCHUNGEL, STEPPE, WUESTE. These are format defaults, not engine
  array limits.

Order matters: Preis 45; Bonus 5 STEPPE; Bonus 2 STEPPE gives 52 for STEPPE
and 45 elsewhere. A later Preis 40 resets every climate to 40. A repeated
PreisLevel list overwrites supplied positions; unprovided positions retain
previous values. Booking reads the stored words **unsigned**, even though the
text parser accepts signed numbers. The researched original-wrap model and a
modern validated wider model must remain distinct.

## City and player upkeep are different accounting owners

City switch helper 0x10072a60..0x10072b44 does nothing when old and new state
are equal. Otherwise it obtains the building's statistic kind via +0x90,
subtracts `type.+0xcc(2, old)` from its city through +0x74, then adds
`type.+0xcc(2, new)` through +0x70. City vtable 0x100c2f40 maps those slots
to 0x1002de40 and 0x1002dd40. Their statistic-1/2 arms modify ONLY city
+0x604/+0x608, with no player call. City booking divides their combined value
by 15, without a carried remainder, then debits the owner.

Player upkeep 0x1001b840 uses different fields: +0x88/+0x8c and the word
remainder +0xa2. It charges their combined value divided by 15, **retains the
remainder**, and sets +0xa4 to the current tick. This is already implemented
by `src/sim/src/player.cpp::book_upkeep`.

Player registration slot +0x8c = 0x1001c5e0 distinguishes list kinds at
0x1001c63d..655. Kind 2 takes the city-registration arm 0x1001c65b..728,
which does not add city running costs to the player's +0x88/+0x8c. Kinds 0
and 1 take paths that obtain the registered object's type via +0x78 and call
its cost getter +0x94(2,0), then add to +0x88 (0x1001c89b..c0) or +0x8c
(0x1001c7de..803). The surrounding position/owner calls support these as
figure-related lists; exact labels (ships, troops, etc.) are not needed or
claimed here. Removal contains a matching +0x88 subtraction at
0x1001cd7f..8d; all removal branches are not yet traced.

**Integration invariant:** never copy city building costs into Player::upkeep_a
or upkeep_b. A test with city costs 28 and player cost 14, remainder 1 must
charge city 1 and player 1, not a second city charge. A second visit with the
same tick must charge neither. Fifteen city visits of cost 14 charge zero,
whereas the player path carries fractions; do not accidentally share the latter
algorithm with city booking.

## Scheduling and current engine seams

City vtable +0x50 = 0x1002d9a0. Direct calls show this sequence:

1. buildings (0x1002d9af -> 0x10023a50);
2. sub-object (0x1002d9bc -> 0x10027400);
3. stage unlock (0x1002d9c4 -> 0x10029610);
4. queued upgrades (0x1002d9cc -> 0x1002d350);
5. booking (0x1002d9d4 -> 0x10029950);
6. store statistics, city events, mood statistics, final pass; stamp written last.

At the research base ec0e832e the engine already persists City::billed as a
u32 vector, gates house updates separately, applies MoneyDelta through the
scheduler and runs stage_pass after building scheduling. `Seams::city_pass`
contains the placeholder economy pass, not a working booking implementation.
Goods metadata currently imports BauFirst/IdSec, not prices/bonuses. Island
climate exists. WorldSettings::prop10 already holds and hashes the tax index,
validates it as 0..2 (`config.cpp`), and imports scene HEAD[0xb]
(`scenario_formats.cpp`). BuildingType::running_cost already parses PROD3
[0x00]/[0x02] and participates in the registry digest (`types.cpp`). Reuse these
existing facts; do not add duplicate tax or running-cost fields.

Original city HEAD2 saves displayed/smoothed sale and trade statistics; original
billed counters are scratch. Existing `annowelt.md` section on HEAD2 and
`population.md` section 11 document those readers. Their exact save/init branches
were NOT re-traced in this checkpoint. Modern saved bills remain an already
accepted adaptation, not a reason to restore the original's loss of pending income.

## Remaining work before engine authorization

1. Trace all upkeep lifecycle edges: base construction, class load, creation,
   upgrade replacement, removal and ownership transfer. The switch helper is
   verified, but its caller coverage is not complete. Resolve the type +0xcc
   getter against PROD3 and record classes with different cost/stat-kind rules.
2. Verify the climate source from the scenario/link reader and default/import
   tax values directly. Current bytes establish getter storage and setter use,
   not every import path. Record invalid input behavior as an adaptation.
3. Decide modern widened price/billing/stat arithmetic and typed overflow policy.
   Do not silently import the oracle's original dword wrapping into the engine.
4. Specify effect ordering when city booking is integrated with live house jobs,
   queued upgrades and the existing player tick; preserve current deterministic
   island-order merges and player-view restrictions.
5. Identify imported booking stamp/statistics and verify modern load continuity;
   choose a NEW stream identity after the now-published MAP stream 128. Research
   remains on ec0e832e; no unreviewed K2/K3 code was copied into this checkout.

These are available static or design tasks, not external blockers. No runtime,
C++ build, sanitizer, renderer or performance result is claimed by this note.

## Executor B1a — inputs, billing and import follow-up (2026-11-06)

Bounded original reads, same three module hashes above. The task-local decoder
is cache/econ-book1/re-tools (Capstone5.0.6/pefile2024.8.26); raw ranges are in
cache/econ-book1/B1/raw. Exploratory ranges with misaligned prefixes are retained
as *exploratory*, never authority for those bytes. Aligned/basic-block addresses
below were checked separately. These facts do not authorize engine work until
the B1/B2 reviews and policy contract are complete.

**Climate/tax source cross-check.** AnnoScene's inline table at1003b328 contains
seven FOUR-BYTE strings, not seven pointers:706f5f00(po_),745f0000(t_),6e5f0000(n_),
70725f00(pr_),645f0000(d_),735f0000(s_),775f0000(w_). Its selection loop
10009ec6..10009f73 initializes climate0 and tests the last path component in
that order; no matching prefix retains original0. That is a decoded valid
original default, not an unknown numeric climate. The chosen link copy starts
at stack+74 (100084c2..100084d0); its climate at+0xc is read at stack+80,
maskedffff and passed to island setter(property7) at10008532..10008545. The
current parser's link_climate order and resolve_links assignment match. Ordinary
INSEL5 uses INFO[0] (scenario_formats.cpp1121..1140); the original island INFO
load path is a remaining direct-byte cross-check before final B1a approval.
Tax property10's original byte getter/setter and scene header read10009dd1..e6
agree with current HEAD[0xb] import and configured validation0..2. Fresh settings
use the existing default; no new tax field or command is needed.

**Prices.** AnnoGame10014e4d..88 fills every climate word on Preis;10014eca..
10014f15 overwrites exactly the supplied PreisLevel prefix;100151e5..1001543e
handles Bonus/Malus and repeated climate names, preserving directive order.
The previous checkpoint's climate names/order and these basic blocks were
re-read. The shipped Data/BGruppen.dat has48 Preis,11 PreisLevel and11 Bonus
records, with no Malus; e.g. NAHRUNG45 plus5 in STEPPE/PRAERIE/DSCHUNGEL,
ALKOHOL50 plus5 there and tax surcharges0/5/10. Two GEWUERZE bonus directives
accumulate in repeated climates; a later Preis resets climate prices but does
not clear already written tax columns. An empty PreisLevel list is an original no-op (10014eee branches out without
writes). Excess columns at the default three-column configuration and unknown
climate names require explicit modern refusal rather than out-of-record/index-
minus-one writes. A deliberately larger modern configured table accepts its
own longer bounded list. A nonempty partial source
list is legal and retains previous/default values in unmentioned columns.

**Bills (new direct reads).** City1002dac0..1002daff checks inclusive goods1a..28,
adds the requested low word at+4 *before* testing take and clipping the actual
stock debit. Out-of-range goods returns request unchanged and adds no bill.
PublicService1006b520..540/School10070120..154 pass take0; MarketStall100702f0..
30c passes take1 (their actual vtable slots identify the caller). Inn100756e0..
10075730 first clips request to tavern u16 stock, debits that stock and books
its type's input0 through the same city call with take0. A return/delivery does
not call that bill again. With request300 and stock50, stall bills300/delivers50;
service bills300 without stock debit; Inn bills50 on input0/delivers50. At price50
with no other bills, rounded booking income is59 versus10; choosing delivered
stall quantity would therefore fail a nonempty distinguishing example.

The accepted engine widens original wrapping u16 bills to u32. resident.cpp::bill
refuses U32_MAX-5+10 transactionally; scheduler.cpp BilledDelta merge instead
saturates that same sum to MAX and records actual credited5. Both are existing
accepted semantics, not a license for this package to silently unify/change them.
Consumer selection remains configured first..last; extra services can be billed
only if the configured role/domain includes their goods. No second tax/credit is
added at resident return, home delivery or booking preparation.

**Imported statistics and stamps (new direct reads).** City constructor
1002fa46..1002fa5e zeroes u16 stamps+8,+a,+10,+12,+14,+e,+c. Booking reads+a at
10029959 and stamps current low word at10029a96..9d; it must not reuse+8 or+10.
HEAD2 reader1002e40d..1002e4a4 takes saved dwords[28]/[38]/[3c] to sales/trade
statistics+60c/+5fc/+600 using low-dword multiply105 followed by unsigned /15
(magic88888889,high>>3). In the nonoverflow domain this equals saved*7. It is
not universally equivalent at original dword overflow; that earlier shorthand
is corrected here. Modern checked/widened import policy must be explicit in B1c.
Original saves omit pending bills and stamps; original fresh/import initializes
those to0. Modern native saved bills remain the accepted adaptation and cannot
be lost when booking is enabled. Exact HEAD versus HEAD2 and save branches will
be included in the remaining B1 lifecycle/import cross-check, not inferred from
this partial range alone.

### B1a remaining load/source cross-check

Original INSEL INFO branch100897a6..10089802 recognizes INFO, clears its12-byte
scratch at stack2c/30/34, reads the payload, takes its first byte at100897e4 and
stores island+c0 at100897ee. This closes the direct INFO[0] cross-check; prefix
selection governs links, ordinary stored islands use INFO[0] itself. Original
world setter accepts any byte; modern tax validation0..2 is a stronger boundary,
not an original validation guarantee. Existing scenario parser stores those same
source fields, and validates numeric climate against configured tables.

The complete city Load1002e170..1002ebaf HEAD branch stores only sales+60c from
saved[28] (1002e30e..1002e352); HEAD2 also restores trade sales/purchases from38/3c.
Neither restores the booking stamp+a or pending bill words (original scratch).
Cost1/2 are not restored from these headers: building creation/load reconstructs
them. The exact modern policy must preserve all authoritative native booking
state instead of reapplying original quantization or dropping pending income.

### Executor B1b primary lifecycle facts (contract review input)

Type cost getter is AnnoGame1000a5a0: selector2 at1000a5b4..c4 zero-extends the
word[type+21c+2*state]. Existing running_cost[0/1] matches the source PROD3 words.
Creation1002518b..100251f1 asks the building property slot+c4(8e) for its active
selector, then calls type+cc(2,selector), building+90 for statistic kind, and adds
to the city+70. Removal100255e4..10025645 repeats that selector/getter sequence
and subtracts city+74 before destruction. Repeated remove must not reach this
subtraction after the slot is already gone.

The raw26-vtable audit (cache/econ-book1/B1/stat-getters.json) covers the named
production, service, market, house, plain and military classes. All inspected
building+90 entries resolve10025400, `mov eax,1; ret`. Do not infer kind2 merely
from a military production/list category. Kind2 is an existing separate city
statistics bucket, not evidence that each military class actually returns2.

Manual switches: Workshop100684c0..d9 uses its old switched_on byte and requested
new byte. Base Dual10069050..10069130 and SmallFarm1006fbd0..1006fc77 compute old
OR(line switches) and new OR(changed line,other line), normalized0/1, before
calling10072a60. Its equal-old/new guard returns before accounting; otherwise
it subtracts old cost then adds new cost to the building's statistic kind. A
second line switch that keeps OR=1 therefore changes no running-cost total.
The13 direct callers of10072a60 are retained in direct-call-xrefs.json; production
blocked-flag notifications are distinct from this manual selector. Current
SetProductionLineCmd supports Workshop/Farm/Mine/WeaponsWorkshop/SmallFarm;
current MarketStall switches are not modelled (SupplierModel flags default-on),
so no new stall switch command is implied. Stall's original property8e at
10070310 returns whether an enabled line has a nonzero output good. Base/service/
house property8e is100253f0, constant1; inactive merely because no resident stock
or output is available must not be invented for those classes.

**House replacement needs a distinct rule, not an assumed helper call.** Upgrade
1002cbe0..1002cdbd and downgrade1002cdd0..1002cf52 detach/attach the type and adjust
residents/counts. They call1002c790 (redraw/placement function, ends1002c82d), not
the adjacent general replacement function1002c830. That latter function *does*
subtract old cost and add new cost at1002c881..1002c8f5. House attachment75b10/
75b70 forwards type/city cases to732d0/73340; those only replace/clear pointers
and type flags. SetGroup76b90 has no city cost call. Therefore the inspected
house upgrade/downgrade path does not itself replace city running-cost totals.
This remains under independent primary review before a modern contract uses it.

A direct Anlagen inventory found81 PIONIER/SIEDLER/BUERGER/KAUFLEUTE/ARISTOKRATEN
entries;80 have costs0/0, one (3979 PIONIER)10/20. Hash remains
b2317a5c24b8c9fe796cf40f3fedaefcfa30ad3f185df9e69b2dca6a03d8dd75. Do not
incorrectly claim every original house has zero costs or silently repair a
historical contribution when a modded replacement changes costs. Native cost
reconstruction versus authoritative history must be decided with this actual
path, not a scan-of-current-types assumption. No engine implementation yet.


### B1b review correction: weapons and research history

WeaponsWorkshop uses vtable100c2d1c, not base Dual100c8150. Its property8e is
10069a90..10069bfa: active iff at least one switched-on output has research id0
or the owner's research state has bit2. Setter10069c00..10069dee computes old/new
states with this filter; disabling the sole eligible line changes active1->0
even when the other (locked) line remains switched on. Enabling a locked line
branches to69de7 before switch/cost mutation. Current engine command acceptance
is broader (it permits that flag); preserve that existing boundary rather than
silently tightening default-off semantics. Filter the new accounting selector.
Raw vtable audit labels were corrected to DualWorkshop(c8150) and
WeaponsWorkshop(c2d1c), and FurTrapper is explicit rather than misnamed Hunter.

Research writes do not repair accounting. Player ctor10021600 initializes
embedded research+b0 via10019190, vtable100c20a0. Its slot+c is10019170: store
low byte at research+86+id, ret8, no notification. The availability setter stage31
path1001bd16..1001bd9d ORs research bit2 through that slot, including optional
output0 grant, without a city/cost scan. Current engine runtime writes are in
permissions.cpp::set_construction_permission, reached by scripts; player progress
has no separate implemented completion body. A newly unlocked line can therefore
change the selector but leave old cost totals, just like house replacement.
Native continuation must retain that historical total, not recompute it. Checked
removal/disable underflow is a typed safeguard, not permission to rescan/repair.

### B1 financial-only inert selector addendum

Original getters/loads for financial activation, independently decoded again by
`/root/econ_b1_primary_review`: Hunter vtablec82a0+c4=6a5f0 ORs bytes28/29;
ctor6a9c0 writes0101 at6a9ec. Load6a700 reads a zero-filled14-byte own-length
record and sends byte1 bits0/1 to its switches. Base Dual(c8150) constructor693c0
writes0101 at693fd and load69140 uses the same financial mask. Those original
own-length writes are unsafe above14, so no trusted modern selector is invented
for that domain. Castle(c8c0c) uses Farm loader6ef70 exactly10/byte1&1 and
constructor6f1b0 inherits the enabled default. Whaler(c9418) ctor746a0 explicitly
sets byte22=1; load741a0 version1 reads a10-byte inner prefix after the version,
whose switch is innerbyte1 (outerbyte2). Only a complete>=11-byte version1 prefix
is accepted for that minimal modern fact; non1 retains the original default.

Stall(c8fe0) ctor70730 writes0101 at70741. Getter70310 requires enabled line and
nonzero type output. Load70490 accepts length exactly1 (not version1), using
byte0 bits0/1; other lengths leave flags alone. A type with both outputs UNUSED
therefore has constructor aggregate0, not an unconditional active default.
These reads authorize finance metadata only, not the unread production bodies.
