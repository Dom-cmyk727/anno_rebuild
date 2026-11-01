// Shared helpers of the route tests (MAP-NAV-2 package C): fixture parsing, the route walk legality
// check (the C++ twin of tools/ship_nav_ref.py path_legality), run formatting.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <anno/sim/route.hpp>

namespace route_test {

namespace sim = anno::sim;

inline std::filesystem::path nav_fixture(const std::string& name) {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "nav" / name;
}

inline std::string trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return {};
    }
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(s);
    while (std::getline(in, cur, sep)) {
        out.push_back(trim(cur));
    }
    return out;
}

inline std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string w;
    while (in >> w) {
        out.push_back(w);
    }
    return out;
}

/// Non-comment, non-empty lines of a fixture file (throws when it cannot be read).
inline std::vector<std::string> fixture_lines(const std::string& name) {
    std::ifstream f(nav_fixture(name));
    if (!f) {
        throw std::runtime_error("cannot read fixture " + nav_fixture(name).string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (!line.empty() && line[0] != '#') {
            lines.push_back(line);
        }
    }
    return lines;
}

/// Decimal "40.5" / "-2.5" to Q24.8, rounding half away from zero (a goal that is exact in Q24.8 - tile
/// centres, k/256 - converts exactly; only those reproduce the f32 emulation bit for bit).
inline std::int32_t decimal_to_q8(const std::string& s) {
    const bool neg = !s.empty() && s[0] == '-';
    const std::string t = neg ? s.substr(1) : s;
    const auto dot = t.find('.');
    std::int64_t whole = std::stoll(t.substr(0, dot));
    std::int64_t nano = 0;
    if (dot != std::string::npos) {
        std::string frac = t.substr(dot + 1);
        frac.resize(9, '0');
        nano = std::stoll(frac.substr(0, 9));
    }
    const std::int64_t q8 = (whole * 1000000000 + nano) * 256;
    const std::int64_t r = (q8 + 500000000) / 1000000000;
    return static_cast<std::int32_t>(neg ? -r : r);
}

inline bool q8_exact(const std::string& s) {
    const auto dot = s.find('.');
    if (dot == std::string::npos) {
        return true;
    }
    std::string frac = s.substr(dot + 1);
    frac.resize(9, '0');
    return (std::stoll(frac) * 256) % 1000000000 == 0;
}

/// floor of a decimal (the fixtures' start points are positive).
inline std::int32_t decimal_floor(const std::string& s) {
    return static_cast<std::int32_t>(std::floor(std::stod(s)));
}

inline const char* dir_name(std::uint8_t d) {
    static const char* const names[8] = {"W", "SW", "S", "SE", "E", "NE", "N", "NW"};
    return names[d & 7u];
}

inline std::string runs_to_string(const std::vector<sim::RouteRun>& runs) {
    if (runs.empty()) {
        return "-";
    }
    std::string out;
    for (const auto& r : runs) {
        if (!out.empty()) {
            out += ' ';
        }
        out += dir_name(r.dir);
        out += std::to_string(r.steps);
    }
    return out;
}

inline std::int32_t dir_code(const std::string& name) {
    for (std::int32_t d = 0; d < 8; ++d) {
        if (name == dir_name(static_cast<std::uint8_t>(d))) {
            return d;
        }
    }
    return -1;
}

/// Parses "E10 NE3" (or "-") to runs.
inline std::vector<sim::RouteRun> parse_runs(const std::vector<std::string>& toks) {
    std::vector<sim::RouteRun> out;
    for (const auto& t : toks) {
        if (t == "-") {
            continue;
        }
        std::size_t i = 0;
        while (i < t.size() && std::isalpha(static_cast<unsigned char>(t[i])) != 0) {
            ++i;
        }
        out.push_back({static_cast<std::uint8_t>(dir_code(t.substr(0, i))),
                       static_cast<std::uint8_t>(std::stoi(t.substr(i)))});
    }
    return out;
}

inline void add_land(sim::TileMapSource& m, const std::vector<std::string>& rect_tokens) {
    for (const auto& tok : rect_tokens) {
        if (tok == "-" || tok == "land") {
            continue;
        }
        const auto p = split(tok, ',');
        m.fill_rect(std::stoi(p[0]), std::stoi(p[1]), std::stoi(p[2]), std::stoi(p[3]), false);
    }
}

struct Walk {
    std::string problem; ///< empty when legal
    std::int32_t end_x = 0, end_z = 0;
};

/// Expands the runs into single steps and checks, independently of the router: run syntax and cap,
/// the map bounds, that every entered tile is water (the start tile is the only land tile that may
/// be left), and that a diagonal step has BOTH side tiles enterable (no corner cutting).
inline Walk walk_runs(const sim::TileSource& src, std::int32_t sx, std::int32_t sz,
                      const std::vector<sim::RouteRun>& runs, std::uint32_t cap) {
    static const std::array<std::array<int, 2>, 8> dxz = {
        {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}}};
    Walk w;
    std::int32_t x = sx;
    std::int32_t z = sz;
    const auto enterable = [&](std::int32_t tx, std::int32_t tz) {
        return (tx == sx && tz == sz) || src.tile_is_water(tx, tz);
    };
    for (const auto& r : runs) {
        if (r.dir > 7 || r.steps < 1 || r.steps > cap) {
            w.problem = "bad run";
            break;
        }
        for (std::uint32_t i = 0; i < r.steps; ++i) {
            const int dx = dxz[r.dir][0];
            const int dz = dxz[r.dir][1];
            const std::int32_t nx = x + dx;
            const std::int32_t nz = z + dz;
            if (nx < 0 || nz < 0 || nx >= src.width() || nz >= src.height()) {
                w.problem = "step leaves the map";
            } else if (!src.tile_is_water(nx, nz)) {
                w.problem = "step enters land";
            } else if (dx != 0 && dz != 0 && !(enterable(x + dx, z) && enterable(x, z + dz))) {
                w.problem = "diagonal step cuts a corner";
            }
            x = nx;
            z = nz;
        }
    }
    w.end_x = x;
    w.end_z = z;
    return w;
}

} // namespace route_test
