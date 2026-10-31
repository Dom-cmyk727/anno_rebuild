// Founding payment preparation (task MAP-2 package C; research: docs/re/ship-founding.md
// section 10.7, contract: docs/sim/ship-founding.md section 5).
//
// A pure function: from ordered ship holds, the payer's gold and a type's costs it either refuses
// with a reason or returns the complete proposed result (the holds and the gold after payment). It
// reads nothing but its arguments and mutates nothing: not the World, the holds it was given, the
// gold, the PRNG, a city, a figure, a tile or a counter. Applying a plan, the all-or-nothing
// atomicity of a command and the order against the city ring are future work; this is not a
// founding command and not a general transaction framework.
//
// Rules (ORIGINAL unless marked; AnnoGame 0x10041760 for the loop, AnnoWelt 0x1003b9a0 for the hold
// slot +0x10, re section 10.7):
//  - costs come from the type data (gold, goods amounts in raw 1/256 ton); nothing is special-cased
//    for warehouse 1302;
//  - goods are processed in ascending goods id; holds in ascending index; a hold visited with
//    need remaining gives take = min(amount, remaining);
//  - a hold whose take is at least its amount ends with amount 0 AND goods id 0 (cleared). This
//    includes a matching hold that was already empty and is visited while need remains (the slot
//    compares take >= amount with 0 >= 0); holds not visited (need already met, other goods) are
//    untouched;
//  - affordability sums all holds of a goods first (duplicates add); gold is checked only when the
//    cost is non-zero and is charged once, without a floor beyond that check;
//  - ADAPTATION: several cost entries of one goods are summed (checked) before use; goods id 0 is
//    "none" (UNUSED, the cleared marker) and never supplies cargo; a sum that does not fit 64 bits
//    is the explicit refusal Overflow, never a wrap; goods are checked before gold.
// Cost: O(c log c + c * h + h) for c cost entries and h holds; no allocation beyond the result.
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim::payment {

/// The costs of one placement: gold and goods amounts in raw 1/256 ton (BuildingType::cost_gold,
/// cost_goods). The goods list may be in any order and may repeat a goods.
struct Costs {
    std::uint32_t gold = 0;
    std::span<const CostEntry> goods;
};

enum class Status : std::uint8_t {
    InsufficientGoods, ///< the holds' total of `goods` is below the requirement
    InsufficientGold,  ///< the payer's gold is below the (non-zero) gold cost
    Overflow,          ///< a total does not fit 64 bits
};

struct Refusal {
    Status status = Status::InsufficientGoods;
    GoodsId goods = 0;           ///< for InsufficientGoods / goods Overflow
    std::uint64_t required = 0;  ///< raw units (goods) or gold
    std::uint64_t available = 0; ///< raw units held (goods) or the gold, floored at 0
    friend bool operator==(const Refusal&, const Refusal&) = default;
};

/// The complete proposed result of a payment.
struct Plan {
    std::vector<ShipHold> holds; ///< the holds after payment, same length and order
    std::int64_t money_after = 0;
    std::uint64_t gold_charged = 0;
    friend bool operator==(const Plan&, const Plan&) = default;
};

[[nodiscard]] formats::Result<Plan, Refusal> prepare(std::span<const ShipHold> holds,
                                                     std::int64_t money, const Costs& costs);

/// The costs of a registry type.
[[nodiscard]] formats::Result<Plan, Refusal> prepare(std::span<const ShipHold> holds,
                                                     std::int64_t money, const BuildingType& type);

} // namespace anno::sim::payment
