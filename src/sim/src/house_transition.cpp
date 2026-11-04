// The House level transitions and removal (header: house_transition.hpp).
#include <algorithm>
#include <limits>

#include <anno/sim/create.hpp>
#include <anno/sim/house_transition.hpp>
#include <anno/sim/resident.hpp>
#include <anno/sim/walk_cells.hpp>

namespace anno::sim {

namespace {

constexpr std::int64_t kU32Max = std::numeric_limits<std::uint32_t>::max();

std::int64_t tdiv(std::int64_t a, std::int64_t b) {
    return a / b; // C++ truncates toward zero, like the original's idiv
}

struct Located {
    HouseFault fault = HouseFault::None;
    City* city = nullptr;
    Building* building = nullptr;
    House* house = nullptr;
    const BuildingType* type = nullptr;
};

Located locate(const HouseEnv& env, Island& island, const BuildingKey& key) {
    Located l;
    if (key.city.island != island.index) {
        l.fault = HouseFault::NoSuchHouse;
        return l;
    }
    l.city = island.city(key.city.city);
    if (l.city == nullptr || key.list >= l.city->index.lists.size() ||
        key.slot >= l.city->index.lists[key.list].slots.size() ||
        !l.city->index.lists[key.list].slots[key.slot].has_value()) {
        l.fault = HouseFault::NoSuchHouse;
        return l;
    }
    l.building = &*l.city->index.lists[key.list].slots[key.slot];
    l.house = std::get_if<House>(&l.building->payload);
    if (l.house == nullptr) {
        l.fault = HouseFault::NoSuchHouse;
        return l;
    }
    l.type = env.types.find(l.building->type);
    if (l.type == nullptr) {
        l.fault = HouseFault::UnknownType;
    }
    return l;
}

/// The population view of the group a house type belongs to.
HouseFault view_of(const HouseEnv& env, const BuildingType& type, std::uint32_t& group,
                   PopulationView& view) {
    const std::optional<std::uint32_t> g = house_group(env.cfg, type.prodtype);
    if (!g) {
        return HouseFault::MissingGroup;
    }
    auto v = env.types.population_view(*g, env.cfg);
    if (!v) {
        return HouseFault::MissingGroup;
    }
    group = *g;
    view = v.value();
    return HouseFault::None;
}

/// The house destructor's detach (D 0x10075bc0: house.detach(fig) 0x10075b70 clears the slot, then
/// fig.vt+0x2c(house)): the figure is NOT ended; it lives on with home = none (as the workshop and
/// farm figures do, OQ-131) and its body ends it at its next arrival or visit without a home.
void detach_walker(const HouseEnv& env, FigureId id, std::uint32_t& detached) {
    Figure* f = id.valid() ? find_figure(env.figures, id) : nullptr;
    if (f == nullptr) {
        return;
    }
    f->home = BuildingId{};
    ++detached;
}

/// Lazy trimming (the original keeps 4 slots and only gates the spawn by the type's maximum): the
/// table grows to the new maximum and drops only EMPTY trailing slots beyond it; an occupied slot
/// stays until its walker releases it.
void trim_walkers(House& h, std::size_t max) {
    while (h.walkers.size() > max && !h.walkers.back().valid()) {
        h.walkers.pop_back();
    }
    if (h.walkers.size() < max) {
        h.walkers.resize(max, FigureId{});
    }
}

/// The new type's footprint equals the old one's and its group is `group`: the checks that make the
/// re-stamp a pure re-labelling (section 16).
HouseFault candidate_problem(const HouseEnv& env, const BuildingType& old_type,
                             const BuildingType& candidate, std::uint32_t group) {
    if (candidate.cls != RuntimeClass::House ||
        house_group(env.cfg, candidate.prodtype) != std::optional<std::uint32_t>{group}) {
        return HouseFault::MissingVariant;
    }
    if (candidate.width != old_type.width || candidate.height != old_type.height) {
        return HouseFault::FootprintMismatch;
    }
    return HouseFault::None;
}

/// Every type the transition can end on: `base` itself (no variants) or base + idx * step.
HouseFault validate_targets(const HouseEnv& env, const BuildingType& old_type,
                            const BuildingType& base, std::uint32_t group) {
    if (const HouseFault f = candidate_problem(env, old_type, base, group); f != HouseFault::None) {
        return f;
    }
    const VariantFacts vf = house_variant_facts(base);
    for (std::uint32_t idx = 0; idx < vf.count; ++idx) {
        const std::uint64_t id = std::uint64_t{base.id.v} + std::uint64_t{idx} * vf.step;
        const BuildingType* t =
            id > 0xffffffffULL ? nullptr : env.types.find(TypeId{static_cast<std::uint32_t>(id)});
        if (t == nullptr) {
            return HouseFault::MissingVariant;
        }
        if (const HouseFault f = candidate_problem(env, old_type, *t, group);
            f != HouseFault::None) {
            return f;
        }
    }
    return HouseFault::None;
}

/// The derived house count of the old group must be non-zero and the new group inside the table:
/// a break of the derived index is reported before anything changes, never clamped.
HouseFault count_problem(const City& city, std::uint32_t old_group, std::uint32_t new_group) {
    if (old_group >= city.index.house_counts.size() ||
        new_group >= city.index.house_counts.size() || city.index.house_counts[old_group] == 0) {
        return HouseFault::CountMismatch;
    }
    return HouseFault::None;
}

HouseOutcome fault_outcome(HouseFault f) {
    HouseOutcome o;
    o.kind = HouseOutcomeKind::Fault;
    o.fault = f;
    return o;
}

/// The shared tail of Upgrade and Downgrade after the checks: the variant draw, counters, swap,
/// SetGroup result, walkers, house counts and the re-stamp. `base` is the next/previous type.
HouseOutcome transition(const HouseEnv& env, Island& island, const BuildingKey& key, Located& loc,
                        const BuildingType& base, std::uint32_t old_group, std::uint32_t new_group,
                        const SetGroupResult& sg) {
    HouseOutcome out;
    out.from = loc.type->id;
    out.old_group = old_group;
    out.new_group = new_group;
    // The variant draw (property 0x32 of the NEW type): before SetGroup.
    const BuildingType* target = &base;
    const VariantFacts vf = house_variant_facts(base);
    if (vf.count != 0) {
        const std::uint16_t r = env.draw.next(DrawSite::HouseVariant);
        out.variant_drawn = true;
        out.variant_draw = r;
        out.variant_index = house_variant_index(vf, r);
        target = env.types.find(
            TypeId{base.id.v + out.variant_index * vf.step}); // validated by validate_targets
    }
    House& h = *loc.house;
    City& city = *loc.city;
    out.counter_sub = tdiv(h.residents_64, 64);
    --city.index.house_counts[old_group]; // checked non-zero before anything changed
    // detach old type, attach the new one, SetGroup (stock, E, residents), walker slots.
    loc.building->type = target->id;
    h.stock = sg.stock;
    h.e = sg.e;
    h.residents_64 = sg.residents_64;
    out.first_assignment_draws = sg.draws;
    trim_walkers(h, house_walker_facts(*target).walker_max);
    out.counter_add = tdiv(h.residents_64, 64); // read AFTER SetGroup
    ++city.index.house_counts[new_group]; // checked inside the table before anything changed
    // The re-stamp of the footprint tiles with the new type (0x1002c790 -> 0x1008ec90 ->
    // 0x10082040).
    std::vector<std::uint64_t> written;
    static_cast<void>(place_object_record(island.tiles, env.types, *target, loc.building->x,
                                          loc.building->y, loc.building->rotation, 0,
                                          loc.building->byte0f, &written));
    invalidate_claims(island, env.rules, written);
    out.to = target->id;
    static_cast<void>(key);
    return out;
}

} // namespace

const char* house_fault_name(HouseFault fault) {
    switch (fault) {
    case HouseFault::None:
        return "none";
    case HouseFault::NoSuchHouse:
        return "no_such_house";
    case HouseFault::UnknownType:
        return "unknown_type";
    case HouseFault::MissingGroup:
        return "missing_group";
    case HouseFault::StockSizeMismatch:
        return "stock_size_mismatch";
    case HouseFault::ZeroDivisor:
        return "zero_divisor";
    case HouseFault::MissingSlot:
        return "missing_slot";
    case HouseFault::FootprintMismatch:
        return "footprint_mismatch";
    case HouseFault::MissingVariant:
        return "missing_variant";
    case HouseFault::IdSecCycle:
        return "idsec_cycle";
    case HouseFault::Overflow:
        return "overflow";
    case HouseFault::NotEvaluated:
        return "not_evaluated";
    case HouseFault::NoSerialContext:
        return "no_serial_context";
    case HouseFault::GroupNotAdjacent:
        return "group_not_adjacent";
    case HouseFault::CountMismatch:
        return "count_mismatch";
    }
    return "unknown";
}

TypeLadder house_type_ladder(const SimConfig& cfg, const TypeRegistry& types,
                             const BuildingType& type) {
    TypeLadder l;
    const std::uint64_t stride = cfg.house_type_stride;
    const std::uint64_t id = type.id.v;
    const std::uint64_t base = id - id % stride;
    const auto find = [&](std::uint64_t v) -> const BuildingType* {
        return v > 0xffffffffULL ? nullptr : types.find(TypeId{static_cast<std::uint32_t>(v)});
    };
    l.next = find(base + stride);
    if (base >= stride) {
        l.previous = find(base - stride);
        if (l.previous == nullptr) {
            l.previous = find(base - stride + 1);
        }
    }
    return l;
}

VariantFacts house_variant_facts(const BuildingType& type) {
    VariantFacts v;
    if (type.head_raw.size() > 11) {
        v.count = type.head_raw[11] & 0x3fU;
        v.step = static_cast<std::uint32_t>(type.head_raw[11] >> 6);
    }
    return v;
}

std::vector<std::uint32_t> house_material_amounts(const SimConfig& cfg, const BuildingType& base) {
    std::vector<std::uint32_t> a(cfg.build_goods_count, 0);
    for (const CostEntry& c : base.cost_goods) {
        if (c.goods >= cfg.build_goods_first && c.goods - cfg.build_goods_first < a.size()) {
            a[c.goods - cfg.build_goods_first] = c.amount_88;
        }
    }
    return a;
}

bool house_materials_available(const SimConfig& cfg, const City& city,
                               std::span<const std::uint32_t> amounts) {
    for (std::size_t k = 0; k < amounts.size(); ++k) {
        if (amounts[k] == 0) {
            continue;
        }
        const std::uint64_t g = std::uint64_t{cfg.build_goods_first} + k;
        if (g >= city.stock.stock.size() || k >= city.build_goods.size()) {
            return false;
        }
        const std::int64_t have =
            std::int64_t{city.stock.stock[g]} - std::int64_t{city.build_goods[k].setting};
        if (std::int64_t{amounts[k]} > have) {
            return false;
        }
    }
    return true;
}

void house_spend_materials(const SimConfig& cfg, City& city,
                           std::span<const std::uint32_t> amounts) {
    for (std::size_t k = 0; k < amounts.size(); ++k) {
        if (amounts[k] == 0) {
            continue;
        }
        city.build_goods[k].spent += amounts[k];
        std::uint32_t& have = city.stock.stock[cfg.build_goods_first + k];
        have -= std::min(amounts[k], have);
    }
}

UpgradeScores upgrade_scores(const PopulationView& cur, const PopulationView& next, std::uint32_t e,
                             std::span<const std::uint32_t> stock) {
    UpgradeScores r;
    if (cur.group->no_upgrade) {
        return r;
    }
    if (cur.group->needs.empty()) {
        r.fault = HouseFault::MissingGroup;
        return r;
    }
    if (stock.size() != cur.group->needs.size()) {
        r.fault = HouseFault::StockSizeMismatch;
        return r;
    }
    if (next.group->needs.empty()) {
        r.fault = HouseFault::MissingGroup;
        return r;
    }
    if (e == 0 || next.group->needs[0].menge == 0) {
        r.fault = HouseFault::ZeroDivisor;
        return r;
    }
    const auto score = [&](std::size_t slot, std::uint32_t menge) {
        return std::min<std::int64_t>(
            tdiv(std::int64_t{stock[slot]} * 1536, std::int64_t{e} * menge), 128);
    };
    r.food = score(0, next.group->needs[0].menge);
    std::int64_t sum = 0, count = 0;
    r.infra = 160;
    const std::size_t goods_end =
        std::min<std::size_t>(next.derived->goods_end, next.group->needs.size());
    const std::size_t infra_end =
        std::min<std::size_t>(next.derived->infra_end, next.group->needs.size());
    const auto slot_of = [&](GoodsId ware) -> std::size_t {
        return ware < cur.derived->lookup.size() ? cur.derived->lookup[ware]
                                                 : cur.derived->need_count;
    };
    for (std::size_t j = 1; j < goods_end; ++j) {
        const HouseNeed& n = next.group->needs[j];
        if (!n.force) {
            continue;
        }
        const std::size_t slot = slot_of(n.ware);
        if (slot >= cur.group->needs.size()) {
            r.fault = HouseFault::MissingSlot;
            return r;
        }
        if (cur.group->needs[slot].menge == 0) {
            r.fault = HouseFault::ZeroDivisor;
            return r;
        }
        sum += score(slot, cur.group->needs[slot].menge);
        ++count;
    }
    for (std::size_t j = goods_end; j < infra_end; ++j) {
        const HouseNeed& n = next.group->needs[j];
        if (!n.force) {
            continue;
        }
        const std::size_t slot = slot_of(n.ware);
        if (slot >= cur.group->needs.size()) {
            r.fault = HouseFault::MissingSlot;
            return r;
        }
        if (cur.group->needs[slot].menge == 0) {
            r.fault = HouseFault::ZeroDivisor;
            return r;
        }
        const std::int64_t s = score(slot, cur.group->needs[slot].menge);
        r.infra = std::min(r.infra, s);
        if (n.ware == cur.roles.tavern_goods) {
            sum += s;
            ++count;
        }
    }
    r.mean = count != 0 ? tdiv(sum, count) : 128;
    r.f = next.group->nahrung[static_cast<std::size_t>((r.food + 4) >> 3)];
    r.b = next.group->bedarf[static_cast<std::size_t>((r.mean + 4) >> 3)];
    r.i = next.group->infra[static_cast<std::size_t>((r.infra + 4) >> 3)];
    r.eligible = r.f > 0 && r.b > 0 && r.i > 0;
    return r;
}

SetGroupResult house_set_group(DrawSource& draw, std::span<const GoodsInfo> goods,
                               const PopulationView* old_group, const PopulationView& new_group,
                               std::uint32_t e, std::int32_t residents_64,
                               std::span<const std::uint32_t> stock) {
    SetGroupResult r;
    r.e = e;
    r.residents_64 = residents_64;
    const std::vector<HouseNeed>& needs = new_group.group->needs;
    std::vector<std::uint8_t> flag(goods.size(), 0);
    std::vector<std::int64_t> sat(goods.size(), 0);
    for (const HouseNeed& n : needs) {
        if (n.ware >= goods.size()) {
            r.fault = HouseFault::MissingGroup;
            return r;
        }
        flag[n.ware] = 1;
    }
    bool first_assignment = false;
    bool downgrade = false;
    if (old_group != nullptr) {
        const std::vector<HouseNeed>& old = old_group->group->needs;
        if (stock.size() != old.size()) {
            r.fault = HouseFault::StockSizeMismatch;
            return r;
        }
        const std::size_t count = std::min<std::size_t>(old_group->derived->infra_end, old.size());
        for (std::size_t j = 0; j < count; ++j) {
            const GoodsId ware = old[j].ware;
            if (ware >= goods.size()) {
                r.fault = HouseFault::MissingGroup;
                return r;
            }
            const std::int64_t d = std::int64_t{e} * old[j].menge;
            if (d == 0) {
                r.fault = HouseFault::ZeroDivisor;
                return r;
            }
            const std::int64_t s =
                std::min<std::int64_t>(tdiv(std::int64_t{stock[j]} * 1536, d), 160);
            if (flag[ware] != 0) {
                sat[ware] = s;
                flag[ware] = 0;
            } else if (s > sat[ware]) {
                sat[ware] = s;
            }
            if (s >= 102) {
                GoodsId next = goods[ware].id_sec;
                std::size_t steps = 0;
                while (next < goods.size() && flag[next] != 0) {
                    if (++steps > goods.size()) {
                        r.fault = HouseFault::IdSecCycle;
                        return r;
                    }
                    flag[next] = 0;
                    sat[next] = s;
                    next = goods[next].id_sec;
                }
            }
        }
        downgrade = old_group->group->number > new_group.group->number;
        if (downgrade) {
            const std::size_t ncount =
                std::min<std::size_t>(new_group.derived->infra_end, needs.size());
            for (std::size_t j = 0; j < ncount; ++j) {
                if (flag[needs[j].ware] != 0) {
                    sat[needs[j].ware] = 102;
                    flag[needs[j].ware] = 0;
                }
            }
            const std::int64_t residents = tdiv(residents_64, 64);
            if (residents < 0 || (residents << 7) > kU32Max) {
                r.fault = HouseFault::Overflow;
                return r;
            }
            r.e = static_cast<std::uint32_t>(residents << 7);
        }
    } else {
        first_assignment = new_group.group->number != 0;
    }
    if (first_assignment) {
        const std::int64_t wohn = new_group.group->wohnmax;
        const std::size_t ncount =
            std::min<std::size_t>(new_group.derived->infra_end, needs.size());
        for (std::size_t j = 0; j < ncount; ++j) {
            if (needs[j].force) {
                const std::uint16_t draw_value = draw.next(DrawSite::HouseFirstAssignment);
                ++r.draws;
                sat[needs[j].ware] = 128 - (draw_value & 31);
                flag[needs[j].ware] = 0;
            }
        }
        r.e = static_cast<std::uint32_t>(wohn << 7);
        r.residents_64 = static_cast<std::int32_t>(wohn << 6);
    }
    r.stock.assign(needs.size(), 0);
    for (std::size_t j = 0; j < needs.size(); ++j) {
        const HouseNeed& n = needs[j];
        std::int64_t v = 0;
        if (flag[n.ware] != 0) {
            v = j == 0 ? tdiv(std::int64_t{r.e} * n.menge, 12) : 0;
        } else {
            v = tdiv(std::int64_t{n.menge} * sat[n.ware] * r.e, 1536);
        }
        if (v < 0 || v > kU32Max) {
            r.fault = HouseFault::Overflow;
            r.stock.clear();
            return r;
        }
        r.stock[j] = static_cast<std::uint32_t>(v);
    }
    return r;
}

std::int64_t residents_at_or_above(const SimConfig& cfg, const TypeRegistry& types,
                                   const City& city, std::uint32_t group) {
    std::int64_t total = 0;
    for (const BuildingList& list : city.index.lists) {
        for (const auto& slot : list.slots) {
            if (!slot.has_value()) {
                continue;
            }
            const auto* h = std::get_if<House>(&slot->payload);
            const BuildingType* t = h != nullptr ? types.find(slot->type) : nullptr;
            const std::optional<std::uint32_t> g =
                t != nullptr ? house_group(cfg, t->prodtype) : std::nullopt;
            if (g && *g >= group) {
                total += tdiv(h->residents_64, 64);
            }
        }
    }
    return total;
}

HouseOutcome upgrade_house(const HouseEnv& env, Island& island, const BuildingKey& key) {
    Located loc = locate(env, island, key);
    if (loc.fault != HouseFault::None) {
        return fault_outcome(loc.fault);
    }
    const TypeLadder ladder = house_type_ladder(env.cfg, env.types, *loc.type);
    if (ladder.next == nullptr) {
        return HouseOutcome{};
    }
    std::uint32_t old_group = 0, new_group = 0;
    PopulationView cur, next;
    if (const HouseFault f = view_of(env, *loc.type, old_group, cur); f != HouseFault::None) {
        return fault_outcome(f);
    }
    if (const HouseFault f = view_of(env, *ladder.next, new_group, next); f != HouseFault::None) {
        return fault_outcome(f);
    }
    if (!house_group_adjacent(old_group, new_group, true)) {
        return fault_outcome(HouseFault::GroupNotAdjacent);
    }
    if (const HouseFault f = count_problem(*loc.city, old_group, new_group); f != HouseFault::None) {
        return fault_outcome(f);
    }
    // The materials gate (0x1002c030) of the BASE next type; a failure changes nothing at all.
    const std::vector<std::uint32_t> amounts = house_material_amounts(env.cfg, *ladder.next);
    if (!house_materials_available(env.cfg, *loc.city, amounts)) {
        HouseOutcome o;
        o.kind = HouseOutcomeKind::UpgradeRefusedMaterials;
        o.from = loc.type->id;
        return o;
    }
    if (const HouseFault f = validate_targets(env, *loc.type, *ladder.next, new_group);
        f != HouseFault::None) {
        return fault_outcome(f);
    }
    // SetGroup is computed before anything is spent (an upgrade has an old group: no draw).
    const SetGroupResult sg =
        house_set_group(env.draw, env.types.goods_table(), &cur, next, loc.house->e,
                        loc.house->residents_64, loc.house->stock);
    if (sg.fault != HouseFault::None) {
        return fault_outcome(sg.fault);
    }
    house_spend_materials(env.cfg, *loc.city, amounts);
    HouseOutcome out = transition(env, island, key, loc, *ladder.next, old_group, new_group, sg);
    out.kind = HouseOutcomeKind::Upgraded;
    return out;
}

HouseOutcome downgrade_house(const HouseEnv& env, Island& island, const BuildingKey& key) {
    Located loc = locate(env, island, key);
    if (loc.fault != HouseFault::None) {
        return fault_outcome(loc.fault);
    }
    const TypeLadder ladder = house_type_ladder(env.cfg, env.types, *loc.type);
    if (ladder.previous == nullptr) {
        return HouseOutcome{};
    }
    std::uint32_t old_group = 0, new_group = 0;
    PopulationView cur, prev;
    if (const HouseFault f = view_of(env, *loc.type, old_group, cur); f != HouseFault::None) {
        return fault_outcome(f);
    }
    if (const HouseFault f = view_of(env, *ladder.previous, new_group, prev);
        f != HouseFault::None) {
        return fault_outcome(f);
    }
    if (!house_group_adjacent(old_group, new_group, false)) {
        return fault_outcome(HouseFault::GroupNotAdjacent);
    }
    if (const HouseFault f = count_problem(*loc.city, old_group, new_group); f != HouseFault::None) {
        return fault_outcome(f);
    }
    if (const HouseFault f = validate_targets(env, *loc.type, *ladder.previous, new_group);
        f != HouseFault::None) {
        return fault_outcome(f);
    }
    const SetGroupResult sg =
        house_set_group(env.draw, env.types.goods_table(), &cur, prev, loc.house->e,
                        loc.house->residents_64, loc.house->stock);
    if (sg.fault != HouseFault::None) {
        return fault_outcome(sg.fault);
    }
    HouseOutcome out =
        transition(env, island, key, loc, *ladder.previous, old_group, new_group, sg);
    out.kind = HouseOutcomeKind::Downgraded;
    return out;
}

HouseOutcome remove_house(const HouseEnv& env, Island& island, const BuildingKey& key) {
    Located loc = locate(env, island, key);
    if (loc.fault != HouseFault::None) {
        return fault_outcome(loc.fault);
    }
    HouseOutcome out;
    out.from = loc.type->id;
    if (const auto g = house_group(env.cfg, loc.type->prodtype)) {
        out.old_group = *g;
    }
    out.counter_sub = tdiv(loc.house->residents_64, 64);
    // The walkers are detached, not ended (0x10075bc0): the slot is cleared, the figure's home
    // becomes none and it lives on.
    for (FigureId& slot : loc.house->walkers) {
        detach_walker(env, slot, out.walkers_detached);
        slot = FigureId{};
    }
    if (!remove_building(env.cfg, env.rules, env.types, env.players, island, key)) {
        return fault_outcome(HouseFault::NoSuchHouse);
    }
    out.kind = HouseOutcomeKind::Removed;
    return out;
}

HouseOutcome apply_house_effects(const HouseEnv& env, Island& island, const BuildingKey& key,
                                 const HouseKernelResult& kernel) {
    if (kernel.status == HouseKernelStatus::Skipped) {
        return HouseOutcome{};
    }
    if (kernel.status == HouseKernelStatus::NotEvaluated) {
        HouseOutcome o = fault_outcome(HouseFault::NotEvaluated);
        o.not_evaluated = kernel.reason;
        return o;
    }
    Located loc = locate(env, island, key);
    if (loc.fault != HouseFault::None) {
        return fault_outcome(loc.fault);
    }
    if (!kernel.stock.empty() && kernel.stock.size() != loc.house->stock.size()) {
        return fault_outcome(HouseFault::StockSizeMismatch);
    }
    // Commit the evaluated numbers (steps 1-5 and the cap/correction of step 6).
    if (kernel.last) {
        loc.house->last = *kernel.last;
    }
    if (kernel.e) {
        loc.house->e = *kernel.e;
    }
    if (!kernel.stock.empty()) {
        loc.house->stock = kernel.stock;
    }
    if (kernel.residents_64) {
        loc.house->residents_64 = *kernel.residents_64;
    }
    switch (kernel.effect) {
    case DeferredEffect::None:
        return HouseOutcome{};
    case DeferredEffect::Downgrade:
        return downgrade_house(env, island, key);
    case DeferredEffect::Remove:
        return remove_house(env, island, key);
    case DeferredEffect::UpgradeCheck:
        break;
    }
    const TypeLadder ladder = house_type_ladder(env.cfg, env.types, *loc.type);
    if (ladder.next == nullptr) {
        return HouseOutcome{};
    }
    std::uint32_t g0 = 0, g1 = 0;
    PopulationView cur, next;
    if (const HouseFault f = view_of(env, *loc.type, g0, cur); f != HouseFault::None) {
        return fault_outcome(f);
    }
    if (const HouseFault f = view_of(env, *ladder.next, g1, next); f != HouseFault::None) {
        return fault_outcome(f);
    }
    const UpgradeScores scores = upgrade_scores(cur, next, loc.house->e, loc.house->stock);
    if (scores.fault != HouseFault::None) {
        HouseOutcome o = fault_outcome(scores.fault);
        o.scores = scores;
        return o;
    }
    if (!scores.eligible) {
        HouseOutcome o;
        o.scores = scores;
        return o;
    }
    HouseOutcome o = upgrade_house(env, island, key);
    o.scores = scores;
    return o;
}

} // namespace anno::sim
