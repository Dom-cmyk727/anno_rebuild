// The scenario corpus (docs/sim/state.md §14.5, stage S14): every Scenes/**/*.sze of
// the game directory through the adapter, World::create, five simulated minutes and
// check_invariants; twice from the same spec, save/load/save. The counts per file are
// held against fixtures/synthetic/sim/scenario_corpus.txt (counts only, no content of
// the files). Needs ANNO1503_GAME_DIR; skipped with the one message otherwise. The
// output names files and counts only.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/sim/scenario_formats.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/parallel.hpp>

namespace sim = anno::sim;
namespace fs = std::filesystem;

namespace {

/// The run of the corpus case: 1 500 advances of 100 ms = 150 000 ticks = 300 s.
constexpr int kAdvances = 1500;
constexpr std::uint32_t kStepMs = 100;

fs::path expectations_path() {
    return fs::path(ANNO_FIXTURES_DIR) / "sim" / "scenario_corpus.txt";
}

std::vector<fs::path> scenario_files(const fs::path& game) {
    std::vector<fs::path> out;
    for (const auto& e : fs::recursive_directory_iterator(game / "Scenes")) {
        if (!e.is_regular_file()) {
            continue;
        }
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".sze") {
            out.push_back(e.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// The file's name relative to Scenes, with forward slashes.
std::string rel_name(const fs::path& game, const fs::path& p) {
    return fs::relative(p, game / "Scenes").generic_string();
}

/// One line of the expectations file: the counts of a loaded scenario.
std::string counts_line(const std::string& name, const sim::ScenarioCounts& c) {
    std::ostringstream o;
    o << name << " islands=" << c.islands << " links=" << c.links << " players=" << c.players
      << " cities=" << c.cities << " buildings=" << c.buildings
      << " tile_objects=" << c.tile_objects << " deposits=" << c.deposits;
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        const auto it = c.warnings.find(k);
        o << " " << sim::warning_kind_name(k) << "=" << (it == c.warnings.end() ? 0 : it->second);
    }
    return o.str();
}

struct FileResult {
    std::string name;
    std::string load_error, create_error, invariants, save_error;
    std::string line;
    sim::ScenarioCounts counts;
    std::uint64_t a14 = 0;             // islands whose w or h is not a multiple of chunk_edge
    std::uint64_t prod2_buildings = 0; // BuildingWithoutCity: the PROD2 buildings of the file
    std::uint64_t prod2_unknown = 0;   // UnknownObject warnings on PROD2 items
    bool deterministic = false, save_identity = false;
    std::uint64_t hash = 0;
};

std::shared_ptr<const sim::TypeRegistry> original_registry(const fs::path& game) {
    const auto buf = anno::test::read_file(game / "Anlagen.dat");
    auto file = anno::formats::anlagen::AnlagenFile::parse(buf);
    REQUIRE(file);
    auto r =
        sim::TypeRegistry::build(file.value().entries, sim::original_goods(), sim::SimConfig{});
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value();
}

/// Load, create and run one scenario; no doctest assertion (parallel worker).
FileResult run_file(const fs::path& game, const fs::path& p,
                    const std::shared_ptr<const sim::TypeRegistry>& types, int advances) {
    FileResult r;
    r.name = rel_name(game, p);
    const auto buf = anno::test::read_file(p);
    const sim::SimConfig cfg;
    auto loaded = sim::load_scenario(buf, *types, cfg);
    if (!loaded) {
        r.load_error = loaded.error();
        return r;
    }
    r.counts = sim::count(loaded.value());
    r.line = counts_line(r.name, r.counts);
    for (const sim::ScenarioSpec::IslandSpec& is : loaded.value().spec.islands) {
        if (is.w % cfg.chunk_edge != 0 || is.h % cfg.chunk_edge != 0) {
            ++r.a14;
        }
    }
    for (const sim::ScenarioWarning& w : loaded.value().warnings) {
        r.prod2_buildings += w.kind == sim::ScenarioWarningKind::BuildingWithoutCity ? 1 : 0;
        r.prod2_unknown +=
            w.kind == sim::ScenarioWarningKind::UnknownObject && w.record == "PROD2" ? 1 : 0;
    }
    auto run = [&](std::uint64_t& hash) -> std::string {
        auto w = sim::World::create(loaded.value().spec, types, cfg);
        if (!w) {
            return w.error();
        }
        sim::World world = std::move(w).value();
        for (int i = 0; i < advances; ++i) {
            world.advance(kStepMs);
        }
        r.invariants = world.check_invariants();
        hash = world.state_hash();
        const std::vector<std::uint8_t> saved = world.save();
        auto back = sim::World::load(saved, types, {});
        if (!back) {
            r.save_error = back.error();
        } else {
            r.save_identity = back.value().save() == saved;
        }
        return {};
    };
    std::uint64_t h1 = 0, h2 = 0;
    r.create_error = run(h1);
    if (r.create_error.empty()) {
        r.create_error = run(h2);
        r.deterministic = h1 == h2;
        r.hash = h1;
    }
    return r;
}

std::map<std::string, std::string> read_expectations() {
    std::map<std::string, std::string> out;
    std::ifstream in(expectations_path());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        out[line.substr(0, line.find(' '))] = line;
    }
    return out;
}

bool update_requested() {
    const char* u = std::getenv("ANNO_UPDATE_GOLDEN");
    const char* ci = std::getenv("CI");
    return u != nullptr && std::string(u) == "1" && (ci == nullptr || std::string(ci).empty());
}

} // namespace

TEST_CASE("scenario corpus: every .sze loads, creates and runs five minutes") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    REQUIRE(files.size() == 50); // P, sze.md "Files covered"
    const auto results = anno::test::parallel_indexed(
        files.size(), [&](std::size_t i) { return run_file(*dir, files[i], types, kAdvances); },
        anno::test::parallel_options());

    std::map<sim::ScenarioWarningKind, std::uint64_t> totals;
    std::uint64_t islands = 0, links = 0, cities = 0, buildings = 0, objects = 0, a14 = 0,
                  with_island = 0, links_only = 0;
    std::vector<std::string> lines;
    for (const FileResult& r : results) {
        INFO(r.name);
        CHECK(r.load_error == "");
        CHECK(r.create_error == "");
        CHECK(r.invariants == "");
        CHECK(r.save_error == "");
        CHECK(r.deterministic);
        CHECK(r.save_identity);
        // Every PROD2 id resolves in the registry.
        CHECK(r.prod2_unknown == 0);
        lines.push_back(r.line);
        islands += r.counts.islands;
        links += r.counts.links;
        cities += r.counts.cities;
        buildings += r.counts.buildings;
        objects += r.counts.tile_objects;
        a14 += r.a14;
        with_island += r.counts.islands > 0 ? 1 : 0;
        links_only += r.counts.islands == 0 && r.counts.links > 0 ? 1 : 0;
        for (const auto& [k, n] : r.counts.warnings) {
            totals[k] += n;
        }
    }
    std::cout << "scenario corpus: " << results.size() << " files, " << with_island
              << " with at least one island, " << links_only << " with links only; islands "
              << islands << ", links " << links << ", cities " << cities << ", buildings "
              << buildings << ", tile objects " << objects << "\n";
    std::cout << "scenario corpus: islands not a multiple of chunk_edge (A14/F20): " << a14 << "\n";
    for (const sim::ScenarioWarningKind k : sim::all_warning_kinds()) {
        std::cout << "scenario corpus: warnings " << sim::warning_kind_name(k) << " " << totals[k]
                  << "\n";
    }

    // The per-file counts against the committed expectations.
    if (update_requested()) {
        std::ofstream out(expectations_path(), std::ios::binary);
        out << "# The scenario corpus through the S14 adapter: per file (relative to Scenes) the\n"
               "# counts of the loaded ScenarioSpec and of its warnings per kind. Counts only, no\n"
               "# content of the files. Rewritten by scenario_test.cpp with "
               "ANNO_UPDATE_GOLDEN=1.\n";
        for (const std::string& l : lines) {
            out << l << "\n";
        }
        MESSAGE("scenario_corpus.txt rewritten");
        return;
    }
    const std::map<std::string, std::string> want = read_expectations();
    CHECK(want.size() == results.size());
    for (const std::string& l : lines) {
        const std::string name = l.substr(0, l.find(' '));
        INFO(name);
        const auto it = want.find(name);
        REQUIRE_MESSAGE(it != want.end(), "not in scenario_corpus.txt (ANNO_UPDATE_GOLDEN=1)");
        CHECK(it->second == l);
    }
}

TEST_CASE("scenario corpus: the ten scenarios with the most PROD2 buildings run 20000 ticks more") {
    // §15 S14: the 5 add-on and the 5 base scenarios with the most buildings run longer;
    // under the asan preset this is the UBSan run. No PROD2 building is placed in stage 1
    // (no tile owners decoded, OQ-141), so "most buildings" ranks the PROD2 building
    // items of the file (the BuildingWithoutCity warnings).
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto types = original_registry(*dir);
    const std::vector<fs::path> files = scenario_files(*dir);
    std::vector<std::pair<std::uint64_t, std::size_t>> base, addon;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto buf = anno::test::read_file(files[i]);
        auto l = sim::load_scenario(buf, *types, sim::SimConfig{});
        REQUIRE(l);
        const auto c = sim::count(l.value());
        const auto it = c.warnings.find(sim::ScenarioWarningKind::BuildingWithoutCity);
        const std::uint64_t n = it == c.warnings.end() ? 0 : it->second;
        (l.value().spec.settings.prop14 == 3 ? addon : base).push_back({n, i});
    }
    auto top5 = [](auto& v) {
        std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        v.resize(std::min<std::size_t>(v.size(), 5));
    };
    top5(base);
    top5(addon);
    std::vector<std::size_t> picked;
    for (const auto& [n, i] : base) {
        picked.push_back(i);
    }
    for (const auto& [n, i] : addon) {
        picked.push_back(i);
    }
    REQUIRE(picked.size() == 10);
    // 20 000 ticks more than the corpus case: 1 500 + 400 advances of 100 ms.
    const auto results = anno::test::parallel_indexed(
        picked.size(),
        [&](std::size_t k) { return run_file(*dir, files[picked[k]], types, kAdvances + 400); },
        anno::test::parallel_options());
    for (const FileResult& r : results) {
        INFO(r.name);
        std::cout << "scenario long run: " << r.name << "\n";
        CHECK(r.create_error == "");
        CHECK(r.invariants == "");
        CHECK(r.deterministic);
        CHECK(r.save_identity);
    }
}
