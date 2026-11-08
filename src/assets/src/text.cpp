#include <algorithm>
#include <cstddef>
#include <vector>

#include <anno/assets/text.hpp>

namespace anno::assets {

namespace {

struct PlacedGlyph {
    formats::zei::GlyphImage image;
    int x = 0;
};

} // namespace

Rgba render_text(const formats::zei::CharSet& font, std::string_view text, const TextStyle& style) {
    std::vector<PlacedGlyph> placed;
    placed.reserve(text.size());
    int x = 0;
    std::size_t height = 0;
    for (const char c : text) {
        const auto code = static_cast<std::size_t>(static_cast<unsigned char>(c));
        if (code >= font.glyphs.size())
            continue;
        const formats::zei::Glyph& glyph = font.glyphs[code];
        if (glyph.width == 0)
            continue;
        if (auto decoded = glyph.decode(); decoded.ok()) {
            height = std::max(height, decoded.value().height);
            placed.push_back({std::move(decoded).value(), x});
        }
        x += static_cast<int>(glyph.width);
    }

    Rgba canvas;
    canvas.width = static_cast<std::uint16_t>(std::max(x, 0));
    canvas.height = static_cast<std::uint16_t>(height);
    canvas.pixels.assign(static_cast<std::size_t>(canvas.width) * canvas.height * 4, 0);

    for (const auto& placed_glyph : placed) {
        const auto& image = placed_glyph.image;
        for (std::size_t row = 0; row < image.height; ++row) {
            const int py = static_cast<int>(row);
            if (py < 0 || py >= canvas.height)
                continue;
            for (std::size_t col = 0; col < image.width; ++col) {
                const auto& level = image.levels[row * image.width + col];
                if (!level)
                    continue;
                const int px = placed_glyph.x + static_cast<int>(col);
                if (px < 0 || px >= canvas.width)
                    continue;
                const std::uint8_t lv = std::clamp<std::uint8_t>(*level, 1, 3);
                const std::uint8_t alpha = style.level_alpha[lv - 1];
                const std::size_t o =
                    (static_cast<std::size_t>(py) * canvas.width + static_cast<std::size_t>(px)) *
                    4;
                canvas.pixels[o] = style.rgb[0];
                canvas.pixels[o + 1] = style.rgb[1];
                canvas.pixels[o + 2] = style.rgb[2];
                canvas.pixels[o + 3] = alpha;
            }
        }
    }
    return canvas;
}

} // namespace anno::assets
