// A test seam for the interval inside a publication (W4-WIN-CLOSE C1; docs/app/game-shell.md,
// "Command outcomes"): after a turn's outcomes are recorded and before the snapshot that follows
// them is swapped in. A scripted session (ANNO_DEV_SCRIPT, dev_script.hpp) arms it, the sim thread
// then waits at its next publication that follows a recorded outcome, and the window thread keeps
// running: its title and its script read what a reader sees inside the interval. The hold is the
// sim thread's wait on a condition variable, not a spin, and it ends by release, by a watchdog or
// by the hold's destruction. It never touches the sim's state.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

#include <anno/app/play.hpp>

namespace anno::app {

class PublicationHold {
  public:
    /// How long a held publication waits before it gives up (a stalled session must end).
    static constexpr std::chrono::milliseconds kDefaultWatchdog{20000};

    /// Installs the gate on `runner` (PlayRunner::set_publish_gate): call before `runner.start()`.
    /// Inert until arm(). `runner` must outlive the sim thread; the hold may be destroyed first.
    explicit PublicationHold(PlayRunner& runner,
                             std::chrono::milliseconds watchdog = kDefaultWatchdog);
    ~PublicationHold();
    PublicationHold(const PublicationHold&) = delete;
    PublicationHold& operator=(const PublicationHold&) = delete;

    /// The next publication that follows a newly recorded outcome waits. Outcomes recorded before
    /// this call do not count. Arming again while armed keeps the first baseline.
    void arm();
    /// Ends the wait and disarms; a no-op when nothing waits.
    void release();
    /// Whether a publication is waiting now.
    [[nodiscard]] bool held() const;
    /// Whether a wait ended by the watchdog (the session was not released in time).
    [[nodiscard]] bool expired() const;
    /// The turn of the publication that waits, or waited last (0 when none did).
    [[nodiscard]] std::uint64_t held_turn() const;
    /// How many publications waited (watchdog ones included).
    [[nodiscard]] std::uint64_t holds() const;

  private:
    struct State {
        std::mutex mu;
        std::condition_variable cv;
        std::atomic<bool> armed{false};
        std::uint64_t baseline = 0;
        bool held = false, expired = false, dead = false;
        std::uint64_t turn = 0, holds = 0;
        std::chrono::milliseconds watchdog;
    };
    PlayRunner& runner_;
    std::shared_ptr<State> state_;
};

} // namespace anno::app
