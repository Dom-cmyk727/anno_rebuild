// Wire format and socket helpers shared by the test-budget coordinator
// (coord_server.cpp) and its clients (coord.cpp). Frames are 96 bytes, fields
// little-endian at fixed offsets; nothing is sent as a struct dump.
#pragma once

#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <anno/test/parallel.hpp>

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
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace anno::test::wire {

/// 'ANC2'. Version 1 ('ANC1', T10c F1) is not understood: a client of one
/// version and a coordinator of the other close the connection.
constexpr std::uint32_t kMagic = 0x32434E41u;
constexpr std::uint32_t kProtocol = 2;
constexpr std::size_t kFrameBytes = 96;
constexpr std::uint16_t kSharedPort = 47631;
constexpr std::uint16_t kLocalPort = 47632;
constexpr std::uint64_t kUnknown = std::numeric_limits<std::uint64_t>::max();

// Client to coordinator.
constexpr std::uint32_t kOpHello = 1;
constexpr std::uint32_t kOpReserve = 2;
constexpr std::uint32_t kOpTry = 3;
constexpr std::uint32_t kOpRelease = 4;
constexpr std::uint32_t kOpCpuAcquire = 5;
constexpr std::uint32_t kOpCpuRelease = 6;
constexpr std::uint32_t kOpCancel = 7;
constexpr std::uint32_t kOpPing = 8;
constexpr std::uint32_t kOpSampleReply = 9;
constexpr std::uint32_t kOpMeterReply = 10;

// Coordinator to client.
constexpr std::uint32_t kStAdmit = 101;
constexpr std::uint32_t kStWait = 102;
constexpr std::uint32_t kStRefuse = 103;
constexpr std::uint32_t kStOk = 104;
/// Asks a holder for its current resident bytes (answer: kOpSampleReply).
constexpr std::uint32_t kStSample = 105;
/// Asks a client meter for a reading taken after epoch `a` began (answer:
/// kOpMeterReply with the same epoch).
constexpr std::uint32_t kStRefresh = 106;
constexpr std::uint32_t kStReject = 107;

constexpr std::uint32_t kWhyAdmitted = 1;
constexpr std::uint32_t kWhyWaiting = 2;
constexpr std::uint32_t kWhyLow = 3;
constexpr std::uint32_t kWhyExceeds = 4;
constexpr std::uint32_t kWhyUnknown = 5;
constexpr std::uint32_t kWhyCancelled = 6;
constexpr std::uint32_t kWhyLost = 7;
constexpr std::uint32_t kWhyFresh = 8;

/// Hello flags.
constexpr std::uint32_t kFlagInternalMeter = 1; ///< the coordinator measures (host budget)
constexpr std::uint32_t kFlagClientMeter = 2;   ///< this client answers kStRefresh

/// Memory domains. A WSL client allocates from its VM (guest) and, through
/// the VM, from the Windows host; a Windows or plain Linux client only from
/// its host.
constexpr std::uint32_t kDomainHost = 1;
constexpr std::uint32_t kDomainGuest = 2;

using Native = std::uintptr_t;
constexpr Native kInvalid = static_cast<Native>(-1);

struct Frame {
    std::uint8_t bytes[kFrameBytes]{};
};

inline void put32(std::uint8_t* bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

inline void put64(std::uint8_t* bytes, std::size_t offset, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

inline std::uint32_t get32(const std::uint8_t* bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(bytes[offset + static_cast<std::size_t>(i)]) << (8 * i);
    }
    return value;
}

inline std::uint64_t get64(const std::uint8_t* bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<std::size_t>(i)];
    }
    return value;
}

// Field offsets: 0 magic, 4 op, 8 budget, 16 token, 24 req, then a..f (u64 at
// 32..72), g and h (u32 at 80, 84), i (u64 at 88).
inline std::uint32_t frame_op(const Frame& frame) {
    return get32(frame.bytes, 4);
}
inline std::uint64_t frame_budget(const Frame& frame) {
    return get64(frame.bytes, 8);
}
inline std::uint64_t frame_token(const Frame& frame) {
    return get64(frame.bytes, 16);
}
inline std::uint64_t frame_req(const Frame& frame) {
    return get64(frame.bytes, 24);
}
inline std::uint64_t frame_a(const Frame& frame) {
    return get64(frame.bytes, 32);
}
inline std::uint64_t frame_b(const Frame& frame) {
    return get64(frame.bytes, 40);
}
inline std::uint64_t frame_c(const Frame& frame) {
    return get64(frame.bytes, 48);
}
inline std::uint64_t frame_d(const Frame& frame) {
    return get64(frame.bytes, 56);
}
inline std::uint64_t frame_e(const Frame& frame) {
    return get64(frame.bytes, 64);
}
inline std::uint64_t frame_f(const Frame& frame) {
    return get64(frame.bytes, 72);
}
inline std::uint32_t frame_g(const Frame& frame) {
    return get32(frame.bytes, 80);
}
inline std::uint32_t frame_h(const Frame& frame) {
    return get32(frame.bytes, 84);
}
inline std::uint64_t frame_i(const Frame& frame) {
    return get64(frame.bytes, 88);
}

inline void fill_header(Frame& frame, std::uint32_t op, std::uint64_t budget, std::uint64_t token,
                        std::uint64_t req) {
    put32(frame.bytes, 0, kMagic);
    put32(frame.bytes, 4, op);
    put64(frame.bytes, 8, budget);
    put64(frame.bytes, 16, token);
    put64(frame.bytes, 24, req);
}

inline std::uint32_t why_code(const std::string& reason) {
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
    if (reason == "waiting for a fresh reading") {
        return kWhyFresh;
    }
    return kWhyLost;
}

inline std::string why_text(std::uint32_t code) {
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
    case kWhyFresh:
        return "waiting for a fresh reading";
    default:
        return "coordinator lost";
    }
}

inline void startup_sockets() {
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

inline int last_socket_error() {
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}

inline bool would_block(int error) {
#if defined(_WIN32)
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEINTR;
#else
    return error == EAGAIN || error == EWOULDBLOCK || error == EINTR;
#endif
}

inline void close_fd(Native fd) {
    if (fd == kInvalid) {
        return;
    }
#if defined(_WIN32)
    ::closesocket(static_cast<SOCKET>(fd));
#else
    ::close(static_cast<int>(fd));
#endif
}

inline void shutdown_fd(Native fd) {
    if (fd == kInvalid) {
        return;
    }
#if defined(_WIN32)
    ::shutdown(static_cast<SOCKET>(fd), SD_BOTH);
#else
    ::shutdown(static_cast<int>(fd), SHUT_RDWR);
#endif
}

inline void set_nodelay(Native fd) {
    int on = 1;
#if defined(_WIN32)
    ::setsockopt(static_cast<SOCKET>(fd), IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&on), sizeof(on));
#else
    ::setsockopt(static_cast<int>(fd), IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
#endif
}

inline bool send_all(Native fd, const Frame& frame) {
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

inline void set_recv_timeout(Native fd, int milliseconds) {
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

/// A whole frame, or false on a timeout, a closed peer or a foreign magic.
inline bool recv_full(Native fd, Frame& frame) {
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
#if !defined(_WIN32)
        if (n < 0 && errno == EINTR) {
            continue;
        }
#endif
        return false;
    }
    return get32(frame.bytes, 0) == kMagic;
}

/// Positive: bytes read. 0: the peer closed. -2: no byte is ready yet.
inline int recv_some(Native fd, std::uint8_t* dest, std::size_t cap) {
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

inline std::uint64_t fnv(std::string_view text) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const char raw : text) {
        hash ^= static_cast<unsigned char>(raw);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

inline std::uint64_t this_pid() {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

/// The OS a process id belongs to: 1 Windows, 2 Linux.
inline std::uint32_t this_namespace() {
#if defined(_WIN32)
    return 1;
#else
    return 2;
#endif
}

#if !defined(_WIN32)
/// starttime (field 22) of /proc/<pid>/stat, 0 when unreadable.
inline std::uint64_t linux_start_time(const std::string& stat_path) {
    std::FILE* file = std::fopen(stat_path.c_str(), "r");
    if (file == nullptr) {
        return 0;
    }
    char buffer[1024];
    const std::size_t n = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);
    if (n == 0) {
        return 0;
    }
    const std::string text(buffer, n);
    const auto mark = text.rfind(')');
    if (mark == std::string::npos || mark + 2 >= text.size()) {
        return 0;
    }
    // After comm (field 2) the fields are space separated: skip 19 of them
    // (3..21) to reach starttime.
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
    std::uint64_t start = 0;
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') {
        start = start * 10 + static_cast<std::uint64_t>(text[index] - '0');
        ++index;
    }
    return start;
}
#endif

#if !defined(_WIN32)
/// True when /proc/<pid>/stat says the process has exited but is not reaped.
inline bool linux_zombie(const std::string& stat_path) {
    std::FILE* file = std::fopen(stat_path.c_str(), "r");
    if (file == nullptr) {
        return false;
    }
    char buffer[1024];
    const std::size_t n = std::fread(buffer, 1, sizeof(buffer) - 1, file);
    std::fclose(file);
    const std::string text(buffer, n);
    const auto mark = text.rfind(')');
    return mark != std::string::npos && mark + 2 < text.size() &&
           (text[mark + 2] == 'Z' || text[mark + 2] == 'X');
}
#endif

/// The creation stamp that, with the pid, names one process: Windows
/// GetProcessTimes creation (FILETIME), Linux /proc starttime.
inline std::uint64_t process_started() {
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
    return linux_start_time("/proc/self/stat");
#endif
}

/// True inside WSL (Microsoft's Linux VM).
inline bool is_wsl() {
#if defined(_WIN32)
    return false;
#else
    static const bool wsl = [] {
        if (::access("/proc/sys/fs/binfmt_misc/WSLInterop", F_OK) == 0 ||
            ::access("/proc/sys/fs/binfmt_misc/WSLInterop-late", F_OK) == 0) {
            return true;
        }
        std::FILE* file = std::fopen("/proc/version", "r");
        if (file == nullptr) {
            return false;
        }
        char buffer[512] = {};
        const std::size_t n = std::fread(buffer, 1, sizeof(buffer) - 1, file);
        std::fclose(file);
        std::string text(buffer, n);
        for (char& c : text) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return text.find("microsoft") != std::string::npos || text.find("wsl") != std::string::npos;
    }();
    return wsl;
#endif
}

/// The memory domain of this process (ANNO_TEST_BUDGET_DOMAIN=host|guest
/// overrides it for tests).
inline std::uint32_t this_domain() {
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_DOMAIN")) {
        const std::string value = env;
        if (value == "host") {
            return kDomainHost;
        }
        if (value == "guest") {
            return kDomainGuest;
        }
    }
    return is_wsl() ? kDomainGuest : kDomainHost;
}

/// How long a client waits for any frame from the coordinator before it
/// treats the connection as lost (ANNO_TEST_BUDGET_LIVENESS_MS, default 10 s).
/// A waiting client pings every recheck period and the coordinator answers at
/// once, so silence this long means it has stopped or hung.
inline std::chrono::milliseconds liveness() {
    std::uint64_t n = detail::env_count("ANNO_TEST_BUDGET_LIVENESS_MS").value_or(10000);
    if (n < 100) {
        n = 100;
    }
    return std::chrono::milliseconds(n > 600000 ? 600000 : n);
}

} // namespace anno::test::wire
