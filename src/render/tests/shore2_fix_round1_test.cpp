// SHORE-2 fix round 1 (orchestrator review, 2026-11-14): F3 and F4's measurements.
//
// F3 asks for the river cells the sea covers at shore 26.5 against 16.5, over the
// river corpus, with both the old chunk-gate rule (corner average <= kLandAverage,
// replaced by SHORE-2) and the new one (any corner below shore_height). F4 asks for
// the tiles that get the sea bit only through the new rule, away from an island's
// own sea border, as a check on "the new rule may over-draw sea in inland basins"
// (docs/open-questions.md OQ-026).
//
// Approximation (both items): tile-level, not per-pixel. A "river cell" is a wet
// cell of Scene::river_mask, mapped to its containing tile; a tile's "terrain" is
// the average of its four corner heights (the same proxy the old classifier used),
// not the smooth, noise-carved height the renderer actually samples per pixel. Both
// rules' chunk gate is exact (IslandWater::build's own logic for the new rule; the
// literal removed condition, reimplemented here, for the old one). "Not connected to
// the sea" (F4) is a 4-connected flood fill, at chunk granularity, of the new rule's
// sea chunks starting from the island's own bounding-rectangle edge (open sea or the
// shore directly against it): a new-rule sea chunk the flood never reaches is an
// enclosed basin, not merely far from that edge in a straight line — the first
// version of this test used edge distance alone (kBorderFadeTiles) and over-counted
// ordinary gently-sloping coastal interior by more than 50x; this version replaces
// that with the flood fill.
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <anno/formats/anlagen.hpp>
#include <anno/formats/insel.hpp>
#include <anno/formats/tex.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/river.hpp>
#include <anno/render/scene.hpp>
#include <anno/render/sea.hpp>
#include <anno/test/game_dir.hpp>
#include <anno/test/parallel.hpp>

namespace render = anno::render;

namespace {

bool old_rule_chunk(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h,
                    std::size_t x, std::size_t y) {
    const auto c = render::corners(heights, w, h, x, y);
    return c[0] + c[1] + c[2] + c[3] <= 4 * render::kLandAverage;
}

std::vector<bool> build_old_chunk(const anno::formats::insel::Island& island, std::size_t cw,
                                  std::size_t ch) {
    std::vector<bool> out(cw * ch, false);
    auto hr = island.heights();
    if (!hr) {
        return out;
    }
    const auto heights = hr.value();
    for (std::size_t y = 0; y < island.height; ++y) {
        for (std::size_t x = 0; x < island.width; ++x) {
            if (old_rule_chunk(heights, island.width, island.height, x, y)) {
                out[(y / 8) * cw + x / 8] = true;
            }
        }
    }
    return out;
}

struct Row {
    std::size_t river_total = 0;
    std::size_t river_old_16 = 0, river_old_26 = 0, river_new_16 = 0, river_new_26 = 0;
    std::size_t basin_only_new = 0;
    std::string basin_sample; // "file#island(x,y)"
    // F5: the stepping itself, at shore 26.5 (avg corner height < 26.5 but the chunk's own
    // gate is false): the old rule's chunk gate ("corner average <= kLandAverage"), independent
    // of shore_height, misses it whenever 19 < avg < 26.5 (every "low beach" chunk DEPTH-
    // INTEGRATE-1's review called out); the new rule's gate ("any corner < shore_height") cannot
    // miss it by construction (avg < 26.5 implies at least one corner < 26.5), so stepping_new
    // must land at exactly 0.
    std::size_t stepping_old = 0, stepping_new = 0, stepping_total = 0;
};

} // namespace

TEST_CASE("SHORE-2 fix round 1: river coverage at 26.5 vs 16.5, basin-only sea tiles, old rule "
          "vs new rule (game files)") {
    const auto dir = anno::test::game_dir();
    if (!dir) {
        MESSAGE("skipping: ANNO1503_GAME_DIR not set");
        return;
    }
    std::vector<std::filesystem::path> files;
    for (const char* climate : {"Dschungel", "Nord", "Praerie", "Steppe", "Tundra"}) {
        for (const auto& p :
             anno::test::files_with_ext(*dir / "Scenes" / "Islands" / climate, "scp")) {
            files.push_back(p);
        }
    }
    for (const char* folder : {"0200Single", "0201SingleAddOn"}) {
        for (const auto& p : anno::test::files_with_ext(*dir / "Scenes" / folder, "sze")) {
            files.push_back(p);
        }
    }
    const auto felder_buf = anno::test::read_file(*dir / "Textures" / "Felder.tex");
    const auto anl_buf = anno::test::read_file(*dir / "Anlagen.dat");
    const auto per_file = anno::test::parallel_indexed(files.size(), [&](std::size_t fi) {
        Row row;
        const auto buf = anno::test::read_file(files[fi]);
        auto anlagen = anno::formats::anlagen::AnlagenFile::parse(anl_buf).value();
        const auto felder = anno::formats::tex::TexFile::parse(felder_buf).value();
        auto textures = anno::assets::island::GroundTextures::load(felder).value();
        if (const auto c = render::water_colour(anlagen)) {
            textures.set_water(*c);
        }
        const render::Scene scene("", anno::formats::insel::islands(buf).value(),
                                  std::move(textures), std::move(anlagen), render::kHeightLift);
        render::river::RiverConfig cfg;
        cfg.log_fallback = false;
        for (std::size_t i = 0; i < scene.islands.size(); ++i) {
            const auto& island = scene.islands[i];
            const std::size_t cw = (island.width + 7) / 8, ch = (island.height + 7) / 8;
            const auto old_chunk = build_old_chunk(island, cw, ch);
            const auto new_16 = render::sea::IslandWater::build(
                island, scene.river_pieces(i), scene.river_cfg.gap_patch_tiles, 16.5F);
            const auto new_26 = render::sea::IslandWater::build(
                island, scene.river_pieces(i), scene.river_cfg.gap_patch_tiles, 26.5F);
            if (!new_16 || !new_26) {
                continue;
            }
            // F4: tiles in a new-rule sea chunk that the old rule did not flag and that a
            // 4-connected flood of new-rule sea chunks, seeded from the island's own
            // bounding-rectangle edge, never reaches — an enclosed basin, not merely an
            // ordinary stretch of low interior connected to the sea by a path of sea chunks.
            auto gr = island.gfx_ids();
            auto hr = island.heights();
            if (gr && hr) {
                const auto gfx = gr.value();
                std::vector<bool> reached(cw * ch, false);
                std::vector<std::size_t> queue;
                for (std::size_t cy = 0; cy < ch; ++cy) {
                    for (std::size_t cx = 0; cx < cw; ++cx) {
                        const bool border = cx == 0 || cy == 0 || cx == cw - 1 || cy == ch - 1;
                        const std::size_t idx = cy * cw + cx;
                        if (border && new_26->sea_chunk[idx] && !reached[idx]) {
                            reached[idx] = true;
                            queue.push_back(idx);
                        }
                    }
                }
                for (std::size_t qi = 0; qi < queue.size(); ++qi) {
                    const std::size_t idx = queue[qi];
                    const std::size_t cx = idx % cw, cy = idx / cw;
                    const std::ptrdiff_t dxs[4] = {-1, 1, 0, 0};
                    const std::ptrdiff_t dys[4] = {0, 0, -1, 1};
                    for (int d = 0; d < 4; ++d) {
                        const std::ptrdiff_t nx = static_cast<std::ptrdiff_t>(cx) + dxs[d];
                        const std::ptrdiff_t ny = static_cast<std::ptrdiff_t>(cy) + dys[d];
                        if (nx < 0 || ny < 0 || static_cast<std::size_t>(nx) >= cw ||
                            static_cast<std::size_t>(ny) >= ch) {
                            continue;
                        }
                        const std::size_t nidx =
                            static_cast<std::size_t>(ny) * cw + static_cast<std::size_t>(nx);
                        if (new_26->sea_chunk[nidx] && !reached[nidx]) {
                            reached[nidx] = true;
                            queue.push_back(nidx);
                        }
                    }
                }
                for (std::size_t y = 0; y < island.height; ++y) {
                    for (std::size_t x = 0; x < island.width; ++x) {
                        const std::size_t chunk_idx = (y / 8) * cw + x / 8;
                        // F5: the stepping itself (avg corner height below the plane, but the
                        // chunk's own gate false).
                        const auto corner =
                            render::corners(hr.value(), island.width, island.height, x, y);
                        const float avg =
                            static_cast<float>(corner[0] + corner[1] + corner[2] + corner[3]) /
                            4.0F;
                        if (avg < 26.5F) {
                            ++row.stepping_total;
                            if (!old_chunk[chunk_idx]) {
                                ++row.stepping_old;
                            }
                            if (!new_26->sea_chunk[chunk_idx]) {
                                ++row.stepping_new;
                            }
                        }
                        if (!new_26->sea_chunk[chunk_idx] || old_chunk[chunk_idx] ||
                            reached[chunk_idx]) {
                            continue;
                        }
                        const auto cell = anno::formats::insel::GroundCell::from_gfx_id(
                            gfx[y * island.width + x]);
                        if (cell.texture == 0) {
                            continue; // open water itself, not a basin
                        }
                        ++row.basin_only_new;
                        if (row.basin_sample.empty()) {
                            row.basin_sample = files[fi].stem().string() + "#" + std::to_string(i) +
                                               "(" + std::to_string(x) + "," + std::to_string(y) +
                                               ")";
                        }
                    }
                }
            }
            // F3: river cells the sea covers at 16.5 vs 26.5, old rule vs new rule.
            if (scene.river_piece_count(i) == 0 || !hr) {
                continue;
            }
            const auto heights = hr.value();
            const render::river::RiverMask mask = scene.river_mask(i, cfg);
            const std::int32_t c = static_cast<std::int32_t>(mask.cells_per_tile());
            if (c <= 0) {
                continue;
            }
            for (std::int32_t acy = mask.y0();
                 acy < mask.y0() + static_cast<std::int32_t>(mask.ny()); ++acy) {
                for (std::int32_t acx = mask.x0();
                     acx < mask.x0() + static_cast<std::int32_t>(mask.nx()); ++acx) {
                    if (!mask.wet(acx, acy)) {
                        continue;
                    }
                    const std::int32_t tx = acx >= 0 ? acx / c : -1, ty = acy >= 0 ? acy / c : -1;
                    if (tx < 0 || ty < 0 || static_cast<std::size_t>(tx) >= island.width ||
                        static_cast<std::size_t>(ty) >= island.height) {
                        continue;
                    }
                    ++row.river_total;
                    const std::size_t ux = static_cast<std::size_t>(tx),
                                      uy = static_cast<std::size_t>(ty);
                    const std::size_t chunk_idx = (uy / 8) * cw + ux / 8;
                    const auto corner =
                        render::corners(heights, island.width, island.height, ux, uy);
                    const float avg =
                        static_cast<float>(corner[0] + corner[1] + corner[2] + corner[3]) / 4.0F;
                    if (old_chunk[chunk_idx] && avg < 16.5F) {
                        ++row.river_old_16;
                    }
                    if (old_chunk[chunk_idx] && avg < 26.5F) {
                        ++row.river_old_26;
                    }
                    if (new_16->sea_chunk[chunk_idx] && avg < 16.5F) {
                        ++row.river_new_16;
                    }
                    if (new_26->sea_chunk[chunk_idx] && avg < 26.5F) {
                        ++row.river_new_26;
                    }
                }
            }
        }
        return std::vector<Row>{row};
    });
    std::size_t river_total = 0, river_old_16 = 0, river_old_26 = 0, river_new_16 = 0,
                river_new_26 = 0, basin_only_new = 0, stepping_old = 0, stepping_new = 0,
                stepping_total = 0;
    std::string basin_sample;
    for (const auto& rows : per_file) {
        for (const Row& r : rows) {
            river_total += r.river_total;
            river_old_16 += r.river_old_16;
            river_old_26 += r.river_old_26;
            river_new_16 += r.river_new_16;
            river_new_26 += r.river_new_26;
            basin_only_new += r.basin_only_new;
            stepping_old += r.stepping_old;
            stepping_new += r.stepping_new;
            stepping_total += r.stepping_total;
            if (basin_sample.empty() && !r.basin_sample.empty()) {
                basin_sample = r.basin_sample;
            }
        }
    }
    MESSAGE("river cells (tile-level, "
            << river_total << " total): old rule 16.5=" << river_old_16 << " 26.5=" << river_old_26
            << " (delta " << (river_old_26 - river_old_16) << "); new rule 16.5=" << river_new_16
            << " 26.5=" << river_new_26 << " (delta " << (river_new_26 - river_new_16) << ")");
    MESSAGE("basin-only tiles (new rule sets the chunk bit, old rule didn't, not open water, "
            "unreachable by a sea-chunk flood from the island's own edge): "
            << basin_only_new << (basin_sample.empty() ? "" : (", e.g. " + basin_sample)));
    MESSAGE("F5 stepping (avg corner height < 26.5 but the tile's own chunk gate false, of "
            << stepping_total << " tiles below the plane): old rule " << stepping_old
            << ", new rule " << stepping_new);
    CHECK(river_total > 0);
    CHECK(stepping_new == 0); // by construction: avg < 26.5 implies a corner < 26.5
}
