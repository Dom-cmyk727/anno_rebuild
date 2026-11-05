// The resident fetch job 0x35 (MAP-POP-LIVE-1 L2; docs/sim/population-live.md sections 1 step 3, 2,
// 3). Integer only. The body draws nothing; the dispatch draws exactly once.
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <variant>
#include <vector>

#include <anno/sim/driver.hpp>
#include <anno/sim/resident_job.hpp>
#include <anno/sim/transport.hpp>
#include <anno/sim/walk_search.hpp>

namespace anno::sim {

namespace {

/// The building's city (its BuildingKey), or nullopt.
std::optional<CityRef> city_of(const SerialWorld& world, BuildingId id) {
    const Island* is = world.island(island_of(id));
    if (is == nullptr) {
        return std::nullopt;
    }
    const auto it = is->cache.by_id.find(id);
    if (it == is->cache.by_id.end()) {
        return std::nullopt;
    }
    return it->second.city;
}

/// The cargo of the house's walkers other than `self` (the skip rule of the choice looks at every
/// walker of the house; the caller passes its own cargo separately).
std::vector<ResidentCargo> others_cargo(const SerialWorld& world, const House& house,
                                        FigureId self) {
    std::vector<ResidentCargo> out;
    for (const FigureId id : house.walkers) {
        if (!id.valid() || id == self) {
            continue;
        }
        const Figure* f = world.find_figure(id);
        if (f != nullptr && f->resident.has_value()) {
            out.insert(out.end(), f->resident->cargo.begin(), f->resident->cargo.end());
        }
    }
    return out;
}

SupplierOffer offer_of(const SupplierModel& m, const std::vector<std::uint32_t>& store,
                       ResidentFault& fault) {
    SupplierOffer o;
    for (int k = 0; k < 2; ++k) {
        o.goods[k] = m.out[k];
        if (m.out[k] == 0) {
            continue;
        }
        const AvailResult a = supplier_available(m, store, m.out[k]);
        if (a.fault != ResidentFault::None) {
            fault = a.fault;
        }
        o.avail[k] = a.amount;
        o.enabled[k] = supplier_enabled(m, m.out[k]);
    }
    return o;
}

/// P-1/P-3 (population-live.md section 19): a typed fault of a choice, bill or add is recorded in
/// the effect buffer (the world stops at the merge) and counted for the diagnostics; it is never a
/// silent skip.
void report_fault(const TickContext& ctx, Effects& fx, ResidentFault fault, BuildingId building,
                  const Figure* figure, GoodsId goods) {
    if (ctx.transport_stats != nullptr) {
        ++ctx.transport_stats->residents.faults;
    }
    FaultRecord rec;
    rec.resident = fault;
    rec.building = building;
    if (figure != nullptr) {
        rec.figure = figure->id;
    }
    rec.goods = goods;
    fx.faults.push_back(rec);
}

} // namespace

bool resident_runs_job(std::uint32_t job) {
    return job == kJobResident;
}

std::uint32_t resident_stand_mask(const ListLayout& layout) {
    std::uint32_t mask = 0;
    for (const ListId id : {layout.roles.stall_list, layout.roles.public_list}) {
        if (id < layout.lists.size()) {
            mask |= layout.lists[id].lookup_mask;
        }
    }
    return mask;
}

std::optional<HouseFacts> house_facts_of(const SimConfig& cfg, const TypeRegistry& types,
                                         const BuildingType& type, const House& house) {
    const auto group = house_group(cfg, type.prodtype);
    const auto groups = types.house_groups();
    const auto derived = types.derived_groups();
    if (!group || *group >= groups.size() || *group >= derived.size()) {
        return std::nullopt;
    }
    HouseFacts f;
    f.needs = groups[*group].needs;
    f.lookup = derived[*group].lookup;
    f.goods = types.goods_table();
    f.e = house.e;
    f.residents_64 = house.residents_64;
    f.stock = house.stock;
    return f;
}

SupplierModel supplier_model_of(const BuildingType& type, const Building* building = nullptr) {
    SupplierModel m;
    m.cls = type.cls;
    // The Inn offers its tavern stock (D +0xa4 = 0x10075940 returns the u16 +0x18).
    if (const auto* inn = building != nullptr ? std::get_if<Inn>(&building->payload) : nullptr) {
        m.tavern_stock = inn->tavern;
    }
    for (std::size_t k = 0; k < 2; ++k) {
        m.out[k] = k < type.output_goods.size() ? type.output_goods[k] : GoodsId{0};
    }
    m.inn_input = type.input_goods.empty() ? GoodsId{0} : type.input_goods[0];
    return m;
}

std::uint32_t resident_walkers_used(const House& house) {
    return static_cast<std::uint32_t>(std::count_if(house.walkers.begin(), house.walkers.end(),
                                                    [](FigureId id) { return id.valid(); }));
}

ResidentDispatch resident_dispatch(DrawSource& draw, const Island& island, const City& city,
                                   const Building& house, const BuildingType& type,
                                   const TypeRegistry& types, std::int64_t lowest,
                                   std::uint32_t walkers_used) {
    ResidentDispatch d;
    const HouseWalkerFacts facts = house_walker_facts(type);
    // A group 0 without figure types never spawns (10a, P): it takes the path of a full house.
    d.condition =
        lowest < kWalkerSpawnBelow && walkers_used < facts.walker_max && facts.variants > 0;
    if (!d.condition) {
        return d;
    }
    // D 0x10076397: the draw happens before the send and whatever the send does.
    d.draw = draw.next(DrawSite::HouseWalkerTarget);
    d.drew = true;
    // k = ((r & 1) + y + x + serial) mod variants, unsigned; x, y, serial are the u16 fields.
    const std::uint32_t sum = (d.draw & 1U) + (static_cast<std::uint32_t>(house.y) & 0xffffU) +
                              (static_cast<std::uint32_t>(house.x) & 0xffffU) +
                              (serial_of(house.id) & 0xffffU);
    d.variant = sum % facts.variants;
    const std::uint32_t kind =
        d.variant < facts.figure_ids.size() ? facts.figure_ids[d.variant] : 0;
    if (kind == 0 || types.find_figure_type(kind) == nullptr) {
        d.refusal = ResidentRefusal::NoFigureType;
        return d;
    }
    if (type.entrances.empty()) {
        d.refusal = ResidentRefusal::NoEntrance; // flag bits 4/5 read as clear (not modelled)
        return d;
    }
    const House* h = std::get_if<House>(&house.payload);
    std::uint32_t slot = 0;
    while (h != nullptr && slot < h->walkers.size() && h->walkers[slot].valid()) {
        ++slot;
    }
    if (h == nullptr || slot >= h->walkers.size()) {
        d.refusal = ResidentRefusal::NoFreeSlot;
        return d;
    }
    SpawnRequest s;
    s.home = house.id;
    s.city = CityRef{island.index, city.number};
    s.home_counter = slot;
    s.job = kJobResident;
    s.figure_kind = kind;
    s.owner = city.owner;
    s.pos = spawn_position(house, type);
    s.resident = true;
    d.spawn = s;
    return d;
}

std::optional<StandChoice> choose_stand(const SerialWorld& world, const TickContext& ctx,
                                        const Figure* walker, const Building& home,
                                        const BuildingType& home_type, CityRef city,
                                        std::uint32_t layer) {
    const Island* island = world.island(city.island);
    const City* c = world.city(city);
    const House* house = std::get_if<House>(&home.payload);
    if (island == nullptr || c == nullptr || house == nullptr) {
        return std::nullopt;
    }
    const std::optional<HouseFacts> facts = house_facts_of(ctx.cfg, ctx.types, home_type, *house);
    if (!facts) {
        return std::nullopt;
    }
    HouseWork* work =
        ctx.transport_stats != nullptr ? &ctx.transport_stats->population.houses[home.id] : nullptr;
    if (work != nullptr) {
        ++work->searches;
    }
    const SearchArea area = search_area(home, home_type);
    // Candidates: the city's buildings of the stand lists in the house's square whose choice is at
    // least the gate (D: choice >= 12), scored once.
    struct Candidate {
        const Building* b;
        std::int64_t score;
        GoodsId goods;
        bool scored = false;
    };
    std::vector<Candidate> candidates;
    std::vector<BuildingId> ids;
    world.rect_query(city, area.square(), resident_stand_mask(world.layout()), ids);
    if (work != nullptr) {
        ++work->rect_queries;
        work->rect_hits += ids.size();
    }
    const std::vector<ResidentCargo> others = walker != nullptr
                                                  ? others_cargo(world, *house, walker->id)
                                                  : others_cargo(world, *house, FigureId{});
    const std::span<const ResidentCargo> own =
        walker != nullptr && walker->resident.has_value()
            ? std::span<const ResidentCargo>(walker->resident->cargo)
            : std::span<const ResidentCargo>();
    for (const BuildingId id : ids) {
        const Building* b = world.find_building(id);
        const BuildingType* t = b != nullptr ? ctx.types.find(b->type) : nullptr;
        if (t == nullptr || b->id == home.id) {
            continue;
        }
        const SupplierModel m = supplier_model_of(*t, b);
        ResidentFault fault = ResidentFault::None;
        const SupplierOffer offer = offer_of(m, c->stock.stock, fault);
        if (fault != ResidentFault::None) {
            report_fault(ctx, world.fx, fault, b->id, walker, 0);
            continue;
        }
        const ChoiceResult r = house_choice(*facts, offer, own, others);
        if (work != nullptr) {
            ++work->candidates_scored;
        }
        if (r.fault != ResidentFault::None) {
            report_fault(ctx, world.fx, r.fault, b->id, walker, 0);
            continue;
        }
        if (r.score >= kPurchaseScoreGate) {
            candidates.push_back(Candidate{b, r.score, r.goods});
            if (work != nullptr) {
                ++work->candidates_accepted;
            }
        }
    }
    if (candidates.empty()) {
        return std::nullopt;
    }
    const auto& tables = ctx.types.walk_tables();
    if (!tables) {
        return std::nullopt;
    }
    const std::vector<WalkPoint> starts = entrance_walk_points(home, home_type);
    struct Goal {
        WalkPoint point;
        std::size_t candidate = 0;
    };
    std::vector<Goal> mapped;
    std::vector<WalkPoint> goals;
    for (std::size_t k = 0; k < candidates.size(); ++k) {
        const BuildingType* t = ctx.types.find(candidates[k].b->type);
        for (const WalkPoint& p : entrance_walk_points(*candidates[k].b, *t)) {
            const auto dup = std::find_if(mapped.begin(), mapped.end(),
                                          [&](const Goal& g) { return g.point == p; });
            if (dup == mapped.end()) {
                mapped.push_back(Goal{p, k});
                goals.push_back(p);
            }
        }
    }
    WalkCells cells(*island, ctx.types, *tables, area.square(), 4, area);
    std::int64_t best_score = -1;
    std::optional<std::size_t> best;
    const SearchResult found =
        search(cells, *tables, layer, starts, goals, [&](const SearchVisit& v) {
            const auto goal = std::find_if(mapped.begin(), mapped.end(),
                                           [&](const Goal& g) { return g.point == v.point; });
            if (goal == mapped.end()) {
                return SearchDecision{};
            }
            Candidate& cand = candidates[goal->candidate];
            if (cand.scored) {
                return SearchDecision{}; // its further entrances score the same
            }
            cand.scored = true;
            // The STRICTLY highest score wins; the first discovery keeps a tie (not the nearest).
            if (cand.score > best_score) {
                best_score = cand.score;
                best = goal->candidate;
                return SearchDecision{GoalAction::Expand, true};
            }
            return SearchDecision{GoalAction::Expand, false};
        });
    if (work != nullptr) {
        ++work->walk_searches;
        work->walk_expansions += found.popped;
    }
    if (!found.complete || !best || found.route.empty()) {
        return std::nullopt;
    }
    StandChoice choice;
    choice.building = candidates[*best].b->id;
    choice.goods = candidates[*best].goods;
    choice.score = candidates[*best].score;
    choice.path = found.route;
    if (found.selected) {
        choice.end_col = found.selected->point.col;
        choice.end_row = found.selected->point.row;
    }
    return choice;
}

namespace {

/// The reverse search of the L5 review P2-C fix, as a candidate only (MAP-POP-CLOSE-1 C3, P-15).
/// The shared path_home searches FROM the walker TO the home's entrance subcell; a blocked entrance
/// subcell is seeded as a start but never reached as a goal, and the walker's end point (the goal
/// tile's centre subcell) can be enclosed, so on real maps it failed for a large share of walkers
/// (S01_01_01: 78 of 201 arrivals). The way out already proved walkable: search FROM the home's
/// entrance points TO the walker's subcell and reverse the route. The search is DIRECTED (the exits
/// of a cell depend on the arrival direction and a constraint can clear one directed member), so the
/// reversed route is only a candidate: resident_path_home replays it forward with route_legal.
std::optional<std::vector<TileXY>>
reverse_route_home(const Island& island, const TypeRegistry& types, const Building& home,
                   const BuildingType& home_type, FixedPos from, std::uint32_t layer) {
    const SearchArea area = search_area(home, home_type);
    const auto& tables = types.walk_tables();
    if (!tables) {
        return std::nullopt;
    }
    WalkCells cells(island, types, *tables, area.square(), 4, area);
    const std::vector<WalkPoint> starts = entrance_walk_points(home, home_type);
    const std::vector<WalkPoint> goals{walk_point_of(from)};
    const SearchResult found = search(cells, *tables, layer, starts, goals, [](const SearchVisit&) {
        return SearchDecision{GoalAction::Stop, true};
    });
    if (!found.complete || !found.selected || found.route.empty()) {
        return std::nullopt;
    }
    return std::vector<TileXY>(found.route.rbegin(), found.route.rend());
}

struct Body {
    const TickContext& ctx;
    SerialWorld& world;
    Figure& fig;
    ResidentState& r;
    const FigureType& ft;
    const Building* home;
    const BuildingType* home_type;
    std::optional<CityRef> city;
    ResidentStats* stats;
    bool delivered = false;

    void reset_walk() {
        r.route.clear();
        r.next = 0;
        r.along = 0;
    }
    void finish() {
        r.wait = 0;
        r.target = BuildingId{};
        reset_walk();
        r.step = ResidentStep::Done;
    }
    /// Step C's start (D 0x10057ae0): inside the home's footprint no walk; else the walk home, or
    /// no path: the job ends (the cargo stays with the figure).
    void start_home() {
        r.wait = 0;
        r.target = BuildingId{};
        reset_walk();
        if (home == nullptr || home_type == nullptr) {
            if (stats != nullptr) {
                ++stats->home_gone;
            }
            finish();
            return;
        }
        r.step = ResidentStep::ToHome;
        if (tile_in_footprint(*home, *home_type, tile_of_q8(fig.pos.x), tile_of_q8(fig.pos.y))) {
            return;
        }
        const Island* hi = world.island(island_of(home->id));
        ResidentPathNote note;
        const auto path =
            hi != nullptr ? resident_path_home(*hi, ctx.types, *home, *home_type, fig.pos, ft.layer, &note)
                          : std::nullopt;
        if (stats != nullptr) {
            stats->reverse_rejected += note.reverse_rejected;
            stats->endpoint_adapted += note.endpoint_adapted;
            stats->forward_used += note.forward_used;
        }
        if (!path) {
            if (stats != nullptr) {
                ++stats->no_path_home;
            }
            finish(); // no path home (0x10057bb1)
            return;
        }
        start_walk(fig.pos, *path, WalkRef{r.route, r.next, r.along});
    }

    /// Step B's arrival (D 0x10056480): the building of the stand lists on the walker's tile (none:
    /// nothing), then at most two purchases against the real supplier.
    void purchase() {
        if (stats != nullptr) {
            ++stats->arrivals;
        }
        const House* house = std::get_if<House>(&home->payload);
        const Building* stand =
            city ? world.building_at(*city, tile_of_q8(fig.pos.x), tile_of_q8(fig.pos.y),
                                     resident_stand_mask(world.layout()))
                 : nullptr;
        const BuildingType* st = stand != nullptr ? ctx.types.find(stand->type) : nullptr;
        const City* c = city ? world.city(*city) : nullptr;
        const std::optional<HouseFacts> facts =
            house != nullptr ? house_facts_of(ctx.cfg, ctx.types, *home_type, *house)
                             : std::nullopt;
        if (st == nullptr || c == nullptr || !facts) {
            if (stats != nullptr) {
                ++stats->arrivals_empty;
            }
            return;
        }
        SupplierModel model = supplier_model_of(*st, stand);
        const std::uint32_t tavern_before = model.tavern_stock;
        std::vector<std::uint32_t> store = c->stock.stock;
        std::vector<std::uint32_t> billed = c->billed;
        BillLedger ledger{store, billed, ctx.cfg.billed_goods_first, ctx.cfg.billed_goods_last};
        const std::vector<ResidentCargo> others = others_cargo(world, *house, fig.id);
        const std::vector<ResidentCargo> cargo_before = r.cargo;
        const BuyResult res = resident_buy(*facts, model, ledger, ft.capacity_88, others, r.cargo);
        if (stats != nullptr) {
            stats->purchases += res.purchases;
            // The per-goods ledger (L5): what each slot gained.
            for (std::size_t k = 0; k < r.cargo.size() && k < cargo_before.size(); ++k) {
                const ResidentCargo& a = r.cargo[k];
                const ResidentCargo& b0 = cargo_before[k];
                if (a.goods == 0) {
                    continue;
                }
                const std::uint32_t was = b0.goods == a.goods ? b0.amount_88 : 0U;
                if (a.amount_88 > was) {
                    stats->bought_by_goods[a.goods] += a.amount_88 - was;
                }
            }
        }
        if (res.fault != ResidentFault::None) {
            report_fault(ctx, world.fx, res.fault, stand->id, &fig, 0);
        }
        if (model.cls == RuntimeClass::Inn && model.tavern_stock < tavern_before) {
            world.fx.tavern_debits.push_back(
                TavernDebit{stand->id, tavern_before - model.tavern_stock});
        }
        // The debit and the billed counters go out as deltas, merged right after this call.
        for (std::size_t g = 0; g < store.size() && g < c->stock.stock.size(); ++g) {
            if (store[g] < c->stock.stock[g]) {
                const std::uint32_t taken = c->stock.stock[g] - store[g];
                world.fx.goods_moves.push_back(
                    GoodsMove{*city, static_cast<GoodsId>(g), -std::int64_t{taken}, true});
                if (stats != nullptr) {
                    stats->bought_88 += taken;
                    stats->taken_by_goods[static_cast<GoodsId>(g)] += taken;
                }
            }
        }
        for (std::size_t g = 0; g < billed.size() && g < c->billed.size(); ++g) {
            if (billed[g] > c->billed[g]) {
                world.fx.billed_deltas.push_back(
                    BilledDelta{*city, static_cast<GoodsId>(g), billed[g] - c->billed[g]});
                if (stats != nullptr) {
                    stats->billed_by_goods[static_cast<GoodsId>(g)] += billed[g] - c->billed[g];
                }
            }
        }
    }

    /// Step C's arrival (D 0x10057d70): every non-empty cargo slot goes to the home house.
    void deliver() {
        if (home == nullptr) {
            if (stats != nullptr) {
                ++stats->home_gone;
            }
            return;
        }
        for (const ResidentCargo& cg : r.cargo) {
            if (cg.goods == 0) {
                continue;
            }
            world.fx.house_deliveries.push_back(
                HouseDelivery{home->id, fig.id, cg.goods, cg.amount_88});
            if (stats != nullptr) {
                ++stats->deliveries;
                stats->delivered_88 += cg.amount_88;
                stats->delivered_by_goods[cg.goods] += cg.amount_88;
            }
        }
        delivered = true;
    }
};

} // namespace

RouteCheck resident_route_legal(const Island& island, const TypeRegistry& types,
                                const Building& home, const BuildingType& home_type, FixedPos from,
                                std::uint32_t layer, std::span<const TileXY> route,
                                bool endpoint_adapter) {
    const SearchArea area = search_area(home, home_type);
    const auto& tables = types.walk_tables();
    if (!tables) {
        RouteCheck c;
        c.reason = "no walk tables";
        return c;
    }
    WalkCells cells(island, types, *tables, area.square(), 4, area);
    // The adapter is scoped (C3 review P3-1): the first step is exempt only when the walker's tile
    // really holds a building (a stand it leaves), the last only when the route's last tile is an
    // entrance tile of the home. A route that does not end on an entrance tile is never legal here.
    bool leave_exempt = false;
    if (endpoint_adapter && !route.empty()) {
        const TileXY t = route.front();
        if (const Slot idx = island.tile_index(t.x, t.y); idx != kNoSlot) {
            const BuildingType* ty = types.find(TypeId{island.tiles.object(idx)});
            leave_exempt = ty != nullptr && ty->walk_class == 2;
        }
    }
    bool ends_at_entrance = false;
    if (!route.empty()) {
        for (const WalkPoint& p : entrance_walk_points(home, home_type)) {
            ends_at_entrance = ends_at_entrance || p.tile == route.back();
        }
    }
    RouteCheck out = route_legal(cells, *tables, layer, walk_point_of(from), route, leave_exempt,
                                 endpoint_adapter && ends_at_entrance);
    if (out.legal && !ends_at_entrance) {
        out.legal = false;
        out.bad_step = route.size();
        out.reason = "the route does not end on an entrance tile of the home";
    }
    return out;
}

std::optional<std::vector<TileXY>>
resident_path_home(const Island& island, const TypeRegistry& types, const Building& home,
                   const BuildingType& home_type, FixedPos from, std::uint32_t layer,
                   ResidentPathNote* note) {
    // P-15 (population-live.md 21.6): the candidate is the reverse search; it is used only when the
    // FORWARD replay accepts it (adjacent steps, the arrival-state moves, the edge keys and the
    // layer's admission) and it ends on one of the home's entrance tiles. Otherwise the shared
    // forward search (legal by construction) is tried; no route at all is nullopt.
    const std::vector<WalkPoint> entrances = entrance_walk_points(home, home_type);
    const auto ends_at_entrance = [&](const std::vector<TileXY>& route) {
        return !route.empty() &&
               std::any_of(entrances.begin(), entrances.end(),
                           [&](const WalkPoint& p) { return p.tile == route.back(); });
    };
    if (auto reversed = reverse_route_home(island, types, home, home_type, from, layer)) {
        const RouteCheck strict =
            resident_route_legal(island, types, home, home_type, from, layer, *reversed, false);
        if (strict.legal && ends_at_entrance(*reversed)) {
            return reversed;
        }
        // The scoped endpoint adapter: every inner step must satisfy the forward rule; only leaving
        // the stand and entering the home are exempt from it (see route_legal).
        const RouteCheck adapted =
            resident_route_legal(island, types, home, home_type, from, layer, *reversed, true);
        if (adapted.legal && ends_at_entrance(*reversed)) {
            if (note != nullptr) {
                ++note->endpoint_adapted;
            }
            return reversed;
        }
        if (note != nullptr) {
            ++note->reverse_rejected;
        }
    }
    if (auto forward = path_home(island, types, home, home_type, from, layer)) {
        if (ends_at_entrance(*forward)) {
            if (note != nullptr) {
                ++note->forward_used;
            }
            return forward;
        }
    }
    return std::nullopt;
}

bool resident_tick(const TickContext& ctx, SerialWorld& world, Figure& fig) {
    const FigureType* ft = ctx.types.find_figure_type(fig.kind);
    if (ft == nullptr || !fig.resident.has_value()) {
        return false;
    }
    const std::uint32_t t = ctx.t;
    if (fig.expiry_stamp.has_value()) {
        fig.stamp = static_cast<std::uint16_t>(t); // ended, waiting for phase C
        return true;
    }
    ResidentState& r = *fig.resident;
    std::uint64_t ticks = static_cast<std::uint16_t>(static_cast<std::uint16_t>(t) - fig.stamp);
    fig.stamp = static_cast<std::uint16_t>(t);
    const Building* home = world.find_building(fig.home);
    const BuildingType* home_type = home != nullptr ? ctx.types.find(home->type) : nullptr;
    const House* house = home != nullptr ? std::get_if<House>(&home->payload) : nullptr;
    // The job acts for its home only while the home is a House that still holds this figure's slot
    // (an old job never acts through a newer owner's slot).
    if (house == nullptr || fig.home_counter >= house->walkers.size() ||
        house->walkers[fig.home_counter] != fig.id) {
        home = nullptr;
        home_type = nullptr;
    }
    Body b{ctx,
           world,
           fig,
           r,
           *ft,
           home,
           home_type,
           home != nullptr ? city_of(world, home->id) : std::nullopt,
           ctx.transport_stats != nullptr ? &ctx.transport_stats->residents : nullptr};
    const std::uint32_t speed = ft->speed_q24;
    for (;;) {
        switch (r.step) {
        case ResidentStep::Start: {
            if (home == nullptr || !b.city) {
                b.finish();
                break;
            }
            if (b.stats != nullptr) {
                ++b.stats->searches;
            }
            const auto stand =
                choose_stand(world, ctx, &fig, *home, *home_type, *b.city, ft->layer);
            if (!stand) {
                if (b.stats != nullptr) {
                    ++b.stats->searches_failed;
                }
                b.start_home(); // step A fails -> step C
                break;
            }
            r.target = stand->building;
            start_walk(fig.pos, stand->path, WalkRef{r.route, r.next, r.along});
            // K2: the walk ends at the selected goal sub-cell, not the tile centre (see StandChoice).
            // ADAPT / U: finer-route stage 0x100054ef..0x100055d5 unread; the sub-cell formula
            // ((2c+1)*256+3)/6-128 is not tied to a read.
            if (!stand->path.empty() && r.route.size() >= 2) {
                const FixedPos tile = tile_centre(stand->path.back());
                const auto sub = [](std::uint32_t c) {
                    return static_cast<std::int32_t>(((2 * c + 1) * 256 + 3) / 6) - 128;
                };
                r.route.back() = FixedPos{tile.x + sub(stand->end_col), tile.y + sub(stand->end_row)};
            }
            r.step = ResidentStep::ToStall;
            break;
        }
        case ResidentStep::ToStall: {
            const auto rest = walk_ticks(fig.pos, WalkRef{r.route, r.next, r.along}, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            if (home == nullptr) {
                if (b.stats != nullptr) {
                    ++b.stats->home_gone;
                }
                b.finish();
                break;
            }
            // D step B start 0x10056330: the wait is the figure type's float (type slot +0x64(1), AnnoGame
            // 0x1000fd10 = LOGIC f32 [4], 1.0 when not above 0) through figure slot +0xe0 = 0x10035260
            // (duration to +0x3c, rate 0.01 to +0x40), the cart's load wait: round(500 * d) ticks =
            // FigureType::load_ticks. The progress persists in r.wait across calls.
            r.wait = ft->load_ticks;
            r.step = ResidentStep::Wait;
            break;
        }
        case ResidentStep::Wait:
            if (ticks < r.wait) {
                r.wait = static_cast<std::uint32_t>(r.wait - ticks);
                return true;
            }
            ticks -= r.wait;
            r.wait = 0;
            if (home == nullptr) {
                if (b.stats != nullptr) {
                    ++b.stats->home_gone;
                }
                b.finish();
                break;
            }
            b.purchase();
            b.start_home(); // the job ignores the result of the arrival
            break;
        case ResidentStep::ToHome: {
            const auto rest = walk_ticks(fig.pos, WalkRef{r.route, r.next, r.along}, ticks, speed);
            if (!rest) {
                return true;
            }
            ticks = *rest;
            b.deliver();
            b.finish();
            break;
        }
        case ResidentStep::Done:
            if (!b.delivered && b.stats != nullptr) {
                for (const ResidentCargo& cg : r.cargo) {
                    b.stats->lost_88 += cg.goods != 0 ? cg.amount_88 : 0U;
                    if (cg.goods != 0) {
                        b.stats->lost_by_goods[cg.goods] += cg.amount_88;
                    }
                }
            }
            world.end_figure(fig, t); // releases the walker slot exactly once
            return true;
        }
    }
}

} // namespace anno::sim
