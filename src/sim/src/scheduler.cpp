// World::advance, the island phase and the effect merge (docs/sim/state.md §5.3–
// §5.8, §4.11, stage S7).
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include <anno/sim/apply.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/driver.hpp>
#include <anno/sim/island_tick.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/scheduler.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

// ---------------------------------------------------------------------------
// The island phase.

void advance_sweep_cursor(std::uint32_t& x, std::uint32_t& y, std::uint32_t width,
                          std::uint32_t height, std::uint32_t chunk_edge) {
    // A3: x resets on every row wrap; y wraps at the bottom of the island. u64
    // sums, so a hostile edge cannot wrap the u32.
    const std::uint64_t nx = std::uint64_t{x} + chunk_edge;
    if (nx >= width) {
        x = 0;
        const std::uint64_t ny = std::uint64_t{y} + chunk_edge;
        y = ny >= height ? 0 : static_cast<std::uint32_t>(ny);
    } else {
        x = static_cast<std::uint32_t>(nx);
    }
}

void object_manager_tick(const TickContext& ctx, Island& island, Effects& fx) {
    // Only the stamp arithmetic is D (0x10095d40); the traversal is the stand-in of
    // A10: the objects are the deposits in index order.
    const std::uint64_t n = island.deposits.size();
    const std::uint64_t count = visit_count(ctx.t, island.objmgr.stamp, n, ctx.cfg.objmgr_div);
    const Seams::IslandPass body = ctx.seams.object_manager.fn;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (body != nullptr) {
            body(ctx.seams.object_manager.ctx, ctx, island, fx);
        }
        island.objmgr.cursor = static_cast<std::uint32_t>((island.objmgr.cursor + 1U) % n);
    }
    island.objmgr.stamp = static_cast<std::uint16_t>(ctx.t);
}

void building_scheduler(const TickContext& ctx, Island& island, City& city, Effects& fx) {
    run_scheduled_lists(
        city.index.lists, ctx.cfg.layout.schedule, city.index.stamp, ctx.t, ctx.cfg.building_div,
        [&](ListId /*list*/, Building& b) { tick_building(ctx, island, city, b, fx); });
}

void city_tick(const TickContext& ctx, Island& island, City& city, Effects& fx) {
    building_scheduler(ctx, island, city, fx);
    for (const Seam<Seams::CityPass>& pass : ctx.seams.city_pass) {
        if (pass.fn != nullptr) {
            pass.fn(pass.ctx, ctx, island, city, fx);
        }
    }
    city.tick_stamp = static_cast<std::uint16_t>(ctx.t);
}

void city_ring_tick(const TickContext& ctx, Island& island, Effects& fx) {
    const std::uint64_t n = island.cities.size();
    const std::uint64_t count = visit_count(ctx.t, island.ring.stamp, n, ctx.cfg.city_ring_div);
    // A cursor at or beyond n is an inconsistent state (A8, validate_islands): nothing
    // is visited then, and the stamp is still written.
    if (island.ring.cursor < n) {
        for (std::uint64_t i = 0; i < count; ++i) {
            std::optional<City>& slot = island.cities[island.ring.cursor];
            if (slot.has_value()) {
                city_tick(ctx, island, *slot, fx);
            }
            island.ring.cursor = static_cast<std::uint32_t>((island.ring.cursor + 1U) % n);
        }
    }
    island.ring.stamp = static_cast<std::uint16_t>(ctx.t);
}

bool start_drought_at(const SimConfig& cfg, Island& island, std::uint32_t x, std::uint32_t y,
                      std::uint32_t radius, std::uint32_t t) {
    if (cfg.city_slots > 32) {
        return false;
    }
    Drought d;
    d.x = x;
    d.y = y;
    d.target_ring = radius / cfg.chunk_edge;
    d.ring = 0;
    d.city_mask = 0;
    d.phase = 0;
    d.step = 0;
    d.state = 1;
    d.counter = 0;
    d.stamp = static_cast<std::uint16_t>(t);
    island.drought = d;
    return true;
}

bool start_drought(const TickContext& ctx, Island& island) {
    if (island.drought.has_value()) {
        return false;
    }
    const ListId market = ctx.cfg.layout.roles.market_list;
    for (const std::optional<City>& city : island.cities) {
        if (!city.has_value() || market >= city->index.lists.size()) {
            continue;
        }
        for (const std::optional<Building>& b : city->index.lists[market].slots) {
            if (!b.has_value()) {
                continue;
            }
            // Tile coordinates inside the world are non-negative (validate_islands);
            // floor to the chunk edge.
            const auto bx = static_cast<std::uint32_t>(b->x);
            const auto by = static_cast<std::uint32_t>(b->y);
            return start_drought_at(ctx.cfg, island, bx - bx % ctx.cfg.chunk_edge,
                                    by - by % ctx.cfg.chunk_edge, ctx.cfg.drought_radius, ctx.t);
        }
    }
    return false;
}

void event_300s(const TickContext& ctx, Island& island, Effects& /*fx*/) {
    const std::uint32_t seconds = ctx.t / 500U;
    const auto gate = static_cast<std::uint16_t>(seconds - island.event300_stamp);
    if (gate < ctx.cfg.event300_gate_seconds) {
        return; // the stamp stays
    }
    const auto stamp = static_cast<std::uint16_t>(seconds);
    if (ctx.settings.drought_off()) {
        island.event300_stamp = stamp; // no draw at all
        return;
    }
    // EXACTLY ONE draw per gate per island, before every other test.
    const std::uint32_t r = ctx.draw.next(DrawSite::Event300);
    bool has_city = false;
    const std::uint64_t scan = std::min<std::uint64_t>(ctx.cfg.neutral_owner, island.cities.size());
    for (std::uint64_t k = 0; k < scan && !has_city; ++k) {
        has_city = island.cities[static_cast<std::size_t>(k)].has_value();
    }
    if (r % ctx.cfg.event300_modulus == 0 && island.climate != 0 && has_city) {
        static_cast<void>(start_drought(ctx, island));
    }
    island.event300_stamp = stamp;
}

void island_tick(const TickContext& ctx, Island& island, Effects& fx) {
    const std::uint32_t t = ctx.t;
    const TileLayer& tiles = island.tiles;
    const auto chunks = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(tiles.chunk_count(), std::numeric_limits<std::uint32_t>::max()));
    const auto d = static_cast<std::uint16_t>(t - island.sweep.stamp);
    // Growth sweep. The chunk visit (grow_chunk, D 0x1008b5f0) is stage S13: the
    // cursor moves exactly as it will then.
    const std::uint64_t grow_chunks = sweep_chunks(chunks, d, ctx.cfg.grow_period);
    for (std::uint64_t i = 0; i < grow_chunks; ++i) {
        advance_sweep_cursor(island.sweep.grow_x, island.sweep.grow_y, tiles.width(),
                             tiles.height(), ctx.cfg.chunk_edge);
    }
    // Fauna sweep: the seam (D: island slot +0x194 = 0x10093480; PRNG, O-12).
    const std::uint64_t fauna_chunks = sweep_chunks(chunks, d, ctx.cfg.fauna_period);
    const Seam<Seams::ChunkPass>& wildlife = ctx.seams.wildlife_sweep;
    for (std::uint64_t i = 0; i < fauna_chunks; ++i) {
        if (wildlife.fn != nullptr) {
            wildlife.fn(wildlife.ctx, ctx, island, island.sweep.fauna_x, island.sweep.fauna_y, fx);
        }
        advance_sweep_cursor(island.sweep.fauna_x, island.sweep.fauna_y, tiles.width(),
                             tiles.height(), ctx.cfg.chunk_edge);
    }
    object_manager_tick(ctx, island, fx);
    // [island+0x30] message manager: I/O, not modelled (O-52).
    city_ring_tick(ctx, island, fx);
    event_300s(ctx, island, fx);
    // drought_tick (D: 0x1008aeb0) is stage S13.
    island.sweep.stamp = static_cast<std::uint16_t>(t);
}

// ---------------------------------------------------------------------------
// The world: advance and the effect merge. Scheduler is the friend of World that
// reaches its private state; everything else in this file works on views.

namespace {

/// The context of one per-player seam call (Seams::player_groups, Seams::defeat),
/// bound to the PlayerHooks of player_tick.
struct PlayerPassCtx {
    World* world = nullptr;
    const TickContext* ctx = nullptr;
    const Seam<Seams::PlayerPass>* seam = nullptr;
    CountingDraw* draws = nullptr;
};

/// The context of the AI call (Seams::ai), bound to PlayerHooks::ai.
struct AiCtx {
    World* world = nullptr;
    const TickContext* ctx = nullptr;
    CountingDraw* draws = nullptr;
};

std::int64_t add_saturating(std::int64_t a, std::int64_t b) {
    if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return a + b;
}

/// |delta| as a u32, cut at 0xFFFFFFFF.
std::uint32_t magnitude32(std::int64_t delta) {
    const std::uint64_t m = delta < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(delta)
                                      : static_cast<std::uint64_t>(delta);
    return m > std::numeric_limits<std::uint32_t>::max() ? std::numeric_limits<std::uint32_t>::max()
                                                         : static_cast<std::uint32_t>(m);
}

/// Adds the draws of one call to the advance's per-site totals (site order).
void add_draws(std::vector<DrawRecord>& total, const std::vector<DrawRecord>& more) {
    for (const DrawRecord& r : more) {
        auto it = std::find_if(total.begin(), total.end(),
                               [&](const DrawRecord& x) { return x.site == r.site; });
        if (it != total.end()) {
            it->count += r.count;
        } else {
            total.push_back(r);
        }
    }
    std::sort(total.begin(), total.end(), [](const DrawRecord& a, const DrawRecord& b) {
        return static_cast<int>(a.site) < static_cast<int>(b.site);
    });
}

} // namespace

struct Scheduler {
    static std::uint64_t advance(World& w, std::uint32_t game_ms);
    static void release_counter(World& w, BuildingId home, std::uint32_t home_counter);
    static void merge(World& w, Effects& fx, std::uint64_t tick, std::uint32_t t);
    static void run_player_pass(void* raw, Player& p);
    static void run_ai(void* raw);
    static Island* island_of_id(World& w, IslandId id);
    static City* city_of_ref(World& w, CityRef ref);
};

Island* Scheduler::island_of_id(World& w, IslandId id) {
    if (!id.valid() || id.v >= w.islands_.size() || !w.islands_[id.v].has_value()) {
        return nullptr;
    }
    return &*w.islands_[id.v];
}

City* Scheduler::city_of_ref(World& w, CityRef ref) {
    Island* is = island_of_id(w, ref.island);
    return is != nullptr ? is->city(ref.city) : nullptr;
}

void Scheduler::release_counter(World& w, BuildingId home, std::uint32_t home_counter) {
    Island* is = island_of_id(w, island_of(home));
    Building* b = is != nullptr ? find_building_mut(*is, home) : nullptr;
    std::uint32_t* counter = b != nullptr ? figure_counter(b->payload, home_counter) : nullptr;
    if (counter != nullptr && *counter > 0) {
        --*counter;
    }
}

void Scheduler::merge(World& w, Effects& fx, std::uint64_t tick, std::uint32_t t) {
    // 1. Spawns: a figure per request, id from the world counter, slot by the group
    // list's policy (A6), and a FigureSpawned event.
    for (const SpawnRequest& s : fx.spawns) {
        // An id space that is used up (next_figure at the invalid id, a state the
        // loader accepts), an id already in the registry or a list that cannot grow
        // is refused by add_figure.
        Figure f;
        f.id = FigureId{w.ids_.next_figure};
        f.kind = s.figure_kind;
        f.owner = s.owner;
        f.home = s.home;
        f.home_counter = s.home_counter;
        f.job = s.job;
        f.pos = s.pos;
        f.born = t;
        if (add_figure(w.figures_, f, w.config_, w.rules_) == kNoSlot) {
            // No figure: the tick raised the home counter for it, so it is taken back
            // here, or the building would count a carrier that no release can ever
            // clear (F35).
            release_counter(w, s.home, s.home_counter);
            continue;
        }
        ++w.ids_.next_figure;
        SimEvent e;
        e.kind = SimEventKind::FigureSpawned;
        e.tick = tick;
        e.player = s.owner;
        e.building = s.home;
        e.figure = f.id;
        e.code = s.figure_kind;
        e.x = s.pos.x / 256;
        e.y = s.pos.y / 256;
        fx.ui.push_back(e);
    }
    // 2. Releases.
    for (const ReleaseCounter& r : fx.releases) {
        release_counter(w, r.home, r.home_counter);
    }
    // 3. Money.
    for (const MoneyDelta& m : fx.money) {
        Player* p = w.players_.find(m.player);
        if (p != nullptr) {
            p->money = add_saturating(p->money, m.delta);
        }
    }
    // 4. Goods moves.
    for (const GoodsMove& g : fx.goods_moves) {
        City* c = city_of_ref(w, g.city);
        if (c == nullptr) {
            continue;
        }
        if (g.delta_88 > 0) {
            static_cast<void>(c->stock.add(g.goods, magnitude32(g.delta_88)));
        } else if (g.delta_88 < 0) {
            static_cast<void>(c->stock.remove(g.goods, magnitude32(g.delta_88)));
        }
    }
    // 5. Reserves.
    for (const Reserve& r : fx.reserves) {
        Figure* f = find_figure(w.figures_, r.figure);
        Island* is = island_of_id(w, island_of(r.building));
        Building* b = is != nullptr ? find_building_mut(*is, r.building) : nullptr;
        if (f == nullptr || b == nullptr) {
            continue;
        }
        const BuildingType* type = w.types_->find(b->type);
        std::uint32_t* store = output_store(b->payload, r.out_index);
        if (type == nullptr || store == nullptr || r.out_index >= type->output_goods.size()) {
            continue;
        }
        const GoodsId goods = type->output_goods[r.out_index];
        if (f->cargo_88 != 0 && f->cargo_goods != goods) {
            continue; // the cart carries something else
        }
        const std::uint32_t room = std::numeric_limits<std::uint32_t>::max() - f->cargo_88;
        const std::uint32_t taken = std::min({r.amount_88, *store, room});
        if (taken == 0) {
            continue;
        }
        *store -= taken;
        f->cargo_goods = goods;
        f->cargo_88 += taken;
    }
    // 6. Deliveries.
    for (const Deliver& d : fx.deliveries) {
        Figure* f = find_figure(w.figures_, d.figure);
        City* c = city_of_ref(w, d.city);
        if (f == nullptr || c == nullptr || f->cargo_88 == 0 ||
            f->cargo_goods >= c->stock.stock.size()) {
            continue;
        }
        const std::uint32_t before = c->stock.stock[f->cargo_goods];
        static_cast<void>(c->stock.add(f->cargo_goods, f->cargo_88));
        const std::uint32_t after = c->stock.stock[f->cargo_goods];
        const std::uint32_t delivered = after > before ? after - before : 0U;
        f->cargo_88 -= std::min(delivered, f->cargo_88);
        if (f->cargo_88 == 0) {
            f->cargo_goods = 0;
        }
    }
    // 7. Timed events.
    for (const EnqueueEvent& e : fx.events) {
        w.events_.enqueue(w.ids_, tick, e.delay, e.e);
    }
    // 8. UI events, and the telemetry of the call.
    for (const SimEvent& e : fx.ui) {
        w.out_.events.push_back(e);
        w.host_events_.push(e);
    }
    add_draws(w.out_.draws, fx.draws);
}

void Scheduler::run_player_pass(void* raw, Player& p) {
    auto& c = *static_cast<PlayerPassCtx*>(raw);
    World& w = *c.world;
    Effects fx;
    SerialWorld serial(w.islands_, w.config_.layout, w.figures_, w.players_, fx);
    const auto before = c.draws->counts;
    c.seam->fn(c.seam->ctx, *c.ctx, serial, p);
    fx.draws = draw_records_between(before, c.draws->counts);
    merge(w, fx, c.ctx->tick, c.ctx->t);
}

void Scheduler::run_ai(void* raw) {
    // docs/sim/state.md §5.10, A15: the AI reads the state through the SerialWorld,
    // draws through the tick's DrawSource, writes its consequences to the Effects
    // (merged right after the call, C2) and returns commands that are applied at once,
    // in order, by the same code as a host command. Nobody logs them: a replay needs
    // the same seam installed.
    auto& c = *static_cast<AiCtx*>(raw);
    World& w = *c.world;
    Effects fx;
    std::vector<Command> commands;
    {
        const SerialWorld serial(w.islands_, w.config_.layout, w.figures_, w.players_, fx);
        const auto before = c.draws->counts;
        w.seams_.ai.fn(w.seams_.ai.ctx, *c.ctx, serial, commands);
        fx.draws = draw_records_between(before, c.draws->counts);
    }
    merge(w, fx, c.ctx->tick, c.ctx->t);
    for (const Command& command : commands) {
        static_cast<void>(Commands::apply(w, command, true));
    }
}

std::uint64_t Scheduler::advance(World& w, std::uint32_t game_ms) {
    // docs/sim/state.md §5.3. No early return for 0: the original adds 0 and still
    // runs every subsystem.
    w.out_ = SimOutput{};
    w.out_.rng_cursor_before = w.rng_.cursor();
    const std::uint64_t tick = w.clock_.begin(game_ms);
    const auto t = static_cast<std::uint32_t>(tick);
    GlobalDraw global(w.rng_);
    CountingDraw draw(global);
    const TickContext ctx{tick,
                          t,
                          w.config_,
                          w.rules_,
                          w.settings_,
                          *w.types_,
                          PlayersView(w.players_),
                          IslandsView(w.islands_),
                          draw,
                          w.seams_};
    // 1. The islands, in table order (D: 0x100a2e60, nulls skipped); each island's
    // Effects are merged right after its tick (C1), so figure ids, registry slots
    // and event sequence numbers follow the island order.
    for (std::size_t i = 0; i < w.islands_.size(); ++i) {
        if (!w.islands_[i].has_value()) {
            continue;
        }
        Effects fx;
        const auto before = draw.counts;
        island_tick(ctx, *w.islands_[i], fx);
        fx.draws = draw_records_between(before, draw.counts);
        merge(w, fx, tick, t);
    }
    // 2. timed events (S12), 3. figures (S12).
    // 4. The player table (D: 0x1009c4d0 via world +0x118), with the two per-player
    // seams bound to the hooks of player_tick; a seam without a function is the
    // documented default (does nothing), so its hook stays null. Seams::ai is the
    // table-level hook, called after the visits and before the stamp (A15).
    PlayerPassCtx groups_ctx{&w, &ctx, &w.seams_.player_groups, &draw};
    PlayerPassCtx defeat_ctx{&w, &ctx, &w.seams_.defeat, &draw};
    PlayerHooks hooks;
    if (w.seams_.player_groups.fn != nullptr) {
        hooks.groups = &Scheduler::run_player_pass;
        hooks.groups_ctx = &groups_ctx;
    }
    if (w.seams_.defeat.fn != nullptr) {
        hooks.defeat = &Scheduler::run_player_pass;
        hooks.defeat_ctx = &defeat_ctx;
    }
    AiCtx ai_ctx{&w, &ctx, &draw};
    if (w.seams_.ai.fn != nullptr) {
        hooks.ai = &Scheduler::run_ai;
        hooks.ai_ctx = &ai_ctx;
    }
    player_table_tick(w.players_, t, w.config_, w.rules_, hooks);
    // 5. script timers, 6. monopoly, 7. the 7.5 min event (S12).
    w.clock_.end();
    w.out_.rng_cursor_after = w.rng_.cursor();
    return tick;
}

std::uint64_t World::advance(std::uint32_t game_ms) {
    return Scheduler::advance(*this, game_ms);
}

std::vector<SimEvent> World::drain_events() {
    std::vector<SimEvent> out(host_events_.events.begin(), host_events_.events.end());
    host_events_.events.clear();
    return out;
}

void World::set_event_capacity(std::uint64_t capacity) {
    host_events_.capacity = capacity;
    host_events_.trim();
}

std::uint64_t World::event_capacity() const {
    return host_events_.capacity;
}

std::uint64_t World::events_dropped() const {
    return host_events_.dropped;
}

} // namespace anno::sim
