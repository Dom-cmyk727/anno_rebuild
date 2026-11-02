# Ship cargo: unload a ship's hold into the owner's warehouse (contract)

**Status:** MAP-CARGO-1 U1, 2026-11-02. A **specification and an independent reference, before any engine code**. Evidence:
[`docs/re/ship-cargo.md`](../re/ship-cargo.md) (every address below is explained there). Independent reference and vectors:
`tools/cargo_ref.py`, `fixtures/synthetic/sim/cargo_vectors.txt`. Labels: **ORIGINAL** a traced rule kept; **ADAPTATION** a deliberate
change of the rebuild; **UNKNOWN** unread, with a bounded blocker in `docs/open-questions.md` (OQ-CARGO-*).
Scope (owner, 2026-11-02): the manual transfer ship hold -> **own** warehouse/city storage through the headless command path.
Excluded: loading city goods aboard, automatic routes, buying/selling, foreign or allied warehouses, cannons, crew, combat, UI,
network transport. Shared visibility is information, never permission. U2 implements this; U3/U4 test it.

## 1. The original action in four lines

The "Unload goods" click of the ship view's "Load/unload ship" tab calls `tNetClient` slot `+0x60`, which calls world slot `+0x100`
(`AnnoWelt.dll 0x100ac360`) with a record `{goods, hold index, tons}`, the ship, flag 1 and the **city of the nearest own warehouse**.
The function takes `min(request, hold amount, free space rounded up to a ton)` from that hold and adds it to the city stock; the
hold loses its goods id when it empties. The view chose the city with `0x1003f200` (a warehouse inside a radius-5 digital disc
around the ship's tile). Nothing at execution repeats that range test, checks the sender, or looks at the ship's state.

## 2. Predicate table

| # | Predicate | Original | Rebuild | Class |
|---|---|---|---|---|
| P1 | Issuer | none in the world function; MP unauthenticated | **host-stamped issuer** (`Command::issuer`); the command carries no payer, owner, city or credit | ADAPTATION (F9 of state.md) |
| P2 | Eligible ship | any figure with holds; the view offers it for type group `0x71` only | figure exists (`UnknownFigure`), its type is the ship class (`NotAShip`, the `FoundCmd` test), `ship.owner == issuer` (`NotShipOwner`) | original class test + ADAPTATION owner |
| P3 | Ship state (moving, turning, hit points, state word, carried) | **no test** in `0x100ac360` or `0x1003f200`; the view's enabling of the tab is UNKNOWN (OQ-CARGO-1) | **none**: a moving ship unloads at its position at the apply tick. `ShipDisabled` (founding's bit `0x02`) and `ShipEmbarked` (navigation) are **not** used | ORIGINAL; any restriction is an owner decision |
| P4 | Eligible warehouse | building of PRODTYPE `0x23` (KONTOR) found by `0x1003f200` mode 0; the world function itself takes any city | the building named by `warehouse` exists (`NoBuildingAtTile`) and its type's PRODTYPE is in `CargoRules::warehouse_prodtypes` (default `{35}`; `NotAWarehouse`) | ORIGINAL type, data-defined |
| P5 | Target | the **city of that warehouse** (`building slot +0x18`); MP wire names (island, city number) | the warehouse's own city, **derived** from the building; never a client city, never "a city on the island" | ORIGINAL |
| P6 | Warehouse owner | view: `city owner == local player`; world: equal owners are free, relation 3 trades (excluded) | `city.owner == issuer` (`NotCityOwner`); every other owner is refused, allied or visible alike | SCOPE EXCLUSION: the original view also accepts a relation-3 (allied) owner; the rebuild's scope is own city only |
| P7 | Range, reference | the ship's **tile** `trunc(pos)` (f32 `/ 1.0`) | tile = `pos_q8 >> 8` (non-negative) | ORIGINAL |
| P8 | Range, test | at click time only: warehouse footprint inside the digital disc of radius 5 (`half-widths 5 5 5 4 3 2`, distance per axis to the nearer edge tile, 0 inside) **and** the warehouse's city owns one of the 22 distinct outline tiles (24 reads, section 5) | the same test **re-run at apply** against live state (`OutOfRange`) | ORIGINAL rule, ADAPTATION: revalidated |
| P9 | Which warehouse | the **first** qualifying one in discovery order; a rival's can hide the player's | the command **names** the warehouse; the first-found choice is a UI matter | ADAPTATION |
| P10 | Hold selection | by **hold index** `rec[1]`; goods `rec[0]` must equal the hold's goods; index `< n0` | `hold` index plus `goods` echo: `BadIndex`, `HoldGoodsMismatch`; **no unload-all** | ORIGINAL |
| P11 | Duplicate holds | no search; the named hold only | the same: two holds of one goods are two addresses; the index is the stable order | ORIGINAL |
| P12 | Request unit | whole tons (u16) | `tons` (u32), `0` refused (`BadValue`); the stored unit stays 1/256 t | ORIGINAL unit, widened |
| P13 | Amount moved (deviation list: residue differs by up to 255 units when free space is fractional) | `min(request, hold amount, ceil_T(capacity - stock))`; credit `min(that, capacity - stock)`; **debit may exceed credit by up to 255 units (lost)** | `min(request, hold amount, free)` with `free = max(0, capacity - stock)`; **debit == credit** | ADAPTATION (conservation) |
| P14 | Stock full, zero free | nothing moves (return 0) | `StockFull`, unchanged world | ORIGINAL effect, explicit reason |
| P15 | Stock above capacity | `< 256` over: nothing; `>= 256` over: the hold gains 256 and the stock is lowered (defect) | `free = 0`: `StockFull`; **stock never lowered, hold never gains** | ADAPTATION (hardening) |
| P16 | Goods id outside the city table | hold debited, credit refused: cargo destroyed (and an out-of-bounds read) | `UnsupportedGoods` before any write | ADAPTATION (hardening) |
| P17 | Emptied hold | amount 0 **and** goods id 0 | the same normal form | ORIGINAL |
| P18 | Gold, tax, message, event | none on the same-owner path | none; the transfer is reported only in the outcome | ORIGINAL |
| P19 | Statistics words `+6`/`+8` of the stock record | incremented by add/remove | not modelled (the rebuild's `CityStock` has no such words) | existing deviation, UNKNOWN consumer (OQ-CARGO-2) |
| P20 | Cannon arm `rec[6] == 1`, foreign/allied arm, load arm | exist in the same function | **not part of the command**; no `rec[6]`, no flag | out of scope |

## 3. The command and its outcome

```cpp
/// 0xF073 (native, MAP-CARGO-1): a ship unloads one hold, or part of it, into the warehouse's city.
/// Issuer = the authenticated player. Nothing about payer, owner, city or credit is supplied.
struct UnloadCmd {
    FigureId ship;
    BuildingId warehouse;        ///< stable: (island << 32) + serial, never reused
    std::uint32_t hold = 0;      ///< index into Figure::holds (the original's rec[1])
    GoodsId goods = 0;           ///< must equal the hold's goods (the original's rec[0])
    std::uint32_t tons = 0;      ///< requested whole tons (the original's rec[2..3], widened); 0 refused
    friend bool operator==(const UnloadCmd&, const UnloadCmd&) = default;
};
```

* Variant alternative 10 (after `StopCmd`), `kWireUnload = 0xF073`, native in `is_native_wire_type`; a `LegacyRawCmd` of that type is
  unencodable as for 0xF070-0xF072. Payload 28 bytes little-endian: `u64 ship, u64 warehouse, u32 hold, u32 goods, u32 tons`; decode refuses
  a wrong length. `kCommandWireVersion` is unchanged (the precedent of `MoveCmd`/`StopCmd`; U2 confirms against the replay-format rule and
  adds the independent command bytes).
* **No new authoritative state:** the holds and the city stock already exist, are hashed and saved. The only stream change is `Rules::cargo` (section 5, stream 124); the command itself adds none.
* **Outcome:** `ApplyResult.status`; on success `records[0]` carries `status None`, `building = warehouse` and a new field
  `std::uint64_t transferred_88 = 0` on `RecordResult` (0 for every other command and every refusal). The hold and the stock after the
  transfer are read from the world.
* **Rejects** (existing preferred; four appended after `ShipEmbarked`, alternative order is the replay format):
  `NotAWarehouse`, `HoldGoodsMismatch`, `UnsupportedGoods`, `StockFull`.

Validation order (the first failing step answers; each step reads live state; a refusal changes **no serialized byte**, no PRNG, no gold, no
cargo, no stock; the only side effect is the existing host-facing `CommandRejected` event that `Commands::apply` pushes for every refusal and
that is never hashed):

| Step | Check | Reject |
|---|---|---|
| 0 | issuer is a known player | `UnknownIssuer` |
| 1 | `tons != 0` | `BadValue` |
| 2 | ship id in the registry | `UnknownFigure` |
| 3 | ship class | `NotAShip` |
| 4 | `ship.owner == issuer` | `NotShipOwner` |
| 5 | warehouse building exists (demolished or replaced = a different or missing `BuildingId`) | `NoBuildingAtTile` |
| 6 | building type's PRODTYPE in `warehouse_prodtypes` | `NotAWarehouse` |
| 7 | the building's city exists and `city.owner == issuer` | `UnknownCity` / `NotCityOwner` |
| 8 | range (section 5) | `OutOfRange` |
| 9 | `hold < min(holds.size(), type cargo hold count)` | `BadIndex` |
| 10 | `0 < goods < city stock goods count` | `UnsupportedGoods` |
| 11 | `holds[hold].goods == goods` and `holds[hold].amount_88 != 0` | `HoldGoodsMismatch` (a hold with a goods id and amount 0 is loadable from saves/manifests and is treated as emptied, as `founding_payment.cpp` does: it holds none of the goods; no new reject, no invariant is assumed) |
| 12 | `free > 0` | `StockFull` |

An empty hold (goods 0) fails step 11 for any supported goods and step 10 for goods 0. The refusal of step 11 for amount 0 is explicit (vector `hold_with_goods_id_and_amount_zero`); `plan_unload` returns it and relies on no invariant.

## 4. Units, arithmetic, capacity (all raw 1/256 ton)

| Quantity | Type | Note |
|---|---|---|
| `ShipHold::amount_88` | u64 | widened from the original u16 (stream 117) |
| `CityStock::stock[g]`, `capacity` | u32 each | `capacity` is one value for every goods (ORIGINAL: `city + 0x624`) |
| request | `u64(tons) * 256` | `tons <= 2^32 - 1`, product `< 2^40`, no overflow |
| `free` | `u64(capacity) - u64(stock)` if positive, else 0 | stock above capacity gives 0 |
| transferred `x` | `min(request, hold.amount_88, free)` | `x > 0` after steps 1-12 |

```
x            = min(request, hold.amount_88, free)
hold.amount  = hold.amount - x;  if it is 0: hold.goods = 0          (emptied-hold normal form)
stock[g]     = stock[g] + x                                          (<= capacity <= 2^32 - 1: checked in u64, asserted)
```

* **Fractions.** A request is whole tons; fractions come from the stored amounts. A hold with 1.5 t that receives a 1 t request gives 1 t and
  keeps 0.5 t; a second 1 t request takes the remaining 0.5 t and empties the hold. A fractional free space (`capacity - stock` not a multiple
  of 256) is filled exactly and the **rest stays aboard** (ADAPTATION of P13: the original destroys up to 255 units here; the residue left aboard differs from the original's by up to 255 units whenever the free space is fractional). The resulting
  stock equals the original's in every state the original produces (`tools/cargo_ref.py selftest` sweeps this); only the hold differs.
* **Conservation.** For every accepted command `hold_before + stock_before == hold_after + stock_after` in a wide accumulator, and
  `debit == credit == transferred_88`. A refusal leaves both unchanged. No path mints, erases or wraps.
* **`CityStock::add` (`src/sim/src/production.cpp`) must not be the transfer.** It returns 0 for `g >= stock.size()` (without writing), forms
  the sum in u64 and, when `capacity < stock + amount`, sets `amount = capacity - stock` in **u32 arithmetic**, so a stock above the capacity
  is **lowered to the capacity** and the return value is a wrapped delta; it returns the u32-wrapped change, not "what was credited". These are
  the original's established ordinary-state behaviours for deliveries by carts and production and stay as they are. For the transfer U2 adds
  `CityStock::free_88(g)` and `CityStock::credit(g, x)` (refuses unless `g < size` and `x <= free`; never lowers a stock; no clamp) and the
  command calls `credit` with the planned `x`; it never deducts the request after calling `add`.
* **Capacity.** The opening city has one market-class building and `CityStock{}.capacity = 50 t`, equal to the original's table entry for one
  building (`0x100dcc84[1] = 50`). The original recomputes the capacity after each build and lowers stocks that exceed a shrunk capacity
  (`0x10028400`); the rebuild does not model that recompute (existing gap, not changed here), which is why a stock above the capacity is
  reachable only from a constructed or loaded state and is handled by P15, not by an assumption.
* **Per-goods limit.** Capacity applies to each goods separately: the baseline's 75 t of tools cannot all be unloaded into a 50 t store
  (section 7, row B14).

## 5. Range and docking (the original's test, re-run at apply)

`CargoRules` (registry/rules data, validated, hashed with the rules; defaults are the original's):

| Field | Default | Evidence |
|---|---|---|
| `warehouse_prodtypes` | `{35}` | `cmp eax, 0x23` at `0x1003f535` |
| `dock_half_widths` | `{5, 5, 5, 4, 3, 2}` | recurrence `0x1003f217`; radius `r = size - 1`; `half_widths[0] == r`, non-increasing |

Let `(tx, ty)` be the ship's tile and `r = dock_half_widths.size() - 1`.

1. **Near cities.** For `k = 0..r` collect the owners of the tiles `(tx -/+ w[k], ty -/+ k)` and `(tx -/+ w[k], ty +/- k)` (the outline of the
   disc, `w = dock_half_widths`; tiles outside every island or without a city owner are skipped). The warehouse's city must be one of them.
   Only the outline is sampled; a city that owns only interior tiles is **not** near (vector `interior_only`).
2. **Footprint.** From the building's origin and its rotated footprint `{x0, y0, w, h}`: per axis `d = 0` if the ship tile lies inside the
   footprint on that axis, else the distance to the nearer edge tile. In range iff `dy <= r` and `dx <= w[dy]`.
3. Both must hold; a failure is `OutOfRange`. The ship's fractional position beyond the tile is ignored (truncation).

The 11 x 11 rectangle query of the original is implied by `dx, dy <= r`; the rebuild uses the footprint test directly (no scan of the market
list is needed for a named building). The rule uses live state: a ship that moved away between submission and execution is refused.

## 6. Commit order, atomicity, repeats, replacement

* **Plan, then commit.** Steps 0-12 and the amount are computed from the live state with no write. The commit is two stores of precomputed
  values: the hold entry (amount, and goods 0 when it reaches 0), then `credit` of the stock; neither allocates or can fail, so there is
  nothing to roll back. U2 asserts `credited == x`.
* **Same turn, repeated.** Commands of one turn run in the host's order (`TurnEntry::seq`); each revalidates, so a second command sees the first
  one's result (a second identical request moves more cargo, or is refused `StockFull`). Two ships unloading into one nearly full city are
  served in command order and the loser gets `StockFull` (vectors `two_ships_contend_*`).
* **Intentional repeat vs retransmission.** Two `TurnEntry` values with different `seq` are two intended commands and both execute. A
  retransmitted entry has the **same** `seq`; dropping it is the job of the turn layer before `apply` (the original resends whole turns by
  turn number, `0x10055320`). The simulation has no per-command dedupe and gets none.
* **Demolition or replacement between submission and execution.** The warehouse is looked up by `BuildingId` at apply. A demolished building,
  or one replaced at the same tile (a new serial), is `NoBuildingAtTile`; the same command naming the new building is a new command. A city
  whose owner changed is `NotCityOwner`; a ship that sank or was removed is `UnknownFigure`.
* **Replay and save/load.** The log holds the command bytes; a replay applies the same validated plan to the same state. Because no state is
  added, save before or after a transfer is the existing holds and stock, and a changed `tons`, `hold` or `warehouse` changes the result and
  therefore the hash.

## 7. Acceptance table (hand-derived before any engine code)

Units are 1/256 t. "Ref" rows are executed by `tools/cargo_ref.py check` (60 cases at the time of writing); "Engine" rows need the world and
belong to U3. `T` = 256. Default store: capacity 12800 (50 t), goods count 56. Goods: 26 food, 41 tools, 42 wood.

### A. Transfer arithmetic (Ref; vector names)

| Case | Hold / stock / request | Expected |
|---|---|---|
| `exact_whole_hold` | wood hold 38 t, stock 0, ask 38 t | ok 9728; hold `0:0`; stock 9728 |
| `request_smaller_than_hold` | wood 38 t, ask 10 t | ok 2560; hold 7168; stock 2560 |
| `request_larger_than_hold_takes_all` | food 10 t, ask 50 t | ok 2560; hold `0:0` |
| `partial_by_capacity` | tools hold 30 t, stock 45 t, ask 30 t | ok 1280 (5 t); hold 6400; stock 12800 |
| `zero_capacity` | capacity 0 | `StockFull`; unchanged |
| `stock_equals_capacity` | stock 12800 | `StockFull`; unchanged |
| `over_capacity_by_200` | stock 13000 | `StockFull`; unchanged |
| `over_capacity_by_300_original_defect` | stock 13100, hold 1000 | `StockFull`; hold stays 1000 (the original: hold 1256, stock 12800) |
| `duplicate_holds_are_addressed_by_index` | tools 50 t and 30 t; ask index 1 (10 t), index 0 (50 t), index 1 (1 t) | 2560, 10240, then `StockFull`; holds `41:2560 41:5120`; stock 12800 |
| `fractional_hold_remainder_two_commands` | wood 384 units; two 1 t requests | 256 then 128; hold `0:0`; stock 384 |
| `fractional_free_space_ship_keeps_the_excess` | stock 12672, hold 768, ask 3 t | ok 128; hold 640 (the original: debit 256, hold 512) |
| `one_unit_free` | stock 12799, hold 9728, ask 1 t | ok 1; hold 9727 (the original: debit 256) |
| `free_space_exactly_one_ton` | stock 12544 | ok 256; hold 9472 (equal to the original) |
| `fractional_stock_whole_request` | stock 100, hold 38 t | ok 9728; stock 9828 |
| `u32_boundary_...` | capacity 2^32-1, stock 2^32-101, hold 1000, ask 1 t | ok 100; stock 2^32-1; hold 900 (the original wraps to 155) |
| `hold_amount_u64_maximum` | hold 2^64-1, ask 1 t | ok 256; hold 2^64-257 |
| `request_and_hold_far_above_u32_units` | capacity 2^32-1, hold 2^41, ask 2^32-1 t | ok 2^32-1 |
| `last_supported_goods_id` / `first_unsupported_goods_id` | goods 55 / 56 | ok 2560 / `UnsupportedGoods`, hold intact |
| `unsupported_goods_id_is_not_erased` | hold goods 60 | `UnsupportedGoods`; hold 60:5000 intact |
| `empty_hold_goods_requested` / `..._zero_requested` | hold `0:0` | `HoldGoodsMismatch` / `UnsupportedGoods` |
| `wrong_goods_for_the_hold`, `hold_index_out_of_range`, `zero_request` | | `HoldGoodsMismatch`, `BadIndex`, `BadValue` |
| `order_*` | simultaneous failures | `BadValue` before `BadIndex` before `UnsupportedGoods` before `HoldGoodsMismatch` before `StockFull` |
| `two_ships_contend_first_wins` / `_order_swapped` | stock 11800 (free 1000), two 5000-unit holds, ask 10 t each | the first gets 1000, the second `StockFull`; total conserved 21800 |
| `other_goods_and_holds_unchanged` | three goods, three holds | only hold 0 and wood stock change |
| `range_*`, `cityset_*`, `tile_*` | section 5 | 15 + 8 + 3 vectors |

### B. Identity, state and sequences (Engine, U3/U4; hand-derived here)

World: players P0, P1; P0's ship S (1502) at tile (10, 10); P0's Kontor W (1302, 4 x 4) at (15, 10) in P0's city C0; holds as stated.

| # | Setup | Command | Expected |
|---|---|---|---|
| B1 | S hold 0 wood 38 t, C0 wood 0 | P0: `{S, W, 0, wood, 38}` | ok `transferred_88 = 9728`; hold 0 `0:0`; C0 wood 9728; gold, PRNG, other goods unchanged |
| B2 | as B1 | P1 issues it | `NotShipOwner`; world hash unchanged |
| B3 | as B1 | ship id 999 | `UnknownFigure` |
| B4 | as B1 | `ship` = the embarked scout (717) | `NotAShip` |
| B5 | as B1, W demolished first | B1's command | `NoBuildingAtTile` |
| B6 | as B1, W demolished and a new Kontor W2 built on the tile | B1's command / the command naming W2 | `NoBuildingAtTile` / ok (if W2 is in range) |
| B7 | P0 market 1301 (PRODTYPE 34) at (15, 10) instead of W | command naming it | `NotAWarehouse` |
| B8 | rival P1 Kontor R at (15, 10) (P0's ship sees it) | command naming R | `NotCityOwner`; shared visibility grants nothing |
| B9 | W at (16, 10) | B1 | `OutOfRange` (dx 6) |
| B10 | W on another island | B1 | `OutOfRange` |
| B11 | the ship has an active order and moves | B1 | executes at the position of the apply tick; refused iff that position is out of range |
| B12 | C0 owns only the interior tiles around S | B1 | `OutOfRange` (outline rule) |
| B13 | two identical B1 commands in one turn | both | first ok 9728, second `HoldGoodsMismatch` (the hold is `0:0`) |
| B14 | **opening after founding**: holds `[wood 38 t, food 10 t, tools 45 t, tools 30 t]` (baseline `wood 50, food 10, tools 50, tools 30` minus the 1302 cost wood 12 t, tools 5 t), C0 empty, capacity 50 t | one command per hold, full tons | wood 9728, food 2560, tools hold 2: 11520, tools hold 3: **1280 only** (store full at 50 t); ship keeps `tools 6400 (25 t)` in hold 3; conserved |
| B15 | the next turn, ship has 6400 tools aboard | the same command again | `StockFull` |
| B16 | after B1, save and load; then apply B1 again | | `HoldGoodsMismatch`; the loaded world equals the uninterrupted one |

## 8. The cargo-dependent sailing speed today (U2 must not touch it)

`docs/sim/ship-navigation.md` 12.8 and 12.14: the speed factor `V` is **derived at every movement segment start** from the ship's current holds
(`nav_speed_factor(facts, rules, capacity_88, holds, owner_volk, health)`, `ship_step.cpp`: the sum of the first `min(hold_count_a, holds.size())`
holds' amounts, clamped to `(hold_count_a + hold_count_b) * capacity_88`); a segment under way keeps the factor it started with, and turn and idle
segments use 100. The original recomputes and stores `+0x3c` on every hold change (`0x1003b9a0` -> ship slot `+0xa0` = `0x100427b0`) but reads it
only at a segment start, so the two agree. Consequences for U2: the unload writes **only** `Figure::holds`; it does not write `NavMotion`, the
segment or any factor; a test that cargo affects sailing starts a **later** segment after the unload (section 3 of `ship_health_test`-style cases),
never edits a running one. The crew (children) are not touched.

## 9. Test obligations handed to U2/U3

* `0xF073` is currently pinned as a legacy type in `src/sim/tests/command_codec_test.cpp:509-512`; U2 must edit that test (as `0xF071`/`0xF072` were removed from the list).
* Codec, dispatch, `RecordResult::transferred_88`, the four rejects and independent command bytes; version-negative tests reseal a valid control.
* Positive control B1 with a known non-zero amount; then one predicate at a time (B2-B12), each asserting its intended reason and an unchanged
  state hash. Conservation checked in a wide accumulator on every accepted case (A rows, contention rows, near-bound rows).
* Two ENGINE mutants, to be written in U3 (they do not exist yet; the Python reference already rejects the analogous mutants, and the mutant
  `free = cap - sum(all goods)` fails the new per-goods vectors): credit without debit, and debit of the request despite a smaller credit, through the normal codec/apply path; both must fail the conservation assertion while the positive control passes.
* Registry-driven: another owner and a data-defined ship type and warehouse type (no player 0, four holds, 1502/1302 or founding whitelist);
  `CargoRules` validation tests (empty table, `half_widths[0] != r`, increasing table).
* Save/load before and after, fresh-process replay, and a changed amount or target that diverges.

## 10. Questions that remain (none blocks U2)

OQ-CARGO-1 (does the view disable the tab for a moving ship; the contract adds no state test), OQ-CARGO-2 (consumer of the stock words `+6`/`+8`
and of the foreign arm's notification records), OQ-CARGO-3 (cannon, `0x6b` and foreign arms: separate packages). `docs/open-questions.md`.

## 11. U2/U3 as implemented (2026-11-02)

`UnloadCmd` is variant alternative 10, tag `0xF073` (`command.hpp`, `codec.cpp`), dispatched by `Commands::apply` to `ShipCargo::unload`
(`ship_cargo.hpp/.cpp`, a friend of `World`). Validation is the order of section 3 against live state, resolving the ship with
`find_figure` and the warehouse by the island its id names (`Island::find_building`, the `by_id` cache for its city): no scan and no clone.
`CityStock::free_88` / `credit` implement section 4 (wide, no clamp, never lowers a stock). The command adds no
authoritative state; `CargoRules` moves the stream to 124 (state.md), `kCommandWireVersion` is unchanged.

* **Ship class** is the founding veto's BEWOHNTYPE 113 (`NotAShip` otherwise); the hold limit of step 9 is `min(holds.size(),
  hold_count_a + hold_count_b)` when the type has navigation facts, else `holds.size()`.
* **Rules (OQ-CARGO-4, closed).** `CargoRules` (`warehouse_prodtypes`, `dock_half_widths`) is a hashed, saved part of `Rules` from stream 124
  (`config.hpp`, `state_fields.hpp`); the eligibility test is `warehouse_prodtypes` against the building type's PRODTYPE, not `founding_types`.
  Validation in `Rules::validate` (specific `CargoRules.<field>` diagnostics). A data-defined warehouse type with its own PRODTYPE needs only a
  rules entry (`unload data`).
* **Fractional requests** cannot be sent: the wire carries whole tons, so the "fractional request" case does not exist; fractional stored
  amounts and remainders are exact (tests `unload fractions`).
* **Acceptance table coverage** (`src/sim/tests/unload_test.cpp`): A rows: every vector of `cargo_vectors.txt` through the engine (34 cases, 15
  range, 3 tile, 8 city-set rows); B1 `unload B1`; B2, B3, B4, B7, B8, B9, B10 `unload predicates`; B5, B6 `unload B5 B6`; B11 `unload B11`
  and `unload sailing`; B12 `unload B12`; B13-B15 `unload B13 B14 B15`; B16 `unload B16` and `unload session`.

