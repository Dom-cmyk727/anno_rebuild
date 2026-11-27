// UI-RANGE-1 B1: which tiles the original darkens to show a city's range (docs/re/ui.md section
// 12). While a Kontor or market is selected, or a market or a road is armed, every island tile
// whose owner is not that city is drawn at about 5/8 brightness, the sea tiles inside an island's
// rectangle included; the open sea never (GOG capture and HE frames, ui.md 12.1-12.2). This module
// only decides the lit city and the dark tiles; drawing them is the render side's (UI-RANGE-1 B2).
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <anno/app/territory_view.hpp>
#include <anno/sim/ids.hpp>

namespace anno::app {

/// What starts the darkening, as data (the original's values as defaults). PRODTYPE 33-35 is the
/// group City::Place claims a territory disc for (HAUPT, MARKT, KONTOR; BUILDINGSOBJECTS.md part D
/// P2). Seen in GOG: the selected Kontor, the armed market, the armed dirt road (ui.md 12.2).
/// Whether every armed building darkens is open (OQ-797): until then only the evidenced cases are
/// listed.
struct RangeTriggers {
    std::vector<std::uint32_t> selected_prodtypes = {33, 34, 35};
    std::vector<std::uint32_t> armed_prodtypes = {33, 34, 35};
    bool armed_road = true;
};

/// The UI state the decision reads: the selected building (its PRODTYPE and a tile of it), the
/// armed tool (its PRODTYPE, or a land road) and the tile under the pointer.
struct RangeInput {
    std::optional<std::uint32_t> selected_prodtype;
    std::optional<sim::TileXY> selected_tile;
    std::optional<std::uint32_t> armed_prodtype;
    bool armed_road = false;
    std::optional<sim::TileXY> pointer_tile;
};

/// The lit city: an island (TerritoryIsland::index) and a tile owner value on it.
struct RangeCity {
    std::uint32_t island = 0;
    std::uint32_t owner = 0;
    friend bool operator==(const RangeCity&, const RangeCity&) = default;
};

/// The owner of world tile `t` and the island it lies on; nullopt off every island rectangle.
[[nodiscard]] std::optional<RangeCity> range_owner_at(const TerritoryView& view, sim::TileXY t);

/// The city whose range is shown, or nullopt (nothing darkens). An armed tool wins over a
/// selection (arming a tool is what the player does next; L). The city is the owner of the
/// selected building's tile, or of the tile under the pointer for an armed tool; a neutral tile
/// gives nullopt (L, OQ-797).
[[nodiscard]] std::optional<RangeCity> range_city(const RangeInput& in, const RangeTriggers& trig,
                                                  const TerritoryView& view);

/// Whether tile `i` (row-major) of `view.islands[island_slot]` is drawn dark while `city` is lit:
/// every tile of another island, and every tile of the lit island not owned by the city.
[[nodiscard]] bool range_dark(const TerritoryView& view, std::size_t island_slot, std::size_t i,
                              const RangeCity& city);

/// The number of dark tiles over all islands (the dev fact ui.range_dark_tiles).
[[nodiscard]] std::size_t range_dark_count(const TerritoryView& view, const RangeCity& city);

} // namespace anno::app
