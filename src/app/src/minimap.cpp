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

/// A 0xRRGGBB colour as the 16-bit RGB565 frame shows it (5, 6, 5 bits, expanded as above): the
/// GOG shot `construction_open` shows the own-ship marker 0x00d000 as (0, 210, 0).
std::uint32_t as_shown_565(std::uint32_t rgb) {
    const std::uint32_t r = expand5(((rgb >> 16) & 0xffU) >> 3U);
    const std::uint32_t g6 = ((rgb >> 8) & 0xffU) >> 2U;
    const std::uint32_t g = (g6 * 255U + 31U) / 63U;
    const std::uint32_t b = expand5((rgb & 0xffU) >> 3U);
    return (r << 16U) | (g << 8U) | b;
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
        } else if (key == "parchment_quad") {
            ok = ints({&cfg.parchment_x0, &cfg.parchment_y0, &cfg.parchment_x1,
                       &cfg.parchment_y1}) &&
                 cfg.parchment_x0 >= 0 && cfg.parchment_y0 >= 0;
        } else if (key == "colour_depth") {
            ok = ints({&cfg.colour_depth}) && (cfg.colour_depth == 32 || cfg.colour_depth == 16);
        } else if (key == "view_box") {
            ok = n == 4 && parse_double(w[1], cfg.box_extent) && parse_double(w[2], cfg.box_skew) &&
                 parse_double(w[3], cfg.box_shift) && parse_int(w[4], cfg.box_colour, 16) &&
                 cfg.box_colour <= 0xffffffU;
        } else if (key == "zoom_start") {
            ok = doubles({&cfg.zoom_start}) && cfg.zoom_start > 0.0 && cfg.zoom_start <= 1.0;
        } else if (key == "zoom_range") {
            ok = doubles({&cfg.zoom_min, &cfg.zoom_max, &cfg.zoom_step}) && cfg.zoom_min > 0.0 &&
                 cfg.zoom_min <= cfg.zoom_max && cfg.zoom_max <= 1.0 && cfg.zoom_step > 0.0;
        } else if (key == "click") {
            ok = n == 3 && parse_int(w[1], cfg.click_dx) && parse_int(w[2], cfg.click_dy) &&
                 parse_double(w[3], cfg.click_scale) && cfg.click_scale > 0.0;
        } else if (key == "owner_rule") {
            std::uint32_t all = 0;
            ok = u32s({&cfg.no_colour, &all}) && all <= 1;
            cfg.all_visible = all == 1;
        } else if (key == "marker_colours") {
            ok = n == 4 && parse_int(w[1], cfg.marker_list0, 16) &&
                 parse_int(w[2], cfg.marker_list1, 16) && parse_int(w[3], cfg.marker_flagged, 16) &&
                 parse_int(w[4], cfg.marker_others, 16) && cfg.marker_list0 <= 0xffffffU &&
                 cfg.marker_list1 <= 0xffffffU && cfg.marker_flagged <= 0xffffffU &&
                 cfg.marker_others <= 0xffffffU;
        } else if (key == "marker_flags") {
            ok = n == 3 && parse_int(w[1], cfg.marker_flag, 16) &&
                 parse_int(w[2], cfg.marker_skip_mask, 16) &&
                 parse_int(w[3], cfg.marker_skip_value, 16);
        } else if (key == "marker_square") {
            ok = n == 2 && parse_int(w[1], cfg.marker_half) && parse_int(w[2], cfg.marker_max) &&
                 cfg.marker_half >= 0 && cfg.marker_half <= 64;
        } else if (key == "marker_others") {
            std::uint32_t shown = 0;
            ok = u32s({&shown}) && shown <= 1;
            cfg.marker_others_shown = shown == 1;
        } else if (key == "alerts") {
            ok = u32s({&cfg.alert_max});
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

std::uint32_t minimap_city_owner(const MinimapConfig& cfg, const TerritoryCity& city,
                                 const MinimapOwnerRule& rule) {
    // AnnoWelt 0x100a404e..0x100a405e: the city's +0xc8(0) mask (+0x61c) against 1 << viewer, or
    // every city when world property 5 is 0 (the mask is then 0xffffffff, 0x100a3f0d).
    const bool known = cfg.all_visible ||
                       std::binary_search(city.known_by.begin(), city.known_by.end(), rule.viewer);
    if (!known) {
        return cfg.no_owner;
    }
    // 0x100a4062..0x100a406c: the city's +0xc8(2), the colour byte +0x16 (0x1002df94), set from
    // the owner's property 11 with 0xff stored as 0 (0x1002f42b..0x1002f439).
    if (city.owner < rule.colour_of_player.size() && rule.colour_of_player[city.owner]) {
        return *rule.colour_of_player[city.owner];
    }
    return cfg.no_colour;
}

std::vector<MinimapOwnerLayer> minimap_owner_layers(const MinimapConfig& cfg,
                                                    const TerritoryView& territory,
                                                    const MinimapOwnerRule& rule) {
    std::vector<MinimapOwnerLayer> out;
    out.reserve(territory.islands.size());
    for (const TerritoryIsland& is : territory.islands) {
        MinimapOwnerLayer layer;
        layer.x = is.x0;
        layer.y = is.y0;
        layer.w = is.w;
        layer.h = is.h;
        // One answer per city, then a table lookup per tile (the original asks per pixel).
        std::vector<std::pair<std::uint32_t, std::uint32_t>> by_number;
        by_number.reserve(is.cities.size());
        for (const TerritoryCity& c : is.cities) {
            by_number.emplace_back(c.number, minimap_city_owner(cfg, c, rule));
        }
        std::sort(by_number.begin(), by_number.end());
        layer.owners.assign(is.owner.size(), cfg.no_owner);
        for (std::size_t i = 0; i < is.owner.size(); ++i) {
            const std::uint32_t v = is.owner[i];
            if (v == territory.neutral_owner) {
                continue;
            }
            const auto it = std::lower_bound(by_number.begin(), by_number.end(), v,
                                             [](const std::pair<std::uint32_t, std::uint32_t>& e,
                                                std::uint32_t k) { return e.first < k; });
            if (it != by_number.end() && it->first == v) {
                layer.owners[i] = it->second;
            }
        }
        out.push_back(std::move(layer));
    }
    return out;
}

std::vector<std::uint16_t> build_minimap_image(const MinimapConfig& cfg, const MinimapWorld& world,
                                               const std::vector<MinimapOwnerLayer>* owners) {
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
    // B2: the published owners over the islands' own (the same world tiles; a layer may reach
    // outside the world: those tiles are dropped).
    if (owners != nullptr) {
        for (const MinimapOwnerLayer& layer : *owners) {
            for (std::uint32_t ty = 0; ty < layer.h; ++ty) {
                const std::int64_t wy = layer.y + ty;
                if (wy < 0 || wy >= static_cast<std::int64_t>(wh)) {
                    continue;
                }
                for (std::uint32_t tx = 0; tx < layer.w; ++tx) {
                    const std::int64_t wx = layer.x + tx;
                    const std::size_t src = std::size_t{ty} * layer.w + tx;
                    if (wx < 0 || wx >= static_cast<std::int64_t>(ww) ||
                        src >= layer.owners.size()) {
                        continue;
                    }
                    owner[static_cast<std::size_t>(wy) * ww + static_cast<std::size_t>(wx)] =
                        layer.owners[src];
                }
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
            if (cfg.colour_depth != 16) {
                // UI-MINIMAP-1 fix F2: the History Edition's 32-bit entries, each 8-bit channel
                // scaled and floored (its lossless capture's land colours, docs/re/ui.md 2.9).
                const auto ch = [&](std::uint32_t v) {
                    return static_cast<std::uint8_t>(
                        std::min<std::uint32_t>(v * factor / cfg.shade_div, 255U));
                };
                out[std::size_t{c} * cfg.shades + k] = {ch(r8), ch(g8), ch(b8), 255};
                continue;
            }
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

/// The point relative to the wrapper's rectangle (AnnoFrame 0x10002970 / 0x10002a10: the
/// rectangle the frame passed, before the renderer's shift 0x10031e3b..0x10031e41).
std::pair<int, int> minimap_click_rel(const MinimapConfig& cfg, const MinimapRect& rect, int x,
                                      int y) {
    return {x - (rect.x - cfg.shift_x), y - (rect.y - cfg.shift_y)};
}

/// AnnoFrame 0x100332f0 on the relative point.
bool minimap_hit_rel(const MinimapRect& rect, int xr, int yr) {
    const double w = rect.w, h = rect.h;
    if (xr < 0 || !(xr < w) || yr < 0 || !(yr < h))
        return false;
    const double y = yr;
    const double t = h * 0.5 < y ? h - y : y;
    const double dx = static_cast<double>(xr) - w * 0.5;
    return dx >= -2.0 * t && dx < 2.0 * t;
}

} // namespace

bool minimap_hit(const MinimapConfig& cfg, const MinimapRect& rect, int x, int y) {
    const auto [xr, yr] = minimap_click_rel(cfg, rect, x, y);
    return minimap_hit_rel(rect, xr, yr);
}

std::optional<std::pair<std::int64_t, std::int64_t>>
minimap_world_at(const MinimapConfig& cfg, const MinimapRect& rect, const MinimapWindow& window,
                 std::uint32_t world_w, std::uint32_t world_h, int x, int y) {
    const auto [xr, yr] = minimap_click_rel(cfg, rect, x, y);
    if (!minimap_hit_rel(rect, xr, yr) || world_w == 0 || world_h == 0 || cfg.image_w == 0 ||
        cfg.image_h == 0)
        return std::nullopt;
    const double w = rect.w, h = rect.h;
    const double sx = static_cast<double>(xr + cfg.click_dx) * cfg.click_scale;
    const double sy = static_cast<double>(yr + cfg.click_dy) * cfg.click_scale;
    const double d = sx - w * 0.5;
    const double n = cfg.image_h;
    // 0x1003347e..0x100334ce: q = h / (2n), r = h / n, P = (sy r + q d) / (2 r q), Q = (sy r - q
    // d) / (2 r q), i.e. n / h * (sy +- d / 2).
    const double p = n / h * (sy + d * 0.5);
    const double q = n / h * (sy - d * 0.5);
    const double ww = world_w, wh = world_h;
    // Turn 0 (0x100334e1..0x100334ff).
    const double x0 = std::trunc(ww * q / static_cast<double>(cfg.image_w));
    const double y0 = std::trunc(wh * p / n);
    // Through the zoom window (0x1003357e..0x10033685): the v span for x, the u span for y.
    const double mid_v = (window.v1 - window.v0) * 0.5 + window.v0;
    const double mid_u = (window.u1 - window.u0) * 0.5 + window.u0;
    const double wx = std::trunc(((x0 / ww - 0.5) * (window.v1 - window.v0) + mid_v) * ww);
    const double wy = std::trunc(((y0 / wh - 0.5) * (window.u1 - window.u0) + mid_u) * wh);
    return std::pair{static_cast<std::int64_t>(wx), static_cast<std::int64_t>(wy)};
}

MinimapRect minimap_click_area(const MinimapConfig& cfg, int ui_w) {
    const MinimapRect r = minimap_rect(cfg, ui_w);
    const int x0 = std::min(r.x + std::min(0, cfg.quad_dx), r.x - cfg.shift_x);
    const int y0 = std::min(r.y + std::min(0, cfg.quad_dy), r.y - cfg.shift_y);
    // The drawn raster (compose_minimap: both quads) and the wrapper's rectangle together.
    const int x1 =
        std::max(r.x + r.w + std::max({0, cfg.quad_dx, cfg.parchment_x1}), r.x - cfg.shift_x + r.w);
    const int y1 =
        std::max(r.y + r.h + std::max({0, cfg.quad_dy, cfg.parchment_y1}), r.y - cfg.shift_y + r.h);
    MinimapRect area = r;
    area.x = x0;
    area.y = y0;
    area.w = x1 - x0;
    area.h = y1 - y0;
    return area;
}

double minimap_zoom_step(const MinimapConfig& cfg, double zoom, bool closer) {
    // The original keeps z, the step and the bounds as floats (AnnoFrame +0x18, 0x10087bd4,
    // 0x10086310); x87 rounds each stored sum to float, which float arithmetic does as well.
    const auto z = static_cast<float>(zoom);
    const auto step = static_cast<float>(cfg.zoom_step);
    float next = closer ? z - step : z + step;
    const auto lo = static_cast<float>(cfg.zoom_min), hi = static_cast<float>(cfg.zoom_max);
    if (next < lo)
        next = lo;
    else if (next > hi)
        next = hi;
    return static_cast<double>(next);
}

namespace {

/// A stage within this relative distance of a tile width is that width: the stage the view is
/// at, or (magnifier_stages) a halving that is the wheel's end itself.
constexpr double kMagnifierSame = 1e-6;

} // namespace

std::optional<double> magnifier_target(const std::vector<double>& level_tile_w, double current,
                                       bool closer) {
    std::optional<double> best;
    for (const double t : level_tile_w) {
        if (!(t > 0.0))
            continue;
        if (closer && t > current * (1.0 + kMagnifierSame) && (!best || t < *best))
            best = t;
        if (!closer && t < current * (1.0 - kMagnifierSame) && (!best || t > *best))
            best = t;
    }
    return best;
}

std::vector<double> magnifier_stages(const std::vector<double>& built_tile_w, double min_tile) {
    std::vector<double> stages = built_tile_w;
    std::optional<double> narrowest;
    for (const double t : built_tile_w) {
        if (t > 0.0 && std::isfinite(t) && (!narrowest || t < *narrowest))
            narrowest = t;
    }
    // No extra stages without a built level, or when the wheel ends at or above the narrowest
    // one (a small scene, a large window): the built levels alone.
    if (!narrowest || !(min_tile > 0.0) || !std::isfinite(min_tile) ||
        !(min_tile < *narrowest * (1.0 - kMagnifierSame)))
        return stages;
    // min_tile > 0 and the factor > 1: the width falls below min_tile after finitely many steps.
    static_assert(kMagnifierStageFactor > 1.0);
    for (double w = *narrowest / kMagnifierStageFactor; w > min_tile * (1.0 + kMagnifierSame);
         w /= kMagnifierStageFactor)
        stages.push_back(w);
    stages.push_back(min_tile);
    return stages;
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

double minimap_level_factor(const std::vector<double>& level_tile_w, double shown_w) {
    const std::size_t n = level_tile_w.size();
    if (n == 0 || !(shown_w > 0.0)) {
        return 1.0;
    }
    if (shown_w >= level_tile_w.front() || n == 1) {
        return level_tile_w.front() / shown_w;
    }
    for (std::size_t k = 0; k + 1 < n; ++k) {
        const double a = level_tile_w[k], b = level_tile_w[k + 1];
        if (shown_w <= a && shown_w >= b && a > b) {
            const double t = std::log(a / shown_w) / std::log(a / b);
            return std::exp2(static_cast<double>(k) + t);
        }
    }
    return std::exp2(static_cast<double>(n - 1)) * level_tile_w.back() / shown_w;
}

std::vector<MinimapMarker> minimap_markers(const MinimapConfig& cfg, const MinimapRect& rect,
                                           const MinimapWindow& window,
                                           const std::vector<MapFigure>& figures,
                                           std::uint32_t world_w, std::uint32_t world_h,
                                           std::uint32_t viewer) {
    std::vector<MinimapMarker> out;
    if (world_w == 0 || world_h == 0) {
        return out;
    }
    const double lu = window.u0 * cfg.map_units, hu = window.u1 * cfg.map_units;
    const double lv = window.v0 * cfg.map_units, hv = window.v1 * cfg.map_units;
    const auto add = [&](const MapFigure& f, std::uint32_t rgb) {
        if (out.size() >= cfg.marker_max) {
            return;
        }
        const double u = cfg.map_units * f.y / static_cast<double>(world_h);
        const double v = cfg.map_units * f.x / static_cast<double>(world_w);
        if (!(u >= lu && u <= hu && v >= lv && v <= hv)) {
            return; // 0x100306f0 answers 0 outside the window
        }
        const auto [sx, sy] = minimap_screen(cfg, rect, window, u, v);
        out.push_back({static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)),
                       cfg.colour_depth == 16 ? as_shown_565(rgb) : rgb});
    };
    const auto own_colour = [&](const MapFigure& f, std::uint32_t plain) {
        return (f.flags & cfg.marker_flag) != 0 ? cfg.marker_flagged : plain;
    };
    // The original's order (0x10002e48..0x100030cd): own list 1, own list 0, the others' list 1.
    for (const MapFigure& f : figures) {
        if (f.owner == viewer && f.list == 1) {
            add(f, own_colour(f, cfg.marker_list1));
        }
    }
    for (const MapFigure& f : figures) {
        if (f.owner == viewer && f.list == 0 &&
            (f.flags & cfg.marker_skip_mask) != cfg.marker_skip_value) {
            add(f, own_colour(f, cfg.marker_list0));
        }
    }
    if (cfg.marker_others_shown) {
        for (const MapFigure& f : figures) {
            if (f.owner != viewer && f.list == 1) {
                add(f, cfg.marker_others);
            }
        }
    }
    return out;
}

std::vector<MinimapPixel> minimap_marker_pixels(const MinimapConfig& cfg,
                                                const std::vector<MinimapMarker>& markers) {
    std::vector<MinimapPixel> out;
    const int h = cfg.marker_half;
    for (const MinimapMarker& m : markers) {
        for (int dy = -h; dy <= h; ++dy) {
            for (int dx = -h; dx <= h; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) == h) {
                    out.push_back({m.x + dx, m.y + dy, m.rgb});
                }
            }
        }
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
    const int w = rect.w + std::max({0, cfg.quad_dx, cfg.parchment_x1});
    const int h = rect.h + std::max({0, cfg.quad_dy, cfg.parchment_y1});
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
    // (1) The parchment quad (MinimapConfig::parchment_x0..y1), its first parchment_rows rows,
    // sampled bilinearly, no blending: its transparent pixels are left out (the GOG shot shows
    // the world there, left of the chrome).
    const int qw = rect.w + cfg.parchment_x1 - cfg.parchment_x0;
    const int qh = rect.h + cfg.parchment_y1 - cfg.parchment_y0;
    if (parchment != nullptr && parchment->width > 0 && parchment->height > 0 && qw > 0 && qh > 0) {
        const int rows = std::min<int>(cfg.parchment_rows, parchment->height);
        const int cols = parchment->width;
        const auto texel = [&](int tx, int ty) {
            tx = std::clamp(tx, 0, cols - 1);
            ty = std::clamp(ty, 0, rows - 1);
            return (static_cast<std::size_t>(ty) * parchment->width +
                    static_cast<std::size_t>(tx)) *
                   4;
        };
        for (int ry = 0; ry < qh; ++ry) {
            // Direct3D: the pixel centre (ry + 0.5) over the quad, texel centres at + 0.5.
            const double v = (static_cast<double>(ry) + 0.5) * rows / qh - 0.5;
            const double fy = std::floor(v);
            const double ty = v - fy;
            const int y0 = static_cast<int>(fy);
            for (int rx = 0; rx < qw; ++rx) {
                const double u = (static_cast<double>(rx) + 0.5) * cols / qw - 0.5;
                const double fx = std::floor(u);
                const double tx = u - fx;
                const int x0 = static_cast<int>(fx);
                const std::array<std::size_t, 4> at = {texel(x0, y0), texel(x0 + 1, y0),
                                                       texel(x0, y0 + 1), texel(x0 + 1, y0 + 1)};
                const std::array<double, 4> wt = {(1.0 - tx) * (1.0 - ty), tx * (1.0 - ty),
                                                  (1.0 - tx) * ty, tx * ty};
                // Premultiplied, so a transparent neighbour never darkens the edge; a pixel
                // under half covered is left out (the parchment is drawn without blending).
                double alpha = 0.0;
                std::array<double, 3> sum{};
                for (std::size_t i = 0; i < 4; ++i) {
                    const double a = wt[i] * parchment->pixels[at[i] + 3] / 255.0;
                    alpha += a;
                    for (std::size_t c = 0; c < 3; ++c)
                        sum[c] += a * parchment->pixels[at[i] + c];
                }
                if (alpha < 0.5)
                    continue;
                std::array<std::uint8_t, 4> px{};
                for (std::size_t c = 0; c < 3; ++c)
                    px[c] = static_cast<std::uint8_t>(std::lround(sum[c] / alpha));
                px[3] = 255;
                if (cfg.colour_depth == 16) {
                    // The GOG build's RGB565 frame.
                    const std::uint32_t rgb = as_shown_565((std::uint32_t{px[0]} << 16U) |
                                                           (std::uint32_t{px[1]} << 8U) | px[2]);
                    px = {static_cast<std::uint8_t>(rgb >> 16U),
                          static_cast<std::uint8_t>((rgb >> 8U) & 0xffU),
                          static_cast<std::uint8_t>(rgb & 0xffU), 255};
                }
                put(rx + cfg.parchment_x0, ry + cfg.parchment_y0, px.data());
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
