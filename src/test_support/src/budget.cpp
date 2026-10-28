// Host memory reservations for the test harness. In-memory budgets are the
// unit-test double. The process budget is a file of fixed-width fields so
// Windows and WSL clients subtract the same pending bytes from free memory.
#include "anno/test/budget.hpp"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "anno/test/parallel.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace anno::test {
namespace {

constexpr std::uint32_t kMagic = 0x31424E41u; // 'ANB1' little-endian
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kHeaderBytes = 64;
constexpr std::uint32_t kSlotBytes = 40;
// Coordination-table size, fixed when the file is created so every client
// reads the same bytes. Not a game limit. ANNO_TEST_BUDGET_SLOTS changes it
// for a file that does not exist yet.
constexpr std::uint32_t kDefaultSlots = 64;

std::uint64_t sat_add(std::uint64_t a, std::uint64_t b) {
    return a > std::numeric_limits<std::uint64_t>::max() - b
               ? std::numeric_limits<std::uint64_t>::max()
               : a + b;
}

enum class Decision { Admit, Wait, Refuse };

struct View {
    bool free_known = false;
    std::uint64_t free = 0;
    std::uint64_t headroom = 0;
    std::uint64_t stop_below = 0;
    std::uint64_t pending = 0;
    std::uint64_t resident = 0;
    std::uint64_t oldest_bytes = 0;
    std::uint64_t oldest_seq = 0;
    std::uint64_t self_seq = 0;
};

Decision decide(const View& view, std::uint64_t bytes, Admit& info) {
    info = Admit{};
    info.estimate = bytes;
    info.headroom = view.headroom;
    info.pending = view.pending;
    info.resident = view.resident;
    if (!view.free_known) {
        info.admitted = true;
        info.reason = "free memory unknown";
        return Decision::Admit;
    }
    const std::uint64_t blocked = sat_add(view.headroom, view.pending);
    info.available = view.free > blocked ? view.free - blocked : 0;
    const std::uint64_t with_release = sat_add(view.free, view.resident);
    const std::uint64_t max_possible =
        with_release > view.headroom ? with_release - view.headroom : 0;
    if (view.stop_below > 0 && view.free < view.stop_below) {
        info.refused = true;
        info.reason = "low-memory stop";
        return Decision::Refuse;
    }
    if (bytes > max_possible) {
        info.refused = true;
        info.reason = "exceeds capacity";
        return Decision::Refuse;
    }
    bool allowed = bytes <= info.available;
    if (allowed && view.oldest_bytes > 0 && view.self_seq != view.oldest_seq) {
        allowed = info.available - bytes >= view.oldest_bytes;
    }
    if (allowed) {
        info.admitted = true;
        info.reason = "admitted";
        return Decision::Admit;
    }
    info.reason = "waiting";
    return Decision::Wait;
}

struct Hold {
    std::uint64_t id = 0;
    std::uint64_t bytes = 0;
    bool resident = false;
};

void observe(std::vector<Hold>& holds, bool& baseline_set, std::uint64_t& baseline,
             const BudgetHooks& hooks) {
    if (hooks.process_private == nullptr) {
        return;
    }
    const std::optional<std::uint64_t> private_bytes = hooks.process_private();
    if (!private_bytes) {
        return;
    }
    if (!baseline_set) {
        baseline = *private_bytes;
        baseline_set = true;
    }
    std::uint64_t grown = *private_bytes > baseline ? *private_bytes - baseline : 0;
    for (Hold& hold : holds) {
        if (grown >= hold.bytes) {
            hold.resident = true;
            grown -= hold.bytes;
        } else {
            hold.resident = false;
        }
    }
}

std::uint64_t sum_reserved(const std::vector<Hold>& holds) {
    std::uint64_t sum = 0;
    for (const Hold& hold : holds) {
        sum = sat_add(sum, hold.bytes);
    }
    return sum;
}

std::uint64_t sum_resident(const std::vector<Hold>& holds) {
    std::uint64_t sum = 0;
    for (const Hold& hold : holds) {
        if (hold.resident) {
            sum = sat_add(sum, hold.bytes);
        }
    }
    return sum;
}

void emit_log(bool enabled, const std::filesystem::path& log_path, const std::string& line) {
    if (!enabled) {
        return;
    }
    std::fprintf(stderr, "%s\n", line.c_str());
    std::fflush(stderr);
    if (log_path.empty()) {
        return;
    }
    std::FILE* file = std::fopen(log_path.string().c_str(), "ab");
    if (file == nullptr) {
        return;
    }
    std::fwrite(line.data(), 1, line.size(), file);
    std::fputc('\n', file);
    std::fflush(file);
#if defined(_WIN32)
    _commit(_fileno(file));
#else
    fsync(fileno(file));
#endif
    std::fclose(file);
}

std::string num(std::uint64_t value) {
    return std::to_string(value);
}

std::string admit_line(const char* kind, const Admit& info) {
    return std::string("[anno-test memory] ") + kind + " estimate=" + num(info.estimate) +
           " available=" + num(info.available) + " headroom=" + num(info.headroom) +
           " pending=" + num(info.pending) + " resident=" + num(info.resident) +
           " reason=" + info.reason;
}

struct Waiter {
    std::uint64_t seq = 0;
    std::uint64_t bytes = 0;
    int slot = -1;
};

struct MemTable {
    std::mutex mu;
    std::condition_variable cv;
    struct Slot {
        BudgetHooks hooks;
        std::vector<Hold> holds;
        bool baseline_set = false;
        std::uint64_t baseline = 0;
    };
    std::vector<Slot> slots;
    std::vector<Waiter> waiters;
    std::uint64_t next_seq = 1;
    std::uint64_t next_id = 1;
};

struct MemEval {
    Decision decision = Decision::Refuse;
    Admit info;
};

MemEval evaluate_mem(MemTable& table, int slot, std::uint64_t bytes, std::uint64_t headroom,
                     std::uint64_t stop_below, std::uint64_t self_seq) {
    std::uint64_t pending = 0;
    std::uint64_t resident = 0;
    for (MemTable::Slot& one : table.slots) {
        observe(one.holds, one.baseline_set, one.baseline, one.hooks);
        for (const Hold& hold : one.holds) {
            if (hold.resident) {
                resident = sat_add(resident, hold.bytes);
            } else {
                pending = sat_add(pending, hold.bytes);
            }
        }
    }
    std::uint64_t oldest_seq = 0;
    std::uint64_t oldest_bytes = 0;
    for (const Waiter& waiter : table.waiters) {
        if (oldest_seq == 0 || waiter.seq < oldest_seq) {
            oldest_seq = waiter.seq;
            oldest_bytes = waiter.bytes;
        }
    }
    View view;
    view.headroom = headroom;
    view.stop_below = stop_below;
    view.pending = pending;
    view.resident = resident;
    view.oldest_bytes = oldest_bytes;
    view.oldest_seq = oldest_seq;
    view.self_seq = self_seq;
    const BudgetHooks& hooks = table.slots.at(static_cast<std::size_t>(slot)).hooks;
    if (hooks.free_bytes != nullptr) {
        if (const std::optional<std::uint64_t> free_bytes = hooks.free_bytes()) {
            view.free_known = true;
            view.free = *free_bytes;
        }
    }
    MemEval out;
    out.decision = decide(view, bytes, out.info);
    return out;
}

std::uint32_t get_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::uint64_t get_u64(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | bytes[offset + static_cast<std::size_t>(i)];
    }
    return value;
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

void put_u64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        bytes[offset + static_cast<std::size_t>(i)] =
            static_cast<std::uint8_t>((value >> (8 * i)) & 0xffu);
    }
}

std::uint64_t this_pid() {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

bool pid_alive(std::uint64_t pid) {
    if (pid == 0) {
        return false;
    }
    if (pid == this_pid()) {
        return true;
    }
#if defined(_WIN32)
    if (pid > 0xffffffffu) {
        return false;
    }
    const HANDLE process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process == nullptr) {
        return false;
    }
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(process, &code);
    CloseHandle(process);
    return ok != 0 && code == STILL_ACTIVE;
#else
    if (pid > static_cast<std::uint64_t>(std::numeric_limits<pid_t>::max())) {
        return false;
    }
    if (kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;
    }
    return errno == EPERM;
#endif
}

std::uint32_t configured_slots() {
    const std::uint64_t n = detail::env_count("ANNO_TEST_BUDGET_SLOTS").value_or(kDefaultSlots);
    if (n < 1) {
        return 1;
    }
    if (n > 256) {
        return 256;
    }
    return static_cast<std::uint32_t>(n);
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

struct Image {
    std::vector<std::uint8_t> bytes;

    std::uint32_t slot_count() const { return get_u32(bytes, 8); }
    std::uint32_t cpu_budget() const { return get_u32(bytes, 12); }
    std::uint32_t cpu_in_use() const { return get_u32(bytes, 16); }
    std::uint64_t oldest_seq() const { return get_u64(bytes, 24); }
    std::uint64_t oldest_bytes() const { return get_u64(bytes, 32); }
    std::uint64_t oldest_pid() const { return get_u64(bytes, 40); }
    std::uint64_t oldest_token() const { return get_u64(bytes, 48); }
    std::uint64_t next_seq() const { return get_u64(bytes, 56); }

    void set_cpu_budget(std::uint32_t value) { put_u32(bytes, 12, value); }
    void set_cpu_in_use(std::uint32_t value) { put_u32(bytes, 16, value); }
    void set_oldest_seq(std::uint64_t value) { put_u64(bytes, 24, value); }
    void set_oldest_bytes(std::uint64_t value) { put_u64(bytes, 32, value); }
    void set_oldest_pid(std::uint64_t value) { put_u64(bytes, 40, value); }
    void set_oldest_token(std::uint64_t value) { put_u64(bytes, 48, value); }
    void set_next_seq(std::uint64_t value) { put_u64(bytes, 56, value); }

    std::size_t slot_offset(std::uint32_t index) const {
        return static_cast<std::size_t>(kHeaderBytes) +
               static_cast<std::size_t>(index) * kSlotBytes;
    }

    std::uint64_t slot_pid(std::uint32_t index) const { return get_u64(bytes, slot_offset(index)); }
    std::uint64_t slot_token(std::uint32_t index) const {
        return get_u64(bytes, slot_offset(index) + 8);
    }
    std::uint64_t slot_reserved(std::uint32_t index) const {
        return get_u64(bytes, slot_offset(index) + 16);
    }
    std::uint64_t slot_resident(std::uint32_t index) const {
        return get_u64(bytes, slot_offset(index) + 24);
    }
    std::uint32_t slot_cpu(std::uint32_t index) const {
        return get_u32(bytes, slot_offset(index) + 32);
    }

    void set_slot_pid(std::uint32_t index, std::uint64_t value) {
        put_u64(bytes, slot_offset(index), value);
    }
    void set_slot_token(std::uint32_t index, std::uint64_t value) {
        put_u64(bytes, slot_offset(index) + 8, value);
    }
    void set_slot_reserved(std::uint32_t index, std::uint64_t value) {
        put_u64(bytes, slot_offset(index) + 16, value);
    }
    void set_slot_resident(std::uint32_t index, std::uint64_t value) {
        put_u64(bytes, slot_offset(index) + 24, value);
    }
    void set_slot_cpu(std::uint32_t index, std::uint32_t value) {
        put_u32(bytes, slot_offset(index) + 32, value);
    }

    static Image fresh(std::uint32_t slots, std::uint32_t cpu) {
        Image image;
        image.bytes.assign(static_cast<std::size_t>(kHeaderBytes) +
                               static_cast<std::size_t>(slots) * kSlotBytes,
                           0);
        put_u32(image.bytes, 0, kMagic);
        put_u32(image.bytes, 4, kVersion);
        put_u32(image.bytes, 8, slots);
        put_u32(image.bytes, 12, cpu);
        put_u64(image.bytes, 56, 1);
        return image;
    }

    bool valid() const {
        return bytes.size() >= kHeaderBytes && get_u32(bytes, 0) == kMagic &&
               get_u32(bytes, 4) == kVersion && slot_count() >= 1 &&
               bytes.size() >= static_cast<std::size_t>(kHeaderBytes) +
                                   static_cast<std::size_t>(slot_count()) * kSlotBytes;
    }
};

void reclaim_image(Image& image) {
    std::uint32_t cpu = image.cpu_in_use();
    const std::uint32_t slots = image.slot_count();
    for (std::uint32_t i = 0; i < slots; ++i) {
        const std::uint64_t pid = image.slot_pid(i);
        if (pid != 0 && !pid_alive(pid)) {
            const std::uint32_t held = image.slot_cpu(i);
            if (held >= cpu) {
                cpu = 0;
            } else {
                cpu -= held;
            }
            if (image.oldest_pid() == pid) {
                image.set_oldest_seq(0);
                image.set_oldest_bytes(0);
                image.set_oldest_pid(0);
                image.set_oldest_token(0);
            }
            image.set_slot_pid(i, 0);
            image.set_slot_token(i, 0);
            image.set_slot_reserved(i, 0);
            image.set_slot_resident(i, 0);
            image.set_slot_cpu(i, 0);
        }
    }
    if (!pid_alive(image.oldest_pid())) {
        image.set_oldest_seq(0);
        image.set_oldest_bytes(0);
        image.set_oldest_pid(0);
        image.set_oldest_token(0);
    }
    image.set_cpu_in_use(cpu);
}

struct OsFile {
#if defined(_WIN32)
    HANDLE handle = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
#else
    int fd = -1;
    sem_t* sem = nullptr;
#endif
    // LockFileEx / flock are per process. A second thread in this process
    // that locks the same handle waits for the process to release it, which
    // never happens while the first thread is the one holding it. The mutex
    // lets only one thread of this process take the file lock.
    std::mutex threads;

    explicit OsFile(const std::filesystem::path& path) {
        std::error_code error;
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path(), error);
        }
#if defined(_WIN32)
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("could not open the test budget file " + path.string());
        }
        const std::wstring name = L"Local\\anno1503-" + hex16(hash_path(path));
        event = CreateEventW(nullptr, TRUE, FALSE, name.c_str());
        if (event == nullptr) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
            throw std::runtime_error("could not open the test budget event");
        }
#else
        fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
        if (fd < 0) {
            throw std::runtime_error("could not open the test budget file " + path.string());
        }
        const std::string name = "/anno1503-" + hex16_utf8(hash_path(path));
        sem = sem_open(name.c_str(), O_CREAT, 0644, 0);
        if (sem == SEM_FAILED) {
            ::close(fd);
            fd = -1;
            sem = nullptr;
            throw std::runtime_error("could not open the test budget semaphore");
        }
#endif
    }

    ~OsFile() {
#if defined(_WIN32)
        if (event != nullptr) {
            CloseHandle(event);
        }
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
#else
        if (sem != nullptr) {
            sem_close(sem);
        }
        if (fd >= 0) {
            ::close(fd);
        }
#endif
    }

    OsFile(const OsFile&) = delete;
    OsFile& operator=(const OsFile&) = delete;

    void lock() {
        threads.lock();
#if defined(_WIN32)
        OVERLAPPED overlapped{};
        if (LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 0xffffffffu, 0xffffffffu, &overlapped) ==
            0) {
            threads.unlock();
            throw std::runtime_error("could not lock the test budget file");
        }
#else
        if (flock(fd, LOCK_EX) != 0) {
            threads.unlock();
            throw std::runtime_error("could not lock the test budget file");
        }
#endif
    }

    void unlock() noexcept {
#if defined(_WIN32)
        OVERLAPPED overlapped{};
        UnlockFileEx(handle, 0, 0xffffffffu, 0xffffffffu, &overlapped);
#else
        flock(fd, LOCK_UN);
#endif
        threads.unlock();
    }

    std::vector<std::uint8_t> read_all() {
#if defined(_WIN32)
        LARGE_INTEGER size{};
        if (GetFileSizeEx(handle, &size) == 0 || size.QuadPart < 0) {
            throw std::runtime_error("could not read the test budget file");
        }
        std::vector<std::uint8_t> out(static_cast<std::size_t>(size.QuadPart));
        if (out.empty()) {
            return out;
        }
        if (SetFilePointer(handle, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
            throw std::runtime_error("could not read the test budget file");
        }
        DWORD got = 0;
        if (ReadFile(handle, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) == 0) {
            throw std::runtime_error("could not read the test budget file");
        }
        out.resize(got);
        return out;
#else
        const off_t size = ::lseek(fd, 0, SEEK_END);
        if (size < 0) {
            throw std::runtime_error("could not read the test budget file");
        }
        std::vector<std::uint8_t> out(static_cast<std::size_t>(size));
        std::size_t got = 0;
        while (got < out.size()) {
            const ssize_t n =
                ::pread(fd, out.data() + got, out.size() - got, static_cast<off_t>(got));
            if (n < 0) {
                throw std::runtime_error("could not read the test budget file");
            }
            if (n == 0) {
                break;
            }
            got += static_cast<std::size_t>(n);
        }
        out.resize(got);
        return out;
#endif
    }

    void write_all(const std::vector<std::uint8_t>& bytes) {
#if defined(_WIN32)
        if (SetFilePointer(handle, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
            throw std::runtime_error("could not write the test budget file");
        }
        DWORD wrote = 0;
        if (!bytes.empty() && (WriteFile(handle, bytes.data(), static_cast<DWORD>(bytes.size()),
                                         &wrote, nullptr) == 0 ||
                               wrote != bytes.size())) {
            throw std::runtime_error("could not write the test budget file");
        }
        if (SetFilePointer(handle, static_cast<LONG>(bytes.size()), nullptr, FILE_BEGIN) ==
            INVALID_SET_FILE_POINTER) {
            throw std::runtime_error("could not write the test budget file");
        }
        if (SetEndOfFile(handle) == 0) {
            throw std::runtime_error("could not write the test budget file");
        }
#else
        std::size_t wrote = 0;
        while (wrote < bytes.size()) {
            const ssize_t n =
                ::pwrite(fd, bytes.data() + wrote, bytes.size() - wrote, static_cast<off_t>(wrote));
            if (n < 0) {
                throw std::runtime_error("could not write the test budget file");
            }
            wrote += static_cast<std::size_t>(n);
        }
        if (::ftruncate(fd, static_cast<off_t>(bytes.size())) != 0) {
            throw std::runtime_error("could not write the test budget file");
        }
#endif
    }

    void signal() noexcept {
#if defined(_WIN32)
        if (event != nullptr) {
            SetEvent(event);
        }
#else
        if (sem != nullptr) {
            sem_post(sem);
        }
#endif
    }

    void reset_signal() noexcept {
#if defined(_WIN32)
        if (event != nullptr) {
            ResetEvent(event);
        }
#else
        if (sem != nullptr) {
            while (sem_trywait(sem) == 0) {
            }
        }
#endif
    }

    void wait_signal() {
#if defined(_WIN32)
        WaitForSingleObject(event, INFINITE);
#else
        while (sem_wait(sem) != 0) {
            if (errno != EINTR) {
                return;
            }
        }
#endif
    }

  private:
    static std::uint64_t hash_path(const std::filesystem::path& path) {
        const std::string text = path.generic_string();
        std::uint64_t hash = 14695981039346656037ull;
        for (const unsigned char c : text) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    static std::string hex16_utf8(std::uint64_t value) {
        static constexpr char kHex[] = "0123456789abcdef";
        std::string out(16, '0');
        for (int i = 15; i >= 0; --i) {
            out[static_cast<std::size_t>(i)] = kHex[value & 0xfu];
            value >>= 4;
        }
        return out;
    }

#if defined(_WIN32)
    static std::wstring hex16(std::uint64_t value) {
        const std::string text = hex16_utf8(value);
        return std::wstring(text.begin(), text.end());
    }
#endif
};

class FileLock {
  public:
    explicit FileLock(OsFile& file) : file_(&file) { file_->lock(); }
    ~FileLock() {
        if (file_ != nullptr) {
            file_->unlock();
        }
    }
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
    void unlock() noexcept {
        if (file_ != nullptr) {
            file_->unlock();
            file_ = nullptr;
        }
    }

  private:
    OsFile* file_;
};

std::filesystem::path default_budget_path() {
    if (const char* env = std::getenv("ANNO_TEST_BUDGET_FILE")) {
        if (env[0] != '\0') {
            return env;
        }
    }
#if defined(_WIN32)
    if (const char* local = std::getenv("LOCALAPPDATA")) {
        if (local[0] != '\0') {
            return std::filesystem::path(local) / "anno1503" / "test-budget.bin";
        }
    }
#else
    if (const char* user = std::getenv("USER")) {
        if (user[0] != '\0') {
            const std::filesystem::path local =
                std::filesystem::path("/mnt/c/Users") / user / "AppData" / "Local";
            std::error_code error;
            if (std::filesystem::is_directory(local, error)) {
                return local / "anno1503" / "test-budget.bin";
            }
        }
    }
    if (const char* home = std::getenv("HOME")) {
        if (home[0] != '\0') {
            return std::filesystem::path(home) / ".cache" / "anno1503" / "test-budget.bin";
        }
    }
#endif
    return std::filesystem::temp_directory_path() / "anno1503" / "test-budget.bin";
}

std::filesystem::path log_path_for(const std::filesystem::path& budget_path) {
    if (const char* env = std::getenv("ANNO_TEST_MEMORY_LOG")) {
        if (env[0] != '\0') {
            return env;
        }
    }
    if (budget_path.empty()) {
        return {};
    }
    return budget_path.parent_path() / "memory.log";
}

struct FileTable {
    std::filesystem::path path;
    std::filesystem::path log_path;
    OsFile os;
    std::vector<Hold> holds;
    std::vector<Waiter> waiters;
    bool baseline_set = false;
    std::uint64_t baseline = 0;
    std::uint64_t token = 0;
    std::uint32_t slot = 0;
    std::uint64_t next_id = 1;
    bool claimed = false;

    explicit FileTable(std::filesystem::path path_)
        : path(std::move(path_)), log_path(log_path_for(path)), os(path) {}

    ~FileTable() {
        if (!claimed) {
            return;
        }
        try {
            FileLock lock(os);
            Image image = locked_image();
            if (slot < image.slot_count() && image.slot_token(slot) == token &&
                image.slot_pid(slot) == this_pid()) {
                const std::uint32_t mine = image.slot_cpu(slot);
                std::uint32_t used = image.cpu_in_use();
                if (mine >= used) {
                    used = 0;
                } else {
                    used -= mine;
                }
                image.set_cpu_in_use(used);
                if (image.oldest_pid() == this_pid() && image.oldest_token() == token) {
                    image.set_oldest_seq(0);
                    image.set_oldest_bytes(0);
                    image.set_oldest_pid(0);
                    image.set_oldest_token(0);
                }
                image.set_slot_pid(slot, 0);
                image.set_slot_token(slot, 0);
                image.set_slot_reserved(slot, 0);
                image.set_slot_resident(slot, 0);
                image.set_slot_cpu(slot, 0);
                os.write_all(image.bytes);
                os.signal();
            }
        } catch (...) {
        }
    }

    Image locked_image() {
        Image image;
        image.bytes = os.read_all();
        if (!image.valid()) {
            if (!image.bytes.empty()) {
                bool zero = true;
                for (const std::uint8_t byte : image.bytes) {
                    if (byte != 0) {
                        zero = false;
                        break;
                    }
                }
                if (!zero) {
                    throw std::runtime_error("the test budget file has an unexpected header");
                }
            }
            image = Image::fresh(configured_slots(), configured_cpu());
            os.write_all(image.bytes);
        } else if (image.cpu_budget() == 0) {
            image.set_cpu_budget(configured_cpu());
            os.write_all(image.bytes);
        }
        reclaim_image(image);
        return image;
    }

    void claim(Image& image) {
        if (claimed) {
            return;
        }
        std::uint64_t nonce = 0;
        while (nonce == 0) {
            nonce = (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^
                    std::random_device{}() ^ this_pid();
        }
        const std::uint32_t slots = image.slot_count();
        for (std::uint32_t i = 0; i < slots; ++i) {
            if (image.slot_pid(i) == 0) {
                image.set_slot_pid(i, this_pid());
                image.set_slot_token(i, nonce);
                slot = i;
                token = nonce;
                claimed = true;
                os.write_all(image.bytes);
                return;
            }
        }
        throw std::runtime_error("the test budget file has no free slot");
    }

    void sync_slot(Image& image) {
        image.set_slot_reserved(slot, sum_reserved(holds));
        image.set_slot_resident(slot, sum_resident(holds));
    }

    void publish_oldest_from(const Waiter& waiter, Image& image) {
        image.set_oldest_seq(waiter.seq);
        image.set_oldest_bytes(waiter.bytes);
        image.set_oldest_pid(this_pid());
        image.set_oldest_token(token);
    }

    void clear_oldest_if_mine(Image& image, std::uint64_t seq) {
        if (image.oldest_seq() != seq) {
            return;
        }
        const Waiter* next = nullptr;
        for (const Waiter& waiter : waiters) {
            if (waiter.seq == seq) {
                continue;
            }
            if (next == nullptr || waiter.seq < next->seq) {
                next = &waiter;
            }
        }
        if (next == nullptr) {
            image.set_oldest_seq(0);
            image.set_oldest_bytes(0);
            image.set_oldest_pid(0);
            image.set_oldest_token(0);
        } else {
            publish_oldest_from(*next, image);
        }
    }
};

MemEval evaluate_file(FileTable& table, Image& image, const BudgetHooks& hooks, std::uint64_t bytes,
                      std::uint64_t headroom, std::uint64_t stop_below, std::uint64_t self_seq) {
    observe(table.holds, table.baseline_set, table.baseline, hooks);
    table.sync_slot(image);
    std::uint64_t pending = 0;
    std::uint64_t resident = 0;
    for (std::uint32_t i = 0; i < image.slot_count(); ++i) {
        if (image.slot_pid(i) == 0) {
            continue;
        }
        std::uint64_t reserved = image.slot_reserved(i);
        std::uint64_t resident_bytes = image.slot_resident(i);
        if (resident_bytes > reserved) {
            resident_bytes = reserved;
        }
        resident = sat_add(resident, resident_bytes);
        pending = sat_add(pending, reserved - resident_bytes);
    }
    View view;
    view.headroom = headroom;
    view.stop_below = stop_below;
    view.pending = pending;
    view.resident = resident;
    view.oldest_bytes = image.oldest_bytes();
    view.oldest_seq = image.oldest_seq();
    view.self_seq = self_seq;
    if (hooks.free_bytes != nullptr) {
        if (const std::optional<std::uint64_t> free_bytes = hooks.free_bytes()) {
            view.free_known = true;
            view.free = *free_bytes;
        }
    }
    MemEval out;
    out.decision = decide(view, bytes, out.info);
    return out;
}

} // namespace

struct Budget::State {
    enum class Kind { Memory, File } kind = Kind::Memory;
    std::shared_ptr<MemTable> mem;
    std::shared_ptr<FileTable> file;
};

Reservation::Reservation(Reservation&& other) noexcept
    : budget_(std::move(other.budget_)), id_(other.id_), bytes_(other.bytes_) {
    other.id_ = 0;
    other.bytes_ = 0;
}

Reservation& Reservation::operator=(Reservation&& other) noexcept {
    if (this != &other) {
        release();
        budget_ = std::move(other.budget_);
        id_ = other.id_;
        bytes_ = other.bytes_;
        other.id_ = 0;
        other.bytes_ = 0;
    }
    return *this;
}

Reservation::~Reservation() {
    release();
}

void Reservation::release() noexcept {
    if (budget_) {
        budget_->release_id(id_);
        budget_.reset();
    }
    id_ = 0;
    bytes_ = 0;
}

CpuToken::CpuToken(CpuToken&& other) noexcept
    : budget_(std::move(other.budget_)), held_(other.held_) {
    other.held_ = false;
}

CpuToken& CpuToken::operator=(CpuToken&& other) noexcept {
    if (this != &other) {
        release();
        budget_ = std::move(other.budget_);
        held_ = other.held_;
        other.held_ = false;
    }
    return *this;
}

CpuToken::~CpuToken() {
    release();
}

void CpuToken::release() noexcept {
    if (budget_ && held_) {
        budget_->release_cpu();
    }
    held_ = false;
    budget_.reset();
}

std::shared_ptr<Budget> Budget::memory(BudgetHooks hooks, std::uint64_t headroom,
                                       const std::shared_ptr<Budget>& share) {
    auto budget = std::shared_ptr<Budget>(new Budget());
    if (share && share->state_ && share->state_->kind == State::Kind::Memory) {
        budget->state_ = share->state_;
    } else {
        budget->state_ = std::make_shared<State>();
        budget->state_->kind = State::Kind::Memory;
        budget->state_->mem = std::make_shared<MemTable>();
    }
    MemTable::Slot slot;
    slot.hooks = hooks;
    budget->state_->mem->slots.push_back(std::move(slot));
    budget->slot_ = static_cast<int>(budget->state_->mem->slots.size()) - 1;
    budget->headroom_ = headroom;
    budget->hooks_ = hooks;
    return budget;
}

std::shared_ptr<Budget> Budget::file(const std::filesystem::path& path, BudgetHooks hooks,
                                     std::uint64_t headroom) {
    auto budget = std::shared_ptr<Budget>(new Budget());
    budget->state_ = std::make_shared<State>();
    budget->state_->kind = State::Kind::File;
    budget->state_->file = std::make_shared<FileTable>(path);
    budget->headroom_ = headroom;
    budget->hooks_ = hooks;
    FileLock lock(budget->state_->file->os);
    Image image = budget->state_->file->locked_image();
    budget->state_->file->claim(image);
    budget->slot_ = static_cast<int>(budget->state_->file->slot);
    return budget;
}

Budget& Budget::process() {
    static std::shared_ptr<Budget> budget = [] {
        BudgetHooks hooks;
        hooks.free_bytes = &free_memory_bytes;
        hooks.process_private = &process_private_bytes;
        auto created = file(default_budget_path(), hooks, memory_headroom_bytes());
        created->logging_ = true;
        created->set_stop_below(memory_stop_bytes());
        return created;
    }();
    return *budget;
}

Reservation Budget::make_hold(std::uint64_t id, std::uint64_t bytes) {
    Reservation reservation;
    reservation.budget_ = shared_from_this();
    reservation.id_ = id;
    reservation.bytes_ = bytes;
    return reservation;
}

Reservation Budget::try_reserve(std::uint64_t bytes, Admit* info) {
    if (state_->kind == State::Kind::Memory) {
        std::lock_guard<std::mutex> lock(state_->mem->mu);
        const MemEval eval = evaluate_mem(*state_->mem, slot_, bytes, headroom_, stop_below_, 0);
        if (info != nullptr) {
            *info = eval.info;
        }
        if (eval.decision != Decision::Admit) {
            return Reservation{};
        }
        Hold hold;
        hold.id = state_->mem->next_id++;
        hold.bytes = bytes;
        state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds.push_back(hold);
        state_->mem->cv.notify_all();
        return make_hold(hold.id, bytes);
    }
    FileTable& table = *state_->file;
    FileLock lock(table.os);
    Image image = table.locked_image();
    table.claim(image);
    const MemEval eval = evaluate_file(table, image, hooks_, bytes, headroom_, stop_below_, 0);
    table.sync_slot(image);
    if (eval.decision == Decision::Admit) {
        Hold hold;
        hold.id = table.next_id++;
        hold.bytes = bytes;
        table.holds.push_back(hold);
        table.sync_slot(image);
        table.os.write_all(image.bytes);
        table.os.signal();
        lock.unlock();
        emit_log(logging_, table.log_path, admit_line("admit", eval.info));
        if (info != nullptr) {
            *info = eval.info;
        }
        return make_hold(hold.id, bytes);
    }
    table.os.write_all(image.bytes);
    if (info != nullptr) {
        *info = eval.info;
    }
    if (eval.decision == Decision::Refuse) {
        emit_log(logging_, table.log_path, admit_line("refuse", eval.info));
    }
    return Reservation{};
}

Reservation Budget::reserve(std::uint64_t bytes, Admit* info) {
    if (state_->kind == State::Kind::Memory) {
        std::unique_lock<std::mutex> lock(state_->mem->mu);
        std::uint64_t my_seq = 0;
        bool hooked = false;
        for (;;) {
            const MemEval eval =
                evaluate_mem(*state_->mem, slot_, bytes, headroom_, stop_below_, my_seq);
            if (eval.decision == Decision::Admit) {
                std::erase_if(state_->mem->waiters,
                              [&](const Waiter& waiter) { return waiter.seq == my_seq; });
                Hold hold;
                hold.id = state_->mem->next_id++;
                hold.bytes = bytes;
                state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds.push_back(hold);
                state_->mem->cv.notify_all();
                Admit out = eval.info;
                out.waited = hooked;
                if (info != nullptr) {
                    *info = out;
                }
                return make_hold(hold.id, bytes);
            }
            if (eval.decision == Decision::Refuse) {
                std::erase_if(state_->mem->waiters,
                              [&](const Waiter& waiter) { return waiter.seq == my_seq; });
                state_->mem->cv.notify_all();
                Admit out = eval.info;
                out.waited = hooked;
                if (info != nullptr) {
                    *info = out;
                }
                return Reservation{};
            }
            if (my_seq == 0) {
                my_seq = state_->mem->next_seq++;
                state_->mem->waiters.push_back(Waiter{my_seq, bytes, slot_});
            }
            if (!hooked) {
                lock.unlock();
                if (hooks_.on_wait != nullptr) {
                    hooks_.on_wait();
                }
                hooked = true;
                lock.lock();
                continue;
            }
            state_->mem->cv.wait(lock);
        }
    }

    FileTable& table = *state_->file;
    std::uint64_t my_seq = 0;
    bool hooked = false;
    for (;;) {
        FileLock lock(table.os);
        Image image = table.locked_image();
        table.claim(image);
        const MemEval eval =
            evaluate_file(table, image, hooks_, bytes, headroom_, stop_below_, my_seq);
        if (eval.decision == Decision::Admit) {
            std::erase_if(table.waiters,
                          [&](const Waiter& waiter) { return waiter.seq == my_seq; });
            table.clear_oldest_if_mine(image, my_seq);
            Hold hold;
            hold.id = table.next_id++;
            hold.bytes = bytes;
            table.holds.push_back(hold);
            table.sync_slot(image);
            table.os.write_all(image.bytes);
            table.os.signal();
            Admit out = eval.info;
            out.waited = hooked;
            emit_log(logging_, table.log_path, admit_line("admit", out));
            if (info != nullptr) {
                *info = out;
            }
            return make_hold(hold.id, bytes);
        }
        if (eval.decision == Decision::Refuse) {
            std::erase_if(table.waiters,
                          [&](const Waiter& waiter) { return waiter.seq == my_seq; });
            table.clear_oldest_if_mine(image, my_seq);
            table.sync_slot(image);
            table.os.write_all(image.bytes);
            table.os.signal();
            Admit out = eval.info;
            out.waited = hooked;
            emit_log(logging_, table.log_path, admit_line("refuse", out));
            if (info != nullptr) {
                *info = out;
            }
            return Reservation{};
        }
        if (my_seq == 0) {
            my_seq = image.next_seq();
            image.set_next_seq(my_seq + 1);
            table.waiters.push_back(Waiter{my_seq, bytes, slot_});
            if (image.oldest_seq() == 0 || image.oldest_seq() > my_seq) {
                table.publish_oldest_from(table.waiters.back(), image);
            }
        }
        table.sync_slot(image);
        table.os.write_all(image.bytes);
        table.os.reset_signal();
        const MemEval again =
            evaluate_file(table, image, hooks_, bytes, headroom_, stop_below_, my_seq);
        if (again.decision != Decision::Wait) {
            lock.unlock();
            continue;
        }
        if (!hooked) {
            lock.unlock();
            emit_log(logging_, table.log_path, admit_line("wait", eval.info));
            if (hooks_.on_wait != nullptr) {
                hooks_.on_wait();
            }
            hooked = true;
            continue;
        }
        lock.unlock();
        table.os.wait_signal();
    }
}

CpuToken Budget::acquire_cpu() {
    if (state_->kind != State::Kind::File) {
        return CpuToken{};
    }
    FileTable& table = *state_->file;
    for (;;) {
        FileLock lock(table.os);
        Image image = table.locked_image();
        table.claim(image);
        const std::uint32_t budget = image.cpu_budget();
        if (budget == 0 || image.cpu_in_use() < budget) {
            image.set_cpu_in_use(image.cpu_in_use() + 1);
            image.set_slot_cpu(table.slot, image.slot_cpu(table.slot) + 1);
            table.os.write_all(image.bytes);
            CpuToken token;
            token.budget_ = shared_from_this();
            token.held_ = true;
            return token;
        }
        table.os.reset_signal();
        if (image.cpu_in_use() < image.cpu_budget()) {
            continue;
        }
        lock.unlock();
        table.os.wait_signal();
    }
}

void Budget::release_id(std::uint64_t id) noexcept {
    if (id == 0 || !state_) {
        return;
    }
    try {
        if (state_->kind == State::Kind::Memory) {
            std::lock_guard<std::mutex> lock(state_->mem->mu);
            auto& holds = state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds;
            std::erase_if(holds, [&](const Hold& hold) { return hold.id == id; });
            state_->mem->cv.notify_all();
            return;
        }
        FileTable& table = *state_->file;
        std::uint64_t released = 0;
        {
            FileLock lock(table.os);
            Image image = table.locked_image();
            for (const Hold& hold : table.holds) {
                if (hold.id == id) {
                    released = hold.bytes;
                }
            }
            std::erase_if(table.holds, [&](const Hold& hold) { return hold.id == id; });
            table.sync_slot(image);
            table.os.write_all(image.bytes);
            table.os.signal();
        }
        if (logging_) {
            const std::optional<std::uint64_t> private_bytes =
                hooks_.process_private != nullptr ? hooks_.process_private() : std::nullopt;
            emit_log(true, table.log_path,
                     "[anno-test memory] end estimate=" + num(released) + " private=" +
                         (private_bytes ? num(*private_bytes) : std::string("unknown")));
        }
    } catch (...) {
    }
}

void Budget::release_cpu() noexcept {
    if (!state_ || state_->kind != State::Kind::File) {
        return;
    }
    try {
        FileTable& table = *state_->file;
        FileLock lock(table.os);
        Image image = table.locked_image();
        if (image.cpu_in_use() > 0) {
            image.set_cpu_in_use(image.cpu_in_use() - 1);
        }
        const std::uint32_t mine = image.slot_cpu(table.slot);
        if (mine > 0) {
            image.set_slot_cpu(table.slot, mine - 1);
        }
        table.os.write_all(image.bytes);
        table.os.signal();
    } catch (...) {
    }
}

std::uint64_t Budget::reserved() const {
    if (!state_) {
        return 0;
    }
    if (state_->kind == State::Kind::Memory) {
        std::lock_guard<std::mutex> lock(state_->mem->mu);
        return sum_reserved(state_->mem->slots.at(static_cast<std::size_t>(slot_)).holds);
    }
    FileTable& table = *state_->file;
    FileLock lock(table.os);
    return sum_reserved(table.holds);
}

void Budget::set_stop_below(std::uint64_t bytes) {
    stop_below_ = bytes;
}

void Budget::wake() {
    if (!state_) {
        return;
    }
    if (state_->kind == State::Kind::Memory) {
        state_->mem->cv.notify_all();
        return;
    }
    state_->file->os.signal();
}

Reservation reserve_cpu_render(std::uint64_t pixels) {
    if (pixels == 0) {
        return Reservation{};
    }
    const std::uint64_t bytes = cpu_render_additional_bytes(pixels);
    Admit info;
    Reservation hold = Budget::process().reserve(bytes, &info);
    if (!hold) {
        throw std::runtime_error("[anno-test memory] refused estimate=" + std::to_string(bytes) +
                                 " reason=" + info.reason +
                                 " available=" + std::to_string(info.available));
    }
    return hold;
}

} // namespace anno::test
