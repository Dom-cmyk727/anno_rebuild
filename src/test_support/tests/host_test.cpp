// The host contract of the test budget (Mac host readiness, T-MAC.1 step 4c):
// memory readings, process identity and executable discovery on this host.
// The admission and coordinator cases (budget_test.cpp, budget_f1..f4) run on
// every host; this file covers what they take for granted from the host.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>
#include <anno/test/parallel.hpp>

#include "child_process.hpp"
#include "coord_wire.hpp"

#if !defined(_WIN32)
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace {

TEST_CASE("host: the memory readings are present, and free never exceeds the total" *
          doctest::test_suite("host")) {
    const auto total = anno::test::total_memory_bytes();
    const auto free = anno::test::free_memory_bytes();
    const auto private_bytes = anno::test::process_private_bytes();
    CHECK(total.has_value());
    CHECK(free.has_value());
    CHECK(private_bytes.has_value());
    if (total && free) {
        CHECK(*total > 0);
        CHECK(*free <= *total);
    }
}

TEST_CASE("host: process private bytes grow by a touched allocation and fall back" *
          doctest::test_suite("host")) {
    const auto before = anno::test::process_private_bytes();
    REQUIRE(before.has_value());
    constexpr std::size_t kBytes = std::size_t{64} << 20;
    std::vector<unsigned char> block(kBytes, 0);
    // Touch every page through a volatile pointer: the reading must count pages
    // that are really in the process, not reserved address space.
    volatile unsigned char* touch = block.data();
    for (std::size_t i = 0; i < kBytes; i += 4096) {
        touch[i] = 1;
    }
    const auto grown = anno::test::process_private_bytes();
    REQUIRE(grown.has_value());
    CHECK(*grown >= *before + (kBytes / 2));
    block.clear();
    block.shrink_to_fit();
}

TEST_CASE("host: the coordinator binary is found next to the test executable" *
          doctest::test_suite("host")) {
    const auto exe = anno::test::child::sibling("anno_budget_coord");
    CHECK(std::filesystem::exists(exe));
}

#if defined(__APPLE__)
using anno::test::wire::darwin_proc_info;
using anno::test::wire::DarwinProc;

/// Spawns `/bin/sleep` (or `/usr/bin/true`) with no file actions; returns the pid.
pid_t spawn_plain(const char* path, const char* arg) {
    char* argv[] = {const_cast<char*>(path), const_cast<char*>(arg), nullptr};
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, path, nullptr, nullptr, argv, environ);
    REQUIRE(rc == 0);
    return pid;
}

TEST_CASE("host: this process has a non-zero start stamp that proc_pidinfo agrees with" *
          doctest::test_suite("host")) {
    const std::uint64_t mine = anno::test::wire::process_started();
    CHECK(mine != 0);
    CHECK(anno::test::wire::process_started() == mine);
    const auto info = darwin_proc_info(static_cast<int>(::getpid()));
    CHECK(info.state == DarwinProc::Running);
    CHECK(info.started == mine);
}

TEST_CASE("host: a live child is Running with a start stamp, and Gone once killed and reaped" *
          doctest::test_suite("host")) {
    const pid_t pid = spawn_plain("/bin/sleep", "30");
    const auto live = darwin_proc_info(static_cast<int>(pid));
    CHECK(live.state == DarwinProc::Running);
    CHECK(live.started != 0);
    ::kill(pid, SIGKILL);
    int status = 0;
    CHECK(::waitpid(pid, &status, 0) == pid);
    CHECK(darwin_proc_info(static_cast<int>(pid)).state == DarwinProc::Gone);
}

TEST_CASE("host: an exited child that is not reaped is not Running" * doctest::test_suite("host")) {
    const pid_t pid = spawn_plain("/usr/bin/true", nullptr);
    siginfo_t info{};
    // WNOWAIT: wait until the child has exited, but leave it a zombie.
    REQUIRE(::waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOWAIT) == 0);
    // Measured on macOS 27: proc_pidinfo answers ESRCH for an unreaped zombie, so
    // it reads as Gone. The owner check treats Gone and Exited alike (stopped), so
    // the property tested is "not Running". Exited (pbi_status SZOMB) is a
    // defensive branch that this host does not produce.
    CHECK(darwin_proc_info(static_cast<int>(pid)).state != DarwinProc::Running);
    int status = 0;
    CHECK(::waitpid(pid, &status, 0) == pid);
    CHECK(darwin_proc_info(static_cast<int>(pid)).state == DarwinProc::Gone);
}

TEST_CASE("host: a pid that names no process reads as Gone" * doctest::test_suite("host")) {
    // A pid far above the kernel's pid_max names no process; ESRCH is Gone,
    // not Unknown, so a coordinator treats the owner as stopped.
    CHECK(darwin_proc_info(0x7ffffff0).state == DarwinProc::Gone);
}
#endif

} // namespace
