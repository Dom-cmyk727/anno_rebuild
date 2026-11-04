// The live House update and the stage pass (MAP-POP-LIVE-1 L4; docs/sim/population-live.md
// section 19).
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <anno/sim/house_kernel.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/resident.hpp>
#include <anno/sim/resident_job.hpp>
#include <anno/sim/transport.hpp>

namespace anno::sim {

std::uint32_t player_flags_set(std::uint32_t value) {
    // D AnnoWelt 0x10020d51-0x10020ecb (docs/sim/population-live.md section 5a). The original
    // keeps a latch at player +0x96/+0x97 that clears bits 24-28 of the incoming value and puts
    // them back, fires an event the first time a bit is present and updates a byte at +0x95.
    // For every value the engine can hold the clear-and-restore is a no-op (the stored value is
    // monotone: it only ever ORs), so the stored result reduces to the two implications below.
    // The latch bytes, the byte +0x95 and the events (0x1f, 0x1e) are not modelled (P-12).
    if ((value & (kPlayerBit25 | kPlayerBit27 | kPlayerBit28)) != 0) {
        value |= kPlayerBit26; // bit 25 via the +0x97 bit 5 restore, bits 27 and 28 directly
    }
    return value;
}

namespace {

void push_fault(Effects& fx, BuildingId building, HouseFault fault,
                NotEvaluatedReason reason = NotEvaluatedReason{}) {
    FaultRecord rec;
    rec.house = fault;
    rec.reason = reason;
    rec.building = building;
    fx.faults.push_back(rec);
}

template <class V> void record(PopulationStats& ps, std::vector<V>& v, V rec) {
    if (v.size() < PopulationStats::kRecordCap) {
        v.push_back(std::move(rec));
    } else {
        ++ps.records_dropped;
    }
}

bool has_serial(const TickContext& ctx) {
    return ctx.serial != nullptr && ctx.serial->players != nullptr &&
           ctx.serial->figures != nullptr;
}

} // namespace

void house_tick(const TickContext& ctx, Island& island, City& city, Building& b,
                const BuildingType& type, Effects& fx) {
    House* h = std::get_if<House>(&b.payload);
    if (h == nullptr) {
        return; // the class table and check_invariants rule this out
    }
    const BuildingId id = b.id;
    if (!has_serial(ctx)) {
        push_fault(fx, id, HouseFault::NoSerialContext);
        return;
    }
    const std::optional<std::uint32_t> group = house_group(ctx.cfg, type.prodtype);
    if (!group) {
        push_fault(fx, id, HouseFault::MissingGroup);
        return;
    }
    const auto view = ctx.types.population_view(*group, ctx.cfg);
    if (!view) {
        push_fault(fx, id, HouseFault::MissingGroup);
        return;
    }
    const PopulationView pv = view.value();
    const HouseWalkerFacts walker_facts = house_walker_facts(type);
    PopulationStats* ps =
        ctx.transport_stats != nullptr ? &ctx.transport_stats->population : nullptr;
    HouseWork* work = ps != nullptr ? &ps->houses[id] : nullptr;
    if (work != nullptr) {
        ++work->visits;
        ++work->kernel_calls;
    }
    // 1-2. The kernel: gate, E', the need loop, the mood, growth, cap or floor.
    HouseKernelInputs in;
    in.view = &pv;
    in.player_mask = ctx.players.flags(city.owner);
    in.now = ctx.t;
    in.last = h->last;
    in.gate_period = ctx.types.class_info(type.cls).gate_period;
    in.residents_64 = h->residents_64;
    in.e = h->e;
    in.stock = h->stock;
    in.mood_flags = 0; // house +0x0c: nothing sets bits 4/5 yet (OQ-462 default)
    in.previous_wohnmax = pv.previous_wohnmax;
    in.walkers = WalkerFacts{resident_walkers_used(*h), walker_facts.walker_max};
    const HouseKernelResult kernel = house_kernel(in);
    if (kernel.status == HouseKernelStatus::Skipped) {
        return; // a skipped gate changes nothing
    }
    if (kernel.status == HouseKernelStatus::NotEvaluated) {
        push_fault(fx, id, HouseFault::NotEvaluated, kernel.reason);
        return;
    }
    if (work != nullptr) {
        ++work->kernel_taken;
    }
    // 3. The spawn block, BEFORE the commit of the effects and before any variant draw: the draw
    // is made whether or not the send succeeds (D 0x10076397).
    if (kernel.lowest.has_value()) {
        const ResidentDispatch d = resident_dispatch(ctx.draw, island, city, b, type, ctx.types,
                                                     *kernel.lowest, resident_walkers_used(*h));
        if (d.spawn.has_value()) {
            fx.spawns.push_back(*d.spawn);
        }
        if (work != nullptr) {
            work->spawn_draws += d.drew ? 1U : 0U;
            work->spawn_requests += d.spawn.has_value() ? 1U : 0U;
        }
    }
    // 4-6. The commit of last/E/stock/residents and the cap or floor effects.
    const auto key = island.cache.by_id.find(id);
    if (key == island.cache.by_id.end()) {
        push_fault(fx, id, HouseFault::NoSuchHouse);
        return;
    }
    const HouseEnv env{
        ctx.cfg, ctx.rules, ctx.types, *ctx.serial->players, ctx.draw, *ctx.serial->figures,
        ctx.t,   work};
    const BuildingKey where = key->second;
    // Diagnostic snapshot for the records (never read by the sim).
    HouseTransitionRecord tr;
    std::vector<std::uint64_t> spent_before;
    std::vector<std::uint32_t> store_before;
    const std::int32_t residents_before = h->residents_64;
    if (ps != nullptr) {
        const std::size_t goods = city.build_goods.size();
        for (std::size_t k = 0; k < goods; ++k) {
            spent_before.push_back(city.build_goods[k].spent);
        }
        for (std::size_t k = 0; k < goods; ++k) {
            const std::size_t g = ctx.cfg.build_goods_first + k;
            store_before.push_back(g < city.stock.stock.size() ? city.stock.stock[g] : 0U);
        }
        tr.building = id;
        tr.tick = ctx.t;
        tr.from = b.type;
        tr.residents_before = residents_before;
        if (const auto g = house_group(ctx.cfg, type.prodtype)) {
            tr.old_group = *g;
            if (*g < city.index.house_counts.size()) {
                tr.houses_old_before = city.index.house_counts[*g];
            }
            tr.counter_old_before = residents_at_or_above(ctx.cfg, ctx.types, city, *g);
        }
    }
    const HouseOutcome outcome = apply_house_effects(env, island, where, kernel);
    if (outcome.kind == HouseOutcomeKind::Fault) {
        push_fault(fx, id, outcome.fault, outcome.not_evaluated);
    }
    if (ps == nullptr) {
        return;
    }
    // MAP-POP-CLOSE-1 F2: what the House really holds now, read from the committed state (the
    // building keeps its slot across an upgrade or downgrade); nullopt when the tick removed it.
    std::optional<std::int32_t> committed;
    if (const City* c = island.city(where.city.city);
        c != nullptr && where.list < c->index.lists.size() &&
        where.slot < c->index.lists[where.list].slots.size()) {
        const auto& slot = c->index.lists[where.list].slots[where.slot];
        if (slot.has_value() && slot->id == id) {
            if (const auto* now = std::get_if<House>(&slot->payload)) {
                committed = now->residents_64;
            }
        }
    }
    if (kernel.residents_64.has_value() && kernel.mood.has_value() &&
        *kernel.residents_64 != residents_before) {
        HouseGrowthRecord gr;
        gr.building = id;
        gr.tick = ctx.t;
        gr.mood = *kernel.mood;
        gr.residents_before = residents_before;
        gr.residents_after = *kernel.residents_64;
        const std::size_t idx = static_cast<std::size_t>(*kernel.mood + 2);
        gr.vermehr =
            pv.group != nullptr && idx < pv.group->vermehr.size() ? pv.group->vermehr[idx] : 0;
        gr.residents_committed = committed.value_or(0);
        gr.removed = !committed.has_value();
        gr.cap_64 = pv.group != nullptr ? std::int32_t{pv.group->wohnmax} * 64 : 0;
        record(*ps, ps->growth, gr);
    }
    using K = HouseTransitionRecord::Kind;
    std::optional<K> kind;
    switch (outcome.kind) {
    case HouseOutcomeKind::Upgraded:
        kind = K::Upgrade;
        break;
    case HouseOutcomeKind::UpgradeRefusedMaterials:
        kind = K::UpgradeRefused;
        break;
    case HouseOutcomeKind::Downgraded:
        kind = K::Downgrade;
        break;
    case HouseOutcomeKind::Removed:
        kind = K::Remove;
        break;
    default:
        break;
    }
    if (!kind) {
        return;
    }
    tr.kind = *kind;
    tr.to = outcome.to;
    tr.new_group = outcome.new_group;
    tr.variant_drawn = outcome.variant_drawn;
    tr.variant_draw = outcome.variant_draw;
    tr.variant_index = outcome.variant_index;
    for (std::size_t k = 0; k < spent_before.size(); ++k) {
        tr.spent_by_good.push_back(static_cast<std::int64_t>(city.build_goods[k].spent) -
                                   static_cast<std::int64_t>(spent_before[k]));
        const std::size_t g = ctx.cfg.build_goods_first + k;
        tr.stock_by_good.push_back(
            (g < city.stock.stock.size() ? std::int64_t{city.stock.stock[g]} : 0) -
            std::int64_t{store_before[k]});
    }
    // The refused upgrade keeps the house: its residents are the committed ones as well.
    if (*kind == K::UpgradeRefused) {
        tr.residents_after = committed.value_or(0);
    }
    if (*kind == K::Upgrade || *kind == K::Downgrade) {
        tr.new_group = outcome.new_group;
        tr.residents_after = committed.value_or(0); // SetGroup keeps the residents (F2: read, not assumed)
        if (tr.old_group < city.index.house_counts.size()) {
            tr.houses_old_after = city.index.house_counts[tr.old_group];
        }
        if (tr.new_group < city.index.house_counts.size()) {
            tr.houses_new_after = city.index.house_counts[tr.new_group];
        }
        tr.counter_old_after = residents_at_or_above(ctx.cfg, ctx.types, city, tr.old_group);
        tr.counter_new_after = residents_at_or_above(ctx.cfg, ctx.types, city, tr.new_group);
        tr.houses_new_before = tr.houses_new_after - 1U;
        tr.houses_old_before = tr.houses_old_after + 1U;
    } else if (*kind == K::Remove) {
        tr.houses_old_after = tr.old_group < city.index.house_counts.size()
                                  ? city.index.house_counts[tr.old_group]
                                  : 0U;
        tr.counter_old_after = residents_at_or_above(ctx.cfg, ctx.types, city, tr.old_group);
    }
    record(*ps, ps->transitions, std::move(tr));
}

void stage_pass(const TickContext& ctx, City& city, Effects& fx, IslandId island) {
    if (!ctx.rules.house_updates) {
        return;
    }
    // The gate and the stamp (D 0x10029610): the stamp moves on the taken path whatever the loop
    // finds.
    if (static_cast<std::uint16_t>(ctx.t - city.stage_stamp) < kStagePassPeriod) {
        return;
    }
    city.stage_stamp = static_cast<std::uint16_t>(ctx.t);
    PopulationStats* ps =
        ctx.transport_stats != nullptr ? &ctx.transport_stats->population : nullptr;
    const std::uint32_t mask_before = city.stage_mask;
    const std::uint32_t flags_before = ctx.players.flags(city.owner);
    if (ps != nullptr) {
        ++ps->stage_passes;
    }
    std::optional<std::vector<std::int64_t>> per_group;
    for (const StageRecord& s : ctx.types.stage_table()) {
        if (s.stage >= 32) {
            continue; // the registry refuses it (the stage mask is 32 bits); defensive
        }
        const std::uint32_t bit = std::uint32_t{1} << s.stage;
        if ((city.stage_mask & bit) != 0) {
            continue;
        }
        if (ps != nullptr) {
            ++ps->stage_evaluations;
        }
        if (!per_group) {
            per_group = residents_at_or_above_all(ctx.cfg, ctx.types, city); // once per pass
        }
        const std::int64_t have = s.group < per_group->size() ? (*per_group)[s.group] : 0;
        if (have >= static_cast<std::int64_t>(s.count)) {
            city.stage_mask |= bit;
            if (ps != nullptr) {
                StageUnlockRecord u;
                u.tick = ctx.t;
                u.city = CityRef{island, city.number};
                u.stage = s.stage;
                u.group = s.group;
                u.count = s.count;
                u.residents_at_or_above = have;
                u.city_mask_before = mask_before;
                u.city_mask_after = city.stage_mask;
                u.player_flags_before = flags_before;
                record(*ps, ps->unlocks, u);
            }
        }
    }
    // The player-mask rule (X16): an unsigned compare of the LOW 23 bits against the city mask,
    // the OR (full dword) only when the player's low part is below it. Not an idempotent OR.
    if (!has_serial(ctx)) {
        push_fault(fx, BuildingId{}, HouseFault::NoSerialContext);
        return;
    }
    Player* p = ctx.serial->players->find(city.owner);
    if (p != nullptr && (p->flags & kStageCompareMask) < city.stage_mask) {
        p->flags = player_flags_set(p->flags | city.stage_mask);
    }
    if (ps != nullptr && p != nullptr) {
        for (auto it = ps->unlocks.rbegin(); it != ps->unlocks.rend() && it->tick == ctx.t &&
                                             it->city == CityRef{island, city.number};
             ++it) {
            it->player_flags_after = p->flags;
        }
    }
}

std::string house_updates_problem(const SimConfig& cfg, const Rules& rules,
                                  const TypeRegistry& types) {
    if (!rules.house_updates) {
        return {};
    }
    // MAP-POP-CLOSE-1 F1: the live path writes the per-city billed counters, and a purchase is
    // refused when one would leave u32. With save_billed_goods off the counter is neither saved nor
    // hashed, so a reload restarts it at 0 and the same world could fault in one run and not in the
    // other. Until a supported implementation exists the combination is refused (create and load).
    if (!rules.save_billed_goods) {
        return "house updates: Rules::save_billed_goods is off, so the billed counters (written by "
               "the live House path and able to refuse a purchase) would differ after a reload; "
               "the combination is not supported";
    }
    const std::uint32_t period = types.class_info(RuntimeClass::House).gate_period;
    if (period < 1 || period > 0xffffU) {
        return "house updates: the House class gate period " + std::to_string(period) +
               " is outside 1..65535 (the gate compares 16-bit differences)";
    }
    const std::span<const HouseGroup> groups = types.house_groups();
    for (std::size_t g = 0; g < groups.size(); ++g) {
        if (groups[g].needs.empty()) {
            return "house updates: house group " + std::to_string(g) +
                   " has no need (the mood reads need slot 0)";
        }
    }
    if (!groups.empty() && !groups[0].no_downgrade) {
        return "house updates: house group 0 does not set NoDowngrade (its downgrade floor would "
               "need a previous group's Wohnmax: PreviousCapacityUnavailable)";
    }
    for (const BuildingType& t : types.types()) {
        if (t.cls != RuntimeClass::House) {
            continue;
        }
        const std::optional<std::uint32_t> g = house_group(cfg, t.prodtype);
        if (!g || *g >= groups.size()) {
            return "house updates: house type " + std::to_string(t.id.v) + " (PRODTYPE " +
                   std::to_string(t.prodtype) + ") names no house group";
        }
    }
    // Second pass (after every type is known to name a group):
    for (const BuildingType& t : types.types()) {
        if (t.cls != RuntimeClass::House) {
            continue;
        }
        const std::optional<std::uint32_t> g = house_group(cfg, t.prodtype);
        // C3: an irregular ladder successor must be declared (BuildingType::special_successor);
        // an undeclared one is a content error, not a silent "not eligible".
        if (house_successor(cfg, types, t) == HouseSuccessor::Malformed) {
            const TypeLadder ladder = house_type_ladder(cfg, types, t);
            const std::optional<std::uint32_t> ng = house_group(cfg, ladder.next->prodtype);
            return "house updates: house type " + std::to_string(t.id.v) + " (group " +
                   std::to_string(*g) + ") has the ladder successor " +
                   std::to_string(ladder.next->id.v) + " (PRODTYPE " +
                   std::to_string(ladder.next->prodtype) + ", " +
                   (ladder.next->cls == RuntimeClass::House
                        ? "group " + (ng ? std::to_string(*ng) : std::string("none")) +
                              (ladder.next->width == t.width && ladder.next->height == t.height
                                   ? std::string()
                                   : std::string(", another footprint"))
                        : std::string("no House")) +
                   "): it is neither the next group's House nor declared as a special successor";
        }
    }
    return {};
}

std::string fault_message(const FaultRecord& f) {
    std::string where;
    if (f.building.valid()) {
        where = " building " + std::to_string(f.building.v);
    }
    if (static_cast<std::uint8_t>(f.resident) != 0) {
        return std::string("resident fault ") + resident_fault_name(f.resident) + where +
               (f.figure.valid() ? " figure " + std::to_string(f.figure.v) : std::string()) +
               " goods " + std::to_string(f.goods);
    }
    std::string m = std::string("house fault ") + house_fault_name(f.house) + where;
    if (f.house == HouseFault::NotEvaluated) {
        m += std::string(" (kernel: ") + not_evaluated_name(f.reason) + ")";
    }
    return m;
}

} // namespace anno::sim
