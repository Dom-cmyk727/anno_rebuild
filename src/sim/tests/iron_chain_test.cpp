// MAP-EXTRACT-2 part T3 (docs/sim/iron-tools.md): the iron chain on the endless02 pilot through
// PUBLIC commands: a new shallow mine, the smelter 1017 (ore + wood -> EISEN), the toolsmith 1019
// (EISEN + wood -> WERKZEUG), a market cart into the city store, and a later BuildCmd that spends
// tools. Needs ANNO1503_GAME_DIR; skips with a message without it.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/mine_facts.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_search.hpp>
#include <anno/sim/world.hpp>

#include "iron_pilot.hpp"

namespace sim = anno::sim;
using namespace iron_pilot;

namespace {

constexpr sim::GoodsId kEisenerz = 1, kEisen = 15, kWerkzeug = 41, kHolz = 42;

std::uint64_t get(const std::map<sim::BuildingId, std::uint64_t>& m, sim::BuildingId k) {
    const auto it = m.find(k);
    return it == m.end() ? 0U : it->second;
}

void dump(const char* label, sim::World& w, const sim::TransportStats& st, sim::BuildingId mine) {
    const sim::City& city = city_of(w);
    const sim::BuildingId smelter_id =
        find_type(w, 1017) ? find_type(w, 1017)->id : sim::BuildingId{};
    std::cerr << label << " store ore " << city.stock.stock[kEisenerz] << " eisen "
              << city.stock.stock[kEisen] << " tools " << city.stock.stock[kWerkzeug] << " holz "
              << city.stock.stock[kHolz];
    if (const sim::Building* b = w.find_building(mine)) {
        std::cerr << " | mine out " << std::get<sim::Mine>(b->payload).out << " made "
                  << get(st.forestry.mine_made_88, mine) << " loaded " << get(st.loaded_from, mine);
    }
    for (const std::uint32_t type : {1017U, 1019U}) {
        const sim::Building* b = find_type(w, type);
        if (b == nullptr) {
            continue;
        }
        const auto& ws = std::get<sim::Workshop>(b->payload);
        std::cerr << " | " << type << " in";
        for (const auto v : ws.in) {
            std::cerr << " " << v;
        }
        std::cerr << " out";
        for (const auto v : ws.out) {
            std::cerr << " " << v;
        }
        if (const auto it = st.forestry.workshops.find(b->id); it != st.forestry.workshops.end()) {
            std::cerr << " made";
            for (const auto v : it->second.made_88) {
                std::cerr << " " << v;
            }
            std::cerr << " used";
            for (const auto v : it->second.consumed_88) {
                std::cerr << " " << v;
            }
        }
        std::cerr << " loaded " << get(st.loaded_from, b->id);
    }
    const auto un = st.unload_offered.find(kWerkzeug);
    std::cerr << " | tools unloaded " << (un == st.unload_offered.end() ? 0U : un->second);
    for (const auto& [g, v] : st.unload_offered) {
        std::cerr << " U" << g << "=" << v;
    }
    for (const auto& [g, v] : st.cart_lost) {
        std::cerr << " L" << g << "=" << v;
    }
    for (const auto& [k, v] : st.loaded_by_home) {
        if (k.first == smelter_id || k.first == mine) {
            std::cerr << " H(" << k.first.v << "<-" << k.second.v << ")=" << v;
        }
    }
    std::cerr << "\n";
}

void map_region(const sim::World& w, const sim::TypeRegistry& types, std::int32_t x0,
                std::int32_t y0, std::int32_t x1, std::int32_t y1) {
    const sim::Island& is = isle(w);
    for (std::int32_t y = y0; y <= y1; ++y) {
        std::string row;
        for (std::int32_t x = x0; x <= x1; ++x) {
            const std::uint64_t local =
                static_cast<std::uint64_t>(y - is.tiles.y0()) * is.tiles.width() +
                static_cast<std::uint64_t>(x - is.tiles.x0());
            const std::uint32_t id = is.tiles.object(local);
            char c = '.';
            if (id != 0) {
                const sim::BuildingType* t = types.find(sim::TypeId{id});
                c = t == nullptr ? '?' : sim::is_land_road(*t) ? '=' : '#';
            }
            row += c;
        }
        std::cerr << "MAP " << y << " " << row << "\n";
    }
}

TEST_CASE("iron chain: probe (opt-in measure)") {
    const char* on = std::getenv("ANNO_IRON_PROBE");
    if (on == nullptr || *on == '\0') {
        MESSAGE("skipping: set ANNO_IRON_PROBE=1 to print the chain's flows");
        return;
    }
    auto s = make(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    const bool zero = std::getenv("ANNO_IRON_ZERO") != nullptr;
    if (zero) {
        auto& st = test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city))->stock;
        st.stock[kEisenerz] = 0;
        st.stock[kEisen] = 0;
    }
    sim::BuildingId mine;
    if (std::getenv("ANNO_IRON_NOMINE") == nullptr) {
        const sim::ApplyResult r = w.apply(build_cmd(1205, 366, 353));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        mine = r.records.at(0).building;
    }
    if (std::getenv("ANNO_IRON_MAP") != nullptr) {
        map_region(w, *s->types, 340, 336, 395, 372);
        for (const std::uint32_t type : {1017U, 1019U, 1216U}) {
            const sim::Building* b = find_type(w, type);
            if (b != nullptr) {
                std::cerr << "BUILDING " << type << " at " << b->x << "," << b->y << " rot "
                          << int(b->rotation) << "\n";
            }
        }
    }
    dump("t=0   ", w, stats, mine);
    const auto fig_dump = [&](const char* tag) {
        for (const auto& f : w.figures().list.slots) {
            if (f && f->cargo_goods == kEisen) {
                std::cerr << tag << " EISENFIG kind " << f->kind << " job " << f->job << " home "
                          << f->home.v << " pos " << f->pos.x / 256 << "," << f->pos.y / 256
                          << " step " << int(f->transport.step) << " route "
                          << f->transport.route.size() << " cargo " << f->cargo_88 << "\n";
            }
        }
        for (const std::uint32_t type : {1017U, 1019U}) {
            const sim::Building* b = find_type(w, type);
            std::cerr << tag << " id " << type << "=" << (b ? b->id.v : 0) << "\n";
        }
    };
    for (int block = 1; block <= 12; ++block) {
        for (int i = 0; i < 500; ++i) {
            w.advance(kAdvanceMs);
        }
        dump(("t=" + std::to_string(block * 500)).c_str(), w, stats, mine);
        if (block == 2 || block == 3) {
            fig_dump("DBG");
        }
    }
}

TEST_CASE("iron chain: toolsmith site scan (opt-in measure)") {
    const char* on = std::getenv("ANNO_IRON_SCAN");
    if (on == nullptr || *on == '\0') {
        MESSAGE("skipping: set ANNO_IRON_SCAN=1 to scan toolsmith sites near the smelter");
        return;
    }
    auto s = make(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World base = s->w;
    {
        auto& st = test_must::must(sim::WorldTestAccess::islands(base)[1]->city(kCity.city))->stock;
        st.stock[kEisenerz] = 0;
        st.stock[kEisen] = 0;
    }
    REQUIRE(base.apply(build_cmd(1205, 366, 353)).ok());
    const int x0 = std::getenv("ANNO_IRON_X0") ? std::atoi(std::getenv("ANNO_IRON_X0")) : 358;
    const int x1 = std::getenv("ANNO_IRON_X1") ? std::atoi(std::getenv("ANNO_IRON_X1")) : 380;
    const int y0 = std::getenv("ANNO_IRON_Y0") ? std::atoi(std::getenv("ANNO_IRON_Y0")) : 340;
    const int y1 = std::getenv("ANNO_IRON_Y1") ? std::atoi(std::getenv("ANNO_IRON_Y1")) : 358;
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (std::uint8_t rot = 0; rot < 4; ++rot) {
                sim::World w = base;
                const sim::ApplyResult r = w.apply(build_cmd(1019, x, y, rot));
                if (!r.ok()) {
                    continue;
                }
                const sim::BuildingId id = r.records.at(0).building;
                sim::TransportStats stats;
                w.set_transport_stats(&stats);
                for (int i = 0; i < 1500; ++i) {
                    w.advance(kAdvanceMs);
                }
                const auto it = stats.forestry.workshops.find(id);
                const std::uint64_t made =
                    it == stats.forestry.workshops.end() ? 0 : it->second.made_88[0];
                const auto un = stats.unload_offered.find(kWerkzeug);
                std::cerr << "SCAN " << x << "," << y << " rot " << int(rot) << " tools made "
                          << made << " eisen_lost "
                          << (stats.cart_lost.count(kEisen) ? stats.cart_lost.at(kEisen) : 0)
                          << " tools_unloaded "
                          << (un == stats.unload_offered.end() ? 0 : un->second) << "\n";
            }
        }
    }
}

TEST_CASE("iron chain: the imported toolsmith now receives EISEN and makes WERKZEUG (goal sub-cell "
          "end)") {
    auto s = make(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    {
        auto& st = test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city))->stock;
        st.stock[kEisenerz] = 0; // labelled setup: the new mine is the only ore source
        st.stock[kEisen] = 0;
    }
    REQUIRE(w.apply(build_cmd(1205, 366, 353)).ok());
    for (int i = 0; i < 1500; ++i) {
        w.advance(kAdvanceMs);
    }
    const sim::Building* smelter = find_type(w, 1017);
    const sim::Building* smith = find_type(w, 1019);
    REQUIRE(smelter != nullptr);
    REQUIRE(smith != nullptr);
    const auto& ws = std::get<sim::Workshop>(smith->payload);
    const auto ledger = stats.forestry.workshops.find(smith->id);
    // T3.2 (stream 137): the walk ends on the goal sub-cell, so the return search starts from the
    // door sub-cell and finds the toolsmith: its carrier delivers, the input fills and is used,
    // WERKZEUG is made and a market cart unloads it into the city store, and nothing is lost
    const auto lifted = stats.loaded_by_home.find({smelter->id, smith->id});
    REQUIRE(lifted != stats.loaded_by_home.end());
    CHECK(lifted->second > 0);
    REQUIRE(ledger != stats.forestry.workshops.end());
    CHECK(ledger->second.consumed_88.at(0) > 0); // EISEN used
    CHECK(ledger->second.made_88.at(0) > 0);     // WERKZEUG made
    CHECK(ws.in.at(0) + ledger->second.consumed_88.at(0) > 0);
    CHECK(stats.cart_lost[kEisen] == 0);
    CHECK(stats.unload_offered[kWerkzeug] > 0);
    const sim::BuildingType& ty = *test_must::must(s->types->find(smith->type));
    const sim::BuildingType& sty = *test_must::must(s->types->find(smelter->type));
    const sim::FigureType& carrier = *test_must::must(s->types->find_figure_type(203));
    // the root cause stays pinned (characterisation): a return search started from the CENTRE
    // sub-cell of the smelter's door tile (where the sim's walks used to end) is dead, started
    // from the door point itself (the sub-cell the forward search aimed at) it finds the toolsmith
    // [old note follows] The forward walk ends on the CENTRE of the smelter's door tile (the
    // sim's route is tile centres, docs/re/transport.md; the original's goal is the door's
    // sub-cell), and the return search starts from that centre sub-cell, which the walk cells mark
    // dead; started from the door point itself (entrance_position, the sub-cell the forward search
    // aimed at) the same search finds the toolsmith
    const sim::FixedPos door = sim::entrance_position(*smelter, sty, 0);
    const sim::FixedPos centre{(door.x / 256) * 256 + 128, (door.y / 256) * 256 + 128};
    MESSAGE("smelter door point " << door.x << "," << door.y << " tile centre " << centre.x << ","
                                  << centre.y);
    CHECK_FALSE(sim::path_home(isle(w), *s->types, *smith, ty, centre, carrier.layer).has_value());
    CHECK(sim::path_home(isle(w), *s->types, *smith, ty, door, carrier.layer).has_value());
    const sim::FixedPos road{366 * 256 + 128, 349 * 256 + 128};
    CHECK(sim::path_home(isle(w), *s->types, *smith, ty, road, carrier.layer).has_value());
    if (std::getenv("ANNO_IRON_GRID") != nullptr) {
        for (std::int32_t y = 343; y <= 354; ++y) {
            std::string row;
            for (std::int32_t x = 362; x <= 374; ++x) {
                const sim::FixedPos p{x * 256 + 128, y * 256 + 128};
                const auto r = sim::path_home(isle(w), *s->types, *smith, ty, p, carrier.layer);
                row += r ? '1' : '0';
            }
            std::cerr << "GRID " << y << " " << row << "\n";
        }
        for (std::size_t i = 0; i < ty.entrances.size(); ++i) {
            const sim::FixedPos e = sim::entrance_position(*smith, ty, i);
            std::cerr << "SMITHDOOR " << e.x / 256 << "," << e.y / 256 << "\n";
        }
        for (std::size_t i = 0; i < sty.entrances.size(); ++i) {
            const sim::FixedPos e = sim::entrance_position(*smelter, sty, i);
            std::cerr << "SMELTERDOOR " << e.x / 256 << "," << e.y / 256 << "\n";
        }
    }
}

TEST_CASE("iron chain: trace of the imported toolsmith's carrier (opt-in measure)") {
    if (std::getenv("ANNO_IRON_TRACE") == nullptr) {
        MESSAGE("skipping: set ANNO_IRON_TRACE=1");
        return;
    }
    auto s = make(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = s->w;
    {
        auto& st = test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city))->stock;
        st.stock[kEisenerz] = 0;
        st.stock[kEisen] = 0;
    }
    REQUIRE(w.apply(build_cmd(1205, 366, 353)).ok());
    const sim::BuildingId smith = find_type(w, 1019)->id;
    std::map<std::uint64_t, std::string> last;
    for (int i = 0; i < 1500; ++i) {
        w.advance(kAdvanceMs);
        for (const auto& f : w.figures().list.slots) {
            if (!f || f->home != smith) {
                continue;
            }
            std::ostringstream o;
            o << "step " << int(f->transport.step) << " pos " << f->pos.x / 256 << ","
              << f->pos.y / 256 << " goods " << f->cargo_goods << " cargo " << f->cargo_88
              << " route " << f->transport.route.size() << " next " << f->transport.next
              << " target " << f->transport.target.v << " expiry " << (f->expiry_stamp ? 1 : 0);
            const std::uint64_t key = f->id.v;
            if (last[key] != o.str()) {
                last[key] = o.str();
                std::cerr << "TRACE adv " << i << " fig " << key << " " << o.str() << "\n";
            }
        }
    }
}

TEST_CASE("transport: a walk ends on the centre of the goal sub-cell (T3.2)") {
    // the rule: tile + (2 col + 1) / 6 per axis in Q24.8, rounded to the nearest 1/256
    const sim::TileXY tile{5, 7};
    const std::int32_t offsets[3] = {43, 128, 213}; // 256/6, 768/6, 1280/6
    for (std::uint32_t col = 0; col < 3; ++col) {
        for (std::uint32_t row = 0; row < 3; ++row) {
            const sim::FixedPos p = sim::subcell_centre(tile, col, row);
            CHECK(p.x == 5 * 256 + offsets[col]);
            CHECK(p.y == 7 * 256 + offsets[row]);
        }
    }
    CHECK(sim::subcell_centre(tile, 1, 1) ==
          sim::tile_centre(tile)); // the middle sub-cell is the centre
    // start_walk: with an end the last waypoint is the sub-cell centre; without, the tile centre
    const std::vector<sim::TileXY> path{{3, 7}, {4, 7}, {5, 7}};
    const sim::FixedPos start{3 * 256 + 100, 7 * 256 + 100};
    std::vector<sim::FixedPos> route;
    std::uint32_t next = 0, along = 0;
    sim::start_walk(start, path, sim::WalkRef{route, next, along});
    REQUIRE(route.size() == 3);
    CHECK(route.back() == sim::tile_centre(tile));
    const sim::WalkPoint end{tile, 2, 0};
    sim::start_walk(start, path, sim::WalkRef{route, next, along}, &end);
    REQUIRE(route.size() == 3);
    CHECK(route.back() == sim::subcell_centre(tile, 2, 0));
    CHECK(route[1] == sim::tile_centre(sim::TileXY{4, 7})); // the inner waypoints stay tile centres
    // and the walk really arrives there
    sim::FixedPos pos = start;
    const auto rest = sim::walk_ticks(pos, sim::WalkRef{route, next, along}, 100000000ULL, 1000);
    CHECK(rest.has_value());
    CHECK(pos == sim::subcell_centre(tile, 2, 0));
}

// ---- T3: the whole chain mine -> smelter -> toolsmith -> market cart -> store -> BuildCmd, scoped
// ledgers and controls (docs/sim/iron-tools.md T1.4 and T1.6, I7 and I8).

/// The first site of a window where `type` is answered with `want`, or nullopt.
std::optional<std::pair<std::int32_t, std::int32_t>>
find_site(const sim::World& base, std::uint32_t type, sim::Reject want) {
    for (std::int32_t y = 338; y <= 366; ++y) {
        for (std::int32_t x = 356; x <= 388; ++x) {
            sim::World w = base;
            if (w.apply(build_cmd(type, x, y)).status == want) {
                return std::pair{x, y};
            }
        }
    }
    return std::nullopt;
}

enum class Arm { Real, NoMine, NoSmelter, NoWood, NoMarkets, FullStore };

struct Chain {
    World pilot;
    sim::TransportStats stats;
    sim::BuildingId mine, smelter, smith;
    // the baselines of the terms the fixture owns (preloaded and imported)
    std::uint64_t smelter_in_ore0 = 0, smith_in_eisen0 = 0;
    std::uint64_t store_ore0 = 0, store_eisen0 = 0, store_tools0 = 0, store_holz0 = 0;
    std::uint64_t tools_spent = 0; // by the test's own BuildCmds
    std::pair<std::int32_t, std::int32_t> build_site{0,
                                                     0}; // a legal 1019 site, found before any edit
    explicit Chain(World&& p) : pilot(std::move(p)) {}
};

const sim::Building* building(const sim::World& w, sim::BuildingId id) {
    return id.valid() ? w.find_building(id) : nullptr;
}

/// `tools_zero`: the labelled setup "no WERKZEUG in the store" (a named fixture edit, family B), so
/// that a later BuildCmd can only be paid by what the chain delivered.
std::unique_ptr<Chain> start_chain(Arm arm, bool tools_zero) {
    auto p = make(true, false, 1000);
    if (!p) {
        return nullptr;
    }
    auto c = std::make_unique<Chain>(std::move(*p));
    sim::World& w = c->pilot.w;
    sim::City& city = *test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city));
    // labelled setup in every arm: the new mine is the only ore source, no EISEN in the store
    city.stock.stock[kEisenerz] = 0;
    city.stock.stock[kEisen] = 0;
    if (tools_zero) {
        // a legal site for the later BuildCmd, found while the store still pays for it
        const auto site = find_site(w, 1019, sim::Reject::None);
        REQUIRE(site.has_value());
        c->build_site = *site;
    }
    c->smelter = test_must::must(find_type(w, 1017))->id;
    c->smith = test_must::must(find_type(w, 1019))->id;
    if (arm == Arm::NoSmelter) {
        sim::DemolishCmd dem;
        dem.city = kCity;
        dem.tiles = {
            {test_must::must(find_type(w, 1017))->x, test_must::must(find_type(w, 1017))->y}};
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
        c->smelter = sim::BuildingId{};
    }
    if (arm == Arm::NoMarkets) {
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city.index.lists)
            for (const auto& b : l.slots)
                if (b && w.types().find(b->type) != nullptr &&
                    w.types().find(b->type)->cls == sim::RuntimeClass::Market)
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    }
    if (arm != Arm::NoMine) {
        const sim::ApplyResult r = w.apply(build_cmd(1205, 366, 353));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        c->mine = r.records.at(0).building;
    }
    if (arm == Arm::NoWood) {
        // the wood sources and stores of the chain are emptied: the foresters demolished, the store
        // and both buildings' wood inputs zeroed (labelled setup of this arm)
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city.index.lists)
            for (const auto& b : l.slots)
                if (b && b->type.v == 801)
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        const sim::ApplyResult dr = w.apply(sim::Command{kPlayer, dem});
        REQUIRE_MESSAGE(dr.ok(), sim::reject_name(dr.status) << " tiles " << dem.tiles.size());
        city.stock.stock[kHolz] = 0;
        for (const sim::BuildingId id : {c->smelter, c->smith}) {
            for (auto& l : city.index.lists)
                for (auto& b : l.slots)
                    if (b && b->id == id)
                        std::get<sim::Workshop>(b->payload).in.at(1) = 0;
        }
    }
    if (tools_zero) {
        // labelled setup AFTER the mine is paid: from here only the chain can put WERKZEUG in
        city.stock.stock[kWerkzeug] = 0;
    }
    if (arm == Arm::FullStore) {
        city.stock.stock[kWerkzeug] = city.stock.capacity;
    }
    REQUIRE_MESSAGE(w.check_invariants().empty(), w.check_invariants());
    w.set_transport_stats(&c->stats);
    if (const sim::Building* b = building(w, c->smelter)) {
        c->smelter_in_ore0 = std::get<sim::Workshop>(b->payload).in.at(0);
    }
    if (const sim::Building* b = building(w, c->smith)) {
        c->smith_in_eisen0 = std::get<sim::Workshop>(b->payload).in.at(0);
    }
    const sim::City& cc = city_of(w);
    c->store_ore0 = cc.stock.stock[kEisenerz];
    c->store_eisen0 = cc.stock.stock[kEisen];
    c->store_tools0 = cc.stock.stock[kWerkzeug];
    c->store_holz0 = cc.stock.stock[kHolz];
    return c;
}

void step_chain(Chain& c, int n) {
    for (int i = 0; i < n; ++i) {
        c.pilot.w.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!c.pilot.w.faulted(), c.pilot.w.fault());
    }
}

struct Terms {
    std::uint64_t mine_made = 0, mine_out = 0, ore_loaded = 0, ore_hands = 0, ore_lost = 0,
                  smelter_in_ore = 0, smelter_cons_ore = 0, ore_store = 0;
    std::uint64_t smelter_made = 0, smelter_out = 0, eisen_hands = 0, eisen_lost = 0,
                  smith_in_eisen = 0, smith_cons_eisen = 0, eisen_store = 0;
    std::uint64_t smith_made = 0, smith_out = 0, tools_hands = 0, tools_lost = 0,
                  tools_unloaded = 0, tools_store = 0;
    std::uint64_t wood_smelter = 0, wood_smith = 0, holz_store = 0; // the chain's takes, not closed
    std::int64_t u_mine = 0, u_ore = 0, u_eisen = 0, u_tools_flow = 0, tools_other = 0;
};

Terms terms_of(Chain& c) {
    Terms t;
    sim::World& w = c.pilot.w;
    const sim::TransportStats& st = c.stats;
    const auto got = [](const auto& m, const auto& k) -> std::uint64_t {
        const auto it = m.find(k);
        return it == m.end() ? 0U : it->second;
    };
    const sim::City& city = city_of(w);
    if (c.mine.valid()) {
        t.mine_made = got(st.forestry.mine_made_88, c.mine);
        t.ore_loaded = got(st.loaded_from, c.mine);
        if (const sim::Building* m = building(w, c.mine))
            t.mine_out = std::get<sim::Mine>(m->payload).out;
    }
    const auto ledger = [&](sim::BuildingId id, std::size_t input, std::size_t output,
                            std::uint64_t& in_now, std::uint64_t& cons, std::uint64_t& made,
                            std::uint64_t& out, std::uint64_t* wood_take) {
        const sim::Building* b = building(w, id);
        if (b == nullptr)
            return;
        const auto& ws = std::get<sim::Workshop>(b->payload);
        in_now = ws.in.at(input);
        out = ws.out.at(output);
        const auto it = st.forestry.workshops.find(id);
        if (it != st.forestry.workshops.end()) {
            cons = it->second.consumed_88.at(input);
            made = it->second.made_88.at(output);
            if (wood_take != nullptr)
                *wood_take = ws.in.at(1) + it->second.consumed_88.at(1);
        }
    };
    ledger(c.smelter, 0, 0, t.smelter_in_ore, t.smelter_cons_ore, t.smelter_made, t.smelter_out,
           &t.wood_smelter);
    ledger(c.smith, 0, 0, t.smith_in_eisen, t.smith_cons_eisen, t.smith_made, t.smith_out,
           &t.wood_smith);
    for (const auto& f : w.figures().list.slots) {
        if (!f || f->expiry_stamp.has_value() || f->owner != city.owner || f->resident)
            continue;
        if (f->cargo_goods == kEisenerz)
            t.ore_hands += f->cargo_88;
        if (f->cargo_goods == kEisen)
            t.eisen_hands += f->cargo_88;
        if (f->cargo_goods == kWerkzeug)
            t.tools_hands += f->cargo_88;
    }
    t.ore_lost = got(st.cart_lost, kEisenerz);
    t.eisen_lost = got(st.cart_lost, kEisen);
    t.tools_lost = got(st.cart_lost, kWerkzeug);
    t.tools_unloaded = got(st.unload_offered, kWerkzeug);
    t.ore_store = city.stock.stock[kEisenerz];
    t.eisen_store = city.stock.stock[kEisen];
    t.tools_store = city.stock.stock[kWerkzeug];
    t.holz_store = city.stock.stock[kHolz];
    const auto I = [](std::uint64_t v) { return static_cast<std::int64_t>(v); };
    // ore: made = in the mine + lifted; lifted = in the hands + lost + into the smelter (input now
    // - input at start + used) + into the store (change)
    t.u_mine = I(t.mine_made) - I(t.mine_out) - I(t.ore_loaded);
    t.u_ore = I(t.ore_loaded) - I(t.ore_hands) - I(t.ore_lost) -
              (I(t.smelter_in_ore) - I(c.smelter_in_ore0) + I(t.smelter_cons_ore)) -
              (I(t.ore_store) - I(c.store_ore0));
    // EISEN: made = in the smelter's output + in the hands + lost + into the toolsmith (input now -
    // input at start + used) + into the store (change)
    t.u_eisen = I(t.smelter_made) - I(t.smelter_out) - I(t.eisen_hands) - I(t.eisen_lost) -
                (I(t.smith_in_eisen) - I(c.smith_in_eisen0) + I(t.smith_cons_eisen)) -
                (I(t.eisen_store) - I(c.store_eisen0));
    // WERKZEUG: made = in the toolsmith's output + in the hands + lost + unloaded by market carts
    t.u_tools_flow =
        I(t.smith_made) - I(t.smith_out) - I(t.tools_hands) - I(t.tools_lost) - I(t.tools_unloaded);
    // the store: change = unloaded - spent by the test's BuildCmds - other consumers (houses'
    // upgrades): the drain is the unexplained remainder, REPORTED (never asserted to be zero)
    t.tools_other = I(t.tools_unloaded) - I(c.tools_spent) - (I(t.tools_store) - I(c.store_tools0));
    return t;
}

void require_closed(const Terms& t, const char* label) {
    INFO("ledger " << label);
    CHECK(t.u_mine == 0);
    CHECK(t.u_ore == 0);
    CHECK(t.u_eisen == 0);
    CHECK(t.u_tools_flow == 0);
}

void say(const char* label, const Terms& t) {
    std::cerr << "CHAIN " << label << " mine_made " << t.mine_made << " ore_loaded " << t.ore_loaded
              << " smelter_made " << t.smelter_made << " smith_made " << t.smith_made
              << " tools_unloaded " << t.tools_unloaded << " tools_store " << t.tools_store
              << " tools_other " << t.tools_other << " holz_store " << t.holz_store
              << " wood_smelter " << t.wood_smelter << " wood_smith " << t.wood_smith << " u "
              << t.u_mine << "," << t.u_ore << "," << t.u_eisen << "," << t.u_tools_flow << "\n";
}

TEST_CASE("iron chain: probe of the zero-tools family (opt-in measure)") {
    if (std::getenv("ANNO_IRON_LEDGER") == nullptr) {
        MESSAGE("skipping: set ANNO_IRON_LEDGER=1");
        return;
    }
    for (const Arm arm : {Arm::Real, Arm::NoMine}) {
        auto c = start_chain(arm, true);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        for (int block = 0; block < 8; ++block) {
            step_chain(*c, 500);
            say((std::string("B arm ") + std::to_string(int(arm)) + " at " +
                 std::to_string((block + 1) * 500))
                    .c_str(),
                terms_of(*c));
        }
    }
}

TEST_CASE("iron chain: probe of the ledgers (opt-in measure)") {
    if (std::getenv("ANNO_IRON_LEDGER") == nullptr) {
        MESSAGE("skipping: set ANNO_IRON_LEDGER=1");
        return;
    }
    for (const Arm arm :
         {Arm::Real, Arm::NoMine, Arm::NoSmelter, Arm::NoWood, Arm::NoMarkets, Arm::FullStore}) {
        auto c = start_chain(arm, false);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        for (int block = 0; block < 6; ++block) {
            step_chain(*c, 1000);
            say((std::string("arm ") + std::to_string(int(arm)) + " block " + std::to_string(block))
                    .c_str(),
                terms_of(*c));
            require_closed(terms_of(*c), "probe");
        }
    }
}

const char* kArmName[] = {"real", "no mine", "no smelter", "no wood", "no markets", "full store"};

/// Runs `arm` for `blocks` x 1000 advances, closing every ledger at each block; `each` sees the
/// terms at the end of each block.
template <class Each> Terms run_arm(Chain& c, int blocks, Each each) {
    for (int b = 0; b < blocks; ++b) {
        step_chain(c, 1000);
        const Terms t = terms_of(c);
        require_closed(t, "block");
        each(t);
    }
    return terms_of(c);
}

TEST_CASE("iron chain: the real arm closes every ledger and delivers WERKZEUG to the store") {
    auto real = start_chain(Arm::Real, false);
    auto base = start_chain(Arm::NoMine, false);
    if (!real || !base) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // baselines: the imported smelter holds no ore and the toolsmith no EISEN at the start (the
    // preloaded wood and the 172 t of WERKZEUG are the imported terms, kept apart below)
    CHECK(real->smelter_in_ore0 == 0);
    CHECK(real->smith_in_eisen0 == 0);
    CHECK(real->store_ore0 == 0);
    CHECK(real->store_eisen0 == 0);
    std::uint64_t made_before = 0;
    const Terms t = run_arm(*real, 6, [&](const Terms& x) {
        CHECK(x.mine_made >= made_before); // production only ever grows
        made_before = x.mine_made;
    });
    static_cast<void>(run_arm(*base, 6, [](const Terms&) {}));
    const Terms b = terms_of(*base);
    say("real", t);
    say("base", b);
    // production beyond the baseline, link by link
    CHECK(t.mine_made > 0);
    CHECK(t.ore_loaded > 0);       // the smelter's carrier lifted the new mine's ore
    CHECK(t.smelter_cons_ore > 0); // and the smelter used it (no other ore existed)
    CHECK(t.smelter_made > 0);     // EISEN
    CHECK(t.smith_cons_eisen > 0); // the toolsmith used it
    CHECK(t.smith_made > 0);       // WERKZEUG
    CHECK(t.tools_unloaded > 0);   // a market cart carried it into the city store
    CHECK(t.tools_lost == 0);
    CHECK(t.eisen_lost == 0);
    CHECK(t.ore_lost == 0);
    // the baseline arm has no chain at all
    CHECK(b.mine_made == 0);
    CHECK(b.smelter_made == 0);
    CHECK(b.smith_made == 0);
    CHECK(b.tools_unloaded == 0);
    // the store: the real arm ends above the baseline arm. The drain by other consumers (the
    // houses' stage ups) is REPORTED and bounded, not hidden: both arms lose WERKZEUG, the real
    // arm's drain is within a measured band of the baseline's (a measurement of this fixture, not a
    // derived constant).
    CHECK(t.tools_store > b.tools_store);
    CHECK(t.tools_other > 0);
    CHECK(b.tools_other > 0);
    const double ratio = static_cast<double>(t.tools_other) / static_cast<double>(b.tools_other);
    CHECK(ratio > 0.75);
    CHECK(ratio < 1.25);
    // wood is a shared drain: the chain's own takes are reported (smelter and toolsmith input plus
    // use), the store's HOLZ is the same in both arms at the end (the foresters refill it)
    MESSAGE("wood taken by the chain: smelter " << t.wood_smelter - real->store_holz0 * 0
                                                << " toolsmith " << t.wood_smith << " store real "
                                                << t.holz_store << " store base " << b.holz_store);
    CHECK(t.wood_smelter > b.wood_smelter); // the smelter really burned wood the baseline did not
}

TEST_CASE(
    "iron chain: with no mine no smelter no wood or no market cover the store does not rise") {
    for (const Arm arm : {Arm::NoMine, Arm::NoSmelter, Arm::NoWood, Arm::NoMarkets}) {
        INFO("arm " << kArmName[int(arm)]);
        auto c = start_chain(arm, false);
        if (!c) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        const Terms t = run_arm(*c, 4, [&](const Terms& x) {
            CHECK(x.tools_unloaded == 0);            // nothing reaches the store
            CHECK(x.tools_store <= c->store_tools0); // so it never rises above where it started
        });
        say(kArmName[int(arm)], t);
        switch (arm) {
        case Arm::NoMine:
            CHECK(t.mine_made == 0);
            CHECK(t.smelter_made == 0);
            CHECK(t.smith_made == 0);
            break;
        case Arm::NoSmelter:
            // the mine works until its own store is full, nothing is lifted and nothing is smelted
            CHECK(t.mine_made > 0);
            CHECK(t.mine_made <=
                  1792 + 21); // its output capacity (7 t) plus the step that crossed it
            CHECK(t.mine_out == t.mine_made); // all of it still in the mine's store
            CHECK(t.ore_loaded == 0);
            CHECK(t.smelter_made == 0);
            CHECK(t.smith_made == 0);
            break;
        case Arm::NoWood:
            // ore reaches the smelter but without wood it makes nothing, and the toolsmith with it
            CHECK(t.mine_made > 0);
            CHECK(t.smelter_made == 0);
            CHECK(t.smith_made == 0);
            CHECK(t.holz_store == 0);
            break;
        case Arm::NoMarkets:
            // the chain runs up to the toolsmith, whose WERKZEUG no cart carries away: it stops
            // when its output store is full
            CHECK(t.smelter_made > 0);
            CHECK(t.smith_made > 0);
            CHECK(t.smith_out > 0);
            break;
        default:
            break;
        }
    }
}

TEST_CASE("iron chain: a full WERKZEUG store does not rise") {
    auto c = start_chain(Arm::FullStore, false);
    if (!c) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::City& city = city_of(c->pilot.w);
    CHECK(c->store_tools0 == city.stock.capacity);
    const Terms t = run_arm(*c, 4, [&](const Terms& x) {
        CHECK(x.tools_store <= c->store_tools0); // never above the full level it started at
    });
    say("full store", t);
    // the toolsmith still makes WERKZEUG (the control for "blocked for the stated reason": the
    // chain is not broken upstream) and carts still offer it, but the store takes only the room
    // the other consumers free
    CHECK(t.smith_made > 0);
}

TEST_CASE("iron chain: a later BuildCmd spends the tools the chain delivered") {
    auto real = start_chain(Arm::Real, true);
    auto none = start_chain(Arm::NoMine, true);
    if (!real || !none) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto& site = real->build_site;
    const sim::BuildingType& ty = *test_must::must(real->pilot.types->find(sim::TypeId{1019}));
    std::uint64_t cost_tools = 0;
    for (const sim::CostEntry& e : ty.cost_goods)
        if (e.goods == kWerkzeug)
            cost_tools = e.amount_88;
    REQUIRE(cost_tools > 0);
    // before any delivery both arms hold no WERKZEUG: the BuildCmd is refused and changes nothing
    for (Chain* c : {real.get(), none.get()}) {
        sim::World w = c->pilot.w;
        const auto before = w.save();
        CHECK(city_of(w).stock.stock[kWerkzeug] == 0);
        CHECK(w.apply(build_cmd(1019, site.first, site.second)).status ==
              sim::Reject::CannotAfford);
        CHECK(w.save() == before);
    }
    step_chain(*real, 2500);
    step_chain(*none, 2500);
    const Terms t = terms_of(*real);
    say("spend, real", t);
    require_closed(t, "before the spend");
    REQUIRE(t.tools_store >= cost_tools);     // the chain paid for it
    CHECK(t.tools_store == t.tools_unloaded); // and nothing else put WERKZEUG there
    // the control without a mine: still nothing, still refused
    CHECK(terms_of(*none).tools_store == 0);
    {
        sim::World w = none->pilot.w;
        const auto before = w.save();
        CHECK(w.apply(build_cmd(1019, site.first, site.second)).status ==
              sim::Reject::CannotAfford);
        CHECK(w.save() == before);
    }
    // the real arm: accepted, the store is debited exactly the type's WERKZEUG cost, and the ledger
    // (unloaded - spent - drain) still closes with the spend booked
    const std::uint64_t store_before = city_of(real->pilot.w).stock.stock[kWerkzeug];
    const sim::ApplyResult r = real->pilot.w.apply(build_cmd(1019, site.first, site.second));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    real->tools_spent += cost_tools;
    CHECK(store_before - city_of(real->pilot.w).stock.stock[kWerkzeug] == cost_tools);
    const Terms after = terms_of(*real);
    CHECK(after.tools_other == t.tools_other); // the spend is booked, the drain is unchanged
    step_chain(*real, 500);
    require_closed(terms_of(*real), "after the spend");
}

} // namespace
