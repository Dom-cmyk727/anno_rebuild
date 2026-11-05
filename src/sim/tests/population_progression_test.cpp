// MAP-POP-CLOSE-2 K3: the first real pioneer -> settler upgrade in an ORIGINAL town
// (docs/sim/population-live.md section 25).
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/hash_tree.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/replay.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "evidence_out.hpp"
#include "tutorial_town.hpp"

namespace {

using namespace tutorial_town;

constexpr int kX = 226, kY = 251;

std::string join_ints(const std::vector<std::uint32_t>& v) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        out += (i ? "," : "") + std::to_string(v[i]);
    }
    return out;
}

/// A 21-entry table in the reference's run-length form ("value*count,...").
std::string rle(const std::array<std::int8_t, 21>& t) {
    std::string out;
    for (std::size_t i = 0; i < t.size();) {
        std::size_t j = i;
        while (j < t.size() && t[j] == t[i]) {
            ++j;
        }
        out += (out.empty() ? "" : ",") + std::to_string(int{t[i]}) + "*" + std::to_string(j - i);
        i = j;
    }
    return out;
}

std::string group_line(const char* name, const sim::PopulationView& g,
                       const std::vector<sim::GoodsId>& wares) {
    std::string needs, lookup;
    for (std::size_t j = 0; j < g.group->needs.size(); ++j) {
        const auto& n = g.group->needs[j];
        needs += (j ? "," : "") + std::to_string(n.ware) + ":" + std::to_string(n.menge) + ":" +
                 (n.force ? "1" : "0");
    }
    for (const sim::GoodsId w : wares) {
        const std::uint32_t slot =
            w < g.derived->lookup.size() ? g.derived->lookup[w] : g.derived->need_count;
        if (slot < g.derived->need_count) {
            lookup += (lookup.empty() ? "" : ",") + std::to_string(w) + ":" + std::to_string(slot);
        }
    }
    return std::string("grp ") + name + " num=" + std::to_string(g.group->number) +
           " needs=" + needs + " lookup=" + (lookup.empty() ? "none" : lookup) +
           " goods_end=" + std::to_string(g.derived->goods_end) +
           " infra_end=" + std::to_string(g.derived->infra_end);
}

/// The reference-vector inputs of one UpgradeCheck, from the registry and the engine's record: the
/// two `grp` lines and the `upcheck` line up to ` => ` (the expected part is the fixture's,
/// computed by tools/house_transition_ref.py).
std::vector<std::string> upcheck_inputs(const Original& o, const sim::SimConfig& cfg,
                                        const sim::HouseTransitionRecord& r) {
    const auto cur = o.types->population_view(r.old_group, cfg);
    const auto next = o.types->population_view(r.new_group, cfg);
    REQUIRE(cur.ok());
    REQUIRE(next.ok());
    std::vector<sim::GoodsId> wares;
    for (std::size_t j = 1; j < next.value().derived->infra_end; ++j) {
        wares.push_back(next.value().group->needs[j].ware);
    }
    return {group_line("P", cur.value(), wares), group_line("S", next.value(), wares),
            "upcheck K3-" + std::to_string(r.tick) +
                " tavern=" + std::to_string(o.types->population_roles().tavern_goods) +
                " e=" + std::to_string(r.check_e) + " stock=" + join_ints(r.check_stock) +
                " cur=P next=S nah=" + rle(next.value().group->nahrung) + " bed=" +
                rle(next.value().group->bedarf) + " inf=" + rle(next.value().group->infra)};
}

struct Opts {
    bool build_house = true;
    bool demolish_inn = false;
    bool demolish_supply = false; ///< demolish every stall, the chapel and the Inn
    std::optional<std::uint32_t>
        drain_holz_below; ///< build stands until the HOLZ store is below this
    std::optional<std::uint32_t> drain_werkzeug_below; ///< ... and the WERKZEUG store
};

constexpr std::size_t kWerkzeugK = 0, kHolzK = 1; // SimConfig::build_goods_first + k

/// One run of the Tutorial town. Not movable: the transport-stats sink and the world hold pointers.
struct Session {
    sim::World world;
    Town town;
    sim::TransportStats stats;
    sim::BuildingId house{};
    /// The build-goods store and spent counters before the house was built.
    std::array<std::uint32_t, 5> pre_store{};
    std::array<std::uint64_t, 5> pre_spent{};
    explicit Session(sim::World&& w) : world(std::move(w)) {}
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    [[nodiscard]] const sim::Building* house_building() const {
        return town.island->building_at(world.config().layout, town.city->number, kX, kY);
    }
    [[nodiscard]] std::uint32_t store(std::size_t k) const {
        return town.city->stock.stock[world.config().build_goods_first + k];
    }
    void advance_to(std::uint64_t tick) {
        while (world.clock().tick() < tick) {
            world.advance(100);
            REQUIRE_FALSE(world.faulted());
        }
    }
    [[nodiscard]] const sim::Player& player() const {
        return *world.players().find(town.city->owner);
    }
    [[nodiscard]] sim::ApplyResult build(sim::TypeId type, int x, int y) {
        return world.apply(sim::Command{
            town.city->owner, sim::BuildCmd{town.ref, {sim::PlacementWire{type, x, y, 0, 0, 0}}}});
    }
    [[nodiscard]] const sim::HouseTransitionRecord*
    transition(sim::HouseTransitionRecord::Kind kind) const {
        for (const auto& r : stats.population.transitions) {
            if (r.building == house && r.kind == kind) {
                return &r;
            }
        }
        return nullptr;
    }
    [[nodiscard]] std::size_t transitions_of_house() const {
        std::size_t n = 0;
        for (const auto& r : stats.population.transitions) {
            n += r.building == house ? 1U : 0U;
        }
        return n;
    }
};

/// Builds the `types` in turn on every free site of a window around the town (row by row) while the
/// build-goods store k is at or above `below`; returns the number built. Ordinary BuildCmds only,
/// so the materials leave the city store exactly as a player's builds would spend them.
int drain(Session& s, std::size_t k, std::uint32_t below, const std::vector<sim::TypeId>& types) {
    int built = 0;
    for (const sim::TypeId type : types) {
        for (int y = 200; y <= 300 && s.store(k) >= below; ++y) {
            for (int x = 170; x <= 270 && s.store(k) >= below; ++x) {
                if (std::abs(x - kX) < 3 && std::abs(y - kY) < 3) {
                    continue; // keep the neighbourhood of the house free of the drain
                }
                built += s.build(type, x, y).ok() ? 1 : 0;
            }
        }
    }
    return built;
}

std::unique_ptr<Session> start(const Original& o, const Opts& op) {
    auto s = std::make_unique<Session>(tutorial_world(o, true));
    s->town = find_town(s->world);
    s->world.set_transport_stats(&s->stats);
    for (std::size_t k = 0; k < s->pre_store.size(); ++k) {
        s->pre_store[k] = s->store(k);
        s->pre_spent[k] = s->town.city->build_goods[k].spent;
    }
    if (op.build_house) {
        const auto res = s->build(kPioneer, kX, kY);
        REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
        s->house = s->house_building()->id;
    }
    if (op.demolish_inn || op.demolish_supply) {
        std::vector<sim::TileXY> tiles;
        for (const auto& l : s->town.city->index.lists) {
            for (const auto& b : l.slots) {
                const sim::BuildingType* bt = b ? o.types->find(b->type) : nullptr;
                if (bt == nullptr) {
                    continue;
                }
                const bool inn = bt->cls == sim::RuntimeClass::Inn;
                const bool supply = bt->cls == sim::RuntimeClass::MarketStall ||
                                    bt->cls == sim::RuntimeClass::PublicService;
                if (inn || (op.demolish_supply && supply)) {
                    tiles.push_back(sim::TileXY{static_cast<std::int32_t>(b->x),
                                                static_cast<std::int32_t>(b->y)});
                }
            }
        }
        REQUIRE_FALSE(tiles.empty());
        const auto res =
            s->world.apply(sim::Command{s->town.city->owner, sim::DemolishCmd{s->town.ref, tiles}});
        REQUIRE_MESSAGE(res.ok(), std::string(sim::reject_name(res.status)));
    }
    if (op.drain_holz_below) {
        const int n =
            drain(*s, kHolzK, *op.drain_holz_below, {sim::TypeId{1321}, sim::TypeId{1314}});
        CHECK(n > 0);
    }
    if (op.drain_werkzeug_below) {
        // PLANTAGE 801: tools 2, wood 0, no input (25 of them take the 50 t of tools and no wood).
        const int n = drain(*s, kWerkzeugK, *op.drain_werkzeug_below, {sim::TypeId{801}});
        CHECK(n > 0);
    }
    return s;
}

constexpr std::uint64_t kStep = 50; // one advance(100) is 100 ms = 50 ticks
constexpr std::uint64_t kUnlockTick = 2500, kUpgradeTick = 57500, kEndTick = 70000;

/// The lines of the committed reference vectors (comments and blanks dropped).
std::vector<std::string> fixture_lines() {
    std::ifstream in(std::string(ANNO_FIXTURES_DIR) + "/sim/house_progression_vectors.txt");
    REQUIRE(in.is_open());
    std::vector<std::string> out;
    for (std::string line; std::getline(in, line);) {
        line = line.substr(0, line.find('#'));
        while (!line.empty() && (line.back() == ' ' || line.back() == '\r')) {
            line.pop_back();
        }
        if (!line.empty()) {
            out.push_back(line);
        }
    }
    return out;
}

std::map<std::string, std::string> kv_of(const std::string& text) {
    std::map<std::string, std::string> out;
    std::istringstream is(text);
    for (std::string tok; is >> tok;) {
        const auto eq = tok.find('=');
        if (eq != std::string::npos) {
            out[tok.substr(0, eq)] = tok.substr(eq + 1);
        }
    }
    return out;
}

std::vector<std::uint32_t> ints_of(const std::string& text) {
    std::vector<std::uint32_t> out;
    std::istringstream is(text);
    for (std::string tok; std::getline(is, tok, ',');) {
        out.push_back(static_cast<std::uint32_t>(std::stoul(tok)));
    }
    return out;
}

/// What the town and the house hold at one tick.
struct Snap {
    std::uint64_t tick = 0;
    sim::TypeId type;
    sim::BuildingId id;
    int x = 0, y = 0;
    std::int32_t residents_64 = 0;
    std::vector<std::uint32_t> house_counts;
    std::uint32_t stage_mask = 0, player_flags = 0;
    std::array<std::uint32_t, 5> store{};
    std::array<std::uint64_t, 5> spent{};
};

Snap snap(const Session& s) {
    Snap n;
    n.tick = s.world.clock().tick();
    n.house_counts = s.town.city->index.house_counts;
    n.stage_mask = s.town.city->stage_mask;
    n.player_flags = s.player().flags;
    for (std::size_t k = 0; k < n.store.size(); ++k) {
        n.store[k] = s.store(k);
        n.spent[k] = s.town.city->build_goods[k].spent;
    }
    if (const sim::Building* b = s.house_building()) {
        n.type = b->type;
        n.id = b->id;
        n.x = b->x;
        n.y = b->y;
        n.residents_64 = std::get<sim::House>(b->payload).residents_64;
    }
    return n;
}

std::vector<std::uint32_t> amounts_of(const Original& o, const sim::SimConfig& cfg,
                                      sim::TypeId type) {
    const sim::BuildingType* t = o.types->find(type);
    REQUIRE(t != nullptr);
    return sim::house_material_amounts(cfg, *t);
}

const sim::StageUnlockRecord* last_unlock(const Session& s) {
    const sim::StageUnlockRecord* last = nullptr;
    for (const auto& u : s.stats.population.unlocks) {
        if (u.city == s.town.ref) {
            last = &u;
        }
    }
    return last;
}

std::size_t unlocks_of_city(const Session& s) {
    std::size_t n = 0;
    for (const auto& u : s.stats.population.unlocks) {
        n += u.city == s.town.ref ? 1U : 0U;
    }
    return n;
}

const sim::Building* find_class(const Session& s, sim::RuntimeClass cls,
                                const sim::TypeRegistry& types) {
    for (const auto& l : s.town.city->index.lists) {
        for (const auto& b : l.slots) {
            const sim::BuildingType* t = b ? types.find(b->type) : nullptr;
            if (t != nullptr && t->cls == cls) {
                return &*b;
            }
        }
    }
    return nullptr;
}

constexpr sim::GoodsId kAlcohol = 29, kTavernRole = 76;

} // namespace

TEST_CASE(
    "progression K3: a house built by BuildCmd in the Tutorial town upgrades, nothing injected") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto sp = start(*o, Opts{});
    Session& S = *sp;
    const sim::SimConfig& cfg = S.world.config();
    // The scenario's own setup: the player's flags 0x7, no stage bit, eight settler houses, one
    // pioneer house built by the command above. The town holds the stalls, the chapel and the Inn
    // of section 22.
    CHECK(S.player().flags == 0x7);
    CHECK(S.town.city->stage_mask == 0);
    const Snap built = snap(S);
    CHECK(built.type == kPioneer);
    CHECK(built.residents_64 == 5 * 64);
    CHECK(built.house_counts[0] == 1);
    CHECK(built.house_counts[1] == 8);
    // the build's own debit: the registry's cost of 501, out of the store, onto the spent counters
    const auto cost_501 = amounts_of(*o, cfg, kPioneer);
    for (std::size_t k = 0; k < 5; ++k) {
        CHECK(S.pre_store[k] - built.store[k] == cost_501[k]);
        CHECK(built.spent[k] - S.pre_spent[k] == cost_501[k]);
    }
    CHECK(cost_501[kHolzK] == 768);
    CHECK(cost_501[kWerkzeugK] == 0);

    S.advance_to(kUpgradeTick - kStep);
    const std::int64_t before_new_counter =
        sim::residents_at_or_above(cfg, *o->types, *S.town.city, 1);
    const Snap before = snap(S);
    CHECK(S.transitions_of_house() == 0);
    CHECK(before.type == kPioneer);
    CHECK(before.stage_mask == 0xff); // unlocked at tick 2500 (below), long before the upgrade
    S.advance_to(kUpgradeTick);
    const Snap after = snap(S);

    // ---- the transition record of the deciding tick ----
    REQUIRE(S.transitions_of_house() == 1);
    const sim::HouseTransitionRecord* upgrade =
        S.transition(sim::HouseTransitionRecord::Kind::Upgrade);
    REQUIRE_MESSAGE(upgrade != nullptr, "no Upgrade transition recorded for the house");
    const sim::HouseTransitionRecord& r = *upgrade;
    CHECK(r.tick == kUpgradeTick);
    CHECK(r.from == kPioneer);
    const sim::TypeLadder ladder =
        sim::house_type_ladder(cfg, *o->types, *o->types->find(kPioneer));
    REQUIRE(ladder.next != nullptr);
    CHECK(ladder.next->id.v == 520); // id - id % 20 + 20
    // the variant draw: idx = r mod count, type = base + idx * step, one draw
    const sim::VariantFacts vf = sim::house_variant_facts(*ladder.next);
    REQUIRE(r.variant_drawn);
    CHECK(r.variant_draw == 21313);
    CHECK(r.variant_index == sim::house_variant_index(vf, r.variant_draw));
    CHECK(r.to.v == ladder.next->id.v + r.variant_index * vf.step);
    CHECK(r.to.v == 525);
    CHECK(vf.count > 0);
    // type, group, population, tile identity
    CHECK(after.type == r.to);
    CHECK(after.id == before.id); // the same building: id, list slot and origin tile
    CHECK(after.id == S.house);
    CHECK((after.x == before.x && after.y == before.y));
    CHECK(after.x == kX);
    CHECK(after.y == kY);
    CHECK(r.old_group == 0);
    CHECK(r.new_group == 1);
    CHECK(sim::house_group(cfg, o->types->find(after.type)->prodtype) ==
          std::optional<std::uint32_t>(1));
    CHECK(r.residents_before == before.residents_64);
    CHECK(r.residents_after == after.residents_64); // SetGroup keeps the residents
    CHECK(after.residents_64 == before.residents_64);
    const sim::BuildingType& new_type = *o->types->find(after.type);
    CHECK(new_type.width == o->types->find(kPioneer)->width); // the footprint is the same size
    CHECK(new_type.height == o->types->find(kPioneer)->height);
    for (std::uint32_t dy = 0; dy < new_type.height; ++dy) {
        for (std::uint32_t dx = 0; dx < new_type.width; ++dx) {
            const sim::Building* at =
                S.town.island->building_at(cfg.layout, S.town.city->number,
                                           kX + static_cast<int>(dx), kY + static_cast<int>(dy));
            REQUIRE(at != nullptr);
            CHECK(at->id == S.house); // every tile of the new footprint belongs to the house
        }
    }
    // city counts
    CHECK(before.house_counts[0] == 1);
    CHECK(before.house_counts[1] == 8);
    CHECK(after.house_counts[0] == 0);
    CHECK(after.house_counts[1] == 9);
    CHECK(r.houses_old_before == 1);
    CHECK(r.houses_old_after == 0);
    CHECK(r.houses_new_before == 8);
    CHECK(r.houses_new_after == 9);
    CHECK(r.counter_old_before == 128);
    CHECK(r.counter_old_after == 128);
    CHECK(r.counter_new_before ==
          120); // the eight settler houses (measured below at the step before)
    CHECK(r.counter_new_before == before_new_counter);
    CHECK(r.counter_new_after == sim::residents_at_or_above(cfg, *o->types, *S.town.city, 1));
    CHECK(r.counter_new_after == 128);
    // the exact material debit: the registry's cost of the NEXT base type, once, from the store and
    // onto spent
    const auto cost_520 = amounts_of(*o, cfg, ladder.next->id);
    CHECK(cost_520[kWerkzeugK] == 256);
    CHECK(cost_520[kHolzK] == 1024);
    for (std::size_t k = 0; k < 5; ++k) {
        CHECK(before.store[k] - after.store[k] == cost_520[k]);
        CHECK(after.spent[k] - before.spent[k] == cost_520[k]);
        CHECK(r.spent_by_good[k] == static_cast<std::int64_t>(cost_520[k]));
        CHECK(r.stock_by_good[k] == -static_cast<std::int64_t>(cost_520[k]));
    }
    // the stage mask and the player mask of the unlock pass (tick 2500, caused by the fresh house's
    // residents)
    CHECK(S.town.city->stage_mask == 0xff);
    CHECK(S.player().flags == 0xff);
    const sim::StageUnlockRecord* first = nullptr;
    for (const auto& u : S.stats.population.unlocks) {
        if (u.city == S.town.ref && first == nullptr) {
            first = &u;
        }
    }
    REQUIRE(first != nullptr);
    CHECK(first->tick == kUnlockTick);
    CHECK(first->city_mask_before == 0);
    CHECK(first->player_flags_before == 0x7);
    CHECK(first->player_flags_before != first->player_flags_after); // the player's mask CHANGED
    CHECK(first->player_flags_after == 0xff);
    CHECK(unlocks_of_city(S) == 8);
    CHECK(last_unlock(S)->city_mask_after == 0xff);
    // stage 3 (group 0, 125 residents) is the bit the fresh house adds: 120 + 5 residents
    bool stage3 = false;
    for (const auto& u : S.stats.population.unlocks) {
        if (u.city == S.town.ref && u.stage == 3) {
            stage3 = true;
            CHECK(u.group == 0);
            CHECK(u.count == 125);
            CHECK(u.residents_at_or_above == 125);
            CHECK(u.tick == kUnlockTick);
        }
    }
    CHECK(stage3);

    S.advance_to(kEndTick);
    CHECK(S.transitions_of_house() == 1); // upgraded once, nothing else happened to it
    CHECK(snap(S).type == r.to);
    CHECK(S.world.check_invariants() == "");
    CHECK(S.stats.residents.faults == 0);
}

TEST_CASE("progression K3: the deciding-tick values equal the independent reference vectors") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto sp = start(*o, Opts{});
    Session& S = *sp;
    S.advance_to(kUpgradeTick);
    const sim::HouseTransitionRecord* upgrade =
        S.transition(sim::HouseTransitionRecord::Kind::Upgrade);
    REQUIRE_MESSAGE(upgrade != nullptr, "no Upgrade transition recorded for the house");
    const sim::HouseTransitionRecord& r = *upgrade;
    REQUIRE(r.check_ran);
    CHECK(r.check_eligible);
    // the engine's own numbers at the deciding tick
    CHECK(r.check_e == 670);
    CHECK(join_ints(r.check_stock) == "1815,868,868,388,968,2156");
    CHECK(r.check_food == 112);
    CHECK(r.check_mean == 121);
    CHECK(r.check_infra == 120);
    CHECK(r.check_f == 1);
    CHECK(r.check_b == 2);
    CHECK(r.check_i == 2);
    // the same inputs, rebuilt from the registry, are the committed vector's inputs
    const std::vector<std::string> lines = fixture_lines();
    REQUIRE(lines.size() == 6); // two groups, three upcheck vectors and the variant draw
    const std::vector<std::string> mine = upcheck_inputs(*o, S.world.config(), r);
    CHECK(lines[0] == mine[0]);
    CHECK(lines[1] == mine[1]);
    const auto arrow = lines[2].find(" => ");
    REQUIRE(arrow != std::string::npos);
    CHECK(lines[2].substr(0, arrow) == mine[2]);
    // every vector (tools/house_transition_ref.py computed the expected parts): the engine's scores
    // agree
    const auto cur = o->types->population_view(0, S.world.config());
    const auto next = o->types->population_view(1, S.world.config());
    REQUIRE(cur.ok());
    REQUIRE(next.ok());
    std::size_t checked = 0;
    for (const std::string& line : lines) {
        if (line.rfind("upcheck ", 0) != 0) {
            continue;
        }
        const auto sep = line.find(" => ");
        REQUIRE(sep != std::string::npos);
        const auto in = kv_of(line.substr(0, sep));
        const auto want = kv_of(line.substr(sep + 4));
        const sim::UpgradeScores got = sim::upgrade_scores(
            cur.value(), next.value(), static_cast<std::uint32_t>(std::stoul(in.at("e"))),
            ints_of(in.at("stock")));
        CHECK(got.fault == sim::HouseFault::None);
        CHECK(std::to_string(got.eligible ? 1 : 0) == want.at("result"));
        CHECK(std::to_string(got.food) == want.at("food"));
        CHECK(std::to_string(got.mean) == want.at("mean"));
        CHECK(std::to_string(got.infra) == want.at("infra"));
        CHECK(std::to_string(got.f) == want.at("f"));
        CHECK(std::to_string(got.b) == want.at("b"));
        CHECK(std::to_string(got.i) == want.at("i"));
        ++checked;
    }
    // the variant draw of the upgrade: the registry's count and step, the engine's draw, the
    // vector's idx and type
    for (const std::string& line : lines) {
        if (line.rfind("variant ", 0) != 0) {
            continue;
        }
        const auto sep = line.find(" => ");
        REQUIRE(sep != std::string::npos);
        const auto in = kv_of(line.substr(0, sep));
        const auto want = kv_of(line.substr(sep + 4));
        const sim::VariantFacts vf = sim::house_variant_facts(*o->types->find(sim::TypeId{520}));
        CHECK(std::to_string(vf.count) == in.at("count"));
        CHECK(std::to_string(vf.step) == in.at("step"));
        CHECK(in.at("base") == "520");
        CHECK(std::to_string(r.variant_draw) == in.at("r"));
        CHECK(want.at("draw") == (r.variant_drawn ? "1" : "0"));
        CHECK(std::to_string(r.variant_index) == want.at("idx"));
        CHECK(std::to_string(r.to.v) == want.at("type"));
        ++checked;
    }
    CHECK(checked == 4);
}

TEST_CASE("progression K3 controls: no build, short materials, no Inn service and a removal") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    SUBCASE("no build: the stage bit does not change and nothing upgrades") {
        Opts op;
        op.build_house = false;
        auto sp = start(*o, op);
        Session& S = *sp;
        S.advance_to(kEndTick);
        CHECK(S.stats.population.transitions.empty());
        CHECK(S.town.city->stage_mask ==
              0xf7);                     // bit 3 (125 residents) never set: 120 residents only
        CHECK(S.player().flags == 0xf7); // 0x7 -> 0xf7, not 0xff
        CHECK(unlocks_of_city(S) == 7);
        for (const auto& u : S.stats.population.unlocks) {
            CHECK((u.city != S.town.ref || u.stage != 3));
        }
        CHECK(S.town.city->index.house_counts[1] == 8);
    }
    SUBCASE("HOLZ short: the check passes, the gate refuses, nothing is spent or drawn") {
        Opts op;
        op.drain_holz_below = 1024;
        auto sp = start(*o, op);
        Session& S = *sp;
        const Snap built = snap(S);
        CHECK(built.store[kHolzK] < 1024);
        CHECK(built.store[kWerkzeugK] >= 256); // only the wood is short
        S.advance_to(kEndTick);
        CHECK(S.transition(sim::HouseTransitionRecord::Kind::Upgrade) == nullptr);
        const auto* refused = S.transition(sim::HouseTransitionRecord::Kind::UpgradeRefused);
        REQUIRE(refused != nullptr);
        CHECK(refused->tick == kUpgradeTick);
        CHECK(refused->check_eligible); // the three scores pass, as in the upgrade run
        CHECK(refused->check_e == 670);
        CHECK(join_ints(refused->check_stock) == "1815,868,868,388,968,2156");
        for (const auto& t : S.stats.population.transitions) {
            if (t.building != S.house) {
                continue;
            }
            CHECK(t.kind == sim::HouseTransitionRecord::Kind::UpgradeRefused);
            CHECK_FALSE(t.variant_drawn);
            for (std::size_t k = 0; k < t.spent_by_good.size(); ++k) {
                CHECK(t.spent_by_good[k] == 0);
                CHECK(t.stock_by_good[k] == 0);
            }
            CHECK(t.from == kPioneer);
            CHECK(t.houses_old_after == 0); // refused records carry no counter change
        }
        const Snap end = snap(S);
        CHECK(end.type == kPioneer);
        CHECK(end.store[kHolzK] == built.store[kHolzK]);
        CHECK(end.spent == built.spent);
        CHECK(end.house_counts[0] == 1);
        CHECK(end.house_counts[1] == 8);
    }
    SUBCASE("WERKZEUG short: the same, with the wood available") {
        Opts op;
        op.drain_werkzeug_below = 256;
        auto sp = start(*o, op);
        Session& S = *sp;
        const Snap built = snap(S);
        CHECK(built.store[kWerkzeugK] < 256);
        CHECK(built.store[kHolzK] >= 1024); // only the tools are short
        S.advance_to(kEndTick);
        CHECK(S.transition(sim::HouseTransitionRecord::Kind::Upgrade) == nullptr);
        const auto* refused = S.transition(sim::HouseTransitionRecord::Kind::UpgradeRefused);
        REQUIRE(refused != nullptr);
        CHECK(refused->tick == kUpgradeTick);
        CHECK_FALSE(refused->variant_drawn);
        const Snap end = snap(S);
        CHECK(end.type == kPioneer);
        CHECK(end.store == built.store);
        CHECK(end.spent == built.spent);
    }
    SUBCASE("no Inn service: the tavern slot stays empty, infra is below 84, the house stays a "
            "pioneer house") {
        Opts op;
        op.demolish_inn = true;
        auto sp = start(*o, op);
        Session& S = *sp;
        const auto cur = o->types->population_view(0, S.world.config());
        REQUIRE(cur.ok());
        const std::uint32_t tavern_slot =
            cur.value().derived->lookup[o->types->population_roles().tavern_goods];
        std::uint32_t tavern_max = 0;
        while (S.world.clock().tick() < kEndTick) {
            S.advance_to(S.world.clock().tick() + 1000);
            tavern_max = std::max(
                tavern_max, std::get<sim::House>(S.house_building()->payload).stock[tavern_slot]);
        }
        CHECK(tavern_max == 0);
        CHECK(S.transitions_of_house() == 0); // not even a refusal: the check never passed
        const Snap end = snap(S);
        CHECK(end.type == kPioneer);
        const auto* h = std::get_if<sim::House>(&S.house_building()->payload);
        REQUIRE(h != nullptr);
        const auto next = o->types->population_view(1, S.world.config());
        REQUIRE(next.ok());
        const sim::UpgradeScores sc =
            sim::upgrade_scores(cur.value(), next.value(), h->e, h->stock);
        CHECK_FALSE(sc.eligible);
        CHECK(sc.infra == 0);
        CHECK(sc.infra < 84);
        CHECK(sc.i <= 0);
    }
    SUBCASE("decline: with the stands, the chapel and the Inn gone the house is removed, never "
            "upgraded") {
        Opts op;
        op.demolish_supply = true;
        auto sp = start(*o, op);
        Session& S = *sp;
        while (S.world.clock().tick() < 400000 && S.transitions_of_house() == 0) {
            S.advance_to(S.world.clock().tick() + 2500);
        }
        CHECK(S.transition(sim::HouseTransitionRecord::Kind::Upgrade) == nullptr);
        const auto* removed = S.transition(sim::HouseTransitionRecord::Kind::Remove);
        REQUIRE(removed != nullptr);
        MESSAGE("removal at tick " << removed->tick << " residents " << removed->residents_before
                                   << " old group " << removed->old_group << " houses "
                                   << removed->houses_old_before << "->"
                                   << removed->houses_old_after);
        CHECK(S.house_building() == nullptr);
        CHECK(removed->old_group == 0);
        CHECK(removed->houses_old_after + 1 == removed->houses_old_before);
        CHECK(S.world.check_invariants() == "");
    }
}

TEST_CASE("progression K3 ledger: alcohol store -> carrier -> Inn -> resident -> house, stalls -> "
          "house") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto sp = start(*o, Opts{});
    Session& S = *sp;
    const sim::Building* inn_b = find_class(S, sim::RuntimeClass::Inn, *o->types);
    REQUIRE(inn_b != nullptr);
    const sim::BuildingId inn_id = inn_b->id;
    const auto inn = [&]() -> const sim::Inn& {
        const sim::Building* b = nullptr;
        for (const auto& l : S.town.city->index.lists) {
            for (const auto& x : l.slots) {
                b = x && x->id == inn_id ? &*x : b;
            }
        }
        REQUIRE(b != nullptr);
        return std::get<sim::Inn>(b->payload);
    };
    const std::uint32_t alcohol_0 = S.town.city->stock.stock[kAlcohol];
    std::map<sim::GoodsId, std::uint32_t> stall_0;
    for (const sim::GoodsId g : {27U, 28U, 33U}) {
        stall_0[g] = S.town.city->stock.stock[g];
    }
    std::uint32_t loads = 0, unloads = 0, drawn = 0;
    std::uint32_t prev_store = alcohol_0, prev_out = inn().carriers_out;
    while (S.world.clock().tick() < kEndTick) {
        S.advance_to(S.world.clock().tick() + kStep);
        const std::uint32_t now_store = S.town.city->stock.stock[kAlcohol];
        if (now_store < prev_store) {
            ++loads;
            drawn += prev_store - now_store;
            CHECK(prev_store - now_store ==
                  768); // one carrier load: 3 t (the Inn's input per trip)
        }
        CHECK(now_store <= prev_store); // nothing else touches the alcohol store in this town
        prev_store = now_store;
        const std::uint32_t out = inn().carriers_out;
        if (out < prev_out) {
            ++unloads;
        }
        prev_out = out;
    }
    CHECK(loads >= 1);
    // in flight at the end: carriers' cargo and residents' cargo
    std::uint64_t carrier_cargo = 0;
    std::map<sim::GoodsId, std::uint64_t> walker_cargo;
    for (const auto& f : S.world.figures().list.slots) {
        if (!f) {
            continue;
        }
        if (f->job == sim::kJobWorkshopCarrier && !f->expiry_stamp.has_value() &&
            f->cargo_goods == kAlcohol) {
            carrier_cargo += f->cargo_88;
        }
        if (f->resident.has_value() && !f->expiry_stamp.has_value()) {
            for (const auto& c : f->resident->cargo) {
                if (c.goods != 0) {
                    walker_cargo[c.goods] += c.amount_88;
                }
            }
        }
    }
    const sim::ResidentStats& rs = S.stats.residents;
    const auto at = [](const std::map<sim::GoodsId, std::uint64_t>& m,
                       sim::GoodsId g) -> std::uint64_t {
        const auto it = m.find(g);
        return it == m.end() ? 0U : it->second;
    };
    // --- alcohol: every term of every identity is zero ---
    std::vector<std::pair<std::string, std::int64_t>> terms;
    const auto term = [&](const char* name, std::int64_t v) { terms.emplace_back(name, v); };
    term("store drop - loads*768",
         static_cast<std::int64_t>(alcohol_0 - prev_store) - std::int64_t{loads} * 768);
    term("store drop - drawn",
         static_cast<std::int64_t>(alcohol_0 - prev_store) - std::int64_t{drawn});
    term("drawn - (Inn stock + residents bought + carriers' cargo)",
         std::int64_t{drawn} - (std::int64_t{inn().tavern} +
                                static_cast<std::int64_t>(at(rs.bought_by_goods, kTavernRole)) +
                                static_cast<std::int64_t>(carrier_cargo)));
    term("loads - unloads - carriers out", std::int64_t{loads} - unloads - inn().carriers_out);
    term("billed(alcohol) - bought(tavern slot)",
         static_cast<std::int64_t>(at(rs.billed_by_goods, kAlcohol)) -
             static_cast<std::int64_t>(at(rs.bought_by_goods, kTavernRole)));
    term("merged billed(alcohol) - billed(alcohol)",
         static_cast<std::int64_t>(at(rs.merged_billed_by_goods, kAlcohol)) -
             static_cast<std::int64_t>(at(rs.billed_by_goods, kAlcohol)));
    term("bought - delivered - walkers' cargo - lost - dropped (tavern slot)",
         static_cast<std::int64_t>(at(rs.bought_by_goods, kTavernRole)) -
             static_cast<std::int64_t>(
                 at(rs.delivered_by_goods, kTavernRole) + walker_cargo[kTavernRole] +
                 at(rs.lost_by_goods, kTavernRole) + at(rs.merge_dropped_by_goods, kTavernRole)));
    term("stock raised - 16 * delivered (tavern slot)",
         static_cast<std::int64_t>(at(rs.stock_primary_by_goods, kTavernRole) +
                                   at(rs.stock_replay_by_goods, kTavernRole)) -
             static_cast<std::int64_t>(16 * at(rs.merged_delivered_by_goods, kTavernRole)));
    term("taken(tavern slot): a public building takes nothing from the store",
         static_cast<std::int64_t>(at(rs.taken_by_goods, kTavernRole)));
    // --- leather, salt, cloth: store -> stall -> house ---
    for (const sim::GoodsId g : {27U, 28U, 33U}) {
        const std::string n = "goods " + std::to_string(g) + ": ";
        const auto store_drop = static_cast<std::int64_t>(stall_0[g]) -
                                static_cast<std::int64_t>(S.town.city->stock.stock[g]);
        terms.emplace_back(n + "store drop - merged taken",
                           store_drop - static_cast<std::int64_t>(at(rs.merged_taken_by_goods, g)));
        terms.emplace_back(n + "taken - merged taken",
                           static_cast<std::int64_t>(at(rs.taken_by_goods, g)) -
                               static_cast<std::int64_t>(at(rs.merged_taken_by_goods, g)));
        terms.emplace_back(n + "billed - bought",
                           static_cast<std::int64_t>(at(rs.billed_by_goods, g)) -
                               static_cast<std::int64_t>(at(rs.bought_by_goods, g)));
        terms.emplace_back(n + "bought - taken",
                           static_cast<std::int64_t>(at(rs.bought_by_goods, g)) -
                               static_cast<std::int64_t>(at(rs.taken_by_goods, g)));
        terms.emplace_back(n + "bought - delivered - walkers' cargo - lost - dropped",
                           static_cast<std::int64_t>(at(rs.bought_by_goods, g)) -
                               static_cast<std::int64_t>(at(rs.delivered_by_goods, g) +
                                                         walker_cargo[g] + at(rs.lost_by_goods, g) +
                                                         at(rs.merge_dropped_by_goods, g)));
        terms.emplace_back(n + "stock raised - expected",
                           static_cast<std::int64_t>(at(rs.stock_primary_by_goods, g) +
                                                     at(rs.stock_replay_by_goods, g)) -
                               static_cast<std::int64_t>(at(rs.stock_expected_by_goods, g)));
        CHECK(at(rs.bought_by_goods, g) > 0); // the term is not zero because nothing happened
    }
    for (const auto& [name, v] : terms) {
        CHECK_MESSAGE(v == 0, name << " = " << v);
    }
    // the chain really ran: alcohol left the store, reached the Inn and a resident bought from it
    CHECK(loads == 1);
    CHECK(drawn == 768);
    CHECK(inn().tavern == 577);
    CHECK(at(rs.bought_by_goods, kTavernRole) == 191);
    CHECK(rs.faults == 0);
    CHECK(rs.lost_88 == 0);
    CHECK(S.world.check_invariants() == "");
    MESSAGE("loads " << loads << " unloads " << unloads << " drawn " << drawn << " Inn stock "
                     << inn().tavern << " bought76 " << at(rs.bought_by_goods, kTavernRole)
                     << " carrier cargo " << carrier_cargo);
}

TEST_CASE("progression K3 persistence: save and reload at the cuts of the real run continue "
          "identically") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto sp = start(*o, Opts{});
    Session& A = *sp;
    struct Pending {
        std::uint64_t at;
        std::uint64_t hash;
        std::string name;
        sim::TypeId type;
        std::uint32_t mask, flags;
    };
    std::vector<Pending> pending;
    const auto summary = [&](sim::World& w, std::string name, std::uint64_t at) {
        const Town t = find_town(w);
        const sim::Building* b = t.island->building_at(w.config().layout, t.city->number, kX, kY);
        REQUIRE(b != nullptr);
        pending.push_back({at, w.state_hash(), std::move(name), b->type, t.city->stage_mask,
                           w.players().find(t.city->owner)->flags});
    };
    const auto live_carriers = [&](const sim::World& w) {
        std::size_t n = 0;
        for (const auto& f : w.figures().list.slots) {
            n += f && f->job == sim::kJobWorkshopCarrier && !f->expiry_stamp.has_value() ? 1U : 0U;
        }
        return n;
    };
    const auto inn_of = [&](const sim::World& w) -> sim::Inn {
        const Town t = find_town(const_cast<sim::World&>(w));
        for (const auto& l : t.city->index.lists) {
            for (const auto& b : l.slots) {
                if (b && std::holds_alternative<sim::Inn>(b->payload)) {
                    return std::get<sim::Inn>(b->payload);
                }
            }
        }
        FAIL("no Inn");
        return sim::Inn{};
    };
    const auto walker_with = [&](const sim::World& w, sim::GoodsId g) {
        for (const auto& f : w.figures().list.slots) {
            if (f && f->resident.has_value()) {
                for (const auto& c : f->resident->cargo) {
                    if (c.goods == g && c.amount_88 > 0) {
                        return true;
                    }
                }
            }
        }
        return false;
    };
    struct Cut {
        std::uint64_t tick;
        std::uint64_t span;
        const char* name;
    };
    // The cut ticks are the first 50-tick steps at which the named condition held in the real run
    // (measured with this stepping; each cut asserts its condition on both worlds).
    const std::vector<Cut> cuts{{kUnlockTick - kStep, 600, "just before the unlock"},
                                {kUnlockTick, 600, "just after the unlock"},
                                {10000, 14000, "carrier on its way to the Inn"},
                                {25000, 2000, "Inn stock above zero"},
                                {38950, 2000, "resident carrying tavern goods"},
                                {kUpgradeTick - kStep, 600, "just before the upgrade"},
                                {kUpgradeTick, 600, "just after the upgrade"}};
    std::size_t next_cut = 0, compared = 0;
    while (A.world.clock().tick() < kEndTick) {
        const std::uint64_t now = A.world.clock().tick();
        if (next_cut < cuts.size() && cuts[next_cut].tick == now) {
            const Cut& cut = cuts[next_cut++];
            const std::string name = cut.name;
            if (name == "carrier on its way to the Inn") {
                CHECK(live_carriers(A.world) == 1);
                CHECK(inn_of(A.world).carriers_out == 1);
            } else if (name == "Inn stock above zero") {
                CHECK(inn_of(A.world).tavern > 0);
            } else if (name == "resident carrying tavern goods") {
                CHECK(walker_with(A.world, kTavernRole));
            } else if (name == "just before the upgrade") {
                CHECK(snap(A).type == kPioneer);
            } else if (name == "just after the upgrade") {
                CHECK(snap(A).type.v == 525);
            } else if (name == "just before the unlock") {
                CHECK(A.town.city->stage_mask == 0);
            } else if (name == "just after the unlock") {
                CHECK(A.town.city->stage_mask == 0xff);
            }
            auto loaded = sim::World::load(A.world.save(), o->types);
            REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
            sim::World C = std::move(loaded).value();
            CHECK(C.state_hash() == A.world.state_hash());
            CHECK(C.clock().tick() == now);
            // the reloaded world holds the same situation
            if (name == "carrier on its way to the Inn") {
                CHECK(live_carriers(C) == 1);
                CHECK(inn_of(C).carriers_out == 1);
            } else if (name == "Inn stock above zero") {
                CHECK(inn_of(C).tavern > 0);
            } else if (name == "resident carrying tavern goods") {
                CHECK(walker_with(C, kTavernRole));
            }
            while (C.clock().tick() < now + cut.span) {
                C.advance(100);
                REQUIRE_FALSE(C.faulted());
            }
            CHECK(C.check_invariants() == "");
            summary(C, name, C.clock().tick());
        }
        for (const Pending& p : pending) {
            if (p.at == now) {
                CHECK_MESSAGE(A.world.state_hash() == p.hash, p.name << " at tick " << now);
                const Snap here = snap(A);
                CHECK(here.type == p.type);
                CHECK(here.stage_mask == p.mask);
                CHECK(here.player_flags == p.flags);
                ++compared;
            }
        }
        A.advance_to(now + kStep);
    }
    CHECK(next_cut == cuts.size());
    CHECK(compared == cuts.size());
    CHECK(snap(A).type.v == 525);
    CHECK(A.world.check_invariants() == "");
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
        (fs::path(dir) / name).string(),
        std::string_view(reinterpret_cast<const char*>(bytes.value().data()),
                         bytes.value().size()));
    REQUIRE_MESSAGE(e.empty(), e);
}

std::string hex64(std::uint64_t v) {
    std::ostringstream o;
    o << std::hex << v;
    return o.str();
}

/// The checkpoint ticks of the recorded run and the state hash the run reached at each.
constexpr std::array<std::uint64_t, 6> kCheckpointTicks{2450, 2500, 57450, 57500, 57550, 70000};

} // namespace

TEST_CASE("progression K3 replay: the recorded run replays from bytes, and a changed command "
          "diverges at once") {
    const auto o = load_original();
    if (!o) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World w = tutorial_world(*o, true);
    auto started = sim::Recorder::start(w);
    REQUIRE_MESSAGE(started.ok(), (started.ok() ? std::string() : started.error()));
    sim::Recorder rec = std::move(started).value();
    const Town t = find_town(w);
    const auto built = rec.submit(sim::Command{
        t.city->owner, sim::BuildCmd{t.ref, {sim::PlacementWire{kPioneer, kX, kY, 0, 0, 0}}}});
    REQUIRE(built.ok());
    std::size_t next_cp = 0;
    while (w.clock().tick() < kEndTick) {
        rec.advance(100);
        if (next_cp < kCheckpointTicks.size() && w.clock().tick() == kCheckpointTicks[next_cp]) {
            rec.checkpoint();
            ++next_cp;
        }
    }
    REQUIRE(next_cp == kCheckpointTicks.size());
    const sim::ReplayLog& log = rec.log();
    std::vector<std::size_t> cp_entries;
    std::vector<std::uint64_t> cp_hashes;
    for (std::size_t i = 0; i < log.entries.size(); ++i) {
        if (log.entries[i].kind == sim::ReplayEntry::Kind::Checkpoint) {
            cp_entries.push_back(i);
            cp_hashes.push_back(log.entries[i].hash);
            CHECK(log.entries[i].tick == kCheckpointTicks[cp_hashes.size() - 1]);
        }
    }
    REQUIRE(cp_hashes.size() == kCheckpointTicks.size());
    for (std::size_t i = 0; i < cp_hashes.size(); ++i) {
        MESSAGE("checkpoint tick " << kCheckpointTicks[i] << " entry " << cp_entries[i] << " hash "
                                   << hex64(cp_hashes[i]));
    }
    // the pinned checkpoints (stream 129, PRNG timeline 5; at S2's first 129 they were
    // bedae9dd2e1a44cd, 1de337941cadec47, 28cf97e36e3b0e21, e85b6fb7cce9be38, 9c76d456ea5de184,
    // c52f3a9fe3df8357; at 128 they were 65d13922f5a15383, 46ea29b9ad8169b7, 666f938018336639,
    // 4b3943d7569830cf, f171056cdedaa7f2, 39644392a84ce22b): re-pinned after a legitimate behaviour
    // change
    constexpr std::array<std::uint64_t, 6> kPinned{0xdeea23714d123ad2ULL, 0x303b68783faf666dULL,
                                                   0x6e74c4256ccdc5afULL, 0x8487410926740e51ULL,
                                                   0x810d0895193d5c7fULL, 0xc69bbf8f30bdd9bfULL};
    for (std::size_t i = 0; i < kPinned.size(); ++i) {
        CHECK_MESSAGE(cp_hashes[i] == kPinned[i],
                      "checkpoint tick " << kCheckpointTicks[i] << " is " << hex64(cp_hashes[i]));
    }
    CHECK(log.rules_digest == w.rules().digest());
    CHECK(std::find(log.prng_timeline_missing.begin(), log.prng_timeline_missing.end(),
                    "HouseVariant") == log.prng_timeline_missing.end());
    export_log("progression_k3.log", log);

    // 1. through bytes, in a fresh world: every checkpoint reproduced, the house upgraded
    const auto bytes = sim::write_log(log);
    REQUIRE(bytes.ok());
    const auto read = sim::read_log(bytes.value());
    REQUIRE(read.ok());
    CHECK(read.value() == log);
    {
        auto fresh = sim::World::load(read.value().initial_save, o->types, {});
        REQUIRE_MESSAGE(fresh.ok(), (fresh.ok() ? std::string() : fresh.error()));
        auto outcome = sim::replay(fresh.value(), read.value());
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        CHECK(outcome.value().checkpoints_total == 6);
        CHECK(outcome.value().checkpoints_matched == 6);
        CHECK(outcome.value().reproduced());
        CHECK(outcome.value().final_tick == kEndTick);
        CHECK(fresh.value().state_hash() == w.state_hash());
        const Town ft = find_town(fresh.value());
        const sim::Building* hb =
            ft.island->building_at(fresh.value().config().layout, ft.city->number, kX, kY);
        REQUIRE(hb != nullptr);
        CHECK(hb->type.v == 525);
        CHECK(ft.city->stage_mask == 0xff);
    }
    // 2. the relevant command changed: a stand instead of the house at the same site. The first
    // checkpoint after the command already differs (the house, its residents and its stock are not
    // there), and the hash tree names the owner's city, not another island.
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
        CHECK(b->placements[0].type == kPioneer);
        b->placements[0].type = sim::TypeId{1314};
        auto fresh = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE(fresh.ok());
        auto outcome = sim::replay(fresh.value(), changed);
        REQUIRE_MESSAGE(outcome.ok(), (outcome.ok() ? std::string() : outcome.error()));
        REQUIRE_FALSE(outcome.value().reproduced());
        CHECK(*outcome.value().diverged_entry == cp_entries[0]);
        CHECK(outcome.value().expected_hash == cp_hashes[0]);
        CHECK(outcome.value().actual_hash != cp_hashes[0]);
        CHECK(outcome.value().checkpoints_matched == 0);
        const auto init_a = sim::World::load(read.value().initial_save, o->types, {});
        const auto init_b = sim::World::load(changed.initial_save, o->types, {});
        REQUIRE((init_a.ok() && init_b.ok()));
        const std::string path =
            sim::diff_trees(sim::tree_at(init_a.value(), read.value(), cp_entries[0], 8),
                            sim::tree_at(init_b.value(), changed, cp_entries[0], 8));
        MESSAGE("first differing scope: " << path);
        // the stand costs less than the house: the owner's player scope (money) differs first, and
        // the building at the site is the stand
        CHECK(path == "world/players/player[2]");
        const Town ft = find_town(fresh.value());
        const sim::Building* site =
            ft.island->building_at(fresh.value().config().layout, ft.city->number, kX, kY);
        REQUIRE(site != nullptr);
        CHECK(site->type.v == 1314);
        CHECK(ft.city->index.house_counts[0] == 0);
        export_log("progression_k3_changed_command.log", changed);
    }
    // 3. one recorded checkpoint hash changed: the replay itself reproduces the world up to it and
    // reports the divergence at exactly that entry (the upgrade checkpoints are 57500 and 57550)
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
        export_log("progression_k3_changed_checkpoint.log", changed);
    }
}
