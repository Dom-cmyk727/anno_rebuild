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

#include "iron_chain_fixture.hpp"
#include "iron_pilot.hpp"

namespace sim = anno::sim;
using namespace iron_pilot;
using namespace iron_chain;

namespace {

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
