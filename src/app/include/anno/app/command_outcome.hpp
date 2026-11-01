// What happened to a command (W4.4 round ee, package A; docs/app/game-shell.md, "Command
// outcomes"). A command gets a local receipt when it is submitted; the runner records the turn
// and position it ran at and the authoritative result `sim::run_turn` returned for it. Receipts
// are app-side bookkeeping: they are never sent to the sim, never hashed, and not a network
// protocol. Submission and a picture update are never acceptance.
#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include <anno/sim/reject.hpp>

namespace anno::app {

/// Where a command came from: the command file (`--commands`) or the window.
enum class CommandOrigin : std::uint8_t { Script, Window };

/// A command's receipt: its origin and its number in that origin's order, from 1 (the script's
/// command order; the window's submission order). Each origin's commands run in that order, so
/// within an origin a larger number never runs before a smaller one.
struct CommandId {
    CommandOrigin origin = CommandOrigin::Window;
    std::uint64_t n = 0;
    auto operator<=>(const CommandId&) const = default;
};

/// The outcome of one command.
struct CommandOutcome {
    enum class State : std::uint8_t {
        /// Issued, no outcome recorded yet: waiting for its turn, or in a turn in flight.
        Queued,
        /// Run: the sim accepted the command (`sim::ApplyResult::ok()`). A drag can be partly
        /// built or a demolish can find nothing on a tile: `records` tells, per record.
        Applied,
        /// Run and refused as a whole (`status` says why). A command with no wire form is
        /// refused as `Unencodable` without reaching the sim (`sim::run_turn`'s rule).
        Refused,
        /// Run, but its outcome is no longer kept (the bounds of OutcomeLog): unknown here.
        Lost,
        /// Never issued.
        Unknown,
    };
    CommandId id;
    State state = State::Unknown;
    /// The turn that ran it, and its index among that turn's commands (Applied, Refused).
    std::uint64_t turn = 0;
    std::uint32_t position = 0;
    sim::Reject status = sim::Reject::None;
    /// Per record (a placement of a Build, a tile of a Demolish), in stream order.
    std::vector<sim::Reject> records;
    /// Its number in the order outcomes were recorded, from 1 with no gap (OutcomeLog).
    std::uint64_t seq = 0;
    /// Records whose status is None (a placed building, a removed one). A Build record of type 0
    /// is skipped by the sim and also None; the window never sends one.
    [[nodiscard]] std::size_t records_done() const;
};

/// What OutcomeLog::since returns: the kept outcomes from the asked sequence number on, in order,
/// and the ones that were evicted before the reader got to them.
struct OutcomeRead {
    std::vector<CommandOutcome> outcomes;
    /// The asked range that is no longer kept: [lost_from, lost_from + lost). Never silent.
    std::uint64_t lost_from = 0, lost = 0;
    /// Where the next read starts.
    std::uint64_t next = 1;
};

/// The outcomes of the commands run so far, bounded: at most `max_outcomes` outcomes and at most
/// `max_records` per-record entries in all (a 4 095-placement drag holds 4 095). The oldest go
/// first. Reason for the defaults (4 096 outcomes, 65 536 records, about 1 MiB at most): minutes
/// of a player's commands at a few per second, and a reader that falls behind further is told
/// exactly what it lost. Thread-safe: the sim thread records, any thread reads.
class OutcomeLog {
  public:
    explicit OutcomeLog(std::size_t max_outcomes = 4096, std::size_t max_records = 65536);

    /// Records the outcome of a command that ran (Applied or Refused): assigns its sequence number.
    void record(CommandOutcome outcome);
    /// Notes that `origin` has issued its commands up to number `n` (the script when it is set,
    /// the window as it submits): those ids are Queued until they run.
    void issued(CommandOrigin origin, std::uint64_t n);
    /// The outcomes from sequence number `from` on. A `from` beyond the next sequence number reads
    /// as nothing, with `next` the next sequence number (a reader never skips ahead).
    [[nodiscard]] OutcomeRead since(std::uint64_t from) const;
    /// What is known about `id` now: Queued (issued, not run), Applied or Refused (kept), Lost
    /// (run, evicted), Unknown (never issued).
    [[nodiscard]] CommandOutcome find(CommandId id) const;
    /// The sequence number the next recorded outcome gets.
    [[nodiscard]] std::uint64_t next() const;
    /// New bounds (at least one outcome is always kept); evicts what no longer fits.
    void set_bounds(std::size_t max_outcomes, std::size_t max_records);
    [[nodiscard]] std::size_t max_outcomes() const;
    [[nodiscard]] std::size_t max_records() const;

  private:
    void evict_locked();

    mutable std::mutex mu_;
    std::size_t max_outcomes_, max_records_;
    std::deque<CommandOutcome> kept_;
    std::size_t kept_records_ = 0;
    std::uint64_t next_seq_ = 1;
    /// The largest number of each origin that ran, and that was issued.
    std::uint64_t ran_[2] = {0, 0}, issued_[2] = {0, 0};
};

/// "Applied", "Refused", ... for a state; "built 3 of 5" style text is the caller's.
[[nodiscard]] const char* outcome_state_name(CommandOutcome::State state);

} // namespace anno::app
