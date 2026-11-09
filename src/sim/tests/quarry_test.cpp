// MAP-EXTRACT-2 part Q3: the quarry, the stonemason's job 0x2c and the brick proof
// (docs/sim/quarry-bricks.md). Tests on the iron pilot (endless02 island 1, owner 8).
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/command.hpp>
#include <anno/sim/mason.hpp>
#include <anno/sim/placement.hpp>
#include <anno/sim/production.hpp>
#include <anno/sim/terrain_view.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "quarry_fixture.hpp"
#include "test_must.hpp"
#include "world_access.hpp"

namespace {

namespace sim = anno::sim;
using namespace quarry_fix;

// ---- Q-1: placement through the quarry's own HIGHMAP table -------------------------------

/// An island holding exactly a (ww + 1) x (hh + 1) vertex window at its origin, all vertices
/// `base`, one vertex (vx, vz) raised to `peak` (as salt_placement_test's island_for).
sim::Island flat_island(std::uint32_t ww, std::uint32_t hh, std::uint8_t base, std::int32_t vx,
                        std::int32_t vz, std::uint8_t peak) {
    sim::Island is;
    REQUIRE(is.tiles.reset(0, 0, ww + 1, hh + 1, 8));
    std::vector<std::uint8_t> height(std::size_t{ww + 1} * (hh + 1), base);
    if (vx >= 0) {
        height[static_cast<std::size_t>(vz) * (ww + 1) + static_cast<std::size_t>(vx)] = peak;
    }
    is.terrain = sim::make_terrain(std::move(height), {}, {});
    return is;
}

TEST_CASE("quarry placement: the two HIGHMAP tables differ in exactly the two bytes of the doc") {
    auto p = make(false, false, 0);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::TypeRegistry& types = p->w.types();
    const sim::BuildingType* mine = types.find(sim::TypeId{1205});
    REQUIRE(mine != nullptr);
    for (const std::uint32_t id : {1201U, 1208U, 1218U, 1220U}) {
        const sim::BuildingType* q = types.find(sim::TypeId{id});
        REQUIRE(q != nullptr);
        REQUIRE(q->highmap.size() == mine->highmap.size());
        std::vector<std::size_t> diff;
        for (std::size_t i = 0; i < q->highmap.size(); ++i)
            if (q->highmap[i] != mine->highmap[i])
                diff.push_back(i);
        // indices 20 and 21 (row 3, columns 2 and 3): 0x20 in the mine, 0xff in the quarry
        REQUIRE(diff.size() == 2);
        CHECK(diff[0] == 20);
        CHECK(diff[1] == 21);
        CHECK(mine->highmap[20] == 0x20);
        CHECK(mine->highmap[21] == 0x20);
        CHECK(q->highmap[20] == 0xff);
        CHECK(q->highmap[21] == 0xff);
    }
}

TEST_CASE("quarry placement: a site only the mine refuses, per rotation, and where both agree") {
    auto p = make(false, false, 0);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::TypeRegistry& types = p->w.types();
    const sim::BuildingType* mine = types.find(sim::TypeId{1205});
    const sim::BuildingType* quarry = types.find(sim::TypeId{1201});
    REQUIRE(mine != nullptr);
    REQUIRE(quarry != nullptr);
    REQUIRE(mine->width == quarry->width);
    REQUIRE(mine->height == quarry->height);
    const sim::PlacementParams params;
    // docs/sim/quarry-bricks.md F5 (reference run, mine False / quarry True): the one vertex whose
    // raise to lo + 0x80 only the quarry accepts
    const std::map<int, std::pair<int, int>> expected{
        {0, {2, 1}}, {1, {1, 2}}, {2, {1, 3}}, {3, {3, 1}}};
    for (std::uint8_t rot = 0; rot < 4; ++rot) {
        const auto [fw, fh] = sim::footprint(*mine, rot);
        std::vector<std::pair<int, int>> only_quarry;
        for (std::int32_t vz = 0; vz <= static_cast<std::int32_t>(fh); ++vz) {
            for (std::int32_t vx = 0; vx <= static_cast<std::int32_t>(fw); ++vx) {
                const sim::Island is = flat_island(fw, fh, 0x20, vx, vz, 0xa0);
                sim::PlacementRecord rec;
                rec.rotation = rot;
                rec.type = mine->id;
                const auto m = sim::placement_rule(is, types, *mine, rec, params);
                rec.type = quarry->id;
                const auto q = sim::placement_rule(is, types, *quarry, rec, params);
                // the quarry never refuses where the mine accepts (a strict superset)
                if (m.reject == sim::Reject::None) {
                    CHECK(q.reject == sim::Reject::None);
                }
                if (m.reject == sim::Reject::TerrainRefused && q.reject == sim::Reject::None) {
                    only_quarry.emplace_back(vx, vz);
                }
                // and every other answer is the same typed refusal or the same acceptance
                if (!(m.reject == sim::Reject::TerrainRefused && q.reject == sim::Reject::None)) {
                    CHECK(m.reject == q.reject);
                }
            }
        }
        INFO("rotation " << int(rot));
        REQUIRE(only_quarry.size() == 1);
        CHECK(only_quarry[0] == expected.at(rot));
        // the legal control: the flat plateau is accepted by both
        const sim::Island flat = flat_island(fw, fh, 0x20, -1, 0, 0);
        sim::PlacementRecord rec;
        rec.rotation = rot;
        rec.type = mine->id;
        CHECK(sim::placement_rule(flat, types, *mine, rec, params).reject == sim::Reject::None);
        rec.type = quarry->id;
        CHECK(sim::placement_rule(flat, types, *quarry, rec, params).reject == sim::Reject::None);
        // the data switch: with quarries off the quarry is typed Unsupported again
        sim::PlacementParams off;
        off.quarries = false;
        CHECK(sim::placement_rule(flat, types, *quarry, rec, off).reject ==
              sim::Reject::Unsupported);
    }
}

TEST_CASE("quarry placement: a BuildCmd creates a quarry with no claims and no deposit") {
    auto p = make(true, false, 1000);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = p->w;
    const std::int64_t gold0 = gold(w);
    for (const Site s : {Site{1201, 362, 355}, Site{1208, 361, 357}}) {
        sim::World c = w;
        const sim::ApplyResult r = c.apply(build_cmd(s.type, s.x, s.y, s.rot));
        REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
        const sim::Building* b = c.find_building(r.records.at(0).building);
        REQUIRE(b != nullptr);
        const auto* q = std::get_if<sim::Quarry>(&b->payload);
        REQUIRE(q != nullptr);
        CHECK(q->claims.size() == (s.type == 1201 ? 2U : 1U)); // one flag per entrance
        CHECK(std::count(q->claims.begin(), q->claims.end(), true) == 0);
        CHECK(gold(c) == gold0 - 300);
        CHECK(c.check_invariants().empty());
    }
    // refused like any building: occupied, out of the island (legal control above)
    sim::World c = w;
    REQUIRE(c.apply(build_cmd(1201, 362, 355)).ok());
    CHECK(c.apply(build_cmd(1201, 362, 355)).status == sim::Reject::Occupied);
    CHECK(c.apply(build_cmd(1201, -5, 358)).status != sim::Reject::None);
    // the marble quarry and mason sit behind stage 13 (3C): denied on the pilot, allowed with the
    // bit
    sim::World m = w;
    CHECK(m.apply(build_cmd(1218, 361, 357)).status == sim::Reject::ConstructionDenied);
    test_must::must(sim::WorldTestAccess::players(m).find(kPlayer))->flags |= (1U << 13);
    CHECK(m.apply(build_cmd(1218, 361, 357)).status == sim::Reject::None);
}

// ---- the pure rules: the need gate, the candidate score, the farm vectors ---------------------

TEST_CASE("mason rules: the need gate and the candidate score (V3, V5, review P2)") {
    // V5: 804 (need 21): raw 629 -> 0x80, 630 -> 0x60, 1259 -> 0x60, 1260 -> none
    CHECK(sim::mason_priority(0, 21) == 0x80);
    CHECK(sim::mason_priority(629, 21) == 0x80);
    CHECK(sim::mason_priority(630, 21) == 0x60);
    CHECK(sim::mason_priority(1259, 21) == 0x60);
    CHECK(!sim::mason_priority(1260, 21).has_value());
    // 803 (need 22): 659 / 660 / 1319 / 1320
    CHECK(sim::mason_priority(659, 22) == 0x80);
    CHECK(sim::mason_priority(660, 22) == 0x60);
    CHECK(sim::mason_priority(1319, 22) == 0x60);
    CHECK(!sim::mason_priority(1320, 22).has_value());
    CHECK(!sim::mason_priority(10, 0).has_value()); // a need of 0 traps in the original
    // the score divides by the CANDIDATE's capacity: stone quarries 1,792 -> 155 / 123, marble
    // quarries 1,280 -> 166 / 134 (the home's capacity is 1,280 for both masons: the old reading)
    CHECK(sim::mason_candidate_score(0x80, 256, 1792) == 155);
    CHECK(sim::mason_candidate_score(0x60, 256, 1792) == 123);
    CHECK(sim::mason_candidate_score(0x80, 256, 1280) == 166);
    CHECK(sim::mason_candidate_score(0x60, 256, 1280) == 134);
    CHECK(!sim::mason_candidate_score(0x80, 256, 0).has_value());
}

TEST_CASE("mason rules: the registry facts the vectors use (types, figure 400, capacities)") {
    auto p = make(false, false, 0);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::TypeRegistry& types = p->w.types();
    const sim::BuildingType* q1201 = types.find(sim::TypeId{1201});
    const sim::BuildingType* q1218 = types.find(sim::TypeId{1218});
    const sim::BuildingType* m804 = types.find(sim::TypeId{804});
    const sim::BuildingType* m803 = types.find(sim::TypeId{803});
    REQUIRE((q1201 && q1218 && m804 && m803));
    CHECK(q1201->entrances.size() == 2);
    CHECK(types.find(sim::TypeId{1208})->entrances.size() == 1);
    CHECK(q1218->entrances.size() == 1);
    CHECK(q1201->prod.capacity.at(0) == 1792);
    CHECK(q1218->prod.capacity.at(0) == 1280);
    CHECK(m804->action_radius == 12);
    CHECK(m804->prod.input_per_step.at(0) == 21);
    CHECK(m803->prod.input_per_step.at(0) == 22);
    CHECK(sim::mason_candidate_score(0x80, 256, q1201->prod.capacity.at(0)) == 155);
    CHECK(sim::mason_candidate_score(0x80, 256, q1218->prod.capacity.at(0)) == 166);
    const sim::FigureType* ft = types.find_figure_type(400);
    REQUIRE(ft != nullptr);
    CHECK(ft->route == 44);
    CHECK(sim::farm_figure_route(ft->route) == sim::FarmFigureRoute::Mason);
    CHECK(ft->load_ticks == 5000); // 10.0 x 500
}

/// Runs the farm step of `type` from `raw` until it stops producing; returns {made, raw left,
/// full steps}. D-derived vector (NOT_RUN in the original).
struct FarmRun {
    std::uint64_t made = 0, left = 0, steps = 0;
};
FarmRun run_farm(const sim::BuildingType& type, std::uint32_t raw) {
    sim::Farm f;
    f.raw = raw;
    FarmRun r;
    std::uint32_t t = 0;
    for (int i = 0; i < 40; ++i) {
        t += sim::kStepTicks;
        const std::uint32_t out0 = f.out;
        const sim::StepResult s = sim::farm_step(f, type.prod, t);
        if (f.out == out0) {
            // raw missing (ratio below 64): the worker is dispatched again
            CHECK(s.send_figure);
            break;
        }
        ++r.steps;
        r.made += f.out - out0;
    }
    r.left = f.raw;
    return r;
}

TEST_CASE(
    "mason rules: the brick and marble vectors V2 and V3 (D-derived, NOT_RUN in the original)") {
    auto p = make(false, false, 0);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::TypeRegistry& types = p->w.types();
    // 804 with 256 raw per trip: 12 full steps make 516 raw ZIEGEL and leave 4 raw
    const FarmRun a = run_farm(*types.find(sim::TypeId{804}), 256);
    CHECK(a.made == 516);
    CHECK(a.left == 4);
    CHECK(a.steps == 12);
    // the next trip starts from 4 + 256 = 260 and makes 12 steps again, leaving 8
    const FarmRun b = run_farm(*types.find(sim::TypeId{804}), 260);
    CHECK(b.made == 516);
    CHECK(b.left == 8);
    // 803: 11 full steps plus a partial make 244 raw MAMOR from 256 and leave 1
    const FarmRun c = run_farm(*types.find(sim::TypeId{803}), 256);
    CHECK(c.made == 244);
    CHECK(c.steps == 12);
    CHECK(c.left == 1);
}

// ---- the job: Q-2 to Q-6 -----------------------------------------------------------------------

std::uint64_t lifted_by(const Fix& f, sim::BuildingId q) {
    const auto it = f.stats.forestry.quarry_lifted_88.find(q);
    return it == f.stats.forestry.quarry_lifted_88.end() ? 0U : it->second;
}

TEST_CASE("quarry job Q-2: a mason without a quarry in reach stays idle, with one it works") {
    // the imported quarry (352,371) is out of this mason's radius; the isolated world has none
    auto idle = start({}, {kMasonA});
    if (!idle) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step(*idle, 1500);
    const Terms t = terms_of(*idle);
    CHECK(t.lifted == 0);
    CHECK(t.delivered == 0);
    CHECK(t.made == 0);
    // the worker is sent again and again (the farm dispatches while starved) and comes home empty
    const sim::BuildingId m = idle->masons[0];
    CHECK(idle->stats.forestry.mason_searches[m] > 0);
    CHECK(idle->stats.forestry.mason_searches_failed[m] == idle->stats.forestry.mason_searches[m]);
    require_closed(t, "idle");
    CHECK(idle->pilot.w.check_invariants().empty());
    // the legal control: the same mason with a quarry makes bricks
    auto work = start({kQuarryA}, {kMasonA});
    REQUIRE(work);
    step(*work, 1500);
    const Terms w = terms_of(*work);
    CHECK(w.lifted > 0);
    CHECK(w.made > 0);
    require_closed(w, "work");
}

TEST_CASE("quarry job Q-3: a trip takes 5,000 ticks at the quarry and adds 256 raw once") {
    auto f = start({kQuarryA}, {kMasonA});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    // follow the first worker: the advances spent waiting, and the deliveries it makes
    int waiting = 0, trips = 0;
    std::uint64_t last_delivered = 0;
    bool seen_wait = false;
    for (int i = 0; i < 3000 && trips < 3; ++i) {
        step(*f, 1);
        const auto ws = workers(*f);
        bool waits = false;
        for (const sim::Figure* fig : ws)
            waits = waits || fig->harvest->step == sim::HarvestStep::Cut;
        if (waits) {
            ++waiting;
            seen_wait = true;
            // while waiting the worker carries nothing: the cargo is added at the END of the wait
            for (const sim::Figure* fig : ws)
                if (fig->harvest->step == sim::HarvestStep::Cut) {
                    CHECK(fig->cargo_88 == 0);
                    CHECK(fig->harvest->wait <= 5000);
                    CHECK(fig->harvest->wait > 0);
                }
        } else if (seen_wait) {
            // the wait just ended: 5,000 ticks are 100 advances of 50 ticks (one advance of
            // granularity at either end)
            CHECK(waiting >= 99);
            CHECK(waiting <= 101);
            waiting = 0;
            seen_wait = false;
        }
        const std::uint64_t d = terms_of(*f).delivered;
        if (d != last_delivered) {
            CHECK(d - last_delivered == 256); // one ton per trip, once
            last_delivered = d;
            ++trips;
        }
    }
    CHECK(trips == 3);
    const Terms t = terms_of(*f);
    CHECK(t.lifted == t.delivered + t.hands + t.lost);
    require_closed(t, "trips");
    // the claim of the single entrance pair: never more flags than workers
    CHECK(set_claims(w, f->quarries[0]) <= claimants(*f));
}

TEST_CASE(
    "quarry job Q-3b: the quarry nearest by walk wins and an unreachable-by-distance one waits") {
    const Site near_q = kQuarryA;
    auto f = start({near_q}, {kMasonA});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    // a second quarry within the mason's radius (12) but farther by walk (measured: it alone serves
    // the mason, 512 raw in 1,200 advances)
    const Site far_q{1201, 354, 358};
    const sim::ApplyResult fr = f->pilot.w.apply(build_cmd(far_q.type, far_q.x, far_q.y));
    REQUIRE(fr.ok());
    f->quarries.push_back(fr.records.at(0).building);
    step(*f, 1500);
    CHECK(lifted_by(*f, f->quarries[0]) > 0);
    CHECK(lifted_by(*f, f->quarries[1]) == 0); // the first goal popped is the path-nearest entrance
    require_closed(terms_of(*f), "two quarries");
    // control: the nearer quarry demolished, the farther one now serves
    sim::DemolishCmd dem;
    dem.city = kCity;
    dem.tiles = {{near_q.x, near_q.y}};
    REQUIRE(f->pilot.w.apply(sim::Command{kPlayer, dem}).ok());
    step(*f, 1500);
    CHECK(lifted_by(*f, f->quarries[1]) > 0);
    require_closed(terms_of(*f), "after the nearer quarry went");
}

TEST_CASE("quarry job Q-4: a mason demolished on its way loses the cargo exactly once") {
    for (const sim::HarvestStep at :
         {sim::HarvestStep::ToTree, sim::HarvestStep::Cut, sim::HarvestStep::ToHome}) {
        auto f = start({kQuarryA}, {kMasonA});
        if (!f) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        sim::World& w = f->pilot.w;
        // wait for the first worker to reach `at`; for ToHome it must hold the ton
        const int k = step_until(*f, 4000, [&] {
            for (const sim::Figure* fig : workers(*f))
                if (fig->harvest->step == at &&
                    (at != sim::HarvestStep::ToHome || fig->cargo_88 > 0))
                    return true;
            return false;
        });
        REQUIRE_MESSAGE(k > 0, "no worker reached step " << int(at));
        std::uint64_t in_hands = 0;
        for (const sim::Figure* fig : workers(*f))
            in_hands += fig->cargo_88;
        const sim::Building* mb = building(w, f->masons[0]);
        REQUIRE(mb != nullptr);
        const auto* farm = std::get_if<sim::Farm>(&mb->payload);
        REQUIRE(farm != nullptr);
        f->retired.raw += farm->raw;
        f->retired.out += farm->out;
        sim::DemolishCmd dem;
        dem.city = kCity;
        dem.tiles = {{mb->x, mb->y}};
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
        CHECK(w.check_invariants().empty());
        step(*f, 400);
        INFO("step " << int(at));
        // whatever was in the hands is lost with the figure, exactly once, never delivered
        const Terms t = terms_of(*f);
        CHECK(t.lost == in_hands);
        CHECK(t.hands == 0);
        require_closed(t, "mason demolished");
        // the claim of a worker aborted in step A or B is cleared (D 0x10055480)
        CHECK(set_claims(w, f->quarries[0]) == 0);
        CHECK(workers(*f).empty());
        CHECK(w.check_invariants().empty());
    }
}

TEST_CASE("quarry job Q-4b: a quarry demolished under a walking worker adds no cargo (V4)") {
    auto f = start({kQuarryA}, {kMasonA});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    const int k =
        step_until(*f, 4000, [&] { return workers_in(*f, sim::HarvestStep::ToTree) > 0; });
    REQUIRE(k > 0);
    sim::DemolishCmd dem;
    dem.city = kCity;
    dem.tiles = {{kQuarryA.x, kQuarryA.y}};
    REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    CHECK(w.check_invariants().empty());
    step(*f, 300); // the walk ends, the wait ends: nothing stands at the tile any more
    const Terms t = terms_of(*f);
    CHECK(t.lifted == 0);
    CHECK(t.delivered == 0); // the job went home with an empty hand
    require_closed(t, "quarry gone");
    CHECK(w.check_invariants().empty());
    // the legal control: with the quarry rebuilt the mason works again
    REQUIRE(w.apply(build_cmd(kQuarryA.type, kQuarryA.x, kQuarryA.y)).ok());
    f->quarries.push_back(test_must::must(find_type(w, 1201))->id);
    step(*f, 2000);
    CHECK(terms_of(*f).lifted > 0);
}

TEST_CASE("quarry brick proof Q-5 Q-8: stone becomes bricks in the store, spent by a BuildCmd") {
    auto f = start({kQuarryA}, {kMasonA});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    // labelled setup: no bricks in the store, so that only what the mason makes can pay a BuildCmd
    sim::City& city = *test_must::must(sim::WorldTestAccess::islands(w)[1]->city(kCity.city));
    city.stock.stock[kZiegel] = 0;
    f->bricks0 = 0;
    // a 1 t brick building: refused with an empty store
    // (type 1013, a workshop: gold 300, WERKZEUG 5 t, HOLZ 6 t, ZIEGEL 1 t; stage 12, opened by the
    // labelled setup of the stage bit so that the only missing good is the bricks)
    constexpr std::uint32_t kBrickBuilding = 1013;
    test_must::must(sim::WorldTestAccess::players(w).find(kPlayer))->flags |= (1U << 12);
    std::optional<std::pair<std::int32_t, std::int32_t>> site;
    for (std::int32_t y = 338; y <= 366 && !site; ++y)
        for (std::int32_t x = 356; x <= 388 && !site; ++x) {
            // the refusal reason must be the bricks, not a bad tile: a rich copy accepts it
            sim::World rich = w;
            test_must::must(sim::WorldTestAccess::islands(rich)[1]->city(kCity.city))
                ->stock.stock[kZiegel] = 10000;
            if (rich.apply(build_cmd(kBrickBuilding, x, y)).status == sim::Reject::None)
                site = {x, y};
        }
    REQUIRE(site.has_value());
    CHECK(w.apply(build_cmd(kBrickBuilding, site->first, site->second)).status ==
          sim::Reject::CannotAfford);
    // run until the store holds a ton
    const int k = step_until(*f, 4000, [&] { return city_of(w).stock.stock[kZiegel] >= 256; });
    REQUIRE_MESSAGE(k > 0, "no bricks reached the store");
    const Terms mid = terms_of(*f);
    CHECK(mid.made > 0);
    CHECK(mid.unloaded > 0);
    require_closed(mid, "bricks arrive");
    // spend: the BuildCmd takes exactly 256 raw of bricks and the 300 gold
    const std::uint32_t before = city_of(w).stock.stock[kZiegel];
    const std::int64_t gold0 = gold(w);
    const sim::ApplyResult r = w.apply(build_cmd(kBrickBuilding, site->first, site->second));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    CHECK(city_of(w).stock.stock[kZiegel] == before - 256);
    CHECK(gold(w) == gold0 - 300);
    f->spent_bricks_88 += 256;
    step(*f, 1500);
    const Terms t = terms_of(*f);
    say("brick proof", t);
    require_closed(t, "brick proof");
    // the store change is the unloaded bricks minus the spend minus the houses' drain (reported)
    CHECK(t.unloaded >= t.made - t.out_now - t.brick_hands - t.brick_lost);
    CHECK(t.store > 0);
    // the baseline kept apart: the same world without a quarry never raises the store
    auto base = start({}, {});
    REQUIRE(base);
    test_must::must(sim::WorldTestAccess::islands(base->pilot.w)[1]->city(kCity.city))
        ->stock.stock[kZiegel] = 0;
    base->bricks0 = 0;
    step(*base, 1500 + k);
    CHECK(city_of(base->pilot.w).stock.stock[kZiegel] == 0);
}

TEST_CASE("quarry controls Q-6: no quarry, no mason, no market cover and a full store") {
    // no quarry: Q-2. No mason: a quarry alone changes nothing
    auto noMason = start({kQuarryA}, {});
    if (!noMason) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step(*noMason, 1500);
    CHECK(terms_of(*noMason).lifted == 0);
    CHECK(city_of(noMason->pilot.w).stock.stock[kZiegel] <= noMason->bricks0);
    // no market cover: the bricks stay in the mason's output store, nothing is unloaded
    auto noMarket = start({kQuarryA}, {kMasonA});
    REQUIRE(noMarket);
    {
        sim::World& w = noMarket->pilot.w;
        sim::DemolishCmd dem;
        dem.city = kCity;
        for (const auto& l : city_of(w).index.lists)
            for (const auto& b : l.slots)
                if (b && w.types().find(b->type) != nullptr &&
                    w.types().find(b->type)->cls == sim::RuntimeClass::Market)
                    dem.tiles.push_back({b->x, b->y});
        REQUIRE(!dem.tiles.empty());
        REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
        noMarket->bricks0 = city_of(w).stock.stock[kZiegel];
    }
    step(*noMarket, 2000);
    const Terms nm = terms_of(*noMarket);
    CHECK(nm.made > 0);
    CHECK(nm.unloaded == 0);
    CHECK(nm.out_now > 0);
    require_closed(nm, "no market");
    // a full store: the brick store never rises above the capacity
    auto full = start({kQuarryA}, {kMasonA});
    REQUIRE(full);
    {
        sim::City& city =
            *test_must::must(sim::WorldTestAccess::islands(full->pilot.w)[1]->city(kCity.city));
        city.stock.stock[kZiegel] = city.stock.capacity;
        full->bricks0 = city.stock.capacity;
    }
    step(*full, 2000);
    CHECK(city_of(full->pilot.w).stock.stock[kZiegel] <= city_of(full->pilot.w).stock.capacity);
    const Terms ft = terms_of(*full);
    CHECK(ft.made > 0);
    require_closed(ft, "full store");
}

TEST_CASE("quarry marble: a marble quarry and a marble mason make MAMOR by the same rule") {
    // stage 13 (3C) is opened by a labelled setup of the owner's flags (the pilot stops at stage
    // 10)
    const Site marble_quarry{1218, 362, 355}, marble_mason{803, 358, 350};
    auto f = start({marble_quarry}, {marble_mason}, true, 1U << 13);
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step(*f, 2500);
    const Terms t = terms_of(*f, kMarmorstein, kMamor);
    say("marble", t);
    CHECK(t.lifted > 0);
    CHECK(t.made > 0);
    require_closed(t, "marble");
    // the marble quarry has ONE entrance: a flag per entrance, never more
    CHECK(claims_of(f->pilot.w, f->quarries[0]).size() == 1);
    // the stone is MARMORSTEIN: a stone mason (804) on the same quarry would find no candidate, and
    // the marble mason never takes from a stone quarry (the output good must equal the input)
    auto mix = start({Site{1201, 362, 355}}, {marble_mason}, true, 1U << 13);
    REQUIRE(mix);
    step(*mix, 1500);
    CHECK(terms_of(*mix, kMarmorstein, kMamor).lifted == 0);
}

// ---- DECIDED Q-A: the claim set ----------------------------------------------------------------

/// Steps `n` advances one at a time; the largest number of workers that hold a claim at once, and
/// whether the quarry's flags equalled the claimants at every advance.
struct ClaimRun {
    std::size_t max_claimants = 0, max_flags = 0;
    bool flags_equal_claimants = true;
};
ClaimRun run_claims(Fix& f, sim::BuildingId quarry, int n) {
    ClaimRun r;
    for (int i = 0; i < n; ++i) {
        step(f, 1);
        const std::size_t c = claimants(f), flags = set_claims(f.pilot.w, quarry);
        r.max_claimants = std::max(r.max_claimants, c);
        r.max_flags = std::max(r.max_flags, flags);
        r.flags_equal_claimants = r.flags_equal_claimants && flags == c;
        CHECK(f.pilot.w.check_invariants().empty());
    }
    return r;
}

TEST_CASE("quarry claims Q-A1: two masons and one 1208 (one entrance) never share the entrance") {
    auto f = start({kQuarryB}, {kMasonB1, kMasonB2});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const ClaimRun r = run_claims(*f, f->quarries[0], 4000);
    CHECK(r.max_claimants == 1); // the second mason finds no unclaimed goal and comes home empty
    CHECK(r.max_flags == 1);
    CHECK(r.flags_equal_claimants);
    const Terms t = terms_of(*f);
    CHECK(t.lifted > 0);
    CHECK(f->stats.forestry.mason_delivered_88[f->masons[0]] > 0);
    CHECK(f->stats.forestry.mason_delivered_88[f->masons[1]] > 0); // both get a turn over time
    // the second mason's failed searches exist (it met the claimed entrance)
    CHECK(f->stats.forestry.mason_searches_failed[f->masons[0]] +
              f->stats.forestry.mason_searches_failed[f->masons[1]] >
          0);
    require_closed(t, "one entrance");
}

TEST_CASE("quarry claims Q-A2: two masons and one 1201 (two entrances) work at once") {
    auto f = start({Site{1201, 361, 357}}, {kMasonB1, kMasonB2});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const ClaimRun r = run_claims(*f, f->quarries[0], 4000);
    CHECK(r.max_claimants == 2); // one worker per entrance
    CHECK(r.max_flags == 2);
    CHECK(r.flags_equal_claimants);
    require_closed(terms_of(*f), "two entrances");
}

TEST_CASE("quarry claims Q-A3: a quarry replaced under a waiting worker is looked up by tile") {
    auto f = start({kQuarryB}, {kMasonB1, kMasonB2});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    // the first worker waits at the quarry
    const int k = step_until(*f, 4000, [&] { return workers_in(*f, sim::HarvestStep::Cut) > 0; });
    REQUIRE(k > 0);
    CHECK(set_claims(w, f->quarries[0]) == 1);
    const sim::BuildingId old_quarry = f->quarries[0];
    // replace the quarry by public commands: the old one goes, a new one stands on the same tiles
    sim::DemolishCmd dem;
    dem.city = kCity;
    dem.tiles = {{kQuarryB.x, kQuarryB.y}};
    REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    const sim::ApplyResult r = w.apply(build_cmd(kQuarryB.type, kQuarryB.x, kQuarryB.y));
    REQUIRE_MESSAGE(r.ok(), sim::reject_name(r.status));
    const sim::BuildingId fresh = r.records.at(0).building;
    CHECK(fresh != old_quarry);
    f->quarries.push_back(fresh);
    CHECK(set_claims(w, fresh) == 0);    // a new quarry starts with no claims (Q-A rule 4)
    CHECK(w.check_invariants().empty()); // flags (0) never exceed the workers
    // the second mason may now claim the NEW quarry's one entrance while the first still waits at
    // the tile: the first worker's end of wait then clears the new quarry's flag (D: the building
    // is found by mask-8 lookup at the stored tile, 0x1005544e, not remembered)
    bool crossed = false;
    for (int i = 0; i < 400 && workers_in(*f, sim::HarvestStep::Cut) > 0; ++i) {
        step(*f, 1);
        crossed = crossed || (claimants(*f) == 2 && set_claims(w, fresh) == 1);
        CHECK(w.check_invariants().empty());
    }
    // the old worker's wait has ended: it took a ton from whatever stands at the tile (the new
    // quarry), and cleared the flag there
    CHECK(lifted_by(*f, fresh) > 0);
    CHECK(lifted_by(*f, old_quarry) == 0);
    if (crossed) {
        CHECK(set_claims(w, fresh) ==
              0); // cleared under the second worker, who still walks or waits
        CHECK(claimants(*f) >= 1);
    }
    MESSAGE("second worker claimed the new quarry before the old one finished: " << crossed);
    CHECK(w.check_invariants().empty());
    require_closed(terms_of(*f), "replaced quarry");
}

TEST_CASE("quarry claims Q-A4: the shared end path clears the claim of a worker it ends") {
    auto f = start({kQuarryB}, {kMasonB1});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    for (const sim::HarvestStep at : {sim::HarvestStep::ToTree, sim::HarvestStep::Cut}) {
        sim::World c = w;
        auto run = std::make_unique<Fix>(World{std::move(c), f->pilot.types});
        run->masons = f->masons;
        run->quarries = f->quarries;
        run->pilot.w.set_transport_stats(&run->stats);
        const int k = step_until(*run, 4000, [&] { return workers_in(*run, at) > 0; });
        REQUIRE_MESSAGE(k > 0, "no worker reached step " << int(at));
        sim::Figure* fig = nullptr;
        for (auto& slot : sim::WorldTestAccess::figures(run->pilot.w).list.slots)
            if (slot && slot->harvest.has_value() && sim::mason_job(slot->job) &&
                !slot->expiry_stamp.has_value())
                fig = &*slot;
        REQUIRE(fig != nullptr);
        const sim::TileXY tile = fig->harvest->target;
        const std::uint8_t col = fig->harvest->goal_col, row = fig->harvest->goal_row;
        sim::Effects fx;
        CHECK(sim::end_figure(*fig, static_cast<std::uint32_t>(run->pilot.w.clock().tick()), fx));
        // the end path asks the merge to clear exactly the flag the worker holds
        REQUIRE(fx.quarry_claims.size() == 1);
        CHECK(!fx.quarry_claims[0].set);
        CHECK(fx.quarry_claims[0].tile == tile);
        CHECK(fx.quarry_claims[0].col == col);
        CHECK(fx.quarry_claims[0].row == row);
        CHECK(fig->harvest->step == sim::HarvestStep::Done);
        // a worker that is not walking or waiting holds nothing: ending it asks for no write
        sim::Effects none;
        static_cast<void>(
            sim::end_figure(*fig, static_cast<std::uint32_t>(run->pilot.w.clock().tick()), none));
        CHECK(none.quarry_claims.empty());
    }
}

// ---- review P2: the invariants fire, the radius binds, the pure source rules
// ---------------------

template <class P> P* payload_of(sim::World& w, sim::BuildingId id) {
    for (auto& city : sim::WorldTestAccess::islands(w)[1]->cities)
        if (city)
            for (auto& list : city->index.lists)
                for (auto& slot : list.slots)
                    if (slot && slot->id == id)
                        return std::get_if<P>(&slot->payload);
    return nullptr;
}

TEST_CASE("quarry registry: only the quarries output STEINE or MARMORSTEIN (the MS2 guard)") {
    auto p = make(false, false, 0);
    if (!p) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const sim::TypeRegistry& types = p->w.types();
    std::set<sim::GoodsId> stones;
    for (sim::GoodsId g = 0; g < 0x8f; ++g) {
        const sim::GoodsInfo* gi = types.goods(g);
        if (gi != nullptr && (gi->name == "STEINE" || gi->name == "MAMORSTEIN"))
            stones.insert(g);
    }
    REQUIRE(stones.size() == 2);
    std::size_t quarries = 0;
    for (const sim::BuildingType& t : types.types()) {
        bool outputs_stone = false;
        for (const sim::GoodsId g : t.output_goods)
            outputs_stone = outputs_stone || stones.contains(g);
        INFO("type " << t.id.v << " class " << int(t.cls));
        if (outputs_stone) {
            CHECK(t.cls == sim::RuntimeClass::Quarry);
            ++quarries;
        }
    }
    // 1201, 1208 (STEINE), 1218, 1220 (MAMORSTEIN): if a non-quarry source of stone ever appears in
    // the data, the non-quarry source path of the mason search (mutant MS2) becomes reachable and
    // needs a play-level test
    CHECK(quarries == 4);
}

TEST_CASE("quarry invariants: a corrupted claim set is refused, in the world and on load") {
    // The pilot's island holds other players' masons whose workers legitimately walk or wait. The
    // legal control is the world as it is; for the corrupted copies below those workers are moved
    // to the return walk and every quarry's flags are cleared (labelled edits), so that no worker
    // holds a claim and any set flag is a violation.
    auto f = start({kQuarryA}, {});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    step(*f, 400);
    sim::World& w = f->pilot.w;
    CHECK(w.check_invariants().empty()); // the legal control
    // (a) a set flag with no mason worker walking to or waiting at a quarry
    {
        sim::World c = w;
        for (auto& fig : sim::WorldTestAccess::figures(c).list.slots)
            if (fig && fig->harvest.has_value() && sim::mason_job(fig->job) &&
                !fig->expiry_stamp.has_value()) {
                fig->harvest->step = sim::HarvestStep::ToHome;
                fig->harvest->wait = 0;
            }
        for (auto& is : sim::WorldTestAccess::islands(c))
            if (is)
                for (auto& city : is->cities)
                    if (city)
                        for (auto& list : city->index.lists)
                            for (auto& slot : list.slots)
                                if (auto* qq =
                                        slot ? std::get_if<sim::Quarry>(&slot->payload) : nullptr)
                                    std::fill(qq->claims.begin(), qq->claims.end(), false);
        CHECK(c.check_invariants().empty()); // still legal with no flag set
        sim::Quarry* q = payload_of<sim::Quarry>(c, f->quarries[0]);
        REQUIRE(q != nullptr);
        REQUIRE(q->claims.size() == 2);
        q->claims[0] = true;
        const std::string problem = c.check_invariants();
        INFO("problem: " << problem);
        CHECK(problem.find("quarry claim flags are set") != std::string::npos);
    }
    // (b) a claim set of the wrong length (one flag too many)
    {
        sim::World c = w;
        sim::Quarry* q = payload_of<sim::Quarry>(c, f->quarries[0]);
        REQUIRE(q != nullptr);
        q->claims.push_back(false);
        const std::string problem = c.check_invariants();
        CHECK(problem.find("claims has 3 entries, the type needs 2") != std::string::npos);
        // (c) the same save is refused on load
        const auto bytes = c.save();
        auto loaded = sim::World::load(bytes, f->pilot.types, {});
        CHECK(!loaded.ok());
        if (!loaded.ok()) {
            CHECK(loaded.error().find("claims has 3 entries") != std::string::npos);
        }
    }
    // the legal save loads
    CHECK(sim::World::load(w.save(), f->pilot.types, {}).ok());
}

TEST_CASE("quarry radius: a quarry beyond the mason's radius 12 is not used, moved inside it is") {
    auto f = start({}, {kMasonA});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    // the mason (358,350) stands 3x3: centre (359,351), radius 12, search square x 347 .. 371 and
    // y 339 .. 363; a quarry (3 x 4) whose footprint does not overlap it is out of reach
    std::optional<std::pair<std::int32_t, std::int32_t>> out;
    for (std::int32_t x = 340; x <= 392 && !out; ++x)
        for (std::int32_t y = 330; y <= 380 && !out; ++y) {
            const bool overlaps = x <= 371 && x + 2 >= 347 && y <= 363 && y + 3 >= 339;
            if (overlaps)
                continue;
            sim::World c = w;
            if (c.apply(build_cmd(1201, x, y)).ok())
                out = {x, y};
        }
    MESSAGE("out-of-reach quarry at " << out->first << "," << out->second);
    REQUIRE(out.has_value());
    const sim::ApplyResult far = w.apply(build_cmd(1201, out->first, out->second));
    REQUIRE(far.ok());
    f->quarries.push_back(far.records.at(0).building);
    step(*f, 1500);
    const sim::BuildingId m = f->masons[0];
    CHECK(terms_of(*f).lifted == 0);
    CHECK(f->stats.forestry.mason_searches[m] > 0);
    CHECK(f->stats.forestry.mason_searches_failed[m] == f->stats.forestry.mason_searches[m]);
    // moved inside the radius (demolish, rebuild at the standard site): it serves
    sim::DemolishCmd dem;
    dem.city = kCity;
    dem.tiles = {{out->first, out->second}};
    REQUIRE(w.apply(sim::Command{kPlayer, dem}).ok());
    const sim::ApplyResult near = w.apply(build_cmd(kQuarryA.type, kQuarryA.x, kQuarryA.y));
    REQUIRE(near.ok());
    f->quarries.push_back(near.records.at(0).building);
    step(*f, 1500);
    CHECK(terms_of(*f).lifted > 0);
    require_closed(terms_of(*f), "radius");
}

TEST_CASE(
    "mason rules: the source rules of the search, pure (output good, stock, non-quarry source)") {
    auto f = start({kQuarryA}, {kMasonA});
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    sim::World& w = f->pilot.w;
    const sim::TypeRegistry& types = w.types();
    const sim::Building* quarry = building(w, f->quarries[0]);
    const sim::BuildingType* qt = types.find(quarry->type);
    const sim::City& city = city_of(w);
    // a quarry answers 256 raw for any goods, forever
    for (const sim::GoodsId g : {kStein, kMarmorstein, sim::GoodsId{42}})
        CHECK(sim::mason_candidate_stock(*quarry, *qt, g, &city) == 256);
    // the output good must be the mason's input (STEINE for 1201, MARMORSTEIN for 1218)
    CHECK(sim::mason_output_matches(*qt, kStein));
    CHECK(!sim::mason_output_matches(*qt, kMarmorstein));
    CHECK(!sim::mason_output_matches(*qt, kZiegel));
    CHECK(sim::mason_output_matches(*types.find(sim::TypeId{1218}), kMarmorstein));
    // a non-quarry source (a forester, output HOLZ) counts its real stock: 255 raw does not
    // qualify, 256 does (no such source exists for STEINE in the shipped data, so the search cannot
    // reach this rule in play; the pure rule is the evidence)
    const sim::Building* forester = find_type(w, 801);
    REQUIRE(forester != nullptr);
    sim::Building fb = *forester;
    const sim::BuildingType* ft = types.find(fb.type);
    REQUIRE(ft != nullptr);
    auto* farm = std::get_if<sim::Farm>(&fb.payload);
    REQUIRE(farm != nullptr);
    farm->reserved = 0;
    farm->out = 255;
    CHECK(sim::mason_candidate_stock(fb, *ft, 42, &city) == 255);
    CHECK(!sim::mason_stock_qualifies(sim::mason_candidate_stock(fb, *ft, 42, &city)));
    farm->out = 256;
    CHECK(sim::mason_stock_qualifies(sim::mason_candidate_stock(fb, *ft, 42, &city)));
    farm->reserved = 100; // the real stock is the output minus the reservation
    CHECK(!sim::mason_stock_qualifies(sim::mason_candidate_stock(fb, *ft, 42, &city)));
}

TEST_CASE("quarry job: the need gate inside the search sends a worker home empty-handed") {
    // Unreachable in play (the farm dispatches only when raw/need <= 8, raw <= 188; the gate closes
    // at 1,260), so the labelled edit raises the mason's raw store after a worker was sent.
    for (const std::uint32_t raw : {1300U, 100U}) {
        auto f = start({kQuarryA}, {kMasonA});
        if (!f) {
            MESSAGE("skipping: ANNO1503_GAME_DIR not set");
            return;
        }
        sim::World& w = f->pilot.w;
        const sim::BuildingId m = f->masons[0];
        const int k = step_until(*f, 2000, [&] {
            for (const sim::Figure* fig : workers(*f))
                if (fig->harvest->step == sim::HarvestStep::Start && fig->harvest->start_delay > 0)
                    return true;
            return false;
        });
        REQUIRE(k > 0);
        auto* farm = payload_of<sim::Farm>(w, m);
        REQUIRE(farm != nullptr);
        const std::uint64_t before = f->stats.forestry.mason_searches[m];
        farm->raw = raw;
        step(*f, 3);
        INFO("raw " << raw);
        CHECK(f->stats.forestry.mason_searches[m] > before); // the worker searched
        if (raw >= 1260) {
            CHECK(f->stats.forestry.mason_searches_failed[m] ==
                  f->stats.forestry.mason_searches[m]); // no candidate: the gate is closed
            CHECK(terms_of(*f).lifted == 0);
        } else {
            step(*f, 400);
            CHECK(terms_of(*f).lifted > 0); // the legal control: below the gate it goes
        }
    }
}

// ---- Q-7: save and reload mid-trip equal the uninterrupted run ------------------------------

template <class At>
void reload_at(const char* boundary, const std::vector<Site>& quarries,
               const std::vector<Site>& masons, At at) {
    auto f = start(quarries, masons);
    if (!f) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    INFO("boundary: " << boundary);
    const int k = step_until(*f, 5000, [&] { return at(*f); });
    REQUIRE_MESSAGE(k > 0, "the boundary state was never reached");
    sim::World& w = f->pilot.w;
    const auto bytes = w.save();
    auto loaded = sim::World::load(bytes, f->pilot.types, {});
    REQUIRE_MESSAGE(loaded.ok(), (loaded.ok() ? std::string() : loaded.error()));
    sim::World& re = loaded.value();
    CHECK(re.check_invariants().empty());
    CHECK(re.save() == bytes);
    CHECK(re.state_hash() == w.state_hash());
    for (int i = 0; i < 800; ++i) {
        w.advance(kAdvanceMs);
        re.advance(kAdvanceMs);
        REQUIRE_MESSAGE(!(w.faulted() || re.faulted()), w.fault());
    }
    CHECK(re.state_hash() == w.state_hash());
    CHECK(re.rng().cursor() == w.rng().cursor());
    CHECK(re.save() == w.save());
    CHECK(re.check_invariants().empty());
    // and a reload that runs one advance fewer differs, so the comparison can tell
    auto shorter = sim::World::load(bytes, f->pilot.types, {});
    REQUIRE(shorter.ok());
    for (int i = 0; i < 799; ++i)
        shorter.value().advance(kAdvanceMs);
    CHECK(shorter.value().state_hash() != w.state_hash());
}

TEST_CASE("quarry continuation Q-7: a reload mid-walk, mid-wait and mid-return equals the run") {
    const std::vector<Site> q{Site{1201, 361, 357}}, m{kMasonB1, kMasonB2};
    reload_at("two workers walk to the quarry holding claims", q, m,
              [](Fix& f) { return workers_in(f, sim::HarvestStep::ToTree) == 2; });
    reload_at("a worker waits at the quarry, the other walks", q, m, [](Fix& f) {
        return workers_in(f, sim::HarvestStep::Cut) >= 1 &&
               workers_in(f, sim::HarvestStep::ToTree) >= 1;
    });
    reload_at("two workers wait at the quarry (both flags set)", q, m, [](Fix& f) {
        return workers_in(f, sim::HarvestStep::Cut) == 2 &&
               set_claims(f.pilot.w, f.quarries[0]) == 2;
    });
    reload_at("a worker walks home with the ton", q, m, [](Fix& f) {
        for (const sim::Figure* fig : workers(f))
            if (fig->harvest->step == sim::HarvestStep::ToHome && fig->cargo_88 == 256)
                return true;
        return false;
    });
    reload_at("a brick cart carries bricks to the store", {kQuarryA}, {kMasonA},
              [](Fix& f) { return terms_of(f).brick_hands > 0; });
}

} // namespace
