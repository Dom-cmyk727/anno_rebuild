// Named simulation pins (docs/sim/state.md §14.3). Extend this single mechanism.
// Format: name stage stream_version value_as_16_hex_digits; '#' starts a comment.
// Record mode writes changed self-baselines atomically, sorted by name. Oracle-owned
// pins are always comparisons. CI refuses record mode before touching any file.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <anno/sim/world.hpp>

namespace anno::test {
struct Golden {
    std::string name;
    std::string stage;
    std::uint32_t version = 0;
    std::uint64_t value = 0;
};
inline std::filesystem::path current_replay_fixture_path(const char* name) {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "current" / name;
}
inline std::filesystem::path goldens_path() {
    return current_replay_fixture_path("goldens.txt");
}
inline std::string hex16(std::uint64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(v));
    return buf;
}
inline bool update_requested() {
    const char* value = std::getenv("ANNO_UPDATE_GOLDEN");
    return value != nullptr && std::string(value) == "1";
}
inline bool in_ci() {
    const char* value = std::getenv("CI");
    return value != nullptr && *value != '\0';
}
inline bool update_mode() {
    return update_requested() && !in_ci();
}

// The retained Python stream transcription owns these checkpoints. Other oracle_*
// pins come from independent primitive/registry transcriptions or retained logs.
inline bool oracle_owned(const std::string& name) {
    return name.starts_with("oracle_") || name == "empty_world" || name == "empty_registry" ||
           name.starts_with("g0_") || name.starts_with("g1_players_") ||
           name.starts_with("s5_islands_") || name.starts_with("s6_g1_");
}
inline std::vector<Golden> read_goldens(const std::filesystem::path& path = goldens_path()) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot read pins: " + path.string());
    std::vector<Golden> out;
    std::set<std::string> names;
    for (std::string line; std::getline(in, line);) {
        line.erase(line.find('#') == std::string::npos ? line.size() : line.find('#'));
        std::istringstream ss(line);
        Golden g;
        if (!(ss >> g.name))
            continue;
        std::string value, extra;
        if (!(ss >> g.stage >> g.version >> value) || (ss >> extra) || value.size() != 16 ||
            value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos ||
            !names.insert(g.name).second)
            throw std::runtime_error("malformed/duplicate pin: " + g.name);
        g.value = std::stoull(value, nullptr, 16);
        out.push_back(g);
    }
    if (!in.eof())
        throw std::runtime_error("error reading pins: " + path.string());
    return out;
}

// Cross-process serialization, including independent test executors recording at
// once. Never remove someone else's lock; an abandoned lock requires inspection.
class PinLock {
    std::filesystem::path path_;

  public:
    explicit PinLock(const std::filesystem::path& path) : path_(path.string() + ".lock") {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        for (;;) {
            std::error_code ec;
            if (std::filesystem::create_directory(path_, ec))
                break;
            if (ec || std::chrono::steady_clock::now() >= end)
                throw std::runtime_error("pins lock unavailable: " + path_.string());
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    PinLock(const PinLock&) = delete;
    PinLock& operator=(const PinLock&) = delete;
    ~PinLock() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
};
inline void write_goldens(const std::filesystem::path& path, std::vector<Golden> pins) {
    std::sort(pins.begin(), pins.end(),
              [](const Golden& a, const Golden& b) { return a.name < b.name; });
    std::ifstream old(path);
    std::string comments;
    for (std::string line; std::getline(old, line);)
        if (line.starts_with("#"))
            comments += line + "\n";
    old.close(); // release the destination handle before Windows replacement
    const std::filesystem::path tmp = path.string() + ".tmp";
    try {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            throw std::runtime_error("cannot write pins temporary file");
        out << comments;
        for (const auto& g : pins)
            out << g.name << ' ' << g.stage << ' ' << g.version << ' ' << hex16(g.value) << '\n';
        out.close();
        if (!out)
            throw std::runtime_error("cannot close pins temporary file");
        // Same-directory replacement: old complete file or new complete file;
        // never delete the destination first (including on Windows).
        std::filesystem::rename(tmp, path);
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw;
    }
}

// Explicit path/mode arguments let the contract exercise the real writer on a
// private self-authored file, without mutating repository pins or environment.
inline std::string check_golden_at(const std::filesystem::path& path, const std::string& name,
                                   const std::string& stage, std::uint64_t value,
                                   std::string& update_line, bool requested, bool ci) {
    update_line =
        name + " " + stage + " " + std::to_string(sim::kStateHashVersion) + " " + hex16(value);
    if (requested && ci)
        return "ANNO_UPDATE_GOLDEN is set in a CI run: refused, unset it";
    try {
        const auto check = [&](const std::vector<Golden>& pins) -> std::string {
            for (const auto& g : pins) {
                if (g.name != name)
                    continue;
                if (g.stage != stage)
                    return "golden '" + name + "' belongs to stage " + g.stage;
                if (g.version != sim::kStateHashVersion)
                    return "golden '" + name + "': re-baseline stale stream version " +
                           std::to_string(g.version);
                if (g.value != value)
                    return "golden '" + name + "' is " + hex16(g.value) + ", got " + hex16(value) +
                           ": re-baseline";
                return "";
            }
            return "golden '" + name + "' is missing from " + path.string();
        };
        // Serialize recorder reads too: Windows replacement must not race
        // another recorder's still-open destination handle.
        std::optional<PinLock> record_lock;
        if (requested)
            record_lock.emplace(path);
        auto pins = read_goldens(path);
        std::string problem = check(pins);
        if (problem.empty() || !requested)
            return problem;
        if (oracle_owned(name))
            return "independent oracle pin is read-only: " + problem;
        auto at =
            std::find_if(pins.begin(), pins.end(), [&](const Golden& g) { return g.name == name; });
        const Golden next{name, stage, sim::kStateHashVersion, value};
        if (at == pins.end())
            pins.push_back(next);
        else
            *at = next;
        write_goldens(path, std::move(pins));
        return "";
    } catch (const std::exception& error) {
        return error.what();
    }
}
inline std::string check_golden(const std::string& name, const std::string& stage,
                                std::uint64_t value, std::string& update_line) {
    return check_golden_at(goldens_path(), name, stage, value, update_line, update_requested(),
                           in_ci());
}
inline bool pin_matches(const std::string& name, std::uint64_t value) {
    std::string line;
    const auto problem = check_golden(name, "PIN", value, line);
    if (!problem.empty())
        std::cerr << problem << '\n';
    return problem.empty();
}
} // namespace anno::test
