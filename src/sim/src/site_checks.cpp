#include <limits>

#include <anno/sim/site_checks.hpp>

namespace anno::sim::site {

namespace {

std::uint32_t group_of(std::uint32_t prodtype) {
    return ((prodtype - 1U) & ~0x1fU) + 1U;
}

std::int64_t centre_axis(std::int64_t click, std::uint32_t d) {
    // C truncating division: (d - 1) / 2 with d == 0 gives 0.
    const std::int64_t half = (std::int64_t{d} - 1) / 2;
    // half >= 0 (d == 0 gives 0): click - half cannot overflow when click >= half, and a
    // click below half clamps to 0 without subtracting.
    return click < half ? 0 : click - half;
}

} // namespace

Origin centred_origin(std::int64_t click_x, std::int64_t click_y, std::uint32_t footprint_w,
                      std::uint32_t footprint_h) {
    return {centre_axis(click_x, footprint_w), centre_axis(click_y, footprint_h)};
}

std::array<std::uint8_t, 4> rotation_trials(std::uint8_t first) {
    const auto r = static_cast<std::uint8_t>(first & 3U);
    return {r, static_cast<std::uint8_t>((r + 1U) & 3U), static_cast<std::uint8_t>((r + 2U) & 3U),
            static_cast<std::uint8_t>((r + 3U) & 3U)};
}

bool footprint_inside(const TileLayer& tiles, Origin origin, std::uint32_t w, std::uint32_t h) {
    // Exact for every i64 origin: the offsets are non-negative differences held in u64.
    if (origin.x < tiles.x0() || origin.y < tiles.y0()) {
        return false;
    }
    const std::uint64_t lx =
        static_cast<std::uint64_t>(origin.x) - static_cast<std::uint64_t>(std::int64_t{tiles.x0()});
    const std::uint64_t ly =
        static_cast<std::uint64_t>(origin.y) - static_cast<std::uint64_t>(std::int64_t{tiles.y0()});
    return lx <= tiles.width() && w <= tiles.width() - lx && ly <= tiles.height() &&
           h <= tiles.height() - ly;
}

bool within_range(std::int64_t click_x, std::int64_t click_y, FixedPos ship,
                  const DistanceRule& rule) {
    // click * 256 - ship must fit i64: |click| <= INT64_MAX / 512 leaves 2^62 for the product
    // and room for the 32-bit ship coordinate (a bound of the arithmetic, not of the rule).
    const std::int64_t click_limit = std::numeric_limits<std::int64_t>::max() / 512;
    if (click_x > click_limit || click_x < -click_limit || click_y > click_limit ||
        click_y < -click_limit) {
        return false;
    }
    // Truncation toward zero of (click - ship) in tiles; C++ integer division truncates.
    const std::int64_t dx = (click_x * 256 - std::int64_t{ship.x}) / 256;
    const std::int64_t dy = (click_y * 256 - std::int64_t{ship.y}) / 256;
    const std::int64_t max = rule.max_distance;
    if (dx > max || dx < -max || dy > max || dy < -max) {
        return false;
    }
    // trunc(sqrt(d2)) <= max  <=>  d2 <= max^2 + 2*max, a bound that fits u64 for every u32
    // max ((2^32-1)^2 + 2*(2^32-1) = 2^64-1). |dx|, |dy| <= max here, so a^2 <= bound and the
    // second comparison subtracts instead of adding (no wrap).
    const auto umax = static_cast<std::uint64_t>(max);
    const std::uint64_t bound = umax * umax + 2 * umax;
    const auto a = static_cast<std::uint64_t>(dx < 0 ? -dx : dx);
    const auto b = static_cast<std::uint64_t>(dy < 0 ? -dy : dy);
    const std::uint64_t a2 = a * a;
    return a2 <= bound && b * b <= bound - a2;
}

Ownership footprint_ownership(const TileLayer& tiles, std::uint32_t neutral_owner, Origin origin,
                              std::uint32_t w, std::uint32_t h) {
    if (!footprint_inside(tiles, origin, w, h)) {
        return Ownership::OutsideIsland;
    }
    for (std::uint32_t dy = 0; dy < h; ++dy) {
        for (std::uint32_t dx = 0; dx < w; ++dx) {
            const Slot idx = tiles.index_of(static_cast<std::int32_t>(origin.x + dx),
                                            static_cast<std::int32_t>(origin.y + dy));
            if (tiles.owner(idx) != neutral_owner) {
                return Ownership::OwnedTile;
            }
        }
    }
    return Ownership::AllNeutral;
}

Compat overlap_with_existing(const BuildingType& placed, const BuildingType& existing) {
    if ((existing.flags_word & 0x20U) != 0) {
        return Compat::Compatible;
    }
    const std::uint32_t pg = group_of(placed.prodtype);
    if (pg != 1U && pg != 0x21U && pg != 0x61U) {
        return Compat::Unsupported;
    }
    if (existing.prodtype == placed.prodtype) {
        return Compat::Unsupported; // needs bit 8 of the existing type's slot +0xd0
    }
    if ((placed.flags_word & 0x10U) != 0 && group_of(existing.prodtype) == 0x141U) {
        return Compat::Compatible;
    }
    return Compat::Incompatible;
}

Compat footprint_occupancy(const TileLayer& tiles, const TypeRegistry& types,
                           const BuildingType& placed, Origin origin, std::uint32_t w,
                           std::uint32_t h) {
    if (!footprint_inside(tiles, origin, w, h)) {
        return Compat::Unsupported;
    }
    Compat worst = Compat::Compatible;
    for (std::uint32_t dy = 0; dy < h; ++dy) {
        for (std::uint32_t dx = 0; dx < w; ++dx) {
            const Slot idx = tiles.index_of(static_cast<std::int32_t>(origin.x + dx),
                                            static_cast<std::int32_t>(origin.y + dy));
            const std::uint32_t id = tiles.object(idx);
            if (id == 0) {
                continue;
            }
            const BuildingType* ex = types.find(TypeId{id});
            const Compat c =
                ex == nullptr ? Compat::Unsupported : overlap_with_existing(placed, *ex);
            if (c == Compat::Incompatible) {
                return Compat::Incompatible;
            }
            if (c == Compat::Unsupported) {
                worst = Compat::Unsupported;
            }
        }
    }
    return worst;
}

bool coast_class_supported(std::uint32_t flags_word) {
    return (flags_word & 0x7017U) == 0x2007U;
}

TerrainVerdict coast_height_check(const VertexGrid& grid, const BuildingType& type,
                                  std::uint8_t rotation, Origin origin) {
    if (!coast_class_supported(type.flags_word)) {
        return {TerrainResult::Unsupported, 0};
    }
    return coast_height_rule(type.width, type.height, rotation,
                             [&](std::uint64_t dx, std::uint64_t dy) {
                                 // dx, dy <= 2^32: refuse a sum that would overflow.
                                 const auto sx = static_cast<std::int64_t>(dx);
                                 const auto sy = static_cast<std::int64_t>(dy);
                                 if (origin.x > std::numeric_limits<std::int64_t>::max() - sx ||
                                     origin.y > std::numeric_limits<std::int64_t>::max() - sy) {
                                     return std::optional<std::uint8_t>{};
                                 }
                                 return grid.at(origin.x + sx, origin.y + sy);
                             });
}

} // namespace anno::sim::site
