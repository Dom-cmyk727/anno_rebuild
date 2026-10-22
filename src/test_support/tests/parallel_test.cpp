// test_threads() and parallel_indexed(): the memory bound, the wait for free
// memory and the bad_alloc retry, with their counters.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>

#include <doctest/doctest.h>

#include <anno/test/parallel.hpp>

namespace {

void set_env(const char* name, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

constexpr std::uint64_t kGiB = 1024ull * 1024 * 1024;

/// Options for the tests: no environment, short pauses, no stderr lines.
anno::test::ParallelOptions quiet_options(std::size_t threads) {
    anno::test::ParallelOptions o;
    o.threads = threads;
    o.budget = 0;
    o.wait_polls = 0;
    o.poll = std::chrono::milliseconds(1);
    o.retry_pause = std::chrono::milliseconds(1);
    o.retries = 5;
    o.log = false;
    return o;
}

// A free-memory query that reports plenty once (the thread bound at the start
// of parallel_indexed) and nothing after that, so every later wait check finds
// memory short while the pool still has its full size.
std::atomic<int> g_free_calls{0};
std::optional<std::uint64_t> plenty_then_none() {
    return g_free_calls++ == 0 ? std::optional<std::uint64_t>(64 * kGiB)
                               : std::optional<std::uint64_t>(0);
}

} // namespace

TEST_CASE("test_threads is bounded by free physical memory") {
    set_env("ANNO_TEST_THREADS", "64");
    set_env("ANNO_TEST_MB_PER_THREAD", "0");
    CHECK(anno::test::test_threads() == 64); // bound off: the request wins

    // A budget larger than any machine's RAM leaves exactly one thread.
    set_env("ANNO_TEST_MB_PER_THREAD", "1000000000");
    if (anno::test::free_memory_bytes()) {
        CHECK(anno::test::test_threads() == 1);
    }

    set_env("ANNO_TEST_MB_PER_THREAD", "1024");
    const std::size_t n = anno::test::test_threads();
    CHECK(n >= 1);
    CHECK(n <= 64);
    set_env("ANNO_TEST_THREADS", "");
    set_env("ANNO_TEST_MB_PER_THREAD", "");
}

TEST_CASE("memory_bounded_threads: unknown free memory is no bound, zero free leaves one thread") {
    using anno::test::memory_bounded_threads;
    CHECK(memory_bounded_threads(16, kGiB, std::nullopt) == 16);    // query failed
    CHECK(memory_bounded_threads(16, 0, std::uint64_t{0}) == 16);   // bound off
    CHECK(memory_bounded_threads(16, kGiB, std::uint64_t{0}) == 1); // nothing left
    CHECK(memory_bounded_threads(16, kGiB, 3 * kGiB + kGiB / 2) == 3);
    CHECK(memory_bounded_threads(2, kGiB, 40 * kGiB) == 2); // the request is the cap
}

TEST_CASE("parallel_indexed runs an item again after std::bad_alloc and keeps the order") {
    std::atomic<int> failures{0};
    anno::test::ParallelStats stats;
    const auto out = anno::test::parallel_indexed(
        8,
        [&](std::size_t i) {
            if (i % 3 == 0 && failures++ < 3) {
                throw std::bad_alloc();
            }
            return static_cast<int>(i * i);
        },
        quiet_options(4), &stats);
    REQUIRE(out.size() == 8);
    for (std::size_t i = 0; i < out.size(); ++i) {
        CHECK(out[i] == static_cast<int>(i * i));
    }
    // Items 0, 3 and 6 fail once each in the pool and pass on their retry.
    CHECK(stats.bad_allocs == 3);
    CHECK(stats.retries == 3);
    CHECK(stats.waits == 0);
}

TEST_CASE("parallel_indexed with the environment's options: ANNO_TEST_THREADS and retries") {
    set_env("ANNO_TEST_THREADS", "4");
    set_env("ANNO_TEST_MB_PER_THREAD", "0");
    const auto out =
        anno::test::parallel_indexed(8, [](std::size_t i) { return static_cast<int>(i) + 1; });
    REQUIRE(out.size() == 8);
    CHECK(out[7] == 8);
    set_env("ANNO_TEST_THREADS", "");
    set_env("ANNO_TEST_MB_PER_THREAD", "");
}

TEST_CASE("ANNO_TEST_BAD_ALLOC_RETRIES: default 5, 0 switches the retry off") {
    set_env("ANNO_TEST_BAD_ALLOC_RETRIES", "");
    CHECK(anno::test::bad_alloc_retries() == 5);
    set_env("ANNO_TEST_BAD_ALLOC_RETRIES", "x");
    CHECK(anno::test::bad_alloc_retries() == 5);
    set_env("ANNO_TEST_BAD_ALLOC_RETRIES", "0");
    CHECK(anno::test::bad_alloc_retries() == 0);
    CHECK(anno::test::parallel_options().retries == 0);
    set_env("ANNO_TEST_BAD_ALLOC_RETRIES", "2");
    CHECK(anno::test::bad_alloc_retries() == 2);
    set_env("ANNO_TEST_BAD_ALLOC_RETRIES", "");
}

TEST_CASE("parallel_indexed: no retry when retries are off, at most `retries` re-runs otherwise") {
    for (const unsigned retries : {0u, 2u}) {
        CAPTURE(retries);
        auto opt = quiet_options(2);
        opt.retries = retries;
        std::atomic<int> calls{0};
        anno::test::ParallelStats stats;
        CHECK_THROWS_AS(anno::test::parallel_indexed(
                            1,
                            [&](std::size_t) -> int {
                                ++calls;
                                throw std::bad_alloc();
                            },
                            opt, &stats),
                        std::bad_alloc);
        CHECK(calls.load() == static_cast<int>(retries) + 1);
        CHECK(stats.retries == retries);
        CHECK(stats.bad_allocs == 1);
    }
}

TEST_CASE("parallel_indexed: a worker waits for memory before an item and counts the wait") {
    auto opt = quiet_options(4);
    opt.budget = kGiB;
    opt.wait_polls = 2;
    opt.free_memory = &plenty_then_none;
    g_free_calls = 0;
    anno::test::ParallelStats stats;
    const auto out = anno::test::parallel_indexed(
        64,
        [](std::size_t i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return static_cast<int>(i);
        },
        opt, &stats);
    REQUIRE(out.size() == 64);
    for (std::size_t i = 0; i < out.size(); ++i) {
        CHECK(out[i] == static_cast<int>(i));
    }
    CHECK(stats.threads == 4);
    CHECK(stats.waits >= 1); // memory never came back: the three helpers wait
    CHECK(stats.wait_timeouts >= 1);
    CHECK(stats.wait_timeouts <= stats.waits);
}

TEST_CASE("parallel_indexed: no worker waits out its memory wait once every item is taken") {
    auto opt = quiet_options(4);
    opt.budget = kGiB;
    opt.poll = std::chrono::milliseconds(5);
    opt.wait_polls = 2000; // 10 s if a worker waited to the end
    opt.free_memory = &plenty_then_none;
    g_free_calls = 0;
    anno::test::ParallelStats stats;
    const auto t0 = std::chrono::steady_clock::now();
    const auto out = anno::test::parallel_indexed(1, [](std::size_t) { return 7; }, opt, &stats);
    const auto elapsed = std::chrono::steady_clock::now() - t0;
    REQUIRE(out.size() == 1);
    CHECK(out[0] == 7);
    CHECK(stats.threads == 1); // min(4 threads, 1 item)

    // Two items on four threads: the helpers that find memory short see the
    // items taken and leave instead of polling to the end.
    g_free_calls = 0;
    const auto t1 = std::chrono::steady_clock::now();
    const auto two = anno::test::parallel_indexed(
        2,
        [](std::size_t i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            return static_cast<int>(i);
        },
        opt, &stats);
    const auto elapsed_two = std::chrono::steady_clock::now() - t1;
    REQUIRE(two.size() == 2);
    CHECK(stats.threads == 2);
    CHECK(stats.wait_timeouts == 0);
    CHECK(elapsed < std::chrono::seconds(5));
    CHECK(elapsed_two < std::chrono::seconds(5));
}

TEST_CASE("memory_budget_bytes ignores an empty or non-numeric value") {
    set_env("ANNO_TEST_MB_PER_THREAD", "");
    // Windows deletes the variable, POSIX keeps it empty: both give the default.
    CHECK(anno::test::memory_budget_bytes() == 1024ull * 1024 * 1024);
    set_env("ANNO_TEST_MB_PER_THREAD", "abc");
    CHECK(anno::test::memory_budget_bytes() == 1024ull * 1024 * 1024);
    set_env("ANNO_TEST_MB_PER_THREAD", "0");
    CHECK(anno::test::memory_budget_bytes() == 0);
    set_env("ANNO_TEST_MB_PER_THREAD", "2");
    CHECK(anno::test::memory_budget_bytes() == 2ull * 1024 * 1024);
    set_env("ANNO_TEST_MB_PER_THREAD", "");
}

TEST_CASE("memory budget and wait saturate instead of wrapping") {
    // 10^17 MiB is more than 2^64 bytes.
    set_env("ANNO_TEST_MB_PER_THREAD", "100000000000000000");
    CHECK(anno::test::memory_budget_bytes() == std::numeric_limits<std::uint64_t>::max());
    set_env("ANNO_TEST_MB_PER_THREAD", "");
    set_env("ANNO_TEST_MEM_WAIT_S", "4611686018427387904"); // 2^62: x4 would wrap
    CHECK(anno::test::memory_wait_seconds() == 24 * 3600);
    CHECK(anno::test::parallel_options().wait_polls == 4 * 24 * 3600);
    set_env("ANNO_TEST_MEM_WAIT_S", "0");
    CHECK(anno::test::memory_wait_seconds() == 0);
    set_env("ANNO_TEST_MEM_WAIT_S", "");
    CHECK(anno::test::memory_wait_seconds() == 120);
}

TEST_CASE("parallel_indexed does not retry std::bad_array_new_length") {
    std::atomic<int> calls{0};
    CHECK_THROWS_AS(anno::test::parallel_indexed(
                        1,
                        [&](std::size_t) -> int {
                            ++calls;
                            throw std::bad_array_new_length();
                        },
                        quiet_options(2)),
                    std::bad_array_new_length);
    CHECK(calls.load() == 1);
}

TEST_CASE("parallel_indexed rethrows other exceptions") {
    CHECK_THROWS_AS(anno::test::parallel_indexed(
                        4,
                        [](std::size_t i) -> int {
                            if (i == 2) {
                                throw std::runtime_error("x");
                            }
                            return 0;
                        },
                        quiet_options(2)),
                    std::runtime_error);
}
