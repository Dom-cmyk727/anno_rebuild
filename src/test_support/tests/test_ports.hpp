// Fixed TCP ports of the budget-coordinator regressions (F2-F5, budget_test).
//
// Why below 32768: a fixed port inside the host's ephemeral (dynamic) port range is also handed
// out as the local port of outgoing connections. Under `ctest -j`, a client socket of one case
// (connected to its own coordinator) can sit on another case's fixed port; that case's
// coordinator then prints BIND-FAILED and the case fails (CI run 41985663081, linux-gcc, F3 on
// 47814: Linux's default range is 32768-60999). SO_REUSEADDR does not help: the port is held by
// a live socket, not by TIME_WAIT. Default ranges: Linux 32768-60999
// (/proc/sys/net/ipv4/ip_local_port_range), Windows 49152-65535 (`netsh int ipv4 show
// dynamicport tcp`), macOS 49152-65535 (sysctl net.inet.ip.portrange.first/last). The band
// below lies under all three, so no default host assigns it to a connection. Binding port 0
// does not fit these cases: handover, crash recovery, step-aside and client-spawned successors
// all need a second coordinator on the port the first one held.
//
// The production ports (coord_wire.hpp kSharedPort 47631, kLocalPort 47632) are unchanged:
// every client of the shared coordinator finds it by that number, so moving it would split the
// budget between trees built before and after the move (owner decision 2026-11-07, docs/perf/
// README.md "Coordinator ports").
#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include <doctest/doctest.h>

#include "child_process.hpp"

namespace anno::test::ports {

/// Every fixed port a coordinator regression binds or connects to lies in this band. The
/// files divide it: F2 27700-27760, F3 27810-27817, F4 27820-27826, F5 27850-27925, and the
/// file-client cases of budget_test and F1 27632 (kFileClientPort). Exempt: the Windows-only F1
/// case "a live Windows owner is not reclaimed by a WSL client" deliberately uses the shared WSL
/// coordinator (production port 47631); it runs only on a Windows host with WSL. A host whose
/// ephemeral range reaches into the band is reported by the case "budget ports: ..." in
/// budget_ports_test.cpp.
constexpr std::uint16_t kBandFirst = 27600;
constexpr std::uint16_t kBandLast = 27999;

struct Range {
    std::uint16_t first = 0;
    std::uint16_t last = 0;
};

constexpr bool in_band(std::uint16_t port) {
    return port >= kBandFirst && port <= kBandLast;
}

/// Whether `port` can never be an ephemeral port of `range` (inclusive bounds).
constexpr bool outside(std::uint16_t port, Range range) {
    return port < range.first || port > range.last;
}

/// The two numbers of /proc/sys/net/ipv4/ip_local_port_range ("32768\t60999"), or nullopt
/// when the text is not two ascending port numbers.
inline std::optional<Range> parse_linux_range(const std::string& text) {
    unsigned long first = 0;
    unsigned long last = 0;
    std::size_t at = 0;
    try {
        std::size_t used = 0;
        first = std::stoul(text, &used);
        at = used;
        last = std::stoul(text.substr(at), &used);
    } catch (...) {
        return std::nullopt;
    }
    if (first == 0 || first > last || last > 65535ul) {
        return std::nullopt;
    }
    return Range{static_cast<std::uint16_t>(first), static_cast<std::uint16_t>(last)};
}

/// This host's ephemeral range for outgoing TCP connections, and where it came from. Linux and
/// macOS read the kernel's setting; Windows and other hosts return nullopt (not read).
inline std::optional<Range> host_ephemeral_range(std::string& source) {
#if defined(__linux__)
    source = "/proc/sys/net/ipv4/ip_local_port_range";
    std::ifstream in(source);
    std::string text;
    std::getline(in, text);
    return parse_linux_range(text);
#elif defined(__APPLE__)
    source = "sysctl net.inet.ip.portrange.first/last";
    int first = 0;
    int last = 0;
    std::size_t size = sizeof(first);
    if (::sysctlbyname("net.inet.ip.portrange.first", &first, &size, nullptr, 0) != 0) {
        return std::nullopt;
    }
    size = sizeof(last);
    if (::sysctlbyname("net.inet.ip.portrange.last", &last, &size, nullptr, 0) != 0) {
        return std::nullopt;
    }
    if (first <= 0 || first > last || last > 65535) {
        return std::nullopt;
    }
    return Range{static_cast<std::uint16_t>(first), static_cast<std::uint16_t>(last)};
#elif defined(_WIN32)
    // The configured range is not read here; returning the documented default 49152-65535 would
    // only compare constants. `netsh int ipv4 show dynamicport tcp` and `... show
    // excludedportrange protocol=tcp` show the host's settings.
    source = "not read on Windows";
    return std::nullopt;
#else
    source = "unknown host";
    return std::nullopt;
#endif
}

/// Not constexpr: a call in a constant expression is a compile error that names the rule.
void fixed_test_port_outside_band_see_test_ports_hpp();

/// A fixed port literal of a regression: compiles only inside the band, so a new case cannot
/// pick a port in an ephemeral range again.
consteval std::uint16_t fixed(unsigned port) {
    if (port < kBandFirst || port > kBandLast) {
        fixed_test_port_outside_band_see_test_ports_hpp();
    }
    return static_cast<std::uint16_t>(port);
}

/// The same for a port computed at run time (a base plus a case index). Fails the calling case
/// when a fixed test port lies outside the band. 0 (the OS picks a free port, for a case that
/// never needs the same port twice) is allowed.
inline void require_test_port(std::uint16_t port) {
    INFO("fixed coordinator test port " << port << " must be 0 or lie in " << kBandFirst << ".."
                                        << kBandLast << " (see test_ports.hpp)");
    REQUIRE((port == 0 || in_band(port)));
}

/// The port of the local coordinator that the file-client cases (budget_test.cpp, the portable
/// F1 cases) start through their client. Without it they use the default local port 47632
/// (coord_wire.hpp kLocalPort), which lies in Linux's ephemeral range.
constexpr std::uint16_t kFileClientPort = fixed(27632);

/// For one case: this process's clients use a local coordinator on kFileClientPort. Restores
/// the previous values of both variables afterwards.
class FileClientPort {
  public:
    FileClientPort() {
        set(0, "ANNO_TEST_BUDGET_COORD", "local");
        set(1, "ANNO_TEST_BUDGET_PORT", std::to_string(kFileClientPort));
    }
    ~FileClientPort() {
        for (int i = 0; i < 2; ++i) {
            anno::test::child::set_env(names_[i], saved_[i] ? saved_[i]->c_str() : "");
        }
    }
    FileClientPort(const FileClientPort&) = delete;
    FileClientPort& operator=(const FileClientPort&) = delete;

  private:
    void set(int index, const char* name, const std::string& value) {
        const char* old = std::getenv(name);
        names_[index] = name;
        saved_[index] = old != nullptr ? std::optional<std::string>(old) : std::nullopt;
        anno::test::child::set_env(name, value.c_str());
    }
    const char* names_[2] = {nullptr, nullptr};
    std::optional<std::string> saved_[2];
};

} // namespace anno::test::ports
