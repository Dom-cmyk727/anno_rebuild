// World::apply: commands applied between advances (docs/sim/state.md §8). Stage S8
// implements SetSpeed and SetProductionLine; the other bodies answer Unsupported until
// stage S10.
#include <algorithm>
#include <optional>
#include <type_traits>
#include <variant>

#include <anno/sim/apply.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

namespace {

/// Switches the line of output `index` of a payload. False when the payload has no
/// switch for that output (a class without one, or an index beyond its lines).
bool switch_line(Payload& payload, std::size_t index, bool on) {
    struct Switch {
        std::size_t index;
        bool on;
        bool operator()(Workshop& w) const {
            w.switched_on = on;
            return true;
        }
        bool operator()(Farm& f) const {
            f.switched_on = on;
            return true;
        }
        bool operator()(Mine& m) const {
            m.switched_on = on;
            return true;
        }
        bool operator()(WeaponsWorkshop& w) const {
            if (index >= w.line_on.size()) {
                return false;
            }
            w.line_on[index] = on;
            return true;
        }
        bool operator()(SmallFarm& s) const {
            if (index >= s.line_on.size()) {
                return false;
            }
            s.line_on[index] = on;
            return true;
        }
        bool operator()(Inert&) const { return false; }
    };
    return std::visit(Switch{index, on}, payload);
}

Reject set_speed(const SimConfig& cfg, WorldSettings& settings, const Command& c,
                 const SetSpeedCmd& s) {
    if (c.issuer != cfg.host_player) {
        return Reject::NotHost;
    }
    if (!is_original_speed(s.percent, cfg.multiplayer)) {
        return Reject::BadSpeed;
    }
    settings.speed_percent = s.percent;
    return Reject::None;
}

Reject set_production_line(World& w, const SimConfig& cfg, const TypeRegistry& types,
                           const Command& c, const SetProductionLineCmd& s) {
    Island* is = w.island_at(s.tile.x, s.tile.y);
    if (is == nullptr) {
        return Reject::UnknownIsland;
    }
    const Slot idx = is->tile_index(s.tile.x, s.tile.y);
    const std::uint32_t owner = is->tiles.owner(idx);
    const City* city = owner == cfg.neutral_owner ? nullptr : is->city(CityNo{owner});
    if (city == nullptr) {
        return Reject::UnknownCity;
    }
    if (city->owner != c.issuer) {
        return Reject::NotCityOwner;
    }
    const Building* found = is->building_at(cfg.layout, city->number, s.tile.x, s.tile.y);
    if (found == nullptr) {
        return Reject::NoBuildingAtTile;
    }
    const BuildingType* type = types.find(found->type);
    if (type == nullptr) {
        return Reject::UnknownType;
    }
    const auto it = std::find(type->output_goods.begin(), type->output_goods.end(), s.goods);
    if (it == type->output_goods.end()) {
        return Reject::LineNotFound;
    }
    Building* b = find_building_mut(*is, found->id);
    if (b == nullptr ||
        !switch_line(b->payload, static_cast<std::size_t>(it - type->output_goods.begin()), s.on)) {
        return Reject::LineNotFound;
    }
    return Reject::None;
}

} // namespace

ApplyResult Commands::apply(World& w, const Command& command, bool inside_advance) {
    ApplyResult result;
    if (w.players_.find(command.issuer) == nullptr) {
        result.status = Reject::UnknownIssuer;
    } else if (const auto* speed = std::get_if<SetSpeedCmd>(&command.body)) {
        result.status = set_speed(w.config_, w.settings_, command, *speed);
    } else if (const auto* line = std::get_if<SetProductionLineCmd>(&command.body)) {
        result.status = set_production_line(w, w.config_, *w.types_, command, *line);
    } else {
        // Build, Demolish and the trade commands are stage S10; a legacy record is never
        // executed (fail closed, §8.3).
        result.status = Reject::Unsupported;
    }
    if (!result.ok()) {
        SimEvent e;
        e.kind = SimEventKind::CommandRejected;
        e.tick = w.clock_.tick();
        e.player = command.issuer;
        e.code = static_cast<std::uint32_t>(result.status);
        w.host_events_.push(e);
        if (inside_advance) {
            w.out_.events.push_back(e);
        }
    }
    return result;
}

ApplyResult World::apply(const Command& command) {
    return Commands::apply(*this, command, false);
}

} // namespace anno::sim
