// Client of the host test-budget coordinator. The coordinator process owns
// admission; a file plus a local PID check is not shared by Windows and WSL.
#pragma once

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
};

struct CoordResult {
    Admit info;
    std::uint64_t hold_id = 0;
};

/// `budget_name` empty means "hash identity". `host_wide` is the process
/// budget: it prefers the shared coordinator (WSL on this machine, the local
/// host when already there) so Windows and WSL subtract the same bytes.
std::shared_ptr<CoordSession> coord_connect(const std::filesystem::path& identity,
                                            const std::string& budget_name, bool host_wide,
                                            BudgetHooks hooks);

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
