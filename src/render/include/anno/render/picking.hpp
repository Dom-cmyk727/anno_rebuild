// Picking: the inverse of the island projection (docs/render/iso.md, "Tiles" and
// "Elevation"). The forward rule is iso::tile_top for a corner and the height lift
// of iso::kHeightLift; the inverse here is the same rule solved for the corner
// (u, v) under a point, with the ground height at that point taken into account.
// Header-only: it is arithmetic, and the window and the tests both use it.
#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

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
    const double x = (u - v) * g.tile_w / 2.0;
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
        return std::pair{(yflat / (g.tile_h / 2.0) + sx / (g.tile_w / 2.0)) / 2.0,
                         (yflat / (g.tile_h / 2.0) - sx / (g.tile_w / 2.0)) / 2.0};
    };
    const auto [u0, v0] = at(0.0);
    const auto [u1, v1] = at(max_height);
    return PickRay{u0, v0, u1, v1};
}

/// The world tile under scene pixel (sx, sy), or nullopt. `height(u, v)` is the ground height in
/// HEIGHT units at the world point (u, v), nullopt over open sea (height 0).
///
/// The screen point is a ray through the heights: the world point drawn there at height h is
/// at u + v = (sy + lift h) / (tile_h / 2), u - v = sx / (tile_w / 2), so a higher h is further
/// forward in the painter's order (x + y). The visible ground is the ray's frontmost meeting with
/// it (round ee, B2: an earlier fixed-point iteration from height 0 settled on the ground behind a
/// raised tile): the ray is marched from kMaxGroundHeight down in steps of one HEIGHT unit (a
/// step moves the point 2.059 * tile_w / 84 px, about a tenth of a tile, and the ground is linear
/// within a tile triangle), and the first step that reaches the ground is refined by bisection.
/// Height 0 always meets it (no ground is below 0), so the march ends.
/// The tile is (floor(u), floor(v)): tile (x, y) spans the corners [x, x+1] x [y, y+1].
/// nullopt: a non-finite input or geometry, and a corner beyond +-2^30 tiles (checked before the
/// conversion, which is then exact).
[[nodiscard]] inline std::optional<std::pair<std::int32_t, std::int32_t>>
pick_tile(double sx, double sy, const PickGeometry& g,
          const std::function<std::optional<float>(float, float)>& height,
          double max_height = kMaxGroundHeight) {
    constexpr double kLimit = 1073741824.0; // 2^30 tiles: far beyond any map, exact in int32
    constexpr int kRefine = 24;
    if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(g.tile_w) ||
        !std::isfinite(g.tile_h) || !std::isfinite(g.lift) || !(g.tile_w > 0.0) ||
        !(g.tile_h > 0.0) || !std::isfinite(max_height) || max_height < 0.0) {
        return std::nullopt;
    }
    // The world point on the ray at height h.
    auto at = [&](double h) {
        const double yflat = sy + g.lift * h;
        return std::pair{(yflat / (g.tile_h / 2.0) + sx / (g.tile_w / 2.0)) / 2.0,
                         (yflat / (g.tile_h / 2.0) - sx / (g.tile_w / 2.0)) / 2.0};
    };
    // Whether the ray at height h is at or below the ground there.
    auto meets = [&](double h) {
        const auto [u, v] = at(h);
        if (!std::isfinite(u) || !std::isfinite(v) || std::fabs(u) >= kLimit ||
            std::fabs(v) >= kLimit) {
            return false;
        }
        return height(static_cast<float>(u), static_cast<float>(v)).value_or(0.0F) >= h;
    };
    double hit = 0.0;
    if (meets(max_height)) {
        hit = max_height;
    } else {
        for (double h = std::floor(max_height); h >= 0.0; h -= 1.0) {
            if (h == max_height) {
                continue;
            }
            if (meets(h)) {
                // The ray is above the ground at `above` and at or below it at h: refine.
                double lo = h, above = std::min(h + 1.0, max_height);
                for (int i = 0; i < kRefine; ++i) {
                    const double mid = (lo + above) / 2.0;
                    (meets(mid) ? lo : above) = mid;
                }
                hit = lo;
                break;
            }
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
