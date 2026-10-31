#include <algorithm>
#include <limits>
#include <utility>

#include <anno/sim/founding_payment.hpp>

namespace anno::sim::payment {

namespace {

using Need = std::pair<GoodsId, std::uint64_t>;

/// a + b into out; false when the sum does not fit 64 bits.
bool checked_add(std::uint64_t a, std::uint64_t b, std::uint64_t& out) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        return false;
    }
    out = a + b;
    return true;
}

} // namespace

formats::Result<Plan, Refusal> prepare(std::span<const ShipHold> holds, std::int64_t money,
                                       const Costs& costs) {
    // 1. The requirement per goods: zero amounts dropped, one goods summed, ascending goods id.
    std::vector<Need> needs;
    needs.reserve(costs.goods.size());
    for (const CostEntry& c : costs.goods) {
        if (c.amount_88 != 0) {
            needs.emplace_back(c.goods, c.amount_88);
        }
    }
    std::sort(needs.begin(), needs.end(),
              [](const Need& a, const Need& b) { return a.first < b.first; });
    std::vector<Need> merged;
    for (const Need& n : needs) {
        if (!merged.empty() && merged.back().first == n.first) {
            if (!checked_add(merged.back().second, n.second, merged.back().second)) {
                return Refusal{Status::Overflow, n.first, 0, 0};
            }
        } else {
            merged.push_back(n);
        }
    }

    // 2. Affordability: every goods' total over all holds (duplicates add), then the gold.
    for (const Need& n : merged) {
        std::uint64_t total = 0;
        if (n.first != 0) { // goods id 0 is "none" and supplies nothing
            for (const ShipHold& h : holds) {
                if (h.goods == n.first && !checked_add(total, h.amount_88, total)) {
                    return Refusal{Status::Overflow, n.first, n.second, 0};
                }
            }
        }
        if (total < n.second) {
            return Refusal{Status::InsufficientGoods, n.first, n.second, total};
        }
    }
    if (costs.gold != 0 && money < static_cast<std::int64_t>(costs.gold)) {
        return Refusal{Status::InsufficientGold, 0, costs.gold,
                       money < 0 ? 0 : static_cast<std::uint64_t>(money)};
    }

    // 3. The proposed result: gold once, then the holds consumed goods by goods, index by index.
    Plan plan;
    plan.holds.assign(holds.begin(), holds.end());
    plan.money_after = money - static_cast<std::int64_t>(costs.gold); // money >= gold >= 0 here
    plan.gold_charged = costs.gold;
    for (const Need& n : merged) {
        std::uint64_t remaining = n.second;
        for (ShipHold& h : plan.holds) {
            if (remaining == 0) {
                break;
            }
            if (h.goods != n.first) {
                continue;
            }
            const std::uint64_t take = std::min(h.amount_88, remaining);
            if (take >=
                h.amount_88) { // AnnoWelt 0x1003b9a0: emptied (also 0 >= 0): amount 0, goods 0
                h.amount_88 = 0;
                h.goods = 0;
            } else {
                h.amount_88 -= take;
            }
            remaining -= take;
        }
    }
    return plan;
}

formats::Result<Plan, Refusal> prepare(std::span<const ShipHold> holds, std::int64_t money,
                                       const BuildingType& type) {
    return prepare(holds, money, Costs{type.cost_gold, type.cost_goods});
}

} // namespace anno::sim::payment
