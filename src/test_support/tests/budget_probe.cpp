// Small budget client for the T10c regressions. Injected free memory (100
// bytes unless argv[4] says otherwise) so the process never asks the host for
// a real allocation.
//
//   anno_budget_probe <mode> <path> <bytes> [free]
//   holder      reserve, print "READY 1", then read commands from stdin:
//               r = release ("RELEASED ms=<n>"), t = try the bytes again
//               ("CLAIM <0|1> ms=<n> reason=<r>"), q or end of input = exit
//   cpu-holder  acquire a CPU token, print "READY 1", hold until q/end of input
//   waiter      reserve, print "ADMITTED <0|1> ms=<n> reason=<r>"; an admitted
//               waiter then holds until q/end of input
//   cpu-waiter  acquire a CPU token, print "ADMITTED <0|1> ms=<n>"
//   try         one try_reserve, print "claim=<0|1> ... reason=<r>"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include <anno/test/budget.hpp>

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

long long ms_since(std::chrono::steady_clock::time_point t0) {
    return static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - t0)
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

} // namespace

int main(int argc, char** argv) {
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
    if (argc < 4) {
        return 2;
    }
    const std::filesystem::path path = argv[2];
    const auto bytes = static_cast<std::uint64_t>(std::stoull(argv[3]));
    if (argc > 4) {
        g_free = static_cast<std::uint64_t>(std::stoull(argv[4]));
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
