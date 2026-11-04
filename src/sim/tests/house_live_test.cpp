// MAP-POP-LIVE-1 L4: the live House update through the production scheduler
// (docs/sim/population-live.md sections 1, 6, 12, 19) with Rules::house_updates on.
//
// Every number below is hand-derived (tools/house_live_ref.py and fixtures/synthetic/sim/
// house_live_vectors.txt carry the chain and the stage pass independently). Setup shared by the
// tests: island 0 at (100, 100) 32 x 32, city 0 of player 0, house type 9501 (PRODTYPE 97, walker
// maximum 2, figure ids 203..205) at (110, 110) with serial 1, a stall at (114, 110). The world
// PRNG table is the 16 values 7 i + 1 with cursor 13 (seed 77 mod 16), so the draws are T[14] = 99,
// T[15] = 106, T[0] = 1, ... The city ring visits city 0 on advance number 1 + 16 k, so the House
// of city 0 is visited at ticks t = 1 + 16 k (the first advance, d = 1, one ring slot per advance);
// the first taken gate (u16(t - last) >= 2500, last = 0) is therefore t = 2513 and the next one,
// with last = 2500, is t = 5009.
//
// Spawn variant: k = ((r & 1) + y + x + serial) mod 3 = ((r & 1) + 221) mod 3: r = 99 (odd) -> 222
// mod 3 = 0 -> figure 203; r = 106 (even) -> 221 mod 3 = 2 -> figure 205.
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/building.hpp>
#include <anno/sim/create.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/resident_job.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

constexpr sim::TypeId kP{9501}; // group 0, variants 9501 / 9503 (count 2, step 2), walker max 2
constexpr sim::TypeId kP3{9503};
constexpr sim::TypeId kPnoEnt{9505}; // group 0 without an entrance point
constexpr sim::TypeId kS0{9520}; // group 1, variants 9520..9522, walker max 1, costs tools + wood
constexpr sim::TypeId kS1{9521};
constexpr sim::TypeId kS2{9522};
constexpr sim::TypeId kStall{9530};
constexpr sim::GoodsId kFood = 26, kOther = 27;
constexpr std::uint32_t kTools = 0x29, kWood = 0x2a;

struct Opt {
    bool idsec = false;        // goods 26 -> 27 (the replay of a delivery)
    bool low_negative = false; // group 0 food table: -2 below index 8, else 2 (else all 2)
    bool bau27 = false;        // goods 27 is enabled by stage 1 only
    bool stages = false;       // [BAUSTUFEN]: stage 1 = group 0 with 5 residents, stage 2 = 50
};

sim::BuildingType house_t(sim::TypeId id, std::uint32_t prodtype, std::uint32_t walkers,
                          std::uint8_t head11, bool entrance = true) {
    sim::BuildingType t;
    t.id = id;
    t.prodtype = prodtype;
    t.cls = sim::RuntimeClass::House;
    t.width = t.height = 2;
    t.action_radius = 8;
    t.head_raw.assign(56, 0);
    t.head_raw[11] = head11;
    if (entrance) {
        t.entrances = {{3, 3}};
    }
    t.has_prod = true;
    t.prod.kind = sim::BuildingClass::Other;
    t.prod.max_figures = {walkers, 0, 0};
    t.prod.figure_type_count = {3, 0, 0};
    t.prod.figure_ids = {203, 204, 205, 0, 0, 0, 0, 0, 0};
    return t;
}

std::shared_ptr<const sim::TypeRegistry> registry(Opt o = {},
                                                  std::vector<sim::ClassInfo> classes = {},
                                                  bool group0_downgradable = false,
                                                  bool empty_group = false) {
    sim::HouseGroup g0;
    g0.number = 0;
    g0.wohnmax = 8;
    g0.no_downgrade = !group0_downgradable;
    g0.needs = {{kFood, true, 41}, {kOther, true, 16}};
    g0.vermehr = {-16, -9, 0, 12, 32};
    for (std::size_t i = 0; i < 21; ++i) {
        g0.nahrung[i] = o.low_negative && i < 8 ? -2 : 2;
        g0.bedarf[i] = 2;
        g0.infra[i] = 2;
    }
    sim::HouseGroup g1;
    g1.number = 1;
    g1.wohnmax = 15;
    g1.needs = {{kFood, true, 60}, {kOther, true, 40}};
    if (empty_group) {
        g1.needs.clear();
    }
    g1.vermehr = g0.vermehr;
    for (std::size_t i = 0; i < 21; ++i) {
        g1.nahrung[i] = i < 10 ? -2 : 2;
        g1.bedarf[i] = 2;
        g1.infra[i] = 2;
    }
    std::vector<sim::GoodsInfo> goods = sim::original_goods();
    if (o.idsec) {
        goods[kFood].id_sec = kOther;
    }
    if (o.bau27) {
        goods[kOther].bau_first = 1;
    }
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.goods(goods);
    if (!classes.empty()) {
        b.classes(classes);
    }
    b.house_groups({g0, g1});
    if (o.stages) {
        b.stage_table({{1, 0, 0, 5}, {2, 0, 0, 50}});
    }
    b.add(house_t(kP, 97, 2, static_cast<std::uint8_t>(2 | (2 << 6))));
    b.add(house_t(kP3, 97, 2, 0));
    b.add(house_t(kPnoEnt, 97, 2, 0, false));
    sim::BuildingType s0 = house_t(kS0, 98, 1, static_cast<std::uint8_t>(3 | (1 << 6)));
    s0.cost_goods = {{kTools, 512}, {kWood, 1024}};
    b.add(s0);
    b.add(house_t(kS1, 98, 1, 0));
    b.add(house_t(kS2, 98, 1, 0));
    sim::BuildingType stall;
    stall.id = kStall;
    stall.prodtype = 161;
    stall.cls = sim::RuntimeClass::MarketStall;
    stall.entrances = {{3, 3}};
    stall.output_goods = {kFood};
    b.add(stall);
    for (const std::uint32_t id : {203U, 204U, 205U}) {
        b.figure_type(sim::FigureType{id, 805300, 3 * 256, 0, 1000});
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

sim::House make_house(sim::TypeId type, std::int32_t residents_64, std::uint32_t e,
                      std::vector<std::uint32_t> stock, std::uint16_t last = 0) {
    sim::House h;
    h.last = last;
    h.residents_64 = residents_64;
    h.e = e;
    h.stock = std::move(stock);
    h.walkers.assign(type == kP || type == kP3 || type == kPnoEnt ? 2 : 1, sim::FigureId{});
    return h;
}

struct Rig {
    sim::ScenarioSpec spec;
    void place(sim::TypeId type, std::int32_t x, std::int32_t y,
               std::optional<sim::Payload> payload = std::nullopt) {
        sim::ScenarioSpec::BuildingSpec b;
        b.type = type;
        b.x = x;
        b.y = y;
        b.payload = std::move(payload);
        spec.islands[0].cities[0].buildings.push_back(std::move(b));
    }
};

Rig make_rig() {
    Rig r;
    r.spec = syn::empty_scenario(77);
    std::vector<std::uint16_t> table;
    for (std::uint16_t i = 0; i < 16; ++i) {
        table.push_back(static_cast<std::uint16_t>(7 * i + 1));
    }
    r.spec.rng_table = table;
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

sim::City& city0(sim::World& w) {
    return *sim::WorldTestAccess::islands(w)[0]->cities[0];
}
const sim::Building* at(const sim::World& w, std::int32_t x, std::int32_t y) {
    return w.islands()[0]->building_at(w.config().layout, sim::CityNo{0}, x, y);
}
const sim::House& house_of(const sim::World& w) {
    const sim::Building* b = at(w, 110, 110);
    REQUIRE(b != nullptr);
    const auto* h = std::get_if<sim::House>(&b->payload);
    REQUIRE(h != nullptr);
    return *h;
}
bool same_house(const sim::House& a, const sim::House& b) {
    return a.last == b.last && a.residents_64 == b.residents_64 && a.e == b.e &&
           a.stock == b.stock && a.walkers == b.walkers;
}
std::uint32_t object_at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[0];
    return is.tiles.object(is.tiles.index_of(x, y));
}
std::size_t figure_count(const sim::World& w) {
    std::size_t n = 0;
    for (const auto& s : w.figures().list.slots) {
        n += s.has_value() ? 1 : 0;
    }
    return n;
}
std::vector<const sim::Figure*> walkers(const sim::World& w) {
    std::vector<const sim::Figure*> out;
    for (const auto& s : w.figures().list.slots) {
        if (s && s->job == sim::kJobResident) {
            out.push_back(&*s);
        }
    }
    return out;
}
bool ended(const sim::World& w, sim::FigureId id) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    return f == nullptr || f->expiry_stamp.has_value();
}

/// One advance of one tick; the invariants every 64 ticks (and at once when `always`).
void step(sim::World& w, bool always = false) {
    w.advance(2);
    if (always || w.clock().tick() % 64 == 0) {
        REQUIRE_MESSAGE(w.check_invariants() == "", w.check_invariants());
    }
}

/// Advances until `done` (at most `limit` ticks).
template <class F> void until(sim::World& w, F done, std::uint64_t limit = 20000) {
    for (std::uint64_t i = 0; i < limit; ++i) {
        step(w);
        if (done()) {
            return;
        }
    }
    FAIL("the condition did not hold within the limit");
}

std::size_t draws_at(const sim::World& w, sim::DrawSite site) {
    for (const sim::DrawRecord& r : w.last_output().draws) {
        if (r.site == site) {
            return r.count;
        }
    }
    return 0;
}

/// The standard chain world: house (110, 110) with E 640, 5 residents, empty stock; a stall; the
/// city store of food = `store`.
sim::World chain_world(const std::shared_ptr<const sim::TypeRegistry>& types,
                       std::uint32_t store = 100) {
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
    r.place(kStall, 114, 110);
    sim::World w = create(r, types);
    city0(w).stock.stock[kFood] = store;
    return w;
}

} // namespace

TEST_CASE(
    "house live: shortage, spawn, purchase, delivery and the next gate through the scheduler") {
    // Chain K1 of house_live_vectors.txt (goods 26 -> 27 IdSec, store 100). Hand derivation:
    //  gate 1 (t = 2513, last 0): E' = 635 + 5 = 640, stock stays 0, lowest 0 < 102 -> ONE draw
    //   (T[14] = 99, figure 203); mood = min(-2, 2, 2) = -2 -> residents 320 - 16 = 304; last 2500.
    //  purchase: R = 4, threshold trunc(4*41*256/4096) = 10, store 100 > 10, demand
    //   trunc(41*640*256/44236) = 151 -> amount min(100, 151, 768) = 100: city store 100 -> 0,
    //   billed +100, house stock +100*16 = 1600 on slot 0 and, by the IdSec replay, on slot 1.
    //  gate 2 (t = 5009, last 2500): E' = trunc(127*640/128) + 4 = 639, stock 1587 / 1587
    //   (trunc(127*1600/128)), s0 = 93 (< 102: ONE draw, T[15] = 106, figure 205), mood
    //   (2 + 2 + 2 + 1) / 3 = 2 -> residents 304 + 32 = 336.
    Opt o;
    o.idsec = true;
    o.low_negative = true;
    const auto types = registry(o);
    sim::World w = chain_world(types, 100);
    REQUIRE(w.rng().cursor() == 13);
    REQUIRE(w.rng().table()[14] == 99);
    REQUIRE(w.rng().table()[15] == 106);
    REQUIRE(sim::serial_of(at(w, 110, 110)->id) == 1);
    const sim::House initial = house_of(w);

    // The skipped gates: every visit before t = 2513 (t = 1, 17, ..., 2497) changes nothing and
    // draws nothing.
    while (w.clock().tick() < 2512) {
        step(w);
        REQUIRE(same_house(house_of(w), initial));
        REQUIRE(w.rng().cursor() == 13);
    }
    CHECK(figure_count(w) == 0);
    CHECK(w.last_output().draws.empty());

    // The taken gate.
    until(w, [&] { return house_of(w).last != 0; }, 100);
    CHECK(w.clock().tick() == 2513);
    CHECK(house_of(w).last == 2500); // last += period, not = t
    CHECK(house_of(w).e == 640);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{0, 0}));
    CHECK(house_of(w).residents_64 == 304);
    REQUIRE(w.last_output().draws.size() == 1);
    CHECK(w.last_output().draws[0].site == sim::DrawSite::HouseWalkerTarget);
    CHECK(w.last_output().draws[0].count == 1);
    CHECK(w.last_output().rng_cursor_before == 13);
    CHECK(w.last_output().rng_cursor_after == 14);
    REQUIRE(walkers(w).size() == 1);
    const sim::FigureId first = walkers(w)[0]->id;
    CHECK(walkers(w)[0]->kind == 203);
    CHECK(walkers(w)[0]->home == at(w, 110, 110)->id);
    CHECK(house_of(w).walkers[0] == first); // the slot is taken at the merge
    CHECK(house_of(w).walkers[1] == sim::FigureId{});

    // The walker's life: purchase, walk home, delivery, end, slot released.
    until(w, [&] { return ended(w, first); });
    const sim::Figure* f = sim::find_figure(w.figures(), first);
    REQUIRE(f != nullptr);
    CHECK(f->resident->cargo[0] == sim::ResidentCargo{kFood, 100});
    CHECK(f->resident->cargo[1] == sim::ResidentCargo{});
    CHECK(city0(w).stock.stock[kFood] == 0); // down by the bought amount
    CHECK(city0(w).billed[kFood] == 100);    // up by the requested amount
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{1600, 1600}));
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
    CHECK(house_of(w).last == 2500); // the purchase does not touch the gate

    // The next taken gate sees the new stock.
    until(w, [&] { return house_of(w).last != 2500; });
    CHECK(w.clock().tick() == 5009);
    CHECK(house_of(w).last == 5000);
    CHECK(house_of(w).e == 639);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{1587, 1587}));
    CHECK(house_of(w).residents_64 == 336);
    REQUIRE(w.last_output().draws.size() == 1);
    CHECK(w.last_output().draws[0].site == sim::DrawSite::HouseWalkerTarget);
    CHECK(w.last_output().rng_cursor_after == 15);
    REQUIRE(walkers(w).size() == 2); // the first one is still in the registry (expiry), the new one
    // The new walker (the store is empty: no stand) has ended within its first call; it was
    // announced with the figure type of T[15] = 106 (even): 205.
    bool announced = false;
    for (const sim::SimEvent& e : w.last_output().events) {
        announced = announced || (e.kind == sim::SimEventKind::FigureSpawned && e.code == 205);
    }
    CHECK(announced);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("house live: a refused send still consumes the draw") {
    // X14: the house type without an entrance point draws once (T[14]) and sends nothing.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kPnoEnt, 110, 110, make_house(kPnoEnt, 320, 640, {0, 0}));
    sim::World w = create(r, types);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.clock().tick() == 2513);
    CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
    CHECK(w.last_output().rng_cursor_after == 14);
    CHECK(walkers(w).empty());
    CHECK(house_of(w).walkers[0] == sim::FigureId{});
}

TEST_CASE(
    "house live: the upgrade tick draws the spawn BEFORE the variant, and every effect lands") {
    // A group-0 house at the cap: residents 512, E 640, stock {2000, 500}, tools 1000, wood 2000.
    //  E' = 635 + 8 = 643; stock {1984, 496}; s0 = 115, s1 = 74 -> lowest 74 < 102: spawn draw
    //  T[14] = 99 (figure 203). Mood 2 -> residents 544 > 512: cap -> 512, UpgradeCheck: food
    //  trunc(1536*1984/(643*60)) = 78 -> table[(78+4)>>3 = 10] = 2 > 0, mean 74, infra 160:
    //  eligible; materials (512 tools, 1024 wood) available -> Upgrade: spend (tools 488, wood
    //  976), variant draw T[15] = 106: 106 mod 3 = 1 -> type 9521 (the other order would draw 99 ->
    //  9520 and give the walker figure 205). SetGroup (upgrade): satisfaction 115 / 74, new stock
    //  trunc(60*115*643/1536) = 2888 and trunc(40*74*643/1536) = 1239; E and residents are kept.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 512, 640, {2000, 500}));
    sim::World w = create(r, types);
    city0(w).stock.stock[kTools] = 1000;
    city0(w).stock.stock[kWood] = 2000;
    REQUIRE(house_of(w).walkers.size() == 2);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.clock().tick() == 2513);
    const sim::Building& b = *at(w, 110, 110);
    CHECK(b.type == kS1);
    CHECK(object_at(w, 110, 110) == kS1.v); // the footprint is re-stamped
    CHECK(object_at(w, 111, 111) == kS1.v);
    CHECK(house_of(w).last == 2500);
    CHECK(house_of(w).e == 643);
    CHECK(house_of(w).residents_64 == 512);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{2888, 1239}));
    CHECK(city0(w).stock.stock[kTools] == 488);
    CHECK(city0(w).stock.stock[kWood] == 976);
    CHECK(city0(w).build_goods[0].spent == 512);
    CHECK(city0(w).build_goods[1].spent == 1024);
    CHECK(city0(w).index.house_counts[0] == 0);
    CHECK(city0(w).index.house_counts[1] == 1);
    // Two draws, in this order: the spawn (T[14]) and the variant (T[15]).
    CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
    CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 1);
    CHECK(w.last_output().rng_cursor_before == 13);
    CHECK(w.last_output().rng_cursor_after == 15);
    REQUIRE(walkers(w).size() == 1);
    CHECK(walkers(w)[0]->kind == 203); // spawn used T[14] = 99 (odd), not 106
    // (the walker finds no stand and walks home within its first call: it has ended already)
    bool announced = false;
    for (const sim::SimEvent& e : w.last_output().events) {
        announced = announced || (e.kind == sim::SimEventKind::FigureSpawned &&
                                  e.building == b.id && e.code == 203);
    }
    CHECK(announced);
    CHECK(house_of(w).walkers.size() == 1); // lazy trim of the empty trailing slot
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "house live: a satisfied house draws nothing and a refused upgrade changes only the gate") {
    // stock {5000, 5000}: lowest 160 -> no spawn draw; the upgrade has no materials in the store.
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 512, 640, {5000, 5000}));
    sim::World w = create(r, types);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.last_output().draws.empty());
    CHECK(at(w, 110, 110)->type == kP); // UpgradeRefusedMaterials: nothing spent, nothing drawn
    CHECK(house_of(w).residents_64 == 512);
    CHECK(house_of(w).e == 643);
    CHECK(walkers(w).empty());
}

TEST_CASE("house live: downgrade and removal, with the spawn draw first") {
    Opt o;
    o.low_negative = true;
    const auto types = registry(o);
    SUBCASE("downgrade of a group-1 house: variant of 9501 after the spawn draw") {
        // residents 300, E 512, stock {1000, 800}: E' = 508 + 4 = 512, stock {992, 793}, s0 = 49,
        // s1 = 59, lowest 49: spawn T[14] (figure 203). Group 1 food table -2 below index 10: mood
        // -2
        // -> residents 284 < floor 48 * 8 = 384 -> Downgrade. Variant of 9501 (count 2, step 2):
        // T[15] = 106 even -> idx 0 -> 9501. SetGroup (downgrade): E = 4 << 7 = 512, stock
        // trunc(41*49*512/1536) = 669 and trunc(16*59*512/1536) = 314.
        Rig r = make_rig();
        r.place(kS0, 110, 110, make_house(kS0, 300, 512, {1000, 800}));
        sim::World w = create(r, types);
        until(w, [&] { return house_of(w).last != 0; }, 3000);
        CHECK(w.clock().tick() == 2513);
        CHECK(at(w, 110, 110)->type == kP);
        CHECK(house_of(w).e == 512);
        CHECK(house_of(w).residents_64 == 284);
        CHECK((house_of(w).stock == std::vector<std::uint32_t>{669, 314}));
        CHECK(city0(w).index.house_counts[1] == 0);
        CHECK(city0(w).index.house_counts[0] == 1);
        CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
        CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 1);
        REQUIRE(walkers(w).size() == 1);
        CHECK(walkers(w)[0]->kind == 203);
        CHECK(house_of(w).walkers.size() == 2); // grown to the new maximum
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("removal of a group-0 house below the floor: the spawn draw, no figure") {
        // residents 70, E 512 -> E' = 508 + 1 = 509, lowest 0: spawn draw T[14]; mood -2 ->
        // residents 54 < floor 64 (NoDowngrade) -> Remove. The request names a house that is gone
        // by the merge: no figure.
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 70, 512, {0, 0}));
        sim::World w = create(r, types);
        until(w, [&] { return at(w, 110, 110) == nullptr; }, 3000);
        CHECK(w.clock().tick() == 2513);
        CHECK(object_at(w, 110, 110) == 0);
        CHECK(city0(w).index.house_counts[0] == 0);
        CHECK(draws_at(w, sim::DrawSite::HouseWalkerTarget) == 1);
        CHECK(draws_at(w, sim::DrawSite::HouseVariant) == 0);
        CHECK(w.last_output().rng_cursor_after == 14);
        CHECK(walkers(w).empty());
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("house live: the stage pass runs after the building ticks and unlocks a need") {
    // Stage 1 = 5 residents of group 0 and above; goods 27 is enabled by stage 1 only (bau 1).
    // House: residents 320 (5), E 640, stock {0, 1000}; player mask 1.
    //  t = 2513: the House tick (mask 1: need 27 NOT selected, stock stays 1000), then the pass:
    //   residents 352 -> 5 >= 5: city mask 0x2, stamp 2513; player 1 < 2 -> flags 0x3.
    //  t = 5009: the House tick sees mask 3: stock1 = trunc(127*1000/128) = 992; the pass is gated
    //   (2496 < 2500, the stamp stays 2513); t = 5025: the pass runs again (stamp 5025).
    Opt o;
    o.bau27 = true;
    o.stages = true;
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 1000}));
    sim::World w = create(r, types);
    CHECK(city0(w).stage_mask == 0);
    CHECK(city0(w).stage_stamp == 0);
    until(w, [&] { return house_of(w).last != 0; }, 3000);
    CHECK(w.clock().tick() == 2513);
    CHECK((house_of(w).stock == std::vector<std::uint32_t>{0, 1000})); // not decayed: pass is later
    CHECK(house_of(w).residents_64 == 352);
    CHECK(city0(w).stage_mask == 0x2);
    CHECK(city0(w).stage_stamp == 2513); // the stamp is t, not stamp + 2500
    CHECK(w.players().find(sim::PlayerId{0})->flags == 0x3);
    until(w, [&] { return house_of(w).last != 2500; });
    CHECK(w.clock().tick() == 5009);
    CHECK(
        (house_of(w).stock == std::vector<std::uint32_t>{0, 992})); // the unlock reached the kernel
    CHECK(city0(w).stage_stamp == 2513);                            // gated: 2496 < 2500
    until(w, [&] { return city0(w).stage_stamp != 2513; });
    CHECK(w.clock().tick() == 5025);
    CHECK(city0(w).stage_stamp == 5025);
    CHECK(city0(w).stage_mask == 0x2); // stage 2 needs 50 residents
}

TEST_CASE("house live: the player-mask rule compares the low 23 bits (X16)") {
    Opt o;
    o.stages = true;
    const auto types = registry(o);
    struct Case {
        std::uint32_t player, city_mask, want;
    };
    // The pass sets stage 1 (5 residents) on top of the preset city mask 0x8 -> 0xa.
    for (const Case c : {Case{0x10, 0x8, 0x10}, // 0x10 >= 0xa: skipped (an OR would give 0x1a)
                         Case{0x04, 0x8, 0x0e}, // 4 < 0xa: performed (full dword OR)
                         Case{0xff800001U, 0x8, 0xff80000bU}, // high bits ignored by the compare
                         Case{0x0a, 0x8, 0x0a}}) {            // equal: skipped
        INFO("player " << c.player);
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}));
        sim::World w = create(r, types);
        city0(w).stage_mask = c.city_mask;
        sim::WorldTestAccess::players(w).find(sim::PlayerId{0})->flags = c.player;
        until(w, [&] { return city0(w).stage_stamp != 0; }, 3000);
        CHECK(city0(w).stage_mask == (c.city_mask | 0x2));
        CHECK(w.players().find(sim::PlayerId{0})->flags == c.want);
    }
}

TEST_CASE(
    "house live: a crafted house (E = 0, residents 0) fails the advance with the typed message") {
    const auto types = registry();
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 0, 0, {5, 5}));
    sim::World w = create(r, types);
    until(w, [&] { return w.faulted(); }, 3000);
    CHECK(w.clock().tick() == 2513);
    CHECK(w.fault() == "house fault not_evaluated building 1 (kernel: zero_divisor)");
    CHECK(w.check_invariants() ==
          "world fault: house fault not_evaluated building 1 (kernel: zero_divisor)");
    // Nothing of the house changed (no commit of a NotEvaluated tick) and no draw happened.
    CHECK(same_house(house_of(w), make_house(kP, 0, 0, {5, 5})));
    CHECK(w.last_output().draws.empty());
    // The world is stopped: a further advance changes nothing, not even the clock.
    const std::uint64_t hash = w.state_hash();
    const std::uint64_t tick = w.clock().tick();
    CHECK(w.advance(2) == tick);
    CHECK(w.state_hash() == hash);
    // The flag is state: it survives a save and load (the text does not).
    auto back = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    CHECK(back.value().faulted());
    CHECK(back.value().fault().empty());
    CHECK(back.value().advance(2) == tick);
    CHECK(back.value().state_hash() == hash);
}

TEST_CASE("house live: world creation refuses what the live update cannot run (P-2)") {
    const Rig r = [] {
        Rig x = make_rig();
        x.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
        return x;
    }();
    sim::Rules live;
    live.house_updates = true;
    const auto create_with = [&](const std::shared_ptr<const sim::TypeRegistry>& t,
                                 sim::Rules rules) {
        return sim::World::create(r.spec, t, syn::population_config(), rules);
    };
    SUBCASE("a gate period outside 1..65535 names the class") {
        std::vector<sim::ClassInfo> classes = sim::original_class_table();
        for (sim::ClassInfo& c : classes) {
            if (c.cls == sim::RuntimeClass::House) {
                c.gate_period = 70000;
            }
        }
        const auto t = registry({}, classes);
        const auto refused = create_with(t, live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("House class gate period 70000") != std::string::npos);
        CHECK(create_with(t, sim::Rules{}).ok()); // off: accepted as before
    }
    SUBCASE("a group without a need names the group") {
        const auto t = registry({}, {}, false, true);
        const auto refused = create_with(t, live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("house group 1 has no need") != std::string::npos);
        CHECK(create_with(t, sim::Rules{}).ok());
    }
    SUBCASE("group 0 without NoDowngrade names the group") {
        const auto t = registry({}, {}, true);
        const auto refused = create_with(t, live);
        REQUIRE_FALSE(refused.ok());
        CHECK(refused.error().find("house group 0 does not set NoDowngrade") != std::string::npos);
        CHECK(refused.error().find("PreviousCapacityUnavailable") != std::string::npos);
        CHECK(create_with(t, sim::Rules{}).ok());
    }
}

TEST_CASE("house live: the stamp of a created house, the cadence and the loaded last (P-6, X15)") {
    const auto types = registry();
    sim::PlacementRecord rec;
    rec.type = kP;
    rec.x = 120;
    rec.y = 120;
    SUBCASE("a created house stamps u16(tick) only with the rule on") {
        for (const bool live : {true, false}) {
            Rig r = make_rig();
            sim::World w = create(r, types, live);
            const auto made = sim::WorldTestAccess::create(w, 0, 0, rec, 70000);
            REQUIRE(made.ok());
            const auto* h = std::get_if<sim::House>(&at(w, 120, 120)->payload);
            REQUIRE(h != nullptr);
            CHECK(h->last == (live ? 4464 : 0)); // u16(70000) = 4464
        }
    }
    SUBCASE("scenario import keeps last 0 and own saves keep the saved last") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {5000, 5000}, 1234));
        r.place(kP3, 114, 114);
        sim::World w = create(r, types);
        CHECK(house_of(w).last == 1234);
        const auto* d = std::get_if<sim::House>(&at(w, 114, 114)->payload);
        REQUIRE(d != nullptr);
        CHECK(d->last == 0);
        auto back = sim::World::load(w.save(), types);
        REQUIRE(back.ok());
        CHECK(house_of(back.value()).last == 1234);
    }
    SUBCASE("a stamped house waits its period; an unstamped one takes at once (t = 70000)") {
        // Stamped (last 4464): the clock is set to t = 69999, so the first advance (t = 70000, d =
        // 70000) visits every ring slot and city 0 is visited again at t = 70001 + 16 k. The first
        // visit with elapsed >= 2500 (t >= 72500) is k = 157, t = 72513; the next one needs t >=
        // 75000: k = 313, t = 75009.
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}, 4464));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 70000 - 2;
        sim::WorldTestAccess::clock(w).last_tick = 69999;
        std::vector<std::uint64_t> takes;
        std::uint16_t last = house_of(w).last;
        while (w.clock().tick() < 75100) {
            w.advance(2);
            if (house_of(w).last != last) {
                takes.push_back(w.clock().tick());
                last = house_of(w).last;
            }
        }
        CHECK((takes == std::vector<std::uint64_t>{72513, 75009}));
        CHECK(house_of(w).last == 9464);
        CHECK(w.check_invariants() == "");
        // Unstamped (0): takes at the first visit.
        Rig r0 = make_rig();
        r0.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}, 0));
        sim::World w0 = create(r0, types);
        sim::WorldTestAccess::clock(w0).ms = 2 * 70000 - 2;
        w0.advance(2);
        CHECK(house_of(w0).last == 2500);
    }
    SUBCASE("overdue: t = 5000 with last 0 takes one step per call") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 5000 - 2;
        w.advance(2); // t = 5000: the first call, every ring slot is due
        CHECK(house_of(w).last == 2500);
        w.advance(2); // t = 5001: city 0 again (the ring wrapped)
        CHECK(house_of(w).last == 5000);
        for (int i = 0; i < 40; ++i) {
            w.advance(2);
        }
        CHECK(house_of(w).last == 5000); // 5000 -> not due again
    }
    SUBCASE("minimal cadence: repeated calls at one tick take once") {
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 2500;
        for (int i = 0; i < 40; ++i) {
            w.advance(0);
        }
        CHECK(house_of(w).last == 2500);
    }
    SUBCASE("a loaded nonzero last continues without a reset") {
        // last 1234, clock t = 2999 then one advance (t = 3000, every ring slot): city 0 is visited
        // again at t = 3001 + 16 k; the first take is the first visit with t - 1234 >= 2500, i.e.
        // t >= 3734: k = 46, t = 3737, last 3734; the second needs t >= 6234: k = 203, t = 6249.
        Rig r = make_rig();
        r.place(kP, 110, 110, make_house(kP, 320, 640, {20000, 20000}, 1234));
        sim::World w = create(r, types);
        sim::WorldTestAccess::clock(w).ms = 2 * 3000 - 2;
        w.advance(2);
        until(w, [&] { return house_of(w).last != 1234; }, 2000);
        CHECK(w.clock().tick() == 3737);
        CHECK(house_of(w).last == 3734);
        auto back = sim::World::load(w.save(), types);
        REQUIRE(back.ok());
        sim::World w2 = std::move(back).value();
        until(w, [&] { return house_of(w).last != 3734; }, 4000);
        until(w2, [&] { return house_of(w2).last != 3734; }, 4000);
        CHECK(w.clock().tick() == 6249);
        CHECK(w2.clock().tick() == 6249);
        CHECK(house_of(w).last == 6234); // 3734 + 2500
        CHECK(w.state_hash() == w2.state_hash());
    }
}

TEST_CASE("house live: with the rule off the same worlds do not tick houses at all") {
    Opt o;
    o.idsec = true;
    o.low_negative = true;
    const auto types = registry(o);
    Rig r = make_rig();
    r.place(kP, 110, 110, make_house(kP, 320, 640, {0, 0}));
    r.place(kStall, 114, 110);
    sim::World w = create(r, types, false);
    city0(w).stock.stock[kFood] = 100;
    const sim::House initial = house_of(w);
    for (int i = 0; i < 6000; ++i) {
        step(w);
    }
    CHECK(same_house(house_of(w), initial));
    CHECK(w.rng().cursor() == 13);
    CHECK(figure_count(w) == 0);
    CHECK(city0(w).stage_mask == 0);
    CHECK(city0(w).stage_stamp == 0);
    CHECK(w.players().find(sim::PlayerId{0})->flags == 1);
    CHECK(city0(w).stock.stock[kFood] == 100);
    CHECK_FALSE(w.faulted());
}

TEST_CASE("house live: save and reload at every cut of the chain, and a replay") {
    Opt o;
    o.idsec = true;
    o.low_negative = true;
    const auto types = registry(o);
    constexpr std::uint64_t kHorizon = 5100;
    sim::World ref = chain_world(types);
    while (ref.clock().tick() < kHorizon) {
        ref.advance(2);
    }
    REQUIRE(ref.check_invariants() == "");
    const std::uint64_t want = ref.state_hash();
    CHECK(house_of(ref).last == 5000);

    // Cuts: before the first gate, right after it (the walker is Start/ToStall), while it waits and
    // while it walks home with the cargo, after its delivery, and after the second gate.
    struct Cut {
        const char* name;
        std::function<bool(const sim::World&)> at;
    };
    const auto step_is = [](const sim::World& w, sim::ResidentStep s) {
        for (const sim::Figure* f : walkers(w)) {
            if (f->resident && f->resident->step == s && !f->expiry_stamp.has_value()) {
                return true;
            }
        }
        return false;
    };
    const std::vector<Cut> cuts = {
        {"before the gate", [](const sim::World& w) { return w.clock().tick() == 2000; }},
        {"gate taken", [](const sim::World& w) { return house_of(w).last == 2500; }},
        {"to stall", [&](const sim::World& w) { return step_is(w, sim::ResidentStep::ToStall); }},
        {"wait", [&](const sim::World& w) { return step_is(w, sim::ResidentStep::Wait); }},
        {"to home with cargo",
         [&](const sim::World& w) { return step_is(w, sim::ResidentStep::ToHome); }},
        {"delivered", [](const sim::World& w) { return house_of(w).stock[0] == 1600; }},
        {"second gate", [](const sim::World& w) { return house_of(w).last == 5000; }},
    };
    for (const Cut& c : cuts) {
        INFO(c.name);
        sim::World w = chain_world(types);
        while (!c.at(w) && w.clock().tick() < kHorizon) {
            w.advance(2);
        }
        REQUIRE(w.clock().tick() < kHorizon);
        REQUIRE(w.check_invariants() == "");
        const std::vector<std::uint8_t> bytes = w.save();
        auto back = sim::World::load(bytes, types);
        REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
        sim::World w2 = std::move(back).value();
        CHECK(w2.state_hash() == w.state_hash());
        CHECK(w2.save() == bytes);
        while (w2.clock().tick() < kHorizon) {
            w2.advance(2);
        }
        CHECK(w2.check_invariants() == "");
        CHECK(w2.state_hash() == want);
    }

    // Through a transition: cut right after the upgrade tick.
    {
        const auto t2 = registry();
        const auto upgrade_world = [&] {
            Rig r = make_rig();
            r.place(kP, 110, 110, make_house(kP, 512, 640, {2000, 500}));
            sim::World w = create(r, t2);
            city0(w).stock.stock[kTools] = 1000;
            city0(w).stock.stock[kWood] = 2000;
            return w;
        };
        sim::World a = upgrade_world();
        while (a.clock().tick() < 4000) {
            a.advance(2);
        }
        sim::World b = upgrade_world();
        while (b.clock().tick() < 2513) {
            b.advance(2);
        }
        REQUIRE(at(b, 110, 110)->type == kS1);
        auto back = sim::World::load(b.save(), t2);
        REQUIRE(back.ok());
        sim::World c = std::move(back).value();
        while (c.clock().tick() < 4000) {
            c.advance(2);
        }
        CHECK(c.check_invariants() == "");
        CHECK(c.state_hash() == a.state_hash());
    }

    // The recorded session replays to the same checkpoints.
    {
        sim::World w = chain_world(types);
        auto rec = sim::Recorder::start(w);
        REQUIRE(rec.ok());
        rec.value().checkpoint();
        for (int i = 0; i < 3000; ++i) {
            rec.value().advance(2);
            if (i % 500 == 0) {
                rec.value().checkpoint();
            }
        }
        rec.value().checkpoint();
        sim::World fresh = chain_world(types);
        auto outcome = sim::replay(fresh, rec.value().log());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().reproduced());
        CHECK(outcome.value().checkpoints_matched == outcome.value().checkpoints_total);
        CHECK(outcome.value().checkpoints_total >= 7);
    }
}
