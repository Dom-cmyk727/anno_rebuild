# Ship cargo: unload a ship's hold into the owner's warehouse (contract)

**Status:** MAP-CARGO-1 U1, 2026-11-02. A **specification and an independent reference, before any engine code (HISTORICAL wording of the U1 draft; section 11 records what was implemented)**. Evidence:
[`docs/re/ship-cargo.md`](../re/ship-cargo.md) (every address below is explained there). Independent reference and vectors:
`tools/cargo_ref.py`, `fixtures/synthetic/sim/cargo_vectors.txt`. Labels: **ORIGINAL** a traced rule kept; **ADAPTATION** a deliberate
change of the rebuild; **UNKNOWN** unread, with a bounded blocker in `docs/open-questions.md` (OQ-CARGO-*).
Scope (owner, 2026-11-02): the manual transfer ship hold -> **own** warehouse/city storage through the headless command path.
Excluded (for sections 1-11; sections 12 ff. propose the load): loading city goods aboard, automatic routes, buying/selling, foreign or allied warehouses, cannons, crew, combat, UI,
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
* Two ENGINE mutants, to be written in U3 (they do not exist yet [HISTORICAL: superseded by section 11; the tag test was edited and the engine mutants were run in U3]; the Python reference already rejects the analogous mutants, and the mutant
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



## 12. Loading: an owned warehouse's city -> the ship's hold (MAP-CARGO-2 L1, PROPOSED, 2026-11-02)

**Status:** a proposed contract, before any engine code; it needs the independent contract/reference review before L2. Evidence:
[`docs/re/ship-cargo.md`](../re/ship-cargo.md) section 11 (the click, the wire, `AnnoWelt.dll 0x100ac360` flag 0). Independent reference:
`tools/cargo_ref.py` (`plan_load` = this section, `orig_load` = the original's arithmetic), 42 hand-derived cases in
`fixtures/synthetic/sim/cargo_load_vectors.txt`. Labels as in section 2. Scope: the manual transfer city store -> **own** ship hold through the headless
command path. Excluded: automatic route stops (they share the world function but their record is not read), buying from foreign or allied warehouses
(the original's relation-3 arm), the cannon arms, `0x6b`, crew, UI. Shared visibility is information, never permission. The sailing-speed contract is
unchanged: the cargo factor is derived at **segment start** from the current holds (section 8); a load writes only `Figure::holds`.

### 12.1 What the original does, in six lines (ORIGINAL, section 11)

The "Load goods" click sends `{goods wanted, hold hint, tons}` plus the ship and the city found by the same `0x1005d8c0`/`0x1003f200` as the unload. The world arm
takes the **named hold only if it already holds that goods and has a whole ton free**; otherwise it **searches** holds `0 .. n0-1` for the same-goods hold with the most
room (any room > 0, lowest index on ties), else the first empty hold, else does nothing. The amount is `min(request tons, room of that hold, the city's stock rounded down to whole
tons)`; the hold gets the goods id and the amount, the city loses the same amount. There is no range, state or sender test at execution.

### 12.2 The command and its outcome

```cpp
/// 0xF074 (native, MAP-CARGO-2): a ship takes goods aboard from the city of the named warehouse.
/// Issuer = the authenticated player. Nothing about payer, owner, city, price or debit is supplied.
struct LoadCmd {
    FigureId ship;
    BuildingId warehouse;        ///< stable: (island << 32) + serial, never reused
    std::uint32_t hold = 0;      ///< the PREFERRED hold (the original's rec[1]); the plan may pick another (12.4 step 11)
    GoodsId goods = 0;           ///< the goods to take aboard (the original's rec[0]); NOT an echo of the hold
    std::uint32_t tons = 0;      ///< requested whole tons (the original's rec[2..3], widened); 0 refused
    friend bool operator==(const LoadCmd&, const LoadCmd&) = default;
};
```

* **Wire tag `0xF074`**, variant alternative 11 (after `UnloadCmd` = 10, so no existing alternative moves), `kWireLoad = 0xF074`, native in `is_native_wire_type`. Checked: `0xF074` is
  unused in `src`, `docs`, `tools`; the **only** mention is `src/sim/tests/command_codec_test.cpp:551-554`, which pins `0xF074` as a *legacy* neighbour (`{0x70, 0xF06F, 0xF074}`):
  L2 must edit that test (move the legacy neighbour to `0xF075`) and add `{0xF074, 28}` to the wrong-length table at line 283. `0xF06F` and `0xF075` stay legacy.
* **Payload 28 bytes**, little endian, same layout as `UnloadCmd`: `u64 ship, u64 warehouse, u32 hold, u32 goods, u32 tons` (`ship`/`warehouse` as `FigureId`/`BuildingId` raw u64). Decode refuses a wrong length.
  `LegacyRawCmd` of `0xF074` becomes unencodable. `kCommandWireVersion` stays 1 (the precedent of `MoveCmd`, `StopCmd`, `UnloadCmd`); L2 confirms against the replay-format rule.
* **Outcome:** `ApplyResult.status`; on success `records[0]` carries `status None`, `building = warehouse` and `transferred_88` (**reused**: the amount that left the city and entered the hold, 1/256 ton).
  The hold that received it is not a new field: it is a deterministic function of the pre-state (12.4 step 11) and readable from the world. If L3 finds a test or UI needs it directly, a `std::uint32_t hold` on
  `RecordResult` is a non-hashed, non-serialized addition; it is not required by this contract.
* **Rejects** reused: `UnknownIssuer`, `BadValue`, `UnknownFigure`, `NotAShip`, `NotShipOwner`, `NoBuildingAtTile`, `NotAWarehouse`, `UnknownCity`, `NotCityOwner`, `OutOfRange`, `BadIndex`, `UnsupportedGoods`.
  **Appended** after `StockFull` (alternative order is the replay format), three, all necessary because no existing enumerator says the same:
  `NoCargoFacts` (the ship's type has no cargo hold count; explicit refusal instead of a guess), `HoldsFull` (no hold can take this goods: none of the first `n0` holds is a same-goods hold with room or an empty hold with
  capacity), `StockShort` (the city holds less than one whole ton of the goods). `HoldGoodsMismatch` is **not** used: the original never refuses a different-goods hold, it redirects.

### 12.3 Predicate table (load)

| # | Predicate | Original | Rebuild | Class |
|---|---|---|---|---|
| L-P1 | Issuer | none (MP unauthenticated) | host-stamped `Command::issuer`; no payer/owner/city/price in the command | ADAPTATION (as P1) |
| L-P2 | Eligible ship | view offers the tab for type group `0x71`; the arm takes any figure with holds | figure exists, ship class (BEWOHNTYPE 113), `ship.owner == issuer` | original class + ADAPTATION owner (as P2) |
| L-P3 | Ship state (moving, turning, hit points, carried) | **no test** in the arm; tab enabling U (OQ-CARGO-1/5) | **none**: a moving ship loads at its position at the apply tick | ORIGINAL (as P3) |
| L-P4 | Eligible warehouse, target city | the same `0x1005d8c0` -> `0x1003f200` mode 0 as the unload click: PRODTYPE `0x23`, the building's own city | the building named exists, PRODTYPE in `CargoRules::warehouse_prodtypes`; the city is **derived** from it | ORIGINAL type/target, data-defined (as P4, P5) |
| L-P5 | Warehouse owner | arm: ship owner == city owner for the free path (else the relation-3 trade arm, excluded) | `city.owner == issuer` (and the ship's owner is the issuer); every other owner refused, allied or visible alike | SCOPE EXCLUSION (as P6) |
| L-P6 | Range | click time only; the **same** function as the unload | the same test as section 5, re-run at apply against live state | ORIGINAL rule, ADAPTATION: revalidated (as P7-P8) |
| L-P7 | Cargo facts | `n0 = LOGIC[0x13]`, per-hold capacity `LOGIC[0x11] << 8` of the figure type | `NavFacts::hold_count_a` and `FigureType::capacity_88`; a type without nav facts: `NoCargoFacts`; capacity 0: `HoldsFull` | ORIGINAL fields; ADAPTATION: explicit refusal |
| L-P8 | Hold index | `rec[1] < n0` (signed), else return 0 | `hold < min(holds.size(), hold_count_a)` else `BadIndex`; **not** `hold_count_b` | ORIGINAL |
| L-P9 | Goods bound | **none** (any byte; out-of-table read and write, defect E1; goods 0 accepted) | `0 < goods < city stock table size` else `UnsupportedGoods`; no whitelist, no class test (the arm has none) | ADAPTATION (hardening) |
| L-P10 | Request unit | whole tons, u16 | `tons` u32, 0 refused (`BadValue`); stored unit 1/256 t | ORIGINAL unit, widened |
| L-P11 | Hold choice | preferred hold if same goods and >= 256 units free, else search (12.4 step 11) | the same; the empty test is goods 0 **and** amount 0 | ORIGINAL; ADAPTATION only for the empty test (E3) |
| L-P12 | Amount | `min(request, hold room, floor_T(stock))`, signed 32-bit | the same in u64: `min(tons*256, hold_cap - amount, floor_T(stock))` | ORIGINAL; widened (E2) |
| L-P13 | Stock fraction | stock rounded **down** to whole tons; a remainder stays in the city | the same | ORIGINAL |
| L-P14 | Credit vs debit | both are the same `esi`; no loss or mint on the same-owner path | `debit == credit == transferred_88`, asserted | ORIGINAL (already conserving) |
| L-P15 | Hold fraction | a hold with `< 1 t` free is only used through the search; the transfer then equals the room (a fractional amount) | the same | ORIGINAL |
| L-P16 | Different goods in the named hold | not a refusal: search elsewhere | the same; refusal only as `HoldsFull` when nothing fits | ORIGINAL |
| L-P17 | Duplicate holds | the emptiest same-goods hold (strict `>`), lowest index on ties; a named same-goods hold with >= 1 t free is taken first | the same | ORIGINAL |
| L-P18 | Stock above store capacity, `goods` held by the city | irrelevant: the load debits only | irrelevant | n/a |
| L-P19 | Gold, tax, message, event, statistics word `+8` | none (the remove increments `removed`, U consumer, OQ-CARGO-2) | none | ORIGINAL; word not modelled |
| L-P20 | Cannon arms, foreign/relation-3 arm, route stop record, `0x6b` | exist / share the world function | **not part of the command**; no `rec[6]`, no flag, no price | out of scope, UNKNOWN (OQ-CARGO-3) |

### 12.4 Validation and commit order

Each step reads live state; a refusal changes **no serialized byte**, no PRNG, no gold, no holds, no stock (only the existing host-facing `CommandRejected` event).

| Step | Check | Reject |
|---|---|---|
| 0 | issuer is a known player | `UnknownIssuer` |
| 1 | `tons != 0` | `BadValue` |
| 2 | ship id in the registry | `UnknownFigure` |
| 3 | ship class | `NotAShip` |
| 4 | `ship.owner == issuer` | `NotShipOwner` |
| 5 | warehouse building exists | `NoBuildingAtTile` |
| 6 | building type's PRODTYPE in `warehouse_prodtypes` | `NotAWarehouse` |
| 7 | the building's city exists and `city.owner == issuer` | `UnknownCity` / `NotCityOwner` |
| 8 | range (section 5), the unload's helper | `OutOfRange` |
| 9 | the ship's type has `nav` facts (`hold_count_a`) | `NoCargoFacts` |
| 10 | `hold < min(holds.size(), hold_count_a)` | `BadIndex` |
| 11 | `0 < goods < city stock table size` | `UnsupportedGoods` |
| 12 | **resolve the hold** `r`: if `holds[hold].goods == goods` and `cap - amount >= 256` then `r = hold`; else over `k = 0 .. min(holds.size(), hold_count_a) - 1`: the first hold with goods 0 **and** amount 0 is the candidate unless a hold with `goods` and `room = cap - amount > 0` exists, then the one with the largest room (strictly greater; lowest index on ties). No candidate, or `room(r) <= 0` (`cap` = `FigureType::capacity_88`): | `HoldsFull` |
| 13 | `avail = floor(stock[goods] / 256) * 256` (u64); `avail == 0` | `StockShort` |
| 14 | `x = min(tons * 256, cap - amount(r), avail)`; `x > 0` here (steps 1, 12, 13) |  |
| commit | `stock.debit(goods, x)` (new `CityStock::debit`: refuses unless `goods` in table and `x <= stock[goods]`; no clamp, no wrap), then `holds[r].goods = goods; holds[r].amount_88 += x` (checked add) | cannot fail; L2 asserts `debited == x` |

Competing requests in one turn run in `TurnEntry::seq` order; each revalidates and sees the previous result (vectors `competing_two_ships_*`). Repeated and retransmitted entries follow section 6. Demolition,
replacement or an ownership change between submission and execution give the same results as for the unload.

### 12.5 Units and arithmetic (raw 1/256 ton)

| Quantity | Type | Note |
|---|---|---|
| `ShipHold::amount_88` | u64 | `amount + x <= cap` is checked in u64 |
| `FigureType::capacity_88` | u32 | per hold; shipped 1502: 50 t = 12800 (original: u16, `LOGIC[0x11] << 8`) |
| `CityStock::stock[g]` | u32 | read as u64; `floor_T` = `(stock / 256) * 256` |
| request | `u64(tons) * 256` | `tons <= 2^32 - 1`, product `< 2^40` |
| room | `cap - amount` if positive else 0 | an over-capacity hold has no room (vectors `overfull_*`) |

Conservation: `hold_after + stock_after == hold_before + stock_before` in a wide accumulator, `debit == credit == transferred_88`; a refusal changes nothing. `x` is a whole number of tons unless
the chosen hold's room is fractional; then `x` equals the room exactly (vector `fractional_hold_room_moves_a_fraction`: 128 units leave the city, 128 enter the hold). **`CityStock::remove` must not be the
debit** (it clamps silently and ignores `g >= size`); `CityStock::add` must not be used for anything here. A load followed by an unload of the same hold with no `Advance` between restores hold and stock
(vectors `round_trip_*`); a smaller store limits the unload, the remainder stays aboard (`round_trip_into_a_smaller_store`).

### 12.6 Data-driven facts and what is missing

| Fact | Where | Missing / zero means |
|---|---|---|
| per-hold capacity | `FigureType::capacity_88` (`types.cpp:453`) | 0 -> every load `HoldsFull` (the type carries nothing); no default is substituted |
| cargo hold count `n0` | `NavFacts::hold_count_a` (`types.cpp:503`) | no `nav` facts -> `NoCargoFacts`; **no fallback to `holds.size()`** (the unload falls back; a guessed limit is exactly what this refusal avoids) |
| goods table size | `CityStock::stock.size()` | goods outside -> `UnsupportedGoods` |
| warehouse set, dock disc | `Rules::cargo` | as the unload |
| hold count `n1` (`hold_count_b`) | not used by the load | the load never addresses unit slots (vector `unit_slots_beyond_n0_are_never_used`) |

Retained limitation (disclosed in CARGO-1): the ship class is BEWOHNTYPE 113 with navigation facts; no separate capability redesign. **Finding F-L1:** the accepted unload bounds the hold index by
`hold_count_a + hold_count_b` (`ship_cargo.cpp`, step 9) where the original bounds it by `n0` (`0x100ac476`); identical for the shipped opening ship, different for a ship with `n1 > 0` and extra hold records.
This section does not change the unload; the decision (align it to `n0`) is for the orchestrator and the next CARGO review.

### 12.7 Original versus adaptation (summary)

| Topic | Original | This contract |
|---|---|---|
| Hold selector | a hint; redirected when the hold has other goods or < 1 t free | the same (ORIGINAL), reported through the world state |
| Different goods in the hold | search elsewhere, return 0 if nothing fits | the same; `HoldsFull` names the failure |
| Empty hold | goods 0 | goods 0 **and** amount 0 (ADAPTATION: the original would relabel an unnamed amount, E3) |
| Goods id | any byte, out-of-table access, goods 0 accepted | bound by the table, 0 refused (ADAPTATION, E1) |
| Arithmetic | 32-bit signed compares; negative `esi` for a stock >= 2^31 (E2) | u64 throughout (ADAPTATION, E2) |
| Stock rounding | down to whole tons; remainder stays | the same (ORIGINAL) |
| Debit vs credit | equal | equal, asserted (ORIGINAL, no hardening needed) |
| Crew / carried / moving state | no test | no test (ORIGINAL); any restriction is an owner decision |
| Range | click time | re-run at apply (ADAPTATION) |
| Sender | none | host-stamped issuer must own ship and city (ADAPTATION) |
| Missing capacity facts | not possible (every type has the bytes) | explicit refusal (ADAPTATION of the registry model) |

### 12.8 State identity: which fields need new hashed data

**None.** The command adds no authoritative state. The per-hold capacity (`FigureType::capacity_88`) and the hold count (`NavFacts::hold_count_a`) are already loaded, validated (`nav_facts_problem`) and in the hashed
registry digest (`types.cpp` lines 334 and 348); `Rules::cargo` (`warehouse_prodtypes`, `dock_half_widths`) is already hashed (stream 124). The three new `Reject` enumerators and the new wire tag are not state. So **no stream bump** is
required by this contract; L2 must not bump it unless review finds a field above that is not in the digest. If the owner later wants a per-ship-class load policy (a rule not in the original), that is new hashed `Rules` data and a
bump; this contract does not add one.

### 12.9 Test obligations handed to L2/L3

* Independent command bytes (hand-written) and wrong-length cases for `0xF074`; edit `command_codec_test.cpp` lines 283 and 551-554; version-negative tests reseal a valid control.
* Positive control with a non-zero amount, then one predicate at a time, each asserting its reason and an unchanged state hash (steps 0-13); a refusal changes no serialized byte.
* Every case of `cargo_load_vectors.txt` through the engine (42 cases); engine mutants: credit the hold without debiting the city; debit the request although the credit is smaller; both must fail the conservation
  assertion while the positive control passes (the Python reference already rejects the analogous mutants: `cargo_ref.py mutants`).
* Registry-driven: another owner, a data-defined ship type with its own capacity and `hold_count_a`, a type without nav facts (`NoCargoFacts`), capacity 0 (`HoldsFull`); no 1502/1302/player-0 constants.
* Save/load before and after a load; fresh-process replay of a log with the command; the real opening canary: found -> unload -> **load back** conserves the combined goods; the cargo factor of a segment started after the load uses
  the new holds, a segment under way keeps its factor.
* Add `cargo_load_vectors.txt` to the CMake reference test (`cargo_reference_agrees` currently runs only the unload file; `cargo_reference_selftest` already runs the load file and the mutants).

### 12.10 L2/L3 as implemented (MAP-CARGO-2, 2026-11-02)

* **Code:** `LoadCmd` (`command.hpp`, variant alternative 11, `kWireLoad = 0xF074`, 28 bytes), `ShipCargo::load` and `ShipCargo::resolve_hold` (`ship_cargo.cpp`), `CityStock::debit`
  (`production.cpp`; refuses unless `goods` is in the table and `amount <= stock[goods]`, no clamp, no wrap), `Reject::NoCargoFacts / HoldsFull / StockShort` appended after `StockFull`.
  The validation order is the table of 12.4 (steps 0-14); the commit is `debit(goods, x)` then `holds[r] = {goods, amount + x}`, with `x` and `r` planned before any write and `amount + x <= capacity`.
  The unload is untouched (finding F-L1 stays an open note); the 24-tile outline test of the unload is repeated in an anonymous helper of `ship_cargo.cpp` rather than extracted, to keep the accepted unload unchanged.
* **State identity:** no hashed field was added; `kStateHashVersion` stays 124 and no golden moved (`tools/sim_stream.py --check`). The wire tag and the three enumerators are not state.
* **Tests:** `src/sim/tests/load_test.cpp` (every command through `encode_turn_entry -> decode_turn_entry -> World::apply`), `command_codec_test.cpp` (hand-written `0xF074` bytes, wrong lengths, `0xF075` is now the legacy neighbour),
  the 42 vectors of `cargo_load_vectors.txt` through the engine, `cargo_ref.py` (`check` reads both vector files, `selftest` fails when the load file is missing, six mutants including four hold-choice ones).
  The fresh-process replay fixtures `cargo_load.log` / `cargo_load_wrong.log` use `synthetic::make_cargo_types()` (the founding registry plus the navigating cargo ship 9510: four holds of 50 t); `anno-sim-replay` knows that registry.
* **Not covered here (L4/L5):** the real-file opening canary (found -> unload -> load back), the independent review, the Windows viewer.
