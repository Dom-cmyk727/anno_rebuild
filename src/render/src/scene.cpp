#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <numeric>

#include <anno/render/scene.hpp>

namespace anno::render {

namespace insel = formats::insel;

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
        d.below = list(Objects::WaterBelowSea);
        for (const auto& p : d.below) {
            if (p.first.anlage != kSurfAnlage) {
                d.below_enhanced.push_back(p);
            }
        }
        d.above = list(Objects::WaterAboveSea);
        auto is_river = [](const insel::PlacedObject& o) {
            return std::find(kRiverAnlagen.begin(), kRiverAnlagen.end(), o.anlage) !=
                   kRiverAnlagen.end();
        };
        for (const auto& p : d.above) {
            if (is_river(p.first)) {
                d.rivers.emplace_back(p.first.x, p.first.y, p.second.first, p.second.second,
                                      p.first.height());
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
    }
}

const sea::SeaPattern* Scene::sea(std::size_t group) const {
    SeaSlot& s = seas_[group];
    std::call_once(s.once, [&] {
        s.pattern = sea::SeaPattern::build(anlagen, group, lift, textures.water(), sprites);
    });
    return s.pattern ? &*s.pattern : nullptr;
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
    std::call_once(*d.water_once, [&] { d.water = sea::IslandWater::build(islands[i], d.rivers); });
    return d.water ? &*d.water : nullptr;
}

sea::WaterField Scene::water_field(Rect rect, std::size_t group) const {
    const auto probe = ground_with_probe(rect, group).second;
    return water_field_from(rect, probe);
}

sea::WaterField Scene::water_field_from(Rect rect, const sea::SceneProbe& probe) const {
    return sea::water_field(rect, probe, [this](std::size_t i) { return island_water(i); });
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
        if (pass == 1 && layers.objects) {
            stats.units += draw_units(canvas, group);
        }
        if (pass == 0 && layers.sea) {
            if (const sea::SeaPattern* pattern = sea(group)) {
                const sea::WaterField field = water_field(rect, group);
                sea::shade_sea(
                    sea::RgbaTarget{canvas.pixels.data()}, canvas.width, rect,
                    [&field](std::size_t i) {
                        return sea::FieldPx{field.cover[i], field.shore[i], field.depth[i],
                                            field.flags[i]};
                    },
                    *pattern, t, layers.water_style, group, lift);
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

std::optional<Rect> Scene::unit_rect(const UnitSprite& u, std::size_t group) const {
    if (group >= u.frames.size() || !u.frames[group]) {
        return std::nullopt;
    }
    const auto& f = u.frames[group]->frame;
    const auto a = u.anchor(group, lift);
    return Rect{a.first - f.anchor_x, a.second - f.anchor_y, f.width, f.height};
}

bool Scene::units_touch(Rect rect, std::size_t group) const {
    return std::any_of(units.begin(), units.end(), [&](const UnitSprite& u) {
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

} // namespace anno::render
