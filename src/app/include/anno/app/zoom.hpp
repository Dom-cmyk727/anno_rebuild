// The maths of the viewer's continuous zoom (docs/render/iso.md "Continuous
// zoom"), without SDL, a clock or a GPU, so tests can run it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>

#include <anno/render/iso.hpp>
#include <anno/render/zoom_levels.hpp>

namespace anno::app {

/// How far past the first and last zoom group's own tile width the continuous
/// zoom may go: half a group (the geometric middle between two groups is at most
/// this far from either), so the range ends half a group beyond group 0
/// (upscaled) and group 2 (reduced). Reaching further out needs mips of the
/// composed grid (T3 stage 3).
inline constexpr double kZoomEndFactor = 1.4142135623730951;
/// The zoom group is switched only when the logarithm of the ratio of the zoom
/// to another group's tile width is smaller than the shown group's by this
/// factor: hysteresis, so a zoom resting at the middle between two groups does
/// not flip between them (each flip changes the chunks drawn).
inline constexpr double kGroupSwitchBias = 1.05;
/// Below this the easing treats the remaining distance (a logarithm) as zero:
/// 0.01 % of a tile width, far below a pixel.
inline constexpr double kZoomSnap = 1e-4;

/// The wanted tile width after a wheel notch (or key) of `factor`, kept inside
/// the range: from the last group's tile width / kZoomEndFactor to the first
/// group's * kZoomEndFactor.
[[nodiscard]] inline double
zoom_target_after(double target, double factor,
                  const render::ZoomLevels& levels = render::ZoomLevels::original(),
                  double min_tile = 0.0) {
    // min_tile: the smallest tile width wanted (the scene fitting the window, or
    // --min-zoom); never below what the last level can show.
    const double lowest = std::max(levels.tile_w(levels.size() - 1) / kZoomEndFactor, min_tile);
    return std::clamp(target * factor, lowest, levels.tile_w(0) * kZoomEndFactor);
}

/// The state of a continuous zoom: the tile width in screen pixels shown (`now`)
/// and wanted (`target`), the anchor (a screen offset from the window centre,
/// the point kept fixed), and the camera (the scene point at the window centre,
/// in pixels of the zoom group shown, `group`).
struct ZoomState {
    double now = 0, target = 0;
    double anchor_x = 0, anchor_y = 0;
    double cam_x = 0, cam_y = 0;
    std::size_t group = 0;
};

/// Screen pixels per scene pixel of the group shown.
[[nodiscard]] inline double
zoom_scale(const ZoomState& s, const render::ZoomLevels& levels = render::ZoomLevels::original()) {
    return s.now / levels.tile_w(s.group);
}

/// Advances the zoom by `dt_ms`: eases `now` towards `target` (in the logarithm:
/// the remaining distance shrinks by e^-3, 95 % covered, in `ease_ms`; 0 = at
/// once), keeps the scene point under the anchor fixed, and switches to the zoom
/// group whose tile width is nearest in ratio (with hysteresis) when
/// `ready(group)`; the camera is converted by the tile ratio as Viewer::zoom
/// does. The group is chosen also when the zoom is at rest, so a group that
/// became ready later is taken then.
[[nodiscard]] inline ZoomState
zoom_advance(ZoomState s, double dt_ms, unsigned ease_ms,
             const std::function<bool(std::size_t)>& ready,
             const render::ZoomLevels& levels = render::ZoomLevels::original()) {
    if (s.now != s.target) {
        const double remaining = std::log(s.target / s.now);
        double next = s.target;
        if (ease_ms > 0 && std::abs(remaining) > kZoomSnap) {
            const double covered = 1.0 - std::exp(-3.0 * dt_ms / static_cast<double>(ease_ms));
            next = s.now * std::exp(remaining * covered);
        }
        // The point under the anchor stays where it is on the screen: p = cam + d / s
        // is fixed, so the camera moves by d (1 / s_old - 1 / s_new).
        const double tile = levels.tile_w(s.group);
        const double s_old = s.now / tile, s_new = next / tile;
        s.cam_x += s.anchor_x * (1.0 / s_old - 1.0 / s_new);
        s.cam_y += s.anchor_y * (1.0 / s_old - 1.0 / s_new);
        s.now = next;
    }
    auto distance = [&](std::size_t g) { return std::abs(std::log(s.now / levels.tile_w(g))); };
    std::size_t best = s.group;
    for (std::size_t g = 0; g < levels.size(); ++g) {
        if (g != s.group && distance(g) * kGroupSwitchBias < distance(best) && ready(g)) {
            best = g;
        }
    }
    if (best != s.group) {
        s.cam_x = s.cam_x * levels.tile_w(best) / levels.tile_w(s.group);
        s.cam_y = s.cam_y * levels.tile_h(best) / levels.tile_h(s.group);
        s.group = best;
    }
    return s;
}

} // namespace anno::app
