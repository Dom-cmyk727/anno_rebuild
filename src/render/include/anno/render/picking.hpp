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

/// The geometry of a built zoom group (`group` 0..2) at a continuous scale of that
/// group (1.0 = the group as built). Scale multiplies the tile size and the lift
/// alike, so a continuous zoom picks with the same rule.
[[nodiscard]] inline PickGeometry pick_geometry(std::size_t group, double scale = 1.0) {
    const TileSize size = kTile[group];
    PickGeometry g;
    g.tile_w = static_cast<double>(size.w) * scale;
    g.tile_h = static_cast<double>(size.h) * scale;
    g.lift = kHeightLift.num / static_cast<double>(kHeightLift.den) * g.tile_w /
             static_cast<double>(kTile[0].w);
    return g;
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

/// The world tile under scene pixel (sx, sy), or nullopt. `height(u, v)` is the
/// ground height in HEIGHT units at the world point (u, v), nullopt over open sea
/// (height 0). The ground height moves the point on screen, so the point is found
/// by a fixed-point iteration on the height: at most `kIterations` steps, stopping
/// when the height no longer changes by more than `kTolerance`.
/// The tile is (floor(u), floor(v)): tile (x, y) spans the corners [x, x+1] x [y, y+1].
[[nodiscard]] inline std::optional<std::pair<std::int32_t, std::int32_t>>
pick_tile(double sx, double sy, const PickGeometry& g,
          const std::function<std::optional<float>(float, float)>& height) {
    constexpr int kIterations = 16;
    constexpr double kTolerance = 1e-4;
    double h = 0.0;
    double u = 0.0, v = 0.0;
    for (int i = 0; i < kIterations; ++i) {
        // Undo the lift of the height assumed at the current point: the screen y of a
        // flat point (u, v) at height 0.
        const double yflat = sy + g.lift * h;
        u = (yflat / (g.tile_h / 2.0) + sx / (g.tile_w / 2.0)) / 2.0;
        v = (yflat / (g.tile_h / 2.0) - sx / (g.tile_w / 2.0)) / 2.0;
        const double next = height(static_cast<float>(u), static_cast<float>(v)).value_or(0.0F);
        if (std::fabs(next - h) <= kTolerance) {
            h = next;
            break;
        }
        h = next;
    }
    if (!std::isfinite(u) || !std::isfinite(v)) {
        return std::nullopt;
    }
    return std::pair{static_cast<std::int32_t>(std::floor(u)),
                     static_cast<std::int32_t>(std::floor(v))};
}

} // namespace anno::render
