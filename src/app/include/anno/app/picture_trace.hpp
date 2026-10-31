// Raw events of the picture's CPU pipeline and their aggregation (round dy, C2/C4;
// docs/perf/README.md, "Raw events"). The bench records events while it runs (the bridge's trace
// seam, the frames it renders, the reclaimer's destructions) and saves them; the summary is
// computed from the saved events, so every aggregate is reproducible from the raw file.
//
// Definitions (CPU stages only; presented-frame latency is the viewer's and the GPU's, later). The
// four measured boundaries (round dz, 2a):
//  - desired state received: a Desired event, island i's desired version became v at time t0 (an
//    edit);
//  - object generation committed: the first Committed event of island i with a version >= v (time
//    t1, object-ready). The edit's objects are in that generation's lists. An edit committed
//    through a later version is counted as coalesced, and its latency runs to that commit: a
//    superseded version is never dropped from the latency accounting;
//  - affected visible chunks simultaneously fresh: ChunksReady(island i, generation g) at t2, where
//    g is the generation of that commit. Its keys are the keys island i's own change invalidated
//    in that commit, within the view; at t2 every one of them is cached and not stale at the same
//    moment (ReadinessTracker). A generation that commits several islands is measured per island:
//    one island's readiness is never another's latency. An island whose keys in that commit miss
//    the view (NoVisibleChange(i, g)) makes its edits offscreen, with no visible latency;
//  - eventual convergence: Converged, the first time the picture has nothing pending (every
//    island's desired version committed) and no visible readiness is outstanding;
//  - an edit that no commit reaches is unfinished; its age runs to the end of the trace;
//  - reclamation delay: from a generation's Retired event to its Reclaimed event;
//  - queue delay and build time: Admitted to Started, and Started to the build's end (Built,
//    Superseded or Failed), per island and version.
// A trace is complete only when its last event is RunEnd, which carries the run's exit status (0:
// the run finished and its oracle passed). A trace without it (the run stopped, or the file was
// cut) is incomplete: it can still be summarized for inspection, but is never a successful run. An
// idle run is complete with no edit.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/render/frame.hpp>

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
    /// A commit made generation `version` (after its Committed events, same time; island 0).
    Generation,
    /// Every visible key island `island`'s change invalidated in generation `version` is cached
    /// and not stale, all at once.
    ChunksReady,
    /// Island `island`'s change in generation `version` invalidated no key of the view.
    NoVisibleChange,
    /// Generation `version` stopped being the picture's current generation.
    Retired,
    /// Generation `version`'s destruction began on the reclaimer.
    Reclaimed,
    /// The measured loop ended and the drain began; the drain ended (converged or timed out).
    DrainStart,
    DrainEnd,
    /// The picture converged: nothing pending and no visible readiness outstanding.
    Converged,
    /// The run ended; `version` is its exit status (0: finished and verified). The last event of a
    /// complete trace.
    RunEnd,
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
    /// Committed events of a version below the island's desired version at that moment: the
    /// intermediate publications of the Monotonic policy (round dz).
    std::size_t intermediate_commits = 0;
    Distribution object_ms, visible_ms, reclaim_ms;
    /// Per build (an island and version with both events): the queue delay (Admitted to Started)
    /// and the build time on a worker (Started to Built, Superseded or Failed).
    Distribution queue_ms, build_ms;
    /// The largest age of an edit: its object-ready latency, or for an unfinished edit the time
    /// from its Desired event to the last event of the trace.
    double max_age_ms = 0;
    /// The time of the last Committed event (-1 when there is none), of the first Converged after
    /// the last Desired (-1 when the picture never converged after its last edit), and of the last
    /// event.
    double last_commit_ms = -1, converged_ms = -1, end_ms = 0;
    /// Whether the trace ends with RunEnd, and the status it carries (meaningful when complete).
    bool complete = false;
    std::uint64_t run_status = 0;
    /// Complete, and the run's status is 0.
    [[nodiscard]] bool succeeded() const { return complete && run_status == 0; }
};
[[nodiscard]] TraceSummary summarize(std::span<const TraceEvent> events);

/// The readiness of the visible chunks one island's change invalidated (round dz, 2b). A set is
/// ready only when every one of its keys is fresh at the same poll: a key seen fresh once and made
/// stale again by a later change keeps the set waiting. Window thread; the bench polls it once per
/// frame with the frame renderer's freshness.
class ReadinessTracker {
  public:
    struct Set {
        std::uint32_t island = 0;
        std::uint64_t generation = 0;
        std::vector<render::ChunkKey> keys;
    };
    /// A key's observed change of state, for diagnosing re-invalidation: the set it belongs to,
    /// the key, and whether it was fresh from this poll on.
    struct KeyEvent {
        double t_ms = 0;
        std::uint32_t island = 0;
        std::uint64_t generation = 0;
        render::ChunkKey key{};
        bool fresh = false;
    };
    /// Waits for `set` (its keys are already the ones within the view; a set with none is not
    /// added: the caller records NoVisibleChange).
    void add(Set set);
    /// Checks every waiting set against `fresh` now. Returns the sets whose keys are all fresh now
    /// (removed from the waiting ones); appends each key's change of state to `log`.
    std::vector<Set> poll(const std::function<bool(const render::ChunkKey&)>& fresh, double t_ms,
                          std::vector<KeyEvent>& log);
    [[nodiscard]] std::size_t waiting() const { return sets_.size(); }

  private:
    struct Waiting {
        Set set;
        /// The state of each key at the previous poll (logged on change).
        std::vector<bool> fresh;
        bool polled = false;
    };
    std::vector<Waiting> sets_;
};

/// The header line of a saved trace (without its newline).
inline constexpr std::string_view kTraceHeader = "t_ms\tkind\tisland\tversion";
/// One event per line: `t_ms<TAB>kind<TAB>island<TAB>version`, after the header line.
[[nodiscard]] std::string to_tsv(std::span<const TraceEvent> events);
/// Reads a saved trace. Refuses (naming the line): an empty text, a missing or different header,
/// a line without exactly four fields, a number with trailing characters, a time that is not a
/// finite non-negative number, an island beyond 32 bits or a version beyond 64 bits, an unknown
/// kind, an event after RunEnd. A blank line is allowed only as the last line.
[[nodiscard]] formats::Result<std::vector<TraceEvent>, std::string>
parse_tsv(std::string_view text);
/// The summary as `key=value` lines (sample counts beside every percentile), starting with the
/// run's completeness and status.
[[nodiscard]] std::string format_summary(const TraceSummary& s);

/// A whole file's text, or why it could not be read (`error` names the path).
struct TextFile {
    bool ok = false;
    std::string text, error;
};
[[nodiscard]] TextFile read_text(const std::filesystem::path& path);
/// Writes `text` to `path` (replacing it), checking the open, the write, the flush and the close;
/// returns an empty string on success, else a message that names the path and the step.
[[nodiscard]] std::string save_text(const std::filesystem::path& path, std::string_view text);
/// Writes `text` to "<path>.partial" (save_text) and renames it to `path`, so `path` is either the
/// whole text or what it was before; on failure the partial file is removed. Returns as save_text.
[[nodiscard]] std::string save_whole(const std::filesystem::path& path, std::string_view text);

/// One file of a run's evidence: its name in the run's directory and its text.
struct EvidenceFile {
    std::string name, text;
};
/// What finalize_evidence saved.
struct EvidenceOutcome {
    /// The status the trace's RunEnd carries: the run's verdict when every file was saved, else 5.
    int status = 0;
    /// Why each file that is not in the directory was not saved (empty: all were).
    std::vector<std::string> unsaved;
    /// events.tsv, the completion record, was saved.
    bool trace_saved = false;
};
/// The exit status of a run whose evidence could not be saved (bench_picture's 5).
inline constexpr int kEvidenceNotSaved = 5;
/// Saves a run's evidence in `dir` so that the trace is its completion record (round ea, 3c/3d).
/// Every file of `files` first, then summary.txt (`summary` of the trace as it will end), then
/// events.tsv: `events` with RunEnd at `end_ms` appended, carrying `verdict` only when everything
/// before it was saved and kEvidenceNotSaved otherwise. Each file is written whole (save_whole).
/// When events.tsv itself cannot be saved there is no trace (an aggregate of it fails as missing)
/// and summary.txt, which describes it, is removed. A successful RunEnd therefore exists only when
/// every file of the run is in `dir`.
[[nodiscard]] EvidenceOutcome
finalize_evidence(const std::filesystem::path& dir, std::span<const EvidenceFile> files,
                  std::vector<TraceEvent> events, double end_ms, int verdict,
                  const std::function<std::string(const TraceSummary&)>& summary);

} // namespace anno::app
