// The command queue from the window thread to the sim thread (W4.4 B2). The window
// pushes; the sim thread takes everything queued at the start of a turn and applies
// it in push order in that turn. Wall time decides only which turn takes a command,
// never its content or its order (game-shell.md; the determinism rule).
//
// Each push gets the next receipt number (from 1, under the lock, so number order is push order:
// round ee, A1). The lock covers a push (one vector push_back) and a take (one vector swap). The
// window therefore waits for the sim only for the length of a swap, never for a turn:
// the sim does not hold the lock while it applies commands. The worst push is measured
// in the tests (docs/perf/README.md).
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include <anno/sim/command.hpp>

namespace anno::app {

class CommandQueue {
  public:
    /// A queued command and its receipt number.
    struct Entry {
        std::uint64_t n = 0;
        sim::Command command;
    };

    /// Adds `command` after everything already queued and returns its receipt number. Called
    /// from the window thread.
    std::uint64_t push(sim::Command command) {
        std::lock_guard lock(mu_);
        pending_.push_back(Entry{++last_, std::move(command)});
        return last_;
    }

    /// Takes every queued command, in push order, and leaves the queue empty. Called
    /// by the sim thread at the start of a turn. The vector is moved out under the
    /// lock; the commands are not copied while the lock is held.
    [[nodiscard]] std::vector<Entry> take() {
        std::vector<Entry> out;
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
    std::vector<Entry> pending_;
    std::uint64_t last_ = 0;
};

} // namespace anno::app
