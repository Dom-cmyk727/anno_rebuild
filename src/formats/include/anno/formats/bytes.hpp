// Explicit little-endian reads and writes of fixed-width integers. File data
// is never reinterpreted as structs (CLAUDE.md, "Explicit serialization").
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace anno::formats {

/// Non-owning: a `Bytes` field of a parsed struct (e.g. `tex::Texture::body`,
/// `tex::Palette::entries`) is a view *into the buffer `parse()` was called with*, not a copy. The
/// caller must keep that buffer alive for at least as long as the parsed struct (W4-UI-2 H1: a
/// cache that stored only the parsed `TexFile`, letting the `std::vector<uint8_t>` it was parsed
/// from go out of scope, left every such span dangling - a real crash, not a hypothetical one).
using Bytes = std::span<const std::uint8_t>;
using ByteBuffer = std::vector<std::uint8_t>;

/// `u16` little-endian at `offset`; the caller checks the bounds.
inline std::uint16_t u16_le(Bytes b, std::size_t offset) {
    return static_cast<std::uint16_t>(b[offset] | (b[offset + 1] << 8));
}

/// `u32` little-endian at `offset`; the caller checks the bounds.
inline std::uint32_t u32_le(Bytes b, std::size_t offset) {
    return static_cast<std::uint32_t>(b[offset]) | static_cast<std::uint32_t>(b[offset + 1]) << 8 |
           static_cast<std::uint32_t>(b[offset + 2]) << 16 |
           static_cast<std::uint32_t>(b[offset + 3]) << 24;
}

/// `i16` little-endian at `offset` (two's complement).
inline std::int16_t i16_le(Bytes b, std::size_t offset) {
    return static_cast<std::int16_t>(u16_le(b, offset));
}

inline void put_u16_le(ByteBuffer& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

inline void put_u32_le(ByteBuffer& out, std::uint32_t v) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xFF));
    }
}

inline void put_bytes(ByteBuffer& out, Bytes b) {
    out.insert(out.end(), b.begin(), b.end());
}

} // namespace anno::formats
