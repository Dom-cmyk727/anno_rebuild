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

namespace anno::sim {

namespace {

std::string bad(const char* owner, const char* field, const std::string& what) {
    return std::string(owner) + "." + field + ": " + what;
}

} // namespace

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

std::string SimConfig::validate() const {
    constexpr const char* kOwner = "SimConfig";
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
        {"drought_phases", drought_phases}};
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
    if (!valid_list_initial_size(list_initial_size)) {
        return bad(kOwner, "list_initial_size",
                   "must be in 1.." + std::to_string(kListSizeBound) + ", is " +
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
    if (city_pass_count > 64) {
        // A sanity bound for the seam vector (Seams::city_pass is sized from it).
        return bad(kOwner, "city_pass_count",
                   "must be at most 64, is " + std::to_string(city_pass_count));
    }
    return {};
}

std::uint64_t SimConfig::digest() const {
    return digest_of(*this);
}

// ---- WorldSettings

std::string WorldSettings::validate() const {
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
    if (prop10 > 2) {
        // The tax level indexes the economy's tables (documented 0..2).
        return bad(kOwner, "prop10", "must be 0..2, is " + std::to_string(prop10));
    }
    return {};
}

// ---- Rules

std::string Rules::validate() const {
    constexpr const char* kOwner = "Rules";
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
    if (events_max_dispatch == 0) {
        return bad(kOwner, "events_max_dispatch", "must be at least 1");
    }
    return {};
}

std::uint64_t Rules::digest() const {
    return digest_of(*this);
}

} // namespace anno::sim
