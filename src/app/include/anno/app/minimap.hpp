// UI-MINIMAP-1 step B1: the minimap, drawn as the original's AnnoFrame draws it under the
// `WELTKARTE` chrome's transparent diamond (docs/re/ui.md section 2.9): the parchment
// `KARTE_NORDUNG` (view turn 0), the world image (owner colour x shade per cell, from the
// palette) and the white 1 px view box. Window-independent, no SDL: the window builds the image
// from the world, keeps the composed raster, and draws it with the HUD (ui_mock.hpp's
// MinimapOverlay). Step B2: the owner colours from the published territory (territory_view.hpp,
// minimap_owner_layers; the image is rebuilt when it changes) and the figure markers
// (minimap_markers), both read from the snapshot, never from the sim.
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
// History Edition capture (OQ-635). The markers support it: the original passes a figure's +0x5c
// (its y) as the first map coordinate (AnnoFrame 0x10002e85..0x10002eae). Turn 0 only: the view
// turn is VIEW-TURN-1's packet.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <anno/app/territory_view.hpp>
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
    /// UI-MINIMAP-1 fix F2: the parchment quad, from (rect.x + parchment_x0, rect.y +
    /// parchment_y0) to (rect.x + rect.w + parchment_x1, rect.y + rect.h + parchment_y1), its
    /// texture sampled bilinearly with Direct3D's conventions (vertices on pixel edges, x - 0.5,
    /// AnnoFrame 0x10086340; texel centres at + 0.5): pixel (x, y) of the quad reads texel (u, v)
    /// = ((x + 0.5) * width / quad_w - 0.5, (y + 0.5) * parchment_rows / quad_h - 0.5).
    /// Default 0 0 2 2: the code's strip, W' = x1 - x0 + 2 and H' = y1 - y0 + 2 from the
    /// rectangle's corner (0x10031ef1..0x10031f1d), as the History Edition draws it (258 x 130 at
    /// (767, 10) at 1024; its lossless capture he_s10_hud_idle_win: mean 8.3 sum-RGB against
    /// 12.3 for the GOG fit). 1 1 1 1 is the GOG frame's fit (256 x 128 at (768, 11), B1): its
    /// columns land on texel centres, so only its rows are filtered. The x0 and y0 offsets are
    /// not negative (the raster starts at the rectangle's corner). docs/re/ui.md 2.9, OQ-628.
    int parchment_x0 = 0, parchment_y0 = 0, parchment_x1 = 2, parchment_y1 = 2;
    /// UI-MINIMAP-1 fix F2: the frame's colour depth. 32, the History Edition: palette entry (c,
    /// k) = floor(channel * (k + shade_add) / shade_div) per 8-bit channel (its lossless capture
    /// shows entries (9, 3) (109, 104, 0), (9, 4), (9, 5), (9, 6) and (1, 3) exactly), markers
    /// and parchment as they are. 16, the GOG build: the 1:5:5:5 entries (0x10033954..
    /// 0x10033a17) as its RGB565 frame shows them, markers and parchment reduced to RGB565.
    int colour_depth = 32;
    /// The view box (0x1002fde0..0x10030316): corners at e = 2^level * box_extent / W and
    /// box_skew * e (map units) around the centre shifted by box_shift / (W / 2), in the colour
    /// 0x1009cfe8..0x1009cff4 = (1, 1, 1, 1).
    double box_extent = 2200.0, box_skew = 0.3, box_shift = 332.8;
    std::uint32_t box_colour = 0xffffff;
    /// The minimap zoom z at the start (AnnoFrame +0x18 = 1.0: the whole world).
    double zoom_start = 1.0;
    /// UI-MINIMAP-1 C: the top-right + and - (command 0x1b, param 1 / 0) change z by -zoom_step /
    /// +zoom_step (AnnoFrame 0x10033770), then z is clamped to [zoom_min, zoom_max]
    /// (0x10033710; zoom_min is the same float 0x10087bd4 = 0.15 as the step, zoom_max 1.0 =
    /// 0x10086310). z is a float in the original; minimap_zoom_step steps it as one.
    double zoom_min = 0.15, zoom_max = 1.0, zoom_step = 0.15;
    /// UI-MINIMAP-1 C: a click's screen -> world inverse (AnnoFrame 0x100333f0): the point
    /// relative to the wrapper's rectangle (the rectangle before the renderer's shift,
    /// 0x10002a10..0x10002a23), plus (click_dx, click_dy), times click_scale (above 800 px wide:
    /// -6, 0 and 1.03, the float 0x3f83d70a; 0x10031e4f..0x10031e63).
    int click_dx = -6, click_dy = 0;
    double click_scale = 1.03;

    /// The owner rule (UI-MINIMAP-1 B2; AnnoWelt 0x100a3ed7..0x100a40bb, docs/re/ui.md 2.9 Q2):
    /// a tile's city colours its cell with the city's colour byte (city +0x16, written from the
    /// owner's colour slot, player property 11 = +0xa8, at 0x1002f42b and by every recolour
    /// 0x1001cdf0 through city property 0xb, 0x1002f5e7) when the viewing player knows the city
    /// (bit 1 << viewer of city +0x61c) or the world shows every city (world property 5 = 0).
    /// `no_colour`: the city colour of an owner without a colour slot (0xff is stored as 0,
    /// 0x1002f437 and 0x1002f5f5). `all_visible`: world property 5 is 0, i.e. world byte
    /// +0x69cd is non-zero (getter 0x100a33ce); a world reset leaves it 0 (0x100a3a93), so the
    /// original's default is false; only AnnoScene's kind-3 instance sets it (0x1000d740).
    std::uint32_t no_colour = 0;
    bool all_visible = false;
    /// The markers (AnnoFrame 0x10002e48..0x100030cd): the viewer's own figures of list 0
    /// (soldiers) in `marker_list0` unless flags & marker_skip_mask == marker_skip_value, of list
    /// 1 (ships) in `marker_list1`, either in `marker_flagged` when flags & marker_flag; the other
    /// players' list-1 figures in `marker_others` only when `marker_others_shown` (the world's
    /// 16-byte property 0xe, world +0x6938, equals GUID {a2414664-1a42-4f88-aae8-c0b27db939f5},
    /// 0x10002fb1..0x10003017; the rebuild's world has no such id, OQ-630). Colours stored by
    /// 0x10002896..0x100028ab. Each marker is a 1 px square outline of half-size `marker_half`
    /// around its point (draw 0x10003135..0x1000327f: corners (x - 1, y - 1), (x + 1, y + 1); the
    /// GOG shot `construction_open` shows the 3 x 3 ring in 0x00d000 with its centre empty), at
    /// most `marker_max` per frame (0x10033b60: 0x5dc).
    std::uint32_t marker_list0 = 0x00ff00, marker_list1 = 0x00d000, marker_flagged = 0xffffff,
                  marker_others = 0xff3000;
    std::uint32_t marker_flag = 0x10, marker_skip_mask = 0xc0, marker_skip_value = 0x80;
    int marker_half = 1;
    std::uint32_t marker_max = 1500;
    bool marker_others_shown = false;
    /// `MINIMAP_ALERT` slots (AnnoFrame 0x10033bb0: `cmp edi, 8`). Not drawn yet: the snapshot
    /// carries no alerts (OQ-633).
    std::uint32_t alert_max = 8;

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

/// UI-MINIMAP-1 B2: whose eyes the owner colours follow and each player's colour slot.
/// `viewer` is the original's local player (world +0xf8, set by AnnoFrame SetPlayer
/// 0x10009060..0x10009189; the first human slot of a new single-player game, 0x10029100).
/// `colour_of_player[p]` is player p's colour slot (player +0xa8, property 11; HEAD +0x4C or the
/// game start's pool, docs/re/annoscene.md section 9); a player beyond it, or one without a slot,
/// has MinimapConfig::no_colour.
struct MinimapOwnerRule {
    std::uint32_t viewer = 0;
    std::vector<std::optional<std::uint32_t>> colour_of_player;
};

/// The owner index (the palette's base colour) the image shows for a tile of `city`:
/// MinimapConfig::no_owner unless cfg.all_visible or `city.known_by` holds rule.viewer; else the
/// owner's colour slot (no_colour without one). AnnoWelt 0x100a403f..0x100a406c.
[[nodiscard]] std::uint32_t minimap_city_owner(const MinimapConfig& cfg, const TerritoryCity& city,
                                               const MinimapOwnerRule& rule);

/// One island's owner indices per tile (row-major), as build_minimap_image reads them.
struct MinimapOwnerLayer {
    std::int64_t x = 0, y = 0;
    std::uint32_t w = 0, h = 0;
    std::vector<std::uint32_t> owners;
};

/// Every island of `territory` as owner indices: a neutral tile, or one whose city number names no
/// city of its island, is no_owner (the original's 0xf test, 0x100a403a); else
/// minimap_city_owner.
[[nodiscard]] std::vector<MinimapOwnerLayer> minimap_owner_layers(const MinimapConfig& cfg,
                                                                  const TerritoryView& territory,
                                                                  const MinimapOwnerRule& rule);

/// The world image: image_w x image_h cells, row-major (row = world y), each owner * shades +
/// shade (the original's byte owner * 16 + shade, AnnoWelt 0x100a3cc0..0x100a40bb):
/// - shade: the cell's window [x, x + shade_window) cells, i.e. tiles [trunc(x * sx),
///   trunc((x + shade_window) * sx)) clamped to the world, likewise in y (sx = W / image_w as a
///   float, as the original stores it); v = trunc(sum of heights / (shade_window^2 * sx * sy));
///   shade = (((v & 0xff) + shade_round) >> shade_shift) mod shades. The original sums
///   height * 0.04 in float and divides by 0.04 again; that rounding is not modelled (OQ-636).
/// - owner: the tile at (trunc((x + 0.5) * sx), trunc((y + 0.5) * sy)); `owners` (B2: the
///   published territory, minimap_owner_layers) overrides the islands' own `owners` where given.
/// Empty when the world or the image has no cells.
[[nodiscard]] std::vector<std::uint16_t>
build_minimap_image(const MinimapConfig& cfg, const MinimapWorld& world,
                    const std::vector<MinimapOwnerLayer>* owners = nullptr);

/// The palette, one RGBA entry per image value (colours.size() * shades entries): transparent
/// (all 0) for the first `transparent` shades, else, at colour_depth 32 (the History Edition),
/// floor(channel * (k + shade_add) / shade_div) per 8-bit channel; at colour_depth 16 (GOG) the
/// original's 1:5:5:5 entry (R and G from (c + 4) scaled, B from (c + 4) >> 3 scaled, each
/// floor(x * (k + shade_add) / shade_div), clamped to 31) as the 16-bit RGB565 frame shows it:
/// red and blue (v * 255 + 15) / 31, green as the 6-bit (v << 1) | (v >> 4), (g6 * 255 + 31) /
/// 63 (matched on the GOG shot's land colours; OQ-628).
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

/// UI-MINIMAP-1 C: the hit test of a click at UI-layer pixel (x, y) (AnnoFrame 0x10002970 ->
/// 0x100332f0): relative to the wrapper's rectangle, (xr, yr) = (x - (rect.x - shift_x), y -
/// (rect.y - shift_y)) - at 1024 the wrapper's (765, 12), two pixels left of and below the drawn
/// (767, 10) - it must lie in [0, rect.w) x [0, rect.h) and inside the inscribed diamond: with t
/// = yr when yr <= rect.h / 2, else rect.h - yr, -2t <= xr - rect.w / 2 < 2t.
[[nodiscard]] bool minimap_hit(const MinimapConfig& cfg, const MinimapRect& rect, int x, int y);

/// UI-MINIMAP-1 C: the world tile a click at UI-layer pixel (x, y) centres the camera on, turn 0
/// (AnnoFrame 0x100029f0 -> 0x100333f0); nullopt when minimap_hit fails or the world is empty.
/// X = (xr + click_dx) * click_scale, Y = (yr + click_dy) * click_scale, D = X - rect.w / 2;
/// P = image_h / rect.h * (Y + D / 2) (the u axis), Q = image_h / rect.h * (Y - D / 2) (the v
/// axis); turn 0's case (jump table 0x100336a4 -> 0x100334e1): x0 = trunc(world_w * Q /
/// image_w), y0 = trunc(world_h * P / image_h); then through the window (0x1003357e..
/// 0x10033680): wx = trunc(((x0 / world_w - 0.5) * (v1 - v0) + (v0 + v1) / 2) * world_w), wy
/// likewise with y0 and the u span (u is the world's y, "Axes"). trunc is the original's __ftol
/// (0x10077da8). The original then recentres the minimap there (0x10033ad0); the rebuild's
/// window follows the camera every frame, which comes to the same. Computed in double; the
/// original stores some intermediates as floats (a difference far below one tile).
[[nodiscard]] std::optional<std::pair<std::int64_t, std::int64_t>>
minimap_world_at(const MinimapConfig& cfg, const MinimapRect& rect, const MinimapWindow& window,
                 std::uint32_t world_w, std::uint32_t world_h, int x, int y);

/// UI-MINIMAP-1 C: the UI-layer rectangle the minimap takes clicks in: the drawn raster (rect
/// plus the quad offset) and the hit test's wrapper rectangle together (at 1024: x 765..1023, y
/// 10..139). Every press in it is the UI's, so none reaches the map (also the part left of the
/// right chrome, x 765..778 at 1024); only one inside the diamond moves the camera.
[[nodiscard]] MinimapRect minimap_click_area(const MinimapConfig& cfg, int ui_w);

/// UI-MINIMAP-1 C: the minimap zoom after one press of the top-right + (`closer`, param 1: z -
/// zoom_step) or - (z + zoom_step), clamped to [zoom_min, zoom_max] (AnnoFrame 0x10033770 ->
/// 0x10033710). Stepped in float arithmetic, as the original keeps z: from 1.0 the + gives 0.85,
/// 0.7, 0.55, 0.4, 0.25, then 0.15 (0.1 clamped).
[[nodiscard]] double minimap_zoom_step(const MinimapConfig& cfg, double zoom, bool closer);

/// UI-MINIMAP-1 C: the main view's zoom after one press of the magnifier's + (`closer`, command
/// 0xa: level - 1, not below 0, AnnoFrame 0x10007710) or - (command 9: level + 1, not above the
/// last, 0x10007692), as the tile width to zoom to. `level_tile_w` holds the stages' tile
/// widths: the built levels', closest first (84, 42, 20 for the original's three), or the
/// continuous zoom's stages (magnifier_stages). The rebuild's view also zooms continuously; one
/// press goes to the next stage in that direction from `current` (the shown or targeted tile
/// width, which the wheel may have left between two stages): the closest stage wider than it, or
/// the widest stage narrower than it. nullopt when there is none (the clamp: nothing changes).
[[nodiscard]] std::optional<double> magnifier_target(const std::vector<double>& level_tile_w,
                                                     double current, bool closer);

/// UI-MINIMAP-2: the tile width ratio between two of the magnifier's stages past the narrowest
/// or the widest built level. The derived levels the renderer draws below the narrowest halve
/// the tile size each (render::ZoomLevels::with_derived), so each such stage is a derived level's
/// own width; past the widest the same ratio doubles it.
inline constexpr double kMagnifierStageFactor = 2.0;

/// UI-MINIMAP-2: the continuous zoom's magnifier stages, closest first (owner decisions
/// 2026-11-17: ui_6.mp4, the magnifier's - reaches the wheel's furthest zoom-out, "rather one
/// too many than too few"; ui_7.mp4, its + reaches the wheel's closest zoom-in). `built_tile_w`
/// holds the built levels' tile widths, closest first; `min_tile` is the narrowest tile width the
/// wheel reaches (zoom_floor in zoom.hpp), `max_tile` the widest (zoom_ceiling; 0, the default,
/// for none). The stages, closest first:
/// - `max_tile` itself, then the widest built width times kMagnifierStageFactor again and again
///   while the result is still narrower than `max_tile` (widest first);
/// - the built widths;
/// - the narrowest built width divided by kMagnifierStageFactor again and again while the result
///   is still wider than `min_tile`, then `min_tile` itself.
/// So the last press either way lands exactly on the wheel's end, also when that last hop is
/// short. When `min_tile` is not below the narrowest built width (or `max_tile` not above the
/// widest), or is not a positive finite width, that side has no extra stages: the built widths
/// alone, as the original. The stepped zoom keeps the original's clamp to the built levels and
/// does not use these. The wheel's far end depends on the window size: build the stages at each
/// press.
[[nodiscard]] std::vector<double> magnifier_stages(const std::vector<double>& built_tile_w,
                                                   double min_tile, double max_tile = 0.0);

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

/// The box's level factor for a main view that shows tiles `shown_w` px wide, given the tile widths
/// of the zoom levels (closest first; 84, 42, 20 for the original's groups 0..2): 2^k exactly at
/// level k (0x1002ff5b: 1 << level), between two levels 2^(k + t) with t the fraction of the way
/// from level k to k + 1 in log tile width, and beyond the ends 2^k scaled by the tile width ratio
/// (the continuous zoom is an extension; the original only has the levels). 1 without levels.
[[nodiscard]] double minimap_level_factor(const std::vector<double>& level_tile_w, double shown_w);

/// One marker: its integer screen point (the transformed point truncated, as the original's
/// ftol does, 0x10003193..0x100031cd) and its colour 0xRRGGBB: as it is at colour_depth 32, as
/// the 16-bit RGB565 frame shows it at colour_depth 16 (0x00d000 is (0, 210, 0), as on the GOG
/// shot `construction_open`).
struct MinimapMarker {
    int x = 0, y = 0;
    std::uint32_t rgb = 0;
    bool operator==(const MinimapMarker&) const = default;
};

/// The markers of `figures` for `viewer` (MinimapConfig marker rule, turn 0): a figure's point is
/// u = map_units * y / H, v = map_units * x / W (the original passes the figure's +0x5c then
/// +0x54 to the transform, 0x10002e85..0x10002eae; minimap.hpp "Axes"); a point outside the
/// window is left out (0x100306f0 returns 0), and at most marker_max are kept, in the order the
/// original collects them (own list 1, own list 0, then the others' list 1).
[[nodiscard]] std::vector<MinimapMarker>
minimap_markers(const MinimapConfig& cfg, const MinimapRect& rect, const MinimapWindow& window,
                const std::vector<MapFigure>& figures, std::uint32_t world_w, std::uint32_t world_h,
                std::uint32_t viewer);

/// The pixels of each marker's outline (max(|dx|, |dy|) == marker_half around its point), in
/// marker order (a later marker paints over an earlier one).
struct MinimapPixel {
    int x = 0, y = 0;
    std::uint32_t rgb = 0;
    bool operator==(const MinimapPixel&) const = default;
};
[[nodiscard]] std::vector<MinimapPixel> minimap_marker_pixels(const MinimapConfig& cfg,
                                                              const std::vector<MinimapMarker>& m);

/// The pixels a 1 px line through each segment covers, both ends included: the screen point (x,
/// y) is pixel (floor(x + 0.5), floor(y + 0.5)) (Direct3D 7's pixel centres at integer
/// coordinates; the exact line rasterisation is OQ-628). Sorted and unique.
[[nodiscard]] std::vector<std::pair<int, int>>
minimap_line_pixels(const std::vector<MinimapSegment>& segments);

/// The parchment and the world image composed for `rect` (the window `window`), straight alpha,
/// rect.w + max(0, quad_dx, parchment_x1) x rect.h + max(0, quad_dy, parchment_y1) pixels whose
/// top-left is (rect.x, rect.y): the parchment quad (MinimapConfig::parchment_x0..y1, its first
/// parchment_rows rows sampled bilinearly, premultiplied; a pixel less than half covered is left
/// out; reduced to RGB565 at colour_depth 16; UI-MINIMAP-1 fix F2, OQ-628); over it, at the quad
/// offset, every pixel whose centre (its integer coordinates, as Direct3D 7 samples) lies inside
/// the window's diamond takes the palette entry of the image cell under it (nearest cell;
/// OQ-628), unless that entry is transparent. `parchment` null: no parchment. `image` must hold
/// image_w * image_h values.
[[nodiscard]] assets::Rgba compose_minimap(const MinimapConfig& cfg, const MinimapRect& rect,
                                           const MinimapWindow& window,
                                           const std::vector<std::uint16_t>& image,
                                           const std::vector<std::array<std::uint8_t, 4>>& palette,
                                           const assets::Rgba* parchment);

} // namespace anno::app
