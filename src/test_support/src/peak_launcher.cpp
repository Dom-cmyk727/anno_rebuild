// anno_peak_launcher <program> [args...]
//
// Opt-in test launcher (W4-NEXT-2 part M, C-3): runs one test process, waits for it, and prints
// one machine-readable line with the process's peak memory to stderr:
//
//   [anno-test peak] peak_bytes=<n> source=<how> exit=<code> wall_ms=<ms> test=<name>
//
// Linux: ru_maxrss of the child from wait4 (KiB on Linux, bytes on macOS); it is the largest
// resident set of the test process or of any descendant it waited for. Windows:
// PeakWorkingSetSize of the child's handle after it exited (GetProcessMemoryInfo), the process
// itself only. `test` is the value of a `--test-case=` argument when there is one (doctest's
// discovered cases), else the program's file name.
//
// It changes no result: stdin, stdout and stderr are inherited, the exit code is the child's,
// and a child killed by a signal kills the launcher with the same signal. Enabled by the CMake
// option ANNO_TEST_PEAK_MEMORY (off by default), which sets it as the doctest targets'
// CROSSCOMPILING_EMULATOR so that doctest's discovery runs every case through it.
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// windows.h first.
#include <psapi.h>
#else
#include <csignal>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#endif

namespace {

constexpr int kLaunchFailed = 125; // as env(1) and timeout(1): the launcher itself failed

std::string test_name(int argc, char** argv) {
    static constexpr char kPrefix[] = "--test-case=";
    for (int i = 2; i < argc; ++i) {
        if (std::strncmp(argv[i], kPrefix, sizeof(kPrefix) - 1) == 0) {
            return argv[i] + sizeof(kPrefix) - 1;
        }
    }
    std::string program = argv[1];
    const auto slash = program.find_last_of("/\\");
    return slash == std::string::npos ? program : program.substr(slash + 1);
}

void report(std::uint64_t peak, const char* source, long long code, long long wall_ms,
            const std::string& name) {
    std::fprintf(stderr,
                 "[anno-test peak] peak_bytes=%llu source=%s exit=%lld wall_ms=%lld test=%s\n",
                 static_cast<unsigned long long>(peak), source, code, wall_ms, name.c_str());
    std::fflush(stderr);
}

#if defined(_WIN32)
/// One argument quoted by the rules CommandLineToArgvW and the MSVC runtime parse with.
std::wstring quote(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            out.append(slashes * 2 + 1, L'\\');
        } else {
            out.append(slashes, L'\\');
        }
        slashes = 0;
        out.push_back(c);
    }
    out.append(slashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}
#endif

} // namespace

#if defined(_WIN32)
int wmain(int argc, wchar_t** wargv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: anno_peak_launcher <program> [args...]\n");
        return kLaunchFailed;
    }
    std::vector<std::string> narrow;
    std::vector<char*> argv;
    for (int i = 0; i < argc; ++i) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) {
            WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), n, nullptr, nullptr);
        }
        narrow.push_back(std::move(s));
    }
    for (auto& s : narrow) {
        argv.push_back(s.data());
    }
    std::wstring command;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) {
            command.push_back(L' ');
        }
        command += quote(wargv[i]);
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    const auto start = std::chrono::steady_clock::now();
    if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                       &startup, &info) == 0) {
        std::fprintf(stderr, "anno_peak_launcher: cannot start %s (error %lu)\n", argv[1],
                     static_cast<unsigned long>(GetLastError()));
        return kLaunchFailed;
    }
    CloseHandle(info.hThread);
    WaitForSingleObject(info.hProcess, INFINITE);
    const auto wall = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    DWORD code = 0;
    GetExitCodeProcess(info.hProcess, &code);
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = sizeof(counters);
    std::uint64_t peak = 0;
    if (GetProcessMemoryInfo(info.hProcess, &counters, sizeof(counters)) != 0) {
        peak = static_cast<std::uint64_t>(counters.PeakWorkingSetSize);
    }
    CloseHandle(info.hProcess);
    report(peak, "PeakWorkingSetSize", static_cast<long long>(code), static_cast<long long>(wall),
           test_name(argc, argv.data()));
    return static_cast<int>(code);
}
#else
int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: anno_peak_launcher <program> [args...]\n");
        return kLaunchFailed;
    }
    const auto start = std::chrono::steady_clock::now();
#if defined(__linux__)
    const pid_t parent = ::getpid();
#endif
    const pid_t pid = ::fork();
    if (pid < 0) {
        std::perror("anno_peak_launcher: fork");
        return kLaunchFailed;
    }
    if (pid == 0) {
#if defined(__linux__)
        // A CTest timeout kills the launcher; the test process must not outlive it.
        ::prctl(PR_SET_PDEATHSIG, SIGKILL);
        if (::getppid() != parent) {
            ::_exit(kLaunchFailed);
        }
#endif
        ::execvp(argv[1], argv + 1);
        std::perror("anno_peak_launcher: exec");
        ::_exit(kLaunchFailed);
    }
    // While the child runs, an interrupt reaches both through the terminal's process group;
    // the launcher waits for the child and reports.
    std::signal(SIGINT, SIG_IGN);
    std::signal(SIGQUIT, SIG_IGN);
    int status = 0;
    rusage usage{};
    pid_t done = -1;
    do {
        done = ::wait4(pid, &status, 0, &usage);
    } while (done < 0 && errno == EINTR);
    const auto wall = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    if (done < 0) {
        std::perror("anno_peak_launcher: wait4");
        return kLaunchFailed;
    }
#if defined(__APPLE__)
    const std::uint64_t peak = static_cast<std::uint64_t>(usage.ru_maxrss); // bytes
    const char* source = "ru_maxrss(bytes)";
#else
    const std::uint64_t peak = static_cast<std::uint64_t>(usage.ru_maxrss) * 1024u; // KiB
    const char* source = "ru_maxrss(KiB)";
#endif
    const std::string name = test_name(argc, argv);
    if (WIFSIGNALED(status)) {
        const int sig = WTERMSIG(status);
        report(peak, source, -sig, static_cast<long long>(wall), name);
        std::signal(sig, SIG_DFL);
        ::raise(sig);
        return 128 + sig;
    }
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : kLaunchFailed;
    report(peak, source, code, static_cast<long long>(wall), name);
    return code;
}
#endif
