// The placement ghost (see the header).
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <set>
#include <tuple>

#include <anno/app/placement_ghost.hpp>
#include <anno/sim/building.hpp>

namespace anno::app {

std::string_view ghost_state_name(GhostState state) {
    switch (state) {
    case GhostState::Hidden:
        return "hidden";
    case GhostState::Refused:
        return "refused";
    case GhostState::Unaffordable:
        return "unaffordable";
    case GhostState::Valid:
        return "valid";
    }
    return "hidden";
}

GhostState ghost_state(const GhostInputs& in, GhostState previous) {
    if (!in.armed || in.footprint_empty || in.pointer_on_chrome) {
        return GhostState::Hidden;
    }
    if (!in.verdict) {
        return previous;
    }
    if (*in.verdict == sim::Reject::None) {
        return GhostState::Valid;
    }
    if (*in.verdict == sim::Reject::CannotAfford) {
        return GhostState::Unaffordable;
    }
    return GhostState::Refused;
}

bool GhostBlink::advance(std::uint64_t dt_ms) {
    // The original adds a u16; a 64-bit sum cannot wrap for any frame time a clock gives.
    acc_ += dt_ms;
    if (acc_ <= kGhostBlinkThresholdMs) {
        return false;
    }
    acc_ -= kGhostBlinkThresholdMs;
    phase_ = !phase_;
    ++flips_;
    return true;
}

std::vector<std::uint32_t> radius_half_widths(std::uint32_t radius) {
    // AnnoFrame 0x100443d1-0x10044439, register for register: eax = the row whose width is
    // still open from the top (starts at r), ecx = the row the loop writes, esi = r*r minus the
    // odd numbers 1, 3, 5, ... so far, edx = (r-1)*r minus the even steps taken, s = 2r. The
    // comparison `ja` is unsigned on 32 bits.
    const std::int64_t r = radius;
    std::vector<std::uint32_t> w(static_cast<std::size_t>(r) + 2, 0);
    std::int64_t eax = r, ecx = 0, edi = 0;
    std::int64_t esi = r * r, edx = (r - 1) * r, s = 2 * r;
    for (;;) {
        esi += -1 - edi;
        w[static_cast<std::size_t>(ecx)] = static_cast<std::uint32_t>(eax);
        if (!(static_cast<std::uint32_t>(esi) > static_cast<std::uint32_t>(edx))) {
            s -= 2;
            w[static_cast<std::size_t>(eax)] = static_cast<std::uint32_t>(ecx);
            --eax;
            edx -= s;
        }
        ++ecx;
        edi += 2;
        if (!(ecx <= eax)) {
            break;
        }
    }
    if (r <= 3) {
        for (std::int64_t i = r - (r + 1) / 4; i <= r; ++i) {
            ++w[static_cast<std::size_t>(i)];
        }
    }
    w.resize(static_cast<std::size_t>(r) + 1);
    return w;
}

std::vector<CornerEdge> footprint_outline(const GhostBox& b) {
    std::vector<CornerEdge> out;
    if (b.w == 0 || b.h == 0) {
        return out;
    }
    const std::int64_t x1 = b.x0 + b.w, y1 = b.y0 + b.h;
    for (std::int64_t x = b.x0; x < x1; ++x) {
        out.push_back({x, b.y0, x + 1, b.y0});
        out.push_back({x, y1, x + 1, y1});
    }
    for (std::int64_t y = b.y0; y < y1; ++y) {
        out.push_back({b.x0, y, b.x0, y + 1});
        out.push_back({x1, y, x1, y + 1});
    }
    return out;
}

std::vector<CornerEdge> radius_outline(const GhostBox& b, std::uint32_t radius) {
    std::vector<CornerEdge> out;
    if (b.w == 0 || b.h == 0) {
        return out;
    }
    const std::vector<std::uint32_t> hw = radius_half_widths(radius);
    const std::int64_t cx = b.x0 + (std::int64_t{b.w} - 1) / 2;
    const std::int64_t cy = b.y0 + (std::int64_t{b.h} - 1) / 2;
    const std::int64_t r = radius;
    auto inside = [&](std::int64_t x, std::int64_t y) {
        const std::int64_t dy = y - cy;
        if (dy < -r || dy > r) {
            return false;
        }
        const std::int64_t half = hw[static_cast<std::size_t>(std::llabs(dy))];
        return x >= cx - half && x <= cx + half;
    };
    std::set<std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t>> edges;
    for (std::int64_t dy = -r; dy <= r; ++dy) {
        const std::int64_t y = cy + dy;
        const std::int64_t half = hw[static_cast<std::size_t>(std::llabs(dy))];
        for (std::int64_t x = cx - half; x <= cx + half; ++x) {
            if (!inside(x, y - 1)) {
                edges.insert({x, y, x + 1, y});
            }
            if (!inside(x, y + 1)) {
                edges.insert({x, y + 1, x + 1, y + 1});
            }
            if (!inside(x - 1, y)) {
                edges.insert({x, y, x, y + 1});
            }
            if (!inside(x + 1, y)) {
                edges.insert({x + 1, y, x + 1, y + 1});
            }
        }
    }
    for (const auto& [x0, y0, x1, y1] : edges) {
        if (x0 >= 0 && y0 >= 0) {
            out.push_back({x0, y0, x1, y1});
        }
    }
    return out;
}

std::pair<double, double> project_corner(const OutlineProjection& p, double u, double v) {
    const double h = (p.height ? p.height(u, v) : 0.0) + kGhostOutlineLift;
    const auto [sx, sy] = render::corner_point(u, v, h, p.geometry);
    return {(sx + p.rounding.first - p.map.x0) * p.map.scale,
            (sy + p.rounding.second - p.map.y0) * p.map.scale};
}

namespace {

/// Bresenham from the pixel holding a to the pixel holding b; `plot` gets every pixel once.
template <class Plot>
void line(std::pair<double, double> a, std::pair<double, double> b, Plot&& plot) {
    if (!std::isfinite(a.first) || !std::isfinite(a.second) || !std::isfinite(b.first) ||
        !std::isfinite(b.second)) {
        return;
    }
    // Far outside any drawable: a corner projected that far is not drawn (and the conversion
    // below stays in range).
    constexpr double kFar = 1e7;
    if (std::abs(a.first) > kFar || std::abs(a.second) > kFar || std::abs(b.first) > kFar ||
        std::abs(b.second) > kFar) {
        return;
    }
    auto x0 = static_cast<std::int64_t>(std::floor(a.first));
    auto y0 = static_cast<std::int64_t>(std::floor(a.second));
    const auto x1 = static_cast<std::int64_t>(std::floor(b.first));
    const auto y1 = static_cast<std::int64_t>(std::floor(b.second));
    const std::int64_t dx = std::llabs(x1 - x0), dy = -std::llabs(y1 - y0);
    const std::int64_t sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    std::int64_t err = dx + dy;
    for (;;) {
        plot(x0, y0);
        if (x0 == x1 && y0 == y1) {
            return;
        }
        const std::int64_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace

std::uint64_t draw_outline_xrgb(std::span<const CornerEdge> edges, const OutlineProjection& p,
                                std::uint32_t* xrgb, int w, int h, std::uint32_t rgb) {
    std::uint64_t written = 0;
    if (xrgb == nullptr || w <= 0 || h <= 0) {
        return written;
    }
    for (const CornerEdge& e : edges) {
        line(project_corner(p, static_cast<double>(e.x0), static_cast<double>(e.y0)),
             project_corner(p, static_cast<double>(e.x1), static_cast<double>(e.y1)),
             [&](std::int64_t x, std::int64_t y) {
                 if (x >= 0 && y >= 0 && x < w && y < h) {
                     xrgb[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                          static_cast<std::size_t>(x)] = rgb & 0xFFFFFFU;
                     ++written;
                 }
             });
    }
    return written;
}

std::uint64_t draw_outline_rgba_under(std::span<const CornerEdge> edges, const OutlineProjection& p,
                                      assets::Rgba& canvas, std::uint32_t rgb) {
    std::uint64_t written = 0;
    const std::int64_t w = canvas.width, h = canvas.height;
    if (canvas.pixels.size() != static_cast<std::size_t>(w * h * 4)) {
        return written;
    }
    for (const CornerEdge& e : edges) {
        line(project_corner(p, static_cast<double>(e.x0), static_cast<double>(e.y0)),
             project_corner(p, static_cast<double>(e.x1), static_cast<double>(e.y1)),
             [&](std::int64_t x, std::int64_t y) {
                 if (x < 0 || y < 0 || x >= w || y >= h) {
                     return;
                 }
                 std::uint8_t* px = &canvas.pixels[static_cast<std::size_t>((y * w + x) * 4)];
                 if (px[3] != 0) {
                     return; // drawn chrome stays on top
                 }
                 px[0] = static_cast<std::uint8_t>((rgb >> 16) & 0xFFU);
                 px[1] = static_cast<std::uint8_t>((rgb >> 8) & 0xFFU);
                 px[2] = static_cast<std::uint8_t>(rgb & 0xFFU);
                 px[3] = 0xFF;
                 ++written;
             });
    }
    return written;
}

std::vector<std::size_t> collect_suppressed_object_indices(const render::Scene& scene,
                                                           std::size_t island,
                                                           std::span<const Cover> covers) {
    std::vector<std::size_t> out;
    if (island >= scene.islands.size() || covers.empty()) {
        return out;
    }
    const auto& list = scene.island_lists(island).other;
    if (list.empty()) {
        return out;
    }
    // `other` is sorted by `(o.first.x + o.second.first + o.first.y + o.second.second, o.first.x)`.
    // Bracket the isometric diagonal band covering all non-negative tile coordinates in `covers` so
    // hover over large islands never scans the full object vector.
    std::uint64_t min_diag = UINT64_MAX;
    std::uint64_t max_diag = 0;
    bool any_non_empty = false;
    for (const Cover& c : covers) {
        if (c.x1 <= c.x0 || c.y1 <= c.y0 || c.x1 <= 0 || c.y1 <= 0) {
            continue;
        }
        any_non_empty = true;
        const auto x0 = static_cast<std::uint64_t>(std::max<std::int64_t>(0, c.x0));
        const auto y0 = static_cast<std::uint64_t>(std::max<std::int64_t>(0, c.y0));
        const auto x1 = static_cast<std::uint64_t>(c.x1);
        const auto y1 = static_cast<std::uint64_t>(c.y1);
        min_diag = std::min(min_diag, x0 + y0);
        max_diag = std::max(max_diag, x1 + y1 + std::uint64_t{512});
    }
    if (!any_non_empty) {
        return out;
    }
    auto diag = [](const render::Prepared& p) -> std::uint64_t {
        return std::uint64_t{p.first.x} + p.second.first + p.first.y + p.second.second;
    };
    const auto lo = std::partition_point(
        list.begin(), list.end(), [&](const render::Prepared& p) { return diag(p) < min_diag; });
    const auto hi = std::partition_point(
        lo, list.end(), [&](const render::Prepared& p) { return diag(p) <= max_diag; });
    const auto start = static_cast<std::size_t>(lo - list.begin());
    const auto end = static_cast<std::size_t>(hi - list.begin());
    for (std::size_t i = start; i < end; ++i) {
        const auto& obj = list[i].first;
        const auto flags = anlage_head_flags(scene.anlagen, obj.anlage);
        if (!flags || (*flags & 0x20U) == 0) {
            continue;
        }
        for (const Cover& c : covers) {
            if (c.holds(obj.x, obj.y) && overwrites(c, *flags)) {
                out.push_back(i);
                break;
            }
        }
    }
    return out;
}

namespace {

bool boxes_overlap(const GhostBox& a, const GhostBox& b) {
    if (a.w == 0 || a.h == 0 || b.w == 0 || b.h == 0) {
        return false;
    }
    return a.x0 < b.x0 + b.w && b.x0 < a.x0 + a.w && a.y0 < b.y0 + b.h && b.y0 < a.y0 + a.h;
}

std::pair<std::uint32_t, std::uint32_t> footprint_dims(const sim::TypeRegistry* registry,
                                                       const formats::anlagen::AnlagenFile* anlagen,
                                                       std::uint32_t type_id,
                                                       std::uint8_t rotation) {
    if (registry != nullptr) {
        if (const sim::BuildingType* bt = registry->find(sim::TypeId{type_id}); bt != nullptr) {
            return sim::footprint(*bt, rotation);
        }
    }
    if (anlagen != nullptr) {
        if (const auto fp = anlagen->footprint(type_id)) {
            const bool odd = (rotation & 1U) != 0;
            return {odd ? fp->second : fp->first, odd ? fp->first : fp->second};
        }
    }
    return {1, 1};
}

} // namespace

std::vector<GhostBox> occupied_blocking_boxes(const BlockingQuery& q) {
    std::vector<GhostBox> out;
    if (q.target.w == 0 || q.target.h == 0) {
        return out;
    }
    std::set<std::tuple<std::int64_t, std::int64_t, std::uint32_t, std::uint32_t>> seen;
    auto push = [&](const GhostBox& b) {
        if (boxes_overlap(b, q.target) && seen.insert({b.x0, b.y0, b.w, b.h}).second) {
            out.push_back(b);
        }
    };
    for (const SceneBuilding& b : q.buildings) {
        const auto [w, h] = footprint_dims(q.registry, q.anlagen, b.type, b.rotation);
        push(GhostBox{std::int64_t{q.island_x0} + b.x, std::int64_t{q.island_y0} + b.y, w, h});
    }
    for (const SceneTileObject& t : q.tile_objects) {
        if (t.type == 0) {
            continue;
        }
        const auto [w, h] = footprint_dims(q.registry, q.anlagen, t.type, t.rotation);
        push(GhostBox{std::int64_t{q.island_x0} + t.x, std::int64_t{q.island_y0} + t.y, w, h});
    }
    if (out.empty() && q.anlagen != nullptr && !q.static_objects.empty()) {
        for (const render::Prepared& o : q.static_objects) {
            const auto flags = anlage_head_flags(*q.anlagen, o.first.anlage);
            if (!flags || (*flags & 0x20U) != 0) {
                continue;
            }
            const std::uint32_t w = std::max<std::uint32_t>(1, o.second.first);
            const std::uint32_t h = std::max<std::uint32_t>(1, o.second.second);
            push(GhostBox{std::int64_t{q.island_x0} + o.first.x,
                          std::int64_t{q.island_y0} + o.first.y, w, h});
        }
    }
    return out;
}

std::vector<CornerEdge> blocking_footprint_outline(std::span<const GhostBox> boxes) {
    std::set<std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t>> unique;
    std::vector<CornerEdge> out;
    for (const GhostBox& b : boxes) {
        for (const CornerEdge& e : footprint_outline(b)) {
            if (unique.insert({e.x0, e.y0, e.x1, e.y1}).second) {
                out.push_back(e);
            }
        }
    }
    return out;
}

} // namespace anno::app
