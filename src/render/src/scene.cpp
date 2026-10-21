#include <algorithm>
#include <cassert>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <iterator>
#include <map>
#include <mutex>
#include <numeric>
#include <thread>

#include <anno/render/frame.hpp>
#include <anno/render/profile.hpp>
#include <anno/render/scene.hpp>

#include "thread_priority.hpp"

namespace anno::render {

namespace insel = formats::insel;

namespace {

/// States of a FlowSlot.
constexpr std::uint8_t kFlowIdle = 0, kFlowQueued = 1, kFlowReady = 2, kFlowFailed = 3;

} // namespace

/// The flow of one island and how far its build is (rivers.md 3.1).
struct Scene::FlowSlot {
    std::once_flag once;
    std::atomic<std::uint8_t> state{kFlowIdle};
    /// Written once under `once`, published by `state` (release).
    std::shared_ptr<const river::RiverFlow> flow;
};

/// The background builders of river flows: a queue of island indices.
struct Scene::FlowBuild {
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::size_t> queue;
    bool stop = false, paused = false;
    std::vector<std::thread> threads;
};

int margin(std::size_t group) {
    return kMargin[group] + kHeightLift.pixels(255, group);
}

std::vector<Placement> layout(std::span<const IslandBox> boxes, std::size_t group) {
    const auto [tw, th] = kTile[group];
    std::vector<std::size_t> order(boxes.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    auto key = [&](std::size_t i) {
        const IslandBox& b = boxes[i];
        return std::tuple{std::uint32_t{b.x} + b.width + b.y + b.height, b.x, i};
    };
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return key(a) < key(b); });
    std::vector<Placement> out;
    out.reserve(order.size());
    for (const std::size_t index : order) {
        const IslandBox& b = boxes[index];
        const auto offset = tile_top(b.x, b.y, group);
        const int w = b.width, h = b.height;
        // Same rectangle as Canvas::for_island with the margin.
        const int left = -h * tw / 2;
        const int bottom = (w + h) * th / 2;
        const Rect bounds{offset.first + left, offset.second - margin(group), w * tw / 2 - left,
                          bottom + margin(group)};
        out.push_back(Placement{index, offset, bounds});
    }
    return out;
}

std::optional<Rect> scene_bounds(std::span<const Placement> placements) {
    std::optional<Rect> r;
    for (const auto& p : placements) {
        r = r ? r->unite(p.bounds) : p.bounds;
    }
    return r;
}

Canvas blank(Rect rect) {
    Canvas c;
    c.width = static_cast<std::size_t>(std::max(rect.w, 0));
    c.height = static_cast<std::size_t>(std::max(rect.h, 0));
    c.origin = {rect.x, rect.y};
    c.pixels.assign(c.width * c.height * 4, 0);
    return c;
}

void composite(Canvas& dst, const Canvas& src, std::pair<int, int> offset) {
    const Rect src_rect{src.origin.first + offset.first, src.origin.second + offset.second,
                        static_cast<int>(src.width), static_cast<int>(src.height)};
    const Rect dst_rect{dst.origin.first, dst.origin.second, static_cast<int>(dst.width),
                        static_cast<int>(dst.height)};
    const auto r = src_rect.intersect(dst_rect);
    if (!r) {
        return;
    }
    const auto w = static_cast<std::size_t>(r->w);
    for (int row = 0; row < r->h; ++row) {
        const std::size_t so = (static_cast<std::size_t>(r->y + row - src_rect.y) * src.width +
                                static_cast<std::size_t>(r->x - src_rect.x)) *
                               4;
        const std::size_t d_o = (static_cast<std::size_t>(r->y + row - dst_rect.y) * dst.width +
                                 static_cast<std::size_t>(r->x - dst_rect.x)) *
                                4;
        for (std::size_t i = 0; i < w; ++i) {
            over(&dst.pixels[d_o + i * 4], &src.pixels[so + i * 4]);
        }
        if (!dst.base.empty() && !src.base.empty()) {
            for (std::size_t i = 0; i < w; ++i) {
                if (src.pixels[so + i * 4 + 3] != 0) {
                    dst.base[d_o / 4 + i] = src.base[so / 4 + i];
                }
            }
        }
    }
}

std::optional<Px> sea_floor_colour(std::span<const insel::Island> islands,
                                   const assets::island::GroundTextures& textures) {
    std::map<std::pair<std::uint8_t, std::uint8_t>, std::uint64_t> count;
    for (const auto& island : islands) {
        auto h = island.heights();
        auto g = island.gfx_ids();
        if (!h || !g) {
            continue;
        }
        const std::size_t n = std::min(h.value().size(), g.value().size());
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint8_t tex = insel::GroundCell::from_gfx_id(g.value()[i]).texture;
            if (tex != 0) {
                ++count[{h.value()[i], tex}];
            }
        }
    }
    if (count.empty()) {
        return std::nullopt;
    }
    const std::uint8_t lowest = count.begin()->first.first;
    // The most common texture at that height (ties: lowest id).
    std::optional<std::pair<std::uint8_t, std::uint64_t>> best;
    for (const auto& [k, n] : count) {
        if (k.first != lowest) {
            continue;
        }
        if (!best || n > best->second) {
            best = std::pair{k.second, n};
        }
    }
    return textures.average(best->first);
}

std::pair<int, int> UnitSprite::anchor(std::size_t group, Lift lift) const {
    const auto [tw, th] = kTile[group];
    const int sx = sat_cast<int>(roundf((x - y) * static_cast<float>(tw) / 2.0F));
    const int sy = sat_cast<int>(roundf((x + y) * static_cast<float>(th) / 2.0F));
    // Height in 1/1024 units, so sloped ground lifts smoothly.
    const int raise = z ? lift.scaled(sat_cast<std::int64_t>(roundf(*z * 1024.0F)), 1024, group)
                        : lift.scaled(kSeaLevel, 1, group);
    return {sx, sy - raise};
}

bool UnitSprite::draw(Canvas& canvas, std::size_t group, Lift lift,
                      std::pair<int, int> offset) const {
    if (group >= frames.size() || !frames[group] || !frames[group]->image) {
        return false;
    }
    const UnitFrame& f = *frames[group];
    const auto a = anchor(group, lift);
    const std::pair<int, int> at{a.first - offset.first, a.second - offset.second};
    auto all = [](int, int, Px px) -> std::optional<Px> { return px; };
    if (group < shadows.size() && shadows[group] && shadows[group]->image) {
        blit_sprite(canvas, *shadows[group]->image, shadows[group]->frame, at, true, all);
    }
    const int left = at.first - f.frame.anchor_x, top = at.second - f.frame.anchor_y;
    const bool hit = canvas.overlaps(left, top, f.frame.width, f.frame.height);
    blit_sprite(canvas, *f.image, f.frame, at, false, all);
    return hit;
}

Scene::Scene(std::string name_, std::vector<insel::Island> islands_,
             assets::island::GroundTextures textures_, formats::anlagen::AnlagenFile anlagen_,
             Lift lift_)
    : name(std::move(name_)), islands(std::move(islands_)), textures(std::move(textures_)),
      anlagen(std::move(anlagen_)), lift(lift_) {
    if (const auto c = sea_floor_colour(islands, textures)) {
        textures.set_water(*c);
    }
    std::vector<IslandBox> boxes;
    for (const auto& i : islands) {
        boxes.push_back(IslandBox::of(i));
    }
    for (std::size_t g = 0; g < kGroups; ++g) {
        layouts[g] = layout(boxes, g);
    }
    for (const auto& i : islands) {
        auto list = [&](Objects which) {
            return prepare_objects(i, anlagen, kObjectRecords, which);
        };
        IslandData d;
        auto in = [](const auto& ids, const insel::PlacedObject& o) {
            return std::find(ids.begin(), ids.end(), o.anlage) != ids.end();
        };
        // Enhanced water: river pieces become the river surface
        // (sea::IslandWater); waterfalls and mouths give it their shape
        // (falls_below/falls_above, Scene::water_field_from); surf is drawn
        // by the sea itself.
        d.below = list(Objects::WaterBelowSea);
        for (const auto& p : d.below) {
            if (in(kRiverFallAnlagen, p.first)) {
                d.falls_below.push_back(p);
            } else if (p.first.anlage != kSurfAnlage) {
                d.below_enhanced.push_back(p);
            }
        }
        d.above = list(Objects::WaterAboveSea);
        for (const auto& p : d.above) {
            if (in(kRiverAnlagen, p.first)) {
                d.river_sprites.push_back(p);
                d.rivers.emplace_back(p.first.x, p.first.y, p.second.first, p.second.second,
                                      p.first.height());
            } else if (in(kRiverFallAnlagen, p.first)) {
                d.falls_above.push_back(p);
            } else {
                d.above_enhanced.push_back(p);
            }
        }
        d.other = list(Objects::NotWater);
        auto gfx = i.gfx_ids();
        auto heights = i.heights();
        if (gfx && heights) {
            d.grids = std::pair{std::move(gfx).value(), std::move(heights).value()};
        }
        objects_.push_back(std::move(d));
        flow_slots_.push_back(std::make_unique<FlowSlot>());
    }
}

const sea::SeaPattern* Scene::sea(std::size_t group) const {
    SeaSlot& s = seas_[group];
    std::call_once(s.once, [&] {
        s.pattern = sea::SeaPattern::build(anlagen, group, lift, textures.water(), sprites);
        s.ready.store(true, std::memory_order_release);
    });
    return s.pattern ? &*s.pattern : nullptr;
}

const sea::SeaPattern* Scene::sea_if_ready(std::size_t group) const {
    const SeaSlot& s = seas_[group];
    if (!s.ready.load(std::memory_order_acquire)) {
        return nullptr;
    }
    return s.pattern ? &*s.pattern : nullptr;
}

bool Scene::sea_built(std::size_t group) const {
    return seas_[group].ready.load(std::memory_order_acquire);
}

std::pair<bool, bool> Scene::has_water_objects(std::size_t index) const {
    const auto& o = objects_[index];
    return {!o.below.empty(), !o.above.empty()};
}

std::vector<SpriteDraw> Scene::sprites_in(Rect rect, std::size_t group, std::uint64_t time_ms,
                                          SpriteLayer layer, WaterStyle style) const {
    std::vector<SpriteDraw> out;
    for (const auto& p : layouts[group]) {
        if (!p.bounds.intersect(rect)) {
            continue;
        }
        const auto& o = objects_[p.index];
        const std::vector<Prepared>& list = layer == SpriteLayer::WaterBelow   ? o.below_list(style)
                                            : layer == SpriteLayer::WaterAbove ? o.above_list(style)
                                                                               : o.other;
        const std::size_t start = out.size();
        place_prepared(
            list, anlagen, group, lift, time_ms,
            [&](int x, int y, int w, int h) {
                return Rect{x + p.offset.first, y + p.offset.second, w, h}
                    .intersect(rect)
                    .has_value();
            },
            out);
        for (std::size_t k = start; k < out.size(); ++k) {
            auto& d = out[k];
            d.anchor = {d.anchor.first + p.offset.first, d.anchor.second + p.offset.second};
            if (layer != SpriteLayer::Objects && style == WaterStyle::Enhanced && !d.is_shadow()) {
                std::get<1>(d.key) = kSmoothed;
            }
        }
    }
    return out;
}

const sea::IslandWater* Scene::island_water(std::size_t i) const {
    IslandData& d = objects_[i];
    std::call_once(*d.water_once, [&] {
        const profile::Scope stage(profile::Stage::island_water);
        d.water = sea::IslandWater::build(islands[i], d.rivers, river_cfg.gap_patch_tiles);
    });
    return d.water ? &*d.water : nullptr;
}

sea::WaterField Scene::water_field(Rect rect, std::size_t group) const {
    const auto probe = ground_with_probe(rect, group).second;
    return water_field_from(rect, probe, group);
}

namespace {

/// Box blur of the alpha channel of `c` (radius r, two passes of a horizontal
/// and a vertical box = a tent-like kernel; edge pixels repeat). Exact integer
/// sums in sliding windows (the kernel weight is (2r+1)^4 · 255), so the cost
/// per pixel does not grow with r and chunks without any alpha are free.
/// `close`: every pixel with any alpha in its 3 x 3 neighbourhood counts as
/// fully covered first (fills the holes of a 50 % checkerboard sprite).
std::vector<float> blurred_alpha(const Canvas& c, int r, bool close = false) {
    const auto w = static_cast<int>(c.width), h = static_cast<int>(c.height);
    std::vector<float> out(c.width * c.height, 0.0F);
    std::vector<std::uint32_t> a(out.size());
    bool any = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = c.pixels[i * 4 + 3];
        any = any || a[i] != 0;
    }
    if (!any) {
        return out;
    }
    std::vector<std::uint32_t> tmp(a.size());
    if (close) {
        // 3 x 3 maximum as two 1-D maxima, then all-or-nothing.
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const auto i = static_cast<std::size_t>(y * w + x);
                tmp[i] = std::max({a[i], x > 0 ? a[i - 1] : 0U, x + 1 < w ? a[i + 1] : 0U});
            }
        }
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const auto i = static_cast<std::size_t>(y * w + x);
                const std::uint32_t m =
                    std::max({tmp[i], y > 0 ? tmp[i - static_cast<std::size_t>(w)] : 0U,
                              y + 1 < h ? tmp[i + static_cast<std::size_t>(w)] : 0U});
                a[i] = m != 0 ? 255U : 0U;
            }
        }
    }
    // One box filter of radius r along a line of `count` samples starting at
    // `first` with stride `step`, from `src` into `dst`.
    auto line = [r](const std::vector<std::uint32_t>& src, std::vector<std::uint32_t>& dst,
                    std::size_t first, std::size_t step, int count) {
        auto at = [&](int k) {
            return src[first + static_cast<std::size_t>(std::clamp(k, 0, count - 1)) * step];
        };
        std::uint32_t sum = 0;
        for (int k = -r; k <= r; ++k) {
            sum += at(k);
        }
        for (int x = 0; x < count; ++x) {
            dst[first + static_cast<std::size_t>(x) * step] = sum;
            sum += at(x + r + 1);
            sum -= at(x - r);
        }
    };
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < h; ++y) {
            line(a, tmp, static_cast<std::size_t>(y) * static_cast<std::size_t>(w), 1, w);
        }
        for (int x = 0; x < w; ++x) {
            line(tmp, a, static_cast<std::size_t>(x), static_cast<std::size_t>(w), h);
        }
    }
    const float n = static_cast<float>(2 * r + 1);
    const float norm = 1.0F / (255.0F * n * n * n * n);
    for (std::size_t i = 0; i < a.size(); ++i) {
        out[i] = static_cast<float>(a[i]) * norm;
    }
    return out;
}

/// Two-pass chamfer distance (1, √2) in place: 0 at the seeds, else large.
void chamfer(std::vector<float>& d, std::size_t w, std::size_t h) {
    constexpr float kDiag = 1.41421356F;
    auto relax = [&](std::size_t i, std::size_t j, float step) {
        d[i] = std::min(d[i], d[j] + step);
    };
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            if (x > 0) {
                relax(i, i - 1, 1.0F);
            }
            if (y > 0) {
                relax(i, i - w, 1.0F);
                if (x > 0) {
                    relax(i, i - w - 1, kDiag);
                }
                if (x + 1 < w) {
                    relax(i, i - w + 1, kDiag);
                }
            }
        }
    }
    for (std::size_t y = h; y-- > 0;) {
        for (std::size_t x = w; x-- > 0;) {
            const std::size_t i = y * w + x;
            if (x + 1 < w) {
                relax(i, i + 1, 1.0F);
            }
            if (y + 1 < h) {
                relax(i, i + w, 1.0F);
                if (x + 1 < w) {
                    relax(i, i + w + 1, kDiag);
                }
                if (x > 0) {
                    relax(i, i + w - 1, kDiag);
                }
            }
        }
    }
}

} // namespace

sea::WaterField Scene::water_field_from(Rect rect, const sea::SceneProbe& probe,
                                        std::size_t group) const {
    std::optional<profile::Scope> stage(std::in_place, profile::Stage::sea_field);
    sea::WaterField f =
        sea::water_field(rect, probe, [this](std::size_t i) { return island_water(i); });
    stage.emplace(profile::Stage::river_sprites);
    // Enhanced river water takes the shapes of the waterfall and mouth
    // sprites (their alpha, softened): the river runs through the falls into
    // the sea as one surface instead of ending at hard-edged sprite patches
    // (tester feedback 2026-10-18). Falls get rapids, mouths stay calm.
    // Original water ignores river pixels (flag bit 1) and draws the sprites.
    // The three masks are created on the first sprite pixel that reaches them:
    // compositing a transparent canvas changes nothing, and with all three
    // blank apply_fall_masks changes nothing either (af = am = ab = 0 gives
    // a = 0 and bed = -kBedEdge * kBedDepth = -2 <= -0.5 at every pixel), so a
    // chunk without river sprite pixels skips both.
    static_assert((0.0F - kBedEdge) * kBedDepth <= -0.5F,
                  "blank masks must leave apply_fall_masks without effect");
    Canvas falls, mouths, beds;
    auto add = [&](Canvas& mask, const Canvas& local, std::pair<int, int> off) {
        bool opaque = false;
        for (std::size_t k = 3; k < local.pixels.size(); k += 4) {
            if (local.pixels[k] != 0) {
                opaque = true;
                break;
            }
        }
        if (!opaque) {
            return;
        }
        if (mask.pixels.empty()) {
            mask = blank(rect);
        }
        composite(mask, local, off);
    };
    bool any = false;
    for (const auto& p : layouts[group]) {
        const auto r = p.bounds.intersect(rect);
        if (!r) {
            continue;
        }
        const auto& o = objects_[p.index];
        if (o.falls_below.empty() && o.falls_above.empty() && o.river_sprites.empty()) {
            continue;
        }
        any = true;
        const auto off = p.offset;
        // At or above the sea surface: the original's depth test (as the
        // water objects in render_with_terrain).
        auto keep = [&](int x, int y, std::uint8_t h) {
            const std::optional<float> z = terrain_at(probe, rect, x + off.first, y + off.second);
            return !z || *z <= static_cast<float>(h) + kRiverTolerance;
        };
        auto all = [](int, int, std::uint8_t) { return true; };
        // The river pieces' own sprites (depth-tested as the original draws them)
        // give the extent of the painted bed: the river water never ends short
        // of it (apply_fall_masks, `beds`).
        if (!o.river_sprites.empty()) {
            Canvas local = blank(Rect{r->x - off.first, r->y - off.second, r->w, r->h});
            draw_prepared_tested(local, o.river_sprites, anlagen, group, lift, 0, sprites, 0, keep);
            add(beds, local, off);
        }
        for (const bool fall : {true, false}) {
            std::vector<Prepared> below, above;
            for (const auto& q : o.falls_below) {
                if ((q.first.anlage == kRiverFallAnlagen[0]) == fall) {
                    below.push_back(q);
                }
            }
            for (const auto& q : o.falls_above) {
                if ((q.first.anlage == kRiverFallAnlagen[0]) == fall) {
                    above.push_back(q);
                }
            }
            if (below.empty() && above.empty()) {
                continue;
            }
            Canvas local = blank(Rect{r->x - off.first, r->y - off.second, r->w, r->h});
            draw_prepared_tested(local, below, anlagen, group, lift, 0, sprites, 0, all);
            draw_prepared_tested(local, above, anlagen, group, lift, 0, sprites, 0, keep);
            add(fall ? falls : mouths, local, off);
        }
    }
    if (any && !(falls.pixels.empty() && mouths.pixels.empty() && beds.pixels.empty())) {
        stage.emplace(profile::Stage::fall_masks);
        for (Canvas* mask : {&falls, &mouths, &beds}) {
            if (mask->pixels.empty()) {
                *mask = blank(rect);
            }
        }
        apply_fall_masks(f, falls, mouths, beds, group);
    }
    stage.emplace(profile::Stage::river_blend);
    blend_rivers_into_sea(f, group);
    return f;
}

void Scene::apply_fall_masks(sea::WaterField& f, const Canvas& falls, const Canvas& mouths,
                             const Canvas& beds, std::size_t group) const {
    // Soft, rounded edges: about 4 px at zoom group 0.
    const int radius = std::max(1, static_cast<int>(kTile[group].h) / 10);
    const std::vector<float> af = blurred_alpha(falls, radius), am = blurred_alpha(mouths, radius),
                             ab = blurred_alpha(beds, radius, true);
    auto byte = [](float v) { return sat_cast<std::uint8_t>(clampf(v, 0.0F, 255.0F)); };
    for (std::size_t i = 0; i < f.flags.size(); ++i) {
        const float a = fmaxf(af[i], am[i]);
        // River pieces: the closed sprite mask, blurred, is 1 inside the bed
        // and 0.5 on its edge; the depth is 0 there (kBedDepth per unit of
        // alpha above it), so even a neck of a few pixels is full water.
        const float bed = (ab[i] - kBedEdge) * kBedDepth;
        if (a <= 0.01F && bed <= -0.5F) {
            continue;
        }
        const float sd = (static_cast<float>(f.depth[i]) - sea::kDepthBias) / sea::kDepthScale;
        // The original's sea pixels follow the tiles; where the enhanced
        // (smooth) sea has no water, the shape fills in. Their bit 0 and
        // cover stay, so the original water is unchanged.
        const bool sea_px = (f.flags[i] & 1U) != 0;
        if (sea_px && sd > 0.0F) {
            continue; // the enhanced sea is here
        }
        // Depth from the mask: shallow at its edge (the soft waterline).
        float rd = a > 0.01F ? a * kFallDepth - 0.5F : bed;
        rd = fmaxf(rd, bed);
        std::uint32_t q = 0;
        if ((f.flags[i] & 2U) != 0) {
            rd = fmaxf(rd, sd);
            q = std::uint32_t{f.flags[i]} >> 2U;
        }
        // Rapids on the falls (slope_of: 1/2 units per step).
        q = std::max(q, sat_cast<std::uint32_t>(roundf(af[i] * kFallRapids * 2.0F)));
        if (!sea_px) {
            f.cover[i] = 1 + sea::kRiverOpenness;
            f.shore[i] = 255;
        }
        f.depth[i] = byte(sea::kDepthBias + rd * sea::kDepthScale);
        f.flags[i] = static_cast<std::uint8_t>((sea_px ? 1U : 0U) | 2U | std::min(q, 63U) << 2U);
    }
}

void Scene::blend_rivers_into_sea(sea::WaterField& f, std::size_t group) const {
    // Where river water meets the sea, its look turns into the sea's over
    // kRiverBlend pixels (scaled with the zoom): distance to the nearest
    // enhanced sea water (two-pass chamfer within the rect) and that
    // water's cover. The shader and shade_sea mix river and sea shading by
    // the distance stored in `shore` (river pixels; 255 = far).
    const auto w = static_cast<std::size_t>(f.rect.w), h = static_cast<std::size_t>(f.rect.h);
    const float reach = kRiverBlend * static_cast<float>(kTile[group].h) / 42.0F;
    constexpr float kFar = 1e9F;
    std::vector<float> dist(w * h, kFar);
    std::vector<std::uint8_t> near_cover(w * h, 0);
    bool rivers = false, sea = false;
    for (std::size_t i = 0; i < w * h; ++i) {
        const float sd = (static_cast<float>(f.depth[i]) - sea::kDepthBias) / sea::kDepthScale;
        if ((f.flags[i] & 2U) != 0) {
            rivers = true;
        } else if (f.cover[i] != 0 && sd > 0.0F) {
            dist[i] = 0.0F;
            near_cover[i] = f.cover[i];
            sea = true;
        }
    }
    if (!rivers || !sea) {
        return;
    }
    auto relax = [&](std::size_t i, std::size_t j, float step) {
        if (dist[j] + step < dist[i]) {
            dist[i] = dist[j] + step;
            near_cover[i] = near_cover[j];
        }
    };
    constexpr float kDiag = 1.41421356F;
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            if (x > 0) {
                relax(i, i - 1, 1.0F);
            }
            if (y > 0) {
                relax(i, i - w, 1.0F);
                if (x > 0) {
                    relax(i, i - w - 1, kDiag);
                }
                if (x + 1 < w) {
                    relax(i, i - w + 1, kDiag);
                }
            }
        }
    }
    for (std::size_t y = h; y-- > 0;) {
        for (std::size_t x = w; x-- > 0;) {
            const std::size_t i = y * w + x;
            if (x + 1 < w) {
                relax(i, i + 1, 1.0F);
            }
            if (y + 1 < h) {
                relax(i, i + w, 1.0F);
                if (x + 1 < w) {
                    relax(i, i + w + 1, kDiag);
                }
                if (x > 0) {
                    relax(i, i + w - 1, kDiag);
                }
            }
        }
    }
    // Sea pixels at the sea's own waterline between river water and sea
    // water would turn transparent (a sand line between the two): they join
    // the river surface with the sea's look (shore 0), fully water.
    std::vector<float> to_river(w * h, kFar);
    for (std::size_t i = 0; i < w * h; ++i) {
        if ((f.flags[i] & 2U) != 0) {
            to_river[i] = 0.0F;
        }
    }
    chamfer(to_river, w, h);
    // ... and to full sea water: only pixels in a narrow gap between the
    // two are filled, the open shoreline stays as it is.
    std::vector<float> to_sea(w * h, kFar);
    for (std::size_t i = 0; i < w * h; ++i) {
        const float sd = (static_cast<float>(f.depth[i]) - sea::kDepthBias) / sea::kDepthScale;
        if ((f.flags[i] & 2U) == 0 && f.cover[i] != 0 && sd >= kSeamDepth) {
            to_sea[i] = 0.0F;
        }
    }
    chamfer(to_sea, w, h);
    const float seam = kRiverSeam * static_cast<float>(kTile[group].h) / 42.0F;
    for (std::size_t i = 0; i < w * h; ++i) {
        const float sd = (static_cast<float>(f.depth[i]) - sea::kDepthBias) / sea::kDepthScale;
        if ((f.flags[i] & 2U) != 0 || f.cover[i] == 0 || sd >= kSeamDepth ||
            to_river[i] + to_sea[i] > seam) {
            continue;
        }
        f.flags[i] = static_cast<std::uint8_t>((f.flags[i] & 1U) | 2U);
        f.shore[i] = 0;
        f.depth[i] = sat_cast<std::uint8_t>(
            clampf(sea::kDepthBias + kSeamDepth * sea::kDepthScale, 0.0F, 255.0F));
    }
    for (std::size_t i = 0; i < w * h; ++i) {
        if ((f.flags[i] & 2U) == 0 || dist[i] >= reach) {
            continue;
        }
        const float k = dist[i] / reach;
        f.shore[i] = sat_cast<std::uint8_t>(roundf(k * 254.0F));
        if ((f.flags[i] & 1U) == 0) {
            const float river = 1.0F + static_cast<float>(sea::kRiverOpenness);
            f.cover[i] =
                sat_cast<std::uint8_t>(roundf(static_cast<float>(near_cover[i]) +
                                              (river - static_cast<float>(near_cover[i])) * k));
        }
    }
}

std::size_t Scene::river_piece_count(std::size_t index) const {
    const auto& o = objects_[index];
    return o.rivers.size() + o.falls_below.size() + o.falls_above.size();
}

Canvas Scene::river_sprites(Rect rect, const sea::SceneProbe& probe, std::size_t group) const {
    Canvas out = blank(rect);
    auto is_river = [](const Prepared& p) {
        const auto id = p.first.anlage;
        return std::find(kRiverAnlagen.begin(), kRiverAnlagen.end(), id) != kRiverAnlagen.end() ||
               std::find(kRiverFallAnlagen.begin(), kRiverFallAnlagen.end(), id) !=
                   kRiverFallAnlagen.end();
    };
    for (const auto& p : layouts[group]) {
        const auto r = p.bounds.intersect(rect);
        if (!r) {
            continue;
        }
        const auto& o = objects_[p.index];
        std::vector<Prepared> below, above;
        std::copy_if(o.below.begin(), o.below.end(), std::back_inserter(below), is_river);
        std::copy_if(o.above.begin(), o.above.end(), std::back_inserter(above), is_river);
        if (below.empty() && above.empty()) {
            continue;
        }
        const auto off = p.offset;
        auto keep = [&](int x, int y, std::uint8_t h) {
            const std::optional<float> z = terrain_at(probe, rect, x + off.first, y + off.second);
            return !z || *z <= static_cast<float>(h) + kRiverTolerance;
        };
        auto all = [](int, int, std::uint8_t) { return true; };
        Canvas local = blank(Rect{r->x - off.first, r->y - off.second, r->w, r->h});
        draw_prepared_tested(local, below, anlagen, group, lift, 0, sprites, 0, all);
        draw_prepared_tested(local, above, anlagen, group, lift, 0, sprites, 0, keep);
        composite(out, local, off);
    }
    return out;
}

float Scene::fluid_depth(float x, float y) const {
    const float waterline = static_cast<float>(kWaterline);
    for (std::size_t k = 0; k < islands.size(); ++k) {
        const auto& isl = islands[k];
        const float lx = x - static_cast<float>(isl.x), ly = y - static_cast<float>(isl.y);
        if (lx < 0.0F || ly < 0.0F || lx >= static_cast<float>(isl.width) ||
            ly >= static_cast<float>(isl.height)) {
            continue;
        }
        if (const auto& g = objects_[k].grids) {
            return waterline - sea::smooth_height(g->second, isl.width, isl.height, lx, ly);
        }
    }
    return waterline;
}

std::optional<float> Scene::terrain_at(const sea::SceneProbe& probe, Rect rect, int sx,
                                       int sy) const {
    if (sx < rect.x || sy < rect.y || sx >= rect.x + rect.w || sy >= rect.y + rect.h) {
        return std::nullopt;
    }
    const auto i = static_cast<std::size_t>((sy - rect.y) * rect.w + (sx - rect.x));
    const std::uint32_t island = probe.island[i];
    if (island == sea::kNoIsland) {
        return std::nullopt;
    }
    const auto& g = objects_[island].grids;
    if (!g) {
        return std::nullopt;
    }
    const auto& isl = islands[island];
    return terrain_height(g->second, isl.width, isl.height, probe.pos[i][0], probe.pos[i][1]);
}

std::vector<std::uint16_t> Scene::terrain_plane(const sea::SceneProbe& probe) const {
    return render::terrain_plane(probe, [this](std::uint32_t island) {
        const auto& g = objects_[island].grids;
        return TerrainGrid{g ? &g->second : nullptr, islands[island].width, islands[island].height};
    });
}

std::vector<std::uint16_t> terrain_plane(const sea::SceneProbe& probe,
                                         const std::function<TerrainGrid(std::uint32_t)>& grid) {
    // terrain_at and the quantisation of Chunk::terrain per pixel, with the
    // island and the tile's corners looked up once per run instead of per
    // pixel; the arithmetic is terrain_height's, so the values are identical.
    std::vector<std::uint16_t> out(probe.island.size(), kNoTerrain);
    std::uint32_t island = sea::kNoIsland;
    const std::vector<std::uint8_t>* heights = nullptr;
    std::size_t w = 0, h = 0;
    std::size_t ctx = 0, cty = 0;
    bool cached = false;
    float a = 0, cc = 0, b = 0, d = 0;
    for (std::size_t i = 0; i < probe.island.size(); ++i) {
        if (probe.island[i] == sea::kNoIsland) {
            continue;
        }
        if (probe.island[i] != island) {
            island = probe.island[i];
            const TerrainGrid g = grid(island);
            heights = g.heights;
            w = g.w;
            h = g.h;
            cached = false;
        }
        if (heights == nullptr) {
            continue;
        }
        const auto [x, y] = probe.pos[i];
        const std::size_t tx = std::min(sat_cast<std::size_t>(fmaxf(x, 0.0F)), w - 1);
        const std::size_t ty = std::min(sat_cast<std::size_t>(fmaxf(y, 0.0F)), h - 1);
        if (!cached || tx != ctx || ty != cty) {
            const auto c = corners(*heights, w, h, tx, ty);
            a = static_cast<float>(c[0]);
            cc = static_cast<float>(c[1]);
            b = static_cast<float>(c[2]);
            d = static_cast<float>(c[3]);
            ctx = tx;
            cty = ty;
            cached = true;
        }
        const float s = clampf(x - static_cast<float>(tx), 0.0F, 1.0F);
        const float t = clampf(y - static_cast<float>(ty), 0.0F, 1.0F);
        const float z = s + t <= 1.0F ? a + s * (cc - a) + t * (b - a)
                                      : d + (1.0F - s) * (b - d) + (1.0F - t) * (cc - d);
        out[i] = sat_cast<std::uint16_t>(
            clampf(roundf(z * kTerrainScale), 0.0F, static_cast<float>(kNoTerrain - 1)));
    }
    return out;
}

std::pair<Canvas, sea::SceneProbe> Scene::ground_with_probe(Rect rect, std::size_t group) const {
    Canvas canvas = blank(rect);
    const std::size_t n = canvas.width * canvas.height;
    sea::SceneProbe probe{std::vector<std::uint32_t>(n, sea::kNoIsland),
                          std::vector<std::array<float, 2>>(n, {0.0F, 0.0F})};
    for (const auto& p : layouts[group]) {
        const auto r = p.bounds.intersect(rect);
        if (!r) {
            continue;
        }
        const auto& island = islands[p.index];
        const auto& g = objects_[p.index].grids;
        if (!g) {
            continue;
        }
        Canvas local = blank(Rect{r->x - p.offset.first, r->y - p.offset.second, r->w, r->h});
        GroundProbe lp = GroundProbe::for_canvas(local);
        draw_ground_grids(local, island.width, island.height, g->first, g->second, textures, group,
                          lift, &lp);
        composite(canvas, local, p.offset);
        const auto idx = static_cast<std::uint32_t>(p.index);
        for (std::size_t k = 0; k < lp.pos.size(); ++k) {
            if (std::isnan(lp.pos[k][0])) {
                continue;
            }
            const int sx = r->x + static_cast<int>(k % local.width);
            const int sy = r->y + static_cast<int>(k / local.width);
            const auto i = static_cast<std::size_t>((sy - rect.y) * rect.w + (sx - rect.x));
            probe.island[i] = idx;
            probe.pos[i] = lp.pos[k];
        }
    }
    return {std::move(canvas), std::move(probe)};
}

std::pair<Canvas, SceneStats> Scene::render(Rect rect, std::size_t group, Layers layers) const {
    return render_with_terrain(rect, group, layers, nullptr);
}

std::pair<Canvas, SceneStats>
Scene::render_with_terrain(Rect rect, std::size_t group, Layers layers,
                           const std::function<std::optional<float>(int, int)>* terrain) const {
    assert(group < kGroups);
    if (terrain != nullptr && !*terrain) {
        terrain = nullptr; // an empty callback means "none"
    }
    Canvas canvas = blank(rect);
    if (layers.bases) {
        canvas.base.assign(canvas.width * canvas.height, 255);
        canvas.base_band = static_cast<int>(kTile[group].h) * 6 / 10;
    }
    SceneStats stats;
    const std::uint64_t t = layers.time_ms;
    std::size_t no_ground = 0, drawn = 0;
    std::optional<sea::SceneProbe> probe;
    const std::size_t water_variant = layers.water_style == WaterStyle::Original ? 0 : kSmoothed;
    // Pass 1: ground and water objects under the sea surface.
    // Pass 2: water objects above it and the other objects.
    for (int pass = 0; pass < 2; ++pass) {
        for (const auto& p : layouts[group]) {
            const auto r = p.bounds.intersect(rect);
            if (!r) {
                continue;
            }
            const auto& island = islands[p.index];
            const auto& objects = objects_[p.index];
            const bool first = pass == 0;
            const bool wanted =
                first ? layers.ground || (layers.water_below && !objects.below.empty())
                      : (layers.water_above && !objects.above.empty()) || layers.objects;
            if (!wanted) {
                continue;
            }
            Canvas local = blank(Rect{r->x - p.offset.first, r->y - p.offset.second, r->w, r->h});
            if (!canvas.base.empty()) {
                local.base.assign(local.width * local.height, 255);
                local.base_band = canvas.base_band;
            }
            if (first) {
                ++drawn;
                if (layers.ground) {
                    if (const auto& g = objects.grids) {
                        draw_ground_grids(local, island.width, island.height, g->first, g->second,
                                          textures, group, lift);
                    } else {
                        ++no_ground;
                    }
                }
                if (layers.water_below) {
                    stats.objects.add(draw_prepared_tested(
                        local, objects.below_list(layers.water_style), anlagen, group, lift, t,
                        sprites, water_variant, [](int, int, std::uint8_t) { return true; }));
                }
            } else {
                if (layers.water_above) {
                    // Depth test against the terrain seen at each pixel, as
                    // the original's depth buffer.
                    if (terrain == nullptr && !probe) {
                        probe = ground_with_probe(rect, group).second;
                    }
                    const auto off = p.offset;
                    auto keep = [&](int x, int y, std::uint8_t h) {
                        const int sx = x + off.first, sy = y + off.second;
                        const std::optional<float> z = terrain != nullptr
                                                           ? (*terrain)(sx, sy)
                                                           : terrain_at(*probe, rect, sx, sy);
                        return !z || *z <= static_cast<float>(h) + kRiverTolerance;
                    };
                    stats.objects.add(
                        draw_prepared_tested(local, objects.above_list(layers.water_style), anlagen,
                                             group, lift, t, sprites, water_variant, keep));
                }
                if (layers.objects) {
                    const auto [s, units_drawn] =
                        draw_island_objects(local, p, objects.other, group);
                    stats.objects.add(s);
                    stats.units += units_drawn;
                }
            }
            composite(canvas, local, p.offset);
        }
        if (pass == 1 && layers.objects && layers.ships) {
            stats.units += draw_units(canvas, group);
        }
        if (pass == 0 && layers.sea) {
            if (const sea::SeaPattern* pattern = sea(group)) {
                const auto field_probe = ground_with_probe(rect, group).second;
                const sea::WaterField field = water_field_from(rect, field_probe, group);
                // Enhanced rivers: the flow lattice of the region, built here
                // (blocking: this is the headless path).
                const bool flowing = layers.water_style == WaterStyle::Enhanced &&
                                     layers.river_flow == RiverFlowMode::Flow && river_active();
                const river::FlowPlane plane =
                    flowing ? flow_plane(rect, group, field_probe, rect, true, nullptr)
                            : river::FlowPlane{};
                const RiverParams params = river_cfg.params();
                const sea::RiverShading shading{&params, river_cfg.min_valid};
                const auto rw = static_cast<std::size_t>(rect.w);
                sea::shade_sea_flow(
                    sea::RgbaTarget{canvas.pixels.data()}, canvas.width, rect,
                    [&field](std::size_t i) {
                        return sea::FieldPx{field.cover[i], field.shore[i], field.depth[i],
                                            field.flags[i]};
                    },
                    [&plane, rect, rw](std::size_t i) {
                        return river::flow_at(plane, rect.x + static_cast<int>(i % rw),
                                              rect.y + static_cast<int>(i / rw));
                    },
                    flowing && !plane.empty() ? &shading : nullptr, *pattern, t, layers.water_style,
                    group, lift);
            }
        }
    }
    stats.islands = drawn;
    stats.no_ground = no_ground;
    return {std::move(canvas), stats};
}

std::optional<float> Scene::ground_height(float x, float y) const {
    for (std::size_t k = 0; k < islands.size(); ++k) {
        const auto& i = islands[k];
        const float lx = x - static_cast<float>(i.x), ly = y - static_cast<float>(i.y);
        const bool inside = lx >= 0.0F && lx < static_cast<float>(i.width) && ly >= 0.0F &&
                            ly < static_cast<float>(i.height);
        const auto& g = objects_[k].grids;
        if (inside && g) {
            return terrain_height(g->second, i.width, i.height, lx, ly);
        }
    }
    return std::nullopt;
}

std::pair<DrawStats, std::size_t> Scene::draw_island_objects(Canvas& local, const Placement& p,
                                                             std::span<const Prepared> list,
                                                             std::size_t group) const {
    const auto& island = islands[p.index];
    const float ix = static_cast<float>(island.x), iy = static_cast<float>(island.y);
    const float w = static_cast<float>(island.width), h = static_cast<float>(island.height);
    using Key = std::pair<std::uint32_t, std::uint32_t>;
    std::vector<std::pair<Key, const UnitSprite*>> on;
    for (const auto& u : units) {
        if (!u.z || !(u.x >= ix && u.x < ix + w) || !(u.y >= iy && u.y < iy + h)) {
            continue;
        }
        const auto tx = sat_cast<std::uint32_t>(u.x - ix), ty = sat_cast<std::uint32_t>(u.y - iy);
        on.emplace_back(Key{tx + 1 + ty + 1, tx}, &u);
    }
    std::stable_sort(on.begin(), on.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    auto key = [](const Prepared& o) {
        return Key{std::uint32_t{o.first.x} + o.second.first + o.first.y + o.second.second,
                   o.first.x};
    };
    DrawStats stats;
    std::size_t start = 0, drawn = 0;
    for (const auto& [k, u] : on) {
        const auto it =
            std::partition_point(list.begin() + static_cast<std::ptrdiff_t>(start), list.end(),
                                 [&](const Prepared& o) { return key(o) <= k; });
        const auto end = static_cast<std::size_t>(it - list.begin());
        stats.add(draw_prepared(local, list.subspan(start, end - start), anlagen, group, lift, 0,
                                sprites));
        start = end;
        if (u->draw(local, group, lift, p.offset)) {
            ++drawn;
        }
    }
    stats.add(draw_prepared(local, list.subspan(start), anlagen, group, lift, 0, sprites));
    return {stats, drawn};
}

ChunkDraws Scene::objects_draws(Rect rect, std::size_t group, bool ships, bool bases,
                                std::uint32_t bin_px) const {
    assert(group < kGroups);
    // render_with_terrain's setup for Layers::objects: the base band of the
    // chunk canvas and of every island's local canvas.
    ChunkDrawsBuilder out(rect, group, bases, bases ? static_cast<int>(kTile[group].h) * 6 / 10 : 0,
                          bin_px);
    const std::uint32_t track = bases ? object_flags::kTrackBase : 0U;
    // Canvas::overlaps of a canvas covering `c`.
    auto overlaps = [](const Rect& c, int x, int y, int w, int h) {
        return x < c.x + c.w && x + w > c.x && y < c.y + c.h && y + h > c.y;
    };
    // The sprites of `list` (placed as draw_prepared does: time 0, kept when
    // the island's local canvas `local` can see them) as records.
    auto emit_list = [&](std::span<const Prepared> list, std::pair<int, int> offset,
                         const Rect& local) {
        std::vector<SpriteDraw> draws;
        place_prepared(
            list, anlagen, group, lift, 0,
            [&](int x, int y, int w, int h) { return overlaps(local, x, y, w, h); }, draws);
        for (const SpriteDraw& d : draws) {
            // A frame that cannot be decoded is left out, as draw_prepared does.
            const auto id = frame_table.intern_building(anlagen, sprites, d.key, *d.frame);
            if (!id) {
                continue;
            }
            const bool shadow = d.is_shadow();
            out.add(ObjectRecord{d.anchor.first + offset.first - d.frame->anchor_x,
                                 d.anchor.second + offset.second - d.frame->anchor_y, *id,
                                 (shadow ? object_flags::kShadow : track) |
                                     std::uint32_t{d.height} << object_flags::kHeightShift,
                                 0, 0, 0});
        }
    };
    // UnitSprite::draw: the shadow, then the frame, in scene coordinates;
    // `canvas` is the canvas blit_sprite would draw into.
    auto emit_unit = [&](const UnitSprite& u, const Rect& canvas) {
        if (group >= u.frames.size() || !u.frames[group] || !u.frames[group]->image) {
            return;
        }
        const auto a = u.anchor(group, lift);
        auto add = [&](const UnitFrame& f, bool shadow) {
            const int x = a.first - f.frame.anchor_x, y = a.second - f.frame.anchor_y;
            if (!overlaps(canvas, x, y, f.image->width, f.image->height)) {
                return;
            }
            const auto id = frame_table.intern(
                f.id.known ? FrameKey::unit(f.id, group, shadow)
                           : FrameKey::unit_image(f.image.get(), group, shadow),
                [&] { return std::pair{f.frame, f.image}; });
            if (id) {
                out.add(ObjectRecord{x, y, *id, shadow ? object_flags::kShadow : track, 0, 0, 0});
            }
        };
        if (group < u.shadows.size() && u.shadows[group] && u.shadows[group]->image) {
            add(*u.shadows[group], true);
        }
        add(*u.frames[group], false);
    };
    for (const auto& p : layouts[group]) {
        const auto r = p.bounds.intersect(rect);
        if (!r) {
            continue;
        }
        out.begin_island(*r);
        const Rect local{r->x - p.offset.first, r->y - p.offset.second, r->w, r->h};
        // draw_island_objects: the island's soldiers are merged into its
        // objects by the key of their 1 x 1 footprint.
        const auto& island = islands[p.index];
        const std::span<const Prepared> list = objects_[p.index].other;
        const float ix = static_cast<float>(island.x), iy = static_cast<float>(island.y);
        const float w = static_cast<float>(island.width), h = static_cast<float>(island.height);
        using Key = std::pair<std::uint32_t, std::uint32_t>;
        std::vector<std::pair<Key, const UnitSprite*>> on;
        for (const auto& u : units) {
            if (!u.z || !(u.x >= ix && u.x < ix + w) || !(u.y >= iy && u.y < iy + h)) {
                continue;
            }
            const auto tx = sat_cast<std::uint32_t>(u.x - ix),
                       ty = sat_cast<std::uint32_t>(u.y - iy);
            on.emplace_back(Key{tx + 1 + ty + 1, tx}, &u);
        }
        std::stable_sort(on.begin(), on.end(),
                         [](const auto& a, const auto& b) { return a.first < b.first; });
        auto key = [](const Prepared& o) {
            return Key{std::uint32_t{o.first.x} + o.second.first + o.first.y + o.second.second,
                       o.first.x};
        };
        std::size_t start = 0;
        for (const auto& [k, u] : on) {
            const auto it =
                std::partition_point(list.begin() + static_cast<std::ptrdiff_t>(start), list.end(),
                                     [&](const Prepared& o) { return key(o) <= k; });
            const auto end = static_cast<std::size_t>(it - list.begin());
            emit_list(list.subspan(start, end - start), p.offset, local);
            start = end;
            emit_unit(*u, *r);
        }
        emit_list(list.subspan(start), p.offset, local);
    }
    if (ships) {
        // draw_units: after all islands, straight onto the chunk canvas.
        out.begin_unclipped();
        std::vector<const UnitSprite*> order;
        for (const auto& u : units) {
            if (!u.z) {
                order.push_back(&u);
            }
        }
        std::stable_sort(order.begin(), order.end(), [](const UnitSprite* a, const UnitSprite* b) {
            return total_less(a->x + a->y, b->x + b->y);
        });
        for (const UnitSprite* u : order) {
            emit_unit(*u, rect);
        }
    }
    return out.finish(frame_table);
}

std::optional<Rect> Scene::unit_rect(const UnitSprite& u, std::size_t group) const {
    if (group >= u.frames.size() || !u.frames[group]) {
        return std::nullopt;
    }
    const auto& f = u.frames[group]->frame;
    const auto a = u.anchor(group, lift);
    return Rect{a.first - f.anchor_x, a.second - f.anchor_y, f.width, f.height};
}

bool Scene::units_touch(Rect rect, std::size_t group, bool ships) const {
    return std::any_of(units.begin(), units.end(), [&](const UnitSprite& u) {
        if (!ships && !u.z) {
            return false;
        }
        const auto r = unit_rect(u, group);
        return r && r->intersect(rect);
    });
}

std::optional<Rect> Scene::bounds(std::size_t group) const {
    std::optional<Rect> r = scene_bounds(layouts[group]);
    for (const auto& u : units) {
        if (const auto ur = unit_rect(u, group)) {
            r = r ? r->unite(*ur) : *ur;
        }
    }
    return r;
}

std::size_t Scene::draw_units(Canvas& canvas, std::size_t group) const {
    std::vector<const UnitSprite*> order;
    for (const auto& u : units) {
        if (!u.z) {
            order.push_back(&u);
        }
    }
    std::stable_sort(order.begin(), order.end(), [](const UnitSprite* a, const UnitSprite* b) {
        return total_less(a->x + a->y, b->x + b->y);
    });
    std::size_t n = 0;
    for (const UnitSprite* u : order) {
        if (u->draw(canvas, group, lift, {0, 0})) {
            ++n;
        }
    }
    return n;
}

Scene::~Scene() {
    if (flow_build_) {
        {
            const std::lock_guard lock(flow_build_->mutex);
            flow_build_->stop = true;
        }
        flow_build_->wake.notify_all();
        for (auto& t : flow_build_->threads) {
            t.join();
        }
    }
}

void Scene::build_flow(std::size_t index) const {
    FlowSlot& slot = *flow_slots_[index];
    std::call_once(slot.once, [&] {
        const profile::Scope stage(profile::Stage::flow_solve);
        try {
            // Phase A (the raster) and phase B (the solve); the raster is only
            // needed for the solve and freed here.
            if (flow_build_fault) {
                flow_build_fault(index);
            }
            const river::RiverMask mask = river_mask(index, river_cfg);
            slot.flow =
                std::make_shared<const river::RiverFlow>(river::RiverFlow::build(mask, river_cfg));
            slot.state.store(kFlowReady, std::memory_order_release);
        } catch (const std::exception& e) {
            std::fprintf(stderr, "river flow of island %zu failed: %s\n", index, e.what());
            slot.state.store(kFlowFailed, std::memory_order_release);
        }
    });
}

void Scene::start_flow_builders() const {
    flow_build_ = std::make_unique<FlowBuild>();
    // Threads: RiverConfig::flow_threads, else a quarter of the hardware
    // threads (1-4): a solve is 10-60 ms, so a few threads clear a scene's
    // islands in the first second without taking cores from the chunk
    // builders and the window thread.
    const unsigned hw = std::thread::hardware_concurrency();
    const unsigned n =
        river_cfg.flow_threads != 0 ? river_cfg.flow_threads : std::clamp(hw / 4, 1U, 4U);
    FlowBuild* fb = flow_build_.get();
    for (unsigned i = 0; i < n; ++i) {
        fb->threads.emplace_back([this, fb] {
            lower_thread_priority();
            for (;;) {
                std::size_t index = 0;
                {
                    std::unique_lock lock(fb->mutex);
                    fb->wake.wait(lock,
                                  [fb] { return fb->stop || (!fb->paused && !fb->queue.empty()); });
                    if (fb->stop) {
                        return;
                    }
                    index = fb->queue.front();
                    fb->queue.pop_front();
                }
                build_flow(index);
            }
        });
    }
}

void Scene::queue_flow(std::size_t index) const {
    std::call_once(flow_build_once_, [this] { start_flow_builders(); });
    {
        const std::lock_guard lock(flow_build_->mutex);
        flow_build_->queue.push_back(index);
    }
    flow_build_->wake.notify_one();
}

bool Scene::river_active() const {
    if (!river_cfg.enabled) {
        return false;
    }
    // The configuration is checked once, at its first use: a refused one
    // leaves the enhanced rivers on the legacy look (a valid stride, a bounded
    // extension grid and periods that divide the hour are what the shading
    // and the lattice rely on).
    std::call_once(river_cfg_once_, [this] {
        const std::string problem = river_cfg.validate(kChunk, kGroups);
        if (!problem.empty()) {
            std::fprintf(stderr, "river configuration refused, rivers keep the legacy look: %s\n",
                         problem.c_str());
            river_cfg_ok_ = false;
        }
    });
    return river_cfg_ok_;
}

Scene::RiverFlowResult Scene::river_flow(std::size_t index, bool wait) const {
    if (!river_active() || !has_river(index)) {
        return {};
    }
    FlowSlot& slot = *flow_slots_[index];
    auto ready = [&]() -> std::optional<RiverFlowResult> {
        switch (slot.state.load(std::memory_order_acquire)) {
        case kFlowReady:
            return RiverFlowResult{slot.flow, false};
        case kFlowFailed:
            return RiverFlowResult{};
        default:
            return std::nullopt;
        }
    };
    if (const auto r = ready()) {
        return *r;
    }
    if (wait) {
        build_flow(index);
        return ready().value_or(RiverFlowResult{});
    }
    std::uint8_t expected = kFlowIdle;
    if (slot.state.compare_exchange_strong(expected, kFlowQueued, std::memory_order_acq_rel)) {
        queue_flow(index);
    }
    if (const auto r = ready()) {
        return *r;
    }
    return {nullptr, true};
}

void Scene::prefetch_river_flow() const {
    for (std::size_t i = 0; i < islands.size(); ++i) {
        if (river_active() && has_river(i)) {
            static_cast<void>(river_flow(i, false));
        }
    }
}

void Scene::pause_river_flow_builders(bool pause) const {
    // The builders (and their queue) exist from the first request on.
    std::call_once(flow_build_once_, [this] { start_flow_builders(); });
    {
        const std::lock_guard lock(flow_build_->mutex);
        flow_build_->paused = pause;
    }
    flow_build_->wake.notify_all();
}

bool Scene::river_flow_done(std::size_t index) const {
    if (!river_active() || !has_river(index)) {
        return true;
    }
    return flow_slots_[index]->state.load(std::memory_order_acquire) >= kFlowReady;
}

river::FlowPlane Scene::flow_plane(Rect area, std::size_t group, const sea::SceneProbe& probe,
                                   Rect probe_rect, bool wait,
                                   std::vector<std::uint32_t>* waiting) const {
    river::FlowPlane plane;
    if (!river_active() || area.w <= 0 || area.h <= 0 || group >= river_cfg.lattice_stride.size() ||
        river_cfg.lattice_stride[group] == 0) {
        return plane;
    }
    const std::uint32_t stride = river_cfg.lattice_stride[group];
    const int s = static_cast<int>(stride);
    const int i0 = div_euclid(area.x, s), i1 = div_euclid(area.x + area.w - 1, s);
    const int j0 = div_euclid(area.y, s), j1 = div_euclid(area.y + area.h - 1, s);
    // A pixel reads the texels floor(p / s) and the next one.
    plane.x0 = i0 * s;
    plane.y0 = j0 * s;
    plane.stride_px = stride;
    plane.nx = static_cast<std::uint32_t>(i1 - i0 + 2);
    plane.ny = static_cast<std::uint32_t>(j1 - j0 + 2);
    plane.tile_w = kTile[group].w;
    plane.tile_h = kTile[group].h;
    const std::size_t count = std::size_t{plane.nx} * plane.ny;
    plane.uv.assign(count * 2, 0);
    plane.aux.assign(count * 4, 0);
    plane.shear.assign(count, 0);

    // The pixels of all texels: [x0, x0 + (nx - 1) s] x [y0, y0 + (ny - 1) s].
    const Rect need{plane.x0, plane.y0, static_cast<int>(plane.nx - 1) * s + 1,
                    static_cast<int>(plane.ny - 1) * s + 1};
    // What the given probe does not cover: thin strips (the apron), probed on
    // their own. Disjoint: top and bottom across the whole width, left and
    // right beside the probe.
    struct Part {
        Rect rect;
        sea::SceneProbe probe;
    };
    std::vector<Part> extra;
    auto add_strip = [&](Rect r) {
        if (const auto c = r.intersect(need)) {
            const profile::Scope stage(profile::Stage::apron);
            extra.push_back({*c, ground_with_probe(*c, group).second});
        }
    };
    const int need_r = need.x + need.w, need_b = need.y + need.h;
    const int probe_r = probe_rect.x + probe_rect.w, probe_b = probe_rect.y + probe_rect.h;
    if (probe_rect.y > need.y) {
        add_strip({need.x, need.y, need.w, probe_rect.y - need.y});
    }
    if (probe_b < need_b) {
        add_strip({need.x, probe_b, need.w, need_b - probe_b});
    }
    const int mid_y0 = std::max(need.y, probe_rect.y), mid_y1 = std::min(need_b, probe_b);
    if (mid_y1 > mid_y0) {
        if (probe_rect.x > need.x) {
            add_strip({need.x, mid_y0, probe_rect.x - need.x, mid_y1 - mid_y0});
        }
        if (probe_r < need_r) {
            add_strip({probe_r, mid_y0, need_r - probe_r, mid_y1 - mid_y0});
        }
    }

    std::vector<std::shared_ptr<const river::RiverFlow>> flows(islands.size());
    std::vector<std::uint8_t> asked(islands.size(), 0);
    bool any = false;
    for (std::uint32_t j = 0; j < plane.ny; ++j) {
        for (std::uint32_t i = 0; i < plane.nx; ++i) {
            const int x = plane.x0 + static_cast<int>(i) * s,
                      y = plane.y0 + static_cast<int>(j) * s;
            std::uint32_t island = sea::kNoIsland;
            std::array<float, 2> pos{0.0F, 0.0F};
            auto from = [&](const sea::SceneProbe& p, Rect r) {
                const auto k = static_cast<std::size_t>(y - r.y) * static_cast<std::size_t>(r.w) +
                               static_cast<std::size_t>(x - r.x);
                island = p.island[k];
                pos = p.pos[k];
            };
            if (x >= probe_rect.x && x < probe_r && y >= probe_rect.y && y < probe_b) {
                from(probe, probe_rect);
            } else {
                for (const Part& part : extra) {
                    if (x >= part.rect.x && x < part.rect.x + part.rect.w && y >= part.rect.y &&
                        y < part.rect.y + part.rect.h) {
                        from(part.probe, part.rect);
                        break;
                    }
                }
            }
            if (island == sea::kNoIsland || island >= islands.size() || !has_river(island)) {
                continue;
            }
            if (asked[island] == 0) {
                asked[island] = 1;
                const RiverFlowResult r = river_flow(island, wait);
                flows[island] = r.flow;
                if (r.pending && waiting != nullptr) {
                    waiting->push_back(island);
                }
            }
            const auto& flow = flows[island];
            if (!flow) {
                continue;
            }
            const river::RiverFlow::Sample f = flow->sample_extended(pos[0], pos[1]);
            if (f.valid <= 0.0F) {
                continue;
            }
            // The shear |grad u|: the flow's forward differences along the tile
            // axes (a pure function of the probe position, so neighbouring
            // chunks agree and the value is smooth over texel borders).
            const float dp = river_cfg.shear_probe_tiles;
            const river::RiverFlow::Sample fx = flow->sample_extended(pos[0] + dp, pos[1]);
            const river::RiverFlow::Sample fy = flow->sample_extended(pos[0], pos[1] + dp);
            float shear2 = 0.0F;
            for (const river::RiverFlow::Sample* g : {&fx, &fy}) {
                if (g->valid > 0.0F) {
                    const float du = (g->u[0] - f.u[0]) / dp, dv = (g->u[1] - f.u[1]) / dp;
                    shear2 += du * du + dv * dv;
                }
            }
            river::set_texel(plane, i, j, f.u[0], f.u[1], f.valid, 0.0F, 0.0F, f.level,
                             river_cfg.velocity_max, std::sqrt(shear2));
            any = true;
        }
    }
    if (!any) {
        return {};
    }
    return plane;
}

} // namespace anno::render
