// The group table parser (header: house_groups.hpp). The formulas are the loader's
// (AnnoGame.dll 0x10014920 ff., docs/re/population.md §1), in integers: a value
// written with two decimals h/100 gives Menge = (h * 4096 + 5000) / 10000 and
// VermehrTab = sign(h) * (|h| * 64 / 100), both truncating.
#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

#include <anno/formats/text.hpp>
#include <anno/sim/house_groups.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

namespace {

namespace text = formats::text;
using Err = std::string;

std::string str(formats::Bytes b) {
    return std::string(b.begin(), b.end());
}

std::string at(const text::Entry& e) {
    return "BGruppen.dat line " + std::to_string(e.line) + ": ";
}

/// TRUE / FALSE.
bool boolean(const text::Entry& e, bool& out, Err& err) {
    if (text::equals(e.value, "TRUE")) {
        out = true;
        return true;
    }
    if (text::equals(e.value, "FALSE")) {
        out = false;
        return true;
    }
    err = at(e) + str(e.key) + " is '" + str(e.value) + "', not TRUE or FALSE";
    return false;
}

/// A list of exactly `N` integers that fit an i8.
template <std::size_t N>
bool table(const text::Entry& e, std::array<std::int8_t, N>& out, Err& err) {
    const auto v = text::ints(e.value);
    if (!v) {
        err = at(e) + str(e.key) + " holds a value that is not an integer";
        return false;
    }
    if (v->size() != N) {
        err = at(e) + str(e.key) + " has " + std::to_string(v->size()) +
              " values, the loader reads " + std::to_string(N);
        return false;
    }
    for (std::size_t i = 0; i < N; ++i) {
        if ((*v)[i] < -128 || (*v)[i] > 127) {
            err = at(e) + str(e.key) + " value " + std::to_string((*v)[i]) + " does not fit a byte";
            return false;
        }
        out[i] = static_cast<std::int8_t>((*v)[i]);
    }
    return true;
}

/// The slot BGruppeNr names: the five names the loader compares (population.md §1).
std::optional<std::uint32_t> slot_of(formats::Bytes name) {
    constexpr std::string_view kNames[] = {"PIONIER", "SIEDLER", "BUERGER", "KAUFLEUTE",
                                           "ARISTOKRATEN"};
    for (std::uint32_t i = 0; i < std::size(kNames); ++i) {
        if (text::equals(name, kNames[i])) {
            return i;
        }
    }
    return std::nullopt;
}

formats::Result<HouseGroup, Err> parse_group(const std::vector<text::Entry>& record,
                                             std::span<const GoodsInfo> goods) {
    auto [head, wares] = text::split_records(record, "Ware");
    HouseGroup g;
    bool have_number = false, have_wohnmax = false, have_nahrung = false, have_bedarf = false,
         have_infra = false, have_vermehr = false;
    std::size_t first_line = record.empty() ? 0 : record.front().line;
    Err err;
    for (const text::Entry& e : head) {
        if (text::equals(e.key, "BGruppeNr")) {
            const auto s = slot_of(e.value);
            if (!s) {
                return at(e) + "BGruppeNr '" + str(e.value) +
                       "' is not one of the five group names";
            }
            g.number = *s;
            have_number = true;
        } else if (text::equals(e.key, "Wohnmax")) {
            const auto v = text::to_int(e.value);
            if (!v || *v < 0 || *v > 0xFFFF) {
                return at(e) + "Wohnmax '" + str(e.value) + "' is not an integer from 0 to 65535";
            }
            g.wohnmax = static_cast<std::uint16_t>(*v);
            have_wohnmax = true;
        } else if (text::equals(e.key, "NoUpgrade")) {
            if (!boolean(e, g.no_upgrade, err)) {
                return err;
            }
        } else if (text::equals(e.key, "NoDowngrade")) {
            if (!boolean(e, g.no_downgrade, err)) {
                return err;
            }
        } else if (text::equals(e.key, "NahrungTab")) {
            if (!table(e, g.nahrung, err)) {
                return err;
            }
            have_nahrung = true;
        } else if (text::equals(e.key, "BedarfTab")) {
            if (!table(e, g.bedarf, err)) {
                return err;
            }
            have_bedarf = true;
        } else if (text::equals(e.key, "InfraTab")) {
            if (!table(e, g.infra, err)) {
                return err;
            }
            have_infra = true;
        } else if (text::equals(e.key, "VermehrTab")) {
            const auto f = text::fields(e.value);
            if (f.size() != g.vermehr.size()) {
                return at(e) + "VermehrTab has " + std::to_string(f.size()) +
                       " values, the loader reads " + std::to_string(g.vermehr.size());
            }
            for (std::size_t i = 0; i < f.size(); ++i) {
                const auto h = text::fixed(f[i], 2);
                if (!h) {
                    return at(e) + "VermehrTab value '" + str(f[i]) +
                           "' is not a decimal with at most two digits";
                }
                const std::int64_t q = (*h < 0 ? -1 : 1) * (std::llabs(*h) * 64 / 100);
                if (q < -128 || q > 127) {
                    return at(e) + "VermehrTab value '" + str(f[i]) + "' does not fit a byte";
                }
                g.vermehr[i] = static_cast<std::int8_t>(q);
            }
            have_vermehr = true;
        }
        // Every other key (BauFirst, BauLast, Hiscore, Wissen, ...) is not part of the table.
    }
    const auto missing = [&](const char* key) -> Err {
        return "BGruppen.dat line " + std::to_string(first_line) + ": group record without " + key;
    };
    if (!have_number) {
        return missing("BGruppeNr");
    }
    if (!have_wohnmax) {
        return missing("Wohnmax");
    }
    if (!have_nahrung) {
        return missing("NahrungTab");
    }
    if (!have_bedarf) {
        return missing("BedarfTab");
    }
    if (!have_infra) {
        return missing("InfraTab");
    }
    if (!have_vermehr) {
        return missing("VermehrTab");
    }
    for (const auto& w : wares) {
        const text::Entry& first = w.front();
        std::optional<GoodsId> id;
        for (std::size_t i = 0; i < goods.size(); ++i) {
            if (text::equals(first.value, goods[i].name)) {
                id = static_cast<GoodsId>(i);
                break;
            }
        }
        if (!id) {
            return at(first) + "Ware '" + str(first.value) + "' is not in the goods table";
        }
        HouseNeed need;
        need.ware = *id;
        bool have_menge = false, have_force = false;
        for (const text::Entry& e : w) {
            if (text::equals(e.key, "Menge")) {
                const auto h = text::fixed(e.value, 2);
                if (!h || *h < 0) {
                    return at(e) + "Menge '" + str(e.value) +
                           "' is not a decimal with at most two digits";
                }
                const std::int64_t m = (*h * 4096 + 5000) / 10000;
                if (m > 0xFFFF) {
                    return at(e) + "Menge '" + str(e.value) + "' does not fit 16 bits";
                }
                need.menge = static_cast<std::uint16_t>(m);
                have_menge = true;
            } else if (text::equals(e.key, "Force")) {
                if (!boolean(e, need.force, err)) {
                    return err;
                }
                have_force = true;
            }
        }
        if (!have_menge) {
            return at(first) + "need '" + str(first.value) + "' without Menge";
        }
        if (!have_force) {
            return at(first) + "need '" + str(first.value) + "' without Force";
        }
        g.needs.push_back(need);
    }
    return g;
}

} // namespace

formats::Result<std::vector<HouseGroup>, std::string>
parse_house_groups(formats::Bytes file, std::span<const GoodsInfo> goods) {
    const auto entries = text::section_entries(file, "BGRUPPE");
    const auto [head, records] = text::split_records(entries, "BGruppeNr");
    if (records.empty()) {
        return Err("BGruppen.dat has no [BGRUPPE] record");
    }
    std::vector<HouseGroup> groups;
    groups.reserve(records.size());
    for (const auto& r : records) {
        auto g = parse_group(r, goods);
        if (!g) {
            return g.error();
        }
        groups.push_back(std::move(g.value()));
    }
    std::sort(groups.begin(), groups.end(),
              [](const HouseGroup& a, const HouseGroup& b) { return a.number < b.number; });
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (i > 0 && groups[i - 1].number == groups[i].number) {
            return "BGruppen.dat: group slot " + std::to_string(groups[i].number) +
                   " is defined twice";
        }
        if (groups[i].number != i) {
            return "BGruppen.dat: group slot " + std::to_string(i) + " is missing";
        }
    }
    return groups;
}

} // namespace anno::sim

namespace anno::sim {

std::string derive_group(const HouseGroup& group, std::span<const GoodsInfo> goods,
                         const PopulationRoles& roles, HouseGroupDerived& out) {
    out = HouseGroupDerived{};
    if (roles.infra_first > roles.infra_last || roles.infra_last >= goods.size() ||
        roles.tavern_alias_goods >= goods.size() || roles.tavern_goods < roles.infra_first ||
        roles.tavern_goods > roles.infra_last ||
        (roles.tavern_alias_goods >= roles.infra_first &&
         roles.tavern_alias_goods <= roles.infra_last)) {
        return "the population roles are outside the goods table";
    }
    const std::size_t n = group.needs.size();
    if (n > 0xFFFFFFFEU) {
        return "the group has too many needs";
    }
    const auto count = static_cast<std::uint32_t>(n);
    for (std::size_t j = 0; j < n; ++j) {
        const HouseNeed& need = group.needs[j];
        if (need.ware == 0) {
            return "need " + std::to_string(j) + ": goods 0 (UNUSED) is not a need";
        }
        if (need.ware >= goods.size()) {
            return "need " + std::to_string(j) + ": goods " + std::to_string(need.ware) +
                   " is not in the goods table";
        }
        if (need.menge == 0) {
            return "need " + std::to_string(j) + ": Menge 0 is a zero demand";
        }
    }
    out.need_count = count;
    // The loader's per-Ware update (0x1001486b-0x100148a6), n = slot + 1; Force is not read.
    for (std::uint32_t j = 0; j < count; ++j) {
        const GoodsId ware = group.needs[j].ware;
        if (ware < roles.infra_first) {
            out.goods_end = j + 1;
        } else if (ware > roles.infra_last && out.goods_end == 0) {
            out.goods_end = j + 1;
        }
    }
    // The post-pass (0x10014afc-0x10014bd9).
    out.lookup.assign(goods.size(), count);
    out.forced.assign(static_cast<std::size_t>(roles.infra_last - roles.infra_first) + 1, 0);
    for (std::uint32_t j = 0; j < count; ++j) {
        const HouseNeed& need = group.needs[j];
        out.lookup[need.ware] = j; // a repeated ware: the last slot wins
        if (need.ware >= roles.infra_first && need.ware <= roles.infra_last) {
            out.infra_end = j + 1; // forced or not (0x10014b33 precedes the Force test)
            if (need.force) {
                out.forced[need.ware - roles.infra_first] = 1;
            }
        }
        if (need.ware == roles.tavern_goods && out.lookup[roles.tavern_alias_goods] == count) {
            out.lookup[roles.tavern_alias_goods] = j; // the first tavern takes the alias
        }
    }
    // One ascending IdSec substitution pass (0x10014b78-0x10014bab).
    for (GoodsId id = roles.infra_first; id <= roles.infra_last; ++id) {
        if (out.lookup[id] == count) {
            const std::uint32_t via = out.lookup[goods[id].id_sec];
            if (via != count) {
                out.lookup[id] = via;
            }
        }
    }
    // MAP-POP-CLOSE-1 G2 (population-contract P-3): the IdSec walk of a delivery starts at any goods
    // id that has a need slot and continues through substitutes that have one; visiting a goods id
    // twice (a cycle or self-loop) is a content error. Only that relevant walk is checked, so an
    // unrelated cycle among goods nobody needs is not an error, and the goods count is irrelevant.
    std::vector<std::uint32_t> visited(goods.size(), 0); // stamp = start + 1: no clearing per walk
    for (GoodsId start = 0; start < goods.size(); ++start) {
        if (out.lookup[start] == count) {
            continue;
        }
        const std::uint32_t stamp = start + 1U;
        visited[start] = stamp;
        GoodsId cur = start;
        for (;;) {
            const GoodsId sec = goods[cur].id_sec;
            if (sec == 0 || sec >= goods.size() || out.lookup[sec] == count) {
                break;
            }
            if (visited[sec] == stamp) {
                return "IdSec cycle through goods " + std::to_string(sec) + " (reached from goods " +
                       std::to_string(start) + "): a delivery would credit the cycle repeatedly";
            }
            visited[sec] = stamp;
            cur = sec;
        }
    }
    out.infra_end = std::max(out.infra_end, out.goods_end);
    return {};
}

} // namespace anno::sim
