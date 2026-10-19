// SimConfig, Rules, ListLayout, WorldSettings and Seams (docs/sim/state.md §3,
// §3.1, §4.12, §9.3; stage S3): the original defaults, the validation of every
// field, the digests, and the header and settings scopes of the state stream.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <doctest/doctest.h>

#include <anno/sim/archive.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/state_fields.hpp>
#include <anno/sim/synthetic.hpp>
#include <anno/sim/world.hpp>

#include "world_access.hpp"

namespace sim = anno::sim;

namespace {

bool starts_with(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

/// The name of a table row's SUBCASE: its index and field, without the
/// characters that doctest filters and CTest names treat specially.
std::string subcase_name(std::size_t index, const char* field) {
    std::string name = std::to_string(index) + " " + field;
    for (char& c : name) {
        if (c == '[' || c == ']' || c == '*' || c == '?' || c == ';' || c == '\\') {
            c = '_';
        }
    }
    return name;
}

/// A little-endian byte stream written by hand, so the expected stream of the
/// header and settings scopes is transcribed from docs/sim/state.md §9.3
/// independently of the field list.
struct Stream {
    std::vector<std::uint8_t> v;
    void u8(std::uint32_t x) { v.push_back(static_cast<std::uint8_t>(x)); }
    void u16(std::uint32_t x) {
        u8(x);
        u8(x >> 8);
    }
    void u32(std::uint32_t x) {
        u16(x);
        u16(x >> 16);
    }
    void u64(std::uint64_t x) {
        u32(static_cast<std::uint32_t>(x));
        u32(static_cast<std::uint32_t>(x >> 32));
    }
    void i64(std::int64_t x) { u64(static_cast<std::uint64_t>(x)); }
    void boolean(bool b) { u8(b ? 1U : 0U); }
    void str(const std::string& s) {
        u64(s.size());
        for (const char c : s) {
            u8(static_cast<std::uint8_t>(c));
        }
    }
    void u32s(const std::vector<std::uint32_t>& list) {
        u64(list.size());
        for (const std::uint32_t x : list) {
            u32(x);
        }
    }
};

/// §9.3 row 1 for the original configuration: SimConfig, then Rules, with the
/// values of docs/sim/state.md §3.
void expected_config(Stream& s) {
    for (const std::uint32_t x : {256U, 16U, 15U,   32U, 31U, 5U,   28U, 20U, 32U, 5U,
                                  65U,  56U, 0x29U, 5U,  2U,  255U, 8U,  1U,  10U, 8U}) {
        s.u32(x); // island_slots .. city_pass_count
    }
    for (const std::uint32_t x : {500U, 50U, 50U, 250U, 50U, 5000U, 2000U, 15000U, 7500U, 300U, 20U,
                                  450000U, 10U, 30000U}) {
        s.u32(x); // building_div .. monopoly_ms
    }
    for (const std::uint32_t x : {2000U, 15U, 104U, 105U}) {
        s.u32(x); // upkeep_period .. stat_decay_den
    }
    s.i64(-3000);
    s.u32(150000);
    for (const std::uint32_t x : {0x32U, 1000U, 120U, 50U, 8U, 4U}) {
        s.u32(x); // drought_radius .. drought_phases
    }
    s.u32s({0x18000, 0xf009, 0xf949, 0xf06b, 0xf6b9, 0xf04d, 0xf04d});
    s.boolean(false); // multiplayer
    s.u32(0);         // host_player
    s.u64(8);         // small_farm_animals.kind_by_draw
    for (const std::uint32_t x : {107U, 110U, 107U, 109U, 107U, 109U, 107U, 109U}) {
        s.u16(x);
    }
    s.u16(106); // base
    s.u64(8);   // limit
    for (const std::uint32_t x : {1U, 2U, 0U, 1U, 1U, 0U, 0U, 32U}) {
        s.u8(x);
    }
    s.u64(6); // layout.lists
    const std::pair<const char*, std::uint32_t> lists[] = {{"stall", 0x20},      {"public", 0x01},
                                                           {"military", 0x10},   {"house", 0x04},
                                                           {"production", 0x08}, {"market", 0x02}};
    for (const auto& [name, mask] : lists) {
        s.str(name);
        s.u32(mask);
    }
    s.u32s({5, 1, 4, 3, 2});
    s.u32s({4, 1, 5, 3, 0, 2});
    s.u32s({1, 3, 4, 2, 0, 5});
    s.u32s({4, 5, 1, 3, 2, 0});
    s.u32(1);  // first_prodtype
    s.u32(32); // block_size
    s.u32(4);  // default_list
    for (const std::uint32_t x : {5U, 3U, 1U, 2U, 4U, 0U}) {
        s.u32(x); // roles: market, house, public, military, production, stall
    }
    // Rules
    s.u8(0); // slot_policy
    s.u8(0); // spatial_order
    s.boolean(false);
    s.u64(1ULL << 20); // events_max_dispatch
    for (int i = 0; i < 6; ++i) {
        s.boolean(
            false); // flush, monopoly_every_30s, full tick, drought centre, atomic, empty city
    }
    s.boolean(false); // event750_keep_overshoot
    s.u8(0);          // prng_mode
    s.u32(0);         // interim_figure_return_ticks
    s.boolean(false); // free_build_cheat
    s.boolean(false); // enforce_research
    s.boolean(false); // stat_decay_wide
    s.boolean(false); // bankrupt_deadline_wrap_safe
}

void expected_settings(Stream& s) {
    s.u32(0);
    s.u32(0); // world size
    s.u8(0);
    s.u8(1);
    s.u8(2);
    s.u16(2550);
    s.boolean(true);
    s.u32(0);
    s.u32(0);
    s.u8(0);
    s.u8(0);
    s.u32(0);
    s.boolean(false);
    s.boolean(false);
    s.u32(3);
    s.u32(100);
}

std::vector<std::uint8_t> save_fields(const sim::World& w) {
    sim::WriteArchive wa;
    fields(wa, w);
    return wa.take();
}

/// The byte offset at which the scope `name` opens, according to a trace.
std::size_t scope_offset(const sim::World& w, const char* name) {
    sim::TraceArchive trace;
    fields(trace, w);
    std::size_t offset = 0;
    for (const sim::TraceEntry& e : trace.entries()) {
        if (e.kind == sim::Prim::ScopeOpen && e.name == name) {
            return offset;
        }
        offset += static_cast<std::size_t>(e.width);
    }
    return offset;
}

/// A function of exactly the type of a seam, doing nothing (and returning a
/// value-initialised result): lets a test install every seam without writing
/// fourteen signatures.
template <class Fn> struct SeamStub;
template <class R, class... Args> struct SeamStub<R (*)(Args...)> {
    static R call(Args...) {
        if constexpr (!std::is_void_v<R>) {
            return R{};
        }
    }
};

template <class Fn> void install(sim::Seam<Fn>& seam, const char* name) {
    seam.fn = &SeamStub<Fn>::call;
    seam.name = name;
}

// The mutation tables hold plain function pointers (every mutation is a captureless
// lambda): GCC 13 at -O3 -Wnull-dereference reports a false positive when a vector of
// pairs with a std::function is grown (std::function move constructor).
using ConfigMut = void (*)(sim::SimConfig&);

/// One mutation per member of SimConfig, in declaration order: each must move
/// the digest, which is how a member missing from the field list is found.
std::vector<std::pair<const char*, ConfigMut>> config_mutations() {
    std::vector<std::pair<const char*, ConfigMut>> m;
#define BUMP(field)                                                                                \
    m.emplace_back(                                                                                \
        #field, [](sim::SimConfig& c) { c.field = static_cast<decltype(c.field)>(c.field + 1); })
    BUMP(island_slots);
    BUMP(city_slots);
    BUMP(neutral_owner);
    BUMP(player_slots);
    BUMP(neutral_player);
    BUMP(script_timers);
    BUMP(monopoly_entries);
    BUMP(monopoly_raw_slots);
    BUMP(service_bits);
    BUMP(house_groups);
    BUMP(research_entries);
    BUMP(warehouse_goods);
    BUMP(build_goods_first);
    BUMP(build_goods_count);
    BUMP(trade_lists);
    BUMP(trade_limit_max);
    BUMP(chunk_edge);
    BUMP(list_initial_size);
    BUMP(list_min_growth);
    BUMP(city_pass_count);
    BUMP(building_div);
    BUMP(city_ring_div);
    BUMP(player_div);
    BUMP(figure_a_div);
    BUMP(figure_b_div);
    BUMP(objmgr_div);
    BUMP(grow_period);
    BUMP(fauna_period);
    BUMP(figure_expiry_ticks);
    BUMP(event300_gate_seconds);
    BUMP(event300_modulus);
    BUMP(event750_ms);
    BUMP(event750_modulus);
    BUMP(monopoly_ms);
    BUMP(upkeep_period);
    BUMP(upkeep_slices);
    BUMP(stat_decay_num);
    BUMP(stat_decay_den);
    BUMP(bankrupt_money);
    BUMP(bankrupt_grace_ticks);
    BUMP(drought_radius);
    BUMP(drought_plateau_period);
    BUMP(drought_plateau_count);
    BUMP(drought_strip_period);
    BUMP(drought_steps);
    BUMP(drought_phases);
#undef BUMP
    m.emplace_back("monopoly_climate_mask",
                   [](sim::SimConfig& c) { c.monopoly_climate_mask.push_back(1); });
    m.emplace_back("multiplayer", [](sim::SimConfig& c) { c.multiplayer = !c.multiplayer; });
    m.emplace_back("host_player", [](sim::SimConfig& c) { c.host_player = sim::PlayerId{1}; });
    m.emplace_back("small_farm_animals",
                   [](sim::SimConfig& c) { c.small_farm_animals.kind_by_draw[0] += 1; });
    m.emplace_back("layout", [](sim::SimConfig& c) { c.layout.schedule.pop_back(); });
    return m;
}

using RulesMut = void (*)(sim::Rules&);

std::vector<std::pair<const char*, RulesMut>> rules_mutations() {
    std::vector<std::pair<const char*, RulesMut>> m;
    m.emplace_back("slot_policy",
                   [](sim::Rules& r) { r.slot_policy = sim::SlotPolicy::LowestFree; });
    m.emplace_back("spatial_order",
                   [](sim::Rules& r) { r.spatial_order = sim::SpatialOrder::Original0x10027000; });
#define FLIP(field) m.emplace_back(#field, [](sim::Rules& r) { r.field = !r.field; })
    FLIP(events_drain_new);
    m.emplace_back("events_max_dispatch", [](sim::Rules& r) { r.events_max_dispatch += 1; });
    FLIP(events_flush_after_figures);
    FLIP(monopoly_every_30s);
    FLIP(player_table_full_tick);
    FLIP(drought_random_centre);
    FLIP(build_atomic);
    FLIP(keep_empty_city);
    FLIP(event750_keep_overshoot);
#undef FLIP
    m.emplace_back("prng_mode", [](sim::Rules& r) { r.prng_mode = sim::PrngMode::PerIsland; });
    m.emplace_back("interim_figure_return_ticks",
                   [](sim::Rules& r) { r.interim_figure_return_ticks += 1; });
    m.emplace_back("free_build_cheat", [](sim::Rules& r) { r.free_build_cheat = true; });
    m.emplace_back("enforce_research", [](sim::Rules& r) { r.enforce_research = true; });
    m.emplace_back("stat_decay_wide", [](sim::Rules& r) { r.stat_decay_wide = true; });
    m.emplace_back("bankrupt_deadline_wrap_safe",
                   [](sim::Rules& r) { r.bankrupt_deadline_wrap_safe = true; });
    return m;
}

using SettingsMut = void (*)(sim::WorldSettings&);

std::vector<std::pair<const char*, SettingsMut>> settings_mutations() {
    std::vector<std::pair<const char*, SettingsMut>> m;
#define BUMP(field)                                                                                \
    m.emplace_back(#field, [](sim::WorldSettings& s) {                                             \
        s.field = static_cast<decltype(s.field)>(s.field + 1);                                     \
    })
    BUMP(world_width);
    BUMP(world_height);
    BUMP(prop0);
    BUMP(prop1);
    BUMP(prop2);
    BUMP(prop3);
    m.emplace_back("prop5", [](sim::WorldSettings& s) { s.prop5 = !s.prop5; });
    BUMP(prop6);
    BUMP(prop7_offset);
    BUMP(prop9);
    BUMP(prop10);
    BUMP(prop11);
    m.emplace_back("prop12", [](sim::WorldSettings& s) { s.prop12 = !s.prop12; });
    m.emplace_back("prop13", [](sim::WorldSettings& s) { s.prop13 = !s.prop13; });
    BUMP(prop14);
    BUMP(speed_percent);
#undef BUMP
    return m;
}

/// One mutation per member of ListLayout (the nested roles one by one).
std::vector<std::pair<const char*, void (*)(sim::ListLayout&)>> layout_mutations() {
    using L = sim::ListLayout;
    std::vector<std::pair<const char*, void (*)(L&)>> m;
    m.emplace_back("lists.name", [](L& l) { l.lists[0].name = "other"; });
    m.emplace_back("lists.lookup_mask", [](L& l) { l.lists[0].lookup_mask = 0x40; });
    m.emplace_back("lists.count", [](L& l) { l.lists.push_back({"extra", 0x80}); });
    m.emplace_back("schedule", [](L& l) { l.schedule = {1, 5, 4, 3, 2}; });
    m.emplace_back("lookup_order", [](L& l) { l.lookup_order = {1, 4, 5, 3, 0, 2}; });
    m.emplace_back("rect_order", [](L& l) { l.rect_order = {3, 1, 4, 2, 0, 5}; });
    m.emplace_back("block_to_list", [](L& l) { l.block_to_list = {5, 4, 1, 3, 2, 0}; });
    m.emplace_back("first_prodtype", [](L& l) { l.first_prodtype = 0; });
    m.emplace_back("block_size", [](L& l) { l.block_size = 16; });
    m.emplace_back("default_list", [](L& l) { l.default_list = 3; });
    m.emplace_back("roles.market_list", [](L& l) { l.roles.market_list = 4; });
    m.emplace_back("roles.house_list", [](L& l) { l.roles.house_list = 4; });
    m.emplace_back("roles.public_list", [](L& l) { l.roles.public_list = 4; });
    m.emplace_back("roles.military_list", [](L& l) { l.roles.military_list = 4; });
    m.emplace_back("roles.production_list", [](L& l) { l.roles.production_list = 5; });
    m.emplace_back("roles.stall_list", [](L& l) { l.roles.stall_list = 5; });
    return m;
}

} // namespace

TEST_CASE("list layout original maps") {
    const sim::ListLayout l = sim::ListLayout::original();
    REQUIRE(l.lists.size() == 6);
    const char* const names[] = {"stall", "public", "military", "house", "production", "market"};
    const std::uint32_t masks[] = {0x20, 0x01, 0x10, 0x04, 0x08, 0x02};
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(l.lists[i].name == names[i]);
        CHECK(l.lists[i].lookup_mask == masks[i]);
    }
    // D: scheduler 0x10023a50, FindByTile 0x10023c80, rect query 0x10023f50.
    CHECK(l.schedule == std::vector<sim::ListId>{5, 1, 4, 3, 2});
    CHECK(l.lookup_order == std::vector<sim::ListId>{4, 1, 5, 3, 0, 2});
    CHECK(l.rect_order == std::vector<sim::ListId>{1, 3, 4, 2, 0, 5});
    CHECK(l.block_to_list == std::vector<sim::ListId>{4, 5, 1, 3, 2, 0});
    CHECK(l.first_prodtype == 1);
    CHECK(l.block_size == 32);
    CHECK(l.default_list == 4);
    CHECK(l.validate().empty());
    // The masks in the orders the spec lists: lookup 8, 1, 2, 4, 0x20, 0x10;
    // rect 1, 4, 8, 0x10, 0x20, 2.
    std::vector<std::uint32_t> lookup_masks, rect_masks;
    for (const sim::ListId id : l.lookup_order) {
        lookup_masks.push_back(l.lists[id].lookup_mask);
    }
    for (const sim::ListId id : l.rect_order) {
        rect_masks.push_back(l.lists[id].lookup_mask);
    }
    CHECK(lookup_masks == std::vector<std::uint32_t>{8, 1, 2, 4, 0x20, 0x10});
    CHECK(rect_masks == std::vector<std::uint32_t>{1, 4, 8, 0x10, 0x20, 2});
    // L0 (the stalls) is never scheduled.
    for (const sim::ListId id : l.schedule) {
        CHECK(id != 0);
    }
}

TEST_CASE("list layout original roles") {
    const sim::ListLayout l = sim::ListLayout::original();
    CHECK(l.roles.market_list == 5);
    CHECK(l.roles.house_list == 3);
    CHECK(l.roles.public_list == 1);
    CHECK(l.roles.military_list == 2);
    CHECK(l.roles.production_list == 4);
    CHECK(l.roles.stall_list == 0);
    // The roles name the lists they play.
    CHECK(l.lists[l.roles.market_list].name == "market");
    CHECK(l.lists[l.roles.house_list].name == "house");
    CHECK(l.lists[l.roles.public_list].name == "public");
    CHECK(l.lists[l.roles.military_list].name == "military");
    CHECK(l.lists[l.roles.production_list].name == "production");
    CHECK(l.lists[l.roles.stall_list].name == "stall");
    // A default-constructed Roles has the same values (the declaration defaults).
    const sim::ListLayout::Roles defaults;
    CHECK(defaults.market_list == l.roles.market_list);
    CHECK(defaults.stall_list == l.roles.stall_list);
}

TEST_CASE("list_for over 0 to 416") {
    const sim::ListLayout l = sim::ListLayout::original();
    // Blocks of 32 PRODTYPE numbers (D: the Create tables 0x1002604c, 0x10026034):
    // 1-32 production, 33-64 market, 65-96 public, 97-128 houses, 129-160
    // military, 161-192 stalls; 0 and everything above is not a building.
    struct Range {
        std::uint32_t lo, hi;
        sim::ListId list;
    };
    const Range ranges[] = {{1, 32, 4},   {33, 64, 5},   {65, 96, 1},
                            {97, 128, 3}, {129, 160, 2}, {161, 192, 0}};
    for (std::uint32_t p = 0; p <= 416; ++p) {
        const auto got = l.list_for(p);
        std::optional<sim::ListId> want;
        for (const Range& r : ranges) {
            if (p >= r.lo && p <= r.hi) {
                want = r.list;
            }
        }
        INFO("prodtype " << p);
        REQUIRE(got.has_value() == want.has_value());
        if (want) {
            CHECK(*got == *want);
        }
    }
    // Spot checks by name: HAUPT 33, KONTOR 35 -> market, PIONIER 97 ->
    // houses, WACHTURM 129 -> military, MARKTSTAND 161 -> stalls.
    CHECK(*l.list_for(35) == l.roles.market_list);
    CHECK(*l.list_for(97) == l.roles.house_list);
    CHECK(*l.list_for(129) == l.roles.military_list);
    CHECK(*l.list_for(161) == l.roles.stall_list);
    CHECK(*l.list_for(1) == l.roles.production_list);
    CHECK(*l.list_for(77) == l.roles.public_list);
    // A modded layout: first_prodtype and block_size are data.
    sim::ListLayout m = l;
    m.first_prodtype = 10;
    m.block_size = 4;
    m.block_to_list = {1, 2};
    CHECK_FALSE(m.list_for(9).has_value());
    CHECK(*m.list_for(10) == 1);
    CHECK(*m.list_for(13) == 1);
    CHECK(*m.list_for(14) == 2);
    CHECK(*m.list_for(17) == 2);
    CHECK_FALSE(m.list_for(18).has_value());
    // A zero block size is not a division by zero.
    m.block_size = 0;
    CHECK_FALSE(m.list_for(10).has_value());
    // The difference is computed without wrapping near the top of the range.
    sim::ListLayout top = l;
    top.first_prodtype = 0xfffffff0U;
    top.block_size = 8;
    top.block_to_list = {2, 3};
    CHECK(*top.list_for(0xfffffff7U) == 2);
    CHECK(*top.list_for(0xfffffff8U) == 3);
    CHECK(*top.list_for(0xffffffffU) == 3);
    CHECK_FALSE(top.list_for(0xffffffefU).has_value());
}

TEST_CASE("schedule order 5 1 4 3 2") {
    const sim::SimConfig cfg;
    // market, public, production, house, military: the begin offsets +0x108,
    // +0x88, +0xe8, +0xc8, +0xa8 of the building index (D 0x10023a50).
    REQUIRE(cfg.layout.schedule.size() == 5);
    CHECK(cfg.layout.schedule[0] == cfg.layout.roles.market_list);
    CHECK(cfg.layout.schedule[1] == cfg.layout.roles.public_list);
    CHECK(cfg.layout.schedule[2] == cfg.layout.roles.production_list);
    CHECK(cfg.layout.schedule[3] == cfg.layout.roles.house_list);
    CHECK(cfg.layout.schedule[4] == cfg.layout.roles.military_list);
    CHECK(cfg.layout.schedule == std::vector<sim::ListId>{5, 1, 4, 3, 2});
}

TEST_CASE("rules defaults are faithful") {
    const sim::Rules r;
    CHECK(r.slot_policy == sim::SlotPolicy::Original);
    CHECK(r.spatial_order == sim::SpatialOrder::YXSlot);
    CHECK_FALSE(r.events_drain_new);
    CHECK(r.events_max_dispatch == sim::kEventsMaxDispatchDefault);
    CHECK(r.events_max_dispatch == (1ULL << 20));
    CHECK_FALSE(r.events_flush_after_figures);
    CHECK_FALSE(r.monopoly_every_30s);
    CHECK_FALSE(r.player_table_full_tick);
    CHECK_FALSE(r.drought_random_centre);
    CHECK_FALSE(r.build_atomic);
    CHECK_FALSE(r.keep_empty_city);
    CHECK_FALSE(r.event750_keep_overshoot);
    CHECK(r.prng_mode == sim::PrngMode::Global);
    CHECK(r.interim_figure_return_ticks == 0);
    CHECK_FALSE(r.free_build_cheat);
    CHECK_FALSE(r.enforce_research);
    CHECK_FALSE(r.stat_decay_wide);
    CHECK_FALSE(r.bankrupt_deadline_wrap_safe);
    CHECK(r.validate().empty());
    static_assert(sim::field_count<sim::Rules>() == 17);
}

TEST_CASE("sim config defaults are the originals") {
    const sim::SimConfig c;
    CHECK(c.island_slots == 256);
    CHECK(c.city_slots == 16);
    CHECK(c.neutral_owner == 15);
    CHECK(c.player_slots == 32);
    CHECK(c.neutral_player == 31);
    CHECK(c.script_timers == 5);
    CHECK(c.monopoly_entries == 28);
    CHECK(c.monopoly_raw_slots == 20);
    CHECK(c.service_bits == 32);
    CHECK(c.house_groups == 5);
    CHECK(c.research_entries == 65);
    CHECK(c.warehouse_goods == 0x38);
    CHECK(c.build_goods_first == 0x29);
    CHECK(c.build_goods_count == 5);
    CHECK(c.trade_lists == 2);
    CHECK(c.trade_limit_max == 255);
    CHECK(c.chunk_edge == 8);
    CHECK(c.list_initial_size == 1);
    CHECK(c.list_min_growth == 10);
    CHECK(c.city_pass_count == 8);
    CHECK(c.building_div == 500);
    CHECK(c.city_ring_div == 50);
    CHECK(c.player_div == 50);
    CHECK(c.figure_a_div == 250);
    CHECK(c.figure_b_div == 50);
    CHECK(c.objmgr_div == 5000);
    CHECK(c.grow_period == 2000);
    CHECK(c.fauna_period == 15000);
    CHECK(c.figure_expiry_ticks == 7500);
    CHECK(c.event300_gate_seconds == 300);
    CHECK(c.event300_modulus == 20);
    CHECK(c.event750_ms == 450000);
    CHECK(c.event750_modulus == 10);
    CHECK(c.monopoly_ms == 30000);
    CHECK(c.upkeep_period == 2000);
    CHECK(c.upkeep_slices == 15);
    CHECK(c.stat_decay_num == 104);
    CHECK(c.stat_decay_den == 105);
    CHECK(c.bankrupt_money == -3000);
    CHECK(c.bankrupt_grace_ticks == 150000);
    CHECK(c.drought_radius == 0x32);
    CHECK(c.drought_plateau_period == 1000);
    CHECK(c.drought_plateau_count == 120);
    CHECK(c.drought_strip_period == 50);
    CHECK(c.drought_steps == 8);
    CHECK(c.drought_phases == 4);
    CHECK(c.monopoly_climate_mask ==
          std::vector<std::uint32_t>{0x18000, 0xf009, 0xf949, 0xf06b, 0xf6b9, 0xf04d, 0xf04d});
    CHECK_FALSE(c.multiplayer);
    CHECK(c.host_player == sim::PlayerId{0});
    CHECK(c.small_farm_animals.kind_by_draw.size() == 8);
    CHECK(c.small_farm_animals.base == 106);
    CHECK(c.layout.schedule == sim::ListLayout::original().schedule);
    CHECK(c.validate().empty());
    // kOriginalWarehouseGoods and the city pass count the Seams default uses.
    CHECK(sim::kOriginalCityPassCount == 8);
    CHECK(sim::Seams{}.city_pass.size() == sim::kOriginalCityPassCount);
}

TEST_CASE("config digest changes with every field") {
    static_assert(sim::field_count<sim::SimConfig>() == 51);
    const sim::SimConfig base;
    const std::uint64_t base_digest = base.digest();
    CHECK(sim::SimConfig{}.digest() == base_digest);
    const auto mutations = config_mutations();
    // One mutation per member: a member without a line in the field list (or
    // a line without a mutation) breaks this count or the digest check below.
    CHECK(mutations.size() == sim::field_count<sim::SimConfig>());
    std::map<std::uint64_t, std::string> seen;
    seen[base_digest] = "base";
    for (const auto& [name, mutate] : mutations) {
        sim::SimConfig c = base;
        mutate(c);
        INFO("field " << name);
        const std::uint64_t d = c.digest();
        CHECK(d != base_digest);
        // No two single-field changes collide either (a field hashed in place
        // of another would).
        CHECK(seen.emplace(d, name).second);
    }
}

TEST_CASE("rules and layout digest change with every field") {
    const sim::Rules base;
    const auto rule_mutations = rules_mutations();
    CHECK(rule_mutations.size() == sim::field_count<sim::Rules>());
    std::map<std::uint64_t, std::string> seen;
    seen[base.digest()] = "base";
    for (const auto& [name, mutate] : rule_mutations) {
        sim::Rules r = base;
        mutate(r);
        INFO("rule " << name);
        CHECK(seen.emplace(r.digest(), name).second);
    }
    const sim::ListLayout layout = sim::ListLayout::original();
    std::map<std::uint64_t, std::string> layout_seen;
    layout_seen[layout.digest()] = "base";
    for (const auto& [name, mutate] : layout_mutations()) {
        sim::ListLayout l = layout;
        mutate(l);
        INFO("layout " << name);
        CHECK(layout_seen.emplace(l.digest(), name).second);
    }
    // The nine members of ListLayout (the roles are one) and the animals table.
    static_assert(sim::field_count<sim::ListLayout>() == 9);
    static_assert(sim::field_count<sim::ListLayout::Roles>() == 6);
    static_assert(sim::field_count<sim::ListSpec>() == 2);
    static_assert(sim::field_count<sim::SmallFarmAnimals>() == 3);
    const sim::SimConfig c;
    const std::uint64_t d = c.digest();
    sim::SimConfig a = c;
    a.small_farm_animals.base += 1;
    CHECK(a.digest() != d);
    a = c;
    a.small_farm_animals.limit[7] += 1;
    CHECK(a.digest() != d);
    a = c;
    a.small_farm_animals.kind_by_draw.push_back(107);
    CHECK(a.digest() != d);
    for (const auto& [name, mutate] : layout_mutations()) {
        a = c;
        mutate(a.layout);
        INFO("config layout " << name);
        CHECK(a.digest() != d);
    }
}

TEST_CASE("settings defaults and helpers") {
    static_assert(sim::field_count<sim::WorldSettings>() == 16);
    const sim::WorldSettings s;
    CHECK(s.world_width == 0);
    CHECK(s.world_height == 0);
    CHECK(s.prop0 == 0);
    CHECK(s.prop1 == 1);
    CHECK(s.prop2 == 2);
    CHECK(s.prop3 == 2550);
    CHECK(s.prop5);
    CHECK(s.prop6 == 0);
    CHECK(s.prop7_offset == 0);
    CHECK(s.prop9 == 0);
    CHECK(s.prop10 == 0);
    CHECK(s.prop11 == 0);
    CHECK_FALSE(s.prop12);
    CHECK_FALSE(s.prop13);
    CHECK(s.prop14 == 3);
    CHECK(s.speed_percent == 100);
    // Property 14 bit 1 (0x2) is the add-on edition (A11); a bare world is the add-on.
    CHECK(s.addon());
    CHECK(s.depletes());
    sim::WorldSettings base_game;
    base_game.prop14 = 1;
    CHECK_FALSE(base_game.addon());
    CHECK_FALSE(base_game.depletes());
    base_game.prop14 = 2;
    CHECK(base_game.addon());
    // Property 6: 0x02 volcano, 0x04 drought, 0x08 city events, 0x80 event 7.5 min.
    sim::WorldSettings p;
    CHECK_FALSE(p.volcano_off());
    CHECK_FALSE(p.drought_off());
    CHECK_FALSE(p.city_events_off());
    CHECK_FALSE(p.event750_off());
    p.prop6 = 0x02;
    CHECK(p.volcano_off());
    CHECK_FALSE(p.drought_off());
    CHECK_FALSE(p.city_events_off());
    CHECK_FALSE(p.event750_off());
    p.prop6 = 0x04;
    CHECK_FALSE(p.volcano_off());
    CHECK(p.drought_off());
    CHECK_FALSE(p.city_events_off());
    CHECK_FALSE(p.event750_off());
    p.prop6 = 0x08;
    CHECK_FALSE(p.volcano_off());
    CHECK_FALSE(p.drought_off());
    CHECK(p.city_events_off());
    CHECK_FALSE(p.event750_off());
    p.prop6 = 0x80;
    CHECK_FALSE(p.volcano_off());
    CHECK_FALSE(p.drought_off());
    CHECK_FALSE(p.city_events_off());
    CHECK(p.event750_off());
    // Bit 5 (0x20) is read by CompLogic only: none of the sim's switches.
    p.prop6 = 0x20 | 0x01 | 0x10 | 0x100;
    CHECK_FALSE(p.volcano_off());
    CHECK_FALSE(p.drought_off());
    CHECK_FALSE(p.city_events_off());
    CHECK_FALSE(p.event750_off());
    p.prop6 = 0xffffffffU;
    CHECK(p.volcano_off());
    CHECK(p.drought_off());
    CHECK(p.city_events_off());
    CHECK(p.event750_off());
    // Property 3 holds values the original's byte (value / 10) cannot (F8).
    p.prop3 = 5000;
    CHECK(p.prop3 == 5000);
}

TEST_CASE("config validate rejects each bad field") {
    const sim::SimConfig base;
    REQUIRE(base.validate().empty());
    struct Row {
        const char* field; // the message must start with "SimConfig.<field>:"
        std::function<void(sim::SimConfig&)> mutate; // the rows capture
    };
    std::vector<Row> rows;
    // Non-zero divisors, moduli and periods.
#define ZERO(f) rows.push_back({#f, [](sim::SimConfig& c) { c.f = 0; }})
    ZERO(building_div);
    ZERO(city_ring_div);
    ZERO(player_div);
    ZERO(figure_a_div);
    ZERO(figure_b_div);
    ZERO(objmgr_div);
    ZERO(grow_period);
    ZERO(fauna_period);
    ZERO(event300_modulus);
    ZERO(event750_modulus);
    ZERO(event750_ms);
    ZERO(monopoly_ms);
    ZERO(upkeep_slices);
    ZERO(stat_decay_den);
    ZERO(chunk_edge);
    ZERO(drought_steps);
    ZERO(drought_phases);
#undef ZERO
    rows.push_back({"layout.block_size", [](sim::SimConfig& c) { c.layout.block_size = 0; }});
    // List sizes: zero, one past the bound, the largest u32.
    for (const std::uint32_t v : {0U, sim::kListSizeBound + 1, 0xFFFFFFFFU}) {
        rows.push_back({"list_min_growth", [v](sim::SimConfig& c) { c.list_min_growth = v; }});
        rows.push_back({"list_initial_size", [v](sim::SimConfig& c) { c.list_initial_size = v; }});
    }
    // Periods compared with a 16-bit difference.
#define PERIOD(f) rows.push_back({#f, [](sim::SimConfig& c) { c.f = 65536; }})
    PERIOD(upkeep_period);
    PERIOD(event300_gate_seconds);
    PERIOD(drought_plateau_period);
    PERIOD(drought_strip_period);
    PERIOD(figure_expiry_ticks);
#undef PERIOD
    // Table sizes.
    rows.push_back({"island_slots", [](sim::SimConfig& c) { c.island_slots = 0; }});
    rows.push_back({"city_slots", [](sim::SimConfig& c) { c.city_slots = 0; }});
    rows.push_back({"player_slots", [](sim::SimConfig& c) { c.player_slots = 0; }});
    rows.push_back({"warehouse_goods", [](sim::SimConfig& c) { c.warehouse_goods = 0; }});
    rows.push_back({"host_player", [](sim::SimConfig& c) { c.host_player = sim::PlayerId{32}; }});
    rows.push_back(
        {"host_player", [](sim::SimConfig& c) { c.host_player = sim::PlayerId{0xFFFFFFFFU}; }});
    rows.push_back({"host_player", [](sim::SimConfig& c) {
                        c.player_slots = 4;
                        c.host_player = sim::PlayerId{4};
                    }});
    rows.push_back({"build_goods_count", [](sim::SimConfig& c) { c.build_goods_count = 16; }});
    rows.push_back({"build_goods_count", [](sim::SimConfig& c) {
                        c.build_goods_first = 0xFFFFFFFFU; // the sum must not wrap
                        c.build_goods_count = 5;
                    }});
    rows.push_back({"build_goods_count", [](sim::SimConfig& c) {
                        c.build_goods_first = 0;
                        c.build_goods_count = 0xFFFFFFFFU;
                    }});
    rows.push_back({"monopoly_raw_slots", [](sim::SimConfig& c) {
                        c.monopoly_entries = 40;
                        c.monopoly_raw_slots = 33;
                    }});
    rows.push_back({"monopoly_raw_slots", [](sim::SimConfig& c) {
                        c.monopoly_entries = 19;
                        c.monopoly_raw_slots = 20;
                    }});
    rows.push_back({"service_bits", [](sim::SimConfig& c) { c.service_bits = 33; }});
    rows.push_back(
        {"monopoly_climate_mask", [](sim::SimConfig& c) { c.monopoly_climate_mask.clear(); }});
    // Index tables.
    rows.push_back({"small_farm_animals.kind_by_draw",
                    [](sim::SimConfig& c) { c.small_farm_animals.kind_by_draw.clear(); }});
    rows.push_back({"small_farm_animals.kind_by_draw",
                    [](sim::SimConfig& c) { c.small_farm_animals.kind_by_draw[3] = 105; }});
    rows.push_back({"small_farm_animals.kind_by_draw",
                    [](sim::SimConfig& c) { c.small_farm_animals.kind_by_draw[0] = 114; }});
    rows.push_back({"small_farm_animals.kind_by_draw",
                    [](sim::SimConfig& c) { c.small_farm_animals.limit.resize(4); }});
    rows.push_back({"city_pass_count", [](sim::SimConfig& c) { c.city_pass_count = 65; }});
    // Table sizes the loader derives containers from without stream backing
    // (F25): one past the bound and the largest u32, per field.
#define TABLE_BOUND(f, bound)                                                                      \
    for (const std::uint32_t v : {(bound) + 1, 0xFFFFFFFFU}) {                                     \
        rows.push_back({#f, [v](sim::SimConfig& c) { c.f = v; }});                                 \
    }
    TABLE_BOUND(island_slots, sim::kSlotTableBound);
    TABLE_BOUND(city_slots, sim::kSlotTableBound);
    TABLE_BOUND(player_slots, sim::kSlotTableBound);
    TABLE_BOUND(warehouse_goods, sim::kEntryTableBound);
    TABLE_BOUND(script_timers, sim::kEntryTableBound);
    TABLE_BOUND(monopoly_entries, sim::kEntryTableBound);
    TABLE_BOUND(house_groups, sim::kEntryTableBound);
    TABLE_BOUND(research_entries, sim::kEntryTableBound);
    TABLE_BOUND(trade_lists, sim::kEntryTableBound);
    TABLE_BOUND(build_goods_count, sim::kEntryTableBound);
    TABLE_BOUND(chunk_edge, sim::kWorldEdgeBound);
#undef TABLE_BOUND
    // The city scans run over the slots below neutral_owner (A5).
    rows.push_back({"neutral_owner", [](sim::SimConfig& c) { c.neutral_owner = 17; }});
    rows.push_back({"neutral_owner", [](sim::SimConfig& c) { c.neutral_owner = 0xFFFFFFFFU; }});
    rows.push_back({"neutral_owner", [](sim::SimConfig& c) { c.city_slots = 4; }});
    // The saved Drought record: u8 step and phase, u16 counter.
    rows.push_back({"drought_steps", [](sim::SimConfig& c) { c.drought_steps = 40; }}); // 40 * 8
    rows.push_back({"drought_steps", [](sim::SimConfig& c) { c.chunk_edge = 64; }});    // 8 * 64
    rows.push_back({"drought_steps", [](sim::SimConfig& c) {
                        c.drought_steps = 0x20000000U; // 2^29 * 8 wraps u32 to 0
                    }});
    rows.push_back({"drought_steps", [](sim::SimConfig& c) { c.drought_steps = 0xFFFFFFFFU; }});
    rows.push_back({"drought_phases", [](sim::SimConfig& c) { c.drought_phases = 256; }});
    rows.push_back({"drought_phases", [](sim::SimConfig& c) { c.drought_phases = 0xFFFFFFFFU; }});
    rows.push_back(
        {"drought_plateau_count", [](sim::SimConfig& c) { c.drought_plateau_count = 65536; }});
    rows.push_back({"drought_plateau_count",
                    [](sim::SimConfig& c) { c.drought_plateau_count = 0xFFFFFFFFU; }});
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::string name = subcase_name(i, rows[i].field);
        SUBCASE(name.c_str()) {
            sim::SimConfig c = base;
            rows[i].mutate(c);
            const std::string message = c.validate();
            INFO(message);
            CHECK(starts_with(message, std::string("SimConfig.") + rows[i].field + ":"));
        }
    }
    // The layout's own problems come out as "ListLayout.<field>:".
    sim::SimConfig bad_layout = base;
    bad_layout.layout.schedule.push_back(6);
    CHECK(starts_with(bad_layout.validate(), "ListLayout.schedule:"));
}

TEST_CASE("config validate accepts the edges of each bound") {
    // The other side of every bound: the largest accepted value.
    auto accepted = [](const char* what, const ConfigMut& mutate) {
        sim::SimConfig c;
        mutate(c);
        INFO(what << ": " << c.validate());
        CHECK(c.validate().empty());
    };
    accepted("list_min_growth 1", [](sim::SimConfig& c) { c.list_min_growth = 1; });
    accepted("list_min_growth bound",
             [](sim::SimConfig& c) { c.list_min_growth = sim::kListSizeBound; });
    accepted("list_initial_size 1", [](sim::SimConfig& c) { c.list_initial_size = 1; });
    accepted("list_initial_size bound",
             [](sim::SimConfig& c) { c.list_initial_size = sim::kListSizeBound; });
    accepted("periods 65535", [](sim::SimConfig& c) {
        c.upkeep_period = 65535;
        c.event300_gate_seconds = 65535;
        c.drought_plateau_period = 65535;
        c.drought_strip_period = 65535;
        c.figure_expiry_ticks = 65535;
    });
    accepted("zero periods", [](sim::SimConfig& c) {
        c.upkeep_period = 0;
        c.drought_strip_period = 0;
        c.figure_expiry_ticks = 0;
    });
    accepted("host_player is the last slot",
             [](sim::SimConfig& c) { c.host_player = sim::PlayerId{31}; });
    accepted("build goods fill the warehouse", [](sim::SimConfig& c) {
        c.build_goods_count = 15; // 0x29 + 15 = 0x38
    });
    accepted("monopoly_raw_slots 32 of 32", [](sim::SimConfig& c) {
        c.monopoly_entries = 32;
        c.monopoly_raw_slots = 32;
    });
    accepted("monopoly_raw_slots 0", [](sim::SimConfig& c) { c.monopoly_raw_slots = 0; });
    accepted("service_bits 32 and 0", [](sim::SimConfig& c) { c.service_bits = 0; });
    accepted("city_pass_count 64", [](sim::SimConfig& c) { c.city_pass_count = 64; });
    accepted("city_pass_count 0", [](sim::SimConfig& c) { c.city_pass_count = 0; });
    accepted("the last kind of the animals table", [](sim::SimConfig& c) {
        c.small_farm_animals.kind_by_draw[0] = 113; // base 106 + 8 limit entries - 1
    });
    accepted("a one entry animals table", [](sim::SimConfig& c) {
        c.small_farm_animals.kind_by_draw = {106};
        c.small_farm_animals.limit = {1};
    });
    accepted("island_slots at the bound",
             [](sim::SimConfig& c) { c.island_slots = sim::kSlotTableBound; });
    accepted("city_slots at the bound", [](sim::SimConfig& c) {
        c.city_slots = sim::kSlotTableBound;
        c.neutral_owner = sim::kSlotTableBound;
    });
    accepted("player_slots at the bound",
             [](sim::SimConfig& c) { c.player_slots = sim::kSlotTableBound; });
    accepted("warehouse_goods at the bound",
             [](sim::SimConfig& c) { c.warehouse_goods = sim::kEntryTableBound; });
    accepted("the entry tables at the bound", [](sim::SimConfig& c) {
        c.script_timers = sim::kEntryTableBound;
        c.monopoly_entries = sim::kEntryTableBound;
        c.house_groups = sim::kEntryTableBound;
        c.research_entries = sim::kEntryTableBound;
        c.trade_lists = sim::kEntryTableBound;
    });
    accepted("build goods fill a warehouse at the bound", [](sim::SimConfig& c) {
        c.warehouse_goods = sim::kEntryTableBound;
        c.build_goods_first = 0;
        c.build_goods_count = sim::kEntryTableBound;
    });
    accepted("empty entry tables", [](sim::SimConfig& c) {
        c.script_timers = 0;
        c.monopoly_entries = 0;
        c.monopoly_raw_slots = 0;
        c.house_groups = 0;
        c.research_entries = 0;
        c.trade_lists = 0;
        c.build_goods_count = 0;
    });
    accepted("neutral_owner equals city_slots", [](sim::SimConfig& c) { c.neutral_owner = 16; });
    accepted("neutral_owner 0", [](sim::SimConfig& c) { c.neutral_owner = 0; });
    accepted("chunk_edge 255 with one step", [](sim::SimConfig& c) {
        c.chunk_edge = 255;
        c.drought_steps = 1;
    });
    accepted("drought step product 255", [](sim::SimConfig& c) {
        c.chunk_edge = 15;
        c.drought_steps = 17; // 17 * 15 = 255, the largest u8
    });
    accepted("drought phases 255 and plateau count 65535", [](sim::SimConfig& c) {
        c.drought_phases = 255;
        c.drought_plateau_count = 65535;
    });
    accepted("drought plateau count 0", [](sim::SimConfig& c) { c.drought_plateau_count = 0; });
    accepted("a modded table size", [](sim::SimConfig& c) {
        c.city_slots = 4;
        c.neutral_owner = 3;
        c.player_slots = 8;
        c.neutral_player = 7;
        c.chunk_edge = 16;
    });
}

TEST_CASE("layout validate rejects each bad id and mask") {
    const sim::ListLayout base = sim::ListLayout::original();
    REQUIRE(base.validate().empty());
    using L = sim::ListLayout;
    struct Row {
        const char* field;
        std::function<void(L&)> mutate;
    };
    std::vector<Row> rows;
    rows.push_back({"lists", [](L& l) { l.lists.clear(); }});
    rows.push_back({"lists", [](L& l) {
                        l.lists.clear();
                        for (std::uint32_t i = 0; i < 33; ++i) {
                            l.lists.push_back({"l" + std::to_string(i), 1U << (i % 32)});
                        }
                    }});
    rows.push_back({"lists[2].lookup_mask", [](L& l) { l.lists[2].lookup_mask = 0; }});
    rows.push_back({"lists[2].lookup_mask", [](L& l) { l.lists[2].lookup_mask = 0x30; }});
    rows.push_back({"lists[4].lookup_mask", [](L& l) { l.lists[4].lookup_mask = 0x20; }});
    rows.push_back({"schedule", [](L& l) { l.schedule.push_back(6); }});
    rows.push_back({"schedule", [](L& l) { l.schedule[0] = 0xFFFFFFFFU; }});
    rows.push_back({"schedule", [](L& l) { l.schedule[4] = 5; }});
    rows.push_back({"lookup_order", [](L& l) { l.lookup_order[2] = 6; }});
    rows.push_back({"lookup_order", [](L& l) { l.lookup_order[5] = l.lookup_order[0]; }});
    rows.push_back({"rect_order", [](L& l) { l.rect_order[0] = 7; }});
    rows.push_back({"rect_order", [](L& l) { l.rect_order[5] = l.rect_order[1]; }});
    rows.push_back({"block_to_list", [](L& l) { l.block_to_list[5] = 6; }});
    rows.push_back({"block_to_list", [](L& l) { l.block_to_list.clear(); }});
    rows.push_back({"block_size", [](L& l) { l.block_size = 0; }});
    rows.push_back({"default_list", [](L& l) { l.default_list = 6; }});
    rows.push_back({"roles.market_list", [](L& l) { l.roles.market_list = 6; }});
    rows.push_back({"roles.house_list", [](L& l) { l.roles.house_list = 6; }});
    rows.push_back({"roles.public_list", [](L& l) { l.roles.public_list = 6; }});
    rows.push_back({"roles.military_list", [](L& l) { l.roles.military_list = 6; }});
    rows.push_back({"roles.production_list", [](L& l) { l.roles.production_list = 6; }});
    rows.push_back({"roles.stall_list", [](L& l) { l.roles.stall_list = 0xFFFFFFFFU; }});
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::string name = subcase_name(i, rows[i].field);
        SUBCASE(name.c_str()) {
            L l = base;
            rows[i].mutate(l);
            const std::string message = l.validate();
            INFO(message);
            CHECK(starts_with(message, std::string("ListLayout.") + rows[i].field + ":"));
        }
    }
    // What the validation allows: roles may coincide (a mod may fold stalls
    // into the market list), block_to_list may repeat a list, the schedule may
    // be shorter than the lists, 32 lists are fine.
    L fold = base;
    fold.roles.stall_list = fold.roles.market_list;
    CHECK(fold.validate().empty());
    fold = base;
    fold.block_to_list = {4, 4, 4, 4, 4, 4};
    CHECK(fold.validate().empty());
    fold = base;
    fold.schedule = {5};
    fold.lookup_order.clear();
    fold.rect_order.clear();
    CHECK(fold.validate().empty());
    fold = base;
    fold.lists.clear();
    for (std::uint32_t i = 0; i < 32; ++i) {
        fold.lists.push_back({"l" + std::to_string(i), 1U << i});
    }
    CHECK(fold.validate().empty());
    // Ids are checked against the list count, so a bigger layout accepts them.
    fold.roles.market_list = 31;
    fold.schedule = {31, 0};
    CHECK(fold.validate().empty());
}

TEST_CASE("rules validate rejects reserved values") {
    sim::Rules r;
    CHECK(r.validate().empty());
    r.prng_mode = sim::PrngMode::PerIsland;
    const std::string per_island = r.validate();
    CHECK(starts_with(per_island, "Rules.prng_mode:"));
    CHECK(per_island.find("reserved") != std::string::npos);
    r = sim::Rules{};
    r.spatial_order = sim::SpatialOrder::Original0x10027000;
    const std::string spatial = r.validate();
    CHECK(starts_with(spatial, "Rules.spatial_order:"));
    CHECK(spatial.find("reserved") != std::string::npos);
    // An enum byte outside its range (a value cast from a save or a config file).
    r = sim::Rules{};
    r.slot_policy = static_cast<sim::SlotPolicy>(2);
    CHECK(starts_with(r.validate(), "Rules.slot_policy:"));
    r.slot_policy = static_cast<sim::SlotPolicy>(255);
    CHECK(starts_with(r.validate(), "Rules.slot_policy:"));
    r = sim::Rules{};
    r.spatial_order = static_cast<sim::SpatialOrder>(2);
    CHECK(starts_with(r.validate(), "Rules.spatial_order:"));
    r = sim::Rules{};
    r.prng_mode = static_cast<sim::PrngMode>(2);
    CHECK(starts_with(r.validate(), "Rules.prng_mode:"));
    r.prng_mode = static_cast<sim::PrngMode>(255);
    CHECK(starts_with(r.validate(), "Rules.prng_mode:"));
    // events_max_dispatch is a cap of at least one.
    r = sim::Rules{};
    r.events_max_dispatch = 0;
    CHECK(starts_with(r.validate(), "Rules.events_max_dispatch:"));
    r.events_max_dispatch = 1;
    CHECK(r.validate().empty());
    r.events_max_dispatch = 0xFFFFFFFFFFFFFFFFULL;
    CHECK(r.validate().empty());
    // Every other option is a valid choice, in any combination.
    r = sim::Rules{};
    r.slot_policy = sim::SlotPolicy::LowestFree;
    r.events_drain_new = true;
    r.events_flush_after_figures = true;
    r.monopoly_every_30s = true;
    r.player_table_full_tick = true;
    r.drought_random_centre = true;
    r.build_atomic = true;
    r.keep_empty_city = true;
    r.event750_keep_overshoot = true;
    r.interim_figure_return_ticks = 7500;
    r.free_build_cheat = true;
    r.enforce_research = true;
    CHECK(r.validate().empty());
}

TEST_CASE("seams names validation and defaults") {
    const sim::SimConfig cfg;
    sim::Seams s;
    CHECK(sim::validate_seams(s, cfg).empty());
    // Declaration order of the names, city_pass first (docs/sim/state.md §9.3).
    std::vector<std::string> names = sim::seam_names(s);
    CHECK(names.size() == 8 + 14);
    for (const std::string& n : names) {
        CHECK(n == "default");
    }
    // Name every seam differently and check the order.
    std::vector<std::string> labels;
    for (int i = 0; i < 22; ++i) {
        labels.push_back("s" + std::to_string(i));
    }
    std::size_t k = 0;
    auto label = [&](const char*& name) { name = labels[k++].c_str(); };
    for (auto& p : s.city_pass) {
        label(p.name);
    }
    label(s.wildlife_sweep.name);
    label(s.object_manager.name);
    label(s.drought_tiles.name);
    label(s.figure_tick.name);
    label(s.player_groups.name);
    label(s.defeat.name);
    label(s.ai.name);
    label(s.tile_cleanup.name);
    label(s.delayed_hit.name);
    label(s.on_placed.name);
    label(s.on_removed.name);
    label(s.placement_rule.name);
    label(s.upgrade_rewrite.name);
    label(s.city_is_empty.name);
    names = sim::seam_names(s);
    REQUIRE(names.size() == 22);
    for (std::size_t i = 0; i < names.size(); ++i) {
        CHECK(names[i] == "s" + std::to_string(i));
    }
    // city_pass must have exactly SimConfig::city_pass_count entries.
    sim::Seams short_list;
    short_list.city_pass.pop_back();
    CHECK(starts_with(sim::validate_seams(short_list, cfg), "Seams.city_pass:"));
    sim::Seams long_list;
    long_list.city_pass.emplace_back();
    CHECK(starts_with(sim::validate_seams(long_list, cfg), "Seams.city_pass:"));
    sim::SimConfig three;
    three.city_pass_count = 3;
    CHECK(sim::validate_seams(sim::Seams::sized_for(three), three).empty());
    CHECK(sim::Seams::sized_for(three).city_pass.size() == 3);
    CHECK_FALSE(sim::validate_seams(sim::Seams{}, three).empty());
    // An installed function needs a non-empty name; an uninstalled one does not.
    auto city_empty = [](void*, const sim::City&) { return false; };
    sim::Seams named;
    named.city_is_empty.fn = city_empty;
    named.city_is_empty.name = "";
    CHECK(starts_with(sim::validate_seams(named, cfg), "Seams.city_is_empty:"));
    named.city_is_empty.name = nullptr;
    CHECK(starts_with(sim::validate_seams(named, cfg), "Seams.city_is_empty:"));
    named.city_is_empty.name = "mine";
    CHECK(sim::validate_seams(named, cfg).empty());
    sim::Seams unnamed;
    unnamed.city_is_empty.name = "";
    CHECK(sim::validate_seams(unnamed, cfg).empty());
    sim::Seams pass;
    pass.city_pass[5].fn = [](void*, const sim::TickContext&, sim::Island&, sim::City&,
                              sim::Effects&) {};
    pass.city_pass[5].name = "";
    CHECK(starts_with(sim::validate_seams(pass, cfg), "Seams.city_pass[5]:"));
    pass.city_pass[5].name = "pass5";
    CHECK(sim::validate_seams(pass, cfg).empty());
}

TEST_CASE("every installed seam needs a name of its own") {
    const sim::SimConfig cfg;
    // One check per seam of the table (the 8 city passes and the 14 others): an
    // installed function under the name "", nullptr or "default" is refused with the
    // field of THIS seam; under a real name the table is valid. A copy-paste in
    // validate_seams (a check that looks at another seam, or none) fails here.
    auto check_row = [&](const std::string& field, const auto& install_fn) {
        INFO(field);
        for (const char* bad_name : {"", "default"}) {
            sim::Seams s;
            install_fn(s, bad_name);
            INFO("name '" << bad_name << "'");
            CHECK(starts_with(sim::validate_seams(s, cfg), "Seams." + field + ":"));
        }
        sim::Seams null_name;
        install_fn(null_name, nullptr);
        CHECK(starts_with(sim::validate_seams(null_name, cfg), "Seams." + field + ":"));
        sim::Seams named;
        install_fn(named, "mine");
        CHECK(sim::validate_seams(named, cfg).empty());
    };
    for (std::size_t i = 0; i < sim::kOriginalCityPassCount; ++i) {
        check_row("city_pass[" + std::to_string(i) + "]",
                  [i](sim::Seams& s, const char* n) { install(s.city_pass[i], n); });
    }
#define SEAM(f) check_row(#f, [](sim::Seams& s, const char* n) { install(s.f, n); })
    SEAM(wildlife_sweep);
    SEAM(object_manager);
    SEAM(drought_tiles);
    SEAM(figure_tick);
    SEAM(player_groups);
    SEAM(defeat);
    SEAM(ai);
    SEAM(tile_cleanup);
    SEAM(delayed_hit);
    SEAM(on_placed);
    SEAM(on_removed);
    SEAM(placement_rule);
    SEAM(upgrade_rewrite);
    SEAM(city_is_empty);
#undef SEAM
    // Why "default" is refused: the name is the only identity of an implementation
    // that the header hash sees, so a function under the default name would hash
    // exactly like no function, and two peers running different logic would not
    // diverge at tick 0.
    sim::World plain;
    sim::World disguised;
    install(sim::WorldTestAccess::seams(disguised).ai, "default");
    CHECK(disguised.state_hash() == plain.state_hash());
    CHECK_FALSE(sim::validate_seams(disguised.seams(), disguised.config()).empty());
    install(sim::WorldTestAccess::seams(disguised).ai, "complogic");
    CHECK(disguised.state_hash() != plain.state_hash());
    CHECK(sim::validate_seams(disguised.seams(), disguised.config()).empty());
    CHECK(std::string(sim::kDefaultSeamName) == "default");
}

TEST_CASE("settings validate rejects each bad field") {
    const sim::WorldSettings base;
    REQUIRE(base.validate().empty());
    struct Row {
        const char* field;
        void (*mutate)(sim::WorldSettings&);
    };
    const Row rows[] = {
        {"world_width", [](sim::WorldSettings& s) { s.world_width = sim::kWorldEdgeBound + 1; }},
        {"world_width", [](sim::WorldSettings& s) { s.world_width = 0xFFFFFFFFU; }},
        {"world_height", [](sim::WorldSettings& s) { s.world_height = sim::kWorldEdgeBound + 1; }},
        {"world_height", [](sim::WorldSettings& s) { s.world_height = 0xFFFFFFFFU; }},
        {"prop10", [](sim::WorldSettings& s) { s.prop10 = 3; }},
        {"prop10", [](sim::WorldSettings& s) { s.prop10 = 255; }},
    };
    for (std::size_t i = 0; i < std::size(rows); ++i) {
        const std::string name = subcase_name(i, rows[i].field);
        SUBCASE(name.c_str()) {
            sim::WorldSettings s = base;
            rows[i].mutate(s);
            const std::string message = s.validate();
            INFO(message);
            CHECK(starts_with(message, std::string("WorldSettings.") + rows[i].field + ":"));
        }
    }
    // The accepted edges: the largest world, every tax level, an empty world.
    sim::WorldSettings edge = base;
    edge.world_width = sim::kWorldEdgeBound;
    edge.world_height = sim::kWorldEdgeBound;
    CHECK(edge.validate().empty());
    for (int tax = 0; tax <= 2; ++tax) {
        edge.prop10 = static_cast<std::uint8_t>(tax);
        CHECK(edge.validate().empty());
    }
    edge = base;
    edge.world_width = 0;
    edge.world_height = 0;
    CHECK(edge.validate().empty());
    // The other properties are not constrained (their meaning is O-29).
    edge.prop0 = 255;
    edge.prop2 = 255;
    edge.prop3 = 65535;
    edge.prop6 = 0xFFFFFFFFU;
    edge.prop14 = 0xFFFFFFFFU;
    edge.speed_percent = 0xFFFFFFFFU;
    CHECK(edge.validate().empty());
}

TEST_CASE("a moved-from world keeps its registry") {
    auto synth = sim::synthetic::make_synthetic_types();
    REQUIRE(synth);
    sim::World a(3);
    sim::WorldTestAccess::set_types(a, synth.value().registry);
    const sim::TypeRegistry* registry = &a.types();
    const std::uint64_t digest = a.types().digest();
    // The registry handle is never null, as the table of a moved-from WorldRng
    // (rng.hpp): a moved-from World can still be asked for its types and hashed.
    sim::World b = std::move(a);
    CHECK(&b.types() == registry);
    CHECK(&a.types() == registry); // NOLINT: the guarantee under test
    CHECK(a.types().digest() == digest);
    static_cast<void>(a.state_hash());
    CHECK(a.rng.size() == 2048);
    sim::World c;
    c = std::move(b);
    CHECK(&c.types() == registry);
    CHECK(&b.types() == registry); // NOLINT
    static_cast<void>(b.state_hash());
    // A moved-from world can be assigned to and used again.
    b = sim::World(9);
    CHECK(b.rng.cursor() == 9);
    CHECK(&b.types() == &sim::World().types());
    // A null registry is replaced by the empty one.
    const sim::RegistryRef null_ref{nullptr};
    CHECK(&*null_ref == sim::TypeRegistry::empty().get());
}

TEST_CASE("world holds config rules settings seams and registry") {
    const sim::World w;
    CHECK(w.config().digest() == sim::SimConfig{}.digest());
    CHECK(w.rules().digest() == sim::Rules{}.digest());
    CHECK(w.settings().prop14 == 3);
    CHECK(w.seams().city_pass.size() == 8);
    CHECK(w.types().digest() == sim::TypeRegistry::empty()->digest());
    CHECK(w.types().types().empty());
    // The default world passes the checks create() and load() will run.
    CHECK(w.config().validate().empty());
    CHECK(w.rules().validate().empty());
    CHECK(sim::validate_seams(w.seams(), w.config()).empty());
    // A seed world is the same world with another cursor.
    const sim::World seeded(77);
    CHECK(seeded.rng.cursor() == 77);
    CHECK(seeded.config().digest() == w.config().digest());
    CHECK(seeded.types().digest() == w.types().digest());
    // Copies are independent snapshots; the registry is shared, not copied.
    sim::World copy = w;
    sim::WorldTestAccess::config(copy).building_div = 7;
    CHECK(w.config().building_div == 500);
    CHECK(copy.config().building_div == 7);
    CHECK(&copy.types() == &w.types());
}

TEST_CASE("state stream header and settings scopes") {
    const sim::World w;
    const std::vector<std::uint8_t> bytes = save_fields(w);
    // The whole stream, transcribed from docs/sim/state.md §9.3: header
    // (versions, SimConfig, Rules, registry digest, seam names), clock_rng,
    // settings, players (32 absent slots, stamp, cursor), islands (slot count 0).
    Stream expected;
    expected.u32(sim::kStateHashVersion);
    expected.u32(sim::kPrngTimelineVersion);
    expected_config(expected);
    expected.u64(sim::TypeRegistry::empty()->digest());
    expected.u64(22);
    for (int i = 0; i < 22; ++i) {
        expected.str("default");
    }
    const std::size_t header_size = expected.v.size();
    expected.u64(0); // clock.ms
    expected.u64(0); // clock.last_tick
    expected.u64(2048);
    expected.u64(w.rng.table_digest());
    expected.u32(0);
    expected_settings(expected);
    expected.u64(32); // players: slot count
    for (int i = 0; i < 32; ++i) {
        expected.u8(0); // an absent slot
    }
    expected.u16(0); // stamp
    expected.u32(0); // cursor
    expected.u64(0); // islands: slot count
    CHECK(bytes == expected.v);
    CHECK(sim::kStateHashVersion == 103);
    // The scopes open where the transcription says.
    CHECK(scope_offset(w, "header") == 0);
    CHECK(scope_offset(w, "clock_rng") == header_size);
    CHECK(scope_offset(w, "settings") == header_size + 36);
    CHECK(scope_offset(w, "players") == header_size + 36 + 38);
    CHECK(scope_offset(w, "islands") == header_size + 36 + 38 + 46);
    CHECK(bytes.size() == header_size + 36 + 38 + 46 + 8);
}

TEST_CASE("state hash moves with every config rules settings and seam field") {
    const sim::World base(5);
    const std::uint64_t base_hash = base.state_hash();
    sim::HashTree base_tree;
    CHECK(base.state_hash(base_tree, 0) == base_hash);
    REQUIRE(base_tree.root.children.size() == 5);
    CHECK(base_tree.root.children[0].name == "header");
    CHECK(base_tree.root.children[1].name == "clock_rng");
    CHECK(base_tree.root.children[2].name == "settings");
    CHECK(base_tree.root.children[3].name == "players");
    CHECK(base_tree.root.children[4].name == "islands");
    auto scope_hashes = [](const sim::World& w) {
        sim::HashTree t;
        static_cast<void>(w.state_hash(t, 0));
        return std::vector<std::uint64_t>{t.root.children[0].hash, t.root.children[1].hash,
                                          t.root.children[2].hash, t.root.children[3].hash};
    };
    const auto base_scopes = scope_hashes(base);
    // Config, rules, registry and seams move the header scope only.
    for (const auto& [name, mutate] : config_mutations()) {
        sim::World w = base;
        mutate(sim::WorldTestAccess::config(w));
        INFO("config " << name);
        CHECK(w.state_hash() != base_hash);
        const auto s = scope_hashes(w);
        CHECK(s[0] != base_scopes[0]);
        CHECK(s[1] == base_scopes[1]);
        CHECK(s[2] == base_scopes[2]);
        CHECK(s[3] == base_scopes[3]);
    }
    for (const auto& [name, mutate] : rules_mutations()) {
        sim::World w = base;
        mutate(sim::WorldTestAccess::rules(w));
        INFO("rule " << name);
        CHECK(w.state_hash() != base_hash);
        const auto s = scope_hashes(w);
        CHECK(s[0] != base_scopes[0]);
        CHECK(s[1] == base_scopes[1]);
        CHECK(s[2] == base_scopes[2]);
        CHECK(s[3] == base_scopes[3]);
    }
    // Settings move the settings scope only.
    for (const auto& [name, mutate] : settings_mutations()) {
        sim::World w = base;
        mutate(sim::WorldTestAccess::settings(w));
        INFO("setting " << name);
        CHECK(w.state_hash() != base_hash);
        const auto s = scope_hashes(w);
        CHECK(s[0] == base_scopes[0]);
        CHECK(s[1] == base_scopes[1]);
        CHECK(s[2] != base_scopes[2]);
        CHECK(s[3] == base_scopes[3]);
    }
    static_assert(sim::field_count<sim::WorldSettings>() == 16);
    CHECK(settings_mutations().size() == sim::field_count<sim::WorldSettings>());
    // Another registry moves the header.
    auto synth = sim::synthetic::make_synthetic_types();
    REQUIRE(synth);
    sim::World with_types = base;
    sim::WorldTestAccess::set_types(with_types, synth.value().registry);
    CHECK(with_types.state_hash() != base_hash);
    CHECK(scope_hashes(with_types)[0] != base_scopes[0]);
    CHECK(scope_hashes(with_types)[1] == base_scopes[1]);
    // So does the name of any single seam (not the function behind it).
    const std::size_t seam_count = sim::seam_names(base.seams()).size();
    CHECK(seam_count == 22);
    sim::World named = base;
    sim::WorldTestAccess::seams(named).city_pass[3].name = "economy";
    CHECK(named.state_hash() != base_hash);
    sim::World named_ai = base;
    sim::WorldTestAccess::seams(named_ai).ai.name = "complogic";
    CHECK(named_ai.state_hash() != base_hash);
    CHECK(named_ai.state_hash() != named.state_hash());
    sim::World last = base;
    sim::WorldTestAccess::seams(last).city_is_empty.name = "empty";
    CHECK(last.state_hash() != base_hash);
    // ctx and the function pointer are not state: only the name is hashed.
    sim::World with_ctx = base;
    int dummy = 0;
    sim::WorldTestAccess::seams(with_ctx).ai.ctx = &dummy;
    CHECK(with_ctx.state_hash() == base_hash);
    // The registry is identified by its digest; two worlds with the same
    // registry digest hash alike.
    auto synth2 = sim::synthetic::make_synthetic_types();
    REQUIRE(synth2);
    sim::World with_types2 = base;
    sim::WorldTestAccess::set_types(with_types2, synth2.value().registry);
    CHECK(with_types2.state_hash() == with_types.state_hash());
}

TEST_CASE("state stream header reads back and validates what it read") {
    auto read_into = [](sim::World& target, const std::vector<std::uint8_t>& in) {
        sim::ReadArchive r(in);
        fields(r, target);
        return r.failed() ? r.error() : (r.remaining() == 0 ? std::string() : "trailing");
    };
    sim::World w(1234);
    sim::WorldTestAccess::config(w).list_min_growth = 12;
    sim::WorldTestAccess::config(w).layout.roles.market_list = 4;
    sim::WorldTestAccess::rules(w).slot_policy = sim::SlotPolicy::LowestFree;
    sim::WorldTestAccess::rules(w).events_max_dispatch = 99;
    sim::WorldTestAccess::settings(w).prop14 = 1;
    sim::WorldTestAccess::settings(w).prop3 = 5000;
    w.advance(10);
    const std::vector<std::uint8_t> bytes = save_fields(w);
    SUBCASE("a fresh world reads it back to the same hash and bytes") {
        sim::World other;
        CHECK(read_into(other, bytes) == "");
        CHECK(other.state_hash() == w.state_hash());
        CHECK(other.config().list_min_growth == 12);
        CHECK(other.config().layout.roles.market_list == 4);
        CHECK(other.rules().slot_policy == sim::SlotPolicy::LowestFree);
        CHECK(other.rules().events_max_dispatch == 99);
        CHECK(other.settings().prop14 == 1);
        CHECK(other.settings().prop3 == 5000);
        CHECK(save_fields(other) == bytes);
    }
    SUBCASE("every prefix fails") {
        for (std::size_t n = 0; n < bytes.size(); ++n) {
            sim::World other;
            const std::vector<std::uint8_t> prefix(bytes.begin(),
                                                   bytes.begin() + static_cast<std::ptrdiff_t>(n));
            INFO("prefix " << n);
            CHECK(read_into(other, prefix) != "");
        }
    }
    SUBCASE("a config with a zero divisor is refused in the header") {
        sim::World bad_world = w;
        sim::WorldTestAccess::config(bad_world).building_div = 0;
        sim::World other;
        const std::string err = read_into(other, save_fields(bad_world));
        CHECK(starts_with(err, "header"));
        CHECK(err.find("SimConfig.building_div") != std::string::npos);
    }
    SUBCASE("every non-zero divisor is refused when read as zero") {
        for (const char* field : {"building_div", "chunk_edge", "grow_period", "upkeep_slices"}) {
            sim::World bad_world = w;
            sim::SimConfig& c = sim::WorldTestAccess::config(bad_world);
            if (std::string(field) == "building_div") {
                c.building_div = 0;
            } else if (std::string(field) == "chunk_edge") {
                c.chunk_edge = 0;
            } else if (std::string(field) == "grow_period") {
                c.grow_period = 0;
            } else {
                c.upkeep_slices = 0;
            }
            sim::World other;
            const std::string err = read_into(other, save_fields(bad_world));
            INFO(field << ": " << err);
            CHECK(err.find(std::string("SimConfig.") + field) != std::string::npos);
        }
    }
    SUBCASE("a layout with a list id beyond the lists is refused") {
        sim::World bad_world = w;
        sim::WorldTestAccess::config(bad_world).layout.schedule[0] = 99;
        sim::World other;
        const std::string err = read_into(other, save_fields(bad_world));
        CHECK(err.find("ListLayout.schedule") != std::string::npos);
    }
    SUBCASE("a reserved rule is refused") {
        sim::World bad_world = w;
        sim::WorldTestAccess::rules(bad_world).prng_mode = sim::PrngMode::PerIsland;
        sim::World other;
        const std::string err = read_into(other, save_fields(bad_world));
        CHECK(starts_with(err, "header"));
        CHECK(err.find("Rules.prng_mode") != std::string::npos);
    }
    SUBCASE("an enum byte out of range is refused by the archive") {
        sim::World bad_world = w;
        sim::WorldTestAccess::rules(bad_world).slot_policy = static_cast<sim::SlotPolicy>(9);
        sim::World other;
        const std::string err = read_into(other, save_fields(bad_world));
        CHECK(starts_with(err, "header"));
        CHECK(err.find("enum") != std::string::npos);
    }
    SUBCASE("a stream made with another registry is refused") {
        auto synth = sim::synthetic::make_synthetic_types();
        REQUIRE(synth);
        sim::World with_types = w;
        sim::WorldTestAccess::set_types(with_types, synth.value().registry);
        sim::World other; // the empty registry
        const std::string err = read_into(other, save_fields(with_types));
        CHECK(starts_with(err, "header"));
        CHECK(err.find("registry") != std::string::npos);
        // The same stream into a world that has that registry reads.
        sim::World matching;
        sim::WorldTestAccess::set_types(matching, synth.value().registry);
        CHECK(read_into(matching, save_fields(with_types)) == "");
    }
    SUBCASE("a stream made with other seam names is refused") {
        sim::World named = w;
        sim::WorldTestAccess::seams(named).tile_cleanup.name = "cleanup";
        sim::World other;
        const std::string err = read_into(other, save_fields(named));
        CHECK(starts_with(err, "header"));
        CHECK(err.find("seam") != std::string::npos);
        sim::World matching;
        sim::WorldTestAccess::seams(matching).tile_cleanup.name = "cleanup";
        CHECK(read_into(matching, save_fields(named)) == "");
    }
    SUBCASE("a config that does not match the world's seams is refused") {
        sim::World three = w;
        sim::WorldTestAccess::config(three).city_pass_count = 3;
        sim::WorldTestAccess::seams(three) = sim::Seams::sized_for(three.config());
        sim::World other; // 8 city passes
        const std::string err = read_into(other, save_fields(three));
        CHECK(err.find("Seams.city_pass") != std::string::npos);
        sim::World matching;
        sim::WorldTestAccess::config(matching).city_pass_count = 3;
        sim::WorldTestAccess::seams(matching) = sim::Seams::sized_for(matching.config());
        CHECK(read_into(matching, save_fields(three)) == "");
    }
    SUBCASE("a config that the world's registry does not fit is refused") {
        sim::TypeRegistry::Builder b;
        sim::BuildingType priced;
        priced.id = sim::TypeId{5};
        priced.cost_goods = {{0x29, 256}};
        b.add(priced);
        auto built = b.build();
        REQUIRE(built);
        sim::World with_types = w;
        sim::WorldTestAccess::set_types(with_types, built.value());
        // Tools are goods 0x29: a stream whose build goods start above it does not fit.
        sim::World shifted = with_types;
        sim::WorldTestAccess::config(shifted).build_goods_first = 0x30;
        REQUIRE(shifted.config().validate().empty());
        sim::World other;
        sim::WorldTestAccess::set_types(other, built.value());
        const std::string err = read_into(other, save_fields(shifted));
        CHECK(starts_with(err, "header"));
        CHECK(err.find("registry does not fit the configuration") != std::string::npos);
        CHECK(err.find("type 5") != std::string::npos);
        // The same registry with the matching config reads.
        CHECK(read_into(other, save_fields(with_types)) == "");
    }
    SUBCASE("a settings scope that is out of range is refused") {
        sim::World bad_world = w;
        sim::WorldTestAccess::settings(bad_world).world_width = 0xFFFFFFFFU;
        sim::World other;
        std::string err = read_into(other, save_fields(bad_world));
        CHECK(starts_with(err, "settings"));
        CHECK(err.find("WorldSettings.world_width") != std::string::npos);
        sim::WorldTestAccess::settings(bad_world) = sim::WorldSettings{};
        sim::WorldTestAccess::settings(bad_world).prop10 = 7;
        err = read_into(other, save_fields(bad_world));
        CHECK(err.find("WorldSettings.prop10") != std::string::npos);
    }
    SUBCASE("a failed read leaves the target world as it was") {
        // The target has its own non-default values everywhere the stream writes.
        auto prepare = [] {
            sim::World t(77);
            t.advance(6);
            sim::WorldTestAccess::config(t).list_min_growth = 33;
            sim::WorldTestAccess::rules(t).enforce_research = true;
            sim::WorldTestAccess::settings(t).prop14 = 1;
            return t;
        };
        const sim::World pristine = prepare();
        const std::uint64_t before = pristine.state_hash();
        auto expect_unchanged = [&](const sim::World& target, const char* why) {
            INFO(why);
            CHECK(target.state_hash() == before);
            CHECK(target.config().list_min_growth == 33);
            CHECK(target.config().building_div == 500);
            CHECK(target.rules().enforce_research);
            CHECK(target.settings().prop14 == 1);
            CHECK(target.settings().world_width == 0);
            CHECK(target.clock.ms == 6);
            CHECK(target.rng.cursor() == 77);
        };
        // A zero divisor: fails in the header, after the config was read.
        sim::World zero = w;
        sim::WorldTestAccess::config(zero).building_div = 0;
        sim::World t1 = prepare();
        CHECK(read_into(t1, save_fields(zero)) != "");
        expect_unchanged(t1, "zero divisor");
        // Other seam names: fails after the config and the rules were read.
        sim::World named = w;
        sim::WorldTestAccess::seams(named).tile_cleanup.name = "cleanup";
        sim::World t2 = prepare();
        CHECK(read_into(t2, save_fields(named)) != "");
        expect_unchanged(t2, "seam names");
        // A hostile settings scope: fails last, after the clock and the cursor were read.
        sim::World bad_settings = w;
        sim::WorldTestAccess::settings(bad_settings).world_height = 0xFFFFFFFFU;
        sim::World t3 = prepare();
        CHECK(read_into(t3, save_fields(bad_settings)) != "");
        expect_unchanged(t3, "settings");
        // A truncated stream.
        sim::World t4 = prepare();
        const std::vector<std::uint8_t> cut(bytes.begin(),
                                            bytes.end() - 5); // inside the settings scope
        CHECK(read_into(t4, cut) != "");
        expect_unchanged(t4, "truncated");
        // And a good stream does change it.
        sim::World t5 = prepare();
        CHECK(read_into(t5, bytes) == "");
        CHECK(t5.state_hash() == w.state_hash());
    }
    SUBCASE("a hostile list count fails before anything is sized") {
        // The layout's lists count (6) is a u64 prefix in front of the first
        // list name ("stall"); patch it to 2^60.
        sim::World other;
        std::vector<std::uint8_t> bad = bytes;
        Stream pattern;
        pattern.u64(6);
        pattern.str("stall");
        const auto at = std::search(bad.begin(), bad.end(), pattern.v.begin(), pattern.v.end());
        REQUIRE(at != bad.end());
        *(at + 7) = 0x10;
        const std::string err = read_into(other, bad);
        CHECK(err != "");
        CHECK(starts_with(err, "header"));
    }
}
