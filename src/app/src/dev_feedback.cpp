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

std::optional<Preview> make_type_preview(const render::Scene& scene, sim::TypeId type,
                                         std::uint8_t rotation, sim::TileXY origin,
                                         std::size_t group, std::uint64_t time_ms) {
    if (type.v >= kPictureAnlageIds || scene.anlagen.entry(type.v) == nullptr ||
        group >= render::kGroups) {
        return std::nullopt;
    }
    std::optional<std::size_t> index;
    for (std::size_t k = 0; k < scene.islands.size() && !index; ++k) {
        const auto& i = scene.islands[k];
        if (origin.x >= i.x && origin.y >= i.y && origin.x < i.x + static_cast<int>(i.width) &&
            origin.y < i.y + static_cast<int>(i.height)) {
            index = k;
        }
    }
    if (!index) {
        return std::nullopt;
    }
    const auto& island = scene.islands[*index];
    const auto lx = static_cast<std::uint16_t>(origin.x - island.x);
    const auto ly = static_cast<std::uint16_t>(origin.y - island.y);
    const float h = scene.ground_height(static_cast<float>(origin.x), static_cast<float>(origin.y))
                        .value_or(0.0F);
    const auto height = static_cast<std::uint8_t>(std::clamp(std::lround(h), 0L, 255L));
    Preview p;
    p.island = *index;
    p.sprites = scene.preview_sprites(*index, static_cast<std::uint16_t>(type.v), rotation, lx, ly,
                                      height, group, time_ms);
    formats::insel::PlacedObject o;
    o.anlage = static_cast<std::uint16_t>(type.v);
    o.rotation = rotation;
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
    seed(buildings, scene);
    return true;
}

void PlacementKnowledge::seed(std::span<const SceneBuilding> local, const render::Scene& scene) {
    ++rebuilds_;
    occupied_.clear();
    indexed_ = false;
    seeded_ = true;
    stale_ = false;
    changed_ = true;
    number_.reset();
    const auto index = city_island(scene, setup_.island);
    if (!index || !setup_.registry) {
        return;
    }
    indexed_ = true;
    number_ = scene.islands[*index].number;
    for (const SceneBuilding& b : local) {
        if (b.island == *number_) {
            add_tiles(b, +1);
        }
    }
}

bool PlacementKnowledge::on_batches(
    const std::vector<std::shared_ptr<const BuildingBatch>>& batches, const render::Scene& scene,
    bool enabled) {
    if (!enabled) {
        stale_ = true; // the batches are not followed while the tool is off; ensure() reseeds
        batches_skipped_disabled_ += batches.size();
        return false;
    }
    if (!seeded_ || stale_) {
        return false; // ensure() seeds from the picture, which already has these batches or will
    }
    if (!indexed_ || !number_) {
        return false;
    }
    bool changed = false;
    for (const auto& batch : batches) {
        if (batch->resync) {
            seed(batch->full, scene); // the list after the batch replaces everything
            changed = true;
            continue;
        }
        for (const BuildingDelta& d : batch->deltas) {
            if (d.building.island != *number_) {
                ++foreign_deltas_;
                continue; // another island: nothing to index (one comparison)
            }
            add_tiles(d.building, d.kind == BuildingDelta::Kind::Added ? +1 : -1);
            ++deltas_applied_;
            changed = true;
        }
    }
    changed_ = changed_ || changed;
    return changed;
}

bool PlacementKnowledge::ensure(const ScenePicture& picture, const render::Scene& scene,
                                bool enabled) {
    if (!enabled || (seeded_ && !stale_)) {
        return false;
    }
    // The city island's number is the scene's; the picture keeps its buildings per island.
    const auto index = city_island(scene, setup_.island);
    seed(index ? std::span<const SceneBuilding>(
                     picture.island_buildings(scene.islands[*index].number))
               : std::span<const SceneBuilding>(),
         scene);
    return true;
}

void PlacementKnowledge::add_tiles(const SceneBuilding& b, int sign) {
    {
        const sim::BuildingType* type = setup_.registry->find(sim::TypeId{b.type});
        const auto [w, h] = type != nullptr ? sim::footprint(*type, b.rotation)
                                            : std::pair<std::uint32_t, std::uint32_t>{1, 1};
        // The rectangle in 64-bit coordinates, clamped to the city's island: a tile outside it
        // is never asked about (check refuses the footprint first), and a registry dimension
        // cannot overflow a 32-bit tile or fill the index past the island (round ee review, R3).
        const std::int64_t bx = std::int64_t{setup_.island.x0} + b.x;
        const std::int64_t by = std::int64_t{setup_.island.y0} + b.y;
        const std::int64_t x1 = std::min(bx + w, std::int64_t{setup_.island.x0} + setup_.island.w);
        const std::int64_t y1 = std::min(by + h, std::int64_t{setup_.island.y0} + setup_.island.h);
        for (std::int64_t y = std::max<std::int64_t>(by, setup_.island.y0); y < y1; ++y) {
            for (std::int64_t x = std::max<std::int64_t>(bx, setup_.island.x0); x < x1; ++x) {
                const std::pair<std::int32_t, std::int32_t> key{static_cast<std::int32_t>(x),
                                                                static_cast<std::int32_t>(y)};
                ++tiles_enumerated_;
                if (sign > 0) {
                    ++occupied_[key];
                } else if (const auto it = occupied_.find(key); it != occupied_.end()) {
                    if (--it->second == 0) {
                        occupied_.erase(it);
                    }
                }
            }
        }
    }
}

bool PlacementKnowledge::occupied(sim::TileXY tile) const {
    return occupied_.contains({tile.x, tile.y});
}

bool PlacementKnowledge::occupied_in(std::int64_t x0, std::int64_t y0, std::int64_t x1,
                                     std::int64_t y1) const {
    // The set orders by x, then y: one lower_bound per column answers whether the column holds
    // an occupied tile in [y0, y1). The caller has clamped the rectangle to the island (int32).
    for (std::int64_t x = x0; x < x1; ++x) {
        const auto it =
            occupied_.lower_bound({static_cast<std::int32_t>(x), static_cast<std::int32_t>(y0)});
        if (it != occupied_.end() && it->first.first == x && it->first.second < y1) {
            return true;
        }
    }
    return false;
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
    // The footprint as a rectangle in 64-bit coordinates, tested against the island before any
    // tile is looked at (round ee review, R3): an oversized or zero definition is refused in O(1)
    // and no tile list is made. What passes lies inside the island, so the occupancy test below
    // costs at most one lookup per column of the admitted region.
    const auto extent = tool.input().footprint_extent(*hover);
    // A zero-sized definition cannot come from the validating registry builder (the sim's rule,
    // F25); the branch keeps the check closed for a registry that does not validate.
    if (!extent || extent->w == 0 || extent->h == 0) {
        v.refused = true;
        v.text = "would be refused: the type has a zero-sized footprint";
        return v;
    }
    const TileRect& isl = setup_.island;
    const std::int64_t ex1 = extent->x0 + extent->w, ey1 = extent->y0 + extent->h;
    if (extent->x0 < isl.x0 || extent->y0 < isl.y0 || ex1 > std::int64_t{isl.x0} + isl.w ||
        ey1 > std::int64_t{isl.y0} + isl.h) {
        v.refused = true;
        v.text = "would be refused: the footprint leaves the city's island";
        return v;
    }
    if (occupied_in(extent->x0, extent->y0, ex1, ey1)) {
        v.refused = true;
        v.text = "would be refused: occupied in the published picture";
        return v;
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

void attach_occupancy(PlayFollower& follower, PlacementKnowledge& knowledge, const DevTool& tool) {
    follower.set_batch_observer(
        [&follower, &knowledge, &tool](const std::vector<std::shared_ptr<const BuildingBatch>>& b) {
            if (const auto scene = follower.picture().scene()) {
                knowledge.on_batches(b, *scene, tool.enabled());
            }
        });
}

std::string drawn_note(const std::optional<CommandOutcome>& latest, Visibility visibility) {
    if (!latest || latest->state != CommandOutcome::State::Applied) {
        return {};
    }
    return visibility_note(visibility, latest->turn);
}

std::string dev_title(const PlaySnapshot& snapshot, const DevTool& tool,
                      const PlacementKnowledge::Verdict& verdict,
                      const std::optional<CommandOutcome>& latest, std::string_view note) {
    return PlayRunner::title_of(snapshot, false) + " — " +
           feedback_line(tool, verdict, &snapshot, latest, note);
}

} // namespace anno::app
