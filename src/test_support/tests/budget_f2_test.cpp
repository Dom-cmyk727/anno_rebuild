// T10c F2 regressions (round cr, C1-C3). Injected counters and coordinators
// that each test starts on its own port and journal; a test stops only the
// processes it started. Nothing here allocates enough to strain the host.
//
// ANNO_TEST_F2_PORT pins every case to one port (the coordinator before F2
// listened on a fixed port only, so its fail-first run sets 47632).
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#include "child_process.hpp"

namespace {

using anno::test::child::Child;
using anno::test::child::set_env;
using anno::test::child::sibling;
using namespace std::chrono_literals;

std::atomic<std::uint64_t> g_free{100};
std::atomic<std::uint64_t> g_private{0};
std::atomic<int> g_waits{0};

std::optional<std::uint64_t> free_of() {
    return g_free.load();
}

std::optional<std::uint64_t> private_of() {
    return g_private.load();
}

void note_wait() {
    g_waits.fetch_add(1);
}

anno::test::BudgetHooks both_hooks(bool count_waits = false) {
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &free_of;
    hooks.process_private = &private_of;
    if (count_waits) {
        hooks.on_wait = &note_wait;
    }
    return hooks;
}

void reset(std::uint64_t free_bytes) {
    g_free = free_bytes;
    g_private = 0;
    g_waits = 0;
}

bool eventually(const std::function<bool()>& done, std::chrono::milliseconds limit) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= end) {
            return false;
        }
        std::this_thread::sleep_for(5ms);
    }
    return true;
}

std::uint16_t port_for(int index) {
    if (const char* fixed = std::getenv("ANNO_TEST_F2_PORT")) {
        if (fixed[0] != '\0') {
            return static_cast<std::uint16_t>(std::stoul(fixed));
        }
    }
#if defined(_WIN32)
    return static_cast<std::uint16_t>(47700 + index);
#else
    return static_cast<std::uint16_t>(47750 + index);
#endif
}

std::filesystem::path f2_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "anno1503-f2";
    std::filesystem::create_directories(dir);
    return dir;
}

/// Starts this test's own coordinator. A coordinator from before F2 prints no
/// "LISTENING" line; it binds at once, so a silent running child counts.
Child start_coord(std::uint16_t port, const std::filesystem::path& journal,
                  const std::vector<std::string>& extra = {}) {
    const auto exe = sibling("anno_budget_coord");
    REQUIRE(std::filesystem::exists(exe));
    std::vector<std::string> args = {std::to_string(port), "local", "--journal", journal.string()};
    args.insert(args.end(), extra.begin(), extra.end());
    for (int attempt = 0; attempt < 60; ++attempt) {
        Child coord = Child::start(exe, args);
        REQUIRE(coord);
        const std::string first = coord.line(1500ms);
        if (first.rfind("LISTENING", 0) == 0) {
            return coord;
        }
        if (first.empty() && coord.running()) {
            return coord;
        }
        // The port is still held (a killed predecessor's sockets): retry.
        coord.stop();
        std::this_thread::sleep_for(500ms);
    }
    FAIL("the test coordinator could not bind port " << port);
    return Child{};
}

/// Environment, coordinator and paths of one coordinator case.
struct Scope {
    std::uint16_t port = 0;
    std::filesystem::path path;
    std::filesystem::path journal;
    Child coord;
    std::vector<const char*> names;

    Scope(const std::string& name, int index, const std::vector<std::string>& extra = {},
          bool start = true) {
        port = port_for(index);
        path = f2_dir() / (name + ".bin");
        journal = f2_dir() / (name + ".journal");
        std::error_code error;
        std::filesystem::remove(journal, error);
        set("ANNO_TEST_BUDGET_COORD", "local");
        set("ANNO_TEST_BUDGET_PORT", std::to_string(port));
        set("ANNO_TEST_BUDGET_SPAWN", "0");
        set("ANNO_TEST_BUDGET_LIVENESS_MS", "1000");
        set("ANNO_TEST_BUDGET_RECHECK_MS", "100");
        set("ANNO_TEST_BUDGET_ID", "f2-" + name);
        if (start) {
            coord = start_coord(port, journal, extra);
        }
    }

    void set(const char* name, const std::string& value) {
        set_env(name, value.c_str());
        names.push_back(name);
    }

    ~Scope() {
        coord.stop();
        for (const char* name : names) {
            set_env(name, "");
        }
        std::error_code error;
        std::filesystem::remove(journal, error);
    }
};

Child probe(const std::vector<std::string>& args) {
    const auto exe = sibling("anno_budget_probe");
    REQUIRE(std::filesystem::exists(exe));
    Child child = Child::start(exe, args);
    REQUIRE(child);
    return child;
}

/// The first line of `child` that starts with `prefix`, skipping others.
std::string line_with(Child& child, const std::string& prefix, std::chrono::milliseconds limit) {
    const auto end = std::chrono::steady_clock::now() + limit;
    for (;;) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= end) {
            return {};
        }
        const std::string line =
            child.line(std::chrono::duration_cast<std::chrono::milliseconds>(end - now));
        if (line.empty() || line.rfind(prefix, 0) == 0) {
            return line;
        }
    }
}

long long ms_field(const std::string& line) {
    const auto at = line.find("ms=");
    if (at == std::string::npos) {
        return -1;
    }
    return std::stoll(line.substr(at + 3));
}

bool has(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("C1: a release that leaves its bytes allocated does not admit on an old reading" *
          doctest::test_suite("budget")) {
    // Review probe "stale": free 100, the holder reserves 80, a waiter queues 60.
    // The holder's 80 are then allocated and stay cached (free 20) and the
    // reservation is released. Only 20 is free: the waiter must not be admitted.
    reset(100);
    Scope scope("stale", 0);
    auto holder = anno::test::Budget::file(scope.path, both_hooks(), 0);
    auto held = holder->reserve(80);
    REQUIRE(held);
    auto waiter = anno::test::Budget::file(scope.path, both_hooks(true), 0);
    std::atomic<int> done{0};
    anno::test::Reservation got;
    anno::test::Admit info;
    std::thread thread([&] {
        got = waiter->reserve(60, &info);
        done = 1;
    });
    REQUIRE(eventually([&] { return g_waits.load() > 0 || done.load() != 0; }, 5000ms));
    REQUIRE(done.load() == 0);
    g_private = 80;
    g_free = 20;
    held.release();
    // A round trip after the release: every decision the release caused has
    // been sent before this reply.
    auto third = anno::test::Budget::file(scope.path, both_hooks(), 0);
    anno::test::Admit sync;
    CHECK_FALSE(third->try_reserve(90, &sync));
    waiter->cancel();
    thread.join();
    CHECK_FALSE(got);
    CHECK(info.reason == "cancelled");
    CHECK(waiter->reserved() == 0);

    // The cached bytes are returned: the same request now fits.
    g_private = 0;
    g_free = 100;
    anno::test::Admit again;
    const auto later = waiter->reserve(60, &again);
    CHECK(later);
    CHECK(again.admitted);
}

TEST_CASE("C1: external pressure after the request queued is seen when a release arrives" *
          doctest::test_suite("budget")) {
    // The request was measured at 60 free. Free memory then falls to 30 for
    // reasons outside the budget before the holder releases. A grant on the
    // old reading would put 40 on 30.
    reset(100);
    Scope scope("pressure", 1);
    auto holder = anno::test::Budget::file(scope.path, both_hooks(), 0);
    auto held = holder->reserve(50);
    REQUIRE(held);
    g_free = 60;
    auto waiter = anno::test::Budget::file(scope.path, both_hooks(true), 0);
    std::atomic<int> done{0};
    anno::test::Reservation got;
    anno::test::Admit info;
    std::thread thread([&] {
        got = waiter->reserve(40, &info);
        done = 1;
    });
    REQUIRE(eventually([&] { return g_waits.load() > 0 || done.load() != 0; }, 5000ms));
    REQUIRE(done.load() == 0);
    g_free = 30;
    held.release();
    auto third = anno::test::Budget::file(scope.path, both_hooks(), 0);
    CHECK_FALSE(third->try_reserve(90));
    waiter->cancel();
    thread.join();
    CHECK_FALSE(got);
    CHECK(info.reason == "cancelled");

    // Recovery without any event in the budget: a recheck sees it.
    g_free = 100;
    anno::test::Admit again;
    const auto later = waiter->reserve(40, &again);
    CHECK(later);
    CHECK(again.admitted);
}

TEST_CASE("C1: a waiter behind an owner with a stale residency report waits, not refused" *
          doctest::test_suite("budget")) {
    // Review probe "capacity": the owner holds 80 and has allocated them
    // (free 20), but its last residency report said 0. A 60-byte request is a
    // temporary shortage: it fits once the owner finishes.
    reset(100);
    Scope scope("capacity", 2);
    auto holder = anno::test::Budget::file(scope.path, both_hooks(), 0);
    auto held = holder->reserve(80);
    REQUIRE(held);
    g_private = 80;
    g_free = 20;
    auto waiter = anno::test::Budget::file(scope.path, both_hooks(true), 0);
    std::atomic<int> done{0};
    anno::test::Reservation got;
    anno::test::Admit info;
    std::thread thread([&] {
        got = waiter->reserve(60, &info);
        done = 1;
    });
    const bool waited = eventually([&] { return g_waits.load() > 0 || done.load() != 0; }, 5000ms);
    CHECK(waited);
    CHECK(done.load() == 0);
    CHECK_FALSE(info.refused);
    g_private = 0;
    g_free = 100;
    held.release();
    CHECK(eventually([&] { return done.load() != 0; }, 5000ms));
    if (done.load() == 0) {
        waiter->cancel();
    }
    thread.join();
    CHECK(got);
    CHECK(info.admitted);
    CHECK_FALSE(info.refused);
}

TEST_CASE("C1: in memory, a shortage behind live reservations waits instead of refusing" *
          doctest::test_suite("budget")) {
    // No private-byte hook: the holder's allocation is not visible as
    // residency. Free 20 with 80 reserved is temporary, not over capacity.
    reset(100);
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &free_of;
    hooks.on_wait = &note_wait;
    auto budget = anno::test::Budget::memory(hooks, 0);
    auto held = budget->try_reserve(80);
    REQUIRE(held);
    g_free = 20;
    std::atomic<int> done{0};
    anno::test::Reservation got;
    anno::test::Admit info;
    std::thread thread([&] {
        got = budget->reserve(60, &info);
        done = 1;
    });
    CHECK(eventually([&] { return g_waits.load() > 0 || done.load() != 0; }, 5000ms));
    CHECK(done.load() == 0);
    g_free = 100;
    held.release();
    budget->wake();
    CHECK(eventually([&] { return done.load() != 0; }, 5000ms));
    if (done.load() == 0) {
        budget->cancel();
    }
    thread.join();
    CHECK(got);
    CHECK_FALSE(info.refused);
}

TEST_CASE("C1: a guest request is bounded by the host reading, not only its own" *
          doctest::test_suite("budget")) {
    // A WSL-like client sees 100 free in its own VM while the host has 20. A
    // 60-byte request must not be admitted on the guest reading alone.
    reset(20);
    Scope scope("domains", 3);
    set_env("ANNO_TEST_BUDGET_DOMAIN", "host");
    auto host = anno::test::Budget::file(scope.path, both_hooks(), 0);
    set_env("ANNO_TEST_BUDGET_DOMAIN", "guest");
    Child first = probe({"try", scope.path.string(), "60", "100"});
    const std::string refused = line_with(first, "claim=", 10000ms);
    INFO("guest try while the host has 20: " << refused);
    CHECK(has(refused, "claim=0"));
    first.stop();

    g_free = 100;
    Child second = probe({"try", scope.path.string(), "60", "100"});
    const std::string admitted = line_with(second, "claim=", 10000ms);
    INFO("guest try after the host recovered: " << admitted);
    CHECK(has(admitted, "claim=1"));
    second.stop();
    set_env("ANNO_TEST_BUDGET_DOMAIN", "");
    CHECK(host->reserved() == 0);
}

TEST_CASE("C2: losing the coordinator ends memory and CPU waits and admitted work promptly" *
          doctest::test_suite("budget")) {
    Scope scope("loss", 4);
    scope.set("ANNO_TEST_CPU_BUDGET", "1");
    Child holder = probe({"holder", scope.path.string(), "80"});
    REQUIRE(has(holder.line(10000ms), "READY 1"));
    Child cpu_holder = probe({"cpu-holder", scope.path.string(), "1"});
    REQUIRE(has(cpu_holder.line(10000ms), "READY 1"));
    Child waiter = probe({"waiter", scope.path.string(), "60"});
    REQUIRE(has(waiter.line(10000ms), "WAITING"));
    Child cpu_waiter = probe({"cpu-waiter", scope.path.string(), "1"});
    CHECK(cpu_waiter.line(500ms).empty());

    scope.coord.stop(); // only the coordinator this test started

    const std::string memory = line_with(waiter, "ADMITTED", 5000ms);
    INFO("memory waiter: " << memory);
    CHECK(has(memory, "ADMITTED 0"));
    CHECK(has(memory, "coordinator"));
    const std::string cpu = line_with(cpu_waiter, "ADMITTED", 5000ms);
    INFO("cpu waiter: " << cpu);
    CHECK(has(cpu, "ADMITTED 0"));

    holder.send("r\n");
    const std::string released = line_with(holder, "RELEASED", 5000ms);
    INFO("release after the loss: " << released);
    CHECK(!released.empty());
    CHECK(ms_field(released) >= 0);
    CHECK(ms_field(released) < 2000);
    holder.send("t\n");
    const std::string again = line_with(holder, "CLAIM", 5000ms);
    INFO("new claim after the loss: " << again);
    CHECK(has(again, "CLAIM 0"));
}

TEST_CASE("C2: a coordinator that stops answering ends a blocked wait within the bound" *
          doctest::test_suite("budget")) {
    Scope scope("mute", 5);
    Child holder = probe({"holder", scope.path.string(), "80"});
    REQUIRE(has(holder.line(10000ms), "READY 1"));
    Child waiter = probe({"waiter", scope.path.string(), "60"});
    REQUIRE(has(waiter.line(10000ms), "WAITING"));
    scope.coord.suspend();
    const std::string result = line_with(waiter, "ADMITTED", 8000ms);
    scope.coord.resume();
    INFO("waiter with a suspended coordinator: " << result);
    CHECK(has(result, "ADMITTED 0"));
    CHECK(has(result, "coordinator"));
    CHECK(ms_field(result) >= 0);
    CHECK(ms_field(result) < 6000);
}

TEST_CASE("C3: a replacement coordinator does not admit against its predecessor's promises" *
          doctest::test_suite("budget")) {
    // Review probe "replacement": the old holder still has 80 of 100 when the
    // coordinator is replaced. A fresh 80 must wait until that work stops.
    Scope scope("replace", 6);
    scope.set("ANNO_TEST_CPU_BUDGET", "1");
    Child holder = probe({"holder", scope.path.string(), "80"});
    REQUIRE(has(holder.line(10000ms), "READY 1"));
    Child cpu_holder = probe({"cpu-holder", scope.path.string(), "1"});
    REQUIRE(has(cpu_holder.line(10000ms), "READY 1"));

    scope.coord.stop();
    scope.coord = start_coord(scope.port, scope.journal);

    Child fresh = probe({"try", scope.path.string(), "80"});
    const std::string claim = line_with(fresh, "claim=", 10000ms);
    INFO("fresh 80 beside the old holder: " << claim);
    CHECK(has(claim, "claim=0"));
    fresh.stop();
    Child cpu_waiter = probe({"cpu-waiter", scope.path.string(), "1"});
    CHECK(cpu_waiter.line(800ms).empty());

    // The old work stops: its promises are released and the waiters go on.
    holder.stop();
    cpu_holder.stop();
    Child waiter = probe({"waiter", scope.path.string(), "80"});
    const std::string admitted = line_with(waiter, "ADMITTED", 15000ms);
    INFO("after the old holder stopped: " << admitted);
    CHECK(has(admitted, "ADMITTED 1"));
    const std::string cpu = line_with(cpu_waiter, "ADMITTED", 15000ms);
    INFO("cpu after the old holder stopped: " << cpu);
    CHECK(has(cpu, "ADMITTED 1"));
}

TEST_CASE("C3: a coordinator from another build is not used" * doctest::test_suite("budget")) {
    reset(100);
    Scope scope("build", 7, {"--build-id", "0123456789abcdef"});
    bool threw = false;
    std::string message;
    try {
        auto budget = anno::test::Budget::file(scope.path, both_hooks(), 0);
        (void)budget->try_reserve(1);
    } catch (const std::exception& error) {
        threw = true;
        message = error.what();
    }
    INFO("connect to a foreign build: " << message);
    CHECK(threw);
    CHECK(has(message, "build"));
    // It held nothing, so it steps aside for the build that asked.
    CHECK(eventually([&] { return !scope.coord.running(); }, 5000ms));
}

TEST_CASE("a budget wait that cannot be served ends at its deadline without admission" *
          doctest::test_suite("budget")) {
    reset(100);
    set_env("ANNO_TEST_ADMIT_WAIT_S", "1");
    auto budget = anno::test::Budget::memory(both_hooks(true), 0);
    auto held = budget->try_reserve(80);
    REQUIRE(held);
    std::atomic<int> done{0};
    anno::test::Reservation got;
    anno::test::Admit info;
    std::thread thread([&] {
        got = budget->reserve(60, &info);
        done = 1;
    });
    CHECK(eventually([&] { return done.load() != 0; }, 6000ms));
    if (done.load() == 0) {
        budget->cancel();
    }
    thread.join();
    set_env("ANNO_TEST_ADMIT_WAIT_S", "");
    CHECK_FALSE(got);
    CHECK(info.refused);
    CHECK(info.reason == "wait deadline");
    CHECK(budget->reserved() == 80);
}

TEST_CASE("a coordinator wait that cannot be served ends at its deadline without admission" *
          doctest::test_suite("budget")) {
    reset(100);
    Scope scope("deadline", 8);
    scope.set("ANNO_TEST_ADMIT_WAIT_S", "1");
    auto holder = anno::test::Budget::file(scope.path, both_hooks(), 0);
    auto held = holder->reserve(80);
    REQUIRE(held);
    auto waiter = anno::test::Budget::file(scope.path, both_hooks(true), 0);
    std::atomic<int> done{0};
    anno::test::Reservation got;
    anno::test::Admit info;
    std::thread thread([&] {
        got = waiter->reserve(60, &info);
        done = 1;
    });
    CHECK(eventually([&] { return done.load() != 0; }, 6000ms));
    if (done.load() == 0) {
        waiter->cancel();
    }
    thread.join();
    CHECK_FALSE(got);
    CHECK(info.refused);
    CHECK(info.reason == "wait deadline");
    CHECK(holder->reserved() == 80);
}
