// The command queue from the window thread to the sim thread (W4.4 B2). The window
// pushes; the sim thread takes everything queued at the start of a turn and applies
// it in push order in that turn. Wall time decides only which turn takes a command,
// never its content or its order (game-shell.md; the determinism rule).
//
// The lock covers a push (one vector push_back) and a take (one vector swap). The
// window therefore waits for the sim only for the length of a swap, never for a turn:
// the sim does not hold the lock while it applies commands. The worst push is measured
// in the tests (docs/perf/README.md).
#pragma once

#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

#include <anno/sim/command.hpp>

namespace anno::app {

class CommandQueue {
  public:
    /// Adds `command` after everything already queued. Called from the window thread.
    void push(sim::Command command) {
        std::lock_guard lock(mu_);
        pending_.push_back(std::move(command));
    }

    /// Takes every queued command, in push order, and leaves the queue empty. Called
    /// by the sim thread at the start of a turn. The vector is moved out under the
    /// lock; the commands are not copied while the lock is held.
    [[nodiscard]] std::vector<sim::Command> take() {
        std::vector<sim::Command> out;
        std::lock_guard lock(mu_);
        out.swap(pending_);
        return out;
    }

    /// The number of queued commands (a snapshot of the moment; for tests and the title).
    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mu_);
        return pending_.size();
    }

  private:
    mutable std::mutex mu_;
    std::vector<sim::Command> pending_;
};

} // namespace anno::app
