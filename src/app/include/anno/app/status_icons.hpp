// The status icons over production buildings (UI-HARVEST-ICON-1 stage B; docs/re/ui.md §11): the
// spinning "Zzz", "raw material missing" and "storage full" figures the original puts over a
// building, read from the sim's own state and drawn as live units over the scene.
//
// Faithful parts (docs/re/ui.md §11.2-11.3): the figure per status code (AnnoWelt.dll table
// 0x100c92b0: 1555, 1570, 1572, 1571), one icon at a time for the lowest set bit among 0-3 (Zzz
// first), the footprint centre, icons only while the owner's Volk is 1 (the gate 0x1007302f), Zzz
// exactly while every line is switched off (switch helper 0x10072a60; two-line buildings pass the
// combined state, 0x100690b0 / 0x1006910c).
//
// Adaptations, labelled (docs/open-questions.md):
// - OQ-721: the icon's anchor stands on the top of the building's own picture (the original lifts
//   it by a type float +0x8 and the building's byte +0x0f, not yet read).
// - OQ-722: frame = (ms / 40) mod 16 on the window's animation clock, never restarted (the
//   original's divisor ANIM_ms / 2 with the tick read as a millisecond; whether a re-created icon
//   restarts its turn is open).
// - OQ-723, RESOLVED by the owner 2026-11-17 ("Own only"): the local player's buildings only, no
//   rules option (the original's draw path was not read for a viewer filter).
//
// KNOWN DIVERGENCE (docs/re/ui.md §11.4, OQ-720): the original rewrites a due "raw material
// missing" (non-crop, non-animal input) or "storage full" (any input) to code 3 and shows the
// "no road" figure 1571 when 0x10071f40 / 0x10072390 return 0. The sim does not model code 3, so
// the rebuild shows icon 1 or 2 there. Figure 1571 is loaded and mapped for when it does.
//
// No sim state is written: collect_status_icons is a read-only pass over the World on the sim
// thread (the snapshot's), the layer and the placement are window-side.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

/// The status code an icon stands for (the original's +0x0d bit index; docs/re/ui.md §11.1).
enum class StatusIcon : std::uint8_t {
    Off = 0,          ///< "Zzz": production switched off (figure 1555)
    MissingInput = 1, ///< KeinRohstoff (1570)
    StorageFull = 2,  ///< KeinLager (1572)
    NoRoad = 3,       ///< KeineStrasse (1571); the sim never reports it yet (OQ-720)
};

/// The FigurenHeader.dat entry of each status code (D: AnnoWelt.dll table 0x100c92b0, read by
/// 0x10073114). Data of the original; a mod may replace the figures, not the codes.
inline constexpr std::array<std::uint16_t, 4> kStatusIconFigures = {1555, 1570, 1572, 1571};

/// The original's frame period divisor: ANIM_ms / 2 with ANIM_ms = 80 for all four figures (P:
/// FigurenHeader.dat ANIM; divisor D: docs/re/annowelt.md "Drawing"). The unit is OQ-722.
inline constexpr std::uint64_t kStatusIconFrameMs = 40;

/// The icon a building shows, or nullopt: the lowest of {Zzz, icon bit 1, icon bit 2, icon bit
/// 3} that holds (0x100730d3-0x100730e4), all only when `icons_allowed` (the owner's Volk is 1).
/// Zzz holds when the payload has switches and every one is off; the sim's own icon bit 0 is not
/// read (the sim never sets it). Inert, houses, markets, inns and quarries have no icon.
[[nodiscard]] std::optional<StatusIcon> status_icon_of(const sim::Payload& payload,
                                                       bool icons_allowed);

/// One building with an icon, as the snapshot carries it: its id, type, north-west tile (world
/// tiles), footprint at its rotation, rotation, the height byte the scene draws it at
/// (Building::byte0f, SceneBuilding::height) and the icon.
struct StatusIconView {
    sim::BuildingId id;
    std::uint32_t type = 0;
    std::int32_t x = 0, y = 0;
    std::uint32_t w = 1, h = 1;
    std::uint8_t rotation = 0;
    std::uint8_t height = 0;
    StatusIcon icon = StatusIcon::Off;
    friend bool operator==(const StatusIconView&, const StatusIconView&) = default;
};

/// The icons of every building in the cities `viewer` owns (OQ-723, the owner's "Own only"), in
/// (island, city, list, slot) order. Read-only; O(the viewer's buildings).
[[nodiscard]] std::vector<StatusIconView> collect_status_icons(const sim::World& world,
                                                               sim::PlayerId viewer);

/// An icon placed in a scene: the footprint centre (world tiles, what UnitSprite::anchor projects)
/// and, per built zoom group, how many scene pixels the icon's anchor stands above the point a
/// unit without z is anchored at (the sea level): up to the top of the building's own picture
/// there (OQ-721 default), or to the ground at its height byte when the scene has no picture of
/// the type.
struct PlacedStatusIcon {
    float x = 0, y = 0;
    StatusIcon icon = StatusIcon::Off;
    std::array<int, render::kGroups> raise{};
};

/// Places `icons` in `scene`. Icons whose tile no island of the scene holds are left out.
[[nodiscard]] std::vector<PlacedStatusIcon>
place_status_icons(std::span<const StatusIconView> icons, const render::Scene& scene);

/// The frame of a 16-frame turn at `time_ms` (OQ-722 default): (time_ms / 40) mod frames.
[[nodiscard]] std::size_t status_icon_frame(std::uint64_t time_ms, std::size_t frames);

/// The four figures' frames, per animation frame and zoom group, decoded once at load and
/// immutable afterwards (shared by the window thread and the tests without a lock).
class StatusIconLayer {
  public:
    /// Reads the figures of kStatusIconFigures from `game_dir`. An error when one is missing.
    [[nodiscard]] static formats::Result<std::shared_ptr<const StatusIconLayer>, std::string>
    load(const std::filesystem::path& game_dir);

    /// A layer from sprites the caller made (tests): per icon code, one UnitSprite per animation
    /// frame (frames per zoom group; x, y are overwritten).
    [[nodiscard]] static std::shared_ptr<const StatusIconLayer>
    from_prepared(std::map<StatusIcon, std::vector<render::UnitSprite>> sets);

    /// The units of `placed` at `time_ms`: each icon's current frame at its footprint centre, its
    /// anchor raised by PlacedStatusIcon::raise. Icons whose code the layer lacks are skipped.
    [[nodiscard]] std::vector<render::UnitSprite> units(std::span<const PlacedStatusIcon> placed,
                                                        std::uint64_t time_ms) const;

    [[nodiscard]] std::size_t sets() const { return sets_.size(); }

  private:
    std::map<StatusIcon, std::vector<render::UnitSprite>> sets_;
};

} // namespace anno::app
