// The play window's opt-in trace (see the header and docs/app/window-trace.md).
#include <cstdio>
#include <set>
#include <sstream>

#include <anno/app/window_trace.hpp>

namespace anno::app {

std::string acquire_status_name(std::uint32_t status) {
    switch (status) {
    case kAcquireSuccessOptimal:
        return "SuccessOptimal";
    case kAcquireSuccessSuboptimal:
        return "SuccessSuboptimal";
    case kAcquireTimeout:
        return "Timeout";
    case kAcquireOutdated:
        return "Outdated";
    case kAcquireLost:
        return "Lost";
    case kAcquireError:
        return "Error";
    case kAcquireOccluded:
        return "Occluded";
    default:
        return "unknown(" + std::to_string(status) + ")";
    }
}

AcquireAction classify_acquire(std::uint32_t status) {
    switch (status) {
    case kAcquireSuccessOptimal:
        return AcquireAction::Draw;
    case kAcquireSuccessSuboptimal:
        return AcquireAction::DrawThenReconfigure;
    case kAcquireOutdated:
    case kAcquireLost:
        return AcquireAction::Reconfigure;
    case kAcquireTimeout:
    case kAcquireOccluded:
        return AcquireAction::BackOff;
    default:
        return AcquireAction::Fatal;
    }
}

std::string turn_outcome(const TurnRecord& t) {
    if (t.applied == 0) {
        return "none";
    }
    if (t.rejected == 0) {
        return "accepted";
    }
    return t.rejected >= t.applied ? "rejected" : "ambiguous";
}

TraceKind trace_kind_of(PictureEvent e) {
    switch (e) {
    case PictureEvent::Desired:
        return TraceKind::Desired;
    case PictureEvent::Admitted:
        return TraceKind::Admitted;
    case PictureEvent::Deferred:
        return TraceKind::Deferred;
    case PictureEvent::Started:
        return TraceKind::Started;
    case PictureEvent::Built:
        return TraceKind::Built;
    case PictureEvent::Failed:
        return TraceKind::Failed;
    case PictureEvent::Superseded:
        return TraceKind::Superseded;
    case PictureEvent::Dropped:
        return TraceKind::Dropped;
    case PictureEvent::Requeued:
        return TraceKind::Requeued;
    case PictureEvent::Committed:
        return TraceKind::Committed;
    }
    return TraceKind::Desired;
}

std::vector<render::ChunkKey> view_keys(render::Rect view, std::size_t group) {
    std::vector<render::ChunkKey> keys;
    for (const auto& [cx, cy] : render::visible_chunks(view)) {
        keys.emplace_back(group, cx, cy);
    }
    return keys;
}

WindowTrace::WindowTrace(std::size_t max_events, std::size_t max_frames, std::size_t max_turns)
    : t0_(std::chrono::steady_clock::now()), max_events_(max_events), max_frames_(max_frames),
      max_turns_(max_turns) {
    events_.reserve(max_events_);
    frames_.reserve(max_frames_);
    turns_.reserve(max_turns_);
}

double WindowTrace::now_ms() const {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0_)
        .count();
}

double WindowTrace::ms_at(std::chrono::steady_clock::time_point t) const {
    return std::chrono::duration<double, std::milli>(t - t0_).count();
}

void WindowTrace::event(double t_ms, TraceKind kind, std::uint32_t island, std::uint64_t version) {
    const std::lock_guard lock(m_);
    if (events_.size() >= max_events_) {
        ++dropped_;
        return;
    }
    events_.push_back({t_ms, kind, island, version});
}

void WindowTrace::frame(FrameRecord record) {
    const std::lock_guard lock(m_);
    if (frames_.size() >= max_frames_) {
        ++dropped_;
        return;
    }
    frames_.push_back(std::move(record));
}

void WindowTrace::turn(TurnRecord record) {
    const std::lock_guard lock(m_);
    if (turns_.size() >= max_turns_) {
        ++dropped_;
        return;
    }
    turns_.push_back(std::move(record));
}

void WindowTrace::note(std::string line) {
    const std::lock_guard lock(m_);
    notes_.push_back(std::move(line));
}

std::uint64_t WindowTrace::dropped() const {
    const std::lock_guard lock(m_);
    return dropped_;
}

std::vector<TraceEvent> WindowTrace::events() const {
    const std::lock_guard lock(m_);
    return events_;
}

std::vector<FrameRecord> WindowTrace::frames() const {
    const std::lock_guard lock(m_);
    return frames_;
}

std::vector<TurnRecord> WindowTrace::turns() const {
    const std::lock_guard lock(m_);
    return turns_;
}

namespace {

std::string ms(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.6f", v);
    return buf;
}

} // namespace

EvidenceOutcome WindowTrace::finalize(const std::filesystem::path& dir, const std::string& manifest,
                                      const std::string& scenario_notes, int scenario) {
    std::vector<TraceEvent> events;
    std::vector<FrameRecord> frames;
    std::vector<TurnRecord> turns;
    std::vector<std::string> notes;
    std::uint64_t dropped = 0;
    {
        const std::lock_guard lock(m_);
        notes = notes_;
        events = events_;
        frames = frames_;
        turns = turns_;
        dropped = dropped_;
    }
    std::ostringstream f;
    f << "frame\tepoch\tgeneration\tview_x\tview_y\tview_w\tview_h\tgroup\tscale\tdrawable_w"
         "\tdrawable_h\tt_start_ms\tpicture_ms\tacquire_start_ms\tacquire_end_ms\tacquire_status"
         "\ttexture\tsubmit_ms\tpresent_start_ms\tpresent_end_ms\tpresent_status\toutcome\n";
    for (const FrameRecord& r : frames) {
        f << r.frame << "\t" << r.epoch << "\t" << r.generation << "\t" << r.view_x << "\t"
          << r.view_y << "\t" << r.view_w << "\t" << r.view_h << "\t" << r.group << "\t" << r.scale
          << "\t" << r.drawable_w << "\t" << r.drawable_h << "\t" << ms(r.t_start_ms) << "\t"
          << ms(r.picture_ms) << "\t" << ms(r.acquire_start_ms) << "\t" << ms(r.acquire_end_ms)
          << "\t" << acquire_status_name(r.acquire_status) << "\t" << (r.texture ? 1 : 0) << "\t"
          << ms(r.submit_ms) << "\t" << ms(r.present_start_ms) << "\t" << ms(r.present_end_ms)
          << "\t"
          << (r.present_status == 0 ? std::string("-")
              : r.present_status == kPresentSuccess
                  ? std::string("Success")
                  : "Error(" + std::to_string(r.present_status) + ")")
          << "\t" << r.outcome << "\n";
    }
    std::ostringstream t;
    t << "turn\tt_ms\tapplied\trejected\toutcome\treasons\tbatch\tdeltas\n";
    for (const TurnRecord& r : turns) {
        t << r.turn << "\t" << ms(r.t_ms) << "\t" << r.applied << "\t" << r.rejected << "\t"
          << turn_outcome(r) << "\t" << (r.reasons.empty() ? std::string("-") : r.reasons) << "\t"
          << (r.batch ? 1 : 0) << "\t" << r.deltas << "\n";
    }
    std::ostringstream m;
    m << manifest;
    for (const std::string& n : notes) {
        m << n << "\n";
    }
    m << "trace events=" << events.size() << " frames=" << frames.size()
      << " turns=" << turns.size() << " dropped=" << dropped << " bounds=" << max_events_ << "/"
      << max_frames_ << "/" << max_turns_ << "\n";
    const std::vector<EvidenceFile> files{
        {"frames.tsv", f.str()}, {"sim.tsv", t.str()}, {"manifest.txt", m.str()}};
    // A trace that dropped records is incomplete: no RunEnd (finalize_evidence, verdict < 0).
    const int verdict = dropped > 0 ? -1 : scenario;
    const double end = now_ms();
    return finalize_evidence(
        dir, files, std::move(events), end, verdict, [&](const TraceSummary& s) {
            std::string text = format_summary(s);
            if (dropped > 0) {
                text += "recording=INCOMPLETE dropped=" + std::to_string(dropped) +
                        " (a bound was reached; no run_end)\n";
            }
            return text + scenario_notes;
        });
}

void PresentCorrelator::gpu_ready(std::uint32_t island, std::uint64_t generation,
                                  std::uint64_t frame) {
    waiting_.emplace(std::pair{island, generation}, frame);
}

std::vector<std::pair<std::uint32_t, std::uint64_t>>
PresentCorrelator::presented(std::uint64_t frame) {
    std::vector<std::pair<std::uint32_t, std::uint64_t>> done;
    for (auto it = waiting_.begin(); it != waiting_.end();) {
        if (it->second <= frame) {
            done.push_back(it->first);
            it = waiting_.erase(it);
        } else {
            ++it;
        }
    }
    return done;
}

void FrameTracer::committed(std::uint32_t island, std::uint64_t generation,
                            std::vector<render::ChunkKey> keys) {
    unwatched_.push_back({island, generation, std::move(keys)});
}

void FrameTracer::begin(FrameRecord row, render::Rect view, std::size_t group) {
    row_ = std::move(row);
    row_.frame = ++frame_;
    row_.epoch = epoch_;
    if (unwatched_.empty()) {
        return;
    }
    const std::vector<render::ChunkKey> keys = view_keys(view, group);
    const std::set<render::ChunkKey> in_view(keys.begin(), keys.end());
    const double t = trace_.now_ms();
    for (Watch& c : unwatched_) {
        ReadinessTracker::Set set{c.island, c.generation, {}};
        for (const render::ChunkKey& k : c.keys) {
            if (in_view.count(k) != 0) {
                set.keys.push_back(k);
            }
        }
        if (set.keys.empty()) {
            trace_.event(t, TraceKind::NoVisibleChange, c.island, c.generation);
        } else {
            gpu_.add(set);
            cpu_.add(std::move(set));
        }
    }
    unwatched_.clear();
}

void FrameTracer::acquired(double start, double end, std::uint32_t status) {
    row_.epoch = epoch_; // after a reconfiguration in the prologue
    row_.acquire_start_ms = start;
    row_.acquire_end_ms = end;
    row_.acquire_status = status;
    row_.texture = acquire_gives_texture(classify_acquire(status));
}

void FrameTracer::failed(const char* outcome) {
    row_.outcome = outcome;
    trace_.event(row_.acquire_end_ms, TraceKind::FrameFailed,
                 static_cast<std::uint32_t>(row_.epoch), row_.frame);
    trace_.frame(std::move(row_));
    row_ = {};
}

void FrameTracer::presented(double t_submit, double p0, double p1, std::uint32_t status,
                            const Ready& fresh, const Ready& usable, bool picture_idle) {
    const auto epoch = static_cast<std::uint32_t>(row_.epoch);
    row_.submit_ms = t_submit;
    row_.present_start_ms = p0;
    row_.present_end_ms = p1;
    row_.present_status = status;
    trace_.event(t_submit, TraceKind::FrameSubmitted, epoch, row_.frame);
    // One observation, at the submission: CPU first, then GPU (GPU-usable implies CPU-ready).
    for (const ReadinessTracker::Set& r : cpu_.poll(fresh, t_submit, key_log_)) {
        trace_.event(t_submit, TraceKind::ChunksReady, r.island, r.generation);
    }
    for (const ReadinessTracker::Set& r : gpu_.poll(usable, t_submit, key_log_)) {
        trace_.event(t_submit, TraceKind::GpuReady, r.island, r.generation);
        correlator_.gpu_ready(r.island, r.generation, row_.frame);
    }
    key_log_.clear(); // the per-key log is not kept (bounded memory)
    if (status == kPresentSuccess) {
        row_.outcome = "presented";
        trace_.event(p1, TraceKind::FramePresented, epoch, row_.frame);
        for (const auto& [island, generation] : correlator_.presented(row_.frame)) {
            trace_.event(p1, TraceKind::Presented, island, generation);
        }
    } else {
        // A failed present completes nothing; a later successful frame may.
        row_.outcome = "present_failed";
        trace_.event(p1, TraceKind::FrameFailed, epoch, row_.frame);
    }
    const bool idle = picture_idle && outstanding() == 0;
    if (idle && !converged_) {
        trace_.event(p1, TraceKind::Converged, 0, 0);
    }
    converged_ = idle;
    trace_.frame(std::move(row_));
    row_ = {};
}

} // namespace anno::app
