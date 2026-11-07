// Stream 137 (MAP-EXTRACT-2 part T2, docs/sim/iron-tools.md T1): iron mines place under the
// data-driven mountain rule, the shallow reserve and the edition setting decide what a mine may
// take, and a mine step's re-lookup searches every ore object of the island and materialises the
// deposit it binds. Layer 1 runs mine_step directly (hand vectors D2 to D4); layer 2 builds in the
// original endless02 island 1 (owner 8, the salt pilot) through PUBLIC commands. Layer 2 needs
// ANNO1503_GAME_DIR and skips with a message without it.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/mine_facts.hpp>
#include <anno/sim/mine_source.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>
#include <anno/test/game_dir.hpp>

#include "original_component.hpp"
#include "test_must.hpp"
#include "tutorial_town.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace tutorial_town;

namespace {

constexpr sim::PlayerId kPlayer{8};
constexpr sim::CityRef kCity{sim::IslandId{1}, sim::CityNo{0}};
constexpr std::uint64_t kAdvanceMs = 100; // 50 ticks; a mine step is 500 ticks = 10 advances

// ---- layer 1: mine_step on hand-derived vectors (D2 to D4 of docs/sim/iron-tools.md T1.1) ------

sim::ProductionType shallow_iron() {
    sim::ProductionType t;
    t.kind = sim::BuildingClass::Mine;
    t.outputs = {"EISENERZ"};
    t.output_per_step = {21, 0};
    t.capacity = {7 << 8, 0}; // 1,792 raw
    return t;
}

constexpr std::uint32_t kStoredRaw = 0x7d000; // 2,000 t = 512,000 raw (D1)

TEST_CASE("iron mine: D2 deep sees the whole deposit and a shallow mine all but the reserve") {
    const sim::Deposit d{"EISENERZ", 5, 5, kStoredRaw, true};
    CHECK(sim::deposit_remaining(d, true) == 512000);
    CHECK(sim::deposit_remaining(d, false) == 102400); // 400 t: 2,000 t minus the 1,600 t reserve
    // the flag decides: an unflagged deposit has no reserve
    const sim::Deposit plain{"EISENERZ", 5, 5, kStoredRaw, false};
    CHECK(sim::deposit_remaining(plain, false) == 512000);
    // deep or shallow comes from the type's research and the ware, not from an id
    sim::ProductionType iron = shallow_iron();
    CHECK_FALSE(sim::mine_is_deep(iron, 0));
    CHECK(sim::mine_is_deep(iron, 0x39));
    sim::ProductionType salt = shallow_iron();
    salt.outputs = {"SALZSTEIN"};
    CHECK(sim::mine_is_deep(salt, 0)); // every non-iron mine is deep
}

TEST_CASE("iron mine: D3 depletion exists only with the add-on edition setting") {
    const sim::ProductionType t = shallow_iron();
    const sim::MineArea a{10, 10, 3, 4};
    for (const bool depletes : {false, true}) {
        INFO("depletes " << depletes);
        std::vector<sim::Deposit> ds = {{"EISENERZ", 11, 11, kStoredRaw, true}};
        sim::Mine m;
        m.deep = false;
        m.deposit = 0;
        for (std::uint32_t k = 1; k <= 100; ++k) {
            m.out = 0; // the output store never limits
            const sim::StepResult r = sim::mine_step(m, t, k * 500, ds, a, depletes);
            REQUIRE(r.made[0] == 21);
        }
        // off: the full rate and the deposit untouched; on: 21 raw lower per step
        CHECK(ds[0].remaining == (depletes ? kStoredRaw - 100U * 21U : kStoredRaw));
    }
    // the control for the deep mine: it takes without lowering the deposit in either setting
    std::vector<sim::Deposit> ds = {{"EISENERZ", 11, 11, kStoredRaw, true}};
    sim::Mine deep;
    deep.deep = true;
    deep.deposit = 0;
    for (std::uint32_t k = 1; k <= 100; ++k) {
        deep.out = 0;
        static_cast<void>(sim::mine_step(deep, t, k * 500, ds, a, true));
    }
    CHECK(ds[0].remaining == kStoredRaw);
}

struct StepNotices {
    std::vector<std::pair<std::uint32_t, std::uint16_t>> notices; // (step number, notice)
    std::vector<std::uint32_t> takes;                             // raw made per step
};

// A relookup that returns the one deposit again (the only ore in reach), kept even when spent.
std::optional<std::size_t> same_deposit(void*) {
    return std::size_t{0};
}

TEST_CASE("iron mine: D4 exhaustion of a shallow deposit and the short step and the one notice") {
    const sim::ProductionType t = shallow_iron();
    const sim::MineArea a{10, 10, 3, 4};
    std::vector<sim::Deposit> ds = {{"EISENERZ", 11, 11, sim::kDepositReserve + 102400, true}};
    sim::Mine m;
    m.deep = false;
    m.deposit = 0;
    const sim::MineRelookup relook{&same_deposit, nullptr};
    StepNotices n;
    for (std::uint32_t step = 1; step <= 4890; ++step) {
        m.out = 0;
        const sim::StepResult r = sim::mine_step(m, t, step * 500, ds, a, true, &relook);
        n.takes.push_back(r.made.empty() ? 0 : r.made[0]);
        if (r.notice != 0) {
            n.notices.emplace_back(step, r.notice);
        }
    }
    // 102,400 / 21 = 4,876 full steps (102,396 taken), a short step 4,877 taking 4, then nothing
    for (std::uint32_t step = 1; step <= 4876; ++step) {
        REQUIRE_MESSAGE(n.takes[step - 1] == 21, "step " << step);
    }
    CHECK(n.takes[4876] == 4); // step 4,877
    for (std::uint32_t step = 4878; step <= 4890; ++step) {
        CHECK(n.takes[step - 1] == 0);
    }
    // the notice 0x15B (deep ore is still held) fires exactly once, on the short step, and the
    // idle steps after it are silent (they never look up again)
    REQUIRE(n.notices.size() == 1);
    CHECK(n.notices[0].first == 4877);
    CHECK(n.notices[0].second == 0x15B);
    CHECK(sim::deposit_remaining(ds[0], false) == 0);
    CHECK(ds[0].remaining == sim::kDepositReserve);
    // the same fixture with depletion off never takes the short step and never raises a notice
    std::vector<sim::Deposit> off = {{"EISENERZ", 11, 11, sim::kDepositReserve + 102400, true}};
    sim::Mine m2;
    m2.deep = false;
    m2.deposit = 0;
    std::uint32_t notices = 0;
    for (std::uint32_t step = 1; step <= 4890; ++step) {
        m2.out = 0;
        const sim::StepResult r = sim::mine_step(m2, t, step * 500, off, a, false, &relook);
        notices += r.notice != 0 ? 1U : 0U;
        REQUIRE(r.made[0] == 21);
    }
    CHECK(notices == 0);
}

// ---- layer 2: the original endless02 island 1, owner 8 (city 0), through PUBLIC commands --------

struct IronWorld {
    sim::World w;
    std::shared_ptr<const sim::TypeRegistry> types;
};

/// The pilot with `live_houses` (Rules::house_updates: the stage pass runs, stage 2C is reached
/// between steps 900 and 1000, docs/sim/iron-tools.md 5.1) advanced `advances` times of 100 ms.
std::optional<IronWorld> iron_world(bool live_houses, bool atomic, int advances) {
    auto o = load_original();
    if (!o) {
        return std::nullopt;
    }
    auto resolved = anno::sim_load::load_scenario_with_links(
        anno::test::read_file(o->dir / "Scenes" / "0011SingleEndlosAddon" / "endless02.sze"),
        o->templates, *o->types, o->cfg, {}, original_component::actor_free_units());
    REQUIRE(resolved);
    auto component =
        original_component::import_only(resolved.value().loaded.spec, o->types, o->cfg);
    REQUIRE_MESSAGE(component, (component ? "" : component.error()));
    sim::Rules rules;
    rules.build_atomic = atomic;
    rules.house_updates = live_houses;
    auto created = sim::World::create(component.value().spec, o->types, o->cfg, rules);
    REQUIRE_MESSAGE(created.ok(), (created.ok() ? std::string() : created.error()));
    sim::World world = std::move(created).value();
    if (world.script().state.context == sim::ScriptContext::Pending) {
        REQUIRE(world.initialize_scripts().empty());
    }
    for (int i = 0; i < advances; ++i) {
        world.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!world.faulted(), world.fault());
    }
    return IronWorld{std::move(world), o->types};
}

sim::Command build_cmd(std::uint32_t type, std::int32_t x, std::int32_t y, std::uint8_t rot = 0) {
    return sim::Command{
        kPlayer, sim::BuildCmd{kCity, {sim::PlacementWire{sim::TypeId{type}, x, y, rot, 0, 0}}}};
}
const sim::Island& isle(const sim::World& w) {
    return *w.islands()[1];
}
std::int64_t gold(const sim::World& w) {
    return test_must::must(w.players().find(kPlayer))->money;
}

TEST_CASE("iron mine: site survey (opt-in measure)") {
    const char* on = std::getenv("ANNO_IRON_SURVEY");
    if (on == nullptr || *on == '\0') {
        MESSAGE("skipping: set ANNO_IRON_SURVEY=1 to print the legal 1205 sites of the pilot");
        return;
    }
    auto s = iron_world(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    REQUIRE(s->w.construction_eligibility(kPlayer, sim::TypeId{1205}).allowed());
    const sim::Island& is = isle(s->w);
    std::cerr << "gold " << gold(s->w) << "\n";
    for (const sim::OreObject& o : is.ore_objects) {
        const sim::FigureType* ft = s->types->find_figure_type(o.type);
        if (ft != nullptr && ft->ware == "EISENERZ") {
            std::cerr << "ORE " << o.x << "," << o.y << " amount " << o.amount << "\n";
        }
    }
    for (std::int32_t y = 345; y <= 368; ++y) {
        for (std::int32_t x = 358; x <= 388; ++x) {
            for (std::uint8_t rot = 0; rot < 1; ++rot) {
                sim::World w = s->w;
                const sim::ApplyResult r = w.apply(build_cmd(1205, x, y, rot));
                if (!r.ok()) {
                    continue;
                }
                const sim::MineArea a{x, y, 3, 4};
                std::string reach;
                for (const sim::OreObject& o : is.ore_objects) {
                    const sim::FigureType* ft = s->types->find_figure_type(o.type);
                    if (ft != nullptr && ft->ware == "EISENERZ" &&
                        sim::deposit_in_reach(a, o.x, o.y)) {
                        reach += " (" + std::to_string(o.x) + "," + std::to_string(o.y) + ")";
                    }
                }
                std::cerr << "SITE " << x << "," << y << " rot " << int(rot) << " reach" << reach
                          << "\n";
            }
        }
    }
}

constexpr std::int32_t kSiteA = 366; // (366,353): the ore (375,353) only is in reach
constexpr std::int32_t kSiteAy = 353;
constexpr std::int32_t kSiteBoth = 371; // (371,354): both (375,353) and (379,361) are in reach
constexpr std::int32_t kSiteBothy = 354;
constexpr std::int32_t kSiteNone = 358; // (358,345): a legal site with no ore in reach
constexpr std::int32_t kSiteNoney = 345;

const sim::Mine& mine_of(const sim::World& w, sim::BuildingId id) {
    return std::get<sim::Mine>(test_must::must(w.find_building(id))->payload);
}

/// The first site of a window where `type` is answered with `want`, or nullopt.
std::optional<std::pair<std::int32_t, std::int32_t>>
find_site(const sim::World& base, std::uint32_t type, sim::Reject want) {
    for (std::int32_t y = 345; y <= 368; ++y) {
        for (std::int32_t x = 358; x <= 388; ++x) {
            sim::World w = base;
            if (w.apply(build_cmd(type, x, y)).status == want) {
                return std::pair{x, y};
            }
        }
    }
    return std::nullopt;
}

TEST_CASE("iron mine: a shallow iron mine places on a legal site and a deep one needs its "
          "research and gold and gems get a typed refusal") {
    auto s = iron_world(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& base = s->w;
    const sim::BuildingType& ty = *test_must::must(s->types->find(sim::TypeId{1205}));
    REQUIRE(base.construction_eligibility(kPlayer, sim::TypeId{1205}).allowed());
    // the legal control: charged once, shallow, bound to the one ore in reach (375,353)
    {
        sim::World w = base;
        const std::int64_t gold0 = gold(w);
        const sim::ApplyResult r = w.apply(build_cmd(1205, kSiteA, kSiteAy));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        CHECK(gold0 - gold(w) == static_cast<std::int64_t>(ty.cost_gold));
        const sim::Mine& m = mine_of(w, r.records.at(0).building);
        CHECK_FALSE(m.deep);
        REQUIRE(m.deposit.has_value());
        const sim::Deposit& d = isle(w).deposits.at(*m.deposit);
        CHECK(d.goods == "EISENERZ");
        CHECK(d.x == 375);
        CHECK(d.y == 353);
        CHECK(d.remaining == kStoredRaw); // 2,000 t on load (D1)
        CHECK(d.reserve);
        CHECK(sim::deposit_remaining(d, m.deep) == 102400); // a shallow mine may use 400 t (D2)
        const auto f = sim::mine_facts(w, r.records.at(0).building);
        REQUIRE(f.has_value());
        CHECK(f->bound);
        CHECK_FALSE(f->deep);
        CHECK(w.check_invariants().empty());
    }
    // an illegal site: refused before payment with the saved state byte-identical, next to the
    // legal control above
    {
        const auto bad = find_site(base, 1205, sim::Reject::TerrainRefused);
        REQUIRE(bad.has_value());
        sim::World w = base;
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd(1205, bad->first, bad->second));
        CHECK(r.status == sim::Reject::TerrainRefused);
        CHECK(w.save() == before);
    }
    // unaffordable: the same legal site answers CannotAfford and changes nothing
    {
        sim::World w = base;
        test_must::must(sim::WorldTestAccess::players(w).find(kPlayer))->money = 0;
        const auto before = w.save();
        CHECK(w.apply(build_cmd(1205, kSiteA, kSiteAy)).status == sim::Reject::CannotAfford);
        CHECK(w.save() == before);
    }
    // a legal site with no ore in reach: accepted, charged, created unbound and idle (section 22
    // of salt-extraction.md, the faithful default), like the salt mine
    {
        sim::World w = base;
        const sim::ApplyResult r = w.apply(build_cmd(1205, kSiteNone, kSiteNoney));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        CHECK_FALSE(mine_of(w, r.records.at(0).building).deposit.has_value());
        CHECK(isle(w).deposits.empty());
    }
    // gold and gem mines: Unsupported (the placement rule precedes the permission step), state
    // byte-identical, on the very site where the iron mine is accepted
    for (const std::uint32_t id : {1206U, 1212U, 1214U}) {
        sim::World w = base;
        const auto before = w.save();
        const sim::ApplyResult r = w.apply(build_cmd(id, kSiteA, kSiteAy));
        CHECK_MESSAGE(r.status == sim::Reject::Unsupported,
                      "type " << id << ": " << sim::reject_name(r.status));
        CHECK(w.save() == before);
    }
    // the deep mine 1216 needs research 57: refused by the permission step without it, accepted
    // and deep with it (the same site)
    {
        sim::World w = base;
        const auto before = w.save();
        CHECK(w.apply(build_cmd(1216, kSiteA, kSiteAy)).status == sim::Reject::ConstructionDenied);
        CHECK(w.save() == before);
        auto& research = test_must::must(sim::WorldTestAccess::players(w).find(kPlayer))->research;
        if (research.state.size() <= 57) {
            research.state.resize(58, 0);
        }
        research.state[57] = static_cast<std::uint8_t>(research.state[57] | 2U);
        const sim::ApplyResult r = w.apply(build_cmd(1216, kSiteA, kSiteAy));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        const sim::Mine& m = mine_of(w, r.records.at(0).building);
        CHECK(m.deep);
        REQUIRE(m.deposit.has_value());
        CHECK(sim::deposit_remaining(isle(w).deposits.at(*m.deposit), m.deep) == 512000); // D2
    }
}

TEST_CASE("iron mine: a stage-locked 1205 is refused next to the eligible control and the dry run "
          "equals the live apply") {
    auto locked = iron_world(false, false, 3);
    auto live = iron_world(true, false, 1000);
    auto dry = iron_world(true, true, 1000);
    if (!locked || !live || !dry) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // without the stage pass the player's stage flags stay at the start value: 2C is not reached
    CHECK(locked->w.construction_eligibility(kPlayer, sim::TypeId{1205}).status ==
          sim::BuildEligibilityStatus::StageLocked);
    {
        sim::World w = locked->w;
        const auto before = w.save();
        CHECK(w.apply(build_cmd(1205, kSiteA, kSiteAy)).status == sim::Reject::ConstructionDenied);
        CHECK(w.save() == before);
    }
    // the control: the same command in the world whose stage pass reached 2C
    REQUIRE(live->w.construction_eligibility(kPlayer, sim::TypeId{1205}).allowed());
    // dry run (build_atomic) and live apply of the same batch: same gold, buildings, deposits
    {
        const sim::Command two = sim::Command{
            kPlayer,
            sim::BuildCmd{kCity,
                          {sim::PlacementWire{sim::TypeId{1205}, kSiteA, kSiteAy, 0, 0, 0},
                           sim::PlacementWire{sim::TypeId{1205}, kSiteBoth, kSiteBothy, 0, 0, 0}}}};
        sim::World a = dry->w, p = live->w;
        const sim::ApplyResult ra = a.apply(two);
        const sim::ApplyResult rp = p.apply(two);
        // the second mine overlaps the first (3 x 4 at 366,353 and 371,354 do not): both accepted
        REQUIRE_MESSAGE(rp.ok(), sim::reject_name(rp.status));
        REQUIRE_MESSAGE(ra.ok(), sim::reject_name(ra.status));
        CHECK(gold(a) == gold(p));
        CHECK(isle(a).cache.by_id.size() == isle(p).cache.by_id.size());
        CHECK(isle(a).deposits.size() == isle(p).deposits.size());
        CHECK(isle(a).deposits.size() == 1); // both mines share the ore (375,353): one deposit
        // an atomic batch with a refused record changes nothing; the plain world keeps the first
        const auto bad = find_site(live->w, 1205, sim::Reject::TerrainRefused);
        REQUIRE(bad.has_value());
        const sim::Command mixed = sim::Command{
            kPlayer, sim::BuildCmd{kCity,
                                   {sim::PlacementWire{sim::TypeId{1205}, kSiteA, kSiteAy, 0, 0, 0},
                                    sim::PlacementWire{sim::TypeId{1205}, bad->first, bad->second,
                                                       0, 0, 0}}}};
        sim::World a2 = dry->w, p2 = live->w;
        const auto before = a2.save();
        CHECK_FALSE(a2.apply(mixed).ok());
        CHECK(a2.save() == before);
        const sim::ApplyResult pm = p2.apply(mixed);
        REQUIRE(pm.records.size() == 2);
        CHECK(pm.records[0].status == sim::Reject::None);
        CHECK(pm.records[1].status == sim::Reject::TerrainRefused);
    }
}

TEST_CASE("iron mine: a short deposit rebinds to an ore no mine has bound and materialises its "
          "deposit") {
    auto s = iron_world(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // ---- positive: the site reaches the ores (375,353) and (379,361); the mine binds the first,
    // its deposit is set to 4 visible raw, the short step re-looks up and materialises the second
    {
        sim::World w = s->w;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        const sim::ApplyResult r = w.apply(build_cmd(1205, kSiteBoth, kSiteBothy));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        const sim::BuildingId id = r.records.at(0).building;
        REQUIRE(isle(w).deposits.size() == 1);
        REQUIRE(mine_of(w, id).deposit == std::optional<std::size_t>{0});
        CHECK(isle(w).deposits[0].x == 375); // the first of two equal ores (tie: first)
        sim::WorldTestAccess::islands(w)[1]->deposits[0].remaining = sim::kDepositReserve + 4;
        static_cast<void>(w.drain_events());
        std::size_t rebound_at = 0;
        for (std::size_t i = 1; i <= 400 && rebound_at == 0; ++i) {
            w.advance(kAdvanceMs);
            REQUIRE_MESSAGE(!w.faulted(), w.fault());
            if (mine_of(w, id).deposit != std::optional<std::size_t>{0}) {
                rebound_at = i;
            }
        }
        REQUIRE(rebound_at != 0);
        // the second ore's deposit was materialised by the re-lookup (it had none before)
        REQUIRE(isle(w).deposits.size() == 2);
        CHECK(mine_of(w, id).deposit == std::optional<std::size_t>{1});
        CHECK(isle(w).deposits[1].x == 379);
        CHECK(isle(w).deposits[1].y == 361);
        CHECK(isle(w).deposits[0].remaining == sim::kDepositReserve); // the 4 raw were taken
        // the short step made its 4 raw; it found an ore with ore left, so no notice
        std::size_t notices = 0;
        for (const sim::SimEvent& e : w.drain_events()) {
            notices += e.kind == sim::SimEventKind::Notice ? 1U : 0U;
        }
        CHECK(notices == 0);
        // the next step takes 21 from the new deposit
        const std::uint32_t before = isle(w).deposits[1].remaining;
        for (int i = 0; i < 40; ++i) {
            w.advance(kAdvanceMs);
        }
        CHECK(isle(w).deposits[1].remaining < before);
        CHECK((before - isle(w).deposits[1].remaining) % 21U == 0);
        CHECK(isle(w).deposits[0].remaining == sim::kDepositReserve); // untouched since
        CHECK(w.check_invariants().empty());
    }
}

TEST_CASE(
    "iron mine: with only the spent ore in reach the mine keeps it and raises the notice once and "
    "idles and never recovers") {
    auto s = iron_world(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // ---- control: only the ore (375,353) in reach: the short step keeps the spent deposit, raises
    // the notice 0x15B exactly once, the mine idles, makes nothing more, and a new ore appearing
    // later is NOT picked up (an idle step never looks up again)
    {
        sim::World w = s->w;
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        const sim::ApplyResult r = w.apply(build_cmd(1205, kSiteA, kSiteAy));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        const sim::BuildingId id = r.records.at(0).building;
        sim::WorldTestAccess::islands(w)[1]->deposits[0].remaining = sim::kDepositReserve + 4;
        static_cast<void>(w.drain_events());
        for (int i = 0; i < 300; ++i) {
            w.advance(kAdvanceMs);
        }
        CHECK(mine_of(w, id).deposit == std::optional<std::size_t>{0}); // kept although spent
        CHECK(isle(w).deposits.size() == 1);
        CHECK(isle(w).deposits[0].remaining == sim::kDepositReserve);
        CHECK(stats.forestry.mine_made_88[id] == 4); // the short step's 4 raw and nothing else
        std::size_t notices = 0;
        for (const sim::SimEvent& e : w.drain_events()) {
            if (e.kind == sim::SimEventKind::Notice) {
                ++notices;
                CHECK(e.code == 0x15B);
            }
        }
        CHECK(notices == 1);
        // an ore appears in reach after the exhaustion: the idle mine does not rebind
        sim::Island& isl = *sim::WorldTestAccess::islands(w)[1];
        sim::OreObject extra;
        for (const sim::OreObject& o : isl.ore_objects) {
            if (o.x == 375 && o.y == 353) {
                extra = o;
            }
        }
        REQUIRE(extra.type != 0);
        extra.x = 372;
        extra.y = 351;
        isl.ore_objects.push_back(extra);
        REQUIRE(sim::mine_site_deposit_in_reach(w, sim::TypeId{1205}, kSiteA, kSiteAy, 0) ==
                std::optional<bool>{true});
        for (int i = 0; i < 300; ++i) {
            w.advance(kAdvanceMs);
        }
        CHECK(mine_of(w, id).deposit == std::optional<std::size_t>{0});
        CHECK(isle(w).deposits.size() == 1);
        CHECK(stats.forestry.mine_made_88[id] == 4);
        std::size_t later = 0;
        for (const sim::SimEvent& e : w.drain_events()) {
            later += e.kind == sim::SimEventKind::Notice ? 1U : 0U;
        }
        CHECK(later == 0); // no repeat notice
    }
}

TEST_CASE("iron mine: the add-on pilot lowers a shallow deposit by what the mine made and a "
          "rebuild does not refill it") {
    auto s = iron_world(true, false, 1000);
    if (!s) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World w = s->w;
    REQUIRE(sim::WorldTestAccess::settings(w).depletes()); // endless02 is an add-on edition file
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    const sim::ApplyResult r = w.apply(build_cmd(1205, kSiteA, kSiteAy));
    REQUIRE(r.ok());
    const sim::BuildingId id = r.records.at(0).building;
    for (int i = 0; i < 400; ++i) {
        w.advance(kAdvanceMs);
    }
    const std::uint64_t made = stats.forestry.mine_made_88[id];
    CHECK(made >= 21);
    CHECK(made % 21U == 0);
    CHECK(isle(w).deposits[0].remaining == kStoredRaw - made); // D3: 21 raw lower per full step
    // teardown and rebuild never refill the ore
    const std::uint32_t worked = isle(w).deposits[0].remaining;
    sim::DemolishCmd dem;
    dem.city = kCity;
    dem.tiles = {{kSiteA, kSiteAy}};
    REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    const sim::ApplyResult again = w.apply(build_cmd(1205, kSiteA, kSiteAy));
    REQUIRE_MESSAGE(again.ok(), sim::reject_name(again.status));
    REQUIRE(isle(w).deposits.size() == 1);
    CHECK(isle(w).deposits[0].remaining == worked);
    CHECK(mine_of(w, again.records.at(0).building).deposit == std::optional<std::size_t>{0});
}

} // namespace
