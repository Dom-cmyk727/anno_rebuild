// The play window's opt-in trace (W4.3 round ec, B2; the contract is docs/app/window-trace.md).
// A bounded in-memory recorder shared by the sim thread and the window thread: events in the
// picture_trace format, one row per attempted GPU frame, one row per turn. Nothing is written until
// finalize(). A record past a bound is counted as dropped; a trace with drops is written without
// RunEnd (incomplete evidence). FrameTracer correlates a change with the first successful present
// call of a frame that draws it; the window's handling of a surface acquire is classify_acquire. No
// WebGPU type appears here: the statuses are the values of webgpu.h and wgpu.h, which the viewer
// checks against the header.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <anno/app/picture_trace.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>

namespace anno::app {

/// The statuses of wgpuSurfaceGetCurrentTexture (webgpu.h WGPUSurfaceGetCurrentTextureStatus;
/// wgpu.h adds Occluded) and of wgpuSurfacePresent (WGPUStatus).
inline constexpr std::uint32_t kAcquireSuccessOptimal = 1, kAcquireSuccessSuboptimal = 2,
                               kAcquireTimeout = 3, kAcquireOutdated = 4, kAcquireLost = 5,
                               kAcquireError = 6, kAcquireOccluded = 0x00030001;
inline constexpr std::uint32_t kPresentSuccess = 1, kPresentError = 2;
[[nodiscard]] std::string acquire_status_name(std::uint32_t status);

/// What the window does with an acquire's status (the viewer's handling, viewer.cpp draw_gpu).
enum class AcquireAction : std::uint8_t {
    /// A usable texture: draw and present.
    Draw,
    /// A usable texture on a suboptimal surface: draw and present, reconfigure before the next.
    DrawThenReconfigure,
    /// No texture: reconfigure and try again (outdated or lost); when more than
    /// `kAcquireBackOffAfter` consecutive attempts failed, back off; when more than
    /// `kAcquireGiveUpAfter` failed, end with an error.
    Reconfigure,
    /// No texture: look again shortly (timeout, occluded).
    BackOff,
    /// No texture, and no way to recover (error, unknown status): end with an error.
    Fatal,
};
inline constexpr int kAcquireBackOffAfter = 2, kAcquireGiveUpAfter = 50;
[[nodiscard]] AcquireAction classify_acquire(std::uint32_t status);
[[nodiscard]] inline bool acquire_gives_texture(AcquireAction a) {
    return a == AcquireAction::Draw || a == AcquireAction::DrawThenReconfigure;
}

/// One attempted GPU frame (docs/app/window-trace.md, frames.tsv). -1 for what does not apply.
struct FrameRecord {
    std::uint64_t frame = 0, epoch = 0, generation = 0;
    int view_x = 0, view_y = 0, view_w = 0, view_h = 0;
    std::size_t group = 0;
    double scale = 1.0;
    std::uint32_t drawable_w = 0, drawable_h = 0;
    double t_start_ms = -1, picture_ms = -1, acquire_start_ms = -1, acquire_end_ms = -1;
    std::uint32_t acquire_status = 0;
    bool texture = false;
    double submit_ms = -1, present_start_ms = -1, present_end_ms = -1;
    /// 0 when the present call was not made.
    std::uint32_t present_status = 0;
    /// presented, present_failed, acquire_skipped, acquire_retry, fatal.
    std::string outcome;
};

/// One turn of the runner (sim.tsv): the commands it applied, the rejections the sim reported, and
/// the batch it pushed.
struct TurnRecord {
    std::uint64_t turn = 0;
    double t_ms = 0;
    std::size_t applied = 0, rejected = 0;
    std::string reasons;
    bool batch = false;
    std::size_t deltas = 0;
};
/// accepted (no rejection), rejected (every command), ambiguous (some), none (no command), and
/// invalid when the sim reported more rejections than the turn applied commands (not a recording).
[[nodiscard]] std::string turn_outcome(const TurnRecord& t);

/// The trace kind of a bridge step (PictureLimits::trace).
[[nodiscard]] TraceKind trace_kind_of(PictureEvent e);

/// The chunk keys a frame composes: zoom group `group`'s chunks that overlap `view` (the scene
/// grid of that group the frame draws; render::visible_chunks).
[[nodiscard]] std::vector<render::ChunkKey> view_keys(render::Rect view, std::size_t group);

class WindowTrace {
  public:
    /// `max_events` and `max_frames` (> 0) bound what is kept; memory for them is reserved here.
    WindowTrace(std::size_t max_events, std::size_t max_frames, std::size_t max_turns);
    WindowTrace(const WindowTrace&) = delete;
    WindowTrace& operator=(const WindowTrace&) = delete;

    /// Milliseconds of the steady clock since the trace was made (every thread uses this clock).
    [[nodiscard]] double now_ms() const;
    /// The same clock's milliseconds at `t` (a time read on another thread, before a record).
    [[nodiscard]] double ms_at(std::chrono::steady_clock::time_point t) const;
    /// Thread-safe. A record past its bound is dropped and counted.
    void event(double t_ms, TraceKind kind, std::uint32_t island, std::uint64_t version);
    void event_now(TraceKind kind, std::uint32_t island, std::uint64_t version) {
        event(now_ms(), kind, island, version);
    }
    void frame(FrameRecord record);
    void turn(TurnRecord record);
    /// A line for manifest.txt (the adapter, the drawable size): a handful per session.
    void note(std::string line);
    [[nodiscard]] std::uint64_t dropped() const;
    /// A record that a bound outside the recorder refused (FrameTracer): counted as dropped.
    void count_drop();
    [[nodiscard]] std::vector<TraceEvent> events() const;
    [[nodiscard]] std::vector<FrameRecord> frames() const;
    [[nodiscard]] std::vector<TurnRecord> turns() const;

    /// Writes events.tsv, frames.tsv, sim.tsv, manifest.txt and summary.txt into `dir` through
    /// finalize_evidence (the events last). `scenario` is the run's own status (0: the scenario
    /// passed); with drops the recording is incomplete and no RunEnd is written. Call once, after
    /// every thread that records has stopped recording.
    EvidenceOutcome finalize(const std::filesystem::path& dir, const std::string& manifest,
                             const std::string& scenario_notes, int scenario);

  private:
    std::chrono::steady_clock::time_point t0_;
    mutable std::mutex m_;
    std::size_t max_events_, max_frames_, max_turns_;
    std::vector<TraceEvent> events_;
    std::vector<FrameRecord> frames_;
    std::vector<TurnRecord> turns_;
    std::vector<std::string> notes_;
    std::uint64_t dropped_ = 0;
};

/// The window trace's bookkeeping of attempted GPU frames (window thread;
/// docs/app/window-trace.md). The viewer calls it at each boundary of a frame; what the window does
/// with an acquire stays the viewer's (classify_acquire). Holds the commits not yet given a view,
/// the visible changes not yet presented, the frame ids and the surface epoch.
///
/// Presentation (round ed): a change is presented by the first frame whose present call succeeds
/// and which draws it. A frame draws a change when at least one key the change invalidated is in
/// that frame's own view (its rectangle and zoom group) and every such key is GPU-usable at that
/// frame's observation: readiness seen in an earlier frame does not count. A failed present, a
/// frame whose view holds none of the change's keys, and a frame in which one of them is stale or
/// not on the GPU complete nothing; the change keeps waiting, and is reported unfinished if the
/// session ends first. The frame's scene is the one taken after the boundary that committed the
/// change, so it holds that generation or a later one: a later commit of the same island over the
/// same keys is drawn instead, which the aggregate counts as coalesced.
class FrameTracer {
  public:
    using Ready = std::function<bool(const render::ChunkKey&)>;
    /// `max_outstanding` bounds the changes waiting at once (outstanding()). Reason: a hidden
    /// window commits without drawing, and a change that never becomes usable waits forever; 100
    /// 000 is far beyond the islands a session commits between two drawn frames. A commit past it
    /// is counted as a drop, so the trace is incomplete rather than unbounded.
    explicit FrameTracer(WindowTrace& trace, std::size_t max_outstanding = 100000)
        : trace_(trace), max_outstanding_(max_outstanding) {}

    /// A commit of the frame boundary: island `island` changed `keys` in `generation`.
    void committed(std::uint32_t island, std::uint64_t generation,
                   std::vector<render::ChunkKey> keys);
    /// The start of an attempt: `row` carries the view and the generation (the frame id is assigned
    /// here). `view` of `group` is this attempt's view. A commit not yet given a view is visible
    /// when one of its keys is in it, else NoVisibleChange (offscreen, never waited for).
    void begin(FrameRecord row, render::Rect view, std::size_t group);
    /// The surface was (re)configured: the epoch advances.
    void reconfigured() { ++epoch_; }
    /// The acquire of the attempt, from `start` to `end` ms, returned `status`.
    void acquired(double start, double end, std::uint32_t status);
    /// The attempt ends without a present call (`outcome`: acquire_skipped, acquire_retry or
    /// fatal): FrameFailed at the acquire's end, and its row.
    void failed(const char* outcome);
    /// The attempt was submitted at `t_submit` and its present call ran from `p0` to `p1` and
    /// returned `status`. Each waiting change's keys within this attempt's view are observed once,
    /// at `t_submit` (`fresh`: a key is CPU-ready, `usable`: GPU-usable): ChunksReady and GpuReady
    /// the first time they are all fresh, and all usable; Drawn in every frame in which they are
    /// all usable; Presented, and the change ends, when that frame's present succeeded.
    /// `picture_idle`: no batch waiting and nothing pending in the picture, for Converged.
    void presented(double t_submit, double p0, double p1, std::uint32_t status, const Ready& fresh,
                   const Ready& usable, bool picture_idle);
    [[nodiscard]] std::uint64_t epoch() const { return epoch_; }
    [[nodiscard]] std::uint64_t frames() const { return frame_; }
    /// Changes not yet presented (not given a view, or visible and not yet drawn by a successful
    /// present).
    [[nodiscard]] std::size_t outstanding() const { return unwatched_.size() + visible_.size(); }

  private:
    struct Watch {
        std::uint32_t island = 0;
        std::uint64_t generation = 0;
        /// Every key the change invalidated, in every zoom group: each frame takes those in its
        /// own view.
        std::vector<render::ChunkKey> keys;
        bool cpu_ready = false, gpu_ready = false;
    };
    WindowTrace& trace_;
    std::size_t max_outstanding_;
    std::vector<Watch> unwatched_, visible_;
    /// This attempt's view keys (begin), for the observation after its render.
    std::set<render::ChunkKey> in_view_;
    std::uint64_t frame_ = 0, epoch_ = 1;
    bool converged_ = false;
    FrameRecord row_;
};

} // namespace anno::app
