// T10c F4 regressions (round cu).
//
//   E1: a successor never reuses a hold id that is in the journal it replayed,
//       so a journaled promise survives two restarts in both orders.
//   E2: one cancel() ends every waiter of a budget that is blocked on connect.
//   E3: each defence layer (exact-build hello, socket CLOEXEC, meter closefrom,
//       client-spawn closefrom) is observed on its own.
//
// Helpers are the F3 process harness (budget_proc_harness.hpp), unchanged.
// Ports 47820–47839; journals under temp_directory_path()/anno1503-f4.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#include "../src/coord_wire.hpp"
#include "budget_proc_harness.hpp"
#include "child_process.hpp"

namespace {

using anno::test::child::Child;
using namespace std::chrono_literals;

constexpr std::uint16_t kPortOrphanReuse = 47820; // E1, orphan kept
constexpr std::uint16_t kPortLiveHold = 47821;    // E1, live hold kept
constexpr std::uint16_t kPortCancelAll = 47822;   // E2
constexpr std::uint16_t kPortRawHello = 47823;    // N2, server exact-build
constexpr std::uint16_t kPortCloexec = 47824;     // N3, sockets CLOEXEC
constexpr std::uint16_t kPortMeterLeak = 47825;   // N4, meter closefrom
constexpr std::uint16_t kPortClientLeak = 47826;  // N4b, client spawn closefrom

std::filesystem::path f4_dir() {
    const auto dir = std::filesystem::temp_directory_path() / "anno1503-f4";
    std::filesystem::create_directories(dir);
    return dir;
}

std::string journal_text(const std::filesystem::path& journal) {
    std::ifstream in(journal);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

std::string wsl_coord_binary() {
#if defined(_WIN32)
    const auto root = std::filesystem::path(__FILE__)
                          .parent_path()  // tests
                          .parent_path()  // test_support
                          .parent_path()  // src
                          .parent_path(); // the worktree
    return as_wsl_path(root / "cache" / "coord" /
                       ("anno_budget_coord-" + hex16(anno::test::coordinator_build())));
#else
    return sibling("anno_budget_coord").generic_string();
#endif
}

#if defined(_WIN32)
std::string wsl_path(const std::filesystem::path& path) {
    return as_wsl_path(path);
}
#else
std::string wsl_path(const std::filesystem::path& path) {
    return path.generic_string();
}
#endif

/// Writes a POSIX script (no nested quotes) and runs it in WSL. F3's
/// wsl_capture wraps `sh -c "..."`, which breaks an inline `for` loop.
std::string wsl_script(const std::string& body, const std::string& arg = {}) {
    static std::atomic<int> serial{0};
    const auto path = f4_dir() / ("wsl-" + std::to_string(serial++) + ".sh");
    {
        std::ofstream out(path, std::ios::binary);
        out << "#!/bin/sh\n" << body;
        if (!body.empty() && body.back() != '\n') {
            out << '\n';
        }
    }
    // `sh script arg` so the file does not need the execute bit. On drvfs
    // Windows temp files are often executable; on a native Linux /tmp they
    // are not (T10c F4 WSL unmutated N3: Permission denied).
    std::string command = "sh " + wsl_path(path);
    if (!arg.empty()) {
        command += " " + arg;
    }
    return wsl_capture(command);
}

struct StopWslPort {
    std::string port;
    ~StopWslPort() {
        if (!port.empty()) {
            wsl_capture("pkill -f '[a]nno_budget_coord[^ ]* " + port + "' || true");
        }
    }
};

} // namespace

TEST_CASE("F4 (E1): an orphan survives a successor's reuse of its hold id and a second restart" *
          doctest::test_suite("budget")) {
    const auto journal = f4_dir() / "a.journal";
    const auto budget_path = (f4_dir() / "a.budget").string();
    remove_file(journal);
    std::string listening;
    Child c1 = start_coordinator(kPortOrphanReuse, journal, std::nullopt, std::nullopt, listening);
    const Vars vars = client_vars(kPortOrphanReuse, "f4-a", "30");

    std::string seen;
    Child holder_p;
    {
        Env env(vars);
        holder_p = probe({"holder", budget_path, "50"});
    }
    const std::string ready_p = want(holder_p, "READY", 15000ms, &seen);
    INFO("P: " << seen);
    REQUIRE(has(ready_p, "READY 1"));

    c1.stop(); // P stays alive; its session is lost; P owns an orphan.

    Child c2 = start_coordinator(kPortOrphanReuse, journal, std::nullopt, std::nullopt, listening);
    const std::string c2_listen = listening;
    INFO("C2 LISTENING: " << c2_listen);
    CHECK(has(c2_listen, "orphans=1"));

    Child holder_q;
    {
        Env env(vars);
        holder_q = probe({"holder", budget_path, "20"});
    }
    seen.clear();
    const std::string ready_q = want(holder_q, "READY", 15000ms, &seen);
    INFO("Q: " << seen);
    REQUIRE(has(ready_q, "READY 1"));
    holder_q.send("q\n");
    REQUIRE(eventually([&] { return !holder_q.running(); }, 10000ms));

    // Compaction rewrites only after 20000 appends (Journal::wants_compaction).
    // C2 has appended a handful of A/R lines, so the file still has the A lines
    // for 50 and 20. C3's open() therefore replays those lines, which is the
    // collision this case is for. Checked by reading the file, not by guessing.
    const std::string journal_before_c3 = journal_text(journal);
    const auto holds = open_hold_bytes(journal);
    INFO("journal before C3:\n" << journal_before_c3);
    REQUIRE(contains_value(holds, 50));
    REQUIRE(contains_value(holds, 20));

    c2.stop();
    Child c3 = start_coordinator(kPortOrphanReuse, journal, std::nullopt, std::nullopt, listening);
    const std::string c3_listen = listening;
    INFO("C3 LISTENING: " << c3_listen);
    CHECK(has(c3_listen, "orphans=1"));

    Child check;
    {
        Env env(vars);
        check = probe({"try", budget_path, "90"});
    }
    seen.clear();
    const std::string claim = want(check, "claim=", 15000ms, &seen);
    INFO("try 90: " << claim);
    CHECK(has(claim, "claim=0"));
    CHECK(has(claim, "pending=50"));

    holder_p.send("q\n");
    c3.stop();
}

TEST_CASE("F4 (E1): a successor's live hold survives the orphan owner's exit and a second restart" *
          doctest::test_suite("budget")) {
    const auto journal = f4_dir() / "b.journal";
    const auto budget_path = (f4_dir() / "b.budget").string();
    remove_file(journal);
    std::string listening;
    Child c1 = start_coordinator(kPortLiveHold, journal, std::nullopt, std::nullopt, listening);
    const Vars vars = client_vars(kPortLiveHold, "f4-b", "30");

    std::string seen;
    Child holder_p;
    {
        Env env(vars);
        holder_p = probe({"holder", budget_path, "50"});
    }
    REQUIRE(has(want(holder_p, "READY", 15000ms, &seen), "READY 1"));

    c1.stop();
    Child c2 = start_coordinator(kPortLiveHold, journal, std::nullopt, std::nullopt, listening);
    CHECK(has(listening, "orphans=1"));

    Child holder_q;
    {
        Env env(vars);
        holder_q = probe({"holder", budget_path, "20"});
    }
    seen.clear();
    REQUIRE(has(want(holder_q, "READY", 15000ms, &seen), "READY 1"));

    holder_p.send("q\n");
    // C2 checks owners every 250 ms. A try of 90 must see only Q's 20.
    bool dropped = false;
    std::string claim;
    const bool waited = eventually(
        [&] {
            Child check;
            {
                Env env(vars);
                check = probe({"try", budget_path, "90"});
            }
            claim = want(check, "claim=", 5000ms);
            dropped = has(claim, "pending=20");
            return dropped;
        },
        10000ms);
    INFO("try 90 against C2 after P exit: " << claim);
    REQUIRE(waited);
    REQUIRE(dropped);

    const std::string journal_before_c3 = journal_text(journal);
    const auto holds = open_hold_bytes(journal);
    INFO("journal before C3:\n" << journal_before_c3);
    REQUIRE(contains_value(holds, 50));
    REQUIRE(contains_value(holds, 20));

    c2.stop();
    Child c3 = start_coordinator(kPortLiveHold, journal, std::nullopt, std::nullopt, listening);
    const std::string c3_listen = listening;
    INFO("C3 LISTENING: " << c3_listen);
    CHECK(has(c3_listen, "orphans=1"));

    Child check;
    {
        Env env(vars);
        check = probe({"try", budget_path, "90"});
    }
    seen.clear();
    const std::string after = want(check, "claim=", 15000ms, &seen);
    INFO("try 90 against C3: " << after);
    CHECK(has(after, "claim=0"));
    CHECK(has(after, "pending=20"));

    holder_q.send("q\n");
    c3.stop();
}

TEST_CASE("F4 (E2): one cancel ends every waiter of a budget" * doctest::test_suite("budget")) {
    // Bound: one connection attempt (an immediate reject here) plus one pause
    // of at most ANNO_TEST_BUDGET_RECHECK_MS (100). 3 s is the margin. At
    // 94cf3cb the second thread does not end within the bound; the case fails
    // with the lines seen and the probe is killed with this Child.
    const auto journal = f4_dir() / "c.journal";
    const auto budget_path = (f4_dir() / "c.budget").string();
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortCancelAll, journal, 0xc1c1c1c1c1c1c1c1ULL, std::nullopt, listening);
    const Vars vars = client_vars(kPortCancelAll, "f4-c", "60");

    std::string seen;
    Child holder;
    {
        Env env(vars);
        holder = probe({"holder", budget_path, "50"});
    }
    REQUIRE(has(want(holder, "READY", 15000ms, &seen), "READY 1"));

    Child requester;
    {
        Env env(vars);
        Env exact({{"ANNO_TEST_BUDGET_EXACT_BUILD", "1"}});
        requester = probe({"reqwait2", budget_path, "30"});
    }
    seen.clear();
    const std::string waiting = want(requester, "WAITING", 15000ms, &seen);
    INFO("requester: " << seen);
    REQUIRE(has(waiting, "WAITING"));
    std::this_thread::sleep_for(300ms);
    requester.send("c\n");

    const auto deadline = std::chrono::steady_clock::now() + 3000ms;
    std::string first;
    std::string second;
    auto remain = [&] {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            return 0ms;
        }
        return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    };
    first = want(requester, "RESULT", remain(), &seen);
    second = want(requester, "RESULT", remain(), &seen);
    INFO("after cancel: " << seen);
    CHECK(has(first, "RESULT"));
    CHECK(has(second, "RESULT"));
    CHECK(has(seen, "RESULT 1 0"));
    CHECK(has(seen, "RESULT 2 0"));
    CHECK(has(seen, "cancelled"));
    CHECK(holder.running());
    CHECK(coord.running());
}

TEST_CASE("F4 (N2): the coordinator alone refuses an exact-build hello of another build" *
          doctest::test_suite("budget")) {
    const auto journal = f4_dir() / "d.journal";
    const auto budget_path = (f4_dir() / "d.budget").string();
    remove_file(journal);
    std::string listening;
    Child coord =
        start_coordinator(kPortRawHello, journal, 0xd1d1d1d1d1d1d1d1ULL, std::nullopt, listening);
    const Vars vars = client_vars(kPortRawHello, "f4-d", "30");

    std::string seen;
    Child holder;
    {
        Env env(vars);
        holder = probe({"holder", budget_path, "50"});
    }
    REQUIRE(has(want(holder, "READY", 15000ms, &seen), "READY 1"));

    const std::string compat = std::to_string(anno::test::wire::kCompatVersion);
    Child exact;
    {
        exact = probe({"rawhello", std::to_string(kPortRawHello), "4", "e2e2e2e2e2e2e2e2", compat});
    }
    seen.clear();
    const std::string reject = want(exact, "REPLY", 15000ms, &seen);
    INFO("exact hello: " << reject);
    CHECK(has(reject, "op=" + std::to_string(anno::test::wire::kStReject)));
    CHECK(has(reject, "reason=" + std::to_string(anno::test::wire::kRejectBuild)));

    Child control;
    {
        control =
            probe({"rawhello", std::to_string(kPortRawHello), "0", "e2e2e2e2e2e2e2e2", compat});
    }
    seen.clear();
    const std::string ok = want(control, "REPLY", 15000ms, &seen);
    INFO("control hello: " << ok);
    CHECK(has(ok, "op=" + std::to_string(anno::test::wire::kStOk)));

    holder.send("q\n");
    coord.stop();
}

TEST_CASE("F4 (N3): every socket of a running coordinator is close-on-exec" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available");
        return;
    }
    const std::string port = std::to_string(kPortCloexec);
    StopWslPort stop{port};
    const auto budget_path = (f4_dir() / "e.budget").string();
    Vars vars = client_vars(kPortCloexec, "f4-e", "120", "1");
    vars[0].second = "wsl";
    Env env(vars);
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &anno::test::free_memory_bytes;
    auto budget = anno::test::Budget::file(budget_path, hooks, 0);
    anno::test::Admit info;
    auto hold = budget->reserve(1, &info);
    INFO("product spawn: " << info.reason);
    REQUIRE(static_cast<bool>(hold));
    const std::string coord_pid =
        token(wsl_capture("pgrep -f '[a]nno_budget_coord[^ ]* " + port + "' | head -1 || true"));
    INFO("coordinator pid: " << coord_pid);
    REQUIRE(!coord_pid.empty());

    const std::string listing =
        wsl_script("pid=$1\n"
                   "for n in /proc/$pid/fd/[0-9]*; do\n"
                   "  b=$(basename \"$n\")\n"
                   "  t=$(readlink \"$n\" 2>/dev/null || true)\n"
                   "  f=$(awk '/^flags:/ {print $2}' /proc/$pid/fdinfo/$b 2>/dev/null || true)\n"
                   "  echo \"$b $t $f\"\n"
                   "done\n",
                   coord_pid);
    INFO("fd listing:\n" << listing);
    bool any_socket = false;
    bool all_sockets_cloexec = true;
    std::istringstream lines(listing);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream words(line);
        std::string fd;
        std::string target;
        std::string flags;
        words >> fd >> target >> flags;
        unsigned long value = 0;
        if (!flags.empty()) {
            try {
                value = std::stoul(flags, nullptr, 8);
            } catch (const std::exception&) {
                value = 0;
            }
        }
        const bool cloexec = (value & 02000000ul) != 0;
        if (target.rfind("socket:", 0) == 0) {
            any_socket = true;
            if (!cloexec) {
                all_sockets_cloexec = false;
            }
        } else {
            try {
                if (std::stoul(fd) >= 3) {
                    MESSAGE("non-socket fd " << fd << " -> " << target << " flags=" << flags
                                             << " cloexec=" << cloexec);
                }
            } catch (const std::exception&) {
            }
        }
    }
    CHECK(any_socket);
    CHECK(all_sockets_cloexec);
}

TEST_CASE("F4 (N4): a descriptor the coordinator holds without close-on-exec does not reach its "
          "meter child" *
          doctest::test_suite("budget")) {
    if (!wsl_meter_available()) {
        MESSAGE("skipping: the real PowerShell meter is not available (it runs only from WSL "
                "with the Windows PowerShell reachable)");
        return;
    }
    const std::string port = std::to_string(kPortMeterLeak);
    StopWslPort stop{port};
    const auto leak = f4_dir() / "n4.leak";
    {
        std::ofstream out(leak);
        out << "n4\n";
    }
    const std::string leak_wsl = wsl_path(leak);
    const std::string coord = wsl_coord_binary();
    const auto journal = f4_dir() / "n4.journal";
    remove_file(journal);
    const std::string journal_wsl = wsl_path(journal);
    wsl_capture("pkill -f '[a]nno_budget_coord[^ ]* " + port + "' || true");
    const std::string started =
        wsl_capture("ANNO_TEST_COORD_LEAK_FD=" + leak_wsl + " nohup " + coord + " " + port +
                    " any --journal " + journal_wsl +
                    " >/tmp/f4-n4-coord.log 2>&1 & "
                    "for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do sleep 0.25; "
                    "if pgrep -f '[a]nno_budget_coord[^ ]* " +
                    port + " ' >/dev/null; then echo STARTED; exit 0; fi; done; echo FAILED");
    INFO("start: " << started << " log: " << wsl_capture("cat /tmp/f4-n4-coord.log || true"));
    REQUIRE(token(started) == "STARTED");

    const std::string coord_pid =
        token(wsl_capture("pgrep -f '[a]nno_budget_coord[^ ]* " + port + "' | head -1 || true"));
    REQUIRE(!coord_pid.empty());

    const std::string coord_has = token(wsl_capture(
        "ls -l /proc/" + coord_pid + "/fd 2>/dev/null | grep -F '" + leak_wsl + "' | wc -l"));
    INFO("coordinator links to leak path: " << coord_has);
    REQUIRE(coord_has != "0");

    const std::string meter_pid =
        token(wsl_capture("pgrep -P " + coord_pid + " | head -1 || true"));
    INFO("meter pid: " << meter_pid);
    REQUIRE(!meter_pid.empty());
    const std::string meter_has = token(wsl_capture(
        "ls -l /proc/" + meter_pid + "/fd 2>/dev/null | grep -F '" + leak_wsl + "' | wc -l"));
    INFO("meter links to leak path: " << meter_has);
    CHECK(meter_has == "0");
}

TEST_CASE("F4 (N4b): a descriptor the client holds without close-on-exec does not reach the "
          "coordinator it spawns" *
          doctest::test_suite("budget")) {
#if defined(_WIN32)
    MESSAGE("skipping on Windows: bInheritHandles = FALSE is at coord.cpp:142 "
            "(CreateProcessW(..., FALSE, ...)); POSIX spawn closefrom is the Linux/WSL path");
    return;
#elif !defined(__linux__)
    MESSAGE("skipping: the descriptor check reads /proc/<pid>/fd, which only Linux has");
    return;
#else
    const auto leak = f4_dir() / "n4b.leak";
    {
        std::ofstream out(leak);
        out << "n4b\n";
    }
    const auto budget_path = (f4_dir() / "g.budget").string();
    const std::string port = std::to_string(kPortClientLeak);
    wsl_capture("pkill -f '[a]nno_budget_coord[^ ]* " + port + "' || true");
    Child spawned;
    {
        Env env(client_vars(kPortClientLeak, "f4-g", "5", "1"));
        Env leak_env({{"ANNO_TEST_PROBE_LEAK_FD", leak.string()}});
        spawned = probe({"try", budget_path, "1"});
    }
    (void)want(spawned, "claim=", 15000ms);
    const std::string coord_pid = token(wsl_capture("pgrep -af 'anno_budget_coord.* " + port +
                                                    "' | awk '{print $1}' | head -1 || true"));
    INFO("spawned coordinator pid: " << coord_pid);
    REQUIRE(!coord_pid.empty());
    const std::string coord_has = token(wsl_capture(
        "ls -l /proc/" + coord_pid + "/fd 2>/dev/null | grep -F '" + leak.string() + "' | wc -l"));
    INFO("coordinator links to leak path: " << coord_has);
    CHECK(coord_has == "0");
    wsl_capture("kill -9 " + coord_pid + " || true");
    wsl_capture("pkill -f '[a]nno_budget_coord[^ ]* " + port + "' || true");
#endif
}
