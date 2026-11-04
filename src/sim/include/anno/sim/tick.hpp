// The read-only views and contexts of one advance, and the effect buffer (spec:
// docs/sim/state.md §4.11, §5.3, stage S7).
//
// An island tick gets a TickContext (read-only views of everything outside its
// island: players, the island table's static facts, the type registry, the
// configuration, the settings, the draw source and the seams) and an Effects
// buffer for the consequences that leave the island. The world merges the buffer
// right after the tick, in island order, so figure ids, registry slots and event
// sequence numbers equal those of the original's synchronous order (C1). A
// serial-phase seam (figure behaviour, event handler, player pass, AI) gets a
// SerialWorld: const queries over the whole state plus its own Effects buffer,
// merged immediately after the seam returns and before the next object is visited
// (C2). Nothing here is state; no seam ever receives a mutable World.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include <anno/sim/city.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/draw.hpp>
#include <anno/sim/events.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/island.hpp>
#include <anno/sim/player.hpp>
#include <anno/sim/seams.hpp>
#include <anno/sim/settings.hpp>
#include <anno/sim/types.hpp>

namespace anno::sim {

struct TransportStats; // transport.hpp (T29, measurement only; not sim state)

// Opaque declarations of the typed population faults (house_transition.hpp, resident.hpp,
// house_kernel.hpp), so the effect buffer can carry them without including those modules.
enum class HouseFault : std::uint8_t;
enum class ResidentFault : std::uint8_t;
enum class NotEvaluatedReason : std::uint8_t;

/// MAP-POP-LIVE-1 L4 (docs/sim/population-live.md section 19): a typed fault of the live House
/// update (P-1/P-3: fail loudly). `house` names the House operation that could not be performed
/// (HouseFault::NotEvaluated carries the kernel's `reason`); `resident` is a ResidentFault of a
/// choice, bill or add. Exactly one of the two is non-zero. The world merges it in issue order;
/// the first one stops the world (World::fault()).
struct FaultRecord {
    HouseFault house{};
    NotEvaluatedReason reason{};
    ResidentFault resident{};
    BuildingId building;
    FigureId figure;
    GoodsId goods = 0;
};

/// The serial resources the live House update and the stage pass write DIRECTLY (not through
/// Effects, because the original order interleaves them with the other building ticks and their
/// draws): the player table (the stage mask OR, the defeat flag of a removal) and the figure
/// registry (a removed or shrunk house ends its walkers). The island phase of World::advance is
/// serial today (islands in table order, each merged before the next); a parallel island phase
/// (stage S16) must move the House path into the merge phase before it can use more than one
/// worker. Null for a context that does not belong to an advance: the House update then reports a
/// fault instead of running half-way.
struct SerialResources {
    PlayerTable* players = nullptr;
    FigureRegistry* figures = nullptr;
};

/// The players as a tick sees them: read-only, and deliberately without a money
/// accessor (money changes only through Effects::money, merged by the world, so
/// that no island tick can read a balance that a serial phase is about to
/// change). A default-constructed view is empty: it holds no player. Every
/// query is total: an invalid id, a slot out of range or an empty slot is a
/// player that is not there, never an error.
class PlayersView {
  public:
    PlayersView() = default;
    explicit PlayersView(const PlayerTable& table) : table_(&table) {}

    [[nodiscard]] bool present(PlayerId id) const { return find(id) != nullptr; }
    /// The player's Volk (D: +0x7a); 0 for a player that is not there.
    [[nodiscard]] std::uint32_t volk(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr ? p->volk : 0;
    }
    /// Whether research `research_id` is done (bit 1 of its state byte); false
    /// for a player that is not there and for an id beyond the table.
    [[nodiscard]] bool researched(PlayerId id, std::uint32_t research_id) const {
        const Player* p = find(id);
        return p != nullptr && p->research.researched(research_id);
    }
    /// The player's stage mask (Player::flags, D +0x90); 0 for a player that is not there.
    [[nodiscard]] std::uint32_t flags(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr ? p->flags : 0;
    }
    /// O-9: whether the player's build cost is waived; false when not there.
    [[nodiscard]] bool free_build(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr && p->free_build;
    }
    /// The player's state; Defeated for a player that is not there (an empty
    /// slot takes no part in the game, like a defeated one). Ask present()
    /// first where the difference matters.
    [[nodiscard]] PlayerState state(PlayerId id) const {
        const Player* p = find(id);
        return p != nullptr ? p->state : PlayerState::Defeated;
    }

  private:
    [[nodiscard]] const Player* find(PlayerId id) const {
        return table_ != nullptr ? table_->find(id) : nullptr;
    }
    const PlayerTable* table_ = nullptr;
};

/// The island table as an island tick sees it: STATIC facts only, nothing here is
/// written by any island tick (the rectangle and the climate), so a parallel
/// island phase (stage S16) cannot race on it. Other islands' mutable state (their
/// cities, buildings, sweeps) is not readable from an island tick. A
/// default-constructed view holds no island. Every query is total.
class IslandsView {
  public:
    IslandsView() = default;
    explicit IslandsView(const std::vector<std::optional<Island>>& islands) : islands_(&islands) {}

    /// The length of the island table (null slots included).
    [[nodiscard]] std::uint32_t count() const {
        return islands_ != nullptr ? static_cast<std::uint32_t>(islands_->size()) : 0;
    }
    [[nodiscard]] bool present(IslandId id) const { return find(id) != nullptr; }
    /// The island's rectangle in world tiles; all zero for an island that is not
    /// there.
    [[nodiscard]] TileRect rect(IslandId id) const {
        const Island* is = find(id);
        return is != nullptr ? is->rect() : TileRect{};
    }
    /// The island's climate; 0 for an island that is not there.
    [[nodiscard]] std::uint32_t climate(IslandId id) const {
        const Island* is = find(id);
        return is != nullptr ? is->climate : 0;
    }
    /// The island whose rectangle holds the world tile (x, y). The rectangles are
    /// disjoint, so there is at most one; the lookup scans the table (a derived
    /// grid over the rectangles is an optimisation, D: world +0x130).
    [[nodiscard]] std::optional<IslandId> island_at(std::int32_t x, std::int32_t y) const {
        if (islands_ == nullptr) {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < islands_->size(); ++i) {
            const std::optional<Island>& is = (*islands_)[i];
            if (is.has_value() && is->tiles.contains(x, y)) {
                return IslandId{static_cast<std::uint32_t>(i)};
            }
        }
        return std::nullopt;
    }

  private:
    [[nodiscard]] const Island* find(IslandId id) const {
        if (islands_ == nullptr || !id.valid() || id.v >= islands_->size() ||
            !(*islands_)[id.v].has_value()) {
            return nullptr;
        }
        return &*(*islands_)[id.v];
    }
    const std::vector<std::optional<Island>>* islands_ = nullptr;
};

/// What every call of an advance can read. `tick` is the 64-bit world tick, `t`
/// its low 32 bits: every gate and stamp uses `t` (D: Advance 0x100ae040 stores
/// (u32)(ms >> 1) at +0x10c). `draw` is the only door to the PRNG: the global
/// stream in M3, for the serial phases and the island ticks alike.
struct TickContext {
    std::uint64_t tick = 0;
    std::uint32_t t = 0;
    const SimConfig& cfg;
    const Rules& rules;
    const WorldSettings& settings;
    const TypeRegistry& types;
    PlayersView players;
    IslandsView islands;
    DrawSource& draw;
    const Seams& seams;
    /// Diagnostic sink for the transport search (task T29, measurement only; not sim
    /// state, not hashed, not drawn). Default null: the sim and every caller leave it
    /// so and the test routes it from World::set_transport_stats, which Scheduler::
    /// advance copies into every context it builds. Single-threaded today; must stay
    /// null if island ticks ever run in parallel (stage S16): a concurrent write
    /// across islands would race on the std::map and the counters.
    TransportStats* transport_stats = nullptr;
    /// MAP-POP-LIVE-1 L4: see SerialResources. Set by Scheduler::advance; default null.
    const SerialResources* serial = nullptr;
};

/// A figure a building asks for. The building's own counter was already raised in
/// its tick (docs/sim/state.md §7.2); the merge creates the figure.
struct SpawnRequest {
    BuildingId home;
    CityRef city;
    std::uint32_t home_counter = 0;
    std::uint32_t job = 0;
    std::uint32_t figure_kind = 0;
    PlayerId owner;
    FixedPos pos;
    /// MAP-ECON-1 E2: the new figure's cargo goods (a farm worker carries its home's input 0, D
    /// 0x1006ee06) and whether it gets the harvest state (docs/sim/forestry.md 2).
    GoodsId cargo_goods = 0;
    bool harvest = false;
    /// MAP-POP-LIVE-1 L2 (docs/sim/population-live.md section 3): the new figure is a house walker
    /// (job 0x35). The merge gives it its ResidentState (empty cargo slots) and writes its id into
    /// the home House's walker slot `home_counter`; a request whose slot is not an empty slot of a
    /// House creates no figure.
    bool resident = false;
};

/// A counter of figures out goes down by one (if the building still exists and the
/// counter is above 0). With `figure` valid the release goes through the figure, as
/// the original's unit slot +0x110 = 0x1003b6e0 does: the figure's own home and counter
/// are released and its home becomes none, and a figure that is gone or has no home
/// any more releases nothing (`home` / `home_counter` are then not read). That is the
/// release a behaviour seam asks for; end_figure's own release names the building.
struct ReleaseCounter {
    BuildingId home;
    std::uint32_t home_counter = 0;
    FigureId figure;
    /// MAP-POP-LIVE-1 L2: the figure that held the slot (set by the end path). A House keeps its
    /// walkers as ids: the slot `home_counter` is cleared only while it still names `holder`, so an
    /// old job never clears a newer owner's slot. Invalid for the counters of the other payloads.
    FigureId holder;
};

/// A change of a player's money (i64; saturating at the ends of the type).
struct MoneyDelta {
    PlayerId player;
    std::int64_t delta = 0;
};

/// A timed event to queue: due at the tick of the merge plus `delay`.
struct EnqueueEvent {
    std::uint64_t delay = 0;
    TimedEvent e;
};

// Goods traffic of the serial figure phase (carts and carriers; docs/re/
// production.md §5), applied at the merge in vector order:

/// A signed change of a city's stock in 8.8 tons: a positive delta is CityStock::add
/// (clipped at the capacity), a negative one CityStock::remove (clipped at zero). A
/// magnitude beyond what a u32 holds is cut to 0xFFFFFFFF.
struct GoodsMove {
    CityRef city;
    GoodsId goods = 0;
    std::int64_t delta_88 = 0;
    /// Diagnostic only (MAP-POP-LIVE-1 L5): the move is a resident purchase (counted in the
    /// per-goods ledger of ResidentStats at the merge). Never read by the sim.
    bool resident = false;
};

/// A cart takes up to `amount_88` from the building's output store `out_index`; the
/// amount actually taken is ADDED to the figure's cargo. The goods are the type's
/// output goods of that store; the cart must be empty or carry the same goods,
/// else nothing is taken. A building that is gone, a store that does not exist or
/// an empty one take 0 and are not an error.
struct Reserve {
    FigureId figure;
    BuildingId building;
    std::uint32_t out_index = 0;
    std::uint32_t amount_88 = 0;
};

/// Moves the figure's whole cargo into the city's stock (CityStock::add); what does
/// not fit stays in the cargo. A figure or a city that is gone does nothing.
struct Deliver {
    FigureId figure;
    CityRef city;
};

/// A change of one of a building's goods stores, which the transport body computed
/// from the state it read (T24: the reservations and the load of a cart, docs/re/
/// transport.md §1.4): `delta_88` is added to the store `index` of `store` and the
/// result clamped to 0 .. 0xFFFFFFFF. A building that is gone or a store it does not
/// have changes nothing.
struct StoreDelta {
    enum class Store : std::uint8_t { Output, Reserved, Input };
    BuildingId building;
    Store store = Store::Output;
    std::uint32_t index = 0;
    std::int64_t delta_88 = 0;
};

/// MAP-POP-LIVE-1 L2 (docs/sim/population-live.md section 2 "Billing"): `amount` is ADDED to the
/// city's billed counter of `goods` (the REQUESTED amount, saturating at u32). A city that is gone
/// or a goods beyond the table changes nothing.
struct BilledDelta {
    CityRef city;
    GoodsId goods = 0;
    std::uint32_t amount = 0;
};

/// MAP-POP-LIVE-1 L2 (section 2 "House delivery"): a resident hands `amount_88` of `goods` to its
/// home house (house_deliver: stock += amount * 16 and the IdSec replay). Applied only while the
/// building still exists, is a House, and `figure` still holds one of its walker slots (so it is
/// merged BEFORE the releases of the same buffer, which the figure's end pushes); a refused
/// delivery (stock overflow) changes nothing and is counted.
struct HouseDelivery {
    BuildingId home;
    FigureId figure;
    GoodsId goods = 0;
    std::uint32_t amount_88 = 0;
};

enum class SimEventKind : std::uint8_t {
    StatusMessage,
    Notice,
    BlockedChanged,
    FigureSpawned,
    Event15a,
    TimerExpired,
    CommandRejected,
    // MAP-NAV-2 (docs/sim/ship-navigation.md 12.1): {player = owner, figure = the ship, order_id}.
    ShipArrived,
    ShipRouteFailed, ///< code = NavFailReason (0 Unreachable, 1 BudgetExceeded, 2 OutsideWindow, 3
                     ///< ShipBlocked)
    ShipStopped
};

/// UI-facing output of the simulation; not state, never hashed. Which fields a
/// kind uses: StatusMessage {player = the city owner, building, code = Status};
/// Notice {player, building, code = the message id}; BlockedChanged {player,
/// building, code = 1 blocked / 0 free, x, y}; FigureSpawned {player, building =
/// the home, figure, code = the figure kind, x, y = the tile}; Event15a {code =
/// 0x15a, x, y = the centre tile of the island the 7.5 minute event hit; player,
/// building and figure stay invalid}; TimerExpired {code = the timer index}.
struct SimEvent {
    SimEventKind kind = SimEventKind::StatusMessage;
    std::uint64_t tick = 0;
    PlayerId player;
    BuildingId building;
    FigureId figure;
    std::uint32_t code = 0;
    std::int32_t x = 0, y = 0;
    /// Ship events: the identity of the order the event is about (never reused).
    std::uint64_t order_id = 0;
};

/// Filled by an island tick or a serial-phase seam, merged by the world (§5.3). Merge
/// order inside one buffer: spawns, house_deliveries, releases, money, goods_moves, billed_deltas,
/// reserves, deliveries, store_deltas, tile_edits, events, ui; each vector in order.
/// A figure moved by its carrier (MAP-NAV-2): a ship's crew (children, grandchildren) is written
/// from the ship's position by the world after the ship's visit, never by the ship's own call.
struct FigureMove {
    FigureId figure;
    FixedPos pos;
};

/// MAP-ECON-1 E3 (docs/sim/forestry.md 3.2): one tile write of the harvest body, applied at the
/// merge in issue order after the store deltas. Claim sets bit 29 (kTileClaim; under
/// Rules::forestry_claim_hygiene also Island::claims[tile] = actor), Unclaim clears it (and the
/// entry), Cut turns the tile object `id` into `id - 1` with phase `& 0x80` when the type `id - 1`
/// is ROHSTWACHS (AnnoWelt 0x10087340; the merge re-applies that predicate). Release
/// (MAP-ECON-1-CLOSE-1, the shared end path, docs/sim/forestry.md 3.5): under
/// Rules::forestry_claim_hygiene only, clears the bit and the entry when `Island::claims[tile]` is
/// still `actor` (never another worker's claim); under the faithful rules it writes nothing.
struct TileEdit {
    enum class Kind : std::uint8_t { Claim, Unclaim, Cut, Release };
    Kind kind = Kind::Claim;
    IslandId island;
    TileXY tile;
    FigureId actor;
};

struct Effects {
    std::vector<SpawnRequest> spawns;
    std::vector<ReleaseCounter> releases;
    std::vector<MoneyDelta> money;
    std::vector<GoodsMove> goods_moves;
    /// MAP-POP-LIVE-1 L2: applied right after the goods moves.
    std::vector<BilledDelta> billed_deltas;
    std::vector<Reserve> reserves;
    std::vector<Deliver> deliveries;
    std::vector<StoreDelta> store_deltas;
    std::vector<EnqueueEvent> events;
    std::vector<SimEvent> ui;
    /// Crew positions mirrored from a navigating ship (applied in order after the spawns).
    std::vector<FigureMove> figure_moves;
    /// Telemetry: the draws the call made, per site (the world fills it).
    std::vector<DrawRecord> draws;
    /// MAP-ECON-1 E3: the harvest body's tile writes, in issue order.
    std::vector<TileEdit> tile_edits;
    /// MAP-POP-LIVE-1 L2: the residents' deliveries to their houses, applied right after the spawns
    /// and before the releases.
    std::vector<HouseDelivery> house_deliveries;
    /// MAP-POP-LIVE-1 L4: typed faults of the live population update, merged in order; the first
    /// one stops the world.
    std::vector<FaultRecord> faults;
    /// Diagnostic (TransportStats::forestry.raw_lost_88, never state): the cargo of harvest workers
    /// that the shared end path ended before their delivery (MAP-ECON-1-CLOSE-1).
    std::uint64_t harvest_lost_88 = 0;
};

/// The world as a serial-phase seam sees it (event handlers, figure behaviour,
/// player passes, the AI): const queries over the whole state plus an Effects
/// buffer. No write other than to the object being ticked and the buffer is
/// possible; the world merges the buffer right after the seam returns (C2).
class SerialWorld {
  public:
    SerialWorld(const std::vector<std::optional<Island>>& islands, const ListLayout& layout,
                const FigureRegistry& figures, const PlayerTable& players, Effects& effects)
        : fx(effects), islands_(islands), layout_(layout), figures_(figures), players_(players) {}

    /// The island in slot `id`, or nullptr.
    [[nodiscard]] const Island* island(IslandId id) const {
        if (!id.valid() || id.v >= islands_.size() || !islands_[id.v].has_value()) {
            return nullptr;
        }
        return &*islands_[id.v];
    }
    /// The city, or nullptr.
    [[nodiscard]] const City* city(CityRef ref) const {
        const Island* is = island(ref.island);
        return is != nullptr ? is->city(ref.city) : nullptr;
    }
    /// The building with this id (the island named by the id), or nullptr: a removed
    /// building is gone, never an error.
    [[nodiscard]] const Building* find_building(BuildingId id) const {
        const Island* is = island(island_of(id));
        return is != nullptr ? is->find_building(id) : nullptr;
    }
    /// The building of `city` on the world tile (x, y) in the masked lists
    /// (Island::building_at), or nullptr.
    [[nodiscard]] const Building* building_at(CityRef city, std::int32_t x, std::int32_t y,
                                              std::uint32_t list_mask = ~0U) const {
        const Island* is = island(city.island);
        return is != nullptr ? is->building_at(layout_, city.city, x, y, list_mask) : nullptr;
    }
    /// Clears `out`, then appends the ids of `city`'s buildings that overlap `rect`
    /// (Island::rect_query, canonical order).
    void rect_query(CityRef city, const TileRect& rect, std::uint32_t list_mask,
                    std::vector<BuildingId>& out) const {
        const Island* is = island(city.island);
        if (is == nullptr) {
            out.clear();
            return;
        }
        is->rect_query(layout_, city.city, rect, list_mask, out);
    }
    [[nodiscard]] const Figure* find_figure(FigureId id) const {
        return anno::sim::find_figure(figures_, id);
    }
    /// The figure registry (const): the transport sums a workshop's carriers' loads.
    [[nodiscard]] const FigureRegistry& figures() const { return figures_; }
    /// The list layout of the world's configuration.
    [[nodiscard]] const ListLayout& layout() const { return layout_; }
    /// Ends the figure a behaviour seam is ticking (end_figure on its fields: an active
    /// harvest job is finished first, the release goes into `fx`, home none, the stamp
    /// u16(t)); the registry appends it to
    /// the expiry list after the seam returns (sync_figure_lists). Returns false when the
    /// figure had ended already.
    bool end_figure(Figure& figure, std::uint32_t t) {
        return anno::sim::end_figure(figure, t, fx);
    }
    [[nodiscard]] const PlayerTable& players() const { return players_; }

    /// The buffer the seam writes its consequences to.
    Effects& fx;

  private:
    const std::vector<std::optional<Island>>& islands_;
    const ListLayout& layout_;
    const FigureRegistry& figures_;
    const PlayerTable& players_;
};

/// What the last advance produced for the host: not state, not hashed, cleared at
/// the start of every advance.
struct SimOutput {
    /// The UI events of the last advance, in merge order.
    std::vector<SimEvent> events;
    /// The draws of the last advance per site, in site order (sites without a draw
    /// are left out).
    std::vector<DrawRecord> draws;
    /// The PRNG cursor before the first and after the last draw of the advance.
    std::uint32_t rng_cursor_before = 0, rng_cursor_after = 0;
    /// The timed events the advance dispatched (both passes of Rules::
    /// events_flush_after_figures together).
    std::uint64_t events_dispatched = 0;
    /// True when a dispatch pass of the advance stopped at Rules::events_max_dispatch
    /// with a due event left in the queue (F24, pop_due): a game never reaches the cap,
    /// so World::check_invariants reports it until the next advance.
    bool events_capped = false;
};

} // namespace anno::sim
