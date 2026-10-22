// Free memory, for the thread bound in anno/test/parallel.hpp.
#include <algorithm>

#include "anno/test/parallel.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cstdio>
#endif

namespace anno::test {

std::optional<std::uint64_t> free_memory_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) == 0) {
        return std::nullopt;
    }
    // bad_alloc is raised when the commit limit is reached, which free RAM does
    // not show (the pagefile counts); take the smaller headroom. 0 stays 0: it
    // is exactly the state in which the next allocation fails.
    return std::min<std::uint64_t>(status.ullAvailPhys, status.ullAvailPageFile);
#elif defined(__linux__)
    std::FILE* f = std::fopen("/proc/meminfo", "r");
    if (f == nullptr) {
        return std::nullopt;
    }
    char line[256];
    std::optional<std::uint64_t> kib;
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        unsigned long long v = 0;
        if (std::sscanf(line, "MemAvailable: %llu kB", &v) == 1) {
            kib = v;
            break;
        }
    }
    std::fclose(f);
    if (!kib) {
        return std::nullopt; // kernels before 3.14 have no MemAvailable
    }
    return *kib * 1024;
#else
    return std::nullopt; // unknown: no memory bound
#endif
}

} // namespace anno::test
