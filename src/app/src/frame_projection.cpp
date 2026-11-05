#include <algorithm>
#include <cmath>

#include <anno/app/frame_projection.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/picking.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/terrain_view.hpp>

namespace anno::app {

namespace {

/// The drawable pixel of scene point (sx, sy): the inverse of render::scene_point.
std::pair<double, double> pixel_of(const render::ScreenMap& m, double sx, double sy) {
    return {(sx - m.x0) * m.scale - 0.5, (sy - m.y0) * m.scale - 0.5};
}

} // namespace

std::optional<PickedPoint> FrameProjection::pick(double px, double py) const {
    const PointerFrame& frame = frame_;
    if (!(px >= 0.0) || !(py >= 0.0) || !(px < frame.drawable_w) || !(py < frame.drawable_h) ||
        !frame.scene) {
        return std::nullopt;
    }
    const auto point = render::scene_point(frame.map, px, py);
    if (!point) {
        return std::nullopt;
    }
    const render::Scene& scene = *frame.scene;
    PickedPoint out;
    // The sea surface under the pixel: the ray at sea level.
    const auto sea = render::pick_ray(point->first, point->second, frame.geometry,
                                      static_cast<double>(render::kSeaLevel));
    if (!sea) {
        return std::nullopt;
    }
    out.x_q8 = static_cast<std::int32_t>(std::floor(sea->u1 * 256.0));
    out.y_q8 = static_cast<std::int32_t>(std::floor(sea->v1 * 256.0));
    out.tile = sim::TileXY{static_cast<std::int32_t>(std::floor(sea->u1)),
                           static_cast<std::int32_t>(std::floor(sea->v1))};
    // Ground: each island is drawn at its integer placement, so the pick solves that island's own
    // rule (placement_rounding) and accepts a tile that lies in it and stands above the waterline.
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        const auto& island = scene.islands[k];
        const auto rounding = placement_rounding(scene, k, frame.group, frame.geometry.tile_w);
        const double sx = point->first - rounding.first, sy = point->second - rounding.second;
        const auto ray = render::pick_ray(sx, sy, frame.geometry);
        if (!ray) {
            continue;
        }
        const double u_lo = std::min(ray->u0, ray->u1) - 1.0,
                     u_hi = std::max(ray->u0, ray->u1) + 1.0;
        const double v_lo = std::min(ray->v0, ray->v1) - 1.0,
                     v_hi = std::max(ray->v0, ray->v1) + 1.0;
        if (island.x + static_cast<double>(island.width) < u_lo || island.x > u_hi ||
            island.y + static_cast<double>(island.height) < v_lo || island.y > v_hi) {
            continue;
        }
        const auto tile = render::pick_tile(sx, sy, frame.geometry, [&scene, k](float u, float v) {
            return scene.island_ground_height(k, u, v);
        });
        if (!tile) {
            continue;
        }
        // The sim's land classifier (docs/render/iso.md, iso.hpp kLandAverage): the average of the
        // tile's four corner heights above 19. Not the centre against the waterline: a shoal tile
        // the sim would plan to is not land here either.
        float sum = 0.0F;
        bool all = true;
        for (const float dx : {0.0F, 1.0F}) {
            for (const float dy : {0.0F, 1.0F}) {
                const auto h = scene.island_ground_height(k, static_cast<float>(tile->first) + dx,
                                                          static_cast<float>(tile->second) + dy);
                all = all && h.has_value();
                sum += h.value_or(0.0F);
            }
        }
        if (all && sum / 4.0F > static_cast<float>(render::kLandAverage)) {
            out.tile = sim::TileXY{tile->first, tile->second};
            out.ground = true;
            return out;
        }
    }
    return out;
}

std::optional<std::pair<double, double>> sea_pixel(const PointerFrame& frame, double tile_x,
                                                   double tile_y) {
    const auto [sx, sy] = render::corner_point(
        tile_x, tile_y, static_cast<double>(render::kSeaLevel), frame.geometry);
    const auto px = pixel_of(frame.map, sx, sy);
    if (!(px.first >= 0.0) || !(px.second >= 0.0) || !(px.first < frame.drawable_w) ||
        !(px.second < frame.drawable_h)) {
        return std::nullopt;
    }
    return px;
}

std::optional<std::pair<double, double>> ground_pixel(const PointerFrame& frame, sim::TileXY tile) {
    if (!frame.scene) {
        return std::nullopt;
    }
    const render::Scene& scene = *frame.scene;
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        const auto& island = scene.islands[k];
        if (tile.x < island.x || tile.y < island.y ||
            tile.x >= island.x + static_cast<int>(island.width) ||
            tile.y >= island.y + static_cast<int>(island.height)) {
            continue;
        }
        const auto rounding = placement_rounding(scene, k, frame.group, frame.geometry.tile_w);
        const double h = scene
                             .island_ground_height(k, static_cast<float>(tile.x) + 0.5F,
                                                   static_cast<float>(tile.y) + 0.5F)
                             .value_or(0.0F);
        const auto [sx, sy] = render::corner_point(tile.x + 0.5, tile.y + 0.5, h, frame.geometry);
        const auto px = pixel_of(frame.map, sx + rounding.first, sy + rounding.second);
        if (!(px.first >= 0.0) || !(px.second >= 0.0) || !(px.first < frame.drawable_w) ||
            !(px.second < frame.drawable_h)) {
            return std::nullopt;
        }
        return px;
    }
    return std::nullopt;
}

std::optional<std::uint8_t> scene_vertex_height(const render::Scene& scene, std::int64_t x,
                                                std::int64_t y) {
    // A vertex inside an island reads its byte; one on an island's far column or row reads the
    // sim's boundary height (review 2026-11-03: the preview had no height there, so a warehouse
    // whose water edge is the island's far edge was left undecided where the sim accepts it). Two
    // islands that touch differ from the sim at the shared vertices: the sim reads the island under
    // the origin, this the island that holds the vertex inside it.
    bool boundary = false;
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        const auto& island = scene.islands[k];
        const std::int64_t lx = x - static_cast<std::int64_t>(island.x);
        const std::int64_t ly = y - static_cast<std::int64_t>(island.y);
        const auto w = static_cast<std::int64_t>(island.width);
        const auto h = static_cast<std::int64_t>(island.height);
        if (lx < 0 || ly < 0 || lx > w || ly > h) {
            continue;
        }
        if (lx == w || ly == h) {
            boundary = true;
            continue;
        }
        if (const auto v =
                scene.island_ground_height(k, static_cast<float>(x), static_cast<float>(y))) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(*v), 0L, 255L));
        }
    }
    if (boundary) {
        return sim::kBoundaryVertexHeight;
    }
    return std::nullopt;
}

} // namespace anno::app
