// Validation, layout and digests of the configuration (spec: docs/sim/state.md
// §3, §3.1). validate() functions are pure and total: they return an empty
// string for a valid value and otherwise one message that starts with the
// name of the first offending field ("SimConfig.building_div: ...").
#include <algorithm>
#include <set>
#include <utility>

#include <anno/sim/config.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/walk_cells.hpp>

namespace anno::sim {

namespace {

std::string bad(const char* owner, const char* field, const std::string& what) {
    return std::string(owner) + "." + field + ": " + what;
}

} // namespace

// ---- LinkTables

std::vector<std::uint8_t> LinkTables::original_ore_exclusion() {
    // D AnnoScene 0x10009130: kind k is refused for a link that has ore e when the byte at
    // 0x1003AF40 + 7·k + e is 0xFF (a 7-byte stride read 9 wide). The bytes from 0x1003AF40
    // give, row k = the kind to place, column e = a kind the link has (1 = refused):
    return {
        1, 0, 0, 0, 0, 0, 0, 0, 1, // k 0: e 0, 8
        0, 1, 1, 1, 1, 1, 1, 0, 0, // k 1: e 1..6
        0, 0, 1, 0, 0, 0, 0, 1, 1, // k 2: e 2, 7, 8
        1, 1, 1, 1, 1, 1, 1, 0, 1, // k 3: e 0..6, 8
        0, 1, 1, 1, 1, 1, 1, 0, 0, // k 4: e 1..6
        0, 0, 1, 0, 0, 1, 0, 0, 1, // k 5: e 2, 5, 8
        0, 1, 1, 1, 1, 1, 1, 0, 0, // k 6: e 1..6
        0, 0, 0, 0, 0, 0, 0, 0, 0, // k 7: none (no 0xFF in 0x1003AF71..0x1003AF79)
        0, 0, 0, 0, 0, 0, 0, 0, 0, // k 8: none
    };
}

namespace {

/// `n` copies of one row of four widths.
void rank_rows(std::vector<std::uint32_t>& out, std::size_t n, std::uint32_t a, std::uint32_t b,
               std::uint32_t c, std::uint32_t d) {
    for (std::size_t i = 0; i < n; ++i) {
        out.insert(out.end(), {a, b, c, d});
    }
}

} // namespace

std::vector<std::uint32_t> LinkTables::original_raw_width_rank() {
    // D AnnoScene 0x1003AF74 + 16·round (u32 widths; read at 0x10009210–0x10009224).
    std::vector<std::uint32_t> t;
    rank_rows(t, 1, 256, 128, 0, 0);    // round 0
    rank_rows(t, 1, 256, 128, 80, 80);  // round 1
    rank_rows(t, 4, 80, 128, 256, 64);  // rounds 2..5
    rank_rows(t, 19, 64, 80, 128, 256); // rounds 6..24
    return t;
}

std::vector<std::uint32_t> LinkTables::original_ore_width_rank() {
    // D AnnoScene 0x1003B144 + 16·round (read at 0x10009400–0x10009429).
    std::vector<std::uint32_t> t;
    rank_rows(t, 1, 256, 128, 0, 0);    // round 0
    rank_rows(t, 1, 256, 128, 80, 80);  // round 1
    rank_rows(t, 13, 128, 256, 80, 64); // rounds 2..14
    rank_rows(t, 10, 64, 80, 128, 256); // rounds 15..24
    return t;
}

std::string LinkTables::validate() const {
    constexpr const char* kOwner = "LinkTables";
    if (raw_kinds == 0 || raw_kinds > 32) {
        return bad(kOwner, "raw_kinds", "must be 1..32 (a shift count into a u32 mask)");
    }
    const std::pair<const char*, std::uint32_t> non_zero[] = {{"ore_kinds", ore_kinds},
                                                              {"rounds", rounds},
                                                              {"best_list_cap", best_list_cap},
                                                              {"pick_list_cap", pick_list_cap}};
    for (const auto& [name, value] : non_zero) {
        if (value == 0) {
            return bad(kOwner, name, "must be non-zero");
        }
    }
    if (ore_mask_bit.size() != ore_kinds) {
        return bad(kOwner, "ore_mask_bit", "needs ore_kinds entries");
    }
    for (const std::uint32_t b : ore_mask_bit) {
        if (b >= 32) {
            return bad(kOwner, "ore_mask_bit", "an entry is not below 32");
        }
    }
    for (const std::uint32_t m : ore_climate_mask) {
        if (m > 0xff) {
            return bad(kOwner, "ore_climate_mask",
                       "an entry is above 0xFF (the original tests a byte)");
        }
    }
    if (static_cast<std::uint64_t>(ore_exclusion.size()) !=
        static_cast<std::uint64_t>(ore_kinds) * ore_kinds) {
        return bad(kOwner, "ore_exclusion", "needs ore_kinds x ore_kinds entries");
    }
    // The original adds the score bonuses in a byte (raw: + 1, 0x10009236; ore: + 2,
    // 0x10009449 and 0x1000946c): scores that stay below 256 keep that sum exact.
    const std::pair<const char*, const std::vector<std::uint32_t>*> scores[] = {
        {"raw_rank_score", &raw_rank_score}, {"ore_rank_score", &ore_rank_score}};
    for (const auto& [name, s] : scores) {
        if (s->empty()) {
            return bad(kOwner, name, "must not be empty");
        }
        for (const std::uint32_t v : *s) {
            if (v > 253) {
                return bad(kOwner, name, "an entry is above 253 (the original sums in a byte)");
            }
        }
    }
    if (static_cast<std::uint64_t>(raw_width_rank.size()) !=
        static_cast<std::uint64_t>(rounds) * raw_rank_score.size()) {
        return bad(kOwner, "raw_width_rank", "needs rounds x raw_rank_score entries");
    }
    if (static_cast<std::uint64_t>(ore_width_rank.size()) !=
        static_cast<std::uint64_t>(rounds) * ore_rank_score.size()) {
        return bad(kOwner, "ore_width_rank", "needs rounds x ore_rank_score entries");
    }
    return {};
}

// ---- ListLayout

std::optional<ListId> ListLayout::list_for(std::uint32_t prodtype) const {
    if (block_size == 0 || prodtype < first_prodtype) {
        return std::nullopt;
    }
    const std::uint64_t block = (prodtype - first_prodtype) / block_size;
    if (block >= block_to_list.size()) {
        return std::nullopt;
    }
    return block_to_list[static_cast<std::size_t>(block)];
}

ListLayout ListLayout::original() {
    ListLayout l;
    // L0..L5 in the original's object order (index + 0x58 + 0x20·i); the masks
    // are the FindByTile / rect query bits (D: 0x10023c80, 0x10023f50).
    l.lists = {{"stall", 0x20}, {"public", 0x01},     {"military", 0x10},
               {"house", 0x04}, {"production", 0x08}, {"market", 0x02}};
    // D: scheduler 0x10023a50 visits begin +0x108, +0x88, +0xe8, +0xc8, +0xa8;
    // L0 (+0x68) is never visited.
    l.schedule = {5, 1, 4, 3, 2};
    // D: FindByTile set order, masks 8, 1, 2, 4, 0x20, 0x10.
    l.lookup_order = {4, 1, 5, 3, 0, 2};
    // D: rect query order, masks 1, 4, 8, 0x10, 0x20, 2.
    l.rect_order = {1, 3, 4, 2, 0, 5};
    // D: blocks (p - 1) >> 5 of 32 numbers: 1-32 production, 33-64 market, 65-96
    // public, 97-128 houses, 129-160 military, 161-192 stalls.
    l.block_to_list = {4, 5, 1, 3, 2, 0};
    return l;
}

std::string ListLayout::validate() const {
    constexpr const char* kOwner = "ListLayout";
    if (lists.empty() || lists.size() > 32) {
        // lookup_mask is one bit of a u32. Path to more lists: widen lookup_mask (and the
        // masks FindByTile and the rect queries build from it) to u64, then raise this bound.
        return bad(kOwner, "lists", "must hold 1..32 lists, has " + std::to_string(lists.size()));
    }
    std::uint32_t seen = 0;
    for (std::size_t i = 0; i < lists.size(); ++i) {
        const std::uint32_t m = lists[i].lookup_mask;
        const std::string at = "lists[" + std::to_string(i) + "].lookup_mask";
        if (m == 0 || (m & (m - 1)) != 0) {
            return bad(kOwner, at.c_str(), "must be a single bit");
        }
        if ((seen & m) != 0) {
            return bad(kOwner, at.c_str(), "is used by an earlier list");
        }
        seen |= m;
    }
    const std::size_t n = lists.size();
    auto check_ids = [&](const char* field, const std::vector<ListId>& ids,
                         bool no_duplicates) -> std::string {
        std::set<ListId> unique;
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] >= n) {
                return bad(kOwner, field,
                           "entry " + std::to_string(i) + " is list " + std::to_string(ids[i]) +
                               ", only " + std::to_string(n) + " lists exist");
            }
            if (!unique.insert(ids[i]).second && no_duplicates) {
                return bad(kOwner, field, "list " + std::to_string(ids[i]) + " appears twice");
            }
        }
        return {};
    };
    if (std::string p = check_ids("schedule", schedule, true); !p.empty()) {
        return p;
    }
    if (std::string p = check_ids("lookup_order", lookup_order, true); !p.empty()) {
        return p;
    }
    if (std::string p = check_ids("rect_order", rect_order, true); !p.empty()) {
        return p;
    }
    if (block_size == 0) {
        return bad(kOwner, "block_size", "must be non-zero");
    }
    if (block_to_list.empty()) {
        return bad(kOwner, "block_to_list", "must not be empty");
    }
    if (std::string p = check_ids("block_to_list", block_to_list, false); !p.empty()) {
        return p;
    }
    if (default_list >= n) {
        return bad(kOwner, "default_list",
                   "is list " + std::to_string(default_list) + ", only " + std::to_string(n) +
                       " lists exist");
    }
    const std::pair<const char*, ListId> roles_in_order[] = {
        {"roles.market_list", roles.market_list},
        {"roles.house_list", roles.house_list},
        {"roles.public_list", roles.public_list},
        {"roles.military_list", roles.military_list},
        {"roles.production_list", roles.production_list},
        {"roles.stall_list", roles.stall_list}};
    for (const auto& [name, id] : roles_in_order) {
        if (id >= n) {
            return bad(kOwner, name,
                       "is list " + std::to_string(id) + ", only " + std::to_string(n) +
                           " lists exist");
        }
    }
    return {};
}

std::uint64_t ListLayout::digest() const {
    return digest_of(*this);
}

// ---- SimConfig

std::string EconomyConfig::validate() const {
    // Shared allocation bound for untrusted configured role tables; raise
    // kEntryTableBound with resource evidence for a larger mod, not the original7/3.
    if (climate_names.empty() || climate_names.size() > kEntryTableBound) {
        return "SimConfig.economy.climate_names: empty or above entry-table bound";
    }
    std::set<std::string> names;
    for (const auto& name : climate_names) {
        if (name.empty() || name.find('\0') != std::string::npos || !names.insert(name).second)
            return "SimConfig.economy.climate_names: empty, NUL or duplicate name";
    }
    if (tax_count == 0 || tax_count > kEntryTableBound)
        return "SimConfig.economy.tax_count: must be 1..entry-table bound";
    if (period == 0 || period > kMaxStampPeriod)
        return "SimConfig.economy.period: must be 1..65535 (word stamp)";
    return {};
}

std::string SimConfig::validate() const {
    constexpr const char* kOwner = "SimConfig";
    if (auto problem = economy.validate(); !problem.empty())
        return problem;
    // Divisors, moduli and periods that are divisors of visit_count, operands
    // of `%` or of `ceil(w / chunk_edge)`.
    const std::pair<const char*, std::uint32_t> non_zero[] = {
        {"building_div", building_div},
        {"city_ring_div", city_ring_div},
        {"player_div", player_div},
        {"figure_a_div", figure_a_div},
        {"figure_b_div", figure_b_div},
        {"objmgr_div", objmgr_div},
        {"grow_period", grow_period},
        {"fauna_period", fauna_period},
        {"event300_modulus", event300_modulus},
        {"event750_modulus", event750_modulus},
        {"event750_ms", event750_ms},
        {"monopoly_ms", monopoly_ms},
        {"upkeep_slices", upkeep_slices},
        {"stat_decay_den", stat_decay_den},
        {"chunk_edge", chunk_edge},
        {"drought_steps", drought_steps},
        {"drought_phases", drought_phases},
        // The plateau needs at least one count: with 0 the counter would rest at 0, a
        // value validate_islands refuses (counter < drought_plateau_count).
        {"drought_plateau_count", drought_plateau_count}};
    for (const auto& [name, value] : non_zero) {
        if (value == 0) {
            return bad(kOwner, name, "must be non-zero");
        }
    }
    if (layout.block_size == 0) {
        return bad(kOwner, "layout.block_size", "must be non-zero");
    }
    // Sizes that reach vector::resize on an insert or a load.
    if (!valid_list_min_growth(list_min_growth)) {
        return bad(kOwner, "list_min_growth",
                   "must be in 1.." + std::to_string(kListSizeBound) + ", is " +
                       std::to_string(list_min_growth));
    }
    if (!valid_list_min_growth(figure_min_growth)) {
        return bad(kOwner, "figure_min_growth",
                   "must be in 1.." + std::to_string(kListSizeBound) + ", is " +
                       std::to_string(figure_min_growth));
    }
    if (!valid_list_initial_size(list_initial_size)) {
        return bad(kOwner, "list_initial_size",
                   "must be in 1.." + std::to_string(kListSizeBound) + ", is " +
                       std::to_string(list_initial_size));
    }
    // Every city starts with list_initial_size null slots in each of its lists (about
    // 250 bytes a slot) before a single byte backs them, and World::create makes the
    // cities of a scenario on worker threads, where bad_alloc must not be thrown: the
    // slots of one city are bounded by kEntryTableBound (the original has 6 x 1).
    if (std::uint64_t{layout.lists.size()} * list_initial_size > kEntryTableBound) {
        return bad(kOwner, "list_initial_size",
                   "lists x list_initial_size must be at most " + std::to_string(kEntryTableBound) +
                       ", is " + std::to_string(layout.lists.size()) + " x " +
                       std::to_string(list_initial_size));
    }
    // Periods compared with a u16 difference: a larger one never opens its gate.
    const std::pair<const char*, std::uint32_t> stamp_periods[] = {
        {"upkeep_period", upkeep_period},
        {"event300_gate_seconds", event300_gate_seconds},
        {"drought_plateau_period", drought_plateau_period},
        {"drought_strip_period", drought_strip_period},
        {"figure_expiry_ticks", figure_expiry_ticks}};
    for (const auto& [name, value] : stamp_periods) {
        if (value > kMaxStampPeriod) {
            return bad(kOwner, name,
                       "must be at most 65535 (a 16-bit stamp difference), is " +
                           std::to_string(value));
        }
    }
    // Table sizes. Each is at least 1 where the model needs an entry and at most
    // a build-time bound, because the loader sizes containers from them without
    // stream backing (kSlotTableBound, kEntryTableBound, kWorldEdgeBound).
    struct SizeRule {
        const char* name;
        std::uint32_t value;
        std::uint32_t min;
        std::uint32_t max;
    };
    const SizeRule sizes[] = {{"island_slots", island_slots, 1, kSlotTableBound},
                              {"city_slots", city_slots, 1, kSlotTableBound},
                              {"player_slots", player_slots, 1, kSlotTableBound},
                              {"warehouse_goods", warehouse_goods, 1, kEntryTableBound},
                              {"script_timers", script_timers, 0, kEntryTableBound},
                              {"monopoly_entries", monopoly_entries, 0, kEntryTableBound},
                              {"house_groups", house_groups, 0, kEntryTableBound},
                              {"research_entries", research_entries, 0, kEntryTableBound},
                              {"trade_lists", trade_lists, 0, kEntryTableBound},
                              {"build_goods_count", build_goods_count, 0, kEntryTableBound},
                              {"chunk_edge", chunk_edge, 1, kWorldEdgeBound}};
    for (const SizeRule& rule : sizes) {
        if (rule.value < rule.min || rule.value > rule.max) {
            return bad(kOwner, rule.name,
                       "must be in " + std::to_string(rule.min) + ".." + std::to_string(rule.max) +
                           ", is " + std::to_string(rule.value));
        }
    }
    // The saved Drought record has a u8 phase, a u8 step and a u16 counter, and
    // drought_tick compares them with these values: one that the narrow field
    // cannot reach would never end the drought. The product is formed in u64.
    if (std::uint64_t{drought_steps} * chunk_edge > kMaxDroughtStep) {
        return bad(kOwner, "drought_steps",
                   "drought_steps * chunk_edge must be at most 255 (the saved step is a u8), is " +
                       std::to_string(std::uint64_t{drought_steps} * chunk_edge));
    }
    if (drought_phases > kMaxDroughtStep) {
        return bad(kOwner, "drought_phases",
                   "must be at most 255 (the saved phase is a u8), is " +
                       std::to_string(drought_phases));
    }
    if (drought_radius / chunk_edge > kMaxDroughtRing) {
        return bad(kOwner, "drought_radius",
                   "drought_radius / chunk_edge must be at most 2147483647 (the saved ring is an "
                   "i32), is " +
                       std::to_string(drought_radius / chunk_edge));
    }
    if (drought_plateau_count > kMaxDroughtCounter) {
        return bad(kOwner, "drought_plateau_count",
                   "must be at most 65535 (the saved counter is a u16), is " +
                       std::to_string(drought_plateau_count));
    }
    if (host_player.v >= player_slots) {
        return bad(kOwner, "host_player", "must be below player_slots");
    }
    if (neutral_owner > city_slots) {
        // The city scans run over the slots 0..neutral_owner-1 (A5).
        return bad(kOwner, "neutral_owner", "must be at most city_slots");
    }
    // The population roles (stream 127): the order only; the goods domain is checked with the
    // registry. A tavern outside the infrastructure range is refused on purpose.
    if (population_infra_first > population_infra_last) {
        return bad(kOwner, "population_infra_first",
                   "must not exceed population_infra_last, is " +
                       std::to_string(population_infra_first) + " > " +
                       std::to_string(population_infra_last));
    }
    if (population_tavern_goods < population_infra_first ||
        population_tavern_goods > population_infra_last) {
        return bad(kOwner, "population_tavern_goods",
                   "must lie in population_infra_first..population_infra_last, is " +
                       std::to_string(population_tavern_goods));
    }
    if (population_tavern_alias_goods >= population_infra_first &&
        population_tavern_alias_goods <= population_infra_last) {
        return bad(kOwner, "population_tavern_alias_goods",
                   "must lie outside the infrastructure range, is " +
                       std::to_string(population_tavern_alias_goods));
    }
    if (billed_goods_first > billed_goods_last) {
        return bad(kOwner, "billed_goods_first",
                   "must not exceed billed_goods_last, is " + std::to_string(billed_goods_first) +
                       " > " + std::to_string(billed_goods_last));
    }
    if (house_type_stride < 1) {
        return bad(kOwner, "house_type_stride", "must be at least 1");
    }
    if (resident_cargo_slots < 1) {
        return bad(kOwner, "resident_cargo_slots", "must be at least 1");
    }
    if (std::uint64_t{build_goods_first} + build_goods_count > warehouse_goods) {
        return bad(kOwner, "build_goods_count",
                   "build_goods_first + build_goods_count must not exceed warehouse_goods");
    }
    if (monopoly_raw_slots > std::min<std::uint32_t>(monopoly_entries, 32)) {
        // s is a shift count into a u32 mask and an index into the table. Path to more raw
        // slots: widen the mask to u64 and raise the 32.
        return bad(kOwner, "monopoly_raw_slots",
                   "must be at most min(monopoly_entries, 32), is " +
                       std::to_string(monopoly_raw_slots));
    }
    if (service_bits > 32) {
        // Path to more counters: widen BuildingType::service_mask to u64 and raise this bound.
        return bad(kOwner, "service_bits",
                   "must be at most 32 (BuildingType::service_mask is u32)");
    }
    if (monopoly_climate_mask.empty()) {
        return bad(kOwner, "monopoly_climate_mask", "must hold at least one climate");
    }
    // Index tables.
    const SmallFarmAnimals& animals = small_farm_animals;
    if (animals.kind_by_draw.empty()) {
        return bad(kOwner, "small_farm_animals.kind_by_draw",
                   "must not be empty (its size is the modulus of the draw)");
    }
    const std::uint32_t base = animals.base;
    for (std::size_t i = 0; i < animals.kind_by_draw.size(); ++i) {
        const std::uint32_t kind = animals.kind_by_draw[i];
        if (kind < base || kind - base >= animals.limit.size()) {
            return bad(kOwner, "small_farm_animals.kind_by_draw",
                       "entry " + std::to_string(i) + " is kind " + std::to_string(kind) +
                           ", outside base " + std::to_string(base) + " .. base + " +
                           std::to_string(animals.limit.size()) + " (limit entries)");
        }
    }
    if (std::string p = layout.validate(); !p.empty()) {
        return p;
    }
    if (std::string p = links.validate(); !p.empty()) {
        return "SimConfig.links: " + p;
    }
    if (city_pass_count > 64) {
        // A sanity bound for the seam vector (Seams::city_pass is sized from it).
        return bad(kOwner, "city_pass_count",
                   "must be at most 64, is " + std::to_string(city_pass_count));
    }
    return {};
}

std::string SimConfig::validate(const TypeRegistry& types) const {
    if (auto e = validate(); !e.empty()) {
        return e;
    }
    if (!types.figure_types().empty() && !types.walk_tables()) {
        return "SimConfig.walk_tables: missing for walking figures";
    }
    if (types.walk_tables()) {
        if (auto e = validate_walk_tables(*types.walk_tables()); !e.empty()) {
            return e;
        }
        for (const auto& figure : types.figure_types()) {
            if (figure.layer >= types.walk_tables()->layers.size()) {
                return "SimConfig.walk_tables: figure layer outside tables";
            }
        }
    }
    return {};
}

std::uint64_t SimConfig::digest() const {
    return digest_of(*this);
}

// ---- WorldSettings

std::string WorldSettings::validate() const {
    return validate(SimConfig{});
}

std::string WorldSettings::validate(const SimConfig& config) const {
    constexpr const char* kOwner = "WorldSettings";
    if (world_width > kWorldEdgeBound) {
        return bad(kOwner, "world_width",
                   "must be at most " + std::to_string(kWorldEdgeBound) + ", is " +
                       std::to_string(world_width));
    }
    if (world_height > kWorldEdgeBound) {
        return bad(kOwner, "world_height",
                   "must be at most " + std::to_string(kWorldEdgeBound) + ", is " +
                       std::to_string(world_height));
    }
    if (prop10 >= config.economy.tax_count) {
        return bad(kOwner, "prop10",
                   "outside SimConfig.economy.tax_count, is " + std::to_string(prop10));
    }
    return {};
}

// ---- Rules

std::string CargoRules::problem() const {
    for (std::size_t i = 0; i < warehouse_prodtypes.size(); ++i) {
        if (warehouse_prodtypes[i] == 0 ||
            (i > 0 && warehouse_prodtypes[i - 1] >= warehouse_prodtypes[i])) {
            return "warehouse_prodtypes: must be ascending, unique and non-zero";
        }
    }
    if (dock_half_widths.empty()) {
        return "dock_half_widths: is empty";
    }
    if (dock_half_widths.size() > std::size_t{kMaxDockRadius} + 1) {
        return "dock_half_widths: more than " + std::to_string(kMaxDockRadius + 1) + " entries";
    }
    if (dock_half_widths[0] != dock_half_widths.size() - 1) {
        return "dock_half_widths: entry 0 is not the radius (size - 1)";
    }
    for (std::size_t i = 1; i < dock_half_widths.size(); ++i) {
        if (dock_half_widths[i] > dock_half_widths[i - 1]) {
            return "dock_half_widths: increases at entry " + std::to_string(i);
        }
    }
    return {};
}

std::string Rules::validate() const {
    constexpr const char* kOwner = "Rules";
    // Absolute maxima are derived from ClientRecord's u16 payload width. A
    // particular definition must also satisfy the combined byte-size bound.
    if (freight.max_stops == 0 || freight.max_stops > (65535U - 12U) / 20U ||
        freight.max_actions == 0 || freight.max_actions > (65535U - 32U) / 13U ||
        freight.work_per_visit == 0)
        return bad(kOwner, "freight", "invalid definition or per-visit work budget");
    if (city_booking && !save_billed_goods)
        return bad(kOwner, "save_billed_goods", "city_booking requires persisted pending bills");
    if (!valid_slot_policy(slot_policy)) {
        return bad(kOwner, "slot_policy", "is not an enumerator");
    }
    const auto spatial = static_cast<std::uint8_t>(spatial_order);
    if (spatial_order == SpatialOrder::Original0x10027000) {
        return bad(kOwner, "spatial_order", "reserved: not implemented (O-7)");
    }
    if (spatial_order != SpatialOrder::YXSlot) {
        return bad(kOwner, "spatial_order",
                   "is not an enumerator (" + std::to_string(spatial) + ")");
    }
    if (prng_mode == PrngMode::PerIsland) {
        return bad(kOwner, "prng_mode", "reserved: not implemented (needs stage S16)");
    }
    if (prng_mode != PrngMode::Global) {
        return bad(kOwner, "prng_mode",
                   "is not an enumerator (" + std::to_string(static_cast<std::uint8_t>(prng_mode)) +
                       ")");
    }
    for (std::size_t i = 0; i < founding_types.size(); ++i) {
        if (founding_types[i] == 0 || (i > 0 && founding_types[i - 1] >= founding_types[i])) {
            return bad(kOwner, "founding_types", "must be ascending, unique and non-zero");
        }
    }
    {
        std::vector<std::uint32_t> all;
        for (std::size_t g = 0; g < shared_visibility.size(); ++g) {
            const std::vector<std::uint32_t>& group = shared_visibility[g];
            if (group.size() < 2) {
                return bad(kOwner, "shared_visibility",
                           "group " + std::to_string(g) + " has fewer than two members");
            }
            for (std::size_t i = 1; i < group.size(); ++i) {
                if (group[i - 1] >= group[i]) {
                    return bad(kOwner, "shared_visibility",
                               "group " + std::to_string(g) + " is not ascending and unique");
                }
            }
            all.insert(all.end(), group.begin(), group.end());
        }
        std::sort(all.begin(), all.end());
        if (std::adjacent_find(all.begin(), all.end()) != all.end()) {
            return bad(kOwner, "shared_visibility", "a player is in more than one group");
        }
    }
    {
        const NavRules& n = nav;
        const auto range = [&](const char* field, std::uint64_t v, std::uint64_t lo,
                               std::uint64_t hi) {
            return v < lo || v > hi ? bad("NavRules", field,
                                          std::to_string(v) + " is not in " + std::to_string(lo) +
                                              " .. " + std::to_string(hi))
                                    : std::string();
        };
        // Reasons for the bounds (each is a validation limit, not game data; a mod needing more
        // widens the bound together with the dependent width): run_cap and retry_cap fit the
        // node count/u8 plan fields, goal_radius_tiles and window_radius_tiles keep the coarse
        // window inside a 16-bit cell grid, unit_cap bounds a query's obstacle list, window_blocks
        // bounds the coarse grid memory, the budgets and memory_bytes stay below 2^40 so the
        // admission arithmetic in u64 cannot overflow.
        for (const std::string& problem :
             {range("plans_per_advance", n.plans_per_advance, 1, 65535),
              range("run_cap", n.run_cap, 1, 255), range("retry_cap", n.retry_cap, 1, 64),
              range("goal_radius_tiles", n.goal_radius_tiles, 1, 32),
              range("unit_cap", n.unit_cap, 1, 1024),
              range("window_radius_tiles", n.window_radius_tiles, 1, 8192),
              range("window_blocks", n.window_blocks, 1, 256),
              range("coarse_budget", n.coarse_budget, 1, std::uint64_t{1} << 40),
              range("fine_budget", n.fine_budget, 1, std::uint64_t{1} << 40),
              range("memory_bytes", n.memory_bytes, 1, std::uint64_t{1} << 40),
              range("volk_number", n.volk_number, 0, 65535),
              range("health_span_percent", n.health_span_percent, 0, 100),
              range("health_floor_percent", n.health_floor_percent, 1, 100),
              range("cargo_penalty_percent", n.cargo_penalty_percent, 0, 100)}) {
            if (!problem.empty()) {
                return problem;
            }
        }
        // V must stay in 1 .. 100 for every hp: floor + span <= 100.
        if (n.health_span_percent + n.health_floor_percent > 100) {
            return bad("NavRules", "health_span_percent",
                       "health_span_percent + health_floor_percent exceeds 100");
        }
        // The cargo factor L is at least 100 - cargo_penalty_percent, so V is at least
        // trunc(floor * (100 - penalty) / 100): that must stay >= 1 (a zero speed budget).
        if (n.health_floor_percent * (100 - n.cargo_penalty_percent) < 100) {
            return bad("NavRules", "cargo_penalty_percent",
                       "floor * (100 - penalty) / 100 is below 1: the speed factor could reach 0");
        }
        if (n.class_costs.empty() || n.class_costs.size() > 256) {
            return bad("NavRules", "class_costs", "must hold 1 .. 256 classes");
        }
        for (std::size_t i = 0; i < n.class_costs.size(); ++i) {
            const NavClassCost& k = n.class_costs[i];
            if (k.open_orth == 0 || k.open_diag < k.open_orth || k.shore_orth == 0 ||
                k.shore_diag < k.shore_orth) {
                return bad("NavRules", "class_costs",
                           "class " + std::to_string(i) +
                               " needs positive costs with diagonal >= orthogonal");
            }
        }
    }
    if (const std::string c = cargo.problem(); !c.empty()) {
        const std::size_t colon = c.find(':');
        return bad("CargoRules", c.substr(0, colon).c_str(), c.substr(colon + 2));
    }
    for (const std::string& n : city_names) {
        if (n.find('\0') != std::string::npos) {
            return bad(kOwner, "city_names", "a name contains a NUL byte");
        }
    }
    if (events_max_dispatch == 0) {
        return bad(kOwner, "events_max_dispatch", "must be at least 1");
    }
    if (island_candidate_order == IslandCandidateOrder::Cached) {
        // The single-player original's InselCache.txt order is a property of the
        // machine that wrote the cache; no cache is read (OQ-150).
        return bad(kOwner, "island_candidate_order", "reserved: not implemented (OQ-150)");
    }
    if (island_candidate_order != IslandCandidateOrder::Sorted) {
        return bad(kOwner, "island_candidate_order",
                   "is not an enumerator (" +
                       std::to_string(static_cast<std::uint8_t>(island_candidate_order)) + ")");
    }
    if (drought_random_centre) {
        // What a random centre is (which tiles, how many draws, which draw site) is the
        // owner's decision (OQ-042); an option that is accepted and does nothing would
        // make a host or a peer believe it changed the game.
        return bad(kOwner, "drought_random_centre", "reserved: not implemented (OQ-042)");
    }
    return {};
}

std::uint64_t Rules::digest() const {
    return digest_of(*this);
}

} // namespace anno::sim
