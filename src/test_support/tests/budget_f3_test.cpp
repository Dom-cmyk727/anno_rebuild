// T10c F3 regressions (round cs, board §1 item 12 and the F3 block).
//
//   D1: a build-id mismatch must not refuse a compatible client. Builds are
//       told apart by their compatibility version; the build id is provenance.
//       An incompatible version, or an exact-build requester, waits cancellably
//       for drain and handover, with a bounded infrastructure diagnostic.
//   D2: no coordinator descriptor is inherited by a helper process (the Windows
//       meter started from WSL held the listening socket after the coordinator
//       died). A connection that answers no hello is "not ready", not a protocol
//       mismatch.
//
// Every case runs real processes (anno_budget_coord, anno_budget_probe) on its
// own port and journal and stops only the processes it started. No real memory
// is used: the probes inject free memory. The oracles are observed lines and
// states, each under a deadline. The cases use only the library surface and the
// probe's modes, so they build against the pre-F3 library for the fail-first run.
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>
#include <anno/test/parallel.hpp>

#include "../src/coord_wire.hpp"
#include "budget_proc_harness.hpp"
#include "child_process.hpp"

namespace {

using anno::test::child::Child;
using anno::test::child::set_env;
using anno::test::child::sibling;
using namespace std::chrono_literals;

// Ports of this file: 47810–47818 (the F2 file uses 47700–47760).
constexpr std::uint16_t kPortAccept = 47810;   // (a) two compatible builds
constexpr std::uint16_t kPortIncompat = 47811; // (b) incompatible version, deadline
constexpr std::uint16_t kPortCancel = 47812;   // (b) exact build, cancellation
constexpr std::uint16_t kPortDrain = 47813;    // (c) graceful drain and handover
constexpr std::uint16_t kPortCrash = 47814;    // (c) crash handover
constexpr std::uint16_t kPortMeter = 47815;    // (d) real meter child in WSL
constexpr std::uint16_t kPortSilentOk = 47816; // silent, then served
constexpr std::uint16_t kPortSilent = 47817;   // silent, deadline
constexpr std::uint16_t kPortPin = 47818;      // compatibility version pin

std::filesystem::path f3_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "anno1503-f3";
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

TEST_CASE("F3 (D1): two compatible builds share one busy coordinator" *
          doctest::test_suite("budget")) {
    // X and Y are different builds with the same compatibility version. Both
    // reservations count against the one budget: 50 + 30 pending on free 100,
    // so a third claim of 30 waits and sees pending=80.
    const auto journal = f3_dir() / "a.journal";
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortAccept, journal, 0xa0a0a0a0a0a0a0a0ULL, std::nullopt, listening);
    CHECK(has(listening, "build=a0a0a0a0a0a0a0a0"));
    CHECK(has(listening, " compat=4"));
    const auto budget_path = (f3_dir() / "a.budget").string();
    const Vars vars = client_vars(kPortAccept, "f3-a", "30");

    std::string seen;
    Child holder_x;
    {
        // Build X: the coordinator's own build id (provenance only), 50 bytes.
        Env env(vars);
        Env build({{"ANNO_TEST_BUDGET_CLIENT_BUILD", "a0a0a0a0a0a0a0a0"}});
        holder_x = probe({"holder", budget_path, "50"});
    }
    const std::string ready_x = want(holder_x, "READY", 15000ms, &seen);
    INFO("build X holder: " << seen);
    CHECK(has(ready_x, "READY 1"));

    Child holder_y;
    {
        // Build Y: another build id, the same compatibility version, 30 bytes.
        Env env(vars);
        Env build({{"ANNO_TEST_BUDGET_CLIENT_BUILD", "b1b1b1b1b1b1b1b1"}});
        holder_y = probe({"holder", budget_path, "30"});
    }
    seen.clear();
    const std::string ready_y = want(holder_y, "READY", 15000ms, &seen);
    INFO("build Y holder: " << seen);
    CHECK(has(ready_y, "READY 1"));

    Child third;
    {
        // This binary's own build: a third client sees both reservations.
        Env env(vars);
        third = probe({"try", budget_path, "30"});
    }
    seen.clear();
    const std::string claim = want(third, "claim=", 15000ms, &seen);
    INFO("third claim: " << claim);
    CHECK(has(claim, "claim=0"));
    CHECK(has(claim, "pending=80"));
    CHECK(has(claim, "reason=waiting"));
    CHECK(coord.running());
}

TEST_CASE("F3 (D1, incompatible): a busy coordinator of another version is waited for, then a "
          "bounded infrastructure diagnostic" *
          doctest::test_suite("budget")) {
    // The coordinator speaks the current compatibility version plus 1 and holds
    // a busy client of that version. This binary speaks kCompatVersion. The
    // requester must neither throw at once nor stop the holder, and with a 3 s
    // admission bound it ends with a diagnostic that names the infrastructure
    // blocker, both builds, both versions and "busy".
    const auto journal = f3_dir() / "b.journal";
    remove_file(journal);
    std::string listening;
    const std::uint64_t foreign = anno::test::wire::kCompatVersion + 1;
    Child coord =
        start_coordinator(kPortIncompat, journal, 0xb0b0b0b0b0b0b0b0ULL, foreign, listening);
    CHECK(has(listening, " compat=" + std::to_string(foreign)));
    const auto budget_path = (f3_dir() / "b.budget").string();

    std::string seen;
    Child holder;
    {
        Env env(client_vars(kPortIncompat, "f3-b", "30"));
        Env version({{"ANNO_TEST_BUDGET_CLIENT_COMPAT", std::to_string(foreign)}});
        holder = probe({"holder", budget_path, "50"});
    }
    const std::string ready = want(holder, "READY", 15000ms, &seen);
    INFO("holder output: " << seen);
    REQUIRE(has(ready, "READY 1"));

    Child requester;
    {
        Env env(client_vars(kPortIncompat, "f3-b", "3"));
        requester = probe({"reqwait", budget_path, "30"});
    }
    seen.clear();
    const std::string waiting = want(requester, "WAITING", 15000ms, &seen);
    INFO("requester output: " << seen);
    REQUIRE(has(waiting, "WAITING"));
    const std::string result = want(requester, "RESULT", 20000ms, &seen);
    INFO("requester output: " << seen);
    REQUIRE(has(result, "RESULT 0"));
    CHECK(has(result, "infrastructure"));
    CHECK(has(result, "busy"));
    CHECK(has(result, "runs build b0b0b0b0b0b0b0b0"));
    CHECK(has(result, "compatibility version " + std::to_string(foreign)));
    CHECK(has(result, "compatibility version " + std::to_string(anno::test::wire::kCompatVersion)));
    CHECK_FALSE(has(result, "speaks another protocol"));
    CHECK(holder.running());
    CHECK(coord.running());
}

TEST_CASE("F3 (D1, exact build): an exact-build requester waits and is ended by cancellation" *
          doctest::test_suite("budget")) {
    // The gate that validates the coordinator's exact build (opt-in
    // ANNO_TEST_BUDGET_EXACT_BUILD=1) is refused by a busy coordinator of another
    // build. It waits; cancel() ends it with "cancelled"; the holder and the
    // coordinator are untouched.
    const auto journal = f3_dir() / "c.journal";
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortCancel, journal, 0xc0c0c0c0c0c0c0c0ULL, std::nullopt, listening);
    const auto budget_path = (f3_dir() / "c.budget").string();

    std::string seen;
    Child holder;
    {
        Env env(client_vars(kPortCancel, "f3-c", "60"));
        holder = probe({"holder", budget_path, "50"});
    }
    const std::string ready = want(holder, "READY", 15000ms, &seen);
    INFO("holder output: " << seen);
    REQUIRE(has(ready, "READY 1"));

    Child requester;
    {
        Env env(client_vars(kPortCancel, "f3-c", "60"));
        Env exact({{"ANNO_TEST_BUDGET_EXACT_BUILD", "1"}});
        requester = probe({"reqwait", budget_path, "30"});
    }
    seen.clear();
    const std::string waiting = want(requester, "WAITING", 15000ms, &seen);
    INFO("requester output: " << seen);
    REQUIRE(has(waiting, "WAITING"));
    requester.send("c\n");
    const std::string result = want(requester, "RESULT", 10000ms, &seen);
    INFO("requester output: " << seen);
    REQUIRE(has(result, "RESULT 0"));
    CHECK(has(result, "cancelled"));
    CHECK(holder.running());
    CHECK(coord.running());
}

TEST_CASE("F3 (D1, handover): a drained busy coordinator steps aside and the requester's "
          "coordinator takes over" *
          doctest::test_suite("budget")) {
    // X (build e0) is busy with a holder of 50. The exact-build requester waits.
    // The holder exits gracefully, so X drains: no clients, no orphans. X steps
    // aside on the requester's next hello, and the requester's own coordinator
    // starts on the same port. The requester's 80 is then admitted; the journal
    // holds only the requester's 80 (the holder's release was journaled by X).
    const auto journal = f3_dir() / "d.journal";
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortDrain, journal, 0xe0e0e0e0e0e0e0e0ULL, std::nullopt, listening);
    const auto budget_path = (f3_dir() / "d.budget").string();

    std::string seen;
    Child holder;
    {
        Env env(client_vars(kPortDrain, "f3-d", "120"));
        holder = probe({"holder", budget_path, "50"});
    }
    const std::string ready = want(holder, "READY", 15000ms, &seen);
    INFO("holder output: " << seen);
    REQUIRE(has(ready, "READY 1"));

    Child requester;
    {
        Env env(client_vars(kPortDrain, "f3-d", "120", "1"));
        Env exact({{"ANNO_TEST_BUDGET_EXACT_BUILD", "1"},
                   {"ANNO_TEST_BUDGET_JOURNAL", journal.string()}});
        requester = probe({"reqwait", budget_path, "80"});
    }
    seen.clear();
    const std::string waiting = want(requester, "WAITING", 15000ms, &seen);
    INFO("requester output: " << seen);
    REQUIRE(has(waiting, "WAITING"));

    holder.send("q\n"); // the holder exits; its release is journaled by X
    REQUIRE(eventually([&] { return !coord.running(); }, 30000ms));
    const std::string result = want(requester, "RESULT", 60000ms, &seen);
    INFO("requester output: " << seen);
    CHECK(has(result, "RESULT 1"));
    CHECK(has(result, "admitted"));
    const auto holds = open_hold_bytes(journal);
    CHECK(contains_value(holds, 80));
    CHECK_FALSE(contains_value(holds, 50));
    requester.send("q\n");
}

TEST_CASE("F3 (D1, crash handover): a journaled promise of a live owner is still counted by the "
          "successor until the owner exits" *
          doctest::test_suite("budget")) {
    // X dies while its holder (50, alive) still has its promise. The requester
    // starts its own coordinator, which replays the journal: the 50 is an
    // orphan. A new claim on the successor sees pending=50. The requester's 80
    // waits until the holder exits; then it is admitted.
    const auto journal = f3_dir() / "e.journal";
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortCrash, journal, 0xe1e1e1e1e1e1e1e1ULL, std::nullopt, listening);
    const auto budget_path = (f3_dir() / "e.budget").string();

    std::string seen;
    Child holder;
    {
        Env env(client_vars(kPortCrash, "f3-e", "120"));
        holder = probe({"holder", budget_path, "50"});
    }
    const std::string ready = want(holder, "READY", 15000ms, &seen);
    INFO("holder output: " << seen);
    REQUIRE(has(ready, "READY 1"));

    Child requester;
    {
        Env env(client_vars(kPortCrash, "f3-e", "120", "1"));
        Env exact({{"ANNO_TEST_BUDGET_EXACT_BUILD", "1"},
                   {"ANNO_TEST_BUDGET_JOURNAL", journal.string()}});
        requester = probe({"reqwait", budget_path, "80"});
    }
    seen.clear();
    const std::string waiting = want(requester, "WAITING", 15000ms, &seen);
    INFO("requester output: " << seen);
    REQUIRE(has(waiting, "WAITING"));

    coord.stop(); // only the coordinator this case started dies

    // Observation of the successor: a fresh claim (no spawning) sees pending=50
    // once the requester's coordinator is listening and has replayed the journal.
    bool counted = false;
    std::string claim;
    for (int attempt = 0; attempt < 40 && !counted; ++attempt) {
        Child check;
        {
            Env no_spawn(client_vars(kPortCrash, "f3-e", "5"));
            check = probe({"try", budget_path, "60"});
        }
        claim = want(check, "claim=", 5000ms);
        counted = has(claim, "pending=50");
        std::this_thread::sleep_for(250ms);
    }
    INFO("claim against the successor: " << claim);
    CHECK(counted);

    holder.send("q\n"); // the owner exits; its 50 is released by the successor
    const std::string result = want(requester, "RESULT", 60000ms, &seen);
    INFO("requester output: " << seen);
    CHECK(has(result, "RESULT 1"));
    requester.send("q\n");
}

TEST_CASE("F3 (D2): coordinator death with the real meter child: no inherited descriptor, the port "
          "is free, a replacement binds at once, a gap client is bounded" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE(
            "skipping: WSL (Ubuntu) is not available; the real PowerShell meter runs only in WSL");
        return;
    }
    const std::string port = std::to_string(kPortMeter);
    const auto budget_path = (f3_dir() / "f.budget").string();
    // The WSL coordinator binary: on Windows the build the product spawn
    // compiles into cache\coord (named by the build id), on WSL the sibling.
    std::string coord_binary;
#if defined(_WIN32)
    const std::filesystem::path root = std::filesystem::path(__FILE__)
                                           .parent_path()  // tests
                                           .parent_path()  // test_support
                                           .parent_path()  // src
                                           .parent_path(); // the worktree
    coord_binary = as_wsl_path(root / "cache" / "coord" /
                               ("anno_budget_coord-" + hex16(anno::test::coordinator_build())));
#else
    coord_binary = sibling("anno_budget_coord").generic_string();
#endif

    // Start the coordinator the way a client does (the product spawn): a WSL
    // coordinator, which starts the real PowerShell meter from WSL.
    {
        Vars vars = client_vars(kPortMeter, "f3-d-meter", "120", "1");
        vars[0].second = "wsl";
        Env env(vars);
        anno::test::BudgetHooks hooks;
        hooks.free_bytes = &anno::test::free_memory_bytes;
        auto budget = anno::test::Budget::file(budget_path, hooks, 0);
        anno::test::Admit info;
        auto hold = budget->reserve(1, &info);
        INFO("client through the product spawn: " << info.reason);
        REQUIRE(static_cast<bool>(hold));
    }
    const std::string coord_pid =
        token(wsl_capture("pgrep -f '[a]nno_budget_coord[^ ]* " + port + "' | head -1 || true"));
    INFO("coordinator pid: " << coord_pid);
    REQUIRE(!coord_pid.empty());
    const std::string meter_pid =
        token(wsl_capture("pgrep -P " + coord_pid + " | head -1 || true"));
    INFO("meter child pid: " << meter_pid);
    REQUIRE(!meter_pid.empty());

    // The socket inodes of the coordinator (its listener and clients) and of
    // the meter child. Inherited descriptors show as shared inodes.
    const std::string common_command =
        "ls -l /proc/" + coord_pid +
        "/fd 2>/dev/null | grep -o 'socket:[[][0-9]*[]]' | sort -u > /tmp/f3-coord.txt; "
        "ls -l /proc/" +
        meter_pid +
        "/fd 2>/dev/null | grep -o 'socket:[[][0-9]*[]]' | sort -u > /tmp/f3-meter.txt; "
        "comm -12 /tmp/f3-coord.txt /tmp/f3-meter.txt | wc -l; "
        "wc -l < /tmp/f3-coord.txt";
    const std::string counts = wsl_capture(common_command);
    std::istringstream numbers(counts);
    std::string shared;
    std::string coord_sockets;
    numbers >> shared >> coord_sockets;
    INFO("shared socket inodes: " << shared << ", coordinator socket inodes: " << coord_sockets
                                  << " (raw: " << counts << ")");
    CHECK(coord_sockets != "0"); // the coordinator really has a listener to compare
    CHECK(shared == "0");        // no descriptor of the coordinator is open in the meter child

    // Phase A, the handover. Kill only that coordinator: the port must be free
    // within 1 s (the poll includes one wsl.exe round trip), and a replacement
    // must bind on the first try (listening within 1 s of the kill).
    wsl_capture("kill -9 " + coord_pid + " || true");
    const auto killed = std::chrono::steady_clock::now();
    const bool freed = eventually(
        [&] { return token(wsl_capture("ss -tln | grep -c ':" + port + " ' || true")) == "0"; },
        1000ms);
    const auto freed_after = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - killed)
                                 .count();
    MESSAGE("port " << port << " free after the kill: " << freed << ", observed " << freed_after
                    << " ms (includes one wsl.exe round trip per poll)");
    CHECK(freed);
    // The relaunch and the wait share one WSL session: a background process
    // started by a session that ends at once does not survive it. The loop
    // prints 1 as soon as the port is listened on, 0 after 5 s (a replacement
    // that cannot bind exits at once and prints 0).
    const auto relaunch = std::chrono::steady_clock::now();
    const std::string listening_now =
        wsl_capture("nohup " + coord_binary + " " + port +
                    " any >/dev/null 2>&1 & "
                    "for i in 1 2 3 4 5 6 7 8 9 10; do sleep 0.5; "
                    "if ss -tln | grep -q ':" +
                    port + " '; then echo 1; exit 0; fi; done; echo 0");
    const auto bound_after = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - relaunch)
                                 .count();
    const bool bound = token(listening_now) == "1";
    MESSAGE("replacement listening: "
            << bound << ", observed " << bound_after
            << " ms after the relaunch (a replacement that cannot bind exits at once)");
    INFO("replacement listening within 5 s of the relaunch: " << listening_now);
    CHECK(bound);
    wsl_capture("pkill -f '[a]nno_budget_coord[^ ]* " + port + "' || true");

    // Phase B, the gap: no coordinator is listening on the port now. A client
    // that connects waits within its bound with a diagnostic and never fails
    // with "speaks another protocol".
    eventually(
        [&] { return token(wsl_capture("ss -tln | grep -c ':" + port + " ' || true")) == "0"; },
        500ms);
    std::string gap_reason;
    try {
        Env gap(client_vars(kPortMeter, "f3-d-meter", "2"));
        anno::test::BudgetHooks hooks;
        hooks.free_bytes = &anno::test::free_memory_bytes;
        auto budget = anno::test::Budget::file(budget_path, hooks, 0);
        anno::test::Admit info;
        auto hold = budget->reserve(1, &info);
        CHECK_FALSE(static_cast<bool>(hold));
        gap_reason = info.reason;
    } catch (const std::exception& error) {
        gap_reason = error.what();
    }
    INFO("gap client: " << gap_reason);
    CHECK_FALSE(gap_reason.empty());
    CHECK_FALSE(has(gap_reason, "speaks another protocol"));
}

TEST_CASE("F3 (D1, silent): a coordinator that accepts but answers no hello is retried within the "
          "admission bound, then served" *
          doctest::test_suite("budget")) {
    // A silent listener accepts the connection and never answers. The client
    // reports WAITING (it retried); the silent listener closes; the client's
    // coordinator starts on the port; the retry is served.
    Child silent = probe({"silent", std::to_string(kPortSilentOk)});
    std::string seen;
    const std::string silent_line = want(silent, "SILENT", 10000ms, &seen);
    INFO("silent output: " << seen);
    REQUIRE(has(silent_line, "SILENT 1"));
    const auto budget_path = (f3_dir() / "g.budget").string();
    Child client;
    {
        Env env(client_vars(kPortSilentOk, "f3-g-silent", "120"));
        Env liveness({{"ANNO_TEST_BUDGET_LIVENESS_MS", "300"}});
        client = probe({"reqwait", budget_path, "30"});
    }
    seen.clear();
    const std::string waiting = want(client, "WAITING", 20000ms, &seen);
    INFO("client output: " << seen);
    REQUIRE(has(waiting, "WAITING"));
    quit_and_wait(silent);
    const auto journal = f3_dir() / "g.journal";
    remove_file(journal);
    std::string listening;
    Child coord = start_coordinator(kPortSilentOk, journal, std::nullopt, std::nullopt, listening);
    const std::string result = want(client, "RESULT", 30000ms, &seen);
    INFO("client output: " << seen);
    CHECK(has(result, "RESULT 1"));
    client.send("q\n");
}

TEST_CASE("F3 (D1, silent): a coordinator that never answers ends at the admission deadline with a "
          "not-ready diagnostic" *
          doctest::test_suite("budget")) {
    Child silent = probe({"silent", std::to_string(kPortSilent)});
    std::string seen;
    const std::string silent_line = want(silent, "SILENT", 10000ms, &seen);
    INFO("silent output: " << seen);
    REQUIRE(has(silent_line, "SILENT 1"));
    const auto budget_path = (f3_dir() / "h.budget").string();
    Child client;
    {
        Env env(client_vars(kPortSilent, "f3-h-silent", "3"));
        Env liveness({{"ANNO_TEST_BUDGET_LIVENESS_MS", "300"}});
        client = probe({"reqwait", budget_path, "30"});
    }
    seen.clear();
    const std::string result = want(client, "RESULT", 30000ms, &seen);
    INFO("client output: " << seen);
    CHECK(has(result, "RESULT 0"));
    CHECK(has(result, "not ready"));
    CHECK_FALSE(has(result, "speaks another protocol"));
    silent.send("q\n");
}

TEST_CASE(
    "F3 (D1, pin): the coordinator reports its compatibility version 4, separately from its build" *
    doctest::test_suite("budget")) {
    // The compatibility version is pinned. A change of the wire format, or of the
    // reservation, journal, residency or recovery semantics, must change this
    // number on purpose (wire::kCompatVersion; docs/perf/README.md, T10c F4).
    const auto journal = f3_dir() / "pin.journal";
    remove_file(journal);
    std::string listening;
    Child coord = start_coordinator(kPortPin, journal, std::nullopt, std::nullopt, listening);
    CHECK(has(listening, " compat=4"));
    CHECK(has(listening, " build="));
    CHECK(has(listening, " incarnation="));
}
