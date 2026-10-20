#pragma once

// Background threads (chunk builders, the reaper, river flow builders, the wave
// simulation setup, texture pre-creation) yield to the window thread.

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

} // namespace anno::render
