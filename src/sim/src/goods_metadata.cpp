// The [WAREN] metadata reader (header: goods_metadata.hpp).
#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <optional>

#include <anno/formats/text.hpp>
#include <anno/sim/goods_metadata.hpp>

namespace anno::sim {

namespace {

namespace text = formats::text;

// D: AnnoGame 0x10024bd0 entries 0..31, read from the module (population-inputs.md 2.2).
constexpr std::array<std::string_view, kStageIndexCount> kStageNames = {
    "INFRA_EVER",     "INFRA_STUFE_1A",      "INFRA_STUFE_1B",      "INFRA_STUFE_1C",
    "INFRA_STUFE_2A", "INFRA_STUFE_2B",      "INFRA_STUFE_2C",      "INFRA_STUFE_2D",
    "INFRA_STUFE_2E", "INFRA_STUFE_2F",      "INFRA_STUFE_2G",      "INFRA_STUFE_3A",
    "INFRA_STUFE_3B", "INFRA_STUFE_3C",      "INFRA_STUFE_3D",      "INFRA_STUFE_3E",
    "INFRA_STUFE_3F", "INFRA_STUFE_4A",      "INFRA_STUFE_4B",      "INFRA_STUFE_4C",
    "INFRA_STUFE_4D", "INFRA_STUFE_4E",      "INFRA_STUFE_4F",      "INFRA_STUFE_5A",
    "INFRA_STUFE_5B", "INFRA_STUFE_5C",      "INFRA_STUFE_5D",      "INFRA_STUFE_5E",
    "INFRA_STUFE_5F", "INFRA_STUFE_TRIUMPH", "INFRA_STUFE_DENKMAL", "INFRA_STUFE_FORSCH"};

std::string str(formats::Bytes b) {
    return std::string(b.begin(), b.end());
}

std::string at(const text::Entry& e) {
    return "BGruppen.dat [WAREN] line " + std::to_string(e.line) + ": ";
}

std::optional<std::size_t> goods_named(formats::Bytes name, std::span<const GoodsInfo> goods) {
    for (std::size_t i = 0; i < goods.size(); ++i) {
        if (text::equals(name, goods[i].name)) {
            return i;
        }
    }
    return std::nullopt;
}

// The general text reader intentionally accepts only18 digits. Economy's
// reviewed signed-i64 input domain includes both19-digit endpoints; keep this
// bounded parser local rather than changing every existing metadata reader.
std::optional<std::int64_t> price_number(formats::Bytes text) {
    const bool negative = !text.empty() && text.front() == '-';
    if (negative)
        text = text.subspan(1);
    if (text.empty())
        return std::nullopt;
    const std::uint64_t limit = negative ? (std::uint64_t{1} << 63)
                                         : std::uint64_t(std::numeric_limits<std::int64_t>::max());
    std::uint64_t value = 0;
    for (const auto c : text) {
        if (c < '0' || c > '9')
            return std::nullopt;
        const auto digit = std::uint64_t(c - '0');
        if (value > (limit - digit) / 10)
            return std::nullopt;
        value = value * 10 + digit;
    }
    if (negative && value == (std::uint64_t{1} << 63))
        return std::numeric_limits<std::int64_t>::min();
    return negative ? -static_cast<std::int64_t>(value) : static_cast<std::int64_t>(value);
}

std::optional<std::vector<std::int64_t>> price_numbers(formats::Bytes text) {
    const auto separator = [](auto c) { return c == ',' || c == ' ' || c == '\t'; };
    std::vector<std::int64_t> out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (separator(text[i])) {
            ++i;
            continue;
        }
        const auto start = i;
        while (i < text.size() && !separator(text[i]))
            ++i;
        const auto value = price_number(text.subspan(start, i - start));
        if (!value)
            return std::nullopt;
        out.push_back(*value);
    }
    return out;
}

} // namespace

std::optional<std::uint32_t> stage_index_of(std::string_view name) {
    for (std::uint32_t i = 0; i < kStageNames.size(); ++i) {
        if (name == kStageNames[i]) {
            return i;
        }
    }
    return std::nullopt;
}

std::string apply_goods_metadata(formats::Bytes file, std::span<GoodsInfo> destination,
                                 const EconomyConfig& economy) {
    if (auto problem = economy.validate(); !problem.empty())
        return problem;
    std::vector<GoodsInfo> goods(destination.begin(), destination.end());
    struct SignedPrices {
        std::vector<std::int64_t> climate, tax;
    };
    std::map<std::size_t, SignedPrices> pending;
    for (const auto& g : goods) {
        if (g.prices && (g.prices->climate.size() != economy.climate_names.size() ||
                         g.prices->tax.size() != economy.tax_count))
            return "BGruppen.dat [WAREN] prices.climate/tax: existing dimensions differ from "
                   "economy";
    }
    const auto price_of = [&](std::size_t id) -> SignedPrices& {
        auto [it, inserted] = pending.try_emplace(id);
        if (inserted) {
            it->second.climate.assign(economy.climate_names.size(), 0);
            it->second.tax.assign(economy.tax_count, 0);
            if (goods[id].prices) {
                const auto& p = *goods[id].prices;
                it->second.climate.assign(p.climate.begin(), p.climate.end());
                it->second.tax.assign(p.tax.begin(), p.tax.end());
            }
        }
        return it->second;
    };
    const auto entries = text::section_entries(file, "WAREN");
    const auto [head, records] = text::split_records(entries, "Id");
    for (const auto& r : records) {
        const text::Entry& first = r.front();
        const std::optional<std::size_t> id = goods_named(first.value, goods);
        if (!id) {
            return at(first) + "Id '" + str(first.value) + "' is not in the goods table";
        }
        for (const text::Entry& e : r) {
            if (text::equals(e.key, "BauFirst")) {
                const std::optional<std::uint32_t> stage = stage_index_of(str(e.value));
                if (!stage) {
                    return at(e) + "BauFirst '" + str(e.value) +
                           "' is not a stage name (INFRA_EVER .. INFRA_STUFE_FORSCH)";
                }
                goods[*id].bau_first = *stage;
            } else if (text::equals(e.key, "IdSec")) {
                const std::optional<std::size_t> sec = goods_named(e.value, goods);
                if (!sec) {
                    return at(e) + "IdSec '" + str(e.value) + "' is not in the goods table";
                }
                goods[*id].id_sec = static_cast<GoodsId>(*sec);
            } else if (text::equals(e.key, "Preis")) {
                const auto value = price_number(e.value);
                if (!value)
                    return at(e) + "Preis: expected signed i64";
                auto& p = price_of(*id);
                std::fill(p.climate.begin(), p.climate.end(), *value);
            } else if (text::equals(e.key, "PreisLevel")) {
                const auto values = price_numbers(e.value);
                if (!values)
                    return at(e) + "PreisLevel: expected signed i64 columns";
                if (values->size() > economy.tax_count)
                    return at(e) + "PreisLevel.columns: exceeds economy.tax_count";
                if (!values->empty()) {
                    auto& p = price_of(*id);
                    std::copy(values->begin(), values->end(), p.tax.begin());
                }
            } else if (text::equals(e.key, "Bonus") || text::equals(e.key, "Malus")) {
                const auto fields = text::fields(e.value);
                if (fields.size() != 2)
                    return at(e) + "Bonus/Malus: needs amount, climate|climate";
                const auto amount = price_number(fields[0]);
                if (!amount)
                    return at(e) + "Bonus/Malus: expected signed i64 amount";
                const bool malus = text::equals(e.key, "Malus");
                if (malus && *amount == std::numeric_limits<std::int64_t>::min())
                    return at(e) + "Malus.negation: outside i64";
                const std::int64_t delta = malus ? -*amount : *amount;
                auto& p = price_of(*id);
                formats::Bytes names = fields[1];
                while (true) {
                    const auto bar = std::find(names.begin(), names.end(), std::uint8_t{'|'});
                    auto name = names.first(static_cast<std::size_t>(bar - names.begin()));
                    while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
                        name = name.subspan(1);
                    while (!name.empty() && (name.back() == ' ' || name.back() == '\t'))
                        name = name.first(name.size() - 1);
                    std::optional<std::size_t> climate;
                    for (std::size_t i = 0; i < economy.climate_names.size(); ++i)
                        if (text::equals(name, economy.climate_names[i])) {
                            climate = i;
                            break;
                        }
                    if (!climate)
                        return at(e) + "Bonus/Malus.climate: unknown '" + str(name) + "'";
                    auto& value = p.climate[*climate];
                    if ((delta > 0 && value > std::numeric_limits<std::int64_t>::max() - delta) ||
                        (delta < 0 && value < std::numeric_limits<std::int64_t>::min() - delta))
                        return at(e) + "Bonus/Malus.price_accumulator: outside i64";
                    value += delta;
                    if (bar == names.end())
                        break;
                    names = names.subspan(static_cast<std::size_t>(bar - names.begin()) + 1);
                }
            }
        }
    }
    for (const auto& [id, raw] : pending) {
        GoodsPrices p;
        const auto resolve = [&](const auto& values, auto& out, const char* field) -> std::string {
            for (const auto value : values) {
                if (value < 0 || std::uint64_t(value) > std::numeric_limits<std::uint32_t>::max())
                    return "BGruppen.dat [WAREN] " + goods[id].name + "." + field +
                           ": outside nonnegative u32";
                out.push_back(static_cast<std::uint32_t>(value));
            }
            return {};
        };
        if (auto problem = resolve(raw.climate, p.climate, "resolved_price"); !problem.empty())
            return problem;
        if (auto problem = resolve(raw.tax, p.tax, "resolved_surcharge"); !problem.empty())
            return problem;
        goods[id].prices = std::move(p);
    }
    std::copy(goods.begin(), goods.end(), destination.begin());
    return {};
}

formats::Result<std::vector<StageRecord>, std::string> parse_stage_table(formats::Bytes file) {
    const auto where = [](const text::Entry& e) {
        return "BGruppen.dat [BAUSTUFEN] line " + std::to_string(e.line) + ": ";
    };
    const auto number = [](formats::Bytes field) -> std::optional<std::uint32_t> {
        const auto v = text::to_int(field);
        if (!v || *v < 0 || *v > 0xffffffffLL) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(*v);
    };
    std::vector<StageRecord> out;
    std::vector<std::uint8_t> have_wissen;
    const auto find = [&](std::uint32_t stage) -> std::optional<std::size_t> {
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (out[i].stage == stage) {
                return i;
            }
        }
        return std::nullopt;
    };
    for (const text::Entry& e : text::section_entries(file, "BAUSTUFEN")) {
        const bool stufe = text::equals(e.key, "Stufe");
        const bool wissen = text::equals(e.key, "Wissen");
        if (!stufe && !wissen) {
            continue;
        }
        const auto f = text::fields(e.value);
        if (f.size() != (stufe ? 3U : 2U)) {
            return where(e) +
                   (stufe ? "Stufe needs <name>, <group>, <count>" : "Wissen needs <name>, <n>");
        }
        const std::optional<std::uint32_t> stage = stage_index_of(str(f[0]));
        if (!stage) {
            return where(e) + "'" + str(f[0]) + "' is not a stage name";
        }
        const auto a = number(f[1]);
        if (!a) {
            return where(e) + "'" + str(f[1]) + "' is not an unsigned 32-bit integer";
        }
        if (stufe) {
            const auto c = number(f[2]);
            if (!c) {
                return where(e) + "'" + str(f[2]) + "' is not an unsigned 32-bit integer";
            }
            if (find(*stage)) {
                return where(e) + "stage " + str(f[0]) + " is listed twice";
            }
            out.push_back(StageRecord{*stage, *a, 0, *c});
            have_wissen.push_back(0);
        } else {
            const auto i = find(*stage);
            if (!i) {
                return where(e) + "Wissen for " + str(f[0]) + " has no Stufe line before it";
            }
            if (have_wissen[*i] != 0) {
                return where(e) + "Wissen for " + str(f[0]) + " is given twice";
            }
            have_wissen[*i] = 1;
            out[*i].wissen = *a;
        }
    }
    std::sort(out.begin(), out.end(),
              [](const StageRecord& x, const StageRecord& y) { return x.stage < y.stage; });
    return out;
}

} // namespace anno::sim
