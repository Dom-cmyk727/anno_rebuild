// anno_peak_alloc <mebibytes> [exit-code|abort [echo-args...]]
//
// Helper of tools/test/peak_launcher_test.py: touches <mebibytes> MiB (every byte written, so
// the pages are resident), prints "allocated <n> MiB ... build asan|plain", every further argument
// ("arg <i>: <text>", to check that the launcher passes arguments unchanged) and one fixed line to
// stdout and one to stderr, then exits with the given code (default 0) or aborts.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace {

/// AddressSanitizer maps one shadow byte per 8 application bytes it touches, so the resident
/// peak of an ASan build is 9/8 of the allocation: the contract expects that, not a plain one.
#if defined(__SANITIZE_ADDRESS__)
constexpr bool kAddressSanitizer = true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
constexpr bool kAddressSanitizer = true;
#else
constexpr bool kAddressSanitizer = false;
#endif
#else
constexpr bool kAddressSanitizer = false;
#endif

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: anno_peak_alloc <mebibytes> [exit-code|abort]\n");
        return 2;
    }
    const unsigned long mib = std::strtoul(argv[1], nullptr, 10);
    const std::size_t bytes = static_cast<std::size_t>(mib) * 1024u * 1024u;
    std::unique_ptr<unsigned char[]> block(new unsigned char[bytes == 0 ? 1 : bytes]);
    std::memset(block.get(), 0x5a, bytes);
    // Read it back so the writes cannot be dropped.
    unsigned long sum = 0;
    for (std::size_t i = 0; i < bytes; i += 4096) {
        sum += block[i];
    }
    std::printf("allocated %lu MiB checksum %lu build %s\n", mib, sum,
                kAddressSanitizer ? "asan" : "plain");
    for (int i = 3; i < argc; ++i) {
        std::printf("arg %d: [%s]\n", i, argv[i]);
    }
    std::printf("stdout line with spaces and \"quotes\"\n");
    std::fflush(stdout);
    std::fprintf(stderr, "stderr line\n");
    std::fflush(stderr);
    if (argc >= 3 && std::string(argv[2]) == "abort") {
#if defined(_WIN32)
        // No "abort() has been called" dialog or crash report: the test would hang on it.
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        std::abort();
    }
    return argc >= 3 ? std::atoi(argv[2]) : 0;
}
