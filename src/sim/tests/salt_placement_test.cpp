// Stream 135, X2: the mountain-class placement and the deposit binding of a player-built mine
// (docs/sim/salt-extraction.md sections 11-15). Layer 1 runs the hand-derived vectors through the
// engine; layer 2 builds in the original endless02 island 1 (owner 8) through PUBLIC commands.
// Needs ANNO1503_GAME_DIR for layer 2.
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/mine_source.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

#include "original_component.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace tutorial_town;

namespace {

// ---- layer 1: the vector file ------------------------------------------------------------

struct Vec {
    std::string kind, name, expect;
    std::uint32_t w = 0, h = 0, rot = 0, off = 0;
    std::vector<std::uint8_t> table;
    std::vector<std::vector<std::uint8_t>> heights;
    std::uint32_t amount = 0, goods = 0, flag = 1;
    std::uint32_t want_remaining = 0, want_flag = 1;
};

std::vector<std::string> split(const std::string& s, char c) {
    std::vector<std::string> out;
    std::string cur;
    for (const char ch : s) {
        if (ch == c) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<Vec> read_vectors() {
    std::vector<Vec> out;
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "mine_terrain_vectors.txt");
    REQUIRE_MESSAGE(in.good(), "mine_terrain_vectors.txt not found");
    std::string line;
    while (std::getline(in, line)) {
        line = line.substr(0, line.find('#'));
        const auto arrow = line.find("=>");
        if (arrow == std::string::npos) {
            continue;
        }
        std::istringstream lhs(line.substr(0, arrow)), rhs(line.substr(arrow + 2));
        Vec v;
        lhs >> v.kind >> v.name;
        std::string tok;
        while (lhs >> tok) {
            const auto eq = tok.find('=');
            const std::string k = tok.substr(0, eq), val = tok.substr(eq + 1);
            if (k == "w") {
                v.w = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "h") {
                v.h = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "rot") {
                v.rot = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "off") {
                v.off = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "amount") {
                v.amount = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "goods") {
                v.goods = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "flag") {
                v.flag = static_cast<std::uint32_t>(std::stoul(val));
            } else if (k == "table") {
                for (const auto& x : split(val, ',')) {
                    v.table.push_back(static_cast<std::uint8_t>(std::stoul(x)));
                }
            } else if (k == "heights") {
                for (const auto& row : split(val, '/')) {
                    std::vector<std::uint8_t> r;
                    for (const auto& x : split(row, ',')) {
                        r.push_back(static_cast<std::uint8_t>(std::stoul(x)));
                    }
                    v.heights.push_back(std::move(r));
                }
            }
        }
        if (v.kind == "deposit") {
            std::string a, b;
            rhs >> a >> b;
            v.want_remaining = static_cast<std::uint32_t>(std::stoul(a.substr(a.find('=') + 1)));
            v.want_flag = static_cast<std::uint32_t>(std::stoul(b.substr(b.find('=') + 1)));
        } else {
            rhs >> v.expect;
        }
        out.push_back(std::move(v));
    }
    return out;
}

std::vector<std::uint8_t> flat(const Vec& v) {
    std::vector<std::uint8_t> w;
    for (const auto& row : v.heights) {
        w.insert(w.end(), row.begin(), row.end());
    }
    return w;
}

/// A salt-mine-like type for the engine: footprint w x h, the table of the vector, flag word 0x3096.
sim::BuildingType mine_type(const Vec& v) {
    sim::BuildingType t;
    t.id = sim::TypeId{1203};
    t.prodtype = 3; // MINE
    t.cls = sim::RuntimeClass::Mine;
    t.width = v.w;
    t.height = v.h;
    t.flags_word = 0x3096;
    t.has_prod = true;
    t.prod.prodtype = "MINE";
    t.prod.kind = sim::BuildingClass::Mine;
    t.prod.outputs = {"SALZSTEIN"};
    t.highmap = v.table;
    return t;
}

/// An island holding exactly the vector's vertex window at its origin (one extra tile row and column
/// so the window's last row/column are real vertices, not the zero boundary).
sim::Island island_for(const Vec& v) {
    const std::uint32_t ww = v.rot % 2 == 0 ? v.w : v.h, hh = v.rot % 2 == 0 ? v.h : v.w;
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, ww + 1, hh + 1, 8));
    std::vector<std::uint8_t> height(std::size_t{ww + 1} * (hh + 1), 0);
    for (std::size_t z = 0; z < v.heights.size(); ++z) {
        for (std::size_t x = 0; x < v.heights[z].size(); ++x) {
            height[z * (ww + 1) + x] = v.heights[z][x];
        }
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    return is;
}

} // namespace

TEST_CASE("mountain helper: every terrain vector of the hand-derived file (integer rule)") {
    const std::vector<Vec> vs = read_vectors();
    std::size_t n = 0;
    for (const Vec& v : vs) {
        if (v.kind != "terrain") {
            continue;
        }
        INFO("vector " << v.name);
        const bool got = sim::site::mountain_helper_accepts(v.w, v.h, v.table,
                                                            static_cast<std::uint8_t>(v.rot),
                                                            flat(v), v.off);
        CHECK(got == (v.expect == "accept"));
        ++n;
    }
    CHECK(n == 40);
}

TEST_CASE("mountain placement: the vectors through placement_rule (lo from the window, step 7)") {
    const std::vector<Vec> vs = read_vectors();
    std::size_t through = 0, place_vectors = 0;
    for (const Vec& v : vs) {
        if (v.kind != "terrain" && v.kind != "place") {
            continue;
        }
        const sim::BuildingType t = mine_type(v);
        const sim::Island is = island_for(v);
        sim::PlacementRecord rec;
        rec.type = t.id;
        rec.x = 0;
        rec.y = 0;
        rec.rotation = static_cast<std::uint8_t>(v.rot);
        sim::PlacementParams params;
        if (v.kind == "terrain") {
            // a terrain vector fixes the helper's offset: it equals the window's lo only for some;
            // those go through the engine with the level floor off (the floor is step 7, the
            // `place` vectors below)
            const std::uint32_t ww = v.rot % 2 == 0 ? v.w : v.h, hh = v.rot % 2 == 0 ? v.h : v.w;
            unsigned lo = 255;
            for (std::uint32_t z = 0; z < hh; ++z) {
                for (std::uint32_t x = 0; x < ww; ++x) {
                    lo = std::min<unsigned>(
                        lo, std::min({v.heights[z][x], v.heights[z][x + 1], v.heights[z + 1][x],
                                      v.heights[z + 1][x + 1]}) &
                                0xf0U);
                }
            }
            if (lo != v.off) {
                continue;
            }
            params.min_level = 0;
        } else {
            ++place_vectors;
        }
        INFO("vector " << v.name);
        const sim::PlacementResult r =
            sim::placement_rule(is, *sim::TypeRegistry::empty(), t, rec, params);
        if (v.expect == "accept") {
            CHECK(r.reject == sim::Reject::None);
        } else {
            CHECK(r.reject == sim::Reject::TerrainRefused);
            CHECK(r.height == 0);
        }
        ++through;
    }
    CHECK(place_vectors == 4);
    CHECK(through > 30);
}

TEST_CASE("mountain placement: types the class does not cover stay Unsupported") {
    const std::vector<Vec> vs = read_vectors();
    Vec v;
    for (const Vec& c : vs) {
        if (c.name == "salt-flat-r0") {
            v = c;
        }
    }
    REQUIRE(v.w == 3);
    const sim::Island is = island_for(v);
    sim::PlacementRecord rec;
    rec.type = sim::TypeId{1203};
    const sim::TypeRegistry& reg = *sim::TypeRegistry::empty();
    // the legal control
    sim::BuildingType t = mine_type(v);
    CHECK(sim::placement_rule(is, reg, t, rec).reject == sim::Reject::None);
    // no table, a table of the wrong length, a quarry class, a shallow iron mine, no production data
    sim::BuildingType no_table = t;
    no_table.highmap.clear();
    CHECK(sim::placement_rule(is, reg, no_table, rec).reject == sim::Reject::Unsupported);
    sim::BuildingType short_table = t;
    short_table.highmap.pop_back();
    CHECK(sim::placement_rule(is, reg, short_table, rec).reject == sim::Reject::Unsupported);
    sim::BuildingType quarry = t;
    quarry.cls = sim::RuntimeClass::Quarry;
    CHECK(sim::placement_rule(is, reg, quarry, rec).reject == sim::Reject::Unsupported);
    sim::BuildingType iron = t;
    iron.prod.outputs = {"EISENERZ"};
    CHECK(sim::placement_rule(is, reg, iron, rec).reject == sim::Reject::Unsupported);
    sim::BuildingType bare = t;
    bare.has_prod = false;
    CHECK(sim::placement_rule(is, reg, bare, rec).reject == sim::Reject::Unsupported);
    // a coverage flag keeps it undecoded
    sim::BuildingType coverage = t;
    coverage.flags_word |= 0x10;
    CHECK(sim::placement_rule(is, reg, coverage, rec).reject == sim::Reject::Unsupported);
}

TEST_CASE("deposit load: the hand-derived vectors (400 t becomes 2,000 t, the flag stays)") {
    const std::vector<Vec> vs = read_vectors();
    std::size_t n = 0;
    for (const Vec& v : vs) {
        if (v.kind != "deposit") {
            continue;
        }
        INFO("vector " << v.name);
        const sim::DepositInit d = sim::deposit_load(v.amount, v.goods, v.flag != 0);
        CHECK(d.remaining == v.want_remaining);
        CHECK(static_cast<std::uint32_t>(d.reserve) == v.want_flag);
        ++n;
    }
    CHECK(n == 7);
}
