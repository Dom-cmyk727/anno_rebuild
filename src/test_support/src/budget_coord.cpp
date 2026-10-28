// Detached host coordinator. Windows clients reach it through localhost
// forwarding; it binds inside WSL (or on the local host for same-OS tests).
#include <cstdint>
#include <string>

#include <anno/test/budget.hpp>

// anno_budget_coord <port> <any|local> [--journal <path>] [--build-id <hex>]
int main(int argc, char** argv) {
    anno::test::BudgetServerOptions options;
    unsigned long port = 47631;
    if (argc > 1) {
        port = std::stoul(argv[1]);
    }
    if (port > 65535ul) {
        return 2;
    }
    options.port = static_cast<std::uint16_t>(port);
    options.all_interfaces = argc > 2 && std::string(argv[2]) == "any";
    for (int i = 3; i + 1 < argc; i += 2) {
        const std::string flag = argv[i];
        if (flag == "--journal") {
            options.journal = argv[i + 1];
        } else if (flag == "--build-id") {
            options.build = std::stoull(argv[i + 1], nullptr, 16);
        } else {
            return 2;
        }
    }
    return anno::test::run_budget_server(options);
}
