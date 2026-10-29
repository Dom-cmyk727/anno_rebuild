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
#include <latch>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#include "../src/coord_wire.hpp"
#include "budget_proc_harness.hpp"
#include "child_process.hpp"

#if !defined(_WIN32)
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
            wsl_capture("kill -9 " + linux_pid + " || true");
            linux_pid.clear();
        }
        child.stop();
    }

  private:
    void take(OwnedCoord& other) noexcept {
        child = std::move(other.child);
        listening = std::move(other.listening);
        linux_pid = std::move(other.linux_pid);
        diagnostic = std::move(other.diagnostic);
        ready = other.ready;
        other.ready = false;
        other.linux_pid.clear();
    }
};

OwnedCoord start_owned_host_coordinator(std::uint16_t port, const std::filesystem::path& journal,
                                        const std::string& leak_path = {}) {
    OwnedCoord owned;
    const auto binary = anno::test::host_coordinator_binary();
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
#endif
    if (owned.listening.empty()) {
        owned.diagnostic = "not ready: " + seen;
        if (!owned.child.running()) {
            owned.diagnostic += " (child exit)";
        }
        return owned;
    }
    if (!has(owned.listening, std::to_string(port)) || !has(owned.listening, "build=") ||
        !has(owned.listening, "compat=")) {
        owned.diagnostic = "LISTENING missing port/build/compat: " + owned.listening;
        return owned;
    }
    owned.ready = true;
    return owned;
}

struct BackupArtifact {
    std::filesystem::path original;
    std::filesystem::path backup;
    bool moved = false;

    explicit BackupArtifact(std::filesystem::path path) : original(std::move(path)) {
        std::error_code error;
        if (std::filesystem::exists(original, error)) {
            backup = original;
            backup += ".f5bak";
            std::filesystem::rename(original, backup, error);
            moved = !error;
        }
    }
    BackupArtifact(const BackupArtifact&) = delete;
    BackupArtifact& operator=(const BackupArtifact&) = delete;
    ~BackupArtifact() {
        std::error_code error;
        if (!moved) {
            return;
        }
#if defined(_WIN32)
        // Keep a completed prepare result so other CTest processes still have
        // the cache file. Drop the backup.
        if (std::filesystem::exists(original, error)) {
            std::filesystem::remove(backup, error);
        } else if (std::filesystem::exists(backup, error)) {
            std::filesystem::rename(backup, original, error);
        }
#else
        // Put the cmake/ASan binary back so later cases in this process do
        // not keep an unsanitized g++ rebuild.
        if (std::filesystem::exists(original, error)) {
            std::filesystem::remove(original, error);
        }
        if (std::filesystem::exists(backup, error)) {
            std::filesystem::rename(backup, original, error);
        }
#endif
    }
};

/// Serializes cases that move or start this worktree's one cache binary so a
/// parallel CTest run cannot rename it out from under another case.
struct CoordArtifactLock {
#if defined(_WIN32)
    HANDLE mutex = nullptr;
    explicit CoordArtifactLock() {
        mutex = CreateMutexW(nullptr, FALSE, L"Local\\anno1503-f5-coord-artifact");
        if (mutex != nullptr) {
            WaitForSingleObject(mutex, 180000);
        }
    }
    ~CoordArtifactLock() {
        if (mutex != nullptr) {
            ReleaseMutex(mutex);
            CloseHandle(mutex);
        }
    }
#else
    int fd = -1;
    explicit CoordArtifactLock() {
        fd = ::open("/tmp/anno1503-f5-coord-artifact.lock", O_CREAT | O_RDWR, 0644);
        if (fd >= 0) {
            ::flock(fd, LOCK_EX);
        }
    }
    ~CoordArtifactLock() {
        if (fd >= 0) {
            ::flock(fd, LOCK_UN);
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
    run_n4_meter_descriptor_case(kPortMeterLeak);
}

TEST_CASE("F5 (cold): N4 prepares this worktree's WSL binary when it is absent" *
          doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    CoordArtifactLock lock;
    BackupArtifact cold(anno::test::host_coordinator_binary());
    run_n4_meter_descriptor_case(kPortCold);
}

TEST_CASE("F5 (warm): N4 reuses a completed artifact" * doctest::test_suite("budget")) {
    if (!wsl_available()) {
        MESSAGE("skipping: WSL (Ubuntu) is not available; the meter path is WSL");
        return;
    }
    CoordArtifactLock lock;
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
    BackupArtifact cold(anno::test::host_coordinator_binary());
    run_n4_meter_descriptor_case(kPortCompanionN4);
    CHECK(peer.child.running());
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
    CoordArtifactLock lock;
    BackupArtifact missing(anno::test::host_coordinator_binary());
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
    CoordArtifactLock lock;
    BackupArtifact missing(anno::test::host_coordinator_binary());
    Env fail({{"ANNO_TEST_COORD_PREPARE_FAIL", "1"}});
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
    CoordArtifactLock lock;
    BackupArtifact missing(anno::test::host_coordinator_binary());
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
