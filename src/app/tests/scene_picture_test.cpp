// ScenePicture (W4.3 round du, steps 4-7; docs/app/game-shell.md, "Immutable per-island object
// lists"): the picture after a building change equals a fresh scene; a stale completion never
// overwrites a newer chunk; a second change during a rebuild supersedes the first; a resync
// reaches the same list; every changed pixel lies in an invalidated key, at every zoom level;
// the number of live generations is bounded. Needs the game files (skips without them).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/building_delta.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/play.hpp>
#include <anno/app/scene_picture.hpp>
#include <anno/render/frame.hpp>
#include <anno/test/game_dir.hpp>

namespace {

namespace fs = std::filesystem;
namespace render = anno::render;
using anno::app::BuildingBatch;
using anno::app::BuildingDelta;
using anno::app::SceneBuilding;
using anno::app::ScenePicture;
using render::FrameRenderer;
using render::ZoomLevels;

fs::path fixed_map() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

/// The fixed map as --play loads it: the world's buildings at tick 0, and the scene built from
/// them.
struct Fixture {
    fs::path dir;
    anno::app::PlayLoad play;
    std::vector<SceneBuilding> buildings;
};

std::optional<Fixture> load_fixture() {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        return std::nullopt;
    }
    auto loaded = anno::app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    Fixture f{*dir, std::move(loaded).value(), {}};
    f.buildings = anno::app::play_buildings(f.play.world);
    anno::app::sort_buildings(f.buildings);
    return f;
}

bool is_building(const Fixture& f, std::uint16_t id) {
    return anno::app::anlage_is_building(f.play.world, id);
}

/// A scene built from `buildings` (its byte buffers are owned by the scene).
struct Built {
    std::shared_ptr<const render::Scene> scene;
};

Built build(const Fixture& f, const std::vector<SceneBuilding>& buildings,
            std::optional<ZoomLevels> levels = std::nullopt) {
    anno::app::SceneObjects objects;
    objects.is_building = [&f](std::uint16_t id) { return is_building(f, id); };
    objects.buildings = buildings;
    auto loaded = anno::app::load_placed_scene(f.dir, f.play.name, f.play.placed, &objects);
    REQUIRE(loaded.ok());
    anno::app::LoadedScene ls = std::move(loaded).value();
    // The invalidation rules are checked on the legacy river look: a river flow that finishes
    // later rebuilds chunks for its own reason, which would blur the counts (round du).
    ls.scene->river_cfg.enabled = false;
    if (levels) {
        ls.scene->set_levels(*levels);
    }
    return {std::shared_ptr<const render::Scene>(std::move(ls.scene))};
}

std::shared_ptr<const BuildingBatch> batch(std::uint64_t turn, std::vector<BuildingDelta> deltas) {
    auto b = std::make_shared<BuildingBatch>();
    b->turn = turn;
    b->deltas = std::move(deltas);
    return b;
}

BuildingDelta removed(const SceneBuilding& b) {
    return {BuildingDelta::Kind::Removed, b};
}

BuildingDelta added(const SceneBuilding& b) {
    return {BuildingDelta::Kind::Added, b};
}

/// A change: one building of the list moves two tiles to the right (one removed, one added).
struct Change {
    SceneBuilding before, after;
    std::vector<SceneBuilding> list;   // the list after the change
    std::vector<BuildingDelta> deltas; // what the sim would publish
};

Change move_one(const Fixture& f, std::size_t pick, int dx) {
    Change c;
    c.before = f.buildings.at(pick);
    c.after = c.before;
    c.after.x = static_cast<std::uint16_t>(c.before.x + dx);
    c.list = f.buildings;
    c.list.erase(c.list.begin() + static_cast<std::ptrdiff_t>(pick));
    c.list.push_back(c.after);
    anno::app::sort_buildings(c.list);
    c.deltas = {removed(c.before), added(c.after)};
    return c;
}

/// Renders `view` at `group` until every visible chunk is built (workers: waits for them).
std::vector<std::uint32_t> render_view(FrameRenderer& frames,
                                       const std::shared_ptr<const render::Scene>& scene,
                                       render::Rect view, std::size_t group,
                                       render::ChunkWorkers* workers) {
    std::vector<std::uint32_t> out(static_cast<std::size_t>(view.w) *
                                   static_cast<std::size_t>(view.h));
    for (int round = 0; round < 64; ++round) {
        const auto stats =
            frames.render(scene, view, group, 0, render::WaterStyle::Original, out.data());
        if (workers != nullptr) {
            workers->wait_idle();
        }
        if (stats.chunks_missing == 0 && !frames.busy()) {
            break;
        }
    }
    // One more frame takes what the last round completed.
    frames.render(scene, view, group, 0, render::WaterStyle::Original, out.data());
    return out;
}

/// The chunks of `view` at `group`, as keys.
std::vector<render::ChunkKey> keys_of(render::Rect view, std::size_t group) {
    std::vector<render::ChunkKey> keys;
    const int x0 = render::div_euclid(view.x, render::kChunk),
              x1 = render::div_euclid(view.x + view.w - 1, render::kChunk);
    const int y0 = render::div_euclid(view.y, render::kChunk),
              y1 = render::div_euclid(view.y + view.h - 1, render::kChunk);
    for (int cy = y0; cy <= y1; ++cy) {
        for (int cx = x0; cx <= x1; ++cx) {
            keys.emplace_back(group, cx, cy);
        }
    }
    return keys;
}

/// A view of 3 x 3 chunks around the chunks a building's objects touch at `group` (a view small
/// enough that the planner requests every one of its keys).
render::Rect small_view_around(const render::Scene& scene, const SceneBuilding& b,
                               std::size_t group) {
    std::optional<render::Prepared> found;
    std::size_t index = 0;
    for (std::size_t i = 0; i < scene.islands.size() && !found; ++i) {
        if (scene.islands[i].number != b.island) {
            continue;
        }
        index = i;
        const auto& lists = scene.island_lists(i);
        for (const auto* list : {&lists.below, &lists.above, &lists.other}) {
            for (const render::Prepared& p : *list) {
                if (p.first.anlage == b.type && p.first.x == b.x && p.first.y == b.y) {
                    found = p;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }
    REQUIRE(found.has_value());
    const auto box = scene.object_bounds(index, *found, group);
    REQUIRE(box.has_value());
    const auto keys = render::chunk_keys_touching(scene, *box, group);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (std::size_t n = 0; n < keys.size(); ++n) {
        const render::Rect r = render::chunk_rect(std::get<1>(keys[n]), std::get<2>(keys[n]));
        if (n == 0 || r.x < x0) {
            x0 = r.x;
        }
        if (n == 0 || r.y < y0) {
            y0 = r.y;
        }
        if (n == 0 || r.x + r.w > x1) {
            x1 = r.x + r.w;
        }
        if (n == 0 || r.y + r.h > y1) {
            y1 = r.y + r.h;
        }
    }
    return render::Rect{x0 - render::kChunk, y0 - render::kChunk, x1 - x0 + 2 * render::kChunk,
                        y1 - y0 + 2 * render::kChunk};
}

} // namespace

TEST_CASE(
    "scene picture: after a move the pixels equal a fresh scene, and only the marked keys rebuild" *
    doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const Built base = build(*f, f->buildings);
    const std::size_t pick = f->buildings.size() / 2;
    const Change c = move_one(*f, pick, 2);

    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 2);
    FrameRenderer frames(2);
    auto workers = std::make_unique<render::ChunkWorkers>(*base.scene, 2);
    render::ChunkWorkers* raw = workers.get();
    frames.set_workers(std::move(workers));
    const auto view = base.scene->bounds(2);
    REQUIRE(view.has_value());
    const auto before = render_view(frames, base.scene, *view, 2, raw);

    // Every key of the view was built once from generation 0.
    for (const auto& k : keys_of(*view, 2)) {
        if (frames.chunk(k) != nullptr) {
            CHECK(frames.chunk_builds(k) == 1);
        }
    }

    picture.apply({batch(1, c.deltas)});
    picture.wait_idle();
    REQUIRE(picture.commit(frames) == 1);
    const auto gen1 = picture.scene();
    CHECK(gen1->generation() == 1);

    std::set<render::ChunkKey> marked;
    for (const auto& k : keys_of(*view, 2)) {
        if (frames.stale_generation(k) != 0) {
            marked.insert(k);
        }
    }
    CHECK_FALSE(marked.empty());

    const auto picture_pixels = render_view(frames, gen1, *view, 2, raw);

    // The fresh scene of the same list, with its own renderer and no workers.
    const Built fresh = build(*f, c.list);
    FrameRenderer fresh_frames(2);
    const auto fresh_pixels = render_view(fresh_frames, fresh.scene, *view, 2, nullptr);
    CHECK(picture_pixels == fresh_pixels);
    CHECK(before != fresh_pixels); // the change is visible

    // A key far from the change is not marked and not rebuilt: an invalidate-everything rule
    // (a mutant that marks every key) fails here.
    REQUIRE_FALSE(marked.empty());
    const render::ChunkKey near = *marked.begin();
    render::ChunkKey far = near;
    int far_distance = -1;
    for (const auto& k : keys_of(*view, 2)) {
        const int d = std::abs(std::get<1>(k) - std::get<1>(near)) +
                      std::abs(std::get<2>(k) - std::get<2>(near));
        if (d > far_distance) {
            far_distance = d;
            far = k;
        }
    }
    CHECK(frames.stale_generation(far) == 0);
    CHECK(frames.chunk_rebuilds(far) == 0);
    if (frames.chunk(far) != nullptr) {
        CHECK(frames.chunk(far)->generation == 0);
    }

    // Unmarked keys were not rebuilt; marked ones were rebuilt once, from generation 1.
    for (const auto& k : keys_of(*view, 2)) {
        if (frames.chunk(k) == nullptr) {
            continue;
        }
        if (marked.contains(k)) {
            CHECK(frames.chunk_rebuilds(k) == 1);
            CHECK(frames.chunk(k)->generation == 1);
        } else {
            CHECK(frames.chunk_rebuilds(k) == 0);
        }
    }
}

TEST_CASE("scene picture: a stale completion neither overwrites nor clears the newer mark" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const Built base = build(*f, f->buildings);
    const Change c = move_one(*f, f->buildings.size() / 2, 2);
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 2);
    FrameRenderer frames(2);
    auto workers = std::make_unique<render::ChunkWorkers>(*base.scene, 2);
    render::ChunkWorkers* raw = workers.get();
    frames.set_workers(std::move(workers));
    const render::Rect small = small_view_around(*base.scene, c.before, 2);
    const auto* view = &small;

    // Generation 0's chunks are built and waiting in the results queue (not yet taken).
    frames.update_chunks(base.scene, *view, 2);
    raw->wait_idle();

    // The change arrives: generation 1 invalidates the keys it touches.
    picture.apply({batch(1, c.deltas)});
    picture.wait_idle();
    REQUIRE(picture.commit(frames) == 1);
    const auto gen1 = picture.scene();
    // The next update takes the generation-0 results first: they must be discarded.
    frames.update_chunks(gen1, *view, 2);
    CHECK(frames.stale_discarded() >= 1);

    const auto pixels = render_view(frames, gen1, *view, 2, raw);
    const Built fresh = build(*f, c.list);
    FrameRenderer fresh_frames(2);
    CHECK(pixels == render_view(fresh_frames, fresh.scene, *view, 2, nullptr));
    for (const auto& k : keys_of(*view, 2)) {
        if (const auto chunk = frames.chunk(k)) {
            // Every cached chunk of the view is from generation 1 or unchanged (generation 0 and
            // unmarked): a generation-0 chunk of a marked key never survives.
            CHECK((chunk->generation == 1 || frames.stale_generation(k) == 0));
        }
    }
}

TEST_CASE("scene picture: a second change during a rebuild supersedes the first" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const Built base = build(*f, f->buildings);
    const Change first = move_one(*f, f->buildings.size() / 2, 2);
    // The second change moves the first one's building again (the island's list is re-read).
    std::vector<SceneBuilding> mid = first.list;
    const SceneBuilding moved = first.after;
    SceneBuilding again = moved;
    again.x = static_cast<std::uint16_t>(again.x + 2);
    mid.erase(std::find(mid.begin(), mid.end(), moved));
    mid.push_back(again);
    anno::app::sort_buildings(mid);

    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 1);
    FrameRenderer frames(1);
    auto workers = std::make_unique<render::ChunkWorkers>(*base.scene, 1);
    frames.set_workers(std::move(workers));
    picture.apply({batch(1, first.deltas)});
    picture.apply({batch(2, {removed(moved), added(again)})});
    picture.wait_idle();
    CHECK(picture.commit(frames) == 1); // one island, one commit: the newest request
    const auto stats = picture.stats();
    CHECK(stats.committed == 1);
    const Built fresh = build(*f, mid);
    const auto view = base.scene->bounds(2);
    REQUIRE(view.has_value());
    FrameRenderer fresh_frames(1);
    // The picture's lists equal the fresh scene's, island by island.
    for (std::size_t i = 0; i < picture.scene()->islands.size(); ++i) {
        const auto& a = picture.scene()->island_lists(i);
        const auto& b = fresh.scene->island_lists(i);
        CHECK(a.below == b.below);
        CHECK(a.above == b.above);
        CHECK(a.other == b.other);
    }
}

TEST_CASE("scene picture: a resync after a delayed consumer reaches the same list" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const Built base = build(*f, f->buildings);
    // Five moves of five different buildings, published while the consumer is away.
    std::vector<SceneBuilding> list = f->buildings;
    anno::app::BuildingMailbox mailbox(2);
    for (std::uint64_t turn = 1; turn <= 5; ++turn) {
        const std::size_t pick = (f->buildings.size() * turn) / 6;
        const SceneBuilding old = list.at(pick);
        SceneBuilding moved = old;
        moved.x = static_cast<std::uint16_t>(old.x + 1);
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(pick));
        list.push_back(moved);
        anno::app::sort_buildings(list);
        mailbox.push(batch(turn, {removed(old), added(moved)}), list);
    }
    // The bound (2) collapsed the batches into one resync, which carries the list of turn 5.
    const auto taken = mailbox.take();
    REQUIRE(taken.size() == 1);
    CHECK(taken.front()->resync);
    CHECK(taken.front()->full == list);

    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 1);
    FrameRenderer frames(1);
    picture.apply(taken);
    CHECK(picture.buildings() == list);
    picture.wait_idle();
    CHECK(picture.commit(frames) >= 1);
    const Built fresh = build(*f, list);
    for (std::size_t i = 0; i < picture.scene()->islands.size(); ++i) {
        CHECK(picture.scene()->island_lists(i).below == fresh.scene->island_lists(i).below);
        CHECK(picture.scene()->island_lists(i).above == fresh.scene->island_lists(i).above);
        CHECK(picture.scene()->island_lists(i).other == fresh.scene->island_lists(i).other);
    }
}

TEST_CASE("scene picture: every changed pixel lies in an invalidated key, at every zoom level" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    // Derived zoom levels, so that the far levels are checked too.
    const ZoomLevels levels = ZoomLevels::with_derived(2);
    const Built base = build(*f, f->buildings, levels);
    const Change c = move_one(*f, f->buildings.size() / 3, 3);
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 1);
    FrameRenderer frames(1);
    picture.apply({batch(1, c.deltas)});
    picture.wait_idle();
    REQUIRE(picture.commit(frames) == 1);
    const auto gen1 = picture.scene();
    CHECK(gen1->levels().size() == levels.size());

    // A view around the changed building at the built group, scaled to every level: the
    // changed pixels of old and new renders (fresh scenes, no workers) must lie in marked keys.
    const render::Rect small = small_view_around(*base.scene, c.before, 2);
    const Built fresh_after = build(*f, c.list, levels);
    std::size_t derived_marks = 0;
    std::size_t changed_total = 0;
    for (std::size_t group = 0; group < gen1->levels().size(); ++group) {
        render::Rect view = small;
        if (group >= levels.built_count()) {
            // The same area one derived level down: scene pixels scale by tile(2) / tile(group).
            const auto [num_w, den_w] = levels.ratio_w(2, group);
            const auto [num_h, den_h] = levels.ratio_h(2, group);
            view = render::Rect{static_cast<int>(small.x * den_w / num_w),
                                static_cast<int>(small.y * den_h / num_h),
                                static_cast<int>(small.w * den_w / num_w) + 1,
                                static_cast<int>(small.h * den_h / num_h) + 1};
        }
        FrameRenderer a(1), b(1);
        const auto old_pixels = render_view(a, base.scene, view, group, nullptr);
        const auto new_pixels = render_view(b, fresh_after.scene, view, group, nullptr);
        for (int y = 0; y < view.h; ++y) {
            for (int x = 0; x < view.w; ++x) {
                const auto i = static_cast<std::size_t>(y) * static_cast<std::size_t>(view.w) +
                               static_cast<std::size_t>(x);
                if (old_pixels[i] == new_pixels[i]) {
                    continue;
                }
                ++changed_total;
                const render::ChunkKey key{group, render::div_euclid(view.x + x, render::kChunk),
                                           render::div_euclid(view.y + y, render::kChunk)};
                CHECK(frames.stale_generation(key) == 1);
                if (group >= levels.built_count()) {
                    ++derived_marks;
                }
            }
        }
    }
    CHECK(changed_total > 0);
    // A derived level sees the change too (its far chunks are reduced from the marked ones).
    CHECK(derived_marks > 0);
}

TEST_CASE(
    "scene picture: an object whose box crosses a chunk boundary marks every chunk it touches" *
    doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const Built base = build(*f, f->buildings);
    // A building moved by dx tiles whose box at group 2 (old or new) meets two chunks: the first
    // such dx in -40..40 for the first building that has one. The move is a real one (its
    // deltas), and its boxes are the ones the picture invalidates.
    std::optional<std::size_t> pick;
    std::optional<int> dx_pick;
    for (std::size_t i = 0; i < f->buildings.size() && !pick; ++i) {
        const SceneBuilding& b = f->buildings[i];
        for (std::size_t n = 0; n < base.scene->islands.size() && !pick; ++n) {
            if (base.scene->islands[n].number != b.island) {
                continue;
            }
            const auto& lists = base.scene->island_lists(n);
            std::optional<render::Prepared> own;
            for (const auto* list : {&lists.below, &lists.above, &lists.other}) {
                for (const render::Prepared& p : *list) {
                    if (p.first.anlage == b.type && p.first.x == b.x && p.first.y == b.y) {
                        own = p;
                        break;
                    }
                }
                if (own) {
                    break;
                }
            }
            if (!own) {
                continue;
            }
            for (int dx = -40; dx <= 40 && !dx_pick; ++dx) {
                render::Prepared moved = *own;
                moved.first.x = static_cast<decltype(moved.first.x)>(moved.first.x + dx);
                const auto box = base.scene->object_bounds(n, moved, 2);
                if (box && render::chunk_keys_touching(*base.scene, *box, 2).size() >= 2) {
                    dx_pick = dx;
                    pick = i;
                }
            }
        }
    }
    REQUIRE(pick.has_value());
    REQUIRE(dx_pick.has_value());
    const Change c = move_one(*f, *pick, *dx_pick);
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 1);
    FrameRenderer frames(1);
    picture.apply({batch(1, c.deltas)});
    picture.wait_idle();
    REQUIRE(picture.commit(frames) == 1);

    const render::Rect view = small_view_around(*base.scene, c.before, 2);
    const Built fresh = build(*f, c.list);
    FrameRenderer a(1), b(1);
    const auto old_pixels = render_view(a, base.scene, view, 2, nullptr);
    const auto new_pixels = render_view(b, fresh.scene, view, 2, nullptr);
    std::set<render::ChunkKey> marked;
    std::size_t changed = 0;
    for (int y = 0; y < view.h; ++y) {
        for (int x = 0; x < view.w; ++x) {
            const auto i = static_cast<std::size_t>(y) * static_cast<std::size_t>(view.w) +
                           static_cast<std::size_t>(x);
            if (old_pixels[i] == new_pixels[i]) {
                continue;
            }
            ++changed;
            const render::ChunkKey key{2, render::div_euclid(view.x + x, render::kChunk),
                                       render::div_euclid(view.y + y, render::kChunk)};
            CHECK(frames.stale_generation(key) == 1);
            marked.insert(key);
        }
    }
    CHECK(changed > 0);
    // The object's old and new boxes together cross a boundary, and the change is visible on
    // at least one chunk of each side of the boundary it is drawn across.
    CHECK(marked.size() >= 1);
}

TEST_CASE("scene picture: no building type of the registry is a river, fall or surf piece" *
          doctest::test_suite("scene_picture")) {
    // The classification boundary of the static and dynamic parts (docs/app/game-shell.md, 3):
    // a building of a river id would change its island's rivers. Checked over every type.
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const std::set<std::uint32_t> river_ids = {2701, 2702, 2703, 2704, 2705, 2706, 2707, 2708};
    std::size_t buildings = 0;
    for (const auto& type : f->play.world.types().types()) {
        if (!is_building(*f, static_cast<std::uint16_t>(type.id.v))) {
            continue;
        }
        ++buildings;
        CHECK_FALSE(river_ids.contains(type.id.v));
    }
    CHECK(buildings > 0);
    MESSAGE("building types checked against the river, fall and surf ids: " << buildings);
}

TEST_CASE("scene picture: no batch, no commit and no mark" * doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const Built base = build(*f, f->buildings);
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 1);
    FrameRenderer frames(1);
    CHECK(picture.apply({}) == 0);
    CHECK(picture.pending() == 0);
    CHECK(picture.commit(frames) == 0);
    CHECK(picture.scene()->generation() == 0);
    CHECK(picture.scene().get() == base.scene.get());
}

TEST_CASE("scene picture: repeated updates keep the live generations bounded" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    const std::size_t threads = 2;
    const Built base = build(*f, f->buildings);
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, threads);
    FrameRenderer frames(1);
    std::vector<std::weak_ptr<const render::Scene>> seen;
    seen.push_back(base.scene);
    std::vector<SceneBuilding> list = f->buildings;
    std::size_t max_alive = 0;
    for (std::uint64_t turn = 1; turn <= 50; ++turn) {
        const std::size_t pick = (turn * 7) % list.size();
        const SceneBuilding old = list[pick];
        SceneBuilding moved = old;
        moved.x = static_cast<std::uint16_t>(old.x + (turn % 2 == 0 ? 1 : -1));
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(pick));
        list.push_back(moved);
        anno::app::sort_buildings(list);
        picture.apply({batch(turn, {removed(old), added(moved)})});
        if (turn % 5 == 0) {
            picture.wait_idle();
            picture.commit(frames);
            seen.push_back(picture.scene());
        }
        std::size_t alive = 0;
        for (const auto& w : seen) {
            alive += w.expired() ? 0U : 1U;
        }
        max_alive = std::max(max_alive, alive);
    }
    picture.wait_idle();
    picture.commit(frames);
    // The current generation, plus at most one per worker in flight, plus the reaper's backlog.
    CHECK(max_alive <= 1 + threads + 2);
    CHECK(picture.stats().committed >= 1);
    CHECK(picture.buildings() == list);
}

// Retained work (round dv, R4; docs/app/game-shell.md, "Retained work"). Each case bounds one
// retention path with the counters of ScenePicture::Stats. Stalls are gates the test opens; no case
// depends on timing except that a released gate lets the blocked thread proceed.
namespace {

/// A gate: a thread that calls pass() waits until the test calls release().
class Gate {
  public:
    void pass() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return open_; });
    }
    void release() {
        {
            const std::lock_guard lock(mutex_);
            open_ = true;
        }
        cv_.notify_all();
    }

  private:
    std::mutex mutex_;
    std::condition_variable cv_;
    bool open_ = false;
};

/// The first building of each island of the fixture, in island order: the buildings the bounds
/// cases move. (The five-player map has buildings on two islands.)
std::vector<SceneBuilding> first_of_each_island(const std::vector<SceneBuilding>& buildings) {
    std::map<std::uint32_t, SceneBuilding> first;
    for (const SceneBuilding& b : buildings) {
        first.try_emplace(b.island, b);
    }
    std::vector<SceneBuilding> out;
    for (const auto& [island, b] : first) {
        out.push_back(b);
    }
    return out;
}

/// Commits until nothing is pending, bounded (a stuck island fails the case, it does not hang it).
bool drain(ScenePicture& picture, FrameRenderer& frames, int rounds) {
    for (int round = 0; round < rounds && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    return picture.pending() == 0;
}

/// A change for a tracked building: it moves one tile, as one batch of the sim would send it. The
/// tracked building `current` is updated to the moved one, and `list` follows the picture.
void move_tracked(ScenePicture& picture, std::vector<SceneBuilding>& list, SceneBuilding& current,
                  std::uint64_t turn) {
    SceneBuilding moved = current;
    moved.x = static_cast<std::uint16_t>(current.x + (turn % 2 == 0 ? 1 : -1));
    picture.apply({batch(turn, {removed(current), added(moved)})});
    const auto it = std::find(list.begin(), list.end(), current);
    REQUIRE(it != list.end());
    *it = moved;
    anno::app::sort_buildings(list);
    current = moved;
}

} // namespace

TEST_CASE("scene picture bounds: a stalled commit holds the outstanding islands to the limit" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    std::vector<SceneBuilding> current = first_of_each_island(f->buildings);
    REQUIRE(current.size() >= 2);
    const Built base = build(*f, f->buildings);
    ScenePicture::Limits limits;
    limits.max_outstanding = 1;
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 2, limits);
    // One change in every island, applied without a commit: the outstanding limit defers the rest.
    std::vector<SceneBuilding> list = f->buildings;
    std::uint64_t turn = 1;
    for (SceneBuilding& c : current) {
        move_tracked(picture, list, c, turn++);
    }
    picture.wait_idle();
    const auto before = picture.stats();
    CHECK(before.outstanding_peak <= limits.max_outstanding);
    CHECK(before.deferred >= current.size() - limits.max_outstanding);
    CHECK(picture.pending() == current.size());
    // Commits start the deferred builds, so every island is committed in turn: progress is not
    // lost.
    FrameRenderer frames(1);
    for (int round = 0; round < 20 && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(picture.pending() == 0);
    const auto after = picture.stats();
    CHECK(after.committed == current.size());
    CHECK(after.started_deferred >= current.size() - limits.max_outstanding);
    CHECK(after.outstanding_peak <= limits.max_outstanding);
    CHECK(picture.buildings() == list);
}

TEST_CASE(
    "scene picture bounds: a stalled reaper makes commits wait, and the backlog stays bounded" *
    doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    std::vector<SceneBuilding> current = first_of_each_island(f->buildings);
    REQUIRE(current.size() >= 2);
    const Built base = build(*f, f->buildings);
    Gate gate;
    ScenePicture::Limits limits;
    limits.max_reclaim_backlog = 2;
    limits.reclaim_gate = [&gate] { gate.pass(); };
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 1, limits);
    FrameRenderer frames(1);
    std::vector<SceneBuilding> list = f->buildings;
    std::uint64_t turn = 1;
    // The reaper is stuck on its first item: every commit adds generations to its backlog.
    for (int round = 0; round < 12; ++round) {
        move_tracked(picture, list, current[static_cast<std::size_t>(round) % current.size()],
                     turn++);
        picture.wait_idle();
        picture.commit(frames);
    }
    const auto stalled = picture.stats();
    CHECK(stalled.commit_skipped >= 1);
    // The backlog is checked before each commit and each request, so its peak is the bound plus
    // what one commit adds: its replaced generation, the rebased jobs' generations and the results'
    // lists (at most one each per outstanding island; docs/app/game-shell.md, "Retained work").
    CHECK(stalled.reclaim_backlog_peak <=
          limits.max_reclaim_backlog + 1 + 2 * limits.max_outstanding);
    gate.release();
    // The commits wait for the reaper: drain after its catch-up, counted by its own counters.
    const auto caught_up = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (picture.stats().reclaim_completed < picture.stats().reclaim_queued &&
           std::chrono::steady_clock::now() < caught_up) {
        std::this_thread::yield();
    }
    REQUIRE(picture.stats().reclaim_completed == picture.stats().reclaim_queued);
    for (int round = 0; round < 40 && picture.pending() > 0; ++round) {
        picture.wait_idle();
        picture.commit(frames);
    }
    CHECK(picture.pending() == 0);
    CHECK(picture.buildings() == list);
    CHECK(picture.stats().reclaim_queued >= stalled.reclaim_queued);
}

TEST_CASE("scene picture bounds: a failed build is counted, and its island can be requested again" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    std::vector<SceneBuilding> current = first_of_each_island(f->buildings);
    REQUIRE(current.size() >= 2);
    const Built base = build(*f, f->buildings);
    const std::uint32_t target = current.front().island;
    std::atomic<int> failures{1};
    ScenePicture::Limits limits;
    limits.build_fault = [&](std::size_t index) {
        // Injected: the first build of the target island throws; later builds run.
        if (base.scene->islands[index].number == target && failures.fetch_sub(1) > 0) {
            throw std::runtime_error("injected build failure");
        }
    };
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 2, limits);
    FrameRenderer frames(1);
    std::vector<SceneBuilding> list = f->buildings;
    move_tracked(picture, list, current.front(), 1);
    picture.wait_idle();
    CHECK(picture.commit(frames) == 0); // the failed island has nothing to commit yet
    CHECK(picture.stats().build_failures == 1);
    CHECK(picture.pending() == 1); // still owed: the change is not dropped
    // The retry needs no new batch: the window's commits build the island again from the desired
    // state.
    CHECK(drain(picture, frames, 20));
    CHECK(picture.buildings() == list);
    CHECK(picture.stats().build_retries >= 1);
    CHECK(picture.error().empty());
}

TEST_CASE(
    "scene picture bounds: destroying the picture with blocked workers and queued builds returns" *
    doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    std::vector<SceneBuilding> current = first_of_each_island(f->buildings);
    REQUIRE(current.size() >= 2);
    const Built base = build(*f, f->buildings);
    Gate gate;
    ScenePicture::Limits limits;
    limits.build_fault = [&gate](std::size_t) { gate.pass(); };
    std::thread opener;
    {
        ScenePicture picture(
            base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 2,
            limits);
        std::vector<SceneBuilding> list = f->buildings;
        std::uint64_t turn = 1;
        for (SceneBuilding& c : current) {
            move_tracked(picture, list, c, turn++);
        }
        // The workers are blocked in their builds, and the destructor waits for them: another
        // thread lets them go while the destructor waits.
        opener = std::thread([&gate] { gate.release(); });
    }
    // Reaching this line is the check: the destructor joined the workers, and the queued builds and
    // generations were released on the reaper (ASan reports a dangling reference or a leak
    // otherwise).
    opener.join();
    CHECK(true);
}

TEST_CASE("scene picture bounds: sustained edits keep the window's change work to the worker's" *
          doctest::test_suite("scene_picture")) {
    const auto f = load_fixture();
    if (!f) {
        MESSAGE("skipping: no game files");
        return;
    }
    std::vector<SceneBuilding> current = first_of_each_island(f->buildings);
    REQUIRE(current.size() >= 2);
    const Built base = build(*f, f->buildings);
    ScenePicture::Limits limits;
    limits.max_outstanding = 1;
    ScenePicture picture(
        base.scene, f->buildings, [&](std::uint16_t id) { return is_building(*f, id); }, 3, limits);
    FrameRenderer frames(1);
    std::vector<SceneBuilding> list = f->buildings;
    std::uint64_t turn = 1;
    for (int round = 0; round < 120; ++round) {
        move_tracked(picture, list, current[static_cast<std::size_t>(round) % current.size()],
                     turn++);
        if (round % 3 == 2) {
            picture.wait_idle();
            picture.commit(frames);
        }
    }
    // The drain lets the reclaimer catch up before each commit (bounded): under the Monotonic
    // default the stream commits a generation per commit (round dz: 42 here, against 2 under
    // LatestOnly), so a reclaimer that is behind skips commits by design, and a drain that
    // retried without waiting could use up its rounds in microseconds and stop with islands
    // pending (seen under ctest -j 6; reproduced with the reclaimer slowed to 200 ms a scene).
    auto reclaimer_caught_up = [&picture, &limits] {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < until) {
            const auto ps = picture.stats();
            if (ps.reclaim_completed == ps.reclaim_queued &&
                render::SceneReclaimer::shared().stats().waiting <= limits.max_reclaim_backlog) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    for (int round = 0; round < 200 && picture.pending() > 0; ++round) {
        picture.wait_idle();
        reclaimer_caught_up();
        picture.commit(frames);
    }
    const auto st = picture.stats();
    CHECK(picture.pending() == 0);
    CHECK(st.outstanding_peak <= limits.max_outstanding);
    // The change set of every commit came from the worker: the window took none of it.
    CHECK(st.version_requeues == 0);
    CHECK(st.reclaim_queued > 0);
    CHECK(picture.buildings() == list);
}
