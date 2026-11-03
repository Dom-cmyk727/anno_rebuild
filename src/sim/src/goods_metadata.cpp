// The [WAREN] metadata reader (header: goods_metadata.hpp).
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

} // namespace anno::sim
