// The fixed ports of the budget-coordinator regressions lie outside every ephemeral port range a
// host hands out to outgoing connections (test_ports.hpp). CI run 41985663081 failed with
// BIND-FAILED 47814 because Linux's default range 32768-60999 contained the F3 ports.
#include <cstdint>
#include <string>

#include <doctest/doctest.h>

#include "test_ports.hpp"

namespace {

namespace ports = anno::test::ports;

/// Whether no port of the band can be an ephemeral port of `range`.
constexpr bool band_outside(ports::Range range) {
    return range.last < ports::kBandFirst || range.first > ports::kBandLast;
}

constexpr ports::Range kLinuxDefault{32768, 60999}; // Linux ip_local_port_range default
constexpr ports::Range kIanaDefault{49152, 65535};  // Windows (Vista and later) and macOS default

} // namespace

TEST_CASE("budget ports: the fixed band lies below every default ephemeral range" *
          doctest::test_suite("budget")) {
    // The band itself.
    CHECK_FALSE(ports::in_band(27599));
    CHECK(ports::in_band(27600));
    CHECK(ports::in_band(27999));
    CHECK_FALSE(ports::in_band(28000));
    // Inclusive bounds of a range, at both edges.
    CHECK(ports::outside(32767, kLinuxDefault));
    CHECK_FALSE(ports::outside(32768, kLinuxDefault));
    CHECK_FALSE(ports::outside(60999, kLinuxDefault));
    CHECK(ports::outside(61000, kLinuxDefault));
    // Negative control: the ports used before (F3 47810-47817, the shared 47631) are inside
    // Linux's default range, so the check can tell the failing layout from the fixed one.
    CHECK_FALSE(ports::outside(47814, kLinuxDefault));
    CHECK_FALSE(ports::outside(47631, kLinuxDefault));
    CHECK_FALSE(ports::outside(47600, kLinuxDefault)); // the old band's edges
    CHECK_FALSE(ports::outside(47999, kLinuxDefault));
    // The band against the three documented defaults.
    CHECK(band_outside(kLinuxDefault));
    CHECK(band_outside(kIanaDefault));
    // A host configured down to 1024 would reach the band; the host case reports it.
    CHECK_FALSE(band_outside(ports::Range{1024, 65535}));
}

TEST_CASE("budget ports: the Linux range text is read as two ascending ports" *
          doctest::test_suite("budget")) {
    const auto standard = ports::parse_linux_range("32768\t60999\n");
    REQUIRE(standard);
    CHECK(standard->first == 32768);
    CHECK(standard->last == 60999);
    const auto spaces = ports::parse_linux_range("1024 65535");
    REQUIRE(spaces);
    CHECK(spaces->first == 1024);
    CHECK(spaces->last == 65535);
    CHECK_FALSE(ports::parse_linux_range(""));
    CHECK_FALSE(ports::parse_linux_range("abc"));
    CHECK_FALSE(ports::parse_linux_range("32768"));
    CHECK_FALSE(ports::parse_linux_range("60999 32768"));
    CHECK_FALSE(ports::parse_linux_range("1024 70000"));
    CHECK_FALSE(ports::parse_linux_range("0 60999"));
}

TEST_CASE("budget ports: this host's ephemeral range does not reach the fixed test ports" *
          doctest::test_suite("budget")) {
    std::string source;
    const auto range = ports::host_ephemeral_range(source);
    INFO("ephemeral range source: " << source);
#if defined(__linux__) || defined(__APPLE__)
    REQUIRE_MESSAGE(range.has_value(), "the host's ephemeral port range could not be read");
#endif
    if (!range) {
        MESSAGE("no ephemeral range known for this host; nothing to compare");
        return;
    }
    INFO("ephemeral range " << range->first << ".." << range->last << ", fixed test ports "
                            << ports::kBandFirst << ".." << ports::kBandLast);
    CHECK_MESSAGE(band_outside(*range),
                  "this host hands out the coordinator regressions' fixed ports to outgoing "
                  "connections: their coordinators can fail with BIND-FAILED under ctest -j. "
                  "Move the band in test_ports.hpp or narrow the host's range.");
}
