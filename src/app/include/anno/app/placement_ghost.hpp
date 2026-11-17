// The placement ghost as the original shows it (UI-GHOST-1; docs/BUILDINGSOBJECTS.md part B
// section 5.1). Four states, chosen in the window from the sim's own verdict on the record a click
// would send (World::check_build_record, asked through PlayRunner::request_build_probe), never from
// a partial check of the window's:
//  - hidden: nothing armed, an empty footprint, or the pointer over the drawn chrome;
//  - refused: the site is refused; a 1-pixel 0xffff00 outline of the footprint along its tile
//    corners (AnnoFrame 0x10041050), no building and no radius;
//  - unaffordable: the site is legal but the cost is not in stock (Reject::CannotAfford); as valid,
//    but the building hides while the blink phase is 1 (the chunk draw's real-tile copy, AnnoWelt
//    0x1009e2fd); the radius outline does not blink;
//  - valid: the building plus the action-radius outline (AnnoFrame 0x100443c0), 0xffff00.
// The blink is one accumulator per view on the window's clock (AnnoFrame 0x100038c8-0x100038f2):
// add the frame's milliseconds, and when the sum exceeds 699 subtract 699 and flip the phase. It is
// window state only: never sim state, never hashed.
// Everything here is pure (no SDL, no World); the window draws with it (viewer.cpp).
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/assets/texture.hpp>
#include <anno/render/picking.hpp>
#include <anno/sim/reject.hpp>

namespace anno::app {

enum class GhostState : std::uint8_t { Hidden, Refused, Unaffordable, Valid };

/// "hidden", "refused", "unaffordable", "valid" (the `tool.ghost` fact).
[[nodiscard]] std::string_view ghost_state_name(GhostState state);

/// What the state is chosen from.
struct GhostInputs {
    /// The tool builds (on, not demolishing, a type selected) and the pointer is over a tile of
    /// the city's island rectangle (DevTool::hover). ADAPTATION (OQ-760): over open sea outside
    /// that rectangle the tool has no hover, so the ghost is hidden there, where the original
    /// draws the refused footprint (AnnoFrame 0x10041050 has no island test). Follow-up: a pick
    /// over the whole map.
    bool armed = false;
    /// The selected type's footprint has no tile (AnnoFrame 0x10048491: nothing is drawn).
    bool footprint_empty = false;
    /// The pointer is over drawn chrome (the panel, the status bar). HE-observed, GOG unread
    /// (OQ-757): the History Edition hides the ghost and its radius outline there (ui_9 4:03-4:05).
    bool pointer_on_chrome = false;
    /// The sim's verdict on exactly the record a click at the hovered tile would send; nullopt
    /// while the answer for that record has not been published yet.
    std::optional<sim::Reject> verdict;
};

/// The state, in the priority hidden > refused > unaffordable > valid. A verdict of None is
/// valid, CannotAfford (step 6 of Build's check, which runs after every site test) unaffordable,
/// any other reject refused. While the verdict is pending the state shown before (`previous`) is
/// kept, so a pointer moving tile by tile does not flash the ghost off for a turn; with nothing
/// shown before (previous hidden) it stays hidden until the answer comes (one turn at most).
[[nodiscard]] GhostState ghost_state(const GhostInputs& in, GhostState previous);

/// The blink threshold: AnnoFrame 0x100038e0 `cmp ax, 0x2bb` (699 ms; HE measures 0.700 s on and
/// 0.700 s off at 80 fps, docs/BUILDINGSOBJECTS.md part B 5.1 (1)).
inline constexpr std::uint64_t kGhostBlinkThresholdMs = 699;

/// The blink phase (AnnoFrame 0x100038c8-0x100038f2, frame +0x1420/+0x1422). Starts shown with an
/// empty accumulator (the reset, AnnoWelt 0x1009ebbb, sets phase 0).
class GhostBlink {
  public:
    /// One frame of `dt_ms` window milliseconds: `acc += dt; if (acc > 699) { acc -= 699; flip }`,
    /// at most one flip per call, the surplus kept, as the original (a frame longer than 1398 ms
    /// leaves more than 699, and the next frames flip once each until it is spent). True when the
    /// phase flipped.
    bool advance(std::uint64_t dt_ms);
    /// Phase 1: the unaffordable ghost's building is hidden.
    [[nodiscard]] bool hidden() const { return phase_; }
    [[nodiscard]] std::uint64_t accumulator() const { return acc_; }
    /// Flips so far.
    [[nodiscard]] std::uint64_t flips() const { return flips_; }
    /// The milliseconds after which the next advance flips (at least 1).
    [[nodiscard]] std::uint64_t ms_to_flip() const {
        return acc_ > kGhostBlinkThresholdMs ? 1 : kGhostBlinkThresholdMs + 1 - acc_;
    }

  private:
    std::uint64_t acc_ = 0;
    bool phase_ = false;
    std::uint64_t flips_ = 0;
};

/// The radius outline's half-width per row, w[0..r] (AnnoFrame 0x100443c0-0x10044439): a
/// midpoint-circle loop over r*r and (r-1)*r with odd steps, plus 1 on the outer rows
/// r - (r+1)/4 .. r when r <= 3. Row cy +- i spans the tiles cx - w[i] .. cx + w[i].
[[nodiscard]] std::vector<std::uint32_t> radius_half_widths(std::uint32_t radius);

/// A footprint in world tiles (corner x0, y0; w x h tiles).
struct GhostBox {
    std::int64_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
};

/// One unit edge between two adjacent world tile corners.
struct CornerEdge {
    std::int64_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    friend bool operator==(const CornerEdge&, const CornerEdge&) = default;
};

/// The footprint rectangle's boundary, one edge per tile side (AnnoFrame 0x10041050: four strips
/// with one vertex per tile corner). Empty for an empty box.
[[nodiscard]] std::vector<CornerEdge> footprint_outline(const GhostBox& box);

/// The boundary of the radius disc: the tiles (cx + dx, cy + dy) with |dy| <= r and |dx| <= w[|dy|]
/// (radius_half_widths), cx = x0 + (w - 1) / 2, cy = y0 + (h - 1) / 2 (0x10044477-0x100444b1).
/// Edges with a corner below 0 are dropped (the original clips to the map, slots +0x54/+0x58; the
/// window does not know the map's far edge, so only the near one is applied). That the strips
/// outline exactly these tiles is L (OQ-756). Empty for an empty box. In ascending order.
[[nodiscard]] std::vector<CornerEdge> radius_outline(const GhostBox& box, std::uint32_t radius);

/// The outline's colour (0x10048471, 0x100484f2: the same 0xffff00 for both outlines in GOG; the
/// History Edition draws the radius green, the packet follows GOG).
inline constexpr std::uint32_t kGhostOutlineRgb = 0xFFFF00;
/// ADAPTATION (accepted by the orchestrator, 2026-11-17): the lift above the ground, 2 x 0.04
/// (0x10087de8, 0x100880fc), applied in the rebuild's HEIGHT units. The original's height unit for
/// this lift is not established; the difference is below a pixel either way.
inline constexpr double kGhostOutlineLift = 0.08;

/// Where a world corner is drawn: the frame's tile geometry and screen map, the city island's
/// placement rounding (placement_rounding) and the ground height at a world point.
struct OutlineProjection {
    render::PickGeometry geometry;
    render::ScreenMap map;
    std::pair<double, double> rounding{0.0, 0.0};
    std::function<double(double u, double v)> height;
};

/// The drawable pixel of world corner (u, v): the pick's forward rule (render::corner_point at the
/// ground height plus kGhostOutlineLift, plus the rounding), through the screen map.
[[nodiscard]] std::pair<double, double> project_corner(const OutlineProjection& p, double u,
                                                       double v);

/// Draws `edges` as 1-pixel lines (Bresenham between the projected corners' pixels) of `rgb`
/// into an XRGB8888 frame of w x h pixels, clipped to it. Returns the pixels written.
std::uint64_t draw_outline_xrgb(std::span<const CornerEdge> edges, const OutlineProjection& p,
                                std::uint32_t* xrgb, int w, int h, std::uint32_t rgb);

/// The same into a straight-alpha RGBA image, opaque, only where the image is fully transparent:
/// the drawn chrome stays on top (the GPU path's UI overlay). Returns the pixels written.
std::uint64_t draw_outline_rgba_under(std::span<const CornerEdge> edges, const OutlineProjection& p,
                                      assets::Rgba& canvas, std::uint32_t rgb);

} // namespace anno::app
