#include <cstdlib>
#include <utility>

#include <anno/app/dev_input.hpp>
#include <anno/sim/building.hpp>

namespace anno::app {

DevInput::DevInput(std::shared_ptr<const sim::TypeRegistry> registry,
                   std::vector<sim::TypeId> types, sim::CityRef city, sim::PlayerId issuer)
    : registry_(std::move(registry)), types_(std::move(types)), city_(city), issuer_(issuer) {}

std::optional<sim::TypeId> DevInput::selected() const {
    if (types_.empty()) {
        return std::nullopt;
    }
    return types_[index_ % types_.size()];
}

const sim::BuildingType* DevInput::selected_type() const {
    const auto id = selected();
    if (!id || !registry_) {
        return nullptr;
    }
    return registry_->find(*id);
}

bool DevInput::road_like() const {
    const sim::BuildingType* type = selected_type();
    return type != nullptr && (type->flags_word & sim::kRoadFlag) != 0;
}

std::vector<sim::TileXY> DevInput::footprint_at(sim::TileXY origin) const {
    std::vector<sim::TileXY> out;
    const sim::BuildingType* type = selected_type();
    if (type == nullptr) {
        return out;
    }
    const auto [w, h] = sim::footprint(*type, rotation_);
    for (std::uint32_t dy = 0; dy < h; ++dy) {
        for (std::uint32_t dx = 0; dx < w; ++dx) {
            out.push_back(sim::TileXY{origin.x + static_cast<std::int32_t>(dx),
                                      origin.y + static_cast<std::int32_t>(dy)});
        }
    }
    return out;
}

std::vector<sim::TileXY> DevInput::line(sim::TileXY from, sim::TileXY to) {
    const std::int32_t dx = to.x - from.x;
    const std::int32_t dy = to.y - from.y;
    const bool along_x = std::abs(dx) >= std::abs(dy);
    const std::int32_t steps = along_x ? std::abs(dx) : std::abs(dy);
    const std::int32_t sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
    const std::int32_t sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
    std::vector<sim::TileXY> out;
    out.reserve(static_cast<std::size_t>(steps) + 1);
    for (std::int32_t i = 0; i <= steps; ++i) {
        out.push_back(
            sim::TileXY{from.x + (along_x ? i * sx : 0), from.y + (along_x ? 0 : i * sy)});
    }
    return out;
}

sim::Command DevInput::build_command(const std::vector<sim::TileXY>& tiles) const {
    sim::BuildCmd build;
    build.city = city_;
    const sim::TypeId type = *selected();
    for (const sim::TileXY& tile : tiles) {
        sim::PlacementWire placement;
        placement.type = type;
        placement.x = tile.x;
        placement.y = tile.y;
        placement.rotation = rotation_;
        build.placements.push_back(placement);
    }
    sim::Command command;
    command.issuer = issuer_;
    command.body = std::move(build);
    return command;
}

sim::Command DevInput::demolish_command(const std::vector<sim::TileXY>& tiles) const {
    sim::DemolishCmd demolish;
    demolish.city = city_;
    demolish.tiles = tiles;
    sim::Command command;
    command.issuer = issuer_;
    command.body = std::move(demolish);
    return command;
}

std::vector<sim::Command> DevInput::handle(const DevEvent& event) {
    message_.clear();
    std::vector<sim::Command> out;
    const std::size_t count = types_.size();
    switch (event.kind) {
    case DevEvent::Kind::NextType:
        if (count > 0) {
            index_ = (index_ + 1) % count;
        }
        break;
    case DevEvent::Kind::PrevType:
        if (count > 0) {
            index_ = (index_ + count - 1) % count;
        }
        break;
    case DevEvent::Kind::Rotate:
        rotation_ = static_cast<std::uint8_t>((rotation_ + 1) % 4);
        break;
    case DevEvent::Kind::ToggleDemolish:
        demolish_ = !demolish_;
        press_.reset();
        break;
    case DevEvent::Kind::Cancel:
        demolish_ = false;
        press_.reset();
        break;
    case DevEvent::Kind::Press:
        press_ = event.tile;
        hover_ = event.tile;
        break;
    case DevEvent::Kind::Move:
        hover_ = event.tile;
        break;
    case DevEvent::Kind::Release: {
        if (!press_) {
            break;
        }
        const sim::TileXY from = *press_;
        press_.reset();
        hover_ = event.tile;
        const std::vector<sim::TileXY> tiles = line(from, event.tile);
        if (demolish_) {
            out.push_back(demolish_command(tiles));
        } else {
            if (!selected()) {
                message_ = "no buildable type";
                break;
            }
            if (tiles.size() > 1 && !road_like()) {
                message_ = "a drag places a road; the selected type is not one";
                break;
            }
            out.push_back(build_command(tiles));
        }
        if (!sim::has_wire_form(out.back().body)) {
            out.clear();
            message_ = "a drag of " + std::to_string(tiles.size()) +
                       " tiles is over the wire limit of one command";
        }
        break;
    }
    }
    return out;
}

} // namespace anno::app
