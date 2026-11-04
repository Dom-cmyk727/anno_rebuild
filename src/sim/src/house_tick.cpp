// The live House update and the stage pass (MAP-POP-LIVE-1 L4; docs/sim/population-live.md
// section 19).
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include <anno/sim/house_kernel.hpp>
#include <anno/sim/house_tick.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/resident.hpp>
#include <anno/sim/resident_job.hpp>

namespace anno::sim {

namespace {

void push_fault(Effects& fx, BuildingId building, HouseFault fault,
                NotEvaluatedReason reason = NotEvaluatedReason{}) {
    FaultRecord rec;
    rec.house = fault;
    rec.reason = reason;
    rec.building = building;
    fx.faults.push_back(rec);
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
    // 3. The spawn block, BEFORE the commit of the effects and before any variant draw: the draw
    // is made whether or not the send succeeds (D 0x10076397).
    if (kernel.lowest.has_value()) {
        const ResidentDispatch d = resident_dispatch(ctx.draw, island, city, b, type, ctx.types,
                                                     *kernel.lowest, resident_walkers_used(*h));
        if (d.spawn.has_value()) {
            fx.spawns.push_back(*d.spawn);
        }
    }
    // 4-6. The commit of last/E/stock/residents and the cap or floor effects.
    const auto key = island.cache.by_id.find(id);
    if (key == island.cache.by_id.end()) {
        push_fault(fx, id, HouseFault::NoSuchHouse);
        return;
    }
    const HouseEnv env{
        ctx.cfg, ctx.rules, ctx.types, *ctx.serial->players, ctx.draw, *ctx.serial->figures, ctx.t};
    const BuildingKey where = key->second;
    const HouseOutcome outcome = apply_house_effects(env, island, where, kernel);
    if (outcome.kind == HouseOutcomeKind::Fault) {
        push_fault(fx, id, outcome.fault, outcome.not_evaluated);
    }
}

void stage_pass(const TickContext& ctx, City& city, Effects& fx) {
    if (!ctx.rules.house_updates) {
        return;
    }
    // The gate and the stamp (D 0x10029610): the stamp moves on the taken path whatever the loop
    // finds.
    if (static_cast<std::uint16_t>(ctx.t - city.stage_stamp) < kStagePassPeriod) {
        return;
    }
    city.stage_stamp = static_cast<std::uint16_t>(ctx.t);
    for (const StageRecord& s : ctx.types.stage_table()) {
        if (s.stage >= 32) {
            continue; // the registry refuses it (the stage mask is 32 bits); defensive
        }
        const std::uint32_t bit = std::uint32_t{1} << s.stage;
        if ((city.stage_mask & bit) != 0) {
            continue;
        }
        if (residents_at_or_above(ctx.cfg, ctx.types, city, s.group) >=
            static_cast<std::int64_t>(s.count)) {
            city.stage_mask |= bit;
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
        p->flags |= city.stage_mask;
    }
}

std::string house_updates_problem(const SimConfig& cfg, const Rules& rules,
                                  const TypeRegistry& types) {
    if (!rules.house_updates) {
        return {};
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
