// UI-STATUS-1 stage B: what the bottom status bar shows for the map tile under the cursor, as the
// original decides it (docs/re/ui.md 5.6; the owner chose the original on all three points of
// OQ-703). A pure decision over the published TerritoryView and the cursor tile, so every rule is
// unit-tested without a window (tests/status_bar_test.cpp). Viewer UI state only: no sim state, no
// command, no sim time (the fertility row's hide delay runs on the window's clock).
//
// - The counters (AnnoFrame tStadtInfo 0x10009920) follow the city whose TERRITORY holds the tile
//   (the tile's owner value, island slot +0xec): the local player's city shows its four building
//   materials and its residents; another player's city shows only its residents, and only when
//   that city knows the local player (City::player_mask, the original's city property 0); no city
//   shows gold only. On island land outside every territory an armed building's four corner tiles
//   pick the first city of the local player among them (0x100099a5..0x10009b23).
// - The fertility row (0x10009770) does not depend on any city: it shows over the land of every
//   island the local player has discovered, with the climate's segments; it hides at once over
//   open sea or an undiscovered island, and after a delay over sea inside an island.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/territory_view.hpp>
#include <anno/sim/ids.hpp>

namespace anno::app {

/// The original's numbers for the fertility row, as data with the original values as defaults
/// (CLAUDE.md "No hardcoded limits").
struct StatusBarRule {
    /// Island property 11's bits that are always set (AnnoScene 0x100085a2: 0xF009 | the raw
    /// marks) and so never name a crop; AnnoFrame 0x100098ba skips them.
    std::uint32_t always_set_bits = 0xf009;
    /// The segment AnnoFrame 0x100098c1 skips besides those (`cmp ecx, 7`).
    std::uint32_t skipped_segment = 7;
    /// The segments `STATUSBAR_ROSHTOFFE` has (Data/statusbar.dat `Segments: 12, 1`; the loop
    /// bound `cmp ecx, 0xc` at 0x100098e1).
    std::uint32_t segment_count = 12;
    /// The slots the bar has (hud.txt `rosh_<k>`; the original makes 4, 0x10063a17..0x10063a61).
    std::size_t slots = 4;
    /// The delay before the row hides over sea inside an island's rectangle or over a discovered
    /// island whose climate names no crop (count 0: 0x10063e5d, now + 2000 ms).
    std::uint64_t hide_delay_ms = 2000;
};

/// The segments the row shows for climate `climate`: for i = 0 .. segment_count - 1 ascending,
/// while fewer than `slots` are taken, every i whose bit is set in climate_mask[climate], not in
/// always_set_bits and not skipped_segment (AnnoFrame 0x10009870..0x100098e4). A climate past the
/// table names none (the jump table's default, 0x10009875).
[[nodiscard]] std::vector<std::uint32_t>
fertility_segments(std::uint32_t climate, const std::vector<std::uint32_t>& climate_mask,
                   const StatusBarRule& rule = {});

/// Which groups the counters show.
enum class StatusCounters : std::uint8_t {
    /// Gold only.
    Gold,
    /// Gold and the residents (another player's city that knows the local player).
    Residents,
    /// Gold, the four building materials and the residents (a city of the local player).
    MaterialsResidents,
};

/// What the fertility row does this frame.
enum class FertilityAction : std::uint8_t {
    /// Show `segments` (at least one).
    Show,
    /// Hide now: open sea, or an island the local player has not discovered (count 0xff,
    /// 0x100097c9).
    HideNow,
    /// Hide after StatusBarRule::hide_delay_ms unless a Show comes first: sea inside a discovered
    /// island's rectangle, or a climate with no segment (count 0, 0x10063e40).
    HideLater,
};

struct StatusPick {
    /// The tile the pick was made for (nullopt: no map tile yet).
    std::optional<sim::TileXY> tile;
    /// The island whose rectangle holds the tile (its sim island slot), nullopt over open sea.
    std::optional<std::uint32_t> island;
    /// The city whose territory holds the tile (nullopt: none).
    std::optional<sim::CityRef> city;
    StatusCounters counters = StatusCounters::Gold;
    /// The city whose figures the counters show (nullopt with Gold): `city`, or the armed
    /// building's corner city.
    std::optional<sim::CityRef> counters_city;
    FertilityAction fertility = FertilityAction::HideNow;
    /// The segments with Show.
    std::vector<std::uint32_t> segments;
};

/// The pick for `tile` (nullopt: no map tile yet, which shows gold only and hides the row).
/// `armed` is the armed building's footprint (x0, y0, w, h in world tiles) while one is armed.
/// The island is the first of `view.islands` whose rectangle holds the tile: the rebuild's
/// islands do not overlap, and the original's 8 x 8 chunk map (world slot +0x8c) names the same
/// island wherever an island's origin and size are multiples of 8 (every SCP template); a tile
/// in a chunk an island only partly covers counts as sea here.
[[nodiscard]] StatusPick pick_status(const TerritoryView& view, std::optional<sim::TileXY> tile,
                                     std::uint32_t local_player,
                                     std::optional<sim::TileRect> armed = std::nullopt,
                                     const StatusBarRule& rule = {});

/// The fertility row's state across frames: the segments shown and the pending hide (the bar's
/// timer +0xa8, Tick 0x10064060). `now_ms` is the window's clock, never sim time.
class FertilityRow {
  public:
    /// Applies this frame's pick: Show replaces the segments and cancels a pending hide (0x10063e96
    /// clears the timer); HideNow clears them; HideLater starts the delay unless one is pending
    /// (0x10063e51) and hides once it has passed. A HideLater for a row already hidden starts no
    /// delay (the original asks again only when the tile changes, 0x10009803).
    void update(const StatusPick& pick, std::uint64_t now_ms, const StatusBarRule& rule = {});
    /// The segments shown, slot by slot (empty: the row is hidden).
    [[nodiscard]] const std::vector<std::uint32_t>& shown() const { return shown_; }
    /// The pending hide's time, if one is pending.
    [[nodiscard]] std::optional<std::uint64_t> hide_at() const { return hide_at_; }

  private:
    std::vector<std::uint32_t> shown_;
    std::optional<std::uint64_t> hide_at_;
};

/// The facts' texts (dev_script.hpp): `hud.counters` ("gold", "residents",
/// "materials+residents"), `hud.fertility` ("6,8,11" or "hidden"), a tile ("x,y" or "none"), an
/// island ("<slot>", "sea", or "none" without a tile) and a city ("<island>:<city>" or "none").
[[nodiscard]] std::string status_counters_name(StatusCounters c);
[[nodiscard]] std::string fertility_text(const std::vector<std::uint32_t>& shown);
[[nodiscard]] std::string status_tile_text(std::optional<sim::TileXY> tile);
[[nodiscard]] std::string status_island_text(const StatusPick& pick);
[[nodiscard]] std::string status_city_text(std::optional<sim::CityRef> city);

} // namespace anno::app
