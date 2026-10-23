// World::advance, the figure registry tick and the effect merge (docs/sim/state.md
// §5.3, §5.7–§5.10, §4.11; stages S7 and S12). The island phase is island_tick.cpp.
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
#include <anno/sim/timers.hpp>
#include <anno/sim/world.hpp>

#include "scheduler_internal.hpp"

namespace anno::sim {

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

void Scheduler::merge(World& w, Effects& fx, std::uint64_t tick, std::uint32_t t, bool in_advance) {
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
    // 2. Releases. One that names a figure goes through it (D 0x1003b6e0: only while the
    // unit's home +0x78 is set, which it then clears), so a figure releases once.
    for (const ReleaseCounter& r : fx.releases) {
        if (!r.figure.valid()) {
            release_counter(w, r.home, r.home_counter);
            continue;
        }
        Figure* f = find_figure(w.figures_, r.figure);
        if (f != nullptr && f->home.valid()) {
            const BuildingId home = f->home;
            f->home = BuildingId{};
            release_counter(w, home, f->home_counter);
        }
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
    // 8. UI events, and the telemetry of the call (inside an advance only: between two
    // advances SimOutput belongs to the last advance and is not touched).
    for (const SimEvent& e : fx.ui) {
        if (in_advance) {
            w.out_.events.push_back(e);
        }
        w.host_events_.push(e);
    }
    if (in_advance) {
        add_draws(w.out_.draws, fx.draws);
    }
}

void Scheduler::destroy_figure(World& w, FigureId id) {
    // Phase C: D 0x10097520 empties the figure's registry entry, calls the unit's
    // destructor (0x10031db0, which frees only the unit) and lowers the live count. It
    // releases nothing: the home counter went back at the figure's end (end_figure,
    // OQ-131; docs/sim/state.md §5.9).
    static_cast<void>(remove_figure(w.figures_, id));
}

void Scheduler::run_figures(World& w, const TickContext& ctx, CountingDraw& draw) {
    // D 0x1009a8a0 (world +0x114). Phase A (0x1009a8b0..0x1009aa21, sector re-bucketing
    // of the float positions, /250) is derived spatial data and not modelled (O-17).
    FigureRegistry& reg = w.figures_;
    const std::uint32_t t = ctx.t;
    // Phase B (0x1009aa2a..0x1009aac1): n = the entries of the vector, read once; an
    // empty vector skips the phase (0x1009aa2f, 0x1009aa3d). count = clamp(d·n/50, 1,
    // n), no +1. A cursor at or beyond n (A8, an inconsistent state that
    // validate_figures refuses) visits nothing.
    const std::uint64_t n = reg.list.slots.size();
    if (n > 0 && reg.list.cursor < n) {
        const std::uint64_t count = visit_count_clamped(t, reg.stamp, n, w.config_.figure_b_div);
        std::uint64_t cur = reg.list.cursor;
        const Seam<Seams::FigureTick>& seam = w.seams_.figure_tick;
        const std::uint32_t interim = w.rules_.interim_figure_return_ticks;
        for (std::uint64_t i = 0; i < count; ++i) {
            // The slot is looked up in every iteration (the original re-reads the vector
            // start, 0x1009aa93): a merge may have grown the list.
            const auto slot = static_cast<std::size_t>(cur);
            if (slot < reg.list.slots.size() && reg.list.slots[slot].has_value()) {
                Figure& fig = *reg.list.slots[slot];
                if (seam.fn != nullptr) {
                    // The unit's slot +0x74 (0x1009aaa8) is the behaviour seam (O-8):
                    // it writes only its own figure and its Effects, merged right after
                    // it returns (C2).
                    const FigureId id = fig.id;
                    const bool had_stamp = fig.expiry_stamp.has_value();
                    Effects fx;
                    {
                        SerialWorld serial(w.islands_, w.config_.layout, reg, w.players_, fx);
                        const auto before = draw.counts;
                        seam.fn(seam.ctx, ctx, serial, fig);
                        fx.draws = draw_records_between(before, draw.counts);
                    }
                    fig.id = id; // the identity is the registry's, never the seam's
                    sync_figure_lists(reg, fig, had_stamp, fx);
                    merge(w, fx, ctx.tick, t); // may grow the list: `fig` is not used below
                } else if (interim > 0 && !fig.expiry_stamp.has_value() &&
                           static_cast<std::uint32_t>(t - fig.born) >= interim) {
                    // D12 (OQ-047, off by default): a figure returns after N ticks. It
                    // ends as the original's do (end_figure: the counter back now, the
                    // figure on the expiry list), and its slot is freed by phase C
                    // figure_expiry_ticks later (D: Remove(unit, 0) at 0x10034844).
                    Effects fx;
                    end_figure(reg, fig, t, fx);
                    merge(w, fx, ctx.tick, t);
                }
            }
            if (++cur >= n) {
                cur -= n;
            }
        }
        reg.list.cursor = static_cast<std::uint32_t>(cur); // 0x1009aac1
    }
    reg.stamp = static_cast<std::uint16_t>(t); // 0x1009aad9, after phase B
    // Phase C (0x1009aacc..0x1009ab31): the expiry list from its front; a figure whose
    // stamp is 7500 ticks old or older (u16 difference, `cmp cx, 0x1d4c; jb` at
    // 0x1009ab02) is destroyed and the scan goes on, the first younger one ends it. A
    // figure without the property (here: gone, or without a stamp) is destroyed too
    // (0x1009aaf8).
    while (!reg.expiry.empty()) {
        const FigureId id = reg.expiry.front();
        const Figure* f = find_figure(reg, id);
        if (f != nullptr && f->expiry_stamp.has_value() &&
            static_cast<std::uint16_t>(t - *f->expiry_stamp) < w.config_.figure_expiry_ticks) {
            break;
        }
        destroy_figure(w, id); // also takes `id` off the list
    }
    // Cost: phase C erases from the front of a vector and remove_figure searches the
    // list, O(k n) for k figures destroyed in one tick; a deque or an index is a later
    // optimisation once bench_sim (stage S15) measures it.
    // Phase D (0x1009ab33..0x1009ab7c) deletes the records on the list +0x58 through
    // registry slot +0x3c = 0x10097770: the pooled sector records of registry slot
    // +0x38, never a figure (OQ-131); not modelled (O-17, OQ-134).
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
    const InAdvance in_advance{c.ctx, c.draws};
    for (const Command& command : commands) {
        static_cast<void>(Commands::apply(w, command, &in_advance));
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
    // 2. The timed events (D: 0x100add90; events.cpp).
    PopResult events = run_events(w, ctx, draw);
    // 3. The figure registry (D: 0x1009a8a0 via world +0x114, slot +0x20).
    run_figures(w, ctx, draw);
    // D4 (OQ-039, off by default): a second event pass after the figure phase, so a
    // delayed hit a figure enqueued with delay 0 lands in this advance.
    if (w.rules_.events_flush_after_figures) {
        const PopResult again = run_events(w, ctx, draw);
        events.dispatched += again.dispatched;
        events.capped = events.capped || again.capped;
    }
    w.out_.events_dispatched = events.dispatched;
    w.out_.events_capped = events.capped;
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
    // 5.-7. (timers.cpp): their UI events and draws go through one Effects buffer. Room
    // for its at most two events is made first: GCC 13 at -O3 otherwise reports a false
    // -Wstringop-overflow in the reallocation path of the second push_back.
    Effects fx;
    fx.ui.reserve(2);
    const auto before = draw.counts;
    // 5. The script timers (D: 0x1009cd30 via world +0x69c8, slot +0x14).
    if (timers_tick(w.timers_, t, w.rules_.timer0_clamp)) {
        SimEvent e;
        e.kind = SimEventKind::TimerExpired;
        e.tick = tick;
        e.code = 0;
        fx.ui.push_back(e);
    }
    // 6. The monopoly refresh (D: 0x1009d2a0 with this = world + 4, the step in ms);
    // game seconds = world property 7 = t / 500 + offset (settings.hpp).
    const std::uint32_t game_seconds = t / 500U + w.settings_.prop7_offset;
    static_cast<void>(monopoly_refresh(w.monopoly_, game_ms, w.config_, w.rules_, w.islands_,
                                       w.players_, game_seconds));
    // 7. The 7.5 minute event (D: 0x100ae140, the step in ms): the message-manager
    // event 0x15a at the island's centre (D 0x100ae1d6..0x100ae240: x0 + w / 2,
    // y0 + h / 2).
    const std::optional<IslandId> hit =
        event750(w.event750_acc_, game_ms, w.config_, w.rules_, w.settings_, w.islands_, draw);
    if (hit.has_value()) {
        const TileRect r = w.islands_[hit->v]->rect();
        SimEvent e;
        e.kind = SimEventKind::Event15a;
        e.tick = tick;
        e.code = 0x15a;
        e.x = static_cast<std::int32_t>(static_cast<std::int64_t>(r.x0) + r.w / 2);
        e.y = static_cast<std::int32_t>(static_cast<std::int64_t>(r.y0) + r.h / 2);
        fx.ui.push_back(e);
    }
    fx.draws = draw_records_between(before, draw.counts);
    merge(w, fx, tick, t);
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
