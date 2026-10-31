// MAP-2 repair regressions: who owns a journaled promise (ownership and recovery).
//
//   O1: a live holder keeps its reservation across a coordinator restart on the same journal
//       (capacity 100: A holds 50, the successor sees it, B asks for 80 and waits), and the
//       exited holder's reservation is released.
//   O2: a pid that was reused (same pid, another start stamp) is a stopped owner.
//   O3: a record the host cannot attribute (another OS's namespace, or a namespace-2 record with
//       the zero stamp that the first macOS build wrote) is kept: fail closed, never a stop.
//   O4: the CPU tokens share that ownership logic (live kept, exited released).
//   O5 (Darwin): records of the earlier Darwin coordinator (namespace 2, microsecond stamp) are
//       judged by Darwin; this process's own namespace is 3.
//
// Every case has its own port (47850..47859), a journal under temp_directory_path()/anno1503-f5,
// bounded waits, and owns each child it starts: Child stops its process in the destructor, and
// no process is found by name.
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#include "../src/coord_wire.hpp"
// The shared harness defines helpers this file does not all use (non-inline by design).
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "budget_proc_harness.hpp"
#include "child_process.hpp"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {

using anno::test::child::Child;
using namespace std::chrono_literals;
namespace wire = anno::test::wire;

constexpr std::uint16_t kPortLive = 47850;
constexpr std::uint16_t kPortReused = 47851;
constexpr std::uint16_t kPortUnattributed = 47852;
constexpr std::uint16_t kPortCpu = 47853;
constexpr std::uint16_t kPortLegacy = 47854;
constexpr std::uint16_t kPortSpace = 47855;
/// A namespace no coordinator knows.
constexpr std::uint32_t kForeignNs = 99;

std::filesystem::path f5_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "anno1503-f5";
    std::filesystem::create_directories(dir);
    return dir;
}

std::vector<std::string> journal_lines(const std::filesystem::path& journal) {
    std::ifstream in(journal);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

/// Rewrites {ns, pid, started} of every `A` (memory hold) and `C` (CPU token) line. `pick` gets
/// the old triple and returns the new one.
struct Owner3 {
    std::uint64_t ns = 0, pid = 0, started = 0;
};

template <class F> void rewrite_owner(const std::filesystem::path& journal, F pick) {
    std::vector<std::string> lines = journal_lines(journal);
    for (std::string& line : lines) {
        std::istringstream in(line);
        std::vector<std::string> field;
        for (std::string word; in >> word;) {
            field.push_back(word);
        }
        std::size_t at = 0;
        if (!field.empty() && field[0] == "A" && field.size() == 8) {
            at = 4; // A budget hold bytes ns pid started domain
        } else if (!field.empty() && field[0] == "C" && field.size() == 5) {
            at = 2; // C budget ns pid started
        } else {
            continue;
        }
        const Owner3 old{std::stoull(field[at]), std::stoull(field[at + 1]),
                         std::stoull(field[at + 2])};
        const Owner3 now = pick(old);
        field[at] = std::to_string(now.ns);
        field[at + 1] = std::to_string(now.pid);
        field[at + 2] = std::to_string(now.started);
        line.clear();
        for (std::size_t i = 0; i < field.size(); ++i) {
            line += (i == 0 ? "" : " ") + field[i];
        }
    }
    std::ofstream out(journal, std::ios::trunc);
    for (const std::string& line : lines) {
        out << line << '\n';
    }
}

/// One holder of 50 bytes (capacity 100 in the probe's meter) on its own coordinator, whose
/// journal is left for the successor.
struct Held {
    std::filesystem::path journal;
    std::string budget_path;
    Vars vars;
    std::string listening;
    Child coord;
    Child holder;
    std::uint64_t pid = 0;
};

Held start_held(std::uint16_t port, const char* name, const char* mode = "holder",
                const char* amount = "50") {
    Held h;
    h.journal = f5_dir() / (std::string(name) + ".journal");
    h.budget_path = (f5_dir() / (std::string(name) + ".budget")).string();
    remove_file(h.journal);
    h.vars = client_vars(port, name, "30");
    h.coord = start_coordinator(port, h.journal, std::nullopt, std::nullopt, h.listening);
    {
        Env env(h.vars);
        h.holder = probe({mode, h.budget_path, amount});
    }
    std::string seen;
    const std::string ready = want(h.holder, "READY", 15000ms, &seen);
    INFO("holder: " << seen);
    REQUIRE(has(ready, "READY 1"));
    return h;
}

/// "claim=0 pending=50 ..." of one `try` of `bytes` against the budget of `h`.
std::string try_claim(const Held& h, const char* bytes) {
    Child check;
    {
        Env env(h.vars);
        check = probe({"try", h.budget_path, bytes});
    }
    std::string seen;
    const std::string line = want(check, "claim=", 15000ms, &seen);
    INFO("try " << bytes << ": " << seen);
    return line;
}

/// Tries `bytes` until it is admitted or `limit` passes (an owner's stop is noticed on the
/// coordinator's own cadence).
bool admitted_eventually(const Held& h, const char* bytes, std::chrono::milliseconds limit) {
    const auto end = std::chrono::steady_clock::now() + limit;
    do {
        if (has(try_claim(h, bytes), "claim=1")) {
            return true;
        }
        std::this_thread::sleep_for(250ms);
    } while (std::chrono::steady_clock::now() < end);
    return false;
}

/// Stops the first coordinator (the holder lives on) and starts a successor on the same journal.
void restart(Held& h, std::uint16_t port, std::string* successor_line = nullptr) {
    h.coord.stop();
    h.coord = start_coordinator(port, h.journal, std::nullopt, std::nullopt, h.listening);
    if (successor_line != nullptr) {
        *successor_line = h.listening;
    }
}

} // namespace

TEST_CASE("O1: a live holder keeps its reservation through a restart; its exit releases it" *
          doctest::test_suite("budget")) {
    Held h = start_held(kPortLive, "o1");
    // The record names this host's own namespace and a real start stamp (not 0).
    const auto lines = journal_lines(h.journal);
    REQUIRE_FALSE(lines.empty());
    INFO("journal: " << lines.front());
    CHECK(has(lines.front(), " " + std::to_string(wire::this_namespace()) + " "));

    std::string successor;
    restart(h, kPortLive, &successor);
    CHECK(has(successor, "orphans=1"));

    // Capacity 100, A (alive) holds 50: B's 80 must wait, not be admitted with pending=0.
    const std::string b = try_claim(h, "80");
    CHECK(has(b, "claim=0"));
    CHECK(has(b, "pending=50"));
    // Still true a moment later: nothing stops the live owner's promise on a timer.
    std::this_thread::sleep_for(1500ms);
    CHECK(has(try_claim(h, "80"), "claim=0"));

    // A exits: the successor notices and releases its 50.
    quit_and_wait(h.holder);
    REQUIRE_FALSE(h.holder.running());
    CHECK(admitted_eventually(h, "80", 20000ms));
    h.coord.stop();
}

TEST_CASE("O2: a reused pid (another start stamp) is a stopped owner" *
          doctest::test_suite("budget")) {
    Held h = start_held(kPortReused, "o2");
    h.coord.stop();
    // The pid is the live holder's own, the stamp is not its own: that is another process.
    rewrite_owner(h.journal, [](Owner3 o) {
        o.started += 1;
        return o;
    });
    h.coord = start_coordinator(kPortReused, h.journal, std::nullopt, std::nullopt, h.listening);
    CHECK(h.holder.running()); // the "old" process is untouched
    CHECK(admitted_eventually(h, "80", 20000ms));
    quit_and_wait(h.holder);
    h.coord.stop();
}

TEST_CASE("O3: a record the host cannot attribute is kept, never read as a stop" *
          doctest::test_suite("budget")) {
    // Another OS's namespace: not judged from this host's process table, whatever the pid.
    {
        Held h = start_held(kPortUnattributed, "o3a");
        h.coord.stop();
        quit_and_wait(h.holder); // even with the real owner gone, the foreign record stays
        rewrite_owner(h.journal, [](Owner3 o) {
            o.ns = kForeignNs;
            return o;
        });
        h.coord = start_coordinator(kPortUnattributed, h.journal, std::nullopt, std::nullopt,
                                    h.listening);
        CHECK(has(h.listening, "orphans=1"));
        const std::string claim = try_claim(h, "80");
        CHECK(has(claim, "claim=0"));
        CHECK(has(claim, "pending=50"));
        std::this_thread::sleep_for(1500ms);
        CHECK(has(try_claim(h, "80"), "claim=0"));
        h.coord.stop();
    }
#if defined(__APPLE__)
    // The zero stamp that the first macOS build (8002ae8) wrote under namespace 2: not
    // reinterpreted, not a stop, even though the pid is dead.
    {
        Held h = start_held(kPortLegacy, "o3b");
        h.coord.stop();
        quit_and_wait(h.holder);
        rewrite_owner(h.journal, [](Owner3 o) {
            o.ns = wire::kNamespaceLinux;
            o.started = 0;
            return o;
        });
        h.coord =
            start_coordinator(kPortLegacy, h.journal, std::nullopt, std::nullopt, h.listening);
        CHECK(has(h.listening, "orphans=1"));
        const std::string claim = try_claim(h, "80");
        CHECK(has(claim, "claim=0"));
        std::this_thread::sleep_for(1500ms);
        CHECK(has(try_claim(h, "80"), "claim=0"));
        h.coord.stop();
    }
#endif
}

#if defined(__APPLE__)
TEST_CASE("O5: Darwin owns namespace 3, and the earlier Darwin stamp format is still judged" *
          doctest::test_suite("budget")) {
    CHECK(wire::this_namespace() == wire::kNamespaceDarwin);
    CHECK(wire::kNamespaceDarwin == 3);
    CHECK(wire::process_started() >= wire::kLegacyDarwinStampFloor); // microseconds since 1970
    CHECK(wire::is_legacy_darwin_owner(2, wire::process_started()));
    CHECK_FALSE(wire::is_legacy_darwin_owner(2, 0));
    CHECK_FALSE(wire::is_legacy_darwin_owner(2, 123456789)); // a Linux starttime in ticks
    CHECK_FALSE(wire::is_legacy_darwin_owner(3, wire::process_started()));

    // A namespace-2 record with a Darwin stamp (the earlier coordinator's format): the live
    // owner is kept, the same pid with another stamp is a stopped owner.
    Held h = start_held(kPortSpace, "o5");
    h.coord.stop();
    rewrite_owner(h.journal, [](Owner3 o) {
        o.ns = wire::kNamespaceLinux;
        return o; // pid and the real Darwin stamp unchanged
    });
    h.coord = start_coordinator(kPortSpace, h.journal, std::nullopt, std::nullopt, h.listening);
    CHECK(has(h.listening, "orphans=1"));
    CHECK(has(try_claim(h, "80"), "claim=0"));
    std::this_thread::sleep_for(1500ms);
    CHECK(has(try_claim(h, "80"), "claim=0"));
    h.coord.stop();
    rewrite_owner(h.journal, [](Owner3 o) {
        o.ns = wire::kNamespaceLinux;
        o.started += 1;
        return o;
    });
    h.coord = start_coordinator(kPortSpace, h.journal, std::nullopt, std::nullopt, h.listening);
    CHECK(admitted_eventually(h, "80", 20000ms));
    quit_and_wait(h.holder);
    h.coord.stop();
}
#endif

TEST_CASE(
    "O4: CPU tokens follow the same ownership: a live owner keeps one, an exited one frees it" *
    doctest::test_suite("budget")) {
    // One CPU token in the budget. The coordinator reads the limit from its own environment.
    Env cpu_env({{"ANNO_TEST_CPU_BUDGET", "1"}});
    Held h = start_held(kPortCpu, "o4", "cpu-holder", "1");
    restart(h, kPortCpu);

    Child waiter;
    {
        Env env(h.vars);
        waiter = probe({"cpu-waiter", h.budget_path, "1"});
    }
    // The owner is alive: the waiter does not get the token.
    CHECK(waiter.line(1500ms).empty());
    quit_and_wait(h.holder);
    REQUIRE_FALSE(h.holder.running());
    std::string seen;
    const std::string got = want(waiter, "ADMITTED", 20000ms, &seen);
    INFO("cpu waiter after the owner exited: " << seen);
    CHECK(has(got, "ADMITTED 1"));
    waiter.stop();
    h.coord.stop();
}
