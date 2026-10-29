// Wire format and socket helpers shared by the test-budget coordinator
// (coord_server.cpp) and its clients (coord.cpp). Frames are 96 bytes, fields
// little-endian at fixed offsets; nothing is sent as a struct dump.
#pragma once

#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
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
#include <spawn.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace anno::test::wire {

/// 'ANC2'. Version 1 ('ANC1', T10c F1) is not understood: a client of one
/// version and a coordinator of the other close the connection.
constexpr std::uint32_t kMagic = 0x32434E41u;
/// The compatibility version (T10c F3, D1). A client and a coordinator share a
/// host budget when this number is equal, whatever their build ids: the build
/// id (a hash of the coordinator's sources) is provenance only.
///
/// Bump it, and say why in docs/perf/README.md ("T10c F3 — compatibility"), for
/// any change of the wire format or of the reservation, journal, residency or
/// recovery semantics that an older peer would not reproduce. Do not bump it for
/// provenance, diagnostics, test hooks or code that changes no semantics. It is
/// 3 since F4: a successor issues no hold id that appears in the journal it
/// replayed (an A or R line), so a journaled promise survives two restarts. An
/// older successor numbers a budget's holds from 1 and can reuse an id, losing
/// the promise. Pinned by the test "F3 (D1, pin)".
constexpr std::uint32_t kCompatVersion = 3;
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
/// The client needs the coordinator's exact build (the gate that validates the
/// coordinator itself). Set by ANNO_TEST_BUDGET_EXACT_BUILD=1; never by default.
constexpr std::uint32_t kFlagExactBuild = 4;

/// Reject frame (kStReject): why the coordinator did not accept the hello.
constexpr std::uint32_t kRejectIncompatible = 1; ///< another compatibility version
constexpr std::uint32_t kRejectBuild = 2;        ///< exact build required, another build
/// Reject frame flags (offset 80): the coordinator is idle and steps aside now.
constexpr std::uint32_t kRejectSteppingAside = 1;

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

/// What a reply read ended with. A timeout means the peer accepted the
/// connection but has not answered (not ready); a closed peer means it went
/// away; only a frame with another magic means a foreign protocol.
enum class Recv { Frame, Timeout, Closed, Foreign };

/// Reads one whole frame within the socket's receive timeout.
inline Recv recv_reply(Native fd, Frame& frame) {
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
        if (n == 0) {
            return Recv::Closed;
        }
        const int error = last_socket_error();
#if defined(_WIN32)
        if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
            return Recv::Timeout;
        }
#else
        if (error == EINTR) {
            continue;
        }
        if (error == EAGAIN || error == EWOULDBLOCK) {
            return Recv::Timeout;
        }
#endif
        return Recv::Closed;
    }
    return get32(frame.bytes, 0) == kMagic ? Recv::Frame : Recv::Foreign;
}

/// Sets close-on-exec on a POSIX descriptor (no-op where it is already set).
inline void set_cloexec(int fd) {
#if !defined(_WIN32)
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
#else
    (void)fd;
#endif
}

/// A TCP stream socket that no helper process inherits (T10c F3, D2).
///  - Linux: SOCK_CLOEXEC at creation. Elsewhere on POSIX: FD_CLOEXEC right after
///    creation (no exec happens in between in this single-threaded use).
///  - Windows: WSA_FLAG_NO_HANDLE_INHERIT. A plain socket() is inheritable there
///    (cache/f3/exp/windows-socket-inheritance.out).
inline Native open_socket() {
    startup_sockets();
#if defined(_WIN32)
    const SOCKET fd = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0,
                                   WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    return fd == INVALID_SOCKET ? kInvalid : static_cast<Native>(fd);
#else
#if defined(SOCK_CLOEXEC)
    const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
#else
    const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
    if (fd < 0) {
        return kInvalid;
    }
    set_cloexec(fd);
    return static_cast<Native>(fd);
#endif
}

/// Accepts a connection that no helper process inherits. On Windows an accepted
/// socket is inheritable too, so the flag is cleared (verified by experiment).
inline Native accept_socket(Native listen_fd) {
#if defined(_WIN32)
    const SOCKET fd = ::accept(static_cast<SOCKET>(listen_fd), nullptr, nullptr);
    if (fd == INVALID_SOCKET) {
        return kInvalid;
    }
    ::SetHandleInformation(reinterpret_cast<HANDLE>(fd), HANDLE_FLAG_INHERIT, 0);
    return static_cast<Native>(fd);
#else
#if defined(__linux__)
    const int fd = ::accept4(static_cast<int>(listen_fd), nullptr, nullptr, SOCK_CLOEXEC);
#else
    const int fd = ::accept(static_cast<int>(listen_fd), nullptr, nullptr);
    if (fd >= 0) {
        set_cloexec(fd);
    }
#endif
    return fd < 0 ? kInvalid : static_cast<Native>(fd);
#endif
}

#if !defined(_WIN32)
/// A pipe whose ends are both close-on-exec: the child gets its own ends through
/// dup2 onto 0, 1 and 2, which clears the flag on those.
inline bool make_pipe(int fds[2]) {
#if defined(__linux__)
    return ::pipe2(fds, O_CLOEXEC) == 0;
#else
    if (::pipe(fds) != 0) {
        return false;
    }
    set_cloexec(fds[0]);
    set_cloexec(fds[1]);
    return true;
#endif
}

/// Adds to a spawn a close of every descriptor above stdio, so a helper process
/// inherits nothing but 0, 1 and 2 (T10c F3, D2).
///  - glibc 2.34 and later: posix_spawn_file_actions_addclosefrom_np (declared in
///    <spawn.h> under __USE_GNU; glibc 2.39 in WSL Ubuntu, checked in
///    cache/f3/exp/fdexp.out).
///  - Elsewhere: one close action per descriptor from 3 to the open-file limit,
///    capped at 4096. A close of a descriptor that is not open succeeds in glibc
///    (checked in cache/f3/exp/fdexp.out), so the loop is safe.
inline bool close_descriptors_from_3(posix_spawn_file_actions_t* actions) {
#if defined(__GLIBC__) && defined(__USE_GNU) &&                                                    \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
    return posix_spawn_file_actions_addclosefrom_np(actions, 3) == 0;
#else
    long limit = ::sysconf(_SC_OPEN_MAX);
    if (limit < 0 || limit > 4096) {
        limit = 4096;
    }
    for (int fd = 3; fd < static_cast<int>(limit); ++fd) {
        if (posix_spawn_file_actions_addclose(actions, fd) != 0) {
            return false;
        }
    }
    return true;
#endif
}

#endif

/// True when 127.0.0.1:port accepts one TCP connection within `budget`. A
/// printed LISTENING line is not this check. The WSL localhost relay can lag
/// the LISTENING line, so callers use a short budget rather than one instant.
inline bool local_port_accepts(std::uint16_t port, std::chrono::milliseconds budget) {
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        const Native fd = open_socket();
        if (fd == kInvalid) {
            return false;
        }
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bool accepted = false;
#if defined(_WIN32)
        u_long nonblocking = 1;
        ::ioctlsocket(static_cast<SOCKET>(fd), FIONBIO, &nonblocking);
        const int started = ::connect(static_cast<SOCKET>(fd), reinterpret_cast<sockaddr*>(&address),
                                      sizeof(address));
        if (started == 0) {
            accepted = true;
        } else {
            fd_set writable;
            FD_ZERO(&writable);
            FD_SET(static_cast<SOCKET>(fd), &writable);
            timeval wait{};
            wait.tv_usec = 200000;
            const int ready = ::select(0, nullptr, &writable, nullptr, &wait);
            int error = 0;
            int length = sizeof(error);
            ::getsockopt(static_cast<SOCKET>(fd), SOL_SOCKET, SO_ERROR,
                         reinterpret_cast<char*>(&error), &length);
            accepted = ready > 0 && error == 0;
        }
#else
        const int flags = ::fcntl(static_cast<int>(fd), F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(static_cast<int>(fd), F_SETFL, flags | O_NONBLOCK);
        }
        const int started = ::connect(static_cast<int>(fd), reinterpret_cast<sockaddr*>(&address),
                                      sizeof(address));
        if (started == 0) {
            accepted = true;
        } else if (errno == EINPROGRESS) {
            pollfd polled{};
            polled.fd = static_cast<int>(fd);
            polled.events = POLLOUT;
            const int ready = ::poll(&polled, 1, 200);
            int error = 0;
            socklen_t length = sizeof(error);
            ::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_ERROR, &error, &length);
            accepted = ready > 0 && error == 0;
        }
#endif
        close_fd(fd);
        if (accepted) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
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
