#include <algorithm>
#include <cmath>
#include <cstddef>

#include <anno/app/ship_motion.hpp>

namespace anno::app {

namespace {

constexpr double kPi = 3.14159265358979323846;
/// A segment shorter than this many tiles does not change the facing it asks for (the positions are
/// Q24.8: a step of 1/256 tile is noise as a direction).
constexpr double kMinTurnSegment = 0.02;

} // namespace

std::uint8_t ShipMotion::dir16_of(double dx, double dy) {
    // dir16 d has the world direction (-cos(22.5 d), sin(22.5 d)): 0 is -x, 4 is +y, 8 is +x, 12 is
    // -y.
    const double degrees = std::atan2(dy, -dx) * 180.0 / kPi;
    const long step = std::lround(degrees / 22.5);
    return static_cast<std::uint8_t>(((step % 16) + 16) % 16);
}

double ShipMotion::progress(const Track& t, double now) {
    if (!(t.dt > 0.0)) {
        return 1.0;
    }
    return std::clamp((now - t.t1) / t.dt, 0.0, 1.0);
}

void ShipMotion::observe(const OpeningView& view, double now) {
    if (last_observed_ >= 0.0) {
        const double gap = now - last_observed_;
        if (gap > 0.001 && gap < 2.0) {
            interval_ = 0.8 * interval_ + 0.2 * gap;
        }
    }
    last_observed_ = now;
    std::map<std::uint64_t, Track> next;
    for (const ShipView& ship : view.ships) {
        const double nx = static_cast<double>(ship.pos.x) / 256.0;
        const double ny = static_cast<double>(ship.pos.y) / 256.0;
        const std::uint8_t sim_dir = static_cast<std::uint8_t>(ship.dir16 & 15U);
        Track t;
        const auto it = tracks_.find(ship.id.v);
        if (it == tracks_.end()) {
            t.x0 = t.x1 = nx;
            t.y0 = t.y1 = ny;
            t.t1 = now;
            t.dt = interval_;
            t.shown = t.target = sim_dir;
            t.last_turn = now;
        } else {
            t = it->second;
            // Start where the ship is drawn now, so the next segment continues the picture.
            const double a = progress(t, now);
            const double cx = t.x0 + (t.x1 - t.x0) * a;
            const double cy = t.y0 + (t.y1 - t.y0) * a;
            if (std::hypot(nx - cx, ny - cy) > kSnapTiles) {
                t.x0 = nx;
                t.y0 = ny;
                t.shown = t.target = sim_dir;
            } else {
                t.x0 = cx;
                t.y0 = cy;
            }
            t.x1 = nx;
            t.y1 = ny;
            t.t1 = now;
            t.dt = interval_;
            const double dx = t.x1 - t.x0, dy = t.y1 - t.y0;
            const std::uint8_t asked = t.target;
            if (std::hypot(dx, dy) >= kMinTurnSegment) {
                t.target = dir16_of(dx, dy);
            } else if (std::hypot(dx, dy) == 0.0) {
                // Not moving at all: the sim's own facing (a ship that was turned in place).
                t.target = sim_dir;
            }
            // The turning clock starts when a ship that shows its facing is asked to turn. A new
            // target in the middle of a turn keeps the clock running: restarting it on every
            // change held the facing back while the targets kept changing (review 2026-11-03).
            if (t.target != asked && t.shown == asked) {
                t.last_turn = now;
            }
        }
        next.emplace(ship.id.v, t);
    }
    tracks_ = std::move(next);
}

OpeningView ShipMotion::display(const OpeningView& newest, double now) {
    OpeningView out = newest;
    for (ShipView& ship : out.ships) {
        const auto it = tracks_.find(ship.id.v);
        if (it == tracks_.end()) {
            continue;
        }
        Track& t = it->second;
        const double a = progress(t, now);
        const double x = t.x0 + (t.x1 - t.x0) * a;
        const double y = t.y0 + (t.y1 - t.y0) * a;
        ship.pos.x = static_cast<std::int32_t>(std::lround(x * 256.0));
        ship.pos.y = static_cast<std::int32_t>(std::lround(y * 256.0));
        if (t.shown == t.target) {
            t.last_turn = now;
        } else {
            const long steps = static_cast<long>((now - t.last_turn) / kTurnStepSeconds);
            if (steps > 0) {
                int d = (static_cast<int>(t.target) - static_cast<int>(t.shown)) & 15;
                if (d > 8) {
                    d -= 16;
                }
                const int move = static_cast<int>(std::min<long>(steps, std::abs(d)));
                t.shown = static_cast<std::uint8_t>((t.shown + (d < 0 ? -move : move) + 16) & 15);
                t.last_turn += static_cast<double>(steps) * kTurnStepSeconds;
                if (t.shown == t.target) {
                    t.last_turn = now;
                }
            }
        }
        ship.dir16 = t.shown;
    }
    return out;
}

std::vector<render::fluid::ShipWake> ShipMotion::wakes(double now,
                                                       std::pair<double, double> centre) const {
    // (squared distance to the centre, figure id): the order the wakes are kept in.
    std::vector<std::pair<std::pair<double, std::uint64_t>, render::fluid::ShipWake>> moving;
    for (const auto& [id, t] : tracks_) {
        const double a = progress(t, now);
        if (a >= 1.0 || !(t.dt > 0.0)) {
            continue; // arrived: it leaves no wake from here on
        }
        if (t.x1 == t.x0 && t.y1 == t.y0) {
            continue; // not moving: it would take a wake's place for nothing
        }
        render::fluid::ShipWake w;
        w.x = static_cast<float>(t.x0 + (t.x1 - t.x0) * a);
        w.y = static_cast<float>(t.y0 + (t.y1 - t.y0) * a);
        w.vx = static_cast<float>((t.x1 - t.x0) / t.dt);
        w.vy = static_cast<float>((t.y1 - t.y0) / t.dt);
        const double ex = static_cast<double>(w.x) - centre.first;
        const double ey = static_cast<double>(w.y) - centre.second;
        moving.push_back({{ex * ex + ey * ey, id}, w});
    }
    const std::size_t n = std::min(moving.size(), render::fluid::kMaxShipWakes);
    std::partial_sort(moving.begin(), moving.begin() + static_cast<std::ptrdiff_t>(n), moving.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<render::fluid::ShipWake> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(moving[i].second);
    }
    return out;
}

bool ShipMotion::animating(double now) const {
    return std::any_of(tracks_.begin(), tracks_.end(), [now](const auto& kv) {
        return progress(kv.second, now) < 1.0 || kv.second.shown != kv.second.target;
    });
}

} // namespace anno::app
