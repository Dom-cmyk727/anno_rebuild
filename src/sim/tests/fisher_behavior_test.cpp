// FISHER-1b: mode 3 and the synthetic bay. Evidence: docs/re/fisher.md §§2–5,
// 2026-11-14 correction; original planar rules, no invented vertical continuation.
#include <algorithm>
#include <array>

#include <doctest/doctest.h>

#include <anno/sim/fisher.hpp>
#include <anno/sim/harvest.hpp>
#include <anno/sim/walk_cells.hpp>
#include <anno/sim/walk_search.hpp>

#include "fisher_helpers.hpp"
namespace sim = anno::sim;
using namespace harvest_test;
namespace {
sim::Building& home(sim::World& w) {
    for (auto& list : sim::WorldTestAccess::islands(w)[0]->cities[0]->index.lists)
        for (auto& b : list.slots)
            if (b && b->type == kBayFishery)
                return *b;
    FAIL("missing fishery");
    return *sim::WorldTestAccess::islands(w)[0]->cities[0]->index.lists[0].slots[0];
}
sim::Farm& farm(sim::World& w) {
    return std::get<sim::Farm>(home(w).payload);
}
sim::Figure* fisher(sim::World& w) {
    for (auto& f : sim::WorldTestAccess::figures(w).list.slots)
        if (f && f->kind == kFisher && !f->expiry_stamp)
            return &*f;
    return nullptr;
}
template <class F> bool until(sim::World& w, F pred, int bound = 4000) {
    for (int n = 0; n < bound && !pred(); ++n)
        w.advance(2);
    return pred();
}
std::uint32_t object(const sim::World& w, int x, int y) {
    const auto& is = *w.islands()[0];
    return is.tiles.object(is.tile_index(x, y));
}
bool claimed(const sim::World& w, int x, int y) {
    const auto& is = *w.islands()[0];
    return is.tiles.claimed(is.tile_index(x, y));
}
} // namespace
TEST_CASE(
    "fisher behavior: mode 3 matches primitives and default versus explicit Land selects equally") {
    auto types = fishery_registry();
    auto w = create_fishery(fishery_rig(), types, true);
    const auto& is = *w.islands()[0];
    const auto& tables = *types->walk_tables();
    sim::WalkCells sea(is, *types, tables, is.rect(), 4, {}, sim::WalkCellMode::FisherSea);
    sim::WalkCells land(is, *types, tables, is.rect());
    sim::WalkCells explicit_land(is, *types, tables, is.rect(), 4, {}, sim::WalkCellMode::Land);
    for (std::uint32_t y = 0; y < is.tiles.height(); ++y)
        for (std::uint32_t x = 0; x < is.tiles.width(); ++x) {
            const sim::TileXY p{100 + static_cast<int>(x), 100 + static_cast<int>(y)};
            const auto c =
                sim::fisher_cell_at(is.terrain->height, is.tiles.width(), is.tiles.height(), x, y);
            REQUIRE(c);
            const auto raw = sea.record(p);
            const auto cell = sea.get(p);
            CHECK(raw.mask9 == c->blocked_mask);
            CHECK(raw.code == (c->walkable_water ? 0 : 7));
            CHECK(cell.low == (c->walkable_water ? 0xfff : 0));
            CHECK(cell.high == (c->walkable_water ? 0 : 7));
            CHECK(land.get(p) == explicit_land.get(p));
            CHECK(land.record(p).mask9 == sim::walk_record(is, *types, p).mask9);
        }
    // This checks default/explicit mode selection equivalence, not an independent mode-0 oracle.
    // The unchanged full pins remain the independent historical mode-0 regression check.
    const std::array starts{sim::WalkPoint{{105, 105}, 1, 1}};
    const std::array goals{sim::WalkPoint{{108, 105}, 1, 1}};
    const auto first = [](const sim::SearchVisit&) {
        return sim::SearchDecision{sim::GoalAction::Stop, true};
    };
    const auto a = sim::search(land, tables, 2, starts, goals, first);
    const auto b = sim::search(explicit_land, tables, 2, starts, goals, first);
    REQUIRE(a.selected);
    REQUIRE(b.selected);
    CHECK(a.route == b.route);
    CHECK(a.cost == b.cost);
    CHECK(sea.complete());
    CHECK(sim::harvest_search_limit(45, true) == 20);
    CHECK(sim::farm_figure_route(45) == sim::FarmFigureRoute::Unsupported);
}
TEST_CASE(
    "fisher behavior: rotated first entrance, input goods and identical eligible-attempt draw") {
    auto types = fishery_registry();
    auto rig = fishery_rig(true, true, 1);
    auto on = create_fishery(rig, types, true), off = create_fishery(rig, types, false);
    bool sent = false;
    for (int n = 0; n < 2000 && !sent; ++n) {
        const auto before = on.rng().cursor();
        on.advance(2);
        off.advance(2);
        CHECK(on.rng().cursor() == off.rng().cursor());
        if (auto* f = fisher(on)) {
            CHECK(on.rng().cursor() != before);
            // D AnnoGame 0x100084c0: rotation 1 maps (6,9) to (9,18-6)
            // sixths. At (110,110), that is (111.5,112), Q24.8 (28544,28672).
            CHECK(f->pos == sim::FixedPos{28544, 28672});
            CHECK(f->pos != sim::FixedPos{28544, 28416}); // second entrance: (9,18-12)
            CHECK(f->cargo_goods == 71);
            CHECK(f->job == 45);
            REQUIRE(f->harvest);
            CHECK(farm(on).workers_out == 1);
            CHECK(farm(off).workers_out == 0);
            CHECK(sim::validate_figures(on.figures(), on.ids(), true).empty());
            CHECK_FALSE(sim::validate_figures(on.figures(), on.ids(), false).empty());
            CHECK(fisher(off) == nullptr);
            sent = true;
        }
    }
    REQUIRE(sent);
}
TEST_CASE("fisher behavior: water fish claim wait three cuts return and food and dry control never "
          "claimed") {
    auto types = fishery_registry();
    auto w = create_fishery(fishery_rig(), types, true);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
    REQUIRE(until(w, [&] {
        auto* f = fisher(w);
        return f && f->harvest->step == sim::HarvestStep::Cut && f->harvest->wait > 1;
    }));
    auto* f = fisher(w);
    if (!f || !f->harvest) {
        FAIL("missing active fisher state");
        return;
    }
    const auto target = f->harvest->target;
    const auto wait = f->harvest->wait;
    CHECK(claimed(w, target.x, target.y));
    CHECK(object(w, target.x, target.y) == 11);
    const auto cursor = w.rng().cursor();
    w.advance(0);
    CHECK(f->harvest->wait == wait);
    w.advance(2);
    CHECK(f->harvest->wait == wait - 1);
    CHECK(w.rng().cursor() == cursor);
    REQUIRE(until(w, [&] {
        CHECK_FALSE(claimed(w, 114, 116));
        return stats.forestry.raw_delivered_88 == 768;
    }));
    CHECK(stats.forestry.cuts == 3);
    CHECK(stats.forestry.raw_credited_88 == 768);
    for (int x = 115; x <= 117; ++x) {
        CHECK(object(w, x, 112) == 10);
        CHECK_FALSE(claimed(w, x, 112));
    }
    CHECK(object(w, 114, 116) == 11);
    CHECK(farm(w).workers_out == 0);
    CHECK(farm(w).raw == 768);
    REQUIRE(until(w, [&] { return farm(w).out > 0; }));
    CHECK(w.check_invariants().empty());
}
TEST_CASE("fisher behavior: mid-cut save reload and deterministic trip continuation draw no body "
          "values") {
    auto types = fishery_registry();
    auto w = create_fishery(fishery_rig(), types, true);
    REQUIRE(until(w, [&] {
        auto* f = fisher(w);
        return f && f->harvest->step == sim::HarvestStep::Cut;
    }));
    auto restored = sim::World::load(w.save(), types);
    REQUIRE_MESSAGE(restored.ok(), (restored.ok() ? "" : restored.error()));
    auto r = std::move(restored).value();
    const auto before = w.rng().cursor();
    for (int n = 0; n < 4000 && farm(w).workers_out != 0; ++n) {
        w.advance(2);
        r.advance(2);
        CHECK(w.state_hash() == r.state_hash());
    }
    REQUIRE(farm(w).workers_out == 0);
    CHECK(farm(w).raw == 768);
    CHECK(farm(r).workers_out == 0);
    CHECK(w.rng().cursor() == before);
    CHECK(w.save() == r.save());
    CHECK(w.check_invariants().empty());
}
TEST_CASE(
    "fisher behavior: no usable fish and disconnected sea release the worker without credit") {
    auto types = fishery_registry();
    for (bool isolated : {false, true}) {
        auto rig = fishery_rig(false, !isolated);
        if (isolated) {
            // One open fish cell, surrounded by cells whose means are at least 64.
            auto& is = rig.spec.islands[0];
            std::vector<std::uint8_t> h(is.w * is.h, 128);
            for (unsigned y = 12; y <= 13; ++y)
                for (unsigned x = 15; x <= 16; ++x)
                    h[y * is.w + x] = 24;
            is.terrain = sim::make_terrain(std::move(h), {}, {});
            rig.object(115, 112, sim::TypeId{11});
        }
        auto w = create_fishery(rig, types, true);
        sim::TransportStats stats;
        w.set_transport_stats(&stats);
        REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
        REQUIRE(until(w, [&] { return farm(w).workers_out == 0; }));
        CHECK(stats.forestry.cuts == 0);
        CHECK(farm(w).raw == 0);
        CHECK(object(w, 114, 116) == 11);
        CHECK_FALSE(claimed(w, 114, 116));
        if (isolated)
            CHECK_FALSE(claimed(w, 115, 112));
        else {
            CHECK(stats.forestry.search_candidates == 1);
            CHECK(stats.forestry.search_popped > 0);
        }
        CHECK(w.check_invariants().empty());
    }
}
TEST_CASE("fisher behavior: receipt is an unclipped u16 addition and releases the worker") {
    auto types = fishery_registry();
    auto w = create_fishery(fishery_rig(), types, true);
    REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
    auto* f = fisher(w);
    if (!f || !f->harvest) {
        FAIL("missing active fisher state");
        return;
    }
    const auto id = f->id;
    f->harvest->start_delay = 0;
    f->harvest->step = sim::HarvestStep::ToHome;
    f->harvest->route.clear();
    f->harvest->next = 0;
    f->harvest->along = 0;
    f->cargo_88 = 768;
    f->pos = sim::entrance_position(home(w), *types->find(kBayFishery), 0);
    farm(w).raw = 65500;
    REQUIRE(until(
        w,
        [&] {
            const auto* g = sim::find_figure(w.figures(), id);
            return g && g->expiry_stamp.has_value();
        },
        20));
    CHECK(farm(w).raw == 732);
    CHECK(farm(w).workers_out == 0);
    CHECK(w.check_invariants().empty());
}
TEST_CASE("fisher behavior: worker occupancy permits on-off draw trajectories to diverge") {
    auto types = fishery_registry(1000);
    auto rig = fishery_rig();
    auto on = create_fishery(rig, types, true), off = create_fishery(rig, types, false);
    for (int n = 0; n < 1100; ++n) {
        on.advance(2);
        off.advance(2);
    }
    CHECK(farm(on).workers_out == 1);
    CHECK(farm(off).workers_out == 0);
    CHECK(on.rng().cursor() + 1 == off.rng().cursor());
}

TEST_CASE("fisher behavior: a SmallFarm home is refused before enabled receipt") {
    auto types = registry();
    auto rig = make_rig();
    rig.place(kFisherFarm, 110, 110, sim::Farm{});
    rig.place(kSmallFarm, 120, 120);
    auto w = create_fishery(rig, types, true);
    REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
    auto* f = fisher(w);
    if (!f || !f->harvest) {
        FAIL("missing active fisher state");
        return;
    }
    auto& is = *sim::WorldTestAccess::islands(w)[0];
    sim::Building* original = nullptr;
    sim::Building* small = nullptr;
    for (auto& list : is.cities[0]->index.lists)
        for (auto& b : list.slots)
            if (b) {
                if (b->id == f->home)
                    original = &*b;
                if (b->type == kSmallFarm)
                    small = &*b;
            }
    if (!original || !small) {
        FAIL("missing homes");
        return;
    }
    REQUIRE(w.check_invariants().empty());
    auto* old_counter = sim::figure_counter(original->payload, f->home_counter);
    auto* new_counter = sim::figure_counter(small->payload, f->home_counter);
    if (!old_counter || !new_counter) {
        FAIL("missing counters");
        return;
    }
    --*old_counter;
    ++*new_counter;
    f->home = small->id;
    CHECK(w.harvest_problem().find("fisher requires a Farm home") != std::string::npos);
    auto loaded = sim::World::load(w.save(), types);
    REQUIRE_FALSE(loaded.ok());
    CHECK(loaded.error().find("fisher requires a Farm home") != std::string::npos);
    // The body also fails closed for an unchecked in-memory misuse; no variant exception.
    f->harvest->start_delay = 0;
    f->harvest->step = sim::HarvestStep::ToHome;
    f->harvest->route.clear();
    f->harvest->next = 0;
    f->harvest->along = 0;
    f->cargo_88 = 768;
    const auto id = f->id;
    REQUIRE(until(
        w,
        [&] {
            const auto* g = sim::find_figure(w.figures(), id);
            return g && g->expiry_stamp.has_value();
        },
        20));
    CHECK(w.check_invariants().empty());
}

TEST_CASE("fisher behavior: truly no fish skips search before any node is popped") {
    auto types = fishery_registry();
    auto rig = fishery_rig(false);
    auto& objects = rig.spec.islands[0].objects;
    std::erase_if(objects, [](const auto& o) { return o.object == 11; });
    auto w = create_fishery(rig, types, true);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
    REQUIRE(until(w, [&] { return farm(w).workers_out == 0; }));
    CHECK(stats.forestry.search_candidates == 0);
    CHECK(stats.forestry.search_popped == 0);
    CHECK(stats.forestry.cuts == 0);
    CHECK(farm(w).raw == 0);
}

TEST_CASE(
    "fisher behavior: first land entrance reaches water and returns through the water entrance") {
    auto types = fishery_registry();
    auto rig = fishery_rig();
    auto& is = rig.spec.islands[0];
    auto heights = is.terrain->height;
    // First entrance (111,111.5): mean (1+63+1+63)/4 = 32, closed.
    // Second entrance (112,111.5): mean (63+0+63+0)/4 = 31, open.
    // West neighbour (110,111): mean (24+1+24+1)/4 = 12, open.
    for (unsigned y : {11U, 12U}) {
        heights[y * is.w + 11] = 1;
        heights[y * is.w + 12] = 63;
        heights[y * is.w + 13] = 0;
    }
    is.terrain = sim::make_terrain(std::move(heights), {}, {});
    auto w = create_fishery(rig, types, true);
    const auto& island = *w.islands()[0];
    sim::WalkCells sea(island, *types, *types->walk_tables(), island.rect(), 4, {},
                       sim::WalkCellMode::FisherSea);
    CHECK(sea.get({111, 111}).low == 0);
    CHECK(sea.get({110, 111}).low == 0xfff);
    CHECK(sea.get({112, 111}).low == 0xfff);
    sim::TransportStats stats;
    w.set_transport_stats(&stats);
    REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
    REQUIRE(until(w, [&] {
        const auto* f = fisher(w);
        return f && f->harvest->step == sim::HarvestStep::Cut;
    }));
    const auto target = fisher(w)->harvest->target;
    CHECK(sea.get(target).low == 0xfff);
    CHECK(claimed(w, target.x, target.y));
    CHECK(stats.forestry.search_popped > 0);
    REQUIRE(until(w, [&] { return stats.forestry.raw_delivered_88 == 768; }));
    CHECK(farm(w).workers_out == 0);
    CHECK(w.check_invariants().empty());
}

TEST_CASE("fisher behavior: two runs from create have equal hashes through a completed trip") {
    const auto types = fishery_registry();
    const auto rig = fishery_rig();
    auto a = create_fishery(rig, types, true), b = create_fishery(rig, types, true);
    CHECK(a.state_hash() == b.state_hash());
    bool sent = false, finished = false;
    for (int n = 0; n < 4000 && !finished; ++n) {
        a.advance(2);
        b.advance(2);
        CHECK(a.state_hash() == b.state_hash());
        sent = sent || farm(a).workers_out != 0;
        finished = sent && farm(a).workers_out == 0;
    }
    REQUIRE(finished);
    CHECK(farm(a).raw == 768);
    CHECK(a.save() == b.save());
}

TEST_CASE("fisher behavior: enabled harvest diagnostic lists job 0x2d") {
    auto types = fishery_registry();
    auto w = create_fishery(fishery_rig(), types, true);
    REQUIRE(until(w, [&] { return fisher(w) != nullptr; }));
    auto* f = fisher(w);
    REQUIRE(f != nullptr);
    f->job = 0xffff;
    CHECK(sim::validate_figures(w.figures(), w.ids(), true).find("fisher job 0x2d") !=
          std::string::npos);
    CHECK(sim::validate_figures(w.figures(), w.ids(), false).find("fisher job 0x2d") ==
          std::string::npos);
}
