// UI-MINIMAP-1 step B2: what the minimap reads of the world, as values the sim thread publishes
// with each PlaySnapshot (play.hpp): every island's tile owners and its cities (owner, the players
// that know the city), and the figures the minimap marks. Plain integers, no sim types: the window
// never sees a World. The minimap's own rules (whose cities it colours, in which colour, which
// figures it marks) are in minimap.hpp and docs/re/ui.md section 2.9.
#pragma once

#include <cstdint>
#include <vector>

namespace anno::app {

/// One city as the minimap's owner rule reads it (AnnoWelt city: number [city+4], owner
/// [city+0x20], the players that know it +0x61c; sim::City number, owner, player_mask).
struct TerritoryCity {
    std::uint32_t number = 0;
    std::uint32_t owner = 0;
    /// The players that know this city (sim::City::player_mask), ascending.
    std::vector<std::uint32_t> known_by;
    friend bool operator==(const TerritoryCity&, const TerritoryCity&) = default;
};

/// One island: its tile rectangle in world tiles and, row-major, each tile's owner value
/// (sim::TileLayer::owner: a city number, or TerritoryView::neutral_owner).
struct TerritoryIsland {
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
    std::vector<std::uint32_t> owner;
    std::vector<TerritoryCity> cities;
    friend bool operator==(const TerritoryIsland&, const TerritoryIsland&) = default;
};

/// Every island's owners. `version` counts the publications whose content differed from the one
/// before (the reader rebuilds its picture only when it changes, as the original's frame rebuilds
/// its texture only when the world image's counter changed, AnnoFrame 0x100030f9..0x10003125);
/// it is not part of the content compared.
struct TerritoryView {
    std::uint64_t version = 0;
    /// The tile owner value of "no city" (sim::SimConfig::neutral_owner; the original's nibble
    /// 0xf, AnnoWelt 0x100a403a).
    std::uint32_t neutral_owner = 0;
    std::vector<TerritoryIsland> islands;
    [[nodiscard]] bool same_content(const TerritoryView& o) const {
        return neutral_owner == o.neutral_owner && islands == o.islands;
    }
};

/// A figure the minimap may mark: its owner, its position in world tiles and the owner's figure
/// list the original keeps it in (player +0x26c + 8 * list: 0 = SOLDAT, 1 = SCHIFF, registered by
/// the SPIELER reader through player slot +0x8c at AnnoWelt 0x1001ddfd (list 0) and 0x1001e06f
/// (list 1)). `flags` is the original's figure flag word (figure slot +0x8c(0)); the rebuild's
/// figures do not carry it (0).
struct MapFigure {
    std::uint32_t owner = 0;
    double x = 0.0, y = 0.0;
    std::uint32_t list = 0;
    std::uint32_t flags = 0;
    friend bool operator==(const MapFigure&, const MapFigure&) = default;
};

} // namespace anno::app
