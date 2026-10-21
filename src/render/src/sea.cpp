#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <tuple>

#include <anno/render/sea.hpp>

namespace anno::render::sea {

namespace insel = formats::insel;

namespace {

/// Passes of a 3 × 3 box blur over the shore distance field: wave crests
/// follow it, so it is smoothed strongly (radius ≈ 3 tiles). Land stays 0.
constexpr std::size_t kShoreSmoothing = 10;

std::vector<float> smooth_shore(std::span<const std::uint8_t> heights, std::size_t w,
                                std::size_t h) {
    std::vector<float> d = shore_distance(heights, w, h);
    for (std::size_t pass = 0; pass < kShoreSmoothing; ++pass) {
        const std::vector<float> prev = d;
        for (std::size_t y = 0; y < h; ++y) {
            for (std::size_t x = 0; x < w; ++x) {
                const std::size_t i = y * w + x;
                if (prev[i] == 0.0F) {
                    continue;
                }
                float sum = 0.0F, n = 0.0F;
                for (std::size_t yy = y == 0 ? 0 : y - 1; yy <= std::min(y + 1, h - 1); ++yy) {
                    for (std::size_t xx = x == 0 ? 0 : x - 1; xx <= std::min(x + 1, w - 1); ++xx) {
                        const float v = prev[yy * w + xx];
                        // Open sea beyond the island keeps growing.
                        if (std::isfinite(v)) {
                            sum += v;
                            n += 1.0F;
                        }
                    }
                }
                if (n > 0.0F) {
                    d[i] = sum / n;
                }
            }
        }
    }
    return d;
}

} // namespace

std::optional<SeaPattern> SeaPattern::build(const formats::anlagen::AnlagenFile& anlagen,
                                            std::size_t group, Lift lift, Px water,
                                            SpriteCache& cache) {
    const formats::figuren::Entry* entry = anlagen.entry(kSeaAnlage);
    if (entry == nullptr || entry->gfx.empty()) {
        return std::nullopt;
    }
    const auto& set = entry->gfx.front();
    const auto g = static_cast<std::uint32_t>(group);
    const formats::figuren::FrameSet* frames = nullptr;
    for (const auto& fs : set.frame_sets) {
        if (fs.group == g) {
            frames = &fs;
            break;
        }
    }
    if (frames == nullptr) {
        return std::nullopt;
    }
    const auto anim = animation(set);
    const std::size_t steps = anim.second == 0 ? 1 : anim.first;
    const auto [tw, th] = kTile[group];
    SeaPattern out;
    out.width = kPeriodTiles * tw;
    out.height = kPeriodTiles * th;
    out.step_ms = anim.second;
    const int raise = lift.scaled(kSeaLevel, 1, group);
    // Lattice points whose sprites can reach the pattern rectangle: its tile
    // range (x ∈ [0, 2p], y ∈ [−p, p]) plus a margin of 8 tiles.
    const int p = kPeriodTiles;
    const auto npx = static_cast<std::size_t>(out.width * out.height);
    for (std::size_t step = 0; step < steps; ++step) {
        const std::uint64_t t = step * anim.second;
        auto blank = [&](Px c) {
            Canvas cv;
            cv.width = static_cast<std::size_t>(out.width);
            cv.height = static_cast<std::size_t>(out.height);
            cv.origin = {0, 0};
            cv.pixels.reserve(npx * 4);
            for (std::size_t i = 0; i < npx; ++i) {
                cv.pixels.insert(cv.pixels.end(), c.begin(), c.end());
            }
            return cv;
        };
        Canvas sprite = blank({0, 0, 0, 0}), filled = blank(water);
        for (int y = -p - 8; y < p + 8; ++y) {
            if (rem_euclid(y, 2) != 0) {
                continue;
            }
            const int x0 = 2 * rem_euclid(y / 2, 2);
            for (int x = -8; x < 2 * p + 8; ++x) {
                if (rem_euclid(x - x0, 4) != 0) {
                    continue;
                }
                const auto phase = static_cast<std::size_t>(rem_euclid(5 * x + 3 * y, 32));
                const std::size_t k = frame_index(frames->frames.size(), anim, 0, t, phase);
                if (k >= frames->frames.size()) {
                    continue;
                }
                const auto& frame = frames->frames[k];
                const auto img = cache.get(anlagen, {kSeaAnlage, 0, false, g, k}, frame);
                if (!img) {
                    continue;
                }
                const auto [sx, sy] = tile_top(x, y, group);
                const std::pair<int, int> at{sx, sy - raise};
                auto all = [](int, int, Px c) -> std::optional<Px> { return c; };
                blit_sprite(sprite, *img, frame, at, false, all);
                blit_sprite(filled, *img, frame, at, false, all);
            }
        }
        auto crop = [npx](const Canvas& c) {
            std::vector<Px> v(npx);
            for (std::size_t i = 0; i < npx; ++i) {
                v[i] = {c.pixels[i * 4], c.pixels[i * 4 + 1], c.pixels[i * 4 + 2],
                        c.pixels[i * 4 + 3]};
            }
            return v;
        };
        out.steps_.push_back(Step{crop(sprite), crop(filled)});
    }
    return out;
}

std::tuple<std::size_t, std::size_t, float> SeaPattern::steps_at(std::uint64_t time_ms) const {
    const std::size_t n = steps_.size();
    if (step_ms == 0 || n < 2) {
        return {0, 0, 0.0F};
    }
    const std::uint64_t ticks = time_ms / step_ms;
    const auto k = static_cast<std::size_t>(ticks % n);
    const float frac = static_cast<float>(time_ms % step_ms) / static_cast<float>(step_ms);
    return {k, (k + 1) % n, frac};
}

std::array<float, 4> SeaPattern::smoothed(std::span<const Px> img, int sx, int sy) const {
    std::array<float, 4> sum{};
    constexpr std::array<std::pair<int, int>, 4> taps = {{{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
    for (const auto& [dx, dy] : taps) {
        const Px& p = img[index(sx + dx, sy + dy)];
        const float a = static_cast<float>(p[3]) / 255.0F;
        for (std::size_t c = 0; c < 3; ++c) {
            sum[c] += static_cast<float>(p[c]) / 255.0F * a;
        }
        sum[3] += a;
    }
    for (float& v : sum) {
        v = v / 4.0F;
    }
    return sum;
}

std::size_t SeaPattern::index(int sx, int sy) const {
    return static_cast<std::size_t>(rem_euclid(sy, height) * width + rem_euclid(sx, width));
}

float smooth_height(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h, float x,
                    float y) {
    auto weights = [](float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float u = 1.0F - t;
        return std::array<float, 4>{u * u * u / 6.0F, (3.0F * t3 - 6.0F * t2 + 4.0F) / 6.0F,
                                    (-3.0F * t3 + 3.0F * t2 + 3.0F * t + 1.0F) / 6.0F, t3 / 6.0F};
    };
    const float fx = std::floor(x), fy = std::floor(y);
    const auto wx = weights(x - fx), wy = weights(y - fy);
    const auto ix = sat_cast<std::int64_t>(fx), iy = sat_cast<std::int64_t>(fy);
    auto at = [&](std::int64_t cx, std::int64_t cy) {
        const std::size_t ux =
            std::min(static_cast<std::size_t>(std::max<std::int64_t>(cx, 0)), w - 1);
        const std::size_t uy =
            std::min(static_cast<std::size_t>(std::max<std::int64_t>(cy, 0)), h - 1);
        return static_cast<float>(heights[uy * w + ux]);
    };
    float v = 0.0F;
    for (std::size_t j = 0; j < 4; ++j) {
        for (std::size_t i = 0; i < 4; ++i) {
            const auto di = static_cast<std::int64_t>(i) - 1, dj = static_cast<std::int64_t>(j) - 1;
            v += wx[i] * wy[j] * at(ix + di, iy + dj);
        }
    }
    return v + height_noise(x, y);
}

float height_noise(float x, float y) {
    const float amp = kWaves.shore_noise;
    return (noise3(x * 0.9F, y * 0.9F, 11.0F) - 0.5F) * 2.0F * amp +
           (noise3(x * 2.7F, y * 2.7F, 23.0F) - 0.5F) * 0.6F * amp;
}

float river_bed_height(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h, float x,
                       float y) {
    // Catmull-Rom weights of the four corners around the position.
    auto weights = [](float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return std::array<float, 4>{(-t3 + 2.0F * t2 - t) * 0.5F,
                                    (3.0F * t3 - 5.0F * t2 + 2.0F) * 0.5F,
                                    (-3.0F * t3 + 4.0F * t2 + t) * 0.5F, (t3 - t2) * 0.5F};
    };
    const float fx = std::floor(x), fy = std::floor(y);
    const auto wx = weights(x - fx), wy = weights(y - fy);
    const auto ix = sat_cast<std::int64_t>(fx), iy = sat_cast<std::int64_t>(fy);
    auto at = [&](std::int64_t cx, std::int64_t cy) {
        const std::size_t ux =
            std::min(static_cast<std::size_t>(std::max<std::int64_t>(cx, 0)), w - 1);
        const std::size_t uy =
            std::min(static_cast<std::size_t>(std::max<std::int64_t>(cy, 0)), h - 1);
        return static_cast<float>(heights[uy * w + ux]);
    };
    float v = 0.0F, lo = 255.0F, hi = 0.0F;
    for (std::size_t j = 0; j < 4; ++j) {
        for (std::size_t i = 0; i < 4; ++i) {
            const auto di = static_cast<std::int64_t>(i) - 1, dj = static_cast<std::int64_t>(j) - 1;
            const float c = at(ix + di, iy + dj);
            v += wx[i] * wy[j] * c;
            // The four corners of the cell the position lies in bound the
            // result (Catmull-Rom overshoots near steep steps).
            if (i == 1 || i == 2) {
                if (j == 1 || j == 2) {
                    lo = std::min(lo, c);
                    hi = std::max(hi, c);
                }
            }
        }
    }
    return std::clamp(v, lo, hi);
}

/// Joins river pieces whose corner footprints are at most max_gap tiles apart
/// (Kruskal over the pairs, shortest first, so only separate pieces are
/// linked): a straight channel of corners, plus-shaped three corners wide,
/// gets the interpolated river level (added to sum / n) and a bed carved
/// kGapBedDepth below it in `heights`. Corners that already carry river are
/// left alone.
void patch_river_gaps(std::span<const RiverPiece> rivers, std::size_t w, std::size_t h,
                      float max_gap, std::vector<std::uint8_t>& heights, std::vector<float>& sum,
                      std::vector<float>& n) {
    struct Link {
        std::int64_t d2;
        std::size_t a, b;
        int ax, ay, bx, by;
    };
    struct Rect {
        int x0, y0, x1, y1;
    };
    std::vector<Rect> r;
    r.reserve(rivers.size());
    for (const auto& [x, y, fw, fh, z] : rivers) {
        (void)z;
        r.push_back({x, y, x + fw, y + fh});
    }
    auto axis = [](int a0, int a1, int b0, int b1, int& pa, int& pb) {
        if (b0 > a1) {
            pa = a1, pb = b0;
        } else if (a0 > b1) {
            pa = a0, pb = b1;
        } else {
            pa = pb = (std::max(a0, b0) + std::min(a1, b1)) / 2;
        }
    };
    const auto limit = static_cast<std::int64_t>(std::floor(max_gap * max_gap));
    std::vector<Link> links;
    for (std::size_t a = 0; a < r.size(); ++a) {
        for (std::size_t b = a + 1; b < r.size(); ++b) {
            Link l{0, a, b, 0, 0, 0, 0};
            axis(r[a].x0, r[a].x1, r[b].x0, r[b].x1, l.ax, l.bx);
            axis(r[a].y0, r[a].y1, r[b].y0, r[b].y1, l.ay, l.by);
            const std::int64_t dx = l.bx - l.ax, dy = l.by - l.ay;
            l.d2 = dx * dx + dy * dy;
            if (l.d2 <= limit) {
                links.push_back(l);
            }
        }
    }
    std::sort(links.begin(), links.end(), [](const Link& p, const Link& q) {
        return std::tie(p.d2, p.a, p.b) < std::tie(q.d2, q.a, q.b);
    });
    std::vector<std::size_t> uf(r.size());
    std::iota(uf.begin(), uf.end(), std::size_t{0});
    auto find = [&](std::size_t v) {
        while (uf[v] != v) {
            uf[v] = uf[uf[v]];
            v = uf[v];
        }
        return v;
    };
    // Pieces that touch or overlap are one river already.
    for (std::size_t a = 0; a < r.size(); ++a) {
        for (std::size_t b = a + 1; b < r.size(); ++b) {
            if (r[a].x0 <= r[b].x1 && r[b].x0 <= r[a].x1 && r[a].y0 <= r[b].y1 &&
                r[b].y0 <= r[a].y1) {
                uf[find(a)] = find(b);
            }
        }
    }
    // The painted bed is a trench of corners at most level + 1 (the original's
    // depth test: terrain <= object height + 10). The water test needs the bed
    // under the level, so the trench corners at level + 1 (and the shallow
    // ones) leave breaks in a one-corner-wide channel, which the original's
    // sprites cover. Every trench corner gets at least kGapBedDepth of depth.
    for (std::size_t i = 0; i < w * h; ++i) {
        if (n[i] > 0.0F) {
            const float level = sum[i] / n[i];
            if (static_cast<float>(heights[i]) <= level + 1.0F) {
                heights[i] = std::min(
                    heights[i], static_cast<std::uint8_t>(std::max(level - kGapBedDepth, 0.0F)));
            }
        }
    }
    const std::vector<float> before = n;
    for (const Link& l : links) {
        if (find(l.a) == find(l.b)) {
            continue;
        }
        uf[find(l.a)] = find(l.b);
        const float za = static_cast<float>(std::get<4>(rivers[l.a])) + kRiverLevel;
        const float zb = static_cast<float>(std::get<4>(rivers[l.b])) + kRiverLevel;
        const int steps = std::max(std::abs(l.bx - l.ax), std::abs(l.by - l.ay));
        for (int k = 0; k <= steps; ++k) {
            const float t = steps == 0 ? 0.5F : static_cast<float>(k) / static_cast<float>(steps);
            const int cx =
                l.ax + static_cast<int>(std::lround(t * static_cast<float>(l.bx - l.ax)));
            const int cy =
                l.ay + static_cast<int>(std::lround(t * static_cast<float>(l.by - l.ay)));
            const float level = za + t * (zb - za);
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    const int px = cx + ox, py = cy + oy;
                    if (std::abs(ox) + std::abs(oy) > 1 || px < 0 || py < 0 ||
                        static_cast<std::size_t>(px) >= w || static_cast<std::size_t>(py) >= h) {
                        continue;
                    }
                    const std::size_t i =
                        static_cast<std::size_t>(py) * w + static_cast<std::size_t>(px);
                    if (before[i] > 0.0F) {
                        continue; // a river corner already
                    }
                    sum[i] += level;
                    n[i] += 1.0F;
                    const float bed = std::max(level - kGapBedDepth, 0.0F);
                    heights[i] = std::min(heights[i], static_cast<std::uint8_t>(bed));
                }
            }
        }
    }
}

std::optional<IslandWater> IslandWater::build(const insel::Island& island,
                                              std::span<const RiverPiece> rivers,
                                              float gap_patch_tiles) {
    auto hr = island.heights();
    auto gr = island.gfx_ids();
    if (!hr || !gr) {
        return std::nullopt;
    }
    IslandWater out;
    out.heights = std::move(hr).value();
    const std::vector<std::uint16_t> gfx = std::move(gr).value();
    const std::size_t iw_w = island.width, iw_h = island.height;
    out.w = iw_w;
    out.h = iw_h;
    const std::size_t cw = (iw_w + 7) / 8;
    out.sea_chunk.assign(cw * ((iw_h + 7) / 8), false);
    for (std::size_t y = 0; y < iw_h; ++y) {
        for (std::size_t x = 0; x < iw_w; ++x) {
            const auto c = corners(out.heights, iw_w, iw_h, x, y);
            if (c[0] + c[1] + c[2] + c[3] <= 4 * kLandAverage) {
                out.sea_chunk[(y / 8) * cw + x / 8] = true;
            }
        }
    }
    auto open_tile = [&](std::ptrdiff_t x, std::ptrdiff_t y) {
        if (x >= 0 && y >= 0 && static_cast<std::size_t>(x) < iw_w &&
            static_cast<std::size_t>(y) < iw_h) {
            const auto cell = insel::GroundCell::from_gfx_id(
                gfx[static_cast<std::size_t>(y) * iw_w + static_cast<std::size_t>(x)]);
            return cell.texture == 0 ? 1.0F : 0.0F;
        }
        return 1.0F;
    };
    out.open.assign(iw_w * iw_h, 0.0F);
    for (std::size_t y = 0; y < iw_h; ++y) {
        for (std::size_t x = 0; x < iw_w; ++x) {
            const auto xi = static_cast<std::ptrdiff_t>(x), yi = static_cast<std::ptrdiff_t>(y);
            float sum = 0.0F;
            for (std::ptrdiff_t dy = -2; dy < 2; ++dy) {
                for (std::ptrdiff_t dx = -2; dx < 2; ++dx) {
                    sum += open_tile(xi + dx, yi + dy);
                }
            }
            // Fade the floor out towards the island border.
            const std::size_t edge = std::min({x, y, iw_w - 1 - x, iw_h - 1 - y});
            const float border = 1.0F - fminf(static_cast<float>(edge) / kBorderFadeTiles, 1.0F);
            out.open[y * iw_w + x] = fmaxf(sum / 16.0F, border);
        }
    }
    // River level: average over the pieces covering each corner, then two
    // passes of averaging with defined neighbours.
    std::vector<float> sum(iw_w * iw_h, 0.0F), n(iw_w * iw_h, 0.0F);
    for (const auto& [x, y, fw, fh, z] : rivers) {
        for (std::size_t dy = 0; dy <= fh; ++dy) {
            for (std::size_t dx = 0; dx <= fw; ++dx) {
                const std::size_t cx = std::size_t{x} + dx, cy = std::size_t{y} + dy;
                if (cx < iw_w && cy < iw_h) {
                    sum[cy * iw_w + cx] += static_cast<float>(z) + kRiverLevel;
                    n[cy * iw_w + cx] += 1.0F;
                }
            }
        }
    }
    if (gap_patch_tiles > 0.0F) {
        patch_river_gaps(rivers, iw_w, iw_h, gap_patch_tiles, out.heights, sum, n);
    }
    out.river.resize(iw_w * iw_h);
    for (std::size_t i = 0; i < iw_w * iw_h; ++i) {
        out.river[i] = n[i] > 0.0F ? sum[i] / n[i] : std::numeric_limits<float>::quiet_NaN();
    }
    for (int pass = 0; pass < 2; ++pass) {
        const std::vector<float> prev = out.river;
        for (std::size_t y = 0; y < iw_h; ++y) {
            for (std::size_t x = 0; x < iw_w; ++x) {
                float s = 0.0F, k = 0.0F;
                for (std::size_t yy = y == 0 ? 0 : y - 1; yy <= std::min(y + 1, iw_h - 1); ++yy) {
                    for (std::size_t xx = x == 0 ? 0 : x - 1; xx <= std::min(x + 1, iw_w - 1);
                         ++xx) {
                        const float v = prev[yy * iw_w + xx];
                        if (!std::isnan(v)) {
                            s += v;
                            k += 1.0F;
                        }
                    }
                }
                if (k > 0.0F) {
                    out.river[y * iw_w + x] = s / k;
                }
            }
        }
    }
    out.shore = smooth_shore(out.heights, iw_w, iw_h);
    out.classify_cells();
    return out;
}

void IslandWater::classify_cells() {
    // Empty cells (docs/render/iso.md "Water field: empty cells"): no river
    // level at the four corners river_level reads, and either no sea in the
    // tile's 8 x 8 block or all 16 corners of smooth_height at least
    // kEmptyLandHeight. Every position in such a tile gets the empty texel.
    empty_cell.assign(w * h, 0);
    const std::size_t cw = (w + 7) / 8;
    auto corner = [&](std::ptrdiff_t x, std::ptrdiff_t y) {
        const auto cx = static_cast<std::size_t>(
            std::clamp<std::ptrdiff_t>(x, 0, static_cast<std::ptrdiff_t>(w) - 1));
        const auto cy = static_cast<std::size_t>(
            std::clamp<std::ptrdiff_t>(y, 0, static_cast<std::ptrdiff_t>(h) - 1));
        return cy * w + cx;
    };
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const auto xi = static_cast<std::ptrdiff_t>(x), yi = static_cast<std::ptrdiff_t>(y);
            if (!std::isnan(river[corner(xi, yi)]) || !std::isnan(river[corner(xi + 1, yi)]) ||
                !std::isnan(river[corner(xi, yi + 1)]) ||
                !std::isnan(river[corner(xi + 1, yi + 1)])) {
                continue;
            }
            bool empty = !sea_chunk[(y / 8) * cw + x / 8];
            if (!empty) {
                std::uint8_t lo = 255;
                for (std::ptrdiff_t dy = -1; dy <= 2; ++dy) {
                    for (std::ptrdiff_t dx = -1; dx <= 2; ++dx) {
                        lo = std::min(lo, heights[corner(xi + dx, yi + dy)]);
                    }
                }
                empty = static_cast<float>(lo) >= kEmptyLandHeight;
            }
            empty_cell[y * w + x] = empty ? 1 : 0;
        }
    }
}

std::optional<float> IslandWater::river_level(std::size_t tx, std::size_t ty, float fx,
                                              float fy) const {
    auto at = [&](std::size_t x, std::size_t y) {
        return river[std::min(y, h - 1) * w + std::min(x, w - 1)];
    };
    float s = 0.0F, k = 0.0F;
    const std::array<std::pair<float, float>, 4> taps = {{
        {at(tx, ty), (1.0F - fx) * (1.0F - fy)},
        {at(tx + 1, ty), fx * (1.0F - fy)},
        {at(tx, ty + 1), (1.0F - fx) * fy},
        {at(tx + 1, ty + 1), fx * fy},
    }};
    for (const auto& [v, wt] : taps) {
        if (!std::isnan(v)) {
            s += v * wt;
            k += wt;
        }
    }
    if (k > 0.25F) {
        return s / k;
    }
    return std::nullopt;
}

float IslandWater::bilinear(std::span<const float> field, std::size_t tx, std::size_t ty, float fx,
                            float fy) const {
    auto at = [&](std::size_t x, std::size_t y) {
        return field[std::min(y, h - 1) * w + std::min(x, w - 1)];
    };
    return at(tx, ty) * (1.0F - fx) * (1.0F - fy) + at(tx + 1, ty) * fx * (1.0F - fy) +
           at(tx, ty + 1) * (1.0F - fx) * fy + at(tx + 1, ty + 1) * fx * fy;
}

TileGround tile_ground(const IslandWater& iw, float x, float y) {
    TileGround g;
    g.under = terrain_height(iw.heights, iw.w, iw.h, x, y);
    g.tx = std::min(sat_cast<std::size_t>(fmaxf(x, 0.0F)), iw.w - 1);
    g.ty = std::min(sat_cast<std::size_t>(fmaxf(y, 0.0F)), iw.h - 1);
    g.fx = clampf(x - static_cast<float>(g.tx), 0.0F, 1.0F);
    g.fy = clampf(y - static_cast<float>(g.ty), 0.0F, 1.0F);
    g.sea_chunk = iw.sea_chunk[(g.ty / 8) * ((iw.w + 7) / 8) + g.tx / 8];
    g.original_water = g.under < static_cast<float>(kWaterline) && g.sea_chunk;
    g.smooth = smooth_height(iw.heights, iw.w, iw.h, x, y);
    g.sd = static_cast<float>(kWaterline) - g.smooth;
    return g;
}

std::optional<RiverPoint> river_point(const IslandWater& iw, float x, float y,
                                      const TileGround& g) {
    const auto level = iw.river_level(g.tx, g.ty, g.fx, g.fy);
    if (!level) {
        return std::nullopt;
    }
    // The bed, not the waterline surface: the approximating B-spline lifts
    // the floor of a one-corner-wide channel by up to the river depth and cut
    // the water there.
    const float bed =
        river_bed_height(iw.heights, iw.w, iw.h, x, y) + height_noise(x, y) * kRiverBankNoise;
    const float rd = *level - bed;
    if (!(rd > -kWaves.shore_edge)) {
        return std::nullopt;
    }
    return RiverPoint{*level, bed, rd};
}

FieldTexel field_at(const IslandWater& iw_ref, float x, float y) {
    const IslandWater* iw = &iw_ref;
    const float edge = kWaves.shore_edge;
    // sat_cast: NaN (e.g. inf · 0 in the shore field of an island without land)
    // becomes 0, as Rust's `as u8`; a plain cast would be undefined.
    auto byte = [](float v) { return sat_cast<std::uint8_t>(clampf(v, 0.0F, 255.0F)); };
    const TileGround g = tile_ground(*iw, x, y);
    const std::size_t tx = g.tx, ty = g.ty;
    const float fx = g.fx, fy = g.fy;
    const bool sea_chunk = g.sea_chunk;
    const bool original_water = g.original_water;
    const float sd = g.sd;
    // Enhanced river water (flag bit 1): river level over the smooth
    // terrain, where no sea is.
    if (!original_water && sd <= 0.0F) {
        if (const auto point = river_point(*iw, x, y, g)) {
            const float rd = point->depth;
            // Slope of the river surface (HEIGHT units per tile): rapids
            // where it falls steeply (waterfalls).
            auto level_at = [&](float px, float py) {
                const std::size_t ax = std::min(sat_cast<std::size_t>(fmaxf(px, 0.0F)), iw->w - 1);
                const std::size_t ay = std::min(sat_cast<std::size_t>(fmaxf(py, 0.0F)), iw->h - 1);
                return iw
                    ->river_level(ax, ay, clampf(px - static_cast<float>(ax), 0.0F, 1.0F),
                                  clampf(py - static_cast<float>(ay), 0.0F, 1.0F))
                    .value_or(point->level);
            };
            const float e = 0.5F;
            const float gx = level_at(x + e, y) - level_at(x - e, y);
            const float gy = level_at(x, y + e) - level_at(x, y - e);
            const float rslope = std::sqrt(gx * gx + gy * gy) / (2.0F * e);
            const auto q = sat_cast<std::uint8_t>(fminf(roundf(rslope * 2.0F), 63.0F));
            return {static_cast<std::uint8_t>(1 + kRiverOpenness), 255,
                    byte(kDepthBias + rd * kDepthScale),
                    static_cast<std::uint8_t>(2U | std::uint32_t{q} << 2U)};
        }
    }
    // Beach slope near the waterline, for the swash run-up.
    float slope = 0.0F;
    if (sd > -kSlopeBand && sd < kWaves.swash_depth + 3.0F) {
        const float e = 0.3F;
        auto at = [&](float px, float py) {
            return smooth_height(iw->heights, iw->w, iw->h, px, py);
        };
        const float gx = at(x + e, y) - at(x - e, y), gy = at(x, y + e) - at(x, y - e);
        slope = std::sqrt(gx * gx + gy * gy) / (2.0F * e);
    }
    const float run_max = kWaves.runup_max_tiles * fmaxf(slope, 0.2F);
    if (!sea_chunk || (!original_water && sd <= -(edge + run_max))) {
        return {};
    }
    const float o = iw->bilinear(iw->open, tx, ty, fx, fy);
    const float d = iw->bilinear(iw->shore, tx, ty, fx, fy);
    const auto slope_q = sat_cast<std::uint8_t>(fminf(roundf(slope * 2.0F), 63.0F));
    return {static_cast<std::uint8_t>(
                1 + sat_cast<std::uint8_t>(roundf(clampf(o, 0.0F, 1.0F) * 254.0F))),
            byte(d * kShoreScale), byte(kDepthBias + sd * kDepthScale),
            static_cast<std::uint8_t>((original_water ? 1U : 0U) | std::uint32_t{slope_q} << 2U)};
}

WaterField water_field(Rect rect, const SceneProbe& probe,
                       const std::function<const IslandWater*(std::size_t)>& water) {
    const auto n = static_cast<std::size_t>(rect.w * rect.h);
    WaterField f{rect, std::vector<std::uint8_t>(n, 255), std::vector<std::uint8_t>(n, 255),
                 std::vector<std::uint8_t>(n, 255), std::vector<std::uint8_t>(n, 1)};
    // The island of the previous pixel: runs of pixels share one.
    std::uint32_t last = kNoIsland;
    const IslandWater* iw = nullptr;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint32_t island = probe.island[i];
        if (island == kNoIsland) {
            continue;
        }
        if (island != last) {
            last = island;
            iw = water(island);
        }
        if (iw == nullptr) {
            continue;
        }
        const auto [x, y] = probe.pos[i];
        FieldTexel t;
        // Inside the island an empty cell needs no evaluation (iso.md "Water
        // field: empty cells"); the cell is the tile tile_ground would use.
        if (!(x >= 0.0F && y >= 0.0F && x < static_cast<float>(iw->w) &&
              y < static_cast<float>(iw->h) &&
              iw->empty_cell[static_cast<std::size_t>(y) * iw->w + static_cast<std::size_t>(x)] !=
                  0)) {
            t = field_at(*iw, x, y);
        }
        f.cover[i] = t.cover;
        f.shore[i] = t.shore;
        f.depth[i] = t.depth;
        f.flags[i] = t.flags;
    }
    return f;
}

Px unpremultiply(std::array<float, 4> p) {
    const float a = p[3];
    auto channel = [](float v) {
        return sat_cast<std::uint8_t>(clampf(roundf(v * 255.0F), 0.0F, 255.0F));
    };
    if (a <= 0.0F) {
        return {0, 0, 0, 0};
    }
    return {channel(p[0] / a), channel(p[1] / a), channel(p[2] / a), channel(a)};
}

Px lerp4(Px a, Px b, std::uint32_t t, std::uint32_t scale) {
    Px out{};
    for (std::size_t c = 0; c < 4; ++c) {
        out[c] = static_cast<std::uint8_t>(
            (std::uint32_t{a[c]} * (scale - t) + std::uint32_t{b[c]} * t) / scale);
    }
    return out;
}

} // namespace anno::render::sea
