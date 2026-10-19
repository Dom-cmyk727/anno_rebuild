// The river raster (RiverMask, phase A): the shipped gap-fix criteria on the
// tile grid. See river.hpp and docs/render/rivers.md, section 3.2.
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>

#include <anno/render/river.hpp>
#include <anno/render/scene.hpp>

namespace anno::render::river {

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

/// How far (tiles) around a hole the dam test looks for a way past it: a river
/// wide enough to go around an object within this reach is not dammed by it.
constexpr std::int32_t kDamReachTiles = 3;

/// The number of components that the cells of `ring` (indices of the raster)
/// fall into among the wet cells that are not holes (and, unless
/// `through_candidates`, not in `candidate`, a sorted list), 4-connected inside
/// the window [wx0, wx1) x [wy0, wy1) (island cells).
std::size_t ring_components(const RiverMask& mask, const std::vector<std::size_t>& ring,
                            const std::vector<std::size_t>& candidate, bool through_candidates,
                            std::int32_t wx0, std::int32_t wy0, std::int32_t wx1,
                            std::int32_t wy1) {
    const auto w = static_cast<std::size_t>(wx1 - wx0), h = static_cast<std::size_t>(wy1 - wy0);
    std::vector<std::int32_t> label(w * h, -1);
    auto local = [&](std::size_t i) {
        const auto x = static_cast<std::int32_t>(i % mask.nx()) + mask.x0();
        const auto y = static_cast<std::int32_t>(i / mask.nx()) + mask.y0();
        return std::pair{x, y};
    };
    auto passable = [&](std::int32_t x, std::int32_t y) {
        if (x < wx0 || y < wy0 || x >= wx1 || y >= wy1) {
            return false;
        }
        const std::size_t i = mask.index(x, y);
        if (i == RiverMask::npos || (mask.flags[i] & kWet) == 0 || (mask.flags[i] & kHole) != 0) {
            return false;
        }
        return through_candidates || !std::binary_search(candidate.begin(), candidate.end(), i);
    };
    std::int32_t next = 0;
    std::vector<std::pair<std::int32_t, std::int32_t>> stack;
    std::vector<std::int32_t> seen_ring;
    for (const std::size_t r : ring) {
        const auto [x, y] = local(r);
        if (!passable(x, y)) {
            continue;
        }
        const std::size_t at =
            static_cast<std::size_t>(y - wy0) * w + static_cast<std::size_t>(x - wx0);
        if (label[at] < 0) {
            label[at] = next;
            stack.assign(1, {x, y});
            while (!stack.empty()) {
                const auto [px, py] = stack.back();
                stack.pop_back();
                for (int d = 0; d < 4; ++d) {
                    const std::int32_t qx = px + (d == 0 ? -1 : d == 1 ? 1 : 0);
                    const std::int32_t qy = py + (d == 2 ? -1 : d == 3 ? 1 : 0);
                    if (!passable(qx, qy)) {
                        continue;
                    }
                    const std::size_t qa =
                        static_cast<std::size_t>(qy - wy0) * w + static_cast<std::size_t>(qx - wx0);
                    if (label[qa] < 0) {
                        label[qa] = next;
                        stack.emplace_back(qx, qy);
                    }
                }
            }
            ++next;
        }
        seen_ring.push_back(label[at]);
    }
    std::sort(seen_ring.begin(), seen_ring.end());
    return static_cast<std::size_t>(std::unique(seen_ring.begin(), seen_ring.end()) -
                                    seen_ring.begin());
}

/// Marks the wet cells under the qualifying objects as holes (rivers.md 2.6).
void apply_obstacles(RiverMask& mask, std::span<const Obstacle> objects, const RiverConfig& cfg) {
    const auto c = static_cast<std::int32_t>(mask.cells_per_tile());
    const float cf = static_cast<float>(c);
    for (const Obstacle& o : objects) {
        ObstacleInfo info;
        info.object = o;
        info.radius =
            std::sqrt(static_cast<float>(std::max(o.w, 0) * std::max(o.h, 0)) / 3.14159265F);
        if (o.w <= 0 || o.h <= 0) {
            mask.obstacles.push_back(info);
            continue;
        }
        if (static_cast<std::uint32_t>(o.w) * static_cast<std::uint32_t>(o.h) >
            cfg.obstacle_max_area) {
            info.result = ObstacleInfo::Result::TooBig;
            mask.obstacles.push_back(info);
            continue;
        }
        // Wet share of the whole footprint.
        std::size_t wet = 0, total = 0;
        for (std::int32_t cy = o.y * c; cy < (o.y + o.h) * c; ++cy) {
            for (std::int32_t cx = o.x * c; cx < (o.x + o.w) * c; ++cx) {
                ++total;
                wet += mask.wet(cx, cy) ? 1U : 0U;
            }
        }
        info.wet_share = total > 0 ? static_cast<float>(wet) / static_cast<float>(total) : 0.0F;
        if (info.wet_share < cfg.obstacle_min_wet) {
            info.result = ObstacleInfo::Result::NotWet;
            mask.obstacles.push_back(info);
            continue;
        }
        // The hole: wet cells whose centre lies in the footprint shrunk about
        // its centre.
        const float mx = static_cast<float>(o.x) + 0.5F * static_cast<float>(o.w);
        const float my = static_cast<float>(o.y) + 0.5F * static_cast<float>(o.h);
        const float hw = 0.5F * static_cast<float>(o.w) * (1.0F - cfg.obstacle_shrink);
        const float hh = 0.5F * static_cast<float>(o.h) * (1.0F - cfg.obstacle_shrink);
        std::vector<std::size_t> candidate;
        for (std::int32_t cy = o.y * c; cy < (o.y + o.h) * c; ++cy) {
            for (std::int32_t cx = o.x * c; cx < (o.x + o.w) * c; ++cx) {
                const float x = (static_cast<float>(cx) + 0.5F) / cf;
                const float y = (static_cast<float>(cy) + 0.5F) / cf;
                const std::size_t i = mask.index(cx, cy);
                if (i != RiverMask::npos && (mask.flags[i] & kWet) != 0 &&
                    (mask.flags[i] & kHole) == 0 && std::fabs(x - mx) <= hw &&
                    std::fabs(y - my) <= hh) {
                    candidate.push_back(i);
                }
            }
        }
        if (candidate.empty()) {
            info.result = ObstacleInfo::Result::NoCells;
            mask.obstacles.push_back(info);
            continue;
        }
        std::sort(candidate.begin(), candidate.end());
        // The way past the object must be at least obstacle_gap_cells wide: the test
        // blocks the hole dilated by one cell less, so that a slit between the hole
        // and a bank (or another hole) narrower than the gap counts as closed.
        const auto grow = static_cast<std::int32_t>(std::max(cfg.obstacle_gap_cells, 1U) - 1U);
        std::vector<std::size_t> blocked;
        for (const std::size_t i : candidate) {
            const auto x = static_cast<std::int32_t>(i % mask.nx()) + mask.x0();
            const auto y = static_cast<std::int32_t>(i / mask.nx()) + mask.y0();
            for (std::int32_t dy = -grow; dy <= grow; ++dy) {
                for (std::int32_t dx = -grow; dx <= grow; ++dx) {
                    const std::size_t j = mask.index(x + dx, y + dy);
                    if (j != RiverMask::npos && (mask.flags[j] & kWet) != 0 &&
                        (mask.flags[j] & kHole) == 0) {
                        blocked.push_back(j);
                    }
                }
            }
        }
        std::sort(blocked.begin(), blocked.end());
        blocked.erase(std::unique(blocked.begin(), blocked.end()), blocked.end());
        // The ring: wet cells beside what is blocked (8-neighbourhood), not in it.
        std::vector<std::size_t> ring;
        std::int32_t bx0 = std::numeric_limits<std::int32_t>::max(), by0 = bx0;
        std::int32_t bx1 = std::numeric_limits<std::int32_t>::min(), by1 = bx1;
        for (const std::size_t i : blocked) {
            const auto x = static_cast<std::int32_t>(i % mask.nx()) + mask.x0();
            const auto y = static_cast<std::int32_t>(i / mask.nx()) + mask.y0();
            bx0 = std::min(bx0, x), bx1 = std::max(bx1, x + 1);
            by0 = std::min(by0, y), by1 = std::max(by1, y + 1);
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    const std::size_t j = mask.index(x + dx, y + dy);
                    if (j != RiverMask::npos && (mask.flags[j] & kWet) != 0 &&
                        (mask.flags[j] & kHole) == 0 &&
                        !std::binary_search(blocked.begin(), blocked.end(), j)) {
                        ring.push_back(j);
                    }
                }
            }
        }
        std::sort(ring.begin(), ring.end());
        ring.erase(std::unique(ring.begin(), ring.end()), ring.end());
        const std::int32_t reach = kDamReachTiles * c;
        const std::size_t before = ring_components(mask, ring, blocked, true, bx0 - reach,
                                                   by0 - reach, bx1 + reach, by1 + reach);
        const std::size_t after = ring_components(mask, ring, blocked, false, bx0 - reach,
                                                  by0 - reach, bx1 + reach, by1 + reach);
        if (after > before) {
            info.result = ObstacleInfo::Result::Dam;
            mask.obstacles.push_back(info);
            continue;
        }
        for (const std::size_t i : candidate) {
            mask.flags[i] = static_cast<std::uint8_t>(mask.flags[i] | kHole);
        }
        info.result = ObstacleInfo::Result::Hole;
        info.hole_cells = candidate.size();
        mask.obstacles.push_back(info);
    }
}

} // namespace

RiverMask::RiverMask(std::uint32_t cells_per_tile, std::int32_t x0, std::int32_t y0,
                     std::uint32_t nx, std::uint32_t ny)
    : c_(cells_per_tile), x0_(x0), y0_(y0), nx_(nx), ny_(ny) {
    const std::size_t n = std::size_t{nx} * ny;
    flags.assign(n, 0);
    bed.assign(n, 0.0F);
    level.assign(n, kNaN);
}

std::size_t RiverMask::index(std::int32_t cx, std::int32_t cy) const {
    const std::int64_t x = std::int64_t{cx} - x0_, y = std::int64_t{cy} - y0_;
    if (x < 0 || y < 0 || x >= nx_ || y >= ny_) {
        return npos;
    }
    return static_cast<std::size_t>(y) * nx_ + static_cast<std::size_t>(x);
}

bool RiverMask::wet(std::int32_t cx, std::int32_t cy) const {
    const std::size_t i = index(cx, cy);
    return i != npos && (flags[i] & kWet) != 0;
}

std::uint8_t RiverMask::flags_at(std::int32_t cx, std::int32_t cy) const {
    const std::size_t i = index(cx, cy);
    return i == npos ? std::uint8_t{0} : flags[i];
}

std::size_t RiverMask::wet_count() const {
    std::size_t n = 0;
    for (const std::uint8_t f : flags) {
        n += (f & kWet) != 0;
    }
    return n;
}

void RiverMask::set_wet(std::int32_t cx, std::int32_t cy, float bed_height, float surface,
                        std::uint8_t extra) {
    const std::size_t i = index(cx, cy);
    if (i == npos) {
        return;
    }
    flags[i] = static_cast<std::uint8_t>((flags[i] & ~kSea) | kWet | extra);
    bed[i] = bed_height;
    level[i] = surface;
}

void RiverMask::set_sea(std::int32_t cx, std::int32_t cy) {
    const std::size_t i = index(cx, cy);
    if (i != npos && (flags[i] & kWet) == 0) {
        flags[i] |= kSea;
    }
}

void RiverMask::set_level(std::int32_t cx, std::int32_t cy, float surface) {
    const std::size_t i = index(cx, cy);
    if (i != npos) {
        level[i] = surface;
    }
}

RiverMask RiverMask::from_island(const sea::IslandWater& iw,
                                 std::span<const sea::RiverPiece> pieces,
                                 std::span<const SpriteCover> covers, const RiverConfig& cfg,
                                 std::span<const Obstacle> obstacles) {
    const std::uint32_t c = std::max(cfg.cells_per_tile, 1U);
    // Tile rectangle of everything that can carry water, plus the margin.
    bool any = false;
    float tx0 = 0, ty0 = 0, tx1 = 0, ty1 = 0;
    auto grow = [&](float ax, float ay, float bx, float by) {
        if (!any) {
            tx0 = ax, ty0 = ay, tx1 = bx, ty1 = by;
            any = true;
        } else {
            tx0 = std::min(tx0, ax), ty0 = std::min(ty0, ay);
            tx1 = std::max(tx1, bx), ty1 = std::max(ty1, by);
        }
    };
    for (const auto& [x, y, w, h, z] : pieces) {
        (void)z;
        grow(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w),
             static_cast<float>(y + h));
    }
    for (const SpriteCover& s : covers) {
        grow(static_cast<float>(s.tx0), static_cast<float>(s.ty0), static_cast<float>(s.tx1),
             static_cast<float>(s.ty1));
    }
    if (!any || iw.w == 0 || iw.h == 0) {
        return RiverMask(c, 0, 0, 0, 0);
    }
    const float m = cfg.mask_margin_tiles;
    const float cf = static_cast<float>(c);
    const auto lo = [&](float v, std::size_t limit) {
        return std::clamp(static_cast<std::int32_t>(std::floor((v - m) * cf)), 0,
                          static_cast<std::int32_t>(limit * c));
    };
    const auto hi = [&](float v, std::size_t limit) {
        return std::clamp(static_cast<std::int32_t>(std::ceil((v + m) * cf)), 0,
                          static_cast<std::int32_t>(limit * c));
    };
    const std::int32_t cx0 = lo(tx0, iw.w), cy0 = lo(ty0, iw.h);
    const std::int32_t cx1 = hi(tx1, iw.w), cy1 = hi(ty1, iw.h);
    if (cx1 <= cx0 || cy1 <= cy0) {
        return RiverMask(c, 0, 0, 0, 0);
    }
    RiverMask mask(c, cx0, cy0, static_cast<std::uint32_t>(cx1 - cx0),
                   static_cast<std::uint32_t>(cy1 - cy0));
    const std::uint32_t nx = mask.nx(), ny = mask.ny();
    // Candidate cells: within the margin of a piece footprint or sprite
    // rectangle. Everything else stays dry and is not classified.
    std::vector<std::uint8_t> near(std::size_t{nx} * ny, 0);
    auto mark = [&](float ax, float ay, float bx, float by) {
        const std::int32_t ix0 =
            std::max(static_cast<std::int32_t>(std::floor((ax - m) * cf)), cx0);
        const std::int32_t iy0 =
            std::max(static_cast<std::int32_t>(std::floor((ay - m) * cf)), cy0);
        const std::int32_t ix1 = std::min(static_cast<std::int32_t>(std::ceil((bx + m) * cf)), cx1);
        const std::int32_t iy1 = std::min(static_cast<std::int32_t>(std::ceil((by + m) * cf)), cy1);
        for (std::int32_t y = iy0; y < iy1; ++y) {
            for (std::int32_t x = ix0; x < ix1; ++x) {
                near[static_cast<std::size_t>(y - cy0) * nx + static_cast<std::size_t>(x - cx0)] =
                    1;
            }
        }
    };
    for (const auto& [x, y, w, h, z] : pieces) {
        (void)z;
        mark(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w),
             static_cast<float>(y + h));
    }
    for (const SpriteCover& s : covers) {
        mark(static_cast<float>(s.tx0), static_cast<float>(s.ty0), static_cast<float>(s.tx1),
             static_cast<float>(s.ty1));
    }
    // Physical test and sea classification, as sea::water_field per pixel.
    // Only one bit per cell is kept from the ground (the sprite pass needs
    // "the original's sea is here"), not the 48-byte TileGround: the raster of
    // a large island has a million cells.
    std::vector<std::uint8_t> sea_here(std::size_t{nx} * ny, 0);
    for (std::int32_t cy = cy0; cy < cy1; ++cy) {
        for (std::int32_t cx = cx0; cx < cx1; ++cx) {
            const std::size_t i = mask.index(cx, cy);
            if (near[i] == 0) {
                continue;
            }
            const float x = (static_cast<float>(cx) + 0.5F) / cf;
            const float y = (static_cast<float>(cy) + 0.5F) / cf;
            const sea::TileGround g = sea::tile_ground(iw, x, y);
            sea_here[i] = g.original_water && g.sd > 0.0F ? 1 : 0;
            if (!g.original_water && g.sd <= 0.0F) {
                if (const auto p = sea::river_point(iw, x, y, g)) {
                    mask.flags[i] = kWet;
                    mask.bed[i] = p->bed;
                    mask.level[i] = p->level;
                    continue;
                }
            }
            if (g.original_water || g.sd > 0.0F) {
                mask.flags[i] = kSea;
            }
            if (const auto lv = iw.river_level(g.tx, g.ty, g.fx, g.fy)) {
                mask.level[i] = *lv;
            }
        }
    }
    // Sprite cover: the painted bed, waterfalls and mouths (never over the
    // original's sea: those pixels stay sea, as Scene::apply_fall_masks).
    for (const SpriteCover& s : covers) {
        const std::int32_t sx0 = s.tx0 * static_cast<std::int32_t>(c);
        const std::int32_t sy0 = s.ty0 * static_cast<std::int32_t>(c);
        const std::int32_t sw = (s.tx1 - s.tx0) * static_cast<std::int32_t>(c);
        const std::int32_t sh = (s.ty1 - s.ty0) * static_cast<std::int32_t>(c);
        if (s.cells.size() != static_cast<std::size_t>(sw) * static_cast<std::size_t>(sh)) {
            continue;
        }
        for (std::int32_t j = 0; j < sh; ++j) {
            for (std::int32_t i = 0; i < sw; ++i) {
                if (s.cells[static_cast<std::size_t>(j * sw + i)] == 0) {
                    continue;
                }
                const std::int32_t cx = sx0 + i, cy = sy0 + j;
                const std::size_t k = mask.index(cx, cy);
                if (k == RiverMask::npos || near[k] == 0) {
                    continue;
                }
                if (sea_here[k] != 0) {
                    continue; // the sea is here
                }
                std::uint8_t extra = s.kind == SpriteCover::Kind::Mouth  ? kMouth
                                     : s.kind == SpriteCover::Kind::Fall ? kFall
                                                                         : std::uint8_t{0};
                if ((mask.flags[k] & kWet) == 0) {
                    extra |= kSprite;
                    const float x = (static_cast<float>(cx) + 0.5F) / cf;
                    const float y = (static_cast<float>(cy) + 0.5F) / cf;
                    const float bed = sea::river_bed_height(iw.heights, iw.w, iw.h, x, y) +
                                      sea::height_noise(x, y) * sea::kRiverBankNoise;
                    float lv = mask.level[k];
                    if (std::isnan(lv)) {
                        lv = static_cast<float>(s.height) + sea::kRiverLevel;
                    }
                    mask.flags[k] = static_cast<std::uint8_t>(kWet | extra);
                    mask.bed[k] = bed;
                    mask.level[k] = lv;
                } else {
                    mask.flags[k] |= extra;
                }
            }
        }
    }
    std::vector<std::uint8_t>().swap(near);
    std::vector<std::uint8_t>().swap(sea_here);
    // Falls and mouths are the links of a river (rivers.md 1.2: 28 pieces of
    // river without them, 5 with): the sprite alpha of a waterfall covers
    // only the painted cascade, and where the cliff hides the terrain behind
    // its edge (the original's depth test sees the edge) the river above and
    // the river below do not touch in the raster. A fall or mouth piece joins
    // the pieces of river inside its footprint (dilated by one tile) with a
    // channel of cells flagged kBridge: straight, three cells wide, at most
    // cfg.bridge_tiles long, depth interpolated. Gaps elsewhere in the data
    // are not patched (decision D13).
    if (cfg.bridge_tiles > 0.0F) {
        const std::size_t total = std::size_t{nx} * ny;
        std::vector<std::int32_t> lab(total, -1);
        std::int32_t labels = 0;
        std::vector<std::size_t> stack;
        for (std::size_t i = 0; i < total; ++i) {
            if ((mask.flags[i] & kWet) == 0 || lab[i] >= 0) {
                continue;
            }
            const std::int32_t id = labels++;
            lab[i] = id;
            stack.assign(1, i);
            while (!stack.empty()) {
                const std::size_t p = stack.back();
                stack.pop_back();
                const std::size_t px = p % nx, py = p / nx;
                const std::array<std::size_t, 4> nbs = {
                    px > 0 ? p - 1 : total, px + 1 < nx ? p + 1 : total, py > 0 ? p - nx : total,
                    py + 1 < ny ? p + nx : total};
                for (const std::size_t q : nbs) {
                    if (q < total && (mask.flags[q] & kWet) != 0 && lab[q] < 0) {
                        lab[q] = id;
                        stack.push_back(q);
                    }
                }
            }
        }
        std::vector<std::int32_t> uf(static_cast<std::size_t>(labels));
        std::iota(uf.begin(), uf.end(), 0);
        auto find = [&](std::int32_t v) {
            while (uf[static_cast<std::size_t>(v)] != v) {
                uf[static_cast<std::size_t>(v)] =
                    uf[static_cast<std::size_t>(uf[static_cast<std::size_t>(v)])];
                v = uf[static_cast<std::size_t>(v)];
            }
            return v;
        };
        const auto max_gap = static_cast<std::int64_t>(cfg.bridge_tiles * cf);
        const auto half = static_cast<std::int32_t>(cfg.bridge_half_width);
        using Cell = std::pair<std::int32_t, std::int32_t>;
        for (const SpriteCover& s : covers) {
            if (s.kind == SpriteCover::Kind::Bed) {
                continue;
            }
            const std::int32_t rx0 = std::max(s.tx0 * static_cast<std::int32_t>(c), cx0);
            const std::int32_t ry0 = std::max(s.ty0 * static_cast<std::int32_t>(c), cy0);
            const std::int32_t rx1 = std::min(s.tx1 * static_cast<std::int32_t>(c), cx1);
            const std::int32_t ry1 = std::min(s.ty1 * static_cast<std::int32_t>(c), cy1);
            // Wet cells in the region by component (ordered: deterministic).
            std::map<std::int32_t, std::vector<Cell>> parts;
            for (std::int32_t y = ry0; y < ry1; ++y) {
                for (std::int32_t x = rx0; x < rx1; ++x) {
                    const std::size_t i = mask.index(x, y);
                    if (lab[i] >= 0) {
                        parts[find(lab[i])].emplace_back(x, y);
                    }
                }
            }
            if (parts.size() < 2) {
                continue;
            }
            // Prim: grow from the largest part by the closest pair.
            std::int32_t root = parts.begin()->first;
            for (const auto& [id, cells] : parts) {
                if (cells.size() > parts[root].size()) {
                    root = id;
                }
            }
            std::vector<Cell> tree = parts[root];
            parts.erase(root);
            while (!parts.empty()) {
                std::int64_t best = std::numeric_limits<std::int64_t>::max();
                Cell ba{0, 0}, bb{0, 0};
                std::int32_t bid = -1;
                for (const auto& [id, cells] : parts) {
                    for (const Cell& b : cells) {
                        for (const Cell& a : tree) {
                            const std::int64_t dx = a.first - b.first, dy = a.second - b.second;
                            const std::int64_t d2 = dx * dx + dy * dy;
                            if (d2 < best) {
                                best = d2, ba = a, bb = b, bid = id;
                            }
                        }
                    }
                }
                if (bid < 0 || best > max_gap * max_gap) {
                    break;
                }
                // Carve a straight channel, three cells wide, depth interpolated.
                const std::size_t ia = mask.index(ba.first, ba.second);
                const std::size_t ib = mask.index(bb.first, bb.second);
                const std::int32_t steps =
                    std::max(std::abs(bb.first - ba.first), std::abs(bb.second - ba.second));
                const std::int32_t root_label = lab[ia];
                for (std::int32_t k = 1; k < steps; ++k) {
                    const float t = static_cast<float>(k) / static_cast<float>(steps);
                    const auto x = static_cast<std::int32_t>(
                        std::lround(static_cast<float>(ba.first) +
                                    t * static_cast<float>(bb.first - ba.first)));
                    const auto y = static_cast<std::int32_t>(
                        std::lround(static_cast<float>(ba.second) +
                                    t * static_cast<float>(bb.second - ba.second)));
                    const float level = mask.level[ia] + t * (mask.level[ib] - mask.level[ia]);
                    const float bed = std::min(mask.bed[ia] + t * (mask.bed[ib] - mask.bed[ia]),
                                               level - cfg.bridge_min_depth);
                    for (std::int32_t oy = -half; oy <= half; ++oy) {
                        for (std::int32_t ox = -half; ox <= half; ++ox) {
                            if (std::abs(ox) + std::abs(oy) > half) {
                                continue;
                            }
                            const std::size_t i = mask.index(x + ox, y + oy);
                            if (i == RiverMask::npos || (mask.flags[i] & (kWet | kSea)) != 0) {
                                continue; // never over water or sea
                            }
                            mask.flags[i] = static_cast<std::uint8_t>(kWet | kBridge);
                            mask.bed[i] = bed;
                            mask.level[i] = level;
                            lab[i] = root_label;
                        }
                    }
                }
                uf[static_cast<std::size_t>(find(bid))] = find(root_label);
                tree.insert(tree.end(), parts[bid].begin(), parts[bid].end());
                parts.erase(bid);
                ++mask.bridges;
            }
        }
    }
    // Objects standing in the river (last: bridges and links are made on the
    // water without them; a hole never cuts a river in two).
    if (cfg.obstacles && !obstacles.empty()) {
        apply_obstacles(mask, obstacles, cfg);
    }
    return mask;
}

namespace detail {

int half_window(float tile_px, std::uint32_t cells_per_tile) {
    const float cell_px = tile_px / static_cast<float>(std::max(cells_per_tile, 1U));
    return std::max(0, static_cast<int>(std::lround((cell_px / 2.0F - 1.0F) / 2.0F)));
}

std::vector<CellPixel> project_cells(std::span<const std::uint8_t> heights, std::size_t w,
                                     std::size_t h, int tx0, int ty0, int tx1, int ty1,
                                     std::uint32_t cells_per_tile, float half_w, float half_h,
                                     float lift_px, bool depth_test, float pass_height) {
    const auto c = static_cast<int>(std::max(cells_per_tile, 1U));
    const float cf = static_cast<float>(c);
    const int sw = (tx1 - tx0) * c, sh = (ty1 - ty0) * c;
    std::vector<CellPixel> out(static_cast<std::size_t>(std::max(sw, 0)) *
                               static_cast<std::size_t>(std::max(sh, 0)));
    for (int j = 0; j < sh; ++j) {
        for (int i = 0; i < sw; ++i) {
            const float x = (static_cast<float>(tx0 * c + i) + 0.5F) / cf;
            const float y = (static_cast<float>(ty0 * c + j) + 0.5F) / cf;
            const float t = terrain_height(heights, w, h, x, y);
            CellPixel& q = out[static_cast<std::size_t>(j * sw + i)];
            q.sx = (x - y) * half_w;
            q.sy = (x + y) * half_h - lift_px * t;
            q.pass = !depth_test || t <= pass_height;
        }
    }
    return out;
}

AlphaPlane plane_for(std::span<const CellPixel> cells, int rx, int ry) {
    AlphaPlane plane;
    if (cells.empty()) {
        return plane;
    }
    float minx = 1e30F, miny = 1e30F, maxx = -1e30F, maxy = -1e30F;
    for (const CellPixel& q : cells) {
        minx = std::min(minx, q.sx), maxx = std::max(maxx, q.sx);
        miny = std::min(miny, q.sy), maxy = std::max(maxy, q.sy);
    }
    // The window (rx, ry), the 3 x 3 closing (1) and a pixel of rounding.
    const int pad = std::max(rx, ry) + 2;
    plane.x0 = static_cast<int>(std::floor(minx)) - pad;
    plane.y0 = static_cast<int>(std::floor(miny)) - pad;
    plane.w = static_cast<int>(std::ceil(maxx)) + pad + 1 - plane.x0;
    plane.h = static_cast<int>(std::ceil(maxy)) + pad + 1 - plane.y0;
    plane.alpha.assign(static_cast<std::size_t>(plane.w) * static_cast<std::size_t>(plane.h), 0);
    return plane;
}

std::vector<std::uint8_t> cover_from_alpha(std::span<const CellPixel> cells,
                                           const AlphaPlane& plane, int rx, int ry) {
    const int cw = plane.w, ch = plane.h;
    // The sprite is a 50 % checkerboard: close it over 3 x 3 pixels.
    std::vector<std::uint8_t> closed(static_cast<std::size_t>(cw) * static_cast<std::size_t>(ch),
                                     0);
    for (int y = 0; y < ch; ++y) {
        for (int x = 0; x < cw; ++x) {
            if (plane.alpha[static_cast<std::size_t>(y) * static_cast<std::size_t>(cw) +
                            static_cast<std::size_t>(x)] < 128) {
                continue;
            }
            for (int yy = std::max(y - 1, 0); yy <= std::min(y + 1, ch - 1); ++yy) {
                for (int xx = std::max(x - 1, 0); xx <= std::min(x + 1, cw - 1); ++xx) {
                    closed[static_cast<std::size_t>(yy) * static_cast<std::size_t>(cw) +
                           static_cast<std::size_t>(xx)] = 1;
                }
            }
        }
    }
    std::vector<std::uint8_t> out(cells.size(), 0);
    for (std::size_t k = 0; k < cells.size(); ++k) {
        if (!cells[k].pass) {
            continue;
        }
        // Coverage of the cell: the closed share of a window of half its
        // size, like the blurred alpha above 0.5 in Scene::apply_fall_masks.
        const int ox = static_cast<int>(std::floor(cells[k].sx)) - plane.x0;
        const int oy = static_cast<int>(std::floor(cells[k].sy)) - plane.y0;
        int hit = 0, total = 0;
        for (int yy = oy - ry; yy <= oy + ry; ++yy) {
            for (int xx = ox - rx; xx <= ox + rx; ++xx) {
                ++total;
                if (xx >= 0 && yy >= 0 && xx < cw && yy < ch) {
                    hit += closed[static_cast<std::size_t>(yy) * static_cast<std::size_t>(cw) +
                                  static_cast<std::size_t>(xx)];
                }
            }
        }
        out[k] = hit * 2 >= total ? 1 : 0;
    }
    return out;
}

} // namespace detail

} // namespace anno::render::river

namespace anno::render {

river::RiverMask Scene::river_mask(std::size_t index, const river::RiverConfig& cfg) const {
    const sea::IslandWater* iw = island_water(index);
    if (iw == nullptr) {
        return river::RiverMask(std::max(cfg.cells_per_tile, 1U), 0, 0, 0, 0);
    }
    const IslandData& d = objects_[index];
    const std::uint32_t c = std::max(cfg.cells_per_tile, 1U);
    const float lift_px = static_cast<float>(lift.num) / static_cast<float>(lift.den);
    const float half_w = static_cast<float>(kTile[0].w) / 2.0F;
    const float half_h = static_cast<float>(kTile[0].h) / 2.0F;
    // A cell projects to (tile width / c) x (tile height / c) pixels (10.5 x
    // 5.25 for the original's 84 x 42 tile at 8 cells per tile); the coverage
    // window is half of that (detail::half_window: 5 x 3 pixels there). Zoom
    // group 0 is the finest the sprites are drawn at; the window follows
    // cells_per_tile.
    const int rx = river::detail::half_window(static_cast<float>(kTile[0].w), c);
    const int ry = river::detail::half_window(static_cast<float>(kTile[0].h), c);
    std::vector<river::SpriteCover> covers;
    auto add = [&](const Prepared& p, river::SpriteCover::Kind kind, bool depth_test) {
        river::SpriteCover s;
        s.kind = kind;
        s.height = p.first.height();
        // The sprite reaches a little beyond its footprint; terrain seen
        // there belongs to cells up to a tile away.
        s.tx0 = std::max(static_cast<int>(p.first.x) - 1, 0);
        s.ty0 = std::max(static_cast<int>(p.first.y) - 1, 0);
        s.tx1 = std::min(static_cast<int>(p.first.x) + p.second.first + 1, static_cast<int>(iw->w));
        s.ty1 =
            std::min(static_cast<int>(p.first.y) + p.second.second + 1, static_cast<int>(iw->h));
        if (s.tx1 <= s.tx0 || s.ty1 <= s.ty0) {
            return;
        }
        // Forward projection of every cell's terrain point (no inverse of the
        // iso mapping needed): its island-local pixel at zoom group 0.
        const std::vector<river::detail::CellPixel> proj = river::detail::project_cells(
            iw->heights, iw->w, iw->h, s.tx0, s.ty0, s.tx1, s.ty1, c, half_w, half_h, lift_px,
            depth_test, static_cast<float>(s.height) + kRiverTolerance);
        river::detail::AlphaPlane plane = river::detail::plane_for(proj, rx, ry);
        Canvas local = blank(Rect{plane.x0, plane.y0, plane.w, plane.h});
        auto all = [](int, int, std::uint8_t) { return true; };
        draw_prepared_tested(local, std::span<const Prepared>(&p, 1), anlagen, 0, lift, 0, sprites,
                             0, all);
        for (std::size_t k = 0; k < plane.alpha.size(); ++k) {
            plane.alpha[k] = local.pixels[k * 4 + 3];
        }
        s.cells = river::detail::cover_from_alpha(proj, plane, rx, ry);
        covers.push_back(std::move(s));
    };
    for (const Prepared& p : d.river_sprites) {
        add(p, river::SpriteCover::Kind::Bed, true);
    }
    for (const Prepared& p : d.falls_above) {
        add(p,
            p.first.anlage == kRiverFallAnlagen[0] ? river::SpriteCover::Kind::Fall
                                                   : river::SpriteCover::Kind::Mouth,
            true);
    }
    for (const Prepared& p : d.falls_below) {
        add(p,
            p.first.anlage == kRiverFallAnlagen[0] ? river::SpriteCover::Kind::Fall
                                                   : river::SpriteCover::Kind::Mouth,
            false);
    }
    // Objects that may stand in the river as obstacles (rivers.md 2.6, D4):
    // the reviewed ids only; from_island applies the footprint rules.
    std::vector<river::Obstacle> obstacles;
    if (cfg.obstacles && !cfg.obstacle_ids.empty()) {
        for (const river::Obstacle& o : island_objects(index, cfg.obstacle_max_area)) {
            if (std::find(cfg.obstacle_ids.begin(), cfg.obstacle_ids.end(), o.id) !=
                cfg.obstacle_ids.end()) {
                obstacles.push_back(o);
            }
        }
    }
    return river::RiverMask::from_island(*iw, d.rivers, covers, cfg, obstacles);
}

std::vector<river::Obstacle> Scene::island_objects(std::size_t index,
                                                   std::uint32_t max_area) const {
    std::vector<river::Obstacle> out;
    if (index >= objects_.size()) {
        return out;
    }
    for (const Prepared& p : objects_[index].other) {
        const int w = p.second.first, h = p.second.second;
        if (w <= 0 || h <= 0 ||
            static_cast<std::uint32_t>(w) * static_cast<std::uint32_t>(h) > max_area) {
            continue;
        }
        out.push_back(
            {p.first.anlage, static_cast<int>(p.first.x), static_cast<int>(p.first.y), w, h});
    }
    return out;
}

} // namespace anno::render
