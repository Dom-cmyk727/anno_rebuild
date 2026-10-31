// Raw events of the picture's CPU pipeline and their aggregation (round dy, C2/C4;
// docs/perf/README.md, "Raw events"). The bench records events while it runs (the bridge's trace
// seam, the frames it renders, the reclaimer's destructions) and saves them; the summary is
// computed from the saved events, so every aggregate is reproducible from the raw file.
//
// Definitions (CPU stages only; presented-frame latency is the viewer's and the GPU's, later):
//  - an edit is a Desired event: island i's desired version became v at time t0;
//  - object-ready: the first Committed event of island i with a version >= v (time t1). The edit's
//    objects are in that generation's lists. An edit committed through a later version is
//    counted as coalesced, and its latency runs to that commit: a superseded version is never
//    dropped from the latency accounting;
//  - visible CPU chunks ready: the first ChunksReady of a generation >= the commit's generation at
//    or after t1 (time t2): every chunk key of the visible view that the commit invalidated is
//    cached again and not stale. A commit whose invalidated keys miss the view (NoVisibleChange)
//    makes its edits offscreen, with no visible latency;
//  - an edit that no commit reaches is unfinished; its age runs to the end of the trace;
//  - reclamation delay: from a generation's Retired event to its Reclaimed event.
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/result.hpp>

namespace anno::app {

enum class TraceKind : std::uint8_t {
    // The bridge's steps (PictureEvent), with the island index and the version.
    Desired,
    Admitted,
    Deferred,
    Started,
    Built,
    Failed,
    Superseded,
    Dropped,
    Requeued,
    Committed,
    // The bench's events, with a generation in `version` (island 0).
    /// A commit made generation `version` (after its Committed events, same time).
    Generation,
    /// Every visible key the commit of generation `version` invalidated is ready.
    ChunksReady,
    /// The commit of generation `version` invalidated no key of the view.
    NoVisibleChange,
    /// Generation `version` stopped being the picture's current generation.
    Retired,
    /// Generation `version`'s destruction began on the reclaimer.
    Reclaimed,
    /// The measured loop ended and the drain began; the drain ended (converged or timed out).
    DrainStart,
    DrainEnd,
};

[[nodiscard]] std::string_view trace_kind_name(TraceKind k);

struct TraceEvent {
    double t_ms = 0;
    TraceKind kind = TraceKind::Desired;
    std::uint32_t island = 0;
    std::uint64_t version = 0;
};

/// A sample's size and nearest-rank percentiles. p95 needs 20 samples and p99 100 to be reported
/// (`p95_valid`, `p99_valid`); below that only the median and the maximum are meaningful.
struct Distribution {
    std::size_t n = 0;
    double p50 = 0, p95 = 0, p99 = 0, max = 0;
    bool p95_valid = false, p99_valid = false;
};
[[nodiscard]] Distribution distribution(std::vector<double> samples);

struct TraceSummary {
    std::size_t edits = 0, object_ready = 0, visible_ready = 0, offscreen = 0, coalesced = 0,
                unfinished = 0;
    std::size_t admitted = 0, deferred = 0, built = 0, failed = 0, superseded = 0, dropped = 0,
                requeued = 0, commits = 0, generations = 0, reclaimed = 0;
    Distribution object_ms, visible_ms, reclaim_ms;
    /// The largest age of an edit: its object-ready latency, or for an unfinished edit the time
    /// from its Desired event to the last event of the trace.
    double max_age_ms = 0;
    /// The time of the last Committed event (-1 when there is none), and of the last event.
    double last_commit_ms = -1, end_ms = 0;
};
[[nodiscard]] TraceSummary summarize(std::span<const TraceEvent> events);

/// One event per line: `t_ms<TAB>kind<TAB>island<TAB>version`, with a header line.
[[nodiscard]] std::string to_tsv(std::span<const TraceEvent> events);
[[nodiscard]] formats::Result<std::vector<TraceEvent>, std::string>
parse_tsv(std::string_view text);
/// The summary as `key=value` lines (sample counts beside every percentile).
[[nodiscard]] std::string format_summary(const TraceSummary& s);

} // namespace anno::app
