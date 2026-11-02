// The placement rules of Build (docs/sim/state.md §8.4 steps 2 and 3, stage S10).
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/placement.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>

namespace anno::sim {

namespace {

std::uint32_t group_of(std::uint32_t prodtype) {
    return ((prodtype - 1U) & ~0x1fU) + 1U;
}

enum class Class : std::uint8_t { Ordinary, Coast, Road, Other };

Class classify(const BuildingType& t, const PlacementParams& p) {
    // The original's flag word is HEAD u16 +0x1c; a wider word is not evidenced (Other).
    if (t.flags_word > 0xffffU || (t.flags_word & p.coverage_flag) != 0) {
        return Class::Other;
    }
    if (is_land_road(t)) {
        return Class::Road;
    }
    if ((t.flags_word & ~p.ignored_flag_bits) == p.ordinary_class) {
        return Class::Ordinary;
    }
    if (site::coast_class_supported(t.flags_word)) {
        return Class::Coast;
    }
    return Class::Other;
}

/// The window result of a land road (AnnoGame 0x10008f90 for flag word & 3 in {0, 2, 3}), the slope
/// test of the slope class, and the level floor. Unsupported when a vertex cannot be read.
PlacementResult road_window(const VertexGrid& grid, const BuildingType& t,
                            const PlacementRecord& rec, std::int64_t fw, std::int64_t fh,
                            const PlacementParams& params, const RoadParams& road) {
    constexpr PlacementResult kUnsupported{Reject::Unsupported, 0};
    const auto w = static_cast<std::size_t>(fw + 1);
    std::vector<std::uint8_t> v(w * static_cast<std::size_t>(fh + 1));
    unsigned mx = 0;
    for (std::int64_t dy = 0; dy <= fh; ++dy) {
        for (std::int64_t dx = 0; dx <= fw; ++dx) {
            const std::optional<std::uint8_t> at = grid.at(rec.x + dx, rec.y + dy);
            if (!at) {
                return kUnsupported;
            }
            v[static_cast<std::size_t>(dy) * w + static_cast<std::size_t>(dx)] = *at;
            mx = std::max<unsigned>(mx, *at);
        }
    }
    unsigned lo = 0xff, hi = 0;
    for (std::size_t y = 0; y < static_cast<std::size_t>(fh); ++y) {
        for (std::size_t x = 0; x < static_cast<std::size_t>(fw); ++x) {
            const unsigned cell = std::min({v[y * w + x], v[y * w + x + 1], v[(y + 1) * w + x],
                                            v[(y + 1) * w + x + 1]}) &
                                  0xf0U;
            lo = std::min(lo, cell);
            hi = std::max(hi, cell);
        }
    }
    const std::uint32_t mode = t.flags_word & 3U;
    const unsigned result = mode == 0 ? hi : mode == 2 ? lo : (mx & 0xf0U); // mode 3
    // The class test runs only with flag bit 4 (A & 0x1000004 == 4, AnnoGame 0x10009111); a data-defined
    // slope-class road without that bit has no slope test, like the original.
    if ((t.flags_word & 4U) != 0 && (t.flags_word & 0x7000U) == road.slope_class &&
        mx - lo > road.slope_limit) {
        return {Reject::TerrainRefused, 0};
    }
    if (result < params.min_level) {
        return {Reject::TerrainRefused, 0};
    }
    return {Reject::None, static_cast<std::uint8_t>(result)};
}

} // namespace

bool is_land_road(const BuildingType& t, const RoadParams& p) {
    const std::uint32_t cls = t.flags_word & 0x7000U;
    return t.prodtype == p.prodtype && t.flags_word <= 0xffffU && (t.flags_word & 0x10U) == 0 &&
           (cls == p.road_class || cls == p.slope_class) && (t.flags_word & 3U) != 1U &&
           t.width > 0 && t.height > 0;
}

PlacementResult placement_rule(const Island& island, const TypeRegistry& types,
                               const BuildingType& effective, const PlacementRecord& rec,
                               const PlacementParams& params) {
    constexpr PlacementResult kUnsupported{Reject::Unsupported, 0};
    // R3 class gate first, as the oracle's engine_place: no flag word outside the two evidenced
    // classes, no rotation above 3 (uninitialised footprint bytes in the original) and no empty
    // footprint ever gets a default answer.
    const Class cls = classify(effective, params);
    const auto [fw, fh] = footprint(effective, rec.rotation);
    if (cls == Class::Other || rec.rotation > 3 || fw == 0 || fh == 0) {
        return kUnsupported;
    }
    // R1 bounds, 64-bit (the original compares 32-bit values of 16-bit fields, no wrap).
    const TileLayer& tiles = island.tiles;
    const site::Origin origin{rec.x, rec.y};
    if (!site::footprint_inside(tiles, origin, fw, fh)) {
        return {Reject::OutOfIsland, 0};
    }
    // R2 occupancy, row-major like the original: the first tile that decides wins.
    for (std::int64_t dy = 0; dy < fh; ++dy) {
        for (std::int64_t dx = 0; dx < fw; ++dx) {
            const std::int64_t lx = rec.x - std::int64_t{tiles.x0()} + dx;
            const std::int64_t ly = rec.y - std::int64_t{tiles.y0()} + dy;
            const std::uint32_t id = tiles.object(static_cast<std::uint64_t>(ly) * tiles.width() +
                                                  static_cast<std::uint64_t>(lx));
            if (id == 0) {
                continue;
            }
            const BuildingType* existing = types.find(TypeId{id});
            if (existing == nullptr) {
                return kUnsupported;
            }
            if (site::overlap_with_existing(effective, *existing) != site::Compat::Compatible) {
                return {Reject::Occupied, 0};
            }
            // R-A3: a group-0xc1 piece that is not itself a land road (ramp, pier) is not replaced
            // by a road (the original accepts it; docs/sim/road-placement.md section 3).
            if (cls == Class::Road && group_of(existing->prodtype) == 0xc1U &&
                !is_land_road(*existing)) {
                return {Reject::Occupied, 0};
            }
        }
    }
    // R4 / R5: the vertex window.
    const VertexGrid grid = VertexGrid::of(island);
    if (!grid.ok()) {
        return kUnsupported;
    }
    if (cls == Class::Road) {
        return road_window(grid, effective, rec, fw, fh, params, RoadParams{});
    }
    if (cls == Class::Coast) {
        const site::TerrainVerdict v =
            site::coast_height_check(grid, effective, rec.rotation, origin);
        switch (v.result) {
        case site::TerrainResult::Accepted:
            return {Reject::None, v.result_byte};
        case site::TerrainResult::Rejected:
            return {Reject::TerrainRefused, 0};
        default:
            return kUnsupported;
        }
    }
    std::optional<std::uint8_t> level;
    for (std::int64_t dy = 0; dy <= fh; ++dy) {
        for (std::int64_t dx = 0; dx <= fw; ++dx) {
            const std::optional<std::uint8_t> v = grid.at(rec.x + dx, rec.y + dy);
            if (!v) {
                return kUnsupported;
            }
            if (!level) {
                level = v;
            } else if (*level != *v) {
                return {Reject::TerrainRefused, 0};
            }
        }
    }
    const std::uint32_t v = *level;
    if (params.level_quantum > 1 && v % params.level_quantum != 0) {
        return {Reject::TerrainRefused, 0};
    }
    if (v < params.min_level && group_of(effective.prodtype) != params.exempt_group) {
        return {Reject::TerrainRefused, 0};
    }
    return {Reject::None, static_cast<std::uint8_t>(v)};
}

PlacementResult stand_in_placement_seam_fn(void*, const Island& island, const TypeRegistry&,
                                           const BuildingType& type, const PlacementRecord& rec) {
    return {stand_in_placement_rule(island, type, rec), 0};
}

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
