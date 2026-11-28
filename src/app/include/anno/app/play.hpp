// Play mode (docs/app/game-shell.md). The window never sees a World. This
// header is the app-side bridge: load the scenario, list the islands and
// buildings the scene is built from, and run turns on a thread that publishes
// a snapshot.
#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <anno/app/building_delta.hpp>
#include <anno/app/command_outcome.hpp>
#include <anno/app/command_queue.hpp>
#include <anno/app/commands_file.hpp>
#include <anno/app/dev_tool.hpp>
#include <anno/app/loader.hpp>
#include <anno/app/minimap.hpp>
#include <anno/app/opening_view.hpp>
#include <anno/app/status_icons.hpp>
#include <anno/app/territory_view.hpp>
#include <anno/formats/result.hpp>
#include <anno/sim/road_drag.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

/// One island the sim placed. `template_key` is "embedded" for an island that
/// was in the scenario file, otherwise the template name below Scenes/Islands
/// (`TemplateSummary::name`).
struct PlayIsland {
    std::uint32_t slot = 0;
    std::int32_t x = 0, y = 0;
    std::uint32_t w = 0, h = 0;
    std::string template_key;
};

/// A loaded scenario and the picture inputs taken from it. The world is the
/// sim thread's once `PlayRunner` is started; until then the caller owns it.
struct PlayLoad {
    sim::World world;
    std::vector<PlayIsland> islands;
    std::vector<LinkTemplate> links;
    /// A fixed map (the layout text, data/maps/*.txt): its scene is built from
    /// `placed` by load_placed_scene, not from a scenario file.
    bool fixed_map = false;
    std::vector<PlacedIsland> placed;
    /// The scene name (the file's stem).
    std::string name;
    /// The world's type registry (shared: the developer tool reads it after the world moves).
    std::shared_ptr<const sim::TypeRegistry> registry;
};

/// What a fixed map's play load does not take from the layout (sim_load::FixedMapOptions).
/// The defaults are `--play`'s: two human seats and the developer's test resources.
struct FixedPlayOptions {
    /// Human seats, start seat 0 first (at most the layout's start regions). A test fixture
    /// (the legal five-city workload, round dy B2) seats five; `--play` seats two.
    std::size_t seats = 2;
    /// The developer's test resources (sim_load::DeveloperResources): documented test
    /// money and goods, charged and refused by the sim as any other.
    std::int64_t gold = 10000000;
    std::uint32_t goods_tons = 190;
    /// Installs the named stand-in placement rule (sim::stand_in_placement_seam) instead of the
    /// terrain rule of MAP-BUILD-1: its verdict ignores the terrain, the item height it stores is
    /// the terrain's (docs/sim/ship-placement.md section 6). Only for suites that isolate another
    /// subsystem from terrain and build on island-relative tiles chosen before that rule existed;
    /// never the default, and the seam's name stays in the world's header hash.
    bool stand_in_placement = false;
    /// PLAY-LIVE-1a: the rules the created (and, for a scenario file, the loaded) world uses.
    /// Default `{}` is the original behaviour (what every start got before this option
    /// existed); `--rules living` gives `cli::apply_rules_profile({}, Living)`.
    sim::Rules rules;
    /// FISHER-VIEW-1: the persistent extensions the created world uses (--fisher-rules on sets
    /// `fisher_rules.enabled`). Default `{}`: every extension off, as before this option.
    sim::ExtensionContext extensions;
};

/// Registry, `load_scenario_with_links`, `World::create`. The link list names
/// the template each resolved link was placed from. A fixed map (`.txt`) is loaded with
/// `fixed` (ignored for a scenario file).
[[nodiscard]] formats::Result<PlayLoad, std::string>
load_play(const std::filesystem::path& game_dir, const std::filesystem::path& file,
          const FixedPlayOptions& fixed = {});

/// The world's buildings, in island-slot order then city, list, slot.
[[nodiscard]] std::vector<SceneBuilding> play_buildings(const sim::World& world);

/// The scenario loader's split: an Anlagen id whose type has a list in the layout.
[[nodiscard]] bool anlage_is_building(const sim::World& world, std::uint16_t anlage);

/// Whether an Anlagen id is a tracked non-building tile object for the play picture (roads first,
/// data-driven via `sim::kRoadFlag` without hardcoded type ids; later crops/trees widen this rule).
[[nodiscard]] bool anlage_is_tracked_tile_object(const sim::World& world, std::uint32_t anlage);

/// The world's tracked runtime tile objects (anchor tiles only, in island-slot order).
[[nodiscard]] std::vector<SceneTileObject> play_tile_objects(const sim::World& world);

/// The types the developer tool cycles (round ee, D1), registry data in TypeId order and of any id
/// width: every type whose PRODTYPE has a list in `layout` (what the sim's Build places;
/// without a list it answers Unsupported, apply.cpp) and every road type (kRoadFlag: the drag's
/// subject; roads are tile objects the sim does not build yet, so their outcome is Unsupported).
[[nodiscard]] std::vector<sim::TypeId> buildable_types(const sim::TypeRegistry& registry,
                                                       const sim::ListLayout& layout);

/// The developer tool's setup (dev_tool.hpp), read from `load` before the runner takes its world:
/// the registry, buildable_types under the world's layout, and the developer player's first city
/// with its island's tiles (none: the tool stays off).
[[nodiscard]] DevToolSetup dev_tool_setup(const PlayLoad& load,
                                          sim::PlayerId player = developer_player());

/// A read-only summary of one building, formatted from its Payload (building_inspect,
/// W4-SETTLEMENT-1 W3/W4: the developer inspection UI). Goods amounts are tons (8.8 fixed-point,
/// `kind`-specific fields only). For a class without a ported tick, `kind` is "Inert" and `status`
/// is empty.
struct BuildingInspection {
    sim::TileXY tile;
    sim::BuildingId id;
    sim::TypeId type;
    std::uint32_t prodtype = 0;
    /// The payload's runtime class name ("SmallFarm", "Workshop", "Farm", "Mine", "Market",
    /// "House", "Inn", or "Inert" for a class without a ported tick).
    std::string kind;
    /// One line of the kind-specific fields (output/raw/input stock in tons, activity,
    /// workers/animals/carriers out); empty for Inert.
    std::string status;
    /// FISHER-VIEW-1: a Farm's (the fisherman's hut's) figures as numbers, for a script's
    /// comparisons: output and raw stock in the payload's 8.8 fixed-point units, workers out.
    /// nullopt for every other kind.
    struct FarmFigures {
        std::uint32_t out_88 = 0;
        std::uint32_t raw_88 = 0;
        std::uint32_t workers_out = 0;
    };
    std::optional<FarmFigures> farm;
};

/// `city`'s building at world tile `tile` (World::building_at), formatted for the developer
/// inspection UI; nullopt when there is none. Read-only, never mutates `world`. Callable from any
/// thread that owns a `const World&` for as long as the call takes (the sim thread, inside
/// PlayRunner::publish; a test with its own World).
[[nodiscard]] std::optional<BuildingInspection>
inspect_building(const sim::World& world, sim::CityRef city, sim::TileXY tile);

/// What the window reads. Not a World.
/// UI-WINDOW-1 fix round R2, UI-STATUS-1 B: a city whose figures the status bar can show
/// (docs/re/ui.md 5.6): a city of the local player (its materials and residents), or another
/// player's city that knows the local player (City::player_mask; its residents only, AnnoFrame
/// 0x10009b5b..0x10009b84). Read on the sim thread at publication from existing read-only
/// accessors only: World::islands(), City::owner/number/stock/player_mask and
/// World::city_population. No state, stream, hash or draw changes.
struct CityCounters {
    sim::CityRef city;
    /// A city of the local player (the default; another player's city that knows the local
    /// player is published with false).
    bool own = true;
    /// The city's goods stock (8.8 tons per goods number); empty for another player's city (the
    /// original never shows a foreign city's materials).
    std::vector<std::uint32_t> goods;
    std::optional<std::int64_t> population;
    /// UI-TABS-1 stage B: a read-only copy of the city's accounts (sim::City::accounts: the
    /// statistics 0, 4, 5 and the cost buckets 1, 2 the balance pages read; docs/re/ui.md 5.5).
    sim::CityAccounts accounts;
};

/// UI-GHOST-1: one Build record of a city the placement ghost asks the sim about.
struct BuildProbe {
    sim::CityRef city;
    sim::PlacementWire record;
    friend bool operator==(const BuildProbe&, const BuildProbe&) = default;
};

/// The sim's verdict on a BuildProbe (World::check_build_record).
struct BuildProbeAnswer {
    BuildProbe probe;
    sim::Reject verdict = sim::Reject::None;
};

/// ROADS-3: multi-record road path the placement ghost asks the sim about
/// (World::check_build_path).
struct RoadPathProbe {
    sim::CityRef city;
    std::vector<sim::PlacementWire> records;
    friend bool operator==(const RoadPathProbe&, const RoadPathProbe&) = default;
};

/// The sim's prefix verdict on a RoadPathProbe (World::check_build_path).
struct RoadPathProbeAnswer {
    RoadPathProbe probe;
    sim::RoadPathCheck check;
    /// ROADS-2b: the records the commit would place after the reshaper (World::shape_road_path):
    /// the path's records with their shaped pieces, then the rewritten neighbours. The ghost draws
    /// these, so the preview equals the commit.
    std::vector<sim::PlacementWire> shaped;
};

struct PlaySnapshot {
    std::uint64_t tick = 0;
    std::uint64_t game_ms = 0;
    /// Cost of the last turn, and the p95 over turns that finished in the
    /// last second, in microseconds.
    std::uint64_t last_turn_us = 0;
    std::uint64_t p95_us = 0;
    /// Real milliseconds of schedule skipped after the catch-up bound.
    /// Reported, not run as extra steps.
    std::uint64_t behind_ms = 0;
    bool paused = false;
    /// UI-KEYS-1: the world's speed in percent after the turn (WorldSettings::speed_percent, set
    /// by SetSpeedCmd: F5-F8).
    std::uint32_t speed_percent = 100;
    /// The last turn this snapshot follows (its index), so a reader can check
    /// the snapshot against the mailbox.
    std::uint64_t turn = 0;
    /// The local player's money after the turn (UI-STATUS-1 B: also before its first city; 0
    /// when the world has no such player, `has_local_player` false), and the goods stock (8.8
    /// tons, per goods number) of its city, empty when the player has no city.
    bool has_local_player = false;
    std::int64_t money = 0;
    std::vector<std::uint32_t> goods;
    /// Build costs are waived for the developer player (its free-build flag, or the session's
    /// free-build rule): a cost the window compares with `money` and `goods` is then no refusal.
    bool costs_waived = false;
    /// The reason of the last refused command and that command ("NotLand (window command #3)"),
    /// kept for a few seconds of turns (empty when none), for the title. Each command's own outcome
    /// is outcome() / outcomes_since().
    std::string last_rejection;
    /// The next outcome sequence number after this turn (OutcomeLog): a reader that has read up
    /// to it has every outcome this snapshot follows.
    std::uint64_t outcomes_next = 1;
    /// The ship-start opening's facts (opening_view.hpp), taken in the same turn as this snapshot;
    /// null unless the runner was given a local participant for an opening (set_opening_player).
    std::shared_ptr<const OpeningView> opening;
    /// The developer inspection UI's answer for the last requested tile (request_inspect), taken
    /// in the same turn as this snapshot (W4-SETTLEMENT-1 W2: one coherent read, not a fresh query
    /// against a newer World); null when nothing was requested or the tile holds no building of
    /// the developer's city.
    std::shared_ptr<const BuildingInspection> inspection;
    /// The tile `inspection` answers (the last requested one), set whenever a tile was requested,
    /// even when no building of the player's city stands there or the player has no city (then
    /// `inspection` is null). The selection (selection.hpp) needs it to tell "answered: nothing
    /// there" from "not answered yet" (W4-NEXT-2 K).
    std::optional<sim::TileXY> inspected;
    /// UI-GHOST-1: the placement ghost's question (request_build_probe) and the sim's verdict on
    /// it, `World::check_build_record` taken in the same turn as this snapshot (a pure read: no
    /// state, stream, hash or draw change). nullopt when nothing is asked.
    std::optional<BuildProbeAnswer> build_probe;
    /// ROADS-3: multi-record road path probe answer (`World::check_build_path`).
    std::optional<RoadPathProbeAnswer> road_probe;
    /// `World::construction_eligibility(local player, type).allowed()` for every type the
    /// registry defines, ascending by id (UI.2 C2: the sim's own lock authority is the one and
    /// only source the construction menu's lock display reads — never a copy of it, and never
    /// `data/ui/construction.txt`'s recorded state, which is RE evidence about the original, not
    /// a live rule). A pure read with no state, stream, hash or draw effect (rule 0.2); computed
    /// and published here, once per turn, rather than let the window thread call
    /// `World::construction_eligibility` directly against the sim thread's own `World` (a race).
    /// Null when the player has no city yet, matching `money`/`goods` above.
    std::shared_ptr<const std::vector<std::pair<sim::TypeId, bool>>> eligibility;
    /// `World::city_population(developer_city_)` (H2: the HUD's residents counter), taken in the
    /// same turn as this snapshot. A new read-only sim query (rule 0.2: no state, stream, hash or
    /// draw change - it wraps `residents_at_or_above`'s own existing aggregate for a `CityRef`).
    /// nullopt when the player has no city yet, matching `money`/`goods`/`eligibility` above.
    std::optional<std::int64_t> population;
    /// UI-WINDOW-1 fix round R2, UI-STATUS-1 B: every city of the local player and every other
    /// player's city that knows the local player, in (island, city) order (CityCounters). Null
    /// when there is none.
    std::shared_ptr<const std::vector<CityCounters>> cities;
    /// UI-STATUS-1 B: the runner's local player (set_local_player), whose cities and discovered
    /// islands the status bar asks about.
    std::uint32_t local_player = 0;
    /// UI-MINIMAP-1 B2: every island's tile owners and cities (territory_view.hpp), taken at
    /// publication from read-only accessors (World::islands, TileLayer::owner, City number, owner
    /// and player_mask, SimConfig::neutral_owner). The same object is republished while the
    /// content does not change; its `version` grows when it does. Never null after the runner's
    /// constructor.
    std::shared_ptr<const TerritoryView> territory;
    /// UI-MINIMAP-1 B2: the figures the minimap marks (collect_map_figures), every turn.
    std::shared_ptr<const std::vector<MapFigure>> map_figures;
    /// UI-HARVEST-ICON-1 B: the status icons over the local player's buildings
    /// (collect_status_icons, read-only), every turn. The same object is republished while the
    /// content does not change, so the window re-places the icons only on a change. Never null
    /// after the runner's constructor.
    std::shared_ptr<const std::vector<StatusIconView>> status_icons;
    /// UI-TABS-1 stage B: whether the sim keeps the cities' accounts up to date
    /// (sim::Rules::city_booking: the cost buckets follow building creation and removal and the
    /// statistics decay at each booking only while it is on; docs/sim/economy-booking.md). The
    /// balance pages draw an account row only while this is true (docs/re/ui.md 5.5).
    bool accounts_live = false;
    /// UI-TABS-1 stage B: the scale the original shows statistics 0, 4 and 5 at, `x * num / den`
    /// (AnnoWelt 0x1002dc70: `15 * x / 105`; docs/re/population.md section 8 and 9: the 15
    /// bookings per game minute and the 105 of the 104/105 decay), copied from
    /// SimConfig::upkeep_slices and SimConfig::stat_decay_den.
    std::uint32_t stat_shown_num = 0, stat_shown_den = 0;
};

/// UI-MINIMAP-1 B2: the minimap's owner rule (minimap.hpp MinimapOwnerRule) for a loaded game,
/// read before the world moves into the runner.
/// - viewer: `local` when given (the play runner's local player: a ship start's participant, or
///   `--player` since UI-MINIMAP-1 C); else the first player in slot order
///   whose kind is human (8 or 11) and whose index is not SimConfig::neutral_player - the slot a
///   new single-player game gives the local identity (AnnoFrame 0x10029100, docs/re/annoscene.md
///   section 9); else developer_player().
/// - colours: for a scenario file (`load.fixed_map` false), the game start's assignment
///   (assets::unit::start_colours over `file` with the original's 8 player colours, as the units
///   are drawn); where that names no colour, the HEAD colour the sim keeps
///   (sim::Player::default_colour, HEAD +0x4C); a player with neither has none.
/// The sim's colour_slot is not used: no load path assigns it (UI-MINIMAP-1 B2 report).
[[nodiscard]] MinimapOwnerRule minimap_owner_rule(const PlayLoad& load,
                                                  const std::filesystem::path& file,
                                                  std::optional<sim::PlayerId> local);

/// UI-MINIMAP-1 B2: the territory of `world` (TerritoryView, version 0). Read-only; O(island
/// tiles + cities). UI-STATUS-1 B: each island's land test (TerritoryIsland::land) is taken from
/// `previous` when it holds the same island (index, rectangle, terrain digest), so it is computed
/// once per terrain, not per publication; without it, from the island's height column.
[[nodiscard]] TerritoryView collect_territory_view(const sim::World& world,
                                                   const TerritoryView* previous = nullptr);

/// UI-MINIMAP-1 B2: the figures the minimap may mark: every ship (a figure with a navigation
/// state and no parent, the same set OpeningView::ships lists) as list 1, the original's SCHIFF
/// list (AnnoWelt 0x1001e06f). Soldiers (list 0) are not figures of the rebuild's sim yet. In
/// FigureId order. Read-only; O(figures).
[[nodiscard]] std::vector<MapFigure> collect_map_figures(const sim::World& world);

/// What the window reads for the developer tool's feedback (round ee review, R1): a snapshot and
/// the outcome of one command, read so that the outcome is never newer than the snapshot. The
/// runner records a turn's outcomes before it swaps in the snapshot that follows them, so the
/// live log can be ahead of the snapshot for a moment; an outcome that is, is withheld
/// (`latest->deferred`, state Queued) until its snapshot is published.
struct PlayFeedback {
    std::shared_ptr<const PlaySnapshot> snapshot;
    std::optional<CommandOutcome> latest;
};

/// The turn `DevScriptState::facts_turn` should carry (W4-OPENING-CONTROLS-FLAKE): in an opening
/// session, the opening snapshot's turn, since the facts a script's `wait fact`/`expect fact`
/// steps read are built from that snapshot, refreshed once per drawn frame — not the live
/// snapshot's turn, which a fresh feedback() read can run ahead of under a slow frame cadence.
/// Equal to the live snapshot's turn in a session without an opening (`opening_snapshot` null),
/// where facts is always empty and this has no effect. Null-safe in both arguments.
[[nodiscard]] std::uint64_t facts_turn_for(const PlaySnapshot* opening_snapshot,
                                           const PlaySnapshot* live_snapshot);

/// `snapshot.inspection` when it is still live, nullptr once it is stale (W4-SETTLEMENT-1 W7
/// review, finding 3): the dev tool must be enabled and its tile must be the one currently
/// hovered. `inspection` answers the tile `request_inspect` was last given, one turn behind, and
/// while paused no new turn publishes to retract it (PlayRunner::note_behind just republishes the
/// same answer) — so a hover that has since moved to another building, to empty ground, or a dev
/// tool that was switched off must be caught here, against the *current* hover, not by waiting
/// for the sim to catch up.
[[nodiscard]] const BuildingInspection*
live_inspection(const PlaySnapshot& snapshot, bool dev_enabled, std::optional<sim::TileXY> hover);

/// What one turn did, for the window trace (round ec, B2; docs/app/window-trace.md): the commands
/// it applied, the refusals among them (from the turn's results, round ee: the reasons in command
/// order), and the batch it pushed. `published` is read just before the batch enters the mailbox,
/// so a window that takes the batch never takes it before that time.
struct TurnObservation {
    std::uint64_t turn = 0;
    /// `applied`: the turn's commands, a command without a wire form (refused as Unencodable)
    /// included; `rejected`: the refused ones among them.
    std::size_t applied = 0, rejected = 0;
    /// The refusals' reason names, in the sim's order, joined with ','.
    std::string reasons;
    bool batch = false;
    std::size_t deltas = 0;
    std::chrono::steady_clock::time_point published{};
};

/// The world as saved at a turn boundary (W4-OPENING-1): the save frame's bytes (World::save), the
/// state hash, and the turn and tick it follows. Taken on the sim thread.
struct WorldCapture {
    std::vector<std::uint8_t> save;
    std::uint64_t hash = 0, turn = 0, tick = 0;
};

/// Clock and sleep for the turn schedule. The app uses `steady_clock`. A test
/// supplies a clock that wakes late by a fixed amount, so drift is
/// deterministic. The runner calls these on the sim thread only. `wake`
/// unblocks a test sleep so `stop` can join; the steady clock polls in short
/// slices, so its `wake` does nothing.
class PlaySchedule {
  public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;
    using duration = clock::duration;

    virtual ~PlaySchedule() = default;
    [[nodiscard]] virtual time_point now() const = 0;
    virtual void sleep_until(time_point deadline) = 0;
    virtual void sleep_for(duration d) = 0;
    virtual void wake() {}
};

/// UI-KEYS-1: game milliseconds of one runner step at `percent` speed, or nullopt for a speed the
/// runner does not step at. The runner is turn-based (sim::Timing: a step every mp_step_real_ms =
/// 10 real ms, mp_steps_per_turn per turn), so game ms per real ms = step / 10 = percent / 100
/// exactly. 50/100/200 are the original's multiplayer steps (sim::multiplayer_step_ms, D
/// AnnoFrame.dll 0x1005fe10). 400 is single-player only in the original (F7, refused for a net
/// client: 0x100071b0, 0x10009440); the original's single player steps per frame
/// (sim::single_player_step_ms), so 400 -> 40 here is an ADAPTATION of that to the turn runner
/// (ORCH III decision 2026-11-27), with the same invariant.
[[nodiscard]] std::optional<std::uint32_t> turn_step_ms(std::uint32_t percent);

/// Owns the world on one thread. `run_n` is the same turn the thread runs,
/// on the caller's thread, for tests. Do not call it while `start` is running.
/// `schedule` must outlive the runner; null uses `steady_clock`.
class PlayRunner {
  public:
    explicit PlayRunner(sim::World world, unsigned speed_percent = 100,
                        PlaySchedule* schedule = nullptr);
    ~PlayRunner();
    PlayRunner(const PlayRunner&) = delete;
    PlayRunner& operator=(const PlayRunner&) = delete;

    void start();
    void stop();
    void set_paused(bool paused);
    /// Extra real time at the start of a turn. It is not added to the step.
    /// Sliced so `stop` is noticed; call it before `start`.
    void set_turn_delay(std::chrono::milliseconds delay);
    /// Late turns run back to back up to this many, then the schedule skips
    /// forward and the skipped real time is added to `behind_ms`. Five is
    /// 500 ms at `Timing::mp_turn_real_ms`: one hitch is absorbed, and a sim
    /// that cannot keep the cadence does not queue an unbounded backlog.
    /// Call before `start`. Zero skips as soon as a deadline is missed.
    void set_catchup_turns(unsigned turns);
    [[nodiscard]] unsigned catchup_turns() const { return catchup_turns_; }
    void run_n(std::uint32_t turns);

    [[nodiscard]] std::shared_ptr<const PlaySnapshot> snapshot() const;
    /// Every building batch published since the last call, oldest first. The
    /// window calls it once per frame. It takes all of them, not only the latest
    /// snapshot's, so a turn between two frames is never skipped (W4.3 spec).
    [[nodiscard]] std::vector<std::shared_ptr<const BuildingBatch>> take_building_batches();
    /// The turns whose batches are waiting, oldest first (tests; the window
    /// uses take_building_batches). Taken under the same lock as a snapshot swap.
    [[nodiscard]] std::vector<std::uint64_t> pending_batch_turns() const;
    /// Whether the mailbox holds a batch the window has not taken: a flag set by the push and
    /// cleared by the take (both under the lock), read without the lock (round eb, 1A). The
    /// window's idle check reads it every loop iteration; it copies nothing. A read that races a
    /// push sees the batch at the next iteration, and the listener below wakes that iteration.
    [[nodiscard]] bool batches_waiting() const { return batches_waiting_.load(); }
    /// Called on the sim thread after a turn pushed a batch or ran commands (outside the snapshot
    /// lock), so a window that waits for events wakes for it (the viewer posts an SDL event): a
    /// refusal or an accepted command with no picture change wakes it too (round ee, A3).
    /// Thread-safe: once a call that clears it returns, the previous listener is not called again.
    void set_wake_listener(std::function<void()> listener);
    /// The local participant (default: developer_player()): whose money, goods and first city the
    /// snapshot reports, and, with `opening`, whose cities the OpeningView lists. The city is
    /// looked up again each turn until one exists, so a participant that founds one later gets it.
    /// Call before `start`.
    void set_local_player(sim::PlayerId player) {
        local_player_ = player;
        developer_city_.reset(); // the constructor looked for the default player's
        has_city_.store(false);
    }
    /// The founding preview's question (probe_founding): answered on the sim thread in every view
    /// published from now on, until another one replaces it. Thread-safe.
    void request_found_probe(const sim::FoundCmd& f) {
        const std::lock_guard lock(probe_mu_);
        probe_ = f;
    }
    /// The developer inspection UI's tile (W4-SETTLEMENT-1 W2/W4): answered against the
    /// developer's city in every snapshot published from now on (PlaySnapshot::inspection), until
    /// another tile replaces it or `std::nullopt` turns it off. Thread-safe; same pattern as
    /// request_found_probe, so the window can call it every frame the pointer moves without
    /// querying the World itself.
    void request_inspect(std::optional<sim::TileXY> tile) {
        const std::lock_guard lock(inspect_mu_);
        inspect_tile_ = tile;
    }
    /// UI-GHOST-1: the placement ghost's record, answered in every snapshot published from now on
    /// (PlaySnapshot::build_probe), also while paused, until another replaces it or
    /// `std::nullopt` turns it off. Thread-safe; the same pattern as request_inspect.
    void request_build_probe(std::optional<BuildProbe> probe) {
        const std::lock_guard lock(inspect_mu_);
        build_probe_ = probe;
    }
    /// ROADS-3: the placement ghost's road drag path, answered in every snapshot published from now
    /// on (PlaySnapshot::road_probe), also while paused, until replaced or cleared with nullopt.
    void request_road_probe(std::optional<RoadPathProbe> probe) {
        const std::lock_guard lock(inspect_mu_);
        road_probe_ = std::move(probe);
    }
    /// Ends a capture that was asked for and has not run (the requester is going away).
    /// Thread-safe.
    void cancel_capture() {
        const std::lock_guard lock(capture_mu_);
        capture_ = nullptr;
    }
    /// Publishes an OpeningView with every snapshot and keeps the ship events for it.
    void set_opening(bool on) { opening_ = on; }
    /// Asks for the world to be captured at the end of the next turn, after that turn's
    /// publication (so the capture and the newest snapshot are the same state). `done` runs once on
    /// the sim thread; file I/O belongs to the caller's own thread. A second request before the
    /// first ran replaces nothing and returns false. Thread-safe.
    bool request_capture(std::function<void(WorldCapture)> done);
    /// The sim's order-id counter at the start of the next turn (sim thread; tests, run_n).
    [[nodiscard]] std::uint64_t next_order_id() const { return world_.ids().next_order_id; }
    /// Test seam: called on the sim thread inside every turn, after its commands are chosen and
    /// before the turn runs, with the turn index (a turn held in flight). Call before `start`.
    void set_turn_gate(std::function<void(std::uint64_t turn)> gate);
    /// Called on the sim thread at the end of every turn, after its publication (and after the
    /// batch listener), with what the turn did. Opt-in (the window trace). Call before `start`.
    void set_turn_observer(std::function<void(const TurnObservation&)> observer);
    /// The outcomes of the commands run since sequence number `from` (round ee, A1; OutcomeLog):
    /// in the order they ran, with the range a slow reader lost, never a silent gap. Any thread.
    [[nodiscard]] OutcomeRead outcomes_since(std::uint64_t from) const {
        return outcomes_.since(from);
    }
    /// What is known about one command now (Queued, Applied, Refused, Lost or Unknown): the raw
    /// log, which can be ahead of snapshot() by a publication. A reader that also shows the
    /// snapshot's money or goods must use feedback() (round ee review, R1).
    [[nodiscard]] CommandOutcome outcome(CommandId id) const { return outcomes_.find(id); }
    /// What is known about `id` as of `snapshot`'s publication (OutcomeLog::find_published): an
    /// outcome recorded after the snapshot's turn is answered Queued with `deferred`, so a view
    /// and the outcome beside it are from the same publication (W4-OPENING-1).
    [[nodiscard]] CommandOutcome outcome_at(CommandId id, const PlaySnapshot& snapshot) const {
        return outcomes_.find_published(id, snapshot.outcomes_next);
    }
    /// The snapshot and, when `receipt` is given, that command's outcome as of the snapshot's
    /// publication (PlayFeedback; OutcomeLog::find_published). The snapshot is read first and the
    /// outcome is answered against its watermark, so money and goods are never from before an
    /// outcome shown next to them. Never waits for the sim: two short locks, one after the other.
    [[nodiscard]] PlayFeedback feedback(std::optional<CommandId> receipt) const;
    /// Test seam: called on the sim thread inside every publication, after the turn's outcomes
    /// are recorded and before its snapshot is swapped in, with the turn index (the interleaving
    /// a concurrent reader can see). No lock is held. Call before `start`.
    void set_publish_gate(std::function<void(std::uint64_t turn)> gate);
    /// The outcome store's bounds (OutcomeLog: outcomes, per-record entries). Call before `start`.
    void set_outcome_bounds(std::size_t outcomes, std::size_t records);
    /// How many batches the mailbox holds before it collapses them into a resync
    /// (BuildingMailbox). Default: one second of turns. Call before `start`.
    void set_batch_bound(std::size_t bound);
    /// Commands for the turns (commands_file.hpp), ordered by turn and, within a turn, in file
    /// order (a stable sort: a line for an earlier turn after a later one still runs). A turn
    /// applies the ones that name it, before its steps (`Turn::commands`). Their receipts are
    /// {Script, 1..} in that order. Call once, before `start` and `run_n` (the receipts restart).
    void set_script(std::vector<ScriptedCommand> script);
    /// Queues a command for the next turn to start (the window's input; W4.4 B2) and returns its
    /// receipt {Window, n}, n from 1 in submission order. Thread-safe and never waits for a turn.
    /// The turn applies its scripted commands first, then the queued ones in push order
    /// (CommandQueue). The receipt is not acceptance: outcome() says what the sim did.
    CommandId post_command(sim::Command command);
    /// Every command given to the sim so far (with a wire form: one without is refused as
    /// Unencodable and never applied), with the turn that applied it, oldest first: the record a
    /// determinism test replays. Thread-safe.
    [[nodiscard]] std::vector<ScriptedCommand> applied_commands() const;
    /// The world's state hash after each turn that ran commands and after every kHashEvery-th
    /// turn, recorded when `on` (round ee, E3: a session's checkpoints, replayed headlessly with
    /// --replay-hashes). Call before `start`. 16 bytes an entry, unbounded: a test session's
    /// length. kHashEvery: a full hash per turn was slower than the turn cadence on lan5 in a
    /// debug build; a test tool's choice, not a rule (it could be an option).
    void set_record_hashes(bool on) { record_hashes_ = on; }
    static constexpr std::uint64_t kHashEvery = 25;
    struct TurnHash {
        std::uint64_t turn = 0, hash = 0;
    };
    /// The recorded hashes, oldest first. Thread-safe.
    [[nodiscard]] std::vector<TurnHash> recorded_hashes() const;
    [[nodiscard]] std::string title_line() const;
    /// title_line of one snapshot: the tick, game time, turn time, lag, pause, the money when
    /// `money`, and the last refusal with the command it belongs to.
    [[nodiscard]] static std::string title_of(const PlaySnapshot& snapshot, bool money);
    /// Observable after synchronous `run_n`; the window never receives World.
    [[nodiscard]] std::uint64_t state_hash() const { return world_.state_hash(); }
    [[nodiscard]] std::uint32_t step_ms() const { return step_ms_; }
    /// UI-KEYS-1: the world's host player (SimConfig::host_player, immutable).
    [[nodiscard]] sim::PlayerId host_player() const { return host_player_; }
    /// UI-KEYS-1: who a speed key's SetSpeedCmd is issued as. In single player the window is its
    /// own host (the original's single-player SetSpeed 0x1005a440 needs no host), so the host
    /// player, whatever the local seat; in a multi-seat session only a local seat that is the
    /// host speaks as the host, any other issues as itself and set_speed refuses it (NotHost),
    /// as the original's multiplayer SetSpeed acts only on the host (0x1005e230, +0x3c8 =
    /// server; docs/re/tick-loop.md). Reads set_local_player's seat: call it before start.
    [[nodiscard]] sim::PlayerId speed_issuer() const {
        return (!multiplayer_ || local_player_ == host_player_) ? host_player_ : local_player_;
    }

  private:
    void loop();
    void one_turn();
    void publish(std::uint64_t turn_us, std::uint64_t turn, std::size_t applied,
                 std::shared_ptr<const BuildingBatch> batch, std::vector<CommandOutcome> ran);
    /// Seconds of turns the title keeps showing the last refusal (30 turns).
    static constexpr std::uint64_t kRejectionShowTurns = 30;
    void publish_snapshot(std::shared_ptr<const PlaySnapshot> snapshot);
    /// The buildings after the turn, diffed against the last list (sim thread).
    [[nodiscard]] std::shared_ptr<const BuildingBatch> building_batch(std::uint64_t turn);
    void note_behind();
    /// While paused no turn publishes; a newly requested inspection tile is answered by
    /// republishing the current snapshot with a fresh `inspection`/`inspected` (a read of the
    /// World on this thread; no state, stream, hash or draw change). W4-NEXT-2 K.
    void answer_inspection_while_paused();
    /// Looks up the local player's first city when there is none yet (each turn does this).
    void find_local_city();
    /// Fills `inspection` and `inspected` from the requested tile.
    void fill_inspection(PlaySnapshot& next);
    /// Fills `build_probe` from the requested probe (UI-GHOST-1).
    void fill_build_probe(PlaySnapshot& next);
    /// Fills `road_probe` from the requested path probe (ROADS-3).
    void fill_road_probe(PlaySnapshot& next);
    [[nodiscard]] std::uint64_t p95_locked() const;

    sim::World world_;
    std::string simulation_error_; // sim-thread owned; published as the refusal reason
    std::unique_ptr<PlaySchedule> owned_schedule_;
    PlaySchedule* schedule_ = nullptr;
    std::uint32_t step_ms_ = 10;
    /// UI-KEYS-1: the world's speed_percent the runner last saw; a SetSpeedCmd that changes it
    /// sets step_ms_ for the next turn (turn_step_ms), so `--speed` holds until one does.
    std::uint32_t seen_speed_percent_ = 0;
    sim::PlayerId host_player_{};
    bool multiplayer_ = false;
    std::uint32_t steps_ = 10;
    unsigned catchup_turns_ = 5;
    std::uint64_t index_ = 0;
    std::uint64_t behind_ms_ = 0;
    std::atomic<bool> stop_{false};
    std::atomic<bool> paused_{false};
    std::atomic<std::uint32_t> delay_ms_{0};
    std::thread thread_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, std::uint64_t>> costs_;
    /// UI-WINDOW-1 fix round R2 (measurement): every turn's cost and the time publish() spent
    /// building its snapshot, in microseconds, kept only with ANNO_PUBLISH_STATS set; stop()
    /// prints their median, p95 and max to stderr. Off: nothing is kept or printed.
    bool publish_stats_ = false;
    std::vector<std::uint64_t> stat_turn_us_, stat_snapshot_us_;
    /// The tracked and cleared tile objects of `world_` after the turn (sim thread).
    [[nodiscard]] std::vector<SceneTileObject> collect_tile_objects() const;
    [[nodiscard]] bool is_building_id(std::uint32_t id) const;
    [[nodiscard]] bool is_tracked_id(std::uint32_t id) const;

    // The sorted building and tile-object lists the picture has been given so far (sim thread).
    std::vector<SceneBuilding> buildings_;
    std::vector<SceneTileObject> tile_objects_;
    std::vector<std::uint8_t> building_type_mask_;
    std::vector<std::uint8_t> tracked_type_mask_;
    /// Per island, per tile: 1 iff tick 0 held a non-building, non-tracked object anchor there.
    std::vector<std::vector<std::uint8_t>> initial_scenery_;
    // The lock only protects the shared_ptr copy/swap and the mailbox's push or
    // take. Snapshot allocation, the building diff and all simulation work
    // happen before it is acquired.
    mutable std::mutex snap_mu_;
    std::shared_ptr<const PlaySnapshot> snap_;
    BuildingMailbox mailbox_{1};
    /// The mailbox is not empty (written under snap_mu_ by push and take).
    std::atomic<bool> batches_waiting_{false};
    std::mutex listener_mu_;
    std::function<void()> listener_;
    std::function<void(std::uint64_t)> turn_gate_;
    std::function<void(std::uint64_t)> publish_gate_;
    std::function<void(const TurnObservation&)> turn_observer_;
    // The script and its cursor (sim thread only once started).
    std::vector<ScriptedCommand> script_;
    std::size_t script_next_ = 0;
    // The window's commands (thread-safe by itself), the record of what was applied
    // (written by the sim thread under snap_mu_; read by applied_commands under it), and the
    // commands' outcomes (thread-safe by itself).
    CommandQueue queue_;
    std::vector<ScriptedCommand> applied_;
    bool record_hashes_ = false;
    std::vector<TurnHash> hashes_;
    OutcomeLog outcomes_;
    std::mutex capture_mu_;
    std::mutex probe_mu_;
    std::optional<sim::FoundCmd> probe_;
    std::mutex inspect_mu_;
    std::optional<sim::TileXY> inspect_tile_;
    std::optional<BuildProbe> build_probe_;   // under inspect_mu_
    std::optional<RoadPathProbe> road_probe_; // under inspect_mu_
    std::function<void(WorldCapture)> capture_;
    // The developer player's city (sim thread, found at construction) and the last refusal
    // shown in the title.
    std::optional<sim::CityRef> developer_city_;
    /// UI-MINIMAP-1 B2: the territory last published (sim thread only).
    std::shared_ptr<const TerritoryView> territory_;
    /// The last published status icons (PlaySnapshot::status_icons); replaced only on a change.
    std::shared_ptr<const std::vector<StatusIconView>> status_icons_;
    void refresh_status_icons();
    /// Takes the world's territory and keeps territory_ when its content is the same (sim thread).
    void refresh_territory();
    /// developer_city_ has a value: the window's title reads it, developer_city_ is the sim
    /// thread's.
    std::atomic<bool> has_city_{false};
    sim::PlayerId local_player_ = developer_player();
    bool opening_ = false;
    ShipEventLog ship_events_;
    std::string last_reason_;
    std::uint64_t last_reason_turn_ = 0;
};

} // namespace anno::app
