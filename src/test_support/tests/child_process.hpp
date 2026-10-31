// Child processes for the budget regressions: start, read a line with a
// deadline, write a line, suspend, kill. A test only ever stops the processes
// it started itself (no coordinator or task process of another run).
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(__APPLE__)
#include <libproc.h>
#endif
#include <unistd.h>
extern char** environ;
#endif

namespace anno::test::child {

inline void set_env(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    if (value[0] == '\0') {
        ::unsetenv(name);
    } else {
        ::setenv(name, value, 1);
    }
#endif
}

/// An executable beside the running test binary (".exe" added on Windows).
inline std::filesystem::path sibling(const char* name) {
#if defined(_WIN32)
    wchar_t buffer[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, 32768);
    if (n == 0 || n >= 32768) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path() / (std::string(name) + ".exe");
#elif defined(__APPLE__)
    char buffer[PROC_PIDPATHINFO_MAXSIZE];
    if (::proc_pidpath(static_cast<int>(::getpid()), buffer, sizeof(buffer)) <= 0) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path() / name;
#else
    char buffer[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        return {};
    }
    buffer[n] = '\0';
    return std::filesystem::path(buffer).parent_path() / name;
#endif
}

class Child {
  public:
    Child() = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    Child(Child&& other) noexcept { take(other); }
    Child& operator=(Child&& other) noexcept {
        if (this != &other) {
            stop();
            take(other);
        }
        return *this;
    }
    ~Child() { stop(); }

    static Child start(const std::filesystem::path& exe, const std::vector<std::string>& args) {
        Child child;
#if defined(_WIN32)
        SECURITY_ATTRIBUTES inherit{};
        inherit.nLength = sizeof(inherit);
        inherit.bInheritHandle = TRUE;
        HANDLE out_read = nullptr;
        HANDLE out_write = nullptr;
        HANDLE in_read = nullptr;
        HANDLE in_write = nullptr;
        if (CreatePipe(&out_read, &out_write, &inherit, 0) == 0) {
            return child;
        }
        if (CreatePipe(&in_read, &in_write, &inherit, 0) == 0) {
            CloseHandle(out_read);
            CloseHandle(out_write);
            return child;
        }
        SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = in_read;
        startup.hStdOutput = out_write;
        startup.hStdError = out_write;
        std::wstring command = L"\"" + exe.wstring() + L"\"";
        for (const std::string& arg : args) {
            command += L" \"" + std::wstring(arg.begin(), arg.end()) + L"\"";
        }
        PROCESS_INFORMATION info{};
        const BOOL ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);
        CloseHandle(in_read);
        CloseHandle(out_write);
        if (ok == 0) {
            CloseHandle(out_read);
            CloseHandle(in_write);
            return child;
        }
        CloseHandle(info.hThread);
        child.process_ = info.hProcess;
        child.pid_ = static_cast<long long>(info.dwProcessId);
        child.in_ = in_write;
        child.out_ = out_read;
#else
        int out_pipe[2];
        int in_pipe[2];
        if (::pipe(out_pipe) != 0) {
            return child;
        }
        if (::pipe(in_pipe) != 0) {
            ::close(out_pipe[0]);
            ::close(out_pipe[1]);
            return child;
        }
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, in_pipe[0], 0);
        posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 1);
        posix_spawn_file_actions_adddup2(&actions, out_pipe[1], 2);
        posix_spawn_file_actions_addclose(&actions, in_pipe[1]);
        posix_spawn_file_actions_addclose(&actions, out_pipe[0]);
        std::vector<std::string> owned;
        owned.push_back(exe.string());
        for (const std::string& arg : args) {
            owned.push_back(arg);
        }
        std::vector<char*> argv;
        for (std::string& arg : owned) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        pid_t pid = 0;
        const int rc =
            posix_spawn(&pid, owned.front().c_str(), &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        ::close(in_pipe[0]);
        ::close(out_pipe[1]);
        if (rc != 0) {
            ::close(in_pipe[1]);
            ::close(out_pipe[0]);
            return child;
        }
        child.pid_ = static_cast<long long>(pid);
        child.in_ = in_pipe[1];
        child.out_ = out_pipe[0];
#endif
        return child;
    }

    explicit operator bool() const noexcept { return pid_ > 0; }
    long long pid() const noexcept { return pid_; }

    bool running() {
        if (pid_ <= 0) {
            return false;
        }
#if defined(_WIN32)
        DWORD code = 0;
        return GetExitCodeProcess(process_, &code) != 0 && code == STILL_ACTIVE;
#else
        if (reaped_) {
            return false;
        }
        int status = 0;
        const pid_t got = ::waitpid(static_cast<pid_t>(pid_), &status, WNOHANG);
        if (got == static_cast<pid_t>(pid_)) {
            reaped_ = true;
            return false;
        }
        return got == 0;
#endif
    }

    /// The next complete output line without its line break, or "" when none
    /// arrived before the deadline or the child closed its output.
    std::string line(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        for (;;) {
            const auto end = buffer_.find('\n');
            if (end != std::string::npos) {
                std::string out = buffer_.substr(0, end);
                buffer_.erase(0, end + 1);
                if (!out.empty() && out.back() == '\r') {
                    out.pop_back();
                }
                return out;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline || !read_some(deadline - now)) {
                return {};
            }
        }
    }

    /// True when the child's output reached end-of-file within `timeout`: every process that
    /// holds the write end of the pipe has closed it, not only this child (a process the child
    /// started and left running keeps the pipe open). Buffered lines stay readable.
    bool wait_eof(std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (!read_some(deadline - std::chrono::steady_clock::now())) {
                return true;
            }
        }
        return false;
    }

    void send(const std::string& text) {
#if defined(_WIN32)
        if (in_ != nullptr) {
            DWORD written = 0;
            WriteFile(in_, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        }
#else
        if (in_ >= 0) {
            const ssize_t n = ::write(in_, text.data(), text.size());
            (void)n;
        }
#endif
    }

    void suspend() {
#if defined(_WIN32)
        using Fn = LONG(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSuspendProcess")));
        if (fn != nullptr && process_ != nullptr) {
            fn(process_);
        }
#else
        if (pid_ > 0) {
            ::kill(static_cast<pid_t>(pid_), SIGSTOP);
        }
#endif
    }

    void resume() {
#if defined(_WIN32)
        using Fn = LONG(NTAPI*)(HANDLE);
        const auto fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtResumeProcess")));
        if (fn != nullptr && process_ != nullptr) {
            fn(process_);
        }
#else
        if (pid_ > 0) {
            ::kill(static_cast<pid_t>(pid_), SIGCONT);
        }
#endif
    }

    /// Kills the child (if still running) and closes its pipes.
    void stop() {
#if defined(_WIN32)
        if (process_ != nullptr) {
            TerminateProcess(process_, 1);
            WaitForSingleObject(process_, 5000);
            CloseHandle(process_);
            process_ = nullptr;
        }
        if (in_ != nullptr) {
            CloseHandle(in_);
            in_ = nullptr;
        }
        if (out_ != nullptr) {
            CloseHandle(out_);
            out_ = nullptr;
        }
#else
        if (pid_ > 0 && !reaped_) {
            ::kill(static_cast<pid_t>(pid_), SIGCONT);
            ::kill(static_cast<pid_t>(pid_), SIGKILL);
            int status = 0;
            ::waitpid(static_cast<pid_t>(pid_), &status, 0);
            reaped_ = true;
        }
        if (in_ >= 0) {
            ::close(in_);
            in_ = -1;
        }
        if (out_ >= 0) {
            ::close(out_);
            out_ = -1;
        }
#endif
        pid_ = -1;
    }

  private:
    void take(Child& other) noexcept {
        process_ = other.process_;
        in_ = other.in_;
        out_ = other.out_;
        pid_ = other.pid_;
        buffer_ = std::move(other.buffer_);
#if !defined(_WIN32)
        reaped_ = other.reaped_;
#endif
        other.process_ = {};
#if defined(_WIN32)
        other.in_ = nullptr;
        other.out_ = nullptr;
#else
        other.in_ = -1;
        other.out_ = -1;
#endif
        other.pid_ = -1;
    }

    bool read_some(std::chrono::steady_clock::duration budget) {
        char chunk[512];
#if defined(_WIN32)
        const auto deadline = std::chrono::steady_clock::now() + budget;
        while (std::chrono::steady_clock::now() < deadline) {
            DWORD available = 0;
            if (PeekNamedPipe(out_, nullptr, 0, nullptr, &available, nullptr) == 0) {
                return false;
            }
            if (available > 0) {
                DWORD got = 0;
                const DWORD want = available < sizeof(chunk) ? available : sizeof(chunk);
                if (ReadFile(out_, chunk, want, &got, nullptr) == 0 || got == 0) {
                    return false;
                }
                buffer_.append(chunk, chunk + got);
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
#else
        pollfd fd{};
        fd.fd = out_;
        fd.events = POLLIN;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(budget).count();
        const int ready = ::poll(&fd, 1, static_cast<int>(ms < 1 ? 1 : ms));
        if (ready <= 0) {
            return ready == 0 || errno == EINTR;
        }
        const ssize_t got = ::read(out_, chunk, sizeof(chunk));
        if (got <= 0) {
            return false;
        }
        buffer_.append(chunk, chunk + got);
        return true;
#endif
    }

#if defined(_WIN32)
    HANDLE process_ = nullptr;
    HANDLE in_ = nullptr;
    HANDLE out_ = nullptr;
#else
    int process_ = 0;
    int in_ = -1;
    int out_ = -1;
    bool reaped_ = false;
#endif
    long long pid_ = -1;
    std::string buffer_;
};

} // namespace anno::test::child
