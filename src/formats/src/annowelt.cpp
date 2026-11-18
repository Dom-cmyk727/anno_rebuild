#include <utility>

#include <anno/formats/annowelt.hpp>

#include "pe_table_view.hpp"

namespace anno::formats::annowelt {

const char* error_message(WalkTablesError e) {
    switch (e) {
    case WalkTablesError::NotPe:
        return "not a PE";
    case WalkTablesError::WrongBuild:
        return "wrong build";
    case WalkTablesError::RvaOutsideSection:
        return "RVA outside section";
    case WalkTablesError::ShortFile:
        return "short file";
    }
    return "not a PE";
}

Result<WalkTables, WalkTablesError> read_walk_tables(Bytes dll) {
    const auto parsed = pe_tables_detail::read(dll, kGogTimeDateStamp, kGogSizeOfImage);
    if (!parsed)
        return parsed.error();
    const auto& image = parsed.value();
    if (image.stamp != kGogTimeDateStamp || image.size != kGogSizeOfImage)
        return WalkTablesError::WrongBuild;

    auto map_off = image.offset(kRvaMapper, kMapperCount * 2);
    if (!map_off) {
        return map_off.error();
    }
    auto goal_off = image.offset(kRvaGoalOrder, kGoalOrderRows * kGoalOrderWidth * 4);
    if (!goal_off) {
        return goal_off.error();
    }

    WalkTables out;
    out.time_date_stamp = image.stamp;
    out.size_of_image = image.size;
    out.mapper.reserve(kMapperCount);
    // Mapper: 512 × u16 at RVA 0xc1af8, indexed by 0x1008ff70 at 0x1009005f.
    for (std::size_t i = 0; i < kMapperCount; ++i) {
        out.mapper.push_back(u16_le(dll, map_off.value() + i * 2));
    }
    out.layers.reserve(kLayerCount);
    for (std::size_t L = 0; L < kLayerCount; ++L) {
        const std::uint32_t rva =
            kRvaLayers + static_cast<std::uint32_t>(L) * static_cast<std::uint32_t>(kLayerSize);
        auto layer_off = image.offset(rva, kLayerSize);
        if (!layer_off) {
            return layer_off.error();
        }
        std::vector<std::uint8_t> layer;
        layer.reserve(kLayerSize);
        // Layer L: 0x1c00 bytes at 0xd1140 + 0x1c00·L (setter 0x1000dd20;
        // admission 0x10006ab0, costs 0x10006ab9).
        for (std::size_t i = 0; i < kLayerSize; ++i) {
            layer.push_back(dll[layer_off.value() + i]);
        }
        out.layers.push_back(std::move(layer));
    }
    out.goal_order.resize(kGoalOrderRows);
    // Goal-order: 5 × 9 × 4 bytes at RVA 0xc1160 (0x100036fb..0x1000371c).
    for (std::size_t r = 0; r < kGoalOrderRows; ++r) {
        for (std::size_t k = 0; k < kGoalOrderWidth; ++k) {
            const std::size_t off = goal_off.value() + (r * kGoalOrderWidth + k) * 4;
            GoalEntry e;
            e.mask = u16_le(dll, off);
            e.col = dll[off + 2];
            e.row = dll[off + 3];
            out.goal_order[r][k] = e;
        }
    }
    return out;
}

} // namespace anno::formats::annowelt
