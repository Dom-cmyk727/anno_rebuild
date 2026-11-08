// Bitmap-font rendering (.zei, docs/formats/zei.md) into RGBA, on top of the already-decoded
// glyph levels (formats::zei::Glyph::decode).
//
// The three glyph levels (1-3) are painted here as alpha-blended steps of one text colour
// (TextStyle::level_alpha), a bitmap-font anti-alias falloff - not a literal three-colour emboss.
// This is a flagged legibility default, not an RE fact: OQ-031 (docs/open-questions.md) leaves
// what the original actually draws for each level open (no Ghidra access on this host for the
// 32-bit original, and no screenshot comparison has been made yet). Nothing in this file is
// stated as the original's own behaviour.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <anno/assets/texture.hpp>
#include <anno/formats/zei.hpp>

namespace anno::assets {

/// How a .zei glyph's three levels are painted; see this header's own note - a legibility
/// default, not an RE-confirmed rule (OQ-031).
struct TextStyle {
    std::array<std::uint8_t, 3> rgb = {255, 255, 255};
    /// Alpha for level 1, 2, 3 respectively (the darkest to the lightest of the glyph's own
    /// fill/edge pixels, zei.md's own description).
    std::array<std::uint8_t, 3> level_alpha = {110, 185, 255};
};

/// Renders `text` (Windows-1252 bytes, zei.md's own code convention) left to right in `font`,
/// each glyph's own `width` as its advance (CharSet::glyphs is indexed by code 0-255 already,
/// zei.hpp). The canvas is exactly as wide as the advances sum to and as tall as the tallest
/// decoded glyph; a code with no glyph (width 0, codes 0-31) advances by 0 and paints nothing, a
/// glyph that fails to decode (never seen in the corpus) is skipped rather than corrupting the
/// image. Fully transparent wherever no glyph painted a pixel.
[[nodiscard]] Rgba render_text(const formats::zei::CharSet& font, std::string_view text,
                               const TextStyle& style = {});

} // namespace anno::assets
