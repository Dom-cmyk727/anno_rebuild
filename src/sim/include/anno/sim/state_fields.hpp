// The one field list of the persistent state (spec: docs/sim/state.md §9.1,
// §9.3). For every persistent type T there is exactly one function
//     template <class A, Same<T> S> void fields(A& a, S& s)
// naming its fields in canonical order; hash, save and load all run it, so a
// field cannot be saved and not hashed. Adding a field is one line at the end
// of its function plus a kStateHashVersion bump and re-baselined goldens in
// the same commit.
//
// Stream at kStateHashVersion 103 (stage S5), the scopes of §9.3 so far:
//   1 header     u32 kStateHashVersion, u32 kPrngTimelineVersion,
//                SimConfig (declaration order; vectors as count + elements;
//                small_farm_animals; layout), Rules (declaration order),
//                u64 registry digest, seam names (count + str each)
//   2 clock_rng  u64 clock.ms, u64 clock.last_tick, u64 rng.size,
//                u64 rng.table_digest, u32 rng.cursor  (= core_hash)
//   3 settings   the WorldSettings
//   5 players    u64 slot count, then per slot (scope `player`, ordinal = slot)
//                an optional Player, then u16 stamp, u32 cursor
//   6 islands    u64 slot count, then per slot (scope `island`, ordinal = slot)
//                an optional Island, whose tile layer is the scope `tiles`
//                (the cities scope of §9.3 follows with stage S6)
// Not in the stream: the PRNG table itself (immutable, identified by its
// size and digest; a custom table travels in the save frame, §9.5), the type
// registry (its digest is in the header) and the seam functions (their names
// are).
//
// Reading the header validates what it just read (SimConfig::validate, Rules::
// validate, validate_seams, the registry against the config; §3.1) before the
// stream goes on, so no later container is sized from an invalid configuration,
// and compares the registry digest and the seam names with the world's own (the
// world is given its registry and seams, they are not in the stream). The
// settings scope is validated the same way (WorldSettings::validate), and the
// players scope against the configuration just read (validate_players).
//
// A failed read leaves the target World unchanged: the configuration, rules,
// settings, clock and PRNG cursor are read into temporaries and committed only
// when the whole stream was read and validated (StateIO::world; the player table
// and the islands too). The islands scope is validated against the configuration,
// the settings and the registry just read (validate_islands).
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/archive.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

namespace anno::sim {

/// The header scope's fields: the versions the stream was made with. A
/// default-constructed value carries this build's versions; reading a stream
/// made with other versions fails.
struct StateHeader {
    std::uint32_t state_hash_version = kStateHashVersion;
    std::uint32_t prng_timeline_version = kPrngTimelineVersion;
};

template <class A, Same<StateHeader> S> void fields(A& a, S& s) {
    a.u32(s.state_hash_version);
    a.u32(s.prng_timeline_version);
    if constexpr (A::kReading) {
        if (!a.failed() && s.state_hash_version != kStateHashVersion) {
            a.fail("state hash version differs from this build's");
        } else if (!a.failed() && s.prng_timeline_version != kPrngTimelineVersion) {
            a.fail("PRNG timeline version differs from this build's");
        }
    }
}

template <class A, Same<WorldClock> S> void fields(A& a, S& s) {
    a.u64(s.ms);
    a.u64(s.last_tick);
}

/// The PRNG: table size and digest identify the (shared, immutable) table,
/// the cursor is the state. Reading checks the first two against the table
/// the world already has and rejects a cursor outside it.
template <class A, Same<WorldRng> S> void fields(A& a, S& s) {
    std::uint64_t size = s.size();
    std::uint64_t digest = s.table_digest();
    std::uint32_t cursor = s.cursor();
    a.u64(size);
    a.u64(digest);
    a.u32(cursor);
    if constexpr (A::kReading) {
        if (!a.failed()) {
            if (size != s.size() || digest != s.table_digest()) {
                a.fail("PRNG table differs from the world's table");
            } else if (cursor >= size) {
                a.fail("PRNG cursor outside the table");
            } else {
                s.set_cursor(cursor);
            }
        }
    }
}

// ---- configuration (header scope)

template <> struct EnumInfo<SlotPolicy> : ContiguousEnum<2> {};
template <> struct EnumInfo<SpatialOrder> : ContiguousEnum<2> {};
template <> struct EnumInfo<PrngMode> : ContiguousEnum<2> {};

/// A vector of u32: u64 count, then the elements.
template <class A, class V> void u32_list(A& a, V& v) {
    ar::seq(a, v, 4, [&](auto& x) { a.u32(x); });
}

// The library itself fails to compile when a persistent struct gains a member
// without a line in its fields() function: each static_assert below names the
// member count that the function after it covers (docs/sim/state.md §9.1, §14.4;
// the digest tests of config_test.cpp pin the same numbers).
static_assert(field_count<ListSpec>() == 2, "a ListSpec member needs a line in fields(ListSpec)");

template <class A, Same<ListSpec> S> void fields(A& a, S& s) {
    a.str(s.name);
    a.u32(s.lookup_mask);
}

static_assert(field_count<ListLayout>() == 9 && field_count<ListLayout::Roles>() == 6,
              "a ListLayout member needs a line in fields(ListLayout) (roles is one member, "
              "six scalars)");

template <class A, Same<ListLayout> S> void fields(A& a, S& s) {
    ar::seq(a, s.lists, 12, [&](auto& l) { fields(a, l); });
    u32_list(a, s.schedule);
    u32_list(a, s.lookup_order);
    u32_list(a, s.rect_order);
    u32_list(a, s.block_to_list);
    a.u32(s.first_prodtype);
    a.u32(s.block_size);
    a.u32(s.default_list);
    a.u32(s.roles.market_list);
    a.u32(s.roles.house_list);
    a.u32(s.roles.public_list);
    a.u32(s.roles.military_list);
    a.u32(s.roles.production_list);
    a.u32(s.roles.stall_list);
}

static_assert(field_count<SmallFarmAnimals>() == 3,
              "a SmallFarmAnimals member needs a line in fields(SmallFarmAnimals)");

template <class A, Same<SmallFarmAnimals> S> void fields(A& a, S& s) {
    ar::seq(a, s.kind_by_draw, 2, [&](auto& x) { a.u16(x); });
    a.u16(s.base);
    ar::seq(a, s.limit, 1, [&](auto& x) { a.u8(x); });
}

static_assert(field_count<SimConfig>() == 51,
              "a SimConfig member needs a line in fields(SimConfig)");

template <class A, Same<SimConfig> S> void fields(A& a, S& s) {
    a.u32(s.island_slots);
    a.u32(s.city_slots);
    a.u32(s.neutral_owner);
    a.u32(s.player_slots);
    a.u32(s.neutral_player);
    a.u32(s.script_timers);
    a.u32(s.monopoly_entries);
    a.u32(s.monopoly_raw_slots);
    a.u32(s.service_bits);
    a.u32(s.house_groups);
    a.u32(s.research_entries);
    a.u32(s.warehouse_goods);
    a.u32(s.build_goods_first);
    a.u32(s.build_goods_count);
    a.u32(s.trade_lists);
    a.u32(s.trade_limit_max);
    a.u32(s.chunk_edge);
    a.u32(s.list_initial_size);
    a.u32(s.list_min_growth);
    a.u32(s.city_pass_count);
    a.u32(s.building_div);
    a.u32(s.city_ring_div);
    a.u32(s.player_div);
    a.u32(s.figure_a_div);
    a.u32(s.figure_b_div);
    a.u32(s.objmgr_div);
    a.u32(s.grow_period);
    a.u32(s.fauna_period);
    a.u32(s.figure_expiry_ticks);
    a.u32(s.event300_gate_seconds);
    a.u32(s.event300_modulus);
    a.u32(s.event750_ms);
    a.u32(s.event750_modulus);
    a.u32(s.monopoly_ms);
    a.u32(s.upkeep_period);
    a.u32(s.upkeep_slices);
    a.u32(s.stat_decay_num);
    a.u32(s.stat_decay_den);
    a.i64(s.bankrupt_money);
    a.u32(s.bankrupt_grace_ticks);
    a.u32(s.drought_radius);
    a.u32(s.drought_plateau_period);
    a.u32(s.drought_plateau_count);
    a.u32(s.drought_strip_period);
    a.u32(s.drought_steps);
    a.u32(s.drought_phases);
    u32_list(a, s.monopoly_climate_mask);
    a.boolean(s.multiplayer);
    a.u32(s.host_player.v);
    fields(a, s.small_farm_animals);
    fields(a, s.layout);
}

static_assert(field_count<Rules>() == 17, "a Rules member needs a line in fields(Rules)");

template <class A, Same<Rules> S> void fields(A& a, S& s) {
    a.enumv(s.slot_policy);
    a.enumv(s.spatial_order);
    a.boolean(s.events_drain_new);
    a.u64(s.events_max_dispatch);
    a.boolean(s.events_flush_after_figures);
    a.boolean(s.monopoly_every_30s);
    a.boolean(s.player_table_full_tick);
    a.boolean(s.drought_random_centre);
    a.boolean(s.build_atomic);
    a.boolean(s.keep_empty_city);
    a.boolean(s.event750_keep_overshoot);
    a.enumv(s.prng_mode);
    a.u32(s.interim_figure_return_ticks);
    a.boolean(s.free_build_cheat);
    a.boolean(s.enforce_research);
    a.boolean(s.stat_decay_wide);
    a.boolean(s.bankrupt_deadline_wrap_safe);
}

// ---- settings scope

static_assert(field_count<WorldSettings>() == 16,
              "a WorldSettings member needs a line in fields(WorldSettings)");

template <class A, Same<WorldSettings> S> void fields(A& a, S& s) {
    a.u32(s.world_width);
    a.u32(s.world_height);
    a.u8(s.prop0);
    a.u8(s.prop1);
    a.u8(s.prop2);
    a.u16(s.prop3);
    a.boolean(s.prop5);
    a.u32(s.prop6);
    a.u32(s.prop7_offset);
    a.u8(s.prop9);
    a.u8(s.prop10);
    a.u32(s.prop11);
    a.boolean(s.prop12);
    a.boolean(s.prop13);
    a.u32(s.prop14);
    a.u32(s.speed_percent);
}

// ---- players scope

static_assert(field_count<ResearchState>() == 2,
              "a ResearchState member needs a line in fields(ResearchState)");

template <class A, Same<ResearchState> S> void fields(A& a, S& s) {
    ar::seq(a, s.state, 1, [&](auto& x) { a.u8(x); });
    ar::seq(a, s.progress, 2, [&](auto& x) { a.u16(x); });
}

static_assert(field_count<Player>() == 20, "a Player member needs a line in fields(Player)");

template <class A, Same<Player> S> void fields(A& a, S& s) {
    a.u32(s.index.v);
    a.u32(s.kind);
    a.u32(s.volk);
    a.i64(s.money);
    a.u32(s.upkeep_a);
    a.u32(s.upkeep_b);
    a.u32(s.upkeep_rem);
    a.u16(s.upkeep_stamp);
    a.u32(s.stat_a);
    a.u32(s.stat_b);
    a.u32(s.flags);
    a.u32(s.flags_a0);
    a.u32(s.colour_slot);
    a.u32(s.default_colour);
    // An open byte (D: player +0x28d stores any value), so no enum validation: the
    // byte is read and written as it is.
    std::uint8_t state = static_cast<std::uint8_t>(s.state);
    a.u8(state);
    if constexpr (A::kReading) {
        s.state = static_cast<PlayerState>(state);
    }
    a.u32(s.bankrupt_deadline);
    a.boolean(s.defeat_pending);
    a.boolean(s.free_build);
    ar::seq(a, s.name, 1, [&](auto& x) { a.u8(x); });
    fields(a, s.research);
}

static_assert(field_count<PlayerTable>() == 3,
              "a PlayerTable member needs a line in fields(PlayerTable)");

/// The slots, each in a scope `player` indexed by the slot (so a desync report
/// can name "players/player[3]", docs/sim/state.md §9.4), then the table's own
/// stamp and cursor. The structural facts (slot count, indices, research sizes,
/// cursor) are checked by validate_players against the configuration; the
/// caller runs it (StateIO::scopes).
template <class A, Same<PlayerTable> S> void fields(A& a, S& s) {
    std::uint64_t n = s.slots.size();
    a.count(n, 1);
    if constexpr (A::kReading) {
        // A slot is one byte on the wire when absent but a whole Player in memory
        // (about 150 bytes), so the remaining input does not bound the allocation
        // as it does for a byte or word list. The count is bounded here by the
        // table limit that SimConfig::validate applies to player_slots
        // (kSlotTableBound) before any slot is made; validate_players then compares
        // it with the configuration's own player_slots. Path to raise the limit:
        // kSlotTableBound.
        if (!a.failed() && n > kSlotTableBound) {
            a.fail("slot count " + std::to_string(n) + " is above the table limit " +
                   std::to_string(kSlotTableBound));
            n = 0;
        }
        s.slots.clear();
        for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
            s.slots.emplace_back();
            a.scope("player", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, s.slots.back(), [&](auto& p) { fields(a, p); }); });
        }
    } else {
        for (std::size_t i = 0; i < s.slots.size(); ++i) {
            a.scope("player", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, s.slots[i], [&](auto& p) { fields(a, p); }); });
        }
    }
    a.u16(s.stamp);
    a.u32(s.cursor);
}

// ---- islands scope

static_assert(field_count<Deposit>() == 5, "a Deposit member needs a line in fields(Deposit)");

template <class A, Same<Deposit> S> void fields(A& a, S& s) {
    a.str(s.goods);
    a.i32(s.x);
    a.i32(s.y);
    a.u32(s.remaining);
    a.boolean(s.reserve);
}

static_assert(field_count<SweepState>() == 5,
              "a SweepState member needs a line in fields(SweepState)");

template <class A, Same<SweepState> S> void fields(A& a, S& s) {
    a.u16(s.stamp);
    a.u32(s.grow_x);
    a.u32(s.grow_y);
    a.u32(s.fauna_x);
    a.u32(s.fauna_y);
}

static_assert(field_count<CityRing>() == 2, "a CityRing member needs a line in fields(CityRing)");

template <class A, Same<CityRing> S> void fields(A& a, S& s) {
    a.u16(s.stamp);
    a.u32(s.cursor);
}

static_assert(field_count<ObjectManagerState>() == 2,
              "an ObjectManagerState member needs a line in fields(ObjectManagerState)");

template <class A, Same<ObjectManagerState> S> void fields(A& a, S& s) {
    a.u16(s.stamp);
    a.u32(s.cursor);
}

static_assert(field_count<Drought>() == 10, "a Drought member needs a line in fields(Drought)");

template <class A, Same<Drought> S> void fields(A& a, S& s) {
    a.u32(s.x);
    a.u32(s.y);
    a.u32(s.target_ring);
    a.i32(s.ring);
    a.u32(s.city_mask);
    a.u8(s.phase);
    a.u8(s.step);
    a.u8(s.state);
    a.u16(s.counter);
    a.u16(s.stamp);
}

/// The one place where the archives differ in what they touch (docs/sim/state.md
/// §9.2). A friend of TileLayer: the columns are moved in bulk, not through the
/// setters (a read ends with every chunk dirty, which reset() leaves).
struct TileIO {
    template <class A, class T> static void columns(A& a, T& t) {
        for (auto& v : t.object_) {
            a.u32(v);
        }
        for (auto& v : t.phase_) {
            a.u8(v);
        }
        for (auto& v : t.owner_) {
            a.u32(v);
        }
        for (auto& v : t.aux_) {
            a.u32(v);
        }
    }
};

/// The scope `tiles` of an island: `u32 width, u32 height, u32 chunk_edge`, then
/// the four columns `object[]` (u32), `phase[]` (u8), `owner[]` (u32), `aux[]`
/// (u32), each w · h elements (kTileBytes = 13 per tile). `x0`, `y0`, `w` and `h`
/// are the island's own rectangle, which the island scope wrote just before; the
/// layer's dimensions must equal them. Reading rejects before allocating:
/// chunk_edge 0, more than kIslandTileBound tiles, or w · h · 13 > remaining()
/// (in u64); the generic count check of ReadArchive does not cover the bulk
/// columns. The hierarchical hash feeds the dimensions and, for every chunk in
/// row-major chunk order, the cached chunk digest (u64) instead of the columns;
/// the raw hash (and the write and trace archives) feed the columns, so the raw
/// digest equals the FNV of the saved bytes.
template <class A, class T>
void tiles_io(A& a, T& t, std::int32_t x0, std::int32_t y0, std::uint32_t w, std::uint32_t h) {
    std::uint32_t tw = t.width();
    std::uint32_t th = t.height();
    std::uint32_t edge = t.chunk_edge();
    a.u32(tw);
    a.u32(th);
    a.u32(edge);
    if constexpr (A::kReading) {
        if (a.failed()) {
            return;
        }
        if (tw != w || th != h) {
            a.fail("tile layer " + std::to_string(tw) + " x " + std::to_string(th) +
                   " differs from the island's " + std::to_string(w) + " x " + std::to_string(h));
            return;
        }
        if (edge == 0) {
            a.fail("chunk_edge is 0");
            return;
        }
        const std::uint64_t tiles = std::uint64_t{tw} * th;
        if (tiles > kIslandTileBound) {
            a.fail("more than " + std::to_string(kIslandTileBound) + " tiles");
            return;
        }
        if (tiles > a.remaining() / kTileBytes) {
            a.fail("tile columns need " + std::to_string(tiles) + " x " +
                   std::to_string(kTileBytes) + " bytes, " + std::to_string(a.remaining()) +
                   " remain");
            return;
        }
        if (!t.reset(x0, y0, tw, th, edge)) {
            a.fail("the tile layer cannot be sized");
            return;
        }
    }
    if constexpr (std::is_same_v<A, HashArchive>) {
        if (!a.raw()) {
            for (std::uint64_t k = 0; k < t.chunk_count(); ++k) {
                a.u64(t.chunk_digest(k));
            }
            return;
        }
    }
    TileIO::columns(a, t);
}

// Island has no user-declared constructor on purpose (C++20), so that the canary
// can count its members: index, tiles, terrain, climate, deposits, next_serial,
// sweep, event300_stamp, drought, objmgr, ring, reserved_object_count.
static_assert(field_count<Island>() == 12, "an Island member needs a line in fields(Island)");

/// One island (docs/sim/state.md §9.3): `u32 index`, the rectangle `i32 x0, i32 y0,
/// u32 w, u32 h`, `u32 climate, u32 next_serial, u64 terrain_digest` (0 if none),
/// the sweep, `u16 event300_stamp`, the object manager, the ring, the optional
/// drought, the deposits, `u64 reserved_object_count`, scope `tiles`. The terrain
/// itself is not in the stream (its blobs travel in the save frame, §9.5): reading
/// leaves a placeholder that carries the digest, and the frame replaces it by the
/// verified blobs.
template <class A, Same<Island> S> void fields(A& a, S& s) {
    a.u32(s.index.v);
    std::int32_t x0 = s.tiles.x0();
    std::int32_t y0 = s.tiles.y0();
    std::uint32_t w = s.tiles.width();
    std::uint32_t h = s.tiles.height();
    a.i32(x0);
    a.i32(y0);
    a.u32(w);
    a.u32(h);
    a.u32(s.climate);
    a.u32(s.next_serial);
    std::uint64_t terrain = s.terrain ? s.terrain->digest : 0;
    a.u64(terrain);
    if constexpr (A::kReading) {
        if (!a.failed() && terrain != 0) {
            auto placeholder = std::make_shared<Terrain>();
            placeholder->digest = terrain;
            s.terrain = std::move(placeholder);
        } else {
            s.terrain.reset();
        }
    }
    fields(a, s.sweep);
    a.u16(s.event300_stamp);
    fields(a, s.objmgr);
    fields(a, s.ring);
    ar::opt(a, s.drought, [&](auto& d) { fields(a, d); });
    // Smallest Deposit on the wire: u64 name length + 3 x 4 + 1 bytes.
    ar::seq(a, s.deposits, 21, [&](auto& d) { fields(a, d); });
    a.u64(s.reserved_object_count);
    a.scope("tiles", [&] { tiles_io(a, s.tiles, x0, y0, w, h); });
}

/// The island table: `u64 slot count`, then per slot (scope `island`, ordinal =
/// slot) an optional Island. On reading the count is bounded by
/// SimConfig::island_slots (already read and validated) before any slot is made,
/// because an absent slot is one byte on the wire but a whole optional<Island> in
/// memory; validate_islands then checks the structure (StateIO::islands_section).
template <class A, class V> void islands_io(A& a, V& islands, const SimConfig& cfg) {
    std::uint64_t n = islands.size();
    a.count(n, 1);
    if constexpr (A::kReading) {
        if (!a.failed() && n > cfg.island_slots) {
            a.fail("slot count " + std::to_string(n) + " is above SimConfig.island_slots " +
                   std::to_string(cfg.island_slots));
            n = 0;
        }
        islands.clear();
        for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
            islands.emplace_back();
            a.scope("island", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, islands.back(), [&](auto& is) { fields(a, is); }); });
        }
    } else {
        for (std::size_t i = 0; i < islands.size(); ++i) {
            a.scope("island", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, islands[i], [&](auto& is) { fields(a, is); }); });
        }
    }
}

/// The hierarchical hash of any persistent value, with the scopes down to
/// `depth` recorded in `tree` when it is not null. World::state_hash is this
/// over the world; it is a template so the depth rule can be tested on values
/// that nest scopes (the world itself only has top-level scopes until S6).
template <class T> std::uint64_t hash_state(const T& value, HashTree* tree, std::uint32_t depth) {
    HashArchive a(HashOptions{tree, depth, false});
    fields(a, value);
    return a.finish();
}

/// The FNV-1a 64 of one value's field list in raw mode (scopes transparent):
/// the digest of a SimConfig, Rules or ListLayout.
template <class T> std::uint64_t digest_of(const T& value) {
    HashArchive a(HashOptions{nullptr, 0, true});
    fields(a, value);
    return a.finish();
}

/// The access door of the field list to the private members of World (a
/// friend of World, world.hpp): the world's own fields() calls it. The stream is
/// five sections, one function each (the save frame writes and reads them one
/// by one, §9.5); `scopes` is the flat stream of all five in order, which is what
/// the hash and the field-list tests run.
struct StateIO {
    /// Section 1. Reading validates what it just read (§3.1) before anything is
    /// sized from it. `w` supplies the registry and the seams, which are not in
    /// the stream.
    template <class A, class W, class Config, class RulesT>
    static void header_section(A& a, W& w, Config& config, RulesT& rules) {
        StateHeader header;
        a.scope("header", [&] {
            fields(a, header);
            fields(a, config);
            fields(a, rules);
            if constexpr (A::kReading) {
                // §3.1: validate what was just read before anything is sized
                // from it.
                if (!a.failed()) {
                    std::string problem = config.validate();
                    if (problem.empty()) {
                        problem = rules.validate();
                    }
                    if (problem.empty()) {
                        problem = validate_seams(w.seams_, config);
                    }
                    if (problem.empty()) {
                        problem = w.types_->validate_against(config);
                        if (!problem.empty()) {
                            problem = "registry does not fit the configuration: " + problem;
                        }
                    }
                    if (!problem.empty()) {
                        a.fail(problem);
                    }
                }
            }
            std::uint64_t registry_digest = w.types_->digest();
            a.u64(registry_digest);
            if constexpr (A::kReading) {
                if (!a.failed() && registry_digest != w.types_->digest()) {
                    a.fail("type registry digest differs from the world's registry");
                }
            }
            std::vector<std::string> names = seam_names(w.seams_);
            ar::seq(a, names, 8, [&](auto& n) { a.str(n); });
            if constexpr (A::kReading) {
                if (!a.failed() && names != seam_names(w.seams_)) {
                    a.fail("seam names differ from the world's seams");
                }
            }
        });
    }

    /// Section 2.
    template <class A, class Clock, class Rng>
    static void clock_rng_section(A& a, Clock& clock, Rng& rng) {
        a.scope("clock_rng", [&] {
            fields(a, clock);
            fields(a, rng);
        });
    }

    /// Section 3.
    template <class A, class Settings> static void settings_section(A& a, Settings& settings) {
        a.scope("settings", [&] {
            fields(a, settings);
            if constexpr (A::kReading) {
                if (!a.failed()) {
                    const std::string problem = settings.validate();
                    if (!problem.empty()) {
                        a.fail(problem);
                    }
                }
            }
        });
    }

    /// Section 4 (the `ids` section of §9.3 is not in the stream yet).
    template <class A, class Players, class Config>
    static void players_section(A& a, Players& players, const Config& config) {
        a.scope("players", [&] {
            fields(a, players);
            if constexpr (A::kReading) {
                if (!a.failed()) {
                    const std::string problem = validate_players(players, config);
                    if (!problem.empty()) {
                        a.fail(problem);
                    }
                }
            }
        });
    }

    /// Section 5. Reading validates the table against the configuration, the
    /// settings and the registry just read (validate_islands).
    template <class A, class W, class Islands, class Config, class Settings>
    static void islands_section(A& a, W& w, Islands& islands, const Config& config,
                                const Settings& settings) {
        a.scope("islands", [&] {
            islands_io(a, islands, config);
            if constexpr (A::kReading) {
                if (!a.failed()) {
                    const std::string problem =
                        validate_islands(islands, config, settings, *w.types_);
                    if (!problem.empty()) {
                        a.fail(problem);
                    }
                }
            }
        });
    }

    /// The scopes of the stream over the configuration, rules, settings, clock,
    /// PRNG, players and islands to use: the world's own members when writing or
    /// hashing, the read-side temporaries when reading (so one function defines
    /// the order of the stream for all four archives).
    template <class A, class W, class Config, class RulesT, class Settings, class Clock, class Rng,
              class Players, class Islands>
    static void scopes(A& a, W& w, Config& config, RulesT& rules, Settings& settings, Clock& clock,
                       Rng& rng, Players& players, Islands& islands) {
        header_section(a, w, config, rules);
        clock_rng_section(a, clock, rng);
        settings_section(a, settings);
        players_section(a, players, config);
        islands_section(a, w, islands, config, settings);
    }

    template <class A, class W> static void world(A& a, W& w) {
        if constexpr (A::kReading) {
            // Read into copies and commit only a stream that was read whole and
            // validated: a failed read leaves the target as it was (the target
            // may be reused by a caller, and an invalid configuration in it
            // would run the scheduler).
            SimConfig config = w.config_;
            Rules rules = w.rules_;
            WorldSettings settings = w.settings_;
            WorldClock clock = w.clock;
            WorldRng rng = w.rng;
            PlayerTable players = w.players_;
            std::vector<std::optional<Island>> islands;
            scopes(a, w, config, rules, settings, clock, rng, players, islands);
            if (!a.failed()) {
                w.config_ = std::move(config);
                w.rules_ = rules;
                w.settings_ = settings;
                w.clock = clock;
                w.rng = rng;
                w.players_ = std::move(players);
                w.islands_ = std::move(islands);
            }
        } else {
            scopes(a, w, w.config_, w.rules_, w.settings_, w.clock, w.rng, w.players_, w.islands_);
        }
    }

    /// The save frame (docs/sim/state.md §9.5; save.cpp): "A3SV", u32 save version,
    /// u32 kStateHashVersion, the PRNG table kind, the sections each with its
    /// byte length in front, the terrain blobs, a trailing FNV-1a 64.
    static std::vector<std::uint8_t> save(const World& w);
    static formats::Result<World, std::string> load(std::span<const std::uint8_t> bytes,
                                                    std::shared_ptr<const TypeRegistry> types,
                                                    Seams seams);
};

/// The world so far: scopes header, clock_rng, settings, players and islands (the
/// order of §9.3).
template <class A, Same<World> S> void fields(A& a, S& w) {
    StateIO::world(a, w);
}

} // namespace anno::sim
