// Raw events of the picture's CPU pipeline and their aggregation (see the header).
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

#include <anno/app/picture_trace.hpp>

namespace anno::app {

namespace {

constexpr std::array<std::string_view, 17> kNames = {
    "desired",           "admitted", "deferred",  "started",     "built",      "failed",
    "superseded",        "dropped",  "requeued",  "committed",   "generation", "chunks_ready",
    "no_visible_change", "retired",  "reclaimed", "drain_start", "drain_end"};

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
    std::vector<std::size_t> awaiting_generation;
    std::map<std::uint64_t, double> retired_at;
    std::vector<double> reclaim;
    for (const TraceEvent& e : events) {
        s.end_ms = std::max(s.end_ms, e.t_ms);
        switch (e.kind) {
        case TraceKind::Desired:
            edits.push_back(Edit{e.t_ms, e.island, e.version});
            ++s.edits;
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
            for (Edit& d : edits) {
                if (d.generation == e.version && d.t1 >= 0 && d.t2 < 0 && !d.offscreen &&
                    e.t_ms >= d.t1) {
                    d.t2 = e.t_ms;
                }
            }
            break;
        case TraceKind::NoVisibleChange:
            for (Edit& d : edits) {
                if (d.generation == e.version && d.t2 < 0) {
                    d.offscreen = true;
                }
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
            break;
        case TraceKind::Deferred:
            ++s.deferred;
            break;
        case TraceKind::Built:
            ++s.built;
            break;
        case TraceKind::Failed:
            ++s.failed;
            break;
        case TraceKind::Superseded:
            ++s.superseded;
            break;
        case TraceKind::Dropped:
            ++s.dropped;
            break;
        case TraceKind::Requeued:
            ++s.requeued;
            break;
        case TraceKind::Started:
        case TraceKind::DrainStart:
        case TraceKind::DrainEnd:
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
    std::vector<TraceEvent> out;
    std::size_t line_no = 0;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        const std::string_view row = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        ++line_no;
        if (line_no == 1 || row.empty()) {
            continue; // the header
        }
        std::array<std::string_view, 4> f{};
        std::string_view rest = row;
        for (std::size_t i = 0; i < f.size(); ++i) {
            const std::size_t tab = rest.find('\t');
            f[i] = rest.substr(0, tab);
            rest = tab == std::string_view::npos ? std::string_view{} : rest.substr(tab + 1);
        }
        TraceEvent e;
        const std::string t(f[0]);
        char* stop = nullptr;
        e.t_ms = std::strtod(t.c_str(), &stop);
        const auto kind = std::find(kNames.begin(), kNames.end(), f[1]);
        unsigned long long version = 0;
        const auto island_ok =
            std::from_chars(f[2].data(), f[2].data() + f[2].size(), e.island).ec == std::errc{};
        const auto version_ok =
            std::from_chars(f[3].data(), f[3].data() + f[3].size(), version).ec == std::errc{};
        if (stop == t.c_str() || kind == kNames.end() || !island_ok || !version_ok) {
            return "trace line " + std::to_string(line_no) + ": cannot parse '" + std::string(row) +
                   "'";
        }
        e.kind = static_cast<TraceKind>(kind - kNames.begin());
        e.version = version;
        out.push_back(e);
    }
    return out;
}

std::string format_summary(const TraceSummary& s) {
    std::ostringstream o;
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
    char buf[160];
    std::snprintf(buf, sizeof buf, "max_age_ms=%.3f last_commit_ms=%.3f end_ms=%.3f\n",
                  s.max_age_ms, s.last_commit_ms, s.end_ms);
    o << buf;
    return o.str();
}

} // namespace anno::app
