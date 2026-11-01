// The ship-founding transaction (task MAP-2; contract: docs/sim/ship-founding.md section 16;
// evidence: docs/re/ship-founding.md sections 8-15). A FoundCmd is applied through
// Commands::apply (apply.cpp) like every command; this is the friend of World that does the work.
//
// Shape: preparation reads the world and decides; it writes nothing. Commit then applies every
// effect with no fallible step left: the single operation that can still refuse (building
// creation) runs first, on a city that exists only locally, and refuses without touching the
// island (create_building validates before it mutates, and creation_problem is the same function
// the preparation called). A rejected command therefore leaves the state hash, the PRNG cursor and
// every byte of the world unchanged.
//
// Reveal (docs/re/ship-founding.md 15.4) is ported literally, including the original's
// single-visit quirk; island objects (treasure, mountains) are not modelled and are named in the
// contract as omitted.
#pragma once

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <anno/sim/apply.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/reject.hpp>

namespace anno::sim {

struct Command;

struct Founding {
    /// Validates and applies one FoundCmd. Total. On success `records[0].building` is the new
    /// warehouse; on a rejection nothing changed.
    static ApplyResult apply(World& w, const Command& command, const FoundCmd& found,
                             const InAdvance* in_advance = nullptr);

    /// The reveal's centre (AnnoWelt 0x10085f31): the origin plus (w - 1) / 2 and (h - 1) / 2 of
    /// the ACCEPTED rotation's footprint, truncated; not the parity terms the territory claim uses.
    [[nodiscard]] static std::pair<std::int64_t, std::int64_t>
    reveal_centre(std::int64_t origin_x, std::int64_t origin_y, std::uint32_t w, std::uint32_t h);

    /// The reveal's visited city (docs/re/ship-founding.md 15.4 points 2-4): the lowest-numbered
    /// city owning a kept sample, i.e. a sample tile whose owner is not `neutral_owner`, is a
    /// present city of the island whose player mask lacks `player`, and lies within `radius` of the
    /// centre (cx, cy) by the half-width table `half_width` (index |dy|, inclusive); nullopt when
    /// none or when every present city already knows the player (the early-out through the
    /// island's complete word). Pure; reads the tile owners and the masks as they are.
    [[nodiscard]] static std::optional<std::uint32_t>
    reveal_city(const Island& island, std::uint32_t neutral_owner, PlayerId player, std::int64_t cx,
                std::int64_t cy, std::uint32_t radius,
                const std::vector<std::uint32_t>& half_width);
};

} // namespace anno::sim
