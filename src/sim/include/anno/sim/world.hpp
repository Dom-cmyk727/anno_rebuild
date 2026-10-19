// The simulated world (spec: docs/sim/core.md, docs/sim/state.md). For now
// the clock, the PRNG, the immutable context a world is created with
// (configuration, rules, settings, seams and the type registry; stage S3), the
// player table (stage S4) and the islands with their tiles (stage S5; World::create
// builds a world from a ScenarioSpec, save and load move it through the save
// frame), the cities with their buildings (stage S6: World::create places them
// in file order, check_invariants checks the structure and the derived indexes).
// Figures follow as their rules are recovered (PLAN M3, docs/sim/state.md §15).
// Once commands exist they become the only way to change it from outside; until
// then `clock` and `rng` are public for tests.
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <anno/formats/result.hpp>
#include <anno/sim/clock.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/executor.hpp>
#include <anno/sim/hash.hpp>
#include <anno/sim/hash_tree.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/rng.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

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
inline constexpr std::uint32_t kStateHashVersion = 104;
/// The version of the save frame (docs/sim/state.md §9.5): the layout around the
/// state stream. A save written by version v is rejected by v + 1.
inline constexpr std::uint32_t kSaveVersion = 1;
/// The PRNG timeline: which original draw sites exist in the sim (docs/sim/
/// state.md §6). Porting a site is a recorded bump, never a silent desync.
inline constexpr std::uint32_t kPrngTimelineVersion = 1;

struct StateIO;
struct ScenarioSpec;

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

    WorldClock clock;
    WorldRng rng;

    [[nodiscard]] const WorldSettings& settings() const { return settings_; }
    [[nodiscard]] const SimConfig& config() const { return config_; }
    [[nodiscard]] const Rules& rules() const { return rules_; }
    [[nodiscard]] const Seams& seams() const { return seams_; }
    [[nodiscard]] const TypeRegistry& types() const { return *types_; }
    [[nodiscard]] const PlayerTable& players() const { return players_; }
    /// The island table, slot order (null slots are unused slots).
    [[nodiscard]] const std::vector<std::optional<Island>>& islands() const { return islands_; }
    /// The island whose rectangle holds the world tile (x, y), or nullptr. The
    /// rectangles are disjoint, so there is at most one; the lookup scans the
    /// table (a derived grid, D: world +0x130, is a later optimisation).
    [[nodiscard]] const Island* island_at(std::int32_t x, std::int32_t y) const;
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
        [[maybe_unused]] const auto& [clock_member, rng_member, settings_member, config_member,
                                      rules_member, seams_member, types_member, players_member,
                                      islands_member] = w;
    }

    /// Advances the world by `game_ms` (original order, D: AnnoWelt.dll
    /// 0x100ae040: islands, timed events, figures, players, timers, 30 s and
    /// 7.5 min events; only the clock and the player table (step 4) exist yet).
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

    WorldSettings settings_;
    SimConfig config_;
    Rules rules_;
    Seams seams_;
    RegistryRef types_; // never null, also after a move (types.hpp)
    PlayerTable players_;
    std::vector<std::optional<Island>> islands_;
};

} // namespace anno::sim
