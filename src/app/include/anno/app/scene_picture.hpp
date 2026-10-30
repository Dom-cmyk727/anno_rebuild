// The picture of a play scene that follows the sim (W4.3, round du; docs/app/game-shell.md,
// "Immutable per-island object lists"). The window thread applies the building batches to
// the picture's building list and asks for a rebuild of each touched island. A worker rewrites
// the island's records and builds its object lists from immutable inputs. The window thread
// commits the newest result of each island as one new scene generation and invalidates the
// chunk keys that the changed objects touch, at every zoom level. No World and no sim type
// appears here: the input is the published batches and the scene.
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

class ScenePicture {
  public:
    /// `base` is generation 0 as loaded. `owner` keeps the buffers its islands point into
    /// alive (the loaded scene's buffers). `buildings` are the sim's buildings at load,
    /// sorted by building_before. `is_building` is the loader's classification of Anlagen
    /// ids (asked once per id up to 0x0FFF). `threads` (at least 1) build object lists.
    ScenePicture(std::shared_ptr<const render::Scene> base, std::shared_ptr<const void> owner,
                 std::vector<SceneBuilding> buildings,
                 const std::function<bool(std::uint16_t)>& is_building, std::size_t threads = 1);
    /// Stops the workers and releases the generations on the reaper thread.
    ~ScenePicture();
    ScenePicture(const ScenePicture&) = delete;
    ScenePicture& operator=(const ScenePicture&) = delete;

    /// The current generation. Window thread.
    [[nodiscard]] std::shared_ptr<const render::Scene> scene() const { return current_; }
    /// The picture's buildings, sorted by building_before. Window thread.
    [[nodiscard]] const std::vector<SceneBuilding>& buildings() const { return picture_; }

    /// Applies the batches in turn order to the building list and requests a rebuild of every
    /// island they touch. Requests coalesce: a newer request for an island replaces an older
    /// one, queued or running, whose result is then discarded. Returns the islands requested.
    /// Window thread; never waits for a worker.
    std::size_t apply(const std::vector<std::shared_ptr<const BuildingBatch>>& batches);

    /// Commits every finished result that is still the newest request of its island, as one
    /// new generation. Each changed object's chunk keys are invalidated in `frames` from the
    /// new generation on, at every zoom level. Returns the islands committed. Window thread.
    std::size_t commit(render::FrameRenderer& frames);

    /// Waits until no object build is queued or running (tests, stills; the window never
    /// calls it). Results are committed by commit().
    void wait_idle();

    /// Islands with a request not yet committed (queued, running or finished).
    [[nodiscard]] std::size_t pending() const;

    struct Stats {
        /// Islands committed, results discarded because a newer request replaced them.
        std::uint64_t committed = 0, discarded = 0;
        /// Chunk keys invalidated by commits, summed over commits.
        std::uint64_t invalidated_keys = 0;
        /// Milliseconds of the newest object-list build (worker) and of the newest commit (window).
        double last_build_ms = 0, last_commit_ms = 0;
    };
    [[nodiscard]] Stats stats() const;

  private:
    struct State;
    struct Reaper;
    std::shared_ptr<const void> owner_;
    std::shared_ptr<const render::Scene> current_;
    std::vector<SceneBuilding> picture_;
    /// The island index of each island slot number (window thread; set at construction).
    std::vector<std::pair<std::uint32_t, std::size_t>> slots_;
    std::shared_ptr<State> state_;
    std::shared_ptr<Reaper> reaper_;
    std::vector<std::thread> threads_;
};

} // namespace anno::app
