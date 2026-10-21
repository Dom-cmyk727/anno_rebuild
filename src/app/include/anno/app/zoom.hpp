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
/// The level shown is switched to a coarser one only once the zoom is this much
/// below that level's tile width (5 %): hysteresis on the coarse side, so a zoom
/// resting at a level's tile width does not flip between two levels (each flip
/// changes the chunks drawn). The switch to the finer level has none: it is taken
/// as soon as the scale would exceed the allowed magnification.
inline constexpr double kLevelCoarseMargin = 0.95;
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
    /// How much of the camera's position is the zoom's doing (the anchoring moves the
    /// camera towards the point under the cursor), in pixels of the group shown: the
    /// camera minus this is where the user pans (pan_centre), which the chunk margin's
    /// velocity follows.
    double shift_x = 0, shift_y = 0;
    std::size_t group = 0;
};

/// The camera without the zoom's anchoring displacement.
[[nodiscard]] inline std::pair<double, double> pan_centre(const ZoomState& s) {
    return {s.cam_x - s.shift_x, s.cam_y - s.shift_y};
}

/// Screen pixels per scene pixel of the group shown.
[[nodiscard]] inline double
zoom_scale(const ZoomState& s, const render::ZoomLevels& levels = render::ZoomLevels::original()) {
    return s.now / levels.tile_w(s.group);
}

/// The level to compose on at tile width `now`, from the `current` one (docs/render/iso.md
/// "Continuous zoom"): the coarsest level whose tile width is at least `now`, so the
/// composed grid is reduced (scale <= 1, the Box filter) and never magnified. The
/// scale may reach `max_upscale` (>= 1, ZoomOptions::max_upscale; 1 = never
/// magnified, 1.45 = the nearest-in-ratio look of the first stage 3 build) before
/// the finer level is taken, and the coarser level is taken only once the scale on
/// it is kLevelCoarseMargin of that. A finer level that is not `ready` is bridged by
/// the next coarser one that is (magnified, with the Area filter), the current
/// level at the latest; of the coarser levels that fit, the coarsest ready one is
/// taken, and with none ready the current level stays (zoom_advance then holds the
/// zoom, see zoom_hold_floor). Above the first level the first is the only choice.
[[nodiscard]] inline std::size_t
zoom_level_for(double now, std::size_t current, const std::function<bool(std::size_t)>& ready,
               const render::ZoomLevels& levels = render::ZoomLevels::original(),
               double max_upscale = 1.0) {
    const std::size_t n = levels.size();
    // Level g fits when the scale on it is at most max_upscale.
    auto fits = [&](std::size_t g, double margin) {
        return now <= max_upscale * margin * levels.tile_w(g);
    };
    std::size_t wanted = current;
    if (!fits(current, 1.0)) {
        // Too magnified: the coarsest level that fits, else the finest.
        wanted = 0;
        for (std::size_t g = current; g-- > 0;) {
            if (fits(g, 1.0)) {
                wanted = g;
                break;
            }
        }
    } else {
        for (std::size_t g = n; g-- > current + 1;) {
            if (fits(g, kLevelCoarseMargin)) {
                wanted = g;
                break;
            }
        }
    }
    // A coarser level that is not ready yet is waited for: the current one is
    // reduced and complete. A finer level that is not ready yet is bridged by the
    // next coarser one that is, magnified.
    if (wanted > current) {
        for (std::size_t g = wanted; g > current; --g) {
            if (ready(g)) {
                return g;
            }
        }
        return current;
    }
    for (std::size_t g = wanted; g <= current; ++g) {
        if (g == current || ready(g)) {
            return g;
        }
    }
    return current;
}

/// The smallest tile width the zoom may reach on level `group` while the next
/// coarser level is not `ready`: the point where the coarser one would be taken
/// (kLevelCoarseMargin of its tile width, times `max_upscale`). Below it the
/// composed grid would grow without bound (the scale on the current level falls with
/// the zoom, the grid is the window over the scale) until the level is ready, so
/// the zoom is held there: the grid is at most 2.2 x the window per axis (the
/// widest tile ratio, 2.1, over the 5 % margin) in the meantime. 0 = no hold (the
/// last level, or the next one is ready).
[[nodiscard]] inline double zoom_hold_floor(std::size_t group,
                                            const std::function<bool(std::size_t)>& ready,
                                            const render::ZoomLevels& levels, double max_upscale) {
    if (group + 1 >= levels.size() || ready(group + 1)) {
        return 0.0;
    }
    return max_upscale * kLevelCoarseMargin * levels.tile_w(group + 1);
}

/// Advances the zoom by `dt_ms`: eases `now` towards `target` (in the logarithm:
/// the remaining distance shrinks by e^-3, 95 % covered, in `ease_ms`; 0 = at
/// once), keeps the scene point under the anchor fixed, and switches the level
/// as zoom_level_for decides; the camera is converted by the tile ratio as
/// Viewer::zoom does. The level is chosen also when the zoom is at rest, so a
/// level that became ready later is taken then.
[[nodiscard]] inline ZoomState zoom_advance(
    ZoomState s, double dt_ms, unsigned ease_ms, const std::function<bool(std::size_t)>& ready,
    const render::ZoomLevels& levels = render::ZoomLevels::original(), double max_upscale = 1.0) {
    if (s.now != s.target) {
        const double remaining = std::log(s.target / s.now);
        double next = s.target;
        if (ease_ms > 0 && std::abs(remaining) > kZoomSnap) {
            const double covered = 1.0 - std::exp(-3.0 * dt_ms / static_cast<double>(ease_ms));
            next = s.now * std::exp(remaining * covered);
        }
        // Held while the next coarser level is not ready (zoom_hold_floor): only a
        // zoom-out is held, a zoom already below the floor is left to move up.
        next =
            std::max(next, std::min(s.now, zoom_hold_floor(s.group, ready, levels, max_upscale)));
        // The point under the anchor stays where it is on the screen: p = cam + d / s
        // is fixed, so the camera moves by d (1 / s_old - 1 / s_new).
        const double tile = levels.tile_w(s.group);
        const double s_old = s.now / tile, s_new = next / tile;
        const double dx = s.anchor_x * (1.0 / s_old - 1.0 / s_new);
        const double dy = s.anchor_y * (1.0 / s_old - 1.0 / s_new);
        s.cam_x += dx;
        s.cam_y += dy;
        s.shift_x += dx;
        s.shift_y += dy;
        s.now = next;
    }
    const std::size_t best = zoom_level_for(s.now, s.group, ready, levels, max_upscale);
    if (best != s.group) {
        const double kx = levels.tile_w(best) / levels.tile_w(s.group);
        const double ky = levels.tile_h(best) / levels.tile_h(s.group);
        s.cam_x *= kx;
        s.cam_y *= ky;
        s.shift_x *= kx;
        s.shift_y *= ky;
        s.group = best;
    }
    return s;
}

} // namespace anno::app
