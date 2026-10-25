#include <utility>

#include <anno/formats/annowelt.hpp>

namespace anno::formats::annowelt {

namespace {

constexpr std::uint16_t kPe32Magic = 0x10b;
constexpr std::size_t kDosELfanew = 0x3c;
constexpr std::size_t kCoffSize = 20;
constexpr std::size_t kSectionHeaderSize = 40;
constexpr std::size_t kOptImageBase = 0x1c;
constexpr std::size_t kOptSizeOfImage = 0x38;
constexpr std::size_t kMinOptionalHeader = kOptSizeOfImage + 4;

struct Section {
    std::uint32_t va = 0;
    std::uint32_t raw_size = 0;
    std::uint32_t raw_ptr = 0;
};

bool starts_pe(Bytes dll, std::size_t at) {
    return at + 4 <= dll.size() && dll[at] == 'P' && dll[at + 1] == 'E' && dll[at + 2] == 0 &&
           dll[at + 3] == 0;
}

Result<std::size_t, WalkTablesError> file_offset(Bytes dll, const std::vector<Section>& secs,
                                                 std::uint32_t rva, std::size_t n) {
    for (const Section& s : secs) {
        if (rva < s.va) {
            continue;
        }
        const std::uint32_t rel = rva - s.va;
        if (rel > s.raw_size || s.raw_size - rel < n) {
            continue;
        }
        if (s.raw_ptr > dll.size() || dll.size() - s.raw_ptr < static_cast<std::size_t>(rel) + n) {
            return WalkTablesError::ShortFile;
        }
        return static_cast<std::size_t>(s.raw_ptr) + rel;
    }
    return WalkTablesError::RvaOutsideSection;
}

} // namespace

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
    if (dll.size() < kDosELfanew + 4 || dll[0] != 'M' || dll[1] != 'Z') {
        return WalkTablesError::NotPe;
    }
    const std::uint32_t e_lfanew = u32_le(dll, kDosELfanew);
    if (!starts_pe(dll, e_lfanew) || dll.size() - e_lfanew < 4 + kCoffSize) {
        return WalkTablesError::NotPe;
    }
    const std::uint16_t nsec = u16_le(dll, e_lfanew + 6);
    const std::uint32_t stamp = u32_le(dll, e_lfanew + 8);
    const std::uint16_t opt_size = u16_le(dll, e_lfanew + 0x14);
    if (opt_size < kMinOptionalHeader) {
        return WalkTablesError::NotPe;
    }
    const std::size_t opt = static_cast<std::size_t>(e_lfanew) + 4 + kCoffSize;
    if (opt > dll.size() || dll.size() - opt < opt_size) {
        return WalkTablesError::NotPe;
    }
    if (u16_le(dll, opt) != kPe32Magic) {
        return WalkTablesError::NotPe;
    }
    // ImageBase at optional +0x1c is part of the PE walk (spec); RVA lookup
    // does not add it.
    (void)u32_le(dll, opt + kOptImageBase);
    const std::uint32_t size_of_image = u32_le(dll, opt + kOptSizeOfImage);
    if (stamp != kGogTimeDateStamp || size_of_image != kGogSizeOfImage) {
        return WalkTablesError::WrongBuild;
    }
    const std::size_t sec0 = opt + opt_size;
    if (nsec > 0 && (sec0 > dll.size() || (dll.size() - sec0) / kSectionHeaderSize < nsec)) {
        return WalkTablesError::NotPe;
    }
    std::vector<Section> secs;
    secs.reserve(nsec);
    for (std::uint16_t i = 0; i < nsec; ++i) {
        const std::size_t off = sec0 + static_cast<std::size_t>(i) * kSectionHeaderSize;
        Section s;
        s.va = u32_le(dll, off + 0x0c);
        s.raw_size = u32_le(dll, off + 0x10);
        s.raw_ptr = u32_le(dll, off + 0x14);
        secs.push_back(s);
    }

    auto map_off = file_offset(dll, secs, kRvaMapper, kMapperCount * 2);
    if (!map_off) {
        return map_off.error();
    }
    auto goal_off = file_offset(dll, secs, kRvaGoalOrder, kGoalOrderRows * kGoalOrderWidth * 4);
    if (!goal_off) {
        return goal_off.error();
    }

    WalkTables out;
    out.time_date_stamp = stamp;
    out.size_of_image = size_of_image;
    out.mapper.reserve(kMapperCount);
    // Mapper: 512 × u16 at RVA 0xc1af8, indexed by 0x1008ff70 at 0x1009005f.
    for (std::size_t i = 0; i < kMapperCount; ++i) {
        out.mapper.push_back(u16_le(dll, map_off.value() + i * 2));
    }
    out.layers.reserve(kLayerCount);
    for (std::size_t L = 0; L < kLayerCount; ++L) {
        const std::uint32_t rva =
            kRvaLayers + static_cast<std::uint32_t>(L) * static_cast<std::uint32_t>(kLayerSize);
        auto layer_off = file_offset(dll, secs, rva, kLayerSize);
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
