// The figure registry's growth and its removal path as the original's (task T22,
// docs/sim/state.md §4.9, §5.9, §9.6; OQ-131 and docs/re/annowelt.md "Unit registry"):
// the registry grows by at least SimConfig::figure_min_growth (100), a figure releases
// its home counter once at its end (end_figure) and waits on the expiry list for phase C.
// Every expected number is derived by hand from the spec and the AnnoWelt.dll addresses
// it cites, never from the code under test.
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <doctest/doctest.h>

#include <anno/sim/driver.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

syn::SyntheticTypes types() {
    auto t = syn::make_synthetic_types();
    REQUIRE_MESSAGE(t.ok(), (t.ok() ? std::string() : t.error()));
    return std::move(t).value();
}

sim::World make(const sim::ScenarioSpec& spec, const syn::SyntheticTypes& t, sim::Seams seams = {},
                sim::Rules rules = {}, sim::SimConfig cfg = {}) {
    auto w = sim::World::create(spec, t.registry, std::move(cfg), rules, std::move(seams));
    REQUIRE_MESSAGE(w.ok(), (w.ok() ? std::string() : w.error()));
    return std::move(w).value();
}

constexpr sim::BuildingId bid(std::uint32_t island, std::uint32_t serial) {
    return sim::BuildingId{(std::uint64_t{island} << 32) | serial};
}

// The serials of synthetic::g4_scenario: island 0, city 0.
constexpr std::uint32_t kWeaver = 1;
constexpr std::uint32_t kFarm = 2;

sim::Building& building(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    sim::Building* b =
        sim::find_building_mut(*sim::WorldTestAccess::islands(w)[island], bid(island, serial));
    REQUIRE(b != nullptr);
    return *b;
}

template <class T> T& payload(sim::World& w, std::uint32_t island, std::uint32_t serial) {
    T* p = std::get_if<T>(&building(w, island, serial).payload);
    REQUIRE(p != nullptr);
    return *p;
}

/// The G4 world with its two buildings switched off (no production, no carrier of their
/// own): the tests place their figures by hand.
sim::World quiet_g4(const syn::SyntheticTypes& t, sim::Seams seams = {}, sim::Rules rules = {},
                    sim::SimConfig cfg = {}) {
    sim::World w = make(syn::g4_scenario(t), t, std::move(seams), rules, std::move(cfg));
    payload<sim::Workshop>(w, 0, kWeaver).switched_on = false;
    payload<sim::Farm>(w, 0, kFarm).switched_on = false;
    return w;
}

/// Puts a figure with the next id into the registry (the slot by the list policy) with
/// no home, born at `born`, and returns its id.
sim::FigureId put_homeless(sim::World& w, std::uint32_t born = 0) {
    sim::IdCounters& ids = sim::WorldTestAccess::ids(w);
    sim::Figure f;
    f.id = sim::FigureId{ids.next_figure};
    f.kind = 1;
    f.owner = sim::PlayerId{0};
    f.born = born;
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    ++ids.next_figure;
    return f.id;
}

/// Puts a figure that is out (it holds `home`'s counter `counter`, raised here) with the
/// next id, born at `born`, and returns its id.
sim::FigureId put_out(sim::World& w, sim::BuildingId home, std::uint32_t counter = 0,
                      std::uint32_t born = 0) {
    sim::IdCounters& ids = sim::WorldTestAccess::ids(w);
    sim::Figure f;
    f.id = sim::FigureId{ids.next_figure};
    f.kind = 1;
    f.owner = sim::PlayerId{0};
    f.home = home;
    f.home_counter = counter;
    f.born = born;
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    ++ids.next_figure;
    sim::Building* b = sim::find_building_mut(
        *sim::WorldTestAccess::islands(w)[sim::island_of(home).v], home);
    REQUIRE(b != nullptr);
    std::uint32_t* c = sim::figure_counter(b->payload, counter);
    REQUIRE(c != nullptr);
    ++*c;
    return f.id;
}

const sim::Figure& figure(const sim::World& w, sim::FigureId id) {
    const sim::Figure* f = sim::find_figure(w.figures(), id);
    REQUIRE(f != nullptr);
    return *f;
}

std::uint32_t carriers(sim::World& w) { return payload<sim::Workshop>(w, 0, kWeaver).carriers_out; }

/// A behaviour seam that, for the figure `target`, does what `script` says at the first
/// visit whose tick is `at` or later, once.
struct Ender {
    sim::FigureId target;
    std::uint32_t at = 0;
    bool done = false;
    /// 0: end through SerialWorld::end_figure; 1: end twice, then a ReleaseCounter that
    /// names the figure; 2: only a ReleaseCounter that names the figure.
    int script = 0;
};

sim::Seams ender_seams(Ender* e) {
    sim::Seams seams;
    seams.figure_tick.name = "test_ender";
    seams.figure_tick.ctx = e;
    seams.figure_tick.fn = [](void* c, const sim::TickContext& ctx, sim::SerialWorld& sw,
                              sim::Figure& f) {
        auto& e = *static_cast<Ender*>(c);
        if (e.done || f.id != e.target || ctx.t < e.at) {
            return;
        }
        e.done = true;
        sim::ReleaseCounter r;
        r.figure = f.id;
        if (e.script == 0) {
            CHECK(sw.end_figure(f, ctx.t));
        } else if (e.script == 1) {
            CHECK(sw.end_figure(f, ctx.t));
            CHECK_FALSE(sw.end_figure(f, ctx.t)); // ended already: nothing
            sw.fx.releases.push_back(r);         // through the figure: home none, nothing
        } else {
            sw.fx.releases.push_back(r);
        }
    };
    return seams;
}

} // namespace

TEST_CASE("figure registry grows by 100") {
    // D 0x10099130: the free-slot search scans from hint + 1 over the whole vector; a
    // full vector of n entries grows to n + n/4, but at least 100 (0x10099185..
    // 0x100991a1), zero filled, and the new unit takes the old length as its slot.
    // SlotList::insert's rule (docs/sim/state.md §5.2) with min_growth m: a full list of
    // n grows to max(n + n/4, m). A fresh registry has one null entry (A7, 0x10098cd6):
    // the first figure takes slot 0 (n = 1, nothing grows), the second finds the list
    // full and grows it to max(1 + 0, m) = m, taking slot 1.
    //   m = 100 (the original): 100 entries; m = 10 (the building lists' list_min_growth,
    //   which S12 used): 10 entries.
    // Phase B then visits clamp(d·n/50, 1, n) entries (0x1009aa4c..0x1009aa77). One
    // advance of 2 ms is one tick (tick = ms / 2), so d = 1 from the stamp 0:
    //   n = 100: 100/50 = 2 entries, the cursor goes 0 -> 2;
    //   n = 10: 10/50 = 0, clamped to 1, the cursor goes 0 -> 1.
    const syn::SyntheticTypes t = types();
    struct Case {
        std::uint32_t min_growth;
        std::size_t length;
        std::uint32_t cursor;
    };
    for (const Case c : {Case{100, 100, 2}, Case{10, 10, 1}}) {
        INFO("figure_min_growth " << c.min_growth);
        sim::SimConfig cfg;
        cfg.figure_min_growth = c.min_growth;
        sim::World w = quiet_g4(t, {}, {}, cfg);
        const sim::FigureRegistry& reg = w.figures();
        REQUIRE(reg.list.slots.size() == 1);
        const sim::FigureId a = put_homeless(w);
        CHECK(reg.list.slots.size() == 1);
        CHECK(reg.by_id.at(a) == 0);
        const sim::FigureId b = put_homeless(w);
        CHECK(reg.list.slots.size() == c.length);
        CHECK(reg.by_id.at(b) == 1);
        REQUIRE(reg.list.cursor == 0);
        REQUIRE(reg.stamp == 0);
        w.advance(2);
        CHECK(reg.list.cursor == c.cursor);
        CHECK(w.check_invariants() == "");
    }
    // The default configuration is the original's 100, not the building lists' 10.
    CHECK(sim::SimConfig{}.figure_min_growth == 100);
    CHECK(sim::SimConfig{}.list_min_growth == 10);
}

TEST_CASE("release at the end, not at the destroy") {
    // D 0x1003b6e0 (unit slot +0x110) at the figure's end calls the home's slot +0x2c,
    // which nulls the building's pointer to the unit: the counter goes down then. The
    // Remove(unit, 0) at 0x10034844 puts the figure on the expiry list with its stamp
    // frozen; phase C destroys it through 0x10097520 at the first advance with u16(t -
    // stamp) >= 7500, which nulls the entry and deletes the unit and releases nothing.
    // Two carriers of the weaver are out (counter 2); a seam ends the first at T = 1000.
    // Advances of 100 ms are 50 ticks; the registry has 100 slots (the second figure grew
    // it), so phase B visits every slot each advance (clamp(50 · 100 / 50, 1, 100)).
    const syn::SyntheticTypes t = types();
    Ender e;
    e.target = sim::FigureId{1};
    e.at = 1000;
    sim::World w = quiet_g4(t, ender_seams(&e));
    const sim::FigureId a = put_out(w, bid(0, kWeaver));
    const sim::FigureId b = put_out(w, bid(0, kWeaver));
    REQUIRE(a == e.target);
    REQUIRE(carriers(w) == 2);
    REQUIRE(w.check_invariants() == "");
    for (int i = 0; i < 19; ++i) {
        w.advance(100); // tick 950
    }
    CHECK(carriers(w) == 2);
    CHECK_FALSE(figure(w, a).expiry_stamp.has_value());
    w.advance(100); // tick 1000: the end
    CHECK(carriers(w) == 1);
    CHECK(figure(w, a).expiry_stamp == std::uint16_t{1000});
    CHECK_FALSE(figure(w, a).home.valid());
    CHECK(w.figures().expiry == std::vector<sim::FigureId>{a});
    CHECK(w.check_invariants() == "");
    for (int i = 0; i < 149; ++i) {
        w.advance(100); // tick 8450: u16(8450 - 1000) = 7450 < 7500
    }
    CHECK(sim::find_figure(w.figures(), a) != nullptr);
    CHECK(carriers(w) == 1);
    w.advance(100); // tick 8500: 7500, phase C frees the slot
    CHECK(sim::find_figure(w.figures(), a) == nullptr);
    CHECK_FALSE(w.figures().list.slots[0].has_value());
    CHECK(w.figures().expiry.empty());
    CHECK(carriers(w) == 1); // still 1, never 0: the destroy released nothing
    CHECK(sim::find_figure(w.figures(), b) != nullptr);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a second release does nothing") {
    // The original clears the unit's home +0x78 after the release (0x1003b6e0), so a
    // second release through the figure finds no home.
    const syn::SyntheticTypes t = types();
    SUBCASE("end_figure twice on a registry") {
        sim::FigureRegistry reg = sim::make_figure_registry(sim::SimConfig{});
        sim::Figure f;
        f.id = sim::FigureId{1};
        f.home = bid(0, kWeaver);
        REQUIRE(sim::add_figure(reg, f, sim::SimConfig{}, sim::Rules{}) == 0);
        sim::Figure& g = *sim::find_figure(reg, f.id);
        sim::Effects fx;
        sim::end_figure(reg, g, 100, fx);
        sim::end_figure(reg, g, 200, fx);
        REQUIRE(fx.releases.size() == 1);
        CHECK(fx.releases[0].home == bid(0, kWeaver));
        CHECK(fx.releases[0].home_counter == 0);
        CHECK_FALSE(fx.releases[0].figure.valid());
        CHECK(g.expiry_stamp == std::uint16_t{100}); // the first end's
        CHECK_FALSE(g.home.valid());
        CHECK(reg.expiry == std::vector<sim::FigureId>{f.id});
    }
    SUBCASE("a seam ends twice and then releases through the figure") {
        Ender e;
        e.target = sim::FigureId{1};
        e.at = 50;
        e.script = 1;
        sim::World w = quiet_g4(t, ender_seams(&e));
        const sim::FigureId a = put_out(w, bid(0, kWeaver));
        put_out(w, bid(0, kWeaver));
        REQUIRE(carriers(w) == 2);
        w.advance(100); // tick 50
        CHECK(e.done);
        CHECK(carriers(w) == 1);
        CHECK(figure(w, a).expiry_stamp == std::uint16_t{50});
        CHECK(w.check_invariants() == "");
    }
    SUBCASE("a release through the figure alone, then the end") {
        Ender e;
        e.target = sim::FigureId{1};
        e.at = 50;
        e.script = 2;
        sim::World w = quiet_g4(t, ender_seams(&e));
        const sim::FigureId a = put_out(w, bid(0, kWeaver));
        put_out(w, bid(0, kWeaver));
        w.advance(100); // tick 50: released, still running without a home
        CHECK(carriers(w) == 1);
        CHECK_FALSE(figure(w, a).home.valid());
        CHECK_FALSE(figure(w, a).expiry_stamp.has_value());
        CHECK(w.check_invariants() == "");
        // Its end later releases nothing more.
        e.done = false;
        e.script = 0;
        w.advance(100); // tick 100
        CHECK(carriers(w) == 1);
        CHECK(figure(w, a).expiry_stamp == std::uint16_t{100});
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("the interim return goes on expiry") {
    // D12 (OQ-047, Rules::interim_figure_return_ticks = N): at the first phase-B visit at
    // or after born + N the figure ends as the original's do (end_figure); its slot stays
    // taken until phase C, figure_expiry_ticks later. The registry's deferred list and
    // phase D are gone: they never held a figure (OQ-131).
    static_assert(sim::field_count<sim::FigureRegistry>() == 4); // list, stamp, expiry, by_id
    const syn::SyntheticTypes t = types();
    sim::Rules rules;
    rules.interim_figure_return_ticks = 1000;
    sim::World w = quiet_g4(t, {}, rules);
    const sim::FigureId a = put_out(w, bid(0, kWeaver), 0, 0);
    REQUIRE(w.figures().list.slots.size() == 1);
    REQUIRE(w.figures().by_id.at(a) == 0);
    for (int i = 0; i < 19; ++i) {
        w.advance(100); // tick 950: age 950
    }
    CHECK_FALSE(figure(w, a).expiry_stamp.has_value());
    CHECK(carriers(w) == 1);
    w.advance(100); // tick 1000: age 1000, the end
    CHECK(figure(w, a).expiry_stamp == std::uint16_t{1000});
    CHECK_FALSE(figure(w, a).home.valid());
    CHECK(carriers(w) == 0);
    CHECK(w.figures().expiry == std::vector<sim::FigureId>{a});
    CHECK(w.figures().list.slots[0].has_value()); // the slot is still taken
    CHECK(w.check_invariants() == "");
    for (int i = 0; i < 20; ++i) {
        w.advance(100); // tick 2000
    }
    // A figure put out 1 000 ticks after the end finds the one slot taken: the list grows
    // 1 -> 100 and it takes slot 1 (the old phase-D path would have freed slot 0 at tick
    // 1000, and this figure would have taken it with the list still at 1).
    const sim::FigureId b = put_out(w, bid(0, kWeaver), 0, 2000);
    CHECK(w.figures().list.slots.size() == 100);
    CHECK(w.figures().by_id.at(b) == 1);
    for (int i = 0; i < 129; ++i) {
        w.advance(100); // tick 8450: u16(8450 - 1000) = 7450
    }
    CHECK(w.figures().list.slots[0].has_value());
    w.advance(100); // tick 8500: phase C frees slot 0
    CHECK_FALSE(w.figures().list.slots[0].has_value());
    CHECK(sim::find_figure(w.figures(), a) == nullptr);
    // b ended at tick 3000 (age 1000) and waits until 10 500.
    CHECK(figure(w, b).expiry_stamp == std::uint16_t{3000});
    CHECK(carriers(w) == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("a figure whose home was demolished ends without a release") {
    // A6 / T21: the original's production buildings detach their figures when they are
    // destroyed (their +0x78 cleared through figure slot +0x2c); such a figure runs on
    // and its end releases nothing, but it still goes on the expiry list.
    sim::FigureRegistry reg = sim::make_figure_registry(sim::SimConfig{});
    sim::Figure f;
    f.id = sim::FigureId{1};
    REQUIRE(sim::add_figure(reg, f, sim::SimConfig{}, sim::Rules{}) == 0);
    sim::Effects fx;
    sim::end_figure(reg, *sim::find_figure(reg, f.id), 300, fx);
    CHECK(fx.releases.empty());
    CHECK(sim::find_figure(reg, f.id)->expiry_stamp == std::uint16_t{300});
    CHECK(reg.expiry == std::vector<sim::FigureId>{f.id});
    // Through the world (D12 with N = 1): no counter moves.
    const syn::SyntheticTypes t = types();
    sim::Rules rules;
    rules.interim_figure_return_ticks = 1;
    sim::World w = quiet_g4(t, {}, rules);
    const sim::FigureId h = put_homeless(w);
    const sim::FigureId o = put_out(w, bid(0, kWeaver));
    w.advance(100); // tick 50: both end
    CHECK(figure(w, h).expiry_stamp == std::uint16_t{50});
    CHECK(figure(w, o).expiry_stamp == std::uint16_t{50});
    CHECK(carriers(w) == 0); // o's one release; h released nothing
    CHECK(payload<sim::Farm>(w, 0, kFarm).workers_out == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("check_invariants counts the figures that still have the home") {
    // docs/sim/state.md §9.6: every counter of figures out equals the figures whose home
    // and counter name it; a figure on the expiry list has no home and is not counted.
    const syn::SyntheticTypes t = types();
    sim::World w = quiet_g4(t);
    const sim::FigureId a = put_out(w, bid(0, kWeaver));
    REQUIRE(carriers(w) == 1);
    CHECK(w.check_invariants() == "");
    // The figure ends but its release is lost: the counter stays above its figures.
    sim::Figure& f = *sim::find_figure(sim::WorldTestAccess::figures(w), a);
    f.home = sim::BuildingId{};
    f.expiry_stamp = std::uint16_t{0};
    sim::WorldTestAccess::figures(w).expiry.push_back(a);
    const std::string problem = w.check_invariants();
    CHECK(problem.find("counter 0 of figures out is 1 but 0 figures refer to it") !=
          std::string::npos);
    // With the release the ended figure is not counted and the state is whole.
    payload<sim::Workshop>(w, 0, kWeaver).carriers_out = 0;
    CHECK(w.check_invariants() == "");
    // An ended figure that kept its home is refused by validate_figures.
    f.home = bid(0, kWeaver);
    payload<sim::Workshop>(w, 0, kWeaver).carriers_out = 1;
    CHECK(w.check_invariants().find("has ended (it has an expiry_stamp) but still holds") !=
          std::string::npos);
}
