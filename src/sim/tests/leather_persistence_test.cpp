// MAP-NEEDS-2 N5: persistence, replay and refusal of the SYNTHETIC MECHANICS FIXTURE hide ->
// leather -> stall -> households (docs/sim/early-needs.md section 13). Labelled hides; no claim
// about hunters.
#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

#include <anno/sim/replay.hpp>

#include "evidence_out.hpp"
#include "leather_town.hpp"

namespace {

namespace sim = anno::sim;
using namespace leather_town;

constexpr std::uint32_t kHides = 8192; // the LABELLED hide stock
constexpr std::uint64_t kSteps = 2000; // 100 000 ticks

std::string hex64(std::uint64_t v) {
    std::ostringstream o;
    o << std::hex << v;
    return o.str();
}

/// The named in-flight states of the TARGET chain, found on the live world. Every figure
/// predicate is scoped (C1): alive, owner = the target city's owner, home in the target city, and
/// the job/home the state names; cargo amounts must be positive. A rival's or another city's
/// loaded figure, a figure of the right goods with the wrong home or job, and an empty resident
/// satisfy nothing.
struct Flight {
    bool worker = false;    // a carrier OF THE TANNERY carrying hides
    bool processor = false; // the tannery holds a full batch AND finished leather
    bool delivery = false;  // leather on a carrier of the tannery or a market cart of the town
    bool resident = false;  // a walker of a target house carrying a positive leather amount
};

Flight flight_of(const Leather& L) {
    const Cloth& c = *L.t;
    Flight f;
    for (const auto& s : c.w.figures().list.slots) {
        if (!s || !in_chain(c, *s)) {
            continue;
        }
        if (s->job == sim::kJobWorkshopCarrier && s->home == L.tannery &&
            s->cargo_goods == kHaeute && s->cargo_88 > 0 && !s->resident.has_value()) {
            f.worker = true;
        }
        if (!s->resident.has_value() && s->cargo_goods == kLeder && s->cargo_88 > 0 &&
            ((s->job == sim::kJobWorkshopCarrier && s->home == L.tannery) ||
             s->job == sim::kJobMarketCart)) {
            f.delivery = true;
        }
        if (s->resident.has_value() && c.houses.count(s->home) != 0) {
            for (const sim::ResidentCargo& rc : s->resident->cargo) {
                if (rc.goods == kLeder && rc.amount_88 > 0) {
                    f.resident = true;
                }
            }
        }
    }
    const sim::Building* tb = c.isl->find_building(L.tannery);
    if (tb != nullptr) {
        const sim::Workshop& w = std::get<sim::Workshop>(tb->payload);
        const sim::BuildingType& t = *c.w.types().find(tb->type);
        f.processor = w.in[0] >= t.prod.input_per_step[0] && w.out[0] > 0;
    }
    return f;
}

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

struct Trace {
    std::vector<std::uint64_t> hashes; // every 10th step
    std::uint64_t final_hash = 0;
    std::uint64_t made = 0, taken = 0, store = 0, consumed = 0;
    std::uint32_t houses = 0;
    std::map<std::string, std::uint64_t> first; // first step of each in-flight state
};

} // namespace

TEST_CASE("synthetic mechanics fixture: reload at in-flight states equals the uninterrupted run") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // reference: uninterrupted, finding the first step of each named state
    Trace ref;
    {
        auto L = start_leather(*o, kHides, true, true);
        Cloth& c = *L->t;
        for (std::uint64_t i = 1; i <= kSteps; ++i) {
            step(c);
            const Flight f = flight_of(*L);
            const auto note = [&](const char* k, bool on) {
                if (on && ref.first.count(k) == 0) {
                    ref.first[k] = i;
                }
            };
            note("worker", f.worker);
            note("processor", f.processor);
            note("delivery", f.delivery);
            note("resident", f.resident);
            if (i % 10 == 0) {
                ref.hashes.push_back(c.w.state_hash());
            }
        }
        const LeatherLedger l = leather_ledger(*L);
        ref.final_hash = c.w.state_hash();
        ref.made = l.leather_made;
        ref.taken = l.leather_taken;
        ref.store = l.leather_store;
        ref.consumed = l.hide_consumed;
        ref.houses = houses_reached(c);
    }
    CHECK(ref.first["worker"] == 61); // measured steps of 50 ticks at stream 130
    CHECK(ref.first["processor"] == 90);
    CHECK(ref.first["delivery"] == 494);
    CHECK(ref.first["resident"] == 666); // scoped to the target houses (was 508: a rival walker)
    CHECK(ref.final_hash == 0x745c326a6dc7bc11ULL); // equals the main run's hash at 100 000 ticks
    for (const char* k : {"worker", "processor", "delivery", "resident"}) {
        std::cerr << "PIN flight " << k << " first step "
                  << (ref.first.count(k) != 0 ? ref.first[k] : 0) << "\n";
        REQUIRE_MESSAGE(ref.first.count(k) != 0, k);
    }
    // the cut run: save, load and continue at each of those steps, and one step after each
    std::set<std::uint64_t> cuts;
    for (const auto& [k, v] : ref.first) {
        cuts.insert(v);
        cuts.insert(v + 1);
    }
    auto L = start_leather(*o, kHides, true, true);
    Cloth& c = *L->t;
    Trace cut;
    std::size_t reloads = 0;
    for (std::uint64_t i = 1; i <= kSteps; ++i) {
        step(c);
        if (cuts.count(i) != 0) {
            // what the cut name says holds at its own step on this run too
            const Flight f = flight_of(*L);
            if (i == ref.first["worker"]) {
                CHECK(f.worker);
            }
            if (i == ref.first["processor"]) {
                CHECK(f.processor);
            }
            if (i == ref.first["delivery"]) {
                CHECK(f.delivery);
            }
            if (i == ref.first["resident"]) {
                CHECK(f.resident);
            }
            const std::uint64_t before = c.w.state_hash();
            const std::vector<std::uint8_t> bytes = c.w.save();
            auto back = sim::World::load(bytes, o->types);
            REQUIRE_MESSAGE(back.ok(), (back.ok() ? std::string() : back.error()));
            CHECK(back.value().state_hash() == before);
            CHECK(back.value().save() == bytes);
            c.w = std::move(back).value();
            rebind(c);
            ++reloads;
        }
        if (i % 10 == 0) {
            cut.hashes.push_back(c.w.state_hash());
        }
    }
    CHECK(reloads == cuts.size());
    CHECK(cut.hashes == ref.hashes);
    CHECK(c.w.state_hash() == ref.final_hash);
    const LeatherLedger l = leather_ledger(*L);
    CHECK(l.u_hides == 0);
    CHECK(l.u_made == 0);
    CHECK(l.u_loaded == 0);
    // the ledger is the sum over the reloads (the stats sink is the test's, not world state)
    CHECK(l.leather_made == ref.made);
    CHECK(l.leather_taken == ref.taken);
    CHECK(l.leather_store == ref.store);
    CHECK(l.hide_consumed == ref.consumed);
    CHECK(houses_reached(c) == ref.houses);
    std::cerr << "PIN reload cuts " << reloads << " hashes " << ref.hashes.size() << " final "
              << hex64(ref.final_hash) << " made " << ref.made << " taken " << ref.taken
              << " houses " << ref.houses << "\n";
}

TEST_CASE("synthetic mechanics fixture: recorded log replays, a changed legal command and a "
          "changed checkpoint diverge where named") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // The named setup (producers removed, the stall built by BuildCmd, the LABELLED hides) is in
    // the log's initial save; the tannery is built by a command INSIDE the log.
    auto L = start_leather(*o, kHides, false, true);
    Cloth& c = *L->t;
    auto started = sim::Recorder::start(c.w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    const sim::Command build =
        cmd(c, sim::BuildCmd{c.ref, {sim::PlacementWire{L->tannery_type, 279, 350, 0, 0, 0}}});
    REQUIRE(rec.submit(build).ok());
    std::vector<std::size_t> cp_entries;
    std::vector<std::uint64_t> cp_hashes;
    constexpr std::uint64_t kLogSteps = 900;
    for (std::uint64_t i = 0; i < kLogSteps; ++i) {
        rec.advance(100);
        if (i % 100 == 99) {
            rec.checkpoint();
            cp_entries.push_back(rec.log().entries.size() - 1);
            cp_hashes.push_back(rec.log().entries.back().hash);
        }
    }
    REQUIRE(cp_entries.size() == 9);
    constexpr std::array<std::uint64_t, 9> kPinned{
        0x4ac3d6d562a7b40bULL, 0x88b7304cb2ce4036ULL, 0x4bc4ff30cbb2c1b7ULL,
        0x021b8a43ca009cecULL, 0xbd3540868e6039bbULL, 0xf4921414a183294bULL,
        0x6d075c61586a21bbULL, 0x05559392598b74a7ULL, 0xe91e9002afeac25aULL};
    for (std::size_t i = 0; i < cp_hashes.size(); ++i) {
        CHECK_MESSAGE(cp_hashes[i] == kPinned[i], "checkpoint " << i << " " << hex64(cp_hashes[i]));
        std::cerr << "PIN log checkpoint " << i << " entry " << cp_entries[i] << " hash "
                  << hex64(cp_hashes[i]) << "\n";
    }
    const sim::ReplayLog& log = rec.log();
    CHECK(log.rules_digest == c.w.rules().digest());
    export_log("leather_fixture.log", log);
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    CHECK(read.value() == log);
    // 1. through bytes, in a fresh world: every checkpoint reproduced, the hides became leather
    {
        auto fresh = sim::World::load(read.value().initial_save, o->types, {});
        REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
        auto outcome = sim::replay(fresh.value(), read.value());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().checkpoints_total == 9);
        CHECK(outcome.value().checkpoints_matched == 9);
        CHECK(outcome.value().reproduced());
        CHECK(fresh.value().state_hash() == c.w.state_hash());
        std::uint32_t hides = 0;
        for (auto& isl : sim::WorldTestAccess::islands(fresh.value())) {
            if (isl && isl->index.v == kIsland7) {
                for (auto& ci : isl->cities) {
                    if (ci && ci->number.v == kCity1) {
                        hides = ci->stock.stock[kHaeute];
                    }
                }
            }
        }
        CHECK(hides < kHides);
        // the households hold more leather than at the start (the stock the log began with)
        auto init = sim::World::load(read.value().initial_save, o->types, {});
        REQUIRE(init.ok());
        const std::uint64_t before = house_leather(init.value(), *o->types);
        const std::uint64_t after = house_leather(fresh.value(), *o->types);
        std::cerr << "PIN replay hides " << hides << " house leather " << before << " -> " << after
                  << "\n";
        CHECK(after > before);
    }
    // 2. a LEGAL changed command: the same site, the stall type instead of the tannery (accepted by
    // the engine). The first checkpoint after it already differs; the hash tree names the owner's
    // player scope (the stall costs less) and the fresh world holds a stall, no tannery, at the
    // site
    {
        sim::ReplayLog changed = read.value();
        std::size_t cmd_entry = changed.entries.size();
        for (std::size_t i = 0; i < changed.entries.size(); ++i) {
            if (changed.entries[i].kind == sim::ReplayEntry::Kind::Commands) {
                cmd_entry = i;
                break;
            }
        }
        REQUIRE(cmd_entry < changed.entries.size());
        auto* b = std::get_if<sim::BuildCmd>(&changed.entries[cmd_entry].commands.at(0).body);
        REQUIRE(b != nullptr);
        REQUIRE(b->placements.size() == 1);
        CHECK(b->placements[0].type == L->tannery_type);
        b->placements[0].type = c.stall_type;
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), changed);
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        REQUIRE_FALSE(outcome.value().reproduced());
        CHECK(*outcome.value().diverged_entry == cp_entries[0]);
        CHECK(outcome.value().checkpoints_matched == 0);
        CHECK(outcome.value().expected_hash == cp_hashes[0]);
        CHECK(outcome.value().actual_hash != cp_hashes[0]);
        const auto init_a = sim::World::load(read.value().initial_save, o->types, {});
        const auto init_b = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE((init_a.ok() && init_b.ok()));
        const std::string path =
            sim::diff_trees(sim::tree_at(init_a.value(), read.value(), cp_entries[0], 8),
                            sim::tree_at(init_b.value(), changed, cp_entries[0], 8));
        std::cerr << "PIN changed command: first differing scope " << path << " expected "
                  << hex64(outcome.value().expected_hash) << " got "
                  << hex64(outcome.value().actual_hash) << "\n";
        // the first differing scope is the id allocator (the tannery's carriers take figure ids,
        // the stall has none); the owner's player scope differs too (the stall costs less)
        CHECK(path == "world/ids");
        CHECK(outcome.value().actual_hash == 0xe085069256b53d17ULL);
        const sim::HashTree ta = sim::tree_at(init_a.value(), read.value(), cp_entries[0], 8);
        const sim::HashTree tb = sim::tree_at(init_b.value(), changed, cp_entries[0], 8);
        const auto child = [](const sim::HashNode& n,
                              std::string_view name) -> const sim::HashNode* {
            for (const sim::HashNode& ch : n.children) {
                if (ch.name == name) {
                    return &ch;
                }
            }
            return nullptr;
        };
        const sim::HashNode* pa = child(ta.root, "players");
        const sim::HashNode* pb = child(tb.root, "players");
        REQUIRE((pa != nullptr && pb != nullptr));
        CHECK(pa->children.at(0).hash != pb->children.at(0).hash);
        bool stall_there = false, tannery_there = false;
        for (auto& isl : sim::WorldTestAccess::islands(fresh.value())) {
            if (isl && isl->index.v == kIsland7) {
                for (auto& ci : isl->cities) {
                    if (ci && ci->number.v == kCity1) {
                        const sim::Building* site =
                            isl->building_at(fresh.value().config().layout, ci->number, 279, 350);
                        stall_there = site != nullptr && site->type == c.stall_type;
                        tannery_there = site != nullptr && site->type == L->tannery_type;
                    }
                }
            }
        }
        CHECK(stall_there);
        CHECK_FALSE(tannery_there);
        export_log("leather_fixture_changed_command.log", changed);
    }
    // 3. one recorded checkpoint hash changed: reported at exactly that entry, the four before
    // match
    {
        sim::ReplayLog changed = read.value();
        changed.entries[cp_entries[4]].hash ^= 1;
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), changed);
        REQUIRE(outcome.ok());
        REQUIRE_FALSE(outcome.value().reproduced());
        CHECK(*outcome.value().diverged_entry == cp_entries[4]);
        CHECK(outcome.value().checkpoints_matched == 4);
        CHECK(outcome.value().actual_hash == cp_hashes[4]);
        export_log("leather_fixture_changed_checkpoint.log", changed);
    }
}

TEST_CASE("synthetic mechanics fixture: refused commands change nothing and replay as refused") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // direct: each refusal names its reason and leaves the world hash unchanged
    {
        auto L = start_leather(*o, kHides, true, true);
        Cloth& c = *L->t;
        run_to(c, 5000);
        const std::uint64_t h0 = c.w.state_hash();
        const std::int64_t money0 = c.w.players().find(c.city->owner)->money;
        // the tannery's own site is taken
        auto occ = c.w.apply(
            cmd(c, sim::BuildCmd{c.ref, {sim::PlacementWire{L->tannery_type, 279, 350, 0, 0, 0}}}));
        CHECK(occ.status == sim::Reject::Occupied);
        // another owner may not build in this city
        sim::Command foreign;
        foreign.issuer = sim::PlayerId{2};
        foreign.body =
            sim::BuildCmd{c.ref, {sim::PlacementWire{L->tannery_type, 270, 360, 0, 0, 0}}};
        CHECK(c.w.apply(foreign).status == sim::Reject::NotCityOwner);
        // a type that does not exist
        auto unk = c.w.apply(cmd(
            c, sim::BuildCmd{c.ref, {sim::PlacementWire{sim::TypeId{60000}, 270, 360, 0, 0, 0}}}));
        CHECK(unk.status == sim::Reject::UnknownType);
        CHECK(c.w.state_hash() == h0);
        CHECK(c.w.players().find(c.city->owner)->money == money0);
    }
    // recorded: the same run with and without a refused command in the middle ends in the same
    // state; the refused command is IN the log (it was issued), and the replay refuses it again
    std::uint64_t final_with = 0, final_without = 0;
    sim::ReplayLog log_with;
    for (int arm = 0; arm < 2; ++arm) {
        auto L = start_leather(*o, kHides, false, true);
        Cloth& c = *L->t;
        auto started = sim::Recorder::start(c.w);
        REQUIRE(started.ok());
        sim::Recorder rec = std::move(started).value();
        const sim::PlacementWire site{L->tannery_type, 279, 350, 0, 0, 0};
        REQUIRE(rec.submit(cmd(c, sim::BuildCmd{c.ref, {site}})).ok());
        for (int i = 0; i < 300; ++i) {
            rec.advance(100);
        }
        if (arm == 0) {
            const auto r = rec.submit(cmd(c, sim::BuildCmd{c.ref, {site}}));
            CHECK(r.status == sim::Reject::Occupied);
        }
        for (int i = 0; i < 300; ++i) {
            rec.advance(100);
        }
        rec.checkpoint();
        (arm == 0 ? final_with : final_without) = c.w.state_hash();
        if (arm == 0) {
            log_with = rec.log();
        }
        std::size_t command_entries = 0;
        for (const sim::ReplayEntry& e : rec.log().entries) {
            command_entries += e.kind == sim::ReplayEntry::Kind::Commands ? 1U : 0U;
        }
        CHECK(command_entries == (arm == 0 ? 2U : 1U));
    }
    CHECK(final_with == final_without);
    export_log("leather_fixture_refused.log", log_with);
    const auto bytes = sim::write_log(log_with);
    REQUIRE(bytes.ok());
    const auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    auto fresh = sim::World::load(read.value().initial_save, o->types, {});
    REQUIRE(fresh.ok());
    auto outcome = sim::replay(fresh.value(), read.value());
    REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
    CHECK(outcome.value().checkpoints_total == 1);
    CHECK(outcome.value().reproduced());
    CHECK(fresh.value().state_hash() == final_with);
    CHECK(final_with ==
          0xf4921414a183294bULL); // equals checkpoint 5 of the log above (600 advances)
    std::cerr << "PIN refused final hash " << hex64(final_with) << "\n";
}

TEST_CASE("synthetic mechanics fixture: scoped in-flight predicates and ledgers (controls)") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto L = start_leather(*o, kHides, true, true);
    Cloth& c = *L->t;
    const sim::PlayerId mine = c.city->owner;
    const sim::PlayerId rival_owner{kOwner};
    // a building of the rival's town (island 3, owner 2) and a house of the target town
    sim::BuildingId rival_home;
    for (auto& isl : sim::WorldTestAccess::islands(c.w)) {
        if (isl && isl->index.v == kIsland) {
            for (auto& ci : isl->cities) {
                if (ci && ci->owner == rival_owner) {
                    for (auto& l : ci->index.lists) {
                        for (auto& b : l.slots) {
                            if (b && !rival_home.valid()) {
                                rival_home = b->id;
                            }
                        }
                    }
                }
            }
        }
    }
    REQUIRE(rival_home.valid());
    REQUIRE_FALSE(c.houses.empty());
    const sim::BuildingId house = c.houses.begin()->first;
    const auto none = [&] {
        const Flight f = flight_of(*L);
        const LeatherLedger l = leather_ledger(*L);
        return !f.worker && !f.delivery && !f.resident && l.hide_cargo == 0 && l.leather_cart == 0;
    };
    // the world at tick 0 holds no figure: no state is satisfied
    CHECK(none());
    // NEGATIVE controls, loaded with the right goods and amount but the wrong scope. Each is
    // checked alone, so one satisfying predicate cannot hide behind another.
    const sim::ResidentCargo loaded{kLeder, 256}, empty{kLeder, 0};
    struct Wrong {
        const char* name;
        sim::PlayerId owner;
        sim::BuildingId home;
        std::uint32_t job;
        sim::GoodsId goods;
        std::optional<sim::ResidentCargo> rc;
    };
    const std::vector<Wrong> wrongs{
        {"rival carrier with hides", rival_owner, rival_home, sim::kJobWorkshopCarrier, kHaeute,
         std::nullopt},
        {"rival owner at the target tannery", rival_owner, L->tannery, sim::kJobWorkshopCarrier,
         kHaeute, std::nullopt},
        {"target owner at a rival home", mine, rival_home, sim::kJobWorkshopCarrier, kHaeute,
         std::nullopt},
        {"target tannery carrier with the wrong job", mine, L->tannery, 0x2e, kHaeute,
         std::nullopt},
        {"rival cart with leather", rival_owner, rival_home, sim::kJobMarketCart, kLeder,
         std::nullopt},
        {"rival carrier with leather", rival_owner, rival_home, sim::kJobWorkshopCarrier, kLeder,
         std::nullopt},
        {"target-owner cart homed in the rival town", mine, rival_home, sim::kJobMarketCart, kLeder,
         std::nullopt},
        {"rival resident with loaded leather", rival_owner, rival_home, sim::kJobResidentCode, 0,
         loaded},
        {"rival owner at a target house", rival_owner, house, sim::kJobResidentCode, 0, loaded},
        {"target resident with an EMPTY leather cargo", mine, house, sim::kJobResidentCode, 0,
         empty},
        {"target resident with another goods", mine, house, sim::kJobResidentCode, 0,
         sim::ResidentCargo{kStoffe, 256}},
    };
    for (const Wrong& w : wrongs) {
        inject(c, w.owner, w.home, w.job, w.goods, w.goods == 0 ? 0U : 256U, w.rc);
        const Flight f = flight_of(*L);
        // no in-flight STATE is satisfied by any wrong-scope figure
        CHECK_MESSAGE((!f.worker && !f.delivery && !f.resident), std::string(w.name));
        // the TARGET LEDGER counts cargo of its own chain whatever the job (the one case below: a
        // target-owner figure homed at the target tannery with the wrong job still holds hides
        // that belong to the chain), and nothing of any other scope
        const LeatherLedger l = leather_ledger(*L);
        const bool own_chain_cargo = std::string(w.name).find("wrong job") != std::string::npos;
        CHECK_MESSAGE(l.hide_cargo == (own_chain_cargo ? 256U : 0U), std::string(w.name));
        CHECK_MESSAGE(l.leather_cart == 0, std::string(w.name));
        remove_figures(c);
    }
    // POSITIVE controls: the actual target figures satisfy exactly their own state and enter the
    // target ledger
    inject(c, mine, L->tannery, sim::kJobWorkshopCarrier, kHaeute, 256);
    {
        const Flight f = flight_of(*L);
        CHECK(f.worker);
        CHECK_FALSE(f.delivery);
        CHECK_FALSE(f.resident);
        CHECK(leather_ledger(*L).hide_cargo == 256);
    }
    remove_figures(c);
    inject(c, mine, L->tannery, sim::kJobWorkshopCarrier, kLeder, 256);
    {
        const Flight f = flight_of(*L);
        CHECK(f.delivery);
        CHECK_FALSE(f.worker);
        CHECK(leather_ledger(*L).leather_cart == 256);
    }
    remove_figures(c);
    inject(c, mine, house, sim::kJobResidentCode, 0, 0, loaded);
    {
        const Flight f = flight_of(*L);
        CHECK(f.resident);
        CHECK_FALSE(f.worker);
        CHECK_FALSE(f.delivery);
    }
    remove_figures(c);
    // a rival's loaded figure does not change the target ledger either
    const LeatherLedger before = leather_ledger(*L);
    inject(c, rival_owner, rival_home, sim::kJobWorkshopCarrier, kHaeute, 256);
    inject(c, rival_owner, rival_home, sim::kJobMarketCart, kLeder, 256);
    const LeatherLedger after = leather_ledger(*L);
    CHECK(after.hide_cargo == before.hide_cargo);
    CHECK(after.leather_cart == before.leather_cart);
    CHECK(after.u_hides == before.u_hides);
    CHECK(after.u_loaded == before.u_loaded);
}
