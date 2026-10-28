// Scheduler regressions. Injected memory and atomics only: no real allocation
// large enough to exhaust RAM, and no timed sleep. The admission stub rejects
// every claim, so these cases fail until the reservation rules are in place.
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

namespace {

std::atomic<std::uint64_t> g_free{0};
std::atomic<std::uint64_t> g_private{0};
std::atomic<int> g_waits{0};

std::optional<std::uint64_t> free_of() {
    return g_free.load();
}

std::optional<std::uint64_t> private_of() {
    return g_private.load();
}

void count_wait() {
    g_waits.fetch_add(1);
}

anno::test::BudgetHooks hooks_free() {
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &free_of;
    return hooks;
}

void reset(std::uint64_t free_bytes) {
    g_free = free_bytes;
    g_private = 0;
    g_waits = 0;
}

} // namespace

TEST_CASE("cpu render reservation is the measured per-pixel ceiling" *
          doctest::test_suite("budget")) {
    // Single_01 group 2: 16960 * 9170 pixels, measured additional 6628712448.
    CHECK(anno::test::cpu_render_additional_bytes(155523200) == 6687497600ULL);
    CHECK(anno::test::cpu_render_additional_bytes(0) == 0);
}

TEST_CASE("free memory below the stop threshold refuses admission" *
          doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    budget->set_stop_below(150);
    anno::test::Admit info;
    const auto hold = budget->try_reserve(10, &info);
    CHECK_FALSE(hold);
    CHECK(info.refused);
    CHECK(info.reason == "low-memory stop");
    CHECK(budget->reserved() == 0);
}

TEST_CASE("a job larger than capacity is refused" * doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    anno::test::Admit info;
    const auto hold = budget->try_reserve(101, &info);
    CHECK_FALSE(hold);
    CHECK_FALSE(info.admitted);
    CHECK(info.refused);
    CHECK(info.reason == "exceeds capacity");
    CHECK(budget->reserved() == 0);
}

TEST_CASE("headroom reduces what a job can reserve" * doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 40);
    anno::test::Admit too_big;
    CHECK_FALSE(budget->try_reserve(70, &too_big));
    CHECK(too_big.refused);
    CHECK(too_big.reason == "exceeds capacity");

    anno::test::Admit fits;
    const auto hold = budget->try_reserve(50, &fits);
    CHECK(hold);
    CHECK(fits.admitted);
    CHECK(budget->reserved() == 50);
}

TEST_CASE("concurrent claims on one free budget admit only one" * doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    std::atomic<int> wins{0};
    std::atomic<int> done{0};
    anno::test::Reservation holds[2];
    std::thread threads[2];
    for (int i = 0; i < 2; ++i) {
        threads[i] = std::thread([&, i] {
            holds[i] = budget->try_reserve(80);
            if (holds[i]) {
                wins.fetch_add(1);
            }
            done.fetch_add(1);
            while (done.load() < 2) {
                std::this_thread::yield();
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(wins.load() == 1);
    CHECK(budget->reserved() == 80);
}

TEST_CASE("differently sized jobs share a budget and keep what they reserved" *
          doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    const auto large = budget->try_reserve(60);
    const auto small = budget->try_reserve(30);
    REQUIRE(large);
    REQUIRE(small);
    CHECK(budget->reserved() == 90);

    anno::test::Admit extra;
    const auto overflow = budget->try_reserve(20, &extra);
    CHECK_FALSE(overflow);
    CHECK_FALSE(extra.admitted);
    CHECK_FALSE(extra.refused);
    CHECK(extra.reason == "waiting");
    CHECK(budget->reserved() == 90);
}

TEST_CASE("releasing a reservation frees its bytes" * doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    auto first = budget->try_reserve(80);
    REQUIRE(first);
    first.release();
    CHECK(budget->reserved() == 0);
    const auto second = budget->try_reserve(80);
    CHECK(second);
    CHECK(budget->reserved() == 80);
}

TEST_CASE("a shared in-memory budget counts every client's reservation" *
          doctest::test_suite("budget")) {
    reset(100);
    auto left = anno::test::Budget::memory(hooks_free(), 0);
    auto right = anno::test::Budget::memory(hooks_free(), 0, left);
    const auto held = left->try_reserve(60);
    REQUIRE(held);
    anno::test::Admit info;
    const auto other = right->try_reserve(50, &info);
    CHECK_FALSE(other);
    CHECK_FALSE(info.refused);
    CHECK(info.reason == "waiting");
    CHECK(left->reserved() == 60);
    CHECK(right->reserved() == 0);
}

TEST_CASE("try_reserve does not admit work that does not fit" * doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    const auto held = budget->try_reserve(80);
    REQUIRE(held);
    anno::test::Admit info;
    const auto blocked = budget->try_reserve(30, &info);
    CHECK_FALSE(blocked);
    CHECK_FALSE(info.admitted);
    CHECK_FALSE(info.refused);
    CHECK(budget->reserved() == 80);
}

TEST_CASE("an exception releases the reservation" * doctest::test_suite("budget")) {
    reset(100);
    auto budget = anno::test::Budget::memory(hooks_free(), 0);
    try {
        const auto hold = budget->reserve(40);
        REQUIRE(hold);
        CHECK(budget->reserved() == 40);
        throw std::runtime_error("cancel");
    } catch (const std::runtime_error&) {
    }
    CHECK(budget->reserved() == 0);
}

TEST_CASE("reserve waits until memory is published and does not skip the wait" *
          doctest::test_suite("budget")) {
    // Free memory alone cannot hold the request, but a releasable reservation
    // can, so the call waits instead of refusing. on_wait publishes the memory.
    reset(50);
    auto hooks = hooks_free();
    hooks.on_wait = [] {
        count_wait();
        g_free = 100;
    };
    auto budget = anno::test::Budget::memory(hooks, 0);
    const auto blocker = budget->try_reserve(45);
    REQUIRE(blocker);
    anno::test::Admit info;
    const auto hold = budget->reserve(10, &info);
    CHECK(hold);
    CHECK(info.admitted);
    CHECK(info.waited);
    CHECK(g_waits.load() == 1);
    CHECK(budget->reserved() == 55);
}

TEST_CASE("a smaller job does not take the bytes a waiting larger job needs" *
          doctest::test_suite("budget")) {
    reset(100);
    auto hooks = hooks_free();
    hooks.on_wait = &count_wait;
    auto budget = anno::test::Budget::memory(hooks, 0);
    auto held = budget->try_reserve(60);
    REQUIRE(held);

    anno::test::Reservation large;
    anno::test::Admit large_info;
    std::thread waiter([&] { large = budget->reserve(80, &large_info); });
    while (g_waits.load() == 0) {
        std::this_thread::yield();
    }

    anno::test::Admit small_info;
    const auto small = budget->try_reserve(10, &small_info);
    CHECK_FALSE(small);
    CHECK_FALSE(small_info.refused);

    held.release();
    budget->wake();
    waiter.join();
    CHECK(large);
    CHECK(large_info.admitted);
    CHECK(large_info.waited);
    CHECK(budget->reserved() == 80);
}

TEST_CASE("resident reservations are not subtracted from free memory twice" *
          doctest::test_suite("budget")) {
    reset(100);
    auto hooks = hooks_free();
    hooks.process_private = &private_of;
    auto budget = anno::test::Budget::memory(hooks, 0);
    const auto held = budget->try_reserve(60);
    REQUIRE(held);
    g_private = 60;
    g_free = 40;
    anno::test::Admit info;
    const auto next = budget->try_reserve(40, &info);
    CHECK(next);
    CHECK(info.admitted);
    CHECK(budget->reserved() == 100);
}

TEST_CASE("two threads of one file client both reserve" * doctest::test_suite("budget")) {
    // The file lock is per process. Both threads share one handle; without an
    // in-process mutex the second LockFileEx waits forever.
    reset(100);
    std::random_device device;
    const auto path = std::filesystem::temp_directory_path() /
                      ("anno1503-budget-threads-" + std::to_string(device()));
    std::filesystem::remove(path);
    auto budget = anno::test::Budget::file(path, hooks_free(), 0);
    std::atomic<int> started{0};
    anno::test::Reservation holds[2];
    std::thread threads[2];
    for (int i = 0; i < 2; ++i) {
        threads[i] = std::thread([&, i] {
            started.fetch_add(1);
            while (started.load() < 2) {
                std::this_thread::yield();
            }
            holds[i] = budget->reserve(30);
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(holds[0]);
    CHECK(holds[1]);
    CHECK(budget->reserved() == 60);
    holds[0].release();
    holds[1].release();
    budget.reset();
    std::filesystem::remove(path);
}

TEST_CASE("two file clients cannot reserve the same free bytes" * doctest::test_suite("budget")) {
    reset(100);
    std::random_device device;
    const auto path =
        std::filesystem::temp_directory_path() / ("anno1503-budget-" + std::to_string(device()));
    std::filesystem::remove(path);
    auto left = anno::test::Budget::file(path, hooks_free(), 0);
    auto right = anno::test::Budget::file(path, hooks_free(), 0);
    const auto first = left->try_reserve(80);
    anno::test::Admit info;
    const auto second = right->try_reserve(80, &info);
    CHECK(first);
    CHECK_FALSE(second);
    CHECK_FALSE(info.admitted);
    left.reset();
    right.reset();
    std::filesystem::remove(path);
}
