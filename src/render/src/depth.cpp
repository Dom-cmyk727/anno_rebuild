#include <cmath>

#include <anno/render/depth.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/numeric.hpp>

namespace anno::render::depth {

namespace fig = formats::figuren;

Params params(std::size_t group, Lift lift) {
    const double th = static_cast<double>(kTile[group].h);
    // Lift::scaled's px per HEIGHT unit at `group`, unrounded.
    const double l = static_cast<double>(lift.num) / static_cast<double>(lift.den) *
                     static_cast<double>(kTile[group].w) / static_cast<double>(kTile[0].w);
    Params p;
    p.ca = static_cast<std::int32_t>(std::lround(256.0 * 256.0 * kPerTileSum / th));
    p.cb = static_cast<std::int32_t>(
        std::lround(256.0 * 4.0 * (2.0 * kPerTileSum * l / th + kPerHeight)));
    p.sv = l > 0.0 ? static_cast<std::int32_t>(std::lround(256.0 * 256.0 * kPerHeight / l)) : 0;
    p.sh = -2 * p.ca;
    return p;
}

std::int32_t at(const Params& p, std::int64_t y2, std::int64_t h64) {
    return static_cast<std::int32_t>(kZero - div_euclid<std::int64_t>(p.ca * y2 + p.cb * h64, 256));
}

Origin origin(const Params& p, std::int64_t y2o) {
    const std::int64_t a = p.ca * y2o;
    return {static_cast<std::int32_t>(kZero - div_euclid<std::int64_t>(a, 256)),
            static_cast<std::int32_t>(a - 256 * div_euclid<std::int64_t>(a, 256))};
}

Rule rule(fig::FrameFormat format) {
    switch (format) {
    case fig::FrameFormat::RgbZ:
    case fig::FrameFormat::RgbZM:
        return {Class::Z, true, false};
    case fig::FrameFormat::RgbAZ:
        return {Class::Z, true, true};
    case fig::FrameFormat::RgbWZ:
        return {Class::Z, false, false};
    case fig::FrameFormat::RgbV:
        return {Class::V, true, false};
    case fig::FrameFormat::RgbAV:
        return {Class::V, false, false};
    case fig::FrameFormat::RgbH:
    case fig::FrameFormat::RgbAH:
        return {Class::H, false, false};
    case fig::FrameFormat::Alpha:
        return {};
    }
    return {};
}

std::int32_t pixel(const Params& p, const Draw& d, std::int32_t row, std::int64_t y2,
                   std::uint8_t z) {
    switch (d.rule.cls) {
    case Class::Z:
        return d.base + 256 * static_cast<std::int32_t>(static_cast<std::int8_t>(z));
    case Class::V:
        return d.base + div_euclid<std::int32_t>(p.sv * (row - d.anchor_row), 256);
    case Class::H:
        return at(p, y2, d.h64);
    case Class::None:
        break;
    }
    return d.base;
}

Target Target::from_terrain(Rect rect, const Params& params,
                            const std::vector<std::uint16_t>& terrain) {
    Target t{rect, params, {}, {}};
    const auto w = static_cast<std::size_t>(rect.w), h = static_cast<std::size_t>(rect.h);
    t.d.assign(w * h, kNone);
    t.tested.assign(w * h, 0);
    if (terrain.size() != w * h) {
        return t;
    }
    for (std::size_t row = 0; row < h; ++row) {
        const std::int64_t y2 = 2 * (std::int64_t{rect.y} + static_cast<std::int64_t>(row)) + 1;
        for (std::size_t col = 0; col < w; ++col) {
            const std::uint16_t v = terrain[row * w + col];
            if (v != kNoTerrain) {
                t.d[row * w + col] = at(params, y2, v);
            }
        }
    }
    return t;
}

std::size_t Target::index(int sx, int sy) const {
    if (sx < rect.x || sy < rect.y || sx >= rect.x + rect.w || sy >= rect.y + rect.h) {
        return static_cast<std::size_t>(-1);
    }
    return static_cast<std::size_t>(sy - rect.y) * static_cast<std::size_t>(rect.w) +
           static_cast<std::size_t>(sx - rect.x);
}

std::vector<std::uint8_t> Target::submerged(std::int64_t sea_h64) const {
    std::vector<std::uint8_t> out;
    const auto w = static_cast<std::size_t>(rect.w);
    bool any = false;
    for (std::size_t i = 0; i < tested.size(); ++i) {
        if (tested[i] == 0) {
            continue;
        }
        const std::int64_t y2 = 2 * (std::int64_t{rect.y} + static_cast<std::int64_t>(i / w)) + 1;
        if (at(params, y2, sea_h64) <= d[i]) {
            if (!any) {
                out.assign(tested.size(), 0);
                any = true;
            }
            out[i] = 1;
        }
    }
    return out;
}

Draw draw(const Params& p, int anchor_y, std::uint8_t height,
          std::optional<fig::FrameFormat> format, int frame_anchor_y) {
    if (!format) {
        return {};
    }
    const std::int64_t h64 = 64 * std::int64_t{height};
    return {at(p, 2 * std::int64_t{anchor_y}, h64), rule(*format), frame_anchor_y, h64};
}

bool test(Target& t, int sx, int sy, const Rgba& img, int col, int row, const Draw& d) {
    if (d.rule.cls == Class::None) {
        return true;
    }
    const std::size_t i = t.index(sx, sy);
    if (i >= t.d.size()) {
        return true;
    }
    const std::size_t k = static_cast<std::size_t>(row) * img.width + static_cast<std::size_t>(col);
    const std::uint8_t z = k < img.z.size() ? img.z[k] : std::uint8_t{0};
    const std::int32_t pd = pixel(t.params, d, row, 2 * std::int64_t{sy} + 1, z);
    if (pd > t.d[i]) {
        return false;
    }
    const bool half = d.rule.skip_half && img.pixels[k * 4 + 3] == kHalfAlpha;
    if (d.rule.writes && !half) {
        t.d[i] = pd;
    }
    t.tested[i] = 1;
    return true;
}

void untest(Target& t, int sx, int sy) {
    const std::size_t i = t.index(sx, sy);
    if (i < t.tested.size()) {
        t.tested[i] = 0;
    }
}

DrawStats draw_prepared(Canvas& canvas, std::span<const Prepared> list,
                        const formats::anlagen::AnlagenFile& anlagen, std::size_t group, Lift lift,
                        std::uint64_t time_ms, SpriteCache& cache, Target& t,
                        std::pair<int, int> offset) {
    std::vector<SpriteDraw> draws;
    DrawStats stats = place_prepared(
        list, anlagen, group, lift, time_ms,
        [&canvas](int x, int y, int w, int h) { return canvas.overlaps(x, y, w, h); }, draws);
    for (const auto& sd : draws) {
        const auto img = cache.get(anlagen, sd.key, *sd.frame);
        if (!img) {
            if (!sd.is_shadow()) {
                --stats.drawn;
                ++stats.failed;
            }
            continue;
        }
        const Draw d = sd.is_shadow() ? Draw{}
                                      : draw(t.params, sd.anchor.second + offset.second, sd.height,
                                             sd.frame->format(), sd.frame->anchor_y);
        const int ox = sd.anchor.first - sd.frame->anchor_x;
        const int oy = sd.anchor.second - sd.frame->anchor_y;
        const Rgba& image = *img;
        blit_sprite(
            canvas, image, *sd.frame, sd.anchor, sd.is_shadow(),
            [&](int x, int y, Px px) -> std::optional<Px> {
                if (!canvas.index(x, y)) {
                    return std::nullopt;
                }
                if (!test(t, x + offset.first, y + offset.second, image, x - ox, y - oy, d)) {
                    return std::nullopt;
                }
                return px;
            });
    }
    return stats;
}

std::int64_t sea_h64(float shore_height) {
    return static_cast<std::int64_t>(std::lround(static_cast<double>(shore_height) * 64.0));
}

} // namespace anno::render::depth
