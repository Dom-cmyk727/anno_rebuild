// Small budget client for the T10c regressions. Injected free memory (100
// bytes unless argv[4] says otherwise) so the process never asks the host for
// a real allocation.
//
//   anno_budget_probe <mode> <path> <bytes> [free]
//   holder      reserve, print "READY 1 reason=<r>", then read commands from stdin:
//               r = release ("RELEASED ms=<n>"), t = try the bytes again
//               ("CLAIM <0|1> ms=<n> reason=<r>"), q or end of input = exit
//   cpu-holder  acquire a CPU token, print "READY 1", hold until q/end of input
//   waiter      reserve, print "ADMITTED <0|1> ms=<n> reason=<r>"; an admitted
//               waiter then holds until q/end of input
//   cpu-waiter  acquire a CPU token, print "ADMITTED <0|1> ms=<n>"
//   try         one try_reserve, print "claim=<0|1> ... reason=<r>"
//   reqwait     reserve on a worker thread; print "WAITING" when the reservation
//               starts to wait; a line "c" cancels it. Prints "RESULT <0|1>
//               reason=<r>" when it ends; an admitted reservation then holds
//               until q/end of input
//   silent      listen on the TCP port given as <path> (a number) and accept
//               nothing; print "SILENT 1"; exit on q/end of input
//   reqwait2    two threads reserve on the same budget; print "WAITING" once
//               from on_wait; a line "c" cancels once. Each thread prints
//               "RESULT <t> <0|1> reason=<r>" with t 1 or 2
//   Any exception from the library prints "EXCEPTION <what>" and exits 3.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include <anno/test/budget.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

std::uint64_t g_free = 100;
std::uint64_t g_private = 0;

std::optional<std::uint64_t> free_of() {
    return g_free;
}

std::optional<std::uint64_t> private_of() {
    return g_private;
}

void waiting() {
    std::cout << "WAITING" << std::endl;
}

std::atomic<int> g_reqwait2_waits{0};

void waiting_once() {
    if (g_reqwait2_waits.fetch_add(1) == 0) {
        std::cout << "WAITING" << std::endl;
    }
}

long long ms_since(std::chrono::steady_clock::time_point t0) {
    return static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
            .count());
}

/// Reads stdin until 'q' or end of input; runs `on` for every other command.
template <class F> void serve(F on) {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        if (line[0] == 'q') {
            return;
        }
        on(line[0]);
    }
}

/// A TCP listener that never accepts: a connection completes in the kernel's
/// backlog and no hello is ever answered.
int silent_listener(std::uint16_t port) {
#if defined(_WIN32)
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return 1;
    }
    const SOCKET fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET) {
        return 1;
    }
#else
    const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return 1;
    }
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(0x7f000001u);
#if defined(_WIN32)
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(fd, 8) != 0) {
        ::closesocket(fd);
        return 1;
    }
#else
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(fd, 8) != 0) {
        ::close(fd);
        return 1;
    }
#endif
    // A stuck coordinator still accepts connections and answers none of them:
    // accept in a thread and keep every connection open, so the backlog never
    // fills (a listener that never accepts refuses new connections once its
    // backlog is full, which is a different condition).
    std::cout << "SILENT 1" << std::endl;
    std::thread acceptor([fd] {
        for (;;) {
#if defined(_WIN32)
            const SOCKET client = ::accept(fd, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                return;
            }
#else
            const int client = ::accept(fd, nullptr, nullptr);
            if (client < 0) {
                return;
            }
#endif
            // Held open on purpose; the process exit closes it.
            (void)client;
        }
    });
    acceptor.detach();
    serve([](char) {});
    return 0; // the exit closes the listener and every held connection
}

int run(int argc, char** argv) {
    if (argc < 2) {
        return 2;
    }
    const std::string mode = argv[1];
    anno::test::BudgetHooks hooks;
    hooks.free_bytes = &free_of;
    hooks.process_private = &private_of;
    hooks.on_wait = &waiting;
    if (mode == "retained") {
        auto budget = anno::test::Budget::memory(hooks, 0);
        auto old = budget->try_reserve(60);
        g_private = 60;
        g_free = 40;
        old.release();
        anno::test::Admit first;
        anno::test::Admit second;
        const auto one = budget->try_reserve(40, &first);
        const auto two = budget->try_reserve(40, &second);
        std::cout << "free=" << g_free << " retained_old=" << g_private
                  << " admitted_new=" << budget->reserved()
                  << " first=" << static_cast<int>(static_cast<bool>(one))
                  << " second=" << static_cast<int>(static_cast<bool>(two))
                  << " second_pending=" << second.pending << " second_resident=" << second.resident
                  << std::endl;
        return two ? 1 : 0;
    }
    if (mode == "silent") {
        if (argc < 3) {
            return 2;
        }
        return silent_listener(static_cast<std::uint16_t>(std::stoul(argv[2])));
    }
    if (argc < 4) {
        return 2;
    }
    const std::filesystem::path path = argv[2];
    const auto bytes = static_cast<std::uint64_t>(std::stoull(argv[3]));
    if (argc > 4) {
        g_free = static_cast<std::uint64_t>(std::stoull(argv[4]));
    }
    if (mode == "reqwait2") {
        hooks.on_wait = &waiting_once;
    }
    std::shared_ptr<anno::test::Budget> budget;
    try {
        budget = anno::test::Budget::file(path, hooks, 0);
    } catch (const std::exception& error) {
        std::cout << "CONNECT 0 reason=" << error.what() << std::endl;
        return 3;
    }
    anno::test::Admit info;
    // The reservation has to stay alive until stdin closes. A temporary would
    // release it before the other client runs.
    anno::test::Reservation hold;
    anno::test::CpuToken cpu;
    if (mode == "reqwait2") {
        std::atomic<bool> quit{false};
        std::thread reader([&] {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!line.empty() && line[0] == 'c') {
                    budget->cancel();
                }
                if (!line.empty() && line[0] == 'q') {
                    break;
                }
            }
            quit = true;
        });
        reader.detach();
        anno::test::Admit waited1;
        anno::test::Admit waited2;
        anno::test::Reservation hold1;
        anno::test::Reservation hold2;
        std::mutex io;
        std::thread worker1([&] {
            hold1 = budget->reserve(bytes, &waited1);
            std::lock_guard<std::mutex> lock(io);
            std::cout << "RESULT 1 " << static_cast<int>(static_cast<bool>(hold1))
                      << " reason=" << waited1.reason << std::endl;
        });
        std::thread worker2([&] {
            hold2 = budget->reserve(bytes, &waited2);
            std::lock_guard<std::mutex> lock(io);
            std::cout << "RESULT 2 " << static_cast<int>(static_cast<bool>(hold2))
                      << " reason=" << waited2.reason << std::endl;
        });
        worker1.join();
        worker2.join();
        (void)quit;
        return (hold1 || hold2) ? 0 : 1;
    }
    if (mode == "reqwait") {
        std::atomic<bool> done{false};
        std::atomic<bool> quit{false};
        std::thread reader([&] {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!line.empty() && line[0] == 'c') {
                    budget->cancel();
                }
                if (!line.empty() && line[0] == 'q') {
                    break;
                }
            }
            quit = true;
        });
        reader.detach();
        anno::test::Admit waited;
        std::thread worker([&] {
            hold = budget->reserve(bytes, &waited);
            done = true;
        });
        worker.join();
        std::cout << "RESULT " << static_cast<int>(static_cast<bool>(hold))
                  << " reason=" << waited.reason << std::endl;
        if (hold) {
            while (!quit.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
        return hold ? 0 : 1;
    }
    if (mode == "holder" || mode == "cpu-holder") {
        if (mode == "holder") {
            hold = budget->reserve(bytes, &info);
        } else {
            cpu = budget->acquire_cpu();
        }
        const bool ready = mode == "holder" ? static_cast<bool>(hold) : static_cast<bool>(cpu);
        std::cout << "READY " << static_cast<int>(ready) << " reason=" << info.reason << std::endl;
        serve([&](char command) {
            const auto t0 = std::chrono::steady_clock::now();
            if (command == 'r') {
                hold.release();
                cpu.release();
                std::cout << "RELEASED ms=" << ms_since(t0) << std::endl;
            } else if (command == 't') {
                anno::test::Admit again;
                auto next = budget->try_reserve(bytes, &again);
                std::cout << "CLAIM " << static_cast<int>(static_cast<bool>(next))
                          << " ms=" << ms_since(t0) << " reason=" << again.reason << std::endl;
            }
        });
        return 0;
    }
    if (mode == "waiter") {
        const auto t0 = std::chrono::steady_clock::now();
        hold = budget->reserve(bytes, &info);
        std::cout << "ADMITTED " << static_cast<int>(static_cast<bool>(hold))
                  << " ms=" << ms_since(t0) << " reason=" << info.reason << std::endl;
        if (hold) {
            serve([](char) {});
        }
        return hold ? 0 : 1;
    }
    if (mode == "cpu-waiter") {
        const auto t0 = std::chrono::steady_clock::now();
        cpu = budget->acquire_cpu();
        std::cout << "ADMITTED " << static_cast<int>(static_cast<bool>(cpu))
                  << " ms=" << ms_since(t0) << std::endl;
        if (cpu) {
            serve([](char) {});
        }
        return cpu ? 0 : 1;
    }
    hold = budget->try_reserve(bytes, &info);
    std::cout << "claim=" << static_cast<int>(static_cast<bool>(hold))
              << " pending=" << info.pending << " resident=" << info.resident
              << " reason=" << info.reason << std::endl;
    return hold ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cout << "EXCEPTION " << error.what() << std::endl;
        return 3;
    }
}
