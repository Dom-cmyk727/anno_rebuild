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
#include <sstream>

#include <anno/app/picture_trace.hpp>

namespace anno::app {

namespace {

constexpr std::array<std::string_view, 19> kNames = {
    "desired",     "admitted",     "deferred",          "started",  "built",
    "failed",      "superseded",   "dropped",           "requeued", "committed",
    "generation",  "chunks_ready", "no_visible_change", "retired",  "reclaimed",
    "drain_start", "drain_end",    "converged",         "run_end"};

struct Edit {
    double t0 = 0;
    std::uint32_t island = 0;
    std::uint64_t version = 0;
    double t1 = -1, t2 = -1;
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
    std::vector<double> reclaim, queue, build;
    std::map<std::pair<std::uint32_t, std::uint64_t>, double> admitted_at, started_at;
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
            edits.push_back(Edit{e.t_ms, e.island, e.version});
            ++s.edits;
            last_desired = e.t_ms;
            s.converged_ms = -1; // a newer edit: convergence is measured again
            break;
        case TraceKind::Committed:
            ++s.commits;
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
            ++s.generations;
            for (const std::size_t i : awaiting_generation) {
                edits[i].generation = e.version;
            }
            awaiting_generation.clear();
            break;
        case TraceKind::ChunksReady:
            // Per island: one island's readiness is never another island's latency (2d).
            for (Edit& d : edits) {
                if (d.island == e.island && d.generation == e.version && d.t1 >= 0 && d.t2 < 0 &&
                    !d.offscreen && e.t_ms >= d.t1) {
                    d.t2 = e.t_ms;
                }
            }
            break;
        case TraceKind::NoVisibleChange:
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
            break;
        }
    }
    std::vector<double> object, visible;
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
          << (s.run_status == 0 ? " (succeeded)" : " (FAILED)") << "\n";
    } else {
        o << "run=INCOMPLETE (no run_end event: the run stopped or the trace was cut)\n";
    }
    o << "edits=" << s.edits << " object_ready=" << s.object_ready
      << " visible_ready=" << s.visible_ready << " offscreen=" << s.offscreen
      << " coalesced=" << s.coalesced << " unfinished=" << s.unfinished << "\n";
    o << "admitted=" << s.admitted << " deferred=" << s.deferred << " built=" << s.built
      << " failed=" << s.failed << " superseded=" << s.superseded << " dropped=" << s.dropped
      << " requeued=" << s.requeued << " commits=" << s.commits << " generations=" << s.generations
      << " reclaimed=" << s.reclaimed << "\n";
    line(o, "object_ready", s.object_ms);
    line(o, "visible_cpu_chunks_ready", s.visible_ms);
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

} // namespace anno::app
