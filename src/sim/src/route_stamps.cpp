// The local-unit query of the route engine (see route_stamps.hpp).
#include <algorithm>

#include <anno/sim/route_stamps.hpp>

namespace anno::sim {

std::int32_t ship_cell_of(std::int32_t q8) {
    if (q8 < 0) {
        return 0;
    }
    return (q8 / 256) >> 4;
}

std::vector<RouteStamp> local_ship_stamps(const ShipSpatialView& view, const LocalStampQuery& query,
                                          const RouteLimits& limits, LocalStampStats* stats) {
    LocalStampStats local;
    LocalStampStats& st = stats != nullptr ? *stats : local;
    st = {};
    std::vector<RouteStamp> stamps;
    if (limits.unit_cap == 0 || query.radius_q8 < 0) {
        return stamps;
    }
    const std::int64_t r = query.radius_q8;
    const std::int64_t r2 = r * r;
    const std::int32_t cells_x = std::max((query.world_w + 15) / 16, 0);
    const std::int32_t cells_z = std::max((query.world_h + 15) / 16, 0);
    const auto clip_cell = [](std::int64_t q8, std::int32_t cells) {
        const std::int32_t c =
            q8 < 0 ? 0
                   : static_cast<std::int32_t>(std::min<std::int64_t>((q8 / 256) >> 4, 0x7fffffff));
        return std::min(c, cells - 1);
    };
    const std::int32_t cx0 = clip_cell(static_cast<std::int64_t>(query.x_q8) - r, cells_x);
    const std::int32_t cx1 = clip_cell(static_cast<std::int64_t>(query.x_q8) + r, cells_x);
    const std::int32_t cz0 = clip_cell(static_cast<std::int64_t>(query.z_q8) - r, cells_z);
    const std::int32_t cz1 = clip_cell(static_cast<std::int64_t>(query.z_q8) + r, cells_z);
    if (cells_x == 0 || cells_z == 0) {
        return stamps;
    }
    std::vector<LocalShip> hits; // accepted units in scan order
    std::vector<LocalShip> cell;
    for (std::int32_t cz = cz0; cz <= cz1 && !st.capped; ++cz) {
        for (std::int32_t cx = cx0; cx <= cx1 && !st.capped; ++cx) {
            cell.clear();
            view.ships_in_cell(cx, cz, cell);
            ++st.cells_scanned;
            std::sort(cell.begin(), cell.end(),
                      [](const LocalShip& a, const LocalShip& b) { return a.id < b.id; });
            for (const LocalShip& s : cell) {
                ++st.ships_examined;
                if (ship_cell_of(s.x_q8) != cx || ship_cell_of(s.z_q8) != cz) {
                    continue; // listed under a cell it does not belong to
                }
                const std::int64_t dx = static_cast<std::int64_t>(s.x_q8) - query.x_q8;
                const std::int64_t dz = static_cast<std::int64_t>(s.z_q8) - query.z_q8;
                if (dx * dx + dz * dz > r2 || (s.state_word & 0xau) != 0 || !s.ship_group) {
                    continue;
                }
                hits.push_back(s);
                if (hits.size() >= limits.unit_cap) {
                    st.capped = true;
                    break;
                }
            }
        }
    }
    for (const LocalShip& s : hits) {
        if (!s.has_direction || s.id == query.self_id) {
            continue;
        }
        // (x + dir.x * 25.0, z + dir.z * 25.0) truncated toward zero (ftol), exactly, in 2^-24 tile
        // units.
        const std::int64_t px =
            static_cast<std::int64_t>(s.x_q8) * 65536 + static_cast<std::int64_t>(s.dir_x_q24) * 25;
        const std::int64_t pz =
            static_cast<std::int64_t>(s.z_q8) * 65536 + static_cast<std::int64_t>(s.dir_z_q24) * 25;
        stamps.push_back({static_cast<std::int32_t>(px / 16777216),
                          static_cast<std::int32_t>(pz / 16777216), StampKernel::K5});
    }
    return stamps;
}

} // namespace anno::sim
