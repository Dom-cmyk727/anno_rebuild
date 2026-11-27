// Shared between anno_render_tests (atlas_pages_corpus_test.cpp, the two .sze scenario scenes)
// and anno_app_tests (the data/maps/lan5.txt fixed-map case, which needs the app layer's own
// loader and so cannot live in anno_render_tests - review F2/orchestrator follow-up, "do not
// link anno_app_core into anno_render_tests; move the shared checker to a header both tests
// include"). Header-only (every function `inline`): test_support's own library stays free of a
// render/assets dependency (CLAUDE.md "Separate libraries by responsibility" - test_support sits
// below every test binary, including ones that know nothing about render), and only a
// translation unit that already links anno_render/anno_assets (both anno_render_tests and
// anno_app_tests, the latter transitively through anno_app_core) ever includes this header.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include <doctest/doctest.h>

#include <anno/assets/texture.hpp>
#include <anno/formats/figuren.hpp>
#include <anno/render/atlas_pages.hpp>
#include <anno/render/frame_table.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/scene.hpp>

namespace anno::test {

/// The static+water frames of `scene` at `group` (dedup via FrameTable + SpriteCache, the
/// engine's own decode path: assets::building::to_rgba, smooth_2x2 for the enhanced-water
/// variant - no second decoder). Mirrors RENDER-DIRECT-1a's own enumeration (prepare_objects,
/// animation()/frame_index(), the enhanced-water exclusion constants), decoding instead of only
/// sizing (RENDER-DIRECT-1b).
inline std::vector<render::AtlasFrameInput> collect_static_water(const render::Scene& scene,
                                                                 std::uint32_t group,
                                                                 render::SpriteCache& cache,
                                                                 render::FrameTable& table) {
    namespace fig = anno::formats::figuren;
    auto add_object = [&](const render::Prepared& p, bool enhanced_eligible) {
        const std::uint32_t id = p.first.anlage;
        const fig::Entry* entry = scene.anlagen.entry(id);
        if (entry == nullptr || entry->gfx.empty()) {
            return;
        }
        const fig::GfxSet& set = entry->gfx.front();
        const fig::FrameSet* frames = nullptr;
        for (const auto& fs : set.frame_sets) {
            if (fs.group == group) {
                frames = &fs;
                break;
            }
        }
        if (frames == nullptr) {
            return;
        }
        const std::size_t n = std::max<std::size_t>(render::animation(set).first, 1);
        const std::uint64_t ms = render::animation(set).second;
        const std::size_t total = frames->frames.size();
        const std::size_t rotations = std::max<std::size_t>(total / n, 1);
        const std::size_t block = (std::size_t{p.first.rotation} % rotations) * n;
        const std::size_t phase =
            id == render::kSurfAnlage
                ? (3 * std::size_t{p.first.y} + 5 * std::size_t{p.first.x}) % 32
                : std::size_t{p.first.phase()};
        const bool animated = ms != 0 && n > 1;
        const std::size_t step_count = animated ? n : 1;
        const std::size_t first_step = phase % n;
        for (std::size_t i = 0; i < step_count; ++i) {
            const std::size_t step = animated ? i : first_step;
            const std::size_t k = block + step;
            if (k >= total || frames->frames[k].length == 0) {
                continue;
            }
            const render::SpriteKey key{id, 0, false, group, k};
            table.intern_building(scene.anlagen, cache, key, frames->frames[k]);
            if (enhanced_eligible) {
                const render::SpriteKey smoothed{id, render::kSmoothed, false, group, k};
                table.intern_building(scene.anlagen, cache, smoothed, frames->frames[k]);
            }
        }
    };
    for (const auto& island : scene.islands) {
        const auto statics = render::prepare_objects(island, scene.anlagen, render::kObjectRecords,
                                                     render::Objects::NotWater);
        for (const auto& p : statics) {
            add_object(p, false);
        }
        const auto below = render::prepare_objects(island, scene.anlagen, render::kObjectRecords,
                                                   render::Objects::WaterBelowSea);
        for (const auto& p : below) {
            const std::uint32_t id = p.first.anlage;
            const bool is_fall =
                std::find(render::kRiverFallAnlagen.begin(), render::kRiverFallAnlagen.end(), id) !=
                render::kRiverFallAnlagen.end();
            add_object(p, id != render::kSurfAnlage && !is_fall);
        }
        const auto above = render::prepare_objects(island, scene.anlagen, render::kObjectRecords,
                                                   render::Objects::WaterAboveSea);
        for (const auto& p : above) {
            const std::uint32_t id = p.first.anlage;
            const bool is_river =
                std::find(render::kRiverAnlagen.begin(), render::kRiverAnlagen.end(), id) !=
                render::kRiverAnlagen.end();
            const bool is_fall =
                std::find(render::kRiverFallAnlagen.begin(), render::kRiverFallAnlagen.end(), id) !=
                render::kRiverFallAnlagen.end();
            add_object(p, !is_river && !is_fall);
        }
    }
    std::vector<render::AtlasFrameInput> out;
    for (render::FrameId id = 0; id < static_cast<render::FrameId>(table.size()); ++id) {
        if (const auto data = table.get(id)) {
            out.push_back(render::AtlasFrameInput{id, data->image});
        }
    }
    return out;
}

/// `img`'s own pixel at (x, y), premultiplied - the expectation every level-0 texel of its own
/// rectangle is checked against directly (review F2: every frame, not a sample).
inline std::array<std::uint8_t, 4> expected_level0(const assets::Rgba& img, std::uint32_t x,
                                                   std::uint32_t y) {
    const std::size_t si = (std::size_t{y} * img.width + x) * 4;
    return render::premultiply_pixel(
        {img.pixels[si + 0], img.pixels[si + 1], img.pixels[si + 2], img.pixels[si + 3]});
}

/// Every frame in `frames`, every level-0 texel of its own rectangle, and every texel of its own
/// mip rectangle, checked against the decoder directly (review F2: the packet's own step 4 asks
/// this of every frame, not a sample - the synthetic tests (atlas_pages_test.cpp) prove the rules
/// exhaustively on small cases, this proves no real frame is the exception).
inline void check_pages_against_source(const std::vector<render::AtlasFrameInput>& frames,
                                       const render::AtlasPages& pages, const char* what) {
    CHECK_MESSAGE(pages.oversized.empty(), what << ": no frame is too large for one page");
    CHECK_MESSAGE(pages.duplicate_ids.empty(), what << ": no frame id repeated");
    for (const auto& f : frames) {
        const render::AtlasFramePlacement* place = nullptr;
        for (const auto& p : pages.placements) {
            if (p.id == f.id) {
                place = &p;
                break;
            }
        }
        REQUIRE_MESSAGE(place != nullptr, what << ": frame " << f.id << " was not placed");
        REQUIRE(place->width == f.image->width);
        REQUIRE(place->height == f.image->height);
        const auto& l0 = pages.level0[place->layer];
        for (std::uint32_t y = 0; y < f.image->height; ++y) {
            for (std::uint32_t x = 0; x < f.image->width; ++x) {
                const std::size_t pi =
                    (std::size_t{place->y + y} * pages.page_size + (place->x + x)) * 4;
                const std::array<std::uint8_t, 4> got{l0[pi], l0[pi + 1], l0[pi + 2], l0[pi + 3]};
                CHECK(got == expected_level0(*f.image, x, y));
            }
        }
        // The mip rectangle, box-filtered from level 0 at the same slot (not from the frame's own
        // source pixels directly - the mip rule averages dilated padding too, at an odd
        // width/height's one-texel-wider mip edge, exactly the case review F4 found wrong). A mip
        // texel at absolute page coordinate (mx_abs, my_abs) is written from the level-0 2x2 box
        // at (2*mx_abs, 2*my_abs) (write_level1: slot_x is even, so slot_x/2 is exact, and this
        // holds for every mip texel of the whole slot, frame sub-rectangle included).
        const auto& l1 = pages.level1[place->layer];
        const std::uint32_t mip_stride = pages.page_size / 2;
        for (std::uint32_t my = 0; my < place->mip_height; ++my) {
            for (std::uint32_t mx = 0; mx < place->mip_width; ++mx) {
                const std::uint32_t mx_abs = place->mip_x + mx, my_abs = place->mip_y + my;
                const std::uint32_t l0x = 2 * mx_abs, l0y = 2 * my_abs; // absolute page coords
                std::array<std::uint8_t, 4> expected{};
                for (std::size_t c = 0; c < 4; ++c) {
                    unsigned s = 0;
                    for (const auto& [dx, dy] : {std::pair{0U, 0U}, {1U, 0U}, {0U, 1U}, {1U, 1U}}) {
                        // Frame-local coordinate of this level-0 texel (place->x/y is the frame's
                        // own absolute origin); clamped exactly as write_level0 clamps the
                        // dilated padding.
                        const std::int64_t fx = std::int64_t{l0x + dx} - place->x;
                        const std::int64_t fy = std::int64_t{l0y + dy} - place->y;
                        const auto cx = static_cast<std::uint32_t>(
                            std::clamp<std::int64_t>(fx, 0, std::int64_t{f.image->width} - 1));
                        const auto cy = static_cast<std::uint32_t>(
                            std::clamp<std::int64_t>(fy, 0, std::int64_t{f.image->height} - 1));
                        s += expected_level0(*f.image, cx, cy)[c];
                    }
                    expected[c] = static_cast<std::uint8_t>((s + 2) / 4);
                }
                const std::size_t mi = (std::size_t{my_abs} * mip_stride + mx_abs) * 4;
                const std::array<std::uint8_t, 4> got{l1[mi], l1[mi + 1], l1[mi + 2], l1[mi + 3]};
                CHECK(got == expected);
            }
        }
    }
    MESSAGE(what << ": " << frames.size() << " frames checked (every frame, every texel)");
}

} // namespace anno::test
