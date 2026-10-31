// A legal, deterministic city workload for the picture's measurements (round dy, B2-B4;
// docs/perf/README.md, "Legal city workload"). A TEST FIXTURE, not a product path: it seats one
// human player per start region of a fixed map with the developer start (one city owning its
// start island, a Kontor, documented test resources) and builds each city through the production
// command path (sim::run_turn with Build commands). The sim validates and charges every placement
// as it does in play; the fixture only chooses candidates. What the current sim does not support
// is recorded, not worked around: roads are refused (Unsupported, O-4), and the placement rule is
// the D17 stand-in (tile objects and overlap only; the coast, mountain and terrain checks of the
// original are unread, O-27/OQ-052), so types that need a coast or a mountain are left out rather
// than placed where the original would refuse them, and a candidate whose footprint leaves land
// (the original's land classifier, render::kLandAverage, AnnoWelt.dll 0x100900c0) is not issued.
// Types whose Anlagen entry has no sprite set are left out too: the sim places them, but they draw
// nothing, so they are not city buildings.
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/play.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

struct LegalWorkloadOptions {
    /// Cities (human seats, one per start region); at most the layout's start regions.
    std::size_t cities = 5;
    /// The candidate order (positions, types, rotations).
    std::uint64_t seed = 1;
    /// Accepted buildings wanted per city, besides its Kontor. The search stops a city there.
    std::size_t buildings_per_city = 40;
    /// Extra buildings wanted in every city but city 0 (the unrelated cities of a local-edit
    /// measurement). Each city's search has its own stream (seed and city), so city 0 is built
    /// the same whatever the others hold.
    std::size_t unrelated_extra = 0;
    /// Candidate positions tried per city before it reports its capacity (a bounded search).
    std::size_t attempts_per_city = 20000;
    /// Road placements issued per city (the current sim refuses them; counted as evidence).
    std::size_t roads_per_city = 4;
    /// Placements issued per turn, one Build command each (a refusal names its own record).
    std::size_t placements_per_turn = 64;
    /// Turns run after the placements, so the cities' producers and houses work (sim time).
    std::size_t settle_turns = 30;
    /// The developer's test resources (documented; the sim charges them as any other).
    std::int64_t gold = 10000000;
    std::uint32_t goods_tons = 190;
};

/// One city of the workload and what its construction did.
struct LegalCity {
    std::uint32_t island_slot = 0, city = 0, player = 0;
    std::uint64_t island_tiles = 0, owned_tiles = 0;
    /// Owned tiles that are land (the original's land classifier on the island's HEIGHT grid), and
    /// of them the ones with no tile object at load (scenery and growing items cover the rest; the
    /// stand-in placement rule refuses a footprint over any object, so these bound the density).
    std::uint64_t land_tiles = 0, free_land_tiles = 0;
    /// Tiles under a building's footprint (the Kontor included).
    std::uint64_t occupied_tiles = 0;
    /// Positions tried; refused before a command by the sim's own placement rules (not issued);
    /// refused before a command because the footprint leaves land (the fixture's rule, not the
    /// sim's: the stand-in does not read terrain); commands issued; buildings the sim placed.
    std::size_t candidates = 0, prefiltered = 0, not_land = 0, issued = 0, accepted = 0;
    /// The pre-filtered candidates by the rule that refused them: the footprint owner rule (off
    /// the island or not the city's tiles), the placement rule (a tile object: scenery, a growing
    /// item), an overlap with a building (or a candidate of the same turn), or the cost.
    std::size_t refused_owner = 0, refused_object = 0, refused_overlap = 0, refused_cost = 0;
    /// Refusals by the sim, by reason (sim::reject_name).
    std::map<std::string, std::size_t> rejected;
    /// Placed buildings by type id, the Kontor included.
    std::map<std::uint32_t, std::size_t> by_type;
    /// The search ran out of attempts before the wanted count.
    bool capacity_reached = false;
    /// Goods (8.8 tons, summed over goods) in the city's stock after the placements and after
    /// the settle turns: evidence that the sim ran the city, not a production claim.
    std::uint64_t stock_after_build = 0, stock_after_settle = 0;
};

/// The outcome of one edit (a build, a demolish, or turns with no command).
struct LegalEdit {
    bool accepted = false;
    /// The sim's reason for a refusal (empty when accepted).
    std::string reason;
    /// The building changes since the previous edit (null when none), as PlayRunner publishes.
    std::shared_ptr<const BuildingBatch> batch;
    /// The island slot and the island-local tile of the edit's building.
    std::uint32_t island = 0;
    std::int32_t x = 0, y = 0;
    std::uint32_t type = 0;
    std::string what;
    /// Milliseconds in sim::run_turn (the production turn), and in the building diff that
    /// publishes its changes (PlayRunner::building_batch's work); the candidate search and the
    /// invariant check are the fixture's and are not in either.
    double turn_ms = 0, diff_ms = 0;
};

/// The workload's world and its record. Not copyable (it owns the world).
class LegalWorkload {
  public:
    /// Loads `map_file` (a fixed map) from `game_dir` with `options.cities` seats and builds the
    /// cities. Fails with a message when the map cannot be loaded, when a city is missing, or when
    /// the world's invariants do not hold after the construction.
    [[nodiscard]] static formats::Result<std::unique_ptr<LegalWorkload>, std::string>
    build(const std::filesystem::path& game_dir, const std::filesystem::path& map_file,
          const LegalWorkloadOptions& options);

    LegalWorkload(const LegalWorkload&) = delete;
    LegalWorkload& operator=(const LegalWorkload&) = delete;

    [[nodiscard]] const PlayLoad& load() const { return load_; }
    [[nodiscard]] const sim::World& world() const { return load_.world; }
    [[nodiscard]] const std::vector<LegalCity>& cities() const { return cities_; }
    /// The world's buildings as the picture sees them, sorted (play_buildings).
    [[nodiscard]] const std::vector<SceneBuilding>& buildings() const { return sorted_; }
    /// World::check_invariants() after the last change (empty: they hold).
    [[nodiscard]] const std::string& invariants() const { return invariants_; }
    [[nodiscard]] std::uint64_t state_hash() const { return load_.world.state_hash(); }
    [[nodiscard]] std::uint64_t registry_digest() const { return load_.world.types().digest(); }
    [[nodiscard]] std::uint64_t turns() const { return turn_; }
    /// Whether every edit checks the world's invariants afterwards (on by default; a contention
    /// world turns it off, so its thread does the production turn's work only).
    void set_check_invariants(bool on) { check_each_edit_ = on; }

    /// Places one more building in city `city` (the next legal candidate of the search), through
    /// one Build command and one turn. Not accepted when the search finds no candidate within
    /// `attempts` positions.
    LegalEdit build_one(std::size_t city, std::size_t attempts = 4000);
    /// Demolishes one of city `city`'s buildings (not its Kontor), chosen by the workload's
    /// generator, through one Demolish command and one turn.
    LegalEdit demolish_one(std::size_t city);
    /// Runs `turns` turns with no command (the sim's own work).
    LegalEdit advance(std::size_t turns);

    /// The manifest: options, identities, per-city counts, refusals, invariants and hashes, as
    /// `key=value` lines (saved with every run that uses the workload).
    [[nodiscard]] std::string manifest() const;

  private:
    LegalWorkload() = default;
    /// The next candidate placement of `city` that the sim's own rules would accept; tries at
    /// most `attempts` positions.
    bool next_candidate(std::size_t city, std::size_t attempts, sim::PlacementWire& out);
    /// Runs one turn with `commands`; returns the refusal reasons of that turn, in order.
    std::vector<std::string> run(std::vector<sim::Command> commands, std::uint32_t steps = 1);
    /// The batch of building changes since the last call (sorted diff).
    std::shared_ptr<const BuildingBatch> take_batch();
    void count_city(LegalCity& c) const;
    /// The next value of city `c`'s stream (xorshift64*, seeded by the seed and the city).
    std::uint64_t next_random(std::size_t c);

    PlayLoad load_;
    LegalWorkloadOptions options_;
    std::vector<LegalCity> cities_;
    std::vector<SceneBuilding> sorted_;
    std::string invariants_;
    std::uint64_t turn_ = 0;
    std::vector<std::uint64_t> rng_;
    bool check_each_edit_ = true;
    /// The duration of the last run(), in milliseconds.
    double last_turn_ms_ = 0;
    /// Per city: the owned land tiles in search order, and the next position of the search.
    std::vector<std::vector<std::uint64_t>> owned_;
    /// Per city: whether each tile of its island is land (row-major, the island's own size).
    std::vector<std::vector<bool>> land_;
    /// The candidates issued in the turn being prepared (one city per turn): their tiles and
    /// costs, so a later candidate of the same turn does not collide with them. Cleared by run().
    std::set<std::uint64_t> claimed_;
    std::int64_t pending_gold_ = 0;
    std::map<std::uint32_t, std::uint64_t> pending_goods_;
    std::vector<std::size_t> cursor_;
    /// The candidate types by kind (houses, producers, others), ascending id, and a road type
    /// (0 when the registry has none).
    std::vector<std::uint32_t> houses_, producers_, others_;
    std::uint32_t road_type_ = 0;
    /// Placeable types left out because their Anlagen entry has no sprite set (they draw nothing).
    std::size_t undrawn_types_ = 0;
    std::uint32_t kontor_type_ = 1302;
    std::uint64_t map_digest_ = 0;
    std::uint64_t roads_issued_ = 0;
    std::map<std::string, std::size_t> road_rejected_;
};

} // namespace anno::app
