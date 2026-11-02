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
constexpr sim::TypeId kKontorOff{9301}; // PRODTYPE 35, but not in Rules::founding_types
constexpr sim::TypeId kWeaver{9201};    // a workshop: a building that is not a warehouse

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
    c1.buildings.push_back({.type = kKontor, .x = 60, .y = 10});
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
        {"a Kontor-class type outside founding_types is not a warehouse",
         sim::Reject::NotAWarehouse, 0, [](Setup& s) { s.wtype = kKontorOff; }, base},
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
