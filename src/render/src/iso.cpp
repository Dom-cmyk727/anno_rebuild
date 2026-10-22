#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <set>

#include <anno/assets/building.hpp>
#include <anno/render/iso.hpp>
#include <anno/render/profile.hpp>

namespace anno::render {

namespace insel = formats::insel;
namespace fig = formats::figuren;

void over(std::uint8_t* dst, const std::uint8_t* src) {
    const std::uint32_t a = src[3];
    if (a == 0) {
        return;
    }
    if (a == 255) {
        std::copy(src, src + 4, dst);
        return;
    }
    const std::uint32_t da = dst[3];
    // Output alpha and colour in units of 1/255², straight alpha.
    const std::uint32_t out_a = a * 255 + da * (255 - a);
    for (std::size_t c = 0; c < 3; ++c) {
        const std::uint32_t num =
            std::uint32_t{src[c]} * a * 255 + std::uint32_t{dst[c]} * da * (255 - a);
        dst[c] = static_cast<std::uint8_t>(num / out_a);
    }
    dst[3] = static_cast<std::uint8_t>(out_a / 255);
}

Canvas Canvas::for_island(const insel::Island& island, std::size_t group, int margin) {
    const auto [tw, th] = kTile[group];
    const int w = island.width, h = island.height;
    const int left = -h * tw / 2;
    const int right = w * tw / 2;
    const int bottom = (w + h) * th / 2;
    Canvas c;
    c.width = static_cast<std::size_t>(right - left);
    c.height = static_cast<std::size_t>(bottom + margin);
    c.origin = {left, -margin};
    c.pixels.assign(c.width * c.height * 4, 0);
    return c;
}

std::optional<std::size_t> Canvas::index(int sx, int sy) const {
    const int x = sx - origin.first, y = sy - origin.second;
    if (x < 0 || y < 0 || static_cast<std::size_t>(x) >= width ||
        static_cast<std::size_t>(y) >= height) {
        return std::nullopt;
    }
    return (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;
}

void Canvas::put(int sx, int sy, Px rgba) {
    if (const auto o = index(sx, sy)) {
        over(&pixels[*o], rgba.data());
    }
}

bool Canvas::overlaps(int x, int y, int w, int h) const {
    const auto [cx, cy] = origin;
    const int cw = static_cast<int>(width), ch = static_cast<int>(height);
    return x < cx + cw && x + w > cx && y < cy + ch && y + h > cy;
}

std::optional<Rgba> Canvas::to_rgba() const {
    constexpr std::size_t max = 0xFFFF;
    if (width > max || height > max) {
        return std::nullopt;
    }
    return Rgba{static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height), pixels};
}

std::pair<int, int> tile_top(int x, int y, std::size_t group) {
    const auto [tw, th] = kTile[group];
    return {(x - y) * tw / 2, (x + y) * th / 2};
}

int Lift::scaled(std::int64_t h_num, std::int64_t h_den, std::size_t group) const {
    const std::int64_t n = h_num * num * kTile[group].w;
    const std::int64_t d = h_den * den * kTile[0].w;
    return static_cast<int>(n / d);
}

std::array<std::int64_t, 4> corners(std::span<const std::uint8_t> heights, std::size_t w,
                                    std::size_t h, std::size_t x, std::size_t y) {
    auto at = [&](std::size_t cx, std::size_t cy) {
        return std::int64_t{heights[std::min(cy, h - 1) * w + std::min(cx, w - 1)]};
    };
    return {at(x, y), at(x + 1, y), at(x, y + 1), at(x + 1, y + 1)};
}

std::int64_t interpolate(std::array<std::int64_t, 4> c, std::int64_t i, std::int64_t j,
                         std::int64_t n) {
    const auto [a, cc, b, d] = c;
    if (i + j <= n) {
        return a * n + i * (cc - a) + j * (b - a);
    }
    return d * n + (n - i) * (b - d) + (n - j) * (cc - d);
}

float terrain_height(std::span<const std::uint8_t> heights, std::size_t w, std::size_t h, float x,
                     float y) {
    const std::size_t tx = std::min(sat_cast<std::size_t>(fmaxf(x, 0.0F)), w - 1);
    const std::size_t ty = std::min(sat_cast<std::size_t>(fmaxf(y, 0.0F)), h - 1);
    const float s = clampf(x - static_cast<float>(tx), 0.0F, 1.0F);
    const float t = clampf(y - static_cast<float>(ty), 0.0F, 1.0F);
    const auto c = corners(heights, w, h, tx, ty);
    const float a = static_cast<float>(c[0]), cc = static_cast<float>(c[1]),
                b = static_cast<float>(c[2]), d = static_cast<float>(c[3]);
    if (s + t <= 1.0F) {
        return a + s * (cc - a) + t * (b - a);
    }
    return d + (1.0F - s) * (b - d) + (1.0F - t) * (cc - d);
}

GroundProbe GroundProbe::for_canvas(const Canvas& canvas) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    return GroundProbe{std::vector<std::array<float, 2>>(canvas.width * canvas.height, {nan, nan})};
}

namespace {

using Vec2 = std::pair<float, float>;

/// Fills a screen triangle; each covered pixel centre (px, py) gets
/// shade(px, py, s, t) with (s, t) interpolated from `uv`. A planar triangle
/// projects affinely, so this is exact; every pixel is drawn once.
template <class F>
void raster_triangle(Canvas& canvas, std::array<Vec2, 3> v, std::array<Vec2, 3> uv, F&& shade) {
    auto edge = [](Vec2 p, Vec2 q, Vec2 r) {
        return (q.first - p.first) * (r.second - p.second) -
               (q.second - p.second) * (r.first - p.first);
    };
    const float area = edge(v[0], v[1], v[2]);
    if (std::fabs(area) < 1e-6F) {
        return;
    }
    const int x0 = sat_cast<int>(std::floor(fminf(fminf(v[0].first, v[1].first), v[2].first)));
    const int x1 = sat_cast<int>(std::ceil(fmaxf(fmaxf(v[0].first, v[1].first), v[2].first)));
    const int y0 = sat_cast<int>(std::floor(fminf(fminf(v[0].second, v[1].second), v[2].second)));
    const int y1 = sat_cast<int>(std::ceil(fmaxf(fmaxf(v[0].second, v[1].second), v[2].second)));
    if (!canvas.overlaps(x0, y0, x1 - x0 + 1, y1 - y0 + 1)) {
        return;
    }
    const auto [cx0, cy0] = canvas.origin;
    const int cw = static_cast<int>(canvas.width), ch = static_cast<int>(canvas.height);
    const float sign = std::copysign(1.0F, area);
    // Small tolerance so shared edges leave no cracks.
    const float eps = -1e-3F * std::fabs(area);
    const float n = std::fabs(area);
    for (int py = std::max(y0, cy0); py <= std::min(y1, cy0 + ch - 1); ++py) {
        for (int px = std::max(x0, cx0); px <= std::min(x1, cx0 + cw - 1); ++px) {
            const Vec2 p{static_cast<float>(px) + 0.5F, static_cast<float>(py) + 0.5F};
            const float w0 = edge(v[1], v[2], p) * sign;
            const float w1 = edge(v[2], v[0], p) * sign;
            const float w2 = edge(v[0], v[1], p) * sign;
            if (w0 < eps || w1 < eps || w2 < eps) {
                continue;
            }
            const float b0 = w0 / n, b1 = w1 / n, b2 = w2 / n;
            const float s =
                clampf(b0 * uv[0].first + b1 * uv[1].first + b2 * uv[2].first, 0.0F, 1.0F);
            const float t =
                clampf(b0 * uv[0].second + b1 * uv[1].second + b2 * uv[2].second, 0.0F, 1.0F);
            canvas.put(px, py, shade(px, py, s, t));
        }
    }
}

} // namespace

void draw_ground_grids(Canvas& canvas, std::size_t w, std::size_t hgt,
                       std::span<const std::uint16_t> gfx, std::span<const std::uint8_t> heights,
                       const assets::island::GroundTextures& textures, std::size_t group, Lift lift,
                       GroundProbe* probe) {
    if (w == 0 || hgt == 0 || gfx.size() < w * hgt || heights.size() < w * hgt) {
        return;
    }
    const float tw = static_cast<float>(kTile[group].w);
    const float th = static_cast<float>(kTile[group].h);
    const float tw0 = static_cast<float>(kTile[0].w);
    const float px_per_unit =
        static_cast<float>(lift.num) / static_cast<float>(lift.den) * tw / tw0;
    // Tile (x, y) with u = x − y, v = x + y covers screen x in
    // [(u − 1)·tw/2, (u + 1)·tw/2] and y in [v·th/2 − raise, (v + 2)·th/2].
    const std::uint8_t top =
        heights.empty() ? std::uint8_t{0} : *std::max_element(heights.begin(), heights.end());
    const float raise = static_cast<float>(top) * px_per_unit;
    const float x0 = static_cast<float>(canvas.origin.first);
    const float y0 = static_cast<float>(canvas.origin.second);
    const float x1 = x0 + static_cast<float>(canvas.width);
    const float y1 = y0 + static_cast<float>(canvas.height);
    const std::int64_t umin = sat_cast<std::int64_t>(std::floor(2.0F * x0 / tw)) - 1;
    const std::int64_t umax = sat_cast<std::int64_t>(std::ceil(2.0F * x1 / tw)) + 1;
    const std::int64_t vmin = sat_cast<std::int64_t>(std::floor(2.0F * y0 / th)) - 2;
    const std::int64_t vmax = sat_cast<std::int64_t>(std::ceil(2.0F * (y1 + raise) / th)) + 1;
    const auto wi = static_cast<std::int64_t>(w), hi = static_cast<std::int64_t>(hgt);
    std::vector<std::size_t> order;
    for (std::int64_t v = std::max<std::int64_t>(vmin, 0); v <= std::min(vmax, wi + hi - 2); ++v) {
        const std::int64_t xa = std::max<std::int64_t>(
            std::max(div_euclid<std::int64_t>(v + umin + 1, 2), v - hi + 1), 0);
        const std::int64_t xb =
            std::min(std::min(div_euclid<std::int64_t>(v + umax, 2), v), wi - 1);
        for (std::int64_t x = xa; x <= xb; ++x) {
            order.push_back(static_cast<std::size_t>((v - x) * wi + x));
        }
    }
    const auto origin = canvas.origin;
    const std::size_t cw = canvas.width;
    for (const std::size_t t : order) {
        const auto cell = insel::GroundCell::from_gfx_id(gfx[t]);
        const auto c = corners(heights, w, hgt, t % w, t / w);
        const float x = static_cast<float>(t % w), y = static_cast<float>(t / w);
        auto screen = [&](float cx, float cy, std::int64_t h) {
            return Vec2{(cx - cy) * tw / 2.0F,
                        (cx + cy) * th / 2.0F - static_cast<float>(h) * px_per_unit};
        };
        // Corners A (x, y), C (x+1, y), B (x, y+1), D (x+1, y+1).
        const Vec2 a = screen(x, y, c[0]);
        const Vec2 cc = screen(x + 1.0F, y, c[1]);
        const Vec2 b = screen(x, y + 1.0F, c[2]);
        const Vec2 d = screen(x + 1.0F, y + 1.0F, c[3]);
        auto texel = [&](int px, int py, float s, float tt) -> Px {
            constexpr std::size_t n = 4096;
            if (probe != nullptr) {
                const std::size_t i = static_cast<std::size_t>(py - origin.second) * cw +
                                      static_cast<std::size_t>(px - origin.first);
                probe->pos[i] = {x + s, y + tt};
            }
            auto q = [](float v) {
                return std::min(sat_cast<std::size_t>(v * static_cast<float>(n)), n - 1);
            };
            return textures.cell_pixel(cell, q(s), q(tt), n);
        };
        // The two triangles of the tile, split along B–C as the original.
        raster_triangle(canvas, {a, b, cc}, {Vec2{0.0F, 0.0F}, Vec2{0.0F, 1.0F}, Vec2{1.0F, 0.0F}},
                        texel);
        raster_triangle(canvas, {cc, b, d}, {Vec2{1.0F, 0.0F}, Vec2{0.0F, 1.0F}, Vec2{1.0F, 1.0F}},
                        texel);
    }
}

void draw_ground(Canvas& canvas, const insel::Island& island,
                 const assets::island::GroundTextures& textures, std::size_t group, Lift lift) {
    auto gfx = island.gfx_ids();
    auto heights = island.heights();
    if (!gfx || !heights) {
        return;
    }
    draw_ground_grids(canvas, island.width, island.height, gfx.value(), heights.value(), textures,
                      group, lift);
}

Rgba smooth_2x2(const Rgba& img) {
    const std::size_t w = img.width, h = img.height;
    auto px = [&](std::size_t x, std::size_t y) -> std::array<float, 4> {
        if (x >= w || y >= h) {
            return {0, 0, 0, 0};
        }
        const std::size_t o = (y * w + x) * 4;
        const float a = static_cast<float>(img.pixels[o + 3]) / 255.0F;
        return {static_cast<float>(img.pixels[o]) * a, static_cast<float>(img.pixels[o + 1]) * a,
                static_cast<float>(img.pixels[o + 2]) * a, a};
    };
    auto q = [](float v) { return sat_cast<std::uint8_t>(clampf(roundf(v), 0.0F, 255.0F)); };
    Rgba out{img.width, img.height, {}};
    out.pixels.reserve(img.pixels.size());
    constexpr std::array<std::pair<std::size_t, std::size_t>, 4> taps = {
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}}};
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            std::array<float, 4> s{};
            for (const auto& [dx, dy] : taps) {
                const auto p = px(x + dx, y + dy);
                for (std::size_t c = 0; c < 4; ++c) {
                    s[c] += p[c] / 4.0F;
                }
            }
            if (s[3] <= 0.0F) {
                out.pixels.insert(out.pixels.end(), {0, 0, 0, 0});
            } else {
                out.pixels.insert(out.pixels.end(), {q(s[0] / s[3]), q(s[1] / s[3]), q(s[2] / s[3]),
                                                     q(s[3] * 255.0F)});
            }
        }
    }
    return out;
}

std::shared_ptr<const Rgba> SpriteCache::get(const formats::anlagen::AnlagenFile& anlagen,
                                             const SpriteKey& key, const fig::Frame& frame) {
    {
        const std::shared_lock lock(mutex_);
        if (const auto it = frames_.find(key); it != frames_.end()) {
            return it->second;
        }
    }
    const profile::Scope stage(profile::Stage::decode);
    std::shared_ptr<const Rgba> img;
    const auto [id, variant, shadow, group, index] = key;
    if (variant == kSmoothed) {
        if (const auto base = get(anlagen, {id, 0, shadow, group, index}, frame)) {
            img = std::make_shared<const Rgba>(smooth_2x2(*base));
        }
    } else if (const fig::Entry* entry = anlagen.entry(id)) {
        if (auto r = assets::building::to_rgba(anlagen, *entry, group, frame)) {
            img = std::make_shared<const Rgba>(std::move(r).value());
        }
    }
    const std::unique_lock lock(mutex_);
    frames_.insert_or_assign(key, img);
    return img;
}

std::pair<std::size_t, std::uint64_t> animation(const fig::GfxSet& set) {
    std::optional<formats::Bytes> anim;
    for (const auto& r : set.records) {
        if (r.name() == "ANIM") {
            anim = r.payload;
            break;
        }
    }
    std::size_t frames = 1;
    std::uint64_t ms = 0;
    if (anim && anim->size() >= 4) {
        frames = std::max<std::size_t>(std::size_t{(*anim)[2]} | std::size_t{(*anim)[3]} << 8, 1);
    }
    if (anim && anim->size() >= 6) {
        ms = std::uint64_t{(*anim)[4]} | std::uint64_t{(*anim)[5]} << 8;
    }
    return {frames, ms};
}

std::size_t frame_index(std::size_t total, std::pair<std::size_t, std::uint64_t> anim,
                        std::uint8_t rotation, std::uint64_t time_ms, std::size_t phase) {
    const auto [n, ms] = anim;
    const std::size_t rotations = std::max<std::size_t>(total / n, 1);
    const std::uint64_t ticks = ms == 0 ? 0 : time_ms / ms;
    const auto step = static_cast<std::size_t>((ticks + phase) % n);
    return (std::size_t{rotation} % rotations) * n + step;
}

std::vector<Prepared> prepare_objects(const insel::Island& island,
                                      const formats::anlagen::AnlagenFile& anlagen,
                                      std::span<const std::string_view> records, Objects which) {
    std::set<insel::PlacedObject> objs;
    for (const auto r : records) {
        if (auto list = island.objects(r)) {
            objs.insert(list.value().begin(), list.value().end());
        }
    }
    std::vector<Prepared> list;
    for (const auto& o : objs) {
        const fig::Entry* e = anlagen.entry(o.anlage);
        bool keep = false;
        if (e == nullptr || e->gfx.empty()) {
            // Counted (failed or invisible) once, with the static objects.
            keep = which == Objects::All || which == Objects::NotWater;
        } else {
            const auto& prefix = e->gfx.front().prefix;
            const bool water = prefix.size() > 2 && prefix[2] == kKindWater;
            const bool below = std::int64_t{o.height()} < kWaterline;
            switch (which) {
            case Objects::All:
                keep = true;
                break;
            case Objects::NotWater:
                keep = !water;
                break;
            case Objects::WaterBelowSea:
                keep = water && below;
                break;
            case Objects::WaterAboveSea:
                keep = water && !below;
                break;
            }
        }
        if (!keep) {
            continue;
        }
        const auto fp =
            anlagen.footprint(o.anlage).value_or(std::pair{std::uint8_t{1}, std::uint8_t{1}});
        list.emplace_back(o, o.footprint(fp.first, fp.second));
    }
    std::stable_sort(list.begin(), list.end(), [](const Prepared& a, const Prepared& b) {
        auto key = [](const Prepared& p) {
            return std::pair{
                std::uint32_t{p.first.x} + p.second.first + p.first.y + p.second.second, p.first.x};
        };
        return key(a) < key(b);
    });
    return list;
}

std::array<int, 4> SpriteDraw::rect() const {
    return {anchor.first - frame->anchor_x, anchor.second - frame->anchor_y, int{frame->width},
            int{frame->height}};
}

std::array<int, 4> SpriteDraw::reach() const {
    auto r = rect();
    r[2] += kOverrunSlack;
    return r;
}

DrawStats draw_prepared(Canvas& canvas, std::span<const Prepared> list,
                        const formats::anlagen::AnlagenFile& anlagen, std::size_t group, Lift lift,
                        std::uint64_t time_ms, SpriteCache& cache) {
    return draw_prepared_tested(canvas, list, anlagen, group, lift, time_ms, cache, 0,
                                [](int, int, std::uint8_t) { return true; });
}

DrawStats draw_objects(Canvas& canvas, const insel::Island& island,
                       const formats::anlagen::AnlagenFile& anlagen,
                       std::span<const std::string_view> records, Objects which, std::size_t group,
                       Lift lift, std::uint64_t time_ms, SpriteCache& cache) {
    const auto list = prepare_objects(island, anlagen, records, which);
    return draw_prepared(canvas, list, anlagen, group, lift, time_ms, cache);
}

std::optional<Px> water_colour(const formats::anlagen::AnlagenFile& anlagen) {
    const fig::Entry* entry = anlagen.entry(kSeaAnlage);
    if (entry == nullptr || entry->gfx.empty()) {
        return std::nullopt;
    }
    const fig::Frame* frame = nullptr;
    for (const auto& fs : entry->gfx.front().frame_sets) {
        if (fs.group == 0) {
            if (!fs.frames.empty()) {
                frame = &fs.frames.front();
            }
            break;
        }
    }
    if (frame == nullptr) {
        return std::nullopt;
    }
    auto img = assets::building::to_rgba(anlagen, *entry, 0, *frame);
    if (!img) {
        return std::nullopt;
    }
    std::array<std::uint64_t, 3> sum{};
    std::uint64_t n = 0;
    const auto& px = img.value().pixels;
    for (std::size_t i = 0; i + 3 < px.size(); i += 4) {
        if (px[i + 3] == 0) {
            continue;
        }
        for (std::size_t c = 0; c < 3; ++c) {
            sum[c] += px[i + c];
        }
        ++n;
    }
    if (n == 0) {
        return std::nullopt;
    }
    return Px{static_cast<std::uint8_t>(sum[0] / n), static_cast<std::uint8_t>(sum[1] / n),
              static_cast<std::uint8_t>(sum[2] / n), 255};
}

std::vector<float> shore_distance(std::span<const std::uint8_t> heights, std::size_t w,
                                  std::size_t h) {
    const float diag = std::sqrt(2.0F);
    std::vector<float> d;
    d.reserve(heights.size());
    for (const std::uint8_t z : heights) {
        d.push_back(std::int64_t{z} >= kWaterline ? 0.0F : std::numeric_limits<float>::infinity());
    }
    auto at = [w](std::size_t x, std::size_t y) { return y * w + x; };
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            float v = d[at(x, y)];
            if (x > 0) {
                v = fminf(v, d[at(x - 1, y)] + 1.0F);
            }
            if (y > 0) {
                v = fminf(v, d[at(x, y - 1)] + 1.0F);
                if (x > 0) {
                    v = fminf(v, d[at(x - 1, y - 1)] + diag);
                }
                if (x + 1 < w) {
                    v = fminf(v, d[at(x + 1, y - 1)] + diag);
                }
            }
            d[at(x, y)] = v;
        }
    }
    for (std::size_t y = h; y-- > 0;) {
        for (std::size_t x = w; x-- > 0;) {
            float v = d[at(x, y)];
            if (x + 1 < w) {
                v = fminf(v, d[at(x + 1, y)] + 1.0F);
            }
            if (y + 1 < h) {
                v = fminf(v, d[at(x, y + 1)] + 1.0F);
                if (x + 1 < w) {
                    v = fminf(v, d[at(x + 1, y + 1)] + diag);
                }
                if (x > 0) {
                    v = fminf(v, d[at(x - 1, y + 1)] + diag);
                }
            }
            d[at(x, y)] = v;
        }
    }
    return d;
}

float smoothstep(float a, float b, float x) {
    const float t = clampf((x - a) / (b - a), 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

float deep_water(float depth) {
    return smoothstep(kWaves.deep_from, kWaves.deep_to, depth);
}

float wave_detail(float tw) {
    return smoothstep(24.0F, 64.0F, tw);
}

namespace {

/// Hash of an integer lattice point to [0, 1).
float hash3(int x, int y, int z) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8DA6B343U ^
                      static_cast<std::uint32_t>(y) * 0xD8163841U ^
                      static_cast<std::uint32_t>(z) * 0xCB1AB31FU;
    h ^= h >> 13;
    h *= 0x5BD1E995U;
    h ^= h >> 15;
    return static_cast<float>(h & 0x00FFFFFFU) / 16777216.0F;
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

/// Irregular surf clock (in wave periods), monotonic.
float surf_time(std::pair<float, float> pos, float t) {
    const float c = t / kWaves.period_ms;
    return c + 0.6F * noise3(pos.first * 0.08F + 2.3F, pos.second * 0.08F, c * 0.3F);
}

/// Slow bending of the crests (tiles of phase).
float surf_bend(std::pair<float, float> pos, float t) {
    return 1.4F * noise3(pos.first * 0.12F, pos.second * 0.12F, t * 0.00003F) +
           0.5F * noise3(pos.first * 0.35F + 5.0F, pos.second * 0.35F, t * 0.00005F);
}

/// Phase of the surf system: crests on lines of equal shore distance.
float surf_phase(float dist, std::pair<float, float> pos, float t) {
    return dist / kWaves.surf_wavelength + surf_time(pos, t) + 0.35F * surf_bend(pos, t);
}

float time_f(std::uint64_t time_ms) {
    return static_cast<float>(time_ms % 3600000);
}

} // namespace

float noise3(float x, float y, float z) {
    const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    const int ix = sat_cast<int>(fx), iy = sat_cast<int>(fy), iz = sat_cast<int>(fz);
    auto fade = [](float t) { return t * t * (3.0F - 2.0F * t); };
    const float u = fade(x - fx), v = fade(y - fy), w = fade(z - fz);
    auto corner = [&](int dx, int dy, int dz) { return hash3(ix + dx, iy + dy, iz + dz); };
    auto plane = [&](int dz) {
        return lerp(lerp(corner(0, 0, dz), corner(1, 0, dz), u),
                    lerp(corner(0, 1, dz), corner(1, 1, dz), u), v);
    };
    return lerp(plane(0), plane(1), w);
}

float noise3_at(float x, float y, int iz) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = sat_cast<int>(fx), iy = sat_cast<int>(fy);
    auto fade = [](float t) { return t * t * (3.0F - 2.0F * t); };
    const float u = fade(x - fx), v = fade(y - fy);
    auto corner = [&](int dx, int dy) { return hash3(ix + dx, iy + dy, iz); };
    return lerp(lerp(corner(0, 0), corner(1, 0), u), lerp(corner(0, 1), corner(1, 1), u), v);
}

float NoisePlane::at(float x, float y) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int ix = sat_cast<int>(fx), iy = sat_cast<int>(fy);
    if (!valid_ || ix != cx_ || iy != cy_) {
        hash_ = {hash3(ix, iy, iz_), hash3(ix + 1, iy, iz_), hash3(ix, iy + 1, iz_),
                 hash3(ix + 1, iy + 1, iz_)};
        cx_ = ix;
        cy_ = iy;
        valid_ = true;
    }
    auto fade = [](float t) { return t * t * (3.0F - 2.0F * t); };
    const float u = fade(x - fx), v = fade(y - fy);
    return lerp(lerp(hash_[0], hash_[1], u), lerp(hash_[2], hash_[3], u), v);
}

float wave_height(float k, std::pair<float, float> pos) {
    const float set = noise3(pos.first * 0.05F + k * 0.19F, pos.second * 0.05F, 7.1F);
    const float each = noise3(pos.first * 0.3F, pos.second * 0.3F, k * 1.37F + 0.5F);
    return (0.3F + 0.7F * smoothstep(0.2F, 0.8F, set)) * (0.45F + 0.55F * each);
}

float runup_height(float a, float slope) {
    const Waves& w = kWaves;
    const float h = a * w.wave_height;
    const float tan_b = fmaxf(slope * 0.04F, 0.004F);
    const float xi = fminf(tan_b / std::sqrt(h * 0.04F / w.surf_wavelength), 2.0F);
    return fminf(w.runup_gain * xi * h, w.runup_max_tiles * fmaxf(slope, 0.2F));
}

std::array<float, 3> swash(float dist, float slope, std::pair<float, float> pos,
                           std::uint64_t time_ms) {
    const float t = time_f(time_ms);
    const float p = surf_phase(dist, pos, t) - 0.9F;
    const float k = std::floor(p);
    const float since = p - k;
    const float a = wave_height(k, pos);
    const float up = 0.08F + 0.1F * a;
    const float shape = since < up
                            ? smoothstep(0.0F, up, since)
                            : std::pow(1.0F - fminf((since - up) / (0.85F - up), 1.0F), 1.6F);
    return {runup_height(a, slope) * shape, since, a};
}

float swash_runup(float dist, float slope, std::pair<float, float> pos, std::uint64_t time_ms) {
    return swash(dist, slope, pos, time_ms)[0];
}

Shade wave_shade(float dist, float depth, float beach_slope, std::pair<float, float> pos,
                 std::uint64_t time_ms, float detail) {
    const Waves& w = kWaves;
    const auto [wx, wy] = pos;
    const float t = time_f(time_ms);
    const float s1 = noise3(wx * 0.22F + t * 0.00011F, wy * 0.22F - t * 0.00004F, t * 0.00006F);
    const float s2 = noise3(wx * 0.55F - t * 0.00019F, wy * 0.55F + t * 0.00013F, t * 0.00011F);
    float light = w.swell * 2.0F * (0.65F * s1 + 0.35F * s2 - 0.5F);
    float foam = 0.0F;
    const float near = smoothstep(w.surf_zone, w.surf_zone * 0.5F, dist);
    if (near > 0.0F) {
        // Two wave systems blended by amplitude: swell from one direction,
        // surf parallel to the shore (docs/render/iso.md, "Enhanced water").
        const float bend = surf_bend(pos, t);
        const float along = -(wx * w.swell_dir[0] + wy * w.swell_dir[1]);
        const float swell_phase = along / w.wavelength + surf_time(pos, t) + bend;
        const float sphase = surf_phase(dist, pos, t);
        const float a = wave_height(std::floor(sphase - 0.4F), pos);
        const float ws = smoothstep(w.surf_zone * 0.9F, w.surf_zone * 0.35F, dist);
        // Height profile, crest at u = 0.9: gentle back, steep front.
        auto profile = [](float u) {
            const float d = u - 0.9F;
            return d < 0.0F ? std::exp(-sq(d / 0.3F)) : std::exp(-sq(d / 0.05F));
        };
        // (height, slope towards the travel direction, foam line, trail)
        auto crest = [&](float phase) {
            const float u = phase - std::floor(phase);
            const float height = profile(u);
            const float slope =
                (profile(fminf(u + 0.01F, 1.0F)) - profile(fmaxf(u - 0.01F, 0.0F))) * 2.5F;
            const float line = std::exp(-sq((u - 0.9F) / 0.035F));
            const float trail = std::exp(-fmaxf(0.9F - u, 0.0F) * 14.0F);
            return std::array<float, 4>{height, slope, line, trail};
        };
        const auto cs = crest(swell_phase), cf = crest(sphase);
        const float surf_amp = 0.3F + 0.7F * a;
        auto mix = [&](std::size_t k) { return cs[k] * (1.0F - ws) + cf[k] * ws * surf_amp; };
        const float height = mix(0), slope = mix(1), line = mix(2), trail = mix(3);
        // Long crests with gentle variation along them.
        const float segment =
            0.55F +
            0.45F * smoothstep(0.3F, 0.7F, noise3(wx * 0.25F + 3.1F, wy * 0.25F, t * 0.00005F));
        // Surf waves break where the depth falls below H / γ.
        const float db =
            w.break_depth * (1.0F - ws) + ws * fmaxf(a * w.wave_height / w.breaker_index, 1.5F);
        const float shoal = smoothstep(w.shoal_depth, db, depth);
        const float breaking = smoothstep(db * 1.4F, db * 0.7F, depth);
        const float bore = 1.0F - 0.7F * smoothstep(db * 0.7F, 0.3F, depth);
        const float amp = near * segment * (0.35F + 0.65F * shoal) * (1.0F - 0.5F * breaking);
        light += amp * (0.10F * slope + 0.10F * height);
        const float fine = noise3(wx * 2.0F, wy * 2.0F, t * 0.0009F);
        const float mottle = 0.55F + detail * (fine - 0.55F);
        if (breaking > 0.0F) {
            // A narrow foam line on the crest with a short whitewater trail.
            const float pieces =
                smoothstep(0.35F, 0.6F, noise3(wx * 0.5F + 9.0F, wy * 0.5F, t * 0.00007F));
            const float surge = breaking * bore * pieces;
            foam = surge * near * segment *
                   fmaxf(0.8F * line, 0.3F * trail * smoothstep(0.25F, 0.7F, mottle));
        }
        if (depth < w.swash_depth) {
            // Swash: a frothy sheet at the moving water edge.
            const auto [run, since, sa] = swash(dist, beach_slope, pos, time_ms);
            const float wet = depth + run;
            const float impact =
                smoothstep(0.0F, 0.06F, since) * std::exp(-since * 3.0F) * (0.35F + 0.65F * sa);
            const float sheet = smoothstep(2.6F, 0.0F, wet) * smoothstep(-0.4F, 0.3F, wet);
            const float froth = smoothstep(0.2F, 0.75F, mottle);
            foam = fmaxf(foam, impact * sheet * (0.35F + 0.6F * froth));
        }
    }
    const float tint = w.shallow_tint * smoothstep(w.tint_depth, 0.0F, depth);
    return {light, clampf(foam, 0.0F, 1.0F), tint};
}

Shade river_shade(std::pair<float, float> pos, float depth, float slope, std::uint64_t time_ms) {
    const auto [wx, wy] = pos;
    const float t = time_f(time_ms);
    const float a = noise3(wx * 1.1F + t * 0.00025F, wy * 1.1F + t * 0.00018F, t * 0.0002F);
    const float b = noise3(wx * 2.7F - t * 0.0003F, wy * 2.7F + t * 0.00022F, t * 0.00035F);
    float light = 0.07F * (0.6F * a + 0.4F * b - 0.5F) * 2.0F;
    const float tint = 0.15F * smoothstep(4.0F, 0.0F, depth);
    // Rapids: a faster, stronger shimmer where the surface falls steeply
    // (tuning, not original: falls were sprites before). No foam on rivers
    // (owner, 2026-10-18: foam only at the sea).
    const float r = smoothstep(kRapidsFrom, kRapidsFull, slope);
    if (r > 0.0F) {
        const float s = noise3(wx * 3.1F + t * 0.0013F, wy * 3.1F + t * 0.0011F, t * 0.0009F);
        const float u = noise3(wx * 7.3F - t * 0.0021F, wy * 7.3F + t * 0.0017F, t * 0.0016F);
        light += r * (0.05F + 0.16F * (0.6F * s + 0.4F * u - 0.5F));
    }
    return {light, 0.0F, tint};
}

float flow_psi(std::array<float, 2> x) {
    return 0.5F * noise3(0.13F * x[0], 0.13F * x[1], 3.0F);
}

FlowPhases flow_phases(std::array<float, 2> x, std::array<float, 2> u, std::uint64_t time_ms,
                       const RiverLayer& layer, float psi) {
    // Hourly wrap: time_f is taken modulo 3,600,000 ms, so every period must
    // divide it (RiverConfig validation), then t / T wraps by whole cycles.
    const float t = time_f(time_ms);
    const float seconds = layer.period_ms * 0.001F;
    FlowPhases out;
    for (std::size_t k = 0; k < 2; ++k) {
        float phi = t / layer.period_ms + psi + 0.5F * static_cast<float>(k);
        phi -= std::floor(phi);
        out.w[k] = 1.0F - std::fabs(2.0F * phi - 1.0F);
        // No stretch along the flow: x is an absolute island coordinate
        // (100-250 tiles from the origin), a stretch about it would move the
        // noise coordinate by (distance) x d(gain)/d(speed) x d(speed)/d(pixel),
        // about ten times the geometry wherever the flow varies.
        const float cx = x[0] - u[0] * seconds * phi;
        const float cy = x[1] - u[1] * seconds * phi;
        // The second phase samples another part of the noise field, so the
        // cross-fade morphs the pattern instead of fading one into itself.
        const float off = static_cast<float>(k);
        out.c[k] = {cx * layer.scale + off * 13.37F, cy * layer.scale + off * 7.91F};
    }
    return out;
}

Shade river_flow_shade(float px, float py, const FlowSample& f, float depth, float slope,
                       std::uint64_t time_ms, float detail, const RiverParams& params,
                       const RiverView& view) {
    // E19: the pattern lives on the river's own water plane, not on the sea
    // plane: the pixel's tile position at the height of the river surface.
    const float pyw = py + view.lambda * f.level;
    const std::array<float, 2> x{px / view.tw + pyw / view.th, pyw / view.th - px / view.tw};
    const std::array<float, 2> u{f.vx, f.vy};
    // Light direction: the tile x axis (as fluid_shade), probe offset e.
    constexpr float kProbe = 0.15F;
    const std::array<float, 2> x2{x[0] - kProbe, x[1]};
    const float psi = flow_psi(x);
    const float rapids = smoothstep(kRapidsFrom, kRapidsFull, slope);
    float light = 0.0F;
    const std::size_t n = std::min<std::size_t>(params.count, params.layers.size());
    for (std::size_t l = 0; l < n; ++l) {
        const RiverLayer& layer = params.layers[l];
        if (params.quality != RiverQuality::High && layer.fades != 0.0F) {
            continue;
        }
        // Shear stretches a feature by T |grad u|: fade the layer out where the
        // cycle would smear it (thin rivers, the margins, falls).
        const float shear_fade = 1.0F - smoothstep(params.shear_from, params.shear_to,
                                                   layer.period_ms * 0.001F * f.shear);
        float amp = layer.amp * shear_fade;
        if (layer.fades != 0.0F) {
            amp *= detail * (1.0F + f.turb);
        }
        if (amp == 0.0F || (layer.rapids != 0.0F && rapids <= 0.0F)) {
            continue;
        }
        // Variance-preserving blend of the two phases (E18) of the centred noise.
        auto field = [&](const FlowPhases& ph) {
            const float m0 = noise3(ph.c[0][0], ph.c[0][1], 0.5F) - 0.5F;
            const float m1 = noise3(ph.c[1][0], ph.c[1][1], 0.5F) - 0.5F;
            const float norm = std::sqrt(ph.w[0] * ph.w[0] + ph.w[1] * ph.w[1]);
            return (ph.w[0] * m0 + ph.w[1] * m1) / norm;
        };
        const FlowPhases a = flow_phases(x, u, time_ms, layer, psi);
        const float na = field(a);
        if (layer.rapids != 0.0F) {
            light += rapids * (params.rapids_base + amp * na);
        } else if (params.quality == RiverQuality::Low) {
            light += amp * kLowTapGain * layer.scale * na;
        } else {
            const FlowPhases b = flow_phases(x2, u, time_ms, layer, psi);
            light += amp * (na - field(b)) / kProbe;
        }
    }
    const float tint = 0.15F * smoothstep(4.0F, 0.0F, depth);
    return {light, 0.0F, tint};
}

Px shade_apply(Px rgba, Shade s) {
    const Waves& w = kWaves;
    const float brightness = 1.0F + s.light;
    auto channel = [&](std::uint8_t c, float shallow, float white) {
        float v = static_cast<float>(c);
        v = (v + (shallow * 255.0F - v) * s.tint) * brightness;
        v = v + (white - v) * s.foam;
        return sat_cast<std::uint8_t>(clampf(v, 0.0F, 255.0F));
    };
    return {channel(rgba[0], w.shallow_colour[0], 236.0F),
            channel(rgba[1], w.shallow_colour[1], 244.0F),
            channel(rgba[2], w.shallow_colour[2], 250.0F), rgba[3]};
}

} // namespace anno::render
