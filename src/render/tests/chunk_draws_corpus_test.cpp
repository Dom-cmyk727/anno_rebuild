// The chunk draw list against the game files (ANNO1503_GAME_DIR; skips
// otherwise): docs/render/gpu-chunks.md 7.1.
//
// For every chunk of Single_01.sze, Single_24.sze and the canyon island
// D_256x256_0045_5B.scp, at every zoom group, the replay of the chunk's draw
// list equals what Scene::render composites for Layers::objects (the code that
// produced the objects and base planes of render_chunk before the list
// existed), byte for byte. That is the proof that the list carries the order,
// the island clipping, the shadows, the base plane and the soldiers.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/figure.hpp>
#include <anno/assets/unit.hpp>
#include <anno/formats/anlagen.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/sze.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/chunk_draws.hpp>
#include <anno/render/frame.hpp>
#include <anno/render/scene.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/parallel.hpp>

#include "chunk_draws_model.hpp"
#include "corpus_scene.hpp"

namespace render = anno::render;
namespace fig = anno::formats::figuren;
using render::Rect;

namespace {

using anno::render::test::busy_chunks;
using anno::render::test::Game;
using anno::render::test::kScenes;
using anno::render::test::SceneFile;

/// What render_chunk's objects layer was before the draw list: Scene::render of
/// Layers::objects.
render::Canvas old_objects(const render::Scene& scene, Rect rect, std::size_t group, bool ships) {
    render::Layers layers = render::Layers::none();
    layers.objects = true;
    layers.ships = ships;
    layers.bases = !ships;
    return scene.render(rect, group, layers).first;
}

struct Outcome {
    bool pixels = true, base = true;
    std::size_t records = 0, missing = 0;
    bool binned_ok = true;
    /// Every clip table entry lies inside the chunk (island bounds intersected
    /// with it, gpu-chunks.md 3.3).
    bool clips_ok = true;
    std::string detail;
};

Outcome compare(const render::Scene& scene, Rect rect, std::size_t group, bool ships, bool binned) {
    Outcome o;
    const render::Canvas want = old_objects(scene, rect, group, ships);
    const render::ChunkDraws draws = scene.objects_draws(rect, group, ships, !ships);
    o.records = draws.object_count();
    for (const Rect& c : draws.clips) {
        o.clips_ok = o.clips_ok && c.w > 0 && c.h > 0 && c.x >= rect.x && c.y >= rect.y &&
                     c.x + c.w <= rect.x + rect.w && c.y + c.h <= rect.y + rect.h;
    }
    const render::Canvas got = render::replay(draws, scene.frame_table, &o.missing);
    o.pixels = got.pixels == want.pixels && got.width == want.width && got.height == want.height &&
               got.origin == want.origin;
    o.base = got.base == want.base;
    if (binned && o.records > 0) {
        std::vector<render::Px> px;
        std::vector<std::uint8_t> base;
        render::test::replay_binned(draws, scene.frame_table, px, base);
        for (std::size_t i = 0; i < px.size() && o.binned_ok; ++i) {
            const render::Px p{want.pixels[i * 4], want.pixels[i * 4 + 1], want.pixels[i * 4 + 2],
                               want.pixels[i * 4 + 3]};
            o.binned_ok = px[i] == p && (want.base.empty() || base[i] == want.base[i]);
            if (!o.binned_ok) {
                char buf[256];
                std::snprintf(
                    buf, sizeof buf,
                    "chunk (%d,%d) px (%zu,%zu) model %d,%d,%d,%d base %d, want %d,%d,%d,%d "
                    "base %d",
                    rect.x / render::kChunk, rect.y / render::kChunk, i % want.width,
                    i / want.width, px[i][0], px[i][1], px[i][2], px[i][3], base[i], p[0], p[1],
                    p[2], p[3], want.base.empty() ? -1 : want.base[i]);
                o.detail = buf;
            }
        }
    }
    return o;
}

} // namespace

TEST_CASE("chunk draw lists replay to the objects and base planes of Scene::render, every chunk "
          "(game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const Game game(*dir);
    for (const SceneFile& sf : kScenes) {
        const auto scene = game.scene(*dir / sf.path);
        for (std::size_t group = 0; group < render::kGroups; ++group) {
            const auto chunks = busy_chunks(*scene, group);
            // Ships left out (the GPU compositor's chunks, with bases) and ships
            // in (the CPU frame cache, no bases); the second only where there are
            // ships to draw.
            const bool with_ships = std::any_of(scene->units.begin(), scene->units.end(),
                                                [](const render::UnitSprite& u) { return !u.z; });
            const std::size_t variants = with_ships ? 2 : 1;
            const auto results =
                anno::test::parallel_indexed(chunks.size() * variants, [&](std::size_t i) {
                    const auto [cx, cy] = chunks[i / variants];
                    const bool ships = i % variants == 1;
                    // The binned model is a per-pixel walk: every 8th chunk.
                    return compare(*scene, render::chunk_rect(cx, cy), group, ships,
                                   (i / variants) % 8 == 0);
                });
            std::size_t bad_pixels = 0, bad_base = 0, records = 0, missing = 0, bad_binned = 0,
                        bad_clips = 0;
            std::string first_detail;
            for (const Outcome& o : results) {
                bad_pixels += o.pixels ? 0U : 1U;
                bad_base += o.base ? 0U : 1U;
                bad_binned += o.binned_ok ? 0U : 1U;
                bad_clips += o.clips_ok ? 0U : 1U;
                if (!o.binned_ok && first_detail.empty()) {
                    first_detail = o.detail;
                }
                records += o.records;
                missing += o.missing;
            }
            INFO(std::string_view(sf.name) << " group " << group);
            MESSAGE(std::string_view(sf.name)
                    << " g" << group << ": " << chunks.size() << " chunks x " << variants << ", "
                    << records << " records, " << scene->frame_table.size() << " frames");
            CHECK(bad_pixels == 0);
            CHECK(bad_base == 0);
            CHECK_MESSAGE(bad_binned == 0, first_detail);
            CHECK(bad_clips == 0);
            CHECK(missing == 0);
            CHECK(records > 0);
        }
    }
}

TEST_CASE("render_chunk: the Cpu builder keeps the planes, GpuList keeps the list (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const Game game(*dir);
    for (const SceneFile& sf : kScenes) {
        const auto scene = game.scene(*dir / sf.path);
        for (std::size_t group = 0; group < render::kGroups; ++group) {
            // Every 11th chunk that holds an island (the rest is what the other
            // test covers cheaply).
            std::vector<std::pair<int, int>> chunks;
            std::size_t k = 0;
            for (const auto& [cx, cy] : busy_chunks(*scene, group)) {
                const Rect rect = render::chunk_rect(cx, cy);
                const bool island =
                    std::any_of(scene->layouts[group].begin(), scene->layouts[group].end(),
                                [&](const render::Placement& p) {
                                    return p.bounds.intersect(rect).has_value();
                                });
                if (island && k++ % 11 == 0) {
                    chunks.emplace_back(cx, cy);
                }
            }
            const bool with_ships = std::any_of(scene->units.begin(), scene->units.end(),
                                                [](const render::UnitSprite& u) { return !u.z; });
            struct R {
                bool cpu_objects = true, cpu_base = true, list_replays = true, list_empty = true,
                     builder_tag = true;
                bool has_land = false;
            };
            const auto results =
                anno::test::parallel_indexed(chunks.size() * 2, [&](std::size_t i) {
                    const auto [cx, cy] = chunks[i / 2];
                    const bool ships = i % 2 == 1 && with_ships;
                    const Rect rect = render::chunk_rect(cx, cy);
                    // What the old render_chunk_data did with Scene::render's canvas.
                    const render::Canvas old = old_objects(*scene, rect, group, ships);
                    std::vector<render::Px> want_objects(old.width * old.height);
                    for (std::size_t p = 0; p < want_objects.size(); ++p) {
                        want_objects[p] = {old.pixels[p * 4], old.pixels[p * 4 + 1],
                                           old.pixels[p * 4 + 2], old.pixels[p * 4 + 3]};
                    }
                    std::vector<std::uint8_t> want_base = old.base;
                    if (std::all_of(want_objects.begin(), want_objects.end(),
                                    [](const render::Px& p) { return p[3] == 0; })) {
                        want_objects.clear();
                        want_base.clear();
                    }
                    R r;
                    render::ChunkBuildOptions o;
                    o.ships = ships;
                    const render::Chunk cpu = render::render_chunk(*scene, group, cx, cy, o);
                    r.cpu_objects = cpu.objects == want_objects;
                    r.cpu_base = cpu.base == want_base;
                    r.builder_tag = cpu.built_by == render::ChunkBuilder::Cpu && !cpu.draws;
                    r.has_land = !cpu.ground.empty();
                    o.builder = render::ChunkBuilder::GpuList;
                    const render::Chunk list = render::render_chunk(*scene, group, cx, cy, o);
                    r.builder_tag = r.builder_tag && list.built_by == render::ChunkBuilder::GpuList;
                    r.list_empty = list.objects.empty() && list.base.empty();
                    if (list.draws) {
                        const render::Canvas c = render::replay(*list.draws, scene->frame_table);
                        r.list_replays = c.pixels == old.pixels && c.base == old.base;
                    } else {
                        r.list_replays = want_objects.empty();
                    }
                    // The same chunk by both builders agrees on everything else.
                    r.list_replays = r.list_replays && list.ground == cpu.ground &&
                                     list.field == cpu.field && list.terrain == cpu.terrain;
                    return r;
                });
            std::size_t n = 0, bad = 0;
            for (const R& r : results) {
                ++n;
                bad +=
                    r.cpu_objects && r.cpu_base && r.list_replays && r.list_empty && r.builder_tag
                        ? 0U
                        : 1U;
            }
            INFO(std::string_view(sf.name) << " group " << group);
            MESSAGE(std::string_view(sf.name)
                    << " g" << group << ": " << n << " render_chunk pairs");
            CHECK(n > 0);
            CHECK(bad == 0);
        }
    }
}

TEST_CASE("FrameRenderer with the GpuList builder: workers and the in-frame path (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    const Game game(*dir);
    const auto scene = game.scene(*dir / kScenes[2].path);
    const std::size_t group = 1;
    const auto b = *scene->bounds(group);
    const Rect view{b.x + b.w / 2 - 600, b.y + b.h / 2 - 400, 1200, 800};
    for (const bool background : {false, true}) {
        render::FrameRenderer frames(2, false);
        frames.chunk_builder = render::ChunkBuilder::GpuList;
        frames.bin_px = 64;
        if (background) {
            frames.set_workers(std::make_unique<render::ChunkWorkers>(*scene, 2));
        }
        const auto t0 = std::chrono::steady_clock::now();
        for (;;) {
            const auto [built, ms, missing] = frames.update_chunks(*scene, view, group);
            (void)built;
            (void)ms;
            if (missing == 0 && !frames.busy()) {
                break;
            }
            REQUIRE(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(120));
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        std::size_t with_draws = 0;
        for (const auto& [cx, cy] : render::visible_chunks(view)) {
            const render::Chunk* c = frames.chunk({group, cx, cy});
            REQUIRE(c != nullptr);
            CHECK(c->built_by == render::ChunkBuilder::GpuList);
            CHECK(c->objects.empty());
            CHECK(c->base.empty());
            if (c->draws) {
                ++with_draws;
                CHECK(c->draws->bin_px == 64);
                CHECK(c->draws->rect == render::chunk_rect(cx, cy));
                // The replay is the Cpu builder's plane.
                const render::Chunk cpu = render::render_chunk(*scene, group, cx, cy, false, true);
                const render::Canvas r = render::replay(*c->draws, scene->frame_table);
                if (cpu.objects.empty()) {
                    // The Cpu builder drops a layer that is all transparent.
                    CHECK(std::all_of(r.pixels.begin(), r.pixels.end(),
                                      [](std::uint8_t v) { return v == 0; }));
                } else {
                    std::vector<std::uint8_t> flat;
                    for (std::size_t i = 0; i < cpu.objects.size(); ++i) {
                        flat.insert(flat.end(), cpu.objects[i].begin(), cpu.objects[i].end());
                    }
                    CHECK(r.pixels == flat);
                    CHECK(r.base == cpu.base);
                }
            }
        }
        CHECK(with_draws > 0);
    }
}
