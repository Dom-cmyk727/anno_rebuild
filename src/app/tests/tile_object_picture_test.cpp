// Runtime tile objects in the play picture (ROADS-0 / PICTURE-OBJECTS-1):
// (1) placing a land road reaches Visibility::Visible (never NoVisualChange), differs visually
//     from bare ground, respects rotation, equals a fresh reference scene, and demolishes back to
//     bare ground;
// (2) trees under a newly placed building or road vanish from the picture and do not reappear on
//     demolish (including direct tree demolition and collapsed mailbox resync).
// Skips when ANNO1503_GAME_DIR is unset.
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/app/play_follow.hpp>
#include <anno/app/png.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/app/visibility.hpp>
#include <anno/formats/insel.hpp>
#include <anno/render/frame.hpp>
#include <anno/sim/placement.hpp>
#include <anno/test/dev_lan5.hpp>

namespace app = anno::app;
namespace render = anno::render;
namespace sim = anno::sim;
using V = app::Visibility;

namespace {

constexpr sim::TypeId kLandRoad{2520};

template <class F> bool within(std::chrono::milliseconds limit, F done) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() > end) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

render::Rect view_around(const anno::test::Lan5& l, sim::TileXY tile, int w = 320, int h = 240) {
    const auto [sx, sy] = anno::test::tile_centre(l, tile);
    return render::Rect{sx - w / 2, sy - h / 2, w, h};
}

std::vector<std::uint32_t> render_settled(render::FrameRenderer& frames,
                                          const std::shared_ptr<const render::Scene>& scene,
                                          render::Rect view, std::size_t group) {
    std::vector<std::uint32_t> out(
        static_cast<std::size_t>(view.w) * static_cast<std::size_t>(view.h), 0);
    for (int round = 0; round < 200; ++round) {
        const auto s =
            frames.render(scene, view, group, 0, render::WaterStyle::Original, out.data());
        if (s.chunks_missing == 0 && !frames.busy()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    frames.render(scene, view, group, 0, render::WaterStyle::Original, out.data());
    return out;
}

V drive_until_settled(app::PlayFollower& follower, app::ScenePicture& picture,
                      render::FrameRenderer& frames, render::Rect view, std::size_t group,
                      std::uint64_t turn, std::vector<std::uint32_t>& out) {
    const auto fresh = [&frames](const render::ChunkKey& k) {
        return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
    };
    out.assign(static_cast<std::size_t>(view.w) * static_cast<std::size_t>(view.h), 0);
    const bool done = within(std::chrono::seconds(30), [&] {
        follower.follow(frames);
        const auto scene = picture.scene();
        follower.visibility().begin_frame(view, group);
        const auto s =
            frames.render(scene, view, group, 0, render::WaterStyle::Original, out.data());
        follower.visibility().frame_done(true, fresh, fresh);
        return s.chunks_missing == 0 && !frames.busy() &&
               !app::visibility_waiting(follower.visibility().classify(turn));
    });
    REQUIRE(done);
    follower.follow(frames);
    follower.visibility().begin_frame(view, group);
    frames.render(picture.scene(), view, group, 0, render::WaterStyle::Original, out.data());
    follower.visibility().frame_done(true, fresh, fresh);
    return follower.visibility().classify(turn);
}

std::size_t count_diff_pixels(const std::vector<std::uint32_t>& a,
                              const std::vector<std::uint32_t>& b) {
    REQUIRE(a.size() == b.size());
    std::size_t n = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            ++n;
        }
    }
    return n;
}

void maybe_dump_png(const char* name, const render::Rect& view,
                    const std::vector<std::uint32_t>& xrgb) {
    const char* dir = std::getenv("ANNO_DUMP_ROADS_PNG");
    if (dir == nullptr || *dir == 0) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::vector<std::uint8_t> rgba(xrgb.size() * 4);
    for (std::size_t i = 0; i < xrgb.size(); ++i) {
        const std::uint32_t px = xrgb[i];
        rgba[i * 4 + 0] = static_cast<std::uint8_t>(px >> 16);
        rgba[i * 4 + 1] = static_cast<std::uint8_t>(px >> 8);
        rgba[i * 4 + 2] = static_cast<std::uint8_t>(px);
        rgba[i * 4 + 3] = 255;
    }
    const std::string err =
        app::write_png(std::filesystem::path(dir) / name, static_cast<std::uint32_t>(view.w),
                       static_cast<std::uint32_t>(view.h), rgba);
    CHECK_MESSAGE(err.empty(), err);
}

bool scene_has_item_in_box(const render::Scene& scene, std::size_t island_index, std::uint16_t x0,
                           std::uint16_t y0, std::uint16_t w, std::uint16_t h,
                           std::optional<std::uint16_t> anlage = std::nullopt) {
    const auto& lists = scene.island_lists(island_index);
    for (const auto* list : {&lists.below, &lists.above, &lists.other}) {
        for (const render::Prepared& p : *list) {
            if (p.first.x >= x0 && p.first.x < x0 + w && p.first.y >= y0 && p.first.y < y0 + h) {
                if (!anlage || p.first.anlage == *anlage) {
                    return true;
                }
            }
        }
    }
    return false;
}

std::optional<sim::TileXY> find_bare_road_tile(const anno::test::Lan5& l) {
    const sim::World& world = l.play.world;
    const sim::Island* island = world.island_at(l.setup.island.x0, l.setup.island.y0);
    const sim::BuildingType* road = world.types().find(kLandRoad);
    if (island == nullptr || road == nullptr || !l.setup.city) {
        return std::nullopt;
    }
    const auto w = static_cast<std::int32_t>(island->tiles.width());
    const auto h = static_cast<std::int32_t>(island->tiles.height());
    for (std::int32_t y = 8; y + 8 < h; ++y) {
        for (std::int32_t x = 8; x + 8 < w; ++x) {
            const std::int32_t wx = island->tiles.x0() + x;
            const std::int32_t wy = island->tiles.y0() + y;
            const std::uint64_t idx = static_cast<std::uint64_t>(y) * island->tiles.width() +
                                      static_cast<std::uint64_t>(x);
            if (island->tiles.object(idx) != 0) {
                continue;
            }
            const sim::PlacementRecord rec{kLandRoad, wx, wy, 0, 0};
            if (sim::footprint_owner_rule(*island, world.config(), *road, rec,
                                          l.setup.city->city) == sim::Reject::None &&
                sim::placement_rule(*island, world.types(), *road, rec).reject ==
                    sim::Reject::None) {
                return sim::TileXY{wx, wy};
            }
        }
    }
    return std::nullopt;
}

struct TreeBuildingSpot {
    sim::TypeId type;
    sim::TileXY world_origin;
    std::uint16_t lx = 0, ly = 0;
    std::uint16_t w = 1, h = 1;
    std::uint16_t tree_lx = 0, tree_ly = 0;
    std::uint16_t tree_anlage = 0;
};

struct LocalBox {
    std::uint16_t x = 0, y = 0, w = 1, h = 1;
    [[nodiscard]] bool overlaps(std::uint16_t ox, std::uint16_t oy, std::uint16_t ow,
                                std::uint16_t oh) const {
        return x < ox + ow && ox < x + w && y < oy + oh && oy < y + h;
    }
};

std::optional<TreeBuildingSpot>
find_tree_spot_for_building(const anno::test::Lan5& l, sim::TypeId building_id,
                            const std::vector<LocalBox>& exclude = {}) {
    const sim::World& world = l.play.world;
    const sim::Island* island = world.island_at(l.setup.island.x0, l.setup.island.y0);
    const sim::BuildingType* bt = world.types().find(building_id);
    if (island == nullptr || bt == nullptr || !l.setup.city) {
        return std::nullopt;
    }
    const auto [fw, fh] = sim::footprint(*bt, 0);
    const auto w = static_cast<std::int32_t>(island->tiles.width());
    const auto h = static_cast<std::int32_t>(island->tiles.height());
    for (std::int32_t y = 4; y + static_cast<std::int32_t>(fh) + 4 <= h; ++y) {
        for (std::int32_t x = 4; x + static_cast<std::int32_t>(fw) + 4 <= w; ++x) {
            const auto ux = static_cast<std::uint16_t>(x);
            const auto uy = static_cast<std::uint16_t>(y);
            const auto uw = static_cast<std::uint16_t>(fw);
            const auto uh = static_cast<std::uint16_t>(fh);
            const bool excluded =
                std::any_of(exclude.begin(), exclude.end(),
                            [&](const LocalBox& b) { return b.overlaps(ux, uy, uw, uh); });
            if (excluded) {
                continue;
            }
            const std::int32_t wx = island->tiles.x0() + x;
            const std::int32_t wy = island->tiles.y0() + y;
            const sim::PlacementRecord rec{building_id, wx, wy, 0, 0};
            if (sim::footprint_owner_rule(*island, world.config(), *bt, rec, l.setup.city->city) !=
                    sim::Reject::None ||
                sim::placement_rule(*island, world.types(), *bt, rec).reject != sim::Reject::None) {
                continue;
            }
            // Look for at least one anchored flag-0x20 tree inside the footprint.
            for (std::uint32_t dy = 0; dy < fh; ++dy) {
                for (std::uint32_t dx = 0; dx < fw; ++dx) {
                    const std::uint64_t idx =
                        static_cast<std::uint64_t>(y + static_cast<std::int32_t>(dy)) *
                            island->tiles.width() +
                        static_cast<std::uint64_t>(x + static_cast<std::int32_t>(dx));
                    const std::uint32_t obj = island->tiles.object(idx);
                    if (obj == 0 || island->tiles.foot_dx(idx) != 0 ||
                        island->tiles.foot_dy(idx) != 0) {
                        continue;
                    }
                    const auto flags = app::anlage_head_flags(l.scene->anlagen, obj);
                    if (flags && (*flags & 0x20U) != 0) {
                        return TreeBuildingSpot{
                            building_id,
                            sim::TileXY{wx, wy},
                            ux,
                            uy,
                            uw,
                            uh,
                            static_cast<std::uint16_t>(x + static_cast<std::int32_t>(dx)),
                            static_cast<std::uint16_t>(y + static_cast<std::int32_t>(dy)),
                            static_cast<std::uint16_t>(obj),
                        };
                    }
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace

TEST_CASE("tile object picture: placed road reaches Visible, differs from bare tile, rotates, "
          "matches fresh reference, and demolishes to bare ground (game files)" *
          doctest::test_suite("tile_object_picture")) {
    const auto game_dir = anno::test::game_dir();
    auto l = anno::test::lan5(false, false);
    if (!l || !game_dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto spot = find_bare_road_tile(*l);
    REQUIRE(spot.has_value());
    const std::uint16_t lx = static_cast<std::uint16_t>(spot->x - l->setup.island.x0);
    const std::uint16_t ly = static_cast<std::uint16_t>(spot->y - l->setup.island.y0);

    app::sort_buildings(l->buildings);
    auto initial_tiles = app::play_tile_objects(l->play.world);
    app::sort_tile_objects(initial_tiles);

    std::vector<bool> building_classes(app::kPictureAnlageIds, false);
    std::vector<bool> tile_classes(app::kPictureAnlageIds, false);
    for (std::size_t id = 0; id < building_classes.size(); ++id) {
        const auto uid = static_cast<std::uint16_t>(id);
        building_classes[id] = app::anlage_is_building(l->play.world, uid);
        tile_classes[id] = app::anlage_is_tracked_tile_object(l->play.world, uid);
    }
    const auto is_b = [&building_classes](std::uint16_t id) {
        return id < building_classes.size() && building_classes[id];
    };
    const auto is_t = [&tile_classes](std::uint16_t id) {
        return id < tile_classes.size() && tile_classes[id];
    };

    app::ScenePicture picture(l->scene, l->buildings, std::move(initial_tiles), is_b, is_t, 1);
    render::FrameRenderer frames(2);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));

    const sim::CityRef city = *l->setup.city;
    app::PlayRunner runner(std::move(l->play.world));
    app::PlayFollower follower(picture, runner);

    const std::size_t group = 1;
    const render::Rect view = view_around(*l, *spot);

    std::vector<std::uint32_t> before;
    const auto fresh = [&frames](const render::ChunkKey& k) {
        return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
    };
    REQUIRE(within(std::chrono::seconds(20), [&] {
        follower.follow(frames);
        before.assign(static_cast<std::size_t>(view.w) * static_cast<std::size_t>(view.h), 0);
        const auto s = frames.render(picture.scene(), view, group, 0, render::WaterStyle::Original,
                                     before.data());
        follower.visibility().begin_frame(view, group);
        follower.visibility().frame_done(true, fresh, fresh);
        return s.chunks_missing == 0 && !frames.busy();
    }));
    maybe_dump_png("01_road_bare.png", view, before);
    CHECK(picture.live_tile_object_count() == 0);
    CHECK_FALSE(scene_has_item_in_box(*picture.scene(), l->island, lx, ly, 1, 1,
                                      static_cast<std::uint16_t>(kLandRoad.v)));

    auto post_road = [&](std::uint8_t rot) -> std::uint64_t {
        sim::BuildCmd b;
        b.city = city;
        b.placements.push_back(sim::PlacementWire{kLandRoad, spot->x, spot->y, rot, 0, 0});
        sim::Command c;
        c.issuer = app::developer_player();
        c.body = std::move(b);
        const app::CommandId id = runner.post_command(std::move(c));
        runner.run_n(1);
        const auto out = runner.outcome(id);
        REQUIRE(out.state == app::CommandOutcome::State::Applied);
        return out.turn;
    };

    // Place road rotation 0.
    const std::uint64_t turn0 = post_road(0);
    std::vector<std::uint32_t> after_rot0;
    const V v0 = drive_until_settled(follower, picture, frames, view, group, turn0, after_rot0);
    CHECK(v0 == V::Visible);
    CHECK(v0 != V::NoVisualChange);
    CHECK(picture.live_tile_object_count() == 1);
    CHECK(scene_has_item_in_box(*picture.scene(), l->island, lx, ly, 1, 1,
                                static_cast<std::uint16_t>(kLandRoad.v)));
    const std::size_t diff0 = count_diff_pixels(before, after_rot0);
    MESSAGE("road rot 0 at (" << spot->x << "," << spot->y << ") changed " << diff0 << " px");
    CHECK(diff0 > 50);
    maybe_dump_png("02_road_rot0.png", view, after_rot0);

    // Must match a freshly loaded scene with the same buildings + tile objects.
    app::SceneObjects ref_objects;
    ref_objects.is_building = is_b;
    ref_objects.buildings = picture.buildings();
    ref_objects.is_tile_object = is_t;
    ref_objects.tile_objects = picture.tile_objects();
    auto ref_loaded = app::load_placed_scene(*game_dir, l->play.name, l->play.placed, &ref_objects);
    REQUIRE(ref_loaded.ok());
    ref_loaded.value().scene->river_cfg.enabled = false;
    render::FrameRenderer ref_frames(2);
    ref_frames.set_workers(std::make_unique<render::ChunkWorkers>(2));
    const auto ref_px = render_settled(ref_frames, ref_loaded.value().scene, view, group);
    CHECK(after_rot0 == ref_px);

    // Demolish road and place rotation 1: pixels differ from both bare tile and rot 0.
    {
        sim::Command dcmd;
        dcmd.issuer = app::developer_player();
        dcmd.body = sim::DemolishCmd{city, {*spot}};
        const app::CommandId did = runner.post_command(std::move(dcmd));
        runner.run_n(1);
        const auto dout = runner.outcome(did);
        REQUIRE(dout.state == app::CommandOutcome::State::Applied);
        std::vector<std::uint32_t> demolished;
        const V vd =
            drive_until_settled(follower, picture, frames, view, group, dout.turn, demolished);
        CHECK(vd == V::Visible);
        CHECK(picture.live_tile_object_count() == 0);
        CHECK(demolished == before);
        maybe_dump_png("03_road_demolished.png", view, demolished);
    }

    const std::uint64_t turn1 = post_road(1);
    std::vector<std::uint32_t> after_rot1;
    const V v1 = drive_until_settled(follower, picture, frames, view, group, turn1, after_rot1);
    CHECK(v1 == V::Visible);
    CHECK(picture.live_tile_object_count() == 1);
    CHECK(count_diff_pixels(before, after_rot1) > 50);
    CHECK(count_diff_pixels(after_rot0, after_rot1) > 0);
    maybe_dump_png("04_road_rot1.png", view, after_rot1);
    runner.stop();
}

TEST_CASE("tile object picture: cleared trees vanish under building and road, stay bare on "
          "demolish (including direct tree demolish and collapsed resync) (game files)" *
          doctest::test_suite("tile_object_picture")) {
    auto l = anno::test::lan5(false, false);
    if (!l) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const auto spot = find_tree_spot_for_building(*l, sim::TypeId{501});
    REQUIRE(spot.has_value());
    std::vector<LocalBox> used{LocalBox{spot->lx, spot->ly, spot->w, spot->h}};
    const auto spot_road = find_tree_spot_for_building(*l, kLandRoad, used);
    REQUIRE(spot_road.has_value());
    used.push_back(LocalBox{spot_road->lx, spot_road->ly, spot_road->w, spot_road->h});
    const auto spot_resync = find_tree_spot_for_building(*l, sim::TypeId{501}, used);
    REQUIRE(spot_resync.has_value());
    used.push_back(LocalBox{spot_resync->lx, spot_resync->ly, spot_resync->w, spot_resync->h});
    const auto spot_direct = find_tree_spot_for_building(*l, kLandRoad, used);
    REQUIRE(spot_direct.has_value());

    app::sort_buildings(l->buildings);
    auto initial_tiles = app::play_tile_objects(l->play.world);
    app::sort_tile_objects(initial_tiles);

    std::vector<bool> building_classes(app::kPictureAnlageIds, false);
    std::vector<bool> tile_classes(app::kPictureAnlageIds, false);
    for (std::size_t id = 0; id < building_classes.size(); ++id) {
        const auto uid = static_cast<std::uint16_t>(id);
        building_classes[id] = app::anlage_is_building(l->play.world, uid);
        tile_classes[id] = app::anlage_is_tracked_tile_object(l->play.world, uid);
    }
    const auto is_b = [&building_classes](std::uint16_t id) {
        return id < building_classes.size() && building_classes[id];
    };
    const auto is_t = [&tile_classes](std::uint16_t id) {
        return id < tile_classes.size() && tile_classes[id];
    };

    app::ScenePicture picture(l->scene, l->buildings, std::move(initial_tiles), is_b, is_t, 1);
    render::FrameRenderer frames(2);
    frames.set_workers(std::make_unique<render::ChunkWorkers>(2));

    const sim::CityRef city = *l->setup.city;
    app::PlayRunner runner(std::move(l->play.world));
    app::PlayFollower follower(picture, runner);

    const std::size_t group = 1;
    const render::Rect view = view_around(*l, spot->world_origin);

    std::vector<std::uint32_t> before_forest;
    const auto fresh = [&frames](const render::ChunkKey& k) {
        return frames.chunk(k) != nullptr && frames.stale_generation(k) == 0;
    };
    REQUIRE(within(std::chrono::seconds(20), [&] {
        follower.follow(frames);
        before_forest.assign(static_cast<std::size_t>(view.w) * static_cast<std::size_t>(view.h),
                             0);
        const auto s = frames.render(picture.scene(), view, group, 0, render::WaterStyle::Original,
                                     before_forest.data());
        follower.visibility().begin_frame(view, group);
        follower.visibility().frame_done(true, fresh, fresh);
        return s.chunks_missing == 0 && !frames.busy();
    }));
    REQUIRE(scene_has_item_in_box(*picture.scene(), l->island, spot->tree_lx, spot->tree_ly, 1, 1,
                                  spot->tree_anlage));
    maybe_dump_png("05_tree_before.png", view, before_forest);

    // 1. Place building over tree: tree disappears from island_lists & pixels change.
    {
        sim::BuildCmd b;
        b.city = city;
        b.placements.push_back(
            sim::PlacementWire{spot->type, spot->world_origin.x, spot->world_origin.y, 0, 0, 0});
        sim::Command c;
        c.issuer = app::developer_player();
        c.body = std::move(b);
        const app::CommandId id = runner.post_command(std::move(c));
        runner.run_n(1);
        const auto out = runner.outcome(id);
        REQUIRE(out.state == app::CommandOutcome::State::Applied);
        std::vector<std::uint32_t> with_house;
        const V v =
            drive_until_settled(follower, picture, frames, view, group, out.turn, with_house);
        CHECK(v == V::Visible);
        CHECK(picture.cleared_tile_count() >= 1);
        CHECK_FALSE(scene_has_item_in_box(*picture.scene(), l->island, spot->tree_lx, spot->tree_ly,
                                          1, 1, spot->tree_anlage));
        CHECK(count_diff_pixels(before_forest, with_house) > 100);
        maybe_dump_png("06_tree_under_house.png", view, with_house);
    }

    // 2. Demolish building: footprint stays bare ground (tree does not reappear).
    std::vector<std::uint32_t> demolished_bare;
    {
        sim::Command c;
        c.issuer = app::developer_player();
        c.body = sim::DemolishCmd{city, {spot->world_origin}};
        const app::CommandId id = runner.post_command(std::move(c));
        runner.run_n(1);
        const auto out = runner.outcome(id);
        REQUIRE(out.state == app::CommandOutcome::State::Applied);
        const V v =
            drive_until_settled(follower, picture, frames, view, group, out.turn, demolished_bare);
        CHECK(v == V::Visible);
        CHECK_FALSE(scene_has_item_in_box(*picture.scene(), l->island, spot->lx, spot->ly, spot->w,
                                          spot->h));
        CHECK(count_diff_pixels(before_forest, demolished_bare) > 10);
        maybe_dump_png("07_tree_demolished_bare.png", view, demolished_bare);
    }

    // 3. Place road over a distinct tree: tree vanishes, road draws; demolish road: bare ground.
    {
        const render::Rect vroad = view_around(*l, spot_road->world_origin);
        const auto before_road_tree = render_settled(frames, picture.scene(), vroad, group);
        REQUIRE(scene_has_item_in_box(*picture.scene(), l->island, spot_road->tree_lx,
                                      spot_road->tree_ly, 1, 1, spot_road->tree_anlage));

        sim::BuildCmd rb;
        rb.city = city;
        rb.placements.push_back(sim::PlacementWire{kLandRoad, spot_road->world_origin.x,
                                                   spot_road->world_origin.y, 0, 0, 0});
        sim::Command rc;
        rc.issuer = app::developer_player();
        rc.body = std::move(rb);
        const app::CommandId rid = runner.post_command(std::move(rc));
        runner.run_n(1);
        const auto rout = runner.outcome(rid);
        REQUIRE(rout.state == app::CommandOutcome::State::Applied);
        std::vector<std::uint32_t> road_over_tree;
        const V rv =
            drive_until_settled(follower, picture, frames, vroad, group, rout.turn, road_over_tree);
        CHECK(rv == V::Visible);
        CHECK_FALSE(scene_has_item_in_box(*picture.scene(), l->island, spot_road->tree_lx,
                                          spot_road->tree_ly, 1, 1, spot_road->tree_anlage));
        CHECK(scene_has_item_in_box(*picture.scene(), l->island, spot_road->lx, spot_road->ly, 1, 1,
                                    static_cast<std::uint16_t>(kLandRoad.v)));
        CHECK(count_diff_pixels(before_road_tree, road_over_tree) > 10);
        maybe_dump_png("08_road_over_tree.png", vroad, road_over_tree);

        sim::Command dc;
        dc.issuer = app::developer_player();
        dc.body = sim::DemolishCmd{city, {spot_road->world_origin}};
        const app::CommandId did = runner.post_command(std::move(dc));
        runner.run_n(1);
        const auto dout = runner.outcome(did);
        REQUIRE(dout.state == app::CommandOutcome::State::Applied);
        std::vector<std::uint32_t> road_demolished;
        const V dv = drive_until_settled(follower, picture, frames, vroad, group, dout.turn,
                                         road_demolished);
        CHECK(dv == V::Visible);
        CHECK_FALSE(
            scene_has_item_in_box(*picture.scene(), l->island, spot_road->lx, spot_road->ly, 1, 1));
        CHECK(count_diff_pixels(before_road_tree, road_demolished) > 10);
    }

    // 4. Mailbox collapse test on untouched `spot_resync`: place house on turn N, demolish on turn
    //    N+1 without calling follow() between turns (bound = 1) -> one resync batch -> bare ground.
    {
        const render::Rect vresync = view_around(*l, spot_resync->world_origin);
        const auto before_resync = render_settled(frames, picture.scene(), vresync, group);
        REQUIRE(scene_has_item_in_box(*picture.scene(), l->island, spot_resync->tree_lx,
                                      spot_resync->tree_ly, 1, 1, spot_resync->tree_anlage));

        runner.set_batch_bound(1);
        sim::BuildCmd b;
        b.city = city;
        b.placements.push_back(sim::PlacementWire{spot_resync->type, spot_resync->world_origin.x,
                                                  spot_resync->world_origin.y, 0, 0, 0});
        sim::Command c;
        c.issuer = app::developer_player();
        c.body = std::move(b);
        const app::CommandId bid = runner.post_command(std::move(c));
        runner.run_n(1);
        REQUIRE(runner.outcome(bid).state == app::CommandOutcome::State::Applied);

        sim::Command dc;
        dc.issuer = app::developer_player();
        dc.body = sim::DemolishCmd{city, {spot_resync->world_origin}};
        const app::CommandId did = runner.post_command(std::move(dc));
        runner.run_n(1);
        const auto dout = runner.outcome(did);
        REQUIRE(dout.state == app::CommandOutcome::State::Applied);

        std::vector<std::uint32_t> after_resync;
        const V vr =
            drive_until_settled(follower, picture, frames, vresync, group, dout.turn, after_resync);
        CHECK(vr == V::Visible);
        CHECK_FALSE(scene_has_item_in_box(*picture.scene(), l->island, spot_resync->lx,
                                          spot_resync->ly, spot_resync->w, spot_resync->h));
        CHECK(count_diff_pixels(before_resync, after_resync) > 10);
    }

    // 5. Direct DemolishCmd on a distinct untouched tree tile without placing a building first.
    {
        const render::Rect v3 = view_around(*l, spot_direct->world_origin);
        const auto before_t3 = render_settled(frames, picture.scene(), v3, group);
        REQUIRE(scene_has_item_in_box(*picture.scene(), l->island, spot_direct->tree_lx,
                                      spot_direct->tree_ly, 1, 1, spot_direct->tree_anlage));
        sim::Command dc;
        dc.issuer = app::developer_player();
        dc.body = sim::DemolishCmd{city, {spot_direct->world_origin}};
        const app::CommandId did = runner.post_command(std::move(dc));
        runner.run_n(1);
        const auto dout = runner.outcome(did);
        REQUIRE(dout.state == app::CommandOutcome::State::Applied);
        std::vector<std::uint32_t> after_t3;
        const V v = drive_until_settled(follower, picture, frames, v3, group, dout.turn, after_t3);
        CHECK(v == V::Visible);
        CHECK_FALSE(scene_has_item_in_box(*picture.scene(), l->island, spot_direct->tree_lx,
                                          spot_direct->tree_ly, 1, 1, spot_direct->tree_anlage));
        CHECK(count_diff_pixels(before_t3, after_t3) > 10);
    }

    runner.stop();
}
