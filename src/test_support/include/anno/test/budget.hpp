// Host memory reservations for tests. The per-pixel rate is the ceiling of a
// measured full-map render (docs/perf/README.md, T10c), not an assumed size.
#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace anno::test {

/// Additional bytes a CPU full-map render allocated above the memory already
/// resident when the render started, per pixel. Ceiling of the measured
/// Single_01 group-2 ratio 42.622 (enhanced legacy, 6628712448 additional
/// bytes over 155523200 pixels). The other measured maps were lower per pixel.
inline constexpr std::uint64_t kCpuRenderBytesPerPixel = 43;

inline std::uint64_t cpu_render_additional_bytes(std::uint64_t pixels) {
    if (pixels > std::numeric_limits<std::uint64_t>::max() / kCpuRenderBytesPerPixel) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return pixels * kCpuRenderBytesPerPixel;
}

/// Injected queries. Null function pointers mean "not provided".
struct BudgetHooks {
    std::optional<std::uint64_t> (*free_bytes)() = nullptr;
    /// Process private bytes, so a reservation can be marked resident once the
    /// allocation is visible and is not subtracted from free memory twice.
    std::optional<std::uint64_t> (*process_private)() = nullptr;
    /// Called once, without the budget lock, when a reserve starts waiting.
    void (*on_wait)() = nullptr;
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
};

class Budget;

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
    /// changing an injected free-memory value.
    void wake();

  private:
    friend class Reservation;
    friend class CpuToken;
    Budget() = default;
    void release_id(std::uint64_t id) noexcept;
    void release_cpu() noexcept;
    Reservation make_hold(std::uint64_t id, std::uint64_t bytes);

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

} // namespace anno::test
