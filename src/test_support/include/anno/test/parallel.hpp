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
#include <mutex>
#include <new>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

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

/// Seconds a worker waits for free memory before an item (default 120, env
/// ANNO_TEST_MEM_WAIT_S; 0 = never wait). A worker that waited that long goes on
/// anyway, so the wait can delay but never deadlock a test.
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
/// memory / budget per thread), at least 1. One whole-scene render of a large
/// scenario holds a canvas of about 640 MB (Single_10, 17440 x 9131 RGBA) and
/// a worker needs about 1 GB with its scratch buffers, while CTest runs other
/// suites (and, on a shared dev machine, other builds) at the same time: with
/// ANNO_TEST_THREADS=16 on a 32 GB machine that is already busy, 16 workers
/// would need more than the free RAM. The budget is ANNO_TEST_MB_PER_THREAD
/// (see memory_budget_bytes). The bound is a snapshot: other suites start and
/// finish while this one runs, so parallel_indexed also waits for free memory
/// between items and retries an item that hit std::bad_alloc. The heavy cases
/// are also scheduled alone by CTest (the "heavy" doctest suite, see
/// anno_add_doctest in the top CMakeLists.txt), which is what keeps them from
/// meeting in the first place.
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
    std::size_t wait_timeouts = 0;     ///< waits that ran out (the item started anyway)
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
};

inline ParallelOptions parallel_options() {
    ParallelOptions o;
    o.threads = requested_test_threads();
    o.budget = memory_budget_bytes();
    o.wait_polls = memory_wait_seconds() * 4; // 250 ms per poll
    o.retries = bad_alloc_retries();
    return o;
}

namespace detail {

inline unsigned long long mib(std::optional<std::uint64_t> bytes) {
    return bytes ? static_cast<unsigned long long>(*bytes / (1024 * 1024)) : 0ULL;
}

} // namespace detail

/// f(i) for i in [0, n) on up to opt.threads threads (bounded by free memory);
/// results in index order. An exception thrown by f is rethrown after all
/// threads finished. Doctest assertions are not thread-safe: f must only
/// compute, the caller checks the results.
///
/// Memory: the thread count is bounded by free memory when the call starts,
/// but other processes run at the same time, so (a) every worker but the first
/// waits (at most opt.wait_polls polls) before taking its next item while free
/// memory is below the per-worker budget, and gives up waiting as soon as all
/// items are taken; (b) an item that throws std::bad_alloc is run again after
/// the pool finished, on one thread, up to opt.retries times with a pause.
/// A bad_alloc is never a wrong result: only the allocation failed, f produced
/// nothing. f must therefore have no side effects outside its return value
/// (true for the callers: they render or parse into locals).
/// std::bad_array_new_length is deterministic and is not retried. Each retry
/// prints one stderr line, and a summary line follows whenever the call
/// waited, retried or ran on fewer threads than requested.
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
    std::atomic<std::size_t> wait_timeouts{0};
    std::atomic<std::uint64_t> wait_ms{0};
    auto below_budget = [&] {
        const std::optional<std::uint64_t> free_bytes = opt.free_memory();
        return free_bytes && *free_bytes < opt.budget;
    };
    auto wait_for_memory = [&](bool first) {
        if (first || opt.budget == 0 || opt.wait_polls == 0 || !below_budget()) {
            return; // the first worker always goes on: the call makes progress
        }
        // Note: the memory the other workers of this pool hold counts as used
        // here, so a pool of big workers may wait for itself; the wait is
        // bounded (wait_polls) and only delays the item.
        const auto t0 = std::chrono::steady_clock::now();
        bool timed_out = true;
        for (std::uint64_t p = 0; p < opt.wait_polls; ++p) {
            std::this_thread::sleep_for(opt.poll);
            if (next.load() >= n || !below_budget()) {
                timed_out = false; // nothing left to take, or memory is back
                break;
            }
        }
        waits++;
        if (timed_out) {
            wait_timeouts++;
        }
        wait_ms += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                  std::chrono::steady_clock::now() - t0)
                                                  .count());
    };
    auto keep_error = [&] {
        if (!error) {
            error = std::current_exception();
        }
    };
    auto work = [&](bool first) {
        for (;;) {
            if (next.load() >= n) {
                return; // all items taken: no wait for memory that nobody needs
            }
            wait_for_memory(first);
            const std::size_t i = next++;
            if (i >= n) {
                return;
            }
            try {
                out[i] = f(i);
            } catch (const std::bad_array_new_length&) { // deterministic: no retry
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
    stats.threads = std::min(memory_bounded_threads(opt.threads, opt.budget, opt.free_memory()), n);
    {
        std::vector<std::jthread> pool;
        for (std::size_t t = 1; t < stats.threads; ++t) {
            pool.emplace_back(work, false);
        }
        work(true);
    }
    stats.waits = waits.load();
    stats.wait_timeouts = wait_timeouts.load();
    stats.wait_ms = wait_ms.load();
    stats.bad_allocs = retry.size();
    std::sort(retry.begin(), retry.end());
    for (const std::size_t i : retry) {
        if (opt.retries == 0) {
            break; // the first bad_alloc is the error
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
            }
            try {
                out[i] = f(i);
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
