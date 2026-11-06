// Stream 135, X2: the mountain-class placement and the deposit binding of a player-built mine
// (docs/sim/salt-extraction.md sections 11-15). Layer 1 runs the hand-derived vectors through the
// engine; layer 2 builds in the original endless02 island 1 (owner 8) through PUBLIC commands.
// Needs ANNO1503_GAME_DIR for layer 2.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
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

std::optional<SaltWorld> salt_world(bool started, bool atomic = false) {
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
    // classes the evidence does not cover keep the typed refusal, on the otherwise legal site
    for (const sim::BuildingType& t : s->types->types()) {
        const bool mountain = (t.flags_word & 0x7000U) == 0x3000U;
        if (!mountain || t.id == kSaltMine) {
            continue;
        }
        const bool deep_mine = t.cls == sim::RuntimeClass::Mine && t.has_prod &&
                               sim::mine_is_deep(t.prod, t.research_id);
        if (deep_mine) {
            continue; // another deep mine would be placeable (permission aside)
        }
        sim::World w = base;
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd({sim::PlacementWire{t.id, 353, 359, 0, 0, 0}}));
        CHECK_MESSAGE(!r.ok(), "type " << t.id.v);
        CHECK_MESSAGE(r.status != sim::Reject::None, "type " << t.id.v);
        CHECK(w.save() == before);
    }
    {
        // the quarry 1201 is Unsupported when its permission is not the reason
        sim::World w = base;
        const sim::ApplyResult r = w.apply(build_cmd({sim::PlacementWire{sim::TypeId{1201}, 353, 359, 0, 0, 0}}));
        const bool typed = r.status == sim::Reject::Unsupported || r.status == sim::Reject::ConstructionDenied;
        CHECK_MESSAGE(typed, sim::reject_name(r.status));
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
    }
    // payment: no gold
    {
        sim::World w = base;
        sim::WorldTestAccess::players(w).find(kSaltPlayer)->money = 0;
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd({mine_at(353, 359)}));
        CHECK(r.status == sim::Reject::CannotAfford);
        CHECK(w.save() == before);
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

TEST_CASE("salt mine: policy A accepts and pays a mine with no ore in reach and leaves it idle") {
    auto s = salt_world(true);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    const std::int64_t gold0 = gold(w);
    const sim::ApplyResult r = w.apply(build_cmd({mine_at(335, 340)}));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    CHECK(gold(w) < gold0);
    CHECK(isle(w).deposits.empty());
    const auto* mine = std::get_if<sim::Mine>(&w.find_building(r.records.at(0).building)->payload);
    REQUIRE(mine != nullptr);
    CHECK_FALSE(mine->deposit.has_value());
    for (int i = 0; i < 20; ++i) {
        w.advance(kStepMs);
        REQUIRE_MESSAGE(!w.faulted(), w.fault());
    }
    CHECK(std::get<sim::Mine>(w.find_building(r.records.at(0).building)->payload).out == 0);
    CHECK(isle(w).deposits.empty());
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
