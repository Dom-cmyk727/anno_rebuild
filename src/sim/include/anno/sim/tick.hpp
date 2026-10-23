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

enum class SimEventKind : std::uint8_t {
    StatusMessage,
    Notice,
    BlockedChanged,
    FigureSpawned,
    Event15a,
    TimerExpired,
    CommandRejected
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
};

/// Filled by an island tick or a serial-phase seam, merged by the world (§5.3). Merge
/// order inside one buffer: spawns, releases, money, goods_moves, reserves,
/// deliveries, events, ui; each vector in order.
struct Effects {
    std::vector<SpawnRequest> spawns;
    std::vector<ReleaseCounter> releases;
    std::vector<MoneyDelta> money;
    std::vector<GoodsMove> goods_moves;
    std::vector<Reserve> reserves;
    std::vector<Deliver> deliveries;
    std::vector<EnqueueEvent> events;
    std::vector<SimEvent> ui;
    /// Telemetry: the draws the call made, per site (the world fills it).
    std::vector<DrawRecord> draws;
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
    /// Ends the figure a behaviour seam is ticking (end_figure on its fields: the
    /// release goes into `fx`, home none, the stamp u16(t)); the registry appends it to
    /// the expiry list after the seam returns (sync_figure_lists). Returns false when the
    /// figure had ended already.
    bool end_figure(Figure& figure, std::uint32_t t) { return anno::sim::end_figure(figure, t, fx); }
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
