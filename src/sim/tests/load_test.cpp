// LoadCmd through the public command path (MAP-CARGO-2 L3). Every command is encoded to its wire
// form, decoded again, stamped with the issuer and applied by World::apply; nothing calls the
// transfer helper directly. Contract: docs/sim/ship-cargo.md section 12 (validation order, units,
// vectors); evidence: docs/re/ship-cargo.md section 11. Expected values are hand-derived in the
// contract and in fixtures/synthetic/sim/cargo_load_vectors.txt (tools/cargo_ref.py), never read
// back from the code under test.
//
// The world: island 0, 80 x 40 tiles at (0, 0). City 0 (player 0) owns x < 50, city 1 (player 1)
// owns x >= 50. Player 0's Kontor W (type 1302, 4 x 4) is at (15, 10); player 1's Kontor R at
// (60, 10). Player 0's ship S (type 9510 of synthetic::make_cargo_types: BEWOHNTYPE 113, four cargo
// holds of 50 t) is at tile (10, 10) with four empty holds. City 0's store holds what a test says.
#include <algorithm>
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
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

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
constexpr std::uint32_t kCargoShip = 9510;
constexpr sim::TypeId kKontor{1302};
constexpr sim::TypeId kWeaver{9201}; // a workshop: a building that is not a warehouse

std::shared_ptr<const sim::TypeRegistry> registry() {
    auto r = syn::make_cargo_types();
    REQUIRE_MESSAGE(r.ok(), (r.ok() ? std::string() : r.error()));
    return r.value().registry;
}

/// The cargo registry plus a ship type `id` (BEWOHNTYPE 113) with the hold capacity `cap_88` and,
/// when `holds` is set, navigation facts with hold_count_a/_b = holds; without them the type has
/// no cargo facts. `extra` are additional building types.
std::shared_ptr<const sim::TypeRegistry>
registry_with(std::uint32_t id, std::uint32_t cap_88,
              std::optional<std::pair<std::uint32_t, std::uint32_t>> holds,
              const std::vector<sim::BuildingType>& extra = {}, bool strip_costs = false) {
    auto reg = registry();
    sim::TypeRegistry::Builder b;
    b.walk_tables(sim::synthetic_walk_tables());
    for (const sim::BuildingType& t : reg->types()) {
        sim::BuildingType copy = t;
        if (strip_costs) {
            copy.cost_goods.clear(); // a goods table smaller than the build goods (test only)
        }
        b.add(copy);
    }
    for (const sim::BuildingType& t : extra) {
        b.add(t);
    }
    for (const sim::FigureType& f : reg->figure_types()) {
        b.figure_type(f);
    }
    sim::FigureType t = *reg->find_figure_type(kCargoShip);
    t.id = id;
    t.capacity_88 = cap_88;
    if (holds.has_value()) {
        t.nav =
            sim::NavFacts{154618816U, 472446400U, 6000U, holds->first, holds->second, 0U, 1280U};
    } else {
        t.nav.reset();
    }
    b.figure_type(t);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

struct ShipDef {
    std::uint32_t kind = kCargoShip;
    std::uint32_t owner = 0;
    int tx = 10, ty = 10;
    std::optional<std::uint32_t> parent;
    std::vector<sim::ShipHold> holds = std::vector<sim::ShipHold>(4);
    std::optional<std::int32_t> raw_x, raw_y; // a Q24.8 position instead of the tile centre
};

struct Setup {
    std::vector<ShipDef> ships = {ShipDef{}};
    std::map<sim::GoodsId, std::uint32_t> stock;  // city 0
    std::map<sim::GoodsId, std::uint32_t> stock1; // city 1
    std::optional<std::uint32_t> capacity;        // city 0's store capacity per goods
    std::uint32_t goods_count = sim::kOriginalWarehouseGoods;
    sim::TypeId wtype = kKontor; // city 0's warehouse
    int wx = 15, wy = 10;
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
    const auto stock_of = [&s](const std::map<sim::GoodsId, std::uint32_t>& m) {
        std::vector<std::uint32_t> v(s.goods_count, 0);
        for (const auto& [g, u] : m) {
            v[g] = u;
        }
        return v;
    };
    sim::ScenarioSpec::CitySpec c0 = syn::plain_city(0, 0);
    c0.stock = stock_of(s.stock);
    c0.capacity = s.capacity;
    c0.buildings.push_back({.type = s.wtype, .x = s.wx, .y = s.wy});
    if (s.workshop) {
        c0.buildings.push_back({.type = kWeaver, .x = 30, .y = 20});
    }
    is.cities.push_back(c0);
    sim::ScenarioSpec::CitySpec c1 = syn::plain_city(1, 1);
    c1.stock = stock_of(s.stock1);
    c1.buildings.push_back({.type = s.rival_type, .x = s.rival_x, .y = s.rival_y});
    is.cities.push_back(c1);
    spec.islands.push_back(std::move(is));
    if (s.second_island) {
        sim::ScenarioSpec::IslandSpec is2 = syn::plain_island(1, 200, 0, 20, 20);
        is2.owner_runs = {{2, 20U * 20U}};
        sim::ScenarioSpec::CitySpec c2 = syn::plain_city(2, 0);
        c2.stock = stock_of({});
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
    sim::SimConfig cfg = s.cfg;
    cfg.warehouse_goods = s.goods_count;
    auto r = sim::World::create(spec_of(s), s.types, cfg, s.rules);
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

sim::LoadCmd cmd(std::uint64_t ship_id, sim::BuildingId wh, std::uint32_t hold, sim::GoodsId goods,
                 std::uint32_t tons) {
    return sim::LoadCmd{sim::FigureId{ship_id}, wh, hold, goods, tons};
}
sim::UnloadCmd ucmd(std::uint64_t ship_id, sim::BuildingId wh, std::uint32_t hold,
                    sim::GoodsId goods, std::uint32_t tons) {
    return sim::UnloadCmd{sim::FigureId{ship_id}, wh, hold, goods, tons};
}

/// encode -> decode (the turn entry, host stamped) -> World::apply. The decoded body must equal the
/// sent one.
template <class C> Sent send(sim::World& w, std::uint32_t issuer, const C& c) {
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
template <class C>
void expect_refused(sim::World& w, std::uint32_t issuer, const C& c, sim::Reject why) {
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

/// An accepted load moves exactly `amount` from the city store into hold `resolved` of the ship
/// (the hold the plan chose, derived by hand in the test, not read back) and conserves every
/// goods in the wide accumulator; other holds, other goods, gold and the PRNG are untouched.
Sent load_ok(sim::World& w, std::uint32_t issuer, const sim::LoadCmd& c, std::uint64_t amount,
             std::size_t resolved, std::uint32_t city_no = 0) {
    const Snap before = snap(w);
    const auto goods_before = totals(w);
    const sim::ShipHold hold_before = ship(w, c.ship.v).holds[resolved];
    const std::uint32_t stock_before = city(w, city_no).stock.stock[c.goods];
    const Sent s = send(w, issuer, c);
    expect_ok(s, c.warehouse, amount);
    const sim::Figure& s1 = ship(w, c.ship.v);
    // The credit equals the debit equals the report.
    CHECK(s1.holds[resolved].amount_88 - hold_before.amount_88 == amount);
    CHECK(s1.holds[resolved].goods == c.goods);
    CHECK(stock_before - city(w, city_no).stock.stock[c.goods] == amount);
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
            if (!(f->id == c.ship && h == resolved)) {
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

/// An accepted unload (the CARGO-1 command) conserves too: used by the round-trip tests.
Sent unload_ok(sim::World& w, std::uint32_t issuer, const sim::UnloadCmd& c, std::uint64_t amount) {
    const auto goods_before = totals(w);
    const Sent s = send(w, issuer, c);
    expect_ok(s, c.warehouse, amount);
    CHECK(totals(w) == goods_before);
    return s;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// The positive control and the predicates, one at a time
// ---------------------------------------------------------------------------------------------

TEST_CASE("load B1: the positive control takes 5 t of wood into the first empty hold") {
    Setup s;
    s.stock[kWood] = 20 * kT;
    sim::World w = make(s);
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    const Snap before = snap(w);
    // Hold 0 is empty (goods 0, amount 0): the plan keeps it (the search finds it first). 1280
    // units leave the city (5120 - 1280 = 3840) and enter the hold, which gets the goods id.
    load_ok(w, 0, cmd(1, wh, 0, kWood, 5), 5 * kT, 0);
    CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 1280});
    CHECK(ship(w).holds[1] == sim::ShipHold{0, 0});
    CHECK(city(w, 0).stock.stock[kWood] == 3840);
    CHECK(snap(w).money == before.money);
    CHECK(w.rng().cursor() == before.cursor);
    CHECK(ship(w).owner == sim::PlayerId{0});
    CHECK(city(w, 0).owner == sim::PlayerId{0});
    CHECK(w.check_invariants().empty());
}

TEST_CASE("load predicates: one predicate flipped at a time each gives its own reason") {
    // The control is B1: every refusal below differs from it in exactly one fact.
    {
        Setup c;
        c.stock[kWood] = 20 * kT;
        sim::World w = make(c);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 5), 1280, 0);
    }
    struct Row {
        const char* name;
        sim::Reject want;
        std::uint32_t issuer;
        std::function<void(Setup&)> world;
        std::function<sim::LoadCmd(const sim::World&)> command;
    };
    const auto base = [](const sim::World& w) {
        return cmd(1, first_of(w, 0, {1302, 9301}), 0, kWood, 5);
    };
    const auto with = [&](auto edit) {
        return [=](const sim::World& w) {
            sim::LoadCmd c = base(w);
            edit(c, w);
            return c;
        };
    };
    static const auto other_goods_in_all_holds = [](Setup& s) {
        s.ships[0].holds = {sim::ShipHold{kFood, 100}, sim::ShipHold{kTools, 100},
                            sim::ShipHold{kTools, 100}, sim::ShipHold{kFood, 100}};
    };
    const std::vector<Row> rows = {
        {"B2 issuer is not the ship owner", sim::Reject::NotShipOwner, 1, [](Setup&) {}, base},
        {"B2b ship owner is not the city owner", sim::Reject::NotCityOwner, 1,
         [](Setup& s) { s.ships[0].owner = 1; }, base},
        {"B2c issuer is not a player", sim::Reject::UnknownIssuer, 9, [](Setup&) {}, base},
        {"B3 unknown ship", sim::Reject::UnknownFigure, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.ship = sim::FigureId{999}; })},
        {"B4 the embarked scout is not a ship", sim::Reject::NotAShip, 0,
         [](Setup& s) {
             ShipDef scout;
             scout.kind = 717;
             scout.parent = 0;
             scout.holds = {sim::ShipHold{kWood, 5 * kT}};
             s.ships.push_back(scout);
         },
         with([](sim::LoadCmd& c, const sim::World&) { c.ship = sim::FigureId{2}; })},
        {"zero request", sim::Reject::BadValue, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.tons = 0; })},
        {"B7 a workshop is not a warehouse", sim::Reject::NotAWarehouse, 0,
         [](Setup& s) { s.workshop = true; },
         with([](sim::LoadCmd& c, const sim::World& w) { c.warehouse = bid(w, 0, 30, 20); })},
        {"a PRODTYPE the rules do not list is not a warehouse", sim::Reject::NotAWarehouse, 0,
         [](Setup& s) { s.rules.cargo.warehouse_prodtypes = {36}; }, base},
        {"B8 the rival's warehouse", sim::Reject::NotCityOwner, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World& w) { c.warehouse = bid(w, 1, 60, 10); })},
        {"unknown warehouse serial", sim::Reject::NoBuildingAtTile, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) {
             c.warehouse = sim::make_building_id(sim::IslandId{0}, 777);
         })},
        {"unknown island in the warehouse id", sim::Reject::NoBuildingAtTile, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) {
             c.warehouse = sim::make_building_id(sim::IslandId{9}, 1);
         })},
        {"B9 one tile too far east", sim::Reject::OutOfRange, 0, [](Setup& s) { s.wx = 16; }, base},
        {"distant ship", sim::Reject::OutOfRange, 0, [](Setup& s) { s.ships[0].ty = 25; }, base},
        {"B10 the warehouse is on another island", sim::Reject::OutOfRange, 0,
         [](Setup& s) { s.second_island = true; },
         with([](sim::LoadCmd& c, const sim::World& w) { c.warehouse = bid(w, 2, 205, 5, 1); })},
        {"a ship type without cargo facts", sim::Reject::NoCargoFacts, 0,
         [](Setup& s) {
             s.types = registry_with(9512, 12800, std::nullopt);
             s.ships[0].kind = 9512;
         },
         base},
        {"the shipped 1502 of the founding set has no cargo facts either",
         sim::Reject::NoCargoFacts, 0, [](Setup& s) { s.ships[0].kind = 1502; }, base},
        {"hold index equals the hold count", sim::Reject::BadIndex, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.hold = 4; })},
        {"hold index 2^32 - 1", sim::Reject::BadIndex, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.hold = 0xFFFFFFFFU; })},
        {"the bound is hold_count_a (2), not the four holds the figure carries",
         sim::Reject::BadIndex, 0,
         [](Setup& s) {
             s.types = registry_with(9513, 12800, std::pair{2U, 0U});
             s.ships[0].kind = 9513;
         },
         with([](sim::LoadCmd& c, const sim::World&) { c.hold = 2; })},
        {"hold_count_b (1) does not widen the bound", sim::Reject::BadIndex, 0,
         [](Setup& s) {
             s.types = registry_with(9514, 12800, std::pair{2U, 1U});
             s.ships[0].kind = 9514;
         },
         with([](sim::LoadCmd& c, const sim::World&) { c.hold = 2; })},
        {"the hold count exceeds the figure's holds: the figure's count bounds",
         sim::Reject::BadIndex, 0,
         [](Setup& s) {
             s.types = registry_with(9515, 12800, std::pair{9U, 0U});
             s.ships[0].kind = 9515;
         },
         with([](sim::LoadCmd& c, const sim::World&) { c.hold = 4; })},
        {"goods 0 requested", sim::Reject::UnsupportedGoods, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.goods = 0; })},
        {"goods 56 is outside the store", sim::Reject::UnsupportedGoods, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.goods = 56; })},
        {"goods 2^32 - 1 is outside the store", sim::Reject::UnsupportedGoods, 0, [](Setup&) {},
         with([](sim::LoadCmd& c, const sim::World&) { c.goods = 0xFFFFFFFFU; })},
        {"the store has no wood", sim::Reject::StockShort, 0, [](Setup& s) { s.stock.clear(); },
         base},
        {"the store holds 255 units: less than one whole ton", sim::Reject::StockShort, 0,
         [](Setup& s) { s.stock[kWood] = 255; }, base},
        {"every hold carries other goods", sim::Reject::HoldsFull, 0,
         [](Setup& s) {
             s.stock[kWood] = 20 * kT;
             other_goods_in_all_holds(s);
         },
         base},
        {"the only wood hold is full and the others carry other goods", sim::Reject::HoldsFull, 0,
         [](Setup& s) {
             s.stock[kWood] = 20 * kT;
             other_goods_in_all_holds(s);
             s.ships[0].holds[2] = sim::ShipHold{kWood, 50 * kT};
         },
         base},
        {"a type whose hold capacity is 0 carries nothing", sim::Reject::HoldsFull, 0,
         [](Setup& s) {
             s.stock[kWood] = 20 * kT;
             s.types = registry_with(9516, 0, std::pair{4U, 0U});
             s.ships[0].kind = 9516;
         },
         base},
        {"a hold of goods 0 with an amount is not empty", sim::Reject::HoldsFull, 0,
         [](Setup& s) {
             s.stock[kWood] = 20 * kT;
             s.ships[0].holds = {sim::ShipHold{0, 7}, sim::ShipHold{0, 7}, sim::ShipHold{0, 7},
                                 sim::ShipHold{0, 7}};
         },
         base},
    };
    for (const Row& row : rows) {
        INFO(row.name);
        Setup s;
        s.stock[kWood] = 20 * kT; // the control's stock, unless the row edits it
        row.world(s);
        sim::World w = make(s);
        expect_refused(w, row.issuer, row.command(w), row.want);
    }
}

TEST_CASE("load B5 B6: a demolished or replaced warehouse is not the warehouse of the command") {
    Setup s;
    s.stock[kWood] = 20 * kT;
    sim::World w = make(s);
    const sim::BuildingId old_id = bid(w, 0, 15, 10);
    const sim::LoadCmd c = cmd(1, old_id, 0, kWood, 5);
    sim::DemolishCmd d;
    d.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    d.tiles = {{15, 10}};
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0}, d}).ok());
    REQUIRE(w.building_at(d.city, 15, 10) == nullptr);
    expect_refused(w, 0, c, sim::Reject::NoBuildingAtTile); // B5
    // B6: a new Kontor on the same tile has a new serial; the old command still refuses.
    const auto made =
        sim::WorldTestAccess::create(w, 0, 0, sim::PlacementRecord{kKontor, 15, 10, 0, 0, 0});
    REQUIRE(made.ok());
    const sim::BuildingId new_id = bid(w, 0, 15, 10);
    CHECK(new_id != old_id);
    expect_refused(w, 0, c, sim::Reject::NoBuildingAtTile);
    load_ok(w, 0, cmd(1, new_id, 0, kWood, 5), 1280, 0); // the command naming the new one works
}

TEST_CASE("load B12: the city must own a tile of the disc outline, not only the interior") {
    const auto owner_with = [](std::vector<std::pair<int, int>> extra) {
        return [extra](int x, int y) {
            if (x >= 11 && x <= 14 && y >= 9 && y <= 12) {
                return 0U; // W's own footprint: none of the 22 outline tiles of the disc around
                           // (10, 10)
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
    control.stock[kWood] = 20 * kT;
    control.wx = 11;
    control.wy = 9;
    control.owner = owner_with({{15, 11}}); // one outline tile (dx 5, dy +1)
    {
        sim::World w = make(control);
        load_ok(w, 0, cmd(1, bid(w, 0, 11, 9), 0, kWood, 5), 1280, 0);
    }
    Setup interior = control;
    interior.owner = owner_with({{10, 10}, {11, 10}, {12, 11}, {9, 9}}); // interior tiles only
    sim::World w = make(interior);
    expect_refused(w, 0, cmd(1, bid(w, 0, 11, 9), 0, kWood, 5), sim::Reject::OutOfRange);
}

TEST_CASE("load B11: a moving ship loads at its position at the apply tick") {
    auto types = registry_with(9507, 1000 * kT, std::pair{4U, 2U});
    Setup s;
    s.types = types;
    s.ships[0].kind = 9507;
    s.ships[0].tx = 16;
    s.ships[0].ty = 38;
    s.wx = 15;
    s.wy = 30; // 4 x 4: y 30..33, the ship is five rows below (dy 5, dx 1 <= half[5] = 2)
    s.capacity = 4000 * kT;
    s.stock[kWood] = 100 * kT;
    const sim::MoveCmd go{sim::FigureId{1}, 70 * 256 + 128, 38 * 256 + 128};
    {
        sim::World still = make(s);
        load_ok(still, 0, cmd(1, bid(still, 0, 15, 30), 0, kWood, 10), 10 * kT, 0);
    }
    sim::World w = make(s);
    REQUIRE(w.apply(sim::Command{sim::PlayerId{0}, go}).ok());
    load_ok(w, 0, cmd(1, bid(w, 0, 15, 30), 0, kWood, 10), 10 * kT,
            0); // ordered, still in the disc
    sim::World far = make(s);
    REQUIRE(far.apply(sim::Command{sim::PlayerId{0}, go}).ok());
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

struct VectorCase {
    std::string name;
    std::vector<std::vector<std::string>> lines;
};

std::vector<VectorCase> read_load_vectors() {
    std::vector<VectorCase> cases;
    std::ifstream in(std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "cargo_load_vectors.txt");
    REQUIRE(in.good());
    for (std::string raw; std::getline(in, raw);) {
        raw = raw.substr(0, raw.find('#'));
        const std::vector<std::string> t = split(raw);
        if (t.empty()) {
            continue;
        }
        if (t[0] == "loadcase") {
            cases.push_back({t[1], {}});
        } else if (!cases.empty()) {
            cases.back().lines.push_back(t);
        }
    }
    return cases;
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

TEST_CASE("load vectors: every case of cargo_load_vectors.txt gives the same answers through the "
          "engine") {
    const std::vector<VectorCase> cases = read_load_vectors();
    CHECK(cases.size() == 43);
    std::size_t loads = 0, load_refusals = 0, unloads = 0, unload_refusals = 0;
    for (const VectorCase& c : cases) {
        INFO("case " << c.name);
        Setup s;
        std::uint32_t hold_cap = 12800;
        std::optional<std::uint32_t> nholds; // none = the first ship's hold count
        std::uint32_t nholds_b = 0;          // the second hold group, never a transfer bound
        bool no_facts = false;
        std::map<std::string, std::uint64_t> ids; // ship label -> figure id
        std::map<std::string, std::vector<sim::ShipHold>> holds;
        std::vector<std::string> order;
        for (const auto& l : c.lines) {
            if (l[0] == "holdcap") {
                hold_cap = static_cast<std::uint32_t>(std::stoull(l[1]));
            } else if (l[0] == "goods_count") {
                s.goods_count = static_cast<std::uint32_t>(std::stoul(l[1]));
            } else if (l[0] == "storecap") {
                s.capacity = static_cast<std::uint32_t>(std::stoull(l[1]));
            } else if (l[0] == "nholds") {
                if (l[1] == "none") {
                    no_facts = true;
                } else {
                    nholds = static_cast<std::uint32_t>(std::stoul(l[1]));
                }
            } else if (l[0] == "nholds_b") {
                nholds_b = static_cast<std::uint32_t>(std::stoul(l[1]));
            } else if (l[0] == "stock") {
                s.stock[static_cast<sim::GoodsId>(std::stoul(l[1]))] =
                    static_cast<std::uint32_t>(std::stoull(l[2]));
            } else if (l[0] == "hold") {
                if (!holds.contains(l[1])) {
                    order.push_back(l[1]);
                }
                holds[l[1]].push_back(
                    sim::ShipHold{static_cast<sim::GoodsId>(std::stoul(l[2])), std::stoull(l[3])});
            }
        }
        if (s.goods_count != sim::kOriginalWarehouseGoods) {
            // A smaller table: the founding goods (tools 0x29, wood 0x2a) must stay below it.
            s.cfg.build_goods_first = 1;
            s.cfg.build_goods_count = 1;
        }
        const std::uint32_t n0 =
            nholds.value_or(static_cast<std::uint32_t>(holds[order[0]].size()));
        s.types = registry_with(9520, hold_cap,
                                no_facts ? std::nullopt : std::optional{std::pair{n0, nholds_b}},
                                {}, s.goods_count != sim::kOriginalWarehouseGoods);
        s.ships.clear();
        for (const std::string& label : order) {
            ShipDef d;
            d.kind = 9520;
            d.holds = holds[label];
            s.ships.push_back(d);
            ids[label] = s.ships.size();
        }
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const auto goods0 = totals(w);
        for (const auto& l : c.lines) {
            if (l[0] != "load" && l[0] != "unload") {
                continue;
            }
            const std::size_t arrow = index_of(l, "->");
            const std::uint32_t hold = static_cast<std::uint32_t>(std::stoul(l[2]));
            const sim::GoodsId goods = static_cast<sim::GoodsId>(std::stoul(l[3]));
            const std::uint32_t tons = static_cast<std::uint32_t>(std::stoul(l[4]));
            const bool ok = l[arrow + 1] == "ok";
            if (l[0] == "load") {
                ++loads;
                const sim::LoadCmd u = cmd(ids[l[1]], wh, hold, goods, tons);
                if (ok) {
                    (void)load_ok(w, 0, u, std::stoull(l[arrow + 2]), std::stoull(l[arrow + 3]));
                } else {
                    ++load_refusals;
                    expect_refused(w, 0, u, reject_by_name(l[arrow + 2]));
                }
            } else {
                ++unloads;
                const sim::UnloadCmd u = ucmd(ids[l[1]], wh, hold, goods, tons);
                if (ok) {
                    (void)unload_ok(w, 0, u, std::stoull(l[arrow + 2]));
                } else {
                    ++unload_refusals;
                    expect_refused(w, 0, u, reject_by_name(l[arrow + 2]));
                }
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
        for (const auto& entry : s.stock) { // a stock with no endstock line is unchanged
            const sim::GoodsId gid = entry.first;
            const bool listed = std::any_of(c.lines.begin(), c.lines.end(), [gid](const auto& l) {
                return l[0] == "endstock" && std::stoul(l[1]) == gid;
            });
            if (!listed) {
                CHECK(city(w, 0).stock.stock[gid] == entry.second);
            }
        }
        CHECK(totals(w) == goods0);
    }
    // The file was run completely: nothing was skipped. (Counted at the time of writing.)
    CHECK(loads >= 42);
    CHECK(load_refusals >= 8);
    CHECK(unloads >= 4);
    MESSAGE("load vectors through the engine: " << cases.size() << " cases, " << loads << " loads ("
                                                << load_refusals << " refused), " << unloads
                                                << " unloads (" << unload_refusals << " refused)");
}

// ---------------------------------------------------------------------------------------------
// Mixed and empty holds, fractions, contention, bounds
// ---------------------------------------------------------------------------------------------

TEST_CASE("load holds: empty gets the goods id, same goods tops up, a hint is redirected") {
    { // an empty hold gets the goods id and the amount
        Setup s;
        s.stock[kWood] = 20 * kT;
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 2, kWood, 4), 4 * kT, 0);
        CHECK(holds_text(ship(w).holds) == "42:1024 0:0 0:0 0:0"); // the first empty hold, not 2
    }
    { // same goods tops up the named hold (>= 1 t free)
        Setup s;
        s.stock[kWood] = 20 * kT;
        s.ships[0].holds[2] = sim::ShipHold{kWood, 10 * kT};
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 2, kWood, 4), 4 * kT, 2);
        CHECK(holds_text(ship(w).holds) == "0:0 0:0 42:3584 0:0");
    }
    { // a different-goods hint hold is redirected to the first empty hold
        Setup s;
        s.stock[kWood] = 20 * kT;
        s.ships[0].holds = {sim::ShipHold{kFood, 5 * kT}, sim::ShipHold{kTools, 6 * kT},
                            sim::ShipHold{0, 0}, sim::ShipHold{0, 0}};
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 4), 4 * kT, 2);
        CHECK(holds_text(ship(w).holds) == "26:1280 41:1536 42:1024 0:0");
    }
    { // a hint that holds the goods but has less than one ton free yields to the search
        Setup s;
        s.stock[kWood] = 20 * kT;
        s.ships[0].holds[0] = sim::ShipHold{kWood, 12800 - 255};
        sim::World w = make(s);
        // search: k0 wood room 255 -> cand 0 (best 255), k1 empty ignored (cand set) -> hold 0 wins
        // with the single most room? the empty hold is only a candidate when no same-goods hold has
        // room, and hold 0 has room 255 > 0, so hold 0 takes min(1024, 255, 5120) = 255.
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 4), 255, 0);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 12800});
    }
    { // no suitable hold: HoldsFull, nothing changes
        Setup s;
        s.stock[kWood] = 20 * kT;
        s.ships[0].holds = {sim::ShipHold{kFood, 100}, sim::ShipHold{kFood, 100},
                            sim::ShipHold{kTools, 100}, sim::ShipHold{kTools, 100}};
        sim::World w = make(s);
        expect_refused(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 4), sim::Reject::HoldsFull);
    }
    { // duplicate same-goods holds: the named one with >= 1 t free wins even when not the roomiest
        Setup s;
        s.stock[kWood] = 30 * kT;
        s.ships[0].holds = {sim::ShipHold{kWood, 40 * kT}, sim::ShipHold{kWood, 10 * kT},
                            sim::ShipHold{kWood, 10 * kT}, sim::ShipHold{kFood, 1 * kT}};
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 2), 2 * kT, 0);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 42 * kT});
    }
    { // ... and when the hint is another goods, the most room wins, the lowest index on ties
        Setup s;
        s.stock[kWood] = 30 * kT;
        s.ships[0].holds = {sim::ShipHold{kWood, 40 * kT}, sim::ShipHold{kWood, 10 * kT},
                            sim::ShipHold{kWood, 10 * kT}, sim::ShipHold{kFood, 1 * kT}};
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        load_ok(w, 0, cmd(1, wh, 3, kWood, 2), 2 * kT, 1); // 1 and 2 tie (40 t free): index 1
        load_ok(w, 0, cmd(1, wh, 3, kWood, 2), 2 * kT, 2); // now 2 has the most room
        load_ok(w, 0, cmd(1, wh, 3, kWood, 2), 2 * kT, 1); // tie again: the lowest index
        CHECK(holds_text(ship(w).holds) == "42:10240 42:3584 42:3072 26:256");
    }
    { // a same-goods hold with room beats an empty hold (and the empty hint)
        Setup s;
        s.stock[kWood] = 30 * kT;
        s.ships[0].holds = {sim::ShipHold{0, 0}, sim::ShipHold{kWood, 10 * kT}, sim::ShipHold{0, 0},
                            sim::ShipHold{0, 0}};
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 2), 2 * kT, 1);
    }
    { // the same goods may fill the whole ship hold by hold; the fifth hold does not exist
        Setup s;
        s.stock[kWood] = 200 * kT;
        s.capacity = 400 * kT;
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        for (std::size_t h = 0; h < 4; ++h) {
            load_ok(w, 0, cmd(1, wh, 0, kWood, 50), 50 * kT, h);
        }
        expect_refused(w, 0, cmd(1, wh, 0, kWood, 50), sim::Reject::HoldsFull);
        CHECK(city(w, 0).stock.stock[kWood] == 0);
    }
}

TEST_CASE("load fractions: the store is read in whole tons, the remainder stays in the city") {
    { // 2.5 t stored (640), a 5 t request: 2 t move, 128 units stay
        Setup s;
        s.stock[kWood] = 640;
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 5), 512, 0);
        CHECK(city(w, 0).stock.stock[kWood] == 128);
        expect_refused(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 1), sim::Reject::StockShort);
    }
    { // a fractional hold room is moved as a fraction (the search, not the preferred path)
        Setup s;
        s.stock[kWood] = 10 * kT;
        s.ships[0].holds[0] = sim::ShipHold{kWood, 12800 - 128};
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 5), 128, 0);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 12800});
        CHECK(city(w, 0).stock.stock[kWood] == 10 * kT - 128);
    }
    { // a store of fractions only: nothing is rounded up to a whole ton
        Setup s;
        s.stock[kWood] = 255;
        sim::World w = make(s);
        expect_refused(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 1), sim::Reject::StockShort);
        CHECK(city(w, 0).stock.stock[kWood] == 255);
    }
    { // legitimate repeats: the same one-ton command, three times, three transfers
        Setup s;
        s.stock[kWood] = 5 * kT;
        sim::World w = make(s);
        const sim::LoadCmd one = cmd(1, bid(w, 0, 15, 10), 0, kWood, 1);
        for (int i = 0; i < 3; ++i) {
            (void)load_ok(w, 0, one, 256, 0);
        }
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 3 * kT});
        CHECK(city(w, 0).stock.stock[kWood] == 2 * kT);
    }
}

TEST_CASE("load contention: two ships against a limited store, in command order") {
    for (const bool swapped : {false, true}) {
        Setup s;
        s.stock[kWood] = 3 * kT + 100; // 868: three whole tons and a remainder
        s.ships = {ShipDef{}, ShipDef{}};
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const std::uint64_t first = swapped ? 2 : 1, second = swapped ? 1 : 2;
        const auto start = totals(w);
        CHECK(start.at(kWood) == Wide{0, 868});
        (void)load_ok(w, 0, cmd(first, wh, 0, kWood, 2), 512, 0);  // 2 t; 356 left
        (void)load_ok(w, 0, cmd(second, wh, 0, kWood, 2), 256, 0); // floor(356) = 256: 1 t
        expect_refused(w, 0, cmd(first, wh, 0, kWood, 2), sim::Reject::StockShort); // 100 left
        CHECK(ship(w, first).holds[0] == sim::ShipHold{kWood, 512});
        CHECK(ship(w, second).holds[0] == sim::ShipHold{kWood, 256});
        CHECK(city(w, 0).stock.stock[kWood] == 100);
        CHECK(totals(w) == start);
    }
}

TEST_CASE("load bounds: quantities near the u32 store and hold limits and the u64 hold amount") {
    { // capacity and stock 2^32 - 1: the hold has 300 units of room, a 2^32 - 1 t request
        Setup s;
        s.types = registry_with(9530, 0xFFFFFFFFU, std::pair{4U, 0U});
        s.capacity = 0xFFFFFFFFU;
        s.ships[0].kind = 9530;
        s.stock[kWood] = 0xFFFFFFFFU;
        s.ships[0].holds[0] = sim::ShipHold{kWood, 0xFFFFFFFFULL - 300};
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        // hint hold 0: room 300 >= 256 -> preferred; x = min(2^40 - 256, 300, 4294967040) = 300.
        load_ok(w, 0, cmd(1, wh, 0, kWood, 0xFFFFFFFFU), 300, 0);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 0xFFFFFFFFULL});
        CHECK(city(w, 0).stock.stock[kWood] == 0xFFFFFFFFU - 300);
        // hold 0 is full: the search takes the first empty hold 1; floor(4294966995 / 256) * 256 =
        // 4294966784 is the whole-ton stock, the capacity 4294967295 is larger, so the stock
        // decides.
        load_ok(w, 0, cmd(1, wh, 0, kWood, 0xFFFFFFFFU), 4294966784ULL, 1);
        CHECK(city(w, 0).stock.stock[kWood] == 211);
        CHECK(ship(w).holds[1] == sim::ShipHold{kWood, 4294966784ULL});
    }
    { // a hold amount above the capacity or at 2^64 - 1 has no room and is never touched
        Setup s;
        s.stock[kWood] = 20 * kT;
        s.ships[0].holds[0] = sim::ShipHold{kWood, ~std::uint64_t{0}};
        s.ships[0].holds[1] = sim::ShipHold{kWood, std::uint64_t{1} << 40};
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 5), 5 * kT, 2);
        CHECK(ship(w).holds[0] == sim::ShipHold{kWood, ~std::uint64_t{0}});
        CHECK(ship(w).holds[1] == sim::ShipHold{kWood, std::uint64_t{1} << 40});
        // all the remaining holds without room: HoldsFull, even with a 2^32 - 1 t request
        Setup t;
        t.stock[kWood] = 20 * kT;
        t.ships[0].holds = std::vector<sim::ShipHold>(4, sim::ShipHold{kWood, ~std::uint64_t{0}});
        sim::World full = make(t);
        expect_refused(full, 0, cmd(1, bid(full, 0, 15, 10), 0, kWood, 0xFFFFFFFFU),
                       sim::Reject::HoldsFull);
    }
    { // the request 2^32 - 1 tons (2^40 - 256 units) is limited by the room and the stock
        Setup s;
        s.stock[kWood] = 20 * kT;
        sim::World w = make(s);
        load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 0xFFFFFFFFU), 20 * kT, 0);
    }
}

TEST_CASE("load data: another owner and data-defined ship and warehouse types need no engine "
          "edit") {
    // Player 1 owns city 1; its ship type 9601 declares two cargo holds (hold_count_a 2, _b 1)
    // although the figure carries five, with a 20 t hold; its warehouse is a data-defined type with
    // PRODTYPE 36 (3 x 3) that Rules::cargo.warehouse_prodtypes lists ({35, 36}). A PRODTYPE 34
    // market is not a warehouse.
    auto reg = registry();
    sim::BuildingType wh36 = *reg->find(kKontor);
    wh36.id = sim::TypeId{9701};
    wh36.prodtype = 36;
    wh36.width = wh36.height = 3;
    sim::BuildingType market34 = *reg->find(kKontor);
    market34.id = sim::TypeId{9702};
    market34.prodtype = 34;
    Setup s;
    s.types = registry_with(9601, 20 * kT, std::pair{2U, 1U}, {wh36, market34});
    s.rules.cargo.warehouse_prodtypes = {35, 36};
    s.rival_type = sim::TypeId{9701};
    s.stock1[kFood] = 30 * kT;
    s.stock1[kWood] = 100 * kT;
    s.stock[kWood] = 100 * kT;
    ShipDef p1;
    p1.kind = 9601;
    p1.owner = 1;
    p1.tx = 56;
    p1.ty = 10;
    p1.holds = std::vector<sim::ShipHold>(5);
    ShipDef p0;
    p0.kind = 9601;
    s.ships = {p0, p1}; // player 0's and player 1's 9601
    {
        sim::World w = make(s);
        const sim::BuildingId r = bid(w, 1, 60, 10);
        // Player 1 loads from its own data-defined warehouse: the first empty hold (0), 3 t food.
        (void)load_ok(w, 1, cmd(2, r, 1, kFood, 3), 3 * kT, 0, 1);
        CHECK(city(w, 1).stock.stock[kFood] == 27 * kT);
        CHECK(city(w, 0).stock.stock[kFood] == 0);
        // 20 t holds: a 30 t wood request takes 20 t into the next empty hold (1), nothing above.
        (void)load_ok(w, 1, cmd(2, r, 0, kWood, 30), 20 * kT, 1, 1);
        // The declared hold count (2) limits the index and the search, not the five holds.
        expect_refused(w, 1, cmd(2, r, 2, kWood, 4), sim::Reject::BadIndex);
        // Both holds hold something: another goods, or a full wood hold, finds no hold of two.
        expect_refused(w, 1, cmd(2, r, 0, kTools, 4), sim::Reject::HoldsFull);
        expect_refused(w, 1, cmd(2, r, 0, kWood, 4), sim::Reject::HoldsFull);
        // Player 0 cannot use player 1's ship or warehouse; player 1 cannot use player 0's.
        expect_refused(w, 0, cmd(2, r, 0, kWood, 5), sim::Reject::NotShipOwner);
        expect_refused(w, 0, cmd(1, r, 0, kWood, 5), sim::Reject::NotCityOwner);
        expect_refused(w, 1, cmd(2, bid(w, 0, 15, 10), 0, kWood, 5), sim::Reject::NotCityOwner);
        // Player 0's own 9601 and its 1302 warehouse load without any player 0 constant in the way.
        (void)load_ok(w, 0, cmd(1, bid(w, 0, 15, 10), 1, kWood, 5), 5 * kT, 0);
    }
    { // Player 0's own city may use the data-defined type too.
        Setup z = s;
        z.wtype = sim::TypeId{9701};
        sim::World w = make(z);
        (void)load_ok(w, 0, cmd(1, first_of(w, 0, {9701}), 0, kWood, 5), 5 * kT, 0);
    }
    { // A PRODTYPE 34 market is no warehouse while the rules list 35 and 36.
        Setup z = s;
        z.wtype = sim::TypeId{9702};
        sim::World w = make(z);
        expect_refused(w, 0, cmd(1, first_of(w, 0, {9702}), 0, kWood, 5),
                       sim::Reject::NotAWarehouse);
    }
    { // The rules list is the data: a world whose list drops 36 refuses the type.
        Setup z = s;
        z.rules.cargo.warehouse_prodtypes = {35};
        sim::World w = make(z);
        expect_refused(w, 1, cmd(2, bid(w, 1, 60, 10), 0, kFood, 3), sim::Reject::NotAWarehouse);
    }
}

TEST_CASE("load facts: the shipped 1502 has a 50 t hold and four cargo holds (real registry)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const anno::sim_load::OriginalRegistryLoad loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    REQUIRE(loaded.types);
    const sim::FigureType* t = loaded.types->find_figure_type(1502);
    REQUIRE(t != nullptr);
    CHECK(t->capacity_88 == 12800);
    REQUIRE(t->nav.has_value());
    CHECK(t->nav->hold_count_a == 4);
    // The synthetic cargo ship carries the same facts, so its tests stand for the real ship.
    const auto cargo_types = registry();
    const sim::FigureType* synthetic = cargo_types->find_figure_type(kCargoShip);
    REQUIRE(synthetic != nullptr);
    CHECK(synthetic->capacity_88 == t->capacity_88);
    CHECK(synthetic->nav->hold_count_a == t->nav->hold_count_a);
}

// ---------------------------------------------------------------------------------------------
// Conservation: load then unload, unload then load, and the guard for the two wrong behaviours
// ---------------------------------------------------------------------------------------------

TEST_CASE("load round trip: load then unload, and unload then load, conserve and restore") {
    { // load then unload with no Advance: the layout comes back (a fractional store included)
        Setup s;
        s.stock[kWood] = 20 * kT + 100;
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const Snap start = snap(w);
        const auto goods = totals(w);
        (void)load_ok(w, 0, cmd(1, wh, 0, kWood, 7), 7 * kT, 0);
        CHECK(totals(w) == goods);
        (void)unload_ok(w, 0, ucmd(1, wh, 0, kWood, 7), 7 * kT);
        CHECK(totals(w) == goods);
        CHECK(snap(w).holds == start.holds);
        CHECK(snap(w).stocks == start.stocks);
        CHECK(snap(w).money == start.money);
        CHECK(snap(w).cursor == start.cursor);
        CHECK(city(w, 0).stock.stock[kWood] == 20 * kT + 100);
    }
    { // unload then load: a 38 t hold of wood into a store with a 100 unit fraction
        Setup s;
        s.ships[0].holds = {sim::ShipHold{kWood, 38 * kT}, sim::ShipHold{kFood, 10 * kT},
                            sim::ShipHold{}, sim::ShipHold{}};
        s.stock[kWood] = 100;
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const Snap start = snap(w);
        const auto goods = totals(w);
        (void)unload_ok(w, 0, ucmd(1, wh, 0, kWood, 38), 38 * kT);
        CHECK(city(w, 0).stock.stock[kWood] == 38 * kT + 100);
        // hold 0 is empty (0:0): the search finds it first; floor(9828) = 9728 moves back
        (void)load_ok(w, 0, cmd(1, wh, 0, kWood, 38), 38 * kT, 0);
        CHECK(totals(w) == goods);
        CHECK(snap(w).holds == start.holds);
        CHECK(snap(w).stocks == start.stocks);
    }
    { // unload into a smaller store leaves the rest aboard; loading it back conserves the sum
        Setup s;
        s.ships[0].holds = {sim::ShipHold{kTools, 30 * kT}, sim::ShipHold{}, sim::ShipHold{},
                            sim::ShipHold{}};
        s.stock[kTools] = 45 * kT;
        sim::World w = make(s);
        const sim::BuildingId wh = bid(w, 0, 15, 10);
        const auto goods = totals(w);
        (void)unload_ok(w, 0, ucmd(1, wh, 0, kTools, 30), 5 * kT); // only 5 t fit
        CHECK(ship(w).holds[0] == sim::ShipHold{kTools, 25 * kT});
        (void)load_ok(w, 0, cmd(1, wh, 0, kTools, 30), 25 * kT, 0); // 25 t is room; stock is 50 t
        CHECK(totals(w) == goods);
        CHECK(ship(w).holds[0] == sim::ShipHold{kTools, 50 * kT});
    }
}

TEST_CASE("load conservation: ship plus city is invariant, debit equals credit equals the report") {
    // Four hand-derived steps on a fresh world. The first one is limited by the hold's room, so a
    // debit of the REQUEST (instead of the credit) breaks the debit-side assertion at step one, and
    // a credit without a debit breaks it too; the report and the credit side stay green for both.
    Setup s;
    s.stock[kWood] = 30 * kT + 40;
    s.stock[kTools] = 2 * kT;
    s.ships[0].holds[1] = sim::ShipHold{kWood, 46 * kT}; // 4 t of room
    sim::World w = make(s);
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    struct Step {
        const char* what;
        sim::LoadCmd c;
        std::uint64_t amount; // the hand-derived transfer
        std::size_t hold;     // the hold that receives it
    };
    const std::vector<Step> steps = {
        // hint hold 1 holds wood and has 4 t (1024 units) free: preferred; request 10 t, room 4 t.
        {"limited by the hold's room", cmd(1, wh, 1, kWood, 10), 1024, 1},
        // hold 1 is full now: search; hold 0 (empty) is the candidate; floor(7656 - 1024 = 6696 /
        // 256)
        // = 26 t = 6656 available; request 7 t = 1792 -> 1792.
        {"whole request into the first empty hold", cmd(1, wh, 1, kWood, 7), 1792, 0},
        // hold 0 holds 1792 (7 t), 43 t free: preferred; stock 6696 - 1792 = 4904, floor = 4864;
        // the
        // request 40 t = 10240 is limited by the store's whole tons: 4864.
        {"limited by the store's whole tons", cmd(1, wh, 0, kWood, 40), 4864, 0},
        // tools: 2 t = 512 in the store, hold 2 is empty: 512 move (request 50 t), 0 remain.
        {"another goods, the store emptied", cmd(1, wh, 0, kTools, 50), 512, 2},
    };
    for (const Step& st : steps) {
        INFO(st.what);
        const auto before = totals(w);
        const sim::ShipHold hold_before = ship(w).holds[st.hold];
        const std::uint32_t stock_before = city(w, 0).stock.stock[st.c.goods];
        const Sent sent = send(w, 0, st.c);
        // The control: the report and the credit side.
        CHECK(sent.r.status == sim::Reject::None);
        REQUIRE(sent.r.records.size() == 1);
        CHECK(sent.r.records[0].transferred_88 == st.amount);
        CHECK(ship(w).holds[st.hold].amount_88 - hold_before.amount_88 == st.amount);
        CHECK(ship(w).holds[st.hold].goods == st.c.goods);
        // The debit side and the conservation: the two things a wrong producer breaks.
        CHECK(stock_before - city(w, 0).stock.stock[st.c.goods] == st.amount);
        CHECK(totals(w) == before);
    }
    CHECK(city(w, 0).stock.stock[kWood] == 40);
    CHECK(city(w, 0).stock.stock[kTools] == 0);
    CHECK(holds_text(ship(w).holds) == "42:6656 42:12800 41:512 0:0");
}

TEST_CASE("load control: a fully satisfied request moves exactly the request") {
    // The legal control of the guard above: here the request is the amount, so a debit by the
    // request equals the credit and only a missing debit shows.
    Setup s;
    s.stock[kWood] = 20 * kT;
    sim::World w = make(s);
    const auto before = totals(w);
    const Sent sent = send(w, 0, cmd(1, bid(w, 0, 15, 10), 0, kWood, 5));
    CHECK(sent.r.status == sim::Reject::None);
    REQUIRE(sent.r.records.size() == 1);
    CHECK(sent.r.records[0].transferred_88 == 1280);
    CHECK(ship(w).holds[0] == sim::ShipHold{kWood, 1280});  // the credit side
    CHECK(city(w, 0).stock.stock[kWood] == 20 * kT - 1280); // the debit side
    CHECK(totals(w) == before);
}

// ---------------------------------------------------------------------------------------------
// Save and load around a session, a recorded session in a fresh process, divergence
// ---------------------------------------------------------------------------------------------

namespace {

Setup session_setup() {
    Setup s;
    s.stock[kWood] = 20 * kT;  // 5120
    s.stock[kFood] = 30 * kT;  // 7680
    s.stock[kTools] = 50 * kT; // 12800: the store's capacity
    return s;
}

/// The session of the committed fixture cargo_load.log: loads into empty and same-goods holds, a
/// redirect, a stock refusal, a rival's warehouse refused, then an unload of what was loaded.
std::vector<sim::Command> session_commands(const sim::World& w) {
    const sim::BuildingId wh = bid(w, 0, 15, 10);
    const sim::BuildingId rival = bid(w, 1, 60, 10);
    const auto l = [&](const sim::LoadCmd& c) {
        return sim::Command{sim::PlayerId{0}, sim::Body{c}};
    };
    return {l(cmd(1, wh, 0, kWood, 10)),   // ok 2560 into the empty hold 0
            l(cmd(1, wh, 0, kWood, 100)),  // ok 2560: hold 0 tops up, the store is emptied
            l(cmd(1, wh, 0, kWood, 1)),    // StockShort
            l(cmd(1, wh, 1, kFood, 5)),    // ok 1280 into hold 1
            l(cmd(1, wh, 0, kTools, 60)),  // ok 12800: hold 0 has wood, redirected to hold 2
            l(cmd(1, wh, 2, kTools, 1)),   // StockShort (hold 2 is full, hold 3 would be taken)
            l(cmd(1, rival, 1, kFood, 1)), // NotCityOwner (visible, not permitted)
            sim::Command{sim::PlayerId{0},
                         sim::Body{ucmd(1, wh, 0, kWood, 20)}}}; // ok 5120 back into the store
}

const std::vector<sim::Reject> kSessionStatus = {
    sim::Reject::None, sim::Reject::None,       sim::Reject::StockShort,   sim::Reject::None,
    sim::Reject::None, sim::Reject::StockShort, sim::Reject::NotCityOwner, sim::Reject::None};
const std::vector<std::uint64_t> kSessionAmount = {2560, 2560, 0, 1280, 12800, 0, 0, 5120};

std::filesystem::path fixture_log_path(const char* name) {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "script1" / name;
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

sim::ReplayLog record_session(std::vector<sim::Reject>* statuses = nullptr,
                              std::vector<std::uint64_t>* amounts = nullptr) {
    sim::World w = make(session_setup());
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

TEST_CASE("load session: the hand-derived outcomes, and save/load before and after a load") {
    sim::World straight = make(session_setup());
    const std::vector<sim::Command> cmds = session_commands(straight);
    std::vector<sim::Reject> status_a;
    std::vector<std::uint64_t> amount_a;
    std::vector<std::vector<std::uint8_t>> saves_a;
    for (const sim::Command& c : cmds) {
        const auto before = totals(straight);
        const sim::ApplyResult r = straight.apply(c);
        status_a.push_back(r.status);
        amount_a.push_back(r.records.empty() ? 0 : r.records[0].transferred_88);
        saves_a.push_back(straight.save());
        CHECK(totals(straight) == before);
    }
    CHECK(status_a == kSessionStatus);
    CHECK(amount_a == kSessionAmount);
    CHECK(holds_text(ship(straight).holds) == "0:0 26:1280 41:12800 0:0");
    CHECK(city(straight, 0).stock.stock[kWood] == 5120);
    CHECK(city(straight, 0).stock.stock[kFood] == 6400);
    CHECK(city(straight, 0).stock.stock[kTools] == 0);
    // The same session, saved and reloaded before every command (and so after every one): the same
    // outcome and the same bytes.
    sim::World w = make(session_setup());
    for (std::size_t i = 0; i < cmds.size(); ++i) {
        auto again = sim::World::load(w.save(), registry());
        REQUIRE(again.ok());
        w = std::move(again).value();
        const sim::ApplyResult r = w.apply(cmds[i]);
        CHECK(r.status == status_a[i]);
        CHECK((r.records.empty() ? 0 : r.records[0].transferred_88) == amount_a[i]);
        CHECK(w.save() == saves_a[i]);
    }
    // Immediately before and after one load, then the next command (a refusal and an accept).
    sim::World a = make(session_setup());
    sim::World b = make(session_setup());
    const sim::LoadCmd first = std::get<sim::LoadCmd>(cmds[0].body);
    auto reloaded_a = sim::World::load(a.save(), registry());
    REQUIRE(reloaded_a.ok());
    a = std::move(reloaded_a).value();
    (void)load_ok(a, 0, first, 2560, 0);
    (void)load_ok(b, 0, first, 2560, 0);
    CHECK(a.save() == b.save());
    auto reloaded_b = sim::World::load(b.save(), registry());
    REQUIRE(reloaded_b.ok());
    b = std::move(reloaded_b).value();
    CHECK(b.state_hash() == a.state_hash());
    const sim::LoadCmd next = cmd(1, first.warehouse, 0, kWood, 100);
    (void)load_ok(a, 0, next, 2560, 0);
    (void)load_ok(b, 0, next, 2560, 0);
    expect_refused(a, 0, next, sim::Reject::StockShort);
    expect_refused(b, 0, next, sim::Reject::StockShort);
    CHECK(a.save() == b.save());
}

TEST_CASE("load session: a recorded load replays, a changed amount or target diverges, and "
          "retries create nothing") {
    std::vector<sim::Reject> statuses;
    std::vector<std::uint64_t> amounts;
    const sim::ReplayLog log = record_session(&statuses, &amounts);
    CHECK(statuses == kSessionStatus);
    CHECK(amounts == kSessionAmount);
    auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    // The wrong log: the first command asks 1 t instead of 10. Its outcome differs, so the first
    // checkpoint after it differs.
    sim::ReplayLog wrong = log;
    bool changed = false;
    for (sim::ReplayEntry& e : wrong.entries) {
        for (sim::Command& c : e.commands) {
            if (!changed && std::holds_alternative<sim::LoadCmd>(c.body)) {
                std::get<sim::LoadCmd>(c.body).tons = 1;
                changed = true;
            }
        }
    }
    REQUIRE(changed);
    auto wrong_bytes = sim::write_log(wrong);
    REQUIRE(wrong_bytes.ok());
    if (anno::test::update_mode()) {
        for (const auto& [name, data] : {std::pair{"cargo_load.log", &bytes.value()},
                                         std::pair{"cargo_load_wrong.log", &wrong_bytes.value()}}) {
            const std::string err = anno::sim::test::write_evidence_file(
                fixture_log_path(name),
                std::string_view(reinterpret_cast<const char*>(data->data()), data->size()));
            REQUIRE_MESSAGE(err.empty(), err);
        }
        MESSAGE("fixtures cargo_load.log and cargo_load_wrong.log rewritten");
    }
    CHECK(read_file(fixture_log_path("cargo_load.log")) == bytes.value());
    CHECK(read_file(fixture_log_path("cargo_load_wrong.log")) == wrong_bytes.value());

    // Replaying the COMMITTED bytes: every checkpoint matches and the end state is the session's.
    const auto committed = sim::read_log(read_file(fixture_log_path("cargo_load.log")));
    REQUIRE(committed.ok());
    sim::World good(1);
    const sim::ReplayOutcome ok = replay_log(committed.value(), good);
    CHECK(ok.reproduced());
    CHECK(ok.checkpoints_total == 10);
    CHECK(ok.checkpoints_matched == 10);
    CHECK(holds_text(ship(good).holds) == "0:0 26:1280 41:12800 0:0");
    CHECK(city(good, 0).stock.stock[kTools] == 0);
    CHECK(city(good, 0).stock.stock[kFood] == 6400);
    // Conservation over the whole replay: what the replay starts with is what it ends with.
    auto initial = sim::World::load(committed.value().initial_save, registry(), {});
    REQUIRE(initial.ok());
    CHECK(totals(initial.value()) == totals(good));
    // Retries create nothing: every command of the session sent again (twice) on the end state
    // moves goods between the hold and the store but never creates or destroys any, per goods.
    const auto end_totals = totals(good);
    for (int round = 0; round < 2; ++round) {
        for (const sim::Command& c : session_commands(good)) {
            (void)good.apply(c);
            CHECK(totals(good) == end_totals);
        }
    }
    CHECK(good.check_invariants().empty());

    // The committed wrong log: detected, and the first command moved less.
    const auto wrong_read = sim::read_log(read_file(fixture_log_path("cargo_load_wrong.log")));
    REQUIRE(wrong_read.ok());
    sim::World bad(1);
    const sim::ReplayOutcome no = replay_log(wrong_read.value(), bad);
    CHECK_FALSE(no.reproduced());
    REQUIRE(no.diverged_entry.has_value());
    CHECK(no.checkpoints_matched < no.checkpoints_total);

    // A changed target (the rival's warehouse instead of the owner's) refuses and diverges; the
    // rival's store received nothing and lost nothing.
    sim::ReplayLog elsewhere = log;
    changed = false;
    for (sim::ReplayEntry& e : elsewhere.entries) {
        for (sim::Command& c : e.commands) {
            if (!changed && std::holds_alternative<sim::LoadCmd>(c.body)) {
                std::get<sim::LoadCmd>(c.body).warehouse = bid(good, 1, 60, 10);
                changed = true;
            }
        }
    }
    REQUIRE(changed);
    sim::World moved(1);
    const sim::ReplayOutcome diverged = replay_log(elsewhere, moved);
    CHECK_FALSE(diverged.reproduced());
    CHECK(city(moved, 1).stock.stock[kWood] == 0);
}

// ---------------------------------------------------------------------------------------------
// Cargo and sailing
// ---------------------------------------------------------------------------------------------

namespace {

/// The factors of the Move segments in the order they start, sampled every 10 ms: a new segment
/// is a Move sample after a non-Move sample or one whose remaining length grew.
struct SegmentWatch {
    std::vector<std::uint32_t> starts;
    bool in_move = false;
    std::uint64_t last_remaining = 0;
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
    }
};

} // namespace

TEST_CASE("load sailing: a segment started after the load has the new factor, the running "
          "segment keeps its factor, the crew stays carried") {
    // An empty ship of 4 + 2 holds of 1000 t sails at V = 100; after 3000 t of wood are aboard, L =
    // 100 - trunc(17 * 768000 / (6 * 256000)) = 92 and V = (35 + 65) * 92 / 100 = 92
    // (tools/ship_step_ref.py, speed_factor_vectors.txt).
    Setup s;
    s.types = registry_with(9507, 1000 * kT, std::pair{4U, 2U});
    s.rules.nav.cargo_penalty_percent = 17;
    ShipDef sh;
    sh.kind = 9507;
    sh.tx = 16;
    sh.ty = 38;
    sh.holds = std::vector<sim::ShipHold>(6);
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
    s.stock[kWood] = 4000 * kT;
    const sim::MoveCmd go{sim::FigureId{1}, 70 * 256 + 128, 38 * 256 + 128};
    sim::World empty = make(s);  // keeps sailing empty
    sim::World loaded = make(s); // loads while the first segment is running
    REQUIRE(empty.apply(sim::Command{sim::PlayerId{0}, go}).ok());
    REQUIRE(loaded.apply(sim::Command{sim::PlayerId{0}, go}).ok());
    SegmentWatch a, b;
    bool done = false;
    for (int i = 0; i < 6000 && a.starts.size() < 8; ++i) {
        empty.advance(10);
        loaded.advance(10);
        a.sample(ship(empty));
        b.sample(ship(loaded));
        if (!done && !b.starts.empty()) {
            CHECK(b.starts[0] == 100); // the first movement segment runs at the empty factor
            const sim::BuildingId wh = bid(loaded, 0, 15, 30);
            const sim::FigureId crew_parent = ship(loaded, 2).parent;
            // Three holds of 1000 t, one command each, with no Advance between them.
            for (std::size_t h = 0; h < 3; ++h) {
                (void)load_ok(loaded, 0, cmd(1, wh, 0, kWood, 3000), 1000 * kT, h);
            }
            CHECK(loaded.figures().list.slots[0]->nav->seg.factor_percent == 100); // untouched
            CHECK(holds_text(ship(loaded).holds) == "42:256000 42:256000 42:256000 0:0 0:0 0:0");
            CHECK(ship(loaded, 2).parent == crew_parent); // the scout is still carried
            CHECK(ship(loaded, 2).parent == sim::FigureId{1});
            done = true;
            b.starts.resize(1);
        }
    }
    REQUIRE(done);
    // The empty ship sails every segment at 100.
    REQUIRE(a.starts.size() >= 6);
    for (const std::uint32_t f : a.starts) {
        CHECK(f == 100);
    }
    // The loaded ship's first segment ran at 100 to its end, every later one starts at 92.
    REQUIRE(b.starts.size() >= 4);
    CHECK(b.starts[0] == 100);
    for (std::size_t i = 1; i < b.starts.size(); ++i) {
        CHECK(b.starts[i] == 92);
    }
    CHECK(ship(loaded, 2).parent == sim::FigureId{1});
    CHECK(loaded.check_invariants().empty());
}
