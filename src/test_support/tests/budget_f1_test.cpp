// T10c F1 regressions. Injected counters and isolated coordinator files only:
// nothing here allocates enough memory to exhaust the host.
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include <doctest/doctest.h>

#include <anno/test/budget.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

std::atomic<std::uint64_t> g_free{100};
std::atomic<std::uint64_t> g_private{0};
std::atomic<int> g_f1_waits{0};

void note_wait() {
    g_f1_waits.fetch_add(1);
}

std::optional<std::uint64_t> free_of() {
    return g_free.load();
}

std::optional<std::uint64_t> private_of() {
    return g_private.load();
}

std::optional<std::uint64_t> free_unknown() {
    return std::nullopt;
}

anno::test::BudgetHooks both_hooks() {
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &free_of;
    hooks.process_private = &private_of;
    return hooks;
}

#if defined(_WIN32)
std::filesystem::path probe_executable() {
    wchar_t buffer[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    REQUIRE(n > 0);
    return std::filesystem::path(buffer).parent_path() / "anno_budget_probe.exe";
}
#endif

std::filesystem::path work_file(const char* name) {
    const auto dir = std::filesystem::temp_directory_path() / "anno1503-f1";
    std::filesystem::create_directories(dir);
    const auto path = dir / name;
    std::error_code error;
    std::filesystem::remove(path, error);
    return path;
}

#if defined(_WIN32)
struct Child {
    HANDLE process = nullptr;
    HANDLE in_write = nullptr;
    HANDLE out_read = nullptr;

    Child() = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    Child(Child&& other) noexcept { *this = std::move(other); }
    Child& operator=(Child&& other) noexcept {
        if (this != &other) {
            stop();
            process = other.process;
            in_write = other.in_write;
            out_read = other.out_read;
            other.process = nullptr;
            other.in_write = nullptr;
            other.out_read = nullptr;
        }
        return *this;
    }

    ~Child() { stop(); }

    void stop() {
        if (process != nullptr) {
            TerminateProcess(process, 1);
            WaitForSingleObject(process, 3000);
            CloseHandle(process);
            process = nullptr;
        }
        if (in_write != nullptr) {
            CloseHandle(in_write);
            in_write = nullptr;
        }
        if (out_read != nullptr) {
            CloseHandle(out_read);
            out_read = nullptr;
        }
    }

    bool running() const {
        if (process == nullptr) {
            return false;
        }
        DWORD code = 0;
        if (GetExitCodeProcess(process, &code) == 0) {
            return false;
        }
        return code == STILL_ACTIVE;
    }
};

Child start_probe(const std::filesystem::path& exe, const std::string& args) {
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE out_read = nullptr;
    HANDLE out_write = nullptr;
    HANDLE in_read = nullptr;
    HANDLE in_write = nullptr;
    REQUIRE(CreatePipe(&out_read, &out_write, &inherit, 0) != 0);
    REQUIRE(CreatePipe(&in_read, &in_write, &inherit, 0) != 0);
    SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(in_write, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = in_read;
    startup.hStdOutput = out_write;
    startup.hStdError = out_write;
    std::wstring command = L"\"" + exe.wstring() + L"\" " + std::wstring(args.begin(), args.end());
    PROCESS_INFORMATION info{};
    const BOOL ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);
    CloseHandle(in_read);
    CloseHandle(out_write);
    REQUIRE(ok != 0);
    CloseHandle(info.hThread);
    Child child;
    child.process = info.hProcess;
    child.in_write = in_write;
    child.out_read = out_read;
    return child;
}

std::string read_for(Child& child, std::chrono::milliseconds budget) {
    std::string out;
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        DWORD available = 0;
        if (PeekNamedPipe(child.out_read, nullptr, 0, nullptr, &available, nullptr) != 0 &&
            available > 0) {
            char buffer[256];
            const DWORD want =
                available < sizeof(buffer) ? available : static_cast<DWORD>(sizeof(buffer));
            DWORD got = 0;
            if (ReadFile(child.out_read, buffer, want, &got, nullptr) != 0 && got > 0) {
                out.append(buffer, buffer + got);
                if (out.find('\n') != std::string::npos) {
                    break;
                }
            }
        } else if (!child.running()) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    return out;
}
#else
struct Child {
    pid_t pid = -1;
    int in_write = -1;
    int out_read = -1;

    ~Child() { stop(); }

    void stop() {
        if (pid > 0) {
            kill(pid, SIGKILL);
            int status = 0;
            waitpid(pid, &status, 0);
            pid = -1;
        }
        if (in_write >= 0) {
            ::close(in_write);
            in_write = -1;
        }
        if (out_read >= 0) {
            ::close(out_read);
            out_read = -1;
        }
    }

    bool running() const { return pid > 0; }
};
#endif

} // namespace

TEST_CASE("retained private bytes are not credited to a new reservation" *
          doctest::test_suite("budget")) {
    // capacity/free 100. An old hold of 60 becomes private cache and is
    // released. Two fresh unallocated requests of 40 must not both be admitted:
    // together they are twice the memory still free.
    g_free = 100;
    g_private = 0;
    auto budget = anno::test::Budget::memory(both_hooks(), 0);
    auto old = budget->try_reserve(60);
    REQUIRE(old);
    g_private = 60;
    g_free = 40;
    old.release();
    anno::test::Admit first;
    anno::test::Admit second;
    const auto one = budget->try_reserve(40, &first);
    const auto two = budget->try_reserve(40, &second);
    CHECK(one);
    CHECK_FALSE(two);
    CHECK(budget->reserved() == 40);
    CHECK(second.resident == 0);
}

TEST_CASE("unknown free memory does not admit the request" * doctest::test_suite("budget")) {
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &free_unknown;
    auto budget = anno::test::Budget::memory(hooks, 0);
    anno::test::Admit info;
    const auto hold = budget->try_reserve(1, &info);
    // A meter that gave no number: not admitted, and a blocking reserve would
    // wait for a reading (bounded by its deadline) instead of failing at once.
    CHECK_FALSE(hold);
    CHECK_FALSE(info.admitted);
    CHECK(info.reason == "free memory unknown");
    CHECK(budget->reserved() == 0);
}

TEST_CASE("a timed recheck does not admit until the budget fits" * doctest::test_suite("budget")) {
    g_free = 50;
    g_private = 0;
    const auto path = work_file("recheck.bin");
    auto budget = anno::test::Budget::file(path, both_hooks(), 0);
    const auto held = budget->try_reserve(40);
    REQUIRE(held);
    std::atomic<int> done{0};
    anno::test::Reservation second;
    anno::test::Admit info;
    std::thread waiter([&] {
        second = budget->reserve(30, &info);
        done.store(1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    CHECK(done.load() == 0);
    g_free = 100;
    for (int i = 0; i < 40 && done.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    waiter.join();
    CHECK(second);
    CHECK(info.admitted);
    CHECK(info.waited);
    CHECK_FALSE(info.refused);
    budget.reset();
    std::filesystem::remove(path);
}

TEST_CASE("cancelling a waiter does not release the bytes it is waiting on" *
          doctest::test_suite("budget")) {
    g_free = 100;
    g_private = 0;
    g_f1_waits = 0;
    auto hooks = both_hooks();
    hooks.on_wait = &note_wait;
    const auto path = work_file("cancel.bin");
    auto holder = anno::test::Budget::file(path, both_hooks(), 0);
    auto waiter = anno::test::Budget::file(path, hooks, 0);
    const auto held = holder->try_reserve(80);
    REQUIRE(held);
    anno::test::Admit info;
    anno::test::Reservation waited;
    std::thread blocked([&] { waited = waiter->reserve(60, &info); });
    const auto start = std::chrono::steady_clock::now();
    while (g_f1_waits.load() == 0 &&
           std::chrono::steady_clock::now() - start < std::chrono::seconds(5)) {
        std::this_thread::yield();
    }
    REQUIRE(g_f1_waits.load() > 0);
    waiter->cancel();
    blocked.join();
    CHECK_FALSE(waited);
    CHECK(info.refused);
    CHECK(info.reason == "cancelled");
    CHECK(held);
    CHECK(holder->reserved() == 80);
    holder.reset();
    waiter.reset();
    std::filesystem::remove(path);
}

#if defined(_WIN32)
TEST_CASE("a killed owner lets a memory waiter and a cpu waiter continue" *
          doctest::test_suite("budget")) {
    _putenv("ANNO_TEST_CPU_BUDGET=1");
    const auto exe = probe_executable();
    REQUIRE(std::filesystem::exists(exe));
    const auto path = work_file("owner-death.bin");
    auto holder = start_probe(exe, "holder \"" + path.string() + "\" 80");
    const std::string ready = read_for(holder, std::chrono::milliseconds(5000));
    REQUIRE(ready.find("READY 1") != std::string::npos);

    auto waiter = start_probe(exe, "waiter \"" + path.string() + "\" 60");
    const std::string waiting = read_for(waiter, std::chrono::milliseconds(5000));
    REQUIRE(waiting.find("WAITING") != std::string::npos);

    auto cpu_holder = start_probe(exe, "cpu-holder \"" + path.string() + "\" 1");
    const std::string cpu_ready = read_for(cpu_holder, std::chrono::milliseconds(5000));
    REQUIRE(cpu_ready.find("READY 1") != std::string::npos);
    auto cpu_waiter = start_probe(exe, "cpu-waiter \"" + path.string() + "\" 1");
    const std::string cpu_early = read_for(cpu_waiter, std::chrono::milliseconds(400));
    CHECK(cpu_early.find("ADMITTED") == std::string::npos);
    CHECK(cpu_waiter.running());

    holder.stop();
    cpu_holder.stop();
    const std::string admitted = read_for(waiter, std::chrono::milliseconds(3000));
    const std::string cpu_admitted = read_for(cpu_waiter, std::chrono::milliseconds(3000));
    CHECK(admitted.find("ADMITTED 1") != std::string::npos);
    CHECK(cpu_admitted.find("ADMITTED 1") != std::string::npos);
    waiter.stop();
    cpu_waiter.stop();
    std::filesystem::remove(path);
    _putenv("ANNO_TEST_CPU_BUDGET=");
}

TEST_CASE("a live Windows owner is not reclaimed by a WSL client" * doctest::test_suite("budget")) {
    const auto exe = probe_executable();
    REQUIRE(std::filesystem::exists(exe));
    // The worktree this test was built from, as WSL sees it.
    const auto source = std::filesystem::path(__FILE__).parent_path().parent_path();
    const auto tree = source.parent_path().parent_path();
    const auto root = tree / "cache" / "f1";
    std::filesystem::create_directories(root);
    const auto path = root / "foreign.bin";
    std::error_code error;
    std::filesystem::remove(path, error);
    auto wsl = [](const std::filesystem::path& p) {
        const std::string text = p.generic_string();
        if (text.size() >= 2 && text[1] == ':') {
            return std::string("/mnt/") +
                   static_cast<char>(std::tolower(static_cast<unsigned char>(text[0]))) +
                   text.substr(2);
        }
        return text;
    };
    _putenv("ANNO_TEST_BUDGET_ID=f1-foreign");
    _putenv("ANNO_TEST_BUDGET_COORD=wsl");
    auto holder = start_probe(exe, "holder \"" + path.string() + "\" 80");
    const std::string ready = read_for(holder, std::chrono::milliseconds(60000));
    REQUIRE(ready.find("READY 1") != std::string::npos);

    const auto linux_probe = root / "budget_probe_linux";
    // The same coordinator build id as this binary: a client of another id
    // is refused by the shared coordinator.
    char build[32];
    std::snprintf(build, sizeof(build), "0x%016llxULL",
                  static_cast<unsigned long long>(anno::test::coordinator_build()));
    const std::string compile = "wsl -d Ubuntu -- g++ -std=c++20 -O1 -g -pthread "
                                "-DANNO_COORD_BUILD=" +
                                std::string(build) + " -I" +
                                wsl(source / "include") + " " + wsl(source / "src" / "budget.cpp") +
                                " " + wsl(source / "src" / "coord.cpp") + " " +
                                wsl(source / "src" / "coord_server.cpp") + " " +
                                wsl(source / "src" / "memory.cpp") + " " +
                                wsl(source / "tests" / "budget_probe.cpp") + " -o " +
                                wsl(linux_probe);
    REQUIRE(std::system(compile.c_str()) == 0);

    const std::string run = "wsl -d Ubuntu -- env ANNO_TEST_BUDGET_ID=f1-foreign "
                            "ANNO_TEST_BUDGET_COORD=wsl " +
                            wsl(linux_probe) + " try " + wsl(path) + " 80";
    FILE* pipe = _popen(run.c_str(), "r");
    REQUIRE(pipe != nullptr);
    std::string output;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int code = _pclose(pipe);
    MESSAGE("wsl try: " << output << " exit " << code);
    // The live Windows reservation must still occupy its 80. A second claim of
    // 80 against a free budget of 100 does not fit.
    CHECK(output.find("claim=0") != std::string::npos);
    CHECK(output.find("reason=admitted") == std::string::npos);
    holder.stop();
    std::filesystem::remove(path);
    std::filesystem::remove(linux_probe);
    _putenv("ANNO_TEST_BUDGET_ID=");
    _putenv("ANNO_TEST_BUDGET_COORD=");
}
#endif
