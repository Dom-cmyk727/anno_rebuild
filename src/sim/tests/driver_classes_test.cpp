// Stage S11 (docs/sim/state.md §15 S11, §7.1, §14.3): the driver runs the weapons
// workshop (with the owner's research as input), the fur trapper, the small farm (one
// world draw per step) and the mine (the island's deposits) through the real scheduler,
// and the golden G3 (two islands, two cities, one building of every ticking class, a mine
// over two deposits, research bytes set) is recorded as fixtures/synthetic/sim/g3.log and
// replayed. Expected values are derived by hand from docs/sim/production.md.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "golden.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

// The serials of synthetic::g3_scenario, in file order per island.
constexpr std::uint32_t kWeaver0 = 1, kFarm0 = 2, kTrapper0 = 3, kSmallFarm0 = 4, kBowMaker0 = 5,
                        kMine0 = 6, kHouse0 = 7;
constexpr std::uint32_t kBowMaker3 = 1, kWeaver3 = 2;

syn::SyntheticTypes g3_types() {
    auto t = syn::make_synthetic_types();
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
    return std::move(t).value();
}

/// The synthetic types with `edit` applied to a copy of every type (same ids and
/// classes), for the tests that need figure ids or another footprint.
syn::SyntheticTypes edited_types(void (*edit)(const syn::SyntheticTypes&, sim::BuildingType&)) {
    syn::SyntheticTypes t = g3_types();
    sim::TypeRegistry::Builder b;
    for (const sim::BuildingType& type : t.registry->types()) {
        sim::BuildingType copy = type;
        edit(t, copy);
        b.add(std::move(copy));
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    t.registry = std::move(built.value());
    return t;
}

struct G3 {
    syn::SyntheticTypes types;
    sim::World world;
    /// The G3 world (synthetic::g3_scenario on `t`, or `custom`) with `cfg`.
    static G3 make(const sim::ScenarioSpec* custom = nullptr,
                   const syn::SyntheticTypes* t = nullptr, sim::SimConfig cfg = {}) {
        syn::SyntheticTypes ty = t != nullptr ? *t : g3_types();
        const sim::ScenarioSpec spec = custom != nullptr ? *custom : syn::g3_scenario(ty);
        auto w = sim::World::create(spec, ty.registry, std::move(cfg));
        REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
        return G3{std::move(ty), std::move(w).value()};
    }
};

/// What a figure of the registry says about where it came from.
struct Sent {
    std::uint64_t home = 0;
    std::uint32_t job = 0, kind = 0, counter = 0, owner = 0, born = 0;
    friend bool operator<(const Sent& a, const Sent& b) {
        return std::tie(a.home, a.counter, a.born, a.job, a.kind, a.owner) <
               std::tie(b.home, b.counter, b.born, b.job, b.kind, b.owner);
    }
    friend bool operator==(const Sent&, const Sent&) = default;
};

std::vector<Sent> sent_figures(const sim::World& w) {
    std::vector<Sent> out;
    for (const auto& slot : w.figures().list.slots) {
        if (slot.has_value()) {
            out.push_back(Sent{slot->home.v, slot->job, slot->kind, slot->home_counter,
                               slot->owner.v, slot->born});
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

sim::Building& building(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    sim::Island& is = *sim::WorldTestAccess::islands(w)[island];
    sim::Building* b =
        sim::find_building_mut(is, sim::make_building_id(sim::IslandId{island}, serial));
    REQUIRE(b != nullptr);
    return *b;
}

template <class P> P& payload(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    P* p = std::get_if<P>(&building(w, island, serial).payload);
    REQUIRE(p != nullptr);
    return *p;
}

std::vector<sim::Deposit>& deposits(sim::World& w, std::uint32_t island = 0) {
    return sim::WorldTestAccess::islands(w)[island]->deposits;
}

/// The stream bytes of one payload or deposit (the state the field list carries; the
/// derived members shows_icons and unlocked are not in it and are compared apart).
template <class T> std::vector<std::uint8_t> bytes(const T& v) {
    sim::WriteArchive wa;
    sim::fields(wa, v);
    return wa.data();
}

const sim::ProductionType& prod(const syn::SyntheticTypes& t, sim::TypeId id) {
    const sim::BuildingType* type = t.registry->find(id);
    REQUIRE(type != nullptr);
    return type->prod;
}

std::uint32_t draws_at(const sim::SimOutput& out, sim::DrawSite site) {
    std::uint32_t n = 0;
    for (const sim::DrawRecord& r : out.draws) {
        n += r.site == site ? r.count : 0U;
    }
    return n;
}

std::uint64_t hash_events(const std::vector<sim::SimEvent>& events) {
    sim::StateHasher h;
    for (const sim::SimEvent& e : events) {
        h.u8(static_cast<std::uint8_t>(e.kind));
        h.u64(e.tick);
        h.u32(e.player.v);
        h.u64(e.building.v);
        h.u64(e.figure.v);
        h.u32(e.code);
        h.u32(static_cast<std::uint32_t>(e.x));
        h.u32(static_cast<std::uint32_t>(e.y));
    }
    return h.value();
}

void check_golden_line(const std::string& name, std::uint64_t value) {
    std::string line;
    const std::string problem = anno::test::check_golden(name, "S11", value, line);
    if (anno::test::update_mode()) {
        MESSAGE("golden line: " << line);
    }
    CHECK_MESSAGE(problem.empty(), problem);
}

/// The figures of the registry with this job.
std::size_t figures_with_job(const sim::World& w, std::uint32_t job) {
    std::size_t n = 0;
    for (const auto& slot : w.figures().list.slots) {
        n += slot.has_value() && slot->job == job ? 1U : 0U;
    }
    return n;
}

} // namespace

// ------------------------------------------------- scheduler equals direct steps

TEST_CASE("each class through the scheduler equals the direct step-function test at the "
          "same ticks") {
    // The G3 world through advance (20 000 steps of 10 ms, tick 100 000), and next to it
    // a copy of every production payload driven by calling its step function directly at
    // the ticks at which the scheduler stepped the building (its gate opened: `last`
    // moved), with the inputs written down here by hand: the owner's research as the
    // weapons workshops' `unlocked` (player 0: BOGEN only, player 1: both), the owner's
    // Volk as `shows_icons` (player 1 is given Volk 2, so its buildings show no icons), a
    // copy of island 0's deposits, the mine's footprint (1 x 1 at 110,118) and the
    // add-on's depletion, a generator copied from the world's (the small farm is the only
    // draw site before the 300 s event at tick 150 000), and what consume does with the
    // result (docs/sim/state.md §7.2): the counters it raises, the figures it asks for
    // (home, job, kind = the type's first figure id, counter, owner, birth tick) and the
    // mine's notices. The types carry distinct figure ids so a figure tells its sender.
    // After every advance the stream bytes must be equal; at the end the figures and the
    // notices.
    const syn::SyntheticTypes types =
        edited_types([](const syn::SyntheticTypes& s, sim::BuildingType& type) {
            const std::pair<sim::TypeId, std::uint32_t> ids[] = {{s.weaver, 11},
                                                                 {s.farm, 22},
                                                                 {s.fur_trapper, 33},
                                                                 {s.small_farm, 44},
                                                                 {s.bow_maker, 55}};
            for (const auto& [id, kind] : ids) {
                if (type.id == id) {
                    type.prod.figure_ids[0] = kind;
                }
            }
        });
    sim::ScenarioSpec spec = syn::g3_scenario(types);
    spec.players[1].volk = 2;
    G3 g = G3::make(&spec, &types);
    sim::World& w = g.world;
    const syn::SyntheticTypes& t = g.types;

    sim::Workshop weaver0 = payload<sim::Workshop>(w, 0, kWeaver0);
    sim::Farm farm0 = payload<sim::Farm>(w, 0, kFarm0);
    sim::Farm trapper0 = payload<sim::Farm>(w, 0, kTrapper0);
    sim::SmallFarm small0 = payload<sim::SmallFarm>(w, 0, kSmallFarm0);
    sim::WeaponsWorkshop bow0 = payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0);
    sim::Mine mine0 = payload<sim::Mine>(w, 0, kMine0);
    sim::WeaponsWorkshop bow3 = payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3);
    sim::Workshop weaver3 = payload<sim::Workshop>(w, 3, kWeaver3);
    bow3.status.shows_icons = false; // player 1, Volk 2
    weaver3.status.shows_icons = false;
    std::vector<sim::Deposit> dep = deposits(w);
    sim::WorldRng rng = w.rng();
    const sim::SmallFarmAnimals animals = w.config().small_farm_animals;
    REQUIRE(mine0.deposit.has_value());
    CHECK(*mine0.deposit == 0); // the larger of the two in reach (2 t against 1 t)

    const auto id0 = [](std::uint32_t serial) {
        return sim::make_building_id(sim::IslandId{0}, serial).v;
    };
    const auto id3 = [](std::uint32_t serial) {
        return sim::make_building_id(sim::IslandId{3}, serial).v;
    };
    std::vector<Sent> expect_sent;
    std::vector<sim::SimEvent> expect_notices, notices;

    struct Steps {
        std::uint32_t weaver0 = 0, farm0 = 0, trapper0 = 0, small0 = 0, bow0 = 0, mine0 = 0,
                      bow3 = 0, weaver3 = 0;
    } steps;
    std::uint32_t trapper_due = 0; // steps in which the trapper's step asked for a trapper

    for (int i = 0; i < 20000; ++i) {
        const std::uint16_t last_w0 = payload<sim::Workshop>(w, 0, kWeaver0).last;
        const std::uint16_t last_f0 = payload<sim::Farm>(w, 0, kFarm0).last;
        const std::uint16_t last_t0 = payload<sim::Farm>(w, 0, kTrapper0).last;
        const std::uint16_t last_s0 = payload<sim::SmallFarm>(w, 0, kSmallFarm0).last;
        const std::uint16_t last_b0 = payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0).last;
        const std::uint16_t last_m0 = payload<sim::Mine>(w, 0, kMine0).last;
        const std::uint16_t last_b3 = payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3).last;
        const std::uint16_t last_w3 = payload<sim::Workshop>(w, 3, kWeaver3).last;
        w.advance(10);
        const auto now = static_cast<std::uint32_t>(w.clock().tick());
        for (const sim::SimEvent& e : w.last_output().events) {
            if (e.kind == sim::SimEventKind::Notice) {
                notices.push_back(e);
            }
        }

        if (payload<sim::Workshop>(w, 0, kWeaver0).last != last_w0) {
            ++steps.weaver0;
            const sim::StepResult r = sim::workshop_step(weaver0, prod(t, t.weaver), now);
            if (r.send_figure) {
                ++weaver0.carriers_out;
                expect_sent.push_back(Sent{id0(kWeaver0), 0x33, 11, 0, 0, now});
            }
        }
        if (payload<sim::Farm>(w, 0, kFarm0).last != last_f0) {
            ++steps.farm0;
            const sim::StepResult r = sim::farm_step(farm0, prod(t, t.farm), now);
            if (r.send_figure) {
                ++farm0.workers_out;
                expect_sent.push_back(Sent{id0(kFarm0), 0, 22, 0, 0, now});
            }
        }
        if (payload<sim::Farm>(w, 0, kTrapper0).last != last_t0) {
            ++steps.trapper0;
            const sim::StepResult r = sim::fur_trapper_step(trapper0, prod(t, t.fur_trapper), now);
            // No prey: the driver sends no trapper, so no counter rises and no figure.
            trapper_due += r.send_figure ? 1U : 0U;
        }
        if (payload<sim::SmallFarm>(w, 0, kSmallFarm0).last != last_s0) {
            ++steps.small0;
            const sim::StepResult r = sim::small_farm_step(small0, prod(t, t.small_farm), now,
                                                           sim::DrawRef::of(rng), animals);
            if (r.send_figure) {
                ++small0.figures_out[0];
                expect_sent.push_back(Sent{id0(kSmallFarm0), 0x2a, 44, 0, 0, now});
            }
            if (r.send_animal) {
                const auto counter = static_cast<std::uint32_t>(r.animal_kind - animals.base);
                ++small0.figures_out[counter];
                expect_sent.push_back(Sent{id0(kSmallFarm0), 0x2e, r.animal_kind, counter, 0, now});
            }
        }
        if (payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0).last != last_b0) {
            ++steps.bow0;
            bow0.unlocked.assign(2, true);
            bow0.unlocked[1] = false; // ARMBRUST (research 9) is not done for player 0
            const sim::StepResult r = sim::weapons_workshop_step(bow0, prod(t, t.bow_maker), now);
            if (r.send_figure) {
                ++bow0.carriers_out;
                expect_sent.push_back(Sent{id0(kBowMaker0), 0x33, 55, 0, 0, now});
            }
        }
        if (payload<sim::Mine>(w, 0, kMine0).last != last_m0) {
            ++steps.mine0;
            const sim::StepResult r = sim::mine_step(mine0, prod(t, t.iron_mine), now, dep,
                                                     sim::MineArea{110, 118, 1, 1}, true);
            if (r.notice != 0) {
                sim::SimEvent e;
                e.kind = sim::SimEventKind::Notice;
                e.tick = now;
                e.player = sim::PlayerId{0};
                e.building = sim::make_building_id(sim::IslandId{0}, kMine0);
                e.code = r.notice;
                expect_notices.push_back(e);
            }
        }
        if (payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3).last != last_b3) {
            ++steps.bow3;
            bow3.unlocked.assign(2, true);
            const sim::StepResult r = sim::weapons_workshop_step(bow3, prod(t, t.bow_maker), now);
            if (r.send_figure) {
                ++bow3.carriers_out;
                expect_sent.push_back(Sent{id3(kBowMaker3), 0x33, 55, 0, 1, now});
            }
        }
        if (payload<sim::Workshop>(w, 3, kWeaver3).last != last_w3) {
            ++steps.weaver3;
            const sim::StepResult r = sim::workshop_step(weaver3, prod(t, t.weaver), now);
            if (r.send_figure) {
                ++weaver3.carriers_out;
                expect_sent.push_back(Sent{id3(kWeaver3), 0x33, 11, 0, 1, now});
            }
        }

        // Equal state after every advance (the payloads, the deposits, the generator).
        const bool equal = bytes(weaver0) == bytes(payload<sim::Workshop>(w, 0, kWeaver0)) &&
                           bytes(farm0) == bytes(payload<sim::Farm>(w, 0, kFarm0)) &&
                           bytes(trapper0) == bytes(payload<sim::Farm>(w, 0, kTrapper0)) &&
                           bytes(small0) == bytes(payload<sim::SmallFarm>(w, 0, kSmallFarm0)) &&
                           bytes(bow0) == bytes(payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0)) &&
                           bytes(mine0) == bytes(payload<sim::Mine>(w, 0, kMine0)) &&
                           bytes(bow3) == bytes(payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3)) &&
                           bytes(weaver3) == bytes(payload<sim::Workshop>(w, 3, kWeaver3)) &&
                           bytes(dep[0]) == bytes(deposits(w)[0]) &&
                           bytes(dep[1]) == bytes(deposits(w)[1]) &&
                           rng.cursor() == w.rng().cursor();
        if (!equal) {
            FAIL_CHECK("the scheduler and the direct steps differ after advance " << i << " (tick "
                                                                                  << now << ")");
            break;
        }
    }
    CHECK(w.clock().tick() == 100000);
    // The driver's derived inputs equal the hand-written ones.
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0).unlocked == bow0.unlocked);
    CHECK(payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3).unlocked == bow3.unlocked);
    CHECK_FALSE(payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3).status.shows_icons);
    CHECK_FALSE(payload<sim::Workshop>(w, 3, kWeaver3).status.shows_icons);
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0).status.shows_icons);
    // The figures the scheduler made are the ones the direct steps asked for.
    std::sort(expect_sent.begin(), expect_sent.end());
    CHECK(sent_figures(w) == expect_sent);
    CHECK(expect_sent.size() >= 6); // two weavers, two bow makers, the farm, the small farm
    // The mine's notices: the same building, owner, code and tick.
    REQUIRE(notices.size() == expect_notices.size());
    for (std::size_t i = 0; i < notices.size(); ++i) {
        CHECK(notices[i].building == expect_notices[i].building);
        CHECK(notices[i].player == expect_notices[i].player);
        CHECK(notices[i].code == expect_notices[i].code);
        CHECK(notices[i].tick == expect_notices[i].tick);
    }
    // Every building stepped about once per 500 ticks: 200 gates in 100 000 ticks, the
    // first one at the first visit after tick 500, so the scheduler may be a few behind.
    for (const std::uint32_t n : {steps.weaver0, steps.farm0, steps.trapper0, steps.small0,
                                  steps.bow0, steps.mine0, steps.bow3, steps.weaver3}) {
        CHECK(n >= 196);
        CHECK(n <= 200);
    }
    // The cases the test is meant to cover did happen: the trapper wanted to send, the
    // mine ran its deposits dry (one notice 0x19).
    CHECK(trapper_due > 0);
    CHECK(expect_notices.size() == 1);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("the driver refreshes the status icons of every class from the owner's Volk") {
    // StatusTracker::shows_icons = the city owner's Volk is 1 (OQ-033), refreshed before
    // every step. Player 0 with Volk 2: after one gate of every building none of player
    // 0's buildings shows icons, while player 1 (Volk 1) keeps them.
    const syn::SyntheticTypes t = g3_types();
    sim::ScenarioSpec spec = syn::g3_scenario(t);
    spec.players[0].volk = 2;
    G3 g = G3::make(&spec);
    sim::World& w = g.world;
    for (int i = 0; i < 200; ++i) { // tick 1 000: every building passed its first gate
        w.advance(10);
    }
    CHECK(payload<sim::Workshop>(w, 0, kWeaver0).last != 0);
    CHECK(payload<sim::Mine>(w, 0, kMine0).last != 0);
    CHECK_FALSE(payload<sim::Workshop>(w, 0, kWeaver0).status.shows_icons);
    CHECK_FALSE(payload<sim::Farm>(w, 0, kFarm0).status.shows_icons);
    CHECK_FALSE(payload<sim::Farm>(w, 0, kTrapper0).status.shows_icons);
    CHECK_FALSE(payload<sim::SmallFarm>(w, 0, kSmallFarm0).status.shows_icons);
    CHECK_FALSE(payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0).status.shows_icons);
    CHECK_FALSE(payload<sim::Mine>(w, 0, kMine0).status.shows_icons);
    CHECK(payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3).status.shows_icons);
    CHECK(payload<sim::Workshop>(w, 3, kWeaver3).status.shows_icons);
    // And back: Volk 1 shows them again at the next step.
    sim::WorldTestAccess::players(w).slots[0]->volk = 1;
    for (int i = 0; i < 200; ++i) {
        w.advance(10);
    }
    CHECK(payload<sim::Farm>(w, 0, kTrapper0).status.shows_icons);
    CHECK(payload<sim::SmallFarm>(w, 0, kSmallFarm0).status.shows_icons);
    CHECK(payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0).status.shows_icons);
    CHECK(payload<sim::Mine>(w, 0, kMine0).status.shows_icons);
}

// ------------------------------------------------------------ weapons workshop

TEST_CASE("weapons workshop unlocked from research") {
    const syn::SyntheticTypes t = g3_types();
    const sim::BuildingType& bow = *t.registry->find(t.bow_maker);
    G3 g = G3::make();
    sim::World& w = g.world;
    sim::PlayerTable& players = sim::WorldTestAccess::players(w);
    const sim::PlayersView view(players);

    // refresh_unlocked: one entry per line, from the Forsch research id of the line's goods
    // (BOGEN 4, ARMBRUST 9) and bit 1 of the owner's research state byte.
    struct Row {
        std::uint8_t s4, s9;
        bool line0, line1;
    };
    for (const Row& row : std::vector<Row>{{0, 0, false, false},
                                           {3, 0, true, false},
                                           {3, 1, true, false},
                                           {0, 3, false, true},
                                           {2, 2, true, true},
                                           {1, 1, false, false},
                                           {0xFD, 0xFF, false, true}}) {
        INFO("state bytes " << int{row.s4} << " " << int{row.s9});
        players.slots[0]->research.state[4] = row.s4;
        players.slots[0]->research.state[9] = row.s9;
        sim::WeaponsWorkshop ww = sim::make_weapons_workshop(bow.prod);
        ww.unlocked.clear(); // as after a load: derived, not saved
        sim::refresh_unlocked(ww, bow, w.types(), view, sim::PlayerId{0});
        REQUIRE(ww.unlocked.size() == 2);
        CHECK(ww.unlocked[0] == row.line0);
        CHECK(ww.unlocked[1] == row.line1);
    }
    {
        // A player that is not there has researched nothing; goods without a research id
        // (a weapons workshop making cloth and bread) need none.
        sim::WeaponsWorkshop ww = sim::make_weapons_workshop(bow.prod);
        sim::refresh_unlocked(ww, bow, w.types(), view, sim::PlayerId{7});
        CHECK(ww.unlocked == std::vector<bool>{false, false});
        sim::TypeRegistry::Builder b;
        b.add_weapons_workshop(sim::TypeId{9500}, "STOFFE", "NAHRUNG", "HOLZ", "SEILE", 9, 5);
        auto reg = b.build();
        REQUIRE(reg.ok());
        const sim::BuildingType& plain = *reg.value()->find(sim::TypeId{9500});
        sim::WeaponsWorkshop pw = sim::make_weapons_workshop(plain.prod);
        sim::refresh_unlocked(pw, plain, *reg.value(), view, sim::PlayerId{7});
        CHECK(pw.unlocked == std::vector<bool>{true, true});
        // A payload with more lines than the type has goods: the extra line names goods 0.
        sim::WeaponsWorkshop wide = sim::make_weapons_workshop(bow.prod);
        wide.out.assign(3, 0);
        players.slots[0]->research.state[4] = 3;
        players.slots[0]->research.state[9] = 0;
        sim::refresh_unlocked(wide, bow, w.types(), view, sim::PlayerId{0});
        CHECK(wide.unlocked == std::vector<bool>{true, false, true});
    }

    // Through the scheduler (the G3 world, put back to the scenario's research): player 0's
    // bow maker works BOGEN only, a full step of 9 per step; player 1's works both lines, 4
    // each (share 64 of 128, docs/sim/production.md). Research done mid-run counts at the
    // next step.
    G3 h = G3::make();
    sim::World& v = h.world;
    for (int i = 0; i < 2000; ++i) { // tick 10 000: about 20 steps
        v.advance(10);
    }
    const auto& b0 = payload<sim::WeaponsWorkshop>(v, 0, kBowMaker0);
    const auto& b3 = payload<sim::WeaponsWorkshop>(v, 3, kBowMaker3);
    const std::uint32_t steps0 = b0.out[0] / 9;
    CHECK(steps0 >= 17);
    CHECK(b0.out[0] == 9 * steps0);
    CHECK(b0.out[1] == 0);
    CHECK(b0.in[0] == 5 * sim::kTon - 8 * steps0);
    CHECK(b0.in[1] == 5 * sim::kTon - 4 * steps0);
    CHECK(b0.unlocked == std::vector<bool>{true, false});
    const std::uint32_t steps3 = b3.out[0] / 4;
    CHECK(steps3 >= 17);
    CHECK(b3.out[1] == 4 * steps3);
    CHECK(b3.in[0] == 5 * sim::kTon - 8 * steps3);
    CHECK(b3.unlocked == std::vector<bool>{true, true});
    // Player 0 researches ARMBRUST (state byte 9 gets bit 1): from the next step on its bow
    // maker shares the step between the lines, 4 and 4.
    sim::WorldTestAccess::players(v).slots[0]->research.state[9] = 3;
    const std::uint32_t out0 = b0.out[0];
    for (int i = 0; i < 2000; ++i) { // tick 20 000: about 20 more steps
        v.advance(10);
    }
    CHECK(b0.unlocked == std::vector<bool>{true, true});
    const std::uint32_t later = b0.out[1] / 4;
    CHECK(later >= 17);
    CHECK(b0.out[0] == out0 + 4 * later);
    CHECK(b0.in[0] == 5 * sim::kTon - 8 * (steps0 + later));
    // And research lost again (a state byte without bit 1) locks the line at once.
    sim::WorldTestAccess::players(v).slots[0]->research.state[4] = 1;
    sim::WorldTestAccess::players(v).slots[0]->research.state[9] = 1;
    const std::uint16_t last_before = b0.last;
    const std::vector<std::uint32_t> in_before = b0.in;
    for (int i = 0; i < 400; ++i) { // tick 22 000: about 4 gates, no working line
        v.advance(10);
    }
    CHECK(b0.unlocked == std::vector<bool>{false, false});
    CHECK(b0.last != last_before); // the gate still opened
    CHECK(b0.out[0] == out0 + 4 * later);
    CHECK(b0.out[1] == 4 * later);
    CHECK(b0.in == in_before);
}

TEST_CASE("weapons workshop save load advance") {
    // `unlocked` is derived: not in the save, empty after a load, sized and filled by the
    // first tick (refresh_unlocked assigns before it writes), and the loaded world runs on
    // equal to the live one.
    G3 g = G3::make();
    sim::World& live = g.world;
    for (int i = 0; i < 5000; ++i) { // tick 25 000
        live.advance(10);
    }
    REQUIRE(payload<sim::WeaponsWorkshop>(live, 0, kBowMaker0).unlocked.size() == 2);
    const std::vector<std::uint8_t> saved = live.save();
    auto loaded_r = sim::World::load(saved, g.types.registry);
    REQUIRE_MESSAGE(loaded_r.ok(), (loaded_r.ok() ? std::string() : loaded_r.error()));
    sim::World loaded = std::move(loaded_r).value();
    CHECK(loaded.state_hash() == live.state_hash());
    CHECK(loaded.save() == saved);
    CHECK(payload<sim::WeaponsWorkshop>(loaded, 0, kBowMaker0).unlocked.empty());
    CHECK(payload<sim::WeaponsWorkshop>(loaded, 3, kBowMaker3).unlocked.empty());
    // The first tick that reaches a weapons workshop sizes `unlocked` before it writes:
    // run until the building scheduler has reached both (it visits a share of each list
    // per city visit; one 500-tick gate period is plenty). The hashes stay equal meanwhile.
    int advances = 0;
    while (advances < 200 &&
           (payload<sim::WeaponsWorkshop>(loaded, 0, kBowMaker0).unlocked.empty() ||
            payload<sim::WeaponsWorkshop>(loaded, 3, kBowMaker3).unlocked.empty())) {
        loaded.advance(10);
        live.advance(10);
        ++advances;
        REQUIRE(loaded.state_hash() == live.state_hash());
    }
    CHECK(advances < 200);
    CHECK(payload<sim::WeaponsWorkshop>(loaded, 0, kBowMaker0).unlocked ==
          std::vector<bool>{true, false});
    CHECK(payload<sim::WeaponsWorkshop>(loaded, 3, kBowMaker3).unlocked ==
          std::vector<bool>{true, true});
    CHECK(loaded.state_hash() == live.state_hash());
    // 10 000 ticks more on both: equal at every advance.
    bool equal = true;
    for (int i = 0; i < 2000 && equal; ++i) {
        loaded.advance(10);
        live.advance(10);
        equal = loaded.state_hash() == live.state_hash();
    }
    CHECK(equal);
    CHECK(loaded.save() == live.save());
    CHECK(loaded.check_invariants() == "");
}

// -------------------------------------------------------------------- the mine

TEST_CASE("mine deposit by index and depletion only in the add-on") {
    // Island 0 of G3 holds two EISENERZ deposits in reach of the 1 x 1 iron mine at
    // (110, 118): index 0 at (110, 120) with 2 t (512) and index 1 at (112, 121) with 1 t
    // (256), both without the reserve flag. The iron mine is shallow (no research 0x39), 21
    // per step, capacity 5 t (1280).
    //
    // Add-on edition (world setting 14 bit 1, the synthetic default 3): the mine takes
    // deposit 0 (the larger), uses it up in 25 steps (24 x 21 = 504, then the last 8 short:
    // search again, deposit 1 holds more), moves to deposit 1, uses it up in 13 steps (12 x
    // 21 = 252, then 4: search again, nothing left, notice 0x19 for iron ore without deep
    // ore), and from then on reports "missing input" on deposit 0 (the first candidate of
    // the search, both empty). Output 768 in all.
    {
        G3 g = G3::make();
        sim::World& w = g.world;
        REQUIRE(w.settings().depletes());
        std::vector<std::size_t> indexes{*payload<sim::Mine>(w, 0, kMine0).deposit};
        std::vector<sim::SimEvent> notices;
        std::uint32_t steps = 0;
        std::uint32_t step_of_move = 0, step_of_back = 0, step_of_notice = 0;
        for (int i = 0; i < 6000; ++i) { // tick 30 000: about 60 steps
            const std::uint16_t last = payload<sim::Mine>(w, 0, kMine0).last;
            w.advance(10);
            const sim::Mine& m = payload<sim::Mine>(w, 0, kMine0);
            steps += m.last != last ? 1U : 0U;
            REQUIRE(m.deposit.has_value());
            if (*m.deposit != indexes.back()) {
                indexes.push_back(*m.deposit);
                if (step_of_move == 0) {
                    step_of_move = steps;
                } else {
                    step_of_back = steps;
                }
            }
            for (const sim::SimEvent& e : w.last_output().events) {
                if (e.kind == sim::SimEventKind::Notice) {
                    notices.push_back(e);
                    step_of_notice = steps;
                }
            }
        }
        const sim::Mine& m = payload<sim::Mine>(w, 0, kMine0);
        CHECK(steps >= 57);
        CHECK(indexes == std::vector<std::size_t>{0, 1, 0});
        CHECK(step_of_move == 25);
        CHECK(step_of_back == 38);
        CHECK(step_of_notice == 38);
        CHECK(m.out == 768);
        CHECK(deposits(w)[0].remaining == 0);
        CHECK(deposits(w)[1].remaining == 0);
        CHECK(m.status.code == sim::Status::MissingInput);
        CHECK(m.blocked);
        REQUIRE(notices.size() == 1);
        CHECK(notices[0].code == 0x19);
        CHECK(notices[0].building == sim::make_building_id(sim::IslandId{0}, kMine0));
        CHECK(notices[0].player == sim::PlayerId{0});
        // The index survives a save and a load and the mine stays dry.
        auto back = sim::World::load(w.save(), g.types.registry);
        REQUIRE(back.ok());
        CHECK(*payload<sim::Mine>(back.value(), 0, kMine0).deposit == 0);
        CHECK(back.value().state_hash() == w.state_hash());
    }
    // Base edition (property 14 = 1, ANNO): nothing is used up, the mine stays on deposit
    // 0 and makes 21 per step until its store is full (61 steps: 1281 >= 1280), no notice.
    {
        const syn::SyntheticTypes t = g3_types();
        sim::ScenarioSpec spec = syn::g3_scenario(t);
        spec.settings.prop14 = 1;
        G3 g = G3::make(&spec);
        sim::World& w = g.world;
        REQUIRE_FALSE(w.settings().depletes());
        std::size_t notices = 0;
        for (int i = 0; i < 8000; ++i) { // tick 40 000: about 80 steps
            w.advance(10);
            for (const sim::SimEvent& e : w.last_output().events) {
                notices += e.kind == sim::SimEventKind::Notice ? 1U : 0U;
            }
        }
        const sim::Mine& m = payload<sim::Mine>(w, 0, kMine0);
        CHECK(*m.deposit == 0);
        CHECK(deposits(w)[0].remaining == 2 * sim::kTon);
        CHECK(deposits(w)[1].remaining == 1 * sim::kTon);
        CHECK(m.out == 61 * 21);
        CHECK(m.status.code == sim::Status::StorageFull);
        CHECK(notices == 0);
    }
    // The reserve flag in the add-on: the shallow iron mine never touches the last 1 600 t;
    // a deposit of 1 600 t + 30 gives 21 and then 9, and nothing after.
    {
        const syn::SyntheticTypes t = g3_types();
        sim::ScenarioSpec spec = syn::g3_scenario(t);
        auto& d = spec.islands[0].deposits;
        d.resize(1);
        d[0].reserve = true;
        d[0].remaining = sim::kDepositReserve + 30;
        G3 g = G3::make(&spec);
        sim::World& w = g.world;
        std::vector<std::uint32_t> notices;
        for (int i = 0; i < 2000; ++i) { // tick 10 000
            w.advance(10);
            for (const sim::SimEvent& e : w.last_output().events) {
                if (e.kind == sim::SimEventKind::Notice) {
                    notices.push_back(e.code);
                }
            }
        }
        CHECK(payload<sim::Mine>(w, 0, kMine0).out == 30);
        CHECK(deposits(w)[0].remaining == sim::kDepositReserve);
        // Nothing shallow left, deep ore left: notice 0x15b.
        CHECK(notices == std::vector<std::uint32_t>{0x15b});
    }
}

// --------------------------------------------------------------- the small farm

TEST_CASE("small farm draws once per step through the world") {
    // The small farm is the only draw site of G3 before tick 150 000: every advance draws
    // exactly as often as the small farm stepped with line 1 on (the draw follows the gate,
    // whatever the limits say), and the world's generator moves by that much.
    G3 g = G3::make();
    sim::World& w = g.world;
    const std::uint32_t cursor0 = w.rng().cursor();
    std::uint32_t steps = 0, draws = 0;
    bool per_advance = true;
    for (int i = 0; i < 6000; ++i) { // tick 30 000: about 60 steps
        const std::uint16_t last = payload<sim::SmallFarm>(w, 0, kSmallFarm0).last;
        w.advance(10);
        const std::uint32_t stepped =
            payload<sim::SmallFarm>(w, 0, kSmallFarm0).last != last ? 1U : 0U;
        const std::uint32_t drawn = draws_at(w.last_output(), sim::DrawSite::SmallFarmAnimal);
        per_advance = per_advance && drawn == stepped && w.last_output().draws.size() == drawn;
        steps += stepped;
        draws += drawn;
    }
    CHECK(per_advance);
    CHECK(steps >= 57);
    CHECK(draws == steps);
    const std::uint32_t len = static_cast<std::uint32_t>(w.rng().table().size());
    CHECK(w.rng().cursor() == (cursor0 + draws) % len);
    // The animals it sent are figures with job 0x2e, their kinds the counters 1..4 count,
    // and never more than max_figures(0) = 3 together.
    const sim::SmallFarm& f = payload<sim::SmallFarm>(w, 0, kSmallFarm0);
    std::uint32_t animals = 0;
    for (std::size_t i = 1; i < f.figures_out.size(); ++i) {
        animals += f.figures_out[i];
    }
    CHECK(animals >= 1);
    CHECK(animals <= 3);
    CHECK(figures_with_job(w, 0x2e) == animals);
    // The line-0 worker (job 0x2a) went out once line 0's 5 t of potatoes were gone (10
    // steps of 128).
    CHECK(f.figures_out[0] == 1);
    CHECK(figures_with_job(w, 0x2a) == 1);

    // Line 1 off: the steps go on, nothing is drawn. Suspended: nothing is drawn either.
    for (const bool suspend : {false, true}) {
        INFO("suspended " << suspend);
        G3 h = G3::make();
        sim::World& v = h.world;
        auto& sf = payload<sim::SmallFarm>(v, 0, kSmallFarm0);
        if (suspend) {
            sf.suspended = true;
        } else {
            sf.line_on[1] = false;
        }
        const std::uint32_t c0 = v.rng().cursor();
        for (int i = 0; i < 2000; ++i) {
            v.advance(10);
        }
        CHECK(sf.last != 0);
        CHECK(v.rng().cursor() == c0);
    }
}

TEST_CASE("small farm animals come from the configured animal table") {
    // SimConfig::small_farm_animals reaches the step: a table that can only pick kind 110
    // (counter 4, limit 1) sends animals of kind 110 only: two, because a step in which line 1
    // made something counts one animal fewer against the limit (D 0x1006f855-0x1006f86b).
    sim::SimConfig cfg;
    cfg.small_farm_animals.kind_by_draw = {110};
    const syn::SyntheticTypes t = g3_types();
    G3 g = G3::make(nullptr, &t, cfg);
    sim::World& w = g.world;
    const std::uint32_t c0 = w.rng().cursor();
    for (int i = 0; i < 6000; ++i) { // tick 30 000
        w.advance(10);
    }
    const auto& f = payload<sim::SmallFarm>(w, 0, kSmallFarm0);
    CHECK(f.figures_out == std::vector<std::uint32_t>{1, 0, 0, 0, 2});
    std::size_t animals = 0;
    for (const auto& slot : w.figures().list.slots) {
        if (slot.has_value() && slot->job == 0x2e) {
            ++animals;
            CHECK(slot->kind == 110);
            CHECK(slot->home_counter == 4);
        }
    }
    CHECK(animals == 2);
    // The draws go on at every step whatever the table holds.
    CHECK(w.rng().cursor() != c0);
}

TEST_CASE("the mine searches from its rotated footprint") {
    // An iron mine of 1 x 4 tiles placed with rotation 1 covers 4 x 1 tiles: x 110..113,
    // y 118 (mine_area, D 0x1006ddf0 builds the search rectangle from the position and the
    // footprint by rotation). Deposit 0 at (110, 120) with 2 t is in reach of any footprint;
    // deposit 1 at (120, 118) with 1 t is 7 columns beyond the rotated footprint (in reach,
    // half-width 7 in row 0) but 10 beyond an unrotated or 1 x 1 one (out of reach). The
    // mine takes deposit 0 (the larger), and when it is used up finds deposit 1 only
    // through the rotated footprint: 768 out in all.
    const syn::SyntheticTypes t =
        edited_types([](const syn::SyntheticTypes& s, sim::BuildingType& type) {
            if (type.id == s.iron_mine) {
                type.width = 1;
                type.height = 4;
            }
        });
    sim::ScenarioSpec spec = syn::g3_scenario(t);
    auto& is0 = spec.islands[0];
    is0.deposits[1].x = 120;
    is0.deposits[1].y = 118;
    for (auto& b : is0.cities[0].buildings) {
        if (b.type == t.iron_mine) {
            b.rotation = 1;
        }
    }
    G3 g = G3::make(&spec, &t);
    sim::World& w = g.world;
    REQUIRE(*payload<sim::Mine>(w, 0, kMine0).deposit == 0);
    std::vector<std::size_t> indexes{0};
    for (int i = 0; i < 6000; ++i) { // tick 30 000: about 60 steps
        w.advance(10);
        const std::size_t now = *payload<sim::Mine>(w, 0, kMine0).deposit;
        if (now != indexes.back()) {
            indexes.push_back(now);
        }
    }
    CHECK(indexes == std::vector<std::size_t>{0, 1, 0});
    CHECK(payload<sim::Mine>(w, 0, kMine0).out == 768);
    CHECK(deposits(w)[1].remaining == 0);
}

// ---------------------------------------------------------------- the fur trapper

TEST_CASE("fur trapper sends nothing without prey") {
    // The trapper works its 2 t of raw stock (26 out of 25 per step) and then wants a
    // trapper at every step; the step function says so, but the radius search finds no
    // animal (the sim has no wildlife, O-12, O-25), so no trapper leaves: no counter, no
    // figure, no spawn.
    G3 g = G3::make();
    sim::World& w = g.world;
    for (int i = 0; i < 20000; ++i) { // tick 100 000
        w.advance(10);
    }
    const sim::Farm& f = payload<sim::Farm>(w, 0, kTrapper0);
    // 20 full steps use 500 of 512 and make 520; at 12 left the ratio is 61 < 64: "missing
    // input" (input before capacity), and from then on a trapper is due at every step.
    CHECK(f.out == 20 * 26);
    CHECK(f.raw == 2 * sim::kTon - 20 * 25);
    CHECK(f.status.code == sim::Status::MissingInput);
    CHECK(f.blocked);
    CHECK(f.workers_out == 0);
    CHECK(figures_with_job(w, 0x0d) == 0);
    for (const auto& slot : w.figures().list.slots) {
        if (slot.has_value()) {
            CHECK(slot->home != sim::make_building_id(sim::IslandId{0}, kTrapper0));
        }
    }
    // The same building with no raw stock: the step function wants a trapper at every
    // step (ratio 0 < 128, none out), tick_building still sends none.
    sim::Building& b = building(w, 0, kTrapper0);
    auto& fp = std::get<sim::Farm>(b.payload);
    fp.raw = 0;
    fp.out = 0;
    sim::Farm direct = fp;
    const sim::BuildingType& type = *g.types.registry->find(g.types.fur_trapper);
    const sim::StepResult want = sim::fur_trapper_step(direct, type.prod, 100500);
    CHECK(want.stepped);
    CHECK(want.send_figure);
    sim::GlobalDraw global(sim::WorldTestAccess::rng(w));
    sim::CountingDraw draw(global);
    const sim::TickContext ctx{100500,
                               100500,
                               w.config(),
                               w.rules(),
                               w.settings(),
                               w.types(),
                               sim::PlayersView(w.players()),
                               sim::IslandsView(w.islands()),
                               draw,
                               w.seams()};
    sim::Effects fx;
    sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
    sim::tick_building(ctx, is, *is.cities[0], b, fx);
    CHECK(fp.last == direct.last);
    CHECK(fp.workers_out == 0);
    CHECK(fx.spawns.empty());
    CHECK(bytes(fp) == bytes(direct));
}

// ------------------------------------------------------------------ the golden G3

namespace {

std::filesystem::path g3_path() {
    return std::filesystem::path(ANNO_FIXTURES_DIR) / "sim" / "g3.log";
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(in),
                                     std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("G3 every production class through advance equals golden") {
    // G3 (docs/sim/state.md §14.3): synthetic::g3_scenario, 2 000 single-player advances of
    // 100 ms (tick 100 000), a checkpoint before the first and after every 100th advance,
    // recorded with the Recorder as fixtures/synthetic/sim/g3.log.
    G3 g = G3::make();
    sim::World& w = g.world;
    REQUIRE(w.check_invariants() == "");
    const std::uint32_t cursor0 = w.rng().cursor();
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    rec.checkpoint();
    sim::StateHasher checkpoints;
    std::uint32_t draws = 0;
    std::vector<sim::SimEvent> ui;
    for (int i = 1; i <= 2000; ++i) {
        rec.advance(100);
        draws += draws_at(w.last_output(), sim::DrawSite::SmallFarmAnimal);
        if (i % 100 == 0) {
            rec.checkpoint();
            checkpoints.u64(w.state_hash());
            REQUIRE(w.check_invariants() == "");
        }
    }
    ui = w.drain_events();
    REQUIRE(w.clock().tick() == 100000);

    // Semantic asserts, derived by hand from docs/sim/production.md (independent of the
    // hash). Every building had its 200 gates; each ran out of input or space long before.
    // The weavers: F2, 116 steps from 5 t of wool (out 696, in 4), then "missing input".
    for (const auto& [island, serial] : {std::pair{0U, kWeaver0}, std::pair{3U, kWeaver3}}) {
        const auto& wk = payload<sim::Workshop>(w, island, serial);
        CHECK(wk.out[0] == 696);
        CHECK(wk.in[0] == 4);
        CHECK(wk.status.code == sim::Status::MissingInput);
        CHECK(wk.carriers_out == 1);
    }
    // The grain farm: F6, 16 steps (out 592, raw 0), then a worker that never returns.
    const auto& farm = payload<sim::Farm>(w, 0, kFarm0);
    CHECK(farm.out == 592);
    CHECK(farm.raw == 0);
    CHECK(farm.workers_out == 1);
    // The fur trapper: 20 steps from 2 t (out 520, raw 12), then "missing input" and a
    // trapper due at every step that finds no prey: none is sent.
    const auto& trapper = payload<sim::Farm>(w, 0, kTrapper0);
    CHECK(trapper.out == 520);
    CHECK(trapper.raw == 12);
    CHECK(trapper.workers_out == 0);
    CHECK(trapper.status.code == sim::Status::MissingInput);
    // The small farm: line 0 makes 13 from 128 for 10 steps (out 130, raw 0) and then sends
    // its worker; line 1 makes 4 from 50 for 25 steps, then 2 from 29 at ratio 76 (out 102,
    // raw 1), then "missing input". At most 3 animals together.
    const auto& small = payload<sim::SmallFarm>(w, 0, kSmallFarm0);
    CHECK(small.out == std::vector<std::uint32_t>{130, 102});
    CHECK(small.raw == std::vector<std::uint32_t>{0, 1});
    CHECK(small.figures_out[0] == 1);
    std::uint32_t animals = 0;
    for (std::size_t i = 1; i < small.figures_out.size(); ++i) {
        animals += small.figures_out[i];
    }
    // Three animals, max_figures(0): two of kind 107 (limit 2) and one of 109 (limit 1), as
    // the world's draws picked them (pinned like the goldens).
    CHECK(small.figures_out == std::vector<std::uint32_t>{1, 2, 0, 1, 0});
    CHECK(animals == 3);
    // Player 0's bow maker: BOGEN only, 9 per step; 143 steps reach the capacity (1287 >=
    // 1280), using 8 wood and 4 ropes each; line 1 is locked and stays empty.
    const auto& bow0 = payload<sim::WeaponsWorkshop>(w, 0, kBowMaker0);
    CHECK(bow0.out == std::vector<std::uint32_t>{1287, 0});
    CHECK(bow0.in == std::vector<std::uint32_t>{1280 - 143 * 8, 1280 - 143 * 4});
    CHECK(bow0.status.code == sim::Status::StorageFull);
    CHECK(bow0.carriers_out == 1);
    // Player 1's bow maker: both lines, 4 each, 160 steps until the wood is gone.
    const auto& bow3 = payload<sim::WeaponsWorkshop>(w, 3, kBowMaker3);
    CHECK(bow3.out == std::vector<std::uint32_t>{640, 640});
    CHECK(bow3.in == std::vector<std::uint32_t>{0, 640});
    CHECK(bow3.status.code == sim::Status::MissingInput);
    // The iron mine: 2 t and 1 t used up (768), dry, back on deposit 0.
    const auto& mine = payload<sim::Mine>(w, 0, kMine0);
    CHECK(mine.out == 768);
    CHECK(*mine.deposit == 0);
    CHECK(deposits(w)[0].remaining == 0);
    CHECK(deposits(w)[1].remaining == 0);
    // The pioneer house is inert: `last` untouched.
    CHECK(std::get<sim::Inert>(building(w, 0, kHouse0).payload).last == 0);
    // One draw per small-farm step, and nothing else draws before tick 150 000: 199 steps in
    // 100 000 ticks (one gate per 500 ticks; the scheduler reaches the farm a little after
    // each gate opens, so the 200th falls after the end). Pinned like the goldens.
    CHECK(draws == 199);
    const std::uint32_t len = static_cast<std::uint32_t>(w.rng().table().size());
    CHECK(w.rng().cursor() == (cursor0 + draws) % len);
    // UI events: one Notice 0x19 (the mine), no figure with the trapper's job.
    std::size_t notices = 0;
    for (const sim::SimEvent& e : ui) {
        if (e.kind == sim::SimEventKind::Notice) {
            ++notices;
            CHECK(e.code == 0x19);
        }
    }
    CHECK(notices == 1);
    CHECK(figures_with_job(w, 0x0d) == 0);
    CHECK(figures_with_job(w, 0x2e) == animals);

    // The log is byte-identical to the committed one (ANNO_UPDATE_GOLDEN=1 rewrites it).
    const sim::ReplayLog log = rec.log();
    auto written = sim::write_log(log);
    REQUIRE_MESSAGE(written.ok(), (written.ok() ? std::string() : written.error()));
    if (anno::test::update_mode()) {
        std::ofstream out(g3_path(), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(written.value().data()),
                  static_cast<std::streamsize>(written.value().size()));
        MESSAGE("wrote " << g3_path().string());
    }
    const std::vector<std::uint8_t> committed = read_bytes(g3_path());
    CHECK_MESSAGE(committed == written.value(), "the recorded G3 log differs from "
                                                    << g3_path().string()
                                                    << " (ANNO_UPDATE_GOLDEN=1 rewrites it)");
    // The committed log replays on a world rebuilt from its initial save: 21 of 21
    // checkpoints.
    auto read = sim::read_log(committed);
    REQUIRE_MESSAGE(read.ok(), (read.ok() ? std::string() : read.error()));
    auto fresh = sim::World::load(read.value().initial_save, g.types.registry);
    REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
    sim::World replayed = std::move(fresh).value();
    auto outcome = sim::replay(replayed, read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().reproduced());
    CHECK(outcome.value().checkpoints_total == 21);
    CHECK(outcome.value().checkpoints_matched == 21);
    CHECK(replayed.state_hash() == w.state_hash());

    // Goldens (self-baselined, goldens.txt; the asserts above do not depend on them).
    check_golden_line("s11_g3_checkpoints", checkpoints.value());
    check_golden_line("s11_g3_end", w.state_hash());
    check_golden_line("s11_g3_events", hash_events(ui));
}
