// MAP-POP-LIVE-1 L1: the persistent state schema of the population live effects (stream 128,
// PRNG timeline 5; docs/sim/population-live.md sections 2, 4, 8): City::billed / stage_mask /
// stage_stamp, House::walkers, Figure::resident, Rules::house_updates / save_billed_goods, the
// SimConfig fields, the house walker facts of the registry and the three declared draw sites.
// Every expected byte string is written by hand from the layout comment of world.hpp, not read back
// from the field list; each layout test has a swapped-field and an omitted-field control that must
// differ from the real bytes (the byte assertion discriminates).
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/city.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/resident.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>
#include <anno/sim_load/original_registry.hpp>
#include <anno/test/game_dir.hpp>

#include "island_helpers.hpp"
#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
namespace syn = anno::sim::synthetic;
using island_test::Stream;
using Bytes = std::vector<std::uint8_t>;

bool contains(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

template <class T> Bytes bytes_of(T& value) {
    sim::WriteArchive w;
    fields(w, value);
    return w.data();
}

} // namespace

TEST_CASE("population schema in SCRIPT1: versions, defaults and the three declared draw sites") {
    CHECK(sim::kStateHashVersion == 138);
    CHECK(sim::kPrngTimelineVersion == 5);
    const sim::Rules r;
    CHECK_FALSE(r.house_updates);
    CHECK(r.save_billed_goods);
    const sim::SimConfig c;
    CHECK(c.billed_goods_first == 0x1a);
    CHECK(c.billed_goods_last == 0x28);
    CHECK(c.resident_cargo_slots == 3);
    // The sites are appended after FarmWorkerDispatch (19), in this order, drawn in the advance
    // phase, and ported (the first two) only while Rules::house_updates is on.
    CHECK(static_cast<int>(sim::DrawSite::HouseWalkerTarget) == 20);
    CHECK(static_cast<int>(sim::DrawSite::HouseVariant) == 21);
    CHECK(static_cast<int>(sim::DrawSite::HouseFirstAssignment) == 22);
    CHECK(static_cast<int>(sim::DrawSite::Count_) == 23);
    for (const sim::DrawSite s : {sim::DrawSite::HouseWalkerTarget, sim::DrawSite::HouseVariant,
                                  sim::DrawSite::HouseFirstAssignment}) {
        CHECK(sim::draw_site_phase(s) == sim::DrawPhase::Advance);
        CHECK(sim::may_draw(s, sim::DrawPhase::Advance));
        CHECK_FALSE(sim::may_draw(s, sim::DrawPhase::Load));
        // The sites draw only with Rules::house_updates: the static answer (default rules) is
        // "not ported", so the replay manifest of a default log still lists them.
        CHECK_FALSE(sim::draw_site_ported(s));
        CHECK_FALSE(sim::draw_site_ported(s, false));
        CHECK(sim::draw_site_ported(s, true)); // C2: the player build draws HouseFirstAssignment
    }
    CHECK(std::string(sim::draw_site_name(sim::DrawSite::HouseWalkerTarget)) ==
          "HouseWalkerTarget");
    CHECK(std::string(sim::draw_site_name(sim::DrawSite::HouseVariant)) == "HouseVariant");
    CHECK(std::string(sim::draw_site_name(sim::DrawSite::HouseFirstAssignment)) ==
          "HouseFirstAssignment");
}

namespace {

enum class Variant { Right, Swapped, Omitted };

/// The expected house bytes: last, residents_64, e, the stock list, then the walker list.
Bytes expected_house(Variant v) {
    Stream s;
    s.u16(0x0102);
    s.i32(-2);
    s.u32(0x0a0b0c0d);
    s.u64(2); // stock
    s.u32(7);
    s.u32(0x11223344);
    if (v == Variant::Omitted) {
        return s.v;
    }
    s.u64(3); // walkers: slot 0 a figure, slot 1 empty (invalid = max), slot 2 a figure
    if (v == Variant::Swapped) {
        s.u64(0x0000000100000002ULL);
        s.u64(0xffffffffffffffffULL);
        s.u64(9);
    } else {
        s.u64(9);
        s.u64(0xffffffffffffffffULL);
        s.u64(0x0000000100000002ULL);
    }
    return s.v;
}

} // namespace

TEST_CASE("stream 128: the house walker slots have the documented bytes") {
    sim::House h;
    h.last = 0x0102;
    h.residents_64 = -2;
    h.e = 0x0a0b0c0d;
    h.stock = {7, 0x11223344};
    h.walkers = {sim::FigureId{9}, sim::FigureId{}, sim::FigureId{0x0000000100000002ULL}};
    const Bytes got = bytes_of(h);
    CHECK(got == expected_house(Variant::Right));
    CHECK(got != expected_house(Variant::Swapped)); // the controls reach the byte assertion
    CHECK(got != expected_house(Variant::Omitted));
    sim::House back;
    sim::ReadArchive r(got);
    fields(r, back);
    CHECK_FALSE(r.failed());
    CHECK(back.walkers == h.walkers);
    CHECK(back.stock == h.stock);
}

namespace {

/// The expected resident state bytes.
Bytes expected_resident(Variant v) {
    Stream s;
    s.u8(2); // step Wait
    s.u32(5);
    s.u64(1); // route
    s.i32(-256);
    s.i32(512);
    s.u32(2);  // next
    s.u32(77); // along
    s.u64(0x0000000300000004ULL);
    if (v == Variant::Omitted) {
        return s.v;
    }
    s.u64(3); // cargo slots
    if (v == Variant::Swapped) {
        s.u32(100); // amount before goods
        s.u32(26);
    } else {
        s.u32(26);
        s.u32(100);
    }
    s.u32(0);
    s.u32(0);
    s.u32(27);
    s.u32(0); // goods set, amount 0: not an empty slot
    return s.v;
}

} // namespace

TEST_CASE("stream 128: the resident state has the documented bytes") {
    sim::ResidentState r;
    r.step = sim::ResidentStep::Wait;
    r.wait = 5;
    r.route = {sim::FixedPos{-256, 512}};
    r.next = 2;
    r.along = 77;
    r.target = sim::BuildingId{0x0000000300000004ULL};
    r.cargo = {sim::ResidentCargo{26, 100}, sim::ResidentCargo{0, 0}, sim::ResidentCargo{27, 0}};
    const Bytes got = bytes_of(r);
    CHECK(got == expected_resident(Variant::Right));
    CHECK(got != expected_resident(Variant::Swapped));
    CHECK(got != expected_resident(Variant::Omitted));
    sim::ResidentState back;
    sim::ReadArchive rd(got);
    fields(rd, back);
    CHECK_FALSE(rd.failed());
    CHECK(back == r);
    // As a figure member it is an optional after the harvest optional: presence byte 1 then
    // these bytes. Since route-only136 the figure ends with u64 freight_generation and the
    // freight presence byte (state_fields.hpp, docs/sim/state.md "Route-only136"), 9 bytes,
    // all zero for a figure without a freight route.
    constexpr std::ptrdiff_t kFreightTail = 8 + 1;
    sim::Figure f;
    const Bytes none = bytes_of(f);
    f.resident = r;
    const Bytes some = bytes_of(f);
    REQUIRE(some.size() == none.size() + got.size());
    REQUIRE(none.size() > static_cast<std::size_t>(kFreightTail));
    CHECK(Bytes(none.end() - kFreightTail, none.end()) == Bytes(kFreightTail, 0));
    CHECK(Bytes(some.end() - kFreightTail, some.end()) == Bytes(kFreightTail, 0));
    // absent resident: one presence byte after the harvest presence byte
    CHECK(*(none.end() - kFreightTail - 1) == 0);
    Bytes tail(some.end() - kFreightTail - static_cast<std::ptrdiff_t>(got.size()) - 1,
               some.end() - kFreightTail);
    CHECK(tail.front() == 1);
    CHECK(Bytes(tail.begin() + 1, tail.end()) == got);
}

namespace {

/// The expected bytes of a city up to and including the new fields (the index scope that
/// follows is unchanged and appended from the real writer by the caller).
Bytes expected_city_prefix(bool save_billed, Variant v) {
    Stream s;
    s.u32(3);      // number
    s.u32(2);      // owner
    s.u16(0x1234); // tick_stamp
    s.u32(50 * 256);
    s.u64(56);
    for (int i = 0; i < 56; ++i) {
        s.u32(0); // stock
    }
    s.u64(56);
    for (int i = 0; i < 56; ++i) {
        s.boolean(false);
        s.i32(-1);
    }
    s.u64(5);
    for (int i = 0; i < 5; ++i) {
        s.u32(0);
        s.u64(0);
    }
    s.u64(2);
    s.u64(0);
    s.u64(0);
    s.u64(2); // name "ab"
    s.u8('a');
    s.u8('b');
    s.u64(1); // player mask {5}
    s.u32(5);
    if (v == Variant::Swapped) {
        s.u16(0xBEEF); // stamp before mask
        s.u32(0x80000005U);
    } else {
        s.u32(0x80000005U); // stage_mask
        s.u16(0xBEEF);      // stage_stamp
    }
    if (save_billed && v != Variant::Omitted) {
        s.u64(56);
        for (int i = 0; i < 56; ++i) {
            s.u32(i == 1 ? 7U : (i == 55 ? 9U : 0U));
        }
    }
    s.u16(0); // CityAccounts stamp (133), independent of save_billed
    for (int i = 0; i < 5; ++i)
        s.u64(0); // sales, trade sales/purchases, civil/military costs
    return s.v;
}

} // namespace

TEST_CASE("stream 128: the city stage fields and the billed counters have the documented bytes") {
    const sim::SimConfig cfg;
    sim::City c = sim::make_city(sim::CityNo{3}, sim::PlayerId{2}, cfg);
    c.tick_stamp = 0x1234;
    c.name = "ab";
    c.player_mask = {sim::PlayerId{5}};
    c.stage_mask = 0x80000005U;
    c.stage_stamp = 0xBEEF;
    c.billed[1] = 7;
    c.billed[55] = 9;
    sim::WriteArchive index_only;
    fields(index_only, c.index);
    for (const bool save_billed : {true, false}) {
        INFO("save_billed " << save_billed);
        sim::WriteArchive w;
        fields(w, c, sim::CityStream{save_billed, cfg.warehouse_goods});
        Bytes want = expected_city_prefix(save_billed, Variant::Right);
        want.insert(want.end(), index_only.data().begin(), index_only.data().end());
        CHECK(w.data() == want);
        // The controls reach the byte assertion: another order or a missing field differs.
        Bytes swapped = expected_city_prefix(save_billed, Variant::Swapped);
        swapped.insert(swapped.end(), index_only.data().begin(), index_only.data().end());
        CHECK(w.data() != swapped);
        if (save_billed) {
            Bytes omitted = expected_city_prefix(save_billed, Variant::Omitted);
            omitted.insert(omitted.end(), index_only.data().begin(), index_only.data().end());
            CHECK(w.data() != omitted);
        }
        // Reading: the counters come back with the flag on; with it off the city starts at zero.
        sim::City back;
        sim::ReadArchive r(w.data());
        fields(r, back, sim::CityStream{save_billed, cfg.warehouse_goods});
        CHECK_FALSE(r.failed());
        CHECK(back.stage_mask == 0x80000005U);
        CHECK(back.stage_stamp == 0xBEEF);
        REQUIRE(back.billed.size() == cfg.warehouse_goods);
        CHECK(back.billed[1] == (save_billed ? 7U : 0U));
        CHECK(back.billed[55] == (save_billed ? 9U : 0U));
    }
    // build_goods already carries the reserve (setting, 0x5d4) and the spent dword (0x5d8):
    // 4 bytes and 8 bytes each, no second copy exists.
    sim::BuildGoods bg;
    bg.setting = 0x01020304;
    bg.spent = 0x1112131415161718ULL;
    const Bytes bgb = bytes_of(bg);
    CHECK(bgb == Bytes{4, 3, 2, 1, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11});
}

namespace {

/// A registry whose pioneer house has walker facts: at most 2 out, figure group 0 = {203, 204}.
std::shared_ptr<const sim::TypeRegistry> house_registry(std::uint32_t max_walkers,
                                                        std::vector<std::uint32_t> ids) {
    auto base = syn::make_population_types();
    REQUIRE_MESSAGE(base.ok(), (base.ok() ? std::string() : base.error()));
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.house_groups(std::vector<sim::HouseGroup>(base.value().registry->house_groups().begin(),
                                                base.value().registry->house_groups().end()));
    for (sim::BuildingType t : base.value().registry->types()) {
        if (t.id == base.value().pioneer_house) {
            t.has_prod = true;
            t.prod.kind = sim::BuildingClass::Other;
            t.prod.max_figures = {max_walkers, 0, 0};
            t.prod.figure_type_count = {static_cast<std::uint32_t>(ids.size()), 0, 0};
            t.prod.figure_ids = ids;
            t.prod.figure_ids.resize(9, 0);
        }
        b.add(t);
    }
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    return built.value();
}

sim::BuildingType house_type(const sim::TypeRegistry& reg) {
    for (const sim::BuildingType& t : reg.types()) {
        if (t.cls == sim::RuntimeClass::House && t.prodtype == 97) {
            return t;
        }
    }
    FAIL("no pioneer house");
    return {};
}

} // namespace

TEST_CASE("house walker facts: read from the production data and part of the registry digest") {
    const auto a = house_registry(2, {203, 204});
    const sim::BuildingType t = house_type(*a);
    const sim::HouseWalkerFacts f = sim::house_walker_facts(t);
    CHECK(f.walker_max == 2);
    CHECK(f.variants == 2);
    REQUIRE(f.figure_ids.size() == 2);
    CHECK(f.figure_ids[0] == 203);
    CHECK(f.figure_ids[1] == 204);
    CHECK(sim::house_walker_problem(t).empty());
    // A byte mask on the count (10a): 258 figure types is 2.
    sim::BuildingType masked = t;
    masked.prod.figure_type_count[0] = 258;
    CHECK(sim::house_walker_facts(masked).variants == 2);
    // A type without production data: nothing, never an error.
    sim::BuildingType plain = t;
    plain.has_prod = false;
    CHECK(sim::house_walker_facts(plain).walker_max == 0);
    CHECK(sim::house_walker_problem(plain).empty());
    // The registry digest covers the maximum, the count and the ids.
    const std::uint64_t base = a->digest();
    CHECK(house_registry(2, {203, 204})->digest() == base);
    CHECK(house_registry(3, {203, 204})->digest() != base);
    CHECK(house_registry(2, {203})->digest() != base);
    CHECK(house_registry(2, {203, 205})->digest() != base);
    CHECK(house_registry(2, {204, 203})->digest() != base);
}

TEST_CASE("house walker facts: a maximum without a figure type is a registry error") {
    // 10a (P): control first, then the one-predicate mutant (walker maximum 1 with zero figures).
    CHECK(house_registry(0, {})->digest() != 0); // max 0 with no figure: a house that never spawns
    auto base = syn::make_population_types();
    REQUIRE(base.ok());
    sim::TypeRegistry::Builder b;
    b.config(syn::population_config());
    b.walk_tables(sim::synthetic_walk_tables());
    b.house_groups(std::vector<sim::HouseGroup>(base.value().registry->house_groups().begin(),
                                                base.value().registry->house_groups().end()));
    for (sim::BuildingType t : base.value().registry->types()) {
        if (t.id == base.value().pioneer_house) {
            t.has_prod = true;
            t.prod.kind = sim::BuildingClass::Other;
            t.prod.max_figures = {1, 0, 0};
            t.prod.figure_type_count = {0, 0, 0};
            t.prod.figure_ids.assign(9, 0);
        }
        b.add(t);
    }
    auto bad = b.build();
    REQUIRE_FALSE(bad.ok());
    CHECK(contains(bad.error(), "walker maximum 1 with no figure type"));
}

TEST_CASE("house walker facts: the shipped house types") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto loaded =
        anno::sim_load::load_original_registry(*dir, anno::sim_load::FigureEntries::Required);
    REQUIRE_MESSAGE(loaded.error.empty(), loaded.error);
    std::size_t houses = 0;
    for (const sim::BuildingType& t : loaded.types->types()) {
        if (t.cls != sim::RuntimeClass::House) {
            continue;
        }
        ++houses;
        const sim::HouseWalkerFacts f = sim::house_walker_facts(t);
        CHECK(f.figure_ids.size() == f.variants); // every listed variant has an id
        CHECK((f.walker_max == 0 || f.variants > 0));
        if (t.id.v == 501) { // PIONIER: one walker, four figure types (P, probed from Anlagen.dat)
            CHECK(f.walker_max == 1);
            REQUIRE(f.variants == 4);
            CHECK(f.figure_ids[0] == 301);
            CHECK(f.figure_ids[3] == 298);
        }
        if (t.id.v == 585) {
            CHECK(f.walker_max == 2);
            CHECK(f.variants == 3);
        }
    }
    CHECK(houses == 81);
}

namespace {

/// A created world with a walker-capable house, one resident figure and billed/stage state.
struct Populated {
    std::shared_ptr<const sim::TypeRegistry> registry;
    sim::World world;
    sim::BuildingId house;
    sim::FigureId figure;
};

sim::Building* find_house(sim::World& w) {
    for (auto& slot : sim::WorldTestAccess::islands(w)[0]->cities[0]->index.lists[3].slots) {
        if (slot.has_value() && std::holds_alternative<sim::House>(slot->payload)) {
            return &*slot;
        }
    }
    return nullptr;
}

/// The house of find_house, which every case here has (fails the case otherwise).
sim::House& house_of(sim::World& w) {
    sim::Building* const house = find_house(w);
    REQUIRE(house != nullptr);
    return std::get<sim::House>(house->payload);
}

Populated populate(bool save_billed) {
    auto base = syn::make_population_types();
    REQUIRE(base.ok());
    syn::SyntheticTypes t = base.value();
    t.registry = house_registry(2, {203, 204});
    sim::Rules rules;
    rules.save_billed_goods = save_billed;
    sim::ScenarioSpec spec = syn::g7_scenario(t);
    // The scenario's own house payload carries its walker slots (sized by the type's maximum).
    std::get<sim::House>(*spec.islands[0].cities[0].buildings[0].payload)
        .walkers.assign(2, sim::FigureId{});
    auto made = sim::World::create(spec, t.registry, syn::population_config(), rules);
    REQUIRE_MESSAGE(made.ok(), (made.ok() ? std::string() : made.error()));
    sim::World w = std::move(made).value();
    sim::Building* house = find_house(w);
    REQUIRE(house != nullptr);
    auto& h = std::get<sim::House>(house->payload);
    REQUIRE(h.walkers.size() == 2); // sized by the type's maximum
    sim::Figure f;
    f.id = sim::FigureId{sim::WorldTestAccess::ids(w).next_figure++};
    f.kind = 203;
    f.owner = sim::PlayerId{0};
    f.home = house->id;
    f.home_counter = 1;
    f.job = 0x35;
    sim::ResidentState r;
    r.step = sim::ResidentStep::ToHome; // a walking step: the job validators allow a route here
    r.wait = 0;
    r.route = {sim::FixedPos{256 * 111, 256 * 111}};
    r.next = 1;
    r.along = 7;
    r.target = house->id;
    r.cargo = {sim::ResidentCargo{26, 11}, sim::ResidentCargo{0, 0}, sim::ResidentCargo{27, 0}};
    f.resident = r;
    REQUIRE(sim::add_figure(sim::WorldTestAccess::figures(w), f, w.config(), w.rules()) !=
            sim::kNoSlot);
    h.walkers[1] = f.id;
    sim::City& city = *sim::WorldTestAccess::islands(w)[0]->cities[0];
    city.billed[26] = 300;
    city.billed[27] = 5;
    city.stage_mask = 0x5;
    city.stage_stamp = 77;
    return Populated{t.registry, std::move(w), house->id, f.id};
}

} // namespace

TEST_CASE("stream 128: a populated world saves and loads with billed saved (the default)") {
    Populated p = populate(true);
    REQUIRE(p.world.check_invariants() == "");
    const std::vector<std::uint8_t> bytes = p.world.save();
    auto back = sim::World::load(bytes, p.registry);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    sim::World w2 = std::move(back).value();
    CHECK(w2.check_invariants() == "");
    CHECK(w2.state_hash() == p.world.state_hash());
    CHECK(w2.save() == bytes); // byte for byte
    const sim::City& city = *w2.islands()[0]->cities[0];
    CHECK(city.billed[26] == 300);
    CHECK(city.billed[27] == 5);
    CHECK(city.stage_mask == 0x5);
    CHECK(city.stage_stamp == 77);
    const sim::Figure* f = sim::find_figure(w2.figures(), p.figure);
    REQUIRE(f != nullptr);
    REQUIRE(f->resident.has_value());
    CHECK(f->resident->step == sim::ResidentStep::ToHome);
    CHECK(f->resident->cargo.size() == 3);
    CHECK(f->resident->cargo[2] == sim::ResidentCargo{27, 0});
    // Each new field is hashed: changing it moves the world hash.
    const std::uint64_t h0 = p.world.state_hash();
    sim::City& c = *sim::WorldTestAccess::islands(p.world)[0]->cities[0];
    c.billed[26] += 1;
    const std::uint64_t h1 = p.world.state_hash();
    CHECK(h1 != h0);
    c.billed[26] -= 1;
    c.stage_mask ^= 1;
    CHECK(p.world.state_hash() != h0);
    c.stage_mask ^= 1;
    c.stage_stamp += 1;
    CHECK(p.world.state_hash() != h0);
    c.stage_stamp -= 1;
    house_of(p.world).walkers[0] = sim::FigureId{};
    CHECK(p.world.state_hash() == h0); // an empty slot stays empty: same bytes
    CHECK(p.world.check_invariants() == "");
}

TEST_CASE("stream 128: with save_billed_goods off the counters are not saved and not hashed") {
    Populated on = populate(true);
    Populated off = populate(false);
    REQUIRE(on.world.check_invariants() == "");
    REQUIRE(off.world.check_invariants() == "");
    // The two flags give two streams (the Rules byte differs, and one has the counters).
    CHECK(on.world.state_hash() != off.world.state_hash());
    CHECK(on.world.save().size() == off.world.save().size() + 8 + 56 * 4);
    // Not hashed: changing billed leaves the world hash alone; the in-memory counters are live.
    const std::uint64_t h0 = off.world.state_hash();
    sim::WorldTestAccess::islands(off.world)[0]->cities[0]->billed[26] += 12345;
    CHECK(off.world.state_hash() == h0);
    // Reload: the city starts at zero billed, everything else is restored.
    const std::vector<std::uint8_t> bytes = off.world.save();
    auto back = sim::World::load(bytes, off.registry);
    REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
    const sim::World& w2 = back.value();
    CHECK(w2.check_invariants() == "");
    const sim::City& city = *w2.islands()[0]->cities[0];
    REQUIRE(city.billed.size() == 56);
    for (const std::uint32_t b : city.billed) {
        CHECK(b == 0);
    }
    CHECK(city.stage_mask == 0x5);
    CHECK(city.stage_stamp == 77);
    CHECK(w2.state_hash() == h0);
    CHECK_FALSE(w2.rules().save_billed_goods);
    // The same save made with the flag on keeps the counters.
    Populated on2 = populate(true);
    auto back_on = sim::World::load(on2.world.save(), on2.registry);
    REQUIRE(back_on.ok());
    CHECK(back_on.value().islands()[0]->cities[0]->billed[26] == 300);
}

TEST_CASE("stream 128: the invariants name a broken walker or resident fact") {
    Populated p = populate(true);
    REQUIRE(p.world.check_invariants() == ""); // the control
    {
        sim::World w = p.world;
        house_of(w).walkers[0] = sim::FigureId{999};
        CHECK(contains(w.check_invariants(), "is not a live figure holding that slot"));
    }
    {
        sim::World w = p.world;
        sim::find_figure(sim::WorldTestAccess::figures(w), p.figure)->home_counter = 0;
        CHECK(contains(w.check_invariants(), "does not name it"));
    }
    {
        sim::World w = p.world;
        sim::find_figure(sim::WorldTestAccess::figures(w), p.figure)->resident->cargo.pop_back();
        CHECK(contains(w.check_invariants(), "SimConfig.resident_cargo_slots is 3"));
    }
    {
        sim::World w = p.world;
        sim::find_figure(sim::WorldTestAccess::figures(w), p.figure)->resident->cargo[0].goods =
            5000;
        CHECK(contains(w.check_invariants(), "5000 is not in the goods table"));
        // An infrastructure goods above the warehouse goods is a legal cargo (a public building's
        // supply, real data: S01_01_01 carries goods 82).
        sim::World ok = p.world;
        sim::find_figure(sim::WorldTestAccess::figures(ok), p.figure)->resident->cargo[0].goods =
            static_cast<sim::GoodsId>(ok.config().warehouse_goods + 1);
        CHECK(ok.check_invariants() == "");
    }
    {
        sim::World w = p.world;
        sim::find_figure(sim::WorldTestAccess::figures(w), p.figure)->resident->cargo[1].amount_88 =
            4;
        CHECK(contains(w.check_invariants(), "an empty slot (goods 0) carries an amount"));
    }
    {
        sim::World w = p.world;
        sim::WorldTestAccess::islands(w)[0]->cities[0]->billed.pop_back();
        CHECK(contains(w.check_invariants(), "billed: 55 counters"));
    }
    {
        // Surplus slots are legal since L4 (a type change trims lazily, never shrinks the table).
        sim::World w = p.world;
        house_of(w).walkers.push_back(sim::FigureId{});
        CHECK(w.check_invariants() == "");
    }
    {
        // Too few slots is as wrong as too many (the vector is sized by the type's maximum).
        sim::World w = p.world;
        house_of(w).walkers.pop_back();
        CHECK(contains(w.check_invariants(), "walkers has 1 entries, the type needs 2"));
    }
    {
        // A live house walker without a home is legal since L4: the house destructor detaches the
        // walkers (0x10075bc0), they live on and end at their next arrival.
        sim::World w = p.world;
        house_of(w).walkers[1] = sim::FigureId{};
        sim::find_figure(sim::WorldTestAccess::figures(w), p.figure)->home = sim::BuildingId{};
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("stream 128: make_payload sizes the walker slots by the type maximum") {
    const auto reg = house_registry(2, {203, 204});
    const sim::BuildingType t = house_type(*reg);
    const sim::ClassInfo& ci = reg->class_info(sim::RuntimeClass::House);
    const sim::Payload p = sim::make_payload(t, ci, syn::population_config(), *reg);
    const auto& h = std::get<sim::House>(p);
    CHECK(h.walkers == std::vector<sim::FigureId>(2, sim::FigureId{}));
    CHECK(sim::payload_problem(p, t, ci, syn::population_config(), *reg).empty());
    const auto reg3 = house_registry(3, {203, 204});
    CHECK(std::get<sim::House>(
              sim::make_payload(house_type(*reg3), ci, syn::population_config(), *reg3))
              .walkers.size() == 3);
    // A registry without production data: no walkers.
    sim::TypeRegistry::Builder plain;
    plain.add_plain(sim::TypeId{1}, 97);
    const auto preg = plain.build().value();
    CHECK(std::get<sim::House>(
              sim::make_payload(*preg->find(sim::TypeId{1}), ci, sim::SimConfig{}, *preg))
              .walkers.empty());
}
