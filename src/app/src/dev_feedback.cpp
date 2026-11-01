// The developer tool's preview and feedback (see the header).
#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

#include <anno/app/dev_feedback.hpp>
#include <anno/sim/building.hpp>

namespace anno::app {

namespace {

/// The scene index of the island whose world origin is the city island's.
std::optional<std::size_t> city_island(const render::Scene& scene, const TileRect& rect) {
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        if (scene.islands[k].x == rect.x0 && scene.islands[k].y == rect.y0) {
            return k;
        }
    }
    return std::nullopt;
}

} // namespace

std::string preview_note(const DevTool& tool, const render::Scene& scene, std::size_t group) {
    const sim::BuildingType* type = tool.input().selected_type();
    if (!tool.enabled() || tool.input().demolish_mode() || !tool.hover() || type == nullptr) {
        return {};
    }
    const std::string id = std::to_string(type->id.v);
    if (type->id.v >= kPictureAnlageIds) {
        return "no preview: id " + id + " does not fit a PROD2 item, the picture does not draw it";
    }
    if (scene.anlagen.entry(type->id.v) == nullptr) {
        return "no preview: the scene's Anlagen has no entry " + id;
    }
    if (group >= render::kGroups) {
        return "no preview at a derived zoom level";
    }
    if (!city_island(scene, tool.setup().island)) {
        return "no preview: the city's island is not in the scene";
    }
    return {};
}

std::optional<Preview> make_preview(const DevTool& tool, const render::Scene& scene,
                                    std::size_t group, std::uint64_t time_ms) {
    const auto hover = tool.hover();
    const sim::BuildingType* type = tool.input().selected_type();
    if (!tool.enabled() || tool.input().demolish_mode() || !hover || type == nullptr ||
        !preview_note(tool, scene, group).empty()) {
        return std::nullopt;
    }
    const TileRect& rect = tool.setup().island;
    const auto index = city_island(scene, rect);
    if (!index) {
        return std::nullopt;
    }
    const auto lx = static_cast<std::uint16_t>(hover->x - rect.x0);
    const auto ly = static_cast<std::uint16_t>(hover->y - rect.y0);
    const float h = scene.ground_height(static_cast<float>(hover->x), static_cast<float>(hover->y))
                        .value_or(0.0F);
    const auto height = static_cast<std::uint8_t>(std::clamp(std::lround(h), 0L, 255L));
    Preview p;
    p.island = *index;
    p.sprites = scene.preview_sprites(*index, static_cast<std::uint16_t>(type->id.v),
                                      tool.input().rotation(), lx, ly, height, group, time_ms);
    formats::insel::PlacedObject o;
    o.anlage = static_cast<std::uint16_t>(type->id.v);
    o.rotation = tool.input().rotation();
    o.state = static_cast<std::uint16_t>(std::uint16_t{height} << 8);
    o.x = lx;
    o.y = ly;
    const auto fp =
        scene.anlagen.footprint(o.anlage).value_or(std::pair{std::uint8_t{1}, std::uint8_t{1}});
    p.object = render::Prepared{o, o.footprint(fp.first, fp.second)};
    return p;
}

void blend_preview(const render::Scene& scene, const Preview& preview, std::size_t group,
                   std::uint64_t time_ms, render::Rect view, std::uint32_t* xrgb) {
    const auto bounds = scene.object_bounds(preview.island, preview.object, group);
    if (!bounds || xrgb == nullptr) {
        return;
    }
    const auto clipped = bounds->intersect(view);
    if (!clipped) {
        return;
    }
    std::optional<std::pair<int, int>> offset;
    for (const render::Placement& p : scene.layouts[group]) {
        if (p.index == preview.island) {
            offset = p.offset;
        }
    }
    if (!offset) {
        return;
    }
    // The object drawn in its island's local coordinates, on a canvas the size of what is seen.
    render::Canvas local;
    local.width = static_cast<std::size_t>(clipped->w);
    local.height = static_cast<std::size_t>(clipped->h);
    local.origin = {clipped->x - offset->first, clipped->y - offset->second};
    local.pixels.assign(local.width * local.height * 4, 0);
    const std::vector<render::Prepared> list{preview.object};
    static_cast<void>(render::draw_prepared(local, list, scene.anlagen, group, scene.lift, time_ms,
                                            scene.sprites));
    for (std::size_t y = 0; y < local.height; ++y) {
        for (std::size_t x = 0; x < local.width; ++x) {
            const std::uint8_t* s = &local.pixels[(y * local.width + x) * 4];
            if (s[3] == 0) {
                continue;
            }
            const auto fx = static_cast<std::size_t>(clipped->x - view.x) + x;
            const auto fy = static_cast<std::size_t>(clipped->y - view.y) + y;
            std::uint32_t& d = xrgb[fy * static_cast<std::size_t>(view.w) + fx];
            const std::uint32_t a = s[3];
            auto mix = [a](std::uint32_t src, std::uint32_t dst) {
                return (src * a + dst * (255U - a) + 127U) / 255U;
            };
            const std::uint32_t r = mix(s[0], (d >> 16) & 0xFFU);
            const std::uint32_t g = mix(s[1], (d >> 8) & 0xFFU);
            const std::uint32_t b = mix(s[2], d & 0xFFU);
            d = (r << 16) | (g << 8) | b;
        }
    }
}

PlacementKnowledge::PlacementKnowledge(DevToolSetup setup) : setup_(std::move(setup)) {}

bool PlacementKnowledge::update(const std::vector<SceneBuilding>& buildings, std::uint64_t version,
                                const render::Scene& scene) {
    if (version_ && *version_ == version) {
        return false;
    }
    version_ = version;
    ++rebuilds_;
    occupied_.clear();
    indexed_ = false;
    const auto index = city_island(scene, setup_.island);
    if (!index || !setup_.registry) {
        return true;
    }
    indexed_ = true;
    const std::uint32_t number = scene.islands[*index].number;
    for (const SceneBuilding& b : buildings) {
        if (b.island != number) {
            continue;
        }
        const sim::BuildingType* type = setup_.registry->find(sim::TypeId{b.type});
        const auto [w, h] = type != nullptr ? sim::footprint(*type, b.rotation)
                                            : std::pair<std::uint32_t, std::uint32_t>{1, 1};
        for (std::uint32_t dy = 0; dy < h; ++dy) {
            for (std::uint32_t dx = 0; dx < w; ++dx) {
                occupied_.insert({setup_.island.x0 + b.x + static_cast<std::int32_t>(dx),
                                  setup_.island.y0 + b.y + static_cast<std::int32_t>(dy)});
            }
        }
    }
    return true;
}

bool PlacementKnowledge::occupied(sim::TileXY tile) const {
    return occupied_.contains({tile.x, tile.y});
}

PlacementKnowledge::Verdict PlacementKnowledge::check(const DevTool& tool,
                                                      const PlaySnapshot* snapshot) const {
    Verdict v;
    const auto hover = tool.hover();
    if (!tool.enabled() || !hover) {
        return v;
    }
    const std::string turn =
        snapshot != nullptr ? " (published turn " + std::to_string(snapshot->turn) + ")" : "";
    if (tool.input().demolish_mode()) {
        v.text = "demolish: the sim reports what it removed";
        return v;
    }
    const sim::BuildingType* type = tool.input().selected_type();
    if (type == nullptr) {
        const auto id = tool.input().selected();
        v.text = id ? "type " + std::to_string(id->v) + " is not in the registry"
                    : std::string("no buildable type");
        return v;
    }
    for (const sim::TileXY& t : tool.input().footprint_at(*hover)) {
        if (!setup_.island.contains(t)) {
            v.refused = true;
            v.text = "would be refused: the footprint leaves the city's island";
            return v;
        }
        if (occupied(t)) {
            v.refused = true;
            v.text = "would be refused: occupied in the published picture";
            return v;
        }
    }
    // What the window did not check, named (round ee review of C, P3): the sim decides those.
    std::vector<std::string> unchecked{"the placement rule"};
    if (!indexed_) {
        unchecked.emplace_back("occupancy (no published index)");
    }
    if (snapshot == nullptr) {
        unchecked.emplace_back("costs (no snapshot)");
    }
    if (snapshot != nullptr && !snapshot->costs_waived) {
        if (snapshot->money < static_cast<std::int64_t>(type->cost_gold)) {
            v.refused = true;
            v.text = "would be refused: gold " + std::to_string(type->cost_gold) + " needed, " +
                     std::to_string(snapshot->money) + " held" + turn;
            return v;
        }
        std::map<sim::GoodsId, std::uint64_t> need;
        for (const sim::CostEntry& c : type->cost_goods) {
            need[c.goods] += c.amount_88;
        }
        for (const auto& [goods, amount] : need) {
            if (goods >= snapshot->goods.size()) {
                unchecked.push_back("goods " + std::to_string(goods) +
                                    " (outside the published stock)");
                continue;
            }
            if (snapshot->goods[goods] < amount) {
                v.refused = true;
                const sim::GoodsInfo* info =
                    setup_.registry ? setup_.registry->goods(goods) : nullptr;
                v.text = "would be refused: " + (info != nullptr ? info->name : "goods") + " " +
                         std::to_string(amount) + " needed, " +
                         std::to_string(snapshot->goods[goods]) + " held" + turn;
                return v;
            }
        }
    }
    std::string list;
    for (std::size_t i = 0; i < unchecked.size(); ++i) {
        list += (i == 0 ? "" : i + 1 == unchecked.size() ? " and " : ", ") + unchecked[i];
    }
    // The sim's placement rule is a stand-in that does not check terrain yet (OQ-052): a building
    // on open water is accepted today, so the text says so rather than promise a terrain check.
    v.text = "unchecked: " + list + " (the sim's; it does not check terrain yet, OQ-052)" + turn;
    return v;
}

std::string feedback_line(const DevTool& tool, const PlacementKnowledge::Verdict& verdict,
                          const PlaySnapshot* snapshot, const std::optional<CommandOutcome>& latest,
                          std::string_view note) {
    std::string s = dev_tool_status(tool);
    if (snapshot != nullptr) {
        s += " | money " + std::to_string(snapshot->money);
        if (const sim::BuildingType* type = tool.input().selected_type();
            type != nullptr && !tool.input().demolish_mode()) {
            // Each cost goods: held / needed, in the stock's units (the type's own costs).
            std::map<sim::GoodsId, std::uint64_t> need;
            for (const sim::CostEntry& c : type->cost_goods) {
                need[c.goods] += c.amount_88;
            }
            for (const auto& [goods, amount] : need) {
                const sim::GoodsInfo* info =
                    tool.setup().registry ? tool.setup().registry->goods(goods) : nullptr;
                s += ", " + (info != nullptr ? info->name : "goods " + std::to_string(goods)) +
                     " " +
                     (goods < snapshot->goods.size() ? std::to_string(snapshot->goods[goods])
                                                     : std::string("?")) +
                     "/" + std::to_string(amount);
            }
        }
        if (snapshot->costs_waived) {
            s += " (costs waived)";
        }
    }
    if (!verdict.text.empty()) {
        s += " | " + verdict.text;
    }
    if (!note.empty()) {
        s += " | ";
        s += note;
    }
    if (latest) {
        s += " | command #" + std::to_string(latest->id.n) + ": " +
             outcome_state_name(latest->state);
        if (latest->state == CommandOutcome::State::Refused) {
            s += " (" + std::string(sim::reject_name(latest->status)) + ")";
        } else if (latest->state == CommandOutcome::State::Applied && latest->records.size() > 1) {
            s += " (" + std::to_string(latest->records_done()) + " of " +
                 std::to_string(latest->records.size()) + " records)";
        } else if (latest->state == CommandOutcome::State::Applied && latest->records.size() == 1 &&
                   latest->records[0] != sim::Reject::None) {
            s += " (" + std::string(sim::reject_name(latest->records[0])) + ")";
        }
    }
    return s;
}

std::string drawn_note(const std::optional<CommandOutcome>& latest,
                       std::optional<std::uint64_t> followed_turn, std::size_t pending) {
    if (!latest || latest->state != CommandOutcome::State::Applied) {
        return {};
    }
    if (!followed_turn || *followed_turn < latest->turn) {
        return "not drawn yet: turn " + std::to_string(latest->turn) + " is not in the picture";
    }
    if (pending > 0) {
        return "not drawn yet: " + std::to_string(pending) +
               (pending == 1 ? " island of the picture is" : " islands of the picture are") +
               " not committed";
    }
    return {};
}

std::string dev_title(const PlaySnapshot& snapshot, const DevTool& tool,
                      const PlacementKnowledge::Verdict& verdict,
                      const std::optional<CommandOutcome>& latest, std::string_view note) {
    return PlayRunner::title_of(snapshot, false) + " — " +
           feedback_line(tool, verdict, &snapshot, latest, note);
}

} // namespace anno::app
