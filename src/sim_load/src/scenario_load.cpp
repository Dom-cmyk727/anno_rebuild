// The game-directory side of the scenario load (docs/sim/state.md §10, S14 stage 2).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <anno/sim_load/scenario_load.hpp>

namespace anno::sim_load {

namespace fs = std::filesystem;

namespace {

std::optional<std::vector<std::uint8_t>> read_bytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

} // namespace

IslandTemplates scan_island_templates(const fs::path& game_dir) {
    IslandTemplates out;
    const fs::path root = game_dir / "Scenes" / "Islands";
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        return out;
    }
    // D AnnoScene 0x10007c50: every directory below Scenes\Islands except . and ..,
    // every file whose name passes the filter and for which GetAnnoInselFileData
    // succeeds. The candidate order is the resolver's (Rules::island_candidate_order);
    // sorting here only makes the list the same on every file system.
    std::vector<std::pair<std::string, fs::path>> found;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        std::error_code fe;
        if (!it->is_regular_file(fe)) {
            continue;
        }
        const std::string file = it->path().filename().string();
        if (!sim::link_candidate_name(file)) {
            continue;
        }
        const std::string rel = fs::relative(it->path(), root, fe).generic_string();
        if (fe) {
            continue;
        }
        found.emplace_back(sim::ascii_upper(rel), it->path());
    }
    std::sort(found.begin(), found.end());
    for (auto& [name, path] : found) {
        const std::optional<std::vector<std::uint8_t>> bytes = read_bytes(path);
        if (!bytes) {
            continue;
        }
        std::optional<sim::TemplateSummary> s = sim::template_summary(*bytes, name);
        if (!s) {
            continue;
        }
        out.summaries.push_back(std::move(*s));
        out.files.push_back(path);
    }
    return out;
}

formats::Result<ResolvedScenario, std::string>
load_scenario_with_links(formats::Bytes file, const IslandTemplates& templates,
                         const sim::TypeRegistry& types, const sim::SimConfig& config,
                         const sim::Rules& rules) {
    auto loaded = sim::load_scenario(file, types, config);
    if (!loaded) {
        return loaded.error();
    }
    ResolvedScenario out{std::move(loaded).value(), {}, 0};
    out.seed_before = out.loaded.spec.seed;
    auto res = sim::resolve_scenario_links(out.loaded, templates.summaries, config, rules);
    if (!res) {
        return res.error();
    }
    out.resolution = std::move(res).value();
    std::vector<std::vector<std::uint8_t>> bytes(out.loaded.links.size());
    std::vector<formats::Bytes> views(out.loaded.links.size());
    for (std::size_t i = 0; i < out.resolution.links.size(); ++i) {
        const std::optional<std::uint32_t> c = out.resolution.links[i].candidate;
        if (!c.has_value()) {
            continue;
        }
        const fs::path& p = templates.files[*c];
        std::optional<std::vector<std::uint8_t>> b = read_bytes(p);
        if (!b) {
            return "links[" + std::to_string(i) + "]: cannot read " + p.string();
        }
        bytes[i] = std::move(*b);
        views[i] = formats::Bytes(bytes[i]);
    }
    if (auto e = sim::place_island_links(out.loaded, out.resolution, views, types, config)) {
        return *e;
    }
    return out;
}

} // namespace anno::sim_load
