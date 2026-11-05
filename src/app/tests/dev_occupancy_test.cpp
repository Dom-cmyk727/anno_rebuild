// The developer tool's occupancy knowledge against edits it must not pay for (W4.4 round ee review,
// R4): the window follows the takes of the picture's batches incrementally, so an edit on another
// island does not touch the selected city's index, a disabled tool does no work, and a local edit
// updates the index by its own deltas. The first cases run lan5 through the real runner,
// ScenePicture and PlayFollower with the window's own wiring (attach_occupancy / ensure) and
// compare the index with a fresh scan of the picture's list; the last measures the costs. Skips
// the lan5 cases when ANNO1503_GAME_DIR is unset.
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/dev_feedback.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/play.hpp>
#include <anno/app/play_follow.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/assets/island.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/iso.hpp>
#include <anno/sim/types.hpp>
#include <anno/test/dev_lan5.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;

namespace {

/// Another player's city, its owner, its island's origin and size, and a 2 x 2 free spot of it
/// (World::building_at is the sim's own lookup).
struct OtherCity {
    sim::CityRef city;
    sim::PlayerId owner;
    sim::TileXY free_at;
    sim::TypeId type;
};

std::optional<OtherCity> other_city(const anno::test::Lan5& l, sim::TypeId type) {
    for (const auto& island : l.play.world.islands()) {
        if (!island) {
            continue;
        }
        for (const auto& c : island->cities) {
            if (!c || c->owner == app::developer_player() ||
                island->index == l.setup.city->island) {
                continue;
            }
            const sim::CityRef ref{island->index, c->number};
            for (std::int32_t y = 2; y + 4 <= static_cast<std::int32_t>(island->tiles.height());
                 ++y) {
                for (std::int32_t x = 2; x + 4 <= static_cast<std::int32_t>(island->tiles.width());
                     ++x) {
                    const sim::TileXY t{island->tiles.x0() + x, island->tiles.y0() + y};
                    if (l.play.world.building_at(ref, t.x, t.y) == nullptr) {
                        return OtherCity{ref, c->owner, t, type};
                    }
                }
            }
        }
    }
    return std::nullopt;
}

sim::Command build_cmd(sim::CityRef city, sim::PlayerId issuer, sim::TypeId type, sim::TileXY at) {
    sim::BuildCmd b;
    b.city = city;
    b.placements.push_back(sim::PlacementWire{type, at.x, at.y, 0, 0, 0});
    sim::Command c;
    c.issuer = issuer;
    c.body = std::move(b);
    return c;
}

sim::Command demolish_cmd(sim::CityRef city, sim::PlayerId issuer, sim::TileXY at) {
    sim::Command c;
    c.issuer = issuer;
    c.body = sim::DemolishCmd{city, {at}};
    return c;
}

/// Whether the index agrees with a fresh scan of the picture's whole list on every tile of the
/// city's island.
bool agrees_with_scan(const app::PlacementKnowledge& knows, const app::ScenePicture& picture,
                      const app::DevToolSetup& setup) {
    app::PlacementKnowledge fresh(setup);
    fresh.update(picture.buildings(), 1, *picture.scene());
    for (std::uint32_t y = 0; y < setup.island.h; ++y) {
        for (std::uint32_t x = 0; x < setup.island.w; ++x) {
            const sim::TileXY t{setup.island.x0 + static_cast<std::int32_t>(x),
                                setup.island.y0 + static_cast<std::int32_t>(y)};
            if (knows.occupied(t) != fresh.occupied(t)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

TEST_CASE("occupancy knowledge: an unrelated island's edit, a local build and demolish, a "
          "disabled spell and a resync, through the window's own wiring (game files)" *
          doctest::test_suite("dev_occupancy")) {
    auto l = anno::test::lan5();
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::optional<sim::TypeId> plain;
    for (const sim::TypeId t : l->setup.types) {
        const sim::BuildingType* type = l->setup.registry->find(t);
        if (!plain && type != nullptr && type->cost_goods.empty() && type->width == 2 &&
            type->height == 2 && (type->flags_word & sim::kRoadFlag) == 0) {
            plain = t;
        }
    }
    if (!plain) { // any plain building will do: take the first without goods or road flag
        for (const sim::TypeId t : l->setup.types) {
            const sim::BuildingType* type = l->setup.registry->find(t);
            if (!plain && type != nullptr && type->cost_goods.empty() &&
                (type->flags_word & sim::kRoadFlag) == 0) {
                plain = t;
            }
        }
    }
    REQUIRE(plain.has_value());
    const auto other = other_city(*l, *plain);
    REQUIRE(other.has_value());
    const auto free = anno::test::free_tile(*l);
    REQUIRE(free.has_value());
    app::DevTool tool = anno::test::tool_at(*l, *plain, *free);
    const app::DevToolSetup setup = l->setup;
    std::vector<bool> classes(app::kPictureAnlageIds, false);
    for (std::size_t id = 0; id < classes.size(); ++id) {
        classes[id] = app::anlage_is_building(l->play.world, static_cast<std::uint16_t>(id));
    }
    app::ScenePicture picture(
        l->scene, l->buildings,
        [&classes](std::uint16_t id) { return id < classes.size() && classes[id]; }, 1);
    render::FrameRenderer frames(2);
    app::PlayRunner runner(std::move(l->play.world));
    app::PlayFollower follower(picture, runner);
    app::PlacementKnowledge knows(setup);
    app::attach_occupancy(follower, knows, tool);
    const sim::PlayerId dev = app::developer_player();
    const auto step = [&](std::vector<sim::Command> commands) {
        for (sim::Command& c : commands) {
            static_cast<void>(runner.post_command(std::move(c)));
        }
        runner.run_n(1);
        follower.follow(frames);
    };

    // The first frame seeds from the picture's per-island list, once.
    follower.follow(frames);
    CHECK(knows.ensure(picture, *picture.scene(), tool.enabled()));
    CHECK(knows.rebuilds() == 1);
    CHECK_FALSE(knows.ensure(picture, *picture.scene(), tool.enabled())); // current: O(1)
    static_cast<void>(knows.take_changed());
    CHECK(agrees_with_scan(knows, picture, setup));

    // An edit on another island: its deltas are compared and ignored, nothing is rebuilt.
    step({build_cmd(other->city, other->owner, other->type, other->free_at)});
    REQUIRE(picture.buildings_version() > 0); // the picture took the batch
    CHECK(knows.deltas_applied() == 0);
    CHECK(knows.rebuilds() == 1);
    CHECK_FALSE(knows.take_changed());
    CHECK(agrees_with_scan(knows, picture, setup));

    // A local build updates the index by its own deltas, no rebuild.
    step({build_cmd(*setup.city, dev, *plain, *free)});
    CHECK(knows.deltas_applied() > 0);
    CHECK(knows.rebuilds() == 1);
    CHECK(knows.take_changed());
    CHECK(knows.occupied(*free));
    CHECK(agrees_with_scan(knows, picture, setup));
    // The feedback sees it: the same tile is now a known refusal.
    CHECK(knows.check(tool, nullptr).refused);

    // A local demolish removes exactly those tiles.
    const std::size_t after_build = knows.deltas_applied();
    step({demolish_cmd(*setup.city, dev, *free)});
    CHECK(knows.deltas_applied() > after_build);
    CHECK_FALSE(knows.occupied(*free));
    CHECK(agrees_with_scan(knows, picture, setup));
    CHECK_FALSE(knows.check(tool, nullptr).refused);

    // A disabled tool does no work: its takes are not followed and nothing is rebuilt; re-enabled,
    // one reseed from the picture's list brings the index up to date (the spell's local edit).
    static_cast<void>(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr));
    REQUIRE_FALSE(tool.enabled());
    const std::size_t deltas_before = knows.deltas_applied();
    step({build_cmd(*setup.city, dev, *plain, *free)});
    CHECK(knows.deltas_applied() == deltas_before);
    CHECK_FALSE(knows.ensure(picture, *picture.scene(), tool.enabled()));
    CHECK(knows.rebuilds() == 1);
    static_cast<void>(tool.handle({app::ToolInput::Kind::Toggle, 0, 0}, nullptr));
    REQUIRE(tool.enabled());
    CHECK(knows.ensure(picture, *picture.scene(), tool.enabled()));
    CHECK(knows.rebuilds() == 2);
    CHECK(knows.occupied(*free));
    CHECK(agrees_with_scan(knows, picture, setup));

    // A resync batch (the mailbox could not hold the turns the window did not take) reseeds from
    // its list. The net local change is not zero: the building at `free` is demolished and stays
    // demolished, so an index that ignored the resync (or applied nothing) still shows it.
    // The observer here is the window's wiring plus a spy that records the batches it was given.
    REQUIRE(knows.occupied(*free));
    std::size_t resyncs = 0, normals = 0;
    follower.set_batch_observer(
        [&](const std::vector<std::shared_ptr<const app::BuildingBatch>>& batches) {
            for (const auto& b : batches) {
                ++(b->resync ? resyncs : normals);
            }
            knows.on_batches(batches, *picture.scene(), tool.enabled());
        });
    runner.set_batch_bound(1);
    static_cast<void>(runner.post_command(demolish_cmd(*setup.city, dev, *free)));
    runner.run_n(1);
    static_cast<void>(runner.post_command(build_cmd(other->city, other->owner, other->type,
                                                    {other->free_at.x + 2, other->free_at.y})));
    runner.run_n(1);
    const std::size_t reseeds = knows.rebuilds(), deltas = knows.deltas_applied();
    follower.follow(frames);
    CHECK(resyncs == 1); // the second push collapsed the first into one resync batch
    CHECK(normals == 0);
    CHECK(knows.rebuilds() == reseeds + 1);
    CHECK(knows.deltas_applied() == deltas); // reseeded, not applied as deltas
    CHECK_FALSE(knows.occupied(*free));      // the net change reached the index
    CHECK(agrees_with_scan(knows, picture, setup));
    // The same take without the resync branch would leave `free` occupied: a valid control is the
    // lines above (they fail if on_batches ignores a resync batch).
    follower.set_batch_observer({});

    // A type missing from the registry indexes as one tile (the sim's registry is the source;
    // the window keeps going on an unknown id).
    app::PlacementKnowledge unknown(setup);
    const app::SceneBuilding stray{
        picture.scene()->islands[l->island].number, 0xFFFFFF, 3, 3, 0, 0};
    unknown.update({stray}, 1, *picture.scene());
    CHECK(unknown.occupied({setup.island.x0 + 3, setup.island.y0 + 3}));
    CHECK_FALSE(unknown.occupied({setup.island.x0 + 4, setup.island.y0 + 3}));
    runner.stop();
}

// ---- The costs, as the local city and the unrelated world grow (a measurement) ----------------
// Synthetic: a flat scene of two islands (the city's, 400 x 400 tiles, number 0, and a neighbour,
// number 1), a registry with one 2 x 2 type, and buildings placed on a grid of non-overlapping 2 x
// 2 blocks. The index is the real PlacementKnowledge; the batches are built by hand, the shape the
// runner publishes (removed then added). Nothing is simulated.

namespace {

constexpr std::int32_t kSynX0 = 1000, kSynY0 = 500;
constexpr std::uint32_t kSynSide = 400;
constexpr sim::TypeId kBlock{601};

std::shared_ptr<const render::Scene> two_islands() {
    auto bytes = std::make_shared<std::vector<std::vector<std::uint8_t>>>();
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    bytes->push_back(std::vector<std::uint8_t>(64, 0));
    std::vector<anno::formats::insel::Island> list(2);
    for (std::size_t k = 0; k < 2; ++k) {
        list[k].head = (*bytes)[k];
        list[k].number = static_cast<std::uint32_t>(k);
        list[k].x = static_cast<std::uint16_t>(kSynX0 + static_cast<std::int32_t>(k) * 450);
        list[k].y = static_cast<std::uint16_t>(kSynY0);
        list[k].width = kSynSide;
        list[k].height = kSynSide;
    }
    return render::Scene::create("occupancy", std::move(list),
                                 anno::assets::island::GroundTextures{},
                                 anno::formats::anlagen::AnlagenFile{}, render::kHeightLift, bytes);
}

app::DevToolSetup synthetic_setup() {
    sim::TypeRegistry::Builder b;
    sim::BuildingType t;
    t.id = kBlock;
    t.prodtype = 161;
    t.width = 2;
    t.height = 2;
    b.add(t);
    auto built = b.build();
    REQUIRE_MESSAGE(built.ok(), (built.ok() ? std::string() : built.error()));
    app::DevToolSetup s;
    s.registry = built.value();
    s.types = {kBlock};
    s.city = sim::CityRef{sim::IslandId{0}, sim::CityNo{0}};
    s.island = app::TileRect{kSynX0, kSynY0, kSynSide, kSynSide};
    s.issuer = sim::PlayerId{1};
    return s;
}

/// `n` non-overlapping 2 x 2 blocks of `island` on a grid (200 per row of the 400-tile island).
std::vector<app::SceneBuilding> grid(std::uint32_t island, std::size_t n) {
    std::vector<app::SceneBuilding> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(app::SceneBuilding{island, kBlock.v,
                                         static_cast<std::uint16_t>((i % 200) * 2),
                                         static_cast<std::uint16_t>((i / 200) * 2), 0, 0});
    }
    return out;
}

std::shared_ptr<const app::BuildingBatch>
delta_batch(std::uint64_t turn, const app::SceneBuilding& b, app::BuildingDelta::Kind kind) {
    auto batch = std::make_shared<app::BuildingBatch>();
    batch->turn = turn;
    batch->deltas.push_back(app::BuildingDelta{kind, b});
    return batch;
}

} // namespace

TEST_CASE("occupancy knowledge: an unrelated edit costs the same at any city size and world size, "
          "a local edit costs its own deltas (a measurement, synthetic)" *
          doctest::test_suite("dev_occupancy")) {
    const auto scene = two_islands();
    const app::DevToolSetup setup = synthetic_setup();
    struct Row {
        std::size_t local, unrelated;
        double seed_ms, old_unrelated_ms, unrelated_us, local_add_us;
    };
    std::vector<Row> rows;
    using clock = std::chrono::steady_clock;
    const auto ms = [](clock::time_point a, clock::time_point b) {
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    for (const std::size_t unrelated : {std::size_t{0}, std::size_t{100000}}) {
        for (const std::size_t local : {std::size_t{1000}, std::size_t{5000}, std::size_t{16000}}) {
            std::vector<app::SceneBuilding> world = grid(0, local);
            const std::vector<app::SceneBuilding> others = grid(1, unrelated);
            world.insert(world.end(), others.begin(), others.end());
            app::PlacementKnowledge knows(setup);
            const auto s0 = clock::now();
            knows.update(world, 1, *scene);
            const auto s1 = clock::now();
            REQUIRE(knows.rebuilds() == 1);

            // What the window did on an unrelated edit before: the global version changed, so the
            // whole list was scanned and the city's index rebuilt (a diagnostic, not the product).
            world.push_back(app::SceneBuilding{1, kBlock.v, 399, 399, 0, 0});
            app::PlacementKnowledge old(setup);
            old.update(world, 1, *scene);
            const auto o0 = clock::now();
            old.update(world, 2, *scene);
            const auto o1 = clock::now();

            // The product: the unrelated edit is one delta of the other island.
            const auto edit = delta_batch(1, world.back(), app::BuildingDelta::Kind::Added);
            constexpr int kReps = 2000;
            const auto u0 = clock::now();
            for (int i = 0; i < kReps; ++i) {
                knows.on_batches({edit}, *scene, true);
            }
            const auto u1 = clock::now();
            CHECK(knows.deltas_applied() == 0);
            CHECK(knows.rebuilds() == 1);

            // A local add and remove of the same block, kReps times: the deltas only.
            const app::SceneBuilding block{0, kBlock.v, 398, 398, 0, 0};
            const auto add = delta_batch(2, block, app::BuildingDelta::Kind::Added);
            const auto rem = delta_batch(3, block, app::BuildingDelta::Kind::Removed);
            const auto l0 = clock::now();
            for (int i = 0; i < kReps; ++i) {
                knows.on_batches({add}, *scene, true);
                knows.on_batches({rem}, *scene, true);
            }
            const auto l1 = clock::now();
            CHECK(knows.deltas_applied() == 2U * kReps);
            CHECK(knows.rebuilds() == 1);
            CHECK_FALSE(knows.occupied({kSynX0 + 398, kSynY0 + 398}));
            rows.push_back({local, unrelated, ms(s0, s1), ms(o0, o1), ms(u0, u1) * 1000.0 / kReps,
                            ms(l0, l1) * 1000.0 / (2.0 * kReps)});
        }
    }
    for (const Row& r : rows) {
        MESSAGE("local " << r.local << ", unrelated " << r.unrelated << ": seed " << r.seed_ms
                         << " ms, scan+rebuild per unrelated edit (before) " << r.old_unrelated_ms
                         << " ms, unrelated edit now " << r.unrelated_us << " us, local delta "
                         << r.local_add_us << " us");
    }
}

TEST_CASE("occupancy knowledge: overlapping buildings are counted, a removal of an uncounted "
          "tile changes nothing, an unknown type indexes as one tile, and a resync reseeds "
          "(synthetic)" *
          doctest::test_suite("dev_occupancy")) {
    const auto scene = two_islands();
    const app::DevToolSetup setup = synthetic_setup();
    app::PlacementKnowledge knows(setup);
    knows.update({}, 1, *scene);
    const auto tile = [](std::int32_t lx, std::int32_t ly) {
        return sim::TileXY{kSynX0 + lx, kSynY0 + ly};
    };
    const app::SceneBuilding a{0, kBlock.v, 10, 10, 0, 0}; // 2 x 2 at local (10..11, 10..11)
    const app::SceneBuilding b{0, kBlock.v, 11, 11, 0, 0}; // overlaps `a` on local (11, 11)
    using Kind = app::BuildingDelta::Kind;
    knows.on_batches({delta_batch(1, a, Kind::Added), delta_batch(2, b, Kind::Added)}, *scene,
                     true);
    CHECK(knows.occupied(tile(11, 11)));
    CHECK(knows.occupied(tile(12, 12)));
    // Removing `a` leaves the shared tile occupied by `b`, and frees `a`'s own tiles.
    knows.on_batches({delta_batch(3, a, Kind::Removed)}, *scene, true);
    CHECK(knows.occupied(tile(11, 11)));
    CHECK_FALSE(knows.occupied(tile(10, 10)));
    // A removal of something never counted (a drift the picture would also drop) changes no tile
    // that another building holds, and never goes below zero.
    const std::size_t before = knows.deltas_applied();
    knows.on_batches({delta_batch(4, app::SceneBuilding{0, kBlock.v, 50, 50, 0, 0}, Kind::Removed)},
                     *scene, true);
    CHECK(knows.deltas_applied() == before + 1);
    CHECK_FALSE(knows.occupied(tile(50, 50)));
    CHECK(knows.occupied(tile(11, 11)));
    // An id the registry does not have indexes as one tile, through the delta path too.
    knows.on_batches({delta_batch(5, app::SceneBuilding{0, 0xABCDE, 20, 20, 0, 0}, Kind::Added)},
                     *scene, true);
    CHECK(knows.occupied(tile(20, 20)));
    CHECK_FALSE(knows.occupied(tile(21, 20)));
    // A resync batch replaces everything with its list (island 0's buildings only count).
    auto resync = std::make_shared<app::BuildingBatch>();
    resync->turn = 9;
    resync->resync = true;
    resync->full = {app::SceneBuilding{0, kBlock.v, 100, 100, 0, 0},
                    app::SceneBuilding{1, kBlock.v, 5, 5, 0, 0}};
    knows.on_batches({resync}, *scene, true);
    CHECK(knows.occupied(tile(100, 100)));
    CHECK_FALSE(knows.occupied(tile(11, 11)));
    CHECK_FALSE(knows.occupied(tile(5, 5))); // island 1's building is not the city's
    CHECK(knows.rebuilds() == 2);
    // While disabled the takes are not followed, and the index is stale until ensure() reseeds it.
    CHECK_FALSE(knows.on_batches({delta_batch(10, a, Kind::Added)}, *scene, false));
    CHECK_FALSE(knows.occupied(tile(10, 10)));
    CHECK_FALSE(knows.on_batches({delta_batch(11, a, Kind::Added)}, *scene, true)); // stale
}

TEST_CASE("occupancy knowledge: ScenePicture::apply of one unrelated delta, for reference "
          "(a measurement, synthetic)" *
          doctest::test_suite("dev_occupancy")) {
    // What the window thread still pays for an edit on another island is the picture's own apply
    // (a sorted insert into the building list and the per-island index), which is not occupancy
    // work and is outside this repair; it is timed here so the gap is visible, not claimed away.
    using clock = std::chrono::steady_clock;
    for (const std::size_t others : {std::size_t{0}, std::size_t{50000}}) {
        const auto scene = two_islands();
        std::vector<app::SceneBuilding> world = grid(0, 1000);
        const std::vector<app::SceneBuilding> rest = grid(1, others);
        world.insert(world.end(), rest.begin(), rest.end());
        app::sort_buildings(world);
        app::ScenePicture picture(scene, world, [](std::uint16_t) { return true; }, 1);
        render::FrameRenderer frames(1);
        picture.wait_idle();
        const auto edit = delta_batch(1, app::SceneBuilding{1, kBlock.v, 399, 399, 0, 0},
                                      app::BuildingDelta::Kind::Added);
        const auto t0 = clock::now();
        picture.apply({edit});
        const auto t1 = clock::now();
        picture.wait_idle();
        picture.commit(frames);
        MESSAGE("picture.apply of one delta on another island, "
                << others << " buildings there: "
                << std::chrono::duration<double, std::milli>(t1 - t0).count()
                << " ms (not occupancy work)");
    }
}
