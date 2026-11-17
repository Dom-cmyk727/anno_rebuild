// UI-TABS-1 stage B: the balance pages' figures (balance_rows.hpp; docs/re/ui.md section 5.5).
#include <limits>

#include <anno/app/balance_rows.hpp>

namespace anno::app {
namespace {

using I64 = std::int64_t;
constexpr std::uint64_t kI64Max = static_cast<std::uint64_t>(std::numeric_limits<I64>::max());

/// `v` as an int64, or nullopt when it does not fit.
std::optional<I64> to_i64(std::uint64_t v) {
    if (v > kI64Max)
        return std::nullopt;
    return static_cast<I64>(v);
}

/// A statistic as the original's city getter shows it (AnnoWelt 0x1002dc70, indices 0, 4, 5:
/// `15 * x / 105`, unsigned, truncating). The original multiplies in 32 bits and wraps; the
/// rebuild's accounts are 64-bit, so a product that does not fit leaves the row empty instead.
std::optional<I64> shown_stat(std::uint64_t x, std::uint32_t num, std::uint32_t den) {
    if (den == 0)
        return std::nullopt;
    if (num != 0 && x > std::numeric_limits<std::uint64_t>::max() / num)
        return std::nullopt;
    return to_i64(x * num / den);
}

constexpr I64 kMax = std::numeric_limits<I64>::max();
constexpr I64 kMin = std::numeric_limits<I64>::min();

std::optional<I64> add(std::optional<I64> a, std::optional<I64> b) {
    if (!a || !b)
        return std::nullopt;
    if ((*b > 0 && *a > kMax - *b) || (*b < 0 && *a < kMin - *b))
        return std::nullopt;
    return *a + *b;
}

std::optional<I64> sub(std::optional<I64> a, std::optional<I64> b) {
    if (!a || !b)
        return std::nullopt;
    if ((*b < 0 && *a > kMax + *b) || (*b > 0 && *a < kMin + *b))
        return std::nullopt;
    return *a - *b;
}

void put(std::map<std::string, I64>& out, const char* key, std::optional<I64> v) {
    if (v)
        out[key] = *v;
}

} // namespace

std::map<std::string, std::int64_t> balance_row_values(const BalanceInputs& in) {
    std::map<std::string, I64> out;

    // The city page, View.dll 0x1006c0f0 (each row's getter call and the itoa that prints it):
    // row 0 city+0x5c -> +4 (0x1006c10c..0x1006c11e; AnnoWelt 0x1002dfb0 / 0x10022df0, the five
    // groups' residents = World::city_population); row 1 city+0x6c(0) (0x1006c1a7); row 2
    // +0x6c(1) + +0x6c(2) (0x1006c1e6..0x1006c1fc); row 3 +0x6c(4) (0x1006c234); row 4 +0x6c(5)
    // (0x1006c275); row 5 +0x6c(2) (0x1006c2b4); row 6 row4 - row5 - row3 - row2 + row1
    // (0x1006c2ef..0x1006c2fd); row 7 city+0x64 -> +0x20, / 256 (0x1006c145..0x1006c167).
    // AnnoWelt's city getter 0x1002dc70: index 0 +0x60c, 4 +0x5fc, 5 +0x600 shown as 15x/105;
    // 1 +0x604 and 2 +0x608 raw. The sim's names: +0x60c sales, +0x5fc trade_sales, +0x600
    // trade_purchases (scenario_formats.cpp, HEAD2 0x28/0x38/0x3c), +0x604 civil_cost, +0x608
    // military_cost (city_booking.cpp, kinds 1 and 2).
    if (in.city != nullptr) {
        put(out, "city.0", in.city->population);
        if (in.accounts_live) {
            const sim::CityAccounts& a = in.city->accounts;
            const auto row1 = shown_stat(a.sales, in.stat_num, in.stat_den);
            const auto civil = to_i64(a.civil_cost);
            const auto military = to_i64(a.military_cost);
            const auto row2 = add(civil, military);
            const auto row3 = shown_stat(a.trade_sales, in.stat_num, in.stat_den);
            const auto row4 = shown_stat(a.trade_purchases, in.stat_num, in.stat_den);
            const auto row5 = military;
            put(out, "city.1", row1);
            put(out, "city.2", row2);
            put(out, "city.3", row3);
            put(out, "city.4", row4);
            put(out, "city.5", row5);
            put(out, "city.6", add(sub(sub(sub(row4, row5), row3), row2), row1));
        }
    }

    // The player overview, View.dll 0x1007f8f0: over the player's cities (player +0x84(2), the
    // list loop 0x1007f977..0x1007f9ee) row 0 sums city+0x5c -> +4, row 1 city+0x6c(0), row 2
    // city+0x6c(1); rows 3, 4 and 5 add the player's own getter 0x1001c0f0 indices 4 (+0x80), 5
    // (+0x84) and 2 + 3 (+0x88, +0x8c) to the cities' 4, 5 and 2 (0x1007f921..0x1007f96d); row 6
    // = row 4 - row 3 - row 5 - row 2 + row 1 (0x1007f9f0..0x1007fa0e). Rows 3 .. 6 are not
    // computed here (balance_rows.hpp).
    std::optional<I64> population = I64{0};
    std::optional<I64> sales = I64{0};
    std::optional<I64> civil = I64{0};
    if (in.cities != nullptr) {
        for (const CityCounters& c : *in.cities) {
            if (!c.own) {
                continue; // UI-STATUS-1: another player's city the local player knows
            }
            population = add(population, c.population);
            if (in.accounts_live) {
                sales = add(sales, shown_stat(c.accounts.sales, in.stat_num, in.stat_den));
                civil = add(civil, to_i64(c.accounts.civil_cost));
            } else {
                sales.reset();
                civil.reset();
            }
        }
    }
    put(out, "player.0", population);
    put(out, "player.1", sales);
    put(out, "player.2", civil);
    return out;
}

std::string balance_text(std::int64_t value) {
    // The original's _itoa (View.dll 0x1008d919 -> 0x1008d946): radix 10 and a negative value
    // write '-' and then the digits of the negated value; no separator.
    std::uint64_t magnitude = value < 0 ? static_cast<std::uint64_t>(-(value + 1)) + 1
                                        : static_cast<std::uint64_t>(value);
    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + magnitude % 10));
        magnitude /= 10;
    } while (magnitude != 0);
    return value < 0 ? "-" + digits : digits;
}

} // namespace anno::app
