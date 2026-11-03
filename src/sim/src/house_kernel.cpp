// The pure House calculation (header: house_kernel.hpp). Written from the instruction spans listed
// there; intermediate values are 64-bit and every output is range-checked before it is stored.
#include <algorithm>
#include <limits>

#include <anno/sim/house_kernel.hpp>

namespace anno::sim {

namespace {

using i64 = std::int64_t;

constexpr i64 kLowestStart = 0x7fffffff; // movl $0x7fffffff, 0x14(%esp)

HouseKernelResult fail(HouseKernelResult r, NotEvaluatedReason why) {
    r.status = HouseKernelStatus::NotEvaluated;
    r.reason = why;
    return r;
}

/// 1536 * stock / (e * menge), truncating; the caller guarantees a non-zero divisor.
i64 score(i64 stock, i64 e, i64 menge, i64 cap) {
    return std::min<i64>((1536 * stock) / (e * menge), cap);
}

} // namespace

int house_mood_combine(int f, int b, int i, std::uint8_t flags) {
    if (f < 0 || b < 0 || i < 0) {
        if ((flags & 0x20U) != 0) {
            return -2;
        }
        return std::min({f, b, i});
    }
    if ((flags & 0x30U) != 0) {
        return (flags & 0x20U) != 0 ? -2 : -1;
    }
    return (f + b + i + 1) / 3;
}

HouseKernelResult house_kernel(const HouseKernelInputs& in) {
    HouseKernelResult r;
    // 1. The gate: an unsigned 16-bit difference (AnnoWelt 0x1007624b), one step per call.
    if (in.gate_period < 1 || in.gate_period > 0xffffU) {
        return fail(r, NotEvaluatedReason::GatePeriodOutOfDomain);
    }
    const auto behind = static_cast<std::uint16_t>(in.now - in.last);
    if (behind < in.gate_period) {
        r.status = HouseKernelStatus::Skipped;
        r.last = in.last;
        return r;
    }
    r.status = HouseKernelStatus::Evaluated;
    r.last = static_cast<std::uint16_t>(in.last + in.gate_period);
    if (in.view == nullptr || in.view->group == nullptr || in.view->derived == nullptr) {
        return fail(r, NotEvaluatedReason::MissingGroup);
    }
    const HouseGroup& group = *in.view->group;
    const HouseGroupDerived& derived = *in.view->derived;
    const std::span<const GoodsInfo> goods = in.view->goods;
    const std::size_t n = group.needs.size();
    if (in.stock.size() != n) {
        return fail(r, NotEvaluatedReason::StockSizeMismatch);
    }
    // 2. E' = trunc(127 E / 128) + trunc(res / 64) (0x10076286-0x100762a5).
    const i64 e1 = (127 * static_cast<i64>(in.e)) / 128 + static_cast<i64>(in.residents_64) / 64;
    if (e1 < 0) {
        return fail(r, NotEvaluatedReason::NegativeSmoothedE);
    }
    r.e = static_cast<std::uint32_t>(e1); // e1 < 2^32 for every u32 E and i32 residents
    // 3. The need loop: only stage-enabled needs decay and count (0x100762c2-0x1007633f).
    std::vector<std::uint32_t> stock(in.stock.begin(), in.stock.end());
    i64 lowest = kLowestStart;
    for (std::size_t j = 0; j < n; ++j) {
        const HouseNeed& need = group.needs[j];
        const std::uint32_t stage = need.ware < goods.size() ? goods[need.ware].bau_first : 0;
        if (stage >= 32) {
            return fail(r, NotEvaluatedReason::StageIndexOutOfDomain);
        }
        if (((in.player_mask >> stage) & 1U) == 0) {
            continue;
        }
        stock[j] = static_cast<std::uint32_t>((127 * static_cast<i64>(stock[j])) / 128);
        if (e1 * need.menge == 0) {
            r.stock = stock;
            return fail(r, NotEvaluatedReason::ZeroDivisor);
        }
        lowest = std::min(lowest, score(stock[j], e1, need.menge, 160));
    }
    r.stock = stock;
    r.lowest = lowest;
    // 4. The spawn condition (0x10076341-0x10076371): a boolean, never a spawn.
    if (in.walkers.has_value()) {
        r.spawn_condition = lowest < 102 && in.walkers->type_max > in.walkers->count;
    }
    // 5. The mood (0x10075fd0): E' == 0 returns 0 before any table is read.
    int mood = 0;
    if (e1 != 0) {
        if (n == 0) {
            return fail(r, NotEvaluatedReason::MissingFirstNeed);
        }
        i64 total = 0, count = 0, low_i = 160;
        for (std::uint32_t j = 1; j < derived.goods_end && j < n; ++j) {
            if (group.needs[j].force) {
                total += score(stock[j], e1, group.needs[j].menge, 128);
                ++count;
            }
        }
        for (std::uint32_t j = derived.goods_end; j < derived.infra_end && j < n; ++j) {
            const HouseNeed& need = group.needs[j];
            if (need.force) {
                low_i = std::min(low_i, score(stock[j], e1, need.menge, 160));
                if (need.ware == in.view->roles.tavern_goods) {
                    total += score(stock[j], e1, need.menge, 128);
                    ++count;
                }
            }
        }
        const i64 s0 = score(stock[0], e1, group.needs[0].menge, 128); // slot 0 is positional
        const i64 avg = count == 0 ? 128 : total / count;
        const int f = group.nahrung[static_cast<std::size_t>((s0 + 4) / 8)];
        const int b = group.bedarf[static_cast<std::size_t>((avg + 4) / 8)];
        const int i = group.infra[static_cast<std::size_t>((low_i + 4) / 8)];
        mood = house_mood_combine(f, b, i, in.mood_flags);
    }
    r.mood = mood;
    if (mood < -2 || mood > 2) {
        return fail(r, NotEvaluatedReason::MoodOutOfRange);
    }
    // 6. Growth (0x100763f3-0x100765af).
    i64 res = in.residents_64;
    r.counter_sub = res / 64;
    res += group.vermehr[static_cast<std::size_t>(mood + 2)];
    if (res < std::numeric_limits<std::int32_t>::min() ||
        res > std::numeric_limits<std::int32_t>::max()) {
        return fail(r, NotEvaluatedReason::Overflow);
    }
    r.counter_add = res / 64;
    const i64 cap = static_cast<i64>(group.wohnmax) * 64;
    if (mood >= 0) {
        r.cap_crossed = res > cap; // signed, strict (0x10076468)
        if (res > cap) {
            r.cap_sub = res / 64 - static_cast<i64>(group.wohnmax);
            res = cap;
            r.effect = DeferredEffect::UpgradeCheck;
        }
    } else {
        const bool remove_floor = group.no_downgrade || (in.mood_flags & 0x30U) != 0;
        i64 floor = 64;
        if (!remove_floor) {
            if (!in.previous_wohnmax.has_value()) {
                return fail(r, NotEvaluatedReason::PreviousCapacityUnavailable);
            }
            floor = 48 * static_cast<i64>(*in.previous_wohnmax);
        }
        if (res < floor) {
            if (remove_floor) {
                r.arm = HouseArm::Remove; // bypasses the correction (0x10076547-0x10076551)
                r.effect = DeferredEffect::Remove;
            } else {
                r.arm = HouseArm::Downgrade;
                r.effect = DeferredEffect::Downgrade;
                if (res < 64) { // signed (0x1007654b)
                    r.below64_add = res / 64 - 64;
                    res = 64;
                }
            }
        }
    }
    r.residents_64 = static_cast<std::int32_t>(res);
    return r;
}

} // namespace anno::sim
