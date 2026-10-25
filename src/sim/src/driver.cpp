// The driver: tick_building and consume (docs/sim/state.md §7.1, §7.2; stages S7 and
// S11).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/transport.hpp>

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
        if (auto* m = std::get_if<Market>(&payload)) {
            return &m->carts_out;
        }
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
        if (const auto* m = std::get_if<Market>(&payload)) {
            return &m->carts_out;
        }
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
    if (std::holds_alternative<Market>(payload) || std::holds_alternative<Workshop>(payload) ||
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

std::uint32_t* reserved_store(Payload& payload, std::uint32_t index) {
    if (auto* w = std::get_if<Workshop>(&payload)) {
        return index < w->reserved.size() ? &w->reserved[index] : nullptr;
    }
    if (auto* w = std::get_if<WeaponsWorkshop>(&payload)) {
        return index < w->reserved.size() ? &w->reserved[index] : nullptr;
    }
    if (auto* s = std::get_if<SmallFarm>(&payload)) {
        return index < s->reserved.size() ? &s->reserved[index] : nullptr;
    }
    if (auto* f = std::get_if<Farm>(&payload)) {
        return index == 0 ? &f->reserved : nullptr;
    }
    if (auto* m = std::get_if<Mine>(&payload)) {
        return index == 0 ? &m->reserved : nullptr;
    }
    return nullptr;
}

std::uint32_t* input_store(Payload& payload, std::uint32_t index) {
    if (auto* w = std::get_if<Workshop>(&payload)) {
        return index < w->in.size() ? &w->in[index] : nullptr;
    }
    if (auto* w = std::get_if<WeaponsWorkshop>(&payload)) {
        return index < w->in.size() ? &w->in[index] : nullptr;
    }
    if (auto* s = std::get_if<SmallFarm>(&payload)) {
        return index < s->raw.size() ? &s->raw[index] : nullptr;
    }
    if (auto* f = std::get_if<Farm>(&payload)) {
        return index == 0 ? &f->raw : nullptr;
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

FixedPos spawn_position(const Building& b, const BuildingType& type) {
    // The send (D 0x10072d20) creates the figure at a reachable entrance point
    // (0x10072b50, not read: likely the first walkable one); the stand-in takes the
    // type's first entrance point, rotated (O-59; docs/re/transport.md §1.6). Without
    // one: the building's north-west tile (the S7 stand-in). Tile coordinates are inside
    // the world (at most kWorldEdgeBound = 2^14), so the Q24.8 value has at most 22 bits.
    if (!type.entrances.empty()) {
        return entrance_position(b, type, 0);
    }
    return FixedPos{static_cast<std::int32_t>(std::int64_t{b.x} * 256),
                    static_cast<std::int32_t>(std::int64_t{b.y} * 256)};
}

FixedPos entrance_position(const Building& b, const BuildingType& type, std::size_t i) {
    // AnnoGame 0x100084c0: rotation 1 (y, W - x), 2 (W - x, H - y), 3 (H - y, x), with W,
    // H the type's HEAD[0], HEAD[1] (table 0x10021780: a 0 reads as 1). In sixths of a
    // tile; to Q24.8 by x6 · 256 / 6 rounded to nearest (sixths are not exact in 1/256).
    const EntrancePoint p = i < type.entrances.size() ? type.entrances[i] : EntrancePoint{};
    const std::int64_t w6 = std::int64_t{std::max<std::uint32_t>(type.width, 1)} * 6;
    const std::int64_t h6 = std::int64_t{std::max<std::uint32_t>(type.height, 1)} * 6;
    std::int64_t x6 = p.x6, y6 = p.y6;
    switch (b.rotation & 3U) {
    case 1:
        x6 = p.y6;
        y6 = w6 - p.x6;
        break;
    case 2:
        x6 = w6 - p.x6;
        y6 = h6 - p.y6;
        break;
    case 3:
        x6 = h6 - p.y6;
        y6 = p.x6;
        break;
    default:
        break;
    }
    auto q = [](std::int64_t tile, std::int64_t six) {
        const std::int64_t v = tile * 256 + (six * 256 + 3) / 6;
        return static_cast<std::int32_t>(std::clamp<std::int64_t>(
            v, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()));
    };
    return FixedPos{q(b.x, x6), q(b.y, y6)};
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
    case TickKind::Market: {
        auto* m = std::get_if<Market>(&b.payload);
        if (m == nullptr) {
            return;
        }
        r = market_step(*m, type->prod, type->has_prod, ctx.t);
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
/// The carrier's 0x33 is kJobWorkshopCarrier and the market cart's 0x34 (D 0x1006b6d0:
/// `this+0x118(0x34, figure list[0], t)`, docs/re/transport.md §1.5) kJobMarketCart,
/// both in transport.hpp.
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
    case RuntimeClass::Market:
        return kJobMarketCart;
    default:
        return 0; // the farm family: unknown (O-8); other classes send no figure here
    }
}

SpawnRequest spawn_request(const Island& island, const City& city, const Building& b,
                           const BuildingType& type, std::uint32_t home_counter, std::uint32_t job,
                           std::uint32_t figure_kind) {
    SpawnRequest s;
    s.home = b.id;
    s.city = CityRef{island.index, city.number};
    s.home_counter = home_counter;
    s.job = job;
    s.figure_kind = figure_kind;
    s.owner = city.owner;
    s.pos = spawn_position(b, type);
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
            fx.spawns.push_back(
                spawn_request(island, city, b, type, 0, send_figure_job(type.cls), kind));
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
                fx.spawns.push_back(spawn_request(island, city, b, type, index, kJobSmallFarmAnimal,
                                                  r.animal_kind));
            }
        }
    }
}

} // namespace anno::sim
