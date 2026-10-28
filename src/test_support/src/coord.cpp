// Host coordinator for test reservations. Windows LockFileEx and Linux flock
// do not exclude each other, and a PID is only meaningful in the OS that
// created it, so admission lives in one process. The shared coordinator binds
// inside WSL on 0.0.0.0; Windows connects to 127.0.0.1. Same-OS unit tests use
// a loopback port instead. A recv timeout never admits: it only re-reads free
// memory and asks the coordinator to decide again.
#include <algorithm>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <anno/test/parallel.hpp>

#include "admit_detail.hpp"
#include "coord_session.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <unistd.h>
extern char** environ;
#endif

namespace anno::test {
namespace {

using namespace detail;

constexpr std::uint32_t kWireMagic = 0x31434E41u; // 'ANC1'
constexpr std::size_t kFrameBytes = 96;
constexpr std::uint16_t kSharedPort = 47631;
constexpr std::uint16_t kLocalPort = 47632;
constexpr std::uint64_t kUnknown = std::numeric_limits<std::uint64_t>::max();

constexpr std::uint32_t kOpHello = 1;
constexpr std::uint32_t kOpReserve = 2;
constexpr std::uint32_t kOpTry = 3;
constexpr std::uint32_t kOpRelease = 4;
constexpr std::uint32_t kOpCpuAcquire = 5;
constexpr std::uint32_t kOpCpuRelease = 6;
constexpr std::uint32_t kOpCancel = 7;
constexpr std::uint32_t kOpHeartbeat = 8;

constexpr std::uint32_t kStAdmit = 101;
constexpr std::uint32_t kStWait = 102;
constexpr std::uint32_t kStRefuse = 103;
constexpr std::uint32_t kStOk = 104;

constexpr std::uint32_t kWhyAdmitted = 1;
constexpr std::uint32_t kWhyWaiting = 2;
constexpr std::uint32_t kWhyLow = 3;
constexpr std::uint32_t kWhyExceeds = 4;
constexpr std::uint32_t kWhyUnknown = 5;
constexpr std::uint32_t kWhyCancelled = 6;
constexpr std::uint32_t kWhyLost = 7;

using Native = std::uintptr_t;
constexpr Native kInvalid = static_cast<Native>(-1);

struct Frame {
    std::uint8_t bytes[kFrameBytes]{};
};

void put32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

void put64(std::uint8_t* bytes, std::size_t offset, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

std::uint32_t get32(const std::uint8_t* bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes[offset + static_cast<std::size_t>(i)]) << (8 * i);
    }
    return value;
}

std::uint64_t get64(const std::uint8_t* bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<std::size_t>(i)];
    }
    return value;
}

std::uint32_t frame_op(const Frame& frame) {
    return get32(frame.bytes, 4);
}
std::uint64_t frame_budget(const Frame& frame) {
    return get64(frame.bytes, 8);
}
std::uint64_t frame_token(const Frame& frame) {
    return get64(frame.bytes, 16);
}
std::uint64_t frame_req(const Frame& frame) {
    return get64(frame.bytes, 24);
}
std::uint64_t frame_a(const Frame& frame) {
    return get64(frame.bytes, 32);
}
std::uint64_t frame_b(const Frame& frame) {
    return get64(frame.bytes, 40);
}
std::uint64_t frame_c(const Frame& frame) {
    return get64(frame.bytes, 48);
}
std::uint64_t frame_d(const Frame& frame) {
    return get64(frame.bytes, 56);
}
std::uint64_t frame_e(const Frame& frame) {
    return get64(frame.bytes, 64);
}
std::uint32_t frame_g(const Frame& frame) {
    return get32(frame.bytes, 80);
}

void fill_header(Frame& frame, std::uint32_t op, std::uint64_t budget, std::uint64_t token,
                 std::uint64_t req) {
    put32(frame.bytes, 0, kWireMagic);
    put32(frame.bytes, 4, op);
    put64(frame.bytes, 8, budget);
    put64(frame.bytes, 16, token);
    put64(frame.bytes, 24, req);
}

std::uint32_t why_code(const std::string& reason) {
    if (reason == "admitted") {
        return kWhyAdmitted;
    }
    if (reason == "waiting") {
        return kWhyWaiting;
    }
    if (reason == "low-memory stop") {
        return kWhyLow;
    }
    if (reason == "exceeds capacity") {
        return kWhyExceeds;
    }
    if (reason == "free memory unknown") {
        return kWhyUnknown;
    }
    if (reason == "cancelled") {
        return kWhyCancelled;
    }
    return kWhyLost;
}

std::string why_text(std::uint32_t code) {
    switch (code) {
    case kWhyAdmitted:
        return "admitted";
    case kWhyWaiting:
        return "waiting";
    case kWhyLow:
        return "low-memory stop";
    case kWhyExceeds:
        return "exceeds capacity";
    case kWhyUnknown:
        return "free memory unknown";
    case kWhyCancelled:
        return "cancelled";
    default:
        return "coordinator lost";
    }
}

void startup_sockets() {
    static const int ready = [] {
#if defined(_WIN32)
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0 ? 1 : 0;
#else
        ::signal(SIGPIPE, SIG_IGN);
        return 1;
#endif
    }();
    if (ready != 1) {
        throw std::runtime_error("could not start the test budget sockets");
    }
}

int last_socket_error() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool would_block(int error) {
#if defined(_WIN32)
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR;
#else
    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
}

void close_fd(Native fd) {
    if (fd == kInvalid) {
        return;
    }
#if defined(_WIN32)
    ::closesocket(static_cast<SOCKET>(fd));
#else
    ::close(static_cast<int>(fd));
#endif
}

void shutdown_fd(Native fd) {
    if (fd == kInvalid) {
        return;
    }
#if defined(_WIN32)
    ::shutdown(static_cast<SOCKET>(fd), SD_BOTH);
#else
    ::shutdown(static_cast<int>(fd), SHUT_RDWR);
#endif
}

void set_nodelay(Native fd) {
    int on = 1;
#if defined(_WIN32)
    ::setsockopt(static_cast<SOCKET>(fd), IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&on), sizeof(on));
#else
    ::setsockopt(static_cast<int>(fd), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
#endif
}

bool send_all(Native fd, const Frame& frame) {
    std::size_t sent = 0;
    while (sent < kFrameBytes) {
#if defined(_WIN32)
        const int n =
            ::send(static_cast<SOCKET>(fd), reinterpret_cast<const char*>(frame.bytes + sent),
                   static_cast<int>(kFrameBytes - sent), 0);
#else
        const ssize_t n =
            ::send(static_cast<int>(fd), frame.bytes + sent, kFrameBytes - sent, MSG_NOSIGNAL);
#endif
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && would_block(last_socket_error())) {
            continue;
        }
        return false;
    }
    return true;
}

void set_recv_timeout(Native fd, int milliseconds) {
#if defined(_WIN32)
    const DWORD timeout = static_cast<DWORD>(milliseconds);
    ::setsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    ::setsockopt(static_cast<int>(fd), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
}

bool recv_full(Native fd, Frame& frame) {
    std::size_t got = 0;
    while (got < kFrameBytes) {
#if defined(_WIN32)
        const int n = ::recv(static_cast<SOCKET>(fd), reinterpret_cast<char*>(frame.bytes + got),
                             static_cast<int>(kFrameBytes - got), 0);
#else
        const ssize_t n = ::recv(static_cast<int>(fd), frame.bytes + got, kFrameBytes - got, 0);
#endif
        if (n > 0) {
            got += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && would_block(last_socket_error())) {
            continue;
        }
        return false;
    }
    return get32(frame.bytes, 0) == kWireMagic;
}

// Positive: bytes read. 0: the peer closed. -2: no byte is ready yet.
int recv_some(Native fd, std::uint8_t* dest, std::size_t cap) {
#if defined(_WIN32)
    const int n =
        ::recv(static_cast<SOCKET>(fd), reinterpret_cast<char*>(dest), static_cast<int>(cap), 0);
#else
    const ssize_t n = ::recv(static_cast<int>(fd), dest, cap, 0);
#endif
    if (n < 0 && would_block(last_socket_error())) {
        return -2;
    }
    return static_cast<int>(n);
}

Native connect_loopback(std::uint16_t port) {
    startup_sockets();
#if defined(_WIN32)
    const SOCKET fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        return kInvalid;
    }
#else
    const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return kInvalid;
    }
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(0x7f000001u);
#if defined(_WIN32)
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::closesocket(fd);
        return kInvalid;
    }
    set_nodelay(static_cast<Native>(fd));
    return static_cast<Native>(fd);
#else
    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return kInvalid;
    }
    set_nodelay(static_cast<Native>(fd));
    return static_cast<Native>(fd);
#endif
}

std::uint64_t fnv(std::string_view text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const char raw : text) {
        hash ^= static_cast<unsigned char>(raw);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

std::uint64_t this_pid() {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::uint32_t this_namespace() {
#if defined(_WIN32)
    return 1;
#else
    return 2;
#endif
}

std::uint64_t process_started() {
#if defined(_WIN32)
    FILETIME created{};
    FILETIME exited{};
    FILETIME kernel{};
    FILETIME user{};
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) == 0) {
        return 0;
    }
    ULARGE_INTEGER stamp{};
    stamp.LowPart = created.dwLowDateTime;
    stamp.HighPart = created.dwHighDateTime;
    return stamp.QuadPart;
#else
    const int fd = ::open("/proc/self/stat", O_RDONLY);
    if (fd < 0) {
        return 0;
    }
    char buffer[1024];
    const ssize_t n = ::read(fd, buffer, sizeof(buffer));
    ::close(fd);
    if (n <= 0) {
        return 0;
    }
    const std::string text(buffer, static_cast<std::size_t>(n));
    const auto mark = text.rfind(')');
    if (mark == std::string::npos || mark + 2 >= text.size()) {
        return 0;
    }
    // starttime is the 20th field after comm (field 2), i.e. field 22.
    std::uint64_t start = 0;
    std::size_t index = mark + 2;
    for (int field = 0; field < 19; ++field) {
        while (index < text.size() && text[index] == ' ') {
            ++index;
        }
        while (index < text.size() && text[index] != ' ') {
            ++index;
        }
    }
    while (index < text.size() && text[index] == ' ') {
        ++index;
    }
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        start = start * 10 + static_cast<std::uint64_t>(text[index] - '0');
        ++index;
    }
    return start;
#endif
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

std::uint64_t recheck_ms() {
    return static_cast<std::uint64_t>(detail::recheck_period().count());
}

/// How long a client waits for any frame from the coordinator before it
/// treats the connection as lost (ANNO_TEST_BUDGET_LIVENESS_MS, default 10 s).
/// A waiting client pings every recheck period and the coordinator answers at
/// once, so silence this long means it has stopped or hung.
std::chrono::milliseconds liveness() {
    std::uint64_t n = detail::env_count("ANNO_TEST_BUDGET_LIVENESS_MS").value_or(10000);
    if (n < 100) {
        n = 100;
    }
    return std::chrono::milliseconds(n > 600000 ? 600000 : n);
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
    std::string path = exe.string();
    std::string port_text = std::to_string(port);
    std::string mode = all_interfaces ? "any" : "local";
    char* args[] = {path.data(), port_text.data(), mode.data(), nullptr};
    pid_t child = 0;
    const int rc = posix_spawn(&child, path.c_str(), nullptr, &attr, args, environ);
    posix_spawnattr_destroy(&attr);
    return rc == 0;
#endif
}

#if defined(_WIN32)
bool ensure_wsl_coordinator(std::uint16_t port) {
    const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    const std::filesystem::path binary = root / "cache" / "coord" / "anno_budget_coord";
    std::error_code error;
    std::filesystem::create_directories(binary.parent_path(), error);
    const HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\anno1503-coord-build");
    if (mutex != nullptr) {
        WaitForSingleObject(mutex, 120000);
    }
    const auto fresh = [&](const std::filesystem::path& source) {
        std::error_code stamp_error;
        if (!std::filesystem::exists(binary, stamp_error)) {
            return false;
        }
        const auto source_time = std::filesystem::last_write_time(source, stamp_error);
        const auto binary_time = std::filesystem::last_write_time(binary, stamp_error);
        return !stamp_error && binary_time >= source_time;
    };
    const std::filesystem::path source_dir = root / "src" / "test_support" / "src";
    const bool up_to_date = fresh(source_dir / "coord.cpp") &&
                            fresh(source_dir / "budget_coord.cpp") &&
                            fresh(source_dir / "admit_detail.hpp");
    bool compiled = up_to_date;
    if (!up_to_date) {
        const std::string include = to_wsl_path(root / "src" / "test_support" / "include");
        const std::string out = to_wsl_path(binary);
        const std::string cmd = "wsl.exe -d Ubuntu -- g++ -std=c++20 -O2 -pthread -I" + include +
                                " " + to_wsl_path(source_dir / "budget_coord.cpp") + " " +
                                to_wsl_path(source_dir / "coord.cpp") + " -o " + out;
        compiled = std::system(cmd.c_str()) == 0;
    }
    if (mutex != nullptr) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    if (!compiled) {
        return false;
    }
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

bool wait_until_connected(std::uint16_t port, int attempts, Native& out) {
    for (int i = 0; i < attempts; ++i) {
        out = connect_loopback(port);
        if (out != kInvalid) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

Native connect_coordinator(bool shared) {
    const std::uint16_t port = coordinator_port(shared);
    Native fd = connect_loopback(port);
    if (fd != kInvalid) {
        return fd;
    }
    if (!may_spawn()) {
        throw std::runtime_error("no test budget coordinator on port " + std::to_string(port) +
                                 " and ANNO_TEST_BUDGET_SPAWN=0");
    }
#if defined(_WIN32)
    if (shared) {
        if (!ensure_wsl_coordinator(port)) {
            throw std::runtime_error("could not start the WSL test budget coordinator");
        }
    } else {
        const auto exe = module_directory() / "anno_budget_coord.exe";
        if (!spawn_local(exe, port, false)) {
            throw std::runtime_error("could not start the local test budget coordinator at " +
                                     exe.string());
        }
    }
#else
    const auto exe = module_directory() / "anno_budget_coord";
    if (!spawn_local(exe, port, shared)) {
        throw std::runtime_error("could not start the local test budget coordinator at " +
                                 exe.string());
    }
#endif
    if (!wait_until_connected(port, shared ? 150 : 30, fd)) {
        throw std::runtime_error("the test budget coordinator did not accept a connection");
    }
    return fd;
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

struct RemoteHold {
    std::uint64_t id = 0;
    std::uint64_t bytes = 0;
};

struct WaitReq {
    std::uint64_t seq = 0;
    std::uint64_t req = 0;
    std::uint64_t bytes = 0;
    std::uint64_t headroom = 0;
    std::uint64_t stop = 0;
    bool free_known = false;
    std::uint64_t free = 0;
    bool cpu = false;
};

struct SClient {
    Native fd = kInvalid;
    bool hello = false;
    std::uint64_t token = 0;
    std::uint64_t budget = 0;
    std::uint32_t ns = 0;
    std::uint64_t pid = 0;
    std::uint64_t started = 0;
    std::vector<RemoteHold> holds;
    std::uint64_t resident = 0;
    std::uint32_t cpu = 0;
    std::vector<WaitReq> waiting;
    std::vector<std::uint64_t> cancelled;
    std::vector<std::uint8_t> incoming;
};

struct SBudget {
    std::uint64_t id = 0;
    std::uint32_t cpu_budget = 0;
    std::uint32_t cpu_in_use = 0;
    std::uint64_t next_hold = 1;
    std::uint64_t next_seq = 1;
};

struct Server {
    std::vector<SClient> clients;
    std::vector<SBudget> budgets;
};

SBudget* find_budget(Server& server, std::uint64_t id, bool create) {
    for (SBudget& budget : server.budgets) {
        if (budget.id == id) {
            return &budget;
        }
    }
    if (!create) {
        return nullptr;
    }
    SBudget created;
    created.id = id;
    server.budgets.push_back(created);
    return &server.budgets.back();
}

std::uint64_t client_reserved(const SClient& client) {
    std::uint64_t sum = 0;
    for (const RemoteHold& hold : client.holds) {
        sum = detail::sat_add(sum, hold.bytes);
    }
    return sum;
}

Frame decision_frame(std::uint32_t status, const SClient& client, std::uint64_t req,
                     std::uint64_t hold, const Admit& info) {
    Frame frame;
    fill_header(frame, status, client.budget, client.token, req);
    put64(frame.bytes, 32, hold);
    put64(frame.bytes, 40, info.available);
    put64(frame.bytes, 48, info.pending);
    put64(frame.bytes, 56, info.resident);
    put64(frame.bytes, 64, info.headroom);
    put32(frame.bytes, 80, why_code(info.reason));
    return frame;
}

void send_frame(SClient& client, const Frame& frame) {
    if (client.fd == kInvalid) {
        return;
    }
    if (!send_all(client.fd, frame)) {
        client.fd = kInvalid;
    }
}

View view_for(Server& server, const SClient& self, const WaitReq& wait) {
    View view;
    view.headroom = wait.headroom;
    view.stop_below = wait.stop;
    view.free_known = wait.free_known;
    view.free = wait.free;
    view.self_seq = wait.seq;
    std::uint64_t oldest_seq = 0;
    std::uint64_t oldest_bytes = 0;
    for (const SClient& client : server.clients) {
        if (client.budget != self.budget) {
            continue;
        }
        const std::uint64_t reserved = client_reserved(client);
        std::uint64_t resident = client.resident;
        if (resident > reserved) {
            resident = reserved;
        }
        view.resident = detail::sat_add(view.resident, resident);
        view.pending = detail::sat_add(view.pending, reserved - resident);
        for (const WaitReq& other : client.waiting) {
            if (other.cpu || other.seq == 0) {
                continue;
            }
            if (oldest_seq == 0 || other.seq < oldest_seq) {
                oldest_seq = other.seq;
                oldest_bytes = other.bytes;
            }
        }
    }
    view.oldest_seq = oldest_seq;
    view.oldest_bytes = oldest_bytes;
    return view;
}

struct WaitRef {
    std::size_t client = 0;
    std::uint64_t req = 0;
    std::uint64_t seq = 0;
    bool cpu = false;
};

void pump(Server& server, std::uint64_t budget_id) {
    auto collect = [&](bool cpu) {
        std::vector<WaitRef> refs;
        for (std::size_t i = 0; i < server.clients.size(); ++i) {
            const SClient& client = server.clients[i];
            if (client.budget != budget_id || client.fd == kInvalid) {
                continue;
            }
            for (const WaitReq& wait : client.waiting) {
                if (wait.cpu == cpu) {
                    refs.push_back(WaitRef{i, wait.req, wait.seq, wait.cpu});
                }
            }
        }
        std::sort(refs.begin(), refs.end(),
                  [](const WaitRef& left, const WaitRef& right) { return left.seq < right.seq; });
        return refs;
    };
    auto finish = [&](std::size_t index, std::uint64_t req, std::uint32_t status,
                      std::uint64_t hold, const Admit& info) {
        SClient& client = server.clients[index];
        send_frame(client, decision_frame(status, client, req, hold, info));
        std::erase_if(client.cancelled, [&](std::uint64_t id) { return id == req; });
        std::erase_if(client.waiting, [&](const WaitReq& wait) { return wait.req == req; });
    };
    bool progressed = true;
    while (progressed) {
        progressed = false;
        SBudget* budget = find_budget(server, budget_id, false);
        if (budget == nullptr) {
            return;
        }
        for (const WaitRef& ref : collect(false)) {
            if (ref.client >= server.clients.size()) {
                break;
            }
            SClient& client = server.clients[ref.client];
            WaitReq* wait = nullptr;
            for (WaitReq& one : client.waiting) {
                if (one.req == ref.req) {
                    wait = &one;
                    break;
                }
            }
            if (wait == nullptr) {
                continue;
            }
            const bool cancelled = std::find(client.cancelled.begin(), client.cancelled.end(),
                                             wait->req) != client.cancelled.end();
            if (cancelled) {
                Admit info;
                info.refused = true;
                info.reason = "cancelled";
                info.estimate = wait->bytes;
                finish(ref.client, wait->req, kStRefuse, 0, info);
                progressed = true;
                break;
            }
            Admit info;
            const Decision decision = decide(view_for(server, client, *wait), wait->bytes, info);
            if (decision == Decision::Wait) {
                // Leave it queued. A later waiter may still fit beside it.
                // Nothing here admits just because a poll timed out.
                continue;
            }
            std::uint64_t hold_id = 0;
            if (decision == Decision::Admit) {
                RemoteHold hold;
                hold.id = budget->next_hold++;
                hold.bytes = wait->bytes;
                hold_id = hold.id;
                client.holds.push_back(hold);
                finish(ref.client, ref.req, kStAdmit, hold_id, info);
            } else {
                finish(ref.client, ref.req, kStRefuse, 0, info);
            }
            progressed = true;
            break;
        }
        if (progressed) {
            continue;
        }
        const std::vector<WaitRef> cpu = collect(true);
        if (cpu.empty()) {
            return;
        }
        const WaitRef& ref = cpu.front();
        if (ref.client >= server.clients.size()) {
            return;
        }
        SClient& client = server.clients[ref.client];
        const bool cancelled = std::find(client.cancelled.begin(), client.cancelled.end(),
                                         ref.req) != client.cancelled.end();
        if (cancelled) {
            Admit info;
            info.refused = true;
            info.reason = "cancelled";
            finish(ref.client, ref.req, kStRefuse, 0, info);
            progressed = true;
            continue;
        }
        if (budget->cpu_budget != 0 && budget->cpu_in_use >= budget->cpu_budget) {
            return;
        }
        budget->cpu_in_use += 1;
        client.cpu += 1;
        Admit info;
        info.admitted = true;
        info.reason = "admitted";
        finish(ref.client, ref.req, kStAdmit, 0, info);
        progressed = true;
    }
}

void note_resident(SClient& client, std::uint64_t reported) {
    const std::uint64_t reserved = client_reserved(client);
    client.resident = reported > reserved ? reserved : reported;
}

bool take_cancel(SClient& client, std::uint64_t req) {
    const auto found = std::find(client.cancelled.begin(), client.cancelled.end(), req);
    if (found == client.cancelled.end()) {
        return false;
    }
    client.cancelled.erase(found);
    return true;
}

void handle_frame(Server& server, SClient& client, const Frame& frame) {
    if (get32(frame.bytes, 0) != kWireMagic) {
        client.fd = kInvalid;
        return;
    }
    const std::uint32_t op = frame_op(frame);
    if (!client.hello) {
        if (op != kOpHello) {
            client.fd = kInvalid;
            return;
        }
        client.hello = true;
        client.budget = frame_budget(frame);
        client.ns = static_cast<std::uint32_t>(frame_c(frame));
        client.pid = frame_a(frame);
        client.started = frame_b(frame);
        std::uint64_t token = 0;
        while (token == 0) {
            token =
                (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}();
        }
        client.token = token;
        SBudget* budget = find_budget(server, client.budget, true);
        if (budget->cpu_budget == 0) {
            budget->cpu_budget = static_cast<std::uint32_t>(frame_d(frame));
        }
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        send_frame(client, reply);
        return;
    }
    if (frame_token(frame) != client.token || frame_budget(frame) != client.budget) {
        client.fd = kInvalid;
        return;
    }
    SBudget* budget = find_budget(server, client.budget, true);
    if (op == kOpHeartbeat) {
        note_resident(client, frame_b(frame));
        const bool known = frame_c(frame) != kUnknown;
        for (WaitReq& wait : client.waiting) {
            if (!wait.cpu) {
                wait.free_known = known;
                wait.free = known ? frame_c(frame) : 0;
            }
        }
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        send_frame(client, reply);
        pump(server, client.budget);
        return;
    }
    if (op == kOpCancel) {
        client.cancelled.push_back(frame_a(frame));
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        send_frame(client, reply);
        pump(server, client.budget);
        return;
    }
    if (op == kOpRelease) {
        const std::uint64_t id = frame_a(frame);
        std::erase_if(client.holds, [&](const RemoteHold& hold) { return hold.id == id; });
        note_resident(client, frame_b(frame));
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        send_frame(client, reply);
        pump(server, client.budget);
        return;
    }
    if (op == kOpCpuRelease) {
        if (client.cpu > 0) {
            client.cpu -= 1;
        }
        if (budget->cpu_in_use > 0) {
            budget->cpu_in_use -= 1;
        }
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        send_frame(client, reply);
        pump(server, client.budget);
        return;
    }
    if (op == kOpCpuAcquire) {
        if (take_cancel(client, frame_req(frame))) {
            Admit info;
            info.refused = true;
            info.reason = "cancelled";
            send_frame(client, decision_frame(kStRefuse, client, frame_req(frame), 0, info));
            return;
        }
        if (budget->cpu_budget == 0 || budget->cpu_in_use < budget->cpu_budget) {
            budget->cpu_in_use += 1;
            client.cpu += 1;
            Admit info;
            info.admitted = true;
            info.reason = "admitted";
            send_frame(client, decision_frame(kStAdmit, client, frame_req(frame), 0, info));
            return;
        }
        WaitReq wait;
        wait.seq = budget->next_seq++;
        wait.req = frame_req(frame);
        wait.cpu = true;
        client.waiting.push_back(wait);
        Frame reply;
        fill_header(reply, kStWait, client.budget, client.token, frame_req(frame));
        send_frame(client, reply);
        return;
    }
    if (op != kOpReserve && op != kOpTry) {
        return;
    }
    note_resident(client, frame_b(frame));
    WaitReq wait;
    wait.req = frame_req(frame);
    wait.bytes = frame_a(frame);
    wait.headroom = frame_d(frame);
    wait.stop = frame_e(frame);
    wait.free_known = frame_c(frame) != kUnknown;
    wait.free = wait.free_known ? frame_c(frame) : 0;
    if (take_cancel(client, wait.req)) {
        Admit info;
        info.refused = true;
        info.reason = "cancelled";
        info.estimate = wait.bytes;
        send_frame(client, decision_frame(kStRefuse, client, wait.req, 0, info));
        return;
    }
    Admit info;
    const Decision decision = decide(view_for(server, client, wait), wait.bytes, info);
    if (decision == Decision::Admit) {
        RemoteHold hold;
        hold.id = budget->next_hold++;
        hold.bytes = wait.bytes;
        client.holds.push_back(hold);
        send_frame(client, decision_frame(kStAdmit, client, wait.req, hold.id, info));
        return;
    }
    if (decision == Decision::Refuse || op == kOpTry) {
        const std::uint32_t status = decision == Decision::Refuse ? kStRefuse : kStWait;
        send_frame(client, decision_frame(status, client, wait.req, 0, info));
        return;
    }
    wait.seq = budget->next_seq++;
    client.waiting.push_back(wait);
    Frame queued;
    fill_header(queued, kStWait, client.budget, client.token, wait.req);
    put32(queued.bytes, 80, kWhyWaiting);
    send_frame(client, queued);
}

void erase_client(Server& server, std::size_t index) {
    if (index >= server.clients.size()) {
        return;
    }
    SClient& client = server.clients[index];
    const std::uint64_t budget_id = client.budget;
    if (client.cpu > 0) {
        if (SBudget* budget = find_budget(server, budget_id, false)) {
            if (budget->cpu_in_use >= client.cpu) {
                budget->cpu_in_use -= client.cpu;
            } else {
                budget->cpu_in_use = 0;
            }
        }
    }
    close_fd(client.fd);
    client.fd = kInvalid;
    server.clients.erase(server.clients.begin() + static_cast<std::ptrdiff_t>(index));
    if (budget_id != 0) {
        pump(server, budget_id);
    }
}

void accept_client(Server& server, Native listen_fd) {
#if defined(_WIN32)
    const SOCKET fd = ::accept(static_cast<SOCKET>(listen_fd), nullptr, nullptr);
    if (fd == INVALID_SOCKET) {
        return;
    }
    const Native native = static_cast<Native>(fd);
#else
    const int fd = ::accept(static_cast<int>(listen_fd), nullptr, nullptr);
    if (fd < 0) {
        return;
    }
    const Native native = static_cast<Native>(fd);
#endif
    set_nodelay(native);
    SClient client;
    client.fd = native;
    server.clients.push_back(std::move(client));
}

void pull_client(Server& server, std::size_t index) {
    SClient& client = server.clients[index];
    std::uint8_t buffer[kFrameBytes];
    const std::size_t need = kFrameBytes - client.incoming.size();
    const int n = recv_some(client.fd, buffer, need);
    if (n == -2) {
        return;
    }
    if (n <= 0) {
        erase_client(server, index);
        return;
    }
    client.incoming.insert(client.incoming.end(), buffer, buffer + n);
    if (client.incoming.size() < kFrameBytes) {
        return;
    }
    Frame frame;
    std::memcpy(frame.bytes, client.incoming.data(), kFrameBytes);
    client.incoming.clear();
    handle_frame(server, server.clients[index], frame);
    if (index < server.clients.size() && server.clients[index].fd == kInvalid) {
        erase_client(server, index);
    }
}

Native listen_on(std::uint16_t port, bool all_interfaces) {
    startup_sockets();
#if defined(_WIN32)
    const SOCKET fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        return kInvalid;
    }
#else
    const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return kInvalid;
    }
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(all_interfaces ? 0u : 0x7f000001u);
#if defined(_WIN32)
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(fd, 128) != 0) {
        ::closesocket(fd);
        return kInvalid;
    }
    return static_cast<Native>(fd);
#else
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(fd, 128) != 0) {
        ::close(fd);
        return kInvalid;
    }
    return static_cast<Native>(fd);
#endif
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

struct Snap {
    std::uint64_t resident = 0;
    std::optional<std::uint64_t> free;
};

Snap snapshot(Session& session) {
    std::lock_guard<std::mutex> lock(session.mu);
    detail::observe(session.holds, session.base, session.hooks);
    Snap snap;
    snap.resident = detail::sum_resident(session.holds);
    if (session.hooks.free_bytes != nullptr) {
        snap.free = session.hooks.free_bytes();
    }
    return snap;
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
            std::lock_guard<std::mutex> lock(session->mu);
            if (get32(frame.bytes, 0) != kWireMagic) {
                mark_dead_locked(*session, "coordinator lost");
                return;
            }
            // Any frame shows the coordinator is alive, also the answer to a
            // ping whose request was already forgotten.
            session->last_rx = std::chrono::steady_clock::now();
            for (Pending& pending : session->pending) {
                if (pending.req == frame_req(frame)) {
                    pending.frames.push_back(frame);
                }
            }
            session->cv.notify_all();
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
    if (status == kStWait && info.reason.empty()) {
        info.reason = "waiting";
    }
    return info;
}

void remember_hold(Session& session, std::uint64_t id, std::uint64_t bytes) {
    std::lock_guard<std::mutex> lock(session.mu);
    detail::Hold hold;
    hold.id = id;
    hold.bytes = bytes;
    session.holds.push_back(hold);
}

void fill_reserve(Frame& frame, std::uint32_t op, std::uint64_t bytes, const Snap& snap,
                  std::uint64_t headroom, std::uint64_t stop) {
    put32(frame.bytes, 4, op);
    put64(frame.bytes, 32, bytes);
    put64(frame.bytes, 40, snap.resident);
    put64(frame.bytes, 48, snap.free ? *snap.free : kUnknown);
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

std::string seconds_text(std::chrono::steady_clock::duration span) {
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(span).count());
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
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + detail::admit_deadline();
    auto next_note = start + std::chrono::seconds(30);
    const auto step = std::min(std::chrono::milliseconds(recheck_ms()), liveness() / 2);
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
            if (call_wait && !saw_wait) {
                saw_wait = true;
                waited = true;
                if (session.hooks.on_wait != nullptr) {
                    session.hooks.on_wait();
                }
            }
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
                            " for " + seconds_text(now - start) + " s");
            }
            // A ping: the coordinator re-decides with a fresh reading, and
            // its answer shows it is alive. The answer is not awaited here.
            const Snap snap = snapshot(session);
            Frame beat;
            put32(beat.bytes, 4, kOpHeartbeat);
            put64(beat.bytes, 40, snap.resident);
            put64(beat.bytes, 48, snap.free ? *snap.free : kUnknown);
            send_unanswered(session, beat);
            continue;
        }
        const std::uint32_t op = frame_op(reply);
        if (op == kStOk) {
            continue;
        }
        if (op == kStWait && block) {
            if (call_wait && !saw_wait) {
                saw_wait = true;
                waited = true;
                if (session.hooks.on_wait != nullptr) {
                    session.hooks.on_wait();
                }
            }
            continue;
        }
        CoordResult result;
        result.info = admit_from(reply, bytes, waited);
        result.hold_id = frame_a(reply);
        if (result.info.admitted && bytes > 0 && op == kStAdmit) {
            remember_hold(session, result.hold_id, bytes);
        }
        if (deadline_hit && result.info.refused && result.info.reason == "cancelled") {
            result.info.reason = "wait deadline";
        }
        cleanup();
        return result;
    }
}

} // namespace

std::shared_ptr<CoordSession> detail::coord_connect(const std::filesystem::path& identity,
                                                    const std::string& budget_name, bool host_wide,
                                                    BudgetHooks hooks) {
    const bool shared = use_shared_coordinator(host_wide);
    const std::uint64_t budget =
        fnv(!budget_name.empty() ? budget_name : identity.generic_string());
    Native fd = connect_coordinator(shared);
    Frame hello;
    put32(hello.bytes, 0, kWireMagic);
    put32(hello.bytes, 4, kOpHello);
    put64(hello.bytes, 8, budget);
    put64(hello.bytes, 32, this_pid());
    put64(hello.bytes, 40, process_started());
    put64(hello.bytes, 48, this_namespace());
    put64(hello.bytes, 56, configured_cpu());
    if (!send_all(fd, hello)) {
        close_fd(fd);
        throw std::runtime_error("could not register with the test budget coordinator");
    }
    set_recv_timeout(fd, static_cast<int>(liveness().count()));
    Frame reply;
    if (!recv_full(fd, reply) || frame_op(reply) != kStOk || frame_token(reply) == 0) {
        close_fd(fd);
        throw std::runtime_error("the test budget coordinator rejected the client");
    }
    set_recv_timeout(fd, 0);
    auto session = std::shared_ptr<Session>(new Session());
    session->fd = fd;
    session->budget = budget;
    session->token = frame_token(reply);
    session->hooks = hooks;
    Session* raw = session.get();
    session->reader = std::thread([raw] { reader_main(raw); });
    return session;
}

detail::CoordResult detail::coord_try(CoordSession& base, std::uint64_t bytes,
                                      std::uint64_t headroom, std::uint64_t stop) {
    auto& session = static_cast<Session&>(base);
    const Snap snap = snapshot(session);
    Frame request;
    fill_reserve(request, kOpTry, bytes, snap, headroom, stop);
    return transact(session, request, bytes, false, false);
}

detail::CoordResult detail::coord_reserve(CoordSession& base, std::uint64_t bytes,
                                          std::uint64_t headroom, std::uint64_t stop) {
    auto& session = static_cast<Session&>(base);
    const Snap snap = snapshot(session);
    Frame request;
    fill_reserve(request, kOpReserve, bytes, snap, headroom, stop);
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
    const Snap snap = snapshot(session);
    Frame request;
    put32(request.bytes, 4, kOpHeartbeat);
    put64(request.bytes, 40, snap.resident);
    put64(request.bytes, 48, snap.free ? *snap.free : kUnknown);
    round_trip(session, request);
}

std::uint64_t detail::coord_local_reserved(CoordSession& base) {
    auto& session = static_cast<Session&>(base);
    std::lock_guard<std::mutex> lock(session.mu);
    return detail::sum_reserved(session.holds);
}

int run_budget_server(std::uint16_t port, bool all_interfaces) {
#if !defined(_WIN32)
    for (int fd = 3; fd < 256; ++fd) {
        ::close(fd);
    }
#endif
    const Native listen_fd = listen_on(port, all_interfaces);
    if (listen_fd == kInvalid) {
        std::printf("BIND-FAILED %u\n", static_cast<unsigned>(port));
        std::fflush(stdout);
        return 3;
    }
    // A test that starts its own coordinator waits for this line.
    std::printf("LISTENING %u\n", static_cast<unsigned>(port));
    std::fflush(stdout);
    Server server;
    auto idle_since = std::chrono::steady_clock::now();
    while (true) {
#if defined(_WIN32)
        std::vector<WSAPOLLFD> polls(server.clients.size() + 1);
#else
        std::vector<pollfd> polls(server.clients.size() + 1);
#endif
        // .at: GCC 13 -O3 -Wnull-dereference treats operator[] on a freshly
        // sized vector as a possible null write.
        polls.at(0).fd =
#if defined(_WIN32)
            static_cast<SOCKET>(listen_fd);
#else
            static_cast<int>(listen_fd);
#endif
        polls.at(0).events = POLLIN;
        for (std::size_t i = 0; i < server.clients.size(); ++i) {
            polls.at(i + 1).fd =
#if defined(_WIN32)
                static_cast<SOCKET>(server.clients[i].fd);
#else
                static_cast<int>(server.clients[i].fd);
#endif
            polls.at(i + 1).events = POLLIN;
        }
#if defined(_WIN32)
        const int ready = WSAPoll(polls.data(), static_cast<ULONG>(polls.size()), 200);
#else
        const int ready = ::poll(polls.data(), static_cast<nfds_t>(polls.size()), 200);
#endif
        if (ready > 0 && (polls.at(0).revents & POLLIN) != 0) {
            accept_client(server, listen_fd);
        }
        if (ready > 0) {
            for (std::size_t i = 0; i + 1 < polls.size(); ++i) {
                const short revents = polls.at(i + 1).revents;
                if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                    erase_client(server, i);
                    break;
                }
                if ((revents & POLLIN) != 0) {
                    const std::size_t before = server.clients.size();
                    pull_client(server, i);
                    if (server.clients.size() != before) {
                        break;
                    }
                }
            }
        }
        if (server.clients.empty()) {
            if (std::chrono::steady_clock::now() - idle_since > std::chrono::seconds(30)) {
                break;
            }
        } else {
            idle_since = std::chrono::steady_clock::now();
        }
    }
    close_fd(listen_fd);
    return 0;
}

} // namespace anno::test
