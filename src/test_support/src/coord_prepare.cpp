// Bounded, fail-closed preparation of the host test coordinator (T10c F6).
// One wall deadline covers the lock, the compiler, validation and cleanup of
// processes this call spawned. A nonempty file is not a usable artifact.
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <anno/test/budget.hpp>

#include "coord_identity.hpp"
#include "coord_wire.hpp"

#if defined(_WIN32)
#include <process.h>
#else
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

namespace anno::test {
namespace {

using clock = std::chrono::steady_clock;

std::string hex64(std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::string hex_bytes(const std::uint8_t* data, std::size_t count) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out(count * 2, '0');
    for (std::size_t i = 0; i < count; ++i) {
        out[i * 2] = kHex[data[i] >> 4];
        out[i * 2 + 1] = kHex[data[i] & 0x0fu];
    }
    return out;
}

std::uint32_t rotr(std::uint32_t value, unsigned shift) {
    return (value >> shift) | (value << (32u - shift));
}

class Sha256 {
  public:
    Sha256() { reset(); }

    void update(const std::uint8_t* data, std::size_t length) {
        bits_ += static_cast<std::uint64_t>(length) * 8u;
        while (length > 0) {
            const std::size_t take = std::min(length, static_cast<std::size_t>(64) - fill_);
            std::memcpy(block_ + fill_, data, take);
            fill_ += take;
            data += take;
            length -= take;
            if (fill_ == 64) {
                compress(block_);
                fill_ = 0;
            }
        }
    }

    std::string final() {
        const std::uint64_t bits = bits_;
        const std::uint8_t one = 0x80;
        update(&one, 1);
        const std::uint8_t zero = 0;
        while (fill_ != 56) {
            update(&zero, 1);
        }
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i) {
            len[i] = static_cast<std::uint8_t>((bits >> (56 - 8 * i)) & 0xffu);
        }
        update(len, 8);
        std::uint8_t out[32];
        for (int i = 0; i < 8; ++i) {
            out[i * 4] = static_cast<std::uint8_t>(state_[i] >> 24);
            out[i * 4 + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
            out[i * 4 + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
            out[i * 4 + 3] = static_cast<std::uint8_t>(state_[i]);
        }
        return hex_bytes(out, 32);
    }

  private:
    void reset() {
        state_[0] = 0x6a09e667u;
        state_[1] = 0xbb67ae85u;
        state_[2] = 0x3c6ef372u;
        state_[3] = 0xa54ff53au;
        state_[4] = 0x510e527fu;
        state_[5] = 0x9b05688cu;
        state_[6] = 0x1f83d9abu;
        state_[7] = 0x5be0cd19u;
        bits_ = 0;
        fill_ = 0;
    }

    void compress(const std::uint8_t block[64]) {
        static constexpr std::uint32_t k[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
            0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
            0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
            0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
            0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<std::uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ ((~e) & g);
            const std::uint32_t t1 = h + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::uint32_t state_[8]{};
    std::uint8_t block_[64]{};
    std::uint64_t bits_ = 0;
    std::size_t fill_ = 0;
};

std::string hash_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    Sha256 sha;
    char buffer[8192];
    while (in) {
        in.read(buffer, static_cast<std::streamsize>(sizeof(buffer)));
        const auto got = in.gcount();
        if (got > 0) {
            sha.update(reinterpret_cast<const std::uint8_t*>(buffer),
                       static_cast<std::size_t>(got));
        }
    }
    return sha.final();
}

bool file_contains(const std::filesystem::path& path, std::string_view needle) {
    std::ifstream in(path, std::ios::binary);
    if (!in || needle.empty()) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0 || static_cast<unsigned long long>(end) > 64ull * 1024ull * 1024ull) {
        return false;
    }
    std::string data(static_cast<std::size_t>(end), '\0');
    in.seekg(0);
    in.read(data.data(), end);
    return data.find(needle) != std::string::npos;
}

bool elf_executable(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    unsigned char header[64] = {};
    in.read(reinterpret_cast<char*>(header), 64);
    if (!in || header[0] != 0x7f || header[1] != 'E' || header[2] != 'L' || header[3] != 'F') {
        return false;
    }
    if (header[4] != 2 || header[5] != 1 || header[6] != 1) {
        return false;
    }
    const unsigned type = header[16] | (static_cast<unsigned>(header[17]) << 8);
    const unsigned machine = header[18] | (static_cast<unsigned>(header[19]) << 8);
    if ((type != 2 && type != 3) || machine != 62) {
        return false;
    }
    std::uint64_t phoff = 0;
    for (int i = 0; i < 8; ++i) {
        phoff |= static_cast<std::uint64_t>(header[32 + i]) << (8 * i);
    }
    const unsigned phentsize = header[54] | (static_cast<unsigned>(header[55]) << 8);
    const unsigned phnum = header[56] | (static_cast<unsigned>(header[57]) << 8);
    if (phnum == 0 || phentsize < 56) {
        return false;
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || phoff > size || static_cast<std::uint64_t>(phnum) * phentsize > size - phoff) {
        return false;
    }
    return true;
}

bool instrumented_file(const std::filesystem::path& path) {
    return file_contains(path, "__asan_init") || file_contains(path, "libasan");
}

std::filesystem::path source_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
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

const char* env_text(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return nullptr;
    }
    return value;
}

bool env_is(const char* name, const char* want) {
    const char* value = env_text(name);
    return value != nullptr && std::string(value) == want;
}

void append_tail(std::string& tail, const char* data, std::size_t count) {
    constexpr std::size_t kCap = 8192;
    tail.append(data, count);
    if (tail.size() > kCap) {
        tail.erase(0, tail.size() - kCap);
    }
}

std::timed_mutex& prepare_mutex() {
    static std::timed_mutex mutex;
    return mutex;
}

struct Captured {
    bool spawn_failed = false;
    bool timed_out = false;
    bool exited = false;
    int exit_code = -1;
    std::string output;
};

#if defined(_WIN32)
void append_windows_arg(std::wstring& command, const std::wstring& arg) {
    if (!command.empty()) {
        command.push_back(L' ');
    }
    const bool quote = arg.empty() || arg.find_first_of(L" \t\"") != std::wstring::npos;
    if (!quote) {
        command += arg;
        return;
    }
    command.push_back(L'"');
    std::size_t backs = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backs;
            continue;
        }
        if (ch == L'"') {
            command.append(backs * 2 + 1, L'\\');
            command.push_back(L'"');
            backs = 0;
            continue;
        }
        if (backs != 0) {
            command.append(backs, L'\\');
            backs = 0;
        }
        command.push_back(ch);
    }
    if (backs != 0) {
        command.append(backs * 2, L'\\');
    }
    command.push_back(L'"');
}

std::wstring wide_utf8(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size =
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring(text.begin(), text.end());
    }
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::filesystem::path wsl_executable() {
    if (const char* root = std::getenv("SystemRoot")) {
        if (root[0] != '\0') {
            return std::filesystem::path(root) / "System32" / "wsl.exe";
        }
    }
    return std::filesystem::path("C:\\Windows\\System32\\wsl.exe");
}

void kill_linux_group(const std::filesystem::path& wsl, const std::string& pid) {
    if (pid.empty()) {
        return;
    }
    std::wstring command;
    append_windows_arg(command, wsl.wstring());
    append_windows_arg(command, L"-d");
    append_windows_arg(command, L"Ubuntu");
    append_windows_arg(command, L"--");
    append_windows_arg(command, L"kill");
    append_windows_arg(command, L"-9");
    append_windows_arg(command, wide_utf8("-" + pid));
    append_windows_arg(command, wide_utf8(pid));
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (CreateProcessW(wsl.c_str(), mutable_command.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) == 0) {
        return;
    }
    WaitForSingleObject(process.hProcess, 200);
    TerminateProcess(process.hProcess, 1);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}

Captured run_bounded(const std::vector<std::string>& argv, const std::filesystem::path& pidfile,
                     clock::time_point deadline) {
    Captured captured;
    const auto wsl = wsl_executable();
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (CreatePipe(&read_end, &write_end, &inherit, 0) == 0) {
        captured.spawn_failed = true;
        return captured;
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    std::wstring command;
    append_windows_arg(command, wsl.wstring());
    append_windows_arg(command, L"-d");
    append_windows_arg(command, L"Ubuntu");
    append_windows_arg(command, L"--");
    for (const std::string& arg : argv) {
        append_windows_arg(command, wide_utf8(arg));
    }
    std::vector<wchar_t> mutable_command(command.begin(), command.end());
    mutable_command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nul == INVALID_HANDLE_VALUE) {
        CloseHandle(read_end);
        CloseHandle(write_end);
        captured.spawn_failed = true;
        return captured;
    }
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nul;
    startup.hStdOutput = write_end;
    startup.hStdError = write_end;
    PROCESS_INFORMATION process{};
    const BOOL created =
        CreateProcessW(wsl.c_str(), mutable_command.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_NEW_PROCESS_GROUP, nullptr,
                       nullptr, &startup, &process);
    CloseHandle(write_end);
    CloseHandle(nul);
    if (created == 0) {
        CloseHandle(read_end);
        captured.spawn_failed = true;
        return captured;
    }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(read_end);
        captured.spawn_failed = true;
        return captured;
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info)) == 0 ||
        AssignProcessToJobObject(job, process.hProcess) == 0) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(job);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(read_end);
        captured.spawn_failed = true;
        return captured;
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    bool eof = false;
    while (clock::now() < deadline) {
        if (!eof) {
            DWORD available = 0;
            if (PeekNamedPipe(read_end, nullptr, 0, nullptr, &available, nullptr) == 0) {
                eof = true;
            } else if (available > 0) {
                char buffer[512];
                const DWORD want =
                    available < sizeof(buffer) ? available : static_cast<DWORD>(sizeof(buffer));
                DWORD got = 0;
                if (ReadFile(read_end, buffer, want, &got, nullptr) != 0 && got > 0) {
                    append_tail(captured.output, buffer, got);
                }
                continue;
            }
        }
        DWORD code = 0;
        if (GetExitCodeProcess(process.hProcess, &code) != 0 && code != STILL_ACTIVE) {
            captured.exited = true;
            captured.exit_code = static_cast<int>(code);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!captured.exited) {
        captured.timed_out = true;
        std::string pid;
        {
            std::ifstream in(pidfile);
            std::getline(in, pid);
        }
        kill_linux_group(wsl, pid);
        TerminateJobObject(job, 1);
        const auto left = clock::now() < deadline
                              ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                                     clock::now())
                                    .count()
                              : 0;
        const DWORD reap = static_cast<DWORD>(left > 0 ? left : 0);
        WaitForSingleObject(process.hProcess, reap > 200 ? 200 : reap);
    }
    CloseHandle(process.hProcess);
    CloseHandle(read_end);
    if (job != nullptr) {
        CloseHandle(job);
    }
    return captured;
}
#else
void kill_group(pid_t pid) {
    if (pid > 0) {
        ::kill(-pid, SIGKILL);
        ::kill(pid, SIGKILL);
    }
}

Captured run_bounded(const std::vector<std::string>& argv, const std::filesystem::path& pidfile,
                     clock::time_point deadline) {
    Captured captured;
    (void)pidfile;
    int pipe_fd[2];
    if (::pipe(pipe_fd) != 0) {
        captured.spawn_failed = true;
        return captured;
    }
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attr;
    if (posix_spawn_file_actions_init(&actions) != 0 || posix_spawnattr_init(&attr) != 0) {
        ::close(pipe_fd[0]);
        ::close(pipe_fd[1]);
        captured.spawn_failed = true;
        return captured;
    }
    if (posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP) != 0 ||
        posix_spawnattr_setpgroup(&attr, 0) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], 1) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], 2) != 0 ||
        posix_spawn_file_actions_addclose(&actions, pipe_fd[0]) != 0 ||
        !wire::close_descriptors_from_3(&actions)) {
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attr);
        ::close(pipe_fd[0]);
        ::close(pipe_fd[1]);
        captured.spawn_failed = true;
        return captured;
    }
    std::vector<std::string> owned = argv;
    std::vector<char*> raw;
    raw.reserve(owned.size() + 1);
    for (std::string& arg : owned) {
        raw.push_back(arg.data());
    }
    raw.push_back(nullptr);
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, owned.front().c_str(), &actions, &attr, raw.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    ::close(pipe_fd[1]);
    if (rc != 0) {
        ::close(pipe_fd[0]);
        captured.spawn_failed = true;
        return captured;
    }
    const int flags = ::fcntl(pipe_fd[0], F_GETFL, 0);
    if (flags < 0 || ::fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK) != 0) {
        kill_group(pid);
        ::waitpid(pid, nullptr, 0);
        ::close(pipe_fd[0]);
        captured.spawn_failed = true;
        return captured;
    }
    bool eof = false;
    while (clock::now() < deadline) {
        if (!eof) {
            pollfd polled{};
            polled.fd = pipe_fd[0];
            polled.events = POLLIN;
            const int ready = ::poll(&polled, 1, 50);
            if (ready > 0 && (polled.revents & (POLLIN | POLLHUP)) != 0) {
                char buffer[512];
                const ssize_t got = ::read(pipe_fd[0], buffer, sizeof(buffer));
                if (got > 0) {
                    append_tail(captured.output, buffer, static_cast<std::size_t>(got));
                    continue;
                }
                if (got == 0) {
                    eof = true;
                }
            }
        }
        int status = 0;
        const pid_t got = ::waitpid(pid, &status, WNOHANG);
        if (got == pid) {
            captured.exited = true;
            if (WIFEXITED(status)) {
                captured.exit_code = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                captured.exit_code = 128 + WTERMSIG(status);
            }
            break;
        }
        if (eof) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    if (!captured.exited) {
        captured.timed_out = true;
        kill_group(pid);
        const auto reap_until = clock::now() < deadline ? deadline : clock::now();
        while (clock::now() < reap_until) {
            int status = 0;
            const pid_t got = ::waitpid(pid, &status, WNOHANG);
            if (got == pid || (got < 0 && errno != EINTR)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        ::waitpid(pid, nullptr, 0);
    }
    ::close(pipe_fd[0]);
    return captured;
}
#endif

struct Manifest {
    bool ok = false;
    std::string build;
    std::string compat;
    std::string config;
    std::string compiler;
    std::string flags;
    std::string content;
};

Manifest read_manifest(const std::filesystem::path& path) {
    Manifest manifest;
    std::ifstream in(path);
    if (!in) {
        return manifest;
    }
    std::string line;
    int seen = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const auto cut = line.find('=');
        if (cut == std::string::npos) {
            return Manifest{};
        }
        const std::string key = line.substr(0, cut);
        const std::string value = line.substr(cut + 1);
        if (key == "format") {
            if (value != "1") {
                return Manifest{};
            }
        } else if (key == "build") {
            manifest.build = value;
        } else if (key == "compat") {
            manifest.compat = value;
        } else if (key == "config") {
            manifest.config = value;
        } else if (key == "compiler") {
            manifest.compiler = value;
        } else if (key == "flags") {
            manifest.flags = value;
        } else if (key == "content") {
            manifest.content = value;
        } else {
            return Manifest{};
        }
        ++seen;
    }
    manifest.ok = seen >= 7 && !manifest.build.empty() && !manifest.compat.empty() &&
                  !manifest.config.empty() && !manifest.compiler.empty() &&
                  !manifest.flags.empty() && manifest.content.size() == 64;
    return manifest;
}

std::string manifest_text(const Manifest& manifest) {
    std::ostringstream out;
    out << "format=1\nbuild=" << manifest.build << "\ncompat=" << manifest.compat
        << "\nconfig=" << manifest.config << "\ncompiler=" << manifest.compiler
        << "\nflags=" << manifest.flags << "\ncontent=" << manifest.content << '\n';
    return out.str();
}

bool expected_identity(const Manifest& manifest) {
    return manifest.ok && manifest.build == hex64(coordinator_build()) &&
           manifest.compat == std::to_string(wire::kCompatVersion) &&
           manifest.config == coordinator_config_name() &&
           manifest.flags == coordinator_flags_text();
}

bool binary_bytes_ok(const std::filesystem::path& binary) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(binary, error) ||
        std::filesystem::file_size(binary, error) == 0 || !elf_executable(binary) ||
        !file_contains(binary, coordinator_marker())) {
        return false;
    }
    if (std::string(coordinator_config_name()) == "asan+ubsan" && !instrumented_file(binary)) {
        return false;
    }
    return true;
}

bool identity_probe(const std::filesystem::path& binary, clock::time_point deadline,
                    CoordinatorPrepare& result) {
    if (clock::now() >= deadline) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return false;
    }
    std::vector<std::string> args;
#if defined(_WIN32)
    args.push_back(to_wsl_path(binary));
#else
    args.push_back(binary.string());
#endif
    args.emplace_back("--identity");
    const auto pidfile = std::filesystem::path(binary.string() + ".probe.pid");
    const Captured captured = run_bounded(args, pidfile, deadline);
    std::error_code error;
    std::filesystem::remove(pidfile, error);
    if (captured.spawn_failed) {
        result.diagnostic = "unusable artifact";
        return false;
    }
    if (captured.timed_out || !captured.exited) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return false;
    }
    if (clock::now() >= deadline) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return false;
    }
    const std::string expect = "COORD-IDENTITY build=" + hex64(coordinator_build()) + " config=" +
                               coordinator_config_name() + " marker=" + coordinator_marker();
    if (captured.exit_code != 0 || captured.output.find(expect) == std::string::npos) {
        result.diagnostic = "unusable artifact";
        return false;
    }
    if (std::string(coordinator_config_name()) == "asan+ubsan" &&
        captured.output.find("instrumented=libasan") == std::string::npos) {
        result.diagnostic = "sanitized coordinator artifact is not instrumented";
        return false;
    }
    return true;
}

bool artifact_usable(const std::filesystem::path& binary, CoordinatorPrepare& result,
                     clock::time_point deadline) {
    if (clock::now() >= deadline) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return false;
    }
    const Manifest manifest = read_manifest(std::filesystem::path(binary.string() + ".identity"));
    if (!expected_identity(manifest) || !binary_bytes_ok(binary)) {
        return false;
    }
    const std::string content = hash_file(binary);
    if (clock::now() >= deadline) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return false;
    }
    if (content.empty() || content != manifest.content) {
        return false;
    }
    if (!identity_probe(binary, deadline, result)) {
        return false;
    }
    result.content = content;
    result.config = manifest.config;
    result.compiler = manifest.compiler;
    result.flags = manifest.flags;
    return true;
}

bool write_manifest(const std::filesystem::path& binary, const std::string& compiler,
                    CoordinatorPrepare& result) {
    Manifest manifest;
    manifest.build = hex64(coordinator_build());
    manifest.compat = std::to_string(wire::kCompatVersion);
    manifest.config = coordinator_config_name();
    manifest.compiler = compiler;
    manifest.flags = coordinator_flags_text();
    manifest.content = hash_file(binary);
    manifest.ok = !manifest.content.empty();
    if (!manifest.ok) {
        return false;
    }
    const auto path = std::filesystem::path(binary.string() + ".identity");
    const auto staging = std::filesystem::path(path.string() + ".staging");
    {
        std::ofstream out(staging, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << manifest_text(manifest);
    }
    std::error_code error;
    std::filesystem::rename(staging, path, error);
    if (error) {
        std::filesystem::remove(staging, error);
        return false;
    }
    result.content = manifest.content;
    result.config = manifest.config;
    result.compiler = manifest.compiler;
    result.flags = manifest.flags;
    return true;
}

std::filesystem::path identity_lock(const std::filesystem::path& binary) {
    return std::filesystem::path(binary.string() + ".lock");
}

class FileLock {
  public:
    bool open_lock(const std::filesystem::path& path) {
#if defined(_WIN32)
        handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            handle_ = nullptr;
            return false;
        }
        SetHandleInformation(handle_, HANDLE_FLAG_INHERIT, 0);
        return true;
#else
        fd_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
        return fd_ >= 0;
#endif
    }

    bool try_acquire() {
#if defined(_WIN32)
        OVERLAPPED overlapped{};
        return LockFileEx(handle_, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0,
                          &overlapped) != 0;
#else
        return ::flock(fd_, LOCK_EX | LOCK_NB) == 0;
#endif
    }

    void release() {
#if defined(_WIN32)
        if (handle_ != nullptr && held_) {
            OVERLAPPED overlapped{};
            UnlockFileEx(handle_, 0, 1, 0, &overlapped);
        }
        held_ = false;
        if (handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
#else
        if (fd_ >= 0 && held_) {
            ::flock(fd_, LOCK_UN);
        }
        held_ = false;
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
#endif
    }

    ~FileLock() { release(); }

    void mark_held() { held_ = true; }

  private:
    bool held_ = false;
#if defined(_WIN32)
    HANDLE handle_ = nullptr;
#else
    int fd_ = -1;
#endif
};

std::string supervise_script() {
    return "#!/bin/sh\n"
           "pidfile=$1\n"
           "shift\n"
           "echo $$ > \"$pidfile\"\n"
           "exec \"$@\"\n";
}

std::vector<std::string> split_flags(const char* text) {
    std::vector<std::string> flags;
    std::istringstream in(text);
    std::string flag;
    while (in >> flag) {
        flags.push_back(flag);
    }
    return flags;
}

bool publish_staged(const std::filesystem::path& staging, const std::filesystem::path& final_path,
                    const std::string& compiler, CoordinatorPrepare& result) {
    std::error_code error;
#if !defined(_WIN32)
    std::filesystem::permissions(staging,
                                 std::filesystem::perms::owner_exec |
                                     std::filesystem::perms::group_exec |
                                     std::filesystem::perms::others_exec,
                                 std::filesystem::perm_options::add, error);
#endif
    const auto staged_identity = std::filesystem::path(staging.string() + ".identity");
    if (!write_manifest(staging, compiler, result)) {
        result.diagnostic = "identity manifest failed";
        return false;
    }
    const auto hold = std::filesystem::path(final_path.string() + ".hold." +
                                            hex64(static_cast<std::uint64_t>(
#if defined(_WIN32)
                                                GetCurrentProcessId()
#else
                                                static_cast<std::uint64_t>(::getpid())
#endif
                                                    )));
    const auto hold_identity = std::filesystem::path(hold.string() + ".identity");
    const auto final_identity = std::filesystem::path(final_path.string() + ".identity");
    auto restore_previous = [&] {
        std::error_code restore;
        if (std::filesystem::exists(hold, restore)) {
            std::filesystem::rename(hold, final_path, restore);
        }
        if (std::filesystem::exists(hold_identity, restore)) {
            std::filesystem::rename(hold_identity, final_identity, restore);
        }
        std::filesystem::remove(staged_identity, restore);
    };
    bool displaced = false;
    if (std::filesystem::exists(final_path, error)) {
        std::filesystem::rename(final_path, hold, error);
        if (error) {
            std::filesystem::remove(staged_identity, error);
            result.diagnostic = "rename of completed artifact failed";
            return false;
        }
        displaced = true;
    }
    if (std::filesystem::exists(final_identity, error)) {
        std::filesystem::rename(final_identity, hold_identity, error);
        if (error) {
            restore_previous();
            result.diagnostic = "rename of completed artifact failed";
            return false;
        }
    }
    if (env_is("ANNO_TEST_COORD_PUBLISH_FAIL", "rename")) {
        restore_previous();
        result.diagnostic = "rename of completed artifact failed";
        return false;
    }
    std::filesystem::rename(staging, final_path, error);
    if (error) {
        restore_previous();
        std::filesystem::remove(staging, error);
        result.diagnostic = "rename of completed artifact failed";
        return false;
    }
    std::filesystem::rename(staged_identity, final_identity, error);
    if (error) {
        std::filesystem::remove(final_path, error);
        restore_previous();
        result.diagnostic = "identity manifest failed";
        return false;
    }
    if (displaced) {
        std::filesystem::remove(hold, error);
        std::filesystem::remove(hold_identity, error);
    }
    return true;
}

} // namespace

std::filesystem::path host_coordinator_binary_path() {
    if (const char* root = env_text("ANNO_TEST_COORD_PREPARE_ROOT")) {
        return std::filesystem::path(root) / ("anno_budget_coord-" + hex64(coordinator_build()));
    }
#if defined(_WIN32)
    return source_root() / "cache" / "coord" / ("anno_budget_coord-" + hex64(coordinator_build()));
#else
    return module_directory() / "anno_budget_coord";
#endif
}

ListeningIdentity coordinator_listening_identity(std::string_view line, std::uint16_t port,
                                                 std::uint64_t build, std::uint64_t compat,
                                                 std::string_view config) {
    ListeningIdentity identity;
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : line) {
        if (ch == ' ' || ch == '\t') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else if (ch != '\r') {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    auto fail = [&](const char* why) {
        identity.diagnostic = std::string("listening identity: ") + why;
        return identity;
    };
    if (tokens.size() != 7 || tokens[0] != "LISTENING") {
        return fail("malformed line");
    }
    char* end = nullptr;
    const unsigned long parsed_port = std::strtoul(tokens[1].c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || parsed_port > 65535ul) {
        return fail("malformed port");
    }
    identity.port = static_cast<std::uint16_t>(parsed_port);
    auto field = [](const std::string& token, const char* key, std::string& value) {
        const std::string prefix = std::string(key) + "=";
        if (token.rfind(prefix, 0) != 0 || token.size() == prefix.size()) {
            return false;
        }
        value = token.substr(prefix.size());
        return true;
    };
    std::string build_text;
    std::string compat_text;
    std::string incarnation_text;
    std::string orphans_text;
    if (!field(tokens[2], "build", build_text) || build_text.size() != 16 ||
        !field(tokens[3], "compat", compat_text) ||
        !field(tokens[4], "incarnation", incarnation_text) || incarnation_text.size() != 16 ||
        !field(tokens[5], "orphans", orphans_text) ||
        !field(tokens[6], "config", identity.config)) {
        return fail("malformed field");
    }
    identity.build = std::strtoull(build_text.c_str(), &end, 16);
    if (end == nullptr || *end != '\0') {
        return fail("malformed build");
    }
    identity.compat = std::strtoull(compat_text.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') {
        return fail("malformed compat");
    }
    identity.incarnation = std::strtoull(incarnation_text.c_str(), &end, 16);
    if (end == nullptr || *end != '\0' || identity.incarnation == 0) {
        return fail("malformed incarnation");
    }
    if (orphans_text.empty() || orphans_text[0] < '0' || orphans_text[0] > '9') {
        return fail("malformed orphans");
    }
    errno = 0;
    const unsigned long long orphans = std::strtoull(orphans_text.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || errno == ERANGE) {
        return fail("malformed orphans");
    }
    identity.orphans = orphans;
    if (identity.port != port) {
        return fail("port mismatch");
    }
    if (identity.build != build) {
        return fail("build mismatch");
    }
    if (identity.compat != compat) {
        return fail("compat mismatch");
    }
    if (identity.config != config) {
        return fail("config mismatch");
    }
    identity.matches = true;
    return identity;
}

std::filesystem::path host_coordinator_binary() {
    return host_coordinator_binary_path();
}

CoordinatorPrepare prepare_host_coordinator() {
    CoordinatorPrepare result;
    result.binary = host_coordinator_binary_path();
    const char* deadline_text = env_text("ANNO_TEST_COORD_PREPARE_DEADLINE_MS");
    unsigned long deadline_ms = 120000;
    if (deadline_text != nullptr) {
        char* end = nullptr;
        deadline_ms = std::strtoul(deadline_text, &end, 10);
        if (end == deadline_text || *end != '\0' || deadline_ms == 0 || deadline_ms > 600000ul) {
            result.diagnostic = "invalid prepare deadline";
            return result;
        }
    }
    const auto deadline = clock::now() + std::chrono::milliseconds(deadline_ms);
    if (env_is("ANNO_TEST_COORD_LOCK_FAIL", "create")) {
        result.diagnostic = "lock creation failed";
        return result;
    }
    if (env_is("ANNO_TEST_COORD_LOCK_FAIL", "acquire")) {
        result.diagnostic = "lock acquire failed";
        return result;
    }
    std::error_code error;
    std::filesystem::create_directories(result.binary.parent_path(), error);
    if (error) {
        result.diagnostic = "lock creation failed";
        return result;
    }
    std::unique_lock<std::timed_mutex> in_process(prepare_mutex(), std::defer_lock);
    if (!in_process.try_lock_until(deadline)) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return result;
    }
    FileLock file_lock;
    if (!file_lock.open_lock(identity_lock(result.binary))) {
        result.diagnostic = "lock creation failed";
        return result;
    }
    bool held = false;
    while (clock::now() < deadline) {
        if (file_lock.try_acquire()) {
            held = true;
            file_lock.mark_held();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    if (!held) {
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return result;
    }
    if (env_is("ANNO_TEST_COORD_PREPARE_FAIL", "1") ||
        (env_text("ANNO_TEST_COORD_PREPARE_FAIL") != nullptr &&
         !env_is("ANNO_TEST_COORD_PREPARE_FAIL", "0"))) {
        result.diagnostic = "ANNO_TEST_COORD_PREPARE_FAIL";
        return result;
    }
    const bool rebuild = env_is("ANNO_TEST_COORD_PREPARE_REBUILD", "1");
    if (!rebuild) {
        if (artifact_usable(result.binary, result, deadline)) {
            result.ok = true;
            result.reused = true;
            return result;
        }
        if (result.timed_out) {
            return result;
        }
    }
    const bool private_root = env_text("ANNO_TEST_COORD_PREPARE_ROOT") != nullptr;
#if !defined(_WIN32)
    // A CMake canonical binary has no manifest until the first prepare. Adopt it
    // when its bytes already match this build and configuration, and write that
    // manifest. A byte mismatch, or a manifest that names different bytes, stays
    // in place and does not get replaced by a compiler invocation.
    if (!private_root && std::filesystem::exists(result.binary, error)) {
        const Manifest existing =
            read_manifest(std::filesystem::path(result.binary.string() + ".identity"));
        if (binary_bytes_ok(result.binary) && !existing.ok) {
            if (!identity_probe(result.binary, deadline, result)) {
                if (!result.timed_out) {
                    result.diagnostic = "canonical coordinator artifact failed identity validation";
                }
                return result;
            }
            if (!write_manifest(result.binary, "cmake", result)) {
                result.diagnostic = "identity manifest failed";
                return result;
            }
            if (clock::now() >= deadline) {
                result.ok = false;
                result.timed_out = true;
                result.diagnostic = "prepare wait deadline";
                return result;
            }
            result.ok = true;
            result.reused = true;
            return result;
        }
        if (!artifact_usable(result.binary, result, deadline)) {
            if (result.timed_out) {
                return result;
            }
            const bool asan = std::string(coordinator_config_name()) == "asan+ubsan";
            const bool marker_ok = file_contains(result.binary, coordinator_marker());
            result.diagnostic = asan && marker_ok && !instrumented_file(result.binary)
                                    ? "sanitized coordinator artifact is not instrumented"
                                    : "canonical coordinator artifact failed identity validation";
            return result;
        }
    }
#else
    (void)private_root;
#endif
    const auto stamp = hex64(static_cast<std::uint64_t>(
#if defined(_WIN32)
                           GetCurrentProcessId()
#else
                           static_cast<unsigned long long>(::getpid())
#endif
                               )) +
                       "-" +
                       hex64(static_cast<std::uint64_t>(clock::now().time_since_epoch().count()));
    const auto staging =
        result.binary.parent_path() / (result.binary.filename().string() + ".building." + stamp);
    const auto pidfile = std::filesystem::path(staging.string() + ".pid");
    const auto script = result.binary.parent_path() / ("supervise-" + stamp + ".sh");
    {
        std::ofstream out(script, std::ios::binary);
        out << supervise_script();
    }
    std::vector<std::string> args;
    const char* override_compiler = env_text("ANNO_TEST_COORD_COMPILER");
    const std::string compiler_name = override_compiler != nullptr ? "override" : "g++";
    if (override_compiler != nullptr) {
        args.emplace_back("sh");
#if defined(_WIN32)
        args.push_back(to_wsl_path(override_compiler));
#else
        args.emplace_back(override_compiler);
#endif
    } else {
        args.emplace_back("g++");
    }
    for (const std::string& flag : split_flags(coordinator_flags_text())) {
        args.push_back(flag);
    }
    const auto ident_header = result.binary.parent_path() / ("coord-id-" + stamp + ".hpp");
    {
        std::ofstream ident(ident_header, std::ios::binary | std::ios::trunc);
        ident << "#define ANNO_COORD_BUILD 0x" << hex64(coordinator_build()) << "ULL\n"
              << "#define ANNO_COORD_CONFIG_LITERAL \"" << coordinator_config_name() << "\"\n"
              << "#define ANNO_COORD_FLAGS_LITERAL \"" << coordinator_flags_text() << "\"\n"
              << "#define ANNO_COORD_MARKER_LITERAL \"" << coordinator_marker() << "\"\n";
    }
    args.emplace_back("-include");
    const auto include = source_root() / "src" / "test_support" / "include";
    const auto sources = source_root() / "src" / "test_support" / "src";
#if defined(_WIN32)
    args.push_back(to_wsl_path(ident_header));
    args.push_back("-I" + to_wsl_path(include));
    args.push_back(to_wsl_path(sources / "budget_coord.cpp"));
    args.push_back(to_wsl_path(sources / "coord_server.cpp"));
    args.push_back(to_wsl_path(sources / "memory.cpp"));
    args.emplace_back("-o");
    args.push_back(to_wsl_path(staging));
    std::vector<std::string> command = {"sh", to_wsl_path(script), to_wsl_path(pidfile)};
#else
    args.push_back(ident_header.string());
    args.push_back("-I" + include.string());
    args.push_back((sources / "budget_coord.cpp").string());
    args.push_back((sources / "coord_server.cpp").string());
    args.push_back((sources / "memory.cpp").string());
    args.emplace_back("-o");
    args.push_back(staging.string());
    std::vector<std::string> command = {"/bin/sh", script.string(), pidfile.string()};
#endif
    command.insert(command.end(), args.begin(), args.end());
    const Captured captured = run_bounded(command, pidfile, deadline);
    std::filesystem::remove(script, error);
    std::filesystem::remove(pidfile, error);
    std::filesystem::remove(ident_header, error);
    auto cleanup_staging = [&] {
        std::filesystem::remove(staging, error);
        std::filesystem::remove(std::filesystem::path(staging.string() + ".argv"), error);
    };
    if (captured.spawn_failed) {
        cleanup_staging();
        result.diagnostic = "spawn failed";
        return result;
    }
    if (captured.timed_out || !captured.exited) {
        cleanup_staging();
        result.timed_out = true;
        result.compiler_exit = -1;
        result.diagnostic = "timeout";
        if (!captured.output.empty()) {
            result.diagnostic.push_back('\n');
            result.diagnostic += captured.output;
        }
        return result;
    }
    result.compiler_exit = captured.exit_code;
    if (captured.exit_code != 0) {
        cleanup_staging();
        result.diagnostic = "compiler exit " + std::to_string(captured.exit_code);
        if (!captured.output.empty()) {
            result.diagnostic.push_back('\n');
            result.diagnostic += captured.output;
        }
        return result;
    }
    if (clock::now() >= deadline) {
        cleanup_staging();
        result.timed_out = true;
        result.diagnostic = "prepare wait deadline";
        return result;
    }
    if (!binary_bytes_ok(staging) || !identity_probe(staging, deadline, result)) {
        cleanup_staging();
        if (!result.timed_out) {
            result.diagnostic = "unusable artifact";
        }
        if (!captured.output.empty()) {
            result.diagnostic.push_back('\n');
            result.diagnostic += captured.output;
        }
        return result;
    }
    if (!publish_staged(staging, result.binary, compiler_name, result)) {
        cleanup_staging();
        return result;
    }
    std::filesystem::remove(std::filesystem::path(staging.string() + ".argv"), error);
    result.ok = true;
    return result;
}

} // namespace anno::test
