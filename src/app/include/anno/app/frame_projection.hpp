// The drawn frame as the opening's controls see it (W4-OPENING-1): drawable pixels to the ground
// tile or the sea point under them, and the ships' screen boxes. It is built from the PointerFrame
// of the last frame drawn (its screen map, zoom geometry and owning scene handle), so what the
// pointer is over is what is on screen, at any zoom, density and window size.
#pragma once

#include <anno/app/dev_tool.hpp>
#include <anno/app/opening_ui.hpp>
#include <anno/app/ship_layer.hpp>

namespace anno::app {

class FrameProjection final : public ScreenProjection {
  public:
    /// `frame` and `ships` outlive the projection.
    FrameProjection(const PointerFrame& frame, const ShipLayer& ships)
        : frame_(frame), ships_(ships) {}
    [[nodiscard]] std::optional<PickedPoint> pick(double px, double py) const override;
    [[nodiscard]] std::optional<ScreenBox> ship_box(const ShipView& ship) const override {
        return ships_.box(ship, frame_);
    }

  private:
    const PointerFrame& frame_;
    const ShipLayer& ships_;
};

/// The pixel (drawable) at which a world point on the sea surface is drawn in `frame`: the forward
/// rule of the pick at sea level (the inverse of FrameProjection::pick's sea point). Tests and the
/// scripted session use it to aim at a point of the sea.
[[nodiscard]] std::optional<std::pair<double, double>>
sea_pixel(const PointerFrame& frame, double tile_x, double tile_y);

/// The pixel at which the centre of ground tile (x, y) is drawn in `frame` (the island under it
/// placed as the scene placed it, at the ground height there), or nullopt when no island of the
/// scene holds the tile or the pixel is off the drawable.
[[nodiscard]] std::optional<std::pair<double, double>>
ground_pixel(const PointerFrame& frame, sim::TileXY tile);

} // namespace anno::app
