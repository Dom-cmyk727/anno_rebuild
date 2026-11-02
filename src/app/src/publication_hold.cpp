#include <anno/app/publication_hold.hpp>

#include <limits>

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
        const bool released =
            s->cv.wait_for(lock, s->watchdog, [&s] { return !s->armed.load() || s->dead; });
        if (!released) {
            s->expired = true;
        }
        s->armed.store(false);
        s->held = false;
    });
}

PublicationHold::~PublicationHold() {
    {
        const std::lock_guard lock(state_->mu);
        state_->dead = true;
        state_->armed.store(false);
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

} // namespace anno::app
