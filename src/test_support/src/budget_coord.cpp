// Detached host coordinator. Windows clients reach it through localhost
// forwarding; it binds inside WSL (or on the local host for same-OS tests).
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

#include <anno/test/budget.hpp>

#include "coord_identity.hpp"

// anno_budget_coord <port> <any|local> [--journal <path>] [--build-id <hex>]
//                   [--compat-version <n>]
// --build-id is provenance: tests start a coordinator "of another build" with it.
// --compat-version sets the compatibility version this coordinator speaks (tests
// only; the default is wire::kCompatVersion).
int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--identity") {
        std::printf("COORD-IDENTITY build=%016llx config=%s marker=%s\n",
                    static_cast<unsigned long long>(anno::test::coordinator_build()),
                    coordinator_config_name(), coordinator_marker());
#if !defined(_WIN32)
        std::ifstream maps("/proc/self/maps");
        std::string map_line;
        while (std::getline(maps, map_line)) {
            if (map_line.find("libasan") != std::string::npos) {
                std::printf("instrumented=libasan\n");
                break;
            }
        }
#endif
        return 0;
    }
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
        } else if (flag == "--compat-version") {
            options.compat = std::stoull(argv[i + 1], nullptr, 10);
        } else {
            return 2;
        }
    }
    return anno::test::run_budget_server(options);
}
