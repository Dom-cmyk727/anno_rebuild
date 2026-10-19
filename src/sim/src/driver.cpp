// The driver: tick_building and consume (docs/sim/state.md §7.1, §7.2, stage S7).
#include <cstddef>
#include <variant>

#include <anno/sim/driver.hpp>

namespace anno::sim {

bool tick_kind_enabled(TickKind kind) {
    // Stage S7: Workshop and Farm only. -Wswitch (no default) makes a tick kind
    // added later a compile error here, so no kind can be forgotten.
    switch (kind) {
    case TickKind::Workshop:
    case TickKind::Farm:
        return true;
    case TickKind::None:
    case TickKind::WeaponsWorkshop:
    case TickKind::FurTrapper:
    case TickKind::Mine:
    case TickKind::SmallFarm:
        return false;
    }
    return false;
}

std::uint32_t* figure_counter(Payload& payload, std::uint32_t home_counter) {
    if (home_counter == 0) {
        if (auto* w = std::get_if<Workshop>(&payload)) {
            return &w->carriers_out;
        }
        if (auto* w = std::get_if<WeaponsWorkshop>(&payload)) {
            return &w->carriers_out;
        }
        if (auto* f = std::get_if<Farm>(&payload)) {
            return &f->workers_out;
        }
    }
    if (auto* s = std::get_if<SmallFarm>(&payload)) {
        if (home_counter < s->figures_out.size()) {
            return &s->figures_out[home_counter];
        }
    }
    return nullptr;
}

const std::uint32_t* figure_counter(const Payload& payload, std::uint32_t home_counter) {
    if (home_counter == 0) {
        if (const auto* w = std::get_if<Workshop>(&payload)) {
            return &w->carriers_out;
        }
        if (const auto* w = std::get_if<WeaponsWorkshop>(&payload)) {
            return &w->carriers_out;
        }
        if (const auto* f = std::get_if<Farm>(&payload)) {
            return &f->workers_out;
        }
    }
    if (const auto* s = std::get_if<SmallFarm>(&payload)) {
        if (home_counter < s->figures_out.size()) {
            return &s->figures_out[home_counter];
        }
    }
    return nullptr;
}

std::uint32_t figure_counter_count(const Payload& payload) {
    if (std::holds_alternative<Workshop>(payload) ||
        std::holds_alternative<WeaponsWorkshop>(payload) || std::holds_alternative<Farm>(payload)) {
        return 1;
    }
    if (const auto* s = std::get_if<SmallFarm>(&payload)) {
        return static_cast<std::uint32_t>(s->figures_out.size());
    }
    return 0;
}

std::uint32_t* output_store(Payload& payload, std::uint32_t out_index) {
    if (auto* w = std::get_if<Workshop>(&payload)) {
        return out_index < w->out.size() ? &w->out[out_index] : nullptr;
    }
    if (auto* w = std::get_if<WeaponsWorkshop>(&payload)) {
        return out_index < w->out.size() ? &w->out[out_index] : nullptr;
    }
    if (auto* s = std::get_if<SmallFarm>(&payload)) {
        return out_index < s->out.size() ? &s->out[out_index] : nullptr;
    }
    if (auto* f = std::get_if<Farm>(&payload)) {
        return out_index == 0 ? &f->out : nullptr;
    }
    if (auto* m = std::get_if<Mine>(&payload)) {
        return out_index == 0 ? &m->out : nullptr;
    }
    return nullptr;
}

bool payload_blocked(const Payload& payload) {
    return std::visit([](const auto& alternative) { return alternative.blocked; }, payload);
}

Building* find_building_mut(Island& island, BuildingId id) {
    const auto it = island.cache.by_id.find(id);
    if (it == island.cache.by_id.end()) {
        return nullptr;
    }
    const BuildingKey& key = it->second;
    City* c = island.city(key.city.city);
    if (c == nullptr || key.list >= c->index.lists.size()) {
        return nullptr;
    }
    BuildingList& list = c->index.lists[key.list];
    if (key.slot >= list.slots.size() || !list.slots[key.slot].has_value() ||
        list.slots[key.slot]->id != id) {
        return nullptr;
    }
    return &*list.slots[key.slot];
}

FixedPos spawn_position(const Building& b) {
    // Tile coordinates are inside the world (at most kWorldEdgeBound = 2^14), so the
    // Q24.8 value has at most 22 bits.
    return FixedPos{static_cast<std::int32_t>(std::int64_t{b.x} * 256),
                    static_cast<std::int32_t>(std::int64_t{b.y} * 256)};
}

void tick_building(const TickContext& ctx, Island& island, City& city, Building& b, Effects& fx) {
    const BuildingType* type = ctx.types.find(b.type);
    if (type == nullptr) {
        return;
    }
    const ClassInfo& ci = ctx.types.class_info(type->cls);
    if (!tick_kind_enabled(ci.tick)) {
        return;
    }
    // StatusTracker::shows_icons is derived from the owner (OQ-033): refreshed
    // before every step, not part of the state stream.
    const bool icons = ctx.players.volk(city.owner) == 1;
    StepResult r;
    switch (ci.tick) {
    case TickKind::Workshop: {
        auto* w = std::get_if<Workshop>(&b.payload);
        if (w == nullptr) {
            return;
        }
        w->status.shows_icons = icons;
        r = workshop_step(*w, type->prod, ctx.t);
        break;
    }
    case TickKind::Farm: {
        auto* f = std::get_if<Farm>(&b.payload);
        if (f == nullptr) {
            return;
        }
        f->status.shows_icons = icons;
        r = farm_step(*f, type->prod, ctx.t);
        break;
    }
    case TickKind::None:
    case TickKind::WeaponsWorkshop:
    case TickKind::FurTrapper:
    case TickKind::Mine:
    case TickKind::SmallFarm:
        return; // masked by tick_kind_enabled
    }
    consume(ctx, island, city, b, *type, r, fx);
}

namespace {

/// The job code of a figure a class sends through send_figure: 0x0d for the fur
/// trapper (D: job table, docs/sim/state.md §4.9), 0 (unknown, O-8) for the rest.
constexpr std::uint32_t kJobFurTrapper = 0x0d;
/// The small-farm animal job (D: 0x2e).
constexpr std::uint32_t kJobSmallFarmAnimal = 0x2e;

SpawnRequest spawn_request(const Island& island, const City& city, const Building& b,
                           std::uint32_t home_counter, std::uint32_t job,
                           std::uint32_t figure_kind) {
    SpawnRequest s;
    s.home = b.id;
    s.city = CityRef{island.index, city.number};
    s.home_counter = home_counter;
    s.job = job;
    s.figure_kind = figure_kind;
    s.owner = city.owner;
    s.pos = spawn_position(b);
    return s;
}

SimEvent ui_event(SimEventKind kind, const TickContext& ctx, const City& city, const Building& b,
                  std::uint32_t code) {
    SimEvent e;
    e.kind = kind;
    e.tick = ctx.tick;
    e.player = city.owner;
    e.building = b.id;
    e.code = code;
    return e;
}

} // namespace

void consume(const TickContext& ctx, const Island& island, const City& city, Building& b,
             const BuildingType& type, const StepResult& r, Effects& fx) {
    if (r.blocked_changed) {
        SimEvent e = ui_event(SimEventKind::BlockedChanged, ctx, city, b,
                              payload_blocked(b.payload) ? 1U : 0U);
        e.x = b.x;
        e.y = b.y;
        fx.ui.push_back(e);
    }
    if (r.message) {
        fx.ui.push_back(ui_event(SimEventKind::StatusMessage, ctx, city, b,
                                 static_cast<std::uint32_t>(r.status)));
    }
    if (r.notice != 0) {
        fx.ui.push_back(ui_event(SimEventKind::Notice, ctx, city, b, r.notice));
    }
    // r.made: nothing; the output stays in the building's store.
    if (r.send_figure) {
        std::uint32_t* counter = figure_counter(b.payload, 0);
        if (counter != nullptr) {
            ++*counter;
            const std::uint32_t kind = type.prod.figure_ids.empty() ? 0 : type.prod.figure_ids[0];
            const std::uint32_t job =
                type.cls == RuntimeClass::FurTrapper ? kJobFurTrapper : std::uint32_t{0};
            fx.spawns.push_back(spawn_request(island, city, b, 0, job, kind));
        }
    }
    if (r.send_animal) {
        const std::uint16_t base = ctx.cfg.small_farm_animals.base;
        // F21: a kind below the base or beyond the counters sends nothing.
        if (r.animal_kind >= base) {
            const std::uint32_t index = static_cast<std::uint32_t>(r.animal_kind - base);
            std::uint32_t* counter = figure_counter(b.payload, index);
            if (counter != nullptr && std::holds_alternative<SmallFarm>(b.payload)) {
                ++*counter;
                fx.spawns.push_back(
                    spawn_request(island, city, b, index, kJobSmallFarmAnimal, r.animal_kind));
            }
        }
    }
}

} // namespace anno::sim
