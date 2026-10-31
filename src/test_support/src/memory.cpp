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
#include <sys/types.h>
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
    // The analogue of MemAvailable: pages that can be taken without paging out anything that
    // is in use (free, speculative and inactive).
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm),
                          &count) != KERN_SUCCESS) {
        return std::nullopt;
    }
    vm_size_t page = 0;
    if (host_page_size(mach_host_self(), &page) != KERN_SUCCESS) {
        return std::nullopt;
    }
    const std::uint64_t pages =
        std::uint64_t{vm.free_count} + vm.speculative_count + vm.inactive_count;
    return pages * page;
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
    std::size_t size = sizeof(bytes);
    if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) != 0) {
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
#elif defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) !=
        KERN_SUCCESS) {
        return std::nullopt;
    }
    return info.phys_footprint;
#else
    return std::nullopt;
#endif
}

} // namespace anno::test
