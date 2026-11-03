// City creation, registration, naming and Kontor identifiers (task MAP-2 packages C and D;
// contract docs/sim/city-creation.md, the cause-specific rows of its section 6). Every refusal case
// has a valid control that reaches the operation, changes the one input that violates the condition
// under test, keeps every earlier check satisfied, asserts the specific refusal and compares the
// bytes, hash and PRNG cursor with those before. PRNG values are hand-derived from the default
// table (MSVC rand() from 1: T[0..7] = 41, 18467, 6334, 26500, 19169, 15724, 11478, 29358; next()
// returns T[cursor + 1]).
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/city_creation.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/scenario.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "island_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;

namespace {

using Cities = std::vector<std::pair<std::uint32_t, std::uint32_t>>; // (number, owner), load order

/// One 6x4 island in slot 0 holding `cities` in the given order; players 0..players-1.
sim::World world_with(const Cities& cities, sim::SimConfig cfg = {}, std::uint32_t players = 25) {
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    for (std::uint32_t p = 0; p < players; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        spec.players.push_back(ps);
    }
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 40, 24, 6, 4);
    for (const auto& [no, owner] : cities) {
        is.cities.push_back(syn::plain_city(no, owner));
    }
    spec.islands.push_back(std::move(is));
    return island_test::make_world(spec, std::move(cfg));
}

struct Snap {
    std::vector<std::uint8_t> bytes;
    std::uint64_t hash;
    std::uint32_t rng;
    friend bool operator==(const Snap&, const Snap&) = default;
};
Snap snap(const sim::World& w) {
    return Snap{w.save(), w.state_hash(), w.rng().cursor()};
}

sim::CityRequest req(std::uint32_t owner, std::uint32_t stamp = 0, std::uint32_t island = 0) {
    sim::CityRequest r;
    r.island = sim::IslandId{island};
    r.owner = sim::PlayerId{owner};
    r.stamp = stamp;
    return r;
}

/// prepare() must refuse with `want` and change nothing.
void refused(const sim::World& w, const sim::CityRequest& r, sim::CityRefusal want) {
    const Snap before = snap(w);
    auto p = sim::CityCreation::prepare(w, r);
    REQUIRE_FALSE(p.ok());
    CHECK(p.error() == want);
    CHECK(snap(w) == before);
}

/// prepare + commit; returns the slot.
std::uint32_t create(sim::World& w, const sim::CityRequest& r) {
    auto p = sim::CityCreation::prepare(w, r);
    REQUIRE(p.ok());
    auto c = sim::CityCreation::commit(w, p.value());
    REQUIRE(c.ok());
    return c.value().v;
}

const sim::Island& island0(const sim::World& w) {
    return *w.islands()[0];
}

} // namespace

TEST_CASE("city creation: refusal order") {
    sim::World w = world_with({{0, 0}});
    // Valid control: reaches the operation.
    REQUIRE(sim::CityCreation::prepare(w, req(1)).ok());
    // Two simultaneous defects: the island is checked before the player.
    refused(w, req(31, 0, 9), sim::CityRefusal::UnknownIsland);
    refused(w, req(1, 0, 9), sim::CityRefusal::UnknownIsland);
    // Only the player is unknown (slot 31 is above the 25 players of this world).
    refused(w, req(31), sim::CityRefusal::UnknownPlayer);
    // The stamp is a u16: 65535 passes, 65536 is refused (the original truncates: ADAPT).
    CHECK(sim::CityCreation::prepare(w, req(1, 65535)).ok());
    refused(w, req(1, 65536), sim::CityRefusal::InvalidMetadata);
    // An out-of-range cursor (set through the test door) is metadata, not a slot problem.
    sim::WorldTestAccess::islands(w)[0]->city_cursor = 16;
    refused(w, req(1), sim::CityRefusal::InvalidMetadata);
    sim::WorldTestAccess::islands(w)[0]->city_cursor = -2;
    refused(w, req(1), sim::CityRefusal::InvalidMetadata);
    sim::WorldTestAccess::islands(w)[0]->city_cursor = 0;
    CHECK(sim::CityCreation::prepare(w, req(1)).ok());
    // The other InvalidMetadata branches: a city table that is not city_slots long, and zero slots.
    {
        sim::World shortened = world_with({{0, 0}});
        sim::WorldTestAccess::islands(shortened)[0]->cities.pop_back();
        refused(shortened, req(1), sim::CityRefusal::InvalidMetadata);
        sim::World none = world_with({{0, 0}});
        sim::WorldTestAccess::config(none).city_slots = 0;
        refused(none, req(1), sim::CityRefusal::InvalidMetadata);
    }
    // Pairs of simultaneous defects, each in the contract's order (section 4):
    // player before metadata ...
    refused(w, req(31, 65536), sim::CityRefusal::UnknownPlayer);
    // ... metadata before the own-city rule, the own-city rule before the name data, the full ring
    // before the name data.
    const std::vector<std::string> none_listed;
    sim::World has_city = world_with({{0, 2}});
    refused(has_city, req(2, 65536), sim::CityRefusal::InvalidMetadata);
    sim::CityRequest own_and_empty = req(2);
    own_and_empty.names = std::span<const std::string>(none_listed);
    refused(has_city, own_and_empty, sim::CityRefusal::OwnCityOnIsland);
    Cities fifteen;
    for (std::uint32_t i = 0; i < 15; ++i) {
        fifteen.emplace_back(i, i);
    }
    const sim::World full = world_with(fifteen);
    sim::CityRequest full_and_empty = req(20);
    full_and_empty.names = std::span<const std::string>(none_listed);
    refused(full, full_and_empty, sim::CityRefusal::NoCitySlot);
}

TEST_CASE("city creation: own city and rivals") {
    // Owner 2 has a city on the island: owner 1 still creates one (a rival never refuses) ...
    sim::World w = world_with({{0, 2}});
    CHECK(sim::CityCreation::prepare(w, req(1)).ok());
    // ... owner 2 does not (same player, same island).
    refused(w, req(2), sim::CityRefusal::OwnCityOnIsland);
    // After owner 1 founds one, a third player is still free to, and owner 1 now refuses.
    create(w, req(1));
    CHECK(sim::CityCreation::prepare(w, req(3)).ok());
    refused(w, req(1), sim::CityRefusal::OwnCityOnIsland);
    // Precedence: with every usable slot taken (15 cities, slot 15 is the neutral value) the
    // own-city refusal still comes before NoCitySlot.
    Cities fifteen;
    for (std::uint32_t i = 0; i < 15; ++i) {
        fifteen.emplace_back(i, i);
    }
    const sim::World full = world_with(fifteen);
    refused(full, req(3), sim::CityRefusal::OwnCityOnIsland);
    refused(full, req(20), sim::CityRefusal::NoCitySlot); // same ring, a player without a city
}

TEST_CASE("city creation: another island's city never refuses") {
    // Optional sharing: the owner's city elsewhere is no blanket prohibition.
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    for (std::uint32_t p = 0; p < 4; ++p) {
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{p};
        spec.players.push_back(ps);
    }
    sim::ScenarioSpec::IslandSpec a = syn::plain_island(0, 40, 24, 6, 4);
    a.cities.push_back(syn::plain_city(0, 1));
    sim::ScenarioSpec::IslandSpec b = syn::plain_island(1, 80, 24, 6, 4);
    spec.islands.push_back(std::move(a));
    spec.islands.push_back(std::move(b));
    const sim::World w = island_test::make_world(spec);
    refused(w, req(1, 0, 0), sim::CityRefusal::OwnCityOnIsland); // island 0 has owner 1's city
    CHECK(sim::CityCreation::prepare(w, req(1, 0, 1)).ok());     // island 1 does not
}

TEST_CASE("city creation: allocation order") {
    // Cursor -1 (empty island): slot 0.
    sim::World empty = world_with({});
    CHECK(island0(empty).city_cursor == -1);
    CHECK(sim::CityCreation::prepare(empty, req(1)).value().number.v == 0);
    // Occupied {0,1}, cursor 1: slot 2.
    sim::World two = world_with({{0, 0}, {1, 1}});
    CHECK(island0(two).city_cursor == 1);
    CHECK(sim::CityCreation::prepare(two, req(5)).value().number.v == 2);
    // The cursor is not the highest slot: occupied {1,2,5,6}, last created 2: next is 3 (an
    // inference from the highest slot would give 7).
    sim::World gaps = world_with({{1, 0}, {5, 1}, {6, 2}, {2, 3}});
    CHECK(island0(gaps).city_cursor == 2);
    CHECK(sim::CityCreation::prepare(gaps, req(10)).value().number.v == 3);
    // Creating into the gap moves the cursor to THAT slot (3), not to the highest occupied one (6):
    // the next allocation is 4.
    CHECK(create(gaps, req(10)) == 3);
    CHECK(island0(gaps).city_cursor == 3);
    CHECK(sim::CityCreation::prepare(gaps, req(11)).value().number.v == 4);
    // Wraparound: everything but slot 3 occupied (14 cities, 15 is neutral), last created 14: the
    // scan wraps past the neutral slot and the occupied ones to 3.
    Cities all;
    for (std::uint32_t i = 0; i < 15; ++i) {
        if (i != 3) {
            all.emplace_back(i, i);
        }
    }
    sim::World wrap = world_with(all);
    CHECK(island0(wrap).city_cursor == 14);
    CHECK(sim::CityCreation::prepare(wrap, req(20)).value().number.v == 3);
    // A cursor on a slot that has since been emptied (the cursor stays at the last ADDED slot):
    // occupied {0,3}, cursor 2 -> 4.
    sim::World moved = world_with({{0, 0}, {3, 1}});
    sim::WorldTestAccess::islands(moved)[0]->city_cursor = 2;
    CHECK(sim::CityCreation::prepare(moved, req(5)).value().number.v == 4);
    // Successive creations follow the moving cursor and persist it.
    sim::World seq = world_with({});
    CHECK(create(seq, req(1)) == 0);
    CHECK(create(seq, req(2)) == 1);
    CHECK(island0(seq).city_cursor == 1);
    CHECK(island0(seq).city(sim::CityNo{1})->owner == sim::PlayerId{2});
}

TEST_CASE("city creation: allocation exhaustion") {
    // Control: the same ring with exactly one usable slot (14) free succeeds ...
    Cities thirteen;
    for (std::uint32_t i = 0; i < 14; ++i) {
        thirteen.emplace_back(i, i);
    }
    sim::World w = world_with(thirteen); // slots 0..13 taken, 14 free, 15 neutral
    CHECK(sim::CityCreation::prepare(w, req(20)).value().number.v == 14);
    // ... and once that slot is filled by another owner the refusal is NoCitySlot, never 15.
    CHECK(create(w, req(20)) == 14);
    refused(w, req(21), sim::CityRefusal::NoCitySlot);
    CHECK(island0(w).cities[15].has_value() == false);

    // Non-default capacities (SimConfig::validate needs neutral_owner <= city_slots).
    sim::SimConfig c4;
    c4.city_slots = 4;
    c4.neutral_owner = 4;
    sim::World four = world_with({{0, 0}, {1, 1}, {2, 2}}, c4);
    CHECK(create(four, req(3)) == 3);
    refused(four, req(4), sim::CityRefusal::NoCitySlot);

    sim::SimConfig c20;
    c20.city_slots = 20;
    c20.neutral_owner = 15;
    Cities fifteen;
    for (std::uint32_t i = 0; i < 15; ++i) {
        fifteen.emplace_back(i, i);
    }
    sim::World twenty = world_with(fifteen, c20);
    CHECK(sim::CityCreation::prepare(twenty, req(16)).value().number.v == 16); // 15 is skipped

    sim::SimConfig c8;
    c8.city_slots = 8;
    c8.neutral_owner = 5;
    sim::World eight = world_with({{0, 0}, {1, 1}, {2, 2}, {3, 3}, {4, 4}}, c8);
    CHECK(create(eight, req(5)) == 6); // 5 is neutral
    CHECK(create(eight, req(6)) == 7);
    refused(eight, req(7), sim::CityRefusal::NoCitySlot);
}

TEST_CASE("city creation: loaded cursor") {
    CHECK(island0(world_with({{5, 0}, {2, 1}, {9, 2}})).city_cursor == 9); // last in load order
    CHECK(island0(world_with({{5, 0}, {9, 1}, {2, 2}})).city_cursor == 2);
    CHECK(island0(world_with({})).city_cursor == -1);
    // The explicit numbers are checked by the loader's existing validation: the neutral value and a
    // number past the capacity are refused with their own diagnostics.
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    spec.players.push_back(ps);
    for (const std::uint32_t bad : {15U, 16U}) {
        sim::ScenarioSpec s = spec;
        sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 40, 24, 6, 4);
        is.cities.push_back(syn::plain_city(bad, 0));
        s.islands.push_back(std::move(is));
        auto r = sim::World::create(s, sim::TypeRegistry::empty());
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().find(bad == 15 ? "is SimConfig.neutral_owner"
                                       : "is not a slot below SimConfig.city_slots") !=
              std::string::npos);
    }
}

TEST_CASE("city creation: scheduler cursor is not the allocation cursor") {
    sim::World w = world_with({{0, 0}, {3, 1}});
    const std::int32_t cursor = island0(w).city_cursor;
    CHECK(cursor == 3);
    const std::uint32_t slot = sim::CityCreation::prepare(w, req(5)).value().number.v;
    for (int i = 0; i < 200; ++i) {
        w.advance(10); // the island scheduler walks its own ring cursor
    }
    CHECK(island0(w).city_cursor == cursor);
    // The two cursors are different words: after the ticks the scheduler's is elsewhere, so a
    // creation that read it as the allocation cursor would allocate a different slot.
    CHECK(static_cast<std::int64_t>(island0(w).ring.cursor) != island0(w).city_cursor);
    CHECK(sim::CityCreation::prepare(w, req(5)).value().number.v == slot);
    // Creating a city does not touch the scheduler's cursor or stamp.
    const sim::CityRing ring = island0(w).ring;
    create(w, req(5));
    CHECK(island0(w).ring.cursor == ring.cursor);
    CHECK(island0(w).ring.stamp == ring.stamp);
}

TEST_CASE("city creation: naming") {
    // No list: no draw, no name.
    {
        sim::World w = world_with({});
        sim::WorldTestAccess::rng(w).set_cursor(0);
        create(w, req(1));
        CHECK(w.rng().cursor() == 0);
        CHECK(island0(w).city(sim::CityNo{0})->name.empty());
    }
    const std::vector<std::string> three = {"Alpha", "Beta", "Gamma"};
    // A one-entry list is the control for the refusals below: one draw, that entry.
    {
        sim::World w = world_with({});
        sim::CityRequest r = req(1);
        const std::vector<std::string> one = {"Solo"};
        r.names = std::span<const std::string>(one);
        create(w, r);
        CHECK(island0(w).city(sim::CityNo{0})->name == "Solo");
    }
    // Hand-derived: cursor 0 -> T[1] = 18467, 18467 mod 3 = 2: "Gamma"; one table step.
    {
        sim::World w = world_with({});
        sim::WorldTestAccess::rng(w).set_cursor(0);
        sim::CityRequest r = req(1);
        r.names = std::span<const std::string>(three);
        create(w, r);
        CHECK(island0(w).city(sim::CityNo{0})->name == "Gamma");
        CHECK(w.rng().cursor() == 1);
    }
    // cursor 4 -> T[5] = 15724, mod 3 = 1: "Beta"; mod 5 = 4 for a list of five.
    {
        sim::World w = world_with({});
        sim::WorldTestAccess::rng(w).set_cursor(4);
        sim::CityRequest r = req(1);
        r.names = std::span<const std::string>(three);
        create(w, r);
        CHECK(island0(w).city(sim::CityNo{0})->name == "Beta");
        CHECK(w.rng().cursor() == 5);
        const std::vector<std::string> five = {"a", "b", "c", "d", "e"};
        sim::World w5 = world_with({});
        sim::WorldTestAccess::rng(w5).set_cursor(4);
        r.names = std::span<const std::string>(five);
        create(w5, r);
        CHECK(island0(w5).city(sim::CityNo{0})->name == "e");
    }
    // A prepared plan that is discarded does not consume the live generator.
    {
        sim::World w = world_with({});
        sim::WorldTestAccess::rng(w).set_cursor(0);
        sim::CityRequest r = req(1);
        r.names = std::span<const std::string>(three);
        const Snap before = snap(w);
        auto plan = sim::CityCreation::prepare(w, r);
        REQUIRE(plan.ok());
        CHECK(plan.value().draws);
        CHECK(plan.value().name == "Gamma");
        CHECK(snap(w) == before);
    }
    // An empty list is refused (the original divides by zero); everything before it is satisfied.
    {
        sim::World w = world_with({});
        const std::vector<std::string> none;
        sim::CityRequest r = req(1);
        r.names = std::span<const std::string>(none);
        refused(w, r, sim::CityRefusal::UnusableNameData);
    }
    // A NUL in the DRAWN entry (index 2 at cursor 0) and, separately, in an UNDRAWN entry: both
    // refuse, because the whole list is validated.
    for (const std::size_t at : {std::size_t{2}, std::size_t{0}}) {
        sim::World w = world_with({});
        sim::WorldTestAccess::rng(w).set_cursor(0);
        std::vector<std::string> bad = three;
        bad[at] = std::string("Ga\0mma", 6);
        sim::CityRequest r = req(1);
        r.names = std::span<const std::string>(bad);
        refused(w, r, sim::CityRefusal::UnusableNameData);
    }
}

TEST_CASE("city creation: stale plan") {
    const auto setup = [] {
        sim::World w = world_with({{0, 0}});
        sim::WorldTestAccess::rng(w).set_cursor(0);
        return w;
    };
    const std::vector<std::string> names = {"A", "B", "C"};
    sim::CityRequest r1 = req(1);
    r1.names = std::span<const std::string>(names);
    // Control: the plan commits when nothing moved.
    {
        sim::World w = setup();
        auto p = sim::CityCreation::prepare(w, r1);
        REQUIRE(p.ok());
        CHECK(sim::CityCreation::commit(w, p.value()).ok());
    }
    const auto stale = [&](auto&& interfere) {
        sim::World w = setup();
        auto p = sim::CityCreation::prepare(w, r1);
        REQUIRE(p.ok());
        interfere(w);
        const Snap before = snap(w);
        auto c = sim::CityCreation::commit(w, p.value());
        REQUIRE_FALSE(c.ok());
        CHECK(c.error() == sim::CityRefusal::StalePlan);
        CHECK(snap(w) == before);
    };
    // (a) another city allocated (slot and cursor moved)
    stale([&](sim::World& w) { create(w, req(2)); });
    // (b) another draw advanced the PRNG (the peeked name would be wrong)
    stale([](sim::World& w) { static_cast<void>(sim::WorldTestAccess::rng(w).next()); });
    // (d) a rival took the planned slot without the allocation cursor moving
    stale([](sim::World& w) {
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        is.cities[1] = sim::make_city(sim::CityNo{1}, sim::PlayerId{2}, w.config());
    });
    // (e) the allocation cursor moved without the planned slot being taken
    stale([](sim::World& w) { sim::WorldTestAccess::islands(w)[0]->city_cursor = 5; });
    // (c) the owner's own city appeared in another slot without the cursor moving
    stale([](sim::World& w) {
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        is.cities[7] = sim::make_city(sim::CityNo{7}, sim::PlayerId{1}, w.config());
    });
}

TEST_CASE("city creation: a hand-built plan naming the neutral slot is refused") {
    // Control: the plan from prepare commits. Defect: the same plan with number set to the neutral
    // value (15, empty and otherwise consistent) is refused and nothing changes.
    sim::World w = world_with({{0, 0}});
    auto p = sim::CityCreation::prepare(w, req(1));
    REQUIRE(p.ok());
    sim::CityPlan forged = p.value();
    forged.number = sim::CityNo{w.config().neutral_owner};
    const Snap before = snap(w);
    auto c = sim::CityCreation::commit(w, forged);
    REQUIRE_FALSE(c.ok());
    CHECK(c.error() == sim::CityRefusal::StalePlan);
    CHECK(snap(w) == before);
    CHECK(sim::CityCreation::commit(w, p.value()).ok());
}

TEST_CASE("city creation: stamp and persistence") {
    sim::World w = world_with({{0, 0}});
    const std::vector<std::string> names = {"A", "B", "C"};
    sim::CityRequest r = req(1, 4242);
    r.names = std::span<const std::string>(names);
    sim::WorldTestAccess::rng(w).set_cursor(0);
    CHECK(create(w, r) == 1);
    const sim::City& c = *island0(w).city(sim::CityNo{1});
    CHECK(c.tick_stamp == 4242);
    CHECK(c.index.stamp == 4242);
    CHECK(c.name == "C");
    CHECK(island0(w).city_cursor == 1);
    const std::vector<std::uint8_t> bytes = w.save();
    auto loaded = sim::World::load(bytes, sim::TypeRegistry::empty());
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().state_hash() == w.state_hash());
    CHECK(loaded.value().save() == bytes);
    CHECK(loaded.value().check_invariants().empty());
    CHECK(island0(loaded.value()).city_cursor == 1);
    CHECK(island0(loaded.value()).city(sim::CityNo{1})->name == "C");
    // The next allocation after a reload continues from the saved cursor.
    CHECK(sim::CityCreation::prepare(loaded.value(), req(2)).value().number.v == 2);
    // The executor does not matter for the hash of a world with a created city.
    sim::World a = w;
    sim::World b = w;
    a.prepare_hash(sim::serial_executor());
    CHECK(a.state_hash() == w.state_hash());
    CHECK(b.state_hash() == w.state_hash());
    // The metadata is state: a different name, cursor or counter is a different world.
    sim::World other = w;
    sim::WorldTestAccess::islands(other)[0]->cities[1]->name = "D";
    CHECK(other.state_hash() != w.state_hash());
    sim::WorldTestAccess::islands(other)[0]->cities[1]->name = "C";
    sim::WorldTestAccess::islands(other)[0]->city_cursor = 0;
    CHECK(other.state_hash() != w.state_hash());
    sim::WorldTestAccess::islands(other)[0]->city_cursor = 1;
    sim::WorldTestAccess::islands(other)[0]->kontor_counter = 2;
    CHECK(other.state_hash() != w.state_hash());
}

TEST_CASE("city creation: draw sites") {
    using sim::DrawSite;
    CHECK(sim::draw_site_ported(DrawSite::CityName));
    CHECK_FALSE(sim::draw_site_ported(DrawSite::CityNameLoad));
    CHECK(sim::draw_site_phase(DrawSite::CityName) == sim::DrawPhase::Advance);
    CHECK(sim::draw_site_phase(DrawSite::CityNameLoad) == sim::DrawPhase::Load);
    CHECK(std::string(sim::draw_site_name(DrawSite::CityName)) == "CityName");
    CHECK(std::string(sim::draw_site_name(DrawSite::CityNameLoad)) == "CityNameLoad");
    CHECK(sim::kPrngTimelineVersion == 4); // 4: FarmWorkerDispatch (MAP-ECON-1 E2)
}

TEST_CASE("kontor ids") {
    const auto make = [](std::uint32_t counter) {
        sim::ScenarioSpec spec = syn::empty_scenario(1234);
        sim::ScenarioSpec::PlayerSpec ps;
        ps.id = sim::PlayerId{0};
        spec.players.push_back(ps);
        sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 40, 24, 6, 4);
        is.kontor_counter = counter;
        spec.islands.push_back(std::move(is));
        return island_test::make_world(spec);
    };
    // Default: the counter is 1.
    CHECK(island0(make(1)).kontor_counter == 1);
    sim::World w = make(7);
    const Snap before = snap(w);
    // Preparing (twice), saving and loading never increment.
    auto p1 = sim::CityCreation::prepare_kontor(w, sim::IslandId{0});
    auto p2 = sim::CityCreation::prepare_kontor(w, sim::IslandId{0});
    REQUIRE(p1.ok());
    REQUIRE(p2.ok());
    CHECK(p1.value().id == 7);
    CHECK(snap(w) == before);
    auto reloaded = sim::World::load(w.save(), sim::TypeRegistry::empty());
    REQUIRE(reloaded.ok());
    CHECK(island0(reloaded.value()).kontor_counter == 7);
    // Commit: the old value is the id, the counter moves by one, nothing else changes.
    const std::uint32_t serial = island0(w).next_serial;
    auto id = sim::CityCreation::commit_kontor(w, p1.value());
    REQUIRE(id.ok());
    CHECK(id.value() == 7);
    CHECK(island0(w).kontor_counter == 8);
    CHECK(island0(w).next_serial == serial); // the building serial counter is never used for it
    // A plan prepared before another commit is stale.
    auto stale = sim::CityCreation::commit_kontor(w, p2.value());
    REQUIRE_FALSE(stale.ok());
    CHECK(stale.error() == sim::KontorRefusal::StaleKontor);
    CHECK(island0(w).kontor_counter == 8);
    // Unknown island.
    CHECK(sim::CityCreation::prepare_kontor(w, sim::IslandId{5}).error() ==
          sim::KontorRefusal::UnknownIsland);
    // Overflow: UINT32_MAX - 1 succeeds (the control), UINT32_MAX is refused with nothing changed.
    sim::World edge = make(std::numeric_limits<std::uint32_t>::max() - 1);
    auto last = sim::CityCreation::prepare_kontor(edge, sim::IslandId{0});
    REQUIRE(last.ok());
    CHECK(sim::CityCreation::commit_kontor(edge, last.value()).value() ==
          std::numeric_limits<std::uint32_t>::max() - 1);
    CHECK(island0(edge).kontor_counter == std::numeric_limits<std::uint32_t>::max());
    const Snap at_max = snap(edge);
    auto over = sim::CityCreation::prepare_kontor(edge, sim::IslandId{0});
    REQUIRE_FALSE(over.ok());
    CHECK(over.error() == sim::KontorRefusal::CounterOverflow);
    CHECK(snap(edge) == at_max);
}

TEST_CASE("kontor ids: an inconsistent hand-built plan is refused") {
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    spec.players.push_back(ps);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 40, 24, 6, 4);
    is.kontor_counter = 7;
    spec.islands.push_back(std::move(is));
    sim::World w = island_test::make_world(spec);
    auto p = sim::CityCreation::prepare_kontor(w, sim::IslandId{0});
    REQUIRE(p.ok());
    sim::KontorPlan forged = p.value();
    forged.id = 99; // would hand out an id that is not the counter
    const Snap before = snap(w);
    auto c = sim::CityCreation::commit_kontor(w, forged);
    REQUIRE_FALSE(c.ok());
    CHECK(c.error() == sim::KontorRefusal::StaleKontor);
    CHECK(snap(w) == before);
    CHECK(sim::CityCreation::commit_kontor(w, p.value()).value() == 7); // the control commits
}

TEST_CASE("city stream layout with a name") {
    // The name sits between the trade lists and the index scope: u64 length, then the bytes
    // (hand-written bytes, not the implementation's output).
    const sim::SimConfig cfg;
    sim::City city = sim::make_city(sim::CityNo{3}, sim::PlayerId{7}, cfg);
    city.name = "Alpha";
    island_test::Stream want;
    island_test::append_empty_city(want, 3, 7, "Alpha");
    sim::WriteArchive wa;
    fields(wa, std::as_const(city));
    CHECK(wa.data() == want.v);
    sim::City empty = sim::make_city(sim::CityNo{3}, sim::PlayerId{7}, cfg);
    sim::WriteArchive wb;
    fields(wb, std::as_const(empty));
    CHECK(wb.data().size() + 5 == wa.data().size()); // only the five name bytes differ in size
    // And it reads back.
    sim::ReadArchive r(wa.data());
    sim::City back = sim::make_city(sim::CityNo{0}, sim::PlayerId{0}, cfg);
    fields(r, back);
    CHECK_FALSE(r.failed());
    CHECK(back.name == "Alpha");
    CHECK(back.number == city.number);
}

TEST_CASE("kontor ids: the building takes the old counter") {
    auto t = syn::make_synthetic_types();
    REQUIRE(t.ok());
    sim::ScenarioSpec spec = syn::empty_scenario(1234);
    sim::ScenarioSpec::PlayerSpec ps;
    ps.id = sim::PlayerId{0};
    spec.players.push_back(ps);
    sim::ScenarioSpec::IslandSpec is = syn::plain_island(0, 100, 100, 32, 32);
    is.kontor_counter = 7;
    is.cities.push_back(syn::plain_city(0, 0));
    spec.islands.push_back(std::move(is));
    auto created = sim::World::create(spec, t.value().registry);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World w = std::move(created).value();
    auto plan = sim::CityCreation::prepare_kontor(w, sim::IslandId{0});
    REQUIRE(plan.ok());
    sim::PlacementRecord rec;
    rec.type = t.value().weaver;
    rec.x = 110;
    rec.y = 110;
    rec.kontor_id = plan.value().id;
    auto made = sim::WorldTestAccess::create(w, 0, 0, rec);
    REQUIRE(made.ok());
    REQUIRE(sim::CityCreation::commit_kontor(w, plan.value()).ok());
    const sim::Building* b = w.find_building(sim::make_building_id(sim::IslandId{0}, 1));
    REQUIRE(b != nullptr);
    CHECK(b->kontor_id == 7);
    CHECK(island0(w).kontor_counter == 8);
    // Saved and loaded: both persist, and a building without one keeps 0.
    auto loaded = sim::World::load(w.save(), t.value().registry);
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    CHECK(loaded.value().find_building(sim::make_building_id(sim::IslandId{0}, 1))->kontor_id == 7);
    CHECK(island0(loaded.value()).kontor_counter == 8);
    CHECK(loaded.value().state_hash() == w.state_hash());
}
