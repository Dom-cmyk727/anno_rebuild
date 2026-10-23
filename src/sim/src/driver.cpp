// The driver: tick_building and consume (docs/sim/state.md §7.1, §7.2; stages S7 and
// S11).
#include <cstddef>
#include <variant>

#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>

namespace anno::sim {

void refresh_unlocked(WeaponsWorkshop& w, const BuildingType& type, const TypeRegistry& types,
                      const PlayersView& players, PlayerId owner) {
    // D: AnnoWelt.dll 0x10069580-0x10069640 (docs/re/production.md §4.1a): line j works
    // only if the requirement byte b of its output goods g = type+0xac(j) is 0 or the
    // owner's research object reports bit 1 for b; b is the goods' Forsch research id
    // (BGruppen [WAREN], OQ-034 resolved; GoodsInfo::forsch) and the bit is "researched"
    // (player +0xb0 table, ResearchState::researched). One entry per line of the
    // payload: weapons_workshop_step indexes `unlocked` by the lines of `out`, so it is
    // sized to them before it is written (it is derived, not saved: empty after a load).
    const std::size_t lines = w.out.size();
    w.unlocked.assign(lines, false);
    for (std::size_t j = 0; j < lines; ++j) {
        // A line without an output goods (beyond the type's list) names goods 0, which has
        // no requirement. The registry resolves every goods name, so goods() of a type's
        // output is never null; a null entry is read as "no requirement" like goods 0.
        const GoodsId g = j < type.output_goods.size() ? type.output_goods[j] : GoodsId{0};
        const GoodsInfo* info = types.goods(g);
        const std::uint32_t forsch = info != nullptr ? info->forsch : 0;
        // Element stores, not a braced list (the GCC 12 -Wnull-dereference false positive
        // of vector<bool>, docs/sim/state.md §15 S2).
        w.unlocked[j] = forsch == 0 || players.researched(owner, forsch);
    }
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
    // StatusTracker::shows_icons is derived from the owner (OQ-033): refreshed
    // before every step, not part of the state stream.
    const bool icons = ctx.players.volk(city.owner) == 1;
    StepResult r;
    // -Wswitch (no default): a tick kind added later is a compile error here.
    switch (ci.tick) {
    case TickKind::None:
        return; // inert: `last` untouched (D15)
    case TickKind::Workshop: {
        auto* w = std::get_if<Workshop>(&b.payload);
        if (w == nullptr) {
            return;
        }
        w->status.shows_icons = icons;
        r = workshop_step(*w, type->prod, ctx.t);
        break;
    }
    case TickKind::WeaponsWorkshop: {
        auto* w = std::get_if<WeaponsWorkshop>(&b.payload);
        if (w == nullptr) {
            return;
        }
        w->status.shows_icons = icons;
        // The research input (§7.1): derived from the owner's research state before the
        // step, every tick.
        refresh_unlocked(*w, *type, ctx.types, ctx.players, city.owner);
        r = weapons_workshop_step(*w, type->prod, ctx.t);
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
    case TickKind::FurTrapper: {
        auto* f = std::get_if<Farm>(&b.payload);
        if (f == nullptr) {
            return;
        }
        f->status.shows_icons = icons;
        r = fur_trapper_step(*f, type->prod, ctx.t);
        // D 0x1006e5d0 (docs/re/production.md §4.2a): a due trapper is sent only with a
        // target from the radius search 0x10071da0(8), the nearest animal of the input
        // goods. The island's animals are wildlife objects, which the sim does not create
        // (O-12; their fauna sweep is the unported WildlifeSweep), so the search finds
        // nothing and no trapper is sent (O-25, docs/sim/state.md §12). The step itself
        // ran in full: what the trapper made from its raw stock stays made.
        r.send_figure = false;
        break;
    }
    case TickKind::Mine: {
        auto* m = std::get_if<Mine>(&b.payload);
        if (m == nullptr) {
            return;
        }
        m->status.shows_icons = icons;
        // The island's deposits in index order (Mine::deposit is an index into them),
        // the footprint by rotation (D 0x1006ddf0) and world setting 14 bit 1, the
        // add-on edition (OQ-035 a, D 0x10063686).
        r = mine_step(*m, type->prod, ctx.t, island.deposits, mine_area(b, *type),
                      ctx.settings.depletes());
        break;
    }
    case TickKind::SmallFarm: {
        auto* f = std::get_if<SmallFarm>(&b.payload);
        if (f == nullptr) {
            return;
        }
        f->status.shows_icons = icons;
        // Line 1's animal kind is a world PRNG draw (D 0x1006f810-0x1006f838, §7.4),
        // through the advance's DrawSource as the site SmallFarmAnimal.
        SiteDraw site{ctx.draw, DrawSite::SmallFarmAnimal};
        r = small_farm_step(*f, type->prod, ctx.t, DrawRef::of(site), ctx.cfg.small_farm_animals);
        break;
    }
    }
    consume(ctx, island, city, b, *type, r, fx);
}

namespace {

/// The job codes of the figures the production classes send (docs/re/production.md §4,
/// §5.1; the D addresses below):
///  - 0x33 the carrier of a workshop (D 0x100680b0, "send a carrier (job 0x33, figure
///    type list[0])", §4.1) and of a weapons workshop, whose tick shares the workshop's
///    carrier rule (D 0x10069550, §4.1a);
///  - 0x0d the fur trapper (D 0x1006e5d0: `this+0x118(job 0x0d, ...)`, §4.2a);
///  - 0x2a the small farm's line-0 worker (D 0x1006f660: `this+0x118(job 0x2a, ...)`,
///    §4.5);
///  - 0x2e the small farm's animal (D 0x1006f660, §4.5).
/// The farm's worker takes its job from the figure type's slot +0x54 (§4.2), which is
/// not read: 0 (unknown, O-8).
constexpr std::uint32_t kJobWorkshopCarrier = 0x33;
constexpr std::uint32_t kJobFurTrapper = 0x0d;
constexpr std::uint32_t kJobSmallFarmWorker = 0x2a;
constexpr std::uint32_t kJobSmallFarmAnimal = 0x2e;

/// The job of the figure `send_figure` sends for a building of class `cls`.
std::uint32_t send_figure_job(RuntimeClass cls) {
    switch (cls) {
    case RuntimeClass::Workshop:
    case RuntimeClass::WeaponsWorkshop:
        return kJobWorkshopCarrier;
    case RuntimeClass::FurTrapper:
        return kJobFurTrapper;
    case RuntimeClass::SmallFarm:
        return kJobSmallFarmWorker;
    default:
        return 0; // the farm family: unknown (O-8); other classes send no figure here
    }
}

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
            fx.spawns.push_back(spawn_request(island, city, b, 0, send_figure_job(type.cls), kind));
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
