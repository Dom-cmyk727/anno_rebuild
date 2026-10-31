// Isolated neutral-tile territory claim of the ship-founding (task MAP-2 U3; research:
// docs/re/ship-founding.md section 11.2, contract: docs/sim/ship-founding.md section 6.5).
//
// The original claims territory with two calls of one routine (AnnoWelt.dll island slot
// +0xe8, 0x10085840): the disc around the footprint (radius R = the type's action radius,
// 24 for the warehouse) and then the footprint rectangle (radius 0). Both paint only tiles
// whose owner is the neutral owner (nibble 15), clipped to the island; own and rival tiles
// are never overwritten and the terrain is not consulted.
//
// This is ownership only. Object discovery, the reveal, events, messages and the city are
// NOT modelled and no Island other than its TileLayer is touched. The only mutation is
// TileLayer::set_owner (dirty-chunk accounting and the state hash follow the existing
// rules); a rejected request leaves the layer untouched.
//
// Arithmetic and the verified domain: the profile and the row bounds are computed in 64
// bits and nothing wraps. The original keeps the profile and the width of each row span in
// BYTES (AnnoWelt.dll 0x100858f7/0x10085908 store T[k] as a byte; 0x10085a12-0x10085a18 pass
// the span width as a byte) in a fixed stack buffer. Emulating the original on a 300 x 300
// island with a 4 x 4 footprint shows the result equal to this port for every tested radius
// up to 126 and different from radius 127 on (fixtures disc-R*-large, disc-R*-original-wraps;
// the span 2 * T + px + 1 passes 255). Radii above that are a DELIBERATE WIDENING, not original
// behaviour; the warehouse (1302) uses 24. `max_radius` is the caller's bound on the O(radius)
// profile: pass the largest action radius of the type data (BuildingType::action_radius), or a
// configured value; a future rules/config field can carry it without a change here.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/island.hpp>

namespace anno::sim::territory {

/// The footprint in world tile coordinates. w and h must be at least 1.
struct Footprint {
    std::int64_t x = 0, y = 0;
    std::uint32_t w = 0, h = 0;
};

enum class Status : std::uint8_t {
    Ok,
    /// The footprint is empty or not fully inside the layer: the original never calls the
    /// claim for such a rectangle and its behaviour is not verified there.
    BadFootprint,
    /// The city owner equals the neutral owner (the claim would be a no-op that reports
    /// success) .
    InvalidOwner,
    /// radius > max_radius (the profile is O(radius) memory; the caller's bound).
    RadiusTooLarge,
};

struct Result {
    Status status = Status::Ok;
    /// Tiles whose owner changed from neutral to the city owner by this call.
    std::uint64_t claimed = 0;
};

/// The disc profile T[0..R] of the original (0x10085840, the loop at 0x100858c8 and the
/// small-radius correction): row pair k (rows cy - k and cy + py + k) spans the columns
/// cx - T[k] .. cx + px + T[k]. R = 0 gives the empty profile (the original takes the
/// rectangle path for radius 0). Integer only; allocates radius + 2 entries, so it refuses
/// (nullopt) a radius above `max_radius`, the caller's bound (see above). Values are not
/// byte-wrapped.
[[nodiscard]] std::optional<std::vector<std::uint32_t>> disc_profile(std::uint32_t radius,
                                                                     std::uint32_t max_radius);

/// Claims the neutral tiles of the footprint rectangle, clipped to the island: the original's
/// radius-0 call. Own, rival and any other non-neutral tile is left as is.
[[nodiscard]] Result claim_neutral_rect(TileLayer& tiles, std::uint32_t neutral_owner,
                                        std::uint32_t city_owner, const Footprint& footprint);

/// Claims the neutral tiles of the disc of `radius` around the footprint, clipped to the
/// island. radius 0 is the original's rectangle call (identical to claim_neutral_rect).
/// The disc's shape depends on the footprint only through its centre tile(s): cx = x +
/// (w - 1) / 2 and cy = y + (h - 1) / 2, extended by px = (w - 1) & 1 and py = (h - 1) & 1.
[[nodiscard]] Result claim_neutral_disc(TileLayer& tiles, std::uint32_t neutral_owner,
                                        std::uint32_t city_owner, const Footprint& footprint,
                                        std::uint32_t radius, std::uint32_t max_radius);

} // namespace anno::sim::territory
