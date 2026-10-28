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
// Shortfalls. More than a domain can ever give (its total minus the headroom)
// is refused as "exceeds capacity". Anything else waits: reservations end, and
// memory used outside the budget can come back. Waits are bounded by the
// client's deadline, which withdraws the request; nothing here admits by time.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <random>
#include <string>
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
        if (::pipe(in_pipe) != 0) {
            unavailable_ = true;
            return;
        }
        if (::pipe(out_pipe) != 0) {
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
        std::string exe = fixed;
        std::string a1 = "-NoProfile";
        std::string a2 = "-NonInteractive";
        std::string a3 = "-ExecutionPolicy";
        std::string a4 = "Bypass";
        std::string a5 = "-EncodedCommand";
        char* argv[] = {exe.data(), a1.data(), a2.data(), a3.data(),
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
    std::uint64_t next_hold = 1;
    std::uint64_t next_seq = 1;
    bool internal = false;
    /// Raised by every event that lowers the pending bytes (see the top).
    std::uint64_t epoch = 1;
    std::uint64_t refreshed_epoch = 0;
    std::chrono::steady_clock::time_point last_tick{};
};

struct Server {
    std::vector<SClient> clients;
    std::vector<SBudget> budgets;
    Meters meters;
};

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
                hold.id = budget->next_hold++;
                hold.bytes = wait->bytes;
                hold_id = hold.id;
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
        if (budget->cpu_budget != 0 && budget->cpu_in_use >= budget->cpu_budget) {
            break;
        }
        budget->cpu_in_use += 1;
        client.cpu += 1;
        Admit info;
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
        if (op != kOpHello || frame_f(frame) != kProtocol) {
            client.fd = kInvalid;
            return;
        }
        client.hello = true;
        client.budget = frame_budget(frame);
        client.ns = static_cast<std::uint32_t>(frame_c(frame));
        client.pid = frame_a(frame);
        client.started = frame_b(frame);
        const std::uint32_t flags = frame_g(frame);
        client.meter = (flags & kFlagClientMeter) != 0;
        client.domain = frame_h(frame) == kDomainGuest ? kDomainGuest : kDomainHost;
        std::uint64_t token = 0;
        while (token == 0) {
            token =
                (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}();
        }
        client.token = token;
        if (find_budget(server, client.budget) == nullptr) {
            SBudget created;
            created.id = client.budget;
            created.internal = (flags & kFlagInternalMeter) != 0;
            created.last_tick = std::chrono::steady_clock::now();
            server.budgets.push_back(created);
        }
        SBudget* budget = find_budget(server, client.budget);
        if (budget->cpu_budget == 0) {
            budget->cpu_budget = static_cast<std::uint32_t>(frame_d(frame));
        }
        Frame reply;
        fill_header(reply, kStOk, client.budget, client.token, frame_req(frame));
        put64(reply.bytes, 48, kProtocol);
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
        }
        if (budget->cpu_in_use > 0) {
            budget->cpu_in_use -= 1;
        }
        send_op(client, kStOk, frame_req(frame));
        pump(server, client.budget);
        return;
    case kOpCpuAcquire: {
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
        if (client.cpu > 0) {
            budget->cpu_in_use = budget->cpu_in_use >= client.cpu ? budget->cpu_in_use - client.cpu : 0;
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
    // No second listener may share the port (Windows' SO_REUSEADDR would let
    // one in).
    int exclusive = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive),
                 sizeof(exclusive));
#else
    const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return kInvalid;
    }
    // Linux never lets two sockets listen on one port; this only lets a
    // replacement bind while the predecessor's closed connections linger.
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
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

} // namespace

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
    Server server;
    server.meters.start();
    // A test that starts its own coordinator waits for this line.
    std::printf("LISTENING %u\n", static_cast<unsigned>(port));
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
        for (std::size_t b = 0; b < server.budgets.size(); ++b) {
            if (now - server.budgets[b].last_tick >= period) {
                server.budgets[b].last_tick = now;
                tick(server, server.budgets[b]);
            }
        }
        if (server.clients.empty()) {
            if (now - idle_since > std::chrono::seconds(30)) {
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
