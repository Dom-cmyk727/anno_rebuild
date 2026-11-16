// UI-MINIMAP-1 step B1: the minimap's data, image, palette, geometry and composition
// (minimap.hpp; docs/re/ui.md section 2.9).
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <numbers>
#include <sstream>

#include <anno/app/minimap.hpp>

namespace anno::app {

namespace {

std::vector<std::string_view> split_words(std::string_view line) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
            ++i;
        const std::size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
            ++i;
        if (i > start)
            out.push_back(line.substr(start, i - start));
    }
    return out;
}

template <typename T> bool parse_int(std::string_view s, T& out, int base = 10) {
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out, base);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size();
}

bool parse_double(std::string_view s, double& out) {
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc{} && r.ptr == s.data() + s.size() && std::isfinite(out);
}

/// The original's channel scale (AnnoFrame 0x1003396a..0x10033a0f): `pre` is the channel already
/// brought to the 5-bit step (R: (c + 4) << 7 >> 10 kept in the low bits, so the scale is applied
/// before the cut; B: (c + 4) >> 3 cut first), `factor` (k + shade_add), divided by shade_div
/// (the 0x8421085 multiply is floor division by 31), clamped to 31.
std::uint32_t scaled5(std::uint32_t numerator, std::uint32_t div) {
    return std::min<std::uint32_t>(numerator / div, 31U);
}

std::uint8_t expand5(std::uint32_t v) {
    return static_cast<std::uint8_t>((v * 255U + 15U) / 31U);
}

/// The 16-bit frame is RGB565: the texture's 5-bit green shows as 6 bits, (g << 1) | (g >> 4)
/// (the top bit repeated, the usual conversion; the GOG shot's land colours (58, 40, 0), (107,
/// 105, 0), (115, 113, 0), (132, 121, 0) are entries (9, 2), (9, 3), (9, 4), (9, 6) this way;
/// UI-MINIMAP-1 B1, docs/re/ui.md 2.9), expanded as (v * 255 + 31) / 63. OQ-628.
std::uint8_t expand5_as6(std::uint32_t v) {
    const std::uint32_t g6 = (v << 1U) | (v >> 4U);
    return static_cast<std::uint8_t>((g6 * 255U + 31U) / 63U);
}

} // namespace

formats::Result<MinimapConfig, std::string> parse_minimap_config(std::string_view text) {
    MinimapConfig cfg;
    bool versioned = false;
    bool colours_given = false, overrides_given = false;
    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        const std::size_t end = std::min(text.find('\n', pos), text.size());
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        ++line_no;
        if (const auto hash = line.find('#'); hash != std::string_view::npos)
            line = line.substr(0, hash);
        const auto w = split_words(line);
        if (w.empty()) {
            if (end == text.size())
                break;
            continue;
        }
        const auto bad = [&](const char* what) {
            return "minimap.txt line " + std::to_string(line_no) + ": " + what;
        };
        if (!versioned) {
            if (w.size() != 2 || w[0] != "anno1503-ui-minimap" || w[1] != "1")
                return bad("expected the version line `anno1503-ui-minimap 1`");
            versioned = true;
            continue;
        }
        const std::string_view key = w[0];
        const std::size_t n = w.size() - 1;
        const auto u32s = [&](std::initializer_list<std::uint32_t*> outs) {
            if (n != outs.size())
                return false;
            std::size_t i = 1;
            for (std::uint32_t* o : outs)
                if (!parse_int(w[i++], *o))
                    return false;
            return true;
        };
        const auto ints = [&](std::initializer_list<int*> outs) {
            if (n != outs.size())
                return false;
            std::size_t i = 1;
            for (int* o : outs)
                if (!parse_int(w[i++], *o))
                    return false;
            return true;
        };
        const auto doubles = [&](std::initializer_list<double*> outs) {
            if (n != outs.size())
                return false;
            std::size_t i = 1;
            for (double* o : outs)
                if (!parse_double(w[i++], *o))
                    return false;
            return true;
        };
        bool ok = false;
        if (key == "image") {
            ok = u32s({&cfg.image_w, &cfg.image_h});
        } else if (key == "shades") {
            ok = u32s({&cfg.shades});
        } else if (key == "transparent") {
            ok = u32s({&cfg.transparent});
        } else if (key == "shade_scale") {
            ok = u32s({&cfg.shade_add, &cfg.shade_div});
        } else if (key == "colour") {
            std::uint32_t index = 0, rgb = 0;
            ok = n == 2 && parse_int(w[1], index) && parse_int(w[2], rgb, 16) && rgb <= 0xffffffU;
            if (ok) {
                if (!colours_given) {
                    cfg.colours.clear();
                    colours_given = true;
                }
                // Indices in file order from 0, so a gap or a repeat is an error, not a guess.
                ok = index == cfg.colours.size();
                if (ok)
                    cfg.colours.push_back(rgb);
            }
        } else if (key == "override") {
            MinimapConfig::Override o;
            ok = n == 3 && parse_int(w[1], o.colour) && parse_int(w[2], o.shade) &&
                 parse_int(w[3], o.rgb, 16) && o.rgb <= 0xffffffU;
            if (ok) {
                if (!overrides_given) {
                    cfg.overrides.clear();
                    overrides_given = true;
                }
                cfg.overrides.push_back(o);
            }
        } else if (key == "no_owner") {
            ok = u32s({&cfg.no_owner});
        } else if (key == "shade_rule") {
            ok = u32s({&cfg.shade_window, &cfg.shade_round, &cfg.shade_shift}) &&
                 cfg.shade_shift < 32;
        } else if (key == "base") {
            ok = ints({&cfg.base});
        } else if (key == "rect") {
            ok = ints({&cfg.rect_dx, &cfg.rect_dy});
        } else if (key == "shift") {
            ok = ints({&cfg.shift_x, &cfg.shift_y});
        } else if (key == "diamond") {
            ok = doubles({&cfg.diamond_cx, &cfg.diamond_cy, &cfg.diamond_ax, &cfg.diamond_ay});
        } else if (key == "diamond_angle") {
            ok = doubles({&cfg.diamond_angle});
        } else if (key == "map_units") {
            ok = doubles({&cfg.map_units}) && cfg.map_units > 0.0;
        } else if (key == "parchment") {
            ok = n == 3 && parse_int(w[2], cfg.parchment_id) &&
                 parse_int(w[3], cfg.parchment_rows) && cfg.parchment_rows > 0;
            if (ok)
                cfg.parchment_file = std::string(w[1]);
        } else if (key == "quad_offset") {
            ok = ints({&cfg.quad_dx, &cfg.quad_dy});
        } else if (key == "view_box") {
            ok = n == 4 && parse_double(w[1], cfg.box_extent) && parse_double(w[2], cfg.box_skew) &&
                 parse_double(w[3], cfg.box_shift) && parse_int(w[4], cfg.box_colour, 16) &&
                 cfg.box_colour <= 0xffffffU;
        } else if (key == "zoom_start") {
            ok = doubles({&cfg.zoom_start}) && cfg.zoom_start > 0.0 && cfg.zoom_start <= 1.0;
        } else {
            return bad("unknown key");
        }
        if (!ok)
            return bad("bad values");
        if (end == text.size())
            break;
    }
    if (!versioned)
        return std::string("minimap.txt: missing the version line `anno1503-ui-minimap 1`");
    if (cfg.image_w == 0 || cfg.image_h == 0 || cfg.shades == 0 || cfg.shade_div == 0 ||
        cfg.shade_window == 0 || cfg.colours.empty() || cfg.base <= 0)
        return std::string("minimap.txt: image, shades, shade_scale, shade_window, base and "
                           "the colours must not be zero or empty");
    if (cfg.no_owner >= cfg.colours.size())
        return std::string("minimap.txt: no_owner names no colour");
    // The image stores owner * shades + shade in 16 bits.
    if (std::uint64_t{cfg.colours.size()} * cfg.shades > 0x10000U)
        return std::string("minimap.txt: colours x shades exceeds the image's 16-bit cells");
    return cfg;
}

formats::Result<MinimapConfig, std::string> load_minimap_config(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "cannot read " + path.string();
    std::ostringstream text;
    text << in.rdbuf();
    return parse_minimap_config(text.str());
}

std::vector<std::uint16_t> build_minimap_image(const MinimapConfig& cfg,
                                               const MinimapWorld& world) {
    if (world.w == 0 || world.h == 0 || cfg.image_w == 0 || cfg.image_h == 0)
        return {};
    // The world's heights and owners as full grids (islands never overlap; the sea is height 0,
    // no owner): the same lookup the original makes per tile through the world's island query
    // (vfuncs +0x5c and +0x8c), done once.
    const std::size_t ww = world.w, wh = world.h;
    std::vector<std::uint8_t> height(ww * wh, 0);
    std::vector<std::uint32_t> owner(ww * wh, cfg.no_owner);
    for (const MinimapIsland& is : world.islands) {
        for (std::uint32_t ty = 0; ty < is.h; ++ty) {
            const std::uint64_t wy = std::uint64_t{is.y} + ty;
            if (wy >= wh)
                break;
            for (std::uint32_t tx = 0; tx < is.w; ++tx) {
                const std::uint64_t wx = std::uint64_t{is.x} + tx;
                if (wx >= ww)
                    break;
                const std::size_t src = std::size_t{ty} * is.w + tx;
                const std::size_t dst = static_cast<std::size_t>(wy * ww + wx);
                if (src < is.heights.size())
                    height[dst] = is.heights[src];
                if (src < is.owners.size())
                    owner[dst] = is.owners[src];
            }
        }
    }
    // Per-row prefix sums of the heights, so a window's sum is O(rows).
    std::vector<std::uint64_t> prefix(wh * (ww + 1), 0);
    for (std::size_t y = 0; y < wh; ++y) {
        std::uint64_t run = 0;
        for (std::size_t x = 0; x < ww; ++x) {
            run += height[y * ww + x];
            prefix[y * (ww + 1) + x + 1] = run;
        }
    }
    // sx, sy as the original stores them: float (fstp dword, AnnoWelt 0x100a3d05/0x100a3d10).
    const float sxf = static_cast<float>(world.w) / static_cast<float>(cfg.image_w);
    const float syf = static_cast<float>(world.h) / static_cast<float>(cfg.image_h);
    const double sx = sxf, sy = syf;
    const double nominal = sx * sy * static_cast<double>(cfg.shade_window * cfg.shade_window);
    const auto clamp_tile = [](double v, std::size_t limit) {
        const double t = std::trunc(v);
        return t <= 0.0 ? std::size_t{0}
                        : std::min(static_cast<std::size_t>(t), limit); // truncation, _ftol
    };
    std::vector<std::uint16_t> image(std::size_t{cfg.image_w} * cfg.image_h, 0);
    for (std::uint32_t y = 0; y < cfg.image_h; ++y) {
        const std::size_t ty0 = clamp_tile(static_cast<double>(y) * sy, wh);
        const std::size_t ty1 = clamp_tile(static_cast<double>(y + cfg.shade_window) * sy, wh);
        const std::size_t oy =
            std::min(clamp_tile((static_cast<double>(y) + 0.5) * sy, wh), wh - 1);
        for (std::uint32_t x = 0; x < cfg.image_w; ++x) {
            const std::size_t tx0 = clamp_tile(static_cast<double>(x) * sx, ww);
            const std::size_t tx1 = clamp_tile(static_cast<double>(x + cfg.shade_window) * sx, ww);
            std::uint64_t sum = 0;
            if (tx1 > tx0) {
                for (std::size_t ty = ty0; ty < ty1; ++ty)
                    sum += prefix[ty * (ww + 1) + tx1] - prefix[ty * (ww + 1) + tx0];
            }
            const auto v =
                static_cast<std::uint64_t>(std::trunc(static_cast<double>(sum) / nominal));
            const std::uint32_t shade =
                (static_cast<std::uint32_t>((v & 0xffU) + cfg.shade_round) >> cfg.shade_shift) %
                cfg.shades;
            const std::size_t ox =
                std::min(clamp_tile((static_cast<double>(x) + 0.5) * sx, ww), ww - 1);
            std::uint32_t own = owner[oy * ww + ox];
            if (own >= cfg.colours.size())
                own = cfg.no_owner;
            image[std::size_t{y} * cfg.image_w + x] =
                static_cast<std::uint16_t>(own * cfg.shades + shade);
        }
    }
    return image;
}

std::vector<std::array<std::uint8_t, 4>> minimap_palette(const MinimapConfig& cfg) {
    std::vector<std::array<std::uint8_t, 4>> out(cfg.colours.size() * cfg.shades,
                                                 std::array<std::uint8_t, 4>{0, 0, 0, 0});
    for (std::uint32_t c = 0; c < cfg.colours.size(); ++c) {
        for (std::uint32_t k = cfg.transparent; k < cfg.shades; ++k) {
            std::uint32_t rgb = cfg.colours[c];
            for (const auto& o : cfg.overrides)
                if (o.colour == c && o.shade == k)
                    rgb = o.rgb;
            const std::uint32_t factor = k + cfg.shade_add;
            const std::uint32_t r8 = (rgb >> 16) & 0xffU, g8 = (rgb >> 8) & 0xffU, b8 = rgb & 0xffU;
            // R: ((rgb & 0xff0000) + 0x40000) >> 9 = (R + 4) << 7, scaled, then bits 10..14.
            const std::uint32_t r5 =
                std::min<std::uint32_t>(((r8 + 4U) << 7U) * factor / cfg.shade_div, 0x7c00U) >> 10U;
            // G: ((rgb & 0xff00) + 0x400) >> 6 = (G + 4) << 2, scaled, then bits 5..9.
            const std::uint32_t g5 =
                std::min<std::uint32_t>(((g8 + 4U) << 2U) * factor / cfg.shade_div, 0x3e0U) >> 5U;
            // B: ((rgb & 0xff) + 4) >> 3, scaled, clamped to 31.
            const std::uint32_t b5 = scaled5(((b8 + 4U) >> 3U) * factor, cfg.shade_div);
            out[std::size_t{c} * cfg.shades + k] = {expand5(r5), expand5_as6(g5), expand5(b5), 255};
        }
    }
    return out;
}

MinimapRect minimap_rect(const MinimapConfig& cfg, int ui_w) {
    MinimapRect r;
    r.x = ui_w - cfg.base / 4 + cfg.rect_dx + cfg.shift_x;
    r.y = cfg.rect_dy + cfg.shift_y;
    r.w = cfg.base / 4;
    r.h = cfg.base / 8;
    r.s = 1.0;
    return r;
}

MinimapWindow minimap_window(double zoom, double cu, double cv) {
    const double half = zoom * 0.5;
    MinimapWindow w{cu - half, cv - half, cu + half, cv + half};
    if (w.u0 < 0.0) {
        w.u0 = 0.0;
        w.u1 = zoom;
    }
    if (w.v0 < 0.0) {
        w.v0 = 0.0;
        w.v1 = zoom;
    }
    if (w.u1 > 1.0) {
        w.u1 = 1.0;
        w.u0 = 1.0 - zoom;
    }
    if (w.v1 > 1.0) {
        w.v1 = 1.0;
        w.v0 = 1.0 - zoom;
    }
    return w;
}

std::pair<double, double> minimap_screen(const MinimapConfig& cfg, const MinimapRect& rect,
                                         const MinimapWindow& window, double u, double v) {
    const double lu = window.u0 * cfg.map_units, hu = window.u1 * cfg.map_units;
    const double lv = window.v0 * cfg.map_units, hv = window.v1 * cfg.map_units;
    const double mu = (hu - lu) * 0.5 + lu, mv = (hv - lv) * 0.5 + lv;
    const double du = (u - mu) / ((hu - lu) * 0.5);
    const double dv = (v - mv) / ((hv - lv) * 0.5);
    const double a = cfg.diamond_angle * std::numbers::pi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    return {rect.x + rect.s * (cfg.diamond_cx - cfg.diamond_ax * (c * dv - s * du)),
            rect.y + rect.s * (cfg.diamond_cy + cfg.diamond_ay * (c * du + s * dv))};
}

namespace {

/// Cohen-Sutherland against [lu, hu] x [lv, hv] (AnnoFrame 0x1002fb70: bit 1 above the high u, 2
/// below the low u, 4 above the high v, 8 below the low v; a point on an edge is inside).
bool clip_segment(double& u0, double& v0, double& u1, double& v1, double lu, double lv, double hu,
                  double hv) {
    const auto code = [&](double u, double v) {
        unsigned c = 0;
        if (u > hu)
            c |= 1U;
        else if (u < lu)
            c |= 2U;
        if (v > hv)
            c |= 4U;
        else if (v < lv)
            c |= 8U;
        return c;
    };
    unsigned c0 = code(u0, v0), c1 = code(u1, v1);
    for (int guard = 0; guard < 8; ++guard) {
        if ((c0 | c1) == 0)
            return true;
        if ((c0 & c1) != 0)
            return false;
        const unsigned out = c0 != 0 ? c0 : c1;
        double u = 0, v = 0;
        if (out & 1U) {
            v = v0 + (v1 - v0) * (hu - u0) / (u1 - u0);
            u = hu;
        } else if (out & 2U) {
            v = v0 + (v1 - v0) * (lu - u0) / (u1 - u0);
            u = lu;
        } else if (out & 4U) {
            u = u0 + (u1 - u0) * (hv - v0) / (v1 - v0);
            v = hv;
        } else {
            u = u0 + (u1 - u0) * (lv - v0) / (v1 - v0);
            v = lv;
        }
        if (out == c0) {
            u0 = u;
            v0 = v;
            c0 = code(u0, v0);
        } else {
            u1 = u;
            v1 = v;
            c1 = code(u1, v1);
        }
    }
    return (c0 | c1) == 0;
}

} // namespace

std::vector<MinimapSegment> minimap_view_box(const MinimapConfig& cfg, const MinimapRect& rect,
                                             const MinimapWindow& window, double camera_wx,
                                             double camera_wy, std::uint32_t world_w,
                                             std::uint32_t world_h, double level_factor) {
    std::vector<MinimapSegment> out;
    if (world_w < 2 || world_h == 0)
        return out;
    const double units = cfg.map_units;
    const double e = level_factor * cfg.box_extent / static_cast<double>(world_w);
    const double f = cfg.box_shift / static_cast<double>(world_w / 2U);
    const double g = -cfg.box_skew * e; // turn 0 (0x1002ffd3: the turn-0 case negates e)
    const double pu = units * camera_wy / static_cast<double>(world_h) + f;
    const double pv = units * camera_wx / static_cast<double>(world_w) + f;
    const std::array<std::pair<double, double>, 4> corners = {
        std::pair{pu - g, pv + e}, std::pair{pu + e, pv - g}, std::pair{pu + g, pv - e},
        std::pair{pu - e, pv + g}};
    const double lu = window.u0 * units, hu = window.u1 * units;
    const double lv = window.v0 * units, hv = window.v1 * units;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        double u0 = corners[i].first, v0 = corners[i].second;
        double u1 = corners[(i + 1) % 4].first, v1 = corners[(i + 1) % 4].second;
        if (!clip_segment(u0, v0, u1, v1, lu, lv, hu, hv))
            continue;
        const auto [x0, y0] = minimap_screen(cfg, rect, window, u0, v0);
        const auto [x1, y1] = minimap_screen(cfg, rect, window, u1, v1);
        out.push_back({x0, y0, x1, y1});
    }
    return out;
}

std::vector<std::pair<int, int>> minimap_line_pixels(const std::vector<MinimapSegment>& segments) {
    std::vector<std::pair<int, int>> out;
    for (const MinimapSegment& s : segments) {
        const int x0 = static_cast<int>(std::floor(s.x0 + 0.5));
        const int y0 = static_cast<int>(std::floor(s.y0 + 0.5));
        const int x1 = static_cast<int>(std::floor(s.x1 + 0.5));
        const int y1 = static_cast<int>(std::floor(s.y1 + 0.5));
        // Bresenham between the two end pixels, both included.
        const int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
        const int stepx = x0 < x1 ? 1 : -1, stepy = y0 < y1 ? 1 : -1;
        int err = dx + dy, x = x0, y = y0;
        for (;;) {
            out.emplace_back(x, y);
            if (x == x1 && y == y1)
                break;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x += stepx;
            }
            if (e2 <= dx) {
                err += dx;
                y += stepy;
            }
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

assets::Rgba compose_minimap(const MinimapConfig& cfg, const MinimapRect& rect,
                             const MinimapWindow& window, const std::vector<std::uint16_t>& image,
                             const std::vector<std::array<std::uint8_t, 4>>& palette,
                             const assets::Rgba* parchment) {
    assets::Rgba out;
    const int w = rect.w + std::max(0, cfg.quad_dx);
    const int h = rect.h + std::max(0, cfg.quad_dy);
    if (w <= 0 || h <= 0 || w > 0xFFFF || h > 0xFFFF)
        return out;
    out.width = static_cast<std::uint16_t>(w);
    out.height = static_cast<std::uint16_t>(h);
    out.pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0);
    const auto put = [&](int x, int y, const std::uint8_t* rgba) {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        const std::size_t d = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                               static_cast<std::size_t>(x)) *
                              4;
        std::copy_n(rgba, 4, out.pixels.begin() + static_cast<std::ptrdiff_t>(d));
    };
    // (1) The parchment, its first parchment_rows rows over the rectangle's height, no blending:
    // its transparent pixels are left out (the GOG shot shows the world there, left of the
    // chrome).
    if (parchment != nullptr && parchment->width > 0 && parchment->height > 0 && rect.h > 0) {
        const int rows = std::min<int>(cfg.parchment_rows, parchment->height);
        const int cols = std::min<int>(rect.w, parchment->width);
        for (int ry = 0; ry < rect.h; ++ry) {
            // Linear between the two rows around the pixel centre's source row.
            const double src = std::max(0.0, (static_cast<double>(ry) + 0.5) * rows / rect.h - 0.5);
            const int sy0 = std::min(rows - 1, static_cast<int>(src));
            const int sy1 = std::min(rows - 1, sy0 + 1);
            const double t = src - sy0;
            for (int rx = 0; rx < cols; ++rx) {
                const std::size_t a = (static_cast<std::size_t>(sy0) * parchment->width +
                                       static_cast<std::size_t>(rx)) *
                                      4;
                const std::size_t b = (static_cast<std::size_t>(sy1) * parchment->width +
                                       static_cast<std::size_t>(rx)) *
                                      4;
                // Premultiplied, so a transparent neighbour never darkens the edge; a pixel
                // under half covered is left out (the parchment is drawn without blending).
                const double wa = (1.0 - t) * parchment->pixels[a + 3] / 255.0;
                const double wb = t * parchment->pixels[b + 3] / 255.0;
                if (wa + wb < 0.5)
                    continue;
                std::array<std::uint8_t, 4> px{};
                for (std::size_t c = 0; c < 3; ++c)
                    px[c] = static_cast<std::uint8_t>(std::lround(
                        (wa * parchment->pixels[a + c] + wb * parchment->pixels[b + c]) /
                        (wa + wb)));
                px[3] = 255;
                put(rx + cfg.quad_dx, ry + cfg.quad_dy, px.data());
            }
        }
    }
    // (2) The map, at the same offset as the parchment: each pixel inside the window's diamond
    // takes the cell under it.
    if (image.size() != std::size_t{cfg.image_w} * cfg.image_h || rect.s <= 0.0)
        return out;
    const double a = cfg.diamond_angle * std::numbers::pi / 180.0;
    const double c = std::cos(a), s = std::sin(a);
    for (int py = 0; py < h; ++py) {
        const double yy = static_cast<double>(py - cfg.quad_dy) / rect.s - cfg.diamond_cy;
        const double q = yy / cfg.diamond_ay;
        for (int px = 0; px < w; ++px) {
            const double xx = static_cast<double>(px - cfg.quad_dx) / rect.s - cfg.diamond_cx;
            const double p = -xx / cfg.diamond_ax;
            const double du = c * q - s * p;
            const double dv = s * q + c * p;
            if (du < -1.0 || du >= 1.0 || dv < -1.0 || dv >= 1.0)
                continue;
            const double fu = window.u0 + (du + 1.0) * 0.5 * (window.u1 - window.u0);
            const double fv = window.v0 + (dv + 1.0) * 0.5 * (window.v1 - window.v0);
            const auto row = std::min<std::size_t>(
                static_cast<std::size_t>(std::max(0.0, fu * cfg.image_h)), cfg.image_h - 1);
            const auto col = std::min<std::size_t>(
                static_cast<std::size_t>(std::max(0.0, fv * cfg.image_w)), cfg.image_w - 1);
            const std::uint16_t value = image[row * cfg.image_w + col];
            if (value >= palette.size() || palette[value][3] == 0)
                continue;
            put(px, py, palette[value].data());
        }
    }
    return out;
}

} // namespace anno::app
