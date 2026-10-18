// Golden hashes of the synthetic scenarios (docs/sim/state.md §14.3).
//
// fixtures/synthetic/sim/goldens.txt has one line per golden:
//     <name> <stage> <stream_version> <value as 16 hex digits>
// ('#' starts a comment). A golden carries the stage and the stream version
// it was made at; a test compares only goldens whose version equals
// kStateHashVersion and fails a stale one with "re-baseline" instead of a
// bare mismatch, and the stage it names must be the stage the test passes. A
// golden changes only with ANNO_UPDATE_GOLDEN=1: the test then does not fail
// but prints the line to paste, and the commit message states which
// field-list or rule change caused it. Update mode is refused when the CI
// environment variable is set, so a stray ANNO_UPDATE_GOLDEN can never turn
// the golden checks of a CI run into passes.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <anno/sim/world.hpp>

namespace anno::test {

struct Golden {
    std::string name;
    std::string stage;
    std::uint32_t version = 0;
    std::uint64_t value = 0;
};

inline std::filesystem::path goldens_path() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "goldens.txt";
}

inline std::vector<Golden> read_goldens() {
    std::vector<Golden> out;
    std::ifstream in(goldens_path());
    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line.erase(hash);
        }
        std::istringstream ss(line);
        Golden g;
        std::string value;
        if (ss >> g.name >> g.stage >> g.version >> value) {
            g.value = std::stoull(value, nullptr, 16);
            out.push_back(g);
        }
    }
    return out;
}

inline std::string hex16(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

/// Whether ANNO_UPDATE_GOLDEN=1 is set (a difference is then printed, not failed).
inline bool update_requested() {
    const char* upd = std::getenv("ANNO_UPDATE_GOLDEN");
    return upd != nullptr && std::string(upd) == "1";
}

/// Whether the run is a CI run (the CI environment variable is set and not
/// empty, as GitHub Actions does).
inline bool in_ci() {
    const char* ci = std::getenv("CI");
    return ci != nullptr && *ci != '\0';
}

/// Whether differences are printed instead of failed: requested and not CI.
inline bool update_mode() { return update_requested() && !in_ci(); }

/// Empty when `value` is the golden `name` at the current stream version and
/// stage; otherwise the reason. With ANNO_UPDATE_GOLDEN=1 (outside CI) a
/// difference is not an error: the line to put into goldens.txt is returned in
/// `update_line`.
inline std::string check_golden(const std::string& name, const std::string& stage,
                                std::uint64_t value, std::string& update_line) {
    update_line =
        name + " " + stage + " " + std::to_string(sim::kStateHashVersion) + " " + hex16(value);
    if (update_requested() && in_ci()) {
        return "ANNO_UPDATE_GOLDEN is set in a CI run (CI is set): refused, unset it";
    }
    const bool update = update_mode();
    for (const Golden& g : read_goldens()) {
        if (g.name != name) {
            continue;
        }
        if (g.stage != stage) {
            return update ? ""
                          : "golden '" + name + "' belongs to stage " + g.stage +
                                " in goldens.txt, the test says stage " + stage;
        }
        if (g.version != sim::kStateHashVersion) {
            return update ? ""
                          : "golden '" + name + "' was made at stream version " +
                                std::to_string(g.version) + ", this build is " +
                                std::to_string(sim::kStateHashVersion) +
                                ": re-baseline with ANNO_UPDATE_GOLDEN=1";
        }
        if (g.value != value) {
            return update ? ""
                          : "golden '" + name + "' is " + hex16(g.value) + ", got " + hex16(value) +
                                " (ANNO_UPDATE_GOLDEN=1 prints the new line)";
        }
        return "";
    }
    return update ? "" : "golden '" + name + "' is missing from " + goldens_path().string();
}

} // namespace anno::test
