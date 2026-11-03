// UnloadCmd through the public command path (MAP-CARGO-1 U3). Every command is encoded to its
// wire form, decoded again, stamped with the issuer and applied by World::apply; nothing calls the
// transfer helper directly. Contract: docs/sim/ship-cargo.md (predicate table, validation order,
// acceptance table B1-B16); evidence: docs/re/ship-cargo.md. Expected values are hand-derived in
// the contract and in fixtures/synthetic/sim/cargo_vectors.txt (tools/cargo_ref.py), never read
// back from the code under test.
//
// The world: island 0, 80 x 40 tiles at (0, 0). City 0 (player 0) owns x < 50, city 1 (player 1)
// owns x >= 50. Player 0's Kontor W (type 1302, 4 x 4) is at (15, 10); player 1's Kontor R at
// (60, 10). Player 0's ship S (type 1502, BEWOHNTYPE 113) is at tile (10, 10) with the holds of the
// opening after founding: wood 38 t, food 10 t, tools 45 t, tools 30 t.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/ship_cargo.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

#include "evidence_out.hpp"
#include "golden.hpp"
#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

constexpr std::uint32_t kT = 256;
constexpr sim::GoodsId kFood = 26, kTools = 41, kWood = 42;
constexpr std::int32_t kWidth = 80, kHeight = 40;
constexpr sim::TypeId kKontor{1302};
constexpr sim::TypeId kWeaver{9201}; // a workshop: a building that is not a warehouse

std::shared_ptr<const sim::TypeRegistry> registry() {
    auto r = syn::make_founding_types();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value().registry;
}

struct ShipDef {
    std::uint32_t kind = 1502;
    std::uint32_t owner = 0;
    int tx = 10, ty = 10;
    std::optional<std::uint32_t> parent;
    std::vector<sim::ShipHold> holds = {
        sim::ShipHold{kWood, 38 * kT}, sim::ShipHold{kFood, 10 * kT},
        sim::ShipHold{kTools, 45 * kT}, sim::ShipHold{kTools, 30 * kT}};
    std::optional<std::int32_t> raw_x, raw_y; // a Q24.8 position instead of the tile centre
};

struct Setup {
    std::vector<ShipDef> ships = {ShipDef{}};
    std::map<sim::GoodsId, std::uint32_t> stock; // city 0
    std::optional<std::uint32_t> capacity;       // city 0
    sim::TypeId wtype = kKontor;                 // city 0's warehouse
    int wx = 15, wy = 10;
    std::uint32_t rotation = 0;
    sim::TypeId rival_type = kKontor; // city 1's warehouse
    int rival_x = 60, rival_y = 10;
    bool workshop = false; // a weaver at (30, 20) in city 0
    bool second_island = false;
    // The owner of a tile: city number, 15 = neutral.
    std::function<std::uint32_t(int, int)> owner = [](int x, int) { return x >= 50 ? 1U : 0U; };
    std::shared_ptr<const sim::TypeRegistry> types = registry();
    sim::Rules rules;
    sim::SimConfig cfg;
    std::uint32_t seed = 1234;
};

sim::ScenarioSpec spec_of(const Setup& s) {
    sim::ScenarioSpec spec = syn::empty_scenario(s.seed);
    for (std::uint32_t p = 0; p < 3; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        ps.kind = 8;
        ps.money = 5000 + 111 * p;
        spec.players.push_back(ps);
    }
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 0, 0, kWidth, kHeight);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::uint32_t o = s.owner(x, y);
            if (!is.owner_runs.empty() && is.owner_runs.back().first == o) {
                ++is.owner_runs.back().second;
            } else {
                is.owner_runs.emplace_back(o, 1U);
            }
        }
    }
    const auto stock_of = [](const std::map<sim::GoodsId, std::uint32_t>& m) {
        std::vector<std::uint32_t> v(sim::kOriginalWarehouseGoods, 0);
        for (const auto& [g, u] : m) {
            v[g] = u;
        }
        return v;
    };
    sim::ScenarioSpec::CitySpec c0 = syn::plain_city(0, 0);
    c0.stock = stock_of(s.stock);
    c0.capacity = s.capacity;
    c0.buildings.push_back(
        {.type = s.wtype, .x = s.wx, .y = s.wy, .rotation = std::uint8_t(s.rotation)});
    if (s.workshop) {
        c0.buildings.push_back({.type = kWeaver, .x = 30, .y = 20});
    }
    is.cities.push_back(c0);
    sim::ScenarioSpec::CitySpec c1 = syn::plain_city(1, 1);
    c1.buildings.push_back({.type = s.rival_type, .x = s.rival_x, .y = s.rival_y});
    is.cities.push_back(c1);
    spec.islands.push_back(std::move(is));
    if (s.second_island) {
        sim::ScenarioSpec::IslandSpec is2 = syn::plain_island(1, 200, 0, 20, 20);
        is2.owner_runs = {{2, 20U * 20U}};
        sim::ScenarioSpec::CitySpec c2 = syn::plain_city(2, 0);
        c2.buildings.push_back({.type = kKontor, .x = 205, .y = 5});
        is2.cities.push_back(c2);
        spec.islands.push_back(std::move(is2));
    }
    for (const ShipDef& d : s.ships) {
        sim::ScenarioSpec::FigureSpec f;
        f.kind = d.kind;
        f.owner = sim::PlayerId{d.owner};
        f.pos =
            sim::FixedPos{d.raw_x.value_or(d.tx * 256 + 128), d.raw_y.value_or(d.ty * 256 + 128)};
        f.parent = d.parent;
        if (d.parent.has_value()) {
            f.crew_slot = 0;
        }
        f.holds = d.holds;
        spec.figures.push_back(f);
    }
    return spec;
}

sim::World make(const Setup& s) {
    auto r = sim::World::create(spec_of(s), s.types, s.cfg, s.rules);
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return std::move(r).value();
}

const sim::Island& isl(const sim::World& w, std::uint32_t slot = 0) {
    return *w.islands()[slot];
}
const sim::City& city(const sim::World& w, std::uint32_t no, std::uint32_t slot = 0) {
    return *isl(w, slot).cities[no];
}
const sim::Figure& ship(const sim::World& w, std::uint64_t id = 1) {
    const sim::Figure* f = sim::find_figure(w.figures(), sim::FigureId{id});
    REQUIRE(f != nullptr);
    return *f;
}
sim::BuildingId bid(const sim::World& w, std::uint32_t no, int x, int y, std::uint32_t slot = 0) {
    const sim::Building* b =
        w.building_at(sim::CityRef{sim::IslandId{slot}, sim::CityNo{no}}, x, y);
    REQUIRE_MESSAGE(b != nullptr, "no building at " << x << "," << y);
    return b->id;
}

/// The first building of city `no` whose type is one of `types` (the list order).
sim::BuildingId first_of(const sim::World& w, std::uint32_t no,
                         std::initializer_list<std::uint32_t> types, std::uint32_t slot = 0) {
    for (const sim::BuildingList& list : city(w, no, slot).index.lists) {
        for (const std::optional<sim::Building>& b : list.slots) {
            if (b.has_value() && std::find(types.begin(), types.end(), b->type.v) != types.end()) {
                return b->id;
            }
        }
    }
    FAIL("no building of the type in city " << no);
    return {};
}

// ---- the wide (two-u64) conservation accumulator ---------------------------------------------

struct Wide {
    std::uint64_t hi = 0, lo = 0;
    void add(std::uint64_t v) {
        lo += v;
        if (lo < v) {
            ++hi;
        }
    }
    friend bool operator==(const Wide&, const Wide&) = default;
};

/// Per goods: the sum of every hold of every figure and of every city store of every island.
std::map<sim::GoodsId, Wide> totals(const sim::World& w) {
    std::map<sim::GoodsId, Wide> t;
    for (const std::optional<sim::Figure>& f : w.figures().list.slots) {
        if (f.has_value()) {
            for (const sim::ShipHold& h : f->holds) {
                if (h.amount_88 != 0) {
                    t[h.goods].add(h.amount_88);
                }
            }
        }
    }
    for (const std::optional<sim::Island>& i : w.islands()) {
        if (!i.has_value()) {
            continue;
        }
        for (const std::optional<sim::City>& c : i->cities) {
            if (!c.has_value()) {
                continue;
            }
            for (std::size_t g = 0; g < c->stock.stock.size(); ++g) {
                if (c->stock.stock[g] != 0) {
                    t[static_cast<sim::GoodsId>(g)].add(c->stock.stock[g]);
                }
            }
        }
    }
    return t;
}

// ---- snapshots --------------------------------------------------------------------------------

struct Snap {
    std::vector<std::uint8_t> save;
    std::uint64_t hash = 0;
    std::uint64_t cursor = 0;
    std::vector<std::int64_t> money;
    std::vector<std::vector<sim::ShipHold>> holds;
    std::vector<std::vector<std::uint32_t>> stocks;
};

Snap snap(const sim::World& w) {
    Snap s{w.save(), w.state_hash(), w.rng().cursor(), {}, {}, {}};
    for (std::uint32_t p = 0; p < 3; ++p) {
        s.money.push_back(w.players().find(sim::PlayerId{p})->money);
    }
    for (const std::optional<sim::Figure>& f : w.figures().list.slots) {
        if (f.has_value()) {
            s.holds.push_back(f->holds);
        }
    }
    for (const std::optional<sim::Island>& i : w.islands()) {
        if (i.has_value()) {
            for (const std::optional<sim::City>& c : i->cities) {
                if (c.has_value()) {
                    s.stocks.push_back(c->stock.stock);
                }
            }
        }
    }
    return s;
}

// ---- sending a command the way a host does ----------------------------------------------------

struct Sent {
    sim::ApplyResult r;
    std::vector<sim::SimEvent> events;
};

sim::UnloadCmd cmd(std::uint64_t ship_id, sim::BuildingId wh, std::uint32_t hold,
                   sim::GoodsId goods, std::uint32_t tons) {
    return sim::UnloadCmd{sim::FigureId{ship_id}, wh, hold, goods, tons};
}

/// encode -> decode (the turn entry, host stamped) -> World::apply. The decoded body must equal the
/// sent one.
Sent send(sim::World& w, std::uint32_t issuer, const sim::UnloadCmd& c) {
    const sim::TurnEntry in{sim::Command{sim::PlayerId{issuer}, sim::Body{c}}, 7};
    auto bytes = sim::encode_turn_entry(in);
    REQUIRE(bytes.ok());
    auto out = sim::decode_turn_entry(bytes.value());
    REQUIRE(out.ok());
    CHECK(out.value() == in);
    (void)w.drain_events();
    Sent s;
    s.r = w.apply(out.value().command);
    s.events = w.drain_events();
    return s;
}

/// A success: the status, one record naming the warehouse and the amount, no host event.
void expect_ok(const Sent& s, sim::BuildingId wh, std::uint64_t amount) {
    CHECK(s.r.status == sim::Reject::None);
    REQUIRE(s.r.records.size() == 1);
    CHECK(s.r.records[0].status == sim::Reject::None);
    CHECK(s.r.records[0].building == wh);
    CHECK(s.r.records[0].transferred_88 == amount);
    CHECK(s.events.empty());
}

/// A refusal: the reason, no record, exactly one CommandRejected host event carrying the reason,
/// and a world that is unchanged byte for byte (save, hash, PRNG, money, cargo, stock).
void expect_refused(sim::World& w, std::uint32_t issuer, const sim::UnloadCmd& c, sim::Reject why) {
    const Snap before = snap(w);
    const auto goods_before = totals(w);
    const Sent s = send(w, issuer, c);
    CHECK_MESSAGE(s.r.status == why,
                  "got " << sim::reject_name(s.r.status) << ", wanted " << sim::reject_name(why));
    CHECK(s.r.records.empty());
    REQUIRE(s.events.size() == 1);
    CHECK(s.events[0].kind == sim::SimEventKind::CommandRejected);
    CHECK(s.events[0].code == static_cast<std::uint32_t>(why));
    CHECK(s.events[0].player == sim::PlayerId{issuer});
    const Snap after = snap(w);
    CHECK(after.save == before.save);
    CHECK(after.hash == before.hash);
    CHECK(after.cursor == before.cursor);
    CHECK(after.money == before.money);
    CHECK(after.holds == before.holds);
    CHECK(after.stocks == before.stocks);
    CHECK(totals(w) == goods_before);
}

/// An accepted command moves exactly `amount` from the hold to the store and conserves every goods
/// (checked in the wide accumulator); other holds, other goods and money are untouched.
Sent unload_ok(sim::World& w, std::uint32_t issuer, const sim::UnloadCmd& c, std::uint64_t amount,
               std::uint32_t city_no = 0) {
    const Snap before = snap(w);
    const auto goods_before = totals(w);
    const sim::Figure& s0 = ship(w, c.ship.v);
    const sim::ShipHold hold_before = s0.holds[c.hold];
    const std::uint32_t stock_before = city(w, city_no).stock.stock[c.goods];
    const Sent s = send(w, issuer, c);
    expect_ok(s, c.warehouse, amount);
    const sim::Figure& s1 = ship(w, c.ship.v);
    // The debit equals the credit equals the report.
    CHECK(hold_before.amount_88 - s1.holds[c.hold].amount_88 == amount);
    CHECK(city(w, city_no).stock.stock[c.goods] - stock_before == amount);
    if (s1.holds[c.hold].amount_88 == 0) {
        CHECK(s1.holds[c.hold].goods == 0); // the emptied-hold normal form
    } else {
        CHECK(s1.holds[c.hold].goods == hold_before.goods);
    }
    // Nothing else moved.
    const Snap after = snap(w);
    CHECK(after.money == before.money);
    CHECK(after.cursor == before.cursor);
    std::size_t k = 0;
    for (const std::optional<sim::Figure>& f : w.figures().list.slots) {
        if (!f.has_value()) {
            continue;
        }
        for (std::size_t h = 0; h < f->holds.size(); ++h) {
            if (!(f->id == c.ship && h == c.hold)) {
                CHECK(f->holds[h] == before.holds[k][h]);
            }
        }
        ++k;
    }
    for (std::size_t g = 0; g < city(w, city_no).stock.stock.size(); ++g) {
        if (g != c.goods) {
            CHECK(city(w, city_no).stock.stock[g] == before.stocks[city_no][g]);
        }
    }
    CHECK(totals(w) == goods_before); // conservation, per goods, in a wide accumulator
    CHECK(w.check_invariants().empty());
    return s;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// B1 and the predicates, one at a time
// ---------------------------------------------------------------------------------------------

TEST_CASE("unload B1: the positive control moves wood 38 t and the world agrees on both sides") {
    sim::World w = make(Setup{});
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    CHECK(city(w, 0).stock.stock[kWood] == 0);
    const Snap before = snap(w);
    unload_ok(w, 0, cmd(1, wh, 0, kWood, 38), 38 * kT);
    // 9728 raw units; hold 0 is emptied (goods id cleared); the other holds and goods are as built.
    CHECK(ship(w).holds[0] == sim::ShipHold{0, 0});
    CHECK(ship(w).holds[1] == sim::ShipHold{kFood, 2560});
    CHECK(ship(w).holds[2] == sim::ShipHold{kTools, 11520});
    CHECK(ship(w).holds[3] == sim::ShipHold{kTools, 7680});
    CHECK(city(w, 0).stock.stock[kWood] == 9728);
    CHECK(city(w, 0).stock.stock[kTools] == 0);
    // Gold, PRNG: unchanged. The crew and the ownership are untouched.
    CHECK(snap(w).money == before.money);
    CHECK(w.rng().cursor() == before.cursor);
    CHECK(ship(w).owner == sim::PlayerId{0});
    CHECK(city(w, 0).owner == sim::PlayerId{0});
    CHECK(w.check_invariants().empty());
}

TEST_CASE("unload predicates: one predicate flipped at a time each gives its own reason") {
    // The control is B1: every refusal below differs from it in exactly one fact.
    {
        sim::World w = make(Setup{});
        unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 38), 9728);
    }
    struct Row {
        const char* name;
        sim::Reject want;
        std::uint32_t issuer;
        std::function<void(Setup&)> world;
        std::function<sim::UnloadCmd(const sim::World&)> command;
    };
    const auto base = [](const sim::World& w) {
        return cmd(1, first_of(w, 0, {1302, 9301}), 0, kWood, 38);
    };
    const auto with = [&](auto edit) {
        return [=](const sim::World& w) {
            sim::UnloadCmd c = base(w);
            edit(c, w);
            return c;
        };
    };
    const std::vector<Row> rows = {
        {"B2 issuer is not the ship owner", sim::Reject::NotShipOwner, 1, [](Setup&) {}, base},
        {"B2b ship owner is not the city owner", sim::Reject::NotCityOwner, 1,
         [](Setup& s) { s.ships[0].owner = 1; }, base},
        {"B3 unknown ship", sim::Reject::UnknownFigure, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) { c.ship = sim::FigureId{999}; })},
        {"B4 the embarked scout is not a ship", sim::Reject::NotAShip, 0,
         [](Setup& s) {
             ShipDef scout;
             scout.kind = 717;
             scout.parent = 0;
             scout.holds = {sim::ShipHold{kWood, 5 * kT}};
             s.ships.push_back(scout);
         },
         with([](sim::UnloadCmd& c, const sim::World&) { c.ship = sim::FigureId{2}; })},
        {"zero request", sim::Reject::BadValue, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) { c.tons = 0; })},
        {"B7 a workshop is not a warehouse", sim::Reject::NotAWarehouse, 0,
         [](Setup& s) { s.workshop = true; },
         with([](sim::UnloadCmd& c, const sim::World& w) { c.warehouse = bid(w, 0, 30, 20); })},
        {"a PRODTYPE the rules do not list is not a warehouse", sim::Reject::NotAWarehouse, 0,
         [](Setup& s) { s.rules.cargo.warehouse_prodtypes = {36}; }, base},
        {"B8 the rival's warehouse", sim::Reject::NotCityOwner, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World& w) { c.warehouse = bid(w, 1, 60, 10); })},
        {"unknown warehouse serial", sim::Reject::NoBuildingAtTile, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) {
             c.warehouse = sim::make_building_id(sim::IslandId{0}, 777);
         })},
        {"unknown island in the warehouse id", sim::Reject::NoBuildingAtTile, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) {
             c.warehouse = sim::make_building_id(sim::IslandId{9}, 1);
         })},
        {"B9 one tile too far east", sim::Reject::OutOfRange, 0, [](Setup& s) { s.wx = 16; }, base},
        {"distant ship", sim::Reject::OutOfRange, 0, [](Setup& s) { s.ships[0].ty = 25; }, base},
        {"B10 the warehouse is on another island", sim::Reject::OutOfRange, 0,
         [](Setup& s) { s.second_island = true; },
         with([](sim::UnloadCmd& c, const sim::World& w) { c.warehouse = bid(w, 2, 205, 5, 1); })},
        {"hold index equals the hold count", sim::Reject::BadIndex, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) { c.hold = 4; })},
        {"hold index 2^32 - 1", sim::Reject::BadIndex, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) { c.hold = 0xFFFFFFFFU; })},
        {"goods differ from the hold's", sim::Reject::HoldGoodsMismatch, 0, [](Setup&) {},
         with([](sim::UnloadCmd& c, const sim::World&) { c.goods = kTools; })},
        {"the hold is empty", sim::Reject::HoldGoodsMismatch, 0,
         [](Setup& s) { s.ships[0].holds[0] = sim::ShipHold{0, 0}; }, base},
        {"a hold with a goods id and amount 0", sim::Reject::HoldGoodsMismatch, 0,
         [](Setup& s) { s.ships[0].holds[0] = sim::ShipHold{kWood, 0}; }, base},
        {"goods 0 requested", sim::Reject::UnsupportedGoods, 0,
         [](Setup& s) { s.ships[0].holds[0] = sim::ShipHold{0, 0}; },
         with([](sim::UnloadCmd& c, const sim::World&) { c.goods = 0; })},
        {"goods 56 is outside the store", sim::Reject::UnsupportedGoods, 0,
         [](Setup& s) { s.ships[0].holds[0] = sim::ShipHold{56, 5000}; },
         with([](sim::UnloadCmd& c, const sim::World&) { c.goods = 56; })},
        {"the store is exactly full", sim::Reject::StockFull, 0,
         [](Setup& s) { s.stock[kWood] = 50 * kT; }, base},
        {"the store is over capacity", sim::Reject::StockFull, 0,
         [](Setup& s) { s.stock[kWood] = 50 * kT + 200; }, base},
        {"the store has capacity 0", sim::Reject::StockFull, 0, [](Setup& s) { s.capacity = 0; },
         base},
    };
    for (const Row& row : rows) {
        INFO(row.name);
        Setup s;
        row.world(s);
        sim::World w = make(s);
        expect_refused(w, row.issuer, row.command(w), row.want);
    }
}

TEST_CASE("unload B5 B6: a demolished or replaced warehouse is not the warehouse of the command") {
    Setup s;
    sim::World w = make(s);
    const sim::BuildingId old_id = bid(w, 0, 15, 10);
    const sim::UnloadCmd c = cmd(1, old_id, 0, kWood, 38);
    // Demolish through the public command (the owner, the tile).
    sim::DemolishCmd d;
    d.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    d.tiles = {{15, 10}};
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0}, d}).ok());
    REQUIRE(w.building_at(d.city, 15, 10) == nullptr);
    expect_refused(w, 0, c, sim::Reject::NoBuildingAtTile); // B5
    // B6: a new Kontor on the same tile has a new serial.
    const auto made =
        sim::WorldTestAccess::create(w, 0, 0, sim::PlacementRecord{kKontor, 15, 10, 0, 0, 0});
    REQUIRE(made.ok());
    const sim::BuildingId new_id = bid(w, 0, 15, 10);
    CHECK(new_id != old_id);
    expect_refused(w, 0, c, sim::Reject::NoBuildingAtTile); // the old command still refuses
    unload_ok(w, 0, cmd(1, new_id, 0, kWood, 38), 9728);    // the command naming the new one works
}

TEST_CASE("unload B12: the city must own a tile of the disc outline, not only the interior") {
    // W at (11, 9) 4 x 4: dx = 1, dy = 0, inside the footprint test; its footprint holds none of
    // the 22 outline tiles of the disc around (10, 10).
    const auto owner_with = [](std::vector<std::pair<int, int>> extra) {
        return [extra](int x, int y) {
            if (x >= 11 && x <= 14 && y >= 9 && y <= 12) {
                return 0U; // W's own footprint
            }
            for (const auto& [ex, ey] : extra) {
                if (ex == x && ey == y) {
                    return 0U;
                }
            }
            return 15U;
        };
    };
    Setup control;
    control.wx = 11;
    control.wy = 9;
    control.owner = owner_with({{15, 11}}); // one outline tile (dx 5, dy +1)
    {
        sim::World w = make(control);
        unload_ok(w, 0, cmd(1, bid(w, 0, 11, 9), 0, kWood, 38), 9728);
    }
    Setup interior = control;
    interior.owner = owner_with({{10, 10}, {11, 10}, {12, 11}, {9, 9}}); // interior tiles only
    sim::World w = make(interior);
    expect_refused(w, 0, cmd(1, bid(w, 0, 11, 9), 0, kWood, 38), sim::Reject::OutOfRange);
}

TEST_CASE("unload B11: a moving ship unloads at its position at the apply tick") {
    // The ship carries an order and sails east away from the dock; a command applied before it has
    // left the disc executes, one applied after is refused OutOfRange (no state test).
    auto reg = registry();
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : reg->types()) {
        b.add(t);
    }
    sim::FigureType nav = *reg->find_figure_type(1502);
    nav.id = 9507;
    nav.capacity_88 = 1000 * kT;
    nav.nav = sim::NavFacts{154618816U, 472446400U, 6000U, 4U, 2U, 0U, 1280U};
    for (const sim::FigureType& f : reg->figure_types()) {
        b.figure_type(f);
    }
    b.figure_type(nav);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    Setup s;
    s.types = built.value();
    s.ships[0].kind = 9507;
    s.ships[0].tx = 16;
    s.ships[0].ty = 38;
    s.wx = 15;
    s.wy = 30; // 4 x 4: y 30..33, the ship is five rows below (dy 5, dx 1 <= half[5] = 2)
    s.capacity = 4000 * kT;
    sim::World w = make(s);
    const sim::BuildingId wh = bid(w, 0, 15, 30);
    // Control: standing still at (16, 38).
    {
        sim::World still = make(s);
        unload_ok(still, 0, cmd(1, bid(still, 0, 15, 30), 0, kWood, 10), 10 * kT);
    }
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0},
                                 sim::MoveCmd{sim::FigureId{1}, 70 * 256 + 128, 38 * 256 + 128}})
                .ok());
    unload_ok(w, 0, cmd(1, wh, 0, kWood, 10), 10 * kT); // ordered but still in the disc
    sim::World far = make(s);
    REQUIRE(far.apply(sim::Command{sim::PlayerId{0},
                                   sim::MoveCmd{sim::FigureId{1}, 70 * 256 + 128, 38 * 256 + 128}})
                .ok());
    for (int i = 0; i < 1500; ++i) {
        far.advance(10);
        if ((ship(far).pos.x >> 8) > 25) {
            break;
        }
    }
    REQUIRE((ship(far).pos.x >> 8) > 25);
    expect_refused(far, 0, cmd(1, bid(far, 0, 15, 30), 0, kWood, 10), sim::Reject::OutOfRange);
}

// ---------------------------------------------------------------------------------------------
// The independent vectors, run through the engine
// ---------------------------------------------------------------------------------------------

namespace {

std::vector<std::string> split(const std::string& line) {
    std::istringstream is(line);
    std::vector<std::string> t;
    for (std::string x; is >> x;) {
        t.push_back(x);
    }
    return t;
}

sim::Reject reject_by_name(const std::string& name) {
    for (std::uint16_t v = 0; v < 64; ++v) {
        if (sim::reject_name(static_cast<sim::Reject>(v)) == name) {
            return static_cast<sim::Reject>(v);
        }
    }
    FAIL("no Reject called " << name);
    return sim::Reject::None;
}

struct VectorFile {
    struct Case {
        std::string name;
        std::vector<std::vector<std::string>> lines;
    };
    std::vector<Case> cases;
    std::vector<std::vector<std::string>> extra; // range / tile / cityset rows
};

VectorFile read_vectors() {
    VectorFile f;
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "cargo_vectors.txt");
    REQUIRE(in.good());
    bool in_case = false;
    for (std::string raw; std::getline(in, raw);) {
        raw = raw.substr(0, raw.find('#'));
        const std::vector<std::string> t = split(raw);
        if (t.empty()) {
            continue;
        }
        if (t[0] == "case") {
            f.cases.push_back({t[1], {}});
            in_case = true;
        } else if (t[0] == "range" || t[0] == "tile" || t[0] == "cityset") {
            f.extra.push_back(t);
            in_case = false;
        } else if (in_case) {
            f.cases.back().lines.push_back(t);
        }
    }
    return f;
}

std::size_t index_of(const std::vector<std::string>& v, const char* token) {
    return static_cast<std::size_t>(std::find(v.begin(), v.end(), token) - v.begin());
}

std::string holds_text(const std::vector<sim::ShipHold>& h) {
    std::string out;
    for (const sim::ShipHold& x : h) {
        out +=
            (out.empty() ? "" : " ") + std::to_string(x.goods) + ":" + std::to_string(x.amount_88);
    }
    return out;
}

} // namespace

TEST_CASE("unload vectors: every case of cargo_vectors.txt gives the same answers through the "
          "engine") {
    const VectorFile vf = read_vectors();
    CHECK(vf.cases.size() == 34);
    for (const VectorFile::Case& c : vf.cases) {
        INFO("case " << c.name);
        Setup s;
        std::map<std::string, std::uint64_t> ids; // ship label -> figure id
        s.ships.clear();
        std::map<std::string, std::vector<sim::ShipHold>> holds;
        std::vector<std::string> order;
        for (const auto& l : c.lines) {
            if (l[0] == "cap") {
                s.capacity = static_cast<std::uint32_t>(std::stoull(l[1]));
            } else if (l[0] == "stock") {
                s.stock[static_cast<sim::GoodsId>(std::stoul(l[1]))] =
                    static_cast<std::uint32_t>(std::stoull(l[2]));
            } else if (l[0] == "hold") {
                if (!holds.contains(l[1])) {
                    order.push_back(l[1]);
                }
                holds[l[1]].push_back(
                    sim::ShipHold{static_cast<sim::GoodsId>(std::stoul(l[2])), std::stoull(l[3])});
            } else if (l[0] == "goods_count") {
                REQUIRE(std::stoul(l[1]) == sim::kOriginalWarehouseGoods); // the default table
            }
        }
        for (const std::string& label : order) {
            ShipDef d;
            d.holds = holds[label];
            s.ships.push_back(d);
            ids[label] = s.ships.size();
        }
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const auto goods0 = totals(w);
        for (const auto& l : c.lines) {
            if (l[0] != "req") {
                continue;
            }
            const std::size_t arrow = index_of(l, "->");
            const sim::UnloadCmd u =
                cmd(ids[l[1]], wh, static_cast<std::uint32_t>(std::stoul(l[2])),
                    static_cast<sim::GoodsId>(std::stoul(l[3])),
                    static_cast<std::uint32_t>(std::stoul(l[4])));
            if (l[arrow + 1] == "ok") {
                (void)unload_ok(w, 0, u, std::stoull(l[arrow + 2]));
            } else {
                expect_refused(w, 0, u, reject_by_name(l[arrow + 2]));
            }
        }
        for (const auto& l : c.lines) {
            if (l[0] == "endhold") {
                std::string want;
                for (std::size_t i = 2; i < l.size(); ++i) {
                    want += (want.empty() ? "" : " ") + l[i];
                }
                CHECK(holds_text(ship(w, ids[l[1]]).holds) == want);
            } else if (l[0] == "endstock") {
                CHECK(city(w, 0).stock.stock[std::stoul(l[1])] == std::stoull(l[2]));
            }
        }
        CHECK(totals(w) == goods0);
    }
}

TEST_CASE("unload vectors: range, tile and city-set rows agree with the engine") {
    const VectorFile vf = read_vectors();
    int range_rows = 0, engine_rows = 0, tile_rows = 0, cityset_rows = 0;
    for (const auto& t : vf.extra) {
        INFO(t[0] << " " << t[1]);
        const std::size_t arrow = index_of(t, "->");
        if (t[0] == "range") {
            ++range_rows;
            const int tx = std::stoi(t[3]), ty = std::stoi(t[4]);
            const int x0 = std::stoi(t[6]), y0 = std::stoi(t[7]), w = std::stoi(t[8]),
                      h = std::stoi(t[9]);
            const bool accept = t[arrow + 1] == "accept";
            CHECK(sim::ShipCargo::footprint_in_range(tx, ty, x0, y0, w, h, sim::CargoRules{}) ==
                  accept);
            if (w == 4 && h == 4) { // the engine's Kontor is 4 x 4
                ++engine_rows;
                Setup s;
                s.ships[0].tx = tx;
                s.ships[0].ty = ty;
                s.wx = x0;
                s.wy = y0;
                s.owner = [](int, int) { return 0U; }; // the outline rule is satisfied everywhere
                sim::World world = make(s);
                const sim::UnloadCmd u = cmd(1, bid(world, 0, x0, y0), 0, kWood, 38);
                if (accept) {
                    (void)unload_ok(world, 0, u, 9728);
                } else {
                    expect_refused(world, 0, u, sim::Reject::OutOfRange);
                }
            }
        } else if (t[0] == "tile") {
            ++tile_rows;
            // A W at (16, 10) makes tile x = 10 out of range and tile x = 11 in range; a W at
            // (0, 6) makes tile y = 0 out of range (dy 6) and tile y = 1 in range.
            const std::int32_t qx = std::stoi(t[1]), qy = std::stoi(t[2]);
            const int tx = std::stoi(t[arrow + 1]), ty = std::stoi(t[arrow + 2]);
            Setup s;
            s.ships[0].raw_x = qx;
            s.ships[0].raw_y = qy;
            s.owner = [](int, int) { return 0U; };
            s.wx = tx == 0 ? 0 : 16;
            s.wy = tx == 0 ? 6 : 10;
            sim::World world = make(s);
            const sim::UnloadCmd u = cmd(1, bid(world, 0, s.wx, s.wy), 0, kWood, 38);
            if (tx == 0) {
                expect_refused(world, 0, u, sim::Reject::OutOfRange); // tile (0, 0): dy 6
            } else if (tx == 10) {
                expect_refused(world, 0, u, sim::Reject::OutOfRange); // tile x 10: dx 6
            } else {
                (void)unload_ok(world, 0, u, 9728); // tile x 11: dx 5
            }
            CHECK(ty >= 0);
        } else {
            ++cityset_rows;
            const int tx = std::stoi(t[3]), ty = std::stoi(t[4]);
            std::vector<std::pair<int, int>> owned;
            for (std::size_t i = index_of(t, "owned") + 1; i < arrow; ++i) {
                const auto comma = t[i].find(',');
                owned.emplace_back(std::stoi(t[i].substr(0, comma)),
                                   std::stoi(t[i].substr(comma + 1)));
            }
            Setup s;
            s.ships[0].tx = tx;
            s.ships[0].ty = ty;
            s.wx = 11; // footprint x 11..14, y 9..12: none of the 22 outline tiles of (10, 10)
            s.wy = 9;
            s.owner = [owned](int x, int y) {
                if (x >= 11 && x <= 14 && y >= 9 && y <= 12) {
                    return 0U;
                }
                return std::find(owned.begin(), owned.end(), std::pair<int, int>{x, y}) !=
                               owned.end()
                           ? 0U
                           : 15U;
            };
            sim::World world = make(s);
            const sim::UnloadCmd u = cmd(1, bid(world, 0, 11, 9), 0, kWood, 38);
            if (t[arrow + 1] == "in") {
                (void)unload_ok(world, 0, u, 9728);
            } else {
                expect_refused(world, 0, u, sim::Reject::OutOfRange);
            }
        }
    }
    CHECK(range_rows == 15);
    CHECK(engine_rows >= 9);
    CHECK(tile_rows == 3);
    CHECK(cityset_rows == 8);
}

// ---------------------------------------------------------------------------------------------
// Sequences B13-B16, holds, fractions, bounds, contention
// ---------------------------------------------------------------------------------------------

TEST_CASE("unload B13 B14 B15: repeats, the opening after founding and the full store") {
    { // B13: two identical commands in one turn
        sim::World w = make(Setup{});
        const sim::UnloadCmd c = cmd(1, bid(w, 0, 15, 10), 0, kWood, 38);
        (void)unload_ok(w, 0, c, 9728);
        expect_refused(w, 0, c, sim::Reject::HoldGoodsMismatch); // the hold is 0:0 now
    }
    // B14: the opening: one command per hold, the 50 t store holds 25 t less of the last tools.
    sim::World w = make(Setup{});
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    (void)unload_ok(w, 0, cmd(1, wh, 0, kWood, 38), 9728);
    (void)unload_ok(w, 0, cmd(1, wh, 1, kFood, 10), 2560);
    (void)unload_ok(w, 0, cmd(1, wh, 2, kTools, 45), 11520);
    (void)unload_ok(w, 0, cmd(1, wh, 3, kTools, 30), 1280); // only 5 t fit: 12800 - 11520
    CHECK(city(w, 0).stock.stock[kWood] == 9728);
    CHECK(city(w, 0).stock.stock[kFood] == 2560);
    CHECK(city(w, 0).stock.stock[kTools] == 12800);
    CHECK(ship(w).holds[0] == sim::ShipHold{0, 0});
    CHECK(ship(w).holds[1] == sim::ShipHold{0, 0});
    CHECK(ship(w).holds[2] == sim::ShipHold{0, 0});
    CHECK(ship(w).holds[3] == sim::ShipHold{kTools, 6400}); // 25 t stay aboard
    // B15: the next turn, the same command again: the store is full.
    expect_refused(w, 0, cmd(1, wh, 3, kTools, 30), sim::Reject::StockFull);
    CHECK(ship(w).holds[3] == sim::ShipHold{kTools, 6400});
    CHECK(w.players().find(sim::PlayerId{0})->money == 5000);
}

TEST_CASE("unload B16: save and load around a transfer equal the uninterrupted world") {
    sim::World straight = make(Setup{});
    const sim::UnloadCmd c = cmd(1, bid(straight, 0, 15, 10), 0, kWood, 38);
    // Save and load immediately before the transfer.
    auto before = sim::World::load(straight.save(), registry());
    REQUIRE(before.ok());
    CHECK(before.value().save() == straight.save());
    CHECK(before.value().state_hash() == straight.state_hash());
    sim::World reloaded = std::move(before).value();
    (void)unload_ok(straight, 0, c, 9728);
    (void)unload_ok(reloaded, 0, c, 9728);
    CHECK(reloaded.save() == straight.save());
    // Save and load immediately after it, then the next command.
    auto after = sim::World::load(straight.save(), registry());
    REQUIRE(after.ok());
    sim::World second = std::move(after).value();
    CHECK(second.state_hash() == straight.state_hash());
    expect_refused(second, 0, c, sim::Reject::HoldGoodsMismatch);
    expect_refused(straight, 0, c, sim::Reject::HoldGoodsMismatch);
    CHECK(second.save() == straight.save());
    // A later command on both: the same answer and the same bytes.
    const sim::UnloadCmd food = cmd(1, bid(second, 0, 15, 10), 1, kFood, 3);
    (void)unload_ok(second, 0, food, 768);
    (void)unload_ok(straight, 0, food, 768);
    CHECK(second.save() == straight.save());
}

TEST_CASE("unload holds: duplicate goods are addressed by index and conserved at every step") {
    Setup s;
    s.ships[0].holds = {sim::ShipHold{kTools, 12800}, sim::ShipHold{kTools, 7680}};
    sim::World w = make(s);
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    (void)unload_ok(w, 0, cmd(1, wh, 1, kTools, 10), 2560);  // the second hold first
    (void)unload_ok(w, 0, cmd(1, wh, 0, kTools, 50), 10240); // only 40 t fit
    expect_refused(w, 0, cmd(1, wh, 1, kTools, 1), sim::Reject::StockFull);
    CHECK(holds_text(ship(w).holds) == "41:2560 41:5120");
    CHECK(city(w, 0).stock.stock[kTools] == 12800);
}

TEST_CASE("unload store: near-full is partial, over capacity is a refusal that lowers nothing") {
    { // 1000 units free: the transfer is the free space, the rest stays aboard
        Setup s;
        s.stock[kWood] = 12800 - 1000;
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 38), 1000);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 9728 - 1000});
        CHECK(city(w, 0).stock.stock[kWood] == 12800);
    }
    for (const std::uint32_t over : {200U, 255U, 256U, 300U, 5000U}) {
        // The original lowers such a store and credits the hold; this build refuses.
        Setup s;
        s.stock[kWood] = 12800 + over;
        sim::World w = make(s);
        expect_refused(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 38), sim::Reject::StockFull);
        CHECK(city(w, 0).stock.stock[kWood] == 12800 + over);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 9728});
    }
    { // The capacity applies per goods: a full tools store does not stop the wood.
        Setup s;
        s.stock[kTools] = 12800;
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 38), 9728);
    }
}

TEST_CASE("unload fractions: stored balances and remainders are exact, nothing is rounded away") {
    { // a fractional hold: 1.5 t, two one-ton requests
        Setup s;
        s.ships[0].holds = {sim::ShipHold{kWood, 384}};
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        (void)unload_ok(w, 0, cmd(1, wh, 0, kWood, 1), 256);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 128});
        (void)unload_ok(w, 0, cmd(1, wh, 0, kWood, 1), 128);
        CHECK(ship(w).holds[0] == sim::ShipHold{0, 0});
        CHECK(city(w, 0).stock.stock[kWood] == 384);
    }
    { // fractional free space: the store is filled exactly and the rest stays aboard
        Setup s;
        s.ships[0].holds = {sim::ShipHold{kWood, 768}};
        s.stock[kWood] = 12672; // 128 free
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 3), 128);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 640}); // the original would keep 512
        CHECK(city(w, 0).stock.stock[kWood] == 12800);
    }
    { // one unit of room
        Setup s;
        s.stock[kWood] = 12799;
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 38), 1);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 9727});
    }
    { // a fractional stored balance, a whole-ton request
        Setup s;
        s.stock[kWood] = 100;
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 38), 9728);
        CHECK(city(w, 0).stock.stock[kWood] == 9828);
    }
    { // legitimate repeats: the same one-ton command, three times, three transfers
        Setup s;
        s.ships[0].holds = {sim::ShipHold{kWood, 5 * kT}};
        sim::World w = make(s);
        const sim::UnloadCmd one = cmd(1, bid(w, 0, 15, 10), 0, kWood, 1);
        for (int i = 0; i < 3; ++i) {
            (void)unload_ok(w, 0, one, 256);
        }
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 2 * kT});
    }
}

TEST_CASE("unload bounds: quantities near the u32 store and the u64 hold limits") {
    { // capacity 2^32 - 1, 100 units free, a 1000 unit hold: credit 100, hold 900
        Setup s;
        s.capacity = 0xFFFFFFFFU;
        s.stock[kWood] = 0xFFFFFFFFU - 100;
        s.ships[0].holds = {sim::ShipHold{kWood, 1000}};
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 1), 100);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 900});
        CHECK(city(w, 0).stock.stock[kWood] == 0xFFFFFFFFU);
        expect_refused(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 1), sim::Reject::StockFull);
    }
    { // a hold of 2^64 - 1 units: one ton leaves 2^64 - 257; the wide sums still agree
        Setup s;
        s.capacity = 0xFFFFFFFFU;
        s.ships[0].holds = {sim::ShipHold{kWood, ~std::uint64_t{0}}};
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 1), 256);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, ~std::uint64_t{0} - 256});
    }
    { // the request 2^32 - 1 tons (2^40 - 256 units) against a 2^41 unit hold: the room decides
        Setup s;
        s.capacity = 0xFFFFFFFFU;
        s.ships[0].holds = {sim::ShipHold{kWood, std::uint64_t{1} << 41}};
        sim::World w = make(s);
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 0xFFFFFFFFU), 0xFFFFFFFFU);
        CHECK(ship(w).holds[0].amount_88 == (std::uint64_t{1} << 41) - 0xFFFFFFFFU);
        CHECK(city(w, 0).stock.stock[kWood] == 0xFFFFFFFFU);
    }
    { // a request far above the hold takes the hold, and nothing more
        sim::World w = make(Setup{});
        (void)unload_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 1, kFood, 0xFFFFFFFFU), 2560);
    }
}

TEST_CASE("unload contention: two ships into one nearly full store in command order") {
    for (const bool swapped : {false, true}) {
        Setup s;
        s.stock[kWood] = 12800 - 1000; // room for 1000 units
        s.ships = {ShipDef{}, ShipDef{}};
        s.ships[0].holds = {sim::ShipHold{kWood, 5000}};
        s.ships[1].holds = {sim::ShipHold{kWood, 5000}};
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const std::uint64_t first = swapped ? 2 : 1, second = swapped ? 1 : 2;
        const auto start = totals(w);
        CHECK(start.at(kWood) == Wide{0, 11800 + 10000});
        (void)unload_ok(w, 0, cmd(first, wh, 0, kWood, 10), 1000);
        expect_refused(w, 0, cmd(second, wh, 0, kWood, 10), sim::Reject::StockFull);
        CHECK(ship(w, first).holds[0] == sim::ShipHold{kWood, 4000});
        CHECK(ship(w, second).holds[0] == sim::ShipHold{kWood, 5000}); // untouched, not erased
        CHECK(totals(w) == start);
    }
    { // three ships, room for 1.5 of them: 5000, 4000 (partial), then a refusal
        Setup s;
        s.capacity = 9000 + 1000; // the store ends at capacity
        s.ships = {ShipDef{}, ShipDef{}, ShipDef{}};
        for (ShipDef& d : s.ships) {
            d.holds = {sim::ShipHold{kWood, 5000}};
        }
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        (void)unload_ok(w, 0, cmd(1, wh, 0, kWood, 100), 5000);
        (void)unload_ok(w, 0, cmd(2, wh, 0, kWood, 100), 5000);
        expect_refused(w, 0, cmd(3, wh, 0, kWood, 100), sim::Reject::StockFull);
    }
}

TEST_CASE("unload units: the stock helpers and the pure rules") {
    sim::CityStock st;
    st.stock = {0, 90, 150};
    st.capacity = 100;
    CHECK(st.free_88(0) == 100);
    CHECK(st.free_88(1) == 10);
    CHECK(st.free_88(2) == 0); // above the capacity: no room, never negative room
    CHECK(st.free_88(3) == 0); // outside the table
    CHECK_FALSE(st.credit(1, 11));
    CHECK(st.stock[1] == 90);
    CHECK_FALSE(st.credit(2, 1));
    CHECK(st.stock[2] == 150); // never lowered
    CHECK_FALSE(st.credit(3, 1));
    CHECK_FALSE(st.credit(3, 0));
    CHECK(st.credit(1, 10));
    CHECK(st.stock[1] == 100);
    CHECK(st.credit(0, 0));
    CHECK(st.stock[0] == 0);
    st.capacity = 0xFFFFFFFFU;
    st.stock[0] = 0;
    CHECK(st.free_88(0) == 0xFFFFFFFFULL);
    CHECK_FALSE(st.credit(0, 0x100000000ULL));
    CHECK(st.credit(0, 0xFFFFFFFFULL));
    CHECK(st.stock[0] == 0xFFFFFFFFU);
    // the plan's amount
    CHECK(sim::ShipCargo::amount_88(0xFFFFFFFFU, ~std::uint64_t{0}, ~std::uint64_t{0}) ==
          0xFFFFFFFFULL * 256);
    CHECK(sim::ShipCargo::amount_88(1, 1000, 5000) == 256);
    CHECK(sim::ShipCargo::amount_88(10, 1000, 5000) == 1000);
    CHECK(sim::ShipCargo::amount_88(10, 5000, 1000) == 1000);
    // the rules table
    CHECK(sim::CargoRules{}.problem().empty());
    sim::CargoRules r;
    r.dock_half_widths = {};
    CHECK(r.problem().find("is empty") != std::string::npos);
    r.dock_half_widths = {4, 5};
    CHECK(r.problem().find("not the radius") != std::string::npos);
    r.dock_half_widths = {2, 2, 3};
    CHECK(r.problem().find("increases") != std::string::npos);
    r.dock_half_widths = {0};
    CHECK(r.problem().empty()); // radius 0: the footprint itself
    r.dock_half_widths = std::vector<std::uint32_t>(300, 299);
    CHECK(r.problem().find("more than 256") != std::string::npos);
    r = {};
    r.warehouse_prodtypes = {36, 35};
    CHECK(r.problem().find("ascending") != std::string::npos);
    r.warehouse_prodtypes = {0};
    CHECK(r.problem().find("non-zero") != std::string::npos);
    r.warehouse_prodtypes = {};
    CHECK(r.problem().empty());
    // Rules::validate names the owner and the field
    sim::Rules rules;
    rules.cargo.dock_half_widths = {3, 3};
    CHECK(rules.validate().find("CargoRules") != std::string::npos);
    CHECK(rules.validate().find("dock_half_widths") != std::string::npos);
}

// ---------------------------------------------------------------------------------------------
// Save and load around a session, a recorded session in a fresh process, divergence
// ---------------------------------------------------------------------------------------------

namespace {

/// The session of the committed fixture cargo_unload.log: the opening after founding, then the
/// repeat that finds the store full, a repeat of the first command, a refusal, an advance.
std::vector<sim::Command> session_commands(const sim::World& w) {
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    const sim::BuildingId rival = bid(w, 1, 60, 10);
    const auto c = [&](const sim::UnloadCmd& u) {
        return sim::Command{sim::PlayerId{0}, sim::Body{u}};
    };
    return {c(cmd(1, wh, 0, kWood, 38)),    // ok 9728
            c(cmd(1, wh, 1, kFood, 10)),    // ok 2560
            c(cmd(1, wh, 2, kTools, 45)),   // ok 11520
            c(cmd(1, wh, 3, kTools, 30)),   // ok 1280, 25 t stay aboard
            c(cmd(1, wh, 3, kTools, 30)),   // StockFull
            c(cmd(1, wh, 0, kWood, 38)),    // HoldGoodsMismatch (a repeat)
            c(cmd(1, rival, 1, kFood, 1))}; // NotCityOwner (visible, not permitted)
}

const std::vector<sim::Reject> kSessionStatus = {
    sim::Reject::None,        sim::Reject::None,      sim::Reject::None,
    sim::Reject::None,        sim::Reject::StockFull, sim::Reject::HoldGoodsMismatch,
    sim::Reject::NotCityOwner};
const std::vector<std::uint64_t> kSessionAmount = {9728, 2560, 11520, 1280, 0, 0, 0};

std::filesystem::path fixture_log_path(const char* name) {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / name;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

sim::ReplayLog record_session(std::vector<sim::Reject>* statuses = nullptr,
                              std::vector<std::uint64_t>* amounts = nullptr) {
    sim::World w = make(Setup{});
    auto started = sim::Recorder::start(w);
    REQUIRE(started.ok());
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    for (const sim::Command& c : session_commands(w)) {
        const sim::ApplyResult r = rec.submit(c);
        if (statuses != nullptr) {
            statuses->push_back(r.status);
        }
        if (amounts != nullptr) {
            amounts->push_back(r.records.empty() ? 0 : r.records[0].transferred_88);
        }
        rec.checkpoint();
    }
    rec.advance(5000);
    rec.checkpoint();
    return rec.log();
}

sim::ReplayOutcome replay_log(const sim::ReplayLog& log, sim::World& world_out) {
    REQUIRE(log.registry_digest == registry()->digest());
    auto loaded = sim::World::load(log.initial_save, registry(), {});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    world_out = std::move(loaded).value();
    auto outcome = sim::replay(world_out, log);
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    return outcome.value();
}

} // namespace

TEST_CASE("unload session: save and load between any two commands change no outcome") {
    sim::World straight = make(Setup{});
    const std::vector<sim::Command> cmds = session_commands(straight);
    std::vector<sim::Reject> status_a;
    std::vector<std::uint64_t> amount_a;
    std::vector<std::vector<std::uint8_t>> saves_a;
    for (const sim::Command& c : cmds) {
        const sim::ApplyResult r = straight.apply(c);
        status_a.push_back(r.status);
        amount_a.push_back(r.records.empty() ? 0 : r.records[0].transferred_88);
        saves_a.push_back(straight.save());
    }
    CHECK(status_a == kSessionStatus);
    CHECK(amount_a == kSessionAmount);
    // The same session, saved and reloaded before every command.
    sim::World w = make(Setup{});
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        auto again = sim::World::load(w.save(), registry());
        REQUIRE(again.ok());
        w = std::move(again).value();
        const sim::ApplyResult r = w.apply(cmds[i]);
        CHECK(r.status == status_a[i]);
        CHECK((r.records.empty() ? 0 : r.records[0].transferred_88) == amount_a[i]);
        CHECK(w.save() == saves_a[i]);
    }
}

TEST_CASE("unload session: a recorded transfer replays, a changed amount or target diverges, and "
          "retries create nothing") {
    std::vector<sim::Reject> statuses;
    std::vector<std::uint64_t> amounts;
    const sim::ReplayLog log = record_session(&statuses, &amounts);
    CHECK(statuses == kSessionStatus);
    CHECK(amounts == kSessionAmount);
    auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    // The wrong log: the first command asks 10 t instead of 38. Its outcome differs, so the first
    // checkpoint after it differs.
    sim::ReplayLog wrong = log;
    bool changed = false;
    for (sim::ReplayEntry& e : wrong.entries) {
        for (sim::Command& c : e.commands) {
            if (!changed && std::holds_alternative<sim::UnloadCmd>(c.body)) {
                std::get<sim::UnloadCmd>(c.body).tons = 10;
                changed = true;
            }
        }
    }
    REQUIRE(changed);
    auto wrong_bytes = sim::write_log(wrong);
    REQUIRE(wrong_bytes.ok());
    if (anno::test::update_mode()) {
        for (const auto& [name, data] :
             {std::pair{"cargo_unload.log", &bytes.value()},
              std::pair{"cargo_unload_wrong.log", &wrong_bytes.value()}}) {
            const std::string err = anno::sim::test::write_evidence_file(
                fixture_log_path(name),
                std::string_view(reinterpret_cast<const char*>(data->data()), data->size()));
            REQUIRE_MESSAGE(err.empty(), err);
        }
        MESSAGE("fixtures cargo_unload.log and cargo_unload_wrong.log rewritten");
    }
    CHECK(read_file(fixture_log_path("cargo_unload.log")) == bytes.value());
    CHECK(read_file(fixture_log_path("cargo_unload_wrong.log")) == wrong_bytes.value());

    // Replaying the COMMITTED bytes: every checkpoint matches and the end state is the session's.
    const auto committed = sim::read_log(read_file(fixture_log_path("cargo_unload.log")));
    REQUIRE(committed.ok());
    sim::World good(1);
    const sim::ReplayOutcome ok = replay_log(committed.value(), good);
    CHECK(ok.reproduced());
    CHECK(ok.checkpoints_total == 9);
    CHECK(ok.checkpoints_matched == 9);
    CHECK(city(good, 0).stock.stock[kWood] == 9728);
    CHECK(city(good, 0).stock.stock[kTools] == 12800);
    CHECK(ship(good).holds[3] == sim::ShipHold{kTools, 6400});
    // Conservation over the whole replay: what the replay starts with is what it ends with.
    auto initial = sim::World::load(committed.value().initial_save, registry(), {});
    REQUIRE(initial.ok());
    // MAP-ROAD-CLOSE-1: the same valid recording with only the version word of its initial save set
    // to the previous stream (124) and the integrity seal recomputed is refused by VERSION, not as
    // checksum damage; the unchanged control loads (above).
    {
        std::vector<std::uint8_t> old_save = committed.value().initial_save;
        island_test::put32(old_save, 8, sim::kStateHashVersion - 1);
        island_test::reseal(old_save);
        const auto refused_old = sim::World::load(old_save, registry(), {});
        REQUIRE_FALSE(refused_old.ok());
        CHECK(refused_old.error().find(
                  "state stream version " + std::to_string(sim::kStateHashVersion - 1) +
                  " is not supported (this build reads " + std::to_string(sim::kStateHashVersion) +
                  ")") != std::string::npos);
        CHECK(refused_old.error().find("checksum") == std::string::npos);
    }
    CHECK(totals(initial.value()) == totals(good));
    // Retries create nothing: send every command of the session again to the end state.
    const Snap end = snap(good);
    const std::vector<sim::Reject> retry = {
        sim::Reject::HoldGoodsMismatch, sim::Reject::HoldGoodsMismatch,
        sim::Reject::HoldGoodsMismatch, sim::Reject::StockFull,
        sim::Reject::StockFull,         sim::Reject::HoldGoodsMismatch,
        sim::Reject::NotCityOwner};
    const std::vector<sim::Command> again = session_commands(good);
    for (std::size_t i = 0; i < again.size(); ++i) {
        const auto& u = std::get<sim::UnloadCmd>(again[i].body);
        expect_refused(good, 0, u, retry[i]);
    }
    CHECK(snap(good).save == end.save);

    // The committed wrong log: detected, and the first command moved less.
    const auto wrong_read = sim::read_log(read_file(fixture_log_path("cargo_unload_wrong.log")));
    REQUIRE(wrong_read.ok());
    sim::World bad(1);
    const sim::ReplayOutcome no = replay_log(wrong_read.value(), bad);
    CHECK_FALSE(no.reproduced());
    REQUIRE(no.diverged_entry.has_value());
    CHECK(no.checkpoints_matched < no.checkpoints_total);

    // A changed target (the rival's warehouse instead of the owner's) refuses and diverges.
    sim::ReplayLog elsewhere = log;
    changed = false;
    for (sim::ReplayEntry& e : elsewhere.entries) {
        for (sim::Command& c : e.commands) {
            if (!changed && std::holds_alternative<sim::UnloadCmd>(c.body)) {
                std::get<sim::UnloadCmd>(c.body).warehouse = bid(good, 1, 60, 10);
                changed = true;
            }
        }
    }
    REQUIRE(changed);
    sim::World moved(1);
    const sim::ReplayOutcome diverged = replay_log(elsewhere, moved);
    CHECK_FALSE(diverged.reproduced());
    CHECK(city(moved, 1).stock.stock[kWood] == 0); // nothing reached the rival either
}

// ---------------------------------------------------------------------------------------------
// Cargo and sailing, and data-defined content
// ---------------------------------------------------------------------------------------------

namespace {

/// The founding registry plus a navigating ship type `id` (BEWOHNTYPE 113) with `hold_a + hold_b`
/// declared holds and a 1000 t hold capacity, and any extra building types.
std::shared_ptr<const sim::TypeRegistry>
registry_with(std::uint32_t id, std::uint32_t hold_a, std::uint32_t hold_b,
              const std::vector<sim::BuildingType>& extra = {}) {
    auto reg = registry();
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : reg->types()) {
        b.add(t);
    }
    for (const sim::BuildingType& t : extra) {
        b.add(t);
    }
    for (const sim::FigureType& f : reg->figure_types()) {
        b.figure_type(f);
    }
    sim::FigureType nav = *reg->find_figure_type(1502);
    nav.id = id;
    nav.capacity_88 = 1000 * kT;
    nav.nav = sim::NavFacts{154618816U, 472446400U, 6000U, hold_a, hold_b, 0U, 1280U};
    b.figure_type(nav);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

/// The factors of the Move segments in the order they start, sampled every 10 ms: a new segment
/// is a Move sample after a non-Move sample or one whose remaining length grew.
struct SegmentWatch {
    std::vector<std::uint32_t> starts;
    bool in_move = false;
    std::uint64_t last_remaining = 0;
    std::uint32_t current = 0;
    void sample(const sim::Figure& f) {
        const sim::NavSegment& seg = f.nav->seg;
        if (seg.kind != sim::NavSegKind::Move) {
            in_move = false;
            return;
        }
        if (!in_move || seg.remaining_x500 > last_remaining) {
            starts.push_back(seg.factor_percent);
        }
        in_move = true;
        last_remaining = seg.remaining_x500;
        current = seg.factor_percent;
    }
};

} // namespace

TEST_CASE("unload sailing: the cargo factor is read at the next segment start, a running segment "
          "keeps its factor, the crew stays carried") {
    // A 3000 t wood hold on a ship of 4 + 2 holds of 1000 t: L = 100 - trunc(17 * 768000 /
    // (6 * 256000)) = 100 - 8 = 92 and, at full health, V = (35 + 65) * 92 / 100 = 92
    // (tools/ship_step_ref.py, speed_factor_vectors.txt). Unloaded, S = 0 and V = 100.
    Setup s;
    s.types = registry_with(9507, 4, 2);
    s.rules.nav.cargo_penalty_percent = 17;
    ShipDef sh;
    sh.kind = 9507;
    sh.tx = 16;
    sh.ty = 38;
    sh.holds = {sim::ShipHold{kWood, 3000 * kT}};
    ShipDef scout;
    scout.kind = 717;
    scout.tx = 16;
    scout.ty = 38;
    scout.parent = 0;
    scout.holds = {};
    s.ships = {sh, scout};
    s.wx = 15;
    s.wy = 30;
    s.capacity = 4000 * kT;
    const sim::MoveCmd go{sim::FigureId{1}, 70 * 256 + 128, 38 * 256 + 128};
    sim::World loaded = make(s);   // keeps its cargo
    sim::World unloaded = make(s); // unloads while the first segment is running
    REQUIRE(loaded.apply(sim::Command{sim::PlayerId{0}, go}).ok());
    REQUIRE(unloaded.apply(sim::Command{sim::PlayerId{0}, go}).ok());
    SegmentWatch a, b;
    bool done = false;
    std::uint32_t factor_at_unload = 0;
    for (int i = 0; i < 6000 && a.starts.size() < 8; ++i) {
        loaded.advance(10);
        unloaded.advance(10);
        a.sample(ship(loaded));
        b.sample(ship(unloaded));
        if (!done && !b.starts.empty()) {
            // The first movement segment is running with the cargo factor.
            CHECK(b.starts[0] == 92);
            const sim::BuildingId wh = bid(unloaded, 0, 15, 30);
            const sim::FigureId crew_parent = ship(unloaded, 2).parent;
            (void)unload_ok(unloaded, 0, cmd(1, wh, 0, kWood, 3000), 3000 * kT);
            factor_at_unload = unloaded.figures().list.slots[0]->nav->seg.factor_percent;
            CHECK(factor_at_unload == 92); // the running segment is untouched
            CHECK(ship(unloaded).holds[0] == sim::ShipHold{0, 0});
            CHECK(ship(unloaded, 2).parent == crew_parent); // the scout is still carried
            CHECK(ship(unloaded, 2).parent == sim::FigureId{1});
            done = true;
            b.starts.resize(1); // forget nothing after this point
        }
    }
    REQUIRE(done);
    // The ship with its cargo sails every segment at 92.
    REQUIRE(a.starts.size() >= 6);
    for (const std::uint32_t f : a.starts) {
        CHECK(f == 92);
    }
    // The unloaded ship's first segment ran at 92 to its end (checked above at the unload and by
    // its sampling), every later one starts at 100.
    REQUIRE(b.starts.size() >= 4);
    CHECK(b.starts[0] == 92);
    for (std::size_t i = 1; i < b.starts.size(); ++i) {
        CHECK(b.starts[i] == 100);
    }
    CHECK(ship(unloaded, 2).parent == sim::FigureId{1}); // still carried after the voyage so far
    CHECK(unloaded.check_invariants().empty());
}

TEST_CASE("unload data: another owner and a data-defined ship and warehouse type need no engine "
          "edit") {
    // Player 1 owns city 1; its ship is a type of the registry with three declared holds although
    // the figure carries five; its warehouse is a data-defined type with PRODTYPE 36 (3 x 3) that
    // Rules::cargo.warehouse_prodtypes lists ({35, 36}). A PRODTYPE 34 market is not a warehouse.
    auto reg = registry();
    sim::BuildingType wh36 = *reg->find(kKontor);
    wh36.id = sim::TypeId{9701};
    wh36.prodtype = 36;
    wh36.width = wh36.height = 3;
    sim::BuildingType market34 = *reg->find(kKontor);
    market34.id = sim::TypeId{9702};
    market34.prodtype = 34;
    Setup s;
    s.types = registry_with(9601, 2, 1, {wh36, market34});
    s.rules.cargo.warehouse_prodtypes = {35, 36};
    s.rival_type = sim::TypeId{9701};
    s.rival_x = 60;
    s.rival_y = 10;
    ShipDef p1;
    p1.kind = 9601;
    p1.owner = 1;
    p1.tx = 56;
    p1.ty = 10;
    p1.holds = {sim::ShipHold{kWood, 20 * kT}, sim::ShipHold{kFood, 7 * kT},
                sim::ShipHold{kTools, 9 * kT}, sim::ShipHold{kTools, 9 * kT},
                sim::ShipHold{kWood, 9 * kT}};
    s.ships = {ShipDef{}, p1}; // player 0's 1502 and player 1's 9601
    {
        sim::World w = make(s);
        const sim::BuildingId r = bid(w, 1, 60, 10);
        // Player 1 unloads into its own data-defined warehouse. Player 0's identical request for
        // player 1's ship is a wrong owner; player 0 into player 1's warehouse is a wrong city.
        (void)unload_ok(w, 1, cmd(2, r, 1, kFood, 3), 3 * kT, 1);
        CHECK(city(w, 1).stock.stock[kFood] == 3 * kT);
        CHECK(city(w, 0).stock.stock[kFood] == 0);
        expect_refused(w, 0, cmd(2, r, 0, kWood, 5), sim::Reject::NotShipOwner);
        expect_refused(w, 0, cmd(1, r, 0, kWood, 5), sim::Reject::NotCityOwner);
        // The cargo hold count of the type (hold_count_a = 2) limits the index, not the five
        // holds and not a + b = 3 (original 0x100ac476: valid indices end at a - 1). Index 2 is a
        // legal request in every other respect (its own tools, room, owner), so BadIndex is the
        // only refusal; index 1 (a - 1) is the legal control above.
        expect_refused(w, 1, cmd(2, r, 2, kTools, 4), sim::Reject::BadIndex);
        expect_refused(w, 1, cmd(2, r, 3, kTools, 4), sim::Reject::BadIndex);
        expect_refused(w, 1, cmd(2, r, 4, kWood, 4), sim::Reject::BadIndex);
        // Player 1's ship cannot use player 0's warehouse.
        expect_refused(w, 1, cmd(2, bid(w, 0, 15, 10), 0, kWood, 5), sim::Reject::NotCityOwner);
    }
    { // Player 0's own city may use the data-defined type too: no player 0 or type 1302 inside.
        Setup z = s;
        z.wtype = sim::TypeId{9701};
        sim::World w = make(z);
        (void)unload_ok(w, 0, cmd(1, first_of(w, 0, {9701}), 0, kWood, 38), 9728);
    }
    { // A PRODTYPE 34 market is no warehouse while the rules list 35 and 36.
        Setup z = s;
        z.wtype = sim::TypeId{9702};
        sim::World w = make(z);
        expect_refused(w, 0, cmd(1, first_of(w, 0, {9702}), 0, kWood, 38),
                       sim::Reject::NotAWarehouse);
    }
    { // The rules list is the data: a world whose list drops 36 refuses the type.
        Setup z = s;
        z.rules.cargo.warehouse_prodtypes = {35};
        sim::World w = make(z);
        expect_refused(w, 1, cmd(2, bid(w, 1, 60, 10), 1, kFood, 3), sim::Reject::NotAWarehouse);
    }
}

/// MAP-CARGO-3 C1 (F-L1): the unload addresses only the first hold_count_a cargo holds; the second
/// group (hold_count_b) and the figure's further records are not transfer targets. Each row is a
/// ship of the registry type 9801 (a, b) carrying `holds` records; `legal` indices unload exactly
/// and conserve, every other index is a BadIndex that leaves the world byte-identical although the
/// request is legal in all other respects (owner, goods of that very hold, range, room).
TEST_CASE("unload hold bound: only the first hold_count_a holds, never a + b (F-L1)") {
    struct Row {
        const char* what;
        std::uint32_t a, b;
        std::size_t records;              // holds the figure carries
        std::vector<std::size_t> legal;   // indices below min(records, a)
        std::vector<std::size_t> refused; // indices at and beyond the bound, all within records + 1
    };
    const std::vector<Row> rows = {
        // The causal pair: a - 1 executes, a is refused while a + b would have allowed it.
        {"a=2 b=1, five records", 2, 1, 5, {0, 1}, {2, 3, 4, 5}},
        {"a=1 b=3, four records", 1, 3, 4, {0}, {1, 2, 3, 4}},
        // Extra records far beyond a + b.
        {"a=2 b=0, five records", 2, 0, 5, {0, 1}, {2, 3, 4}},
        // The carried holds are shorter than a: the figure caps the bound.
        {"a=4 b=2, two records", 4, 2, 2, {0, 1}, {2, 3, 4, 5, 6}},
        // a = 0 (valid for the registry): no cargo hold at all, even with b > 0 and records.
        {"a=0 b=2, three records", 0, 2, 3, {}, {0, 1, 2, 3}},
    };
    const sim::GoodsId goods_of[] = {kWood, kFood, kTools, kTools, kWood, kFood};
    for (const Row& row : rows) {
        INFO(row.what);
        Setup s;
        s.types = registry_with(9801, row.a, row.b);
        ShipDef sh;
        sh.kind = 9801;
        sh.holds.clear();
        for (std::size_t k = 0; k < row.records; ++k) {
            sh.holds.push_back(sim::ShipHold{goods_of[k], 20 * kT});
        }
        s.ships = {sh};
        s.capacity = 4000 * kT;
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        for (const std::size_t k : row.legal) {
            INFO("legal index " << k);
            const auto before = totals(w);
            (void)unload_ok(w, 0, cmd(1, wh, static_cast<std::uint32_t>(k), goods_of[k], 5),
                            5 * kT);
            CHECK(ship(w).holds[k].amount_88 == 15 * kT);
            CHECK(totals(w) == before);
        }
        for (const std::size_t k : row.refused) {
            INFO("refused index " << k);
            // A request for the goods that record k really carries (or wood past the records).
            const sim::GoodsId g = k < row.records ? goods_of[k] : kWood;
            expect_refused(w, 0, cmd(1, wh, static_cast<std::uint32_t>(k), g, 5),
                           sim::Reject::BadIndex);
        }
    }
}

TEST_CASE("unload hold bound: a type without navigation facts keeps the carried-holds fallback") {
    // Established adaptation (not an original rule): a registry type without NavFacts has no hold
    // count, so the carried records bound the index; the load refuses such a type (NoCargoFacts).
    Setup s; // the 1502 of registry() carries no NavFacts here, four holds
    sim::World w = make(s);
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    CHECK_FALSE(w.types().find_figure_type(1502)->nav.has_value());
    (void)unload_ok(w, 0, cmd(1, wh, 3, kTools, 5), 5 * kT);
    expect_refused(w, 0, cmd(1, wh, 4, kTools, 5), sim::Reject::BadIndex);
}

// ---------------------------------------------------------------------------------------------
// The conservation guard: written so that each of the two wrong behaviours fails ONE assertion
// here while the credit side and the reported amount of the control stay green.
// ---------------------------------------------------------------------------------------------

TEST_CASE(
    "unload conservation: ship plus city is invariant, debit equals credit equals the report") {
    Setup s;
    s.stock[kTools] = 11520; // 5 t of room for tools
    sim::World w = make(s);
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    struct Step {
        const char* what;
        sim::UnloadCmd c;
        std::uint64_t amount; // the hand-derived transfer
    };
    const std::vector<Step> steps = {
        // The positive control: a whole hold into an empty store, request >= hold.
        {"control: wood 38 t, whole hold", cmd(1, wh, 0, kWood, 38), 9728},
        {"request smaller than the hold", cmd(1, wh, 1, kFood, 4), 1024},
        {"request larger than the hold", cmd(1, wh, 1, kFood, 100), 1536},
        // The store takes 5 t of the 45 t tools hold: 1280 units, 40 t stay aboard.
        {"limited by the room", cmd(1, wh, 2, kTools, 45), 1280},
    };
    for (const Step& st : steps) {
        INFO(st.what);
        const auto before = totals(w);
        const sim::ShipHold hold_before = ship(w).holds[st.c.hold];
        const std::uint32_t stock_before = city(w, 0).stock.stock[st.c.goods];
        const Sent sent = send(w, 0, st.c);
        // The control: the report and the credit side.
        CHECK(sent.r.status == sim::Reject::None);
        REQUIRE(sent.r.records.size() == 1);
        CHECK(sent.r.records[0].transferred_88 == st.amount);
        CHECK(city(w, 0).stock.stock[st.c.goods] - stock_before == st.amount);
        // The debit side and the conservation: the two things a wrong consumer breaks.
        CHECK(hold_before.amount_88 - ship(w).holds[st.c.hold].amount_88 == st.amount);
        CHECK(totals(w) == before);
    }
    CHECK(ship(w).holds[2] == sim::ShipHold{kTools, 45 * kT - 1280});
}
