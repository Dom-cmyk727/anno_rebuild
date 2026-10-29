// Runs independent pieces of a corpus test on several threads, keeping the
// results in input order (so expected-line comparisons stay deterministic).
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <anno/test/budget.hpp>

namespace anno::test {

/// Memory that a new allocation can still get right now, in bytes; nullopt
/// when the platform has no implementation or the query failed (then no memory
/// bound applies). 0 is a real value: nothing is left. Windows: the smaller of
/// free physical memory and the headroom under the commit limit
/// (GlobalMemoryStatusEx ullAvailPhys, ullAvailPageFile): a std::bad_alloc
/// comes from the commit limit, not from free RAM. Linux: MemAvailable of
/// /proc/meminfo.
std::optional<std::uint64_t> free_memory_bytes();

namespace detail {

/// A non-negative decimal environment value, or nullopt when the variable is
/// unset, empty, negative or not a number (so a typo cannot switch a bound
/// off). Values above the range of long long saturate.
inline std::optional<std::uint64_t> env_count(const char* name) {
    const char* v = std::getenv(name);
    if (v == nullptr) {
        return std::nullopt;
    }
    char* end = nullptr;
    const long long n = std::strtoll(v, &end, 10);
    if (end == v || *end != '\0' || n < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(n);
}

} // namespace detail

/// Memory one worker may need, in bytes: ANNO_TEST_MB_PER_THREAD (default
/// 1024); 0 switches every memory bound off. An empty or non-numeric value is
/// ignored (the default applies), so it cannot switch the bound off by accident.
/// A value too large for bytes saturates instead of wrapping.
inline std::uint64_t memory_budget_bytes() {
    constexpr std::uint64_t kMiB = 1024 * 1024;
    const std::uint64_t mb = detail::env_count("ANNO_TEST_MB_PER_THREAD").value_or(1024);
    if (mb > std::numeric_limits<std::uint64_t>::max() / kMiB) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return mb * kMiB;
}

/// Host headroom kept free for the rest of the machine, in bytes
/// (ANNO_TEST_HEADROOM_MB, default 4096). Measured on this PC: one full-map
/// render left 8656285696 bytes available, so 4 GiB still admits that render
/// (docs/perf/README.md, T10c). 0 disables the headroom.
inline std::uint64_t memory_headroom_bytes() {
    constexpr std::uint64_t kMiB = 1024 * 1024;
    const std::uint64_t mb = detail::env_count("ANNO_TEST_HEADROOM_MB").value_or(4096);
    if (mb > std::numeric_limits<std::uint64_t>::max() / kMiB) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return mb * kMiB;
}

/// Available physical memory at or below this refuses new budgeted admissions
/// (ANNO_TEST_STOP_MB, default 2048). Above the point where the unbudgeted
/// heavy run was killed. 0 disables the stop.
inline std::uint64_t memory_stop_bytes() {
    constexpr std::uint64_t kMiB = 1024 * 1024;
    const std::uint64_t mb = detail::env_count("ANNO_TEST_STOP_MB").value_or(2048);
    if (mb > std::numeric_limits<std::uint64_t>::max() / kMiB) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return mb * kMiB;
}

/// Seconds a worker waits for free memory before an item (default 120, env
/// ANNO_TEST_MEM_WAIT_S). Budgeted work does not use this to start anyway.
/// Kept so a caller can still read the value. 0 is a stored zero.
inline std::uint64_t memory_wait_seconds() {
    // Capped at a day: the poll count (4 per second) must not wrap.
    constexpr std::uint64_t kMax = 24 * 3600;
    return std::min(detail::env_count("ANNO_TEST_MEM_WAIT_S").value_or(120), kMax);
}

/// How often an item that threw std::bad_alloc is run again (env
/// ANNO_TEST_BAD_ALLOC_RETRIES, default 5; 0 = never, the bad_alloc fails the
/// test at once). Every retry prints a line, so a retry that hides a real
/// fault (a corrupted size throws bad_alloc too) shows in the log.
inline unsigned bad_alloc_retries() {
    constexpr std::uint64_t kMax = 1000; // more than enough; keeps the cast exact
    return static_cast<unsigned>(
        std::min(detail::env_count("ANNO_TEST_BAD_ALLOC_RETRIES").value_or(5), kMax));
}

/// The thread count before the memory bound: ANNO_TEST_THREADS if set (>= 1),
/// else min(cores, 4).
inline std::size_t requested_test_threads() {
    const std::uint64_t n = detail::env_count("ANNO_TEST_THREADS").value_or(0);
    if (n >= 1) {
        return static_cast<std::size_t>(
            std::min<std::uint64_t>(n, std::numeric_limits<std::size_t>::max()));
    }
    const unsigned hw = std::thread::hardware_concurrency();
    return std::clamp<std::size_t>(hw, 1, 4);
}

/// `requested` bounded by free memory / budget, at least 1. No bound when the
/// budget is 0 or the free memory is unknown; 0 free bytes leave one thread.
inline std::size_t memory_bounded_threads(std::size_t requested, std::uint64_t budget,
                                          std::optional<std::uint64_t> free_bytes) {
    if (budget == 0 || !free_bytes) {
        return requested;
    }
    const std::uint64_t by_memory = std::max<std::uint64_t>(*free_bytes / budget, 1);
    return static_cast<std::size_t>(std::min<std::uint64_t>(requested, by_memory));
}

/// Threads for parallel corpus work: requested_test_threads(), at most (free
/// memory / ANNO_TEST_MB_PER_THREAD), at least 1. This is a snapshot for
/// callers that size their own loops. parallel_indexed reserves each item and
/// does not start work the reservation refuses. A full-map render passes its
/// measured size (cpu_render_additional_bytes) instead of the 1 GiB default.
inline std::size_t test_threads() {
    return memory_bounded_threads(requested_test_threads(), memory_budget_bytes(),
                                  free_memory_bytes());
}

/// What parallel_indexed did about memory; also printed (one line, stderr,
/// prefix "[anno-test memory]") whenever a wait, a retry or a thread cut by the
/// memory bound happened.
struct ParallelStats {
    std::size_t requested_threads = 0; ///< before the memory bound
    std::size_t threads = 0;           ///< workers used
    std::size_t waits = 0;             ///< items before which a worker waited for memory
    std::size_t wait_timeouts = 0;     ///< always 0: a wait no longer starts the item
    std::uint64_t wait_ms = 0;         ///< total time spent waiting, all workers
    std::size_t bad_allocs = 0;        ///< items that threw std::bad_alloc in the pool
    std::size_t retries = 0;           ///< re-runs after std::bad_alloc
};

/// The knobs of parallel_indexed; parallel_options() reads them from the
/// environment. Tests replace free_memory and shorten the pauses.
struct ParallelOptions {
    std::size_t threads = 1; ///< requested, before the memory bound
    std::uint64_t budget = 0;
    std::uint64_t wait_polls = 0; ///< polls before a waiting worker goes on
    std::chrono::milliseconds poll{250};
    std::chrono::milliseconds retry_pause{3000};
    unsigned retries = 0;
    std::optional<std::uint64_t> (*free_memory)() = &free_memory_bytes;
    bool log = true;
    /// When set, reservations use this budget. Otherwise a non-zero `budget`
    /// uses the process budget, or an in-memory one when free_memory is injected.
    std::shared_ptr<Budget> admission;
    std::uint64_t headroom = 0;
};

inline ParallelOptions parallel_options() {
    ParallelOptions o;
    o.threads = requested_test_threads();
    o.budget = memory_budget_bytes();
    o.headroom = memory_headroom_bytes();
    o.wait_polls = memory_wait_seconds() * 4; // kept for readers; admission does not time out
    o.retries = bad_alloc_retries();
    return o;
}

namespace detail {

inline unsigned long long mib(std::optional<std::uint64_t> bytes) {
    return bytes ? static_cast<unsigned long long>(*bytes / (1024 * 1024)) : 0ULL;
}

} // namespace detail

/// f(i) for i in [0, n) on up to opt.threads threads; results in index order.
/// An exception thrown by f is rethrown after all threads finished. Doctest
/// assertions are not thread-safe: f must only compute, the caller checks the
/// results.
///
/// When opt.budget is non-zero, every item, including the first and including
/// a bad_alloc retry, reserves that many bytes before f runs. The reservation
/// waits until the bytes fit or refuses them. It does not start the item after
/// a timeout. A refusal throws std::runtime_error and f is not called. An item
/// that throws std::bad_alloc is run again after the pool finished, on one
/// thread, up to opt.retries times. std::bad_array_new_length is not retried.
/// f must have no side effects outside its return value.
template <class F>
auto parallel_indexed(std::size_t n, F f, const ParallelOptions& opt,
                      ParallelStats* stats_out = nullptr) {
    using R = decltype(f(std::size_t{0}));
    std::vector<R> out(n);
    std::atomic<std::size_t> next{0};
    std::exception_ptr error;
    std::vector<std::size_t> retry;
    std::mutex error_mutex;
    std::atomic<std::size_t> waits{0};
    std::atomic<std::uint64_t> wait_ms{0};
    std::shared_ptr<Budget> owned;
    Budget* gate = opt.admission.get();
    bool process_gate = false;
    if (opt.budget > 0 && gate == nullptr) {
        if (opt.free_memory == &free_memory_bytes) {
            gate = &Budget::process();
            process_gate = true;
        } else {
            BudgetHooks hooks;
            hooks.free_bytes = opt.free_memory;
            owned = Budget::memory(hooks, opt.headroom);
            gate = owned.get();
        }
    }
    auto reserve_item = [&] {
        Reservation hold;
        if (opt.budget == 0 || gate == nullptr) {
            return hold;
        }
        const auto t0 = std::chrono::steady_clock::now();
        Admit info;
        hold = gate->reserve(opt.budget, &info);
        if (info.waited) {
            waits.fetch_add(1);
            wait_ms.fetch_add(
                static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - t0)
                                               .count()));
        }
        if (!hold) {
            throw std::runtime_error("[anno-test memory] refused estimate=" +
                                     std::to_string(opt.budget) + " reason=" + info.reason);
        }
        return hold;
    };
    auto call_f = [&](std::size_t i) {
        const Reservation hold = reserve_item();
        CpuToken cpu;
        if (process_gate && gate != nullptr) {
            cpu = gate->acquire_cpu();
        }
        out[i] = f(i);
        (void)hold;
        (void)cpu;
    };
    auto keep_error = [&] {
        if (!error) {
            error = std::current_exception();
        }
    };
    auto work = [&] {
        for (;;) {
            if (next.load() >= n) {
                return;
            }
            const std::size_t i = next++;
            if (i >= n) {
                return;
            }
            try {
                call_f(i);
            } catch (const std::bad_array_new_length&) {
                const std::lock_guard<std::mutex> lock(error_mutex);
                keep_error();
            } catch (const std::bad_alloc&) {
                const std::lock_guard<std::mutex> lock(error_mutex);
                if (opt.retries == 0) {
                    keep_error();
                }
                retry.push_back(i);
            } catch (...) {
                const std::lock_guard<std::mutex> lock(error_mutex);
                keep_error();
            }
        }
    };
    ParallelStats stats;
    stats.requested_threads = opt.threads;
    const std::size_t requested = opt.threads == 0 ? 1 : opt.threads;
    stats.threads = n == 0 ? 0 : std::min(requested, n);
    {
        std::vector<std::jthread> pool;
        for (std::size_t t = 1; t < stats.threads; ++t) {
            pool.emplace_back(work);
        }
        work();
    }
    stats.waits = waits.load();
    stats.wait_timeouts = 0;
    stats.wait_ms = wait_ms.load();
    stats.bad_allocs = retry.size();
    std::sort(retry.begin(), retry.end());
    for (const std::size_t i : retry) {
        if (opt.retries == 0) {
            break;
        }
        for (unsigned attempt = 1;; ++attempt) {
            if (attempt > 1) {
                std::this_thread::sleep_for(opt.retry_pause);
            }
            stats.retries++;
            if (opt.log) {
                std::fprintf(stderr,
                             "[anno-test memory] item %zu of %zu threw std::bad_alloc: "
                             "retry %u of %u (free %llu MiB)\n",
                             i, n, attempt, opt.retries, detail::mib(opt.free_memory()));
                std::fflush(stderr);
            }
            try {
                call_f(i);
                break;
            } catch (const std::bad_array_new_length&) {
                keep_error();
                break;
            } catch (const std::bad_alloc&) {
                if (attempt >= opt.retries) {
                    keep_error();
                    break;
                }
            } catch (...) {
                keep_error();
                break;
            }
        }
    }
    const bool cut = stats.threads < std::min(stats.requested_threads, n);
    if (opt.log && (stats.waits > 0 || stats.bad_allocs > 0 || cut)) {
        std::fprintf(stderr,
                     "[anno-test memory] %zu items on %zu of %zu requested threads; "
                     "%zu memory waits (%llu ms, %zu ran out), %zu bad_alloc items, "
                     "%zu retries; free %llu MiB, budget %llu MiB per thread\n",
                     n, stats.threads, stats.requested_threads, stats.waits,
                     static_cast<unsigned long long>(stats.wait_ms), stats.wait_timeouts,
                     stats.bad_allocs, stats.retries, detail::mib(opt.free_memory()),
                     static_cast<unsigned long long>(opt.budget / (1024 * 1024)));
        std::fflush(stderr);
    }
    if (stats_out != nullptr) {
        *stats_out = stats;
    }
    if (error) {
        std::rethrow_exception(error);
    }
    return out;
}

/// parallel_indexed with the options of the environment (parallel_options()).
template <class F> auto parallel_indexed(std::size_t n, F f) {
    return parallel_indexed(n, std::move(f), parallel_options());
}

} // namespace anno::test
