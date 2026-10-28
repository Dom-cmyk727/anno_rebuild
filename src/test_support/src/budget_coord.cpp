// Detached host coordinator. Windows clients reach it through localhost
// forwarding; it binds inside WSL (or on the local host for same-OS tests).
#include <cstdint>
#include <string>

#include <anno/test/budget.hpp>

int main(int argc, char** argv) {
    unsigned long port = 47631;
    if (argc > 1) {
        port = std::stoul(argv[1]);
    }
    const bool all = argc > 2 && std::string(argv[2]) == "any";
    if (port > 65535ul) {
        return 2;
    }
    return anno::test::run_budget_server(static_cast<std::uint16_t>(port), all);
}
