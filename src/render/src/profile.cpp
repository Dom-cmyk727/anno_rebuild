#include "anno/render/profile.hpp"

#include <chrono>
#include <vector>

namespace anno::render::profile {

namespace {

using Clock = std::chrono::steady_clock;

struct State {
    Times* sink = nullptr;
    /// The open scopes, innermost last; the innermost one is charged.
    std::vector<Stage> open;
    Clock::time_point last;
};

thread_local State state;

/// Charges the time since the last event to the innermost open stage.
void charge(Clock::time_point now) {
    if (!state.open.empty()) {
        (*state.sink)[state.open.back()] +=
            std::chrono::duration<double, std::milli>(now - state.last).count();
    }
    state.last = now;
}

} // namespace

void attach(Times* sink) {
    state.sink = sink;
    state.open.clear();
}

Scope::Scope(Stage stage) {
    if (state.sink == nullptr) {
        return;
    }
    active_ = true;
    charge(Clock::now());
    state.open.push_back(stage);
}

Scope::~Scope() {
    // attach() between a scope's start and end drops it (the stack was cleared).
    if (!active_ || state.sink == nullptr || state.open.empty()) {
        return;
    }
    charge(Clock::now());
    state.open.pop_back();
}

} // namespace anno::render::profile
