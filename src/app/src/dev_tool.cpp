// The developer building tool (see the header).
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <anno/app/dev_tool.hpp>
#include <anno/sim/building.hpp>

namespace anno::app {

std::pair<double, double> placement_rounding(const render::Scene& scene, std::size_t index,
                                             std::size_t group, double tile_w) {
    const std::size_t built = std::min(group, render::kGroups - 1);
    if (index >= scene.islands.size()) {
        return {0.0, 0.0};
    }
    const auto& island = scene.islands[index];
    for (const render::Placement& p : scene.layouts[built]) {
        if (p.index != index) {
            continue;
        }
        const auto [tw, th] = render::kTile[built];
        const double x = static_cast<double>(island.x), y = static_cast<double>(island.y);
        const double ex = p.offset.first - (x - y) * tw / 2.0;
        const double ey = p.offset.second - (x + y) * th / 2.0;
        const double scale = group < render::kGroups ? 1.0 : tile_w / tw;
        return {ex * scale, ey * scale};
    }
    return {0.0, 0.0};
}

PointerFrame pointer_frame(render::Rect view, const render::Resample* resample, std::size_t group,
                           double tile_w, double tile_h, std::shared_ptr<const render::Scene> scene,
                           int drawable_w, int drawable_h) {
    PointerFrame f;
    f.group = group;
    f.map.x0 = view.x + (resample != nullptr ? resample->offset_x : 0.0);
    f.map.y0 = view.y + (resample != nullptr ? resample->offset_y : 0.0);
    f.map.scale = resample != nullptr ? resample->scale : 1.0;
    f.geometry = render::pick_geometry_of(tile_w, tile_h);
    f.scene = std::move(scene);
    f.drawable_w = drawable_w;
    f.drawable_h = drawable_h;
    return f;
}

DevTool::DevTool(DevToolSetup setup)
    : setup_(std::move(setup)),
      input_(setup_.registry, setup_.types, setup_.city.value_or(sim::CityRef{}), setup_.issuer) {}

std::optional<sim::TileXY> DevTool::pick(const PointerFrame& frame, double px, double py,
                                         std::string* why) const {
    auto fail = [why](const char* reason) {
        if (why != nullptr) {
            *why = reason;
        }
        return std::optional<sim::TileXY>{};
    };
    if (!(px >= 0.0) || !(py >= 0.0) || !(px < frame.drawable_w) || !(py < frame.drawable_h)) {
        return fail("outside the window");
    }
    const auto point = render::scene_point(frame.map, px, py);
    if (!point || !frame.scene) {
        return fail("no tile here");
    }
    const render::Scene& scene = *frame.scene;
    // The city's island is drawn at its integer placement: undo that rounding first, so the pick
    // solves the rule the island's pixels were drawn with.
    std::optional<std::pair<double, double>> rounding;
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        if (scene.islands[k].x == setup_.island.x0 && scene.islands[k].y == setup_.island.y0) {
            rounding = placement_rounding(scene, k, frame.group, frame.geometry.tile_w);
            break;
        }
    }
    if (!rounding) {
        return fail("the city's island is not in the scene"); // never a pick without its offset
    }
    const double sx = point->first - rounding->first, sy = point->second - rounding->second;
    // The islands the ray's segment can meet, in index order (round ee, D6): the march then asks
    // only those, and gets what Scene::ground_height (the first island in index order) gives.
    // A tile of margin covers the float conversion of the lookups.
    const auto ray = render::pick_ray(sx, sy, frame.geometry);
    if (!ray) {
        return fail("no tile here");
    }
    const double u_lo = std::min(ray->u0, ray->u1) - 1.0, u_hi = std::max(ray->u0, ray->u1) + 1.0;
    const double v_lo = std::min(ray->v0, ray->v1) - 1.0, v_hi = std::max(ray->v0, ray->v1) + 1.0;
    std::vector<std::size_t> crossed;
    for (std::size_t k = 0; k < scene.islands.size(); ++k) {
        const auto& i = scene.islands[k];
        if (i.x + static_cast<double>(i.width) >= u_lo && i.x <= u_hi &&
            i.y + static_cast<double>(i.height) >= v_lo && i.y <= v_hi) {
            crossed.push_back(k);
        }
    }
    const auto tile =
        render::pick_tile(sx, sy, frame.geometry, [&scene, &crossed](float u, float v) {
            for (const std::size_t k : crossed) {
                if (const auto h = scene.island_ground_height(k, u, v)) {
                    return h;
                }
            }
            return std::optional<float>{};
        });
    if (!tile) {
        return fail("no tile here");
    }
    const sim::TileXY t{tile->first, tile->second};
    if (!setup_.island.contains(t)) {
        return fail("not the city's island");
    }
    return t;
}

void DevTool::forward(DevEvent::Kind kind, sim::TileXY tile, std::vector<sim::Command>& out) {
    for (sim::Command& c : input_.handle(DevEvent{kind, tile})) {
        out.push_back(std::move(c));
    }
    if (!input_.message().empty()) {
        message_ = input_.message();
    }
}

std::vector<sim::Command> DevTool::handle(const ToolInput& in, const PointerFrame* frame) {
    std::vector<sim::Command> out;
    using K = ToolInput::Kind;
    if (in.kind == K::Toggle) {
        if (enabled_) {
            forward(DevEvent::Kind::Cancel, {}, out);
            press_.reset();
            enabled_ = false;
            message_.clear();
        } else if (!setup_.city) {
            message_ = "no city of the developer player: the tool is off";
        } else {
            enabled_ = true;
            message_.clear();
        }
        return out;
    }
    if (!enabled_) {
        return out;
    }
    message_.clear();
    switch (in.kind) {
    case K::NextType:
        forward(DevEvent::Kind::NextType, {}, out);
        break;
    case K::PrevType:
        forward(DevEvent::Kind::PrevType, {}, out);
        break;
    case K::Rotate:
        forward(DevEvent::Kind::Rotate, {}, out);
        break;
    case K::ToggleDemolish:
        forward(DevEvent::Kind::ToggleDemolish, {}, out);
        press_.reset();
        break;
    case K::Cancel:
        forward(DevEvent::Kind::Cancel, {}, out);
        press_.reset();
        break;
    case K::Abort:
        forward(DevEvent::Kind::Abort, {}, out);
        press_.reset();
        hover_.reset();
        break;
    case K::Press:
    case K::Move:
    case K::Release: {
        std::string why;
        const auto tile = frame != nullptr ? pick(*frame, in.px, in.py, &why) : std::nullopt;
        if (frame == nullptr) {
            why = "no frame drawn yet";
        }
        hover_ = tile;
        if (in.kind == K::Move) {
            if (tile && input_.pressed()) {
                forward(DevEvent::Kind::Move, *tile, out); // motion never makes a command
            }
            break;
        }
        if (!tile) {
            // A press off the island starts nothing; a release there drops the press, so it
            // cannot place anything later.
            if (in.kind == K::Release && input_.pressed()) {
                forward(DevEvent::Kind::Abort, {}, out);
                message_ = why + ": nothing sent";
            } else {
                message_ = why;
            }
            press_.reset();
            break;
        }
        if (in.kind == K::Press) {
            press_ = tile;
            forward(DevEvent::Kind::Press, *tile, out);
        } else {
            press_.reset();
            forward(DevEvent::Kind::Release, *tile, out);
        }
        break;
    }
    case K::Toggle:
        break;
    }
    return out;
}

std::string dev_tool_status(const DevTool& tool) {
    const DevInput& in = tool.input();
    std::string s = "tool: ";
    if (in.demolish_mode()) {
        s += "demolish";
    } else if (const sim::BuildingType* type = in.selected_type()) {
        const auto [w, h] = sim::footprint(*type, in.rotation());
        s += "build " + std::to_string(type->id.v) + " (" + std::to_string(w) + "x" +
             std::to_string(h) + ", " + std::to_string(type->cost_gold) + " gold) rotation " +
             std::to_string(in.rotation());
    } else if (const auto id = in.selected()) {
        s += "type " + std::to_string(id->v) + " is not in the registry"; // a malformed setup
    } else {
        s += "no buildable type";
    }
    if (const auto t = tool.hover()) {
        s += " at " + std::to_string(t->x) + "," + std::to_string(t->y);
    }
    if (!tool.message().empty()) {
        s += " — " + tool.message();
    }
    return s;
}

} // namespace anno::app
