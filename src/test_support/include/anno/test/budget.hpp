// Host memory reservations for tests. The per-pixel rate is the ceiling of a
// measured full-map render (docs/perf/README.md, T10c), not an assumed size.
#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace anno::test {

namespace detail {
struct CoordSession;
} // namespace detail

/// Additional bytes a CPU full-map render allocated above the scene-load high
/// water, per pixel. Measured 2026-10-28 (docs/perf/README.md). Single_01
/// group 2, enhanced legacy, 16960×9170: peak private 7620632576 minus the
/// load high water 530382848 is 7090249728 bytes over 155523200 pixels,
/// 45.590 bytes/pixel. Single_24 group 2, 7200×2811, peaked at 1268985856;
/// above its own early peak of 356696064 that is 45.062 bytes/pixel. The
/// rate is one byte above the higher ratio.
inline constexpr std::uint64_t kCpuRenderBytesPerPixel = 46;

/// Highest private-byte peak of loading Single_01 without the full-map image:
/// a 1600×900 group-1 view at tile 800,400 (530382848). The full-map run's
/// peak at 0.729 s, before that image grew, was 529973248. Three other
/// 1600×900 views peaked between 529661952 and 530006016. Reserved before
/// the scene is built, so the setup allocations are a reservation.
inline constexpr std::uint64_t kCpuRenderSetupBytes = 530382848;

inline std::uint64_t cpu_render_additional_bytes(std::uint64_t pixels) {
    if (pixels > std::numeric_limits<std::uint64_t>::max() / kCpuRenderBytesPerPixel) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return pixels * kCpuRenderBytesPerPixel;
}

inline std::uint64_t cpu_render_setup_bytes() {
    return kCpuRenderSetupBytes;
}

/// Injected queries. Null function pointers mean "not provided".
struct BudgetHooks {
    std::optional<std::uint64_t> (*free_bytes)() = nullptr;
    /// Process private bytes, so a reservation can be marked resident once the
    /// allocation is visible and is not subtracted from free memory twice.
    std::optional<std::uint64_t> (*process_private)() = nullptr;
    /// Called once, without the budget lock, when a reserve starts waiting.
    void (*on_wait)() = nullptr;
    /// The most memory the domain can ever give (total_memory_bytes()). A
    /// request above it minus the headroom is refused as "exceeds capacity";
    /// any other shortfall waits. Null: unknown.
    std::optional<std::uint64_t> (*total_bytes)() = nullptr;
};

struct Admit {
    bool admitted = false;
    bool refused = false;
    bool waited = false;
    std::string reason;
    std::uint64_t estimate = 0;
    std::uint64_t available = 0;
    std::uint64_t headroom = 0;
    std::uint64_t pending = 0;
    std::uint64_t resident = 0;
    /// Free memory and capacity of the host domain the decision used (0:
    /// unknown).
    std::uint64_t free = 0;
    std::uint64_t total = 0;
};

class Budget;

/// The most memory this host can ever give a process: Windows the smaller
/// of physical memory and the commit limit (GlobalMemoryStatusEx ullTotalPhys,
/// ullTotalPageFile), Linux MemTotal. Nullopt when the query fails.
std::optional<std::uint64_t> total_memory_bytes();

/// Private bytes of this process (Windows: PrivateUsage; Linux: RssAnon, or
/// VmRSS when RssAnon is absent). Nullopt when the query fails.
std::optional<std::uint64_t> process_private_bytes();

/// Holds one admission until release or destruction. Move-only.
class Reservation {
  public:
    Reservation() noexcept = default;
    Reservation(const Reservation&) = delete;
    Reservation& operator=(const Reservation&) = delete;
    Reservation(Reservation&& other) noexcept;
    Reservation& operator=(Reservation&& other) noexcept;
    ~Reservation();

    explicit operator bool() const noexcept { return budget_ != nullptr; }
    std::uint64_t bytes() const noexcept { return bytes_; }
    void release() noexcept;

  private:
    friend class Budget;
    std::shared_ptr<Budget> budget_;
    std::uint64_t id_ = 0;
    std::uint64_t bytes_ = 0;
};

/// A held CPU token. In-memory budgets do not limit CPU. The process budget does.
class CpuToken {
  public:
    CpuToken() noexcept = default;
    CpuToken(const CpuToken&) = delete;
    CpuToken& operator=(const CpuToken&) = delete;
    CpuToken(CpuToken&& other) noexcept;
    CpuToken& operator=(CpuToken&& other) noexcept;
    ~CpuToken();

    explicit operator bool() const noexcept { return budget_ != nullptr; }
    void release() noexcept;

  private:
    friend class Budget;
    std::shared_ptr<Budget> budget_;
    bool held_ = false;
};

/// Coordinates reservations so concurrent work cannot each claim the same
/// free memory. `memory` is in-process (tests). `file` shares a file with
/// other clients. `process` is the host-wide budget used by the test harness.
class Budget : public std::enable_shared_from_this<Budget> {
  public:
    static std::shared_ptr<Budget> memory(BudgetHooks hooks, std::uint64_t headroom,
                                          const std::shared_ptr<Budget>& share = nullptr);
    static std::shared_ptr<Budget> file(const std::filesystem::path& path, BudgetHooks hooks,
                                        std::uint64_t headroom);
    static Budget& process();

    Reservation try_reserve(std::uint64_t bytes, Admit* info = nullptr);
    /// Blocks until the bytes are admitted or refused. There is no timeout
    /// and no path that starts the work without an admission.
    Reservation reserve(std::uint64_t bytes, Admit* info = nullptr);
    CpuToken acquire_cpu();

    std::uint64_t reserved() const;
    std::uint64_t headroom() const noexcept { return headroom_; }
    /// Bytes of free memory at or below which new admissions are refused.
    /// Zero disables the stop. The process budget uses ANNO_TEST_STOP_MB.
    void set_stop_below(std::uint64_t bytes);
    /// Wakes threads blocked in reserve or acquire_cpu. Tests use this after
    /// changing an injected free-memory value. A timed recheck does this too;
    /// neither path admits a request the budget does not fit.
    void wake();
    /// Refuses this client's blocked reserve and acquire_cpu calls. Held
    /// reservations stay held.
    void cancel();

  private:
    friend class Reservation;
    friend class CpuToken;
    Budget() = default;
    static std::shared_ptr<Budget> open_coord(const std::filesystem::path& path, BudgetHooks hooks,
                                              std::uint64_t headroom, bool host_wide);
    void release_id(std::uint64_t id) noexcept;
    void release_cpu() noexcept;
    Reservation make_hold(std::uint64_t id, std::uint64_t bytes);
    /// File budgets: the session if one is connected, else null.
    std::shared_ptr<detail::CoordSession> session_now() const;
    /// File budgets: the session, connecting first if needed. Unconnected and
    /// `waiting`, this retries until connected, cancelled or the admission
    /// deadline; otherwise it makes one attempt. On null, `refused` says why.
    std::shared_ptr<detail::CoordSession> session_for(bool waiting, Admit& refused);

    struct State;
    std::shared_ptr<State> state_;
    int slot_ = -1;
    std::uint64_t headroom_ = 0;
    std::uint64_t stop_below_ = 0;
    BudgetHooks hooks_{};
    bool logging_ = false;
};

/// Reserves cpu_render_additional_bytes(pixels) on the process budget.
/// Zero pixels reserves nothing. A refusal throws std::runtime_error.
Reservation reserve_cpu_render(std::uint64_t pixels);

/// Reserves cpu_render_setup_bytes() on the process budget. A refusal throws.
Reservation reserve_cpu_setup();

/// The build id of this library's coordinator sources: a hash of the sources
/// that make up the coordinator (src/test_support/CMakeLists.txt). It is
/// provenance: logged in every gate line, and never a compatibility test by
/// itself (T10c F3, D1). Compatibility is the compatibility version
/// (wire::kCompatVersion).
std::uint64_t coordinator_build();

/// Result of preparing this build's host coordinator artifact. Preparation does
/// not start a coordinator. `reused` is a validated manifest hit. `timed_out`
/// is the preparation deadline, which covers the lock, the compiler, validation
/// and owned cleanup (default 120 s; ANNO_TEST_COORD_PREPARE_DEADLINE_MS).
struct CoordinatorPrepare {
    bool ok = false;
    std::filesystem::path binary;
    int compiler_exit = 0;
    std::string diagnostic;
    bool reused = false;
    bool timed_out = false;
    std::string content;
    std::string config;
    std::string compiler;
    std::string flags;
};

/// Path of this build's host coordinator artifact. On Windows that is the WSL
/// binary named by coordinator_build() under the source tree's cache/coord; on
/// Linux it is the cmake-built binary beside this process. ANNO_TEST_COORD_PREPARE_ROOT,
/// when set, selects <root>/anno_budget_coord-<build> instead, so a cold test
/// does not rename the canonical file.
std::filesystem::path host_coordinator_binary();

/// Obtains a completed usable host coordinator artifact. Does not start it.
/// One wall deadline (default 120 s) covers the lock, compilation, validation
/// and cleanup of processes this call spawned. Callers are serialized on
/// <artifact>.lock (and an in-process timed mutex). A nonempty file is not
/// enough: reuse requires a manifest whose build, compatibility, config, flags
/// and content hash match, a loadable ELF64 executable, and a supervised
/// `--identity` probe of that file. The same deadline covers the hash and the
/// probe: a match that finishes after the deadline is not success.
/// A failed attempt deletes only its own staging file. Test-only hooks, all
/// documented in docs/perf/README.md: ANNO_TEST_COORD_PREPARE_FAIL,
/// ANNO_TEST_COORD_PREPARE_DEADLINE_MS, ANNO_TEST_COORD_PREPARE_ROOT,
/// ANNO_TEST_COORD_COMPILER, ANNO_TEST_COORD_LOCK_FAIL, ANNO_TEST_COORD_PREPARE_REBUILD.
CoordinatorPrepare prepare_host_coordinator();

/// Exact fields of a coordinator LISTENING line. `matches` is true only when
/// every field is well formed and equals the expected port, build, compatibility
/// version and config. A substring of the port is not a match.
struct ListeningIdentity {
    bool matches = false;
    std::uint16_t port = 0;
    std::uint64_t build = 0;
    std::uint64_t compat = 0;
    std::uint64_t incarnation = 0;
    /// Decimal `orphans=` count. A non-numeric value does not match.
    std::uint64_t orphans = 0;
    std::string config;
    std::string diagnostic;
};

ListeningIdentity coordinator_listening_identity(std::string_view line, std::uint16_t port,
                                                 std::uint64_t build, std::uint64_t compat,
                                                 std::string_view config);

struct BudgetServerOptions {
    std::uint16_t port = 47631;
    /// Every interface (the WSL host coordinator), else loopback only.
    bool all_interfaces = false;
    /// The promise journal; empty: ANNO_TEST_BUDGET_JOURNAL, else
    /// coord-<port>.journal in the user's cache.
    std::filesystem::path journal;
    /// Provenance only (the build id). Default: coordinator_build().
    std::optional<std::uint64_t> build;
    /// The compatibility version this coordinator speaks. Default: the built-in
    /// version. Tests use another value to make an incompatible peer.
    std::optional<std::uint64_t> compat;
};

/// Process entry for anno_budget_coord. Prints "LISTENING <port> ..." once it
/// owns the port and the journal, "BIND-FAILED" or "JOURNAL-FAILED" otherwise.
int run_budget_server(const BudgetServerOptions& options);

} // namespace anno::test
