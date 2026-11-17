// UI-INFO-2b: the info panel's building picture - the building and its green entrance arrows
// composed in one canvas sized over all four turns, and the size it is drawn at in the box.
// Rules and evidence: docs/re/ui.md section 2.5 (GOG View.dll 0x1001d760 / 0x1001d870,
// AnnoGame.dll 0x1000bb60, AnnoDisplay.dll 0x1000ac80); the reference is tools/ui_layout.py
// (preview_parts, preview_canvas, preview_scaled_size), which this code must equal pixel for pixel.
#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <anno/assets/texture.hpp>
#include <anno/formats/objectpos.hpp>

namespace anno::app::info_preview {

/// One entrance arrow of a type: a `CONPO2` record (3 bytes). `x3` / `y3` count thirds of a tile
/// from the footprint's corner, `side` is the footprint side it points out of: 0 = -x, 1 = +y,
/// 2 = +x, 3 = -y (AnnoGame.dll 0x1000bb60's side table 0x10021828).
struct EntranceArrow {
    std::uint8_t x3 = 0, y3 = 0, side = 0;
    bool operator==(const EntranceArrow&) const = default;
};

/// The arrows the picture draws from a decoded Anlagen `OBJECTPOS` record (formats::objectpos:
/// its CONPO2 records, AnnoGame.dll loader 0x10009e3e..0x10009edf). A record whose side is not
/// 0..3 is skipped (types 261 and 1571 carry 215; the original indexes past its side table
/// there). `CONPOS` (one type, 1542) is not drawn: the original's loader drops y and never writes
/// the side.
std::vector<EntranceArrow> entrance_arrows(const formats::objectpos::ObjectPos& objectpos);

/// One image of the picture: its pixels (RGBA, alpha 0 = not drawn) and the pixel that lands on
/// its projected point (a frame anchor, signed).
struct Sprite {
    const assets::Rgba* image = nullptr;
    int anchor_x = 0, anchor_y = 0;
};

/// The screen point of world point (x, y) (tiles, height 0) in the preview: View.dll 0x10088af0's
/// camera (Ry 135 degrees, Rx 42 pi / 256, scale 59.1633, as the map's), each coordinate truncated
/// toward zero (AnnoDisplay.dll 0x10030e10, called by the frame rectangle 0x1000ac80).
std::pair<int, int> project(double x, double y);

/// The world point of `arrow` on a `w` x `h` footprint turned `rotation` quarter turns
/// (AnnoGame.dll 0x1000bb60's four cases) and its direction ((rotation + side) & 3).
struct ArrowPlace {
    double x = 0, y = 0;
    unsigned direction = 0;
};
ArrowPlace place_arrow(const EntranceArrow& arrow, unsigned w, unsigned h, unsigned rotation);

/// The arrow figure's frame for `direction` among `frames` frames (one per view direction):
/// ((direction << 2) >> shift) & (frames - 1), shift 0 for 16 or more directions, 1 for 8..15,
/// 2 below (AnnoDisplay.dll tHausFigur slot +0x6c 0x100055b0, its setter 0x100052c7).
std::size_t arrow_frame(unsigned direction, std::size_t frames);

/// UI-INFO-2d (OQ-712): one further sprite set of the type at one turn: its sprite, its offset
/// from the footprint centre (tiles, before the turn) and its height (tiles), and whether it is
/// animated. The type keeps up to four sets in slots at +0x244 (16 bytes: display object, x,
/// height, y; AnnoGame.dll property 0x17 setter 0x1000c13b); the offsets are the set prefix's
/// f32 at bytes 4..15 (P, measured: OQ-750).
struct SetPart {
    Sprite sprite;
    double x = 0, height = 0, y = 0;
    bool animated = false;
};

/// What the picture is made of: the building's sprite (sprite set 0) per turn, the type's
/// further sprite sets per turn, and the arrows with the arrow figure's frames (empty: no
/// arrows).
struct Input {
    /// Footprint in tiles (Anlagen HEAD bytes 0 and 1).
    unsigned w = 1, h = 1;
    /// The building's sprite at turns 0..3.
    std::vector<Sprite> building;
    /// Whether sprite set 0 is animated (it is in no GOG building; see SetPart).
    bool building_animated = false;
    /// The further sprite sets at turns 0..3, in file order (empty: none).
    std::vector<std::vector<SetPart>> sets;
    std::vector<EntranceArrow> arrows;
    std::vector<Sprite> arrow_frames;
};

/// The point of a sprite set offset (x, y) from the footprint centre (cx, cy) at turn `rotation`:
/// AnnoGame.dll type slots 0xa0 / 0xa4 (0x1000b8c0 / 0x1000ba10), cases 0x1000b966..0x1000b99b:
/// (x, y), (y, -x), (-x, -y), (-y, x) added to the centre.
std::pair<double, double> turn_offset(double x, double y, unsigned rotation);

/// project() of a point raised by `height` tiles: screen y less 59.1633 cos(42 pi / 256) per
/// tile (the same camera, View.dll 0x10088af0), truncated once.
std::pair<int, int> project_raised(double x, double y, double height);

/// The picture at `rotation`: the parts of all four turns are measured, the canvas is their
/// largest width and height, the current turn's rectangle sits centred in it (halving toward
/// zero, View.dll 0x1001da40..0x1001da82), then drawn: the still sprite sets (set 0 first, then
/// the further ones in file order; type slot 0xa0), the animated ones (slot 0xa4, the display
/// object's slot 0x60 non-zero), then the arrows (slot 0xa8) - View.dll 0x1001d000's order -
/// every drawn pixel opaque, clipped to the canvas. nullopt when a turn has no building.
std::optional<assets::Rgba> compose(const Input& in, unsigned rotation);

/// View.dll 0x1001db7f..0x1001dbfc: a `w` x `h` canvas in a `bw` x `bh` box is drawn at
/// (trunc(w / s), trunc(h / s)) with s = max(1, w / bw, h / bh), the larger quotient kept as f32.
std::pair<unsigned, unsigned> scaled_size(unsigned w, unsigned h, unsigned bw, unsigned bh);

} // namespace anno::app::info_preview
