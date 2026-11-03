// The opening's controls as window-free logic (W4-OPENING-1; docs/app/ship-opening.md). The viewer
// maps SDL events and the drawn frame to the calls below; the tests drive the same calls. The
// logic reads only an OpeningView and the outcomes the runner published, remembers stable ids, and
// produces sim::Commands. It never reads a World. The sim decides every command; the previews and
// the numbers shown here are advisory and say so.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <anno/app/command_outcome.hpp>
#include <anno/app/opening_view.hpp>
#include <anno/sim/command.hpp>
#include <anno/sim/config.hpp>
#include <anno/sim/ids.hpp>
#include <anno/sim/types.hpp>

namespace anno::app {

/// One island's tile rectangle (world tiles).
struct IslandRect {
    std::uint32_t slot = 0;
    std::int32_t x0 = 0, y0 = 0;
    std::uint32_t w = 0, h = 0;
    [[nodiscard]] bool contains(std::int64_t x, std::int64_t y) const {
        return x >= x0 && y >= y0 && x < std::int64_t{x0} + w && y < std::int64_t{y0} + h;
    }
};

/// What the controls need from the world, read once at load: data, not state.
struct OpeningSetup {
    std::shared_ptr<const sim::TypeRegistry> registry;
    sim::PlayerId local;
    /// Rules::founding_types, the building types the ship-founding action offers.
    std::vector<sim::TypeId> founding_types;
    /// Rules::cargo: the warehouse PRODTYPEs and the dock disc.
    sim::CargoRules cargo;
    std::vector<IslandRect> islands;
    /// The world size in tiles (a MoveCmd target must be inside).
    std::uint32_t world_w = 0, world_h = 0;
    /// The sim's founding reach: dx*dx + dy*dy <= this (docs/sim/ship-founding.md P10), advisory.
    std::int64_t founding_reach_sq = 168;
};

/// A drawn pixel as the pointer sees it.
struct PickedPoint {
    /// The ground tile under the pixel (the tile at sea level over open water).
    sim::TileXY tile;
    /// The point on the sea surface under the pixel, Q24.8 tiles.
    std::int32_t x_q8 = 0, y_q8 = 0;
    /// The pixel is over ground (above the waterline), not open water.
    bool ground = false;
};

/// A ship's drawn rectangle in drawable pixels and its centre.
struct ScreenBox {
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    double cx = 0, cy = 0;
    [[nodiscard]] bool contains(double px, double py) const {
        return px >= x0 && py >= y0 && px < x1 && py < y1;
    }
};

/// The drawn frame as the controls need it (implemented over PointerFrame by the viewer; tests use
/// a plain linear map).
class ScreenProjection {
  public:
    virtual ~ScreenProjection() = default;
    [[nodiscard]] virtual std::optional<PickedPoint> pick(double px, double py) const = 0;
    [[nodiscard]] virtual std::optional<ScreenBox> ship_box(const ShipView& ship) const = 0;
};

enum class OpeningKey : std::uint8_t {
    Found,    ///< F
    Unload,   ///< U
    Halt,     ///< H
    Rotate,   ///< T (in the founding mode)
    NextHold, ///< Tab
    QtyDown,  ///< [
    QtyUp,    ///< ]
    Send,     ///< Enter
    Save,     ///< F5
    Escape,   ///< Esc
};

enum class OpeningMode : std::uint8_t { Select, Found, Unload };
enum class PointerButton : std::uint8_t { Left, Right };

/// What an input made. Commands are posted by the caller in order; `submitted` then names each
/// receipt. `save` asks the caller for a quick-save.
struct UiResult {
    bool taken = false;
    std::vector<sim::Command> commands;
    bool save = false;
};

/// The cursor of the founding mode as the sprite preview needs it.
struct FoundPreview {
    sim::TypeId type;
    sim::TileXY origin;
    std::uint8_t rotation = 0;
    std::uint32_t w = 0, h = 0;
    sim::TileXY click;
    /// The sim's own reason, as far as the controls can tell without the world (advisory); empty
    /// when none is known. Terrain, occupancy and ownership are not checked.
    std::string advisory;
};

/// What the controls read at a moment: the newest published view, the outcome lookup consistent
/// with it (PlayRunner::outcome_at) and the tick of the view the last delivered frame drew.
struct UiFeed {
    const OpeningView* view = nullptr;
    std::function<CommandOutcome(CommandId)> outcome;
    std::optional<std::uint64_t> shown_tick;
};

/// How a tracked command stands now.
enum class TrackState : std::uint8_t {
    Queued,      ///< submitted, no outcome yet
    Refused,     ///< the sim refused it (reason in the line)
    Lost,        ///< ran, outcome no longer kept
    Planning,    ///< Move accepted, not planned yet
    Sailing,     ///< Move accepted and planned, underway
    Arrived,     ///< the sim reported arrival for this order
    RouteFailed, ///< the sim reported that this order's route failed
    Stopped,     ///< this order was cancelled by a stop
    Superseded,  ///< a newer order replaced this one before it ended
    Ended,       ///< the order is gone and its event is no longer kept
    Applied,     ///< Found / Unload accepted
    ShipGone,    ///< the ship is not in the view any more
};

[[nodiscard]] const char* track_state_name(TrackState s);

enum class TrackKind : std::uint8_t { Move, Stop, Found, Unload };

struct Tracked {
    CommandId receipt;
    TrackKind kind = TrackKind::Move;
    sim::FigureId ship;
    std::int32_t target_x_q8 = 0, target_y_q8 = 0; ///< Move
    sim::TileXY click;                             ///< Found
    sim::BuildingId warehouse;                     ///< Unload
    std::uint32_t hold = 0, tons = 0;              ///< Unload
    sim::GoodsId goods = 0;                        ///< Unload
    std::uint64_t hold_before_88 = 0;              ///< Unload: the hold when it was sent
    std::uint64_t submitted_tick = 0;
};

class OpeningUi {
  public:
    explicit OpeningUi(OpeningSetup setup);

    // ---- input -----------------------------------------------------------------------------
    [[nodiscard]] UiResult key(OpeningKey key, bool shift, const UiFeed& feed);
    /// A press. Pointer coordinates are drawable pixels.
    void pointer_down(PointerButton button, double px, double py);
    void pointer_move(double px, double py, const ScreenProjection* frame, const UiFeed& feed);
    /// A release: a click (moved less than kClickSlop pixels since its press and not cancelled)
    /// acts once. A drag acts never.
    [[nodiscard]] UiResult pointer_up(PointerButton button, double px, double py,
                                      const ScreenProjection* frame, const UiFeed& feed);
    /// Focus lost, pointer left the window, window hidden: every gesture in progress is dropped.
    void abort_gestures();
    /// Pointer travel (pixels) up to which a press is still a click. Reason: a hand tremor and a
    /// pan are told apart by the pan's travel; 5 px is below any deliberate drag.
    static constexpr double kClickSlop = 5.0;

    /// The caller posted `commands[index]` of the last UiResult and got `receipt`.
    void submitted(std::size_t index, CommandId receipt, std::uint64_t tick);

    // ---- state -----------------------------------------------------------------------------
    /// Revalidates the selection and every id the controls hold against the newest view; a ship,
    /// city or warehouse that vanished is dropped with a message. Call when a new view arrives.
    void on_view(const UiFeed& feed);
    [[nodiscard]] OpeningMode mode() const { return mode_; }
    [[nodiscard]] std::optional<sim::FigureId> selected() const { return selected_; }
    /// Something Esc would cancel: a gesture, a mode or a selection.
    [[nodiscard]] bool has_pending() const;
    /// A gesture is in progress (a press not yet released).
    [[nodiscard]] bool pressed() const { return left_.down || right_.down; }
    [[nodiscard]] const std::string& message() const { return message_; }
    [[nodiscard]] const std::vector<Tracked>& tracked() const { return tracked_; }
    [[nodiscard]] TrackState state_of(const Tracked& t, const UiFeed& feed) const;
    /// The founding cursor (mode Found with a hovered tile), else nullopt.
    [[nodiscard]] std::optional<FoundPreview> found_preview(const UiFeed& feed) const;
    /// The selected hold, quantity and target of the unload mode.
    struct UnloadPlan {
        std::uint32_t hold = 0, tons = 0;
        sim::GoodsId goods = 0;
        sim::BuildingId warehouse;
        std::string advisory;
    };
    [[nodiscard]] std::optional<UnloadPlan> unload_plan(const UiFeed& feed) const;
    /// The HUD line (the window title's lead): the mode, the last message, the latest command's own
    /// state, the unload / founding advisory and the pointer first (a caption shows about 150
    /// characters), then gold, the selected ship with its cargo and the stores.
    [[nodiscard]] std::string status_line(const UiFeed& feed) const;
    /// Another mode (the building tool) took the controls: leave the founding and unload modes.
    void leave_modes();
    /// The tile under the pointer at its last move (nullopt: none known or the pointer left), and
    /// whether the title's pointer text changed since the last call (the window then refreshes the
    /// title). The HUD shows the tile so a player can aim at a given place.
    [[nodiscard]] bool take_hover_changed() {
        const bool c = hover_changed_;
        hover_changed_ = false;
        return c;
    }

    [[nodiscard]] const OpeningSetup& setup() const { return setup_; }

  private:
    struct Gesture {
        bool down = false, cancelled = false;
        double x = 0, y = 0;
        double travel = 0;
    };
    [[nodiscard]] UiResult click_left(double px, double py, const ScreenProjection* frame,
                                      const UiFeed& feed);
    [[nodiscard]] UiResult click_right(double px, double py, const ScreenProjection* frame,
                                       const UiFeed& feed);
    [[nodiscard]] const ShipView* hit_ship(double px, double py, const ScreenProjection& frame,
                                           const UiFeed& feed) const;
    [[nodiscard]] const ShipView* own_selected(const UiFeed& feed, std::string& why) const;
    [[nodiscard]] std::optional<FoundPreview> make_found(const ShipView& ship,
                                                         sim::TileXY click) const;
    [[nodiscard]] std::optional<sim::BuildingId> default_target(const ShipView& ship,
                                                                const OpeningView& view) const;
    void clamp_unload(const ShipView& ship);
    [[nodiscard]] std::string goods_name(sim::GoodsId g) const;
    void add_tracked(Tracked t);

    OpeningSetup setup_;
    OpeningMode mode_ = OpeningMode::Select;
    std::optional<sim::FigureId> selected_;
    std::optional<std::uint32_t> selected_owner_;
    Gesture left_, right_;
    std::optional<sim::TileXY> hover_tile_;
    /// The pointer's tile and whether it is over ground, in every mode (the HUD's pointer text).
    std::optional<std::pair<sim::TileXY, bool>> hover_point_;
    bool hover_changed_ = false;
    std::uint8_t found_rotation_ = 0;
    std::size_t found_type_ = 0;
    std::uint32_t unload_hold_ = 0, unload_tons_ = 0;
    std::optional<sim::BuildingId> unload_target_;
    std::string message_;
    std::vector<Tracked> tracked_;
    std::vector<Tracked> pending_submit_;
    /// How many tracked commands are kept: the HUD shows the latest, the rest are for the
    /// session record. Reason: bounded memory for a long session.
    static constexpr std::size_t kMaxTracked = 64;
};

/// "12.5" for 12.5 tons (raw 1/256 ton): the whole part and the exact fraction when there is one.
[[nodiscard]] std::string tons_text(std::uint64_t amount_88);

} // namespace anno::app
