// The window's side of play, without a window (round eb, 1A; docs/app/gpu-handoff.md): the frame
// boundary that follows the sim's building batches into the picture, and the demand that decides
// whether the window draws a frame or waits for events. The viewer calls both every loop
// iteration; the tests drive the same code with a real PlayRunner and ScenePicture.
#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/app/window_trace.hpp>
#include <anno/render/frame.hpp>

namespace anno::app {

/// Follows one PlayRunner's mailbox into one ScenePicture. Window thread only; both must outlive
/// it. Holds no generation of its own.
class PlayFollower {
  public:
    PlayFollower(ScenePicture& picture, PlayRunner& runner) : picture_(picture), runner_(runner) {}

    /// The frame boundary: takes the waiting batches, applies them, and commits what is ready
    /// (invalidating the change's keys in `frames`). Returns the islands committed. Never waits.
    /// With `trace` (the window trace, opt-in): a BatchTaken per batch taken (its turn; a collapsed
    /// resync carries the newest), at the take, and a Generation after a commit.
    std::size_t follow(render::FrameRenderer& frames, WindowTrace* trace = nullptr);
    /// The newest turn whose building changes are all in the picture's desired state (round ee,
    /// E4): the snapshot's turn read before the last take (a turn's batch is in the mailbox before
    /// its snapshot is published, PlayRunner::publish). nullopt before any turn was published.
    /// Committed and drawn is more: ScenePicture::pending() is 0 then.
    [[nodiscard]] std::optional<std::uint64_t> followed_turn() const { return followed_turn_; }
    /// Whether the picture needs frames without new input: a batch is waiting in the mailbox
    /// (PlayRunner::batches_waiting, a flag read without copying anything), or a change is not
    /// committed yet (ScenePicture::pending). O(islands) for the pending count, no building scan.
    [[nodiscard]] bool has_work() const;
    [[nodiscard]] ScenePicture& picture() { return picture_; }
    [[nodiscard]] const ScenePicture& picture() const { return picture_; }
    [[nodiscard]] PlayRunner& runner() { return runner_; }

  private:
    ScenePicture& picture_;
    PlayRunner& runner_;
    std::optional<std::uint64_t> followed_turn_;
};

/// What the window loop knows at the top of an iteration. The paused animation stops the frames,
/// but work that a frame must finish does not: chunk builds in the background, a zoom on its way,
/// the picture's batches and commits, and the GPU compositor's uploads and dissolves.
struct FrameDemand {
    /// The window can draw (not hidden, not backing off).
    bool can_draw = true;
    /// The animation is paused (space).
    bool paused = false;
    /// The chunk workers have work queued or running (FrameRenderer::busy).
    bool chunks_busy = false;
    bool zoom_animating = false;
    /// PlayFollower::has_work.
    bool play_work = false;
    /// The compositor has uploads or a dissolve to finish (gpu::GpuFrame::work_pending).
    bool gpu_work = false;
    /// Something changed that the next frame must show (an event, a resize).
    bool dirty = false;

    /// Frames follow back to back.
    [[nodiscard]] bool animating() const {
        return can_draw && (!paused || chunks_busy || zoom_animating || play_work || gpu_work);
    }
    /// The iteration draws a frame (after handling its events).
    [[nodiscard]] bool draw() const { return can_draw && (animating() || dirty); }
};

} // namespace anno::app
