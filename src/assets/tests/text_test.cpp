// Synthetic tests of anno_assets::render_text (no game files needed): builds a tiny CharSet by
// hand (two glyphs, the same row RLE grammar docs/formats/zei.md describes) rather than parsing a
// real .zei file, since the glyph shapes themselves are not what this module adds - zei.cpp's own
// corpus test already proves every real glyph decodes; this proves render_text composes decoded
// glyphs correctly (advance, canvas size, transparency, colour/alpha).
#include <cstddef>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/text.hpp>
#include <anno/formats/bytes.hpp>
#include <anno/formats/zei.hpp>

namespace assets = anno::assets;
namespace zei = anno::formats::zei;
using anno::formats::Bytes;

namespace {

/// A 256-code CharSet where every code is a width-0, length-0 glyph except `code`, which gets
/// `data` (owned by `storage`, which the caller must keep alive - Bytes is a non-owning span,
/// formats::Bytes's own documented lifetime rule) and `width`.
zei::CharSet make_charset(std::uint8_t code, std::uint32_t width,
                          const std::vector<std::uint8_t>& storage) {
    zei::CharSet cs;
    cs.glyphs.resize(256);
    for (std::size_t i = 0; i < cs.glyphs.size(); ++i) {
        cs.glyphs[i].code = static_cast<std::uint8_t>(i);
    }
    cs.glyphs[code].width = width;
    cs.glyphs[code].data = Bytes(storage);
    return cs;
}

} // namespace

TEST_CASE("render_text: one 3x2 glyph at level 2 paints every pixel with the style's level-2 "
          "alpha, sized to the glyph exactly") {
    // row 0: skip 0, count 3, levels {2,2,2}; 0xFE; row 1 (last): same run; 0xFF.
    const std::vector<std::uint8_t> glyph_a_rle = {0, 3, 2, 2, 2, 0xFE, 0, 3, 2, 2, 2, 0xFF};
    const zei::CharSet font = make_charset('A', 3, glyph_a_rle);

    const assets::Rgba image = assets::render_text(font, "A");
    REQUIRE(image.width == 3);
    REQUIRE(image.height == 2);
    REQUIRE(image.pixels.size() == std::size_t{3} * 2 * 4);
    const assets::TextStyle style; // default: white, level_alpha {110, 185, 255}
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            const std::size_t o =
                (static_cast<std::size_t>(row) * 3 + static_cast<std::size_t>(col)) * 4;
            CHECK(image.pixels[o] == style.rgb[0]);
            CHECK(image.pixels[o + 1] == style.rgb[1]);
            CHECK(image.pixels[o + 2] == style.rgb[2]);
            CHECK(image.pixels[o + 3] == style.level_alpha[1]); // level 2 -> index 1
        }
    }
}

TEST_CASE("render_text: a code below 32 (width 0, no decode call at all) advances by nothing and "
          "paints nothing") {
    const std::vector<std::uint8_t> glyph_a_rle = {0, 1, 3, 0xFF}; // one opaque pixel, level 3
    zei::CharSet font = make_charset('A', 1, glyph_a_rle);
    // Code 1 keeps its default width 0 / empty data from make_charset - never decoded.
    const assets::Rgba image = assets::render_text(font, std::string_view("\x01"
                                                                          "A",
                                                                          2));
    // Only 'A' (width 1) contributes to the canvas; the control code advanced by 0.
    CHECK(image.width == 1);
    CHECK(image.height == 1);
    CHECK(image.pixels[3] == assets::TextStyle{}.level_alpha[2]); // level 3 -> index 2
}

TEST_CASE("render_text: a glyph with width but an empty (fully transparent) decode advances the "
          "canvas without painting any pixel") {
    const std::vector<std::uint8_t> blank_rle = {0xFF}; // immediate end: 1 row, 0 runs
    const zei::CharSet font = make_charset(' ', 2, blank_rle);
    const assets::Rgba image = assets::render_text(font, " ");
    REQUIRE(image.width == 2);
    REQUIRE(image.height == 1);
    for (const std::uint8_t byte : image.pixels) {
        CHECK(byte == 0); // transparent (alpha 0) and black, never a stray colour
    }
}

TEST_CASE("render_text: two glyphs advance left to right by their own widths, never overlapping") {
    const std::vector<std::uint8_t> glyph_a_rle = {0, 1, 3, 0xFF};
    const std::vector<std::uint8_t> glyph_b_rle = {0, 1, 1, 0xFF};
    zei::CharSet font = make_charset('A', 4, glyph_a_rle);
    font.glyphs['B'].width = 5;
    font.glyphs['B'].data = Bytes(glyph_b_rle);

    const assets::Rgba image = assets::render_text(font, "AB");
    REQUIRE(image.width == 9); // 4 + 5
    REQUIRE(image.height == 1);
    const assets::TextStyle style;
    // 'A' painted its one pixel at column 0 (level 3), 'B' at column 4 (level 1, its own glyph's
    // own x=0 relative position, offset by 'A' 's advance).
    CHECK(image.pixels[0 * 4 + 3] == style.level_alpha[2]);
    CHECK(image.pixels[4 * 4 + 3] == style.level_alpha[0]);
    // Nothing else in the row was painted.
    for (std::size_t col = 0; col < 9; ++col) {
        if (col == 0 || col == 4)
            continue;
        CHECK(image.pixels[col * 4 + 3] == 0);
    }
}
