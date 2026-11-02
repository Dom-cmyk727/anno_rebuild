// The route engine's local-unit query (MAP-NAV-2 package C, requirement 3): which ships near the
// start stamp their look-ahead point as ship-density for the goal-candidate choice. The original's
// query is registry slot +0x14 (0x100983a0) called by 0x100a8c50 (docs/re/ship-navigation.md 9.4,
// RI 6 and RK 2): the registry scans 16 x 16-tile cells by z-cell rows ascending (outer), x-cell
// columns ascending (inner), keeps a unit if dx^2 + dz^2 <= r^2 (inclusive, r = 5.0 for ships),
// (state & 0xa) == 0 and its class group is the ship group, and STOPS at `cap` hits (0x40; later
// units are silently dropped). The consumer then skips a unit without a direction and the querying
// ship itself (both still counted against the cap), and stamps (x + v.x * 25.0, z + v.z * 25.0)
// with the 5 x 5 kernel, where v is the unit's VELOCITY: the segment direction already multiplied
// by speed and the segment's factor (slot +0x60 returns unit +0x20; docs/re/ship-navigation.md
// 7.2, 7.3, 9.4), in tiles per stepper unit (5 ticks). It is not a normalised heading.
//
// ADAPTATION (docs/sim/ship-navigation.md section 5): the sim has no cell lists, so the in-cell
// order is ascending figure id instead of registry insertion order. The query touches only the
// cells that the radius overlaps, never the whole registry: its work is bounded by the cells
// scanned and the ships in them. Pure: no state, deterministic, independent of the view's internal
// order.
#pragma once

#include <cstdint>
#include <vector>

#include <anno/sim/route.hpp>

namespace anno::sim {

/// One ship as the spatial view reports it.
struct LocalShip {
    std::uint64_t id = 0;
    /// Position, Q24.8 tiles.
    std::int32_t x_q8 = 0, z_q8 = 0;
    /// The ship's state word (bits 0x8 and 0x2 exclude it from the query).
    std::uint32_t state_word = 0;
    /// Class group of the ships (ids 113..128 in the original); other figures must not be listed
    /// or must have this false.
    bool ship_group = true;
    /// False = the original's NULL direction pointer: the consumer skips the unit.
    bool has_direction = true;
    /// The velocity of the current segment (`unit +0x20`): direction * speed * factor, in tiles per
    /// stepper unit (5 ticks) as 2^-32 tile, signed. 0 when the segment does not move (turn, idle,
    /// stop). Bounded by the type speed (<= 2^28) because the factor is <= 100 percent, so the
    /// stamp arithmetic below cannot overflow.
    std::int32_t vel_x_q32 = 0, vel_z_q32 = 0;
};

/// The minimal spatial interface: the ships whose position lies in one 16 x 16-tile cell. The cell
/// of a position is (trunc(x_q8 / 256) >> 4, trunc(z_q8 / 256) >> 4) with negative positions in
/// cell 0 (the original clips to the registry grid). Order inside the answer is irrelevant; a ship
/// listed under a cell it does not belong to is ignored.
class ShipSpatialView {
  public:
    virtual ~ShipSpatialView() = default;
    virtual void ships_in_cell(std::int32_t cell_x, std::int32_t cell_z,
                               std::vector<LocalShip>& out) const = 0;
};

/// The cell index of a Q24.8 coordinate (see ShipSpatialView).
[[nodiscard]] std::int32_t ship_cell_of(std::int32_t q8);

struct LocalStampQuery {
    /// The query point: the planning ship's start position, Q24.8.
    std::int32_t x_q8 = 0, z_q8 = 0;
    /// The planning ship, skipped by the consumer.
    std::uint64_t self_id = 0;
    /// Query radius in Q24.8 (ships: LOGIC[0x28] / 8 = 5.0 tiles = 1280).
    std::int32_t radius_q8 = 1280;
    /// The world size in tiles; the cell scan is clipped to the registry grid ceil(size / 16).
    std::int32_t world_w = 0, world_h = 0;
};

struct LocalStampStats {
    std::uint64_t cells_scanned = 0;
    std::uint64_t ships_examined = 0;
    /// The scan stopped at `unit_cap` hits.
    bool capped = false;
};

/// The stamps of the ships around the query point, in the original's order, at most
/// `limits.unit_cap` hits scanned (self and direction-less ships count against the cap, as in the
/// original) -> stamps (fewer when some are skipped). Kernel K5 (ship querier).
[[nodiscard]] std::vector<RouteStamp> local_ship_stamps(const ShipSpatialView& view,
                                                        const LocalStampQuery& query,
                                                        const RouteLimits& limits,
                                                        LocalStampStats* stats = nullptr);

} // namespace anno::sim
