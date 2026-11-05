// What the window may know about the ship-start opening (W4-OPENING-1; docs/app/ship-opening.md):
// an immutable copy of the facts a player's controls need, taken by the sim thread after a turn.
// The window never sees a World, a Figure or a registry slot: ships, the local participant's cities
// and the events of the ships' orders are values with the sim's stable identities (FigureId,
// BuildingId, CityRef). A holder of an old view cannot make it stale by holding it.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <anno/sim/command.hpp>
#include <anno/sim/figure.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/reject.hpp>
#include <anno/sim/types.hpp>
#include <anno/sim/world.hpp>

namespace anno::app {

struct ShipHoldView {
    sim::GoodsId goods = 0;
    /// Raw units of 1/256 ton (sim::ShipHold).
    std::uint64_t amount_88 = 0;
    friend bool operator==(const ShipHoldView&, const ShipHoldView&) = default;
};

/// A ship as published after a turn. Embarked scouts and donkeys are not ships and are not listed.
struct ShipView {
    sim::FigureId id;
    sim::PlayerId owner;
    std::uint32_t kind = 0;
    /// Q24.8 tiles; the pixels are drawn from this, never from a tile.
    sim::FixedPos pos;
    /// 0..15 (Figure::dir16): 0 is -x, 4 is +y, 8 is +x, 12 is -y (docs/re/ship-navigation.md 4).
    std::uint8_t dir16 = 0;
    std::vector<ShipHoldView> holds;
    /// The accepted order the ship is carrying out (NavMotion::order), by its never-reused id.
    std::optional<std::uint64_t> order_id;
    /// The order has a plan (the original's phase 1). false: accepted, not planned yet.
    bool routed = false;
    std::int32_t target_x_q8 = 0, target_y_q8 = 0;
};

enum class ShipEventKind : std::uint8_t { Arrived, RouteFailed, Stopped };

/// A ship's order ended (sim::SimEvent kinds ShipArrived, ShipRouteFailed, ShipStopped).
struct ShipEventView {
    ShipEventKind kind = ShipEventKind::Arrived;
    std::uint64_t tick = 0;
    sim::FigureId ship;
    sim::PlayerId player;
    std::uint64_t order_id = 0;
    /// RouteFailed: sim::NavFailReason (0 Unreachable, 1 BudgetExceeded, 2 OutsideWindow,
    /// 3 ShipBlocked).
    std::uint32_t reason = 0;
    /// The sequence number of this event in the runner's record (from 1, no gap).
    std::uint64_t seq = 0;
    friend bool operator==(const ShipEventView&, const ShipEventView&) = default;
};

/// The warehouse of a city (the first building whose type is an eligible warehouse by the world's
/// cargo rules).
struct WarehouseView {
    sim::BuildingId id;
    sim::TypeId type;
    /// North-west tile and the rotated footprint.
    sim::TileXY tile;
    std::uint32_t w = 0, h = 0;
    std::uint8_t rotation = 0;
    friend bool operator==(const WarehouseView&, const WarehouseView&) = default;
};

/// A city of the local participant. `stock_88` is CityStock::stock by goods number (8.8 tons) and
/// `capacity_88` its per-goods capacity.
struct CityView {
    sim::CityRef ref;
    sim::PlayerId owner;
    std::string name;
    std::optional<WarehouseView> warehouse;
    std::vector<std::uint32_t> stock_88;
    std::uint32_t capacity_88 = 0;
    friend bool operator==(const CityView&, const CityView&) = default;
};

/// The sim thread's answer to the founding preview's question (PlayRunner::request_found_probe):
/// what the sim would answer to `cmd` on the world of the turn this view was published with,
/// without applying it: the reason it would refuse it, or the rotation it would build at
/// (probe_founding).
struct FoundingProbeView {
    sim::FoundCmd cmd;
    sim::Reject reason = sim::Reject::None;
    std::optional<std::uint8_t> rotation;
    friend bool operator==(const FoundingProbeView&, const FoundingProbeView&) = default;
};

struct OpeningView {
    std::uint64_t turn = 0, tick = 0;
    sim::PlayerId local;
    std::int64_t gold = 0;
    /// Every ship the participant may see, in FigureId order.
    std::vector<ShipView> ships;
    /// The local participant's cities, in (island, city) order.
    std::vector<CityView> cities;
    /// The newest ship events, oldest first, at most kMaxEvents (a reader that fell behind is told
    /// by `events_next` jumping past what it had read).
    std::vector<ShipEventView> events;
    std::uint64_t events_next = 1;
    /// The answer to the founding preview's latest question, if one was asked.
    std::optional<FoundingProbeView> found_probe;
    /// The sim's order-id counter after the turn: an order accepted later gets at least this id.
    std::uint64_t next_order_id = 1;

    /// Events kept in a view. Reason: a window that stops drawing for ten seconds still finds the
    /// ends of the orders it tracks (a few per turn at most); the bound keeps each turn's copy
    /// small.
    static constexpr std::size_t kMaxEvents = 256;

    [[nodiscard]] const ShipView* ship(sim::FigureId id) const;
    [[nodiscard]] const CityView* city(sim::CityRef ref) const;
    /// The event that ended `order_id` of `ship`, if it is in the view.
    [[nodiscard]] const ShipEventView* event_for(sim::FigureId ship, std::uint64_t order_id) const;
};

/// The events the runner keeps between turns: `since` numbers them. The runner appends what the
/// sim reported in a turn, then takes the newest kMaxEvents into the view.
class ShipEventLog {
  public:
    void append(const sim::SimEvent& event);
    [[nodiscard]] std::uint64_t next() const { return next_; }
    [[nodiscard]] std::vector<ShipEventView> newest() const;

  private:
    std::vector<ShipEventView> kept_;
    std::uint64_t next_ = 1;
};

/// Takes the view from a world. `local` is the participant whose cities and gold are listed; ships
/// of every owner are (the shared-information start; a policy that hides them is the loader's
/// business, docs/sim/shared-visibility.md). O(figures + the local cities' buildings).
[[nodiscard]] OpeningView collect_opening_view(const sim::World& world, sim::PlayerId local,
                                               const ShipEventLog& events, std::uint64_t turn);

} // namespace anno::app
