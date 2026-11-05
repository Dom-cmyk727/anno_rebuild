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
