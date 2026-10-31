// The simulated world (spec: docs/sim/core.md, docs/sim/state.md). For now
// the clock, the PRNG, the immutable context a world is created with
// (configuration, rules, settings, seams and the type registry; stage S3), the
// player table (stage S4) and the islands with their tiles (stage S5; World::create
// builds a world from a ScenarioSpec, save and load move it through the save
// frame), the cities with their buildings (stage S6: World::create places them
// in file order, check_invariants checks the structure and the derived indexes).
// Figures follow as their rules are recovered (PLAN M3, docs/sim/state.md §15).
// Since stage S8 the commands (apply) and advance are the only way to change it from
// outside: the clock and the PRNG are private with const accessors.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/events.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/hash_tree.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/tick.hpp>
#include <anno/sim/timers.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

struct TransportStats; // transport.hpp (T29, measurement only; not sim state)

// Development counter until the stream is complete (stage S15), then FROZEN
// as 2 (the first released stream). Values 100, 101, ... are development
// versions: the counter is bumped with every change of the field list (every
// stage S1..S14 changes it), goldens made at a development version carry the
// stage and the version they were made at (docs/sim/state.md §14.3), and all
// of them are re-baselined in the one commit that sets the value to 2. 1 is
// the legacy core_hash. After S15 every field-list change is version 3, 4, ...
// Stream at 100 (stage S1): header (the two versions), clock_rng.
// Stream at 101 (stage S3): header gains SimConfig, Rules, the registry digest
// and the seam names; new scope settings.
// Stream at 102 (stage S4): new scope players (after settings).
// Stream at 103 (stage S5): new scope islands (after players).
// Stream at 104 (stage S6): each island gains the scope cities (after tiles): per
// city slot a scope city with an optional City, whose building lists are the scopes
// index / list / building.
// Stream at 105 (stage S7): new scope ids (after settings: u64 next_figure, u64
// next_event_seq), new scopes figures (the FigureRegistry) and events (the timed-event
// queue) after islands.
// Stream at 106 (stage S12): the ids scope gains u32 event750_acc, u32 monopoly.acc and
// bool faulted; new scope timers_monopoly after events (the script timers and the
// monopoly entries). advance() now also writes the figure registry's stamp and cursor.
// Stream at 107 (stage S12 fix-up): Rules gains timer0_clamp (the header scope).
// Stream at 108 (stage S13): the drought's city_mask is u64 count + u32 words
// (ceil(city_slots / 32) words, O-55) instead of one u32.
// Stream at 109 (task T22, OQ-131): the header's SimConfig gains figure_min_growth, the
// figure record loses marked_for_delete and the figure registry its deferred list.
// Stream at 110 (stage S14 stage 2): the header's SimConfig gains links (LinkTables, the
// island-link selection's tables) and Rules gains island_candidate_order.
// Stream at 111 (task T24, transport stage 1): the header's registry digest gains the
// building types' entrance points and the figure types; the payloads gain the cart
// reservations (Workshop, WeaponsWorkshop, SmallFarm per line, Farm and Mine one) and
// the alternative Market (tag 6, carts_out); the figure record gains its stamp and
// the transport state (docs/sim/state.md §4.9, §5.9).
// Stream at 113 (task T33.5 stage 2, fix-up F1): each tile gains foot_dx (u16),
// foot_dy (u16), foot_rotation (u8) and foot_anchor (u8). kTileBytes 17 → 23.
// F1 adds the anchor without a further bump: 113 had not been recorded yet.
// Stream at 114 (task T33.5 stage 3): Rules gains removal_clear_tile, one byte
// of the hashed header. The tile columns are unchanged.
// Stream at 115 (task P1, population stage 1): the building variant gains the alternative
// House (tag 7: u16 last, i32 residents_64, u32 e, u64 count + u32 stock); the class table's
// House row is tick House / payload House; the registry digest gains the group table
// (u64 count, 0 when there is none). No PRNG site is drawn (the timeline stays 2).
// Stream at 116 (task MAP-2, the ship-start load): the figure record gains u8 dir16, u64
// parent (invalid = max), an optional u32 crew slot and the ship's holds (u64 count, then u32
// goods, u32 tons each). A figure that no loader gave these keeps 0, invalid, none and empty,
// so a world without ships changes hash only because its stream grew. No PRNG site.
inline constexpr std::uint32_t kStateHashVersion = 116;
/// The version of the save frame (docs/sim/state.md §9.5): the layout around the
/// state stream. A save written by version v is rejected by v + 1.
inline constexpr std::uint32_t kSaveVersion = 1;
/// The PRNG timeline: which original draw sites exist in the sim (docs/sim/
/// state.md §6). Porting a site is a recorded bump, never a silent desync.
/// Version 2 (S14 stage 2): the first load-time sites, the island-link selection's
/// LinkRawMaterial, LinkOre, LinkTemplatePick, LinkSpecialMark and LinkTreasure
/// (D AnnoScene 0x10009550), and the unported load sites LinkOrePlace,
/// LinkNativeName, LinkTreasureKind, LinkTreasurePlace (0x10008490) and
/// ComputerName (0x100025a0) in the manifest.
inline constexpr std::uint32_t kPrngTimelineVersion = 2;

struct StateIO;
struct ScenarioSpec;

/// The UI events waiting for the host. Not state (never hashed or saved); a World
/// copy carries at most `capacity` events. A full queue drops its OLDEST event for
/// each new one and counts it in `dropped`.
struct HostEventQueue {
    std::deque<SimEvent> events;
    std::uint64_t capacity = kHostEventCapacityDefault;
    std::uint64_t dropped = 0;

    /// Drops the oldest events until at most `capacity` remain.
    void trim() {
        while (static_cast<std::uint64_t>(events.size()) > capacity) {
            events.pop_front();
            ++dropped;
        }
    }
    void push(const SimEvent& e) {
        events.push_back(e);
        trim();
    }
};

class World {
  public:
    /// An empty world: the original configuration, default rules and settings,
    /// no seams installed, and the empty type registry (docs/sim/state.md
    /// §4.14). World::create (S5) builds the real ones.
    World();
    /// A world whose PRNG cursor is `seed mod table size` (the host's game
    /// seed, D: AnnoFrame.dll 0x1009b978 → AnnoScene.dll 0x10002e40).
    explicit World(std::uint32_t seed);

    /// Builds a world from a scenario (docs/sim/state.md §10): validates the
    /// configuration, the rules, the layout, the seams, the registry's fit and the
    /// settings first (§3.1), then the scenario, naming the offending path in one
    /// message ("islands[0].deposits[1]: ..."), and only then builds the state:
    /// players by slot, islands by slot (the content of the islands is built on
    /// `ex`, one island per job, and placed in table order, so the result does not
    /// depend on the executor). The clock starts at 0 and the PRNG cursor at
    /// `seed mod table size`.
    static formats::Result<World, std::string> create(const ScenarioSpec& spec,
                                                      std::shared_ptr<const TypeRegistry> types,
                                                      SimConfig config = {}, Rules rules = {},
                                                      Seams seams = {},
                                                      const Executor& ex = serial_executor());

    /// Reads a save (docs/sim/state.md §9.5): checks the frame, the versions, the
    /// registry digest and the seam names, validates the configuration before any
    /// container is sized from it, reads the sections with the bounds-checked
    /// archive and verifies the terrain blobs. Every failure is an error message
    /// that names the scope path; none is an exception. The registry and the seams
    /// are not in the save (their digest and names are, and must match).
    static formats::Result<World, std::string> load(std::span<const std::uint8_t> bytes,
                                                    std::shared_ptr<const TypeRegistry> types,
                                                    Seams seams = {});

    /// The save frame of the state (docs/sim/state.md §9.5). `load(save(w))` has the
    /// same hash, and `save(load(save(w))) == save(w)` byte for byte. Precondition:
    /// every island's terrain satisfies terrain_matches(); a world filled from the
    /// flat state stream alone holds digest-only placeholders and is for hashing
    /// only (a frame written from it is refused by load, and a debug build asserts).
    [[nodiscard]] std::vector<std::uint8_t> save() const;

    /// The clock and the PRNG (stage S8: private; the world is changed only through
    /// apply() and advance()). Const views for the host, the UI and the tests.
    [[nodiscard]] const WorldClock& clock() const { return clock_; }
    [[nodiscard]] const WorldRng& rng() const { return rng_; }
    [[nodiscard]] const WorldSettings& settings() const { return settings_; }
    [[nodiscard]] const SimConfig& config() const { return config_; }
    [[nodiscard]] const Rules& rules() const { return rules_; }
    [[nodiscard]] const Seams& seams() const { return seams_; }
    [[nodiscard]] const TypeRegistry& types() const { return *types_; }
    [[nodiscard]] const PlayerTable& players() const { return players_; }
    /// The counters of the ids assigned in the serial merge (figures, event sequence
    /// numbers).
    [[nodiscard]] const IdCounters& ids() const { return ids_; }
    /// The figure registry (stage S7: created by the merge of SpawnRequests; ticked by
    /// advance since stage S12).
    [[nodiscard]] const FigureRegistry& figures() const { return figures_; }
    /// The timed-event queue (stage S7: filled by the merge of EnqueueEvents;
    /// dispatched by advance since stage S12).
    [[nodiscard]] const EventQueue& events() const { return events_; }
    /// The script timers (stage S12; D: the SKTIMER object behind world +0x69c8).
    [[nodiscard]] const ScriptTimers& timers() const { return timers_; }
    /// The monopoly table with its accumulator (stage S12; D: world +4, +0xe8).
    [[nodiscard]] const MonopolyTable& monopoly() const { return monopoly_; }
    /// The 7.5 minute event's accumulator in ms (stage S12; D: world +0x69e4).
    [[nodiscard]] std::uint32_t event750_acc() const { return event750_acc_; }
    /// Whether a draw-guard violation was recorded (docs/sim/state.md §11, stage
    /// S16). Hashed and saved; nothing sets it before S16, so it is always false.
    [[nodiscard]] bool faulted() const { return faulted_; }
    /// What the last advance produced for the host (UI events of that advance, the
    /// draws per site, the PRNG cursor before and after). Not state, not hashed.
    [[nodiscard]] const SimOutput& last_output() const { return out_; }
    /// The UI events since the last call, oldest first (the queue is emptied). Not
    /// state, not hashed. At most event_capacity() events are kept between two calls;
    /// when more arrive the oldest are dropped and counted in events_dropped().
    [[nodiscard]] std::vector<SimEvent> drain_events();
    /// The most UI events kept for drain_events() (default kHostEventCapacityDefault;
    /// 0 keeps none). Lowering it drops the oldest queued events at once. Not state.
    void set_event_capacity(std::uint64_t capacity);
    [[nodiscard]] std::uint64_t event_capacity() const;
    /// How many UI events were dropped for lack of capacity since the world was made
    /// (the per-advance list SimOutput::events is never dropped). Not state.
    [[nodiscard]] std::uint64_t events_dropped() const;
    /// Routes a transport-stats sink into every TickContext Scheduler::advance builds
    /// (task T29, measurement only; TransportStats declared in transport.hpp). Not
    /// state, not hashed, not saved: the pointer is held in a non-state member and is
    /// not restored by load. Default and test-only; the sim leaves it null.
    void set_transport_stats(TransportStats* stats);
    /// The currently installed transport-stats sink (nullptr by default). Not state.
    [[nodiscard]] TransportStats* transport_stats() const;
    /// The island table, slot order (null slots are unused slots).
    [[nodiscard]] const std::vector<std::optional<Island>>& islands() const { return islands_; }
    /// The island whose rectangle holds the world tile (x, y), or nullptr. The
    /// rectangles are disjoint, so there is at most one; the lookup scans the
    /// table (a derived grid, D: world +0x130, is a later optimisation). Both
    /// overloads share one scan body (sim.cpp); the mutable one is the single
    /// island lookup for command apply (§8.4, §4.14).
    [[nodiscard]] const Island* island_at(std::int32_t x, std::int32_t y) const;
    [[nodiscard]] Island* island_at(std::int32_t x, std::int32_t y);
    /// The building with this id (the island named by the id, then its by_id
    /// cache), or nullptr: a removed building is "gone", never an error.
    [[nodiscard]] const Building* find_building(BuildingId id) const;
    /// The building of city `city` on the world tile (x, y) in the lists named by
    /// `list_mask` (the layout's lookup order); nullptr when there is none, or the
    /// island, the city or the tile does not exist. These three delegate to the
    /// Island members of the same names (island.hpp), which they locate.
    [[nodiscard]] const Building* building_at(CityRef city, std::int32_t x, std::int32_t y,
                                              std::uint32_t list_mask = ~0U) const;
    /// Clears `out`, then appends the ids of the buildings of `city` in the masked
    /// lists whose footprint shares a tile with `rect`, in the canonical order
    /// (list in the layout's rect order, y, x, id; D2).
    void rect_query(CityRef city, const TileRect& rect, std::uint32_t list_mask,
                    std::vector<BuildingId>& out) const;
    /// Applies one command at the current tick, between two advances (docs/sim/
    /// state.md §8). Total: any input yields a result, nothing throws. Validation is
    /// pure: a rejected command changes no hashed state (the hash before equals the
    /// hash after); it only queues a CommandRejected UI event for the host. The
    /// commands the host stamped come in order, one call each; the AI seam's commands
    /// are applied inside advance by the same code and are not logged by anyone.
    ApplyResult apply(const Command& command);

    /// The structural facts and the derived-equals-recomputed facts of docs/sim/
    /// state.md §9.6 that exist at this stage. Empty when they all hold, else one
    /// message that names the first violation with its path ("islands[0].cities[1].
    /// index.lists[4].slots[2].payload: ..."). It runs at the end of create() and
    /// load(); tests run it after every step. Structure first (so the derived checks
    /// never read a state the structure forbids): the configuration, the player
    /// table, the islands and their cities and buildings (validate_islands), the
    /// city owners, then the footprints and the derived indexes.
    [[nodiscard]] std::string check_invariants() const;

    /// Compile-time canary (never called): binds every data member of World.
    /// A member added without extending this line fails to compile here,
    /// which is the reminder to give it a line in fields(World)
    /// (state_fields.hpp) and to bump kStateHashVersion, so it cannot be left
    /// outside hash and save (docs/sim/state.md §14.4).
    static constexpr void field_list_canary(const World& w) {
        // host_events_, out_ and transport_stats_ are host-facing or diagnostic,
        // deliberately not in the field list (not state).
        [[maybe_unused]] const auto& [clock_member, rng_member, settings_member, config_member,
                                      rules_member, seams_member, types_member, ids_member,
                                      players_member, islands_member, figures_member, events_member,
                                      timers_member, monopoly_member, event750_acc_member,
                                      faulted_member, host_events_member, out_member,
                                      transport_stats_member] = w;
    }

    /// Advances the world by `game_ms` (original order, D: AnnoWelt.dll
    /// 0x100ae040, docs/sim/state.md §5.3): 1. the islands (with the merge of their
    /// effects, stage S7), 2. the timed events (S12), 3. the figure registry (S12),
    /// 4. the player table with the AI (S4, S8), 5. the script timers, 6. the monopoly
    /// refresh and 7. the 7.5 minute event (S12).
    /// `game_ms` == 0 is not special-cased: the original adds 0 and still runs
    /// every subsystem. Returns the new tick.
    std::uint64_t advance(std::uint32_t game_ms);

    /// Hash of the canonical state, version kStateHashVersion: hierarchical,
    /// over the one field list of state_fields.hpp (docs/sim/state.md §9).
    [[nodiscard]] std::uint64_t state_hash() const;
    /// The same value, with the scopes down to `depth` recorded in `tree`
    /// (docs/sim/state.md §9.4).
    [[nodiscard]] std::uint64_t state_hash(HashTree& tree, std::uint32_t depth) const;
    /// The legacy v1 hash (docs/sim/core.md): FNV-1a 64 of `clock.ms`,
    /// `clock.last_tick`, the PRNG table length, its digest and the cursor. Kept
    /// so the old golden stays checkable; state_hash() begins with the same
    /// five fields inside its clock_rng scope.
    [[nodiscard]] std::uint64_t core_hash() const;
    /// Computes the cached chunk digests of every island's tile layer on `ex` (one
    /// chunk per job). A pure speed-up: state_hash() returns the same value with or
    /// without it, and for every executor. It needs exclusive access to the world
    /// (it is the one place that writes the caches); state_hash() and core_hash()
    /// are const and never write them, so any number of threads may hash one
    /// snapshot at the same time (a net thread checksumming a copy while the host
    /// keeps ticking its own). Without a prepare_hash since the last write, a
    /// hash computes the dirty chunks again.
    void prepare_hash(const Executor& ex);

  private:
    friend struct StateIO;
    friend struct WorldTestAccess;
    friend struct Scheduler;
    friend struct Commands;

    WorldClock clock_;
    WorldRng rng_;
    WorldSettings settings_;
    SimConfig config_;
    Rules rules_;
    Seams seams_;
    RegistryRef types_; // never null, also after a move (types.hpp)
    IdCounters ids_;
    PlayerTable players_;
    std::vector<std::optional<Island>> islands_;
    FigureRegistry figures_;
    EventQueue events_;
    ScriptTimers timers_;
    MonopolyTable monopoly_;
    std::uint32_t event750_acc_ = 0;
    bool faulted_ = false;
    // Not state (never hashed or saved): the host-facing output.
    HostEventQueue host_events_;
    SimOutput out_;
    // Not state (not hashed, not saved): a transport-stats sink the T29 scenario test
    // routes into every TickContext Scheduler::advance builds. Default null; nothing
    // in the sim writes it.
    TransportStats* transport_stats_ = nullptr;
};

} // namespace anno::sim
