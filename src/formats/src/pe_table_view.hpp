// Private checked PE32 table view shared by the two original-module readers.
// Maps bounded data ranges only; no code loading, relocation or execution.
#pragma once
#include <anno/formats/annowelt.hpp>
namespace anno::formats::pe_tables_detail {
using annowelt::WalkTablesError;

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

inline bool starts_pe(Bytes dll, std::size_t at) {
    return at + 4 <= dll.size() && dll[at] == 'P' && dll[at + 1] == 'E' && dll[at + 2] == 0 &&
           dll[at + 3] == 0;
}

inline Result<std::size_t, WalkTablesError> file_offset(Bytes dll, const std::vector<Section>& secs,
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

struct Image {
    Bytes bytes;
    std::vector<Section> sections;
    std::uint16_t machine;
    std::uint32_t stamp, size, base;
    Result<std::size_t, WalkTablesError> offset(std::uint32_t rva, std::size_t n) const {
        if (rva > size || n > std::uint64_t{size} - rva)
            return WalkTablesError::RvaOutsideSection;
        return file_offset(bytes, sections, rva, n);
    }
};
inline Result<Image, WalkTablesError> read(Bytes dll, std::uint32_t expected_stamp,
                                           std::uint32_t expected_size) {
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
    const auto image_base = u32_le(dll, opt + kOptImageBase);
    const std::uint32_t size_of_image = u32_le(dll, opt + kOptSizeOfImage);
    // Preserve the legacy reader's wrong-build priority before section validation.
    if (stamp != expected_stamp || size_of_image != expected_size)
        return WalkTablesError::WrongBuild;
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

    return Image{dll, std::move(secs), u16_le(dll, e_lfanew + 4), stamp, size_of_image, image_base};
}
} // namespace anno::formats::pe_tables_detail
