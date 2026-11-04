// MAP-POP-CLOSE-2 K1: the Inn (src/sim/src/inn.cpp, driver.cpp, transport.cpp). The pure rules
// against fixtures/synthetic/sim/inn_vectors.txt (hand-derived BEFORE the C++; tools/inn_ref.py
// checks the same file independently), then the Inn end to end on a small synthetic island: the
// tick sends the existing workshop carrier (job 0x33), which loads alcohol from the warehouse and
// unloads it into the tavern stock. Evidence: docs/re/population.md section 18.
//
// Hand derivations of the end-to-end numbers (the Inn at (111,115), the market at (110,110), a
// carrier of capacity 768, speed 0.012):
//  * the Inn's gate opens at tick 5000 (last 0, diff 5000): stock 0 < 0x100 -> a carrier;
//  * priority: 0 steps of need 48 -> 0x80; the market's offer 0x80 + 12800*192/1280 = 2048;
//  * the walk of six orthogonal tile edges is 2501 ticks (the workshop carrier test), the load
//    wait of figure 204 is 500 ticks, the load takes min(768, 12800) = 768, the walk back 2501;
//  * the unload adds 768 to the tavern stock (8.8, no x16).
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/inn.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

// ---- the vector file ------------------------------------------------------------------------

using Kv = std::map<std::string, std::string>;

Kv parse_kv(const std::vector<std::string>& t, std::size_t from, std::size_t to) {
    Kv m;
    for (std::size_t i = from; i < to; ++i) {
        const auto eq = t[i].find('=');
        REQUIRE_MESSAGE(eq != std::string::npos, t[i]);
        m[t[i].substr(0, eq)] = t[i].substr(eq + 1);
    }
    return m;
}

std::int64_t num(const Kv& kv, const char* key) {
    const auto it = kv.find(key);
    REQUIRE_MESSAGE(it != kv.end(), key);
    return std::stoll(it->second);
}

std::string str(std::int64_t v) {
    return std::to_string(v);
}

} // namespace

TEST_CASE("inn vectors: the gate, decay, stops, take, unload, priority and pick agree with the file") {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/inn_vectors.txt");
    REQUIRE(in.good());
    std::string line;
    int executed = 0, bad = 0;
    std::map<std::string, int> kinds;
    while (std::getline(in, line)) {
        line = line.substr(0, line.find('#'));
        std::istringstream is(line);
        std::vector<std::string> t;
        for (std::string w; is >> w;) {
            t.push_back(w);
        }
        if (t.empty()) {
            continue;
        }
        std::size_t arrow = 0;
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (t[i] == "=>") {
                arrow = i;
            }
        }
        REQUIRE(arrow > 1);
        const Kv kv = parse_kv(t, 2, arrow);
        const Kv exp = parse_kv(t, arrow + 1, t.size());
        std::map<std::string, std::string> got;
        if (t[0] == "tick") {
            sim::Inn inn;
            inn.last = static_cast<std::uint16_t>(num(kv, "last"));
            inn.taken = static_cast<std::uint32_t>(num(kv, "taken"));
            inn.tavern = static_cast<std::uint32_t>(num(kv, "stock"));
            inn.suspended = (num(kv, "flags") & 0x30) != 0;
            inn.carriers_out = static_cast<std::uint32_t>(num(kv, "count"));
            const sim::InnTickResult r = sim::inn_step(inn, static_cast<std::uint32_t>(num(kv, "tick")),
                                                       static_cast<std::int32_t>(num(kv, "limit")));
            got = {{"gate", r.stepped ? "open" : "closed"},
                   {"last", str(inn.last)},
                   {"taken", str(inn.taken)},
                   {"send", r.send ? "1" : "0"}};
            CHECK(inn.carriers_out == static_cast<std::uint32_t>(num(kv, "count"))); // never touched
        } else if (t[0] == "take") {
            sim::Inn inn;
            inn.tavern = static_cast<std::uint32_t>(num(kv, "stock"));
            inn.taken = static_cast<std::uint32_t>(num(kv, "taken"));
            const std::int64_t n = sim::inn_take(inn, num(kv, "request"));
            got = {{"returned", str(n)}, {"stock", str(inn.tavern)}, {"taken", str(inn.taken)}};
        } else if (t[0] == "unload") {
            sim::Inn inn;
            inn.tavern = static_cast<std::uint32_t>(num(kv, "stock"));
            sim::inn_unload(inn, static_cast<std::uint32_t>(num(kv, "amount")));
            got = {{"stock", str(inn.tavern)}};
        } else if (t[0] == "priority") {
            const auto p = sim::base_priority(static_cast<std::uint32_t>(num(kv, "stock")),
                                              static_cast<std::uint32_t>(num(kv, "need")));
            got = {{"prio", p ? str(*p) : "refused"}};
        } else if (t[0] == "offer") {
            // The carrier's offer: matches (output 0 ALL or the goods, or output 1 the goods), then
            // the pick score with the Inn's base priority.
            std::vector<std::int64_t> out;
            std::istringstream os(kv.at("out"));
            for (std::string w; std::getline(os, w, ',');) {
                out.push_back(std::stoll(w));
            }
            REQUIRE(out.size() == 2);
            std::int64_t score = -1;
            const auto goods = static_cast<sim::GoodsId>(num(kv, "input"));
            if (sim::pick_matches(goods, static_cast<sim::GoodsId>(out[0]),
                                  static_cast<sim::GoodsId>(out[1]))) {
                const auto p = sim::base_priority(static_cast<std::uint32_t>(num(kv, "stock")),
                                                  static_cast<std::uint32_t>(num(kv, "need")));
                score = sim::pick_score(p.value_or(-1), num(kv, "avail"), num(kv, "cap"));
            }
            got = {{"score", str(score)}};
        } else {
            FAIL("unknown record " << t[0]);
        }
        ++executed;
        ++kinds[t[0]];
        for (const auto& [k, want] : exp) {
            const auto it = got.find(k);
            const std::string have = it != got.end() ? it->second : "<absent>";
            if (have != want) {
                MESSAGE(t[1] << ": " << k << " expected " << want << ", got " << have);
                ++bad;
            }
        }
    }
    CHECK(bad == 0);
    CHECK(executed == 73);
    // every record kind is exercised
    CHECK(kinds.size() == 5);
}

namespace {

constexpr sim::TypeId kMarketT{9701};
constexpr sim::TypeId kInnT{9702};    // PRODTYPE 66, input ALKOHOL (29), need 48, one carrier
constexpr sim::TypeId kInnAlt{9703};  // a non-original Inn type: input goods 47, need 30
constexpr sim::TypeId kInnTwo{9704};  // two carriers at once
constexpr std::uint32_t kCartFigure = 220;
constexpr std::uint32_t kInnCarrier = 204;
constexpr sim::GoodsId kAlcohol = 29;
constexpr sim::GoodsId kAltGoods = 47;

sim::BuildingType inn_type(sim::TypeId id, sim::GoodsId in, std::uint32_t need,
                           std::uint32_t max_figures) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = 66;
    t.cls = sim::RuntimeClass::Inn;
    t.entrances = {{3, 3}};
    t.input_goods = {in};
    t.output_goods = {76};
    t.action_radius = 20;
    t.has_prod = true;
    t.prod.kind = sim::BuildingClass::Other;
    t.prod.input_per_step = {need, 0};
    t.prod.max_figures = {max_figures, 0, 0};
    t.prod.figure_type_count = {1, 0, 0};
    t.prod.figure_ids = {kInnCarrier, 0, 0, 0, 0, 0, 0, 0, 0};
    t.prod.capacity = {5 * 256, 0};
    return t;
}

std::vector<sim::HouseGroup> groups() {
    sim::HouseGroup g;
    g.number = 0;
    g.wohnmax = 8;
    g.no_downgrade = true;
    g.needs = {{26, true, 41}};
    g.vermehr = {-16, -9, 0, 12, 32};
    sim::HouseGroup g1 = g;
    g1.number = 1;
    g1.no_downgrade = false;
    return {g, g1};
}

sim::TypeRegistry::Builder base_builder() {
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.house_groups(groups());
    b.add_market(kMarketT, kCartFigure, 0, 8).entrances(kMarketT, {{3, 3}});
    b.figure_type(sim::FigureType{kCartFigure, 53687, 5 * 256, 3, 500});
    b.figure_type(sim::FigureType{kInnCarrier, 40265, 3 * 256, 0, 500});
    return b;
}

std::shared_ptr<const sim::TypeRegistry> registry() {
    sim::TypeRegistry::Builder b = base_builder();
    b.add(inn_type(kInnT, kAlcohol, 48, 1));
    b.add(inn_type(kInnAlt, kAltGoods, 30, 1));
    b.add(inn_type(kInnTwo, kAlcohol, 48, 2));
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

struct Rig {
    sim::ScenarioSpec spec;
    sim::ScenarioSpec::CitySpec& city() { return spec.islands[0].cities[0]; }
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> payload = std::nullopt) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(payload);
        city().buildings.push_back(std::move(b));
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

sim::World create(const Rig& r, const std::shared_ptr<const sim::TypeRegistry>& types,
                  bool live = true) {
    sim::Rules rules;
    rules.house_updates = live;
    auto w = sim::World::create(r.spec, types, syn::population_config(), rules);
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

const sim::Building& at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Building* b = w.islands()[0]->building_at(w.config().layout, sim::CityNo{0}, x, y);
    REQUIRE(b != nullptr);
    return *b;
}

const sim::Inn& inn_at(const sim::World& w, std::int32_t x = 111, std::int32_t y = 115) {
    const auto* inn = std::get_if<sim::Inn>(&at(w, x, y).payload);
    REQUIRE(inn != nullptr);
    return *inn;
}

const sim::City& city0(const sim::World& w) {
    return *w.islands()[0]->cities[0];
}

std::vector<const sim::Figure*> carriers(const sim::World& w) {
    std::vector<const sim::Figure*> out;
    for (const auto& s : w.figures().list.slots) {
        if (s && s->job == sim::kJobWorkshopCarrier && !s->expiry_stamp.has_value()) {
            out.push_back(&*s);
        }
    }
    return out;
}

template <class F> std::uint64_t until(sim::World& w, F done, std::uint64_t limit = 20000) {
    for (std::uint64_t i = 0; i < limit; ++i) {
        w.advance(2);
        if (done()) {
            return w.clock().tick();
        }
    }
    FAIL("the condition did not hold within the limit");
    return 0;
}

void advance_to(sim::World& w, std::uint64_t tick) {
    while (w.clock().tick() < tick) {
        w.advance(2);
    }
}

std::vector<std::uint32_t> stock_of(sim::GoodsId g, std::uint32_t amount) {
    std::vector<std::uint32_t> s(56, 0);
    s[g] = amount;
    return s;
}

/// One end-to-end run for an Inn type with the given input goods and capacity of the store.
void end_to_end(sim::TypeId type, sim::GoodsId goods) {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarketT, 110, 110);
    r.place(type, 111, 115);
    r.city().stock = stock_of(goods, 12800);
    sim::World w = create(r, types);
    REQUIRE(inn_at(w).carriers_out == 0);
    const std::uint64_t spawn = until(w, [&] { return !carriers(w).empty(); });
    // The gate opens at tick 5000 (diff 5000, stock 0 < 0x100); the building is visited on the island
    // pass schedule, so the figure appears within the same 50-tick step.
    CHECK(spawn >= 5000);
    CHECK(spawn < 5050);
    {
        const sim::Inn& inn = inn_at(w);
        CHECK(inn.last == 5000);
        CHECK(inn.carriers_out == 1);
        const sim::Figure& c = *carriers(w)[0];
        CHECK(c.kind == kInnCarrier);
        CHECK(c.home == at(w, 111, 115).id);
        CHECK(c.transport.target == at(w, 110, 110).id);
        CHECK(c.cargo_goods == goods);
        CHECK(c.cargo_88 == 0);
        REQUIRE(c.transport.route.size() == 7);
    }
    const std::uint64_t load = until(w, [&] { return city0(w).stock.stock[goods] != 12800; });
    CHECK(load - spawn == 2501 + 500);
    CHECK(city0(w).stock.stock[goods] == 12800 - 768);
    CHECK(inn_at(w).tavern == 0);
    const std::uint64_t unload = until(w, [&] { return inn_at(w).tavern != 0; });
    CHECK(unload - load == 2501);
    CHECK(inn_at(w).tavern == 768); // 8.8, no x16
    CHECK(inn_at(w).taken == 0);
    until(w, [&] { return carriers(w).empty(); });
    CHECK(inn_at(w).carriers_out == 0); // released at the end of the figure
    // The gate at 10000: stock 768 >= 0x100 and >= taken / 2 = 0: no second carrier.
    advance_to(w, 10200);
    CHECK(carriers(w).empty());
    CHECK(inn_at(w).last == 10000);
    CHECK(inn_at(w).tavern == 768);
    CHECK(city0(w).stock.stock[goods] == 12800 - 768);
    CHECK(w.check_invariants() == "");
}

} // namespace

TEST_CASE("inn end to end: the tick sends job 0x33, it loads from the warehouse, the Inn gains 768") {
    end_to_end(kInnT, kAlcohol);
}

TEST_CASE("inn end to end with a non-original Inn type id and goods id") {
    end_to_end(kInnAlt, kAltGoods);
}

TEST_CASE("inn limit: no second carrier while one is out, and a limit of two sends two") {
    const auto types = registry();
    {
        Rig r = make_rig();
        r.place(kMarketT, 110, 110);
        r.place(kInnT, 111, 115);
        r.city().stock = stock_of(kAlcohol, 12800);
        sim::World w = create(r, types);
        until(w, [&] { return !carriers(w).empty(); });
        // The gate at 10000 finds the stock still 0 (the carrier is on its way back) but limit 1 ==
        // count 1: no second send; the stamp advanced and nothing else changed.
        advance_to(w, 10002);
        CHECK(carriers(w).size() == 1);
        CHECK(inn_at(w).last == 10000);
        CHECK(inn_at(w).carriers_out == 1);
    }
    {
        // Two carriers at once: limit 2. The first gate sends one (count 0 < 2); the stock stays 0 and
        // the carrier is out for 5502 ticks, so the gate at 10000 sends a second (count 1 < 2).
        Rig r = make_rig();
        r.place(kMarketT, 110, 110);
        r.place(kInnTwo, 111, 115);
        r.city().stock = stock_of(kAlcohol, 12800);
        sim::World w = create(r, types);
        until(w, [&] { return !carriers(w).empty(); });
        advance_to(w, 10002);
        CHECK(carriers(w).size() == 2);
        CHECK(inn_at(w).carriers_out == 2);
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("inn controls: switched off still decays, the rule off is inert, enough stock sends nothing") {
    const auto types = registry();
    {
        sim::Inn off;
        off.suspended = true; // flags bits 4/5
        off.taken = 64;
        Rig r = make_rig();
        r.place(kMarketT, 110, 110);
        r.place(kInnT, 111, 115, off);
        r.city().stock = stock_of(kAlcohol, 12800);
        sim::World w = create(r, types);
        advance_to(w, 5200);
        CHECK(carriers(w).empty());
        CHECK(inn_at(w).last == 5000); // the gate opened ...
        CHECK(inn_at(w).taken == 62);  // ... and the statistic decayed: (31 * 64) / 32
    }
    {
        Rig r = make_rig();
        r.place(kMarketT, 110, 110);
        r.place(kInnT, 111, 115);
        r.city().stock = stock_of(kAlcohol, 12800);
        sim::World w = create(r, types, false); // Rules::house_updates off (the default)
        advance_to(w, 5200);
        CHECK(carriers(w).empty());
        CHECK(inn_at(w).last == 0); // inert: the stamp untouched
    }
    {
        sim::Inn stocked;
        stocked.tavern = 300; // >= 0x100 and taken 0: nothing to fetch
        Rig r = make_rig();
        r.place(kMarketT, 110, 110);
        r.place(kInnT, 111, 115, stocked);
        r.city().stock = stock_of(kAlcohol, 12800);
        sim::World w = create(r, types);
        advance_to(w, 5200);
        CHECK(carriers(w).empty());
        CHECK(inn_at(w).last == 5000);
    }
    {
        sim::Inn busy;
        busy.tavern = 300;
        busy.taken = 2000; // decays to 1937, half 968 > 300: the Inn fetches although it holds 1 t
        Rig r = make_rig();
        r.place(kMarketT, 110, 110);
        r.place(kInnT, 111, 115, busy);
        r.city().stock = stock_of(kAlcohol, 12800);
        sim::World w = create(r, types);
        advance_to(w, 5200);
        REQUIRE(carriers(w).size() == 1);
        CHECK(inn_at(w).taken == 1937);
    }
}

TEST_CASE("inn with no alcohol in the warehouse: the carrier keeps the goods byte and goes home empty") {
    // D 0x100756a9-0x100756c3: the send pre-sets the cargo goods; with no source (the market holds
    // none) the carrier walks nowhere, the cargo amount stays 0 and the Inn gains nothing.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kMarketT, 110, 110);
    r.place(kInnT, 111, 115);
    r.city().stock = stock_of(kAlcohol, 0);
    sim::World w = create(r, types);
    // The carrier is created at the first gate and ends in the same pass (no source, it stands in
    // the Inn's footprint): watch every tick for the figure and its cargo goods byte.
    bool seen = false;
    for (int i = 0; i < 5200 && !seen; ++i) {
        w.advance(2);
        for (const auto& slot : w.figures().list.slots) {
            if (!slot || slot->job != sim::kJobWorkshopCarrier) {
                continue;
            }
            const sim::Figure* f = &*slot; // also one already on the expiry list (ended this pass)
            seen = true;
            CHECK(f->cargo_goods == kAlcohol); // D 0x100756a9-0x100756c3: pre-set by the send
            CHECK(f->cargo_88 == 0);
        }
    }
    CHECK(seen);
    advance_to(w, 5300);
    CHECK(inn_at(w).tavern == 0);
    CHECK(city0(w).stock.stock[kAlcohol] == 0);
    CHECK(carriers(w).empty()); // it ended at home at once
    CHECK(inn_at(w).carriers_out == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("inn save and reload with stock above zero and a carrier on its way") {
    const auto types = registry();
    sim::Inn busy;
    busy.tavern = 300;
    busy.taken = 2000;
    Rig r = make_rig();
    r.place(kMarketT, 110, 110);
    r.place(kInnT, 111, 115, busy);
    r.city().stock = stock_of(kAlcohol, 12800);
    sim::World a = create(r, types);
    until(a, [&] { return !carriers(a).empty(); });
    advance_to(a, 5000 + 1500); // on the way out
    REQUIRE(carriers(a).size() == 1);
    auto back = sim::World::load(a.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    sim::World c = std::move(back).value();
    CHECK(c.state_hash() == a.state_hash());
    CHECK(inn_at(c).carriers_out == 1);
    CHECK(inn_at(c).tavern == 300);
    CHECK(inn_at(c).taken == 1937);
    CHECK(inn_at(c).last == 5000);
    // the carrier is back at about 5009 + 3001 + 2501 = 10511
    for (int i = 0; i < 4300; ++i) {
        a.advance(2);
        c.advance(2);
    }
    CHECK(c.state_hash() == a.state_hash());
    CHECK(inn_at(c).tavern == 300 + 768);
    CHECK(inn_at(c).carriers_out == 0);
    CHECK(c.check_invariants() == "");
}

TEST_CASE("inn state: the 16-bit fields are refused above 65535, and a zero input is a typed refusal") {
    const auto types = registry();
    {
        sim::Inn bad;
        bad.tavern = 65536;
        Rig r = make_rig();
        r.place(kInnT, 111, 115, bad);
        auto w = sim::World::create(r.spec, types, syn::population_config());
        CHECK_FALSE(w.ok());
    }
    {
        sim::Inn edge;
        edge.tavern = 65535;
        edge.taken = 65535;
        Rig r = make_rig();
        r.place(kInnT, 111, 115, edge);
        auto w = sim::World::create(r.spec, types, syn::population_config());
        CHECK(w.ok());
    }
    {
        sim::TypeRegistry::Builder b = base_builder();
        b.add(inn_type(kInnT, kAlcohol, 0, 1)); // the base priority would divide by 0
        auto built = b.build();
        REQUIRE_FALSE(built.ok());
        CHECK(built.error().find("input per step of 0") != std::string::npos);
    }
}
