// The driver: tick_building and consume (docs/sim/state.md §7.1, §7.2; stages S7 and
// S11).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/inn.hpp>
#include <anno/sim/mine_source.hpp>
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
        if (auto* i = std::get_if<Inn>(&payload)) {
            return &i->carriers_out;
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
        if (const auto* i = std::get_if<Inn>(&payload)) {
            return &i->carriers_out;
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
        std::holds_alternative<WeaponsWorkshop>(payload) || std::holds_alternative<Farm>(payload) ||
        std::holds_alternative<Inn>(payload)) {
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

std::uint32_t input_slot_for(const BuildingType& type, GoodsId goods) {
    return type.input_goods.size() > 1 && goods == type.input_goods[1] ? 1U : 0U;
}

std::uint32_t harvest_credit_slot(const Payload& payload, const BuildingType& type, GoodsId goods) {
    return std::holds_alternative<SmallFarm>(payload) ? input_slot_for(type, goods) : 0U;
}

bool payload_blocked(const Payload& payload) {
    return std::visit(
        [](const auto& alternative) {
            if constexpr (std::is_same_v<std::decay_t<decltype(alternative)>, House>) {
                return false; // a house is never blocked
            } else {
                return alternative.blocked;
            }
        },
        payload);
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
    case TickKind::House:
        // MAP-POP-LIVE-1 L4: the complete live update behind Rules::house_updates (default off:
        // P1's inert house, `last` untouched, like None). Serial path: house_tick.hpp.
        if (ctx.rules.house_updates && fx.faults.empty()) {
            house_tick(ctx, island, city, b, *type, fx);
        }
        return;
    case TickKind::Inn: {
        // MAP-POP-CLOSE-2 K1 (D 0x100755f0): behind Rules::house_updates like the house (default
        // off: the Inn is inert, `last` untouched). The limit is the type's max figures of group 0
        // (+0xb8(0)); a type without production data sends nothing.
        auto* inn = std::get_if<Inn>(&b.payload);
        if (inn == nullptr || !ctx.rules.house_updates || !fx.faults.empty()) {
            return;
        }
        const std::int32_t limit = type->has_prod && !type->prod.max_figures.empty()
                                       ? static_cast<std::int32_t>(type->prod.max_figures[0])
                                       : 0;
        r.send_figure = inn_step(*inn, ctx.t, limit).send;
        break;
    }
    case TickKind::Workshop: {
        auto* w = std::get_if<Workshop>(&b.payload);
        if (w == nullptr) {
            return;
        }
        w->status.shows_icons = icons;
        std::vector<std::uint32_t> in_before,
            out_before; // diagnostic ledger only: no copy without a sink
        if (ctx.transport_stats != nullptr) {
            in_before = w->in;
            out_before = w->out;
        }
        r = workshop_step(*w, type->prod, ctx.t);
        if (ctx.transport_stats != nullptr && r.stepped) {
            // MAP-NEEDS-2 N3 ledger (diagnostic, never state): what this step consumed of each
            // input and made of each output.
            ForestryStats::WorkshopLedger& l = ctx.transport_stats->forestry.workshops[b.id];
            l.consumed_88.resize(in_before.size());
            l.made_88.resize(out_before.size());
            for (std::size_t i = 0; i < in_before.size() && i < w->in.size(); ++i) {
                l.consumed_88[i] += in_before[i] - w->in[i];
            }
            for (std::size_t i = 0; i < out_before.size() && i < w->out.size(); ++i) {
                l.made_88[i] += w->out[i] - out_before[i];
            }
        }
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
        const std::uint32_t raw_before = f->raw;
        const std::uint32_t out_before = f->out;
        r = farm_step(*f, type->prod, ctx.t);
        if (ctx.transport_stats != nullptr && r.stepped) {
            // MAP-ECON-1 E4 ledger (diagnostic): what this step consumed and made.
            ForestryStats::FarmLedger& l = ctx.transport_stats->forestry.farms[b.id];
            l.consumed_88 += raw_before - f->raw;
            l.made_88 += f->out - out_before;
        }
        // MAP-ECON-1 E2: one world draw per ELIGIBLE dispatch attempt, before the send and whatever
        // the send then does (D 0x1006edf3-0x1006ee06: city +0x18, island +0x18, world +0xb4 =
        // 0x100a1740, the result discarded; docs/sim/forestry.md 3.1). farm_step's send_figure is
        // exactly that eligibility (gate, switched on, not suspended, the count and rate tests).
        if (r.send_figure) {
            static_cast<void>(ctx.draw.next(DrawSite::FarmWorkerDispatch));
        }
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
        const std::uint32_t mine_out_before = m->out;
        // Stream 137: the re-lookup of a spent or short deposit searches every ore object of the
        // island (D 0x100787c0) and materialises the deposit it binds, like the attach lookup.
        struct Relook {
            Island& island;
            const TypeRegistry& types;
            const Building& b;
            const BuildingType& type;
            bool deep;
            OreLookupStats* stats;
        } relook{
            island,  ctx.types,
            b,       *type,
            m->deep, ctx.transport_stats != nullptr ? &ctx.transport_stats->ore_lookup : nullptr};
        const MineRelookup relookup{
            [](void* p) -> std::optional<std::size_t> {
                const Relook& rl = *static_cast<const Relook*>(p);
                if (const auto bound =
                        bind_mine_deposit(rl.island, rl.types, rl.b, rl.type, rl.deep)) {
                    return bound;
                }
                if (rl.stats != nullptr) {
                    ++rl.stats->relookup_fallbacks; // diagnostic only; bind's own counters stay out
                }
                // No ore object in reach. A world whose deposits carry no ore object (a synthetic
                // fixture) keeps the earlier search among its deposits; on original data every
                // deposit comes from an ore object, so this finds nothing the lookup missed.
                return find_deposit(rl.island.deposits,
                                    rl.type.prod.outputs.empty() ? std::string()
                                                                 : rl.type.prod.outputs[0],
                                    rl.deep, mine_area(rl.b, rl.type));
            },
            &relook};
        r = mine_step(*m, type->prod, ctx.t, island.deposits, mine_area(b, *type),
                      ctx.settings.depletes(), &relookup);
        if (ctx.transport_stats != nullptr && r.stepped) {
            // MAP-EXTRACT-1 X3 ledger (diagnostic, never state): what this step made.
            ctx.transport_stats->forestry.mine_made_88[b.id] += m->out - mine_out_before;
        }
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
        std::vector<std::uint32_t> raw_before,
            out_before; // the diagnostic ledger only (no sink: no copy)
        if (ctx.transport_stats != nullptr) {
            raw_before = f->raw;
            out_before = f->out;
        }
        r = small_farm_step(*f, type->prod, ctx.t, DrawRef::of(site), ctx.cfg.small_farm_animals);
        if (ctx.transport_stats != nullptr && r.stepped) {
            // MAP-SUPPLY-1 S4 ledger (diagnostic): per line, what this step used and made.
            ForestryStats::SmallFarmLedger& l = ctx.transport_stats->forestry.small_farms[b.id];
            for (std::size_t line = 0; line < 2 && line < raw_before.size() && line < f->raw.size();
                 ++line) {
                l.consumed_88[line] += raw_before[line] - f->raw[line];
                l.made_88[line] += f->out[line] - out_before[line];
            }
        }
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
/// The farm family's worker takes its job from the figure type's slot +0x54 = its ROUTETYPE
/// (FigureType::route, MAP-ECON-1 E2, docs/re/forestry.md 2.1, 2.2), see consume.
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
    case RuntimeClass::Inn: // D 0x10075691-0x1007569f: the constant 0x33
        return kJobWorkshopCarrier;
    case RuntimeClass::FurTrapper:
        return kJobFurTrapper;
    case RuntimeClass::SmallFarm:
        return kJobSmallFarmWorker;
    case RuntimeClass::Market:
        return kJobMarketCart;
    default:
        return 0; // the farm family: the figure type's route (consume); others send no figure here
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
        const std::uint32_t first_kind = type.prod.figure_ids.empty() ? 0 : type.prod.figure_ids[0];
        if (counter != nullptr && ctx.types.class_info(type.cls).tick == TickKind::Farm) {
            // MAP-NEEDS-2 N2: a route the engine does not implement is refused explicitly (no
            // figure, no counter, no cargo); the dispatch draw was already taken by the tick (it is
            // not conditional on the send).
            const FigureType* rt = ctx.types.find_figure_type(first_kind);
            if (rt != nullptr && farm_figure_route(rt->route) == FarmFigureRoute::Unsupported) {
                if (ctx.transport_stats != nullptr) {
                    ++ctx.transport_stats->forestry.unsupported_routes;
                }
                counter = nullptr;
            }
        }
        if (counter != nullptr) {
            ++*counter;
            const std::uint32_t kind = first_kind;
            SpawnRequest s =
                spawn_request(island, city, b, type, 0, send_figure_job(type.cls), kind);
            // The farm family is the class whose tick is the farm tick (the one test used for the
            // dispatch draw in tick_building too, so a class table cannot split the two).
            if (ctx.types.class_info(type.cls).tick == TickKind::Farm) {
                // MAP-ECON-1 E2 (D 0x1006ee06-0x1006ee56): the job is the figure type's slot +0x54
                // (its ROUTETYPE), and the new figure's cargo carries the farm's input 0. A harvest
                // job (0x29/0x2a worker, 0x2e animal: N2) of a Farm-payload home gets the harvest
                // state (docs/sim/forestry.md 2: chosen by the spawn, never by the job alone). An
                // unknown figure type has no route: job 0, as before.
                const FigureType* ft = ctx.types.find_figure_type(kind);
                s.job = ft != nullptr ? ft->route : 0;
                s.cargo_goods = type.input_goods.empty() ? GoodsId{0} : type.input_goods[0];
                s.harvest = (farm_figure_route(s.job) == FarmFigureRoute::Worker ||
                             farm_figure_route(s.job) == FarmFigureRoute::Animal) &&
                            std::holds_alternative<Farm>(b.payload);
            }
            if (std::holds_alternative<SmallFarm>(b.payload)) {
                // MAP-SUPPLY-1 S2 (D 0x1006f78d-0x1006f7c2): the small farm passes the literal job
                // 0x2a (not the figure type's route) and sets the new figure's cargo goods to its
                // input 0; the 0x2a body is the shared harvest job
                // (docs/sim/small-farm-supply.md 2.3).
                s.cargo_goods = type.input_goods.empty() ? GoodsId{0} : type.input_goods[0];
                s.harvest = harvest_job(s.job);
            }
            if (type.cls == RuntimeClass::Inn) {
                // D 0x100756a9-0x100756c3: the new figure's cargo goods byte is the Inn's input 0
                // (type +0xb0(0)); the amount is not touched.
                s.cargo_goods = type.input_goods.empty() ? GoodsId{0} : type.input_goods[0];
            }
            fx.spawns.push_back(s);
        }
    }
    if (r.send_animal) {
        const std::uint16_t base = ctx.cfg.small_farm_animals.base;
        // F21: a kind below the base or beyond the counters sends nothing.
        if (r.animal_kind >= base) {
            const std::uint32_t index = static_cast<std::uint32_t>(r.animal_kind - base);
            std::uint32_t* counter = figure_counter(b.payload, index);
            // A small-farm type without input goods 1 has no animal line to feed: no animal is sent
            // (the original reads type+0xb0(1) unchecked; there is no invented goods fallback
            // here).
            if (counter != nullptr && std::holds_alternative<SmallFarm>(b.payload) &&
                type.input_goods.size() > 1) {
                ++*counter;
                SpawnRequest s =
                    spawn_request(island, city, b, type, index, kJobSmallFarmAnimal, r.animal_kind);
                // D 0x1006f8ed-0x1006f920: the animal's cargo goods are the farm's input goods 1
                // (type+0xb0(1)); job 0x2e is the shared harvest job with the animal search limit.
                s.cargo_goods = type.input_goods[1];
                s.harvest = harvest_state_job(s.job);
                fx.spawns.push_back(s);
            }
        }
    }
}

} // namespace anno::sim
