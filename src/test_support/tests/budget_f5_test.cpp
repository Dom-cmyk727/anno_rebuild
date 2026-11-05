// MAP-2 repair regressions: who owns a journaled promise (ownership and recovery).
//
//   O1: a live holder keeps its reservation across a coordinator restart on the same journal
//       (capacity 100: A holds 50, the successor sees it, B asks for 80 and waits), and the
//       exited holder's reservation is released.
//   O2: a pid that was reused (same pid, another start stamp) is a stopped owner.
//   O3: a record the host cannot attribute (another OS's namespace, or a namespace-2 record with
//       the zero stamp that the first macOS build wrote) is kept: fail closed, never a stop.
//   O4: the CPU tokens share that ownership logic (live kept, exited released).
//   S1: a budget client that starts its coordinator and exits does not keep the coordinator on
//       its stdout: the pipe reaches EOF while the coordinator still listens (detached stdio).
//   S2: a coordinator that a test starts in the foreground keeps its stdout (LISTENING).
//   S3: one captured command line (a shell pipeline, as ctest runs a test) ends without waiting
//       for the coordinator's idle shutdown.
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
#include <map>
#include <optional>
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
[[maybe_unused]] constexpr std::uint16_t kPortLegacy = 47854; // macOS cases only
[[maybe_unused]] constexpr std::uint16_t kPortSpace = 47855;  // macOS cases only
constexpr std::uint16_t kPortStreams = 47856;
constexpr std::uint16_t kPortForeground = 47857;
constexpr std::uint16_t kPortPipeline = 47858;
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

TEST_CASE("O1: a live holder keeps its reservation through a restart and its exit releases it" *
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

namespace {

/// Whether something accepts a connection on 127.0.0.1:`port` (a bare connect, closed at once).
bool listening(std::uint16_t port) {
    const wire::Native fd = wire::open_socket();
    if (fd == wire::kInvalid) {
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(0x7f000001u);
#if defined(_WIN32)
    const bool up = ::connect(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&address),
                              sizeof(address)) == 0;
#else
    const bool up = ::connect(static_cast<int>(fd), reinterpret_cast<sockaddr*>(&address),
                              sizeof(address)) == 0;
#endif
    wire::close_fd(fd);
    return up;
}

/// Asks the idle coordinator on `port` to step aside, the documented handover: a client that
/// speaks another compatibility version is refused and an idle coordinator then exits. The
/// coordinator is found by its port, never by name.
void step_aside(std::uint16_t port, const Vars& vars, const std::string& budget_path) {
    Vars other = vars;
    other.emplace_back("ANNO_TEST_BUDGET_CLIENT_COMPAT", "99");
    other.emplace_back("ANNO_TEST_BUDGET_SPAWN", "0");
    {
        Env env(other);
        Child refused = probe({"try", budget_path, "1"});
        refused.line(5000ms);
    }
    REQUIRE(eventually([&] { return !listening(port); }, 15000ms));
}

} // namespace

TEST_CASE("S1: a client that starts the coordinator and exits leaves its stdout at EOF" *
          doctest::test_suite("budget")) {
    const auto journal = f5_dir() / "s1.journal";
    const std::string budget_path = (f5_dir() / "s1.budget").string();
    remove_file(journal);
    REQUIRE_FALSE(listening(kPortStreams));
    Vars vars = client_vars(kPortStreams, "s1", "30", "1"); // the client may start a coordinator
    vars.emplace_back("ANNO_TEST_BUDGET_JOURNAL", journal.string());

    Child client;
    {
        Env env(vars);
        client = probe({"waiter", budget_path, "10"});
    }
    std::string seen;
    REQUIRE(has(want(client, "ADMITTED", 30000ms, &seen), "ADMITTED 1"));
    INFO("client: " << seen);
    // It reserved through the coordinator it started, which keeps running after the client.
    REQUIRE(listening(kPortStreams));
    quit_and_wait(client);
    REQUIRE_FALSE(client.running());

    // The client is gone. Its output pipe must end now, not when the coordinator dies
    // (30 s idle): the coordinator must not hold the client's stdout.
    const auto t0 = std::chrono::steady_clock::now();
    const bool eof = client.wait_eof(5000ms);
    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    CHECK_MESSAGE(eof, "stdout still open " << waited.count()
                                            << " ms after the client exited: the detached "
                                               "coordinator inherited it");
    CHECK_MESSAGE(listening(kPortStreams), "the coordinator was expected to still be alive");

    step_aside(kPortStreams, vars, budget_path);
}

TEST_CASE("S2: a coordinator a test starts in the foreground keeps its stdout" *
          doctest::test_suite("budget")) {
    const auto journal = f5_dir() / "s2.journal";
    remove_file(journal);
    std::string line;
    Child coord = start_coordinator(kPortForeground, journal, std::nullopt, std::nullopt, line);
    CHECK(line.rfind("LISTENING", 0) == 0);
    CHECK(has(line, "compat="));
    coord.stop();
}

TEST_CASE("S3: a captured pipeline ends without waiting for the coordinator's idle exit" *
          doctest::test_suite("budget")) {
#if defined(_WIN32)
    MESSAGE("skipped: the pipeline uses a POSIX shell");
#else
    const auto journal = f5_dir() / "s3.journal";
    const std::string budget_path = (f5_dir() / "s3.budget").string();
    remove_file(journal);
    REQUIRE_FALSE(listening(kPortPipeline));
    const Vars vars = client_vars(kPortPipeline, "s3", "30", "1");
    // `cat` reads the client's stdout to EOF, as the capture of a ctest test does. Every writer
    // of that pipe must close it; the coordinator started by the client must not be one.
    const std::string command =
        "ANNO_TEST_BUDGET_COORD=local ANNO_TEST_BUDGET_PORT=" + std::to_string(kPortPipeline) +
        " ANNO_TEST_BUDGET_ID=s3 ANNO_TEST_BUDGET_RECHECK_MS=100 ANNO_TEST_BUDGET_SPAWN=1"
        " ANNO_TEST_ADMIT_WAIT_S=30 ANNO_TEST_BUDGET_JOURNAL=" +
        journal.string() + " sh -c '{ sleep 2; printf \"q\\n\"; } | " +
        sibling("anno_budget_probe").string() + " waiter " + budget_path + " 10' 2>&1 | cat";
    const auto t0 = std::chrono::steady_clock::now();
    std::string out;
    if (FILE* pipe = popen(command.c_str(), "r")) {
        char buffer[256];
        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            out += buffer;
        }
        pclose(pipe);
    }
    const auto waited =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - t0);
    INFO("command: " << command);
    INFO("output: " << out);
    CHECK(has(out, "ADMITTED 1"));
    CHECK_MESSAGE(waited.count() < 15,
                  "the pipeline took " << waited.count() << " s: it waited for the coordinator");
    CHECK(listening(kPortPipeline)); // and the coordinator it started is still there
    step_aside(kPortPipeline, vars, budget_path);
#endif
}

// ---- compatibility 4: the transition from earlier coordinators (MAP-2 repair 2)
//
//   T1  (modeled) a coordinator started as compatibility 3 and a compatibility-4 client refuse each
//       other: idle steps aside, busy is waited for with a diagnostic and its holder is untouched.
//   T2  every version uses one default journal per port (a second file loses promises across a
//       handoff, see the H cases).
//   T5  (opt-in, ANNO_TEST_OLD_COORD) a real older coordinator binary steps aside for a
//       compatibility-4 client. The "older coordinator" of T1 to T4 is this build started with
//       --compat-version 3: it models the handshake and the journal name, not the older recovery
//       code. The mixed-binary evidence is tools/test/mixed_coordinator_check.sh.
//
// Ports 47860..47869.
#if !defined(_WIN32)
namespace {

constexpr std::uint16_t kPortT1Idle = 47860;
constexpr std::uint16_t kPortT1Busy = 47861;
constexpr std::uint16_t kPortT2 = 47862;
constexpr std::uint16_t kPortT5 = 47867;

/// A private HOME so the default journals of a coordinator that is started without --journal land
/// in a directory of this case.
struct Home {
    std::filesystem::path dir;
    explicit Home(const char* name) : dir(f5_dir() / (std::string("home-") + name)) {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
        std::filesystem::create_directories(dir);
    }
    std::filesystem::path journal(std::uint16_t port, const char* suffix = "") const {
        return dir / ".cache" / "anno1503" /
               ("coord-" + std::to_string(port) + suffix + ".journal");
    }
    Vars vars() const { return {{"HOME", dir.string()}, {"ANNO_TEST_BUDGET_JOURNAL", ""}}; }
};

/// Starts this build's coordinator with the default journal under the Home in the environment.
/// `compat` models an older version.
Child start_default(std::uint16_t port, const Home& home, std::optional<std::uint64_t> compat,
                    std::string& listening) {
    const auto exe = sibling("anno_budget_coord");
    REQUIRE(std::filesystem::exists(exe));
    std::vector<std::string> args = {std::to_string(port), "local"};
    if (compat) {
        args.emplace_back("--compat-version");
        args.push_back(std::to_string(*compat));
    }
    Env env(home.vars());
    Child coord = Child::start(exe, args);
    REQUIRE(static_cast<bool>(coord));
    listening = coord.line(15000ms);
    INFO("coordinator on port " << port << " first line: '" << listening << "'");
    REQUIRE(listening.rfind("LISTENING", 0) == 0);
    return coord;
}

} // namespace

TEST_CASE("T1: a compat 3 coordinator and a compat 4 client refuse each other and keep the holder" *
          doctest::test_suite("budget")) {
    // Idle: it steps aside for the new client (the documented handover).
    {
        const auto journal = f5_dir() / "t1-idle.journal";
        remove_file(journal);
        std::string line;
        Child coord = start_coordinator(kPortT1Idle, journal, std::nullopt, 3, line);
        CHECK(has(line, " compat=3"));
        Vars vars = client_vars(kPortT1Idle, "t1i", "3");
        Env env(vars);
        Child client = probe({"try", (f5_dir() / "t1-idle.budget").string(), "10"});
        client.line(5000ms);
        CHECK(eventually([&] { return !listening(kPortT1Idle); }, 15000ms));
    }
    // Busy: a holder of the old version is served by it; the new client is refused, waits for a
    // bounded time and ends with a diagnostic that names both versions. The holder is untouched.
    {
        const auto journal = f5_dir() / "t1-busy.journal";
        const std::string budget = (f5_dir() / "t1-busy.budget").string();
        remove_file(journal);
        std::string line;
        Child coord = start_coordinator(kPortT1Busy, journal, std::nullopt, 3, line);
        Vars old_vars = client_vars(kPortT1Busy, "t1b", "30");
        old_vars.emplace_back("ANNO_TEST_BUDGET_CLIENT_COMPAT", "3");
        Child holder;
        {
            Env env(old_vars);
            holder = probe({"holder", budget, "50"});
        }
        REQUIRE(has(want(holder, "READY", 15000ms), "READY 1"));
        Vars vars = client_vars(kPortT1Busy, "t1b", "2");
        Child client;
        {
            Env env(vars);
            client = probe({"waiter", budget, "10"});
        }
        std::string seen;
        const std::string result = want(client, "ADMITTED", 15000ms, &seen);
        INFO("client: " << seen);
        CHECK(has(result, "ADMITTED 0"));
        CHECK(has(result, "compatibility version 3"));
        CHECK(has(result, "compatibility version 4"));
        CHECK(has(result, "incompatible compatibility version"));
        CHECK(has(result, "wait deadline"));
        CHECK(listening(kPortT1Busy));
        holder.send("t\n"); // the holder still talks to its coordinator
        CHECK(has(want(holder, "CLAIM", 5000ms), "CLAIM"));
    }
}

TEST_CASE("T2: every version uses one default journal per port" * doctest::test_suite("budget")) {
    Home home("t2");
    std::string line;
    {
        Child coord = start_default(kPortT2, home, std::nullopt, line);
        CHECK(has(line, " compat=4"));
        CHECK(std::filesystem::exists(home.journal(kPortT2)));
        CHECK_FALSE(std::filesystem::exists(home.journal(kPortT2, "-v4")));
    }
    {
        Child coord = start_default(kPortT2, home, 3, line);
        CHECK(has(line, " compat=3"));
        CHECK(std::filesystem::exists(home.journal(kPortT2)));
        CHECK_FALSE(std::filesystem::exists(home.journal(kPortT2, "-v4")));
    }
}

TEST_CASE(
    "T5: a real older coordinator steps aside for a compat 4 client (opt-in ANNO_TEST_OLD_COORD)" *
    doctest::test_suite("budget")) {
    const char* old = std::getenv("ANNO_TEST_OLD_COORD");
    if (old == nullptr || old[0] == '\0') {
        MESSAGE("skipping: set ANNO_TEST_OLD_COORD=<an older anno_budget_coord> to run");
        return;
    }
    const auto journal = f5_dir() / "t5.journal";
    remove_file(journal);
    Child coord =
        Child::start(old, {std::to_string(kPortT5), "local", "--journal", journal.string()});
    REQUIRE(static_cast<bool>(coord));
    const std::string line = coord.line(15000ms);
    INFO("older coordinator: " << line);
    REQUIRE(line.rfind("LISTENING", 0) == 0);
    CHECK(has(line, " compat=3"));
    Vars vars = client_vars(kPortT5, "t5", "3");
    Env env(vars);
    Child client = probe({"try", (f5_dir() / "t5.budget").string(), "10"});
    client.line(5000ms);
    CHECK(eventually([&] { return !listening(kPortT5); }, 15000ms));
}
#endif

// ---- handoffs between coordinator versions must keep every outstanding promise (MAP-2 repair 4)
//
//   H1  memory: v4 starts idle and stops, a v3 coordinator admits a live holder of 50 and stops,
//       v4 restarts: 80 must still be refused (capacity 100), and is admitted after the holder
//       exits
//   H2  the same for a CPU token (budget 1)
//   H3  control: v4 starts for the first time on the holder's journal
//   H4  repeated v4, v3, v4, v3, v4 handoffs
//   H5  downgrade: a v4 holder, the v4 coordinator is lost, v3 starts and must refuse the 80, v4
//       returns and releases after the holder exits
//   H6  an owner the host cannot attribute stays protected through the handoffs
// "v3" is this build started with --compat-version 3 (modeled) or, in the (real) cases, the older
// binary named by ANNO_TEST_OLD_COORD (opt-in). Ports 47880..47889.
#if !defined(_WIN32)
namespace {

constexpr std::uint16_t kPortH1 = 47880;
constexpr std::uint16_t kPortH2 = 47881;
constexpr std::uint16_t kPortH3 = 47882;
constexpr std::uint16_t kPortH4 = 47883;
constexpr std::uint16_t kPortH5 = 47884;
constexpr std::uint16_t kPortH6 = 47885;

enum class Kind { Memory, Cpu };

struct Handoff {
    Home home;
    Kind kind;
    std::uint16_t port;
    const char* old_exe; // nullptr: modeled
    Vars vars;
    std::string budget_path;
    std::string listening;
    Child coord;
    Child holder;
    Handoff(const char* name, std::uint16_t p, Kind k, const char* old)
        : home(name), kind(k), port(p), old_exe(old) {
        budget_path = (f5_dir() / (std::string(name) + ".budget")).string();
        vars = client_vars(port, name, "30");
        vars.emplace_back("HOME", home.dir.string());
        vars.emplace_back("ANNO_TEST_BUDGET_JOURNAL", "");
    }
    void stop_coordinator() { coord.stop(); }
    void start_v4() { coord = start_default(port, home, std::nullopt, listening); }
    void start_v3() {
        if (old_exe == nullptr) {
            coord = start_default(port, home, 3, listening);
            return;
        }
        Env env(home.vars());
        coord = Child::start(old_exe, {std::to_string(port), "local"});
        REQUIRE(static_cast<bool>(coord));
        listening = coord.line(15000ms);
        REQUIRE(listening.rfind("LISTENING", 0) == 0);
        REQUIRE(has(listening, " compat=3"));
    }
    /// A live holder of the version `v3_client ? 3 : 4` (50 bytes, or the CPU token).
    void hold(bool v3_client) {
        Vars use = vars;
        if (v3_client) {
            use.emplace_back("ANNO_TEST_BUDGET_CLIENT_COMPAT", "3");
        }
        {
            Env env(use);
            holder = probe({kind == Kind::Memory ? "holder" : "cpu-holder", budget_path, "50"});
        }
        std::string seen;
        REQUIRE(has(want(holder, "READY", 15000ms, &seen), "READY 1"));
    }
    /// One request of 80 bytes (or the second CPU token) from a client of the given version.
    /// True when it was admitted at once.
    bool asks(bool v3_client) {
        Vars use = vars;
        if (v3_client) {
            use.emplace_back("ANNO_TEST_BUDGET_CLIENT_COMPAT", "3");
        }
        Env env(use);
        if (kind == Kind::Memory) {
            Child check = probe({"try", budget_path, "80"});
            return has(want(check, "claim=", 15000ms), "claim=1");
        }
        Child waiter = probe({"cpu-waiter", budget_path, "1"});
        return has(waiter.line(2500ms), "ADMITTED 1");
    }
    bool admitted_after_exit(bool v3_client) {
        holder.stop();
        for (int i = 0; i < 60; ++i) {
            if (asks(v3_client)) {
                return true;
            }
            std::this_thread::sleep_for(250ms);
        }
        return false;
    }
    void leave_unattributable() {
        rewrite_owner(home.journal(port), [](Owner3 o) {
            o.ns = 99;
            return o;
        });
    }
};

/// The shared check after any handoff: the promise of the live holder still counts.
void refused(Handoff& h, bool v3_client, const char* where) {
    INFO("after " << std::string(where) << ", a request of the " << (v3_client ? "v3" : "v4")
                  << " client must not be admitted beside the live holder");
    CHECK_FALSE(h.asks(v3_client));
}

void run_h1(Kind kind, std::uint16_t port, const char* name, const char* old) {
    Env cpu_env({{"ANNO_TEST_CPU_BUDGET", "1"}});
    Handoff h(name, port, kind, old);
    h.start_v4(); // first start: idle, leaves its journal
    h.stop_coordinator();
    h.start_v3();
    h.hold(true);
    h.stop_coordinator(); // v3 exits, the holder lives on
    h.start_v4();
    refused(h, false, "v4 idle, v3 holder, v3 exit, v4 restart");
    CHECK(h.admitted_after_exit(false));
}

} // namespace

TEST_CASE("H1: a holder admitted by v3 between two v4 runs keeps its 50 (modeled v3)" *
          doctest::test_suite("budget")) {
    run_h1(Kind::Memory, kPortH1, "h1", nullptr);
}

TEST_CASE("H2: the same for a CPU token (modeled v3)" * doctest::test_suite("budget")) {
    run_h1(Kind::Cpu, kPortH2, "h2", nullptr);
}

TEST_CASE("H3: control, the first v4 start on the journal of a v3 holder refuses too (modeled v3)" *
          doctest::test_suite("budget")) {
    Env cpu_env({{"ANNO_TEST_CPU_BUDGET", "1"}});
    for (const Kind kind : {Kind::Memory, Kind::Cpu}) {
        Handoff h(kind == Kind::Memory ? "h3m" : "h3c", kPortH3, kind, nullptr);
        h.start_v3();
        h.hold(true);
        h.stop_coordinator();
        h.start_v4();
        refused(h, false, "the first v4 start");
        CHECK(h.admitted_after_exit(false));
    }
}

TEST_CASE("H4: repeated v4, v3, v4, v3, v4 handoffs keep the promise (modeled v3)" *
          doctest::test_suite("budget")) {
    Handoff h("h4", kPortH4, Kind::Memory, nullptr);
    h.start_v4();
    h.stop_coordinator();
    h.start_v3();
    h.hold(true);
    h.stop_coordinator();
    h.start_v4();
    refused(h, false, "the first v4 restart");
    h.stop_coordinator();
    h.start_v3();
    refused(h, true, "the second v3 start");
    h.stop_coordinator();
    h.start_v4();
    refused(h, false, "the second v4 restart");
    CHECK(h.admitted_after_exit(false));
}

namespace {
void run_h5(std::uint16_t port, const char* name, const char* old) {
    Handoff h(name, port, Kind::Memory, old);
    h.start_v4();
    h.hold(false);
    h.stop_coordinator(); // the coordinator is lost, the holder lives on
    h.start_v3();
    refused(h, true, "a v3 start over a v4 holder");
    h.stop_coordinator();
    h.start_v4();
    refused(h, false, "the return of v4");
    CHECK(h.admitted_after_exit(false));
}

} // namespace

TEST_CASE(
    "H5: downgrade, a v4 holder survives its coordinator, v3 starts, v4 returns (modeled v3)" *
    doctest::test_suite("budget")) {
    run_h5(kPortH5, "h5", nullptr);
}

TEST_CASE("H6: an owner the host cannot attribute stays protected through the handoffs" *
          doctest::test_suite("budget")) {
    Handoff h("h6", kPortH6, Kind::Memory, nullptr);
    h.start_v3();
    h.hold(true);
    h.stop_coordinator();
    h.leave_unattributable();
    h.start_v4();
    refused(h, false, "v4 over an unattributable record");
    h.holder.stop();
    for (int i = 0; i < 12; ++i) {
        CHECK_FALSE(h.asks(false)); // never released: fail closed
        std::this_thread::sleep_for(250ms);
    }
    h.stop_coordinator();
    h.start_v3();
    refused(h, true, "v3 over the same record");
}

TEST_CASE("H8: a pid that was reused while v4 was away is released at the restart" *
          doctest::test_suite("budget")) {
    Handoff h("h8", kPortH6 + 2, Kind::Memory, nullptr);
    h.start_v3();
    h.hold(true);
    h.stop_coordinator();
    rewrite_owner(h.home.journal(h.port), [](Owner3 o) {
        o.started += 7777; // another start stamp: the recorded process is not this one
        return o;
    });
    h.start_v4();
    bool admitted = false;
    for (int i = 0; i < 40 && !admitted; ++i) {
        admitted = h.asks(false);
        std::this_thread::sleep_for(250ms);
    }
    CHECK(admitted);
}

#if defined(__APPLE__)
TEST_CASE("H9: a Darwin owner recorded in the earlier format (namespace 2, microsecond stamp) "
          "is judged by Darwin through the handoffs" *
          doctest::test_suite("budget")) {
    Handoff h("h9", kPortH6 + 3, Kind::Memory, nullptr);
    h.start_v3();
    h.hold(true);
    h.stop_coordinator();
    rewrite_owner(h.home.journal(h.port), [](Owner3 o) {
        o.ns = 2; // what the MAC-1 coordinators wrote
        return o;
    });
    h.start_v4();
    refused(h, false, "v4 over a namespace-2 Darwin record");
    h.stop_coordinator();
    h.start_v3();
    refused(h, true, "v3 over the same record");
    h.stop_coordinator();
    h.start_v4();
    CHECK(h.admitted_after_exit(false));
}
#endif

TEST_CASE("H7: the handoffs again with the real older coordinator (opt-in ANNO_TEST_OLD_COORD)" *
          doctest::test_suite("budget")) {
    const char* old = std::getenv("ANNO_TEST_OLD_COORD");
    if (old == nullptr || old[0] == '\0') {
        MESSAGE("skipping: set ANNO_TEST_OLD_COORD=<an older anno_budget_coord> to run");
        return;
    }
    run_h1(Kind::Memory, kPortH1 + 10, "h7m", old);
    run_h1(Kind::Cpu, kPortH2 + 10, "h7c", old);
    run_h5(kPortH5 + 10, "h7d", old);
}
#endif

// ---- MAP-2 repair 4: the previously published compat-4 coordinator's versioned journal --------
//
// That build kept its promises in <cache>/coord-<port>-v4.journal, which the repaired build
// never reads. Both advertise compat 4, so only the persisted state tells them apart. A start-up
// that finds outstanding or unprovable promises in the predecessor's file refuses, before it
// announces anything, admits anything or rewrites any journal.
//
//   V1/V2 (opt-in ANNO_TEST_V4_COORD + ANNO_TEST_V4_PROBE: the real published binaries) a live
//         memory holder / CPU token of the predecessor is not forgotten: the successor refuses
//         to start, the bytes are unchanged, a client that starts it is told why, and a recovery
//         (the holder gone, the file moved aside) works.
//   V3    the journal-state matrix (absent, empty, released, open memory, open CPU, dead owner,
//         unknown owner, unreadable, malformed, truncated, both journals present, repeated starts).
//   V4    a predecessor journal of another port does not block.
// Ports 47900..47919, a private HOME per case.
#if !defined(_WIN32)
namespace {

constexpr std::uint16_t kPortV1 = 47900;
constexpr std::uint16_t kPortV2 = 47901;
constexpr std::uint16_t kPortV3 = 47902; // + case index, up to 47919
constexpr std::uint16_t kPortV4 = 47920;

void put(const std::filesystem::path& path, const std::string& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

std::string slurp(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    return all.str();
}

/// Every journal file under the home's cache directory, name -> bytes ("<directory>" for a
/// directory). The refusal notice is a new file by design and is not a journal.
std::map<std::string, std::string> journal_snapshot(const Home& home) {
    std::map<std::string, std::string> all;
    const auto dir = home.dir / ".cache" / "anno1503";
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.size() > 8 && name.compare(name.size() - 8, 8, ".refused") == 0) {
            continue;
        }
        all[name] = entry.is_directory() ? "<directory>" : slurp(entry.path());
    }
    return all;
}

bool gone(Child& child, std::chrono::milliseconds limit) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (child.running()) {
        if (std::chrono::steady_clock::now() >= end) {
            return false;
        }
        std::this_thread::sleep_for(20ms);
    }
    return true;
}

struct Start {
    Child coord;
    std::string first;
};

/// Starts the coordinator of this build on the home's default journal and returns whatever its
/// first line is (LISTENING, JOURNAL-REFUSED, ...); no requirement on it.
Start start_unchecked(std::uint16_t port, const Home& home) {
    const auto exe = sibling("anno_budget_coord");
    REQUIRE(std::filesystem::exists(exe));
    Env env(home.vars());
    Start s;
    s.coord = Child::start(exe, {std::to_string(port), "local"});
    REQUIRE(static_cast<bool>(s.coord));
    s.first = s.coord.line(15000ms);
    return s;
}

struct Expect {
    const char* name;
    std::optional<std::string> predecessor; // nullopt: no such file
    bool blocks;
    const char* word; // in the refusal text
};

} // namespace

TEST_CASE("V3: the predecessor journal states: clean ones start, outstanding or unprovable ones "
          "refuse without touching a byte" *
          doctest::test_suite("budget")) {
    const std::string hold = "A 1 7 50 3 999999 1 0\n";
    const std::string cpu = "C 1 3 999999 1\n";
    const std::vector<Expect> cases = {
        {"absent", std::nullopt, false, ""},
        {"empty", std::string(), false, ""},
        {"released", hold + "R 1 7\n" + cpu + "D 1 3 999999 1\n", false, ""},
        {"open-memory", hold, true, "holds promises"},
        {"open-cpu-token", cpu, true, "holds promises"},
        {"known-dead-owner", hold + "R 1 7\n" + hold, true, "holds promises"},
        {"unknown-owner", "A 1 7 50 99 5 5 0\n", true, "holds promises"},
        {"malformed", "this is not a journal\n", true, "malformed or truncated"},
        {"truncated-record", hold + "R 1", true, "malformed or truncated"},
        {"truncated-but-parses", hold + "R 1 7", true, "malformed or truncated"},
        {"released-then-garbage", hold + "R 1 7\n???\n", true, "malformed or truncated"},
    };
    REQUIRE(cases.size() <= 17);
    std::uint16_t port = kPortV3;
    for (const Expect& c : cases) {
        INFO("case " << c.name);
        Home home((std::string("v3-") + c.name).c_str());
        if (c.predecessor) {
            put(home.journal(port, "-v4"), *c.predecessor);
        }
        const auto before = journal_snapshot(home);
        Start s = start_unchecked(port, home);
        if (c.blocks) {
            CHECK(s.first.rfind("JOURNAL-REFUSED", 0) == 0);
            CHECK(has(s.first, c.word));
            CHECK(has(s.first, "Recovery"));
            CHECK(has(s.first, "-v4.journal"));
            CHECK(gone(s.coord, 5000ms));
            CHECK(journal_snapshot(home) == before); // not a byte, and no canonical journal
            CHECK_FALSE(std::filesystem::exists(home.journal(port)));
            // The answer is stable: a repeated start refuses again, still touching nothing.
            for (int i = 0; i < 2; ++i) {
                Start again = start_unchecked(port, home);
                CHECK(again.first.rfind("JOURNAL-REFUSED", 0) == 0);
                CHECK(gone(again.coord, 5000ms));
                CHECK(journal_snapshot(home) == before);
            }
        } else {
            CHECK(s.first.rfind("LISTENING", 0) == 0);
            CHECK(has(s.first, " orphans=0"));
            CHECK(std::filesystem::exists(home.journal(port)));
            if (c.predecessor) {
                CHECK(slurp(home.journal(port, "-v4")) == *c.predecessor); // read, never changed
            }
        }
        port += 1;
    }
    // An unreadable predecessor: a directory where the file belongs, then a file without read
    // permission (skipped for a user who can read it anyway).
    {
        Home home("v3-unreadable-directory");
        std::filesystem::create_directories(home.journal(port, "-v4"));
        Start s = start_unchecked(port, home);
        CHECK(s.first.rfind("JOURNAL-REFUSED", 0) == 0);
        CHECK(has(s.first, "cannot be read"));
        CHECK(gone(s.coord, 5000ms));
        CHECK_FALSE(std::filesystem::exists(home.journal(port)));
        port += 1;
    }
    {
        Home home("v3-unreadable-mode");
        put(home.journal(port, "-v4"), hold + "R 1 7\n");
        std::filesystem::permissions(home.journal(port, "-v4"), std::filesystem::perms::none);
        if (std::ifstream(home.journal(port, "-v4")).good()) {
            MESSAGE("this user can read a mode-000 file; the permission variant is not run");
        } else {
            Start s = start_unchecked(port, home);
            CHECK(s.first.rfind("JOURNAL-REFUSED", 0) == 0);
            CHECK(has(s.first, "cannot be read"));
            CHECK(gone(s.coord, 5000ms));
            CHECK_FALSE(std::filesystem::exists(home.journal(port)));
        }
        std::filesystem::permissions(home.journal(port, "-v4"),
                                     std::filesystem::perms::owner_read |
                                         std::filesystem::perms::owner_write);
        port += 1;
    }
    REQUIRE(port <= kPortV4);
}

TEST_CASE("V3b: both journals present, and the recovery path after a refusal" *
          doctest::test_suite("budget")) {
    const std::string canonical = "A 1 5 30 99 5 5 0\n"; // an unattributable orphan: kept
    const std::string released = "A 1 7 50 3 999999 1 0\nR 1 7\n";
    const std::string open = "A 1 7 50 3 999999 1 0\n";
    constexpr std::uint16_t port = kPortV4 + 1;
    {
        Home home("v3b-released");
        put(home.journal(port), canonical);
        put(home.journal(port, "-v4"), released);
        Start s = start_unchecked(port, home);
        CHECK(s.first.rfind("LISTENING", 0) == 0);
        CHECK(has(s.first, " orphans=1")); // the canonical record is honoured as before
        CHECK(slurp(home.journal(port, "-v4")) == released);
    }
    Home home("v3b-open");
    put(home.journal(port), canonical);
    put(home.journal(port, "-v4"), open);
    const auto before = journal_snapshot(home);
    {
        Start s = start_unchecked(port, home);
        CHECK(s.first.rfind("JOURNAL-REFUSED", 0) == 0);
        CHECK(gone(s.coord, 5000ms));
        CHECK(journal_snapshot(home) == before); // the canonical journal is not rewritten either
    }
    // The documented recovery: the operator has confirmed the owners are gone and moves the file
    // aside. Nothing is deleted or merged.
    std::filesystem::rename(home.journal(port, "-v4"),
                            home.journal(port, "-v4").string() + ".retired");
    Start s = start_unchecked(port, home);
    CHECK(s.first.rfind("LISTENING", 0) == 0);
    CHECK(has(s.first, " orphans=1"));
    CHECK(slurp(home.journal(port, "-v4").string() + ".retired") == open);
    CHECK_FALSE(std::filesystem::exists(home.journal(port).string() + ".refused"));
}

TEST_CASE("V4: a predecessor journal of another port does not block" *
          doctest::test_suite("budget")) {
    Home home("v4-other-port");
    put(home.journal(kPortV4 + 5, "-v4"), "A 1 7 50 3 999999 1 0\n");
    Start s = start_unchecked(kPortV4 + 2, home);
    CHECK(s.first.rfind("LISTENING", 0) == 0);
}

namespace {

/// The real published predecessor: its coordinator and probe, hash-checked by the caller.
void run_predecessor(Kind kind, std::uint16_t port, const char* name, const char* coord_exe,
                     const char* probe_exe) {
    Env cpu_env({{"ANNO_TEST_CPU_BUDGET", "1"}});
    Home home(name);
    const std::string budget_path = (f5_dir() / (std::string(name) + ".budget")).string();
    Vars vars = client_vars(port, name, "30");
    for (const auto& v : home.vars()) {
        vars.push_back(v);
    }
    std::string seen;
    Child old_coord;
    Child holder;
    {
        Env env(vars);
        old_coord = Child::start(coord_exe, {std::to_string(port), "local"});
        REQUIRE(static_cast<bool>(old_coord));
        const std::string listening = old_coord.line(15000ms);
        REQUIRE(listening.rfind("LISTENING", 0) == 0);
        REQUIRE(has(listening, " compat=4"));
        holder = Child::start(probe_exe,
                              {kind == Kind::Memory ? "holder" : "cpu-holder", budget_path, "50"});
        REQUIRE(static_cast<bool>(holder));
        REQUIRE(has(want(holder, "READY", 15000ms, &seen), "READY 1"));
    }
    // The promise is in the versioned journal, and only there.
    const auto v4 = home.journal(port, "-v4");
    REQUIRE(std::filesystem::exists(v4));
    CHECK(slurp(v4).find(kind == Kind::Memory ? "A " : "C ") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(home.journal(port)));
    old_coord.stop(); // only the coordinator this case started; the holder lives on
    REQUIRE(holder.running());
    const std::string bytes = slurp(v4);

    // Direct start: refused before anything is announced or admitted.
    Start s = start_unchecked(port, home);
    CHECK(s.first.rfind("JOURNAL-REFUSED", 0) == 0);
    CHECK(gone(s.coord, 5000ms));
    CHECK(slurp(v4) == bytes);
    CHECK_FALSE(std::filesystem::exists(home.journal(port)));

    // The ordinary client path: a request that starts the coordinator itself is not admitted and
    // its reason names the refusal. Bounded: a short wait, nothing admitted beside the holder.
    if (kind == Kind::Memory) {
        Vars ask = vars;
        for (auto& v : ask) {
            if (v.first == "ANNO_TEST_BUDGET_SPAWN") {
                v.second = "1";
            }
            if (v.first == "ANNO_TEST_ADMIT_WAIT_S") {
                v.second = "8";
            }
        }
        Env env(ask);
        const auto t0 = std::chrono::steady_clock::now();
        Child waiter = probe({"waiter", budget_path, "80"});
        const std::string line = want(waiter, "ADMITTED", 30000ms);
        CHECK(has(line, "ADMITTED 0"));
        CHECK(has(line, "startup refused"));
        CHECK(has(line, "-v4.journal"));
        CHECK(std::chrono::steady_clock::now() - t0 < 20s);
        CHECK(slurp(v4) == bytes);
        CHECK_FALSE(std::filesystem::exists(home.journal(port)));
    }

    // Recovery: the owner is gone (confirmed by the operator), the file is moved aside.
    holder.stop();
    std::filesystem::rename(v4, v4.string() + ".retired");
    Start ok = start_unchecked(port, home);
    CHECK(ok.first.rfind("LISTENING", 0) == 0);
    CHECK(slurp(v4.string() + ".retired") == bytes);
}

} // namespace

TEST_CASE("V1: the real published compat-4 coordinator's live memory holder blocks the successor "
          "(opt-in ANNO_TEST_V4_COORD, ANNO_TEST_V4_PROBE)" *
          doctest::test_suite("budget")) {
    const char* coord = std::getenv("ANNO_TEST_V4_COORD");
    const char* probe_exe = std::getenv("ANNO_TEST_V4_PROBE");
    if (coord == nullptr || coord[0] == '\0' || probe_exe == nullptr || probe_exe[0] == '\0') {
        MESSAGE("skipping: set ANNO_TEST_V4_COORD and ANNO_TEST_V4_PROBE to the published "
                "compat-4 binaries to run");
        return;
    }
    run_predecessor(Kind::Memory, kPortV1, "v1", coord, probe_exe);
}

TEST_CASE("V2: the same for the CPU token (opt-in ANNO_TEST_V4_COORD, ANNO_TEST_V4_PROBE)" *
          doctest::test_suite("budget")) {
    const char* coord = std::getenv("ANNO_TEST_V4_COORD");
    const char* probe_exe = std::getenv("ANNO_TEST_V4_PROBE");
    if (coord == nullptr || coord[0] == '\0' || probe_exe == nullptr || probe_exe[0] == '\0') {
        MESSAGE("skipping: set ANNO_TEST_V4_COORD and ANNO_TEST_V4_PROBE to the published "
                "compat-4 binaries to run");
        return;
    }
    run_predecessor(Kind::Cpu, kPortV2, "v2", coord, probe_exe);
}
#endif
