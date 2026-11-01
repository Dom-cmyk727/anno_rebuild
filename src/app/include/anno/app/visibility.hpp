// Whether an applied command's change is on screen (W4.4 round ee review, R2). The window decides
// it from the same boundaries the window trace records (docs/app/window-trace.md), with or without
// a trace: a command's turn reaches the picture's desired state (PlayFollower), its island's object
// list is committed (ScenePicture::commit), the change's chunk keys are fresh on the CPU and usable
// on the GPU in a frame whose view holds them (FrameTracer's observation, shared in observe_keys),
// and that frame's present call succeeded. Present-call success is not physical display time.
//
// Window thread only. Memory is bounded: one part per commit of an island, at most kMaxParts per
// island (beyond it the two oldest merge, which only makes the wait stricter), a bounded queue of
// takes that are not yet shown, and a bounded ring of the outcomes of settled ones. Nothing here
// waits, and nothing is kept per command.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/app/window_trace.hpp>
#include <anno/render/frame.hpp>

namespace anno::app {

/// Where one command's visual change is. The first group still waits (the order is the order of
/// the pipeline, so the smallest value of a change's islands is the stage holding it back); the
/// second group is settled.
enum class Visibility : std::uint8_t {
    /// The command's turn is not yet in the picture's desired state.
    NotInPicture,
    /// In the desired state; the island's rebuilt object list is not committed yet.
    Committing,
    /// Committed; no frame attempt has looked at it yet (visible or offscreen is undecided).
    AwaitingFrame,
    /// In view; some chunk of the change is not fresh on the CPU yet.
    ChunksStale,
    /// In view and fresh on the CPU; a chunk is not usable on the GPU yet (upload or dissolve).
    NotOnGpu,
    /// Drawn with every chunk usable, but no present call has succeeded for such a frame yet.
    NotPresented,
    /// It was in view; the view moved away, and no frame draws it now.
    OutOfView,

    /// A frame that draws every chunk of the change was presented (the present call succeeded).
    Visible,
    /// Committed while none of its chunks was in view (the first attempt after the commit): the
    /// change is in the picture and not on screen; it is never waited for.
    Offscreen,
    /// Committed, and it invalidated no chunk (nothing the picture draws changed).
    NothingToDraw,
    /// The turn published no building change (a refused or no-op command, or an applied one with
    /// no net change).
    NoVisualChange,
    /// The turn is older than what the bounded history keeps.
    Unknown,
};

[[nodiscard]] inline bool visibility_waiting(Visibility v) {
    return v <= Visibility::OutOfView;
}
/// "" for Visible; the reason a change is not shown (yet) otherwise, for the title and the script.
[[nodiscard]] std::string visibility_note(Visibility v, std::uint64_t turn);
[[nodiscard]] const char* visibility_name(Visibility v);

class ChangeVisibility {
  public:
    /// Bounds, with their reasons. kMaxTakes: takes of building changes that are not yet shown
    /// (changes wait here while their chunks are stale, the view is elsewhere or a stuck older
    /// change holds the front); beyond it the two oldest merge (a stricter wait,
    /// never a lost one). kMaxHistory: settled takes kept to answer for recent commands; a command
    /// older than that reads Unknown. kMaxTurns: turns one take names; beyond it the take answers
    /// for its whole range.
    static constexpr std::size_t kMaxTakes = 1024, kMaxHistory = 1024, kMaxTurns = 1024,
                                 kMaxParts = 64, kMaxSettles = 256;

    /// The frame boundary took `batches` (possibly none) and the picture's desired versions went
    /// from `before` to `after` (ScenePicture::desired_versions); `followed` is the newest turn
    /// whose changes are in the desired state now (PlayFollower::followed_turn).
    void taken(std::optional<std::uint64_t> followed,
               const std::vector<std::shared_ptr<const BuildingBatch>>& batches,
               const std::vector<std::uint64_t>& before, const std::vector<std::uint64_t>& after);
    /// The commit of a frame boundary (ScenePicture::last_commit).
    void committed(const std::vector<ScenePicture::CommittedIsland>& islands);
    /// The start of a frame attempt: decides, for each change committed since the last attempt,
    /// whether it is in this attempt's view (watched) or not (Offscreen), as FrameTracer::begin.
    void begin_frame(render::Rect view, std::size_t group);
    /// The end of an attempt that reached its present call: one observation of each watched
    /// change's chunks in this frame's view (fresh on the CPU, usable on the GPU); a change whose
    /// chunks are all usable is shown when `present_ok`.
    void frame_done(bool present_ok, const FrameTracer::Ready& fresh,
                    const FrameTracer::Ready& usable);

    /// Where the change of the command that ran in `turn` is.
    [[nodiscard]] Visibility classify(std::uint64_t turn) const;
    /// Changes not shown yet (waiting takes).
    [[nodiscard]] std::size_t waiting() const { return takes_.size(); }
    /// Whether any change still needs frames to settle (a take not shown, or a watch).
    [[nodiscard]] bool busy() const { return !takes_.empty(); }
    [[nodiscard]] std::optional<std::uint64_t> followed() const { return followed_; }

  private:
    enum class Settle : std::uint8_t { None, Presented, Offscreen, NothingToDraw };
    /// One commit of an island: the chunks it invalidated, judged and shown on their own. An
    /// island's parts settle in version order (a later part that is shown waits for an earlier
    /// one), so a version is never called shown because an older commit's chunks were (round ee
    /// review, R2). Parts of one island beyond kMaxParts merge (the two oldest: union of the
    /// chunks, the newer version, judged again): a stricter wait, never a lost change.
    struct Part {
        std::uint64_t version = 0;
        std::vector<render::ChunkKey> keys;
        enum class State : std::uint8_t { Unjudged, Viewed, Offscreen, Shown, Empty };
        State state = State::Unjudged;
        bool cpu_ready = false, gpu_ready = false, in_view = true;
    };
    struct IslandState {
        /// The newest version committed, and the newest version for which every version up to it
        /// is settled (`settles` says how each ended).
        std::uint64_t committed = 0, settled = 0;
        /// How each settled version ended, ascending (bounded): a take's result is the end of the
        /// settle that completed it, not the island's latest (round ee review, R2).
        std::deque<std::pair<std::uint64_t, Settle>> settles;
        std::deque<Part> parts;
    };
    struct Take {
        std::uint64_t from = 0, through = 0;
        std::vector<std::uint64_t> turns; // the turns with a batch, ascending
        bool coarse = false; // a resync, or too many turns: the whole range belongs to it
        std::vector<std::pair<std::size_t, std::uint64_t>> islands; // (index, desired version)
        /// How it ended, fixed the moment every island of it settled (finalize_takes): a take
        /// behind a stuck older one is classified live, and must not read a later settle.
        std::optional<Visibility> result;
    };
    struct Settled {
        std::uint64_t from = 0, through = 0;
        std::vector<std::uint64_t> turns;
        bool coarse = false;
        Visibility result = Visibility::Visible;
    };

    /// Settles the island's finished parts from the front, in version order.
    void advance(IslandState& s);
    void record_settle(IslandState& s, std::uint64_t version, Settle kind);
    /// How `t` ended, from the settles that completed its islands (Visible if any was presented).
    [[nodiscard]] Visibility take_result(const Take& t) const;
    /// Fixes the result of every take that has just become done (O(takes)), then retires the done
    /// takes from the front.
    void retire_takes();
    [[nodiscard]] bool take_done(const Take& t) const;
    [[nodiscard]] Visibility take_stage(const Take& t) const;
    [[nodiscard]] static bool belongs(std::uint64_t turn, std::uint64_t from, std::uint64_t through,
                                      bool coarse, const std::vector<std::uint64_t>& turns);

    std::map<std::size_t, IslandState> islands_;
    std::deque<Take> takes_;
    std::deque<Settled> history_;
    /// The newest turn in the desired state, the turn after which the history is complete (older
    /// turns that are not in it read Unknown), and the last take's followed turn.
    std::optional<std::uint64_t> followed_;
    std::optional<std::uint64_t> floor_;
    std::set<render::ChunkKey> in_view_;
};

} // namespace anno::app
