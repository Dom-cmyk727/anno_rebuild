// Client of the host coordinator for test reservations (coord_server.cpp).
// Windows LockFileEx and Linux flock do not exclude each other, and a PID is
// only meaningful in the OS that created it, so admission lives in one
// process. The shared coordinator binds inside WSL on 0.0.0.0; Windows
// connects to 127.0.0.1. Same-OS unit tests use a loopback port instead.
//
// The coordinator decides; this side only reports. It answers the
// coordinator's residency samples and, for a budget whose clients are the
// meters (unit tests with injected readings), its reading requests from the
// reader thread. A timeout never admits: it pings, and silence for the
// liveness period ends the wait with a refusal.
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <anno/test/parallel.hpp>

#include "admit_detail.hpp"
#include "coord_session.hpp"
#include "coord_wire.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <spawn.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace anno::test {
namespace {

using namespace wire;
using detail::CoordResult;
using detail::CoordSession;

Native connect_loopback(std::uint16_t port) {
    const Native fd = open_socket();
    if (fd == kInvalid) {
        return kInvalid;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(0x7f000001u);
#if defined(_WIN32)
    if (::connect(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&address),
                  sizeof(address)) != 0) {
        ::closesocket(static_cast<SOCKET>(fd));
        return kInvalid;
    }
#else
    if (::connect(static_cast<int>(fd), reinterpret_cast<sockaddr*>(&address), sizeof(address)) !=
        0) {
        ::close(static_cast<int>(fd));
        return kInvalid;
    }
#endif
    set_nodelay(fd);
    return fd;
}

std::uint32_t configured_cpu() {
    std::uint64_t n = detail::env_count("ANNO_TEST_CPU_BUDGET").value_or(0);
    if (n == 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        n = hw == 0 ? 1 : hw;
    }
    if (n > 0xffffffffu) {
        return 0xffffffffu;
    }
    return static_cast<std::uint32_t>(n);
}

std::uint16_t coordinator_port(bool shared) {
    if (const auto port = detail::env_count("ANNO_TEST_BUDGET_PORT")) {
        if (*port > 0 && *port <= 65535) {
            return static_cast<std::uint16_t>(*port);
        }
    }
    return shared ? kSharedPort : kLocalPort;
}

/// ANNO_TEST_BUDGET_SPAWN=0: never start a coordinator, only connect. Tests
/// that run their own coordinator set it so a client cannot start another.
bool may_spawn() {
    const auto n = detail::env_count("ANNO_TEST_BUDGET_SPAWN");
    return !n || *n != 0;
}

std::filesystem::path module_directory() {
#if defined(_WIN32)
    wchar_t buffer[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, 32768);
    if (n == 0 || n >= 32768) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
#else
    char buffer[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        return {};
    }
    buffer[n] = '\0';
    return std::filesystem::path(buffer).parent_path();
#endif
}

std::string hex64(std::uint64_t value);

#if defined(_WIN32)
std::string to_wsl_path(const std::filesystem::path& path) {
    const std::string text = path.generic_string();
    if (text.size() >= 2 && text[1] == ':') {
        const char drive = static_cast<char>(std::tolower(static_cast<unsigned char>(text[0])));
        return std::string("/mnt/") + drive + text.substr(2);
    }
    return text;
}
#endif

bool spawn_local(const std::filesystem::path& exe, std::uint16_t port, bool all_interfaces) {
#if defined(_WIN32)
    // CreateProcessW with bInheritHandles FALSE: no handle is inherited. Our
    // sockets are not inheritable either (open_socket, accept_socket).
    std::wstring command = L"\"" + exe.wstring() + L"\" " + std::to_wstring(port) +
                           (all_interfaces ? L" any" : L" local");
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(exe.c_str(), mutable_command.data(), nullptr, nullptr, FALSE,
                       DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr,
                       nullptr, &startup, &process) == 0) {
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    posix_spawnattr_t attr;
    if (posix_spawnattr_init(&attr) != 0) {
        return false;
    }
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
    // The coordinator is detached: it keeps stdio and no other descriptor of
    // the starting client (T10c F3, D2).
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        posix_spawnattr_destroy(&attr);
        return false;
    }
    close_descriptors_from_3(&actions);
    std::string path = exe.string();
    std::string port_text = std::to_string(port);
    std::string mode = all_interfaces ? "any" : "local";
    char* args[] = {path.data(), port_text.data(), mode.data(), nullptr};
    pid_t child = 0;
    const int rc = posix_spawn(&child, path.c_str(), &actions, &attr, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    return rc == 0;
#endif
}

std::pair<int, std::string> run_captured(const std::string& command) {
    std::string out;
#if defined(_WIN32)
    FILE* pipe = _popen((command + " 2>&1").c_str(), "r");
#else
    FILE* pipe = ::popen((command + " 2>&1").c_str(), "r");
#endif
    if (pipe == nullptr) {
        return {-1, "popen failed"};
    }
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        out += buffer;
    }
#if defined(_WIN32)
    const int rc = _pclose(pipe);
    return {rc, out};
#else
    const int status = ::pclose(pipe);
    if (status == -1) {
        return {-1, out};
    }
    if (WIFEXITED(status)) {
        return {WEXITSTATUS(status), out};
    }
    return {-1, out};
#endif
}

std::filesystem::path host_coordinator_binary_path() {
#if defined(_WIN32)
    const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    return root / "cache" / "coord" / ("anno_budget_coord-" + hex64(coordinator_build()));
#else
    return module_directory() / "anno_budget_coord";
#endif
}

CoordinatorPrepare prepare_host_coordinator_impl() {
    CoordinatorPrepare result;
    result.binary = host_coordinator_binary_path();
    std::error_code error;
#if defined(_WIN32)
    const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    std::filesystem::create_directories(result.binary.parent_path(), error);
    const HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\anno1503-coord-build");
    bool held = false;
    if (mutex != nullptr) {
        const DWORD wait = WaitForSingleObject(mutex, 120000);
        held = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
        if (!held) {
            result.diagnostic = "prepare wait deadline (120 s)";
            CloseHandle(mutex);
            return result;
        }
    }
    const auto release = [&] {
        if (mutex != nullptr) {
            if (held) {
                ReleaseMutex(mutex);
            }
            CloseHandle(mutex);
        }
    };
    if (const char* fail = std::getenv("ANNO_TEST_COORD_PREPARE_FAIL")) {
        if (fail[0] != '\0' && std::string(fail) != "0") {
            result.diagnostic = "ANNO_TEST_COORD_PREPARE_FAIL";
            release();
            return result;
        }
    }
    if (std::filesystem::exists(result.binary, error) &&
        std::filesystem::file_size(result.binary, error) > 0) {
        result.ok = true;
        release();
        return result;
    }
    const std::string id = hex64(coordinator_build());
    const std::filesystem::path source_dir = root / "src" / "test_support" / "src";
    const std::filesystem::path staging =
        result.binary.parent_path() / (result.binary.filename().string() + ".building");
    std::filesystem::remove(staging, error);
    const std::string include = to_wsl_path(root / "src" / "test_support" / "include");
    // Same compiler command as the former ensure_wsl_coordinator path; only the
    // output name is the staging file so a failed or partial compile cannot be
    // the usable artifact.
    const std::string cmd = "wsl.exe -d Ubuntu -- g++ -std=c++20 -O2 -pthread "
                            "-DANNO_COORD_BUILD=0x" +
                            id + "ULL -I" + include + " " +
                            to_wsl_path(source_dir / "budget_coord.cpp") + " " +
                            to_wsl_path(source_dir / "coord_server.cpp") + " " +
                            to_wsl_path(source_dir / "memory.cpp") + " -o " + to_wsl_path(staging);
    const auto [rc, output] = run_captured(cmd);
    result.compiler_exit = rc;
    if (rc != 0) {
        std::filesystem::remove(staging, error);
        std::filesystem::remove(result.binary, error);
        result.diagnostic = "compiler exit " + std::to_string(rc) + "\n" + output;
        release();
        return result;
    }
    if (!std::filesystem::exists(staging, error) ||
        std::filesystem::file_size(staging, error) == 0) {
        std::filesystem::remove(staging, error);
        result.diagnostic = "compiler produced no artifact\n" + output;
        release();
        return result;
    }
    std::filesystem::remove(result.binary, error);
    std::filesystem::rename(staging, result.binary, error);
    if (error || !std::filesystem::exists(result.binary, error) ||
        std::filesystem::file_size(result.binary, error) == 0) {
        std::filesystem::remove(staging, error);
        std::filesystem::remove(result.binary, error);
        result.diagnostic = "rename of completed artifact failed";
        release();
        return result;
    }
    result.ok = true;
    release();
    return result;
#else
    const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    std::filesystem::create_directories(result.binary.parent_path(), error);
    static std::mutex compile_mu;
    const std::lock_guard<std::mutex> in_process(compile_mu);
    const int lock_fd = ::open("/tmp/anno1503-coord-build.lock", O_CREAT | O_RDWR, 0644);
    if (lock_fd >= 0) {
        if (::flock(lock_fd, LOCK_EX) != 0) {
            result.diagnostic = "prepare flock failed";
            ::close(lock_fd);
            return result;
        }
    }
    const auto release = [&] {
        if (lock_fd >= 0) {
            ::flock(lock_fd, LOCK_UN);
            ::close(lock_fd);
        }
    };
    if (const char* fail = std::getenv("ANNO_TEST_COORD_PREPARE_FAIL")) {
        if (fail[0] != '\0' && std::string(fail) != "0") {
            result.diagnostic = "ANNO_TEST_COORD_PREPARE_FAIL";
            release();
            return result;
        }
    }
    if (std::filesystem::exists(result.binary, error) &&
        std::filesystem::file_size(result.binary, error) > 0) {
        result.ok = true;
        release();
        return result;
    }
    const std::string id = hex64(coordinator_build());
    const std::filesystem::path source_dir = root / "src" / "test_support" / "src";
    const std::filesystem::path staging =
        result.binary.parent_path() / (result.binary.filename().string() + ".building");
    std::filesystem::remove(staging, error);
    const std::string include = (root / "src" / "test_support" / "include").generic_string();
    const std::string cmd = "g++ -std=c++20 -O2 -pthread -DANNO_COORD_BUILD=0x" + id + "ULL -I" +
                            include + " " + (source_dir / "budget_coord.cpp").generic_string() +
                            " " + (source_dir / "coord_server.cpp").generic_string() + " " +
                            (source_dir / "memory.cpp").generic_string() + " -o " +
                            staging.generic_string();
    const auto [rc, output] = run_captured(cmd);
    result.compiler_exit = rc;
    if (rc != 0) {
        std::filesystem::remove(staging, error);
        std::filesystem::remove(result.binary, error);
        result.diagnostic = "compiler exit " + std::to_string(rc) + "\n" + output;
        release();
        return result;
    }
    if (!std::filesystem::exists(staging, error) ||
        std::filesystem::file_size(staging, error) == 0) {
        std::filesystem::remove(staging, error);
        result.diagnostic = "compiler produced no artifact\n" + output;
        release();
        return result;
    }
    std::filesystem::remove(result.binary, error);
    std::filesystem::rename(staging, result.binary, error);
    if (error || !std::filesystem::exists(result.binary, error) ||
        std::filesystem::file_size(result.binary, error) == 0) {
        std::filesystem::remove(staging, error);
        std::filesystem::remove(result.binary, error);
        result.diagnostic = "rename of completed artifact failed";
        release();
        return result;
    }
    result.ok = true;
    release();
    return result;
#endif
}

#if defined(_WIN32)
bool ensure_wsl_coordinator(std::uint16_t port) {
    const CoordinatorPrepare prep = prepare_host_coordinator_impl();
    if (!prep.ok) {
        return false;
    }
    const std::filesystem::path binary = prep.binary;
    // wsl.exe stays up for the life of the coordinator. Detach it so this
    // client does not wait, and so the coordinator is not tied to the
    // client's stdin pipe.
    const std::string command =
        "wsl.exe -d Ubuntu -- " + to_wsl_path(binary) + " " + std::to_string(port) + " any";
    std::vector<char> mutable_command(command.begin(), command.end());
    mutable_command.push_back('\0');
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessA(nullptr, mutable_command.data(), nullptr, nullptr, FALSE,
                       DETACHED_PROCESS | CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr,
                       nullptr, &startup, &process) == 0) {
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}
#endif

/// Starts this build's coordinator on `port`, detached. False when it cannot be
/// started at all (the build failed, or the start was refused).
bool start_coordinator_process(bool shared, std::uint16_t port) {
#if defined(_WIN32)
    if (shared) {
        return ensure_wsl_coordinator(port);
    }
    return spawn_local(module_directory() / "anno_budget_coord.exe", port, false);
#else
    return spawn_local(module_directory() / "anno_budget_coord", port, shared);
#endif
}

/// Client settings that are not budget settings: the build id and the
/// compatibility version this client reports. Both default to this build's own
/// values; the environment overrides exist for tests only, and say so in the
/// report (T10c F3, decision "test hooks").
std::uint64_t client_build() {
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_CLIENT_BUILD")) {
        if (env[0] != '\0') {
            return std::strtoull(env, nullptr, 16);
        }
    }
    return coordinator_build();
}

std::uint64_t client_compat() {
    if (const auto value = detail::env_count("ANNO_TEST_BUDGET_CLIENT_COMPAT")) {
        return *value;
    }
    return kCompatVersion;
}

/// ANNO_TEST_BUDGET_EXACT_BUILD=1: this client validates the coordinator's exact
/// build (the gate that tests the coordinator itself). Off by default.
bool exact_build_required() {
    const char* env = std::getenv("ANNO_TEST_BUDGET_EXACT_BUILD");
    return env != nullptr && std::string(env) == "1";
}

bool use_shared_coordinator(bool host_wide) {
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_COORD")) {
        if (env[0] != '\0') {
            const std::string mode = env;
            if (mode == "wsl" || mode == "shared") {
                return true;
            }
            if (mode == "local") {
                return false;
            }
        }
    }
    if (!host_wide) {
        return false;
    }
#if defined(_WIN32)
    static const int cached = std::system("wsl.exe -d Ubuntu -- true") == 0 ? 1 : 0;
    return cached == 1;
#else
    return true;
#endif
}

/// ANNO_TEST_BUDGET_METER=internal: the coordinator measures this budget
/// itself, as for the process budget (a test of that path on its own port).
bool wants_internal_meter(bool host_wide) {
    if (host_wide) {
        return true;
    }
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_METER")) {
        return std::string(env) == "internal";
    }
    return false;
}

struct Pending {
    std::uint64_t req = 0;
    std::deque<Frame> frames;
};

// Lock order: write_mu may be taken first and mu inside it, never the other
// way round. No helper that locks mu is called while mu is held: the
// *_locked helpers below expect the caller to hold it.
struct Session final : CoordSession {
    Native fd = kInvalid;
    std::uint64_t budget = 0;
    std::uint64_t token = 0;
    BudgetHooks hooks{};
    bool meter = false;
    std::mutex write_mu;
    std::mutex mu;
    std::condition_variable cv;
    std::thread reader;
    bool dead = false;
    bool stopping = false;
    std::string lost_reason = "coordinator lost";
    std::chrono::steady_clock::time_point last_rx = std::chrono::steady_clock::now();
    std::uint64_t next_req = 1;
    std::vector<Pending> pending;
    std::vector<detail::Hold> holds;
    detail::ReadingBase base;
    std::vector<std::uint64_t> blocking;

    ~Session() override { close_session(); }

    void close_session() {
        {
            std::lock_guard<std::mutex> lock(mu);
            if (stopping) {
                return;
            }
            stopping = true;
        }
        shutdown_fd(fd);
        if (reader.joinable()) {
            reader.join();
        }
        close_fd(fd);
        fd = kInvalid;
    }
};

void mark_dead_locked(Session& session, const char* why) {
    if (!session.dead) {
        session.dead = true;
        session.lost_reason = why;
    }
    session.cv.notify_all();
}

/// Ends the connection, so the reader thread and every waiting call return.
void mark_dead(Session& session, const char* why) {
    {
        std::lock_guard<std::mutex> lock(session.mu);
        mark_dead_locked(session, why);
    }
    shutdown_fd(session.fd);
}

/// Resident bytes of this client's holds, read now.
std::uint64_t resident_now(Session& session) {
    std::lock_guard<std::mutex> lock(session.mu);
    detail::observe(session.holds, session.base, session.hooks);
    return detail::sum_resident(session.holds);
}

bool send_request(Session& session, Frame& frame, std::uint64_t req) {
    fill_header(frame, frame_op(frame), session.budget, session.token, req);
    bool sent = false;
    {
        std::lock_guard<std::mutex> lock(session.write_mu);
        sent = send_all(session.fd, frame);
    }
    if (!sent) {
        mark_dead(session, "coordinator lost");
    }
    return sent;
}

/// The coordinator's questions, answered on the reader thread so a busy or
/// blocked caller does not delay them. Residency is computed before the free
/// reading so the reading postdates the allocation it credits.
void answer(Session& session, const Frame& frame) {
    const std::uint32_t op = frame_op(frame);
    if (op == kStSample) {
        Frame reply;
        put32(reply.bytes, 4, kOpSampleReply);
        put64(reply.bytes, 40, resident_now(session));
        send_request(session, reply, 0);
        return;
    }
    if (op == kStRefresh && session.meter) {
        const std::uint64_t resident = resident_now(session);
        std::optional<std::uint64_t> free_bytes;
        std::optional<std::uint64_t> total;
        if (session.hooks.free_bytes != nullptr) {
            free_bytes = session.hooks.free_bytes();
        }
        if (session.hooks.total_bytes != nullptr) {
            total = session.hooks.total_bytes();
        }
        Frame reply;
        put32(reply.bytes, 4, kOpMeterReply);
        put64(reply.bytes, 32, frame_a(frame));
        put64(reply.bytes, 40, resident);
        put64(reply.bytes, 48, free_bytes ? *free_bytes : kUnknown);
        put64(reply.bytes, 56, total ? *total : kUnknown);
        send_request(session, reply, 0);
    }
}

void reader_main(Session* session) {
    std::vector<std::uint8_t> incoming;
    while (true) {
        std::uint8_t buffer[kFrameBytes];
        const int n = recv_some(session->fd, buffer, kFrameBytes);
        if (n == -2) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        if (n <= 0) {
            std::lock_guard<std::mutex> lock(session->mu);
            mark_dead_locked(*session, "coordinator lost");
            return;
        }
        incoming.insert(incoming.end(), buffer, buffer + n);
        while (incoming.size() >= kFrameBytes) {
            Frame frame;
            std::memcpy(frame.bytes, incoming.data(), kFrameBytes);
            incoming.erase(incoming.begin(),
                           incoming.begin() + static_cast<std::ptrdiff_t>(kFrameBytes));
            bool question = false;
            {
                std::lock_guard<std::mutex> lock(session->mu);
                if (get32(frame.bytes, 0) != kMagic) {
                    mark_dead_locked(*session, "coordinator lost");
                    return;
                }
                // Any frame shows the coordinator is alive, also the answer
                // to a ping whose request was already forgotten.
                session->last_rx = std::chrono::steady_clock::now();
                const std::uint32_t op = frame_op(frame);
                question = op == kStSample || op == kStRefresh;
                if (!question) {
                    for (Pending& pending : session->pending) {
                        if (pending.req == frame_req(frame)) {
                            pending.frames.push_back(frame);
                        }
                    }
                }
                session->cv.notify_all();
            }
            if (question) {
                answer(*session, frame);
            }
        }
    }
}

enum class Got { Frame, Timeout, Dead };

Got wait_frame(Session& session, std::uint64_t req, std::chrono::milliseconds timeout, Frame& out) {
    std::unique_lock<std::mutex> lock(session.mu);
    // begin_req can reallocate pending while this wait has released mu.
    // Look the request up again under the lock; a pointer would dangle.
    auto find = [&]() -> Pending* {
        for (Pending& one : session.pending) {
            if (one.req == req) {
                return &one;
            }
        }
        return nullptr;
    };
    session.cv.wait_for(lock, timeout, [&] {
        const Pending* pending = find();
        return pending == nullptr || session.dead || !pending->frames.empty();
    });
    Pending* pending = find();
    if (pending != nullptr && !pending->frames.empty()) {
        out = pending->frames.front();
        pending->frames.pop_front();
        return Got::Frame;
    }
    return session.dead ? Got::Dead : Got::Timeout;
}

void forget_pending_locked(Session& session, std::uint64_t req) {
    std::erase_if(session.pending, [&](const Pending& pending) { return pending.req == req; });
}

void forget_pending(Session& session, std::uint64_t req) {
    std::lock_guard<std::mutex> lock(session.mu);
    forget_pending_locked(session, req);
}

std::uint64_t begin_req(Session& session) {
    std::lock_guard<std::mutex> lock(session.mu);
    const std::uint64_t req = session.next_req++;
    session.pending.push_back(Pending{req, {}});
    return req;
}

bool is_dead(Session& session, std::string* why = nullptr) {
    std::lock_guard<std::mutex> lock(session.mu);
    if (session.dead && why != nullptr) {
        *why = session.lost_reason;
    }
    return session.dead;
}

/// No frame since `since` (or since the last one, if later) for longer than
/// the liveness period.
bool silent_too_long(Session& session, std::chrono::steady_clock::time_point since) {
    std::lock_guard<std::mutex> lock(session.mu);
    const auto seen = session.last_rx > since ? session.last_rx : since;
    return std::chrono::steady_clock::now() - seen > liveness();
}

/// Sends a frame whose answer only proves liveness. Its request is forgotten
/// at once; the reader still notes the reply's arrival.
void send_unanswered(Session& session, Frame& frame) {
    const std::uint64_t req = begin_req(session);
    forget_pending(session, req);
    send_request(session, frame, req);
}

/// One request and its single answer, bounded by the liveness period. A dead
/// session sends nothing.
void round_trip(Session& session, Frame& frame) {
    if (is_dead(session)) {
        return;
    }
    const std::uint64_t req = begin_req(session);
    if (send_request(session, frame, req)) {
        Frame reply;
        if (wait_frame(session, req, liveness(), reply) == Got::Timeout) {
            mark_dead(session, "coordinator not responding");
        }
    }
    forget_pending(session, req);
}

Admit admit_from(const Frame& frame, std::uint64_t bytes, bool waited) {
    Admit info;
    const std::uint32_t status = frame_op(frame);
    info.admitted = status == kStAdmit;
    info.refused = status == kStRefuse;
    info.waited = waited;
    info.reason = why_text(frame_g(frame));
    info.estimate = bytes;
    info.available = frame_b(frame);
    info.pending = frame_c(frame);
    info.resident = frame_d(frame);
    info.headroom = frame_e(frame);
    info.free = frame_f(frame);
    info.total = frame_i(frame);
    return info;
}

void remember_hold(Session& session, std::uint64_t id, std::uint64_t bytes) {
    std::lock_guard<std::mutex> lock(session.mu);
    detail::Hold hold;
    hold.id = id;
    hold.bytes = bytes;
    session.holds.push_back(hold);
}

void fill_reserve(Frame& frame, std::uint32_t op, std::uint64_t bytes, std::uint64_t resident,
                  std::uint64_t headroom, std::uint64_t stop) {
    put32(frame.bytes, 4, op);
    put64(frame.bytes, 32, bytes);
    put64(frame.bytes, 40, resident);
    put64(frame.bytes, 56, headroom);
    put64(frame.bytes, 64, stop);
}

CoordResult finish_lost(std::uint64_t bytes, bool waited, const std::string& reason) {
    CoordResult result;
    result.info.refused = true;
    result.info.waited = waited;
    result.info.reason = reason;
    result.info.estimate = bytes;
    return result;
}

std::string hex64(std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::string seconds_text(std::chrono::steady_clock::duration span) {
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(span).count());
}

std::string wait_text(const Admit& last) {
    return " available=" + std::to_string(last.available) + " free=" + std::to_string(last.free) +
           " headroom=" + std::to_string(last.headroom) +
           " pending=" + std::to_string(last.pending) +
           " resident=" + std::to_string(last.resident) + " reason=" + last.reason;
}

CoordResult transact(Session& session, Frame request, std::uint64_t bytes, bool block,
                     bool call_wait) {
    std::string why;
    if (is_dead(session, &why)) {
        return finish_lost(bytes, false, why);
    }
    const std::uint64_t req = begin_req(session);
    auto cleanup = [&] {
        std::lock_guard<std::mutex> lock(session.mu);
        std::erase_if(session.blocking, [&](std::uint64_t id) { return id == req; });
        forget_pending_locked(session, req);
    };
    if (block) {
        std::lock_guard<std::mutex> lock(session.mu);
        session.blocking.push_back(req);
    }
    if (!send_request(session, request, req)) {
        cleanup();
        is_dead(session, &why);
        return finish_lost(bytes, false, why);
    }
    bool waited = false;
    bool saw_wait = false;
    bool deadline_hit = false;
    Admit last;
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + detail::admit_deadline();
    auto next_note = start + std::chrono::seconds(30);
    const auto step = std::min(detail::recheck_period(), liveness() / 2);
    auto note_wait = [&] {
        if (call_wait && !saw_wait) {
            saw_wait = true;
            waited = true;
            if (session.hooks.on_wait != nullptr) {
                session.hooks.on_wait();
            }
        }
    };
    for (;;) {
        Frame reply;
        const Got got = wait_frame(session, req, block ? step : liveness(), reply);
        if (got == Got::Dead) {
            cleanup();
            is_dead(session, &why);
            return finish_lost(bytes, waited, why);
        }
        if (got == Got::Timeout) {
            if (!block || silent_too_long(session, start)) {
                // Nothing at all came back within the liveness period: the
                // coordinator stopped answering. Refuse; never admit.
                mark_dead(session, "coordinator not responding");
                cleanup();
                return finish_lost(bytes, waited, "coordinator not responding");
            }
            note_wait();
            const auto now = std::chrono::steady_clock::now();
            if (!deadline_hit && now >= deadline) {
                // Bounded wait: withdraw the request. If the coordinator
                // admitted it first, that admission is what comes back.
                deadline_hit = true;
                Frame cancel;
                put32(cancel.bytes, 4, kOpCancel);
                put64(cancel.bytes, 32, req);
                send_unanswered(session, cancel);
            }
            if (now >= next_note && session.log) {
                next_note = now + std::chrono::seconds(30);
                session.log("[anno-test memory] still waiting estimate=" + std::to_string(bytes) +
                            " for " + seconds_text(now - start) + " s" + wait_text(last));
            }
            // A ping keeps the liveness check honest and reports this
            // client's residency. Its answer is not awaited here.
            Frame ping;
            put32(ping.bytes, 4, kOpPing);
            put64(ping.bytes, 40, resident_now(session));
            send_unanswered(session, ping);
            continue;
        }
        const std::uint32_t op = frame_op(reply);
        if (op == kStOk) {
            continue;
        }
        if (op == kStWait && block) {
            last = admit_from(reply, bytes, true);
            note_wait();
            continue;
        }
        CoordResult result;
        result.info = admit_from(reply, bytes, waited);
        result.hold_id = frame_a(reply);
        if (result.info.admitted && bytes > 0 && op == kStAdmit) {
            remember_hold(session, result.hold_id, bytes);
        }
        if (deadline_hit && result.info.refused && result.info.reason == "cancelled") {
            // The diagnostic: what the last decision before the deadline saw.
            const std::string seen = wait_text(last);
            result.info = last;
            result.info.admitted = false;
            result.info.refused = true;
            result.info.waited = true;
            result.info.estimate = bytes;
            result.info.reason = "wait deadline";
            if (session.log) {
                session.log("[anno-test memory] deadline estimate=" + std::to_string(bytes) +
                            " after " + seconds_text(std::chrono::steady_clock::now() - start) +
                            " s; last decision" + seen);
            }
        }
        cleanup();
        return result;
    }
}

} // namespace

std::filesystem::path host_coordinator_binary() {
    return host_coordinator_binary_path();
}

CoordinatorPrepare prepare_host_coordinator() {
    return prepare_host_coordinator_impl();
}

namespace {

/// What a hello got back.
enum class Hello {
    Served,  ///< the coordinator accepted this client (an OK frame)
    Refused, ///< the coordinator rejected it (a reject frame, with its reason)
    Silent,  ///< the connection was accepted but no reply came within the liveness bound
    Closed,  ///< the connection closed during the hello
    Foreign, ///< a reply that is not a coordinator's frame: another protocol
};

struct HelloRequest {
    std::uint64_t budget = 0;
    bool internal = false;
    bool meter = false;
    bool exact = false;
    std::uint64_t build = 0;
    std::uint64_t compat = kCompatVersion;
};

Hello exchange_hello(Native fd, const HelloRequest& request, Frame& reply) {
    Frame hello;
    put32(hello.bytes, 0, kMagic);
    put32(hello.bytes, 4, kOpHello);
    put64(hello.bytes, 8, request.budget);
    put64(hello.bytes, 32, this_pid());
    put64(hello.bytes, 40, process_started());
    put64(hello.bytes, 48, this_namespace());
    put64(hello.bytes, 56, configured_cpu());
    put64(hello.bytes, 64, request.build);
    put64(hello.bytes, 72, request.compat);
    std::uint32_t flags = (request.internal ? kFlagInternalMeter : 0u) |
                          (request.meter ? kFlagClientMeter : 0u) |
                          (request.exact ? kFlagExactBuild : 0u);
    put32(hello.bytes, 80, flags);
    put32(hello.bytes, 84, this_domain());
    reply = Frame{};
    if (!send_all(fd, hello)) {
        return Hello::Closed;
    }
    set_recv_timeout(fd, static_cast<int>(liveness().count()));
    switch (recv_reply(fd, reply)) {
    case Recv::Timeout:
        return Hello::Silent;
    case Recv::Closed:
        return Hello::Closed;
    case Recv::Foreign:
        return Hello::Foreign;
    case Recv::Frame:
        break;
    }
    if (frame_op(reply) == kStReject) {
        return Hello::Refused;
    }
    if (frame_op(reply) == kStOk && frame_token(reply) != 0) {
        return Hello::Served;
    }
    return Hello::Foreign;
}

} // namespace

detail::ConnectOutcome detail::coord_connect_once(const ConnectArgs& args,
                                                  ConnectProgress& progress) {
    ConnectOutcome out;
    const bool shared = use_shared_coordinator(args.host_wide);
    const std::uint64_t budget =
        fnv(!args.budget_name.empty() ? args.budget_name : args.identity.generic_string());
    const bool internal = wants_internal_meter(args.host_wide);
    const bool meter = !internal && args.hooks.free_bytes != nullptr;
    const std::uint16_t port = coordinator_port(shared);
    const std::string where = "the test budget coordinator on port " + std::to_string(port);
    Native fd = connect_loopback(port);
    if (fd == kInvalid) {
        out.kind = Attempt::NotReady;
        if (!may_spawn()) {
            out.diagnostic = "no test budget coordinator is listening on port " +
                             std::to_string(port) + " (ANNO_TEST_BUDGET_SPAWN=0)";
            return out;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now < progress.next_spawn) {
            out.diagnostic = "no test budget coordinator is listening on port " +
                             std::to_string(port) + "; the last start was less than 2 s ago";
            return out;
        }
        progress.next_spawn = now + std::chrono::seconds(2);
        if (!start_coordinator_process(shared, port)) {
            out.kind = Attempt::Failed;
            out.diagnostic =
                "could not start the test budget coordinator on port " + std::to_string(port);
            return out;
        }
        out.diagnostic = "no test budget coordinator was listening on port " +
                         std::to_string(port) + "; one was started";
        return out;
    }
    const HelloRequest request{budget,         internal,       meter, exact_build_required(),
                               client_build(), client_compat()};
    Frame reply;
    const Hello result = exchange_hello(fd, request, reply);
    if (result == Hello::Served) {
        if (frame_c(reply) != request.compat) {
            // The coordinator checked the same number; a mismatch here is a defect.
            close_fd(fd);
            out.kind = Attempt::Failed;
            out.diagnostic = where + " speaks compatibility version " +
                             std::to_string(frame_c(reply)) + ", this client speaks " +
                             std::to_string(request.compat);
            return out;
        }
        if (request.exact && frame_a(reply) != request.build) {
            // The coordinator checks this too; a mismatch here is a defect.
            close_fd(fd);
            out.kind = Attempt::Busy;
            out.diagnostic = where + " runs build " + hex64(frame_a(reply)) +
                             ", not the exact build " + hex64(request.build) + " this client needs";
            return out;
        }
        set_recv_timeout(fd, 0);
        auto session = std::shared_ptr<Session>(new Session());
        session->fd = fd;
        session->budget = budget;
        session->token = frame_token(reply);
        session->hooks = args.hooks;
        session->meter = meter;
        session->identity =
            "build=" + hex64(frame_a(reply)) + " compat=" + std::to_string(frame_c(reply)) +
            " incarnation=" + hex64(frame_b(reply)) + " pid=" + std::to_string(frame_d(reply)) +
            " port=" + std::to_string(port) + (shared ? " shared" : " local") +
            (internal ? " meter=internal" : " meter=clients");
        Session* raw = session.get();
        session->reader = std::thread([raw] { reader_main(raw); });
        out.kind = Attempt::Connected;
        out.session = session;
        return out;
    }
    close_fd(fd);
    switch (result) {
    case Hello::Refused: {
        const std::uint32_t reason = frame_h(reply);
        const bool stepped = (frame_g(reply) & kRejectSteppingAside) != 0;
        if (stepped) {
            out.kind = Attempt::NotReady;
            out.diagnostic = where + " (build " + hex64(frame_a(reply)) +
                             ", compatibility version " + std::to_string(frame_c(reply)) +
                             ") was idle and stepped aside; the next attempt starts or finds the "
                             "coordinator of this client";
            return out;
        }
        const char* why = reason == kRejectIncompatible ? "an incompatible compatibility version"
                          : reason == kRejectBuild      ? "an exact build is required"
                                                        : "not accepted";
        out.kind = Attempt::Busy;
        out.diagnostic = "infrastructure blocker: " + where + " runs build " +
                         hex64(frame_a(reply)) + " (compatibility version " +
                         std::to_string(frame_c(reply)) + "), this client is build " +
                         hex64(request.build) + " (compatibility version " +
                         std::to_string(request.compat) + "); the coordinator is busy with " +
                         std::to_string(frame_e(reply)) +
                         " client(s) and does not accept this client (" + why + ")";
        return out;
    }
    case Hello::Silent:
        out.kind = Attempt::NotReady;
        out.diagnostic = where + " accepted the connection but did not answer the hello within " +
                         std::to_string(liveness().count()) + " ms: not ready";
        return out;
    case Hello::Closed:
        out.kind = Attempt::NotReady;
        out.diagnostic = where + " closed the connection during the hello: not ready";
        return out;
    case Hello::Foreign:
    default:
        out.kind = Attempt::Failed;
        out.diagnostic = where + " speaks another protocol";
        return out;
    }
}

detail::CoordResult detail::coord_try(CoordSession& base, std::uint64_t bytes,
                                      std::uint64_t headroom, std::uint64_t stop) {
    auto& session = static_cast<Session&>(base);
    Frame request;
    fill_reserve(request, kOpTry, bytes, resident_now(session), headroom, stop);
    return transact(session, request, bytes, false, false);
}

detail::CoordResult detail::coord_reserve(CoordSession& base, std::uint64_t bytes,
                                          std::uint64_t headroom, std::uint64_t stop) {
    auto& session = static_cast<Session&>(base);
    Frame request;
    fill_reserve(request, kOpReserve, bytes, resident_now(session), headroom, stop);
    return transact(session, request, bytes, true, true);
}

std::uint64_t detail::coord_release(CoordSession& base, std::uint64_t hold_id) noexcept {
    std::uint64_t released = 0;
    try {
        auto& session = static_cast<Session&>(base);
        std::uint64_t resident = 0;
        {
            std::lock_guard<std::mutex> lock(session.mu);
            detail::observe(session.holds, session.base, session.hooks);
            for (const detail::Hold& hold : session.holds) {
                if (hold.id == hold_id) {
                    released = hold.bytes;
                    detail::fold_released(session.base, hold);
                }
            }
            std::erase_if(session.holds,
                          [&](const detail::Hold& hold) { return hold.id == hold_id; });
            resident = detail::sum_resident(session.holds);
        }
        Frame request;
        put32(request.bytes, 4, kOpRelease);
        put64(request.bytes, 32, hold_id);
        put64(request.bytes, 40, resident);
        round_trip(session, request);
    } catch (...) {
    }
    return released;
}

detail::CoordResult detail::coord_cpu_acquire(CoordSession& base) {
    auto& session = static_cast<Session&>(base);
    Frame request;
    put32(request.bytes, 4, kOpCpuAcquire);
    return transact(session, request, 0, true, false);
}

void detail::coord_cpu_release(CoordSession& base) noexcept {
    try {
        auto& session = static_cast<Session&>(base);
        Frame request;
        put32(request.bytes, 4, kOpCpuRelease);
        round_trip(session, request);
    } catch (...) {
    }
}

void detail::coord_cancel(CoordSession& base) {
    auto& session = static_cast<Session&>(base);
    std::vector<std::uint64_t> reqs;
    {
        std::lock_guard<std::mutex> lock(session.mu);
        reqs = session.blocking;
    }
    for (const std::uint64_t blocked : reqs) {
        Frame request;
        put32(request.bytes, 4, kOpCancel);
        put64(request.bytes, 32, blocked);
        round_trip(session, request);
    }
}

void detail::coord_wake(CoordSession& base) {
    auto& session = static_cast<Session&>(base);
    Frame request;
    put32(request.bytes, 4, kOpPing);
    put64(request.bytes, 40, resident_now(session));
    // A wake asks the coordinator to measure again (free memory may have
    // changed outside the budget).
    put32(request.bytes, 80, 1);
    round_trip(session, request);
}

std::uint64_t detail::coord_local_reserved(CoordSession& base) {
    auto& session = static_cast<Session&>(base);
    std::lock_guard<std::mutex> lock(session.mu);
    return detail::sum_reserved(session.holds);
}

} // namespace anno::test
