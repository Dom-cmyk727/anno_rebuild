// The developer picker (W4.4 B3): a minimal test tool, not the final UI. It turns
// abstract events (cycle the type, rotate, press, move, release, demolish mode, cancel)
// into sim commands. It reads only immutable data (the type registry) and the city it
// was given; it never reads the World. The viewer maps SDL events to DevEvent; the
// tests drive DevInput with synthetic events. The sim decides every command.
//
// No key or mouse binding is wired in this change: the viewer's SDL mapping (keys,
// the pointer, the pick through render::pick_tile) is not in it, so nothing in the
// window produces these events yet. The events are the contract that mapping feeds.
// The owner's round-dr decision keeps this a test tool; the original's controls are
// for UI.1/UI.2 (docs/app/game-shell.md).
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
        Rotate,
        ToggleDemolish,
        Cancel,
        Press,
        Move,
        Release,
    };
    Kind kind = Kind::Cancel;
    sim::TileXY tile;
};

class DevInput {
  public:
    /// `types` are the buildable types the picker cycles (data, from the registry).
    /// `city` is the developer player's city; `issuer` stamps the commands.
    DevInput(std::shared_ptr<const sim::TypeRegistry> registry, std::vector<sim::TypeId> types,
             sim::CityRef city, sim::PlayerId issuer);

    /// The commands this event produces, in order (none, or one). A drag that cannot
    /// be sent (over the wire limit, or a non-road type dragged) sets message().
    [[nodiscard]] std::vector<sim::Command> handle(const DevEvent& event);

    [[nodiscard]] std::optional<sim::TypeId> selected() const;
    [[nodiscard]] const sim::BuildingType* selected_type() const;
    [[nodiscard]] std::uint8_t rotation() const { return rotation_; }
    [[nodiscard]] bool demolish_mode() const { return demolish_; }
    /// The last refusal or hint for the player ("" when none); cleared by the next event.
    [[nodiscard]] const std::string& message() const { return message_; }
    /// The tiles the selected type covers at `origin` with the current rotation (the
    /// preview's footprint; the sim's verdict comes back as CommandRejected).
    [[nodiscard]] std::vector<sim::TileXY> footprint_at(sim::TileXY origin) const;
    /// The tiles a drag from `from` to `to` covers: a straight line along the longer
    /// axis (ties go to x), from `from` to `to` inclusive, in stream order.
    [[nodiscard]] static std::vector<sim::TileXY> line(sim::TileXY from, sim::TileXY to);

  private:
    [[nodiscard]] bool road_like() const;
    [[nodiscard]] sim::Command build_command(const std::vector<sim::TileXY>& tiles) const;
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
