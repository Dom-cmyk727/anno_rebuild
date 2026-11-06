// The placement rules of Build (docs/sim/state.md §8.4 steps 2 and 3, stage S10).
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/placement.hpp>
#include <anno/sim/site_checks.hpp>
#include <anno/sim/terrain_view.hpp>

namespace anno::sim {

namespace {

std::uint32_t group_of(std::uint32_t prodtype) {
    return ((prodtype - 1U) & ~0x1fU) + 1U;
}

enum class Class : std::uint8_t { Ordinary, Coast, Road, Mountain, Other };

Class classify(const BuildingType& t, const PlacementParams& p) {
    // The original's flag word is HEAD u16 +0x1c; a wider word is not evidenced (Other).
    if (t.flags_word > 0xffffU) {
        return Class::Other;
    }
    // Stream 135: the mountain class (flag word & 0x7000 == 0x3000) is placeable only for a deep MINE
    // type that carries its HIGHMAP table (exactly 2h x 2w bytes); every other type of the class
    // (quarries, shallow iron mines, a type without a table) stays Unsupported. The shipped mines
    // carry flag bit 0x10, so this branch precedes the gate below: their coverage test (a second
    // predicate, docs/sim/salt-extraction.md section 18) is implemented in coverage_rule.
    if ((t.flags_word & 0x7000U) == p.mountain_class && t.cls == RuntimeClass::Mine && t.has_prod &&
        mine_is_deep(t.prod, t.research_id) &&
        t.highmap.size() == std::size_t{4} * t.width * t.height && (t.flags_word & 3U) != 1U &&
        !t.prod.outputs.empty() &&
        std::find(p.mountain_wares.begin(), p.mountain_wares.end(), t.prod.outputs[0]) !=
            p.mountain_wares.end()) {
        return Class::Mountain;
    }
    // Every other type with the coverage bit stays Unsupported (the test is evidenced for the mountain
    // class only).
    if ((t.flags_word & p.coverage_flag) != 0) {
        return Class::Other;
    }
    if (is_land_road(t)) {
        return Class::Road;
    }
    if ((t.flags_word & ~p.ignored_flag_bits) == p.ordinary_class) {
        return Class::Ordinary;
    }
    if (site::coast_class_supported(t.flags_word)) {
        return Class::Coast;
    }
    return Class::Other;
}

/// The vertex window of a footprint and its level analysis (AnnoGame 0x10008f90, docs/sim/ship-
/// placement.md section 3): the (fw + 1) x (fh + 1) vertex bytes, their maximum, and the lowest and
/// highest cell minimum (each cell's four vertices, `& 0xf0`). Shared by the land road and the
/// mountain class (stream 135: extracted from road_window, behaviour unchanged). nullopt when a
/// vertex cannot be read.
struct WindowLevels {
    std::vector<std::uint8_t> v; ///< row-major, stride fw + 1
    unsigned mx = 0, lo = 0xff, hi = 0;
};

std::optional<WindowLevels> read_window(const VertexGrid& grid, const PlacementRecord& rec,
                                        std::int64_t fw, std::int64_t fh) {
    const auto w = static_cast<std::size_t>(fw + 1);
    WindowLevels out;
    out.v.resize(w * static_cast<std::size_t>(fh + 1));
    for (std::int64_t dy = 0; dy <= fh; ++dy) {
        for (std::int64_t dx = 0; dx <= fw; ++dx) {
            const std::optional<std::uint8_t> at = grid.at(rec.x + dx, rec.y + dy);
            if (!at) {
                return std::nullopt;
            }
            out.v[static_cast<std::size_t>(dy) * w + static_cast<std::size_t>(dx)] = *at;
            out.mx = std::max<unsigned>(out.mx, *at);
        }
    }
    for (std::size_t y = 0; y < static_cast<std::size_t>(fh); ++y) {
        for (std::size_t x = 0; x < static_cast<std::size_t>(fw); ++x) {
            const unsigned cell = std::min({out.v[y * w + x], out.v[y * w + x + 1],
                                            out.v[(y + 1) * w + x], out.v[(y + 1) * w + x + 1]}) &
                                  0xf0U;
            out.lo = std::min(out.lo, cell);
            out.hi = std::max(out.hi, cell);
        }
    }
    return out;
}

/// The window result by the flag word's mode (flag word & 3: 0 hi, 2 lo, 3 the maximum & 0xf0).
unsigned mode_result(const WindowLevels& win, std::uint32_t flags_word) {
    const std::uint32_t mode = flags_word & 3U;
    return mode == 0 ? win.hi : mode == 2 ? win.lo : (win.mx & 0xf0U);
}

/// The window result of a land road (AnnoGame 0x10008f90 for flag word & 3 in {0, 2, 3}), the slope
/// test of the slope class, and the level floor. Unsupported when a vertex cannot be read.
PlacementResult road_window(const VertexGrid& grid, const BuildingType& t,
                            const PlacementRecord& rec, std::int64_t fw, std::int64_t fh,
                            const PlacementParams& params, const RoadParams& road) {
    constexpr PlacementResult kUnsupported{Reject::Unsupported, 0};
    const std::optional<WindowLevels> win = read_window(grid, rec, fw, fh);
    if (!win) {
        return kUnsupported;
    }
    const unsigned mx = win->mx, lo = win->lo;
    const unsigned result = mode_result(*win, t.flags_word);
    // The class test runs only with flag bit 4 (A & 0x1000004 == 4, AnnoGame 0x10009111); a
    // data-defined slope-class road without that bit has no slope test, like the original.
    if ((t.flags_word & 4U) != 0 && (t.flags_word & 0x7000U) == road.slope_class &&
        mx - lo > road.slope_limit) {
        return {Reject::TerrainRefused, 0};
    }
    // The original needs a byte >= min_level for the slope class (0x1007cbeb) but accepts the road
    // class at once; the floor for the road class is the enhanced choice R-A2.
    const bool slope_class = (t.flags_word & 0x7000U) == road.slope_class;
    if (result < params.min_level && (slope_class || params.road_require_land_level)) {
        return {Reject::TerrainRefused, 0};
    }
    return {Reject::None, static_cast<std::uint8_t>(result)};
}

/// The island function's per-chunk COVERAGE test (AnnoWelt 0x1007c955-0x1007cb3d; docs/sim/
/// salt-extraction.md section 18; reference placement_ref.py coverage_ok, proved equal to the original
/// bytes by placement_emulate.py over 40,000 random cases and 31 hand-derived vectors). It runs for a
/// type whose flag word has bit 0x10. A footprint tile is KEPT when it carries an existing object whose
/// type has bit 0x10 AND whose tile anchor bit (TileLayer::foot_anchor, tile byte 3 bit 7) is set; every
/// other tile marks its world-aligned chunk; every chunk the footprint touches needs a mark.
/// Typed Unsupported (never a default answer) for what the evidence does not cover: a footprint wider
/// or taller than the chunk (the original's mark array is 2 x 2 chunks), a negative world coordinate
/// (the original's `cdq; and 7` rounding differs there), an existing id the registry does not know,
/// and an object of the dry run's overlay on the footprint (its anchor bit is not modelled).
PlacementResult coverage_rule(const Island& island, const TypeRegistry& types,
                              const PlacementRecord& rec, std::int64_t fw, std::int64_t fh,
                              const PlacementParams& params) {
    constexpr PlacementResult kUnsupported{Reject::Unsupported, 0};
    const std::int64_t edge = std::int64_t{1} << params.coverage_chunk_shift;
    if (fw > edge || fh > edge || rec.x < 0 || rec.y < 0) {
        return kUnsupported;
    }
    const TileLayer& tiles = island.tiles;
    const std::int64_t cx0 = std::int64_t{rec.x} >> params.coverage_chunk_shift;
    const std::int64_t cy0 = std::int64_t{rec.y} >> params.coverage_chunk_shift;
    const std::int64_t cols = ((std::int64_t{rec.x} + fw + edge - 1) >> params.coverage_chunk_shift) - cx0;
    const std::int64_t rows = ((std::int64_t{rec.y} + fh + edge - 1) >> params.coverage_chunk_shift) - cy0;
    // at most 2 x 2 chunks for a footprint of at most one chunk edge
    std::array<bool, 4> marked{};
    for (std::int64_t dy = 0; dy < fh; ++dy) {
        for (std::int64_t dx = 0; dx < fw; ++dx) {
            const std::int64_t lx = rec.x - std::int64_t{tiles.x0()} + dx;
            const std::int64_t ly = rec.y - std::int64_t{tiles.y0()} + dy;
            const std::uint64_t local =
                static_cast<std::uint64_t>(ly) * tiles.width() + static_cast<std::uint64_t>(lx);
            if (params.object_overlay != nullptr && params.object_overlay->find(local) != params.object_overlay->end()) {
                return kUnsupported;
            }
            bool kept = false;
            if (const std::uint32_t id = tiles.object(local); id != 0) {
                const BuildingType* existing = types.find(TypeId{id});
                if (existing == nullptr) {
                    return kUnsupported;
                }
                kept = (existing->flags_word & params.coverage_flag) != 0 && tiles.foot_anchor(local) != 0;
            }
            if (!kept) {
                const std::int64_t cx = ((std::int64_t{rec.x} + dx) >> params.coverage_chunk_shift) - cx0;
                const std::int64_t cy = ((std::int64_t{rec.y} + dy) >> params.coverage_chunk_shift) - cy0;
                marked[static_cast<std::size_t>(cx + 2 * cy)] = true;
            }
        }
    }
    for (std::int64_t r = 0; r < rows; ++r) {
        for (std::int64_t c = 0; c < cols; ++c) {
            if (!marked[static_cast<std::size_t>(c + 2 * r)]) {
                return {Reject::TerrainRefused, 0};
            }
        }
    }
    return {Reject::None, 0};
}

} // namespace

bool is_land_road(const BuildingType& t, const RoadParams& p) {
    const std::uint32_t cls = t.flags_word & 0x7000U;
    return t.prodtype == p.prodtype && t.flags_word <= 0xffffU && (t.flags_word & 0x10U) == 0 &&
           (cls == p.road_class || cls == p.slope_class) && (t.flags_word & 3U) != 1U &&
           t.width > 0 && t.height > 0;
}

PlacementResult placement_rule(const Island& island, const TypeRegistry& types,
                               const BuildingType& effective, const PlacementRecord& rec,
                               const PlacementParams& params) {
    constexpr PlacementResult kUnsupported{Reject::Unsupported, 0};
    // R3 class gate first, as the oracle's engine_place: no flag word outside the two evidenced
    // classes, no rotation above 3 (uninitialised footprint bytes in the original) and no empty
    // footprint ever gets a default answer.
    const Class cls = classify(effective, params);
    const auto [fw, fh] = footprint(effective, rec.rotation);
    if (cls == Class::Other || rec.rotation > 3 || fw == 0 || fh == 0) {
        return kUnsupported;
    }
    // R1 bounds, 64-bit (the original compares 32-bit values of 16-bit fields, no wrap).
    const TileLayer& tiles = island.tiles;
    const site::Origin origin{rec.x, rec.y};
    if (!site::footprint_inside(tiles, origin, fw, fh)) {
        return {Reject::OutOfIsland, 0};
    }
    // R2 occupancy, row-major like the original: the first tile that decides wins.
    for (std::int64_t dy = 0; dy < fh; ++dy) {
        for (std::int64_t dx = 0; dx < fw; ++dx) {
            const std::int64_t lx = rec.x - std::int64_t{tiles.x0()} + dx;
            const std::int64_t ly = rec.y - std::int64_t{tiles.y0()} + dy;
            const std::uint64_t local =
                static_cast<std::uint64_t>(ly) * tiles.width() + static_cast<std::uint64_t>(lx);
            // The latest object: an earlier record of the dry run supersedes the physical one.
            std::uint32_t id = tiles.object(local);
            if (params.object_overlay != nullptr) {
                if (const auto it = params.object_overlay->find(local);
                    it != params.object_overlay->end()) {
                    id = it->second.v;
                }
            }
            if (id == 0) {
                continue;
            }
            const BuildingType* existing = types.find(TypeId{id});
            if (existing == nullptr) {
                return kUnsupported;
            }
            if (site::overlap_with_existing(effective, *existing) != site::Compat::Compatible) {
                return {Reject::Occupied, 0};
            }
            // R-A3: a group-0xc1 piece that is not itself a land road (ramp, pier) is not replaced
            // by a road (the original accepts it; docs/sim/road-placement.md section 3).
            if (cls == Class::Road && params.road_preserve_nonroad_pieces &&
                group_of(existing->prodtype) == 0xc1U && !is_land_road(*existing)) {
                return {Reject::Occupied, 0};
            }
        }
    }
    // Coverage (flag bit 0x10 of the placed type): after occupancy compatibility, before the height
    // analysis, like the original (0x1007c955 before 0x1007cb50). Its refusal and the terrain
    // refusal both answer TerrainRefused; the order is held by the oracle and the disassembly only.
    if ((effective.flags_word & params.coverage_flag) != 0) {
        const PlacementResult cov = coverage_rule(island, types, rec, fw, fh, params);
        if (cov.reject != Reject::None) {
            return cov;
        }
    }
    // R4 / R5: the vertex window.
    const VertexGrid grid = VertexGrid::of(island);
    if (!grid.ok()) {
        return kUnsupported;
    }
    if (cls == Class::Road) {
        return road_window(grid, effective, rec, fw, fh, params, RoadParams{});
    }
    if (cls == Class::Coast) {
        const site::TerrainVerdict v =
            site::coast_height_check(grid, effective, rec.rotation, origin);
        switch (v.result) {
        case site::TerrainResult::Accepted:
            return {Reject::None, v.result_byte};
        case site::TerrainResult::Rejected:
            return {Reject::TerrainRefused, 0};
        default:
            return kUnsupported;
        }
    }
    if (cls == Class::Mountain) {
        // AnnoGame 0x10008f90 analysis (mode result = the offset), the helper 0x10008a80 and the
        // level floor of the island function's step 7 (docs/sim/salt-extraction.md section 11).
        const std::optional<WindowLevels> win = read_window(grid, rec, fw, fh);
        if (!win) {
            return kUnsupported;
        }
        const unsigned result = mode_result(*win, effective.flags_word);
        if (!site::mountain_helper_accepts(effective.width, effective.height, effective.highmap,
                                           rec.rotation, win->v, result)) {
            return {Reject::TerrainRefused, 0};
        }
        if (result < params.min_level && group_of(effective.prodtype) != params.exempt_group) {
            return {Reject::TerrainRefused, 0};
        }
        return {Reject::None, static_cast<std::uint8_t>(result)};
    }
    std::optional<std::uint8_t> level;
    for (std::int64_t dy = 0; dy <= fh; ++dy) {
        for (std::int64_t dx = 0; dx <= fw; ++dx) {
            const std::optional<std::uint8_t> v = grid.at(rec.x + dx, rec.y + dy);
            if (!v) {
                return kUnsupported;
            }
            if (!level) {
                level = v;
            } else if (*level != *v) {
                return {Reject::TerrainRefused, 0};
            }
        }
    }
    const std::uint32_t v = *level;
    if (params.level_quantum > 1 && v % params.level_quantum != 0) {
        return {Reject::TerrainRefused, 0};
    }
    if (v < params.min_level && group_of(effective.prodtype) != params.exempt_group) {
        return {Reject::TerrainRefused, 0};
    }
    return {Reject::None, static_cast<std::uint8_t>(v)};
}

std::uint8_t stand_in_placement_height(const Island& island, const TypeRegistry& types,
                                       const BuildingType& type, const PlacementRecord& rec) {
    const PlacementResult real = placement_rule(island, types, type, rec);
    if (real.reject == Reject::None) {
        return real.height;
    }
    // Own design (N), not the original's: a site the rule refuses gets the highest vertex of its
    // window, so the item never lies under its own terrain (equal to the R4 level on a flat
    // window). A vertex VertexGrid cannot read is skipped; none readable gives 0. The loop runs
    // over the window clipped to the grid's range (x0..x0 + W, y0..y0 + H), where `at` can answer.
    const auto [fw, fh] = footprint(type, rec.rotation);
    const VertexGrid grid = VertexGrid::of(island);
    const std::int64_t x_lo = std::max<std::int64_t>(rec.x, grid.x0());
    const std::int64_t x_hi =
        std::min<std::int64_t>(std::int64_t{rec.x} + fw, std::int64_t{grid.x0()} + grid.tiles_x());
    const std::int64_t y_lo = std::max<std::int64_t>(rec.y, grid.y0());
    const std::int64_t y_hi =
        std::min<std::int64_t>(std::int64_t{rec.y} + fh, std::int64_t{grid.y0()} + grid.tiles_y());
    std::uint8_t top = 0;
    for (std::int64_t vy = y_lo; vy <= y_hi; ++vy) {
        for (std::int64_t vx = x_lo; vx <= x_hi; ++vx) {
            if (const std::optional<std::uint8_t> v = grid.at(vx, vy)) {
                top = std::max(top, *v);
            }
        }
    }
    return top;
}

PlacementResult stand_in_placement_seam_fn(void*, const Island& island, const TypeRegistry& types,
                                           const BuildingType& type, const PlacementRecord& rec) {
    const Reject verdict = stand_in_placement_rule(island, type, rec);
    if (verdict != Reject::None) {
        return {verdict, 0};
    }
    return {Reject::None, stand_in_placement_height(island, types, type, rec)};
}

Reject footprint_owner_rule(const Island& island, const SimConfig& cfg, const BuildingType& type,
                            const PlacementRecord& rec, CityNo city) {
    const auto [fw, fh] = footprint(type, rec.rotation);
    const TileLayer& tiles = island.tiles;
    if (!inside_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw,
                      fh)) {
        return Reject::OutOfIsland;
    }
    const TileBox box =
        clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw, fh);
    // D 0x10058890: every tile is neutral or carries one common owner, and that owner is
    // the target city. An all-neutral footprint has no owner (the original's lookup ends
    // at the city numbered neutral_owner, which does not exist).
    std::optional<std::uint32_t> common;
    for (std::int64_t y = box.y0; y < box.y1; ++y) {
        for (std::int64_t x = box.x0; x < box.x1; ++x) {
            const std::uint32_t owner =
                tiles.owner(static_cast<std::uint64_t>(y * tiles.width() + x));
            if (owner == cfg.neutral_owner) {
                continue;
            }
            if (common.has_value() && *common != owner) {
                return Reject::TileNotOwned;
            }
            common = owner;
        }
    }
    if (!common.has_value() || *common != city.v) {
        return Reject::TileNotOwned;
    }
    return Reject::None;
}

Reject stand_in_placement_rule(const Island& island, const BuildingType& type,
                               const PlacementRecord& rec) {
    const auto [fw, fh] = footprint(type, rec.rotation);
    const TileLayer& tiles = island.tiles;
    const TileBox box =
        clip_to_layer(tiles.x0(), tiles.y0(), tiles.width(), tiles.height(), rec.x, rec.y, fw, fh);
    for (std::int64_t y = box.y0; y < box.y1; ++y) {
        for (std::int64_t x = box.x0; x < box.x1; ++x) {
            if (tiles.object(static_cast<std::uint64_t>(y * tiles.width() + x)) != 0) {
                return Reject::Occupied;
            }
        }
    }
    return Reject::None;
}

} // namespace anno::sim
