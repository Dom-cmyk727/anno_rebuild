// The picture of a play scene that follows the sim (W4.3, rounds du, dv and dw;
// docs/app/game-shell.md, "Immutable per-island object lists" and "Retained work and reclamation").
// The window thread applies the sim's building batches to the picture's building list, which is the
// DESIRED state, and asks for a rebuild of each island it changed. A worker rewrites the island's
// records and builds its object lists and change set from immutable inputs. The window thread
// commits the newest valid result of each island as one new scene generation (the COMMITTED state)
// and invalidates the chunk keys of the change. A failed build keeps the island dirty and is
// retried from the desired state; nothing waits for another sim batch. Final destruction of
// generations runs on render::SceneReclaimer, never on the window.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>

namespace anno::app {

/// The bounds of the retained work (round dw, DW4). Each value is a default with its reason; the
/// viewer may pass others (the play options). Test seams are for the test suite only.
struct PictureLimits {
    /// Islands with a build in flight or a finished result not yet committed. A request beyond it
    /// is deferred (kept in order, not dropped) and started by a later commit. Reason: each such
    /// island holds a copy of its records, lists and change set until it is committed or dropped.
    std::size_t max_outstanding = 8;
    /// Estimated bytes of the island records of the builds in flight or finished and not committed
    /// (ScenePicture::Stats::outstanding_bytes_peak). A build beyond it is deferred, unless it is
    /// the only one (a single over-budget island still makes progress). Provisional: the value is
    /// set from the record sizes of the representative five-player island, to be measured in DW5
    /// (docs/perf).
    std::size_t max_outstanding_bytes = std::size_t{64} << 20;
    /// Retired items (generations, jobs, results) that the reaper has not released yet. A commit
    /// that would add to a longer backlog is skipped and retried by the next call; it never blocks.
    /// While the reaper is behind, an island that is already in flight is deferred, not replaced
    /// (its queued job and result stay until the reaper catches up). Reason: a stalled reaper must
    /// not let work pile up.
    std::size_t max_reclaim_backlog = 4;
    /// Failed builds of one island before it is reported as failed (ScenePicture::error()). Between
    /// attempts the island waits 1, 2, 4, ... commit calls (the window's frames), so a transient
    /// failure recovers without another sim batch and a persistent one stops after this many
    /// attempts.
    std::uint32_t max_attempts = 4;
    /// Test seam: a build that throws for the island index (an injected worker failure).
    std::function<void(std::size_t index)> build_fault;
    /// Test seam: called by the reaper before it releases each item (a stalled reaper).
    std::function<void()> reclaim_gate;
};

class ScenePicture {
  public:
    using Limits = PictureLimits;

    /// `base` is generation 0 as loaded; its byte buffers are owned by the scene itself
    /// (render::Scene::Backing), so the picture needs no other owner. `buildings` are the sim's
    /// buildings at load, sorted by building_before. `is_building` is the loader's classification
    /// of Anlagen ids (asked once per id up to 0x0FFF). `threads` (at least 1) build object lists.
    ScenePicture(std::shared_ptr<const render::Scene> base, std::vector<SceneBuilding> buildings,
                 const std::function<bool(std::uint16_t)>& is_building, std::size_t threads = 1,
                 Limits limits = {});
    /// Stops the workers; the jobs, results and generation still held are released on the
    /// reclaimer.
    ~ScenePicture();
    ScenePicture(const ScenePicture&) = delete;
    ScenePicture& operator=(const ScenePicture&) = delete;

    /// The current generation (the committed state). Window thread.
    [[nodiscard]] std::shared_ptr<const render::Scene> scene() const { return current_; }
    /// The picture's buildings (the desired state), sorted by building_before. Window thread.
    [[nodiscard]] const std::vector<SceneBuilding>& buildings() const { return picture_; }

    /// Applies the batches in turn order to the desired state and requests a rebuild of every
    /// island they touch. Every change advances the island's desired version at once, even when its
    /// build is deferred. Returns the islands whose build started now (not the deferred ones).
    /// Window thread; never waits for a worker.
    std::size_t apply(const std::vector<std::shared_ptr<const BuildingBatch>>& batches);

    /// Commits every finished result that is still the desired version of its island, as one new
    /// generation, and invalidates the chunk keys of the change at every zoom level. Starts the
    /// builds that are due (deferred, retrying after a failure, or stale). Returns the islands
    /// committed. Returns 0 without committing when the reaper backlog is full (retried by the next
    /// call). Window thread.
    std::size_t commit(render::FrameRenderer& frames);

    /// Waits until no object build is queued or running (tests, stills; the window never calls it).
    /// Results are committed by commit().
    void wait_idle();

    /// Islands whose committed state differs from the desired state: changed, deferred, waiting for
    /// a retry or failed. Zero means converged. Window thread.
    [[nodiscard]] std::size_t pending() const;

    /// The failures for good of the islands that have them: one entry per island ("island N:
    /// message"), joined by "; "; empty when no island has failed for good. An island's failure is
    /// cleared by a change to that island or by retry_failed(), and by nothing else. Window thread.
    [[nodiscard]] std::string error() const;

    /// Restarts the builds of the islands that failed for good, from the current desired state. No
    /// sim command is replayed: the build reads the picture's buildings, and the last good picture
    /// stays until the new build is committed. The starts pass the same admission as any change.
    /// Returns the islands restarted. Window thread.
    std::size_t retry_failed();

    struct Stats {
        /// Islands committed; results discarded (superseded, stale or invalid).
        std::uint64_t committed = 0, discarded = 0;
        /// Results re-queued at commit because the island's lists had changed since the build.
        std::uint64_t version_requeues = 0;
        /// Chunk keys invalidated by commits, summed over commits.
        std::uint64_t invalidated_keys = 0;
        /// Requests deferred by the outstanding limit, the byte budget or reaper overload; deferred
        /// builds started later; and builds deferred by the byte budget.
        std::uint64_t deferred = 0, started_deferred = 0, budget_deferrals = 0;
        /// Finished results replaced by a newer one of the same island before their commit.
        std::uint64_t coalesced = 0;
        /// Queued jobs moved to the generation a commit made (so no old generation stays pinned).
        std::uint64_t rebased = 0;
        /// Builds that threw, builds retried after a failure, and islands that failed for good.
        std::uint64_t build_failures = 0, build_retries = 0;
        std::size_t failed_islands = 0;
        /// Islands whose committed state differs from the desired state now.
        std::size_t dirty_islands = 0;
        /// Commits skipped because the reaper backlog was full.
        std::uint64_t commit_skipped = 0;
        /// Generations, jobs and results handed to the reaper, and released by it (the scene itself
        /// is destroyed by render::SceneReclaimer; see its own counters).
        std::uint64_t reclaim_queued = 0, reclaim_completed = 0;
        /// Largest counts seen at once: builds in flight or finished and uncommitted, the reaper
        /// backlog, and the estimated bytes of those builds.
        std::size_t outstanding_peak = 0, reclaim_backlog_peak = 0;
        std::size_t outstanding_bytes = 0, outstanding_bytes_peak = 0;
        /// Milliseconds of the newest object build (worker) and of the newest commit (window).
        double last_build_ms = 0, last_commit_ms = 0;
    };
    [[nodiscard]] Stats stats() const;

    /// The per-build samples in milliseconds (bounded at 200000 builds; the newest are not kept
    /// past that): the time a build waited in the queue, and the time it took in a worker. Bench
    /// and tests only.
    struct Timings {
        std::vector<double> queue_ms, build_ms;
    };
    [[nodiscard]] Timings timings() const;

  private:
    struct State;
    struct Reaper;
    std::shared_ptr<const render::Scene> current_;
    std::vector<SceneBuilding> picture_;
    /// The island index of each island slot number (window thread; set at construction).
    std::vector<std::pair<std::uint32_t, std::size_t>> slots_;
    std::shared_ptr<State> state_;
    std::shared_ptr<Reaper> reaper_;
    std::vector<std::thread> threads_;
    Limits limits_;

    /// Starts the builds of `wanted` islands and of the due ones (deferred, retrying), within the
    /// limits. Hands what it replaces to `dropped` (retired by the caller). Returns the wanted
    /// islands started.
    std::size_t dispatch(const std::vector<std::size_t>& wanted,
                         std::vector<std::shared_ptr<const void>>& dropped);
    /// True while the release of what the picture gave up is behind: the reaper holds more than
    /// `max_reclaim_backlog` items, or the process reclaimer holds more than that many generations
    /// it has not destroyed (render::SceneReclaimer::stats().waiting, round dx DX2c). The window
    /// thread then neither commits nor replaces a build.
    [[nodiscard]] bool reclaim_overloaded() const;
};

} // namespace anno::app
