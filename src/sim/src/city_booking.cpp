#include <algorithm>
#include <limits>
#include <type_traits>

#include <anno/sim/building.hpp>
#include <anno/sim/city_booking.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {
namespace {
constexpr auto kMax = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint64_t kBias = std::uint64_t{1} << 63;
// Ordered representation of the entire i64 domain, without signed overflow,
// implementation-defined narrowing or a nonportable __int128 dependency.
std::uint64_t money_offset(std::int64_t n) {
    return n < 0 ? static_cast<std::uint64_t>(n - std::numeric_limits<std::int64_t>::min())
                 : kBias + static_cast<std::uint64_t>(n);
}
std::int64_t money_from_offset(std::uint64_t n) {
    return n < kBias ? std::numeric_limits<std::int64_t>::min() + static_cast<std::int64_t>(n)
                     : static_cast<std::int64_t>(n - kBias);
}
bool decay(std::uint64_t n, std::uint32_t num, std::uint32_t den, std::uint64_t& out) {
    const auto q = n / den;
    if (num != 0 && q > kMax / num)
        return false;
    const auto whole = q * num;
    const auto fraction = (n % den) * num / den; // two u32-sized factors fit u64
    if (fraction > kMax - whole)
        return false;
    out = whole + fraction;
    return true;
}
} // namespace

const char* accounting_fault_name(AccountingFault fault) {
    switch (fault) {
    case AccountingFault::None:
        return "none";
    case AccountingFault::Product:
        return "product";
    case AccountingFault::Sum:
        return "sum";
    case AccountingFault::Rounding:
        return "rounding";
    case AccountingFault::CostSum:
        return "cost_sum";
    case AccountingFault::Credit:
        return "credit";
    case AccountingFault::Debit:
        return "debit";
    case AccountingFault::StatDecay:
        return "stat_decay";
    case AccountingFault::Sales:
        return "sales";
    case AccountingFault::Configuration:
        return "configuration";
    case AccountingFault::City:
        return "city";
    case AccountingFault::Owner:
        return "owner";
    case AccountingFault::PriceDimensions:
        return "price_dimensions";
    case AccountingFault::FinancialState:
        return "financial_state";
    case AccountingFault::CostUnderflow:
        return "cost_underflow";
    case AccountingFault::CostOverflow:
        return "cost_overflow";
    case AccountingFault::Type:
        return "type";
    }
    return "unknown";
}

BookingResult calculate_city_booking(const SimConfig& cfg, std::uint32_t tick,
                                     const CityAccounts& before, std::span<const BookingRow> rows,
                                     std::int64_t money) {
    BookingResult out;
    out.money = money;
    out.accounts = before;
    const auto fail = [&](AccountingFault fault) {
        out.fault = fault;
        return out;
    };
    if (cfg.economy.period == 0 || cfg.economy.period > 65535 || cfg.upkeep_slices == 0 ||
        cfg.stat_decay_den == 0)
        return fail(AccountingFault::Configuration);
    if (static_cast<std::uint16_t>(tick - before.stamp) < cfg.economy.period)
        return out;
    std::uint64_t total = 0;
    for (const auto& row : rows) {
        ++out.rows;
        const auto price = std::uint64_t{row.price} + row.surcharge;
        if (row.billed != 0 && price > kMax / row.billed)
            return fail(AccountingFault::Product);
        const auto product = price * row.billed;
        if (product > kMax - total)
            return fail(AccountingFault::Sum);
        total += product;
    }
    if (total > kMax - 128)
        return fail(AccountingFault::Rounding);
    const auto income = (total + 128) / 256;
    if (before.military_cost > kMax - before.civil_cost)
        return fail(AccountingFault::CostSum);
    const auto upkeep = (before.civil_cost + before.military_cost) / cfg.upkeep_slices;
    auto offset = money_offset(money);
    if (income > kMax - offset)
        return fail(AccountingFault::Credit);
    offset += income; // gross credit, not netted against the later debit
    if (upkeep > offset)
        return fail(AccountingFault::Debit);
    offset -= upkeep;
    CityAccounts next = before;
    if (!decay(before.sales, cfg.stat_decay_num, cfg.stat_decay_den, next.sales) ||
        !decay(before.trade_sales, cfg.stat_decay_num, cfg.stat_decay_den, next.trade_sales) ||
        !decay(before.trade_purchases, cfg.stat_decay_num, cfg.stat_decay_den,
               next.trade_purchases))
        return fail(AccountingFault::StatDecay);
    if (income > kMax - next.sales)
        return fail(AccountingFault::Sales);
    next.sales += income;
    next.stamp = static_cast<std::uint16_t>(tick);
    out.accounts = next;
    out.money = money_from_offset(offset);
    out.income = income;
    out.upkeep = upkeep;
    out.stepped = true;
    return out;
}

formats::Result<BuildingCost, AccountingFault> building_cost(const TypeRegistry& types,
                                                             const BuildingType& type,
                                                             const Building& building,
                                                             const Player* owner) {
    if ((type.statistic_kind != 1 && type.statistic_kind != 2) ||
        (!type.running_cost.empty() && type.running_cost.size() != 2))
        return AccountingFault::Type;
    BuildingCost out;
    out.kind = type.statistic_kind;
    AccountingFault fault = AccountingFault::None;
    out.active = std::visit(
        [&](const auto& p) -> bool {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, Workshop> || std::is_same_v<T, Farm> ||
                          std::is_same_v<T, Mine>) {
                ++out.lines;
                return p.switched_on;
            } else if constexpr (std::is_same_v<T, SmallFarm>) {
                for (const auto on : p.line_on) {
                    ++out.lines;
                    if (on)
                        return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, WeaponsWorkshop>) {
                if (p.line_on.size() != type.output_goods.size()) {
                    fault = AccountingFault::Type;
                    return false;
                }
                for (std::size_t i = 0; i < p.line_on.size(); ++i) {
                    ++out.lines;
                    if (!p.line_on[i])
                        continue;
                    const auto* g = types.goods(type.output_goods[i]);
                    if (g == nullptr) {
                        fault = AccountingFault::Type;
                        return false;
                    }
                    if (g->forsch == 0)
                        return true;
                    if (owner == nullptr) {
                        fault = AccountingFault::Owner;
                        return false;
                    }
                    if (owner->research.researched(g->forsch))
                        return true;
                }
                return false;
            } else if constexpr (std::is_same_v<T, Inert>) {
                if (!p.financial_active) {
                    fault = AccountingFault::FinancialState;
                    return false;
                }
                return *p.financial_active;
            } else
                return true; // Original constant property8e for House/Market/Inn.
        },
        building.payload);
    if (fault != AccountingFault::None)
        return fault;
    out.amount = type.running_cost.empty() ? 0 : type.running_cost[out.active ? 1 : 0];
    return out;
}

formats::Result<CityAccounts, AccountingFault>
change_city_cost(const CityAccounts& before, const BuildingCost& contribution, bool add) {
    if (contribution.kind != 1 && contribution.kind != 2)
        return AccountingFault::Type;
    CityAccounts next = before;
    auto& bucket = contribution.kind == 1 ? next.civil_cost : next.military_cost;
    if (add) {
        if (contribution.amount > kMax - bucket)
            return AccountingFault::CostOverflow;
        bucket += contribution.amount;
    } else {
        if (contribution.amount > bucket)
            return AccountingFault::CostUnderflow;
        bucket -= contribution.amount;
    }
    return next;
}
} // namespace anno::sim
