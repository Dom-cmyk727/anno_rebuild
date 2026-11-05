#include <algorithm>
#include <cmath>

#include <anno/app/loader.hpp>
#include <anno/app/ship_layer.hpp>
#include <anno/assets/figure.hpp>
#include <anno/render/iso.hpp>

namespace anno::app {

namespace {

constexpr std::uint32_t kRingFigure = 0xFFFFU; // no FigurenHeader entry has this id (u16 max)

} // namespace

/// The sprite frame of a ship's facing (sim::Figure::dir16: 0 is -x, 4 is +y, 8 is +x, 12 is -y).
/// MEASURED 2026-11-03 (owner playtest, frames of one ship captured sailing the eight directions):
/// frame k shows its bow towards screen compass 45 + 22.5 k degrees (frame 0 north-east, 4
/// south-east, 8 south-west, 12 north-west). In the original's handedness (docs/render/iso.md,
/// "Handedness": tile x runs left-down, y right-down) those are the directions -x, +y, +x, -y,
/// i.e. dir16 0, 4, 8, 12, both turning clockwise on the screen: frame = dir16, the frame the
/// original shows for a scenario ship's facing `attr` too (docs/render/assets.md). Before the
/// handedness fix the same measurement, taken in the mirrored picture, gave (12 - d) mod 16. A set
/// that does not have 16 frames keeps the plain index.
std::size_t ShipLayer::facing_frame(std::uint8_t dir16, std::size_t count) {
    return static_cast<std::size_t>(dir16) % count;
}

namespace {

/// A ring (an ellipse outline of the tile diamond's proportions) as a unit frame of zoom `group`.
/// Anchor at the ellipse's centre, which is where a ship's anchor (its waterline) stands.
render::UnitFrame make_ring(std::size_t group, std::uint32_t rgb) {
    const auto [tw, th] = render::kTile[group];
    const int rx = std::max(4, static_cast<int>(tw * 3 / 4));
    const int ry = std::max(2, static_cast<int>(th * 3 / 4));
    const int thick = std::max(1, static_cast<int>(tw / 40));
    const int w = 2 * rx + 3, h = 2 * ry + 3;
    auto image = std::make_shared<render::Rgba>();
    image->width = static_cast<std::uint16_t>(w);
    image->height = static_cast<std::uint16_t>(h);
    image->pixels.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double dx = (x - (rx + 1)) / static_cast<double>(rx);
            const double dy = (y - (ry + 1)) / static_cast<double>(ry);
            const double r = std::sqrt(dx * dx + dy * dy);
            const double inner = 1.0 - static_cast<double>(thick) / rx;
            if (r <= 1.0 && r >= inner) {
                std::uint8_t* p =
                    &image->pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                                    static_cast<std::size_t>(x)) *
                                   4];
                p[0] = static_cast<std::uint8_t>(rgb >> 16);
                p[1] = static_cast<std::uint8_t>(rgb >> 8);
                p[2] = static_cast<std::uint8_t>(rgb);
                p[3] = 255;
            }
        }
    }
    formats::figuren::Frame frame;
    frame.width = image->width;
    frame.height = image->height;
    frame.anchor_x = static_cast<std::int16_t>(rx + 1);
    frame.anchor_y = static_cast<std::int16_t>(ry + 1);
    return render::UnitFrame{
        frame, image,
        render::UnitFrameId{true, kRingFigure, static_cast<std::uint32_t>(group), rgb}};
}

} // namespace

void ShipLayer::make_rings() {
    rings_.clear();
    for (const std::uint32_t rgb : {0xFFE040U, 0x40C0FFU}) { // own: yellow; foreign: blue
        render::UnitSprite ring;
        for (std::size_t g = 0; g < render::kGroups; ++g) {
            ring.frames.push_back(make_ring(g, rgb));
            ring.shadows.emplace_back();
        }
        rings_.push_back(std::move(ring));
    }
}

formats::Result<std::shared_ptr<const ShipLayer>, std::string>
ShipLayer::load(const std::filesystem::path& game_dir, const std::vector<Wanted>& wanted) {
    auto layer = std::make_shared<ShipLayer>();
    const auto& palette = assets::figure::kOriginalPlayerColours;
    for (const Wanted& w : wanted) {
        if (layer->sets_.contains(w)) {
            continue;
        }
        if (w.first > 0xFFFEU) {
            return "figure kind " + std::to_string(w.first) + " is not a FigurenHeader entry";
        }
        auto sprites = load_figure_facings(game_dir, static_cast<std::uint16_t>(w.first), 16,
                                           palette[w.second % palette.size()]);
        if (!sprites.ok()) {
            return "ship sprites for kind " + std::to_string(w.first) + ": " + sprites.error();
        }
        std::vector<render::UnitSprite> facings;
        for (const assets::unit::UnitFrames& f : sprites.value().facings) {
            render::UnitSprite s;
            for (const auto& d : f.frames) {
                s.frames.push_back(d ? std::optional<render::UnitFrame>(render::to_unit_frame(
                                           *d, static_cast<std::uint16_t>(w.first), false))
                                     : std::nullopt);
            }
            for (const auto& d : f.shadows) {
                s.shadows.push_back(d ? std::optional<render::UnitFrame>(render::to_unit_frame(
                                            *d, static_cast<std::uint16_t>(w.first), true))
                                      : std::nullopt);
            }
            facings.push_back(std::move(s));
        }
        layer->sets_.emplace(w, std::move(facings));
    }
    layer->make_rings();
    return std::shared_ptr<const ShipLayer>(std::move(layer));
}

std::shared_ptr<const ShipLayer> ShipLayer::from_prepared(std::vector<Prepared> sets) {
    auto layer = std::make_shared<ShipLayer>();
    for (Prepared& p : sets) {
        layer->sets_.emplace(Wanted{p.kind, p.owner}, std::move(p.facings));
    }
    layer->make_rings();
    return layer;
}

std::vector<render::UnitSprite> ShipLayer::units(const OpeningView& view,
                                                 std::optional<sim::FigureId> selected,
                                                 bool selected_is_own, std::size_t* missing) const {
    std::vector<render::UnitSprite> out;
    out.reserve(view.ships.size() + 1);
    for (const ShipView& ship : view.ships) {
        const auto it = sets_.find(Wanted{ship.kind, ship.owner.v});
        if (it == sets_.end() || it->second.empty()) {
            if (missing != nullptr) {
                ++*missing;
            }
            continue;
        }
        const float x = static_cast<float>(ship.pos.x) / 256.0F;
        const float y = static_cast<float>(ship.pos.y) / 256.0F;
        if (selected && *selected == ship.id) {
            render::UnitSprite ring = rings_[selected_is_own ? 0 : 1];
            ring.x = x;
            ring.y = y;
            out.push_back(std::move(ring));
        }
        render::UnitSprite sprite = it->second[facing_frame(ship.dir16, it->second.size())];
        sprite.x = x;
        sprite.y = y;
        out.push_back(std::move(sprite));
    }
    return out;
}

std::optional<ScreenBox> ShipLayer::box(const ShipView& ship, const PointerFrame& frame) const {
    if (!frame.scene) {
        return std::nullopt;
    }
    const auto it = sets_.find(Wanted{ship.kind, ship.owner.v});
    if (it == sets_.end() || it->second.empty()) {
        return std::nullopt;
    }
    const render::UnitSprite& proto = it->second[facing_frame(ship.dir16, it->second.size())];
    const render::Scene& scene = *frame.scene;
    // A derived zoom level draws the ship of its built ancestor reduced by 2^shift.
    const render::ZoomLevels& levels = scene.levels();
    std::size_t base = frame.group;
    unsigned shift = 0;
    if (levels.size() > frame.group && frame.group >= levels.built_count()) {
        base = levels.ancestor(frame.group);
        shift = levels[frame.group].shift;
    }
    if (base >= proto.frames.size() || !proto.frames[base]) {
        return std::nullopt;
    }
    render::UnitSprite at = proto;
    at.x = static_cast<float>(ship.pos.x) / 256.0F;
    at.y = static_cast<float>(ship.pos.y) / 256.0F;
    const auto a = at.anchor(base, scene.lift);
    const render::UnitFrame& f = *proto.frames[base];
    const double r = 1.0 / static_cast<double>(1U << shift);
    const double left = (a.first - f.frame.anchor_x) * r, top = (a.second - f.frame.anchor_y) * r;
    const double w = f.frame.width * r, h = f.frame.height * r;
    const auto to_px = [&](double sx, double sy) {
        return std::pair{(sx - frame.map.x0) * frame.map.scale,
                         (sy - frame.map.y0) * frame.map.scale};
    };
    const auto [x0, y0] = to_px(left, top);
    const auto [x1, y1] = to_px(left + w, top + h);
    ScreenBox box;
    box.x0 = x0;
    box.y0 = y0;
    box.x1 = x1;
    box.y1 = y1;
    // The middle of the frame is the reference for ties between overlapping ships.
    box.cx = (x0 + x1) / 2.0;
    box.cy = (y0 + y1) / 2.0;
    return box;
}

void blend_units(std::span<const render::UnitSprite> units, const render::Scene& scene,
                 std::size_t group, render::Rect view, std::uint32_t* xrgb) {
    if (xrgb == nullptr || group >= render::kGroups) {
        return;
    }
    std::vector<const render::UnitSprite*> order;
    for (const render::UnitSprite& u : units) {
        if (!u.z && group < u.frames.size() && u.frames[group] && u.frames[group]->image) {
            order.push_back(&u);
        }
    }
    std::stable_sort(order.begin(), order.end(),
                     [](const render::UnitSprite* a, const render::UnitSprite* b) {
                         return render::total_less(a->x + a->y, b->x + b->y);
                     });
    for (const render::UnitSprite* u : order) {
        const auto a = u->anchor(group, scene.lift);
        // The rectangle of the frame and of its shadow, in scene pixels.
        int x0 = a.first - u->frames[group]->frame.anchor_x;
        int y0 = a.second - u->frames[group]->frame.anchor_y;
        int x1 = x0 + u->frames[group]->image->width, y1 = y0 + u->frames[group]->image->height;
        if (group < u->shadows.size() && u->shadows[group] && u->shadows[group]->image) {
            const render::UnitFrame& s = *u->shadows[group];
            x0 = std::min(x0, a.first - s.frame.anchor_x);
            y0 = std::min(y0, a.second - s.frame.anchor_y);
            x1 = std::max(x1, a.first - s.frame.anchor_x + s.image->width);
            y1 = std::max(y1, a.second - s.frame.anchor_y + s.image->height);
        }
        const auto clipped = render::Rect{x0, y0, x1 - x0, y1 - y0}.intersect(view);
        if (!clipped) {
            continue;
        }
        render::Canvas local;
        local.width = static_cast<std::size_t>(clipped->w);
        local.height = static_cast<std::size_t>(clipped->h);
        local.origin = {clipped->x, clipped->y};
        local.pixels.assign(local.width * local.height * 4, 0);
        static_cast<void>(u->draw(local, group, scene.lift, {0, 0}));
        for (std::size_t y = 0; y < local.height; ++y) {
            for (std::size_t x = 0; x < local.width; ++x) {
                const std::uint8_t* s = &local.pixels[(y * local.width + x) * 4];
                if (s[3] == 0) {
                    continue;
                }
                const auto fx = static_cast<std::size_t>(clipped->x - view.x) + x;
                const auto fy = static_cast<std::size_t>(clipped->y - view.y) + y;
                std::uint32_t& d = xrgb[fy * static_cast<std::size_t>(view.w) + fx];
                const std::uint32_t al = s[3];
                const auto mix = [al](std::uint32_t src, std::uint32_t dst) {
                    return (src * al + dst * (255U - al) + 127U) / 255U;
                };
                const std::uint32_t r = mix(s[0], (d >> 16) & 0xFFU);
                const std::uint32_t g = mix(s[1], (d >> 8) & 0xFFU);
                const std::uint32_t b = mix(s[2], d & 0xFFU);
                d = (r << 16) | (g << 8) | b;
            }
        }
    }
}

} // namespace anno::app
