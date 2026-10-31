// The test-budget coordinator: one process that owns every admission of a
// budget (docs/perf/README.md, T10c F2).
//
// Readings. A grant needs free memory measured after every event that lowered
// the pending bytes: a release, a client that disconnected with holds, or a
// holder whose allocation became visible (resident). A reading taken before
// such an event does not show those bytes as used, so it cannot authorize a
// grant (review round cr, C1). Each such event starts a new epoch.
//  - The process budget (the "internal" meter): the coordinator measures at
//    decision time, after the events it has processed. Host: Windows
//    GlobalMemoryStatusEx (from WSL through a PowerShell meter process); in
//    WSL also the VM's own /proc/meminfo (guest).
//  - A budget of unit tests: its clients are the meters (injected readings).
//    The coordinator asks every meter client for a reading of the current
//    epoch and decides only when all have answered it.
// A WSL request is bounded by both its VM and the host; a Windows request by
// the host. Holders are sampled for residency while requests wait.
//
// Replacement (review round cr, C3). Every promise (a memory hold or a CPU
// token) is written to a journal before the client hears of it. A coordinator
// that starts finds its predecessor's open promises there and counts them as
// fully pending ("orphans") until their owner process is gone, checked by pid
// and creation stamp. It never starts from an empty budget while old work may
// still run. A client of another build (or protocol) is refused; an idle
// coordinator of another build steps aside for it.
//
// Shortfalls. More than a domain can ever give (its total minus the headroom)
// is refused as "exceeds capacity". Anything else waits: reservations end, and
// memory used outside the budget can come back. Waits are bounded by the
// client's deadline, which withdraws the request; nothing here admits by time.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <anno/test/budget.hpp>
#include <anno/test/parallel.hpp>

#include "admit_detail.hpp"
#include "coord_wire.hpp"

#if !defined(_WIN32)
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace anno::test {
namespace {

using namespace wire;
using detail::Constraint;
using detail::Decision;
using detail::View;

struct Reading {
    bool meter = false;
    std::optional<std::uint64_t> free;
    std::optional<std::uint64_t> total;
};

Reading local_reading() {
    Reading reading;
    reading.meter = true;
    reading.free = free_memory_bytes();
    reading.total = total_memory_bytes();
    return reading;
}

#if !defined(_WIN32)
/// PowerShell 5.1 script of the host meter: GlobalMemoryStatusEx on request
/// ("M"), and whether a Windows process (pid, creation FILETIME) still runs
/// ("A pid stamp").
constexpr const char* kMeterScript = R"PS($ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AnnoMeter {
    [StructLayout(LayoutKind.Sequential)]
    public struct Status {
        public uint Length; public uint Load;
        public ulong TotalPhys; public ulong AvailPhys;
        public ulong TotalPageFile; public ulong AvailPageFile;
        public ulong TotalVirtual; public ulong AvailVirtual; public ulong AvailExtended;
    }
    [DllImport("kernel32.dll")] static extern bool GlobalMemoryStatusEx(ref Status s);
    [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] static extern bool GetProcessTimes(IntPtr h, out long c, out long e, out long k, out long u);
    [DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr h, out uint code);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
    public static string Mem() {
        Status s = new Status();
        s.Length = (uint)Marshal.SizeOf(typeof(Status));
        if (!GlobalMemoryStatusEx(ref s)) { return "M ?"; }
        return "M " + s.AvailPhys + " " + s.AvailPageFile + " " + s.TotalPhys + " " + s.TotalPageFile;
    }
    public static string Alive(uint pid, long started) {
        IntPtr h = OpenProcess(0x1000, false, pid);
        if (h == IntPtr.Zero) { return Marshal.GetLastWin32Error() == 87 ? "A 0" : "A ?"; }
        try {
            long c, e, k, u; uint code;
            if (!GetProcessTimes(h, out c, out e, out k, out u)) { return "A ?"; }
            if (c != started) { return "A 0"; }
            if (!GetExitCodeProcess(h, out code)) { return "A ?"; }
            return code == 259 ? "A 1" : "A 0";
        } finally { CloseHandle(h); }
    }
}
'@
[Console]::Out.WriteLine('READY')
[Console]::Out.Flush()
while ($true) {
    $line = [Console]::In.ReadLine()
    if ($line -eq $null) { break }
    if ($line -eq 'M') { $out = [AnnoMeter]::Mem() }
    elseif ($line.StartsWith('A ')) { $p = $line.Split(' '); $out = [AnnoMeter]::Alive([uint32]$p[1], [int64]$p[2]) }
    else { $out = '?' }
    [Console]::Out.WriteLine($out)
    [Console]::Out.Flush()
}
)PS";

std::string base64(const std::vector<std::uint8_t>& data) {
    static const char* const kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) | data[i + 2];
        out += kTable[(v >> 18) & 63u];
        out += kTable[(v >> 12) & 63u];
        out += kTable[(v >> 6) & 63u];
        out += kTable[v & 63u];
    }
    if (i + 1 == data.size()) {
        const std::uint32_t v = static_cast<std::uint32_t>(data[i]) << 16;
        out += kTable[(v >> 18) & 63u];
        out += kTable[(v >> 12) & 63u];
        out += "==";
    } else if (i + 2 == data.size()) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8);
        out += kTable[(v >> 18) & 63u];
        out += kTable[(v >> 12) & 63u];
        out += kTable[(v >> 6) & 63u];
        out += '=';
    }
    return out;
}

/// The Windows side of the host, measured from inside WSL. The VM's
/// /proc/meminfo only knows the VM; the host's free memory is what Windows
/// clients and the VM itself allocate from.
class HostMeter {
  public:
    ~HostMeter() { stop(); }

    void start() {
        const char* const fixed = "/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe";
        if (::access(fixed, X_OK) != 0) {
            unavailable_ = true;
            return;
        }
        std::vector<std::uint8_t> utf16;
        for (const char* c = kMeterScript; *c != '\0'; ++c) {
            utf16.push_back(static_cast<std::uint8_t>(*c));
            utf16.push_back(0);
        }
        std::string encoded = base64(utf16);
        int in_pipe[2];
        int out_pipe[2];
        if (!make_pipe(in_pipe)) {
            unavailable_ = true;
            return;
        }
        if (!make_pipe(out_pipe)) {
            ::close(in_pipe[0]);
            ::close(in_pipe[1]);
            unavailable_ = true;
            return;
        }
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, in_pipe[0], 0);
        posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
        posix_spawn_file_actions_addopen(&actions, 2, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addclose(&actions, in_pipe[1]);
        posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
        // The meter inherits no coordinator descriptor: the listening socket
        // stayed open in this child after the coordinator died (T10c F3, D2).
        close_descriptors_from_3(&actions);
        std::string exe = fixed;
        std::string a1 = "-NoProfile";
        std::string a2 = "-NonInteractive";
        std::string a3 = "-ExecutionPolicy";
        std::string a4 = "Bypass";
        std::string a5 = "-EncodedCommand";
        char* argv[] = {exe.data(), a1.data(), a2.data(),      a3.data(),
                        a4.data(),  a5.data(), encoded.data(), nullptr};
        pid_t pid = 0;
        const int rc = posix_spawn(&pid, fixed, &actions, nullptr, argv, environ);
        posix_spawn_file_actions_destroy(&actions);
        ::close(in_pipe[0]);
        ::close(out_pipe[1]);
        if (rc != 0) {
            ::close(in_pipe[1]);
            ::close(out_pipe[0]);
            unavailable_ = true;
            return;
        }
        pid_ = pid;
        in_ = in_pipe[1];
        out_ = out_pipe[0];
        ready_ = false;
        started_at_ = std::chrono::steady_clock::now();
    }

    /// Never startable (no PowerShell): requests are refused, not left waiting.
    bool unavailable() const noexcept { return unavailable_; }

    /// Checks, without blocking, whether the start-up line has arrived.
    void poll_ready() {
        if (pid_ <= 0) {
            if (!unavailable_ && std::chrono::steady_clock::now() >= retry_at_) {
                start();
            }
            return;
        }
        if (ready_) {
            return;
        }
        const std::optional<std::string> line = read_line(0);
        if (line && *line == "READY") {
            ready_ = true;
        } else if (std::chrono::steady_clock::now() - started_at_ > std::chrono::seconds(60)) {
            restart();
        }
    }

    Reading host() {
        Reading reading;
        reading.meter = !unavailable_;
        if (!ready_) {
            poll_ready();
            if (!ready_) {
                return reading;
            }
        }
        const std::optional<std::string> line = query("M");
        if (!line) {
            return reading;
        }
        unsigned long long avail_phys = 0;
        unsigned long long avail_page = 0;
        unsigned long long total_phys = 0;
        unsigned long long total_page = 0;
        if (std::sscanf(line->c_str(), "M %llu %llu %llu %llu", &avail_phys, &avail_page,
                        &total_phys, &total_page) == 4) {
            // As free_memory_bytes on Windows: the commit limit fails an
            // allocation before free RAM does.
            reading.free = std::min<std::uint64_t>(avail_phys, avail_page);
            reading.total = std::min<std::uint64_t>(total_phys, total_page);
        }
        return reading;
    }

    /// Whether Windows process `pid` with creation stamp `started` runs.
    std::optional<bool> alive(std::uint64_t pid, std::uint64_t started) {
        if (!ready_) {
            return std::nullopt;
        }
        const std::optional<std::string> line =
            query("A " + std::to_string(pid) + " " + std::to_string(started));
        if (!line) {
            return std::nullopt;
        }
        if (*line == "A 1") {
            return true;
        }
        if (*line == "A 0") {
            return false;
        }
        return std::nullopt;
    }

  private:
    std::optional<std::string> query(const std::string& request) {
        const std::string text = request + "\n";
        if (::write(in_, text.data(), text.size()) != static_cast<ssize_t>(text.size())) {
            restart();
            return std::nullopt;
        }
        std::optional<std::string> line = read_line(2000);
        if (!line) {
            restart();
        }
        return line;
    }

    std::optional<std::string> read_line(int timeout_ms) {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        for (;;) {
            const auto end = buffer_.find('\n');
            if (end != std::string::npos) {
                std::string line = buffer_.substr(0, end);
                buffer_.erase(0, end + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
                    line.pop_back();
                }
                return line;
            }
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  deadline - std::chrono::steady_clock::now())
                                  .count();
            pollfd fd{};
            fd.fd = out_;
            fd.events = POLLIN;
            const int ready = ::poll(&fd, 1, static_cast<int>(left > 0 ? left : 0));
            if (ready <= 0) {
                return std::nullopt;
            }
            char chunk[256];
            const ssize_t n = ::read(out_, chunk, sizeof(chunk));
            if (n <= 0) {
                return std::nullopt;
            }
            buffer_.append(chunk, chunk + n);
        }
    }

    void stop() {
        if (in_ >= 0) {
            ::close(in_);
            in_ = -1;
        }
        if (out_ >= 0) {
            ::close(out_);
            out_ = -1;
        }
        if (pid_ > 0) {
            ::kill(pid_, SIGKILL);
            int status = 0;
            ::waitpid(pid_, &status, 0);
            pid_ = -1;
        }
        ready_ = false;
        buffer_.clear();
    }

    void restart() {
        stop();
        retry_at_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    }

    pid_t pid_ = -1;
    int in_ = -1;
    int out_ = -1;
    bool ready_ = false;
    bool unavailable_ = false;
    std::string buffer_;
    std::chrono::steady_clock::time_point started_at_{};
    std::chrono::steady_clock::time_point retry_at_{};
};
#endif

/// The coordinator's own readings (the process budget's meters).
class Meters {
  public:
    void start() {
#if !defined(_WIN32)
        wsl_ = is_wsl();
        if (wsl_) {
            host_.start();
        }
#endif
    }

    void poll() {
#if !defined(_WIN32)
        if (wsl_) {
            host_.poll_ready();
        }
#endif
    }

    Reading host() {
#if !defined(_WIN32)
        if (wsl_) {
            return host_.host();
        }
#endif
        return local_reading();
    }

    /// Whether a Windows process still runs, asked from WSL. Nullopt: unknown.
    std::optional<bool> windows_alive(std::uint64_t pid, std::uint64_t started) {
#if !defined(_WIN32)
        if (wsl_) {
            return host_.alive(pid, started);
        }
#endif
        (void)pid;
        (void)started;
        return std::nullopt;
    }

    /// The VM a WSL client allocates from; no meter outside WSL.
    Reading guest() {
#if !defined(_WIN32)
        if (wsl_) {
            return local_reading();
        }
#endif
        return Reading{};
    }

  private:
#if !defined(_WIN32)
    bool wsl_ = false;
    HostMeter host_;
#endif
};

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
    bool cpu = false;
    /// A try: decided once, as soon as current readings exist.
    bool once = false;
    /// The client was told it waits (a real shortfall, not only a reading
    /// being refreshed), so it ran its on_wait hook.
    bool told = false;
    Admit last;
};

struct SClient {
    Native fd = kInvalid;
    bool hello = false;
    std::uint64_t token = 0;
    std::uint64_t budget = 0;
    std::uint32_t ns = 0;
    std::uint64_t pid = 0;
    std::uint64_t started = 0;
    std::uint32_t domain = kDomainHost;
    bool meter = false;
    std::vector<RemoteHold> holds;
    std::uint64_t resident = 0;
    std::uint32_t cpu = 0;
    std::vector<WaitReq> waiting;
    std::vector<std::uint64_t> cancelled;
    std::vector<std::uint8_t> incoming;
    /// The last meter answer and the epoch it was asked for.
    std::uint64_t meter_epoch = 0;
    std::optional<std::uint64_t> meter_free;
    std::optional<std::uint64_t> meter_total;
};

struct SBudget {
    std::uint64_t id = 0;
    std::uint32_t cpu_budget = 0;
    std::uint32_t cpu_in_use = 0;
    std::uint64_t next_seq = 1;
    bool internal = false;
    /// Raised by every event that lowers the pending bytes (see the top).
    std::uint64_t epoch = 1;
    std::uint64_t refreshed_epoch = 0;
    std::chrono::steady_clock::time_point last_tick{};
};

/// The process that holds a promise: OS, pid and creation stamp.
struct Owner {
    std::uint32_t ns = 0;
    std::uint64_t pid = 0;
    std::uint64_t started = 0;
    bool operator<(const Owner& other) const {
        return std::tie(ns, pid, started) < std::tie(other.ns, other.pid, other.started);
    }
    bool operator==(const Owner& other) const {
        return ns == other.ns && pid == other.pid && started == other.started;
    }
};

/// A predecessor's promise whose owner may still be working.
struct Orphan {
    std::uint64_t budget = 0;
    std::uint64_t hold = 0; ///< 0: a CPU token
    std::uint64_t bytes = 0;
    std::uint32_t domain = kDomainHost;
    Owner owner;
};

/// Append-only record of the open promises. Lines: A (memory hold admitted),
/// R (released), C (CPU token admitted), D (returned). Replayed at start and
/// rewritten (compacted) from the live state.
/// fopen with close-on-exec on POSIX so a meter child does not inherit the
/// journal (T10c F4, N3). glibc 'e' in the mode; Windows uses bInheritHandles
/// FALSE on helper spawns instead.
std::FILE* fopen_cloexec(const char* path, const char* mode) {
#if defined(_WIN32)
    return std::fopen(path, mode);
#else
    std::string with_e = mode;
    with_e.push_back('e');
    return std::fopen(path, with_e.c_str());
#endif
}

class Journal {
  public:
    /// Largest hold id in any A or R line, over every budget. A successor
    /// issues none at or below this (T10c F4, E1).
    std::uint64_t max_hold_id() const noexcept { return max_hold_id_; }

    bool open(const std::filesystem::path& path, std::vector<Orphan>& orphans) {
        path_ = path;
        max_hold_id_ = 0;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::map<std::pair<std::uint64_t, std::uint64_t>, Orphan> holds;
        std::map<std::pair<std::uint64_t, Owner>, std::uint64_t> cpu;
        if (std::FILE* in = fopen_cloexec(path.string().c_str(), "r")) {
            char line[512];
            while (std::fgets(line, sizeof(line), in) != nullptr) {
                char kind = 0;
                unsigned long long b = 0;
                unsigned long long h = 0;
                unsigned long long bytes = 0;
                unsigned ns = 0;
                unsigned long long pid = 0;
                unsigned long long started = 0;
                unsigned domain = 0;
                if (std::sscanf(line, "A %llu %llu %llu %u %llu %llu %u", &b, &h, &bytes, &ns, &pid,
                                &started, &domain) == 7) {
                    kind = 'A';
                } else if (std::sscanf(line, "R %llu %llu", &b, &h) == 2) {
                    kind = 'R';
                } else if (std::sscanf(line, "C %llu %u %llu %llu", &b, &ns, &pid, &started) == 4) {
                    kind = 'C';
                } else if (std::sscanf(line, "D %llu %u %llu %llu", &b, &ns, &pid, &started) == 4) {
                    kind = 'D';
                }
                if ((kind == 'A' || kind == 'R') && h > max_hold_id_) {
                    max_hold_id_ = h;
                }
                const Owner owner{ns, pid, started};
                if (kind == 'A') {
                    holds[{b, h}] = Orphan{
                        b, h, bytes, domain == kDomainGuest ? kDomainGuest : kDomainHost, owner};
                } else if (kind == 'R') {
                    holds.erase({b, h});
                } else if (kind == 'C') {
                    cpu[{b, owner}] += 1;
                } else if (kind == 'D') {
                    auto found = cpu.find({b, owner});
                    if (found != cpu.end() && --found->second == 0) {
                        cpu.erase(found);
                    }
                }
            }
            std::fclose(in);
        }
        for (const auto& [key, orphan] : holds) {
            orphans.push_back(orphan);
        }
        for (const auto& [key, count] : cpu) {
            for (std::uint64_t i = 0; i < count; ++i) {
                Orphan token;
                token.budget = key.first;
                token.owner = key.second;
                orphans.push_back(token);
            }
        }
        return rewrite(lines_for(orphans));
    }

    static std::vector<std::string> lines_for(const std::vector<Orphan>& orphans) {
        std::vector<std::string> lines;
        for (const Orphan& o : orphans) {
            lines.push_back(o.hold != 0 ? hold_line(o.budget, o.hold, o.bytes, o.owner, o.domain)
                                        : cpu_line('C', o.budget, o.owner));
        }
        return lines;
    }

    static std::string hold_line(std::uint64_t budget, std::uint64_t hold, std::uint64_t bytes,
                                 const Owner& owner, std::uint32_t domain) {
        return "A " + std::to_string(budget) + " " + std::to_string(hold) + " " +
               std::to_string(bytes) + " " + std::to_string(owner.ns) + " " +
               std::to_string(owner.pid) + " " + std::to_string(owner.started) + " " +
               std::to_string(domain);
    }

    static std::string cpu_line(char kind, std::uint64_t budget, const Owner& owner) {
        return std::string(1, kind) + " " + std::to_string(budget) + " " +
               std::to_string(owner.ns) + " " + std::to_string(owner.pid) + " " +
               std::to_string(owner.started);
    }

    /// Writes one line and flushes it to the OS before the caller acts on
    /// it. False: the promise must not be made.
    bool append(const std::string& line) {
        if (file_ == nullptr) {
            return false;
        }
        if (std::fputs((line + "\n").c_str(), file_) < 0 || std::fflush(file_) != 0) {
            return false;
        }
        appended_ += 1;
        return true;
    }

    bool wants_compaction() const noexcept { return appended_ > 20000; }

    /// Replaces the file with exactly `lines` (written beside it, then renamed).
    bool rewrite(const std::vector<std::string>& lines) {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
        const std::filesystem::path tmp = path_.string() + ".tmp";
        std::FILE* out = fopen_cloexec(tmp.string().c_str(), "w");
        if (out == nullptr) {
            return false;
        }
        bool ok = true;
        for (const std::string& line : lines) {
            ok = ok && std::fputs((line + "\n").c_str(), out) >= 0;
        }
        ok = std::fclose(out) == 0 && ok;
        std::error_code error;
        std::filesystem::rename(tmp, path_, error);
        if (!ok || error) {
            return false;
        }
        file_ = fopen_cloexec(path_.string().c_str(), "a");
        appended_ = 0;
        return file_ != nullptr;
    }

    Journal() = default;
    Journal(const Journal&) = delete;
    Journal& operator=(const Journal&) = delete;
    ~Journal() {
        if (file_ != nullptr) {
            std::fclose(file_);
        }
    }

  private:
    std::filesystem::path path_;
    std::FILE* file_ = nullptr;
    std::uint64_t appended_ = 0;
    std::uint64_t max_hold_id_ = 0;
};

struct Server {
    std::vector<SClient> clients;
    std::vector<SBudget> budgets;
    Meters meters;
    Journal journal;
    std::vector<Orphan> orphans;
    std::uint64_t build = 0;
    /// The compatibility version this coordinator speaks (wire::kCompatVersion
    /// unless a test sets another).
    std::uint64_t compat = kCompatVersion;
    std::uint64_t incarnation = 0;
    /// Issued for every budget. Seeded above the largest A/R hold id in the
    /// journal this process replayed, so a successor never reuses a predecessor's
    /// id (T10c F4, E1). Uniqueness is per budget; one counter needs no map, and
    /// a u64 cannot overflow in practice (a hold per nanosecond is ~584 years).
    std::uint64_t next_hold = 1;
    bool exit_requested = false;
    std::chrono::steady_clock::time_point next_orphan_check{};
};

Owner owner_of(const SClient& client) {
    return Owner{client.ns, client.pid, client.started};
}

std::uint32_t orphan_cpu(const Server& server, std::uint64_t budget) {
    std::uint32_t n = 0;
    for (const Orphan& orphan : server.orphans) {
        n += orphan.budget == budget && orphan.hold == 0 ? 1u : 0u;
    }
    return n;
}

bool cpu_free(const Server& server, const SBudget& budget) {
    return budget.cpu_budget == 0 ||
           budget.cpu_in_use + orphan_cpu(server, budget.id) < budget.cpu_budget;
}

SBudget* find_budget(Server& server, std::uint64_t id) {
    for (SBudget& budget : server.budgets) {
        if (budget.id == id) {
            return &budget;
        }
    }
    return nullptr;
}

std::uint64_t client_reserved(const SClient& client) {
    std::uint64_t sum = 0;
    for (const RemoteHold& hold : client.holds) {
        sum = detail::sat_add(sum, hold.bytes);
    }
    return sum;
}

std::uint64_t client_resident(const SClient& client) {
    const std::uint64_t reserved = client_reserved(client);
    return client.resident > reserved ? reserved : client.resident;
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
    put64(frame.bytes, 72, info.free);
    put32(frame.bytes, 80, why_code(info.reason));
    put64(frame.bytes, 88, info.total);
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

void send_op(SClient& client, std::uint32_t op, std::uint64_t req, std::uint64_t a = 0) {
    Frame frame;
    fill_header(frame, op, client.budget, client.token, req);
    put64(frame.bytes, 32, a);
    send_frame(client, frame);
}

bool in_budget(const SClient& client, std::uint64_t budget) {
    return client.hello && client.budget == budget;
}

/// An event lowered the pending bytes: earlier readings no longer count.
void bump(SBudget& budget) {
    budget.epoch += 1;
}

void note_resident(SBudget& budget, SClient& client, std::uint64_t reported) {
    const std::uint64_t before = client_resident(client);
    client.resident = reported;
    if (client_resident(client) > before) {
        bump(budget);
    }
}

void sample_holders(Server& server, const SBudget& budget) {
    for (SClient& client : server.clients) {
        if (in_budget(client, budget.id) && !client.holds.empty()) {
            send_op(client, kStSample, 0);
        }
    }
}

void request_refresh(Server& server, SBudget& budget) {
    if (budget.internal || budget.refreshed_epoch == budget.epoch) {
        return;
    }
    budget.refreshed_epoch = budget.epoch;
    for (SClient& client : server.clients) {
        if (in_budget(client, budget.id) && client.meter) {
            send_op(client, kStRefresh, 0, budget.epoch);
        }
    }
}

/// Internal readings, measured once per decision round, after the events the
/// round follows.
struct ReadCache {
    std::optional<Reading> host;
    std::optional<Reading> guest;
};

bool has_client_meter(const Server& server, const SBudget& budget, std::uint32_t domain) {
    for (const SClient& client : server.clients) {
        if (in_budget(client, budget.id) && client.meter && client.domain == domain &&
            client.fd != kInvalid) {
            return true;
        }
    }
    return false;
}

Constraint constraint_for(Server& server, const SBudget& budget, std::uint32_t domain,
                          ReadCache& cache) {
    Constraint c;
    // The host carries every client's allocations (a VM allocates from it
    // too); the guest only those of its own clients.
    for (const SClient& client : server.clients) {
        if (!in_budget(client, budget.id)) {
            continue;
        }
        if (domain == kDomainHost || client.domain == domain) {
            const std::uint64_t resident = client_resident(client);
            c.resident = detail::sat_add(c.resident, resident);
            c.pending = detail::sat_add(c.pending, client_reserved(client) - resident);
        }
    }
    // A predecessor's promise: nothing is known about its residency, so all of
    // it is pending (its allocated part is counted twice, never zero times).
    for (const Orphan& orphan : server.orphans) {
        if (orphan.budget == budget.id && orphan.hold != 0 &&
            (domain == kDomainHost || orphan.domain == domain)) {
            c.pending = detail::sat_add(c.pending, orphan.bytes);
        }
    }
    if (budget.internal) {
        std::optional<Reading>& slot = domain == kDomainHost ? cache.host : cache.guest;
        if (!slot) {
            slot = domain == kDomainHost ? server.meters.host() : server.meters.guest();
        }
        c.meter = slot->meter;
        c.free = slot->free;
        c.total = slot->total;
        c.ready = true;
        return c;
    }
    std::vector<const SClient*> meters;
    for (const SClient& client : server.clients) {
        if (in_budget(client, budget.id) && client.meter && client.domain == domain &&
            client.fd != kInvalid) {
            meters.push_back(&client);
        }
    }
    c.meter = !meters.empty();
    c.ready = c.meter && std::all_of(meters.begin(), meters.end(), [&](const SClient* m) {
                  return m->meter_epoch == budget.epoch;
              });
    if (!c.ready) {
        return c;
    }
    // Several meters of one domain: the lowest reading bounds the request;
    // one meter without a number makes the domain unknown.
    bool free_known = true;
    bool total_known = true;
    for (const SClient* m : meters) {
        free_known = free_known && m->meter_free.has_value();
        total_known = total_known && m->meter_total.has_value();
        if (m->meter_free && (!c.free || *m->meter_free < *c.free)) {
            c.free = m->meter_free;
        }
        if (m->meter_total && (!c.total || *m->meter_total < *c.total)) {
            c.total = m->meter_total;
        }
    }
    if (!free_known) {
        c.free = std::nullopt;
    }
    if (!total_known) {
        c.total = std::nullopt;
    }
    return c;
}

View view_for(Server& server, const SBudget& budget, const SClient& self, const WaitReq& wait,
              ReadCache& cache) {
    View view;
    view.headroom = wait.headroom;
    view.stop_below = wait.stop;
    view.self_seq = wait.seq;
    std::uint64_t oldest_seq = 0;
    std::uint64_t oldest_bytes = 0;
    for (const SClient& client : server.clients) {
        if (!in_budget(client, budget.id)) {
            continue;
        }
        for (const WaitReq& other : client.waiting) {
            if (other.cpu || other.once || other.seq == 0) {
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
    // The host comes first. A guest request also needs its VM; in a test
    // budget the host bounds it when some client meters the host.
    if (self.domain == kDomainGuest) {
        if (budget.internal || has_client_meter(server, budget, kDomainHost)) {
            view.constraints.push_back(constraint_for(server, budget, kDomainHost, cache));
        }
        view.constraints.push_back(constraint_for(server, budget, kDomainGuest, cache));
    } else {
        view.constraints.push_back(constraint_for(server, budget, kDomainHost, cache));
    }
    return view;
}

struct WaitRef {
    std::size_t client = 0;
    std::uint64_t req = 0;
    std::uint64_t seq = 0;
};

std::vector<WaitRef> waiters_of(const Server& server, std::uint64_t budget_id, bool cpu) {
    std::vector<WaitRef> refs;
    for (std::size_t i = 0; i < server.clients.size(); ++i) {
        const SClient& client = server.clients[i];
        if (!in_budget(client, budget_id) || client.fd == kInvalid) {
            continue;
        }
        for (const WaitReq& wait : client.waiting) {
            if (wait.cpu == cpu) {
                refs.push_back(WaitRef{i, wait.req, wait.seq});
            }
        }
    }
    std::sort(refs.begin(), refs.end(),
              [](const WaitRef& left, const WaitRef& right) { return left.seq < right.seq; });
    return refs;
}

WaitReq* find_wait(SClient& client, std::uint64_t req) {
    for (WaitReq& one : client.waiting) {
        if (one.req == req) {
            return &one;
        }
    }
    return nullptr;
}

bool is_cancelled(const SClient& client, std::uint64_t req) {
    return std::find(client.cancelled.begin(), client.cancelled.end(), req) !=
           client.cancelled.end();
}

void finish(SClient& client, std::uint64_t req, std::uint32_t status, std::uint64_t hold,
            const Admit& info) {
    send_frame(client, decision_frame(status, client, req, hold, info));
    std::erase_if(client.cancelled, [&](std::uint64_t id) { return id == req; });
    std::erase_if(client.waiting, [&](const WaitReq& wait) { return wait.req == req; });
}

void pump(Server& server, std::uint64_t budget_id) {
    SBudget* budget = find_budget(server, budget_id);
    if (budget == nullptr) {
        return;
    }
    ReadCache cache;
    bool need_refresh = false;
    bool progressed = true;
    while (progressed) {
        progressed = false;
        for (const WaitRef& ref : waiters_of(server, budget_id, false)) {
            SClient& client = server.clients[ref.client];
            WaitReq* wait = find_wait(client, ref.req);
            if (wait == nullptr) {
                continue;
            }
            if (is_cancelled(client, wait->req)) {
                Admit info = wait->last;
                info.refused = true;
                info.admitted = false;
                info.reason = "cancelled";
                info.estimate = wait->bytes;
                finish(client, wait->req, kStRefuse, 0, info);
                progressed = true;
                break;
            }
            Admit info;
            const Decision decision =
                detail::decide(view_for(server, *budget, client, *wait, cache), wait->bytes, info);
            if (decision == Decision::NotReady) {
                need_refresh = true;
                wait->last = info;
                continue;
            }
            if (decision == Decision::Wait) {
                wait->last = info;
                if (wait->once) {
                    finish(client, wait->req, kStWait, 0, info);
                    progressed = true;
                    break;
                }
                if (!wait->told) {
                    wait->told = true;
                    send_frame(client, decision_frame(kStWait, client, wait->req, 0, info));
                }
                // Leave it queued. A later waiter may still fit beside it.
                continue;
            }
            std::uint64_t hold_id = 0;
            if (decision == Decision::Admit) {
                RemoteHold hold;
                hold.id = server.next_hold++;
                hold.bytes = wait->bytes;
                hold_id = hold.id;
                // Written before the client hears of it: a successor must find
                // every promise a client may be acting on.
                if (!server.journal.append(Journal::hold_line(budget->id, hold.id, hold.bytes,
                                                              owner_of(client), client.domain))) {
                    info.admitted = false;
                    info.refused = true;
                    info.reason = "journal unavailable";
                    finish(client, wait->req, kStRefuse, 0, info);
                    progressed = true;
                    break;
                }
                client.holds.push_back(hold);
                finish(client, wait->req, kStAdmit, hold_id, info);
            } else {
                finish(client, wait->req, kStRefuse, 0, info);
            }
            progressed = true;
            break;
        }
        if (progressed) {
            continue;
        }
        const std::vector<WaitRef> cpu = waiters_of(server, budget_id, true);
        if (cpu.empty()) {
            break;
        }
        const WaitRef& ref = cpu.front();
        SClient& client = server.clients[ref.client];
        if (is_cancelled(client, ref.req)) {
            Admit info;
            info.refused = true;
            info.reason = "cancelled";
            finish(client, ref.req, kStRefuse, 0, info);
            progressed = true;
            continue;
        }
        if (!cpu_free(server, *budget)) {
            break;
        }
        Admit info;
        if (!server.journal.append(Journal::cpu_line('C', budget->id, owner_of(client)))) {
            info.refused = true;
            info.reason = "journal unavailable";
            finish(client, ref.req, kStRefuse, 0, info);
            progressed = true;
            continue;
        }
        budget->cpu_in_use += 1;
        client.cpu += 1;
        info.admitted = true;
        info.reason = "admitted";
        finish(client, ref.req, kStAdmit, 0, info);
        progressed = true;
    }
    if (need_refresh) {
        request_refresh(server, *budget);
    }
}

/// Periodic work of a budget with waiting requests: sample the holders'
/// residency, start a new epoch (memory may have changed outside the budget)
/// and decide again; tell blocked clients what the last decision saw.
void tick(Server& server, SBudget& budget) {
    bool memory = false;
    for (const SClient& client : server.clients) {
        if (!in_budget(client, budget.id)) {
            continue;
        }
        for (const WaitReq& wait : client.waiting) {
            memory = memory || (!wait.cpu && !wait.once);
        }
    }
    if (memory) {
        sample_holders(server, budget);
        bump(budget);
    }
    pump(server, budget.id);
    for (SClient& client : server.clients) {
        if (!in_budget(client, budget.id)) {
            continue;
        }
        for (const WaitReq& wait : client.waiting) {
            if (wait.told) {
                send_frame(client, decision_frame(kStWait, client, wait.req, 0, wait.last));
            }
        }
    }
}

void handle_frame(Server& server, SClient& client, const Frame& frame) {
    if (get32(frame.bytes, 0) != kMagic) {
        client.fd = kInvalid;
        return;
    }
    const std::uint32_t op = frame_op(frame);
    if (!client.hello) {
        if (op != kOpHello) {
            client.fd = kInvalid;
            return;
        }
        // Compatibility is decided by the compatibility version, never by the
        // build id (T10c F3, D1). A client of another build but the same version
        // is served; the build id is provenance. Only a client that asks for the
        // exact build (ANNO_TEST_BUDGET_EXACT_BUILD=1) is refused on a build id.
        const std::uint32_t hello_flags = frame_g(frame);
        std::uint32_t reason = 0;
        if (frame_f(frame) != server.compat) {
            reason = kRejectIncompatible;
        } else if ((hello_flags & kFlagExactBuild) != 0 && frame_e(frame) != server.build) {
            reason = kRejectBuild;
        }
        if (reason != 0) {
            // Refused, and the client waits and retries. The coordinator steps
            // aside only when it is idle: nothing held, no orphan, no other client.
            // A busy coordinator stays and serves its clients (never terminated).
            bool idle = server.orphans.empty();
            std::uint64_t busy = server.orphans.size();
            for (const SClient& other : server.clients) {
                if (&other != &client && other.hello) {
                    idle = false;
                    ++busy;
                }
            }
            Frame reject;
            fill_header(reject, kStReject, frame_budget(frame), 0, frame_req(frame));
            put64(reject.bytes, 32, server.build);
            put64(reject.bytes, 40, server.incarnation);
            put64(reject.bytes, 48, server.compat);
            put64(reject.bytes, 56, this_pid());
            put64(reject.bytes, 64, busy);
            put32(reject.bytes, 80, idle ? kRejectSteppingAside : 0u);
            put32(reject.bytes, 84, reason);
            send_frame(client, reject);
            client.fd = kInvalid;
            server.exit_requested = server.exit_requested || idle;
            return;
        }
        client.hello = true;
        client.budget = frame_budget(frame);
        client.ns = static_cast<std::uint32_t>(frame_c(frame));
        client.pid = frame_a(frame);
        client.started = frame_b(frame);
        const std::uint32_t flags = hello_flags;
        client.meter = (flags & kFlagClientMeter) != 0;
        client.domain = frame_h(frame) == kDomainGuest ? kDomainGuest : kDomainHost;
        std::uint64_t token = 0;
        while (token == 0) {
            token =
                (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}();
        }
        client.token = token;
        SBudget* budget = find_budget(server, client.budget);
        if (budget == nullptr) {
            SBudget created;
            created.id = client.budget;
            created.internal = (flags & kFlagInternalMeter) != 0;
            created.last_tick = std::chrono::steady_clock::now();
            server.budgets.push_back(created);
            budget = &server.budgets.back();
        }
        if (budget->cpu_budget == 0) {
            budget->cpu_budget = static_cast<std::uint32_t>(frame_d(frame));
        }
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        put64(reply.bytes, 32, server.build);
        put64(reply.bytes, 40, server.incarnation);
        put64(reply.bytes, 48, server.compat);
        put64(reply.bytes, 56, this_pid());
        send_frame(client, reply);
        return;
    }
    if (frame_token(frame) != client.token || frame_budget(frame) != client.budget) {
        client.fd = kInvalid;
        return;
    }
    SBudget* budget = find_budget(server, client.budget);
    if (budget == nullptr) {
        client.fd = kInvalid;
        return;
    }
    switch (op) {
    case kOpPing:
        note_resident(*budget, client, frame_b(frame));
        if (frame_g(frame) == 1) {
            bump(*budget); // a wake: measure again
        }
        send_op(client, kStOk, frame_req(frame));
        pump(server, client.budget);
        return;
    case kOpSampleReply:
        note_resident(*budget, client, frame_b(frame));
        pump(server, client.budget);
        return;
    case kOpMeterReply:
        if (frame_a(frame) == budget->epoch) {
            client.meter_epoch = frame_a(frame);
            client.meter_free =
                frame_c(frame) == kUnknown ? std::nullopt : std::optional(frame_c(frame));
            client.meter_total =
                frame_d(frame) == kUnknown ? std::nullopt : std::optional(frame_d(frame));
            pump(server, client.budget);
        }
        return;
    case kOpCancel:
        client.cancelled.push_back(frame_a(frame));
        send_op(client, kStOk, frame_req(frame));
        pump(server, client.budget);
        return;
    case kOpRelease: {
        const std::uint64_t id = frame_a(frame);
        if (std::any_of(client.holds.begin(), client.holds.end(),
                        [&](const RemoteHold& hold) { return hold.id == id; })) {
            server.journal.append("R " + std::to_string(budget->id) + " " + std::to_string(id));
        }
        std::erase_if(client.holds, [&](const RemoteHold& hold) { return hold.id == id; });
        client.resident = frame_b(frame);
        bump(*budget);
        send_op(client, kStOk, frame_req(frame));
        pump(server, client.budget);
        return;
    }
    case kOpCpuRelease:
        if (client.cpu > 0) {
            client.cpu -= 1;
            server.journal.append(Journal::cpu_line('D', budget->id, owner_of(client)));
        }
        if (budget->cpu_in_use > 0) {
            budget->cpu_in_use -= 1;
        }
        send_op(client, kStOk, frame_req(frame));
        pump(server, client.budget);
        return;
    case kOpCpuAcquire: {
        if (cpu_free(server, *budget) && waiters_of(server, budget->id, true).empty()) {
            Admit info;
            if (!server.journal.append(Journal::cpu_line('C', budget->id, owner_of(client)))) {
                info.refused = true;
                info.reason = "journal unavailable";
                send_frame(client, decision_frame(kStRefuse, client, frame_req(frame), 0, info));
                return;
            }
            budget->cpu_in_use += 1;
            client.cpu += 1;
            info.admitted = true;
            info.reason = "admitted";
            send_frame(client, decision_frame(kStAdmit, client, frame_req(frame), 0, info));
            return;
        }
        WaitReq wait;
        wait.seq = budget->next_seq++;
        wait.req = frame_req(frame);
        wait.cpu = true;
        wait.told = true;
        wait.last.reason = "waiting";
        client.waiting.push_back(wait);
        send_frame(client, decision_frame(kStWait, client, wait.req, 0, wait.last));
        return;
    }
    case kOpReserve:
    case kOpTry: {
        note_resident(*budget, client, frame_b(frame));
        WaitReq wait;
        wait.seq = budget->next_seq++;
        wait.req = frame_req(frame);
        wait.bytes = frame_a(frame);
        wait.headroom = frame_d(frame);
        wait.stop = frame_e(frame);
        wait.once = op == kOpTry;
        wait.last.reason = "waiting for a fresh reading";
        client.waiting.push_back(wait);
        // Every request is decided on readings taken after it arrived, with
        // the holders' residency asked for again.
        bump(*budget);
        sample_holders(server, *budget);
        pump(server, client.budget);
        return;
    }
    default:
        return;
    }
}

void erase_client(Server& server, std::size_t index) {
    if (index >= server.clients.size()) {
        return;
    }
    SClient& client = server.clients[index];
    const std::uint64_t budget_id = client.budget;
    SBudget* budget = client.hello ? find_budget(server, budget_id) : nullptr;
    if (budget != nullptr) {
        for (const RemoteHold& hold : client.holds) {
            server.journal.append("R " + std::to_string(budget->id) + " " +
                                  std::to_string(hold.id));
        }
        for (std::uint32_t i = 0; i < client.cpu; ++i) {
            server.journal.append(Journal::cpu_line('D', budget->id, owner_of(client)));
        }
        if (client.cpu > 0) {
            budget->cpu_in_use =
                budget->cpu_in_use >= client.cpu ? budget->cpu_in_use - client.cpu : 0;
        }
        if (!client.holds.empty()) {
            bump(*budget);
        }
    }
    close_fd(client.fd);
    client.fd = kInvalid;
    server.clients.erase(server.clients.begin() + static_cast<std::ptrdiff_t>(index));
    if (budget != nullptr) {
        pump(server, budget_id);
    }
}

void accept_client(Server& server, Native listen_fd) {
    const Native native = accept_socket(listen_fd);
    if (native == kInvalid) {
        return;
    }
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

/// Whether the owner of an orphan still runs. Nullopt: cannot tell (the
/// orphan is kept: fail closed).
std::optional<bool> owner_alive(Server& server, const Owner& owner) {
    if (owner.ns == this_namespace()) {
#if defined(_WIN32)
        const HANDLE process =
            OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(owner.pid));
        if (process == nullptr) {
            return GetLastError() == ERROR_INVALID_PARAMETER ? std::optional<bool>(false)
                                                             : std::nullopt;
        }
        FILETIME created{};
        FILETIME exited{};
        FILETIME kernel{};
        FILETIME user{};
        DWORD code = 0;
        const bool times = GetProcessTimes(process, &created, &exited, &kernel, &user) != 0;
        const bool status = GetExitCodeProcess(process, &code) != 0;
        CloseHandle(process);
        if (!times || !status) {
            return std::nullopt;
        }
        ULARGE_INTEGER stamp{};
        stamp.LowPart = created.dwLowDateTime;
        stamp.HighPart = created.dwHighDateTime;
        return stamp.QuadPart == owner.started && code == STILL_ACTIVE;
#elif defined(__APPLE__)
        const DarwinProcess process = darwin_process(owner.pid);
        switch (process.state) {
        case DarwinProcess::State::Gone:
        case DarwinProcess::State::Zombie:
            return false;
        case DarwinProcess::State::Unknown:
            return std::nullopt;
        case DarwinProcess::State::Running:
            break;
        }
        if (process.started == 0) {
            return std::nullopt;
        }
        return process.started == owner.started;
#else
        const std::string stat = "/proc/" + std::to_string(owner.pid) + "/stat";
        if (::access(stat.c_str(), F_OK) != 0) {
            return false;
        }
        if (linux_zombie(stat)) {
            return false;
        }
        const std::uint64_t started = linux_start_time(stat);
        if (started == 0) {
            return std::nullopt;
        }
        return started == owner.started;
#endif
    }
    if (owner.ns == 1) {
        return server.meters.windows_alive(owner.pid, owner.started);
    }
    return std::nullopt;
}

/// Drops the orphans whose owner has stopped; their budgets decide again.
void check_orphans(Server& server) {
    std::map<Owner, bool> gone;
    for (const Orphan& orphan : server.orphans) {
        if (gone.count(orphan.owner) == 0) {
            const std::optional<bool> alive = owner_alive(server, orphan.owner);
            gone[orphan.owner] = alive.has_value() && !*alive;
        }
    }
    std::vector<std::uint64_t> touched;
    for (const Orphan& orphan : server.orphans) {
        if (gone[orphan.owner]) {
            touched.push_back(orphan.budget);
            server.journal.append(orphan.hold != 0
                                      ? "R " + std::to_string(orphan.budget) + " " +
                                            std::to_string(orphan.hold)
                                      : Journal::cpu_line('D', orphan.budget, orphan.owner));
        }
    }
    if (touched.empty()) {
        return;
    }
    std::erase_if(server.orphans, [&](const Orphan& orphan) { return gone[orphan.owner]; });
    std::sort(touched.begin(), touched.end());
    touched.erase(std::unique(touched.begin(), touched.end()), touched.end());
    for (const std::uint64_t id : touched) {
        if (SBudget* budget = find_budget(server, id)) {
            bump(*budget);
            pump(server, id);
        }
    }
}

/// The open promises now: live holds and tokens, then orphans.
std::vector<std::string> journal_state(const Server& server) {
    std::vector<std::string> lines = Journal::lines_for(server.orphans);
    for (const SClient& client : server.clients) {
        if (!client.hello) {
            continue;
        }
        for (const RemoteHold& hold : client.holds) {
            lines.push_back(Journal::hold_line(client.budget, hold.id, hold.bytes, owner_of(client),
                                               client.domain));
        }
        for (std::uint32_t i = 0; i < client.cpu; ++i) {
            lines.push_back(Journal::cpu_line('C', client.budget, owner_of(client)));
        }
    }
    return lines;
}

std::filesystem::path default_journal(std::uint16_t port) {
    // ANNO_TEST_BUDGET_JOURNAL (tests only): one journal for a coordinator and
    // the successor a client starts with the same environment (T10c F3).
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_JOURNAL")) {
        if (env[0] != '\0') {
            return env;
        }
    }
    const std::string name = "coord-" + std::to_string(port) + ".journal";
#if defined(_WIN32)
    if (const char* local = std::getenv("LOCALAPPDATA")) {
        if (local[0] != '\0') {
            return std::filesystem::path(local) / "anno1503" / name;
        }
    }
#else
    if (const char* home = std::getenv("HOME")) {
        if (home[0] != '\0') {
            return std::filesystem::path(home) / ".cache" / "anno1503" / name;
        }
    }
#endif
    return std::filesystem::temp_directory_path() / "anno1503" / name;
}

Native listen_on(std::uint16_t port, bool all_interfaces) {
    const Native fd = open_socket();
    if (fd == kInvalid) {
        return kInvalid;
    }
#if defined(_WIN32)
    const SOCKET sock = static_cast<SOCKET>(fd);
    // No second listener may share the port (Windows' SO_REUSEADDR would let
    // one in).
    int exclusive = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive),
                 sizeof(exclusive));
#else
    const int sock = static_cast<int>(fd);
    // Linux never lets two sockets listen on one port; this only lets a
    // replacement bind while the predecessor's closed connections linger.
    int reuse = 1;
    ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(all_interfaces ? 0u : 0x7f000001u);
#if defined(_WIN32)
    if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(sock, 128) != 0) {
        ::closesocket(sock);
        return kInvalid;
    }
#else
    if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(sock, 128) != 0) {
        ::close(sock);
        return kInvalid;
    }
#endif
    return fd;
}

} // namespace

std::uint64_t coordinator_build() {
#if defined(ANNO_COORD_BUILD)
    return ANNO_COORD_BUILD;
#else
    return 0;
#endif
}

int run_budget_server(const BudgetServerOptions& options) {
#if !defined(_WIN32)
    for (int fd = 3; fd < 256; ++fd) {
        ::close(fd);
    }
#endif
    const std::uint16_t port = options.port;
    const Native listen_fd = listen_on(port, options.all_interfaces);
    if (listen_fd == kInvalid) {
        std::printf("BIND-FAILED %u\n", static_cast<unsigned>(port));
        std::fflush(stdout);
        return 3;
    }
    // Only the process that holds the port owns the journal.
    Server server;
    server.build = options.build ? *options.build : coordinator_build();
    server.compat = options.compat ? *options.compat : kCompatVersion;
    while (server.incarnation == 0) {
        server.incarnation =
            (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}();
    }
    const std::filesystem::path journal =
        options.journal.empty() ? default_journal(port) : options.journal;
    if (!server.journal.open(journal, server.orphans)) {
        std::printf("JOURNAL-FAILED %s\n", journal.string().c_str());
        std::fflush(stdout);
        close_fd(listen_fd);
        return 4;
    }
    server.next_hold = server.journal.max_hold_id() + 1;
#if !defined(_WIN32)
    // Test-only (T10c F4, N4). Never set in production. Opened without
    // O_CLOEXEC after the start-up close of 3..255, so a meter spawn that
    // forgets closefrom inherits it. Documented with the other test-only hooks.
    if (const char* leak = std::getenv("ANNO_TEST_COORD_LEAK_FD")) {
        if (leak[0] != '\0') {
            const int leaked = ::open(leak, O_RDONLY);
            (void)leaked;
        }
    }
#endif
    server.meters.start();
    // A test that starts its own coordinator waits for this line.
    std::printf("LISTENING %u build=%016llx compat=%llu incarnation=%016llx orphans=%zu\n",
                static_cast<unsigned>(port), static_cast<unsigned long long>(server.build),
                static_cast<unsigned long long>(server.compat),
                static_cast<unsigned long long>(server.incarnation), server.orphans.size());
    std::fflush(stdout);
    const auto period = detail::recheck_period();
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
        const int wait_ms = static_cast<int>(std::min<long long>(period.count(), 100));
#if defined(_WIN32)
        const int ready = WSAPoll(polls.data(), static_cast<ULONG>(polls.size()), wait_ms);
#else
        const int ready = ::poll(polls.data(), static_cast<nfds_t>(polls.size()), wait_ms);
#endif
        if (ready > 0 && (polls.at(0).revents & POLLIN) != 0) {
            accept_client(server, listen_fd);
        }
        if (ready > 0) {
            for (std::size_t i = 0; i + 1 < polls.size(); ++i) {
                const short revents = polls.at(i + 1).revents;
                if ((revents & POLLIN) != 0) {
                    const std::size_t before = server.clients.size();
                    pull_client(server, i);
                    if (server.clients.size() != before) {
                        break;
                    }
                } else if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                    erase_client(server, i);
                    break;
                }
            }
        }
        server.meters.poll();
        const auto now = std::chrono::steady_clock::now();
        if (!server.orphans.empty() && now >= server.next_orphan_check) {
            server.next_orphan_check = now + std::chrono::milliseconds(250);
            check_orphans(server);
        }
        if (server.journal.wants_compaction()) {
            server.journal.rewrite(journal_state(server));
        }
        if (server.exit_requested) {
            break;
        }
        for (std::size_t b = 0; b < server.budgets.size(); ++b) {
            if (now - server.budgets[b].last_tick >= period) {
                server.budgets[b].last_tick = now;
                tick(server, server.budgets[b]);
            }
        }
        if (server.clients.empty()) {
            // Orphans keep it alive: a successor would only find them again.
            if (server.orphans.empty() && now - idle_since > std::chrono::seconds(30)) {
                break;
            }
        } else {
            idle_since = now;
        }
    }
    close_fd(listen_fd);
    return 0;
}

} // namespace anno::test
