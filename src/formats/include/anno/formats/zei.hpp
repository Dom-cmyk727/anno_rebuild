// Bitmap fonts (.zei): root CHARSET with HEAD and BODY.
// Spec: docs/formats/zei.md; reference: tools/zei.py.
//
//   HEAD (2,064 bytes): u16 size_a, u16 size_b (OQ-031), 12 reserved bytes,
//        256 × {u32 width, u32 length} per character code.
//   BODY: the glyphs of all codes with length > 0, in code order.
// A glyph is a row RLE as .tex BSH_RGB: (skip, count, count × level) runs,
// 0xFE ends a row, 0xFF the glyph; levels 1–3. All glyphs of a font have
// the same number of rows (not stored).
#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include <anno/formats/bytes.hpp>
#include <anno/formats/chunk.hpp>
#include <anno/formats/result.hpp>

namespace anno::formats::zei {

inline constexpr std::size_t kHeadLen = 16 + 256 * 8;

struct GlyphImage {
    /// The glyph width, or more if a row's pixels reach further (never in the corpus).
    std::size_t width = 0;
    std::size_t height = 0;
    std::vector<std::optional<std::uint8_t>> levels;
};

struct Glyph {
    std::uint8_t code = 0;
    std::uint32_t width = 0;
    Bytes data;
    /// Decodes the RLE; only pixels widen the image (skips may end past it).
    [[nodiscard]] Result<GlyphImage, std::string> decode() const;
};

struct CharSet {
    /// Name fields of CHARSET, HEAD, BODY as stored (the two Scape fonts
    /// carry bytes after the NUL; OQ-031).
    std::array<chunk::NameField, 3> name_fields{};
    std::uint16_t size_a = 0, size_b = 0;
    std::array<std::uint8_t, 12> reserved{};
    /// All 256 codes in order.
    std::vector<Glyph> glyphs;

    static Result<CharSet, std::string> parse(Bytes buf);
    [[nodiscard]] ByteBuffer head_bytes() const;
    [[nodiscard]] ByteBuffer body_bytes() const;
    [[nodiscard]] ByteBuffer to_bytes() const;
};

} // namespace anno::formats::zei
