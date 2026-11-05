// What the window draws of a moving ship between two published turns (W4-OPENING-1, owner playtest
// 2026-11-03: the ship looks like it is teleporting from tile to tile). The sim publishes a
// position every turn, about ten times a second; the window draws at the display's rate. ShipMotion
// keeps, per ship, the segment from the position it showed when a turn arrived to the position that
// turn published, and shows the point of that segment the wall clock has reached (one publication
// interval behind, never ahead: no overshoot at a stop). The facing follows the direction of that
// segment, turning by at most one of the sprite's 16 steps per kTurnStepSeconds, and falls back to
// the sim's own dir16 for a ship that is not moving.
//
// Render-side only: it reads published views and the wall clock, owns no game state, and nothing
// reads it back except the window's drawing and hit test (so a click lands on the sprite that is
// drawn). The sim's positions (the facts, the commands, the HUD's tile) are untouched.
#pragma once

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <anno/app/opening_view.hpp>
#include <anno/render/fluid.hpp>

namespace anno::app {

class ShipMotion {
  public:
    /// The most a sprite turns, in steps of its 16 frames, per this many seconds of wall clock.
    /// Tuning, not evidence: a quarter turn takes 4 steps.
    static constexpr double kTurnStepSeconds = 0.06;
    /// A published jump larger than this many tiles is not sailed but shown at once (a resume, a
    /// ship that appeared). Reason: a ship covers well under a tile per turn; bigger is not motion.
    static constexpr double kSnapTiles = 6.0;

    /// A newly published view arrived at wall-clock second `now` (monotonic seconds). Call once per
    /// distinct published view, in order.
    void observe(const OpeningView& view, double now);

    /// `newest` (the last observed view) with each ship's position and dir16 as they are drawn at
    /// `now`. Advances the turning state; call at most once per drawn frame.
    [[nodiscard]] OpeningView display(const OpeningView& newest, double now);

    /// Some ship has not reached its published position or finished turning at `now`.
    [[nodiscard]] bool animating(double now) const;

    /// The ships acting on the waves at `now`: position as drawn and the velocity of the segment
    /// (tiles per second), only the ships that are moving (render::fluid::ShipWake), at most
    /// render::fluid::kMaxShipWakes of them: the nearest to `centre` (tile position, the wave
    /// window's centre), ties by figure id.
    [[nodiscard]] std::vector<render::fluid::ShipWake>
    wakes(double now, std::pair<double, double> centre = {0.0, 0.0}) const;

    /// The seconds between the last publications (smoothed): what the drawing lags by.
    [[nodiscard]] double interval() const { return interval_; }

    /// The sprite step (0..15, sim::Figure::dir16 convention: 0 is -x, 4 is +y, 8 is +x, 12 is -y)
    /// nearest to the world direction (dx, dy).
    [[nodiscard]] static std::uint8_t dir16_of(double dx, double dy);

  private:
    struct Track {
        double x0 = 0, y0 = 0;   // where the segment starts (tiles)
        double x1 = 0, y1 = 0;   // where the newest publication put the ship
        double t1 = 0;           // wall second the newest publication arrived
        double dt = 0.1;         // seconds the segment takes
        std::uint8_t target = 0; // the facing the segment asks for
        std::uint8_t shown = 0;  // the facing drawn now
        double last_turn = 0;    // wall second of the last step of `shown`
    };
    [[nodiscard]] static double progress(const Track& t, double now);
    std::map<std::uint64_t, Track> tracks_;
    double interval_ = 0.1;
    double last_observed_ = -1.0;
};

} // namespace anno::app
