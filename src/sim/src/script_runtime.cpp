#include <algorithm>
#include <bit>
#include <map>
#include <set>
#include <utility>

#include <anno/sim/hash.hpp>
#include <anno/sim/script_runtime.hpp>
namespace anno::sim {
namespace {
auto compile_runtime(const ScriptRuntime& r, const TypeRegistry& types, const SimConfig& cfg) {
    std::vector<TypeId> ids;
    ids.reserve(types.types().size());
    for (const auto& t : types.types())
        ids.push_back(t.id);
    std::vector<std::string_view> goods;
    for (const auto& g : types.goods_table())
        goods.push_back(g.name);
    std::optional<std::string_view> ns;
    if (r.asset_namespace)
        ns = *r.asset_namespace;
    return compile_script(r.source, {ids, cfg.player_slots, cfg.script_timers, goods, ns},
                          r.limits);
}
struct Host final : ScriptHost {
    std::vector<SimEvent>* output;
    std::uint64_t event_tick;
    std::vector<SimEvent> staged_events;
    bool wave(std::string_view asset_namespace, std::string_view relative, std::uint32_t offset,
              ScriptWork& work) override {
        if (!output)
            return false;
        const auto bytes = asset_namespace.size() + 1 + relative.size();
        for (std::size_t i = 0; i < bytes; ++i)
            if (!work.consume())
                return false;
        std::string asset;
        asset.reserve(bytes);
        asset.append(asset_namespace);
        asset.push_back('/');
        asset.append(relative);
        SimEvent e;
        e.kind = SimEventKind::ScriptWave;
        e.tick = event_tick;
        e.code = 317;
        e.wave = ScriptWave{std::move(asset), offset, WaveAudience::AllLocal};
        staged_events.push_back(std::move(e));
        return true;
    }
    const FigureRegistry* figures;
    const WorldSettings* settings;
    ScriptQueryStats& stats;
    bool indexed = false;
    std::map<std::int32_t, std::map<std::int32_t, std::vector<const Figure*>>> cells;
    std::set<FigureId> crewed;
    bool touch(const ScriptArea& a, std::uint32_t mapping, bool ship, ScriptWork& work,
               bool& result) override {
        result = false;
        if (!figures || !settings || mapping >= bindings.size())
            return false;
        if (!indexed) {
            for (const auto& slot : figures->list.slots) {
                if (!work.consume())
                    return false;
                ++stats.slots;
                if (!slot)
                    continue;
                const auto& f = *slot;
                if (f.parent.valid() && f.crew_slot) {
                    if (!work.consume())
                        return false;
                    ++stats.crew;
                    crewed.insert(f.parent);
                }
                if (f.parent.valid() || f.expiry_stamp)
                    continue;
                const auto* type = types.find_figure_type(f.kind);
                if (!type || type->bewohntype < 97 || type->bewohntype > 128)
                    continue;
                if (!work.consume())
                    return false;
                ++stats.inserted;
                cells[f.pos.y / 256 / 16][f.pos.x / 256 / 16].push_back(&f);
            }
            indexed = true;
        }
        const auto end = [](std::int32_t hi, std::uint32_t dimension) {
            const auto wrapped = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(hi) + 15u);
            return std::min<std::int64_t>(wrapped, dimension) / 16;
        };
        const auto x0 = std::max(a.x0, 0) / 16, y0 = std::max(a.y0, 0) / 16;
        const auto x1 = end(a.x1, settings->world_width), y1 = end(a.y1, settings->world_height);
        for (auto row = cells.lower_bound(y0); row != cells.end() && row->first < y1; ++row) {
            if (!work.consume())
                return false;
            ++stats.rows;
            for (auto cell = row->second.lower_bound(x0);
                 cell != row->second.end() && cell->first < x1; ++cell) {
                if (!work.consume())
                    return false;
                ++stats.cells;
                for (const auto* f : cell->second) {
                    if (!work.consume())
                        return false;
                    ++stats.candidates;
                    const auto kind = types.find_figure_type(f->kind)->bewohntype;
                    const auto x = f->pos.x / 256, y = f->pos.y / 256;
                    if (f->owner != bindings[mapping] || (f->state_word & 0x200e) ||
                        (ship ? kind < 113 || kind > 128 : kind < 97 || kind > 112) ||
                        (kind >= 108 && kind <= 112 && !crewed.contains(f->id)) || x < a.x0 ||
                        x >= a.x1 || y < a.y0 || y >= a.y1)
                        continue;
                    result = true;
                    return true;
                }
            }
        }
        return true;
    }
    PlayerTable& players;
    ScriptTimers& timers;
    const TypeRegistry& types;
    const std::vector<PlayerId>& bindings;
    std::map<PlayerId, Player> staged;
    std::map<PlayerId, std::int64_t> staged_money;
    std::optional<ScriptTimers> staged_timers;
    Host(PlayerTable& p, ScriptTimers& t, const TypeRegistry& registry,
         const std::vector<PlayerId>& b, const FigureRegistry* f, const WorldSettings* s,
         ScriptQueryStats& q, std::vector<SimEvent>* events, std::uint64_t tick)
        : output(events), event_tick(tick), figures(f), settings(s), stats(q), players(p),
          timers(t), types(registry), bindings(b) {}
    bool begin() override {
        staged_events.clear();
        staged.clear();
        staged_money.clear();
        staged_timers.reset();
        return true;
    }
    void commit() override {
        if (output)
            for (auto& e : staged_events)
                output->push_back(std::move(e));
        for (auto& [id, player] : staged)
            *players.find(id) = std::move(player);
        for (const auto& [id, balance] : staged_money)
            players.find(id)->money = balance;
        if (staged_timers)
            timers = std::move(*staged_timers);
        staged_events.clear();
        staged.clear();
        staged_money.clear();
        staged_timers.reset();
    }
    void rollback() override {
        staged_events.clear();
        staged.clear();
        staged_money.clear();
        staged_timers.reset();
    }
    bool permission(std::uint32_t mapping, TypeId id, std::uint32_t raw, bool all,
                    ScriptWork& work) override {
        if (mapping >= bindings.size())
            return false;
        const auto owner = bindings[mapping];
        const auto* player = players.find(owner);
        if (!player)
            return false;
        auto it = staged.find(owner);
        if (it == staged.end()) {
            it = staged.emplace(owner, *player).first;
            const auto money = staged_money.find(owner);
            if (money != staged_money.end()) {
                it->second.money = money->second;
                staged_money.erase(money);
            }
        }
        return all ? set_all_construction_permissions(it->second, it->second.permissions, types,
                                                      raw, work)
                   : set_construction_permission(it->second, it->second.permissions, types, id,
                                                 raw);
    }
    bool money(std::uint32_t mapping, std::int32_t delta) override {
        if (mapping >= bindings.size())
            return false;
        const auto owner = bindings[mapping];
        const auto* player = players.find(owner);
        if (!player)
            return false;
        if (auto it = staged.find(owner); it != staged.end()) {
            it->second.money = add_money_saturating(it->second.money, delta);
        } else {
            auto [entry, inserted] = staged_money.try_emplace(owner, player->money);
            (void)inserted;
            entry->second = add_money_saturating(entry->second, delta);
        }
        return true;
    }
    bool timer(ScriptAction action, std::uint32_t index, std::uint32_t value) override {
        if (index >= timers.counter.size() || index >= timers.active.size())
            return false;
        if (!staged_timers)
            staged_timers = timers;
        if (action == ScriptAction::TimerSet)
            staged_timers->counter[index] = value * 500u;
        else if (action == ScriptAction::TimerStart)
            staged_timers->active[index] = 1;
        else if (action == ScriptAction::TimerStop)
            staged_timers->active[index] = 0;
        else
            return false;
        return true;
    }
    std::uint32_t timer_seconds(std::uint32_t index) const override {
        const auto& current = staged_timers ? *staged_timers : timers;
        return index < current.counter.size() ? current.counter[index] / 500u : 0;
    }
};
} // namespace
formats::Result<ScriptRuntime, std::string>
prepare_script(std::string source, const TypeRegistry& types, const PlayerTable& players,
               const SimConfig& cfg, ScriptLimits limits,
               std::optional<std::string> asset_namespace) {
    ScriptRuntime r;
    r.source = std::move(source);
    r.limits = limits;
    r.asset_namespace = std::move(asset_namespace);
    auto program = compile_runtime(r, types, cfg);
    if (!program)
        return "script offset " + std::to_string(program.error().offset) + ": " +
               program.error().message;
    r.state = make_script_state(program.value());
    for (const auto& m : program.value().mappings) {
        PlayerId id{m.slot};
        if (!players.find(id))
            id = PlayerId{cfg.neutral_player};
        if (!players.find(id))
            return std::string("script mapping has no player or neutral fallback: ") + m.name;
        r.state.bindings.push_back(id);
    }
    r.program = std::make_shared<const ScriptProgram>(std::move(program).value());
    return r;
}
std::uint64_t script_content_digest(const ScriptRuntime& r) {
    StateHasher h;
    const auto text = [&](std::string_view value) {
        h.u64(value.size());
        h.bytes(std::span(reinterpret_cast<const std::uint8_t*>(value.data()), value.size()));
    };
    h.u32(r.policy);
    h.u32(r.limits.source_bytes);
    h.u32(r.limits.nodes);
    h.u32(r.limits.depth);
    h.u32(r.limits.work);
    h.u32(r.limits.asset_bytes);
    text(r.source);
    h.u8(r.asset_namespace ? 1 : 0);
    if (r.asset_namespace)
        text(*r.asset_namespace);
    return h.value();
}
std::string validate_script(const ScriptRuntime& r, const TypeRegistry& types,
                            const PlayerTable& players, const SimConfig& cfg) {
    const auto& s = r.state;
    if (r.policy != kScriptPolicy)
        return "script execution policy differs";
    if (r.limits.source_bytes == 0 || r.limits.source_bytes > 1048576 || r.limits.nodes == 0 ||
        r.limits.nodes > 65536 || r.limits.depth == 0 || r.limits.depth > 128 ||
        r.limits.work == 0 || r.limits.work > 10000000 || r.limits.asset_bytes == 0 ||
        r.limits.asset_bytes > 65536)
        return "invalid script limits";
    for (const auto& p : players.slots)
        if (p) {
            auto problem = validate_permissions(p->permissions, types);
            if (!problem.empty())
                return problem;
        }
    if (s.context == ScriptContext::Missing || s.context == ScriptContext::ExplicitNone) {
        if (r.asset_namespace || !r.source.empty() || r.program || !s.signals.empty() ||
            !s.bindings.empty() || s.cursor || s.last_arg || s.last_work ||
            s.fault != ScriptFault::None || s.fault_offset)
            return "script-free context carries execution state";
        return {};
    }
    if (s.context != ScriptContext::Pending && s.context != ScriptContext::Ready &&
        s.context != ScriptContext::Fault)
        return "unplayable script context";
    if (!r.program || r.program->source != r.source || r.program->policy != r.policy)
        return "script executable identity differs";
    const auto& p = *r.program;
    if (s.signals.size() != p.initial.size() || s.bindings.size() != p.mappings.size() ||
        s.cursor >= std::max<std::size_t>(1, p.branches.size()) || s.last_work > r.limits.work ||
        std::any_of(s.signals.begin(), s.signals.end(), [](auto b) { return b > 1; }))
        return "invalid script continuation";
    for (std::size_t i = 0; i < s.bindings.size(); ++i) {
        const auto id = s.bindings[i];
        if (id.v >= cfg.player_slots || !players.find(id) ||
            (id.v != p.mappings[i].slot && id.v != cfg.neutral_player))
            return "invalid script binding";
    }
    if (s.fault_offset > r.source.size() || (s.fault == ScriptFault::None && s.fault_offset != 0))
        return "invalid script fault location";
    if ((s.context == ScriptContext::Fault) != (s.fault != ScriptFault::None))
        return "invalid script fault state";
    if (s.context == ScriptContext::Pending &&
        (s.cursor || s.signals != p.initial || s.last_work || s.last_arg))
        return "invalid pending script state";
    return {};
}
std::string restore_script(ScriptRuntime& r, const TypeRegistry& types, const PlayerTable& players,
                           const SimConfig& cfg) {
    if (r.policy != kScriptPolicy)
        return "script execution policy differs";
    if (!r.source.empty()) {
        auto p = compile_runtime(r, types, cfg);
        if (!p)
            return "script offset " + std::to_string(p.error().offset) + ": " + p.error().message;
        r.program = std::make_shared<const ScriptProgram>(std::move(p).value());
    } else
        r.program.reset();
    return validate_script(r, types, players, cfg);
}
bool advance_script(ScriptRuntime& r, PlayerTable& players, ScriptTimers& timers,
                    const TypeRegistry& types, std::uint32_t arg, const FigureRegistry* figures,
                    const WorldSettings* settings, std::vector<SimEvent>* output,
                    std::uint64_t event_tick) {
    r.query_stats = {};
    if (!r.program)
        return r.state.context == ScriptContext::Missing ||
               r.state.context == ScriptContext::ExplicitNone;
    Host host(players, timers, types, r.state.bindings, figures, settings, r.query_stats, output,
              event_tick);
    return visit_script(*r.program, r.state, arg, host);
}
} // namespace anno::sim
