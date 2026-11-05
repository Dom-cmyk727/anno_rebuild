// MAP-SUPPLY-1 S5: interruption and recovery, owners, reload and replay of the real chain.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/world.hpp>

#include "evidence_out.hpp"
#include "harvest_helpers.hpp"
#include "island7_town.hpp"

namespace {
namespace sim = anno::sim;
using namespace island7;
namespace ht = harvest_test;

std::uint64_t sold(const Chain& c) {
    const auto& m = c.stats.residents.inn_taken_by_building;
    return m.count(c.inn) ? m.at(c.inn) : 0U;
}

/// The tavern goods the houses of the town hold, summed over their tavern slot.
std::uint64_t house_tavern(const Chain& c, const Original& o) {
    std::uint64_t n = 0;
    for (auto& l : c.city->index.lists) {
        for (auto& b : l.slots) {
            if (const auto* h = b ? std::get_if<sim::House>(&b->payload) : nullptr) {
                const sim::BuildingType* t = o.types->find(b->type);
                const auto g = sim::house_group(c.w.config(), t->prodtype);
                const auto view = o.types->population_view(*g, c.w.config());
                n += h->stock[view.value()
                                  .derived->lookup[o.types->population_roles().tavern_goods]];
            }
        }
    }
    return n;
}

void step_to(Chain& c, std::uint64_t tick) {
    while (c.w.clock().tick() < tick) {
        c.w.advance(100);
        REQUIRE_FALSE(c.w.faulted());
    }
}

/// Two cities of one island for the cross-city cases: rows 100-102 and 107-131 are city 0 (player
/// 0), rows 103-106 are city 1 (player 1); a road runs along x = 110, y 103-112. Returns the spec
/// with the road and the second city.
sim::ScenarioSpec two_city_island() {
    sim::ScenarioSpec spec = ht::make_rig().spec;
    spec.islands[0].objects.clear();
    for (std::int32_t y = 103; y <= 112; ++y) {
        const auto& is = spec.islands[0];
        spec.islands[0].objects.push_back(sim::ScenarioSpec::TileObjectSpec{
            static_cast<std::uint32_t>((y - is.y0) * static_cast<std::int32_t>(is.w) +
                                       (110 - is.x0)),
            ht::kRoad.v, 0});
    }
    spec.islands[0].owner_runs = {{0, 3 * 32}, {1, 4 * 32}, {0, 25 * 32}};
    spec.islands[0].cities.push_back(anno::sim::synthetic::plain_city(1, 1));
    return spec;
}

void place_in(sim::ScenarioSpec& spec, std::size_t city, sim::TypeId type, std::int32_t x,
              std::int32_t y, std::optional<sim::Payload> p) {
    sim::ScenarioSpec::BuildingSpec b;
    b.type = type;
    b.x = x;
    b.y = y;
    b.payload = std::move(p);
    spec.islands[0].cities[city].buildings.push_back(std::move(b));
}

} // namespace

TEST_CASE("resilience: the alcohol path interrupted by demolishing the farm, drained, and "
          "recovered by rebuilding it") {
    // The Tutorial island-7 town at the checkpoint schedule of the chain test (advance(100) steps).
    // At tick 150 000 the Inn has sold 312 units made by the farm; the potato farm is demolished by
    // DemolishCmd, the path drains, and at 270 000 a farm 809 is rebuilt by BuildCmd on the same
    // site. The control never rebuilds. The houses' own tavern stocks (13 537 at the interruption)
    // are initial inventory that decays by itself; what the households depended on is the
    // DIFFERENCE between the two runs after the rebuild.
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    constexpr std::uint64_t T0 = 150000, T1 = 270000, T2 = 510000;
    auto R = start(*o, true);
    auto C = start(*o, true);
    Chain& r = *R;
    Chain& c = *C;
    step_to(r, T0);
    step_to(c, T0);
    CHECK(r.w.state_hash() == c.w.state_hash());
    const std::uint64_t sold0 = sold(r), htav0 = house_tavern(r, *o);
    CHECK(sold0 == 312);
    CHECK(htav0 == 13537);
    sim::Command d;
    d.issuer = r.city->owner;
    d.body = sim::DemolishCmd{r.ref, {sim::TileXY{261, 344}}};
    REQUIRE(r.w.apply(d).ok());
    REQUIRE(c.w.apply(d).ok());
    CHECK(r.at(r.farm_potato) == nullptr); // the farm is gone
    // ---- the gap: drained, supply stops, the households' stock only falls ----
    step_to(r, T1);
    step_to(c, T1);
    CHECK(r.w.state_hash() == c.w.state_hash()); // both worlds are the same until the rebuild
    CHECK(sold(r) == sold0);                     // nothing more was sold in the whole gap
    CHECK(r.inn_state().tavern == 0);            // the Inn is drained
    CHECK(cargo_in_flight(r, kAlcohol) == 0);
    CHECK(r.city->stock.stock[kAlcohol] == 0);
    CHECK(house_tavern(r, *o) < htav0);
    CHECK(house_tavern(r, *o) == 8188);
    // ---- the restoration: a public command ----
    sim::Command b;
    b.issuer = r.city->owner;
    b.body = sim::BuildCmd{r.ref, {sim::PlacementWire{sim::TypeId{809}, 261, 344, 0, 0, 0}}};
    CHECK_FALSE(r.w.rules().free_build_cheat); // the rebuild is paid, not free
    const std::int64_t money0 = r.w.players().find(r.city->owner)->money;
    const std::uint32_t wz0 = r.city->stock.stock[41], holz0 = r.city->stock.stock[42];
    const auto rebuilt = r.w.apply(b);
    REQUIRE_MESSAGE(rebuilt.ok(), std::string(sim::reject_name(rebuilt.status)));
    const sim::Building* nb = r.isl->building_at(r.w.config().layout, r.city->number, 261, 344);
    REQUIRE(nb != nullptr);
    CHECK(nb->type.v == 809);
    CHECK(nb->id != r.farm_potato); // a new building, new counters, nothing carried over
    // the cost of type 809 (registry): gold 250, tools 2 t (512), wood 4 t (1024), taken from the
    // owner and the store
    CHECK(money0 - r.w.players().find(r.city->owner)->money == 250);
    CHECK(wz0 - r.city->stock.stock[41] == 512);
    CHECK(holz0 - r.city->stock.stock[42] == 1024);
    std::set<sim::BuildingId> houses_up, houses_up_control;
    std::map<sim::BuildingId, std::uint32_t> prev, prev_control;
    const auto watch = [&](Chain& ch, std::set<sim::BuildingId>& up,
                           std::map<sim::BuildingId, std::uint32_t>& before) {
        for (auto& l : ch.city->index.lists) {
            for (auto& bb : l.slots) {
                if (const auto* h = bb ? std::get_if<sim::House>(&bb->payload) : nullptr) {
                    const auto view = o->types->population_view(
                        *sim::house_group(ch.w.config(), o->types->find(bb->type)->prodtype),
                        ch.w.config());
                    const std::uint32_t slot =
                        view.value().derived->lookup[o->types->population_roles().tavern_goods];
                    const auto it = before.find(bb->id);
                    if (it != before.end() && h->stock[slot] > it->second) {
                        up.insert(bb->id);
                    }
                    before[bb->id] = h->stock[slot];
                }
            }
        }
    };
    while (r.w.clock().tick() < T2) {
        r.w.advance(100);
        c.w.advance(100);
        watch(r, houses_up, prev);
        watch(c, houses_up_control, prev_control);
    }
    const std::uint64_t sold_r = sold(r), sold_c = sold(c);
    MESSAGE("recovered: sold " << sold_r << " (control " << sold_c << "), house tavern "
                               << house_tavern(r, *o) << " (control " << house_tavern(c, *o)
                               << "), houses that received " << houses_up.size());
    CHECK(sold_c == sold0); // without the farm nothing is ever sold again
    CHECK(sold_r > sold0);  // with it the Inn sells again: actual production and delivery
    CHECK(sold_r == 936);
    CHECK(house_tavern(r, *o) > house_tavern(c, *o));
    CHECK(house_tavern(r, *o) == 9726);
    CHECK(house_tavern(c, *o) == 1957);
    CHECK(houses_up.size() >=
          3); // at least three households received tavern goods after the rebuild
    CHECK(houses_up_control.empty()); // and without the rebuild none did
    const auto& ledger = r.stats.forestry.small_farms.at(nb->id);
    CHECK(ledger.made_88[0] > 0); // the new farm made alcohol from potatoes its own worker cut
    CHECK(r.w.check_invariants() == "");
    CHECK(c.w.check_invariants() == "");
}

TEST_CASE("resilience: two owners on one island: a rival farm nearer to the market is not "
          "collected, each worker credits its own farm") {
    // One island, two cities: rows 100-102 and 107-131 belong to city 0 (player 0), rows 103-106 to
    // city 1 (player 1). Player 0 owns the market (109,100) and the farm (110,108) at the end of a
    // road (x = 110, y 103-107); player 1's farm (111,103) stands beside that road, NEARER to the
    // market. Both farms hold 960 units of output (a focused fixture: production lines off, the
    // output preset, disclosed) and the cart must take only its own city's. A control world
    // without the rival must give the owner the identical ledger (single-owner control).
    const auto types = ht::registry(3 * 256);
    struct Outcome {
        std::uint32_t own_out, rival_out, store0, store1;
        bool rival_loaded;
        bool invariants_ok;
    };
    const auto run = [&](bool with_rival) {
        sim::ScenarioSpec spec = two_city_island();
        const auto preset = [&](sim::TypeId type) {
            sim::SmallFarm f = sim::make_small_farm(types->find(type)->prod);
            f.line_on = {false, false};
            f.out = {960, 0}; // KORN is a raw good: a cart takes it only above 75 % of the 1024
                              // capacity (docs/re/production.md 5.2)
            return f;
        };
        place_in(spec, 0, ht::kMarket, 109, 100, std::nullopt);
        place_in(spec, 0, ht::kSmallFarm, 110, 108, preset(ht::kSmallFarm));
        if (with_rival) {
            place_in(spec, 1, ht::kSmallFarm, 111, 103, preset(ht::kSmallFarm));
        }
        auto built = sim::World::create(spec, types, sim::SimConfig{}, sim::Rules{});
        REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
        sim::World w = std::move(built).value();
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        const sim::Island& is = *w.islands()[0];
        const sim::Building* own = is.building_at(w.config().layout, sim::CityNo{0}, 110, 108);
        const sim::Building* rival =
            with_rival ? is.building_at(w.config().layout, sim::CityNo{1}, 111, 103) : nullptr;
        REQUIRE(own != nullptr);
        REQUIRE((rival != nullptr) == with_rival);
        for (int i = 0; i < 6000; ++i) {
            w.advance(100);
        }
        const sim::GoodsId korn = ht::goods_id("KORN");
        const auto out_of = [&](const sim::Building* x) {
            return std::get<sim::SmallFarm>(is.find_building(x->id)->payload).out[0];
        };
        return Outcome{out_of(own),
                       rival != nullptr ? out_of(rival) : 0U,
                       is.cities[0]->stock.stock[korn],
                       is.cities[1]->stock.stock[korn],
                       rival != nullptr && stats.loaded_from.count(rival->id) != 0,
                       w.check_invariants() == ""};
    };
    const Outcome with = run(true), without = run(false);
    MESSAGE("with rival: own out " << with.own_out << " rival out " << with.rival_out << " store0 "
                                   << with.store0 << " store1 " << with.store1
                                   << "; without: own out " << without.own_out << " store0 "
                                   << without.store0);
    CHECK(with.rival_out ==
          960); // the rival's output was never loaded, though it is nearer to the market
    CHECK(with.store1 == 0);   // the other city received nothing
    CHECK(with.own_out == 0);  // the owner's cart took its own farm's output ...
    CHECK(with.store0 == 960); // ... and its warehouse received it
    CHECK_FALSE(with.rival_loaded);
    // the single-owner control: the same world without the rival gives the owner the identical
    // ledger
    CHECK(without.own_out == with.own_out);
    CHECK(without.store0 == with.store0);
    CHECK(with.invariants_ok);
    CHECK(without.invariants_ok);
}

TEST_CASE("resilience: save and reload at the crop, grazing, production, cart and resident "
          "boundaries of the real chain") {
    // The Tutorial island-7 chain (advance(100) steps, as the chain test). Each boundary is the
    // first step at which its condition holds in the uninterrupted run; the save is loaded there,
    // the loaded world advances 3000 ticks, and the uninterrupted world must have the same state
    // hash when it reaches that tick.
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto A = start(*o, true);
    Chain& c = *A;
    struct Pending {
        std::uint64_t at;
        std::uint64_t hash;
        std::string name;
    };
    std::vector<Pending> pending;
    std::set<std::string> done;
    // The conditions read any world of this scenario, so the loaded copy can be asked the same
    // question.
    const auto island_of_world = [&](const sim::World& w) -> const sim::Island* {
        for (const auto& i : w.islands()) {
            if (i && i->index.v == kIsland7) {
                return &*i;
            }
        }
        return nullptr;
    };
    const auto living = [&](const sim::World& w, auto pred) {
        for (const auto& s : w.figures().list.slots) {
            if (s.has_value() && !s->expiry_stamp.has_value() && pred(*s)) {
                return true;
            }
        }
        return false;
    };
    const std::vector<std::pair<std::string, std::function<bool(const sim::World&)>>> boundaries{
        {"crop work (a worker waiting at a field)",
         [&](const sim::World& w) {
             return living(w, [&](const sim::Figure& f) {
                 return f.job == 0x2a && f.harvest && f.harvest->step == sim::HarvestStep::Cut;
             });
         }},
        {"grazing (an animal waiting at a field)",
         [&](const sim::World& w) {
             return living(w, [&](const sim::Figure& f) {
                 return sim::animal_job(f.job) && f.harvest &&
                        f.harvest->step == sim::HarvestStep::Cut;
             });
         }},
        {"production / input balance (raw potatoes in the farm)",
         [&](const sim::World& w) {
             const sim::Building* b = island_of_world(w)->find_building(c.farm_potato);
             return b != nullptr && std::get<sim::SmallFarm>(b->payload).raw[0] > 0;
         }},
        {"cart cargo (a market cart carrying goods)",
         [&](const sim::World& w) {
             return living(w,
                           [&](const sim::Figure& f) { return f.job == 0x34 && f.cargo_88 > 0; });
         }},
        {"resident cargo (a house walker carrying purchases)",
         [&](const sim::World& w) {
             return living(w, [&](const sim::Figure& f) {
                 if (!f.resident) {
                     return false;
                 }
                 for (const auto& cg : f.resident->cargo) {
                     if (cg.goods != 0 && cg.amount_88 > 0) {
                         return true;
                     }
                 }
                 return false;
             });
         }},
        {"Inn stock above zero",
         [&](const sim::World& w) {
             const sim::Building* b = island_of_world(w)->find_building(c.inn);
             return b != nullptr && std::get<sim::Inn>(b->payload).tavern > 0;
         }},
    };
    std::map<std::string, std::uint64_t> at_tick;
    while (c.w.clock().tick() < 250000 && done.size() < boundaries.size()) {
        const std::uint64_t now = c.w.clock().tick();
        for (const auto& [name, cond] : boundaries) {
            if (done.contains(name) || !cond(c.w)) {
                continue;
            }
            done.insert(name);
            at_tick[name] = now;
            auto loaded = sim::World::load(c.w.save(), o->types);
            REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
            sim::World l = std::move(loaded).value();
            CHECK(l.state_hash() == c.w.state_hash());
            CHECK_MESSAGE(cond(l),
                          name << ": the boundary condition holds on the loaded world too");
            while (l.clock().tick() < now + 3000) {
                l.advance(100);
                REQUIRE_FALSE(l.faulted());
            }
            CHECK(l.check_invariants() == "");
            pending.push_back({l.clock().tick(), l.state_hash(), name});
        }
        for (auto it = pending.begin(); it != pending.end();) {
            if (it->at == now) {
                CHECK_MESSAGE(c.w.state_hash() == it->hash, it->name << " at tick " << now);
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
        c.w.advance(100);
    }
    for (auto it = pending.begin(); it != pending.end();) {
        // a pending comparison whose tick was passed while the loop ended: finish the run
        while (c.w.clock().tick() < it->at) {
            c.w.advance(100);
        }
        CHECK_MESSAGE(c.w.state_hash() == it->hash, it->name);
        it = pending.erase(it);
    }
    for (const auto& [name, cond] : boundaries) {
        MESSAGE(name << ": first at tick " << (at_tick.count(name) ? at_tick.at(name) : 0));
        CHECK_MESSAGE(done.contains(name), name << " never occurred");
    }
    // measured and pinned (the advance(100) schedule; a legitimate change of the models re-pins
    // them)
    CHECK(at_tick.at(boundaries[0].first) == 800);
    CHECK(at_tick.at(boundaries[1].first) == 1450);
    CHECK(at_tick.at(boundaries[2].first) == 14100);
    CHECK(at_tick.at(boundaries[3].first) == 8600);
    CHECK(at_tick.at(boundaries[4].first) == 8050);
    CHECK(at_tick.at(boundaries[5].first) == 95700);
    CHECK(c.w.check_invariants() == "");
}

TEST_CASE("resilience: operation counters of the harvest search do not grow with unrelated farms") {
    // One farm with fields and k unrelated farms far away without any field in reach. The farm-A
    // search costs (tiles examined per search, candidates, walk nodes popped) are properties of the
    // window and the fields; the k other farms add searches that stop at the empty candidate list.
    // Deterministic counters, no timing.
    const auto measure = [&](int k) {
        sim::ScenarioSpec spec = ht::make_rig().spec;
        std::vector<sim::ScenarioSpec::TileObjectSpec>& objs = spec.islands[0].objects;
        const auto obj = [&](std::int32_t x, std::int32_t y, sim::TypeId t) {
            const auto& is = spec.islands[0];
            objs.push_back(sim::ScenarioSpec::TileObjectSpec{
                static_cast<std::uint32_t>((y - is.y0) * static_cast<std::int32_t>(is.w) +
                                           (x - is.x0)),
                t.v, 0});
        };
        obj(113, 111, ht::kGrain);
        obj(113, 112, ht::kGrain);
        const auto types = ht::registry(3 * 256);
        const auto add = [&](std::int32_t x, std::int32_t y) {
            sim::ScenarioSpec::BuildingSpec b;
            b.type = ht::kSmallFarm;
            b.x = x;
            b.y = y;
            b.payload = sim::make_small_farm(types->find(ht::kSmallFarm)->prod);
            spec.islands[0].cities[0].buildings.push_back(std::move(b));
        };
        add(110, 110);
        for (int i = 0; i < k; ++i) {
            add(102 + 4 * (i % 7),
                118 + 4 * (i / 7)); // a grid of farms, none within 3 tiles of a field
        }
        auto built = sim::World::create(spec, types, sim::SimConfig{}, sim::Rules{});
        REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
        sim::World w = std::move(built).value();
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        for (int i = 0; i < 8000; ++i) {
            w.advance(100);
        }
        struct R {
            std::uint64_t searches, scanned, candidates, popped, cuts;
        };
        return R{stats.forestry.searches, stats.forestry.search_tiles_scanned,
                 stats.forestry.search_candidates, stats.forestry.search_popped,
                 stats.forestry.cuts};
    };
    const auto r0 = measure(0), r10 = measure(7), r30 = measure(21);
    MESSAGE("k=0: searches " << r0.searches << " scanned " << r0.scanned << " candidates "
                             << r0.candidates << " popped " << r0.popped << " cuts " << r0.cuts);
    MESSAGE("k=7: searches " << r10.searches << " scanned " << r10.scanned << " popped "
                             << r10.popped);
    MESSAGE("k=21: searches " << r30.searches << " scanned " << r30.scanned << " popped "
                              << r30.popped);
    // the work done for farm A is unchanged by the unrelated farms: same cuts, same walk nodes
    // popped, candidates
    CHECK(r10.cuts == r0.cuts);
    CHECK(r30.cuts == r0.cuts);
    CHECK(r10.popped == r0.popped);
    CHECK(r30.popped == r0.popped);
    // each extra farm adds a bounded cost per search: the window (7 x 7 = 49 tiles), never a scan
    // of the island
    CHECK(r10.searches > r0.searches);
    CHECK(r30.searches > r10.searches);
    CHECK(r10.scanned - r0.scanned == 49U * (r10.searches - r0.searches));
    CHECK(r30.scanned - r0.scanned == 49U * (r30.searches - r0.searches));
    CHECK(r30.candidates == r0.candidates); // no extra candidate tiles anywhere
}

namespace {

void export_log(const char* name, const sim::ReplayLog& log) {
    const char* dir = std::getenv("ANNO_POP_LIVE_OUT_DIR");
    if (dir == nullptr || *dir == '\0') {
        return;
    }
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const std::string e = sim::test::write_evidence_file(
        (std::filesystem::path(dir) / name).string(),
        std::string_view(reinterpret_cast<const char*>(bytes.value().data()),
                         bytes.value().size()));
    REQUIRE_MESSAGE(e.empty(), e);
}

} // namespace

TEST_CASE("resilience: the interruption and recovery run is recorded, replays from bytes, and a "
          "changed command diverges at the next checkpoint") {
    // Recorder over the interruption scenario: demolish the potato farm at 150 000, rebuild it at
    // 270 000, checkpoints every 40 000 ticks to 430 000. The same advance(100) stepping as every
    // run above. The log is replayed from bytes in a fresh world; logs with one changed legal
    // command are replayed too: the divergence is at the first checkpoint AFTER the changed command
    // and the earlier checkpoints still match (causal localisation).
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World w = tutorial_world(*o, true);
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    const sim::CityRef ref{sim::IslandId{kIsland7}, sim::CityNo{kCity1}};
    const sim::PlayerId owner{0};
    const auto cmd = [&](sim::Body body) {
        sim::Command c;
        c.issuer = owner;
        c.body = std::move(body);
        return c;
    };
    const std::vector<std::uint64_t> cps{110000, 150000, 190000, 270000, 310000, 350000, 430000};
    std::size_t next_cp = 0;
    bool demolished = false, rebuilt = false;
    while (w.clock().tick() < cps.back()) {
        if (!demolished && w.clock().tick() >= 150000) {
            REQUIRE(rec.submit(cmd(sim::DemolishCmd{ref, {sim::TileXY{261, 344}}})).ok());
            demolished = true;
        }
        if (!rebuilt && w.clock().tick() >= 270000) {
            REQUIRE(rec.submit(cmd(sim::BuildCmd{
                                   ref, {sim::PlacementWire{sim::TypeId{809}, 261, 344, 0, 0, 0}}}))
                        .ok());
            rebuilt = true;
        }
        rec.advance(100);
        if (next_cp < cps.size() && w.clock().tick() == cps[next_cp]) {
            rec.checkpoint();
            ++next_cp;
        }
    }
    REQUIRE(next_cp == cps.size());
    const sim::ReplayLog& log = rec.log();
    std::vector<std::size_t> cp_entries;
    std::vector<std::size_t> cmd_entries;
    for (std::size_t i = 0; i < log.entries.size(); ++i) {
        if (log.entries[i].kind == sim::ReplayEntry::Kind::Checkpoint) {
            cp_entries.push_back(i);
        } else if (log.entries[i].kind == sim::ReplayEntry::Kind::Commands) {
            cmd_entries.push_back(i);
        }
    }
    REQUIRE(cp_entries.size() == cps.size());
    REQUIRE(cmd_entries.size() == 2);
    for (std::size_t i = 0; i < cps.size(); ++i) {
        MESSAGE("checkpoint " << cps[i] << " entry " << cp_entries[i] << " hash " << std::hex
                              << log.entries[cp_entries[i]].hash);
    }
    // the pinned checkpoints (stream 129, the advance(100) schedule; a legitimate change of the
    // models re-pins them)
    constexpr std::array<std::uint64_t, 7> kPinned{
        0x75d0ff60eba4153aULL, 0x8ad338178bf6bdb2ULL, 0x137fc9ad91424c61ULL, 0x9d53b64a865a113fULL,
        0x833edcef11749aabULL, 0xf68461b9d4b4a252ULL, 0x8a665b979c6fc9f5ULL};
    for (std::size_t i = 0; i < kPinned.size(); ++i) {
        CHECK_MESSAGE(log.entries[cp_entries[i]].hash == kPinned[i], "checkpoint " << cps[i]);
    }
    export_log("supply_chain.log", log);
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    CHECK(read.value() == log);
    const auto replay = [&](const sim::ReplayLog& l) {
        auto fresh = sim::World::load(l.initial_save, o->types, {});
        REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
        auto out = sim::replay(fresh.value(), l);
        REQUIRE_MESSAGE(out.ok(), (out.ok() ? std::string() : out.error()));
        return out.value();
    };
    const sim::ReplayOutcome genuine = replay(read.value());
    CHECK(genuine.reproduced());
    CHECK(genuine.checkpoints_total == cps.size());
    CHECK(genuine.checkpoints_matched == cps.size());
    // changed command 1: the demolition names the other farm (259,352): legal, but a different
    // world from then on
    {
        sim::ReplayLog changed = read.value();
        auto* d =
            std::get_if<sim::DemolishCmd>(&changed.entries[cmd_entries[0]].commands.at(0).body);
        REQUIRE(d != nullptr);
        d->tiles = {sim::TileXY{259, 352}};
        export_log("supply_chain_changed_demolish.log", changed);
        const sim::ReplayOutcome out = replay(changed);
        REQUIRE_FALSE(out.reproduced());
        CHECK(*out.diverged_entry == cp_entries[2]); // the first checkpoint after 150 000 (190
                                                     // 000); 110 000 and 150 000 matched
        CHECK(out.checkpoints_matched == 2);
    }
    // changed command 2: the rebuild is moved one tile east (still legal; the farm stands
    // elsewhere): the first checkpoint after 270 000 differs, the earlier ones match
    {
        sim::ReplayLog changed = read.value();
        auto* b = std::get_if<sim::BuildCmd>(&changed.entries[cmd_entries[1]].commands.at(0).body);
        REQUIRE(b != nullptr);
        b->placements[0].x = 262; // the same type one tile east: legal; the farm stands elsewhere
        export_log("supply_chain_changed_rebuild.log", changed);
        const sim::ReplayOutcome out = replay(changed);
        REQUIRE_FALSE(out.reproduced());
        CHECK(*out.diverged_entry ==
              cp_entries[4]); // 310 000: the checkpoints through 270 000 matched
        CHECK(out.checkpoints_matched == 4);
    }
}

TEST_CASE(
    "resilience: two owners: an Inn's carrier never loads from the rival city's nearer source") {
    // Player 0's Inn (input ALKOHOL) at (107,107) beside the road; player 1's alcohol farm at
    // (111,103) beside the same road, nearer to the Inn than player 0's own farm at (110,113). Both
    // farms hold 960 units of alcohol (lines off, a disclosed preset). The carrier (job 0x33) must
    // load only from its own city's farm.
    sim::BuildingType inn;
    inn.id = sim::TypeId{9702};
    inn.prodtype = 66;
    inn.cls = sim::RuntimeClass::Inn;
    inn.entrances = {{3, 3}};
    inn.input_goods = {ht::goods_id("ALKOHOL")};
    inn.output_goods = {76};
    inn.action_radius = 20;
    inn.has_prod = true;
    inn.prod.kind = sim::BuildingClass::Other;
    inn.prod.input_per_step = {48, 0};
    inn.prod.max_figures = {1, 0, 0};
    inn.prod.figure_type_count = {1, 0, 0};
    inn.prod.figure_ids = {204, 0, 0, 0, 0, 0, 0, 0, 0};
    inn.prod.capacity = {5 * 256, 0};
    inn.width = 3;
    inn.height = 3;
    const auto types = ht::registry(3 * 256, {inn}, {sim::FigureType{204, 40265, 3 * 256, 0, 500}});
    sim::ScenarioSpec spec = two_city_island();
    const auto preset = [&](sim::TypeId type) {
        sim::SmallFarm f = sim::make_small_farm(types->find(type)->prod);
        f.line_on = {false, false};
        f.out = {960, 0};
        return f;
    };
    place_in(spec, 0, sim::TypeId{9702}, 107, 107, sim::Inn{});
    place_in(spec, 0, ht::kSmallFarm2, 110, 113, preset(ht::kSmallFarm2));
    place_in(spec, 1, ht::kSmallFarm2, 111, 103, preset(ht::kSmallFarm2));
    sim::Rules rules;
    rules.house_updates =
        true; // the Inn's tick runs only with the live population rules (test world only)
    auto built = sim::World::create(spec, types, sim::SimConfig{}, rules);
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    sim::World w = std::move(built).value();
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    const sim::Island& is = *w.islands()[0];
    const sim::Building* own = is.building_at(w.config().layout, sim::CityNo{0}, 110, 113);
    const sim::Building* rival = is.building_at(w.config().layout, sim::CityNo{1}, 111, 103);
    const sim::Building* ib = is.building_at(w.config().layout, sim::CityNo{0}, 107, 107);
    REQUIRE((own != nullptr && rival != nullptr && ib != nullptr));
    for (int i = 0; i < 6000; ++i) {
        w.advance(100);
    }
    const auto out_of = [&](const sim::Building* b) {
        return std::get<sim::SmallFarm>(is.find_building(b->id)->payload).out[0];
    };
    const std::uint32_t tavern = std::get<sim::Inn>(is.find_building(ib->id)->payload).tavern;
    MESSAGE("own out " << out_of(own) << " rival out " << out_of(rival) << " Inn tavern "
                       << tavern);
    CHECK(out_of(rival) == 960);
    CHECK(stats.loaded_from.count(rival->id) == 0);
    CHECK(stats.loaded_from.count(own->id) == 1); // the carrier did load, from its own city's farm
    CHECK(out_of(own) < 960);
    CHECK(tavern > 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("resilience: two owners: workers of two cities sharing a field area each credit their "
          "own farm") {
    // City 0's farm (110,108) and city 1's farm (111,103) both reach the fields (113,106) and
    // (113,107); the harvest candidate list has no owner test (docs/re/forestry.md 2.6), so either
    // farm may cut either field. Whatever each worker cuts, its cargo reaches ITS OWN home: the
    // farm's credited potatoes equal 256 per cut of its worker.
    const auto types = ht::registry(3 * 256);
    sim::ScenarioSpec spec = two_city_island();
    spec.islands[0].objects.push_back(sim::ScenarioSpec::TileObjectSpec{
        static_cast<std::uint32_t>((106 - 100) * 32 + (113 - 100)), ht::kGrain.v, 0});
    spec.islands[0].objects.push_back(sim::ScenarioSpec::TileObjectSpec{
        static_cast<std::uint32_t>((107 - 100) * 32 + (113 - 100)), ht::kGrain.v, 0});
    place_in(spec, 0, ht::kSmallFarm, 110, 108,
             sim::make_small_farm(types->find(ht::kSmallFarm)->prod));
    place_in(spec, 1, ht::kSmallFarm, 111, 103,
             sim::make_small_farm(types->find(ht::kSmallFarm)->prod));
    auto built = sim::World::create(spec, types, sim::SimConfig{}, sim::Rules{});
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    sim::World w = std::move(built).value();
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    const sim::Island& is = *w.islands()[0];
    const sim::Building* a = is.building_at(w.config().layout, sim::CityNo{0}, 110, 108);
    const sim::Building* b = is.building_at(w.config().layout, sim::CityNo{1}, 111, 103);
    REQUIRE((a != nullptr && b != nullptr));
    for (int i = 0; i < 12000; ++i) {
        w.advance(100);
    }
    const auto& la = stats.forestry.small_farms.at(a->id);
    const auto& lb = stats.forestry.small_farms.at(b->id);
    const auto raw = [&](const sim::Building* x) {
        return std::get<sim::SmallFarm>(is.find_building(x->id)->payload).raw[0];
    };
    MESSAGE("farm A raw " << raw(a) << " used " << la.consumed_88[0] << "; farm B raw " << raw(b)
                          << " used " << lb.consumed_88[0]);
    CHECK(stats.forestry.cuts == 2);
    CHECK(stats.forestry.raw_delivered_88 == 512);
    // each farm's potatoes are 256 per field its own worker cut: together exactly 512, and each is
    // a multiple of 256
    const std::uint64_t ta = raw(a) + la.consumed_88[0], tb = raw(b) + lb.consumed_88[0];
    CHECK(ta + tb == 512);
    CHECK(ta % 256 == 0);
    CHECK(tb % 256 == 0);
    CHECK((ta == 256 && tb == 256)); // two workers, two distinct fields: one each
    CHECK(w.check_invariants() == "");
}
