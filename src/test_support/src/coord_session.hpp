// Client of the test-budget coordinator (coord_server.cpp). The coordinator
// decides; a client only reports and waits. Admission is by compatibility
// version (wire::kCompatVersion), not by build id (T10c F3, D1).
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include <anno/test/budget.hpp>

namespace anno::test::detail {

struct CoordSession {
    virtual ~CoordSession() = default;
    /// Progress lines of a long wait (the process budget logs them).
    std::function<void(const std::string&)> log;
    /// The coordinator this session talks to, for the gate logs: build,
    /// compatibility version, incarnation, pid, port and meter.
    std::string identity;
};

struct CoordResult {
    Admit info;
    std::uint64_t hold_id = 0;
};

/// What one connection attempt to the coordinator found.
enum class Attempt {
    Connected, ///< the coordinator accepted this client
    NotReady,  ///< no coordinator is listening, or it did not answer yet: retry
    Busy,      ///< a coordinator is there but does not accept this client now
    Failed,    ///< a foreign protocol, or a coordinator that cannot be started
};

/// The identity of a budget's connection (fixed when the budget is created).
struct ConnectArgs {
    std::filesystem::path identity;
    std::string budget_name; ///< empty: hash of the identity
    bool host_wide = false;
    BudgetHooks hooks{};
};

/// Per-budget state of the connection attempts (spawning is throttled).
struct ConnectProgress {
    std::chrono::steady_clock::time_point next_spawn{};
    std::uint64_t attempts = 0;
};

struct ConnectOutcome {
    Attempt kind = Attempt::NotReady;
    std::shared_ptr<CoordSession> session;
    /// Why not connected: what was found, in words for a gate log.
    std::string diagnostic;
};

/// One attempt: connect, spawn the coordinator if none listens (throttled),
/// send the hello, read the reply. Never waits for a busy or silent coordinator;
/// the caller decides how long to retry.
ConnectOutcome coord_connect_once(const ConnectArgs& args, ConnectProgress& progress);

CoordResult coord_try(CoordSession& session, std::uint64_t bytes, std::uint64_t headroom,
                      std::uint64_t stop);
CoordResult coord_reserve(CoordSession& session, std::uint64_t bytes, std::uint64_t headroom,
                          std::uint64_t stop);
std::uint64_t coord_release(CoordSession& session, std::uint64_t hold_id) noexcept;
CoordResult coord_cpu_acquire(CoordSession& session);
void coord_cpu_release(CoordSession& session) noexcept;
void coord_cancel(CoordSession& session);
void coord_wake(CoordSession& session);
std::uint64_t coord_local_reserved(CoordSession& session);

} // namespace anno::test::detail
