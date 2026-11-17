// Whether a background section's own recorded instance overlaps a slow finish call's own span
// (ZOOM-SMOOTH-3d, the review of 3c's own F1), without a window or a real background thread, so
// tests can run it.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace anno::app {

/// Mirrors anno::render::gpu::kSectionRingSize (gpu_fluid.hpp) - a separate copy, not a shared
/// include, so this header keeps no render-layer dependency and compiles for every viewer build,
/// GPU or not (FrameCounts, viewer.cpp, is declared unconditionally). viewer.cpp's own GPU-only
/// construction site static_asserts the two stay equal.
inline constexpr std::size_t kSectionRingSize = 4;

/// The last kSectionRingSize instances of one background section - the app-layer copy of
/// anno::render::gpu::SectionRing::Snapshot's own shape (same reason as kSectionRingSize above).
struct SectionSnapshot {
    std::array<std::int64_t, kSectionRingSize> begin_ns{};
    std::array<std::int64_t, kSectionRingSize> end_ns{};
};

/// Whether a background section's own instance (`begin_ns`/`end_ns`, steady_clock ticks since
/// the epoch; `end_ns == 0` means it has not ended as of `read_time_ns` - still running) overlaps
/// a finish call's own span `[call_begin_ns, call_begin_ns + call_ms]`, and by how many
/// milliseconds. A still-running instance is read as running until `read_time_ns`, not as ended
/// at time 0 - the fix for 3c's review F1 ("a section still running has a new begin and the old
/// end, and overlap_ms then reports no overlap"). `begin_ns == 0` means the slot has never been
/// used. Pure: no clock read inside, so a test drives every case with synthetic timestamps; the
/// caller reads `read_time_ns` itself, after the finish call has returned (ZOOM-SMOOTH-3d item 1)
/// - 3c's own bug was reading the sections' stamps long before that point, not a bug in this
/// formula, but overlap_ms_old below reproduces the shape of its mistake (no read_time_ns, a
/// still-running instance's end taken as literally 0) for the regression test.
[[nodiscard]] inline std::optional<double> overlap_ms(std::int64_t call_begin_ns, double call_ms,
                                                      std::int64_t begin_ns, std::int64_t end_ns,
                                                      std::int64_t read_time_ns) {
    if (begin_ns == 0) {
        return std::nullopt;
    }
    const std::int64_t effective_end_ns = end_ns != 0 ? end_ns : read_time_ns;
    const auto call_end_ns = call_begin_ns + static_cast<std::int64_t>(call_ms * 1.0e6);
    const auto lo = std::max(call_begin_ns, begin_ns);
    const auto hi = std::min(call_end_ns, effective_end_ns);
    if (hi <= lo) {
        return std::nullopt;
    }
    return static_cast<double>(hi - lo) / 1.0e6;
}

/// The shape of 3c's own mistake (not what 3c literally computed, which read stale stamps from
/// before the call entirely - this reproduces its *formula's* blind spot for the regression test
/// below): no `read_time_ns`, so a still-running instance's `end_ns == 0` is taken literally,
/// never overlapping anything (`hi = min(call_end_ns, 0)` is never greater than a positive `lo`).
[[nodiscard]] inline std::optional<double> overlap_ms_old(std::int64_t call_begin_ns,
                                                          double call_ms, std::int64_t begin_ns,
                                                          std::int64_t end_ns) {
    if (begin_ns == 0) {
        return std::nullopt;
    }
    const auto call_end_ns = call_begin_ns + static_cast<std::int64_t>(call_ms * 1.0e6);
    const auto lo = std::max(call_begin_ns, begin_ns);
    const auto hi = std::min(call_end_ns, end_ns);
    if (hi <= lo) {
        return std::nullopt;
    }
    return static_cast<double>(hi - lo) / 1.0e6;
}

} // namespace anno::app
