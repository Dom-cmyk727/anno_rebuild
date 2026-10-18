// Runs independent pieces of a corpus test on several threads, keeping the
// results in input order (so expected-line comparisons stay deterministic).
#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace anno::test {

/// Threads for parallel corpus work: ANNO_TEST_THREADS if set (≥ 1), else
/// min(cores, 4). The default cap bounds memory: one whole-scene render of a
/// large scenario holds a canvas of about 640 MB (Single_10, 17440 × 9131
/// RGBA), and CTest may run other suites at the same time.
inline std::size_t test_threads() {
    if (const char* v = std::getenv("ANNO_TEST_THREADS")) {
        const long n = std::strtol(v, nullptr, 10);
        if (n >= 1) {
            return static_cast<std::size_t>(n);
        }
    }
    const unsigned hw = std::thread::hardware_concurrency();
    return std::clamp<std::size_t>(hw, 1, 4);
}

/// f(i) for i in [0, n) on up to test_threads() threads; results in index
/// order. An exception thrown by f is rethrown after all threads finished.
/// Doctest assertions are not thread-safe: f must only compute, the caller
/// checks the results.
template <class F> auto parallel_indexed(std::size_t n, F f) {
    using R = decltype(f(std::size_t{0}));
    std::vector<R> out(n);
    std::atomic<std::size_t> next{0};
    std::exception_ptr error;
    std::mutex error_mutex;
    auto work = [&] {
        for (std::size_t i = next++; i < n; i = next++) {
            try {
                out[i] = f(i);
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
            pool.emplace_back(work);
        }
        work();
    }
    if (error) {
        std::rethrow_exception(error);
    }
    return out;
}

} // namespace anno::test
