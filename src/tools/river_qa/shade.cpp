// river_qa --shade: the river flow shading of a rectangle as images for review
// (docs/render/rivers.md 10.2, Q3/Q5/Q6), built from the viewer's own chunks
// (render::render_chunk: the baked flow lattice) and the same shading code as
// the CPU frame path (render::river_flow_shade through river::flow_at).
//
// Per time a grey image of the light of the river pixels (128 + light * 1500;
// river pixels without flow show the legacy shimmer in red tint), and one
// quiver image: the water mask, hue = flow direction on screen, an arrow every
// 24 pixels with a length proportional to the screen speed. The coverage line
// is the Q5 check "flow coverage >= 99.9 % of the river pixels".
#include "shade.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include <anno/app/png.hpp>
#include <anno/render/frame.hpp>

namespace fs = std::filesystem;
namespace render = anno::render;

namespace {

struct Px4 {
    std::uint8_t r = 0, g = 0, b = 0;
};

Px4 hsv(float h, float s, float v) {
    const float c = v * s;
    const float x = c * (1.0F - std::fabs(std::fmod(h * 6.0F, 2.0F) - 1.0F));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    const int k = static_cast<int>(h * 6.0F) % 6;
    switch (k) {
    case 0:
        r = c, g = x;
        break;
    case 1:
        r = x, g = c;
        break;
    case 2:
        g = c, b = x;
        break;
    case 3:
        g = x, b = c;
        break;
    case 4:
        r = x, b = c;
        break;
    default:
        r = c, b = x;
        break;
    }
    auto q = [&](float f) { return static_cast<std::uint8_t>(std::lround((f + m) * 255.0F)); };
    return {q(r), q(g), q(b)};
}

} // namespace

bool run_shade(const render::Scene& scene, std::size_t group, render::Rect view,
               const std::vector<std::uint64_t>& times, const fs::path& out,
               const std::string& name, float gain) {
    std::error_code ec;
    fs::create_directories(out, ec);
    const auto w = static_cast<std::size_t>(view.w), h = static_cast<std::size_t>(view.h);
    // The viewer's chunks (blocking: the flow is built here if nobody has).
    std::vector<std::uint8_t> flags(w * h, 0), shore(w * h, 255), depth(w * h, 0);
    std::vector<render::FlowSample> flows(w * h);
    const int cx0 = render::div_euclid(view.x, render::kChunk);
    const int cy0 = render::div_euclid(view.y, render::kChunk);
    const int cx1 = render::div_euclid(view.x + view.w - 1, render::kChunk);
    const int cy1 = render::div_euclid(view.y + view.h - 1, render::kChunk);
    for (int cy = cy0; cy <= cy1; ++cy) {
        for (int cx = cx0; cx <= cx1; ++cx) {
            const render::Chunk c = render::render_chunk(scene, group, cx, cy, false, true);
            const render::Rect rect = render::chunk_rect(cx, cy);
            const auto r = rect.intersect(view);
            if (!r || c.field.empty()) {
                continue;
            }
            for (int y = r->y; y < r->y + r->h; ++y) {
                for (int x = r->x; x < r->x + r->w; ++x) {
                    const auto i = static_cast<std::size_t>((y - rect.y) * rect.w + (x - rect.x));
                    const auto o = static_cast<std::size_t>(y - view.y) * w +
                                   static_cast<std::size_t>(x - view.x);
                    shore[o] = c.field[i * 4 + 1];
                    depth[o] = c.field[i * 4 + 2];
                    flags[o] = c.field[i * 4 + 3];
                    if (!c.flow.empty()) {
                        flows[o] = render::river::flow_at(c.flow, x, y);
                    }
                }
            }
        }
    }
    std::size_t river = 0, covered = 0;
    for (std::size_t i = 0; i < w * h; ++i) {
        if ((flags[i] & 2U) != 0) {
            ++river;
            covered += flows[i].valid >= scene.river_cfg.min_valid ? 1U : 0U;
        }
    }
    std::printf("| %s | %zu | %d,%d %dx%d | %zu | %zu | %.3f %% |\n", name.c_str(), group, view.x,
                view.y, view.w, view.h, river, covered,
                river ? 100.0 * static_cast<double>(covered) / static_cast<double>(river) : 100.0);

    render::river::RiverConfig cfg = scene.river_cfg;
    cfg.light_gain = gain;
    const render::RiverParams params = cfg.params();
    const auto [tw, th] = render::kTile[group];
    const render::RiverView rv{static_cast<float>(tw), static_cast<float>(th),
                               static_cast<float>(scene.lift.num) /
                                   static_cast<float>(scene.lift.den) * static_cast<float>(tw) /
                                   static_cast<float>(render::kTile[0].w)};
    const float detail = render::wave_detail(static_cast<float>(tw));
    const float raise_px = static_cast<float>(scene.lift.scaled(render::kSeaLevel, 1, group));
    for (const std::uint64_t t : times) {
        std::vector<std::uint8_t> img(w * h * 4, 255);
        // The water as it is drawn (tint, light and aeration on a flat open-water colour,
        // no sea pattern, no objects): the look of the shading alone.
        std::vector<std::uint8_t> col(w * h * 4, 255);
        for (std::size_t j = 0; j < h; ++j) {
            for (std::size_t i = 0; i < w; ++i) {
                const std::size_t o = j * w + i;
                std::uint8_t* p = &img[o * 4];
                if ((flags[o] & 2U) == 0) {
                    p[0] = 14, p[1] = 14, p[2] = 24;
                    col[o * 4] = 14, col[o * 4 + 1] = 14, col[o * 4 + 2] = 24;
                    continue;
                }
                const float px = static_cast<float>(view.x + static_cast<int>(i));
                const float py = static_cast<float>(view.y + static_cast<int>(j));
                const render::FlowSample& f = flows[o];
                const float sd = (static_cast<float>(depth[o]) - render::sea::kDepthBias) /
                                 render::sea::kDepthScale;
                const float slope = render::sea::slope_of(flags[o]);
                float light;
                bool legacy = false;
                if (f.valid >= scene.river_cfg.min_valid) {
                    const render::Shade sh =
                        render::river_flow_shade(px, py, f, sd, slope, t, detail, params, rv);
                    light = sh.light;
                    const render::Px c = render::shade_apply({50, 105, 140, 255}, sh);
                    col[o * 4] = c[0], col[o * 4 + 1] = c[1], col[o * 4 + 2] = c[2];
                } else {
                    const float pyw = py + raise_px;
                    light = render::river_shade(
                                {px / static_cast<float>(tw) + pyw / static_cast<float>(th),
                                 pyw / static_cast<float>(th) - px / static_cast<float>(tw)},
                                sd, slope, t)
                                .light;
                    legacy = true;
                }
                const float g = std::clamp(128.0F + light * 1500.0F, 0.0F, 255.0F);
                const auto gv = static_cast<std::uint8_t>(g);
                p[0] = legacy ? 255 : gv, p[1] = gv, p[2] = gv;
            }
        }
        char file[256];
        std::snprintf(file, sizeof file, "%s_g%zu_t%05llu_light.png", name.c_str(), group,
                      static_cast<unsigned long long>(t));
        const std::string err = anno::app::write_png(out / file, static_cast<std::uint32_t>(w),
                                                     static_cast<std::uint32_t>(h), img);
        if (!err.empty()) {
            std::fprintf(stderr, "%s\n", err.c_str());
            return false;
        }
        std::snprintf(file, sizeof file, "%s_g%zu_t%05llu_colour.png", name.c_str(), group,
                      static_cast<unsigned long long>(t));
        static_cast<void>(anno::app::write_png(out / file, static_cast<std::uint32_t>(w),
                                               static_cast<std::uint32_t>(h), col));
    }
    // Quiver: flow direction on screen (hue), speed (value), arrows.
    std::vector<std::uint8_t> img(w * h * 4, 255);
    for (std::size_t o = 0; o < w * h; ++o) {
        std::uint8_t* p = &img[o * 4];
        if ((flags[o] & 2U) == 0) {
            p[0] = 14, p[1] = 14, p[2] = 24;
            continue;
        }
        const render::FlowSample& f = flows[o];
        if (f.valid < scene.river_cfg.min_valid) {
            p[0] = 255, p[1] = 0, p[2] = 255;
            continue;
        }
        const float sx = (f.vx - f.vy) * static_cast<float>(tw) * 0.5F;
        const float sy = (f.vx + f.vy) * static_cast<float>(th) * 0.5F;
        const float speed = std::hypot(f.vx, f.vy);
        const float hue = (std::atan2(sy, sx) / 6.2831853F) + 0.5F;
        const Px4 c =
            hsv(std::clamp(hue, 0.0F, 0.9999F), 0.8F, std::clamp(0.35F + speed * 0.5F, 0.0F, 1.0F));
        p[0] = c.r, p[1] = c.g, p[2] = c.b;
    }
    auto plot = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= view.w || y >= view.h) {
            return;
        }
        std::uint8_t* p = &img[(static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)) * 4];
        p[0] = p[1] = p[2] = 255;
    };
    for (int y = 12; y < view.h; y += 24) {
        for (int x = 12; x < view.w; x += 24) {
            const std::size_t o = static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x);
            if ((flags[o] & 2U) == 0 || flows[o].valid < scene.river_cfg.min_valid) {
                continue;
            }
            const render::FlowSample& f = flows[o];
            const float sx = (f.vx - f.vy) * static_cast<float>(tw) * 0.5F;
            const float sy = (f.vx + f.vy) * static_cast<float>(th) * 0.5F;
            const float len = std::hypot(sx, sy);
            if (len < 1e-3F) {
                plot(x, y);
                continue;
            }
            const float scale = std::min(10.0F, len * 2.0F) / len;
            const float ex = sx * scale, ey = sy * scale;
            const int steps = static_cast<int>(std::ceil(std::hypot(ex, ey)));
            for (int k = 0; k <= steps; ++k) {
                const float a =
                    steps > 0 ? static_cast<float>(k) / static_cast<float>(steps) : 0.0F;
                plot(x + static_cast<int>(std::lround(ex * (a - 0.5F))),
                     y + static_cast<int>(std::lround(ey * (a - 0.5F))));
            }
            // Arrow head: two short strokes back from the tip.
            const float ang = std::atan2(ey, ex);
            for (const float da : {2.6F, -2.6F}) {
                for (int k = 1; k <= 3; ++k) {
                    plot(x + static_cast<int>(std::lround(ex * 0.5F + std::cos(ang + da) *
                                                                          static_cast<float>(k))),
                         y + static_cast<int>(std::lround(ey * 0.5F + std::sin(ang + da) *
                                                                          static_cast<float>(k))));
                }
            }
        }
    }
    // Extras (stage S7): turbulence in red, wake in green, the Froude number
    // (0 to 1.5) in blue, over the river pixels; legacy-shimmer pixels magenta.
    {
        std::vector<std::uint8_t> ex(w * h * 4, 255);
        for (std::size_t o = 0; o < w * h; ++o) {
            std::uint8_t* p = &ex[o * 4];
            if ((flags[o] & 2U) == 0) {
                p[0] = 14, p[1] = 14, p[2] = 24;
                continue;
            }
            const render::FlowSample& f = flows[o];
            if (f.valid < scene.river_cfg.min_valid) {
                p[0] = 255, p[1] = 0, p[2] = 255;
                continue;
            }
            const float sd =
                (static_cast<float>(depth[o]) - render::sea::kDepthBias) / render::sea::kDepthScale;
            const float head = render::kHeightUnitTiles * std::max(sd, scene.river_cfg.h_min);
            const float fr = std::hypot(f.vx, f.vy) / std::sqrt(scene.river_cfg.gravity * head);
            p[0] = static_cast<std::uint8_t>(std::clamp(f.turb, 0.0F, 1.0F) * 255.0F);
            p[1] = static_cast<std::uint8_t>(std::clamp(f.wake, 0.0F, 1.0F) * 255.0F);
            p[2] = static_cast<std::uint8_t>(std::clamp(fr / 1.5F, 0.0F, 1.0F) * 255.0F);
        }
        char xfile[256];
        std::snprintf(xfile, sizeof xfile, "%s_g%zu_extras.png", name.c_str(), group);
        const std::string xerr = anno::app::write_png(out / xfile, static_cast<std::uint32_t>(w),
                                                      static_cast<std::uint32_t>(h), ex);
        if (!xerr.empty()) {
            std::fprintf(stderr, "%s\n", xerr.c_str());
        }
    }
    char file[256];
    std::snprintf(file, sizeof file, "%s_g%zu_quiver.png", name.c_str(), group);
    const std::string err = anno::app::write_png(out / file, static_cast<std::uint32_t>(w),
                                                 static_cast<std::uint32_t>(h), img);
    if (!err.empty()) {
        std::fprintf(stderr, "%s\n", err.c_str());
        return false;
    }
    return true;
}
