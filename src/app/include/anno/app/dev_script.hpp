// A scripted developer-tool session in the live play window (W4.4 round ee, E3): a text script of
// steps that the window turns into real SDL events (pushed into SDL's own queue, so they reach the
// window's handler exactly as a person's input does), waits on what the window shows, and checks
// on the title and the tool's latest outcome. A test driver (ANNO_DEV_SCRIPT, main.cpp), not UI.2.
//
// One step per line; '#' starts a comment. World tiles are absolute (the commands' coordinates).
//   look X Y              centre the camera on world tile (X, Y) (setup, not input)
//   key b|n|p|t|x|esc     press and release a key
//   select ID             press N until the tool's type is ID (fails after one cycle); `select
//   paid`:
//                         the cheapest of the tool's types that costs gold (not a road)
//   rotate R              press T until the rotation is R
//   click X Y             move to, press and release over the drawn centre of tile (X, Y)
//   click_px X Y          move to, press and release at the raw drawable pixel (X, Y) (UI.2 C4:
//                         construction_handle's own coordinate space, data/ui/'s 1024 x 768
//                         reference - not a world tile; `click`'s window_point projection does
//                         not apply)
//   drag X0 Y0 X1 Y1      press over (X0, Y0), move in steps to (X1, Y1), release
//   pan DX DY             a right-button drag by (DX, DY) window points from the window centre
//   wheel N               N wheel notches at the window centre (positive: closer)
//   wait frames N | wait turns N | wait outcome | wait drawn | wait held | wait ms N
//                         `wait drawn` ends when an applied command's change is settled on its way
//                         to the screen: presented (Visible), or classified as not shown at all
//                         (Offscreen, NothingToDraw, NoVisualChange). It does not end at the
//                         earlier boundaries (not in the picture, object list not committed,
//                         chunks not fresh, GPU copy not usable, no successful present, out of
//                         view). Use `expect visibility Visible` to require that it was shown.
//   hold chunks | uploads | presents   (GPU window) hold a stage on the way to the screen while the
//                         window keeps drawing and handling input: `chunks`: the chunk builders
//                         wait before each build (ChunkWorkers' test gate), so a chunk an edit
//                         needs stays stale on the CPU; `uploads`: the compositor uploads and
//                         replaces no chunk (GpuFrame::set_test_hold_uploads), so a built chunk
//                         stays off the GPU; `presents`: every present call counts as failed to the
//                         window's consumers (the frame is still presented; the consumer seam is
//                         the one a failing surface takes). `release ...` ends each. `wait blocked`
//                         ends when a builder waits in the gate. `expect held NAME` (NAME as above
//                         or `publication`) requires the hold to be in effect now.
//   fail update | submit | present   (CPU window) make that stage of the CPU frame's delivery
//   really
//                         fail: `update` SDL_UpdateTexture, `submit` SDL_RenderTexture, `present`
//                         SDL_RenderPresent are called with an invalid object (a null texture or
//                         renderer), so SDL itself returns false and the window reads that result
//                         as it would any other. `pass STAGE` ends it; `expect failing STAGE`
//                         requires it.
//   note frames           remember the frames drawn so far; `expect frames at-most N` requires at
//   most N
//                         frames since (a retry that backs off draws about ten a second)
//   resize W H            ask the window for a drawable of W x H points (a new surface epoch)
//   wait visibility NAME  ends when the latest applied command's change is at NAME
//   (visibility_name) hold publication      arm the publication hold (publication_hold.hpp): the
//   sim thread waits at
//                         the next publication that follows a recorded outcome, between the outcome
//                         and the snapshot. `wait held` ends when it waits (fails if the watchdog
//                         ended a wait). The window keeps drawing and handling input meanwhile.
//   release publication   end the wait
//   note money            remember the snapshot's money; `expect money same|less` compares to it
//   note work NAME        remember an occupancy count (NAME as in `expect work`)
//   local build TYPE X Y | local demolish X Y   post a command of the tool's own city through the
//                         runner's queue at this moment (what the command file does at a turn),
//                         whether the tool is on or off; no receipt, no title line
//   foreign build|demolish   post a command of another player's city on another island (the tool's
//                         ForeignSpot) through the runner's queue, as a second player's edit would
//                         arrive; the window sees a building batch of an island that is not the
//                         tool's. It is not the tool's command: no receipt, no title line.
//   The ship-start opening's steps (W4-OPENING-1, an --ship-start session; tile arguments are world
//   tiles, the pointer aims at their drawn centre; all of them are real SDL events):
//   lookw X Y             centre the camera on the sea point of tile (X, Y) (setup, not input)
//   lookship ID          centre the camera on ship ID's tile (the published view; setup, not input)
//   pick-ship ID          left click on the drawn centre of ship ID (the published view's position)
//   sail X Y              right click on the sea surface over tile (X, Y)
//   rdown X Y | rup X Y   the right button pressed / released over the sea tile (a gesture in two
//                         steps: a focus loss or Esc between them cancels it)
//   click-land X Y        left click on the ground of tile (X, Y); hover-land X Y moves the pointer
//                         there without a button
//   lpan DX DY            a left-button drag by (DX, DY) window points from the window centre
//   focus-lost            the window loses focus (SDL_EVENT_WINDOW_FOCUS_LOST)
//   key f|u|h|tab|enter|lbracket|rbracket|f5|t|esc|b|n|p|x   (the opening's keys, and the tool's)
//   wait fact KEY OP VALUE | expect fact KEY OP VALUE   the window's named facts (the opening view,
//                         the controls and the latest command's state): OP is ==, !=, >=, <=, >, <
//                         (integers when both sides are) or `contains`. Facts: cities, gold, mode,
//                         selected, tracked, latest (the controls' last command: queued, refused,
//                         accepted, accepted-planning, sailing, arrived, route-failed, ...),
//                         latest_kind, latest_reason, latest_moved (raw 1/256 ton of an unload),
//                         ship.<id>.x|.y|.dir|.order|.hold<i>, store.<goods> (raw 8.8),
//                         tracked.<n>.state|.kind, tool, tick, generation / committed / discarded /
//                         pending_builds (the picture's work), save. `wait fact KEY OP VALUE within
//                         MS` waits up to MS milliseconds instead of the 30 s limit (a voyage of
//                         hundreds of tiles).
//   note fact KEY         remember the fact's text; `expect fact KEY same` requires it unchanged
//   expect title TEXT     the window title contains TEXT
//   expect title-lacks TEXT   the window title does not contain TEXT
//   expect published      the latest outcome, when it is Applied or Refused, is not newer than the
//                         snapshot the window holds (its sequence number is below the snapshot's
//                         outcomes_next): the window never shows an outcome beside older resources
//   expect money same|less    the snapshot's money equals / is below the noted money
//   expect work NAME OP N     a count of the tool's occupancy index (PlacementKnowledge): NAME is
//                         rebuilds, deltas, tiles, foreign or off_batches; OP is ==, <= or >=
//   expect work NAME same|grew   the count equals / is above the one noted with `note work NAME`
//   expect occupied X Y yes|no   whether the index holds world tile (X, Y) as occupied
//   expect visibility NAME  the latest command's change is at NAME (visibility_name: Visible,
//                         Offscreen, NothingToDraw, NoVisualChange, or a waiting stage)
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
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <anno/app/command_outcome.hpp>
#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/play.hpp>
#include <anno/app/visibility.hpp>
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
    /// Where the latest command's change is on its way to the screen (ChangeVisibility::classify
    /// of its turn; NoVisualChange for a command that was not applied).
    Visibility visibility = Visibility::NoVisualChange;
    const DevTool* tool = nullptr;
    /// The tool's occupancy index (null without the tool), and the publication hold's state: a
    /// hold exists, a publication waits in it, a wait ended by the watchdog.
    const PlacementKnowledge* knowledge = nullptr;
    bool hold_available = false, held = false, hold_expired = false;
    /// The stage holds (hold chunks|uploads|presents): builders waiting in the gate now, whether
    /// the chunk gate is armed, uploads held, presents counted as failed; and whether the window
    /// has a GPU path and a chunk gate at all.
    std::size_t chunks_blocked = 0;
    bool chunks_armed = false, uploads_held = false, presents_failing = false;
    bool stage_holds_available = false;
    /// The CPU delivery seam (fail|pass update|submit|present): available in a scripted CPU window,
    /// and the stages (bit 0 update, 1 submit, 2 present) now failing.
    bool cpu_fail_available = false;
    std::uint8_t cpu_failing = 0;
    /// The scene generation of the last drawn frame and the surface epoch (the count of surface
    /// reconfigurations since the window opened, 0 on the CPU path; the window trace's epoch column
    /// is this plus 1), for the checkpoint line: the same command, scene generation, frame count
    /// and surface are named at every transition.
    std::uint64_t scene_generation = 0, surface_epoch = 0;
    /// The last drawn frame (none before the first).
    const PointerFrame* frame = nullptr;
    float density = 1.0F;
    /// The window's size in window points.
    int window_w = 0, window_h = 0;
    std::string title;
    /// The opening's named facts (see `wait fact`); empty in a session without an opening.
    std::map<std::string, std::string> facts;
    /// The turn `facts` reflects (W4-OPENING-CONTROLS-FLAKE): in an opening session, facts are
    /// built from a snapshot taken once per drawn frame, which can run behind `snapshot->turn`
    /// (read fresh every call) under a slow or contended frame cadence. `wait turns N` baselines
    /// and checks against this turn, not `snapshot->turn`, so it cannot report "N turns later" as
    /// true before a frame exists whose facts actually show it. Equal to `snapshot->turn` in a
    /// session without an opening (facts is then always empty, so this has no effect there).
    std::uint64_t facts_turn = 0;
    /// The window point of ship `id`'s drawn centre in the last frame, if it is drawn.
    std::function<std::optional<std::pair<float, float>>(std::uint64_t)> ship_point;
};

/// What one call of DevScript::step asks of the window.
struct DevScriptAction {
    std::vector<SDL_Event> events;
    /// Centre the camera on this world tile.
    std::optional<sim::TileXY> look;
    /// Centre the camera on this world point (tiles) at sea level (`lookw`).
    std::optional<std::pair<double, double>> look_world;
    /// Capture the window as this name (DevScript's screenshot step).
    std::optional<std::string> screenshot;
    /// Arm or release the publication hold (hold publication, release publication).
    enum class Hold : std::uint8_t { None, Arm, Release };
    Hold hold = Hold::None;
    /// A stage hold to arm or release (hold|release chunks|uploads|presents), and a resize.
    enum class Stage : std::uint8_t {
        None,
        ArmChunks,
        ReleaseChunks,
        ArmUploads,
        ReleaseUploads,
        FailPresents,
        PassPresents
    };
    Stage stage = Stage::None;
    /// Make a CPU delivery stage (bit 0 update, 1 submit, 2 present) report failure / stop doing
    /// so.
    std::uint8_t cpu_fail_set = 0, cpu_fail_clear = 0;
    std::optional<std::pair<int, int>> resize;
    /// Post the foreign build (1) or demolish (2) command (the `foreign` step).
    enum class Foreign : std::uint8_t { None, Build, Demolish };
    Foreign foreign = Foreign::None;
    /// Post a command of the tool's own city (the `local` step): a build of `local_type` or a
    /// demolish, at `local_at`.
    enum class Local : std::uint8_t { None, Build, Demolish };
    Local local = Local::None;
    sim::TypeId local_type;
    sim::TileXY local_at;
    bool quit = false;
};

class DevScript {
  public:
    /// Parses `text`; an error names the line. The first line of log() is the identity of what was
    /// parsed (identity_line): the window's own record of the whole script, every argument of every
    /// step including the waits, for the session record's validator (tools/test/viewer_session.py).
    [[nodiscard]] static formats::Result<DevScript, std::string> parse(std::string_view text);

    /// The identity line's first words.
    static constexpr std::string_view kIdentityPrefix = "script identity:";

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

    /// The identity of a parsed script: `script identity: steps=N fnv1a64=HEX`, the hash over
    /// canonical_text.
    [[nodiscard]] static std::string identity_line(const std::vector<Step>& steps);
    /// One line per step, `LINE VERB ARG...` (arguments separated by one space, a title's text in
    /// one piece as parsed), each ended by a newline: what the script says after its comments and
    /// whitespace are gone, wait steps included.
    [[nodiscard]] static std::string canonical_text(const std::vector<Step>& steps);

  private:
    void fail(const Step& s, const std::string& why);

    std::vector<Step> steps_;
    std::size_t next_ = 0;
    bool failed_ = false;
    /// The state a wait started from, and how many key presses a select or rotate has made.
    std::optional<std::uint64_t> wait_frames_, wait_turn_;
    std::optional<double> wait_ms_;
    std::optional<std::int64_t> noted_money_;
    std::optional<std::uint64_t> noted_frames_;
    std::map<std::string, std::int64_t> noted_work_;
    std::map<std::string, std::string> noted_facts_;
    std::size_t presses_ = 0;
    std::vector<std::string> log_;
};

} // namespace anno::app
