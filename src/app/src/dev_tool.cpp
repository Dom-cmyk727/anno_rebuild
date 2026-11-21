// The developer building tool (see the header).
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <anno/app/dev_tool.hpp>
#include <anno/app/frame_projection.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/site_checks.hpp>

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
        const auto [ox, oy] =
            render::tile_screen(x, y, static_cast<double>(tw), static_cast<double>(th));
        const double ex = p.offset.first - ox;
        const double ey = p.offset.second - oy;
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
      input_(setup_.registry, setup_.types, setup_.city.value_or(sim::CityRef{}), setup_.issuer,
             setup_.road_rules) {}

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
    // The islands the ray's segment can meet, in index order (round ee, D6): the pick then asks
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

std::uint8_t DevTool::turn() const {
    const std::uint8_t cursor = input_.rotation();
    const sim::BuildingType* type = input_.selected_type();
    if (!hover_ || type == nullptr || !scene_ || !coast_turned(*type) ||
        !sim::site::coast_class_supported(type->flags_word)) {
        return cursor;
    }
    const sim::TileXY o = input_.origin(*hover_);
    const TileRect& isl = setup_.island;
    for (const std::uint8_t rot : sim::site::rotation_trials(cursor)) {
        const auto [w, h] = sim::footprint(*type, rot);
        if (!isl.contains(o) ||
            !isl.contains(sim::TileXY{static_cast<std::int32_t>(std::int64_t{o.x} + w - 1),
                                      static_cast<std::int32_t>(std::int64_t{o.y} + h - 1)})) {
            continue;
        }
        const auto verdict = sim::site::coast_height_rule(
            type->width, type->height, rot, [&](std::uint64_t vx, std::uint64_t vy) {
                return scene_vertex_height(*scene_,
                                           std::int64_t{o.x} + static_cast<std::int64_t>(vx),
                                           std::int64_t{o.y} + static_cast<std::int64_t>(vy));
            });
        if (verdict.result == sim::site::TerrainResult::Accepted) {
            return rot;
        }
    }
    return cursor;
}

void DevTool::forward(DevEvent::Kind kind, sim::TileXY tile, std::vector<sim::Command>& out) {
    std::optional<std::uint8_t> chosen;
    if (kind == DevEvent::Kind::Release) {
        chosen = turn();
    }
    for (sim::Command& c : input_.handle(DevEvent{kind, tile}, chosen)) {
        out.push_back(std::move(c));
    }
    if (!input_.message().empty()) {
        message_ = input_.message();
    }
}

bool DevTool::arm(sim::TypeId type) {
    if (!setup_.city) {
        message_ = "no city yet: found one first (F with a ship selected), then B";
        return false;
    }
    if (!input_.arm(type)) {
        message_ = "type " + std::to_string(type.v) + " is not in the tool's cycle list";
        return false;
    }
    if (input_.demolish_mode()) {
        // A building chosen while demolishing is placed, not demolished (UI-PLACE-1).
        static_cast<void>(input_.handle(DevEvent{DevEvent::Kind::ToggleDemolish, {}}));
        press_.reset();
    }
    enabled_ = true;
    demolish_only_ = false;
    message_.clear();
    return true;
}

bool DevTool::toggle_demolish() {
    if (!enabled_) {
        if (!setup_.city) {
            message_ = "no city yet: found one first (F with a ship selected), then B";
            return false;
        }
        // Nothing armed: the pickaxe turns the tool on for the demolish mode alone.
        enabled_ = true;
        demolish_only_ = true;
        if (!input_.demolish_mode()) {
            static_cast<void>(input_.handle(DevEvent{DevEvent::Kind::ToggleDemolish, {}}));
        }
    } else {
        static_cast<void>(input_.handle(DevEvent{DevEvent::Kind::ToggleDemolish, {}}));
    }
    press_.reset();
    message_.clear();
    settle_demolish_only();
    return true;
}

void DevTool::settle_demolish_only() {
    // The demolish mode the pickaxe turned on, now off: nothing is armed (GOG p2_pickaxe_off).
    if (demolish_only_ && !input_.demolish_mode()) {
        enabled_ = false;
        demolish_only_ = false;
        press_.reset();
    }
}

void DevTool::retarget(sim::CityRef city, TileRect island) {
    // A new DevInput starts at the first type: keep the type and the rotation the player chose.
    const std::optional<sim::TypeId> kept = input_.selected();
    const std::uint8_t rotation = input_.rotation();
    setup_.city = city;
    setup_.island = island;
    input_ = DevInput(setup_.registry, setup_.types, city, setup_.issuer);
    if (kept) {
        for (std::size_t i = 0; i < setup_.types.size() && input_.selected() != kept; ++i) {
            static_cast<void>(input_.handle(DevEvent{DevEvent::Kind::NextType, {}}));
        }
    }
    for (std::uint8_t r = 0; r < rotation; ++r) {
        static_cast<void>(input_.handle(DevEvent{DevEvent::Kind::Rotate, {}}));
    }
    hover_.reset();
    ghost_hover_.reset();
    press_.reset();
    settle_demolish_only(); // the new DevInput starts out of the demolish mode
}

std::vector<sim::Command> DevTool::handle(const ToolInput& in, const PointerFrame* frame) {
    std::vector<sim::Command> out;
    using K = ToolInput::Kind;
    if (in.kind == K::Toggle) {
        if (enabled_) {
            forward(DevEvent::Kind::Cancel, {}, out);
            press_.reset();
            enabled_ = false;
            demolish_only_ = false;
            message_.clear();
        } else if (!setup_.city) {
            message_ = "no city yet: found one first (F with a ship selected), then B";
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
    case K::NextTypeFast:
    case K::PrevTypeFast:
        for (int i = 0; i < 10; ++i) {
            forward(in.kind == K::NextTypeFast ? DevEvent::Kind::NextType
                                               : DevEvent::Kind::PrevType,
                    {}, out);
        }
        break;
    case K::Rotate:
        forward(DevEvent::Kind::Rotate, {}, out);
        break;
    case K::RotateBack:
        forward(DevEvent::Kind::RotateBack, {}, out);
        break;
    case K::ToggleDemolish:
        forward(DevEvent::Kind::ToggleDemolish, {}, out);
        press_.reset();
        settle_demolish_only();
        break;
    case K::Cancel:
        forward(DevEvent::Kind::Cancel, {}, out);
        press_.reset();
        settle_demolish_only();
        break;
    case K::Abort:
        forward(DevEvent::Kind::Abort, {}, out);
        press_.reset();
        hover_.reset();
        ghost_hover_.reset();
        break;
    case K::Press:
    case K::Move:
    case K::Release: {
        std::string why;
        const auto tile = frame != nullptr ? pick(*frame, in.px, in.py, &why) : std::nullopt;
        if (frame == nullptr) {
            why = "no frame drawn yet";
        } else {
            scene_ = frame->scene;
        }
        hover_ = tile;
        // UI-GHOST-1 R1: off the city's island the pointer still names the map tile under it (any
        // island's ground, else the sea-level tile): the ghost shows the sim's refusal there, and
        // a click sends its record so that the sim refuses it with its own reason (OutOfIsland,
        // TileNotOwned, ...), the same verdict the ghost shows.
        ghost_hover_ = tile;
        if (!tile && frame != nullptr) {
            if (const auto any = pick_point(*frame, in.px, in.py)) {
                ghost_hover_ = any->tile;
            }
        }
        const std::optional<sim::TileXY> at = ghost_hover_;
        if (in.kind == K::Move) {
            if (at && input_.pressed()) {
                forward(DevEvent::Kind::Move, *at, out); // motion never makes a command
            }
            break;
        }
        if (!at) {
            // A press with no map tile under it (outside the window, no frame) starts nothing; a
            // release there drops the press, so it cannot place anything later.
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
            press_ = at;
            forward(DevEvent::Kind::Press, *at, out);
        } else {
            press_.reset();
            forward(DevEvent::Kind::Release, *at, out);
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
        const std::uint8_t turn = tool.turn();
        const auto [w, h] = sim::footprint(*type, turn);
        s += "build " + std::to_string(type->id.v) + " (" + std::to_string(w) + "x" +
             std::to_string(h) + ", " + std::to_string(type->cost_gold) + " gold) rotation " +
             std::to_string(turn);
        if (turn != in.rotation()) {
            s += " (cursor " + std::to_string(in.rotation()) + ", turned to the coast)";
        }
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
