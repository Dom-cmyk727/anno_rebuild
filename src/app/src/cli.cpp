#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>

#include <anno/app/cli.hpp>

namespace anno::app {

namespace fs = std::filesystem;

namespace {

template <class T> std::optional<T> number(std::string_view s) {
    if (!s.empty() && s.front() == '+') {
        s.remove_prefix(1);
        if (!s.empty() && s.front() == '-') {
            return std::nullopt;
        }
    }
    T v{};
    const auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (s.empty() || ec != std::errc() || p != s.data() + s.size()) {
        return std::nullopt;
    }
    return v;
}

std::optional<std::pair<int, int>> pair_of(std::string_view s, char sep) {
    const auto k = s.find(sep);
    if (k == std::string_view::npos) {
        return std::nullopt;
    }
    const auto a = number<int>(s.substr(0, k)), b = number<int>(s.substr(k + 1));
    if (!a || !b) {
        return std::nullopt;
    }
    return std::pair{*a, *b};
}

} // namespace

fs::path utf8_path(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string to_utf8(const fs::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::optional<fs::path> env_path(const char* name) {
#ifdef _WIN32
    const std::wstring wide(name, name + std::char_traits<char>::length(name));
    const wchar_t* v = _wgetenv(wide.c_str());
#else
    const char* v = std::getenv(name);
#endif
    if (v == nullptr || *v == 0) {
        return std::nullopt;
    }
    return fs::path(v);
}

std::optional<Args> parse_args(const std::vector<std::string>& args) {
    Args out;
    bool have_file = false;
    for (std::size_t i = 0; i < args.size(); i += 2) {
        if (i + 1 >= args.size()) {
            return std::nullopt;
        }
        const std::string& a = args[i];
        const std::string& v = args[i + 1];
        if (a == "--view") {
            out.file = utf8_path(v);
            have_file = true;
        } else if (a == "--game-dir") {
            out.game_dir = utf8_path(v);
        } else if (a == "--group") {
            const auto g = number<std::size_t>(v);
            if (!g || *g >= render::kGroups) {
                return std::nullopt;
            }
            out.group = g;
        } else if (a == "--png") {
            out.png = utf8_path(v);
        } else if (a == "--time") {
            const auto t = number<std::uint64_t>(v);
            if (!t) {
                return std::nullopt;
            }
            out.time = *t;
        } else if (a == "--at") {
            const auto at = pair_of(v, ',');
            if (!at || at->first < -kMaxAtTile || at->first > kMaxAtTile ||
                at->second < -kMaxAtTile || at->second > kMaxAtTile) {
                return std::nullopt;
            }
            out.at = at;
        } else if (a == "--size") {
            const auto s = pair_of(v, 'x');
            if (!s || s->first <= 0 || s->second <= 0 || s->first > kMaxSizePx ||
                s->second > kMaxSizePx) {
                return std::nullopt;
            }
            out.size = *s;
        } else if (a == "--water") {
            if (v == "original") {
                out.water = render::WaterStyle::Original;
            } else if (v == "enhanced") {
                out.water = render::WaterStyle::Enhanced;
            } else {
                return std::nullopt;
            }
        } else if (a == "--river-flow") {
            if (v == "legacy") {
                out.river_flow = render::RiverFlowMode::Legacy;
            } else if (v == "flow") {
                out.river_flow = render::RiverFlowMode::Flow;
            } else {
                return std::nullopt;
            }
        } else if (a == "--river-gain") {
            const auto g = number<float>(v);
            if (!g || !(*g >= 0.0F) || *g > 1000.0F) {
                return std::nullopt;
            }
            out.river_gain = *g;
        } else if (a == "--river-gap-patch") {
            const auto g = number<float>(v);
            if (!g || !(*g >= 0.0F) || *g > 64.0F) {
                return std::nullopt;
            }
            out.river_gap_patch = *g;
        } else if (a == "--river-speed") {
            if (v == "median") {
                out.river_speed = render::river::RiverConfig::Reference::MedianCell;
            } else if (v == "fastest") {
                out.river_speed = render::river::RiverConfig::Reference::FastestSection;
            } else {
                return std::nullopt;
            }
        } else if (a == "--lookahead-chunks") {
            const auto n = number<std::size_t>(v);
            if (!n || *n > kMaxLookaheadChunks) {
                return std::nullopt;
            }
            out.lookahead_chunks = *n;
        } else if (a == "--zoom-mode") {
            out.zoom.mode_explicit = true;
            if (v == "stepped") {
                out.zoom.mode = ZoomMode::Stepped;
            } else if (v == "continuous") {
                out.zoom.mode = ZoomMode::Continuous;
            } else {
                return std::nullopt;
            }
        } else if (a == "--zoom-step") {
            const auto f = number<double>(v);
            if (!f || !(*f >= kMinZoomStep) || *f > kMaxZoomStep) {
                return std::nullopt;
            }
            out.zoom.step = *f;
        } else if (a == "--min-zoom") {
            const auto f = number<double>(v);
            if (!f || !(*f >= kMinMinZoom) || *f > kMaxMinZoom) {
                return std::nullopt;
            }
            out.zoom.min_zoom = *f;
        } else if (a == "--max-derived-levels") {
            const auto n = number<unsigned>(v);
            if (!n || *n > kMaxDerivedLevels) {
                return std::nullopt;
            }
            out.zoom.max_derived_levels = *n;
        } else if (a == "--zoom-ease") {
            const auto ms = number<unsigned>(v);
            if (!ms || *ms > kMaxZoomEaseMs) {
                return std::nullopt;
            }
            out.zoom.ease_ms = *ms;
        } else if (a == "--renderer") {
            if (v == "auto") {
                out.renderer = Renderer::Auto;
            } else if (v == "gpu") {
                out.renderer = Renderer::Gpu;
            } else if (v == "cpu") {
                out.renderer = Renderer::Cpu;
            } else {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    if (!have_file) {
        return std::nullopt;
    }
    return out;
}

} // namespace anno::app
