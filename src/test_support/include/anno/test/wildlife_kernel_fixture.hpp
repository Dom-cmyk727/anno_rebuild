// One authored PE layout for kernel-reader tests, not an original binary/table.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <anno/formats/bytes.hpp>

namespace anno::test::kernel_fixture {
inline constexpr std::size_t pe = 0x40, opt = 0x58, section = 0x138, raw = 0x300;
inline constexpr std::uint32_t va = 0xca000;
inline void u16(formats::ByteBuffer& b, std::size_t at, std::uint16_t n) {
    b.at(at) = static_cast<std::uint8_t>(n & 255U);
    b.at(at + 1) = static_cast<std::uint8_t>(n >> 8U);
}
inline void u32(formats::ByteBuffer& b, std::size_t at, std::uint32_t n) {
    for (std::size_t i = 0; i < 4; ++i)
        b.at(at + i) = static_cast<std::uint8_t>((n >> (8U * i)) & 255U);
}
inline formats::ByteBuffer image(std::uint32_t extent = 0xc0) {
    formats::ByteBuffer b(raw + extent, 0);
    b.at(0) = 'M';
    b.at(1) = 'Z';
    u32(b, 0x3c, pe);
    b.at(pe) = 'P';
    b.at(pe + 1) = 'E';
    u16(b, pe + 4, 0x14c);
    u16(b, pe + 6, 1);
    u32(b, pe + 8, 0x44c0c353);
    u16(b, pe + 0x14, 0xe0);
    u16(b, opt, 0x10b);
    u32(b, opt + 0x1c, 0x10000000);
    u32(b, opt + 0x38, 0xf1000);
    u32(b, section + 8, extent);
    u32(b, section + 12, va);
    u32(b, section + 16, extent);
    u32(b, section + 20, raw);
    u32(b, section + 36, 0x40000040);
    std::fill(b.begin() + raw, b.end(), 0xe2);
    std::fill_n(b.begin() + raw, 8, 0xd1);
    for (std::size_t row = 0; row < 7; ++row)
        for (std::size_t i = 0; i < 25; ++i) {
            // Authored rows, deliberately asymmetric and different from each other.
            const auto value = row == 6 ? (i == 12   ? 12U
                                           : i == 13 ? 24U
                                           : i == 17 ? 12U
                                                     : 0U)
                                        : static_cast<unsigned>((row * 31 + i * 7 + 3) % 256);
            b.at(raw + 8 + row * 25 + i) = static_cast<std::uint8_t>(value);
        }
    return b;
}
} // namespace anno::test::kernel_fixture
