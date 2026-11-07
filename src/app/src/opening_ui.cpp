#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <anno/app/opening_ui.hpp>
#include <anno/sim/building.hpp>
#include <anno/sim/ship_cargo.hpp>
#include <anno/sim/site_checks.hpp>

namespace anno::app {

namespace {

std::string fmt(const char* format, long long a = 0, long long b = 0, long long c = 0,
                long long d = 0) {
    char buf[256];
    std::snprintf(buf, sizeof buf, format, a, b, c, d);
    return buf;
}

/// Whole ship tile of a Q24.8 coordinate.
std::int64_t tile_of(std::int32_t q8) {
    return static_cast<std::int64_t>(q8) >> 8;
}

} // namespace

std::string tons_text(std::uint64_t amount_88) {
    const std::uint64_t whole = amount_88 / 256, rest = amount_88 % 256;
    if (rest == 0) {
        return std::to_string(whole);
    }
    // 1/256 ton is exact in at most 8 decimals; two decimals are enough to read.
    char buf[64];
    std::snprintf(buf, sizeof buf, "%llu.%02llu", static_cast<unsigned long long>(whole),
                  static_cast<unsigned long long>(rest * 100 / 256));
    return buf;
}

const char* track_state_name(TrackState s) {
    switch (s) {
    case TrackState::Queued:
        return "queued";
    case TrackState::Refused:
        return "refused";
    case TrackState::Lost:
        return "outcome lost";
    case TrackState::Planning:
        return "accepted, planning";
    case TrackState::Sailing:
        return "sailing";
    case TrackState::Arrived:
        return "arrived";
    case TrackState::RouteFailed:
        return "route failed";
    case TrackState::Stopped:
        return "stopped";
    case TrackState::Superseded:
        return "replaced by a newer order";
    case TrackState::Ended:
        return "ended (event no longer kept)";
    case TrackState::Applied:
        return "accepted";
    case TrackState::ShipGone:
        return "ship gone";
    }
    return "?";
}

OpeningUi::OpeningUi(OpeningSetup setup) : setup_(std::move(setup)) {}

std::string OpeningUi::goods_name(sim::GoodsId g) const {
    if (setup_.registry) {
        if (const sim::GoodsInfo* info = setup_.registry->goods(g)) {
            return info->name;
        }
    }
    return "goods " + std::to_string(g);
}

bool OpeningUi::has_pending() const {
    return left_.down || right_.down || mode_ != OpeningMode::Select || selected_.has_value();
}

void OpeningUi::leave_modes() {
    mode_ = OpeningMode::Select;
    unload_target_.reset();
    load_source_.reset();
    hover_tile_.reset();
}

void OpeningUi::abort_gestures() {
    // Every press in progress is cancelled: its release acts on nothing (a click after focus came
    // back must not issue an order the user never finished).
    // The press is forgotten, not only marked: its release may never come (focus lost, another
    // window got it, the building tool took the pointer), and a press that stays down would swallow
    // Esc for good. An unpaired release is ignored (pointer_up).
    left_ = Gesture{};
    right_ = Gesture{};
    if (hover_point_) {
        hover_point_.reset(); // the pointer left, or the window lost focus: its tile is not known
        hover_changed_ = true;
    }
}

const ShipView* OpeningUi::own_selected(const UiFeed& feed, std::string& why) const {
    if (feed.view == nullptr) {
        why = "no world state yet";
        return nullptr;
    }
    if (!selected_) {
        why = "select one of your ships first (left click)";
        return nullptr;
    }
    const ShipView* ship = feed.view->ship(*selected_);
    if (ship == nullptr) {
        why = "the selected ship is gone";
        return nullptr;
    }
    if (ship->owner != setup_.local) {
        why = "ship #" + std::to_string(ship->id.v) + " belongs to seat " +
              std::to_string(ship->owner.v) + ": inspect only";
        return nullptr;
    }
    return ship;
}

const ShipView* OpeningUi::hit_ship(double px, double py, const ScreenProjection& frame,
                                    const UiFeed& feed) const {
    if (feed.view == nullptr) {
        return nullptr;
    }
    const ShipView* best = nullptr;
    double best_front = 0, best_dist = 0;
    for (const ShipView& s : feed.view->ships) {
        const auto box = frame.ship_box(s);
        if (!box || !box->contains(px, py)) {
            continue;
        }
        // The front-most ship wins (the painter's order is x + y), then the nearer centre, then the
        // lower id: a click on overlapping ships is deterministic.
        const double front = (static_cast<double>(s.pos.x) + s.pos.y) / 256.0;
        const double dist = std::hypot(px - box->cx, py - box->cy);
        if (best == nullptr || front > best_front + 1e-9 ||
            (std::fabs(front - best_front) <= 1e-9 && dist < best_dist - 1e-9)) {
            best = &s;
            best_front = front;
            best_dist = dist;
        }
    }
    return best;
}

void OpeningUi::pointer_down(PointerButton button, double px, double py) {
    Gesture& g = button == PointerButton::Left ? left_ : right_;
    g = Gesture{true, false, px, py, 0.0};
}

void OpeningUi::pointer_move(double px, double py, const ScreenProjection* frame,
                             const UiFeed& feed) {
    for (Gesture* g : {&left_, &right_}) {
        if (g->down) {
            g->travel = std::max(g->travel, std::hypot(px - g->x, py - g->y));
        }
    }
    hover_tile_.reset();
    std::optional<std::pair<sim::TileXY, bool>> point;
    if (frame != nullptr) {
        if (const auto p = frame->pick(px, py)) {
            point = std::pair{p->tile, p->ground};
            if (mode_ == OpeningMode::Found) {
                hover_tile_ = p->tile;
            }
        }
    }
    if (point != hover_point_) {
        hover_changed_ = true;
        hover_point_ = point;
    }
    static_cast<void>(feed);
}

UiResult OpeningUi::pointer_up(PointerButton button, double px, double py,
                               const ScreenProjection* frame, const UiFeed& feed) {
    Gesture& g = button == PointerButton::Left ? left_ : right_;
    if (!g.down) {
        return {}; // a release without a press of ours (focus returned mid-gesture)
    }
    g.travel = std::max(g.travel, std::hypot(px - g.x, py - g.y));
    const bool click = !g.cancelled && g.travel < kClickSlop;
    g.down = false;
    g.cancelled = false;
    if (!click || frame == nullptr) {
        return {};
    }
    return button == PointerButton::Left ? click_left(px, py, frame, feed)
                                         : click_right(px, py, frame, feed);
}

UiResult OpeningUi::click_right(double px, double py, const ScreenProjection* frame,
                                const UiFeed& feed) {
    UiResult out;
    out.taken = true;
    std::string why;
    const ShipView* ship = own_selected(feed, why);
    if (ship == nullptr) {
        message_ = "no order: " + why;
        return out;
    }
    const auto p = frame->pick(px, py);
    if (!p) {
        message_ = "no order: nothing under the pointer";
        return out;
    }
    if (p->ground) {
        message_ = "no order: that is land, click open water";
        return out;
    }
    if (p->x_q8 < 0 || p->y_q8 < 0 ||
        static_cast<std::int64_t>(p->x_q8) >= std::int64_t{setup_.world_w} * 256 ||
        static_cast<std::int64_t>(p->y_q8) >= std::int64_t{setup_.world_h} * 256) {
        message_ = "no order: outside the world";
        return out;
    }
    sim::MoveCmd cmd;
    cmd.ship = ship->id;
    cmd.target_x_q8 = p->x_q8;
    cmd.target_y_q8 = p->y_q8;
    out.commands.push_back(sim::Command{setup_.local, cmd});
    Tracked t;
    t.kind = TrackKind::Move;
    t.ship = ship->id;
    t.target_x_q8 = p->x_q8;
    t.target_y_q8 = p->y_q8;
    pending_submit_.clear();
    pending_submit_.push_back(t);
    message_ = "move order sent to " + fmt("(%lld, %lld)", tile_of(p->x_q8), tile_of(p->y_q8));
    return out;
}

std::optional<FoundPreview> OpeningUi::make_found(const ShipView& ship, sim::TileXY click,
                                                  const UiFeed& feed) const {
    if (setup_.founding_types.empty() || !setup_.registry) {
        return std::nullopt;
    }
    const sim::TypeId type = setup_.founding_types[found_type_ % setup_.founding_types.size()];
    const sim::BuildingType* bt = setup_.registry->find(type);
    if (bt == nullptr) {
        return std::nullopt;
    }
    FoundPreview p;
    p.type = type;
    p.rotation = found_rotation_;
    p.cursor_rotation = found_rotation_;
    p.click = click;
    const auto [w, h] = sim::footprint(*bt, found_rotation_);
    p.w = w;
    p.h = h;
    // docs/sim/ship-founding.md section 3: ox = max(0, cx - (w - 1) / 2), the same for y, with the
    // footprint of the command's rotation. The sim derives it again and refuses a mismatch.
    p.origin = sim::TileXY{static_cast<std::int32_t>(std::max<std::int64_t>(
                               0, std::int64_t{click.x} - (std::int64_t{w} - 1) / 2)),
                           static_cast<std::int32_t>(std::max<std::int64_t>(
                               0, std::int64_t{click.y} - (std::int64_t{h} - 1) / 2))};
    std::string a;
    const auto add = [&a](const std::string& s) { a += (a.empty() ? "" : "; ") + s; };
    // Reach: truncation toward zero of the click minus the ship position (Q24.8), dx^2 + dy^2.
    const std::int64_t dx = (std::int64_t{click.x} * 256 - ship.pos.x) / 256;
    const std::int64_t dy = (std::int64_t{click.y} * 256 - ship.pos.y) / 256;
    if (dx * dx + dy * dy > setup_.founding_reach_sq) {
        add("too far from the ship (" + fmt("%lld,%lld", dx, dy) + "; sail closer)");
    }
    const IslandRect* island = nullptr;
    for (const IslandRect& r : setup_.islands) {
        if (r.contains(click.x, click.y)) {
            island = &r;
        }
    }
    if (island == nullptr) {
        add("the click is on no island");
    } else if (!island->contains(p.origin.x, p.origin.y) ||
               !island->contains(std::int64_t{p.origin.x} + w - 1,
                                 std::int64_t{p.origin.y} + h - 1)) {
        add("footprint leaves the island");
    }
    // The coast rule (docs/sim/ship-founding.md P11.4, site::coast_height_rule: the sim's own pure
    // function) over the terrain's height bytes, for each rotation the sim tries in turn from the
    // command's: the origin stays, the first rotation whose footprint is on the island and whose
    // shore edge is the beach (0x30) with water opposite is the one the sim builds. This is where
    // the warehouse turns to the coast. Occupancy and ownership are still not checked.
    if (island != nullptr && feed.vertex_height &&
        sim::site::coast_class_supported(bt->flags_word)) {
        bool decided = true;
        std::optional<std::uint8_t> accepted;
        for (const std::uint8_t rot : sim::site::rotation_trials(found_rotation_)) {
            const auto [fw, fh] = sim::footprint(*bt, rot);
            if (!island->contains(p.origin.x, p.origin.y) ||
                !island->contains(std::int64_t{p.origin.x} + fw - 1,
                                  std::int64_t{p.origin.y} + fh - 1)) {
                continue;
            }
            const auto verdict = sim::site::coast_height_rule(
                bt->width, bt->height, rot, [&](std::uint64_t vx, std::uint64_t vy) {
                    return feed.vertex_height(
                        std::int64_t{p.origin.x} + static_cast<std::int64_t>(vx),
                        std::int64_t{p.origin.y} + static_cast<std::int64_t>(vy));
                });
            if (verdict.result == sim::site::TerrainResult::Accepted) {
                accepted = rot;
                break;
            }
            if (verdict.result == sim::site::TerrainResult::Unreadable) {
                decided = false;
            }
        }
        if (accepted) {
            p.rotation = *accepted;
            p.coast_ok = true;
            add("it faces the shore");
        } else if (decided) {
            add("not on a coast: the shore must be a beach with water on the opposite side (the "
                "sim answers NoRotationFits)");
        }
    }
    p.cmd.ship = ship.id;
    p.cmd.placement.type = type;
    p.cmd.placement.rotation = found_rotation_;
    p.cmd.placement.x = p.origin.x;
    p.cmd.placement.y = p.origin.y;
    p.cmd.click_x = click.x;
    p.cmd.click_y = click.y;
    // The sim's own answer for this very command, when the runner has published one: it also knows
    // the tiles' owners and what stands on them (owner playtest 2026-11-03: a quarter of the coast
    // sites the terrain allows are taken by rocks or trees, and the preview said "ok" there).
    if (feed.view != nullptr && feed.view->found_probe && feed.view->found_probe->cmd == p.cmd) {
        const FoundingProbeView& probe = *feed.view->found_probe;
        p.sim_answer = probe.reason;
        a.clear();
        if (probe.reason == sim::Reject::None && probe.rotation) {
            p.rotation = *probe.rotation;
            p.coast_ok = true;
            add("the sim accepts it here, facing the shore");
        } else {
            p.coast_ok = false;
            switch (probe.reason) {
            case sim::Reject::NoRotationFits:
                add("not here: the warehouse must stand at the water's edge (beach on one side, "
                    "water on "
                    "the other) on free ground, in one of its four turns (NoRotationFits)");
                break;
            case sim::Reject::OutOfRange:
                add("too far from the ship: sail closer (OutOfRange)");
                break;
            case sim::Reject::OutOfIsland:
                add("the warehouse would leave the island (OutOfIsland)");
                break;
            case sim::Reject::UnknownIsland:
                add("not on an island (UnknownIsland)");
                break;
            default:
                add(std::string("the sim refuses it here: ") +
                    std::string(sim::reject_name(probe.reason)));
                break;
            }
        }
    }
    p.advisory = a;
    return p;
}

UiResult OpeningUi::click_left(double px, double py, const ScreenProjection* frame,
                               const UiFeed& feed) {
    UiResult out;
    out.taken = true;
    if (mode_ == OpeningMode::Found) {
        std::string why;
        const ShipView* ship = own_selected(feed, why);
        if (ship == nullptr) {
            message_ = "no founding: " + why;
            mode_ = OpeningMode::Select;
            return out;
        }
        const auto p = frame->pick(px, py);
        if (!p) {
            message_ = "no founding: nothing under the pointer";
            return out;
        }
        const auto preview = make_found(*ship, p->tile, feed);
        if (!preview) {
            message_ = "no founding: no building type is offered";
            return out;
        }
        sim::FoundCmd cmd;
        cmd.ship = ship->id;
        cmd.placement.type = preview->type;
        cmd.placement.rotation = preview->cursor_rotation; // the sim tries the rotations from here
        cmd.placement.x = preview->origin.x;
        cmd.placement.y = preview->origin.y;
        cmd.click_x = p->tile.x;
        cmd.click_y = p->tile.y;
        out.commands.push_back(sim::Command{setup_.local, cmd});
        Tracked t;
        t.kind = TrackKind::Found;
        t.ship = ship->id;
        t.click = p->tile;
        pending_submit_.clear();
        pending_submit_.push_back(t);
        message_ = "founding sent at " + fmt("(%lld, %lld)", p->tile.x, p->tile.y);
        mode_ = OpeningMode::Select; // one click, one command
        hover_tile_.reset();
        return out;
    }
    if (mode_ == OpeningMode::Unload || mode_ == OpeningMode::Load) {
        const auto p = frame->pick(px, py);
        if (p && feed.view != nullptr) {
            for (const CityView& c : feed.view->cities) {
                if (c.warehouse && p->tile.x >= c.warehouse->tile.x &&
                    p->tile.y >= c.warehouse->tile.y &&
                    p->tile.x < c.warehouse->tile.x + static_cast<std::int64_t>(c.warehouse->w) &&
                    p->tile.y < c.warehouse->tile.y + static_cast<std::int64_t>(c.warehouse->h)) {
                    if (mode_ == OpeningMode::Load) {
                        load_source_ = c.warehouse->id;
                        message_ = "load source: your warehouse in " + c.name;
                    } else {
                        unload_target_ = c.warehouse->id;
                        message_ = "unload target: your warehouse in " + c.name;
                    }
                    return out;
                }
            }
        }
        message_ = (mode_ == OpeningMode::Load ? "click one of your warehouses to choose it as the "
                                                  "source, or Enter for the nearest"
                                               : "click one of your warehouses to choose it, or "
                                                 "Enter for the nearest");
        return out;
    }
    // Select mode.
    if (const ShipView* hit = hit_ship(px, py, *frame, feed)) {
        selected_ = hit->id;
        selected_owner_ = hit->owner.v;
        message_ = hit->owner == setup_.local
                       ? "selected your ship #" + std::to_string(hit->id.v)
                       : "selected ship #" + std::to_string(hit->id.v) + " of seat " +
                             std::to_string(hit->owner.v) + " (inspect only)";
    } else {
        selected_.reset();
        selected_owner_.reset();
        message_ = "nothing selected";
    }
    return out;
}

std::optional<sim::BuildingId> OpeningUi::default_target(const ShipView& ship,
                                                         const OpeningView& view) const {
    std::optional<sim::BuildingId> best;
    std::int64_t best_d = 0;
    const std::int64_t sx = tile_of(ship.pos.x), sy = tile_of(ship.pos.y);
    for (const CityView& c : view.cities) {
        if (!c.warehouse) {
            continue;
        }
        const std::int64_t cx = c.warehouse->tile.x + static_cast<std::int64_t>(c.warehouse->w / 2);
        const std::int64_t cy = c.warehouse->tile.y + static_cast<std::int64_t>(c.warehouse->h / 2);
        const std::int64_t d = std::max(std::abs(cx - sx), std::abs(cy - sy));
        if (!best || d < best_d) {
            best = c.warehouse->id;
            best_d = d;
        }
    }
    return best;
}

void OpeningUi::clamp_unload(const ShipView& ship) {
    if (ship.holds.empty()) {
        unload_hold_ = 0;
        unload_tons_ = 0;
        return;
    }
    if (unload_hold_ >= ship.holds.size()) {
        unload_hold_ = 0;
    }
    const std::uint64_t whole = ship.holds[unload_hold_].amount_88 / 256;
    const std::uint32_t most = static_cast<std::uint32_t>(std::max<std::uint64_t>(
        1, std::min<std::uint64_t>(whole + ((ship.holds[unload_hold_].amount_88 % 256) != 0),
                                   0xFFFFFFFFu)));
    unload_tons_ = std::clamp<std::uint32_t>(unload_tons_, 1, most);
}

void OpeningUi::clamp_load(const ShipView& ship) {
    if (ship.holds.empty()) {
        load_hold_ = 0;
        load_tons_ = 1;
        return;
    }
    if (load_hold_ >= ship.holds.size()) {
        load_hold_ = 0;
    }
    load_tons_ = std::max<std::uint32_t>(load_tons_, 1);
}

std::optional<OpeningUi::LoadPlan> OpeningUi::load_plan(const UiFeed& feed) const {
    if (feed.view == nullptr || !selected_) {
        return std::nullopt;
    }
    const ShipView* ship = feed.view->ship(*selected_);
    if (ship == nullptr || ship->holds.empty() || load_hold_ >= ship->holds.size()) {
        return std::nullopt;
    }
    LoadPlan plan;
    plan.hold = load_hold_;
    plan.tons = load_tons_;
    std::optional<sim::BuildingId> source = load_source_;
    if (source) {
        bool present = false;
        for (const CityView& c : feed.view->cities) {
            present = present || (c.warehouse && c.warehouse->id == *source);
        }
        if (!present) {
            source.reset();
        }
    }
    if (!source) {
        source = default_target(*ship, *feed.view);
    }
    if (!source) {
        plan.advisory = "you have no warehouse yet (found one first)";
        return plan;
    }
    plan.warehouse = *source;
    // The hold's own goods, if it already carries any (topping up); else the source warehouse's
    // most-stocked real goods (stock_88[0] is the sim's "empty" sentinel, never a real good).
    plan.goods = ship->holds[load_hold_].goods;
    if (ship->holds[load_hold_].amount_88 == 0) {
        plan.goods = 0;
        for (const CityView& c : feed.view->cities) {
            if (!c.warehouse || c.warehouse->id != *source) {
                continue;
            }
            std::uint32_t best = 0;
            for (std::size_t g = 1; g < c.stock_88.size(); ++g) {
                if (c.stock_88[g] > best) {
                    best = c.stock_88[g];
                    plan.goods = static_cast<sim::GoodsId>(g);
                }
            }
        }
    }
    for (const CityView& c : feed.view->cities) {
        if (!c.warehouse || c.warehouse->id != *source) {
            continue;
        }
        const bool in_range = sim::ShipCargo::footprint_in_range(
            tile_of(ship->pos.x), tile_of(ship->pos.y), c.warehouse->tile.x, c.warehouse->tile.y,
            c.warehouse->w, c.warehouse->h, setup_.cargo);
        const std::uint64_t stock = plan.goods < c.stock_88.size() ? c.stock_88[plan.goods] : 0;
        // The ship's free room per hold is not in OpeningView (OQ, flagged in the D1 report): this
        // advisory can only bound the move by the source's stock, not by the ship's own capacity.
        const std::uint64_t move = std::min<std::uint64_t>(std::uint64_t{plan.tons} * 256, stock);
        plan.advisory = std::string(in_range ? "in range" : "OUT OF RANGE (sail closer)") +
                        "; would move up to " + tons_text(move) + " t of " + goods_name(plan.goods) +
                        " from store " + tons_text(stock) + " t (ship room unknown to this advisory)";
    }
    return plan;
}

std::optional<OpeningUi::UnloadPlan> OpeningUi::unload_plan(const UiFeed& feed) const {
    if (feed.view == nullptr || !selected_) {
        return std::nullopt;
    }
    const ShipView* ship = feed.view->ship(*selected_);
    if (ship == nullptr || ship->holds.empty() || unload_hold_ >= ship->holds.size()) {
        return std::nullopt;
    }
    UnloadPlan plan;
    plan.hold = unload_hold_;
    plan.tons = unload_tons_;
    plan.goods = ship->holds[unload_hold_].goods;
    std::optional<sim::BuildingId> target = unload_target_;
    if (target) {
        bool present = false;
        for (const CityView& c : feed.view->cities) {
            present = present || (c.warehouse && c.warehouse->id == *target);
        }
        if (!present) {
            target.reset();
        }
    }
    if (!target) {
        target = default_target(*ship, *feed.view);
    }
    if (!target) {
        plan.advisory = "you have no warehouse yet (found one first)";
        return plan;
    }
    plan.warehouse = *target;
    for (const CityView& c : feed.view->cities) {
        if (!c.warehouse || c.warehouse->id != *target) {
            continue;
        }
        const bool in_range = sim::ShipCargo::footprint_in_range(
            tile_of(ship->pos.x), tile_of(ship->pos.y), c.warehouse->tile.x, c.warehouse->tile.y,
            c.warehouse->w, c.warehouse->h, setup_.cargo);
        const std::uint64_t stock = plan.goods < c.stock_88.size() ? c.stock_88[plan.goods] : 0;
        const std::uint64_t room = stock >= c.capacity_88 ? 0 : c.capacity_88 - stock;
        const std::uint64_t move =
            sim::ShipCargo::amount_88(plan.tons, ship->holds[plan.hold].amount_88, room);
        plan.advisory = std::string(in_range ? "in range" : "OUT OF RANGE (sail closer)") +
                        "; would move " + tons_text(move) + " t of " + goods_name(plan.goods) +
                        ", store " + tons_text(stock) + "/" + tons_text(c.capacity_88) + " t";
    }
    return plan;
}

UiResult OpeningUi::key(OpeningKey key, bool shift, const UiFeed& feed) {
    UiResult out;
    switch (key) {
    case OpeningKey::Escape:
        if (left_.down || right_.down) {
            abort_gestures();
            message_ = "gesture cancelled";
            out.taken = true;
        } else if (mode_ != OpeningMode::Select) {
            leave_modes();
            message_ = "mode cancelled";
            out.taken = true;
        } else if (selected_) {
            selected_.reset();
            selected_owner_.reset();
            message_ = "selection cleared";
            out.taken = true;
        }
        return out;
    case OpeningKey::Save:
        out.taken = true;
        out.save = true;
        message_ = "quick-save requested";
        return out;
    case OpeningKey::Halt: {
        out.taken = true;
        std::string why;
        const ShipView* ship = own_selected(feed, why);
        if (ship == nullptr) {
            message_ = "no stop: " + why;
            return out;
        }
        out.commands.push_back(sim::Command{setup_.local, sim::StopCmd{ship->id}});
        Tracked t;
        t.kind = TrackKind::Stop;
        t.ship = ship->id;
        pending_submit_.clear();
        pending_submit_.push_back(t);
        message_ = "stop sent";
        return out;
    }
    case OpeningKey::Found: {
        out.taken = true;
        if (mode_ == OpeningMode::Found) {
            leave_modes();
            message_ = "founding cancelled";
            return out;
        }
        std::string why;
        if (own_selected(feed, why) == nullptr) {
            message_ = "no founding: " + why;
            return out;
        }
        leave_modes();
        mode_ = OpeningMode::Found;
        message_ = "founding: move the pointer to a coast within reach (the warehouse turns to the "
                   "shore by itself), click sends";
        return out;
    }
    case OpeningKey::Unload: {
        out.taken = true;
        if (mode_ == OpeningMode::Unload) {
            leave_modes();
            message_ = "unloading cancelled";
            return out;
        }
        std::string why;
        const ShipView* ship = own_selected(feed, why);
        if (ship == nullptr) {
            message_ = "no unloading: " + why;
            return out;
        }
        leave_modes();
        mode_ = OpeningMode::Unload;
        unload_hold_ = 0;
        for (std::uint32_t i = 0; i < ship->holds.size(); ++i) {
            if (ship->holds[i].amount_88 != 0) {
                unload_hold_ = i;
                break;
            }
        }
        unload_tons_ = ship->holds.empty() ? 0
                                           : static_cast<std::uint32_t>(std::max<std::uint64_t>(
                                                 1, ship->holds[unload_hold_].amount_88 / 256));
        clamp_unload(*ship);
        message_ = "unloading: Tab hold, [ ] quantity, Enter sends";
        return out;
    }
    case OpeningKey::Load: {
        out.taken = true;
        if (mode_ == OpeningMode::Load) {
            leave_modes();
            message_ = "loading cancelled";
            return out;
        }
        std::string why;
        const ShipView* ship = own_selected(feed, why);
        if (ship == nullptr) {
            message_ = "no loading: " + why;
            return out;
        }
        leave_modes();
        mode_ = OpeningMode::Load;
        load_hold_ = 0;
        load_tons_ = 1;
        clamp_load(*ship);
        message_ = "loading (developer adapter, not the original's trade panel): Tab hold, [ ] "
                   "quantity, Enter sends";
        return out;
    }
    case OpeningKey::Rotate:
    case OpeningKey::RotateBack:
        if (mode_ != OpeningMode::Found) {
            return out; // T is the building tool's
        }
        out.taken = true;
        found_rotation_ = static_cast<std::uint8_t>(
            (found_rotation_ + (key == OpeningKey::Rotate ? 1U : 3U)) & 3U);
        message_ = "founding turn " + std::to_string(found_rotation_) +
                   ": the warehouse still turns to the shore, starting from this turn (it decides "
                   "where two turns fit)";
        return out;
    case OpeningKey::NextHold: {
        if (mode_ == OpeningMode::Found) {
            out.taken = true;
            found_type_ = setup_.founding_types.empty()
                              ? 0
                              : (found_type_ + 1) % setup_.founding_types.size();
            return out;
        }
        if (mode_ == OpeningMode::Load) {
            out.taken = true;
            const ShipView* ship = feed.view == nullptr || !selected_
                                       ? nullptr
                                       : feed.view->ship(*selected_);
            if (ship == nullptr || ship->holds.empty()) {
                return out;
            }
            load_hold_ = static_cast<std::uint32_t>((load_hold_ + 1) % ship->holds.size());
            clamp_load(*ship);
            return out;
        }
        if (mode_ != OpeningMode::Unload || feed.view == nullptr || !selected_) {
            return out;
        }
        out.taken = true;
        const ShipView* ship = feed.view->ship(*selected_);
        if (ship == nullptr || ship->holds.empty()) {
            return out;
        }
        for (std::size_t step = 1; step <= ship->holds.size(); ++step) {
            const std::size_t i = (unload_hold_ + step) % ship->holds.size();
            if (ship->holds[i].amount_88 != 0) {
                unload_hold_ = static_cast<std::uint32_t>(i);
                break;
            }
        }
        unload_tons_ = static_cast<std::uint32_t>(
            std::max<std::uint64_t>(1, ship->holds[unload_hold_].amount_88 / 256));
        clamp_unload(*ship);
        return out;
    }
    case OpeningKey::QtyDown:
    case OpeningKey::QtyUp: {
        if (mode_ != OpeningMode::Unload && mode_ != OpeningMode::Load) {
            return out;
        }
        if (feed.view == nullptr || !selected_) {
            return out;
        }
        out.taken = true;
        const ShipView* ship = feed.view->ship(*selected_);
        if (ship == nullptr) {
            return out;
        }
        const std::uint32_t step = shift ? 10 : 1;
        std::uint32_t& tons = mode_ == OpeningMode::Load ? load_tons_ : unload_tons_;
        if (key == OpeningKey::QtyUp) {
            tons += step;
        } else {
            tons = tons > step ? tons - step : 1;
        }
        if (mode_ == OpeningMode::Load) {
            clamp_load(*ship);
        } else {
            clamp_unload(*ship);
        }
        return out;
    }
    case OpeningKey::Send: {
        if (mode_ == OpeningMode::Load) {
            out.taken = true;
            std::string why;
            const ShipView* ship = own_selected(feed, why);
            if (ship == nullptr) {
                message_ = "no load: " + why;
                return out;
            }
            const auto plan = load_plan(feed);
            if (!plan || !plan->warehouse.valid()) {
                message_ = "no load: " + (plan ? plan->advisory : std::string("no ship selected"));
                return out;
            }
            if (plan->tons == 0) {
                message_ = "no load: the quantity is zero";
                return out;
            }
            if (plan->goods == 0) {
                // The hold is empty and the source warehouse has no stock of any real good
                // (goods index 0 is the sim's "empty" sentinel): there is nothing to load. Refuse
                // here with the UI's own message instead of sending a LoadCmd for the sentinel
                // (W4-MAIN-2 review D finding 4).
                message_ = "no load: the warehouse has no stock to load";
                return out;
            }
            sim::LoadCmd cmd;
            cmd.ship = ship->id;
            cmd.warehouse = plan->warehouse;
            cmd.hold = plan->hold;
            cmd.goods = plan->goods;
            cmd.tons = plan->tons;
            out.commands.push_back(sim::Command{setup_.local, cmd});
            Tracked t;
            t.kind = TrackKind::Load;
            t.ship = ship->id;
            t.warehouse = plan->warehouse;
            t.hold = plan->hold;
            t.tons = plan->tons;
            t.goods = plan->goods;
            t.hold_before_88 = ship->holds[plan->hold].amount_88;
            pending_submit_.clear();
            pending_submit_.push_back(t);
            message_ = "load sent: " + std::to_string(plan->tons) + " t of " + goods_name(plan->goods);
            return out;
        }
        if (mode_ != OpeningMode::Unload) {
            return out;
        }
        out.taken = true;
        std::string why;
        const ShipView* ship = own_selected(feed, why);
        if (ship == nullptr) {
            message_ = "no unload: " + why;
            return out;
        }
        const auto plan = unload_plan(feed);
        if (plan && plan->hold < ship->holds.size() && ship->holds[plan->hold].amount_88 == 0) {
            message_ = "no unload: hold [" + std::to_string(plan->hold) + "] is empty (Tab)";
            return out;
        }
        if (!plan || !plan->warehouse.valid()) {
            message_ =
                "no unload: " + (plan ? plan->advisory : std::string("the ship has no cargo"));
            return out;
        }
        if (plan->tons == 0) {
            message_ = "no unload: the quantity is zero";
            return out;
        }
        sim::UnloadCmd cmd;
        cmd.ship = ship->id;
        cmd.warehouse = plan->warehouse;
        cmd.hold = plan->hold;
        cmd.goods = plan->goods;
        cmd.tons = plan->tons;
        out.commands.push_back(sim::Command{setup_.local, cmd});
        Tracked t;
        t.kind = TrackKind::Unload;
        t.ship = ship->id;
        t.warehouse = plan->warehouse;
        t.hold = plan->hold;
        t.tons = plan->tons;
        t.goods = plan->goods;
        t.hold_before_88 = ship->holds[plan->hold].amount_88;
        pending_submit_.clear();
        pending_submit_.push_back(t);
        message_ =
            "unload sent: " + std::to_string(plan->tons) + " t of " + goods_name(plan->goods);
        return out;
    }
    }
    return out;
}

void OpeningUi::submitted(std::size_t index, CommandId receipt, std::uint64_t tick) {
    if (index >= pending_submit_.size()) {
        return;
    }
    Tracked t = pending_submit_[index];
    t.receipt = receipt;
    t.submitted_tick = tick;
    add_tracked(std::move(t));
}

void OpeningUi::add_tracked(Tracked t) {
    tracked_.push_back(std::move(t));
    if (tracked_.size() > kMaxTracked) {
        tracked_.erase(tracked_.begin());
    }
}

void OpeningUi::on_view(const UiFeed& feed) {
    if (feed.view == nullptr) {
        return;
    }
    if (selected_ && feed.view->ship(*selected_) == nullptr) {
        message_ = "the selected ship #" + std::to_string(selected_->v) + " is gone";
        selected_.reset();
        selected_owner_.reset();
        if (mode_ != OpeningMode::Select) {
            leave_modes();
        }
    }
    if (unload_target_) {
        bool present = false;
        for (const CityView& c : feed.view->cities) {
            present = present || (c.warehouse && c.warehouse->id == *unload_target_);
        }
        if (!present) {
            unload_target_.reset();
            message_ = "the chosen warehouse is gone";
        }
    }
}

TrackState OpeningUi::state_of(const Tracked& t, const UiFeed& feed) const {
    if (!feed.outcome) {
        return TrackState::Queued;
    }
    const CommandOutcome out = feed.outcome(t.receipt);
    switch (out.state) {
    case CommandOutcome::State::Queued:
    case CommandOutcome::State::Unknown:
        return TrackState::Queued;
    case CommandOutcome::State::Refused:
        return TrackState::Refused;
    case CommandOutcome::State::Lost:
        return TrackState::Lost;
    case CommandOutcome::State::Applied:
        break;
    }
    if (t.kind != TrackKind::Move) {
        return TrackState::Applied;
    }
    if (!out.order_id || feed.view == nullptr) {
        return TrackState::Planning;
    }
    if (const ShipEventView* e = feed.view->event_for(t.ship, *out.order_id)) {
        switch (e->kind) {
        case ShipEventKind::Arrived:
            return TrackState::Arrived;
        case ShipEventKind::RouteFailed:
            return TrackState::RouteFailed;
        case ShipEventKind::Stopped:
            return TrackState::Stopped;
        }
    }
    const ShipView* ship = feed.view->ship(t.ship);
    if (ship == nullptr) {
        return TrackState::ShipGone;
    }
    if (ship->order_id == out.order_id) {
        return ship->routed ? TrackState::Sailing : TrackState::Planning;
    }
    if (ship->order_id && *ship->order_id > *out.order_id) {
        return TrackState::Superseded;
    }
    // The ship carries no order of this id and no event names it: either a later accepted order of
    // the same ship replaced it (a replacement ends no event), or its event is no longer kept.
    for (const Tracked& later : tracked_) {
        if (later.kind != TrackKind::Move || later.ship != t.ship ||
            later.receipt.n <= t.receipt.n) {
            continue;
        }
        const CommandOutcome next = feed.outcome(later.receipt);
        if (next.state == CommandOutcome::State::Applied && next.order_id &&
            *next.order_id > *out.order_id) {
            return TrackState::Superseded;
        }
    }
    return TrackState::Ended;
}

std::optional<FoundPreview> OpeningUi::found_preview(const UiFeed& feed) const {
    if (mode_ != OpeningMode::Found || !hover_tile_ || feed.view == nullptr || !selected_) {
        return std::nullopt;
    }
    const ShipView* ship = feed.view->ship(*selected_);
    if (ship == nullptr) {
        return std::nullopt;
    }
    return make_found(*ship, *hover_tile_, feed);
}

std::optional<sim::FoundCmd> OpeningUi::probe_wanted(const UiFeed& feed) const {
    const auto p = found_preview(feed);
    if (!p) {
        return std::nullopt;
    }
    return p->cmd;
}

std::string OpeningUi::status_line(const UiFeed& feed) const {
    // What the owner reads first is what a window caption shows first: the mode, the message, the
    // latest command's state, the advisory and the pointer; then gold, the ship, the stores (a
    // caption of a 1280 px window shows about 150 characters, the rest is cut off).
    std::string gold_s, ship_s, city_s, adv_s, cmd_s, ptr_s;
    const char* mode = mode_ == OpeningMode::Found     ? "FOUND"
                       : mode_ == OpeningMode::Unload  ? "UNLOAD"
                       : mode_ == OpeningMode::Load    ? "LOAD"
                                                       : "select";
    const std::string mode_s = std::string("mode ") + mode;
    const OpeningView* view = feed.view;
    if (view != nullptr) {
        gold_s = "gold " + std::to_string(view->gold);
        if (selected_) {
            if (const ShipView* ship = view->ship(*selected_)) {
                std::string s = "ship #" + std::to_string(ship->id.v) + " seat " +
                                std::to_string(ship->owner.v) +
                                (ship->owner == setup_.local ? " (yours)" : " (inspect)") + " at " +
                                fmt("(%lld,%lld)", tile_of(ship->pos.x), tile_of(ship->pos.y));
                std::string cargo;
                for (std::size_t i = 0; i < ship->holds.size(); ++i) {
                    if (ship->holds[i].amount_88 == 0) {
                        continue;
                    }
                    cargo += (cargo.empty() ? "" : ", ") + std::string("[") + std::to_string(i) +
                             "] " + goods_name(ship->holds[i].goods) + " " +
                             tons_text(ship->holds[i].amount_88);
                }
                s += " cargo: " + (cargo.empty() ? std::string("empty") : cargo);
                ship_s = s;
            }
        } else {
            ship_s = "no ship selected";
        }
        if (!view->cities.empty()) {
            std::string c;
            for (const CityView& city : view->cities) {
                c += (c.empty() ? "" : "; ") + std::string(city.name.empty() ? "city" : city.name);
                std::string goods;
                for (std::size_t g = 0; g < city.stock_88.size(); ++g) {
                    if (city.stock_88[g] != 0) {
                        goods += (goods.empty() ? " " : ", ") +
                                 goods_name(static_cast<sim::GoodsId>(g)) + " " +
                                 tons_text(city.stock_88[g]);
                    }
                }
                c += goods.empty() ? " (empty store)" : goods;
            }
            city_s = c;
        }
    }
    if (mode_ == OpeningMode::Unload) {
        if (const auto plan = unload_plan(feed)) {
            adv_s = "hold [" + std::to_string(plan->hold) + "] " + goods_name(plan->goods) +
                    " request " + std::to_string(plan->tons) + " t: " + plan->advisory;
        }
    }
    if (mode_ == OpeningMode::Load) {
        if (const auto plan = load_plan(feed)) {
            adv_s = "hold [" + std::to_string(plan->hold) + "] " + goods_name(plan->goods) +
                    " request " + std::to_string(plan->tons) + " t: " + plan->advisory;
        }
    }
    if (mode_ == OpeningMode::Found) {
        if (const auto p = found_preview(feed)) {
            const sim::BuildingType* bt =
                setup_.registry ? setup_.registry->find(p->type) : nullptr;
            std::string cost = "type " + std::to_string(p->type.v) + " " + std::to_string(p->w) +
                               "x" + std::to_string(p->h) + " rot " + std::to_string(p->rotation) +
                               " (turn " + std::to_string(p->cursor_rotation) + ")";
            if (bt != nullptr) {
                cost += " price " + std::to_string(bt->cost_gold) + " gold";
                for (const sim::CostEntry& e : bt->cost_goods) {
                    cost += ", " + tons_text(e.amount_88) + " t " + goods_name(e.goods);
                }
            }
            adv_s = cost +
                    (p->advisory.empty() ? " (no problem known here)" : " (" + p->advisory + ")") +
                    " [advisory]";
        }
    }
    if (!tracked_.empty()) {
        const Tracked& t = tracked_.back();
        std::string s;
        switch (t.kind) {
        case TrackKind::Move:
            s = "move #" + std::to_string(t.receipt.n);
            break;
        case TrackKind::Stop:
            s = "stop #" + std::to_string(t.receipt.n);
            break;
        case TrackKind::Found:
            s = "found #" + std::to_string(t.receipt.n);
            break;
        case TrackKind::Unload:
            s = "unload #" + std::to_string(t.receipt.n);
            break;
        case TrackKind::Load:
            s = "load #" + std::to_string(t.receipt.n);
            break;
        }
        const TrackState st = state_of(t, feed);
        s += std::string(": ") + track_state_name(st);
        if (feed.outcome) {
            const CommandOutcome out = feed.outcome(t.receipt);
            if (out.state == CommandOutcome::State::Refused) {
                s += " (" + std::string(sim::reject_name(out.status)) + ")";
            }
            if (st == TrackState::RouteFailed && feed.view != nullptr && out.order_id) {
                if (const ShipEventView* e = feed.view->event_for(t.ship, *out.order_id)) {
                    static const char* const names[] = {"unreachable", "budget exceeded",
                                                        "outside window", "ship blocked"};
                    s += std::string(" (") + (e->reason < 4 ? names[e->reason] : "?") + ")";
                }
            }
            if ((t.kind == TrackKind::Unload || t.kind == TrackKind::Load) &&
                out.state == CommandOutcome::State::Applied) {
                s += ": moved " + tons_text(out.transferred_88) + " t of " +
                     std::to_string(t.tons) + " requested" +
                     (out.transferred_88 < std::uint64_t{t.tons} * 256 ? " (limited)" : "");
                if (feed.view != nullptr) {
                    if (const ShipView* ship = feed.view->ship(t.ship)) {
                        if (t.hold < ship->holds.size()) {
                            s += "; hold " + tons_text(t.hold_before_88) + " -> " +
                                 tons_text(ship->holds[t.hold].amount_88) + " t";
                        }
                    }
                }
            }
            if (t.kind == TrackKind::Found && out.state == CommandOutcome::State::Applied) {
                s += ": warehouse #" + std::to_string(out.created.v);
            }
            if (st == TrackState::Arrived && feed.shown_tick && feed.view != nullptr &&
                out.order_id) {
                if (const ShipEventView* e = feed.view->event_for(t.ship, *out.order_id)) {
                    s += *feed.shown_tick >= e->tick ? " (shown)" : " (not drawn yet)";
                }
            }
        }
        cmd_s = s;
    }
    if (hover_point_) {
        ptr_s = "pointer (" + std::to_string(hover_point_->first.x) + "," +
                std::to_string(hover_point_->first.y) + ") " +
                (hover_point_->second ? "land" : "sea");
    }
    std::string line;
    const std::string* const parts[] = {&mode_s, &message_, &cmd_s,  &adv_s,
                                        &ptr_s,  &gold_s,   &ship_s, &city_s};
    for (const std::string* s : parts) {
        if (!s->empty()) {
            line += (line.empty() ? "" : " | ") + *s;
        }
    }
    return line;
}

} // namespace anno::app
