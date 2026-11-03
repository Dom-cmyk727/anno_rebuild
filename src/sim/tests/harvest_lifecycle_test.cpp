// MAP-ECON-1 E3: the harvest lifecycle through the real scheduler (docs/sim/forestry.md 3.2, 3.5,
// E-14, E-21; docs/re/forestry.md 2.4-2.10). Every expectation is derived in the comment next to it
// from the contract's rules, not read back from the C++.
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/figure.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/world.hpp>

#include "harvest_helpers.hpp"
#include "world_access.hpp"

namespace sim = anno::sim;
using namespace harvest_test;

namespace {

sim::Building* building_of(sim::World& w, std::size_t island, sim::TypeId type) {
    sim::City& c = *sim::WorldTestAccess::islands(w)[island]->cities[0];
    for (auto& list : c.index.lists) {
        for (auto& b : list.slots) {
            if (b.has_value() && b->type == type) {
                return &*b;
            }
        }
    }
    return nullptr;
}

sim::Farm& forester(sim::World& w) {
    sim::Building* b = building_of(w, 0, kForester);
    REQUIRE(b != nullptr);
    return std::get<sim::Farm>(b->payload);
}

/// The forester's workers (kind kWorker), in slot order.
std::vector<sim::Figure*> workers(sim::World& w) {
    std::vector<sim::Figure*> out;
    for (auto& f : sim::WorldTestAccess::figures(w).list.slots) {
        if (f.has_value() && f->kind == kWorker) {
            out.push_back(&*f);
        }
    }
    return out;
}

std::uint32_t object_at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[0];
    return is.tiles.object(is.tile_index(x, y));
}
bool claimed_at(const sim::World& w, std::int32_t x, std::int32_t y) {
    const sim::Island& is = *w.islands()[0];
    return is.tiles.claimed(is.tile_index(x, y));
}

/// Advances 1 tick (2 ms) at a time until `pred` holds or `max_ticks` passed.
template <class Pred> bool tick_until(sim::World& w, Pred pred, int max_ticks) {
    for (int i = 0; i < max_ticks; ++i) {
        if (pred()) {
            return true;
        }
        w.advance(2);
    }
    return pred();
}

/// A forester at (110, 110) (3 x 3, entrance inside), trees as given, a fresh world whose first
/// farm step (the first city visit after tick 500) sends the first worker.
struct Forest {
    std::shared_ptr<const sim::TypeRegistry> types;
    sim::World w;
    sim::TransportStats stats;
};

Forest make_forest(const std::vector<std::pair<std::int32_t, std::int32_t>>& trees,
                   sim::TypeId tree = kTree, const sim::Rules& rules = sim::Rules{},
                   std::uint32_t capacity = 256) {
    Rig r = make_rig();
    for (const auto& [x, y] : trees) {
        r.object(x, y, tree);
    }
    r.place(kForester, 110, 110, sim::Farm{});
    auto types = registry(capacity);
    Forest f{types, create(r, types, rules), {}};
    return f;
}

} // namespace

TEST_CASE("harvest lifecycle: one worker cuts the tree, carries 256 home, Farm.raw gets it") {
    Forest f = make_forest({{115, 111}});
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    REQUIRE(object_at(w, 115, 111) == kTree.v);
    // Until the first worker exists.
    REQUIRE(tick_until(w, [&] { return !workers(w).empty(); }, 2000));
    sim::Figure* worker = workers(w)[0];
    const sim::FigureId id = worker->id;
    // Run until that worker has ended (expiry stamp) or a generous bound: 5 + walk (< 3 000) + 6
    // 500 + walk home (< 3 000) ticks, plus the visit lag once the registry has grown.
    const auto ended = [&] {
        const sim::Figure* g = sim::find_figure(w.figures(), id);
        return g != nullptr && g->expiry_stamp.has_value();
    };
    REQUIRE(tick_until(w, ended, 20000));
    const sim::Figure* g = sim::find_figure(w.figures(), id);
    // The tree became its young stage (id - 1) with phase & 0x80 (0 here), unclaimed.
    CHECK(object_at(w, 115, 111) == kSapling.v);
    CHECK_FALSE(claimed_at(w, 115, 111));
    // One successful cut credited 256 and the worker handed exactly that to its home's raw store.
    CHECK(f.stats.forestry.cuts == 1);
    CHECK(f.stats.forestry.raw_credited_88 == 256);
    CHECK(f.stats.forestry.raw_delivered_88 == 256);
    CHECK(f.stats.forestry.raw_lost_88 == 0);
    CHECK(f.stats.forestry.cut_mismatches == 0);
    CHECK(g->cargo_88 == 256);    // historical: the ended figure keeps its last cargo fields
    CHECK_FALSE(g->home.valid()); // released at its end
    // Farm.raw = delivered - consumed by the farm's steps since (the step's own arithmetic).
    const auto& ledger = f.stats.forestry.farms;
    REQUIRE(ledger.size() == 1);
    CHECK(forester(w).raw == 256 - ledger.begin()->second.consumed_88);
    CHECK(w.check_invariants() == "");
}

namespace {

/// Advances until the forester's first worker exists (the spawning advance visits it with 0
/// elapsed ticks: nothing happens yet).
sim::FigureId first_worker(sim::World& w) {
    REQUIRE(tick_until(w, [&] { return !workers(w).empty(); }, 2000));
    return workers(w)[0]->id;
}

sim::Figure& fig_of(sim::World& w, sim::FigureId id) {
    sim::Figure* g = sim::find_figure(sim::WorldTestAccess::figures(w), id);
    REQUIRE(g != nullptr);
    return *g;
}

sim::BuildingKey key_of(const sim::World& w, sim::BuildingId id) {
    const sim::Island& is = *w.islands()[sim::island_of(id).v];
    return is.cache.by_id.at(id);
}

} // namespace

TEST_CASE("harvest lifecycle: the first delay through World::advance (E-21, T-F19)") {
    // After the spawning advance the worker owes 5 ticks. 1-tick advances visit the lone worker
    // every time (the clamped phase-B count is at least 1).
    const auto fresh = [] {
        Forest f = make_forest({{115, 111}});
        const sim::FigureId id = first_worker(f.w);
        REQUIRE(fig_of(f.w, id).harvest->start_delay == 5);
        return std::pair{std::move(f), id};
    };
    // (a) five 1-tick advances: 4, 3, 2, 1 owed, then the fifth starts the job (search, claim,
    // walk) with no surplus.
    auto [a, ida] = fresh();
    for (std::uint32_t owed : {4U, 3U, 2U, 1U}) {
        a.w.advance(2);
        const sim::Figure& g = fig_of(a.w, ida);
        CHECK(g.harvest->start_delay == owed);
        CHECK(g.harvest->step == sim::HarvestStep::Start);
        CHECK_FALSE(claimed_at(a.w, 115, 111));
    }
    a.w.advance(2);
    const sim::HarvestState after_five = *fig_of(a.w, ida).harvest;
    CHECK(after_five.start_delay == 0);
    CHECK(after_five.step == sim::HarvestStep::ToTree);
    CHECK(after_five.target == sim::TileXY{115, 111});
    CHECK(after_five.along == 0); // no surplus: the walk has not moved yet
    CHECK(claimed_at(a.w, 115, 111));
    // (b) one 5-tick advance: the same worker and tile state.
    auto [b, idb] = fresh();
    b.w.advance(10);
    CHECK(*fig_of(b.w, idb).harvest == after_five);
    CHECK(fig_of(b.w, idb).pos == fig_of(a.w, ida).pos);
    CHECK(claimed_at(b.w, 115, 111));
    // (c) zero elapsed time does nothing.
    auto [c, idc] = fresh();
    c.w.advance(0);
    CHECK(fig_of(c.w, idc).harvest->start_delay == 5);
    c.w.advance(1); // 1 ms: still tick 0 elapsed for the figure (ms >> 1)
    CHECK(fig_of(c.w, idc).harvest->start_delay == 5);
    // (d) save and reload after two ticks, then three more: the same as (a), and the whole world
    // hashes equal (the same advance partition).
    auto [d, idd] = fresh();
    d.w.advance(2);
    d.w.advance(2);
    CHECK(fig_of(d.w, idd).harvest->start_delay == 3);
    auto loaded = sim::World::load(d.w.save(), d.types);
    REQUIRE(loaded.ok());
    sim::World r = std::move(loaded).value();
    CHECK(fig_of(r, idd).harvest->start_delay == 3);
    for (int i = 0; i < 3; ++i) {
        r.advance(2);
    }
    CHECK(*fig_of(r, idd).harvest == after_five);
    CHECK(r.state_hash() == a.w.state_hash());
    // (e) a 50-tick first visit: 5 for the delay, 45 walked; equal (worker and tile) to (a)
    // followed by 45 single ticks (the isolated transition, not the whole world).
    auto [e, ide] = fresh();
    e.w.advance(100);
    for (int i = 0; i < 45; ++i) {
        a.w.advance(2);
    }
    CHECK(*fig_of(e.w, ide).harvest == *fig_of(a.w, ida).harvest);
    CHECK(fig_of(e.w, ide).pos == fig_of(a.w, ida).pos);
    CHECK(fig_of(e.w, ide).harvest->along > 0);
}

TEST_CASE("harvest lifecycle: capacity 768, two trees, one long call: the overlay (E-14, T-F13)") {
    // A data-defined capacity of 3 t repeats the job from step A after each cut. One visit of 40
    // 000 ticks does everything in one call: the second search must see the first tree's claim and
    // cut through the overlay (else it would pick the same tree again, E-15), and the first delay
    // is not re-armed at the second Start (E-21).
    Forest f = make_forest({{115, 111}, {111, 115}}, kTree, sim::Rules{}, 768);
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    const sim::FigureId id = first_worker(w);
    w.advance(80000); // 40 000 ticks in one figure call
    const sim::Figure& g = fig_of(w, id);
    CHECK(f.stats.forestry.cuts == 2);
    CHECK(f.stats.forestry.cuts_failed == 0);
    CHECK(g.cargo_88 == 512); // two successful planned consumptions (not 768 from one tree)
    CHECK(object_at(w, 115, 111) == kSapling.v);
    CHECK(object_at(w, 111, 115) == kSapling.v);
    CHECK_FALSE(claimed_at(w, 115, 111));
    CHECK_FALSE(claimed_at(w, 111, 115));
    CHECK(g.expiry_stamp.has_value()); // 768 > 512: a third search failed -> home -> Done
    CHECK(f.stats.forestry.raw_delivered_88 == 512);
    CHECK(f.stats.forestry.cut_mismatches == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE(
    "harvest lifecycle: after a cut the next Start runs at once (the delay is not re-armed)") {
    Forest f = make_forest({{115, 111}, {111, 115}}, kTree, sim::Rules{}, 768);
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    const sim::FigureId id = first_worker(w);
    forester(w).switched_on = false; // one worker: a second one would claim the second tree first
    // 1-tick advances until the first cut happened: in that same call the job is back in step A,
    // which searched and claimed the second tree at once (no 5 ticks owed, E-21).
    REQUIRE(tick_until(w, [&] { return f.stats.forestry.cuts == 1; }, 12000));
    const sim::Figure& g = fig_of(w, id);
    // The other tree (whichever the walk search popped second) is the new target, claimed.
    const sim::TileXY other =
        object_at(w, 115, 111) == kTree.v ? sim::TileXY{115, 111} : sim::TileXY{111, 115};
    REQUIRE(object_at(w, other.x, other.y) == kTree.v);
    CHECK(g.harvest->start_delay == 0);
    CHECK(g.harvest->step == sim::HarvestStep::ToTree);
    CHECK(g.harvest->target == other);
    CHECK(claimed_at(w, other.x, other.y));
    CHECK(g.cargo_88 == 256);
}

TEST_CASE("harvest lifecycle: the cut comes exactly the type's wait after the arrival") {
    // One worker only (the farm is switched off after its first send): the registry keeps one slot
    // and the clamped phase-B count visits that worker at every 1-tick advance.
    Forest f = make_forest({{115, 111}});
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    const sim::FigureId id = first_worker(w);
    forester(w).switched_on = false;
    REQUIRE(w.figures().list.slots.size() == 1);
    REQUIRE(
        tick_until(w, [&] { return fig_of(w, id).harvest->step == sim::HarvestStep::Cut; }, 4000));
    // The arrival visit starts the wait with its leftover ticks: a 1-tick visit leaves none (the
    // walk hands back whole ticks), so the whole wait is owed. Count the ticks until the cut.
    const std::uint32_t owed = fig_of(w, id).harvest->wait;
    CHECK(owed == 6500);
    std::uint32_t ticks = 0;
    while (f.stats.forestry.cuts == 0 && ticks < 7000) {
        w.advance(2);
        ++ticks;
    }
    CHECK(ticks == owed); // one tick per advance, the cut in the visit that uses the last owed tick
}

TEST_CASE("harvest lifecycle: contention, two workers and one tree") {
    // The first worker claims the tree. The second (the next farm step, 500 ticks later) finds the
    // bit set: step A fails, it goes home (inside the footprint: no walk), delivers 0 and ends; the
    // farm's counter is back and the next step sends again. The tree is cut once.
    Forest f = make_forest({{115, 111}});
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    REQUIRE(tick_until(w, [&] { return f.stats.forestry.searches_failed >= 1; }, 3000));
    CHECK(f.stats.forestry.searches >= 2);
    REQUIRE(tick_until(w, [&] { return f.stats.forestry.cuts >= 1; }, 12000));
    for (int i = 0; i < 4000; ++i) {
        w.advance(2);
    }
    CHECK(f.stats.forestry.cuts == 1);
    CHECK(f.stats.forestry.raw_credited_88 == 256);
    CHECK(object_at(w, 115, 111) == kSapling.v);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("harvest lifecycle: two workers, two trees: two different trees") {
    Forest f = make_forest({{115, 111}, {110, 115}});
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    REQUIRE(tick_until(w, [&] { return workers(w).size() == 2; }, 3000));
    // The registry has grown to 100 slots: a figure is visited about every 50 one-tick advances.
    REQUIRE(tick_until(
        w,
        [&] {
            const auto v = workers(w);
            return v.size() == 2 && v[1]->harvest->step == sim::HarvestStep::ToTree;
        },
        400));
    const auto ws = workers(w);
    REQUIRE(ws.size() == 2);
    REQUIRE(ws[0]->harvest->step == sim::HarvestStep::ToTree);
    CHECK(ws[0]->harvest->target != ws[1]->harvest->target);
    REQUIRE(tick_until(w, [&] { return f.stats.forestry.raw_delivered_88 == 512; }, 16000));
    CHECK(f.stats.forestry.cuts == 2);
    CHECK(object_at(w, 115, 111) == kSapling.v);
    CHECK(object_at(w, 110, 115) == kSapling.v);
}

TEST_CASE("harvest lifecycle: an uncuttable tree, faithful and with skip_uncuttable") {
    // kOddTree's id - 1 is ROHSTOFF: the cut always fails. Faithful (the original's candidate
    // list): the worker chooses it, waits the full 6500 ticks, gets nothing, and (256 > 0) searches
    // again.
    Forest f = make_forest({{115, 111}}, kOddTree);
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    REQUIRE(tick_until(w, [&] { return f.stats.forestry.cuts_failed >= 1; }, 12000));
    for (int i = 0; i < 3000; ++i) { // time for a (wrong) credit to travel home
        w.advance(2);
    }
    CHECK(f.stats.forestry.cuts == 0);
    CHECK(f.stats.forestry.raw_credited_88 == 0);
    CHECK(object_at(w, 115, 111) == kOddTree.v);
    // No consumption, no cargo, no raw: nothing reaches the farm (credit follows the cut).
    for (const sim::Figure* g : workers(w)) {
        CHECK(g->cargo_88 == 0);
    }
    CHECK(f.stats.forestry.raw_delivered_88 == 0);
    CHECK(forester(w).raw == 0);
    // Enhanced skip: the tile is no candidate; every search fails and nothing waits there.
    sim::Rules skip;
    skip.forestry_skip_uncuttable = true;
    Forest g = make_forest({{115, 111}}, kOddTree, skip);
    g.w.set_transport_stats(&g.stats);
    for (int i = 0; i < 3000; ++i) {
        g.w.advance(2);
    }
    CHECK(g.stats.forestry.searches >= 1);
    CHECK(g.stats.forestry.searches_failed == g.stats.forestry.searches);
    CHECK_FALSE(claimed_at(g.w, 115, 111));
}

TEST_CASE("harvest lifecycle: the home is demolished on the way out, and while carrying") {
    for (const bool carrying : {false, true}) {
        INFO("carrying " << carrying);
        Forest f = make_forest({{115, 111}});
        sim::World& w = f.w;
        w.set_transport_stats(&f.stats);
        const sim::FigureId id = first_worker(w);
        const sim::HarvestStep at = carrying ? sim::HarvestStep::ToHome : sim::HarvestStep::ToTree;
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).harvest->step == at; }, 12000));
        const sim::BuildingId home = fig_of(w, id).home;
        REQUIRE(sim::WorldTestAccess::remove(w, key_of(w, home)));
        // The worker stays a valid state with an unresolved home (no repair, no invariant).
        CHECK(w.check_invariants() == "");
        CHECK(fig_of(w, id).home == home);
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).expiry_stamp.has_value(); }, 12000));
        CHECK(f.stats.forestry.raw_delivered_88 == 0);
        if (!carrying) {
            // The abort at the arrival: claim cleared, no cut (docs/re/forestry.md 4.3).
            CHECK(f.stats.forestry.aborts == 1);
            CHECK(f.stats.forestry.cuts == 0);
            CHECK(object_at(w, 115, 111) == kTree.v);
            CHECK_FALSE(claimed_at(w, 115, 111));
        } else {
            // Cut already done; the cargo is lost with the figure (no home to receive it).
            CHECK(f.stats.forestry.cuts == 1);
            CHECK(f.stats.forestry.raw_lost_88 == 256);
            CHECK(object_at(w, 115, 111) == kSapling.v);
        }
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("harvest lifecycle: placement over a claimed tree, faithful keeps the claim") {
    // Faithful (E2.0): the placement writer keeps bit 29; the worker walks on, arrives, waits,
    // clears the bit and runs the cut on whatever the tile holds (a market: id - 1 is no
    // ROHSTWACHS: no change, no credit), then searches again.
    Forest f = make_forest({{115, 111}});
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    const sim::FigureId id = first_worker(w);
    REQUIRE(tick_until(
        w, [&] { return fig_of(w, id).harvest->step == sim::HarvestStep::ToTree; }, 100));
    auto made = sim::WorldTestAccess::create(w, 0, 0, sim::PlacementRecord{kMarket, 114, 110});
    REQUIRE(made.ok());
    CHECK(claimed_at(w, 115, 111)); // kept
    CHECK(object_at(w, 115, 111) == kMarket.v);
    CHECK(w.check_invariants() == "");
    REQUIRE(tick_until(w, [&] { return f.stats.forestry.cuts_failed >= 1; }, 12000));
    CHECK(f.stats.forestry.cuts == 0);
    CHECK(object_at(w, 115, 111) == kMarket.v);
    CHECK(fig_of(w, id).cargo_88 == 0);
}

TEST_CASE("harvest lifecycle: enhanced hygiene invalidates at placement, the owner drops") {
    sim::Rules rules;
    rules.forestry_claim_hygiene = true;
    Forest f = make_forest({{115, 111}}, kTree, rules);
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    const sim::FigureId id = first_worker(w);
    REQUIRE(tick_until(
        w, [&] { return fig_of(w, id).harvest->step == sim::HarvestStep::ToTree; }, 100));
    CHECK(w.islands()[0]->claims.at(sim::TileXY{115, 111}) == id);
    auto made = sim::WorldTestAccess::create(w, 0, 0, sim::PlacementRecord{kMarket, 114, 110});
    REQUIRE(made.ok());
    // The invalidation: bit and entry gone; the worker still targets the tile (E-iii, valid).
    CHECK_FALSE(claimed_at(w, 115, 111));
    CHECK(w.islands()[0]->claims.empty());
    CHECK(w.check_invariants() == "");
    // Its next visit drops the target (no cut, no wait there).
    w.advance(2);
    CHECK(f.stats.forestry.drops == 1);
    CHECK(fig_of(w, id).harvest->step != sim::HarvestStep::Cut);
    for (int i = 0; i < 10000; ++i) {
        w.advance(2);
    }
    CHECK(f.stats.forestry.cuts == 0);
    CHECK(f.stats.forestry.cuts_failed == 0);
    CHECK(w.check_invariants() == "");
}

TEST_CASE("harvest lifecycle: an old job never clears a newer worker's claim (enhanced)") {
    // A state that the enhanced rules reach after an invalidation and a re-claim: worker A still
    // targets T without an entry (E-iii) while worker B owns T. A's next visit fails `valid` and
    // drops T without touching B's bit or entry; faithful has no owner: the shared target is an
    // accepted loaded state, and the first arrival clears the one bit for both (the one-bit model).
    for (const bool hygiene : {true, false}) {
        INFO("hygiene " << hygiene);
        sim::Rules rules;
        rules.forestry_claim_hygiene = hygiene;
        Forest f = make_forest({{115, 111}, {111, 115}}, kTree, rules);
        sim::World& w = f.w;
        w.set_transport_stats(&f.stats);
        REQUIRE(tick_until(w, [&] { return workers(w).size() == 2; }, 3000));
        REQUIRE(tick_until(
            w,
            [&] {
                const auto v = workers(w);
                return v.size() == 2 && v[1]->harvest->step == sim::HarvestStep::ToTree;
            },
            400));
        auto ws = workers(w);
        REQUIRE(ws.size() == 2);
        const bool first_has_t = ws[0]->harvest->target == sim::TileXY{115, 111};
        sim::Figure& a = first_has_t ? *ws[0] : *ws[1];
        sim::Figure& b = first_has_t ? *ws[1] : *ws[0];
        REQUIRE(a.harvest->target == sim::TileXY{115, 111});
        REQUIRE(b.harvest->step == sim::HarvestStep::ToTree);
        // Make B target T too (the re-claim after an invalidation); A keeps T but loses the entry.
        sim::Island& is = *sim::WorldTestAccess::islands(w)[0];
        const sim::TileXY b_old = b.harvest->target;
        const sim::Slot other = is.tile_index(b.harvest->target.x, b.harvest->target.y);
        is.tiles.set_aux(other, is.tiles.aux(other) & ~sim::kTileClaim);
        b.harvest->target = sim::TileXY{115, 111};
        if (hygiene) {
            is.claims.clear();
            is.claims[sim::TileXY{115, 111}] = b.id;
        }
        REQUIRE(w.check_invariants() == "");
        const sim::FigureId aid = a.id, bid = b.id;
        const sim::TileXY o = b_old;
        // Until A's next visit (the registry visits a figure about every 50 one-tick advances).
        const std::uint16_t a_stamp = a.stamp;
        REQUIRE(tick_until(w, [&] { return fig_of(w, aid).stamp != a_stamp; }, 200));
        if (hygiene) {
            // A's revalidation fails (owner B): it drops T without touching B's bit or entry.
            CHECK(f.stats.forestry.drops == 1);
            CHECK(claimed_at(w, 115, 111));
            CHECK(w.islands()[0]->claims.at(sim::TileXY{115, 111}) == bid);
            CHECK(fig_of(w, aid).harvest->target != sim::TileXY{115, 111});
        }
        // Both trees end cut exactly once; every credit is a successful cut. Faithful: both reach
        // T, the second arrival's cut finds the sapling and fails (no credit); then it takes the
        // other tree. Enhanced: A took the other tree after its drop.
        REQUIRE(tick_until(w, [&] { return f.stats.forestry.cuts >= 2; }, 30000));
        CHECK(object_at(w, 115, 111) == kSapling.v);
        CHECK(object_at(w, o.x, o.y) == kSapling.v);
        CHECK(f.stats.forestry.cuts == 2);
        CHECK(f.stats.forestry.raw_credited_88 == 2 * 256);
        CHECK(f.stats.forestry.cuts_failed == (hygiene ? 0U : 1U));
        CHECK(w.check_invariants() == "");
    }
}

TEST_CASE("harvest lifecycle: save and reload in every step resume like the uninterrupted run") {
    for (const sim::HarvestStep at :
         {sim::HarvestStep::ToTree, sim::HarvestStep::Cut, sim::HarvestStep::ToHome}) {
        INFO("step " << static_cast<int>(at));
        Forest f = make_forest({{115, 111}});
        sim::World& w = f.w;
        const sim::FigureId id = first_worker(w);
        REQUIRE(tick_until(w, [&] { return fig_of(w, id).harvest->step == at; }, 12000));
        auto loaded = sim::World::load(w.save(), f.types);
        REQUIRE(loaded.ok());
        sim::World r = std::move(loaded).value();
        CHECK(r.state_hash() == w.state_hash());
        for (int i = 0; i < 1500; ++i) { // 15 000 ticks in 10-tick steps: through the delivery
            w.advance(20);
            r.advance(20);
        }
        CHECK(r.state_hash() == w.state_hash());
        CHECK(r.save() == w.save());
        // The worker has ended (and may be destroyed by phase C already, 7500 ticks after its end).
        const sim::Figure* g = sim::find_figure(w.figures(), id);
        CHECK((g == nullptr || g->expiry_stamp.has_value()));
    }
}

TEST_CASE("harvest lifecycle: end, counter release once, expiry, and a switched-off farm") {
    Forest f = make_forest({{115, 111}});
    sim::World& w = f.w;
    w.set_transport_stats(&f.stats);
    const sim::FigureId id = first_worker(w);
    // Switch the farm off: the worker already out finishes and delivers; no new send (no draw).
    forester(w).switched_on = false;
    std::uint32_t draws = 0;
    for (int i = 0; i < 9000; ++i) { // 18 000 ticks: through the delivery
        w.advance(4);
        draws += draws_at(w.last_output(), sim::DrawSite::FarmWorkerDispatch);
        REQUIRE(w.check_invariants() == "");
    }
    CHECK(draws == 0);
    CHECK(f.stats.forestry.raw_delivered_88 == 256);
    CHECK(forester(w).workers_out == 0); // released once, at its end
    const sim::Figure* g = sim::find_figure(w.figures(), id);
    // Phase C destroys it 7500 ticks after its end (12 000 ticks have passed): gone or still
    // waiting.
    if (g != nullptr) {
        CHECK(g->expiry_stamp.has_value());
    }
    for (int i = 0; i < 4000; ++i) {
        w.advance(4);
    }
    CHECK(sim::find_figure(w.figures(), id) == nullptr);
    CHECK(forester(w).workers_out == 0);
}
