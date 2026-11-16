// UI-MINIMAP-1 step B1: the minimap, drawn as the original's AnnoFrame draws it under the
// `WELTKARTE` chrome's transparent diamond (docs/re/ui.md section 2.9): the parchment
// `KARTE_NORDUNG` (view turn 0), the world image (owner colour x shade per cell, from the
// palette) and the white 1 px view box. Window-independent, no SDL: the window builds the image
// once from the world, keeps the composed raster, and draws it with the HUD (ui_mock.hpp's
// MinimapOverlay).
//
// Every number is data (data/ui/minimap.txt, parse_minimap_config); MinimapConfig's defaults are
// the original's values, each with its address there and in docs/re/ui.md 2.9, and a test checks
// that the shipped file states the same values. What section 2.9 does not establish is a flagged
// default named in docs/open-questions.md (OQ-628, OQ-629, OQ-634, OQ-635..OQ-637).
//
// Axes: the original's minimap has two map axes u, v (0..map_units across the image); a world
// point (wx, wy) is u = map_units * wy / H, v = map_units * wx / W - u is the world's y. The
// swap is the one the original's wrapper does for the camera ("negated and swapped",
// 0x10002990); its direction here is chosen so that the minimap shows the world in the main
// view's handedness (docs/render/iso.md: tile x runs left-down, y right-down), and checked on the
// History Edition capture (OQ-635). Turn 0 only: the view turn is VIEW-TURN-1's packet.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/assets/texture.hpp>
#include <anno/formats/result.hpp>

namespace anno::app {

/// The minimap's numbers (data/ui/minimap.txt). The defaults are the original's (GOG 2.0.0.5).
struct MinimapConfig {
    /// The world image's cells across and down; it always covers the whole world (AnnoWelt
    /// 0x100aaa53..0x100aaa8f: 0x100a40d0(world, 0x100, 0x100); AnnoFrame 0x100027a5).
    std::uint32_t image_w = 256, image_h = 256;
    /// Shades per base colour; the first `transparent` of them draw nothing, so the parchment
    /// shows (AnnoFrame 0x10033943: entries 0 and 1 are 0). Entry (c, k) for k >= transparent is
    /// colour c scaled by (k + shade_add) / shade_div per channel, packed 1:5:5:5
    /// (0x10033954..0x10033a17).
    std::uint32_t shades = 16, transparent = 2, shade_add = 16, shade_div = 31;
    /// The base colours 0xRRGGBB by owner index (0x100338c0..0x1003391a). Which player owns which
    /// index: OQ-629.
    std::vector<std::uint32_t> colours = {0xe6aa10, 0x00bbff, 0xff4400, 0xfff40a, 0x00e6ce,
                                          0xff9e00, 0x15e015, 0xf839ff, 0xfafafa, 0xb2ab00};
    /// A (colour, shade) entry with its own base colour: (9, 2) uses 0x664400
    /// (0x10033954..0x10033961).
    struct Override {
        std::uint32_t colour = 0, shade = 0, rgb = 0;
        bool operator==(const Override&) const = default;
    };
    std::vector<Override> overrides = {{9, 2, 0x664400}};
    /// The owner index of a cell that no visible settlement owns (AnnoWelt 0x100a4001 `mov bl,
    /// 9`): the olive land and, at shade 0, the sea.
    std::uint32_t no_owner = 9;
    /// The shade of a cell: the summed height of the tiles under a window of shade_window x
    /// shade_window cells from the cell, divided by the window's nominal tile count
    /// (shade_window^2 * sx * sy), then ((v & 0xff) + shade_round) >> shade_shift, kept modulo
    /// `shades` as the original's nibble packing keeps it (AnnoWelt 0x100a3dad..0x100a3e90).
    std::uint32_t shade_window = 2, shade_round = 8, shade_shift = 4;
    /// The rectangle: x = W - base/4 + rect_dx, y = rect_dy, base/4 x base/8, then the renderer's
    /// shift for a screen wider than 800 (AnnoFrame 0x1000a038..0x1000a342, 0x10031a40..
    /// 0x10031ef1). The UI layer is never narrower than 1024 (ui_transform.hpp), so the 800 and
    /// 640 classes are not modelled; the scale s is 1.
    int base = 1024, rect_dx = -3, rect_dy = 12, shift_x = 2, shift_y = -2;
    /// The diamond inside the rectangle: centre (s * diamond_cx, s * diamond_cy), half-axes s *
    /// diamond_ax and s * diamond_ay before the turn by diamond_angle degrees (turn 0: 45 - 90 *
    /// 0) (0x10031a40..0x10031ef1; transform 0x100306f0..0x10030927).
    double diamond_cx = 128.0, diamond_cy = 64.0, diamond_ax = 88.0, diamond_ay = 44.0;
    double diamond_angle = 45.0;
    /// Map units across the whole world on each axis (0x10087b90 = 128.0).
    double map_units = 128.0;
    /// The parchment: `KARTE_NORDUNG` id 52 (+ the view turn, 0 here) of fenster.tex
    /// (0x10002842..0x10002867), its first `parchment_rows` rows of a 256-row texture shown over
    /// the rectangle's height (v up to 133/256, 0x10031dd3).
    std::string parchment_file = "toolgfx/fenster.tex";
    int parchment_id = 52;
    int parchment_rows = 133;
    /// Where the parchment and the map quads land relative to the rectangle: one pixel right and
    /// down. Measured, not read: the parchment's best fit on the GOG shot (docs/re/ui.md 2.9;
    /// UI-MINIMAP-1 B1: shift 0 against +-1 gives median 22 against 40+ sum-RGB) and the
    /// islands' best fit on the History Edition capture (x +1). The view box is not shifted (no
    /// evidence either way). OQ-628.
    int quad_dx = 1, quad_dy = 1;
    /// The view box (0x1002fde0..0x10030316): corners at e = 2^level * box_extent / W and
    /// box_skew * e (map units) around the centre shifted by box_shift / (W / 2), in the colour
    /// 0x1009cfe8..0x1009cff4 = (1, 1, 1, 1).
    double box_extent = 2200.0, box_skew = 0.3, box_shift = 332.8;
    std::uint32_t box_colour = 0xffffff;
    /// The minimap zoom z at the start (AnnoFrame +0x18 = 1.0: the whole world).
    double zoom_start = 1.0;

    bool operator==(const MinimapConfig&) const = default;
};

/// Parses data/ui/minimap.txt (`anno1503-ui-minimap 1`, then `key value...` lines, `#` comments;
/// data/ui/README.md). A key not given keeps its default. An error names the line.
[[nodiscard]] formats::Result<MinimapConfig, std::string>
parse_minimap_config(std::string_view text);
[[nodiscard]] formats::Result<MinimapConfig, std::string>
load_minimap_config(const std::filesystem::path& path);

/// One island of the world as the minimap reads it: its rectangle in world tiles, its HEIGHT grid
/// (one byte per tile, row-major) and, per tile, the owner index the image shows (empty: every
/// tile is MinimapConfig::no_owner).
struct MinimapIsland {
    std::uint32_t x = 0, y = 0, w = 0, h = 0;
    std::vector<std::uint8_t> heights;
    std::vector<std::uint32_t> owners;
};

/// The world in tiles and its islands. Outside every island the height is 0 (the sea) and the
/// owner is no_owner.
struct MinimapWorld {
    std::uint32_t w = 0, h = 0;
    std::vector<MinimapIsland> islands;
};

/// The world image: image_w x image_h cells, row-major (row = world y), each owner * shades +
/// shade (the original's byte owner * 16 + shade, AnnoWelt 0x100a3cc0..0x100a40bb):
/// - shade: the cell's window [x, x + shade_window) cells, i.e. tiles [trunc(x * sx),
///   trunc((x + shade_window) * sx)) clamped to the world, likewise in y (sx = W / image_w as a
///   float, as the original stores it); v = trunc(sum of heights / (shade_window^2 * sx * sy));
///   shade = (((v & 0xff) + shade_round) >> shade_shift) mod shades. The original sums
///   height * 0.04 in float and divides by 0.04 again; that rounding is not modelled (OQ-636).
/// - owner: the tile at (trunc((x + 0.5) * sx), trunc((y + 0.5) * sy)).
/// Empty when the world or the image has no cells.
[[nodiscard]] std::vector<std::uint16_t> build_minimap_image(const MinimapConfig& cfg,
                                                             const MinimapWorld& world);

/// The palette, one RGBA entry per image value (colours.size() * shades entries): transparent
/// (all 0) for the first `transparent` shades, else the original's 1:5:5:5 entry (R and G from
/// (c + 4) scaled, B from (c + 4) >> 3 scaled, each floor(x * (k + shade_add) / shade_div),
/// clamped to 31) as the 16-bit RGB565 frame shows it: red and blue (v * 255 + 15) / 31, green
/// as the 6-bit (v << 1) | (v >> 4), (g6 * 255 + 31) / 63 (matched on the GOG shot's land
/// colours; OQ-628).
[[nodiscard]] std::vector<std::array<std::uint8_t, 4>> minimap_palette(const MinimapConfig& cfg);

/// The minimap's rectangle in a UI layer `ui_w` pixels wide (section 2.9 Q1/Q7): x = ui_w -
/// base/4 + rect_dx + shift_x, y = rect_dy + shift_y, base/4 x base/8. At 1024: (767, 10) 256 x
/// 128.
struct MinimapRect {
    int x = 0, y = 0, w = 0, h = 0;
    double s = 1.0;
    bool operator==(const MinimapRect&) const = default;
};
[[nodiscard]] MinimapRect minimap_rect(const MinimapConfig& cfg, int ui_w);

/// The part of the world the minimap shows, in fractions of the two map axes: [u0, u1] x [v0,
/// v1] = [c - z/2, c + z/2], pushed back inside [0, 1] at the edges (0x10033710,
/// 0x1002fdf9..0x1002feaa). At z = 1 it is the whole world.
struct MinimapWindow {
    double u0 = 0.0, v0 = 0.0, u1 = 1.0, v1 = 1.0;
};
[[nodiscard]] MinimapWindow minimap_window(double zoom, double cu, double cv);

/// The screen point of map point (u, v) (map units, 0..map_units): du, dv in [-1, 1] across the
/// window, then x = rect.x + s * (diamond_cx - diamond_ax * (cos a * dv - sin a * du)), y =
/// rect.y + s * (diamond_cy + diamond_ay * (cos a * du + sin a * dv)) (0x100306f0..0x10030927,
/// the same in 0x1002fde0 for the box).
[[nodiscard]] std::pair<double, double> minimap_screen(const MinimapConfig& cfg,
                                                       const MinimapRect& rect,
                                                       const MinimapWindow& window, double u,
                                                       double v);

/// One line of the view box in screen coordinates.
struct MinimapSegment {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};

/// The view box (0x1002fde0..0x10030316, turn 0): centre P = (u, v) of the camera in map units
/// (camera tile (wx, wy): u = map_units * wy / H, v = map_units * wx / W), shifted by f =
/// box_shift / (W / 2) on both axes (W / 2 an integer division, `sar`); e = level_factor *
/// box_extent / W, g = -box_skew * e; corners A = (P.u - g, P.v + e), B = (P.u + e, P.v - g),
/// C = (P.u + g, P.v - e), D = (P.u - e, P.v + g); segments A-B, B-C, C-D, D-A, each clipped
/// to the window (Cohen-Sutherland, 0x1002fb70) and transformed by minimap_screen. A segment
/// outside the window is left out. `level_factor` is 2^level for the original's zoom levels
/// 0..2 (0x1002ff5b: 1 << [+0x8ee8]); the rebuild's continuous zoom passes the ratio of the
/// closest level's tile width to the shown one (an extension, the same at the three levels).
/// Which camera point the original's centre is, and so whether the shift f applies to the
/// view's own centre: OQ-637.
[[nodiscard]] std::vector<MinimapSegment>
minimap_view_box(const MinimapConfig& cfg, const MinimapRect& rect, const MinimapWindow& window,
                 double camera_wx, double camera_wy, std::uint32_t world_w, std::uint32_t world_h,
                 double level_factor);

/// The pixels a 1 px line through each segment covers, both ends included: the screen point (x,
/// y) is pixel (floor(x + 0.5), floor(y + 0.5)) (Direct3D 7's pixel centres at integer
/// coordinates; the exact line rasterisation is OQ-628). Sorted and unique.
[[nodiscard]] std::vector<std::pair<int, int>>
minimap_line_pixels(const std::vector<MinimapSegment>& segments);

/// The parchment and the world image composed for `rect` (the window `window`), straight alpha,
/// rect.w + max(0, quad_dx) x rect.h + max(0, quad_dy) pixels whose top-left is (rect.x,
/// rect.y), both quads at the quad offset: the parchment's first parchment_rows rows stretched
/// to rect.h rows (linear between the two source rows around (r + 0.5) * rows / rect.h - 0.5,
/// premultiplied; a pixel less than half covered is left out; OQ-628), columns 1:1; over it every
/// pixel whose centre (its integer coordinates, as Direct3D 7 samples) lies inside the window's
/// diamond takes the palette entry of the image cell under it (nearest cell; OQ-628), unless that
/// entry is transparent. `parchment` null: no parchment. `image` must hold image_w * image_h
/// values.
[[nodiscard]] assets::Rgba compose_minimap(const MinimapConfig& cfg, const MinimapRect& rect,
                                           const MinimapWindow& window,
                                           const std::vector<std::uint16_t>& image,
                                           const std::vector<std::array<std::uint8_t, 4>>& palette,
                                           const assets::Rgba* parchment);

} // namespace anno::app
