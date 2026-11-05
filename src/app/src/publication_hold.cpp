#include <limits>

#include <anno/app/publication_hold.hpp>

namespace anno::app {

namespace {

/// The sequence number the next recorded outcome gets (a read beyond the end answers with it).
std::uint64_t outcomes_next(const PlayRunner& runner) {
    return runner.outcomes_since(std::numeric_limits<std::uint64_t>::max()).next;
}

} // namespace

PublicationHold::PublicationHold(PlayRunner& runner, std::chrono::milliseconds watchdog)
    : runner_(runner), state_(std::make_shared<State>()) {
    state_->watchdog = watchdog;
    // The gate holds the state, not the hold: a hold destroyed before the sim thread stops leaves
    // an inert gate. `runner` is the object the gate is installed in, so it outlives the call.
    runner.set_publish_gate([s = state_, &runner](std::uint64_t turn) {
        if (!s->armed.load()) {
            return;
        }
        std::unique_lock lock(s->mu);
        if (s->dead || !s->armed.load() || outcomes_next(runner) <= s->baseline) {
            return;
        }
        s->held = true;
        s->turn = turn;
        ++s->holds;
        const std::uint64_t entered = s->epoch;
        const bool released = s->cv.wait_for(
            lock, s->watchdog, [&s, entered] { return s->epoch != entered || s->dead; });
        if (!released) {
            s->expired = true;
            s->armed.store(false); // a release has disarmed already, and a new arm() must stand
        }
        s->held = false;
    });
}

PublicationHold::~PublicationHold() {
    {
        const std::lock_guard lock(state_->mu);
        state_->dead = true;
        state_->armed.store(false);
        ++state_->epoch;
    }
    state_->cv.notify_all();
}

void PublicationHold::arm() {
    const std::lock_guard lock(state_->mu);
    if (!state_->armed.load()) {
        state_->baseline = outcomes_next(runner_);
        state_->armed.store(true);
    }
}

void PublicationHold::release() {
    {
        const std::lock_guard lock(state_->mu);
        state_->armed.store(false);
        ++state_->epoch;
    }
    state_->cv.notify_all();
}

bool PublicationHold::held() const {
    const std::lock_guard lock(state_->mu);
    return state_->held;
}

bool PublicationHold::expired() const {
    const std::lock_guard lock(state_->mu);
    return state_->expired;
}

std::uint64_t PublicationHold::held_turn() const {
    const std::lock_guard lock(state_->mu);
    return state_->turn;
}

std::uint64_t PublicationHold::holds() const {
    const std::lock_guard lock(state_->mu);
    return state_->holds;
}

void StageHold::State::pass() {
    std::unique_lock lock(mu);
    if (!armed || dead) {
        return;
    }
    ++blocked;
    const std::uint64_t entered = epoch;
    const bool released =
        cv.wait_for(lock, watchdog, [this, entered] { return epoch != entered || dead; });
    --blocked;
    if (!released) {
        expired = true;
        armed = false;
        ++epoch; // the other waiters end with this one
        cv.notify_all();
    }
}

StageHold::StageHold(std::chrono::milliseconds watchdog) : state_(std::make_shared<State>()) {
    state_->watchdog = watchdog;
}

StageHold::~StageHold() {
    {
        const std::lock_guard lock(state_->mu);
        state_->dead = true;
        state_->armed = false;
        ++state_->epoch;
    }
    state_->cv.notify_all();
}

void StageHold::pass() {
    state_->pass();
}

std::function<void()> StageHold::gate() {
    return [s = state_] { s->pass(); };
}

void StageHold::arm() {
    const std::lock_guard lock(state_->mu);
    state_->armed = true;
}

void StageHold::release() {
    {
        const std::lock_guard lock(state_->mu);
        state_->armed = false;
        ++state_->epoch;
    }
    state_->cv.notify_all();
}

bool StageHold::armed() const {
    const std::lock_guard lock(state_->mu);
    return state_->armed;
}

std::size_t StageHold::blocked() const {
    const std::lock_guard lock(state_->mu);
    return state_->blocked;
}

bool StageHold::expired() const {
    const std::lock_guard lock(state_->mu);
    return state_->expired;
}

} // namespace anno::app
