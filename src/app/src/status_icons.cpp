#include <algorithm>
#include <climits>
#include <type_traits>
#include <utility>
#include <variant>

#include <anno/app/loader.hpp>
#include <anno/app/status_icons.hpp>
#include <anno/sim/tick.hpp>

namespace anno::app {

namespace {

/// Whether a payload has production switches, and whether every one of them is off. The switch
/// helper 0x10072a60 receives the OR of a two-line building's lines (0x100690b0 / 0x1006910c), so
/// the Zzz shows only when no line runs (docs/re/ui.md §11.2).
std::optional<bool> all_switched_off(const sim::Payload& payload) {
    return std::visit(
        [](const auto& p) -> std::optional<bool> {
            using P = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<P, sim::Workshop> || std::is_same_v<P, sim::Farm> ||
                          std::is_same_v<P, sim::Mine>) {
                return !p.switched_on;
            } else if constexpr (std::is_same_v<P, sim::WeaponsWorkshop> ||
                                 std::is_same_v<P, sim::SmallFarm>) {
                if (p.line_on.empty()) {
                    return std::nullopt;
                }
                return std::none_of(p.line_on.begin(), p.line_on.end(), [](bool on) { return on; });
            } else {
                return std::nullopt;
            }
        },
        payload);
}

/// The StatusTracker of a production payload, null for the classes without icons.
const sim::StatusTracker* tracker_of(const sim::Payload& payload) {
    return std::visit(
        [](const auto& p) -> const sim::StatusTracker* {
            using P = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<P, sim::Workshop> || std::is_same_v<P, sim::Farm> ||
                          std::is_same_v<P, sim::Mine> || std::is_same_v<P, sim::WeaponsWorkshop> ||
                          std::is_same_v<P, sim::SmallFarm>) {
                return &p.status;
            } else {
                return nullptr;
            }
        },
        payload);
}

} // namespace

std::optional<StatusIcon> status_icon_of(const sim::Payload& payload, bool icons_allowed) {
    if (!icons_allowed) {
        return std::nullopt; // the owner gate 0x1007302f: no bit is ever stored
    }
    if (all_switched_off(payload).value_or(false)) {
        return StatusIcon::Off;
    }
    const sim::StatusTracker* status = tracker_of(payload);
    if (status == nullptr) {
        return std::nullopt;
    }
    for (const StatusIcon code :
         {StatusIcon::MissingInput, StatusIcon::StorageFull, StatusIcon::NoRoad}) {
        if ((status->icons & (1U << static_cast<unsigned>(code))) != 0) {
            return code;
        }
    }
    return std::nullopt;
}

std::vector<StatusIconView> collect_status_icons(const sim::World& world, sim::PlayerId viewer) {
    std::vector<StatusIconView> out;
    const sim::PlayersView players(world.players());
    const auto& islands = world.islands();
    for (const auto& island : islands) {
        if (!island) {
            continue;
        }
        for (const std::optional<sim::City>& city : island->cities) {
            if (!city || city->owner != viewer) {
                continue;
            }
            // StatusTracker::shows_icons is derived the same way before every step (driver.cpp).
            const bool allowed = players.volk(city->owner) == 1;
            for (const sim::BuildingList& list : city->index.lists) {
                for (const std::optional<sim::Building>& b : list.slots) {
                    if (!b) {
                        continue;
                    }
                    const auto icon = status_icon_of(b->payload, allowed);
                    if (!icon) {
                        continue;
                    }
                    StatusIconView v;
                    v.id = b->id;
                    v.type = b->type.v;
                    v.x = b->x;
                    v.y = b->y;
                    if (const sim::BuildingType* t = world.types().find(b->type)) {
                        const auto [w, h] = sim::footprint(*t, b->rotation);
                        v.w = w;
                        v.h = h;
                    }
                    v.rotation = b->rotation;
                    v.height = b->byte0f;
                    v.icon = *icon;
                    out.push_back(v);
                }
            }
        }
    }
    return out;
}

std::vector<PlacedStatusIcon> place_status_icons(std::span<const StatusIconView> icons,
                                                 const render::Scene& scene) {
    std::vector<PlacedStatusIcon> out;
    out.reserve(icons.size());
    for (const StatusIconView& v : icons) {
        std::optional<std::size_t> index;
        for (std::size_t k = 0; k < scene.islands.size() && !index; ++k) {
            const auto& i = scene.islands[k];
            if (v.x >= i.x && v.y >= i.y && v.x < i.x + static_cast<int>(i.width) &&
                v.y < i.y + static_cast<int>(i.height)) {
                index = k;
            }
        }
        if (!index) {
            continue;
        }
        const auto& island = scene.islands[*index];
        PlacedStatusIcon p;
        // The footprint centre (D: x + w * 0.5, y + h * 0.5 with 0x100c1288 = 0.5, 0x1007313f).
        p.x = static_cast<float>(v.x) + static_cast<float>(v.w) / 2.0F;
        p.y = static_cast<float>(v.y) + static_cast<float>(v.h) / 2.0F;
        p.icon = v.icon;
        render::UnitSprite probe;
        probe.x = p.x;
        probe.y = p.y;
        for (std::size_t g = 0; g < render::kGroups; ++g) {
            const int sea = probe.anchor(g, scene.lift).second;
            // The ground at the building's height byte (the scene's own lift), then up to the
            // top of the building's picture where the scene has one (OQ-721 default).
            int top = sea + scene.lift.scaled(render::kSeaLevel, 1, g) -
                      scene.lift.scaled(v.height, 1, g);
            if (v.type <= 0xFFFFU) {
                const auto sprites = scene.preview_sprites(
                    *index, static_cast<std::uint16_t>(v.type), v.rotation,
                    static_cast<std::uint16_t>(v.x - island.x),
                    static_cast<std::uint16_t>(v.y - island.y), v.height, g, 0);
                int highest = INT_MAX;
                for (const render::SpriteDraw& s : sprites) {
                    if (!s.is_shadow() && s.frame != nullptr) {
                        highest = std::min(highest, s.rect()[1]);
                    }
                }
                if (highest != INT_MAX) {
                    top = highest;
                }
            }
            p.raise[g] = sea - top;
        }
        out.push_back(p);
    }
    return out;
}

std::size_t status_icon_frame(std::uint64_t time_ms, std::size_t frames) {
    if (frames == 0) {
        return 0;
    }
    return static_cast<std::size_t>((time_ms / kStatusIconFrameMs) % frames);
}

formats::Result<std::shared_ptr<const StatusIconLayer>, std::string>
StatusIconLayer::load(const std::filesystem::path& game_dir) {
    std::map<StatusIcon, std::vector<render::UnitSprite>> sets;
    for (std::size_t code = 0; code < kStatusIconFigures.size(); ++code) {
        const std::uint16_t figure = kStatusIconFigures[code];
        // 16 = the figures' ANIM frame count (P); facing_sprites takes frame k mod n, so a mod
        // with fewer frames still loads.
        auto sprites = load_figure_facings(game_dir, figure, 16, std::nullopt);
        if (!sprites.ok()) {
            return "status icon figure " + std::to_string(figure) + ": " + sprites.error();
        }
        std::vector<render::UnitSprite> frames;
        for (const assets::unit::UnitFrames& f : sprites.value().facings) {
            render::UnitSprite s;
            for (const auto& d : f.frames) {
                s.frames.push_back(
                    d ? std::optional<render::UnitFrame>(render::to_unit_frame(*d, figure, false))
                      : std::nullopt);
            }
            frames.push_back(std::move(s)); // the icons have no SHADOW set (P)
        }
        sets.emplace(static_cast<StatusIcon>(code), std::move(frames));
    }
    return from_prepared(std::move(sets));
}

std::shared_ptr<const StatusIconLayer>
StatusIconLayer::from_prepared(std::map<StatusIcon, std::vector<render::UnitSprite>> sets) {
    auto layer = std::make_shared<StatusIconLayer>();
    layer->sets_ = std::move(sets);
    return layer;
}

std::vector<render::UnitSprite> StatusIconLayer::units(std::span<const PlacedStatusIcon> placed,
                                                       std::uint64_t time_ms) const {
    std::vector<render::UnitSprite> out;
    out.reserve(placed.size());
    for (const PlacedStatusIcon& p : placed) {
        const auto it = sets_.find(p.icon);
        if (it == sets_.end() || it->second.empty()) {
            continue;
        }
        render::UnitSprite sprite = it->second[status_icon_frame(time_ms, it->second.size())];
        sprite.x = p.x;
        sprite.y = p.y;
        for (std::size_t g = 0; g < sprite.frames.size() && g < render::kGroups; ++g) {
            if (sprite.frames[g]) {
                // The frame keeps its image and identity; only this instance's anchor moves up.
                const int ay = sprite.frames[g]->frame.anchor_y + p.raise[g];
                sprite.frames[g]->frame.anchor_y =
                    static_cast<std::int16_t>(std::clamp(ay, int{INT16_MIN}, int{INT16_MAX}));
            }
        }
        out.push_back(std::move(sprite));
    }
    return out;
}

} // namespace anno::app
