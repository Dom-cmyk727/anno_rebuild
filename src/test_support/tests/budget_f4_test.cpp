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
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <latch>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#include "../src/coord_identity.hpp"
#include "../src/coord_wire.hpp"
#include "budget_proc_harness.hpp"
#include "child_process.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

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
constexpr std::uint16_t kPortCompanion = 47827;   // F5, compatible peer
constexpr std::uint16_t kPortCold = 47829;        // F5 cold N4
constexpr std::uint16_t kPortWarm = 47830;        // F5 warm N4
constexpr std::uint16_t kPortCompanionN4 = 47831; // F5 companion's N4
constexpr std::uint16_t kPortFailBind = 47832;    // F5 bind-failure diagnostic
constexpr std::uint16_t kPortConcurrent = 47833;  // F5 concurrent usable start
constexpr std::uint16_t kPortFailMissing = 47834; // F5 missing-executable

std::string process_scratch_token() {
    static const std::string token = [] {
#if defined(_WIN32)
        const auto pid = static_cast<unsigned long long>(GetCurrentProcessId());
#else
        const auto pid = static_cast<unsigned long long>(::getpid());
#endif
        const auto stamp = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        return std::to_string(pid) + "-" + std::to_string(stamp);
    }();
    return token;
}

std::filesystem::path f4_dir() {
    static const auto dir =
        std::filesystem::temp_directory_path() / ("anno1503-f4-" + process_scratch_token());
    std::filesystem::create_directories(dir);
    return dir;
}

std::string journal_text(const std::filesystem::path& journal) {
    std::ifstream in(journal);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
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

#if defined(_WIN32)
std::filesystem::path wsl_executable() {
    if (const char* root = std::getenv("SystemRoot")) {
        if (root[0] != '\0') {
            return std::filesystem::path(root) / "System32" / "wsl.exe";
        }
    }
    return std::filesystem::path("C:\\Windows\\System32\\wsl.exe");
}
#endif

struct OwnedCoord {
    Child child;
    std::string listening;
    std::string linux_pid;
    std::string linux_start;
    std::string diagnostic;
    bool ready = false;

    OwnedCoord() = default;
    OwnedCoord(const OwnedCoord&) = delete;
    OwnedCoord& operator=(const OwnedCoord&) = delete;
    OwnedCoord(OwnedCoord&& other) noexcept { take(other); }
    OwnedCoord& operator=(OwnedCoord&& other) noexcept {
        if (this != &other) {
            stop_owned();
            take(other);
        }
        return *this;
    }
    ~OwnedCoord() { stop_owned(); }

    void stop_owned() {
        if (!linux_pid.empty()) {
            // Kill only while the starttime still matches. A recycled pid is not ours.
            const std::string live = token(
                wsl_capture("sed -n 's/.*) //p' /proc/" + linux_pid + "/stat | cut -d' ' -f20"));
            if (linux_start.empty() || live == linux_start) {
                wsl_capture("kill -9 " + linux_pid + " || true");
            }
            linux_pid.clear();
            linux_start.clear();
        }
        child.stop();
    }

  private:
    void take(OwnedCoord& other) noexcept {
        child = std::move(other.child);
        listening = std::move(other.listening);
        linux_pid = std::move(other.linux_pid);
        linux_start = std::move(other.linux_start);
        diagnostic = std::move(other.diagnostic);
        ready = other.ready;
        other.ready = false;
        other.linux_pid.clear();
    }
};

OwnedCoord start_owned_host_coordinator(std::uint16_t port, const std::filesystem::path& journal,
                                        const std::string& leak_path = {},
                                        const std::filesystem::path& binary_override = {}) {
    OwnedCoord owned;
    const auto binary =
        binary_override.empty() ? anno::test::host_coordinator_binary() : binary_override;
    std::error_code error;
    if (!std::filesystem::exists(binary, error) || std::filesystem::file_size(binary, error) == 0) {
        owned.diagnostic = "missing executable: " + binary.generic_string();
        return owned;
    }
#if defined(_WIN32)
    static std::atomic<int> owned_serial{0};
    const auto script = f4_dir() / ("owned-" + std::to_string(owned_serial++) + ".sh");
    {
        std::ofstream out(script, std::ios::binary);
        out << "#!/bin/sh\n"
               "echo PID:$$\n"
               "if [ -n \"$1\" ]; then export ANNO_TEST_COORD_LEAK_FD=\"$1\"; fi\n"
               "exec \"$2\" \"$3\" any --journal \"$4\"\n";
    }
    const auto wsl = wsl_executable();
    if (!std::filesystem::exists(wsl, error)) {
        owned.diagnostic = "missing wsl.exe";
        return owned;
    }
    owned.child =
        Child::start(wsl, {"-d", "Ubuntu", "--", "sh", as_wsl_path(script), leak_path,
                           as_wsl_path(binary), std::to_string(port), as_wsl_path(journal)});
#else
    std::optional<Env> leak_env;
    if (!leak_path.empty()) {
        leak_env.emplace(Vars{{"ANNO_TEST_COORD_LEAK_FD", leak_path}});
    }
    owned.child =
        Child::start(binary, {std::to_string(port), "any", "--journal", journal.string()});
    (void)leak_env;
#endif
    if (!owned.child) {
        owned.diagnostic = "child exit: spawn failed";
        return owned;
    }
    std::string seen;
    const auto deadline = std::chrono::steady_clock::now() + 15000ms;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        const std::string line = owned.child.line(remain);
        if (line.empty()) {
            break;
        }
        seen += line + " | ";
        if (line.rfind("PID:", 0) == 0) {
            owned.linux_pid = line.substr(4);
            owned.linux_start = token(wsl_capture("sed -n 's/.*) //p' /proc/" + owned.linux_pid +
                                                  "/stat | cut -d' ' -f20"));
            continue;
        }
        if (line.rfind("LISTENING", 0) == 0) {
            owned.listening = line;
            break;
        }
        if (line.rfind("BIND-FAILED", 0) == 0 || line.rfind("JOURNAL-FAILED", 0) == 0) {
            owned.diagnostic = line;
            return owned;
        }
    }
#if !defined(_WIN32)
    if (owned.linux_pid.empty() && owned.child.pid() > 0) {
        owned.linux_pid = std::to_string(owned.child.pid());
    }
    if (owned.linux_start.empty() && !owned.linux_pid.empty()) {
        owned.linux_start = token(
            wsl_capture("sed -n 's/.*) //p' /proc/" + owned.linux_pid + "/stat | cut -d' ' -f20"));
    }
#endif
    if (owned.listening.empty()) {
        owned.diagnostic = "not ready: " + seen;
        if (!owned.child.running()) {
            owned.diagnostic += " (child exit)";
        }
        return owned;
    }
    const anno::test::ListeningIdentity identity = anno::test::coordinator_listening_identity(
        owned.listening, port, anno::test::coordinator_build(), anno::test::wire::kCompatVersion,
        coordinator_config_name());
    if (!identity.matches) {
        owned.diagnostic = identity.diagnostic.empty() ? std::string("listening identity rejected")
                                                       : identity.diagnostic;
        return owned;
    }
    if (!owned.child.running()) {
        owned.diagnostic = "listening line but process has exited";
        return owned;
    }
    if (!anno::test::wire::local_port_accepts(port, 2000ms)) {
        owned.diagnostic = "listening line but no listener";
        return owned;
    }
    owned.ready = true;
    return owned;
}

/// Serializes cases that start this worktree's canonical cache binary so a
/// parallel CTest run cannot prepare it under another case. Cold F6 cases use
/// a private root and do not take this lock.
struct CoordArtifactLock {
    bool acquired = false;
#if defined(_WIN32)
    HANDLE mutex = nullptr;
    explicit CoordArtifactLock() {
        mutex = CreateMutexW(nullptr, FALSE, L"Local\\anno1503-f5-coord-artifact");
        if (mutex == nullptr) {
            return;
        }
        const DWORD wait = WaitForSingleObject(mutex, 180000);
        acquired = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        if (!acquired) {
            CloseHandle(mutex);
            mutex = nullptr;
        }
    }
    ~CoordArtifactLock() {
        if (acquired && mutex != nullptr) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
    }
#else
    int fd = -1;
    explicit CoordArtifactLock() {
        fd = ::open("/tmp/anno1503-f5-coord-artifact.lock", O_CREAT | O_RDWR | O_CLOEXEC, 0644);
        if (fd < 0) {
            return;
        }
        const auto deadline = std::chrono::steady_clock::now() + 180000ms;
        while (std::chrono::steady_clock::now() < deadline) {
            if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
                acquired = true;
                return;
            }
            std::this_thread::sleep_for(20ms);
        }
        ::close(fd);
        fd = -1;
    }
    ~CoordArtifactLock() {
        if (fd >= 0) {
            if (acquired) {
                ::flock(fd, LOCK_UN);
            }
            ::close(fd);
        }
    }
#endif
    CoordArtifactLock(const CoordArtifactLock&) = delete;
    CoordArtifactLock& operator=(const CoordArtifactLock&) = delete;
};

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

std::filesystem::path f6_dir();

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

void run_n4_meter_descriptor_case(std::uint16_t port) {
    const auto leak = f4_dir() / ("n4-" + std::to_string(port) + ".leak");
    {
        std::ofstream out(leak);
        out << "n4\n";
    }
    const std::string leak_wsl = wsl_path(leak);
    const auto journal = f4_dir() / ("n4-" + std::to_string(port) + ".journal");
    remove_file(journal);

    const anno::test::CoordinatorPrepare prep = anno::test::prepare_host_coordinator();
    INFO("prepare: ok=" << prep.ok << " exit=" << prep.compiler_exit << " " << prep.diagnostic
                        << " binary=" << prep.binary.generic_string());
    REQUIRE(prep.ok);

    OwnedCoord coord = start_owned_host_coordinator(port, journal, leak_wsl);
    INFO("start: ready=" << coord.ready << " pid=" << coord.linux_pid
                         << " listening=" << coord.listening << " diag=" << coord.diagnostic);
    REQUIRE(coord.ready);
    REQUIRE(!coord.linux_pid.empty());
    REQUIRE(has(coord.listening, "LISTENING"));
    CHECK(has(coord.listening, std::to_string(port)));
    CHECK(has(coord.listening, "build="));
    CHECK(has(coord.listening, "compat="));

    const std::string coord_has = token(wsl_capture(
        "ls -l /proc/" + coord.linux_pid + "/fd 2>/dev/null | grep -F '" + leak_wsl + "' | wc -l"));
    INFO("coordinator links to leak path: " << coord_has);
    REQUIRE(coord_has != "0");

    const std::string meter_pid =
        token(wsl_capture("pgrep -P " + coord.linux_pid + " | head -1 || true"));
    INFO("meter pid: " << meter_pid);
    REQUIRE(!meter_pid.empty());
    const std::string meter_has = token(wsl_capture(
        "ls -l /proc/" + meter_pid + "/fd 2>/dev/null | grep -F '" + leak_wsl + "' | wc -l"));
    INFO("meter links to leak path: " << meter_has);
    CHECK(meter_has == "0");
}

TEST_CASE("F4 (N4): a descriptor the coordinator holds without close-on-exec does not reach its "
          "meter child" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    CoordArtifactLock lock;
    REQUIRE(lock.acquired);
    run_n4_meter_descriptor_case(kPortMeterLeak);
}

TEST_CASE("F5 (cold): N4 prepares this worktree's WSL binary when it is absent" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    const auto root = f6_dir() / "cold-n4";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Env prepare_root({{"ANNO_TEST_COORD_PREPARE_ROOT", root.string()}});
    run_n4_meter_descriptor_case(kPortCold);
}

TEST_CASE("F5 (warm): N4 reuses a completed artifact" * doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    CoordArtifactLock lock;
    REQUIRE(lock.acquired);
    const anno::test::CoordinatorPrepare first = anno::test::prepare_host_coordinator();
    INFO("first prepare: ok=" << first.ok << " " << first.diagnostic);
    REQUIRE(first.ok);
    REQUIRE(std::filesystem::exists(first.binary));
    const anno::test::CoordinatorPrepare again = anno::test::prepare_host_coordinator();
    REQUIRE(again.ok);
    CHECK(again.compiler_exit == 0);
    run_n4_meter_descriptor_case(kPortWarm);
}

TEST_CASE("F5 (companion): cold N4 leaves a compatible peer's pid, incarnation and hold" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    CoordArtifactLock lock;
    REQUIRE(lock.acquired);
    const anno::test::CoordinatorPrepare prep = anno::test::prepare_host_coordinator();
    REQUIRE(prep.ok);
    const auto peer_journal = f4_dir() / "f5-peer.journal";
    const auto budget_path = (f4_dir() / "f5-peer.budget").string();
    remove_file(peer_journal);
    OwnedCoord peer = start_owned_host_coordinator(kPortCompanion, peer_journal);
    INFO("peer: " << peer.listening << " diag=" << peer.diagnostic);
    REQUIRE(peer.ready);
    const std::string peer_pid = peer.linux_pid;
    const std::string peer_line = peer.listening;
    Vars vars = client_vars(kPortCompanion, "f5-peer", "30");
    vars[0].second = "wsl";
    std::string seen;
    Child holder;
    {
        Env env(vars);
        holder = probe({"holder", budget_path, "40"});
    }
    REQUIRE(has(want(holder, "READY", 15000ms, &seen), "READY 1"));
    const auto cold_root = f6_dir() / "companion-cold";
    std::filesystem::remove_all(cold_root);
    std::filesystem::create_directories(cold_root);
    {
        Env prepare_root({{"ANNO_TEST_COORD_PREPARE_ROOT", cold_root.string()}});
        run_n4_meter_descriptor_case(kPortCompanionN4);
    }
    CHECK(peer.child.running());
    const std::string live_start =
        token(wsl_capture("sed -n 's/.*) //p' /proc/" + peer_pid + "/stat | cut -d' ' -f20"));
    const std::string live_cmd = wsl_capture("tr '\\0' ' ' < /proc/" + peer_pid + "/cmdline");
    INFO("live start=" << live_start << " cmd=" << live_cmd);
    CHECK(live_start == peer.linux_start);
    CHECK(has(live_cmd, "anno_budget_coord"));
    CHECK(peer.linux_pid == peer_pid);
    CHECK(peer.listening == peer_line);
    Child check;
    {
        Env env(vars);
        check = probe({"try", budget_path, "90"});
    }
    seen.clear();
    const std::string claim = want(check, "claim=", 15000ms, &seen);
    INFO("peer try 90: " << claim);
    CHECK(has(claim, "pending=40"));
    holder.send("q\n");
}

TEST_CASE("F5 (fail): missing executable is not ready" * doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    const auto root = f6_dir() / "missing";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Env prepare_root({{"ANNO_TEST_COORD_PREPARE_ROOT", root.string()}});
    const auto journal = f4_dir() / "f5-miss.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(kPortFailMissing, journal);
    INFO("diag=" << coord.diagnostic << " listening=" << coord.listening);
    CHECK(!coord.ready);
    CHECK(has(coord.diagnostic, "missing executable"));
    CHECK(!has(coord.diagnostic, "STARTED"));
    CHECK(!has(coord.listening, "LISTENING"));
    CHECK(!has(coord.listening, "STARTED"));
}

TEST_CASE("F5 (fail): forced prepare failure is not success" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "prepare-fail";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Env fail(
        {{"ANNO_TEST_COORD_PREPARE_FAIL", "1"}, {"ANNO_TEST_COORD_PREPARE_ROOT", root.string()}});
    const anno::test::CoordinatorPrepare prep = anno::test::prepare_host_coordinator();
    INFO("prepare: ok=" << prep.ok << " " << prep.diagnostic);
    CHECK(!prep.ok);
    CHECK(has(prep.diagnostic, "ANNO_TEST_COORD_PREPARE_FAIL"));
    CHECK(!std::filesystem::exists(prep.binary));
}

TEST_CASE("F5 (fail): bind failure is not ready" * doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available");
        return;
    }
    CoordArtifactLock lock;
    REQUIRE(lock.acquired);
    const anno::test::CoordinatorPrepare prep = anno::test::prepare_host_coordinator();
    REQUIRE(prep.ok);
    const auto journal_a = f4_dir() / "f5-bind-a.journal";
    const auto journal_b = f4_dir() / "f5-bind-b.journal";
    remove_file(journal_a);
    remove_file(journal_b);
    OwnedCoord first = start_owned_host_coordinator(kPortFailBind, journal_a);
    REQUIRE(first.ready);
    OwnedCoord second = start_owned_host_coordinator(kPortFailBind, journal_b);
    INFO("second: ready=" << second.ready << " diag=" << second.diagnostic
                          << " listening=" << second.listening);
    CHECK(!second.ready);
    CHECK(has(second.diagnostic, "BIND-FAILED"));
    CHECK(!has(second.diagnostic, "STARTED"));
    CHECK(!second.ready);
}

TEST_CASE("F5 (concurrent): two preparers contend for the same missing artifact" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; Windows preparation compiles in WSL");
        return;
    }
    const auto root = f6_dir() / "concurrent";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Env prepare_root({{"ANNO_TEST_COORD_PREPARE_ROOT", root.string()}});
    anno::test::CoordinatorPrepare first;
    anno::test::CoordinatorPrepare second;
    std::latch go(2);
    std::thread a([&] {
        go.arrive_and_wait();
        first = anno::test::prepare_host_coordinator();
    });
    std::thread b([&] {
        go.arrive_and_wait();
        second = anno::test::prepare_host_coordinator();
    });
    a.join();
    b.join();
    INFO("first: ok=" << first.ok << " " << first.diagnostic);
    INFO("second: ok=" << second.ok << " " << second.diagnostic);
    REQUIRE(first.ok);
    REQUIRE(second.ok);
    REQUIRE(std::filesystem::exists(first.binary));
    CHECK(std::filesystem::file_size(first.binary) > 0);
    std::ifstream in(first.binary, std::ios::binary);
    char magic[4]{};
    in.read(magic, 4);
    CHECK(magic[0] == '\x7f');
    CHECK(magic[1] == 'E');
    CHECK(magic[2] == 'L');
    CHECK(magic[3] == 'F');
    const auto journal = f4_dir() / "f5-conc.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(kPortConcurrent, journal);
    INFO("usable: " << coord.listening << " " << coord.diagnostic);
    REQUIRE(coord.ready);
}

std::filesystem::path f6_dir() {
    static const auto dir =
        std::filesystem::temp_directory_path() / ("anno1503-f6-" + process_scratch_token());
    std::filesystem::create_directories(dir);
    return dir;
}

std::filesystem::path self_exe() {
#if defined(_WIN32)
    wchar_t buffer[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, 32768);
    if (n == 0 || n >= 32768) {
        return {};
    }
    return std::filesystem::path(buffer);
#else
    char buffer[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        return {};
    }
    buffer[n] = '\0';
    return std::filesystem::path(buffer);
#endif
}

std::filesystem::path write_f6_script(const std::string& name, const std::string& body) {
    const auto path = f6_dir() / name;
    std::ofstream out(path, std::ios::binary);
    out << "#!/bin/sh\n" << body;
    if (!body.empty() && body.back() != '\n') {
        out << '\n';
    }
    out.close();
#if !defined(_WIN32)
    std::filesystem::permissions(path, std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::add);
#endif
    return path;
}

/// One compiler stand-in. The mode is the file name (`fake-hang.sh`, ...).
/// It records argv and, on the success modes, writes an ELF-prefixed marker
/// taken from `-DANNO_COORD_MARKER_LITERAL=`.
std::filesystem::path f6_fake_compiler(const std::string& name) {
    static const char* body =
        "out=\n"
        "prev=\n"
        "marker=\n"
        "config=\n"
        "build=\n"
        "for a in \"$@\"; do\n"
        "  if [ \"$prev\" = \"-o\" ]; then out=$a; fi\n"
        "  if [ \"$prev\" = \"-include\" ]; then\n"
        "    marker=$(sed -n 's/^#define ANNO_COORD_MARKER_LITERAL \"\\(.*\\)\"/\\1/p' \"$a\")\n"
        "    config=$(sed -n 's/^#define ANNO_COORD_CONFIG_LITERAL \"\\(.*\\)\"/\\1/p' \"$a\")\n"
        "    build=$(sed -n 's/^#define ANNO_COORD_BUILD 0x\\([0-9a-fA-F]*\\)ULL/\\1/p' \"$a\")\n"
        "  fi\n"
        "  case \"$a\" in\n"
        "    -DANNO_COORD_MARKER_LITERAL=*)\n"
        "      marker=${a#-DANNO_COORD_MARKER_LITERAL=}\n"
        "      marker=${marker#\\\"}\n"
        "      marker=${marker%\\\"}\n"
        "      ;;\n"
        "  esac\n"
        "  prev=$a\n"
        "done\n"
        "base=$(basename \"$0\")\n"
        "if [ -n \"$out\" ]; then printf '%s\\n' \"$@\" > \"${out}.argv\"; fi\n"
        "payload=$marker\n"
        "if [ \"$config\" = \"asan+ubsan\" ]; then payload=\"${payload}__asan_init\"; fi\n"
        "emit_identity() {\n"
        "  src=\"${out}.c\"\n"
        "  cat > \"$src\" << 'END'\n"
        "#include <stdio.h>\n"
        "#include <string.h>\n"
        "int main(int argc, char** argv) {\n"
        "  if (argc > 1 && strcmp(argv[1], \"--identity\") == 0) {\n"
        "    printf(\"COORD-IDENTITY build=%s config=%s marker=%s\\n\", \"BUILDTOKEN\", "
        "\"CONFIGTOKEN\", \"MARKERTOKEN\");\n"
        "    return 0;\n"
        "  }\n"
        "  return 2;\n"
        "}\n"
        "END\n"
        "  sed -i \"s|BUILDTOKEN|$build|g; s|CONFIGTOKEN|$config|g; s|MARKERTOKEN|$marker|g\" "
        "\"$src\"\n"
        "  g++ -O0 -o \"$out\" \"$src\"\n"
        "  rc=$?\n"
        "  rm -f \"$src\"\n"
        "  return $rc\n"
        "}\n"
        "case \"$base\" in\n"
        "  fake-partial.sh)\n"
        "    printf 'PARTIAL-OUTPUT-NOT-AN-ARTIFACT' > \"$out\"\n"
        "    echo partial-stdout\n"
        "    exit 1\n"
        "    ;;\n"
        "  fake-bad.sh)\n"
        "    printf 'hello-not-elf' > \"$out\"\n"
        "    exit 0\n"
        "    ;;\n"
        "  fake-hang.sh)\n"
        "    echo started-hang\n"
        "    sleep 60\n"
        "    exit 0\n"
        "    ;;\n"
        "  fake-hangchild.sh)\n"
        "    echo started-child\n"
        "    sleep 60 &\n"
        "    wait\n"
        "    ;;\n"
        "  fake-slow.sh)\n"
        "    printf 'PARTIAL' > \"$out\"\n"
        "    sleep 2\n"
        "    emit_identity || exit 1\n"
        "    exit 0\n"
        "    ;;\n"
        "  fake-delay.sh)\n"
        "    sleep 1\n"
        "    emit_identity || exit 1\n"
        "    exit 0\n"
        "    ;;\n"
        "  fake-prefix.sh)\n"
        "    printf '\\177ELF%s' \"$payload\" > \"$out\"\n"
        "    exit 0\n"
        "    ;;\n"
        "  fake-closefd.sh)\n"
        "    exec 1>&- 2>&-\n"
        "    sleep 0.2\n"
        "    exit 0\n"
        "    ;;\n"
        "  fake-big.sh)\n"
        "    emit_identity || exit 1\n"
        "    dd if=/dev/zero bs=1024 count=2048 >> \"$out\" 2>/dev/null\n"
        "    exit 0\n"
        "    ;;\n"
        "  *)\n"
        "    emit_identity || exit 1\n"
        "    exit 0\n"
        "    ;;\n"
        "esac\n";
    return write_f6_script(name, body);
}

std::string one_line(std::string text) {
    for (char& ch : text) {
        if (ch == '\n' || ch == '\r') {
            ch = ' ';
        }
    }
    return text;
}

struct PrepareChild {
    bool finished = false;
    bool ok = false;
    int compiler_exit = 0;
    bool reused = false;
    bool timed_out = false;
    std::string binary;
    std::string diagnostic;
};

PrepareChild run_prepare_child(const Vars& extra, std::chrono::milliseconds wall) {
    PrepareChild result;
    Vars vars = extra;
    vars.push_back({"ANNO_TEST_F6_CHILD", "1"});
    Child child;
    {
        Env env(vars);
        child = Child::start(self_exe(),
                             {"--test-case=*F6 helper prepare*", "--no-intro", "--no-version"});
    }
    if (!child) {
        result.diagnostic = "prepare child failed to start";
        return result;
    }
    const auto deadline = std::chrono::steady_clock::now() + wall;
    std::string result_line;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto remain = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        const std::string line = child.line(remain);
        if (line.rfind("RESULT ", 0) == 0) {
            result_line = line;
        } else if (line.rfind("BINARY ", 0) == 0) {
            result.binary = line.substr(7);
        } else if (line.rfind("DIAG ", 0) == 0) {
            result.diagnostic = line.substr(5);
        } else if (line.empty()) {
            break;
        }
        if (!result_line.empty() && !result.diagnostic.empty()) {
            break;
        }
    }
    if (result_line.empty()) {
        child.stop();
        result.finished = false;
        if (result.diagnostic.empty()) {
            result.diagnostic = "outer wall timeout";
        }
        return result;
    }
    int ok_bit = 0;
    int reused_bit = 0;
    int timeout_bit = 0;
    if (std::sscanf(result_line.c_str(), "RESULT ok=%d exit=%d reused=%d timeout=%d", &ok_bit,
                    &result.compiler_exit, &reused_bit, &timeout_bit) >= 2) {
        result.ok = ok_bit != 0;
        result.reused = reused_bit != 0;
        result.timed_out = timeout_bit != 0;
    }
    result.finished = true;
    child.stop();
    return result;
}

Vars f6_prepare_env(const std::filesystem::path& root, const std::filesystem::path& compiler,
                    const char* deadline_ms) {
    return {{"ANNO_TEST_COORD_PREPARE_ROOT", root.string()},
            {"ANNO_TEST_COORD_COMPILER", compiler.string()},
            {"ANNO_TEST_COORD_PREPARE_DEADLINE_MS", deadline_ms}};
}

std::filesystem::path f6_artifact(const std::filesystem::path& root) {
    char build[17];
    std::snprintf(build, sizeof(build), "%016llx",
                  static_cast<unsigned long long>(anno::test::coordinator_build()));
    return root / (std::string("anno_budget_coord-") + build);
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::string data;
    if (!in) {
        return data;
    }
    char buffer[4096];
    while (in.read(buffer, static_cast<std::streamsize>(sizeof(buffer))) || in.gcount() > 0) {
        const auto got = static_cast<std::size_t>(in.gcount());
        if (got == 0) {
            break;
        }
        data.append(buffer, got);
    }
    return data;
}

bool any_building(const std::filesystem::path& root) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
        if (entry.path().filename().string().find(".building.") != std::string::npos) {
            return true;
        }
    }
    return false;
}

TEST_CASE("F6 helper prepare" * doctest::test_suite("budget")) {
    if (std::getenv("ANNO_TEST_F6_CHILD") == nullptr) {
        return;
    }
    const anno::test::CoordinatorPrepare prep = anno::test::prepare_host_coordinator();
    std::printf("RESULT ok=%d exit=%d reused=%d timeout=%d\n", prep.ok ? 1 : 0, prep.compiler_exit,
                prep.reused ? 1 : 0, prep.timed_out ? 1 : 0);
    std::printf("BINARY %s\n", prep.binary.generic_string().c_str());
    std::printf("DIAG %s\n", one_line(prep.diagnostic).c_str());
    std::fflush(stdout);
}

TEST_CASE("F6 helper hold build lock" * doctest::test_suite("budget")) {
    const char* path = std::getenv("ANNO_TEST_F6_HOLD_LOCK");
    if (path == nullptr || path[0] == '\0') {
        return;
    }
#if defined(_WIN32)
    const HANDLE file =
        CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::printf("HOLD-FAILED\n");
        std::fflush(stdout);
        return;
    }
    OVERLAPPED overlapped{};
    if (LockFileEx(file, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped) == 0) {
        std::printf("HOLD-FAILED\n");
        std::fflush(stdout);
        CloseHandle(file);
        return;
    }
    std::printf("HELD\n");
    std::fflush(stdout);
    std::this_thread::sleep_for(30s);
    UnlockFileEx(file, 0, 1, 0, &overlapped);
    CloseHandle(file);
#else
    const int fd = ::open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0644);
    if (fd < 0 || ::flock(fd, LOCK_EX) != 0) {
        std::printf("HOLD-FAILED\n");
        std::fflush(stdout);
        if (fd >= 0) {
            ::close(fd);
        }
        return;
    }
    std::printf("HELD\n");
    std::fflush(stdout);
    std::this_thread::sleep_for(30s);
    ::flock(fd, LOCK_UN);
    ::close(fd);
#endif
}

TEST_CASE("F6 (fail): hung compiler" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "hang";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto compiler = f6_fake_compiler("fake-hang.sh");
    const PrepareChild got = run_prepare_child(f6_prepare_env(root, compiler, "4000"), 12000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "timeout"));
}

TEST_CASE("F6 (fail): child holds compiler pipe" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "hangchild";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto compiler = f6_fake_compiler("fake-hangchild.sh");
    const PrepareChild got = run_prepare_child(f6_prepare_env(root, compiler, "4000"), 12000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "timeout"));
}

TEST_CASE("F6 (fail): lock held by another process" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "locked";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    char build[17];
    std::snprintf(build, sizeof(build), "%016llx",
                  static_cast<unsigned long long>(anno::test::coordinator_build()));
    const auto lock_path = (root / (std::string("anno_budget_coord-") + build)).string() + ".lock";
    Child holder;
    {
        Env hold({{"ANNO_TEST_F6_HOLD_LOCK", lock_path}});
        holder = Child::start(self_exe(),
                              {"--test-case=*hold build lock*", "--no-intro", "--no-version"});
    }
    REQUIRE(static_cast<bool>(holder));
    const std::string held = want(holder, "HELD", 8000ms);
    INFO("holder: " << held);
    REQUIRE(has(held, "HELD"));
    const auto compiler = f6_fake_compiler("fake-ok.sh");
    auto env = f6_prepare_env(root, compiler, "2500");
    const PrepareChild got = run_prepare_child(env, 8000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "prepare wait deadline"));
    holder.stop();
}

TEST_CASE("F6 (fail): lock creation failure" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "lockcreate";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto compiler = f6_fake_compiler("fake-ok.sh");
    auto env = f6_prepare_env(root, compiler, "4000");
    env.push_back({"ANNO_TEST_COORD_LOCK_FAIL", "create"});
    const PrepareChild got = run_prepare_child(env, 8000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "lock creation failed"));
}

TEST_CASE("F6 (fail): partial compiler output" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "partial";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto compiler = f6_fake_compiler("fake-partial.sh");
    const PrepareChild got = run_prepare_child(f6_prepare_env(root, compiler, "15000"), 20000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " exit=" << got.compiler_exit
                     << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "compiler exit"));
}

TEST_CASE("F6 (fail): unusable artifact" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "bad";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto compiler = f6_fake_compiler("fake-bad.sh");
    const PrepareChild got = run_prepare_child(f6_prepare_env(root, compiler, "15000"), 20000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "unusable"));
}

TEST_CASE("F6 (fail): wrong listening identity" * doctest::test_suite("budget")) {
    const auto script =
        write_f6_script("bad-listen.sh", "echo 'LISTENING 147825 build=0000000000000000 compat=99 "
                                         "incarnation=0000000000000001 orphans=0 config=none'\n"
                                         "sleep 5\n");
    const auto journal = f6_dir() / "bad-listen.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(47825, journal, {}, script);
    INFO("ready=" << coord.ready << " listening=" << coord.listening
                  << " diag=" << coord.diagnostic);
    REQUIRE(has(coord.listening, "LISTENING"));
    CHECK(!coord.ready);
    CHECK(has(coord.diagnostic, "identity"));
}

TEST_CASE("F6 (ok): path with spaces" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "dir with spaces";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto compiler = f6_fake_compiler("fake-ok.sh");
    const PrepareChild got = run_prepare_child(f6_prepare_env(root, compiler, "20000"), 25000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " binary=" << got.binary
                     << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(got.ok);
    CHECK(has(got.binary, "dir with spaces"));
}

TEST_CASE("F6 (fail): lock acquire failure" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "lockacquire";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    auto env = f6_prepare_env(root, f6_fake_compiler("fake-ok.sh"), "4000");
    env.push_back({"ANNO_TEST_COORD_LOCK_FAIL", "acquire"});
    const PrepareChild got = run_prepare_child(env, 8000ms);
    INFO("finished=" << got.finished << " ok=" << got.ok << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(has(got.diagnostic, "lock acquire failed"));
}

TEST_CASE("F6: failed rebuild keeps a verified artifact" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "keep";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const PrepareChild first =
        run_prepare_child(f6_prepare_env(root, f6_fake_compiler("fake-ok.sh"), "20000"), 25000ms);
    INFO("first diag=" << first.diagnostic);
    REQUIRE(first.finished);
    REQUIRE(first.ok);
    const auto binary = f6_artifact(root);
    const std::string before = slurp(binary);
    const std::string manifest = slurp(std::filesystem::path(binary.string() + ".identity"));
    REQUIRE(!before.empty());
    auto again_env = f6_prepare_env(root, f6_fake_compiler("fake-partial.sh"), "15000");
    again_env.push_back({"ANNO_TEST_COORD_PREPARE_REBUILD", "1"});
    const PrepareChild again = run_prepare_child(again_env, 20000ms);
    INFO("again diag=" << again.diagnostic);
    CHECK(again.finished);
    CHECK(!again.ok);
    CHECK(has(again.diagnostic, "compiler exit"));
    CHECK(slurp(binary) == before);
    CHECK(slurp(std::filesystem::path(binary.string() + ".identity")) == manifest);
    CHECK(!any_building(root));
    auto fail_env = f6_prepare_env(root, f6_fake_compiler("fake-ok.sh"), "4000");
    fail_env.push_back({"ANNO_TEST_COORD_PREPARE_FAIL", "1"});
    const PrepareChild failed = run_prepare_child(fail_env, 8000ms);
    CHECK(failed.finished);
    CHECK(!failed.ok);
    CHECK(slurp(binary) == before);
}

TEST_CASE("F6: two preparer processes" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "two";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto env = f6_prepare_env(root, f6_fake_compiler("fake-delay.sh"), "20000");
    PrepareChild left;
    PrepareChild right;
    std::thread left_thread([&] { left = run_prepare_child(env, 25000ms); });
    std::thread right_thread([&] { right = run_prepare_child(env, 25000ms); });
    left_thread.join();
    right_thread.join();
    INFO("left=" << left.ok << " " << left.diagnostic << " right=" << right.ok << " "
                 << right.diagnostic);
    CHECK(left.finished);
    CHECK(right.finished);
    CHECK(left.ok);
    CHECK(right.ok);
    CHECK(std::filesystem::exists(f6_artifact(root)));
    CHECK(!any_building(root));
}

TEST_CASE("F6: interrupted builder then recovery" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "recover";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const PrepareChild hung =
        run_prepare_child(f6_prepare_env(root, f6_fake_compiler("fake-hang.sh"), "3000"), 10000ms);
    CHECK(hung.finished);
    CHECK(!hung.ok);
    CHECK(!any_building(root));
    const PrepareChild recovered =
        run_prepare_child(f6_prepare_env(root, f6_fake_compiler("fake-ok.sh"), "20000"), 25000ms);
    INFO("recovered diag=" << recovered.diagnostic);
    CHECK(recovered.finished);
    CHECK(recovered.ok);
    CHECK(!any_building(root));
}

TEST_CASE("F6: observer never sees a partial publication" * doctest::test_suite("budget")) {
    const auto root = f6_dir() / "observe";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto binary = f6_artifact(root);
    std::atomic<bool> stop{false};
    std::atomic<int> saw_partial{0};
    std::thread observer([&] {
        while (!stop.load()) {
            const std::string data = slurp(binary);
            if (data.find("PARTIAL") != std::string::npos) {
                saw_partial.store(1);
            }
            std::this_thread::sleep_for(5ms);
        }
    });
    const PrepareChild got =
        run_prepare_child(f6_prepare_env(root, f6_fake_compiler("fake-slow.sh"), "15000"), 20000ms);
    stop.store(true);
    observer.join();
    INFO("diag=" << got.diagnostic << " saw=" << saw_partial.load());
    CHECK(got.finished);
    CHECK(got.ok);
    CHECK(saw_partial.load() == 0);
    CHECK(slurp(binary).find("PARTIAL") == std::string::npos);
}

TEST_CASE("F6 (fail): early exit is not ready" * doctest::test_suite("budget")) {
    const auto script = write_f6_script("early-exit.sh", "exit 3\n");
    const auto journal = f6_dir() / "early.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(47836, journal, {}, script);
    INFO("diag=" << coord.diagnostic);
    CHECK(!coord.ready);
    CHECK(has(coord.diagnostic, "not ready"));
}

TEST_CASE("F6 (fail): foreign listener is not ready" * doctest::test_suite("budget")) {
    const auto script = write_f6_script("foreign.sh", "echo FOREIGN-LISTENER\nexit 0\n");
    const auto journal = f6_dir() / "foreign.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(47837, journal, {}, script);
    INFO("diag=" << coord.diagnostic << " listening=" << coord.listening);
    CHECK(!coord.ready);
    CHECK(!has(coord.listening, "LISTENING"));
}

TEST_CASE("F6 (asan): private prepare keeps an instrumented coordinator" *
          doctest::test_suite("budget")) {
#if !defined(__SANITIZE_ADDRESS__)
    MESSAGE("skipping: this translation unit is not built with address sanitizer");
    return;
#else
    if (!wsl_available() && !std::filesystem::exists("/proc/self/maps")) {
        MESSAGE("skipping: no Linux proc maps to check libasan");
        return;
    }
    const auto root = f6_dir() / "asan";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    Env prepare_root({{"ANNO_TEST_COORD_PREPARE_ROOT", root.string()},
                      {"ANNO_TEST_COORD_PREPARE_DEADLINE_MS", "120000"},
                      {"ANNO_TEST_COORD_COMPILER", ""}});
    const anno::test::CoordinatorPrepare prep = anno::test::prepare_host_coordinator();
    INFO(prep.diagnostic);
    REQUIRE(prep.ok);
    CHECK(prep.config == "asan+ubsan");
    CHECK(has(prep.flags, "fsanitize"));
    const std::string manifest = slurp(std::filesystem::path(prep.binary.string() + ".identity"));
    CHECK(has(manifest, "config=asan+ubsan"));
    const auto journal = f6_dir() / "asan.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(47838, journal, {}, prep.binary);
    INFO(coord.diagnostic << " " << coord.listening);
    REQUIRE(coord.ready);
#if !defined(_WIN32)
    const std::string maps = slurp("/proc/" + coord.linux_pid + "/maps");
    CHECK(has(maps, "libasan"));
#else
    const std::string maps = wsl_capture("tr '\\n' ' ' < /proc/" + coord.linux_pid + "/maps");
    CHECK(has(maps, "libasan"));
#endif
#endif
}

TEST_CASE("F4 (N4b): a descriptor the client holds without close-on-exec does not reach the "
          "coordinator it spawns" *
          doctest::test_suite("budget")) {
#if defined(_WIN32)
    MESSAGE("skipping on Windows: bInheritHandles = FALSE is at coord.cpp:142 "
            "(CreateProcessW(..., FALSE, ...)); POSIX spawn closefrom is the Linux/WSL path");
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

TEST_CASE("F6 (R1): orphans=garbage is not a listening identity" * doctest::test_suite("budget")) {
    char line[320];
    std::snprintf(line, sizeof(line),
                  "LISTENING 47839 build=%016llx compat=%llu incarnation=0000000000000001 "
                  "orphans=garbage config=%s",
                  static_cast<unsigned long long>(anno::test::coordinator_build()),
                  static_cast<unsigned long long>(anno::test::wire::kCompatVersion),
                  coordinator_config_name());
    const auto identity = anno::test::coordinator_listening_identity(
        line, 47839, anno::test::coordinator_build(), anno::test::wire::kCompatVersion,
        coordinator_config_name());
    INFO(identity.diagnostic);
    CHECK(!identity.matches);
}

TEST_CASE("F6 (R1): an ELF prefix plus the marker is not a reusable executable" *
          doctest::test_suite("budget")) {
    const auto root = f6_dir() / "r1-prefix";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const PrepareChild got = run_prepare_child(
        f6_prepare_env(root, f6_fake_compiler("fake-prefix.sh"), "20000"), 25000ms);
    INFO("ok=" << got.ok << " reused=" << got.reused << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.ok);
    CHECK(!got.reused);
}

TEST_CASE("F6 (R1): a forged LISTENING line with no listener is not ready" *
          doctest::test_suite("budget")) {
    char body[400];
    std::snprintf(body, sizeof(body),
                  "echo \"LISTENING $1 build=%016llx compat=%llu incarnation=0000000000000001 "
                  "orphans=0 config=%s\"\n"
                  "exit 0\n",
                  static_cast<unsigned long long>(anno::test::coordinator_build()),
                  static_cast<unsigned long long>(anno::test::wire::kCompatVersion),
                  coordinator_config_name());
    const auto script = write_f6_script("forged-listen.sh", body);
    const auto journal = f6_dir() / "forged.journal";
    remove_file(journal);
    OwnedCoord coord = start_owned_host_coordinator(47839, journal, {}, script);
    INFO("ready=" << coord.ready << " listening=" << coord.listening
                  << " diag=" << coord.diagnostic);
    CHECK(!coord.ready);
}

TEST_CASE("F6 (R2): closing stdout does not time out a child that exits inside the deadline" *
          doctest::test_suite("budget")) {
    const auto root = f6_dir() / "r2-closefd";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const PrepareChild got = run_prepare_child(
        f6_prepare_env(root, f6_fake_compiler("fake-closefd.sh"), "1000"), 8000ms);
    INFO("ok=" << got.ok << " timeout=" << got.timed_out << " exit=" << got.compiler_exit
               << " diag=" << got.diagnostic);
    CHECK(got.finished);
    CHECK(!got.timed_out);
    CHECK(got.compiler_exit == 0);
}

TEST_CASE("F6 (R2): hashing a warm artifact does not succeed after the deadline" *
          doctest::test_suite("budget")) {
    const auto root = f6_dir() / "r2-big";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const PrepareChild first =
        run_prepare_child(f6_prepare_env(root, f6_fake_compiler("fake-big.sh"), "30000"), 40000ms);
    INFO("first ok=" << first.ok << " diag=" << first.diagnostic);
    REQUIRE(first.finished);
    REQUIRE(first.ok);
    const PrepareChild second =
        run_prepare_child(f6_prepare_env(root, f6_fake_compiler("fake-big.sh"), "1"), 15000ms);
    INFO("second ok=" << second.ok << " reused=" << second.reused << " timeout=" << second.timed_out
                      << " diag=" << second.diagnostic);
    CHECK(second.finished);
    CHECK(!second.ok);
    CHECK(second.timed_out);
}

TEST_CASE("F6 (R3): the F6 scratch directory is owned by this process" *
          doctest::test_suite("budget")) {
    const auto name = f6_dir().filename().string();
    INFO(name);
    CHECK(name.rfind("anno1503-f6-", 0) == 0);
#if defined(_WIN32)
    const auto pid_text = std::to_string(GetCurrentProcessId());
#else
    const auto pid_text = std::to_string(static_cast<unsigned long>(::getpid()));
#endif
    CHECK(name.find(pid_text) != std::string::npos);
}
