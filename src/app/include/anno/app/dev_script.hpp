// A scripted developer-tool session in the live play window (W4.4 round ee, E3): a text script of
// steps that the window turns into real SDL events (pushed into SDL's own queue, so they reach the
// window's handler exactly as a person's input does), waits on what the window shows, and checks
// on the title and the tool's latest outcome. A test driver (ANNO_DEV_SCRIPT, main.cpp), not UI.2.
//
// One step per line; '#' starts a comment. World tiles are absolute (the commands' coordinates).
//   look X Y              centre the camera on world tile (X, Y) (setup, not input)
//   key b|n|p|t|x|esc     press and release a key
//   select ID             press N until the tool's type is ID (fails after one cycle)
//   rotate R              press T until the rotation is R
//   click X Y             move to, press and release over the drawn centre of tile (X, Y)
//   drag X0 Y0 X1 Y1      press over (X0, Y0), move in steps to (X1, Y1), release
//   pan DX DY             a right-button drag by (DX, DY) window points from the window centre
//   wheel N               N wheel notches at the window centre (positive: closer)
//   wait frames N | wait turns N | wait outcome | wait drawn | wait ms N
//   expect title TEXT     the window title contains TEXT
//   expect outcome STATE [REASON]   the tool's latest command is STATE (Queued, Applied, Refused,
//                         Lost), with the sim's REASON (reject_name) when given; `none`: the
//                         latest click sent no command (the window shows no receipt)
//   checkpoint NAME       log the turn, the title and the latest outcome
//   screenshot NAME       the window's own rectangle as NAME.png in the session directory
//                         (macOS screencapture -R; elsewhere logged as not run)
//   quit
// A step that cannot run (a tile not drawn, an expectation not met, a wait past its limit) fails
// the script: the log names it and the window closes.
#pragma once

#include <SDL3/SDL_events.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/app/command_outcome.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/result.hpp>

namespace anno::app {

/// The device id the script's key and mouse events carry (`which`): while a script runs the window
/// takes only input with it, so a person's mouse or keyboard over the window cannot change the
/// session (round ee, E3: a first live run picked up the desktop's pointer).
inline constexpr std::uint32_t kScriptDevice = 0x7E5C1Du;

/// Whether `e` is keyboard, mouse or wheel input that did not come from the script.
[[nodiscard]] bool foreign_input(const SDL_Event& e);

/// What the window shows at a loop iteration, for the script.
struct DevScriptState {
    /// Frames drawn so far.
    std::uint64_t frames = 0;
    double now_ms = 0;
    std::shared_ptr<const PlaySnapshot> snapshot;
    std::optional<CommandOutcome> latest;
    std::optional<std::uint64_t> followed_turn;
    std::size_t pending = 0;
    const DevTool* tool = nullptr;
    /// The last drawn frame (none before the first).
    const PointerFrame* frame = nullptr;
    float density = 1.0F;
    /// The window's size in window points.
    int window_w = 0, window_h = 0;
    std::string title;
};

/// What one call of DevScript::step asks of the window.
struct DevScriptAction {
    std::vector<SDL_Event> events;
    /// Centre the camera on this world tile.
    std::optional<sim::TileXY> look;
    /// Capture the window as this name (DevScript's screenshot step).
    std::optional<std::string> screenshot;
    bool quit = false;
};

class DevScript {
  public:
    /// Parses `text`; an error names the line.
    [[nodiscard]] static formats::Result<DevScript, std::string> parse(std::string_view text);

    /// The events and requests of the steps that are due in `state`, in order; a wait that is not
    /// over yet ends the call. The window pushes the events, then calls again on a later
    /// iteration (after they were handled). Appends to log().
    [[nodiscard]] DevScriptAction step(const DevScriptState& state);

    /// How long a wait (or the first frame) may take before the script fails: a stalled session
    /// must end, not hang.
    static constexpr double kWaitLimitMs = 30000.0;

    [[nodiscard]] bool done() const { return next_ >= steps_.size() || failed_; }
    /// Fails the script for a reason outside its steps (the window's).
    void abort(const std::string& why) {
        failed_ = true;
        log_.push_back("FAIL: " + why);
    }
    [[nodiscard]] bool failed() const { return failed_; }
    [[nodiscard]] const std::vector<std::string>& log() const { return log_; }
    /// The window's own lines (a screenshot's result) in the log.
    void note(std::string line) { log_.push_back(std::move(line)); }

    /// The window point over the drawn centre of world tile `tile` in `frame` (the forward rule
    /// of the pick: render::corner_point at the ground height there plus the island placement's
    /// rounding, then the frame's screen map), or nullopt when it is not on the drawable.
    [[nodiscard]] static std::optional<std::pair<float, float>>
    window_point(const PointerFrame& frame, const DevTool& tool, sim::TileXY tile, float density);
    /// The scene point (at `frame`'s zoom level) of the drawn centre of world tile `tile` of the
    /// tool's city island; nullopt without the island in the frame's scene.
    [[nodiscard]] static std::optional<std::pair<double, double>>
    scene_point(const PointerFrame& frame, const DevTool& tool, sim::TileXY tile);

    struct Step {
        std::string verb;
        std::vector<std::string> args;
        std::size_t line = 0;
    };

  private:
    void fail(const Step& s, const std::string& why);

    std::vector<Step> steps_;
    std::size_t next_ = 0;
    bool failed_ = false;
    /// The state a wait started from, and how many key presses a select or rotate has made.
    std::optional<std::uint64_t> wait_frames_, wait_turn_;
    std::optional<double> wait_ms_;
    std::size_t presses_ = 0;
    std::vector<std::string> log_;
};

} // namespace anno::app
