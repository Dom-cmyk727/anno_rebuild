// The resident/supplier transactions as pure, checked functions (MAP-POP-LIVE-1 L1;
// docs/sim/population-live.md section 2). House demand, the house's choice between the two goods
// of a supplier, the suppliers' availability and billing, the city billing record, the house's
// delivery and the walker's two-purchase arrival. No World, no PRNG, no I/O, nothing is hidden:
// every state a function may change is passed in explicitly (a ledger of spans, a stock span, a
// cargo span). The House tick does not call any of them yet (the House dispatch stays a no-op in
// this component).
//
// Evidence: D AnnoWelt.dll house demand 0x100765c0, house choice 0x10076670 with the IdSec helper
// 0x10076960, house delivery 0x10076610, walker purchase step B arrival 0x10056480, city billing
// 0x1002dac0. Units: amounts are 8.8 tons (256 = 1 t); a delivery adds amount * 16 to a house
// stock, so stock / 16 is in delivered units. Division truncates toward zero. All arithmetic is
// widened (int64/uint64) and checked: an unrepresentable result or a zero divisor is a typed
// ResidentFault, never a wrap, a clamp or "not chosen" (P-3).
//
// Independent reference: tools/resident_ref.py over fixtures/synthetic/sim/resident_vectors.txt.
#pragma once

#include <compare>
#include <cstdint>
#include <span>
#include <string>

#include <anno/sim/house_groups.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

/// One cargo slot of a resident walker: a goods id (0 = the slot is empty) and the amount in 8.8
/// tons (D: cargo +4 goods, +8 amount, vtable +8 = 0x10037570 returns the amount). A slot that
/// holds goods with amount 0 is NOT empty (a purchase that returned 0), and its goods is not
/// excluded from the next choice (the skip rule tests the amount first).
struct ResidentCargo {
    GoodsId goods = 0;
    std::uint32_t amount_88 = 0;
    friend constexpr auto operator<=>(const ResidentCargo&, const ResidentCargo&) = default;
};

/// Why a transaction was refused. Nothing is changed by a refused call, except that
/// resident_buy keeps what its earlier passes already did.
enum class ResidentFault : std::uint8_t {
    None,
    ChoiceDivisor,   // E * Menge == 0 for a candidate (or a member of its IdSec chain): P-3
    GoodsOutOfTable, // a goods id beyond a table the call needs (city store, billed)
    BilledOverflow,  // billed[g] + amount does not fit u32 (the original's u16 wraps; widened,
                     // refused)
    StockOverflow,   // a house stock would leave u32
    CargoOverflow,   // a cargo slot amount would leave u32
};

/// The stable text of a fault (the vector files and test messages).
[[nodiscard]] const char* resident_fault_name(ResidentFault f);

// ---- constants of the original's formulas -------------------------------------------------------
/// D 0x100765c0: demand = Menge * E * 256 / 44236 - stock / 16.
inline constexpr std::int64_t kDemandDivisor = 44236;
/// D 0x10076670: s = min(1536 * stock / (E * Menge), 160); urgency = 128 - s.
inline constexpr std::int64_t kUrgencyScale = 1536;
inline constexpr std::int64_t kUrgencySaturation = 160;
inline constexpr std::int64_t kUrgencyBase = 128;
/// D 0x10076670: a supplier's stock must exceed trunc(R * Menge * 256 / 4096), R = residents.
inline constexpr std::int64_t kThresholdNum = 256;
inline constexpr std::int64_t kThresholdDen = 4096;
/// D step B arrival 0x10056480: a choice score below this buys nothing (the callers' gate 0xc).
inline constexpr std::int64_t kPurchaseScoreGate = 12;
/// D delivery 0x10076610: a delivery adds amount * 16 to the stock.
inline constexpr std::uint64_t kStockPerDelivered = 16;
/// D: the constant availability of the infrastructure suppliers (PublicService, School, Doctor
/// 256; FireBrigade 512), 8.8 tons. Original behaviour defaults of those classes' slot +0xa4, not
/// limits of the engine.
inline constexpr std::uint64_t kServiceAvailability = 256;
inline constexpr std::uint64_t kFireBrigadeAvailability = 512;

/// The walker facts of a house TYPE (docs/sim/population-live.md section 1 step 3 and 10a): the
/// maximum number of walkers out at once (type +0xb8(0) = PROD3 byte 0x29), the number of figure
/// types of group 0 (+0xbc(0) = PROD3 byte 0x28, masked to a byte) and their FigurenHeader ids
/// (+0xb4(0, k) = the first `variants` of the nine u16 ids at PROD3 0x32; group 0 starts at the
/// list's start). All zero for a type without production data. A count of 0 means the house never
/// spawns; a maximum above 0 with no figure type is a registry error (house_walker_problem).
struct HouseWalkerFacts {
    std::uint32_t walker_max = 0;
    std::uint32_t variants = 0;
    std::span<const std::uint32_t> figure_ids;
};
[[nodiscard]] HouseWalkerFacts house_walker_facts(const BuildingType& type);
/// Empty when the facts of a House type are usable, else the reason (contract 10a, P): a nonzero
/// maximum with zero figure types, or more figure types than the type lists ids for.
[[nodiscard]] std::string house_walker_problem(const BuildingType& type);

/// D 0x100765c0, signed, in delivered units.
[[nodiscard]] std::int64_t house_demand(std::uint32_t menge, std::uint32_t e, std::uint32_t stock);

/// The house's side of a transaction: its group's needs, the goods -> need slot lookup (one entry
/// per goods id, need_count = none; HouseGroupDerived::lookup), the goods table (IdSec) and the
/// house's numbers. Views only.
struct HouseFacts {
    std::span<const HouseNeed> needs;
    std::span<const std::uint32_t> lookup;
    std::span<const GoodsInfo> goods;
    std::uint32_t e = 0;
    std::int32_t residents_64 = 0;
    std::span<const std::uint32_t> stock;
};

/// What a supplier offers: its two WARE goods, and per good the availability (8.8, the supplier's
/// +0xa4) and the enabled predicate (+0xc4). A goods id of 0 is no offer.
struct SupplierOffer {
    GoodsId goods[2] = {0, 0};
    std::uint64_t avail[2] = {0, 0};
    bool enabled[2] = {true, true};
};

struct ChoiceResult {
    /// The best urgency, or -1 when nothing was accepted.
    std::int64_t score = -1;
    /// The accepted goods (0 when none).
    GoodsId goods = 0;
    ResidentFault fault = ResidentFault::None;
    [[nodiscard]] bool chosen() const { return fault == ResidentFault::None && score >= 0; }
};

/// D 0x10076670 (docs/sim/population-live.md section 2, point 3). `carried` and `carried_more`
/// are the cargo slots of the house's walkers (the calling walker's included); a slot with a
/// non-zero amount zeroes the offered goods it holds (A and B tested independently; the original
/// text "A or B" is read as two independent tests, P). The IdSec chain is followed at most
/// goods.size() steps (a cycle is cut there; the original recurses unboundedly).
[[nodiscard]] ChoiceResult house_choice(const HouseFacts& house, const SupplierOffer& offer,
                                        std::span<const ResidentCargo> carried,
                                        std::span<const ResidentCargo> carried_more = {});

/// What a supplier is, for availability and billing: keyed by the runtime class of its type (data
/// from the registry), never by a shipped id.
struct SupplierModel {
    RuntimeClass cls = RuntimeClass::Plain;
    /// The type's two WARE goods.
    GoodsId out[2] = {0, 0};
    /// MarketStall: flags +0x18/+0x19 per output slot (default 1; nobody flips them yet, U).
    bool flag[2] = {true, true};
    /// Inn: the tavern stock (u16 +0x18, widened), taken by a bill.
    std::uint32_t tavern_stock = 0;
    /// Inn: the input good its bill records.
    GoodsId inn_input = 0;
};

struct AvailResult {
    std::uint64_t amount = 0;
    ResidentFault fault = ResidentFault::None;
};

/// D slot +0xa4: MarketStall the full city store of g (no reservation, no range check;
/// GoodsOutOfTable beyond the store); PublicService, School, Doctor 256; FireBrigade 512; Inn the
/// tavern stock; every other class 0.
[[nodiscard]] AvailResult supplier_available(const SupplierModel& s,
                                             std::span<const std::uint32_t> city_store, GoodsId g);
/// D slot +0xc4: MarketStall reads `flag[out0 != g]` (output 0 its flag +0x18, every other goods
/// flag +0x19; the ALL arm 0x8e and the Castle flag +0x22 are not modelled); every other class
/// true.
[[nodiscard]] bool supplier_enabled(const SupplierModel& s, GoodsId g);

/// The city's consumer-goods record: the store (8.8), the billed counter, and the goods range that
/// is billed (D 0x1002dac0 tests 0x1a..0x28; configuration, SimConfig::billed_goods_first/last).
/// The member defaults are those original values; a caller with a World passes the SimConfig's.
struct BillLedger {
    std::span<std::uint32_t> store;
    std::span<std::uint32_t> billed;
    GoodsId first = 0x1a;
    GoodsId last = 0x28;
};

struct BillResult {
    std::uint32_t returned = 0;
    ResidentFault fault = ResidentFault::None;
};

/// D city +0x88 = 0x1002dac0: outside [first, last] returns `amount` and changes nothing; else
/// billed[g] += amount (the REQUESTED amount, checked u32), take == false returns amount, else
/// t = min(amount, store[g]), store -= t, returns t. A refused call changes nothing.
[[nodiscard]] BillResult bill(BillLedger& ledger, GoodsId g, std::uint32_t amount, bool take);

/// The supplier wrappers: MarketStall take = 1; PublicService and School take = 0; Doctor and
/// FireBrigade return 0 and change nothing; Inn (D 0x100756e0) takes t = min(tavern, amount) from
/// its own stock and bills its input good for the CLIPPED amount t with take = 0 (even t = 0),
/// returning t (its taken-stat +0x1a is not modelled, U); any other class returns 0.
[[nodiscard]] BillResult supplier_bill(SupplierModel& s, BillLedger& ledger, GoodsId g,
                                       std::uint32_t amount);

struct DeliverResult {
    /// Need slots whose stock was raised (0 for a goods the house does not need).
    std::uint32_t touched = 0;
    ResidentFault fault = ResidentFault::None;
};

/// D 0x10076610: slot = lookup[g]; none -> nothing. Else stock[slot] += amount * 16 and the IdSec
/// replay, which is a RECURSIVE deliver of the substitute (house vtable slot 0xc0 is this
/// function): each substitute with a need slot is raised and replays its own IdSec; a substitute
/// without a slot ends the chain (g 26 -> 27 -> 28 raises three slots, touched = 3). The original
/// recurses unboundedly; P-3: at most goods.size() replays, so a cycle or self-loop is cut there.
/// Every add of the chain is checked against u32 first; any overflow refuses the whole call.
[[nodiscard]] DeliverResult house_deliver(const HouseFacts& house, std::span<std::uint32_t> stock,
                                          GoodsId g, std::uint32_t amount);

struct BuyResult {
    std::uint32_t purchases = 0;
    ResidentFault fault = ResidentFault::None;
};

/// D step B arrival 0x10056480 (section 2, "Walker purchase"): the first empty cargo slot, a
/// choice, `amt = min(avail, demand, capacity)` (signed), a bill, the slot's goods set and its
/// amount raised by the returned amount; a second pass only if the next slot is empty; never a
/// third. `others` is the cargo of the house's other walkers. A bill or cargo overflow stops the
/// call and leaves the cargo as the failing step found it.
[[nodiscard]] BuyResult resident_buy(const HouseFacts& house, SupplierModel& supplier,
                                     BillLedger& ledger, std::uint32_t capacity_88,
                                     std::span<const ResidentCargo> others,
                                     std::span<ResidentCargo> cargo);

} // namespace anno::sim
