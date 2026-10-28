// Small budget client for the T10c F1 regressions. Injected free memory is 100
// bytes so the process never asks the host for a real allocation.
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
    auto budget = anno::test::Budget::file(path, hooks, 0);
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
        std::cout << "READY " << static_cast<int>(ready) << std::endl;
        char ch = 0;
        std::cin.get(ch);
        return 0;
    }
    if (mode == "waiter") {
        hold = budget->reserve(bytes, &info);
        std::cout << "ADMITTED " << static_cast<int>(static_cast<bool>(hold)) << " " << info.reason
                  << std::endl;
        return hold ? 0 : 1;
    }
    if (mode == "cpu-waiter") {
        cpu = budget->acquire_cpu();
        std::cout << "ADMITTED " << static_cast<int>(static_cast<bool>(cpu)) << std::endl;
        return cpu ? 0 : 1;
    }
    hold = budget->try_reserve(bytes, &info);
    std::cout << "claim=" << static_cast<int>(static_cast<bool>(hold))
              << " pending=" << info.pending << " resident=" << info.resident
              << " reason=" << info.reason << std::endl;
    return hold ? 0 : 1;
}
