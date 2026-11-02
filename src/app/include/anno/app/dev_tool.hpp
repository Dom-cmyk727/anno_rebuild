// The developer building tool in the play window (W4.4 round ee, package B): drawable-pixel
// pointer events and tool keys in, sim commands out. It picks the tile under the pointer from the
// frame that was drawn (its screen map, zoom geometry and owning scene handle), keeps the pick on
// the developer city's island, and drives DevInput, which makes the commands. It never reads the
// World: the type registry, the buildable types and the city's island come from DevToolSetup,
// taken at load. The window's SDL mapping (sdl_input.hpp) and the tests feed it the same
// ToolInput. A test tool (the owner's round-dr decision), not UI.2: UI.2 can reuse the pick, the
// commands and the feedback.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/dev_input.hpp>
#include <anno/render/picking.hpp>
#include <anno/render/resample.hpp>
#include <anno/render/scene.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::app {

/// A world-tile rectangle (an island's tiles).
struct TileRect {
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
    [[nodiscard]] bool contains(sim::TileXY t) const {
        return static_cast<std::int64_t>(t.x) >= x0 && static_cast<std::int64_t>(t.y) >= y0 &&
               static_cast<std::int64_t>(t.x) < static_cast<std::int64_t>(x0) + w &&
               static_cast<std::int64_t>(t.y) < static_cast<std::int64_t>(y0) + h;
    }
};

/// A spot where another player's city could build, for the scripted session's `foreign` step
/// (dev_script.hpp): a stand-in for a command of another player arriving through the runner's
/// queue like any other, so the window sees an edit on an island that is not the tool's. Not used
/// by the tool or by play without a script.
struct ForeignSpot {
    sim::CityRef city;
    sim::PlayerId owner;
    /// A free tile of that city's island, and a type to build there.
    sim::TileXY at;
    sim::TypeId type;
};

/// What the tool needs from the world, read once at load (PlayLoad, before the runner owns it).
struct DevToolSetup {
    std::shared_ptr<const sim::TypeRegistry> registry;
    /// The registry's types the tool cycles: those the loader places as buildings, in TypeId
    /// order (dev_tool_setup). Empty: the tool can only demolish.
    std::vector<sim::TypeId> types;
    /// The developer player's city and its island's tiles; no city: the tool stays off.
    std::optional<sim::CityRef> city;
    TileRect island;
    sim::PlayerId issuer;
    /// Another player's city on another island (none when the world has no second city).
    std::optional<ForeignSpot> foreign;
};

/// One drawn frame, as the pointer needs it: how drawable pixels map to its scene pixels, the
/// tile geometry of its zoom level, its owning scene handle (heights) and its drawable size.
struct PointerFrame {
    render::ScreenMap map;
    render::PickGeometry geometry;
    /// The zoom level drawn (a built group, or a derived level after them).
    std::size_t group = 0;
    std::shared_ptr<const render::Scene> scene;
    int drawable_w = 0, drawable_h = 0;
};

/// The pointer frame of a frame drawn at `view` of a zoom level with `tile_w` x `tile_h` scene
/// pixels per diamond: a stepped zoom (`resample` null) shows scene pixel view.x + px at px; a
/// continuous zoom resamples (render::resampled_view: the view's origin plus the Resample's
/// offset, at its scale). `scene` is the frame's own generation.
[[nodiscard]] PointerFrame pointer_frame(render::Rect view, const render::Resample* resample,
                                         std::size_t group, double tile_w, double tile_h,
                                         std::shared_ptr<const render::Scene> scene, int drawable_w,
                                         int drawable_h);

/// A tool input in drawable pixels (the window converts points with its pixel density).
struct ToolInput {
    enum class Kind : std::uint8_t {
        Toggle,
        NextType,
        PrevType,
        Rotate,
        ToggleDemolish,
        /// Esc: drop a press, leave the demolish mode.
        Cancel,
        /// Focus lost, the pointer left the window: drop a press only.
        Abort,
        Press,
        Move,
        Release,
    };
    Kind kind = Kind::Move;
    double px = 0, py = 0;
};

class DevTool {
  public:
    explicit DevTool(DevToolSetup setup);

    /// The commands this input produces (none, or one: a release). Pointer inputs need the frame
    /// that was drawn; without one (none drawn yet) a press or release does nothing.
    [[nodiscard]] std::vector<sim::Command> handle(const ToolInput& input,
                                                   const PointerFrame* frame);
    /// The world tile under drawable pixel (px, py) of `frame`, or nullopt with the reason in
    /// `why`: outside the drawable, no tile (render::pick_tile), or not the city's island.
    [[nodiscard]] std::optional<sim::TileXY> pick(const PointerFrame& frame, double px, double py,
                                                  std::string* why = nullptr) const;
    [[nodiscard]] bool enabled() const { return enabled_; }
    /// Something Esc would cancel: a press in progress or the demolish mode.
    [[nodiscard]] bool has_pending() const {
        return enabled_ && (input_.pressed() || input_.demolish_mode());
    }
    /// The tile under the pointer at the last pointer input (nullopt off the city's island).
    [[nodiscard]] std::optional<sim::TileXY> hover() const { return hover_; }
    /// Where the press in progress started.
    [[nodiscard]] std::optional<sim::TileXY> press() const { return press_; }
    /// The last hint or local refusal ("" when none), DevInput's included.
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] const DevInput& input() const { return input_; }
    [[nodiscard]] const DevToolSetup& setup() const { return setup_; }

  private:
    void forward(DevEvent::Kind kind, sim::TileXY tile, std::vector<sim::Command>& out);

    DevToolSetup setup_;
    DevInput input_;
    bool enabled_ = false;
    std::optional<sim::TileXY> hover_, press_;
    std::string message_;
};

/// The rounding of island `index`'s placement at zoom level `group` of `scene`, in scene pixels:
/// the scene draws an island at the integer iso::tile_top of its world origin, so at group 1
/// (tile height 21) an origin with an odd x + y sits half a pixel above the continuous rule the
/// pick solves (round ee review of B). A derived level is a reduction of group 2's picture: its
/// rounding is group 2's at its scale. (0, 0) when the scene has no placement for the island.
[[nodiscard]] std::pair<double, double>
placement_rounding(const render::Scene& scene, std::size_t index, std::size_t group, double tile_w);

/// One line for the window title: the selected type (its id and footprint), the rotation, the
/// demolish mode, the tile under the pointer and the last hint.
[[nodiscard]] std::string dev_tool_status(const DevTool& tool);

} // namespace anno::app
