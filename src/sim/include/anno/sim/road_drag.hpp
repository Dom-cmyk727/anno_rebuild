// Original road drag helpers (ROADS-1 / SIM-ROADS-1, docs/BUILDINGSOBJECTS.md §10.1):
// L-path generation, figure veto check, and BuildCmd drag construction.
#pragma once

#include <cstdint>
#include <vector>

#include <anno/sim/command.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/reject.hpp>

namespace anno::sim {

class World;

/// Original road drag path (AnnoFrame 0x1004b392-0x1004b51f): L path from `start` to `end`,
/// longer axis first, y first on a tie (`|dx| <= |dy|`), truncated at `cap` tiles (default 40).
[[nodiscard]] std::vector<TileXY> road_path(TileXY start, TileXY end, std::uint32_t cap = 40);

/// Evaluates the nearby-figure veto (AnnoFrame 0x1004af90 -> 0x10045080) for a road footprint
/// `[x, x + fw) x [y, y + fh)` placed by `placing`.
[[nodiscard]] Reject check_road_figure_veto(const World& w, PlayerId placing, std::int64_t x,
                                            std::int64_t y, std::uint32_t fw, std::uint32_t fh);

/// Builds a kind-9 `BuildCmd` from `road_path(start, end, cap)` using `road_type` and `rotation`.
[[nodiscard]] BuildCmd make_road_drag_cmd(CityRef city, TypeId road_type, TileXY start, TileXY end,
                                          std::uint8_t rotation = 0, std::uint32_t cap = 40);

/// ROADS-3: read-only multi-tile prefix verdict returned by `World::check_build_path`.
struct RoadPathCheck {
    std::uint32_t valid_prefix = 0; ///< leading site-valid records (stops at first site refusal)
    std::uint32_t affordable_prefix = 0; ///< leading valid records fitting cumulative budget
    Reject first_refusal = Reject::None; ///< refusal at affordable/valid boundary, else None
    friend bool operator==(const RoadPathCheck&, const RoadPathCheck&) = default;
};

} // namespace anno::sim
