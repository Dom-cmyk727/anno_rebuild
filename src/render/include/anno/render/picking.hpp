// Picking: the inverse of the island projection (docs/render/iso.md, "Tiles" and
// "Elevation"). The forward rule is iso::tile_top for a corner and the height lift
// of iso::kHeightLift; the inverse here is the same rule solved for the corner
// (u, v) under a point, with the ground height at that point taken into account.
// Header-only: it is arithmetic, and the window and the tests both use it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <anno/render/iso.hpp>

namespace anno::render {

/// The tile geometry of one zoom level, in scene pixels: the diamond's width and
/// height, and the pixels one HEIGHT unit lifts a corner (the lift scales with the
/// tile width, as Lift::scaled does).
struct PickGeometry {
    double tile_w = 84.0;
    double tile_h = 42.0;
    double lift = kHeightLift.num / static_cast<double>(kHeightLift.den);
};

/// The geometry of a zoom level whose diamond is `tile_w` x `tile_h` scene pixels (a built
/// group or a derived level, ZoomLevels::tile_w / tile_h): the lift scales with the tile width,
/// as Lift::scaled does.
[[nodiscard]] inline PickGeometry pick_geometry_of(double tile_w, double tile_h) {
    PickGeometry g;
    g.tile_w = tile_w;
    g.tile_h = tile_h;
    g.lift = kHeightLift.num / static_cast<double>(kHeightLift.den) * g.tile_w /
             static_cast<double>(kTile[0].w);
    return g;
}

/// The geometry of a built zoom group (`group` 0..2) at a continuous scale of that
/// group (1.0 = the group as built). Scale multiplies the tile size and the lift
/// alike, so a continuous zoom picks with the same rule.
[[nodiscard]] inline PickGeometry pick_geometry(std::size_t group, double scale = 1.0) {
    const TileSize size = kTile[group];
    return pick_geometry_of(static_cast<double>(size.w) * scale,
                            static_cast<double>(size.h) * scale);
}

/// How a frame's drawable pixels map to the scene pixels it shows (round ee, B1): the scene
/// point under the centre of the drawable pixel holding (px, py) is (x0 + (floor(px) + 0.5) /
/// scale, ...): a pointer with a fraction (a high-density display reports one) is in the pixel
/// its integer part names. A stepped zoom draws scene pixel view.x + px at px: x0 = view.x,
/// scale 1. A continuous zoom resamples (resample.hpp): x0 = view.x + Resample::offset_x, scale =
/// Resample::scale (the scale resampled_view settled on, not the one asked for).
struct ScreenMap {
    double x0 = 0.0, y0 = 0.0;
    double scale = 1.0;
};

/// The scene point under drawable pixel (px, py), or nullopt for a non-finite input or scale.
[[nodiscard]] inline std::optional<std::pair<double, double>> scene_point(const ScreenMap& m,
                                                                          double px, double py) {
    if (!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(m.x0) || !std::isfinite(m.y0) ||
        !std::isfinite(m.scale) || !(m.scale > 0.0)) {
        return std::nullopt;
    }
    return std::pair{m.x0 + (std::floor(px) + 0.5) / m.scale,
                     m.y0 + (std::floor(py) + 0.5) / m.scale};
}

/// Scene pixel of corner (u, v) at height `h` (HEIGHT units): the forward rule,
/// continuous in (u, v). At integer corners and h = 0 it equals iso::tile_top
/// (the test checks that).
[[nodiscard]] inline std::pair<double, double> corner_point(double u, double v, double h,
                                                            const PickGeometry& g) {
    const double x = (v - u) * g.tile_w / 2.0;
    const double y = (u + v) * g.tile_h / 2.0 - g.lift * h;
    return {x, y};
}

/// The highest ground in HEIGHT units: the files store a corner height as one byte.
inline constexpr double kMaxGroundHeight = 255.0;

/// The world points pick_tile's ray passes through for scene pixel (sx, sy): (u0, v0) at height
/// 0 and (u1, v1) at `max_height` (the ray is a straight segment between them). A caller can keep
/// its height lookups to what this segment crosses (round ee, D6: a lookup over every island per
/// step grew with the scene). nullopt for the inputs pick_tile refuses.
struct PickRay {
    double u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};
[[nodiscard]] inline std::optional<PickRay> pick_ray(double sx, double sy, const PickGeometry& g,
                                                     double max_height = kMaxGroundHeight) {
    if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(g.tile_w) ||
        !std::isfinite(g.tile_h) || !std::isfinite(g.lift) || !(g.tile_w > 0.0) ||
        !(g.tile_h > 0.0) || !std::isfinite(max_height) || max_height < 0.0) {
        return std::nullopt;
    }
    auto at = [&](double h) {
        const double yflat = sy + g.lift * h;
        return std::pair{(yflat / (g.tile_h / 2.0) - sx / (g.tile_w / 2.0)) / 2.0,
                         (yflat / (g.tile_h / 2.0) + sx / (g.tile_w / 2.0)) / 2.0};
    };
    const auto [u0, v0] = at(0.0);
    const auto [u1, v1] = at(max_height);
    return PickRay{u0, v0, u1, v1};
}

/// The world tile under scene pixel (sx, sy), or nullopt. `height(u, v)` is the ground height in
/// HEIGHT units at the world point (u, v), nullopt over open sea (height 0).
///
/// The screen point is a ray through the heights: the world point drawn there at height h is
/// at u + v = (sy + lift h) / (tile_h / 2), v - u = sx / (tile_w / 2), so a higher h is further
/// forward in the painter's order (x + y). The visible ground is the ray's frontmost meeting with
/// it (round ee, B2: an earlier fixed-point iteration from height 0 settled on the ground behind a
/// raised tile). Along the ray u - v is constant and u, v grow by lift / tile_h per HEIGHT unit,
/// so the ray crosses the tile triangles' edges (u = k, v = k, and the B-C diagonal u + v = k) at
/// heights it can list; between two crossings it stays in one triangle, where the ground is linear
/// (terrain_height). The segments are walked from max_height down, the ground and the ray compared
/// at two points inside each (not on an edge, where an island border or a step in `height` jumps)
/// and extended linearly to the segment's ends; the first segment where the ray reaches the ground
/// holds the hit. Exact for ground linear per triangle: a march in steps of one HEIGHT unit missed
/// the thin sliver of a triangle in front of a crest (W4-OPENING-1-CLOSE, lan5: 0.06 px from a
/// tile's B-C edge). The work is two `height` calls per triangle the ray crosses. Height 0 always
/// meets the ground (no ground is below 0), so the walk ends.
/// The tile is (floor(u), floor(v)): tile (x, y) spans the corners [x, x+1] x [y, y+1].
/// nullopt: a non-finite input or geometry, and a corner beyond +-2^30 tiles (checked before the
/// conversion, which is then exact).
[[nodiscard]] inline std::optional<std::pair<std::int32_t, std::int32_t>>
pick_tile(double sx, double sy, const PickGeometry& g,
          const std::function<std::optional<float>(float, float)>& height,
          double max_height = kMaxGroundHeight) {
    constexpr double kLimit = 1073741824.0; // 2^30 tiles: far beyond any map, exact in int32
    if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(g.tile_w) ||
        !std::isfinite(g.tile_h) || !std::isfinite(g.lift) || !(g.tile_w > 0.0) ||
        !(g.tile_h > 0.0) || !std::isfinite(max_height) || max_height < 0.0) {
        return std::nullopt;
    }
    // The world point on the ray at height h.
    auto at = [&](double h) {
        const double yflat = sy + g.lift * h;
        return std::pair{(yflat / (g.tile_h / 2.0) - sx / (g.tile_w / 2.0)) / 2.0,
                         (yflat / (g.tile_h / 2.0) + sx / (g.tile_w / 2.0)) / 2.0};
    };
    // The ray's world point at height 0 and at the top, and the crossings in between.
    const auto [u0, v0] = at(0.0);
    const auto [u1, v1] = at(max_height);
    for (const double c : {u0, v0, u1, v1}) {
        if (!std::isfinite(c) || std::fabs(c) >= kLimit) {
            return std::nullopt;
        }
    }
    const double rate = g.lift / g.tile_h; // du/dh = dv/dh
    std::vector<double> cuts{max_height, 0.0};
    auto crossings = [&](double c0, double c1, double per_unit) {
        if (per_unit == 0.0) {
            return;
        }
        for (double k = std::ceil(std::min(c0, c1)); k <= std::max(c0, c1); k += 1.0) {
            const double h = (k - c0) / per_unit;
            if (h > 0.0 && h < max_height) {
                cuts.push_back(h);
            }
        }
    };
    crossings(u0, u1, rate);
    crossings(v0, v1, rate);
    crossings(u0 + v0, u1 + v1, 2.0 * rate);
    std::sort(cuts.begin(), cuts.end(), std::greater<>());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    // The ground above the ray at height h: >= 0 where the ray is at or below the ground.
    auto above = [&](double h) {
        const auto [u, v] = at(h);
        return static_cast<double>(
                   height(static_cast<float>(u), static_cast<float>(v)).value_or(0.0F)) -
               h;
    };
    double hit = 0.0;
    for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
        const double hi = cuts[i], lo = cuts[i + 1];
        const double q1 = above(lo + (hi - lo) / 4.0), q3 = above(lo + 3.0 * (hi - lo) / 4.0);
        // Linear in the segment: at hi q3 + (q3 - q1) / 2, at lo q1 - (q3 - q1) / 2. The top
        // reached means the hit is at hi; the ray is in the ground before lo means it is inside.
        if (q3 + (q3 - q1) / 2.0 >= 0.0 || q1 - (q3 - q1) / 2.0 >= 0.0) {
            hit = (lo + hi) / 2.0; // one tile for the whole segment
            break;
        }
    }
    const auto [u, v] = at(hit);
    if (!std::isfinite(u) || !std::isfinite(v) || std::fabs(u) >= kLimit ||
        std::fabs(v) >= kLimit) {
        return std::nullopt;
    }
    return std::pair{static_cast<std::int32_t>(std::floor(u)),
                     static_cast<std::int32_t>(std::floor(v))};
}

} // namespace anno::render
