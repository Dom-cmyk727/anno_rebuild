// The chunk comparison of the building picture (round dx, DX1d; docs/perf/README.md "Verifier"). On
// a real edited view of the fixed map: one added building on island 0. The positive control must
// pass; each mutation must fail, and the mutations act on copies (nothing is restored because
// nothing is changed). Needs the game files (skips without them).
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include <doctest/doctest.h>

#include <anno/app/loader.hpp>
#include <anno/app/picture_check.hpp>
#include <anno/app/play.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>

namespace fs = std::filesystem;
namespace render = anno::render;
namespace app = anno::app;

namespace {

fs::path fixed_map() {
    return fs::path(ANNO_FIXTURES_DIR).parent_path().parent_path() / "data" / "maps" / "lan5.txt";
}

/// The fixed map loaded with `buildings` as its desired list (generation 0), rivers off (the
/// building check is the legacy look; river flow is a separate, asynchronous path with its own
/// tests).
std::shared_ptr<const render::Scene> scene_with(const fs::path& dir, const app::PlayLoad& play,
                                                const std::vector<app::SceneBuilding>& buildings) {
    app::SceneObjects objects;
    objects.is_building = [&play](std::uint16_t id) {
        return app::anlage_is_building(play.world, id);
    };
    objects.buildings = buildings;
    auto loaded = app::load_placed_scene(dir, play.name, play.placed, &objects);
    REQUIRE(loaded.ok());
    app::LoadedScene ls = std::move(loaded).value();
    ls.scene->river_cfg.enabled = false;
    return std::shared_ptr<const render::Scene>(std::move(ls.scene));
}

/// The chunks of `scene` in `view`, rendered synchronously (the independent CPU build).
render::FrameRenderer rendered(const render::Scene& scene, render::Rect view) {
    render::FrameRenderer frames(1);
    for (int round = 0; round < 3; ++round) {
        frames.update_chunks(scene, view, 2);
    }
    return frames;
}

/// One more building of `base` on island `island_index`: a free tile nearest the island's centre
/// (so the bounded view centred on the island contains it).
std::vector<app::SceneBuilding> with_one_more(const std::vector<app::SceneBuilding>& base,
                                              const render::Scene& scene,
                                              std::size_t island_index) {
    const auto& island = scene.islands[island_index];
    std::vector<app::SceneBuilding> out = base;
    app::SceneBuilding extra = base.front();
    extra.island = island.number;
    const int cx = island.width / 2, cy = island.height / 2;
    int best = -1;
    int best_x = 0, best_y = 0;
    for (int y = 0; y < island.height; ++y) {
        for (int x = 0; x < island.width; ++x) {
            const bool taken =
                std::any_of(out.begin(), out.end(), [&](const app::SceneBuilding& b) {
                    return b.island == island.number && b.x == x && b.y == y;
                });
            const int d = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (!taken && (best < 0 || d < best)) {
                best = d;
                best_x = x;
                best_y = y;
            }
        }
    }
    REQUIRE(best >= 0);
    extra.x = static_cast<std::uint16_t>(best_x);
    extra.y = static_cast<std::uint16_t>(best_y);
    out.push_back(extra);
    app::sort_buildings(out);
    return out;
}

app::ChunkLookup lookup_of(const render::FrameRenderer& frames) {
    return [&frames](const render::ChunkKey& key) { return frames.chunk(key); };
}

} // namespace

TEST_CASE("picture check: an added building changes objects, and an equal reference compares equal "
          "(game files)" *
          doctest::test_suite("picture_check")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    const app::PlayLoad play = std::move(loaded).value();
    std::vector<app::SceneBuilding> base = app::play_buildings(play.world);
    REQUIRE(!base.empty());
    const auto before = scene_with(*dir, play, base);
    const std::vector<app::SceneBuilding> edited = with_one_more(base, *before, 0);
    const auto after = scene_with(*dir, play, edited);

    const render::Rect view = [&] {
        for (const render::Placement& p : before->layouts[2]) {
            if (p.index == 0) {
                const int cx = p.bounds.x + p.bounds.w / 2, cy = p.bounds.y + p.bounds.h / 2;
                return render::Rect{cx - 512, cy - 384, 1024, 768};
            }
        }
        return render::Rect{};
    }();
    const auto keys = render::chunk_keys_touching(*before, view, 2);
    REQUIRE(!keys.empty());
    const render::FrameRenderer ref_before = rendered(*before, view);
    const render::FrameRenderer ref_after = rendered(*after, view);

    // The positive control: equal references compare equal, and the view has object pixels to
    // compare.
    const app::ChunkDiff same =
        app::compare_chunks(keys, lookup_of(ref_before), lookup_of(ref_before));
    CHECK(same.equal());
    CHECK(same.with_objects > 0);
    CHECK(same.missing == 0);
    // The edit is visible in the view: the added building changes some object pixels
    // (before/after).
    const app::ChunkDiff changed =
        app::compare_chunks(keys, lookup_of(ref_after), lookup_of(ref_before));
    CHECK(changed.objects > 0);
    CHECK(!changed.equal());
}

TEST_CASE("picture check: negative controls fail (erased objects, a shifted object, a swapped "
          "chunk, an omitted "
          "chunk, a stale chunk) (game files)" *
          doctest::test_suite("picture_check")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    const app::PlayLoad play = std::move(loaded).value();
    const std::vector<app::SceneBuilding> base = app::play_buildings(play.world);
    REQUIRE(!base.empty());
    const auto scene = scene_with(*dir, play, base);
    const render::Rect view = [&] {
        for (const render::Placement& p : scene->layouts[2]) {
            if (p.index == 0) {
                const int cx = p.bounds.x + p.bounds.w / 2, cy = p.bounds.y + p.bounds.h / 2;
                return render::Rect{cx - 512, cy - 384, 1024, 768};
            }
        }
        return render::Rect{};
    }();
    const auto keys = render::chunk_keys_touching(*scene, view, 2);
    REQUIRE(!keys.empty());
    const render::FrameRenderer reference = rendered(*scene, view);

    // The chunks of the reference with object pixels, in key order (the controls act on these).
    std::vector<render::ChunkKey> with_objects;
    for (const render::ChunkKey& key : keys) {
        const render::Chunk* c = reference.chunk(key);
        if (c != nullptr && !c->objects.empty()) {
            with_objects.push_back(key);
        }
    }
    REQUIRE(with_objects.size() >= 2);
    const render::ChunkKey target = with_objects.front();
    const render::ChunkKey other = with_objects.back();

    // C0: the positive control compares equal to itself.
    CHECK(app::compare_chunks(keys, lookup_of(reference), lookup_of(reference)).equal());

    // C1: the object layer of one chunk is erased.
    {
        render::Chunk erased = *reference.chunk(target);
        erased.objects.assign(erased.objects.size(), render::Px{});
        const app::ChunkLookup candidate = [&](const render::ChunkKey& k) {
            return k == target ? &erased : reference.chunk(k);
        };
        const app::ChunkDiff d = app::compare_chunks(keys, candidate, lookup_of(reference));
        CHECK(d.objects > 0);
        CHECK(!d.equal());
    }
    // C2: one object moves by one pixel (the same number of object pixels, shifted).
    {
        render::Chunk shifted = *reference.chunk(target);
        std::rotate(shifted.objects.begin(), shifted.objects.begin() + 1, shifted.objects.end());
        const app::ChunkLookup candidate = [&](const render::ChunkKey& k) {
            return k == target ? &shifted : reference.chunk(k);
        };
        const app::ChunkDiff d = app::compare_chunks(keys, candidate, lookup_of(reference));
        CHECK(d.objects > 0);
        CHECK(!d.equal());
    }
    // C3: a count-preserving substitution: one chunk shows another chunk's objects (a per-island
    // swap).
    {
        render::Chunk swapped = *reference.chunk(target);
        if (reference.chunk(other)->objects != swapped.objects) {
            swapped.objects = reference.chunk(other)->objects;
            const app::ChunkLookup candidate = [&](const render::ChunkKey& k) {
                return k == target ? &swapped : reference.chunk(k);
            };
            const app::ChunkDiff d = app::compare_chunks(keys, candidate, lookup_of(reference));
            CHECK(d.objects > 0);
            CHECK(!d.equal());
        }
    }
    // C4: a required chunk is omitted.
    {
        const app::ChunkLookup candidate = [&](const render::ChunkKey& k) {
            return k == target ? nullptr : reference.chunk(k);
        };
        const app::ChunkDiff d = app::compare_chunks(keys, candidate, lookup_of(reference));
        CHECK(d.missing > 0);
        CHECK(!d.equal());
    }
    // C5: a stale chunk: the view as it was before one more building (the edit is not in it).
    {
        const std::vector<app::SceneBuilding> edited = with_one_more(base, *scene, 0);
        const auto newer = scene_with(*dir, play, edited);
        const render::FrameRenderer newer_ref = rendered(*newer, view);
        const app::ChunkDiff d =
            app::compare_chunks(keys, lookup_of(reference), lookup_of(newer_ref));
        CHECK(d.objects > 0);
        CHECK(!d.equal());
    }
    // The reference itself is unchanged by the controls (they acted on copies).
    CHECK(app::compare_chunks(keys, lookup_of(reference), lookup_of(reference)).equal());
}

TEST_CASE("picture check: a moved or rotated building is a different island, with the same count "
          "(game files)" *
          doctest::test_suite("picture_check")) {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    auto loaded = app::load_play(*dir, fixed_map());
    REQUIRE(loaded.ok());
    const app::PlayLoad play = std::move(loaded).value();
    const std::vector<app::SceneBuilding> base = app::play_buildings(play.world);
    REQUIRE(!base.empty());
    const auto scene = scene_with(*dir, play, base);
    const std::vector<app::BuildingItem> want = app::building_items(scene->islands[0]);
    REQUIRE(!want.empty());
    // The same island with one building moved to a free neighbouring tile (the count is unchanged).
    std::vector<app::SceneBuilding> moved = base;
    for (app::SceneBuilding& b : moved) {
        if (b.island == scene->islands[0].number) {
            b.x = static_cast<std::uint16_t>(b.x + 1);
            break;
        }
    }
    app::sort_buildings(moved);
    const auto moved_scene = scene_with(*dir, play, moved);
    const auto got_moved = app::building_items(moved_scene->islands[0]);
    CHECK(got_moved.size() == want.size());
    CHECK(got_moved != want);
    // The same island with one building rotated (the count is unchanged).
    std::vector<app::SceneBuilding> rotated = base;
    for (app::SceneBuilding& b : rotated) {
        if (b.island == scene->islands[0].number) {
            b.rotation = static_cast<std::uint8_t>((b.rotation + 1) % 4);
            break;
        }
    }
    app::sort_buildings(rotated);
    const auto rotated_scene = scene_with(*dir, play, rotated);
    const auto got_rotated = app::building_items(rotated_scene->islands[0]);
    CHECK(got_rotated.size() == want.size());
    CHECK(got_rotated != want);
    // The unchanged island compares equal to itself.
    CHECK(app::building_items(scene->islands[0]) == want);
}
