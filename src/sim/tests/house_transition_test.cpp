// MAP-POP-LIVE-1 L3: the House level transitions and removal as production-path operations
// (docs/sim/population-live.md sections 5, 6, 12, 16). The numeric rules run against the
// hand-derived vectors of fixtures/synthetic/sim/house_transition_vectors.txt (the independent
// reference tools/house_transition_ref.py reads the same file); the world tests pin the gates, the
// order of the effects, the counters, the draw traces and the removal. The operations are not wired
// into the tick.
//
// Hand derivations used below (E = 640, group 0 needs goods 26 (Menge 41) and 27 (Menge 16); group
// 1 needs 26 (Menge 60) and 27 (Menge 40)):
//  * stock0 2000 -> s = trunc(2000*1536/(640*41)) = 117; stock1 3200 -> min(480, 160) = 160;
//    new stock = trunc(60*117*640/1536) = 2925 and trunc(40*160*640/1536) = 2666.
//  * UpgradeCheck food (NEXT Menge0 60): trunc(stock0*1536/(640*60)) = stock0/25; the food table is
//  0
//    below index 10, so the score must be >= 76: stock0 1875 -> 75 -> index 9 -> 0 (blocked), 1900
//    -> 76
//    -> index 10 -> 5 (passes).
//  * downgrade of group 1 -> 0 with E 2000, residents_64 1280: s = min(30000*1536/(2000*60), 160) =
//  160,
//    min(8000*1536/(2000*40), 160) = 153; E' = (1280/64) << 7 = 2560; new stock
//    trunc(41*160*2560/1536) = 10933 and trunc(16*153*2560/1536) = 4080.
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/driver.hpp>
#include <anno/sim/goods_metadata.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/resident_job.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

// ------------------------------------------------------------------ the vector file

std::vector<std::string> words(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string w; in >> w;) {
        out.push_back(w);
    }
    return out;
}

using KV = std::map<std::string, std::string>;

KV kv_of(const std::vector<std::string>& parts, std::size_t from, std::size_t to) {
    KV kv;
    for (std::size_t i = from; i < to; ++i) {
        const auto eq = parts[i].find('=');
        REQUIRE(eq != std::string::npos);
        kv[parts[i].substr(0, eq)] = parts[i].substr(eq + 1);
    }
    return kv;
}

std::vector<std::int64_t> ints(const std::string& s) {
    std::vector<std::int64_t> out;
    if (s == "none" || s.empty()) {
        return out;
    }
    std::istringstream in(s);
    for (std::string t; std::getline(in, t, ',');) {
        out.push_back(std::stoll(t));
    }
    return out;
}

std::string join(const std::vector<std::int64_t>& v) {
    if (v.empty()) {
        return "none";
    }
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        s += (i ? "," : "") + std::to_string(v[i]);
    }
    return s;
}

std::map<std::int64_t, std::int64_t> pairs(const std::string& s) {
    std::map<std::int64_t, std::int64_t> m;
    if (s == "none") {
        return m;
    }
    std::istringstream in(s);
    for (std::string t; std::getline(in, t, ',');) {
        const auto c = t.find(':');
        m[std::stoll(t.substr(0, c))] = std::stoll(t.substr(c + 1));
    }
    return m;
}

std::array<std::int8_t, 21> table(const std::string& s) {
    std::array<std::int8_t, 21> t{};
    std::size_t at = 0;
    std::istringstream in(s);
    for (std::string x; std::getline(in, x, ',');) {
        const auto star = x.find('*');
        const int v = std::stoi(x.substr(0, star));
        for (int n = std::stoi(x.substr(star + 1)); n > 0; --n) {
            REQUIRE(at < t.size());
            t[at++] = static_cast<std::int8_t>(v);
        }
    }
    REQUIRE(at == t.size());
    return t;
}

struct VGroup {
    sim::HouseGroup group;
    sim::HouseGroupDerived derived;
};

struct Scripted final : sim::DrawSource {
    std::vector<std::uint16_t> values;
    std::vector<sim::DrawSite> sites;
    std::uint16_t next(sim::DrawSite site) override {
        sites.push_back(site);
        REQUIRE(sites.size() <= values.size());
        return values[sites.size() - 1];
    }
};

sim::PopulationView view_of(const VGroup& g, std::span<const sim::GoodsInfo> goods,
                            std::int64_t tavern) {
    sim::PopulationView v;
    v.group = &g.group;
    v.derived = &g.derived;
    v.goods = goods;
    v.roles.tavern_goods = static_cast<sim::GoodsId>(tavern < 0 ? 1000 : tavern);
    return v;
}

} // namespace

TEST_CASE("house transition: the hand-derived vectors (reference agrees, see CTest)") {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/house_transition_vectors.txt");
    REQUIRE(in.good());
    std::map<std::string, VGroup> groups;
    std::map<std::string, std::map<std::int64_t, std::int64_t>> goods_sec;
    std::vector<sim::GoodsInfo> goods = sim::original_goods();
    std::string raw;
    std::size_t executed = 0, lineno = 0;
    while (std::getline(in, raw)) {
        ++lineno;
        const std::string line = raw.substr(0, raw.find('#'));
        const std::vector<std::string> parts = words(line);
        if (parts.empty()) {
            continue;
        }
        const std::string& kind = parts[0];
        const std::string where =
            "vector line " + std::to_string(lineno) + " " + (parts.size() > 1 ? parts[1] : "");
        CAPTURE(where);
        if (kind == "goods") {
            const KV kv = kv_of(parts, 2, parts.size());
            goods = sim::original_goods();
            for (const auto& [a, b] : pairs(kv.count("sec") ? kv.at("sec") : "none")) {
                goods[static_cast<std::size_t>(a)].id_sec = static_cast<sim::GoodsId>(b);
            }
            continue;
        }
        if (kind == "grp") {
            const KV kv = kv_of(parts, 2, parts.size());
            VGroup g;
            g.group.number = static_cast<std::uint32_t>(std::stoul(kv.at("num")));
            g.group.wohnmax =
                static_cast<std::uint16_t>(kv.count("wohn") ? std::stoul(kv.at("wohn")) : 0);
            std::istringstream needs(kv.at("needs"));
            for (std::string t; std::getline(needs, t, ',');) {
                const auto a = t.find(':'), b = t.rfind(':');
                g.group.needs.push_back(sim::HouseNeed{
                    static_cast<sim::GoodsId>(std::stoul(t.substr(0, a))), t.substr(b + 1) == "1",
                    static_cast<std::uint16_t>(std::stoul(t.substr(a + 1, b - a - 1)))});
            }
            g.derived.need_count = static_cast<std::uint32_t>(g.group.needs.size());
            g.derived.goods_end = kv.count("goods_end")
                                      ? static_cast<std::uint32_t>(std::stoul(kv.at("goods_end")))
                                      : 0;
            g.derived.infra_end = kv.count("infra_end")
                                      ? static_cast<std::uint32_t>(std::stoul(kv.at("infra_end")))
                                      : g.derived.need_count;
            g.derived.lookup.assign(96, g.derived.need_count);
            for (const auto& [ware, slot] : pairs(kv.count("lookup") ? kv.at("lookup") : "none")) {
                g.derived.lookup[static_cast<std::size_t>(ware)] = static_cast<std::uint32_t>(slot);
            }
            groups[parts[1]] = std::move(g);
            continue;
        }
        const auto arrow = std::find(parts.begin(), parts.end(), "=>");
        REQUIRE(arrow != parts.end());
        const std::size_t split = static_cast<std::size_t>(arrow - parts.begin());
        const KV in_kv = kv_of(parts, 2, split);
        const KV want = kv_of(parts, split + 1, parts.size());
        KV got;
        if (kind == "ladder") {
            sim::TypeRegistry::Builder b;
            for (const auto id : ints(in_kv.at("have"))) {
                b.add_plain(sim::TypeId{static_cast<std::uint32_t>(id)}, 97);
            }
            sim::SimConfig cfg;
            cfg.house_type_stride = static_cast<std::uint32_t>(std::stoul(in_kv.at("stride")));
            auto reg = b.build();
            REQUIRE(reg.ok());
            sim::BuildingType t;
            t.id = sim::TypeId{static_cast<std::uint32_t>(std::stoul(in_kv.at("id")))};
            const sim::TypeLadder l = sim::house_type_ladder(cfg, *reg.value(), t);
            got["next"] = l.next ? std::to_string(l.next->id.v) : "none";
            got["prev"] = l.previous ? std::to_string(l.previous->id.v) : "none";
        } else if (kind == "variant") {
            sim::VariantFacts vf{static_cast<std::uint32_t>(std::stoul(in_kv.at("count"))),
                                 static_cast<std::uint32_t>(std::stoul(in_kv.at("step")))};
            const auto r = static_cast<std::uint16_t>(std::stoul(in_kv.at("r")));
            const std::uint32_t idx = sim::house_variant_index(vf, r);
            got["draw"] = vf.count != 0 ? "1" : "0";
            got["idx"] = std::to_string(idx);
            got["type"] = std::to_string(std::stoul(in_kv.at("base")) + idx * vf.step);
        } else if (kind == "matgate") {
            sim::SimConfig cfg;
            sim::City city = sim::make_city(sim::CityNo{0}, sim::PlayerId{0}, cfg);
            const auto amounts_i = ints(in_kv.at("amounts"));
            std::vector<std::uint32_t> amounts(amounts_i.begin(), amounts_i.end());
            const auto store = ints(in_kv.at("store")), setting = ints(in_kv.at("setting")),
                       spent = ints(in_kv.at("spent"));
            for (std::size_t k = 0; k < 5; ++k) {
                city.stock.stock[0x29 + k] = static_cast<std::uint32_t>(store[k]);
                city.build_goods[k].setting = static_cast<std::uint32_t>(setting[k]);
                city.build_goods[k].spent = static_cast<std::uint64_t>(spent[k]);
            }
            const bool ok = sim::house_materials_available(cfg, city, amounts);
            if (ok) {
                sim::house_spend_materials(cfg, city, amounts);
            }
            std::vector<std::int64_t> s_after, p_after;
            for (std::size_t k = 0; k < 5; ++k) {
                s_after.push_back(city.stock.stock[0x29 + k]);
                p_after.push_back(static_cast<std::int64_t>(city.build_goods[k].spent));
            }
            got["ok"] = ok ? "1" : "0";
            got["store"] = join(s_after);
            got["spent"] = join(p_after);
        } else if (kind == "upcheck") {
            VGroup cur = groups.at(in_kv.at("cur"));
            VGroup next = groups.at(in_kv.at("next"));
            next.group.nahrung = table(in_kv.at("nah"));
            next.group.bedarf = table(in_kv.at("bed"));
            next.group.infra = table(in_kv.at("inf"));
            cur.group.no_upgrade = in_kv.count("noup") && in_kv.at("noup") == "1";
            const auto stock_i = ints(in_kv.at("stock"));
            std::vector<std::uint32_t> stock(stock_i.begin(), stock_i.end());
            const std::int64_t tavern = in_kv.count("tavern") ? std::stoll(in_kv.at("tavern")) : -1;
            const sim::UpgradeScores s =
                sim::upgrade_scores(view_of(cur, goods, tavern), view_of(next, goods, tavern),
                                    static_cast<std::uint32_t>(std::stoul(in_kv.at("e"))), stock);
            if (s.fault != sim::HouseFault::None) {
                got["fault"] = sim::house_fault_name(s.fault);
            } else {
                got["result"] = s.eligible ? "1" : "0";
                if (!cur.group.no_upgrade) {
                    got["f"] = std::to_string(s.f);
                    got["b"] = std::to_string(s.b);
                    got["i"] = std::to_string(s.i);
                }
            }
        } else if (kind == "setgroup") {
            const VGroup next = groups.at(in_kv.at("new"));
            const bool first = in_kv.at("old") == "none";
            const auto stock_i = ints(in_kv.at("stock"));
            std::vector<std::uint32_t> stock(stock_i.begin(), stock_i.end());
            Scripted draw;
            for (const auto v : ints(in_kv.at("draws"))) {
                draw.values.push_back(static_cast<std::uint16_t>(v));
            }
            sim::PopulationView nv = view_of(next, goods, -1);
            sim::PopulationView ov;
            if (!first) {
                ov = view_of(groups.at(in_kv.at("old")), goods, -1);
            }
            const sim::SetGroupResult r = sim::house_set_group(
                draw, goods, first ? nullptr : &ov, nv,
                static_cast<std::uint32_t>(std::stoul(in_kv.at("e"))),
                static_cast<std::int32_t>(std::stol(in_kv.at("res64"))), stock);
            if (r.fault != sim::HouseFault::None) {
                // a refused call: the fault name and the number of draws really made (G3: none)
                got["fault"] = sim::house_fault_name(r.fault);
                got["used"] = std::to_string(draw.sites.size());
                for (const auto& [k, v] : want) {
                    CHECK_MESSAGE(got[k] == v, where << " " << k << " expected " << v << " got " << got[k]);
                }
                ++executed;
                continue;
            }
            std::vector<std::int64_t> out(r.stock.begin(), r.stock.end());
            got["stock"] = join(out);
            got["e"] = std::to_string(r.e);
            got["res64"] = std::to_string(r.residents_64);
            got["used"] = std::to_string(r.draws);
            CHECK(draw.sites.size() == r.draws);
            for (const sim::DrawSite s : draw.sites) {
                CHECK(s == sim::DrawSite::HouseFirstAssignment);
            }
        } else if (kind == "adjacent") {
            got["ok"] =
                sim::house_group_adjacent(static_cast<std::uint32_t>(std::stoul(in_kv.at("old"))),
                                          static_cast<std::uint32_t>(std::stoul(in_kv.at("new"))),
                                          in_kv.at("dir") == "up")
                    ? "1"
                    : "0";
        } else {
            FAIL("unknown vector kind " << kind);
        }
        for (const auto& [k, v] : want) {
            CHECK_MESSAGE(got[k] == v, where << " " << k << " expected " << v << " got " << got[k]);
        }
        ++executed;
    }
    CHECK(executed >= 50);
}

// ------------------------------------------------------------------ world tests

namespace {

constexpr sim::TypeId kP1{9501}; // group 0, variants 9501, 9503 (count 2, step 2), walker max 2
constexpr sim::TypeId kP3{9503};
constexpr sim::TypeId kS0{9520}; // group 1, variants 9520..9522 (count 3, step 1), walker max 1
constexpr sim::TypeId kS1{9521};
constexpr sim::TypeId kS2{9522};
constexpr sim::TypeId kStallT{9530};
constexpr sim::GoodsId kFood = 26, kOther = 27;
constexpr std::uint32_t kTools = 0x29, kWood = 0x2a;

struct Mode {
    bool footprint_mismatch = false; // 9520 is 3 x 3
    bool no_variant_9522 = false;
    bool no_upgrade_group0 = false;
    bool group1_food_only = false; // group 1 needs only goods 26 (a missing-slot probe is not this)
    bool group1_needs_28 = false;  // group 1 needs goods 28 the house group does not have
    bool s_in_group0 = false;      // the 9520 family is PRODTYPE 97 (not the adjacent group)
};

sim::BuildingType house_t(sim::TypeId id, std::uint32_t prodtype, std::uint32_t w,
                          std::uint32_t walkers, std::uint8_t head11) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = sim::RuntimeClass::House;
    t.width = t.height = w;
    t.action_radius = 8;
    t.head_raw.assign(56, 0);
    t.head_raw[11] = head11;
    t.entrances = {{3, 3}};
    t.has_prod = true;
    t.prod.kind = sim::BuildingClass::Other;
    t.prod.max_figures = {walkers, 0, 0};
    t.prod.figure_type_count = {3, 0, 0};
    t.prod.figure_ids = {203, 204, 205, 0, 0, 0, 0, 0, 0};
    return t;
}

std::shared_ptr<const sim::TypeRegistry> registry(Mode m = {}) {
    sim::HouseGroup g0;
    g0.number = 0;
    g0.wohnmax = 8;
    g0.no_upgrade = m.no_upgrade_group0;
    g0.needs = {{kFood, true, 41}, {kOther, true, 16}};
    g0.vermehr = {-16, -9, 0, 12, 32};
    sim::HouseGroup g1;
    g1.number = 1;
    g1.wohnmax = 15;
    g1.needs = {{kFood, true, 60}, {kOther, true, 40}};
    if (m.group1_needs_28) {
        g1.needs.push_back({28, true, 20});
    }
    g1.vermehr = g0.vermehr;
    for (std::size_t i = 0; i < 21; ++i) {
        g1.nahrung[i] = i < 10 ? 0 : 5;
        g1.bedarf[i] = 5;
        g1.infra[i] = 5;
    }
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.house_groups({g0, g1});
    b.add(house_t(kP1, 97, 2, 2, static_cast<std::uint8_t>(2 | (2 << 6))));
    b.add(house_t(kP3, 97, 2, 2, 0));
    sim::BuildingType s0 = house_t(kS0, m.s_in_group0 ? 97 : 98, m.footprint_mismatch ? 3 : 2, 1,
                                   static_cast<std::uint8_t>(3 | (1 << 6)));
    s0.cost_goods = {{kTools, 512}, {kWood, 1024}};
    b.add(s0);
    b.add(house_t(kS1, m.s_in_group0 ? 97 : 98, 2, 1, 0));
    if (!m.no_variant_9522) {
        b.add(house_t(kS2, m.s_in_group0 ? 97 : 98, 2, 1, 0));
    }
    sim::BuildingType stall;
    stall.id = kStallT;
    stall.prodtype = 161;
    stall.cls = sim::RuntimeClass::MarketStall;
    stall.entrances = {{3, 3}};
    stall.output_goods = {kFood};
    b.add(stall);
    b.figure_type(sim::FigureType{203, 40265, 3 * 256, 0, 1000});
    b.figure_type(sim::FigureType{204, 40265, 3 * 256, 0, 1000});
    b.figure_type(sim::FigureType{205, 40265, 3 * 256, 0, 1000});
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

sim::House make_house(sim::TypeId type, std::int32_t residents_64, std::uint32_t e,
                      std::vector<std::uint32_t> stock) {
    sim::House h;
    h.residents_64 = residents_64;
    h.e = e;
    h.stock = std::move(stock);
    h.walkers.assign(type == kP1 || type == kP3 ? 2 : 1, sim::FigureId{});
    return h;
}

struct Rig {
    sim::ScenarioSpec spec;
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> p = std::nullopt) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(p);
        spec.islands[0].cities[0].buildings.push_back(std::move(b));
    }
};

Rig make_rig() {
    Rig r;
    r.spec = syn::empty_scenario(77);
    sim::ScenarioSpec::PlayerSpec p;
    p.id = sim::PlayerId{0};
    r.spec.players.push_back(p);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 100, 100, 32, 32);
    is.owner_runs = {{0, 32U * 32U}};
    is.cities.push_back(syn::plain_city(0, 0));
    r.spec.islands.push_back(std::move(is));
    return r;
}

sim::World create(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types) {
    auto w = sim::World::create(r.spec, types, syn::population_config());
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

sim::Island& island0(sim::World& w) {
    return *sim::WorldTestAccess::islands(w)[0];
}
sim::City& city0(sim::World& w) {
    return *island0(w).cities[0];
}
const sim::Building* at(sim::World& w, std::int32_t x, std::int32_t y) {
    return island0(w).building_at(w.config().layout, sim::CityNo{0}, x, y);
}
sim::BuildingKey key_at(sim::World& w, std::int32_t x, std::int32_t y) {
    return island0(w).cache.by_id.at(at(w, x, y)->id);
}
sim::House& house_at(sim::World& w, std::int32_t x, std::int32_t y) {
    sim::Island& is = island0(w);
    const sim::BuildingKey k = key_at(w, x, y);
    return std::get<sim::House>(is.cities[0]->index.lists[k.list].slots[k.slot].value().payload);
}

/// Scripted draws that can look at the world at the moment of each draw.
struct Spy final : sim::DrawSource {
    std::vector<std::uint16_t> values;
    std::vector<sim::DrawSite> sites;
    std::function<void()> on_draw;
    std::uint16_t next(sim::DrawSite site) override {
        sites.push_back(site);
        if (on_draw) {
            on_draw();
        }
        REQUIRE(sites.size() <= values.size());
        return values[sites.size() - 1];
    }
};

sim::HouseEnv env_of(sim::World& w, sim::DrawSource& d, std::uint32_t t = 1000) {
    return sim::HouseEnv{w.config(), w.rules(),
                         w.types(),  sim::WorldTestAccess::players(w),
                         d,          sim::WorldTestAccess::figures(w),
                         t};
}

constexpr std::uint32_t kOrigin = 110;

/// A pioneer house at (110, 110), E 640, stock {2000, 3200}, 8 residents, materials in the store.
Rig upgrade_rig(std::uint32_t stock0 = 2000, sim::TypeId type = kP1) {
    Rig r = make_rig();
    r.place(type, kOrigin, kOrigin, make_house(type, 8 * 64, 640, {stock0, 3200}));
    return r;
}

void stock_materials(sim::World& w, std::uint32_t tools = 1000, std::uint32_t wood = 2000) {
    city0(w).stock.stock[kTools] = tools;
    city0(w).stock.stock[kWood] = wood;
}

std::uint32_t object_at(sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = island0(w);
    return is.tiles.object(is.tiles.index_of(x, y));
}

sim::HouseKernelResult evaluated(sim::DeferredEffect effect, sim::HouseArm arm, std::uint32_t e,
                                 std::vector<std::uint32_t> stock, std::int32_t residents_64) {
    sim::HouseKernelResult k;
    k.status = sim::HouseKernelStatus::Evaluated;
    k.e = e;
    k.stock = std::move(stock);
    k.residents_64 = residents_64;
    k.last = 2500;
    k.arm = arm;
    k.effect = effect;
    return k;
}

} // namespace

TEST_CASE("house transition: upgrade 9501 -> variant of 9520, every number hand-derived") {
    const auto types = registry();
    sim::World w = create(upgrade_rig(), types);
    stock_materials(w);
    REQUIRE(w.check_invariants() == "");
    CHECK(object_at(w, 110, 110) == kP1.v);
    CHECK(city0(w).index.house_counts == std::vector<std::uint32_t>{1, 0});
    CHECK(sim::residents_at_or_above(w.config(), *types, city0(w), 0) == 8);
    CHECK(sim::residents_at_or_above(w.config(), *types, city0(w), 1) == 0);

    // The order: at the moment of the variant draw the materials are spent but the type, E and the
    // stock are still the old ones (variant BEFORE SetGroup).
    Spy spy;
    spy.values = {7}; // 7 mod 3 = 1 -> 9521
    bool checked = false;
    spy.on_draw = [&] {
        checked = true;
        CHECK(at(w, 110, 110)->type == kP1);
        CHECK(house_at(w, 110, 110).stock == std::vector<std::uint32_t>{2000, 3200});
        CHECK(city0(w).stock.stock[kTools] == 1000 - 512);
        CHECK(city0(w).build_goods[0].spent == 512);
    };
    const sim::HouseOutcome o = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
    CHECK(checked);
    REQUIRE(o.kind == sim::HouseOutcomeKind::Upgraded);
    CHECK(o.from == kP1);
    CHECK(o.to == kS1);
    CHECK(o.variant_drawn);
    CHECK(o.variant_draw == 7);
    CHECK(o.variant_index == 1);
    CHECK(spy.sites == std::vector<sim::DrawSite>{sim::DrawSite::HouseVariant});
    CHECK(o.old_group == 0);
    CHECK(o.new_group == 1);
    CHECK(o.counter_sub == 8);
    CHECK(o.counter_add == 8);
    CHECK(o.first_assignment_draws == 0);
    // The house: type, stock carry, residents and E kept, walker slots of the new type, tiles.
    CHECK(at(w, 110, 110)->type == kS1);
    const sim::House& h = house_at(w, 110, 110);
    CHECK(h.stock == std::vector<std::uint32_t>{2925, 2666});
    CHECK(h.e == 640);
    CHECK(h.residents_64 == 512);
    CHECK(h.walkers.size() == 1);
    CHECK(city0(w).index.house_counts == std::vector<std::uint32_t>{0, 1});
    CHECK(sim::residents_at_or_above(w.config(), *types, city0(w), 1) == 8);
    CHECK(object_at(w, 110, 110) == kS1.v);
    CHECK(object_at(w, 111, 111) == kS1.v);
    // The materials: spent += amount, store -= amount.
    CHECK(city0(w).stock.stock[kTools] == 488);
    CHECK(city0(w).stock.stock[kWood] == 2000 - 1024);
    CHECK(city0(w).build_goods[0].spent == 512);
    CHECK(city0(w).build_goods[1].spent == 1024);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("house transition: the materials gate, one unit short or reserved changes nothing") {
    const auto types = registry();
    sim::World w = create(upgrade_rig(), types);
    SUBCASE("short by one unit of the second good") {
        stock_materials(w, 1000, 1023);
        const std::uint64_t before = w.state_hash();
        Spy spy;
        const auto o = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        CHECK(o.kind == sim::HouseOutcomeKind::UpgradeRefusedMaterials);
        CHECK(spy.sites.empty());        // no draw
        CHECK(w.state_hash() == before); // no spend, no counter, no type
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("a reserve (the build-goods setting) blocks, the cumulative spent does not") {
        stock_materials(w, 600, 2000);
        city0(w).build_goods[0].setting = 89; // 600 - 89 = 511 < 512
        const std::uint64_t before = w.state_hash();
        Spy spy;
        CHECK(sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110)).kind ==
              sim::HouseOutcomeKind::UpgradeRefusedMaterials);
        CHECK(w.state_hash() == before);
        city0(w).build_goods[0].setting = 88;   // exactly 512
        city0(w).build_goods[0].spent = 100000; // never read
        spy.values = {0};
        CHECK(sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110)).kind ==
              sim::HouseOutcomeKind::Upgraded);
    }
    SUBCASE("exactly enough passes") {
        stock_materials(w, 512, 1024);
        Spy spy;
        spy.values = {0};
        CHECK(sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110)).kind ==
              sim::HouseOutcomeKind::Upgraded);
        CHECK(city0(w).stock.stock[kTools] == 0);
        CHECK(city0(w).stock.stock[kWood] == 0);
    }
}

TEST_CASE("house transition: two houses cannot spend the same material") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP1, 110, 110, make_house(kP1, 512, 640, {2000, 3200}));
    r.place(kP1, 116, 110, make_house(kP1, 512, 640, {2000, 3200}));
    sim::World w = create(r, types);
    stock_materials(w, 512, 1024); // enough for one
    Spy spy;
    spy.values = {0, 0};
    const auto a = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
    const auto b = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 116, 110));
    CHECK(a.kind == sim::HouseOutcomeKind::Upgraded);
    CHECK(b.kind == sim::HouseOutcomeKind::UpgradeRefusedMaterials);
    CHECK(spy.sites.size() == 1);
    CHECK(city0(w).build_goods[0].spent == 512);
    CHECK(at(w, 116, 110)->type == kP1);
    CHECK(city0(w).index.house_counts == std::vector<std::uint32_t>{1, 1});
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "house transition: apply_house_effects, UpgradeCheck arm with one-predicate near misses") {
    const auto types = registry();
    const auto run = [&](std::uint32_t stock0, std::uint32_t tools, sim::TypeId type) {
        sim::World w = create(upgrade_rig(stock0, type), types);
        stock_materials(w, tools, 2000);
        Spy spy;
        spy.values = {0};
        const auto k = evaluated(sim::DeferredEffect::UpgradeCheck, sim::HouseArm::None, 640,
                                 {stock0, 3200}, 15 * 64 - 1 + 1);
        const auto o = sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k);
        CHECK(w.check_invariants() == "");
        return std::pair{o, spy.sites.size()};
    };
    // food score 76 -> table index 10 -> 5: passes; 75 -> index 9 -> 0: blocked (strictly > 0)
    const auto pass = run(1900, 1000, kP1);
    CHECK(pass.first.kind == sim::HouseOutcomeKind::Upgraded);
    CHECK(pass.first.scores.food == 76);
    CHECK(pass.first.scores.f == 5);
    CHECK(pass.second == 1);
    const auto low = run(1875, 1000, kP1);
    CHECK(low.first.kind == sim::HouseOutcomeKind::NoChange);
    CHECK(low.first.scores.food == 75);
    CHECK(low.first.scores.f == 0);
    CHECK(low.second == 0);
    // everything else equal, one material short: eligible but refused, the scores are reported
    const auto poor = run(1900, 511, kP1);
    CHECK(poor.first.kind == sim::HouseOutcomeKind::UpgradeRefusedMaterials);
    CHECK(poor.second == 0);
    // a missing next type (9521 -> 9540): the end of the ladder, a named outcome (C3), nothing changes
    const auto none = run(1900, 1000, kS1);
    CHECK(none.first.kind == sim::HouseOutcomeKind::NotEligibleTerminal);
    CHECK(none.second == 0);
    // NoUpgrade on the current group: no change
    {
        const auto t2 = registry(Mode{false, false, true});
        sim::World w = create(upgrade_rig(1900), t2);
        stock_materials(w);
        Spy spy;
        const auto k = evaluated(sim::DeferredEffect::UpgradeCheck, sim::HouseArm::None, 640,
                                 {1900, 3200}, 512);
        CHECK(sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k).kind ==
              sim::HouseOutcomeKind::NoChange);
    }
}

TEST_CASE(
    "house transition: the kernel result is committed first, skipped and unevaluated are typed") {
    const auto types = registry();
    sim::World w = create(upgrade_rig(1900), types);
    stock_materials(w);
    Spy spy;
    sim::HouseKernelResult skipped;
    CHECK(sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), skipped).kind ==
          sim::HouseOutcomeKind::NoChange);
    sim::HouseKernelResult bad;
    bad.status = sim::HouseKernelStatus::NotEvaluated;
    bad.reason = sim::NotEvaluatedReason::ZeroDivisor;
    const auto f = sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), bad);
    CHECK(f.kind == sim::HouseOutcomeKind::Fault);
    CHECK(f.fault == sim::HouseFault::NotEvaluated);
    CHECK(f.not_evaluated == sim::NotEvaluatedReason::ZeroDivisor);
    // An evaluated result without an effect only commits its numbers.
    auto k = evaluated(sim::DeferredEffect::None, sim::HouseArm::None, 700, {5, 6}, 600);
    CHECK(sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k).kind ==
          sim::HouseOutcomeKind::NoChange);
    const sim::House& h = house_at(w, 110, 110);
    CHECK(h.e == 700);
    CHECK(h.residents_64 == 600);
    CHECK(h.stock == std::vector<std::uint32_t>{5, 6});
    CHECK(h.last == 2500);
    // A stock of the wrong size is refused before anything is committed.
    k = evaluated(sim::DeferredEffect::None, sim::HouseArm::None, 1, {1, 2, 3}, 1);
    CHECK(sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k).fault ==
          sim::HouseFault::StockSizeMismatch);
    CHECK(house_at(w, 110, 110).e == 700);
    CHECK(spy.sites.empty());
}

TEST_CASE("house transition: downgrade 9521 -> variant of 9501, E reset, numbers hand-derived") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kS1, 110, 110, make_house(kS1, 1280, 2000, {30000, 8000}));
    sim::World w = create(r, types);
    CHECK(city0(w).index.house_counts == std::vector<std::uint32_t>{0, 1});
    Spy spy;
    spy.values = {3}; // 3 mod 2 = 1 -> 9503 (previous base: 9521 -> 9500 missing -> 9501)
    spy.on_draw = [&] { CHECK(at(w, 110, 110)->type == kS1); };
    const auto o = sim::downgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
    REQUIRE(o.kind == sim::HouseOutcomeKind::Downgraded);
    CHECK(o.to == kP3);
    CHECK(o.variant_index == 1);
    CHECK(spy.sites == std::vector<sim::DrawSite>{sim::DrawSite::HouseVariant});
    CHECK(o.counter_sub == 20);
    CHECK(o.counter_add == 20);
    const sim::House& h = house_at(w, 110, 110);
    CHECK(h.e == 2560);
    CHECK(h.residents_64 == 1280);
    CHECK(h.stock == std::vector<std::uint32_t>{10933, 4080});
    CHECK(h.walkers.size() == 2);
    CHECK(city0(w).index.house_counts == std::vector<std::uint32_t>{1, 0});
    CHECK(object_at(w, 111, 110) == kP3.v);
    CHECK(city0(w).build_goods[0].spent == 0); // no materials
    CHECK(w.check_invariants() == "");
    // No previous type: nothing happens, nothing is drawn.
    const std::uint64_t before = w.state_hash();
    Spy none;
    CHECK(sim::downgrade_house(env_of(w, none), island0(w), key_at(w, 110, 110)).kind ==
          sim::HouseOutcomeKind::NoChange);
    CHECK(w.state_hash() == before);
}

TEST_CASE("house transition: faults happen before anything changes") {
    SUBCASE("a different footprint") {
        const auto types = registry(Mode{true});
        sim::World w = create(upgrade_rig(1900), types);
        stock_materials(w);
        const std::uint64_t before = w.state_hash();
        Spy spy;
        const auto o = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        CHECK(o.kind == sim::HouseOutcomeKind::Fault);
        CHECK(o.fault == sim::HouseFault::FootprintMismatch);
        CHECK(spy.sites.empty());
        CHECK(w.state_hash() == before);
    }
    SUBCASE("a missing variant type") {
        const auto types = registry(Mode{false, true});
        sim::World w = create(upgrade_rig(1900), types);
        stock_materials(w);
        const std::uint64_t before = w.state_hash();
        Spy spy;
        const auto o = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        CHECK(o.fault == sim::HouseFault::MissingVariant);
        CHECK(w.state_hash() == before);
    }
    SUBCASE("E == 0: a zero divisor, nothing spent or drawn") {
        const auto types = registry();
        Rig r = make_rig();
        r.place(kP1, 110, 110, make_house(kP1, 512, 0, {2000, 3200}));
        sim::World w = create(r, types);
        stock_materials(w);
        const std::uint64_t before = w.state_hash();
        Spy spy;
        const auto o = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        CHECK(o.fault == sim::HouseFault::ZeroDivisor);
        CHECK(spy.sites.empty());
        CHECK(w.state_hash() == before);
    }
    SUBCASE("a forced need of the next group the current group lacks is a typed fault") {
        const auto types = registry(Mode{false, false, false, false, true});
        sim::World w = create(upgrade_rig(1900), types);
        stock_materials(w);
        Spy spy;
        const auto k = evaluated(sim::DeferredEffect::UpgradeCheck, sim::HouseArm::None, 640,
                                 {1900, 3200}, 512);
        // group 1 has the extra need 28, but its goods_end covers it: the lookup of 28 in group 0
        // is empty
        const auto o = sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k);
        CHECK(o.kind == sim::HouseOutcomeKind::Fault);
        CHECK(o.fault == sim::HouseFault::MissingSlot);
        CHECK(spy.sites.empty());
    }
    SUBCASE("not a house") {
        const auto types = registry();
        Rig r = make_rig();
        r.place(kStallT, 110, 110);
        sim::World w = create(r, types);
        Spy spy;
        CHECK(sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110)).fault ==
              sim::HouseFault::NoSuchHouse);
    }
}

TEST_CASE("house transition: removal through the production path, counters derived, F-3 pinned") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kS1, 110, 110, make_house(kS1, 1280, 2000, {30000, 8000}));
    r.place(kP1, 116, 110, make_house(kP1, 8 * 64, 640, {1, 2}));
    sim::World w = create(r, types);
    CHECK(sim::residents_at_or_above(w.config(), *types, city0(w), 0) == 28);
    // Remove arm: the kernel result carries the final residents (no correction on this arm).
    Spy spy;
    const auto k =
        evaluated(sim::DeferredEffect::Remove, sim::HouseArm::Remove, 640, {30000, 8000}, 300);
    const auto o = sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k);
    CHECK(o.kind == sim::HouseOutcomeKind::Removed);
    CHECK(o.counter_sub == 4); // trunc(300 / 64): no below-64 correction on the remove arm
    CHECK(at(w, 110, 110) == nullptr);
    CHECK(object_at(w, 110, 110) == 0);
    CHECK(city0(w).index.house_counts == std::vector<std::uint32_t>{1, 0});
    CHECK(sim::residents_at_or_above(w.config(), *types, city0(w), 0) == 8);
    CHECK(spy.sites.empty());
    CHECK(w.check_invariants() == "");
}

TEST_CASE("house transition: the below-64 correction exists only on the downgrade arm (F-3)") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kS1, 110, 110, make_house(kS1, 700, 2000, {30000, 8000}));
    sim::World w = create(r, types);
    // The kernel's final residents are 64 (corrected from 40) and below64_add keeps the ORIGINAL's
    // value trunc(40/64) - 64 = -64; the derived counter reads trunc(64/64) = 1 for the house.
    sim::HouseKernelResult k = evaluated(sim::DeferredEffect::Downgrade, sim::HouseArm::Downgrade,
                                         2000, {30000, 8000}, 64);
    k.below64_add = -64;
    Spy spy;
    spy.values = {0};
    const auto o = sim::apply_house_effects(env_of(w, spy), island0(w), key_at(w, 110, 110), k);
    REQUIRE(o.kind == sim::HouseOutcomeKind::Downgraded);
    CHECK(o.counter_sub == 1); // the derived value, not the original's incremental one
    CHECK(o.counter_add == 1);
    CHECK(house_at(w, 110, 110).residents_64 == 64);
    CHECK(house_at(w, 110, 110).e == 128); // (64/64) << 7
    CHECK(*k.below64_add == -64);
    CHECK(sim::residents_at_or_above(w.config(), *types, city0(w), 0) == 1);
}

namespace {

// -- a walker in flight (the L2 harness in small)

struct Sender {
    sim::BuildingId house;
    int armed = 0;
};

void send_pass(void* raw, const sim::TickContext& ctx, sim::Island& island, sim::City& city,
               sim::Effects& fx) {
    auto& s = *static_cast<Sender*>(raw);
    if (s.armed <= 0) {
        return;
    }
    const sim::Building* b = island.find_building(s.house);
    const sim::BuildingType* t = b != nullptr ? ctx.types.find(b->type) : nullptr;
    if (b == nullptr || t == nullptr) {
        return;
    }
    const auto* h = std::get_if<sim::House>(&b->payload);
    const sim::ResidentDispatch d = sim::resident_dispatch(
        ctx.draw, island, city, *b, *t, ctx.types, 50, sim::resident_walkers_used(*h));
    const bool both = s.armed >= 2;
    --s.armed;
    if (d.spawn) {
        fx.spawns.push_back(*d.spawn);
        if (both) { // a second walker in slot 1 in the same pass
            sim::SpawnRequest second = *d.spawn;
            second.home_counter = 1;
            fx.spawns.push_back(second);
            s.armed = 0;
        }
    }
}

std::vector<const sim::Figure*> walkers_of(const sim::World& w) {
    std::vector<const sim::Figure*> out;
    for (const auto& s : w.figures().list.slots) {
        if (s && s->job == sim::kJobResident) {
            out.push_back(&*s);
        }
    }
    return out;
}

} // namespace

TEST_CASE("house transition: removal with an active walker carrying cargo ends it once, no leak") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP1, 110, 110, make_house(kP1, 8 * 64, 640, {0, 3200}));
    r.place(kStallT, 114, 110);
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    sender.house = at(w, 110, 110)->id;
    sender.armed = 1;
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.city_pass[0].name = "house_transition_test_sender";
    seams.city_pass[0].fn = &send_pass;
    seams.city_pass[0].ctx = &sender;
    for (int i = 0; i < 40000 && walkers_of(w).empty(); ++i) {
        w.advance(2);
    }
    seams.city_pass[0] = sim::Seam<sim::Seams::CityPass>{};
    REQUIRE(walkers_of(w).size() == 1);
    const sim::FigureId id = walkers_of(w)[0]->id;
    // Walk until the cargo is bought (the walker is on its way home with food).
    for (int i = 0; i < 40000; ++i) {
        const sim::Figure* f = sim::find_figure(w.figures(), id);
        if (f != nullptr && f->resident && f->resident->cargo[0].goods != 0) {
            break;
        }
        w.advance(2);
    }
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    REQUIRE(f->resident->cargo[0].goods == kFood);
    REQUIRE(f->resident->step == sim::ResidentStep::ToHome);
    CHECK(house_at(w, 110, 110).walkers[0] == id);
    const auto stock_after = city0(w).stock.stock[kFood];

    Spy spy;
    const auto o = sim::remove_house(env_of(w, spy, 5000), island0(w), key_at(w, 110, 110));
    REQUIRE(o.kind == sim::HouseOutcomeKind::Removed);
    CHECK(o.walkers_detached == 1);
    CHECK(at(w, 110, 110) == nullptr);
    f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    // The destructor detaches (0x10075bc0): the walker is NOT ended; it lives on without a home.
    CHECK_FALSE(f->expiry_stamp.has_value());
    CHECK_FALSE(f->home.valid());
    REQUIRE(f->resident.has_value());
    CHECK(f->resident->step == sim::ResidentStep::ToHome);
    CHECK(f->resident->cargo[0].goods == kFood);
    CHECK(std::count(w.figures().expiry.begin(), w.figures().expiry.end(), id) == 0);
    CHECK(city0(w).stock.stock[kFood] == stock_after);
    CHECK(w.check_invariants() == "");
    // The world keeps running; the homeless walker finishes its walk, delivers nothing (its cargo
    // is lost), ends, and the expiry list destroys it.
    for (int i = 0; i < 40000 && !sim::find_figure(w.figures(), id)->expiry_stamp.has_value() &&
                    sim::find_figure(w.figures(), id) != nullptr;
         ++i) {
        w.advance(2);
        REQUIRE_MESSAGE(w.check_invariants() == "", w.check_invariants());
    }
    CHECK(city0(w).stock.stock[kFood] == stock_after);
    for (int i = 0; i < 30000 && sim::find_figure(w.figures(), id) != nullptr; ++i) {
        w.advance(2);
    }
    CHECK(sim::find_figure(w.figures(), id) == nullptr);
    CHECK(walkers_of(w).empty());
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "house transition: a smaller walker maximum keeps occupied slots and trims only empty ones") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP1, 110, 110, make_house(kP1, 8 * 64, 640, {0, 3200}));
    r.place(kStallT, 114, 110);
    sim::World w = create(r, types);
    stock_materials(w);
    city0(w).stock.stock[kFood] = 5000;
    Sender sender;
    sender.house = at(w, 110, 110)->id;
    sender.armed = 2;
    sim::Seams& seams = sim::WorldTestAccess::seams(w);
    seams.city_pass[0].name = "house_transition_test_sender";
    seams.city_pass[0].fn = &send_pass;
    seams.city_pass[0].ctx = &sender;
    for (int i = 0; i < 40000 && walkers_of(w).size() < 2; ++i) {
        w.advance(2);
        if (sender.armed <= 0) {
            break;
        }
    }
    seams.city_pass[0] = sim::Seam<sim::Seams::CityPass>{};
    REQUIRE(walkers_of(w).size() == 2); // type 9501 allows two
    const sim::FigureId second = house_at(w, 110, 110).walkers[1];
    REQUIRE(second.valid());
    Spy spy;
    spy.values = {0};
    const auto o = sim::upgrade_house(env_of(w, spy, 777), island0(w), key_at(w, 110, 110));
    REQUIRE(o.kind == sim::HouseOutcomeKind::Upgraded); // 9520 allows one walker
    // The original keeps its slots; the type maximum only gates the spawn (lazy trimming).
    CHECK(house_at(w, 110, 110).walkers.size() == 2);
    CHECK(house_at(w, 110, 110).walkers[1] == second);
    CHECK_FALSE(sim::find_figure(w.figures(), second)->expiry_stamp.has_value());
    CHECK(sim::find_figure(w.figures(), second)->home.valid());
    CHECK(w.check_invariants() == "");
}

TEST_CASE("house transition: save, reload and the same operations give identical hashes") {
    const auto types = registry();
    auto fresh = [&] {
        sim::World w = create(upgrade_rig(), types);
        stock_materials(w);
        return w;
    };
    auto run_ops = [&](sim::World& w) {
        Spy spy;
        spy.values = {7, 3};
        const auto up = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        REQUIRE(up.kind == sim::HouseOutcomeKind::Upgraded);
        const auto down = sim::downgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        REQUIRE(down.kind == sim::HouseOutcomeKind::Downgraded);
        CHECK(w.check_invariants() == "");
    };
    sim::World a = fresh(), b = fresh();
    CHECK(a.state_hash() == b.state_hash());
    run_ops(a);
    run_ops(b);
    CHECK(a.state_hash() == b.state_hash());
    CHECK(a.state_hash() != fresh().state_hash());
    // save/reload after the first operation, then continue: the same final hash
    sim::World c = fresh();
    {
        Spy spy;
        spy.values = {7};
        REQUIRE(sim::upgrade_house(env_of(c, spy), island0(c), key_at(c, 110, 110)).kind ==
                sim::HouseOutcomeKind::Upgraded);
    }
    const std::vector<std::uint8_t> bytes = c.save();
    auto back = sim::World::load(bytes, types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    sim::World d = std::move(back).value();
    CHECK(d.state_hash() == c.state_hash());
    CHECK(d.check_invariants() == "");
    Spy spy;
    spy.values = {3};
    REQUIRE(sim::downgrade_house(env_of(d, spy), island0(d), key_at(d, 110, 110)).kind ==
            sim::HouseOutcomeKind::Downgraded);
    CHECK(d.state_hash() == a.state_hash());
    CHECK(d.save() == a.save());
}

TEST_CASE("house transition: the new group must be the adjacent record (SetGroup cur +/- 1)") {
    // 0x1002cd0e..0x1002cd77 (upgrade: cur + 0x118), 0x1002ceb1..0x1002cf06 (downgrade: cur -
    // 0x118).
    Mode m;
    m.s_in_group0 = true; // the next type of 9501 is in the SAME group
    const auto types = registry(m);
    {
        sim::World w = create(upgrade_rig(), types);
        stock_materials(w);
        const std::uint64_t before = w.state_hash();
        Spy spy;
        spy.values = {0};
        const auto o = sim::upgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        CHECK(o.kind == sim::HouseOutcomeKind::Fault);
        CHECK(o.fault == sim::HouseFault::GroupNotAdjacent);
        CHECK(spy.sites.empty());        // nothing drawn
        CHECK(w.state_hash() == before); // nothing spent, nothing changed
    }
    {
        // a downgrade from group 0 has no previous record
        sim::World w = create(upgrade_rig(2000, kS0), types);
        const std::uint64_t before = w.state_hash();
        Spy spy;
        spy.values = {0};
        const auto o = sim::downgrade_house(env_of(w, spy), island0(w), key_at(w, 110, 110));
        CHECK(o.kind == sim::HouseOutcomeKind::Fault);
        CHECK(o.fault == sim::HouseFault::GroupNotAdjacent);
        CHECK(w.state_hash() == before);
    }
}

// ------------------------------------------------------------------ MAP-POP-CLOSE-1 C1 (G3)

namespace {

/// A group (number 2, one forced need on goods 26) with its derived table from the real derivation.
struct DomainGroup {
    sim::HouseGroup group;
    sim::HouseGroupDerived derived;
};

DomainGroup domain_group(std::uint16_t wohn, std::uint16_t menge, std::uint32_t number = 2) {
    DomainGroup d;
    d.group.number = number;
    d.group.wohnmax = wohn;
    d.group.needs = {{26, true, menge}};
    const std::vector<sim::GoodsInfo> goods = sim::original_goods();
    const std::string problem = sim::derive_group(d.group, goods, sim::PopulationRoles{}, d.derived);
    REQUIRE_MESSAGE(problem.empty(), problem);
    return d;
}

} // namespace

TEST_CASE("house set group G3: a refused first assignment draws nothing and changes nothing") {
    const std::vector<sim::GoodsInfo> goods = sim::original_goods();
    const auto run = [&](const DomainGroup& d, Scripted& draw) {
        sim::PopulationView v;
        v.group = &d.group;
        v.derived = &d.derived;
        v.goods = goods;
        const std::vector<std::uint32_t> none;
        return sim::house_set_group(draw, goods, nullptr, v, 640, 320, none);
    };
    // Refused: Wohnmax 65535 with Menge 65535 (stock far beyond u32), and the edge Menge 6145
    // (6145 * 699040 = 4295600800 > 4294967295). No draw, the inputs echo back, no stock.
    for (const std::uint16_t menge : {std::uint16_t{65535}, std::uint16_t{6145}}) {
        const DomainGroup d = domain_group(65535, menge);
        Scripted draw;
        draw.values = {0};
        const sim::SetGroupResult r = run(d, draw);
        CHECK(r.fault == sim::HouseFault::Overflow);
        CHECK(draw.sites.empty()); // the cursor of a real source would not move
        CHECK(r.draws == 0);
        CHECK(r.stock.empty());
        CHECK(r.e == 640);
        CHECK(r.residents_64 == 320);
    }
    // Accepted controls: the edge Menge 6144 (4294901760, draw 0 -> sat 128) and a neighbour
    // (Wohnmax 100, Menge 255, draw 7 -> sat 121: 257125); each draws exactly once, in order.
    {
        const DomainGroup d = domain_group(65535, 6144);
        Scripted draw;
        draw.values = {0};
        const sim::SetGroupResult r = run(d, draw);
        CHECK(r.fault == sim::HouseFault::None);
        CHECK(draw.sites.size() == 1);
        CHECK(r.stock == std::vector<std::uint32_t>{4294901760U});
        CHECK(r.e == 8388480);
        CHECK(r.residents_64 == 4194240);
    }
    {
        const DomainGroup d = domain_group(100, 255);
        Scripted draw;
        draw.values = {7};
        const sim::SetGroupResult r = run(d, draw);
        CHECK(r.fault == sim::HouseFault::None);
        CHECK(draw.sites.size() == 1);
        CHECK(r.stock == std::vector<std::uint32_t>{257125U});
        CHECK(r.e == 12800);
        CHECK(r.residents_64 == 6400);
    }
    // The bound is per need, not only for slot 0: a forced need in slot 1 (6145 refused, 6144
    // accepted) and an unforced food slot 0 with a huge Menge (E * Menge / 12 > u32) while another
    // need is forced (the draw must not happen).
    {
        const auto two = [&](std::uint16_t m0, bool f0, std::uint16_t m1) {
            DomainGroup d;
            d.group.number = 2;
            d.group.wohnmax = 65535;
            d.group.needs = {{26, f0, m0}, {27, true, m1}};
            const std::string problem =
                sim::derive_group(d.group, goods, sim::PopulationRoles{}, d.derived);
            REQUIRE_MESSAGE(problem.empty(), problem);
            return d;
        };
        {
            const DomainGroup d = two(1, false, 6145);
            Scripted draw;
            draw.values = {0, 0};
            const sim::SetGroupResult r = run(d, draw);
            CHECK(r.fault == sim::HouseFault::Overflow);
            CHECK(draw.sites.empty());
        }
        {
            const DomainGroup d = two(1, false, 6144);
            Scripted draw;
            draw.values = {0};
            const sim::SetGroupResult r = run(d, draw);
            CHECK(r.fault == sim::HouseFault::None);
            CHECK(draw.sites.size() == 1);
            CHECK(r.stock == std::vector<std::uint32_t>{699040U, 4294901760U});
        }
        {
            const DomainGroup d = two(65535, false, 1);
            Scripted draw;
            draw.values = {0};
            const sim::SetGroupResult r = run(d, draw);
            CHECK(r.fault == sim::HouseFault::Overflow);
            CHECK(draw.sites.empty());
        }
    }
    // A repeated ware (derive_group lets the last slot win): the forced draw of slot 0 satisfies every
    // slot of ware 26, so the UNFORCED slot 1 is bounded at sat = 128 too (review P2).
    {
        const auto rep = [&](std::uint16_t m1) {
            DomainGroup d;
            d.group.number = 2;
            d.group.wohnmax = 65535;
            d.group.needs = {{26, true, 1}, {26, false, m1}};
            const std::string problem =
                sim::derive_group(d.group, goods, sim::PopulationRoles{}, d.derived);
            REQUIRE_MESSAGE(problem.empty(), problem);
            return d;
        };
        {
            const DomainGroup d = rep(65535);
            Scripted draw;
            draw.values = {0};
            const sim::SetGroupResult r = run(d, draw);
            CHECK(r.fault == sim::HouseFault::Overflow);
            CHECK(draw.sites.empty());
            CHECK(r.draws == 0);
        }
        {
            const DomainGroup d = rep(6144);
            Scripted draw;
            draw.values = {0};
            const sim::SetGroupResult r = run(d, draw);
            CHECK(r.fault == sim::HouseFault::None);
            CHECK(draw.sites.size() == 1);
            CHECK(r.stock == std::vector<std::uint32_t>{699040U, 4294901760U});
        }
    }
    // Group number 0 never draws and keeps the constructor numbers: the huge group is accepted by
    // the first-assignment path (it does not use Wohnmax there).
    {
        const DomainGroup d = domain_group(65535, 65535, 0);
        Scripted draw;
        const sim::SetGroupResult r = run(d, draw);
        CHECK(draw.sites.empty());
        CHECK(r.e == 640);
        CHECK(r.residents_64 == 320);
    }
}
