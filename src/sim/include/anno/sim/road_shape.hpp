// ROADS-2a: the original's road reshaper as pure functions (docs/re/annowelt.md "The road
// reshaper's classifier", AnnoWelt 0x1007ef60; table 0x100c9d90; docs/BUILDINGSOBJECTS.md part
// B 10.1 items 7-8). A road tile's piece and rotation follow from its four orthogonal road
// neighbours (mask N=1, E=2, S=4, W=8); only road ids connect. Ramps, the slope junction and
// bridges are not shaped here (ROADS-5, OQ-764/765). Oracle: tools/road_shape.py.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include <anno/sim/ids.hpp>

namespace anno::sim {

/// One table entry: the cobblestone-family id and the rotation for a neighbour mask.
struct RoadShapeEntry {
    std::uint32_t id = 0;
    std::uint8_t rotation = 0;
    friend bool operator==(const RoadShapeEntry&, const RoadShapeEntry&) = default;
};

/// The reshaper's data, the original's values as defaults (D: AnnoWelt 0x100c9d90, 0x1007efc3-
/// 0x1007f049). The table has one entry per mask of the four directions (16).
struct RoadShapeTable {
    /// mask -> (cobblestone id, rotation), 0x100c9d90 (8 bytes per mask).
    std::vector<RoadShapeEntry> by_mask = {
        {2500, 3}, {2508, 1}, {2508, 0}, {2505, 1}, {2508, 3}, {2500, 3}, {2505, 0}, {2507, 1},
        {2508, 2}, {2505, 2}, {2500, 0}, {2507, 2}, {2505, 3}, {2507, 3}, {2507, 0}, {2506, 3}};
    /// The cobblestone straight and its cosmetic variants (0x9c4-0x9c8 folded at 0x1007efd9).
    std::uint32_t cobble_first = 2500;
    std::uint32_t cobble_straight_last = 2504;
    /// Dirt = cobblestone + 20 (0x1007efcf: ids 0x9d8-0x9f5 fold by -0x14).
    std::uint32_t dirt_offset = 20;
    std::uint32_t dirt_first = 2520;
    std::uint32_t dirt_last = 2549;
    /// Every id that connects as a road neighbour: 0x1007f0c0 zeroes ids from 0x9f6 up, then
    /// 0x1007f0cf keeps 0x9c4 and above, so 0x9c4-0x9f5 (2500-2549).
    std::uint32_t road_first = 2500;
    std::uint32_t road_last = 2549;
    /// The pieces the reshaper places (the table's ids); other road ids (ramps, junction, bridges)
    /// connect but are not re-shaped here.
    std::vector<std::uint32_t> shaped = {2500, 2505, 2506, 2507, 2508};
};

/// One reshaped tile: where, the id (in the tile's own family) and the rotation.
struct ShapedRoad {
    TileXY tile;
    std::uint32_t id = 0;
    std::uint8_t rotation = 0;
    friend bool operator==(const ShapedRoad&, const ShapedRoad&) = default;
};

/// Whether `id` connects as a road neighbour.
[[nodiscard]] bool road_connects(const RoadShapeTable& t, std::uint32_t id);
/// `id` folded onto the cobblestone family and its straight variants onto the base; nullopt when
/// it is not a piece the reshaper places.
[[nodiscard]] std::optional<std::uint32_t> road_shaped_base(const RoadShapeTable& t,
                                                            std::uint32_t id);
/// The piece for `mask` in the family of `family_id` (a dirt id gives the dirt piece).
[[nodiscard]] RoadShapeEntry road_shape(const RoadShapeTable& t, std::uint32_t family_id,
                                        unsigned mask);

/// An object already on a tile: its id and rotation.
struct RoadObject {
    std::uint32_t id = 0;
    std::uint8_t rotation = 0;
};
/// The object at a world tile, or nullopt (the world's current objects).
using RoadAt = std::function<std::optional<RoadObject>(TileXY)>;

/// The records a road path places: each path tile shaped from the path and the existing roads
/// (`existing`), in path order, then every existing shaped road beside the path whose piece or
/// rotation changes, in first-touch order (OQ-900). A straight path tile keeps its id from `ids`
/// (the cosmetic variant), a straight neighbour that stays straight keeps its own (OQ-901).
[[nodiscard]] std::vector<ShapedRoad> reshape_road_path(const RoadShapeTable& t,
                                                        const std::vector<TileXY>& path,
                                                        const std::vector<std::uint32_t>& ids,
                                                        const RoadAt& existing);

} // namespace anno::sim
