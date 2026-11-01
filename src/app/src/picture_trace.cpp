// Raw events of the picture's CPU pipeline and their aggregation (see the header).
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>

#include <anno/app/picture_trace.hpp>

namespace anno::app {

namespace {

constexpr std::array<std::string_view, 28> kNames = {"desired",
                                                     "admitted",
                                                     "deferred",
                                                     "started",
                                                     "built",
                                                     "failed",
                                                     "superseded",
                                                     "dropped",
                                                     "requeued",
                                                     "committed",
                                                     "generation",
                                                     "chunks_ready",
                                                     "no_visible_change",
                                                     "retired",
                                                     "reclaimed",
                                                     "drain_start",
                                                     "drain_end",
                                                     "converged",
                                                     "run_end",
                                                     "gpu_mode",
                                                     "gpu_ready",
                                                     "batch_published",
                                                     "batch_taken",
                                                     "frame_submitted",
                                                     "frame_presented",
                                                     "frame_failed",
                                                     "presented",
                                                     "drawn"};

struct Edit {
    double t0 = 0;
    std::uint32_t island = 0;
    std::uint64_t version = 0;
    double t1 = -1, t2 = -1, t3 = -1, t4 = -1;
    std::uint64_t generation = 0;
    bool coalesced = false, offscreen = false;
};

void line(std::ostringstream& o, std::string_view name, const Distribution& d) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%.*s n=%zu p50_ms=%.3f p95_ms=%s p99_ms=%s max_ms=%.3f\n",
                  static_cast<int>(name.size()), name.data(), d.n, d.p50,
                  d.p95_valid ? std::to_string(d.p95).c_str() : "insufficient",
                  d.p99_valid ? std::to_string(d.p99).c_str() : "insufficient", d.max);
    o << buf;
}

} // namespace

std::string_view trace_kind_name(TraceKind k) {
    const auto i = static_cast<std::size_t>(k);
    return i < kNames.size() ? kNames[i] : "unknown";
}

Distribution distribution(std::vector<double> samples) {
    Distribution d;
    d.n = samples.size();
    if (samples.empty()) {
        return d;
    }
    std::sort(samples.begin(), samples.end());
    auto rank = [&samples](double p) {
        const auto r = static_cast<std::size_t>(std::ceil(p * static_cast<double>(samples.size())));
        return samples[std::clamp<std::size_t>(r == 0 ? 0 : r - 1, 0, samples.size() - 1)];
    };
    d.p50 = rank(0.50);
    d.p95 = rank(0.95);
    d.p99 = rank(0.99);
    d.max = samples.back();
    d.p95_valid = d.n >= 20;
    d.p99_valid = d.n >= 100;
    return d;
}

TraceSummary summarize(std::span<const TraceEvent> input) {
    std::vector<TraceEvent> events(input.begin(), input.end());
    std::stable_sort(events.begin(), events.end(),
                     [](const TraceEvent& a, const TraceEvent& b) { return a.t_ms < b.t_ms; });
    TraceSummary s;
    std::vector<Edit> edits;
    double last_desired = 0;
    std::vector<std::size_t> awaiting_generation;
    std::map<std::uint64_t, double> retired_at;
    // A generation number is never reused and never goes back (each commit makes the next one).
    std::optional<std::uint64_t> last_generation;
    double run_end_ms = -1;
    std::vector<double> reclaim, queue, build;
    std::map<std::pair<std::uint32_t, std::uint64_t>, double> admitted_at, started_at;
    std::map<std::uint32_t, std::uint64_t> desired_of;
    // For the causal checks (round ec, A1): the islands committed since the last Generation event,
    // the time each (island, generation) was committed, and the readiness events per pair.
    std::vector<std::pair<std::uint32_t, double>> committed_pending;
    std::map<std::pair<std::uint32_t, std::uint64_t>, double> committed_at;
    std::map<std::pair<std::uint32_t, std::uint64_t>, std::vector<double>> cpu_at, gpu_at, none_at,
        presented_at, drawn_at;
    // The window trace: frames submitted, presented and failed by id (with their times), and the
    // frame of each time (every event of a frame carries that frame's time).
    std::map<std::uint64_t, double> submitted_frame, presented_frame;
    std::map<std::uint64_t, std::vector<double>> failed_frame;
    // The surface epoch each frame's events carry (all of a frame's events carry one), and the
    // desired events of a window trace with their times (each follows a batch_taken).
    std::map<std::uint64_t, std::uint32_t> frame_epoch;
    std::vector<std::pair<double, std::pair<std::uint32_t, std::uint64_t>>> desired_at;
    std::vector<double> taken_times, frame_times;
    auto epoch_of = [&](const TraceEvent& e) {
        const auto [it, inserted] = frame_epoch.emplace(e.version, e.island);
        if (!inserted && it->second != e.island) {
            s.contradictions.push_back("frame " + std::to_string(e.version) + " in epochs " +
                                       std::to_string(it->second) + " and " +
                                       std::to_string(e.island));
        }
    };
    std::multimap<double, std::uint64_t> submitted_at_time, presented_at_time;
    std::map<std::uint64_t, std::vector<double>> published_turn, taken_turn;
    auto build_end = [&](const TraceEvent& e) {
        const auto it = started_at.find({e.island, e.version});
        if (it != started_at.end()) {
            build.push_back(e.t_ms - it->second);
            started_at.erase(it);
        }
    };
    for (const TraceEvent& e : events) {
        s.end_ms = std::max(s.end_ms, e.t_ms);
        switch (e.kind) {
        case TraceKind::Desired:
            desired_at.push_back({e.t_ms, {e.island, e.version}});
            edits.push_back(Edit{e.t_ms, e.island, e.version});
            desired_of[e.island] = std::max(desired_of[e.island], e.version);
            ++s.edits;
            last_desired = e.t_ms;
            s.converged_ms = -1; // a newer edit: convergence is measured again
            break;
        case TraceKind::Committed:
            committed_pending.emplace_back(e.island, e.t_ms);
            ++s.commits;
            s.intermediate_commits += e.version < desired_of[e.island] ? 1U : 0U;
            s.last_commit_ms = e.t_ms;
            for (std::size_t i = 0; i < edits.size(); ++i) {
                Edit& d = edits[i];
                if (d.island == e.island && d.t1 < 0 && d.version <= e.version) {
                    d.t1 = e.t_ms;
                    d.coalesced = d.version < e.version;
                    awaiting_generation.push_back(i);
                }
            }
            break;
        case TraceKind::Generation:
            if (last_generation && e.version <= *last_generation) {
                s.contradictions.push_back("generation " + std::to_string(e.version) +
                                           " after generation " + std::to_string(*last_generation) +
                                           " (generations only increase)");
            }
            last_generation = e.version;
            if (committed_pending.empty()) {
                s.contradictions.push_back("generation " + std::to_string(e.version) +
                                           " without a committed island");
            }
            for (const auto& [island, t] : committed_pending) {
                committed_at.emplace(std::pair{island, e.version}, t);
            }
            committed_pending.clear();
            ++s.generations;
            for (const std::size_t i : awaiting_generation) {
                edits[i].generation = e.version;
            }
            awaiting_generation.clear();
            break;
        case TraceKind::ChunksReady:
            cpu_at[{e.island, e.version}].push_back(e.t_ms);
            // Per island: one island's readiness is never another island's latency (2d).
            for (Edit& d : edits) {
                if (d.island == e.island && d.generation == e.version && d.t1 >= 0 && d.t2 < 0 &&
                    !d.offscreen && e.t_ms >= d.t1) {
                    d.t2 = e.t_ms;
                }
            }
            break;
        case TraceKind::GpuReady:
            gpu_at[{e.island, e.version}].push_back(e.t_ms);
            // The GPU boundary of the same island and generation, not before the commit.
            for (Edit& d : edits) {
                if (d.island == e.island && d.generation == e.version && d.t1 >= 0 && d.t3 < 0 &&
                    !d.offscreen && e.t_ms >= d.t1) {
                    d.t3 = e.t_ms;
                }
            }
            break;
        case TraceKind::GpuMode:
            s.gpu_mode = true;
            break;
        case TraceKind::BatchPublished:
            s.window = true;
            ++s.batches_published;
            published_turn[e.version].push_back(e.t_ms);
            break;
        case TraceKind::BatchTaken:
            s.window = true;
            ++s.batches_taken;
            taken_turn[e.version].push_back(e.t_ms);
            taken_times.push_back(e.t_ms);
            break;
        case TraceKind::FrameSubmitted:
            s.window = true;
            ++s.frames_submitted;
            epoch_of(e);
            frame_times.push_back(e.t_ms);
            if (!submitted_frame.emplace(e.version, e.t_ms).second) {
                s.contradictions.push_back("frame_submitted of frame " + std::to_string(e.version) +
                                           " twice");
            }
            submitted_at_time.emplace(e.t_ms, e.version);
            break;
        case TraceKind::FramePresented:
            s.window = true;
            ++s.frames_presented;
            epoch_of(e);
            frame_times.push_back(e.t_ms);
            if (!presented_frame.emplace(e.version, e.t_ms).second) {
                s.contradictions.push_back("frame_presented of frame " + std::to_string(e.version) +
                                           " twice");
            }
            presented_at_time.emplace(e.t_ms, e.version);
            break;
        case TraceKind::FrameFailed:
            s.window = true;
            ++s.frames_failed;
            epoch_of(e);
            frame_times.push_back(e.t_ms);
            failed_frame[e.version].push_back(e.t_ms);
            break;
        case TraceKind::Presented:
            s.window = true;
            presented_at[{e.island, e.version}].push_back(e.t_ms);
            for (Edit& d : edits) {
                if (d.island == e.island && d.generation == e.version && d.t1 >= 0 && d.t4 < 0 &&
                    !d.offscreen && e.t_ms >= d.t1) {
                    d.t4 = e.t_ms;
                }
            }
            break;
        case TraceKind::Drawn:
            s.window = true;
            drawn_at[{e.island, e.version}].push_back(e.t_ms);
            break;
        case TraceKind::NoVisibleChange:
            none_at[{e.island, e.version}].push_back(e.t_ms);
            for (Edit& d : edits) {
                if (d.island == e.island && d.generation == e.version && d.t2 < 0) {
                    d.offscreen = true;
                }
            }
            break;
        case TraceKind::Converged:
            if (s.converged_ms < 0 && e.t_ms >= last_desired) {
                s.converged_ms = e.t_ms;
            }
            break;
        case TraceKind::Retired:
            retired_at.emplace(e.version, e.t_ms);
            break;
        case TraceKind::Reclaimed: {
            ++s.reclaimed;
            const auto it = retired_at.find(e.version);
            if (it != retired_at.end()) {
                reclaim.push_back(e.t_ms - it->second);
                retired_at.erase(it);
            }
            break;
        }
        case TraceKind::Admitted:
            ++s.admitted;
            admitted_at[{e.island, e.version}] = e.t_ms;
            break;
        case TraceKind::Deferred:
            ++s.deferred;
            break;
        case TraceKind::Built:
            ++s.built;
            build_end(e);
            break;
        case TraceKind::Failed:
            ++s.failed;
            build_end(e);
            break;
        case TraceKind::Superseded:
            ++s.superseded;
            build_end(e);
            break;
        case TraceKind::Dropped:
            ++s.dropped;
            break;
        case TraceKind::Requeued:
            ++s.requeued;
            break;
        case TraceKind::Started: {
            started_at[{e.island, e.version}] = e.t_ms;
            const auto it = admitted_at.find({e.island, e.version});
            if (it != admitted_at.end()) {
                queue.push_back(e.t_ms - it->second);
                admitted_at.erase(it);
            }
            break;
        }
        case TraceKind::DrainStart:
        case TraceKind::DrainEnd:
            break;
        case TraceKind::RunEnd:
            s.complete = true;
            s.run_status = e.version;
            run_end_ms = e.t_ms;
            break;
        }
    }
    // A commit is made by a generation: committed islands with no Generation after them, and an
    // event later than the run's end, cannot be a recording.
    for (const auto& [island, t] : committed_pending) {
        s.contradictions.push_back("island " + std::to_string(island) + " committed at " +
                                   std::to_string(t) + " ms without a generation after it");
    }
    if (s.complete && s.end_ms > run_end_ms) {
        s.contradictions.push_back("an event at " + std::to_string(s.end_ms) +
                                   " ms after run_end at " + std::to_string(run_end_ms) + " ms");
    }
    std::vector<double> object, visible, gpu, present;
    for (const Edit& d : edits) {
        if (d.t1 < 0) {
            ++s.unfinished;
            s.max_age_ms = std::max(s.max_age_ms, s.end_ms - d.t0);
            continue;
        }
        ++s.object_ready;
        s.coalesced += d.coalesced ? 1U : 0U;
        object.push_back(d.t1 - d.t0);
        s.max_age_ms = std::max(s.max_age_ms, d.t1 - d.t0);
        if (d.offscreen) {
            ++s.offscreen;
        } else if (d.t2 >= 0) {
            ++s.visible_ready;
            visible.push_back(d.t2 - d.t0);
        }
        if (s.gpu_mode && !d.offscreen) {
            if (d.t3 >= 0) {
                ++s.gpu_ready;
                gpu.push_back(d.t3 - d.t0);
            } else if (d.t2 >= 0) {
                ++s.gpu_unfinished;
            }
        }
        if (s.window && !d.offscreen) {
            if (d.t4 >= 0) {
                ++s.presented;
                present.push_back(d.t4 - d.t0);
                // A later commit of the island before the presentation. The trace does not say
                // whether it touched the same chunks, so this is not a count of coalesced pixels:
                // the frame may show the newer content, or the edit's own.
                const auto later = committed_at.upper_bound({d.island, d.generation});
                if (later != committed_at.end() && later->first.first == d.island &&
                    later->second < d.t4) {
                    ++s.present_after_recommit;
                }
            } else if (d.t3 >= 0) {
                ++s.present_unfinished;
            }
        }
    }
    s.present_ms = distribution(std::move(present));
    s.gpu_ms = distribution(std::move(gpu));
    // The causal checks over the whole trace.
    auto pair_name = [](const std::pair<std::uint32_t, std::uint64_t>& p) {
        return "island " + std::to_string(p.first) + " generation " + std::to_string(p.second);
    };
    auto check_committed = [&](const char* kind, const auto& by_pair) {
        for (const auto& [p, times] : by_pair) {
            const auto c = committed_at.find(p);
            if (c == committed_at.end()) {
                s.contradictions.push_back(std::string(kind) + " of " + pair_name(p) +
                                           ": the island was not committed in that generation");
                continue;
            }
            for (const double t : times) {
                if (t < c->second) {
                    s.contradictions.push_back(std::string(kind) + " of " + pair_name(p) +
                                               " before its commit");
                }
            }
            if (times.size() > 1) {
                s.contradictions.push_back(std::string(kind) + " of " + pair_name(p) + " " +
                                           std::to_string(times.size()) + " times");
            }
        }
    };
    check_committed("chunks_ready", cpu_at);
    check_committed("no_visible_change", none_at);
    check_committed("gpu_ready", gpu_at);
    for (const auto& [p, times] : none_at) {
        if (cpu_at.contains(p)) {
            s.contradictions.push_back("both no_visible_change and chunks_ready for " +
                                       pair_name(p));
        }
    }
    // The window trace's rules (docs/app/window-trace.md).
    check_committed("presented", presented_at);
    auto frame_at = [](const std::multimap<double, std::uint64_t>& by_time, double t) {
        const auto it = by_time.find(t);
        return it == by_time.end() ? std::optional<std::uint64_t>{} : std::optional{it->second};
    };
    for (const auto& [p, times] : presented_at) {
        const auto ready = gpu_at.find(p);
        if (ready == gpu_at.end()) {
            s.contradictions.push_back("presented of " + pair_name(p) + " without its gpu_ready");
            continue;
        }
        const double t_gpu = *std::min_element(ready->second.begin(), ready->second.end());
        const auto gpu_frame = frame_at(submitted_at_time, t_gpu);
        for (const double t : times) {
            const auto frame = frame_at(presented_at_time, t);
            if (t < t_gpu) {
                s.contradictions.push_back("presented of " + pair_name(p) +
                                           " before its gpu_ready");
            } else if (!frame) {
                s.contradictions.push_back("presented of " + pair_name(p) +
                                           " at a time with no frame_presented");
            } else if (!gpu_frame || *frame < *gpu_frame) {
                s.contradictions.push_back(
                    "presented of " + pair_name(p) + " by frame " + std::to_string(*frame) +
                    (gpu_frame ? ", before the frame " + std::to_string(*gpu_frame) +
                                     " that made it GPU-usable"
                               : ", but its gpu_ready is at no frame_submitted"));
            } else {
                // Round ed: the presenting frame draws the change (a Drawn at its submission), and
                // it is the first presented frame that does: an earlier presented frame that drew
                // it is a present the trace skipped. A presented frame that did not draw it (the
                // view elsewhere, the chunks stale) completes nothing.
                const auto drawn = drawn_at.find(p);
                const auto sub = submitted_frame.find(*frame);
                const auto draws = [&](std::uint64_t f) {
                    const auto fs = submitted_frame.find(f);
                    return drawn != drawn_at.end() && fs != submitted_frame.end() &&
                           std::find(drawn->second.begin(), drawn->second.end(), fs->second) !=
                               drawn->second.end();
                };
                if (sub == submitted_frame.end() || !draws(*frame)) {
                    s.contradictions.push_back("presented of " + pair_name(p) + " by frame " +
                                               std::to_string(*frame) +
                                               ", which has no drawn of it (the frame does not "
                                               "draw the change)");
                    continue;
                }
                for (auto it = presented_frame.lower_bound(*gpu_frame);
                     it != presented_frame.end() && it->first < *frame; ++it) {
                    if (draws(it->first)) {
                        s.contradictions.push_back("presented of " + pair_name(p) + " by frame " +
                                                   std::to_string(*frame) + ", but frame " +
                                                   std::to_string(it->first) +
                                                   " drew and presented it earlier");
                        break;
                    }
                }
            }
        }
    }
    for (const auto& [frame, t] : presented_frame) {
        const auto sub = submitted_frame.find(frame);
        if (sub == submitted_frame.end() || sub->second > t) {
            s.contradictions.push_back("frame_presented of frame " + std::to_string(frame) +
                                       " without its frame_submitted before it");
        }
        if (failed_frame.contains(frame) && !failed_frame[frame].empty()) {
            s.contradictions.push_back("frame " + std::to_string(frame) +
                                       " both presented and failed");
        }
    }
    for (const auto& [frame, times] : failed_frame) {
        if (times.size() > 1) {
            s.contradictions.push_back("frame_failed of frame " + std::to_string(frame) + " " +
                                       std::to_string(times.size()) + " times");
        }
        const auto sub = submitted_frame.find(frame);
        if (sub != submitted_frame.end() && times.front() < sub->second) {
            s.contradictions.push_back("frame_failed of frame " + std::to_string(frame) +
                                       " before its frame_submitted");
        }
    }
    // Epochs: one per frame (above), and never lower for a later frame.
    std::optional<std::pair<std::uint64_t, std::uint32_t>> last_epoch;
    for (const auto& [frame, epoch] : frame_epoch) {
        if (last_epoch && epoch < last_epoch->second) {
            s.contradictions.push_back("frame " + std::to_string(frame) + " in epoch " +
                                       std::to_string(epoch) + " after frame " +
                                       std::to_string(last_epoch->first) + " in epoch " +
                                       std::to_string(last_epoch->second));
        }
        last_epoch = {frame, epoch};
    }
    // In a window trace an edit comes from a batch its frame boundary took: a desired event needs
    // a batch_taken at or before it with no frame event between the two (the take and the apply
    // are one frame boundary, before that frame's render).
    if (s.window) {
        std::sort(taken_times.begin(), taken_times.end());
        std::sort(frame_times.begin(), frame_times.end());
        for (const auto& [t, p] : desired_at) {
            const auto take = std::upper_bound(taken_times.begin(), taken_times.end(), t);
            const std::string what = "desired of island " + std::to_string(p.first) + " version " +
                                     std::to_string(p.second);
            if (take == taken_times.begin()) {
                s.contradictions.push_back(what + " with no batch_taken before it (window trace)");
                continue;
            }
            const double t_take = *std::prev(take);
            const auto frame = std::upper_bound(frame_times.begin(), frame_times.end(), t_take);
            if (frame != frame_times.end() && *frame < t) {
                s.contradictions.push_back(what +
                                           " not in the frame boundary of its batch_taken (a "
                                           "frame event lies between them)");
            }
        }
    }
    for (const auto& [turn, times] : taken_turn) {
        const auto pub = published_turn.find(turn);
        for (const double t : times) {
            if (pub == published_turn.end() ||
                *std::min_element(pub->second.begin(), pub->second.end()) > t) {
                s.contradictions.push_back("batch_taken of turn " + std::to_string(turn) +
                                           " without its batch_published before it");
            }
        }
    }
    for (const auto& [p, times] : gpu_at) {
        if (!s.gpu_mode) {
            s.contradictions.push_back("gpu_ready of " + pair_name(p) + " without gpu_mode");
        }
        const auto cpu = cpu_at.find(p);
        if (cpu == cpu_at.end()) {
            s.contradictions.push_back("gpu_ready of " + pair_name(p) +
                                       " without its chunks_ready (GPU-usable implies CPU-ready)");
            continue;
        }
        const double first_cpu = *std::min_element(cpu->second.begin(), cpu->second.end());
        for (const double t : times) {
            if (t < first_cpu) {
                s.contradictions.push_back("gpu_ready of " + pair_name(p) +
                                           " before its chunks_ready");
            }
        }
    }
    // Round ed: each drawn names a submitted frame, follows the pair's gpu_ready, is once per
    // frame and ends at the pair's presented; a window gpu_ready is the first frame that draws it.
    for (const auto& [p, times] : drawn_at) {
        const auto c = committed_at.find(p);
        const auto ready = gpu_at.find(p);
        const auto shown = presented_at.find(p);
        std::set<double> seen;
        // The first frame that drew the change and presented must carry its presented (round ed
        // review: a completion the producer lost would otherwise read as unfinished work).
        std::optional<std::uint64_t> first_shown;
        for (const double t : times) {
            const auto frame = frame_at(submitted_at_time, t);
            if (frame && presented_frame.contains(*frame) &&
                (!first_shown || *frame < *first_shown)) {
                first_shown = frame;
            }
            if (!frame) {
                s.contradictions.push_back("drawn of " + pair_name(p) +
                                           " at a time with no frame_submitted");
            }
            if (c == committed_at.end() || t < c->second) {
                s.contradictions.push_back("drawn of " + pair_name(p) + " before its commit");
            }
            if (ready == gpu_at.end() ||
                t < *std::min_element(ready->second.begin(), ready->second.end())) {
                s.contradictions.push_back("drawn of " + pair_name(p) + " before its gpu_ready");
            }
            if (!seen.insert(t).second) {
                s.contradictions.push_back("drawn of " + pair_name(p) + " twice in one frame");
            }
            if (shown != presented_at.end() &&
                t > *std::min_element(shown->second.begin(), shown->second.end())) {
                s.contradictions.push_back("drawn of " + pair_name(p) + " after its presented");
            }
        }
        if (first_shown) {
            const double t_present = presented_frame.at(*first_shown);
            if (shown == presented_at.end() || std::find(shown->second.begin(), shown->second.end(),
                                                         t_present) == shown->second.end()) {
                s.contradictions.push_back("frame " + std::to_string(*first_shown) + " drew " +
                                           pair_name(p) +
                                           " and presented, but no presented of it "
                                           "at that frame");
            }
        }
    }
    if (s.window) {
        for (const auto& [p, times] : gpu_at) {
            const auto drawn = drawn_at.find(p);
            const double first = *std::min_element(times.begin(), times.end());
            if (drawn == drawn_at.end() || std::find(drawn->second.begin(), drawn->second.end(),
                                                     first) == drawn->second.end()) {
                s.contradictions.push_back("gpu_ready of " + pair_name(p) +
                                           " with no drawn of its frame (window trace)");
            }
        }
    }
    s.object_ms = distribution(std::move(object));
    s.visible_ms = distribution(std::move(visible));
    s.reclaim_ms = distribution(std::move(reclaim));
    s.queue_ms = distribution(std::move(queue));
    s.build_ms = distribution(std::move(build));
    return s;
}

std::string to_tsv(std::span<const TraceEvent> events) {
    std::ostringstream o;
    o << "t_ms\tkind\tisland\tversion\n";
    char buf[128];
    for (const TraceEvent& e : events) {
        const std::string_view name = trace_kind_name(e.kind);
        std::snprintf(buf, sizeof buf, "%.6f\t%.*s\t%u\t%llu\n", e.t_ms,
                      static_cast<int>(name.size()), name.data(), e.island,
                      static_cast<unsigned long long>(e.version));
        o << buf;
    }
    return o.str();
}

formats::Result<std::vector<TraceEvent>, std::string> parse_tsv(std::string_view text) {
    if (text.empty()) {
        return std::string("trace: empty (no header line)");
    }
    std::vector<TraceEvent> out;
    std::size_t line_no = 0;
    bool ended = false;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view row = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        ++line_no;
        const std::string where = "trace line " + std::to_string(line_no) + ": ";
        if (line_no == 1) {
            if (row != kTraceHeader) {
                return where + "the header is '" + std::string(row) + "', expected '" +
                       std::string(kTraceHeader) + "'";
            }
            continue;
        }
        if (row.empty()) {
            if (!text.empty()) {
                return where + "a blank line before the end";
            }
            break;
        }
        if (ended) {
            return where + "an event after run_end";
        }
        std::array<std::string_view, 4> f{};
        std::size_t fields = 0;
        std::string_view rest = row;
        for (;;) {
            const std::size_t tab = rest.find('\t');
            if (fields < f.size()) {
                f[fields] = rest.substr(0, tab);
            }
            ++fields;
            if (tab == std::string_view::npos) {
                break;
            }
            rest = rest.substr(tab + 1);
        }
        if (fields != f.size()) {
            return where + std::to_string(fields) + " fields, expected 4";
        }
        TraceEvent e;
        // The time: a whole token, finite and not negative (strtod reads the C locale's numbers).
        const std::string t(f[0]);
        char* stop = nullptr;
        errno = 0;
        e.t_ms = t.empty() ? 0 : std::strtod(t.c_str(), &stop);
        if (t.empty() || stop != t.c_str() + t.size() || errno == ERANGE ||
            !std::isfinite(e.t_ms) || e.t_ms < 0) {
            return where + "the time '" + t + "' is not a finite non-negative number";
        }
        const auto kind = std::find(kNames.begin(), kNames.end(), f[1]);
        if (kind == kNames.end()) {
            return where + "unknown kind '" + std::string(f[1]) + "'";
        }
        e.kind = static_cast<TraceKind>(kind - kNames.begin());
        auto whole = [&where](std::string_view token, auto& value,
                              const char* name) -> std::string {
            const auto [ptr, ec] =
                std::from_chars(token.data(), token.data() + token.size(), value);
            if (ec == std::errc::result_out_of_range) {
                return where + "the " + name + " '" + std::string(token) + "' is out of range";
            }
            if (token.empty() || ec != std::errc{} || ptr != token.data() + token.size()) {
                return where + "the " + name + " '" + std::string(token) +
                       "' is not a whole number";
            }
            return {};
        };
        unsigned long long version = 0;
        if (std::string bad = whole(f[2], e.island, "island"); !bad.empty()) {
            return bad;
        }
        if (std::string bad = whole(f[3], version, "version"); !bad.empty()) {
            return bad;
        }
        e.version = version;
        ended = e.kind == TraceKind::RunEnd;
        out.push_back(e);
    }
    if (line_no == 0) {
        return std::string("trace: empty (no header line)");
    }
    return out;
}

std::string format_summary(const TraceSummary& s) {
    std::ostringstream o;
    if (s.complete) {
        o << "run=complete status=" << s.run_status
          << (s.run_status != 0 ? " (FAILED)"
              : s.consistent()  ? " (succeeded)"
                                : " (status 0, but the trace contradicts itself: not a success)")
          << "\n";
    } else {
        o << "run=INCOMPLETE (no run_end event: the run stopped or the trace was cut)\n";
    }
    if (!s.contradictions.empty()) {
        o << "consistency=CONTRADICTORY (" << s.contradictions.size()
          << " contradiction(s): not a measurement)\n";
        for (const std::string& c : s.contradictions) {
            o << "contradiction: " << c << "\n";
        }
    }
    o << "edits=" << s.edits << " object_ready=" << s.object_ready
      << " visible_ready=" << s.visible_ready << " offscreen=" << s.offscreen
      << " coalesced=" << s.coalesced << " unfinished=" << s.unfinished
      << " intermediate_commits=" << s.intermediate_commits << "\n";
    o << "admitted=" << s.admitted << " deferred=" << s.deferred << " built=" << s.built
      << " failed=" << s.failed << " superseded=" << s.superseded << " dropped=" << s.dropped
      << " requeued=" << s.requeued << " commits=" << s.commits << " generations=" << s.generations
      << " reclaimed=" << s.reclaimed << "\n";
    line(o, "object_ready", s.object_ms);
    line(o, "visible_cpu_chunks_ready", s.visible_ms);
    if (s.gpu_mode) {
        o << "gpu_mode=1 gpu_ready=" << s.gpu_ready << " gpu_unfinished=" << s.gpu_unfinished
          << " gpu_completion=unobserved presentation="
          << (s.window ? "present_call_return" : "unmeasured") << "\n";
        line(o, "visible_gpu_usable", s.gpu_ms);
    } else {
        o << "gpu_mode=0\n";
    }
    if (s.window) {
        o << "window=1 presented=" << s.presented << " present_unfinished=" << s.present_unfinished
          << " present_after_recommit=" << s.present_after_recommit
          << " frames_submitted=" << s.frames_submitted
          << " frames_presented=" << s.frames_presented << " frames_failed=" << s.frames_failed
          << " batches_published=" << s.batches_published << " batches_taken=" << s.batches_taken
          << " present=call_return display=unmeasured\n";
        line(o, "visible_present_call_returned", s.present_ms);
    }
    line(o, "reclamation_delay", s.reclaim_ms);
    line(o, "build_queue_delay", s.queue_ms);
    line(o, "build_time_worker", s.build_ms);
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "max_age_ms=%.3f last_commit_ms=%.3f converged_ms=%.3f end_ms=%.3f\n",
                  s.max_age_ms, s.last_commit_ms, s.converged_ms, s.end_ms);
    o << buf;
    return o.str();
}

void ReadinessTracker::add(Set set) {
    if (set.keys.empty()) {
        return;
    }
    Waiting w;
    w.fresh.assign(set.keys.size(), false);
    w.set = std::move(set);
    sets_.push_back(std::move(w));
}

std::vector<ReadinessTracker::Set>
ReadinessTracker::poll(const std::function<bool(const render::ChunkKey&)>& fresh, double t_ms,
                       std::vector<KeyEvent>& log) {
    std::vector<Set> ready;
    for (auto it = sets_.begin(); it != sets_.end();) {
        bool all = true;
        for (std::size_t k = 0; k < it->set.keys.size(); ++k) {
            // The state now, for every key of the set: a key fresh at an earlier poll and stale
            // again (a later change invalidated it) keeps the set waiting.
            const bool now = fresh(it->set.keys[k]);
            if (!it->polled || now != it->fresh[k]) {
                log.push_back({t_ms, it->set.island, it->set.generation, it->set.keys[k], now});
            }
            it->fresh[k] = now;
            all = all && now;
        }
        it->polled = true;
        if (all) {
            ready.push_back(std::move(it->set));
            it = sets_.erase(it);
        } else {
            ++it;
        }
    }
    return ready;
}

TextFile read_text(const std::filesystem::path& path) {
    TextFile f;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        f.error = "cannot open " + path.string() + " for reading";
        return f;
    }
    f.text.assign(std::istreambuf_iterator<char>(in), {});
    if (in.bad()) {
        f.error = "cannot read " + path.string();
        f.text.clear();
        return f;
    }
    f.ok = true;
    return f;
}

std::string save_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return "cannot open " + path.string() + " for writing";
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!out) {
        return "cannot write " + path.string();
    }
    out.flush();
    if (!out) {
        return "cannot flush " + path.string();
    }
    out.close();
    if (out.fail()) {
        return "cannot close " + path.string();
    }
    return {};
}

std::string save_whole(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::path partial = path;
    partial += ".partial";
    std::error_code ec;
    std::string error = save_text(partial, text);
    if (error.empty()) {
        std::filesystem::rename(partial, path, ec);
        if (ec) {
            error =
                "cannot rename " + partial.string() + " to " + path.string() + ": " + ec.message();
        }
    }
    if (!error.empty()) {
        std::filesystem::remove(partial, ec);
    }
    return error;
}

EvidenceOutcome finalize_evidence(const std::filesystem::path& dir,
                                  std::span<const EvidenceFile> files,
                                  std::vector<TraceEvent> events, double end_ms, int verdict,
                                  const std::function<std::string(const TraceSummary&)>& summary) {
    EvidenceOutcome out;
    auto save = [&out](const std::filesystem::path& path, std::string_view text) {
        if (std::string error = save_whole(path, text); !error.empty()) {
            out.unsaved.push_back(std::move(error));
            return false;
        }
        return true;
    };
    for (const EvidenceFile& f : files) {
        save(dir / f.name, f.text);
    }
    out.status = out.unsaved.empty() && verdict >= 0 ? verdict : kEvidenceNotSaved;
    // A recording known to be incomplete gets no RunEnd: its trace reads as incomplete.
    const bool complete_recording = verdict >= 0;
    if (complete_recording) {
        events.push_back({end_ms, TraceKind::RunEnd, 0, static_cast<std::uint64_t>(out.status)});
    }
    TraceSummary summarized = summarize(events);
    if (complete_recording && out.status == 0 && !summarized.consistent()) {
        // The run's own trace contradicts itself (round ec review): it is not a success.
        out.status = kEvidenceContradictory;
        events.back().version = static_cast<std::uint64_t>(out.status);
        summarized = summarize(events);
    }
    const bool summary_saved = save(dir / "summary.txt", summary(summarized));
    if (!summary_saved) {
        out.status = kEvidenceNotSaved;
        if (complete_recording) {
            events.back().version = static_cast<std::uint64_t>(out.status);
        }
    }
    out.trace_saved = save(dir / "events.tsv", to_tsv(events));
    if (!out.trace_saved) {
        out.status = kEvidenceNotSaved;
        std::error_code ec;
        if (summary_saved && std::filesystem::remove(dir / "summary.txt", ec)) {
            out.unsaved.push_back("summary.txt removed: it describes a trace that was not saved");
        }
    }
    return out;
}

} // namespace anno::app
