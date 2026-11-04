// The developer picker (W4.4 B3): a minimal test tool, not the final UI. It turns
// abstract events (cycle the type, rotate, press, move, release, demolish mode, cancel)
// into sim commands. It reads only immutable data (the type registry) and the city it
// was given; it never reads the World. The viewer maps SDL events to DevEvent; the
// tests drive DevInput with synthetic events. The sim decides every command.
//
// The window feeds it through DevTool (dev_tool.hpp: the pointer picked to a tile), which the
// viewer's SDL mapping (sdl_input.hpp) drives (round ee, B3). The owner's round-dr decision keeps
// this a test tool; the original's controls are for UI.1/UI.2 (docs/app/game-shell.md).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/command.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::app {

/// One input event in world tiles (the viewer converts the pointer with
/// render::pick_tile before it gets here).
struct DevEvent {
    enum class Kind : std::uint8_t {
        NextType,
        PrevType,
        /// The turn on (+1: `,` and T) and back (-1: `.`), as the original's keys (D: View.dll
        /// 0x100190b3 adds -1 or +1 to the active cursor's byte +0x46, `& 3`; Wine runner
        /// 2026-11-04: `.` back, `,` on).
        Rotate,
        RotateBack,
        ToggleDemolish,
        Cancel,
        /// Drops a press in progress only (focus lost, a release off the island): unlike Cancel
        /// it keeps the demolish mode.
        Abort,
        Press,
        Move,
        Release,
    };
    Kind kind = Kind::Cancel;
    sim::TileXY tile;
};

/// Whether `type` takes its turn from the coast: the original's ordinary cursor tries the turns
/// r, r+1, r+2, r+3 from the cursor's for PRODTYPE 6 FISCHER, 7 WERFT, 15 WALFAENGER and 35 KONTOR
/// (D: AnnoFrame.dll 0x100473cf-0x1004744c) and places the first the coast accepts; the cursor's
/// own turn stays. The PRODTYPEs are the original's data (a mod adding a coast kind adds it here).
[[nodiscard]] bool coast_turned(const sim::BuildingType& type);
/// Whether `type` takes its turn from a deposit: PRODTYPE 3 MINE and 13 STEINBRUCH get the mine
/// cursor (D: View.dll 0x1002b199-0x1002b1cd), which snaps to the nearest deposit record of the
/// island and builds with that record's turn (D: AnnoFrame.dll 0x10046300, 0x10046715); the
/// player's turn only centres the search. The deposit records are not ported (OQ-024), so the
/// tool keeps the turn where it is for these types and says why.
[[nodiscard]] bool deposit_turned(const sim::BuildingType& type);

class DevInput {
  public:
    /// `types` are the buildable types the picker cycles (data, from the registry).
    /// `city` is the developer player's city; `issuer` stamps the commands.
    DevInput(std::shared_ptr<const sim::TypeRegistry> registry, std::vector<sim::TypeId> types,
             sim::CityRef city, sim::PlayerId issuer);

    /// The commands this event produces, in order (none, or one). A drag that cannot
    /// be sent (over the wire limit, or a non-road type dragged) sets message(). `turn`: on a
    /// release of one tile, the turn to build with when the tool chose it by the coast trials
    /// (DevTool::turn); none: the cursor's turn.
    [[nodiscard]] std::vector<sim::Command> handle(const DevEvent& event,
                                                   std::optional<std::uint8_t> turn = std::nullopt);

    [[nodiscard]] std::optional<sim::TypeId> selected() const;
    [[nodiscard]] const sim::BuildingType* selected_type() const;
    [[nodiscard]] std::uint8_t rotation() const { return rotation_; }
    [[nodiscard]] bool demolish_mode() const { return demolish_; }
    /// The last refusal or hint for the player ("" when none); cleared by the next event.
    [[nodiscard]] const std::string& message() const { return message_; }
    /// The rectangle the selected type covers at `origin` with the current rotation, in 64-bit
    /// tile coordinates [x0, x0 + w) x [y0, y0 + h): made without enumerating a tile, so any
    /// registry dimension (nonzero 32-bit today) is safe to ask about (round ee review, R3).
    /// nullopt: no selected type. A zero w or h is a definition without a footprint.
    struct Extent {
        std::int64_t x0 = 0, y0 = 0;
        std::uint32_t w = 0, h = 0;
        [[nodiscard]] std::uint64_t area() const { return std::uint64_t{w} * h; }
    };
    /// `hover` is the tile under the pointer: the footprint at the cursor's turn is centred on it,
    /// clamped at 0 per axis (sim::site::centred_origin; D: AnnoFrame.dll 0x10043e20, 0x10049340),
    /// and `turn` (default the cursor's) gives its size; a coast trial keeps that origin (D:
    /// 0x10048de7, the origin is not recomputed).
    [[nodiscard]] std::optional<Extent> footprint_extent(sim::TileXY hover) const;
    [[nodiscard]] std::optional<Extent> footprint_extent(sim::TileXY hover,
                                                         std::uint8_t turn) const;
    /// The north-west tile a placement at `hover` gets (the centred origin above); `hover` itself
    /// when no type is selected.
    [[nodiscard]] sim::TileXY origin(sim::TileXY hover) const;
    /// The tiles the selected type covers at `hover` (centred) with the current rotation, row by
    /// row.
    /// Empty when the rectangle has more than `max_tiles` tiles or leaves the 32-bit tile
    /// coordinates: the caller names the region it admits (the city's island), so the allocation
    /// is bounded by it and not by a definition's dimensions. The preview and the feedback do not
    /// call it; they use footprint_extent (the sim's verdict comes back as CommandRejected).
    [[nodiscard]] std::vector<sim::TileXY> footprint_at(sim::TileXY origin,
                                                        std::uint64_t max_tiles) const;
    /// The tiles a drag from `from` to `to` covers: a straight line along the longer
    /// axis (ties go to x), from `from` to `to` inclusive, in stream order.
    [[nodiscard]] static std::vector<sim::TileXY> line(sim::TileXY from, sim::TileXY to);
    /// The number of tiles line(from, to) covers, computed without making it (64-bit: the tiles
    /// of a pick are within +-2^30). A drag is refused before its path is made when it covers
    /// more tiles than one record's payload has bytes (sim::kMaxRecordPayload, at least one byte
    /// a tile); sim::has_wire_form decides the rest (4 095 placements, 8 190 tiles today). It is
    /// never split: one drag is one command.
    [[nodiscard]] static std::uint64_t line_length(sim::TileXY from, sim::TileXY to);
    /// Whether a press is in progress (a release would send a command).
    [[nodiscard]] bool pressed() const { return press_.has_value(); }

  private:
    [[nodiscard]] bool road_like() const;
    [[nodiscard]] sim::Command build_command(const std::vector<sim::TileXY>& tiles,
                                             std::optional<std::uint8_t> turn) const;
    [[nodiscard]] sim::Command demolish_command(const std::vector<sim::TileXY>& tiles) const;

    std::shared_ptr<const sim::TypeRegistry> registry_;
    std::vector<sim::TypeId> types_;
    sim::CityRef city_;
    sim::PlayerId issuer_;
    std::size_t index_ = 0;
    std::uint8_t rotation_ = 0;
    bool demolish_ = false;
    std::optional<sim::TileXY> press_;
    std::optional<sim::TileXY> hover_;
    std::string message_;
};

} // namespace anno::app
