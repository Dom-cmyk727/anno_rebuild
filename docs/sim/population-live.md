# Population live effects: the effect contract (MAP-POP-LIVE-1, L0)

Status: **bound contract** for L1-L5 (2026-11-03). Start `73e7ab1a` (stream 127, PRNG timeline 4). It sits on [`population-contract.md`](population-contract.md) (the reviewed arithmetic kernel `house_kernel`, which is *reused*, not re-implemented) and the evidence in [`docs/re/population.md`](../re/population.md) (§5, §12-§14, §16). Labels: **D** read from the original's instructions (AnnoWelt.dll sha256 `f220a878...`, AnnoGame.dll `ac721220...`), **A** approved adaptation (an existing owner/orchestrator decision), **P** proposal made here for review, **U** unread edge. Notation: `trunc` = toward zero; "8.8" = 256 units per ton.

Two research notes of this task (read-only, 2026-11-03) are folded in below; their unread edges are listed in section 9. Corrections to older text: the stall wait/purchase loop buys **at most two** slots (the first empty slot `i` and slot `i+1`, with `+0xd4` re-evaluated between them); a step-B arrival that "fails" changes **no job flow** (the job ignores the arrival result); the city's upgrade **queue is never filled** in this build, so an upgrade is **immediate** inside the House tick.

## 1. The taken House tick, in order (AnnoWelt `0x10076230`-`0x100765b9`)

| # | Step | Contract | Where implemented |
|---|---|---|---|
| 1 | Gate | unsigned u16 `t - last` against the class gate period; `last += period` (D). Skipped call changes nothing. | kernel (done) |
| 2 | `E'`, need loop, `lowest` | kernel (done) | kernel |
| 3 | **Spawn** | if `lowest < 102` **and** the house has fewer than `type_max` walkers (count of non-null house walker slots): **draw the world PRNG** (site `HouseWalkerTarget`, one draw, `0x10076397`), `k = ((r & 1) + y + x + serial) mod variants` (unsigned; `x`,`y`,`serial` are the house's u16 fields `+6`,`+8`,`+4`; `variants` = the type's figure-group-0 count, `+0xbc(0)`), figure type = `type +0xb4(0, k)`, then the base send `0x10072d20(job 0x35, figure type, t)` (D). **The draw happens before and whatever the send does**; the send refuses when house flag bits 4/5 are set or no reachable entrance exists (D `0x10072b50`, U: first-walkable-entrance rule; the engine uses the existing `spawn_position`). | L2 |
| 4 | Mood | kernel | kernel |
| 5 | Growth | `counter.sub(g, trunc(res/64))`; `res += VermehrTab[mood]`; `counter.add(g, trunc(res'/64))` (D). Engine counters are **derived** from the houses (OQ-165, accepted) except the one documented exception F-3 (below-64 correction). | L4 |
| 6a | mood >= 0, cap crossed (`res' > Wohnmax*64`, signed strict) | `counter.sub(g, trunc(res'/64) - Wohnmax)`; `res := Wohnmax*64`; **UpgradeCheck** (section 5); if it returns non-zero, city slot `+0x98` = **Upgrade now** (D `0x100764a1`-`0x100764af`). | L3 |
| 6b | mood < 0 | floor 64 (NoDowngrade or flags `0x30`) or `48 * previous Wohnmax`; below floor: floor 64 -> **remove** (island `+0xcc`, args `(x, y, 0, 0, 0)`); floor `48*prev` -> optional below-64 correction (`counter.add(g, trunc(res'/64) - 64)`, `res := 64`) then **Downgrade now** (city slot `+0x9c`) (D). | L3 |

The kernel result already carries every number in 2-6 (`lowest`, `spawn_condition`, `mood`, `counter_sub/add`, `cap_sub`, `below64_add`, `residents_64`, `arm`, `effect`). The live tick **applies** them in this order and nothing else; it never recomputes a formula.

## 2. Resident supply: facts (D, section S1-S4 of the research)

**House demand** (`+0xa4`, `0x100765c0`): `Menge * E * 256 / 44236 - stock / 16` (D, population.md §5), signed, in the same 8.8 units a delivery adds (a delivery adds `amount * 16` to the stock, so `stock/16` is in delivered units).

**House choice** (`+0xd4`, `0x10076670`), `(supplier, &goods_out) -> score`:
1. `A = supplier.type.out0`, `B = supplier.type.out1` (the type's two WARE goods).
2. **Skip rule:** over the house's walker slots, each walker's cargo slots `0..n-1`: a slot with goods `gid` and a **non-zero amount** zeroes `A` (if `gid == A`) or `B` (if `gid == B`). The calling walker is one of those walkers. (U: whether the cargo test is "amount != 0" or "goods != 0"; a purchase that returns amount 0 sets goods without amount, see the Doctor note in section 3.) **P:** use "goods != 0 and amount != 0"; the two differ only for a zero-amount purchase.
3. Per candidate good `g` (A first, then B), `slot = lookup[g]`; skip if `slot >= n`. `s = min(trunc(1536 * stock[slot] / (E * Menge[slot])), 160)`, `urg = 128 - s` (range -32..128). **IdSec chain:** `urg = max(urg, urgency of lookup[IdSec(Ware(slot))])`, recursive along the chain (helper `0x10076960`), signed. Accept `g` only if all hold: `urg >= 0`; `supplier.avail(g) > trunc(R * Menge * 256 / 4096)` with `R = trunc(res64/64)` (signed, strict); `supplier.enabled(g) != 0` (`+0xc4`). Accepted: `*goods_out = g`, `best = urg`. `B` is accepted only if its `urg` is **strictly greater** than `best` (a tie keeps `A`). Result `best`, or `-1` when nothing was accepted.
4. Callers require `score >= 0xc` (12), i.e. `s <= 116` for a good with no IdSec boost.
5. **Domain (A, OD-4):** the original has no `E == 0`/`Menge == 0` guard here (divide fault). The engine returns *not chosen* (`-1`) for a candidate whose divisor is 0 and records a `ChoiceDomain` diagnostic count; the registry already forbids `Menge == 0` (population-contract §3), and `E == 0` is reached only by crafted state. **IdSec recursion** is bounded: the chain is followed at most `goods_count` steps and a cycle returns the best urgency found so far (the original recurses unboundedly; a cycle would crash it). Registry validation reports an IdSec cycle among *needed* goods as a content error (P).

**Supplier availability** (`+0xa4(g)`, 8.8): MarketStall (PRODTYPE 161): the full city store of `g` (`city.stock[g]`, no reservation, no range check). PublicService, School, Doctor: the constant **256** for any `g`. FireBrigade: 512. Inn: its tavern stock `u16 +0x18` (g ignored). Plain/others: 0. Enabled (`+0xc4`): MarketStall flags `+0x18`/`+0x19` (default 1, per output slot; ALL `0x8e` = either flag with a non-zero output), Castle `+0x22`, all others 1. **U:** who flips the stall flags (setter `+0xc8` not read) - the engine keeps both at 1.

**Walker purchase** (step B arrival `0x10056480`, D): building at the walker's tile with city mask `0x21` (none: nothing happens, job continues). Cargo slots `n = figure type slot count`. Find the first empty slot `i`. If none, done. Slot `i`: `score = choice(bld, &g)`; if `score < 12` done (nothing more is bought). `amt = min(bld.avail(g), house.demand(g), capacity)` (all signed 32); if `amt > 0`: set the slot's goods `g`, `r = bld.bill(g, amt, third)`, set the slot's amount to **the returned `r`**, then `i += 1`. Then **second pass**: if `i < n` and slot `i` is empty, re-run the choice (the walker's own cargo now excludes `g`), same `score >= 12` gate and the same `min` and bill. At most two purchases. The third bill argument (`BGruppeNr`) is ignored by every billing class.

**Billing** (`bill`, city slot `+0x88` = `0x1002dac0`, args `(goods, amount, third, take)`, D):
```
if goods < 0x1a or goods > 0x28: return amount            # nothing billed, nothing taken
rec = city.goods_record[goods]
rec.billed += amount            (u16, the REQUESTED amount, before clipping)
if take == 0: return amount
t = min(amount, rec.store)      (unsigned)
rec.taken_stat += t (u16); rec.store -= t; return t
```
Wrappers: MarketStall `take = 1` (returns `t`); PublicService and School `take = 0` (return `amount`; School also bumps a statistic, U effect); Doctor/FireBrigade return 0; Inn takes `min(stock, amount)` from its own `+0x18`, bills its *input* good with `take = 0`, returns the taken amount. Consequence: a church/chapel/school/bath/theatre supplies `min(256, demand, capacity)` of its own infrastructure good **without consuming anything**; a stall consumes the city store.

**Billed counters and `save_billed_goods` (owner 2026-10-25, default true; A):** `billed[g]` (consumer goods `0x1a`-`0x28` only; u16 in the original, widened to u32) is accumulated per city and consumed by the tax pass, which is *outside this task*. The engine adds `City::billed` (vector sized `warehouse_goods`) and a rule `save_billed_goods` (default **true**): true = `billed` is saved, hashed and restored; false = it is *not* saved (a loaded city starts with zero billed), reproducing the original's dropped scratch. Either way the live tick writes it. Both values are tested.

**House delivery** (`+0xc0`, `0x10076610`, D): `goods -> slot = lookup[goods]`; if the goods has a slot, `stock[slot] += amount * 16`; if the goods has an `IdSec`, the same amount is delivered again for the `IdSec` good (the replay). A good the house does not need returns 0. Step C arrival (`0x10057d70`): for cargo slots `0..2` with non-zero goods, `home.deliver(goods, amount)`. Widened/checked (A): a delivery that would leave `u32` is refused whole and counted.

## 3. The resident job `0x35` lifecycle (D, population.md §14; research S3)

Figure type: `type +0xb4(0, k)` (shipped house types list ids 250, 252, 293-305, 324-329, 512; capacity `LOGIC[0x11] << 8`; layer `LOGIC[0x10]` = 0 for all of them: open ground is walkable, roads are cheaper). Steps: **A** (start `0x1005b170`): area = the house's centre square of radius `house type +0x65`; candidates = buildings of mask `0x21` (stall list + public list) in it with `choice >= 12`; walk search from the house entrance; the chosen stand is the reachable candidate with the **strictly highest** choice score, **first discovery wins ties** (path-cost order), not the nearest; no candidate/no path -> go to C. For a type-161 stall step A also advances the figure timeline by 1/3 (U: units; no sim effect assumed). **B**: walk, then the arrival above; the start wait (`+0xe0(float)`) and the stall attach calls `+0x28/+0x2c` are **U** (the engine uses a fixed one-tick wait and no stall state; this is an approved simplification only if the research gap stays open, and is listed in the limits). **C**: walk home (same as the cart's `0x10057ae0`), arrival `0x10057d70` delivers all cargo slots, then the figure ends; the house's walker slot is released.

**Ownership:** the house holds up to `type_max` walker references (the original has 4 slots at `+0x18..+0x24`); a figure holds its home (`Figure::home`, counter in the House walker slot). The standard end path releases the slot **exactly once** (the harvest lessons: finish before release; an old job never clears a newer state).

## 4. Decisions carried (A)

Exact own saves/resync (`last` saved); widened checked arithmetic; configurable roles; counts and ids are data (the original's 4 walker slots and 3 cargo slots are **defaults**: `SimConfig::house_walker_slots = 4` is *not* introduced; the House keeps `walkers: vector<FigureId>` sized by the type's maximum and the cargo slot count comes from the figure type, both validated and hashed); original import `last = 0`; `save_billed_goods = true`.

## 5. Upgrade, Downgrade, SetGroup, removal (D, research T1-T3)

**UpgradeCheck** (`0x10075dd0`): `NoUpgrade` -> 0. Scores use cap **128**, signed idiv. Goods loop: next-group needs `j = 1 .. next.goods_end-1`, **forced** only, the score taken at the **current** group's slot `lookup_cur[Ware_next(j)]` with the **current** record's `Menge` (a missing current slot is a data-invariant violation: **A** treat as *not eligible*). Infra loop: `next.goods_end .. next.infra_end-1`, forced only, `min_infra` starts 160 (scores are <= 128); a need whose ware is the tavern role also joins the goods sum/count. Food uses the **next** group's `Menge[0]` and the house's slot-0 stock. `f = Nahrung_next[(food+4)>>3]`, `mean = count ? sum/count : 128`, `b = Bedarf_next[(mean+4)>>3]`, `i = Infra_next[(min_infra+4)>>3]`; require `f > 0`, then `b > 0`, then `i > 0` (strict). Then city `+0x94`: queue count `< 10` (always true: the queue is never filled), the house kind in block `0x61..0x80` (**A:** the engine's equivalent is "the house type belongs to a house group with a next group"), and the next type (`id - id%20 + 20`) exists with the materials (goods `0x29`-`0x2d`, amounts from the next type) available: `amount <= store - spent` for each of the five goods.

**Upgrade** (`0x1002cbe0`, immediate): next type `id - id%20 + 20`; none -> nothing happens. Materials: for `k = 0..4` (goods `0x29+k`): `spent[k] += amount_k`; `store[g] -= min(amount_k, store[g])`. **Variant:** property `0x32` (count `c`, step `s`): if `c != 0`: **draw** the world PRNG, `idx = r mod c` (unsigned), new type = the type `base id + idx * s` (no materials re-check; null lookup is **U**: the engine refuses the whole upgrade, restoring nothing, if the variant type does not exist). Then in this order: `counter.sub(oldG, trunc(res64/64))`, `houses[oldG] -= 1`, detach old type, attach new type, `SetGroup(oldG + 1)`, `counter.add(newG, trunc(res64/64))` (res read **after** SetGroup), `houses[newG] += 1`. The PRNG draw is the **variant draw only** (SetGroup's own draw is first-assignment only). `UpgradeCheck`'s "one-predicate near misses": a strict `> 0` table entry, a forced need below the score threshold, a missing material, a missing next type.

**Downgrade** (`0x1002cdd0`): previous type `id - id%20 - 20`, else `id - id%20 - 19`, else nothing. Same variant draw (before SetGroup), same counter/house-count/SetGroup/type swap, no materials. **SetGroup** (`0x10076b90`): carry each old need's satisfaction `min(stock*1536/(oldMenge*E),160)` by goods id (also to the IdSec chain when `>= 102`); new stock `trunc(newMenge * s * E / 1536)` (widened); new needs get 0 except food (slot 0) `Menge*E/12`; **upgrade** keeps residents and `E`; **downgrade** gives new needs `s = 102` and sets `E = trunc(res64/64) << 7`.

**Removal** (island `+0xcc` = SetHitPoints `0x10093c10`, `hp = 0`): destroys the house through the owner city's remove-at-tile `0x10028fd0` -> building-index remove `0x100255c0`, which **subtracts** `trunc(res64/64)` from the group counter, decrements the house count, removes the building and detaches walkers (their home becomes none). Quirk (U, A): a damage record that already exists with hp 0 makes the call return silently; the engine has no damage records, so removal always proceeds. The engine reuses `remove_building`; the new parts are the counter subtraction (derived counters make it automatic) and the explicit walker detach (the standard figure end path with `home = none`).

## 6. Stage unlock (D, research T4) and counters

Stage pass `0x10029610` (city, every call, gate `u16(t - city.stage_stamp) >= 2500`, then `stamp = t`): for stages `i = 0..30` not set in the city mask: if `residents_at_or_above(stage.group) >= stage.count`, set bit `i`. If `(player.mask & 0x7fffff) >= city.mask` (unsigned) **skip** the OR; otherwise OR the city mask into the player mask (quirk kept, A: it is the original's compare, not an idempotent OR). Announcements are UI only. `[BAUSTUFEN]` (31 records of `{group u8, wissen u8, count u16}`) is **not parsed yet**: package L4 adds the parser (registry data, digest) and the pass. The player-mask extra OR at `0x1001bda3` (OQ-460) stays **U** and out of scope.

**Counters.** Resident counters are derived: `residents_at_or_above(g) = sum over houses of groups >= g of trunc(res64/64)`; maintained by recomputation per query, **A:** the one divergence from the original's incremental counter is the below-64 correction, where the original adds `trunc(res/64) - 64` *after* having added `trunc(res/64)` (F-3). The engine's derived counter reads `trunc(64/64) = 1` for the corrected house; the divergence is recorded, not replicated, and a test pins the original value in the kernel output (`below64_add`).

## 7. NotEvaluated in a live tick (A / P)

| Reason | Reachable how | Live treatment |
|---|---|---|
| `GatePeriodOutOfDomain` | class data | **world creation refuses** a registry whose House class has a gate period outside 1..65535 when house updates are on |
| `MissingGroup`, `StockSizeMismatch`, `StageIndexOutOfDomain` | only an invalid state/registry | already refused by validators (`check_invariants`, registry build); an unreachable branch in a validated world; if hit anyway the tick **aborts the whole advance with an error** (never a silent skip) |
| `MissingFirstNeed` | a registry group with no need | **world creation refuses** a house group with no need when house updates are on (content error naming the group) |
| `NegativeSmoothedE`, `ZeroDivisor`, `MoodOutOfRange`, `Overflow`, `PreviousCapacityUnavailable` | crafted/modded state or tables | **not skipped, not clamped:** the tick returns a typed `HouseFault` through `Effects`, `check_invariants` reports it and `advance` fails with that message. This is a decided treatment under OD-4 (no guessed clamp, wrap or native fault): a crafted state *fails loudly*; it is never skipped. A *quarantine* alternative would be new policy and is not implemented. |
| `PreviousCapacityUnavailable` for group 0 without NoDowngrade | modded table | same fault; a registry whose group 0 lacks NoDowngrade is refused at creation (the original reads layout-adjacent bytes, OD-5) |

## 8. Activation, identity and rules

* `Rules::house_updates` (default **false**, hashed): off = House dispatch stays `return;` exactly as accepted; on = the full path of sections 1-6 (never a reduced numeric tick). Default false keeps every accepted golden/replay/corpus baseline; flipping the default is a separate orchestrator decision. **P.**
* `Rules::save_billed_goods` (default true).
* One combined state increment **128** (`City::billed`, House walkers, resident job state, city stage mask/stamp, two Rules booleans) and one PRNG timeline increment **5** (new draw sites `HouseWalkerTarget`, `HouseVariant`, `HouseFirstAssignment`; they draw only with `house_updates` on). Command wire unchanged.
* **Create timestamp (OD-2):** the scenario/savegame ENTRY path passes the constant 0 (D, population.md §16). The player-build path forwards a caller argument (U). **P:** a player-built House stamps `last = world tick (u16)` - the same as the engine's other Create stamps (`stamp_placement`) - and this is recorded as an adaptation, not an original fact.

## 9. Unread edges (disclosed, none blocks the component work)

Cargo-class vtable `+8` (amount vs goods test); the stall wait `+0xe0(float)` and the stall attach `+0x28/+0x2c`; `0x10030090` timeline units; stall flag setter `+0xc8`; School's statistic; IdSec cycles in the shipped file (the engine validates); who passes the Create timestamp on the human build path (View.dll sites); OQ-460; `player+0x96/+0x97` in SetProperty `0x21`; the city-event/UI callee `0x1002c790`; property `0x32` values of the shipped house types; null variant lookup; E == 0 / Menge == 0 faults in `UpgradeCheck`/`SetGroup`; damage-record quirk of `SetHitPoints`.

## 10. Distinguishing cases (hand-derived; each row names the wrong reading it kills)

| Id | Rule | Wrong reading | Input -> expected |
|---|---|---|---|
| X1 | choice threshold | `stock > residents * Menge` | R = 10, Menge = 41: thr = trunc(10*41*256/4096) = 25: supplier 25 rejected, 26 accepted |
| X2 | tie order | B wins ties | A, B both urg 100: A |
| X3 | skip rule | goods "being fetched" in the abstract | a walker carrying A with amount > 0: A zeroed, B considered |
| X4 | two purchases at most | three slots | empty cargo [x,x,x], stall offers 3 goods: exactly 2 slots filled |
| X5 | returned amount | cargo = requested amount | a bill that clips: cargo = returned `t` |
| X6 | billing range | bill every goods | goods 0x4c: returns amount, billed unchanged |
| X7 | billed requested amount | billed = taken | store 100, amount 300, take: billed += 300, store 0, returns 100 |
| X8 | public supply | consumes the store | KAPELLE offers min(256, demand, capacity), store/billed untouched |
| X9 | upgrade type | `id + 20` | id 45 -> 60; downgrade 45 -> 20 (else 21) |
| X10 | variant | `r & 3` | count 3, step 2, base 60, r = 7: 7 mod 3 = 1 -> type 62 |
| X11 | counters | add before SetGroup | sub(oldG, 8), SetGroup, add(newG, res after) for res64 = 522 |
| X12 | UpgradeCheck Menge | use next Menge for goods | E = 640, stock0 = 4167, next Menge0 = 100, cur Menge0 = 80: food 100 -> index 13 (current Menge would give 125 -> 16) |
| X13 | strict `> 0` | `>= 0` | table entry 0 blocks the upgrade |
| X14 | draw before send | draw only on success | a refused send (flag bit 4) still consumes exactly one draw |
| X15 | gate | consumes time | t = 5000, last = 0: one tick per call (kernel D1b), spawn once per taken call |
| X16 | unlock compare | idempotent OR | player low23 = 0x10, city mask 0x08: OR skipped (0x10 >= 0x08); low23 = 0x04 with city 0x08: OR performed |

## 11. Limits

Taxes, upkeep booking, fire/plague, military and win/loss are outside this task; billed counters are accumulated but not consumed. No UI. Windows/MSVC, Linux/WSL, original-runtime, GPU and multiplayer scale remain NOT_RUN. The stall wait and stall attach state are simplified (section 3) until their edges are read.
