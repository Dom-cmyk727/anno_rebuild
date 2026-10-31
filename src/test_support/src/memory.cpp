// Free memory, for the thread bound in anno/test/parallel.hpp.
#include <algorithm>

#include "anno/test/budget.hpp"
#include "anno/test/parallel.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// psapi.h uses the Windows types. clang-format would sort these
// alphabetically and put psapi.h first, which does not compile.
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#elif defined(__linux__)
#include <cstdio>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <unistd.h>
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
#elif defined(__APPLE__)
    // macOS has no /proc/meminfo. What can be had without paging anything out:
    // the free pages (free_count leaves out the speculative ones), the
    // speculative pages and the inactive ones (the "available" of the memory
    // pressure tools, an estimate like MemAvailable).
    const long page = ::sysconf(_SC_PAGESIZE);
    if (page <= 0) {
        return std::nullopt;
    }
    static const mach_port_t host = ::mach_host_self();
    vm_statistics64_data_t vm{};
    // HOST_VM_INFO64_COUNT, spelled out: the macro is a C-style cast.
    mach_msg_type_number_t count =
        static_cast<mach_msg_type_number_t>(sizeof(vm) / sizeof(integer_t));
    if (::host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) !=
        KERN_SUCCESS) {
        return std::nullopt;
    }
    const std::uint64_t pages = static_cast<std::uint64_t>(vm.free_count) +
                                static_cast<std::uint64_t>(vm.speculative_count) +
                                static_cast<std::uint64_t>(vm.inactive_count);
    return pages * static_cast<std::uint64_t>(page);
#else
    return std::nullopt; // unknown: no memory bound
#endif
}

std::optional<std::uint64_t> total_memory_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status) == 0) {
        return std::nullopt;
    }
    return std::min<std::uint64_t>(status.ullTotalPhys, status.ullTotalPageFile);
#elif defined(__linux__)
    std::FILE* f = std::fopen("/proc/meminfo", "r");
    if (f == nullptr) {
        return std::nullopt;
    }
    char line[256];
    std::optional<std::uint64_t> kib;
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        unsigned long long v = 0;
        if (std::sscanf(line, "MemTotal: %llu kB", &v) == 1) {
            kib = v;
            break;
        }
    }
    std::fclose(f);
    if (!kib) {
        return std::nullopt;
    }
    return *kib * 1024;
#elif defined(__APPLE__)
    std::uint64_t bytes = 0;
    std::size_t length = sizeof(bytes);
    if (::sysctlbyname("hw.memsize", &bytes, &length, nullptr, 0) != 0 || length != sizeof(bytes)) {
        return std::nullopt;
    }
    return bytes;
#else
    return std::nullopt;
#endif
}

std::optional<std::uint64_t> process_private_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)) == 0) {
        return std::nullopt;
    }
    return counters.PrivateUsage;
#elif defined(__linux__)
    std::FILE* file = std::fopen("/proc/self/status", "r");
    if (file == nullptr) {
        return std::nullopt;
    }
    char line[256];
    std::optional<std::uint64_t> anon;
    std::optional<std::uint64_t> rss;
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        unsigned long long value = 0;
        if (std::sscanf(line, "RssAnon: %llu kB", &value) == 1) {
            anon = value * 1024;
        } else if (std::sscanf(line, "VmRSS: %llu kB", &value) == 1) {
            rss = value * 1024;
        }
    }
    std::fclose(file);
    if (anon) {
        return anon;
    }
    return rss;
#else
    return std::nullopt;
#endif
}

} // namespace anno::test
