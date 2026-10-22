// The placement rules of Build (docs/sim/state.md §8.4 steps 2 and 3, stage S10).
#include <cstdint>
#include <optional>

#include <anno/sim/placement.hpp>

namespace anno::sim {

Reject footprint_owner_rule(const Island& island, const SimConfig& cfg, const BuildingType& type,
                            const PlacementRecord& rec, CityNo city) {
    const auto [fw, fh] = footprint(type, rec.rotation);
    const TileLayer& tiles = island.tiles;
    if (!inside_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw,
                      fh)) {
        return Reject::OutOfIsland;
    }
    const TileBox box =
        clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw, fh);
    // D 0x10058890: every tile is neutral or carries one common owner, and that owner is
    // the target city. An all-neutral footprint has no owner (the original's lookup ends
    // at the city numbered neutral_owner, which does not exist).
    std::optional<std::uint32_t> common;
    for (std::int64_t y = box.y0; y < box.y1; ++y) {
        for (std::int64_t x = box.x0; x < box.x1; ++x) {
            const std::uint32_t owner =
                tiles.owner(static_cast<std::uint64_t>(y * tiles.width() + x));
            if (owner == cfg.neutral_owner) {
                continue;
            }
            if (common.has_value() && *common != owner) {
                return Reject::TileNotOwned;
            }
            common = owner;
        }
    }
    if (!common.has_value() || *common != city.v) {
        return Reject::TileNotOwned;
    }
    return Reject::None;
}

Reject stand_in_placement_rule(const Island& island, const BuildingType& type,
                               const PlacementRecord& rec) {
    const auto [fw, fh] = footprint(type, rec.rotation);
    const TileLayer& tiles = island.tiles;
    const TileBox box =
        clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw, fh);
    for (std::int64_t y = box.y0; y < box.y1; ++y) {
        for (std::int64_t x = box.x0; x < box.x1; ++x) {
            if (tiles.object(static_cast<std::uint64_t>(y * tiles.width() + x)) != 0) {
                return Reject::Occupied;
            }
        }
    }
    return Reject::None;
}

} // namespace anno::sim
