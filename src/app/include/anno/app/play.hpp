// Play mode (docs/app/game-shell.md). The window never sees a World. This
// header is the app-side bridge: load the scenario, list the islands and
// buildings the scene is built from, and run turns on a thread that publishes
// a snapshot.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/commands_file.hpp>
#include <anno/app/loader.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

/// One island the sim placed. `template_key` is "embedded" for an island that
/// was in the scenario file, otherwise the template name below Scenes/Islands
/// (`TemplateSummary::name`).
struct PlayIsland {
    std::uint32_t slot = 0;
    std::int32_t x = 0, y = 0;
    std::uint32_t w = 0, h = 0;
    std::string template_key;
};

/// A loaded scenario and the picture inputs taken from it. The world is the
/// sim thread's once `PlayRunner` is started; until then the caller owns it.
struct PlayLoad {
    sim::World world;
    std::vector<PlayIsland> islands;
    std::vector<LinkTemplate> links;
    /// A fixed map (the layout text, data/maps/*.txt): its scene is built from
    /// `placed` by load_placed_scene, not from a scenario file.
    bool fixed_map = false;
    std::vector<PlacedIsland> placed;
    /// The scene name (the file's stem).
    std::string name;
};

/// Registry, `load_scenario_with_links`, `World::create`. The link list names
/// the template each resolved link was placed from.
[[nodiscard]] formats::Result<PlayLoad, std::string>
load_play(const std::filesystem::path& game_dir, const std::filesystem::path& file);

/// The world's buildings, in island-slot order then city, list, slot.
[[nodiscard]] std::vector<SceneBuilding> play_buildings(const sim::World& world);

/// The scenario loader's split: an Anlagen id whose type has a list in the layout.
[[nodiscard]] bool anlage_is_building(const sim::World& world, std::uint16_t anlage);

/// What the window reads. Not a World.
struct PlaySnapshot {
    std::uint64_t tick = 0;
    std::uint64_t game_ms = 0;
    /// Cost of the last turn, and the p95 over turns that finished in the
    /// last second, in microseconds.
    std::uint64_t last_turn_us = 0;
    std::uint64_t p95_us = 0;
    /// Real milliseconds of schedule skipped after the catch-up bound.
    /// Reported, not run as extra steps.
    std::uint64_t behind_ms = 0;
    bool paused = false;
    /// The last turn this snapshot follows (its index), so a reader can check
    /// the snapshot against the mailbox.
    std::uint64_t turn = 0;
};

/// Clock and sleep for the turn schedule. The app uses `steady_clock`. A test
/// supplies a clock that wakes late by a fixed amount, so drift is
/// deterministic. The runner calls these on the sim thread only. `wake`
/// unblocks a test sleep so `stop` can join; the steady clock polls in short
/// slices, so its `wake` does nothing.
class PlaySchedule {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    virtual ~PlaySchedule() = default;
    [[nodiscard]] virtual time_point now() const = 0;
    virtual void sleep_until(time_point deadline) = 0;
    virtual void sleep_for(duration d) = 0;
    virtual void wake() {}
};

/// Owns the world on one thread. `run_n` is the same turn the thread runs,
/// on the caller's thread, for tests. Do not call it while `start` is running.
/// `schedule` must outlive the runner; null uses `steady_clock`.
class PlayRunner {
  public:
    explicit PlayRunner(sim::World world, unsigned speed_percent = 100,
                        PlaySchedule* schedule = nullptr);
    ~PlayRunner();
    PlayRunner(const PlayRunner&) = delete;
    PlayRunner& operator=(const PlayRunner&) = delete;

    void start();
    void stop();
    void set_paused(bool paused);
    /// Extra real time at the start of a turn. It is not added to the step.
    /// Sliced so `stop` is noticed; call it before `start`.
    void set_turn_delay(std::chrono::milliseconds delay);
    /// Late turns run back to back up to this many, then the schedule skips
    /// forward and the skipped real time is added to `behind_ms`. Five is
    /// 500 ms at `Timing::mp_turn_real_ms`: one hitch is absorbed, and a sim
    /// that cannot keep the cadence does not queue an unbounded backlog.
    /// Call before `start`. Zero skips as soon as a deadline is missed.
    void set_catchup_turns(unsigned turns);
    [[nodiscard]] unsigned catchup_turns() const { return catchup_turns_; }
    void run_n(std::uint32_t turns);

    [[nodiscard]] std::shared_ptr<const PlaySnapshot> snapshot() const;
    /// Every building batch published since the last call, oldest first. The
    /// window calls it once per frame. It takes all of them, not only the latest
    /// snapshot's, so a turn between two frames is never skipped (W4.3 spec).
    [[nodiscard]] std::vector<std::shared_ptr<const BuildingBatch>> take_building_batches();
    /// The turns whose batches are waiting, oldest first (tests; the window
    /// uses take_building_batches). Taken under the same lock as a snapshot swap.
    [[nodiscard]] std::vector<std::uint64_t> pending_batch_turns() const;
    /// How many batches the mailbox holds before it collapses them into a resync
    /// (BuildingMailbox). Default: one second of turns. Call before `start`.
    void set_batch_bound(std::size_t bound);
    /// Commands for the turns, in file order (commands_file.hpp). A turn applies
    /// the ones that name it, before its steps (`Turn::commands`). Call before `start`.
    void set_script(std::vector<ScriptedCommand> script);
    [[nodiscard]] std::string title_line() const;
    /// Observable after synchronous `run_n`; the window never receives World.
    [[nodiscard]] std::uint64_t state_hash() const { return world_.state_hash(); }
    [[nodiscard]] std::uint32_t step_ms() const { return step_ms_; }

  private:
    void loop();
    void one_turn();
    void publish(std::uint64_t turn_us, std::uint64_t turn,
                 std::shared_ptr<const BuildingBatch> batch);
    void publish_snapshot(std::shared_ptr<const PlaySnapshot> snapshot);
    /// The buildings after the turn, diffed against the last list (sim thread).
    [[nodiscard]] std::shared_ptr<const BuildingBatch> building_batch(std::uint64_t turn);
    void note_behind();
    [[nodiscard]] std::uint64_t p95_locked() const;

    sim::World world_;
    std::unique_ptr<PlaySchedule> owned_schedule_;
    PlaySchedule* schedule_ = nullptr;
    std::uint32_t step_ms_ = 10;
    std::uint32_t steps_ = 10;
    unsigned catchup_turns_ = 5;
    std::uint64_t index_ = 0;
    std::uint64_t behind_ms_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<bool> paused_{false};
    std::atomic<std::uint32_t> delay_ms_{0};
    std::thread thread_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::uint64_t>> costs_;
    // The sorted building list the picture has been given so far (sim thread only).
    std::vector<SceneBuilding> buildings_;
    // The lock only protects the shared_ptr copy/swap and the mailbox's push or
    // take. Snapshot allocation, the building diff and all simulation work
    // happen before it is acquired.
    mutable std::mutex snap_mu_;
    std::shared_ptr<const PlaySnapshot> snap_;
    BuildingMailbox mailbox_{1};
    // The script and its cursor (sim thread only once started).
    std::vector<ScriptedCommand> script_;
    std::size_t script_next_ = 0;
};

} // namespace anno::app
