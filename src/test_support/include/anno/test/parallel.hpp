// Runs independent pieces of a corpus test on several threads, keeping the
// results in input order (so expected-line comparisons stay deterministic).
#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

namespace anno::test {

/// Physical memory that is free right now, in bytes; 0 when the platform has
/// no implementation (then test_threads() applies no memory bound). Windows:
/// GlobalMemoryStatusEx().ullAvailPhys; Linux: MemAvailable of /proc/meminfo.
std::uint64_t free_physical_bytes();

/// Memory one worker may need, in bytes: ANNO_TEST_MB_PER_THREAD (default
/// 1024); 0 switches every memory bound off.
inline std::uint64_t memory_budget_bytes() {
    std::uint64_t mb = 1024;
    if (const char* v = std::getenv("ANNO_TEST_MB_PER_THREAD")) {
        const long n = std::strtol(v, nullptr, 10);
        if (n >= 0) {
            mb = static_cast<std::uint64_t>(n);
        }
    }
    return mb * 1024 * 1024;
}

/// Threads for parallel corpus work: ANNO_TEST_THREADS if set (>= 1), else
/// min(cores, 4); in both cases at most (free physical memory / budget per
/// thread), at least 1. One whole-scene render of a large scenario holds a
/// canvas of about 640 MB (Single_10, 17440 x 9131 RGBA) and a worker needs
/// about 1 GB with its scratch buffers, while CTest runs other suites (and, on
/// a shared dev machine, other builds) at the same time: with
/// ANNO_TEST_THREADS=16 on a 32 GB machine that is already busy, 16 workers
/// would need more than the free RAM. The budget is ANNO_TEST_MB_PER_THREAD
/// (see memory_budget_bytes). The bound is a snapshot: other suites start and
/// finish while this one runs, so parallel_indexed also waits for free memory
/// between items and retries an item that hit std::bad_alloc.
inline std::size_t test_threads() {
    std::size_t threads = 0;
    if (const char* v = std::getenv("ANNO_TEST_THREADS")) {
        const long n = std::strtol(v, nullptr, 10);
        if (n >= 1) {
            threads = static_cast<std::size_t>(n);
        }
    }
    if (threads == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        threads = std::clamp<std::size_t>(hw, 1, 4);
    }
    const std::uint64_t budget = memory_budget_bytes();
    const std::uint64_t free_bytes = free_physical_bytes();
    if (budget > 0 && free_bytes > 0) {
        const std::uint64_t by_memory = free_bytes / budget;
        threads = std::min<std::size_t>(threads, std::max<std::uint64_t>(by_memory, 1));
    }
    return threads;
}

/// f(i) for i in [0, n) on up to test_threads() threads; results in index
/// order. An exception thrown by f is rethrown after all threads finished.
/// Doctest assertions are not thread-safe: f must only compute, the caller
/// checks the results.
///
/// Memory: the thread count is bounded by free RAM when the call starts, but
/// CTest runs other suites at the same time, so (a) every worker but the first
/// waits (at most 2 minutes) before taking its next item while free RAM is
/// below the per-worker budget, and (b) an item that throws std::bad_alloc is
/// run again after the pool finished, on one thread, up to 5 times with a
/// pause (the other suites will have released memory by then). A bad_alloc is
/// never a wrong result: only the allocation failed, f produced nothing.
template <class F> auto parallel_indexed(std::size_t n, F f) {
    using R = decltype(f(std::size_t{0}));
    std::vector<R> out(n);
    std::atomic<std::size_t> next{0};
    std::exception_ptr error;
    std::vector<std::size_t> retry;
    std::mutex error_mutex;
    const std::uint64_t budget = memory_budget_bytes();
    auto wait_for_memory = [&](bool first) {
        if (first || budget == 0) {
            return; // the first worker always goes on: the call makes progress
        }
        for (int i = 0; i < 480; ++i) {
            const std::uint64_t free_bytes = free_physical_bytes();
            if (free_bytes == 0 || free_bytes >= budget) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    };
    auto work = [&](bool first) {
        for (;;) {
            wait_for_memory(first);
            const std::size_t i = next++;
            if (i >= n) {
                return;
            }
            try {
                out[i] = f(i);
            } catch (const std::bad_alloc&) {
                const std::lock_guard<std::mutex> lock(error_mutex);
                retry.push_back(i);
            } catch (...) {
                const std::lock_guard<std::mutex> lock(error_mutex);
                if (!error) {
                    error = std::current_exception();
                }
            }
        }
    };
    {
        std::vector<std::jthread> pool;
        const std::size_t k = std::min(test_threads(), n);
        for (std::size_t t = 1; t < k; ++t) {
            pool.emplace_back(work, false);
        }
        work(true);
    }
    std::sort(retry.begin(), retry.end());
    for (const std::size_t i : retry) {
        for (int attempt = 0;; ++attempt) {
            try {
                out[i] = f(i);
                break;
            } catch (const std::bad_alloc&) {
                if (attempt >= 4) {
                    if (!error) {
                        error = std::current_exception();
                    }
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(3));
            } catch (...) {
                if (!error) {
                    error = std::current_exception();
                }
                break;
            }
        }
    }
    if (error) {
        std::rethrow_exception(error);
    }
    return out;
}

} // namespace anno::test
