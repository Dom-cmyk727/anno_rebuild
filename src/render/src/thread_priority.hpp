#pragma once

// Background threads (chunk builders, the reaper, river flow builders, the wave
// simulation setup, texture pre-creation) yield to the window thread.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#endif

namespace anno::render {

/// Lowers the priority of the calling thread only (a nice value is per thread
/// on Linux but per process on macOS, the development host, where the QoS
/// class is the per-thread equivalent). Without it the chunk builders (half the
/// cores) and the window thread have the same priority, and the window thread
/// was descheduled for 8-10 ms in single frames (in steps that are a few
/// microseconds of map lookups otherwise). ANNO_BACKGROUND_PRIORITY=normal
/// leaves the priority alone, for measuring the difference.
inline void lower_thread_priority() {
    const char* env = std::getenv("ANNO_BACKGROUND_PRIORITY");
    if (env != nullptr && std::strcmp(env, "normal") == 0) {
        return;
    }
#if defined(_WIN32)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#elif defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#elif defined(__linux__)
    // The kernel's thread id is a task id, which setpriority takes.
    const int rc = setpriority(PRIO_PROCESS, static_cast<id_t>(::syscall(SYS_gettid)), 5);
    static_cast<void>(rc);
#endif
}

#if defined(_WIN32)
/// Thread CPU cycles the calling thread has consumed so far (QueryThreadCycleTime), not a time
/// (ZOOM-SMOOTH-3c F2, replacing GetThreadTimes: its ~15.6 ms system-clock quantum made a direct
/// millisecond reading meaningless for a call a few milliseconds long, ZOOM-SMOOTH-3b's own
/// finding - and there is no reliable API to convert cycles to time). No conversion is needed:
/// compare a slow call's own cycles against this run's own cycles-per-millisecond yardstick,
/// computed from the ordinary calls of the same run (the caller's job, not this function's) -
/// far fewer cycles than the yardstick for the call's own wall time means it was waiting (a lock,
/// descheduled, another thread's deferred wgpu work), not computing. 0 if the platform call
/// fails.
inline std::uint64_t thread_cpu_cycles() {
    ULONG64 cycles = 0;
    if (QueryThreadCycleTime(GetCurrentThread(), &cycles) == 0) {
        return 0;
    }
    return cycles;
}
#else
/// CPU time (ms) the calling thread has consumed so far, not wall time: comparing this against a
/// wall-clock duration around the same call tells "computing" (CPU close to wall) apart from
/// "waiting on a lock, descheduled, or paying for another thread's deferred destruction" (CPU
/// well under wall). -1 if the platform call fails. Windows uses thread_cpu_cycles() instead
/// (ZOOM-SMOOTH-3c F2) - clock_gettime's own resolution is fine where it is available.
inline double thread_cpu_time_ms() {
    struct timespec ts{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return -1.0;
    }
    return static_cast<double>(ts.tv_sec) * 1000.0 + static_cast<double>(ts.tv_nsec) / 1.0e6;
}
#endif

} // namespace anno::render
