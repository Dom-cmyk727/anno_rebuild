// Stream 135, X2: the mountain-class placement and the deposit binding of a player-built mine
// (docs/sim/salt-extraction.md sections 11-15). Layer 1 runs the hand-derived vectors through the
// engine; layer 2 builds in the original endless02 island 1 (owner 8) through PUBLIC commands.
// Needs ANNO1503_GAME_DIR for layer 2.
#include <algorithm>
#include <chrono>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/mine_facts.hpp>
#include <anno/sim/mine_source.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/transport.hpp>
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

TEST_CASE("mountain placement: the vectors through placement_rule with lo from the window") {
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
    // the coverage bit is evidenced for the mountain class only: an ordinary-land type with it stays
    // Unsupported (the island of this test is flat and empty, so the class gate decides)
    sim::BuildingType coverage = t;
    coverage.flags_word = 0x0094;
    coverage.highmap.clear();
    coverage.cls = sim::RuntimeClass::Plain;
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

// ---- layer 1b: the coverage vectors (docs/sim/salt-extraction.md section 18) -----------------

namespace {

struct CovType {
    std::uint32_t id = 0, w = 1, h = 1, prodtype = 0, flags = 0;
    std::vector<std::uint8_t> table;
};
struct CovCase {
    std::string name, expect;
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t W = 0, H = 0;
    std::vector<std::vector<std::uint8_t>> rows;
    std::vector<std::array<std::int32_t, 3>> objs;
    std::vector<std::array<std::int32_t, 2>> anchors;
    std::uint32_t tid = 0, rot = 0, flags = 0;
    std::int32_t x = 0, y = 0;
    std::uint8_t want_height = 0;
};

std::vector<std::uint8_t> hex_bytes(const std::string& h) {
    std::vector<std::uint8_t> out;
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(h.substr(i, 2), nullptr, 16)));
    }
    return out;
}

void read_coverage(std::vector<CovType>& types, std::vector<CovCase>& cases) {
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "mine_coverage_vectors.txt");
    REQUIRE_MESSAGE(in.good(), "mine_coverage_vectors.txt not found");
    std::string line;
    while (std::getline(in, line)) {
        line = line.substr(0, line.find('#'));
        std::istringstream ls(line);
        std::string k;
        if (!(ls >> k)) {
            continue;
        }
        if (k == "type") {
            CovType t;
            std::string fl, rep, tab;
            ls >> t.id >> t.w >> t.h >> t.prodtype >> fl >> rep >> tab;
            t.flags = static_cast<std::uint32_t>(std::stoul(fl, nullptr, 16));
            t.table = hex_bytes(tab);
            types.push_back(t);
        } else if (k == "case") {
            cases.emplace_back();
            ls >> cases.back().name;
        } else if (k == "island") {
            ls >> cases.back().x0 >> cases.back().y0 >> cases.back().W >> cases.back().H;
        } else if (k == "v") {
            std::size_t r = 0;
            std::string hx;
            ls >> r >> hx;
            auto& rows = cases.back().rows;
            rows.resize(std::max(rows.size(), r + 1));
            rows[r] = hex_bytes(hx);
        } else if (k == "obj") {
            std::array<std::int32_t, 3> o{};
            ls >> o[0] >> o[1] >> o[2];
            cases.back().objs.push_back(o);
        } else if (k == "anchor") {
            std::array<std::int32_t, 2> a{};
            ls >> a[0] >> a[1];
            cases.back().anchors.push_back(a);
        } else if (k == "place") {
            std::string fl;
            unsigned rec8 = 0;
            CovCase& c = cases.back();
            ls >> c.tid >> c.x >> c.y >> c.rot >> fl >> rec8;
            c.flags = static_cast<std::uint32_t>(std::stoul(fl, nullptr, 16));
        } else if (k == "expect") {
            CovCase& c = cases.back();
            ls >> c.expect;
            unsigned b = 0;
            if (c.expect == "accept") {
                ls >> b;
                c.want_height = static_cast<std::uint8_t>(b);
            } else if (c.expect == "reject") {
                ls >> c.expect; // the reason
            }
        }
    }
}

} // namespace

TEST_CASE("mountain coverage: the hand-derived vectors through placement_rule as a separate predicate") {
    std::vector<CovType> ctypes;
    std::vector<CovCase> cases;
    read_coverage(ctypes, cases);
    // the registry the existing objects are looked up in
    sim::TypeRegistry::Builder b;
    for (const CovType& ct : ctypes) {
        sim::BuildingType t;
        t.id = sim::TypeId{ct.id};
        t.prodtype = ct.prodtype;
        t.cls = sim::RuntimeClass::Plain;
        t.width = ct.w;
        t.height = ct.h;
        t.flags_word = ct.flags;
        b.add(t);
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    const auto reg = built.value();
    std::size_t ran = 0, skipped = 0, coverage_refusals = 0, accepts = 0;
    for (const CovCase& c : cases) {
        const auto tt = std::find_if(ctypes.begin(), ctypes.end(),
                                     [&](const CovType& t) { return t.id == c.tid; });
        REQUIRE(tt != ctypes.end());
        // Only what the sim can express: the salt-like mine (class 0x3000 with its exact table) with no
        // caller flags beyond 0x2000000 (no sim analogue; every other bit of the original's argument,
        // the apply, skip and caller-0x10 flags, has no counterpart in a command's placement).
        if ((tt->flags & 0x7000U) != 0x3000U || tt->table.size() != std::size_t{4} * tt->w * tt->h ||
            (c.flags & ~0x2000000U) != 0) {
            ++skipped;
            continue;
        }
        sim::BuildingType t;
        t.id = sim::TypeId{tt->id};
        t.prodtype = 3;
        t.cls = sim::RuntimeClass::Mine;
        t.width = tt->w;
        t.height = tt->h;
        t.flags_word = tt->flags;
        t.has_prod = true;
        t.prod.prodtype = "MINE";
        t.prod.kind = sim::BuildingClass::Mine;
        t.prod.outputs = {"SALZSTEIN"};
        t.highmap = tt->table;
        sim::Island is;
        REQUIRE(is.tiles.reset(c.x0, c.y0, c.W, c.H, 8));
        std::vector<std::uint8_t> height(std::size_t{c.W} * c.H, 0);
        for (std::size_t r = 0; r < c.H && r < c.rows.size(); ++r) {
            for (std::size_t x = 0; x < c.W; ++x) {
                height[r * c.W + x] = c.rows[r][x];
            }
        }
        is.terrain = sim::make_terrain(std::move(height), {}, {});
        const auto idx = [&](std::int32_t x, std::int32_t y) {
            return static_cast<std::uint64_t>(y - c.y0) * c.W + static_cast<std::uint64_t>(x - c.x0);
        };
        for (const auto& o : c.objs) {
            is.tiles.set_object(idx(o[0], o[1]), static_cast<std::uint32_t>(o[2]));
        }
        for (const auto& a : c.anchors) {
            is.tiles.set_foot_anchor(idx(a[0], a[1]), 1);
        }
        sim::PlacementRecord rec;
        rec.type = t.id;
        rec.x = c.x;
        rec.y = c.y;
        rec.rotation = static_cast<std::uint8_t>(c.rot);
        INFO("vector " << c.name);
        const sim::PlacementResult r = sim::placement_rule(is, *reg, t, rec);
        if (c.expect == "accept") {
            CHECK(r.reject == sim::Reject::None);
            CHECK(r.height == c.want_height);
            ++accepts;
        } else if (c.expect == "OCCUPIED") {
            CHECK(r.reject == sim::Reject::Occupied);
        } else {
            // COVERAGE and TERRAIN are externally one code (the reason is held by the reference)
            CHECK(r.reject == sim::Reject::TerrainRefused);
            coverage_refusals += c.expect == "COVERAGE" ? 1U : 0U;
        }
        ++ran;
    }
    // 31 vectors: the caller-flag pair, the apply-flag case and the 0x400000 case cannot be expressed
    CHECK(skipped == 4);
    CHECK(ran == cases.size() - skipped);
    CHECK(coverage_refusals >= 10);
    CHECK(accepts >= 10);
}

// ---- layer 2: the original endless02 island 1, player 8 (city 0), through PUBLIC commands ------
// Site coordinates were chosen with the independent oracle (tools/placement_ref.py over an export of the
// island, docs/sim/salt-extraction.md 18.1); the C++ rule decides here, the tests pin the agreement.

namespace {

constexpr sim::PlayerId kSaltPlayer{8};
constexpr sim::CityRef kSaltCity{sim::IslandId{1}, sim::CityNo{0}};
constexpr sim::TypeId kSaltMine{1203};
constexpr std::uint64_t kStepMs = 100;

struct SaltWorld {
    sim::World w;
    std::shared_ptr<const sim::TypeRegistry> types;
};

std::optional<SaltWorld> salt_world(bool started, bool atomic = false, bool live_houses = false) {
    auto o = load_original();
    if (!o) {
        return std::nullopt;
    }
    auto resolved = anno::sim_load::load_scenario_with_links(
        anno::test::read_file(o->dir / "Scenes" / "0011SingleEndlosAddon" / "endless02.sze"),
        o->templates, *o->types, o->cfg, {}, original_component::actor_free_units());
    REQUIRE(resolved);
    auto component = original_component::import_only(resolved.value().loaded.spec, o->types, o->cfg);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    sim::Rules rules;
    rules.build_atomic = atomic;
    rules.house_updates = live_houses;
    auto created = sim::World::create(component.value().spec, o->types, o->cfg, rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World world = std::move(created).value();
    if (world.script().state.context == sim::ScriptContext::Pending) {
        REQUIRE(world.initialize_scripts().empty());
    }
    if (started) {
        for (int i = 0; i < 3; ++i) {
            world.advance(kStepMs);
            REQUIRE_MESSAGE(!world.faulted(), world.fault());
        }
    }
    return SaltWorld{std::move(world), o->types};
}

sim::Command build_cmd(std::vector<sim::PlacementWire> p) {
    return sim::Command{kSaltPlayer, sim::BuildCmd{kSaltCity, std::move(p)}};
}
sim::PlacementWire mine_at(std::int32_t x, std::int32_t y, std::uint8_t rot = 0) {
    return sim::PlacementWire{kSaltMine, x, y, rot, 0, 0};
}
const sim::Island& isle(const sim::World& w) { return *w.islands()[1]; }
std::int64_t gold(const sim::World& w) { return w.players().find(kSaltPlayer)->money; }

/// A refusal changes nothing (the saved state is byte-identical), then the legal neighbour is accepted.
void refusal_row(const sim::World& base, const sim::PlacementWire& bad, sim::Reject want,
                 const sim::PlacementWire& good) {
    sim::World w = base;
    const auto before = w.save();
    const sim::ApplyResult r = w.apply(build_cmd({bad}));
    CHECK_MESSAGE(r.status == want, "got " << sim::reject_name(r.status) << " wanted "
                                           << sim::reject_name(want));
    CHECK(w.save() == before);
    sim::World n = base;
    const sim::ApplyResult ok = n.apply(build_cmd({good}));
    CHECK_MESSAGE(ok.ok(), "legal neighbour: " << sim::reject_name(ok.status));
}

} // namespace

TEST_CASE("salt mine: the legal site with its payment and the deposit bound at placement") {
    auto s = salt_world(true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    REQUIRE(w.construction_eligibility(kSaltPlayer, kSaltMine).allowed());
    CHECK(isle(w).deposits.empty()); // the loader makes none: imported mines stay inert
    const sim::BuildingType& ty = *s->types->find(kSaltMine);
    const std::int64_t gold0 = gold(w);
    const std::size_t cursor0 = w.rng().cursor();
    const sim::City& city = *isle(w).city(kSaltCity.city);
    std::map<sim::GoodsId, std::uint32_t> stock0;
    for (const sim::CostEntry& e : ty.cost_goods) {
        stock0[e.goods] = city.stock.stock[e.goods];
    }
    const sim::ApplyResult r = w.apply(build_cmd({mine_at(353, 359)}));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    // the original price, once
    CHECK(gold0 - gold(w) == static_cast<std::int64_t>(ty.cost_gold));
    for (const sim::CostEntry& e : ty.cost_goods) {
        CHECK(stock0[e.goods] - city.stock.stock[e.goods] == e.amount_88);
    }
    // no PRNG draw: the bind adds no draw site (draw-count identity at the command)
    CHECK(w.rng().cursor() == cursor0);
    // bound to the one ore in reach, as a shared deposit loaded by the original rule
    REQUIRE(isle(w).deposits.size() == 1);
    const sim::Deposit& d = isle(w).deposits[0];
    CHECK(d.goods == "SALZSTEIN");
    CHECK(d.x == 357);
    CHECK(d.y == 369);
    CHECK(d.remaining == 2000U * sim::kTon);
    CHECK(d.reserve);
    const sim::Building* b = w.find_building(r.records.at(0).building);
    REQUIRE(b != nullptr);
    const auto* mine = std::get_if<sim::Mine>(&b->payload);
    REQUIRE(mine != nullptr);
    CHECK(mine->deposit == std::optional<std::size_t>{0});
    CHECK(w.check_invariants().empty());
}

TEST_CASE("salt mine: the refusal matrix with a legal neighbour per row") {
    auto s = salt_world(true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::World& base = s->w;
    // terrain: (364,363) is refused by the helper, its neighbour (363,363) is legal
    refusal_row(base, mine_at(364, 363), sim::Reject::TerrainRefused, mine_at(363, 363));
    // occupancy: a building lies under (353,360); (353,359) is free
    refusal_row(base, mine_at(353, 360), sim::Reject::Occupied, mine_at(353, 359));
    // ownership: (373,340) is neutral land (no tile of the city); (353,359) is the city's
    refusal_row(base, mine_at(373, 340), sim::Reject::TileNotOwned, mine_at(353, 359));
    // bounds: x beyond the island; neighbour inside
    refusal_row(base, mine_at(590, 359), sim::Reject::OutOfIsland, mine_at(353, 359));
    // every other mountain-class type (quarries, shallow iron, and the deep gold, gems and iron mines)
    // keeps the typed refusal: exactly Unsupported on a free footprint the owner rule passes (the class
    // gate precedes the permission step, so the eligibility gate cannot mask it)
    {
        std::set<std::uint32_t> deep_other;
        std::size_t checked = 0;
        for (const sim::BuildingType& t : s->types->types()) {
            if ((t.flags_word & 0x7000U) != 0x3000U || t.id == kSaltMine) {
                continue;
            }
            if (t.cls == sim::RuntimeClass::Mine && t.has_prod && sim::mine_is_deep(t.prod, t.research_id) &&
                t.highmap.size() == std::size_t{4} * t.width * t.height) {
                deep_other.insert(t.id.v);
            }
            std::optional<sim::Reject> got;
            for (std::int32_t y = 352; y < 372 && !got; ++y) {
                for (std::int32_t x = 345; x < 362 && !got; ++x) {
                    sim::World w = base;
                    const auto before = w.save();
                    const sim::ApplyResult r =
                        w.apply(build_cmd({sim::PlacementWire{t.id, x, y, 0, 0, 0}}));
                    if (r.status == sim::Reject::TileNotOwned || r.status == sim::Reject::OutOfIsland ||
                        r.status == sim::Reject::Occupied) {
                        continue;
                    }
                    got = r.status;
                    CHECK(w.save() == before);
                }
            }
            REQUIRE_MESSAGE(got.has_value(), "type " << t.id.v);
            CHECK_MESSAGE(*got == sim::Reject::Unsupported,
                          "type " << t.id.v << ": " << sim::reject_name(*got));
            ++checked;
        }
        CHECK(checked == 11);
        // the deep non-salt mines of the registry: gold 1206 and 1212, gems 1214, deep iron 1216 and 1217
        CHECK(deep_other == std::set<std::uint32_t>{1206, 1212, 1214, 1216, 1217});
    }
    // permission: a type the town is not yet allowed to build is refused by the permission check,
    // not by the placement rule; the mine (Allowed) on the same site is the legal neighbour
    {
        const sim::BuildingType* locked = nullptr;
        for (const sim::BuildingType& t : s->types->types()) {
            if (t.cls != sim::RuntimeClass::Mine && !base.construction_eligibility(kSaltPlayer, t.id).allowed() &&
                base.construction_eligibility(kSaltPlayer, t.id).status ==
                    sim::BuildEligibilityStatus::StageLocked) {
                locked = &t;
                break;
            }
        }
        REQUIRE(locked != nullptr);
        sim::World w = base;
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd({sim::PlacementWire{locked->id, 353, 359, 0, 0, 0}}));
        CHECK_MESSAGE(r.status == sim::Reject::ConstructionDenied, "stage-locked type " << locked->id.v << ": " << sim::reject_name(r.status));
        CHECK(w.save() == before);
        // the legal neighbour: the allowed mine on the same site
        sim::World n = base;
        CHECK(n.apply(build_cmd({mine_at(353, 359)})).ok());
    }
    // payment: no gold
    {
        sim::World w = base;
        sim::WorldTestAccess::players(w).find(kSaltPlayer)->money = 0;
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd({mine_at(353, 359)}));
        CHECK(r.status == sim::Reject::CannotAfford);
        CHECK(w.save() == before);
        // the legal neighbour: the same command with the gold restored
        sim::WorldTestAccess::players(w).find(kSaltPlayer)->money = gold(base);
        CHECK(w.apply(build_cmd({mine_at(353, 359)})).ok());
    }
}

TEST_CASE("salt mine: the COVERAGE row, a separate predicate with its legal neighbour") {
    auto s = salt_world(true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // a rock type of the coverage group (flag 0x10, group 0x141), compatible with the mine's occupancy
    const sim::BuildingType* rock = s->types->find(sim::TypeId{29});
    REQUIRE(rock != nullptr);
    REQUIRE((rock->flags_word & 0x10U) != 0);
    REQUIRE(rock->width == 1);
    REQUIRE(rock->height == 1);
    // footprint x 353..355, y 359..362: the world chunk rows split at y = 360, so row y = 359 is one chunk
    // (3 tiles) and rows 360..362 the other (9 tiles)
    const auto arrange = [&](const std::vector<std::array<std::int32_t, 2>>& clear) {
        sim::World w = s->w;
        sim::Island& is = *sim::WorldTestAccess::islands(w)[1];
        for (std::int32_t y = 359; y < 363; ++y) {
            for (std::int32_t x = 353; x < 356; ++x) {
                const sim::Slot i = is.tiles.index_of(x, y);
                is.tiles.set_object(i, rock->id.v);
                const bool cleared = std::any_of(clear.begin(), clear.end(), [&](const auto& c) {
                    return c[0] == x && c[1] == y;
                });
                is.tiles.set_foot_anchor(i, cleared ? 0 : 1);
            }
        }
        return w;
    };
    const auto attempt = [&](sim::World w) {
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd({mine_at(353, 359)}));
        if (!r.ok()) {
            CHECK(w.save() == before);
        }
        return r.status;
    };
    // every tile kept: refused, although terrain and ownership are fine
    CHECK(attempt(arrange({})) == sim::Reject::TerrainRefused);
    // an anchor clear only in the 9-tile chunk: the 3-tile chunk (y = 359) stays unmarked -> refused
    CHECK(attempt(arrange({{354, 361}})) == sim::Reject::TerrainRefused);
    // and only in the 3-tile chunk: the other stays unmarked -> refused
    CHECK(attempt(arrange({{354, 359}})) == sim::Reject::TerrainRefused);
    // one clear anchor in EACH chunk: both are marked -> accepted over the rocks (the legal neighbour)
    CHECK(attempt(arrange({{354, 359}, {354, 361}})) == sim::Reject::None);
}

TEST_CASE("salt mine: policy B refuses a mine with no deposit in reach before any payment") {
    // ADAPTATION (docs/sim/salt-extraction.md section 22): the radius-7 lookup is our placement rule, not a
    // proven copy of the original's predicate; discovery is not consulted. Each refusal leaves the saved state
    // byte-identical (no payment, no serial, no building, no deposit) and the dry run (build_atomic) answers
    // the same as the live apply.
    auto live = salt_world(true, false);
    auto dry = salt_world(true, true);
    if (!live || !dry) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    struct Row {
        std::int32_t x, y;
        const char* what;
    };
    for (const Row r : {Row{335, 340, "no ore of any ware in reach"},
                        Row{366, 353, "only a deposit of another ware in reach (wrong mineral)"},
                        Row{352, 359, "a salt deposit just outside the reach"}}) {
        INFO(r.what << " at " << r.x << "," << r.y);
        for (SaltWorld* sw : {&*live, &*dry}) {
            sim::World w = sw->w;
            const auto before = w.save();
            const std::int64_t gold0 = gold(w);
            const sim::ApplyResult res = w.apply(build_cmd({mine_at(r.x, r.y)}));
            // the atomic command reports AtomicAbort as a whole, with the same per-record reason
            CHECK(res.status == (sw == &*dry ? sim::Reject::AtomicAbort : sim::Reject::NoDepositInReach));
            REQUIRE(res.records.size() >= 1);
            CHECK(res.records[0].status == sim::Reject::NoDepositInReach);
            CHECK(gold(w) == gold0);
            CHECK(w.save() == before);
            CHECK(isle(w).deposits.empty());
        }
    }
    // refusal order: a site that is BOTH unaffordable and without a deposit answers NoDepositInReach (the check
    // precedes the affordability test), with no charge, no serial and nothing changed
    for (SaltWorld* sw : {&*live, &*dry}) {
        sim::World w = sw->w;
        sim::WorldTestAccess::players(w).find(kSaltPlayer)->money = 0;
        const auto before = w.save();
        const sim::ApplyResult res = w.apply(build_cmd({mine_at(335, 340)}));
        REQUIRE(res.records.size() >= 1);
        CHECK(res.records[0].status == sim::Reject::NoDepositInReach);
        CHECK(w.save() == before);
        // while a site with a deposit and no gold is the affordability refusal
        const sim::ApplyResult poor = w.apply(build_cmd({mine_at(353, 359)}));
        REQUIRE(poor.records.size() >= 1);
        CHECK(poor.records[0].status == sim::Reject::CannotAfford);
        CHECK(w.save() == before);
    }
    // the neighbour just inside the reach builds, and the mine it makes is bound: a mine made by a command
    // never exists without a deposit
    for (SaltWorld* sw : {&*live, &*dry}) {
        sim::World w = sw->w;
        const sim::ApplyResult res = w.apply(build_cmd({mine_at(353, 359)}));
        REQUIRE_MESSAGE(res.ok(), sim::reject_name(res.status));
        const auto* m = std::get_if<sim::Mine>(&w.find_building(res.records.at(0).building)->payload);
        REQUIRE(m != nullptr);
        CHECK(m->deposit.has_value());
        CHECK(isle(w).deposits.size() == 1);
    }
    // a refusal inside a batch: the whole atomic command changes nothing; the plain world keeps the first
    // record and refuses the second, and no mine of either is unbound
    {
        sim::World a = dry->w, p = live->w;
        const auto before = a.save();
        const sim::Command mixed = build_cmd({mine_at(353, 359), mine_at(335, 340)});
        CHECK_FALSE(a.apply(mixed).ok());
        CHECK(a.save() == before);
        const sim::ApplyResult pr = p.apply(mixed);
        REQUIRE(pr.records.size() == 2);
        CHECK(pr.records[0].status == sim::Reject::None);
        CHECK(pr.records[1].status == sim::Reject::NoDepositInReach);
    }
    // the imported mine stays inert and unchanged
    const sim::Building* imported = nullptr;
    for (const auto& l : isle(live->w).city(kSaltCity.city)->index.lists)
        for (const auto& b : l.slots)
            if (b && b->type.v == 1203) imported = &*b;
    REQUIRE(imported != nullptr);
    CHECK_FALSE(std::get<sim::Mine>(imported->payload).deposit.has_value());
}

TEST_CASE("salt mine: salt never depletes while the edition setting depletes") {
    auto s = salt_world(true, false, true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    REQUIRE(sim::WorldTestAccess::settings(s->w).depletes()); // the add-on edition (prop14 bit 1): the setting that depletes
    const sim::ApplyResult r = s->w.apply(build_cmd({mine_at(353, 359)}));
    REQUIRE(r.ok());
    std::uint64_t out_seen = 0;
    for (int i = 0; i < 3000; ++i) {
        s->w.advance(kStepMs);
        REQUIRE_MESSAGE(!s->w.faulted(), s->w.fault());
        out_seen = std::max<std::uint64_t>(out_seen, std::get<sim::Mine>(s->w.find_building(r.records.at(0).building)->payload).out);
    }
    CHECK(out_seen > 0); // it really produced
    REQUIRE(isle(s->w).deposits.size() == 1);
    CHECK(isle(s->w).deposits[0].remaining == 2000U * sim::kTon); // deep: worked, never drawn down
    CHECK(isle(s->w).deposits[0].reserve);
}

TEST_CASE("salt mine: shared deposit and no refill by teardown rebuild or reload") {
    auto s = salt_world(true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    const sim::ApplyResult a = w.apply(build_cmd({mine_at(353, 359)}));
    REQUIRE(a.ok());
    // a second mine in reach of the same ore shares its deposit (one deposit per ore tile)
    const sim::ApplyResult b = w.apply(build_cmd({mine_at(356, 359)}));
    REQUIRE_MESSAGE(b.ok(), sim::reject_name(b.status));
    REQUIRE(isle(w).deposits.size() == 1);
    CHECK(std::get<sim::Mine>(w.find_building(b.records.at(0).building)->payload).deposit ==
          std::optional<std::size_t>{0});
    // the ore has been worked down: teardown and rebuild never refill it
    sim::WorldTestAccess::islands(w)[1]->deposits[0].remaining = 123;
    sim::DemolishCmd dem;
    dem.city = kSaltCity;
    dem.tiles = {{353, 359}, {356, 359}};
    REQUIRE(w.apply(sim::Command{kSaltPlayer, dem}).ok());
    CHECK(w.find_building(a.records.at(0).building) == nullptr);
    const sim::ApplyResult again = w.apply(build_cmd({mine_at(353, 359)}));
    REQUIRE_MESSAGE(again.ok(), sim::reject_name(again.status));
    REQUIRE(isle(w).deposits.size() == 1);
    CHECK(isle(w).deposits[0].remaining == 123);
    CHECK(std::get<sim::Mine>(w.find_building(again.records.at(0).building)->payload).deposit ==
          std::optional<std::size_t>{0});
    // save / load: the same bytes and the same hash, the deposit persists
    const auto bytes = w.save();
    auto loaded = sim::World::load(bytes, s->types, {});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().save() == bytes);
    CHECK(loaded.value().state_hash() == w.state_hash());
    REQUIRE(isle(loaded.value()).deposits.size() == 1);
    CHECK(isle(loaded.value()).deposits[0].remaining == 123);
    CHECK(w.check_invariants().empty());
}

TEST_CASE("salt mine: the atomic dry run equals the live apply and a refused batch binds nothing") {
    auto atomic = salt_world(true, true);
    auto plain = salt_world(true, false);
    if (!atomic || !plain) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // two legal mines: the same result, the same final state
    const sim::Command two = build_cmd({mine_at(353, 359), mine_at(356, 359)});
    const sim::ApplyResult ra = atomic->w.apply(two);
    const sim::ApplyResult rp = plain->w.apply(two);
    REQUIRE(ra.ok());
    REQUIRE(rp.ok());
    CHECK(gold(atomic->w) == gold(plain->w));
    REQUIRE(isle(atomic->w).deposits.size() == isle(plain->w).deposits.size());
    for (std::size_t i = 0; i < isle(plain->w).deposits.size(); ++i) {
        const sim::Deposit& da = isle(atomic->w).deposits[i];
        const sim::Deposit& dp = isle(plain->w).deposits[i];
        CHECK(da.goods == dp.goods);
        CHECK(da.x == dp.x);
        CHECK(da.y == dp.y);
        CHECK(da.remaining == dp.remaining);
        CHECK(da.reserve == dp.reserve);
    }
    CHECK(atomic->w.islands()[1]->cache.by_id.size() == plain->w.islands()[1]->cache.by_id.size());
    CHECK(atomic->w.rng().cursor() == plain->w.rng().cursor());
    CHECK(isle(atomic->w).deposits.size() == 1);
    // a batch with a refused record: atomic leaves no building, no payment and NO deposit; the
    // non-atomic world keeps the first record (and binds it)
    auto atomic2 = salt_world(true, true);
    auto plain2 = salt_world(true, false);
    const sim::Command mixed = build_cmd({mine_at(353, 359), mine_at(364, 363)});
    const auto before = atomic2->w.save();
    CHECK_FALSE(atomic2->w.apply(mixed).ok());
    CHECK(atomic2->w.save() == before);
    CHECK(isle(atomic2->w).deposits.empty());
    const sim::ApplyResult pr = plain2->w.apply(mixed);
    CHECK(pr.status == sim::Reject::None); // a non-atomic batch reports per record, as before
    REQUIRE(pr.records.size() == 2);
    CHECK(pr.records[0].status == sim::Reject::None);
    CHECK(pr.records[1].status == sim::Reject::TerrainRefused);
    CHECK(isle(plain2->w).deposits.size() == 1);
}

TEST_CASE("salt mine: a standing mine adds no PRNG draw over the same world without it") {
    auto with = salt_world(true);
    auto without = salt_world(true);
    if (!with || !without) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    REQUIRE(with->w.apply(build_cmd({mine_at(353, 359)})).ok());
    for (int i = 0; i < 60; ++i) {
        with->w.advance(kStepMs);
        without->w.advance(kStepMs);
        REQUIRE_MESSAGE(!with->w.faulted(), with->w.fault());
        REQUIRE_MESSAGE(!without->w.faulted(), without->w.fault());
    }
    CHECK(with->w.rng().cursor() == without->w.rng().cursor());
}

// ---- the supported-ware allowlist and the typed Unsupported branches of the coverage rule -----

namespace {

/// A flat all-0x30 island of w x h tiles at origin (x0, y0), with the registry holding one rock.
struct FlatIsland {
    sim::Island is;
    std::shared_ptr<const sim::TypeRegistry> reg;
};
FlatIsland flat_island(std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h) {
    FlatIsland f;
    REQUIRE(f.is.tiles.reset(x0, y0, w, h, 8));
    f.is.terrain = sim::make_terrain(std::vector<std::uint8_t>(std::size_t{w} * h, 0x30), {}, {});
    sim::TypeRegistry::Builder b;
    sim::BuildingType rock;
    rock.id = sim::TypeId{29};
    rock.prodtype = 322;
    rock.cls = sim::RuntimeClass::Plain;
    rock.flags_word = 0x12;
    b.add(rock);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    f.reg = built.value();
    return f;
}
sim::BuildingType table_mine(std::uint32_t w, std::uint32_t h, const char* ware) {
    sim::BuildingType t;
    t.id = sim::TypeId{1203};
    t.prodtype = 3;
    t.cls = sim::RuntimeClass::Mine;
    t.width = w;
    t.height = h;
    t.flags_word = 0x3096;
    t.has_prod = true;
    t.prod.prodtype = "MINE";
    t.prod.kind = sim::BuildingClass::Mine;
    t.prod.outputs = {ware};
    t.highmap.assign(std::size_t{4} * w * h, 0xff);
    return t;
}
sim::Reject rule(const FlatIsland& f, const sim::BuildingType& t, std::int32_t x, std::int32_t y,
                 const sim::PlacementParams& p = {}) {
    sim::PlacementRecord rec;
    rec.type = t.id;
    rec.x = x;
    rec.y = y;
    return sim::placement_rule(f.is, *f.reg, t, rec, p).reject;
}

} // namespace

TEST_CASE("mountain placement: only the listed wares are placed and the list is data") {
    auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const FlatIsland f = flat_island(0, 0, 40, 40);
    // salt: the legal control
    CHECK(rule(f, *o->types->find(sim::TypeId{1203}), 4, 4) == sim::Reject::None);
    // the five deep mines of other wares: Unsupported by default on a site the rule would accept
    // with their ware listed
    for (const std::uint32_t id : {1206U, 1212U, 1214U, 1216U, 1217U}) {
        const sim::BuildingType& t = *o->types->find(sim::TypeId{id});
        INFO("type " << id << " ware " << t.prod.outputs.at(0));
        CHECK(rule(f, t, 4, 4) == sim::Reject::Unsupported);
        sim::PlacementParams listed;
        listed.mountain_wares = {t.prod.outputs.at(0)};
        CHECK(rule(f, t, 4, 4, listed) == sim::Reject::None);
        // and listing another ware does not unlock it
        sim::PlacementParams other;
        other.mountain_wares = {"SALZSTEIN"};
        CHECK(rule(f, t, 4, 4, other) == sim::Reject::Unsupported);
    }
    // an empty list places nothing, salt included
    sim::PlacementParams none;
    none.mountain_wares.clear();
    CHECK(rule(f, *o->types->find(sim::TypeId{1203}), 4, 4, none) == sim::Reject::Unsupported);
}

TEST_CASE("mountain coverage: the typed Unsupported branches and the chunk edge as data") {
    const FlatIsland f = flat_island(0, 0, 40, 40);
    // footprints above one chunk edge (8): Unsupported, 8 x 8 is the largest accepted
    CHECK(rule(f, table_mine(8, 8, "SALZSTEIN"), 4, 4) == sim::Reject::None);
    CHECK(rule(f, table_mine(9, 3, "SALZSTEIN"), 4, 4) == sim::Reject::Unsupported);
    CHECK(rule(f, table_mine(3, 9, "SALZSTEIN"), 4, 4) == sim::Reject::Unsupported);
    // a chunk edge of 16 (shift 4) accepts them
    sim::PlacementParams wide;
    wide.coverage_chunk_shift = 4;
    CHECK(rule(f, table_mine(9, 3, "SALZSTEIN"), 4, 4, wide) == sim::Reject::None);
    CHECK(rule(f, table_mine(3, 9, "SALZSTEIN"), 4, 4, wide) == sim::Reject::None);
    // a negative world coordinate: Unsupported; the same footprint at a non-negative origin is accepted
    const FlatIsland neg = flat_island(-8, -8, 40, 40);
    CHECK(rule(neg, table_mine(3, 4, "SALZSTEIN"), -2, 2) == sim::Reject::Unsupported);
    CHECK(rule(neg, table_mine(3, 4, "SALZSTEIN"), 2, 2) == sim::Reject::None);
    // an object of the dry run's overlay on the footprint: Unsupported; elsewhere it is ignored
    const sim::BuildingType mine = table_mine(3, 4, "SALZSTEIN");
    const auto idx = [&](std::int32_t x, std::int32_t y) {
        return static_cast<std::uint64_t>(y) * 40U + static_cast<std::uint64_t>(x);
    };
    std::map<std::uint64_t, sim::TypeId> overlay{{idx(5, 5), sim::TypeId{29}}};
    sim::PlacementParams with_overlay;
    with_overlay.object_overlay = &overlay;
    CHECK(rule(f, mine, 4, 4, with_overlay) == sim::Reject::Unsupported);
    CHECK(rule(f, mine, 20, 20, with_overlay) == sim::Reject::None);
    CHECK(rule(f, mine, 4, 4) == sim::Reject::None);
}

TEST_CASE("salt mine: a standing mine produces and adds no PRNG draw at any step") {
    auto with = salt_world(true);
    auto without = salt_world(true);
    if (!with || !without) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // two mines bound to the same ore, so that both work the deposit
    const auto a = with->w.apply(build_cmd({mine_at(353, 359), mine_at(356, 359)}));
    REQUIRE(a.ok());
    std::uint32_t produced = 0;
    for (int i = 0; i < 600; ++i) {
        with->w.advance(kStepMs);
        without->w.advance(kStepMs);
        REQUIRE_MESSAGE(!with->w.faulted(), with->w.fault());
        REQUIRE(with->w.rng().cursor() == without->w.rng().cursor());
        produced = 0;
        for (const auto& r : a.records) {
            produced += std::get<sim::Mine>(with->w.find_building(r.building)->payload).out;
        }
    }
    CHECK(produced > 0);
}


// ---- X3: the whole chain mine -> carrier -> saltworks -> carrier -> store -> stall -> households --
// (docs/sim/salt-extraction.md section 20). Real arm and counterfactual arms on endless02 island 1,
// city 0 of player 8, with scoped ledgers whose unexplained terms must be zero.

namespace {

constexpr sim::GoodsId kSalzstein = 3; // SALZ is tutorial_town::kSalz = 28
constexpr std::int32_t kMineX = 353, kMineY = 359;   // a legal site (docs 18.1), beside the imported saltworks
constexpr std::int32_t kWorksX = 351, kWorksY = 363; // the imported saltworks 1022

enum class Arm { Real, NoNewMine, SourceDisabled, ProducerRemoved, SaltworksRemoved };

struct Chain {
    SaltWorld sw;
    sim::TransportStats stats;
    sim::BuildingId mine, works;
    std::uint32_t works_in0 = 0;
    std::uint64_t salz_store0 = 0, ore_store0 = 0;
    std::map<sim::DrawSite, std::uint64_t> draws;
    std::vector<std::uint64_t> cursors;
};

const sim::City& city_of(const sim::World& w) { return *isle(w).city(kSaltCity.city); }
const sim::Building* find_type(const sim::World& w, std::uint32_t type, sim::BuildingId skip = {}) {
    for (const auto& l : city_of(w).index.lists)
        for (const auto& b : l.slots)
            if (b && b->type.v == type && !(skip.valid() && b->id == skip)) return &*b;
    return nullptr;
}

/// zero_ore: the labelled setup step "no preloaded SALZSTEIN in the store" (a named fixture edit
/// applied to every arm of that family, not a game command), so the ore baseline is exactly zero.
std::unique_ptr<Chain> start_chain(Arm arm, bool zero_ore, bool live_houses = true) {
    auto sw = salt_world(true, false, live_houses);
    if (!sw) return nullptr;
    auto c = std::make_unique<Chain>(Chain{std::move(*sw), {}, {}, {}, 0, 0, 0, {}, {}});
    sim::World& w = c->sw.w;
    if (zero_ore) {
        sim::WorldTestAccess::islands(w)[1]->city(kSaltCity.city)->stock.stock[kSalzstein] = 0;
        REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    }
    c->works = find_type(w, 1022)->id;
    const sim::Workshop& ws = std::get<sim::Workshop>(find_type(w, 1022)->payload);
    c->works_in0 = ws.in.at(0);
    if (arm == Arm::SaltworksRemoved) {
        sim::DemolishCmd dem;
        dem.city = kSaltCity;
        dem.tiles = {{kWorksX, kWorksY}};
        REQUIRE(w.apply(sim::Command{kSaltPlayer, dem}).ok());
        REQUIRE(find_type(w, 1022) == nullptr);
    }
    w.set_transport_stats(&c->stats);
    c->salz_store0 = city_of(w).stock.stock[kSalz];
    c->ore_store0 = city_of(w).stock.stock[kSalzstein];
    if (arm != Arm::NoNewMine) {
        const sim::ApplyResult r = w.apply(build_cmd({mine_at(kMineX, kMineY)}));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        c->mine = r.records.at(0).building;
        if (arm == Arm::SourceDisabled) {
            // the source switched off: the built, paid mine is unbound (its deposit link removed)
            for (auto& l : sim::WorldTestAccess::islands(w)[1]->city(kSaltCity.city)->index.lists)
                for (auto& b : l.slots)
                    if (b && b->id == c->mine) std::get<sim::Mine>(b->payload).deposit.reset();
        }
        if (arm == Arm::ProducerRemoved) {
            sim::DemolishCmd dem;
            dem.city = kSaltCity;
            dem.tiles = {{kMineX, kMineY}};
            REQUIRE(w.apply(sim::Command{kSaltPlayer, dem}).ok());
            REQUIRE(w.find_building(c->mine) == nullptr);
        }
    }
    return c;
}

void step_chain(Chain& c, int n) {
    for (int i = 0; i < n; ++i) {
        c.sw.w.advance(kStepMs);
        REQUIRE_MESSAGE(!c.sw.w.faulted(), c.sw.w.fault());
        for (const sim::DrawRecord& r : c.sw.w.last_output().draws) c.draws[r.site] += r.count;
        c.cursors.push_back(c.sw.w.rng().cursor());
    }
}

struct Terms {
    std::uint64_t mine_made = 0, mine_out = 0, ore_loaded = 0, ore_cargo = 0, works_in = 0, works_consumed = 0,
                  works_made = 0, works_out = 0, salz_loaded = 0, salz_cargo = 0, salz_unloaded = 0, salz_lost = 0,
                  ore_lost = 0, bought = 0, taken = 0, delivered = 0, res_lost = 0, res_cargo = 0, salz_store = 0,
                  ore_store = 0;
    std::int64_t u_mine = 0, u_ore = 0, u_made = 0, u_salz_out = 0, u_store = 0, u_buy = 0, u_deliver = 0,
                 u_house = 0;
};

Terms terms_of(Chain& c) {
    Terms t;
    sim::World& w = c.sw.w;
    const sim::TransportStats& st = c.stats;
    const auto get = [](const auto& m, const auto& k) -> std::uint64_t {
        const auto it = m.find(k);
        return it == m.end() ? 0U : it->second;
    };
    const sim::City& city = city_of(w);
    if (c.mine.valid()) {
        t.mine_made = get(st.forestry.mine_made_88, c.mine);
        t.ore_loaded = get(st.loaded_from, c.mine);
        if (const sim::Building* m = w.find_building(c.mine)) t.mine_out = std::get<sim::Mine>(m->payload).out;
    }
    const sim::Building* works = w.find_building(c.works);
    if (works != nullptr) {
        const sim::Workshop& ws = std::get<sim::Workshop>(works->payload);
        t.works_in = ws.in.at(0);
        t.works_out = ws.out.at(0);
        t.salz_loaded = get(st.loaded_from, c.works);
    }
    if (const auto it = st.forestry.workshops.find(c.works); it != st.forestry.workshops.end()) {
        t.works_consumed = it->second.consumed_88.at(0);
        t.works_made = it->second.made_88.at(0);
    }
    for (const auto& f : w.figures().list.slots) {
        if (!f || f->expiry_stamp.has_value() || f->owner != city.owner) continue;
        if (f->cargo_goods == kSalzstein && !f->resident) t.ore_cargo += f->cargo_88;
        if (f->cargo_goods == kSalz && !f->resident) t.salz_cargo += f->cargo_88;
        if (f->resident)
            for (const sim::ResidentCargo& rc : f->resident->cargo)
                if (rc.goods == kSalz) t.res_cargo += rc.amount_88;
    }
    t.salz_unloaded = get(st.unload_offered, kSalz);
    t.salz_lost = get(st.cart_lost, kSalz);
    t.ore_lost = get(st.cart_lost, kSalzstein);
    t.salz_store = city.stock.stock[kSalz];
    t.ore_store = city.stock.stock[kSalzstein];
    t.bought = get(st.residents.bought_by_goods, kSalz);
    t.taken = get(st.residents.merged_taken_by_city, std::make_pair(kSaltCity, kSalz));
    t.delivered = get(st.residents.delivered_by_goods, kSalz);
    t.res_lost = get(st.residents.lost_by_goods, kSalz);
    const auto I = [](std::uint64_t v) { return static_cast<std::int64_t>(v); };
    // ore: made = still in the mine + loaded; loaded = in the carriers' hands + into the saltworks
    // (input now - input at start + consumed) + into the store (change) + lost
    t.u_mine = I(t.mine_made) - I(t.mine_out) - I(t.ore_loaded);
    t.u_ore = I(t.ore_loaded) - I(t.ore_cargo) - I(t.ore_lost) -
              (I(t.works_in) - I(c.works_in0) + I(t.works_consumed)) - (I(t.ore_store) - I(c.ore_store0));
    // SALZ: made = out + loaded; loaded = in the carts' hands + unloaded into the store + lost
    t.u_made = I(t.works_made) - I(t.works_out) - I(t.salz_loaded);
    t.u_salz_out = I(t.salz_loaded) - I(t.salz_cargo) - I(t.salz_unloaded) - I(t.salz_lost);
    t.u_store = I(t.salz_store) - I(c.salz_store0) - I(t.salz_unloaded) + I(t.taken);
    t.u_buy = I(t.bought) - I(t.taken);
    t.u_deliver = I(t.bought) - I(t.delivered) - I(t.res_lost) - I(t.res_cargo);
    // u_house: the merge's own per-delivery closure (the stock a delivery raised equals what it was
    // meant to raise). It does NOT see a direct write into a house's stock between deliveries: house
    // stock also falls by consumption and moves with upgrades, which this ledger does not model.
    t.u_house = I(get(st.residents.stock_primary_by_goods, kSalz) + get(st.residents.stock_replay_by_goods, kSalz)) -
                I(get(st.residents.stock_expected_by_goods, kSalz));
    return t;
}

void require_closed(const Terms& t, const char* label) {
    INFO("ledger " << label);
    CHECK(t.u_mine == 0);
    CHECK(t.u_ore == 0);
    CHECK(t.u_made == 0);
    CHECK(t.u_salz_out == 0);
    CHECK(t.u_store == 0);
    CHECK(t.u_buy == 0);
    CHECK(t.u_deliver == 0);
    CHECK(t.u_house == 0);
}

} // namespace

TEST_CASE("salt chain X3: the real arm delivers salt to households and every ledger term closes") {
    auto c = start_chain(Arm::Real, true);
    if (!c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // baseline: the preloaded SALZ is 20 t, the ore store is empty by the labelled setup step
    CHECK(c->salz_store0 == 20U * sim::kTon);
    CHECK(c->ore_store0 == 0);
    for (int block = 0; block < 7; ++block) {
        step_chain(*c, 1000);
        require_closed(terms_of(*c), "real, every 1000 steps");
    }
    const Terms t = terms_of(*c);
    MESSAGE("real: mine_made " << t.mine_made << " ore_loaded " << t.ore_loaded << " works_made " << t.works_made
            << " salz_unloaded " << t.salz_unloaded << " taken " << t.taken << " delivered " << t.delivered);
    // production beyond the baseline
    CHECK(t.mine_made > 0);
    CHECK(t.ore_loaded > 0);       // the saltworks' carrier lifted ore from the new mine
    CHECK(t.works_consumed > 0);   // the saltworks used it (no other ore existed)
    CHECK(t.works_made > 0);
    CHECK(t.salz_unloaded > 0);    // SALZ reached the store by the stall's cart
    // receipt beyond the baseline: more SALZ left the store for households than the preload held
    CHECK(t.taken > c->salz_store0);
    CHECK(t.delivered > 0);
    // WHO lifted the ore: the saltworks' own carriers (home = the saltworks), not a market cart
    std::uint64_t by_works = 0, by_others = 0;
    for (const auto& [key, n] : c->stats.loaded_by_home) {
        if (key.first != c->mine) continue;
        (key.second == c->works ? by_works : by_others) += n;
    }
    MESSAGE("ore lifted from the new mine: by the saltworks' carriers " << by_works << ", by any other home " << by_others);
    CHECK(by_works == t.ore_loaded);
    CHECK(by_works > 0);
    CHECK(by_others == 0);
    // and the new mine is the only producer: the imported mine stayed inert
    const sim::Building* imported = find_type(c->sw.w, 1203, c->mine);
    REQUIRE(imported != nullptr);
    CHECK(std::get<sim::Mine>(imported->payload).out == 0);
    CHECK(c->stats.forestry.mine_made_88[imported->id] == 0U);
    CHECK(c->stats.residents.faults == 0);
    CHECK(c->sw.w.check_invariants().empty());
}

TEST_CASE("salt chain X3: the counterfactual arms produce no new salt and the baseline only") {
    for (const Arm arm : {Arm::NoNewMine, Arm::SourceDisabled, Arm::ProducerRemoved, Arm::SaltworksRemoved}) {
        auto c = start_chain(arm, true);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        INFO("arm " << static_cast<int>(arm));
        for (int block = 0; block < 7; ++block) {
            step_chain(*c, 1000);
            require_closed(terms_of(*c), "counterfactual");
        }
        const Terms t = terms_of(*c);
        CHECK(t.works_made == 0);
        CHECK(t.salz_unloaded == 0);
        CHECK(t.ore_loaded == 0);
        // every unit that reached a household came out of the preloaded 20 t
        CHECK(t.taken <= c->salz_store0);
        CHECK(t.taken > 0); // the baseline stock is really consumed (the households are live)
        if (arm == Arm::SaltworksRemoved) {
            // the mine works and its ore stays in the mine: no carrier, no chain without the processor
            CHECK(t.mine_made > 0);
            CHECK(t.mine_out == t.mine_made);
        } else {
            CHECK(t.mine_made == 0);
        }
    }
}

TEST_CASE("salt chain X3: road and carrier reach differ across the legal sites and the near ones connect") {
    struct Site {
        std::int32_t x, y;
        std::uint8_t rot;
        bool connects;
    };
    // sampled from the 356 legal sites (docs 18.1), nearest to farthest by the Euclidean tile distance
    // from the saltworks' origin: the four within 10 tiles are lifted, the two beyond 15 tiles are
    // not. The saltworks type's action radius is 16 and its carrier searches the SearchArea disc
    // around the footprint; radius and road connectivity are NOT separated by this test (a far site
    // may fail either), it pins only the measured outcome.
    for (const Site s : {Site{352, 360, 1, true}, Site{357, 363, 0, true}, Site{358, 365, 0, true},
                         Site{359, 369, 1, true}, Site{358, 377, 0, false}, Site{365, 381, 3, false}}) {
        auto c = start_chain(Arm::NoNewMine, true);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        INFO("site " << s.x << "," << s.y << " rot " << int(s.rot));
        const sim::ApplyResult r = c->sw.w.apply(build_cmd({mine_at(s.x, s.y, s.rot)}));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        c->mine = r.records.at(0).building;
        step_chain(*c, 1500);
        const Terms t = terms_of(*c);
        require_closed(t, "reach");
        CHECK(t.mine_made > 0);
        CHECK((t.ore_loaded > 0) == s.connects);
        CHECK((t.works_made > 0) == s.connects);
    }
}

TEST_CASE("salt chain X3: the pilot households are stage 0 and that stage needs salt") {
    auto sw = salt_world(true, false, true);
    if (!sw) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::set<std::uint32_t> groups;
    const sim::City& city = city_of(sw->w);
    for (const auto& l : city.index.lists)
        for (const auto& b : l.slots)
            if (b && std::holds_alternative<sim::House>(b->payload))
                groups.insert(*sim::house_group(sw->w.config(), sw->types->find(b->type)->prodtype));
    // the town's 82 houses are all group 0 (types 501..509): the other stages are NOT exercised here
    CHECK(groups == std::set<std::uint32_t>{0});
    for (std::uint32_t g = 0; g < 5; ++g) {
        const auto view = sw->types->population_view(g, sw->w.config());
        REQUIRE(view.ok());
        const auto it = std::find_if(view.value().group->needs.begin(), view.value().group->needs.end(),
                                     [](const sim::HouseNeed& n) { return n.ware == kSalz; });
        const bool has = it != view.value().group->needs.end();
        MESSAGE("group " << g << ": " << (has ? "SALZ need, force " + std::to_string(it->force) + ", menge " +
                                                    std::to_string(it->menge)
                                              : std::string("no SALZ need")));
        // the registry: stages 0..3 need salt (menge 8, 8, 10, 10; forced from stage 1), stage 4 does not
        CHECK(has == (g < 4));
        if (has) {
            CHECK(it->menge == (g < 2 ? 8U : 10U));
            CHECK(it->force == (g > 0)); // forced from stage 1 up
        }
    }
}

TEST_CASE("salt chain X3: with the preloads in place the new mine adds ore") {
    auto real = start_chain(Arm::Real, false);
    auto base = start_chain(Arm::NoNewMine, false);
    if (!real || !base) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // the baselines are the original's preloads: 20 t SALZ and 50 t SALZSTEIN, kept apart
    CHECK(real->salz_store0 == 20U * sim::kTon);
    CHECK(real->ore_store0 == 50U * sim::kTon);
    for (int block = 0; block < 4; ++block) {
        step_chain(*real, 1000);
        step_chain(*base, 1000);
        require_closed(terms_of(*real), "real with preloads");
        require_closed(terms_of(*base), "base with preloads");
    }
    const Terms r = terms_of(*real), b = terms_of(*base);
    // the new mine's ore reached the saltworks beside the preload, and the preload was used less
    CHECK(r.ore_loaded > 0);
    CHECK(r.ore_store > b.ore_store);
    CHECK(b.ore_loaded == 0);
}

TEST_CASE("salt chain X3: a payment-matched control has the same draws at every step as the real arm") {
    // The construction PAYMENT (gold 700, HOLZ 15 t, WERKZEUG 5 t) changes city stock and gold, and
    // with live households that alone moves the households' own draws (HouseWalkerTarget,
    // HouseVariant) from step 2,052 of the run: the producer-removed arm (mine built and paid, then
    // demolished before it works) is the payment-matched control. Against it the real arm, which
    // also produces and delivers salt, has the same PRNG cursor and the same draws of every site at
    // every step: the mine, the saltworks and the carriers add no draw. Which household mechanism
    // consumes HOLZ or WERKZEUG is NOT established here.
    auto real = start_chain(Arm::Real, false);
    auto paid = start_chain(Arm::ProducerRemoved, false);
    auto none = start_chain(Arm::NoNewMine, false);
    if (!real || !paid || !none) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step_chain(*real, 4000);
    step_chain(*paid, 4000);
    step_chain(*none, 4000);
    CHECK(terms_of(*real).works_made > 0);
    REQUIRE(real->cursors.size() == paid->cursors.size());
    CHECK(real->cursors == paid->cursors);
    CHECK(real->draws == paid->draws);
    // the payment is the cause: with no payment at all the households' draws differ
    CHECK(none->cursors != paid->cursors);
    std::size_t first = 0;
    while (first < none->cursors.size() && none->cursors[first] == paid->cursors[first]) ++first;
    MESSAGE("no-payment arm diverges from the paid arm at step " << first);
    CHECK(first < 4000);
}

TEST_CASE("salt chain X3: with the households not live the mine leaves the PRNG cursor equal at every step") {
    auto real = start_chain(Arm::Real, false, false);
    auto base = start_chain(Arm::NoNewMine, false, false);
    if (!real || !base) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step_chain(*real, 3000);
    step_chain(*base, 3000);
    CHECK(terms_of(*real).mine_made > 0);
    CHECK(terms_of(*real).works_made > 0);
    CHECK(real->cursors == base->cursors);
    CHECK(real->draws == base->draws);
}

TEST_CASE("salt chain X3: the ledger catches a direct salt credit and a direct ore credit") {
    auto c = start_chain(Arm::Real, true);
    if (!c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step_chain(*c, 500);
    require_closed(terms_of(*c), "before the credits");
    // a NAMED negative control, not a game command: 2 t of SALZ written into the store, and 3 t of ore
    // written into the saltworks' input
    sim::City& city = *sim::WorldTestAccess::islands(c->sw.w)[1]->city(kSaltCity.city);
    city.stock.stock[kSalz] += 2U * sim::kTon;
    for (auto& l : city.index.lists)
        for (auto& b : l.slots)
            if (b && b->id == c->works) std::get<sim::Workshop>(b->payload).in.at(0) += 3U * sim::kTon;
    step_chain(*c, 1);
    const Terms t = terms_of(*c);
    CHECK(t.u_store == 2 * 256);
    CHECK(t.u_ore == -3 * 256);
}


// ---- X4: interruption, restoration, lifetime and timing boundaries --------------------------------

namespace {

struct XChain {
    std::unique_ptr<Chain> c;
    std::vector<sim::BuildingId> mines, works;
    struct Hist {
        std::uint64_t mine_out = 0, works_in = 0, works_out = 0;
    } hist;
};

/// Low-stock setup (labelled fixture edit): no preloaded ore AND no preloaded SALZ in the store, so
/// the households can only buy what the renewed chain delivers.
XChain start_x(bool mine, bool live_houses = true) {
    XChain x;
    x.c = start_chain(mine ? Arm::Real : Arm::NoNewMine, true, live_houses);
    if (!x.c) return x;
    sim::World& w = x.c->sw.w;
    sim::WorldTestAccess::islands(w)[1]->city(kSaltCity.city)->stock.stock[kSalz] = 0;
    x.c->salz_store0 = 0;
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    if (mine) x.mines.push_back(x.c->mine);
    x.works.push_back(x.c->works);
    return x;
}

const sim::Building* building_on(const sim::World& w, std::int32_t x, std::int32_t y) {
    return isle(w).building_at(w.config().layout, kSaltCity.city, x, y);
}

/// Demolishes the buildings on `tiles`; what the removed mines and saltworks still held is history.
void demolish_x(XChain& x, const std::vector<std::pair<std::int32_t, std::int32_t>>& tiles) {
    sim::World& w = x.c->sw.w;
    sim::DemolishCmd dem;
    dem.city = kSaltCity;
    for (const auto& [tx, ty] : tiles) {
        dem.tiles.push_back({tx, ty});
        const sim::Building* b = building_on(w, tx, ty);
        if (b == nullptr) continue;
        if (const auto* m = std::get_if<sim::Mine>(&b->payload)) x.hist.mine_out += m->out;
        if (const auto* ws = std::get_if<sim::Workshop>(&b->payload)) {
            x.hist.works_in += ws->in.at(0);
            x.hist.works_out += ws->out.at(0);
        }
    }
    const sim::ApplyResult r = w.apply(sim::Command{kSaltPlayer, dem});
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
}

sim::BuildingId build_x(XChain& x, std::uint32_t type, std::int32_t px, std::int32_t py, std::uint8_t rot) {
    const sim::ApplyResult r = x.c->sw.w.apply(
        build_cmd({sim::PlacementWire{sim::TypeId{type}, px, py, rot, 0, 0}}));
    REQUIRE_MESSAGE(r.ok(), "build " << type << " at " << px << "," << py << ": " << sim::reject_name(r.status));
    const sim::BuildingId id = r.records.at(0).building;
    if (type == 1203) x.mines.push_back(id);
    if (type == 1022) x.works.push_back(id);
    return id;
}

struct Terms2 {
    std::uint64_t mine_made = 0, mine_out = 0, ore_loaded = 0, works_made = 0, works_out = 0, works_in = 0,
                  consumed = 0, salz_loaded = 0, ore_cargo = 0, salz_cargo = 0, unloaded = 0, salz_lost = 0, ore_lost = 0,
                  bought = 0, taken = 0, delivered = 0, res_lost = 0, res_cargo = 0, salz_store = 0, ore_store = 0;
    std::int64_t u_mine = 0, u_ore = 0, u_made = 0, u_salz_out = 0, u_store = 0, u_buy = 0, u_deliver = 0;
};

Terms2 terms2(XChain& x) {
    Terms2 t;
    Chain& c = *x.c;
    sim::World& w = c.sw.w;
    const sim::TransportStats& st = c.stats;
    const auto get = [](const auto& m, const auto& k) -> std::uint64_t {
        const auto it = m.find(k);
        return it == m.end() ? 0U : it->second;
    };
    const sim::City& city = city_of(w);
    for (const sim::BuildingId id : x.mines) {
        t.mine_made += get(st.forestry.mine_made_88, id);
        t.ore_loaded += get(st.loaded_from, id);
        if (const sim::Building* m = w.find_building(id)) t.mine_out += std::get<sim::Mine>(m->payload).out;
    }
    for (const sim::BuildingId id : x.works) {
        t.salz_loaded += get(st.loaded_from, id);
        if (const sim::Building* b = w.find_building(id)) {
            const auto& ws = std::get<sim::Workshop>(b->payload);
            t.works_in += ws.in.at(0);
            t.works_out += ws.out.at(0);
        }
        if (const auto it = st.forestry.workshops.find(id); it != st.forestry.workshops.end()) {
            t.consumed += it->second.consumed_88.at(0);
            t.works_made += it->second.made_88.at(0);
        }
    }
    for (const auto& f : w.figures().list.slots) {
        if (!f || f->expiry_stamp.has_value() || f->owner != city.owner) continue;
        if (f->cargo_goods == kSalzstein && !f->resident) t.ore_cargo += f->cargo_88;
        if (f->cargo_goods == kSalz && !f->resident) t.salz_cargo += f->cargo_88;
        if (f->resident)
            for (const sim::ResidentCargo& rc : f->resident->cargo)
                if (rc.goods == kSalz) t.res_cargo += rc.amount_88;
    }
    t.unloaded = get(st.unload_offered, kSalz);
    t.salz_lost = get(st.cart_lost, kSalz);
    t.ore_lost = get(st.cart_lost, kSalzstein);
    t.salz_store = city.stock.stock[kSalz];
    t.ore_store = city.stock.stock[kSalzstein];
    t.bought = get(st.residents.bought_by_goods, kSalz);
    t.taken = get(st.residents.merged_taken_by_city, std::make_pair(kSaltCity, kSalz));
    t.delivered = get(st.residents.delivered_by_goods, kSalz);
    t.res_lost = get(st.residents.lost_by_goods, kSalz);
    const auto I = [](std::uint64_t v) { return static_cast<std::int64_t>(v); };
    t.u_mine = I(t.mine_made) - I(t.mine_out) - I(t.ore_loaded) - I(x.hist.mine_out);
    t.u_ore = I(t.ore_loaded) - I(t.ore_cargo) - I(t.ore_lost) -
              (I(t.works_in) + I(x.hist.works_in) - I(c.works_in0) + I(t.consumed)) - (I(t.ore_store) - I(c.ore_store0));
    t.u_made = I(t.works_made) - I(t.works_out) - I(x.hist.works_out) - I(t.salz_loaded);
    t.u_salz_out = I(t.salz_loaded) - I(t.salz_cargo) - I(t.unloaded) - I(t.salz_lost);
    t.u_store = I(t.salz_store) - I(c.salz_store0) - I(t.unloaded) + I(t.taken);
    t.u_buy = I(t.bought) - I(t.taken);
    t.u_deliver = I(t.bought) - I(t.delivered) - I(t.res_lost) - I(t.res_cargo);
    return t;
}

void closed2(const Terms2& t, const char* label) {
    const std::string checkpoint = label; // logged as a string, so a failure names the checkpoint
    INFO("ledger " << checkpoint);
    CHECK(t.u_mine == 0);
    CHECK(t.u_ore == 0);
    CHECK(t.u_made == 0);
    CHECK(t.u_salz_out == 0);
    CHECK(t.u_store == 0);
    CHECK(t.u_buy == 0);
    CHECK(t.u_deliver == 0);
}

/// Validate, save, load: the same bytes and hash, and the loaded world continues like the original.
void validate_and_reload(XChain& x, const char* label) {
    INFO("terminal path " << label);
    sim::World& w = x.c->sw.w;
    CHECK(w.check_invariants().empty());
    const auto bytes = w.save();
    auto loaded = sim::World::load(bytes, x.c->sw.types, {});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().save() == bytes);
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().check_invariants().empty());
    // the deposit table survives and every Mine::deposit index is valid
    for (const auto& l : city_of(loaded.value()).index.lists)
        for (const auto& b : l.slots)
            if (const auto* m = b ? std::get_if<sim::Mine>(&b->payload) : nullptr)
                if (m->deposit) CHECK(*m->deposit < isle(loaded.value()).deposits.size());
}

struct Snap {
    std::uint64_t works_made = 0, mine_made = 0, unloaded = 0, taken = 0, delivered = 0, ore_loaded = 0;
};
Snap snap(XChain& x) {
    const Terms2 t = terms2(x);
    return {t.works_made, t.mine_made, t.unloaded, t.taken, t.delivered, t.ore_loaded};
}
Snap diff(const Snap& a, const Snap& b) {
    return {b.works_made - a.works_made, b.mine_made - a.mine_made, b.unloaded - a.unloaded,
            b.taken - a.taken, b.delivered - a.delivered, b.ore_loaded - a.ore_loaded};
}

} // namespace


namespace {

struct Placed {
    std::uint32_t type;
    std::int32_t x, y;
    std::uint8_t rot;
};

struct Windows {
    std::vector<Snap> p1, p2, p3; // 500-step windows
    std::map<sim::DrawSite, std::uint64_t> draws;
    std::vector<std::uint64_t> cursors;
    std::uint64_t start_salz = ~0ULL, start_ore = ~0ULL; // the store at the start of the run (the low-stock setup)
};

/// The chain, interrupted and restored: phase 1 (1,000 steps) establishes it from the low-stock
/// setup, phase 2 (4,000) runs interrupted, phase 3 (3,000) runs restored; native state, ledger and
/// save/load are checked at once after the cut and after the restoration, and again at every phase end; the ledger must close at
/// the end of every phase.
template <class Interrupt, class Restore>
Windows run_interruption(const char* name, Interrupt interrupt, Restore restore, bool print, bool instant = false, bool live = true) {
    Windows out;
    auto x = start_x(true, live);
    if (!x.c) return out;
    out.start_salz = city_of(x.c->sw.w).stock.stock[kSalz];
    out.start_ore = city_of(x.c->sw.w).stock.stock[kSalzstein];
    const auto phase = [&](std::vector<Snap>& v, int windows, const char* label) {
        for (int i = 0; i < windows; ++i) {
            const Snap a = snap(x);
            step_chain(*x.c, 500);
            v.push_back(diff(a, snap(x)));
        }
        closed2(terms2(x), label);
        CHECK(x.c->sw.w.check_invariants().empty());
    };
    const auto immediately = [&](const char* label) {
        // native state checked at once after a cut or a restoration, before any tick
        CHECK(x.c->sw.w.check_invariants().empty());
        closed2(terms2(x), label);
        validate_and_reload(x, label);
    };
    phase(out.p1, 2, "P1");
    interrupt(x);
    immediately("at the cut");
    if (instant) restore(x);
    phase(out.p2, 8, "P2");
    if (!instant) restore(x);
    immediately("at the restoration");
    phase(out.p3, 6, "P3");
    validate_and_reload(x, name);
    out.draws = x.c->draws;
    out.cursors = x.c->cursors;
    if (print) {
        const auto show = [&](const char* ph, const std::vector<Snap>& v) {
            for (std::size_t i = 0; i < v.size(); ++i)
                std::cout << name << " " << ph << i << ": works_made " << v[i].works_made << " mine_made " << v[i].mine_made
                          << " unloaded " << v[i].unloaded << " taken " << v[i].taken << " delivered " << v[i].delivered << "\n";
        };
        show("P1.", out.p1); show("P2.", out.p2); show("P3.", out.p3);
    }
    return out;
}

std::vector<Placed> placed_of(const sim::World& w, std::uint32_t type) {
    std::vector<Placed> v;
    for (const auto& l : city_of(w).index.lists)
        for (const auto& b : l.slots)
            if (b && b->type.v == type) v.push_back({type, b->x, b->y, b->rotation});
    return v;
}

} // namespace


namespace {

const std::vector<std::pair<std::int32_t, std::int32_t>> kMarketTiles = {{346, 346}, {346, 371}};

bool all_zero(const std::vector<Snap>& v, std::size_t from, bool works, bool mine, bool unloaded, bool taken, bool delivered) {
    for (std::size_t i = from; i < v.size(); ++i) {
        if ((works && v[i].works_made) || (mine && v[i].mine_made) || (unloaded && v[i].unloaded) ||
            (taken && v[i].taken) || (delivered && v[i].delivered))
            return false;
    }
    return true;
}
bool all_positive(const std::vector<Snap>& v, std::size_t from, bool works, bool mine, bool unloaded, bool taken, bool delivered) {
    for (std::size_t i = from; i < v.size(); ++i) {
        if ((works && !v[i].works_made) || (mine && !v[i].mine_made) || (unloaded && !v[i].unloaded) ||
            (taken && !v[i].taken) || (delivered && !v[i].delivered))
            return false;
    }
    return true;
}

/// The uninterrupted control of the same length.
Windows control_run(bool live = true) {
    return run_interruption("control", [](XChain&) {}, [](XChain&) {}, false, false, live);
}

/// PRNG discipline.
///  - Households NOT live, interruption and restoration at the SAME instant (demolish and rebuild with
///    no time between): the draws of every site and the final cursor equal the uninterrupted control (the
///    new buildings have new ids, so WHEN within the run a draw falls may shift, not how many fall). The
///    demolitions, the rebuilds, their payments and the chain draw nothing.
///  - Households NOT live, the link down for 4,000 steps: only FarmWorkerDispatch may differ. It is an
///    existing site drawn once per eligible farm dispatch attempt; the farms share the markets' carts with
///    the salt chain, so an idle chain frees carts for the farms and their eligibility moves. No new site
///    appears and no other site changes.
///  - Households live: HouseWalkerTarget, HouseVariant and FarmWorkerDispatch follow economy state (any
///    small perturbation, even a payment-matched instant rebuild, moves them); the other sites are equal.
template <class Interrupt, class Restore>
void draws_unchanged(const Windows& live_run, Interrupt interrupt, Restore restore, bool quiet_worlds = true,
                     bool instant_exact = true) {
    const Windows control = control_run(true);
    // the low-stock setup is applied alike in every arm (it is the baseline the comparison stands on)
    CHECK(live_run.start_salz == 0);
    CHECK(live_run.start_ore == 0);
    CHECK(control.start_salz == 0);
    CHECK(control.start_ore == 0);
    const auto stable = {sim::DrawSite::HouseWalkerTarget, sim::DrawSite::HouseVariant, sim::DrawSite::FarmWorkerDispatch};
    const auto minus_sites = [](std::map<sim::DrawSite, std::uint64_t> m, std::initializer_list<sim::DrawSite> sites) {
        for (const sim::DrawSite site : sites) m.erase(site);
        return m;
    };
    CHECK(minus_sites(live_run.draws, stable) == minus_sites(control.draws, stable));
    for (const auto& [site, n] : live_run.draws) CHECK(control.draws.count(site) == 1U);
    if (!quiet_worlds) return;
    const Windows quiet_control = control_run(false);
    const Windows instant = run_interruption("quiet-instant", interrupt, restore, false, true, false);
    CHECK(quiet_control.start_salz == 0);
    CHECK(quiet_control.start_ore == 0);
    CHECK(instant.start_salz == 0);
    CHECK(instant.start_ore == 0);
    REQUIRE(instant.cursors.size() == quiet_control.cursors.size());
    if (instant_exact) {
        CHECK(instant.cursors.back() == quiet_control.cursors.back()); // the same number of draws overall
        CHECK(instant.draws == quiet_control.draws);                     // at every site
    } else {
        // a link whose demolition or rebuild is not a no-op for the rest of the economy (the markets cannot be
        // rebuilt at all; a rebuilt saltworks has a new id and moves the existing state-following sites by a
        // draw): only the sites that do not follow economy state must be equal
        auto strip3 = [](std::map<sim::DrawSite, std::uint64_t> m) {
            m.erase(sim::DrawSite::HouseWalkerTarget);
            m.erase(sim::DrawSite::HouseVariant);
            m.erase(sim::DrawSite::FarmWorkerDispatch);
            return m;
        };
        CHECK(strip3(instant.draws) == strip3(quiet_control.draws));
    }
    const Windows quiet = run_interruption("quiet", interrupt, restore, false, false, false);
    CHECK(quiet.start_salz == 0);
    CHECK(quiet.start_ore == 0);
    auto minus = [](std::map<sim::DrawSite, std::uint64_t> m, std::initializer_list<sim::DrawSite> sites) {
        for (const sim::DrawSite site : sites) m.erase(site);
        return m;
    };
    CHECK(minus(quiet.draws, {sim::DrawSite::FarmWorkerDispatch}) ==
          minus(quiet_control.draws, {sim::DrawSite::FarmWorkerDispatch}));
    for (const auto& [site, n] : quiet.draws) CHECK(quiet_control.draws.count(site) == 1U);
}

} // namespace

TEST_CASE("salt interruption X4: the mine demolished and rebuilt") {
    const Windows w = run_interruption("mine", [](XChain& x) { demolish_x(x, {{kMineX, kMineY}}); },
                                       [](XChain& x) { build_x(x, 1203, kMineX, kMineY, 0); }, false);
    if (w.p1.empty()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    CHECK(all_positive(w.p1, 1, true, true, true, true, true));
    // drained: the ore in the saltworks runs out, then nothing is made, unloaded, bought or delivered
    CHECK(all_zero(w.p2, 4, true, true, true, true, true));
    // renewed: production, receipts, purchases and deliveries all return
    CHECK(all_positive(w.p3, 3, true, true, true, true, true));
    draws_unchanged(w, [](XChain& x) { demolish_x(x, {{kMineX, kMineY}}); },
                    [](XChain& x) { build_x(x, 1203, kMineX, kMineY, 0); });
}

TEST_CASE("salt interruption X4: the road cut and rebuilt") {
    std::vector<std::pair<std::pair<int, int>, std::uint32_t>> tiles;
    const auto interrupt = [&](XChain& x) {
        const auto& L = isle(x.c->sw.w).tiles;
        std::vector<std::pair<std::int32_t, std::int32_t>> t;
        tiles.clear();
        for (int y = 350; y <= 366; ++y)
            for (int xx = 350; xx <= 355; ++xx) {
                const auto id = L.object(L.index_of(xx, y));
                const auto* ty = id ? x.c->sw.types->find(sim::TypeId{id}) : nullptr;
                if (ty && sim::is_land_road(*ty)) {
                    tiles.push_back({{xx, y}, id});
                    t.push_back({xx, y});
                }
            }
        REQUIRE(t.size() == 22);
        demolish_x(x, t);
    };
    const auto restore = [&](XChain& x) {
        for (const auto& [xy, id] : tiles) {
            const sim::ApplyResult r = x.c->sw.w.apply(
                build_cmd({sim::PlacementWire{sim::TypeId{id}, xy.first, xy.second, 0, 0, 0}}));
            CHECK_MESSAGE(r.ok(), "road " << xy.first << "," << xy.second << ": " << sim::reject_name(r.status));
        }
    };
    const Windows w = run_interruption("road", interrupt, restore, false);
    if (w.p1.empty()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    CHECK(all_zero(w.p2, 4, true, true, true, true, true));
    CHECK(all_positive(w.p3, 3, true, true, true, true, true));
    // the quiet (households not live) worlds are skipped here: some road pieces are stage-locked there
    // (ConstructionDenied) because the stage mask only rises with live households
    draws_unchanged(w, interrupt, restore, false);
}

TEST_CASE("salt interruption X4: the markets that carry the salt demolished and not restorable") {
    std::vector<Placed> markets;
    sim::Reject rebuild = sim::Reject::None;
    const auto interrupt = [&](XChain& x) {
        markets.clear();
        for (const Placed& p : placed_of(x.c->sw.w, 1305))
            if ((p.x == 346 && p.y == 346) || (p.x == 346 && p.y == 371)) markets.push_back(p);
        REQUIRE(markets.size() == 2);
        demolish_x(x, kMarketTiles);
    };
    const auto restore = [&](XChain& x) {
        // the market type is not buildable for the player (permission): the link cannot be restored
        rebuild = x.c->sw.w.apply(build_cmd({sim::PlacementWire{sim::TypeId{1305}, markets[0].x, markets[0].y, markets[0].rot, 0, 0}})).status;
    };
    const Windows w = run_interruption("markets", interrupt, restore, false);
    if (w.p1.empty()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    CHECK(rebuild == sim::Reject::ConstructionDenied);
    // no cart lifts the SALZ any more: the saltworks fills up and stops, nothing reaches the store
    CHECK(all_zero(w.p2, 1, false, false, true, false, false));
    CHECK(all_zero(w.p2, 2, true, false, true, true, true));
    CHECK(all_zero(w.p2, 4, true, true, true, true, true));
    // and there is no recovery without the restorable link
    CHECK(all_zero(w.p3, 0, true, true, true, true, true));
    draws_unchanged(w, interrupt, restore, true, false);
}

TEST_CASE("salt interruption X4: the saltworks demolished and rebuilt") {
    const auto interrupt = [](XChain& x) { demolish_x(x, {{kWorksX, kWorksY}}); };
    const auto restore = [](XChain& x) { build_x(x, 1022, kWorksX, kWorksY, 0); };
    const Windows w = run_interruption("saltworks", interrupt, restore, false);
    if (w.p1.empty()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    // the mine goes on until its output is full and then stops; no ore is processed, no salt arrives
    CHECK(all_zero(w.p2, 0, true, false, false, false, false));
    CHECK(all_zero(w.p2, 4, true, true, true, true, true));
    CHECK(all_positive(w.p3, 3, true, true, true, true, true));
    draws_unchanged(w, interrupt, restore, true, false);
}

TEST_CASE("salt interruption X4: the stalls demolished and rebuilt") {
    std::vector<Placed> stalls;
    const auto interrupt = [&](XChain& x) {
        stalls = placed_of(x.c->sw.w, 1310);
        REQUIRE(stalls.size() == 4);
        std::vector<std::pair<std::int32_t, std::int32_t>> t;
        for (const Placed& p : stalls) t.push_back({p.x, p.y});
        demolish_x(x, t);
    };
    const auto restore = [&](XChain& x) { for (const Placed& p : stalls) build_x(x, 1310, p.x, p.y, p.rot); };
    const Windows w = run_interruption("stalls", interrupt, restore, false);
    if (w.p1.empty()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    // production and receipts go on (the store fills), the households buy and receive nothing
    CHECK(all_positive(w.p2, 1, true, true, true, false, false));
    CHECK(all_zero(w.p2, 1, false, false, false, true, true));
    CHECK(all_positive(w.p3, 0, true, true, true, true, true));
    draws_unchanged(w, interrupt, restore);
}


// ---- X4: lifetime boundaries (cargo in flight, shared deposit) --------------------------------

namespace {

std::uint64_t in_flight(XChain& x, sim::BuildingId home, sim::GoodsId g) {
    std::uint64_t n = 0;
    for (const auto& f : x.c->sw.w.figures().list.slots)
        if (f && !f->expiry_stamp.has_value() && !f->resident && f->home == home && f->cargo_goods == g) n += f->cargo_88;
    return n;
}

template <class Pred>
int step_until(XChain& x, int limit, Pred pred) {
    for (int i = 1; i <= limit; ++i) {
        step_chain(*x.c, 1);
        if (pred()) return i;
    }
    return -1;
}

} // namespace

TEST_CASE("salt lifetime X4: cargo in flight when its home is demolished is history") {
    // (a) the saltworks demolished while its carrier holds ore
    {
        auto x = start_x(true);
        if (!x.c) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
        const sim::BuildingId works = x.works[0];
        REQUIRE(step_until(x, 3000, [&] { return in_flight(x, works, kSalzstein) > 0; }) > 0);
        const std::uint64_t premise = in_flight(x, works, kSalzstein);
        const std::uint64_t lost0 = terms2(x).ore_lost;
        demolish_x(x, {{kWorksX, kWorksY}});
        step_chain(*x.c, 400);
        const Terms2 t = terms2(x);
        CHECK(in_flight(x, works, kSalzstein) == 0);
        CHECK(t.ore_lost - lost0 == premise);
        closed2(t, "works demolished with ore in the carrier's hands");
        validate_and_reload(x, "works with cargo");
    }
    // (b) both markets demolished while their carts hold SALZ
    {
        auto x = start_x(true);
        const auto* a = building_on(x.c->sw.w, 346, 346);
        const auto* b = building_on(x.c->sw.w, 346, 371);
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        const sim::BuildingId ia = a->id, ib = b->id;
        REQUIRE(step_until(x, 4000, [&] { return in_flight(x, ia, kSalz) + in_flight(x, ib, kSalz) > 0; }) > 0);
        const std::uint64_t premise = in_flight(x, ia, kSalz) + in_flight(x, ib, kSalz);
        const std::uint64_t lost0 = terms2(x).salz_lost;
        demolish_x(x, kMarketTiles);
        step_chain(*x.c, 600);
        const Terms2 t = terms2(x);
        CHECK(in_flight(x, ia, kSalz) + in_flight(x, ib, kSalz) == 0);
        CHECK(t.salz_lost - lost0 == premise);
        closed2(t, "markets demolished with SALZ in the carts");
        validate_and_reload(x, "markets with cargo");
    }
    // (c) the MINE demolished while the saltworks' carrier holds its ore: the cargo is delivered, nothing is lost
    {
        auto x = start_x(true);
        const sim::BuildingId works = x.works[0];
        REQUIRE(step_until(x, 3000, [&] { return in_flight(x, works, kSalzstein) > 0; }) > 0);
        const std::uint64_t lost0 = terms2(x).ore_lost;
        demolish_x(x, {{kMineX, kMineY}});
        step_chain(*x.c, 400);
        const Terms2 t = terms2(x);
        CHECK(t.ore_lost == lost0);
        closed2(t, "mine demolished with ore in the carrier's hands");
        validate_and_reload(x, "mine with cargo");
    }
}

TEST_CASE("salt lifetime X4: one deposit shared by two mines survives the demolition of one") {
    auto x = start_x(false);
    if (!x.c) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    const sim::BuildingId a = build_x(x, 1203, kMineX, kMineY, 0);
    const sim::BuildingId b = build_x(x, 1203, 356, 359, 0);
    REQUIRE(isle(x.c->sw.w).deposits.size() == 1);
    sim::WorldTestAccess::islands(x.c->sw.w)[1]->deposits[0].remaining = 321;
    step_chain(*x.c, 300);
    demolish_x(x, {{kMineX, kMineY}});
    CHECK(x.c->sw.w.find_building(a) == nullptr);
    const auto* mb = std::get_if<sim::Mine>(&x.c->sw.w.find_building(b)->payload);
    REQUIRE(mb != nullptr);
    CHECK(mb->deposit == std::optional<std::size_t>{0}); // still bound to the shared deposit
    step_chain(*x.c, 1500);
    CHECK(terms2(x).mine_made > 0);
    CHECK(isle(x.c->sw.w).deposits.size() == 1);
    CHECK(isle(x.c->sw.w).deposits[0].remaining == 321); // deep and flagged: worked, never refilled, never used up
    // the other one demolished too, then a rebuild: the deposit is the same object, not refilled
    demolish_x(x, {{356, 359}});
    step_chain(*x.c, 100);
    const sim::BuildingId c = build_x(x, 1203, kMineX, kMineY, 0);
    CHECK(std::get<sim::Mine>(x.c->sw.w.find_building(c)->payload).deposit == std::optional<std::size_t>{0});
    CHECK(isle(x.c->sw.w).deposits.size() == 1);
    CHECK(isle(x.c->sw.w).deposits[0].remaining == 321);
    step_chain(*x.c, 500);
    closed2(terms2(x), "shared deposit");
    validate_and_reload(x, "shared deposit");
}

// ---- X4: timed-transition edges ------------------------------------------------------------------

TEST_CASE("salt timing X4: zero and smallest positive elapsed times") {
    // One tick is 2 ms. advance(game_ms) runs every subsystem once per call (docs/sim/state.md 5.3:
    // the original adds 0 and still runs them), so the state depends on how the elapsed time is
    // sliced into calls; a recording therefore holds the slice of every advance. What must hold for
    // every slicing: the clock and the chain's observable flow equal, no fault, the ledger closed,
    // and the same slicing reproduces the same state (and cursor) byte for byte. The PRNG cursor
    // itself follows the number of calls (the existing per-call draw sites), so it is compared only
    // between equal slicings.
    auto a = start_x(true), b = start_x(true), c = start_x(true), c2 = start_x(true);
    if (!a.c) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    for (XChain* x : {&a, &b, &c, &c2}) step_chain(*x->c, 300);
    const auto tick0 = a.c->sw.w.clock().tick();
    const std::uint64_t cursor0 = a.c->sw.w.rng().cursor();
    a.c->sw.w.advance(0); // zero elapsed time: no tick, a valid world; the subsystems still run
    REQUIRE_MESSAGE(!a.c->sw.w.faulted(), a.c->sw.w.fault());
    CHECK(a.c->sw.w.clock().tick() == tick0);
    // ... and draw where they draw per call: only the households' and the farm workers' own sites,
    // never anything of the mine, the saltworks or the carriers
    std::set<sim::DrawSite> zero_sites;
    for (const sim::DrawRecord& r : a.c->sw.w.last_output().draws) zero_sites.insert(r.site);
    for (const sim::DrawSite site : zero_sites)
        CHECK((site == sim::DrawSite::HouseWalkerTarget || site == sim::DrawSite::HouseVariant ||
               site == sim::DrawSite::FarmWorkerDispatch));
    MESSAGE("advance(0) drew " << (a.c->sw.w.rng().cursor() - cursor0) << " values at " << zero_sites.size() << " sites");
    CHECK(a.c->sw.w.check_invariants().empty());
    // the same 100 x 100 ms cut three ways (a had its extra zero-time advance above, which is part of its slicing)
    for (int i = 0; i < 100; ++i) a.c->sw.w.advance(100);
    for (int i = 0; i < 10000; ++i) b.c->sw.w.advance(1);
    const auto mix = [](XChain& x) {
        for (int i = 0; i < 100; ++i) {
            x.c->sw.w.advance(0);
            x.c->sw.w.advance(1);
            x.c->sw.w.advance(99);
        }
    };
    mix(c);
    mix(c2);
    for (XChain* x : {&a, &b, &c, &c2}) {
        REQUIRE_MESSAGE(!x->c->sw.w.faulted(), x->c->sw.w.fault());
        CHECK(x->c->sw.w.check_invariants().empty());
        closed2(terms2(*x), "sliced");
    }
    CHECK(a.c->sw.w.clock().tick() == b.c->sw.w.clock().tick());
    CHECK(a.c->sw.w.clock().tick() == c.c->sw.w.clock().tick());
    const Terms2 ta = terms2(a), tb = terms2(b), tc = terms2(c);
    CHECK(ta.works_made == tb.works_made);
    CHECK(ta.works_made == tc.works_made);
    CHECK(ta.unloaded == tb.unloaded);
    CHECK(ta.taken == tc.taken);
    // the same slicing is reproduced exactly
    CHECK(c.c->sw.w.state_hash() == c2.c->sw.w.state_hash());
    CHECK(c.c->sw.w.save() == c2.c->sw.w.save());
    CHECK(c.c->sw.w.rng().cursor() == c2.c->sw.w.rng().cursor());
}

namespace {

/// A chain advanced k steps (deterministic: the same construction gives the same world).
XChain chain_at(int k) {
    XChain x = start_x(true);
    if (x.c) step_chain(*x.c, k);
    return x;
}

template <class Event>
int first_event_step(Event event) {
    XChain x = start_x(true);
    if (!x.c) return -1;
    for (int i = 1; i <= 6000; ++i) {
        const Terms2 before = terms2(x);
        step_chain(*x.c, 1);
        if (event(before, terms2(x))) return i;
    }
    return -1;
}

} // namespace

TEST_CASE("salt timing X4: a demolition on the step of a load an unload and an arrival") {
    if (!load_original()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    // LOAD: a carrier lifts ore from the mine in step k; the mine is demolished just before it
    {
        const int k = first_event_step([](const Terms2& a, const Terms2& b) { return b.ore_loaded > a.ore_loaded; });
        REQUIRE(k > 0);
        XChain with = chain_at(k - 1), without = chain_at(k - 1);
        const Terms2 w0 = terms2(with);
        step_chain(*with.c, 1);
        CHECK(terms2(with).ore_loaded > w0.ore_loaded); // the load happens
        const Terms2 b0 = terms2(without);
        demolish_x(without, {{kMineX, kMineY}});
        step_chain(*without.c, 1);
        const Terms2 b1 = terms2(without);
        CHECK(b1.ore_loaded == b0.ore_loaded); // no load from a mine that is gone, no double credit
        closed2(b1, "demolished on the load step");
        step_chain(*with.c, 400);
        step_chain(*without.c, 400);
        closed2(terms2(with), "load step, kept");
        closed2(terms2(without), "load step, demolished");
        validate_and_reload(without, "load step");
    }
    // UNLOAD: a cart unloads SALZ into the store in step k; both markets are demolished just before it
    {
        const int k = first_event_step([](const Terms2& a, const Terms2& b) { return b.unloaded > a.unloaded; });
        REQUIRE(k > 0);
        XChain with = chain_at(k - 1), without = chain_at(k - 1);
        const Terms2 w0 = terms2(with);
        step_chain(*with.c, 1);
        CHECK(terms2(with).unloaded > w0.unloaded);
        const Terms2 b0 = terms2(without);
        const sim::BuildingId ia = building_on(without.c->sw.w, 346, 346)->id, ib = building_on(without.c->sw.w, 346, 371)->id;
        const std::uint64_t premise = in_flight(without, ia, kSalz) + in_flight(without, ib, kSalz);
        demolish_x(without, kMarketTiles);
        step_chain(*without.c, 1);
        CHECK(terms2(without).unloaded == b0.unloaded);
        step_chain(*without.c, 600);
        const Terms2 b1 = terms2(without);
        CHECK(b1.unloaded == b0.unloaded);
        CHECK(b1.salz_lost - b0.salz_lost == premise); // history, exactly once
        closed2(b1, "demolished on the unload step");
        validate_and_reload(without, "unload step");
    }
    // ARRIVAL: the carrier reaches the saltworks with ore in step k; the saltworks is demolished just before it
    {
        const int k = first_event_step([](const Terms2& a, const Terms2& b) { return a.ore_cargo > 0 && b.ore_cargo < a.ore_cargo; });
        REQUIRE(k > 0);
        XChain with = chain_at(k - 1), without = chain_at(k - 1);
        const Terms2 w0 = terms2(with);
        step_chain(*with.c, 1);
        CHECK(terms2(with).ore_cargo < w0.ore_cargo);
        const Terms2 b0 = terms2(without);
        const sim::BuildingId works = without.works[0];
        const std::uint64_t premise = in_flight(without, works, kSalzstein);
        REQUIRE(premise > 0);
        demolish_x(without, {{kWorksX, kWorksY}});
        step_chain(*without.c, 400);
        const Terms2 b1 = terms2(without);
        CHECK(b1.ore_lost - b0.ore_lost == premise);
        closed2(b1, "demolished on the arrival step");
        validate_and_reload(without, "arrival step");
    }
}

// ---- X4: two owners on one island ------------------------------------------------------------------

namespace {

struct Rival {
    SaltWorld sw;
    sim::PlayerId player;
    sim::CityRef city;
};

/// The pilot with a SECOND city on island 1 for another player: its territory is two rectangles cut
/// out of the pilot city's tiles (labelled fixture edit of the scenario spec, not a game command).
std::optional<Rival> rival_world(bool with_rival) {
    auto o = load_original();
    if (!o) return std::nullopt;
    auto resolved = anno::sim_load::load_scenario_with_links(
        anno::test::read_file(o->dir / "Scenes" / "0011SingleEndlosAddon" / "endless02.sze"), o->templates, *o->types,
        o->cfg, {}, original_component::actor_free_units());
    REQUIRE(resolved);
    auto component = original_component::import_only(resolved.value().loaded.spec, o->types, o->cfg);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    sim::ScenarioSpec spec = component.value().spec;
    Rival r{SaltWorld{}, sim::PlayerId{0}, sim::CityRef{sim::IslandId{1}, sim::CityNo{1}}};
    if (with_rival) {
        sim::ScenarioSpec::IslandSpec* isl = nullptr;
        for (auto& i : spec.islands) if (i.slot.v == 1) isl = &i;
        REQUIRE(isl != nullptr);
        const sim::ScenarioSpec::PlayerSpec* p8 = nullptr;
        for (const auto& p : spec.players) if (p.id == kSaltPlayer) p8 = &p;
        REQUIRE(p8 != nullptr);
        // a player with no city on this island: the first free slot above 8
        std::set<std::uint32_t> used;
        for (const auto& c : isl->cities) used.insert(c.owner.v);
        std::uint32_t rid = 9;
        while (used.count(rid) != 0) ++rid;
        sim::ScenarioSpec::PlayerSpec* rp = nullptr;
        for (auto& p : spec.players) if (p.id.v == rid) rp = &p;
        if (rp == nullptr) {
            spec.players.push_back(*p8);
            rp = &spec.players.back();
            rp->id = sim::PlayerId{rid};
            rp->relations.clear();
            rp->discovery.clear();
        } else {
            rp->permissions = p8->permissions;
            rp->flags = p8->flags;
        }
        rp->money = 100000;
        r.player = sim::PlayerId{rid};
        sim::ScenarioSpec::CitySpec c1 = isl->cities.at(0);
        c1.no = sim::CityNo{1};
        c1.owner = r.player;
        c1.buildings.clear();
        isl->cities.push_back(c1);
        // decode the owner runs, cut the rectangles, encode again
        std::vector<std::uint32_t> own;
        own.reserve(std::size_t{isl->w} * isl->h);
        for (const auto& [owner, len] : isl->owner_runs) own.insert(own.end(), len, owner);
        own.resize(std::size_t{isl->w} * isl->h, o->cfg.neutral_owner);
        const auto cut = [&](int xa, int xb, int ya, int yb) {
            for (int y = ya; y <= yb; ++y)
                for (int x = xa; x <= xb; ++x) {
                    auto& t = own[std::size_t(y - isl->y0) * isl->w + std::size_t(x - isl->x0)];
                    if (t == 0) t = 1;
                }
        };
        cut(340, 349, 352, 366); // west of the trunk road: room for a rival saltworks beside it
        cut(356, 362, 357, 366); // east of our saltworks: room for a rival mine in reach of the same ore
        isl->owner_runs.clear();
        for (std::size_t i = 0; i < own.size();) {
            std::size_t j = i;
            while (j < own.size() && own[j] == own[i]) ++j;
            isl->owner_runs.push_back({own[i], static_cast<std::uint32_t>(j - i)});
            i = j;
        }
    }
    sim::Rules rules;
    rules.house_updates = true;
    auto created = sim::World::create(spec, o->types, o->cfg, rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World world = std::move(created).value();
    if (world.script().state.context == sim::ScriptContext::Pending) REQUIRE(world.initialize_scripts().empty());
    for (int i = 0; i < 3; ++i) world.advance(kStepMs);
    r.sw = SaltWorld{std::move(world), o->types};
    return r;
}

} // namespace


namespace {

struct TwoOwners {
    XChain x;                                   // our chain (city 0, player 8)
    sim::BuildingId rival_mine, rival_works;    // city 1, player 9
    sim::PlayerId rival;
    bool has_rival = false;
};

/// Our pilot chain (mine at (359,369) rotation 1, 10 tiles from the saltworks) beside, when asked, a
/// rival city whose mine at (356,361) is CLOSER to our saltworks than ours and whose own saltworks
/// stands beside the same trunk road.
std::optional<TwoOwners> two_owners(bool with_rival, bool build_rival = true, bool rival_mine_on = true, bool rival_works_on = true) {
    auto r = rival_world(with_rival);
    if (!r) return std::nullopt;
    TwoOwners t;
    t.has_rival = with_rival;
    t.rival = r->player;
    t.x.c = std::make_unique<Chain>(Chain{std::move(r->sw), {}, {}, {}, 0, 0, 0, {}, {}});
    Chain& c = *t.x.c;
    sim::World& w = c.sw.w;
    sim::WorldTestAccess::islands(w)[1]->city(kSaltCity.city)->stock.stock[kSalzstein] = 0;
    sim::WorldTestAccess::islands(w)[1]->city(kSaltCity.city)->stock.stock[kSalz] = 0;
    c.works = find_type(w, 1022)->id;
    c.works_in0 = std::get<sim::Workshop>(find_type(w, 1022)->payload).in.at(0);
    w.set_transport_stats(&c.stats);
    c.salz_store0 = 0;
    c.ore_store0 = 0;
    t.x.works.push_back(c.works);
    if (with_rival && build_rival) {
        const auto place = [&](std::uint32_t type, int xa, int xb, int ya, int yb) -> sim::BuildingId {
            for (int y = ya; y <= yb; ++y)
                for (int x = xa; x <= xb; ++x) {
                    const auto res = w.apply(sim::Command{r->player, sim::BuildCmd{r->city, {sim::PlacementWire{sim::TypeId{type}, x, y, 0, 0, 0}}}});
                    if (res.ok()) return res.records.at(0).building;
                }
            return {};
        };
        if (rival_mine_on) t.rival_mine = place(1203, 356, 360, 361, 363);
        if (rival_works_on) t.rival_works = place(1022, 346, 346, 352, 364); // x 346..349: beside the trunk road at x = 350
    }
    c.mine = build_x(t.x, 1203, 359, 369, 1);
    return t;
}

sim::CityNo city_no_of(const sim::World& w, sim::BuildingId id) {
    const auto it = isle(w).cache.by_id.find(id);
    return it == isle(w).cache.by_id.end() ? sim::CityNo{0xffffU} : it->second.city.city;
}

} // namespace

namespace {

/// The rival city's own chain ledger (mine -> saltworks; it has no market, so its SALZ stays in its
/// saltworks): the same terms as ours, scoped to the rival's buildings and figures.
struct RivalTerms {
    std::uint64_t mine_made = 0, mine_out = 0, ore_loaded = 0, works_in = 0, works_consumed = 0, works_made = 0,
                  works_out = 0, salz_loaded = 0, ore_cargo = 0;
    std::int64_t u_mine = 0, u_ore = 0, u_made = 0;
};

RivalTerms rival_terms(TwoOwners& t) {
    RivalTerms r;
    sim::World& w = t.x.c->sw.w;
    const sim::TransportStats& st = t.x.c->stats;
    const auto get = [](const auto& m, const auto& k) -> std::uint64_t {
        const auto it = m.find(k);
        return it == m.end() ? 0U : it->second;
    };
    r.mine_made = get(st.forestry.mine_made_88, t.rival_mine);
    r.ore_loaded = get(st.loaded_from, t.rival_mine);
    if (const sim::Building* m = w.find_building(t.rival_mine)) r.mine_out = std::get<sim::Mine>(m->payload).out;
    if (const sim::Building* b = w.find_building(t.rival_works)) {
        const auto& ws = std::get<sim::Workshop>(b->payload);
        r.works_in = ws.in.at(0);
        r.works_out = ws.out.at(0);
    }
    if (const auto it = st.forestry.workshops.find(t.rival_works); it != st.forestry.workshops.end()) {
        r.works_consumed = it->second.consumed_88.at(0);
        r.works_made = it->second.made_88.at(0);
    }
    r.salz_loaded = get(st.loaded_from, t.rival_works);
    for (const auto& f : w.figures().list.slots)
        if (f && !f->expiry_stamp.has_value() && f->owner == t.rival && f->cargo_goods == kSalzstein && !f->resident)
            r.ore_cargo += f->cargo_88;
    const auto I = [](std::uint64_t v) { return static_cast<std::int64_t>(v); };
    r.u_mine = I(r.mine_made) - I(r.mine_out) - I(r.ore_loaded);
    r.u_ore = I(r.ore_loaded) - I(r.ore_cargo) - I(r.works_in) - I(r.works_consumed);
    r.u_made = I(r.works_made) - I(r.works_out) - I(r.salz_loaded);
    return r;
}

} // namespace

TEST_CASE("salt isolation X4: two owners credit their own city and never lift across") {
    auto two = two_owners(true);
    auto solo = two_owners(false);
    auto cut_only = two_owners(true, false); // the rival city and territory exist, but it builds nothing
    auto unbound = two_owners(true, true, true, false); // the rival's mine only, its source cut: a building that produces nothing
    auto gone = two_owners(true); // the rival's buildings, demolished at once
    if (!two || !solo || !cut_only || !unbound || !gone) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    REQUIRE(two->rival_mine.valid());
    REQUIRE(two->rival_works.valid());
    for (auto& l : sim::WorldTestAccess::islands(unbound->x.c->sw.w)[1]->city(sim::CityNo{1})->index.lists)
        for (auto& b : l.slots)
            if (b && b->id == unbound->rival_mine) std::get<sim::Mine>(b->payload).deposit.reset();
    {
        sim::DemolishCmd dem;
        dem.city = sim::CityRef{sim::IslandId{1}, sim::CityNo{1}};
        const sim::Building* rw = gone->x.c->sw.w.find_building(gone->rival_works);
        REQUIRE(rw != nullptr);
        dem.tiles = {{356, 361}, {rw->x, rw->y}};
        REQUIRE(gone->x.c->sw.w.apply(sim::Command{gone->rival, dem}).ok());
    }
    for (TwoOwners* t : {&*two, &*solo, &*cut_only, &*unbound, &*gone}) step_chain(*t->x.c, 3000);
    sim::World& w = two->x.c->sw.w;
    // the rival's mine works (in reach of the shared ore, closer to our saltworks than ours is) ...
    const auto& rm = std::get<sim::Mine>(w.find_building(two->rival_mine)->payload);
    CHECK(rm.deposit.has_value());
    CHECK(two->x.c->stats.forestry.mine_made_88[two->rival_mine] > 0);
    // ... and no load in the whole run crosses cities: the lifting figure's home and the source are in one city
    std::size_t resolved = 0;
    for (const auto& [key, n] : two->x.c->stats.loaded_by_home) {
        const sim::CityNo a = city_no_of(w, key.first), b = city_no_of(w, key.second);
        if (a.v == 0xffffU || b.v == 0xffffU) continue; // a building of another island
        ++resolved;
        CHECK_MESSAGE(a == b, "load of " << n << " from city " << a.v << " by a figure of city " << b.v);
    }
    CHECK(resolved > 0);
    // positive control: the rival's own chain runs (its saltworks, beside the same trunk road, lifts its own mine's
    // ore and makes salt) and ITS ledger closes, so the cross-owner zeros below are not vacuous
    CHECK(two->x.c->stats.loaded_by_home[{two->rival_mine, two->rival_works}] > 0U);
    {
        const RivalTerms rt = rival_terms(*two);
        CHECK(rt.works_made > 0);
        CHECK(rt.u_mine == 0);
        CHECK(rt.u_ore == 0);
        CHECK(rt.u_made == 0);
    }
    // our saltworks never lifted the rival's ore (this zero discriminates: the city-filter mutant makes it positive);
    // the rival's saltworks never lifted ours (this zero is a CONSISTENCY check only: under the mutant the rival
    // saltworks still prefers its nearer own mine, so the value does not change)
    CHECK(two->x.c->stats.loaded_by_home[{two->rival_mine, two->x.c->works}] == 0U);
    CHECK(two->x.c->stats.loaded_by_home[{two->x.mines[0], two->rival_works}] == 0U);
    // single-owner control: our ledger closes in both worlds and the rival changed none of OUR flows
    const Terms2 a = terms2(two->x), b = terms2(solo->x), c = terms2(cut_only->x), u = terms2(unbound->x), g = terms2(gone->x);
    closed2(a, "two owners");
    closed2(b, "single owner");
    closed2(c, "rival territory only");
    closed2(u, "rival mine, source cut");
    closed2(g, "rival buildings demolished");
    CHECK(a.ore_loaded > 0);
    CHECK(b.ore_loaded > 0);
    // The rival's ACTIVITY changes none of our flows: with its mine producing, bound to the shared deposit
    // and its saltworks standing, our flows equal those with the same mine standing idle (source cut, no
    // saltworks), exactly; and with its buildings demolished they equal the world with no rival buildings.
    // What a standing rival mine does change is OCCUPANCY: a building on the shared land moves our carriers'
    // routes a little (a.ore_loaded differs from c.ore_loaded), which is not a transfer between owners.
    for (const Terms2* x : {&u, &c}) {
        const Terms2& ref = x == &u ? a : g;
        CHECK(ref.ore_loaded == x->ore_loaded);
        CHECK(ref.works_made == x->works_made);
        CHECK(ref.unloaded == x->unloaded);
        CHECK(ref.taken == x->taken);
        CHECK(ref.delivered == x->delivered);
    }
    MESSAGE("our ore_loaded: two owners " << a.ore_loaded << ", rival territory only " << c.ore_loaded << ", single owner " << b.ore_loaded);
    // both mines bind the one deposit of the shared ore: ore is an island object (as in the original), the
    // OUTPUT stores, lifts and credits are per city
    CHECK(isle(w).deposits.size() == 1);
    CHECK(w.check_invariants().empty());
}


TEST_CASE("salt interruption X4: the Kontor is not a link of the chain") {
    // The warehouse (Kontor 1306) is far from the salt chain; the city store is the CITY's, reached by the
    // markets' carts at unload and by the stalls at purchase, not through the Kontor building. Demolishing it
    // leaves production, receipts, purchases and deliveries going (ledger closed, save/load valid), and the
    // salt chain's flows stay positive in every window.
    sim::Reject st = sim::Reject::Unsupported;
    const auto interrupt = [&](XChain& x) {
        sim::DemolishCmd dem;
        dem.city = kSaltCity;
        dem.tiles = {{268, 293}};
        REQUIRE(building_on(x.c->sw.w, 268, 293) != nullptr);
        REQUIRE(building_on(x.c->sw.w, 268, 293)->type.v == 1306);
        st = x.c->sw.w.apply(sim::Command{kSaltPlayer, dem}).status;
    };
    const auto restore = [](XChain&) {};
    const Windows w = run_interruption("kontor", interrupt, restore, false);
    if (w.p1.empty()) { MESSAGE("skipping: ANNO1503_GAME_DIR not set"); return; }
    CHECK(st == sim::Reject::None);
    CHECK(all_positive(w.p2, 1, true, true, true, true, true));
    CHECK(all_positive(w.p3, 0, true, true, true, true, true));
    const Windows control = control_run();
    const auto same = [](const std::vector<Snap>& a, const std::vector<Snap>& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].works_made != b[i].works_made || a[i].mine_made != b[i].mine_made || a[i].unloaded != b[i].unloaded ||
                a[i].taken != b[i].taken || a[i].delivered != b[i].delivered)
                return false;
        return true;
    };
    // before the cut the two runs are identical; after it the flows stay positive but are NOT byte-equal to the
    // control (the Kontor's carts serve other producers of the city, so its removal shifts timing elsewhere:
    // the chain's windows move by a few units, the claim is only that no window of the salt chain stops)
    CHECK(same(w.p1, control.p1));
    // PRNG: only the existing state-following sites may differ (the Kontor's removal is not a no-op for the
    // rest of the economy, unlike the instant demolish-and-rebuild of the chain's own links)
    const Windows quiet = run_interruption("kontor-quiet", interrupt, restore, false, false, false);
    const Windows quiet_control = control_run(false);
    const auto minus3 = [](std::map<sim::DrawSite, std::uint64_t> m) {
        m.erase(sim::DrawSite::HouseWalkerTarget);
        m.erase(sim::DrawSite::HouseVariant);
        m.erase(sim::DrawSite::FarmWorkerDispatch);
        return m;
    };
    CHECK(minus3(quiet.draws) == minus3(quiet_control.draws));
    CHECK(minus3(w.draws) == minus3(control.draws));
    CHECK(quiet.start_salz == 0);
    CHECK(quiet_control.start_salz == 0);
}

TEST_CASE("salt mine: a refused mine command is logged and the log reproduces") {
    // Reject::NoDepositInReach is appended at the end of the enum. A Reject is a host-facing answer: it is
    // in no command wire form, no replay log, no saved state and no hash (a log holds the commands and the
    // checkpoint hashes), so the append needs no format or stream version change. This test records a
    // refused command between accepted ones, writes and reads the log, and replays it from the initial save.
    auto s = salt_world(true, false, true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto started = sim::Recorder::start(s->w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    const auto before = s->w.save();
    const sim::ApplyResult refused = rec.submit(build_cmd({mine_at(335, 340)}));
    CHECK(refused.status == sim::Reject::NoDepositInReach);
    CHECK(s->w.save() == before); // nothing changed; the command is nevertheless in the log
    rec.advance(100);
    REQUIRE(rec.submit(build_cmd({mine_at(353, 359)})).ok());
    for (int i = 0; i < 20; ++i) rec.advance(100);
    rec.checkpoint();
    const auto bytes = sim::write_log(rec.log());
    REQUIRE_MESSAGE(bytes.ok(), (bytes.ok() ? std::string() : bytes.error()));
    auto read = sim::read_log(bytes.value());
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    CHECK(read.value() == rec.log());
    auto fresh = sim::World::load(rec.log().initial_save, s->types, {});
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    auto replayed = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(replayed.ok(), (replayed.ok() ? std::string() : replayed.error()));
    CHECK(replayed.value().reproduced());
    CHECK(fresh.value().save() == s->w.save());
}


// ---- X5: read-only facts, continuation, fresh-process replay, bounded lookup cost -------------------------

TEST_CASE("salt X5: the mine facts against known state, read-only") {
    auto s = salt_world(true, false, true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    sim::TransportStats stats;
    const sim::ApplyResult r = w.apply(build_cmd({mine_at(353, 359)}));
    REQUIRE(r.ok());
    const sim::BuildingId mine = r.records.at(0).building;
    // The expected values are independent of the code under test: the ore at (357,369) with 2,000 t is the OCCUR
    // object read from endless02 (cache/close-extract/endless02_i1.txt line "R 1562 357 369 2000"), the load rule
    // amount<<8 raised to 2,000 t = 512,000 in 8.8 units with the flag set is OQ-035 b/c
    // (fixtures/synthetic/sim/mine_terrain_vectors.txt deposit vectors).
    const auto before = w.save();
    const auto f = sim::mine_facts(w, mine);
    REQUIRE(f.has_value());
    CHECK(f->bound);
    CHECK(f->deposit_index == std::optional<std::size_t>{0});
    CHECK(f->ore_x == 357);
    CHECK(f->ore_y == 369);
    CHECK(f->remaining_88 == 512000U);
    CHECK(f->reserve);
    CHECK(f->deep);
    CHECK_FALSE(f->depletes); // a salt mine is deep: even in the add-on edition (setting on) it never uses the deposit up
    CHECK(sim::WorldTestAccess::settings(w).depletes());
    CHECK(f->state == sim::MineState::Working);
    CHECK(f->output_88 == 0);
    CHECK_FALSE(f->produced_88.has_value()); // no diagnostic sink installed
    CHECK(w.save() == before); // reading changed nothing, byte for byte
    // with a sink the produced total appears and equals the ledger; the output store equals the Mine's
    w.set_transport_stats(&stats);
    for (int i = 0; i < 400; ++i) w.advance(kStepMs);
    const auto g = sim::mine_facts(w, mine);
    REQUIRE(g.has_value());
    REQUIRE(g->produced_88.has_value());
    CHECK(*g->produced_88 > 0);
    CHECK(*g->produced_88 == stats.forestry.mine_made_88.at(mine));
    CHECK(g->output_88 == std::get<sim::Mine>(w.find_building(mine)->payload).out);
    CHECK(g->remaining_88 == 512000U); // deep, flagged: worked, not drawn down
    const auto mid = w.save();
    static_cast<void>(sim::mine_facts(w, mine));
    CHECK(w.save() == mid);
    // the imported mine is unbound and idle; a non-mine and an unknown id answer nothing
    const sim::Building* imported = nullptr;
    const sim::Building* works = nullptr;
    for (const auto& l : isle(w).city(kSaltCity.city)->index.lists)
        for (const auto& b : l.slots) {
            if (b && b->type.v == 1203 && b->id != mine) imported = &*b;
            if (b && b->type.v == 1022) works = &*b;
        }
    REQUIRE(imported != nullptr);
    REQUIRE(works != nullptr);
    const auto u = sim::mine_facts(w, imported->id);
    REQUIRE(u.has_value());
    CHECK_FALSE(u->bound);
    CHECK_FALSE(u->deposit_index.has_value());
    CHECK(u->state == sim::MineState::Unbound);
    CHECK_FALSE(sim::mine_facts(w, works->id).has_value());
    CHECK_FALSE(sim::mine_facts(w, sim::BuildingId{}).has_value());
    CHECK(w.save() == mid);
}

TEST_CASE("salt X5: the depletes fact follows the extraction rule for every combination") {
    auto s = salt_world(true, false, true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    const sim::ApplyResult r = w.apply(build_cmd({mine_at(353, 359)}));
    REQUIRE(r.ok());
    const sim::BuildingId id = r.records.at(0).building;
    sim::Island& is = *sim::WorldTestAccess::islands(w)[1];
    sim::Mine* m = nullptr;
    for (auto& l : is.city(kSaltCity.city)->index.lists)
        for (auto& b : l.slots)
            if (b && b->id == id) m = &std::get<sim::Mine>(b->payload);
    REQUIRE(m != nullptr);
    // hand-written truth table (reserve, deep, setting) -> depletes (docs/re/production.md 4.4)
    struct Row {
        bool reserve, deep, setting, want;
    };
    for (const Row row : {Row{true, true, true, false}, Row{true, true, false, false}, Row{true, false, true, true},
                          Row{true, false, false, false}, Row{false, true, true, true}, Row{false, true, false, false},
                          Row{false, false, true, true}, Row{false, false, false, false}}) {
        is.deposits[0].reserve = row.reserve;
        is.deposits[0].remaining = 2000U * sim::kTon; // above the 1,600 t reserve floor
        m->deep = row.deep;
        sim::WorldTestAccess::settings(w).prop14 = row.setting ? 3U : 1U;
        INFO("reserve " << row.reserve << " deep " << row.deep << " setting " << row.setting);
        const auto f = sim::mine_facts(w, id);
        REQUIRE(f.has_value());
        CHECK(f->depletes == row.want);
        // and the extraction code agrees: the deposit really goes down exactly when the fact says so
        sim::Deposit probe = is.deposits[0];
        const std::uint32_t got = sim::deposit_extract(probe, 100U, row.deep, row.setting);
        CHECK(got <= 100U);
        CHECK((probe.remaining != is.deposits[0].remaining) == row.want);
    }
    // off and blocked states from the Mine's own switches
    sim::WorldTestAccess::settings(w).prop14 = 3U;
    is.deposits[0].reserve = true;
    m->deep = true;
    m->switched_on = false;
    CHECK(sim::mine_facts(w, id)->state == sim::MineState::SwitchedOff);
    m->switched_on = true;
    m->blocked = true;
    CHECK(sim::mine_facts(w, id)->state == sim::MineState::Blocked);
    m->blocked = false;
    CHECK(sim::mine_facts(w, id)->state == sim::MineState::Working);
}

namespace {

/// Saves at the first step where `at` holds, loads that save into a second world and continues both for 800
/// steps: the bytes, the hash and the PRNG cursor must stay equal (no stats sink in the loaded world: the
/// sink is diagnostic and does not decide anything).
template <class At>
void continuation_at(const char* boundary, At at) {
    XChain x = start_x(true);
    if (!x.c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    INFO("boundary: " << boundary);
    const int k = step_until(x, 4000, [&] { return at(x); });
    REQUIRE_MESSAGE(k > 0, "the boundary state was never reached");
    sim::World& w = x.c->sw.w;
    const auto bytes = w.save();
    auto loaded = sim::World::load(bytes, x.c->sw.types, {});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World& re = loaded.value();
    CHECK(re.check_invariants().empty());
    CHECK(re.save() == bytes);
    CHECK(re.state_hash() == w.state_hash());
    for (int i = 0; i < 800; ++i) {
        w.advance(kStepMs);
        re.advance(kStepMs);
        const bool faulted = w.faulted() || re.faulted();
        REQUIRE_MESSAGE(!faulted, w.fault());
    }
    CHECK(re.state_hash() == w.state_hash());
    CHECK(re.rng().cursor() == w.rng().cursor());
    CHECK(re.save() == w.save());
    CHECK(re.check_invariants().empty());
}

} // namespace

TEST_CASE("salt X5: a reload at each boundary of the chain continues like the uninterrupted run") {
    // the mine: the step in which a carrier lifts ore from it
    {
        std::uint64_t seen = 0;
        continuation_at("mine, at a load", [&](XChain& x) {
            const auto it = x.c->stats.loaded_from.find(x.mines[0]);
            const std::uint64_t now = it == x.c->stats.loaded_from.end() ? 0U : it->second;
            const bool hit = now > seen;
            seen = now;
            return hit;
        });
    }
    // the saltworks carrier with ore in its hands
    continuation_at("saltworks carrier holds ore", [&](XChain& x) { return in_flight(x, x.works[0], kSalzstein) > 0; });
    // a market cart (the stall's side of the chain) with SALZ in its hands
    continuation_at("market cart holds SALZ", [&](XChain& x) {
        const sim::Building* a = building_on(x.c->sw.w, 346, 346);
        const sim::Building* b = building_on(x.c->sw.w, 346, 371);
        return a != nullptr && b != nullptr && in_flight(x, a->id, kSalz) + in_flight(x, b->id, kSalz) > 0;
    });
    // a household walker with SALZ in its hands
    continuation_at("household walker holds SALZ", [&](XChain& x) { return terms2(x).res_cargo > 0; });
}

TEST_CASE("salt X5: the chain recorded for the fresh-process replay and its changed command") {
    // A real session: the mine is built by a public command, the chain runs 1,500 steps with a checkpoint every
    // 50. The same log is replayed in-process here; with ANNO_SALT_OUT_DIR set the log and a changed-command
    // twin (the mine placed at (356,359) instead of (353,359)) are also written for the fresh-process CTests
    // (anno-sim-replay in a child process, cmake/SaltChainProperties.cmake.in).
    auto s = salt_world(true, false, true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto started = sim::Recorder::start(s->w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    REQUIRE(rec.submit(build_cmd({mine_at(353, 359)})).ok());
    rec.advance(100);
    rec.checkpoint();
    for (int block = 0; block < 30; ++block) {
        for (int i = 0; i < 50; ++i) rec.advance(100);
        rec.checkpoint();
    }
    REQUIRE(s->w.check_invariants().empty());
    sim::ReplayLog changed = rec.log();
    std::size_t command_entry = 0, first_checkpoint = 0;
    for (std::size_t i = 0; i < changed.entries.size(); ++i) {
        if (changed.entries[i].kind == sim::ReplayEntry::Kind::Commands && command_entry == 0) {
            command_entry = i;
            std::get<sim::BuildCmd>(changed.entries[i].commands[0].body).placements.at(0).x = 356;
        }
        if (changed.entries[i].kind == sim::ReplayEntry::Kind::Checkpoint && first_checkpoint == 0) first_checkpoint = i;
    }
    REQUIRE(first_checkpoint > command_entry);
    const auto good_bytes = sim::write_log(rec.log());
    const auto bad_bytes = sim::write_log(changed);
    REQUIRE(good_bytes.ok());
    REQUIRE(bad_bytes.ok());
    // in-process: the log reproduces all 31 checkpoints, the changed twin diverges at the first one
    {
        auto fresh = sim::World::load(rec.log().initial_save, s->types, {});
        REQUIRE(fresh.ok());
        auto out = sim::replay(fresh.value(), rec.log());
        REQUIRE(out.ok());
        CHECK(out.value().checkpoints_total == 31);
        CHECK(out.value().checkpoints_matched == 31);
        CHECK(out.value().reproduced());
    }
    {
        auto fresh = sim::World::load(changed.initial_save, s->types, {});
        REQUIRE(fresh.ok());
        auto out = sim::replay(fresh.value(), changed);
        REQUIRE(out.ok());
        REQUIRE(out.value().diverged_entry.has_value());
        CHECK(*out.value().diverged_entry == first_checkpoint);
        CHECK(out.value().checkpoints_matched == 0);
    }
    MESSAGE("command entry " << command_entry << ", first checkpoint entry " << first_checkpoint);
    if (const char* dir = std::getenv("ANNO_SALT_OUT_DIR"); dir != nullptr && *dir != '\0') {
        const std::filesystem::path out(dir);
        const auto write = [&](const char* name, const std::vector<std::uint8_t>& bytes) {
            std::ofstream f(out / name, std::ios::binary);
            REQUIRE(f.good());
            f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        };
        write("salt-chain.log", good_bytes.value());
        write("salt-chain-changed-command.log", bad_bytes.value());
    }
}

// ---- X5: bounded cost of the reach lookup and the bind -----------------------------------------------

namespace {

/// An independent copy of the reach table (docs/re/production.md 4.4: rows 0..7 beyond the footprint have
/// half-widths 7, 7, 7, 6, 6, 5, 4, 2), written from the document, not from the code under test.
bool reach_ref(std::int32_t ax, std::int32_t ay, std::int32_t w, std::int32_t h, std::int32_t ox, std::int32_t oy) {
    static const int half[8] = {7, 7, 7, 6, 6, 5, 4, 2};
    int dy = oy < ay ? ay - oy : (oy > ay + h - 1 ? oy - (ay + h - 1) : 0);
    int dx = ox < ax ? ax - ox : (ox > ax + w - 1 ? ox - (ax + w - 1) : 0);
    return dy <= 7 && dx <= half[dy];
}

struct OreIsland {
    sim::Island is;
    std::size_t in_reach_expected = 0;
};

/// A bare island of 400 x 400 tiles with `k` ore objects of the salt ore symbol 1562, `near` of them inside
/// the reach of a 3 x 4 mine at (200,200) and the rest far away on a grid, and `deposits` unrelated deposits.
OreIsland ore_island(std::size_t k, std::size_t near_count, std::size_t deposits) {
    OreIsland o;
    REQUIRE(o.is.tiles.reset(0, 0, 400, 400, 8));
    for (std::size_t i = 0; i < k; ++i) {
        sim::OreObject ob;
        ob.type = 1562;
        ob.amount = 2000;
        if (i < near_count) { // a column of tiles just below the footprint: all within the table
            ob.x = 200 + static_cast<std::int32_t>(i % 3);
            ob.y = 204 + static_cast<std::int32_t>(i / 3 % 4);
        } else { // far: x in 0..99, y in 0..99 (the footprint is at 200,200: out of reach)
            ob.x = static_cast<std::int32_t>(i % 100);
            ob.y = static_cast<std::int32_t>(i / 100 % 100);
        }
        o.is.ore_objects.push_back(ob);
        o.in_reach_expected += reach_ref(200, 200, 3, 4, ob.x, ob.y) ? 1U : 0U;
    }
    for (std::size_t d = 0; d < deposits; ++d)
        o.is.deposits.push_back(sim::Deposit{"EISENERZ", 300 + static_cast<std::int32_t>(d % 90), 300 + static_cast<std::int32_t>(d / 90), 1000U, true});
    return o;
}

} // namespace

TEST_CASE("salt X5: the lookup visits the ore objects of its island once and nothing else") {
    auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::BuildingType& mine = *o->types->find(sim::TypeId{1203});
    const sim::MineArea area{200, 200, 3, 4};
    std::vector<sim::OreLookupStats> series;
    for (const std::size_t k : {10U, 100U, 1000U, 10000U}) {
        const std::size_t deposits = 25;
        OreIsland oi = ore_island(k, 5, deposits);
        REQUIRE(oi.in_reach_expected == 5); // the independent reach count, constant while k grows
        sim::OreLookupStats st;
        const auto found = sim::find_ore_in_reach(oi.is, *o->types, mine, area, true, &st);
        INFO("ore objects " << k);
        CHECK(found.has_value());
        CHECK(st.lookups == 1);
        CHECK(st.ore_objects_examined == k);                  // linear in the island's ore objects, exactly
        CHECK(st.in_reach == oi.in_reach_expected);           // equals the independent count
        CHECK(st.ware_matches == oi.in_reach_expected);       // all of them are salt
        CHECK(st.deposits_examined == oi.in_reach_expected * deposits); // per in-reach ore, the island's deposit table
        series.push_back(st);
    }
    // what does NOT grow with the number of ore objects: the in-reach work
    for (const auto& st : series) {
        CHECK(st.in_reach == series.front().in_reach);
        CHECK(st.deposits_examined == series.front().deposits_examined);
    }
    // the bind adds one pass over the deposit table (the dedupe) and nothing else
    {
        OreIsland oi = ore_island(1000, 5, 25);
        sim::Building b;
        b.type = sim::TypeId{1203};
        b.x = 200;
        b.y = 200;
        b.rotation = 0;
        sim::OreLookupStats st;
        const auto idx = sim::bind_mine_deposit(oi.is, *o->types, b, mine, true, &st);
        REQUIRE(idx.has_value());
        CHECK(st.ore_objects_examined == 1000);
        CHECK(st.deposits_examined == 5 * 25 + 25); // lookup + the dedupe scan, once each
        CHECK(oi.is.deposits.size() == 26);
    }
}

TEST_CASE("salt X5: the cost of a command's lookup does not depend on cities or other mines") {
    // the same BuildCmd of our mine in three worlds: the pilot alone, the pilot with a second city, and the pilot
    // with a second city holding a mine and a saltworks. Per lookup the ore objects visited equal the island's
    // ore objects in every world; the number of cities and mines changes nothing.
    auto solo = two_owners(false);
    auto cut = two_owners(true, false);
    auto both = two_owners(true);
    if (!solo || !cut || !both) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::size_t lookups_seen = 0;
    for (TwoOwners* t : {&*solo, &*cut, &*both}) {
        const sim::OreLookupStats& st = t->x.c->stats.ore_lookup;
        INFO("cities " << (t == &*solo ? 1 : 2) << ", ore lookups " << st.lookups);
        REQUIRE(st.lookups >= 2); // our command: the check and the bind (the rival's add two more)
        CHECK(st.ore_objects_examined == st.lookups * isle(t->x.c->sw.w).ore_objects.size());
        CHECK(st.in_reach <= st.ore_objects_examined);
        lookups_seen += st.lookups;
    }
    CHECK(solo->x.c->stats.ore_lookup.lookups == 2);
    CHECK(cut->x.c->stats.ore_lookup.lookups == 2);
    CHECK(both->x.c->stats.ore_lookup.lookups == 4);
    // our own command's lookups: identical counts in every world
    CHECK(solo->x.c->stats.ore_lookup.ore_objects_examined / 2 == both->x.c->stats.ore_lookup.ore_objects_examined / 4);
    CHECK(lookups_seen == 8);
}

TEST_CASE("salt X5: lookup timing sample (opt-in measure)") {
    const char* on = std::getenv("ANNO_SALT_PERF");
    if (on == nullptr || *on == '\0') {
        MESSAGE("skipping: set ANNO_SALT_PERF=1 to run the lookup timing sample");
        return;
    }
    auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::BuildingType& mine = *o->types->find(sim::TypeId{1203});
    const sim::MineArea area{200, 200, 3, 4};
    for (const std::size_t k : {100U, 1000U, 10000U, 100000U}) {
        OreIsland oi = ore_island(k, 5, 25);
        sim::OreLookupStats st;
        const int reps = static_cast<int>(2000000 / k) + 10;
        const auto t0 = std::chrono::steady_clock::now();
        std::size_t sink = 0;
        for (int r = 0; r < reps; ++r) sink += sim::find_ore_in_reach(oi.is, *o->types, mine, area, true, &st).value_or(0);
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / reps;
        std::cout << "PERF ore_objects " << k << " reps " << reps << " us_per_lookup " << us << " examined_per_lookup "
                  << st.ore_objects_examined / st.lookups << " sink " << sink << "\n";
    }
}
