// The one field list of the persistent state (spec: docs/sim/state.md §9.1,
// §9.3). For every persistent type T there is exactly one function
//     template <class A, Same<T> S> void fields(A& a, S& s)
// naming its fields in canonical order; hash, save and load all run it, so a
// field cannot be saved and not hashed. Adding a field is one line at the end
// of its function plus a kStateHashVersion bump and re-baselined goldens in
// the same commit.
//
// Stream at kStateHashVersion 113 (task T33.5 stage 2), the scopes of §9.3 so far:
//   1 header     u32 kStateHashVersion, u32 kPrngTimelineVersion,
//                SimConfig (declaration order; vectors as count + elements;
//                small_farm_animals; layout), Rules (declaration order),
//                u64 registry digest, u64 walk-table digest,
//                seam names (count + str each)
//   2 clock_rng  u64 clock.ms, u64 clock.last_tick, u64 rng.size,
//                u64 rng.table_digest, u32 rng.cursor  (= core_hash)
//   3 settings   the WorldSettings
//   4 ids        u64 next_figure, u64 next_event_seq, u32 event750_acc,
//                u32 monopoly.acc, bool faulted
//   5 players    u64 slot count, then per slot (scope `player`, ordinal = slot)
//                an optional Player, then u16 stamp, u32 cursor
//   6 islands    u64 slot count, then per slot (scope `island`, ordinal = slot)
//                an optional Island, whose tile layer is the scope `tiles` and
//                whose cities are the scope `cities` (per slot a scope `city`
//                with an optional City, whose building lists are the scopes
//                `index` / `list` / `building`)
//   7 figures    the FigureRegistry: i64 hint, u32 cursor, u64 slot count, per slot
//                (scope `figure`, ordinal = slot) an optional Figure, u16 stamp,
//                the expiry ids
//   8 events     the timed-event queue: u64 count, per event its key (u64 due, u64
//                seq) and the TimedEvent
//   9 timers_monopoly  the ScriptTimers (u32 last_tick, u8 flags, u32 counters) and
//                the monopoly entries (u32 time_s, optional u32 player each)
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

#include <array>
#include <cstdint>
#include <limits>
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
#include <anno/sim/create.hpp>
#include <anno/sim/events.hpp>
#include <anno/sim/field_count.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/timers.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/walk_cells.hpp>
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
template <> struct EnumInfo<IslandCandidateOrder> : ContiguousEnum<2> {};

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

static_assert(field_count<LinkTables>() == 13,
              "a LinkTables member needs a line in fields(LinkTables)");

template <class A, Same<LinkTables> S> void fields(A& a, S& s) {
    a.u32(s.raw_kinds);
    a.u32(s.ore_kinds);
    a.u32(s.rounds);
    a.u32(s.best_list_cap);
    a.u32(s.pick_list_cap);
    u32_list(a, s.raw_climate_mask);
    u32_list(a, s.ore_climate_mask);
    u32_list(a, s.ore_mask_bit);
    ar::seq(a, s.ore_exclusion, 1, [&](auto& x) { a.u8(x); });
    u32_list(a, s.raw_width_rank);
    u32_list(a, s.ore_width_rank);
    u32_list(a, s.raw_rank_score);
    u32_list(a, s.ore_rank_score);
}

static_assert(field_count<SimConfig>() == 52,
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
    a.u32(s.chunk_edge);
    a.u32(s.list_initial_size);
    a.u32(s.list_min_growth);
    a.u32(s.figure_min_growth);
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
    fields(a, s.links);
}

static_assert(field_count<Rules>() == 19, "a Rules member needs a line in fields(Rules)");

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
    a.boolean(s.timer0_clamp);
    a.enumv(s.island_candidate_order);
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
    ar::seq(a, s.city_mask, 4, [&](auto& w) { a.u32(w); });
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
        for (auto& v : t.walk_code_) {
            a.u16(v);
        }
        for (auto& v : t.walk_mask_) {
            a.u16(v);
        }
        for (auto& v : t.foot_dx_) {
            a.u16(v);
        }
        for (auto& v : t.foot_dy_) {
            a.u16(v);
        }
        for (auto& v : t.foot_rotation_) {
            a.u8(v);
        }
        for (auto& v : t.foot_anchor_) {
            a.u8(v);
        }
    }
};

/// The scope `tiles` of an island: `u32 width, u32 height, u32 chunk_edge`, then
/// the ten columns `object[]` (u32), `phase[]` (u8), `owner[]` (u32), `aux[]`
/// (u32), `walk_code[]` (u16), `walk_mask[]` (u16), `foot_dx[]` (u16),
/// `foot_dy[]` (u16), `foot_rotation[]` (u8), `foot_anchor[]` (u8), each w · h
/// elements (kTileBytes = 23 per tile). `x0`, `y0`, `w` and `h` are the island's own
/// rectangle, which the island scope wrote just before; the layer's dimensions
/// must equal them. Reading rejects before allocating:
/// chunk_edge 0, more than kIslandTileBound tiles, or w · h · 23 > remaining()
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

// ---- cities and buildings (inside the islands scope, stage S6)

template <> struct EnumInfo<Status> : ContiguousEnum<3> {};

// `shows_icons` is derived (the driver sets it before every step, §7.1) and is not
// in the stream.
static_assert(field_count<StatusTracker>() == 4,
              "a StatusTracker member needs a line in fields(StatusTracker) or a derived note");

template <class A, Same<StatusTracker> S> void fields(A& a, S& s) {
    a.enumv(s.code);
    a.u8(s.icons);
    a.u16(s.time);
}

static_assert(field_count<Inert>() == 4, "an Inert member needs a line in fields(Inert)");

template <class A, Same<Inert> S> void fields(A& a, S& s) {
    a.u16(s.last);
    a.boolean(s.blocked);
    a.boolean(s.suspended);
    fields(a, s.status);
}

static_assert(field_count<Workshop>() == 10, "a Workshop member needs a line in fields(Workshop)");

template <class A, Same<Workshop> S> void fields(A& a, S& s) {
    a.u16(s.last);
    a.boolean(s.switched_on);
    a.boolean(s.suspended);
    a.boolean(s.blocked);
    u32_list(a, s.out);
    u32_list(a, s.in);
    u32_list(a, s.activity);
    u32_list(a, s.reserved);
    a.u32(s.carriers_out);
    fields(a, s.status);
}

// `unlocked` is derived (refreshed from the owner's research before every step,
// §7.1) and is not in the stream.
static_assert(field_count<WeaponsWorkshop>() == 11,
              "a WeaponsWorkshop member needs a line in fields(WeaponsWorkshop) or a derived note");

template <class A, Same<WeaponsWorkshop> S> void fields(A& a, S& s) {
    a.u16(s.last);
    ar::bools(a, s.line_on);
    a.boolean(s.suspended);
    a.boolean(s.blocked);
    u32_list(a, s.out);
    u32_list(a, s.in);
    ar::seq(a, s.activity, 2, [&](auto& x) { a.u16(x); });
    u32_list(a, s.reserved);
    a.u32(s.carriers_out);
    fields(a, s.status);
}

static_assert(field_count<Farm>() == 10, "a Farm member needs a line in fields(Farm)");

template <class A, Same<Farm> S> void fields(A& a, S& s) {
    a.u16(s.last);
    a.boolean(s.switched_on);
    a.boolean(s.suspended);
    a.boolean(s.blocked);
    a.u32(s.out);
    a.u32(s.raw);
    a.u16(s.activity);
    a.u32(s.reserved);
    a.u32(s.workers_out);
    fields(a, s.status);
}

static_assert(field_count<SmallFarm>() == 10,
              "a SmallFarm member needs a line in fields(SmallFarm)");

template <class A, Same<SmallFarm> S> void fields(A& a, S& s) {
    a.u16(s.last);
    ar::bools(a, s.line_on);
    a.boolean(s.suspended);
    a.boolean(s.blocked);
    u32_list(a, s.out);
    u32_list(a, s.raw);
    ar::seq(a, s.activity, 2, [&](auto& x) { a.u16(x); });
    u32_list(a, s.reserved);
    u32_list(a, s.figures_out);
    fields(a, s.status);
}

static_assert(field_count<Mine>() == 9, "a Mine member needs a line in fields(Mine)");

template <class A, Same<Mine> S> void fields(A& a, S& s) {
    a.u16(s.last);
    a.boolean(s.switched_on);
    a.boolean(s.deep);
    a.boolean(s.blocked);
    a.u32(s.out);
    a.u16(s.activity);
    a.u32(s.reserved);
    // The deposit is an index into Island::deposits: a presence byte, then a u64 (the
    // optional<size_t> is widened at the archive boundary).
    bool has = s.deposit.has_value();
    a.boolean(has);
    std::uint64_t deposit = has ? static_cast<std::uint64_t>(*s.deposit) : 0;
    if (has) {
        a.u64(deposit);
    }
    if constexpr (A::kReading) {
        if (!a.failed() && has) {
            s.deposit = static_cast<std::size_t>(deposit);
        } else {
            s.deposit.reset();
        }
    }
    fields(a, s.status);
}

static_assert(field_count<Market>() == 5, "a Market member needs a line in fields(Market)");

template <class A, Same<Market> S> void fields(A& a, S& s) {
    a.u16(s.last);
    a.boolean(s.blocked);
    a.boolean(s.suspended);
    fields(a, s.status);
    a.u32(s.carts_out);
}

static_assert(field_count<Building>() == 8, "a Building member needs a line in fields(Building)");

/// The u8 tag of each Payload alternative in the stream: the PayloadTag values
/// (Inert 0, Workshop 1, WeaponsWorkshop 2, Farm 3, SmallFarm 4, Mine 5, Market 6),
/// append-only.
inline constexpr std::array<std::uint8_t, 7> kPayloadTags = {
    static_cast<std::uint8_t>(PayloadTag::Inert),
    static_cast<std::uint8_t>(PayloadTag::Workshop),
    static_cast<std::uint8_t>(PayloadTag::WeaponsWorkshop),
    static_cast<std::uint8_t>(PayloadTag::Farm),
    static_cast<std::uint8_t>(PayloadTag::SmallFarm),
    static_cast<std::uint8_t>(PayloadTag::Mine),
    static_cast<std::uint8_t>(PayloadTag::Market)};

template <class A, Same<Building> S> void fields(A& a, S& s) {
    a.u64(s.id.v);
    a.u32(s.type.v);
    a.i32(s.x);
    a.i32(s.y);
    a.u8(s.rotation);
    a.u8(s.byte0e);
    a.u8(s.byte0f);
    ar::variant<kPayloadTags>(a, s.payload, [&](auto& alternative) { fields(a, alternative); });
}

/// Whether `n` slots of type T fit the load budget of one list (kSlotListLoadBytes).
template <class T> [[nodiscard]] constexpr bool slot_list_fits_load_budget(std::uint64_t n) {
    return n <= kSlotListLoadBytes / sizeof(std::optional<T>);
}

/// One building list (inside the scope `list` of its index): `i64 hint, u32 cursor,
/// u64 slot_count`, then per slot (scope `building`, ordinal = slot) an optional
/// Building. A slot is one byte on the wire when empty but a whole optional<Building>
/// in memory, so the count is bounded by kListSizeBound (the largest list
/// SimConfig::validate allows) and by kSlotListLoadBytes before any slot is made, and
/// the slots are made one at a time (the memory used tracks the bytes read).
/// validate_cities then checks the structure (hint, cursor, at least one slot).
template <class A, Same<BuildingList> S> void fields(A& a, S& s) {
    a.i64(s.hint);
    a.u32(s.cursor);
    std::uint64_t n = s.slots.size();
    a.count(n, 1);
    if constexpr (A::kReading) {
        if (!a.failed() && n > kListSizeBound) {
            a.fail("slot count " + std::to_string(n) + " is above the list limit " +
                   std::to_string(kListSizeBound));
            n = 0;
        }
        if (!a.failed() && !slot_list_fits_load_budget<Building>(n)) {
            a.fail("slot count " + std::to_string(n) + " is above the load budget of " +
                   std::to_string(kSlotListLoadBytes) + " bytes per list");
            n = 0;
        }
        s.slots.clear();
        for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
            s.slots.emplace_back();
            a.scope("building", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, s.slots.back(), [&](auto& b) { fields(a, b); }); });
        }
    } else {
        for (std::size_t i = 0; i < s.slots.size(); ++i) {
            a.scope("building", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, s.slots[i], [&](auto& b) { fields(a, b); }); });
        }
    }
}

// `service_counters` and `house_counts` are derived (rebuilt on load, compared with a
// recomputation by check_invariants) and are not in the stream.
static_assert(field_count<BuildingIndex>() == 4,
              "a BuildingIndex member needs a line in fields(BuildingIndex) or a derived note");

/// The scope `index` of a city: `u16 stamp`, `u64 list_count`, per list (scope `list`,
/// ordinal = ListId) a BuildingList. A list is 20 bytes at least on the wire.
template <class A, Same<BuildingIndex> S> void fields(A& a, S& s) {
    a.scope("index", [&] {
        a.u16(s.stamp);
        std::uint64_t n = s.lists.size();
        a.count(n, 20);
        if constexpr (A::kReading) {
            s.lists.clear();
            for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
                s.lists.emplace_back();
                a.scope("list", static_cast<std::uint32_t>(i), [&] { fields(a, s.lists.back()); });
            }
        } else {
            for (std::size_t i = 0; i < s.lists.size(); ++i) {
                a.scope("list", static_cast<std::uint32_t>(i), [&] { fields(a, s.lists[i]); });
            }
        }
    });
}

static_assert(field_count<CityStock>() == 2,
              "a CityStock member needs a line in fields(CityStock)");

template <class A, Same<CityStock> S> void fields(A& a, S& s) {
    a.u32(s.capacity);
    u32_list(a, s.stock);
}

static_assert(field_count<GoodsSetting>() == 2,
              "a GoodsSetting member needs a line in fields(GoodsSetting)");

template <class A, Same<GoodsSetting> S> void fields(A& a, S& s) {
    a.boolean(s.buy);
    a.i32(s.list_index);
}

static_assert(field_count<BuildGoods>() == 2,
              "a BuildGoods member needs a line in fields(BuildGoods)");

template <class A, Same<BuildGoods> S> void fields(A& a, S& s) {
    a.u32(s.setting);
    a.u64(s.spent);
}

static_assert(field_count<TradeEntry>() == 3,
              "a TradeEntry member needs a line in fields(TradeEntry)");

template <class A, Same<TradeEntry> S> void fields(A& a, S& s) {
    a.u32(s.goods);
    a.u32(s.amount_88);
    a.u32(s.price);
}

static_assert(field_count<City>() == 8, "a City member needs a line in fields(City)");

/// One city (docs/sim/state.md §9.3): `u32 number, u32 owner, u16 tick_stamp`, the
/// stock (`u32 capacity`, u32 list), the goods settings, the build goods, the trade
/// lists (a list of lists), then the scope `index`. The sizes of the config-sized
/// containers are checked against the configuration by validate_cities.
template <class A, Same<City> S> void fields(A& a, S& s) {
    a.u32(s.number.v);
    a.u32(s.owner.v);
    a.u16(s.tick_stamp);
    fields(a, s.stock);
    ar::seq(a, s.goods, 9, [&](auto& g) { fields(a, g); });
    ar::seq(a, s.build_goods, 12, [&](auto& g) { fields(a, g); });
    ar::seq(a, s.trade_lists, 8,
            [&](auto& list) { ar::seq(a, list, 12, [&](auto& e) { fields(a, e); }); });
    fields(a, s.index);
}

/// The city table of an island (the scope `cities`): `u64 slot count`, then per slot
/// (scope `city`, ordinal = slot) an optional City. On reading the count is bounded by
/// kSlotTableBound before any slot is made (an absent slot is one byte on the wire
/// but a whole optional<City> in memory, F29); validate_cities then compares it with
/// SimConfig::city_slots.
template <class A, class V> void cities_io(A& a, V& cities) {
    std::uint64_t n = cities.size();
    a.count(n, 1);
    if constexpr (A::kReading) {
        if (!a.failed() && n > kSlotTableBound) {
            a.fail("slot count " + std::to_string(n) + " is above the table limit " +
                   std::to_string(kSlotTableBound));
            n = 0;
        }
        cities.clear();
        for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
            cities.emplace_back();
            a.scope("city", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, cities.back(), [&](auto& c) { fields(a, c); }); });
        }
    } else {
        for (std::size_t i = 0; i < cities.size(); ++i) {
            a.scope("city", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, cities[i], [&](auto& c) { fields(a, c); }); });
        }
    }
}

// Island has no user-declared constructor on purpose (C++20), so that the canary
// can count its members: index, tiles, terrain, climate, deposits, next_serial,
// sweep, event300_stamp, drought, objmgr, ring, cities, reserved_object_count, cache
// (the derived index cache is not in the stream).
static_assert(field_count<Island>() == 14,
              "an Island member needs a line in fields(Island) or a derived note");

/// One island (docs/sim/state.md §9.3): `u32 index`, the rectangle `i32 x0, i32 y0,
/// u32 w, u32 h`, `u32 climate, u32 next_serial, u64 terrain_digest` (0 if none),
/// the sweep, `u16 event300_stamp`, the object manager, the ring, the optional
/// drought, the deposits, `u64 reserved_object_count`, scope `tiles`, scope `cities`
/// (stage S6). The terrain itself is not in the stream (its blobs travel in the save frame, §9.5):
/// reading leaves a placeholder that carries the digest, and the frame replaces it by the verified
/// blobs.
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
    a.scope("cities", [&] { cities_io(a, s.cities); });
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

// ---- ids, figures and the event queue (stage S7)

static_assert(field_count<IdCounters>() == 2,
              "an IdCounters member needs a line in fields(IdCounters)");

template <class A, Same<IdCounters> S> void fields(A& a, S& s) {
    a.u64(s.next_figure);
    a.u64(s.next_event_seq);
}

template <> struct EnumInfo<TransportStep> : ContiguousEnum<5> {};

static_assert(field_count<TransportState>() == 7,
              "a TransportState member needs a line in fields(TransportState)");

template <class A, Same<TransportState> S> void fields(A& a, S& s) {
    a.enumv(s.step);
    a.u32(s.wait);
    ar::seq(a, s.route, 8, [&](auto& p) {
        a.i32(p.x);
        a.i32(p.y);
    });
    a.u32(s.next);
    a.u32(s.along);
    a.u64(s.target.v);
    a.u32(s.reserved_88);
}

static_assert(field_count<Figure>() == 14, "a Figure member needs a line in fields(Figure)");

template <class A, Same<Figure> S> void fields(A& a, S& s) {
    a.u64(s.id.v);
    a.u32(s.kind);
    a.u32(s.owner.v);
    a.u64(s.home.v);
    a.u32(s.home_counter);
    a.u32(s.job);
    a.i32(s.pos.x);
    a.i32(s.pos.y);
    a.u32(s.cargo_goods);
    a.u32(s.cargo_88);
    a.u32(s.hp);
    a.u32(s.born);
    a.u16(s.stamp);
    fields(a, s.transport);
    ar::opt(a, s.expiry_stamp, [&](auto& v) { a.u16(v); });
}

/// The registry's slot list, like a building list: `i64 hint, u32 cursor, u64
/// slot_count`, then per slot (scope `figure`, ordinal = slot) an optional Figure.
/// A slot is one byte on the wire when empty but a whole optional<Figure> in
/// memory, so the count is bounded by kListSizeBound and by kSlotListLoadBytes before
/// any slot is made and the slots are made one at a time. validate_figures then
/// checks the structure.
template <class A, Same<SlotList<Figure>> S> void fields(A& a, S& s) {
    a.i64(s.hint);
    a.u32(s.cursor);
    std::uint64_t n = s.slots.size();
    a.count(n, 1);
    if constexpr (A::kReading) {
        if (!a.failed() && n > kListSizeBound) {
            a.fail("slot count " + std::to_string(n) + " is above the list limit " +
                   std::to_string(kListSizeBound));
            n = 0;
        }
        if (!a.failed() && !slot_list_fits_load_budget<Figure>(n)) {
            a.fail("slot count " + std::to_string(n) + " is above the load budget of " +
                   std::to_string(kSlotListLoadBytes) + " bytes per list");
            n = 0;
        }
        s.slots.clear();
        for (std::uint64_t i = 0; i < n && !a.failed(); ++i) {
            s.slots.emplace_back();
            a.scope("figure", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, s.slots.back(), [&](auto& f) { fields(a, f); }); });
        }
    } else {
        for (std::size_t i = 0; i < s.slots.size(); ++i) {
            a.scope("figure", static_cast<std::uint32_t>(i),
                    [&] { ar::opt(a, s.slots[i], [&](auto& f) { fields(a, f); }); });
        }
    }
}

// `by_id` is derived (rebuilt after a read) and is not in the stream.
static_assert(field_count<FigureRegistry>() == 4,
              "a FigureRegistry member needs a line in fields(FigureRegistry) or a derived note");

template <class A, Same<FigureRegistry> S> void fields(A& a, S& s) {
    fields(a, s.list);
    a.u16(s.stamp);
    ar::seq(a, s.expiry, 8, [&](auto& id) { a.u64(id.v); });
}

template <> struct EnumInfo<EventKind> : ContiguousEnum<2> {};

static_assert(field_count<TimedEvent>() == 7,
              "a TimedEvent member needs a line in fields(TimedEvent)");

template <class A, Same<TimedEvent> S> void fields(A& a, S& s) {
    a.enumv(s.kind);
    a.u32(s.subtype);
    a.u32(s.value);
    a.i32(s.tile.x);
    a.i32(s.tile.y);
    a.u64(s.target.v);
    a.u32(s.attacker_kind);
    a.u64(s.attacker.v);
}

static_assert(field_count<EventQueue>() == 1,
              "an EventQueue member needs a line in fields(EventQueue)");

/// The queue in key order: u64 count, then per event its key (u64 due, u64 seq) and
/// the event (53 bytes at least). Reading requires strictly ascending keys.
template <class A, Same<EventQueue> S> void fields(A& a, S& s) {
    ar::map(
        a, s.q, 53,
        [&](auto& k) {
            a.u64(k.first);
            a.u64(k.second);
        },
        [&](auto& e) { fields(a, e); });
}

// ---- the script timers and the monopoly table (stage S12)

static_assert(field_count<ScriptTimers>() == 3,
              "a ScriptTimers member needs a line in fields(ScriptTimers)");

/// `u32 last_tick`, the running flags (u8, count + elements) and the counters (u32,
/// count + elements); reading bounds both counts by the input, StateIO checks the
/// sizes against the configuration (validate_timers).
template <class A, Same<ScriptTimers> S> void fields(A& a, S& s) {
    a.u32(s.last_tick);
    ar::seq(a, s.active, 1, [&](auto& v) { a.u8(v); });
    ar::seq(a, s.counter, 4, [&](auto& v) { a.u32(v); });
}

static_assert(field_count<MonopolyEntry>() == 2,
              "a MonopolyEntry member needs a line in fields(MonopolyEntry)");

template <class A, Same<MonopolyEntry> S> void fields(A& a, S& s) {
    a.u32(s.time_s);
    ar::opt(a, s.player, [&](auto& p) { a.u32(p.v); });
}

// MonopolyTable::acc is in the ids scope (docs/sim/state.md §9.3), not here.
static_assert(field_count<MonopolyTable>() == 2,
              "a MonopolyTable member needs a line in fields(MonopolyTable) or the ids scope");

/// The entries (count, then each `u32 time_s, optional u32 player`, 5 bytes at least).
template <class A, Same<MonopolyTable> S> void fields(A& a, S& s) {
    ar::seq(a, s.entries, 5, [&](auto& e) { fields(a, e); });
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
                        problem = config.validate(*w.types_);
                        if (problem.empty()) {
                            problem = w.types_->validate_against(config);
                        }
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
            std::uint64_t walk_digest =
                w.types_->walk_tables() ? walk_tables_digest(*w.types_->walk_tables()) : 0;
            a.u64(walk_digest);
            if constexpr (A::kReading) {
                const std::uint64_t expected =
                    w.types_->walk_tables() ? walk_tables_digest(*w.types_->walk_tables()) : 0;
                if (!a.failed() && walk_digest != expected) {
                    a.fail("walk-table digest differs from the world's tables");
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

    /// Section 4: the id counters, then the two millisecond accumulators of the
    /// world-level events and the draw-guard flag (stage S12: `u32 event750_acc, u32
    /// monopoly.acc, bool faulted`). Reading rejects a counter of 0 (ids start at 1).
    /// The maximum is accepted: it is the "used up" state a counter reaches after it
    /// handed out 2^64 - 2 (the last valid id; 2^64 - 1 is the invalid id), and a
    /// counter in it refuses further ids instead of wrapping (F35).
    template <class A, class Ids, class U32, class Bool>
    static void ids_section(A& a, Ids& ids, U32& event750_acc, U32& monopoly_acc, Bool& faulted) {
        a.scope("ids", [&] {
            fields(a, ids);
            a.u32(event750_acc);
            a.u32(monopoly_acc);
            a.boolean(faulted);
            if constexpr (A::kReading) {
                if (!a.failed() && ids.next_figure < 1) {
                    a.fail("next_figure " + std::to_string(ids.next_figure) +
                           " is not in 1 .. 2^64 - 1");
                }
                if (!a.failed() && ids.next_event_seq < 1) {
                    a.fail("next_event_seq " + std::to_string(ids.next_event_seq) +
                           " is not in 1 .. 2^64 - 1");
                }
            }
        });
    }

    /// Section 5.
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

    /// Section 6. Reading validates the table against the configuration, the
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

    /// Section 7: the figure registry. Reading rebuilds the id map (derived) and
    /// validates the structure against the id counters just read.
    template <class A, class Figures, class Ids>
    static void figures_section(A& a, Figures& figures, const Ids& ids) {
        a.scope("figures", [&] {
            fields(a, figures);
            if constexpr (A::kReading) {
                if (!a.failed()) {
                    rebuild_figure_index(figures);
                    const std::string problem = validate_figures(figures, ids);
                    if (!problem.empty()) {
                        a.fail(problem);
                    }
                }
            }
        });
    }

    /// Section 8: the timed-event queue. Reading rejects a sequence number the id
    /// counter has not handed out (a later enqueue could clash with it).
    template <class A, class Events, class Ids>
    static void events_section(A& a, Events& events, const Ids& ids) {
        a.scope("events", [&] {
            fields(a, events);
            if constexpr (A::kReading) {
                if (!a.failed()) {
                    for (const auto& entry : events.q) {
                        if (entry.first.second >= ids.next_event_seq) {
                            a.fail("event sequence number " + std::to_string(entry.first.second) +
                                   " is not below next_event_seq " +
                                   std::to_string(ids.next_event_seq));
                            break;
                        }
                    }
                }
            }
        });
    }

    /// Section 9 (stage S12): the script timers and the monopoly table. Reading
    /// checks their sizes against the configuration just read and the players the
    /// monopoly entries name against the player table's size.
    template <class A, class Timers, class Monopoly, class Config>
    static void timers_monopoly_section(A& a, Timers& timers, Monopoly& monopoly,
                                        const Config& config) {
        a.scope("timers_monopoly", [&] {
            fields(a, timers);
            fields(a, monopoly);
            if constexpr (A::kReading) {
                if (!a.failed()) {
                    std::string problem = validate_timers(timers, config);
                    if (problem.empty()) {
                        problem = validate_monopoly(monopoly, config);
                    }
                    if (!problem.empty()) {
                        a.fail(problem);
                    }
                }
            }
        });
    }

    /// The scopes of the stream over the configuration, rules, settings, clock,
    /// PRNG, id counters, players, islands, figures and events to use: the world's
    /// own members when writing or hashing, the read-side temporaries when reading
    /// (so one function defines the order of the stream for all four archives).
    template <class A, class W, class Config, class RulesT, class Settings, class Clock, class Rng,
              class Ids, class Players, class Islands, class Figures, class Events, class Timers,
              class Monopoly, class U32, class Bool>
    static void scopes(A& a, W& w, Config& config, RulesT& rules, Settings& settings, Clock& clock,
                       Rng& rng, Ids& ids, Players& players, Islands& islands, Figures& figures,
                       Events& events, Timers& timers, Monopoly& monopoly, U32& event750_acc,
                       Bool& faulted) {
        header_section(a, w, config, rules);
        clock_rng_section(a, clock, rng);
        settings_section(a, settings);
        ids_section(a, ids, event750_acc, monopoly.acc, faulted);
        players_section(a, players, config);
        islands_section(a, w, islands, config, settings);
        figures_section(a, figures, ids);
        events_section(a, events, ids);
        timers_monopoly_section(a, timers, monopoly, config);
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
            WorldClock clock = w.clock_;
            WorldRng rng = w.rng_;
            IdCounters ids = w.ids_;
            PlayerTable players = w.players_;
            std::vector<std::optional<Island>> islands;
            FigureRegistry figures = w.figures_;
            EventQueue events = w.events_;
            ScriptTimers timers = w.timers_;
            MonopolyTable monopoly = w.monopoly_;
            std::uint32_t event750_acc = w.event750_acc_;
            bool faulted = w.faulted_;
            scopes(a, w, config, rules, settings, clock, rng, ids, players, islands, figures,
                   events, timers, monopoly, event750_acc, faulted);
            if (!a.failed()) {
                // The derived indexes (id map, footprint grids, counters) are not in
                // the stream: rebuilt from the lists (docs/sim/state.md §9.1).
                for (std::optional<Island>& is : islands) {
                    if (is.has_value()) {
                        rebuild_island_index(*is, config, *w.types_);
                    }
                }
                w.config_ = std::move(config);
                w.rules_ = rules;
                w.settings_ = settings;
                w.clock_ = clock;
                w.rng_ = rng;
                w.ids_ = ids;
                w.players_ = std::move(players);
                w.islands_ = std::move(islands);
                w.figures_ = std::move(figures);
                w.events_ = std::move(events);
                w.timers_ = std::move(timers);
                w.monopoly_ = std::move(monopoly);
                w.event750_acc_ = event750_acc;
                w.faulted_ = faulted;
            }
        } else {
            scopes(a, w, w.config_, w.rules_, w.settings_, w.clock_, w.rng_, w.ids_, w.players_,
                   w.islands_, w.figures_, w.events_, w.timers_, w.monopoly_, w.event750_acc_,
                   w.faulted_);
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

/// The world: scopes header, clock_rng, settings, ids, players, islands, figures,
/// events and timers_monopoly (the order of §9.3).
template <class A, Same<World> S> void fields(A& a, S& w) {
    StateIO::world(a, w);
}

} // namespace anno::sim
