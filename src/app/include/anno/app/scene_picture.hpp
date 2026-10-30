// The picture of a play scene that follows the sim (W4.3, rounds du and dv; docs/app/game-shell.md,
// "Immutable per-island object lists" and "Retained work"). The window thread applies the
// building batches to the picture's building list and asks for a rebuild of each touched island.
// A worker rewrites the island's records, builds its object lists and the change set (the objects
// that appear and disappear, and their chunk keys) from immutable inputs. The window thread
// commits the newest result of each island as one new scene generation and invalidates the chunk
// keys of the change. Nothing the window thread drops is freed on it: superseded generations,
// jobs and results go to a reaper thread. No World and no sim type appears here: the input is the
// published batches and the scene.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

struct PictureLimits {
    /// Islands with a request not yet committed (queued, running or finished). A request
    /// beyond it is deferred: the building list keeps the change, and the island is built
    /// when a commit frees a slot, in the order it was deferred. Reason: an outstanding island
    /// holds a copy of its records and lists, so this bounds what a stalled commit retains.
    std::size_t max_outstanding = 8;
    /// Retired items (generations, jobs, results) that the reaper has not released yet. A
    /// commit that would add to a longer backlog is skipped and retried on the next call; it
    /// never blocks. Reason: a stalled reaper must not let generations pile up unseen.
    std::size_t max_reclaim_backlog = 4;
    /// Test seam: a build that throws for the island index (an injected worker failure).
    std::function<void(std::size_t index)> build_fault;
    /// Test seam: called by the reaper before it releases each item (a stalled reaper).
    std::function<void()> reclaim_gate;
};

/// The picture of the play scene (see the header comment).
class ScenePicture {
  public:
    /// The bounds of the retained work (round dv, R4). Each value is a default with its reason;
    /// the viewer may pass others (the play options). Test seams are for the test suite only.
    using Limits = PictureLimits;

    /// `base` is generation 0 as loaded; its byte buffers are owned by the scene itself
    /// (render::Scene::Backing), so the picture needs no other owner. `buildings` are the sim's
    /// buildings at load, sorted by building_before. `is_building` is the loader's classification
    /// of Anlagen ids (asked once per id up to 0x0FFF). `threads` (at least 1) build object lists.
    ScenePicture(std::shared_ptr<const render::Scene> base, std::vector<SceneBuilding> buildings,
                 const std::function<bool(std::uint16_t)>& is_building, std::size_t threads = 1,
                 Limits limits = {});
    /// Stops the workers; the jobs, results and generations still held are released on the reaper.
    ~ScenePicture();
    ScenePicture(const ScenePicture&) = delete;
    ScenePicture& operator=(const ScenePicture&) = delete;

    /// The current generation. Window thread.
    [[nodiscard]] std::shared_ptr<const render::Scene> scene() const { return current_; }
    /// The picture's buildings, sorted by building_before. Window thread.
    [[nodiscard]] const std::vector<SceneBuilding>& buildings() const { return picture_; }

    /// Applies the batches in turn order to the building list and requests a rebuild of every
    /// island they touch. A newer request for an island replaces an older one, queued or
    /// finished, and the replaced work is reclaimed off the window thread. Islands beyond
    /// Limits::max_outstanding are deferred, not dropped. Returns the islands requested (not the
    /// deferred ones). Window thread; never waits for a worker.
    std::size_t apply(const std::vector<std::shared_ptr<const BuildingBatch>>& batches);

    /// Commits every finished result that is still the newest request of its island, as one new
    /// generation, and invalidates the chunk keys of the change at every zoom level. Returns the
    /// islands committed. Returns 0 without committing when the reaper backlog is full (retried by
    /// the next call). Window thread.
    std::size_t commit(render::FrameRenderer& frames);

    /// Waits until no object build is queued or running (tests, stills; the window never calls
    /// it). Results are committed by commit(), which also starts the deferred builds.
    void wait_idle();

    /// Islands with a request not yet committed, deferred ones included.
    [[nodiscard]] std::size_t pending() const;

    struct Stats {
        /// Islands committed, results discarded because a newer request replaced them.
        std::uint64_t committed = 0, discarded = 0;
        /// Chunk keys invalidated by commits, summed over commits.
        std::uint64_t invalidated_keys = 0;
        /// Requests deferred by Limits::max_outstanding, and the deferred builds started later.
        std::uint64_t deferred = 0, started_deferred = 0;
        /// Finished results replaced by a newer one of the same island before their commit.
        std::uint64_t coalesced = 0;
        /// Queued jobs moved to the generation a commit made (so no old generation stays pinned).
        std::uint64_t rebased = 0;
        /// Builds that threw; the island keeps its last committed lists until it is requested
        /// again.
        std::uint64_t build_failures = 0;
        /// Commits skipped because the reaper backlog was full.
        std::uint64_t commit_skipped = 0;
        /// Commits whose change set was recomputed on the window thread because the island's lists
        /// had changed since the build (expected 0; a counter, not a path that should run).
        std::uint64_t recomputed_changes = 0;
        /// Items handed to the reaper (generations, jobs, results), and the largest counts seen.
        std::uint64_t reclaimed = 0;
        std::size_t outstanding_peak = 0, reclaim_backlog_peak = 0;
        /// Milliseconds of the newest object-list build and change set (worker), and of the newest
        /// commit (window).
        double last_build_ms = 0, last_commit_ms = 0;
    };
    [[nodiscard]] Stats stats() const;

  private:
    struct State;
    struct Reaper;
    /// Requests the rebuilds of `wanted` islands and of the deferred ones that fit the outstanding
    /// limit. Hands what it replaces to `dropped` (the caller retires it). Returns the wanted
    /// islands requested. Window thread.
    std::size_t dispatch(const std::vector<std::size_t>& wanted,
                         std::vector<std::shared_ptr<const void>>& dropped);
    std::shared_ptr<const render::Scene> current_;
    std::vector<SceneBuilding> picture_;
    /// The island index of each island slot number (window thread; set at construction).
    std::vector<std::pair<std::uint32_t, std::size_t>> slots_;
    std::shared_ptr<State> state_;
    std::shared_ptr<Reaper> reaper_;
    std::vector<std::thread> threads_;
    Limits limits_;
};

} // namespace anno::app
