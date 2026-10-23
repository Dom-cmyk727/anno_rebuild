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
#include <anno/sim/synthetic.hpp>
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
