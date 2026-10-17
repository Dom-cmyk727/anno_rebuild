#include <algorithm>
#include <limits>

#include <anno/assets/island.hpp>

namespace anno::assets::island {

namespace insel = formats::insel;

Result<GroundTextures, std::string> GroundTextures::load(const formats::tex::TexFile& felder) {
    GroundTextures out;
    constexpr std::size_t n = insel::kGroundTexturePx;
    for (const auto& t : felder.textures) {
        if (t.head.width != n || t.head.height != n) {
            continue;
        }
        const formats::tex::Palette* palette = felder.palette(t.head.palette_id);
        if (palette == nullptr) {
            continue;
        }
        auto px = t.decode();
        if (!px) {
            return px.error();
        }
        std::vector<formats::tex::Rgb> rgb;
        rgb.reserve(px.value().size());
        for (const auto& p : px.value()) {
            rgb.push_back(p ? palette->rgb(*p) : formats::tex::Rgb{0, 0, 0});
        }
        out.textures_.insert_or_assign(t.id, std::move(rgb));
    }
    return out;
}

std::optional<Colour> GroundTextures::average(std::uint16_t id) const {
    const auto it = textures_.find(id);
    if (it == textures_.end()) {
        return std::nullopt;
    }
    std::array<std::uint64_t, 3> sum{};
    for (const auto& px : it->second) {
        for (std::size_t c = 0; c < 3; ++c) {
            sum[c] += px[c];
        }
    }
    const std::uint64_t count = std::max<std::uint64_t>(it->second.size(), 1);
    return Colour{static_cast<std::uint8_t>(sum[0] / count),
                  static_cast<std::uint8_t>(sum[1] / count),
                  static_cast<std::uint8_t>(sum[2] / count), 255};
}

Colour GroundTextures::cell_pixel(insel::GroundCell cell, std::size_t s, std::size_t t,
                                  std::size_t n) const {
    if (cell.texture == 0) {
        return water_;
    }
    const auto it = textures_.find(cell.texture);
    if (it == textures_.end()) {
        return kMissing;
    }
    const auto [sx, sy] = cell.texel(s, t, n);
    const auto& rgb = it->second[sy * insel::kGroundTexturePx + sx];
    return {rgb[0], rgb[1], rgb[2], 255};
}

Result<Rgba, std::string> ground_top_down(const insel::Island& island,
                                          const GroundTextures& textures, std::uint16_t scale) {
    auto gfx = island.gfx_ids();
    if (!gfx) {
        return gfx.error();
    }
    const std::size_t w = island.width;
    const std::size_t s = std::clamp<std::uint16_t>(scale, 1, 32);
    const std::size_t out_w = w * s;
    const std::size_t out_h = std::size_t{island.height} * s;
    constexpr std::size_t max_dim = std::numeric_limits<std::uint16_t>::max();
    if (out_w > max_dim || out_h > max_dim) {
        return std::string("image larger than 65535 pixels");
    }
    std::vector<std::uint8_t> pixels(out_w * out_h * 4);
    const auto& ids = gfx.value();
    for (std::size_t t = 0; t < ids.size(); ++t) {
        const auto cell = insel::GroundCell::from_gfx_id(ids[t]);
        const std::size_t tx = t % w, ty = t / w;
        for (std::size_t j = 0; j < s; ++j) {
            for (std::size_t i = 0; i < s; ++i) {
                const Colour px = textures.cell_pixel(cell, i, j, s);
                const std::size_t o = ((ty * s + j) * out_w + tx * s + i) * 4;
                std::copy(px.begin(), px.end(), pixels.begin() + static_cast<std::ptrdiff_t>(o));
            }
        }
    }
    return Rgba{static_cast<std::uint16_t>(out_w), static_cast<std::uint16_t>(out_h),
                std::move(pixels)};
}

} // namespace anno::assets::island
