#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

#include <anno/app/dev_input.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/site_checks.hpp>

namespace anno::app {

bool coast_turned(const sim::BuildingType& type) {
    switch (type.prodtype) {
    case 6:  // FISCHER
    case 7:  // WERFT
    case 15: // WALFAENGER
    case 35: // KONTOR
        return true;
    default:
        return false;
    }
}

bool deposit_turned(const sim::BuildingType& type) {
    return type.prodtype == 3 || type.prodtype == 13; // MINE, STEINBRUCH
}

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

sim::TileXY DevInput::origin(sim::TileXY hover) const {
    const sim::BuildingType* type = selected_type();
    if (type == nullptr) {
        return hover;
    }
    const auto [w, h] = sim::footprint(*type, rotation_);
    const sim::site::Origin o = sim::site::centred_origin(hover.x, hover.y, w, h);
    // Centred and clamped at 0, the origin lies between 0 and the hovered tile: a 32-bit tile.
    return sim::TileXY{static_cast<std::int32_t>(o.x), static_cast<std::int32_t>(o.y)};
}

std::optional<DevInput::Extent> DevInput::footprint_extent(sim::TileXY hover) const {
    return footprint_extent(hover, rotation_);
}

std::optional<DevInput::Extent> DevInput::footprint_extent(sim::TileXY hover,
                                                           std::uint8_t turn) const {
    const sim::BuildingType* type = selected_type();
    if (type == nullptr) {
        return std::nullopt;
    }
    const sim::TileXY o = origin(hover);
    const auto [w, h] = sim::footprint(*type, turn);
    return Extent{o.x, o.y, w, h};
}

std::vector<sim::TileXY> DevInput::footprint_at(sim::TileXY hover, std::uint64_t max_tiles) const {
    std::vector<sim::TileXY> out;
    const auto extent = footprint_extent(hover);
    if (!extent || extent->area() > max_tiles) {
        return out;
    }
    // The far corner must be a 32-bit tile coordinate too, or a tile could not be named.
    constexpr std::int64_t kMax = std::numeric_limits<std::int32_t>::max();
    if (extent->x0 + extent->w - 1 > kMax || extent->y0 + extent->h - 1 > kMax) {
        return out;
    }
    out.reserve(static_cast<std::size_t>(extent->area()));
    for (std::uint32_t dy = 0; dy < extent->h; ++dy) {
        for (std::uint32_t dx = 0; dx < extent->w; ++dx) {
            out.push_back(sim::TileXY{static_cast<std::int32_t>(extent->x0 + dx),
                                      static_cast<std::int32_t>(extent->y0 + dy)});
        }
    }
    return out;
}

std::uint64_t DevInput::line_length(sim::TileXY from, sim::TileXY to) {
    const std::int64_t dx = static_cast<std::int64_t>(to.x) - from.x;
    const std::int64_t dy = static_cast<std::int64_t>(to.y) - from.y;
    return static_cast<std::uint64_t>(std::max(std::llabs(dx), std::llabs(dy))) + 1;
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

sim::Command DevInput::build_command(const std::vector<sim::TileXY>& tiles,
                                     std::optional<std::uint8_t> turn) const {
    sim::BuildCmd build;
    build.city = city_;
    const sim::TypeId type = *selected();
    for (const sim::TileXY& tile : tiles) {
        // The footprint centred on the tile (a road's 1 x 1 stays on it).
        const sim::TileXY o = origin(tile);
        sim::PlacementWire placement;
        placement.type = type;
        placement.x = o.x;
        placement.y = o.y;
        placement.rotation = turn.value_or(rotation_);
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

std::vector<sim::Command> DevInput::handle(const DevEvent& event,
                                           std::optional<std::uint8_t> turn) {
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
    case DevEvent::Kind::RotateBack:
        if (const sim::BuildingType* type = selected_type();
            type != nullptr && deposit_turned(*type)) {
            message_ = "a mine or quarry takes its turn from the deposit (not modelled yet)";
            break;
        }
        rotation_ = static_cast<std::uint8_t>(
            (rotation_ + (event.kind == DevEvent::Kind::Rotate ? 1U : 3U)) % 4U);
        break;
    case DevEvent::Kind::ToggleDemolish:
        demolish_ = !demolish_;
        press_.reset();
        break;
    case DevEvent::Kind::Cancel:
        demolish_ = false;
        press_.reset();
        break;
    case DevEvent::Kind::Abort:
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
        const std::uint64_t length = line_length(from, event.tile);
        if (length > sim::kMaxRecordPayload) {
            message_ = "a drag of " + std::to_string(length) +
                       " tiles is over the wire limit of one command";
            break;
        }
        const std::vector<sim::TileXY> tiles = line(from, event.tile);
        if (demolish_) {
            out.push_back(demolish_command(tiles));
        } else {
            if (!selected()) {
                message_ = "no buildable type";
                break;
            }
            if (selected_type() == nullptr) {
                // A listed id the registry lacks (a malformed setup): the sim would refuse it
                // (UnknownType); nothing is sent (round ee D4).
                message_ = "type " + std::to_string(selected()->v) + " is not in the registry";
                break;
            }
            if (tiles.size() > 1 && !road_like()) {
                message_ = "a drag places a road; the selected type is not one";
                break;
            }
            out.push_back(build_command(tiles, tiles.size() == 1 ? turn : std::nullopt));
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
