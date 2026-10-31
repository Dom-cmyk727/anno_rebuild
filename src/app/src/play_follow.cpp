// The window's side of play (see the header).
#include <anno/app/play_follow.hpp>

namespace anno::app {

std::size_t PlayFollower::follow(render::FrameRenderer& frames) {
    const auto batches = runner_.take_building_batches();
    if (!batches.empty()) {
        picture_.apply(batches);
    }
    // Commits what is ready (a full reclaim backlog skips it until a later frame) and invalidates
    // the change's keys: the cached chunks keep drawing until their rebuilds arrive.
    return picture_.commit(frames);
}

bool PlayFollower::has_work() const {
    return runner_.batches_waiting() || picture_.pending() > 0;
}

} // namespace anno::app
