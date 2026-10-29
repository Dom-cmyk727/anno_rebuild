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
#include <string>
#include <thread>
#include <vector>

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
    /// Real milliseconds a turn ran past the 100 ms turn, summed. Reported,
    /// not replayed as extra steps.
    std::uint64_t behind_ms = 0;
    bool paused = false;
};

/// Owns the world on one thread. `run_n` is the same turn the thread runs,
/// on the caller's thread, for tests. Do not call it while `start` is running.
class PlayRunner {
  public:
    explicit PlayRunner(sim::World world, unsigned speed_percent = 100);
    ~PlayRunner();
    PlayRunner(const PlayRunner&) = delete;
    PlayRunner& operator=(const PlayRunner&) = delete;

    void start();
    void stop();
    void set_paused(bool paused);
    /// Extra real time at the start of a turn. It is not added to the step.
    void set_turn_delay(std::chrono::milliseconds delay);
    void run_n(std::uint32_t turns);

    [[nodiscard]] std::shared_ptr<const PlaySnapshot> snapshot() const;
    [[nodiscard]] std::string title_line() const;
    /// Observable after synchronous `run_n`; the window never receives World.
    [[nodiscard]] std::uint64_t state_hash() const { return world_.state_hash(); }
    [[nodiscard]] std::uint32_t step_ms() const { return step_ms_; }

  private:
    void loop();
    void one_turn();
    void publish(std::uint64_t turn_us);
    [[nodiscard]] std::uint64_t p95_locked() const;

    sim::World world_;
    std::uint32_t step_ms_ = 10;
    std::uint32_t steps_ = 10;
    std::uint64_t index_ = 0;
    std::uint64_t behind_ms_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<bool> paused_{false};
    std::atomic<std::uint32_t> delay_ms_{0};
    std::thread thread_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::uint64_t>> costs_;
    std::shared_ptr<const PlaySnapshot> snap_;
};

} // namespace anno::app
