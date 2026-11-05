// The [WAREN] metadata reader (header: goods_metadata.hpp).
#include <algorithm>
#include <array>
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

} // namespace

std::optional<std::uint32_t> stage_index_of(std::string_view name) {
    for (std::uint32_t i = 0; i < kStageNames.size(); ++i) {
        if (name == kStageNames[i]) {
            return i;
        }
    }
    return std::nullopt;
}

std::string apply_goods_metadata(formats::Bytes file, std::span<GoodsInfo> goods) {
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
            }
        }
    }
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
